// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "../../support/linux/bus.h"

#include <altun/linux/bus/spi.h>

#include <array>
#include <cstdint>
#include <future>
#include <system_error>
#include <vector>

using namespace std::chrono_literals;
using namespace altun_test_support;

namespace
{

template <typename Device, typename Buffer>
concept spi_array_readable = requires(Device &device) {
	device.template read<Buffer>();
};

template <typename Device>
concept spi_detached_array_readable = requires(Device &device) {
	device.template read<std::array<std::uint8_t,1>>(riwo::detached);
};

using compile_test_spi = altun::bus::spi;
static_assert(spi_array_readable<compile_test_spi,std::array<std::uint8_t,1>>);
static_assert(not spi_array_readable<compile_test_spi,std::vector<std::uint8_t>>);
static_assert(not spi_detached_array_readable<compile_test_spi>);

} // namespace

ALTUN_TEST("virtual-device", "SPI configures and transfers through an ioctl simulator")
{
	using spi_type = altun::bus::basic_spi<asio::io_context::executor_type>;
	temporary_file backing_file;
	asio::io_context context;
	spi_type device(context.get_executor());
	std::error_code error;

	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		virtual_spi_devices.clear();
	}
	device.open(spi_type::node(backing_file.path(), 4'000'000,
		altun::bus::spi_mode3, 16, 12us, true), error);
	ALTUN_REQUIRE(not error);
	ALTUN_REQUIRE(device.is_open());
	ALTUN_REQUIRE_EQ(device.attributes().mode,
		static_cast<altun::bus::spi_mode_t>(altun::bus::spi_mode3));
	ALTUN_REQUIRE_EQ(device.attributes().max_speed_hz, 4'000'000U);
	ALTUN_REQUIRE_EQ(device.attributes().bits_per_word, 16);
	ALTUN_REQUIRE_EQ(device.attributes().delay, 12us);
	ALTUN_REQUIRE(device.attributes().cs_change);

	const int descriptor = device.handle().native_handle();
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		const auto &state = virtual_spi_devices.at(descriptor);
		ALTUN_REQUIRE_EQ(state.mode, static_cast<std::uint32_t>(SPI_MODE_3));
		ALTUN_REQUIRE_EQ(state.max_speed_hz, 4'000'000U);
		ALTUN_REQUIRE_EQ(state.bits_per_word, 16);
	}
	const auto empty = device.read<std::array<std::uint8_t,0>>(error);
	ALTUN_REQUIRE(not error);
	ALTUN_REQUIRE(empty.empty());
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		ALTUN_REQUIRE_EQ(virtual_spi_devices.at(descriptor).read_count, 0U);
	}

	const std::array<std::uint8_t, 3> payload {0x9F, 0x00, 0x00};
	const auto written = device.write(
		riwo::const_buffer(payload.data(), payload.size()), error
	);
	ALTUN_REQUIRE(not error);
	ALTUN_REQUIRE_EQ(written, payload.size());
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		const auto &state = virtual_spi_devices.at(descriptor);
		ALTUN_REQUIRE_EQ(state.written_data,
			(std::vector<std::uint8_t> {0x9F, 0x00, 0x00}));
		ALTUN_REQUIRE_EQ(state.transfer_speed_hz, 4'000'000U);
		ALTUN_REQUIRE_EQ(state.delay_usecs, 12);
		ALTUN_REQUIRE_EQ(state.transfer_bits_per_word, 16);
		ALTUN_REQUIRE(state.cs_change);
		virtual_spi_devices[descriptor].read_data = {0xEF, 0x40, 0x18};
	}

	std::array<std::uint8_t, 3> received {};
	const auto read = device.read(
		riwo::mutable_buffer(received.data(), received.size()), error
	);
	ALTUN_REQUIRE(not error);
	ALTUN_REQUIRE_EQ(read, received.size());
	ALTUN_REQUIRE_EQ(received,
		(std::array<std::uint8_t, 3> {0xEF, 0x40, 0x18}));
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		virtual_spi_devices[descriptor].read_data = {0x12, 0x34, 0x56};
	}
	const auto array_received = device.read<std::array<std::uint8_t,3>>();
	ALTUN_REQUIRE_EQ(array_received,
		(std::array<std::uint8_t, 3> {0x12, 0x34, 0x56}));

	const std::array<std::uint8_t, 2> command {0x05, 0x00};
	std::array<std::uint8_t, 2> response {};
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		virtual_spi_devices[descriptor].read_data = {0x00, 0x02};
	}
	const auto transferred = device.transfer(
		riwo::const_buffer(command.data(), command.size()),
		riwo::mutable_buffer(response.data(), response.size()), error
	);
	ALTUN_REQUIRE(not error);
	ALTUN_REQUIRE_EQ(transferred, command.size());
	ALTUN_REQUIRE_EQ(response, (std::array<std::uint8_t, 2> {0x00, 0x02}));
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		ALTUN_REQUIRE_EQ(virtual_spi_devices.at(descriptor).written_data,
			(std::vector<std::uint8_t> {0x05, 0x00}));
	}

	std::array<std::uint8_t, 2> async_command {0x0B, 0x00};
	std::array<std::uint8_t, 2> async_response {};
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		virtual_spi_devices[descriptor].read_data = {0xAA, 0x55};
	}
	auto future = device.transfer(
		riwo::const_buffer(async_command.data(), async_command.size()),
		riwo::mutable_buffer(async_response.data(), async_response.size()),
		riwo::use_future
	);
	context.run();
	ALTUN_REQUIRE_EQ(future.get(), async_command.size());
	ALTUN_REQUIRE_EQ(async_response,
		(std::array<std::uint8_t, 2> {0xAA, 0x55}));

	context.restart();
	std::array<std::uint8_t, 2> detached_payload {0x12, 0x34};
	device.write(riwo::const_buffer(detached_payload.data(), detached_payload.size()),
		riwo::detached);
	detached_payload.fill(0);
	context.run();
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		ALTUN_REQUIRE_EQ(virtual_spi_devices.at(descriptor).written_data,
			(std::vector<std::uint8_t> {0x12, 0x34}));
		virtual_spi_devices[descriptor].read_data = {0xDE, 0xAD};
	}

	context.restart();
	auto read_future = device.read<std::array<std::uint8_t,2>>(riwo::use_future);
	context.run();
	ALTUN_REQUIRE_EQ(read_future.get(),
		(std::array<std::uint8_t, 2> {0xDE, 0xAD}));

	using batch_buffer = std::array<std::uint8_t,4>;
	constexpr std::size_t batch_size = 64;
	std::size_t read_count_before = 0;
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		auto &state = virtual_spi_devices.at(descriptor);
		read_count_before = state.read_count;
		for(std::size_t index = 0; index < batch_size; ++index)
		{
			state.read_sequence.push_back({
				static_cast<std::uint8_t>(index + 3),
				static_cast<std::uint8_t>(index + 2),
				static_cast<std::uint8_t>(index + 1),
				static_cast<std::uint8_t>(index)
			});
		}
	}
	context.restart();
	std::vector<std::future<batch_buffer>> futures;
	futures.reserve(batch_size);
	for(std::size_t index = 0; index < batch_size; ++index)
		futures.push_back(device.read<batch_buffer>(riwo::use_future));
	context.run();
	for(std::size_t index = 0; index < batch_size; ++index)
	{
		const batch_buffer expected {
			static_cast<std::uint8_t>(index + 3),
			static_cast<std::uint8_t>(index + 2),
			static_cast<std::uint8_t>(index + 1),
			static_cast<std::uint8_t>(index)
		};
		ALTUN_REQUIRE_EQ(futures[index].get(), expected);
	}
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		const auto &state = virtual_spi_devices.at(descriptor);
		ALTUN_REQUIRE(state.read_sequence.empty());
		ALTUN_REQUIRE_EQ(state.read_count, read_count_before + batch_size);
	}

	device.close(error);
	ALTUN_REQUIRE(not error);
	ALTUN_REQUIRE(not device.is_open());
}

ALTUN_TEST("virtual-device", "SPI validates failures before touching hardware")
{
	using spi_type = altun::bus::basic_spi<asio::io_context::executor_type>;
	asio::io_context context;
	spi_type device(context.get_executor());
	std::error_code error;
	std::uint8_t byte = 0;

	const auto closed_write = device.write(riwo::const_buffer(&byte, 1), error);
	ALTUN_REQUIRE_EQ(closed_write, 0U);
	ALTUN_REQUIRE_EQ(error, std::make_error_code(std::errc::bad_file_descriptor));
	const auto closed_read = device.read<std::array<std::uint8_t,2>>(error);
	ALTUN_REQUIRE_EQ(closed_read, (std::array<std::uint8_t, 2> {}));
	ALTUN_REQUIRE_EQ(error, std::make_error_code(std::errc::bad_file_descriptor));
	const auto closed_empty = device.read<std::array<std::uint8_t,0>>(error);
	ALTUN_REQUIRE(closed_empty.empty());
	ALTUN_REQUIRE(not error);

	auto closed_future = device.read<std::array<std::uint8_t,2>>(riwo::use_future);
	context.run();
	ALTUN_REQUIRE_SYSTEM_ERROR(std::make_error_code(std::errc::bad_file_descriptor),
		static_cast<void>(closed_future.get()));

	temporary_file backing_file;
	device.open(spi_type::node(backing_file.path()), error);
	ALTUN_REQUIRE(not error);
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		virtual_spi_devices[device.handle().native_handle()].read_data = {0xA5};
	}
	const auto recovered = device.read<std::array<std::uint8_t,1>>(error);
	ALTUN_REQUIRE(not error);
	ALTUN_REQUIRE_EQ(recovered, (std::array<std::uint8_t, 1> {0xA5}));
	device.close(error);
	ALTUN_REQUIRE(not error);

	std::array<std::uint8_t, 2> response {};
	const auto mismatched = device.transfer(
		riwo::const_buffer(&byte, 1),
		riwo::mutable_buffer(response.data(), response.size()), error
	);
	ALTUN_REQUIRE_EQ(mismatched, 0U);
	ALTUN_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));

	auto handle = spi_type::make_handle(spi_type::node("/dev/null", 0),
		context.get_executor(), error);
	ALTUN_REQUIRE(not handle.is_open());
	ALTUN_REQUIRE_EQ(error, std::make_error_code(std::errc::invalid_argument));
}


ALTUN_TEST("virtual-device", "SPI array read retains operation state after device destruction")
{
	using spi_type = altun::bus::basic_spi<asio::io_context::executor_type>;
	using result_type = std::array<std::uint8_t,4>;
	reset_virtual_spi_devices();

	temporary_file backing_file;
	asio::io_context context;
	std::future<result_type> future;
	int descriptor = -1;
	{
		spi_type device(context.get_executor());
		device.open(spi_type::node(backing_file.path()));
		descriptor = device.handle().native_handle();
		{
			std::scoped_lock lock(virtual_ioctl_mutex);
			virtual_spi_devices[descriptor].read_data = {0xA0, 0xB0, 0xC0, 0xD0};
		}
		future = device.read<result_type>(riwo::use_future);
	}
	context.run();
	ALTUN_REQUIRE_EQ(future.get(),
		(result_type {0xA0, 0xB0, 0xC0, 0xD0}));
	{
		std::scoped_lock lock(virtual_ioctl_mutex);
		ALTUN_REQUIRE_EQ(virtual_spi_devices.at(descriptor).read_count, 1U);
	}
}

