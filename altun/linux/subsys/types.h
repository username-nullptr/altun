// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_SUBSYS_TYPES_H
#define ALTUN_LINUX_SUBSYS_TYPES_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/global.h>

namespace altun { namespace subsys
{

#define ALTUN_SUBSYSTEM \
X_MACRO( gpio     , "gpio"      ) \
X_MACRO( led      , "leds"      ) \
X_MACRO( usb      , "usb"       ) \
X_MACRO( tty      , "tty"       ) \
X_MACRO( net      , "net"       ) \
X_MACRO( block    , "block"     ) \
X_MACRO( backlight, "backlight" ) \
X_MACRO( pwm      , "pwm"       ) \
// ... ...

enum class enumeration {
#define X_MACRO(e,v)  e,
	ALTUN_SUBSYSTEM
#undef X_MACRO
	none
};

template <enumeration Enum>
[[nodiscard]] consteval bool is_valid() noexcept;

template <enumeration Enum>
constexpr bool is_valid_v = is_valid<Enum>();

template <enumeration Enum>
[[nodiscard]] consteval const char *string() noexcept
	requires is_valid_v<Enum>;

ALTUN_LINUX_VAPI bool check(enumeration e, bool _throw = true);
[[nodiscard]] ALTUN_LINUX_VAPI const char *string(enumeration e, bool _throw = true);
[[nodiscard]] ALTUN_LINUX_VAPI enumeration from_string(std::string_view str);

} //namespace subsys

using subsys_enum = subsys::enumeration;

} //namespace altun
#include <altun/linux/subsys/detail/types.h>

#endif //__linux__
#endif //ALTUN_LINUX_SUBSYS_TYPES_H
