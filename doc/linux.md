# Linux module guide

[Documentation home](README.md) · [Building and integration](getting-started.md)

The Linux module wraps device discovery, storage, buses, hardware control, and serial-port binding. Its link target is `empp.linux`, and it can be built only on Linux. You can include the `<libempp/linux.h>` aggregate header; larger projects should include component headers as needed.

## Topic index

| Topic | Main interfaces | System resources |
| --- | --- | --- |
| [udev device discovery](linux/device-discovery.md) | Enumeration, property matching, and device events | `libudev`, udev database |
| [Storage](linux/storage.md) | Device identity and reads, filesystem inspection/formatting, partitions, and mounts | Block devices, `libudev`, `libblkid`, `sfdisk`, `mkfs.*` |
| [I²C and SPI buses](linux/buses.md) | Register reads/writes, half-duplex and full-duplex transfers | `/dev/i2c-*`, `/dev/spidev*` |
| [Hardware control](linux/hardware-control.md) | Backlights, LEDs, GPIO, and PWM | sysfs, GPIO/PWM character devices |
| [Serial-port binding](linux/serial-port-binding.md) | udev rules, hot-plug handling, and asynchronous I/O | `/dev/tty*`, `libudev` |

`empp.linux` publicly depends on `empp.core` and `libudev`, and internally uses `libblkid`. See [Building and integration](getting-started.md) for build dependencies, installation commands, and general CMake options.

## GPIO and PWM backends

GPIO and PWM select their backends at configuration time and do not switch backends at runtime:

| CMake option | `AUTO` | `ON` | `OFF` |
| --- | --- | --- | --- |
| `LIBEMPP_USE_GPIOD` | Use libgpiod when detected; otherwise fall back to sysfs | libgpiod must be found | Always use GPIO sysfs |
| `LIBEMPP_USE_PWM_CDEV` | Use the newer PWM waveform UAPI when detected; otherwise fall back to sysfs | A suitable `<linux/pwm.h>` is required | Always use PWM sysfs |

```sh
cmake -S . -B build \
  -DLIBEMPP_USE_GPIOD=AUTO \
  -DLIBEMPP_USE_PWM_CDEV=AUTO
```

Both libgpiod 1.x and 2.x are supported. The PWM character-device backend uses the kernel UAPI directly and requires no additional user-space library. Use `gpio::backend_name()` and `pwm::backend_name()` to record the backend selected for the current build. See [Hardware control](linux/hardware-control.md) for resource naming, exclusivity, and fallback differences.

## Preflight checks

- Use `udevadm info -q property <device>` to confirm that device properties match your rules.
- Confirm that the driver created the target node or sysfs directory, such as `/dev/i2c-*`, `/dev/spidev*`, `/dev/tty*`, `/dev/gpiochip*`, `/dev/pwmchip*`, `/sys/class/gpio/`, `/sys/class/pwm/`, `/sys/class/backlight/`, or `/sys/class/leds/`.
- Grant the service account access through groups or narrowly scoped udev rules instead of relying on root.
- Before changing storage layout, confirm that the device is neither mounted nor in use. Formatting, partitioning, mounting, and unmounting normally require root or `CAP_SYS_ADMIN`.
- Before writing to real hardware, verify voltage, address, register, mode, frequency, and timing requirements.

## Lifetimes and error handling

- Storage interfaces return `libgs::sys_expected<T>`. Device-control exception and `std::error_code` conventions are documented by topic. Hot-plug recovery must re-enumerate and verify device identity.
- Device objects own file descriptors or kernel resources. Do not race destruction or lifecycle operations with unfinished object access, and keep external buffers valid as documented. See [Execution and I/O model](io-model.md) for executor, completion-handler, and strand conventions.
- udev events indicate state changes; they are not a durable event log. Re-enumerate devices after a monitor error or prolonged blocking to recover the final state.
- Production logs should retain the device node, operation type, and `std::error_code`, rather than only the error text.

## Quick troubleshooting

| Symptom | Check first |
| --- | --- |
| No udev match | Actual `udevadm info -q property` values, capitalization, and wildcards |
| Block-device or filesystem query fails | Target type, read permission, hot-plug state, and `libblkid` |
| Partitioning, formatting, or mounting fails | Whether the target is in use, permissions, and availability of `sfdisk`/`mkfs.*` |
| I²C/SPI failure | Device node, permissions, driver, address/mode/frequency, and wiring |
| Backlight/LED failure | sysfs name, attribute permissions, brightness range, and available triggers |
| GPIO/PWM failure | Selected build backend, chip/line/channel, permissions, and resource ownership |
| Serial port repeatedly closes | Disconnects, port parameters, I/O errors, and rule-object lifetime |

See the [examples guide](../examples/README.md) for runnable programs, command-line parameters, and safety notes for real devices.
