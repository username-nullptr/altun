// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_STORAGE_ERROR_H
#define LIBEMPP_LINUX_STORAGE_ERROR_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/global.h>

namespace libempp::storage
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

[[nodiscard]] LIBEMPP_LINUX_API
const std::error_category &error_category() noexcept;

[[nodiscard]] LIBEMPP_LINUX_API
std::error_code make_error_code(errc error) noexcept;

} // namespace libempp::storage

namespace std
{

template <>
struct is_error_code_enum<libempp::storage::errc> : true_type {};

} // namespace std

#endif //__linux__
#endif // LIBEMPP_LINUX_STORAGE_ERROR_H
