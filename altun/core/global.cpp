// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "global.h"
#include "log.h"

#if defined(__linux__)
# include <execinfo.h>
#endif

namespace altun
{

const char *version_string() noexcept
{
	return ALTUN_VERSION_STR;
}

#if defined(__linux__)
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

	write(fd, head_buf, ofs);
	write(STDERR_FILENO, head_buf, ofs);

	if( symbols )
	{
		for(decltype(num_frames) i=0; i<num_frames; i++)
		{
			write(fd, symbols[i], strlen(symbols[i]));
			write(fd, "\n", 1);
			write(STDERR_FILENO, symbols[i], strlen(symbols[i]));
			write(STDERR_FILENO, "\n", 1);
		}
		free(symbols);
		close(fd);
	}
	else
	{
		constexpr auto text = "The stack cannot be traced.\n";
		static const size_t len = strlen(text);
		write(fd, text, len);
		write(STDERR_FILENO, text, len);
	}
	// exit(signo);
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
