// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_CORE_SETTINGS_H
#define LIBEMPP_CORE_SETTINGS_H

#include <libempp/core/global.h>
#include <riwo/utils/settings.h>

#define libempp_settings(...) \
	riwo::utils::settings::instance(__VA_ARGS__)

#define libempp_default_settings \
	libempp_settings()


#endif //LIBEMPP_CORE_SETTINGS_H
