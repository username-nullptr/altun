// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "led.h"

#ifdef __linux__
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

namespace libempp::subsys
{

namespace fs = std::filesystem;

class LIBGS_DECL_HIDDEN led::impl
{
	LIBGS_DISABLE_COPY_MOVE(impl)

public:
	impl() = default;
	~impl() {
		close();
	}

public:
	static bool read_text(int descriptor, std::string &value,
		std::error_code &error) noexcept
	{
		error.clear();
		if( ::lseek(descriptor, 0, SEEK_SET) < 0 )
		{
			error = std::error_code(errno, std::system_category());
			return false;
		}
		char buffer[4096] {};
		ssize_t size = -1;
		do {
			size = ::read(descriptor, buffer, sizeof(buffer));
		}
		while( size < 0 and errno == EINTR );

		if( size < 0 )
		{
			error = std::error_code(errno, std::system_category());
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

	static bool read_brightness(int descriptor, brightness_t &value, std::error_code &error) noexcept
	{
		std::string text;
		if( not read_text(descriptor, text, error) )
			return false;

		const auto result = libgs::strtls::to_uint32(text);
		if( not result )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return false;
		}
		value = *result;
		return true;
	}

	static bool write_text(int descriptor, std::string_view value, std::error_code &error) noexcept
	{
		error.clear();
		if( ::lseek(descriptor, 0, SEEK_SET) < 0 )
		{
			error = std::error_code(errno, std::system_category());
			return false;
		}
		ssize_t size = -1;
		do {
			size = ::write(descriptor, value.data(), value.size());
		}
		while( size < 0 and errno == EINTR );

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

	static bool write_file(const fs::path &path, std::string_view value, std::error_code &error) noexcept
	{
		const int descriptor = ::open(path.c_str(), O_WRONLY | O_CLOEXEC);
		if( descriptor < 0 )
		{
			error = std::error_code(errno, std::system_category());
			return false;
		}
		const bool result = write_text(descriptor, value, error);
		const int saved_errno = errno;

		::close(descriptor);
		errno = saved_errno;
		return result;
	}

	void open(const node_t &node, std::error_code &error) noexcept
	{
		close();
		error.clear();

		if( node.name.empty() or fs::path(node.name).filename() != node.name )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return ;
		}
		m_sys_dir = fs::path("/sys/class/leds") / node.name;
		std::error_code fs_error;

		if( not fs::exists(m_sys_dir, fs_error) )
		{
			error = fs_error ? fs_error :
				std::make_error_code(std::errc::no_such_device);
			m_sys_dir.clear();
			return ;
		}
		do {
			m_fd_max_brightness = ::open (
				(m_sys_dir / "max_brightness").c_str(), O_RDONLY | O_CLOEXEC
			);
			if( m_fd_max_brightness < 0 )
				break;
			if( not read_brightness(m_fd_max_brightness, m_max_brightness, error) )
				break;

			m_fd_brightness = ::open (
				(m_sys_dir / "brightness").c_str(), O_RDWR | O_CLOEXEC
			);
			if( m_fd_brightness < 0 )
				break;

			if( ::flock(m_fd_brightness, LOCK_EX | LOCK_NB) < 0 )
				break;

			if( not read_brightness(m_fd_brightness, m_brightness, error) )
				break;

			fs_error.clear();
			m_has_trigger = fs::exists(m_sys_dir / "trigger", fs_error);
			if( fs_error )
			{
				error = fs_error;
				break;
			}
			return ;
		}
		while(false);

		if( not error )
			error = std::error_code(errno, std::system_category());
		close();
	}

	void close() noexcept
	{
		if( m_fd_max_brightness >= 0 )
			::close(m_fd_max_brightness);

		if( m_fd_brightness >= 0 )
			::close(m_fd_brightness);

		m_fd_max_brightness = -1;
		m_fd_brightness = -1;
		m_sys_dir.clear();

		m_brightness = 0;
		m_max_brightness = 0;
		m_has_trigger = false;
	}

public:
	fs::path m_sys_dir {};
	int m_fd_max_brightness = -1;
	int m_fd_brightness = -1;

	mutable brightness_t m_brightness = 0;
	brightness_t m_max_brightness = 0;

	bool m_has_trigger = false;
};

led::led(const node_t &node) :
	m_impl(std::make_unique<impl>())
{
	std::error_code error;
	m_impl->open(node, error);
	if( error )
		libgs::system_error::loc_throw(error, "libempp::subsys::led::open");
}

led::led() :
	m_impl(std::make_unique<impl>())
{

}

led::~led() = default;

led::led(led &&other) noexcept = default;

led &led::operator=(led &&other) noexcept = default;

led &led::open(const node_t &node, std::error_code &error) noexcept
{
	if( not m_impl )
	{
		m_impl.reset(new(std::nothrow) impl());
		if( not m_impl )
		{
			error = std::make_error_code(std::errc::not_enough_memory);
			return *this;
		}
	}
	m_impl->open(node, error);
	return *this;
}

led &led::open(const node_t &node)
{
	std::error_code error;
	open(node, error);
	if( error )
		libgs::system_error::loc_throw(error, "libempp::subsys::led::open");
	return *this;
}

led &led::close() noexcept
{
	if( m_impl )
		m_impl->close();
	return *this;
}

led &led::set_brightness(brightness_t brightness, std::error_code &error) noexcept
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
	const auto text = std::format("{}\n", brightness);
	if( impl::write_text(m_impl->m_fd_brightness, text, error) )
		m_impl->m_brightness = brightness;
	return *this;
}

led &led::set_brightness(brightness_t brightness)
{
	std::error_code error;
	set_brightness(brightness, error);
	if( error )
		libgs::system_error::loc_throw(error, "libempp::subsys::led::set_brightness");
	return *this;
}

led &led::set_trigger(std::string_view trigger, std::error_code &error) noexcept
{
	error.clear();
	if( not is_open() )
	{
		error = std::make_error_code(std::errc::bad_file_descriptor);
		return *this;
	}
	if( not m_impl->m_has_trigger )
	{
		error = std::make_error_code(std::errc::operation_not_supported);
		return *this;
	}
	if( trigger.empty() or trigger.find_first_of(" \t\r\n") !=
		std::string_view::npos )
	{
		error = std::make_error_code(std::errc::invalid_argument);
		return *this;
	}
	const auto text = std::format("{}\n", trigger);
	impl::write_file(m_impl->m_sys_dir / "trigger", text, error);
	return *this;
}

led &led::set_trigger(std::string_view trigger)
{
	std::error_code error;
	set_trigger(trigger, error);
	if( error )
		libgs::system_error::loc_throw(error, "libempp::subsys::led::set_trigger");
	return *this;
}

led &led::set_blink(duration_t delay_on, duration_t delay_off, std::error_code &error) noexcept
{
	error.clear();
	if( delay_on.count() < 0 or delay_off.count() < 0 )
	{
		error = std::make_error_code(std::errc::invalid_argument);
		return *this;
	}
	set_trigger("timer", error);
	if( error )
		return *this;

	const auto on_text = std::format("{}\n", delay_on.count());
	if( not impl::write_file(m_impl->m_sys_dir / "delay_on", on_text, error) )
		return *this;

	const auto off_text = std::format("{}\n", delay_off.count());
	impl::write_file(m_impl->m_sys_dir / "delay_off", off_text, error);
	return *this;
}

led &led::set_blink(duration_t delay_on, duration_t delay_off)
{
	std::error_code error;
	set_blink(delay_on, delay_off, error);
	if( error )
		libgs::system_error::loc_throw(error, "libempp::subsys::led::set_blink");
	return *this;
}

led::brightness_t led::brightness() const noexcept
{
	if( is_open() )
	{
		std::error_code ignored;
		impl::read_brightness(
			m_impl->m_fd_brightness, m_impl->m_brightness, ignored
		);
	}
	return m_impl ? m_impl->m_brightness : 0;
}

led::brightness_t led::max_brightness() const noexcept
{
	return m_impl ? m_impl->m_max_brightness : 0;
}

std::string led::trigger(std::error_code &error) const noexcept
{
	error.clear();
	if( not is_open() )
	{
		error = std::make_error_code(std::errc::bad_file_descriptor);
		return {};
	}
	if( not m_impl->m_has_trigger )
	{
		error = std::make_error_code(std::errc::operation_not_supported);
		return {};
	}
	const int descriptor = ::open (
		(m_impl->m_sys_dir / "trigger").c_str(), O_RDONLY | O_CLOEXEC
	);
	if( descriptor < 0 )
	{
		error = std::error_code(errno, std::system_category());
		return {};
	}
	std::string text;
	impl::read_text(descriptor, text, error);
	::close(descriptor);

	if( error )
		return {};
	try {
		const auto begin = text.find('[');
		const auto end = begin == std::string::npos ?
			std::string::npos : text.find(']', begin + 1);

		if( begin == std::string::npos or end == std::string::npos )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return {};
		}
		return text.substr(begin + 1, end - begin - 1);
	}
	catch(const std::bad_alloc&)
	{
		error = std::make_error_code(std::errc::not_enough_memory);
		return {};
	}
}

std::string led::trigger() const
{
	std::error_code error;
	auto result = trigger(error);
	if( error )
		libgs::system_error::loc_throw(error, "libempp::subsys::led::trigger");
	return result;
}

std::vector<std::string> led::triggers(std::error_code &error) const noexcept
{
	error.clear();
	if( not is_open() )
	{
		error = std::make_error_code(std::errc::bad_file_descriptor);
		return {};
	}
	if( not m_impl->m_has_trigger )
	{
		error = std::make_error_code(std::errc::operation_not_supported);
		return {};
	}
	const int descriptor = ::open (
		(m_impl->m_sys_dir / "trigger").c_str(), O_RDONLY | O_CLOEXEC
	);
	if( descriptor < 0 )
	{
		error = std::error_code(errno, std::system_category());
		return {};
	}
	std::string text;
	impl::read_text(descriptor, text, error);
	::close(descriptor);

	if( error )
		return {};
	try {
		std::istringstream stream(text);
		std::vector<std::string> result;
		for(std::string item; stream >> item; )
		{
			if( item.size() >= 2 and item.front() == '[' and item.back() == ']' )
				item = item.substr(1, item.size() - 2);
			result.emplace_back(std::move(item));
		}
		return result;
	}
	catch(const std::bad_alloc&)
	{
		error = std::make_error_code(std::errc::not_enough_memory);
		return {};
	}
}

std::vector<std::string> led::triggers() const
{
	std::error_code error;
	auto result = triggers(error);
	if( error )
		libgs::system_error::loc_throw(error, "libempp::subsys::led::triggers");
	return result;
}

bool led::supports_triggers() const noexcept
{
	return m_impl and m_impl->m_has_trigger;
}

bool led::is_open() const noexcept
{
	return m_impl and m_impl->m_fd_max_brightness >= 0 and
		m_impl->m_fd_brightness >= 0;
}

} // namespace libempp::subsys

#endif //__linux__
