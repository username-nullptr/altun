// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_SERIAL_PORT_BINDING_H
#define ALTUN_LINUX_SERIAL_PORT_BINDING_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/udev/event.h>
#include <altun/linux/udev/enumeration.h>

#include <riwo/utils/signal_slot.h>
#include <riwo/core/async_expected.h>
#include <riwo/core/execution.h>

namespace altun
{

struct serial_port_options
{
	enum baud_rate_t : uint32_t
	{
		baud_rate_4800   = 4800  ,
		baud_rate_9600   = 9600  ,
		baud_rate_19200  = 19200 ,
		baud_rate_38400  = 38400 ,
		baud_rate_57600  = 57600 ,
		baud_rate_115200 = 115200,
		baud_rate_230400 = 230400,
		baud_rate_460800 = 460800
	};
	uint32_t baud_rate = baud_rate_115200;

	enum data_bits_t : uint8_t
	{
		data_bits_5 = 5,
		data_bits_6 = 6,
		data_bits_7 = 7,
		data_bits_8 = 8
	};
	uint8_t data_bits = data_bits_8;

	enum stop_bits_t : uint8_t
	{
		stop_bits_1   = 1,
		stop_bits_2   = 2,
		stop_bits_1p5 = 3
	};
	uint8_t stop_bits = stop_bits_1;

	enum class parity_t : uint8_t {
		none, odd, even
	} parity = parity_t::none;

	enum class flow_control_t : uint8_t {
		none, software, hardware
	} flow_control = flow_control_t::none;
};

template <riwo::concepts::exec Exec = asio::any_io_executor>
class ALTUN_LINUX_TAPI basic_serial_port_binding
{
	RIWO_DISABLE_COPY_MOVE(basic_serial_port_binding)

public:
	using executor_type = Exec;
	using executor_t = executor_type;

	using stream_t = asio::basic_serial_port<executor_t>;
	using stream_ptr = std::shared_ptr<stream_t>;

	using udev_t = udev::enumeration<subsys::enumeration::tty>;
	using options_t = serial_port_options;
	using rules_t = std::map<std::string,riwo::value>;

	explicit basic_serial_port_binding(riwo::concepts::match_sched<Exec> auto &&exec);
	basic_serial_port_binding() requires riwo::concepts::match_def_exec<Exec>;
	~basic_serial_port_binding();

public:
	template <typename...Args>
	using signal_t = riwo::utils::signal<riwo::awaitable<void>(Args...)>;

	class rule_context;
	using rule_context_ptr = std::shared_ptr<rule_context>;

	class io_context;
	using io_context_ptr = std::shared_ptr<io_context>;

	struct device_t
	{
		std::string port;
		device_t(riwo::concepts::string_p<char> auto &&port);
		device_t(const udev_t &dev);
	};

public:
	rule_context_ptr make_rule (
		riwo::concepts::string_p<char> auto &&rule_key,
		riwo::value value, const options_t &options = {}
	);
	rule_context_ptr make_rule(rules_t rules, const options_t &options = {});
	rule_context_ptr make_rule(device_t port, const options_t &options = {});

	[[nodiscard]] executor_t get_executor() noexcept;

public:
	signal_t<std::string_view> opened;
	signal_t<std::string_view,std::error_code> closed;

	signal_t<io_context_ptr> received;
	signal_t<std::string_view,std::error_code> error;

private:
	class impl;
	std::shared_ptr<impl> m_impl {};
};

using serial_port_binding = basic_serial_port_binding<>;

template <riwo::concepts::exec Exec>
class ALTUN_LINUX_TAPI basic_serial_port_binding<Exec>::rule_context :
	public std::enable_shared_from_this<rule_context>
{
	RIWO_DISABLE_COPY_MOVE(rule_context)
	friend class basic_serial_port_binding;
	friend class io_context;

public:
	using executor_type = Exec;
	using executor_t = executor_type;
	using ptr_t = std::shared_ptr<rule_context>;

	rule_context(const executor_t &exec, std::string port, const options_t &options);
	rule_context(const executor_t &exec, rules_t rules, const options_t &options);
	~rule_context();

public:
	ptr_t open();
	ptr_t close();

	template <riwo::concepts::dis_func_tf_opt_token<std::error_code,size_t> Token = riwo::use_sync_t>
	auto write(const std::vector<std::string> &ports, riwo::const_buffer buffer, Token &&token = {});

	template <riwo::concepts::dis_func_tf_opt_token<std::error_code,size_t> Token = riwo::use_sync_t>
	auto write(std::string_view port, riwo::const_buffer buffer, Token &&token = {});

	template <riwo::concepts::dis_func_tf_opt_token<std::error_code,size_t> Token = riwo::use_sync_t>
	auto write(riwo::const_buffer buffer, Token &&token = {});

public:
	[[nodiscard]] std::vector<std::string> ports() const noexcept;
	[[nodiscard]] executor_t get_executor() noexcept;

public:
	signal_t<std::string_view> opened;
	signal_t<std::string_view,std::error_code> closed;

	signal_t<io_context_ptr> received;
	signal_t<std::string_view,std::error_code> error;

private:
	class impl;
	std::shared_ptr<impl> m_impl {};
};

template <riwo::concepts::exec Exec>
class ALTUN_LINUX_TAPI basic_serial_port_binding<Exec>::io_context :
	public std::enable_shared_from_this<io_context>
{
	RIWO_DISABLE_COPY_MOVE(io_context)
	using rule_ptr = std::shared_ptr<typename rule_context::impl>;

public:
	using executor_type = Exec;
	using executor_t = executor_type;
	using payload_t = std::vector<std::byte>;

	io_context(rule_ptr rule, std::string port, stream_ptr stream, payload_t payload);
	~io_context();

	template <riwo::concepts::dis_func_tf_opt_token<std::error_code,size_t> Token = riwo::use_sync_t>
	auto write(riwo::const_buffer buffer, Token &&token = {});

public:
	template <typename T>
	static constexpr bool is_buffer_v =
		riwo::is_vector_buffer_v<T> or riwo::is_string_buffer_v<T>;

	template <typename Buffer = payload_t>
	[[nodiscard]] decltype(auto) payload() const
		noexcept(std::same_as<Buffer,std::string> or std::same_as<Buffer,payload_t>)
		requires is_buffer_v<Buffer>;

	template <typename Buffer = payload_t>
	[[nodiscard]] Buffer take_payload() noexcept(std::same_as<Buffer,payload_t>)
		requires is_buffer_v<Buffer>;

	[[nodiscard]] std::string_view port() const noexcept;
	[[nodiscard]] executor_t get_executor() noexcept;

private:
	class impl;
	std::shared_ptr<impl> m_impl {};
};

} //namespace altun
#include <altun/linux/detail/serial_port_binding.h>

#endif //__linux__
#endif //ALTUN_LINUX_SERIAL_PORT_BINDING_H
