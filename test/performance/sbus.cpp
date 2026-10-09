// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "benchmark.h"

#include <altun/core/sbus/detail/cyclone_interface.h>
#include <altun/core/sbus/detail/dbus_interface.h>
#include <altun/core/sbus/detail/shm_interface.h>
#include <riwo/utils/process.h>
#include <riwo/utils/sbus.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <sstream>
#include <thread>
#if ALTUN_PERFORMANCE_SBUS_SHM
#include <unistd.h>
#endif

namespace
{

using namespace std::chrono_literals;
using altun_test::performance::clock;
using altun_test::performance::duration;
using altun_test::performance::result;

constexpr uint64_t message_magic = 0x5342555350455246ULL;
constexpr uint64_t stop_run = std::numeric_limits<uint64_t>::max();
constexpr size_t sample_count = 3;
constexpr size_t pipeline_depth = 32;
constexpr size_t pipeline_byte_limit = 8 * 1'024 * 1'024;
constexpr std::array<size_t,4> payload_sizes {32, 1'024, 64 * 1'024, 1'024 * 1'024};

struct message_header
{
	uint64_t magic = message_magic;
	uint64_t run = 0;
	uint64_t sequence = 0;
};

static_assert(sizeof(message_header) <= payload_sizes.front());

template <typename Interface>
using subscriber_t = riwo::utils::sbus::basic_subscriber<Interface>;

template <typename Interface>
struct transport_traits;

#if ALTUN_PERFORMANCE_SBUS_DBUS
template <>
struct transport_traits<altun::sbus::dbus_interface>
{
	static constexpr std::string_view name = "dbus";
};
#endif

#if ALTUN_PERFORMANCE_SBUS_CYCLONE
template <>
struct transport_traits<altun::sbus::cyclone_interface>
{
	static constexpr std::string_view name = "cyclone";
};
#endif

#if ALTUN_PERFORMANCE_SBUS_SHM
template <>
struct transport_traits<altun::sbus::shm_interface>
{
	static constexpr std::string_view name = "shm";
};
#endif

class receiver
{
public:
	void activate(uint64_t run, size_t payload_size) noexcept
	{
		m_active_run.store(0, std::memory_order_release);
		m_payload_size.store(payload_size, std::memory_order_relaxed);
		m_received.store(0, std::memory_order_relaxed);
		m_invalid.store(false, std::memory_order_relaxed);
		m_active_run.store(run, std::memory_order_release);
	}

	void operator()(const void *data, size_t size) noexcept
	{
		if( not data or size < sizeof(message_header) )
		{
			m_invalid.store(true, std::memory_order_relaxed);
			return ;
		}
		message_header header;
		std::memcpy(&header, data, sizeof(header));
		const auto active_run = m_active_run.load(std::memory_order_acquire);
		if( header.run != active_run )
			return ;

		if( header.magic != message_magic or
			size != m_payload_size.load(std::memory_order_relaxed) )
			m_invalid.store(true, std::memory_order_relaxed);

		const auto expected = m_received.fetch_add(1, std::memory_order_release);
		if( header.sequence != expected )
			m_invalid.store(true, std::memory_order_relaxed);
	}

	[[nodiscard]] bool wait(size_t expected, duration timeout = 15s) const noexcept
	{
		const auto deadline = clock::now() + timeout;
		while( m_received.load(std::memory_order_acquire) < expected )
		{
			if( clock::now() >= deadline )
				return false;
			std::this_thread::yield();
		}
		return true;
	}

	void verify(size_t expected) const
	{
		if( m_received.load(std::memory_order_acquire) != expected )
			throw std::runtime_error("SBus benchmark did not receive every message");
		if( m_invalid.load(std::memory_order_relaxed) )
			throw std::runtime_error("SBus benchmark received corrupt or reordered data");
	}

private:
	std::atomic_uint64_t m_active_run {0};
	std::atomic_size_t m_payload_size {0};
	std::atomic_size_t m_received {0};
	std::atomic_bool m_invalid {false};
};

void set_header(std::vector<std::byte> &payload, uint64_t run, uint64_t sequence)
{
	const message_header header {message_magic, run, sequence};
	std::memcpy(payload.data(), &header, sizeof(header));
}

struct throughput_sample
{
	duration publish_elapsed {};
	duration end_to_end_elapsed {};
};

template <typename Interface>
throughput_sample measure_throughput(
	receiver &target,
	std::string_view topic,
	size_t payload_size,
	size_t count,
	uint64_t run
)
{
	std::vector<std::byte> payload(payload_size, std::byte {0x2a});
	target.activate(run, payload_size);

	duration publish_elapsed {};
	const auto begin = clock::now();
	const auto window = std::max<size_t>(
		1, std::min(pipeline_depth, pipeline_byte_limit / payload_size)
	);
	for(size_t published = 0; published < count; )
	{
		const auto batch = std::min(window, count - published);
		const auto publish_begin = clock::now();
		for(size_t offset = 0; offset < batch; ++offset)
		{
			set_header(payload, run, published + offset);
			Interface::publish(topic, payload.data(), payload.size());
		}
		publish_elapsed += clock::now() - publish_begin;
		published += batch;
		if( not target.wait(published) )
			throw std::runtime_error("SBus throughput sample timed out");
	}
	const auto end_to_end_elapsed = clock::now() - begin;
	target.verify(count);
	return {publish_elapsed, end_to_end_elapsed};
}

template <typename Interface>
std::vector<duration> measure_latency(
	receiver &target,
	std::string_view topic,
	size_t payload_size,
	size_t count,
	uint64_t run
)
{
	std::vector<std::byte> payload(payload_size, std::byte {0x2a});
	std::vector<duration> samples;
	samples.reserve(count);
	target.activate(run, payload_size);

	for(size_t sequence = 0; sequence < count; ++sequence)
	{
		set_header(payload, run, sequence);
		const auto begin = clock::now();
		Interface::publish(topic, payload.data(), payload.size());
		if( not target.wait(sequence + 1) )
			throw std::runtime_error("SBus latency sample timed out");
		samples.emplace_back(clock::now() - begin);
	}
	target.verify(count);
	return samples;
}

size_t local_message_count(size_t payload_size)
{
	const auto scale = altun_test::performance::scale;
	if( payload_size <= 32 )
		return 4'000 * scale;
	if( payload_size <= 1'024 )
		return 2'000 * scale;
	if( payload_size <= 64 * 1'024 )
		return 256 * scale;
	return 16 * scale;
}

size_t ipc_message_count(size_t payload_size)
{
	const auto scale = altun_test::performance::scale;
	if( payload_size <= 32 )
		return 1'000 * scale;
	if( payload_size <= 1'024 )
		return 500 * scale;
	if( payload_size <= 64 * 1'024 )
		return 128 * scale;
	return 8 * scale;
}

class temporary_directory
{
public:
	temporary_directory()
	{
		const auto stamp = clock::now().time_since_epoch().count();
		m_path = std::filesystem::temp_directory_path() /
			("altun-sbus-performance-" + std::to_string(stamp));
		std::filesystem::create_directories(m_path);
	}

	~temporary_directory()
	{
		std::error_code ignored;
		std::filesystem::remove_all(m_path, ignored);
	}

	temporary_directory(const temporary_directory&) = delete;
	temporary_directory &operator=(const temporary_directory&) = delete;

	[[nodiscard]] const std::filesystem::path &path() const noexcept {
		return m_path;
	}

private:
	std::filesystem::path m_path;
};

void stop_process(riwo::utils::process &process) noexcept
{
	if( not process.joinable() )
		return ;
	process.terminate();
	(void) process.join(2s);
	if( process.joinable() )
	{
		process.kill();
		(void) process.join(2s);
	}
}

template <typename Interface>
int run_peer(
	const std::filesystem::path &ready_file,
	std::string_view request_topic,
	std::string_view response_topic
)
{
	asio::thread_pool pool(1);
	subscriber_t<Interface> subscriber(pool);
	std::mutex mutex;
	std::condition_variable changed;
	bool stopped = false;

	subscriber.subscribe(request_topic,
	[&](const void *data, size_t size)
	{
		if( not data or size < sizeof(message_header) )
			return ;
		message_header header;
		std::memcpy(&header, data, sizeof(header));
		if( header.magic != message_magic )
			return ;
		if( header.run == stop_run )
		{
			{
				std::lock_guard lock(mutex);
				stopped = true;
			}
			changed.notify_one();
			return ;
		}
		Interface::publish(response_topic, data, size);
	});

	{
		std::ofstream ready(ready_file);
		ready << "ready";
		if( not ready.good() )
			return 65;
	}

	std::unique_lock lock(mutex);
	const auto completed = changed.wait_for(lock, 120s, [&] { return stopped; });
	lock.unlock();
	subscriber.cancel();
	pool.stop();
	pool.join();
	return completed ? 0 : 66;
}

template <typename Interface>
class peer_process
{
public:
	peer_process(std::string request_topic, std::string response_topic) :
		m_request_topic(std::move(request_topic)),
		m_response_topic(std::move(response_topic)),
		m_ready_file(m_directory.path() / "ready")
	{
		const auto started = m_process.start(
			ALTUN_PERFORMANCE_SBUS_EXECUTABLE,
			"--peer", transport_traits<Interface>::name,
			m_ready_file.string(), m_request_topic, m_response_topic
		);
		if( not started )
			throw std::runtime_error("failed to start SBus echo process");

		const auto deadline = clock::now() + 10s;
		while( not std::filesystem::exists(m_ready_file) )
		{
			if( clock::now() >= deadline )
			{
				stop_process(m_process);
				throw std::runtime_error("SBus echo process did not become ready");
			}
			std::this_thread::sleep_for(5ms);
		}
		m_running = true;
	}

	~peer_process()
	{
		if( not m_running )
			return ;
		std::vector<std::byte> payload(sizeof(message_header));
		set_header(payload, stop_run, 0);
		Interface::publish(m_request_topic, payload.data(), payload.size());
		const auto exit_code = m_process.join(10s);
		if( exit_code != 0 )
			stop_process(m_process);
	}

	peer_process(const peer_process&) = delete;
	peer_process &operator=(const peer_process&) = delete;

	[[nodiscard]] std::string_view request_topic() const noexcept {
		return m_request_topic;
	}

private:
	temporary_directory m_directory;
	riwo::utils::process m_process;
	std::string m_request_topic;
	std::string m_response_topic;
	std::filesystem::path m_ready_file;
	bool m_running = false;
};

template <typename Interface>
class transport_benchmark
{
public:
	transport_benchmark() :
		m_subscriber(m_pool),
		m_prefix("altun.performance.sbus." +
			std::string(transport_traits<Interface>::name) + "." +
			std::to_string(clock::now().time_since_epoch().count())),
		m_local_topic(m_prefix + ".local"),
		m_request_topic(m_prefix + ".request"),
		m_response_topic(m_prefix + ".response")
	{
		m_subscriber.subscribe(m_local_topic,
			[this](const void *data, size_t size) { m_local_receiver(data, size); });
		m_subscriber.subscribe(m_response_topic,
			[this](const void *data, size_t size) { m_ipc_receiver(data, size); });
	}

	~transport_benchmark()
	{
		m_subscriber.cancel();
		m_pool.stop();
		m_pool.join();
	}

	[[nodiscard]] std::vector<result> run()
	{
		std::vector<result> results;
		std::cerr << "[PERF] " << transport_traits<Interface>::name
			<< ": local benchmark\n";
		warm_up_local();
		measure_scope(results, "local", m_local_receiver, m_local_topic, false);

		std::cerr << "[PERF] " << transport_traits<Interface>::name
			<< ": IPC benchmark\n";
		peer_process<Interface> peer(m_request_topic, m_response_topic);
		warm_up_ipc(peer.request_topic());
		measure_scope(results, "ipc-round-trip", m_ipc_receiver,
			peer.request_topic(), true);
		return results;
	}

private:
	uint64_t next_run() noexcept {
		return ++m_run;
	}

	void warm_up_local()
	{
		(void) measure_throughput<Interface>(
			m_local_receiver, m_local_topic, payload_sizes.front(), 128, next_run()
		);
	}

	void warm_up_ipc(std::string_view topic)
	{
		std::vector<std::byte> payload(payload_sizes.front(), std::byte {0x2a});
		for(size_t retry = 0; retry < 200; ++retry)
		{
			const auto run = next_run();
			m_ipc_receiver.activate(run, payload.size());
			set_header(payload, run, 0);
			Interface::publish(topic, payload.data(), payload.size());
			if( m_ipc_receiver.wait(1, 50ms) )
			{
				m_ipc_receiver.verify(1);
				return ;
			}
		}
		throw std::runtime_error("SBus echo process was not discovered");
	}

	void measure_scope(
		std::vector<result> &results,
		std::string_view scope,
		receiver &target,
		std::string_view topic,
		bool ipc
	)
	{
		for(const auto payload_size : payload_sizes)
		{
			const auto count = ipc ?
				ipc_message_count(payload_size) : local_message_count(payload_size);
			std::vector<duration> publish_samples;
			std::vector<duration> end_to_end_samples;
			publish_samples.reserve(sample_count);
			end_to_end_samples.reserve(sample_count);

			for(size_t sample = 0; sample < sample_count; ++sample)
			{
				const auto measured = measure_throughput<Interface>(
					target, topic, payload_size, count, next_run()
				);
				publish_samples.emplace_back(measured.publish_elapsed);
				end_to_end_samples.emplace_back(measured.end_to_end_elapsed);
			}
			results.push_back({std::string(transport_traits<Interface>::name),
				std::string(scope), "publish-throughput", payload_size, count,
				altun_test::performance::median(std::move(publish_samples))});
			results.push_back({std::string(transport_traits<Interface>::name),
				std::string(scope), "end-to-end-throughput", payload_size, count,
				altun_test::performance::median(std::move(end_to_end_samples))});
		}

		const auto latency_count = (ipc ? 100U : 200U) *
			altun_test::performance::scale;
		auto latency = measure_latency<Interface>(
			target, topic, payload_sizes.front(), latency_count, next_run()
		);
		results.push_back({std::string(transport_traits<Interface>::name),
			std::string(scope), "latency-p50", payload_sizes.front(), 1,
			altun_test::performance::percentile(latency, 0.50)});
		results.push_back({std::string(transport_traits<Interface>::name),
			std::string(scope), "latency-p95", payload_sizes.front(), 1,
			altun_test::performance::percentile(latency, 0.95)});
		results.push_back({std::string(transport_traits<Interface>::name),
			std::string(scope), "latency-p99", payload_sizes.front(), 1,
			altun_test::performance::percentile(std::move(latency), 0.99)});
	}

	asio::thread_pool m_pool {1};
	subscriber_t<Interface> m_subscriber;
	receiver m_local_receiver;
	receiver m_ipc_receiver;
	std::string m_prefix;
	std::string m_local_topic;
	std::string m_request_topic;
	std::string m_response_topic;
	uint64_t m_run = 0;
};

void write_results(const std::filesystem::path &path, const std::vector<result> &results)
{
	std::ofstream output(path);
	for(const auto &value : results)
	{
		output << value.transport << '\t' << value.scope << '\t' << value.metric << '\t'
			<< value.payload_size << '\t' << value.operations << '\t'
			<< std::chrono::duration_cast<std::chrono::nanoseconds>(value.elapsed).count()
			<< '\n';
	}
	if( not output.good() )
		throw std::runtime_error("failed to write SBus benchmark results");
}

std::vector<result> read_results(const std::filesystem::path &path)
{
	std::ifstream input(path);
	std::vector<result> results;
	std::string line;
	while( std::getline(input, line) )
	{
		std::istringstream fields(line);
		result value;
		std::string payload_size;
		std::string operations;
		std::string elapsed;
		if( not std::getline(fields, value.transport, '\t') or
			not std::getline(fields, value.scope, '\t') or
			not std::getline(fields, value.metric, '\t') or
			not std::getline(fields, payload_size, '\t') or
			not std::getline(fields, operations, '\t') or
			not std::getline(fields, elapsed, '\t') )
			throw std::runtime_error("invalid SBus benchmark result row");
		value.payload_size = std::stoull(payload_size);
		value.operations = std::stoull(operations);
		value.elapsed = std::chrono::duration_cast<duration>(
			std::chrono::nanoseconds(std::stoll(elapsed))
		);
		results.emplace_back(std::move(value));
	}
	if( not input.eof() or results.empty() )
		throw std::runtime_error("failed to read SBus benchmark results");
	return results;
}

void print_comparison(
	const std::vector<result> &numerator,
	const std::vector<result> &denominator
)
{
	if( numerator.size() != denominator.size() )
		throw std::runtime_error("SBus benchmark result sets do not match");
	for(size_t index = 0; index < numerator.size(); ++index)
	{
		const auto &numerator_value = numerator[index];
		const auto &denominator_value = denominator[index];
		if( numerator_value.scope != denominator_value.scope or
			numerator_value.metric != denominator_value.metric or
			numerator_value.payload_size != denominator_value.payload_size or
			numerator_value.operations != denominator_value.operations )
			throw std::runtime_error("SBus benchmark result keys do not match");

		const auto numerator_cost =
			altun_test::performance::nanoseconds_per_operation(numerator_value);
		const auto denominator_cost =
			altun_test::performance::nanoseconds_per_operation(denominator_value);
		std::cout << std::fixed << std::setprecision(2)
			<< "[COMPARE] scope=" << numerator_value.scope
			<< " metric=" << numerator_value.metric
			<< " payload=" << numerator_value.payload_size << "B "
			<< numerator_value.transport << '/' << denominator_value.transport
			<< "-cost=" << numerator_cost / denominator_cost << 'x'
			<< ' ' << numerator_value.transport << '=' << numerator_cost
			<< " ns/message " << denominator_value.transport << '='
			<< denominator_cost << " ns/message\n";
	}
}

template <typename Interface>
int run_worker(const std::filesystem::path &output)
{
	transport_benchmark<Interface> benchmark;
	write_results(output, benchmark.run());
	return 0;
}

int run_child(const std::string &transport, const std::filesystem::path &output)
{
#if ALTUN_PERFORMANCE_SBUS_DBUS
	if( transport == "dbus" )
		return run_worker<altun::sbus::dbus_interface>(output);
#endif
#if ALTUN_PERFORMANCE_SBUS_CYCLONE
	if( transport == "cyclone" )
		return run_worker<altun::sbus::cyclone_interface>(output);
#endif
#if ALTUN_PERFORMANCE_SBUS_SHM
	if( transport == "shm" )
		return run_worker<altun::sbus::shm_interface>(output);
#endif
	throw std::invalid_argument("unknown SBus transport: " + transport);
}

int run_peer_child(
	const std::string &transport,
	const std::filesystem::path &ready_file,
	std::string_view request_topic,
	std::string_view response_topic
)
{
#if ALTUN_PERFORMANCE_SBUS_DBUS
	if( transport == "dbus" )
		return run_peer<altun::sbus::dbus_interface>(
			ready_file, request_topic, response_topic);
#endif
#if ALTUN_PERFORMANCE_SBUS_CYCLONE
	if( transport == "cyclone" )
		return run_peer<altun::sbus::cyclone_interface>(
			ready_file, request_topic, response_topic);
#endif
#if ALTUN_PERFORMANCE_SBUS_SHM
	if( transport == "shm" )
		return run_peer<altun::sbus::shm_interface>(
			ready_file, request_topic, response_topic);
#endif
	throw std::invalid_argument("unknown SBus transport: " + transport);
}

std::vector<result> launch_worker(
	std::string_view transport,
	const std::filesystem::path &output
)
{
	riwo::utils::process worker;
	const auto started = worker.start(
		ALTUN_PERFORMANCE_SBUS_EXECUTABLE,
		"--benchmark", transport, output.string()
	);
	if( not started )
		throw std::runtime_error("failed to start SBus benchmark worker");
	const auto exit_code = worker.join(120s);
	if( exit_code != 0 )
	{
		stop_process(worker);
		throw std::runtime_error("SBus benchmark worker failed");
	}
	return read_results(output);
}

#if ALTUN_PERFORMANCE_SBUS_SHM
void isolate_shm_namespace()
{
	const char *configured = std::getenv("ALTUN_SBUS_SHM_NAME");
	if( not configured )
		return;
	const auto name = std::string(configured) + '-' + std::to_string(getpid());
	if( setenv("ALTUN_SBUS_SHM_NAME", name.c_str(), 1) != 0 )
		throw std::runtime_error("failed to isolate SBus shared-memory namespace");
}
#endif

int run_comparison()
{
	std::vector<std::vector<result>> result_sets;
#if ALTUN_PERFORMANCE_SBUS_SHM
	isolate_shm_namespace();
#endif
	temporary_directory directory;
#if ALTUN_PERFORMANCE_SBUS_DBUS
	result_sets.emplace_back(
		launch_worker("dbus", directory.path() / "dbus.tsv")
	);
#endif
#if ALTUN_PERFORMANCE_SBUS_CYCLONE
	result_sets.emplace_back(
		launch_worker("cyclone", directory.path() / "cyclone.tsv")
	);
#endif
#if ALTUN_PERFORMANCE_SBUS_SHM
	result_sets.emplace_back(
		launch_worker("shm", directory.path() / "shm.tsv")
	);
#endif
	for(const auto &results : result_sets)
	{
		for(const auto &value : results)
			altun_test::performance::print_result(value);
	}
	if( result_sets.size() > 1 )
		std::cout << "\n[COMPARE] cost ratio above 1.0 means the numerator is slower\n";
	for(size_t numerator = 0; numerator < result_sets.size(); ++numerator)
	{
		for(size_t denominator = numerator + 1;
			denominator < result_sets.size(); ++denominator)
			print_comparison(result_sets[numerator], result_sets[denominator]);
	}
	return 0;
}

} //namespace

int main(int argc, char **argv)
{
	try
	{
		if( argc == 4 and std::string_view(argv[1]) == "--benchmark" )
			return run_child(argv[2], argv[3]);
		if( argc == 6 and std::string_view(argv[1]) == "--peer" )
			return run_peer_child(argv[2], argv[3], argv[4], argv[5]);
		if( argc != 1 )
		{
			std::cerr << "usage: " << argv[0] << '\n';
			return 64;
		}
		return run_comparison();
	}
	catch(const std::exception &exception)
	{
		std::cerr << "SBus performance benchmark failed: " << exception.what() << '\n';
		return 1;
	}
}
