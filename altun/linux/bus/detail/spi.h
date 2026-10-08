// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_BUS_DETAIL_SPI_H
#define ALTUN_LINUX_BUS_DETAIL_SPI_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/core/log.h>
#include <sys/file.h>

namespace altun::bus
{

template <riwo::concepts::exec Exec>
class ALTUN_LINUX_TAPI basic_spi<Exec>::impl :
	public std::enable_shared_from_this<impl>
{
	RIWO_DISABLE_COPY_MOVE(impl)

public:
	explicit impl(handle_t &&handle, const attributes_t &attrs) :
		m_attributes(attrs), m_handle(std::move(handle)) {}

	explicit impl(riwo::concepts::match_sched<Exec> auto &&exec) :
		m_handle(riwo::get_executor_helper(std::forward<decltype(exec)>(exec))) {}

public:
	[[nodiscard]] riwo::io_expected transfer
	(const riwo::const_buffer &tx_buffer, const riwo::mutable_buffer &rx_buffer) noexcept
	{
		if( tx_buffer.size() != rx_buffer.size() )
		{
			return riwo::io_unexpected (
				std::make_error_code(std::errc::invalid_argument)
			);
		}
		return ctrl(tx_buffer, rx_buffer, "transfer");
	}

	[[nodiscard]] riwo::io_expected write(const riwo::const_buffer &buffer) noexcept {
		return ctrl(buffer, riwo::mutable_buffer{}, "write");
	}

	[[nodiscard]] riwo::io_expected read(const riwo::mutable_buffer &buffer) noexcept
	{
		if( buffer.size() == 0 )
			return size_t {};
		return ctrl(riwo::const_buffer{}, buffer, "read");
	}

	template <typename Token>
	[[nodiscard]] auto transfer
	(const riwo::const_buffer &tx_buffer, const riwo::mutable_buffer &rx_buffer, Token &&token)
	{
		if constexpr(riwo::is_error_code_token_v<Token>)
		{
			return riwo::expected_value_or_error (
				transfer(tx_buffer, rx_buffer), token
			);
		}
		else if constexpr(riwo::is_sync_opt_token_v<Token>)
			return riwo::expected_value_or_throw(transfer(tx_buffer, rx_buffer));
		else
		{
			return riwo::initiate_io<size_t>(m_handle.get_executor(),
			[self = this->shared_from_this(), tx_buffer, rx_buffer]
			<typename T0>(T0 &&completion_token) mutable
			{
				self->async_execute([self, tx_buffer, rx_buffer] {
					return self->transfer(tx_buffer, rx_buffer);
				}, std::forward<T0>(completion_token));
			},
			std::forward<Token>(token));
		}
	}

	template <typename Token>
	[[nodiscard]] auto write(const riwo::const_buffer &buffer, Token &&token)
	{
		using token_t = std::remove_cvref_t<Token>;
		if constexpr(riwo::is_error_code_token_v<Token>)
			return riwo::expected_value_or_error(write(buffer), token);

		else if constexpr(riwo::is_sync_opt_token_v<Token>)
			return riwo::expected_value_or_throw(write(buffer));

		else if constexpr(riwo::is_detached_v<riwo::token_unbound_t<token_t>>)
		{
			auto owner = copy_write_buffer(buffer);
			return riwo::initiate_io<size_t>(m_handle.get_executor(),
			[self = this->shared_from_this(), owner]<typename T0>(T0 &&completion_token) mutable
			{
				self->async_execute([self, owner]
				{
					return self->write (
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
			[self = this->shared_from_this(), buffer]<typename T0>(T0 &&completion_token) mutable
			{
				self->async_execute([self, buffer] {
					return self->write(buffer);
				}, std::forward<T0>(completion_token));
			},
			std::forward<Token>(token));
		}
	}

	template <typename Token>
	[[nodiscard]] auto read(const riwo::mutable_buffer &buffer, Token &&token)
	{
		if constexpr(riwo::is_error_code_token_v<Token>)
			return riwo::expected_value_or_error(read(buffer), token);

		else if constexpr(riwo::is_sync_opt_token_v<Token>)
			return riwo::expected_value_or_throw(read(buffer));

		else
		{
			return riwo::initiate_io<size_t>(m_handle.get_executor(),
			[self = this->shared_from_this(), buffer]<typename T0>(T0 &&completion_token) mutable
			{
				self->async_execute([self, buffer] {
					return self->read(buffer);
				}, std::forward<T0>(completion_token));
			},
			std::forward<Token>(token));
		}
	}

	template <riwo::concepts::array_buffer Buffer, typename Token>
	[[nodiscard]] auto async_read_buffer(Token &&token)
	{
		using token_t = std::remove_cvref_t<Token>;
		token_t completion_token(std::forward<Token>(token));

		return asio::async_initiate<token_t,void(riwo::error_code,Buffer)>(
		[self = this->shared_from_this()]<typename T0>(T0 completion_handler) mutable
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
				[result, completion = std::move(completion_handler)](riwo::error_code error, size_t) mutable {
					std::move(completion)(error, std::move(*result));
				}))
			));
			self->read(riwo::buffer(*result), std::move(next_handler));
		},
		completion_token);
	}

private:
	[[nodiscard]] riwo::io_expected ctrl(const riwo::const_buffer &tx_buffer,
		const riwo::mutable_buffer &rx_buffer, std::string_view operation) noexcept
	{
		const auto size = std::max(tx_buffer.size(), rx_buffer.size());
		if( size > std::numeric_limits<uint32_t>::max() )
		{
			return riwo::io_unexpected (
				std::make_error_code(std::errc::message_size)
			);
		}
		if( not m_handle.is_open() )
		{
			return riwo::io_unexpected (
				std::make_error_code(std::errc::bad_file_descriptor)
			);
		}
		spi_ioc_transfer transfer {};
		transfer.tx_buf = static_cast<__u64>(
			reinterpret_cast<std::uintptr_t>(tx_buffer.data())
		);
		transfer.rx_buf = static_cast<__u64>(
			reinterpret_cast<std::uintptr_t>(rx_buffer.data())
		);
		transfer.len = static_cast<__u32>(size);
		transfer.speed_hz = m_attributes.max_speed_hz;
		transfer.delay_usecs = static_cast<__u16>(m_attributes.delay.count());
		transfer.bits_per_word = m_attributes.bits_per_word;
		transfer.cs_change = m_attributes.cs_change;

		const auto result = ioctl(m_handle.native_handle(), SPI_IOC_MESSAGE(1), &transfer);
		if( result >= 0 and static_cast<size_t>(result) == size )
			return size;

		riwo::error_code error;
		if( result < 0 )
			error = riwo::error_code(errno, riwo::system_category());
		else
			error = riwo::make_system_error_code(std::errc::io_error);

		altun_log_warning("Altun.Linux",
			"spi::{}: ioctl(SPI_IOC_MESSAGE) failed: {}", operation, error
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
			riwo::error_code error;
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
basic_spi<Exec>::node::node(path_t dev_name, speed_t max_speed_hz,
	mode_t mode, bits_t bits_per_word, const duration_t &delay, bool cs_change) :
	dev_name(std::move(dev_name))
{
	this->mode = mode;
	this->max_speed_hz = max_speed_hz;
	this->bits_per_word = bits_per_word;
	this->delay = delay;
	this->cs_change = cs_change;
}

template <riwo::concepts::exec Exec>
basic_spi<Exec>::basic_spi(riwo::concepts::match_sched<Exec> auto &&exec) :
	m_impl(std::make_shared<impl>(std::forward<decltype(exec)>(exec)))
{

}

template <riwo::concepts::exec Exec>
basic_spi<Exec>::basic_spi()
	requires riwo::concepts::match_def_exec<Exec> :
	basic_spi(riwo::io_context())
{

}

template <riwo::concepts::exec Exec>
basic_spi<Exec>::basic_spi(const node &dev, riwo::concepts::match_sched<Exec> auto &&exec) :
	basic_spi(make_handle(dev, std::forward<decltype(exec)>(exec)), static_cast<const attributes_t&>(dev))
{

}

template <riwo::concepts::exec Exec>
basic_spi<Exec>::basic_spi(const node &dev)
	requires riwo::concepts::match_def_exec<Exec> :
	basic_spi(dev, riwo::io_context())
{

}

template <riwo::concepts::exec Exec>
basic_spi<Exec>::basic_spi(handle_t &&handle, const attributes_t &attrs) :
	m_impl(std::make_shared<impl>(std::move(handle), attrs))
{

}

template <riwo::concepts::exec Exec>
basic_spi<Exec>::~basic_spi() = default;

template <riwo::concepts::exec Exec>
template <riwo::concepts::match_sched<Exec> Exec0>
basic_spi<Exec>::basic_spi(basic_spi<Exec0> &&other) noexcept
{
	if constexpr(std::same_as<Exec,Exec0>)
	{
		auto source_exec = other.get_executor();
		m_impl = std::move(other.m_impl);
		other.m_impl = std::make_shared<typename basic_spi<Exec0>::impl>(source_exec);
	}
	else
	{
		m_impl = std::make_shared<impl>(
			handle_t(std::move(other.handle())), other.attributes()
		);
	}
}

template <riwo::concepts::exec Exec>
template <riwo::concepts::match_sched<Exec> Exec0>
basic_spi<Exec> &basic_spi<Exec>::operator=(basic_spi<Exec0> &&other) noexcept
{
	if constexpr(std::same_as<Exec,Exec0>)
	{
		if( this == &other )
			return *this;

		auto source_exec = other.get_executor();
		m_impl = std::move(other.m_impl);

		other.m_impl = std::make_shared
			<typename basic_spi<Exec0>::impl>(source_exec);
	}
	else
	{
		m_impl = std::make_shared<impl>(
			handle_t(std::move(other.handle())), other.attributes()
		);
	}
	return *this;
}

template <riwo::concepts::exec Exec>
template <typename Error>
void basic_spi<Exec>::open(const node &dev, Error &error) noexcept
	requires riwo::is_error_code_token_v<Error&>
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
template <typename Error>
void basic_spi<Exec>::close(Error &error) noexcept
	requires riwo::is_error_code_token_v<Error&>
{
	error.clear();
	if( m_impl->m_handle.is_open() )
	{
		auto adapted_error = riwo::adapt_error_code(error);
		m_impl->m_handle.close(adapted_error.get());
	}
}

template <riwo::concepts::exec Exec>
void basic_spi<Exec>::open(const node &dev)
{
	std::error_code error;
	open(dev, error);
	if( error )
	{
		riwo::system_error::loc_throw(error, std::format (
			"altun::spi::open('{}')", dev.dev_name.string()
		));
	}
}

template <riwo::concepts::exec Exec>
void basic_spi<Exec>::close()
{
	std::error_code error;
	close(error);
	if( error )
		riwo::system_error::loc_throw(error, "altun::spi::close");
}

template <riwo::concepts::exec Exec>
template <typename Token>
auto basic_spi<Exec>::transfer
(riwo::const_buffer tx_buffer, riwo::mutable_buffer rx_buffer, Token &&token)
	requires read_token_v<Token>
{
	return m_impl->transfer(tx_buffer, rx_buffer, std::forward<Token>(token));
}

template <riwo::concepts::exec Exec>
template <typename Token>
auto basic_spi<Exec>::write(riwo::const_buffer buffer, Token &&token)
	requires task_token_v<Token>
{
	return m_impl->write(buffer, std::forward<Token>(token));
}

template <riwo::concepts::exec Exec>
template <typename Token>
auto basic_spi<Exec>::read(riwo::mutable_buffer buffer, Token &&token)
	requires read_token_v<Token>
{
	return m_impl->read(buffer, std::forward<Token>(token));
}

template <riwo::concepts::exec Exec>
template <riwo::concepts::array_buffer Buffer, typename Token>
auto basic_spi<Exec>::read(Token &&token) requires read_token_v<Token,Buffer>
{
	if constexpr( riwo::is_sync_opt_token_v<Token> )
	{
		Buffer result {};
		RIWO_UNUSED(read(riwo::buffer(result), std::forward<Token>(token)));
		return result;
	}
	else
	{
		return riwo::initiate_io<Buffer>(get_executor(),
		[implementation = m_impl]<typename T0>(T0 &&completion_token) mutable
		{
			return implementation->template async_read_buffer<Buffer>(
				std::forward<T0>(completion_token)
			);
		},
		std::forward<Token>(token));
	}
}

template <riwo::concepts::exec Exec>
auto basic_spi<Exec>::attributes() const noexcept -> attributes_t
{
	return m_impl->m_attributes;
}

template <riwo::concepts::exec Exec>
auto basic_spi<Exec>::get_executor() noexcept -> executor_t
{
	return m_impl->m_handle.get_executor();
}

template <riwo::concepts::exec Exec>
bool basic_spi<Exec>::is_open() const noexcept
{
	return m_impl->m_handle.is_open();
}

template <riwo::concepts::exec Exec>
auto basic_spi<Exec>::handle() const noexcept -> const handle_t&
{
	return m_impl->m_handle;
}

template <riwo::concepts::exec Exec>
auto basic_spi<Exec>::handle() noexcept -> handle_t&
{
	return m_impl->m_handle;
}

template <riwo::concepts::exec Exec>
template <typename Error>
auto basic_spi<Exec>::make_handle
(const node &dev, riwo::concepts::match_sched<Exec> auto &&exec, Error &error) noexcept -> handle_t
	requires riwo::is_error_code_token_v<Error&>
{
	handle_t stream(riwo::get_executor_helper (
		std::forward<decltype(exec)>(exec)
	));
	error.clear();
	const auto delay_count = dev.delay.count();

	if( dev.max_speed_hz == 0 or dev.bits_per_word == 0 or
		delay_count < 0 or delay_count > std::numeric_limits<uint16_t>::max() )
		error = std::make_error_code(std::errc::invalid_argument);
	else
	{
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
			auto mode = dev.mode;
			auto bits_per_word = dev.bits_per_word;
			auto max_speed_hz = dev.max_speed_hz;

			if( ioctl(fd, SPI_IOC_WR_MODE32, &mode) < 0 or
				ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits_per_word) < 0 or
				ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &max_speed_hz) < 0 )
			{
				error = std::error_code(errno, std::system_category());
				break;
			}
			{
				auto adapted_error = riwo::adapt_error_code(error);
				stream.assign(fd, adapted_error.get());
			}
			if( error )
				break;
			return stream;
		}
		while(false);

		if( fd >= 0 )
			::close(fd);
	}
	altun_log_warning("Altun.Linux",
		"basic_spi<Exec>::make_handle: '{}': {}", dev.dev_name.string(),
		riwo::error_code(error)
	);
	return stream;
}

template <riwo::concepts::exec Exec>
template <typename Error>
auto basic_spi<Exec>::make_handle(const node &dev, Error &error) noexcept -> handle_t
	requires (riwo::concepts::match_def_exec<Exec> and riwo::is_error_code_token_v<Error&>)
{
	return make_handle(dev, riwo::io_context(), error);
}

template <riwo::concepts::exec Exec>
template <riwo::concepts::match_sched<Exec> Exec0>
auto basic_spi<Exec>::make_handle(const node &dev, Exec0 &&exec) -> handle_t
{
	std::error_code error;
	auto stream = make_handle(dev, std::forward<Exec0>(exec), error);
	if( error )
		riwo::system_error::loc_throw(error, "altun::spi::make_handle");
	return stream;
}

} // namespace altun::bus

#endif //__linux__
#endif // ALTUN_LINUX_BUS_DETAIL_SPI_H
