from pyspark.sql import SparkSession

from pyspark.sql import functions as F



spark = SparkSession.builder.appName("EVChargingAnalysis").getOrCreate()



orders = spark.read.option("header", "true").option("inferSchema", "true").csv("hdfs:///evcharging/raw/orders/charging_orders.csv")

stations = spark.read.option("header", "true").option("inferSchema", "true").csv("hdfs:///evcharging/raw/stations/charging_stations.csv")

piles = spark.read.option("header", "true").option("inferSchema", "true").csv("hdfs:///evcharging/raw/piles/charging_piles.csv")
users = spark.read.option("header", "true").option("inferSchema", "true").csv("hdfs:///evcharging/raw/users/users.csv")

clean_orders = orders.dropDuplicates(["id"])
clean_orders = clean_orders.filter(

    (F.col("energy_kwh") >= 0) &

    (F.col("amount") >= 0) &

    (F.col("occupancy_fee") >= 0)

)



valid_status = ["reserved", "charging", "awaiting_payment", "completed", "cancelled"]



clean_orders = clean_orders.filter(F.col("status").isin(valid_status))



clean_orders = clean_orders.filter(

    F.col("ended_at").isNull() |

    F.col("started_at").isNull() |

    (F.col("ended_at") >= F.col("started_at"))

)


clean_orders.write.mode("overwrite").parquet("hdfs:///evcharging/dwd/charging_orders")
joined = clean_orders.join(piles, clean_orders.pile_id == piles.id, "left")

joined = joined.join(stations, piles.station_id == stations.id, "left")



daily_orders = clean_orders.groupBy(

    F.to_date("created_at").alias("date")

).count().orderBy("date")



daily_energy = clean_orders.groupBy(

    F.to_date("created_at").alias("date")

).agg(

    F.sum("energy_kwh").alias("total_energy")

).orderBy("date")



daily_revenue = clean_orders.groupBy(

    F.to_date("created_at").alias("date")

).agg(

    F.sum("amount").alias("total_revenue")

).orderBy("date")



station_orders = joined.groupBy(

    stations.name.alias("station_name")

).count().orderBy(

    F.desc("count")

)



station_energy = joined.groupBy(

    stations.name.alias("station_name")

).agg(

    F.sum("energy_kwh").alias("total_energy")

).orderBy(

    F.desc("total_energy")

)



station_revenue = joined.groupBy(

    stations.name.alias("station_name")

).agg(

    F.sum("amount").alias("total_revenue")

).orderBy(

    F.desc("total_revenue")
)
pile_type_usage = joined.groupBy(

    piles.type.alias("pile_type")

).agg(

    F.count("*").alias("order_count"),

    F.sum("energy_kwh").alias("total_energy"),

    F.sum("amount").alias("total_revenue")

).orderBy(

    F.desc("order_count")

)



order_status = clean_orders.groupBy(

    "status"

).count().orderBy(

    F.desc("count")

)



station_daily_orders = joined.groupBy(

    stations.name.alias("station_name"),

    F.to_date(clean_orders.created_at).alias("date")

).agg(

    F.count("*").alias("order_count")

).orderBy(

    "date",

    "station_name"

)



time_df = joined.withColumn(

    "time_period",

    F.when(

        (F.hour(clean_orders.created_at) >= 6) &

        (F.hour(clean_orders.created_at) < 12),

        "morning"

    ).when(

        (F.hour(clean_orders.created_at) >= 12) &

        (F.hour(clean_orders.created_at) < 18),

        "afternoon"

    ).when(

        (F.hour(clean_orders.created_at) >= 18) &

        (F.hour(clean_orders.created_at) < 24),

        "evening"
    ).otherwise("night")
)

type_time_usage = time_df.groupBy(
    piles.type.alias("pile_type"),
    "time_period"
).agg(
    F.count("*").alias("order_count"),
    F.sum("energy_kwh").alias("total_energy")
)
daily_orders.write.mode("overwrite").parquet("hdfs:///evcharging/ads/daily_orders")

daily_energy.write.mode("overwrite").parquet("hdfs:///evcharging/ads/daily_energy")

daily_revenue.write.mode("overwrite").parquet("hdfs:///evcharging/ads/daily_revenue")



station_orders.write.mode("overwrite").parquet("hdfs:///evcharging/ads/station_orders")

station_energy.write.mode("overwrite").parquet("hdfs:///evcharging/ads/station_energy")

station_revenue.write.mode("overwrite").parquet("hdfs:///evcharging/ads/station_revenue")



pile_type_usage.write.mode("overwrite").parquet("hdfs:///evcharging/ads/pile_type_usage")

order_status.write.mode("overwrite").parquet("hdfs:///evcharging/ads/order_status")

station_daily_orders.write.mode("overwrite").parquet("hdfs:///evcharging/ads/station_daily_orders")

type_time_usage.write.mode("overwrite").parquet("hdfs:///evcharging/ads/type_time_usage")



print("EV charging analysis completed successfully.")



spark.stop()
