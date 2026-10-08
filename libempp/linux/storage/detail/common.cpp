// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "common.h"
#ifdef __linux__

#include <libempp/linux/storage/block_device.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <spawn.h>
#include <csignal>
#include <thread>

namespace libempp::storage::detail { namespace
{

class RIWO_DECL_HIDDEN file_descriptor
{
public:
	explicit file_descriptor(int descriptor = -1) noexcept :
		m_descriptor(descriptor) {}

	~file_descriptor()
	{
		if( m_descriptor >= 0 )
			::close(m_descriptor);
	}

	file_descriptor(const file_descriptor&) = delete;
	file_descriptor &operator=(const file_descriptor&) = delete;

	[[nodiscard]] int get() const noexcept {
		return m_descriptor;
	}

private:
	int m_descriptor = -1;
};

void check_spawn_action(int result, std::string_view operation)
{
	if( result != 0 )
		throw_error(std::error_code(result, std::generic_category()), operation);
}

} // namespace

[[noreturn]] void throw_error(const std::error_code &error, std::string_view operation)
{
	riwo::system_error::loc_throw(error, operation);
	std::terminate();
}

[[noreturn]] void throw_errno(std::string_view operation)
{
	throw_error(std::error_code (
		errno != 0 ? errno : EIO, std::system_category()), operation
	);
}

void ensure_path(const path_t &path, std::string_view operation)
{
	const auto &native = path.native();
	if( native.empty() or native.front() == '-' or native.find('\0') != std::string::npos )
		throw_error(std::make_error_code(std::errc::invalid_argument), operation);
}

void ensure_string(const std::string &value, std::string_view operation)
{
	if( value.find('\0') != std::string::npos )
		throw_error(std::make_error_code(std::errc::invalid_argument), operation);
}

void ensure_device(const device_info &device, std::string_view operation)
{
	ensure_path(device.device, operation);
	auto current = resolve_device(device.device);

	if( not current )
		throw_error(current.error(), operation);

	if( current->id != device.id or
		(not device.sys_path.empty() and current->sys_path != device.sys_path) or
		(not device.serial.empty() and current->serial != device.serial) )
		throw_error(make_error_code(errc::device_changed), operation);
}

void run_command
(std::vector<std::string> arguments, std::string_view standard_input, std::chrono::milliseconds timeout)
{
	constexpr std::string_view operation =
		"libempp::storage: execute helper";

	if( arguments.empty() or arguments.front().empty() )
		throw_error(std::make_error_code(std::errc::invalid_argument), operation);

	if( timeout <= std::chrono::milliseconds::zero() )
		throw_error(std::make_error_code(std::errc::invalid_argument), operation);

	for(const auto &argument : arguments)
		ensure_string(argument, operation);

	struct file_closer
	{
		void operator()(std::FILE *file) const noexcept
		{
			if( file )
				static_cast<void>(std::fclose(file));
		}
	};
	using file_ptr = std::unique_ptr<std::FILE, file_closer>;
	file_ptr input(std::tmpfile());

	if( not input )
		throw_errno(operation);

	if( not standard_input.empty() )
	{
		if( std::fwrite(standard_input.data(), 1, standard_input.size(), input.get()) != standard_input.size() or
			std::fflush(input.get()) != 0 or std::fseek(input.get(), 0, SEEK_SET) != 0 )
			throw_errno(operation);
	}
	file_descriptor null_output(::open("/dev/null", O_WRONLY | O_CLOEXEC));
	if( null_output.get() < 0 )
		throw_errno(operation);

	posix_spawn_file_actions_t actions;
	check_spawn_action(::posix_spawn_file_actions_init(&actions), operation);

	struct actions_guard
	{
		posix_spawn_file_actions_t *value;
		~actions_guard() {
			::posix_spawn_file_actions_destroy(value);
		}
	}
	guard {&actions};

	check_spawn_action(::posix_spawn_file_actions_adddup2 (
		&actions, ::fileno(input.get()), STDIN_FILENO), operation
	);
	check_spawn_action(::posix_spawn_file_actions_adddup2 (
		&actions, null_output.get(), STDOUT_FILENO), operation
	);
	check_spawn_action(::posix_spawn_file_actions_adddup2 (
		&actions, null_output.get(), STDERR_FILENO), operation
	);
	std::vector<char*> argv;
	argv.reserve(arguments.size() + 1);

	for(auto &argument : arguments)
		argv.push_back(argument.data());
	argv.push_back(nullptr);

	pid_t process = -1;
	const int spawn_result = ::posix_spawnp(&process, argv.front(),
		&actions, nullptr, argv.data(), environ
	);
	if( spawn_result != 0 )
		throw_error(std::error_code(spawn_result, std::generic_category()), operation);

	const auto deadline = std::chrono::steady_clock::now() + timeout;
	int status = 0;
	for(;;)
	{
		const auto waited = ::waitpid(process, &status, WNOHANG);
		if( waited == process )
			break;

		if( waited < 0 and errno != EINTR )
			throw_errno(operation);

		if( std::chrono::steady_clock::now() >= deadline )
		{
			::kill(process, SIGTERM);
			const auto terminate_deadline = std::chrono::steady_clock::now() +
				std::chrono::milliseconds(200);

			while( std::chrono::steady_clock::now() < terminate_deadline )
			{
				const auto terminated = ::waitpid(process, &status, WNOHANG);
				if( terminated == process )
					break;

				if( terminated < 0 and errno != EINTR )
					throw_errno(operation);

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			if( ::waitpid(process, &status, WNOHANG) == 0 )
			{
				::kill(process, SIGKILL);
				while( ::waitpid(process, &status, 0) < 0 and errno == EINTR ) {}
			}
			throw_error(make_error_code(errc::command_timed_out), operation);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	if( WIFSIGNALED(status) )
		throw_error(make_error_code(errc::command_terminated), operation);

	if( not WIFEXITED(status) or WEXITSTATUS(status) != 0 )
		throw_error(make_error_code(errc::command_failed), operation);
}

} // namespace libempp::storage::detail

#endif //__linux__
