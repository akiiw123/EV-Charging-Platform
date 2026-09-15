"""Business CSV -> ODS -> DWD -> Spark SQL DWS -> canonical ten ADS tables."""
from __future__ import annotations
import argparse
import os
import re
import sys
import json
import uuid
import hashlib
from decimal import Decimal
from pathlib import Path


def execute_sql(spark, path: Path, cost: Decimal):
    source = "\n".join(line.split("--", 1)[0] for line in path.read_text(encoding="utf-8").splitlines())
    source = source.replace("${COST_PER_KWH}", str(cost))
    for statement in source.split(";"):
        if statement.strip():
            spark.sql(statement)


def run(spark, raw: str, warehouse: str, cost: Decimal):
    from pyspark.sql import Window, functions as F
    analysis_id = uuid.uuid4().hex
    paths = ["orders/charging_orders.csv", "stations/charging_stations.csv", "piles/charging_piles.csv", "users/users.csv"]

    def input_snapshot():
        result = {}
        for relative in paths:
            path = spark._jvm.org.apache.hadoop.fs.Path(f"{raw.rstrip('/')}/{relative}")
            filesystem = path.getFileSystem(spark.sparkContext._jsc.hadoopConfiguration())
            status = filesystem.getFileStatus(path)
            checksum = filesystem.getFileChecksum(path)
            result[relative] = {"bytes": status.getLen(), "modified_ms": status.getModificationTime(),
                                "hdfs_checksum": checksum.toString() if checksum is not None else None}
        return result

    snapshot = input_snapshot()
    for database in ("ncs_ods", "ncs_dwd", "ncs_dws", "ncs_ads"):
        spark.sql(f"CREATE DATABASE IF NOT EXISTS {database} LOCATION '{warehouse.rstrip('/')}/{database}'")

    def read(name, relative, required):
        frame = spark.read.option("header", True).option("inferSchema", False).option("mode", "FAILFAST").csv(f"{raw.rstrip('/')}/{relative}")
        missing = set(required) - set(frame.columns)
        if missing:
            raise ValueError(f"{name}: missing columns {sorted(missing)}")
        frame.write.mode("overwrite").format("parquet").saveAsTable(f"ncs_ods.ods_{name}")
        return frame

    orders = read("orders", "orders/charging_orders.csv", ["id", "user_id", "pile_id", "status", "created_at", "started_at", "ended_at", "energy_kwh", "amount"])
    stations = read("stations", "stations/charging_stations.csv", ["id", "name", "address"])
    piles = read("piles", "piles/charging_piles.csv", ["id", "station_id", "type"])
    users = read("users", "users/users.csv", ["id"])

    def optional(frame, name, default=None):
        return F.col(name) if name in frame.columns else F.lit(default)

    def identifier(name):
        return F.when(F.trim(F.col(name)).rlike(r"^[1-9][0-9]*$"), F.col(name).cast("long"))

    def unique_dimension(frame, label, required):
        if frame.filter(F.col("id").isNull()).limit(1).count():
            raise ValueError(f"{label}: invalid ID")
        if frame.groupBy("id").count().filter("count > 1").limit(1).count():
            raise ValueError(f"{label}: duplicate ID")
        for column in required:
            if frame.filter(F.col(column).isNull() | (F.length(F.col(column)) == 0)).limit(1).count():
                raise ValueError(f"{label}: empty {column}")
        return frame.cache()

    city = F.trim(optional(stations, "city"))
    stations = stations.select(identifier("id").alias("id"), F.trim("name").alias("station_name"), "address",
                               F.coalesce(F.when(F.length(city) > 0, city), F.lit("未知区域")).alias("station_area"))
    stations = unique_dimension(stations, "stations", ["station_name", "station_area"])
    if stations.filter((F.length("station_name") > 100) | (F.length("station_area") > 50)).limit(1).count():
        raise ValueError("station name/area exceeds ADS contract")
    piles = piles.select(identifier("id").alias("id"), identifier("station_id").alias("station_id"), F.trim("type").alias("station_type"))
    piles = unique_dimension(piles, "piles", ["station_type"])
    if piles.filter(F.col("station_id").isNull() | (F.length("station_type") > 20)).limit(1).count():
        raise ValueError("piles: invalid station_id/type")
    if piles.join(stations, piles.station_id == stations.id, "left_anti").limit(1).count():
        raise ValueError("piles reference missing stations")
    users = unique_dimension(users.select(identifier("id").alias("id")), "users", [])
    raw_count = orders.count()
    if raw_count == 0:
        raise ValueError("orders CSV is empty")
    timestamp = lambda name: F.to_timestamp(F.col(name))
    platform = F.lower(F.trim(optional(orders, "platform")))
    platform = F.when(platform.isNull() | (platform == ""), F.lit(None).cast("string")).when(platform == "ios", "iOS").when(platform == "android", "Android").when(platform == "web", "Web").otherwise("其他")
    soc = optional(orders, "soc").cast("double")
    soc = F.when(soc.between(0, 100) & ~F.isnan(soc), soc)
    parsed = orders.select(identifier("id").alias("id"), identifier("user_id").alias("user_id"), identifier("pile_id").alias("pile_id"),
                           F.trim("status").alias("status"), timestamp("created_at").alias("created_at"),
                           timestamp("started_at").alias("started_at"), timestamp("ended_at").alias("ended_at"),
                           F.col("energy_kwh").cast("decimal(18,3)").alias("kwh"),
                           F.col("amount").cast("decimal(18,2)").alias("amount"),
                           optional(orders, "occupancy_fee", "0").cast("decimal(18,2)").alias("occupancy_fee"),
                           platform.alias("platform"), soc.alias("soc"),
                           ((F.length(F.trim("started_at")) > 0) & timestamp("started_at").isNull()).alias("bad_start"),
                           ((F.length(F.trim("ended_at")) > 0) & timestamp("ended_at").isNull()).alias("bad_end"))
    valid = parsed.filter(F.col("id").isNotNull() & F.col("user_id").isNotNull() & F.col("pile_id").isNotNull() & F.col("created_at").isNotNull()
                          & F.col("status").isin("reserved", "charging", "awaiting_payment", "completed", "cancelled")
                          & (F.col("kwh") >= 0) & (F.col("amount") >= 0) & (F.col("occupancy_fee") >= 0)
                          & ~F.coalesce(F.col("bad_start"), F.lit(False)) & ~F.coalesce(F.col("bad_end"), F.lit(False))
                          & (F.col("started_at").isNull() | F.col("ended_at").isNull() | (F.col("ended_at") >= F.col("started_at"))))
    fingerprint = F.sha2(F.to_json(F.struct(*[F.col(name) for name in valid.columns])), 256)
    window = Window.partitionBy("id").orderBy(F.col("created_at").desc(), fingerprint.desc())
    valid = valid.withColumn("row_number", F.row_number().over(window)).filter("row_number = 1").drop("row_number", "bad_start", "bad_end")
    valid = valid.join(users.select(F.col("id").alias("user_id")), "user_id", "inner")
    detail = valid.join(piles.select(F.col("id").alias("pile_id"), "station_id", "station_type"), "pile_id", "inner")
    detail = detail.join(stations.select(F.col("id").alias("station_id"), "station_name", "station_area", "address"), "station_id", "inner")
    detail = detail.withColumn("charge_time", F.coalesce("started_at", "created_at"))
    detail = detail.withColumn("charge_date", F.to_date("charge_time")).withColumn("hour", F.hour("charge_time"))
    detail = detail.withColumn("is_weekend", F.when(F.dayofweek("charge_time").isin(1, 7), 1).otherwise(0))
    detail = detail.withColumn("total_fee", F.when(F.col("status") == "completed", F.col("amount") + F.col("occupancy_fee")).otherwise(F.lit(0)).cast("decimal(18,2)")).cache()
    valid_count = detail.count()
    if valid_count == 0:
        raise ValueError("all orders were rejected; fix IDs, numeric values or timestamps")
    actual = detail.filter(F.col("status").isin("charging", "awaiting_payment", "completed"))
    stats = actual.agg(F.count("*").alias("sessions"), F.count("platform").alias("platform_rows"), F.count("soc").alias("soc_rows")).first()
    script_hash = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    quality = spark.createDataFrame([(raw_count, valid_count, raw_count-valid_count, "platform" in orders.columns, "soc" in orders.columns, int(stats.platform_rows), int(stats.soc_rows), float(cost), analysis_id, raw, json.dumps(snapshot), script_hash, "building")],
                                    "raw_count long, valid_count long, rejected_count long, platform_column boolean, soc_column boolean, platform_rows long, soc_rows long, cost_per_kwh double, analysis_id string, raw_source string, input_snapshot_json string, script_sha256 string, analysis_state string")
    for name, frame in (("dwd_charge_detail", detail), ("dwd_station", stations), ("dwd_pile", piles), ("dwd_user", users), ("dwd_quality", quality)):
        frame.write.mode("overwrite").format("parquet").saveAsTable(f"ncs_dwd.{name}")
    directory = Path(__file__).resolve().parents[1] / "spark_sql"
    execute_sql(spark, directory / "02_dws_business.sql", cost)
    execute_sql(spark, directory / "03_ads_dashboard.sql", cost)
    if input_snapshot() != snapshot:
        raise RuntimeError("Source CSV files changed during analysis; rerun after uploads finish")
    quality.withColumn("analysis_state", F.lit("success")).write.mode("overwrite").format("parquet").saveAsTable("ncs_dwd.dwd_quality")
    print(f"Analysis complete: raw={raw_count}, valid={valid_count}, rejected={raw_count-valid_count}; platform={stats.platform_rows}; SOC={stats.soc_rows}")


if __name__ == "__main__":
    if sys.version_info[:2] not in ((3, 11), (3, 12)):
        raise SystemExit("Requires Python 3.11 or 3.12")
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw", default=os.getenv("RAW_HDFS_DIR", "/evcharging/raw"))
    parser.add_argument("--warehouse", default=os.getenv("ANALYTICS_WAREHOUSE", "/evcharging/warehouse"))
    parser.add_argument("--cost-per-kwh", default=os.getenv("COST_PER_KWH", "0.6"))
    args = parser.parse_args()
    if any(not re.fullmatch(r"[A-Za-z0-9_./:\-]+", value) for value in (args.raw, args.warehouse)):
        raise SystemExit("Unsupported raw/warehouse path")
    cost = Decimal(args.cost_per_kwh)
    if not cost.is_finite() or not 0 <= cost <= 100:
        raise SystemExit("COST_PER_KWH must be between 0 and 100")
    from pyspark.sql import SparkSession
    spark = SparkSession.builder.appName("EVChargingStage2").config("spark.sql.session.timeZone", "Asia/Shanghai").config("spark.sql.ansi.enabled", "false").config("spark.sql.shuffle.partitions", "20").enableHiveSupport().getOrCreate()
    try:
        if not spark.version.startswith(("3.4.", "3.5.")):
            raise RuntimeError(f"Use matching Spark/PySpark 3.4.x or 3.5.x; got {spark.version}")
        if spark.version.startswith("3.4.") and sys.version_info[:2] != (3, 11):
            raise RuntimeError("Use Python 3.11 with existing Spark 3.4.x; Python 3.12 needs matching Spark 3.5.x")
        if not spark.sparkContext._jsc.hadoopConfiguration().get("fs.defaultFS", "file:///").startswith("hdfs://"):
            raise RuntimeError("Configure the real HDFS NameNode using HADOOP_CONF_DIR")
        run(spark, args.raw, args.warehouse, cost)
    finally:
        spark.stop()
