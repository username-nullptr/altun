# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

set(ALTUN_HEAVY_COMPILE_JOBS_DEFAULT 0)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(ALTUN_HEAVY_COMPILE_JOBS_DEFAULT 8)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(ALTUN_HEAVY_COMPILE_JOBS_DEFAULT 6)
endif ()

set(ALTUN_HEAVY_COMPILE_JOBS ${ALTUN_HEAVY_COMPILE_JOBS_DEFAULT} CACHE STRING
	"Maximum concurrent memory-heavy altun compilations; 0 disables the limit."
)
if (NOT ALTUN_HEAVY_COMPILE_JOBS MATCHES "^[0-9]+$")
	message(FATAL_ERROR
		"${PRO_NAME}: ALTUN_HEAVY_COMPILE_JOBS must be a non-negative integer."
	)
endif ()

option(ALTUN_LOW_MEMORY_DEBUG_INFO
	"-- ${PRO_NAME}: Use reduced GCC debug information to lower compiler memory use." OFF
)
if (ALTUN_LOW_MEMORY_DEBUG_INFO)
	if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		add_compile_options(
			$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Debug>>:-g1>
		)
		message(STATUS
			"${PRO_NAME}: Use reduced GCC debug information for lower compile memory."
		)
	else ()
		message(FATAL_ERROR
			"${PRO_NAME}: ALTUN_LOW_MEMORY_DEBUG_INFO requires the GNU compiler."
		)
	endif ()
endif ()

if (ALTUN_HEAVY_COMPILE_JOBS GREATER 0)
	message(STATUS
		"${PRO_NAME}: Limit concurrent heavy compilations to ${ALTUN_HEAVY_COMPILE_JOBS}."
	)
	if (CMAKE_GENERATOR MATCHES "Ninja")
		set_property(GLOBAL APPEND PROPERTY JOB_POOLS
			altun_heavy_compile=${ALTUN_HEAVY_COMPILE_JOBS}
		)
	endif ()
endif ()

function(altun_limit_heavy_compile target)
	if (ALTUN_HEAVY_COMPILE_JOBS EQUAL 0)
		return()
	endif ()

	if (CMAKE_GENERATOR MATCHES "Ninja")
		set_property(TARGET ${target} PROPERTY JOB_POOL_COMPILE altun_heavy_compile)
		return()
	endif ()

	# Other generators have no compile pool. Keep independent heavy targets in
	# bounded dependency lanes while leaving unrelated targets fully parallel.
	get_property(altun_heavy_index GLOBAL PROPERTY ALTUN_HEAVY_COMPILE_INDEX)
	if (NOT altun_heavy_index)
		set(altun_heavy_index 0)
	endif ()

	math(EXPR altun_heavy_lane
		"${altun_heavy_index} % ${ALTUN_HEAVY_COMPILE_JOBS}"
	)
	get_property(altun_heavy_previous GLOBAL PROPERTY
		ALTUN_HEAVY_COMPILE_LANE_${altun_heavy_lane}
	)
	if (altun_heavy_previous)
		add_dependencies(${target} ${altun_heavy_previous})
	endif ()

	set_property(GLOBAL PROPERTY
		ALTUN_HEAVY_COMPILE_LANE_${altun_heavy_lane} ${target}
	)
	math(EXPR altun_heavy_index "${altun_heavy_index} + 1")
	set_property(GLOBAL PROPERTY ALTUN_HEAVY_COMPILE_INDEX ${altun_heavy_index})
endfunction()
