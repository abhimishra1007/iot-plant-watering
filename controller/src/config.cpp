#include "config.hpp"

#include <fstream>
#include <functional>
#include <map>
#include <stdexcept>

namespace {
std::string trim(const std::string &s)
{
    const char *ws = " \t\r\n";
    auto start = s.find_first_not_of(ws);
    if (start == std::string::npos) {
        return "";
    }
    return s.substr(start, s.find_last_not_of(ws) - start + 1);
}

bool parse_bool(const std::string &v)
{
    if (v == "true" || v == "1" || v == "yes") return true;
    if (v == "false" || v == "0" || v == "no") return false;
    throw std::invalid_argument("expected true/false");
}
}  // namespace

Config load_config(const std::string &path)
{
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open config file " + path);
    }

    Config c;
    std::map<std::string, std::function<void(const std::string &)>> setters = {
        {"spi_device", [&](const std::string &v) { c.spi_device = v; }},
        {"adc_channel", [&](const std::string &v) { c.adc_channel = std::stoi(v); }},
        {"gpio_chip", [&](const std::string &v) { c.gpio_chip = v; }},
        {"relay_line", [&](const std::string &v) { c.relay_line = std::stoi(v); }},
        {"relay_active_low", [&](const std::string &v) { c.relay_active_low = parse_bool(v); }},
        {"dry_raw", [&](const std::string &v) { c.dry_raw = std::stoi(v); }},
        {"wet_raw", [&](const std::string &v) { c.wet_raw = std::stoi(v); }},
        {"threshold_pct", [&](const std::string &v) { c.threshold_pct = std::stod(v); }},
        {"target_pct", [&](const std::string &v) { c.target_pct = std::stod(v); }},
        {"max_runtime_s", [&](const std::string &v) { c.max_runtime_s = std::stoi(v); }},
        {"cooldown_s", [&](const std::string &v) { c.cooldown_s = std::stoi(v); }},
        {"samples_per_reading", [&](const std::string &v) { c.samples_per_reading = std::stoi(v); }},
        {"sample_interval_s", [&](const std::string &v) { c.sample_interval_s = std::stoi(v); }},
        {"database", [&](const std::string &v) { c.database = v; }},
    };

    std::string line;
    int line_no = 0;
    while (std::getline(in, line)) {
        line_no++;
        auto hash = line.find('#');
        if (hash != std::string::npos) {
            line.erase(hash);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            throw std::runtime_error(path + ":" + std::to_string(line_no) + ": expected key = value");
        }
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        auto it = setters.find(key);
        if (it == setters.end()) {
            throw std::runtime_error(path + ":" + std::to_string(line_no) + ": unknown key '" + key + "'");
        }
        try {
            it->second(value);
        } catch (const std::exception &) {
            throw std::runtime_error(path + ":" + std::to_string(line_no) + ": bad value for " + key);
        }
    }

    if (c.dry_raw == c.wet_raw) {
        throw std::runtime_error("dry_raw and wet_raw must differ");
    }
    if (c.target_pct <= c.threshold_pct) {
        throw std::runtime_error("target_pct must be greater than threshold_pct");
    }
    return c;
}
