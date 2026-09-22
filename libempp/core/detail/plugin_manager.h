// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_CORE_DETAIL_PLUGIN_MANAGER_H
#define LIBEMPP_CORE_DETAIL_PLUGIN_MANAGER_H

namespace libempp
{

template <typename Func>
libgs::optional<plugin_manager::library_t> plugin_manager::library(Func &&predicate) noexcept
	requires is_predicate_v<Func>
{
	for(auto &[name, lib] : libraries())
	{
		if( predicate(name, lib) )
			return lib;
	}
	return {};
}

} //namespace libempp


#endif //LIBEMPP_CORE_DETAIL_PLUGIN_MANAGER_H
