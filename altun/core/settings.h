// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_CORE_SETTINGS_H
#define ALTUN_CORE_SETTINGS_H

#include <altun/core/global.h>
#include <riwo/utils/settings.h>

#define altun_settings(...) \
	riwo::utils::settings::instance(__VA_ARGS__)

#define altun_default_settings \
	altun_settings()


#endif //ALTUN_CORE_SETTINGS_H
