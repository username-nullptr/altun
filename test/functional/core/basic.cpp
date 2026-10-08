// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "../../test.h"

#include <altun/core/global.h>
#include <altun/core/plugin_manager.h>
#include <altun/core/settings.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{

using altun_test::temporary_directory;

void write_text(const std::filesystem::path &path, std::string_view content)
{
	std::ofstream stream(path);
	ALTUN_REQUIRE(stream.is_open());
	stream << content;
	ALTUN_REQUIRE(stream.good());
}

} // namespace

ALTUN_TEST("core", "version string matches the build version")
{
	const std::string_view version = altun::version_string();
	ALTUN_REQUIRE(not version.empty());
	ALTUN_REQUIRE_EQ(version, std::string_view(ALTUN_VERSION_STR));
}

ALTUN_TEST("core", "settings load, change, and persist an INI file")
{
	temporary_directory directory;
	const auto file = directory.path() / "settings.ini";
	write_text(file, "[server]\nport=9000\n");

	const auto suffix = std::to_string(altun_test::current_run().iteration);
	const auto settings_name = std::string("altun-test-settings-") + suffix;
	auto &settings = altun_settings(settings_name);
	struct signal_cleanup
	{
		riwo::utils::settings &settings;
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

	ALTUN_REQUIRE(settings.load(file));
	ALTUN_REQUIRE_EQ(loads, 1U);
	ALTUN_REQUIRE_EQ(settings.file_name(), file);
	ALTUN_REQUIRE_EQ(settings.name(), std::string_view(settings_name));
	ALTUN_REQUIRE(not settings.get("server/missing"));

	const auto original_port = settings.get("server/port");
	ALTUN_REQUIRE(original_port);
	ALTUN_REQUIRE_EQ(original_port->to_uint().value_or(0), 9000U);

	settings.set("server/port", 9100).set("server/host", "127.0.0.1");
	ALTUN_REQUIRE_EQ(changes, 2U);
	ALTUN_REQUIRE_EQ(changed_key, std::string("server/host"));
	for(std::uint32_t index = 0; index < 32; ++index)
		settings.set("runtime/counter", index);
	ALTUN_REQUIRE_EQ(changes, 34U);
	ALTUN_REQUIRE_EQ(changed_key, std::string("runtime/counter"));
	for(std::size_t iteration = 0; iteration < 8; ++iteration)
		ALTUN_REQUIRE(settings.sync());
	ALTUN_REQUIRE(settings.load(file));
	ALTUN_REQUIRE_EQ(loads, 2U);
	const auto counter = settings.get("runtime/counter");
	ALTUN_REQUIRE(counter);
	ALTUN_REQUIRE_EQ(counter->to_uint().value_or(0), 31U);

	std::ifstream persisted(file);
	const std::string content {
		std::istreambuf_iterator<char>(persisted),
		std::istreambuf_iterator<char>()
	};
	ALTUN_REQUIRE(content.find("port=9100") != std::string::npos);
	ALTUN_REQUIRE(content.find("host=\"127.0.0.1\"") != std::string::npos);
	ALTUN_REQUIRE(content.find("counter=31") != std::string::npos);
	ALTUN_REQUIRE(&settings == &altun_settings(settings_name));
	const auto missing_name = std::string("altun-test-settings-missing-") + suffix;
	ALTUN_REQUIRE_THROWS(std::runtime_error,
		static_cast<void>(riwo::utils::settings::instance(
			missing_name, false
		)));
	const auto conflict_name = std::string("altun-test-settings-conflict-") + suffix;
	auto &conflict = altun_settings(conflict_name);
	const auto conflict_result = conflict.load(file);
	ALTUN_REQUIRE(not conflict_result);
	ALTUN_REQUIRE_EQ(conflict_result.error(),
		std::make_error_code(std::errc::device_or_resource_busy));

	const auto names = riwo::utils::settings::names();
	ALTUN_REQUIRE(std::ranges::find(names, settings_name) != names.end());
}

ALTUN_TEST("core", "plugin manager parses object and array configuration forms")
{
	using manager = altun::plugin_manager;
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
	ALTUN_REQUIRE_EQ(processes.size(), 2U);
	ALTUN_REQUIRE_EQ(libraries.size(), 2U);

	const auto collector = std::ranges::find_if(processes, [](const auto &node) {
		return node.name == "collector";
	});
	ALTUN_REQUIRE(collector != processes.end());
	ALTUN_REQUIRE_EQ(collector->file_name, std::filesystem::path("./virtual/collector.bin"));
	ALTUN_REQUIRE_EQ(collector->args.size(), 2U);
	ALTUN_REQUIRE_EQ(collector->args[0].to_string(), std::string("--interval"));
	ALTUN_REQUIRE_EQ(collector->envs.at("LOG_LEVEL").to_string(), std::string("debug"));
	ALTUN_REQUIRE_EQ(collector->envs.at("ALTUN_PLUGIN_GROUP").to_string(), std::string());
	ALTUN_REQUIRE_EQ(collector->envs.at("ALTUN_PLUGIN_NAME").to_string(), std::string("collector"));

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
	ALTUN_REQUIRE_EQ(manager::config(), grouped_config);
	for(std::size_t iteration = 0; iteration < 32; ++iteration)
	{
		manager::parse("virtual-group");
		const auto grouped_processes = manager::process_nodes();
		const auto grouped_libraries = manager::library_nodes();
		ALTUN_REQUIRE_EQ(grouped_processes.size(), 1U);
		ALTUN_REQUIRE_EQ(grouped_libraries.size(), 1U);
		ALTUN_REQUIRE_EQ(grouped_processes[0].name, std::string("worker"));
		ALTUN_REQUIRE_EQ(grouped_processes[0].file_name,
			std::filesystem::path("./group-devices/worker.bin"));
		ALTUN_REQUIRE_EQ(grouped_processes[0].envs.at("ALTUN_PLUGIN_GROUP").to_string(),
			std::string("virtual-group"));
		ALTUN_REQUIRE_EQ(grouped_libraries[0].name, std::string("sensor"));
		ALTUN_REQUIRE_EQ(grouped_libraries[0].file_name,
			std::filesystem::path("./group-devices/sensor.so"));
	}
}

ALTUN_TEST("core", "plugin manager rejects malformed and duplicate descriptors deterministically")
{
	using manager = altun::plugin_manager;
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
		ALTUN_REQUIRE(manager::library_nodes().empty());
		ALTUN_REQUIRE(manager::process_nodes().empty());
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
	ALTUN_REQUIRE_EQ(manager::process_nodes().size(), 1U);
	ALTUN_REQUIRE_EQ(manager::library_nodes().size(), 1U);
	ALTUN_REQUIRE_EQ(manager::process_nodes()[0].name, std::string("worker"));
	ALTUN_REQUIRE_EQ(manager::library_nodes()[0].name, std::string("codec"));
}

ALTUN_TEST("core", "plugin config file resolves relative paths from its directory")
{
	temporary_directory directory;
	const auto config_file = directory.path() / "plugins.json";
	write_text(config_file, R"json([
  {
    "path": "plugins",
    "libs": {"virtual": "virtual.so"}
  }
])json");

	altun::plugin_manager::set_config_file(config_file);
	altun::plugin_manager::parse();
	const auto nodes = altun::plugin_manager::library_nodes();

	ALTUN_REQUIRE_EQ(nodes.size(), 1U);
	ALTUN_REQUIRE_EQ(nodes[0].name, std::string("virtual"));
	ALTUN_REQUIRE_EQ(nodes[0].file_name, directory.path() / "plugins/virtual.so");
}

ALTUN_TEST("core", "plugin manager loads managed processes and a virtual shared library")
{
	using manager = altun::plugin_manager;
	const std::filesystem::path plugin_file = ALTUN_TEST_PLUGIN_FILE;
	const std::filesystem::path app_file = ALTUN_TEST_APP_FILE;
	ALTUN_REQUIRE(std::filesystem::is_regular_file(plugin_file));
	ALTUN_REQUIRE(std::filesystem::is_regular_file(app_file));
	if(altun_test::current_run().iteration > 1)
	{
		const auto library = manager::library("virtual");
		ALTUN_REQUIRE(library);
		const auto function = (*library)->interface<int(int)>("altun_test_double");
		ALTUN_REQUIRE(function);
		ALTUN_REQUIRE_EQ((*function)(42), 84);
		ALTUN_REQUIRE(manager::process("managed"));
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

	ALTUN_REQUIRE_EQ(manager::libraries().size(), 1U);
	ALTUN_REQUIRE_EQ(manager::processes().size(), 1U);
	const auto process = manager::process("managed");
	ALTUN_REQUIRE(process);
	ALTUN_REQUIRE((*process)->joinable());
	ALTUN_REQUIRE_EQ((*process)->state(), riwo::utils::process_state::running);
	ALTUN_REQUIRE(not manager::process("missing-app"));
	const auto library = manager::library("virtual");
	ALTUN_REQUIRE(library);
	const auto function = (*library)->interface<int(int)>("altun_test_double");
	ALTUN_REQUIRE(function);
	for(int value = -1024; value <= 1024; ++value)
		ALTUN_REQUIRE_EQ((*function)(value), value * 2);
	ALTUN_REQUIRE(not (*library)->interface<void()>("altun_test_missing_symbol"));

	const auto predicate_match = manager::library([](std::string_view name, const auto&) {
		return name == "virtual";
	});
	ALTUN_REQUIRE(predicate_match);
	ALTUN_REQUIRE(not manager::library([](std::string_view, const auto&) {
		return false;
	}));
	ALTUN_REQUIRE(not manager::library("missing"));
	ALTUN_REQUIRE_THROWS(std::runtime_error, manager::parse());
	ALTUN_REQUIRE_THROWS(std::runtime_error, manager::load());
	manager::set_config(nullptr);
	ALTUN_REQUIRE_EQ(manager::config(), configured);
	ALTUN_REQUIRE(manager::library("virtual"));
}
