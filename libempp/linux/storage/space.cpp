// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "space.h"
#ifdef __linux__

#include <sys/statvfs.h>

namespace libempp::storage { namespace
{

[[nodiscard]] riwo::optional<uint64_t> multiply(uint64_t left, uint64_t right) noexcept
{
	if( left != 0 and right > std::numeric_limits<uint64_t>::max() / left )
		return riwo::nullopt;
	return left * right;
}

} // namespace

result_t<space_info> space(const path_t &path)
{
	if( path.empty() )
		return riwo::sys_unexpected(std::make_error_code(std::errc::invalid_argument));

	struct statvfs status {};
	if( ::statvfs(path.c_str(), &status) < 0 )
		return riwo::sys_unexpected(std::error_code(errno, std::system_category()));

	const uint64_t block_size = status.f_frsize != 0 ? status.f_frsize : status.f_bsize;
	const auto total = multiply(status.f_blocks, block_size);

	const auto free = multiply(status.f_bfree, block_size);
	const auto available = multiply(status.f_bavail, block_size);

	if( not total or not free or not available or *free > *total or *available > *free )
		return riwo::sys_unexpected(std::make_error_code(std::errc::value_too_large));

	return space_info {
		.total = *total,
		.used = *total - *free,
		.free = *free,
		.available = *available,
		.files = status.f_files,
		.files_free = status.f_ffree
	};
}

} // namespace libempp::storage

#endif //__linux__
