// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_STORAGE_SPACE_H
#define LIBEMPP_LINUX_STORAGE_SPACE_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/storage/types.h>
#include <libempp/linux/global.h>

namespace libempp::storage
{

[[nodiscard]] LIBEMPP_LINUX_API result_t<space_info> space(const path_t &path);

} // namespace libempp::storage

#endif //__linux__
#endif // LIBEMPP_LINUX_STORAGE_SPACE_H
