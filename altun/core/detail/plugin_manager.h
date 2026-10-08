// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_CORE_DETAIL_PLUGIN_MANAGER_H
#define ALTUN_CORE_DETAIL_PLUGIN_MANAGER_H

namespace altun
{

template <typename Func>
riwo::optional<plugin_manager::library_t> plugin_manager::library(Func &&predicate) noexcept
	requires is_predicate_v<Func>
{
	for(auto &[name, lib] : libraries())
	{
		if( predicate(name, lib) )
			return lib;
	}
	return {};
}

} //namespace altun


#endif //ALTUN_CORE_DETAIL_PLUGIN_MANAGER_H
