// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <altun/core/log.h>

int main()
{
	altun_log_info("altun {} is ready", altun::version_string());
	altun_log_warning("Temperature: {:.1f} C", 42.5);
	altun_clog_info("sensor", "Custom logger: {}", "online");
	return 0;
}
