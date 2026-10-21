#include "mcp3008.hpp"

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace {
std::runtime_error sys_error(const std::string &what)
{
    return std::runtime_error(what + ": " + std::strerror(errno));
}
}  // namespace

Mcp3008::Mcp3008(const std::string &device, uint32_t speed_hz) : speed_hz_(speed_hz)
{
    fd_ = ::open(device.c_str(), O_RDWR);
    if (fd_ < 0) {
        throw sys_error("open " + device);
    }

    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    if (ioctl(fd_, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz_) < 0) {
        int saved = errno;
        ::close(fd_);
        errno = saved;
        throw sys_error("configure " + device);
    }
}

Mcp3008::~Mcp3008()
{
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

uint16_t Mcp3008::read_channel(uint8_t channel)
{
    if (channel > 7) {
        throw std::invalid_argument("MCP3008 channel must be 0-7");
    }

    // Datasheet figure 6-1: start bit, then SGL/DIFF=1 and the 3 channel bits,
    // then 10 result bits clocked out across the last two bytes.
    uint8_t tx[3] = {0x01, static_cast<uint8_t>((0x08 | channel) << 4), 0x00};
    uint8_t rx[3] = {0, 0, 0};

    spi_ioc_transfer tr{};
    tr.tx_buf = reinterpret_cast<uintptr_t>(tx);
    tr.rx_buf = reinterpret_cast<uintptr_t>(rx);
    tr.len = sizeof tx;
    tr.speed_hz = speed_hz_;
    tr.bits_per_word = 8;

    if (ioctl(fd_, SPI_IOC_MESSAGE(1), &tr) < 0) {
        throw sys_error("SPI transfer");
    }
    return static_cast<uint16_t>(((rx[1] & 0x03) << 8) | rx[2]);
}
