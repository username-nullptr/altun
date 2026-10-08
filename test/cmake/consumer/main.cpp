// SPDX-FileCopyrightText: 2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#if LIBEMPP_CONSUME_UMBRELLA
# include <libempp.h>
#endif

#if LIBEMPP_CONSUME_CORE
# include <libempp/core.h>
#endif

#if LIBEMPP_CONSUME_LINUX
# include <libempp/linux.h>
# include <libempp/linux/storage/error.h>
#endif

#include <libempp/core/global.h>

#include <string_view>

int main()
{
	const std::string_view version = libempp::version_string();
	if(version.empty())
		return 1;

#if LIBEMPP_CONSUME_LINUX
	// Exercise a symbol implemented in empp.linux so the test validates the
	// installed library and its transitive Core dependency, not headers alone.
	const auto error = libempp::storage::make_error_code(
		libempp::storage::errc::command_failed
	);
	if(error.value() != 1 or
		std::string_view(error.category().name()) != "libempp.storage")
	{
		return 2;
	}
#endif

	return 0;
}
