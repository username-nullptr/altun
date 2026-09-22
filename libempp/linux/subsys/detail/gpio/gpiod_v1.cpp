// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <libempp/linux/subsys/gpio.h>
#ifdef __linux__

#include <gpiod.h>

namespace libempp::subsys::detail { namespace
{

std::error_code system_error_from_errno() noexcept
{
	return { errno ? errno : EIO, std::system_category() };
}

std::filesystem::path chip_path(const std::filesystem::path &chip)
{
	if( chip.has_parent_path() )
		return chip;
	return std::filesystem::path("/dev") / chip;
}

int request_type(const gpio_node_t &node) noexcept
{
	switch(node.edge)
	{
	case gpio_edge_t::rising:
		return GPIOD_LINE_REQUEST_EVENT_RISING_EDGE;

	case gpio_edge_t::falling:
		return GPIOD_LINE_REQUEST_EVENT_FALLING_EDGE;

	case gpio_edge_t::both:
		return GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES;

	case gpio_edge_t::none:
		break;
	}
	return node.direction == gpio_direction_t::input ?
		GPIOD_LINE_REQUEST_DIRECTION_INPUT : GPIOD_LINE_REQUEST_DIRECTION_OUTPUT;
}

class gpiod_v1_backend final : public gpio_backend
{
public:
	~gpiod_v1_backend() override {
		close();
	}

	void open(const gpio_node_t &node, std::error_code &error) noexcept override
	{
		close();
		error.clear();

		const auto path = chip_path(node.chip);
		auto *chip = ::gpiod_chip_open(path.c_str());

		if( not chip )
		{
			error = system_error_from_errno();
			return ;
		}
		auto *line = ::gpiod_chip_get_line(chip, static_cast<unsigned int>(node.line));
		if( not line )
			error = system_error_from_errno();

		gpiod_line_request_config config {};
		config.consumer = node.consumer.empty() ? "libempp" : node.consumer.c_str();
		config.flags = node.active_low ? GPIOD_LINE_REQUEST_FLAG_ACTIVE_LOW : 0;
		config.request_type = request_type(node);

		if( not error and ::gpiod_line_request(line, &config, node.initial_value ? 1 : 0) < 0 )
			error = system_error_from_errno();
		if( error )
		{
			::gpiod_chip_close(chip);
			return ;
		}
		m_chip = chip;
		m_line = line;
	}

	void close() noexcept override
	{
		if( m_line )
			::gpiod_line_release(m_line);

		if( m_chip )
			::gpiod_chip_close(m_chip);

		m_line = nullptr;
		m_chip = nullptr;
	}

	[[nodiscard]] bool is_open() const noexcept override {
		return m_line != nullptr;
	}

	[[nodiscard]] bool value(std::error_code &error) const noexcept override
	{
		error.clear();
		const int result = ::gpiod_line_get_value(m_line);

		if( result < 0 )
		{
			error = system_error_from_errno();
			return false;
		}
		return result != 0;
	}

	void set_value(bool value, std::error_code &error) noexcept override
	{
		error.clear();
		if( ::gpiod_line_set_value(m_line, value ? 1 : 0) < 0 )
			error = system_error_from_errno();
	}

	[[nodiscard]] int event_handle() const noexcept override
	{
		return m_line ? ::gpiod_line_event_get_fd(m_line) : -1;
	}

	[[nodiscard]] gpio_event_wait_t event_wait_type() const noexcept override
	{
		return gpio_event_wait_t::readable;
	}

	[[nodiscard]] bool wait_event
	(gpio_event_t &event, gpio_duration_t timeout, std::error_code &error) noexcept override
	{
		event = {};
		error.clear();

		timespec native_timeout {};
		const timespec *timeout_pointer = nullptr;

		if( timeout.count() >= 0 )
		{
			native_timeout.tv_sec = static_cast<time_t>(timeout.count() / 1000);
			native_timeout.tv_nsec = (timeout.count() % 1000) * 1'000'000;
			timeout_pointer = &native_timeout;
		}
		const int pending = ::gpiod_line_event_wait(m_line, timeout_pointer);
		if( pending < 0 )
		{
			error = system_error_from_errno();
			return false;
		}
		if( pending == 0 )
			return false;

		gpiod_line_event native_event {};
		if( ::gpiod_line_event_read(m_line, &native_event) < 0 )
		{
			error = system_error_from_errno();
			return false;
		}
		event.edge = native_event.event_type == GPIOD_LINE_EVENT_RISING_EDGE ?
			gpio_edge_t::rising : gpio_edge_t::falling;

		event.timestamp_ns = static_cast<uint64_t>(native_event.ts.tv_sec) *
			1'000'000'000ULL + static_cast<uint64_t>(native_event.ts.tv_nsec);
		return true;
	}

private:
	gpiod_chip *m_chip = nullptr;
	gpiod_line *m_line = nullptr;
};

} // namespace

std::unique_ptr<gpio_backend> make_gpio_backend(std::error_code &error) noexcept
{
	error.clear();
	auto result = std::unique_ptr<gpio_backend>(std::make_unique<gpiod_v1_backend>());
	if( not result )
		error = std::make_error_code(std::errc::not_enough_memory);
	return result;
}

gpio_backend_t gpio_backend_type() noexcept
{
	return gpio_backend_t::libgpiod;
}

const char *gpio_backend_name() noexcept
{
	return "libgpiod";
}

} // namespace libempp::subsys::detail

#endif //__linux__
