// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_GLOBAL_H
#define LIBEMPP_LINUX_GLOBAL_H

#include <libempp/core/cxx/configs.h>
#include <libgs/core/global.h>

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

# if LIBEMPP_BUILD_STATIC
#  define LIBEMPP_LINUX_API
# elif defined(empp_linux_EXPORTS)
#  define LIBEMPP_LINUX_API  LIBGS_DECL_EXPORT
# else //empp_linux_EXPORTS
#  define LIBEMPP_LINUX_API  LIBGS_DECL_IMPORT
# endif //empp_linux_EXPORTS

# define LIBEMPP_LINUX_VAPI
# define LIBEMPP_LINUX_TAPI

#endif //__linux__

#endif //LIBEMPP_LINUX_GLOBAL_H
