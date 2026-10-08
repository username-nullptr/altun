// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "../../test.h"

#include <altun/linux/serial_port_binding.h>
#include <altun/linux/udev/enumeration.h>

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

ALTUN_TEST("virtual-device", "subsystem names and invalid udev objects are safe")
{
	using altun::subsys::enumeration;
	const std::array subsystem_names {
		std::pair {enumeration::usb, std::string_view("usb")},
		std::pair {enumeration::tty, std::string_view("tty")},
		std::pair {enumeration::net, std::string_view("net")},
		std::pair {enumeration::block, std::string_view("block")},
		std::pair {enumeration::backlight, std::string_view("backlight")},
		std::pair {enumeration::led, std::string_view("leds")},
		std::pair {enumeration::gpio, std::string_view("gpio")},
		std::pair {enumeration::pwm, std::string_view("pwm")}
	};
	for(const auto &[value, name] : subsystem_names)
	{
		ALTUN_REQUIRE(altun::subsys::check(value, false));
		ALTUN_REQUIRE_EQ(std::string_view(altun::subsys::string(value)), name);
		ALTUN_REQUIRE_EQ(altun::subsys::from_string(name), value);
	}
	ALTUN_REQUIRE(not altun::subsys::check(enumeration::none, false));
	ALTUN_REQUIRE_EQ(altun::subsys::from_string("not-a-subsystem"), enumeration::none);
	ALTUN_REQUIRE_THROWS(std::invalid_argument,
		static_cast<void>(altun::subsys::string(enumeration::none)));
	ALTUN_REQUIRE_EQ(std::string_view(altun::udev::prop_key::dev_type), "DEVTYPE");
	ALTUN_REQUIRE_EQ(std::string_view(altun::udev::prop_key::id_serial_short),
		"ID_SERIAL_SHORT");
	ALTUN_REQUIRE_EQ(std::string_view(altun::udev::prop_key::id_path), "ID_PATH");
	ALTUN_REQUIRE_EQ(std::string_view(altun::udev::prop_key::id_fs_type),
		"ID_FS_TYPE");
	ALTUN_REQUIRE_EQ(std::string_view(altun::udev::prop_key::id_input_keyboard),
		"ID_INPUT_KEYBOARD");
	ALTUN_REQUIRE_EQ(std::string_view(altun::udev::prop_key::interface), "INTERFACE");
	using altun::udev::event_action;
	const std::array action_names {
		std::pair {event_action::add, std::string_view("add")},
		std::pair {event_action::remove, std::string_view("remove")},
		std::pair {event_action::change, std::string_view("change")},
		std::pair {event_action::move, std::string_view("move")},
		std::pair {event_action::online, std::string_view("online")},
		std::pair {event_action::offline, std::string_view("offline")},
		std::pair {event_action::bind, std::string_view("bind")},
		std::pair {event_action::unbind, std::string_view("unbind")}
	};
	for(const auto &[value, name] : action_names)
	{
		ALTUN_REQUIRE_EQ(altun::udev::string(value), name);
		ALTUN_REQUIRE_EQ(altun::udev::event_action_from_string(name), value);
	}
	ALTUN_REQUIRE_EQ(altun::udev::string(event_action::unknown),
		std::string_view("unknown"));
	ALTUN_REQUIRE_EQ(altun::udev::event_action_from_string("not-an-action"),
		event_action::unknown);

	altun::udev::device_event event;
	ALTUN_REQUIRE(not event.is_valid());
	ALTUN_REQUIRE(not event.property("missing"));
	event.action = altun::udev::event_action::add;
	event.sys_path = "/sys/devices/virtual/test";
	event.dev_node = "/dev/test";
	event.properties.emplace(altun::udev::prop_key::id_model, "USB_Serial_Adapter");
	ALTUN_REQUIRE(event.is_valid());
	ALTUN_REQUIRE(event.matches({}));
	ALTUN_REQUIRE_EQ(event.property(altun::udev::prop_key::id_model),
		std::optional<std::string_view>("USB_Serial_Adapter"));
	ALTUN_REQUIRE(event.matches({
		{altun::udev::prop_key::id_model, "USB_Serial*"}
	}));
	ALTUN_REQUIRE(not event.matches({
		{altun::udev::prop_key::id_model, "NVMe*"}
	}));

	using event_type = altun::udev::basic_event<
		enumeration::tty, asio::io_context::executor_type
	>;
	static_assert(not std::is_reference_v<decltype(event_type::received)>);
	static_assert(not std::is_reference_v<decltype(event_type::error)>);

	asio::io_context event_context;
	event_type events(event_context.get_executor());
	bool observed = false;
	bool monitor_failed = false;
	events.received.connect([&observed](const altun::udev::device_event&) {
		observed = true;
	});
	events.error.connect([&monitor_failed](const std::error_code&) {
		monitor_failed = true;
	});

	std::error_code monitor_error;
	events.open(monitor_error);
	if( not monitor_error )
	{
		event_context.poll();
		std::error_code close_error;
		events.close(close_error);
		ALTUN_REQUIRE(not close_error);
		event_context.restart();
		event_context.run();
		ALTUN_REQUIRE(not observed);
		ALTUN_REQUIRE(not monitor_failed);
	}

	using tty_device = altun::udev::enumeration<enumeration::tty>;
	tty_device invalid;
	ALTUN_REQUIRE(not invalid.is_valid());
	ALTUN_REQUIRE(invalid.path().empty());
	ALTUN_REQUIRE(invalid.native() == nullptr);
	ALTUN_REQUIRE(not invalid.property(altun::udev::prop_key::dev_name));

	altun::serial_port_binding::device_t serial_device(invalid);
	ALTUN_REQUIRE(serial_device.port.empty());

	const auto matches = tty_device::list("ALTUN_TEST_IMPOSSIBLE_PROPERTY", "never");
	ALTUN_REQUIRE(matches.empty());

	using net_device = altun::udev::enumeration<enumeration::net>;
	const auto network_devices = net_device::list();
	ALTUN_REQUIRE(not network_devices.empty());
	const auto loopback = std::ranges::find_if(network_devices,
		[](const auto &device) {
			const auto interface = device.property(altun::udev::prop_key::interface);
			return interface and interface->to_string() == "lo";
		});
	ALTUN_REQUIRE(loopback != network_devices.end());
	net_device copied_loopback(*loopback);
	ALTUN_REQUIRE(copied_loopback.is_valid());
	ALTUN_REQUIRE_EQ(copied_loopback.path(), loopback->path());

	net_device assigned_loopback;
	assigned_loopback = copied_loopback;
	ALTUN_REQUIRE(assigned_loopback.is_valid());
	ALTUN_REQUIRE_EQ(assigned_loopback.path(), loopback->path());

	net_device moved_loopback(std::move(copied_loopback));
	ALTUN_REQUIRE(moved_loopback.is_valid());
	ALTUN_REQUIRE(not copied_loopback.is_valid());

	using backlight_device = altun::udev::enumeration<enumeration::backlight>;
	const auto backlights = backlight_device::list(
		"ALTUN_TEST_IMPOSSIBLE_PROPERTY", "never"
	);
	ALTUN_REQUIRE(backlights.empty());

	// A property rule owns an asynchronous udev monitor. Destroying the rule
	// must cancel the pending descriptor wait so the executor can drain.
	using binding_type = altun::basic_serial_port_binding<asio::io_context::executor_type>;
	asio::io_context monitor_context;
	binding_type binding(monitor_context.get_executor());
	auto rule = binding.make_rule("ALTUN_TEST_IMPOSSIBLE_PROPERTY", "never");
	rule->open();
	monitor_context.poll();
	rule.reset();
	monitor_context.restart();
	monitor_context.run();
}
