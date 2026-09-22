# Backlights, LEDs, GPIO, and PWM

[Back to the Linux module guide](../linux.md) · [Back to the documentation index](../README.md)

These interfaces are in `libempp::subsys` and use the `empp.linux` link target. Backlights and LEDs use sysfs. GPIO and PWM select a character-device or sysfs backend at build time; see [GPIO and PWM backends](../linux.md#gpio-and-pwm-backends).

## Backlights

`libempp::subsys::backlight` operates on the standard attributes under `/sys/class/backlight/<name>/`. It reads maximum and actual brightness and provides chainable setters:

```cpp
#include <libempp/linux/subsys/backlight.h>
#include <iostream>

int main()
{
    std::error_code error;
    libempp::subsys::backlight display({"intel_backlight"});
    display.set_brightness(500, error);
    if(!error)
        display.enable(error);
    if(error)
        return 1;

    std::cout << display.actual_brightness() << '/'
              << display.max_brightness() << '\n';
}
```

`set_brightness()` rejects values above `max_brightness()`. `enable()` and `disable()` set `bl_power` to `unblank` and `power_down`, respectively. Select other sleep levels with `set_power()` and `normal`, `vsync_suspend`, or `hsync_suspend`.

While the object is open, it acquires non-blocking exclusive locks on the `brightness` and `bl_power` files. Closing or destroying the object releases the files without restoring the previous brightness or power state. `actual_brightness()` rereads the value reported by the driver.

## LEDs

`libempp::subsys::led` operates on `/sys/class/leds/<name>/`. In addition to brightness, it can inspect and set kernel LED triggers. Trigger methods return `operation_not_supported` when the device does not support them.

```cpp
#include <libempp/linux/subsys/led.h>

using namespace std::chrono_literals;

int main()
{
    libempp::subsys::led status({"status:red:system"});
    status.set_brightness(status.max_brightness());

    if(status.supports_triggers())
        status.set_blink(200ms, 800ms);
}
```

`set_trigger()` accepts only triggers currently listed by the device, such as `none`, `timer`, or `heartbeat`; `triggers()` returns the available values. `set_blink()` selects `timer` and sets `delay_on` and `delay_off` in milliseconds. Closing or destroying the object does not restore the previous brightness or trigger.

## GPIO

Each `libempp::subsys::gpio` object requests one GPIO line. With libgpiod, `chip` may be `/dev/gpiochip0` or `gpiochip0`; `line` is the offset within that chip, not the global sysfs GPIO number.

```cpp
#include <libempp/linux/subsys/gpio.h>

int main()
{
    using gpio = libempp::subsys::gpio;

    gpio::node_t node;
    node.chip = "/dev/gpiochip0";
    node.line = 17;
    node.direction = gpio::direction_t::output;
    node.initial_value = false;
    node.consumer = "status-output";

    gpio output(node);
    output.set(true);
    auto current = output.get(); // libgs::sys_expected<bool>
    output.invert(); // Invert the current logical level: true -> false.
}
```

`invert()` applies only to output lines: it reads the current logical level and writes the opposite value. With `active_low`, it still inverts the logical level visible to the caller. Calling it on an input line returns `operation_not_permitted`.

`gpio_group` wraps one chip. Specify the chip once in the constructor or `open()`; line configurations do not repeat it. Lines and non-empty aliases must be unique within the group. Access an individual `gpio` by line or alias, operate on the entire group, or submit a map of lines or aliases to values:

```cpp
#include <libempp/linux/subsys/gpio_group.h>

using gpio_group = libempp::subsys::gpio_group;
using direction = gpio_group::gpio_t::direction_t;

gpio_group outputs("gpiochip0", {
    {.line = 17, .direction = direction::output,
        .consumer = "panel", .alias = "red"},
    {.line = 18, .direction = direction::output,
        .consumer = "panel", .alias = "green"},
    {.line = 19, .direction = direction::output,
        .consumer = "panel", .alias = "blue"}
});

outputs.set(17, true);                   // Select by line.
outputs.set("green", true);              // Select by alias.
outputs["blue"].invert();                // Access one gpio and its complete API.
outputs.rising({17, 18});                // Drive selected lines high.
outputs.falling({"red", "blue"});       // Drive selected aliases low.

outputs.set(gpio_group::line_values_t {
    {17, false}, {18, true}
});
outputs.set(gpio_group::alias_values_t {
    {"red", false}, {"green", true}, {"blue", false}
});
outputs.set(false);                      // Drive the whole group low.
auto current = outputs.get();            // sys_expected<line_values_t>
auto chip = outputs.chip();              // gpiochip0

outputs.close("blue");                  // Release and remove only blue.
outputs.open("gpiochip0", {              // Add one line to the existing group.
    .line = 19, .direction = direction::output,
    .consumer = "panel", .alias = "blue"
});
```

`gpio::get()` and `gpio_group::get(line/alias)` return `libgs::sys_expected<bool>` and report read failures through its `std::error_code`. They have no `std::error_code&` overload and do not throw. The no-argument `gpio_group::get()` returns the current group values as `libgs::sys_expected<line_values_t>`. `rising()` and `falling()` are convenience operations that drive output lines high or low, with single-item, list, whole-group, and whole-manager overloads. Explicit value conversion and `operator*()` read the current value and throw `std::system_error` on failure. `operator~()` returns an element-wise inverted snapshot without writing hardware.

Before writing, batch `set()`, `rising()`, and `falling()` verify that every key exists and every target is an open output line. Whole-group operations and `invert()` also validate the complete group first. Actual writes then proceed in group order; hardware atomicity across lines is not guaranteed, and completed writes are not rolled back if a later driver error occurs. If any line in a batch `open()` fails, the group remains closed with no partially opened members. A `std::error_code&` overload called with an unknown line or alias returns `no_such_device_or_address`; `at()` and `operator[]` throw `std::out_of_range`.

The `gpio_group::open()` overload that accepts one `line_config_t` is incremental and does not close existing lines. The target chip must match the current group, and neither the line nor a non-empty alias may be duplicated. `close(line/alias)` releases and removes only the selected line and is idempotent for a missing key. Removing the last line also clears `chip()`. The existing `open()` overload that accepts a line list still replaces the entire group and leaves it closed if any line fails to open.

Use `gpio_manager` for configurations across chips. It creates a `gpio_group` for each chip, indexes lines by `(chip, line)` or a globally unique alias, and supports selected batch operations and whole-subsystem operations. A bare chip name such as `gpiochip0` and its `/dev/gpiochip0` path refer to the same device:

```cpp
#include <libempp/linux/subsys/gpio_manager.h>

using gpio_manager = libempp::subsys::gpio_manager;
using direction = gpio_manager::gpio_t::direction_t;
using index = gpio_manager::index_t;

gpio_manager gpios {
    {.chip = "gpiochip0", .line = 17, .direction = direction::output,
        .consumer = "panel", .alias = "status"},
    {.chip = "gpiochip1", .line = 3, .direction = direction::output,
        .consumer = "power", .alias = "enable"}
};

gpios.set(index {"gpiochip0", 17}, true);
gpios.set("enable", true);
gpios.set(gpio_manager::index_values_t {
    {{"gpiochip0", 17}, false},
    {{"gpiochip1", 3}, true}
});
gpios.group("gpiochip0").invert();       // Operate on one chip.
gpios.set(false);                        // Operate on every chip.
auto current = gpios.get();              // sys_expected<index_values_t>

gpio_manager::node_t alarm {
    .chip = "gpiochip1", .line = 4, .direction = direction::output,
    .consumer = "power", .alias = "alarm"
};
gpios.open(alarm);                        // Add one node incrementally.
gpios.close(index {"gpiochip1", 4});     // Release and remove by index.
```

Use `chips()`, `group_count()`, and `group(chip)` to inspect or access managed chips. `nodes()` and `size()` describe lines across the subsystem. If a manager batch open fails, it remains closed with no partially requested groups. The `open()` overload that accepts one `node_t` retains existing nodes and adds the new one. A duplicate index or globally duplicated non-empty alias returns `invalid_argument`. `close(index/alias)` removes only the target node and succeeds for an absent key. When the last node on a chip is removed, the empty `gpio_group` is removed from the manager. The `open()` overload that accepts a node list still replaces the complete configuration.

Input lines can detect rising, falling, or both edges:

```cpp
gpio::node_t button;
button.chip = "/dev/gpiochip0";
button.line = 22;
button.direction = gpio::direction_t::input;
button.active_low = true;
button.edge = gpio::edge_t::both;

asio::io_context context;
using async_gpio = libempp::subsys::basic_gpio<asio::io_context::executor_type>;
async_gpio input(button, context);
gpio::event_t filter {.edge = gpio::edge_t::both};
input.on_event(filter, [](gpio::event_t event) {
    std::cout << (event.edge == gpio::edge_t::rising ? "pressed" : "released");
});

context.run();
```

After `on_event()` is registered, it continuously drains the kernel event queue and submits each matching edge in arrival order to the object's executor. Pass `edge_t::both` to receive both edge types, or pass an empty callback to remove all callbacks for that type. The application must keep driving the execution context passed to `basic_gpio`. `gpio` is a convenience alias for `basic_gpio<asio::any_io_executor>`; its no-argument executor constructor binds the default LibGS `io_context`, which can be driven with `libgs::exec()`.

`wait_event()` is a one-shot wait using a LibGS completion token. It blocks synchronously by default, or accepts a callback, `use_future`, `use_awaitable`, or `token | timeout` for an asynchronous timeout. Events that occurred before the wait began are not guaranteed to be retained. If `on_event()` is also registered, a one-shot wait does not steal events from the callback channel.

The sysfs fallback reads `/sys/class/gpio/<chip>/base` to convert the chip-relative offset to a global GPIO number. It unexports a line on close only if the object exported that line. The sysfs ABI does not provide libgpiod's kernel-enforced exclusive line request, so processes must still avoid operating on the same line. sysfs edge timestamps use the monotonic clock time at which user space receives the event, while the libgpiod backend returns the kernel event timestamp.

Use `gpio::backend()` or `gpio::backend_name()` to inspect the backend selected for the current build.

## PWM

`libempp::subsys::pwm` prefers the Linux PWM character-device waveform UAPI and falls back to sysfs when the build environment lacks the newer UAPI. A logical chip name such as `pwmchip0` works with both backends: the character-device backend resolves it to `/dev/pwmchip0`, and the sysfs backend resolves it to `/sys/class/pwm/pwmchip0`. Period and duty cycle are measured in nanoseconds.

```cpp
#include <libempp/linux/subsys/pwm.h>

int main()
{
    libempp::subsys::pwm output({"pwmchip0", 0});
    output.set(20'000'000, 1'500'000).enable();

    // Run application logic.

    output.disable();
}
```

The character-device backend acquires the channel exclusively with `PWM_IOCTL_REQUEST` and uses waveform ioctls to read, round, and apply timing. Closing or destroying the object releases the request. The sysfs backend attempts to export the channel and acquires non-blocking exclusive locks on `period`, `duty_cycle`, and `enable`. It unexports the channel on close only if the object exported it. Multiple processes must not control the same channel concurrently.

Both backends reject `duty_cycle > period`. When output is enabled, the character-device backend uses the driver-rounded waveform. A sysfs driver may require the channel to be disabled before changing its period. Use `pwm::backend()` or `pwm::backend_name()` to inspect the selected backend.

## State retention and troubleshooting

- Hardware state written through backlight and LED objects remains after destruction. The caller must restore state explicitly when required.
- GPIO/PWM objects release resources they requested or exported, but multiple processes must still avoid competing for the same resource.
- When opening fails, first log `backend_name()`, then inspect the corresponding `/dev/*chip*` or `/sys/class/*` path, permissions, numbering, and owning process.
- When a setter fails, check the valid range, current enabled state, driver capabilities, and kernel log.

Complete programs are available in [`examples/linux/backlight.cpp`](../../examples/linux/backlight.cpp), [`examples/linux/led.cpp`](../../examples/linux/led.cpp), [`examples/linux/gpio.cpp`](../../examples/linux/gpio.cpp), and [`examples/linux/pwm.cpp`](../../examples/linux/pwm.cpp).
