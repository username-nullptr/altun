// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_STORAGE_ERROR_H
#define ALTUN_LINUX_STORAGE_ERROR_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/global.h>

namespace altun::storage
{

enum class errc
{
	command_failed = 1,
	command_terminated,
	no_partition_table,
	partition_not_found,
	ambiguous_signature,
	malformed_mount_table,
	command_timed_out,
	not_mounted,
	verification_failed,
	device_identity_required,
	device_changed
};

[[nodiscard]] ALTUN_LINUX_API
const std::error_category &error_category() noexcept;

[[nodiscard]] ALTUN_LINUX_API
std::error_code make_error_code(errc error) noexcept;

} // namespace altun::storage

namespace std
{

template <>
struct is_error_code_enum<altun::storage::errc> : true_type {};

} // namespace std

#endif //__linux__
#endif // ALTUN_LINUX_STORAGE_ERROR_H
