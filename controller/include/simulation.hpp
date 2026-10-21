#pragma once

#include "hardware.hpp"

// A crude pot of soil for running the controller without a Raspberry Pi:
// moisture drains slowly and rises while the pump is on.
class SimulatedPlant {
public:
    SimulatedPlant(int dry_raw, int wet_raw, double start_pct);
    void step(double dt_s);  // advance the model by dt_s seconds

    class Sensor : public AnalogInput {
    public:
        explicit Sensor(SimulatedPlant &p) : p_(p) {}
        uint16_t read() override;
    private:
        SimulatedPlant &p_;
    };

    class Pump : public PumpOutput {
    public:
        explicit Pump(SimulatedPlant &p) : p_(p) {}
        void set(bool on) override { p_.pump_on_ = on; }
        bool is_on() const override { return p_.pump_on_; }
    private:
        SimulatedPlant &p_;
    };

private:
    int dry_raw_;
    int wet_raw_;
    double pct_;
    bool pump_on_ = false;
};
