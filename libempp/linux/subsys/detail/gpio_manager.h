// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_SUBSYS_DETAIL_GPIO_MANAGER_H
#define LIBEMPP_LINUX_SUBSYS_DETAIL_GPIO_MANAGER_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

namespace libempp::subsys
{

template <riwo::concepts::exec Exec>
class LIBEMPP_LINUX_TAPI basic_gpio_manager<Exec>::impl
{
	RIWO_DISABLE_COPY_MOVE(impl)

private:
	using group_ptr = std::unique_ptr<group_t>;
	struct group_entry
	{
		std::filesystem::path chip;
		group_ptr group;
	};
	using group_list = std::vector<group_entry>;

	struct group_config
	{
		std::filesystem::path chip;
		group_t::line_configs_t lines;
	};

public:
	explicit impl(riwo::concepts::match_sched<Exec> auto &&exec) :
		m_exec(riwo::get_executor_helper(std::forward<decltype(exec)>(exec))) {}

	~impl() {
		close();
	}

public:
	void open(const node_t &node, std::error_code &error) noexcept
	{
		error.clear();
		try {
			if( const index_t index {.chip = node.chip, .line = node.line};
				node.chip.empty() or find(index) or (not node.alias.empty() and find(node.alias)) )
			{
				error = std::make_error_code(std::errc::invalid_argument);
				return ;
			}
			node_t staged_node = node;
			typename group_t::line_config_t config;

			config.line = node.line;
			config.direction = node.direction;
			config.initial_value = node.initial_value;
			config.active_low = node.active_low;
			config.edge = node.edge;
			config.consumer = node.consumer;
			config.alias = node.alias;

			m_nodes.reserve(m_nodes.size() + 1);
			if( auto *existing = find_group(node.chip) )
			{
				existing->open(node.chip, config, error);
				if( error )
					return ;

				m_nodes.push_back(std::move(staged_node));
				return ;
			}
			group_entry staged_group {
				.chip = node.chip,
				.group = std::make_unique<group_t>(m_exec)
			};
			m_groups.reserve(m_groups.size() + 1);
			staged_group.group->open(node.chip, config, error);

			if( error )
				return ;

			m_groups.push_back(std::move(staged_group));
			m_nodes.push_back(std::move(staged_node));
		}
		catch(...) {
			error = riwo::exception_error(std::current_exception());
		}
	}

	void open(const nodes_t &nodes, std::error_code &error) noexcept
	{
		close();
		error.clear();

		if( nodes.empty() )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return ;
		}
		for(auto iterator = nodes.begin(); iterator != nodes.end(); ++iterator)
		{
			if( iterator->chip.empty() )
			{
				error = std::make_error_code(std::errc::invalid_argument);
				return ;
			}
			if( std::ranges::find_if(iterator + 1, nodes.end(), [&iterator](const auto &node) {
					return iterator->line == node.line and equal_chip(iterator->chip, node.chip);
				}) != nodes.end() )
			{
				error = std::make_error_code(std::errc::invalid_argument);
				return ;
			}
			if( not iterator->alias.empty() and
				std::ranges::find(iterator + 1, nodes.end(), iterator->alias, &node_t::alias) != nodes.end() )
			{
				error = std::make_error_code(std::errc::invalid_argument);
				return ;
			}
		}
		group_list staged;
		nodes_t staged_nodes;
		try {
			staged_nodes = nodes;
			std::vector<group_config> grouped_lines {};

			for(const auto &node : nodes)
			{
				auto group = std::ranges::find_if(grouped_lines, [&node](const auto &config) {
					return equal_chip(config.chip, node.chip);
				});
				if( group == grouped_lines.end() )
				{
					grouped_lines.push_back({node.chip, {}});
					group = grouped_lines.end() - 1;
				}
				typename group_t::line_config_t config;
				config.line = node.line;
				config.direction = node.direction;
				config.initial_value = node.initial_value;
				config.active_low = node.active_low;
				config.edge = node.edge;
				config.consumer = node.consumer;
				config.alias = node.alias;
				group->lines.push_back(std::move(config));
			}
			staged.reserve(grouped_lines.size());

			for(const auto &config : grouped_lines)
			{
				auto group = std::make_unique<group_t>(m_exec);
				group->open(config.chip, config.lines, error);

				if( error )
					return ;
				staged.push_back({config.chip, std::move(group)});
			}
		}
		catch(...)
		{
			error = riwo::exception_error(std::current_exception());
			return ;
		}
		m_groups = std::move(staged);
		m_nodes = std::move(staged_nodes);
	}

	void close() noexcept
	{
		m_groups.clear();
		m_nodes.clear();
	}

	void close(const index_t &index) noexcept
	{
		const auto result = std::ranges::find_if(m_nodes, [&index](const auto &node) {
			return index.line == node.line and equal_chip(index.chip, node.chip);
		});
		close(result);
	}

	void close(const riwo::concepts::text_p<char> auto &alias) noexcept
	{
		const auto alias_view = riwo::strtls::to_view(alias);
		if( alias_view.empty() )
			return ;

		const auto result = std::ranges::find(m_nodes, alias_view, [](const auto &node) {
			return std::string_view(node.alias);
		});
		close(result);
	}

public:
	[[nodiscard]] group_t &group(const std::filesystem::path &chip)
	{
		if( auto *result = find_group(chip) )
			return *result;

		riwo::out_of_range::loc_throw (
			"libempp::subsys::basic_gpio_manager<Exec>::group: chip not found"
		);
		// return xxx;
	}

	[[nodiscard]] const group_t &group(const std::filesystem::path &chip) const
	{
		if( const auto *result = find_group(chip) )
			return *result;

		riwo::out_of_range::loc_throw (
			"libempp::subsys::basic_gpio_manager<Exec>::group: chip not found"
		);
		// return xxx;
	}

	[[nodiscard]] bool contains_chip(const std::filesystem::path &chip) const noexcept {
		return find_group(chip);
	}

	[[nodiscard]] gpio_t &at(const index_t &index)
	{
		if( auto *result = find(index) )
			return *result;

		riwo::out_of_range::loc_throw (
			"libempp::subsys::basic_gpio_manager<Exec>::at: index not found"
		);
		// return xxx;
	}

	[[nodiscard]] const gpio_t &at(const index_t &index) const
	{
		if( const auto *result = find(index) )
			return *result;

		riwo::out_of_range::loc_throw (
			"libempp::subsys::basic_gpio_manager<Exec>::at: index not found"
		);
		// return xxx;
	}

	[[nodiscard]] gpio_t &at(const riwo::concepts::text_p<char> auto &alias)
	{
		if( auto *result = find(alias) )
			return *result;

		riwo::out_of_range::loc_throw (
			"libempp::subsys::basic_gpio_manager<Exec>::at: alias not found"
		);
		// return xxx;
	}

	[[nodiscard]] const gpio_t &at(
		const riwo::concepts::text_p<char> auto &alias) const
	{
		if( const auto *result = find(alias) )
			return *result;

		riwo::out_of_range::loc_throw (
			"libempp::subsys::basic_gpio_manager<Exec>::at: alias not found"
		);
		// return xxx;
	}

	[[nodiscard]] bool contains(const index_t &index) const noexcept {
		return find(index);
	}

	[[nodiscard]] bool contains(const riwo::concepts::text_p<char> auto &alias) const noexcept {
		return find(alias);
	}

public:
	void set(const index_t &index, bool value, std::error_code &error) noexcept
	{
		if( auto *output = find(index) )
			output->set(value, error);
		else
			error = missing_key_error();
	}

	void set(const riwo::concepts::text_p<char> auto &alias, bool value, std::error_code &error) noexcept
	{
		if( auto *output = find(alias) )
			output->set(value, error);
		else
			error = missing_key_error();
	}

	void set(bool value, std::error_code &error) noexcept
	{
		error = validate_outputs();
		if( error )
			return ;

		for(const auto &node : m_nodes)
		{
			find(index_t {node.chip, node.line})->set(value, error);
			if( error )
				break;
		}
	}

	void set(const index_values_t &values, std::error_code &error) noexcept {
		set_impl(values, error);
	}

	void set(const alias_values_t &values, std::error_code &error) noexcept {
		set_impl(values, error);
	}

	void rising(const index_t &index, std::error_code &error) noexcept {
		set(index, true, error);
	}

	void rising(const indexes_t &indexes, std::error_code &error) noexcept {
		set_keys(indexes, true, error);
	}

	void rising(const aliases_t &aliases, std::error_code &error) noexcept {
		set_keys(aliases, true, error);
	}

	void rising(std::error_code &error) noexcept {
		set(true, error);
	}

	void rising(const riwo::concepts::text_p<char> auto &alias, std::error_code &error) noexcept {
		set(alias, true, error);
	}

	void falling(const index_t &index, std::error_code &error) noexcept {
		set(index, false, error);
	}

	void falling(const indexes_t &indexes, std::error_code &error) noexcept {
		set_keys(indexes, false, error);
	}

	void falling(const aliases_t &aliases, std::error_code &error) noexcept {
		set_keys(aliases, false, error);
	}

	void falling(std::error_code &error) noexcept {
		set(false, error);
	}

	void falling(const riwo::concepts::text_p<char> auto &alias, std::error_code &error) noexcept {
		set(alias, false, error);
	}

	void invert(const index_t &index, std::error_code &error) noexcept
	{
		if( auto *output = find(index) )
			output->invert(error);
		else
			error = missing_key_error();
	}

	void invert(const riwo::concepts::text_p<char> auto &alias, std::error_code &error) noexcept
	{
		if( auto *output = find(alias) )
			output->invert(error);
		else
			error = missing_key_error();
	}

	void invert(std::error_code &error) noexcept
	{
		error = validate_outputs();
		if( error )
			return ;

		for(const auto &node : m_nodes)
		{
			find(index_t {node.chip, node.line})->invert(error);
			if( error )
				break;
		}
	}

	[[nodiscard]] riwo::sys_expected<bool> get(const index_t &index) const noexcept
	{
		if( const auto *input = find(index) )
			return input->get();
		return riwo::sys_unexpected(missing_key_error());
	}

	[[nodiscard]] riwo::sys_expected<bool>
	get(const riwo::concepts::text_p<char> auto &alias) const noexcept
	{
		if( const auto *input = find(alias) )
			return input->get();
		return riwo::sys_unexpected(missing_key_error());
	}

	[[nodiscard]] riwo::sys_expected<index_values_t> get() const noexcept
	{
		index_values_t result;
		if( m_nodes.empty() )
		{
			return riwo::sys_unexpected (
				std::make_error_code(std::errc::bad_file_descriptor)
			);
		}
		try {
			for(const auto &node : m_nodes)
			{
				const index_t index {node.chip, node.line};
				auto current = find(index)->get();

				if( not current )
					return riwo::sys_unexpected(current.error());
				result.emplace(index, *current);
			}
		}
		catch(...)
		{
			return riwo::sys_unexpected (
				riwo::exception_error(std::current_exception())
			);
		}
		return result;
	}

public:
	[[nodiscard]] chips_t chips() const
	{
		chips_t result;
		result.reserve(m_groups.size());

		for(const auto &entry : m_groups)
			result.push_back(entry.chip);
		return result;
	}

	[[nodiscard]] nodes_t nodes() const {
		return m_nodes;
	}

	[[nodiscard]] std::size_t group_count() const noexcept {
		return m_groups.size();
	}

	[[nodiscard]] std::size_t size() const noexcept {
		return m_nodes.size();
	}

	[[nodiscard]] bool empty() const noexcept {
		return m_nodes.empty();
	}

	[[nodiscard]] bool is_open() const noexcept
	{
		return not m_groups.empty() and
			std::ranges::all_of(m_groups, [](const auto &entry) {
				return entry.group->is_open();
			});
	}

	[[nodiscard]] executor_t get_executor() noexcept {
		return m_exec;
	}

private:
	[[nodiscard]] group_t *find_group(const std::filesystem::path &chip) noexcept
	{
		const auto result = std::ranges::find_if(m_groups, [&chip](const auto &entry) {
			return equal_chip(entry.chip, chip);
		});
		return result == m_groups.end() ? nullptr : result->group.get();
	}

	[[nodiscard]] const group_t *find_group(const std::filesystem::path &chip) const noexcept
	{
		const auto result = std::ranges::find_if(m_groups, [&chip](const auto &entry) {
			return equal_chip(entry.chip, chip);
		});
		return result == m_groups.end() ? nullptr : result->group.get();
	}

	[[nodiscard]] gpio_t *find(const index_t &index) noexcept
	{
		auto *result = find_group(index.chip);
		return result and result->contains(index.line) ? &result->at(index.line) : nullptr;
	}

	[[nodiscard]] const gpio_t *find(const index_t &index) const noexcept
	{
		const auto *result = find_group(index.chip);
		return result and result->contains(index.line) ? &result->at(index.line) : nullptr;
	}

	[[nodiscard]] gpio_t *find(const riwo::concepts::text_p<char> auto &alias) noexcept
	{
		for(auto &entry : m_groups)
		{
			if( entry.group->contains(alias) )
				return &entry.group->at(alias);
		}
		return nullptr;
	}

	[[nodiscard]] const gpio_t *find(const riwo::concepts::text_p<char> auto &alias) const noexcept
	{
		for(const auto &entry : m_groups)
		{
			if( entry.group->contains(alias) )
				return &entry.group->at(alias);
		}
		return nullptr;
	}

	template <typename Values>
	void set_impl(const Values &values, std::error_code &error) noexcept
	{
		error.clear();
		std::vector<gpio_t*> outputs;
		try {
			outputs.reserve(values.size());
			for(const auto &[key, value] : values)
			{
				auto *output = find(key);
				if( not output )
				{
					error = missing_key_error();
					return ;
				}
				if( not output->is_open() )
				{
					error = std::make_error_code(std::errc::bad_file_descriptor);
					return ;
				}
				if( output->node().direction != gpio_direction_t::output )
				{
					error = std::make_error_code(std::errc::operation_not_permitted);
					return ;
				}
				outputs.push_back(output);
			}
		}
		catch(...)
		{
			error = riwo::exception_error(std::current_exception());
			return ;
		}
		auto output = outputs.begin();

		for(const auto &[key, value] : values)
		{
			(*output++)->set(value, error);
			if( error )
				break;
		}
	}

	template <typename Keys>
	void set_keys(const Keys &keys, bool value, std::error_code &error) noexcept
	{
		error.clear();
		try {
			for(const auto &key : keys)
			{
				const auto *output = find(key);
				if( not output )
				{
					error = missing_key_error();
					return ;
				}
				if( not output->is_open() )
				{
					error = std::make_error_code(std::errc::bad_file_descriptor);
					return ;
				}
				if( output->node().direction != gpio_direction_t::output )
				{
					error = std::make_error_code(std::errc::operation_not_permitted);
					return ;
				}
			}
		}
		catch(...)
		{
			error = riwo::exception_error(std::current_exception());
			return ;
		}
		for(const auto &key : keys)
		{
			find(key)->set(value, error);
			if( error )
				break;
		}
	}

	[[nodiscard]] std::error_code validate_outputs() const noexcept
	{
		if( m_nodes.empty() )
			return std::make_error_code(std::errc::bad_file_descriptor);

		for(const auto &node : m_nodes)
		{
			const auto *output = find(index_t {
				.chip = node.chip, .line = node.line
			});
			if( not output or not output->is_open() )
				return std::make_error_code(std::errc::bad_file_descriptor);

			if( node.direction != gpio_direction_t::output )
				return std::make_error_code(std::errc::operation_not_permitted);
		}
		return {};
	}

	[[nodiscard]] static std::error_code missing_key_error() noexcept {
		return std::make_error_code(std::errc::no_such_device_or_address);
	}

	[[nodiscard]] static bool equal_chip
	(const std::filesystem::path &left, const std::filesystem::path &right) noexcept
	{
		return index_t::compare_chip(left, right) == std::strong_ordering::equal;
	}

	void close(nodes_t::iterator iterator) noexcept
	{
		if( iterator == m_nodes.end() )
			return ;

		const auto group = std::ranges::find_if(m_groups, [iterator](const auto &entry) {
			return equal_chip(entry.chip, iterator->chip);
		});
		if( group != m_groups.end() )
		{
			group->group->close(iterator->line);
			if( group->group->empty() )
				m_groups.erase(group);
		}
		m_nodes.erase(iterator);
	}

private:
	executor_t m_exec {};
	group_list m_groups {};
	nodes_t m_nodes {};
};

template <riwo::concepts::exec Exec>
template <typename Exec0>
basic_gpio_manager<Exec>::basic_gpio_manager(const nodes_t &nodes, Exec0 &&exec) requires (
	not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio_manager> and
	riwo::concepts::match_sched<Exec0,executor_t>
) : m_impl(std::make_shared<impl>(std::forward<decltype(exec)>(exec)))
{
	open(nodes);
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec>::basic_gpio_manager(const nodes_t &nodes)
	requires riwo::concepts::match_def_exec<executor_t> :
	basic_gpio_manager(nodes, riwo::io_context())
{

}

template <riwo::concepts::exec Exec>
template <typename Exec0>
basic_gpio_manager<Exec>::basic_gpio_manager(std::initializer_list<node_t> nodes, Exec0 &&exec) requires (
	not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio_manager> and
	riwo::concepts::match_sched<Exec0,executor_t>
) : basic_gpio_manager(nodes_t(nodes), std::forward<decltype(exec)>(exec))
{

}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec>::basic_gpio_manager(std::initializer_list<node_t> nodes)
	requires riwo::concepts::match_def_exec<executor_t> :
	basic_gpio_manager(nodes_t(nodes), riwo::io_context())
{

}

template <riwo::concepts::exec Exec>
template <typename Exec0>
basic_gpio_manager<Exec>::basic_gpio_manager(Exec0 &&exec) requires (
	not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio_manager> and
	riwo::concepts::match_sched<Exec0,executor_t>
) : m_impl(std::make_shared<impl>(std::forward<decltype(exec)>(exec)))
{

}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec>::basic_gpio_manager()
	requires riwo::concepts::match_def_exec<Exec> :
	basic_gpio_manager(riwo::io_context())
{

}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec>::~basic_gpio_manager() = default;

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec>::basic_gpio_manager(basic_gpio_manager &&other) noexcept :
	m_impl(std::move(other.m_impl))
{
	other.m_impl = std::make_shared<impl>(m_impl->get_executor());
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::operator=(basic_gpio_manager &&other) noexcept
{
	if( this == &other )
		return *this;
	m_impl = std::move(other.m_impl);
	other.m_impl = std::make_shared<impl>(m_impl->get_executor());
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::open(const node_t &node, std::error_code &error) noexcept
{
	m_impl->open(node, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::open(const node_t &node)
{
	std::error_code error;
	open(node, error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::open"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::open
(const nodes_t &nodes, std::error_code &error) noexcept
{
	m_impl->open(nodes, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::open(const nodes_t &nodes)
{
	std::error_code error;
	open(nodes, error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::open"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::open
(std::initializer_list<node_t> nodes, std::error_code &error) noexcept
{
	try {
		return open(nodes_t(nodes), error);
	}
	catch(...)
	{
		close();
		error = riwo::exception_error(std::current_exception());
		return *this;
	}
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::open(std::initializer_list<node_t> nodes)
{
	return open(nodes_t(nodes));
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::close(const index_t &index) noexcept
{
	m_impl->close(index);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::close
(const riwo::concepts::text_p<char> auto &alias) noexcept
{
	m_impl->close(alias);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::close() noexcept
{
	m_impl->close();
	return *this;
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::group(const std::filesystem::path &chip) -> group_t&
{
	return m_impl->group(chip);
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::group(const std::filesystem::path &chip) const -> const group_t&
{
	return m_impl->group(chip);
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::at(const index_t &index) -> gpio_t&
{
	return m_impl->at(index);
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::at(const index_t &index) const -> const gpio_t&
{
	return m_impl->at(index);
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::at(const riwo::concepts::text_p<char> auto &alias) -> gpio_t&
{
	return m_impl->at(alias);
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::at
(const riwo::concepts::text_p<char> auto &alias) const -> const gpio_t&
{
	return m_impl->at(alias);
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::operator[](const index_t &index) -> gpio_t&
{
	return at(index);
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::operator[](const index_t &index) const -> const gpio_t&
{
	return at(index);
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::operator[](const riwo::concepts::text_p<char> auto &alias) -> gpio_t&
{
	return at(alias);
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::operator[]
(const riwo::concepts::text_p<char> auto &alias) const -> const gpio_t&
{
	return at(alias);
}

template <riwo::concepts::exec Exec>
bool basic_gpio_manager<Exec>::contains_chip(const std::filesystem::path &chip) const noexcept
{
	return m_impl->contains_chip(chip);
}

template <riwo::concepts::exec Exec>
bool basic_gpio_manager<Exec>::contains(const index_t &index) const noexcept
{
	return m_impl->contains(index);
}

template <riwo::concepts::exec Exec>
bool basic_gpio_manager<Exec>::contains(const riwo::concepts::text_p<char> auto &alias) const noexcept
{
	return m_impl->contains(alias);
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::set
(const index_t &index, bool value, std::error_code &error) noexcept
{
	m_impl->set(index, value, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::set(const index_t &index, bool value)
{
	std::error_code error;
	set(index, value, error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::set"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::set
(const index_values_t &values, std::error_code &error) noexcept
{
	m_impl->set(values, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::set(const index_values_t &values)
{
	std::error_code error;
	set(values, error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::set"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::set
(const alias_values_t &values, std::error_code &error) noexcept
{
	m_impl->set(values, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::set(const alias_values_t &values)
{
	std::error_code error;
	set(values, error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::set"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::set(bool value, std::error_code &error) noexcept
{
	m_impl->set(value, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::set(bool value)
{
	std::error_code error;
	set(value, error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::set"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::set
(const riwo::concepts::text_p<char> auto &alias, bool value, std::error_code &error) noexcept
{
	m_impl->set(alias, value, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::set
(const riwo::concepts::text_p<char> auto &alias, bool value)
{
	std::error_code error;
	set(alias, value, error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::set"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::rising
(index_t index, std::error_code &error) noexcept
{
	m_impl->rising(index, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::rising(index_t index)
{
	std::error_code error;
	rising(std::move(index), error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::rising"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::rising
(const indexes_t &indexes, std::error_code &error) noexcept
{
	m_impl->rising(indexes, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::rising(const indexes_t &indexes)
{
	std::error_code error;
	rising(indexes, error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::rising"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::rising
(const aliases_t &aliases, std::error_code &error) noexcept
{
	m_impl->rising(aliases, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::rising(const aliases_t &aliases)
{
	std::error_code error;
	rising(aliases, error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::rising"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::rising(std::error_code &error) noexcept
{
	m_impl->rising(error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::rising()
{
	std::error_code error;
	rising(error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::rising"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::rising
(const riwo::concepts::text_p<char> auto &alias, std::error_code &error) noexcept
{
	m_impl->rising(alias, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::rising(const riwo::concepts::text_p<char> auto &alias)
{
	std::error_code error;
	rising(alias, error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::rising"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::falling(index_t index, std::error_code &error) noexcept
{
	m_impl->falling(index, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::falling(index_t index)
{
	std::error_code error;
	falling(std::move(index), error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::falling"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::falling
(const indexes_t &indexes, std::error_code &error) noexcept
{
	m_impl->falling(indexes, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::falling(const indexes_t &indexes)
{
	std::error_code error;
	falling(indexes, error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::falling"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::falling
(const aliases_t &aliases, std::error_code &error) noexcept
{
	m_impl->falling(aliases, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::falling(const aliases_t &aliases)
{
	std::error_code error;
	falling(aliases, error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::falling"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::falling(std::error_code &error) noexcept
{
	m_impl->falling(error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::falling()
{
	std::error_code error;
	falling(error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::falling"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::falling
(const riwo::concepts::text_p<char> auto &alias, std::error_code &error) noexcept
{
	m_impl->falling(alias, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::falling
(const riwo::concepts::text_p<char> auto &alias)
{
	std::error_code error;
	falling(alias, error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::falling"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::invert
(const index_t &index, std::error_code &error) noexcept
{
	m_impl->invert(index, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::invert(const index_t &index)
{
	std::error_code error;
	invert(index, error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::invert"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::invert(std::error_code &error) noexcept
{
	m_impl->invert(error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::invert()
{
	std::error_code error;
	invert(error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::invert"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::invert
(const riwo::concepts::text_p<char> auto &alias, std::error_code &error) noexcept
{
	m_impl->invert(alias, error);
	return *this;
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec> &basic_gpio_manager<Exec>::invert(const riwo::concepts::text_p<char> auto &alias)
{
	std::error_code error;
	invert(alias, error);
	if( error )
	{
		riwo::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_manager<Exec>::invert"
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::get() const noexcept -> riwo::sys_expected<index_values_t>
{
	return m_impl->get();
}

template <riwo::concepts::exec Exec>
riwo::sys_expected<bool> basic_gpio_manager<Exec>::get(const index_t &index) const noexcept
{
	return m_impl->get(index);
}

template <riwo::concepts::exec Exec>
riwo::sys_expected<bool> basic_gpio_manager<Exec>::get
(const riwo::concepts::text_p<char> auto &alias) const noexcept
{
	return m_impl->get(alias);
}

template <riwo::concepts::exec Exec>
basic_gpio_manager<Exec>::operator index_values_t() const
{
	return riwo::expected_value_or_throw(get());
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::operator*() const -> index_values_t
{
	return riwo::expected_value_or_throw(get());
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::operator~() const -> index_values_t
{
	auto values = riwo::expected_value_or_throw(get());
	for(auto &[index, value] : values)
	{
		(void)index;
		value = not value;
	}
	return values;
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::chips() const -> chips_t
{
	return m_impl->chips();
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::nodes() const -> nodes_t
{
	return m_impl->nodes();
}

template <riwo::concepts::exec Exec>
std::size_t basic_gpio_manager<Exec>::group_count() const noexcept
{
	return m_impl->group_count();
}

template <riwo::concepts::exec Exec>
std::size_t basic_gpio_manager<Exec>::size() const noexcept
{
	return m_impl->size();
}

template <riwo::concepts::exec Exec>
bool basic_gpio_manager<Exec>::empty() const noexcept
{
	return m_impl->empty();
}

template <riwo::concepts::exec Exec>
bool basic_gpio_manager<Exec>::is_open() const noexcept
{
	return m_impl->is_open();
}

template <riwo::concepts::exec Exec>
auto basic_gpio_manager<Exec>::get_executor() noexcept -> executor_t
{
	return m_impl->get_executor();
}

} // namespace libempp::subsys

#endif //__linux__
#endif //LIBEMPP_LINUX_SUBSYS_DETAIL_GPIO_MANAGER_H
