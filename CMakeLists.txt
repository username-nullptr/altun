# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)

set(MAJOR_VERSION 0)
set(MINOR_VERSION 6)
set(PATCH_VERSION 1)

set(PRO_VERSION ${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION})
set(PRO_NAME libEMpp)

project(${PRO_NAME} VERSION ${PRO_VERSION} LANGUAGES C CXX)
add_definitions(-DLIBEMPP_VERSION_STR="${PRO_VERSION}-dev")

# CMake include path.
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Build output path.
set(LIBEMPP_OUTPUT_DIR ${CMAKE_BINARY_DIR}/output)

include(platform)  # Check compiler.
include(heavy_compile)
include(option)
include(test)
include(CTest)

message(STATUS "")
message(STATUS "${PRO_NAME}: using C: " ${CMAKE_C_STANDARD})
message(STATUS "${PRO_NAME}: using C++: " ${CMAKE_CXX_STANDARD})
message(STATUS "${PRO_NAME}: build type: " ${CMAKE_BUILD_TYPE})
message(STATUS "${PRO_NAME}: install prefix: " ${CMAKE_INSTALL_PREFIX})

set(LIBEMPP_OUTPUT_DIR ${CMAKE_BINARY_DIR}/output)

# Add 3rd-libraries
add_subdirectory(3rd_party)
include(3rd_party)

if (CMAKE_BUILD_TYPE AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
	add_link_options("$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:-rdynamic>")
	add_definitions(-D${PRO_NAME}_DEBUG)
endif()

include_directories(.)
add_subdirectory(libempp)

if (BUILD_TESTING)
	add_subdirectory(test)
endif ()

option(LIBEMPP_BUILD_EXAMPLES
	"-- ${PRO_NAME}: Enable this to build the examples." OFF
)
if (LIBEMPP_BUILD_EXAMPLES)
	message(STATUS "${PRO_NAME}: Enable this to build the examples.")
	add_subdirectory(examples)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES libempp.h)
install(FILES libempp.h DESTINATION include/)

install(DIRECTORY libempp/ DESTINATION include/libempp/
	FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
