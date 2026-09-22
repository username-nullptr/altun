// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_SUBSYS_GPIO_GROUP_H
#define LIBEMPP_LINUX_SUBSYS_GPIO_GROUP_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/subsys/gpio.h>

namespace libempp::subsys
{

struct gpio_line_config_t
{
	gpio_line_t line = 0;
	gpio_direction_t direction = gpio_direction_t::input;

	bool initial_value = false;
	bool active_low = false;

	gpio_edge_t edge = gpio_edge_t::none;
	std::string consumer = "libempp";
	std::string alias {};
};

template <libgs::concepts::exec Exec = asio::any_io_executor>
class LIBEMPP_LINUX_TAPI basic_gpio_group
{
	LIBGS_DISABLE_COPY(basic_gpio_group)

public:
	using executor_type = Exec;
	using executor_t = executor_type;
	using gpio_t = basic_gpio<executor_t>;

	using line_t = gpio_line_t;
	using line_config_t = gpio_line_config_t;

	using lines_t = std::vector<line_t>;
	using line_configs_t = std::vector<line_config_t>;
	using aliases_t = std::vector<std::string>;

	using line_values_t = std::map<gpio_line_t,bool>;
	using alias_values_t = std::map<std::string,bool>;

public:
	template <typename Exec0>
	explicit basic_gpio_group
	(const std::filesystem::path &chip, const line_configs_t &lines, Exec0 &&exec) requires (
		not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio_group> and
		libgs::concepts::match_sched<Exec0,executor_t>
	);
	explicit basic_gpio_group(const std::filesystem::path &chip, const line_configs_t &lines)
		requires libgs::concepts::match_def_exec<executor_t>;

	template <typename Exec0>
	explicit basic_gpio_group
	(const std::filesystem::path &chip, std::initializer_list<line_config_t> lines, Exec0 &&exec) requires (
		not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio_group> and
		libgs::concepts::match_sched<Exec0,executor_t>
	);
	explicit basic_gpio_group(const std::filesystem::path &chip, std::initializer_list<line_config_t> lines)
		requires libgs::concepts::match_def_exec<executor_t>;

	template <typename Exec0>
	explicit basic_gpio_group(Exec0 &&exec) requires (
		not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio_group> and
		libgs::concepts::match_sched<Exec0,executor_t>
	);
	explicit basic_gpio_group()
		requires libgs::concepts::match_def_exec<Exec>;

	~basic_gpio_group();
	basic_gpio_group(basic_gpio_group &&other) noexcept;
	basic_gpio_group &operator=(basic_gpio_group &&other) noexcept;

public:
	basic_gpio_group &open(const std::filesystem::path &chip,
		const line_config_t &line, std::error_code &error
	) noexcept;

	basic_gpio_group &open(const std::filesystem::path &chip,
		const line_config_t &line
	);
	basic_gpio_group &open(const std::filesystem::path &chip,
		const line_configs_t &lines, std::error_code &error
	) noexcept;

	basic_gpio_group &open(const std::filesystem::path &chip,
		const line_configs_t &lines
	);
	basic_gpio_group &open(const std::filesystem::path &chip,
		std::initializer_list<line_config_t> lines, std::error_code &error
	) noexcept;

	basic_gpio_group &open(const std::filesystem::path &chip,
		std::initializer_list<line_config_t> lines
	);
	basic_gpio_group &close(line_t line) noexcept;
	basic_gpio_group &close(const libgs::concepts::text_p<char> auto &alias) noexcept;
	basic_gpio_group &close() noexcept;

public:
	[[nodiscard]] gpio_t &at(line_t line);
	[[nodiscard]] const gpio_t &at(line_t line) const;

	[[nodiscard]] gpio_t &at(const libgs::concepts::text_p<char> auto &alias);
	[[nodiscard]] const gpio_t &at(const libgs::concepts::text_p<char> auto &alias) const;

	[[nodiscard]] gpio_t &operator[](line_t line);
	[[nodiscard]] const gpio_t &operator[](line_t line) const;

	[[nodiscard]] gpio_t &operator[](const libgs::concepts::text_p<char> auto &alias);
	[[nodiscard]] const gpio_t &operator[](const libgs::concepts::text_p<char> auto &alias) const;

	[[nodiscard]] bool contains(line_t line) const noexcept;
	[[nodiscard]] bool contains(const libgs::concepts::text_p<char> auto &alias) const noexcept;

public:
	basic_gpio_group &set(line_t line, bool value, std::error_code &error) noexcept;
	basic_gpio_group &set(line_t line, bool value);

	basic_gpio_group &set(const line_values_t &values, std::error_code &error) noexcept;
	basic_gpio_group &set(const line_values_t &values);

	basic_gpio_group &set(const alias_values_t &values, std::error_code &error) noexcept;
	basic_gpio_group &set(const alias_values_t &values);

	basic_gpio_group &set(bool value, std::error_code &error) noexcept;
	basic_gpio_group &set(bool value);

	basic_gpio_group &set (
		const libgs::concepts::text_p<char> auto &alias, bool value,
		std::error_code &error
	) noexcept;

	basic_gpio_group &set (
		const libgs::concepts::text_p<char> auto &alias, bool value
	);

public:
	basic_gpio_group &rising(line_t line, std::error_code &error) noexcept;
	basic_gpio_group &rising(line_t line);

	basic_gpio_group &rising(const lines_t &lines, std::error_code &error) noexcept;
	basic_gpio_group &rising(const lines_t &lines);

	basic_gpio_group &rising(const aliases_t &aliases, std::error_code &error) noexcept;
	basic_gpio_group &rising(const aliases_t &aliases);

	basic_gpio_group &rising(std::error_code &error) noexcept;
	basic_gpio_group &rising();

	basic_gpio_group &rising (
		const libgs::concepts::text_p<char> auto &alias, std::error_code &error
	) noexcept;

	basic_gpio_group &rising (
		const libgs::concepts::text_p<char> auto &alias
	);

public:
	basic_gpio_group &falling(line_t line, std::error_code &error) noexcept;
	basic_gpio_group &falling(line_t line);

	basic_gpio_group &falling(const lines_t &lines, std::error_code &error) noexcept;
	basic_gpio_group &falling(const lines_t &lines);

	basic_gpio_group &falling(const aliases_t &aliases, std::error_code &error) noexcept;
	basic_gpio_group &falling(const aliases_t &aliases);

	basic_gpio_group &falling(std::error_code &error) noexcept;
	basic_gpio_group &falling();

	basic_gpio_group &falling (
		const libgs::concepts::text_p<char> auto &alias, std::error_code &error
	) noexcept;

	basic_gpio_group &falling (
		const libgs::concepts::text_p<char> auto &alias
	);

public:
	basic_gpio_group &invert(line_t line, std::error_code &error) noexcept;
	basic_gpio_group &invert(line_t line);

	basic_gpio_group &invert(std::error_code &error) noexcept;
	basic_gpio_group &invert();

	basic_gpio_group &invert (
		const libgs::concepts::text_p<char> auto &alias, std::error_code &error
	) noexcept;

	basic_gpio_group &invert (
		const libgs::concepts::text_p<char> auto &alias
	);

public:
	[[nodiscard]] libgs::sys_expected<line_values_t> get() const noexcept;
	[[nodiscard]] libgs::sys_expected<bool> get(line_t line) const noexcept;

	[[nodiscard]] libgs::sys_expected<bool> get (
		const libgs::concepts::text_p<char> auto &alias
	) const noexcept;

	[[nodiscard]] explicit operator line_values_t() const;
	[[nodiscard]] line_values_t operator*() const;
	[[nodiscard]] line_values_t operator~() const;

public:
	[[nodiscard]] std::filesystem::path chip() const;
	[[nodiscard]] line_configs_t lines() const;

	[[nodiscard]] std::size_t size() const noexcept;
	[[nodiscard]] bool empty() const noexcept;

	[[nodiscard]] bool is_open() const noexcept;
	[[nodiscard]] executor_t get_executor() noexcept;

private:
	class impl;
	std::shared_ptr<impl> m_impl;
};

using gpio_group = basic_gpio_group<>;

} // namespace libempp::subsys
#include <libempp/linux/subsys/detail/gpio_group.h>

#endif //__linux__
#endif //LIBEMPP_LINUX_SUBSYS_GPIO_GROUP_H
