// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "backend.h"
#ifdef __linux__

#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

namespace altun::subsys::detail { namespace
{

namespace fs = std::filesystem;

std::error_code system_error_from_errno() noexcept {
	return { errno != 0 ? errno : EIO, std::system_category() };
}

bool write_attribute(int descriptor, std::string_view value, std::error_code &error) noexcept
{
	error.clear();
	if( ::lseek(descriptor, 0, SEEK_SET) < 0 )
	{
		error = system_error_from_errno();
		return false;
	}
	ssize_t size = -1;
	do {
		size = ::write(descriptor, value.data(), value.size());
	}
	while( size < 0 and errno == EINTR );

	if( size < 0 )
	{
		error = system_error_from_errno();
		return false;
	}
	if( static_cast<size_t>(size) != value.size() )
	{
		error = std::make_error_code(std::errc::io_error);
		return false;
	}
	return true;
}

bool write_uint_attribute(int descriptor, uint32_t value, std::error_code &error) noexcept
{
	std::array<char, 16> buffer {};
	const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size() - 1, value);

	if( result.ec != std::errc() )
	{
		error = std::make_error_code(result.ec);
		return false;
	}
	*result.ptr = '\n';
	return write_attribute(descriptor,
		{buffer.data(), static_cast<size_t>(result.ptr - buffer.data() + 1)}, error
	);
}

bool read_attribute(int descriptor, std::string &value, std::error_code &error) noexcept
{
	error.clear();
	if( ::lseek(descriptor, 0, SEEK_SET) < 0 )
	{
		error = system_error_from_errno();
		return false;
	}
	char buffer[64] {};
	ssize_t size = -1;
	do {
		size = ::read(descriptor, buffer, sizeof(buffer));
	}
	while(size < 0 and errno == EINTR);

	if( size < 0 )
	{
		error = system_error_from_errno();
		return false;
	}
	try {
		value.assign(buffer, static_cast<size_t>(size));
	}
	catch(const std::bad_alloc&)
	{
		error = std::make_error_code(std::errc::not_enough_memory);
		return false;
	}
	return true;
}

class sysfs_pwm_backend final : public pwm_backend
{
public:
	~sysfs_pwm_backend() override {
		close();
	}

	void open(const pwm::node_t &node, std::error_code &error) noexcept override
	{
		close();
		error.clear();
		try {
			const fs::path configured_chip(node.chip);
			m_chip_dir = configured_chip.has_parent_path() ?
				configured_chip : fs::path("/sys/class/pwm") / configured_chip;

			m_channel = node.channel;
			std::error_code fs_error;

			if( not fs::exists(m_chip_dir, fs_error) )
			{
				error = fs_error ? fs_error : std::make_error_code(std::errc::no_such_device);
				return ;
			}
			const auto channel_text = std::format (
				"{}\n", static_cast<unsigned int>(node.channel)
			);
			const int export_descriptor = ::open (
				(m_chip_dir / "export").c_str(), O_WRONLY | O_CLOEXEC
			);
			if( export_descriptor < 0 )
			{
				error = system_error_from_errno();
				return ;
			}
			ssize_t export_size = -1;
			do {
				export_size = ::write(export_descriptor,
					channel_text.data(), channel_text.size()
				);
			}
			while( export_size < 0 and errno == EINTR );

			const int export_errno = errno;
			::close(export_descriptor);

			if( export_size < 0 and export_errno != EBUSY )
			{
				error = {
					export_errno != 0 ? export_errno : EIO,
					std::system_category()
				};
				return ;
			}
			if( export_size >= 0 and static_cast<size_t>(export_size) != channel_text.size() )
			{
				error = std::make_error_code(std::errc::io_error);
				return ;
			}
			m_exported = export_size >= 0;
			m_pwm_dir = m_chip_dir / std::format (
				"pwm{}", static_cast<unsigned int>(node.channel)
			);
			if( not fs::exists(m_pwm_dir, fs_error) )
			{
				error = fs_error ? fs_error : std::make_error_code(std::errc::operation_not_supported);
				close();
				return ;
			}
			m_period_descriptor = open_attribute("period", error);
			if( error )
				return close();

			m_duty_descriptor = open_attribute("duty_cycle", error);
			if( error )
				return close();

			m_enable_descriptor = open_attribute("enable", error);
			if( error )
				return close();

			std::string text;
			if( not read_attribute(m_period_descriptor, text, error) )
				return close();

			const auto period = riwo::strtls::to_uint32(text);
			if( not period )
			{
				error = std::make_error_code(std::errc::invalid_argument);
				return close();
			}
			m_period = *period;

			if( not read_attribute(m_duty_descriptor, text, error) )
				return close();

			const auto duty_cycle = riwo::strtls::to_uint32(text);
			if( not duty_cycle )
			{
				error = std::make_error_code(std::errc::invalid_argument);
				return close();
			}
			m_duty_cycle = *duty_cycle;

			if( not read_attribute(m_enable_descriptor, text, error) )
				return close();

			const auto enabled = riwo::strtls::to_bool(text);
			if( not enabled )
			{
				error = std::make_error_code(std::errc::invalid_argument);
				return close();
			}
			m_enabled = *enabled;
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
		close_descriptor(m_period_descriptor);
		close_descriptor(m_duty_descriptor);
		close_descriptor(m_enable_descriptor);
		if( m_exported )
		{
			try {
				const auto text = std::format (
					"{}\n", static_cast<unsigned int>(m_channel)
				);
				const int descriptor = ::open (
					(m_chip_dir / "unexport").c_str(), O_WRONLY | O_CLOEXEC
				);
				if( descriptor >= 0 )
				{
					static_cast<void>(::write(descriptor, text.data(), text.size()));
					::close(descriptor);
				}
			}
			catch(...) {}
		}
		m_chip_dir.clear();
		m_pwm_dir.clear();

		m_channel = 0;
		m_exported = false;

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
		if( write_uint_attribute(m_period_descriptor, period, error) )
			m_period = period;
	}

	void set_duty_cycle(uint32_t duty_cycle, std::error_code &error) noexcept override
	{
		error.clear();
		if( duty_cycle > m_period )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return ;
		}
		if( write_uint_attribute(m_duty_descriptor, duty_cycle, error) )
			m_duty_cycle = duty_cycle;
	}

	void set(uint32_t period, uint32_t duty_cycle, std::error_code &error) noexcept override
	{
		error.clear();
		if( duty_cycle > period )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return ;
		}
		if( period < m_duty_cycle )
		{
			set_duty_cycle(duty_cycle, error);
			if( not error )
				set_period(period, error);
		}
		else
		{
			set_period(period, error);
			if( not error )
				set_duty_cycle(duty_cycle, error);
		}
	}

	void set_enable(bool enable, std::error_code &error) noexcept override
	{
		if( write_attribute(m_enable_descriptor, enable ? "1\n" : "0\n", error) )
			m_enabled = enable;
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
		return m_period_descriptor >= 0 and m_duty_descriptor >= 0 and
			m_enable_descriptor >= 0;
	}

private:
	int open_attribute(const char *name, std::error_code &error)
	{
		const int descriptor = ::open (
			(m_pwm_dir / name).c_str(), O_RDWR | O_CLOEXEC
		);
		if( descriptor < 0 )
			error = system_error_from_errno();

		else if( ::flock(descriptor, LOCK_EX | LOCK_NB) < 0 )
		{
			error = system_error_from_errno();
			::close(descriptor);
			return -1;
		}
		return descriptor;
	}

	static void close_descriptor(int &descriptor) noexcept
	{
		if( descriptor >= 0 )
			::close(descriptor);
		descriptor = -1;
	}

private:
	fs::path m_chip_dir;
	fs::path m_pwm_dir;

	pwm::channel_t m_channel = 0;
	bool m_exported = false;

	int m_period_descriptor = -1;
	int m_duty_descriptor = -1;
	int m_enable_descriptor = -1;

	uint32_t m_period = 0;
	uint32_t m_duty_cycle = 0;

	bool m_enabled = false;
};

} // namespace

std::unique_ptr<pwm_backend> make_pwm_backend(std::error_code &error) noexcept
{
	error.clear();
	auto result = std::unique_ptr<pwm_backend>(new(std::nothrow) sysfs_pwm_backend());
	if( not result )
		error = std::make_error_code(std::errc::not_enough_memory);
	return result;
}

pwm::backend_t pwm_backend_type() noexcept
{
	return pwm::backend_t::sysfs;
}

const char *pwm_backend_name() noexcept
{
	return "sysfs";
}

} // namespace altun::subsys::detail

#endif //__linux__
