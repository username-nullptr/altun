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

set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)
set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)
