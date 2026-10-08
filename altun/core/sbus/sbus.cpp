// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "sbus.h"

namespace altun::sbus
{

void publish(std::string_view topic, const void *buffer, size_t size)
{
	riwo::utils::sbus::publish<interface>(topic, buffer, size);
}

void publish(std::string_view topic, const char *str)
{
	riwo::utils::sbus::publish<interface>(topic, str, strlen(str));
}

} //namespace altun::sbus
