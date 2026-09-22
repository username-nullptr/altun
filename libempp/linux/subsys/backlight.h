// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_SUBSYS_BACKLIGHT_H
#define LIBEMPP_LINUX_SUBSYS_BACKLIGHT_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/global.h>

namespace libempp::subsys
{

class LIBEMPP_LINUX_API backlight
{
	LIBGS_DISABLE_COPY(backlight)

public:
	using brightness_t = uint32_t;

	enum class power_t : uint8_t
	{
		unblank = 0,
		normal = 1,
		vsync_suspend = 2,
		hsync_suspend = 3,
		power_down = 4
	};

	struct node_t {
		std::string name = "backlight0";
	};

public:
	explicit backlight(node_t node);
	explicit backlight();
	~backlight();

	backlight(backlight &&other) noexcept;
	backlight &operator=(backlight &&other) noexcept;

public:
	backlight &open(const node_t& node, std::error_code &error) noexcept;
	backlight &open(const node_t& node);
	backlight &close();

public:
	backlight &set_brightness(brightness_t brightness, std::error_code &error) noexcept;
	backlight &set_brightness(brightness_t brightness);

	backlight &set_power(power_t power, std::error_code &error) noexcept;
	backlight &set_power(power_t power);

public:
	backlight &set_enable(bool enable, std::error_code &error) noexcept;
	backlight &set_enable(bool enable);

	backlight &enable(std::error_code &error) noexcept;
	backlight &enable();

	backlight &disable(std::error_code &error) noexcept;
	backlight &disable();

public:
	[[nodiscard]] brightness_t brightness() const noexcept;
	[[nodiscard]] brightness_t actual_brightness() const noexcept;
	[[nodiscard]] brightness_t max_brightness() const noexcept;
	[[nodiscard]] power_t power() const noexcept;

	[[nodiscard]] bool is_enabled() const noexcept;
	[[nodiscard]] bool is_open() const noexcept;

private:
	class impl;
	impl *m_impl = nullptr;
};

} // namespace libempp::subsys

#endif //__linux__
#endif // LIBEMPP_LINUX_SUBSYS_BACKLIGHT_H
