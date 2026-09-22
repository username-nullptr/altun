# I²C and SPI buses

[Back to the Linux module guide](../linux.md) · [Back to the documentation index](../README.md)

The bus interfaces are in `libempp::bus` and use the `empp.linux` link target. I²C uses Linux `i2c-dev`, while SPI uses Linux `spidev`. Both support synchronous and asynchronous LibGS completion tokens.

## I²C

`libempp::bus::i2c` uses 7-bit device addresses and an 8-bit register address by default:

```cpp
#include <libempp/linux/bus/i2c.h>
#include <iostream>

int main()
{
    std::error_code error;
    libempp::bus::i2c device;
    device.open({"/dev/i2c-1", 0x48}, error);
    if(error)
    {
        std::cerr << error.message() << '\n';
        return 1;
    }

    std::uint8_t value = 0;
    const auto transferred = device.read(
        0x00,
        libgs::mutable_buffer(&value, sizeof(value)),
        error
    );
    if(error)
        return 1;

    std::cout << "read " << transferred << " byte(s)\n";
}
```

Write a register with:

```cpp
std::uint8_t value = 0x80;
device.write(0x01, libgs::const_buffer(&value, sizeof(value)), error);
```

Select a 16-bit register address with a template argument:

```cpp
device.read<libempp::bus::reg_bit16>(
    0x0120,
    libgs::mutable_buffer(&value, sizeof(value)),
    error
);
```

When the read length is known, the interface can return a `std::array` directly. The array type is the first template argument:

```cpp
const auto values = device.read<std::array<std::uint8_t, 4>>(0x00, error);
const auto wide_reg = device.read<std::array<std::uint8_t, 2>,
    libempp::bus::reg_bit16>(0x0120, error);
```

The third argument to `i2c::node` is a timeout in milliseconds and defaults to 30 ms. During asynchronous writes, a caller-provided buffer must remain valid until the operation completes. Detached writes copy the transmitted data. The asynchronous `read<Buffer>()` overload that returns an array retains its own storage and supplies `Buffer` as the completion value; it does not accept a detached token.

## SPI

`libempp::bus::spi` supports half-duplex reads and writes and full-duplex transfers. A node defaults to mode 0, a 500 kHz clock, and 8 bits per word:

```cpp
#include <libempp/linux/bus/spi.h>
#include <array>

int main()
{
    std::error_code error;
    libempp::bus::spi device;
    device.open({"/dev/spidev0.0", 1'000'000}, error);
    if(error)
        return 1;

    const std::array<std::uint8_t, 4> command {0x9f, 0x00, 0x00, 0x00};
    std::array<std::uint8_t, 4> response {};
    device.transfer(
        libgs::const_buffer(command.data(), command.size()),
        libgs::mutable_buffer(response.data(), response.size()),
        error
    );
    return error ? 1 : 0;
}
```

The transmit and receive buffers passed to `transfer()` must have equal lengths. Use `write()` or `read()` for transmit-only or receive-only operations. Fixed-length reads can also return an array directly:

```cpp
const auto id = device.read<std::array<std::uint8_t, 3>>(error);
auto future = device.read<std::array<std::uint8_t, 3>>(libgs::use_future);
```

This node uses mode 3, 16 bits per word, a 4 MHz clock, a 12 µs transfer delay, and requests a chip-select toggle after the transfer:

```cpp
using namespace std::chrono_literals;
libempp::bus::spi::node sensor(
    "/dev/spidev0.0",
    4'000'000,
    libempp::bus::spi_mode3,
    16,
    12us,
    true
);
```

Caller-provided buffers for asynchronous reads and full-duplex transfers must remain valid until completion. Array-returning asynchronous reads retain their own buffer. Detached asynchronous writes copy transmitted data.

## Troubleshooting order

1. Confirm that `/dev/i2c-*` or `/dev/spidev*` exists and is accessible to the current user.
2. Check the kernel driver, device tree, and kernel log.
3. For I²C, verify the 7-bit address, register width, wiring, and pull-up resistors.
4. For SPI, verify the mode, word width, frequency, chip select, command format, and logic-analyzer trace.

Complete programs are available in [`examples/linux/i2c.cpp`](../../examples/linux/i2c.cpp) and [`examples/linux/spi.cpp`](../../examples/linux/spi.cpp).
