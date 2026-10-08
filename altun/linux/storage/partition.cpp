// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "partition.h"
#ifdef __linux__

#include "detail/common.h"
#include "detail/information.h"
#include "block_device.h"
#include "information.h"
#include "mount.h"

#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <thread>

namespace altun::storage { namespace
{

[[nodiscard]] bool valid_partition_type(std::string_view value)
{
	return value.empty() or std::ranges::all_of(value, [](unsigned char character)
	{
		return (character >= 'a' and character <= 'z') or
			(character >= 'A' and character <= 'Z') or
			(character >= '0' and character <= '9') or
			character == '-' or character == '{' or character == '}';
	});
}

[[nodiscard]] bool valid_partition_name(std::string_view value)
{
	return std::ranges::none_of(value, [](char character)
	{
		return character == '\0' or character == '\r' or character == '\n' or
			character == '"' or character == '\\';
	});
}

[[nodiscard]] std::string table_name(partition_table_type type)
{
	switch(type)
	{
		case partition_table_type::dos: return "dos";
		case partition_table_type::gpt: return "gpt";
	}
	detail::throw_error(std::make_error_code(std::errc::invalid_argument),
		"altun::storage::partition table"
	);
}

[[nodiscard]] riwo::optional<device_id> device_identity(const path_t &device)
{
	struct stat status {};
	if( ::stat(device.c_str(), &status) < 0 )
		detail::throw_errno("altun::storage::partition identity");

	if( S_ISREG(status.st_mode) )
		return riwo::nullopt;

	if( not S_ISBLK(status.st_mode) )
	{
		detail::throw_error(std::error_code(ENOTBLK, std::system_category()),
			"altun::storage::partition identity"
		);
	}
	return device_id {
		.major = ::major(status.st_rdev),
		.minor = ::minor(status.st_rdev)
	};
}

void ensure_identity(const path_t &device, const riwo::optional<device_id> &before)
{
	if( const auto after = device_identity(device); after != before )
	{
		detail::throw_error(make_error_code(errc::device_changed),
			"altun::storage::partition identity"
		);
	}
}

void ensure_expected_identity
(const riwo::optional<device_id> &actual, const riwo::optional<device_id> &expected)
{
	if( actual and not expected )
	{
		detail::throw_error(make_error_code(errc::device_identity_required),
			"altun::storage::partition identity"
		);
	}
	if( expected and actual != expected )
	{
		detail::throw_error(make_error_code(errc::device_changed),
			"altun::storage::partition identity"
		);
	}
}

void ensure_unmounted(const path_t &device, const riwo::optional<device_id> &identity)
{
	if( not identity )
		return ;

	auto mounted = find_mounts_by_device(device);
	if( not mounted )
		detail::throw_error(mounted.error(), "altun::storage::partition mount check");

	if( not mounted->empty() )
	{
		detail::throw_error(std::make_error_code(std::errc::device_or_resource_busy),
			"altun::storage::partition mount check"
		);
	}
}

void validate_spec(const partition_spec &spec, partition_table_type table)
{
	if( not valid_partition_type(spec.type) or not valid_partition_name(spec.name) or
		(not spec.name.empty() and table != partition_table_type::gpt) )
	{
		detail::throw_error(std::make_error_code(std::errc::invalid_argument),
			"altun::storage::partition specification"
		);
	}
}

void validate_options(const partition_options &options)
{
	if( options.timeout <= std::chrono::milliseconds::zero() or
		options.poll_interval <= std::chrono::milliseconds::zero() )
	{
		detail::throw_error(std::make_error_code(std::errc::invalid_argument),
			"altun::storage::partition options"
		);
	}
}

[[nodiscard]] std::string partition_line(const partition_spec &spec)
{
	std::string input;
	if( spec.start_sector != 0 )
		input += "start=" + std::to_string(spec.start_sector) + ", ";

	input += spec.size_sectors == 0 ?
		"size=+" : "size=" + std::to_string(spec.size_sectors);

	if( not spec.type.empty() )
		input += ", type=" + spec.type;

	if( not spec.name.empty() )
		input += ", name=\"" + spec.name + '"';

	input += '\n';
	return input;
}

[[nodiscard]] bool partition_node_ready(const partition_info &partition, uint32_t sector_size)
{
	if( not partition.device )
		return true;

	auto expected_size = partition.size_bytes(sector_size);
	if( not expected_size )
		return false;

	auto opened = block_device::open(*partition.device);
	if( not opened )
		return false;

	auto information = (*opened)->info();
	return information and information->geometry.capacity_bytes == *expected_size;
}

[[nodiscard]] bool matches_specs(const disk_info &disk, const std::vector<partition_spec> *specifications)
{
	if( not specifications )
		return true;

	if( disk.partitions.size() != specifications->size() )
		return false;

	for(std::size_t index = 0; index < specifications->size(); ++index)
	{
		const auto &actual = disk.partitions[index];

		if( const auto &expected = (*specifications)[index];
			(expected.start_sector != 0 and actual.start_sector != expected.start_sector) or
			(expected.size_sectors != 0 and actual.size_sectors != expected.size_sectors) or
			(not expected.name.empty() and actual.name != expected.name) )
			return false;
	}
	return true;
}

[[nodiscard]] disk_info wait_for_layout(const path_t &device, std::string_view expected_table,
	std::size_t expected_count, const partition_options &options,
	const std::vector<partition_spec> *specifications = nullptr)
{
	const auto deadline = std::chrono::steady_clock::now() + options.timeout;
	for(;;)
	{
		if( auto inspected = inspect_disk(device);
			inspected and inspected->table and inspected->table->type == expected_table and
			inspected->partitions.size() == expected_count and matches_specs(*inspected, specifications) )
		{
			bool nodes_ready = true;
			for(const auto &partition : inspected->partitions)
			{
				if( not partition_node_ready(partition, inspected->sector_size) )
				{
					nodes_ready = false;
					break;
				}
			}
			if( nodes_ready )
				return std::move(*inspected);
		}
		if( std::chrono::steady_clock::now() >= deadline )
		{
			detail::throw_error(make_error_code(errc::verification_failed),
				"altun::storage::partition settle"
			);
		}
		std::this_thread::sleep_for(options.poll_interval);
	}
}

[[nodiscard]] std::vector<uint32_t> partition_numbers(const disk_info &disk)
{
	std::vector<uint32_t> result;
	result.reserve(disk.partitions.size());

	for(const auto &partition : disk.partitions)
		result.push_back(partition.number);
	return result;
}

} // namespace

namespace detail
{

[[nodiscard]] static result_t<disk_info> replace_partition_table(const path_t &device,
	partition_table_type type, const std::vector<partition_spec> &partitions,
	const partition_options &options, const riwo::optional<device_id> &expected)
{
	return detail::capture_expected<disk_info>([&]
	{
		ensure_path(device, "altun::storage::replace_partition_table");
		validate_options(options);

		for(const auto &partition : partitions)
			validate_spec(partition, type);

		const auto identity = device_identity(device);
		ensure_expected_identity(identity, expected);
		ensure_unmounted(device, identity);

		std::string input = "label: " + table_name(type) + '\n';
		for(const auto &partition : partitions)
			input += partition_line(partition);

		run_command({"sfdisk", "--quiet", "--lock", "--wipe", "always",
			"--wipe-partitions", "always", "--", device.native()}, input, options.timeout
		);
		ensure_identity(device, identity);

		return wait_for_layout (
			device, table_name(type), partitions.size(), options, &partitions
		);
	});
}

[[nodiscard]] static result_t<partition_info> create_partition(const path_t &device,
	const partition_spec &spec, const partition_options &options, const riwo::optional<device_id> &expected)
{
	return detail::capture_expected<partition_info>([&]
	{
		ensure_path(device, "altun::storage::create_partition");
		validate_options(options);

		const auto before = inspect_disk(device);
		require_partition_table(before, "altun::storage::create_partition");

		const auto table = before.table->type == "gpt" ?
			partition_table_type::gpt : partition_table_type::dos;
		validate_spec(spec, table);

		const auto identity = device_identity(device);
		ensure_expected_identity(identity, expected);
		ensure_unmounted(device, identity);

		const auto old_numbers = partition_numbers(before);
		run_command (
			{
				"sfdisk", "--quiet", "--lock", "--append",
				"--wipe-partitions", "never", "--",
				device.native()
			},
			partition_line(spec), options.timeout
		);
		ensure_identity(device, identity);
		const auto after = wait_for_layout(device, before.table->type,
			before.partitions.size() + 1, options);

		const auto created = std::ranges::find_if(after.partitions,
			[&old_numbers](const partition_info &partition) {
				return std::ranges::find(old_numbers, partition.number) == old_numbers.end();
			});
		if( created == after.partitions.end() )
		{
			throw_error(make_error_code(errc::verification_failed),
				"altun::storage::create_partition"
			);
		}
		if( (spec.start_sector != 0 and created->start_sector != spec.start_sector) or
			(spec.size_sectors != 0 and created->size_sectors != spec.size_sectors) or
			(not spec.name.empty() and created->name != spec.name) )
		{
			throw_error(make_error_code(errc::verification_failed),
				"altun::storage::create_partition"
			);
		}
		return *created;
	});
}

[[nodiscard]] static result_t<> delete_partition(const path_t &device, uint32_t number,
	const partition_options &options, const riwo::optional<device_id> &expected)
{
	return capture_expected([&]
	{
		validate_options(options);
		if( number == 0 )
		{
			throw_error(std::make_error_code(std::errc::invalid_argument),
				"altun::storage::delete_partition"
			);
		}
		const auto before = inspect_disk(device);
		require_partition_table(before, "altun::storage::delete_partition");
		static_cast<void>(find_partition(before, number));

		const auto identity = device_identity(device);
		ensure_expected_identity(identity, expected);
		ensure_unmounted(device, identity);

		run_command({"sfdisk", "--quiet", "--lock", "--delete", "--",
			device.native(), std::to_string(number)}, {}, options.timeout
		);
		ensure_identity(device, identity);
		const auto after = wait_for_layout(device, before.table->type,
			before.partitions.size() - 1, options
		);
		if( std::ranges::find(after.partitions, number, &partition_info::number) != after.partitions.end() )
		{
			throw_error(make_error_code(errc::verification_failed),
				"altun::storage::delete_partition");
		}
	});
}

[[nodiscard]] static result_t<partition_info> expand_partition(const path_t &device, uint32_t number,
	sector_t new_size_sectors, const partition_options &options, const riwo::optional<device_id> &expected)
{
	return detail::capture_expected<partition_info>([&]
	{
		validate_options(options);
		if( number == 0 )
		{
			throw_error(std::make_error_code(std::errc::invalid_argument),
				"altun::storage::expand_partition"
			);
		}
		const auto before = inspect_disk(device);
		require_partition_table(before, "altun::storage::expand_partition");

		const auto current = find_partition(before, number);
		if( new_size_sectors != 0 and new_size_sectors <= current.size_sectors )
		{
			throw_error(std::make_error_code(std::errc::invalid_argument),
				"altun::storage::expand_partition"
			);
		}
		const auto identity = device_identity(device);
		ensure_expected_identity(identity, expected);
		ensure_unmounted(device, identity);

		const std::string input = new_size_sectors == 0 ?
			"size=+\n" : "size=" + std::to_string(new_size_sectors) + "\n";

		run_command({"sfdisk", "--quiet", "--lock", "-N",
			std::to_string(number), "--", device.native()}, input, options.timeout
		);
		ensure_identity(device, identity);

		const auto deadline = std::chrono::steady_clock::now() + options.timeout;
		for(;;)
		{
			if( auto inspected = inspect_partition(device, number);
				inspected and inspected->start_sector == current.start_sector and
				inspected->size_sectors > current.size_sectors and
				partition_node_ready(*inspected, before.sector_size) )
				return std::move(*inspected);

			if( std::chrono::steady_clock::now() >= deadline )
			{
				throw_error(make_error_code(errc::verification_failed),
					"altun::storage::expand_partition"
				);
			}
			std::this_thread::sleep_for(options.poll_interval);
		}
	});
}

} //namespace detail

result_t<disk_info> replace_partition_table(const path_t &device, partition_table_type type,
	const std::vector<partition_spec> &partitions, const partition_options &options)
{
	return detail::replace_partition_table (
		device, type, partitions, options, riwo::nullopt
	);
}

result_t<disk_info> replace_partition_table(const device_info &device, partition_table_type type,
	const std::vector<partition_spec> &partitions, const partition_options &options)
{
	return detail::capture_expected<disk_info>([&]
	{
		detail::ensure_device(device, "altun::storage::replace_partition_table");
		auto replaced = detail::replace_partition_table (
			device.device, type, partitions, options, device.id
		);
		if( not replaced )
		{
			detail::throw_error(replaced.error(),
				"altun::storage::replace_partition_table"
			);
		}
		detail::ensure_device(device, "altun::storage::replace_partition_table");
		return std::move(*replaced);
	});
}

result_t<partition_info> create_partition
(const path_t &device, const partition_spec &spec, const partition_options &options)
{
	return detail::create_partition(device, spec, options, riwo::nullopt);
}

result_t<partition_info> create_partition
(const device_info &device, const partition_spec &spec, const partition_options &options)
{
	return detail::capture_expected<partition_info>([&]
	{
		detail::ensure_device(device, "altun::storage::create_partition");
		auto created = detail::create_partition(device.device, spec, options, device.id);

		if( not created )
			detail::throw_error(created.error(), "altun::storage::create_partition");

		detail::ensure_device(device, "altun::storage::create_partition");
		return std::move(*created);
	});
}

result_t<> delete_partition(const path_t &device, uint32_t number, const partition_options &options)
{
	return detail::delete_partition(device, number, options, riwo::nullopt);
}

result_t<> delete_partition(const device_info &device, uint32_t number, const partition_options &options)
{
	return detail::capture_expected([&]
	{
		detail::ensure_device(device, "altun::storage::delete_partition");

		if( auto deleted = detail::delete_partition(device.device, number, options, device.id); not deleted )
			detail::throw_error(deleted.error(), "altun::storage::delete_partition");

		detail::ensure_device(device, "altun::storage::delete_partition");
	});
}

result_t<partition_info> expand_partition
(const path_t &device, uint32_t number, sector_t new_size_sectors, const partition_options &options)
{
	return detail::expand_partition(device, number, new_size_sectors, options, riwo::nullopt);
}

result_t<partition_info> expand_partition
(const device_info &device, uint32_t number, sector_t new_size_sectors, const partition_options &options)
{
	return detail::capture_expected<partition_info>([&]
	{
		detail::ensure_device(device, "altun::storage::expand_partition");

		auto expanded = detail::expand_partition (
			device.device, number, new_size_sectors, options, device.id
		);
		if( not expanded )
			detail::throw_error(expanded.error(), "altun::storage::expand_partition");

		detail::ensure_device(device, "altun::storage::expand_partition");
		return std::move(*expanded);
	});
}

} // namespace altun::storage

#endif //__linux__
