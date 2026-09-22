// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "../../test.h"

#include <libempp/linux/storage.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <system_error>
#include <vector>

EMPP_TEST("virtual-device", "block device validates nodes and enumeration")
{
	using libempp::storage::block_device;
	auto empty = block_device::open(libempp::storage::path_t{});
	EMPP_REQUIRE(not empty);
	EMPP_REQUIRE_EQ(empty.error(), std::make_error_code(std::errc::invalid_argument));

	for(std::size_t iteration = 0; iteration < 8; ++iteration)
	{
		auto regular = block_device::open("/dev/null");
		EMPP_REQUIRE(not regular);
		EMPP_REQUIRE_EQ(regular.error(),
			std::error_code(ENOTBLK, std::system_category()));
	}

	auto missing = block_device::open("/dev/libempp-block-device-that-does-not-exist");
	EMPP_REQUIRE(not missing);
	EMPP_REQUIRE_EQ(missing.error(),
		std::error_code(ENOENT, std::system_category()));

	auto devices = libempp::storage::enumerate_devices();
	EMPP_REQUIRE(devices);
	const auto empty_resolve = libempp::storage::resolve_device({});
	EMPP_REQUIRE(not empty_resolve);
	EMPP_REQUIRE_EQ(empty_resolve.error(),
		std::make_error_code(std::errc::invalid_argument));
	const auto non_block = libempp::storage::resolve_device("/dev/null");
	EMPP_REQUIRE(not non_block);
	EMPP_REQUIRE_EQ(non_block.error(),
		std::error_code(ENOTBLK, std::system_category()));
}

EMPP_TEST("real-device", "optional block device read-only integration")
{
	const char *device_path = std::getenv("LIBEMPP_TEST_BLOCK_DEVICE");
	if( not device_path or *device_path == '\0' )
		return;

	auto selected = libempp::storage::resolve_device(device_path);
	EMPP_REQUIRE(selected);
	auto stale = *selected;
	stale.id.minor ^= 1U;
	auto rejected = libempp::storage::block_device::open(stale);
	EMPP_REQUIRE(not rejected);
	EMPP_REQUIRE_EQ(rejected.error(),
		libempp::storage::make_error_code(libempp::storage::errc::device_changed));
	const auto unbound_format = libempp::storage::format(device_path);
	EMPP_REQUIRE(not unbound_format);
	EMPP_REQUIRE_EQ(unbound_format.error(), libempp::storage::make_error_code(
		libempp::storage::errc::device_identity_required));

	auto opened = libempp::storage::block_device::open(*selected);
	EMPP_REQUIRE(opened);
	auto &device = **opened;
	auto info = device.info();
	EMPP_REQUIRE(info);
	EMPP_REQUIRE(info->geometry.logical_block_size > 0);
	EMPP_REQUIRE(info->geometry.physical_block_size > 0);

	if( info->geometry.capacity_bytes > 0 )
	{
		const auto size = static_cast<size_t>(std::min (
			info->geometry.capacity_bytes,
			static_cast<libempp::storage::block_device::capacity_t>(
				info->geometry.logical_block_size
			)
		));
		std::vector<std::byte> buffer(size);
		const auto read_size = device.read_some_at (
			0, libgs::mutable_buffer(buffer.data(), buffer.size())
		);
		EMPP_REQUIRE(read_size);
		EMPP_REQUIRE(*read_size > 0);
		EMPP_REQUIRE(*read_size <= buffer.size());
	}
}
