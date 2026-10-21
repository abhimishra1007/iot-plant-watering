#pragma once

#include <cstdint>

// Interfaces the control loop depends on, so the same logic runs against real
// hardware on the Raspberry Pi or against a simulation on a laptop.

class AnalogInput {
public:
    virtual ~AnalogInput() = default;
    // Returns a raw 10-bit reading (0-1023).
    virtual uint16_t read() = 0;
};

class PumpOutput {
public:
    virtual ~PumpOutput() = default;
    virtual void set(bool on) = 0;
    virtual bool is_on() const = 0;
};
