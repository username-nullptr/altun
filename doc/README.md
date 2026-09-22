# libEMpp documentation

The documentation is organized around getting started, shared conventions, and module interfaces. It describes only behavior and requirements provided by this source tree.

## Getting started

| Task | Document |
| --- | --- |
| Prepare dependencies; build, test, install, or integrate with CMake | [Building and integration](getting-started.md) |
| Run minimal programs to verify the build environment or hardware access | [Examples](../examples/README.md) |
| Run functional, stress, or sanitizer tests | [Testing guide](../test/README.md) |

## Shared conventions

| Topic | Document |
| --- | --- |
| Completion tokens, executors, completion handlers, cancellation, and buffer lifetimes | [Execution and I/O model](io-model.md) |
| Linux dependencies, GPIO/PWM backends, permissions, and general troubleshooting | [Linux module](linux.md) |

Each module defines its own error model. Storage interfaces consistently return `libgs::sys_expected<T>` and use `libgs::optional<T>` for an absent value in a successful result. Other synchronous device interfaces use exceptions or `std::error_code` as documented. For asynchronous interfaces, follow the object, buffer, and handler conventions in [Execution and I/O model](io-model.md).

## Module interfaces

| Area | Guide | Recommended header | CMake target |
| --- | --- | --- | --- |
| Logging, INI configuration, and plugins | [Core module](core.md) | Use `libempp/core/*.h` as needed | `empp.core` |
| udev enumeration and events | [Device discovery](linux/device-discovery.md) | `<libempp/linux/udev/enumeration.h>`, `event.h` | `empp.linux` |
| Block devices, filesystems, partitions, and mounts | [Storage](linux/storage.md) | `<libempp/linux/storage.h>` | `empp.linux` |
| I²C and SPI | [Bus interfaces](linux/buses.md) | `<libempp/linux/bus/i2c.h>`, `spi.h` | `empp.linux` |
| Backlights, LEDs, GPIO, and PWM | [Hardware control](linux/hardware-control.md) | Use `libempp/linux/subsys/*.h` as needed | `empp.linux` |
| Hot-pluggable serial ports | [Serial-port binding](linux/serial-port-binding.md) | `<libempp/linux/serial_port_binding.h>` | `empp.linux` |

The `<libempp/core.h>` and `<libempp/linux.h>` aggregate headers include the public interfaces for their respective modules. Larger projects should include component headers as needed to reduce compile-time dependencies.

## Safety boundaries

- Device, filesystem, and udev-property queries are normally read-only, but they still require permission to access device nodes.
- Writes to GPIO, PWM, LEDs, backlights, and buses change hardware state immediately.
- Formatting and replacing a partition table destroy data; mounting and unmounting change the filesystem state visible to the process.
- Examples demonstrate APIs. They do not replace production-grade target validation, permission control, timeouts, recovery, or auditing.
