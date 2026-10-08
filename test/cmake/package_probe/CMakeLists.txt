# SPDX-FileCopyrightText: 2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.15)
project(altun_package_probe NONE)

if (NOT PROBE_COMPONENT)
	message(FATAL_ERROR "PROBE_COMPONENT is required.")
endif ()

find_package(altun CONFIG QUIET COMPONENTS ${PROBE_COMPONENT})

if (PROBE_EXPECT_FOUND)
	if (NOT altun_FOUND)
		message(FATAL_ERROR
			"altun component '${PROBE_COMPONENT}' was unexpectedly rejected: "
			"${altun_NOT_FOUND_MESSAGE}"
		)
	endif ()
else ()
	if (altun_FOUND)
		message(FATAL_ERROR
			"altun component '${PROBE_COMPONENT}' was unexpectedly accepted."
		)
	endif ()

	if (PROBE_EXPECT_ERROR AND NOT altun_NOT_FOUND_MESSAGE MATCHES
		"${PROBE_EXPECT_ERROR}")
		message(FATAL_ERROR
			"altun component '${PROBE_COMPONENT}' failed for the wrong reason.\n"
			"Expected: ${PROBE_EXPECT_ERROR}\n"
			"Actual: ${altun_NOT_FOUND_MESSAGE}"
		)
	endif ()
endif ()
