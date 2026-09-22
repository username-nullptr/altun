// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_STORAGE_FORMAT_H
#define LIBEMPP_LINUX_STORAGE_FORMAT_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/storage/types.h>
#include <libempp/linux/global.h>

namespace libempp::storage
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
[[nodiscard]] LIBEMPP_LINUX_API result_t<filesystem_info> format (
	const path_t &device, const format_options &options = {}
);
[[nodiscard]] LIBEMPP_LINUX_API result_t<filesystem_info> format (
	const device_info &device, const format_options &options = {}
);

[[nodiscard]] LIBEMPP_LINUX_API
const char *string(filesystem_type type) noexcept;

} // namespace libempp::storage

#endif //__linux__
#endif // LIBEMPP_LINUX_STORAGE_FORMAT_H
