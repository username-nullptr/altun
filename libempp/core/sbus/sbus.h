// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_CORE_SBUS_SBUS_H
#define LIBEMPP_CORE_SBUS_SBUS_H

#include <libempp/core/global.h>
#include <riwo/utils/sbus.h>

#if LIBEMPP_SBUS_INTERFACE_DBUS
# include <libempp/core/sbus/detail/dbus_interface.h>
#elif LIBEMPP_SBUS_INTERFACE_CYCLONE
# include <libempp/core/sbus/detail/cyclone_interface.h>
#elif LIBEMPP_SBUS_INTERFACE_SHM
# include <libempp/core/sbus/detail/shm_interface.h>
#endif

namespace libempp::sbus
{

#define LIBEMPP_SBUS_TYPE(_t) \
	RIWO_UTILS_SBUS_TYPE(libempp._t)

#define LIBEMPP_SBUS_META_TYPE(_t, ...) \
	RIWO_UTILS_SBUS_META_TYPE(libempp._t, __VA_ARGS__)

#define LIBEMPP_SBUS_AUTO_TOPIC \
	"riwo.utils.sbus.topic.libempp." __FILE__ RIWO_SHARP(:RIWO_AUTO_XX_NAME())

#define LIBEMPP_SBUS_AUTO_TYPE \
	RIWO_UTILS_SBUS_TYPE_IMPL(LIBEMPP_SBUS_AUTO_TOPIC);

#define LIBEMPP_SBUS_AUTO_META_TYPE(...) \
	LIBEMPP_SBUS_AUTO_TYPE RIWO_META_FIELDS(__VA_ARGS__)

#if LIBEMPP_SBUS_INTERFACE_DBUS
using interface = dbus_interface;
#elif LIBEMPP_SBUS_INTERFACE_CYCLONE
using interface = cyclone_interface;
#elif LIBEMPP_SBUS_INTERFACE_SHM
using interface = shm_interface;
#elif LIBEMPP_SBUS_INTERFACE_LOCAL
using interface = riwo::utils::sbus::local_interface;
#elif LIBEMPP_SBUS_INTERFACE_UDP
using interface = riwo::utils::sbus::udp_interface;
#elif LIBEMPP_SBUS_INTERFACE_DEFAULT
using interface = riwo::utils::sbus::default_interface;
#else
# error "No libEMpp SBus interface was selected."
#endif

using subscriber = riwo::utils::sbus::basic_subscriber<interface>;

LIBEMPP_CORE_API void publish (
	std::string_view topic, const void *buffer, size_t size
);

template <riwo::concepts::any_string_p...Args>
LIBEMPP_CORE_TAPI void publish(std::string_view topic, Args&&...args)
	requires (sizeof...(Args) > 0);

template <riwo::utils::sbus::concepts::unregistered_type_p...Args>
LIBEMPP_CORE_TAPI void publish(std::string_view topic, Args&&...args)
	requires (sizeof...(Args) > 0);

template <riwo::utils::sbus::concepts::topic_type...Args>
LIBEMPP_CORE_TAPI void publish(Args&&...args)
	requires (sizeof...(Args) > 0);

template <riwo::concepts::match_sched<subscriber::executor_t> Exec0, typename...Args>
LIBEMPP_CORE_TAPI std::pair<subscriber,uint64_t> subscribe(Exec0 &&exec, Args&&...args) requires requires {
	riwo::utils::sbus::subscribe<subscriber>(std::forward<Exec0>(exec), std::forward<Args>(args)...);
};

template <typename...Args>
LIBEMPP_CORE_TAPI std::pair<subscriber,uint64_t> subscribe(Args&&...args) requires requires {
	riwo::utils::sbus::subscribe<subscriber>(std::forward<Args>(args)...);
};

} //namespace libempp::sbus
#include <libempp/core/sbus/detail/sbus.h>


#endif //LIBEMPP_CORE_SBUS_SBUS_H
