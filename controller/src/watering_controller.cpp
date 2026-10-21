#include "watering_controller.hpp"

#include <algorithm>

double moisture_percent(int raw, int dry_raw, int wet_raw)
{
    double pct = 100.0 * (dry_raw - raw) / static_cast<double>(dry_raw - wet_raw);
    return std::clamp(pct, 0.0, 100.0);
}

const char *to_string(StopReason r)
{
    switch (r) {
    case StopReason::TargetReached: return "target_reached";
    case StopReason::MaxRuntime:    return "max_runtime";
    case StopReason::Shutdown:      return "shutdown";
    }
    return "unknown";
}

bool WateringController::in_cooldown(int64_t now_s) const
{
    return last_stop_ && (now_s - *last_stop_) < s_.cooldown_s;
}

WateringController::Decision WateringController::update(int64_t now_s, double moisture_pct)
{
    if (pump_on_) {
        std::optional<StopReason> stop;
        if (moisture_pct >= s_.target_pct) {
            stop = StopReason::TargetReached;
        } else if (now_s - started_at_ >= s_.max_runtime_s) {
            stop = StopReason::MaxRuntime;
        }
        if (stop) {
            pump_on_ = false;
            last_stop_ = now_s;
            return {false, PumpEvent{PumpEvent::Kind::Stopped, *stop}};
        }
        return {true, std::nullopt};
    }

    if (moisture_pct < s_.threshold_pct && !in_cooldown(now_s)) {
        pump_on_ = true;
        started_at_ = now_s;
        return {true, PumpEvent{PumpEvent::Kind::Started}};
    }
    return {false, std::nullopt};
}

std::optional<PumpEvent> WateringController::shutdown()
{
    if (!pump_on_) {
        return std::nullopt;
    }
    pump_on_ = false;
    return PumpEvent{PumpEvent::Kind::Stopped, StopReason::Shutdown};
}
