// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "backend.h"
#ifdef __linux__

#include <linux/pwm.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

namespace altun::subsys::detail { namespace
{

std::error_code system_error_from_errno() noexcept {
	return { errno != 0 ? errno : EIO, std::system_category() };
}

std::filesystem::path chip_path(const std::string &chip)
{
	const std::filesystem::path configured(chip);
	return configured.has_parent_path() ? configured :
		std::filesystem::path("/dev") / configured;
}

int channel_ioctl(int descriptor, unsigned long request, pwm::channel_t channel) noexcept
{
	int result = -1;
	do {
		result = ::ioctl(descriptor, request,
			static_cast<unsigned long>(channel)
		);
	}
	while( result < 0 and errno == EINTR );
	return result;
}

int waveform_ioctl(int descriptor, unsigned long request, pwmchip_waveform &waveform) noexcept
{
	int result = -1;
	do {
		result = ::ioctl(descriptor, request, &waveform);
	}
	while( result < 0 and errno == EINTR) ;
	return result;
}

class cdev_pwm_backend final : public pwm_backend
{
public:
	~cdev_pwm_backend() override {
		close();
	}

	void open(const pwm::node_t &node, std::error_code &error) noexcept override
	{
		close();
		error.clear();
		try {
			const auto path = chip_path(node.chip);
			const int descriptor = ::open(path.c_str(), O_RDWR | O_CLOEXEC);

			if( descriptor < 0 )
			{
				error = errno == ENOENT ?
					std::make_error_code(std::errc::no_such_device) :
					system_error_from_errno();
				return ;
			}
			m_descriptor = descriptor;
			m_channel = node.channel;

			if( channel_ioctl(m_descriptor, PWM_IOCTL_REQUEST, m_channel) < 0 )
			{
				error = system_error_from_errno();
				close();
				return ;
			}
			m_requested = true;
			pwmchip_waveform waveform {};
			waveform.hwpwm = m_channel;

			if( waveform_ioctl(m_descriptor, PWM_IOCTL_GETWF, waveform) < 0 )
			{
				error = system_error_from_errno();
				close();
				return ;
			}
			if( waveform.period_length_ns > std::numeric_limits<uint32_t>::max() or
				waveform.duty_length_ns > std::numeric_limits<uint32_t>::max() )
			{
				error = std::make_error_code(std::errc::value_too_large);
				close();
				return ;
			}
			m_period = static_cast<uint32_t>(waveform.period_length_ns);
			m_duty_cycle = static_cast<uint32_t>(waveform.duty_length_ns);
			m_enabled = waveform.period_length_ns != 0;
		}
		catch(const std::bad_alloc&)
		{
			error = std::make_error_code(std::errc::not_enough_memory);
			close();
		}
		catch(...)
		{
			error = std::make_error_code(std::errc::io_error);
			close();
		}
	}

	void close() noexcept override
	{
		if( m_descriptor >= 0 and m_requested )
			static_cast<void>(channel_ioctl(m_descriptor, PWM_IOCTL_FREE, m_channel));

		if( m_descriptor >= 0 )
			::close(m_descriptor);

		m_descriptor = -1;
		m_channel = 0;
		m_requested = false;

		m_period = 0;
		m_duty_cycle = 0;
		m_enabled = false;
	}

	void set_period(uint32_t period, std::error_code &error) noexcept override
	{
		error.clear();
		if( period < m_duty_cycle )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return ;
		}
		apply(period, m_duty_cycle, m_enabled, error);
	}

	void set_duty_cycle(uint32_t duty_cycle, std::error_code &error) noexcept override
	{
		error.clear();
		if( duty_cycle > m_period )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return ;
		}
		apply(m_period, duty_cycle, m_enabled, error);
	}

	void set(uint32_t period, uint32_t duty_cycle, std::error_code &error) noexcept override
	{
		error.clear();
		if( duty_cycle > period )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return ;
		}
		apply(period, duty_cycle, m_enabled, error);
	}

	void set_enable(bool enable, std::error_code &error) noexcept override
	{
		error.clear();
		if( enable and m_period == 0 )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return ;
		}
		apply(m_period, m_duty_cycle, enable, error);
	}

	[[nodiscard]] uint32_t period() const noexcept override {
		return m_period;
	}

	[[nodiscard]] uint32_t duty_cycle() const noexcept override {
		return m_duty_cycle;
	}

	[[nodiscard]] bool is_enabled() const noexcept override {
		return m_enabled;
	}

	[[nodiscard]] bool is_open() const noexcept override {
		return m_descriptor >= 0 and m_requested;
	}

private:
	void apply(uint32_t period, uint32_t duty_cycle, bool enabled, std::error_code &error) noexcept
	{
		error.clear();
		if( enabled and period == 0 )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return ;
		}
		if( not enabled )
		{
			if( m_enabled )
			{
				pwmchip_waveform disabled {};
				disabled.hwpwm = m_channel;

				if( waveform_ioctl(m_descriptor, PWM_IOCTL_SETEXACTWF, disabled) < 0 )
				{
					error = system_error_from_errno();
					return ;
				}
			}
			m_period = period;
			m_duty_cycle = duty_cycle;
			m_enabled = false;
			return ;
		}

		pwmchip_waveform waveform {};
		waveform.hwpwm = m_channel;
		waveform.period_length_ns = period;
		waveform.duty_length_ns = duty_cycle;

		if( waveform_ioctl(m_descriptor, PWM_IOCTL_ROUNDWF, waveform) < 0 )
		{
			error = system_error_from_errno();
			return ;
		}
		if( waveform.period_length_ns > std::numeric_limits<uint32_t>::max() or
			waveform.duty_length_ns > std::numeric_limits<uint32_t>::max() )
		{
			error = std::make_error_code(std::errc::value_too_large);
			return ;
		}
		if( waveform_ioctl(m_descriptor, PWM_IOCTL_SETEXACTWF, waveform) < 0 )
		{
			error = system_error_from_errno();
			return ;
		}
		m_period = static_cast<uint32_t>(waveform.period_length_ns);
		m_duty_cycle = static_cast<uint32_t>(waveform.duty_length_ns);
		m_enabled = true;
	}

private:
	int m_descriptor = -1;
	pwm::channel_t m_channel = 0;
	bool m_requested = false;

	uint32_t m_period = 0;
	uint32_t m_duty_cycle = 0;
	bool m_enabled = false;
};

} // namespace

std::unique_ptr<pwm_backend> make_pwm_backend(std::error_code &error) noexcept
{
	error.clear();
	auto result = std::unique_ptr<pwm_backend>(new(std::nothrow) cdev_pwm_backend());
	if( not result )
		error = std::make_error_code(std::errc::not_enough_memory);
	return result;
}

pwm::backend_t pwm_backend_type() noexcept
{
	return pwm::backend_t::cdev;
}

const char *pwm_backend_name() noexcept
{
	return "cdev";
}

} // namespace altun::subsys::detail

#endif //__linux__
