// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_STORAGE_TYPES_H
#define LIBEMPP_LINUX_STORAGE_TYPES_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/global.h>

namespace libempp::storage
{

using path_t = std::filesystem::path;
using sector_t = uint64_t;

template <typename Value = void>
using result_t = riwo::sys_expected<Value>;

struct device_id
{
	uint32_t major = 0;
	uint32_t minor = 0;

	friend bool operator==
	(const device_id&, const device_id&) = default;
};

struct block_geometry
{
	uint64_t capacity_bytes = 0;
	uint32_t logical_block_size = 0;
	uint32_t physical_block_size = 0;
	bool read_only = false;
};

struct block_info
{
	path_t device {};
	device_id id {};
	block_geometry geometry {};
};

struct device_info
{
	path_t device {};
	path_t sys_path {};
	riwo::optional<path_t> parent_device {};

	device_id id {};
	riwo::optional<block_geometry> geometry {};

	std::string bus {};
	std::string model {};
	std::string serial {};

	bool partition = false;
	bool removable = false;
	bool read_only = false;
};

struct filesystem_info
{
	path_t device {};

	std::string type {};
	std::string secondary_type {};
	std::string label {};

	std::string uuid {};
	std::string usage {};
	std::string version {};

	uint64_t size_bytes = 0;
	uint64_t block_size = 0;

};

struct partition_table_info
{
	std::string type {};
	std::string id {};
};

struct LIBEMPP_LINUX_API partition_info
{
	riwo::optional<path_t> device {};
	uint32_t number = 0;

	sector_t start_sector = 0;
	sector_t size_sectors = 0;

	std::string type {};
	std::string uuid {};
	std::string name {};

	uint64_t flags = 0;

	bool primary = false;
	bool logical = false;
	bool extended = false;

	riwo::optional<filesystem_info> filesystem {};

	[[nodiscard]] riwo::optional<sector_t> end_sector() const noexcept;
	[[nodiscard]] riwo::optional<uint64_t> size_bytes(uint32_t sector_size) const noexcept;
};

struct disk_info
{
	path_t device {};
	riwo::optional<partition_table_info> table {};

	uint64_t size_bytes = 0;
	uint32_t sector_size = 0;
	std::vector<partition_info> partitions {};

};

struct space_info
{
	uint64_t total = 0;
	uint64_t used = 0;
	uint64_t free = 0;
	uint64_t available = 0;

	uint64_t files = 0;
	uint64_t files_free = 0;
};

struct operation_options {
	std::chrono::milliseconds timeout {120000};
};

} // namespace libempp::storage

#endif //__linux__
#endif // LIBEMPP_LINUX_STORAGE_TYPES_H
