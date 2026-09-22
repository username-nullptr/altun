// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_SUBSYS_DETAIL_GPIO_GROUP_H
#define LIBEMPP_LINUX_SUBSYS_DETAIL_GPIO_GROUP_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

namespace libempp::subsys
{

template <libgs::concepts::exec Exec>
class LIBEMPP_LINUX_TAPI basic_gpio_group<Exec>::impl
{
	LIBGS_DISABLE_COPY_MOVE(impl)

private:
	using gpio_ptr = std::unique_ptr<gpio_t>;
	struct gpio_entry
	{
		line_config_t config;
		gpio_ptr gpio;
	};
	using gpio_list = std::vector<gpio_entry>;

public:
	explicit impl(libgs::concepts::match_sched<Exec> auto &&exec) :
		m_exec(libgs::get_executor_helper(std::forward<decltype(exec)>(exec))) {}

	~impl() {
		close();
	}

public:
	void open(const std::filesystem::path &chip, const line_config_t &config, std::error_code &error) noexcept
	{
		error.clear();
		if( chip.empty() or
			(not m_gpios.empty() and not equal_chip(m_chip, chip)) or
			find(config.line) or (not config.alias.empty() and find(config.alias)) )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return ;
		}
		try {
			std::filesystem::path staged_chip = chip;
			gpio_entry staged {
				.config = config,
				.gpio = std::make_unique<gpio_t>(m_exec)
			};
			gpio_node_t node;
			node.chip = chip;
			node.line = config.line;
			node.direction = config.direction;
			node.initial_value = config.initial_value;
			node.active_low = config.active_low;
			node.edge = config.edge;
			node.consumer = config.consumer;
			node.alias = config.alias;

			staged.gpio->open(node, error);
			if( error )
				return ;

			m_gpios.push_back(std::move(staged));
			if( m_gpios.size() == 1 )
				m_chip.swap(staged_chip);
		}
		catch(...) {
			error = libgs::exception_error(std::current_exception());
		}
	}

	void open(const std::filesystem::path &chip, const line_configs_t &lines,
		std::error_code &error) noexcept
	{
		close();
		error.clear();

		if( chip.empty() or lines.empty() )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return ;
		}
		for(auto iterator = lines.begin(); iterator != lines.end(); ++iterator)
		{
			if( std::ranges::find(iterator + 1, lines.end(), iterator->line,
				&line_config_t::line) != lines.end() )
			{
				error = std::make_error_code(std::errc::invalid_argument);
				return ;
			}
			if( not iterator->alias.empty() and
				std::ranges::find(iterator + 1, lines.end(), iterator->alias,
					&line_config_t::alias) != lines.end() )
			{
				error = std::make_error_code(std::errc::invalid_argument);
				return ;
			}
		}
		gpio_list staged;
		std::filesystem::path staged_chip;
		try {
			staged_chip = chip;
			staged.reserve(lines.size());

			for(const auto &config : lines)
			{
				gpio_node_t node;
				node.chip = chip;
				node.line = config.line;
				node.direction = config.direction;
				node.initial_value = config.initial_value;
				node.active_low = config.active_low;
				node.edge = config.edge;
				node.consumer = config.consumer;
				node.alias = config.alias;

				auto line = std::make_unique<gpio_t>(m_exec);
				line->open(node, error);

				if( error )
					return ;
				staged.push_back({config, std::move(line)});
			}
		}
		catch(...)
		{
			error = libgs::exception_error(std::current_exception());
			return ;
		}
		m_gpios = std::move(staged);
		m_chip = std::move(staged_chip);
	}

	void close() noexcept
	{
		m_gpios.clear();
		m_chip.clear();
	}

	void close(line_t line) noexcept
	{
		const auto result = std::ranges::find(m_gpios, line, [](const auto &entry) {
			return entry.config.line;
		});
		close(result);
	}

	void close(const libgs::concepts::text_p<char> auto &alias) noexcept
	{
		const auto alias_view = libgs::strtls::to_view(alias);
		if( alias_view.empty() )
			return ;

		const auto result = std::ranges::find(m_gpios, alias_view, [](const auto &entry) {
			return std::string_view(entry.config.alias);
		});
		close(result);
	}

public:
	[[nodiscard]] gpio_t &at(line_t line)
	{
		if( auto *result = find(line) )
			return *result;

		libgs::out_of_range::loc_throw (
			"libempp::subsys::basic_gpio_group<Exec>::at: line not found"
		);
		// return xxx;
	}

	[[nodiscard]] const gpio_t &at(line_t line) const
	{
		if( const auto *result = find(line) )
			return *result;

		libgs::out_of_range::loc_throw (
			"libempp::subsys::basic_gpio_group<Exec>::at: line not found"
		);
		// return xxx;
	}

	[[nodiscard]] gpio_t &at(const libgs::concepts::text_p<char> auto &alias)
	{
		if( auto *result = find(alias) )
			return *result;

		libgs::out_of_range::loc_throw (
			"libempp::subsys::basic_gpio_group<Exec>::at: alias not found"
		);
		// return xxx;
	}

	[[nodiscard]] const gpio_t &at(const libgs::concepts::text_p<char> auto &alias) const
	{
		if( const auto *result = find(alias) )
			return *result;

		libgs::out_of_range::loc_throw (
			"libempp::subsys::basic_gpio_group<Exec>::at: alias not found"
		);
		// return xxx;
	}

	[[nodiscard]] bool contains(line_t line) const noexcept {
		return find(line);
	}

	[[nodiscard]] bool contains(const libgs::concepts::text_p<char> auto &alias) const noexcept {
		return find(alias);
	}

public:
	void set(line_t line, bool value, std::error_code &error) noexcept
	{
		if( auto *output = find(line) )
			output->set(value, error);
		else
			error = missing_key_error();
	}

	void set(const libgs::concepts::text_p<char> auto &alias, bool value, std::error_code &error) noexcept
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

		for(auto &output : m_gpios)
		{
			output.gpio->set(value, error);
			if( error )
				break;
		}
	}

	void set(const line_values_t &values,
		std::error_code &error) noexcept
	{
		set_impl(values, error);
	}

	void set(const alias_values_t &values, std::error_code &error) noexcept
	{
		set_impl(values, error);
	}

	void rising(line_t line, std::error_code &error) noexcept {
		set(line, true, error);
	}

	void rising(const lines_t &lines, std::error_code &error) noexcept {
		set_keys(lines, true, error);
	}

	void rising(const aliases_t &aliases, std::error_code &error) noexcept {
		set_keys(aliases, true, error);
	}

	void rising(std::error_code &error) noexcept {
		set(true, error);
	}

	void rising(const libgs::concepts::text_p<char> auto &alias, std::error_code &error) noexcept {
		set(alias, true, error);
	}

	void falling(line_t line, std::error_code &error) noexcept {
		set(line, false, error);
	}

	void falling(const lines_t &lines, std::error_code &error) noexcept {
		set_keys(lines, false, error);
	}

	void falling(const aliases_t &aliases, std::error_code &error) noexcept {
		set_keys(aliases, false, error);
	}

	void falling(std::error_code &error) noexcept {
		set(false, error);
	}

	void falling(const libgs::concepts::text_p<char> auto &alias, std::error_code &error) noexcept {
		set(alias, false, error);
	}

	void invert(line_t line, std::error_code &error) noexcept
	{
		if( auto *output = find(line) )
			output->invert(error);
		else
			error = missing_key_error();
	}

	void invert(const libgs::concepts::text_p<char> auto &alias, std::error_code &error) noexcept
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

		for(auto &output : m_gpios)
		{
			output.gpio->invert(error);
			if( error )
				break;
		}
	}

	[[nodiscard]] libgs::sys_expected<bool> get(line_t line) const noexcept
	{
		if( const auto *input = find(line) )
			return input->get();
		return libgs::sys_unexpected(missing_key_error());
	}

	[[nodiscard]] libgs::sys_expected<bool> get
	(const libgs::concepts::text_p<char> auto &alias) const noexcept
	{
		if( const auto *input = find(alias) )
			return input->get();
		return libgs::sys_unexpected(missing_key_error());
	}

	[[nodiscard]] libgs::sys_expected<line_values_t> get() const noexcept
	{
		line_values_t result;
		if( m_gpios.empty() )
		{
			return libgs::sys_unexpected (
				std::make_error_code(std::errc::bad_file_descriptor)
			);
		}
		try {
			for(const auto &input : m_gpios)
			{
				auto current = input.gpio->get();
				if( not current )
					return libgs::sys_unexpected(current.error());
				result.emplace(input.config.line, *current);
			}
		}
		catch(...)
		{
			return libgs::sys_unexpected (
				libgs::exception_error(std::current_exception())
			);
		}
		return result;
	}

public:
	[[nodiscard]] std::filesystem::path chip() const {
		return m_chip;
	}

	[[nodiscard]] line_configs_t lines() const
	{
		line_configs_t result;
		result.reserve(m_gpios.size());

		for(const auto &line : m_gpios)
			result.push_back(line.config);
		return result;
	}

	[[nodiscard]] std::size_t size() const noexcept {
		return m_gpios.size();
	}

	[[nodiscard]] bool empty() const noexcept {
		return m_gpios.empty();
	}

	[[nodiscard]] bool is_open() const noexcept
	{
		return not m_gpios.empty() and std::ranges::all_of(m_gpios, [](const auto &line) {
			return line.gpio->is_open();
		});
	}

	[[nodiscard]] executor_t get_executor() noexcept {
		return m_exec;
	}

private:
	[[nodiscard]] gpio_t *find(line_t line) noexcept
	{
		const auto result = std::ranges::find(m_gpios, line, [](const auto &entry) {
			return entry.config.line;
		});
		return result == m_gpios.end() ? nullptr : result->gpio.get();
	}

	[[nodiscard]] const gpio_t *find(line_t line) const noexcept
	{
		const auto result = std::ranges::find(m_gpios, line, [](const auto &entry) {
			return entry.config.line;
		});
		return result == m_gpios.end() ? nullptr : result->gpio.get();
	}

	[[nodiscard]] gpio_t *find
	(const libgs::concepts::text_p<char> auto &alias) noexcept
	{
		const auto alias_view = libgs::strtls::to_view(alias);
		if( alias_view.empty() )
			return nullptr;

		const auto result = std::ranges::find(m_gpios, alias_view, [](const auto &entry) {
			return std::string_view(entry.config.alias);
		});
		return result == m_gpios.end() ? nullptr : result->gpio.get();
	}

	[[nodiscard]] const gpio_t *find
	(const libgs::concepts::text_p<char> auto &alias) const noexcept
	{
		const auto alias_view = libgs::strtls::to_view(alias);
		if( alias_view.empty() )
			return nullptr;

		const auto result = std::ranges::find(m_gpios, alias_view, [](const auto &entry) {
			return std::string_view(entry.config.alias);
		});
		return result == m_gpios.end() ? nullptr : result->gpio.get();
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
			error = libgs::exception_error(std::current_exception());
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
			error = libgs::exception_error(std::current_exception());
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
		if( m_gpios.empty() )
			return std::make_error_code(std::errc::bad_file_descriptor);

		for(const auto &output : m_gpios)
		{
			if( not output.gpio->is_open() )
				return std::make_error_code(std::errc::bad_file_descriptor);

			if( output.config.direction != gpio_direction_t::output )
				return std::make_error_code(std::errc::operation_not_permitted);
		}
		return {};
	}

	[[nodiscard]] static std::error_code missing_key_error() noexcept {
		return std::make_error_code(std::errc::no_such_device_or_address);
	}

	void close(gpio_list::iterator iterator) noexcept
	{
		if( iterator == m_gpios.end() )
			return ;
		m_gpios.erase(iterator);
		if( m_gpios.empty() )
			m_chip.clear();
	}

	[[nodiscard]] static bool equal_chip
	(const std::filesystem::path &left, const std::filesystem::path &right) noexcept
	{
		const std::string_view left_view(left.native());
		const std::string_view right_view(right.native());
		constexpr std::string_view device_prefix = "/dev/";

		const bool left_bare = left_view.find('/') == std::string_view::npos;
		const bool right_bare = right_view.find('/') == std::string_view::npos;

		if( left_bare == right_bare )
			return left_view == right_view;

		return left_bare ?
			right_view.starts_with(device_prefix) and right_view.substr(device_prefix.size()) == left_view :
			left_view.starts_with(device_prefix) and left_view.substr(device_prefix.size()) == right_view;
	}

private:
	executor_t m_exec {};
	std::filesystem::path m_chip {};
	gpio_list m_gpios {};
};

template <libgs::concepts::exec Exec>
template <typename Exec0>
basic_gpio_group<Exec>::basic_gpio_group
(const std::filesystem::path &chip, const line_configs_t &lines, Exec0 &&exec) requires (
	not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio_group> and
	libgs::concepts::match_sched<Exec0,executor_t>
) : m_impl(std::make_shared<impl>(std::forward<decltype(exec)>(exec)))
{
	open(chip, lines);
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec>::basic_gpio_group(const std::filesystem::path &chip, const line_configs_t &lines)
	requires libgs::concepts::match_def_exec<executor_t> :
	basic_gpio_group(chip, lines, libgs::io_context())
{

}

template <libgs::concepts::exec Exec>
template <typename Exec0>
basic_gpio_group<Exec>::basic_gpio_group
(const std::filesystem::path &chip, std::initializer_list<line_config_t> lines, Exec0 &&exec) requires (
	not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio_group> and
	libgs::concepts::match_sched<Exec0,executor_t>
) : basic_gpio_group(chip, line_configs_t(lines),
	std::forward<decltype(exec)>(exec))
{

}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec>::basic_gpio_group
(const std::filesystem::path &chip, std::initializer_list<line_config_t> lines)
	requires libgs::concepts::match_def_exec<executor_t> :
	basic_gpio_group(chip, line_configs_t(lines), libgs::io_context())
{

}

template <libgs::concepts::exec Exec>
template <typename Exec0>
basic_gpio_group<Exec>::basic_gpio_group(Exec0 &&exec) requires (
	not std::same_as<std::remove_cvref_t<Exec0>,basic_gpio_group> and
	libgs::concepts::match_sched<Exec0,executor_t>
) : m_impl(std::make_shared<impl>(std::forward<decltype(exec)>(exec)))
{

}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec>::basic_gpio_group()
	requires libgs::concepts::match_def_exec<Exec> :
	basic_gpio_group(libgs::io_context())
{

}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec>::~basic_gpio_group() = default;

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec>::basic_gpio_group(basic_gpio_group &&other) noexcept :
	m_impl(std::move(other.m_impl))
{
	other.m_impl = std::make_shared<impl>(m_impl->get_executor());
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::operator=(basic_gpio_group &&other) noexcept
{
	if( this == &other )
		return *this;

	m_impl = std::move(other.m_impl);
	other.m_impl = std::make_shared<impl>(m_impl->get_executor());
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::open
(const std::filesystem::path &chip, const line_config_t &line, std::error_code &error) noexcept
{
	m_impl->open(chip, line, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::open
(const std::filesystem::path &chip, const line_config_t &line)
{
	std::error_code error;
	open(chip, line, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::open"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::open
(const std::filesystem::path &chip, const line_configs_t &lines, std::error_code &error) noexcept
{
	m_impl->open(chip, lines, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::open
(const std::filesystem::path &chip, const line_configs_t &lines)
{
	std::error_code error;
	open(chip, lines, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::open"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::open
(const std::filesystem::path &chip, std::initializer_list<line_config_t> lines, std::error_code &error) noexcept
{
	try {
		return open(chip, line_configs_t(lines), error);
	}
	catch(...)
	{
		close();
		error = libgs::exception_error(std::current_exception());
		return *this;
	}
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::open
(const std::filesystem::path &chip, std::initializer_list<line_config_t> lines)
{
	return open(chip, line_configs_t(lines));
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::close(line_t line) noexcept
{
	m_impl->close(line);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::close
(const libgs::concepts::text_p<char> auto &alias) noexcept
{
	m_impl->close(alias);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::close() noexcept
{
	m_impl->close();
	return *this;
}

template <libgs::concepts::exec Exec>
auto basic_gpio_group<Exec>::at(line_t line) -> gpio_t&
{
	return m_impl->at(line);
}

template <libgs::concepts::exec Exec>
auto basic_gpio_group<Exec>::at(line_t line) const -> const gpio_t&
{
	return m_impl->at(line);
}

template <libgs::concepts::exec Exec>
auto basic_gpio_group<Exec>::at(const libgs::concepts::text_p<char> auto &alias) -> gpio_t&
{
	return m_impl->at(alias);
}

template <libgs::concepts::exec Exec>
auto basic_gpio_group<Exec>::at
(const libgs::concepts::text_p<char> auto &alias) const -> const gpio_t&
{
	return m_impl->at(alias);
}

template <libgs::concepts::exec Exec>
auto basic_gpio_group<Exec>::operator[](line_t line) -> gpio_t&
{
	return at(line);
}

template <libgs::concepts::exec Exec>
auto basic_gpio_group<Exec>::operator[](line_t line) const -> const gpio_t&
{
	return at(line);
}

template <libgs::concepts::exec Exec>
auto basic_gpio_group<Exec>::operator[](const libgs::concepts::text_p<char> auto &alias) -> gpio_t&
{
	return at(alias);
}

template <libgs::concepts::exec Exec>
auto basic_gpio_group<Exec>::operator[]
(const libgs::concepts::text_p<char> auto &alias) const -> const gpio_t&
{
	return at(alias);
}

template <libgs::concepts::exec Exec>
bool basic_gpio_group<Exec>::contains(line_t line) const noexcept
{
	return m_impl->contains(line);
}

template <libgs::concepts::exec Exec>
bool basic_gpio_group<Exec>::contains(const libgs::concepts::text_p<char> auto &alias) const noexcept
{
	return m_impl->contains(alias);
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::set
(line_t line, bool value, std::error_code &error) noexcept
{
	m_impl->set(line, value, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::set(line_t line, bool value)
{
	std::error_code error;
	set(line, value, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::set"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::set
(const line_values_t &values, std::error_code &error) noexcept
{
	m_impl->set(values, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::set(const line_values_t &values)
{
	std::error_code error;
	set(values, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::set"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::set
(const alias_values_t &values, std::error_code &error) noexcept
{
	m_impl->set(values, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::set(const alias_values_t &values)
{
	std::error_code error;
	set(values, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::set"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::set(bool value, std::error_code &error) noexcept
{
	m_impl->set(value, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::set(bool value)
{
	std::error_code error;
	set(value, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::set"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::set
(const libgs::concepts::text_p<char> auto &alias, bool value, std::error_code &error) noexcept
{
	m_impl->set(alias, value, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::set
(const libgs::concepts::text_p<char> auto &alias, bool value)
{
	std::error_code error;
	set(alias, value, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::set"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::rising(line_t line, std::error_code &error) noexcept
{
	m_impl->rising(line, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::rising(line_t line)
{
	std::error_code error;
	rising(line, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::rising"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::rising
(const lines_t &lines, std::error_code &error) noexcept
{
	m_impl->rising(lines, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::rising(const lines_t &lines)
{
	std::error_code error;
	rising(lines, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::rising"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::rising
(const aliases_t &aliases, std::error_code &error) noexcept
{
	m_impl->rising(aliases, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::rising(const aliases_t &aliases)
{
	std::error_code error;
	rising(aliases, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::rising"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::rising(std::error_code &error) noexcept
{
	m_impl->rising(error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::rising()
{
	std::error_code error;
	rising(error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::rising"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::rising
(const libgs::concepts::text_p<char> auto &alias, std::error_code &error) noexcept
{
	m_impl->rising(alias, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::rising(const libgs::concepts::text_p<char> auto &alias)
{
	std::error_code error;
	rising(alias, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::rising"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::falling(line_t line, std::error_code &error) noexcept
{
	m_impl->falling(line, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::falling(line_t line)
{
	std::error_code error;
	falling(line, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::falling"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::falling
(const lines_t &lines, std::error_code &error) noexcept
{
	m_impl->falling(lines, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::falling(const lines_t &lines)
{
	std::error_code error;
	falling(lines, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::falling"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::falling
(const aliases_t &aliases, std::error_code &error) noexcept
{
	m_impl->falling(aliases, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::falling(const aliases_t &aliases)
{
	std::error_code error;
	falling(aliases, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::falling"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::falling(std::error_code &error) noexcept
{
	m_impl->falling(error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::falling()
{
	std::error_code error;
	falling(error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::falling"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::falling
(const libgs::concepts::text_p<char> auto &alias, std::error_code &error) noexcept
{
	m_impl->falling(alias, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::falling
(const libgs::concepts::text_p<char> auto &alias)
{
	std::error_code error;
	falling(alias, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::falling"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::invert(line_t line, std::error_code &error) noexcept
{
	m_impl->invert(line, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::invert(line_t line)
{
	std::error_code error;
	invert(line, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::invert"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::invert(std::error_code &error) noexcept
{
	m_impl->invert(error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::invert()
{
	std::error_code error;
	invert(error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::invert"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::invert
(const libgs::concepts::text_p<char> auto &alias,
	std::error_code &error) noexcept
{
	m_impl->invert(alias, error);
	return *this;
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec> &basic_gpio_group<Exec>::invert(const libgs::concepts::text_p<char> auto &alias)
{
	std::error_code error;
	invert(alias, error);
	if( error )
	{
		libgs::system_error::loc_throw(error,
			"libempp::subsys::basic_gpio_group<Exec>::invert"
		);
	}
	return *this;
}

template <libgs::concepts::exec Exec>
auto basic_gpio_group<Exec>::get() const noexcept -> libgs::sys_expected<line_values_t>
{
	return m_impl->get();
}

template <libgs::concepts::exec Exec>
libgs::sys_expected<bool> basic_gpio_group<Exec>::get(line_t line) const noexcept
{
	return m_impl->get(line);
}

template <libgs::concepts::exec Exec>
libgs::sys_expected<bool> basic_gpio_group<Exec>::get
(const libgs::concepts::text_p<char> auto &alias) const noexcept
{
	return m_impl->get(alias);
}

template <libgs::concepts::exec Exec>
basic_gpio_group<Exec>::operator line_values_t() const
{
	return libgs::expected_value_or_throw(get());
}

template <libgs::concepts::exec Exec>
auto basic_gpio_group<Exec>::operator*() const -> line_values_t
{
	return libgs::expected_value_or_throw(get());
}

template <libgs::concepts::exec Exec>
auto basic_gpio_group<Exec>::operator~() const -> line_values_t
{
	auto values = libgs::expected_value_or_throw(get());
	for(auto &[line, value] : values)
	{
		(void)line;
		value = not value;
	}
	return values;
}

template <libgs::concepts::exec Exec>
std::filesystem::path basic_gpio_group<Exec>::chip() const
{
	return m_impl->chip();
}

template <libgs::concepts::exec Exec>
auto basic_gpio_group<Exec>::lines() const -> line_configs_t
{
	return m_impl->lines();
}

template <libgs::concepts::exec Exec>
std::size_t basic_gpio_group<Exec>::size() const noexcept
{
	return m_impl->size();
}

template <libgs::concepts::exec Exec>
bool basic_gpio_group<Exec>::empty() const noexcept
{
	return m_impl->empty();
}

template <libgs::concepts::exec Exec>
bool basic_gpio_group<Exec>::is_open() const noexcept
{
	return m_impl->is_open();
}

template <libgs::concepts::exec Exec>
auto basic_gpio_group<Exec>::get_executor() noexcept -> executor_t
{
	return m_impl->get_executor();
}

} // namespace libempp::subsys

#endif //__linux__
#endif //LIBEMPP_LINUX_SUBSYS_DETAIL_GPIO_GROUP_H
