// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "event.h"

#include <riwo/core/algorithm/misc.h>
#include <riwo/core/async_expected.h>

#include <algorithm>
#include <unistd.h>

namespace libempp::udev
{

namespace detail
{

class RIWO_DECL_HIDDEN event_core::impl : public std::enable_shared_from_this<impl>
{
	RIWO_DISABLE_COPY_MOVE(impl)

public:
	using handle_t = asio::posix::basic_stream_descriptor<>;

	impl(asio::any_io_executor exec, std::string subsystem, received_signal_t &received, error_signal_t &error) :
		m_exec(std::move(exec)), m_handle(m_exec), m_subsystem(std::move(subsystem)),
		m_received(&received), m_error(&error) {}

	~impl()
	{
		std::error_code ignored;
		close(ignored);
	}

	void detach() noexcept
	{
		std::error_code ignored;
		close(ignored);
		m_received = nullptr;
		m_error = nullptr;
	}

	void open(std::string_view dev_type, std::error_code &ec) noexcept
	{
		close(ec);
		if( ec )
			return ;

		::udev *new_udev = udev_new();
		if( not new_udev )
		{
			ec = current_system_error();
			return ;
		}
		udev_monitor *new_monitor = udev_monitor_new_from_netlink(new_udev, "udev");
		if( not new_monitor )
		{
			ec = current_system_error();
			udev_unref(new_udev);
			return ;
		}
		auto release_native = [&]
		{
			udev_monitor_unref(new_monitor);
			udev_unref(new_udev);
		};
		std::string dev_type_value;
		try {
			dev_type_value = dev_type;
		}
		catch(...)
		{
			ec = riwo::exception_error(std::current_exception());
			release_native();
			return ;
		}
		int result = udev_monitor_filter_add_match_subsystem_devtype (
			new_monitor, m_subsystem.c_str(),
			dev_type_value.empty() ? nullptr : dev_type_value.c_str()
		);
		if( result < 0 )
		{
			ec = std::error_code(-result, std::system_category());
			release_native();
			return ;
		}
		result = udev_monitor_enable_receiving(new_monitor);
		if( result < 0 )
		{
			ec = std::error_code(-result, std::system_category());
			release_native();
			return ;
		}
		const int native_fd = udev_monitor_get_fd(new_monitor);
		if( native_fd < 0 )
		{
			ec = current_system_error();
			release_native();
			return ;
		}
		const int wait_fd = ::dup(native_fd);
		if( wait_fd < 0 )
		{
			ec = current_system_error();
			release_native();
			return ;
		}
		ec = m_handle.assign(wait_fd, ec);
		if( ec )
		{
			::close(wait_fd);
			release_native();
			return ;
		}
		m_udev = new_udev;
		m_monitor = new_monitor;

		const auto generation = m_generation;
		try {
			riwo::dispatch(m_exec, event_work(generation));
		}
		catch(...)
		{
			ec = riwo::exception_error(std::current_exception());
			std::error_code ignored;
			close(ignored);
		}
	}

	void close(std::error_code &ec) noexcept
	{
		++m_generation;
		ec.clear();

		if( m_handle.is_open() )
		{
			std::error_code cancel_error;
			ec = m_handle.cancel(cancel_error);
			ec = m_handle.close(ec);

			if( not ec )
				ec = cancel_error;
		}
		std::scoped_lock lock(m_native_mutex);
		if( m_monitor )
		{
			udev_monitor_unref(m_monitor);
			m_monitor = nullptr;
		}
		if( m_udev )
		{
			udev_unref(m_udev);
			m_udev = nullptr;
		}
	}

	[[nodiscard]] bool is_open() const noexcept {
		return m_handle.is_open();
	}

private:
	[[nodiscard]] riwo::awaitable<void> event_work(size_t generation)
	{
		auto self = this->shared_from_this();
		RIWO_UNUSED(self);

		while( generation == m_generation and m_handle.is_open() )
		{
			std::error_code monitor_error;
			co_await m_handle.async_wait(handle_t::wait_read,
				asio::redirect_error(riwo::use_awaitable, monitor_error)
			);
			if( monitor_error )
			{
				if( generation == m_generation )
					co_await fail_monitor(monitor_error);
				co_return ;
			}
			if( generation != m_generation )
				co_return ;

			std::optional<device_event> event;
			try {
				event = receive_ready(monitor_error);
			}
			catch(...) {
				monitor_error = riwo::exception_error(std::current_exception());
			}
			if( monitor_error )
			{
				co_await fail_monitor(monitor_error);
				co_return ;
			}
			if( not event )
				continue;

			if( not m_received )
				co_return ;

			std::error_code observer_error;
			try {
				co_await (*m_received)(std::move(*event));
			}
			catch(...) {
				observer_error = riwo::exception_error(std::current_exception());
			}
			if( observer_error )
				co_await emit_error(observer_error);
		}
		co_return ;
	}

	[[nodiscard]] riwo::awaitable<void> fail_monitor(std::error_code monitor_error)
	{
		std::error_code ignored;
		close(ignored);
		co_await emit_error(monitor_error);
		co_return ;
	}

	[[nodiscard]] riwo::awaitable<void> emit_error(std::error_code event_error) noexcept
	{
		if( not m_error )
			co_return ;
		try {
			co_await (*m_error)(event_error);
		}
		catch(...) {}
		co_return ;
	}

	[[nodiscard]] std::optional<device_event>
	receive_ready(std::error_code &ec)
	{
		std::scoped_lock lock(m_native_mutex);
		ec.clear();
		if( not m_monitor )
		{
			ec = std::make_error_code(std::errc::bad_file_descriptor);
			return std::nullopt;
		}
		errno = 0;
		if( udev_device *device = udev_monitor_receive_device(m_monitor) )
		{
			std::unique_ptr<udev_device,decltype(&udev_device_unref)> owner (
				device, &udev_device_unref
			);
			return make_event(owner.get());
		}
		if( errno != 0 and errno != EAGAIN and errno != EWOULDBLOCK )
			ec = current_system_error();
		return std::nullopt;
	}

	[[nodiscard]] static device_event make_event(udev_device *device)
	{
		device_event result;
		if( not device )
			return result;

		if( const char *value = udev_device_get_action(device) )
			result.action = event_action_from_string(value);

		if( const char *value = udev_device_get_syspath(device) )
			result.sys_path = value;

		if( const char *value = udev_device_get_sysname(device) )
			result.sys_name = value;

		if( const char *value = udev_device_get_devnode(device) )
			result.dev_node = value;

		if( const char *value = udev_device_get_devtype(device) )
			result.dev_type = value;

		udev_list_entry *entry = nullptr;
		auto properties = udev_device_get_properties_list_entry(device);

		udev_list_entry_foreach(entry, properties)
		{
			const char *key = udev_list_entry_get_name(entry);
			if( const char *value = udev_list_entry_get_value(entry); key and value )
				result.properties.emplace(key, value);
		}
		return result;
	}

	[[nodiscard]] static std::error_code current_system_error() noexcept {
		return { errno ? errno : EIO, std::system_category() };
	}

private:
	asio::any_io_executor m_exec;
	handle_t m_handle;
	std::string m_subsystem;

	received_signal_t *m_received = nullptr;
	error_signal_t *m_error = nullptr;

	::udev *m_udev = nullptr;
	udev_monitor *m_monitor = nullptr;

	size_t m_generation = 0;
	std::mutex m_native_mutex;
};

event_core::event_core
(asio::any_io_executor exec, std::string subsystem, received_signal_t &received, error_signal_t &error) :
	m_impl(std::make_shared<impl>(std::move(exec), std::move(subsystem), received, error))
{

}

event_core::~event_core() = default;

void event_core::open(std::string_view dev_type, std::error_code &error) noexcept
{
	m_impl->open(dev_type, error);
}

void event_core::close(std::error_code &error) noexcept
{
	m_impl->close(error);
}

void event_core::detach() noexcept
{
	m_impl->detach();
}

bool event_core::is_open() const noexcept
{
	return m_impl->is_open();
}

} //namespace detail

bool device_event::is_valid() const noexcept
{
	return not sys_path.empty();
}

bool device_event::matches(const properties_t &rules) const
{
	return std::ranges::all_of(rules, [this](const auto &rule)
	{
		const auto &[key, expected] = rule;
		const auto value = property(key);
		return value and riwo::wildcard_match(expected, *value) >= 0;
	});
}

std::optional<std::string_view>
device_event::property(std::string_view key) const noexcept
{
	const auto it = properties.find(key);
	if( it == properties.end() )
		return std::nullopt;
	return it->second;
}

} //namespace libempp::udev
