# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

add_subdirectory(core)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_subdirectory(linux)
endif ()
