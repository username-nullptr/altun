# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

add_altun_functional_test(core altun.core
	core/basic.cpp
)
target_compile_definitions(altun.test.core PRIVATE
	ALTUN_TEST_PLUGIN_FILE="$<TARGET_FILE:altun.test.plugin>"
	ALTUN_TEST_APP_FILE="$<TARGET_FILE:altun.test.app>"
)
add_dependencies(altun.test.core altun.test.plugin altun.test.app)
set_property(TEST altun.core APPEND PROPERTY LABELS core)

if (ALTUN_SBUS_INTERFACE MATCHES "^(local|udp|dbus|shm|default)$")
	set(test_target altun.test.sbus)
	add_altun_test_program(${test_target} altun.core core/sbus.cpp)
	set(altun_sbus_test_command $<TARGET_FILE:${test_target}>)

	if (ALTUN_SBUS_INTERFACE STREQUAL "dbus" AND UNIX)
		find_program(ALTUN_TEST_DBUS_RUN_SESSION NAMES dbus-run-session)
	endif ()
	if ((ALTUN_SBUS_INTERFACE STREQUAL "dbus" AND
		ALTUN_TEST_DBUS_RUN_SESSION) OR
		ALTUN_SBUS_INTERFACE STREQUAL "shm")
		add_executable(altun.test.sbus.peer core/sbus_peer.cpp)
		target_link_libraries(altun.test.sbus.peer PRIVATE altun.core)
		configure_altun_test_target(altun.test.sbus.peer)

		target_compile_definitions(${test_target} PRIVATE
			ALTUN_TEST_SBUS_INTERPROCESS=1
			ALTUN_TEST_SBUS_PEER_FILE="$<TARGET_FILE:altun.test.sbus.peer>"
		)
		add_dependencies(${test_target} altun.test.sbus.peer)
		if (ALTUN_SBUS_INTERFACE STREQUAL "dbus")
			set(altun_sbus_test_command
				${ALTUN_TEST_DBUS_RUN_SESSION} -- $<TARGET_FILE:${test_target}>
			)
		endif ()
	elseif (ALTUN_SBUS_INTERFACE STREQUAL "dbus")
		message(STATUS
			"${PRO_NAME}: dbus-run-session was not found; running only local SBus tests."
		)
	endif ()

	add_altun_ctest(altun.sbus ${altun_sbus_test_command})
	set_tests_properties(altun.sbus PROPERTIES
		LABELS "functional;core"
		TIMEOUT ${ALTUN_FUNCTIONAL_TIMEOUT}
	)
	set_property(TEST altun.sbus APPEND PROPERTY ENVIRONMENT
		"ALTUN_TEST_REPEAT=${ALTUN_FUNCTIONAL_REPEAT}"
		"ALTUN_TEST_SEED=${ALTUN_FUNCTIONAL_SEED}"
		"ALTUN_TEST_FAIL_FAST=1"
	)
	if (ALTUN_SBUS_INTERFACE STREQUAL "shm")
		target_compile_definitions(${test_target} PRIVATE
			ALTUN_TEST_SBUS_SHM=1
		)
		string(MD5 altun_sbus_test_namespace "${CMAKE_BINARY_DIR}")
		set_property(TEST altun.sbus APPEND PROPERTY ENVIRONMENT
			"ALTUN_SBUS_SHM_NAME=/altun-sbus-test-${altun_sbus_test_namespace}"
		)
	endif ()
	configure_altun_sanitized_test(altun.sbus)
endif ()

if (TARGET altun.linux)
	# Keep real ioctl integration out of the executable that interposes ioctl
	# for the I2C, SPI and PWM simulators.
	add_altun_functional_test(block_device altun.linux
		linux/block.cpp
	)
	set_property(TEST altun.block_device APPEND PROPERTY LABELS linux real-device)

	# This executable is inert unless the caller supplies both the destructive
	# opt-in token and an exact device path. Keep it separate from the virtual
	# suite so a privileged hardware run cannot affect unrelated tests.
	add_altun_functional_test(storage_destructive altun.linux
		linux/storage_destructive.cpp
	)
	set_property(TEST altun.storage_destructive APPEND PROPERTY LABELS
		linux real-device destructive
	)
	set_property(TEST altun.storage_destructive PROPERTY TIMEOUT 120)

	add_altun_functional_test(virtual_devices altun.linux
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
	if (ALTUN_GPIOD_FOUND AND ALTUN_GPIOD_VERSION VERSION_LESS "2.0")
		target_compile_definitions(altun.test.virtual_devices PRIVATE
			ALTUN_TEST_GPIOD_V1
		)
	endif ()
	set_property(TEST altun.virtual_devices APPEND PROPERTY LABELS linux)
endif ()
