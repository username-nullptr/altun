# Core module guide

[Documentation home](README.md) · [Building and integration](getting-started.md)

The Core module provides logging, INI configuration, software bus support, and plugin management. Its link target is `empp.core`. Include the `<libempp/core.h>` aggregate header, or include only the component headers you need to reduce compilation time.

## Version information

```cpp
#include <libempp/core/global.h>
#include <iostream>

int main()
{
    std::cout << libempp::version_string() << '\n';
}
```

Development builds return the project version from the top-level `CMakeLists.txt` with a `-dev` suffix. Treat this interface as authoritative rather than maintaining a separate libEMpp version string in the application.

## Logging

The `<libempp/core/log.h>` header provides two families of formatted logging macros:

- `libempp_log_trace/debug/info/warning/error/critical(...)` uses the default logger.
- `libempp_clog_trace/debug/info/warning/error/critical(name, ...)` uses a named logger.

Format strings use fmt syntax:

```cpp
#include <libempp/core/log.h>

void report(double temperature)
{
    libempp_log_info("Temperature: {:.1f} C", temperature);
    libempp_clog_warning("sensor", "Value is near the limit: {}", temperature);
}
```

The macros forward to the LibGS logger. Further customization of levels, sinks, and named loggers is available in `libgs/utils/logger.h`.

## INI configuration

`libempp_default_settings` returns the process-wide default settings instance. `libempp_settings("name")` returns a named instance. The library owns these instances; do not release them manually.

```cpp
#include <libempp/core/settings.h>
#include <iostream>

int main()
{
    auto &settings = libempp_default_settings;

    if(auto result = settings.load("service.ini"); not result)
    {
        std::cerr << result.error().message() << '\n';
        return 1;
    }

    settings
        .set("server/host", "127.0.0.1")
        .set("server/port", 8080);

    if(auto result = settings.sync(); not result)
    {
        std::cerr << result.error().message() << '\n';
        return 1;
    }

    if(auto value = settings.get("server/port"))
        std::cout << value->to_uint().value_or(0) << '\n';
}
```

The corresponding INI content is:

```ini
[server]
host=127.0.0.1
port=8080
```

Paths use the `group/key` form. `load()` and `sync()` return expected-style results, so callers must check the failure path. Signals can report value changes and completed file loads:

```cpp
settings.changed.connect([](std::string_view key, const libgs::value &value) {
    libempp_log_info("{} changed to {}", key, value.to_string());
});

settings.loaded.connect([] {
    libempp_log_info("Settings loaded");
});
```

`set()` changes only the in-memory value. Call `sync()` to persist changes.

## Software bus

`<libempp/core/sbus/sbus.h>` provides typed and raw-byte publish/subscribe interfaces. It selects the LibGS `default_interface` by default; set `LIBEMPP_SBUS_INTERFACE=local` or `udp` to pin a LibGS backend. POSIX platforms can also select the `shm` backend for same-host, cross-process communication with the same API:

```cpp
#include <libempp/core/sbus/sbus.h>

asio::thread_pool pool(1);
libempp::sbus::subscriber subscriber(pool);
subscriber.subscribe("sensor.temperature", [](const void *data, size_t size) {
    // data is valid only during this callback; copy it for asynchronous use.
});

const double value = 23.5;
libempp::sbus::publish("sensor.temperature", &value, sizeof(value));
```

The shared-memory backend uses a bounded 32 MiB ring with independent process read cursors and 64 KiB frames. It supports topics up to 4 KiB and payloads up to 16 MiB. Publishing applies backpressure until the slowest active reader releases enough space. If a process exits unexpectedly, later publishes reclaim its read cursor. Same-process subscriptions are delivered directly; the process's own shared-memory messages advance the cursor without triggering duplicate callbacks. Processes in one namespace must use ABI-compatible libEMpp versions. The current implementation does not support rolling upgrades on the same shared-memory region. See [SBus backends](getting-started.md#sbus-backends) for build and namespace options.

## Shared-library plugins

`libempp::plugin_manager` loads shared libraries from JSON or in-memory configuration and resolves C ABI symbols through the LibGS `library::interface`.

### Plugin interface

Export entry points with `extern "C"` to avoid C++ name mangling and ABI differences:

```cpp
#include <libgs/core/cxx/attributes.h>

extern "C" LIBGS_DECL_EXPORT int square(int value)
{
    return value * value;
}
```

### In-memory configuration and loading

```cpp
#include <libempp/core/plugin_manager.h>
#include <iostream>

int main()
{
    libempp::plugin_manager::set_config({
        {
            {"path", "./plugins"},
            {"libs", {{"math", "math-plugin.so"}}}
        }
    });
    libempp::plugin_manager::load();

    auto library = libempp::plugin_manager::library("math");
    if(not library)
        return 1;

    auto square = (*library)->interface<int(int)>("square");
    if(not square)
        return 1;

    std::cout << (*square)(12) << '\n';
}
```

### JSON configuration file

Equivalent `plugins.json`:

```json
[
  {
    "path": "./plugins",
    "libs": {
      "math": "math-plugin.so"
    }
  }
]
```

Load the configuration file with:

```cpp
libempp::plugin_manager::set_config_file("plugins.json");
libempp::plugin_manager::load();
```

When a configuration file is used, a relative `path` is resolved from the file's directory. One package can declare multiple libraries:

```json
[
  {
    "path": "./plugins",
    "libs": {
      "codec": "codec.so",
      "device": "device.so"
    }
  }
]
```

`parse()` only parses configuration and fills `library_nodes()`/`process_nodes()`. `load()` loads shared libraries and starts configured processes. The plugin manager has process-wide singleton state. After `load()` successfully enters initialization, configuration cannot be changed and loading cannot be repeated, so configure it once during application startup.

Complete runnable plugin and host implementations are in [`examples/core/plugin.cpp`](../examples/core/plugin.cpp) and [`examples/core/plugin_manager.cpp`](../examples/core/plugin_manager.cpp).

## Subprocess plugins

Plugin configuration also supports `apps`. Each entry can specify an executable, arguments, and environment variables:

```json
[
  {
    "path": "./plugins",
    "apps": {
      "collector": {
        "file_name": "collector",
        "args": ["--interval", "1000"],
        "envs": {
          "LOG_LEVEL": "info"
        }
      }
    }
  }
]
```

The loader also sets `LIBEMPP_PLUGIN_GROUP` and `LIBEMPP_PLUGIN_NAME` in the child process. Call `plugin_manager::process("collector")` to retrieve the started process object. Production deployments should also define restart behavior, log collection, and least-privilege execution.
