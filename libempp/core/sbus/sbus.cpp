// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "sbus.h"

namespace libempp::sbus
{

void publish(std::string_view topic, const void *buffer, size_t size)
{
	libgs::utils::sbus::publish<interface>(topic, buffer, size);
}

void publish(std::string_view topic, const char *str)
{
	libgs::utils::sbus::publish<interface>(topic, str, strlen(str));
}

} //namespace libempp::sbus
