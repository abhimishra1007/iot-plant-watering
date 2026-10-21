// Plant watering controller for a Raspberry Pi.
//
//   soil probe -> MCP3008 (SPI) -> this program -> GPIO -> relay -> pump
//                                         |
//                                         +-> SQLite <- Flask dashboard
//
// Usage:
//   plant_controller --config plant.conf              run on the Pi
//   plant_controller --simulate --speedup 60 [...]    run anywhere, simulated pot
//   plant_controller --config plant.conf --calibrate  print raw ADC readings

#include <chrono>
#include <csignal>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "config.hpp"
#include "data_logger.hpp"
#include "gpio_relay.hpp"
#include "mcp3008.hpp"
#include "simulation.hpp"
#include "watering_controller.hpp"

namespace {
volatile std::sig_atomic_t g_stop = 0;

void on_signal(int) { g_stop = 1; }

void usage(const char *argv0)
{
    std::cerr << "usage: " << argv0
              << " [--config FILE] [--simulate] [--speedup N] [--duration SECONDS] [--calibrate]\n";
}

int average_reading(AnalogInput &in, int samples)
{
    long sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += in.read();
    }
    return static_cast<int>(sum / samples);
}
}  // namespace

int main(int argc, char **argv)
{
    std::string config_path;
    bool simulate = false;
    bool calibrate = false;
    double speedup = 1.0;
    long duration_s = -1;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--calibrate") {
            calibrate = true;
        } else if (arg == "--simulate") {
            simulate = true;
        } else if (arg == "--speedup" && i + 1 < argc) {
            speedup = std::stod(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            duration_s = std::stol(argv[++i]);
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    try {
        Config cfg = config_path.empty() ? Config{} : load_config(config_path);

        std::unique_ptr<SimulatedPlant> plant;
        std::unique_ptr<Mcp3008> adc;
        std::unique_ptr<AnalogInput> sensor;
        std::unique_ptr<PumpOutput> pump;

        if (simulate) {
            plant = std::make_unique<SimulatedPlant>(cfg.dry_raw, cfg.wet_raw, 45.0);
            sensor = std::make_unique<SimulatedPlant::Sensor>(*plant);
            pump = std::make_unique<SimulatedPlant::Pump>(*plant);
        } else {
            adc = std::make_unique<Mcp3008>(cfg.spi_device);
            sensor = std::make_unique<Mcp3008Channel>(*adc, static_cast<uint8_t>(cfg.adc_channel));
            pump = std::make_unique<GpioRelay>(cfg.gpio_chip, cfg.relay_line, cfg.relay_active_low);
        }

        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);

        if (calibrate) {
            // Hold the probe in dry air, then in water, and copy the two raw
            // values into dry_raw / wet_raw in the config file.
            std::cout << "raw ADC readings, Ctrl+C to stop\n";
            while (!g_stop) {
                std::cout << average_reading(*sensor, cfg.samples_per_reading) << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            return 0;
        }

        DataLogger log(cfg.database);
        WateringController ctl({cfg.threshold_pct, cfg.target_pct, cfg.max_runtime_s, cfg.cooldown_s});

        std::cout << (simulate ? "[sim] " : "") << "watering below " << cfg.threshold_pct
                  << "%, stopping at " << cfg.target_pct << "%, max run " << cfg.max_runtime_s
                  << " s, cooldown " << cfg.cooldown_s << " s; logging to " << cfg.database << "\n";

        // The loop ticks once per (simulated) second so the max-runtime cutoff is
        // enforced to within a second; readings are stored every sample_interval_s.
        const int64_t start = static_cast<int64_t>(std::time(nullptr));
        int64_t now = start;
        int64_t last_logged = now - cfg.sample_interval_s;
        int64_t event_id = -1;
        double pct = 0.0;

        while (!g_stop && (duration_s < 0 || now - start < duration_s)) {
            int raw = average_reading(*sensor, cfg.samples_per_reading);
            pct = moisture_percent(raw, cfg.dry_raw, cfg.wet_raw);

            auto decision = ctl.update(now, pct);
            pump->set(decision.pump_on);

            if (decision.event) {
                if (decision.event->kind == PumpEvent::Kind::Started) {
                    event_id = log.start_watering(now, pct);
                    std::cout << "pump ON  at " << pct << "%\n";
                } else {
                    log.finish_watering(event_id, now, pct, to_string(decision.event->reason));
                    std::cout << "pump OFF at " << pct << "% (" << to_string(decision.event->reason) << ")\n";
                }
            }

            if (decision.event || now - last_logged >= cfg.sample_interval_s) {
                log.log_reading(now, raw, pct, pump->is_on());
                last_logged = now;
            }

            if (simulate) {
                plant->step(1.0);
                std::this_thread::sleep_for(std::chrono::duration<double>(1.0 / speedup));
                now += 1;
            } else {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                now = static_cast<int64_t>(std::time(nullptr));
            }
        }

        if (auto ev = ctl.shutdown()) {
            pump->set(false);
            log.finish_watering(event_id, now, pct, to_string(ev->reason));
        }
        pump->set(false);
        std::cout << "stopped\n";
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
