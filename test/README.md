# Tests

[Project](../README.md) · [Build guide](../doc/getting-started.md)

CTest names are prefixed with `empp.` so libEMpp tests can be selected independently from bundled Riwo tests.

## Functional

```sh
cmake -S . -B build-test \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON
cmake --build build-test --parallel
ctest --test-dir build-test -R '^empp\.' -L functional --output-on-failure
```

| CTest name | Platform/condition | Scope |
| --- | --- | --- |
| `empp.core` | All | Version, settings, plugins, and processes |
| `empp.sbus` | `default`, `local`, `udp`, `dbus`, or `shm` selected | Publish/subscribe and available IPC behavior |
| `empp.virtual_devices` | Linux | Storage images, serial PTYs, I²C/SPI simulation, subsystems, and udev |
| `empp.block_device` | Linux | Block-device validation; optional read-only real-device checks |
| `empp.storage_destructive` | Linux | Explicitly opted-in destructive removable-media cycle |

`empp.block_device` accesses a real device only when `LIBEMPP_TEST_BLOCK_DEVICE` is set.

`empp.storage_destructive` does nothing unless both `LIBEMPP_TEST_STORAGE_DESTRUCTIVE=YES_I_UNDERSTAND` and `LIBEMPP_TEST_STORAGE_DEVICE` are set. When enabled, it repartitions, formats, mounts, writes to, and reformats the selected removable device. `LIBEMPP_TEST_STORAGE_SERIAL` can require an exact serial number. Use only disposable media.

See [Functional tests](functional/README.md) for coverage and contributor rules.

## Stress

```sh
cmake -S . -B build-stress \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_TESTING=ON \
  -DLIBEMPP_BUILD_STRESS_TESTS=ON
cmake --build build-stress --parallel
ctest --test-dir build-stress -R '^empp\.' -L stress --output-on-failure
```

Targets are `empp.stress.core` and, on Linux, `empp.stress.linux`. See [Stress tests](stress/README.md).

## Performance

The SBus comparison requires CycloneDDS, D-Bus, shared memory, UNIX, and `dbus-run-session`:

```sh
cmake -S . -B build-perf \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DLIBEMPP_BUILD_PERFORMANCE_TESTS=ON \
  -DLIBEMPP_BUILD_SBUS_CYCLONE=ON \
  -DLIBEMPP_BUILD_SBUS_DBUS=ON \
  -DLIBEMPP_BUILD_SBUS_SHM=ON
cmake --build build-perf --parallel
ctest --test-dir build-perf -R '^empp\.performance\.sbus$' -V
```

See [SBus benchmark](performance/README.md) for metric definitions.

## Select and reproduce

```sh
ctest --test-dir build-test -N -R '^empp\.'
ctest --test-dir build-test -R '^empp\.core$' --output-on-failure

build-test/output/bin/empp.test.core --list
build-test/output/bin/empp.test.core \
  --case 'plugin manager parses object and array configuration forms' \
  --repeat 10 --seed 42 --fail-fast
```

The runner also reads `LIBEMPP_TEST_CASE`, `LIBEMPP_TEST_REPEAT`, `LIBEMPP_TEST_SEED`, and `LIBEMPP_TEST_FAIL_FAST=1`.

## Installed package

`empp.cmake.install-consumer` installs the current build into an isolated
prefix, configures a standalone downstream project with `find_package`, and
builds and runs consumers of every installed component and compatibility
target. It also verifies that the package rejects an unavailable component.

```sh
ctest --test-dir build-test \
  -R '^empp\.cmake\.install-consumer$' --output-on-failure
```

## Test configuration

| Option | Default | Purpose |
| --- | :---: | --- |
| `BUILD_TESTING` | `OFF` | Enable CTest and build functional tests |
| `LIBEMPP_BUILD_CMAKE_TESTS` | value of `BUILD_TESTING` | Test the installed CMake package |
| `LIBEMPP_BUILD_STRESS_TESTS` | `OFF` | Build stress tests |
| `LIBEMPP_BUILD_PERFORMANCE_TESTS` | `OFF` | Build benchmarks |
| `LIBEMPP_ENABLE_TEST_SANITIZERS` | `OFF` | Enable ASan and UBSan |
| `LIBEMPP_ENABLE_TEST_TSAN` | `OFF` | Enable TSan |
| `LIBEMPP_FUNCTIONAL_REPEAT`, `LIBEMPP_FUNCTIONAL_SEED`, `LIBEMPP_FUNCTIONAL_TIMEOUT` | `1`, `1`, `60` | Functional execution controls |
| `LIBEMPP_STRESS_SCALE`, `LIBEMPP_STRESS_REPEAT`, `LIBEMPP_STRESS_SEED`, `LIBEMPP_STRESS_TIMEOUT` | `4`, `1`, `1`, `180` | Stress execution controls |
| `LIBEMPP_PERFORMANCE_SCALE`, `LIBEMPP_PERFORMANCE_TIMEOUT` | `1`, `180` | Benchmark controls |

ASan/UBSan and TSan are mutually exclusive, require GCC or Clang and
`BUILD_TESTING=ON`, and cannot be combined with `ENABLE_LTO`. Performance
tests cannot be combined with either sanitizer mode; invalid combinations are
rejected during configuration.
