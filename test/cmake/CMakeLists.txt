# SPDX-FileCopyrightText: 2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

set(altun_cmake_test_work_dir "${CMAKE_CURRENT_BINARY_DIR}/work")
set(altun_available_components core)

if (ALTUN_BUILD_LINUX)
	list(APPEND altun_available_components linux)
endif ()

function(add_altun_configure_case name)
	cmake_parse_arguments(CASE "SUCCESS" "ERROR" "OPTIONS;COMPONENTS" ${ARGN})

	string(MD5 case_work_id "configure-${name}")
	string(SUBSTRING "${case_work_id}" 0 12 case_work_id)

	if (CASE_SUCCESS)
		set(case_expect_success ON)
	else ()
		set(case_expect_success OFF)
	endif ()

	string(JOIN "|" case_options ${CASE_OPTIONS})
	string(JOIN "," case_components ${CASE_COMPONENTS})

	add_test(NAME altun.cmake.configure.${name}
		COMMAND ${CMAKE_COMMAND}
			"-DTEST_NAME=${name}"
			"-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
			"-DTEST_BINARY_DIR=${altun_cmake_test_work_dir}/c-${case_work_id}"
			"-DTEST_GENERATOR=${CMAKE_GENERATOR}"
			"-DTEST_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
			"-DTEST_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
			"-DTEST_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
			"-DTEST_C_COMPILER=${CMAKE_C_COMPILER}"
			"-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
			"-DTEST_RC_COMPILER=${CMAKE_RC_COMPILER}"
			"-DTEST_MT=${CMAKE_MT}"
			"-DTEST_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
			"-DTEST_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
			"-DTEST_EXPECT_SUCCESS=${case_expect_success}"
			"-DTEST_EXPECT_ERROR=${CASE_ERROR}"
			"-DTEST_OPTIONS=${case_options}"
			"-DTEST_EXPECT_COMPONENTS=${case_components}"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/run_configure.cmake"
	)
	configure_altun_ctest_environment(altun.cmake.configure.${name})

	set_tests_properties(altun.cmake.configure.${name} PROPERTIES
		LABELS "cmake;configure"
		TIMEOUT 120
	)
endfunction()

function(add_altun_package_probe_case name)
	cmake_parse_arguments(CASE "FOUND" "COMPONENT;ERROR"
		"OPTIONS;COMPONENTS;PROBE_OPTIONS" ${ARGN}
	)

	string(MD5 case_work_id "package-${name}")
	string(SUBSTRING "${case_work_id}" 0 12 case_work_id)

	if (CASE_FOUND)
		set(case_expect_found ON)
	else ()
		set(case_expect_found OFF)
	endif ()

	string(JOIN "|" case_options ${CASE_OPTIONS})
	string(JOIN "," case_components ${CASE_COMPONENTS})
	string(JOIN "|" case_probe_options ${CASE_PROBE_OPTIONS})

	add_test(NAME altun.cmake.package.${name}
		COMMAND ${CMAKE_COMMAND}
			"-DTEST_NAME=${name}"
			"-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
			"-DTEST_BINARY_DIR=${altun_cmake_test_work_dir}/p-${case_work_id}"
			"-DTEST_PROBE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/package_probe"
			"-DTEST_GENERATOR=${CMAKE_GENERATOR}"
			"-DTEST_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
			"-DTEST_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
			"-DTEST_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
			"-DTEST_C_COMPILER=${CMAKE_C_COMPILER}"
			"-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
			"-DTEST_RC_COMPILER=${CMAKE_RC_COMPILER}"
			"-DTEST_MT=${CMAKE_MT}"
			"-DTEST_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
			"-DTEST_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
			-DTEST_EXPECT_SUCCESS=ON
			"-DTEST_OPTIONS=${case_options}"
			"-DTEST_EXPECT_COMPONENTS=${case_components}"
			"-DTEST_PROBE_COMPONENT=${CASE_COMPONENT}"
			"-DTEST_PROBE_EXPECT_FOUND=${case_expect_found}"
			"-DTEST_PROBE_EXPECT_ERROR=${CASE_ERROR}"
			"-DTEST_PROBE_OPTIONS=${case_probe_options}"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/run_package_probe.cmake"
	)
	configure_altun_ctest_environment(altun.cmake.package.${name})

	set_tests_properties(altun.cmake.package.${name} PROPERTIES
		LABELS "cmake;package"
		TIMEOUT 120
	)
endfunction()

# Validate every dependency-free SBus selector and the relationship between
# opt-in transports and their selected default.
foreach(interface default local udp)
	add_altun_configure_case(sbus-${interface} SUCCESS
		OPTIONS
			"ALTUN_SBUS_INTERFACE=${interface}"
			"ALTUN_BUILD_SBUS_CYCLONE=OFF"
			"ALTUN_BUILD_SBUS_DBUS=OFF"
			"ALTUN_BUILD_SBUS_SHM=OFF"
		COMPONENTS ${altun_available_components}
	)
endforeach()

add_altun_configure_case(sbus-cyclone-disabled
	ERROR "Unsupported Riwo.Utilities.SoftBus interface: cyclone"
	OPTIONS
		"ALTUN_SBUS_INTERFACE=cyclone"
		"ALTUN_BUILD_SBUS_CYCLONE=OFF"
)
if (ALTUN_BUILD_SBUS_CYCLONE)
	add_altun_configure_case(sbus-cyclone-enabled SUCCESS
		OPTIONS
			"ALTUN_SBUS_INTERFACE=cyclone"
			"ALTUN_BUILD_SBUS_CYCLONE=ON"
		COMPONENTS ${altun_available_components}
	)
endif ()

add_altun_configure_case(sbus-dbus-disabled
	ERROR "Unsupported Riwo.Utilities.SoftBus interface: dbus"
	OPTIONS
		"ALTUN_SBUS_INTERFACE=dbus"
		"ALTUN_BUILD_SBUS_DBUS=OFF"
)
if (UNIX)
	add_altun_configure_case(sbus-shm-disabled
		ERROR "Unsupported Riwo.Utilities.SoftBus interface: shm"
		OPTIONS
			"ALTUN_SBUS_INTERFACE=shm"
			"ALTUN_BUILD_SBUS_SHM=OFF"
	)
	add_altun_configure_case(sbus-shm-enabled SUCCESS
		OPTIONS
			"ALTUN_SBUS_INTERFACE=shm"
			"ALTUN_BUILD_SBUS_SHM=ON"
		COMPONENTS ${altun_available_components}
	)
else ()
	add_altun_configure_case(sbus-shm-requires-posix
		ERROR "shared-memory SBus transport currently requires POSIX"
		OPTIONS "ALTUN_BUILD_SBUS_SHM=ON"
	)
endif ()

add_altun_configure_case(sbus-unknown
	ERROR "Unknown Riwo.Utilities.SoftBus interface: invalid"
	OPTIONS "ALTUN_SBUS_INTERFACE=invalid"
)
add_altun_configure_case(bundled-riwo-prefix
	ERROR "ALTUN_RIWO_INSTALL_PREFIX is only valid"
	OPTIONS
		"ALTUN_USE_BUNDLED_RIWO=ON"
		"ALTUN_RIWO_INSTALL_PREFIX=${altun_cmake_test_work_dir}/unused-riwo"
)
add_altun_configure_case(bundled-riwo-without-coro
	ERROR "bundled Riwo requires RIWO_BUILD_CORO=ON"
	OPTIONS "RIWO_BUILD_CORO=OFF"
)
add_altun_configure_case(bundled-riwo-without-utilities
	ERROR "bundled Riwo requires RIWO_BUILD_UTILITIES=ON"
	OPTIONS "RIWO_BUILD_UTILITIES=OFF"
)
add_altun_configure_case(embedded-nlohmann-prefix
	ERROR "ALTUN_NLOHMANN_INSTALL_PREFIX is only valid"
	OPTIONS
		"ALTUN_USE_EMBEDDED_NLOHMANN=ON"
		"ALTUN_NLOHMANN_INSTALL_PREFIX=${altun_cmake_test_work_dir}/unused-json"
)
add_altun_configure_case(missing-external-riwo-prefix
	ERROR "Riwo install prefix does not exist"
	OPTIONS
		"ALTUN_USE_BUNDLED_RIWO=OFF"
		"ALTUN_RIWO_INSTALL_PREFIX=${altun_cmake_test_work_dir}/missing-riwo"
)
add_altun_configure_case(missing-external-nlohmann-prefix
	ERROR "nlohmann/json install prefix does not exist"
	OPTIONS
		"ALTUN_USE_EMBEDDED_NLOHMANN=OFF"
		"ALTUN_NLOHMANN_INSTALL_PREFIX=${altun_cmake_test_work_dir}/missing-json"
)
add_altun_configure_case(stress-without-testing
	ERROR "Stress and performance tests require BUILD_TESTING=ON"
	OPTIONS "ALTUN_BUILD_STRESS_TESTS=ON"
)
add_altun_configure_case(performance-without-testing
	ERROR "Stress and performance tests require BUILD_TESTING=ON"
	OPTIONS "ALTUN_BUILD_PERFORMANCE_TESTS=ON"
)
add_altun_configure_case(cmake-tests-without-testing
	ERROR "CMake integration tests require BUILD_TESTING=ON"
	OPTIONS "ALTUN_BUILD_CMAKE_TESTS=ON"
)
add_altun_configure_case(sanitizers-mutually-exclusive
	ERROR "ASan/UBSan and TSan cannot be enabled together"
	OPTIONS
		"ALTUN_ENABLE_TEST_SANITIZERS=ON"
		"ALTUN_ENABLE_TEST_TSAN=ON"
)
add_altun_configure_case(sanitizer-without-testing
	ERROR "Test sanitizers require BUILD_TESTING=ON"
	OPTIONS "ALTUN_ENABLE_TEST_SANITIZERS=ON"
)
add_altun_configure_case(performance-with-sanitizer
	ERROR "Performance tests cannot be combined with test sanitizers"
	OPTIONS
		"BUILD_TESTING=ON"
		"ALTUN_BUILD_PERFORMANCE_TESTS=ON"
		"ALTUN_ENABLE_TEST_SANITIZERS=ON"
)
foreach(option
	ALTUN_FUNCTIONAL_REPEAT
	ALTUN_FUNCTIONAL_TIMEOUT
	ALTUN_STRESS_SCALE
	ALTUN_STRESS_REPEAT
	ALTUN_STRESS_TIMEOUT
	ALTUN_PERFORMANCE_SCALE
	ALTUN_PERFORMANCE_TIMEOUT
)
	string(TOLOWER "${option}" case_name)
	string(REPLACE "_" "-" case_name "${case_name}")

	add_altun_configure_case(invalid-${case_name}
		ERROR "${option} must be a positive integer"
		OPTIONS "${option}=0"
	)
endforeach()

foreach(option ALTUN_FUNCTIONAL_SEED ALTUN_STRESS_SEED)
	string(TOLOWER "${option}" case_name)
	string(REPLACE "_" "-" case_name "${case_name}")
	add_altun_configure_case(invalid-${case_name}
		ERROR "${option} must be a non-negative integer"
		OPTIONS "${option}=-1"
	)
endforeach()

add_altun_configure_case(invalid-heavy-compile-jobs
	ERROR "ALTUN_HEAVY_COMPILE_JOBS must be a non-negative integer"
	OPTIONS "ALTUN_HEAVY_COMPILE_JOBS=-1"
)
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	add_altun_configure_case(libcxx-requires-clang
		ERROR "ALTUN_USE_LIBCXX requires the Clang compiler"
		OPTIONS "ALTUN_USE_LIBCXX=ON"
	)
	add_altun_configure_case(lld-requires-clang
		ERROR "ALTUN_USE_LLD requires the Clang compiler"
		OPTIONS "ALTUN_USE_LLD=ON"
	)
endif ()

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_altun_configure_case(lto-requires-gnu
		ERROR "ALTUN_ENABLE_LTO requires the GNU compiler"
		OPTIONS "ALTUN_ENABLE_LTO=ON"
	)
endif ()

if (ALTUN_BUILD_LINUX)
	add_altun_configure_case(invalid-gpiod-mode
		ERROR "ALTUN_USE_GPIOD must be AUTO, ON, or OFF"
		OPTIONS "ALTUN_USE_GPIOD=invalid"
	)
	add_altun_configure_case(invalid-pwm-cdev-mode
		ERROR "ALTUN_USE_PWM_CDEV must be AUTO, ON, or OFF"
		OPTIONS "ALTUN_USE_PWM_CDEV=invalid"
	)
endif ()

# A missing component must be diagnosed before any unrelated dependency is
# searched. This keeps package failures actionable on partially provisioned
# consumer systems.
add_altun_package_probe_case(missing-component-precedes-dependencies
	COMPONENT unknown
	ERROR "Required altun components are unavailable: unknown"
	COMPONENTS ${altun_available_components}
	PROBE_OPTIONS "CMAKE_DISABLE_FIND_PACKAGE_Riwo=TRUE"
)

string(JOIN "," altun_consumer_components_arg
	${altun_available_components}
)
set(altun_missing_component linux)

if (ALTUN_BUILD_LINUX)
	set(altun_missing_component unknown)
endif ()

if (CMAKE_CONFIGURATION_TYPES)
	set(altun_consumer_configuration "$<CONFIG>")
else ()
	set(altun_consumer_configuration "${CMAKE_BUILD_TYPE}")
endif ()

# Exercise the installed package from an independent build tree. This catches
# missing headers, incomplete exports, broken transitive dependencies, and
# source-tree-only target assumptions that an in-tree test cannot detect.
add_altun_ctest(altun.cmake.install-consumer
	${CMAKE_COMMAND}
	"-DTEST_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/consumer"
	"-DTEST_BINARY_DIR=${altun_cmake_test_work_dir}/consumer-build"
	"-DTEST_MISSING_BINARY_DIR=${altun_cmake_test_work_dir}/consumer-missing-build"
	"-DTEST_INSTALL_PREFIX=${altun_cmake_test_work_dir}/install"
	"-DTEST_INSTALL_CMAKEDIR=${ALTUN_INSTALL_CMAKEDIR}"
	"-DTEST_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
	"-DTEST_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
	"-DALTUN_BINARY_DIR=${PROJECT_BINARY_DIR}"
	"-DTEST_GENERATOR=${CMAKE_GENERATOR}"
	"-DTEST_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
	"-DTEST_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
	"-DTEST_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
	"-DTEST_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
	"-DTEST_RC_COMPILER=${CMAKE_RC_COMPILER}"
	"-DTEST_MT=${CMAKE_MT}"
	"-DTEST_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
	"-DTEST_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
	"-DTEST_CONFIGURATION=${altun_consumer_configuration}"
	"-DTEST_VERSION=${PROJECT_VERSION}"
	"-DTEST_COMPONENTS=${altun_consumer_components_arg}"
	"-DTEST_MISSING_COMPONENT=${altun_missing_component}"
	"-DTEST_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
	-P "${CMAKE_CURRENT_SOURCE_DIR}/run_install_consumer.cmake"
)
set_tests_properties(altun.cmake.install-consumer PROPERTIES
	LABELS "cmake;install;consumer"
	RUN_SERIAL TRUE
	TIMEOUT 300
)
