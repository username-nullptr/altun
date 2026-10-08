# Documentation

The documentation follows the public source layout. Start with [Building and integration](getting-started.md), then open the module that owns the header you use.

## Core: `altun/core/`

| Public area | Header | Guide |
| --- | --- | --- |
| Version and logging | `global.h`, `log.h` | [Core](core.md#version-and-logging) |
| INI settings | `settings.h` | [Core](core.md#settings) |
| Software bus | `sbus.h`, `sbus/*` | [Core](core.md#software-bus) |
| Plugins and child processes | `plugin_manager.h` | [Core](core.md#plugins) |

Link these interfaces with `altun.core`.

## Linux: `altun/linux/`

| Source area | Public interface | Guide |
| --- | --- | --- |
| `udev/` | Enumeration and device events | [Device discovery](linux/device-discovery.md) |
| `storage/` | Block devices, filesystems, partitions, and mounts | [Storage](linux/storage.md) |
| `bus/` | I²C and SPI | [Bus interfaces](linux/buses.md) |
| `subsys/` | Backlights, LEDs, GPIO, and PWM | [Hardware control](linux/hardware-control.md) |
| `serial_port_binding.h` | Hot-pluggable serial ports | [Serial-port binding](linux/serial-port-binding.md) |

All Linux interfaces link with `altun.linux`. The module overview covers [dependencies, backend selection, and device checks](linux.md).

## Shared conventions

- [Execution and I/O model](io-model.md): completion tokens, executors, thread safety, cancellation, and buffer lifetime.
- [Examples](../examples/README.md): programs that exercise each public area.
- [Tests](../test/README.md): test targets, labels, sanitizers, and opt-in hardware tests.

Aggregate headers are available as `<altun/core.h>`, `<altun/linux.h>`, and `<altun.h>`. `<altun/linux.h>` does not include `<altun/linux/serial_port_binding.h>`; include that header explicitly when needed.
