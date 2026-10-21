#pragma once

#include <string>
#include "hardware.hpp"

// MCP3008 10-bit ADC on the Linux spidev interface (e.g. /dev/spidev0.0).
class Mcp3008 {
public:
    Mcp3008(const std::string &device, uint32_t speed_hz = 1000000);
    ~Mcp3008();
    Mcp3008(const Mcp3008 &) = delete;
    Mcp3008 &operator=(const Mcp3008 &) = delete;

    // Single-ended read of channel 0-7.
    uint16_t read_channel(uint8_t channel);

private:
    int fd_ = -1;
    uint32_t speed_hz_;
};

// One ADC channel exposed as an AnalogInput.
class Mcp3008Channel : public AnalogInput {
public:
    Mcp3008Channel(Mcp3008 &adc, uint8_t channel) : adc_(adc), channel_(channel) {}
    uint16_t read() override { return adc_.read_channel(channel_); }

private:
    Mcp3008 &adc_;
    uint8_t channel_;
};
