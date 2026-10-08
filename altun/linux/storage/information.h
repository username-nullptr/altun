// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_STORAGE_INFORMATION_H
#define ALTUN_LINUX_STORAGE_INFORMATION_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/storage/types.h>
#include <altun/linux/global.h>

namespace altun::storage
{

[[nodiscard]] ALTUN_LINUX_API
result_t<riwo::optional<filesystem_info>> inspect_filesystem(const path_t &device);

[[nodiscard]] ALTUN_LINUX_API
result_t<riwo::optional<filesystem_info>> inspect_filesystem(const device_info &device);

[[nodiscard]] ALTUN_LINUX_API
result_t<disk_info> inspect_disk(const path_t &device);

[[nodiscard]] ALTUN_LINUX_API
result_t<disk_info> inspect_disk(const device_info &device);

[[nodiscard]] ALTUN_LINUX_API
result_t<partition_info> inspect_partition(const path_t &device, std::uint32_t number);

[[nodiscard]] ALTUN_LINUX_API
result_t<partition_info> inspect_partition(const device_info &device, std::uint32_t number);

} // namespace altun::storage

#endif //__linux__
#endif // ALTUN_LINUX_STORAGE_INFORMATION_H
