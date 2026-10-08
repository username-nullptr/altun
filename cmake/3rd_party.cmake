# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

set(${PRO_NAME}_3rd_path ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party)

if (LIBEMPP_USE_EMBEDDED_NLOHMANN)
	include_directories(SYSTEM
		${${PRO_NAME}_3rd_path}/nlohmann.json
	)
endif ()

if (LIBEMPP_USE_EMBEDDED_RIWO)
	include_directories(SYSTEM
		${${PRO_NAME}_3rd_path}/riwo/3rd_party/spdlog
		${${PRO_NAME}_3rd_path}/riwo/3rd_party/asio
		${${PRO_NAME}_3rd_path}/riwo
		${RIWO_CONFIG_INCLUDE}
	)
endif ()
