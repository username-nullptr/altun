// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <altun/linux/bus/spi.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

unsigned long parse_number(const char *text, unsigned long max)
{
	size_t end = 0;
	const auto value = std::stoul(text, &end, 0);

	if( text[end] != '\0' or value > max )
		throw std::out_of_range(text);
	return value;
}

int main(int argc, const char *argv[])
{
	if( argc < 4 )
	{
		std::cerr << "Usage: spi <device> <speed-hz> <byte> [byte ...]\n";
		return 1;
	}
	try {
		const auto speed = static_cast<uint32_t>(parse_number (
			argv[2], std::numeric_limits<uint32_t>::max()
		));
		std::vector<uint8_t> transmitted;
		for(int index = 3; index < argc; ++index)
		{
			transmitted.push_back(static_cast<uint8_t>(
				parse_number(argv[index], std::numeric_limits<uint8_t>::max())
			));
		}
		std::vector<uint8_t> received(transmitted.size());
		std::error_code error;
		altun::bus::spi device;

		device.open({argv[1], speed}, error);
		if( error )
		{
			std::cerr << "Open failed: " << error.message() << '\n';
			return 1;
		}
		const auto transferred = device.transfer (
			riwo::const_buffer(transmitted.data(), transmitted.size()),
			riwo::mutable_buffer(received.data(), received.size()), error
		);
		if( error )
		{
			std::cerr << "Transfer failed: " << error.message() << '\n';
			return 1;
		}
		std::cout << "Received " << transferred << " byte(s):";
		for(const auto byte : received)
		{
			std::cout << " 0x" << std::hex << std::setw(2) << std::setfill('0')
				<< static_cast<unsigned int>(byte);
		}
		std::cout << '\n';
	}
	catch(const std::exception &exception)
	{
		std::cerr << "Invalid argument: " << exception.what() << '\n';
		return 1;
	}
	return 0;
}
