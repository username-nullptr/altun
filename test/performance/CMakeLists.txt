# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

set(test_target empp.test.performance.sbus)
add_executable(${test_target} sbus.cpp)
target_link_libraries(${test_target} PRIVATE empp.core)
configure_libempp_test_target(${test_target})
target_compile_definitions(${test_target} PRIVATE
	LIBEMPP_PERFORMANCE_SCALE=${LIBEMPP_PERFORMANCE_SCALE}
	LIBEMPP_PERFORMANCE_SBUS_EXECUTABLE="$<TARGET_FILE:empp.test.performance.sbus>"
)

# A private session bus makes the D-Bus measurements independent of the
# desktop session and is inherited by the benchmark's echo child process.
add_libempp_ctest(empp.performance.sbus
	${LIBEMPP_PERFORMANCE_DBUS_RUN_SESSION} --
	$<TARGET_FILE:empp.test.performance.sbus>
)
set_tests_properties(empp.performance.sbus PROPERTIES
	LABELS performance
	RUN_SERIAL TRUE
	TIMEOUT ${LIBEMPP_PERFORMANCE_TIMEOUT}
)
string(MD5 libempp_sbus_performance_namespace "${CMAKE_BINARY_DIR}")
set_property(TEST empp.performance.sbus APPEND PROPERTY ENVIRONMENT
	"LIBEMPP_SBUS_SHM_NAME=/libempp-sbus-performance-${libempp_sbus_performance_namespace}"
)
