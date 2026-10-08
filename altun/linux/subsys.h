// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_SUBSYS_H
#define ALTUN_LINUX_SUBSYS_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/subsys/gpio_manager.h>
#include <altun/linux/subsys/backlight.h>
#include <altun/linux/subsys/pwm.h>
#include <altun/linux/subsys/led.h>

#endif //__linux__
#endif //ALTUN_LINUX_SUBSYS_H
