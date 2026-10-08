# Functional tests

[Test guide](../README.md)

Functional tests exercise public behavior, validation, errors, asynchronous completion, ownership, and state transitions.

| CTest name | Main coverage |
| --- | --- |
| `altun.core` | Version, settings, plugin parsing/loading, and child processes |
| `altun.sbus` | Selected SBus backend and available interprocess path |
| `altun.virtual_devices` | Linux storage images, serial PTYs, simulated buses/subsystems, and udev |
| `altun.block_device` | Validation and optional read-only real block device |
| `altun.storage_destructive` | Explicitly opted-in removable-media mutation |

The virtual suite needs no physical board. Real-device environment variables and the destructive-test warning are documented in the [main test guide](../README.md#functional).

When adding tests:

- Exercise public headers and deterministic observable behavior.
- Cover completion styles separately only when behavior differs.
- Assert errors, cancellation, timeouts, ownership, and buffer lifetime.
- Use local fixtures or simulators unless the test is explicitly labeled `real-device`.
- Put sustained concurrency and heavy load in [stress tests](../stress/README.md).
