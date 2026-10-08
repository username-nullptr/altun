// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_STORAGE_DETAIL_COMMON_H
#define ALTUN_LINUX_STORAGE_DETAIL_COMMON_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/storage/types.h>
#include <altun/linux/storage/error.h>

namespace altun::storage::detail
{

[[noreturn]] ALTUN_LINUX_API void throw_error (
	const std::error_code &error, std::string_view operation
);
[[noreturn]] ALTUN_LINUX_API void throw_errno(std::string_view operation);

ALTUN_LINUX_API void ensure_path(const path_t &path, std::string_view operation);
ALTUN_LINUX_API void ensure_string(const std::string &value, std::string_view operation);
ALTUN_LINUX_API void ensure_device(const device_info &device, std::string_view operation);

ALTUN_LINUX_API void run_command (
	std::vector<std::string> arguments, std::string_view standard_input = {},
	std::chrono::milliseconds timeout = std::chrono::milliseconds{10000}
);

template <typename Result = void, typename Function>
ALTUN_LINUX_TAPI result_t<Result> capture_expected(Function &&function)
{
	result_t<Result> res;
	try {
		if constexpr( std::is_void_v<Result> )
			std::forward<Function>(function)();
		else
			res = std::forward<Function>(function)();
	}
	catch(const std::system_error &exception) {
		res.despair(exception.code());
	}
	return res;
}

} // namespace altun::storage::detail

#endif //__linux__
#endif // ALTUN_LINUX_STORAGE_DETAIL_COMMON_H
