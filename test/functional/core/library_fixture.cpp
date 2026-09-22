// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#if defined(_WIN32)
# define EMPP_TEST_EXPORT __declspec(dllexport)
#else
# define EMPP_TEST_EXPORT __attribute__((visibility("default")))
#endif

extern "C" EMPP_TEST_EXPORT int libempp_test_double(int value)
{
	return value * 2;
}
