// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_SUBSYS_DETAIL_TYPES_H
#define LIBEMPP_LINUX_SUBSYS_DETAIL_TYPES_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

namespace libempp::subsys
{

template <enumeration Enum>
consteval bool is_valid() noexcept
{
#define X_MACRO(e,v)  \
	if constexpr( Enum == enumeration::e ) return true;
	LIBEMPP_SUBSYSTEM
#undef X_MACRO
	else return false;
}

template <enumeration Enum>
consteval const char *string() noexcept requires is_valid_v<Enum>
{
#define X_MACRO(e,v)  \
	if constexpr( Enum == enumeration::e ) return v;
	LIBEMPP_SUBSYSTEM
#undef X_MACRO
	else return "";
}

inline bool check(enumeration _e, bool _throw)
{
#define X_MACRO(e,v)  \
	if( _e == enumeration::e ) return true;
	LIBEMPP_SUBSYSTEM
#undef X_MACRO
	if( _throw )
	{
		riwo::invalid_argument::loc_throw(std::format (
			"libempp::subsys::check: Invalid subsystem ({})", _e
		));
	}
	return false;
}

inline const char *string(enumeration _e, bool _throw)
{
#define X_MACRO(e,v)  \
	if( _e == enumeration::e ) return v;
	LIBEMPP_SUBSYSTEM
#undef X_MACRO
	if( _throw )
	{
		riwo::invalid_argument::loc_throw(std::format (
			"libempp::subsys::string: Invalid subsystem ({})", _e
		));
	}
	return "";
}

inline enumeration from_string(std::string_view str)
{
#define X_MACRO(e,v)  \
	if( str == v ) return enumeration::e;
	LIBEMPP_SUBSYSTEM
#undef X_MACRO
	return enumeration::none;
}

} //namespace libempp::subsys

#endif //__linux__
#endif //LIBEMPP_LINUX_SUBSYS_DETAIL_TYPES_H
