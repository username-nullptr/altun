// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_CORE_GLOBAL_H
#define LIBEMPP_CORE_GLOBAL_H

#include <libempp/core/cxx/configs.h>
#include <libgs/utils/global.h>

#ifdef libempp_EXPORTS
# define LIBEMPP_CORE_API  LIBGS_DECL_EXPORT
#else //libempp_EXPORTS
# define LIBEMPP_CORE_API  LIBGS_DECL_IMPORT
#endif //libempp_EXPORTS

#define LIBEMPP_CORE_VAPI
#define LIBEMPP_CORE_TAPI

namespace libempp
{

LIBEMPP_CORE_API [[nodiscard]] const char *version_string() noexcept;

} //namespace libempp


#endif //LIBEMPP_CORE_GLOBAL_H
