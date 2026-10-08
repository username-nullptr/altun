// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_STORAGE_SPACE_H
#define ALTUN_LINUX_STORAGE_SPACE_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/storage/types.h>
#include <altun/linux/global.h>

namespace altun::storage
{

[[nodiscard]] ALTUN_LINUX_API result_t<space_info> space(const path_t &path);

} // namespace altun::storage

#endif //__linux__
#endif // ALTUN_LINUX_STORAGE_SPACE_H
