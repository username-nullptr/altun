// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "block_device.h"

#ifdef __linux__
#include "error.h"
#include <linux/fs.h>

#include <sys/sysmacros.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <libudev.h>
#include <unistd.h>
#include <fcntl.h>

#include <memory>

namespace libempp::storage { namespace
{

[[nodiscard]] bool sysattr_bool(udev_device *device, const char *name) noexcept
{
	const char *value = ::udev_device_get_sysattr_value(device, name);
	return value and value[0] == '1' and value[1] == '\0';
}

[[nodiscard]] std::string property(udev_device *device, const char *name)
{
	const char *value = ::udev_device_get_property_value(device, name);
	return value ? value : "";
}

[[nodiscard]] std::error_code udev_error(int result) noexcept
{
	const int value = result < 0 ? -result : result;
	return { value != 0 ? value : EIO, std::system_category() };
}

[[nodiscard]] device_info make_device_info(udev_device *device)
{
	device_info info {};
	const char *node = ::udev_device_get_devnode(device);
	const char *sys_path = ::udev_device_get_syspath(device);

	if( not node or not sys_path )
		return info;

	info.device = node;
	info.sys_path = sys_path;

	const auto id = ::udev_device_get_devnum(device);
	info.id.major = ::major(id);
	info.id.minor = ::minor(id);

	const char *devtype = ::udev_device_get_devtype(device);
	info.partition = devtype and std::string_view(devtype) == "partition";

	info.removable = sysattr_bool(device, "removable");
	info.read_only = sysattr_bool(device, "ro");

	info.bus = property(device, "ID_BUS");
	info.model = property(device, "ID_MODEL");
	info.serial = property(device, "ID_SERIAL_SHORT");

	if( info.partition )
	{
		if( auto *parent = ::udev_device_get_parent_with_subsystem_devtype(device, "block", "disk") )
		{
			if( const char *parent_node = ::udev_device_get_devnode(parent) )
				info.parent_device = path_t(parent_node);

			info.removable = info.removable or sysattr_bool(parent, "removable");
			info.read_only = info.read_only or sysattr_bool(parent, "ro");

			info.bus = property(parent, "ID_BUS");
			info.model = property(parent, "ID_MODEL");
			info.serial = property(parent, "ID_SERIAL_SHORT");
		}
	}
	return info;
}

} // namespace

class LIBGS_DECL_HIDDEN block_device::impl
{
	LIBGS_DISABLE_COPY_MOVE(impl)

public:
	impl() = default;
	~impl() {
		close();
	}

	void open(path_t device, std::error_code &error) noexcept
	{
		close();
		error.clear();

		if( device.empty() )
		{
			error = std::make_error_code(std::errc::invalid_argument);
			return ;
		}
		const int descriptor = ::open(device.c_str(), O_RDONLY | O_CLOEXEC);
		if( descriptor < 0 )
		{
			error = std::error_code(errno, std::system_category());
			return ;
		}
		m_descriptor = descriptor;
		m_info.device = std::move(device);

		refresh(error);
		if( error )
			close();
	}

	void refresh(std::error_code &error) noexcept
	{
		error.clear();
		if( m_descriptor < 0 )
		{
			error = std::make_error_code(std::errc::bad_file_descriptor);
			return ;
		}
		struct stat status {};
		if( ::fstat(m_descriptor, &status) < 0 )
		{
			error = std::error_code(errno, std::system_category());
			return ;
		}
		if( not S_ISBLK(status.st_mode) )
		{
			error = std::error_code(ENOTBLK, std::system_category());
			return ;
		}
		block_info updated = m_info;
		updated.id.major = ::major(status.st_rdev);
		updated.id.minor = ::minor(status.st_rdev);

		int logical_block_size = 0;
		unsigned int physical_block_size = 0;
		int read_only = 0;

		if( ::ioctl(m_descriptor, BLKGETSIZE64, &updated.geometry.capacity_bytes) < 0 or
			::ioctl(m_descriptor, BLKSSZGET, &logical_block_size) < 0 or
			::ioctl(m_descriptor, BLKPBSZGET, &physical_block_size) < 0 or
			::ioctl(m_descriptor, BLKROGET, &read_only) < 0 )
		{
			error = std::error_code(errno, std::system_category());
			return ;
		}
		if( logical_block_size <= 0 or physical_block_size == 0 )
		{
			error = std::make_error_code(std::errc::io_error);
			return ;
		}
		updated.geometry.logical_block_size = static_cast<block_size_t>(logical_block_size);
		updated.geometry.physical_block_size = physical_block_size;
		updated.geometry.read_only = read_only != 0;
		m_info = std::move(updated);
	}

	void close() noexcept
	{
		if( m_descriptor >= 0 )
			::close(m_descriptor);
		m_descriptor = -1;
		m_info = {};
	}

	[[nodiscard]] libgs::io_expected read_some_at(offset_t offset, const libgs::mutable_buffer &buffer) const
	{
		if( m_descriptor < 0 )
			return libgs::io_unexpected(std::make_error_code(std::errc::bad_file_descriptor));

		if( buffer.size() == 0 or offset >= m_info.geometry.capacity_bytes )
			return libgs::make_io_expected(0);

		if( offset > static_cast<offset_t>(std::numeric_limits<off_t>::max()) )
			return libgs::io_unexpected(std::make_error_code(std::errc::value_too_large));

		auto count = static_cast<size_t>(std::min<capacity_t>(
			m_info.geometry.capacity_bytes - offset, buffer.size()
		));
		count = std::min(count,
			static_cast<size_t>(std::numeric_limits<ssize_t>::max()));

		ssize_t read_size = -1;
		do {
			read_size = ::pread(m_descriptor, buffer.data(), count,
				static_cast<off_t>(offset));
		}
		while( read_size < 0 and errno == EINTR );

		if( read_size < 0 )
			return libgs::io_unexpected(std::error_code(errno, std::system_category()));

		return libgs::make_io_expected(static_cast<size_t>(read_size));
	}

	int m_descriptor = -1;
	block_info m_info {};
};

block_device::block_device(std::unique_ptr<impl> implementation) noexcept :
	m_impl(std::move(implementation)) {}

block_device::~block_device() = default;

block_device::block_device(block_device &&other) noexcept = default;

block_device &block_device::operator=(block_device &&other) noexcept = default;

result_t<std::unique_ptr<block_device>> block_device::open(path_t device)
{
	std::error_code error;
	auto implementation = std::make_unique<impl>();

	implementation->open(std::move(device), error);
	if( error )
		return libgs::sys_unexpected(error);

	return std::make_unique<block_device>(std::move(implementation));
}

result_t<std::unique_ptr<block_device>> block_device::open(const device_info &device)
{
	auto opened = open(device.device);
	if( not opened )
		return opened;

	const auto current = (*opened)->info();
	if( not current )
		return libgs::sys_unexpected(current.error());

	auto resolved = resolve_device(device.device);
	if( not resolved )
		return libgs::sys_unexpected(resolved.error());

	if( current->id != device.id or resolved->id != current->id or
		(not device.sys_path.empty() and resolved->sys_path != device.sys_path) or
		(not device.serial.empty() and resolved->serial != device.serial) )
		return libgs::sys_unexpected(make_error_code(errc::device_changed));
	return opened;
}

result_t<std::vector<device_info>> enumerate_devices()
{
	using context_ptr = std::unique_ptr<udev, decltype(&::udev_unref)>;
	using enumerate_ptr = std::unique_ptr<udev_enumerate, decltype(&::udev_enumerate_unref)>;
	using device_ptr = std::unique_ptr<udev_device, decltype(&::udev_device_unref)>;

	context_ptr context(::udev_new(), &::udev_unref);
	if( not context )
		return libgs::sys_unexpected(std::make_error_code(std::errc::not_enough_memory));

	enumerate_ptr devices(::udev_enumerate_new(context.get()), &::udev_enumerate_unref);
	if( not devices )
		return libgs::sys_unexpected(std::make_error_code(std::errc::not_enough_memory));

	if( const int error = ::udev_enumerate_add_match_subsystem(devices.get(), "block");
		error < 0 )
		return libgs::sys_unexpected(udev_error(error));

	if( const int error = ::udev_enumerate_scan_devices(devices.get()); error < 0 )
		return libgs::sys_unexpected(udev_error(error));

	std::vector<device_info> result;
	udev_list_entry *entry = nullptr;

	udev_list_entry_foreach(entry, ::udev_enumerate_get_list_entry(devices.get()))
	{
		const char *sys_path = ::udev_list_entry_get_name(entry);
		if( not sys_path )
			continue;

		device_ptr device (
			::udev_device_new_from_syspath(context.get(), sys_path),
			&::udev_device_unref
		);
		if( not device )
			continue;

		if( const char *node = ::udev_device_get_devnode(device.get()); not node )
			continue;

		auto info = make_device_info(device.get());
		if( auto opened = block_device::open(info.device) )
		{
			if( auto current = (*opened)->info() )
			{
				info.geometry = current->geometry;
				info.read_only = info.read_only or current->geometry.read_only;
			}
		}
		result.push_back(std::move(info));
	}
	std::ranges::sort(result, {}, &device_info::device);
	return result;
}

result_t<device_info> resolve_device(const path_t &device)
{
	if( device.empty() )
		return libgs::sys_unexpected(std::make_error_code(std::errc::invalid_argument));

	struct stat status {};
	if( ::stat(device.c_str(), &status) < 0 )
		return libgs::sys_unexpected(std::error_code(errno, std::system_category()));

	if( not S_ISBLK(status.st_mode) )
		return libgs::sys_unexpected(std::error_code(ENOTBLK, std::system_category()));

	using context_ptr = std::unique_ptr<udev, decltype(&::udev_unref)>;
	using device_ptr = std::unique_ptr<udev_device, decltype(&::udev_device_unref)>;

	context_ptr context(::udev_new(), &::udev_unref);
	if( not context )
		return libgs::sys_unexpected(std::make_error_code(std::errc::not_enough_memory));

	device_ptr resolved (
		::udev_device_new_from_devnum(context.get(), 'b', status.st_rdev),
		&::udev_device_unref
	);
	if( not resolved )
		return libgs::sys_unexpected(std::make_error_code(std::errc::no_such_device));

	auto info = make_device_info(resolved.get());
	if( info.device.empty() )
		return libgs::sys_unexpected(std::make_error_code(std::errc::no_such_device));

	if( auto opened = block_device::open(info.device) )
	{
		if( auto current = (*opened)->info() )
		{
			info.geometry = current->geometry;
			info.read_only = info.read_only or current->geometry.read_only;
		}
	}
	return info;
}

block_device &block_device::close() noexcept
{
	if( m_impl )
		m_impl->close();
	return *this;
}

libgs::io_expected block_device::read_some_at(offset_t offset, libgs::mutable_buffer buffer)
{
	if( not m_impl )
		return libgs::io_unexpected(std::make_error_code(std::errc::bad_file_descriptor));
	return m_impl->read_some_at(offset, buffer);
}

result_t<block_info> block_device::refresh()
{
	if( not m_impl )
		return libgs::sys_unexpected(std::make_error_code(std::errc::bad_file_descriptor));

	std::error_code error;
	m_impl->refresh(error);

	if( error )
		return libgs::sys_unexpected(error);
	return m_impl->m_info;
}

result_t<block_info> block_device::info() const
{
	if( not is_open() )
		return libgs::sys_unexpected(std::make_error_code(std::errc::bad_file_descriptor));
	return m_impl->m_info;
}

bool block_device::is_open() const noexcept
{
	return m_impl and m_impl->m_descriptor >= 0;
}

} // namespace libempp::storage

#endif //__linux__
