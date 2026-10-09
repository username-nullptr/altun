// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "cyclone_interface.h"
#if ALTUN_SBUS_CYCLONE_SUPPORT

#include "cyclone_message.h"
#include "log.h"

#include <riwo/core/lock_free_queue.h>
#include <riwo/core/shared_mutex.h>
#include <riwo/core/execution.h>

#include <dds/version.h>
#include <dds/dds.h>

#include <unordered_set>
#include <limits>

namespace altun::sbus { namespace
{

using payload_buffer_t = std::vector<std::byte>;
using shared_payload_t = std::shared_ptr<const payload_buffer_t>;

struct RIWO_DECL_HIDDEN transparent_string_hash
{
	using is_transparent = void;

	[[nodiscard]] size_t operator()(std::string_view value) const noexcept {
		return std::hash<std::string_view>{}(value);
	}
	[[nodiscard]] size_t operator()(const std::string &value) const noexcept {
		return operator()(std::string_view(value));
	}
};

class RIWO_DECL_HIDDEN payload_t
{
public:
	payload_t(const void *data, size_t size) :
		m_shared(false)
	{
		if( size == 0 )
			new (&m_storage.owned) payload_buffer_t();
		else
		{
			auto begin = static_cast<const std::byte*>(data);
			new (&m_storage.owned) payload_buffer_t(begin, begin + size);
		}
	}

	explicit payload_t(shared_payload_t payload) noexcept :
		m_shared(true)
	{
		new (&m_storage.shared) shared_payload_t(std::move(payload));
	}

	payload_t(const payload_t &other) :
		m_shared(other.m_shared)
	{
		if( m_shared )
			new (&m_storage.shared) shared_payload_t(other.m_storage.shared);
		else
			new (&m_storage.owned) payload_buffer_t(other.m_storage.owned);
	}

	payload_t(payload_t &&other) noexcept :
		m_shared(other.m_shared)
	{
		if( m_shared )
			new (&m_storage.shared) shared_payload_t(std::move(other.m_storage.shared));
		else
			new (&m_storage.owned) payload_buffer_t(std::move(other.m_storage.owned));
	}

	~payload_t()
	{
		if( m_shared )
			m_storage.shared.~shared_payload_t();
		else
			m_storage.owned.~payload_buffer_t();
	}

	[[nodiscard]] const std::byte *data() const noexcept {
		return m_shared ? m_storage.shared->data() : m_storage.owned.data();
	}

	[[nodiscard]] size_t size() const noexcept {
		return m_shared ? m_storage.shared->size() : m_storage.owned.size();
	}

private:
	union storage_t
	{
		storage_t() noexcept {}
		~storage_t() {}

		payload_buffer_t owned;
		shared_payload_t shared;
	}
	m_storage;
	bool m_shared;
};

static_assert(sizeof(payload_t) <= sizeof(payload_buffer_t) + sizeof(void*));

constexpr size_t g_queue_max_size = 128;
constexpr size_t g_shared_payload_threshold = 64 * 1'024;

[[noreturn]] void uncaught_exception(const std::exception &ex) noexcept
{
	altun_clog_critical("Altun.Core", "Uncaught exception: {}", ex);
	riwo::forced_termination();
}

class /* RIWO_DECL_HIDDEN */ subscriber_thread
{
	RIWO_DISABLE_COPY_MOVE(subscriber_thread)

protected:
	subscriber_thread() = default;

	void start(std::function<void()> task_arg)
	{
		m_run.store(true, std::memory_order_release);
		m_thread = std::thread([this, task = std::move(task_arg)]() mutable noexcept
		{
			try {
				do_task(task);
			}
			catch(const std::exception &ex) {
				uncaught_exception(ex);
			}
		});
	}

	void notify() noexcept
	{
		// A monotonic generation cannot be cleared over a concurrent enqueue.
		m_epoch.fetch_add(1, std::memory_order_release);
		std::atomic_notify_one(&m_epoch);
	}

public:
	virtual ~subscriber_thread() {
		stop();
	}

protected:
	void stop() noexcept
	{
		m_run.store(false, std::memory_order_release);
		notify();
		if( m_thread.joinable() )
			m_thread.join();
	}

private:
	void do_task(const std::function<void()> &task)
	{
		uint64_t observed_epoch = 0;
		while( m_run.load(std::memory_order_acquire) )
		{
			while( m_epoch.load(std::memory_order_acquire) == observed_epoch )
			{
				std::atomic_wait_explicit (
					&m_epoch, observed_epoch, std::memory_order_acquire
				);
			}
			if( not m_run.load(std::memory_order_acquire) )
				break;
			do {
				observed_epoch = m_epoch.load(std::memory_order_acquire);
				task();
			}
			while( m_epoch.load(std::memory_order_acquire) != observed_epoch and
				m_run.load(std::memory_order_acquire) );
		}
	}

	alignas(64) std::atomic_uint64_t m_epoch {0};
	alignas(64) std::atomic_bool m_run {false};
	/*
	 * The support for std::jthread by clang requires at least version 20.
	 * So, it is still advisable to use the traditional std::thread.
	 */
	std::thread m_thread {};
};

class /* RIWO_DECL_HIDDEN */ global_subscriber : public subscriber_thread
{
	RIWO_DISABLE_COPY_MOVE(global_subscriber)
	using callback_t = std::function<void(std::string_view,const payload_t&)>;

public:
	explicit global_subscriber(callback_t callback) :
		m_callback(std::move(callback))
	{
		start([this]
		{
			while( auto event = m_queue.dequeue() )
				m_callback(event->first, event->second);
		});
	}

	~global_subscriber() override {
		stop();
	}

	void tigger(std::string_view topic, const void *data, size_t size) noexcept
	{
		m_queue.force_emplace (
			std::make_pair(std::string(topic), payload_t(data, size))
		);
		notify();
	}

	void tigger(std::string_view topic, const shared_payload_t &payload) noexcept
	{
		m_queue.force_emplace (
			std::make_pair(std::string(topic), payload_t(payload))
		);
		notify();
	}

private:
	riwo::circular_lock_free_queue <
		std::pair<std::string,payload_t>, g_queue_max_size
	> m_queue {};

	callback_t m_callback {};
};

using global_subscriber_ptr = std::shared_ptr<global_subscriber>;

class /* RIWO_DECL_HIDDEN */ subscriber : public subscriber_thread
{
	RIWO_DISABLE_COPY_MOVE(subscriber)
	using callback_t = std::function<void(const payload_t&)>;

public:
	explicit subscriber(callback_t callback) :
		m_callback(std::move(callback))
	{
		start([this]
		{
			while( auto event = m_queue.dequeue() )
				m_callback(*event);
		});
	}

	~subscriber() override {
		stop();
	}

	void tigger(const void *data, size_t size) noexcept
	{
		m_queue.force_emplace(data, size);
		notify();
	}

	void tigger(const shared_payload_t &payload) noexcept
	{
		m_queue.force_emplace(payload);
		notify();
	}

private:
	riwo::circular_lock_free_queue<payload_t,g_queue_max_size> m_queue {};
	callback_t m_callback {};
};

using subscriber_ptr = std::shared_ptr<subscriber>;

} //namespace

class RIWO_DECL_HIDDEN cyclone_interface::impl
{
	RIWO_DISABLE_COPY_MOVE(impl)

	using subscriber_map = std::unordered_map <
		uint64_t, subscriber_ptr
	>;
	using topic_map = std::unordered_map <
		std::string, subscriber_map, transparent_string_hash, std::equal_to<>
	>;

public:
	impl() = default;

	[[nodiscard]] std::pair<uint64_t,subscriber_ptr>
	make_subscriber(std::string_view topic, std::function<void(const payload_t&)> callback) noexcept
	{
		auto id = m_id_seq++;
		auto obj = std::make_shared<subscriber>(std::move(callback));

		std::unique_lock lock(m_subscribers_lock);
		auto it = m_subscribers.emplace (
			std::string(topic), subscriber_map()
		);
		it.first->second.emplace(id, obj);
		m_topics_by_sid.emplace(id, it.first->first);
		return { id, obj };
	}

	[[nodiscard]] std::pair<uint64_t,global_subscriber_ptr>
	make_subscriber(std::function<void(std::string_view, const payload_t&)> callback) noexcept
	{
		auto id = m_id_seq++;
		auto obj = std::make_shared<global_subscriber>(std::move(callback));

		std::unique_lock lock(m_global_subscribers_lock);
		m_global_subscribers.emplace(id, obj);
		return { id, obj };
	}

	void global_broadcast(std::string_view topic, const void *data, size_t size) noexcept
	{
		std::shared_lock lock(m_global_subscribers_lock);
		if( m_global_subscribers.empty() )
			return ;

		for(auto &[id, subscriber] : m_global_subscribers)
			subscriber->tigger(topic, data, size);
	}

	void global_broadcast(std::string_view topic, const shared_payload_t &payload) noexcept
	{
		std::shared_lock lock(m_global_subscribers_lock);
		for(auto &[id, subscriber] : m_global_subscribers)
			subscriber->tigger(topic, payload);
	}

	void broadcast(std::string_view topic, const void *data, size_t size) noexcept
	{
		std::shared_lock lock(m_subscribers_lock);
		auto it = m_subscribers.find(topic);

		if( it == m_subscribers.end() or it->second.empty() )
			return ;

		for(auto &[id, subscriber] : it->second)
			subscriber->tigger(data, size);
	}

	void broadcast(std::string_view topic, const shared_payload_t &payload) noexcept
	{
		std::shared_lock lock(m_subscribers_lock);
		auto it = m_subscribers.find(topic);

		if( it == m_subscribers.end() )
			return ;

		for(auto &[id, subscriber] : it->second)
			subscriber->tigger(payload);
	}

	[[nodiscard]] size_t global_subscriber_count() const noexcept
	{
		std::shared_lock lock(m_global_subscribers_lock);
		return m_global_subscribers.size();
	}

	[[nodiscard]] size_t topic_subscriber_count(std::string_view topic) const noexcept
	{
		std::shared_lock lock(m_subscribers_lock);
		if( auto it = m_subscribers.find(topic); it != m_subscribers.end() )
			return it->second.size();
		return 0;
	}

public:
	std::atomic_uint64_t m_id_seq {0};
	topic_map m_subscribers {};

	std::unordered_map<uint64_t,std::string> m_topics_by_sid {};
	mutable riwo::shared_mutex m_subscribers_lock {};

	std::unordered_map<uint64_t,
		global_subscriber_ptr
	> m_global_subscribers {};

	mutable riwo::shared_mutex m_global_subscribers_lock {};
};

RIWO_DECL_HIDDEN void bridge_cyclone_data_available (
	std::string_view topic, const void *data, size_t size
);
namespace
{

using interface_set = std::unordered_set<cyclone_interface*>;

using topic_interface_map = std::unordered_map <
	std::string, interface_set, transparent_string_hash, std::equal_to<>
>;

std::unordered_map <
	cyclone_interface*, std::shared_ptr<cyclone_interface>
> g_obj_map {};

interface_set g_global_interfaces {};
topic_interface_map g_topic_interfaces {};
riwo::shared_mutex m_objs_lock {};

asio::io_context g_ioc {};
std::thread g_ioc_thread {};
std::mutex g_runtime_mutex {};

dds_entity_t g_participant = 0;
dds_entity_t g_topic = 0;

dds_entity_t g_publisher = 0;
dds_entity_t g_writer = 0;

dds_entity_t g_subscriber = 0;
dds_entity_t g_reader = 0;

struct runtime_guard
{
	~runtime_guard()
	{
		std::unique_lock lock(g_runtime_mutex);
		g_ioc.stop();

		if( g_ioc_thread.joinable() )
			g_ioc_thread.join();

		if( g_participant > 0 )
			dds_delete(g_participant);
	}
}
g_runtime_guard;

dds_qos_t *create_base_reliable_qos() noexcept
{
	auto qos = dds_create_qos();
	if( not qos )
		return nullptr;

	dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
	dds_qset_history(qos, DDS_HISTORY_KEEP_ALL, 1);

	dds_qset_resource_limits (
		qos, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED
	);
	dds_qset_latency_budget(qos, 0);
	return qos;
}

dds_qos_t *create_reliable_writer_qos() noexcept
{
	auto qos = create_base_reliable_qos();
	if( qos )
	{
#if DDS_VERSION_MAJOR > 0 || (DDS_VERSION_MAJOR == 0 && DDS_VERSION_MINOR >= 10)
		dds_qset_writer_batching(qos, false);
#else
		dds_write_set_batch(false);
#endif
	}
	return qos;
}

dds_qos_t *create_reliable_reader_qos() noexcept
{
	return create_base_reliable_qos();
}

void on_data_available(dds_entity_t reader, void*)
{
	for(;;)
	{
		constexpr size_t batch_size = 32;
		void *samples[batch_size] {};

		dds_sample_info_t infos[batch_size] {};
		auto ret = dds_take(reader, samples, infos, batch_size, batch_size);

		if( ret < 0 )
		{
			altun_clog_error("Altun.Core",
				"metsan::sbus::on_data_available: dds_take failed: {}",
				dds_strretcode(-ret)
			);
			return ;
		}
		if( ret <= 0 )
			return ;

		for(dds_return_t i=0; i<ret; i++)
		{
			auto msg = static_cast<altun_sbus_message*>(samples[i]);
			if( not msg )
				continue;

			if( infos[i].valid_data )
			{
				bridge_cyclone_data_available(msg->topic,
					msg->content._buffer, msg->content._length
				);
			}
		}
		auto loan_result = dds_return_loan(reader, samples, ret);
		if( loan_result != DDS_RETCODE_OK )
		{
			altun_clog_error("Altun.Core",
				"metsan::sbus::on_data_available: dds_return_loan failed: {}",
				dds_strretcode(-loan_result)
			);
			return ;
		}
	}
}

} //namespace

void bridge_cyclone_data_available(std::string_view topic, const void *data, size_t size)
{
	std::shared_lock lock(m_objs_lock);
	auto topic_pos = g_topic_interfaces.find(topic);

	if( g_global_interfaces.empty() and topic_pos == g_topic_interfaces.end() )
		return ;

	if( size >= g_shared_payload_threshold )
	{
		size_t subscriber_count = 0;
		for(auto *obj : g_global_interfaces)
			subscriber_count += obj->m_impl->global_subscriber_count();

		if( topic_pos != g_topic_interfaces.end() )
		{
			for(auto *obj : topic_pos->second)
				subscriber_count += obj->m_impl->topic_subscriber_count(topic);
		}
		if( subscriber_count > 1 )
		{
			auto begin = static_cast<const std::byte*>(data);
			shared_payload_t payload =
				std::make_shared<payload_buffer_t>(begin, begin + size);

			for(auto *obj : g_global_interfaces)
				obj->m_impl->global_broadcast(topic, payload);

			if( topic_pos != g_topic_interfaces.end() )
			{
				for(auto *obj : topic_pos->second)
					obj->m_impl->broadcast(topic, payload);
			}
			return ;
		}
	}
	for(auto *obj : g_global_interfaces)
		obj->m_impl->global_broadcast(topic, data, size);

	if( topic_pos != g_topic_interfaces.end() )
	{
		for(auto *obj : topic_pos->second)
			obj->m_impl->broadcast(topic, data, size);
	}
}

cyclone_interface::cyclone_interface() :
	m_impl(std::make_unique<impl>())
{
	RIWO_UNUSED(g_runtime_guard);
}

cyclone_interface::~cyclone_interface() = default;

void cyclone_interface::init()
{
	std::unique_lock lock(g_runtime_mutex);
	if( g_participant > 0 )
		return ;

	altun_log_debug("Altun.Core", "altun.sbus.init ...");
	auto participant = dds_create_participant(0, nullptr, nullptr);

	if( participant < 0 )
	{
		altun_clog_error("Altun.Core",
			"altun.sbus.init: dds_create_participant: {}",
			dds_strretcode(-participant)
		);
		return ;
	}
	auto cleanup = [&participant]
	{
		if( participant > 0 )
			dds_delete(participant);
	};
	auto topic = dds_create_topic (
		participant, &altun_sbus_message_desc, "altun_sbus_topic",
		nullptr, nullptr
	);
	if( topic < 0 )
	{
		altun_clog_error("Altun.Core",
			"altun.sbus.init: dds_create_topic: {}",
			dds_strretcode(-topic)
		);
		cleanup();
		return ;
	}
	auto publisher = dds_create_publisher(participant, nullptr, nullptr);
	if( publisher < 0 )
	{
		altun_clog_error("Altun.Core",
			"altun.sbus.init: dds_create_publisher: {}",
			dds_strretcode(-publisher)
		);
		cleanup();
		return ;
	}
	auto qos = create_reliable_writer_qos();
	if( not qos )
	{
		altun_clog_error("Altun.Core",
			"altun.sbus.init: dds_create_qos failed");
		cleanup();
		return ;
	}
	auto writer = dds_create_writer(publisher, topic, qos, nullptr);
	dds_delete_qos(qos);
	if( writer < 0 )
	{
		altun_clog_error("Altun.Core",
			"altun.sbus.init: dds_create_writer: {}",
			dds_strretcode(-writer)
		);
		cleanup();
		return ;
	}
	auto dds_subscriber = dds_create_subscriber(participant, nullptr, nullptr);
	if( dds_subscriber < 0 )
	{
		altun_clog_error("Altun.Core",
			"altun.sbus.init: dds_create_subscriber: {}",
			dds_strretcode(-dds_subscriber)
		);
		cleanup();
		return ;
	}
	auto listener = dds_create_listener(nullptr);
	if( not listener )
	{
		altun_clog_error("Altun.Core",
			"altun.sbus.init: dds_create_listener failed"
		);
		cleanup();
		return ;
	}
	dds_lset_data_available(listener, on_data_available);
	qos = create_reliable_reader_qos();
	if( not qos )
	{
		dds_delete_listener(listener);
		altun_clog_error("Altun.Core",
			"altun.sbus.init: dds_create_qos failed");
		cleanup();
		return ;
	}
	auto reader = dds_create_reader(dds_subscriber, topic, qos, listener);
	dds_delete_listener(listener);
	dds_delete_qos(qos);
	if( reader < 0 )
	{
		altun_clog_error("Altun.Core",
			"altun.sbus.init: dds_create_reader: {}",
			dds_strretcode(-reader)
		);
		cleanup();
		return ;
	}
	g_participant = participant;
	g_topic = topic;
	g_publisher = publisher;
	g_writer = writer;
	g_subscriber = dds_subscriber;
	g_reader = reader;
	altun_clog_debug("Altun.Core", "altun.sbus.init finished.");

	// Start the event loop in a separate thread.
	g_ioc_thread = std::thread([] { riwo::exec(g_ioc); });
}

void cyclone_interface::publish(std::string_view topic, const void *buffer, size_t size)
{
	if( size > std::numeric_limits<uint32_t>::max() )
	{
		altun_clog_error("Altun.Core",
			"altun.sbus.cyclone: payload size {} exceeds the DDS limit",
			size
		);
		return ;
	}
	std::span view {
		static_cast<const std::byte*>(buffer), size
	};
	riwo::dispatch(g_ioc,
	[topic = std::string(topic), payload = payload_buffer_t{ view.begin(), view.end() }]
	{
		auto msg = altun_sbus_message__alloc();
		if( not msg )
		{
			altun_clog_error("Altun.Core",
				"metsan::sbus::send_local: altun_sbus_message__alloc failed"
			);
			return ;
		}
		msg->topic = riwo::remove_const(topic.c_str());
		msg->content._buffer = payload.empty() ?
			nullptr : reinterpret_cast<char*>(riwo::remove_const(payload.data()));

		const auto payload_size = static_cast<uint32_t>(payload.size());
		msg->content._length = msg->content._maximum = payload_size;
		auto res= dds_write(g_writer, msg);

		if( res != DDS_RETCODE_OK )
		{
			altun_clog_error("Altun.Core",
				"metsan::sbus::send_local: dds_write: {}",
				dds_strretcode(-res)
			);
		}
		msg->topic = msg->content._buffer = nullptr;
		msg->content._length = msg->content._maximum = 0;
		msg->content._release = false;
		altun_sbus_message_free(msg, DDS_FREE_ALL);
	});
}

uint64_t cyclone_interface::subscribe
(std::string_view topic, std::function<void(const void*, size_t)> func)
{
	std::unique_lock objs_lock(m_objs_lock);
	auto [id, subr] = m_impl->make_subscriber(topic,
	[func = std::move(func)](const payload_t &payload) {
		func(payload.data(), payload.size());
	});
	riwo::ignore_unused(subr);
	g_obj_map.emplace(this, shared_from_this());
	g_topic_interfaces[std::string(topic)].emplace(this);
	return id;
}

uint64_t cyclone_interface::subscribe
(std::function<void(std::string_view topic, const void*, size_t)> func)
{
	std::unique_lock objs_lock(m_objs_lock);
	auto [id, subr] = m_impl->make_subscriber(
	[func = std::move(func)](std::string_view topic, const payload_t &payload) {
		func(topic, payload.data(), payload.size());
	});
	riwo::ignore_unused(subr);
	g_obj_map.emplace(this, shared_from_this());
	g_global_interfaces.emplace(this);
	return id;
}

void cyclone_interface::cancel_topic(std::string_view topic)
{
	std::unique_lock objs_lock(m_objs_lock);
	{
		std::unique_lock lock(m_impl->m_subscribers_lock);
		if( auto it = m_impl->m_subscribers.find(topic); it != m_impl->m_subscribers.end() )
		{
			for(auto &[sid, subscriber] : it->second)
			{
				riwo::ignore_unused(subscriber);
				m_impl->m_topics_by_sid.erase(sid);
			}
			m_impl->m_subscribers.erase(it);
		}
	}
	if( auto pos = g_topic_interfaces.find(topic); pos != g_topic_interfaces.end() )
	{
		pos->second.erase(this);
		if( pos->second.empty() )
			g_topic_interfaces.erase(pos);
	}
	std::shared_lock global_lock(m_impl->m_global_subscribers_lock);
	std::shared_lock topic_lock(m_impl->m_subscribers_lock);

	if( m_impl->m_global_subscribers.empty() and m_impl->m_subscribers.empty() )
		g_obj_map.erase(this);
}

void cyclone_interface::cancel_sid(uint64_t sid)
{
	std::unique_lock objs_lock(m_objs_lock);
	bool erased = false;
	bool global_empty = false;
	{
		std::unique_lock lock(m_impl->m_global_subscribers_lock);
		erased = m_impl->m_global_subscribers.erase(sid) > 0;
		global_empty = m_impl->m_global_subscribers.empty();
	}
	std::string topic {};
	bool topic_subscription = false;
	bool topic_empty = false;

	if( not erased )
	{
		std::unique_lock lock(m_impl->m_subscribers_lock);
		auto sid_pos = m_impl->m_topics_by_sid.find(sid);

		if( sid_pos != m_impl->m_topics_by_sid.end() )
		{
			topic_subscription = true;
			topic = sid_pos->second;

			m_impl->m_topics_by_sid.erase(sid_pos);
			if( auto pos = m_impl->m_subscribers.find(topic); pos != m_impl->m_subscribers.end() )
			{
				erased = pos->second.erase(sid) > 0;
				topic_empty = pos->second.empty();

				if( topic_empty )
					m_impl->m_subscribers.erase(pos);
			}
		}
	}
	if( not erased )
		return ;

	if( not topic_subscription )
	{
		if( global_empty )
			g_global_interfaces.erase(this);
	}
	else if( topic_empty )
	{
		if( auto pos = g_topic_interfaces.find(topic); pos != g_topic_interfaces.end() )
		{
			pos->second.erase(this);
			if( pos->second.empty() )
				g_topic_interfaces.erase(pos);
		}
	}
	std::shared_lock global_lock(m_impl->m_global_subscribers_lock);
	std::shared_lock topic_lock(m_impl->m_subscribers_lock);

	if( m_impl->m_global_subscribers.empty() and m_impl->m_subscribers.empty() )
		g_obj_map.erase(this);
}

void cyclone_interface::cancel()
{
	std::unique_lock objs_lock(m_objs_lock);
	for(auto &[topic, subscribers] : m_impl->m_subscribers)
	{
		riwo::ignore_unused(subscribers);
		if( auto pos = g_topic_interfaces.find(topic); pos != g_topic_interfaces.end() )
		{
			pos->second.erase(this);
			if( pos->second.empty() )
				g_topic_interfaces.erase(pos);
		}
	}
	g_global_interfaces.erase(this);

	m_impl->m_global_subscribers_lock.lock();
	m_impl->m_global_subscribers.clear();
	m_impl->m_global_subscribers_lock.unlock();

	m_impl->m_subscribers_lock.lock();
	m_impl->m_subscribers.clear();
	m_impl->m_topics_by_sid.clear();
	m_impl->m_subscribers_lock.unlock();

	g_obj_map.erase(this);
}

} //namespace altun::sbus

#endif //ALTUN_SBUS_CYCLONE_SUPPORT
