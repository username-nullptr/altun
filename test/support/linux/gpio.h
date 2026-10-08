// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_TEST_LINUX_GPIO_TEST_SUPPORT_H
#define ALTUN_TEST_LINUX_GPIO_TEST_SUPPORT_H

namespace altun_test_support
{

[[nodiscard]] bool virtual_gpio_available() noexcept;
void reset_virtual_gpio() noexcept;
void push_virtual_gpio_event(bool rising);
void wait_for_virtual_gpio_waiter();

} // namespace altun_test_support

#endif // ALTUN_TEST_LINUX_GPIO_TEST_SUPPORT_H
