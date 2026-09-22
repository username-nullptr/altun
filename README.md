# libEMpp

libEMpp is a C++20 component library for embedded applications. The Core module provides logging, INI configuration, and plugin management. The Linux module provides interfaces for udev, storage, I²C, SPI, GPIO, PWM, LEDs, backlights, and serial devices. Asynchronous execution, logging, and foundational utilities are provided by the bundled [LibGS](3rd_party/libgs/README.md).

| Module | CMake target | Main capabilities |
| --- | --- | --- |
| Core | `empp.core` | Logging, INI configuration, shared-library plugins, and subprocess plugins |
| Linux | `empp.linux` | Device discovery, storage, buses, hardware control, and serial-port binding |

`empp.linux` publicly depends on `empp.core`. Application code should include only the component headers it needs. Use the `<libempp/core.h>` or `<libempp/linux.h>` aggregate headers only when the whole module is required.

## Quick start

The basic requirements are CMake 3.16, C++20, GCC 13+ or Clang 17+. Linux builds also require the development packages for `pkg-config`, `libudev`, and `libblkid`.

On Debian or Ubuntu, install:

```sh
sudo apt update
sudo apt install build-essential cmake pkg-config libudev-dev libblkid-dev \
  libgpiod-dev util-linux e2fsprogs
```

Clone and build the source:

```sh
git clone --recurse-submodules https://gitee.com/jin-xiaoqiang/libempp.git
cd libempp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

If an existing source tree is missing its submodules, run:

```sh
git submodule update --init --recursive
```

Shared libraries are written to `build/output/bin/`. See [Building and integration](doc/getting-started.md) for dependencies, build options, installation, and CMake integration.

## Minimal example

```cpp
#include <libempp/core/log.h>

int main()
{
    libempp_log_info("libEMpp {} is ready", libempp::version_string());
    return 0;
}
```

Link against `empp.core`:

```cmake
add_subdirectory(third_party/libempp)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE empp.core)
```

For Linux device interfaces, link against `empp.linux`; there is no need to link `empp.core` again.

## Documentation

| Goal | Entry point |
| --- | --- |
| Install dependencies, configure builds, install, and integrate with a project | [Building and integration](doc/getting-started.md) |
| Find public headers and module documentation | [Documentation index](doc/README.md) |
| Understand completion tokens, executors, and thread-safety conventions | [Execution and I/O model](doc/io-model.md) |
| Use logging, configuration, and plugins | [Core module](doc/core.md) |
| Use Linux device interfaces | [Linux module](doc/linux.md) |
| Run minimal programs to verify the environment or hardware | [Examples](examples/README.md) |
| Build and run automated tests | [Testing](test/README.md) |

## Usage boundaries

- Linux device interfaces depend on kernel drivers, the device tree, device nodes, udev properties, and access permissions.
- The I²C, SPI, GPIO, PWM, LED, backlight, and serial examples access hardware directly. Verify the device and parameters before running them.
- Formatting, partition changes, mounting, and unmounting alter system or storage state. Confirm the target and its current use before calling them.
- Observe all object and buffer lifetime requirements during asynchronous operations. See [Execution and I/O model](doc/io-model.md).

## Repository layout

```text
libempp/           Public headers and implementations
  core/            Logging, configuration, and plugin management
  linux/           Linux device and subsystem wrappers
doc/               Build instructions and API guides
examples/          Runnable minimal examples
test/              Functional, stress, and performance tests
cmake/             Build helper modules
3rd_party/libgs/   LibGS submodule
```
