// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#if defined(_WIN32)
# define ALTUN_TEST_EXPORT __declspec(dllexport)
#else
# define ALTUN_TEST_EXPORT __attribute__((visibility("default")))
#endif

extern "C" ALTUN_TEST_EXPORT int altun_test_double(int value)
{
	return value * 2;
}
