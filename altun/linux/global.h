// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_GLOBAL_H
#define ALTUN_LINUX_GLOBAL_H

#include <altun/core/cxx/configs.h>
#include <riwo/core/global.h>

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

# if ALTUN_BUILD_STATIC
#  define ALTUN_LINUX_API
# elif defined(altun_linux_EXPORTS)
#  define ALTUN_LINUX_API  RIWO_DECL_EXPORT
# else //altun_linux_EXPORTS
#  define ALTUN_LINUX_API  RIWO_DECL_IMPORT
# endif //altun_linux_EXPORTS

# define ALTUN_LINUX_VAPI
# define ALTUN_LINUX_TAPI

#endif //__linux__

#endif //ALTUN_LINUX_GLOBAL_H
