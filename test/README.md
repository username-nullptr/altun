# libEMpp testing guide

[Project home](../README.md) · [Building and integration](../doc/getting-started.md)

Tests are divided into functional, stress, and performance categories. Functional and stress tests use the same runner. Performance tests emit validated measurements. All categories can be selected with CTest labels.

| Category | CTest name | Executable | Description |
| --- | --- | --- | --- |
| Functional | `empp.core` | `output/bin/empp.test.core` | [Core functional tests](functional/README.md) |
| Functional | `empp.virtual_devices` | `output/bin/empp.test.virtual_devices` | [Linux virtual-device tests](functional/README.md) |
| Stress | `empp.stress.core` | `output/bin/empp.test.stress.core` | [Core stress tests](stress/README.md) |
| Stress | `empp.stress.linux` | `output/bin/empp.test.stress.linux` | [Linux stress tests](stress/README.md) |
| Performance | `empp.performance.sbus` | `output/bin/empp.test.performance.sbus` | [SBus performance comparison](performance/README.md) |

`empp.virtual_devices` and `empp.stress.linux` are registered only on platforms that build `empp.linux`. The repository also builds LibGS tests, so the commands below use `-R '^empp\.'` to select only libEMpp entries.

## Functional

Functional tests verify deterministic public API behavior, errors, ownership, and state changes. They are built when `BUILD_TESTING=ON`:

```sh
cmake -S . -B build-test \
  -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-test --parallel
ctest --test-dir build-test -R '^empp\.' -L functional --output-on-failure
```

Linux tests use file images, pseudo-terminals, and in-process simulators and do not require a real board. Set `LIBEMPP_TEST_BLOCK_DEVICE=/dev/...` to enable additional read-only integration checks on a selected block device.

## Stress

Stress tests verify concurrency, bulk asynchronous operations, and repeated lifecycles. They must be enabled explicitly:

```sh
cmake -S . -B build-stress \
  -DBUILD_TESTING=ON \
  -DLIBEMPP_BUILD_STRESS_TESTS=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-stress --parallel
ctest --test-dir build-stress -R '^empp\.' -L stress --output-on-failure
```

## Performance

Performance benchmarks must be enabled explicitly and should use a Release build. The SBus comparison also requires the CycloneDDS, D-Bus, and shared-memory backends:

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

See the [performance guide](performance/README.md) for payloads, isolation, and interpretation of results.

## Filtering and reproduction

List or run an individual CTest entry:

```sh
ctest --test-dir build-test -N -R '^empp\.'
ctest --test-dir build-test -R '^empp\.core$' --output-on-failure
```

When running a test executable directly, select cases, repeat counts, and a seed:

```sh
build-test/output/bin/empp.test.core --list
build-test/output/bin/empp.test.core \
  --case 'plugin manager parses object and array configuration forms' \
  --repeat 10 --seed 42 --fail-fast
```

| Environment variable | Command-line argument | Purpose |
| --- | --- | --- |
| `LIBEMPP_TEST_CASE` | `--case <name>` | Select an exact case name; may be repeated |
| `LIBEMPP_TEST_REPEAT` | `--repeat <count>` | Repeat each selected case |
| `LIBEMPP_TEST_SEED` | `--seed <value>` | Set the reproducible base seed |
| `LIBEMPP_TEST_FAIL_FAST=1` | `--fail-fast` | Stop after the first failure |
| — | `--list` | List cases |
| — | `--help` | Show runner arguments |

## CMake testing options

| Option | Default | Purpose |
| --- | :---: | --- |
| `BUILD_TESTING` | `OFF` | Build and register functional tests |
| `LIBEMPP_BUILD_STRESS_TESTS` | `OFF` | Build and register stress tests |
| `LIBEMPP_FUNCTIONAL_REPEAT` | `1` | Runs of each functional test case |
| `LIBEMPP_FUNCTIONAL_SEED` | `1` | Base functional-test seed |
| `LIBEMPP_FUNCTIONAL_TIMEOUT` | `60` | Timeout in seconds for each functional executable |
| `LIBEMPP_STRESS_SCALE` | `4` | Work multiplier for one stress fixture |
| `LIBEMPP_STRESS_REPEAT` | `1` | Fixture reconstructions for each stress case |
| `LIBEMPP_STRESS_SEED` | `1` | Base stress-test seed |
| `LIBEMPP_STRESS_TIMEOUT` | `180` | Timeout in seconds for each stress executable |
| `LIBEMPP_BUILD_PERFORMANCE_TESTS` | `OFF` | Build performance benchmarks |
| `LIBEMPP_PERFORMANCE_SCALE` | `1` | Work multiplier for performance measurements |
| `LIBEMPP_PERFORMANCE_TIMEOUT` | `180` | Timeout in seconds for each performance benchmark |
| `LIBEMPP_ENABLE_TEST_SANITIZERS` | `OFF` | Enable ASan and UBSan |
| `LIBEMPP_ENABLE_TEST_TSAN` | `OFF` | Enable TSan |

ASan/UBSan and TSan cannot be enabled together. Sanitizers require GCC or Clang and `BUILD_TESTING=ON`, and they cannot be combined with `ENABLE_LTO`.

## Sanitizers

ASan/UBSan:

```sh
cmake -S . -B build-asan \
  -DBUILD_TESTING=ON \
  -DLIBEMPP_BUILD_STRESS_TESTS=ON \
  -DLIBEMPP_ENABLE_TEST_SANITIZERS=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-asan --parallel
ctest --test-dir build-asan -R '^empp\.' -L sanitizer --output-on-failure
```

Enable TSan with `LIBEMPP_ENABLE_TEST_TSAN=ON`. On supported Linux hosts, CTest uses `setarch -R` to disable ASLR only for TSan test processes.
