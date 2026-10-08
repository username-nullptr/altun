// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "../../test.h"

#include <altun/linux/bus/i2c.h>
#include <altun/linux/bus/spi.h>
#include <altun/linux/serial_port_binding.h>
#include <altun/linux/subsys/gpio.h>
#include <altun/linux/subsys/gpio_group.h>
#include <altun/linux/subsys/gpio_manager.h>
#include <altun/linux/udev/event.h>

#include <array>
#include <concepts>
#include <cstdint>
#include <system_error>
#include <utility>

namespace
{

template <typename T>
concept canonical_executor_type = requires {
	typename T::executor_type;
	typename T::executor_t;
	{ std::declval<T&>().get_executor() } -> std::same_as<typename T::executor_type>;
} and std::same_as<typename T::executor_type,typename T::executor_t>;

using serial_binding = altun::basic_serial_port_binding<>;
using udev_event = altun::udev::event<altun::subsys::enumeration::tty>;

template <typename Error>
concept relaxed_error_code_interfaces = requires (
	altun::bus::i2c &i2c,
	const altun::bus::i2c::node &i2c_node,
	altun::bus::spi &spi,
	const altun::bus::spi::node &spi_node,
	altun::subsys::gpio &gpio,
	const altun::subsys::gpio::node_t &gpio_node,
	altun::subsys::gpio::event_t &gpio_event,
	udev_event &events,
	serial_binding::rule_context &serial_rule,
	serial_binding::io_context &serial_io,
	riwo::const_buffer buffer,
	Error &error
) {
	i2c.open(i2c_node, error);
	i2c.close(error);
	spi.open(spi_node, error);
	spi.close(error);
	gpio.open(gpio_node, error);
	gpio.set(false, error);
	gpio.rising(error);
	gpio.falling(error);
	gpio.invert(error);
	gpio.wait_event(gpio_event, error);
	events.open(error);
	events.close(error);
	serial_rule.write(buffer, error);
	serial_io.write(buffer, error);
};

static_assert(canonical_executor_type<altun::bus::i2c>);
static_assert(canonical_executor_type<altun::bus::spi>);
static_assert(canonical_executor_type<altun::subsys::gpio>);
static_assert(canonical_executor_type<altun::subsys::gpio_group>);
static_assert(canonical_executor_type<altun::subsys::gpio_manager>);
static_assert(canonical_executor_type<altun::udev::event<
	altun::subsys::enumeration::tty>>);
static_assert(canonical_executor_type<serial_binding>);
static_assert(canonical_executor_type<serial_binding::rule_context>);
static_assert(canonical_executor_type<serial_binding::io_context>);
static_assert(relaxed_error_code_interfaces<std::error_code>);
static_assert(relaxed_error_code_interfaces<riwo::error_code>);

template <typename Error>
void exercise_error_code_interfaces()
{
	asio::io_context context;
	using i2c_t = altun::bus::basic_i2c<asio::io_context::executor_type>;
	using spi_t = altun::bus::basic_spi<asio::io_context::executor_type>;
	i2c_t i2c(context.get_executor());
	spi_t spi(context.get_executor());
	altun::subsys::basic_gpio<asio::io_context::executor_type> gpio(context.get_executor());
	altun::udev::basic_event<altun::subsys::enumeration::tty,
		asio::io_context::executor_type> events(context.get_executor());
	const i2c_t::node i2c_node("/dev/null/altun-i2c", 0x20);
	const spi_t::node spi_node("/dev/null/altun-spi");
	std::array<std::uint8_t,1> buffer {};
	altun::subsys::gpio::event_t event;

	Error error;
	i2c.close(error);
	ALTUN_REQUIRE(not error);
	spi.close(error);
	ALTUN_REQUIRE(not error);

	i2c.open(i2c_node, error);
	ALTUN_REQUIRE(error);
	spi.open(spi_node, error);
	ALTUN_REQUIRE(error);
	ALTUN_REQUIRE_EQ(i2c.read(0, riwo::buffer(buffer), error), 0U);
	ALTUN_REQUIRE(error);
	ALTUN_REQUIRE_EQ(spi.read(riwo::buffer(buffer), error), 0U);
	ALTUN_REQUIRE(error);
	gpio.set(false, error);
	ALTUN_REQUIRE(error);
	gpio.wait_event(event, error);
	ALTUN_REQUIRE(error);
	events.close(error);
	ALTUN_REQUIRE(not error);
}

ALTUN_TEST("io-model", "public interfaces accept standard and provider error codes")
{
	exercise_error_code_interfaces<std::error_code>();
	exercise_error_code_interfaces<riwo::error_code>();
}

ALTUN_TEST("io-model", "bus operations use the completion token executor")
{
	using executor_t = asio::io_context::executor_type;
	using i2c_t = altun::bus::basic_i2c<executor_t>;
	using spi_t = altun::bus::basic_spi<executor_t>;

	asio::io_context io_context;
	asio::io_context completion_context;
	i2c_t i2c(io_context.get_executor());
	spi_t spi(io_context.get_executor());

	std::array<std::uint8_t,1> i2c_buffer {};
	std::array<std::uint8_t,1> spi_buffer {};
	bool i2c_completed = false;
	bool spi_completed = false;
	bool i2c_array_completed = false;
	bool spi_array_completed = false;

	i2c.read(0, riwo::buffer(i2c_buffer),
		asio::bind_executor(completion_context.get_executor(),
		[&](riwo::error_code error, std::size_t transferred)
		{
			ALTUN_REQUIRE_EQ(error,
				riwo::make_system_error_code(std::errc::bad_file_descriptor));
			ALTUN_REQUIRE_EQ(transferred, 0U);
			i2c_completed = true;
		}));
	spi.read(riwo::buffer(spi_buffer),
		asio::bind_executor(completion_context.get_executor(),
		[&](riwo::error_code error, std::size_t transferred)
		{
			ALTUN_REQUIRE_EQ(error,
				riwo::make_system_error_code(std::errc::bad_file_descriptor));
			ALTUN_REQUIRE_EQ(transferred, 0U);
			spi_completed = true;
		}));
	i2c.read<std::array<std::uint8_t,1>>(0,
		asio::bind_executor(completion_context.get_executor(),
		[&](riwo::error_code error, std::array<std::uint8_t,1> value)
		{
			ALTUN_REQUIRE_EQ(error,
				riwo::make_system_error_code(std::errc::bad_file_descriptor));
			ALTUN_REQUIRE_EQ(value, (std::array<std::uint8_t,1> {}));
			i2c_array_completed = true;
		}));
	spi.read<std::array<std::uint8_t,1>>(
		asio::bind_executor(completion_context.get_executor(),
		[&](riwo::error_code error, std::array<std::uint8_t,1> value)
		{
			ALTUN_REQUIRE_EQ(error,
				riwo::make_system_error_code(std::errc::bad_file_descriptor));
			ALTUN_REQUIRE_EQ(value, (std::array<std::uint8_t,1> {}));
			spi_array_completed = true;
		}));

	ALTUN_REQUIRE(not i2c_completed);
	ALTUN_REQUIRE(not spi_completed);
	ALTUN_REQUIRE(not i2c_array_completed);
	ALTUN_REQUIRE(not spi_array_completed);
	io_context.run();
	ALTUN_REQUIRE(not i2c_completed);
	ALTUN_REQUIRE(not spi_completed);
	ALTUN_REQUIRE(not i2c_array_completed);
	ALTUN_REQUIRE(not spi_array_completed);

	completion_context.run();
	ALTUN_REQUIRE(i2c_completed);
	ALTUN_REQUIRE(spi_completed);
	ALTUN_REQUIRE(i2c_array_completed);
	ALTUN_REQUIRE(spi_array_completed);
}

ALTUN_TEST("io-model", "GPIO immediate failures are never invoked inline")
{
	using gpio_t = altun::subsys::basic_gpio<asio::io_context::executor_type>;
	asio::io_context io_context;
	asio::io_context completion_context;
	gpio_t gpio(io_context.get_executor());
	gpio_t::event_t event;

	bool initiating = true;
	bool completed = false;
	gpio.wait_event(event,
		asio::bind_immediate_executor(completion_context.get_executor(),
		[&](riwo::error_code error)
		{
			ALTUN_REQUIRE(not initiating);
			ALTUN_REQUIRE_EQ(error,
				riwo::make_system_error_code(std::errc::bad_file_descriptor));
			completed = true;
		}));
	initiating = false;

	ALTUN_REQUIRE(not completed);
	ALTUN_REQUIRE_EQ(io_context.poll(), 0U);
	completion_context.run();
	ALTUN_REQUIRE(completed);
}

} // namespace
