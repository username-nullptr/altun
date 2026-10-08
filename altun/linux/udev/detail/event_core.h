// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_UDEV_DETAIL_EVENT_CORE_H
#define ALTUN_LINUX_UDEV_DETAIL_EVENT_CORE_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

namespace altun::udev::detail
{

class ALTUN_LINUX_API event_core
{
	RIWO_DISABLE_COPY_MOVE(event_core)

public:
	using received_signal_t = riwo::utils::signal <
		riwo::awaitable<void>(device_event)
	>;
	using error_signal_t = riwo::utils::signal <
		riwo::awaitable<void>(std::error_code)
	>;
	event_core(asio::any_io_executor exec, std::string subsystem,
		received_signal_t &received, error_signal_t &error
	);
	~event_core();

	void open(std::string_view dev_type, std::error_code &error) noexcept;
	void close(std::error_code &error) noexcept;

	void detach() noexcept;
	[[nodiscard]] bool is_open() const noexcept;

private:
	class impl;
	std::shared_ptr<impl> m_impl;
};

} //namespace altun::udev::detail

#endif //__linux__
#endif //ALTUN_LINUX_UDEV_DETAIL_EVENT_CORE_H
