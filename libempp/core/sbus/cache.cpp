// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "cache.h"

namespace libempp::sbus { namespace
{

class cache_runtime
{
public:
	cache_runtime() :
		cache(context),
		worker([this] { libgs::exec(context); }) {}

	~cache_runtime()
	{
		context.stop();
		if( worker.joinable() )
			worker.join();
	}

	asio::io_context context;
	cache_t cache;
	std::thread worker;
};

} //namespace

cache_t &cache() noexcept
{
	static cache_runtime runtime;
	return runtime.cache;
}

} //namespace libempp::sbus
