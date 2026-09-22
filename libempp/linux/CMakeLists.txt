# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
	message(FATAL_ERROR "This module is only for Linux platform.")
endif ()

find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED IMPORTED_TARGET libudev)
pkg_check_modules(BLKID REQUIRED IMPORTED_TARGET blkid)

include(CheckCXXSourceCompiles)

set(LIBEMPP_USE_GPIOD "AUTO" CACHE STRING
	"Use libgpiod for GPIO access: AUTO, ON, or OFF."
)
set_property(CACHE LIBEMPP_USE_GPIOD PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${LIBEMPP_USE_GPIOD}" LIBEMPP_GPIOD_MODE)
set(GPIOD_FOUND FALSE)

set(LIBEMPP_USE_PWM_CDEV "AUTO" CACHE STRING
	"Use the Linux PWM character-device API: AUTO, ON, or OFF."
)
set_property(CACHE LIBEMPP_USE_PWM_CDEV PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${LIBEMPP_USE_PWM_CDEV}" LIBEMPP_PWM_CDEV_MODE)

if(NOT LIBEMPP_GPIOD_MODE MATCHES "^(AUTO|ON|OFF)$")
	message(FATAL_ERROR
		"LIBEMPP_USE_GPIOD must be AUTO, ON, or OFF "
		"(received '${LIBEMPP_USE_GPIOD}')."
	)
endif()

if(NOT LIBEMPP_PWM_CDEV_MODE MATCHES "^(AUTO|ON|OFF)$")
	message(FATAL_ERROR
		"LIBEMPP_USE_PWM_CDEV must be AUTO, ON, or OFF "
		"(received '${LIBEMPP_USE_PWM_CDEV}')."
	)
endif()

if(LIBEMPP_GPIOD_MODE STREQUAL "ON")
	pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod)
elseif(LIBEMPP_GPIOD_MODE STREQUAL "AUTO")
	pkg_check_modules(GPIOD QUIET IMPORTED_TARGET libgpiod)
endif()

set(gpio_backend_dir "${CMAKE_CURRENT_SOURCE_DIR}/subsys/detail/gpio")

if(GPIOD_FOUND AND GPIOD_VERSION VERSION_GREATER_EQUAL "2.0")
	set(gpio_disabled_backends
		"${gpio_backend_dir}/gpiod_v1.cpp"
		"${gpio_backend_dir}/sysfs.cpp"
	)
elseif(GPIOD_FOUND)
	set(gpio_disabled_backends
		"${gpio_backend_dir}/gpiod_v2.cpp"
		"${gpio_backend_dir}/sysfs.cpp"
	)
else()
	set(gpio_disabled_backends
		"${gpio_backend_dir}/gpiod_v1.cpp"
		"${gpio_backend_dir}/gpiod_v2.cpp"
	)
endif()

if(LIBEMPP_PWM_CDEV_MODE STREQUAL "OFF")
	set(PWM_CDEV_FOUND FALSE)
else()
	check_cxx_source_compiles([=[
		#include <linux/pwm.h>
		#ifndef PWM_IOCTL_REQUEST
		# error "Linux PWM character-device API is unavailable"
		#endif
		#ifndef PWM_IOCTL_FREE
		# error "Linux PWM character-device API is unavailable"
		#endif
		#ifndef PWM_IOCTL_ROUNDWF
		# error "Linux PWM character-device API is unavailable"
		#endif
		#ifndef PWM_IOCTL_GETWF
		# error "Linux PWM character-device API is unavailable"
		#endif
		#ifndef PWM_IOCTL_SETEXACTWF
		# error "Linux PWM character-device API is unavailable"
		#endif
		int main()
		{
			pwmchip_waveform waveform {};
			return static_cast<int>(waveform.hwpwm);
		}
	]=] PWM_CDEV_FOUND)
endif()

if(LIBEMPP_PWM_CDEV_MODE STREQUAL "ON" AND NOT PWM_CDEV_FOUND)
	message(FATAL_ERROR
		"LIBEMPP_USE_PWM_CDEV=ON requires a linux/pwm.h header with "
		"the PWM character-device waveform API."
	)
endif()

set(pwm_backend_dir "${CMAKE_CURRENT_SOURCE_DIR}/subsys/detail/pwm")
if(PWM_CDEV_FOUND)
	set(pwm_disabled_backend "${pwm_backend_dir}/sysfs.cpp")
else()
	set(pwm_disabled_backend "${pwm_backend_dir}/cdev.cpp")
endif()

# libempp_add_library discovers every .cpp file recursively. Keep the
# non-selected implementations visible to IDEs without compiling them.
set_source_files_properties(${gpio_disabled_backends}
	PROPERTIES HEADER_FILE_ONLY TRUE
)
set_source_files_properties(${pwm_disabled_backend}
	PROPERTIES HEADER_FILE_ONLY TRUE
)

set(target_name empp.linux)
libempp_add_library(${target_name})

target_compile_definitions(${target_name} PRIVATE _FILE_OFFSET_BITS=64)
target_link_libraries(${target_name} PUBLIC
   empp.core PkgConfig::UDEV
)
target_link_libraries(${target_name} PRIVATE PkgConfig::BLKID)

if(GPIOD_FOUND)
	target_link_libraries(${target_name} PRIVATE PkgConfig::GPIOD)
	message(STATUS "${PRO_NAME}: GPIO backend: libgpiod ${GPIOD_VERSION}")
else()
	message(STATUS "${PRO_NAME}: GPIO backend: legacy sysfs")
endif()

if(PWM_CDEV_FOUND)
	message(STATUS "${PRO_NAME}: PWM backend: character device")
else()
	message(STATUS "${PRO_NAME}: PWM backend: legacy sysfs")
endif()
