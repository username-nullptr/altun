// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_CORE_GLOBAL_H
#define LIBEMPP_CORE_GLOBAL_H

#include <libempp/core/cxx/configs.h>
#include <riwo/utils/global.h>

#if LIBEMPP_BUILD_STATIC
# define LIBEMPP_CORE_API
#elif defined(empp_core_EXPORTS)
# define LIBEMPP_CORE_API  RIWO_DECL_EXPORT
#else //empp_core_EXPORTS
# define LIBEMPP_CORE_API  RIWO_DECL_IMPORT
#endif //empp_core_EXPORTS

#define LIBEMPP_CORE_VAPI
#define LIBEMPP_CORE_TAPI

namespace libempp
{

[[nodiscard]] LIBEMPP_CORE_API
const char *version_string() noexcept;

} //namespace libempp


#endif //LIBEMPP_CORE_GLOBAL_H
