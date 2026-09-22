// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <libempp/core/log.h>

int main()
{
	libempp_log_info("libEMpp {} is ready", libempp::version_string());
	libempp_log_warning("Temperature: {:.1f} C", 42.5);
	libempp_clog_info("sensor", "Custom logger: {}", "online");
	return 0;
}
