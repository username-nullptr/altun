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

enum gpiod_line_edge edge_type(gpio_edge_t edge) noexcept
{
	switch(edge)
	{
	case gpio_edge_t::none   : return GPIOD_LINE_EDGE_NONE   ;
	case gpio_edge_t::rising : return GPIOD_LINE_EDGE_RISING ;
	case gpio_edge_t::falling: return GPIOD_LINE_EDGE_FALLING;
	case gpio_edge_t::both   : return GPIOD_LINE_EDGE_BOTH   ;
	}
	return GPIOD_LINE_EDGE_NONE;
}

int64_t timeout_nanoseconds(gpio_duration_t timeout) noexcept
{
	if( timeout.count() < 0 )
		return -1;

	constexpr int64_t multiplier = 1'000'000;
	if( timeout.count() > std::numeric_limits<int64_t>::max() / multiplier )
		return std::numeric_limits<int64_t>::max();

	return timeout.count() * multiplier;
}

class gpiod_v2_backend final : public gpio_backend
{
public:
	~gpiod_v2_backend() override {
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
		auto *settings = ::gpiod_line_settings_new();
		auto *line_config = ::gpiod_line_config_new();
		auto *request_config = ::gpiod_request_config_new();

		if( not settings or not line_config or not request_config )
			error = std::make_error_code(std::errc::not_enough_memory);

		if( not error and
			::gpiod_line_settings_set_direction
			(settings, node.direction == gpio_direction_t::input ?
				GPIOD_LINE_DIRECTION_INPUT : GPIOD_LINE_DIRECTION_OUTPUT) < 0
		  )
			error = system_error_from_errno();

		if( not error )
			::gpiod_line_settings_set_active_low(settings, node.active_low);

		if( not error and node.direction == gpio_direction_t::output and
			::gpiod_line_settings_set_output_value
			(settings, node.initial_value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE) < 0 )
			error = system_error_from_errno();

		if( not error and ::gpiod_line_settings_set_edge_detection(settings, edge_type(node.edge)) < 0 )
			error = system_error_from_errno();

		const auto offset = static_cast<unsigned int>(node.line);
		if( not error and ::gpiod_line_config_add_line_settings(line_config, &offset, 1, settings) < 0 )
			error = system_error_from_errno();

		if( not error )
		{
			::gpiod_request_config_set_consumer (
				request_config, node.consumer.empty() ?
					"libempp" : node.consumer.c_str()
			);
			m_request = ::gpiod_chip_request_lines (
				chip, request_config, line_config
			);
			if( not m_request )
				error = system_error_from_errno();
		}
		if( not error and node.edge != gpio_edge_t::none )
		{
			m_event_buffer = ::gpiod_edge_event_buffer_new(1);
			if( not m_event_buffer )
				error = std::make_error_code(std::errc::not_enough_memory);
		}
		::gpiod_request_config_free(request_config);
		::gpiod_line_config_free(line_config);
		::gpiod_line_settings_free(settings);

		if( error )
		{
			if( m_event_buffer )
				::gpiod_edge_event_buffer_free(m_event_buffer);

			if( m_request )
				::gpiod_line_request_release(m_request);

			m_event_buffer = nullptr;
			m_request = nullptr;

			::gpiod_chip_close(chip);
			return ;
		}
		m_chip = chip;
		m_line = node.line;
	}

	void close() noexcept override
	{
		if( m_event_buffer )
			::gpiod_edge_event_buffer_free(m_event_buffer);

		if( m_request )
			::gpiod_line_request_release(m_request);

		if( m_chip )
			::gpiod_chip_close(m_chip);

		m_event_buffer = nullptr;
		m_request = nullptr;
		m_chip = nullptr;
		m_line = 0;
	}

	[[nodiscard]] bool is_open() const noexcept override {
		return m_request != nullptr;
	}

	[[nodiscard]] bool value(std::error_code &error) const noexcept override
	{
		error.clear();
		const auto result = ::gpiod_line_request_get_value (
			m_request, static_cast<unsigned int>(m_line)
		);
		if( result == GPIOD_LINE_VALUE_ERROR )
		{
			error = system_error_from_errno();
			return false;
		}
		return result == GPIOD_LINE_VALUE_ACTIVE;
	}

	void set_value(bool value, std::error_code &error) noexcept override
	{
		error.clear();
		if( ::gpiod_line_request_set_value(m_request, static_cast<unsigned int>(m_line),
			value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE) < 0 )
			error = system_error_from_errno();
	}

	[[nodiscard]] int event_handle() const noexcept override
	{
		return m_request ? ::gpiod_line_request_get_fd(m_request) : -1;
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

		const int pending = ::gpiod_line_request_wait_edge_events (
			m_request, timeout_nanoseconds(timeout)
		);
		if( pending < 0 )
		{
			error = system_error_from_errno();
			return false;
		}
		if( pending == 0 )
			return false;

		const int count = ::gpiod_line_request_read_edge_events (
			m_request, m_event_buffer, 1
		);
		if( count < 0 )
		{
			error = system_error_from_errno();
			return false;
		}
		if( count == 0 )
		{
			error = std::make_error_code(std::errc::io_error);
			return false;
		}
		auto *native_event = ::gpiod_edge_event_buffer_get_event(m_event_buffer, 0);
		if( not native_event )
		{
			error = std::make_error_code(std::errc::io_error);
			return false;
		}
		event.edge = ::gpiod_edge_event_get_event_type(native_event) == GPIOD_EDGE_EVENT_RISING_EDGE ?
			gpio_edge_t::rising : gpio_edge_t::falling;

		event.timestamp_ns = ::gpiod_edge_event_get_timestamp_ns(native_event);
		return true;
	}

private:
	gpiod_chip *m_chip = nullptr;
	struct gpiod_line_request *m_request = nullptr;
	struct gpiod_edge_event_buffer *m_event_buffer = nullptr;
	gpio_line_t m_line = 0;
};

} // namespace

std::unique_ptr<gpio_backend> make_gpio_backend(std::error_code &error) noexcept
{
	error.clear();
	auto result = std::unique_ptr<gpio_backend>(new(std::nothrow) gpiod_v2_backend());
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
