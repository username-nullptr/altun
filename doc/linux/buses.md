# I²C and SPI

[Linux module](../linux.md) · [Execution model](../io-model.md)

The interfaces in `libempp::bus` support synchronous calls and LibGS completion tokens. Link `empp.linux`.

## I²C

`bus::i2c` uses Linux `i2c-dev`, a 7-bit device address, an 8-bit register address by default, and a 30 ms default timeout.

```cpp
#include <libempp/linux/bus/i2c.h>

std::error_code error;
libempp::bus::i2c device;
device.open({"/dev/i2c-1", 0x48}, error);

std::uint8_t value = 0;
device.read(0x00, libgs::mutable_buffer(&value, 1), error);

value = 0x80;
device.write(0x01, libgs::const_buffer(&value, 1), error);
```

Use `reg_bit16` for a 16-bit register address. A fixed-size read can return an array:

```cpp
auto value = device.read<std::array<std::uint8_t, 4>>(0x00, error);
auto wide = device.read<std::array<std::uint8_t, 2>,
    libempp::bus::reg_bit16>(0x0120, error);
```

## SPI

`bus::spi` uses Linux `spidev`. Defaults are mode 0, 500 kHz, and 8 bits per word.

```cpp
#include <libempp/linux/bus/spi.h>

std::error_code error;
libempp::bus::spi device;
device.open({"/dev/spidev0.0", 1'000'000}, error);

std::array<std::uint8_t, 4> tx {0x9f, 0x00, 0x00, 0x00};
std::array<std::uint8_t, 4> rx {};
device.transfer(
    libgs::const_buffer(tx.data(), tx.size()),
    libgs::mutable_buffer(rx.data(), rx.size()),
    error
);
```

`transfer()` requires equal transmit and receive sizes. `read()` and `write()` provide half-duplex operations. `spi::node` also configures mode, bits per word, delay, and chip-select behavior.

## Asynchronous operations

- Caller-provided asynchronous buffers remain valid until completion.
- Detached writes copy outgoing data.
- Array-returning reads own their result buffer and cannot use a detached token.
- Do not overlap operations or lifecycle changes on the same device object.

Before opening a bus, verify the device node, permissions, driver, address/mode, frequency, word size, wiring, and peripheral protocol.

See [`examples/linux/i2c.cpp`](../../examples/linux/i2c.cpp) and [`examples/linux/spi.cpp`](../../examples/linux/spi.cpp).
