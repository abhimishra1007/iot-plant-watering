#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Converts raw ADC counts to 0-100 % using the two-point calibration.
double moisture_percent(int raw, int dry_raw, int wet_raw);

struct ControllerSettings {
    double threshold_pct;
    double target_pct;
    int64_t max_runtime_s;
    int64_t cooldown_s;
};

enum class StopReason { TargetReached, MaxRuntime, Shutdown };
const char *to_string(StopReason r);

struct PumpEvent {
    enum class Kind { Started, Stopped } kind;
    StopReason reason = StopReason::TargetReached;  // only meaningful for Stopped
};

// Closed-loop watering logic with no I/O, so it can be unit tested with a
// fake clock. Call update() with each new reading; it returns the pump state
// the caller should apply and, if the state changed, an event to log.
//
// Safeguards against overwatering:
//   * hysteresis: start below threshold_pct, stop at target_pct
//   * max_runtime_s: a single run is cut off even if the target isn't reached
//     (e.g. a dry reservoir or a disconnected probe)
//   * cooldown_s: after any run, wait before pumping again so water can soak
//     into the soil and reach the probe
class WateringController {
public:
    explicit WateringController(const ControllerSettings &s) : s_(s) {}

    struct Decision {
        bool pump_on;
        std::optional<PumpEvent> event;
    };

    Decision update(int64_t now_s, double moisture_pct);
    std::optional<PumpEvent> shutdown();

    bool pump_on() const { return pump_on_; }
    bool in_cooldown(int64_t now_s) const;

private:
    ControllerSettings s_;
    bool pump_on_ = false;
    int64_t started_at_ = 0;
    std::optional<int64_t> last_stop_;
};
