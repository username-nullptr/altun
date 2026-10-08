// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_STORAGE_MOUNT_H
#define ALTUN_LINUX_STORAGE_MOUNT_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/storage/types.h>
#include <altun/linux/global.h>

namespace altun::storage
{

struct mount_options : operation_options
{
	enum class backend {
		kernel,
		helper,
		automatic
	};
	riwo::optional<std::string> filesystem_type {};

	// Linux MS_* flags from <sys/mount.h>.
	unsigned long flags = 0;

	// Comma-separated filesystem-specific mount data.
	std::string data {};
	backend implementation = backend::automatic;
};

struct unmount_options
{
	// Linux MNT_* flags from <sys/mount.h>.
	int flags = 0;
};

struct mount_info
{
	uint32_t id = 0;
	uint32_t parent_id = 0;
	uint32_t device_major = 0;
	uint32_t device_minor = 0;

	path_t root {};
	path_t target {};
	path_t source {};

	std::string filesystem_type {};
	std::string options {};
	std::string super_options {};
};

[[nodiscard]] ALTUN_LINUX_API result_t<mount_info> mount (
	const device_info &source, const path_t &target, const mount_options &options = {}
);

[[nodiscard]] ALTUN_LINUX_API result_t<> unmount_target (
	const path_t &target, const unmount_options &options = {}
);

[[nodiscard]] ALTUN_LINUX_API result_t<std::size_t> unmount_device (
	const device_info &device, const unmount_options &options = {}
);

[[nodiscard]] ALTUN_LINUX_API result_t<std::vector<mount_info>> mounts();

[[nodiscard]] ALTUN_LINUX_API result_t<riwo::optional<mount_info>>
find_mount_by_target(const path_t &target);

[[nodiscard]] ALTUN_LINUX_API result_t<std::vector<mount_info>>
find_mounts_by_device(const path_t &device);

[[nodiscard]] ALTUN_LINUX_API result_t<std::vector<mount_info>>
find_mounts_by_device(const device_info &device);

} // namespace altun::storage

#endif //__linux__
#endif // ALTUN_LINUX_STORAGE_MOUNT_H
