# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

function(altun_add_example target component source)
	add_executable(${target} ${source})
	altun_enable_strict_warnings(${target})

	if(component STREQUAL "linux")
		altun_limit_heavy_compile(${target})
	endif()

	target_link_libraries(${target} PRIVATE altun.${component})
	set_target_properties(${target} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${ALTUN_OUTPUT_DIR}/examples/${component}
	)
	install(TARGETS ${target} DESTINATION examples/${component})
endfunction()


altun_add_example(log core core/log.cpp)
altun_add_example(settings core core/settings.cpp)
altun_add_example(plugin_manager core core/plugin_manager.cpp)

add_library(altun_example_plugin SHARED core/plugin.cpp)
altun_enable_strict_warnings(altun_example_plugin)
target_link_libraries(altun_example_plugin PRIVATE altun.core)

set_target_properties(altun_example_plugin PROPERTIES
	PREFIX ""
	OUTPUT_NAME plugin
	RUNTIME_OUTPUT_DIRECTORY ${ALTUN_OUTPUT_DIR}/examples/core
	LIBRARY_OUTPUT_DIRECTORY ${ALTUN_OUTPUT_DIR}/examples/core
)
add_dependencies(plugin_manager altun_example_plugin)

target_compile_definitions(plugin_manager PRIVATE
	ALTUN_EXAMPLE_PLUGIN_NAME="$<TARGET_FILE_NAME:altun_example_plugin>"
)
install(TARGETS altun_example_plugin
	RUNTIME DESTINATION examples/core
	LIBRARY DESTINATION examples/core
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	altun_add_example(udev linux linux/udev.cpp)
	altun_add_example(udev_event linux linux/udev_event.cpp)
	altun_add_example(block linux linux/block.cpp)
	altun_add_example(storage linux linux/storage.cpp)
	altun_add_example(backlight linux linux/backlight.cpp)
	altun_add_example(gpio linux linux/gpio.cpp)
	altun_add_example(i2c linux linux/i2c.cpp)
	altun_add_example(led linux linux/led.cpp)
	altun_add_example(spi linux linux/spi.cpp)
	altun_add_example(pwm linux linux/pwm.cpp)
	altun_add_example(serial_port_binding linux linux/serial_port_binding.cpp)
endif()
