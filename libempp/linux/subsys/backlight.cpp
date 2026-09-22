// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "backlight.h"

#ifdef __linux__
#include <sys/file.h>

namespace libempp::subsys
{

namespace fs = std::filesystem;

class LIBGS_DECL_HIDDEN backlight::impl
{
	LIBGS_DISABLE_COPY_MOVE(impl)

public:
	impl() = default;
	explicit impl(node_t node) {
		open(std::move(node));
	}
	~impl() {
		close();
	}

public:
	void open(const node_t &node, std::error_code &error) noexcept
	{
		close();
		error.clear();

		m_sys_dir = "/sys/class/backlight/" + node.name;
		std::error_code fs_error;

		if( not fs::exists(m_sys_dir, fs_error) )
		{
			error = fs_error ? fs_error :
				std::make_error_code(std::errc::no_such_device);
			return ;
		}
		if( not m_sys_dir.ends_with('/') )
			m_sys_dir += '/';
		do {
			m_fd_max_brightness = ::open (
				(m_sys_dir + "max_brightness").c_str(), O_RDONLY
			);
			if( m_fd_max_brightness < 0 )
				break;
			if( not read_attribute(m_fd_max_brightness, m_max_brightness, error) )
				break;

			m_fd_brightness = ::open((m_sys_dir + "brightness").c_str(), O_RDWR);
			if( m_fd_brightness < 0 )
				break;
			if( flock(m_fd_brightness, LOCK_EX | LOCK_NB) < 0 )
				break;
			if( not read_attribute(m_fd_brightness, m_brightness, error) )
				break;

			m_fd_actual_brightness = ::open (
				(m_sys_dir + "actual_brightness").c_str(), O_RDONLY
			);
			if( m_fd_actual_brightness < 0 )
				break;
			if( not read_attribute(m_fd_actual_brightness, m_actual_brightness, error) )
				break;

			m_fd_power = ::open((m_sys_dir + "bl_power").c_str(), O_RDWR);
			if( m_fd_power < 0 )
				break;
			if( flock(m_fd_power, LOCK_EX | LOCK_NB) < 0 )
				break;

			uint32_t power = 0;
			if( not read_attribute(m_fd_power, power, error) )
				break;
			if( power > static_cast<uint32_t>(power_t::power_down) )
			{
				error = std::make_error_code(std::errc::invalid_argument);
				break;
			}
			m_power = static_cast<power_t>(power);
			return ;
		}
		while(false);

		if( not error )
			error = std::error_code(errno, std::system_category());
		close();
	}

	void open(const node_t &node)
	{
		std::error_code error;
		open(node, error);
		if( error )
		{
			libgs::system_error::loc_throw (
				error, "libempp::subsys::backlight::open"
			);
		}
	}

	void close()
	{
		close_descriptor(m_fd_max_brightness);
		close_descriptor(m_fd_brightness);
		close_descriptor(m_fd_actual_brightness);
		close_descriptor(m_fd_power);

		m_sys_dir.clear();
		m_brightness = 0;
		m_actual_brightness = 0;
		m_max_brightness = 0;
		m_power = power_t::power_down;
	}

	static bool read_attribute(int fd, uint32_t &value, std::error_code &error) noexcept
	{
		error.clear();
		if( ::lseek(fd, 0, SEEK_SET) < 0 )
		{
			error = std::error_code(errno, std::system_category());
			return false;
		}
		char buffer[64] {};
		const auto size = ::read(fd, buffer, sizeof(buffer));
		if( size < 0 )
		{
			error = std::error_code(errno, std::system_category());
			return false;
		}
		const auto result = libgs::strtls::to_uint32 (
			std::string_view(buffer, static_cast<size_t>(size))
		);
		if( not result )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return false;
		}
		value = *result;
		return true;
	}

	static bool write_attribute(int fd, std::string_view value, std::error_code &error) noexcept
	{
		error.clear();
		if( ::lseek(fd, 0, SEEK_SET) < 0 )
		{
			error = std::error_code(errno, std::system_category());
			return false;
		}
		const auto size = ::write(fd, value.data(), value.size());
		if( size < 0 )
		{
			error = std::error_code(errno, std::system_category());
			return false;
		}
		if( static_cast<size_t>(size) != value.size() )
		{
			error = std::make_error_code(std::errc::io_error);
			return false;
		}
		return true;
	}

	static void close_descriptor(int &fd) noexcept
	{
		if( fd >= 0 )
		{
			::close(fd);
			fd = -1;
		}
	}

public:
	std::string m_sys_dir {};
	int m_fd_max_brightness = -1;
	int m_fd_brightness = -1;
	int m_fd_actual_brightness = -1;
	int m_fd_power = -1;

	brightness_t m_brightness = 0;
	brightness_t m_actual_brightness = 0;
	brightness_t m_max_brightness = 0;
	power_t m_power = power_t::power_down;
};

backlight::backlight(node_t node) :
	m_impl(new impl(std::move(node)))
{

}

backlight::backlight() :
	m_impl(new impl())
{

}

backlight::~backlight()
{
	delete m_impl;
}

backlight::backlight(backlight &&other) noexcept :
	m_impl(other.m_impl)
{
	other.m_impl = new impl();
}

backlight &backlight::operator=(backlight &&other) noexcept
{
	if( this == &other )
		return *this;
	delete m_impl;
	m_impl = other.m_impl;
	other.m_impl = new impl();
	return *this;
}

backlight &backlight::open(const node_t &node, std::error_code &error) noexcept
{
	m_impl->open(node, error);
	return *this;
}

backlight &backlight::open(const node_t &node)
{
	m_impl->open(node);
	return *this;
}

backlight &backlight::close()
{
	m_impl->close();
	return *this;
}

backlight &backlight::set_brightness(brightness_t brightness, std::error_code &error) noexcept
{
	error.clear();
	if( not is_open() )
	{
		error = std::make_error_code(std::errc::bad_file_descriptor);
		return *this;
	}
	if( brightness > m_impl->m_max_brightness )
	{
		error = std::make_error_code(std::errc::result_out_of_range);
		return *this;
	}
	const auto value = std::format("{}\n", brightness);
	if( not impl::write_attribute(m_impl->m_fd_brightness, value, error) )
		return *this;

	m_impl->m_brightness = brightness;
	return *this;
}

backlight &backlight::set_brightness(brightness_t brightness)
{
	std::error_code error;
	set_brightness(brightness, error);
	if( error )
	{
		libgs::system_error::loc_throw (
			error, "libempp::subsys::backlight::set_brightness"
		);
	}
	return *this;
}

backlight &backlight::set_power(power_t power, std::error_code &error) noexcept
{
	error.clear();
	if( not is_open() )
	{
		error = std::make_error_code(std::errc::bad_file_descriptor);
		return *this;
	}
	const auto raw_power = static_cast<uint32_t>(power);
	if( raw_power > static_cast<uint32_t>(power_t::power_down) )
	{
		error = std::make_error_code(std::errc::invalid_argument);
		return *this;
	}
	const auto value = std::format("{}\n", raw_power);
	if( not impl::write_attribute(m_impl->m_fd_power, value, error) )
		return *this;

	m_impl->m_power = power;
	return *this;
}

backlight &backlight::set_power(power_t power)
{
	std::error_code error;
	set_power(power, error);
	if( error )
	{
		libgs::system_error::loc_throw (
			error, "libempp::subsys::backlight::set_power"
		);
	}
	return *this;
}

backlight &backlight::set_enable(bool enable, std::error_code &error) noexcept
{
	return set_power(enable ? power_t::unblank : power_t::power_down, error);
}

backlight &backlight::set_enable(bool enable)
{
	return set_power(enable ? power_t::unblank : power_t::power_down);
}

backlight &backlight::enable(std::error_code &error) noexcept
{
	return set_enable(true, error);
}

backlight &backlight::enable()
{
	return set_enable(true);
}

backlight &backlight::disable(std::error_code &error) noexcept
{
	return set_enable(false, error);
}

backlight &backlight::disable()
{
	return set_enable(false);
}

backlight::brightness_t backlight::brightness() const noexcept
{
	return m_impl->m_brightness;
}

backlight::brightness_t backlight::actual_brightness() const noexcept
{
	if( is_open() )
	{
		std::error_code ignored;
		impl::read_attribute (
			m_impl->m_fd_actual_brightness, m_impl->m_actual_brightness,
			ignored
		);
	}
	return m_impl->m_actual_brightness;
}

backlight::brightness_t backlight::max_brightness() const noexcept
{
	return m_impl->m_max_brightness;
}

backlight::power_t backlight::power() const noexcept
{
	return m_impl->m_power;
}

bool backlight::is_enabled() const noexcept
{
	return m_impl->m_power == power_t::unblank;
}

bool backlight::is_open() const noexcept
{
	return m_impl->m_fd_max_brightness >= 0 and
		   m_impl->m_fd_brightness >= 0 and
		   m_impl->m_fd_actual_brightness >= 0 and
		   m_impl->m_fd_power >= 0;
}

} // namespace libempp::subsys

#endif //__linux__
