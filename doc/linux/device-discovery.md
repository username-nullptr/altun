# udev device discovery

[Back to the Linux module guide](../linux.md) · [Back to the documentation index](../README.md)

The udev interfaces find devices by subsystem and property and monitor hot-plug or property changes. Their headers are `<libempp/linux/udev/enumeration.h>` and `<libempp/linux/udev/event.h>`, and both use the `empp.linux` link target.

## Enumerating devices

`libempp::udev::enumeration<Subsys>` enumerates devices by subsystem and properties. Supported subsystems include `usb`, `tty`, `net`, `block`, `backlight`, `led`, `gpio`, and `pwm`; `led` maps to the kernel subsystem name `leds`.

This example matches tty devices by `ID_MODEL`:

```cpp
#include <libempp/linux/udev/enumeration.h>
#include <iostream>

using tty_device =
    libempp::udev::enumeration<libempp::subsys_enum::tty>;

int main()
{
    auto devices = tty_device::list(
        libempp::udev::prop_key::id_model,
        "USB_Serial*"
    );

    for(const auto &device : devices)
    {
        if(auto name = device.property(libempp::udev::prop_key::dev_name))
            std::cout << name->to_string() << '\n';
    }
}
```

Property values support wildcard matching. Every rule in a multi-property query must match:

```cpp
auto devices = tty_device::list({
    {libempp::udev::prop_key::id_vendor_id, "1a86"},
    {libempp::udev::prop_key::id_model_id, "7523"}
});
```

Calling `list()` without filters returns every device in the subsystem:

```cpp
using net_device =
    libempp::udev::enumeration<libempp::subsys_enum::net>;

for(const auto &device : net_device::list())
{
    if(auto name = device.property(libempp::udev::prop_key::interface))
        std::cout << name->to_string() << '\n';
}
```

`prop_key` provides common udev properties, including:

- Basic device information: `DEVNAME`, `DEVTYPE`, `DEVLINKS`, `DRIVER`, and `MODALIAS`.
- Hardware identity and paths: `ID_VENDOR_ID`, `ID_MODEL_ID`, `ID_SERIAL_SHORT`, and `ID_PATH`.
- Filesystem and partition data: `ID_FS_TYPE`, `ID_FS_UUID`, and `ID_PART_ENTRY_UUID`.
- Input-device types and network-interface properties.

You can also pass a udev property name directly. `path()` returns the sysfs path, and `property_keys()` helps diagnose the properties a device actually exposes. These keys are udev properties, not sysfs attributes. For example, access a backlight's `brightness` through `libempp::subsys::backlight`.

## Monitoring device events

`libempp::udev::event<Subsys>` integrates a libudev monitor with LibGS/Asio. Each `device_event` owns copies of its path, node name, and properties, so it does not depend on the lifetime of a native `udev_device`.

```cpp
#include <libempp/linux/udev/event.h>
#include <iostream>

using block_events =
    libempp::udev::event<libempp::subsys_enum::block>;

block_events events;
events.received.connect([](const libempp::udev::device_event &event) {
    std::cout << libempp::udev::string(event.action)
              << ' ' << event.dev_node << '\n';
});
events.error.connect([](const std::error_code &error) {
    std::cerr << "udev monitor: " << error.message() << '\n';
});
events.open("disk");
```

Actions include `add`, `remove`, `change`, `move`, `online`, `offline`, `bind`, and `unbind`; any other action is preserved as `unknown`. `device_event::matches()` uses the same property rules as enumeration. `open("disk")` or `open("partition")` adds a kernel-level filter by device type. Call `open()` without arguments when no filter is needed.

## Consumption and recovery semantics

The event interface exposes signals rather than `next()`: every observer receives the same event. A synchronous slot connected in the default mode is awaited by the reader coroutine and therefore applies backpressure without creating an unbounded queue inside the library. When `slot_mode::async` is used explicitly, tasks may accumulate on the corresponding executor; the caller owns that queueing and concurrency policy.

Linux netlink sockets still have finite capacity, so the kernel can drop events when observers block for too long. Applications that must recover the final device state should run `udev::enumeration` again after an `error` signal or another anomaly. Do not treat hot-plug events as a durable, complete event log.

Complete programs are available in [`examples/linux/udev.cpp`](../../examples/linux/udev.cpp) and [`examples/linux/udev_event.cpp`](../../examples/linux/udev_event.cpp).
