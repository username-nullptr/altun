# SPDX-FileCopyrightText: 2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.15)
project(libempp_install_consumer LANGUAGES CXX)

if (LIBEMPP_REQUEST_MISSING_COMPONENT)
	find_package(libEMpp CONFIG QUIET COMPONENTS
		${LIBEMPP_REQUEST_MISSING_COMPONENT}
	)
	if (libEMpp_FOUND)
		message(FATAL_ERROR
			"libEMpp unexpectedly provides component "
			"'${LIBEMPP_REQUEST_MISSING_COMPONENT}'."
		)
	endif ()

	message(STATUS
		"libEMpp correctly rejected component "
		"'${LIBEMPP_REQUEST_MISSING_COMPONENT}'."
	)
	return()
endif ()

if (NOT LIBEMPP_EXPECTED_COMPONENTS)
	message(FATAL_ERROR "LIBEMPP_EXPECTED_COMPONENTS is required.")
endif ()

string(REPLACE "," ";" libempp_components
	"${LIBEMPP_EXPECTED_COMPONENTS}"
)

find_package(libEMpp ${LIBEMPP_EXPECTED_VERSION} EXACT CONFIG REQUIRED
	COMPONENTS ${libempp_components}
)
enable_testing()

set(libempp_source_targets)
set(libempp_expected_definitions)

if (WIN32 AND LIBEMPP_RUNTIME_DIR)
	file(GLOB libempp_runtime_dlls "${LIBEMPP_RUNTIME_DIR}/*.dll")

	# Keep the test independent of the caller's PATH and copy the runtime that
	# belongs to the compiler used for this standalone consumer.
	if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		get_filename_component(compiler_runtime_dir
			"${CMAKE_CXX_COMPILER}" DIRECTORY
		)
		foreach(runtime_name
			libstdc++-6.dll
			libgcc_s_seh-1.dll
			libgcc_s_dw2-1.dll
			libgcc_s_sjlj-1.dll
			libwinpthread-1.dll
		)
			execute_process(
				COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=${runtime_name}
				OUTPUT_VARIABLE runtime_path
				OUTPUT_STRIP_TRAILING_WHITESPACE
				ERROR_QUIET
			)
			if (NOT IS_ABSOLUTE "${runtime_path}" OR
				NOT EXISTS "${runtime_path}")
				set(runtime_path "${compiler_runtime_dir}/${runtime_name}")
			endif ()
			if (IS_ABSOLUTE "${runtime_path}" AND EXISTS "${runtime_path}")
				list(APPEND libempp_runtime_dlls "${runtime_path}")
			endif ()
		endforeach()
		list(REMOVE_DUPLICATES libempp_runtime_dlls)
	endif ()
endif ()

function(add_libempp_consumer_runtime_test target)
	# CMake 3.15 predates TARGET_RUNTIME_DLLS and CTest's
	# ENVIRONMENT_MODIFICATION property.
	if (libempp_runtime_dlls)
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				${libempp_runtime_dlls} "$<TARGET_FILE_DIR:${target}>"
			COMMAND_EXPAND_LISTS
		)
	endif ()

	add_test(NAME ${target} COMMAND $<TARGET_FILE:${target}>)

	# The install tree is intentionally outside the system loader's configured
	# prefixes. Make all co-installed shared dependencies discoverable without
	# changing their production RPATH policy.
	if (UNIX AND LIBEMPP_LIBRARY_DIR)
		if (APPLE)
			set(loader_path_variable DYLD_LIBRARY_PATH)
		else ()
			set(loader_path_variable LD_LIBRARY_PATH)
		endif ()
		set_property(TEST ${target} PROPERTY ENVIRONMENT
			"${loader_path_variable}=${LIBEMPP_LIBRARY_DIR}:$ENV{${loader_path_variable}}"
		)
	endif ()
endfunction()

foreach(component IN LISTS libempp_components)
	if (NOT libEMpp_${component}_FOUND)
		message(FATAL_ERROR "Component '${component}' was not reported as found.")
	endif ()

	if (NOT TARGET libEMpp::${component})
		message(FATAL_ERROR "Missing imported target libEMpp::${component}.")
	endif ()

	if (NOT TARGET empp.${component})
		message(FATAL_ERROR "Missing compatibility target empp.${component}.")
	endif ()

	string(TOUPPER "${component}" component_upper)
	set(target libempp_consumer_${component})

	add_executable(${target} main.cpp)
	target_compile_features(${target} PRIVATE cxx_std_20)
	target_compile_definitions(${target} PRIVATE
		"LIBEMPP_CONSUME_${component_upper}=1"
	)
	target_link_libraries(${target} PRIVATE libEMpp::${component})
	add_libempp_consumer_runtime_test(${target})

	list(APPEND libempp_source_targets empp.${component})
	list(APPEND libempp_expected_definitions
		"LIBEMPP_CONSUME_${component_upper}=1"
	)
endforeach()

add_executable(libempp_consumer_source_targets main.cpp)
target_compile_features(libempp_consumer_source_targets PRIVATE cxx_std_20)
target_compile_definitions(libempp_consumer_source_targets PRIVATE
	${libempp_expected_definitions}
	LIBEMPP_CONSUME_UMBRELLA=1
)
target_link_libraries(libempp_consumer_source_targets PRIVATE
	${libempp_source_targets}
)
add_libempp_consumer_runtime_test(libempp_consumer_source_targets)
