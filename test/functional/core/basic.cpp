// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "../../test.h"

#include <libempp/core/global.h>
#include <libempp/core/plugin_manager.h>
#include <libempp/core/settings.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{

using empp_test::temporary_directory;

void write_text(const std::filesystem::path &path, std::string_view content)
{
	std::ofstream stream(path);
	EMPP_REQUIRE(stream.is_open());
	stream << content;
	EMPP_REQUIRE(stream.good());
}

} // namespace

EMPP_TEST("core", "version string matches the build version")
{
	const std::string_view version = libempp::version_string();
	EMPP_REQUIRE(not version.empty());
	EMPP_REQUIRE_EQ(version, std::string_view(LIBEMPP_VERSION_STR));
}

EMPP_TEST("core", "settings load, change, and persist an INI file")
{
	temporary_directory directory;
	const auto file = directory.path() / "settings.ini";
	write_text(file, "[server]\nport=9000\n");

	const auto suffix = std::to_string(empp_test::current_run().iteration);
	const auto settings_name = std::string("empp-test-settings-") + suffix;
	auto &settings = libempp_settings(settings_name);
	struct signal_cleanup
	{
		libgs::utils::settings &settings;
		~signal_cleanup()
		{
			settings.changed.disconnect();
			settings.loaded.disconnect();
			settings.synced.disconnect();
		}
	} cleanup {settings};
	std::size_t changes = 0;
	std::size_t loads = 0;
	std::string changed_key;
	settings.changed.connect([&](std::string_view key) {
		++changes;
		changed_key = key;
	});
	settings.loaded.connect([&] {
		++loads;
	});

	EMPP_REQUIRE(settings.load(file));
	EMPP_REQUIRE_EQ(loads, 1U);
	EMPP_REQUIRE_EQ(settings.file_name(), file);
	EMPP_REQUIRE_EQ(settings.name(), std::string_view(settings_name));
	EMPP_REQUIRE(not settings.get("server/missing"));

	const auto original_port = settings.get("server/port");
	EMPP_REQUIRE(original_port);
	EMPP_REQUIRE_EQ(original_port->to_uint().value_or(0), 9000U);

	settings.set("server/port", 9100).set("server/host", "127.0.0.1");
	EMPP_REQUIRE_EQ(changes, 2U);
	EMPP_REQUIRE_EQ(changed_key, std::string("server/host"));
	for(std::uint32_t index = 0; index < 32; ++index)
		settings.set("runtime/counter", index);
	EMPP_REQUIRE_EQ(changes, 34U);
	EMPP_REQUIRE_EQ(changed_key, std::string("runtime/counter"));
	for(std::size_t iteration = 0; iteration < 8; ++iteration)
		EMPP_REQUIRE(settings.sync());
	EMPP_REQUIRE(settings.load(file));
	EMPP_REQUIRE_EQ(loads, 2U);
	const auto counter = settings.get("runtime/counter");
	EMPP_REQUIRE(counter);
	EMPP_REQUIRE_EQ(counter->to_uint().value_or(0), 31U);

	std::ifstream persisted(file);
	const std::string content {
		std::istreambuf_iterator<char>(persisted),
		std::istreambuf_iterator<char>()
	};
	EMPP_REQUIRE(content.find("port=9100") != std::string::npos);
	EMPP_REQUIRE(content.find("host=\"127.0.0.1\"") != std::string::npos);
	EMPP_REQUIRE(content.find("counter=31") != std::string::npos);
	EMPP_REQUIRE(&settings == &libempp_settings(settings_name));
	const auto missing_name = std::string("empp-test-settings-missing-") + suffix;
	EMPP_REQUIRE_THROWS(std::runtime_error,
		static_cast<void>(libgs::utils::settings::instance(
			missing_name, false
		)));
	const auto conflict_name = std::string("empp-test-settings-conflict-") + suffix;
	auto &conflict = libempp_settings(conflict_name);
	const auto conflict_result = conflict.load(file);
	EMPP_REQUIRE(not conflict_result);
	EMPP_REQUIRE_EQ(conflict_result.error(),
		std::make_error_code(std::errc::device_or_resource_busy));

	const auto names = libgs::utils::settings::names();
	EMPP_REQUIRE(std::ranges::find(names, settings_name) != names.end());
}

EMPP_TEST("core", "plugin manager parses object and array configuration forms")
{
	using manager = libempp::plugin_manager;
	using json = manager::json_t;

	manager::set_config(json::array({
		{
			{"path", "virtual"},
			{"apps", {
				{"collector", {
					{"file_name", "collector.bin"},
					{"args", {"--interval", "1000"}},
					{"envs", {{"LOG_LEVEL", "debug"}}}
				}},
				{"compact", "compact.bin"}
			}},
			{"libs", {
				{"codec", "codec.so"},
				{"device", {{"file_name", "device.so"}}}
			}}
		}
	}));
	manager::parse();

	const auto processes = manager::process_nodes();
	const auto libraries = manager::library_nodes();
	EMPP_REQUIRE_EQ(processes.size(), 2U);
	EMPP_REQUIRE_EQ(libraries.size(), 2U);

	const auto collector = std::ranges::find_if(processes, [](const auto &node) {
		return node.name == "collector";
	});
	EMPP_REQUIRE(collector != processes.end());
	EMPP_REQUIRE_EQ(collector->file_name, std::filesystem::path("./virtual/collector.bin"));
	EMPP_REQUIRE_EQ(collector->args.size(), 2U);
	EMPP_REQUIRE_EQ(collector->args[0].to_string(), std::string("--interval"));
	EMPP_REQUIRE_EQ(collector->envs.at("LOG_LEVEL").to_string(), std::string("debug"));
	EMPP_REQUIRE_EQ(collector->envs.at("LIBEMPP_PLUGIN_GROUP").to_string(), std::string());
	EMPP_REQUIRE_EQ(collector->envs.at("LIBEMPP_PLUGIN_NAME").to_string(), std::string("collector"));

	const json grouped_config {
		{"virtual-group", json::array({
			{
				{"path", "group-devices"},
				{"apps", json::array({
					{{"name", "worker"}, {"file_name", "worker.bin"}}
				})},
				{"libs", json::array({
					{{"name", "sensor"}, {"file_name", "sensor.so"}}
				})}
			}
		})}
	};
	manager::set_config(grouped_config);
	EMPP_REQUIRE_EQ(manager::config(), grouped_config);
	for(std::size_t iteration = 0; iteration < 32; ++iteration)
	{
		manager::parse("virtual-group");
		const auto grouped_processes = manager::process_nodes();
		const auto grouped_libraries = manager::library_nodes();
		EMPP_REQUIRE_EQ(grouped_processes.size(), 1U);
		EMPP_REQUIRE_EQ(grouped_libraries.size(), 1U);
		EMPP_REQUIRE_EQ(grouped_processes[0].name, std::string("worker"));
		EMPP_REQUIRE_EQ(grouped_processes[0].file_name,
			std::filesystem::path("./group-devices/worker.bin"));
		EMPP_REQUIRE_EQ(grouped_processes[0].envs.at("LIBEMPP_PLUGIN_GROUP").to_string(),
			std::string("virtual-group"));
		EMPP_REQUIRE_EQ(grouped_libraries[0].name, std::string("sensor"));
		EMPP_REQUIRE_EQ(grouped_libraries[0].file_name,
			std::filesystem::path("./group-devices/sensor.so"));
	}
}

EMPP_TEST("core", "plugin manager rejects malformed and duplicate descriptors deterministically")
{
	using manager = libempp::plugin_manager;
	using json = manager::json_t;

	const std::array malformed_configs {
		json::object({{"unexpected", true}}),
		json::array({{{"path", 42}, {"libs", {{"bad", "bad.so"}}}}}),
		json::array({{{"path", "plugins"}, {"apps", "not-a-container"}}})
	};
	for(const auto &config : malformed_configs)
	{
		manager::set_config(config);
		manager::parse();
		EMPP_REQUIRE(manager::library_nodes().empty());
		EMPP_REQUIRE(manager::process_nodes().empty());
	}

	manager::set_config(json::array({
		{
			{"path", "duplicates"},
			{"apps", json::array({
				{{"name", "worker"}, {"file_name", "worker.bin"}},
				{{"name", "worker"}, {"file_name", "other.bin"}},
				{{"name", "other"}, {"file_name", "worker.bin"}}
			})},
			{"libs", json::array({
				{{"name", "codec"}, {"file_name", "codec.so"}},
				{{"name", "codec"}, {"file_name", "other.so"}},
				{{"name", "other"}, {"file_name", "codec.so"}}
			})}
		}
	}));
	manager::parse();
	EMPP_REQUIRE_EQ(manager::process_nodes().size(), 1U);
	EMPP_REQUIRE_EQ(manager::library_nodes().size(), 1U);
	EMPP_REQUIRE_EQ(manager::process_nodes()[0].name, std::string("worker"));
	EMPP_REQUIRE_EQ(manager::library_nodes()[0].name, std::string("codec"));
}

EMPP_TEST("core", "plugin config file resolves relative paths from its directory")
{
	temporary_directory directory;
	const auto config_file = directory.path() / "plugins.json";
	write_text(config_file, R"json([
  {
    "path": "plugins",
    "libs": {"virtual": "virtual.so"}
  }
])json");

	libempp::plugin_manager::set_config_file(config_file);
	libempp::plugin_manager::parse();
	const auto nodes = libempp::plugin_manager::library_nodes();

	EMPP_REQUIRE_EQ(nodes.size(), 1U);
	EMPP_REQUIRE_EQ(nodes[0].name, std::string("virtual"));
	EMPP_REQUIRE_EQ(nodes[0].file_name, directory.path() / "plugins/virtual.so");
}

EMPP_TEST("core", "plugin manager loads managed processes and a virtual shared library")
{
	using manager = libempp::plugin_manager;
	const std::filesystem::path plugin_file = LIBEMPP_TEST_PLUGIN_FILE;
	const std::filesystem::path app_file = LIBEMPP_TEST_APP_FILE;
	EMPP_REQUIRE(std::filesystem::is_regular_file(plugin_file));
	EMPP_REQUIRE(std::filesystem::is_regular_file(app_file));
	if(empp_test::current_run().iteration > 1)
	{
		const auto library = manager::library("virtual");
		EMPP_REQUIRE(library);
		const auto function = (*library)->interface<int(int)>("libempp_test_double");
		EMPP_REQUIRE(function);
		EMPP_REQUIRE_EQ((*function)(42), 84);
		EMPP_REQUIRE(manager::process("managed"));
		return;
	}

	manager::set_config({
		{
			{"path", plugin_file.parent_path().string()},
			{"apps", {
				{"managed", app_file.filename().string()},
				{"missing-app", "missing-app"}
			}},
			{"libs", {
				{"virtual", plugin_file.filename().string()},
				{"missing", "missing-plugin.so"}
			}}
		}
	});
	const auto configured = manager::config();
	manager::load();

	EMPP_REQUIRE_EQ(manager::libraries().size(), 1U);
	EMPP_REQUIRE_EQ(manager::processes().size(), 1U);
	const auto process = manager::process("managed");
	EMPP_REQUIRE(process);
	EMPP_REQUIRE((*process)->joinable());
	EMPP_REQUIRE_EQ((*process)->state(), libgs::utils::process_state::running);
	EMPP_REQUIRE(not manager::process("missing-app"));
	const auto library = manager::library("virtual");
	EMPP_REQUIRE(library);
	const auto function = (*library)->interface<int(int)>("libempp_test_double");
	EMPP_REQUIRE(function);
	for(int value = -1024; value <= 1024; ++value)
		EMPP_REQUIRE_EQ((*function)(value), value * 2);
	EMPP_REQUIRE(not (*library)->interface<void()>("libempp_test_missing_symbol"));

	const auto predicate_match = manager::library([](std::string_view name, const auto&) {
		return name == "virtual";
	});
	EMPP_REQUIRE(predicate_match);
	EMPP_REQUIRE(not manager::library([](std::string_view, const auto&) {
		return false;
	}));
	EMPP_REQUIRE(not manager::library("missing"));
	EMPP_REQUIRE_THROWS(std::runtime_error, manager::parse());
	EMPP_REQUIRE_THROWS(std::runtime_error, manager::load());
	manager::set_config(nullptr);
	EMPP_REQUIRE_EQ(manager::config(), configured);
	EMPP_REQUIRE(manager::library("virtual"));
}
