// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_UDEV_DETAIL_ENUMERATION_CORE_H
#define LIBEMPP_LINUX_UDEV_DETAIL_ENUMERATION_CORE_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

namespace libempp::udev::detail
{

class LIBEMPP_LINUX_API enumeration_core
{
public:
	enumeration_core(std::string_view subsystem, const properties_t &properties);
	enumeration_core();
	~enumeration_core();

	enumeration_core(const enumeration_core &other);
	enumeration_core &operator=(const enumeration_core &other);

	enumeration_core(enumeration_core &&other) noexcept;
	enumeration_core &operator=(enumeration_core &&other) noexcept;

public:
	[[nodiscard]] libgs::optional<libgs::value> property(std::string_view key) const noexcept;
	[[nodiscard]] std::vector<std::string> property_keys() const;

	[[nodiscard]] bool is_valid() const noexcept;
	[[nodiscard]] std::string_view path() const noexcept;

	[[nodiscard]] udev_native_t native() const noexcept;
	[[nodiscard]] udev_native_t native() noexcept;

	[[nodiscard]] static std::vector<enumeration_core> list (
		std::string_view subsystem, const properties_t &properties
	);

private:
	class impl;
	std::unique_ptr<impl> m_impl;
};

} //namespace libempp::udev::detail

#endif //__linux__
#endif //LIBEMPP_LINUX_UDEV_DETAIL_ENUMERATION_CORE_H
