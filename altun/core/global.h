// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_CORE_GLOBAL_H
#define ALTUN_CORE_GLOBAL_H

#include <altun/core/cxx/configs.h>
#include <riwo/utils/global.h>

#if ALTUN_BUILD_STATIC
# define ALTUN_CORE_API
#elif defined(altun_core_EXPORTS)
# define ALTUN_CORE_API  RIWO_DECL_EXPORT
#else //altun_core_EXPORTS
# define ALTUN_CORE_API  RIWO_DECL_IMPORT
#endif //altun_core_EXPORTS

#define ALTUN_CORE_VAPI
#define ALTUN_CORE_TAPI

namespace altun
{

[[nodiscard]] ALTUN_CORE_API
const char *version_string() noexcept;

} //namespace altun


#endif //ALTUN_CORE_GLOBAL_H
