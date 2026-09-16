"""校验 Spark 导出物，并把十张 ADS 结果原子发布到 MySQL。

输入：含十个 CSV 和 manifest.json 的目录，以及 MYSQL_* 私有环境变量。
输出/接口：先写临时表，再在一个事务中替换正式表；失败回滚并保留上一批结果。
"""
from __future__ import annotations
import argparse
import json
import os
import re
import sys
import uuid
from pathlib import Path
from ads_common import TABLES, validate_export


def publish(connection, directory: Path, manifest: dict, tables: dict):
    cursor = connection.cursor()
    batch_id = uuid.uuid4().hex
    locked = False
    batch_started = False
    try:
        cursor.execute("SELECT GET_LOCK(%s,0)", (connection.database + "_etl",))
        locked = cursor.fetchone()[0] == 1
        if not locked:
            raise RuntimeError("Another import is running")
        cursor.execute("INSERT INTO etl_batches(batch_id,source_path,status) VALUES(%s,%s,'running')", (batch_id,str(directory)))
        batch_started = True
        for table, spec in TABLES.items():
            columns = [column[0] for column in spec["columns"]]
            cursor.execute("SELECT COLUMN_NAME,COLUMN_TYPE,IS_NULLABLE FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=%s AND TABLE_NAME=%s ORDER BY ORDINAL_POSITION", (connection.database,table))
            actual = cursor.fetchall()
            if [row[0] for row in actual] != columns:
                raise RuntimeError(f"{table}: old MySQL schema differs; run bootstrap_mysql_users.sh")
            for (name, kind), (_, actual_type, nullable) in zip(spec["columns"], actual):
                normalized = re.sub(r"^(bigint|tinyint|int)\(\d+\)$", r"\1", actual_type.lower())
                if normalized != kind or (nullable == "YES") != (name in spec.get("nullable",[])):
                    raise RuntimeError(f"{table}.{name}: MySQL type/nullability differs from contract")
            cursor.execute(f"CREATE TEMPORARY TABLE stage_{table} LIKE {table}")
            if tables[table]:
                query = f"INSERT INTO stage_{table} ({','.join(columns)}) VALUES ({','.join(['%s']*len(columns))})"
                for start in range(0,len(tables[table]),1000):
                    cursor.executemany(query,tables[table][start:start+1000])
                    cursor.execute("SHOW WARNINGS")
                    if cursor.fetchall():
                        raise RuntimeError(f"{table}: MySQL reported a staging warning")
        connection.start_transaction()
        for table in TABLES:
            cursor.execute(f"DELETE FROM {table}")
            cursor.execute(f"INSERT INTO {table} SELECT * FROM stage_{table}")
        cursor.execute("INSERT INTO etl_metadata(singleton, batch_id, payload) VALUES(1,%s,%s) ON DUPLICATE KEY UPDATE batch_id=VALUES(batch_id),payload=VALUES(payload)",
                       (batch_id,json.dumps(manifest,ensure_ascii=False)))
        cursor.execute("UPDATE etl_batches SET status='success',finished_at=NOW(),detail='10 tables published atomically' WHERE batch_id=%s", (batch_id,))
        connection.commit()
        return batch_id
    except Exception:
        connection.rollback()
        if batch_started:
            try:
                cursor.execute("UPDATE etl_batches SET status='failed',finished_at=NOW(),detail='import rejected; previous results retained' WHERE batch_id=%s", (batch_id,))
            except Exception:
                pass
        raise
    finally:
        if locked:
            cursor.execute("SELECT RELEASE_LOCK(%s)",(connection.database + "_etl",))
            cursor.fetchone()
        cursor.close()


if __name__ == "__main__":
    if sys.version_info[:2] not in ((3,11),(3,12)):
        raise SystemExit("Requires Python 3.11 or 3.12")
    parser = argparse.ArgumentParser()
    parser.add_argument("--dir",type=Path,default=os.getenv("ADS_EXPORT_DIR"))
    parser.add_argument("--validate-only",action="store_true")
    args = parser.parse_args()
    if args.dir is None:
        parser.error("--dir or ADS_EXPORT_DIR is required")
    directory = args.dir.resolve()
    if len(str(directory)) > 512:
        raise SystemExit("Export directory path exceeds batch metadata limit")
    manifest,tables = validate_export(directory)
    if args.validate_only:
        print("All files, keys, numbers and checksums validated")
        raise SystemExit(0)
    import mysql.connector
    connection = mysql.connector.connect(host=os.getenv("MYSQL_HOST","127.0.0.1"),port=int(os.getenv("MYSQL_PORT","3306")),
                                         user=os.environ["MYSQL_USER"],password=os.environ["MYSQL_PASSWORD"],database=os.getenv("MYSQL_DATABASE","charging_ads"),
                                         autocommit=True,connection_timeout=5)
    try:
        print("Published batch:",publish(connection,directory,manifest,tables))
    finally:
        connection.close()
