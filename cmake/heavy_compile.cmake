# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(LIBEMPP_HEAVY_COMPILE_JOBS_DEFAULT 8)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(LIBEMPP_HEAVY_COMPILE_JOBS_DEFAULT 6)
else ()
	set(LIBEMPP_HEAVY_COMPILE_JOBS_DEFAULT 0)
endif ()

set(LIBEMPP_HEAVY_COMPILE_JOBS ${LIBEMPP_HEAVY_COMPILE_JOBS_DEFAULT} CACHE STRING
	"Maximum concurrent memory-heavy libEMpp compilations; 0 disables the limit."
)
if (NOT LIBEMPP_HEAVY_COMPILE_JOBS MATCHES "^[0-9]+$")
	message(FATAL_ERROR
		"${PRO_NAME}: LIBEMPP_HEAVY_COMPILE_JOBS must be a non-negative integer."
	)
endif ()

option(LIBEMPP_LOW_MEMORY_DEBUG_INFO
	"-- ${PRO_NAME}: Use reduced GCC debug information to lower compiler memory use." OFF
)
if (LIBEMPP_LOW_MEMORY_DEBUG_INFO)
	if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		add_compile_options(
			$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:-g1>
		)
		message(STATUS
			"${PRO_NAME}: Use reduced GCC debug information for lower compile memory."
		)
	else ()
		message(FATAL_ERROR
			"${PRO_NAME}: LIBEMPP_LOW_MEMORY_DEBUG_INFO requires the GNU compiler."
		)
	endif ()
endif ()

if (LIBEMPP_HEAVY_COMPILE_JOBS GREATER 0)
	message(STATUS
		"${PRO_NAME}: Limit concurrent heavy compilations to ${LIBEMPP_HEAVY_COMPILE_JOBS}."
	)
	if (CMAKE_GENERATOR MATCHES "Ninja")
		set_property(GLOBAL APPEND PROPERTY JOB_POOLS
			libempp_heavy_compile=${LIBEMPP_HEAVY_COMPILE_JOBS}
		)
	endif ()
endif ()

function(libempp_limit_heavy_compile target)
	if (LIBEMPP_HEAVY_COMPILE_JOBS EQUAL 0)
		return()
	endif ()

	if (CMAKE_GENERATOR MATCHES "Ninja")
		set_property(TARGET ${target} PROPERTY JOB_POOL_COMPILE libempp_heavy_compile)
		return()
	endif ()

	# Other generators have no compile pool. Keep independent heavy targets in
	# bounded dependency lanes while leaving unrelated targets fully parallel.
	get_property(libempp_heavy_index GLOBAL PROPERTY LIBEMPP_HEAVY_COMPILE_INDEX)
	if (NOT libempp_heavy_index)
		set(libempp_heavy_index 0)
	endif ()

	math(EXPR libempp_heavy_lane
		"${libempp_heavy_index} % ${LIBEMPP_HEAVY_COMPILE_JOBS}"
	)
	get_property(libempp_heavy_previous GLOBAL PROPERTY
		LIBEMPP_HEAVY_COMPILE_LANE_${libempp_heavy_lane}
	)
	if (libempp_heavy_previous)
		add_dependencies(${target} ${libempp_heavy_previous})
	endif ()

	set_property(GLOBAL PROPERTY
		LIBEMPP_HEAVY_COMPILE_LANE_${libempp_heavy_lane} ${target}
	)
	math(EXPR libempp_heavy_index "${libempp_heavy_index} + 1")
	set_property(GLOBAL PROPERTY LIBEMPP_HEAVY_COMPILE_INDEX ${libempp_heavy_index})
endfunction()
