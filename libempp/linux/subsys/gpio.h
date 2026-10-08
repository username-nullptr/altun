// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_SUBSYS_GPIO_H
#define LIBEMPP_LINUX_SUBSYS_GPIO_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/global.h>
#include <riwo/core/async_expected.h>

namespace libempp::subsys
{

using gpio_line_t = uint32_t;
using gpio_duration_t = std::chrono::milliseconds;

enum class gpio_direction_t : uint8_t {
	input, output
};

enum class gpio_edge_t : uint8_t {
	none, rising, falling, both
};

enum class gpio_backend_t : uint8_t {
	libgpiod, sysfs
};

struct gpio_node_t
{
	std::filesystem::path chip = "/dev/gpiochip0";
	gpio_line_t line = 0;

	gpio_direction_t direction = gpio_direction_t::input;
	bool initial_value = false;
	bool active_low = false;

	gpio_edge_t edge = gpio_edge_t::none;
	std::string consumer = "libempp";
	std::string alias {};
};

struct gpio_event_t
{
	gpio_edge_t edge = gpio_edge_t::none;
	uint64_t timestamp_ns = 0;
};

template <riwo::concepts::exec Exec = asio::any_io_executor>
class LIBEMPP_LINUX_TAPI basic_gpio
{
	RIWO_DISABLE_COPY(basic_gpio)

public:
	using executor_type = Exec;
	using executor_t = executor_type;

	using line_t = gpio_line_t;
	using duration_t = gpio_duration_t;

	using direction_t = gpio_direction_t;
	using edge_t = gpio_edge_t;
	using backend_t = gpio_backend_t;

	using node_t = gpio_node_t;
	using event_t = gpio_event_t;

public:
	template <typename Exec0>
	explicit basic_gpio(const node_t &node, Exec0 &&exec) requires (
		not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio> and
		riwo::concepts::match_sched<Exec0,executor_t>
	);
	explicit basic_gpio(const node_t &node) requires
		riwo::concepts::match_def_exec<executor_t>;

	template <typename Exec0>
	explicit basic_gpio(Exec0 &&exec) requires (
		not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio> and
		riwo::concepts::match_sched<Exec0,executor_t>
	);
	explicit basic_gpio() requires
		riwo::concepts::match_def_exec<executor_t>;

	~basic_gpio();
	basic_gpio(basic_gpio &&other) noexcept;
	basic_gpio &operator=(basic_gpio &&other) noexcept;

public:
	basic_gpio &open(const node_t &node, std::error_code &error) noexcept;
	basic_gpio &open(const node_t &node);
	basic_gpio &close() noexcept;

public:
	basic_gpio &set(bool value, std::error_code &error) noexcept;
	basic_gpio &set(bool value);

	basic_gpio &rising(std::error_code &error) noexcept;
	basic_gpio &rising();

	basic_gpio &falling(std::error_code &error) noexcept;
	basic_gpio &falling();

	basic_gpio &invert(std::error_code &error) noexcept;
	basic_gpio &invert();

	[[nodiscard]] riwo::sys_expected<bool> get() const noexcept;
	[[nodiscard]] explicit operator bool() const;
	[[nodiscard]] bool operator*() const;

public:
	template <typename Token>
	static constexpr bool event_token_v =
		riwo::concepts::tf_opt_token<Token,std::error_code> and
		not riwo::is_detached_v<riwo::token_unbound_t<Token>>;

	template <typename Token = const riwo::use_sync_t&>
	auto wait_event(event_t &event, Token &&token = riwo::use_sync)
		requires event_token_v<Token>;

	using on_event_t = std::function<void(event_t)>;
	basic_gpio &on_event(const event_t &event, on_event_t callback);

public:
	[[nodiscard]] node_t node() const;
	[[nodiscard]] bool is_open() const noexcept;

	[[nodiscard]] static backend_t backend() noexcept;
	[[nodiscard]] static const char *backend_name() noexcept;

	[[nodiscard]] executor_t get_executor() noexcept;

private:
	class impl;
	std::shared_ptr<impl> m_impl;
};

using gpio = basic_gpio<>;

} // namespace libempp::subsys
#include <libempp/linux/subsys/detail/gpio.h>

#endif //__linux__
#endif //LIBEMPP_LINUX_SUBSYS_GPIO_H
