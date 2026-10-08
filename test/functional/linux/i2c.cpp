// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "../../support/linux/bus.h"

#include <libempp/linux/bus/i2c.h>

#include <array>
#include <cstdint>
#include <future>
#include <limits>
#include <ranges>
#include <system_error>
#include <vector>

using namespace std::chrono_literals;
using namespace empp_test_support;

namespace
{

template <typename Device, typename Buffer>
concept i2c_array_readable = requires(Device &device) {
	device.template read<Buffer>(std::uint8_t {});
};

template <typename Device>
concept i2c_detached_array_readable = requires(Device &device) {
	device.template read<std::array<std::uint8_t,1>>(
		std::uint8_t {}, riwo::detached
	);
};

using compile_test_i2c = libempp::bus::i2c;
static_assert(i2c_array_readable<compile_test_i2c,std::array<std::uint8_t,1>>);
static_assert(not i2c_array_readable<compile_test_i2c,std::vector<std::uint8_t>>);
static_assert(not i2c_detached_array_readable<compile_test_i2c>);
static_assert(not riwo::concepts::array_buffer<std::array<const std::uint8_t,1>>);
static_assert(not compile_test_i2c::is_valid_reg_bit_v<
	static_cast<libempp::bus::i2c_reg_bit>(3)>);

} // namespace

EMPP_TEST("virtual-device", "I2C reads and writes registers through an ioctl simulator")
{
	using i2c_type = libempp::bus::basic_i2c<asio::io_context::executor_type>;
	temporary_file backing_file;
	asio::io_context context;
	i2c_type device(context.get_executor());
	std::error_code error;

	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		virtual_i2c_devices.clear();
	}
	device.open(i2c_type::node(backing_file.path(), 0x52, 25ms), error);
	EMPP_REQUIRE(not error);
	EMPP_REQUIRE(device.is_open());
	EMPP_REQUIRE_EQ(device.attributes().address, 0x52);
	EMPP_REQUIRE_EQ(device.attributes().timeout, 25ms);

	const int descriptor = device.handle().native_handle();
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		const auto &state = virtual_i2c_devices.at(descriptor);
		EMPP_REQUIRE_EQ(state.timeout_units, 3UL);
		EMPP_REQUIRE_EQ(state.slave_address, 0x52UL);
	}
	const auto empty = device.read<std::array<std::uint8_t,0>>(0x00, error);
	EMPP_REQUIRE(not error);
	EMPP_REQUIRE(empty.empty());
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		EMPP_REQUIRE_EQ(virtual_i2c_devices.at(descriptor).read_count, 0U);
	}

	const std::array<std::uint8_t, 2> payload {0xA5, 0x5A};
	const auto written = device.write(0x2A,
		riwo::const_buffer(payload.data(), payload.size()), error);
	EMPP_REQUIRE(not error);
	EMPP_REQUIRE_EQ(written, 3U);
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		const auto &state = virtual_i2c_devices.at(descriptor);
		EMPP_REQUIRE_EQ(state.message_address, 0x52);
		EMPP_REQUIRE_EQ(state.written_frame,
			(std::vector<std::uint8_t> {0x2A, 0xA5, 0x5A}));
		virtual_i2c_devices[descriptor].read_data = {0x11, 0x22, 0x33};
	}

	std::array<std::uint8_t, 3> result {};
	const auto read = device.read<libempp::bus::reg_bit16>(0x1234,
		riwo::mutable_buffer(result.data(), result.size()), error);
	EMPP_REQUIRE(not error);
	EMPP_REQUIRE_EQ(read, result.size());
	EMPP_REQUIRE_EQ(result, (std::array<std::uint8_t, 3> {0x11, 0x22, 0x33}));
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		EMPP_REQUIRE_EQ(virtual_i2c_devices.at(descriptor).register_bytes,
			(std::vector<std::uint8_t> {0x12, 0x34}));
		virtual_i2c_devices[descriptor].read_data = {0x44, 0x55, 0x66};
	}
	const auto array_result = device.read<std::array<std::uint8_t,3>,
		libempp::bus::reg_bit16>(0x5678, error);
	EMPP_REQUIRE(not error);
	EMPP_REQUIRE_EQ(array_result,
		(std::array<std::uint8_t, 3> {0x44, 0x55, 0x66}));
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		EMPP_REQUIRE_EQ(virtual_i2c_devices.at(descriptor).register_bytes,
			(std::vector<std::uint8_t> {0x56, 0x78}));
		virtual_i2c_devices[descriptor].read_data = {0x77, 0x88};
	}

	auto future = device.read<std::array<std::uint8_t,2>>(
		0x2B, riwo::use_future
	);
	context.run();
	EMPP_REQUIRE_EQ(future.get(),
		(std::array<std::uint8_t, 2> {0x77, 0x88}));
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		EMPP_REQUIRE_EQ(virtual_i2c_devices.at(descriptor).register_bytes,
			(std::vector<std::uint8_t> {0x2B}));
	}

	using batch_buffer = std::array<std::uint8_t,4>;
	constexpr std::size_t batch_size = 64;
	std::size_t read_count_before = 0;
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		auto &state = virtual_i2c_devices.at(descriptor);
		read_count_before = state.read_count;
		for(std::size_t index = 0; index < batch_size; ++index)
		{
			state.read_sequence.push_back({
				static_cast<std::uint8_t>(index),
				static_cast<std::uint8_t>(index + 1),
				static_cast<std::uint8_t>(index + 2),
				static_cast<std::uint8_t>(index + 3)
			});
		}
	}
	context.restart();
	std::vector<std::future<batch_buffer>> futures;
	futures.reserve(batch_size);
	for(std::size_t index = 0; index < batch_size; ++index)
	{
		futures.push_back(device.read<batch_buffer>(
			static_cast<std::uint8_t>(0x40 + index), riwo::use_future
		));
	}
	context.run();
	for(std::size_t index = 0; index < batch_size; ++index)
	{
		const batch_buffer expected {
			static_cast<std::uint8_t>(index),
			static_cast<std::uint8_t>(index + 1),
			static_cast<std::uint8_t>(index + 2),
			static_cast<std::uint8_t>(index + 3)
		};
		EMPP_REQUIRE_EQ(futures[index].get(), expected);
	}
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		const auto &state = virtual_i2c_devices.at(descriptor);
		EMPP_REQUIRE(state.read_sequence.empty());
		EMPP_REQUIRE_EQ(state.read_count, read_count_before + batch_size);
		EMPP_REQUIRE_EQ(state.register_bytes,
			(std::vector<std::uint8_t> {0x7F}));
	}

	device.close(error);
	EMPP_REQUIRE(not error);
	EMPP_REQUIRE(not device.is_open());
}

EMPP_TEST("virtual-device", "I2C validates failures before touching hardware")
{
	using i2c_type = libempp::bus::basic_i2c<asio::io_context::executor_type>;
	asio::io_context context;
	i2c_type device(context.get_executor());
	std::error_code error;
	std::uint8_t byte = 0;

	const auto closed_write = device.write(0x01, riwo::const_buffer(&byte, 1), error);
	EMPP_REQUIRE_EQ(closed_write, 0U);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::bad_file_descriptor));
	const auto closed_read = device.read<std::array<std::uint8_t,2>>(0x01, error);
	EMPP_REQUIRE_EQ(closed_read, (std::array<std::uint8_t, 2> {}));
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::bad_file_descriptor));
	const auto closed_empty = device.read<std::array<std::uint8_t,0>>(0x01, error);
	EMPP_REQUIRE(closed_empty.empty());
	EMPP_REQUIRE(not error);

	auto closed_future = device.read<std::array<std::uint8_t,2>>(
		0x01, riwo::use_future
	);
	context.run();
	EMPP_REQUIRE_SYSTEM_ERROR(std::make_error_code(std::errc::bad_file_descriptor),
		static_cast<void>(closed_future.get()));

	using oversized_buffer = std::array<
		std::uint8_t, std::numeric_limits<std::uint16_t>::max() + 1ULL
	>;
	const auto oversized_read = device.read<oversized_buffer>(0x01, error);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::message_size));
	EMPP_REQUIRE(std::ranges::all_of(oversized_read,
		[](std::uint8_t value) { return value == 0; }
	));

	const auto oversized = device.write(0x01,
		riwo::const_buffer(&byte, std::numeric_limits<std::uint16_t>::max()), error);
	EMPP_REQUIRE_EQ(oversized, 0U);
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::message_size));

	temporary_file backing_file;
	device.open(i2c_type::node(backing_file.path(), 0x20), error);
	EMPP_REQUIRE(not error);
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		virtual_i2c_devices[device.handle().native_handle()].read_data = {0x5A};
	}
	const auto recovered = device.read<std::array<std::uint8_t,1>>(0x02, error);
	EMPP_REQUIRE(not error);
	EMPP_REQUIRE_EQ(recovered, (std::array<std::uint8_t, 1> {0x5A}));
	device.close(error);
	EMPP_REQUIRE(not error);

	auto handle = i2c_type::make_handle(
		i2c_type::node("/path/that/is/not/an/i2c-device", 0x20, -1ms),
		context.get_executor(), error
	);
	EMPP_REQUIRE(not handle.is_open());
	EMPP_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
}


EMPP_TEST("virtual-device", "I2C array read retains operation state after device destruction")
{
	using i2c_type = libempp::bus::basic_i2c<asio::io_context::executor_type>;
	using result_type = std::array<std::uint8_t,4>;
	reset_virtual_i2c_devices();

	temporary_file backing_file;
	asio::io_context context;
	std::future<result_type> future;
	int descriptor = -1;
	{
		i2c_type device(context.get_executor());
		device.open(i2c_type::node(backing_file.path(), 0x32));
		descriptor = device.handle().native_handle();
		{
			std::scoped_lock lock(virtual_ioctl_mutex);
			virtual_i2c_devices[descriptor].read_data = {0x10, 0x20, 0x30, 0x40};
		}
		future = device.read<result_type>(0x08, riwo::use_future);
	}
	context.run();
	EMPP_REQUIRE_EQ(future.get(),
		(result_type {0x10, 0x20, 0x30, 0x40}));
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		EMPP_REQUIRE_EQ(virtual_i2c_devices.at(descriptor).read_count, 1U);
	}
}

