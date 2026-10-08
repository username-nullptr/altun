// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "gpio.h"

#include <array>
#include <cerrno>
#include <climits>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#ifdef ALTUN_TEST_GPIOD_V1

struct gpiod_chip
{
	std::size_t index = 0;
};
struct gpiod_line
{
	unsigned int offset = 0;
	int event_pipe[2] {-1, -1};
	bool value = false;
	bool waiting = false;
};
struct gpiod_line_request_config;
struct gpiod_line_event
{
	timespec ts;
	int event_type;
};

namespace
{

std::mutex virtual_gpio_mutex;
std::condition_variable virtual_gpio_ready;
std::array<gpiod_chip, 2> virtual_chips {{{0}, {1}}};
std::array<std::array<gpiod_line, 10>, 2> virtual_lines;

void close_virtual_gpio(gpiod_line &line) noexcept
{
	for(int &descriptor : line.event_pipe)
	{
		if(descriptor >= 0)
			::close(descriptor);
		descriptor = -1;
	}
	line.waiting = false;
	virtual_gpio_ready.notify_all();
}

} // namespace

extern "C" gpiod_chip *gpiod_chip_open(const char *path) noexcept
{
	if(not path)
	{
		errno = ENOENT;
		return nullptr;
	}
	const std::string_view path_view(path);
	if(path_view == "/dev/altun-test-virtual-gpiochip")
		return &virtual_chips[0];
	if(path_view == "/dev/altun-test-virtual-gpiochip-2")
		return &virtual_chips[1];

	errno = ENOENT;
	return nullptr;
}

extern "C" void gpiod_chip_close(gpiod_chip*) noexcept {}

extern "C" gpiod_line *gpiod_chip_get_line(gpiod_chip *chip,
	unsigned int offset) noexcept
{
	if(not chip or chip->index >= virtual_chips.size() or
		chip != &virtual_chips[chip->index] or
		offset >= virtual_lines[chip->index].size())
	{
		errno = EINVAL;
		return nullptr;
	}
	virtual_lines[chip->index][offset].offset = offset;
	return &virtual_lines[chip->index][offset];
}

extern "C" int gpiod_line_request(gpiod_line *line,
	const gpiod_line_request_config*, int default_value) noexcept
{
	std::scoped_lock lock(virtual_gpio_mutex);
	if(not line)
	{
		errno = EINVAL;
		return -1;
	}
	close_virtual_gpio(*line);
	if(::pipe2(line->event_pipe, O_NONBLOCK | O_CLOEXEC) < 0)
		return -1;
	line->value = default_value != 0;
	return 0;
}

extern "C" void gpiod_line_release(gpiod_line *line) noexcept
{
	if(not line)
		return ;
	std::scoped_lock lock(virtual_gpio_mutex);
	close_virtual_gpio(*line);
}

extern "C" int gpiod_line_get_value(gpiod_line *line) noexcept
{
	std::scoped_lock lock(virtual_gpio_mutex);
	if(not line)
	{
		errno = EINVAL;
		return -1;
	}
	return line->value ? 1 : 0;
}

extern "C" int gpiod_line_set_value(gpiod_line *line, int value) noexcept
{
	std::scoped_lock lock(virtual_gpio_mutex);
	if(not line)
	{
		errno = EINVAL;
		return -1;
	}
	line->value = value != 0;
	return 0;
}

extern "C" int gpiod_line_event_get_fd(gpiod_line *line) noexcept
{
	std::scoped_lock lock(virtual_gpio_mutex);
	if(not line or line->event_pipe[0] < 0)
	{
		errno = EBADF;
		return -1;
	}
	return line->event_pipe[0];
}

extern "C" int gpiod_line_event_wait(gpiod_line *line,
	const timespec *timeout) noexcept
{
	int descriptor = -1;
	{
		std::scoped_lock lock(virtual_gpio_mutex);
		if(not line or line->event_pipe[0] < 0)
		{
			errno = EBADF;
			return -1;
		}
		descriptor = line->event_pipe[0];
	}

	int timeout_ms = -1;
	if(timeout)
	{
		const auto milliseconds = timeout->tv_sec * 1000L +
			timeout->tv_nsec / 1'000'000L;
		timeout_ms = milliseconds > INT_MAX ? INT_MAX :
			static_cast<int>(milliseconds);
	}
	pollfd poll_descriptor {descriptor, POLLIN, 0};
	if(timeout_ms != 0)
	{
		std::scoped_lock lock(virtual_gpio_mutex);
		line->waiting = true;
		virtual_gpio_ready.notify_all();
	}
	int result = -1;
	do {
		result = ::poll(&poll_descriptor, 1, timeout_ms);
	}
	while(result < 0 and errno == EINTR);
	if(timeout_ms != 0)
	{
		std::scoped_lock lock(virtual_gpio_mutex);
		line->waiting = false;
	}
	return result;
}

extern "C" int gpiod_line_event_read(gpiod_line *line,
	gpiod_line_event *event) noexcept
{
	if(not event)
	{
		errno = EINVAL;
		return -1;
	}
	int descriptor = -1;
	{
		std::scoped_lock lock(virtual_gpio_mutex);
		if(not line or line->event_pipe[0] < 0)
		{
			errno = EBADF;
			return -1;
		}
		descriptor = line->event_pipe[0];
	}

	unsigned char type = 0;
	ssize_t size = -1;
	do {
		size = ::read(descriptor, &type, sizeof(type));
	}
	while(size < 0 and errno == EINTR);
	if(size != sizeof(type))
		return -1;

	::clock_gettime(CLOCK_MONOTONIC, &event->ts);
	event->event_type = type;
	return 0;
}

#endif // ALTUN_TEST_GPIOD_V1

namespace altun_test_support
{

bool virtual_gpio_available() noexcept
{
#ifdef ALTUN_TEST_GPIOD_V1
	return true;
#else
	return false;
#endif
}

void reset_virtual_gpio() noexcept
{
#ifdef ALTUN_TEST_GPIOD_V1
	std::scoped_lock lock(virtual_gpio_mutex);
	for(auto &chip : virtual_lines)
	{
		for(auto &line : chip)
		{
			close_virtual_gpio(line);
			line.value = false;
		}
	}
#endif
}

void push_virtual_gpio_event(bool rising)
{
#ifdef ALTUN_TEST_GPIOD_V1
	std::scoped_lock lock(virtual_gpio_mutex);
	auto &line = virtual_lines[0][7];
	if(line.event_pipe[1] < 0)
		throw std::runtime_error("virtual GPIO is not open");
	const unsigned char type = rising ? 1 : 2;
	if(::write(line.event_pipe[1], &type, sizeof(type)) != sizeof(type))
		throw std::runtime_error(std::strerror(errno));
#else
	(void)rising;
	throw std::runtime_error("virtual GPIO backend is unavailable");
#endif
}

void wait_for_virtual_gpio_waiter()
{
#ifdef ALTUN_TEST_GPIOD_V1
	using namespace std::chrono_literals;
	std::unique_lock lock(virtual_gpio_mutex);
	if(not virtual_gpio_ready.wait_for(lock, 2s, [] {
		return virtual_lines[0][7].waiting;
	}))
		throw std::runtime_error("virtual GPIO did not enter a blocking wait");
#endif
}

} // namespace altun_test_support
