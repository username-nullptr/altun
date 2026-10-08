# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT


function(altun_add_library target_name)

	file(GLOB_RECURSE ${target_name}_sources "*.cpp" "*.c" "*.ixx")
	file(GLOB_RECURSE ${target_name}_headers "*.hpp" "*.h" "*.ipp")

	set(all_files
		${${target_name}_sources}
		${${target_name}_headers}
	)
	source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${all_files})

	if (ALTUN_BUILD_STATIC)
		add_library(${target_name} STATIC ${all_files})
	else ()
		add_library(${target_name} SHARED ${all_files})

		if (ALTUN_ADD_LIBRARY_VERSION)
			set_target_properties(${target_name} PROPERTIES
				VERSION ${PRO_VERSION} SOVERSION ${MAJOR_VERSION}
			)
		endif ()
	endif ()

	string(REPLACE "." "_" target_micro "${target_name}")

	target_compile_definitions(${target_name} PRIVATE ${target_micro}_EXPORTS)
	target_compile_features(${target_name} PUBLIC cxx_std_20)

	# Public headers require the conforming MSVC preprocessor. Consumers also
	# need the same C++ standard-library ABI selected for this build.
	target_compile_options(${target_name} PUBLIC
		"$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Zc:preprocessor>"
	)
	if (ALTUN_USE_LIBCXX)
		target_compile_options(${target_name} PUBLIC
			"$<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>"
		)
		target_link_options(${target_name} PUBLIC -stdlib=libc++)
	endif ()

	target_include_directories(${target_name} PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
		$<BUILD_INTERFACE:${ALTUN_CONFIG_INCLUDE}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	)
	target_include_directories(${target_name} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
	)
	if (NOT ${ARGN} STREQUAL "")
		target_link_libraries(${target_name} PUBLIC ${ARGN})
	endif ()

	set_target_properties(${target_name} PROPERTIES
		LIBRARY_OUTPUT_DIRECTORY ${ALTUN_OUTPUT_DIR}/bin
		RUNTIME_OUTPUT_DIRECTORY ${ALTUN_OUTPUT_DIR}/bin
		ARCHIVE_OUTPUT_DIRECTORY ${ALTUN_OUTPUT_DIR}/lib
	)
	string(REGEX REPLACE "^altun\\." "" target_export_name "${target_name}")

	set_target_properties(${target_name} PROPERTIES
		EXPORT_NAME ${target_export_name}
	)
	add_library(altun::${target_export_name} ALIAS ${target_name})

	install(TARGETS ${target_name}
		EXPORT altunTargets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	)
endfunction ()


function(altun_add_executable target_name)

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
		LIBRARY_OUTPUT_DIRECTORY ${ALTUN_OUTPUT_DIR}/bin
		RUNTIME_OUTPUT_DIRECTORY ${ALTUN_OUTPUT_DIR}/bin
		ARCHIVE_OUTPUT_DIRECTORY ${ALTUN_OUTPUT_DIR}/lib
	)
	install(TARGETS ${target_name} DESTINATION ${CMAKE_INSTALL_BINDIR})

endfunction ()
