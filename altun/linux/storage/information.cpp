// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "information.h"
#ifdef __linux__

#include "detail/information.h"
#include "detail/common.h"

#include <blkid/blkid.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <unistd.h>
#include <fcntl.h>

namespace altun::storage { namespace
{

class RIWO_DECL_HIDDEN file_descriptor
{
	RIWO_DISABLE_COPY_MOVE(file_descriptor)

public:
	explicit file_descriptor(int descriptor = -1) noexcept :
		m_descriptor(descriptor) {}

	~file_descriptor()
	{
		if( m_descriptor >= 0 )
			::close(m_descriptor);
	}

	[[nodiscard]] int get() const noexcept {
		return m_descriptor;
	}

private:
	int m_descriptor;
};

using probe_ptr = std::unique_ptr <
	std::remove_pointer_t<blkid_probe>, decltype(&blkid_free_probe)
>;

// These symbolic return values were added to the public libblkid headers after
// the return-value ABI of blkid_do_safeprobe() had already been established.
// Keep the names local so older sysroots (for example, util-linux 2.37) remain
// usable without defining identifiers in libblkid's namespace.
constexpr int probe_ok = 0;
constexpr int probe_error = -1;
constexpr int probe_ambiguous = -2;

[[nodiscard]] std::string probe_value(blkid_probe probe, const char *name)
{
	const char *data = nullptr;
	size_t size = 0;

	if( ::blkid_probe_lookup_value(probe, name, &data, &size) != 0 or not data )
		return {};

	while( size > 0 and data[size - 1] == '\0' )
		--size;
	return {data, size};
}

[[nodiscard]] std::uint64_t parse_uint64(std::string_view value)
{
	std::uint64_t result = 0;
	const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
	return ec == std::errc{} and ptr == value.data() + value.size() ?
		result : 0;
}

[[nodiscard]] riwo::optional<filesystem_info> probe_filesystem
(int descriptor, const path_t &device, std::uint64_t offset, std::uint64_t size, bool report_ambiguous)
{
	probe_ptr probe(::blkid_new_probe(), &blkid_free_probe);
	if( not probe )
		detail::throw_errno("altun::storage::inspect_filesystem");

	int superblock_flags =
		BLKID_SUBLKS_LABEL | BLKID_SUBLKS_UUID | BLKID_SUBLKS_TYPE |
		BLKID_SUBLKS_SECTYPE | BLKID_SUBLKS_USAGE | BLKID_SUBLKS_VERSION;

#ifdef BLKID_SUBLKS_FSINFO
	superblock_flags |= BLKID_SUBLKS_FSINFO;
#endif

	if( offset > static_cast<std::uint64_t>(std::numeric_limits<blkid_loff_t>::max()) or
		size > static_cast<std::uint64_t>(std::numeric_limits<blkid_loff_t>::max()) or
		::blkid_probe_set_device(probe.get(), descriptor,
			static_cast<blkid_loff_t>(offset), static_cast<blkid_loff_t>(size)) != 0 or
		::blkid_probe_enable_partitions(probe.get(), 0) != 0 or
		::blkid_probe_enable_superblocks(probe.get(), 1) != 0 or
		::blkid_probe_set_superblocks_flags(probe.get(), superblock_flags) != 0 )
		detail::throw_errno("altun::storage::inspect_filesystem");

	const int result = ::blkid_do_safeprobe(probe.get());
	if( result == probe_error )
		detail::throw_errno("altun::storage::inspect_filesystem");

	if( result == probe_ambiguous and report_ambiguous )
	{
		detail::throw_error(make_error_code(errc::ambiguous_signature),
			"altun::storage::inspect_filesystem"
		);
	}
	if( result != probe_ok )
		return riwo::nullopt;

	filesystem_info info;
	info.device = device;

	info.type = probe_value(probe.get(), "TYPE");
	info.secondary_type = probe_value(probe.get(), "SEC_TYPE");

	info.label = probe_value(probe.get(), "LABEL");
	info.uuid = probe_value(probe.get(), "UUID");

	info.usage = probe_value(probe.get(), "USAGE");
	info.version = probe_value(probe.get(), "VERSION");

	info.size_bytes = parse_uint64(probe_value(probe.get(), "FSSIZE"));
	info.block_size = parse_uint64(probe_value(probe.get(), "FSBLOCKSIZE"));
	return info;
}

[[nodiscard]] path_t partition_device_path(const path_t &disk, std::uint32_t number)
{
	std::string path = disk.native();
	if( not path.empty() and path.back() >= '0' and path.back() <= '9' )
		path += 'p';

	path += std::to_string(number);
	return path;
}

struct RIWO_DECL_HIDDEN raw_partition
{
	partition_info info;
	std::uint64_t offset_bytes = 0;
	std::uint64_t size_bytes = 0;
};

[[nodiscard]] riwo::optional<filesystem_info> probe_partition_device(const raw_partition &partition)
{
	if( not partition.info.device )
		return riwo::nullopt;

	// A partition node has its own block-cache mapping. Probing it directly
	// avoids stale parent-device pages after the partition was reformatted.
	file_descriptor descriptor(::open (
		partition.info.device->c_str(), O_RDONLY | O_CLOEXEC
	));
	if( descriptor.get() < 0 )
		return riwo::nullopt;

	struct stat status {};
	std::uint64_t size = 0;

	if( ::fstat(descriptor.get(), &status) < 0 or not S_ISBLK(status.st_mode) or
		::ioctl(descriptor.get(), BLKGETSIZE64, &size) < 0 or
		size != partition.size_bytes )
		return riwo::nullopt;

	return probe_filesystem (
		descriptor.get(), *partition.info.device, 0, 0, false
	);
}

} // namespace

namespace detail
{

disk_info inspect_disk(const path_t &device)
{
	constexpr std::string_view operation = "altun::storage::inspect_disk";
	ensure_path(device, operation);

	file_descriptor descriptor(::open(device.c_str(), O_RDONLY | O_CLOEXEC));
	if (descriptor.get() < 0 )
		throw_errno(operation);

	struct stat device_status {};
	if( ::fstat(descriptor.get(), &device_status) < 0 )
		throw_errno(operation);

	const bool is_block_device = S_ISBLK(device_status.st_mode);
	path_t resolved_device = device;

	if( is_block_device )
		resolved_device = std::filesystem::canonical(device);

	probe_ptr probe(::blkid_new_probe(), &blkid_free_probe);
	if( not probe )
		throw_errno(operation);

	if( ::blkid_probe_set_device(probe.get(), descriptor.get(), 0, 0) != 0 or
		::blkid_probe_enable_superblocks(probe.get(), 0) != 0 or
		::blkid_probe_enable_partitions(probe.get(), 1) != 0 or
		::blkid_probe_set_partitions_flags(probe.get(), BLKID_PARTS_ENTRY_DETAILS) != 0 )
		throw_errno(operation);

	if( const int probe_result = ::blkid_do_fullprobe(probe.get()); probe_result < 0 )
		throw_errno(operation);

	disk_info result;
	result.device = resolved_device;

	if( const auto device_size = ::blkid_probe_get_size(probe.get()); device_size > 0 )
		result.size_bytes = static_cast<std::uint64_t>(device_size);

	result.sector_size = ::blkid_probe_get_sectorsize(probe.get());
	if( result.sector_size == 0 )
		result.sector_size = 512;

	const auto list = ::blkid_probe_get_partitions(probe.get());
	if( not list )
		return result;

	if( const auto table = ::blkid_partlist_get_table(list) )
	{
		partition_table_info table_info;
		if( const char *type = ::blkid_parttable_get_type(table) )
			table_info.type = type;

		if( const char *id = ::blkid_parttable_get_id(table) )
			table_info.id = id;

		if( not table_info.type.empty() )
			result.table = std::move(table_info);
	}
	const int partition_count = ::blkid_partlist_numof_partitions(list);
	std::vector<raw_partition> raw_partitions;

	if( partition_count > 0 )
		raw_partitions.reserve(static_cast<std::size_t>(partition_count));

	for(int index = 0; index < partition_count; ++index)
	{
		const auto partition = ::blkid_partlist_get_partition(list, index);
		if( not partition )
			continue;

		const auto part_number = ::blkid_partition_get_partno(partition);
		const auto start = ::blkid_partition_get_start(partition);
		const auto size = ::blkid_partition_get_size(partition);

		if( part_number <= 0 or start < 0 or size < 0 )
			continue;

		raw_partition raw;
		raw.info.number = static_cast<std::uint32_t>(part_number);

		if( is_block_device )
			raw.info.device = partition_device_path(resolved_device, raw.info.number);

		raw.info.start_sector = static_cast<sector_t>(start);
		raw.info.size_sectors = static_cast<sector_t>(size);

		if( const char *type = ::blkid_partition_get_type_string(partition) )
			raw.info.type = type;

		if( const char *uuid = ::blkid_partition_get_uuid(partition) )
			raw.info.uuid = uuid;

		if( const char *name = ::blkid_partition_get_name(partition) )
			raw.info.name = name;

		raw.info.flags = ::blkid_partition_get_flags(partition);
		raw.info.primary = ::blkid_partition_is_primary(partition) == 1;
		raw.info.logical = ::blkid_partition_is_logical(partition) == 1;
		raw.info.extended = ::blkid_partition_is_extended(partition) == 1;

		if( raw.info.start_sector <= std::numeric_limits<std::uint64_t>::max() / result.sector_size and
			raw.info.size_sectors <= std::numeric_limits<std::uint64_t>::max() / result.sector_size )
		{
			raw.offset_bytes = raw.info.start_sector * result.sector_size;
			raw.size_bytes = raw.info.size_sectors * result.sector_size;
		}
		raw_partitions.push_back(std::move(raw));
	}
	for(auto &raw : raw_partitions)
	{
		if( raw.size_bytes != 0 )
		{
			riwo::optional<filesystem_info> direct;
			if( is_block_device )
				direct = probe_partition_device(raw);

			raw.info.filesystem = direct ? std::move(direct) :
				probe_filesystem(descriptor.get(), resolved_device, raw.offset_bytes, raw.size_bytes, false);
		}
		result.partitions.push_back(std::move(raw.info));
	}
	std::ranges::sort(result.partitions, {}, &partition_info::number);
	return result;
}

partition_info find_partition(const disk_info &disk, std::uint32_t number)
{
	const auto found = std::ranges::find (
		disk.partitions, number, &partition_info::number
	);
	if( found == disk.partitions.end() )
	{
		throw_error(make_error_code(errc::partition_not_found),
			"altun::storage::inspect_partition"
		);
	}
	return *found;
}

void require_partition_table(const disk_info &disk, std::string_view operation)
{
	if( not disk.table )
		throw_error(make_error_code(errc::no_partition_table), operation);
}

} // namespace detail

result_t<riwo::optional<filesystem_info>> inspect_filesystem(const path_t &device)
{
	return detail::capture_expected<riwo::optional<filesystem_info>>([&]
	{
		detail::ensure_path(device, "altun::storage::inspect_filesystem");
		file_descriptor descriptor(::open(device.c_str(), O_RDONLY | O_CLOEXEC));

		if( descriptor.get() < 0 )
			detail::throw_errno("altun::storage::inspect_filesystem");

		return probe_filesystem(descriptor.get(), device, 0, 0, true);
	});
}

result_t<riwo::optional<filesystem_info>> inspect_filesystem(const device_info &device)
{
	return detail::capture_expected<riwo::optional<filesystem_info>>([&]
	{
		constexpr std::string_view operation =
			"altun::storage::inspect_filesystem";

		detail::ensure_device(device, operation);
		auto inspected = inspect_filesystem(device.device);

		if( not inspected )
			detail::throw_error(inspected.error(), operation);

		detail::ensure_device(device, operation);
		return std::move(*inspected);
	});
}

result_t<disk_info> inspect_disk(const path_t &device)
{
	return detail::capture_expected<disk_info>([&] {
		return detail::inspect_disk(device);
	});
}

result_t<disk_info> inspect_disk(const device_info &device)
{
	return detail::capture_expected<disk_info>([&]
	{
		constexpr std::string_view operation = "altun::storage::inspect_disk";
		detail::ensure_device(device, operation);

		auto inspected = inspect_disk(device.device);
		if( not inspected )
			detail::throw_error(inspected.error(), operation);

		detail::ensure_device(device, operation);
		return std::move(*inspected);
	});
}

result_t<partition_info> inspect_partition(const path_t &device, std::uint32_t number)
{
	return detail::capture_expected<partition_info>([&]
	{
		if( number == 0 )
		{
			detail::throw_error(std::make_error_code(std::errc::invalid_argument),
				"altun::storage::inspect_partition"
			);
		}
		const auto disk = detail::inspect_disk(device);

		detail::require_partition_table(disk,
			"altun::storage::inspect_partition"
		);
		return detail::find_partition(disk, number);
	});
}

result_t<partition_info> inspect_partition(const device_info &device, std::uint32_t number)
{
	return detail::capture_expected<partition_info>([&]
	{
		constexpr std::string_view operation =
			"altun::storage::inspect_partition";

		detail::ensure_device(device, operation);
		auto inspected = inspect_partition(device.device, number);

		if( not inspected )
			detail::throw_error(inspected.error(), operation);

		detail::ensure_device(device, operation);
		return std::move(*inspected);
	});
}

} // namespace altun::storage

#endif //__linux__
