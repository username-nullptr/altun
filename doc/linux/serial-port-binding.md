# Serial-port binding

[Back to the Linux module guide](../linux.md) · [Back to the documentation index](../README.md)

`libempp::serial_port_binding` creates rules from udev properties or explicit port names, opens matching devices, performs asynchronous I/O, and converts lifecycle events into signals. It is designed for hot-pluggable USB serial ports. Include `<libempp/linux/serial_port_binding.h>` and link `empp.linux`.

## How it works

After a property rule starts, it enumerates existing tty devices once and then processes additions, removals, and property changes through udev events. It does not periodically rescan the whole tty subsystem. If a device node exists but cannot yet be opened, only that node is retried on a timer. Closing or destroying the rule cancels the udev wait and any serial operations in progress.

## Serial-port options

```cpp
libempp::serial_port_options options;
options.baud_rate = 115200;
options.data_bits = libempp::serial_port_options::data_bits_8;
options.stop_bits = libempp::serial_port_options::stop_bits_1;
options.parity = libempp::serial_port_options::parity_t::none;
options.flow_control = libempp::serial_port_options::flow_control_t::none;
```

Predefined baud rates range from 4800 to 460800. You may also assign another device-supported value to `baud_rate`. Data bits support 5, 6, 7, or 8; stop bits support 1, 1.5, or 2; and flow control supports none, software, or hardware modes.

## Binding by property

```cpp
#include <libempp/linux/serial_port_binding.h>

int main()
{
    libempp::serial_port_binding binding;

    binding.opened.connect([](std::string_view port) {
        libempp_log_info("opened {}", port);
    });

    binding.received.connect(
        [](const libempp::serial_port_binding::io_context_ptr &context)
        {
            auto payload = context->take_payload<std::string>();
            libempp_log_info("{}: {}", context->port(), payload);

            std::error_code error;
            context->write("ACK\n", error);
        }
    );

    auto rule = binding.make_rule(
        libempp::udev::prop_key::id_model,
        "USB_Serial*"
    );
    rule->open();

    return libgs::exec();
}
```

The rule object must remain alive for as long as monitoring is required. `binding` exposes aggregate events for every rule, while each `rule_context` exposes its own `opened`, `closed`, `received`, and `error` signals.

Multi-property rules require every property to match. You can also bypass udev property matching and specify an explicit port:

```cpp
auto by_properties = binding.make_rule({
    {libempp::udev::prop_key::id_vendor_id, "1a86"},
    {libempp::udev::prop_key::id_model_id, "7523"}
});

auto by_port = binding.make_rule(
    libempp::serial_port_binding::device_t("/dev/ttyUSB0")
);
```

Call `open()` after creating a rule to begin opening devices and receiving data.

## I/O and lifetimes

- `rule_context::write()` writes to all currently open ports for the rule, one port, or a selected set of ports.
- `io_context::write()` replies to the port that produced the current received data.
- `io_context::payload()` views received data, while `take_payload()` transfers ownership. Both support byte vectors and `std::string`.
- A synchronous write finishes using its buffer before returning. A normal asynchronous write requires the buffer to remain valid until completion. A detached write copies the transmitted data.
- `closed` marks the end of an opened port's lifecycle. `error` covers opening, I/O, and monitor errors. Logs should retain the port name and `std::error_code` supplied by the signal.

Destroying a rule stops hot-plug monitoring. If an application depends on the rule, store the returned `shared_ptr<rule_context>` for at least as long as the corresponding service.

## Troubleshooting order

1. Run `udevadm info -q property /dev/tty...` and confirm that the rule's keys and values exist.
2. Check `/dev/tty*` permissions, group membership, serial parameters, and the device driver.
3. Subscribe to `closed` and `error` to distinguish removal, open failure, and I/O failure.
4. Confirm that the event loop is running and neither `binding` nor `rule_context` was destroyed early.

A complete program is available in [`examples/linux/serial_port_binding.cpp`](../../examples/linux/serial_port_binding.cpp).
