// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_H
#define ALTUN_LINUX_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

# include <altun/linux/storage.h>
# include <altun/linux/subsys.h>
# include <altun/linux/udev.h>
# include <altun/linux/bus.h>

#endif //__linux__
#endif //ALTUN_LINUX_H
