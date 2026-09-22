# Stress tests

[Testing guide](../README.md)

Stress tests verify correctness under concurrency, bulk asynchronous operations, and repeated lifecycles; they do not produce performance benchmarks. CTest marks every stress executable `RUN_SERIAL` to prevent high-load processes from interfering with each other.

| CTest name | Main coverage |
| --- | --- |
| `empp.stress.core` | Large numbers of plugin descriptors, multithreaded shared-library interfaces, and version queries |
| `empp.stress.linux` | Multiple virtual devices and bulk asynchronous I²C and SPI reads across multiple threads |

See the [testing guide](../README.md#stress) for build, filtering, workload scale, repeat count, and seed configuration.

## Writing cases

- Every run must accept the seed supplied by the runner and include reproduction parameters in failure messages.
- `LIBEMPP_STRESS_SCALE` controls work within one fixture; `LIBEMPP_STRESS_REPEAT` controls fixture reconstruction count.
- Use virtual devices or in-process simulators so hardware timing variability is not mistaken for library behavior.
- Clean up threads, executors, unfinished operations, and temporary files deterministically.
- Put behavior that can be reproduced with a single deterministic assertion in [Functional](../functional/README.md).
