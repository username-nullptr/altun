// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "../../test.h"

#include <libempp/linux.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>

#include <fcntl.h>
#include <unistd.h>

namespace
{

class temporary_image
{
public:
	explicit temporary_image(std::uint64_t size)
	{
		std::array<char, 40> pattern {};
		const std::string value = "/tmp/libempp-filesystem-XXXXXX";
		std::ranges::copy(value, pattern.begin());
		const int descriptor = ::mkstemp(pattern.data());
		if(descriptor < 0)
			empp_test::fail("mkstemp failed");
		m_path = pattern.data();
		if(size > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()) or
			::ftruncate(descriptor, static_cast<off_t>(size)) < 0)
		{
			::close(descriptor);
			empp_test::fail("ftruncate failed");
		}
		::close(descriptor);
	}

	~temporary_image()
	{
		std::error_code ignored;
		std::filesystem::remove(m_path, ignored);
	}

	[[nodiscard]] const std::filesystem::path &path() const noexcept {
		return m_path;
	}

private:
	std::filesystem::path m_path;
};

bool executable_exists(const char *path)
{
	return ::access(path, X_OK) == 0;
}

void copy_image_at(const std::filesystem::path &source,
	const std::filesystem::path &target, std::uint64_t offset)
{
	std::ifstream input(source, std::ios::binary);
	std::fstream output(target, std::ios::in | std::ios::out | std::ios::binary);
	EMPP_REQUIRE(input.is_open());
	EMPP_REQUIRE(output.is_open());
	output.seekp(static_cast<std::streamoff>(offset));
	std::array<char, 64U * 1024U> buffer {};
	while(input)
	{
		input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		const auto count = input.gcount();
		if(count > 0)
			output.write(buffer.data(), count);
	}
	EMPP_REQUIRE(input.eof());
	EMPP_REQUIRE(output.good());
}

} // namespace

EMPP_TEST("virtual-device", "filesystem inspection validates paths and reads mounts")
{
		namespace fs = libempp::storage;
	const auto missing = fs::inspect_disk("/tmp/libempp-filesystem-device-that-does-not-exist");
	EMPP_REQUIRE(not missing);
	EMPP_REQUIRE_EQ(missing.error(), std::error_code(ENOENT, std::system_category()));

	const auto empty = fs::inspect_filesystem(fs::path_t{});
	EMPP_REQUIRE(not empty);
	EMPP_REQUIRE_EQ(empty.error(), std::make_error_code(std::errc::invalid_argument));
	const auto invalid_unmount = fs::unmount_target({});
	EMPP_REQUIRE(not invalid_unmount);
	EMPP_REQUIRE_EQ(invalid_unmount.error(), std::make_error_code(std::errc::invalid_argument));

	const auto mounted = fs::mounts();
	EMPP_REQUIRE(mounted);
	EMPP_REQUIRE(not mounted->empty());
	EMPP_REQUIRE(std::ranges::any_of(*mounted, [](const fs::mount_info &entry) {
		return entry.target == "/";
	}));
	const auto root = fs::find_mount_by_target("/");
	EMPP_REQUIRE(root);
	EMPP_REQUIRE(*root);
	const auto root_space = fs::space("/");
	EMPP_REQUIRE(root_space);
	EMPP_REQUIRE(root_space->total >= root_space->free);
}

EMPP_TEST("virtual-device", "filesystem formatter creates and detects ext4 images")
{
	namespace fs = libempp::storage;
	temporary_image image(16U * 1024U * 1024U);
	fs::format_options unsupported_options;
	unsupported_options.type = fs::filesystem_type::exfat;
	unsupported_options.mode = fs::format_mode::full;
	const auto unsupported = fs::format(image.path(), unsupported_options);
	EMPP_REQUIRE(not unsupported);
	EMPP_REQUIRE_EQ(unsupported.error(),
		std::make_error_code(std::errc::operation_not_supported));

	if(not executable_exists("/usr/sbin/mkfs.ext4") and
		not executable_exists("/sbin/mkfs.ext4"))
		return;

	fs::format_options invalid_options;
	invalid_options.timeout = std::chrono::milliseconds::zero();
	const auto invalid = fs::format(image.path(), invalid_options);
	EMPP_REQUIRE(not invalid);
	EMPP_REQUIRE_EQ(invalid.error(),
		std::make_error_code(std::errc::invalid_argument));
	const auto still_empty = fs::inspect_filesystem(image.path());
	EMPP_REQUIRE(still_empty);
	EMPP_REQUIRE(not *still_empty);

	fs::format_options options;
	options.type = fs::filesystem_type::ext4;
	options.label = "libempp-test";
	const auto formatted = fs::format(image.path(), options);
	EMPP_REQUIRE(formatted);
	EMPP_REQUIRE_EQ(formatted->type, std::string("ext4"));
	EMPP_REQUIRE_EQ(formatted->label, std::string("libempp-test"));
	EMPP_REQUIRE(not formatted->uuid.empty());
	EMPP_REQUIRE(formatted->size_bytes > 0);
	EMPP_REQUIRE(formatted->block_size > 0);

	const auto info = fs::inspect_filesystem(image.path());
	EMPP_REQUIRE(info);
	EMPP_REQUIRE(*info);
	EMPP_REQUIRE_EQ((*info)->uuid, formatted->uuid);
}

EMPP_TEST("virtual-device", "partition table supports create expand and delete on an image")
{
	if(not executable_exists("/usr/sbin/sfdisk") and
		not executable_exists("/sbin/sfdisk"))
		return;

	namespace fs = libempp::storage;
	temporary_image image(64U * 1024U * 1024U);
	auto disk = fs::inspect_disk(image.path());
	EMPP_REQUIRE(disk);
	EMPP_REQUIRE(not disk->table);
	fs::partition_options invalid_options;
	invalid_options.poll_interval = std::chrono::milliseconds::zero();
	const auto invalid_replace = fs::replace_partition_table(image.path(),
		fs::partition_table_type::gpt, {}, invalid_options);
	EMPP_REQUIRE(not invalid_replace);
	EMPP_REQUIRE_EQ(invalid_replace.error(),
		std::make_error_code(std::errc::invalid_argument));
	disk = fs::inspect_disk(image.path());
	EMPP_REQUIRE(disk);
	EMPP_REQUIRE(not disk->table);

	const auto no_table = fs::create_partition(image.path());
	EMPP_REQUIRE(not no_table);
	EMPP_REQUIRE_EQ(no_table.error(), fs::make_error_code(fs::errc::no_partition_table));

	disk = fs::replace_partition_table(image.path(), fs::partition_table_type::gpt);
	EMPP_REQUIRE(disk);
	EMPP_REQUIRE(disk->table);
	EMPP_REQUIRE_EQ(disk->table->type, std::string("gpt"));
	EMPP_REQUIRE_EQ(disk->partitions.size(), 0U);
	EMPP_REQUIRE_EQ(disk->size_bytes, 64U * 1024U * 1024U);
	EMPP_REQUIRE_EQ(disk->sector_size, 512U);

	fs::partition_spec spec;
	spec.start_sector = 2048;
	spec.size_sectors = 16384;
	spec.type = "linux";
	spec.name = "data";
	const auto created = fs::create_partition(image.path(), spec);
	EMPP_REQUIRE(created);
	EMPP_REQUIRE_EQ(created->number, 1U);
	EMPP_REQUIRE_EQ(created->start_sector, 2048U);
	EMPP_REQUIRE_EQ(created->size_sectors, 16384U);
	EMPP_REQUIRE_EQ(created->name, std::string("data"));
	if(executable_exists("/usr/sbin/mkfs.ext4") or
		executable_exists("/sbin/mkfs.ext4"))
	{
		temporary_image filesystem_image(8U * 1024U * 1024U);
		fs::format_options format;
		format.label = "partition-data";
		EMPP_REQUIRE(fs::format(filesystem_image.path(), format));
		copy_image_at(filesystem_image.path(), image.path(),
			created->start_sector * disk->sector_size);
		const auto inspected = fs::inspect_partition(image.path(), created->number);
		EMPP_REQUIRE(inspected);
		EMPP_REQUIRE(inspected->filesystem);
		EMPP_REQUIRE_EQ(inspected->filesystem->type, std::string("ext4"));
		EMPP_REQUIRE_EQ(inspected->filesystem->label, std::string("partition-data"));
	}

	const auto expanded = fs::expand_partition(image.path(), created->number, 32768);
	EMPP_REQUIRE(expanded);
	EMPP_REQUIRE_EQ(expanded->start_sector, created->start_sector);
	EMPP_REQUIRE_EQ(expanded->size_sectors, 32768U);
	const auto filled = fs::expand_partition(image.path(), created->number);
	EMPP_REQUIRE(filled);
	EMPP_REQUIRE_EQ(filled->start_sector, created->start_sector);
	EMPP_REQUIRE(filled->size_sectors > expanded->size_sectors);

	const auto invalid_delete = fs::delete_partition(image.path(), 0);
	EMPP_REQUIRE(not invalid_delete);
	EMPP_REQUIRE_EQ(invalid_delete.error(), std::make_error_code(std::errc::invalid_argument));
	EMPP_REQUIRE(fs::delete_partition(image.path(), created->number));
	disk = fs::inspect_disk(image.path());
	EMPP_REQUIRE(disk);
	EMPP_REQUIRE(disk->partitions.empty());

	const auto missing_partition = fs::inspect_partition(image.path(), created->number);
	EMPP_REQUIRE(not missing_partition);
	EMPP_REQUIRE_EQ(missing_partition.error(), fs::make_error_code(fs::errc::partition_not_found));
}
