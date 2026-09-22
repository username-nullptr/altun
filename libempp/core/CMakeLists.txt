# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

set(target_name empp.core)
libempp_add_library(${target_name})

if (LIBEMPP_SBUS_INTERFACE STREQUAL "udp" AND
	NOT LIBGS_BUILD_UTILITIES_SBUS_UDP)
	message(FATAL_ERROR
		"${PRO_NAME}: The UDP SBus interface requires LIBGS_BUILD_UTILITIES_SBUS_UDP=ON."
	)
endif ()

target_link_libraries(${target_name} PUBLIC
	gs.coro gs.utils
)
target_include_directories(${target_name} PUBLIC)

if (LIBEMPP_BUILD_SBUS_CYCLONE)
	find_package(CycloneDDS REQUIRED)
	idlc_generate (
		TARGET cyclone_message
		FILES ${CMAKE_CURRENT_SOURCE_DIR}/sbus/detail/cyclone_message.idl
	)
	target_link_libraries (
		${target_name} PRIVATE
		CycloneDDS::ddsc
		cyclone_message
	)
	target_compile_definitions(${target_name} PUBLIC
		-DLIBEMPP_SBUS_CYCLONE_SUPPORT=1
	)
endif ()

if (LIBEMPP_BUILD_SBUS_DBUS)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(LIBEMPP_DBUS REQUIRED IMPORTED_TARGET dbus-1)

	target_link_libraries(${target_name} PRIVATE
		PkgConfig::LIBEMPP_DBUS
	)
	target_compile_definitions(${target_name} PUBLIC
		-DLIBEMPP_SBUS_DBUS_SUPPORT=1
	)
endif ()

if (LIBEMPP_BUILD_SBUS_SHM)
	target_compile_definitions(${target_name} PUBLIC
		-DLIBEMPP_SBUS_SHM_SUPPORT=1
	)
	if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
		target_link_libraries(${target_name} PRIVATE rt)
	endif ()
endif ()

set(LIBEMPP_CORE_CONFIG_INCLUDE
	${LIBEMPP_OUTPUT_DIR}/config_include
)
set(LIBEMPP_CORE_CONFIG_INCLUDE
	${LIBEMPP_CORE_CONFIG_INCLUDE} CACHE PATH
	"Path to libempp config include directory."
)
configure_file (
	${PROJECT_SOURCE_DIR}/libempp/core/cxx/configs.h.in
	${LIBEMPP_CORE_CONFIG_INCLUDE}/libempp/core/cxx/configs.h
	@ONLY
)
include_directories(${LIBEMPP_CORE_CONFIG_INCLUDE})

install(FILES
	${LIBEMPP_CORE_CONFIG_INCLUDE}/libempp/core/cxx/configs.h
	DESTINATION include/libempp/core/cxx
)
