"""Offline checks of contracts, privacy, SQL metric arithmetic and import rollback.

SQL SELECTs run using a SQLite compatibility adapter; this does NOT replace
real Spark/MySQL smoke testing. Fixtures are artificial, never exported as project data.
"""
from __future__ import annotations
import csv
import json
import re
import sqlite3
import sys
import tempfile
import unittest
from datetime import date, datetime, timezone
from decimal import Decimal
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT / "scripts"))
from ads_common import TABLES, digest, read_table, validate_export
from export_business_csv import export_database
from import_ads_mysql import publish
from verify_chain import verify, GROUPS


def sql_fixture():
    connection=sqlite3.connect(":memory:")
    for name in ("ncs_dwd","ncs_dws","ncs_ads"):
        connection.execute(f"ATTACH DATABASE ':memory:' AS {name}")
    connection.create_function("GREATEST",-1,max)
    connection.create_function("DATEDIFF",2,lambda end,start:(date.fromisoformat(end)-date.fromisoformat(start)).days if end and start else None)
    connection.executescript("""
      CREATE TABLE ncs_dwd.dwd_charge_detail(id INTEGER,user_id INTEGER,station_id INTEGER,station_type TEXT,status TEXT,kwh REAL,total_fee REAL,charge_date TEXT,hour INTEGER,is_weekend INTEGER,platform TEXT,soc REAL,station_area TEXT);
      INSERT INTO ncs_dwd.dwd_charge_detail VALUES
      (1,1,1,'fast','completed',10,22,'2026-09-11',8,0,'Android',10,'北京'),
      (2,1,1,'fast','completed',5,11,'2026-09-13',8,1,'Android',50,'北京'),
      (3,2,2,'slow','charging',2,0,'2026-09-13',20,1,NULL,NULL,'深圳'),
      (4,2,2,'slow','cancelled',0,0,'2026-09-13',20,1,NULL,NULL,'深圳'),
      (5,3,2,'slow','reserved',0,0,'2026-09-13',20,1,NULL,NULL,'深圳');
      CREATE TABLE ncs_dwd.dwd_station(id INTEGER,station_name TEXT,station_area TEXT);
      INSERT INTO ncs_dwd.dwd_station VALUES(1,'北京站','北京'),(2,'深圳站','深圳');
      CREATE TABLE ncs_dwd.dwd_pile(id INTEGER,station_id INTEGER,station_type TEXT);
      INSERT INTO ncs_dwd.dwd_pile VALUES(1,1,'fast'),(2,1,'fast'),(3,2,'slow');
      CREATE TABLE ncs_dwd.dwd_user(id INTEGER);
      INSERT INTO ncs_dwd.dwd_user VALUES(1),(2),(3);
      CREATE TABLE ncs_dwd.dwd_quality(raw_count INTEGER,valid_count INTEGER,rejected_count INTEGER);
      INSERT INTO ncs_dwd.dwd_quality VALUES(6,5,1);
    """)
    return connection


def execute_sql_offline(connection):
    for name in ("02_dws_business.sql","03_ads_dashboard.sql"):
        source=(ROOT / "spark_sql" / name).read_text(encoding="utf-8")
        source="\n".join(line.split("--",1)[0] for line in source.splitlines())
        source=source.replace(" USING PARQUET AS"," AS").replace("${COST_PER_KWH}","0.6")
        source=source.replace("range(24)","(WITH RECURSIVE hours(id) AS (SELECT 0 UNION ALL SELECT id+1 FROM hours WHERE id<23) SELECT id FROM hours)")
        # Spark '/' is floating-point division; SQLite integer '/' is not.
        source=source.replace("/","*1.0/")
        connection.executescript(source)


def make_export(directory):
    connection=sql_fixture()
    execute_sql_offline(connection)
    manifest={"contract_version":1,"generated_at":datetime.now(timezone.utc).isoformat(),
              "analysis":{"module":"analytics/scripts/evcharging_analysis.py","script_sha256":digest(ROOT / "scripts/evcharging_analysis.py")},
              "quality":{"raw_count":6,"valid_count":5,"rejected_count":1,"platform_rows":2,"soc_rows":2,"cost_per_kwh":0.6},"tables":{}}
    for table,spec in TABLES.items():
        cursor=connection.execute(f"SELECT * FROM ncs_ads.{table}")
        rows=[]
        for row in cursor.fetchall():
            values=[]
            for value,(_,kind) in zip(row,spec["columns"]):
                if value is not None and kind.startswith("decimal"):
                    scale=int(re.findall(r"\d+",kind)[1]);value=f"{value:.{scale}f}"
                values.append(value)
            rows.append(values)
        path=directory / f"{table}.csv"
        with path.open("w",encoding="utf-8",newline="") as stream:
            csv.writer(stream,delimiter="|",lineterminator="\n").writerows(rows)
        manifest["tables"][table]={"rows":len(rows),"sha256":digest(path)}
    (directory / "manifest.json").write_text(json.dumps(manifest),encoding="utf-8")
    connection.close()
    return manifest


class OfflinePipelineTest(unittest.TestCase):
    def test_live_comparison_checks_values_and_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            manifest = make_export(directory)
            _, tables = validate_export(directory)
            data = {}
            for table, group in GROUPS.items():
                columns = [item[0] for item in TABLES[table]['columns']]
                rows = [dict(zip(columns, row)) for row in tables[table]]
                data[group] = rows[0] if group == 'overview' else rows
            payload = {'code': 0, 'data': data, 'metadata': dict(manifest, batch_id='fixture')}
            self.assertEqual(verify(directory, payload), 'fixture')
            data['overview']['sessions'] += 1
            with self.assertRaisesRegex(ValueError, 'API values differ'):
                verify(directory, payload)
            data['overview']['sessions'] -= 1
            payload['metadata']['batch_id'] = None
            with self.assertRaisesRegex(ValueError, 'batch identity'):
                verify(directory, payload)

    def test_sql_metrics_and_column_order(self):
        connection=sql_fixture();execute_sql_offline(connection)
        for name,spec in TABLES.items():
            cursor=connection.execute(f"SELECT * FROM ncs_ads.{name}")
            self.assertEqual([item[0] for item in cursor.description],[item[0] for item in spec["columns"]])
        kpi=connection.execute("SELECT * FROM ncs_ads.ads_kpi_overview").fetchone()
        self.assertEqual(kpi[:4],(3,17,33,2));self.assertAlmostEqual(kpi[4],16.667)
        self.assertEqual(connection.execute("SELECT * FROM ncs_ads.ads_platform_dist").fetchall(),[('Android',1)])
        self.assertEqual(connection.execute("SELECT sessions FROM ncs_ads.ads_hour_trend WHERE hour=8").fetchone()[0],2)
        self.assertEqual(connection.execute("SELECT COUNT(*) FROM ncs_ads.ads_hour_trend").fetchone()[0],24)
        self.assertEqual(connection.execute("SELECT daily_kwh FROM ncs_ads.ads_station_type_eff WHERE gun_type='fast'").fetchone()[0],2.5)
        self.assertIsNone(connection.execute("SELECT profit_rate FROM ncs_ads.ads_area_cost WHERE station_area='深圳'").fetchone()[0])
        self.assertEqual(connection.execute("SELECT COUNT(*) FROM ncs_ads.ads_user_radar WHERE dim_value IS NOT NULL").fetchone()[0],0)
        connection.close()

    def test_missing_optional_fields_produce_empty_results(self):
        connection=sql_fixture();connection.execute("UPDATE ncs_dwd.dwd_charge_detail SET platform=NULL,soc=NULL");execute_sql_offline(connection)
        for table in ('ads_platform_dist','ads_battery_health'):
            self.assertEqual(connection.execute(f"SELECT COUNT(*) FROM ncs_ads.{table}").fetchone()[0],0)
        connection.close()

    def test_no_charging_sessions_have_zero_hours_and_no_peak(self):
        connection=sql_fixture();connection.execute("UPDATE ncs_dwd.dwd_charge_detail SET status='reserved'");execute_sql_offline(connection)
        self.assertEqual(connection.execute("SELECT SUM(sessions),SUM(is_peak),COUNT(*) FROM ncs_ads.ads_hour_trend").fetchone(),(0,0,24))
        connection.close()

    def test_export_round_trip_and_checksum_rejection(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);make_export(directory)
            manifest,tables=validate_export(directory)
            self.assertEqual(tables['ads_kpi_overview'][0][2],Decimal('33'))
            (directory/'ads_kpi_overview.csv').write_text('corrupted',encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'checksum'):validate_export(directory)

    def test_nullable_numbers_and_quoted_pipe_names(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary)
            (directory/'ads_user_radar.csv').write_text('低频用户|消费金额|\n',encoding='utf-8')
            self.assertIsNone(read_table(directory,'ads_user_radar')[0][2])
            with (directory/'ads_platform_dist.csv').open('w',encoding='utf-8',newline='') as stream:
                csv.writer(stream,delimiter='|').writerow(['其他|网页',1])
            self.assertEqual(read_table(directory,'ads_platform_dist')[0][0],'其他|网页')

    def test_bad_numeric_values_and_duplicate_keys_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);path=directory/'ads_user_level_dist.csv'
            for value in ('NaN','1.5','9223372036854775808','-1'):
                path.write_text(f'低频用户|{value}\n',encoding='utf-8')
                with self.assertRaises(ValueError):read_table(directory,'ads_user_level_dist')
            path.write_text('低频用户|1\n低频用户|2\n',encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'duplicate'):read_table(directory,'ads_user_level_dist')

    def test_exports_without_analysis_identity_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);manifest=make_export(directory)
            del manifest['analysis']
            (directory/'manifest.json').write_text(json.dumps(manifest),encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'identity'):validate_export(directory)

    def test_sqlite_export_is_private_and_read_only(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);db=directory/'business.db'
            connection=sqlite3.connect(db)
            project=ROOT.parent
            connection.executescript((project/'database/schema.sql').read_text(encoding='utf-8'))
            connection.execute("INSERT INTO users(phone,nickname) VALUES('13800000000','测试')");connection.commit();connection.close()
            before=db.read_bytes();export_database(db,directory/'csv')
            users=(directory/'csv/users/users.csv').read_text(encoding='utf-8')
            self.assertNotIn('13800000000',users);self.assertNotIn('phone',users);self.assertEqual(before,db.read_bytes())


class FakeCursor:
    def __init__(self,connection):self.connection=connection;self.rows=[]
    def execute(self,query,params=()):
        self.connection.events.append(query)
        if query.startswith('SELECT GET_LOCK'):self.rows=[(1,)]
        elif 'information_schema.COLUMNS' in query:
            spec=TABLES[params[1]];self.rows=[(name,kind,'YES' if name in spec.get('nullable',[]) else 'NO') for name,kind in spec['columns']]
        elif query=='SHOW WARNINGS':self.rows=[]
        elif query.startswith('SELECT RELEASE_LOCK'):self.rows=[(1,)]
        elif query.startswith('INSERT INTO ads_area_cost SELECT') and self.connection.fail:raise RuntimeError('simulated late write failure')
        elif query.startswith('DELETE FROM ads_'):self.connection.pending.append(query)
    def executemany(self,query,rows):self.connection.events.append(query)
    def fetchone(self):return self.rows.pop(0)
    def fetchall(self):return self.rows
    def close(self):pass


class FakeConnection:
    database='charging_ads'
    def __init__(self,fail=False):self.fail=fail;self.events=[];self.pending=[];self.committed=[];self.rollbacks=0
    def cursor(self):return FakeCursor(self)
    def start_transaction(self):self.events.append('BEGIN')
    def commit(self):self.committed=list(self.pending);self.events.append('COMMIT')
    def rollback(self):self.pending.clear();self.rollbacks+=1


class ImportTransactionTest(unittest.TestCase):
    def test_late_failure_rolls_back_whole_batch(self):
        connection=FakeConnection(fail=True)
        with self.assertRaisesRegex(RuntimeError,'late write'):publish(connection,Path('/export'),{}, {table:[] for table in TABLES})
        self.assertEqual(connection.committed,[]);self.assertEqual(connection.pending,[]);self.assertEqual(connection.rollbacks,1)
    def test_all_stages_precede_single_publish_transaction(self):
        connection=FakeConnection();publish(connection,Path('/export'),{}, {table:[] for table in TABLES})
        begin=connection.events.index('BEGIN')
        self.assertEqual(sum(event.startswith('CREATE TEMPORARY') for event in connection.events[:begin]),10)
        self.assertEqual(connection.events.count('COMMIT'),1);self.assertEqual(len(connection.committed),10)


if __name__ == '__main__':unittest.main()
