// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <libempp/linux/udev/event.h>
#include <iostream>

using block_events = libempp::udev::event<libempp::subsys_enum::block>;

int main(int argc, const char *argv[])
{
	block_events events;
	std::error_code error;

	events.received.connect([](const libempp::udev::device_event &event)
	{
		std::cout << libempp::udev::string(event.action)
			<< "  " << (event.dev_node.empty() ? event.sys_path : event.dev_node)
			<< '\n';
	});
	events.error.connect([](const std::error_code &error)
	{
		std::cerr << "Block event failed: " << error.message() << '\n';
		riwo::exit(1);
	});

	// DEVTYPE is optional. For block devices, common values are "disk" and "partition".
	if( argc > 1 )
		events.open(argv[1], error);
	else
		events.open(error);

	if( error )
	{
		std::cerr << "Failed to monitor block devices: " << error.message() << '\n';
		return 1;
	}
	return riwo::exec();
}
