// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_SUBSYS_DETAIL_GPIO_BACKEND_H
#define LIBEMPP_LINUX_SUBSYS_DETAIL_GPIO_BACKEND_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/global.h>

namespace libempp::subsys
{

using gpio_duration_t = std::chrono::milliseconds;
enum class gpio_backend_t : uint8_t;

struct gpio_node_t;
struct gpio_event_t;

} // namespace libempp::subsys

namespace libempp::subsys::detail
{

enum class gpio_event_wait_t : uint8_t {
	readable, priority
};

class LIBEMPP_LINUX_API gpio_backend
{
	RIWO_DISABLE_COPY_MOVE(gpio_backend)

public:
	gpio_backend() = default;
	virtual ~gpio_backend() = default;

	virtual void open(const gpio_node_t &node, std::error_code &error) noexcept = 0;
	virtual void close() noexcept = 0;

	[[nodiscard]] virtual bool is_open() const noexcept = 0;

	[[nodiscard]] virtual bool value(std::error_code &error) const noexcept = 0;
	virtual void set_value(bool value, std::error_code &error) noexcept = 0;

	[[nodiscard]] virtual int event_handle() const noexcept = 0;
	[[nodiscard]] virtual gpio_event_wait_t event_wait_type() const noexcept = 0;

	[[nodiscard]] virtual bool wait_event (
		gpio_event_t &event, gpio_duration_t timeout, std::error_code &error
	) noexcept = 0;
};

[[nodiscard]] LIBEMPP_LINUX_API
std::unique_ptr<gpio_backend> make_gpio_backend(std::error_code &error) noexcept;

[[nodiscard]] LIBEMPP_LINUX_API
gpio_backend_t gpio_backend_type() noexcept;

[[nodiscard]] LIBEMPP_LINUX_API
const char *gpio_backend_name() noexcept;

} // namespace libempp::subsys::detail

#endif //__linux__
#endif // LIBEMPP_LINUX_SUBSYS_DETAIL_GPIO_BACKEND_H
