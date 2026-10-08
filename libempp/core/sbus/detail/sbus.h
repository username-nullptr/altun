// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_CORE_SBUS_DETAIL_SBUS_H
#define LIBEMPP_CORE_SBUS_DETAIL_SBUS_H

namespace libempp::sbus
{

template <riwo::concepts::any_string_p...Args>
void publish(std::string_view topic, Args&&...args) requires (sizeof...(Args) > 0)
{
	riwo::utils::sbus::publish<interface>(topic, std::forward<Args>(args)...);
}

template <riwo::utils::sbus::concepts::unregistered_type_p...Args>
void publish(std::string_view topic, Args&&...args) requires (sizeof...(Args) > 0)
{
	riwo::utils::sbus::publish<interface>(topic, std::forward<Args>(args)...);
}

template <riwo::utils::sbus::concepts::topic_type...Args>
void publish(Args&&...args) requires (sizeof...(Args) > 0)
{
	riwo::utils::sbus::publish<interface>(std::forward<Args>(args)...);
}

template <riwo::concepts::match_sched<subscriber::executor_t> Exec0, typename...Args>
std::pair<subscriber,uint64_t> subscribe(Exec0 &&exec, Args&&...args) requires requires
	{ riwo::utils::sbus::subscribe<subscriber>(std::forward<Exec0>(exec), std::forward<Args>(args)...); }
{
	return riwo::utils::sbus::subscribe<subscriber>(std::forward<Exec0>(exec), std::forward<Args>(args)...);
}

template <typename...Args>
std::pair<subscriber,uint64_t> subscribe(Args&&...args) requires requires
	{ riwo::utils::sbus::subscribe<subscriber>(std::forward<Args>(args)...); }
{
	return riwo::utils::sbus::subscribe<subscriber>(std::forward<Args>(args)...);
}

} //namespace libempp::sbus


#endif //LIBEMPP_CORE_SBUS_DETAIL_SBUS_H
