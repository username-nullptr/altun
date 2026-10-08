// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_BUS_I2C_H
#define LIBEMPP_LINUX_BUS_I2C_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/global.h>
#include <riwo/core/async_expected.h>

namespace libempp::bus
{

using i2c_address_t = uint8_t;

enum i2c_reg_bit {
	reg_bit8 = 1, reg_bit16
};
template <i2c_reg_bit RegBit>
using i2c_data_t = riwo::byte_unsigned_t<RegBit>;

template <riwo::concepts::exec Exec = asio::any_io_executor>
class LIBEMPP_LINUX_TAPI basic_i2c
{
	RIWO_DISABLE_COPY(basic_i2c)

public:
	using executor_type = Exec;
	using executor_t = executor_type;

	using handle_t = asio::posix::basic_stream_descriptor<executor_t>;
	using path_t = std::filesystem::path;

	using duration_t = std::chrono::milliseconds;
	using address_t = i2c_address_t;
	using reg_bit_t = i2c_reg_bit;

	template <reg_bit_t RegBit>
	using data_t = i2c_data_t<RegBit>;

	struct attributes_t
	{
		address_t address : 7 = 0;
		duration_t timeout {};
	};
	struct LIBEMPP_LINUX_API node : attributes_t
	{
		path_t dev_name;
		node(path_t dev_name, address_t addr,
			const duration_t &timeout = duration_t(30)
		);
	};

public:
	explicit basic_i2c(riwo::concepts::match_sched<Exec> auto &&exec);
	explicit basic_i2c() requires riwo::concepts::match_def_exec<Exec>;

	explicit basic_i2c(const node &dev, riwo::concepts::match_sched<Exec> auto &&exec);
	explicit basic_i2c(const node &dev) requires riwo::concepts::match_def_exec<Exec>;

	basic_i2c(handle_t &&handle, const attributes_t &attrs);
	~basic_i2c();

	template <riwo::concepts::match_sched<Exec> Exec0>
	basic_i2c(basic_i2c<Exec0> &&other) noexcept;

	template <riwo::concepts::match_sched<Exec> Exec0>
	basic_i2c &operator=(basic_i2c<Exec0> &&other) noexcept;

public:
	void open(const node &dev, std::error_code &error) noexcept;
	void open(const node &dev);

	void close(std::error_code &error) noexcept;
	void close();

public:
	template <reg_bit_t RegBit>
	static constexpr bool is_valid_reg_bit_v =
		RegBit == reg_bit8 or RegBit == reg_bit16;

	template <typename Token, typename Value = size_t>
	static constexpr bool task_token_v =
		riwo::concepts::tf_opt_token<Token,std::error_code,Value>;

	template <typename Token, typename Value = size_t>
	static constexpr bool read_token_v = task_token_v<Token,Value> and
		not riwo::is_detached_v<riwo::token_unbound_t<Token>>;

public:
	template <reg_bit_t RegBit = reg_bit8, typename Token = riwo::use_sync_t>
	auto write(data_t<RegBit> reg, riwo::const_buffer buffer, Token &&token = {})
		requires is_valid_reg_bit_v<RegBit> and task_token_v<Token>;

	template <reg_bit_t RegBit = reg_bit8, typename Token = riwo::use_sync_t>
	auto write(data_t<RegBit> reg, Token &&token = {})
		requires is_valid_reg_bit_v<RegBit> and task_token_v<Token>;

	template <reg_bit_t RegBit = reg_bit8, typename Token = riwo::use_sync_t>
	auto read(data_t<RegBit> reg, riwo::mutable_buffer buffer, Token &&token = {})
		requires is_valid_reg_bit_v<RegBit> and read_token_v<Token>;

	template <riwo::concepts::array_buffer Buffer,
		reg_bit_t RegBit = reg_bit8, typename Token = riwo::use_sync_t>
	auto read(data_t<RegBit> reg, Token &&token = {}) requires
		is_valid_reg_bit_v<RegBit> and read_token_v<Token,Buffer>;

public:
	[[nodiscard]] attributes_t attributes () const noexcept;
	[[nodiscard]] executor_t get_executor() noexcept;
	[[nodiscard]] bool is_open() const noexcept;

	[[nodiscard]] const handle_t &handle() const noexcept;
	[[nodiscard]] handle_t &handle() noexcept;

public:
	[[nodiscard]] static handle_t make_handle (
		const node &dev, riwo::concepts::match_sched<Exec> auto &&exec,
		std::error_code &error
	) noexcept;

	[[nodiscard]] static handle_t make_handle (
		const node &dev, std::error_code &error
	) noexcept requires riwo::concepts::match_def_exec<Exec>;

	template <riwo::concepts::match_sched<Exec> Exec0 = riwo::io_context_t&>
	[[nodiscard]] static handle_t make_handle (
		const node &dev, Exec0 &&exec = riwo::io_context()
	);

private:
	template <riwo::concepts::exec>
	friend class basic_i2c;

	class impl;
	std::shared_ptr<impl> m_impl {};
};

using i2c = basic_i2c<>;

} //namespace libempp::bus
#include <libempp/linux/bus/detail/i2c.h>

#endif //__linux__
#endif //LIBEMPP_LINUX_BUS_I2C_H
