// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_STORAGE_INFORMATION_H
#define LIBEMPP_LINUX_STORAGE_INFORMATION_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/storage/types.h>
#include <libempp/linux/global.h>

namespace libempp::storage
{

[[nodiscard]] LIBEMPP_LINUX_API
result_t<libgs::optional<filesystem_info>> inspect_filesystem(const path_t &device);

[[nodiscard]] LIBEMPP_LINUX_API
result_t<libgs::optional<filesystem_info>> inspect_filesystem(const device_info &device);

[[nodiscard]] LIBEMPP_LINUX_API
result_t<disk_info> inspect_disk(const path_t &device);

[[nodiscard]] LIBEMPP_LINUX_API
result_t<disk_info> inspect_disk(const device_info &device);

[[nodiscard]] LIBEMPP_LINUX_API
result_t<partition_info> inspect_partition(const path_t &device, std::uint32_t number);

[[nodiscard]] LIBEMPP_LINUX_API
result_t<partition_info> inspect_partition(const device_info &device, std::uint32_t number);

} // namespace libempp::storage

#endif //__linux__
#endif // LIBEMPP_LINUX_STORAGE_INFORMATION_H
