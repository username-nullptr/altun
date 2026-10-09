# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

set(ALTUN_TEST_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ALTUN_TEST_LAUNCHER)

# CMake's Ninja generator relies on INCLUDE/LIB from the MSVC developer
# environment, but CTest is often launched by an IDE or shell which does not
# reproduce that environment. Preserve the values seen by the successful
# outer configure so nested CMake tests use the same compiler toolchain.
if (MSVC)
	foreach(variable INCLUDE LIB LIBPATH)
		if (NOT "$ENV{${variable}}" STREQUAL "")
			set(ALTUN_MSVC_TEST_${variable} "$ENV{${variable}}"
				CACHE INTERNAL "MSVC environment for nested CMake tests" FORCE
			)
		endif ()
	endforeach()
endif ()


function(configure_altun_ctest_environment test_name)
	foreach(variable INCLUDE LIB LIBPATH)
		if (ALTUN_MSVC_TEST_${variable})
			string(REPLACE ";" "\\;" environment_value
				"${ALTUN_MSVC_TEST_${variable}}"
			)
			set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT
				"${variable}=${environment_value}"
			)
		endif ()
	endforeach()

	if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
		IS_ABSOLUTE "${altun_gnu_runtime_dir}" AND
		EXISTS "${altun_gnu_runtime_dir}")

		set(environment_value "${altun_gnu_runtime_dir};$ENV{PATH}")
		string(REPLACE ";" "\\;" environment_value "${environment_value}")

		set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT
			"PATH=${environment_value}"
		)
	endif ()
endfunction()


# Some Linux kernels prevent GCC TSan from reserving its shadow-memory range
# under ASLR. Use a per-process launcher when the host permits it.
if (ALTUN_ENABLE_TEST_TSAN AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
	find_program(ALTUN_TEST_SETARCH_EXECUTABLE NAMES setarch)
	if (ALTUN_TEST_SETARCH_EXECUTABLE)
		execute_process(
			COMMAND ${ALTUN_TEST_SETARCH_EXECUTABLE}
				${CMAKE_SYSTEM_PROCESSOR} -R ${CMAKE_COMMAND} -E true
			RESULT_VARIABLE altun_test_setarch_result
			OUTPUT_QUIET ERROR_QUIET
		)
		if (altun_test_setarch_result EQUAL 0)
			set(ALTUN_TEST_LAUNCHER
				${ALTUN_TEST_SETARCH_EXECUTABLE} ${CMAKE_SYSTEM_PROCESSOR} -R)
			message(STATUS
				"${PRO_NAME}: TSan tests disable ASLR per process with setarch."
			)
		endif ()
	endif ()
	mark_as_advanced(ALTUN_TEST_SETARCH_EXECUTABLE)
endif ()


function(configure_altun_test_target test_target)
	altun_limit_heavy_compile(${test_target})
	altun_enable_strict_warnings(${test_target})

	target_include_directories(${test_target} PRIVATE ${ALTUN_TEST_SOURCE_DIR})
	set_target_properties(${test_target} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${ALTUN_OUTPUT_DIR}/bin
	)
	if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
		target_compile_definitions(${test_target} PRIVATE _GLIBCXX_ASSERTIONS)
	endif ()

	if (UNIX)
		target_link_options(${test_target} PRIVATE -Wl,-rpath,$ORIGIN)
	endif ()

	if (ALTUN_ENABLE_TEST_SANITIZERS OR ALTUN_ENABLE_TEST_TSAN)
		target_link_libraries(${test_target} PRIVATE altun.test.sanitizer)
	endif ()
endfunction()


function(add_altun_ctest test_name)
	add_test(NAME ${test_name} COMMAND ${ALTUN_TEST_LAUNCHER} ${ARGN})
	configure_altun_ctest_environment(${test_name})
endfunction()

function(configure_altun_sanitized_test test_name)
	if (ALTUN_ENABLE_TEST_SANITIZERS OR ALTUN_ENABLE_TEST_TSAN)
		set_property(TEST ${test_name} APPEND PROPERTY LABELS sanitizer)
		set_property(TEST ${test_name} PROPERTY RUN_SERIAL TRUE)

		set_property(TEST ${test_name} APPEND PROPERTY FAIL_REGULAR_EXPRESSION
			"AddressSanitizer" "LeakSanitizer" "ThreadSanitizer" "runtime error:"
		)
		if (ALTUN_ENABLE_TEST_SANITIZERS)
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


function(add_altun_test_program test_target library)
	add_executable(${test_target} ${ALTUN_TEST_SOURCE_DIR}/main.cpp ${ARGN})
	target_link_libraries(${test_target} PRIVATE ${library})
	configure_altun_test_target(${test_target})
endfunction()


function(add_altun_functional_test name library)
	set(test_target altun.test.${name})
	add_altun_test_program(${test_target} ${library} ${ARGN})

	add_altun_ctest(altun.${name} $<TARGET_FILE:${test_target}>)
	set_tests_properties(altun.${name} PROPERTIES
		LABELS functional
		TIMEOUT ${ALTUN_FUNCTIONAL_TIMEOUT}
	)
	set_property(TEST altun.${name} APPEND PROPERTY ENVIRONMENT
		"ALTUN_TEST_REPEAT=${ALTUN_FUNCTIONAL_REPEAT}"
		"ALTUN_TEST_SEED=${ALTUN_FUNCTIONAL_SEED}"
		"ALTUN_TEST_FAIL_FAST=1"
	)
	configure_altun_sanitized_test(altun.${name})
endfunction()


function(add_altun_stress_test name library)
	set(test_target altun.test.stress.${name})
	add_altun_test_program(${test_target} ${library} ${ARGN})

	target_compile_definitions(${test_target} PRIVATE
		ALTUN_STRESS_SCALE=${ALTUN_STRESS_SCALE}
	)
	add_altun_ctest(altun.stress.${name} $<TARGET_FILE:${test_target}>
		--repeat ${ALTUN_STRESS_REPEAT}
		--seed ${ALTUN_STRESS_SEED}
		--fail-fast
	)
	set_tests_properties(altun.stress.${name} PROPERTIES
		LABELS stress
		RUN_SERIAL TRUE
		TIMEOUT ${ALTUN_STRESS_TIMEOUT}
	)
	configure_altun_sanitized_test(altun.stress.${name})
endfunction()


# Instrument the library code under test, not just the test executables.
if (ALTUN_ENABLE_TEST_SANITIZERS OR ALTUN_ENABLE_TEST_TSAN)
	foreach(target altun.core altun.linux)
		if (TARGET ${target})
			target_link_libraries(${target} PUBLIC altun.test.sanitizer)
		endif ()
	endforeach()
endif ()

add_library(altun.test.plugin SHARED functional/core/library_fixture.cpp)
set_target_properties(altun.test.plugin PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY ${ALTUN_OUTPUT_DIR}/bin
	RUNTIME_OUTPUT_DIRECTORY ${ALTUN_OUTPUT_DIR}/bin
)
configure_altun_test_target(altun.test.plugin)

add_executable(altun.test.app functional/core/process_app.cpp)
configure_altun_test_target(altun.test.app)

add_subdirectory(functional)
if (ALTUN_BUILD_STRESS_TESTS)
	add_subdirectory(stress)
else ()
	message(STATUS
		"${PRO_NAME}: Enable ALTUN_BUILD_STRESS_TESTS for stress tests."
	)
endif ()

if (ALTUN_BUILD_PERFORMANCE_TESTS AND
	(ALTUN_PERFORMANCE_SBUS_CYCLONE OR ALTUN_PERFORMANCE_SBUS_DBUS OR
	ALTUN_PERFORMANCE_SBUS_SHM))
	add_subdirectory(performance)

elseif (ALTUN_BUILD_PERFORMANCE_TESTS)
	message(STATUS
		"${PRO_NAME}: No usable optional SBus transport is enabled; "
		"skipping performance benchmarks."
	)
else ()
	message(STATUS
		"${PRO_NAME}: Enable ALTUN_BUILD_PERFORMANCE_TESTS for performance benchmarks."
	)
endif ()

if (ALTUN_BUILD_CMAKE_TESTS)
	add_subdirectory(cmake)
endif ()
