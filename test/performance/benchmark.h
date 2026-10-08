// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_TEST_PERFORMANCE_BENCHMARK_H
#define ALTUN_TEST_PERFORMANCE_BENCHMARK_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace altun_test::performance
{

#ifndef ALTUN_PERFORMANCE_SCALE
# define ALTUN_PERFORMANCE_SCALE 1
#endif

inline constexpr size_t scale = ALTUN_PERFORMANCE_SCALE;
static_assert(scale > 0);

using clock = std::chrono::steady_clock;
using duration = clock::duration;

struct result
{
	std::string transport;
	std::string scope;
	std::string metric;
	size_t payload_size = 0;
	size_t operations = 0;
	duration elapsed {};
};

inline double nanoseconds_per_operation(const result &value)
{
	if( value.operations == 0 )
		throw std::invalid_argument("performance result requires an operation");
	return std::chrono::duration<double,std::nano>(value.elapsed).count() /
		static_cast<double>(value.operations);
}

inline double operations_per_second(const result &value)
{
	const auto seconds = std::chrono::duration<double>(value.elapsed).count();
	if( seconds <= 0 )
		throw std::runtime_error("performance timer did not advance");
	return static_cast<double>(value.operations) / seconds;
}

inline void print_result(const result &value)
{
	std::cout << std::fixed << std::setprecision(2)
		<< "[PERF] transport=" << value.transport
		<< " scope=" << value.scope
		<< " metric=" << value.metric
		<< " payload=" << value.payload_size << "B"
		<< " rate=" << operations_per_second(value) << " message/s"
		<< " cost=" << nanoseconds_per_operation(value) << " ns/message"
		<< " elapsed="
		<< std::chrono::duration<double,std::milli>(value.elapsed).count() << " ms"
		<< " count=" << value.operations
		<< " scale=" << scale << '\n';

	if( value.metric.find("throughput") != std::string::npos )
	{
		const auto mib_per_second = operations_per_second(value) *
			static_cast<double>(value.payload_size) / (1024.0 * 1024.0);
		std::cout << "[PERF] transport=" << value.transport
			<< " scope=" << value.scope
			<< " metric=effective-payload-throughput"
			<< " payload=" << value.payload_size << "B"
			<< " rate=" << mib_per_second << " MiB/s\n";
	}
}

inline duration median(std::vector<duration> values)
{
	if( values.empty() )
		throw std::invalid_argument("median requires a sample");
	std::ranges::sort(values);
	return values[values.size() / 2];
}

inline duration percentile(std::vector<duration> values, double quantile)
{
	if( values.empty() or quantile < 0.0 or quantile > 1.0 )
		throw std::invalid_argument("invalid percentile request");
	std::ranges::sort(values);
	const auto index = static_cast<size_t>(std::ceil(
		quantile * static_cast<double>(values.size())
	));
	return values[std::min(values.size() - 1, index == 0 ? 0 : index - 1)];
}

} //namespace altun_test::performance

#endif //ALTUN_TEST_PERFORMANCE_BENCHMARK_H
