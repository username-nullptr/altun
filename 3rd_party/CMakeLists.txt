# SPDX-FileCopyrightText: 2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

option(ALTUN_USE_BUNDLED_RIWO
	"-- ${PRO_NAME}: Use the bundled Riwo dependency." ON
)
set(ALTUN_RIWO_INSTALL_PREFIX "" CACHE PATH
	"Install prefix of an external Riwo package."
)
if (ALTUN_USE_BUNDLED_RIWO)
	if (ALTUN_RIWO_INSTALL_PREFIX)
		message(FATAL_ERROR
			"${PRO_NAME}: ALTUN_RIWO_INSTALL_PREFIX is only valid when "
			"ALTUN_USE_BUNDLED_RIWO=OFF."
		)
	endif ()

	if (DEFINED RIWO_BUILD_CORO AND NOT RIWO_BUILD_CORO)
		message(FATAL_ERROR
			"${PRO_NAME}: The bundled Riwo requires RIWO_BUILD_CORO=ON."
		)
	endif ()

	if (DEFINED RIWO_BUILD_UTILITIES AND NOT RIWO_BUILD_UTILITIES)
		message(FATAL_ERROR
			"${PRO_NAME}: The bundled Riwo requires RIWO_BUILD_UTILITIES=ON."
		)
	endif ()

	set(RIWO_BUILD_STATIC ${ALTUN_BUILD_STATIC})
	set(RIWO_ADD_LIBRARY_VERSION ${ALTUN_ADD_LIBRARY_VERSION})
	set(RIWO_USE_LIBCXX ${ALTUN_USE_LIBCXX})
	set(RIWO_USE_LLD ${ALTUN_USE_LLD})
	set(RIWO_ENABLE_LTO ${ALTUN_ENABLE_LTO})

	set(RIWO_BUILD_CORO ON)
	set(RIWO_BUILD_HTTP OFF)
	set(RIWO_BUILD_WEBSOCKET OFF)
	set(RIWO_BUILD_UTILITIES ON)
	set(RIWO_BUILD_EXAMPLES OFF)
	set(RIWO_BUILD_CMAKE_TESTS OFF)
	set(RIWO_BUILD_FUZZERS OFF)
	set(RIWO_BUILD_STRESS_TESTS OFF)
	set(RIWO_BUILD_PERFORMANCE_TESTS OFF)
	set(RIWO_ENABLE_TEST_SANITIZERS OFF)
	set(RIWO_ENABLE_TEST_TSAN OFF)

	# Riwo is a dependency here. Preserve the parent test selection while
	# preventing the dependency's own test tree from entering this build.
	set(altun_build_testing_was_defined OFF)

	if (DEFINED BUILD_TESTING)
		set(altun_build_testing_was_defined ON)
		set(altun_saved_build_testing "${BUILD_TESTING}")
	endif ()

	set(BUILD_TESTING OFF)
	add_subdirectory(riwo)

	if (ALTUN_ENABLE_TEST_SANITIZERS OR ALTUN_ENABLE_TEST_TSAN)
		# The dependency's test tree stays disabled, but its libraries are part
		# of the code under test and must use the parent's sanitizer mode.
		foreach(altun_bundled_riwo_target
			riwo.core riwo.coro riwo.http riwo.websocket riwo.utils
		)
			if (TARGET ${altun_bundled_riwo_target})
				target_link_libraries(${altun_bundled_riwo_target} PRIVATE
					altun.test.sanitizer
				)
			endif ()
		endforeach()
	endif ()

	if (altun_build_testing_was_defined)
		set(BUILD_TESTING "${altun_saved_build_testing}")
	else ()
		unset(BUILD_TESTING)
	endif ()

	message(STATUS "${PRO_NAME}: Use bundled Riwo.")

else ()
	altun_validate_dependency_prefix(Riwo ALTUN_RIWO_INSTALL_PREFIX)

	if (ALTUN_RIWO_INSTALL_PREFIX)
		find_package(Riwo CONFIG REQUIRED COMPONENTS coro utils
			PATHS "${ALTUN_RIWO_INSTALL_PREFIX}"
			NO_DEFAULT_PATH
		)
	else ()
		find_package(Riwo CONFIG REQUIRED COMPONENTS coro utils)
	endif ()

	altun_validate_msvc_imported_targets(Riwo
		ALTUN_RIWO_INSTALL_PREFIX
		Riwo::core Riwo::coro Riwo::utils
	)
	foreach(altun_riwo_target
		Riwo::core Riwo::coro Riwo::utils
		riwo.core riwo.coro riwo.utils
	)
		if (TARGET ${altun_riwo_target})
			set_property(TARGET ${altun_riwo_target} PROPERTY
				IMPORTED_GLOBAL TRUE
			)
		endif ()
	endforeach()

	message(STATUS "${PRO_NAME}: Use external Riwo ${Riwo_VERSION}.")
endif ()

option(ALTUN_USE_EMBEDDED_NLOHMANN
	"-- ${PRO_NAME}: Use the embedded nlohmann/json dependency." ON
)
set(ALTUN_NLOHMANN_INSTALL_PREFIX "" CACHE PATH
	"Install prefix of an external nlohmann/json package."
)
add_library(altun.nlohmann INTERFACE)
add_library(altun::nlohmann ALIAS altun.nlohmann)

set_target_properties(altun.nlohmann PROPERTIES EXPORT_NAME nlohmann)

if (ALTUN_USE_EMBEDDED_NLOHMANN)
	if (ALTUN_NLOHMANN_INSTALL_PREFIX)
		message(FATAL_ERROR
			"${PRO_NAME}: ALTUN_NLOHMANN_INSTALL_PREFIX is only valid when "
			"ALTUN_USE_EMBEDDED_NLOHMANN=OFF."
		)
	endif ()

	target_include_directories(altun.nlohmann SYSTEM INTERFACE
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/nlohmann.json>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	)
	add_subdirectory(nlohmann.json)
	message(STATUS "${PRO_NAME}: Use embedded nlohmann/json.")

else ()
	altun_validate_dependency_prefix(nlohmann/json
		ALTUN_NLOHMANN_INSTALL_PREFIX
	)

	if (ALTUN_NLOHMANN_INSTALL_PREFIX)
		find_package(nlohmann_json CONFIG REQUIRED
			PATHS "${ALTUN_NLOHMANN_INSTALL_PREFIX}"
			NO_DEFAULT_PATH
		)
	else ()
		find_package(nlohmann_json CONFIG REQUIRED)
	endif ()

	set_property(TARGET nlohmann_json::nlohmann_json PROPERTY
		IMPORTED_GLOBAL TRUE
	)
	target_link_libraries(altun.nlohmann INTERFACE
		nlohmann_json::nlohmann_json
	)
	message(STATUS
		"${PRO_NAME}: Use external nlohmann/json ${nlohmann_json_VERSION}."
	)
endif ()

install(TARGETS altun.nlohmann EXPORT altunTargets)
