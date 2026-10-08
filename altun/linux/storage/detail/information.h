// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_STORAGE_DETAIL_INFORMATION_H
#define ALTUN_LINUX_STORAGE_DETAIL_INFORMATION_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/storage/types.h>

namespace altun::storage::detail
{

[[nodiscard]] ALTUN_LINUX_API
disk_info inspect_disk(const path_t &device);

[[nodiscard]] ALTUN_LINUX_API
partition_info find_partition(const disk_info &disk, std::uint32_t number);

ALTUN_LINUX_API
void require_partition_table(const disk_info &disk, std::string_view operation);

} // namespace altun::storage::detail

#endif //__linux__
#endif // ALTUN_LINUX_STORAGE_DETAIL_INFORMATION_H
