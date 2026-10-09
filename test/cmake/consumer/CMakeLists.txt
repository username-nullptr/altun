# SPDX-FileCopyrightText: 2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.15)
project(altun_install_consumer LANGUAGES CXX)

if (ALTUN_REQUEST_MISSING_COMPONENT)
	find_package(Altun CONFIG QUIET COMPONENTS
		${ALTUN_REQUEST_MISSING_COMPONENT}
	)
	if (Altun_FOUND)
		message(FATAL_ERROR
			"Altun unexpectedly provides component "
			"'${ALTUN_REQUEST_MISSING_COMPONENT}'."
		)
	endif ()

	message(STATUS
		"Altun correctly rejected component "
		"'${ALTUN_REQUEST_MISSING_COMPONENT}'."
	)
	return()
endif ()

if (NOT ALTUN_EXPECTED_COMPONENTS)
	message(FATAL_ERROR "ALTUN_EXPECTED_COMPONENTS is required.")
endif ()

string(REPLACE "," ";" altun_components
	"${ALTUN_EXPECTED_COMPONENTS}"
)
find_package(Altun ${ALTUN_EXPECTED_VERSION} EXACT CONFIG REQUIRED
	COMPONENTS ${altun_components}
)
enable_testing()

set(altun_source_targets)
set(altun_expected_definitions)

if (WIN32 AND ALTUN_RUNTIME_DIR)
	file(GLOB altun_runtime_dlls "${ALTUN_RUNTIME_DIR}/*.dll")
	string(REPLACE "|" ";" dependency_runtime_directories
		"${ALTUN_DEPENDENCY_RUNTIME_DIRS}"
	)
	foreach(runtime_directory IN LISTS dependency_runtime_directories)
		if (IS_DIRECTORY "${runtime_directory}")
			file(GLOB dependency_runtime_dlls "${runtime_directory}/*.dll")
			list(APPEND altun_runtime_dlls ${dependency_runtime_dlls})
		endif ()
	endforeach()

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
			execute_process (
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
				list(APPEND altun_runtime_dlls "${runtime_path}")
			endif ()
		endforeach()
	endif ()

	list(REMOVE_DUPLICATES altun_runtime_dlls)
endif ()


function(add_altun_consumer_runtime_test target)
	if (WIN32)
		set_target_properties(${target} PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY
				"${CMAKE_CURRENT_BINARY_DIR}/runtime/$<CONFIG>"
		)
	endif ()

	add_test(NAME ${target} COMMAND $<TARGET_FILE:${target}>)
	list(APPEND altun_consumer_runtime_test_targets ${target})

	set(altun_consumer_runtime_test_targets
		${altun_consumer_runtime_test_targets} PARENT_SCOPE
	)
	# The install tree is intentionally outside the system loader's configured
	# prefixes. Make co-installed libraries and external shared dependencies
	# discoverable without changing their production RPATH policy. DT_RUNPATH
	# is not transitive, so the executable's runpath alone cannot resolve a
	# dependency of an Altun shared library from a separate Riwo prefix.
	if (UNIX AND ALTUN_LIBRARY_DIR)
		if (APPLE)
			set(loader_path_variable DYLD_LIBRARY_PATH)
		else ()
			set(loader_path_variable LD_LIBRARY_PATH)
		endif ()

		string(REPLACE "|" ";" dependency_runtime_directories
			"${ALTUN_DEPENDENCY_RUNTIME_DIRS}"
		)
		set(runtime_directories
			"${ALTUN_LIBRARY_DIR}"
			${dependency_runtime_directories}
		)
		if (NOT "$ENV{${loader_path_variable}}" STREQUAL "")
			list(APPEND runtime_directories
				"$ENV{${loader_path_variable}}"
			)
		endif ()

		list(FILTER runtime_directories EXCLUDE REGEX "^$")
		list(REMOVE_DUPLICATES runtime_directories)
		list(JOIN runtime_directories ":" loader_path)

		set_property(TEST ${target} PROPERTY ENVIRONMENT
			"${loader_path_variable}=${loader_path}"
		)
	endif ()
endfunction()


foreach(component IN LISTS altun_components)
	if (NOT Altun_${component}_FOUND)
		message(FATAL_ERROR "Component '${component}' was not reported as found.")
	endif ()

	if (NOT TARGET altun::${component})
		message(FATAL_ERROR "Missing imported target altun::${component}.")
	endif ()

	if (NOT TARGET altun.${component})
		message(FATAL_ERROR "Missing compatibility target altun.${component}.")
	endif ()

	string(TOUPPER "${component}" component_upper)
	set(target altun_consumer_${component})

	add_executable(${target} main.cpp)
	target_compile_features(${target} PRIVATE cxx_std_20)

	target_compile_definitions(${target} PRIVATE
		"ALTUN_CONSUME_${component_upper}=1"
	)
	target_link_libraries(${target} PRIVATE altun::${component})
	add_altun_consumer_runtime_test(${target})

	list(APPEND altun_source_targets altun.${component})
	list(APPEND altun_expected_definitions
		"ALTUN_CONSUME_${component_upper}=1"
	)
endforeach()

add_executable(altun_consumer_source_targets main.cpp)
target_compile_features(altun_consumer_source_targets PRIVATE cxx_std_20)

target_compile_definitions(altun_consumer_source_targets PRIVATE
	${altun_expected_definitions}
	ALTUN_CONSUME_UMBRELLA=1
)
target_link_libraries(altun_consumer_source_targets PRIVATE
	${altun_source_targets}
)
add_altun_consumer_runtime_test(altun_consumer_source_targets)

# CMake 3.15 predates TARGET_RUNTIME_DLLS and CTest's
# ENVIRONMENT_MODIFICATION property.  Prepare the shared runtime once instead
# of attaching identical POST_BUILD copies to every consumer: Ninja may link
# consumers concurrently, and parallel copies of the same DLL can race.
if (WIN32 AND altun_runtime_dlls)
	set(altun_consumer_runtime_output_directory
		"${CMAKE_CURRENT_BINARY_DIR}/runtime/$<CONFIG>"
	)
	add_custom_target(altun_consumer_runtime ALL
		COMMAND ${CMAKE_COMMAND} -E make_directory
			"${altun_consumer_runtime_output_directory}"

		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${altun_runtime_dlls}
			"${altun_consumer_runtime_output_directory}"

		COMMAND_EXPAND_LISTS
	)
	foreach(target IN LISTS altun_consumer_runtime_test_targets)
		add_dependencies(${target} altun_consumer_runtime)
	endforeach()
endif ()
