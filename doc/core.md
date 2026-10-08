# Core module

[Documentation](README.md) · [Build options](getting-started.md)

Link `empp.core`. Include individual headers, or use `<libempp/core.h>` for the whole module.

| Area | Header | Main interface |
| --- | --- | --- |
| Version | `core/global.h` | `libempp::version_string()` |
| Logging | `core/log.h` | `libempp_log_*`, `libempp_clog_*` |
| Settings | `core/settings.h` | `libempp_default_settings`, `libempp_settings(name)` |
| Software bus | `core/sbus.h` | `libempp::sbus` |
| Plugins | `core/plugin_manager.h` | `libempp::plugin_manager` |

## Version and logging

```cpp
#include <libempp/core/log.h>

libempp_log_info("libEMpp {}", libempp::version_string());
libempp_clog_warning("sensor", "temperature: {:.1f}", 82.5);
```

The first family uses the default Riwo logger; `libempp_clog_*` uses a named logger. Format strings use fmt syntax.

## Settings

The settings macros return process-owned Riwo settings instances. Keys use `group/key`; `set()` changes memory and `sync()` writes the loaded file.

```cpp
#include <libempp/core/settings.h>

auto &settings = libempp_default_settings;
if(auto result = settings.load("service.ini"); not result)
    return 1;

settings.set("server/host", "127.0.0.1")
        .set("server/port", 8080);

if(auto result = settings.sync(); not result)
    return 1;
```

Use `changed` and `loaded` signals when the application needs notifications. See [`examples/core/settings.cpp`](../examples/core/settings.cpp) for a complete program.

## Software bus

`libempp::sbus` exposes Riwo-compatible typed and raw publish/subscribe operations. The build selects one interface; see [SBus build options](getting-started.md#sbus).

```cpp
#include <libempp/core/sbus/sbus.h>

asio::thread_pool pool(1);
libempp::sbus::subscriber subscriber(pool);
subscriber.subscribe("sensor.temperature", [](const void *data, size_t size) {
    // Copy data here if it must outlive the callback.
});

const double value = 23.5;
libempp::sbus::publish("sensor.temperature", &value, sizeof(value));
```

The `local` backend is in-process; `udp`, `dbus`, `cyclone`, and `shm` provide transport-specific communication. All processes using one shared-memory namespace must use ABI-compatible builds.

## Plugins

`plugin_manager` parses one process-wide JSON configuration, loads shared libraries, and starts configured child processes. Configure it before the first `load()`; loading is a one-time startup operation.

```json
[
  {
    "path": "./plugins",
    "libs": {
      "math": "math-plugin.so"
    },
    "apps": {
      "collector": {
        "file_name": "collector",
        "args": ["--interval", "1000"],
        "envs": {"LOG_LEVEL": "info"}
      }
    }
  }
]
```

```cpp
#include <libempp/core/plugin_manager.h>

libempp::plugin_manager::set_config_file("plugins.json");
libempp::plugin_manager::load();

auto library = libempp::plugin_manager::library("math");
if(library)
{
    auto square = (*library)->interface<int(int)>("square");
    if(square)
        return (*square)(4) == 16 ? 0 : 1;
}
return 1;
```

Export shared-library entry points with `extern "C"`. Relative plugin paths are resolved from the configuration file's directory. Child processes receive `LIBEMPP_PLUGIN_GROUP` and `LIBEMPP_PLUGIN_NAME`.

Runnable plugin code is in [`examples/core/plugin.cpp`](../examples/core/plugin.cpp) and [`examples/core/plugin_manager.cpp`](../examples/core/plugin_manager.cpp).
