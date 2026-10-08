# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

option(BUILD_TESTING
	"-- ${PRO_NAME}: Build automated tests." OFF
)
option(ALTUN_BUILD_CMAKE_TESTS
	"-- ${PRO_NAME}: Test the installed CMake package."
	${BUILD_TESTING}
)
option(ALTUN_ENABLE_TEST_SANITIZERS
	"-- ${PRO_NAME}: Enable ASan and UBSan for functional and stress tests." OFF
)
option(ALTUN_ENABLE_TEST_TSAN
	"-- ${PRO_NAME}: Enable TSan for functional and stress tests." OFF
)
option(ALTUN_BUILD_STRESS_TESTS
	"-- ${PRO_NAME}: Build high-pressure stability tests." OFF
)
option(ALTUN_BUILD_PERFORMANCE_TESTS
	"-- ${PRO_NAME}: Build opt-in performance benchmarks." OFF
)

# Compatibility with the option used by the original single-binary test tree.
option(ALTUN_TEST_SANITIZERS
	"Deprecated alias for ALTUN_ENABLE_TEST_SANITIZERS." OFF
)
if (ALTUN_TEST_SANITIZERS)
	set(ALTUN_ENABLE_TEST_SANITIZERS ON CACHE BOOL
		"-- ${PRO_NAME}: Enable ASan and UBSan for functional and stress tests."
		FORCE
	)
endif ()

set(ALTUN_FUNCTIONAL_REPEAT 3 CACHE STRING
	"Execution count for each altun functional test case (positive integer)."
)
set(ALTUN_FUNCTIONAL_SEED 1 CACHE STRING
	"Base seed for reproducible altun functional tests (non-negative integer)."
)
set(ALTUN_FUNCTIONAL_TIMEOUT 120 CACHE STRING
	"CTest timeout in seconds for each altun functional executable."
)
set(ALTUN_STRESS_SCALE 5 CACHE STRING
	"Work multiplier for altun stress tests (positive integer)."
)
set(ALTUN_STRESS_REPEAT 3 CACHE STRING
	"Fixture recreation count for each altun stress case (positive integer)."
)
set(ALTUN_STRESS_SEED 1 CACHE STRING
	"Base seed for reproducible altun stress scheduling (non-negative integer)."
)
set(ALTUN_STRESS_TIMEOUT 180 CACHE STRING
	"CTest timeout in seconds for each altun stress executable."
)
set(ALTUN_PERFORMANCE_SCALE 1 CACHE STRING
	"Work multiplier for altun performance benchmarks (positive integer)."
)
set(ALTUN_PERFORMANCE_TIMEOUT 180 CACHE STRING
	"CTest timeout in seconds for each altun performance benchmark."
)

foreach(option
	ALTUN_FUNCTIONAL_REPEAT
	ALTUN_FUNCTIONAL_TIMEOUT
	ALTUN_STRESS_SCALE
	ALTUN_STRESS_REPEAT
	ALTUN_STRESS_TIMEOUT
	ALTUN_PERFORMANCE_SCALE
	ALTUN_PERFORMANCE_TIMEOUT
)
	if (NOT ${option} MATCHES "^[1-9][0-9]*$")
		message(FATAL_ERROR "${option} must be a positive integer.")
	endif ()
endforeach ()

foreach(option ALTUN_FUNCTIONAL_SEED ALTUN_STRESS_SEED)
	if (NOT ${option} MATCHES "^[0-9]+$")
		message(FATAL_ERROR "${option} must be a non-negative integer.")
	endif ()
endforeach ()

if (ALTUN_ENABLE_TEST_SANITIZERS AND ALTUN_ENABLE_TEST_TSAN)
	message(FATAL_ERROR
		"${PRO_NAME}: ASan/UBSan and TSan cannot be enabled together."
	)
endif ()

if (ALTUN_BUILD_CMAKE_TESTS AND NOT BUILD_TESTING)
	message(FATAL_ERROR
		"${PRO_NAME}: CMake integration tests require BUILD_TESTING=ON."
	)
endif ()

if ((ALTUN_BUILD_STRESS_TESTS OR ALTUN_BUILD_PERFORMANCE_TESTS) AND
	NOT BUILD_TESTING)
	message(FATAL_ERROR
		"${PRO_NAME}: Stress and performance tests require BUILD_TESTING=ON."
	)
endif ()

if (ALTUN_BUILD_PERFORMANCE_TESTS AND
	(ALTUN_ENABLE_TEST_SANITIZERS OR ALTUN_ENABLE_TEST_TSAN))
	message(FATAL_ERROR
		"${PRO_NAME}: Performance tests cannot be combined with test sanitizers."
	)
endif ()

if (ALTUN_BUILD_PERFORMANCE_TESTS)
	if (NOT UNIX)
		message(FATAL_ERROR
			"${PRO_NAME}: Performance tests require a UNIX platform."
		)
	endif ()

	if (NOT (ALTUN_BUILD_SBUS_CYCLONE AND ALTUN_BUILD_SBUS_DBUS AND
		ALTUN_BUILD_SBUS_SHM))
		message(FATAL_ERROR
			"${PRO_NAME}: Performance tests require all CycloneDDS, D-Bus, "
			"and shared-memory SBus transports."
		)
	endif ()

	find_program(ALTUN_PERFORMANCE_DBUS_RUN_SESSION NAMES dbus-run-session)
	if (NOT ALTUN_PERFORMANCE_DBUS_RUN_SESSION)
		message(FATAL_ERROR
			"${PRO_NAME}: Performance tests require dbus-run-session."
		)
	endif ()
endif ()

if (ALTUN_ENABLE_TEST_SANITIZERS OR ALTUN_ENABLE_TEST_TSAN)
	if (NOT BUILD_TESTING)
		message(FATAL_ERROR
			"${PRO_NAME}: Test sanitizers require BUILD_TESTING=ON."
		)
	endif ()

	if (MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
		message(FATAL_ERROR
			"${PRO_NAME}: Test sanitizers require GCC or Clang with a GNU-style driver."
		)
	endif ()

	if (ALTUN_ENABLE_LTO)
		message(FATAL_ERROR
			"${PRO_NAME}: Disable ALTUN_ENABLE_LTO for sanitizer builds."
		)
	endif ()

	include(CheckCXXSourceCompiles)
	set(altun_saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
	set(altun_saved_required_link_options "${CMAKE_REQUIRED_LINK_OPTIONS}")

	if (ALTUN_ENABLE_TEST_SANITIZERS)
		set(altun_sanitizer_flags -fsanitize=address,undefined)
	else ()
		set(altun_sanitizer_flags -fsanitize=thread)
	endif ()

	set(CMAKE_REQUIRED_FLAGS
		"${altun_saved_required_flags} ${altun_sanitizer_flags}"
	)
	set(CMAKE_REQUIRED_LINK_OPTIONS
		${altun_saved_required_link_options} ${altun_sanitizer_flags}
	)
	unset(ALTUN_TEST_SANITIZER_AVAILABLE CACHE)

	check_cxx_source_compiles("int main() { return 0; }"
		ALTUN_TEST_SANITIZER_AVAILABLE
	)
	set(CMAKE_REQUIRED_FLAGS "${altun_saved_required_flags}")
	set(CMAKE_REQUIRED_LINK_OPTIONS ${altun_saved_required_link_options})

	if (NOT ALTUN_TEST_SANITIZER_AVAILABLE)
		message(FATAL_ERROR
			"${PRO_NAME}: Requested test sanitizer runtime is unavailable."
		)
	endif ()

	add_library(altun.test.sanitizer INTERFACE)
	set_target_properties(altun.test.sanitizer PROPERTIES
		EXPORT_NAME sanitizer
	)
	add_library(altun::sanitizer ALIAS altun.test.sanitizer)
	install(TARGETS altun.test.sanitizer EXPORT altunTargets)

	if (ALTUN_ENABLE_TEST_SANITIZERS)
		target_compile_options(altun.test.sanitizer INTERFACE
			-fsanitize=address,undefined
			-fno-omit-frame-pointer
			-fno-sanitize-recover=all
		)
		target_link_options(altun.test.sanitizer INTERFACE
			-fsanitize=address,undefined
		)
		# Keep the bundled Riwo modules on the same instrumentation mode.
		set(RIWO_ENABLE_TEST_SANITIZERS ON)
	else ()
		target_compile_options(altun.test.sanitizer INTERFACE
			-fsanitize=thread
			-fno-omit-frame-pointer
			-fno-sanitize-recover=all
		)
		target_link_options(altun.test.sanitizer INTERFACE -fsanitize=thread)
		set(RIWO_ENABLE_TEST_TSAN ON)
	endif ()
endif ()
