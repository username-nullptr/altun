// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_CORE_SBUS_CACHE_H
#define LIBEMPP_CORE_SBUS_CACHE_H

#include <libempp/core/sbus/sbus.h>

namespace libempp::sbus
{

using cache_t = libgs::utils::sbus::cache<subscriber>;

[[nodiscard]] LIBEMPP_CORE_API cache_t &cache() noexcept;

#define libempp_sbus_cache  libempp::sbus::cache()

} //namespace libempp::sbus


#endif //LIBEMPP_CORE_SBUS_CACHE_H
