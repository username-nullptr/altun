// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_BUS_DETAIL_I2C_H
#define ALTUN_LINUX_BUS_DETAIL_I2C_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/core/log.h>
#include <riwo/core/utils/byte_order.h>

#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/file.h>

namespace altun::bus
{

template <riwo::concepts::exec Exec>
class ALTUN_LINUX_TAPI basic_i2c<Exec>::impl :
	public std::enable_shared_from_this<impl>
{
	RIWO_DISABLE_COPY_MOVE(impl)

public:
	explicit impl(handle_t &&handle, const attributes_t &attrs) :
		m_attributes(attrs), m_handle(std::move(handle)) {}

	explicit impl(riwo::concepts::match_sched<Exec> auto &&exec) :
		m_handle(riwo::get_executor_helper(std::forward<decltype(exec)>(exec))) {}

public:
	template <i2c_reg_bit RegBit>
	[[nodiscard]] riwo::io_expected write
	(address_t address, data_t<RegBit> reg, const riwo::const_buffer &buffer) noexcept
	{
		if( constexpr auto max_frame_size = std::numeric_limits<uint16_t>::max();
			buffer.size() > max_frame_size - RegBit )
		{
			return riwo::io_unexpected (
				std::make_error_code(std::errc::message_size)
			);
		}
		const auto frame_size = RegBit + buffer.size();
		std::vector<uint8_t> frame;
		try {
			frame.resize(frame_size);
		}
		catch(const std::bad_alloc&)
		{
			return riwo::io_unexpected (
				std::make_error_code(std::errc::not_enough_memory)
			);
		}
		const auto network_reg = riwo::to_big_endian(reg);
		std::memcpy(frame.data(), &network_reg, RegBit);

		if( buffer.size() > 0 )
			std::memcpy(frame.data() + RegBit, buffer.data(), buffer.size());

		i2c_msg message {};
		message.addr = address;
		message.flags = 0;
		message.len = static_cast<uint16_t>(frame.size());
		message.buf = frame.data();

		i2c_rdwr_ioctl_data transfer {
			.msgs = &message, .nmsgs = 1
		};
		return ctrl(transfer, frame_size, "write");
	}

	template <i2c_reg_bit RegBit>
	[[nodiscard]] riwo::io_expected read
	(address_t address, data_t<RegBit> reg, const riwo::mutable_buffer &buffer) noexcept
	{
		if( buffer.size() == 0 )
			return size_t {};
		if( buffer.size() > std::numeric_limits<uint16_t>::max() )
		{
			return riwo::io_unexpected (
				std::make_error_code(std::errc::message_size)
			);
		}
		auto network_reg = riwo::to_big_endian(reg);
		i2c_msg messages[2] {};

		messages[0].addr = address;
		messages[0].flags = 0;
		messages[0].len = RegBit;
		messages[0].buf = reinterpret_cast<uint8_t*>(&network_reg);

		messages[1].addr = address;
		messages[1].flags = I2C_M_RD;
		messages[1].len = static_cast<uint16_t>(buffer.size());
		messages[1].buf = static_cast<uint8_t*>(buffer.data());

		i2c_rdwr_ioctl_data transfer {
			.msgs = messages, .nmsgs = 2
		};
		return ctrl(transfer, buffer.size(), "read");
	}

	template <i2c_reg_bit RegBit, typename Token>
	[[nodiscard]] auto write
	(address_t address, data_t<RegBit> reg, const riwo::const_buffer &buffer, Token &&token)
	{
		using token_t = std::remove_cvref_t<Token>;
		if constexpr( riwo::is_error_code_token_v<Token> )
		{
			return riwo::expected_value_or_error (
				write<RegBit>(address, reg, buffer), token
			);
		}
		else if constexpr( riwo::is_sync_opt_token_v<Token> )
		{
			return riwo::expected_value_or_throw (
				write<RegBit>(address, reg, buffer)
			);
		}
		else if constexpr( riwo::is_detached_v<riwo::token_unbound_t<token_t>> )
		{
			auto owner = copy_write_buffer(buffer);
			return riwo::initiate_io<size_t>(m_handle.get_executor(),
			[self = this->shared_from_this(), address, reg, owner]
			<typename T0>(T0 &&completion_token) mutable
			{
				self->async_execute([self, address, reg, owner]
				{
					return self->template write<RegBit>(address, reg,
						riwo::const_buffer(owner->data(), owner->size())
					);
				},
				std::forward<T0>(completion_token));
			},
			std::forward<Token>(token));
		}
		else
		{
			return riwo::initiate_io<size_t>(m_handle.get_executor(),
			[self = this->shared_from_this(), address, reg, buffer]
			<typename T0>(T0 &&completion_token) mutable
			{
				self->async_execute([self, address, reg, buffer]{
					return self->template write<RegBit>(address, reg, buffer);
				}, std::forward<T0>(completion_token));
			},
			std::forward<Token>(token));
		}
	}

	template <i2c_reg_bit RegBit, typename Token>
	[[nodiscard]] auto read
	(address_t address, data_t<RegBit> reg, const riwo::mutable_buffer &buffer, Token &&token)
	{
		if constexpr( riwo::is_error_code_token_v<Token> )
		{
			return riwo::expected_value_or_error (
				read<RegBit>(address, reg, buffer), token
			);
		}
		else if constexpr( riwo::is_sync_opt_token_v<Token> )
		{
			return riwo::expected_value_or_throw (
				read<RegBit>(address, reg, buffer)
			);
		}
		else
		{
			return riwo::initiate_io<size_t>(m_handle.get_executor(),
			[self = this->shared_from_this(), address, reg, buffer]
			<typename T0>(T0 &&completion_token) mutable
			{
				self->async_execute([self, address, reg, buffer]{
					return self->template read<RegBit>(address, reg, buffer);
				}, std::forward<T0>(completion_token));
			},
			std::forward<Token>(token));
		}
	}

	template <i2c_reg_bit RegBit, riwo::concepts::array_buffer Buffer, typename Token>
	[[nodiscard]] auto async_read_buffer
	(address_t address, data_t<RegBit> reg, Token &&token)
	{
		using token_t = std::remove_cvref_t<Token>;
		token_t completion_token(std::forward<Token>(token));

		return asio::async_initiate<token_t,void(std::error_code,Buffer)>(
		[self = this->shared_from_this(), address, reg]<typename T0>(T0 completion_handler) mutable
		{
			auto allocator = asio::get_associated_allocator(completion_handler);
			using allocator_t = std::allocator_traits
				<decltype(allocator)>::template rebind_alloc<Buffer>;

			auto result = std::allocate_shared<Buffer>(allocator_t(allocator));
			auto executor = asio::get_associated_executor (
				completion_handler, self->m_handle.get_executor()
			);
			auto immediate_executor = asio::get_associated_immediate_executor (
				completion_handler, self->m_handle.get_executor()
			);
			auto slot = asio::get_associated_cancellation_slot(completion_handler);

			auto next_handler = asio::bind_immediate_executor(immediate_executor,
				asio::bind_allocator(allocator, asio::bind_executor(executor, asio::bind_cancellation_slot(slot,
				[result, completion = std::move(completion_handler)](std::error_code error, size_t) mutable {
					std::move(completion)(error, std::move(*result));
				}))
			));
			self->template read<RegBit>(address, reg,
				riwo::buffer(*result), std::move(next_handler)
			);
		},
		completion_token);
	}

private:
	[[nodiscard]] riwo::io_expected ctrl
	(i2c_rdwr_ioctl_data &transfer, size_t transferred_size, std::string_view operation) noexcept
	{
		if( not m_handle.is_open() )
		{
			return riwo::io_unexpected (
				std::make_error_code(std::errc::bad_file_descriptor)
			);
		}
		const auto result = ioctl(m_handle.native_handle(), I2C_RDWR, &transfer);
		if( result == static_cast<int>(transfer.nmsgs) )
			return transferred_size;

		std::error_code error;
		if( result < 0 )
			error = std::error_code(errno, std::system_category());
		else
			error = std::make_error_code(std::errc::io_error);

		altun_log_warning("Altun.Linux",
			"i2c::{}: ioctl(I2C_RDWR) failed: {}", operation, error
		);
		return riwo::io_unexpected(error);
	}

	[[nodiscard]] static std::shared_ptr<std::vector<std::byte>>
	copy_write_buffer(const riwo::const_buffer &buffer)
	{
		auto owner = std::make_shared<std::vector<std::byte>>(buffer.size());
		if( buffer.size() > 0 )
			std::memcpy(owner->data(), buffer.data(), buffer.size());
		return owner;
	}

	template <typename Operation, typename Handler>
	void async_execute(Operation operation, Handler &&handler)
	{
		auto allocator = asio::get_associated_allocator(handler);
		auto io_work = asio::make_work_guard(m_handle.get_executor());

		auto completion_work = asio::make_work_guard(handler, m_handle.get_executor());
		auto completion_exec = completion_work.get_executor();

		asio::post(m_handle.get_executor(), asio::bind_allocator(allocator, [
			operation = std::move(operation), allocator, completion_exec, io_work = std::move(io_work),
			completion_work = std::move(completion_work), completion = std::forward<Handler>(handler)
		]() mutable
		{
			RIWO_UNUSED(io_work);
			std::error_code error;
			size_t transferred = 0;
			try {
				auto result = operation();
				if( result )
					transferred = *result;
				else
					error = result.error();
			}
			catch(...) {
				error = riwo::exception_error(std::current_exception());
			}
			asio::dispatch(completion_exec, asio::bind_allocator(allocator, [
				completion_work = std::move(completion_work),
				completion = std::move(completion), error, transferred
			]() mutable
			{
				RIWO_UNUSED(completion_work);
				std::move(completion)(error, transferred);
			}));
		}));
	}

public:
	attributes_t m_attributes {};
	handle_t m_handle;
};

template <riwo::concepts::exec Exec>
basic_i2c<Exec>::node::node(path_t dev_name, address_t addr,
	const duration_t &timeout) :
	dev_name(std::move(dev_name))
{
	this->address = addr;
	this->timeout = timeout;
}

template <riwo::concepts::exec Exec>
basic_i2c<Exec>::basic_i2c(riwo::concepts::match_sched<Exec> auto &&exec) :
	m_impl(std::make_shared<impl>(std::forward<decltype(exec)>(exec)))
{

}

template <riwo::concepts::exec Exec>
basic_i2c<Exec>::basic_i2c()
	requires riwo::concepts::match_def_exec<Exec> :
	basic_i2c(riwo::io_context())
{

}

template <riwo::concepts::exec Exec>
basic_i2c<Exec>::basic_i2c(const node &dev, riwo::concepts::match_sched<Exec> auto &&exec) :
	basic_i2c(make_handle(dev, std::forward<decltype(exec)>(exec)),
		static_cast<const attributes_t&>(dev)
	)
{

}

template <riwo::concepts::exec Exec>
basic_i2c<Exec>::basic_i2c(const node &dev)
	requires riwo::concepts::match_def_exec<Exec> :
	basic_i2c(dev, riwo::io_context())
{

}

template <riwo::concepts::exec Exec>
basic_i2c<Exec>::basic_i2c(handle_t &&handle, const attributes_t &attrs) :
	m_impl(std::make_shared<impl>(std::move(handle), attrs))
{

}

template <riwo::concepts::exec Exec>
basic_i2c<Exec>::~basic_i2c() = default;

template <riwo::concepts::exec Exec>
template <riwo::concepts::match_sched<Exec> Exec0>
basic_i2c<Exec>::basic_i2c(basic_i2c<Exec0> &&other) noexcept
{
	if constexpr( std::same_as<Exec,Exec0> )
	{
		auto source_exec = other.get_executor();
		m_impl = std::move(other.m_impl);
		other.m_impl = std::make_shared<typename basic_i2c<Exec0>::impl>(source_exec);
	}
	else
	{
		const auto other_attrs = other.attributes();
		attributes_t attrs {};

		attrs.address = other_attrs.address;
		attrs.timeout = other_attrs.timeout;

		m_impl = std::make_shared<impl>(
			handle_t(std::move(other.handle())), attrs
		);
	}
}

template <riwo::concepts::exec Exec>
template <riwo::concepts::match_sched<Exec> Exec0>
basic_i2c<Exec> &basic_i2c<Exec>::operator=(basic_i2c<Exec0> &&other) noexcept
{
	if constexpr( std::same_as<Exec,Exec0> )
	{
		if( this == &other )
			return *this;

		auto source_exec = other.get_executor();
		m_impl = std::move(other.m_impl);

		other.m_impl = std::make_shared
			<typename basic_i2c<Exec0>::impl>(source_exec);
	}
	else
	{
		const auto other_attrs = other.attributes();
		attributes_t attrs {};

		attrs.address = other_attrs.address;
		attrs.timeout = other_attrs.timeout;

		m_impl = std::make_shared<impl>(
			handle_t(std::move(other.handle())), attrs
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
void basic_i2c<Exec>::open(const node &dev, std::error_code &error) noexcept
{
	error.clear();
	if( is_open() )
	{
		close(error);
		if( error )
			return ;
	}
	auto new_handle = make_handle(dev, get_executor(), error);
	if( error )
		return ;

	m_impl->m_handle = std::move(new_handle);
	m_impl->m_attributes = static_cast<const attributes_t&>(dev);
}

template <riwo::concepts::exec Exec>
void basic_i2c<Exec>::open(const node &dev)
{
	std::error_code error;
	open(dev, error);
	if( error )
	{
		riwo::system_error::loc_throw(error, std::format (
			"altun::i2c::open('{}')", dev.dev_name.string()
		));
	}
}

template <riwo::concepts::exec Exec>
void basic_i2c<Exec>::close(std::error_code &error) noexcept
{
	error.clear();
	if( m_impl->m_handle.is_open() )
		m_impl->m_handle.close(error);
}

template <riwo::concepts::exec Exec>
void basic_i2c<Exec>::close()
{
	std::error_code error;
	close(error);
	if( error )
		riwo::system_error::loc_throw(error, "altun::i2c::close");
}

template <riwo::concepts::exec Exec>
template <i2c_reg_bit RegBit, typename Token>
auto basic_i2c<Exec>::write(data_t<RegBit> reg, riwo::const_buffer buffer, Token &&token)
	requires is_valid_reg_bit_v<RegBit> and task_token_v<Token>
{
	return m_impl->template write<RegBit>(
		attributes().address, reg, buffer, std::forward<Token>(token)
	);
}

template <riwo::concepts::exec Exec>
template <i2c_reg_bit RegBit, typename Token>
auto basic_i2c<Exec>::write(data_t<RegBit> reg, Token &&token)
	requires is_valid_reg_bit_v<RegBit> and task_token_v<Token>
{
	return write<RegBit>(reg, riwo::const_buffer{}, std::forward<Token>(token));
}

template <riwo::concepts::exec Exec>
template <i2c_reg_bit RegBit, typename Token>
auto basic_i2c<Exec>::read(data_t<RegBit> reg, riwo::mutable_buffer buffer, Token &&token)
	requires is_valid_reg_bit_v<RegBit> and read_token_v<Token>
{
	return m_impl->template read<RegBit>(
		attributes().address, reg, buffer, std::forward<Token>(token)
	);
}

template <riwo::concepts::exec Exec>
template <riwo::concepts::array_buffer Buffer, i2c_reg_bit RegBit, typename Token>
auto basic_i2c<Exec>::read(data_t<RegBit> reg, Token &&token) requires
	is_valid_reg_bit_v<RegBit> and read_token_v<Token,Buffer>
{
	if constexpr( riwo::is_sync_opt_token_v<Token> )
	{
		Buffer result {};
		RIWO_UNUSED(read<RegBit>(reg, riwo::buffer(result),
			std::forward<Token>(token)
		));
		return result;
	}
	else
	{
		return riwo::initiate_io<Buffer>(get_executor(),
		[implementation = m_impl, address = attributes().address, reg]
		<typename T0>(T0 &&completion_token) mutable
		{
			return implementation->template async_read_buffer<RegBit,Buffer>(
				address, reg, std::forward<T0>(completion_token)
			);
		},
		std::forward<Token>(token));
	}
}

template <riwo::concepts::exec Exec>
auto basic_i2c<Exec>::attributes() const noexcept -> attributes_t
{
	return m_impl->m_attributes;
}

template <riwo::concepts::exec Exec>
auto basic_i2c<Exec>::get_executor() noexcept -> executor_t
{
	return m_impl->m_handle.get_executor();
}

template <riwo::concepts::exec Exec>
bool basic_i2c<Exec>::is_open() const noexcept
{
	return m_impl->m_handle.is_open();
}

template <riwo::concepts::exec Exec>
auto basic_i2c<Exec>::handle() const noexcept -> const handle_t&
{
	return m_impl->m_handle;
}

template <riwo::concepts::exec Exec>
auto basic_i2c<Exec>::handle() noexcept -> handle_t&
{
	return m_impl->m_handle;
}

template <riwo::concepts::exec Exec>
auto basic_i2c<Exec>::make_handle(const node &dev,
	riwo::concepts::match_sched<Exec> auto &&exec, std::error_code &error) noexcept -> handle_t
{
	handle_t stream(riwo::get_executor_helper (
		std::forward<decltype(exec)>(exec)
	));
	error.clear();

	if( dev.timeout < duration_t::zero() )
		error = std::make_error_code(std::errc::invalid_argument);
	else
	{
		const auto timeout_ms = dev.timeout.count();
		const auto timeout_units = static_cast<unsigned long>(
			timeout_ms / 10 + (timeout_ms % 10 != 0)
		);
		int fd = ::open(dev.dev_name.string().c_str(), O_RDWR | O_CLOEXEC);
		do {
			if( fd < 0 )
			{
				error = std::error_code(errno, std::system_category());
				break;
			}
			if( flock(fd, LOCK_EX | LOCK_NB) < 0 )
			{
				error = std::error_code(errno, std::system_category());
				break;
			}
			if( ioctl(fd, I2C_TIMEOUT, timeout_units) < 0 )
			{
				error = std::error_code(errno, std::system_category());
				break;
			}
			if( ioctl(fd, I2C_SLAVE, static_cast<unsigned long>(dev.address)) < 0 )
			{
				error = std::error_code(errno, std::system_category());
				break;
			}
			error = stream.assign(fd, error);
			if( error )
				break;
			return stream;
		}
		while(false);

		if( fd >= 0 )
			::close(fd);
	}
	altun_log_warning("Altun.Linux",
		"basic_i2c<Exec>::make_handle: '{}': {}", dev.dev_name.string(), error
	);
	return stream;
}

template <riwo::concepts::exec Exec>
auto basic_i2c<Exec>::make_handle(const node &dev, std::error_code &error) noexcept -> handle_t
	requires riwo::concepts::match_def_exec<Exec>
{
	return make_handle(dev, riwo::io_context(), error);
}

template <riwo::concepts::exec Exec>
template <riwo::concepts::match_sched<Exec> Exec0>
auto basic_i2c<Exec>::make_handle(const node &dev, Exec0 &&exec) -> handle_t
{
	std::error_code error;
	auto stream = make_handle(dev, std::forward<Exec0>(exec), error);
	if( error )
		riwo::system_error::loc_throw(error, "altun::i2c::make_handle");
	return stream;
}

} //namespace altun::bus

#endif //__linux__
#endif //ALTUN_LINUX_BUS_DETAIL_I2C_H
