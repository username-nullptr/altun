# Building and integration

[Documentation](README.md) · [Examples](../examples/README.md) · [Tests](../test/README.md)

## Requirements

| Item | Requirement |
| --- | --- |
| CMake | 3.16+ |
| Language | C++20 |
| Compiler | GCC 13+, Clang 17+, or supported MSVC 2022 |
| LibGS | Bundled Git submodule |
| Linux libraries | `pkg-config`, `libudev`, and `libblkid` development files |
| Optional Linux backend | libgpiod 1.x/2.x; otherwise GPIO uses sysfs |
| Runtime tools | `sfdisk` for partition changes; matching `mkfs.*` tools for formatting |

Debian/Ubuntu base packages:

```sh
sudo apt update
sudo apt install build-essential cmake pkg-config libudev-dev libblkid-dev \
  libgpiod-dev util-linux e2fsprogs
```

Initialize a source tree that was cloned without submodules:

```sh
git submodule update --init --recursive
```

## Build and output

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

| Artifact | Path |
| --- | --- |
| Shared libraries and test executables | `build/output/bin/` |
| Static libraries | `build/output/lib/` |
| Examples | `build/output/examples/<module>/` |

Build examples or functional tests with:

```sh
cmake -S . -B build -DLIBEMPP_BUILD_EXAMPLES=ON
cmake --build build --parallel

cmake -S . -B build-test -DBUILD_TESTING=ON
cmake --build build-test --parallel
ctest --test-dir build-test -R '^empp\.' --output-on-failure
```

## Main CMake options

| Option | Default | Effect |
| --- | :---: | --- |
| `LIBEMPP_BUILD_EXAMPLES` | `OFF` | Build `examples/` |
| `LIBEMPP_BUILD_STATIC` | `OFF` | Build static instead of shared libraries |
| `LIBEMPP_ADD_LIBRARY_VERSION` | `ON` | Add version/SOVERSION to shared libraries |
| `LIBEMPP_USE_GPIOD` | `AUTO` | `AUTO`, `ON`, or `OFF` for the GPIO backend |
| `LIBEMPP_USE_PWM_CDEV` | `AUTO` | `AUTO`, `ON`, or `OFF` for the PWM character-device backend |
| `LIBEMPP_HEAVY_COMPILE_JOBS` | GCC `6`, Clang `8`, other `0` | Limit concurrent memory-heavy compilations; `0` disables the limit |
| `LIBEMPP_LOW_MEMORY_DEBUG_INFO` | `OFF` | Use `-g1` for GCC Debug builds |

`AUTO` selects the character-device backend when its dependency or kernel header is available, otherwise sysfs. `ON` makes the character-device backend mandatory. The selected backend is available at runtime through `gpio::backend_name()` and `pwm::backend_name()`.

### SBus

| Option | Default | Effect |
| --- | :---: | --- |
| `LIBEMPP_SBUS_INTERFACE` | `default` | Select `default`, `local`, `udp`, `dbus`, `cyclone`, or `shm` |
| `LIBEMPP_BUILD_SBUS_DBUS` | `OFF` | Build the D-Bus transport |
| `LIBEMPP_BUILD_SBUS_CYCLONE` | `OFF` | Build the CycloneDDS transport |
| `LIBEMPP_BUILD_SBUS_SHM` | `OFF` | Build the POSIX shared-memory transport |

Selecting `dbus`, `cyclone`, or `shm` also requires its corresponding build option. Selecting `udp` requires `LIBGS_BUILD_UTILITIES_SBUS_UDP=ON`.

Shared-memory participants use `/libempp-sbus-<uid>` by default. Set the same `LIBEMPP_SBUS_SHM_NAME=/name` in all participating processes to use another namespace.

Testing options are listed in the [testing guide](../test/README.md).

## Integrate with CMake

The repository exports source-tree targets, not a `find_package` config:

```cmake
add_subdirectory(third_party/libempp)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE empp.core)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(my_app PRIVATE empp.linux)
endif()
```

## Install

```sh
cmake --install build --prefix /opt/libempp
```

Shared libraries, headers, and enabled examples are installed below `lib/`, `include/`, and `examples/`. Static library targets have no install rule; consume a static build with `add_subdirectory`.
