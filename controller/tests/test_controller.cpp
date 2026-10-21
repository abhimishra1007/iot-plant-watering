// Unit tests for the watering logic. No framework needed: build and run with
// `cmake --build build && ctest --test-dir build`.
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "watering_controller.hpp"

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while (0)

static ControllerSettings settings()
{
    return {35.0, 55.0, 15, 600};
}

static void test_moisture_percent_calibration()
{
    CHECK(moisture_percent(820, 820, 410) == 0.0);
    CHECK(moisture_percent(410, 820, 410) == 100.0);
    CHECK(std::fabs(moisture_percent(615, 820, 410) - 50.0) < 1e-9);
    CHECK(moisture_percent(900, 820, 410) == 0.0);    // drier than calibration: clamp
    CHECK(moisture_percent(300, 820, 410) == 100.0);  // wetter than calibration: clamp
}

static void test_starts_below_threshold_and_stops_at_target()
{
    WateringController c(settings());
    auto d = c.update(0, 40.0);
    CHECK(!d.pump_on && !d.event);

    d = c.update(1, 34.0);
    CHECK(d.pump_on && d.event && d.event->kind == PumpEvent::Kind::Started);

    d = c.update(2, 45.0);  // between threshold and target: keep going (hysteresis)
    CHECK(d.pump_on && !d.event);

    d = c.update(3, 56.0);
    CHECK(!d.pump_on && d.event && d.event->kind == PumpEvent::Kind::Stopped);
    CHECK(d.event->reason == StopReason::TargetReached);
}

static void test_max_runtime_cuts_off_pump()
{
    WateringController c(settings());
    c.update(100, 10.0);
    for (int t = 101; t < 115; t++) {
        CHECK(c.update(t, 12.0).pump_on);
    }
    auto d = c.update(115, 12.0);
    CHECK(!d.pump_on && d.event && d.event->reason == StopReason::MaxRuntime);
}

static void test_cooldown_blocks_restart()
{
    WateringController c(settings());
    c.update(0, 10.0);
    c.update(15, 10.0);  // max runtime stop at t=15
    CHECK(!c.pump_on());

    CHECK(!c.update(16, 10.0).pump_on);
    CHECK(!c.update(614, 10.0).pump_on);
    CHECK(c.in_cooldown(614));

    auto d = c.update(615, 10.0);
    CHECK(d.pump_on && d.event && d.event->kind == PumpEvent::Kind::Started);
}

static void test_shutdown_reports_running_pump()
{
    WateringController c(settings());
    CHECK(!c.shutdown());
    c.update(0, 10.0);
    auto ev = c.shutdown();
    CHECK(ev && ev->reason == StopReason::Shutdown);
    CHECK(!c.pump_on());
}

int main()
{
    test_moisture_percent_calibration();
    test_starts_below_threshold_and_stops_at_target();
    test_max_runtime_cuts_off_pump();
    test_cooldown_blocks_restart();
    test_shutdown_reports_running_pump();

    if (failures) {
        std::printf("%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("All controller tests passed\n");
    return EXIT_SUCCESS;
}
