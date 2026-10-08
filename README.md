# Altun

altun (‌ئالتۇن) is a C++20 component library for embedded applications. The source tree has two library modules:

| Module | Target | Contents |
| --- | --- | --- |
| Core | `altun.core` | Logging, INI settings, SBus, shared-library plugins, and subprocess plugins |
| Linux | `altun.linux` | udev, storage, I²C, SPI, GPIO, PWM, LEDs, backlights, and serial-port binding |

`altun.linux` is built only on Linux and publicly links `altun.core`. Riwo is included as a Git submodule and provides the execution, logging, and utility foundations.

## Build

Requirements: CMake 3.16+, C++20, and GCC 13+, Clang 17+, or supported MSVC 2022. Linux builds also need `pkg-config`, `libudev`, and `libblkid` development files.

```sh
git clone --recurse-submodules https://gitee.com/jin-xiaoqiang/altun.git
cd altun
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Shared libraries are written to `build/output/bin/`. See [Building and integration](doc/getting-started.md) for dependencies, options, tests, installation, and CMake integration.

## Use

```cpp
#include <altun/core/log.h>

int main()
{
    altun_log_info("altun {}", altun::version_string());
}
```

```cmake
add_subdirectory(third_party/altun)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE altun.core)
```

Link Linux applications to `altun.linux`; it already carries the public Core dependency.

After installing altun, downstream projects can use its CMake package:

```cmake
find_package(altun 0.6 CONFIG REQUIRED COMPONENTS core linux)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE altun::linux)
```

The installed package also provides the compatibility targets `altun.core` and
`altun.linux`.

## Documentation

- [Documentation index](doc/README.md)
- [Core module](doc/core.md)
- [Linux module](doc/linux.md)
- [Execution and I/O model](doc/io-model.md)
- [Examples](examples/README.md)
- [Tests](test/README.md)

## Repository layout

```text
altun/core/      Core public headers and implementations
altun/linux/     Linux public headers and implementations
doc/               Build and module guides
examples/          Runnable API examples
test/              Functional, stress, and performance tests
cmake/             Build configuration
3rd_party/riwo/   Bundled Riwo source
```

Hardware writes and storage mutation take effect immediately. Verify the device, permissions, electrical parameters, mount state, and stable device identity before using those APIs.
