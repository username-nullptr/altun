# libEMpp examples

[Project home](../README.md) · [Documentation index](../doc/README.md)

The examples verify the build environment and individual APIs. Core examples can run directly on a development machine. Linux examples access the specified device nodes or sysfs attributes.

## Building

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DLIBEMPP_BUILD_EXAMPLES=ON
cmake --build build --parallel
```

Executables are written to `build/output/examples/core/` and `build/output/examples/linux/`.

## Core

| Program | Invocation | Behavior |
| --- | --- | --- |
| `log` | `log` | Print default, warning, and named log messages |
| `settings` | `settings [config.ini]` | Load and write an INI file; uses `libempp-example.ini` in the working directory when omitted |
| `plugin_manager` | `plugin_manager [plugin-file]` | Load a shared library and call `libempp_example_square` |

Examples:

```sh
./build/output/examples/core/log
./build/output/examples/core/settings /tmp/libempp-example.ini
./build/output/examples/core/plugin_manager
```

`settings` modifies the specified file, so do not pass a configuration that must remain unchanged. By default, `plugin_manager` loads the `plugin.so` built alongside the host in the build directory.

See the [Core module](../doc/core.md) for API details.

## Linux

| Program | Invocation | Access type |
| --- | --- | --- |
| `udev` | `udev [model-pattern]` | Read-only enumeration of tty devices |
| `udev_event` | `udev_event [disk\|partition]` | Read-only monitoring of block-device events |
| `block` | `block <device>` | Read-only inspection and reading of the first logical block |
| `storage` | `storage <device-or-image>` | Read-only inspection of filesystems, partition tables, and mount state |
| `backlight` | `backlight <name> <brightness>` | Write backlight brightness and enable the device |
| `led` | `led <name> <brightness>` | Write brightness to an LED class device |
| `gpio` | `gpio <chip> <line> <in\|out> ...` | Read, write, or wait for a GPIO edge |
| `i2c` | `i2c <device> <address> <register> [value]` | Read a register, or write it when `value` is supplied |
| `spi` | `spi <device> <speed-hz> <byte> [byte ...]` | Perform one full-duplex transfer |
| `pwm` | `pwm <chip> <channel> <period-ns> <duty-ns>` | Enable PWM for one second, then disable it |
| `serial_port_binding` | `serial_port_binding <model-pattern> [baud-rate]` | Monitor serial ports continuously and reply `ACK` to received data |

Common invocations:

```sh
./build/output/examples/linux/udev 'USB_Serial*'
./build/output/examples/linux/udev_event disk
./build/output/examples/linux/block /dev/disk/by-id/example
./build/output/examples/linux/storage /dev/sdb

./build/output/examples/linux/i2c /dev/i2c-1 0x48 0x00
./build/output/examples/linux/spi /dev/spidev0.0 1000000 0x9f 0x00 0x00 0x00
./build/output/examples/linux/gpio /dev/gpiochip0 22 in both 5000
./build/output/examples/linux/pwm pwmchip0 0 20000000 1500000
./build/output/examples/linux/serial_port_binding 'USB_Serial*' 115200
```

See the corresponding guide for numeric limits and API behavior:

- [udev device discovery](../doc/linux/device-discovery.md)
- [Storage](../doc/linux/storage.md)
- [I²C and SPI](../doc/linux/buses.md)
- [Backlights, LEDs, GPIO, and PWM](../doc/linux/hardware-control.md)
- [Serial-port binding](../doc/linux/serial-port-binding.md)

## Preflight checks

```sh
udevadm info -q property /dev/ttyUSB0
ls -l /dev/ttyUSB0 /dev/i2c-1 /dev/spidev0.0
ls -l /dev/gpiochip* /dev/pwmchip*
ls -l /sys/class/pwm/ /sys/class/backlight/ /sys/class/leds/ /sys/class/gpio/
```

- If a device node is missing, check the driver, device tree, and kernel log. If it exists but cannot be opened, check group membership and udev permission rules.
- Backlight and LED examples leave the written state in place. When operating a backlight remotely, avoid a value that may turn the display off.
- GPIO, I²C, SPI, and PWM parameters must match the peripheral circuit and device data sheet. Do not substitute broad device rules for target verification.
- The block-device and filesystem examples are read-only. They do not invoke the partitioning, formatting, or mounting APIs.
