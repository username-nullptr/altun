// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_CORE_SBUS_CACHE_H
#define ALTUN_CORE_SBUS_CACHE_H

#include <altun/core/sbus/sbus.h>

namespace altun::sbus
{

using cache_t = riwo::utils::sbus::cache<subscriber>;

[[nodiscard]] ALTUN_CORE_API cache_t &cache() noexcept;

#define altun_sbus_cache  altun::sbus::cache()

} //namespace altun::sbus


#endif //ALTUN_CORE_SBUS_CACHE_H
