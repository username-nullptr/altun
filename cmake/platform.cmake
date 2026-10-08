# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

# Accept the historical unprefixed cache variables while exposing the same
# project-scoped option names used by Riwo.
if (DEFINED USE_LIBCXX AND NOT DEFINED LIBEMPP_USE_LIBCXX)
	set(LIBEMPP_USE_LIBCXX "${USE_LIBCXX}")
endif ()

if (DEFINED USE_LLD AND NOT DEFINED LIBEMPP_USE_LLD)
	set(LIBEMPP_USE_LLD "${USE_LLD}")
endif ()

if (DEFINED ENABLE_LTO AND NOT DEFINED LIBEMPP_ENABLE_LTO)
	set(LIBEMPP_ENABLE_LTO "${ENABLE_LTO}")
endif ()

option(LIBEMPP_USE_LIBCXX
	"-- ${PRO_NAME}: Use clang libc++." OFF
)
option(LIBEMPP_USE_LLD
	"-- ${PRO_NAME}: Use clang lld." OFF
)
option(LIBEMPP_ENABLE_LTO
	"-- ${PRO_NAME}: Use GNU LTO." OFF
)
if (LIBEMPP_USE_LIBCXX AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	message(FATAL_ERROR
		"${PRO_NAME}: LIBEMPP_USE_LIBCXX requires the Clang compiler."
	)
endif ()

if (LIBEMPP_USE_LLD AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	message(FATAL_ERROR
		"${PRO_NAME}: LIBEMPP_USE_LLD requires the Clang compiler."
	)
endif ()

if (LIBEMPP_ENABLE_LTO AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	message(FATAL_ERROR
		"${PRO_NAME}: LIBEMPP_ENABLE_LTO requires the GNU compiler."
	)
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	if (CMAKE_CXX_COMPILER_VERSION LESS 17)
		message(FATAL_ERROR "The minimum version of 'Clang' required is 17.")
	endif ()
	add_compile_options(-Wall)

	if (LIBEMPP_USE_LIBCXX OR LIBEMPP_USE_LLD)
		include(CheckCXXSourceCompiles)

		set(libempp_saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
		set(libempp_saved_required_link_options "${CMAKE_REQUIRED_LINK_OPTIONS}")

		set(libempp_clang_required_flags)
		set(libempp_clang_required_link_options)

		if (LIBEMPP_USE_LIBCXX)
			list(APPEND libempp_clang_required_flags -stdlib=libc++)
			list(APPEND libempp_clang_required_link_options -stdlib=libc++)
		endif ()

		if (LIBEMPP_USE_LLD)
			list(APPEND libempp_clang_required_link_options -fuse-ld=lld)
		endif ()

		string(JOIN " " libempp_clang_required_flags_string
			${libempp_clang_required_flags}
		)
		set(CMAKE_REQUIRED_FLAGS
			"${libempp_saved_required_flags} ${libempp_clang_required_flags_string}"
		)
		set(CMAKE_REQUIRED_LINK_OPTIONS
			${libempp_saved_required_link_options}
			${libempp_clang_required_link_options}
		)
		unset(LIBEMPP_CLANG_TOOLCHAIN_OPTIONS_AVAILABLE CACHE)

		check_cxx_source_compiles (
			"#include <string>\nint main() { std::string value; return value.size(); }"
			LIBEMPP_CLANG_TOOLCHAIN_OPTIONS_AVAILABLE
		)
		set(CMAKE_REQUIRED_FLAGS "${libempp_saved_required_flags}")
		set(CMAKE_REQUIRED_LINK_OPTIONS ${libempp_saved_required_link_options})

		if (NOT LIBEMPP_CLANG_TOOLCHAIN_OPTIONS_AVAILABLE)
			message(FATAL_ERROR
				"${PRO_NAME}: Requested Clang runtime/linker options are unavailable."
			)
		endif ()
	endif ()

	if (LIBEMPP_USE_LIBCXX)
		message(STATUS "${PRO_NAME}: Use clang libc++.")
		add_compile_options(-stdlib=libc++)
		add_link_options(-stdlib=libc++)
	endif ()

	if (LIBEMPP_USE_LLD)
		message(STATUS "${PRO_NAME}: Use clang lld.")
		add_link_options(-fuse-ld=lld)
	endif ()

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	if (CMAKE_CXX_COMPILER_VERSION LESS 13)
		message(FATAL_ERROR "The minimum version of 'GNU' required is 13.")
	endif ()
	add_compile_options(-Wall)

	if (LIBEMPP_ENABLE_LTO)
		include(CheckIPOSupported)
		check_ipo_supported(RESULT libempp_lto_available
			OUTPUT libempp_lto_error LANGUAGES CXX
		)
		if (NOT libempp_lto_available)
			message(FATAL_ERROR
				"${PRO_NAME}: GNU LTO is unavailable: ${libempp_lto_error}"
			)
		endif ()

		message(STATUS "${PRO_NAME}: Use GNU LTO.")
		add_compile_options(-flto)
		add_link_options(-flto)
	endif ()

elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	if (MSVC_VERSION LESS 1930)
		message(FATAL_ERROR "The minimum version of 'MSVC' required is 1930 (VS2022).")
	endif ()

	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0A00)
	add_compile_options(/W4 /wd4819 /Zc:preprocessor /bigobj)

else ()
	message(STATUS
		"Unknown compiler: ${CMAKE_CXX_COMPILER_ID} (${CMAKE_CXX_COMPILER_VERSION})."
	)
endif ()

set(libempp_build_static_default OFF)
set(libempp_gnu_shared_runtime_available TRUE)

if (WIN32)
	set(OS_CPP win)
	set(IS_CPP winnt)
	set(install_dir bin)

	if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		execute_process (
			COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libstdc++-6.dll
			OUTPUT_VARIABLE libempp_gnu_libstdcxx_dll
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
		)
		if (NOT IS_ABSOLUTE "${libempp_gnu_libstdcxx_dll}" OR
			NOT EXISTS "${libempp_gnu_libstdcxx_dll}")
			get_filename_component(libempp_gnu_compiler_dir
				"${CMAKE_CXX_COMPILER}" DIRECTORY
			)
			set(libempp_gnu_libstdcxx_dll
				"${libempp_gnu_compiler_dir}/libstdc++-6.dll"
			)
		endif ()

		if (IS_ABSOLUTE "${libempp_gnu_libstdcxx_dll}" AND
			EXISTS "${libempp_gnu_libstdcxx_dll}")
			get_filename_component(libempp_gnu_runtime_dir
				"${libempp_gnu_libstdcxx_dll}" DIRECTORY
			)
		endif ()

		if (NOT IS_ABSOLUTE "${libempp_gnu_libstdcxx_dll}" OR
			NOT EXISTS "${libempp_gnu_libstdcxx_dll}")
			set(libempp_gnu_shared_runtime_available FALSE)
			set(libempp_build_static_default ON)
			message(STATUS "${PRO_NAME}: GNU C++ runtime is static-only.")
		endif ()
	endif ()

else ()
	if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		add_link_options (
			"$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:-rdynamic>"
		)
	endif ()

	if (APPLE)
		set(OS_CPP apple)
	elseif (ANDROID)
		set(OS_CPP android)
	else ()
		set(OS_CPP unix)
	endif ()

	set(IS_CPP posix)
	set(install_dir lib)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(LIBEMPP_OUTPUT_DIR ${CMAKE_BINARY_DIR}/output)

message(STATUS "")
message(STATUS "${PRO_NAME}: Using C++: ${CMAKE_CXX_STANDARD}")
message(STATUS "${PRO_NAME}: Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "${PRO_NAME}: Install prefix: ${CMAKE_INSTALL_PREFIX}")
