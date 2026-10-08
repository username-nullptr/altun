# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

add_altun_stress_test(core altun.core core.cpp)
target_compile_definitions(altun.test.stress.core PRIVATE
	ALTUN_TEST_PLUGIN_FILE="$<TARGET_FILE:altun.test.plugin>"
)
add_dependencies(altun.test.stress.core altun.test.plugin)
set_property(TEST altun.stress.core APPEND PROPERTY LABELS core)

if (TARGET altun.linux)
	add_altun_stress_test(linux altun.linux
		linux.cpp
		../support/linux/bus.cpp
	)
	set_property(TEST altun.stress.linux APPEND PROPERTY LABELS linux)
endif ()
