// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "../test.h"

#include <altun/core/global.h>
#include <altun/core/plugin_manager.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace
{

constexpr std::size_t scale = ALTUN_STRESS_SCALE;
static_assert(scale > 0);

ALTUN_TEST("core", "plugin descriptor parsing pressure")
{
	using manager = altun::plugin_manager;
	using json = manager::json_t;
	constexpr std::size_t descriptor_count = 32;
	const std::size_t repetitions = 250 * scale;

	json config = json::array();
	for(std::size_t index = 0; index < descriptor_count; ++index)
	{
		const auto suffix = std::to_string(index);
		config.push_back({
			{"path", "pack-" + suffix},
			{"apps", {
				{"worker-" + suffix, {
					{"file_name", "worker-" + suffix},
					{"args", {"--slot", suffix}},
					{"envs", {{"SLOT", suffix}}}
				}}
			}},
			{"libs", {
				{"codec-" + suffix, "codec-" + suffix + ".so"}
			}}
		});
	}

	for(std::size_t iteration = 0; iteration < repetitions; ++iteration)
	{
		manager::set_config(config);
		manager::parse();
		const auto processes = manager::process_nodes();
		const auto libraries = manager::library_nodes();
		ALTUN_REQUIRE_EQ(processes.size(), descriptor_count);
		ALTUN_REQUIRE_EQ(libraries.size(), descriptor_count);
		ALTUN_REQUIRE_EQ(processes.front().name, std::string("worker-0"));
		const auto final_library = std::string("codec-") +
			std::to_string(descriptor_count - 1);
		ALTUN_REQUIRE(std::ranges::find_if(libraries, [&](const auto &node) {
			return node.name == final_library;
		}) != libraries.end());
	}
}

ALTUN_TEST("core", "shared library interface concurrency pressure")
{
	riwo::library library(std::filesystem::path(ALTUN_TEST_PLUGIN_FILE));
	ALTUN_REQUIRE(library.load());
	const auto function = library.interface<int(int)>("altun_test_double");
	ALTUN_REQUIRE(function);

	constexpr std::size_t thread_count = 8;
	const std::size_t calls_per_thread = 25'000 * scale;
	const auto seed = altun_test::current_seed();
	std::atomic_bool corrupt {false};
	std::atomic_size_t completed {0};
	std::array<std::thread,thread_count> threads;
	for(std::size_t thread_index = 0; thread_index < thread_count; ++thread_index)
	{
		threads[thread_index] = std::thread([&, thread_index]
		{
			altun_test::random_sequence random(seed ^ (thread_index + 1));
			for(std::size_t index = 0; index < calls_per_thread; ++index)
			{
				const auto value = static_cast<int>(random.bounded(200'001)) - 100'000;
				if((*function)(value) != value * 2 or
					std::string_view(altun::version_string()).empty())
				{
					corrupt.store(true, std::memory_order_relaxed);
				}
			}
			completed.fetch_add(1, std::memory_order_release);
		});
	}
	for(auto &thread : threads)
		thread.join();

	ALTUN_REQUIRE_EQ(completed.load(std::memory_order_acquire), thread_count);
	ALTUN_REQUIRE(not corrupt.load(std::memory_order_relaxed));
	ALTUN_REQUIRE(library.unload());
}

} // namespace
