# SPDX-FileCopyrightText: 2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

set(libempp_cmake_test_work_dir "${CMAKE_CURRENT_BINARY_DIR}/work")
set(libempp_consumer_components core)

if (LIBEMPP_BUILD_LINUX)
	list(APPEND libempp_consumer_components linux)
endif ()

string(JOIN "," libempp_consumer_components_arg
	${libempp_consumer_components}
)
set(libempp_missing_component linux)

if (LIBEMPP_BUILD_LINUX)
	set(libempp_missing_component unknown)
endif ()

if (CMAKE_CONFIGURATION_TYPES)
	set(libempp_consumer_configuration "$<CONFIG>")
else ()
	set(libempp_consumer_configuration "${CMAKE_BUILD_TYPE}")
endif ()

# Exercise the installed package from an independent build tree. This catches
# missing headers, incomplete exports, broken transitive dependencies, and
# source-tree-only target assumptions that an in-tree test cannot detect.
add_libempp_ctest(empp.cmake.install-consumer
	${CMAKE_COMMAND}
	"-DTEST_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/consumer"
	"-DTEST_BINARY_DIR=${libempp_cmake_test_work_dir}/consumer-build"
	"-DTEST_MISSING_BINARY_DIR=${libempp_cmake_test_work_dir}/consumer-missing-build"
	"-DTEST_INSTALL_PREFIX=${libempp_cmake_test_work_dir}/install"
	"-DTEST_INSTALL_CMAKEDIR=${LIBEMPP_INSTALL_CMAKEDIR}"
	"-DTEST_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
	"-DLIBEMPP_BINARY_DIR=${PROJECT_BINARY_DIR}"
	"-DTEST_GENERATOR=${CMAKE_GENERATOR}"
	"-DTEST_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
	"-DTEST_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
	"-DTEST_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
	"-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
	"-DTEST_RC_COMPILER=${CMAKE_RC_COMPILER}"
	"-DTEST_MT=${CMAKE_MT}"
	"-DTEST_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
	"-DTEST_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
	"-DTEST_CONFIGURATION=${libempp_consumer_configuration}"
	"-DTEST_VERSION=${PROJECT_VERSION}"
	"-DTEST_COMPONENTS=${libempp_consumer_components_arg}"
	"-DTEST_MISSING_COMPONENT=${libempp_missing_component}"
	"-DTEST_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
	-P "${CMAKE_CURRENT_SOURCE_DIR}/run_install_consumer.cmake"
)

set_tests_properties(empp.cmake.install-consumer PROPERTIES
	LABELS "cmake;install;consumer"
	RUN_SERIAL TRUE
	TIMEOUT 300
)
