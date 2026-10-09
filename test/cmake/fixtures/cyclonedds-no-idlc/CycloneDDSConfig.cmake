# SPDX-FileCopyrightText: 2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

# Model a valid CycloneDDS runtime package installed without its optional IDL
# compiler/development tools.  In particular, do not define idlc_generate.
if (NOT TARGET CycloneDDS::ddsc)
	add_library(CycloneDDS::ddsc INTERFACE IMPORTED)
endif ()
