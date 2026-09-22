// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <libempp/core/plugin_manager.h>
#include <libgs/core/system/app_utls.h>

#include <filesystem>
#include <iostream>

#ifndef LIBEMPP_EXAMPLE_PLUGIN_NAME
# define LIBEMPP_EXAMPLE_PLUGIN_NAME "plugin.so"
#endif

std::filesystem::path default_plugin_file()
{
	auto directory = libgs::app::dir_path();
	return directory ? *directory / LIBEMPP_EXAMPLE_PLUGIN_NAME :
		std::filesystem::path(LIBEMPP_EXAMPLE_PLUGIN_NAME);
}

int main(int argc, const char *argv[])
{
	const std::filesystem::path plugin_file = argc > 1 ?
		argv[1] : default_plugin_file();

	const auto plugin_dir = plugin_file.has_parent_path() ?
		plugin_file.parent_path() : std::filesystem::path(".");

	libempp::plugin_manager::set_config({
		{
			{"path", plugin_dir.string()},
			{"libs", {{"math", plugin_file.filename().string()}}}
		}
	});
	libempp::plugin_manager::load();

	auto plugin = libempp::plugin_manager::library("math");
	if(not plugin)
	{
		std::cerr << "Plugin was not loaded: " << plugin_file << '\n';
		return 1;
	}
	auto square = (*plugin)->interface<int(int)>("libempp_example_square");
	if(not square)
	{
		std::cerr << "Plugin symbol was not found\n";
		return 1;
	}

	std::cout << "square(12) = " << (*square)(12) << '\n';
	return 0;
}
