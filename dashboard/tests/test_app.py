"""Run with: python -m unittest discover -s tests"""
import os
import sqlite3
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from app import create_app  # noqa: E402

SCHEMA = """
CREATE TABLE readings (id INTEGER PRIMARY KEY, ts INTEGER, raw INTEGER, moisture_pct REAL, pump_on INTEGER);
CREATE TABLE watering_events (id INTEGER PRIMARY KEY, started_at INTEGER, ended_at INTEGER, duration_s INTEGER,
                              start_moisture REAL, end_moisture REAL, stop_reason TEXT);
"""


class DashboardTest(unittest.TestCase):
    def setUp(self):
        fd, self.db_path = tempfile.mkstemp(suffix=".db")
        os.close(fd)
        db = sqlite3.connect(self.db_path)
        db.executescript(SCHEMA)
        now = 1_800_000_000
        db.executemany(
            "INSERT INTO readings (ts, raw, moisture_pct, pump_on) VALUES (?, ?, ?, ?)",
            [(now - 7200, 700, 30.0, 0), (now - 3600, 690, 33.0, 1), (now, 600, 52.0, 0)],
        )
        db.execute(
            "INSERT INTO watering_events (started_at, ended_at, duration_s, start_moisture, end_moisture, stop_reason) "
            "VALUES (?, ?, 12, 33.0, 55.5, 'target_reached')",
            (now - 3600, now - 3588),
        )
        db.commit()
        db.close()
        self.client = create_app(self.db_path).test_client()

    def tearDown(self):
        os.remove(self.db_path)

    def test_readings_window_is_relative_to_latest(self):
        all_rows = self.client.get("/api/readings?hours=24").get_json()
        self.assertEqual(len(all_rows), 3)
        recent = self.client.get("/api/readings?hours=1.5").get_json()
        self.assertEqual([r["moisture_pct"] for r in recent], [33.0, 52.0])

    def test_events(self):
        events = self.client.get("/api/events").get_json()
        self.assertEqual(len(events), 1)
        self.assertEqual(events[0]["stop_reason"], "target_reached")

    def test_status(self):
        s = self.client.get("/api/status").get_json()
        self.assertEqual(s["latest"]["moisture_pct"], 52.0)
        self.assertEqual(s["total_runs"], 1)
        self.assertEqual(s["total_pump_seconds"], 12)

    def test_index_renders(self):
        res = self.client.get("/")
        self.assertEqual(res.status_code, 200)
        self.assertIn(b"moistureChart", res.data)

    def test_missing_database_returns_503(self):
        client = create_app("/nonexistent/plant.db").test_client()
        self.assertEqual(client.get("/api/status").status_code, 503)


if __name__ == "__main__":
    unittest.main()
