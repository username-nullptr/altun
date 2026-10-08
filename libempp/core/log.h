// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_CORE_LOG_H
#define LIBEMPP_CORE_LOG_H

#include <libempp/core/global.h>
#include <riwo/utils/logger.h>

#define libempp_clog(Level, name, ...)    riwo_utils_clog         (Level, name, __VA_ARGS__)
#define libempp_clog_trace(name, ...)     riwo_utils_clog_trace   (       name, __VA_ARGS__)
#define libempp_clog_debug(name, ...)     riwo_utils_clog_debug   (       name, __VA_ARGS__)
#define libempp_clog_info(name, ...)      riwo_utils_clog_info    (       name, __VA_ARGS__)
#define libempp_clog_warning(name, ...)   riwo_utils_clog_warning (       name, __VA_ARGS__)
#define libempp_clog_error(name, ...)     riwo_utils_clog_error   (       name, __VA_ARGS__)
#define libempp_clog_critical(name, ...)  riwo_utils_clog_critical(       name, __VA_ARGS__)

#define libempp_log(Level, ...)    riwo_utils_log         (Level, __VA_ARGS__)
#define libempp_log_trace(...)     riwo_utils_log_trace   (       __VA_ARGS__)
#define libempp_log_debug(...)     riwo_utils_log_debug   (       __VA_ARGS__)
#define libempp_log_info(...)      riwo_utils_log_info    (       __VA_ARGS__)
#define libempp_log_warning(...)   riwo_utils_log_warning (       __VA_ARGS__)
#define libempp_log_error(...)     riwo_utils_log_error   (       __VA_ARGS__)
#define libempp_log_critical(...)  riwo_utils_log_critical(       __VA_ARGS__)


#endif //LIBEMPP_CORE_LOG_H
