#include "simulation.hpp"

#include <algorithm>
#include <cstdlib>

SimulatedPlant::SimulatedPlant(int dry_raw, int wet_raw, double start_pct)
    : dry_raw_(dry_raw), wet_raw_(wet_raw), pct_(start_pct)
{
}

void SimulatedPlant::step(double dt_s)
{
    const double drain_per_s = 0.02;  // evaporation and uptake
    const double fill_per_s = 2.0;    // pump flow
    pct_ += (pump_on_ ? fill_per_s : -drain_per_s) * dt_s;
    pct_ = std::clamp(pct_, 0.0, 100.0);
}

uint16_t SimulatedPlant::Sensor::read()
{
    double raw = p_.dry_raw_ - (p_.pct_ / 100.0) * (p_.dry_raw_ - p_.wet_raw_);
    raw += (std::rand() % 7) - 3;  // a little ADC noise
    return static_cast<uint16_t>(std::clamp(raw, 0.0, 1023.0));
}
