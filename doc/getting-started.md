# Building and integration

[Documentation](README.md) · [Examples](../examples/README.md) · [Tests](../test/README.md)

## Requirements

| Item | Requirement |
| --- | --- |
| CMake | 3.16+ |
| Language | C++20 |
| Compiler | GCC 13+, Clang 17+, or supported MSVC 2022 |
| Riwo | Embedded by default; an external CMake package is also supported |
| nlohmann/json | Embedded by default; an external CMake package is also supported |
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
cmake -S . -B build -DALTUN_BUILD_EXAMPLES=ON
cmake --build build --parallel

cmake -S . -B build-test -DBUILD_TESTING=ON
cmake --build build-test --parallel
ctest --test-dir build-test -R '^altun\.' --output-on-failure
```

## Main CMake options

| Option | Default | Effect |
| --- | :---: | --- |
| `ALTUN_BUILD_EXAMPLES` | `OFF` | Build `examples/` |
| `ALTUN_BUILD_STATIC` | `OFF` | Build static instead of shared libraries |
| `ALTUN_ADD_LIBRARY_VERSION` | `ON` | Add version/SOVERSION to shared libraries |
| `ALTUN_USE_LIBCXX` | `OFF` | Use libc++ with Clang |
| `ALTUN_USE_LLD` | `OFF` | Use lld with Clang |
| `ALTUN_ENABLE_LTO` | `OFF` | Enable LTO with GCC |
| `ALTUN_USE_BUNDLED_RIWO` | `ON` | Use the bundled Riwo dependency |
| `ALTUN_RIWO_INSTALL_PREFIX` | empty | Absolute install prefix of an external Riwo package; requires `ALTUN_USE_BUNDLED_RIWO=OFF` |
| `ALTUN_USE_EMBEDDED_NLOHMANN` | `ON` | Use the embedded nlohmann/json dependency |
| `ALTUN_NLOHMANN_INSTALL_PREFIX` | empty | Absolute install prefix of an external nlohmann/json package; requires `ALTUN_USE_EMBEDDED_NLOHMANN=OFF` |
| `ALTUN_USE_GPIOD` | `AUTO` | `AUTO`, `ON`, or `OFF` for the GPIO backend |
| `ALTUN_USE_PWM_CDEV` | `AUTO` | `AUTO`, `ON`, or `OFF` for the PWM character-device backend |
| `ALTUN_HEAVY_COMPILE_JOBS` | GCC `6`, Clang `8`, other `0` | Limit concurrent memory-heavy compilations; `0` disables the limit |
| `ALTUN_LOW_MEMORY_DEBUG_INFO` | `OFF` | Use `-g1` for GCC Debug builds |

`AUTO` selects the character-device backend when its dependency or kernel header is available, otherwise sysfs. `ON` makes the character-device backend mandatory. The selected backend is available at runtime through `gpio::backend_name()` and `pwm::backend_name()`.

### SBus

| Option | Default | Effect |
| --- | :---: | --- |
| `ALTUN_SBUS_INTERFACE` | `default` | Select `default`, `local`, `udp`, `dbus`, `cyclone`, or `shm` |
| `ALTUN_BUILD_SBUS_DBUS` | `OFF` | Build the D-Bus transport |
| `ALTUN_BUILD_SBUS_CYCLONE` | `OFF` | Build the CycloneDDS transport |
| `ALTUN_BUILD_SBUS_SHM` | `OFF` | Build the POSIX shared-memory transport |

Selecting `dbus`, `cyclone`, or `shm` also requires its corresponding build option. Selecting `udp` requires `RIWO_BUILD_UTILITIES_SBUS_UDP=ON`.

Shared-memory participants use `/altun-sbus-<uid>` by default. Set the same `ALTUN_SBUS_SHM_NAME=/name` in all participating processes to use another namespace.

Testing options are listed in the [testing guide](../test/README.md).

## Integrate with CMake

Use the source tree directly with:

```cmake
add_subdirectory(third_party/altun)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE altun.core)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(my_app PRIVATE altun.linux)
endif()
```

For an installed package, use the exported namespaced targets:

```cmake
find_package(altun 0.6 CONFIG REQUIRED COMPONENTS core)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE altun::core)
```

On Linux, request and link the `linux` component when needed:

```cmake
find_package(altun 0.6 CONFIG REQUIRED COMPONENTS core linux)
target_link_libraries(my_app PRIVATE altun::linux)
```

The package restores its Riwo and platform dependencies automatically and
also defines `altun.core` and `altun.linux` as compatibility targets.

To build against an already installed Riwo outside the normal CMake search
prefixes, configure altun with its absolute install prefix:

```sh
cmake -S . -B build \
  -DALTUN_USE_BUNDLED_RIWO=OFF \
  -DALTUN_RIWO_INSTALL_PREFIX=/opt/riwo
```

That prefix is recorded in the installed altun package. A downstream build
can override it before `find_package(altun)` when Riwo has moved:

```cmake
set(ALTUN_RIWO_INSTALL_PREFIX "/another/riwo/prefix")
find_package(altun CONFIG REQUIRED)
```

The same pattern selects an external nlohmann/json package:

```sh
cmake -S . -B build \
  -DALTUN_USE_EMBEDDED_NLOHMANN=OFF \
  -DALTUN_NLOHMANN_INSTALL_PREFIX=/opt/nlohmann-json
```

The nlohmann/json prefix is also recorded in the installed package and can be
overridden by setting `ALTUN_NLOHMANN_INSTALL_PREFIX` before
`find_package(altun)`.

## Install

```sh
cmake --install build --prefix /opt/altun
```

Shared or static libraries, headers, enabled examples, and CMake package files
are installed below `lib/`, `include/`, `examples/`, and
`lib/cmake/altun/`. When bundled Riwo is enabled, its libraries and package
files are installed into the same prefix.
