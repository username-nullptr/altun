# SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
# SPDX-License-Identifier: MIT

if (LIBEMPP_BUILD_SUBMODEL_LIBGS)
	set(${PRO_NAME}_3rd_path ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party)

	include_directories(SYSTEM
		${${PRO_NAME}_3rd_path}/libgs/3rd_party/nlohmann.json
		${${PRO_NAME}_3rd_path}/libgs/3rd_party/spdlog
		${${PRO_NAME}_3rd_path}/libgs/3rd_party/asio
		${${PRO_NAME}_3rd_path}/libgs
		${LIBGS_CONFIG_INCLUDE}
	)
endif ()
