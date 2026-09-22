// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "../../test.h"

#include <libempp/linux/storage.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>

#include <sys/mount.h>
#include <unistd.h>

namespace
{

constexpr std::string_view destructive_token = "YES_I_UNDERSTAND";

bool executable_exists(const char *path) noexcept
{
	return ::access(path, X_OK) == 0;
}

class mount_guard
{
public:
	explicit mount_guard(std::filesystem::path target) :
		m_target(std::move(target)) {}

	~mount_guard()
	{
		if( m_mounted )
			static_cast<void>(libempp::storage::unmount_target(m_target));
	}

	void mounted() noexcept {
		m_mounted = true;
	}

	void released() noexcept {
		m_mounted = false;
	}

private:
	std::filesystem::path m_target;
	bool m_mounted = false;
};

} // namespace

EMPP_TEST("destructive-storage", "partition format mount write and capacity cycle")
{
	const char *opt_in = std::getenv("LIBEMPP_TEST_STORAGE_DESTRUCTIVE");
	const char *device_text = std::getenv("LIBEMPP_TEST_STORAGE_DEVICE");
	if( not opt_in or std::string_view(opt_in) != destructive_token or
		not device_text or *device_text == '\0' )
		return;

	namespace storage = libempp::storage;

	const auto device = std::filesystem::canonical(device_text);
	const auto devices = storage::enumerate_devices();
	EMPP_REQUIRE(devices);
	const auto selected = std::ranges::find(*devices, device, &storage::device_info::device);
	EMPP_REQUIRE(selected != devices->end());
	EMPP_REQUIRE(not selected->partition);
	EMPP_REQUIRE(selected->removable);
	EMPP_REQUIRE(not selected->read_only);

	if( const char *serial = std::getenv("LIBEMPP_TEST_STORAGE_SERIAL");
		serial and *serial )
		EMPP_REQUIRE_EQ(selected->serial, std::string(serial));

	auto opened = storage::block_device::open(*selected);
	EMPP_REQUIRE(opened);
	const auto device_info = (*opened)->info();
	EMPP_REQUIRE(device_info);
	EMPP_REQUIRE(device_info->geometry.capacity_bytes >= 1024ULL * 1024ULL * 1024ULL);
	EMPP_REQUIRE(not device_info->geometry.read_only);
	opened->reset();

	const auto unmounted = storage::unmount_device(*selected);
	EMPP_REQUIRE(unmounted);

	storage::partition_spec first;
	first.start_sector = 2048;
	first.size_sectors = 131072;
	first.type = "83";
	auto disk = storage::replace_partition_table(*selected,
		storage::partition_table_type::dos, {first});
	EMPP_REQUIRE(disk);
	EMPP_REQUIRE(disk->table);
	EMPP_REQUIRE_EQ(disk->table->type, std::string("dos"));
	EMPP_REQUIRE_EQ(disk->partitions.size(), 1U);
	EMPP_REQUIRE(disk->partitions.front().device);
	const auto partition = *disk->partitions.front().device;
	const auto partition_device = storage::resolve_device(partition);
	EMPP_REQUIRE(partition_device);
	if( not selected->serial.empty() )
		EMPP_REQUIRE_EQ(partition_device->serial, selected->serial);

	storage::format_options format;
	format.type = storage::filesystem_type::ext4;
	format.label = "LIBEMPP_TEST";
	const auto ext4 = storage::format(*partition_device, format);
	if( not ext4 )
		empp_test::fail(std::format("ext4 format failed: {}:{} ({})",
			ext4.error().category().name(), ext4.error().value(),
			ext4.error().message()));
	EMPP_REQUIRE_EQ(ext4->type, std::string("ext4"));
	EMPP_REQUIRE_EQ(ext4->label, format.label);

	empp_test::temporary_directory mount_directory;
	mount_guard mounted(mount_directory.path());
	storage::mount_options mount_options;
	mount_options.flags = MS_NODEV | MS_NOSUID | MS_SYNCHRONOUS;
	const auto mount = storage::mount(*partition_device, mount_directory.path(),
		mount_options);
	EMPP_REQUIRE(mount);
	mounted.mounted();

	const auto capacity = storage::space(mount_directory.path());
	EMPP_REQUIRE(capacity);
	EMPP_REQUIRE(capacity->total > 0);
	EMPP_REQUIRE(capacity->available <= capacity->free);

	const auto marker = mount_directory.path() / "libempp-storage-test.txt";
	{
		std::ofstream output(marker);
		EMPP_REQUIRE(output.is_open());
		output << "libempp destructive storage integration\n";
		EMPP_REQUIRE(output.good());
	}
	{
		std::ifstream input(marker);
		std::string value;
		std::getline(input, value);
		EMPP_REQUIRE_EQ(value, std::string("libempp destructive storage integration"));
	}
	EMPP_REQUIRE(storage::unmount_target(mount_directory.path()));
	mounted.released();

	storage::partition_spec second;
	second.size_sectors = 131072;
	second.type = "7";
	const auto created = storage::create_partition(*selected, second);
	EMPP_REQUIRE(created);
	EMPP_REQUIRE_EQ(created->number, 2U);
	EMPP_REQUIRE(storage::delete_partition(*selected, created->number));

	const auto expanded = storage::expand_partition(*selected, 1);
	EMPP_REQUIRE(expanded);
	EMPP_REQUIRE(expanded->size_sectors > first.size_sectors);

	if( executable_exists("/usr/sbin/mkfs.ntfs") or
		executable_exists("/sbin/mkfs.ntfs") )
	{
		format.type = storage::filesystem_type::ntfs;
		const auto ntfs = storage::format(*partition_device, format);
		if( not ntfs )
			empp_test::fail(std::format("ntfs format failed: {}:{} ({})",
				ntfs.error().category().name(), ntfs.error().value(),
				ntfs.error().message()));
		EMPP_REQUIRE_EQ(ntfs->type, std::string("ntfs"));
	}

	format.type = storage::filesystem_type::ext4;
	format.timeout = std::chrono::seconds(30);
	const auto final_format = storage::format(*partition_device, format);
	if( not final_format )
		empp_test::fail(std::format("final ext4 format failed: {}:{} ({})",
			final_format.error().category().name(), final_format.error().value(),
			final_format.error().message()));
	EMPP_REQUIRE_EQ(final_format->type, std::string("ext4"));

	if( not executable_exists("/usr/sbin/mkfs.exfat") and
		not executable_exists("/sbin/mkfs.exfat") )
	{
		format.type = storage::filesystem_type::exfat;
		const auto missing_exfat = storage::format(*partition_device, format);
		EMPP_REQUIRE(not missing_exfat);
		EMPP_REQUIRE_EQ(missing_exfat.error(),
			std::make_error_code(std::errc::no_such_file_or_directory));
	}
}
