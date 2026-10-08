// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "format.h"
#ifdef __linux__

#include "detail/common.h"
#include "information.h"
#include "mount.h"

#include <linux/fs.h>
#include <sys/sysmacros.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <unistd.h>
#include <fcntl.h>

namespace altun::storage { namespace
{

[[nodiscard]] riwo::optional<device_id> ensure_format_target
(const path_t &device, const riwo::optional<device_id> &expected)
{
	detail::ensure_path(device, "altun::storage::format");
	struct stat status {};

	if( ::stat(device.c_str(), &status) < 0 )
		detail::throw_errno("altun::storage::format");

	if( not S_ISBLK(status.st_mode) )
	{
		if( expected )
		{
			detail::throw_error(make_error_code(errc::device_changed),
				"altun::storage::format"
			);
		}
		if( not S_ISREG(status.st_mode) )
		{
			detail::throw_error(std::make_error_code(std::errc::invalid_argument),
				"altun::storage::format"
			);
		}
		return riwo::nullopt;
	}
	const device_id identity {
		.major = ::major(status.st_rdev),
		.minor = ::minor(status.st_rdev)
	};
	if( not expected )
	{
		detail::throw_error(make_error_code(errc::device_identity_required),
			"altun::storage::format"
		);
	}
	if( expected and identity != *expected )
	{
		detail::throw_error(make_error_code(errc::device_changed),
			"altun::storage::format"
		);
	}
	const int descriptor = ::open(device.c_str(), O_RDONLY | O_CLOEXEC);
	if( descriptor < 0 )
		detail::throw_errno("altun::storage::format");

	int read_only = 0;
	const int ioctl_result = ::ioctl(descriptor, BLKROGET, &read_only);
	const int ioctl_error = errno;
	::close(descriptor);

	if( ioctl_result < 0 )
	{
		detail::throw_error(std::error_code(ioctl_error, std::system_category()),
			"altun::storage::format"
		);
	}
	if( read_only != 0 )
	{
		detail::throw_error(std::make_error_code(std::errc::read_only_file_system),
			"altun::storage::format"
		);
	}
	auto mounted = find_mounts_by_device(device);
	if( not mounted )
		detail::throw_error(mounted.error(), "altun::storage::format");

	if( not mounted->empty() )
	{
		detail::throw_error(std::make_error_code(std::errc::device_or_resource_busy),
			"altun::storage::format"
		);
	}
	return identity;
}

void ensure_identity(const path_t &device, const riwo::optional<device_id> &before)
{
	if( not before )
		return ;

	struct stat status {};
	if( ::stat(device.c_str(), &status) < 0 or not S_ISBLK(status.st_mode) )
	{
		detail::throw_error(make_error_code(errc::device_changed),
			"altun::storage::format"
		);
	}
	const device_id after {
		.major = ::major(status.st_rdev),
		.minor = ::minor(status.st_rdev)
	};
	if( after != *before )
	{
		detail::throw_error(make_error_code(errc::device_changed),
			"altun::storage::format"
		);
	}
}

[[nodiscard]] std::vector<std::string> formatter_arguments
(const path_t &device, const format_options &options, bool legacy_exfat = false)
{
	std::vector<std::string> arguments;
	switch(options.type)
	{
		case filesystem_type::exfat:
			if( options.mode != format_mode::quick )
			{
				detail::throw_error(std::make_error_code(std::errc::operation_not_supported),
					"altun::storage::format"
				);
			}
			arguments.emplace_back("mkfs.exfat");
			if( not options.label.empty() )
			{
				arguments.emplace_back(legacy_exfat ? "-n" : "-L");
				arguments.push_back(options.label);
			}
			break;

		case filesystem_type::ext4:
			arguments.emplace_back("mkfs.ext4");
			arguments.emplace_back("-F");

			if( options.mode == format_mode::full )
			{
				arguments.emplace_back("-E");
				arguments.emplace_back("lazy_itable_init=0,lazy_journal_init=0");
			}
			if( not options.label.empty() )
			{
				arguments.emplace_back("-L");
				arguments.push_back(options.label);
			}
			break;

		case filesystem_type::ntfs:
			arguments.emplace_back("mkfs.ntfs");
			arguments.emplace_back("-F");

			if( options.mode == format_mode::quick )
				arguments.emplace_back("-Q");

		if( not options.label.empty() )
			{
				arguments.emplace_back("-L");
				arguments.push_back(options.label);
			}
			break;

		default:
			detail::throw_error(std::make_error_code(std::errc::invalid_argument),
				"altun::storage::format"
			);
	}
	arguments.push_back(device.native());
	return arguments;
}

} // namespace

namespace detail
{

[[nodiscard]] static result_t<filesystem_info> format
(const path_t &device, const format_options &options, const riwo::optional<device_id> &expected)
{
	return detail::capture_expected<filesystem_info>([&]
	{
		ensure_string(options.label, "altun::storage::format");
		const auto identity = ensure_format_target(device, expected);
		try {
			run_command(formatter_arguments(device, options), {}, options.timeout);
		}
		catch(const std::system_error &exception)
		{
			// exfatprogs uses -L while the older exfat-utils formatter uses -n.
			// Retry only an option/command failure; timeouts and system failures must
			// remain visible and must not start a second destructive operation.
			if( options.type != filesystem_type::exfat or options.label.empty() or
				exception.code() != make_error_code(errc::command_failed) )
				throw;
			run_command(formatter_arguments(device, options, true), {},
				options.timeout);
		}
		ensure_identity(device, identity);

		auto inspected = inspect_filesystem(device);
		if( not inspected )
			throw_error(inspected.error(), "altun::storage::format");

		if( not *inspected or (*inspected)->type != string(options.type) or
			(not options.label.empty() and (*inspected)->label != options.label) )
		{
			throw_error(make_error_code(errc::verification_failed),
				"altun::storage::format"
			);
		}
		return **inspected;
	});
}

} //namespace detail

result_t<filesystem_info> format(const path_t &device, const format_options &options)
{
	return detail::format(device, options, riwo::nullopt);
}

result_t<filesystem_info> format(const device_info &device, const format_options &options)
{
	return detail::capture_expected<filesystem_info>([&]
	{
		detail::ensure_device(device, "altun::storage::format");
		auto formatted = detail::format(device.device, options, device.id);

		if( not formatted )
			detail::throw_error(formatted.error(), "altun::storage::format");

		detail::ensure_device(device, "altun::storage::format");
		return std::move(*formatted);
	});
}

const char *string(filesystem_type type) noexcept
{
	switch(type)
	{
		case filesystem_type::exfat: return "exfat";
		case filesystem_type::ext4: return "ext4";
		case filesystem_type::ntfs: return "ntfs";
	}
	return "";
}

} // namespace altun::storage

#endif //__linux__
