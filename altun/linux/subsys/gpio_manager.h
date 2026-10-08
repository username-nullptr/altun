// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_SUBSYS_GPIO_MANAGER_H
#define ALTUN_LINUX_SUBSYS_GPIO_MANAGER_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/subsys/gpio_group.h>

namespace altun::subsys
{

struct ALTUN_LINUX_API gpio_index
{
	std::filesystem::path chip = "/dev/gpiochip0";
	gpio_line_t line = 0;

	[[nodiscard]] static std::strong_ordering compare_chip (
		const std::filesystem::path &left, const std::filesystem::path &right
	) noexcept;

	[[nodiscard]] bool operator==(const gpio_index &other) const noexcept;
	[[nodiscard]] std::strong_ordering operator<=>(const gpio_index &other) const noexcept;
};

template <riwo::concepts::exec Exec = asio::any_io_executor>
class ALTUN_LINUX_TAPI basic_gpio_manager
{
	RIWO_DISABLE_COPY(basic_gpio_manager)

public:
	using executor_type = Exec;
	using executor_t = executor_type;

	using gpio_t = basic_gpio<Exec>;
	using group_t = basic_gpio_group<Exec>;

	using line_t = gpio_line_t;
	using index_t = gpio_index;
	using indexes_t = std::vector<index_t>;

	using node_t = gpio_node_t;
	using nodes_t = std::vector<node_t>;

	using chips_t = std::vector<std::filesystem::path>;
	using aliases_t = std::vector<std::string>;

	using index_values_t = std::map<index_t,bool>;
	using alias_values_t = std::map<std::string,bool>;

public:
	template <typename Exec0>
	explicit basic_gpio_manager(const nodes_t &nodes, Exec0 &&exec) requires (
		not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio_manager> and
		riwo::concepts::match_sched<Exec0,executor_t>
	);
	explicit basic_gpio_manager(const nodes_t &nodes)
		requires riwo::concepts::match_def_exec<executor_t>;

	template <typename Exec0>
	explicit basic_gpio_manager(std::initializer_list<node_t> nodes, Exec0 &&exec) requires (
		not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio_manager> and
		riwo::concepts::match_sched<Exec0,executor_t>
	);
	explicit basic_gpio_manager(std::initializer_list<node_t> nodes)
		requires riwo::concepts::match_def_exec<executor_t>;

	template <typename Exec0>
	explicit basic_gpio_manager(Exec0 &&exec) requires (
		not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio_manager> and
		riwo::concepts::match_sched<Exec0,executor_t>
	);
	explicit basic_gpio_manager()
		requires riwo::concepts::match_def_exec<Exec>;

	~basic_gpio_manager();
	basic_gpio_manager(basic_gpio_manager &&other) noexcept;
	basic_gpio_manager &operator=(basic_gpio_manager &&other) noexcept;

public:
	basic_gpio_manager &open(const node_t &node, std::error_code &error) noexcept;
	basic_gpio_manager &open(const node_t &node);

	basic_gpio_manager &open(const nodes_t &nodes, std::error_code &error) noexcept;
	basic_gpio_manager &open(const nodes_t &nodes);

	basic_gpio_manager &open(std::initializer_list<node_t> nodes, std::error_code &error) noexcept;
	basic_gpio_manager &open(std::initializer_list<node_t> nodes);

	basic_gpio_manager &close(const index_t &index) noexcept;
	basic_gpio_manager &close(const riwo::concepts::text_p<char> auto &alias) noexcept;
	basic_gpio_manager &close() noexcept;

public:
	[[nodiscard]] group_t &group(const std::filesystem::path &chip);
	[[nodiscard]] const group_t &group(const std::filesystem::path &chip) const;

	[[nodiscard]] gpio_t &at(const index_t &index);
	[[nodiscard]] const gpio_t &at(const index_t &index) const;

	[[nodiscard]] gpio_t &at(const riwo::concepts::text_p<char> auto &alias);
	[[nodiscard]] const gpio_t &at(const riwo::concepts::text_p<char> auto &alias) const;

	[[nodiscard]] gpio_t &operator[](const index_t &index);
	[[nodiscard]] const gpio_t &operator[](const index_t &index) const;

	[[nodiscard]] gpio_t &operator[](const riwo::concepts::text_p<char> auto &alias);
	[[nodiscard]] const gpio_t &operator[](const riwo::concepts::text_p<char> auto &alias) const;

	[[nodiscard]] bool contains_chip(const std::filesystem::path &chip) const noexcept;
	[[nodiscard]] bool contains(const index_t &index) const noexcept;
	[[nodiscard]] bool contains(const riwo::concepts::text_p<char> auto &alias) const noexcept;

public:
	basic_gpio_manager &set(const index_t &index, bool value, std::error_code &error) noexcept;
	basic_gpio_manager &set(const index_t &index, bool value);

	basic_gpio_manager &set(const index_values_t &values, std::error_code &error) noexcept;
	basic_gpio_manager &set(const index_values_t &values);

	basic_gpio_manager &set(const alias_values_t &values, std::error_code &error) noexcept;
	basic_gpio_manager &set(const alias_values_t &values);

	basic_gpio_manager &set(bool value, std::error_code &error) noexcept;
	basic_gpio_manager &set(bool value);

	basic_gpio_manager &set (
		const riwo::concepts::text_p<char> auto &alias, bool value, std::error_code &error
	) noexcept;

	basic_gpio_manager &set (
		const riwo::concepts::text_p<char> auto &alias, bool value
	);

public:
	basic_gpio_manager &rising(index_t index, std::error_code &error) noexcept;
	basic_gpio_manager &rising(index_t index);

	basic_gpio_manager &rising(const indexes_t &indexes, std::error_code &error) noexcept;
	basic_gpio_manager &rising(const indexes_t &indexes);

	basic_gpio_manager &rising(const aliases_t &aliases, std::error_code &error) noexcept;
	basic_gpio_manager &rising(const aliases_t &aliases);

	basic_gpio_manager &rising(std::error_code &error) noexcept;
	basic_gpio_manager &rising();

	basic_gpio_manager &rising (
		const riwo::concepts::text_p<char> auto &alias, std::error_code &error
	) noexcept;

	basic_gpio_manager &rising (
		const riwo::concepts::text_p<char> auto &alias
	);

public:
	basic_gpio_manager &falling(index_t index, std::error_code &error) noexcept;
	basic_gpio_manager &falling(index_t index);

	basic_gpio_manager &falling(const indexes_t &indexes, std::error_code &error) noexcept;
	basic_gpio_manager &falling(const indexes_t &indexes);

	basic_gpio_manager &falling(const aliases_t &aliases, std::error_code &error) noexcept;
	basic_gpio_manager &falling(const aliases_t &aliases);

	basic_gpio_manager &falling(std::error_code &error) noexcept;
	basic_gpio_manager &falling();

	basic_gpio_manager &falling (
		const riwo::concepts::text_p<char> auto &alias, std::error_code &error
	) noexcept;

	basic_gpio_manager &falling (
		const riwo::concepts::text_p<char> auto &alias
	);

public:
	basic_gpio_manager &invert(const index_t &index, std::error_code &error) noexcept;
	basic_gpio_manager &invert(const index_t &index);

	basic_gpio_manager &invert(std::error_code &error) noexcept;
	basic_gpio_manager &invert();

	basic_gpio_manager &invert (
		const riwo::concepts::text_p<char> auto &alias, std::error_code &error
	) noexcept;

	basic_gpio_manager &invert (
		const riwo::concepts::text_p<char> auto &alias
	);

public:
	[[nodiscard]] riwo::sys_expected<index_values_t> get() const noexcept;
	[[nodiscard]] riwo::sys_expected<bool> get(const index_t &index) const noexcept;

	[[nodiscard]] riwo::sys_expected<bool> get(
		const riwo::concepts::text_p<char> auto &alias
	) const noexcept;

	[[nodiscard]] explicit operator index_values_t() const;
	[[nodiscard]] index_values_t operator*() const;
	[[nodiscard]] index_values_t operator~() const;

public:
	[[nodiscard]] chips_t chips() const;
	[[nodiscard]] nodes_t nodes() const;

	[[nodiscard]] std::size_t group_count() const noexcept;
	[[nodiscard]] std::size_t size() const noexcept;

	[[nodiscard]] bool empty() const noexcept;
	[[nodiscard]] bool is_open() const noexcept;

	[[nodiscard]] executor_t get_executor() noexcept;

private:
	class impl;
	std::shared_ptr<impl> m_impl;
};

using gpio_manager = basic_gpio_manager<>;

} // namespace altun::subsys
#include <altun/linux/subsys/detail/gpio_manager.h>

#endif //__linux__
#endif //ALTUN_LINUX_SUBSYS_GPIO_MANAGER_H
