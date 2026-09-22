// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "gpio_manager.h"

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

namespace libempp::subsys
{

std::strong_ordering gpio_index::compare_chip
(const std::filesystem::path &left, const std::filesystem::path &right) noexcept
{
	const std::string_view left_view(left.native());
	const std::string_view right_view(right.native());
	constexpr std::string_view device_prefix = "/dev/";

	const bool left_bare = left_view.find('/') == std::string_view::npos;
	const bool right_bare = right_view.find('/') == std::string_view::npos;

	const auto left_size = left_view.size() + (left_bare ? device_prefix.size() : 0);
	const auto right_size = right_view.size() + (right_bare ? device_prefix.size() : 0);
	const auto common_size = std::min(left_size, right_size);

	for(std::size_t index = 0; index < common_size; ++index)
	{
		const char left_char = left_bare and index < device_prefix.size() ?
			device_prefix[index] : left_view[index - (left_bare ? device_prefix.size() : 0)];

		const char right_char = right_bare and index < device_prefix.size() ?
			device_prefix[index] : right_view[index - (right_bare ? device_prefix.size() : 0)];

		if( left_char < right_char )
			return std::strong_ordering::less;

		if( left_char > right_char )
			return std::strong_ordering::greater;
	}
	return left_size <=> right_size;
}

[[nodiscard]] bool gpio_index::operator==(const gpio_index &other) const noexcept
{
	return line == other.line and
		compare_chip(chip, other.chip) == std::strong_ordering::equal;
}

[[nodiscard]] std::strong_ordering gpio_index::operator<=>(const gpio_index &other) const noexcept
{
	if( const auto chip_order = compare_chip(chip, other.chip);
		chip_order != std::strong_ordering::equal )
		return chip_order;
	return line <=> other.line;
}

} //namespace libsep::subsys

#endif //__linux__
