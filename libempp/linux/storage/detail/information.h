// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_STORAGE_DETAIL_INFORMATION_H
#define LIBEMPP_LINUX_STORAGE_DETAIL_INFORMATION_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/storage/types.h>

namespace libempp::storage::detail
{

[[nodiscard]] LIBEMPP_LINUX_API
disk_info inspect_disk(const path_t &device);

[[nodiscard]] LIBEMPP_LINUX_API
partition_info find_partition(const disk_info &disk, std::uint32_t number);

LIBEMPP_LINUX_API
void require_partition_table(const disk_info &disk, std::string_view operation);

} // namespace libempp::storage::detail

#endif //__linux__
#endif // LIBEMPP_LINUX_STORAGE_DETAIL_INFORMATION_H
