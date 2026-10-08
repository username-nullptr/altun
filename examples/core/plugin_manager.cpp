// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <altun/core/plugin_manager.h>
#include <riwo/core/system/app_utls.h>

#include <filesystem>
#include <iostream>

#ifndef ALTUN_EXAMPLE_PLUGIN_NAME
# define ALTUN_EXAMPLE_PLUGIN_NAME "plugin.so"
#endif

std::filesystem::path default_plugin_file()
{
	auto directory = riwo::app::dir_path();
	return directory ? *directory / ALTUN_EXAMPLE_PLUGIN_NAME :
		std::filesystem::path(ALTUN_EXAMPLE_PLUGIN_NAME);
}

int main(int argc, const char *argv[])
{
	const std::filesystem::path plugin_file = argc > 1 ?
		argv[1] : default_plugin_file();

	const auto plugin_dir = plugin_file.has_parent_path() ?
		plugin_file.parent_path() : std::filesystem::path(".");

	altun::plugin_manager::set_config({
		{
			{"path", plugin_dir.string()},
			{"libs", {{"math", plugin_file.filename().string()}}}
		}
	});
	altun::plugin_manager::load();

	auto plugin = altun::plugin_manager::library("math");
	if(not plugin)
	{
		std::cerr << "Plugin was not loaded: " << plugin_file << '\n';
		return 1;
	}
	auto square = (*plugin)->interface<int(int)>("altun_example_square");
	if(not square)
	{
		std::cerr << "Plugin symbol was not found\n";
		return 1;
	}

	std::cout << "square(12) = " << (*square)(12) << '\n';
	return 0;
}
