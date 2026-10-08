// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <altun/linux/subsys/gpio.h>
#ifdef __linux__

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

namespace altun::subsys::detail { namespace
{

namespace fs = std::filesystem;

std::error_code system_error_from_errno() noexcept
{
	return { errno ? errno : EIO, std::system_category() };
}

bool read_descriptor(int descriptor, std::string &value, std::error_code &error) noexcept
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
	while( size < 0 and errno == EINTR );
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

bool write_descriptor(int descriptor, std::string_view value, std::error_code &error) noexcept
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

bool write_file(const fs::path &path, std::string_view value, std::error_code &error) noexcept
{
	const int descriptor = ::open(path.c_str(), O_WRONLY | O_CLOEXEC);
	if( descriptor < 0 )
	{
		error = system_error_from_errno();
		return false;
	}
	const bool result = write_descriptor(descriptor, value, error);
	::close(descriptor);
	return result;
}

bool read_uint_file(const fs::path &path, uint32_t &value, std::error_code &error) noexcept
{
	const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
	if( descriptor < 0 )
	{
		error = system_error_from_errno();
		return false;
	}
	std::string text;
	const bool read = read_descriptor(descriptor, text, error);
	::close(descriptor);

	if( not read )
		return false;

	const auto result = riwo::strtls::to_uint32(text);
	if( not result )
	{
		error = std::make_error_code(std::errc::invalid_argument);
		return false;
	}
	value = *result;
	return true;
}

std::string_view edge_name(gpio_edge_t edge) noexcept
{
	switch(edge)
	{
	case gpio_edge_t::none   : return "none\n"   ;
	case gpio_edge_t::rising : return "rising\n" ;
	case gpio_edge_t::falling: return "falling\n";
	case gpio_edge_t::both   : return "both\n"   ;
	}
	return "none\n";
}

class sysfs_backend final : public gpio_backend
{
public:
	~sysfs_backend() override {
		close();
	}

	void open(const gpio_node_t &node, std::error_code &error) noexcept override
	{
		close();
		error.clear();

		const fs::path class_dir = "/sys/class/gpio";
		const auto chip_name = node.chip.filename();

		if( chip_name.empty() )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return ;
		}
		const auto chip_dir = class_dir / chip_name;
		std::error_code fs_error;

		if( not fs::exists(chip_dir, fs_error) )
		{
			error = fs_error ? fs_error :
				std::make_error_code(std::errc::no_such_device);
			return ;
		}
		uint32_t base = 0;
		uint32_t line_count = 0;

		if( not read_uint_file(chip_dir / "base", base, error) or
			not read_uint_file(chip_dir / "ngpio", line_count, error) )
			return ;

		if( node.line >= line_count or base > std::numeric_limits<uint32_t>::max() - node.line )
		{
			error = std::make_error_code(std::errc::result_out_of_range);
			return ;
		}
		m_global_line = base + node.line;
		m_line_dir = class_dir / std::format("gpio{}", m_global_line);
		fs_error.clear();

		if( not fs::exists(m_line_dir, fs_error) )
		{
			if( fs_error )
			{
				error = fs_error;
				return ;
			}
			if( not write_file(class_dir / "export", std::format("{}\n", m_global_line), error) )
			{
				if( error.value() != EBUSY )
					return ;
				error.clear();
			}
			else
				m_exported = true;

			for(unsigned int retry = 0; retry < 100; ++retry)
			{
				fs_error.clear();
				if( fs::exists(m_line_dir, fs_error) or fs_error )
					break;
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			if( fs_error or not fs::exists(m_line_dir, fs_error) )
			{
				error = fs_error ? fs_error :
					std::make_error_code(std::errc::timed_out);
				close();
				return ;
			}
		}

		if( not write_file(m_line_dir / "active_low", node.active_low ? "1\n" : "0\n", error) )
		{
			close();
			return ;
		}
		const bool raw_initial_value = node.initial_value != node.active_low;

		const std::string_view direction = node.direction == gpio_direction_t::input ?
			"in\n" : (raw_initial_value ? "high\n" : "low\n");

		if( not write_file(m_line_dir / "direction", direction, error) )
		{
			close();
			return ;
		}
		if( node.direction == gpio_direction_t::input and
			not write_file(m_line_dir / "edge", edge_name(node.edge), error) )
		{
			close();
			return ;
		}
		m_value_fd = ::open((m_line_dir / "value").c_str(),
			(node.direction == gpio_direction_t::output ? O_RDWR : O_RDONLY) |
				O_NONBLOCK | O_CLOEXEC
		);
		if( m_value_fd < 0 )
		{
			error = system_error_from_errno();
			close();
			return ;
		}
		m_last_value = read_value(error);
		if( error )
			close();
	}

	void close() noexcept override
	{
		if( m_value_fd >= 0 )
			::close(m_value_fd);
		m_value_fd = -1;

		if( m_exported )
		{
			std::error_code ignored;
			write_file(
				fs::path("/sys/class/gpio/unexport"),
				std::format("{}\n", m_global_line), ignored
			);
		}
		m_exported = false;
		m_global_line = 0;
		m_line_dir.clear();
		m_last_value = false;
	}

	[[nodiscard]] bool is_open() const noexcept override
	{
		return m_value_fd >= 0;
	}

	[[nodiscard]] bool value(std::error_code &error) const noexcept override
	{
		return read_value(error);
	}

	void set_value(bool value, std::error_code &error) noexcept override
	{
		if( write_descriptor(m_value_fd, value ? "1\n" : "0\n", error) )
			m_last_value = value;
	}

	[[nodiscard]] int event_handle() const noexcept override
	{
		return m_value_fd;
	}

	[[nodiscard]] gpio_event_wait_t event_wait_type() const noexcept override
	{
		return gpio_event_wait_t::priority;
	}

	[[nodiscard]] bool wait_event
	(gpio_event_t &event, gpio_duration_t timeout, std::error_code &error) noexcept override
	{
		event = {};
		error.clear();

		pollfd poll_descriptor {
			.fd = m_value_fd,
			.events = POLLPRI | POLLERR,
			.revents = 0
		};
		const auto clamped_timeout = timeout.count() < 0 ?
			-1 : static_cast<int>(std::min<int64_t>(timeout.count(), INT_MAX));

		int result = -1;
		do {
			result = ::poll(&poll_descriptor, 1, clamped_timeout);
		}
		while( result < 0 and errno == EINTR );

		if( result < 0 )
		{
			error = system_error_from_errno();
			return false;
		}
		if( result == 0 )
			return false;

		if( poll_descriptor.revents & POLLNVAL )
		{
			error = std::make_error_code(std::errc::bad_file_descriptor);
			return false;
		}
		const bool previous = m_last_value;
		const bool current = read_value(error);

		if( error )
			return false;

		event.edge = current and not previous ?
			gpio_edge_t::rising : gpio_edge_t::falling;

		event.timestamp_ns = static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now().time_since_epoch()
			).count()
		);
		return true;
	}

private:
	[[nodiscard]] bool read_value(std::error_code &error) const noexcept
	{
		std::string text;
		if( not read_descriptor(m_value_fd, text, error) )
			return false;

		const auto result = riwo::strtls::to_bool(text);
		if( not result )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return false;
		}
		m_last_value = *result;
		return *result;
	}

private:
	fs::path m_line_dir {};

	int m_value_fd = -1;
	uint32_t m_global_line = 0;

	bool m_exported = false;
	mutable bool m_last_value = false;
};

} // namespace

std::unique_ptr<gpio_backend> make_gpio_backend(std::error_code &error) noexcept
{
	error.clear();
	auto result = std::unique_ptr<gpio_backend>(new(std::nothrow) sysfs_backend());
	if( not result )
		error = std::make_error_code(std::errc::not_enough_memory);
	return result;
}

gpio_backend_t gpio_backend_type() noexcept
{
	return gpio_backend_t::sysfs;
}

const char *gpio_backend_name() noexcept
{
	return "sysfs";
}

} // namespace altun::subsys::detail

#endif //__linux__
