# Functional tests

[Testing guide](../README.md)

Functional tests define deterministic public API contracts and cover successful paths, input validation, error returns, asynchronous completion, resource ownership, and state transitions. Set `BUILD_TESTING=ON` to build them.

| CTest name | Main coverage |
| --- | --- |
| `empp.core` | Version interface, settings loading and persistence, plugin descriptor parsing, process loading, and shared-library loading |
| `empp.virtual_devices` | Filesystem images, serial PTYs, I²C/SPI simulation, GPIO/PWM/LED/backlight/block devices, and udev |

Linux tests use local files, pseudo-terminals, and in-process simulators and do not require a real board. Only when `LIBEMPP_TEST_BLOCK_DEVICE` is set do they access the selected real block device for additional read-only checks.

See the [testing guide](../README.md#functional) for build, filtering, and runner arguments.

## Writing cases

- Prefer calling interfaces through public headers.
- Cover synchronous, callback, future, and coroutine paths separately only when their behavior truly differs.
- Assert error reporting, cancellation, timeout, object lifetime, and buffer lifetime explicitly.
- Isolate external state with local fixtures or simulators so results are reproducible with the same seed.
- Put tests whose main purpose is sustained concurrency or heavy load in [Stress](../stress/README.md).
