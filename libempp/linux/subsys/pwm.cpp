// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "pwm.h"

#ifdef __linux__

#include "detail/pwm/backend.h"

#include <new>
#include <utility>

namespace libempp::subsys
{

class RIWO_DECL_HIDDEN pwm::impl
{
	RIWO_DISABLE_COPY_MOVE(impl)

public:
	impl() = default;
	~impl() {
		close();
	}

	void open(const node_t &node, std::error_code &error) noexcept
	{
		close();
		error.clear();
		if(node.chip.empty())
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return ;
		}
		if(not m_backend)
		{
			m_backend = detail::make_pwm_backend(error);
			if(error)
				return ;
		}
		m_backend->open(node, error);
	}

	void close() noexcept
	{
		if(m_backend)
			m_backend->close();
	}

	[[nodiscard]] bool is_open() const noexcept {
		return m_backend and m_backend->is_open();
	}

	std::unique_ptr<detail::pwm_backend> m_backend;
};

pwm::pwm(node_t node) :
	m_impl(std::make_unique<impl>())
{
	std::error_code error;
	m_impl->open(node, error);
	if(error)
		riwo::system_error::loc_throw(error, "libempp::subsys::pwm::open");
}

pwm::pwm() :
	m_impl(std::make_unique<impl>())
{
}

pwm::~pwm() = default;

pwm::pwm(pwm &&other) noexcept = default;

pwm &pwm::operator=(pwm &&other) noexcept = default;

pwm &pwm::open(node_t node, std::error_code &error) noexcept
{
	if(not m_impl)
	{
		m_impl.reset(new(std::nothrow) impl());
		if(not m_impl)
		{
			error = std::make_error_code(std::errc::not_enough_memory);
			return *this;
		}
	}
	m_impl->open(node, error);
	return *this;
}

pwm &pwm::open(node_t node)
{
	std::error_code error;
	open(std::move(node), error);
	if(error)
		riwo::system_error::loc_throw(error, "libempp::subsys::pwm::open");
	return *this;
}

pwm &pwm::close() noexcept
{
	if(m_impl)
		m_impl->close();
	return *this;
}

pwm &pwm::set_period(uint32_t period, std::error_code &error) noexcept
{
	if(not is_open())
		error = std::make_error_code(std::errc::bad_file_descriptor);
	else
		m_impl->m_backend->set_period(period, error);
	return *this;
}

pwm &pwm::set_period(uint32_t period)
{
	std::error_code error;
	set_period(period, error);
	if(error)
		riwo::system_error::loc_throw(error, "libempp::subsys::pwm::set_period");
	return *this;
}

pwm &pwm::set_duty_cycle(uint32_t duty_cycle,
	std::error_code &error) noexcept
{
	if(not is_open())
		error = std::make_error_code(std::errc::bad_file_descriptor);
	else
		m_impl->m_backend->set_duty_cycle(duty_cycle, error);
	return *this;
}

pwm &pwm::set_duty_cycle(uint32_t duty_cycle)
{
	std::error_code error;
	set_duty_cycle(duty_cycle, error);
	if(error)
		riwo::system_error::loc_throw(
			error, "libempp::subsys::pwm::set_duty_cycle");
	return *this;
}

pwm &pwm::set(uint32_t period, uint32_t duty_cycle,
	std::error_code &error) noexcept
{
	if(not is_open())
		error = std::make_error_code(std::errc::bad_file_descriptor);
	else
		m_impl->m_backend->set(period, duty_cycle, error);
	return *this;
}

pwm &pwm::set(uint32_t period, uint32_t duty_cycle)
{
	std::error_code error;
	set(period, duty_cycle, error);
	if(error)
		riwo::system_error::loc_throw(error, "libempp::subsys::pwm::set");
	return *this;
}

pwm &pwm::set_enable(bool enable, std::error_code &error) noexcept
{
	if(not is_open())
		error = std::make_error_code(std::errc::bad_file_descriptor);
	else
		m_impl->m_backend->set_enable(enable, error);
	return *this;
}

pwm &pwm::set_enable(bool enable)
{
	std::error_code error;
	set_enable(enable, error);
	if(error)
		riwo::system_error::loc_throw(error, "libempp::subsys::pwm::set_enable");
	return *this;
}

pwm &pwm::enable(std::error_code &error) noexcept
{
	return set_enable(true, error);
}

pwm &pwm::enable()
{
	return set_enable(true);
}

pwm &pwm::disable(std::error_code &error) noexcept
{
	return set_enable(false, error);
}

pwm &pwm::disable()
{
	return set_enable(false);
}

uint32_t pwm::period() const noexcept
{
	return is_open() ? m_impl->m_backend->period() : 0;
}

uint32_t pwm::duty_cycle() const noexcept
{
	return is_open() ? m_impl->m_backend->duty_cycle() : 0;
}

bool pwm::is_enabled() const noexcept
{
	return is_open() and m_impl->m_backend->is_enabled();
}

bool pwm::is_open() const noexcept
{
	return m_impl and m_impl->is_open();
}

pwm::backend_t pwm::backend() noexcept
{
	return detail::pwm_backend_type();
}

const char *pwm::backend_name() noexcept
{
	return detail::pwm_backend_name();
}

} // namespace libempp::subsys

#endif //__linux__
