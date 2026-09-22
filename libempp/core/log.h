// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_CORE_LOG_H
#define LIBEMPP_CORE_LOG_H

#include <libempp/core/global.h>
#include <libgs/utils/logger.h>

#define libempp_clog(Level, name, ...)    libgs_utils_clog         (Level, name, __VA_ARGS__)
#define libempp_clog_trace(name, ...)     libgs_utils_clog_trace   (       name, __VA_ARGS__)
#define libempp_clog_debug(name, ...)     libgs_utils_clog_debug   (       name, __VA_ARGS__)
#define libempp_clog_info(name, ...)      libgs_utils_clog_info    (       name, __VA_ARGS__)
#define libempp_clog_warning(name, ...)   libgs_utils_clog_warning (       name, __VA_ARGS__)
#define libempp_clog_error(name, ...)     libgs_utils_clog_error   (       name, __VA_ARGS__)
#define libempp_clog_critical(name, ...)  libgs_utils_clog_critical(       name, __VA_ARGS__)

#define libempp_log(Level, ...)    libgs_utils_log         (Level, __VA_ARGS__)
#define libempp_log_trace(...)     libgs_utils_log_trace   (       __VA_ARGS__)
#define libempp_log_debug(...)     libgs_utils_log_debug   (       __VA_ARGS__)
#define libempp_log_info(...)      libgs_utils_log_info    (       __VA_ARGS__)
#define libempp_log_warning(...)   libgs_utils_log_warning (       __VA_ARGS__)
#define libempp_log_error(...)     libgs_utils_log_error   (       __VA_ARGS__)
#define libempp_log_critical(...)  libgs_utils_log_critical(       __VA_ARGS__)


#endif //LIBEMPP_CORE_LOG_H
