// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_SUBSYS_DETAIL_GPIO_H
#define ALTUN_LINUX_SUBSYS_DETAIL_GPIO_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/subsys/detail/gpio/backend.h>

namespace altun::subsys { namespace detail
{

[[nodiscard]] ALTUN_LINUX_API bool valid_direction(gpio_direction_t direction) noexcept;
[[nodiscard]] ALTUN_LINUX_API bool valid_edge(gpio_edge_t edge) noexcept;

} // namespace detail

template <riwo::concepts::exec Exec>
class ALTUN_LINUX_TAPI basic_gpio<Exec>::impl : public std::enable_shared_from_this<impl>
{
	RIWO_DISABLE_COPY_MOVE(impl)

private:
	using event_handler_t = asio::any_completion_handler<void(std::error_code)>;
	using event_handle_t = asio::posix::basic_stream_descriptor<executor_t>;
	using callback_executor_t = asio::strand<executor_t>;
	using io_work_t = decltype(asio::make_work_guard(std::declval<const executor_t&>()));
	using completion_work_t = decltype(asio::make_work_guard(
		std::declval<const event_handler_t&>(), std::declval<const executor_t&>()));

	struct wait_operation
	{
		wait_operation(event_t &event, event_handler_t handler,
			io_work_t io_guard, completion_work_t completion_guard) :
			destination(&event), completion(std::move(handler)),
			io_work(std::move(io_guard)), completion_work(std::move(completion_guard)) {}

		uint64_t id = 0;
		event_t *destination = nullptr;
		event_handler_t completion;
		io_work_t io_work;
		completion_work_t completion_work;
	};

	struct event_subscription
	{
		edge_t edge = edge_t::none;
		std::shared_ptr<on_event_t> callback;
	};

public:
	explicit impl(riwo::concepts::match_sched<Exec> auto &&exec) :
		m_exec(riwo::get_executor_helper(std::forward<decltype(exec)>(exec))),
		m_event_handle(m_exec),
		m_callback_exec(asio::make_strand(m_exec)) {}

	~impl() {
		close();
	}

public:
	void open(const node_t &node, std::error_code &error) noexcept
	{
		close();
		error.clear();

		if( node.chip.empty() or not detail::valid_direction(node.direction) or
			not detail::valid_edge(node.edge) or
			(node.direction == direction_t::output and node.edge != edge_t::none) )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return ;
		}
		if( not m_backend )
		{
			m_backend = detail::make_gpio_backend(error);
			if( error )
				return ;
		}
		{
			std::scoped_lock backend_lock(m_backend_mutex);
			m_backend->open(node, error);
		}
		if( error )
			return ;

		if( node.direction == direction_t::input and node.edge != edge_t::none )
		{
			int native_handle = -1;
			{
				std::scoped_lock backend_lock(m_backend_mutex);
				native_handle = m_backend->event_handle();
			}
			if( native_handle < 0 )
				error = std::make_error_code(std::errc::bad_file_descriptor);
			else
			{
				const int monitor_handle = ::dup(native_handle);
				if( monitor_handle < 0 )
					error = {errno, std::system_category()};
				else
				{
					m_event_handle.assign(monitor_handle, error);
					if( error )
						::close(monitor_handle);
				}
			}
		}
		if( error )
		{
			std::scoped_lock backend_lock(m_backend_mutex);
			m_backend->close();
			return ;
		}
		{
			std::scoped_lock backend_lock(m_backend_mutex);
			m_node = node;
		}
	}

	void close() noexcept
	{
		std::deque<std::shared_ptr<wait_operation>> waiters;
		{
			std::scoped_lock lock(m_event_mutex);
			++m_generation;

			stop_monitor_locked();
			m_subscriptions.clear();
			waiters.swap(m_waiters);

			if( m_event_handle.is_open() )
			{
				std::error_code ignored;
				m_event_handle.close(ignored);
			}
		}
		{
			std::scoped_lock backend_lock(m_backend_mutex);
			if( m_backend )
				m_backend->close();
			m_node = {};
		}
		complete_waiters(std::move(waiters),
			asio::error::make_error_code(asio::error::operation_aborted)
		);
	}

	[[nodiscard]] bool is_open() const noexcept
	{
		std::scoped_lock backend_lock(m_backend_mutex);
		return m_backend and m_backend->is_open();
	}

	[[nodiscard]] riwo::sys_expected<bool> get() const noexcept
	{
		std::scoped_lock backend_lock(m_backend_mutex);
		if( not m_backend or not m_backend->is_open() )
		{
			return riwo::sys_unexpected (
				std::make_error_code(std::errc::bad_file_descriptor)
			);
		}
		std::error_code error;
		const bool result = m_backend->value(error);
		if( error )
			return riwo::sys_unexpected(error);
		return result;
	}

	void set(bool value, std::error_code &error) noexcept
	{
		error.clear();
		std::scoped_lock backend_lock(m_backend_mutex);
		if( not m_backend or not m_backend->is_open() )
		{
			error = std::make_error_code(std::errc::bad_file_descriptor);
			return ;
		}
		if( m_node.direction != direction_t::output )
		{
			error = std::make_error_code(std::errc::operation_not_permitted);
			return ;
		}
		m_backend->set_value(value, error);
	}

	void invert(std::error_code &error) noexcept
	{
		error.clear();
		std::scoped_lock backend_lock(m_backend_mutex);
		if( not m_backend or not m_backend->is_open() )
		{
			error = std::make_error_code(std::errc::bad_file_descriptor);
			return ;
		}
		if( m_node.direction != direction_t::output )
		{
			error = std::make_error_code(std::errc::operation_not_permitted);
			return ;
		}
		const bool current = m_backend->value(error);
		if( not error )
			m_backend->set_value(not current, error);
	}

	[[nodiscard]] executor_t get_executor() noexcept {
		return m_exec;
	}

	[[nodiscard]] node_t node() const
	{
		std::scoped_lock backend_lock(m_backend_mutex);
		return m_node;
	}

	void wait_event(event_t &event, std::error_code &error) noexcept
	{
		event = {};
		error.clear();

		if( auto source_error = validate_event_source() )
		{
			error = source_error;
			return ;
		}
		// A synchronous wait must not depend on somebody else running the bound
		// execution context. Temporarily make this call the sole native event
		// reader; events it consumes are still published to every on_event
		// subscriber and to all asynchronous waiters.
		std::unique_lock reader_lock(m_sync_reader_mutex);
		uint64_t generation = 0;
		{
			std::scoped_lock lock(m_event_mutex);
			generation = m_generation;
			m_sync_reading = true;
			stop_monitor_locked();
		}
		bool received = false;
		for(;;)
		{
			{
				std::scoped_lock lock(m_event_mutex);
				if( generation != m_generation )
				{
					error = asio::error::make_error_code(asio::error::operation_aborted);
					break;
				}
			}
			{
				std::scoped_lock backend_lock(m_backend_mutex);
				if( not m_backend or not m_backend->is_open() )
					error = std::make_error_code(std::errc::bad_file_descriptor);
				else
					received = m_backend->wait_event(event,
						duration_t(50), error);
			}
			if( error or received )
				break;
		}
		{
			std::scoped_lock lock(m_event_mutex);
			if( generation != m_generation )
			{
				event = {};
				error = asio::error::make_error_code(asio::error::operation_aborted);
				received = false;
			}
		}
		if( received )
			publish_event(event);

		else if( error )
		{
			std::deque<std::shared_ptr<wait_operation>> waiters;
			{
				std::scoped_lock lock(m_event_mutex);
				if( generation == m_generation )
					waiters.swap(m_waiters);
			}
			complete_waiters(std::move(waiters), error);
		}
		std::deque<std::shared_ptr<wait_operation>> failed;
		std::error_code restart_error;
		{
			std::scoped_lock lock(m_event_mutex);
			m_sync_reading = false;

			if( generation != m_generation or not error )
			{
				restart_error = start_monitor_locked();
				if( restart_error )
					failed.swap(m_waiters);
			}
		}
		complete_waiters(std::move(failed), restart_error);
	}

	void async_wait_event(event_t &event, event_handler_t handler)
	{
		event = {};
		if( auto error = validate_event_source() )
		{
			post_handler(std::move(handler), error);
			return ;
		}
		std::shared_ptr<wait_operation> waiter;
		try {
			auto allocator = asio::get_associated_allocator(handler);

			using allocator_t = std::allocator_traits
				<decltype(allocator)>::template rebind_alloc<wait_operation>;

			auto io_work = asio::make_work_guard(m_exec);
			auto completion_work = asio::make_work_guard(handler, m_exec);

			waiter = std::allocate_shared<wait_operation>(allocator_t(allocator),
				event, std::move(handler), std::move(io_work), std::move(completion_work)
			);
		}
		catch(...)
		{
			post_handler(std::move(handler),
				riwo::exception_error(std::current_exception())
			);
			return ;
		}
		std::error_code start_error;
		bool queued = false;
		try {
			std::scoped_lock lock(m_event_mutex);
			waiter->id = ++m_next_waiter_id;

			m_waiters.push_back(waiter);
			queued = true;

			if( auto slot = asio::get_associated_cancellation_slot(waiter->completion);
				slot.is_connected() )
			{
				slot.assign([weak = this->weak_from_this(), id = waiter->id]
				(asio::cancellation_type type) noexcept
				{
					if( type == asio::cancellation_type::none )
						return ;

					if( auto self = weak.lock() )
					{
						try {
							auto executor = self->m_event_handle.get_executor();
							riwo::dispatch(std::move(executor), [self = std::move(self), id] {
								self->cancel_waiter(id);
							});
						}
						catch(...) {}
					}
				});
			}
			start_error = start_monitor_locked();
			if( start_error )
			{
				m_waiters.pop_back();
				queued = false;
			}
		}
		catch(...)
		{
			start_error = riwo::exception_error(std::current_exception());
			if( queued )
			{
				std::scoped_lock lock(m_event_mutex);
				const auto iterator = std::ranges::find (
					m_waiters, waiter->id, &wait_operation::id
				);
				if( iterator != m_waiters.end() )
					m_waiters.erase(iterator);
			}
		}
		if( start_error )
			post_waiter(std::move(waiter), start_error);
	}

	void on_event(const event_t &event, on_event_t callback)
	{
		if( event.edge != edge_t::rising and event.edge != edge_t::falling and
			event.edge != edge_t::both )
		{
			riwo::system_error::loc_throw(
				std::make_error_code(std::errc::invalid_argument),
				"altun::subsys::basic_gpio<Exec>::on_event"
			);
		}
		if( auto error = validate_event_source() )
		{
			riwo::system_error::loc_throw(error,
				"altun::subsys::basic_gpio<Exec>::on_event"
			);
		}
		std::scoped_lock lock(m_event_mutex);
		if( not callback )
		{
			std::erase_if(m_subscriptions, [edge = event.edge](const auto &entry) {
				return entry.edge == edge;
			});
			if( not has_consumers_locked() )
				stop_monitor_locked();
			return ;
		}
		m_subscriptions.push_back({event.edge,
			std::make_shared<on_event_t>(std::move(callback))}
		);
		if( auto error = start_monitor_locked() )
		{
			m_subscriptions.pop_back();
			riwo::system_error::loc_throw(error,
				"altun::subsys::basic_gpio<Exec>::on_event"
			);
		}
	}

	template <typename Handler>
	static void wait_event_async
	(executor_t executor, std::shared_ptr<impl> implementation, event_t &event, Handler &&completion_handler)
	{
		event_handler_t handler(std::forward<Handler>(completion_handler));
		if( not implementation )
		{
			event = {};
			if( handler )
			{
				riwo::post_completion(executor, std::move(handler),
					std::make_error_code(std::errc::bad_file_descriptor)
				);
			}
			return ;
		}
		implementation->async_wait_event(event, std::move(handler));
	}

private:
	[[nodiscard]] std::error_code validate_event_source() const noexcept
	{
		std::scoped_lock backend_lock(m_backend_mutex);
		if( not m_backend or not m_backend->is_open() )
			return std::make_error_code(std::errc::bad_file_descriptor);

		if( m_node.direction != direction_t::input or m_node.edge == edge_t::none )
			return std::make_error_code(std::errc::invalid_argument);
		return {};
	}

	[[nodiscard]] bool has_consumers_locked() const noexcept {
		return not m_subscriptions.empty() or not m_waiters.empty();
	}

	[[nodiscard]] asio::posix::descriptor_base::wait_type wait_type() const noexcept
	{
		return m_backend->event_wait_type() == detail::gpio_event_wait_t::priority ?
			asio::posix::descriptor_base::wait_error :
			asio::posix::descriptor_base::wait_read;
	}

	[[nodiscard]] std::error_code start_monitor_locked() noexcept
	{
		if( m_monitoring or m_sync_reading or not has_consumers_locked() )
			return {};

		if( not m_event_handle.is_open() )
			return std::make_error_code(std::errc::bad_file_descriptor);

		m_monitoring = true;
		const auto generation = m_generation;
		const auto ticket = ++m_monitor_ticket;
		try {
			m_event_handle.async_wait(wait_type(),
			[self = this->shared_from_this(), generation, ticket](std::error_code error) mutable {
				self->event_ready(generation, ticket, error);
			});
		}
		catch(...)
		{
			m_monitoring = false;
			return riwo::exception_error(std::current_exception());
		}
		return {};
	}

	void stop_monitor_locked() noexcept
	{
		if( not m_monitoring )
			return ;

		m_monitoring = false;
		++m_monitor_ticket;

		std::error_code ignored;
		m_event_handle.cancel(ignored);
	}

	void event_ready(uint64_t generation, uint64_t ticket, std::error_code wait_error) noexcept
	{
		{
			std::scoped_lock lock(m_event_mutex);
			if( generation != m_generation or ticket != m_monitor_ticket or
				not m_monitoring )
				return ;
		}
		if( wait_error )
		{
			{
				std::scoped_lock lock(m_event_mutex);
				if( generation == m_generation and ticket == m_monitor_ticket )
					m_monitoring = false;
			}
			if( wait_error != asio::error::operation_aborted )
				fail_waiters(wait_error);
			return ;
		}

		for(;;)
		{
			{
				std::scoped_lock lock(m_event_mutex);
				if( generation != m_generation or ticket != m_monitor_ticket or
					not m_monitoring or not has_consumers_locked() )
					break;
			}
			event_t event;
			std::error_code read_error;
			bool received = false;
			{
				std::scoped_lock backend_lock(m_backend_mutex);
				{
					std::scoped_lock lock(m_event_mutex);
					if( generation != m_generation or ticket != m_monitor_ticket or
						not m_monitoring )
						break;
				}
				received = m_backend->wait_event(event, duration_t::zero(), read_error);
			}
			if( read_error )
			{
				{
					std::scoped_lock lock(m_event_mutex);
					if( generation == m_generation and ticket == m_monitor_ticket )
						m_monitoring = false;
				}
				fail_waiters(read_error);
				return ;
			}
			if( not received )
				break;

			publish_event(event);
		}
		std::deque<std::shared_ptr<wait_operation>> failed;
		std::error_code restart_error;
		{
			std::scoped_lock lock(m_event_mutex);
			if( generation == m_generation and ticket == m_monitor_ticket and
				m_monitoring )
			{
				m_monitoring = false;
				restart_error = start_monitor_locked();

				if( restart_error )
					failed.swap(m_waiters);
			}
		}
		complete_waiters(std::move(failed), restart_error);
	}

	void publish_event(const event_t &event) noexcept
	{
		std::vector<std::shared_ptr<on_event_t>> callbacks;
		std::deque<std::shared_ptr<wait_operation>> waiters;
		try {
			std::scoped_lock lock(m_event_mutex);
			for(const auto &subscription : m_subscriptions)
			{
				if( subscription.edge == edge_t::both or subscription.edge == event.edge )
					callbacks.push_back(subscription.callback);
			}
			waiters.swap(m_waiters);
		}
		catch(...)
		{
			fail_waiters(riwo::exception_error(std::current_exception()));
			return ;
		}
		for(auto &waiter : waiters)
		{
			if( waiter->destination )
				*waiter->destination = event;
			complete_waiter(std::move(waiter), {});
		}
		for(auto &callback : callbacks)
		{
			try {
				riwo::post(m_callback_exec,
				[callback = std::move(callback), event]() mutable noexcept
				{
					try {
						std::invoke(*callback, event);
					}
					catch(...) {}
				});
			}
			catch(...) {}
		}
	}

	void cancel_waiter(uint64_t id) noexcept
	{
		std::shared_ptr<wait_operation> cancelled;
		{
			std::scoped_lock lock(m_event_mutex);
			const auto iterator = std::ranges::find(m_waiters, id, &wait_operation::id);
			if( iterator == m_waiters.end() )
				return ;

			cancelled = std::move(*iterator);
			m_waiters.erase(iterator);

			if( not has_consumers_locked() )
				stop_monitor_locked();
		}
		complete_waiter(std::move(cancelled),
			asio::error::make_error_code(asio::error::operation_aborted));
	}

	void fail_waiters(std::error_code error) noexcept
	{
		std::deque<std::shared_ptr<wait_operation>> waiters;
		{
			std::scoped_lock lock(m_event_mutex);
			waiters.swap(m_waiters);
		}
		complete_waiters(std::move(waiters), error);
	}

	void post_handler(event_handler_t handler, std::error_code error)
	{
		if( handler )
			riwo::post_completion(m_exec, std::move(handler), error);
	}

	void post_waiter
	(std::shared_ptr<wait_operation> waiter, std::error_code error)
	{
		if( not waiter )
			return ;

		if( auto slot = asio::get_associated_cancellation_slot(waiter->completion);
			slot.is_connected() )
			slot.clear();

		post_handler(std::move(waiter->completion), error);
	}

	static void complete_waiter
	(std::shared_ptr<wait_operation> waiter, std::error_code error) noexcept
	{
		if( not waiter )
			return ;

		if( auto slot = asio::get_associated_cancellation_slot(waiter->completion);
			slot.is_connected() )
			slot.clear();
		try {
			auto executor = waiter->completion_work.get_executor();
			auto allocator = asio::get_associated_allocator(waiter->completion);

			asio::dispatch(executor, asio::bind_allocator(allocator,
			[waiter = std::move(waiter), error]() mutable
			{
				try {
					if( waiter->completion )
						std::move(waiter->completion)(error);
				}
				catch(...) {}
			}));
		}
		catch(...) {}
	}

	static void complete_waiters
	(std::deque<std::shared_ptr<wait_operation>> waiters, std::error_code error) noexcept
	{
		for(auto &waiter : waiters)
			complete_waiter(std::move(waiter), error);
	}

public:
	std::unique_ptr<detail::gpio_backend> m_backend {};
	node_t m_node {};

private:
	executor_t m_exec;
	event_handle_t m_event_handle;
	callback_executor_t m_callback_exec;

	std::mutex m_event_mutex;
	mutable std::mutex m_backend_mutex;
	std::mutex m_sync_reader_mutex;

	std::vector<event_subscription> m_subscriptions;
	std::deque<std::shared_ptr<wait_operation>> m_waiters;

	uint64_t m_next_waiter_id = 0;
	uint64_t m_generation = 0;
	uint64_t m_monitor_ticket = 0;

	bool m_monitoring = false;
	bool m_sync_reading = false;
};

template <riwo::concepts::exec Exec>
template <typename Exec0>
basic_gpio<Exec>::basic_gpio(const node_t &node, Exec0 &&exec) requires (
	not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio> and
	riwo::concepts::match_sched<Exec0,executor_t>
) : m_impl(std::make_shared<impl>(std::forward<decltype(exec)>(exec)))
{
	std::error_code error;
	m_impl->open(node, error);
	if( error )
		riwo::system_error::loc_throw(error, "altun::subsys::basic_gpio<Exec>::open");
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec>::basic_gpio(const node_t &node)
	requires riwo::concepts::match_def_exec<Exec> :
	basic_gpio(node, riwo::io_context())
{

}

template <riwo::concepts::exec Exec>
template <typename Exec0>
basic_gpio<Exec>::basic_gpio(Exec0 &&exec) requires (
	not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio> and
	riwo::concepts::match_sched<Exec0,executor_t>
) : m_impl(std::make_shared<impl>(std::forward<decltype(exec)>(exec)))
{

}

template <riwo::concepts::exec Exec>
basic_gpio<Exec>::basic_gpio()
	requires riwo::concepts::match_def_exec<Exec> :
	basic_gpio(riwo::io_context())
{

}

template <riwo::concepts::exec Exec>
basic_gpio<Exec>::~basic_gpio()
{
	if( m_impl )
		m_impl->close();
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec>::basic_gpio(basic_gpio &&other) noexcept :
	m_impl(std::move(other.m_impl))
{
	other.m_impl = std::make_shared<impl>(m_impl->get_executor());
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec> &basic_gpio<Exec>::operator=(basic_gpio &&other) noexcept
{
	if( this == &other )
		return *this;

	if( m_impl )
		m_impl->close();

	m_impl = std::move(other.m_impl);
	other.m_impl = std::make_shared<impl>(m_impl->get_executor());
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec> &basic_gpio<Exec>::open(const node_t &node, std::error_code &error) noexcept
{
	if( not m_impl )
	{
		error = std::make_error_code(std::errc::bad_file_descriptor);
		return *this;
	}
	m_impl->open(node, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec> &basic_gpio<Exec>::open(const node_t &node)
{
	std::error_code error;
	open(node, error);
	if( error )
		riwo::system_error::loc_throw(error, "altun::subsys::basic_gpio<Exec>::open");
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec> &basic_gpio<Exec>::close() noexcept
{
	if( m_impl )
		m_impl->close();
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec> &basic_gpio<Exec>::set(bool value, std::error_code &error) noexcept
{
	if( not m_impl )
		error = std::make_error_code(std::errc::bad_file_descriptor);
	else
		m_impl->set(value, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec> &basic_gpio<Exec>::set(bool value)
{
	std::error_code error;
	set(value, error);
	if( error )
		riwo::system_error::loc_throw(error, "altun::subsys::basic_gpio<Exec>::set");
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec> &basic_gpio<Exec>::rising(std::error_code &error) noexcept
{
	return set(true, error);
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec> &basic_gpio<Exec>::rising()
{
	return set(true);
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec> &basic_gpio<Exec>::falling(std::error_code &error) noexcept
{
	return set(false, error);
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec> &basic_gpio<Exec>::falling()
{
	return set(false);
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec> &basic_gpio<Exec>::invert(std::error_code &error) noexcept
{
	if( not m_impl )
		error = std::make_error_code(std::errc::bad_file_descriptor);
	else
		m_impl->invert(error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec> &basic_gpio<Exec>::invert()
{
	std::error_code error;
	invert(error);
	if( error )
		riwo::system_error::loc_throw(error, "altun::subsys::basic_gpio<Exec>::invert");
	return *this;
}

template <riwo::concepts::exec Exec>
riwo::sys_expected<bool> basic_gpio<Exec>::get() const noexcept
{
	if( not m_impl )
	{
		return riwo::sys_unexpected (
			std::make_error_code(std::errc::bad_file_descriptor)
		);
	}
	return m_impl->get();
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec>::operator bool() const
{
	return riwo::expected_value_or_throw(get());
}

template <riwo::concepts::exec Exec>
bool basic_gpio<Exec>::operator*() const
{
	return riwo::expected_value_or_throw(get());
}

template <riwo::concepts::exec Exec>
template <typename Token>
auto basic_gpio<Exec>::wait_event(event_t &event, Token &&token)
	requires event_token_v<Token>
{
	if constexpr( riwo::is_error_code_token_v<Token> )
	{
		if( m_impl )
			m_impl->wait_event(event, token);
		else
		{
			event = {};
			token = std::make_error_code(std::errc::bad_file_descriptor);
		}
	}
	else if constexpr( riwo::is_sync_opt_token_v<Token> )
	{
		std::error_code error;
		if( m_impl )
			m_impl->wait_event(event, error);
		else
		{
			event = {};
			error = std::make_error_code(std::errc::bad_file_descriptor);
		}
		if( error )
		{
			riwo::system_error::loc_throw(error,
				"altun::subsys::basic_gpio<Exec>::wait_event"
			);
		}
	}
	else
	{
		return riwo::initiate_io_void(get_executor(),
			[executor = get_executor(), implementation = m_impl, destination = &event]
			<typename Handler>(Handler &&completion_handler) mutable
			{
				impl::wait_event_async(std::move(executor),
					std::move(implementation), *destination, std::forward<Handler>(completion_handler)
				);
			},
			std::forward<Token>(token));
	}
}

template <riwo::concepts::exec Exec>
basic_gpio<Exec> &basic_gpio<Exec>::on_event(const event_t &event, on_event_t callback)
{
	if( not m_impl )
	{
		riwo::system_error::loc_throw (
			std::make_error_code(std::errc::bad_file_descriptor),
			"altun::subsys::basic_gpio<Exec>::on_event"
		);
	}
	m_impl->on_event(event, std::move(callback));
	return *this;
}

template <riwo::concepts::exec Exec>
auto basic_gpio<Exec>::node() const -> node_t
{
	return m_impl ? m_impl->node() : node_t{};
}

template <riwo::concepts::exec Exec>
bool basic_gpio<Exec>::is_open() const noexcept
{
	return m_impl and m_impl->is_open();
}

template <riwo::concepts::exec Exec>
auto basic_gpio<Exec>::backend() noexcept -> backend_t
{
	return detail::gpio_backend_type();
}

template <riwo::concepts::exec Exec>
const char *basic_gpio<Exec>::backend_name() noexcept
{
	return detail::gpio_backend_name();
}

template <riwo::concepts::exec Exec>
auto basic_gpio<Exec>::get_executor() noexcept -> executor_t
{
	return m_impl->get_executor();
}

} //namespace altun::subsys


#endif //__linux__
#endif //ALTUN_LINUX_SUBSYS_DETAIL_GPIO_H
