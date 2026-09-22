# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

set(LIBEMPP_TEST_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LIBEMPP_TEST_LAUNCHER)

# Some Linux kernels prevent GCC TSan from reserving its shadow-memory range
# under ASLR. Use a per-process launcher when the host permits it.
if (LIBEMPP_ENABLE_TEST_TSAN AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
	find_program(LIBEMPP_TEST_SETARCH_EXECUTABLE NAMES setarch)
	if (LIBEMPP_TEST_SETARCH_EXECUTABLE)
		execute_process(
			COMMAND ${LIBEMPP_TEST_SETARCH_EXECUTABLE}
				${CMAKE_SYSTEM_PROCESSOR} -R ${CMAKE_COMMAND} -E true
			RESULT_VARIABLE libempp_test_setarch_result
			OUTPUT_QUIET ERROR_QUIET
		)
		if (libempp_test_setarch_result EQUAL 0)
			set(LIBEMPP_TEST_LAUNCHER
				${LIBEMPP_TEST_SETARCH_EXECUTABLE} ${CMAKE_SYSTEM_PROCESSOR} -R)
			message(STATUS
				"${PRO_NAME}: TSan tests disable ASLR per process with setarch."
			)
		endif ()
	endif ()
	mark_as_advanced(LIBEMPP_TEST_SETARCH_EXECUTABLE)
endif ()

function(configure_libempp_test_target test_target)
	libempp_limit_heavy_compile(${test_target})
	target_include_directories(${test_target} PRIVATE ${LIBEMPP_TEST_SOURCE_DIR})
	set_target_properties(${test_target} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${LIBEMPP_OUTPUT_DIR}/bin
	)

	if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
		target_compile_options(${test_target} PRIVATE
			-Wall -Wextra -Wpedantic -Werror
		)
		target_compile_definitions(${test_target} PRIVATE _GLIBCXX_ASSERTIONS)
	endif ()
	if (UNIX)
		target_link_options(${test_target} PRIVATE -Wl,-rpath,$ORIGIN)
	endif ()
	if (LIBEMPP_ENABLE_TEST_SANITIZERS OR LIBEMPP_ENABLE_TEST_TSAN)
		target_link_libraries(${test_target} PRIVATE libempp.test.sanitizer)
	endif ()
endfunction()

function(add_libempp_ctest test_name)
	add_test(NAME ${test_name} COMMAND ${LIBEMPP_TEST_LAUNCHER} ${ARGN})
endfunction()

function(configure_libempp_sanitized_test test_name)
	if (LIBEMPP_ENABLE_TEST_SANITIZERS OR LIBEMPP_ENABLE_TEST_TSAN)
		set_property(TEST ${test_name} APPEND PROPERTY LABELS sanitizer)
		set_property(TEST ${test_name} PROPERTY RUN_SERIAL TRUE)
		set_property(TEST ${test_name} APPEND PROPERTY FAIL_REGULAR_EXPRESSION
			"AddressSanitizer" "LeakSanitizer" "ThreadSanitizer" "runtime error:"
		)
		if (LIBEMPP_ENABLE_TEST_SANITIZERS)
			set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT
				"ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1"
				"UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1"
			)
		else ()
			set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT
				"TSAN_OPTIONS=halt_on_error=1"
			)
		endif ()
	endif ()
endfunction()

function(add_libempp_test_program test_target library)
	add_executable(${test_target} ${LIBEMPP_TEST_SOURCE_DIR}/main.cpp ${ARGN})
	target_link_libraries(${test_target} PRIVATE ${library})
	configure_libempp_test_target(${test_target})
endfunction()

function(add_libempp_functional_test name library)
	set(test_target empp.test.${name})
	add_libempp_test_program(${test_target} ${library} ${ARGN})
	add_libempp_ctest(empp.${name} $<TARGET_FILE:${test_target}>)
	set_tests_properties(empp.${name} PROPERTIES
		LABELS functional
		TIMEOUT ${LIBEMPP_FUNCTIONAL_TIMEOUT}
	)
	set_property(TEST empp.${name} APPEND PROPERTY ENVIRONMENT
		"LIBEMPP_TEST_REPEAT=${LIBEMPP_FUNCTIONAL_REPEAT}"
		"LIBEMPP_TEST_SEED=${LIBEMPP_FUNCTIONAL_SEED}"
		"LIBEMPP_TEST_FAIL_FAST=1"
	)
	configure_libempp_sanitized_test(empp.${name})
endfunction()

function(add_libempp_stress_test name library)
	set(test_target empp.test.stress.${name})
	add_libempp_test_program(${test_target} ${library} ${ARGN})
	target_compile_definitions(${test_target} PRIVATE
		LIBEMPP_STRESS_SCALE=${LIBEMPP_STRESS_SCALE}
	)
	add_libempp_ctest(empp.stress.${name} $<TARGET_FILE:${test_target}>
		--repeat ${LIBEMPP_STRESS_REPEAT}
		--seed ${LIBEMPP_STRESS_SEED}
		--fail-fast
	)
	set_tests_properties(empp.stress.${name} PROPERTIES
		LABELS stress
		RUN_SERIAL TRUE
		TIMEOUT ${LIBEMPP_STRESS_TIMEOUT}
	)
	configure_libempp_sanitized_test(empp.stress.${name})
endfunction()

# Instrument the library code under test, not just the test executables.
if (LIBEMPP_ENABLE_TEST_SANITIZERS OR LIBEMPP_ENABLE_TEST_TSAN)
	foreach(target empp.core empp.linux)
		if (TARGET ${target})
			target_link_libraries(${target} PRIVATE libempp.test.sanitizer)
		endif ()
	endforeach()
endif ()

add_library(empp.test.plugin SHARED functional/core/library_fixture.cpp)
set_target_properties(empp.test.plugin PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY ${LIBEMPP_OUTPUT_DIR}/bin
	RUNTIME_OUTPUT_DIRECTORY ${LIBEMPP_OUTPUT_DIR}/bin
)
configure_libempp_test_target(empp.test.plugin)

add_executable(empp.test.app functional/core/process_app.cpp)
configure_libempp_test_target(empp.test.app)

add_subdirectory(functional)
if (LIBEMPP_BUILD_STRESS_TESTS)
	add_subdirectory(stress)
else ()
	message(STATUS
		"${PRO_NAME}: Enable LIBEMPP_BUILD_STRESS_TESTS for stress tests."
	)
endif ()
if (LIBEMPP_BUILD_PERFORMANCE_TESTS)
	if (LIBEMPP_ENABLE_TEST_SANITIZERS OR LIBEMPP_ENABLE_TEST_TSAN)
		message(STATUS
			"${PRO_NAME}: Performance tests disabled in sanitizer builds."
		)
	else ()
		add_subdirectory(performance)
	endif ()
else ()
	message(STATUS
		"${PRO_NAME}: Enable LIBEMPP_BUILD_PERFORMANCE_TESTS for performance benchmarks."
	)
endif ()
