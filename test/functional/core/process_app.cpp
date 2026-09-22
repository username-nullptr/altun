// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <chrono>
#include <thread>

int main()
{
	using namespace std::chrono_literals;
	std::this_thread::sleep_for(30s);
	return 0;
}
