// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_CORE_SBUS_SBUS_H
#define ALTUN_CORE_SBUS_SBUS_H

#include <altun/core/global.h>
#include <riwo/utils/sbus.h>

#if ALTUN_SBUS_INTERFACE_DBUS
# include <altun/core/sbus/detail/dbus_interface.h>
#elif ALTUN_SBUS_INTERFACE_CYCLONE
# include <altun/core/sbus/detail/cyclone_interface.h>
#elif ALTUN_SBUS_INTERFACE_SHM
# include <altun/core/sbus/detail/shm_interface.h>
#endif

namespace altun::sbus
{

#define ALTUN_SBUS_TYPE(_t) \
	RIWO_UTILS_SBUS_TYPE(altun._t)

#define ALTUN_SBUS_META_TYPE(_t, ...) \
	RIWO_UTILS_SBUS_META_TYPE(altun._t, __VA_ARGS__)

#define ALTUN_SBUS_AUTO_TOPIC \
	"riwo.utils.sbus.topic.altun." __FILE__ RIWO_SHARP(:RIWO_AUTO_XX_NAME())

#define ALTUN_SBUS_AUTO_TYPE \
	RIWO_UTILS_SBUS_TYPE_IMPL(ALTUN_SBUS_AUTO_TOPIC);

#define ALTUN_SBUS_AUTO_META_TYPE(...) \
	ALTUN_SBUS_AUTO_TYPE RIWO_META_FIELDS(__VA_ARGS__)

#if ALTUN_SBUS_INTERFACE_DBUS
using interface = dbus_interface;
#elif ALTUN_SBUS_INTERFACE_CYCLONE
using interface = cyclone_interface;
#elif ALTUN_SBUS_INTERFACE_SHM
using interface = shm_interface;
#elif ALTUN_SBUS_INTERFACE_LOCAL
using interface = riwo::utils::sbus::local_interface;
#elif ALTUN_SBUS_INTERFACE_UDP
using interface = riwo::utils::sbus::udp_interface;
#elif ALTUN_SBUS_INTERFACE_DEFAULT
using interface = riwo::utils::sbus::default_interface;
#else
# error "No altun SBus interface was selected."
#endif

using subscriber = riwo::utils::sbus::basic_subscriber<interface>;

ALTUN_CORE_API void publish (
	std::string_view topic, const void *buffer, size_t size
);

template <riwo::concepts::any_string_p...Args>
ALTUN_CORE_TAPI void publish(std::string_view topic, Args&&...args)
	requires (sizeof...(Args) > 0);

template <riwo::utils::sbus::concepts::unregistered_type_p...Args>
ALTUN_CORE_TAPI void publish(std::string_view topic, Args&&...args)
	requires (sizeof...(Args) > 0);

template <riwo::utils::sbus::concepts::topic_type...Args>
ALTUN_CORE_TAPI void publish(Args&&...args)
	requires (sizeof...(Args) > 0);

template <riwo::concepts::match_sched<subscriber::executor_t> Exec0, typename...Args>
ALTUN_CORE_TAPI std::pair<subscriber,uint64_t> subscribe(Exec0 &&exec, Args&&...args) requires requires {
	riwo::utils::sbus::subscribe<subscriber>(std::forward<Exec0>(exec), std::forward<Args>(args)...);
};

template <typename...Args>
ALTUN_CORE_TAPI std::pair<subscriber,uint64_t> subscribe(Args&&...args) requires requires {
	riwo::utils::sbus::subscribe<subscriber>(std::forward<Args>(args)...);
};

} //namespace altun::sbus
#include <altun/core/sbus/detail/sbus.h>


#endif //ALTUN_CORE_SBUS_SBUS_H
