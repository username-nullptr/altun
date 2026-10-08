// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_SUBSYS_DETAIL_TYPES_H
#define ALTUN_LINUX_SUBSYS_DETAIL_TYPES_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

namespace altun::subsys
{

template <enumeration Enum>
consteval bool is_valid() noexcept
{
#define X_MACRO(e,v)  \
	if constexpr( Enum == enumeration::e ) return true;
	ALTUN_SUBSYSTEM
#undef X_MACRO
	else return false;
}

template <enumeration Enum>
consteval const char *string() noexcept requires is_valid_v<Enum>
{
#define X_MACRO(e,v)  \
	if constexpr( Enum == enumeration::e ) return v;
	ALTUN_SUBSYSTEM
#undef X_MACRO
	else return "";
}

inline bool check(enumeration _e, bool _throw)
{
#define X_MACRO(e,v)  \
	if( _e == enumeration::e ) return true;
	ALTUN_SUBSYSTEM
#undef X_MACRO
	if( _throw )
	{
		riwo::invalid_argument::loc_throw(std::format (
			"altun::subsys::check: Invalid subsystem ({})", _e
		));
	}
	return false;
}

inline const char *string(enumeration _e, bool _throw)
{
#define X_MACRO(e,v)  \
	if( _e == enumeration::e ) return v;
	ALTUN_SUBSYSTEM
#undef X_MACRO
	if( _throw )
	{
		riwo::invalid_argument::loc_throw(std::format (
			"altun::subsys::string: Invalid subsystem ({})", _e
		));
	}
	return "";
}

inline enumeration from_string(std::string_view str)
{
#define X_MACRO(e,v)  \
	if( str == v ) return enumeration::e;
	ALTUN_SUBSYSTEM
#undef X_MACRO
	return enumeration::none;
}

} //namespace altun::subsys

#endif //__linux__
#endif //ALTUN_LINUX_SUBSYS_DETAIL_TYPES_H
