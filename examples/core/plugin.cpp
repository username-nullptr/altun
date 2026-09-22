// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <libgs/core/cxx/attributes.h>

extern "C" LIBGS_DECL_EXPORT int libempp_example_square(int value)
{
	return value * value;
}
