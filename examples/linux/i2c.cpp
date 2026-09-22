// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <libempp/linux/bus/i2c.h>
#include <stdexcept>
#include <iostream>

unsigned long parse_number(const char *text, unsigned long max)
{
	size_t end = 0;
	const auto value = std::stoul(text, &end, 0);
	if(text[end] != '\0' or value > max)
		throw std::out_of_range(text);
	return value;
}

int main(int argc, const char *argv[])
{
	if( argc != 4 and argc != 5 )
	{
		std::cerr << "Usage: i2c <device> <address> <register> [value]\n";
		return 1;
	}
	try {
		const auto address = static_cast<uint8_t>(parse_number(argv[2], 0x7f));
		const auto reg = static_cast<uint8_t>(parse_number(argv[3], 0xff));

		std::error_code error;
		libempp::bus::i2c device;

		device.open({argv[1], address}, error);
		if(error)
		{
			std::cerr << "Open failed: " << error.message() << '\n';
			return 1;
		}
		uint8_t value = 0;
		size_t transferred = 0;

		if( argc == 5 )
		{
			value = static_cast<uint8_t>(parse_number(argv[4], 0xff));
			transferred = device.write (
				reg, libgs::const_buffer(&value, 1), error
			);
		}
		else
		{
			transferred = device.read (
				reg, libgs::mutable_buffer(&value, 1), error
			);
		}
		if(error)
		{
			std::cerr << "Transfer failed: " << error.message() << '\n';
			return 1;
		}
		std::cout << (argc == 5 ? "Wrote " : "Read ")
			<< transferred << " byte(s), value = 0x" << std::hex
			<< static_cast<unsigned int>(value) << '\n';
	}
	catch(const std::exception &exception)
	{
		std::cerr << "Invalid argument: " << exception.what() << '\n';
		return 1;
	}
	return 0;
}
