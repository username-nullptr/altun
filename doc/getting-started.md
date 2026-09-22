# Building and integration

[Documentation home](README.md) · [Examples](../examples/README.md) · [Testing guide](../test/README.md)

This guide covers source acquisition, dependencies, build options, installation, and CMake integration. Start at the [documentation home](README.md) for API usage.

## Requirements

| Dependency | Requirement or purpose |
| --- | --- |
| CMake | 3.16+ |
| C++ | C++20 |
| GCC | 13+ |
| Clang | 17+ |
| MSVC | 1930+; 1950 is unsupported; only platform-available modules are built |
| LibGS | Git submodule bundled in this repository |
| pkg-config | Finds `libudev`, `libblkid`, and optional `libgpiod` on Linux |
| libudev development package | Required by `empp.linux` |
| libblkid development package | Required by `empp.linux` |
| libgpiod development package | Optional GPIO character-device backend; supports 1.x and 2.x |
| Linux PWM waveform UAPI headers | Optional PWM character-device backend |
| util-linux | Provides `sfdisk` for partition operations at runtime |
| Filesystem tools | Provide the corresponding `mkfs.*` or `mkswap` command for formatting |

Base environment on Debian or Ubuntu:

```sh
sudo apt update
sudo apt install build-essential cmake pkg-config libudev-dev libblkid-dev \
  libgpiod-dev util-linux e2fsprogs
```

Hardware interfaces also require the corresponding kernel drivers and permission for the current account to access device nodes or sysfs attributes.

## Getting the source

```sh
git clone --recurse-submodules https://gitee.com/jin-xiaoqiang/libempp.git
cd libempp
```

Initialize submodules in an existing source tree with:

```sh
git submodule update --init --recursive
```

## Building

Release build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Output directories:

| Content | Directory |
| --- | --- |
| Shared libraries | `build/output/bin/` |
| Static libraries | `build/output/lib/` |
| Examples | `build/output/examples/core/`, `build/output/examples/linux/` |
| Test executables | `build/output/bin/` |

Build the examples:

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DLIBEMPP_BUILD_EXAMPLES=ON
cmake --build build --parallel
```

Build and run functional tests:

```sh
cmake -S . -B build-test \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON
cmake --build build-test --parallel
ctest --test-dir build-test -R '^empp\.' -L functional --output-on-failure
```

See the [testing guide](../test/README.md) for test categories and stress and sanitizer configurations.

## CMake options

### Libraries and examples

| Option | Default | Purpose |
| --- | :---: | --- |
| `LIBEMPP_BUILD_EXAMPLES` | `OFF` | Build programs in `examples/` |
| `LIBEMPP_BUILD_STATIC` | `OFF` | Build static libraries |
| `LIBEMPP_ADD_LIBRARY_VERSION` | `ON` | Set version and SOVERSION on shared libraries |
| `LIBGS_BUILD_STATIC` | `OFF` | Build the bundled LibGS components as static libraries |

`LIBEMPP_ADD_LIBRARY_VERSION` applies only to shared-library builds.

### Linux backends

| Option | Default | `AUTO` | `ON` | `OFF` |
| --- | :---: | --- | --- | --- |
| `LIBEMPP_USE_GPIOD` | `AUTO` | Use the character device when libgpiod is detected; otherwise use sysfs | libgpiod must be found | Use sysfs |
| `LIBEMPP_USE_PWM_CDEV` | `AUTO` | Use the character device when the PWM waveform UAPI is detected; otherwise use sysfs | A suitable `<linux/pwm.h>` is required | Use sysfs |

GPIO and PWM backends are selected during CMake configuration. At runtime, use `gpio::backend_name()` and `pwm::backend_name()` to record the selected backend.

### SBus backends

| Option | Default | Purpose |
| --- | :---: | --- |
| `LIBEMPP_BUILD_SBUS_SHM` | `OFF` | Build the POSIX same-host shared-memory transport |
| `LIBEMPP_BUILD_SBUS_DBUS` | `OFF` | Build the D-Bus transport |
| `LIBEMPP_BUILD_SBUS_CYCLONE` | `OFF` | Build the CycloneDDS transport |
| `LIBEMPP_SBUS_INTERFACE` | `default` | Select the `default`, `local`, `udp`, `shm`, `dbus`, or `cyclone` transport |

`default` uses `libgs::utils::sbus::default_interface`; its concrete transport is selected by `LIBGS_UTILS_SBUS_DEFAULT_INTERFACE`. `local` and `udp` explicitly select the LibGS `local_interface` and `udp_interface` and are independent of the LibGS default. Selecting `udp` requires `LIBGS_BUILD_UTILITIES_SBUS_UDP=ON`, which is enabled by default.

The shared-memory backend currently supports POSIX platforms. Enable and select it with:

```sh
cmake -S . -B build-shm \
  -DLIBEMPP_BUILD_SBUS_SHM=ON \
  -DLIBEMPP_SBUS_INTERFACE=shm \
  -DCMAKE_BUILD_TYPE=Release
```

Processes owned by the same user use the `/libempp-sbus-<uid>` shared-memory namespace by default. To isolate application instances or test environments, set the same `LIBEMPP_SBUS_SHM_NAME=/name` in every participating process. The name must begin with `/` and contain no other `/` characters.

### Build resources

| Option | Default | Purpose |
| --- | :---: | --- |
| `LIBEMPP_HEAVY_COMPILE_JOBS` | GCC `2`, Clang `4`, others `0` | Limit parallel compilation of high-memory targets; `0` means unlimited |
| `LIBEMPP_LOW_MEMORY_DEBUG_INFO` | `OFF` | Use `-g1` for GCC Debug builds |

Testing options are listed in the [testing guide](../test/README.md).

## Installation

```sh
cmake --install build --prefix /opt/libempp
```

Shared libraries, public headers, and enabled examples are installed to `<prefix>/lib/`, `<prefix>/include/`, and `<prefix>/examples/`, respectively. Static-library targets do not define installation rules; integrate static builds with `add_subdirectory`. Installing to a system directory requires the corresponding write permission.

## CMake project integration

The repository provides targets through source-directory integration:

```cmake
add_subdirectory(third_party/libempp)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE empp.core)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(my_app PRIVATE empp.linux)
endif()
```

`empp.linux` publicly links `empp.core` and `libudev`, so applications do not need to declare them again. The project does not provide `libEMppConfig.cmake`; do not use `find_package(libEMpp)`.

## Common build issues

| Symptom | Resolution |
| --- | --- |
| LibGS headers are missing | Run `git submodule update --init --recursive` |
| `libudev` or `libblkid` is missing | Install the development packages and `pkg-config`, then reconfigure CMake |
| An example cannot find shared libraries at startup | Run it from the build tree, or install the libraries and configure the dynamic-linker search path |
| Compiler version is unsupported | Select GCC 13+, Clang 17+, or a supported MSVC toolchain |
| A device returns `Permission denied` | Check the device node, group membership, and narrowly scoped udev access rules |

See the [Linux module](linux.md) for device existence, permission, and parameter troubleshooting.
