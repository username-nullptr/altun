// SPDX-FileCopyrightText: 2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#if ALTUN_CONSUME_UMBRELLA
# include <altun.h>
#endif

#if ALTUN_CONSUME_CORE
# include <altun/core.h>
#endif

#if ALTUN_CONSUME_LINUX
# include <altun/linux.h>
# include <altun/linux/storage/error.h>
#endif

#include <altun/core/global.h>
#include <string_view>

int main()
{
	const std::string_view version = altun::version_string();
	if(version.empty())
		return 1;

#if ALTUN_CONSUME_LINUX
	// Exercise a symbol implemented in altun.linux so the test validates the
	// installed library and its transitive Core dependency, not headers alone.
	const auto error = altun::storage::make_error_code(
		altun::storage::errc::command_failed
	);
	if(error.value() != 1 or
		std::string_view(error.category().name()) != "altun.storage")
	{
		return 2;
	}
#endif
	return 0;
}
