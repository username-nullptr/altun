// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_SUBSYS_PWM_H
#define LIBEMPP_LINUX_SUBSYS_PWM_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/global.h>

namespace libempp::subsys
{

class LIBEMPP_LINUX_API pwm
{
	RIWO_DISABLE_COPY(pwm)

public:
	using channel_t = uint8_t;

	enum class backend_t : uint8_t
	{
		cdev,
		sysfs
	};

	struct node_t
	{
		std::string chip = "pwmchip0";
		channel_t channel = 0;
	};
	explicit pwm(node_t node);
	explicit pwm();
	~pwm();

	pwm(pwm &&other) noexcept;
	pwm &operator=(pwm &&other) noexcept;

public:
	pwm &open(node_t node, std::error_code &error) noexcept;
	pwm &open(node_t node);
	pwm &close() noexcept;

public:
	pwm &set_period(uint32_t period, std::error_code &error) noexcept;
	pwm &set_period(uint32_t period);

	pwm &set_duty_cycle(uint32_t duty_cycle, std::error_code &error) noexcept;
	pwm &set_duty_cycle(uint32_t duty_cycle);

	pwm &set(uint32_t period, uint32_t duty_cycle, std::error_code &error) noexcept;
	pwm &set(uint32_t period, uint32_t duty_cycle);

public:
	pwm &set_enable(bool enable, std::error_code &error) noexcept;
	pwm &set_enable(bool enable);

	pwm &enable(std::error_code &error) noexcept;
	pwm &enable();

	pwm &disable(std::error_code &error) noexcept;
	pwm &disable();

public:
	[[nodiscard]] uint32_t period() const noexcept;
	[[nodiscard]] uint32_t duty_cycle() const noexcept;

	[[nodiscard]] bool is_enabled() const noexcept;
	[[nodiscard]] bool is_open() const noexcept;

	[[nodiscard]] static backend_t backend() noexcept;
	[[nodiscard]] static const char *backend_name() noexcept;

private:
	class impl;
	std::unique_ptr<impl> m_impl;
};

} //namespace libempp::subsys

#endif //__linux__
#endif //LIBEMPP_LINUX_SUBSYS_PWM_H
