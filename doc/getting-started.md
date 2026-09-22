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
| `LIBEMPP_BUILD_SUBMODEL_LIBGS` | `ON` | Build the bundled LibGS submodule |
| `LIBSEPP_LIBGS_INSTALL_PREFIX` | empty | Absolute install prefix of an external LibGS package; requires `LIBEMPP_BUILD_SUBMODEL_LIBGS=OFF` |
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

Use the source tree directly with:

```cmake
add_subdirectory(third_party/libempp)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE empp.core)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(my_app PRIVATE empp.linux)
endif()
```

For an installed package, use the exported namespaced targets:

```cmake
find_package(libEMpp 0.6 CONFIG REQUIRED COMPONENTS core)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE libEMpp::core)
```

On Linux, request and link the `linux` component when needed:

```cmake
find_package(libEMpp 0.6 CONFIG REQUIRED COMPONENTS core linux)
target_link_libraries(my_app PRIVATE libEMpp::linux)
```

The package restores its LibGS and platform dependencies automatically and
also defines `empp.core` and `empp.linux` as compatibility targets.

To build against an already installed LibGS outside the normal CMake search
prefixes, configure libEMpp with its absolute install prefix:

```sh
cmake -S . -B build \
  -DLIBEMPP_BUILD_SUBMODEL_LIBGS=OFF \
  -DLIBSEPP_LIBGS_INSTALL_PREFIX=/opt/libgs
```

That prefix is recorded in the installed libEMpp package. A downstream build
can override it before `find_package(libEMpp)` when LibGS has moved:

```cmake
set(LIBSEPP_LIBGS_INSTALL_PREFIX "/another/libgs/prefix")
find_package(libEMpp CONFIG REQUIRED)
```

## Install

```sh
cmake --install build --prefix /opt/libempp
```

Shared or static libraries, headers, enabled examples, and CMake package files
are installed below `lib/`, `include/`, `examples/`, and
`lib/cmake/libEMpp/`. When bundled LibGS is enabled, its libraries and package
files are installed into the same prefix.
