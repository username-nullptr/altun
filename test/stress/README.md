# Stress tests

[Test guide](../README.md)

Stress tests check correctness under concurrency, bulk asynchronous work, and repeated object lifecycles. They are not benchmarks and run serially under CTest.

| CTest name | Coverage |
| --- | --- |
| `empp.stress.core` | Plugin descriptors, concurrent library interfaces, and version queries |
| `empp.stress.linux` | Multiple virtual devices and concurrent I²C/SPI operations |

`LIBEMPP_STRESS_SCALE` controls work within a fixture; `LIBEMPP_STRESS_REPEAT` controls fixture reconstruction; `LIBEMPP_STRESS_SEED` reproduces scheduling.

New stress cases must report reproduction parameters, clean up threads and unfinished operations deterministically, and avoid physical-hardware timing dependencies.
