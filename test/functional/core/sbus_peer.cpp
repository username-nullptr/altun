// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <libempp/core/sbus/sbus.h>

#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>

int main(int argc, char **argv)
{
	using namespace std::chrono_literals;
	if( argc != 3 and argc != 4 )
		return 64;

	const std::filesystem::path ready_file = argv[1];
	const std::filesystem::path result_file = argv[2];
	size_t expected_count = 0;
	bool crash_on_message = false;
	if( argc == 4 )
	{
		const std::string_view text = argv[3];
		if( text == "crash" )
			crash_on_message = true;
		else
		{
			const auto parsed = std::from_chars(
				text.data(), text.data() + text.size(), expected_count
			);
			if( parsed.ec != std::errc{} or parsed.ptr != text.data() + text.size() or
				expected_count == 0 )
				return 64;
		}
	}
	asio::thread_pool pool(1);
	libempp::sbus::subscriber subscriber(pool);
	std::mutex mutex;
	std::condition_variable changed;
	bool received = false;
	size_t received_count = 0;
	bool valid = true;

	subscriber.subscribe("libempp.test.sbus.interprocess",
	[&](const void *data, size_t size)
	{
		if( crash_on_message )
			std::_Exit(0);
		if( expected_count != 0 )
		{
			std::lock_guard lock(mutex);
			uint64_t sequence = 0;
			if( size != sizeof(sequence) )
				valid = false;
			else
			{
				std::memcpy(&sequence, data, sizeof(sequence));
				valid = valid and sequence == received_count;
			}
			++received_count;
			if( received_count == expected_count )
			{
				std::ofstream output(result_file);
				output << (valid ? "ok" : "invalid");
				received = output.good() and valid;
				changed.notify_one();
			}
			return ;
		}

		std::ofstream output(result_file, std::ios::binary);
		output.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
		{
			std::lock_guard lock(mutex);
			received = output.good();
		}
		changed.notify_one();
	});

	{
		std::ofstream ready(ready_file);
		ready << "ready";
		if( not ready.good() )
			return 65;
	}

	std::unique_lock lock(mutex);
	const bool completed = changed.wait_for(lock, 5s, [&] { return received; });
	lock.unlock();
	subscriber.cancel();
	pool.stop();
	pool.join();
	return completed ? 0 : 66;
}
