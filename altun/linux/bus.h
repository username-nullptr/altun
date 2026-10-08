// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_BUS_H
#define ALTUN_LINUX_BUS_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/bus/spi.h>
#include <altun/linux/bus/i2c.h>

#endif //__linux__
#endif //ALTUN_LINUX_BUS_H
