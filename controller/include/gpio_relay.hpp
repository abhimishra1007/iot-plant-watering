#pragma once

#include <string>
#include "hardware.hpp"

// Relay driven by one GPIO line through the Linux GPIO character device
// (uAPI v2, /dev/gpiochipN). Many relay boards are active-low.
class GpioRelay : public PumpOutput {
public:
    GpioRelay(const std::string &chip, unsigned line, bool active_low);
    ~GpioRelay() override;
    GpioRelay(const GpioRelay &) = delete;
    GpioRelay &operator=(const GpioRelay &) = delete;

    void set(bool on) override;
    bool is_on() const override { return on_; }

private:
    int line_fd_ = -1;
    bool on_ = false;
};
