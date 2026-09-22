// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_STORAGE_PARTITION_H
#define LIBEMPP_LINUX_STORAGE_PARTITION_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/storage/types.h>
#include <libempp/linux/global.h>

namespace libempp::storage
{

enum class partition_table_type {
	dos, gpt
};

struct partition_spec
{
	// Zero selects sfdisk's aligned default start or all remaining space.
	sector_t start_sector = 0;
	sector_t size_sectors = 0;

	// util-linux aliases (such as linux, swap, uefi) and GUID/MBR type codes
	// are accepted. Empty selects the util-linux default Linux data type.
	std::string type = "linux";

	// GPT partition name. DOS partition tables do not support names.
	std::string name;
};

struct partition_options : operation_options {
	std::chrono::milliseconds poll_interval {100};
};

// Path overloads accept regular image files only. Physical block devices must
// use device_info so their identity is checked before and after each operation.
// Replaces the complete partition layout and verifies the kernel-visible result.
[[nodiscard]] LIBEMPP_LINUX_API result_t<disk_info> replace_partition_table (
	const path_t &device, partition_table_type type,
	const std::vector<partition_spec> &partitions = {},
	const partition_options &options = {}
);

[[nodiscard]] LIBEMPP_LINUX_API result_t<disk_info> replace_partition_table (
	const device_info &device, partition_table_type type,
	const std::vector<partition_spec> &partitions = {},
	const partition_options &options = {}
);

[[nodiscard]] LIBEMPP_LINUX_API result_t<partition_info> create_partition (
	const path_t &device, const partition_spec &spec = {}, const partition_options &options = {}
);

[[nodiscard]] LIBEMPP_LINUX_API result_t<partition_info> create_partition (
	const device_info &device, const partition_spec &spec = {}, const partition_options &options = {}
);

// Deletes only the partition-table entry; filesystem contents are not wiped.
[[nodiscard]] LIBEMPP_LINUX_API result_t<> delete_partition (
	const path_t &device, std::uint32_t number, const partition_options &options = {}
);

[[nodiscard]] LIBEMPP_LINUX_API result_t<> delete_partition (
	const device_info &device, std::uint32_t number, const partition_options &options = {}
);

// A size of zero grows to the next partition or end of device. Shrinking is
// intentionally rejected. The filesystem inside the partition is not resized.
[[nodiscard]] LIBEMPP_LINUX_API result_t<partition_info> expand_partition (
	const path_t &device, std::uint32_t number, sector_t new_size_sectors = 0,
	const partition_options &options = {}
);

[[nodiscard]] LIBEMPP_LINUX_API result_t<partition_info> expand_partition (
	const device_info &device, std::uint32_t number, sector_t new_size_sectors = 0,
	const partition_options &options = {}
);

} // namespace libempp::storage

#endif //__linux__
#endif // LIBEMPP_LINUX_STORAGE_PARTITION_H
