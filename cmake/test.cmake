# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

option(BUILD_TESTING
	"-- ${PRO_NAME}: Build automated tests." OFF
)
option(LIBEMPP_ENABLE_TEST_SANITIZERS
	"-- ${PRO_NAME}: Enable ASan and UBSan for functional and stress tests." OFF
)
option(LIBEMPP_ENABLE_TEST_TSAN
	"-- ${PRO_NAME}: Enable TSan for functional and stress tests." OFF
)
option(LIBEMPP_BUILD_STRESS_TESTS
	"-- ${PRO_NAME}: Build high-pressure stability tests." OFF
)
option(LIBEMPP_BUILD_PERFORMANCE_TESTS
	"-- ${PRO_NAME}: Build opt-in performance benchmarks." OFF
)

# Compatibility with the option used by the original single-binary test tree.
option(LIBEMPP_TEST_SANITIZERS
	"Deprecated alias for LIBEMPP_ENABLE_TEST_SANITIZERS." OFF
)
if (LIBEMPP_TEST_SANITIZERS)
	set(LIBEMPP_ENABLE_TEST_SANITIZERS ON CACHE BOOL
		"-- ${PRO_NAME}: Enable ASan and UBSan for functional and stress tests."
		FORCE
	)
endif ()

set(LIBEMPP_FUNCTIONAL_REPEAT 1 CACHE STRING
	"Execution count for each libEMpp functional test case (positive integer)."
)
set(LIBEMPP_FUNCTIONAL_SEED 1 CACHE STRING
	"Base seed for reproducible libEMpp functional tests (non-negative integer)."
)
set(LIBEMPP_FUNCTIONAL_TIMEOUT 60 CACHE STRING
	"CTest timeout in seconds for each libEMpp functional executable."
)
set(LIBEMPP_STRESS_SCALE 4 CACHE STRING
	"Work multiplier for libEMpp stress tests (positive integer)."
)
set(LIBEMPP_STRESS_REPEAT 1 CACHE STRING
	"Fixture recreation count for each libEMpp stress case (positive integer)."
)
set(LIBEMPP_STRESS_SEED 1 CACHE STRING
	"Base seed for reproducible libEMpp stress scheduling (non-negative integer)."
)
set(LIBEMPP_STRESS_TIMEOUT 180 CACHE STRING
	"CTest timeout in seconds for each libEMpp stress executable."
)
set(LIBEMPP_PERFORMANCE_SCALE 1 CACHE STRING
	"Work multiplier for libEMpp performance benchmarks (positive integer)."
)
set(LIBEMPP_PERFORMANCE_TIMEOUT 180 CACHE STRING
	"CTest timeout in seconds for each libEMpp performance benchmark."
)

foreach(option
	LIBEMPP_FUNCTIONAL_REPEAT
	LIBEMPP_FUNCTIONAL_TIMEOUT
	LIBEMPP_STRESS_SCALE
	LIBEMPP_STRESS_REPEAT
	LIBEMPP_STRESS_TIMEOUT
	LIBEMPP_PERFORMANCE_SCALE
	LIBEMPP_PERFORMANCE_TIMEOUT
)
	if (NOT ${option} MATCHES "^[1-9][0-9]*$")
		message(FATAL_ERROR "${option} must be a positive integer.")
	endif ()
endforeach ()

foreach(option LIBEMPP_FUNCTIONAL_SEED LIBEMPP_STRESS_SEED)
	if (NOT ${option} MATCHES "^[0-9]+$")
		message(FATAL_ERROR "${option} must be a non-negative integer.")
	endif ()
endforeach ()

if (LIBEMPP_ENABLE_TEST_SANITIZERS AND LIBEMPP_ENABLE_TEST_TSAN)
	message(FATAL_ERROR
		"${PRO_NAME}: ASan/UBSan and TSan cannot be enabled together."
	)
endif ()

if (LIBEMPP_ENABLE_TEST_SANITIZERS OR LIBEMPP_ENABLE_TEST_TSAN)
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
	if (ENABLE_LTO)
		message(FATAL_ERROR "${PRO_NAME}: Disable ENABLE_LTO for sanitizer builds.")
	endif ()

	add_library(libempp.test.sanitizer INTERFACE)
	if (LIBEMPP_ENABLE_TEST_SANITIZERS)
		target_compile_options(libempp.test.sanitizer INTERFACE
			-fsanitize=address,undefined
			-fno-omit-frame-pointer
			-fno-sanitize-recover=all
		)
		target_link_options(libempp.test.sanitizer INTERFACE
			-fsanitize=address,undefined
		)
		# Keep the bundled LibGS modules on the same instrumentation mode.
		set(LIBGS_ENABLE_TEST_SANITIZERS ON)
	else ()
		target_compile_options(libempp.test.sanitizer INTERFACE
			-fsanitize=thread
			-fno-omit-frame-pointer
			-fno-sanitize-recover=all
		)
		target_link_options(libempp.test.sanitizer INTERFACE -fsanitize=thread)
		set(LIBGS_ENABLE_TEST_TSAN ON)
	endif ()
endif ()
