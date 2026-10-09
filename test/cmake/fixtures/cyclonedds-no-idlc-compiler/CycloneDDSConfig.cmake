# SPDX-FileCopyrightText: 2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

# Model a package that exports the IDL CMake helper but neither an imported
# compiler target nor a host idlc program.  The find switches keep this fixture
# independent of idlc tools installed on the machine running the test.
if (NOT TARGET CycloneDDS::ddsc)
	add_library(CycloneDDS::ddsc INTERFACE IMPORTED)
endif ()


function(idlc_generate)
endfunction()


# Disable every host search source that find_program() can consult.  In
# particular, vcpkg toolchains populate CMAKE_PROGRAM_PATH with package tools,
# which would otherwise let this intentionally incomplete fixture find the
# machine's real idlc executable and turn the negative test into a host-
# dependent result.
set(CMAKE_FIND_USE_PACKAGE_ROOT_PATH FALSE)
set(CMAKE_FIND_USE_CMAKE_PATH FALSE)

set(CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH FALSE)
set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)

set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)
set(CMAKE_FIND_USE_INSTALL_PREFIX FALSE)

set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY FALSE)
