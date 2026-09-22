# Linux module

[Documentation](README.md) · [Build options](getting-started.md)

`empp.linux` is built only on Linux and publicly depends on `empp.core` and `libudev`; storage also uses `libblkid` internally.

| Source area | Guide | Resource |
| --- | --- | --- |
| `linux/udev/` | [Device discovery](linux/device-discovery.md) | udev database and monitor |
| `linux/storage/` | [Storage](linux/storage.md) | block devices, filesystems, partitions, mounts |
| `linux/bus/` | [I²C and SPI](linux/buses.md) | `/dev/i2c-*`, `/dev/spidev*` |
| `linux/subsys/` | [Hardware control](linux/hardware-control.md) | sysfs, GPIO/PWM character devices |
| `linux/serial_port_binding.h` | [Serial-port binding](linux/serial-port-binding.md) | `/dev/tty*` and udev |

`<libempp/linux.h>` aggregates storage, subsystem, udev, and bus headers. Include `<libempp/linux/serial_port_binding.h>` separately.

## Backend selection

GPIO and PWM select one backend at CMake configuration time:

| Option | `AUTO` | `ON` | `OFF` |
| --- | --- | --- | --- |
| `LIBEMPP_USE_GPIOD` | Use detected libgpiod, otherwise sysfs | Require libgpiod | Use sysfs |
| `LIBEMPP_USE_PWM_CDEV` | Use detected PWM waveform UAPI, otherwise sysfs | Require the UAPI | Use sysfs |

Both libgpiod 1.x and 2.x are supported. Inspect the selected backend with `gpio::backend_name()` or `pwm::backend_name()`.

## Device checklist

1. Confirm the kernel driver and expected `/dev` node or `/sys/class` entry exist.
2. Inspect udev properties with `udevadm info -q property <device>`.
3. Grant the service account access through device groups or narrow udev rules.
4. Verify addresses, line/channel numbers, voltage, mode, timing, and resource ownership before writing hardware.
5. For storage mutation, use a stable device identity and confirm the disk and its partitions are unmounted.

Storage functions return `libgs::sys_expected<T>`. Other device APIs provide throwing and/or `std::error_code` overloads. Follow the [execution and I/O model](io-model.md) for asynchronous lifetimes and serialization.
