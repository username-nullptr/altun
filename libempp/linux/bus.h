// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_BUS_H
#define LIBEMPP_LINUX_BUS_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/bus/spi.h>
#include <libempp/linux/bus/i2c.h>

#endif //__linux__
#endif //LIBEMPP_LINUX_BUS_H
