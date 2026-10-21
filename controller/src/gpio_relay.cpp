#include "gpio_relay.hpp"

#include <fcntl.h>
#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

GpioRelay::GpioRelay(const std::string &chip, unsigned line, bool active_low)
{
    int chip_fd = ::open(chip.c_str(), O_RDWR | O_CLOEXEC);
    if (chip_fd < 0) {
        throw std::runtime_error("open " + chip + ": " + std::strerror(errno));
    }

    gpio_v2_line_request req{};
    req.offsets[0] = line;
    req.num_lines = 1;
    std::strncpy(req.consumer, "plant-watering", sizeof req.consumer - 1);
    req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
    if (active_low) {
        req.config.flags |= GPIO_V2_LINE_FLAG_ACTIVE_LOW;
    }
    // Request the line already driven to "off" so the pump never twitches at startup.
    req.config.num_attrs = 1;
    req.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
    req.config.attrs[0].attr.values = 0;
    req.config.attrs[0].mask = 1;

    int rc = ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &req);
    int saved = errno;
    ::close(chip_fd);
    if (rc < 0) {
        throw std::runtime_error("request GPIO line " + std::to_string(line) + ": " +
                                 std::strerror(saved));
    }
    line_fd_ = req.fd;
}

GpioRelay::~GpioRelay()
{
    if (line_fd_ >= 0) {
        try {
            set(false);
        } catch (...) {
        }
        ::close(line_fd_);
    }
}

void GpioRelay::set(bool on)
{
    gpio_v2_line_values values{};
    values.mask = 1;
    values.bits = on ? 1 : 0;
    if (ioctl(line_fd_, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
        throw std::runtime_error(std::string("set GPIO value: ") + std::strerror(errno));
    }
    on_ = on;
}
