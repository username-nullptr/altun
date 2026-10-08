// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "gpio.h"
#ifdef __linux__

namespace altun::subsys::detail
{

bool valid_direction(gpio_direction_t direction) noexcept
{
	return direction == gpio_direction_t::input or direction == gpio_direction_t::output;
}

bool valid_edge(gpio_edge_t edge) noexcept
{
	return edge == gpio_edge_t::none or edge == gpio_edge_t::rising or
		   edge == gpio_edge_t::falling or edge == gpio_edge_t::both;
}

} // namespace altun::subsys::detail

#endif //__linux__
