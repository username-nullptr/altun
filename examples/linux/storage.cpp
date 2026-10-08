// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <libempp/linux/storage.h>
#include <cerrno>
#include <iostream>
#include <system_error>

namespace storage = libempp::storage;

void print_filesystem(const riwo::optional<storage::filesystem_info> &info,
	std::string_view indent)
{
	if( not info )
	{
		std::cout << indent << "Filesystem: none detected\n";
		return ;
	}
	std::cout << indent << "Filesystem: " << info->type << '\n';
	if( not info->secondary_type.empty() )
		std::cout << indent << "Secondary type: " << info->secondary_type << '\n';

	if( not info->label.empty() )
		std::cout << indent << "Label: " << info->label << '\n';

	if( not info->uuid.empty() )
		std::cout << indent << "UUID: " << info->uuid << '\n';

	if( not info->usage.empty() )
		std::cout << indent << "Usage: " << info->usage << '\n';

	if( not info->version.empty() )
		std::cout << indent << "Version: " << info->version << '\n';

	std::cout
		<< indent << "Size: " << info->size_bytes << " bytes\n"
		<< indent << "Block size: " << info->block_size << " bytes\n";
}

int main(int argc, const char *argv[])
{
	if( argc != 2 )
	{
		std::cerr << "Usage: storage <device-or-image>\n";
		return 1;
	}
	const storage::path_t device = argv[1];
	riwo::optional<storage::device_info> selected;
	const auto resolved = storage::resolve_device(device);
	if( resolved )
		selected = *resolved;
	else if( resolved.error() != std::error_code(ENOTBLK, std::system_category()) )
	{
		std::cerr << "Device resolution failed: " << resolved.error().message() << '\n';
		return 1;
	}

	const auto filesystem = selected ? storage::inspect_filesystem(*selected) :
		storage::inspect_filesystem(device);
	const auto disk = selected ? storage::inspect_disk(*selected) :
		storage::inspect_disk(device);
	if( not filesystem or not disk )
	{
		const auto error = not filesystem ? filesystem.error() : disk.error();
		std::cerr << "Filesystem inspection failed: " << error.message() << '\n';
		return 1;
	}
	std::size_t mounted_count = 0;
	if( selected )
	{
		const auto mounted = storage::find_mounts_by_device(*selected);
		if( not mounted )
		{
			std::cerr << "Mount inspection failed: " << mounted.error().message() << '\n';
			return 1;
		}
		mounted_count = mounted->size();
	}

	std::cout
		<< "Device: " << device << '\n'
		<< "Mounted entries: " << mounted_count << '\n';

	print_filesystem(*filesystem, "");

	std::cout
		<< "Disk size: " << disk->size_bytes << " bytes\n"
		<< "Sector size: " << disk->sector_size << " bytes\n"
		<< "Partition table: "
		<< (disk->table ? disk->table->type : "none") << '\n';

	for(const auto &partition : disk->partitions)
	{
		std::cout
			<< "\nPartition " << partition.number << ": "
			<< partition.device.value_or(storage::path_t{}) << '\n'
			<< "  Sectors: " << partition.start_sector << '-'
			<< partition.end_sector().value_or() << '\n'
			<< "  Size: " << partition.size_bytes(disk->sector_size).value_or()
			<< " bytes\n";

		if( not partition.type.empty() )
			std::cout << "  Type: " << partition.type << '\n';

		if( not partition.name.empty() )
			std::cout << "  Name: " << partition.name << '\n';

		if( not partition.uuid.empty() )
			std::cout << "  Partition UUID: " << partition.uuid << '\n';

		print_filesystem(partition.filesystem, "  ");
	}
	return 0;
}
