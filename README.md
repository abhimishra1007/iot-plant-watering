# IoT Plant Monitoring & Watering System

A closed-loop watering system for a Raspberry Pi. A C++ controller reads soil
moisture through an MCP3008 ADC, switches a pump through a relay, and logs every
reading and pump run to SQLite. A Python Flask dashboard shows live moisture and
the full watering history with Chart.js graphs.

> Runs on a Raspberry Pi with the parts below. Without the hardware you can still
> build everything and run the controller in `--simulate` mode to feed the dashboard.

```
soil probe ─► MCP3008 ─SPI─► plant_controller (C++) ─GPIO─► relay ─► pump
                                    │
                                    ▼
                               plant.db (SQLite) ◄── Flask dashboard ◄── browser
```

## Control logic

`WateringController` (`controller/src/watering_controller.cpp`) has no I/O, so it is unit tested with a fake clock.

- **Calibrated threshold**: raw ADC counts become 0–100 % using two calibration
  points (probe in dry air and in water). The pump starts when moisture drops below
  `threshold_pct` and stops once it reaches `target_pct`, so it doesn't chatter
  around a single set point.
- **Max runtime**: a single run is cut off after `max_runtime_s` even if the target
  isn't reached, which covers an empty reservoir, a probe that slipped out of the soil,
  or a disconnected sensor.
- **Cooldown**: after any run the pump stays off for `cooldown_s` so water can soak
  down to the probe before the next decision.
- **Safe shutdown**: the relay line is requested in the "off" state, and SIGINT/SIGTERM
  turn the pump off and close out the watering record before exit.

The loop ticks once a second (so the runtime cap is enforced to the second), averages
several ADC samples per reading, and stores a reading every `sample_interval_s`
plus one at every pump start and stop.

## Hardware

| Part | Notes |
|------|-------|
| Raspberry Pi (any with the 40-pin header) | SPI enabled with `raspi-config` |
| MCP3008 10-bit ADC | SPI0, CE0 |
| Capacitive soil moisture sensor | analog out to MCP3008 CH0 |
| 1-channel 5 V relay module | IN on GPIO17 |
| Small 5 V submersible pump | powered from its own supply through the relay |

MCP3008 wiring: VDD and VREF to 3.3 V, AGND and DGND to GND, CLK to GPIO11 (SCLK),
DOUT to GPIO9 (MISO), DIN to GPIO10 (MOSI), CS to GPIO8 (CE0).

## Build and run the controller

Needs CMake, a C++17 compiler, and the SQLite development package
(`sudo apt install cmake g++ libsqlite3-dev`).

```sh
cd controller
cmake -S . -B build
cmake --build build
ctest --test-dir build          # controller unit tests

cp plant.conf.example plant.conf
./build/plant_controller --config plant.conf --calibrate   # read dry_raw / wet_raw
./build/plant_controller --config plant.conf               # run
```

No Pi? Simulate a pot of soil, 60x faster than real time:

```sh
mkdir -p ../data
./build/plant_controller --simulate --speedup 60 --config plant.conf
```

The data logs to `data/plant.db` (the `database` key in the config, relative to where you run it).

## Run the dashboard

```sh
cd dashboard
python3 -m venv .venv && . .venv/bin/activate
pip install -r requirements.txt
PLANT_DB=../data/plant.db flask --app app run --host 0.0.0.0
python -m unittest discover -s tests     # API tests
```

Open `http://<pi-address>:5000`. The page refreshes every 5 seconds and shows:

- current moisture, pump state, last watering, and total runs
- a moisture-over-time chart with pump-on periods and the threshold and target lines
- a bar chart of pump run lengths and a table of every watering (start/end moisture, why it stopped)

API: `GET /api/readings?hours=24`, `GET /api/events?hours=24`, `GET /api/status`.
The dashboard opens the database read-only; the controller is the only writer
(SQLite WAL mode lets both run at once).

## Layout

```
controller/
  include/, src/
    watering_controller   threshold / max-runtime / cooldown logic
    mcp3008               SPI driver (Linux spidev)
    gpio_relay            relay output (Linux GPIO character device)
    data_logger           SQLite schema and inserts
    simulation            simulated sensor and pump for testing without hardware
    config                key = value config parser
    main.cpp              control loop
  tests/test_controller.cpp
  plant.conf.example
dashboard/
  app.py                  Flask app and JSON API
  templates/index.html    page
  static/dashboard.js     Chart.js charts and polling
  tests/test_app.py
```
