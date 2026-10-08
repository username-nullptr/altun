// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <altun/core/settings.h>
#include <filesystem>
#include <iostream>

int main(int argc, const char *argv[])
{
	const std::filesystem::path path = argc > 1 ?
		argv[1] : "altun-example.ini";

	auto &settings = altun_default_settings;
	settings.changed.connect([](std::string_view key, const riwo::value &value) {
		std::cout << "Changed " << key << " = " << value.to_string() << '\n';
	});

	if(auto result = settings.load(path); not result)
	{
		std::cerr << "Load failed: " << result.error().message() << '\n';
		return 1;
	}
	settings
		.set("server/host", "127.0.0.1")
		.set("server/port", 8080);

	if(auto result = settings.sync(); not result)
	{
		std::cerr << "Save failed: " << result.error().message() << '\n';
		return 1;
	}

	const auto port = settings.get("server/port");
	std::cout << "Saved " << settings.file_name()
		<< ", port = " << (port ? port->to_uint().value_or(0) : 0) << '\n';
	return 0;
}
