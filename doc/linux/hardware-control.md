# Hardware control

[Linux module](../linux.md) · [Execution model](../io-model.md)

These interfaces are in `altun::subsys` and link with `altun.linux`.

| Interface | Resource | Backend |
| --- | --- | --- |
| `backlight` | `/sys/class/backlight/<name>` | sysfs |
| `led` | `/sys/class/leds/<name>` | sysfs |
| `gpio`, `gpio_group`, `gpio_manager` | GPIO chips and lines | libgpiod or sysfs |
| `pwm` | PWM chip and channel | waveform character device or sysfs |

GPIO and PWM backends are selected at build time; see [Backend selection](../linux.md#backend-selection).

## Backlight and LED

```cpp
#include <altun/linux/subsys/backlight.h>
#include <altun/linux/subsys/led.h>

altun::subsys::backlight display({"intel_backlight"});
display.set_brightness(500).enable();

altun::subsys::led status({"status:red:system"});
status.set_brightness(status.max_brightness());
if(status.supports_triggers())
    status.set_trigger("heartbeat");
```

Backlight brightness cannot exceed `max_brightness()`. LED triggers must appear in `triggers()`; `set_blink()` selects the timer trigger and sets its intervals. Destruction releases files but does not restore brightness, power, or trigger state.

## GPIO

One `gpio` object requests one chip-relative line:

```cpp
#include <altun/linux/subsys/gpio.h>

using gpio = altun::subsys::gpio;
gpio::node_t node;
node.chip = "gpiochip0";
node.line = 17;
node.direction = gpio::direction_t::output;
node.initial_value = false;
node.consumer = "status";

gpio output(node);
output.set(true);
auto value = output.get(); // riwo::sys_expected<bool>
```

`gpio_group` manages several lines on one chip; `gpio_manager` manages groups across chips. Both support lookup by line/index or unique alias and provide batch `set()`, `rising()`, `falling()`, `invert()`, and `get()` operations.

```cpp
#include <altun/linux/subsys/gpio_manager.h>

using manager = altun::subsys::gpio_manager;
using direction = manager::gpio_t::direction_t;

manager gpios {
    {.chip = "gpiochip0", .line = 17,
     .direction = direction::output, .alias = "status"},
    {.chip = "gpiochip1", .line = 3,
     .direction = direction::output, .alias = "enable"}
};
gpios.set("enable", true);
```

Batch calls validate their targets before writing, but writes across several lines are not hardware-atomic and are not rolled back after a later driver failure.

Input lines support one-shot `wait_event()` and persistent `on_event()` callbacks:

```cpp
gpio::node_t button;
button.chip = "gpiochip0";
button.line = 22;
button.direction = gpio::direction_t::input;
button.edge = gpio::edge_t::both;

gpio input(button);
input.on_event({.edge = gpio::edge_t::both}, [](gpio::event_t event) {
    std::cout << event.timestamp_ns << '\n';
});
riwo::exec();
```

The application must keep the executor running. Inspect the compiled backend with `gpio::backend_name()`.

## PWM

Period and duty cycle are nanoseconds, and duty cycle must not exceed period.

```cpp
#include <altun/linux/subsys/pwm.h>

altun::subsys::pwm output({"pwmchip0", 0});
output.set(20'000'000, 1'500'000).enable();
// ...
output.disable();
```

The character-device backend requests a channel exclusively. The sysfs backend exports the channel when needed and locks its control files. Destruction releases library-owned resources; explicitly disable output when the application requires a known final state.

Before writing, verify chip and line/channel numbering, permissions, valid ranges, electrical requirements, and that no other process owns the resource. Runnable programs are listed in the [examples guide](../../examples/README.md#linux).
