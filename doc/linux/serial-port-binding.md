# Serial-port binding

[Linux module](../linux.md) · [Execution model](../io-model.md)

`libempp::serial_port_binding` finds tty devices by udev properties or an explicit path, tracks hot-plug changes, and exposes serial I/O through signals. Include `<libempp/linux/serial_port_binding.h>` explicitly and link `empp.linux`.

## Configure and open

```cpp
#include <libempp/linux/serial_port_binding.h>

libempp::serial_port_options options;
options.baud_rate = 115200;
options.data_bits = libempp::serial_port_options::data_bits_8;
options.stop_bits = libempp::serial_port_options::stop_bits_1;
options.parity = libempp::serial_port_options::parity_t::none;
options.flow_control = libempp::serial_port_options::flow_control_t::none;

libempp::serial_port_binding binding;
binding.received.connect(
    [](const libempp::serial_port_binding::io_context_ptr &context) {
        auto payload = context->take_payload<std::string>();
        std::error_code error;
        context->write("ACK\n", error);
    }
);

auto rule = binding.make_rule(
    libempp::udev::prop_key::id_model,
    "USB_Serial*",
    options
);
rule->open();
return riwo::exec();
```

Keep the returned `rule_context` alive while monitoring. `binding` aggregates events from all rules; each rule also exposes `opened`, `closed`, `received`, and `error`.

Property maps use AND matching. An explicit device path bypasses property matching:

```cpp
auto rule = binding.make_rule(
    libempp::serial_port_binding::device_t("/dev/ttyUSB0"),
    options
);
```

At startup a property rule enumerates existing tty devices, then follows udev events. Failed opens for a known node are retried; the entire subsystem is not periodically rescanned.

## I/O ownership

- `rule_context::write()` writes to all, one, or selected open ports in the rule.
- `io_context::write()` replies to the port that produced the payload.
- `payload()` views received data; `take_payload()` transfers it as bytes or `std::string`.
- Ordinary asynchronous writes borrow their buffer; detached writes copy it.
- Closing or destroying a rule cancels its monitor and serial operations.

Check actual udev properties, `/dev/tty*` permissions, serial parameters, event-loop lifetime, and the `closed`/`error` signals when a device does not stay connected.

See [`examples/linux/serial_port_binding.cpp`](../../examples/linux/serial_port_binding.cpp).
