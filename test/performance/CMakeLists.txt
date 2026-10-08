# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

set(test_target altun.test.performance.sbus)
add_executable(${test_target} sbus.cpp)
target_link_libraries(${test_target} PRIVATE altun.core)
configure_altun_test_target(${test_target})
target_compile_definitions(${test_target} PRIVATE
	ALTUN_PERFORMANCE_SCALE=${ALTUN_PERFORMANCE_SCALE}
	ALTUN_PERFORMANCE_SBUS_EXECUTABLE="$<TARGET_FILE:altun.test.performance.sbus>"
)

# A private session bus makes the D-Bus measurements independent of the
# desktop session and is inherited by the benchmark's echo child process.
add_altun_ctest(altun.performance.sbus
	${ALTUN_PERFORMANCE_DBUS_RUN_SESSION} --
	$<TARGET_FILE:altun.test.performance.sbus>
)
set_tests_properties(altun.performance.sbus PROPERTIES
	LABELS performance
	RUN_SERIAL TRUE
	TIMEOUT ${ALTUN_PERFORMANCE_TIMEOUT}
)
string(MD5 altun_sbus_performance_namespace "${CMAKE_BINARY_DIR}")
set_property(TEST altun.performance.sbus APPEND PROPERTY ENVIRONMENT
	"ALTUN_SBUS_SHM_NAME=/altun-sbus-performance-${altun_sbus_performance_namespace}"
)
