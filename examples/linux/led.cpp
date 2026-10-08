// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <altun/linux/subsys/led.h>

#include <iostream>
#include <limits>

unsigned long parse_decimal(const char *text, unsigned long max)
{
	size_t end = 0;
	const auto value = std::stoul(text, &end);
	if( text[end] != '\0' or value > max )
		throw std::out_of_range(text);
	return value;
}

int main(int argc, const char *argv[])
{
	if( argc != 3 )
	{
		std::cerr << "Usage: led <name> <brightness>\n";
		return 1;
	}

	try {
		const auto brightness = static_cast<uint32_t>(parse_decimal(
			argv[2], std::numeric_limits<uint32_t>::max()
		));
		altun::subsys::led output({argv[1]});
		if( brightness > output.max_brightness() )
		{
			std::cerr << "Brightness exceeds maximum "
				<< output.max_brightness() << '\n';
			return 1;
		}
		output.set_brightness(brightness);
		std::cout << "LED brightness: " << output.brightness() << '/'
			<< output.max_brightness() << '\n';
	}
	catch(const std::exception &exception)
	{
		std::cerr << "LED failed: " << exception.what() << '\n';
		return 1;
	}
	return 0;
}
