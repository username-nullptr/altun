// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_SUBSYS_LED_H
#define ALTUN_LINUX_SUBSYS_LED_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/global.h>

namespace altun::subsys
{

class ALTUN_LINUX_API led
{
	RIWO_DISABLE_COPY(led)

public:
	using brightness_t = uint32_t;
	using duration_t = std::chrono::milliseconds;

	struct node_t {
		std::string name = "led0";
	};

public:
	explicit led(const node_t &node);
	explicit led();
	~led();

	led(led &&other) noexcept;
	led &operator=(led &&other) noexcept;

public:
	led &open(const node_t &node, std::error_code &error) noexcept;
	led &open(const node_t &node);
	led &close() noexcept;

public:
	led &set_brightness(brightness_t brightness, std::error_code &error) noexcept;
	led &set_brightness(brightness_t brightness);

	led &set_trigger(std::string_view trigger, std::error_code &error) noexcept;
	led &set_trigger(std::string_view trigger);

	led &set_blink(duration_t delay_on, duration_t delay_off, std::error_code &error) noexcept;
	led &set_blink(duration_t delay_on, duration_t delay_off);

public:
	[[nodiscard]] brightness_t brightness() const noexcept;
	[[nodiscard]] brightness_t max_brightness() const noexcept;

	[[nodiscard]] std::string trigger(std::error_code &error) const noexcept;
	[[nodiscard]] std::string trigger() const;

	[[nodiscard]] std::vector<std::string> triggers(std::error_code &error) const noexcept;
	[[nodiscard]] std::vector<std::string> triggers() const;

	[[nodiscard]] bool supports_triggers() const noexcept;
	[[nodiscard]] bool is_open() const noexcept;

private:
	class impl;
	std::unique_ptr<impl> m_impl;
};

} // namespace altun::subsys

#endif //__linux__
#endif // ALTUN_LINUX_SUBSYS_LED_H
