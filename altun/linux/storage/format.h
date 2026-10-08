// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_STORAGE_FORMAT_H
#define ALTUN_LINUX_STORAGE_FORMAT_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/storage/types.h>
#include <altun/linux/global.h>

namespace altun::storage
{

enum class filesystem_type {
	exfat, ext4, ntfs
};

enum class format_mode {
	quick, full
};

struct format_options : operation_options
{
	filesystem_type type = filesystem_type::ext4;
	std::string label {};
	format_mode mode = format_mode::quick;
};

// The path overload accepts regular image files only. Physical block devices
// must use the device_info overload so their identity can be verified.
[[nodiscard]] ALTUN_LINUX_API result_t<filesystem_info> format (
	const path_t &device, const format_options &options = {}
);
[[nodiscard]] ALTUN_LINUX_API result_t<filesystem_info> format (
	const device_info &device, const format_options &options = {}
);

[[nodiscard]] ALTUN_LINUX_API
const char *string(filesystem_type type) noexcept;

} // namespace altun::storage

#endif //__linux__
#endif // ALTUN_LINUX_STORAGE_FORMAT_H
