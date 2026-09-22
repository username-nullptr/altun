// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_GLOBAL_H
#define LIBEMPP_LINUX_GLOBAL_H

#include <libgs/core/global.h>

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

# ifdef libempp_EXPORTS
#  define LIBEMPP_LINUX_API  LIBGS_DECL_EXPORT
# else //libempp_EXPORTS
#  define LIBEMPP_LINUX_API  LIBGS_DECL_IMPORT
# endif //libempp_EXPORTS

# define LIBEMPP_LINUX_VAPI
# define LIBEMPP_LINUX_TAPI

#endif //__linux__

#endif //LIBEMPP_LINUX_GLOBAL_H
