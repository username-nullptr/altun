# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.15)

set(MAJOR_VERSION 0)
set(MINOR_VERSION 7)
set(PATCH_VERSION 0)

set(PRO_VERSION ${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION})
set(PRO_NAME libEMpp)

project(${PRO_NAME} VERSION ${PRO_VERSION} LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_definitions (
	-DLIBEMPP_MAJOR_VERSION=${MAJOR_VERSION}
	-DLIBEMPP_MINOR_VERSION=${MINOR_VERSION}
	-DLIBEMPP_PATCH_VERSION=${PATCH_VERSION}
	-DLIBEMPP_VERSION_STR="${PRO_VERSION}-dev"
)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(platform)
include(heavy_compile)

include(options)
include(dependencies)

add_subdirectory(3rd_party)
include(3rd_party)

include(test)
include(CTest)

if (CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
	add_definitions(-DLIBEMPP_DEBUG)
endif ()

include_directories(.)
add_subdirectory(libempp)

# Generate the installed package only after every provider and exported target
# has been resolved.  External providers choose their concrete imported target
# names while configuring 3rd_party, so generating this file earlier leaves
# empty target names in libEMppConfig.cmake.
include(provider)

if (BUILD_TESTING)
	add_subdirectory(test)
endif ()

if (LIBEMPP_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif ()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES libempp.h)
install(FILES libempp.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(DIRECTORY libempp/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libempp/
	FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
