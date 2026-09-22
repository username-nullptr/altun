// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "shm_interface.h"
#if LIBEMPP_SBUS_SHM_SUPPORT

#include "log.h"
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <shared_mutex>

using namespace std::chrono_literals;

namespace libempp::sbus
{

LIBGS_DECL_HIDDEN void bridge_shm_data_available (
	std::string_view, const void*, size_t
);
namespace
{

constexpr std::array region_magic {'L','S','S','H','M','0','0','1'};
constexpr uint32_t region_version = 1;

constexpr size_t frame_size = 64 * 1'024;
constexpr size_t frame_count = 512;

constexpr size_t reader_count = 64;

constexpr size_t max_topic_size = 4 * 1'024;
constexpr size_t max_payload_size = 16 * 1'024 * 1'024;

struct reader_record
{
	int64_t pid = 0;
	uint64_t token = 0;
	uint64_t read_sequence = 0;
	uint64_t heartbeat_ns = 0;
};

struct frame_header
{
	uint64_t sequence = 0;
	uint64_t message_id = 0;
	uint64_t source_token = 0;

	uint32_t topic_size = 0;
	uint32_t payload_size = 0;

	uint32_t fragment_index = 0;
	uint32_t fragment_count = 0;
	uint32_t fragment_size = 0;

	uint32_t reserved = 0;
};

constexpr size_t frame_payload_size = frame_size - sizeof(frame_header);

struct alignas(64) frame_slot
{
	frame_header header {};
	std::array<std::byte,frame_payload_size> payload {};
};
static_assert(sizeof(frame_slot) == frame_size);

struct region_header
{
	std::array<char,8> magic {};

	uint32_t version = 0;
	uint32_t header_size = 0;

	uint32_t slot_size = 0;
	uint32_t slot_count = 0;

	uint32_t max_readers = 0;
	uint32_t reserved = 0;

	pthread_mutex_t mutex {};
	pthread_cond_t data_available {};
	pthread_cond_t space_available {};

	uint64_t write_sequence = 0;
	uint64_t next_message_id = 0;

	std::array<reader_record,reader_count> readers {};
};

struct alignas(64) shared_region
{
	region_header header {};
	alignas(64) std::array<frame_slot,frame_count> slots {};
};
static_assert(std::is_trivially_copyable_v<shared_region>);

struct incoming_message
{
	std::string topic {};
	std::vector<std::byte> payload {};
	uint64_t source_token = 0;
};

[[nodiscard]] uint64_t monotonic_nanoseconds() noexcept
{
	timespec value {};
	if( clock_gettime(CLOCK_MONOTONIC, &value) != 0 )
		return 0;

	return static_cast<uint64_t>(value.tv_sec) * 1'000'000'000ULL +
		static_cast<uint64_t>(value.tv_nsec);
}

[[nodiscard]] timespec realtime_deadline(std::chrono::milliseconds timeout) noexcept
{
	timespec value {};
	clock_gettime(CLOCK_REALTIME, &value);

	const auto nanoseconds = value.tv_nsec +
		std::chrono::duration_cast<std::chrono::nanoseconds>(timeout).count();

	value.tv_sec += nanoseconds / 1'000'000'000LL;
	value.tv_nsec = nanoseconds % 1'000'000'000LL;
	return value;
}

[[nodiscard]] std::string shared_memory_name()
{
	if( const char *configured = std::getenv("LIBEMPP_SBUS_SHM_NAME") )
	{
		if( const std::string_view value(configured);
			value.size() > 1 and value.front() == '/' and value.find('/', 1) == std::string_view::npos )
			return std::string(value);

		libempp_clog_error("LibEMpp.Core",
			"libempp.sbus.shm: ignoring invalid LIBEMPP_SBUS_SHM_NAME"
		);
	}
	return "/libempp-sbus-" + std::to_string(static_cast<uint64_t>(getuid()));
}

[[nodiscard]] bool set_file_lock(int fd, short type) noexcept
{
	flock lock {};
	lock.l_type = type;
	lock.l_whence = SEEK_SET;

	while( fcntl(fd, F_SETLKW, &lock) != 0 )
	{
		if( errno != EINTR )
			return false;
	}
	return true;
}

class shared_lock
{
	LIBGS_DISABLE_COPY_MOVE(shared_lock)

public:
	explicit shared_lock(pthread_mutex_t &mutex) noexcept :
		m_mutex(&mutex)
	{
		if( const auto result = pthread_mutex_lock(m_mutex); result == 0 )
			m_locked = true;
#if __linux__
		else if( result == EOWNERDEAD )
			m_locked = pthread_mutex_consistent(m_mutex) == 0;
#endif //__linux__
	}

	~shared_lock()
	{
		if( m_locked )
			pthread_mutex_unlock(m_mutex);
	}

	[[nodiscard]] explicit operator bool() const noexcept {
		return m_locked;
	}

	[[nodiscard]] int wait(pthread_cond_t &condition, const timespec &deadline) noexcept
	{
		const auto result = pthread_cond_timedwait(&condition, m_mutex, &deadline);
#if __linux__
		if( result == EOWNERDEAD )
		{
			if( pthread_mutex_consistent(m_mutex) == 0 )
				return 0;
			m_locked = false;
		}
#endif //__linux__
		return result;
	}

private:
	pthread_mutex_t *m_mutex = nullptr;
	bool m_locked = false;
};

[[nodiscard]] bool initialize_region(shared_region &region) noexcept
{
	std::fill_n(reinterpret_cast<unsigned char*>(&region.header),
		sizeof(region.header), 0
	);
	pthread_mutexattr_t mutex_attributes;
	if( pthread_mutexattr_init(&mutex_attributes) != 0 )
		return false;

	bool valid = pthread_mutexattr_setpshared (
		&mutex_attributes, PTHREAD_PROCESS_SHARED
	) == 0;

#if __linux__
	valid = valid and pthread_mutexattr_setrobust (
		&mutex_attributes, PTHREAD_MUTEX_ROBUST
	) == 0;
#endif //__linux__

	valid = valid and pthread_mutex_init (
		&region.header.mutex, &mutex_attributes
	) == 0;

	pthread_mutexattr_destroy(&mutex_attributes);
	if( not valid )
		return false;

	pthread_condattr_t condition_attributes;
	if( pthread_condattr_init(&condition_attributes) != 0 )
		return false;

	valid = pthread_condattr_setpshared (
		&condition_attributes, PTHREAD_PROCESS_SHARED
	) == 0;

	valid = valid and pthread_cond_init (
		&region.header.data_available, &condition_attributes
	) == 0;

	valid = valid and pthread_cond_init (
		&region.header.space_available, &condition_attributes
	) == 0;

	pthread_condattr_destroy(&condition_attributes);
	if( not valid )
		return false;

	region.header.version = region_version;
	region.header.header_size = sizeof(region_header);
	region.header.slot_size = sizeof(frame_slot);

	region.header.slot_count = frame_count;
	region.header.max_readers = reader_count;

	region.header.next_message_id = 1;
	region.header.magic = region_magic;
	return true;
}

[[nodiscard]] bool valid_region(const shared_region &region) noexcept
{
	return region.header.magic == region_magic and
		   region.header.version == region_version and
		   region.header.header_size == sizeof(region_header) and
		   region.header.slot_size == sizeof(frame_slot) and
		   region.header.slot_count == frame_count and
		   region.header.max_readers == reader_count;
}

void deactivate_dead_readers(region_header &header) noexcept
{
	for(auto &reader : header.readers)
	{
		if( reader.pid <= 0 )
			continue;

		if( kill(static_cast<pid_t>(reader.pid), 0) != 0 and errno == ESRCH )
			reader = {};
	}
}

[[nodiscard]] uint64_t oldest_read_sequence(region_header &header) noexcept
{
	uint64_t oldest = header.write_sequence;
	for(const auto &reader : header.readers)
	{
		if( reader.pid > 0 )
			oldest = std::min(oldest, reader.read_sequence);
	}
	return oldest;
}

[[nodiscard]] uint64_t make_process_token(const void *address) noexcept
{
	auto token = monotonic_nanoseconds();
	token ^= static_cast<uint64_t>(getpid()) << 32U;
	token ^= reinterpret_cast<uintptr_t>(address);
	return token == 0 ? 1 : token;
}

class runtime
{
	LIBGS_DISABLE_COPY_MOVE(runtime)

public:
	runtime() :
		m_process_token(make_process_token(this))
	{
		open_region();
		if( not m_region or not register_reader() )
			return;

		m_run.store(true, std::memory_order_release);
		m_receive_thread = std::thread([this]() noexcept { receive_messages(); });
	}

	~runtime()
	{
		m_run.store(false, std::memory_order_release);
		if( m_region )
		{
			shared_lock lock(m_region->header.mutex);
			if( lock )
				pthread_cond_broadcast(&m_region->header.data_available);
		}
		if( m_receive_thread.joinable() )
			m_receive_thread.join();

		unregister_reader();
		if( m_region )
			munmap(m_region, sizeof(shared_region));

		if( m_fd >= 0 )
			close(m_fd);
	}

	void publish(std::string_view topic, const void *buffer, size_t size) noexcept
	{
		if( not m_region )
			return;

		if( topic.size() > max_topic_size or size > max_payload_size )
		{
			libempp_clog_error("LibEMpp.Core",
				"libempp.sbus.shm: topic or payload exceeds the transport limit"
			);
			return;
		}
		const size_t total_size = topic.size() + size;

		const size_t required_frames = std::max<size_t>(
			1, (total_size + frame_payload_size - 1) / frame_payload_size
		);
		if( required_frames > frame_count )
		{
			libempp_clog_error("LibEMpp.Core",
				"libempp.sbus.shm: message does not fit in the shared ring"
			);
			return;
		}
		shared_lock lock(m_region->header.mutex);
		if( not lock )
		{
			log_mutex_failure("publish");
			return;
		}
		auto &header = m_region->header;

		while( header.write_sequence - oldest_read_sequence(header) > frame_count - required_frames )
		{
			deactivate_dead_readers(header);
			if( header.write_sequence - oldest_read_sequence(header) <= frame_count - required_frames )
				break;

			const auto deadline = realtime_deadline(500ms);

			if( const auto result = lock.wait(header.space_available, deadline);
				result != 0 and result != ETIMEDOUT )
			{
				log_wait_failure("wait for shared ring space", result);
				return;
			}
			deactivate_dead_readers(header);
		}
		const auto message_id = header.next_message_id++;

		for(size_t index = 0; index < required_frames; ++index)
		{
			const auto sequence = header.write_sequence + index;
			auto &slot = m_region->slots[sequence % frame_count];

			const auto byte_offset = index * frame_payload_size;
			const auto bytes = std::min(frame_payload_size, total_size - byte_offset);

			slot.header.sequence = sequence;
			slot.header.message_id = message_id;
			slot.header.source_token = m_process_token;

			slot.header.topic_size = static_cast<uint32_t>(topic.size());
			slot.header.payload_size = static_cast<uint32_t>(size);

			slot.header.fragment_index = static_cast<uint32_t>(index);
			slot.header.fragment_count = static_cast<uint32_t>(required_frames);

			slot.header.fragment_size = static_cast<uint32_t>(bytes);
			slot.header.reserved = 0;

			copy_message_fragment(slot.payload.data(),
				topic, buffer, size, byte_offset, bytes
			);
		}
		header.write_sequence += required_frames;
		pthread_cond_broadcast(&header.data_available);
	}

private:
	void open_region() noexcept
	{
		m_name = shared_memory_name();
		int flags = O_RDWR | O_CREAT;
#ifdef O_CLOEXEC
		flags |= O_CLOEXEC;
#endif //O_CLOEXEC

		m_fd = shm_open(m_name.c_str(), flags, S_IRUSR | S_IWUSR);
		if( m_fd < 0 )
		{
			log_errno("open shared memory", errno);
			return;
		}
		if( not set_file_lock(m_fd, F_WRLCK) )
		{
			log_errno("lock shared memory initialization", errno);
			return;
		}
		struct stat status {};
		bool valid = fstat(m_fd, &status) == 0;

		if( valid and status.st_size != static_cast<off_t>(sizeof(shared_region)) )
			valid = ftruncate(m_fd, sizeof(shared_region)) == 0;

		if( not valid )
		{
			const auto error = errno;
			(void)set_file_lock(m_fd, F_UNLCK);

			log_errno("size shared memory", error);
			return;
		}
		void *mapping = mmap(nullptr, sizeof(shared_region),
			PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0
		);
		if( mapping == MAP_FAILED )
		{
			const auto error = errno;
			(void)set_file_lock(m_fd, F_UNLCK);

			log_errno("map shared memory", error);
			return;
		}
		m_region = static_cast<shared_region*>(mapping);
		if( not valid_region(*m_region) and not initialize_region(*m_region) )
		{
			(void)set_file_lock(m_fd, F_UNLCK);

			libempp_clog_error("LibEMpp.Core",
				"libempp.sbus.shm: failed to initialize process-shared synchronization"
			);
			munmap(m_region, sizeof(shared_region));
			m_region = nullptr;
			return;
		}
		(void)set_file_lock(m_fd, F_UNLCK);
	}

	[[nodiscard]] bool register_reader() noexcept
	{
		shared_lock lock(m_region->header.mutex);
		if( not lock )
		{
			log_mutex_failure("register reader");
			return false;
		}
		deactivate_dead_readers(m_region->header);
		for(auto &reader : m_region->header.readers)
		{
			if( reader.pid == static_cast<int64_t>(getpid()) )
				reader = {};
		}
		for(size_t index = 0; index < m_region->header.readers.size(); ++index)
		{
			auto &reader = m_region->header.readers[index];
			if( reader.pid != 0 )
				continue;

			reader.pid = static_cast<int64_t>(getpid());
			reader.token = m_process_token;

			reader.read_sequence = m_region->header.write_sequence;
			reader.heartbeat_ns = monotonic_nanoseconds();

			m_reader_index = index;
			return true;
		}
		libempp_clog_error("LibEMpp.Core",
			"libempp.sbus.shm: shared reader table is full"
		);
		return false;
	}

	void unregister_reader() noexcept
	{
		if( not m_region or not m_reader_index )
			return;

		shared_lock lock(m_region->header.mutex);
		if( not lock )
			return;

		auto &reader = m_region->header.readers[*m_reader_index];
		if( reader.token == m_process_token )
			reader = {};

		pthread_cond_broadcast(&m_region->header.space_available);
	}

	void receive_messages() noexcept
	{
		try {
			incoming_message message;
			while( m_run.load(std::memory_order_acquire) )
			{
				if( receive_one(message) and message.source_token != m_process_token )
				{
					bridge_shm_data_available(message.topic,
						message.payload.data(), message.payload.size()
					);
				}
			}
		}
		catch(const std::exception &exception)
		{
			m_run.store(false, std::memory_order_release);
			unregister_reader();

			libempp_clog_error("LibEMpp.Core",
				"libempp.sbus.shm: receive worker failed: {}", exception
			);
		}
		catch(...)
		{
			m_run.store(false, std::memory_order_release);
			unregister_reader();

			libempp_clog_error("LibEMpp.Core",
				"libempp.sbus.shm: receive worker failed with an unknown exception"
			);
		}
	}

	[[nodiscard]] bool receive_one(incoming_message &message)
	{
		uint64_t sequence = 0;
		frame_header first_header {};

		size_t total_size = 0;
		size_t expected_frames = 0;
		{
			shared_lock lock(m_region->header.mutex);
			if( not lock )
			{
				log_mutex_failure("receive");
				m_run.store(false, std::memory_order_release);
				return false;
			}
			auto &header = m_region->header;
			auto &reader = header.readers[*m_reader_index];

			while( m_run.load(std::memory_order_acquire) and
				   reader.read_sequence == header.write_sequence )
			{
				reader.heartbeat_ns = monotonic_nanoseconds();
				const auto deadline = realtime_deadline(500ms);

				if( const auto result = lock.wait(header.data_available, deadline);
					result != 0 and result != ETIMEDOUT )
				{
					log_wait_failure("wait for shared data", result);
					m_run.store(false, std::memory_order_release);
					return false;
				}
			}
			if( not m_run.load(std::memory_order_acquire) )
				return false;

			sequence = reader.read_sequence;
			const auto &first = m_region->slots[sequence % frame_count];

			total_size = static_cast<size_t>(first.header.topic_size) +
				static_cast<size_t>(first.header.payload_size);

			expected_frames = std::max<size_t>(
				1, (total_size + frame_payload_size - 1) / frame_payload_size
			);
			if( not valid_first_frame(first, sequence, total_size, expected_frames) or
				header.write_sequence - sequence < expected_frames )
			{
				recover_reader(header, reader);
				return false;
			}
			first_header = first.header;
			for(size_t index = 0; index < expected_frames; ++index)
			{
				const auto &slot = m_region->slots[(sequence + index) % frame_count];
				const auto offset = index * frame_payload_size;
				const auto expected_size = std::min(frame_payload_size, total_size - offset);

				if( not valid_fragment(slot, first_header, sequence + index,
					index, expected_frames, expected_size) )
				{
					recover_reader(header, reader);
					return false;
				}
			}
			if( first_header.source_token == m_process_token )
			{
				advance_reader(header, reader, expected_frames);
				return false;
			}
			if( expected_frames == 1 )
			{
				message.source_token = first_header.source_token;
				const auto topic_size = static_cast<size_t>(first_header.topic_size);

				message.topic.resize(topic_size);
				message.payload.resize(first_header.payload_size);

				if( topic_size != 0 )
					std::memcpy(message.topic.data(), first.payload.data(), topic_size);

				if( first_header.payload_size != 0 )
				{
					std::memcpy(message.payload.data(),
						first.payload.data() + topic_size, first_header.payload_size
					);
				}
				advance_reader(header, reader, 1);
				return true;
			}
			if( expected_frames == 2 )
			{
				message.source_token = first_header.source_token;
				const auto topic_size = static_cast<size_t>(first_header.topic_size);

				message.topic.resize(topic_size);
				message.payload.resize(first_header.payload_size);

				for(size_t index = 0; index < expected_frames; ++index)
				{
					const auto &slot = m_region->slots[(sequence + index) % frame_count];
					const auto offset = index * frame_payload_size;

					const auto expected_size = std::min (
						frame_payload_size, total_size - offset
					);
					const auto fragment_end = offset + expected_size;
					const auto topic_end = std::min(fragment_end, topic_size);

					if( offset < topic_end )
					{
						std::memcpy(message.topic.data() + offset,
							slot.payload.data(), topic_end - offset
						);
					}
					const auto payload_begin = std::max(offset, topic_size);
					if( payload_begin < fragment_end )
					{
						std::memcpy(message.payload.data() + payload_begin - topic_size,
							slot.payload.data() + payload_begin - offset,
							fragment_end - payload_begin
						);
					}
				}
				advance_reader(header, reader, expected_frames);
				return true;
			}
		}
		message.source_token = first_header.source_token;
		const auto topic_size = static_cast<size_t>(first_header.topic_size);

		message.topic.resize(topic_size);
		message.payload.resize(first_header.payload_size);

		// Keeping the reader cursor unchanged pins these committed slots while
		// large payloads are copied without monopolizing the process-shared mutex.
		for(size_t index = 0; index < expected_frames; ++index)
		{
			const auto &slot = m_region->slots[(sequence + index) % frame_count];
			const auto offset = index * frame_payload_size;
			const auto expected_size = std::min(frame_payload_size, total_size - offset);

			const auto fragment_end = offset + expected_size;
			const auto topic_end = std::min(fragment_end, topic_size);

			if( offset < topic_end )
			{
				std::memcpy(message.topic.data() + offset,
					slot.payload.data(), topic_end - offset
				);
			}
			const auto payload_begin = std::max(offset, topic_size);
			if( payload_begin < fragment_end )
			{
				std::memcpy(message.payload.data() + payload_begin - topic_size,
					slot.payload.data() + payload_begin - offset,
					fragment_end - payload_begin
				);
			}
		}
		shared_lock lock(m_region->header.mutex);
		if( not lock )
		{
			log_mutex_failure("commit receive");
			m_run.store(false, std::memory_order_release);
			return false;
		}
		auto &header = m_region->header;
		auto &reader = header.readers[*m_reader_index];

		if( reader.token != m_process_token or reader.read_sequence != sequence )
		{
			libempp_clog_error("LibEMpp.Core",
				"libempp.sbus.shm: shared reader state changed during receive"
			);
			m_run.store(false, std::memory_order_release);
			return false;
		}
		advance_reader(header, reader, expected_frames);
		return true;
	}

	static void advance_reader(region_header &header, reader_record &reader, size_t frames) noexcept
	{
		reader.read_sequence += frames;
		reader.heartbeat_ns = monotonic_nanoseconds();
		pthread_cond_broadcast(&header.space_available);
	}

	static void recover_reader(region_header &header, reader_record &reader) noexcept
	{
		libempp_clog_error("LibEMpp.Core",
			"libempp.sbus.shm: invalid shared-memory frame; dropping unread data"
		);
		reader.read_sequence = header.write_sequence;
		reader.heartbeat_ns = monotonic_nanoseconds();
		pthread_cond_broadcast(&header.space_available);
	}

	[[nodiscard]] static bool valid_first_frame
	(const frame_slot &slot, uint64_t sequence, size_t total_size, size_t expected_frames) noexcept
	{
		return slot.header.sequence == sequence and
			   slot.header.fragment_index == 0 and
			   slot.header.fragment_count == expected_frames and
			   slot.header.topic_size <= max_topic_size and
			   slot.header.payload_size <= max_payload_size and
			   total_size <= max_topic_size + max_payload_size and
			   expected_frames <= frame_count;
	}

	[[nodiscard]] static bool valid_fragment(const frame_slot &slot, const frame_header &first,
		uint64_t sequence, size_t index, size_t fragments, size_t expected_size) noexcept
	{
		return slot.header.sequence == sequence and
			   slot.header.message_id == first.message_id and
			   slot.header.source_token == first.source_token and
			   slot.header.topic_size == first.topic_size and
			   slot.header.payload_size == first.payload_size and
			   slot.header.fragment_index == index and
			   slot.header.fragment_count == fragments and
			   slot.header.fragment_size == expected_size;
	}

	static void copy_message_fragment(std::byte *destination, std::string_view topic,
		const void *payload, size_t payload_size, size_t offset, size_t size) noexcept
	{
		size_t written = 0;
		if( offset < topic.size() )
		{
			const auto topic_bytes = std::min(size, topic.size() - offset);
			std::memcpy(destination, topic.data() + offset, topic_bytes);

			written += topic_bytes;
			offset += topic_bytes;
		}
		if( written < size )
		{
			const auto payload_offset = offset - topic.size();
			const auto bytes = std::min(size - written, payload_size - payload_offset);

			std::memcpy(destination + written,
				static_cast<const std::byte*>(payload) + payload_offset, bytes
			);
		}
	}

	static void log_errno(std::string_view action, int error) noexcept
	{
		libempp_clog_error("LibEMpp.Core",
			"libempp.sbus.shm: failed to {}: {}", action, std::strerror(error)
		);
	}

	static void log_wait_failure(std::string_view action, int error) noexcept {
		log_errno(action, error);
	}

	static void log_mutex_failure(std::string_view action) noexcept
	{
		libempp_clog_error("LibEMpp.Core",
			"libempp.sbus.shm: failed to {}: shared mutex is unavailable", action
		);
	}

	std::string m_name {};
	int m_fd = -1;

	shared_region *m_region = nullptr;
	std::optional<size_t> m_reader_index {};

	uint64_t m_process_token = 0;
	std::atomic_bool m_run {false};

	std::thread m_receive_thread {};
};

runtime &bus_runtime()
{
	static runtime value;
	return value;
}

using topic_callback = std::function<void(const void*,size_t)>;
using global_callback = std::function<void(std::string_view,const void*,size_t)>;

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

std::mutex g_interfaces_mutex {};
std::unordered_map<shm_interface*,std::shared_ptr<shm_interface>> g_interfaces {};

using interface_list = std::vector<std::shared_ptr<shm_interface>>;
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
		LIBGS_UNUSED(pointer);
		snapshot->emplace_back(object);
	}
	g_interface_snapshot.store(std::move(snapshot), std::memory_order_release);
}

} //namespace

class LIBGS_DECL_HIDDEN shm_interface::impl
{
	LIBGS_DISABLE_COPY_MOVE(impl)

public:
	impl() = default;

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

	[[nodiscard]] bool empty() const noexcept {
		return topic_callbacks.empty() and global_callbacks.empty();
	}

	void rebuild_topic_snapshot(std::string_view topic)
	{
		auto source = topic_callbacks.find(topic);
		if( source == topic_callbacks.end() )
		{
			topic_snapshots.erase(std::string(topic));
			return;
		}
		auto snapshot = std::make_shared<topic_callback_list>();
		snapshot->reserve(source->second.size());

		for(const auto &[sid, callback] : source->second)
		{
			LIBGS_UNUSED(sid);
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
			LIBGS_UNUSED(sid);
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

void bridge_shm_data_available(std::string_view topic, const void *data, size_t size)
{
	auto interfaces = g_interface_snapshot.load(std::memory_order_acquire);
	for(const auto &interface : *interfaces)
	{
		shm_interface::impl::topic_callback_list_ptr topic_callbacks;
		shm_interface::impl::global_callback_list_ptr global_callbacks;
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

shm_interface::shm_interface() :
	m_impl(std::make_unique<impl>())
{

}

shm_interface::~shm_interface() = default;

void shm_interface::init()
{
	LIBGS_UNUSED(bus_runtime());
}

void shm_interface::publish(std::string_view topic, const void *buffer, size_t size)
{
	if( size != 0 and not buffer )
	{
		libempp_clog_error("LibEMpp.Core",
			"libempp.sbus.shm: a non-empty payload has a null buffer"
		);
		return;
	}
	bridge_shm_data_available(topic, buffer, size);
	bus_runtime().publish(topic, buffer, size);
}

uint64_t shm_interface::subscribe(std::string_view topic, topic_callback func)
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

		LIBGS_UNUSED(position);
		if( inserted )
			rebuild_interface_snapshot();
	}
	return sid;
}

uint64_t shm_interface::subscribe(global_callback func)
{
	std::lock_guard lock(m_impl->mutex);
	const auto sid = m_impl->next_sid++;
	m_impl->global_callbacks.emplace(sid, std::move(func));

	m_impl->rebuild_global_snapshot();
	{
		std::lock_guard interfaces_lock(g_interfaces_mutex);
		const auto [position, inserted] =
			g_interfaces.insert_or_assign(this, shared_from_this());

		LIBGS_UNUSED(position);
		if( inserted )
			rebuild_interface_snapshot();
	}
	return sid;
}

void shm_interface::cancel_topic(std::string_view topic)
{
	std::lock_guard lock(m_impl->mutex);
	if( auto it = m_impl->topic_callbacks.find(topic);
		it != m_impl->topic_callbacks.end() )
	{
		for(const auto &[sid, callback] : it->second)
		{
			LIBGS_UNUSED(callback);
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

void shm_interface::cancel_sid(uint64_t sid)
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

void shm_interface::cancel()
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

#endif //LIBEMPP_SBUS_SHM_SUPPORT
