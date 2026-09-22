# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

add_libempp_stress_test(core empp.core core.cpp)
target_compile_definitions(empp.test.stress.core PRIVATE
	LIBEMPP_TEST_PLUGIN_FILE="$<TARGET_FILE:empp.test.plugin>"
)
add_dependencies(empp.test.stress.core empp.test.plugin)
set_property(TEST empp.stress.core APPEND PROPERTY LABELS core)

if (TARGET empp.linux)
	add_libempp_stress_test(linux empp.linux
		linux.cpp
		../support/linux/bus.cpp
	)
	set_property(TEST empp.stress.linux APPEND PROPERTY LABELS linux)
endif ()
