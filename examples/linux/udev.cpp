// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <libempp/linux/udev/enumeration.h>
#include <iostream>

using tty_device = libempp::udev::enumeration<libempp::subsys_enum::tty>;

std::string property_or(const tty_device &device, std::string_view key)
{
	auto value = device.property(key);
	return value ? value->to_string() : "-";
}

int main(int argc, const char *argv[])
{
	const std::string_view model_pattern = argc > 1 ? argv[1] : "*";

	auto devices = tty_device::list (
		libempp::udev::prop_key::id_model, model_pattern
	);
	std::cout << "Matched " << devices.size() << " tty device(s)\n";

	for(const auto &device : devices)
	{
		std::cout
			<< property_or(device, libempp::udev::prop_key::dev_name)
			<< "  model=" << property_or(device, libempp::udev::prop_key::id_model)
			<< "  serial=" << property_or(device, libempp::udev::prop_key::id_serial)
			<< '\n';
	}
	return 0;
}
