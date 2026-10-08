// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_UDEV_DETAIL_EVENT_H
#define ALTUN_LINUX_UDEV_DETAIL_EVENT_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/udev/detail/event_core.h>

namespace altun::udev
{

constexpr std::string_view string(event_action action) noexcept
{
	switch(action)
	{
	case event_action::add    : return "add"    ;
	case event_action::remove : return "remove" ;
	case event_action::change : return "change" ;
	case event_action::move   : return "move"   ;
	case event_action::online : return "online" ;
	case event_action::offline: return "offline";
	case event_action::bind   : return "bind"   ;
	case event_action::unbind : return "unbind" ;
	case event_action::unknown: return "unknown";
	}
	return "unknown";
}

constexpr event_action event_action_from_string(std::string_view action) noexcept
{
	if( action == "add"     ) return event_action::add    ;
	if( action == "remove"  ) return event_action::remove ;
	if( action == "change"  ) return event_action::change ;
	if( action == "move"    ) return event_action::move   ;
	if( action == "online"  ) return event_action::online ;
	if( action == "offline" ) return event_action::offline;
	if( action == "bind"    ) return event_action::bind   ;
	if( action == "unbind"  ) return event_action::unbind ;
	return event_action::unknown;
}

template <subsys_enum Subsys, riwo::concepts::exec Exec>
	requires subsys::is_valid_v<Subsys>
basic_event<Subsys,Exec>::basic_event(riwo::concepts::match_sched<Exec> auto &&exec) :
	m_exec(riwo::get_executor_helper(std::forward<decltype(exec)>(exec))),
	m_impl(std::make_shared<detail::event_core>(asio::any_io_executor(m_exec),
		subsys::string<Subsys>(), received, error
	))
{

}

template <subsys_enum Subsys, riwo::concepts::exec Exec>
	requires subsys::is_valid_v<Subsys>
basic_event<Subsys,Exec>::basic_event()
	requires riwo::concepts::match_def_exec<Exec> :
	basic_event(riwo::io_context())
{

}

template <subsys_enum Subsys, riwo::concepts::exec Exec>
	requires subsys::is_valid_v<Subsys>
basic_event<Subsys,Exec>::~basic_event()
{
	m_impl->detach();
}

template <subsys_enum Subsys, riwo::concepts::exec Exec>
	requires subsys::is_valid_v<Subsys>
void basic_event<Subsys,Exec>::open(std::error_code &ec) noexcept
{
	m_impl->open({}, ec);
}

template <subsys_enum Subsys, riwo::concepts::exec Exec>
	requires subsys::is_valid_v<Subsys>
void basic_event<Subsys,Exec>::open(std::string_view dev_type, std::error_code &ec) noexcept
{
	m_impl->open(dev_type, ec);
}

template <subsys_enum Subsys, riwo::concepts::exec Exec>
	requires subsys::is_valid_v<Subsys>
void basic_event<Subsys,Exec>::open(std::string_view dev_type)
{
	std::error_code ec;
	open(dev_type, ec);
	if( ec )
	{
		riwo::system_error::loc_throw (
			ec, "altun::udev::basic_event::open"
		);
	}
}

template <subsys_enum Subsys, riwo::concepts::exec Exec>
	requires subsys::is_valid_v<Subsys>
void basic_event<Subsys,Exec>::open()
{
	std::error_code ec;
	open(ec);
	if( ec )
	{
		riwo::system_error::loc_throw (
			ec, "altun::udev::basic_event::open"
		);
	}
}

template <subsys_enum Subsys, riwo::concepts::exec Exec>
	requires subsys::is_valid_v<Subsys>
void basic_event<Subsys,Exec>::close(std::error_code &ec) noexcept
{
	m_impl->close(ec);
}

template <subsys_enum Subsys, riwo::concepts::exec Exec>
	requires subsys::is_valid_v<Subsys>
void basic_event<Subsys,Exec>::close()
{
	std::error_code ec;
	close(ec);
	if( ec )
	{
		riwo::system_error::loc_throw (
			ec, "altun::udev::basic_event::close"
		);
	}
}

template <subsys_enum Subsys, riwo::concepts::exec Exec>
	requires subsys::is_valid_v<Subsys>
basic_event<Subsys,Exec>::executor_t basic_event<Subsys,Exec>::get_executor() noexcept
{
	return m_exec;
}

template <subsys_enum Subsys, riwo::concepts::exec Exec>
	requires subsys::is_valid_v<Subsys>
bool basic_event<Subsys,Exec>::is_open() const noexcept
{
	return m_impl->is_open();
}

} //namespace altun::udev

#endif //__linux__
#endif //ALTUN_LINUX_UDEV_DETAIL_EVENT_H
