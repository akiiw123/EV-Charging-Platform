"""把 Spark ADS 表安全导出为可交付、可校验的一整批文件。

输入：Spark 中十张 ncs_ads 表，可选先执行完整分析。
输出/接口：十个正确转义的竖线 CSV 和带行数、SHA256、脚本身份、输入快照的 manifest.json。
"""
from __future__ import annotations
import argparse
import csv
import json
import os
import shutil
import sys
import tempfile
from datetime import datetime, timezone
from decimal import Decimal
from pathlib import Path
from ads_common import CONTRACT, TABLES, digest, read_table, validate_export
from evcharging_analysis import run


def export(spark, destination: Path):
    destination = destination.resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=f".{destination.name}.", dir=destination.parent))
    try:
        manifest = {"contract_version": CONTRACT["version"], "generated_at": datetime.now(timezone.utc).isoformat(),
                    "quality": spark.table("ncs_dwd.dwd_quality").first().asDict(), "tables": {}}
        manifest["analysis"] = {"module": "analytics/scripts/evcharging_analysis.py",
                                "script_sha256": manifest["quality"]["script_sha256"],
                                "spark_version": spark.version, "python_version": sys.version.split()[0],
                                "analysis_id": manifest["quality"]["analysis_id"],
                                "raw_source": manifest["quality"]["raw_source"],
                                "input_snapshot": json.loads(manifest["quality"].pop("input_snapshot_json"))}
        for name, spec in TABLES.items():
            columns = [column[0] for column in spec["columns"]]
            frame = spark.table(f"ncs_ads.{name}")
            if frame.columns != columns:
                raise ValueError(f"{name}: Spark field order differs from contract")
            if spec.get("key"):
                frame = frame.orderBy(*spec["key"])
            path = temporary / f"{name}.csv"
            with path.open("w", encoding="utf-8", newline="") as stream:
                writer = csv.writer(stream, delimiter="|", lineterminator="\n")
                for row in frame.toLocalIterator():
                    writer.writerow(row)
            manifest["tables"][name] = {"rows": len(read_table(temporary, name)), "sha256": digest(path)}
        (temporary / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
        validate_export(temporary)
        previous = None
        if destination.exists():
            previous = destination.with_name(destination.name + ".previous." + datetime.now(timezone.utc).strftime("%Y%m%d%H%M%S%f"))
            destination.rename(previous)
        try:
            temporary.rename(destination)
        except Exception:
            if previous is not None:
                previous.rename(destination)
            raise
        print(f"Validated ten ADS files: {destination}")
    finally:
        if temporary.exists():
            shutil.rmtree(temporary)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--analyze", action="store_true", help="Rebuild all layers before exporting")
    parser.add_argument("--raw", default=os.getenv("RAW_HDFS_DIR", "/evcharging/raw"))
    parser.add_argument("--warehouse", default=os.getenv("ANALYTICS_WAREHOUSE", "/evcharging/warehouse"))
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    if sys.version_info[:2] not in ((3, 11), (3, 12)):
        raise SystemExit("Requires Python 3.11 or 3.12")
    import re
    if any(not re.fullmatch(r"[A-Za-z0-9_./:\-]+", value) for value in (args.raw,args.warehouse)):
        raise SystemExit("Unsupported raw/warehouse path")
    cost = Decimal(os.getenv("COST_PER_KWH", "0.6"))
    if not cost.is_finite() or not 0 <= cost <= 100:
        raise SystemExit("Invalid COST_PER_KWH")
    from pyspark.sql import SparkSession
    spark = SparkSession.builder.appName("EVChargingADSExport").config("spark.sql.session.timeZone", "Asia/Shanghai").config("spark.sql.ansi.enabled", "false").config("spark.sql.shuffle.partitions", "20").enableHiveSupport().getOrCreate()
    try:
        if not spark.version.startswith(("3.4.", "3.5.")):
            raise RuntimeError(f"Requires Spark 3.4.x or 3.5.x, got {spark.version}")
        if spark.version.startswith("3.4.") and sys.version_info[:2] != (3, 11):
            raise RuntimeError("Spark 3.4.x requires Python 3.11 for this classroom configuration")
        default_fs = spark.sparkContext._jsc.hadoopConfiguration().get("fs.defaultFS", "file:///")
        if not default_fs.startswith("hdfs://"):
            raise RuntimeError("Hadoop fs.defaultFS must point to the actual HDFS NameNode; check HADOOP_CONF_DIR")
        if args.analyze:
            run(spark, args.raw, args.warehouse, cost)
        # Detect modifications since the DWD input snapshot, even in export-only mode.
        quality = spark.table("ncs_dwd.dwd_quality").first().asDict()
        if quality.get("analysis_state") != "success" or quality.get("script_sha256") != digest(Path(__file__).with_name("evcharging_analysis.py")):
            raise RuntimeError("Analysis incomplete or script changed; rebuild with --analyze")
        for relative, expected in json.loads(quality["input_snapshot_json"]).items():
            path = spark._jvm.org.apache.hadoop.fs.Path(f"{quality['raw_source'].rstrip('/')}/{relative}")
            filesystem = path.getFileSystem(spark.sparkContext._jsc.hadoopConfiguration())
            status = filesystem.getFileStatus(path)
            checksum = filesystem.getFileChecksum(path)
            actual = {"bytes": status.getLen(), "modified_ms": status.getModificationTime(),
                      "hdfs_checksum": checksum.toString() if checksum is not None else None}
            if actual != expected:
                raise RuntimeError("Raw CSV changed after analysis; export with --analyze")
        export(spark, args.out)
    finally:
        spark.stop()
