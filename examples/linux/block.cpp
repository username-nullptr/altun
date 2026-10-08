// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <libempp/linux/storage/block_device.h>
#include <algorithm>
#include <iostream>
#include <cstddef>
#include <vector>

int main(int argc, const char *argv[])
{
	if( argc != 2 )
	{
		std::cerr << "Usage: block <device>\n";
		return 1;
	}
	auto opened = libempp::storage::block_device::open(argv[1]);
	if( not opened )
	{
		std::cerr << "Block device failed: " << opened.error().message() << '\n';
		return 1;
	}
	auto &device = **opened;
	auto information = device.info();
	if( not information )
	{
		std::cerr << "Block device failed: " << information.error().message() << '\n';
		return 1;
	}
	const auto &geometry = information->geometry;
	std::cout
		<< "Device: " << information->device << '\n'
		<< "Device id: " << information->id.major << ':' << information->id.minor << '\n'
		<< "Capacity: " << geometry.capacity_bytes << " bytes\n"
		<< "Logical block size: " << geometry.logical_block_size << " bytes\n"
		<< "Physical block size: " << geometry.physical_block_size << " bytes\n"
		<< "Logical block count: "
		<< geometry.capacity_bytes / geometry.logical_block_size << '\n'
		<< "Kernel read-only: "
		<< (geometry.read_only ? "yes" : "no") << '\n';

	const auto first_block_size = static_cast<size_t>(std::min (
		geometry.capacity_bytes, static_cast<libempp::storage::block_device::capacity_t>(
			geometry.logical_block_size
		)
	));
	std::vector<std::byte> first_block(first_block_size);

	const auto read_size = device.read_some_at (
		0, riwo::mutable_buffer(first_block.data(), first_block.size())
	);
	if( not read_size )
	{
		std::cerr << "Block read failed: " << read_size.error().message() << '\n';
		return 1;
	}
	std::cout << "Read from offset 0: " << *read_size << " bytes\n";
	return 0;
}
