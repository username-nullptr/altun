// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_UDEV_EVENT_H
#define ALTUN_LINUX_UDEV_EVENT_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/udev/properties.h>
#include <riwo/utils/signal_slot.h>
#include <riwo/core/execution.h>
#include <libudev.h>

namespace altun::udev { namespace detail {
class event_core;
} //namespace detail

enum class event_action
{
	add,
	remove,
	change,
	move,
	online,
	offline,
	bind,
	unbind,
	unknown
};

[[nodiscard]] constexpr std::string_view string(event_action action) noexcept;
[[nodiscard]] constexpr event_action event_action_from_string(std::string_view action) noexcept;

struct ALTUN_LINUX_API device_event
{
	event_action action = event_action::unknown;
	std::string sys_path {};
	std::string sys_name {};
	std::string dev_node {};
	std::string dev_type {};

	std::map <
		std::string, std::string, std::less<>
	> properties {};

	[[nodiscard]] bool is_valid() const noexcept;
	[[nodiscard]] bool matches(const properties_t &rules) const;

	[[nodiscard]] std::optional<std::string_view> property(std::string_view key) const noexcept;
};

template <subsys_enum Subsys, riwo::concepts::exec Exec = asio::any_io_executor>
	requires subsys::is_valid_v<Subsys>
class ALTUN_LINUX_TAPI basic_event
{
	RIWO_DISABLE_COPY_MOVE(basic_event)

public:
	using executor_type = Exec;
	using executor_t = executor_type;

	using handle_t = asio::posix::basic_stream_descriptor<executor_t>;
	using signal_t = riwo::utils::signal<riwo::awaitable<void>(device_event)>;
	using error_signal_t = riwo::utils::signal<riwo::awaitable<void>(riwo::error_code)>;

	explicit basic_event(riwo::concepts::match_sched<Exec> auto &&exec);
	basic_event() requires riwo::concepts::match_def_exec<Exec>;
	~basic_event();

public:
	template <typename Error>
	void open(Error &error) noexcept
		requires riwo::is_error_code_token_v<Error&>;

	template <typename Error>
	void open(std::string_view dev_type, Error &error) noexcept
		requires riwo::is_error_code_token_v<Error&>;

	template <typename Error>
	void close(Error &error) noexcept
		requires riwo::is_error_code_token_v<Error&>;

	void open();
	void open(std::string_view dev_type);
	void close();

public:
	[[nodiscard]] executor_t get_executor() noexcept;
	[[nodiscard]] bool is_open() const noexcept;

public:
	signal_t received;
	error_signal_t error;

private:
	executor_t m_exec;
	std::shared_ptr<detail::event_core> m_impl {};
};

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
using event = basic_event<Subsys>;

} // namespace altun::udev

#include <altun/linux/udev/detail/event.h>

#endif //__linux__
#endif // ALTUN_LINUX_UDEV_EVENT_H
