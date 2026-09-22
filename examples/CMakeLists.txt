# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

function(libempp_add_example target component source)
	add_executable(${target} ${source})
	if(component STREQUAL "linux")
		libempp_limit_heavy_compile(${target})
	endif()
	target_link_libraries(${target} PRIVATE empp.${component})

	set_target_properties(${target} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${LIBEMPP_OUTPUT_DIR}/examples/${component}
	)
	install(TARGETS ${target} DESTINATION examples/${component})
endfunction()

libempp_add_example(log core core/log.cpp)
libempp_add_example(settings core core/settings.cpp)
libempp_add_example(plugin_manager core core/plugin_manager.cpp)

add_library(libempp_example_plugin SHARED core/plugin.cpp)
target_link_libraries(libempp_example_plugin PRIVATE empp.core)

set_target_properties(libempp_example_plugin PROPERTIES
	PREFIX ""
	OUTPUT_NAME plugin
	RUNTIME_OUTPUT_DIRECTORY ${LIBEMPP_OUTPUT_DIR}/examples/core
	LIBRARY_OUTPUT_DIRECTORY ${LIBEMPP_OUTPUT_DIR}/examples/core
)
add_dependencies(plugin_manager libempp_example_plugin)

target_compile_definitions(plugin_manager PRIVATE
	LIBEMPP_EXAMPLE_PLUGIN_NAME="$<TARGET_FILE_NAME:libempp_example_plugin>"
)
install(TARGETS libempp_example_plugin
	RUNTIME DESTINATION examples/core
	LIBRARY DESTINATION examples/core
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	libempp_add_example(udev linux linux/udev.cpp)
	libempp_add_example(udev_event linux linux/udev_event.cpp)
	libempp_add_example(block linux linux/block.cpp)
	libempp_add_example(storage linux linux/storage.cpp)
	libempp_add_example(backlight linux linux/backlight.cpp)
	libempp_add_example(gpio linux linux/gpio.cpp)
	libempp_add_example(i2c linux linux/i2c.cpp)
	libempp_add_example(led linux linux/led.cpp)
	libempp_add_example(spi linux linux/spi.cpp)
	libempp_add_example(pwm linux linux/pwm.cpp)
	libempp_add_example(serial_port_binding linux linux/serial_port_binding.cpp)
endif()
