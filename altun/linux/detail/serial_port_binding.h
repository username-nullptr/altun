// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_DETAIL_SERIAL_PORT_BINDING_H
#define ALTUN_LINUX_DETAIL_SERIAL_PORT_BINDING_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/core/log.h>
#include <riwo/core/algorithm/misc.h>
#include <riwo/coro/utils.h>
#include <algorithm>
#include <set>

namespace altun
{

template <riwo::concepts::exec Exec>
class ALTUN_LINUX_TAPI basic_serial_port_binding<Exec>::impl :
	public std::enable_shared_from_this<impl>
{
	RIWO_DISABLE_COPY_MOVE(impl)
	using baud_rate_t      = stream_t::baud_rate;
	using character_size_t = stream_t::character_size;
	using stop_bits_t      = stream_t::stop_bits;
	using parity_t         = stream_t::parity;
	using flow_control_t   = stream_t::flow_control;

public:
	explicit impl(auto &&exec) :
		m_exec(riwo::get_executor_helper(std::forward<decltype(exec)>(exec))) {}

	void signal_relay(basic_serial_port_binding *q_ptr, const rule_context_ptr &context)
	{
		auto self = this->shared_from_this();
		context->opened.connect(self, [q_ptr](std::string_view port) -> riwo::awaitable<void> {
			co_return co_await q_ptr->opened(port);
		});
		context->closed.connect(self, [q_ptr]
		(std::string_view port, const std::error_code &error) -> riwo::awaitable<void> {
			co_return co_await q_ptr->closed(port, error);
		});
		context->received.connect(self, [q_ptr](io_context_ptr ioc) -> riwo::awaitable<void> {
			co_return co_await q_ptr->received(std::move(ioc));
		});
		context->error.connect(self, [q_ptr]
		(std::string_view port, const std::error_code &error) -> riwo::awaitable<void> {
			co_return co_await q_ptr->error(port, error);
		});
	}

private:
	static void set_option(stream_t &stream, const options_t &options)
	{
		stream.set_option(baud_rate_t(options.baud_rate));
		stream.set_option(character_size_t(options.data_bits));

		if( options.stop_bits == options_t::stop_bits_1 )
			stream.set_option(stop_bits_t(stop_bits_t::one));
		else if( options.stop_bits == options_t::stop_bits_2 )
			stream.set_option(stop_bits_t(stop_bits_t::two));
		else if( options.stop_bits == options_t::stop_bits_1p5 )
			stream.set_option(stop_bits_t(stop_bits_t::onepointfive));

		if( options.parity == options_t::parity_t::none )
			stream.set_option(parity_t(parity_t::none));
		else if( options.parity == options_t::parity_t::odd )
			stream.set_option(parity_t(parity_t::odd));
		else if( options.parity == options_t::parity_t::even )
			stream.set_option(parity_t(parity_t::even));

		if( options.flow_control == options_t::flow_control_t::none )
			stream.set_option(flow_control_t(flow_control_t::none));
		else if( options.flow_control == options_t::flow_control_t::software )
			stream.set_option(flow_control_t(flow_control_t::software));
		else if( options.flow_control == options_t::flow_control_t::hardware )
			stream.set_option(flow_control_t(flow_control_t::hardware));
	}

public:
	executor_t m_exec;
};

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::basic_serial_port_binding
(riwo::concepts::match_sched<Exec> auto &&exec) :
	m_impl(std::make_shared<impl>(std::forward<decltype(exec)>(exec)))
{

}

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::basic_serial_port_binding()
	requires riwo::concepts::match_def_exec<Exec> :
	m_impl(std::make_shared<impl>(riwo::io_context()))
{

}

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::~basic_serial_port_binding() = default;

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::device_t::device_t(riwo::concepts::string_p<char> auto &&port) :
	port(riwo::strtls::to_string(std::forward<decltype(port)>(port)))
{

}

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::device_t::device_t(const udev_t &dev)
{
	if( dev.is_valid() )
	{
		port = dev.property(udev::basic_prop_key::dev_name)
			.or_else()->to_string();
	}
}

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::executor_t basic_serial_port_binding<Exec>::get_executor() noexcept
{
	return m_impl->m_exec;
}

template <riwo::concepts::exec Exec>
class ALTUN_LINUX_TAPI basic_serial_port_binding<Exec>::rule_context::impl :
	public std::enable_shared_from_this<impl>
{
	RIWO_DISABLE_COPY_MOVE(impl)
	using baud_rate_t      = stream_t::baud_rate;
	using character_size_t = stream_t::character_size;
	using stop_bits_t      = stream_t::stop_bits;
	using parity_t         = stream_t::parity;
	using flow_control_t   = stream_t::flow_control;

	using udev_event_t = udev::basic_event <
		subsys::enumeration::tty, executor_t
	>;

public:
	impl(const executor_t &exec, std::string port, const options_t &options) :
		m_exec(exec), m_event(m_exec), m_options(options)
	{
		m_devs.emplace(port, std::make_shared<stream_t>(m_exec));
	}

	impl(const executor_t &exec, rules_t rules, const options_t &options) :
		m_exec(exec), m_event(m_exec), m_options(options), m_rules(std::move(rules)) {}

	void start(const rule_context_ptr &ptr)
	{
		q_ptr = ptr;
		if( not m_rules.empty() )
		{
			auto self = this->shared_from_this();
			m_event.received.connect(self, &impl::handle_monitor_event);
			m_event.error.connect(self, &impl::handle_monitor_error);
			riwo::dispatch(m_exec, rule_work());
		}
	}

public:
	void open() noexcept
	{
		if( m_open )
			return ;

		m_open = true;
		const auto generation = ++m_generation;

		for(auto &[port,stream] : m_devs)
		{
			if( not stream->is_open() )
				riwo::dispatch(m_exec, dev_work(port, stream, false, generation));
		}
	}

	void close() noexcept
	{
		if( not m_open )
			return ;

		m_open = false;
		++m_generation;

		std::error_code close_error {};
		for(auto &[port,stream] : m_devs)
		{
			close_error.clear();
			if( stream->is_open() )
				stream->close(close_error);

			if( m_opened_ports.erase(port) > 0 )
				notify_closed(port, close_error);
		}
	}

	void shutdown() noexcept
	{
		m_shutdown = true;
		close();

		std::error_code ignored;
		m_event.close(ignored);
	}

public:
	using io_handler_t = asio::any_completion_handler<void(std::error_code,size_t)>;
	using write_target_t = std::pair<std::string,stream_ptr>;
	using write_targets_t = std::vector<write_target_t>;

	[[nodiscard]] riwo::io_expected write
	(const stream_ptr &stream, const riwo::const_buffer &buffer) noexcept
	{
		if( not stream )
		{
			return riwo::io_unexpected (
				std::make_error_code(std::errc::no_such_device)
			);
		}
		std::error_code write_error {};
		auto sum = asio::write(*stream, buffer, write_error);

		if( write_error )
			return riwo::io_unexpected(write_error);
		return sum;
	}

	template <typename Token>
	[[nodiscard]] auto write(const std::string &port, const stream_ptr &stream,
		const riwo::const_buffer &buffer, Token &&token)
	{
		if constexpr( riwo::is_error_code_token_v<Token> )
		{
			return riwo::expected_value_or_error (
				write_and_notify(port, stream, buffer), token
			);
		}
		else if constexpr( riwo::is_sync_opt_token_v<Token> )
			return write_and_notify(port, stream, buffer);
		else
		{
			using token_t = std::remove_cvref_t<Token>;
			if constexpr( riwo::is_detached_v<riwo::token_unbound_t<token_t>> )
			{
				auto owner = copy_write_buffer(buffer);
				return riwo::initiate_preserved_expected<size_t>(m_exec,
				[self = this->shared_from_this(), port, stream, owner]() mutable {
					return self->co_write(port, stream, riwo::const_buffer(*owner));
				}, std::forward<Token>(token));
			}
			else
			{
				return riwo::initiate_preserved_expected<size_t>(m_exec,
				[self = this->shared_from_this(), port, stream, buffer]() mutable {
					return self->co_write(port, stream, buffer);
				}, std::forward<Token>(token));
			}
		}
	}

	[[nodiscard]] riwo::awaitable<riwo::io_expected> co_write
	(std::string port, stream_ptr stream, riwo::const_buffer buffer)
	{
		std::error_code write_error {};
		auto size = co_await async_write(std::move(port), stream, buffer,
			asio::redirect_error(riwo::use_awaitable, write_error)
		);
		if( write_error )
			co_return riwo::io_unexpected(write_error);
		co_return size;
	}

	template <typename Token>
	[[nodiscard]] auto async_write
	(std::string port, const stream_ptr &stream, riwo::const_buffer buffer, Token &&token)
	{
		using token_t = std::remove_cvref_t<Token>;
		token_t completion_token(std::forward<Token>(token));

		return asio::async_initiate<token_t,void(std::error_code,size_t)>(
		[self = this->shared_from_this(), port = std::move(port), stream, buffer](auto handler) mutable
		{
			self->async_write(std::move(port), stream, buffer,
				io_handler_t(std::move(handler))
			);
		},
		completion_token);
	}

	void async_write
	(std::string port, const stream_ptr &stream, riwo::const_buffer buffer, io_handler_t handler)
	{
		if( not stream )
		{
			const auto write_error = std::make_error_code(std::errc::no_such_device);
			notify_error(port, write_error);
			post_write_result(std::move(handler),
				write_error, 0
			);
			return ;
		}
		auto slot = asio::get_associated_cancellation_slot(handler);
		auto exec = asio::get_associated_executor(handler, m_exec);

		auto immediate_exec = asio::get_associated_immediate_executor(handler, m_exec);
		auto alloc = asio::get_associated_allocator(handler);

		auto completion = [self = this->shared_from_this(),
			port = std::move(port), handler = std::move(handler)
		](std::error_code error, size_t size) mutable
		{
			if( error )
				self->notify_error(port, error);
			std::move(handler)(error, size);
		};
		asio::async_write(*stream, buffer, asio::bind_immediate_executor(immediate_exec,
			asio::bind_allocator(alloc, asio::bind_executor(exec,
				asio::bind_cancellation_slot(slot, std::move(completion))
			)))
		);
	}

	[[nodiscard]] riwo::sys_expected<> write
	(const write_targets_t &targets, const riwo::const_buffer &buffer) {
		return write_many(targets, buffer);
	}

	template <typename Token>
	[[nodiscard]] auto write(const write_targets_t &targets,
		const riwo::const_buffer &buffer, Token &&token)
	{
		if constexpr( riwo::is_error_code_token_v<Token> )
		{
			auto result = write_many(targets, buffer);
			token = result ? std::error_code{} : result.error();
		}
		else if constexpr( riwo::is_sync_opt_token_v<Token> )
			return write_many(targets, buffer);
		else
		{
			using token_t = std::remove_cvref_t<Token>;
			if constexpr( riwo::is_detached_v<riwo::token_unbound_t<token_t>> )
			{
				auto owner = copy_write_buffer(buffer);
				return riwo::initiate_preserved_expected<void>(m_exec,
				[self = this->shared_from_this(), targets, owner]() mutable {
					return self->co_write_many(targets, riwo::const_buffer(*owner));
				}, std::forward<Token>(token));
			}
			else
			{
				return riwo::initiate_preserved_expected<void>(m_exec,
				[self = this->shared_from_this(), targets, buffer]() mutable {
					return self->co_write_many(targets, buffer);
				}, std::forward<Token>(token));
			}
		}
	}

private:
	[[nodiscard]] static std::shared_ptr<std::string>
	copy_write_buffer(const riwo::const_buffer &buffer)
	{
		auto owner = std::make_shared<std::string>();
		if( buffer.size() > 0 )
		{
			owner->assign (
				static_cast<const char*>(buffer.data()), buffer.size()
			);
		}
		return owner;
	}

	void post_write_result(io_handler_t handler, std::error_code write_error, size_t size) {
		riwo::post_completion(m_exec, std::move(handler), write_error, size);
	}

	void notify_error(std::string_view port, std::error_code operation_error) noexcept
	{
		try {
			auto context = q_ptr.lock();
			if( not context )
				return ;

			std::string owned_port(port);
			riwo::dispatch(m_exec,
			[context = std::move(context), port = std::move(owned_port), operation_error]
			() mutable -> riwo::awaitable<void> {
				co_await context->error(port, operation_error);
				co_return ;
			});
		}
		catch(...) {}
	}

	void notify_closed(std::string_view port, std::error_code close_error) noexcept
	{
		try {
			auto context = q_ptr.lock();
			if( not context )
				return ;

			std::string owned_port(port);
			riwo::dispatch(m_exec,
			[context = std::move(context), port = std::move(owned_port), close_error]
			() mutable -> riwo::awaitable<void> {
				co_await context->closed(port, close_error);
				co_return ;
			});
		}
		catch(...) {}
	}

	[[nodiscard]] riwo::awaitable<void> emit_closed
	(const std::string &port, const std::error_code &close_error)
	{
		if( m_opened_ports.erase(port) == 0 )
			co_return ;

		if( auto context = q_ptr.lock() )
			co_await context->closed(port, close_error);
		co_return ;
	}

	[[nodiscard]] riwo::io_expected write_and_notify
	(std::string_view port, const stream_ptr &stream, const riwo::const_buffer &buffer) noexcept
	{
		auto result = write(stream, buffer);
		if( not result )
			notify_error(port, result.error());
		return result;
	}

	[[nodiscard]] riwo::sys_expected<> write_many
	(const write_targets_t &targets, const riwo::const_buffer &buffer)
	{
		std::error_code first_error {};
		for( const auto &[port, stream] : targets )
		{
			auto result = write_and_notify(port, stream, buffer);
			if( result )
				continue;

			if( not first_error )
				first_error = result.error();

			log_write_error(port, result.error());
		}
		if( first_error )
			return riwo::sys_unexpected(first_error);
		return riwo::make_sys_expected();
	}

	[[nodiscard]] riwo::awaitable<riwo::sys_expected<>>
	co_write_many(write_targets_t targets, riwo::const_buffer buffer)
	{
		std::error_code first_error {};
		for( auto &[port, stream] : targets )
		{
			std::error_code write_error {};
			co_await write(port, stream, buffer,
				asio::redirect_error(riwo::use_awaitable, write_error)
			);
			if( write_error )
			{
				if( not first_error )
					first_error = write_error;

				log_write_error(port, write_error);
				if( write_error == asio::error::operation_aborted )
					co_return riwo::sys_unexpected(write_error);
			}
		}
		if( first_error )
			co_return riwo::sys_unexpected(first_error);
		co_return riwo::make_sys_expected();
	}

	static void log_write_error
	(std::string_view port, const std::error_code &write_error)
	{
		altun_clog_warning("Altun.Linux",
			"serial_port_binding: Failed to write device [{}]: {}",
			port, write_error
		);
	}

private:
	[[nodiscard]] riwo::awaitable<void> rule_work()
	{
		auto self = this->shared_from_this();
		RIWO_UNUSED(self);

		if( m_shutdown or q_ptr.expired() )
			co_return ;

		std::error_code monitor_error;
		m_event.open(monitor_error);

		udev::properties_t properties;
		for(auto &[key,value] : m_rules)
			properties.emplace(key, *value);

		for(auto &dev : udev_t::list(properties))
			register_rule_device(dev.native());

		if( m_shutdown or q_ptr.expired() )
			co_return ;

		if( monitor_error )
		{
			altun_clog_warning("Altun.Linux",
				"serial_port_binding: Failed to monitor tty devices: {}",
				monitor_error
			);
			if( auto context = q_ptr.lock() )
				co_await context->error(std::string_view{}, monitor_error);
			co_return ;
		}

		co_return ;
	}

	void handle_monitor_event(const udev::device_event &event)
	{
		const bool removed = event.action == udev::event_action::remove or
			event.action == udev::event_action::unbind;

		const auto port = device_port(event);
		const auto &sys_path = event.sys_path;

		if( removed )
		{
			unregister_rule_device(port, sys_path);
			return ;
		}
		if( matches_rule(event) )
			register_rule_device(event);
		else
			unregister_rule_device(port, sys_path);
	}

	void handle_monitor_error(const std::error_code &event_error) noexcept
	{
		if( event_error == asio::error::operation_aborted or
			event_error == std::errc::operation_canceled or m_shutdown )
			return ;

		altun_clog_warning("Altun.Linux",
			"serial_port_binding: Failed to receive tty event: {}",
			event_error
		);
		notify_error(std::string_view{}, event_error);
	}

	[[nodiscard]] bool matches_rule(const udev::device_event &event) const
	{
		return std::ranges::all_of(m_rules, [&event](const auto &rule)
		{
			const auto &[key, value] = rule;
			const auto property = event.property(key);
			return property and riwo::wildcard_match(*value, *property) >= 0;
		});
	}

	[[nodiscard]] static std::string device_port(const udev::device_event &event)
	{
		if( not event.dev_node.empty() )
			return event.dev_node;
		if( const auto port = event.property(udev::basic_prop_key::dev_name) )
			return std::string(*port);
		return {};
	}

	void register_rule_device(const udev::device_event &event)
	{
		register_rule_device(device_port(event), event.sys_path);
	}

	void register_rule_device(udev_device *device)
	{
		if( not device )
			return ;

		std::string port;
		if( const char *value = udev_device_get_devnode(device) )
			port = value;

		else if( const char *dev_name =
			udev_device_get_property_value(device, udev::basic_prop_key::dev_name) )
			port = dev_name;

		std::string sys_path;
		if( const char *value = udev_device_get_syspath(device) )
			sys_path = value;
		register_rule_device(std::move(port), std::move(sys_path));
	}

	void register_rule_device(std::string port, std::string sys_path)
	{
		if( port.empty() )
			return ;

		if( not sys_path.empty() )
		{
			auto previous = m_sys_path_ports.find(sys_path);
			if( previous != m_sys_path_ports.end() and previous->second != port )
				unregister_rule_device(previous->second, sys_path);
			m_sys_path_ports[sys_path] = port;
		}
		auto [it, inserted] = m_devs.emplace (
			port, std::make_shared<stream_t>(m_exec)
		);
		if( inserted and m_open )
		{
			riwo::dispatch(m_exec,
				dev_work(port, it->second, true, m_generation)
			);
		}
	}

	void unregister_rule_device(std::string port, const std::string &sys_path)
	{
		if( port.empty() and not sys_path.empty() )
		{
			if( auto it = m_sys_path_ports.find(sys_path); it != m_sys_path_ports.end() )
				port = it->second;
		}
		if( not sys_path.empty() )
			m_sys_path_ports.erase(sys_path);

		if( port.empty() )
			return ;

		std::erase_if(m_sys_path_ports, [&port](const auto &entry) {
			return entry.second == port;
		});
		auto it = m_devs.find(port);
		if( it == m_devs.end() )
			return ;

		auto stream = std::move(it->second);
		m_devs.erase(it);

		std::error_code close_error;
		if( stream->is_open() )
			stream->close(close_error);

		if( m_opened_ports.erase(port) > 0 )
		{
			if( not close_error )
				close_error = std::make_error_code(std::errc::no_such_device);
			notify_closed(port, close_error);
		}
		if( close_error and close_error != std::errc::no_such_device )
			notify_error(port, close_error);
	}

	[[nodiscard]] bool is_current_rule_device
	(const std::string &port, const stream_ptr &stream, bool is_rule) const noexcept
	{
		if( not is_rule )
			return true;
		const auto it = m_devs.find(port);
		return it != m_devs.end() and it->second == stream;
	}

	[[nodiscard]] riwo::awaitable<void> dev_work
	(std::string port, stream_ptr stream, bool is_rule, size_t generation)
	{
		using namespace std::chrono_literals;
		using namespace riwo::coro::literals;

		auto self = this->shared_from_this();
		RIWO_UNUSED(self);

		while( not q_ptr.expired() and m_open and generation == m_generation and
			   is_current_rule_device(port, stream, is_rule) )
		{
			std::error_code open_error {};
			std::error_code cleanup_error {};
			try {
				stream->open(port);
				set_option(stream);
			}
			catch(const std::system_error &ex)
			{
				open_error = ex.code();
				if( stream->is_open() )
					stream->close(cleanup_error);
			}
			if( q_ptr.expired() or not m_open or generation != m_generation or
				not is_current_rule_device(port, stream, is_rule) )
			{
				if( stream->is_open() )
				{
					cleanup_error.clear();
					stream->close(cleanup_error);
				}
				break;
			}
			else if( not open_error )
			{
				if( m_opened_ports.emplace(port).second and not q_ptr.expired() )
				{
					m_open_error = {};
					co_await q_ptr.lock()->opened(port);
				}
				if( q_ptr.expired() or not m_open or generation != m_generation or
					not is_current_rule_device(port, stream, is_rule) )
					break;

				co_await read_work(port, stream, generation);
				if( q_ptr.expired() or not m_open or generation != m_generation or
					not is_current_rule_device(port, stream, is_rule) )
					break;

				co_await 500_ms;
				if( q_ptr.expired() or not m_open or generation != m_generation )
					break;
				continue;
			}
			if( m_open_error != open_error or m_open_err_cr++ % 5 == 0 )
			{
				altun_clog_warning("Altun.Linux",
					"serial_port_binding: Failed to open device [{}]: {}",
					port, open_error
				);
				m_open_error = open_error;
			}
			if( not q_ptr.expired() )
				co_await q_ptr.lock()->error(port, open_error);

			if( cleanup_error and not q_ptr.expired() )
				co_await q_ptr.lock()->error(port, cleanup_error);

			if( q_ptr.expired() or not m_open or generation != m_generation or
				not is_current_rule_device(port, stream, is_rule) )
				break;
			co_await 1_s;
		}
		co_return ;
	}

	[[nodiscard]] riwo::awaitable<void> read_work
	(std::string port, stream_ptr stream, size_t generation)
	{
		using namespace std::chrono_literals;
		using namespace riwo::coro::literals;
		using namespace riwo::operators;

		auto self = this->shared_from_this();
		constexpr size_t read_buffer_size = 1024;
		typename io_context::payload_t read_buffer(read_buffer_size);
		std::error_code closed_error {};
		bool closed_edge = false;

		while( not q_ptr.expired() and m_open and generation == m_generation )
		{
			std::error_code read_error {};
			auto sum = co_await stream->async_read_some (
				asio::buffer(read_buffer), asio::use_awaitable | read_error
			);
			if( q_ptr.expired() or not m_open or generation != m_generation )
				break;

			else if( sum > 0 and not q_ptr.expired() )
			{
				read_buffer.resize(sum);
				co_await q_ptr.lock()->received(std::make_shared<io_context>(
					self, port, stream, std::move(read_buffer)
				));
				read_buffer.resize(read_buffer_size);
			}
			if( not read_error )
				continue;

			else if( read_error == asio::error::operation_aborted )
			{
				if( stream->is_open() )
					continue;

				closed_error = read_error;
				closed_edge = true;
				break;
			}
			else if( read_error == asio::error::eof )
			{
				closed_error = read_error;
				closed_edge = true;
				break;
			}
			else if( read_error == asio::error::bad_descriptor )
			{
				altun_clog_warning("Altun.Linux",
					"serial_port_binding[{}]: Read error: {}", port, read_error
				);
				if( not q_ptr.expired() )
					co_await q_ptr.lock()->error(port, read_error);

				closed_error = read_error;
				closed_edge = true;
				break;
			}
			altun_clog_warning("Altun.Linux",
				"serial_port_binding[{}]: Read error: {}", port, read_error
			);
			if( not q_ptr.expired() )
				co_await q_ptr.lock()->error(port, read_error);

			closed_error = read_error;
			closed_edge = true;
			break;
		}
		if( generation == m_generation )
		{
			std::error_code close_error {};
			if( stream->is_open() )
				stream->close(close_error);

			if( closed_edge )
			{
				if( not closed_error and close_error )
					closed_error = close_error;
				co_await emit_closed(port, closed_error);
			}
			if( close_error and not q_ptr.expired() )
				co_await q_ptr.lock()->error(port, close_error);
		}
		co_return ;
	}

private:
	void set_option(stream_ptr &stream)
	{
		stream->set_option(baud_rate_t(m_options.baud_rate));
		stream->set_option(character_size_t(m_options.data_bits));

		if( m_options.stop_bits == options_t::stop_bits_1 )
			stream->set_option(stop_bits_t(stop_bits_t::one));
		else if( m_options.stop_bits == options_t::stop_bits_2 )
			stream->set_option(stop_bits_t(stop_bits_t::two));
		else if( m_options.stop_bits == options_t::stop_bits_1p5 )
			stream->set_option(stop_bits_t(stop_bits_t::onepointfive));

		if( m_options.parity == options_t::parity_t::none )
			stream->set_option(parity_t(parity_t::none));
		else if( m_options.parity == options_t::parity_t::odd )
			stream->set_option(parity_t(parity_t::odd));
		else if( m_options.parity == options_t::parity_t::even )
			stream->set_option(parity_t(parity_t::even));

		if( m_options.flow_control == options_t::flow_control_t::none )
			stream->set_option(flow_control_t(flow_control_t::none));
		else if( m_options.flow_control == options_t::flow_control_t::software )
			stream->set_option(flow_control_t(flow_control_t::software));
		else if( m_options.flow_control == options_t::flow_control_t::hardware )
			stream->set_option(flow_control_t(flow_control_t::hardware));
	}

public:
	std::weak_ptr<rule_context> q_ptr {};
	executor_t m_exec {};
	udev_event_t m_event;

	std::map<std::string, stream_ptr> m_devs {};
	std::map<std::string, std::string> m_sys_path_ports {};
	std::set<std::string> m_opened_ports {};
	options_t m_options	{};

	rules_t m_rules {};
	bool m_open = false;
	bool m_shutdown = false;
	size_t m_generation = 0;

	std::error_code m_open_error {};
	size_t m_open_err_cr = 0;
};

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::rule_context_ptr basic_serial_port_binding<Exec>::make_rule
(riwo::concepts::string_p<char> auto &&rule_key, riwo::value value, const options_t &options)
{
	rules_t rule {{
		riwo::strtls::to_string(std::forward<decltype(rule_key)>(rule_key)),
		std::move(value)
	}};
	return make_rule(std::move(rule), options);
}

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::rule_context_ptr basic_serial_port_binding<Exec>::make_rule
(rules_t rules, const options_t &options)
{
	auto context = std::make_shared<rule_context>(
		get_executor(), std::move(rules), options
	);
	m_impl->signal_relay(this, context);
	context->m_impl->start(context);
	return context;
}

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::rule_context_ptr basic_serial_port_binding<Exec>::make_rule
(device_t port, const options_t &options)
{
	auto context = std::make_shared<rule_context>(
		get_executor(), std::move(port.port), options
	);
	m_impl->signal_relay(this, context);
	context->m_impl->start(context);
	return context;
}

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::rule_context::rule_context
(const executor_t &exec, std::string port, const options_t &options) :
	m_impl(std::make_shared<impl>(exec, std::move(port), options))
{

}

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::rule_context::rule_context
(const executor_t &exec, rules_t rules, const options_t &options) :
	m_impl(std::make_shared<impl>(exec, std::move(rules), options))
{

}

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::rule_context::~rule_context()
{
	m_impl->shutdown();
}

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::rule_context::ptr_t
basic_serial_port_binding<Exec>::rule_context::open()
{
	m_impl->open();
	return this->shared_from_this();
}

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::rule_context::ptr_t
basic_serial_port_binding<Exec>::rule_context::close()
{
	m_impl->close();
	return this->shared_from_this();
}

template <riwo::concepts::exec Exec>
template <riwo::concepts::dis_func_tf_opt_token<std::error_code,size_t> Token>
auto basic_serial_port_binding<Exec>::rule_context::write
(const std::vector<std::string> &ports, riwo::const_buffer buffer, Token &&token)
{
	typename impl::write_targets_t targets;
	targets.reserve(ports.size());
	for( const auto &port : ports )
	{
		auto it = m_impl->m_devs.find(port);
		targets.emplace_back(port,
			it == m_impl->m_devs.end() ? stream_ptr{} : it->second
		);
	}
	return m_impl->write(targets, buffer, std::forward<Token>(token));
}

template <riwo::concepts::exec Exec>
template <riwo::concepts::dis_func_tf_opt_token<std::error_code,size_t> Token>
auto basic_serial_port_binding<Exec>::rule_context::write
(std::string_view port, riwo::const_buffer buffer, Token &&token)
{
	std::string target_port(port);
	stream_ptr stream;

	auto it = m_impl->m_devs.find(target_port);
	if( it != m_impl->m_devs.end() )
	{
		target_port = it->first;
		stream = it->second;
	}
	return m_impl->write(target_port, stream, buffer,
		std::forward<Token>(token)
	);
}

template <riwo::concepts::exec Exec>
template <riwo::concepts::dis_func_tf_opt_token<std::error_code,size_t> Token>
auto basic_serial_port_binding<Exec>::rule_context::write
(riwo::const_buffer buffer, Token &&token)
{
	typename impl::write_targets_t targets;
	targets.reserve(m_impl->m_devs.size());

	for( const auto &[port, stream] : m_impl->m_devs )
		targets.emplace_back(port, stream);

	return m_impl->write(targets, buffer, std::forward<Token>(token));
}

template <riwo::concepts::exec Exec>
std::vector<std::string> basic_serial_port_binding<Exec>::rule_context::ports() const noexcept
{
	std::vector<std::string> ports;
	for(auto &[port,stream] : m_impl->m_devs)
		ports.emplace_back(port);
	return ports;
}

template <riwo::concepts::exec Exec>
auto basic_serial_port_binding<Exec>::rule_context::get_executor() noexcept -> executor_t
{
	return m_impl->m_exec;
}

template <riwo::concepts::exec Exec>
class ALTUN_LINUX_TAPI basic_serial_port_binding<Exec>::io_context::impl
{
	RIWO_DISABLE_COPY_MOVE(impl)

public:
	impl(rule_ptr rule, std::string port, stream_ptr stream, payload_t payload) :
		m_rule(std::move(rule)), m_port(std::move(port)), m_stream(std::move(stream)),
		m_payload(std::move(payload)) {}

	rule_ptr m_rule {};
	std::string m_port {};
	stream_ptr m_stream {};
	payload_t m_payload {};
};

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::io_context::io_context
(rule_ptr rule, std::string port, stream_ptr stream, payload_t payload) :
	m_impl(std::make_shared<impl>(
		std::move(rule), std::move(port), std::move(stream), std::move(payload)
	))
{

}

template <riwo::concepts::exec Exec>
basic_serial_port_binding<Exec>::io_context::~io_context() = default;

template <riwo::concepts::exec Exec>
template <riwo::concepts::dis_func_tf_opt_token<std::error_code,size_t> Token>
auto basic_serial_port_binding<Exec>::io_context::write
(riwo::const_buffer buffer, Token &&token)
{
	return m_impl->m_rule->write (
		m_impl->m_port, m_impl->m_stream, buffer,
		std::forward<Token>(token)
	);
}

template <riwo::concepts::exec Exec>
template <typename Buffer>
decltype(auto) basic_serial_port_binding<Exec>::io_context::payload() const
	noexcept(std::same_as<Buffer,std::string> or std::same_as<Buffer,payload_t>)
	requires is_buffer_v<Buffer>
{
	if constexpr( std::same_as<Buffer,std::string> )
	{
		if( m_impl->m_payload.empty() )
			return std::string_view {};

		return std::string_view (
			reinterpret_cast<const char*>(m_impl->m_payload.data()),
			m_impl->m_payload.size()
		);
	}
	else if constexpr( std::same_as<Buffer,payload_t> )
		return static_cast<const payload_t&>(m_impl->m_payload);
	else
		return riwo::copy_buffer_data<Buffer>(m_impl->m_payload);
}

template <riwo::concepts::exec Exec>
template <typename Buffer>
Buffer basic_serial_port_binding<Exec>::io_context::take_payload()
	noexcept(std::same_as<Buffer,payload_t>)
	requires is_buffer_v<Buffer>
{
	if constexpr( std::same_as<Buffer,payload_t> )
		return std::move(m_impl->m_payload);
	else
	{
		auto result = riwo::copy_buffer_data<Buffer>(m_impl->m_payload);
		m_impl->m_payload.clear();
		return result;
	}
}

template <riwo::concepts::exec Exec>
std::string_view basic_serial_port_binding<Exec>::io_context::port() const noexcept
{
	return m_impl->m_port;
}

template <riwo::concepts::exec Exec>
auto basic_serial_port_binding<Exec>::io_context::get_executor() noexcept -> executor_t
{
	return m_impl->m_rule->m_exec;
}

} //namespace altun

#endif //__linux__
#endif //ALTUN_LINUX_DETAIL_SERIAL_PORT_BINDING_H
