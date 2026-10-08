# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

set(target_name empp.core)
libempp_add_library(${target_name})

if (LIBEMPP_SBUS_INTERFACE STREQUAL "udp")
	get_target_property(libempp_riwo_utils_definitions
		Riwo::utils INTERFACE_COMPILE_DEFINITIONS
	)
	if (NOT libempp_riwo_utils_definitions MATCHES
		"(^|;)RIWO_UTILS_SBUS_UDP_INTERFACE_SUPPORT=1(;|$)")
		message(FATAL_ERROR
			"${PRO_NAME}: LIBEMPP_SBUS_INTERFACE=udp requires a Riwo "
			"Utilities library built with RIWO_BUILD_UTILITIES_SBUS_UDP=ON."
		)
	endif ()
endif ()

target_link_libraries(${target_name} PUBLIC
	Riwo::coro Riwo::utils empp.nlohmann
)
if (LIBEMPP_BUILD_SBUS_CYCLONE)
	find_package(CycloneDDS REQUIRED)
	idlc_generate (
		TARGET cyclone_message
		FILES ${CMAKE_CURRENT_SOURCE_DIR}/sbus/detail/cyclone_message.idl
	)
	target_link_libraries (
		${target_name} PRIVATE
		CycloneDDS::ddsc
		$<BUILD_INTERFACE:cyclone_message>
	)
	target_compile_definitions(${target_name} PUBLIC
		LIBEMPP_SBUS_CYCLONE_SUPPORT=1
	)
endif ()

if (LIBEMPP_BUILD_SBUS_DBUS)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(LIBEMPP_DBUS REQUIRED IMPORTED_TARGET dbus-1)

	target_link_libraries(${target_name} PRIVATE
		PkgConfig::LIBEMPP_DBUS
	)
	target_compile_definitions(${target_name} PUBLIC
		LIBEMPP_SBUS_DBUS_SUPPORT=1
	)
endif ()

if (LIBEMPP_BUILD_SBUS_SHM)
	target_compile_definitions(${target_name} PUBLIC
		LIBEMPP_SBUS_SHM_SUPPORT=1
	)
	if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
		target_link_libraries(${target_name} PRIVATE rt)
	endif ()
endif ()
