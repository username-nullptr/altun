// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "../../test.h"

#include <altun/linux/storage.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <system_error>
#include <vector>

ALTUN_TEST("virtual-device", "block device validates nodes and enumeration")
{
	using altun::storage::block_device;
	auto empty = block_device::open(altun::storage::path_t{});
	ALTUN_REQUIRE(not empty);
	ALTUN_REQUIRE_EQ(empty.error(), std::make_error_code(std::errc::invalid_argument));

	for(std::size_t iteration = 0; iteration < 8; ++iteration)
	{
		auto regular = block_device::open("/dev/null");
		ALTUN_REQUIRE(not regular);
		ALTUN_REQUIRE_EQ(regular.error(),
			std::error_code(ENOTBLK, std::system_category()));
	}

	auto missing = block_device::open("/dev/altun-block-device-that-does-not-exist");
	ALTUN_REQUIRE(not missing);
	ALTUN_REQUIRE_EQ(missing.error(),
		std::error_code(ENOENT, std::system_category()));

	auto devices = altun::storage::enumerate_devices();
	ALTUN_REQUIRE(devices);
	const auto empty_resolve = altun::storage::resolve_device({});
	ALTUN_REQUIRE(not empty_resolve);
	ALTUN_REQUIRE_EQ(empty_resolve.error(),
		std::make_error_code(std::errc::invalid_argument));
	const auto non_block = altun::storage::resolve_device("/dev/null");
	ALTUN_REQUIRE(not non_block);
	ALTUN_REQUIRE_EQ(non_block.error(),
		std::error_code(ENOTBLK, std::system_category()));
}

ALTUN_TEST("real-device", "optional block device read-only integration")
{
	const char *device_path = std::getenv("ALTUN_TEST_BLOCK_DEVICE");
	if( not device_path or *device_path == '\0' )
		return;

	auto selected = altun::storage::resolve_device(device_path);
	ALTUN_REQUIRE(selected);
	auto stale = *selected;
	stale.id.minor ^= 1U;
	auto rejected = altun::storage::block_device::open(stale);
	ALTUN_REQUIRE(not rejected);
	ALTUN_REQUIRE_EQ(rejected.error(),
		altun::storage::make_error_code(altun::storage::errc::device_changed));
	const auto unbound_format = altun::storage::format(device_path);
	ALTUN_REQUIRE(not unbound_format);
	ALTUN_REQUIRE_EQ(unbound_format.error(), altun::storage::make_error_code(
		altun::storage::errc::device_identity_required));

	auto opened = altun::storage::block_device::open(*selected);
	ALTUN_REQUIRE(opened);
	auto &device = **opened;
	auto info = device.info();
	ALTUN_REQUIRE(info);
	ALTUN_REQUIRE(info->geometry.logical_block_size > 0);
	ALTUN_REQUIRE(info->geometry.physical_block_size > 0);

	if( info->geometry.capacity_bytes > 0 )
	{
		const auto size = static_cast<size_t>(std::min (
			info->geometry.capacity_bytes,
			static_cast<altun::storage::block_device::capacity_t>(
				info->geometry.logical_block_size
			)
		));
		std::vector<std::byte> buffer(size);
		const auto read_size = device.read_some_at (
			0, riwo::mutable_buffer(buffer.data(), buffer.size())
		);
		ALTUN_REQUIRE(read_size);
		ALTUN_REQUIRE(*read_size > 0);
		ALTUN_REQUIRE(*read_size <= buffer.size());
	}
}
