// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "enumeration.h"
#include <altun/core/log.h>
#include <riwo/core/algorithm/misc.h>

namespace altun::udev::detail { namespace
{

class RIWO_DECL_HIDDEN udev_protector
{
	RIWO_DISABLE_COPY_MOVE(udev_protector)

public:
	udev_protector(std::string_view subsystem, const properties_t &properties)
	{
		m_context = udev_new();
		if( not m_context )
		{
			altun_clog_critical("Altun.Linux",
				"udev::enumeration: Failed to create udev context."
			);
			abort();
		}
		m_enumerate = udev_enumerate_new(m_context);
		if( not m_enumerate )
		{
			altun_clog_critical("Altun.Linux",
				"udev::enumeration <bug>: Failed to create udev enumerate."
			);
			riwo::forced_termination();
		}
		const std::string subsystem_value(subsystem);
		udev_enumerate_add_match_subsystem(m_enumerate, subsystem_value.c_str());
		udev_enumerate_scan_devices(m_enumerate);

		auto devices = udev_enumerate_get_list_entry(m_enumerate);
		if( not devices )
			return ;

		udev_list_entry *entry = nullptr;
		udev_list_entry_foreach(entry, devices)
		{
			if( properties.empty() )
			{
				m_devices.emplace_back(entry);
				continue;
			}
			const char *sys_path = udev_list_entry_get_name(entry);
			auto *device = udev_device_new_from_syspath(m_context, sys_path);
			if( not device )
				continue;

			bool matched = true;
			for(const auto &[key,value] : properties)
			{
				const std::string property_key(key);
				const char *property = udev_device_get_property_value (
					device, property_key.c_str()
				);
				if( not property or riwo::wildcard_match(value, property) < 0 )
				{
					matched = false;
					break;
				}
			}
			udev_device_unref(device);
			if( matched )
				m_devices.emplace_back(entry);
		}
	}

	~udev_protector()
	{
		if( m_enumerate )
			udev_enumerate_unref(m_enumerate);
		if( m_context )
			udev_unref(m_context);
	}

	[[nodiscard]] ::udev *context() const noexcept {
		return m_context;
	}
	[[nodiscard]] const std::vector<udev_list_entry*> &devices() const noexcept {
		return m_devices;
	}

private:
	::udev *m_context = nullptr;
	udev_enumerate *m_enumerate = nullptr;
	std::vector<udev_list_entry*> m_devices;
};

} //namespace

class RIWO_DECL_HIDDEN enumeration_core::impl
{
public:
	impl(std::shared_ptr<udev_protector> protector, udev_list_entry *entry) :
		m_protector(std::move(protector))
	{
		init(entry);
	}

	impl(std::string_view subsystem, const properties_t &properties) :
		m_protector(std::make_shared<udev_protector>(subsystem, properties))
	{
		if( not m_protector->devices().empty() )
			init(m_protector->devices().front());
	}

	impl() = default;
	~impl()
	{
		if( m_device )
			udev_device_unref(m_device);
	}

	void init(udev_list_entry *entry)
	{
		if( m_device )
		{
			udev_device_unref(m_device);
			m_device = nullptr;
		}
		m_entry = entry;
		m_path.clear();

		if( not m_protector or not entry )
			return ;

		const char *path = udev_list_entry_get_name(entry);
		if( not path )
			return ;

		m_path = path;
		m_device = udev_device_new_from_syspath (
			m_protector->context(), m_path.c_str()
		);
	}

	std::shared_ptr<udev_protector> m_protector {};
	udev_list_entry *m_entry = nullptr;

	std::string m_path {};
	udev_device *m_device = nullptr;
};

enumeration_core::enumeration_core(std::string_view subsystem, const properties_t &properties) :
	m_impl(std::make_unique<impl>(subsystem, properties))
{

}

enumeration_core::enumeration_core() :
	m_impl(std::make_unique<impl>())
{

}

enumeration_core::~enumeration_core() = default;

enumeration_core::enumeration_core(const enumeration_core &other) :
	m_impl(other.is_valid() ?
		std::make_unique<impl>(other.m_impl->m_protector, other.m_impl->m_entry) :
		std::make_unique<impl>()
	)
{

}

enumeration_core &enumeration_core::operator=(const enumeration_core &other)
{
	if( &other == this )
		return *this;

	auto replacement = other.is_valid() ?
		std::make_unique<impl>(other.m_impl->m_protector, other.m_impl->m_entry) :
		std::make_unique<impl>();

	m_impl = std::move(replacement);
	return *this;
}

enumeration_core::enumeration_core(enumeration_core &&other) noexcept = default;

enumeration_core &enumeration_core::operator=(enumeration_core &&other) noexcept = default;

riwo::optional<riwo::value>
enumeration_core::property(std::string_view key) const noexcept
{
	riwo::optional<riwo::value> result;
	if( not is_valid() )
		return result;
	try {
		const std::string key_value(key);
		if( const char *property = udev_device_get_property_value(
			native(), key_value.c_str()) )
		{
			result = property;
		}
	}
	catch(...) {}
	return result;
}

std::vector<std::string> enumeration_core::property_keys() const
{
	std::vector<std::string> keys;
	if( not is_valid() )
		return keys;

	auto properties = udev_device_get_properties_list_entry(native());
	if( not properties )
		return keys;

	udev_list_entry *entry = nullptr;
	udev_list_entry_foreach(entry, properties)
	{
		if( const char *key = udev_list_entry_get_name(entry) )
			keys.emplace_back(key);
	}
	return keys;
}

bool enumeration_core::is_valid() const noexcept
{
	return m_impl and m_impl->m_protector and m_impl->m_device;
}

std::string_view enumeration_core::path() const noexcept
{
	return is_valid() ? std::string_view(m_impl->m_path) : std::string_view{};
}

udev_native_t enumeration_core::native() const noexcept
{
	return m_impl ? m_impl->m_device : nullptr;
}

udev_native_t enumeration_core::native() noexcept
{
	return m_impl ? m_impl->m_device : nullptr;
}

std::vector<enumeration_core> enumeration_core::list
(std::string_view subsystem, const properties_t &properties)
{
	auto protector = std::make_shared<udev_protector>(subsystem, properties);
	std::vector<enumeration_core> devices {};
	devices.reserve(protector->devices().size());

	for(auto *entry : protector->devices())
	{
		enumeration_core device;
		device.m_impl = std::make_unique<impl>(protector, entry);
		devices.push_back(std::move(device));
	}
	return devices;
}

} //namespace altun::udev::detail
