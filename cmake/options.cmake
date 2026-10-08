# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

option(LIBEMPP_BUILD_SBUS_CYCLONE
	"-- ${PRO_NAME}: Build the CycloneDDS SBus transport." OFF
)
set(LIBEMPP_SBUS_CYCLONE_SUPPORT ${LIBEMPP_BUILD_SBUS_CYCLONE})
if (LIBEMPP_BUILD_SBUS_CYCLONE)
	message(STATUS "${PRO_NAME}: Build Riwo.Utilities.SoftBus interface: Cyclone-DDS")
endif ()

option(LIBEMPP_BUILD_SBUS_DBUS
	"-- ${PRO_NAME}: Build the D-Bus SBus transport." OFF
)
set(LIBEMPP_SBUS_DBUS_SUPPORT ${LIBEMPP_BUILD_SBUS_DBUS})
if (LIBEMPP_BUILD_SBUS_DBUS)
	message(STATUS "${PRO_NAME}: Build Riwo.Utilities.SoftBus interface: D-Bus")
endif ()

option(LIBEMPP_BUILD_SBUS_SHM
	"-- ${PRO_NAME}: Build the POSIX shared-memory SBus transport." OFF
)
if (LIBEMPP_BUILD_SBUS_SHM AND NOT UNIX)
	message(FATAL_ERROR
		"${PRO_NAME}: The shared-memory SBus transport currently requires POSIX."
	)
endif ()

set(LIBEMPP_SBUS_SHM_SUPPORT ${LIBEMPP_BUILD_SBUS_SHM})
if (LIBEMPP_BUILD_SBUS_SHM)
	message(STATUS "${PRO_NAME}: Build Riwo.Utilities.SoftBus interface: shared memory")
endif ()

set(LIBEMPP_SBUS_INTERFACE "default" CACHE STRING
	"Select the Riwo.Utilities.SoftBus interface. (Default: riwo default)"
)
string(TOLOWER "${LIBEMPP_SBUS_INTERFACE}" LIBEMPP_SBUS_INTERFACE_NORMALIZED)

set(LIBEMPP_SBUS_INTERFACE "${LIBEMPP_SBUS_INTERFACE_NORMALIZED}" CACHE STRING
	"Select the Riwo.Utilities.SoftBus interface. (Default: riwo default)" FORCE
)
set_property(CACHE LIBEMPP_SBUS_INTERFACE PROPERTY STRINGS
	default local udp dbus cyclone shm
)

if ((LIBEMPP_SBUS_INTERFACE STREQUAL "cyclone" AND NOT LIBEMPP_BUILD_SBUS_CYCLONE) OR
	(LIBEMPP_SBUS_INTERFACE STREQUAL "dbus" AND NOT LIBEMPP_BUILD_SBUS_DBUS) OR
	(LIBEMPP_SBUS_INTERFACE STREQUAL "shm" AND NOT LIBEMPP_BUILD_SBUS_SHM))
	message(FATAL_ERROR "${PRO_NAME}: Unsupported Riwo.Utilities.SoftBus interface: ${LIBEMPP_SBUS_INTERFACE}")
endif ()

if (NOT LIBEMPP_SBUS_INTERFACE MATCHES "^(default|local|udp|dbus|cyclone|shm)$")
	message(FATAL_ERROR
		"${PRO_NAME}: Unknown Riwo.Utilities.SoftBus interface: ${LIBEMPP_SBUS_INTERFACE}"
	)
endif ()

set(LIBEMPP_SBUS_INTERFACE_DBUS 0)
set(LIBEMPP_SBUS_INTERFACE_CYCLONE 0)
set(LIBEMPP_SBUS_INTERFACE_SHM 0)
set(LIBEMPP_SBUS_INTERFACE_LOCAL 0)
set(LIBEMPP_SBUS_INTERFACE_UDP 0)
set(LIBEMPP_SBUS_INTERFACE_DEFAULT 0)

if (LIBEMPP_SBUS_INTERFACE STREQUAL "dbus")
	set(LIBEMPP_SBUS_INTERFACE_DBUS 1)
elseif (LIBEMPP_SBUS_INTERFACE STREQUAL "cyclone")
	set(LIBEMPP_SBUS_INTERFACE_CYCLONE 1)
elseif (LIBEMPP_SBUS_INTERFACE STREQUAL "shm")
	set(LIBEMPP_SBUS_INTERFACE_SHM 1)
elseif (LIBEMPP_SBUS_INTERFACE STREQUAL "local")
	set(LIBEMPP_SBUS_INTERFACE_LOCAL 1)
elseif (LIBEMPP_SBUS_INTERFACE STREQUAL "udp")
	set(LIBEMPP_SBUS_INTERFACE_UDP 1)
else ()
	set(LIBEMPP_SBUS_INTERFACE_DEFAULT 1)
endif ()

option(LIBEMPP_BUILD_EXAMPLES
	"-- ${PRO_NAME}: Enable this to build the examples." OFF
)
if (LIBEMPP_BUILD_EXAMPLES)
	message(STATUS "${PRO_NAME}: Enable this to build the examples.")
endif()
