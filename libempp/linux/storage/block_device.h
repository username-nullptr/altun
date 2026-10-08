// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_STORAGE_BLOCK_DEVICE_H
#define LIBEMPP_LINUX_STORAGE_BLOCK_DEVICE_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/storage/types.h>
#include <libempp/linux/global.h>

namespace libempp::storage
{

class LIBEMPP_LINUX_API block_device
{
	RIWO_DISABLE_COPY(block_device)
	class impl;

public:
	using path_t = std::filesystem::path;
	using offset_t = uint64_t;

	using capacity_t = uint64_t;
	using block_size_t = uint32_t;

	explicit block_device(std::unique_ptr<impl> implementation) noexcept;

public:
	~block_device();
	block_device(block_device &&other) noexcept;
	block_device &operator=(block_device &&other) noexcept;

	[[nodiscard]] static result_t<std::unique_ptr<block_device>> open(path_t device);
	[[nodiscard]] static result_t<std::unique_ptr<block_device>> open(const device_info &device);
	block_device &close() noexcept;

public:
	// offset and the returned size are expressed in bytes.
	[[nodiscard]] riwo::io_expected read_some_at (
		offset_t offset, riwo::mutable_buffer buffer
	);
	[[nodiscard]] result_t<block_info> refresh();
	[[nodiscard]] result_t<block_info> info() const;
	[[nodiscard]] bool is_open() const noexcept;

private:
	std::unique_ptr<impl> m_impl;
};

[[nodiscard]] LIBEMPP_LINUX_API result_t<std::vector<device_info>> enumerate_devices();

// Resolves aliases such as /dev/disk/by-id/* into one stable device identity.
[[nodiscard]] LIBEMPP_LINUX_API result_t<device_info> resolve_device(const path_t &device);

} // namespace libempp::storage

#endif //__linux__
#endif // LIBEMPP_LINUX_STORAGE_BLOCK_DEVICE_H
