# SBus performance benchmark

[Test guide](../README.md)

`empp.performance.sbus` compares CycloneDDS, D-Bus, and POSIX shared memory. Each backend runs in an isolated worker and uses a separate echo process for IPC round trips.

| Scope | Metric | Meaning |
| --- | --- | --- |
| `local` | `publish-throughput` | Time inside `publish()` |
| `local` | `end-to-end-throughput` | Rate until same-process callbacks complete |
| `ipc-round-trip` | `publish-throughput` | Request submission cost |
| `ipc-round-trip` | `end-to-end-throughput` | Complete request/echo rate |
| Both | `latency-p50/p95/p99` | Completion latency for 32-byte messages |

Throughput payloads are 32 B, 1 KiB, 64 KiB, and 1 MiB. The benchmark validates sequence, size, duplication, ordering, and timeout, then reports the median of three measurements. Prefer `ipc-round-trip` end-to-end throughput and latency for cross-backend comparisons; local publish cost reflects different submission semantics.

Build and run commands are in the [main test guide](../README.md#performance). CTest provides an isolated D-Bus session and shared-memory namespace and marks the benchmark `RUN_SERIAL`.

`LIBEMPP_PERFORMANCE_SCALE` multiplies the sample count. Keep compiler, Release configuration, host, CPU policy, and background load fixed when comparing results.
