// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_BUS_SPI_H
#define ALTUN_LINUX_BUS_SPI_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/global.h>
#include <riwo/core/async_expected.h>
#include <linux/spi/spidev.h>

namespace altun::bus
{

using spi_mode_t = uint32_t;

enum spi_mode : spi_mode_t
{
	spi_mode0 = SPI_MODE_0,
	spi_mode1 = SPI_MODE_1,
	spi_mode2 = SPI_MODE_2,
	spi_mode3 = SPI_MODE_3
};

template <riwo::concepts::exec Exec = asio::any_io_executor>
class ALTUN_LINUX_TAPI basic_spi
{
	RIWO_DISABLE_COPY(basic_spi)

public:
	using executor_type = Exec;
	using executor_t = executor_type;

	using handle_t = asio::posix::basic_stream_descriptor<executor_t>;
	using path_t = std::filesystem::path;

	using duration_t = std::chrono::microseconds;
	using mode_t = spi_mode_t;
	using speed_t = uint32_t;
	using bits_t = uint8_t;

	struct attributes_t
	{
		mode_t mode = spi_mode0;
		speed_t max_speed_hz = 500'000;
		bits_t bits_per_word = 8;
		duration_t delay {};
		bool cs_change = false;
	};
	struct ALTUN_LINUX_API node : attributes_t
	{
		path_t dev_name;
		node(path_t dev_name, speed_t max_speed_hz = 500'000,
			mode_t mode = spi_mode0, bits_t bits_per_word = 8,
			const duration_t &delay = duration_t::zero(), bool cs_change = false
		);
	};

public:
	explicit basic_spi(riwo::concepts::match_sched<Exec> auto &&exec);
	explicit basic_spi() requires riwo::concepts::match_def_exec<Exec>;

	explicit basic_spi(const node &dev, riwo::concepts::match_sched<Exec> auto &&exec);
	explicit basic_spi(const node &dev) requires riwo::concepts::match_def_exec<Exec>;

	basic_spi(handle_t &&handle, const attributes_t &attrs);
	~basic_spi();

	template <riwo::concepts::match_sched<Exec> Exec0>
	basic_spi(basic_spi<Exec0> &&other) noexcept;

	template <riwo::concepts::match_sched<Exec> Exec0>
	basic_spi &operator=(basic_spi<Exec0> &&other) noexcept;

public:
	template <typename Error>
	void open(const node &dev, Error &error) noexcept
		requires riwo::is_error_code_token_v<Error&>;

	template <typename Error>
	void close(Error &error) noexcept
		requires riwo::is_error_code_token_v<Error&>;

	void open(const node &dev);
	void close();

public:
	template <typename Token, typename Value = size_t>
	static constexpr bool task_token_v =
		riwo::concepts::tf_opt_token<Token,riwo::error_code,Value>;

	template <typename Token, typename Value = size_t>
	static constexpr bool read_token_v = task_token_v<Token,Value> and
		not riwo::is_detached_v<riwo::token_unbound_t<Token>>;

public:
	template <typename Token = riwo::use_sync_t>
	auto transfer(riwo::const_buffer tx_buffer, riwo::mutable_buffer rx_buffer,
		Token &&token = {}) requires read_token_v<Token>;

	template <typename Token = riwo::use_sync_t>
	auto write(riwo::const_buffer buffer, Token &&token = {})
		requires task_token_v<Token>;

	template <typename Token = riwo::use_sync_t>
	auto read(riwo::mutable_buffer buffer, Token &&token = {})
		requires read_token_v<Token>;

	template <riwo::concepts::array_buffer Buffer, typename Token = riwo::use_sync_t>
	auto read(Token &&token = {}) requires read_token_v<Token,Buffer>;

public:
	[[nodiscard]] attributes_t attributes() const noexcept;
	[[nodiscard]] executor_t get_executor() noexcept;
	[[nodiscard]] bool is_open() const noexcept;

	[[nodiscard]] const handle_t &handle() const noexcept;
	[[nodiscard]] handle_t &handle() noexcept;

public:
	template <typename Error>
	[[nodiscard]] static handle_t make_handle (
		const node &dev, riwo::concepts::match_sched<Exec> auto &&exec, Error &error
	) noexcept requires riwo::is_error_code_token_v<Error&>;

	template <typename Error>
	[[nodiscard]] static handle_t make_handle(const node &dev, Error &error) noexcept
		requires (riwo::concepts::match_def_exec<Exec> and riwo::is_error_code_token_v<Error&>);

	template <riwo::concepts::match_sched<Exec> Exec0 = riwo::io_context_t&>
	[[nodiscard]] static handle_t make_handle (
		const node &dev, Exec0 &&exec = riwo::io_context()
	);

private:
	template <riwo::concepts::exec>
	friend class basic_spi;

	class impl;
	std::shared_ptr<impl> m_impl {};
};

using spi = basic_spi<>;

} // namespace altun::bus
#include <altun/linux/bus/detail/spi.h>

#endif //__linux__
#endif // ALTUN_LINUX_BUS_SPI_H
