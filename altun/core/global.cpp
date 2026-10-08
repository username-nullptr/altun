// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "global.h"
#include "log.h"

#if defined(__linux__)
# include <cerrno>
# include <execinfo.h>
#endif

namespace altun
{

const char *version_string() noexcept
{
	return ALTUN_VERSION_STR;
}

#if defined(__linux__)
namespace
{

// Fatal-signal output is deliberately best-effort, but it must still handle
// interrupted and partial writes. Keep this helper limited to async-signal-
// safe operations.
void write_all_best_effort(int descriptor, const char *data, size_t size) noexcept
{
	if( descriptor < 0 )
		return ;

	while( size > 0 )
	{
		ssize_t written = -1;
		do {
			written = ::write(descriptor, data, size);
		}
		while( written < 0 and errno == EINTR );

		if( written <= 0 )
			return ;

		data += written;
		size -= static_cast<size_t>(written);
	}
}

} // namespace

static void signal_handler(int signo, siginfo_t*, void*)
{
	if( signo == SIGINT )
	{
		// Application cleanup can occasionally block a normal shutdown.
		// Force termination until a safe recovery path is available.
		riwo::forced_termination();
	}
	else if( signo == SIGTRAP )
		return ;

	else if( signo == SIGPIPE )
	{
		altun_log_warning("Altun", "---- Caught SIGPIPE signal ----");
		return ;
	}
	void *backtrace_buffer[64];
	auto num_frames = backtrace(backtrace_buffer, 64);

	char **symbols = backtrace_symbols(backtrace_buffer, num_frames);
	auto pid = getpid();

	int fd = open(std::format("crash.{}.log", pid).c_str(),
		O_WRONLY | O_TRUNC | O_CREAT, 0644
	);
	char head_buf[] = "Signal [\0\0\0\0\0";
	int ofs = 8;

	auto num = signo / 100;
	if( !!num )
		head_buf[ofs++] = static_cast<char>('0' + num);

	num = signo / 10 % 10;
	if( !!num )
		head_buf[ofs++] = static_cast<char>('0' + num);

	num = signo % 10;
	if( !!num )
		head_buf[ofs++] = static_cast<char>('0' + num);

	head_buf[ofs++] = ']';
	head_buf[ofs++] = '\n';

	write_all_best_effort(fd, head_buf, static_cast<size_t>(ofs));
	write_all_best_effort(STDERR_FILENO, head_buf, static_cast<size_t>(ofs));

	if( symbols )
	{
		for(decltype(num_frames) i=0; i<num_frames; i++)
		{
			const auto symbol_size = strlen(symbols[i]);
			write_all_best_effort(fd, symbols[i], symbol_size);
			write_all_best_effort(fd, "\n", 1);
			write_all_best_effort(STDERR_FILENO, symbols[i], symbol_size);
			write_all_best_effort(STDERR_FILENO, "\n", 1);
		}
		free(symbols);
	}
	else
	{
		constexpr auto text = "The stack cannot be traced.\n";
		static const size_t len = strlen(text);
		write_all_best_effort(fd, text, len);
		write_all_best_effort(STDERR_FILENO, text, len);
	}
	if( fd >= 0 )
		close(fd);
	riwo::forced_termination();
}

RIWO_REGISTRATION
{
	struct sigaction sa {};
	sa.sa_sigaction = signal_handler;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;

	sigaction(SIGSEGV, &sa, nullptr); // Segmentation fault.
	sigaction(SIGABRT, &sa, nullptr); // Process abort.
	sigaction(SIGBUS , &sa, nullptr); // Bus error.
	sigaction(SIGFPE , &sa, nullptr); // Arithmetic exception.
	sigaction(SIGILL , &sa, nullptr); // Illegal instruction.

	sigaction(SIGTRAP, &sa, nullptr); // Debug trap.
	sigaction(SIGPIPE, &sa, nullptr); // Broken pipe.
	sigaction(SIGINT , &sa, nullptr); // Normal termination request.
}
#endif //__linux__

} //namespace altun
