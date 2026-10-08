# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

option(ALTUN_BUILD_STATIC
	"-- ${PRO_NAME}: Build static libraries." ${altun_build_static_default}
)
if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
	NOT altun_gnu_shared_runtime_available AND NOT ALTUN_BUILD_STATIC)
	message(FATAL_ERROR
		"${PRO_NAME}: Shared libraries require a shared GNU C++ runtime on Windows. "
		"Use -DALTUN_BUILD_STATIC=ON or a MinGW toolchain that provides libstdc++-6.dll."
	)
endif ()

option(ALTUN_ADD_LIBRARY_VERSION
	"-- ${PRO_NAME}: Add version information to shared library names." ON
)
option(ALTUN_BUILD_SBUS_CYCLONE
	"-- ${PRO_NAME}: Build the CycloneDDS SBus transport." OFF
)
set(ALTUN_SBUS_CYCLONE_SUPPORT ${ALTUN_BUILD_SBUS_CYCLONE})

if (ALTUN_BUILD_SBUS_CYCLONE)
	message(STATUS "${PRO_NAME}: Build Riwo.Utilities.SoftBus interface: Cyclone-DDS")
endif ()

option(ALTUN_BUILD_SBUS_DBUS
	"-- ${PRO_NAME}: Build the D-Bus SBus transport." OFF
)
set(ALTUN_SBUS_DBUS_SUPPORT ${ALTUN_BUILD_SBUS_DBUS})

if (ALTUN_BUILD_SBUS_DBUS)
	message(STATUS "${PRO_NAME}: Build Riwo.Utilities.SoftBus interface: D-Bus")
endif ()

option(ALTUN_BUILD_SBUS_SHM
	"-- ${PRO_NAME}: Build the POSIX shared-memory SBus transport." OFF
)
if (ALTUN_BUILD_SBUS_SHM AND NOT UNIX)
	message(FATAL_ERROR
		"${PRO_NAME}: The shared-memory SBus transport currently requires POSIX."
	)
endif ()

set(ALTUN_SBUS_SHM_SUPPORT ${ALTUN_BUILD_SBUS_SHM})

if (ALTUN_BUILD_SBUS_SHM)
	message(STATUS "${PRO_NAME}: Build Riwo.Utilities.SoftBus interface: shared memory")
endif ()

set(ALTUN_SBUS_INTERFACE "default" CACHE STRING
	"Select the Riwo.Utilities.SoftBus interface. (Default: riwo default)"
)
string(TOLOWER "${ALTUN_SBUS_INTERFACE}" ALTUN_SBUS_INTERFACE_NORMALIZED)

set(ALTUN_SBUS_INTERFACE "${ALTUN_SBUS_INTERFACE_NORMALIZED}" CACHE STRING
	"Select the Riwo.Utilities.SoftBus interface. (Default: riwo default)" FORCE
)
set_property(CACHE ALTUN_SBUS_INTERFACE PROPERTY STRINGS
	default local udp dbus cyclone shm
)

if ((ALTUN_SBUS_INTERFACE STREQUAL "cyclone" AND NOT ALTUN_BUILD_SBUS_CYCLONE) OR
	(ALTUN_SBUS_INTERFACE STREQUAL "dbus" AND NOT ALTUN_BUILD_SBUS_DBUS) OR
	(ALTUN_SBUS_INTERFACE STREQUAL "shm" AND NOT ALTUN_BUILD_SBUS_SHM))
	message(FATAL_ERROR "${PRO_NAME}: Unsupported Riwo.Utilities.SoftBus interface: ${ALTUN_SBUS_INTERFACE}")
endif ()

if (NOT ALTUN_SBUS_INTERFACE MATCHES "^(default|local|udp|dbus|cyclone|shm)$")
	message(FATAL_ERROR
		"${PRO_NAME}: Unknown Riwo.Utilities.SoftBus interface: ${ALTUN_SBUS_INTERFACE}"
	)
endif ()

set(ALTUN_SBUS_INTERFACE_DBUS 0)
set(ALTUN_SBUS_INTERFACE_CYCLONE 0)
set(ALTUN_SBUS_INTERFACE_SHM 0)

set(ALTUN_SBUS_INTERFACE_LOCAL 0)
set(ALTUN_SBUS_INTERFACE_UDP 0)
set(ALTUN_SBUS_INTERFACE_DEFAULT 0)

if (ALTUN_SBUS_INTERFACE STREQUAL "dbus")
	set(ALTUN_SBUS_INTERFACE_DBUS 1)
elseif (ALTUN_SBUS_INTERFACE STREQUAL "cyclone")
	set(ALTUN_SBUS_INTERFACE_CYCLONE 1)
elseif (ALTUN_SBUS_INTERFACE STREQUAL "shm")
	set(ALTUN_SBUS_INTERFACE_SHM 1)
elseif (ALTUN_SBUS_INTERFACE STREQUAL "local")
	set(ALTUN_SBUS_INTERFACE_LOCAL 1)
elseif (ALTUN_SBUS_INTERFACE STREQUAL "udp")
	set(ALTUN_SBUS_INTERFACE_UDP 1)
else ()
	set(ALTUN_SBUS_INTERFACE_DEFAULT 1)
endif ()

option(ALTUN_BUILD_EXAMPLES
	"-- ${PRO_NAME}: Enable this to build the examples." OFF
)
if (ALTUN_BUILD_EXAMPLES)
	message(STATUS "${PRO_NAME}: Enable this to build the examples.")
endif ()

set(ALTUN_CONFIG_INCLUDE
	${ALTUN_OUTPUT_DIR}/config_include
)
set(ALTUN_CONFIG_INCLUDE
	${ALTUN_CONFIG_INCLUDE} CACHE PATH
	"Path to the altun generated config include directory."
)
configure_file (
	${PROJECT_SOURCE_DIR}/altun/core/cxx/configs.h.in
	${ALTUN_CONFIG_INCLUDE}/altun/core/cxx/configs.h
	@ONLY
)
install(FILES
	${ALTUN_CONFIG_INCLUDE}/altun/core/cxx/configs.h
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/altun/core/cxx
)
