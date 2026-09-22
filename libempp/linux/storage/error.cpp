// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "error.h"
#ifdef __linux__

namespace libempp::storage { namespace
{

class storage_error_category final : public std::error_category
{
public:
	[[nodiscard]] const char *name() const noexcept override {
		return "libempp.storage";
	}

	[[nodiscard]] std::string message(int value) const override
	{
		switch(static_cast<errc>(value))
		{
			case errc::command_failed:
				return "storage helper command failed";
			case errc::command_terminated:
				return "storage helper command was terminated";
			case errc::no_partition_table:
				return "device has no partition table";
			case errc::partition_not_found:
				return "partition was not found";
			case errc::ambiguous_signature:
				return "device has ambiguous filesystem signatures";
			case errc::malformed_mount_table:
				return "malformed Linux mount table";
			case errc::command_timed_out:
				return "storage helper command timed out";
			case errc::not_mounted:
				return "path is not mounted";
			case errc::verification_failed:
				return "storage operation could not be verified";
			case errc::device_identity_required:
				return "block device operation requires a resolved device identity";
			case errc::device_changed:
				return "block device identity changed during operation";
		}
		return "unknown storage error";
	}
};

const storage_error_category local_error_category;

} // namespace

const std::error_category &error_category() noexcept
{
	return local_error_category;
}

std::error_code make_error_code(errc error) noexcept
{
	return {static_cast<int>(error), error_category()};
}

} // namespace libempp::storage

#endif //__linux__
