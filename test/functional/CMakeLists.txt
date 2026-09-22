# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

add_libempp_functional_test(core empp.core
	core/basic.cpp
)
target_compile_definitions(empp.test.core PRIVATE
	LIBEMPP_TEST_PLUGIN_FILE="$<TARGET_FILE:empp.test.plugin>"
	LIBEMPP_TEST_APP_FILE="$<TARGET_FILE:empp.test.app>"
)
add_dependencies(empp.test.core empp.test.plugin empp.test.app)
set_property(TEST empp.core APPEND PROPERTY LABELS core)

if (LIBEMPP_SBUS_INTERFACE MATCHES "^(local|udp|dbus|shm|default)$")
	set(test_target empp.test.sbus)
	add_libempp_test_program(${test_target} empp.core core/sbus.cpp)
	set(libempp_sbus_test_command $<TARGET_FILE:${test_target}>)

	if (LIBEMPP_SBUS_INTERFACE STREQUAL "dbus" AND UNIX)
		find_program(LIBEMPP_TEST_DBUS_RUN_SESSION NAMES dbus-run-session)
	endif ()
	if ((LIBEMPP_SBUS_INTERFACE STREQUAL "dbus" AND
		LIBEMPP_TEST_DBUS_RUN_SESSION) OR
		LIBEMPP_SBUS_INTERFACE STREQUAL "shm")
		add_executable(empp.test.sbus.peer core/sbus_peer.cpp)
		target_link_libraries(empp.test.sbus.peer PRIVATE empp.core)
		configure_libempp_test_target(empp.test.sbus.peer)

		target_compile_definitions(${test_target} PRIVATE
			LIBEMPP_TEST_SBUS_INTERPROCESS=1
			LIBEMPP_TEST_SBUS_PEER_FILE="$<TARGET_FILE:empp.test.sbus.peer>"
		)
		add_dependencies(${test_target} empp.test.sbus.peer)
		if (LIBEMPP_SBUS_INTERFACE STREQUAL "dbus")
			set(libempp_sbus_test_command
				${LIBEMPP_TEST_DBUS_RUN_SESSION} -- $<TARGET_FILE:${test_target}>
			)
		endif ()
	elseif (LIBEMPP_SBUS_INTERFACE STREQUAL "dbus")
		message(STATUS
			"${PRO_NAME}: dbus-run-session was not found; running only local SBus tests."
		)
	endif ()

	add_libempp_ctest(empp.sbus ${libempp_sbus_test_command})
	set_tests_properties(empp.sbus PROPERTIES
		LABELS "functional;core"
		TIMEOUT ${LIBEMPP_FUNCTIONAL_TIMEOUT}
	)
	set_property(TEST empp.sbus APPEND PROPERTY ENVIRONMENT
		"LIBEMPP_TEST_REPEAT=${LIBEMPP_FUNCTIONAL_REPEAT}"
		"LIBEMPP_TEST_SEED=${LIBEMPP_FUNCTIONAL_SEED}"
		"LIBEMPP_TEST_FAIL_FAST=1"
	)
	if (LIBEMPP_SBUS_INTERFACE STREQUAL "shm")
		target_compile_definitions(${test_target} PRIVATE
			LIBEMPP_TEST_SBUS_SHM=1
		)
		string(MD5 libempp_sbus_test_namespace "${CMAKE_BINARY_DIR}")
		set_property(TEST empp.sbus APPEND PROPERTY ENVIRONMENT
			"LIBEMPP_SBUS_SHM_NAME=/libempp-sbus-test-${libempp_sbus_test_namespace}"
		)
	endif ()
	configure_libempp_sanitized_test(empp.sbus)
endif ()

if (TARGET empp.linux)
	# Keep real ioctl integration out of the executable that interposes ioctl
	# for the I2C, SPI and PWM simulators.
	add_libempp_functional_test(block_device empp.linux
		linux/block.cpp
	)
	set_property(TEST empp.block_device APPEND PROPERTY LABELS linux real-device)

	# This executable is inert unless the caller supplies both the destructive
	# opt-in token and an exact device path. Keep it separate from the virtual
	# suite so a privileged hardware run cannot affect unrelated tests.
	add_libempp_functional_test(storage_destructive empp.linux
		linux/storage_destructive.cpp
	)
	set_property(TEST empp.storage_destructive APPEND PROPERTY LABELS
		linux real-device destructive
	)
	set_property(TEST empp.storage_destructive PROPERTY TIMEOUT 120)

	add_libempp_functional_test(virtual_devices empp.linux
		linux/storage.cpp
		linux/i2c.cpp
		linux/io_model.cpp
		linux/serial.cpp
		linux/spi.cpp
		linux/subsys.cpp
		linux/udev.cpp
		../support/linux/bus.cpp
		../support/linux/gpio.cpp
	)
	if (GPIOD_FOUND AND GPIOD_VERSION VERSION_LESS "2.0")
		target_compile_definitions(empp.test.virtual_devices PRIVATE
			LIBEMPP_TEST_GPIOD_V1
		)
	endif ()
	set_property(TEST empp.virtual_devices APPEND PROPERTY LABELS linux)
endif ()
