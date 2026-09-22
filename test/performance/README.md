# SBus performance benchmark

[Testing guide](../README.md)

`empp.performance.sbus` compares CycloneDDS, D-Bus, and POSIX shared memory in the same executable with the same compiler, workloads, and statistics. It starts an isolated worker for each backend so that threads left by an earlier backend do not affect later measurements. Each worker then starts a separate echo process to measure real IPC.

## Measurement definitions

| Scope | Metric | Meaning |
| --- | --- | --- |
| `local` | `publish-throughput` | Time spent by the caller inside `publish()` |
| `local` | `end-to-end-throughput` | Throughput until all same-process subscription callbacks complete |
| `ipc-round-trip` | `publish-throughput` | Caller overhead for submitting a request to a separate echo process |
| `ipc-round-trip` | `end-to-end-throughput` | Complete RTT throughput for a request sent over the bus and returned by the echo process |
| Both | `latency-p50/p95/p99` | Latency percentiles while waiting for each 32 B message to complete |

Throughput covers 32 B, 1 KiB, 64 KiB, and 1 MiB payloads. Each sample uses a maximum depth of 32 and a window of at most 8 MiB of one-way in-flight payload. Every round waits for all callbacks to complete. This prevents asynchronous enqueue speed from being misreported as end-to-end throughput, stays within the Cyclone backend's internal queue, and prevents the bounded shared-memory ring from creating a backpressure loop in round-trip traffic. Every message carries a run ID and strictly increasing sequence number. Missing, duplicate, reordered, incorrectly sized, or timed-out messages fail the benchmark. Throughput is the median of three measurements.

The final `dbus/cyclone-cost`, `dbus/shm-cost`, and `cyclone/shm-cost` values divide time per message. A value greater than `1.0x` means the backend in the numerator is slower. `publish-throughput` reflects synchronous versus asynchronous submission semantics; use `end-to-end-throughput` and latency first when evaluating application-visible differences.

D-Bus and shared-memory same-process subscriptions invoke local callbacks directly while still submitting messages to their IPC transports. Same-process CycloneDDS messages still use the DDS data path. The `local` figures therefore describe each implementation but are not equivalent cross-transport comparisons; use `ipc-round-trip` for comparisons between backends. D-Bus `publish-throughput` includes one enqueue copy and applies backpressure when its bounded queue reaches 256 messages or a 32 MiB high-water mark, so it is not unbounded artificial asynchronous throughput.

## Building and running

```sh
cmake -S . -B build-perf \
  -DBUILD_TESTING=ON \
  -DLIBEMPP_BUILD_PERFORMANCE_TESTS=ON \
  -DLIBEMPP_BUILD_SBUS_CYCLONE=ON \
  -DLIBEMPP_BUILD_SBUS_DBUS=ON \
  -DLIBEMPP_BUILD_SBUS_SHM=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-perf --parallel
ctest --test-dir build-perf -R '^empp\.performance\.sbus$' -V
```

CTest creates an isolated session bus with `dbus-run-session` and derives the shared-memory namespace from the build directory and current parent-process PID. This prevents reader state from an interrupted run from contaminating later measurements. CMake skips the benchmark if that command is missing or the platform is not UNIX. The benchmark is registered with `RUN_SERIAL` to prevent other performance cases in the same build tree from competing for CPU.

`LIBEMPP_PERFORMANCE_SCALE` multiplies the sample count. When comparing machines or commits, keep the host, compiler, Release configuration, scale, CPU-frequency policy, and system load consistent. Performance tests do not run in sanitizer builds.
