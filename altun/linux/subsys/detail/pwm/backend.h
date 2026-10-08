// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_SUBSYS_DETAIL_PWM_BACKEND_H
#define ALTUN_LINUX_SUBSYS_DETAIL_PWM_BACKEND_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/subsys/pwm.h>

namespace altun::subsys::detail
{

class ALTUN_LINUX_API pwm_backend
{
	RIWO_DISABLE_COPY_MOVE(pwm_backend)

public:
	pwm_backend() = default;
	virtual ~pwm_backend() = default;

	virtual void open(const pwm::node_t &node, std::error_code &error) noexcept = 0;
	virtual void close() noexcept = 0;

	virtual void set_period(uint32_t period, std::error_code &error) noexcept = 0;
	virtual void set_duty_cycle(uint32_t duty_cycle, std::error_code &error) noexcept = 0;

	virtual void set(uint32_t period, uint32_t duty_cycle, std::error_code &error) noexcept = 0;
	virtual void set_enable(bool enable, std::error_code &error) noexcept = 0;

	[[nodiscard]] virtual uint32_t period() const noexcept = 0;
	[[nodiscard]] virtual uint32_t duty_cycle() const noexcept = 0;

	[[nodiscard]] virtual bool is_enabled() const noexcept = 0;
	[[nodiscard]] virtual bool is_open() const noexcept = 0;
};

[[nodiscard]] ALTUN_LINUX_API
std::unique_ptr<pwm_backend> make_pwm_backend(std::error_code &error) noexcept;

[[nodiscard]] ALTUN_LINUX_API
pwm::backend_t pwm_backend_type() noexcept;

[[nodiscard]] ALTUN_LINUX_API
const char *pwm_backend_name() noexcept;

} // namespace altun::subsys::detail

#endif //__linux__
#endif // ALTUN_LINUX_SUBSYS_DETAIL_PWM_BACKEND_H
