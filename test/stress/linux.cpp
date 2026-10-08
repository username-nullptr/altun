// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "../test.h"
#include "../support/linux/bus.h"

#include <libempp/linux/bus/i2c.h>
#include <libempp/linux/bus/spi.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

using namespace empp_test_support;

namespace
{

constexpr std::size_t scale = LIBEMPP_STRESS_SCALE;
constexpr std::size_t device_count = 8;
constexpr std::size_t worker_count = 4;
static_assert(scale > 0);

template <typename Context>
void run_workers(Context &context)
{
	std::array<std::thread,worker_count> workers;
	for(auto &worker : workers)
		worker = std::thread([&] { context.run(); });
	for(auto &worker : workers)
		worker.join();
}

EMPP_TEST("linux", "I2C asynchronous device pressure")
{
	using device_type = libempp::bus::basic_i2c<asio::io_context::executor_type>;
	using buffer_type = std::array<std::uint8_t,8>;
	const std::size_t operations_per_device = 1'000 * scale;

	reset_virtual_i2c_devices();
	asio::io_context context;
	std::vector<std::unique_ptr<temporary_file>> files;
	std::vector<std::unique_ptr<device_type>> devices;
	files.reserve(device_count);
	devices.reserve(device_count);
	for(std::size_t index = 0; index < device_count; ++index)
	{
		files.push_back(std::make_unique<temporary_file>());
		auto device = std::make_unique<device_type>(context.get_executor());
		device->open(device_type::node(files.back()->path(),
			static_cast<std::uint16_t>(0x20 + index)));
		const auto descriptor = device->handle().native_handle();
		{
			std::scoped_lock lock(virtual_ioctl_mutex);
			virtual_i2c_devices[descriptor].read_data.assign(
				buffer_type{}.size(), static_cast<std::uint8_t>(index));
		}
		devices.push_back(std::move(device));
	}

	struct pending_read
	{
		std::uint8_t expected;
		std::future<buffer_type> result;
	};
	std::vector<pending_read> reads;
	reads.reserve(device_count * operations_per_device);
	empp_test::random_sequence random;
	for(std::size_t operation = 0; operation < operations_per_device; ++operation)
	{
		const auto offset = random.bounded(devices.size());
		for(std::size_t position = 0; position < devices.size(); ++position)
		{
			const auto index = (position + offset) % devices.size();
			reads.push_back({static_cast<std::uint8_t>(index),
				devices[index]->read<buffer_type>(
					static_cast<std::uint8_t>(operation), riwo::use_future)});
		}
	}

	run_workers(context);
	for(auto &read : reads)
	{
		const auto value = read.result.get();
		for(const auto byte : value)
			EMPP_REQUIRE_EQ(byte, read.expected);
	}
	for(const auto &device : devices)
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		EMPP_REQUIRE_EQ(
			virtual_i2c_devices.at(device->handle().native_handle()).read_count,
			operations_per_device);
	}
}

EMPP_TEST("linux", "SPI asynchronous device pressure")
{
	using device_type = libempp::bus::basic_spi<asio::io_context::executor_type>;
	using buffer_type = std::array<std::uint8_t,8>;
	const std::size_t operations_per_device = 1'000 * scale;

	reset_virtual_spi_devices();
	asio::io_context context;
	std::vector<std::unique_ptr<temporary_file>> files;
	std::vector<std::unique_ptr<device_type>> devices;
	files.reserve(device_count);
	devices.reserve(device_count);
	for(std::size_t index = 0; index < device_count; ++index)
	{
		files.push_back(std::make_unique<temporary_file>());
		auto device = std::make_unique<device_type>(context.get_executor());
		device->open(device_type::node(files.back()->path()));
		const auto descriptor = device->handle().native_handle();
		{
			std::scoped_lock lock(virtual_ioctl_mutex);
			virtual_spi_devices[descriptor].read_data.assign(
				buffer_type{}.size(), static_cast<std::uint8_t>(index + 0x40));
		}
		devices.push_back(std::move(device));
	}

	struct pending_read
	{
		std::uint8_t expected;
		std::future<buffer_type> result;
	};
	std::vector<pending_read> reads;
	reads.reserve(device_count * operations_per_device);
	empp_test::random_sequence random;
	for(std::size_t operation = 0; operation < operations_per_device; ++operation)
	{
		const auto offset = random.bounded(devices.size());
		for(std::size_t position = 0; position < devices.size(); ++position)
		{
			const auto index = (position + offset) % devices.size();
			reads.push_back({static_cast<std::uint8_t>(index + 0x40),
				devices[index]->read<buffer_type>(riwo::use_future)});
		}
	}

	run_workers(context);
	for(auto &read : reads)
	{
		const auto value = read.result.get();
		for(const auto byte : value)
			EMPP_REQUIRE_EQ(byte, read.expected);
	}
	for(const auto &device : devices)
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		EMPP_REQUIRE_EQ(
			virtual_spi_devices.at(device->handle().native_handle()).read_count,
			operations_per_device);
	}
}

} // namespace
