"""Flask dashboard for the plant watering controller.

Reads the SQLite database the C++ controller writes and serves a page with
live soil moisture and watering history charts (Chart.js).

    PLANT_DB=../data/plant.db flask --app app run --host 0.0.0.0
"""
import os
import sqlite3
import time

from flask import Flask, abort, g, jsonify, render_template, request

DEFAULT_DB = os.path.join(os.path.dirname(__file__), "..", "data", "plant.db")


def create_app(db_path=None):
    app = Flask(__name__)
    app.config["DB_PATH"] = db_path or os.environ.get("PLANT_DB", DEFAULT_DB)
    app.config["THRESHOLD_PCT"] = float(os.environ.get("PLANT_THRESHOLD_PCT", 35))
    app.config["TARGET_PCT"] = float(os.environ.get("PLANT_TARGET_PCT", 55))

    def get_db():
        if "db" not in g:
            path = os.path.abspath(app.config["DB_PATH"])
            if not os.path.exists(path):
                abort(503, description=f"database not found: {path}")
            # Read-only: the controller is the only writer.
            g.db = sqlite3.connect(f"file:{path}?mode=ro", uri=True)
            g.db.row_factory = sqlite3.Row
        return g.db

    @app.teardown_appcontext
    def close_db(_exc):
        db = g.pop("db", None)
        if db is not None:
            db.close()

    def window_start():
        """Start of the requested time window, ending at the newest reading."""
        hours = request.args.get("hours", default=24, type=float)
        hours = min(max(hours, 0.1), 24 * 90)
        latest = get_db().execute("SELECT MAX(ts) FROM readings").fetchone()[0]
        end = latest if latest is not None else int(time.time())
        return int(end - hours * 3600)

    @app.route("/")
    def index():
        return render_template(
            "index.html",
            threshold=app.config["THRESHOLD_PCT"],
            target=app.config["TARGET_PCT"],
        )

    @app.route("/api/readings")
    def readings():
        rows = get_db().execute(
            "SELECT ts, raw, moisture_pct, pump_on FROM readings WHERE ts >= ? ORDER BY ts",
            (window_start(),),
        ).fetchall()
        # Thin out long windows so the browser isn't sent tens of thousands of points.
        max_points = 1500
        step = max(1, len(rows) // max_points)
        return jsonify([dict(r) for r in rows[::step]])

    @app.route("/api/events")
    def events():
        rows = get_db().execute(
            "SELECT id, started_at, ended_at, duration_s, start_moisture, end_moisture, stop_reason "
            "FROM watering_events WHERE started_at >= ? ORDER BY started_at",
            (window_start(),),
        ).fetchall()
        return jsonify([dict(r) for r in rows])

    @app.route("/api/status")
    def status():
        db = get_db()
        latest = db.execute(
            "SELECT ts, raw, moisture_pct, pump_on FROM readings ORDER BY ts DESC LIMIT 1"
        ).fetchone()
        last_event = db.execute(
            "SELECT started_at, duration_s, stop_reason FROM watering_events "
            "ORDER BY started_at DESC LIMIT 1"
        ).fetchone()
        totals = db.execute(
            "SELECT COUNT(*) AS runs, COALESCE(SUM(duration_s), 0) AS pump_seconds "
            "FROM watering_events"
        ).fetchone()
        return jsonify(
            {
                "latest": dict(latest) if latest else None,
                "last_watering": dict(last_event) if last_event else None,
                "total_runs": totals["runs"],
                "total_pump_seconds": totals["pump_seconds"],
                "threshold_pct": app.config["THRESHOLD_PCT"],
                "target_pct": app.config["TARGET_PCT"],
            }
        )

    return app


app = create_app()

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)
