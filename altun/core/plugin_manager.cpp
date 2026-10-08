// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "plugin_manager.h"
#include "log.h"

#include <riwo/core/system/app_utls.h>
#include <set>

namespace altun
{

namespace fs = std::filesystem;
using json_t = nlohmann::json;

namespace
{

json_t g_config {};
std::string g_file_name {};

std::atomic_bool g_initialized {false};
std::atomic_bool g_dirty {true};

std::vector<plugin_manager::library_node> g_library_nodes {};
std::vector<plugin_manager::process_node> g_process_nodes {};

class RIWO_DECL_HIDDEN process_registry final
{
	RIWO_DISABLE_COPY_MOVE(process_registry)

public:
	process_registry() = default;
	~process_registry() noexcept
	{
		// Process-wide teardown can run after the operating system has already
		// stopped monitor threads (notably during Windows DLL detach). Never wait
		// for a monitor from a static destructor; signal the child and release join
		// ownership so normal running-time cleanup remains asynchronous.
		for(auto &[name, process] : processes)
		{
			RIWO_UNUSED(name);
			if( process and process->joinable() )
				process->cancel(riwo::utils::process::cancel_option::kill);
		}
	}
	plugin_manager::processes_t processes {};
};

} //namespace

void plugin_manager::set_config_file(const path_t &file_name)
{
	if( g_initialized )
	{
		altun_clog_warning("Altun.Core",
			"plugin_manager: Set config file '{}' failed: Already initialized.",
			file_name
		);
		return ;
	}
	riwo::app::absolute_path(file_name)
	.and_then([](const path_t &config_file) -> riwo::sys_expected<path_t>
	{
		std::ifstream file(config_file);
		if( not file.is_open() )
			return {std::make_error_code(static_cast<std::errc>(errno))};

		std::string content {
			std::istreambuf_iterator(file),
			std::istreambuf_iterator<char>()
		};
		if( content.empty() )
			return config_file;

		g_file_name = config_file.string();
		try {
			g_config = json_t::parse(content);
			g_dirty = true;
		}
		catch(...)
		{
			altun_clog_error("Altun.Core",
				"plugin_manager: Set config file '{}' failed: Json format invalid.",
				config_file
			);
		}
		return config_file;
	})
	.or_else([&](const riwo::error_code &error)
	{
		altun_clog_error("Altun.Core",
			"plugin_manager: Set config file '{}' failed: {}.",
			file_name, error
		);
		return riwo::sys_expected<path_t>{""};
	});
}

void plugin_manager::set_config(json_t json)
{
	if( g_initialized )
	{
		altun_clog_warning("Altun.Core",
			"plugin_manager: Set config failed: Already initialized."
		);
		return ;
	}
	g_file_name = {};
	g_config = std::move(json);
	g_dirty = true;
}

static plugin_manager::processes_t &managed_processes() noexcept
{
	static process_registry registry;
	return registry.processes;
}

static plugin_manager::libraries_t g_libraries;

class RIWO_DECL_HIDDEN config_parser
{
	RIWO_DISABLE_COPY_MOVE(config_parser)

public:
	struct app_desc
	{
		std::string file_name {};
		std::vector<fs::path> args {};
		std::map<std::string,fs::path> envs {};
	};
	explicit config_parser(std::string_view group) :
		m_group(group) {}

	std::map<std::string, std::vector<app_desc>> applications {};
	std::map<std::string, std::vector<std::string>> plugins {};

public:
	void parse() noexcept
	{
		auto root_path = riwo::strtls::file_path(g_file_name);
		try {
			// Remove Duplicates.
			for(auto config = m_group.empty() ? g_config : g_config[m_group];
				auto &pack_obj : config)
			{
				auto path = pack_obj["path"].get<std::string>() + "/";
				if( not riwo::app::is_absolute_path(path) )
					path.insert(0, root_path);

				if( auto apps_obj = pack_obj["apps"]; apps_obj.is_object() )
				{
					for(auto &app_obj : apps_obj.items())
						parse_app_obj(app_obj.value(), app_obj.key(), path);
				}
				else if( apps_obj.is_array() )
				{
					for(auto &app_obj : apps_obj)
					{
						auto name = app_obj["name"].get<std::string>();
						parse_app_obj(app_obj, name, path);
					}
				}
				if( auto app_obj = pack_obj["app"]; app_obj.is_object() )
				{
					auto name = app_obj["name"].get<std::string>();
					parse_app_obj(app_obj, name, path);
				}
				m_names.clear();
				m_file_names.clear();

				if( auto libs_obj = pack_obj["libs"]; libs_obj.is_object() )
				{
					for(auto &lib_obj : libs_obj.items())
						parse_lib_obj(lib_obj.value(), lib_obj.key(), path);
				}
				else if( libs_obj.is_array() )
				{
					for(auto &lib_obj : libs_obj)
					{
						auto name = lib_obj["name"].get<std::string>();
						parse_lib_obj(lib_obj, name, path);
					}
				}
			}
		}
		catch(const std::exception &ex)
		{
			if( g_file_name.empty() )
			{
				altun_clog_error("Altun.Core",
					"plugin_manager: Load plugins config(json) failed: {}.", ex
				);
				return ;
			}
			altun_clog_error("Altun.Core",
				"plugin_manager: Load plugins config(json) '{}' failed: {}.",
				g_file_name, ex
			);
		}
	}

private:
	void parse_app_obj(json_t &app_obj, std::string name, const std::string &path)
	{
		if( name.empty() )
		{
			static size_t seq = 0;
			name = std::format("application_{}", seq++);
		}
		if( app_obj.is_string() )
		{
			auto file_name = path + app_obj.get<std::string>();
			if( auto desc = emplace(name, file_name) )
			{
				desc->envs.emplace("ALTUN_PLUGIN_GROUP", m_group);
				desc->envs.emplace("ALTUN_PLUGIN_NAME", std::move(name));
			}
			return ;
		}
		auto file_name = path + app_obj["file_name"].get<std::string>();
		auto desc = emplace(name, file_name);
		if( not desc )
			return ;

		for(auto &arg_obj : app_obj["args"])
			desc->args.emplace_back(arg_obj.get<std::string>());

		desc->envs.emplace("ALTUN_PLUGIN_GROUP", m_group);
		desc->envs.emplace("ALTUN_PLUGIN_NAME", std::move(name));

		if( auto envs_obj = app_obj["envs"]; envs_obj.is_object() )
		{
			for(auto &env_obj : envs_obj.items())
				parse_env_obj(env_obj.value(), env_obj.key(), *desc);
		}
		else if( envs_obj.is_array() )
		{
			for(auto &env_obj : envs_obj)
			{
				auto key = env_obj["value"].get<std::string>();
				parse_env_obj(env_obj, key, *desc);
			}
		}
	}

	void parse_lib_obj(json_t &lib_obj, std::string name, const std::string &path)
	{
		if( name.empty() )
		{
			static size_t seq = 0;
			name = std::format("library_{}", seq++);
		}
		std::string file_name {};
		if( lib_obj.is_string() )
			file_name = path + lib_obj.get<std::string>();
		else
			file_name = path + lib_obj["file_name"].get<std::string>();

		if( not check(name, file_name) )
			return ;

		plugins[std::move(name)]
			.emplace_back(std::move(file_name));
	}

	static void parse_env_obj(json_t &env_obj, std::string key, app_desc &desc)
	{
		std::string value;
		if( env_obj.is_string() )
			value = env_obj.get<std::string>();
		else
			value = env_obj["value"].get<std::string>();
		desc.envs.emplace(key, value);
	}

private:
	[[nodiscard]] app_desc *emplace(std::string name, std::string file_name) noexcept
	{
		if( not check(name, file_name) )
			return nullptr;
		auto &desc = applications[std::move(name)]
			.emplace_back(std::move(file_name));
		return &desc;
	}

	[[nodiscard]] bool check(std::string name, std::string file_name) noexcept
	{
		{
			if( auto [it, inserted] = m_names.emplace(name); not inserted )
			{
				altun_clog_warning("Altun.Core",
					"plugin_manager: Application '{}' already exists.", name
				);
				return false;
			}
		}{
			if( auto [it, inserted] = m_file_names.emplace(file_name); not inserted )
			{
				altun_clog_warning("Altun.Core",
					"plugin_manager: Application file '{}' already exists.", file_name
				);
				return false;
			}
		}
		return true;
	}

private:
	std::string_view m_group {};
	std::set<std::string> m_file_names {};
	std::set<std::string> m_names {};
};

void plugin_manager::parse(std::string_view group)
{
	if( g_initialized )
		riwo::runtime_error::loc_throw("Already initialized.");

	g_process_nodes.clear();
	g_library_nodes.clear();

	config_parser parser(group);
	parser.parse();

	for(auto &[name, desc_list] : parser.applications)
	{
		for(auto &[file_name, args, envs] : desc_list)
		{
			g_process_nodes.emplace_back(process_node {
				{ .name = name, .file_name = std::move(file_name) },
				std::move(args), std::move(envs)
			});
		}
	}
	for(auto &[name, file_names] : parser.plugins)
	{
		for(auto &file_name : file_names)
		{
			g_library_nodes.emplace_back (
				name, std::move(file_name)
			);
		}
	}
	g_dirty = false;
}

void plugin_manager::load(std::string_view group)
{
	if( g_initialized )
		riwo::runtime_error::loc_throw("Already initialized.");

	if( g_dirty )
		parse(group);

	g_initialized = true;
	for(auto &node : g_process_nodes)
	{
		if( not fs::exists(node.file_name) )
		{
			altun_clog_warning("Altun.Core",
				"plugin_manager: Load application '{}' failed: {}.",
				node.file_name, riwo::make_system_error_code(std::errc::no_such_file_or_directory)
			);
			continue;
		}
		auto [it, inserted] = managed_processes().emplace (
			node.name, std::make_shared<riwo::utils::process> (
				node.file_name, node.args
			)
		);
		assert(inserted);

		for(auto &[key, value] : node.envs)
			it->second->setenv(key, std::move(value));

		it->second->start()
		.transform([&]
		{
			altun_clog_info("Altun.Core",
				"plugin_manager: Load application: '{}'.",
				node.file_name
			);
		}).
		or_else([&](const riwo::error_code &error)
		{
			altun_clog_warning("Altun.Core",
				"plugin_manager: Load application '{}' failed: {}.",
				node.file_name, error
			);
		});
	}
	for(auto &[name, file_name] : g_library_nodes)
	{
		if( not fs::exists(file_name) )
		{
			altun_clog_warning("Altun.Core",
				"plugin_manager: Load plugin '{}' failed: {}.",
				file_name, riwo::make_system_error_code(std::errc::no_such_file_or_directory)
			);
			continue;
		}
		auto [it, inserted] = g_libraries.emplace (
			name, std::make_shared<riwo::library>(file_name)
		);
		assert(inserted);

		it->second->load()
		.transform([&]
		{
			altun_clog_info("Altun.Core",
				"plugin_manager: Load plugin: '{}'.", file_name
			);
		}).
		or_else([&](const riwo::error_code &error)
		{
			altun_clog_warning("Altun.Core",
				"plugin_manager: Load plugin '{}' failed: {}.",
				file_name, error
			);
		});
	}
}

std::vector<plugin_manager::library_node> plugin_manager::library_nodes() noexcept
{
	return g_library_nodes;
}

std::vector<plugin_manager::process_node> plugin_manager::process_nodes() noexcept
{
	return g_process_nodes;
}

plugin_manager::json_t plugin_manager::config() noexcept
{
	return g_config;
}

riwo::optional<plugin_manager::library_t> plugin_manager::library(std::string_view name) noexcept
{
	auto it = g_libraries.find(std::string(name));
	return it == g_libraries.end() ?
		riwo::optional<library_t>() : riwo::make_optional(it->second);
}

plugin_manager::libraries_t plugin_manager::libraries() noexcept
{
	return g_libraries;
}

riwo::optional<plugin_manager::process_t> plugin_manager::process(std::string_view name) noexcept
{
	auto &processes = managed_processes();
	auto it = processes.find(std::string(name));
	return it == processes.end() ?
		riwo::optional<process_t>() : riwo::make_optional(it->second);
}

plugin_manager::processes_t plugin_manager::processes() noexcept
{
	return managed_processes();
}

} //namespace altun
