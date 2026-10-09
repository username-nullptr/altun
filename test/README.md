# Tests

[Project](../README.md) · [Build guide](../doc/getting-started.md)

CTest names are prefixed with `altun.` so altun tests can be selected independently from bundled Riwo tests.

## Functional

```sh
cmake -S . -B build-test \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON
cmake --build build-test --parallel
ctest --test-dir build-test -R '^altun\.' -L functional --output-on-failure
```

| CTest name | Platform/condition | Scope |
| --- | --- | --- |
| `altun.core` | All | Version, settings, plugins, and processes |
| `altun.sbus` | `default`, `local`, `udp`, `dbus`, or `shm` selected | Publish/subscribe and available IPC behavior |
| `altun.virtual_devices` | Linux | Storage images, serial PTYs, I²C/SPI simulation, subsystems, and udev |
| `altun.block_device` | Linux | Block-device validation; optional read-only real-device checks |
| `altun.storage_destructive` | Linux | Explicitly opted-in destructive removable-media cycle |

`altun.block_device` accesses a real device only when `ALTUN_TEST_BLOCK_DEVICE` is set.

`altun.storage_destructive` does nothing unless both `ALTUN_TEST_STORAGE_DESTRUCTIVE=YES_I_UNDERSTAND` and `ALTUN_TEST_STORAGE_DEVICE` are set. When enabled, it repartitions, formats, mounts, writes to, and reformats the selected removable device. `ALTUN_TEST_STORAGE_SERIAL` can require an exact serial number. Use only disposable media.

See [Functional tests](functional/README.md) for coverage and contributor rules.

## Stress

```sh
cmake -S . -B build-stress \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_TESTING=ON \
  -DALTUN_BUILD_STRESS_TESTS=ON
cmake --build build-stress --parallel
ctest --test-dir build-stress -R '^altun\.' -L stress --output-on-failure
```

Targets are `altun.stress.core` and, on Linux, `altun.stress.linux`. See [Stress tests](stress/README.md).

## Performance

The SBus benchmark is UNIX-only and uses whichever optional transports are
available. For example, to measure shared memory alone:

```sh
cmake -S . -B build-perf \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DALTUN_BUILD_PERFORMANCE_TESTS=ON \
  -DALTUN_BUILD_SBUS_SHM=ON
cmake --build build-perf --parallel
ctest --test-dir build-perf -R '^altun\.performance\.sbus$' -V
```

Add the CycloneDDS or D-Bus options when those dependencies are available. The
benchmark runs each enabled transport and prints pairwise comparisons when at
least two are present. If no usable optional transport is enabled, the
benchmark is skipped. D-Bus is skipped when `dbus-run-session` is unavailable.

See [SBus benchmark](performance/README.md) for metric definitions.

## Select and reproduce

```sh
ctest --test-dir build-test -N -R '^altun\.'
ctest --test-dir build-test -R '^altun\.core$' --output-on-failure

build-test/output/bin/altun.test.core --list
build-test/output/bin/altun.test.core \
  --case 'plugin manager parses object and array configuration forms' \
  --repeat 10 --seed 42 --fail-fast
```

The runner also reads `ALTUN_TEST_CASE`, `ALTUN_TEST_REPEAT`, `ALTUN_TEST_SEED`, and `ALTUN_TEST_FAIL_FAST=1`.

## Installed package

`altun.cmake.install-consumer` installs the current build into an isolated
prefix, configures a standalone downstream project with `find_package`, and
builds and runs consumers of every installed component and compatibility
target. It also verifies that the package rejects an unavailable component.

```sh
ctest --test-dir build-test \
  -R '^altun\.cmake\.install-consumer$' --output-on-failure
```

## Test configuration

| Option | Default | Purpose |
| --- | :---: | --- |
| `BUILD_TESTING` | `OFF` | Enable CTest and build functional tests |
| `ALTUN_BUILD_CMAKE_TESTS` | value of `BUILD_TESTING` | Test the installed CMake package |
| `ALTUN_BUILD_STRESS_TESTS` | `OFF` | Build stress tests |
| `ALTUN_BUILD_PERFORMANCE_TESTS` | `OFF` | Build benchmarks |
| `ALTUN_ENABLE_TEST_SANITIZERS` | `OFF` | Enable ASan and UBSan |
| `ALTUN_ENABLE_TEST_TSAN` | `OFF` | Enable TSan |
| `ALTUN_FUNCTIONAL_REPEAT`, `ALTUN_FUNCTIONAL_SEED`, `ALTUN_FUNCTIONAL_TIMEOUT` | `3`, `1`, `120` | Functional execution controls |
| `ALTUN_STRESS_SCALE`, `ALTUN_STRESS_REPEAT`, `ALTUN_STRESS_SEED`, `ALTUN_STRESS_TIMEOUT` | `5`, `3`, `1`, `180` | Stress execution controls |
| `ALTUN_PERFORMANCE_SCALE`, `ALTUN_PERFORMANCE_TIMEOUT` | `1`, `180` | Benchmark controls |

ASan/UBSan and TSan are mutually exclusive, require GCC or Clang and
`BUILD_TESTING=ON`, and cannot be combined with `ALTUN_ENABLE_LTO`.
Performance tests cannot be combined with either sanitizer mode; invalid
combinations are rejected during configuration.
