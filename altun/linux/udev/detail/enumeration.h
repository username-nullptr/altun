// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_UDEV_DETAIL_ENUMERATION_H
#define ALTUN_LINUX_UDEV_DETAIL_ENUMERATION_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/udev/detail/enumeration_core.h>

namespace altun::udev
{

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
enumeration<Subsys>::enumeration(std::string_view pro_key, const riwo::value &pro_value) :
	enumeration(properties_t{{ pro_key, *pro_value }})
{

}

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
enumeration<Subsys>::enumeration
(std::initializer_list<std::pair<std::string_view,riwo::value>> properties)
{
	properties_t map;
	for(const auto &[key,value] : properties)
		map[key] = *value;

	m_impl = std::make_unique<detail::enumeration_core>(
		subsys::string<Subsys>(), map
	);
}

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
enumeration<Subsys>::enumeration(const properties_t &properties) :
	m_impl(std::make_unique<detail::enumeration_core>(subsys::string<Subsys>(), properties))
{

}

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
enumeration<Subsys>::enumeration() :
	m_impl(std::make_unique<detail::enumeration_core>())
{

}

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
enumeration<Subsys>::~enumeration() = default;

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
enumeration<Subsys>::enumeration(const enumeration &other) :
	m_impl(other.m_impl ?
		std::make_unique<detail::enumeration_core>(*other.m_impl) :
		std::make_unique<detail::enumeration_core>()
	)
{

}

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
enumeration<Subsys> &enumeration<Subsys>::operator=(const enumeration &other)
{
	if( &other == this )
		return *this;

	auto replacement = other.m_impl ?
		std::make_unique<detail::enumeration_core>(*other.m_impl) :
		std::make_unique<detail::enumeration_core>();

	m_impl = std::move(replacement);
	return *this;
}

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
enumeration<Subsys>::enumeration(enumeration &&other) noexcept = default;

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
enumeration<Subsys> &enumeration<Subsys>::operator=(enumeration &&other) noexcept = default;

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
riwo::optional<riwo::value> enumeration<Subsys>::property
(riwo::concepts::string_p<char> auto &&key) const noexcept
{
	if( not m_impl )
		return {};

	return m_impl->property(riwo::strtls::to_view(
		std::forward<decltype(key)>(key)
	));
}

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
std::vector<std::string> enumeration<Subsys>::property_keys() const
{
	return m_impl ? m_impl->property_keys() : std::vector<std::string>{};
}

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
bool enumeration<Subsys>::is_valid() const noexcept
{
	return m_impl and m_impl->is_valid();
}

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
std::string_view enumeration<Subsys>::path() const noexcept
{
	return m_impl ? m_impl->path() : std::string_view{};
}

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
udev_native_t enumeration<Subsys>::native() const noexcept
{
	return m_impl ? m_impl->native() : nullptr;
}

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
udev_native_t enumeration<Subsys>::native() noexcept
{
	return m_impl ? m_impl->native() : nullptr;
}

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
std::vector<enumeration<Subsys>> enumeration<Subsys>::list
(std::string_view pro_key, const riwo::value &pro_value)
{
	return list({{ pro_key, *pro_value }});
}

template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
std::vector<enumeration<Subsys>> enumeration<Subsys>::list
(const properties_t &properties)
{
	auto implementations = detail::enumeration_core::list (
		subsys::string<Subsys>(), properties
	);
	std::vector<enumeration> devices {};
	devices.reserve(implementations.size());

	for(auto &implementation : implementations)
	{
		enumeration device;
		device.m_impl = std::make_unique<detail::enumeration_core>(
			std::move(implementation)
		);
		devices.push_back(std::move(device));
	}
	return devices;
}

} //namespace altun::udev

#endif //__linux__
#endif //ALTUN_LINUX_UDEV_DETAIL_ENUMERATION_H
