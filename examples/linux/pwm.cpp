// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <libempp/linux/subsys/pwm.h>
#include <iostream>
#include <limits>
#include <thread>

unsigned long parse_decimal(const char *text, unsigned long max)
{
	size_t end = 0;
	const auto value = std::stoul(text, &end);
	if(text[end] != '\0' or value > max)
		throw std::out_of_range(text);
	return value;
}

int main(int argc, const char *argv[])
{
	if( argc != 5 )
	{
		std::cerr << "Usage: pwm <chip> <channel> <period-ns> <duty-ns>\n";
		return 1;
	}
	try {
		const auto channel = parse_decimal (
			argv[2], std::numeric_limits<uint8_t>::max()
		);
		const auto period = parse_decimal (
			argv[3], std::numeric_limits<uint32_t>::max()
		);
		const auto duty = parse_decimal (
			argv[4], std::numeric_limits<uint32_t>::max()
		);
		if( duty > period )
			riwo::out_of_range::loc_throw("PWM value");

		libempp::subsys::pwm output ({
			argv[1], static_cast<uint8_t>(channel)
		});
		output.set(static_cast<uint32_t>(period), static_cast<uint32_t>(duty))
			.enable();

		std::cout << "PWM enabled for one second\n";
		std::this_thread::sleep_for(std::chrono::seconds(1));
		output.disable();
	}
	catch(const std::exception &exception)
	{
		std::cerr << "PWM failed: " << exception.what() << '\n';
		return 1;
	}
	return 0;
}
