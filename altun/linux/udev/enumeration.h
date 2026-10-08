// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_UDEV_ENUMERATION_H
#define ALTUN_LINUX_UDEV_ENUMERATION_H
#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/udev/properties.h>
#include <riwo/core/value.h>
#include <libudev.h>

namespace altun::udev { namespace detail {
class enumeration_core;
} //namespace detail

using udev_native_t = udev_device*;
/*
 * Retrieve device properties similar to the output of:
 * udevadm info -q property /dev/tty*
 */
template <subsys_enum Subsys> requires subsys::is_valid_v<Subsys>
class ALTUN_LINUX_TAPI enumeration
{
public:
	// Retrieve a device that matches the supplied properties.
	enumeration(std::string_view pro_key, const riwo::value &pro_value);
	enumeration(std::initializer_list<std::pair<std::string_view, riwo::value>> properties);
	enumeration(const properties_t &properties);

	enumeration(); // Invalid instance.
	~enumeration();

	enumeration(const enumeration &other);
	enumeration &operator=(const enumeration &other);

	enumeration(enumeration &&other) noexcept;
	enumeration &operator=(enumeration &&other) noexcept;

public:
	[[nodiscard]] riwo::optional<riwo::value> property (
		riwo::concepts::string_p<char> auto &&key
	) const noexcept;

	[[nodiscard]] std::vector<std::string> property_keys() const;

public:
	[[nodiscard]] bool is_valid() const noexcept;
	[[nodiscard]] std::string_view path() const noexcept; // sysfs interface path.

	[[nodiscard]] udev_native_t native() const noexcept;
	[[nodiscard]] udev_native_t native() noexcept;

public: // Retrieve all devices that match the supplied properties.
	[[nodiscard]] static std::vector<enumeration> list (
		std::string_view pro_key, const riwo::value &pro_value
	);
	[[nodiscard]] static std::vector<enumeration> list (
		const properties_t &properties = {}
	);

private:
	std::unique_ptr<detail::enumeration_core> m_impl {};
};

} //namespace altun::udev
#include <altun/linux/udev/detail/enumeration.h>

#endif //__linux__
#endif //ALTUN_LINUX_UDEV_ENUMERATION_H
