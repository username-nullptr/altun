// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "mount.h"
#ifdef __linux__

#include "detail/common.h"
#include "information.h"

#include <sys/sysmacros.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <utility>

namespace libempp::storage { namespace
{

[[nodiscard]] riwo::optional<std::uint32_t> parse_uint32(std::string_view value)
{
	std::uint32_t result = 0;
	const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);

	if( parsed.ec != std::errc{} or parsed.ptr != value.data() + value.size() )
		return riwo::nullopt;
	return result;
}

[[nodiscard]] std::string decode_mount_field(std::string_view value)
{
	std::string result;
	result.reserve(value.size());

	for(std::size_t index = 0; index < value.size(); ++index)
	{
		if( value[index] == '\\' and index + 3 < value.size() and
			value[index + 1] >= '0' and value[index + 1] <= '7' and
			value[index + 2] >= '0' and value[index + 2] <= '7' and
			value[index + 3] >= '0' and value[index + 3] <= '7' )
		{
			const unsigned int decoded =
				static_cast<unsigned int>(value[index + 1] - '0') * 64U +
				static_cast<unsigned int>(value[index + 2] - '0') * 8U +
				static_cast<unsigned int>(value[index + 3] - '0');

			result.push_back(static_cast<char>(decoded));
			index += 3;
		}
		else
			result.push_back(value[index]);
	}
	return result;
}

[[nodiscard]] std::vector<mount_info> mounts_impl()
{
	std::ifstream stream("/proc/self/mountinfo");
	if( not stream.is_open() )
		detail::throw_errno("libempp::storage::mounts");

	std::vector<mount_info> result;
	std::string line;

	while( std::getline(stream, line) )
	{
		std::istringstream fields(line);
		std::string id_text, parent_text, device_text;
		std::string root, target, options;

		if( not (fields >> id_text >> parent_text >> device_text >> root >> target >> options) )
		{
			detail::throw_error(make_error_code(errc::malformed_mount_table),
				"libempp::storage::mounts"
			);
		}
		std::string field;
		while(fields >> field and field != "-") {}

		std::string filesystem_type, source, super_options;
		if( field != "-" or not (fields >> filesystem_type >> source >> super_options) )
		{
			detail::throw_error(make_error_code(errc::malformed_mount_table),
				"libempp::storage::mounts"
			);
		}
		const auto id = parse_uint32(id_text);
		const auto parent_id = parse_uint32(parent_text);
		const auto separator = device_text.find(':');

		const auto major_number = separator == std::string::npos ?
			riwo::optional<uint32_t>{} :
			parse_uint32(std::string_view(device_text).substr(0, separator));

		const auto minor_number = separator == std::string::npos ?
			riwo::optional<uint32_t>{} :
			parse_uint32(std::string_view(device_text).substr(separator + 1));

		if( not id or not parent_id or not major_number or not minor_number )
		{
			detail::throw_error(make_error_code(errc::malformed_mount_table),
				"libempp::storage::mounts"
			);
		}
		mount_info info;
		info.id = *id;

		info.parent_id = *parent_id;
		info.device_major = *major_number;
		info.device_minor = *minor_number;

		info.root = decode_mount_field(root);
		info.target = decode_mount_field(target);
		info.source = decode_mount_field(source);

		info.filesystem_type = std::move(filesystem_type);
		info.options = std::move(options);
		info.super_options = std::move(super_options);

		result.push_back(std::move(info));
	}
	if( stream.bad() )
		detail::throw_errno("libempp::storage::mounts");
	return result;
}

[[nodiscard]] device_id device_identity(const path_t &device)
{
	detail::ensure_path(device, "libempp::storage::device_identity");
	struct stat status {};

	if( ::stat(device.c_str(), &status) < 0 )
		detail::throw_errno("libempp::storage::device_identity");

	if( not S_ISBLK(status.st_mode) )
	{
		detail::throw_error(std::error_code(ENOTBLK, std::system_category()),
			"libempp::storage::device_identity"
		);
	}
	return {
		.major = ::major(status.st_rdev),
		.minor = ::minor(status.st_rdev)
	};
}

[[nodiscard]] riwo::optional<device_id> block_identity(const path_t &device) noexcept
{
	struct stat status {};
	if( ::stat(device.c_str(), &status) < 0 or not S_ISBLK(status.st_mode) )
		return riwo::nullopt;

	return device_id {
		.major = ::major(status.st_rdev),
		.minor = ::minor(status.st_rdev)
	};
}

[[nodiscard]] bool same_or_child(const path_t &root, std::string_view root_text, const device_id &candidate)
{
	std::error_code error;
	const auto path = std::filesystem::canonical (
		std::format("/sys/dev/block/{}:{}", candidate.major, candidate.minor), error
	);
	if( error )
		return false;

	return path == root or
		path.native().substr(0, root_text.size()) == root_text;
}

[[nodiscard]] std::vector<mount_info> mounts_for(const device_id &id)
{
	auto entries = mounts_impl();
	const auto root = std::filesystem::canonical(std::format (
		"/sys/dev/block/{}:{}", id.major, id.minor
	));
	const auto root_text = root.native() + '/';

	std::erase_if(entries, [&root, &root_text](const mount_info &entry)
	{
		if( same_or_child(root, root_text, {
			.major = entry.device_major,
			.minor = entry.device_minor}) )
			return false;

		const auto source_id = block_identity(entry.source);
		return not source_id or not same_or_child(root, root_text, *source_id);
	});
	return entries;
}

[[nodiscard]] riwo::optional<mount_info> mount_at(const path_t &target)
{
	const auto normalized = std::filesystem::weakly_canonical(target);
	for(auto &entry : mounts_impl())
	{
		if( std::filesystem::weakly_canonical(entry.target) == normalized )
			return std::move(entry);
	}
	return riwo::nullopt;
}

[[nodiscard]] std::string mount_option_text(unsigned long flags, const std::string &data)
{
	constexpr unsigned long supported = MS_RDONLY | MS_NOSUID | MS_NODEV |
		MS_NOEXEC | MS_SYNCHRONOUS | MS_NOATIME | MS_NODIRATIME | MS_RELATIME;

	if( (flags & ~supported) != 0 )
	{
		detail::throw_error(std::make_error_code(std::errc::operation_not_supported),
			"libempp::storage::mount helper"
		);
	}
	std::vector<std::string_view> values;
	values.emplace_back((flags & MS_RDONLY) != 0 ? "ro" : "rw");

	if( flags & MS_NOSUID )
		values.emplace_back("nosuid");
	if( flags & MS_NODEV )
		values.emplace_back("nodev");
	if( flags & MS_NOEXEC )
		values.emplace_back("noexec");
	if( flags & MS_SYNCHRONOUS )
		values.emplace_back("sync");
	if( flags & MS_NOATIME )
		values.emplace_back("noatime");
	if( flags & MS_NODIRATIME )
		values.emplace_back("nodiratime");
	if( flags & MS_RELATIME )
		values.emplace_back("relatime");

	std::string result;
	for(auto value : values)
	{
		if( not result.empty() ) result += ',';
		result += value;
	}
	if( not data.empty() )
	{
		if( not result.empty() ) result += ',';
		result += data;
	}
	return result;
}

void helper_mount
(const path_t &source, const path_t &target, std::string_view filesystem_type, const mount_options &options)
{
	std::vector<std::string> arguments {"mount"};
	if( not filesystem_type.empty() )
	{
		arguments.emplace_back("-t");
		arguments.emplace_back(filesystem_type);
	}
	if( const auto option_text = mount_option_text(options.flags, options.data); not option_text.empty() )
	{
		arguments.emplace_back("-o");
		arguments.push_back(option_text);
	}
	arguments.push_back(source.native());
	arguments.push_back(target.native());
	detail::run_command(std::move(arguments), {}, options.timeout);
}

class RIWO_DECL_HIDDEN mount_rollback
{
public:
	explicit mount_rollback(path_t target) :
		m_target(std::move(target)) {}

	~mount_rollback()
	{
		if( m_active )
			static_cast<void>(::umount2(m_target.c_str(), MNT_DETACH));
	}

	void arm() noexcept {
		m_active = true;
	}

	void release() noexcept {
		m_active = false;
	}

private:
	path_t m_target;
	bool m_active = false;
};

} // namespace

namespace detail
{

[[nodiscard]] static result_t<mount_info> mount
(const path_t &source, const path_t &target, const mount_options &options, const riwo::optional<device_id> &expected)
{
	return detail::capture_expected<mount_info>([&]
	{
		ensure_path(source, "libempp::storage::mount");
		ensure_path(target, "libempp::storage::mount");
		ensure_string(options.data, "libempp::storage::mount");

		if( options.timeout <= std::chrono::milliseconds::zero() )
		{
			throw_error(std::make_error_code(std::errc::invalid_argument),
				"libempp::storage::mount"
			);
		}
		if( mount_at(target) )
		{
			throw_error(std::make_error_code(std::errc::device_or_resource_busy),
				"libempp::storage::mount"
			);
		}
		const auto source_id = device_identity(source);
		if( expected and source_id != *expected )
		{
			throw_error(make_error_code(errc::device_changed),
				"libempp::storage::mount"
			);
		}
		std::string filesystem_type;
		if( options.filesystem_type )
			filesystem_type = *options.filesystem_type;
		else
		{
			auto inspected = inspect_filesystem(source);
			if( not inspected )
				throw_error(inspected.error(), "libempp::storage::mount");

			if( not *inspected )
			{
				throw_error(std::make_error_code(std::errc::invalid_argument),
					"libempp::storage::mount"
				);
			}
			filesystem_type = (*inspected)->type;
		}
		bool mounted = false;
		mount_rollback rollback(target);

		if( options.implementation != mount_options::backend::helper )
		{
			if( const void *data = options.data.empty() ? nullptr : options.data.c_str();
				::mount(source.c_str(), target.c_str(), filesystem_type.c_str(), options.flags, data) == 0 )
			{
				mounted = true;
				rollback.arm();
			}
			else if( options.implementation == mount_options::backend::kernel or
					 (errno != EINVAL and errno != ENODEV and errno != ENOENT) )
				throw_errno("libempp::storage::mount");
		}
		if( not mounted )
		{
			helper_mount(source, target, filesystem_type, options);
			rollback.arm();
		}
		auto observed = mount_at(target);
		if( not observed )
		{
			throw_error(make_error_code(errc::verification_failed),
				"libempp::storage::mount"
			);
		}
		const device_id mounted_id {
			.major = observed->device_major,
			.minor = observed->device_minor
		};
		if( mounted_id != source_id and block_identity(observed->source) != source_id )
		{
			throw_error(make_error_code(errc::verification_failed),
				"libempp::storage::mount"
			);
		}
		if( device_identity(source) != source_id )
		{
			throw_error(make_error_code(errc::device_changed),
				"libempp::storage::mount"
			);
		}
		rollback.release();
		return *observed;
	});
}

[[nodiscard]] static result_t<std::size_t> unmount_device
(const path_t &device, const unmount_options &options, const riwo::optional<device_id> &expected)
{
	return detail::capture_expected<std::size_t>([&]
	{
		const auto identity = device_identity(device);
		if( expected and identity != *expected )
		{
			throw_error(make_error_code(errc::device_changed),
				"libempp::storage::unmount_device"
			);
		}
		auto entries = mounts_for(identity);
		std::ranges::sort(entries, [](const auto &left, const auto &right) {
			return left.target.native().size() > right.target.native().size();
		});
		for(const auto &entry : entries)
		{
			if( ::umount2(entry.target.c_str(), options.flags) < 0 )
				throw_errno("libempp::storage::unmount_device");
		}
		if( device_identity(device) != identity )
		{
			throw_error(make_error_code(errc::device_changed),
				"libempp::storage::unmount_device"
			);
		}
		if( not mounts_for(identity).empty() )
		{
			throw_error(make_error_code(errc::verification_failed),
				"libempp::storage::unmount_device"
			);
		}
		return entries.size();
	});
}

} //namespace detail

result_t<> unmount_target(const path_t &target, const unmount_options &options)
{
	return detail::capture_expected([&]
	{
		detail::ensure_path(target, "libempp::storage::unmount_target");
		if( not mount_at(target) )
		{
			detail::throw_error(make_error_code(errc::not_mounted),
				"libempp::storage::unmount_target"
			);
		}
		if( ::umount2(target.c_str(), options.flags) < 0 )
			detail::throw_errno("libempp::storage::unmount_target");

		if( mount_at(target) )
		{
			detail::throw_error(make_error_code(errc::verification_failed),
				"libempp::storage::unmount_target"
			);
		}
	});
}

result_t<mount_info> mount(const device_info &source, const path_t &target, const mount_options &options)
{
	return detail::capture_expected<mount_info>([&]
	{
		detail::ensure_device(source, "libempp::storage::mount");
		auto mounted = detail::mount(source.device, target, options, source.id);
		if( not mounted )
			detail::throw_error(mounted.error(), "libempp::storage::mount");
		detail::ensure_device(source, "libempp::storage::mount");
		return std::move(*mounted);
	});
}

result_t<std::size_t> unmount_device(const device_info &device, const unmount_options &options)
{
	return detail::capture_expected<std::size_t>([&]
	{
		detail::ensure_device(device, "libempp::storage::unmount_device");
		auto count = detail::unmount_device(device.device, options, device.id);
		if( not count )
			detail::throw_error(count.error(), "libempp::storage::unmount_device");
		detail::ensure_device(device, "libempp::storage::unmount_device");
		return *count;
	});
}

result_t<std::vector<mount_info>> mounts()
{
	return detail::capture_expected<std::vector<mount_info>>([] {
		return mounts_impl();
	});
}

result_t<riwo::optional<mount_info>> find_mount_by_target(const path_t &target)
{
	return detail::capture_expected<riwo::optional<mount_info>>([&]
	{
		detail::ensure_path(target, "libempp::storage::find_mount_by_target");
		return mount_at(target);
	});
}

result_t<std::vector<mount_info>> find_mounts_by_device(const path_t &device)
{
	return detail::capture_expected<std::vector<mount_info>>([&]{
		return mounts_for(device_identity(device));
	});
}

result_t<std::vector<mount_info>> find_mounts_by_device(const device_info &device)
{
	return detail::capture_expected<std::vector<mount_info>>([&]
	{
		detail::ensure_device(device, "libempp::storage::find_mounts_by_device");
		auto found = find_mounts_by_device(device.device);

		if( not found )
		{
			detail::throw_error(found.error(),
				"libempp::storage::find_mounts_by_device"
			);
		}
		detail::ensure_device(device, "libempp::storage::find_mounts_by_device");
		return std::move(*found);
	});
}

} // namespace libempp::storage

#endif //__linux__
