# libEMpp

libEMpp is a C++20 component library for embedded applications. The source tree has two library modules:

| Module | Target | Contents |
| --- | --- | --- |
| Core | `empp.core` | Logging, INI settings, SBus, shared-library plugins, and subprocess plugins |
| Linux | `empp.linux` | udev, storage, I²C, SPI, GPIO, PWM, LEDs, backlights, and serial-port binding |

`empp.linux` is built only on Linux and publicly links `empp.core`. LibGS is included as a Git submodule and provides the execution, logging, and utility foundations.

## Build

Requirements: CMake 3.16+, C++20, and GCC 13+, Clang 17+, or supported MSVC 2022. Linux builds also need `pkg-config`, `libudev`, and `libblkid` development files.

```sh
git clone --recurse-submodules https://gitee.com/jin-xiaoqiang/libempp.git
cd libempp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Shared libraries are written to `build/output/bin/`. See [Building and integration](doc/getting-started.md) for dependencies, options, tests, installation, and CMake integration.

## Use

```cpp
#include <libempp/core/log.h>

int main()
{
    libempp_log_info("libEMpp {}", libempp::version_string());
}
```

```cmake
add_subdirectory(third_party/libempp)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE empp.core)
```

Link Linux applications to `empp.linux`; it already carries the public Core dependency.

After installing libEMpp, downstream projects can use its CMake package:

```cmake
find_package(libEMpp 0.6 CONFIG REQUIRED COMPONENTS core linux)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE libEMpp::linux)
```

The installed package also provides the compatibility targets `empp.core` and
`empp.linux`.

## Documentation

- [Documentation index](doc/README.md)
- [Core module](doc/core.md)
- [Linux module](doc/linux.md)
- [Execution and I/O model](doc/io-model.md)
- [Examples](examples/README.md)
- [Tests](test/README.md)

## Repository layout

```text
libempp/core/      Core public headers and implementations
libempp/linux/     Linux public headers and implementations
doc/               Build and module guides
examples/          Runnable API examples
test/              Functional, stress, and performance tests
cmake/             Build configuration
3rd_party/libgs/   Bundled LibGS source
```

Hardware writes and storage mutation take effect immediately. Verify the device, permissions, electrical parameters, mount state, and stable device identity before using those APIs.
