// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_CORE_SBUS_DETAIL_CYCLONE_INTERFACE_H
#define LIBEMPP_CORE_SBUS_DETAIL_CYCLONE_INTERFACE_H

#include <libempp/core/global.h>
#if LIBEMPP_SBUS_CYCLONE_SUPPORT

namespace libempp::sbus
{

class LIBEMPP_CORE_API cyclone_interface final :
	public std::enable_shared_from_this<cyclone_interface>
{
	RIWO_DISABLE_COPY_MOVE(cyclone_interface)

public:
	friend void bridge_cyclone_data_available (
		std::string_view, const void*, size_t
	);
	cyclone_interface();
	~cyclone_interface();

	static void init();
	static void publish(std::string_view topic, const void *buffer, size_t size);

	uint64_t subscribe(std::string_view topic, std::function<void(const void*, size_t)> func);
	uint64_t subscribe(std::function<void(std::string_view topic, const void*, size_t)> func);

	void cancel_topic(std::string_view topic);
	void cancel_sid(uint64_t sid);
	void cancel();

private:
	class impl;
	std::unique_ptr<impl> m_impl {};
};

} //namespace libempp::sbus

#endif //LIBEMPP_SBUS_CYCLONE_SUPPORT
#endif //LIBEMPP_CORE_SBUS_DETAIL_CYCLONE_INTERFACE_H
