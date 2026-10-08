// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "../../test.h"

#include <libempp/linux/serial_port_binding.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <mutex>
#include <poll.h>
#include <string>
#include <system_error>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std::chrono_literals;

namespace
{

class pseudo_terminal
{
public:
	pseudo_terminal()
	{
		m_master = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
		if(m_master < 0 or ::grantpt(m_master) < 0 or ::unlockpt(m_master) < 0)
			empp_test::fail(std::format("cannot create PTY: {}", std::strerror(errno)));

		std::array<char, 128> slave_name {};
		if(::ptsname_r(m_master, slave_name.data(), slave_name.size()) != 0)
			empp_test::fail(std::format("cannot resolve PTY slave: {}", std::strerror(errno)));
		m_slave = slave_name.data();

		const int slave = ::open(m_slave.c_str(), O_RDWR | O_NOCTTY);
		if(slave < 0)
			empp_test::fail(std::format("cannot open PTY slave: {}", std::strerror(errno)));

		termios attributes {};
		if(::tcgetattr(slave, &attributes) < 0)
		{
			::close(slave);
			empp_test::fail("tcgetattr failed for PTY slave");
		}
		::cfmakeraw(&attributes);
		if(::tcsetattr(slave, TCSANOW, &attributes) < 0)
		{
			::close(slave);
			empp_test::fail("tcsetattr failed for PTY slave");
		}
		::close(slave);
	}

	~pseudo_terminal()
	{
		if(m_master >= 0)
			::close(m_master);
	}

	[[nodiscard]] int master() const noexcept
	{
		return m_master;
	}

	[[nodiscard]] const std::string &slave() const noexcept
	{
		return m_slave;
	}

private:
	int m_master = -1;
	std::string m_slave;
};

class io_runner
{
public:
	explicit io_runner(asio::io_context &context) :
		m_context(context), m_thread([&context] { context.run(); })
	{
	}

	~io_runner()
	{
		m_context.stop();
		if(m_thread.joinable())
			m_thread.join();
	}

private:
	asio::io_context &m_context;
	std::thread m_thread;
};

bool write_all(int descriptor, std::string_view data)
{
	std::size_t offset = 0;
	while(offset < data.size())
	{
		const auto size = ::write(descriptor, data.data() + offset, data.size() - offset);
		if(size > 0)
			offset += static_cast<std::size_t>(size);
		else if(size < 0 and errno == EINTR)
			continue;
		else
			return false;
	}
	return true;
}

std::string read_available(int descriptor, std::size_t expected_size,
	std::chrono::milliseconds timeout)
{
	std::string result;
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	while(result.size() < expected_size)
	{
		const auto now = std::chrono::steady_clock::now();
		if(now >= deadline)
			break;
		const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
		pollfd event {descriptor, POLLIN, 0};
		const int ready = ::poll(&event, 1, static_cast<int>(remaining.count()));
		if(ready < 0 and errno == EINTR)
			continue;
		if(ready <= 0)
			break;

		std::array<char, 256> buffer {};
		const auto size = ::read(descriptor, buffer.data(), buffer.size());
		if(size > 0)
			result.append(buffer.data(), static_cast<std::size_t>(size));
		else if(size < 0 and (errno == EAGAIN or errno == EINTR))
			continue;
		else
			break;
	}
	return result;
}


} // namespace

EMPP_TEST("virtual-device", "serial binding exchanges data over a pseudo terminal")
{
	using binding_type = libempp::basic_serial_port_binding<asio::io_context::executor_type>;
	pseudo_terminal terminal;
	asio::io_context context;
	binding_type binding(context.get_executor());
	std::mutex mutex;
	std::condition_variable changed;
	bool opened = false;
	bool closed = false;
	std::string opened_port;
	std::string received_payload;
	std::string closed_port;
	std::error_code reply_error;
	std::error_code close_error;
	std::size_t reply_size = 0;

	binding.opened.connect([&](std::string_view port) {
		std::scoped_lock lock(mutex);
		opened = true;
		opened_port = port;
		changed.notify_all();
	});
	binding.received.connect([&](const binding_type::io_context_ptr &io) {
		const auto payload = io->take_payload<std::string>();
		std::error_code error;
		const auto size = io->write(riwo::buffer(payload), error);
		std::scoped_lock lock(mutex);
		received_payload.append(payload);
		reply_size += size;
		if(error)
			reply_error = error;
		else if(size != payload.size())
			reply_error = std::make_error_code(std::errc::io_error);
		changed.notify_all();
	});
	binding.closed.connect([&](std::string_view port, const std::error_code &error) {
		std::scoped_lock lock(mutex);
		closed = true;
		closed_port = port;
		close_error = error;
		changed.notify_all();
	});

	auto rule = binding.make_rule(binding_type::device_t(terminal.slave()));
	EMPP_REQUIRE_EQ(rule->ports(), (std::vector<std::string> {terminal.slave()}));
	rule->open();
	io_runner runner(context);

	{
		std::unique_lock lock(mutex);
		EMPP_REQUIRE(changed.wait_for(lock, 2s, [&] { return opened; }));
		EMPP_REQUIRE_EQ(opened_port, terminal.slave());
	}
	std::string inbound(1024, '\0');
	for(std::size_t index = 0; index < inbound.size(); ++index)
		inbound[index] = static_cast<char>('!' + index % 90);
	EMPP_REQUIRE(write_all(terminal.master(), inbound));
	{
		std::unique_lock lock(mutex);
		EMPP_REQUIRE(changed.wait_for(lock, 2s, [&] {
			return received_payload.size() >= inbound.size();
		}));
		EMPP_REQUIRE_EQ(received_payload, inbound);
		EMPP_REQUIRE(not reply_error);
		EMPP_REQUIRE_EQ(reply_size, inbound.size());
	}
	EMPP_REQUIRE_EQ(read_available(terminal.master(), inbound.size(), 2s), inbound);

	std::error_code write_error;
	for(std::size_t index = 0; index < 32; ++index)
	{
		const auto payload = std::format("frame-{:02}", index);
		EMPP_REQUIRE_EQ(rule->write(terminal.slave(), payload, write_error),
			payload.size());
		EMPP_REQUIRE(not write_error);
		EMPP_REQUIRE_EQ(read_available(terminal.master(), payload.size(), 2s),
			payload);
	}
	for(std::size_t index = 0; index < 16; ++index)
	{
		EMPP_REQUIRE_EQ(rule->write("/dev/missing", "x", write_error), 0U);
		EMPP_REQUIRE_EQ(write_error,
			std::make_error_code(std::errc::no_such_device));
	}
	EMPP_REQUIRE_EQ(rule->write(terminal.slave(), "recovered", write_error), 9U);
	EMPP_REQUIRE(not write_error);
	EMPP_REQUIRE_EQ(read_available(terminal.master(), 9, 2s),
		std::string("recovered"));

	asio::post(context, [rule] { rule->close(); });
	{
		std::unique_lock lock(mutex);
		EMPP_REQUIRE(changed.wait_for(lock, 2s, [&] { return closed; }));
		EMPP_REQUIRE_EQ(closed_port, terminal.slave());
		EMPP_REQUIRE(not close_error);
	}
}

