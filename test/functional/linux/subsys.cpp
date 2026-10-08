// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "../../test.h"
#include "../../support/linux/bus.h"
#include "../../support/linux/gpio.h"

#include <libempp/linux/subsys/backlight.h>
#include <libempp/linux/subsys/gpio.h>
#include <libempp/linux/subsys/gpio_group.h>
#include <libempp/linux/subsys/gpio_manager.h>
#include <libempp/linux/subsys/led.h>
#include <libempp/linux/subsys/pwm.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <concepts>
#include <cstddef>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace
{

class context_runner
{
public:
	explicit context_runner(asio::io_context &context) :
		m_context(context),
		m_guard(asio::make_work_guard(m_context))
	{
		m_context.restart();
		m_thread = std::thread([this] {
			m_context.run();
		});
	}

	~context_runner()
	{
		m_guard.reset();
		m_context.stop();
		if(m_thread.joinable())
			m_thread.join();
		m_context.restart();
	}

private:
	asio::io_context &m_context;
	asio::executor_work_guard<asio::io_context::executor_type> m_guard;
	std::thread m_thread;
};

class temporary_file
{
public:
	temporary_file()
	{
		std::array<char, 32> pattern {};
		const std::string value = "/tmp/libempp-i2c-XXXXXX";
		std::ranges::copy(value, pattern.begin());
		const int descriptor = ::mkstemp(pattern.data());
		if(descriptor < 0)
			empp_test::fail("mkstemp failed");
		::close(descriptor);
		m_path = pattern.data();
	}

	~temporary_file()
	{
		std::error_code ignored;
		std::filesystem::remove(m_path, ignored);
	}

	[[nodiscard]] const std::filesystem::path &path() const noexcept
	{
		return m_path;
	}

private:
	std::filesystem::path m_path;
};

} // namespace

EMPP_TEST("virtual-device", "LED closed and missing-device paths need no hardware")
{
	using led = libempp::subsys::led;
	led output;
	EMPP_REQUIRE(not output.is_open());
	EMPP_REQUIRE(not output.supports_triggers());
	EMPP_REQUIRE_EQ(output.brightness(), 0U);
	EMPP_REQUIRE_EQ(output.max_brightness(), 0U);

	const auto bad_descriptor = std::make_error_code(std::errc::bad_file_descriptor);
	std::error_code error;
	output.set_brightness(1, error);
	EMPP_REQUIRE_EQ(error, bad_descriptor);
	output.set_trigger("timer", error);
	EMPP_REQUIRE_EQ(error, bad_descriptor);
	output.set_blink(std::chrono::milliseconds(10),
		std::chrono::milliseconds(10), error);
	EMPP_REQUIRE_EQ(error, bad_descriptor);
	EMPP_REQUIRE(output.trigger(error).empty());
	EMPP_REQUIRE_EQ(error, bad_descriptor);
	EMPP_REQUIRE(output.triggers(error).empty());
	EMPP_REQUIRE_EQ(error, bad_descriptor);

	output.open({"libempp-test-led-that-does-not-exist"}, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::no_such_device));
	EMPP_REQUIRE(not output.is_open());
	output.open({"../invalid"}, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));

	led moved(std::move(output));
	EMPP_REQUIRE(not moved.is_open());
	EMPP_REQUIRE(not output.is_open());
	led assigned;
	assigned = std::move(moved);
	EMPP_REQUIRE(not assigned.is_open());
	EMPP_REQUIRE(not moved.is_open());
}

EMPP_TEST("virtual-device", "GPIO validation and closed operations need no hardware")
{
	using gpio = libempp::subsys::gpio;
	gpio line;
	static_assert(std::same_as<decltype(line.get()),riwo::sys_expected<bool>>);
	static_assert(noexcept(line.get()));
	EMPP_REQUIRE(not line.is_open());
	EMPP_REQUIRE(std::string_view(gpio::backend_name()) ==
		(gpio::backend() == gpio::backend_t::libgpiod ? "libgpiod" : "sysfs"));

	const auto bad_descriptor = std::make_error_code(std::errc::bad_file_descriptor);
	std::error_code error;
	line.set(true, error);
	EMPP_REQUIRE_EQ(error, bad_descriptor);
	line.rising(error);
	EMPP_REQUIRE_EQ(error, bad_descriptor);
	line.falling(error);
	EMPP_REQUIRE_EQ(error, bad_descriptor);
	line.invert(error);
	EMPP_REQUIRE_EQ(error, bad_descriptor);
	const auto line_value = line.get();
	EMPP_REQUIRE(not line_value);
	EMPP_REQUIRE_EQ(line_value.error(), bad_descriptor);
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor, (void)static_cast<bool>(line));
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor, (void)*line);
	gpio::event_t event;
	line.wait_event(event, error);
	EMPP_REQUIRE_EQ(error, bad_descriptor);
	EMPP_REQUIRE_EQ(event.edge, gpio::edge_t::none);
	EMPP_REQUIRE_EQ(event.timestamp_ns, 0U);
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor, line.wait_event(event));
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor,
		line.on_event({.edge = gpio::edge_t::both}, [](gpio::event_t) {}));

	gpio::node_t invalid;
	invalid.chip.clear();
	line.open(invalid, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));

	invalid.chip = "/dev/gpiochip0";
	invalid.direction = gpio::direction_t::output;
	invalid.edge = gpio::edge_t::rising;
	line.open(invalid, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));

	gpio::node_t missing;
	missing.chip = "libempp-test-gpiochip-that-does-not-exist";
	line.open(missing, error);
	EMPP_REQUIRE(static_cast<bool>(error));
	EMPP_REQUIRE(not line.is_open());

	gpio moved(std::move(line));
	EMPP_REQUIRE(not moved.is_open());
	EMPP_REQUIRE(not line.is_open());
	gpio assigned;
	assigned = std::move(moved);
	EMPP_REQUIRE(not assigned.is_open());
	EMPP_REQUIRE(not moved.is_open());

	libempp::subsys::gpio_group group;
	group.set(true, error);
	EMPP_REQUIRE_EQ(error, bad_descriptor);
	group.rising(error);
	EMPP_REQUIRE_EQ(error, bad_descriptor);
	group.falling(error);
	EMPP_REQUIRE_EQ(error, bad_descriptor);
	const auto group_values = group.get();
	EMPP_REQUIRE(not group_values);
	EMPP_REQUIRE_EQ(group_values.error(), bad_descriptor);
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor,
		(void)static_cast<libempp::subsys::gpio_group::line_values_t>(group));
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor, (void)*group);
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor, (void)~group);
	group.open({}, {}, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
	libempp::subsys::gpio_group::line_config_t invalid_line;
	group.open({}, invalid_line, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));

	using gpio_manager = libempp::subsys::gpio_manager;
	const gpio_manager::index_t bare_index {"gpiochip0", 7};
	const gpio_manager::index_t device_index {"/dev/gpiochip0", 7};
	const gpio_manager::index_t next_index {"gpiochip0", 8};
	EMPP_REQUIRE(bare_index == device_index);
	EMPP_REQUIRE((bare_index <=> next_index) == std::strong_ordering::less);

	gpio_manager manager;
	manager.set(true, error);
	EMPP_REQUIRE_EQ(error, bad_descriptor);
	manager.rising(error);
	EMPP_REQUIRE_EQ(error, bad_descriptor);
	manager.falling(error);
	EMPP_REQUIRE_EQ(error, bad_descriptor);
	const auto manager_values = manager.get();
	EMPP_REQUIRE(not manager_values);
	EMPP_REQUIRE_EQ(manager_values.error(), bad_descriptor);
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor,
		(void)static_cast<gpio_manager::index_values_t>(manager));
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor, (void)*manager);
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor, (void)~manager);
	manager.open({}, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
	gpio_manager::node_t invalid_node;
	invalid_node.chip.clear();
	manager.open(invalid_node, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
}

EMPP_TEST("virtual-device", "GPIO group wraps one chip with line alias and batch operations")
{
	using gpio_group = libempp::subsys::gpio_group;
	if(not empp_test_support::virtual_gpio_available() or
		gpio_group::gpio_t::backend() != gpio_group::gpio_t::backend_t::libgpiod)
		return;

	empp_test_support::reset_virtual_gpio();
	auto output_config = [](gpio_group::line_t line, std::string alias,
		bool initial_value = false)
	{
		gpio_group::line_config_t config;
		config.line = line;
		config.direction = gpio_group::gpio_t::direction_t::output;
		config.initial_value = initial_value;
		config.consumer = "libempp-group-test";
		config.alias = std::move(alias);
		return config;
	};

	const std::filesystem::path chip = "libempp-test-virtual-gpiochip";
	const auto red = output_config(7, "red");
	const auto green = output_config(8, "green", true);
	const auto blue = output_config(9, "blue");
	gpio_group outputs(chip, {red, green, blue});
	static_assert(std::same_as <
		decltype(outputs.get(gpio_group::line_t {})),riwo::sys_expected<bool>
	>);
	static_assert(noexcept(outputs.get(gpio_group::line_t {})));
	static_assert(std::same_as <
		decltype(outputs.get()),riwo::sys_expected<gpio_group::line_values_t>
	>);
	static_assert(noexcept(outputs.get()));
	const std::string red_alias = "red";
	const std::string_view green_alias = "green";
	char blue_alias[] = "blue";
	std::error_code error;
	EMPP_REQUIRE(outputs.is_open());
	EMPP_REQUIRE_EQ(outputs.size(), 3U);
	EMPP_REQUIRE_EQ(outputs.chip(), chip);
	EMPP_REQUIRE_EQ(outputs.lines().size(), 3U);
	EMPP_REQUIRE(outputs.contains(7));
	EMPP_REQUIRE(outputs.contains(red_alias));
	EMPP_REQUIRE(outputs.contains(green_alias));
	EMPP_REQUIRE(outputs.contains(blue_alias));
	EMPP_REQUIRE(not outputs.contains('r'));
	EMPP_REQUIRE(not outputs.contains("missing"));
	EMPP_REQUIRE(&outputs.at(8) == &outputs[green_alias]);
	EMPP_REQUIRE(outputs.get(green_alias).value_or(false));
	static_assert(noexcept(outputs.close(gpio_group::line_t {})));
	static_assert(noexcept(outputs.close(std::string_view {})));

	outputs.close(7).close(green_alias).close(blue_alias);
	EMPP_REQUIRE(outputs.empty());
	EMPP_REQUIRE(not outputs.is_open());
	EMPP_REQUIRE(outputs.chip().empty());
	outputs.close(42).close("missing");
	EMPP_REQUIRE(outputs.empty());

	outputs.open(chip, red, error);
	EMPP_REQUIRE(not error);
	outputs.open(chip, green, error);
	EMPP_REQUIRE(not error);
	outputs.open(chip, blue, error);
	EMPP_REQUIRE(not error);
	EMPP_REQUIRE(outputs.is_open());
	EMPP_REQUIRE_EQ(outputs.size(), 3U);

	outputs.open(chip, red, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
	EMPP_REQUIRE_EQ(outputs.size(), 3U);
	auto duplicate_alias = output_config(10, "red");
	outputs.open(chip, duplicate_alias, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
	EMPP_REQUIRE_EQ(outputs.size(), 3U);
	outputs.open("libempp-test-virtual-gpiochip-2",
		output_config(10, "other-chip"), error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
	EMPP_REQUIRE_EQ(outputs.size(), 3U);
	outputs.open(chip, output_config(99, "missing-line"), error);
	EMPP_REQUIRE(static_cast<bool>(error));
	EMPP_REQUIRE(outputs.is_open());
	EMPP_REQUIRE_EQ(outputs.size(), 3U);

	outputs[7].rising(error);
	EMPP_REQUIRE(not error);
	EMPP_REQUIRE(static_cast<bool>(outputs[7]));
	outputs[7].falling();
	EMPP_REQUIRE(not *outputs[7]);

	outputs.falling(error);
	EMPP_REQUIRE(not error);
	outputs.rising(7, error).falling(7);
	EMPP_REQUIRE(not error);
	outputs.rising(8).falling(8, error);
	EMPP_REQUIRE(not error);
	outputs.rising(green_alias, error).falling(green_alias);
	EMPP_REQUIRE(not error);
	outputs.rising(gpio_group::lines_t {7, 9}, error);
	EMPP_REQUIRE(not error);
	outputs.falling(gpio_group::lines_t {7}).rising(gpio_group::lines_t {8});
	outputs.rising(gpio_group::aliases_t {"red"}, error);
	EMPP_REQUIRE(not error);
	outputs.falling(gpio_group::aliases_t {"red"})
		.rising(gpio_group::aliases_t {"blue"});
	outputs.rising().falling(error);
	EMPP_REQUIRE(not error);
	outputs.rising(error);
	EMPP_REQUIRE(not error);
	outputs.falling(gpio_group::lines_t {9}, error);
	EMPP_REQUIRE(not error);
	outputs.falling(gpio_group::aliases_t {"green"}, error);
	EMPP_REQUIRE(not error);
	outputs.falling().rising("red").falling("red", error);
	EMPP_REQUIRE(not error);

	const auto missing = std::make_error_code(std::errc::no_such_device_or_address);
	outputs.falling();
	outputs.rising(gpio_group::lines_t {7, 42}, error);
	EMPP_REQUIRE_EQ(error, missing);
	EMPP_REQUIRE(not outputs.get(7).value_or(true));
	outputs.rising(gpio_group::aliases_t {"red", "missing"}, error);
	EMPP_REQUIRE_EQ(error, missing);
	EMPP_REQUIRE(not outputs.get(7).value_or(true));

	outputs.set(7, true).set(green_alias, false);
	EMPP_REQUIRE(outputs.get(red_alias).value_or(false));
	EMPP_REQUIRE(not outputs.get(8).value_or(true));

	outputs.set(gpio_group::line_values_t {
		{7, false}, {8, true}, {9, true}
	});
	auto line_values = outputs.get();
	EMPP_REQUIRE(line_values);
	EMPP_REQUIRE(not line_values->at(7));
	EMPP_REQUIRE(line_values->at(8));
	EMPP_REQUIRE(line_values->at(9));

	outputs.set(gpio_group::alias_values_t {
		{"red", true}, {"blue", false}
	});
	EMPP_REQUIRE(outputs.get("red").value_or(false));
	EMPP_REQUIRE(outputs.get("green").value_or(false));
	EMPP_REQUIRE(not outputs.get("blue").value_or(true));
	const auto converted_values = static_cast<gpio_group::line_values_t>(outputs);
	const auto dereferenced_values = *outputs;
	const auto complemented_values = ~outputs;
	EMPP_REQUIRE_EQ(converted_values, dereferenced_values);
	for(const auto &[line, value] : converted_values)
		EMPP_REQUIRE_EQ(complemented_values.at(line), not value);

	outputs.set(false).invert();
	const auto inverted_values = outputs.get();
	EMPP_REQUIRE(inverted_values);
	for(const auto &[line, value] : *inverted_values)
	{
		(void)line;
		EMPP_REQUIRE(value);
	}
	outputs.invert(blue_alias);
	EMPP_REQUIRE(not outputs.get(9).value_or(true));

	outputs.set("missing", true, error);
	EMPP_REQUIRE_EQ(error, missing);
	const auto missing_value = outputs.get(42);
	EMPP_REQUIRE(not missing_value);
	EMPP_REQUIRE_EQ(missing_value.error(), missing);
	EMPP_REQUIRE_THROWS(std::out_of_range, (void)outputs.at("missing"));

	outputs.open(chip, {red, output_config(7, "duplicate-line")}, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
	EMPP_REQUIRE(outputs.empty());
	EMPP_REQUIRE(outputs.chip().empty());
	outputs.open(chip, {red, output_config(8, "red")}, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
	EMPP_REQUIRE(outputs.empty());
	outputs.open({}, {red}, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
	EMPP_REQUIRE(outputs.empty());
	outputs.open(chip, {red, output_config(99, "missing-line")}, error);
	EMPP_REQUIRE(static_cast<bool>(error));
	EMPP_REQUIRE(outputs.empty());

	auto input = output_config(8, "input");
	input.direction = gpio_group::gpio_t::direction_t::input;
	outputs.open(chip, {output_config(7, "output", true), input});
	outputs.set(false, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::operation_not_permitted));
	EMPP_REQUIRE(outputs.get("output").value_or(false));

	gpio_group moved(std::move(outputs));
	EMPP_REQUIRE(moved.is_open());
	EMPP_REQUIRE(outputs.empty());
	gpio_group assigned;
	assigned = std::move(moved);
	EMPP_REQUIRE(assigned.is_open());
	EMPP_REQUIRE(moved.empty());

	empp_test_support::reset_virtual_gpio();
}

EMPP_TEST("virtual-device", "GPIO manager coordinates chips indices aliases and groups")
{
	using gpio_manager = libempp::subsys::gpio_manager;
	if(not empp_test_support::virtual_gpio_available() or
		gpio_manager::gpio_t::backend() != gpio_manager::gpio_t::backend_t::libgpiod)
		return;

	empp_test_support::reset_virtual_gpio();
	auto output_node = [](gpio_manager::line_t line, std::string alias,
		std::filesystem::path chip, bool initial_value = false)
	{
		gpio_manager::node_t node;
		node.chip = std::move(chip);
		node.line = line;
		node.direction = gpio_manager::gpio_t::direction_t::output;
		node.initial_value = initial_value;
		node.consumer = "libempp-manager-test";
		node.alias = std::move(alias);
		return node;
	};

	const auto red = output_node(7, "red", "libempp-test-virtual-gpiochip");
	const auto green = output_node(7, "green",
		"libempp-test-virtual-gpiochip-2", true);
	const auto blue = output_node(9, "blue", "libempp-test-virtual-gpiochip");
	const gpio_manager::index_t red_index {red.chip, red.line};
	const gpio_manager::index_t green_index {green.chip, green.line};
	const gpio_manager::index_t blue_index {blue.chip, blue.line};
	const gpio_manager::index_t red_device_index {
		"/dev/libempp-test-virtual-gpiochip", red.line
	};
	gpio_manager manager {red, green, blue};
	std::error_code error;
	static_assert(std::same_as <
		decltype(manager.get(gpio_manager::index_t {})),riwo::sys_expected<bool>
	>);
	static_assert(noexcept(manager.get(
		std::declval<const gpio_manager::index_t&>())));
	static_assert(std::same_as <
		decltype(manager.get()),riwo::sys_expected<gpio_manager::index_values_t>
	>);
	static_assert(noexcept(manager.get()));
	EMPP_REQUIRE(manager.is_open());
	EMPP_REQUIRE_EQ(manager.size(), 3U);
	EMPP_REQUIRE_EQ(manager.group_count(), 2U);
	EMPP_REQUIRE_EQ(manager.chips().size(), 2U);
	EMPP_REQUIRE(manager.contains_chip("/dev/libempp-test-virtual-gpiochip"));
	EMPP_REQUIRE(manager.contains(red_index));
	EMPP_REQUIRE(manager.contains(green_index));
	EMPP_REQUIRE(red_index == red_device_index);
	EMPP_REQUIRE(manager.contains(red_device_index));
	EMPP_REQUIRE(manager.contains("green"));
	EMPP_REQUIRE(&manager.group(red.chip).at(7) == &manager.at(red_index));
	static_assert(noexcept(manager.close(
		std::declval<const gpio_manager::index_t&>())));
	static_assert(noexcept(manager.close(std::string_view {})));

	manager.close(red_device_index).close("green").close(blue_index);
	EMPP_REQUIRE(manager.empty());
	EMPP_REQUIRE(not manager.is_open());
	EMPP_REQUIRE_EQ(manager.group_count(), 0U);
	EMPP_REQUIRE(manager.chips().empty());
	manager.close(red_index).close("missing");
	EMPP_REQUIRE(manager.empty());

	manager.open(red, error);
	EMPP_REQUIRE(not error);
	manager.open(green, error);
	EMPP_REQUIRE(not error);
	manager.open(blue, error);
	EMPP_REQUIRE(not error);
	EMPP_REQUIRE(manager.is_open());
	EMPP_REQUIRE_EQ(manager.size(), 3U);
	EMPP_REQUIRE_EQ(manager.group_count(), 2U);

	manager.open(red, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
	EMPP_REQUIRE_EQ(manager.size(), 3U);
	auto duplicate_alias_node = output_node(10, "red", green.chip);
	manager.open(duplicate_alias_node, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
	EMPP_REQUIRE_EQ(manager.size(), 3U);
	manager.open(output_node(99, "missing-line", red.chip), error);
	EMPP_REQUIRE(static_cast<bool>(error));
	EMPP_REQUIRE(manager.is_open());
	EMPP_REQUIRE_EQ(manager.size(), 3U);
	EMPP_REQUIRE_EQ(manager.group_count(), 2U);

	manager.falling(error);
	EMPP_REQUIRE(not error);
	manager.rising(red_index, error).falling(red_index);
	EMPP_REQUIRE(not error);
	manager.rising(green_index).falling(green_index, error);
	EMPP_REQUIRE(not error);
	manager.rising("green", error).falling("green");
	EMPP_REQUIRE(not error);
	manager.rising(gpio_manager::indexes_t {red_index, blue_index}, error);
	EMPP_REQUIRE(not error);
	manager.falling(gpio_manager::indexes_t {red_index})
		.rising(gpio_manager::indexes_t {green_index});
	manager.rising(gpio_manager::aliases_t {"red"}, error);
	EMPP_REQUIRE(not error);
	manager.falling(gpio_manager::aliases_t {"red"})
		.rising(gpio_manager::aliases_t {"blue"});
	manager.rising().falling(error);
	EMPP_REQUIRE(not error);
	manager.rising(error);
	EMPP_REQUIRE(not error);
	manager.falling(gpio_manager::indexes_t {blue_index}, error);
	EMPP_REQUIRE(not error);
	manager.falling(gpio_manager::aliases_t {"green"}, error);
	EMPP_REQUIRE(not error);
	manager.falling().rising("red").falling("red", error);
	EMPP_REQUIRE(not error);

	const auto missing = std::make_error_code(std::errc::no_such_device_or_address);
	manager.falling();
	manager.rising(gpio_manager::indexes_t {red_index, {red.chip, 42}}, error);
	EMPP_REQUIRE_EQ(error, missing);
	EMPP_REQUIRE(not manager.get(red_index).value_or(true));
	manager.rising(gpio_manager::aliases_t {"red", "missing"}, error);
	EMPP_REQUIRE_EQ(error, missing);
	EMPP_REQUIRE(not manager.get(red_index).value_or(true));

	manager.set(red_index, true).set("green", false);
	EMPP_REQUIRE(manager.get("red").value_or(false));
	EMPP_REQUIRE(not manager.get(green_index).value_or(true));
	manager.set(gpio_manager::index_values_t {
		{red_index, false}, {green_index, true}, {blue_index, true}
	});
	const auto index_values = manager.get();
	EMPP_REQUIRE(index_values);
	EMPP_REQUIRE(not index_values->at(red_device_index));
	EMPP_REQUIRE(index_values->at(green_index));
	EMPP_REQUIRE(index_values->at(blue_index));
	manager.set(gpio_manager::alias_values_t {
		{"red", true}, {"blue", false}
	});
	EMPP_REQUIRE(manager.get("red").value_or(false));
	EMPP_REQUIRE(manager.get("green").value_or(false));
	EMPP_REQUIRE(not manager.get("blue").value_or(true));
	const auto converted_values = static_cast<gpio_manager::index_values_t>(manager);
	const auto dereferenced_values = *manager;
	const auto complemented_values = ~manager;
	EMPP_REQUIRE_EQ(converted_values, dereferenced_values);
	for(const auto &[index, value] : converted_values)
		EMPP_REQUIRE_EQ(complemented_values.at(index), not value);

	manager.set(false).invert();
	const auto inverted_values = manager.get();
	EMPP_REQUIRE(inverted_values);
	for(const auto &[index, value] : *inverted_values)
	{
		(void)index;
		EMPP_REQUIRE(value);
	}
	manager.invert("blue");
	EMPP_REQUIRE(not manager.get(blue_index).value_or(true));

	manager.set("missing", true, error);
	EMPP_REQUIRE_EQ(error, missing);
	const auto missing_value = manager.get({red.chip, 42});
	EMPP_REQUIRE(not missing_value);
	EMPP_REQUIRE_EQ(missing_value.error(), missing);
	EMPP_REQUIRE_THROWS(std::out_of_range, (void)manager.group("missing-chip"));

	auto duplicate_index = output_node(7, "duplicate-index",
		"/dev/libempp-test-virtual-gpiochip");
	manager.open({red, duplicate_index}, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
	EMPP_REQUIRE(manager.empty());
	manager.open({red, output_node(8, "red",
		"libempp-test-virtual-gpiochip-2")}, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
	EMPP_REQUIRE(manager.empty());

	auto input = output_node(8, "input", "libempp-test-virtual-gpiochip-2");
	input.direction = gpio_manager::gpio_t::direction_t::input;
	manager.open({output_node(7, "output", "libempp-test-virtual-gpiochip", true),
		input});
	manager.set(false, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::operation_not_permitted));
	EMPP_REQUIRE(manager.get("output").value_or(false));

	gpio_manager moved(std::move(manager));
	EMPP_REQUIRE(moved.is_open());
	EMPP_REQUIRE(manager.empty());
	gpio_manager assigned;
	assigned = std::move(moved);
	EMPP_REQUIRE(assigned.is_open());
	EMPP_REQUIRE(moved.empty());

	empp_test_support::reset_virtual_gpio();
}

EMPP_TEST("virtual-device", "GPIO event callbacks preserve every queued edge")
{
	using namespace std::chrono_literals;
	using gpio = libempp::subsys::basic_gpio<asio::io_context::executor_type>;
	if(not empp_test_support::virtual_gpio_available() or
		gpio::backend() != gpio::backend_t::libgpiod)
		return;

	empp_test_support::reset_virtual_gpio();
	gpio::node_t node;
	node.chip = "libempp-test-virtual-gpiochip";
	node.line = 7;
	node.direction = gpio::direction_t::input;
	node.edge = gpio::edge_t::both;

	asio::io_context context;
	gpio input(node, context);
	EMPP_REQUIRE(input.get_executor() == context.get_executor());

	// The synchronous token reads natively and must not require context.run().
	gpio::event_t direct_event;
	auto direct_future = std::async(std::launch::async, [&] {
		std::error_code error;
		input.wait_event(direct_event, error);
		return error;
	});
	empp_test_support::wait_for_virtual_gpio_waiter();
	empp_test_support::push_virtual_gpio_event(false);
	EMPP_REQUIRE(direct_future.wait_for(2s) == std::future_status::ready);
	EMPP_REQUIRE(not direct_future.get());
	EMPP_REQUIRE_EQ(direct_event.edge, gpio::edge_t::falling);

	context_runner runner(context);
	std::mutex events_mutex;
	std::condition_variable events_ready;
	std::vector<gpio::event_t> events;
	const gpio::event_t both_edges {.edge = gpio::edge_t::both};

	input.on_event(both_edges, [&](gpio::event_t event) {
		std::scoped_lock lock(events_mutex);
		events.push_back(event);
		events_ready.notify_all();
	});

	constexpr std::size_t event_count = 64;
	for(std::size_t index = 0; index < event_count; ++index)
		empp_test_support::push_virtual_gpio_event(index % 2 == 0);

	{
		std::unique_lock lock(events_mutex);
		EMPP_REQUIRE(events_ready.wait_for(lock, 2s, [&] {
			return events.size() == event_count;
		}));
		for(std::size_t index = 0; index < events.size(); ++index)
		{
			EMPP_REQUIRE_EQ(events[index].edge, index % 2 == 0 ?
				gpio::edge_t::rising : gpio::edge_t::falling);
			EMPP_REQUIRE(events[index].timestamp_ns != 0);
		}
	}

	gpio::event_t waited_event;
	auto event_future = input.wait_event(waited_event, riwo::use_future);
	empp_test_support::push_virtual_gpio_event(true);
	EMPP_REQUIRE(event_future.wait_for(2s) == std::future_status::ready);
	event_future.get();
	EMPP_REQUIRE_EQ(waited_event.edge, gpio::edge_t::rising);
	{
		std::unique_lock lock(events_mutex);
		EMPP_REQUIRE(events_ready.wait_for(lock, 2s, [&] {
			return events.size() == event_count + 1;
		}));
		EMPP_REQUIRE_EQ(events.back().edge, gpio::edge_t::rising);
	}

	gpio::event_t synchronous_event;
	auto synchronous_future = std::async(std::launch::async, [&] {
		std::error_code error;
		input.wait_event(synchronous_event, error);
		return error;
	});
	empp_test_support::wait_for_virtual_gpio_waiter();
	empp_test_support::push_virtual_gpio_event(false);
	EMPP_REQUIRE(synchronous_future.wait_for(2s) == std::future_status::ready);
	EMPP_REQUIRE(not synchronous_future.get());
	EMPP_REQUIRE_EQ(synchronous_event.edge, gpio::edge_t::falling);
	{
		std::unique_lock lock(events_mutex);
		EMPP_REQUIRE(events_ready.wait_for(lock, 2s, [&] {
			return events.size() == event_count + 2;
		}));
		EMPP_REQUIRE_EQ(events.back().edge, gpio::edge_t::falling);
	}
	input.on_event(both_edges, gpio::on_event_t{});

	using namespace riwo::operators;
	gpio::event_t timed_event;
	auto timed_future = input.wait_event(timed_event, riwo::use_future | 25ms);
	EMPP_REQUIRE_SYSTEM_ERROR(
		asio::error::make_error_code(asio::error::timed_out), timed_future.get());
	EMPP_REQUIRE_EQ(timed_event.edge, gpio::edge_t::none);

	gpio::event_t cancelled_event;
	auto cancelled_future = input.wait_event(cancelled_event, riwo::use_future);
	input.close();
	EMPP_REQUIRE_SYSTEM_ERROR(
		asio::error::make_error_code(asio::error::operation_aborted),
		cancelled_future.get());
	EMPP_REQUIRE_EQ(cancelled_event.edge, gpio::edge_t::none);
	empp_test_support::reset_virtual_gpio();
}

EMPP_TEST("virtual-device", "PWM closed and missing-device paths need no board hardware")
{
	using pwm = libempp::subsys::pwm;
	pwm output;
	EMPP_REQUIRE(not output.is_open());
	EMPP_REQUIRE(std::string_view(pwm::backend_name()) ==
		(pwm::backend() == pwm::backend_t::cdev ? "cdev" : "sysfs"));
	EMPP_REQUIRE(not output.is_enabled());
	EMPP_REQUIRE_EQ(output.period(), 0U);
	EMPP_REQUIRE_EQ(output.duty_cycle(), 0U);

	const auto bad_descriptor = std::make_error_code(std::errc::bad_file_descriptor);
	std::error_code error;
	for(std::size_t iteration = 0; iteration < 16; ++iteration)
	{
		output.set_period(1000, error);
		EMPP_REQUIRE_EQ(error, bad_descriptor);
		output.set_duty_cycle(500, error);
		EMPP_REQUIRE_EQ(error, bad_descriptor);
		output.set(1000, 500, error);
		EMPP_REQUIRE_EQ(error, bad_descriptor);
		output.set_enable(iteration % 2 == 0, error);
		EMPP_REQUIRE_EQ(error, bad_descriptor);
		output.enable(error);
		EMPP_REQUIRE_EQ(error, bad_descriptor);
		output.disable(error);
		EMPP_REQUIRE_EQ(error, bad_descriptor);
		output.close();
	}
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor, output.set_period(1000));
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor, output.set_duty_cycle(500));
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor, output.set(1000, 500));
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor, output.enable());
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor, output.disable());

	pwm::node_t invalid;
	invalid.chip.clear();
	output.open(invalid, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));

	output.open({"libempp-test-pwm-chip-that-does-not-exist", 0}, error);
	EMPP_REQUIRE_EQ(error,
		std::make_error_code(std::errc::no_such_device));
	EMPP_REQUIRE(not output.is_open());

	pwm moved(std::move(output));
	EMPP_REQUIRE(not moved.is_open());
	EMPP_REQUIRE(not output.is_open());
	pwm assigned;
	assigned = std::move(moved);
	auto *assigned_ptr = &assigned;
	assigned = std::move(*assigned_ptr);
	EMPP_REQUIRE(not assigned.is_open());
	EMPP_REQUIRE(not moved.is_open());
}

EMPP_TEST("virtual-device", "PWM character-device backend applies waveforms")
{
	using pwm = libempp::subsys::pwm;
	if(pwm::backend() != pwm::backend_t::cdev)
		return;

	empp_test_support::reset_virtual_pwm_devices();
	empp_test_support::temporary_file chip;
	pwm output({chip.path().string(), 3});
	EMPP_REQUIRE(output.is_open());
	EMPP_REQUIRE(not output.is_enabled());

	std::error_code error;
	output.set(1000, 250, error);
	EMPP_REQUIRE(not error);
	EMPP_REQUIRE_EQ(output.period(), 1000U);
	EMPP_REQUIRE_EQ(output.duty_cycle(), 250U);
	output.enable(error);
	EMPP_REQUIRE(not error);
	EMPP_REQUIRE(output.is_enabled());

	output.set_period(2000, error);
	EMPP_REQUIRE(not error);
	output.set_duty_cycle(500, error);
	EMPP_REQUIRE(not error);
	output.set(3000, 1000, error);
	EMPP_REQUIRE(not error);
	output.set_duty_cycle(3001, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
	output.set(1000, 1001, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));

	output.disable(error);
	EMPP_REQUIRE(not error);
	EMPP_REQUIRE(not output.is_enabled());
	EMPP_REQUIRE_EQ(output.period(), 3000U);
	EMPP_REQUIRE_EQ(output.duty_cycle(), 1000U);
	output.close();
	EMPP_REQUIRE(not output.is_open());

	std::scoped_lock lock(empp_test_support::virtual_ioctl_mutex);
	EMPP_REQUIRE_EQ(empp_test_support::virtual_pwm_devices.size(), 1U);
	const auto &device = empp_test_support::virtual_pwm_devices.begin()->second;
	EMPP_REQUIRE(not device.requested);
	EMPP_REQUIRE_EQ(device.channel, 3U);
	EMPP_REQUIRE_EQ(device.period_ns, 0U);
	EMPP_REQUIRE_EQ(device.duty_cycle_ns, 0U);
	EMPP_REQUIRE_EQ(device.round_count, 4U);
	EMPP_REQUIRE_EQ(device.set_count, 5U);
	EMPP_REQUIRE_EQ(device.free_count, 1U);
}

EMPP_TEST("virtual-device", "backlight closed and missing-device paths need no hardware")
{
	using backlight = libempp::subsys::backlight;
	backlight output;
	EMPP_REQUIRE(not output.is_open());
	EMPP_REQUIRE(not output.is_enabled());
	EMPP_REQUIRE_EQ(output.brightness(), 0U);
	EMPP_REQUIRE_EQ(output.actual_brightness(), 0U);
	EMPP_REQUIRE_EQ(output.max_brightness(), 0U);
	EMPP_REQUIRE_EQ(output.power(), backlight::power_t::power_down);

	const auto bad_descriptor = std::make_error_code(std::errc::bad_file_descriptor);
	std::error_code error;
	for(std::size_t iteration = 0; iteration < 16; ++iteration)
	{
		output.set_brightness(static_cast<backlight::brightness_t>(iteration), error);
		EMPP_REQUIRE_EQ(error, bad_descriptor);
		output.set_power(backlight::power_t::normal, error);
		EMPP_REQUIRE_EQ(error, bad_descriptor);
		output.set_enable(iteration % 2 == 0, error);
		EMPP_REQUIRE_EQ(error, bad_descriptor);
		output.enable(error);
		EMPP_REQUIRE_EQ(error, bad_descriptor);
		output.disable(error);
		EMPP_REQUIRE_EQ(error, bad_descriptor);
		output.close();
	}
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor, output.set_brightness(1));
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor,
		output.set_power(backlight::power_t::normal));
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor, output.enable());
	EMPP_REQUIRE_SYSTEM_ERROR(bad_descriptor, output.disable());

	output.open({"libempp-test-backlight-that-does-not-exist"}, error);
	EMPP_REQUIRE_EQ(error,
		std::make_error_code(std::errc::no_such_device));
	EMPP_REQUIRE(not output.is_open());

	backlight moved(std::move(output));
	EMPP_REQUIRE(not moved.is_open());
	EMPP_REQUIRE(not output.is_open());
	backlight assigned;
	assigned = std::move(moved);
	auto *assigned_ptr = &assigned;
	assigned = std::move(*assigned_ptr);
	EMPP_REQUIRE(not assigned.is_open());
	EMPP_REQUIRE(not moved.is_open());
}
