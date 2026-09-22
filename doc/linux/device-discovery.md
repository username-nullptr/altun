# Device discovery

[Linux module](../linux.md) · [Documentation](../README.md)

The udev API has two parts:

| Header | Interface | Purpose |
| --- | --- | --- |
| `linux/udev/enumeration.h` | `udev::enumeration<Subsys>` | List devices and read udev properties |
| `linux/udev/event.h` | `udev::event<Subsys>` | Receive device lifecycle events |

Supported subsystems are `usb`, `tty`, `net`, `block`, `backlight`, `led`, `gpio`, and `pwm`.

## Enumerate

```cpp
#include <libempp/linux/udev/enumeration.h>

using tty_device =
    libempp::udev::enumeration<libempp::subsys_enum::tty>;

auto devices = tty_device::list(
    libempp::udev::prop_key::id_model,
    "USB_Serial*"
);

for(const auto &device : devices)
{
    if(auto name = device.property(libempp::udev::prop_key::dev_name))
        std::cout << name->to_string() << '\n';
}
```

Property values accept wildcards. A property map uses AND matching:

```cpp
auto devices = tty_device::list({
    {libempp::udev::prop_key::id_vendor_id, "1a86"},
    {libempp::udev::prop_key::id_model_id, "7523"}
});
```

`list()` without properties returns all devices in the subsystem. `path()` returns the sysfs path; `property_keys()` lists the udev properties actually present. Property names may be passed directly when `prop_key` does not define one.

## Monitor

```cpp
#include <libempp/linux/udev/event.h>

using block_events =
    libempp::udev::event<libempp::subsys_enum::block>;

block_events events;
events.received.connect([](const libempp::udev::device_event &event) {
    std::cout << libempp::udev::string(event.action)
              << ' ' << event.dev_node << '\n';
});
events.error.connect([](const std::error_code &error) {
    std::cerr << error.message() << '\n';
});
events.open("disk");
```

`open("disk")` and `open("partition")` add a device-type filter; `open()` monitors the whole subsystem. Each `device_event` owns its path, node, type, and property data.

Every connected observer receives the event. Slow synchronous slots delay the monitor loop; explicitly asynchronous slots move queueing responsibility to the application. Kernel event delivery is not durable, so re-enumerate after monitor errors or suspected event loss.

See [`examples/linux/udev.cpp`](../../examples/linux/udev.cpp) and [`examples/linux/udev_event.cpp`](../../examples/linux/udev_event.cpp).
