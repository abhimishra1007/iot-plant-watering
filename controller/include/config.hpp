#pragma once

#include <string>

struct Config {
    // Hardware
    std::string spi_device = "/dev/spidev0.0";
    int adc_channel = 0;
    std::string gpio_chip = "/dev/gpiochip0";
    int relay_line = 17;            // BCM GPIO17
    bool relay_active_low = true;

    // Calibration: raw ADC readings with the probe in dry air and in water.
    // Capacitive probes read higher when dry.
    int dry_raw = 820;
    int wet_raw = 410;

    // Control
    double threshold_pct = 35.0;    // start watering below this
    double target_pct = 55.0;       // stop watering once this is reached
    int max_runtime_s = 15;         // hard cap on a single pump run
    int cooldown_s = 600;           // minimum time between pump runs
    int samples_per_reading = 8;    // ADC samples averaged per reading

    // Timing and storage
    int sample_interval_s = 5;
    std::string database = "plant.db";
};

// Parses a simple "key = value" file; '#' starts a comment.
// Throws std::runtime_error on unknown keys or bad values.
Config load_config(const std::string &path);
