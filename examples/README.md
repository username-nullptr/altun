# Examples

[Project](../README.md) · [Documentation](../doc/README.md)

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DALTUN_BUILD_EXAMPLES=ON
cmake --build build --parallel
```

Programs are written to `build/output/examples/<module>/`.

## Core

| Program | Usage | Effect |
| --- | --- | --- |
| `log` | `log` | Emit default and named log messages |
| `settings` | `settings [config.ini]` | Load, modify, and save an INI file |
| `plugin_manager` | `plugin_manager [plugin-file]` | Load the example shared library and call its exported function |

```sh
./build/output/examples/core/log
./build/output/examples/core/settings /tmp/altun-example.ini
./build/output/examples/core/plugin_manager
```

`settings` writes the selected file. Without an argument it uses `altun-example.ini` in the working directory.

## Linux

| Program | Usage | Access |
| --- | --- | --- |
| `udev` | `udev [model-pattern]` | Enumerate tty devices |
| `udev_event` | `udev_event [disk\|partition]` | Monitor block-device events |
| `block` | `block <device>` | Read block geometry and the first logical block |
| `storage` | `storage <device-or-image>` | Inspect filesystems, partitions, and mounts |
| `i2c` | `i2c <device> <address> <register> [value]` | Read or write one register |
| `spi` | `spi <device> <speed-hz> <byte> [byte ...]` | Run one full-duplex transfer |
| `backlight` | `backlight <name> <brightness>` | Set brightness and enable the backlight |
| `led` | `led <name> <brightness>` | Set LED brightness |
| `gpio` | `gpio <chip> <line> <in\|out> ...` | Read/write a line or wait for an edge |
| `pwm` | `pwm <chip> <channel> <period-ns> <duty-ns>` | Enable output for one second, then disable it |
| `serial_port_binding` | `serial_port_binding <model-pattern> [baud-rate]` | Monitor matching ports and reply to received data |

The `block` and `storage` examples are read-only. I²C, SPI, GPIO, PWM, LED, backlight, and serial examples access real hardware; confirm the target node, permissions, electrical parameters, and accepted ranges before running them.

API details are in the [Linux module guide](../doc/linux.md).
