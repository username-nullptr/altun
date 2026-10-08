// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "dbus_interface.h"
#if LIBEMPP_SBUS_DBUS_SUPPORT

#include "log.h"
#include <dbus/dbus.h>
#include <shared_mutex>

namespace libempp::sbus
{

RIWO_DECL_HIDDEN void bridge_dbus_data_available (
	std::string_view, const void*, size_t
);
namespace
{

constexpr auto bus_path = "/org/libempp/SBus";
constexpr auto bus_interface = "org.libempp.SBus";
constexpr auto bus_member = "Message";

constexpr auto bus_match =
	"type='signal',path='/org/libempp/SBus',"
	"interface='org.libempp.SBus',member='Message'";

using topic_callback = std::function<void(const void*,size_t)>;
using global_callback = std::function<void(std::string_view,const void*,size_t)>;

struct outgoing_message
{
	std::string topic;
	std::vector<std::byte> payload;

	[[nodiscard]] size_t storage_size() const noexcept {
		return topic.size() + payload.size();
	}
};

constexpr size_t send_queue_max_messages = 256;
constexpr size_t send_queue_max_bytes = 32 * 1'024 * 1'024;
constexpr size_t send_batch_max_messages = 64;

struct transparent_string_hash
{
	using is_transparent = void;

	[[nodiscard]] size_t operator()(std::string_view value) const noexcept {
		return std::hash<std::string_view>{}(value);
	}

	[[nodiscard]] size_t operator()(const std::string &value) const noexcept {
		return operator()(std::string_view(value));
	}
};

class runtime;
runtime &bus_runtime();

std::mutex g_interfaces_mutex {};
std::unordered_map<dbus_interface*,std::shared_ptr<dbus_interface>> g_interfaces {};

using interface_list = std::vector<std::shared_ptr<dbus_interface>>;
using interface_list_ptr = std::shared_ptr<const interface_list>;

std::atomic g_interface_snapshot {
	std::make_shared<const interface_list>()
};

void rebuild_interface_snapshot()
{
	auto snapshot = std::make_shared<interface_list>();
	snapshot->reserve(g_interfaces.size());
	for(const auto &[pointer, object] : g_interfaces)
	{
		RIWO_UNUSED(pointer);
		snapshot->emplace_back(object);
	}
	g_interface_snapshot.store(std::move(snapshot), std::memory_order_release);
}

class runtime
{
	RIWO_DISABLE_COPY_MOVE(runtime)

public:
	runtime()
	{
		if( not dbus_threads_init_default() )
		{
			libempp_clog_error("LibEMpp.Core",
				"libempp.sbus.dbus: failed to initialize D-Bus threading"
			);
			return ;
		}
		DBusError error;
		dbus_error_init(&error);

		m_receive_connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
		if( not m_receive_connection )
		{
			log_error("create the D-Bus receive connection", error);
			dbus_error_free(&error);
			return ;
		}
		dbus_connection_set_exit_on_disconnect(m_receive_connection, false);
		dbus_bus_add_match(m_receive_connection, bus_match, &error);

		if( dbus_error_is_set(&error) )
		{
			log_error("install the signal match", error);
			dbus_error_free(&error);
			close_connection(m_receive_connection);
			return ;
		}
		dbus_connection_flush(m_receive_connection);

		// A blocking read owns libdbus's connection lock. Keep publishing on a
		// second connection so senders never queue behind the receive poll.
		m_send_connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
		if( not m_send_connection )
		{
			log_error("create the D-Bus send connection", error);
			dbus_error_free(&error);
			close_connection(m_receive_connection);
			return ;
		}
		dbus_connection_set_exit_on_disconnect(m_send_connection, false);
		if( const char *name = dbus_bus_get_unique_name(m_send_connection) )
			m_send_name = name;
		else
		{
			libempp_clog_error("LibEMpp.Core",
				"libempp.sbus.dbus: send connection has no unique bus name"
			);
			close_connection(m_send_connection);
			close_connection(m_receive_connection);
			return ;
		}

		m_send_run = true;
		m_receive_run.store(true, std::memory_order_release);
		m_send_thread = std::thread([this]() noexcept
		{
			try
			{
				send_messages();
			}
			catch(const std::exception &exception)
			{
				stop_sender();
				libempp_clog_error("LibEMpp.Core",
					"libempp.sbus.dbus: send worker failed: {}", exception
				);
			}
			catch(...)
			{
				stop_sender();
				libempp_clog_error("LibEMpp.Core",
					"libempp.sbus.dbus: send worker failed with an unknown exception"
				);
			}
		});
		m_receive_thread = std::thread([this] { receive_messages(); });
	}

	~runtime()
	{
		{
			std::lock_guard lock(m_send_mutex);
			m_send_run = false;
		}
		m_send_changed.notify_all();
		if( m_send_thread.joinable() )
			m_send_thread.join();

		m_receive_run.store(false, std::memory_order_release);
		if( m_receive_thread.joinable() )
			m_receive_thread.join();

		close_connection(m_send_connection);
		close_connection(m_receive_connection);
	}

	void publish(std::string_view topic, const void *buffer, size_t size) noexcept
	{
		if( not m_send_connection )
			return ;

		if( topic.size() > static_cast<size_t>(INT_MAX) or size > static_cast<size_t>(INT_MAX) )
		{
			libempp_clog_error("LibEMpp.Core",
				"libempp.sbus.dbus: topic or payload exceeds the D-Bus array limit"
			);
			return ;
		}
		if( size != 0 and not buffer )
		{
			libempp_clog_error("LibEMpp.Core",
				"libempp.sbus.dbus: a non-empty payload has a null buffer"
			);
			return ;
		}
		try
		{
			outgoing_message message;
			message.topic = topic;

			if( size != 0 )
			{
				auto *begin = static_cast<const std::byte*>(buffer);
				message.payload.assign(begin, begin + size);
			}
			enqueue(std::move(message));
		}
		catch(const std::exception &exception)
		{
			libempp_clog_error("LibEMpp.Core",
				"libempp.sbus.dbus: failed to queue a signal: {}", exception
			);
		}
	}

private:
	void enqueue(outgoing_message message)
	{
		const auto storage_size = message.storage_size();
		std::unique_lock lock(m_send_mutex);

		m_send_space.wait(lock, [this, storage_size]
		{
			if( not m_send_run )
				return true;

			if( m_send_queue.size() >= send_queue_max_messages )
				return false;

			if( storage_size > send_queue_max_bytes )
				return m_send_queue.empty();

			return m_send_queue_bytes <= send_queue_max_bytes - storage_size;
		});
		if( not m_send_run )
			return ;

		m_send_queue_bytes += storage_size;
		m_send_queue.emplace_back(std::move(message));

		lock.unlock();
		m_send_changed.notify_one();
	}

	static void close_connection(DBusConnection *&connection) noexcept
	{
		if( not connection )
			return ;
		dbus_connection_close(connection);
		dbus_connection_unref(connection);
		connection = nullptr;
	}

	static void log_error(std::string_view action, const DBusError &error) noexcept
	{
		libempp_clog_error("LibEMpp.Core",
			"libempp.sbus.dbus: failed to {}: {}",
			action, error.message ? error.message : "unknown D-Bus error"
		);
	}

	static bool append_bytes(DBusMessageIter &arguments, const void *data, int size) noexcept
	{
		DBusMessageIter array;
		if( not dbus_message_iter_open_container
			(&arguments, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &array) )
			return false;

		if( size != 0 )
		{
			auto *bytes = static_cast<const unsigned char*>(data);
			if( not dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE, &bytes, size) )
			{
				dbus_message_iter_abandon_container(&arguments, &array);
				return false;
			}
		}
		return dbus_message_iter_close_container(&arguments, &array);
	}

	static DBusMessage *encode_message(const outgoing_message &value) noexcept
	{
		auto *message = dbus_message_new_signal(bus_path, bus_interface, bus_member);
		if( not message )
			return nullptr;

		DBusMessageIter arguments;
		dbus_message_iter_init_append(message, &arguments);

		const auto valid =
			append_bytes(arguments, value.topic.data(), static_cast<int>(value.topic.size())) and
			append_bytes(arguments, value.payload.data(), static_cast<int>(value.payload.size()));

		if( valid )
			return message;

		dbus_message_unref(message);
		return nullptr;
	}

	static bool read_bytes(DBusMessageIter &arguments, const unsigned char *&data, int &size) noexcept
	{
		if( dbus_message_iter_get_arg_type(&arguments) != DBUS_TYPE_ARRAY or
			dbus_message_iter_get_element_type(&arguments) != DBUS_TYPE_BYTE )
			return false;

		DBusMessageIter array;
		dbus_message_iter_recurse(&arguments, &array);

		void *value = nullptr;
		dbus_message_iter_get_fixed_array(&array, &value, &size);

		data = static_cast<const unsigned char*>(value);
		return size >= 0;
	}

	void stop_sender() noexcept
	{
		{
			std::lock_guard lock(m_send_mutex);
			m_send_run = false;
			m_send_queue.clear();
			m_send_queue_bytes = 0;
		}
		m_send_changed.notify_all();
		m_send_space.notify_all();
	}

	void send_messages()
	{
		std::vector<outgoing_message> batch;
		batch.reserve(send_batch_max_messages);
		for(;;)
		{
			{
				std::unique_lock lock(m_send_mutex);
				m_send_changed.wait(lock, [this] {
					return not m_send_run or not m_send_queue.empty();
				});
				if( m_send_queue.empty() and not m_send_run )
					break;

				batch.clear();
				const auto count = std::min(
					send_batch_max_messages, m_send_queue.size()
				);
				for(size_t index = 0; index < count; ++index)
				{
					m_send_queue_bytes -= m_send_queue.front().storage_size();
					batch.emplace_back(std::move(m_send_queue.front()));
					m_send_queue.pop_front();
				}
			}
			m_send_space.notify_all();

			bool sent = false;
			for(const auto &value : batch)
			{
				auto *message = encode_message(value);
				if( not message )
				{
					libempp_clog_error("LibEMpp.Core",
						"libempp.sbus.dbus: failed to encode a signal"
					);
					continue;
				}
				if( dbus_connection_send(m_send_connection, message, nullptr) )
					sent = true;
				else
				{
					libempp_clog_error("LibEMpp.Core",
						"libempp.sbus.dbus: failed to send a signal"
					);
				}
				dbus_message_unref(message);
			}
			if( sent )
				dbus_connection_flush(m_send_connection);
		}
	}

	void receive_messages() noexcept
	{
		while( m_receive_run.load(std::memory_order_acquire) )
		{
			if( not dbus_connection_read_write(m_receive_connection, 50) )
				break;

			while( auto *message = dbus_connection_pop_message(m_receive_connection) )
			{
				handle_message(message);
				dbus_message_unref(message);
			}
		}
	}

	void handle_message(DBusMessage *message) noexcept
	{
		if( const char *path = dbus_message_get_path(message);
			not dbus_message_is_signal(message, bus_interface, bus_member) or
			std::string_view(path ? path : "") != bus_path )
			return ;

		// publish() already delivers to local subscribers. Ignore the copy from
		// this runtime's dedicated send connection so local delivery stays exact.
		const char *sender = dbus_message_get_sender(message);

		if( sender and std::string_view(sender) == m_send_name )
			return ;

		DBusMessageIter arguments;
		if( not dbus_message_iter_init(message, &arguments) )
			return ;

		const unsigned char *topic_data = nullptr;
		int topic_size = 0;

		if( not read_bytes(arguments, topic_data, topic_size) or
			not dbus_message_iter_next(&arguments) )
			return ;

		const unsigned char *payload = nullptr;
		int payload_size = 0;

		if( not read_bytes(arguments, payload, payload_size) or
			dbus_message_iter_next(&arguments) )
			return ;

		const std::string_view topic = topic_size == 0 ? std::string_view{} :
			std::string_view(reinterpret_cast<const char*>(topic_data), topic_size);

		bridge_dbus_data_available(topic, payload, static_cast<size_t>(payload_size));
	}

	DBusConnection *m_receive_connection = nullptr;
	DBusConnection *m_send_connection = nullptr;
	std::string m_send_name {};
	std::atomic_bool m_receive_run {false};
	std::thread m_receive_thread {};

	bool m_send_run = false;
	std::deque<outgoing_message> m_send_queue {};

	size_t m_send_queue_bytes = 0;
	std::thread m_send_thread {};
	std::mutex m_send_mutex {};

	std::condition_variable m_send_changed {};
	std::condition_variable m_send_space {};
};

runtime &bus_runtime()
{
	static runtime value;
	return value;
}

}} //namespace libempp::sbus::<anonymous>

namespace libempp::sbus
{

class RIWO_DECL_HIDDEN dbus_interface::impl
{
	RIWO_DISABLE_COPY_MOVE(impl)

public:
	using topic_subscribers = std::unordered_map<uint64_t,topic_callback>;
	using topic_callback_list = std::vector<topic_callback>;
	using topic_callback_list_ptr = std::shared_ptr<const topic_callback_list>;
	using global_callback_list = std::vector<global_callback>;
	using global_callback_list_ptr = std::shared_ptr<const global_callback_list>;

	using topic_map = std::unordered_map <
		std::string, topic_subscribers, transparent_string_hash, std::equal_to<>
	>;
	using topic_snapshot_map = std::unordered_map <
		std::string, topic_callback_list_ptr, transparent_string_hash, std::equal_to<>
	>;
	impl() = default;

	[[nodiscard]] bool empty() const noexcept {
		return topic_callbacks.empty() and global_callbacks.empty();
	}

	void rebuild_topic_snapshot(std::string_view topic)
	{
		auto source = topic_callbacks.find(topic);
		if( source == topic_callbacks.end() )
		{
			if( auto snapshot = topic_snapshots.find(topic);
				snapshot != topic_snapshots.end() )
				topic_snapshots.erase(snapshot);
			return ;
		}
		auto snapshot = std::make_shared<topic_callback_list>();
		snapshot->reserve(source->second.size());
		for(const auto &[sid, callback] : source->second)
		{
			RIWO_UNUSED(sid);
			snapshot->emplace_back(callback);
		}
		topic_snapshots.insert_or_assign(source->first, std::move(snapshot));
	}

	void rebuild_global_snapshot()
	{
		auto snapshot = std::make_shared<global_callback_list>();
		snapshot->reserve(global_callbacks.size());
		for(const auto &[sid, callback] : global_callbacks)
		{
			RIWO_UNUSED(sid);
			snapshot->emplace_back(callback);
		}
		global_snapshot = std::move(snapshot);
	}
	std::shared_mutex mutex {};

	uint64_t next_sid = 1;
	topic_map topic_callbacks {};
	topic_snapshot_map topic_snapshots {};

	std::unordered_map<uint64_t,std::string> topics_by_sid {};
	std::unordered_map<uint64_t,global_callback> global_callbacks {};

	global_callback_list_ptr global_snapshot =
		std::make_shared<const global_callback_list>();
};

void bridge_dbus_data_available(std::string_view topic, const void *data, size_t size)
{
	auto interfaces = g_interface_snapshot.load(std::memory_order_acquire);
	for(const auto &interface : *interfaces)
	{
		dbus_interface::impl::topic_callback_list_ptr topic_callbacks;
		dbus_interface::impl::global_callback_list_ptr global_callbacks;
		{
			std::shared_lock lock(interface->m_impl->mutex);
			if( auto it = interface->m_impl->topic_snapshots.find(topic);
				it != interface->m_impl->topic_snapshots.end() )
				topic_callbacks = it->second;
			global_callbacks = interface->m_impl->global_snapshot;
		}
		if( topic_callbacks )
		{
			for(const auto &callback : *topic_callbacks)
				callback(data, size);
		}

		for(const auto &callback : *global_callbacks)
			callback(topic, data, size);
	}
}

dbus_interface::dbus_interface() :
	m_impl(std::make_unique<impl>())
{

}

dbus_interface::~dbus_interface() = default;

void dbus_interface::init()
{
	RIWO_UNUSED(bus_runtime());
}

void dbus_interface::publish(std::string_view topic, const void *buffer, size_t size)
{
	if( size != 0 and not buffer )
	{
		libempp_clog_error("LibEMpp.Core",
			"libempp.sbus.dbus: a non-empty payload has a null buffer"
		);
		return ;
	}
	bridge_dbus_data_available(topic, buffer, size);
	bus_runtime().publish(topic, buffer, size);
}

uint64_t dbus_interface::subscribe(std::string_view topic, topic_callback func)
{
	std::lock_guard lock(m_impl->mutex);
	const auto sid = m_impl->next_sid++;

	m_impl->topic_callbacks[std::string(topic)].emplace(sid, std::move(func));
	m_impl->topics_by_sid.emplace(sid, topic);
	m_impl->rebuild_topic_snapshot(topic);
	{
		std::lock_guard interfaces_lock(g_interfaces_mutex);
		const auto [position, inserted] =
			g_interfaces.insert_or_assign(this, shared_from_this());
		RIWO_UNUSED(position);
		if( inserted )
			rebuild_interface_snapshot();
	}
	return sid;
}

uint64_t dbus_interface::subscribe(global_callback callback)
{
	std::lock_guard lock(m_impl->mutex);
	const auto sid = m_impl->next_sid++;

	m_impl->global_callbacks.emplace(sid, std::move(callback));
	m_impl->rebuild_global_snapshot();
	{
		std::lock_guard interfaces_lock(g_interfaces_mutex);
		const auto [position, inserted] =
			g_interfaces.insert_or_assign(this, shared_from_this());
		RIWO_UNUSED(position);
		if( inserted )
			rebuild_interface_snapshot();
	}
	return sid;
}

void dbus_interface::cancel_topic(std::string_view topic)
{
	std::lock_guard lock(m_impl->mutex);
	if( auto it = m_impl->topic_callbacks.find(topic);
		it != m_impl->topic_callbacks.end() )
	{
		for(const auto &[sid, callback] : it->second)
		{
			RIWO_UNUSED(callback);
			m_impl->topics_by_sid.erase(sid);
		}
		m_impl->topic_callbacks.erase(it);
		m_impl->rebuild_topic_snapshot(topic);
	}
	if( m_impl->empty() )
	{
		std::lock_guard interfaces_lock(g_interfaces_mutex);
		if( g_interfaces.erase(this) > 0 )
			rebuild_interface_snapshot();
	}
}

void dbus_interface::cancel_sid(uint64_t sid)
{
	std::lock_guard lock(m_impl->mutex);
	if( m_impl->global_callbacks.erase(sid) == 0 )
	{
		if( auto topic_it = m_impl->topics_by_sid.find(sid);
			topic_it != m_impl->topics_by_sid.end() )
		{
			const auto topic = std::move(topic_it->second);
			m_impl->topics_by_sid.erase(topic_it);

			if( auto it = m_impl->topic_callbacks.find(topic);
				it != m_impl->topic_callbacks.end() )
			{
				it->second.erase(sid);
				if( it->second.empty() )
					m_impl->topic_callbacks.erase(it);
				m_impl->rebuild_topic_snapshot(topic);
			}
		}
	}
	else
		m_impl->rebuild_global_snapshot();
	if( m_impl->empty() )
	{
		std::lock_guard interfaces_lock(g_interfaces_mutex);
		if( g_interfaces.erase(this) > 0 )
			rebuild_interface_snapshot();
	}
}

void dbus_interface::cancel()
{
	std::lock_guard lock(m_impl->mutex);
	m_impl->topic_callbacks.clear();
	m_impl->topic_snapshots.clear();
	m_impl->topics_by_sid.clear();
	m_impl->global_callbacks.clear();
	m_impl->rebuild_global_snapshot();

	std::lock_guard interfaces_lock(g_interfaces_mutex);
	if( g_interfaces.erase(this) > 0 )
		rebuild_interface_snapshot();
}

} //namespace libempp::sbus

#endif //LIBEMPP_SBUS_DBUS_SUPPORT
