// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "../../test.h"

#include <libempp/core/sbus/sbus.h>
#include <riwo/utils/process.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

EMPP_TEST("core.sbus", "local delivery is exact and subscriptions can be cancelled")
{
	using namespace std::chrono_literals;
	constexpr std::string_view topic = "libempp.test.sbus.local";
	const std::array<std::byte,6> payload {
		std::byte {0x00}, std::byte {0x11}, std::byte {0x7f},
		std::byte {0x80}, std::byte {0xfe}, std::byte {0xff}
	};

	asio::thread_pool pool(2);
	libempp::sbus::subscriber topic_subscriber(pool);
	libempp::sbus::subscriber global_subscriber(pool);
	std::mutex mutex;
	std::condition_variable changed;
	std::vector<std::byte> topic_payload;
	std::vector<std::byte> global_payload;
	std::string global_topic;
	std::size_t topic_count = 0;
	std::size_t global_count = 0;

	const auto topic_sid = topic_subscriber.subscribe(topic,
	[&](const void *data, size_t size)
	{
		auto begin = static_cast<const std::byte*>(data);
		std::lock_guard lock(mutex);
		topic_payload.assign(begin, begin + size);
		++topic_count;
		changed.notify_one();
	});
	global_subscriber.subscribe(
	[&](std::string_view received_topic, const void *data, size_t size)
	{
		auto begin = static_cast<const std::byte*>(data);
		std::lock_guard lock(mutex);
		global_topic = received_topic;
		global_payload.assign(begin, begin + size);
		++global_count;
		changed.notify_one();
	});

	libempp::sbus::publish(topic, payload.data(), payload.size());
	{
		std::unique_lock lock(mutex);
		EMPP_REQUIRE(changed.wait_for(lock, 2s, [&] {
			return topic_count == 1 and global_count == 1;
		}));
		EMPP_REQUIRE_EQ(topic_payload, std::vector(payload.begin(), payload.end()));
		EMPP_REQUIRE_EQ(global_payload, std::vector(payload.begin(), payload.end()));
		EMPP_REQUIRE_EQ(global_topic, std::string(topic));
	}

	// A transport loopback must not duplicate the process-local delivery.
	std::this_thread::sleep_for(100ms);
	{
		std::lock_guard lock(mutex);
		EMPP_REQUIRE_EQ(topic_count, 1U);
		EMPP_REQUIRE_EQ(global_count, 1U);
	}

	topic_subscriber.cancel_sid(topic_sid);
	libempp::sbus::publish(topic, payload.data(), payload.size());
	{
		std::unique_lock lock(mutex);
		EMPP_REQUIRE(changed.wait_for(lock, 2s, [&] { return global_count == 2; }));
		EMPP_REQUIRE_EQ(topic_count, 1U);
	}

	global_subscriber.cancel();
	pool.stop();
	pool.join();
}

#if defined(LIBEMPP_TEST_SBUS_INTERPROCESS)
EMPP_TEST("core.sbus", "selected transport carries binary messages between processes")
{
	using namespace std::chrono_literals;
	empp_test::temporary_directory directory;
	const auto ready_file = directory.path() / "ready";
	const auto result_file = directory.path() / "result";
	std::vector<char> payload(1 * 1'024 * 1'024 + 37);
	for(size_t index = 0; index < payload.size(); ++index)
		payload[index] = static_cast<char>((index * 31U) & 0xffU);

	riwo::utils::process peer;
	const auto started = peer.start(
		LIBEMPP_TEST_SBUS_PEER_FILE, ready_file.string(), result_file.string()
	);
	EMPP_REQUIRE(started);

	for(int retry = 0; retry < 200 and not std::filesystem::exists(ready_file); ++retry)
		std::this_thread::sleep_for(10ms);
	EMPP_REQUIRE(std::filesystem::exists(ready_file));

	libempp::sbus::publish(
		"libempp.test.sbus.interprocess", payload.data(), payload.size()
	);
	EMPP_REQUIRE_EQ(peer.join(6s), 0);

	std::ifstream input(result_file, std::ios::binary);
	const std::vector<char> received {
		std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()
	};
	EMPP_REQUIRE_EQ(received, payload);
}

EMPP_TEST("core.sbus", "selected transport drains an ordered interprocess burst")
{
	using namespace std::chrono_literals;
	constexpr size_t message_count = 512;
	empp_test::temporary_directory directory;
	const auto ready_file = directory.path() / "ready";
	const auto result_file = directory.path() / "result";

	riwo::utils::process peer;
	const auto started = peer.start(
		LIBEMPP_TEST_SBUS_PEER_FILE, ready_file.string(), result_file.string(),
		std::to_string(message_count)
	);
	EMPP_REQUIRE(started);

	for(int retry = 0; retry < 200 and not std::filesystem::exists(ready_file); ++retry)
		std::this_thread::sleep_for(10ms);
	EMPP_REQUIRE(std::filesystem::exists(ready_file));

	for(uint64_t sequence = 0; sequence < message_count; ++sequence)
	{
		libempp::sbus::publish(
			"libempp.test.sbus.interprocess", &sequence, sizeof(sequence)
		);
	}
	EMPP_REQUIRE_EQ(peer.join(10s), 0);

	std::ifstream input(result_file);
	std::string result;
	input >> result;
	EMPP_REQUIRE_EQ(result, "ok");
}

#if defined(LIBEMPP_TEST_SBUS_SHM)
EMPP_TEST("core.sbus", "shared memory reclaims a crashed reader when the ring fills")
{
	using namespace std::chrono_literals;
	constexpr size_t message_count = 1'024;
	empp_test::temporary_directory directory;
	const auto ready_file = directory.path() / "ready";
	const auto result_file = directory.path() / "result";

	riwo::utils::process peer;
	const auto started = peer.start(
		LIBEMPP_TEST_SBUS_PEER_FILE, ready_file.string(), result_file.string(), "crash"
	);
	EMPP_REQUIRE(started);

	for(int retry = 0; retry < 200 and not std::filesystem::exists(ready_file); ++retry)
		std::this_thread::sleep_for(10ms);
	EMPP_REQUIRE(std::filesystem::exists(ready_file));

	const uint64_t trigger = 0;
	libempp::sbus::publish(
		"libempp.test.sbus.interprocess", &trigger, sizeof(trigger)
	);
	EMPP_REQUIRE_EQ(peer.join(2s), 0);
	EMPP_REQUIRE(not peer.joinable());

	const auto begin = std::chrono::steady_clock::now();
	for(uint64_t sequence = 0; sequence < message_count; ++sequence)
	{
		libempp::sbus::publish(
			"libempp.test.sbus.crashed-reader", &sequence, sizeof(sequence)
		);
	}
	EMPP_REQUIRE(std::chrono::steady_clock::now() - begin < 2s);
}
#endif
#endif
