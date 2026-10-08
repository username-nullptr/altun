// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_CORE_PLUGIN_MANAGER_H
#define LIBEMPP_CORE_PLUGIN_MANAGER_H

#include <libempp/core/global.h>
#include <riwo/core/system/library.h>
#include <riwo/utils/process.h>
#include <nlohmann/json.hpp>
#include <map>

namespace libempp
{

class LIBEMPP_CORE_API plugin_manager
{
	RIWO_DISABLE_COPY_MOVE(plugin_manager)

public:
	using path_t = std::filesystem::path;
	using json_t = nlohmann::json;

	using library_t = std::shared_ptr<riwo::library>;
	using libraries_t = std::map<std::string,library_t>;

	using process_t = std::shared_ptr<riwo::utils::process>;
	using processes_t = std::map<std::string,process_t>;

	struct library_node
	{
		std::string name;
		path_t file_name;
	};
	struct process_node : library_node
	{
		std::vector<riwo::value> args {};
		std::map<std::string,riwo::value> envs {};
	};

public:
	static void set_config_file(const path_t &file_name);
	static void set_config(json_t json);

	static void parse(std::string_view group = "");
	static void load(std::string_view group = "");

	[[nodiscard]] static std::vector<library_node> library_nodes() noexcept;
	[[nodiscard]] static std::vector<process_node> process_nodes() noexcept;
	[[nodiscard]] static json_t config() noexcept;

public:
	[[nodiscard]] static riwo::optional<library_t> library(std::string_view name) noexcept;
	[[nodiscard]] static libraries_t libraries() noexcept;

	/* Environment variables :
		LIBEMPP_PLUGIN_GROUP
		LIBEMPP_PLUGIN_NAME
	*/
	[[nodiscard]] static riwo::optional<process_t> process(std::string_view name) noexcept;
	[[nodiscard]] static processes_t processes() noexcept;

public:
	template <typename Func>
	static constexpr bool is_predicate_v = riwo::concepts::callable_ret <
		Func, bool, std::string_view, library_t
	>;
	template <typename Func>
	[[nodiscard]] static riwo::optional<library_t> library(Func &&predicate)
		noexcept requires is_predicate_v<Func>;
};

} //namespace libempp
#include <libempp/core/detail/plugin_manager.h>


#endif //LIBEMPP_CORE_PLUGIN_MANAGER_H
