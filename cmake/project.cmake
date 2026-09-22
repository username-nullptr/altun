# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

if (WIN32)
	set(OS_CPP win)
	set(IS_CPP winnt)
elseif (UNIX)
	if (APPLE)
		set(OS_CPP apple)
	else ()
		set(OS_CPP unix)
	endif ()
	set(IS_CPP posix)
endif()

option(LIBEMPP_BUILD_STATIC
	"-- ${PRO_NAME}: Build static libraries." OFF
)
if (NOT LIBEMPP_BUILD_STATIC)
	option(LIBEMPP_ADD_LIBRARY_VERSION
		"-- ${PRO_NAME}: Add version information to library names." ON
	)
endif ()


function(libempp_add_library target_name)

	file(GLOB_RECURSE ${target_name}_sources "*.cpp" "*.c" "*.ixx")
	file(GLOB_RECURSE ${target_name}_headers "*.hpp" "*.h" "*.ipp")

	set(all_files
		${${target_name}_sources}
		${${target_name}_headers}
	)
	source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${all_files})

	if (LIBEMPP_BUILD_STATIC)
		add_library(${target_name} STATIC ${all_files})
	else ()
		add_library(${target_name} SHARED ${all_files})
		if (LIBEMPP_ADD_LIBRARY_VERSION)
			set_target_properties(${target_name} PROPERTIES
				VERSION ${PRO_VERSION} SOVERSION ${MAJOR_VERSION}
			)
		endif ()
	endif ()

	string(REPLACE "." "_" target_micro "${target_name}")
	target_compile_definitions(${target_name} PRIVATE ${target_micro}_EXPORTS)
	target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

	if (NOT ${ARGN} STREQUAL "")
		target_link_libraries(${target_name} PUBLIC ${ARGN})
	endif ()

	set_target_properties(${target_name} PROPERTIES
		LIBRARY_OUTPUT_DIRECTORY ${LIBEMPP_OUTPUT_DIR}/bin
		RUNTIME_OUTPUT_DIRECTORY ${LIBEMPP_OUTPUT_DIR}/bin
		ARCHIVE_OUTPUT_DIRECTORY ${LIBEMPP_OUTPUT_DIR}/lib
	)
	if (NOT LIBEMPP_BUILD_STATIC)
		install(TARGETS ${target_name} DESTINATION ${install_dir}
			PERMISSIONS
			OWNER_READ OWNER_WRITE OWNER_EXECUTE
			GROUP_READ GROUP_EXECUTE
			WORLD_READ WORLD_EXECUTE
		)
	endif ()

endfunction ()


function(libempp_add_executable target_name)

	file(GLOB_RECURSE ${target_name}_sources "*.cpp" "*.c" "*.ixx")
	file(GLOB_RECURSE ${target_name}_headers "*.hpp" "*.h" "*.ipp")

	set(all_files
		${${target_name}_sources}
		${${target_name}_headers}
	)
	source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${all_files})
	add_executable(${target_name} ${all_files})

	target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

	if (NOT ${ARGN} STREQUAL "")
		target_link_libraries(${target_name} PUBLIC ${ARGN})
	endif ()

	set_target_properties(${target_name} PROPERTIES
		LIBRARY_OUTPUT_DIRECTORY ${LIBEMPP_OUTPUT_DIR}/bin
		RUNTIME_OUTPUT_DIRECTORY ${LIBEMPP_OUTPUT_DIR}/bin
		ARCHIVE_OUTPUT_DIRECTORY ${LIBEMPP_OUTPUT_DIR}/lib
	)
	install(TARGETS ${target_name} DESTINATION bin)

endfunction ()