// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <altun/linux/udev/event.h>
#include <iostream>

using block_events = altun::udev::event<altun::subsys_enum::block>;

int main(int argc, const char *argv[])
{
	block_events events;
	std::error_code error;

	events.received.connect([](const altun::udev::device_event &event)
	{
		std::cout << altun::udev::string(event.action)
			<< "  " << (event.dev_node.empty() ? event.sys_path : event.dev_node)
			<< '\n';
	});
	events.error.connect([](const std::error_code &event_error)
	{
		std::cerr << "Block event failed: " << event_error.message() << '\n';
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
