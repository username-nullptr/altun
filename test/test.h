// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_TEST_H
#define LIBEMPP_TEST_H

#include <atomic>
#include <charconv>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <initializer_list>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace empp_test
{

class temporary_directory
{
public:
	temporary_directory()
	{
		static std::atomic_uint64_t sequence {0};
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		m_path = std::filesystem::temp_directory_path() /
			("libempp-test-" + std::to_string(stamp) + '-' +
			 std::to_string(sequence.fetch_add(1)));
		std::filesystem::create_directories(m_path);
	}

	~temporary_directory()
	{
		std::error_code ignored;
		std::filesystem::remove_all(m_path, ignored);
	}

	temporary_directory(const temporary_directory&) = delete;
	temporary_directory &operator=(const temporary_directory&) = delete;

	[[nodiscard]] const std::filesystem::path &path() const noexcept
	{
		return m_path;
	}

private:
	std::filesystem::path m_path;
};

struct test_case
{
	std::string_view suite;
	std::string_view name;
	void (*function)();
};

inline std::vector<test_case> &registry()
{
	static std::vector<test_case> tests;
	return tests;
}

class registrar
{
public:
	registrar(std::string_view suite, std::string_view name, void (*function)())
	{
		registry().push_back({suite, name, function});
	}
};

struct run_context
{
	std::string_view suite {};
	std::string_view test_name {};
	std::size_t iteration = 1;
	std::size_t repeat = 1;
	std::uint64_t seed = 0;
};

inline thread_local run_context active_run_context {};

[[nodiscard]] inline const run_context &current_run() noexcept
{
	return active_run_context;
}

[[nodiscard]] inline std::uint64_t current_seed() noexcept
{
	return active_run_context.seed;
}

class random_sequence
{
public:
	explicit random_sequence(std::uint64_t seed = current_seed()) noexcept :
		m_state(seed != 0 ? seed : 0x9e3779b97f4a7c15ULL) {}

	[[nodiscard]] std::uint64_t next() noexcept
	{
		m_state += 0x9e3779b97f4a7c15ULL;
		auto value = m_state;
		value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
		value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
		return value ^ (value >> 31);
	}

	[[nodiscard]] std::size_t bounded(std::size_t upper_bound) noexcept
	{
		return upper_bound == 0 ? 0 : static_cast<std::size_t>(next() % upper_bound);
	}

private:
	std::uint64_t m_state;
};

[[noreturn]] inline void fail(std::string message,
	const std::source_location &location = std::source_location::current())
{
	throw std::runtime_error(std::format(
		"{}:{}: {}", location.file_name(), location.line(), message
	));
}

inline void require(bool condition, std::string_view expression,
	const std::source_location &location = std::source_location::current())
{
	if(not condition)
		fail(std::format("requirement failed: {}", expression), location);
}

template <typename Actual, typename Expected>
void require_equal(const Actual &actual, const Expected &expected,
	std::string_view actual_expression, std::string_view expected_expression,
	const std::source_location &location = std::source_location::current())
{
	const bool equal = [&]
	{
		if constexpr(std::integral<Actual> and std::integral<Expected> and
			not std::same_as<Actual,bool> and not std::same_as<Expected,bool> and
			(std::is_signed_v<Actual> != std::is_signed_v<Expected>))
		{
			if constexpr(std::is_signed_v<Actual>)
				return actual < 0 ? false :
					static_cast<std::make_unsigned_t<Actual>>(actual) == expected;
			else
				return expected < 0 ? false :
					actual == static_cast<std::make_unsigned_t<Expected>>(expected);
		}
		else
			return actual == expected;
	}();
	if(equal)
		return;

	std::ostringstream message;
	message << actual_expression << " != " << expected_expression;
	if constexpr(requires { message << actual << expected; })
		message << " (actual: " << actual << ", expected: " << expected << ')';
	fail(message.str(), location);
}

inline void require_equal(const std::error_code &actual,
	const std::error_code &expected, std::string_view actual_expression,
	std::string_view expected_expression,
	const std::source_location &location = std::source_location::current())
{
	if(actual != expected)
	{
		fail(std::format(
			"{} is '{}:{}' ({}), but {} is '{}:{}' ({})",
			actual_expression, actual.category().name(), actual.value(), actual.message(),
			expected_expression, expected.category().name(), expected.value(),
			expected.message()
		), location);
	}
}

template <typename Exception, typename Function>
void require_throws(Function &&function, std::string_view expression,
	const std::source_location &location = std::source_location::current())
{
	try {
		std::forward<Function>(function)();
	}
	catch(const Exception&)
	{
		return;
	}
	catch(...)
	{
		fail(std::format("{} threw an unexpected exception type", expression), location);
	}
	fail(std::format("{} did not throw", expression), location);
}

template <typename Function>
void require_system_error(const std::error_code &expected, Function &&function,
	std::string_view expression,
	const std::source_location &location = std::source_location::current())
{
	try {
		std::forward<Function>(function)();
	}
	catch(const std::system_error &exception)
	{
		if(exception.code() != expected)
		{
			fail(std::format(
				"{} threw system_error '{}' ({}), expected '{}' ({})",
				expression, exception.code().message(), exception.code().value(),
				expected.message(), expected.value()
			), location);
		}
		return;
	}
	catch(...)
	{
		fail(std::format("{} threw an unexpected exception type", expression), location);
	}
	fail(std::format("{} did not throw", expression), location);
}

namespace detail
{

struct run_options
{
	std::vector<std::string_view> cases;
	std::size_t repeat = 1;
	std::uint64_t seed = 0;
	bool list = false;
	bool fail_fast = false;
	bool help = false;
};

template <typename Integer>
[[nodiscard]] Integer parse_integer(std::string_view text, std::string_view option)
{
	Integer value {};
	const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
	if(result.ec != std::errc {} or result.ptr != text.data() + text.size())
		throw std::invalid_argument(std::string(option) + " requires an integer");
	return value;
}

[[nodiscard]] inline run_options environment_options()
{
	run_options options;
	options.seed = static_cast<std::uint64_t>(
		std::chrono::steady_clock::now().time_since_epoch().count());
	if(const auto *value = std::getenv("LIBEMPP_TEST_CASE"); value and *value)
		options.cases.emplace_back(value);
	if(const auto *value = std::getenv("LIBEMPP_TEST_REPEAT"); value and *value)
	{
		options.repeat = parse_integer<std::size_t>(value, "LIBEMPP_TEST_REPEAT");
		if(options.repeat == 0)
			throw std::invalid_argument("LIBEMPP_TEST_REPEAT must be positive");
	}
	if(const auto *value = std::getenv("LIBEMPP_TEST_SEED"); value and *value)
		options.seed = parse_integer<std::uint64_t>(value, "LIBEMPP_TEST_SEED");
	if(const auto *value = std::getenv("LIBEMPP_TEST_FAIL_FAST"); value and *value)
		options.fail_fast = std::string_view(value) != "0";
	return options;
}

[[nodiscard]] inline run_options parse_options(int argc, const char *const argv[])
{
	auto options = environment_options();
	for(int index = 1; index < argc; ++index)
	{
		const std::string_view argument(argv[index]);
		auto value_after = [&](std::string_view option) -> std::string_view
		{
			if(index + 1 >= argc)
				throw std::invalid_argument(std::string(option) + " requires a value");
			return argv[++index];
		};

		if(argument == "--case")
			options.cases.emplace_back(value_after(argument));
		else if(argument == "--repeat")
		{
			options.repeat = parse_integer<std::size_t>(value_after(argument), argument);
			if(options.repeat == 0)
				throw std::invalid_argument("--repeat must be positive");
		}
		else if(argument == "--seed")
			options.seed = parse_integer<std::uint64_t>(value_after(argument), argument);
		else if(argument == "--list")
			options.list = true;
		else if(argument == "--fail-fast")
			options.fail_fast = true;
		else if(argument == "--help" or argument == "-h")
			options.help = true;
		else
			throw std::invalid_argument("unknown option: " + std::string(argument));
	}
	return options;
}

[[nodiscard]] inline std::uint64_t hash_name(
	std::string_view suite, std::string_view name) noexcept
{
	std::uint64_t hash = 1469598103934665603ULL;
	for(const auto text : {suite, name})
	{
		for(const auto value : text)
		{
			hash ^= static_cast<unsigned char>(value);
			hash *= 1099511628211ULL;
		}
	}
	return hash;
}

[[nodiscard]] inline std::uint64_t iteration_seed(std::uint64_t base,
	const test_case &test, std::size_t iteration) noexcept
{
	random_sequence sequence(base ^ hash_name(test.suite, test.name) ^
		(static_cast<std::uint64_t>(iteration) * 0x9e3779b97f4a7c15ULL));
	return sequence.next();
}

[[nodiscard]] inline bool selected(const test_case &test,
	const std::vector<std::string_view> &cases) noexcept
{
	if(cases.empty())
		return true;
	for(const auto selected_case : cases)
	{
		if(test.name == selected_case)
			return true;
	}
	return false;
}

inline void print_help()
{
	std::cout
		<< "Options:\n"
		<< "  --list              list test cases without running them\n"
		<< "  --case <name>       run one named case; may be repeated\n"
		<< "  --repeat <count>    recreate and run each selected case count times\n"
		<< "  --seed <value>      reproduce scheduling perturbations\n"
		<< "  --fail-fast         stop after the first failed iteration\n";
}

inline int run_with_options(const run_options &options)
{
	const auto &tests = registry();
	if(options.help)
	{
		print_help();
		return 0;
	}
	if(options.list)
	{
		for(const auto &test : tests)
			std::cout << test.suite << " / " << test.name << '\n';
		return tests.empty() ? 2 : 0;
	}

	std::size_t selected_count = 0;
	for(const auto &test : tests)
		selected_count += selected(test, options.cases);
	if(selected_count == 0)
	{
		std::cerr << "no test case matched";
		if(not options.cases.empty())
			std::cerr << ": " << options.cases.front();
		std::cerr << '\n';
		return 2;
	}

	std::cout << "[CONFIG] repeat=" << options.repeat
		<< " seed=" << options.seed << '\n' << std::flush;
	std::size_t failures = 0;
	std::size_t completed = 0;
	for(const auto &test : tests)
	{
		if(not selected(test, options.cases))
			continue;
		for(std::size_t iteration = 1; iteration <= options.repeat; ++iteration)
		{
			active_run_context = {
				test.suite,
				test.name,
				iteration,
				options.repeat,
				iteration_seed(options.seed, test, iteration)
			};
			try {
				const auto begin = std::chrono::steady_clock::now();
				test.function();
				const auto elapsed = std::chrono::duration<double,std::milli>(
					std::chrono::steady_clock::now() - begin).count();
				std::cout << "[PASS] " << test.suite << " / " << test.name;
				if(options.repeat != 1)
					std::cout << " (iteration " << iteration << '/' << options.repeat
						<< ", seed " << active_run_context.seed << ')';
				std::cout << " [" << elapsed << " ms]\n";
			}
			catch(const std::exception &exception)
			{
				++failures;
				std::cerr << "[FAIL] " << test.suite << " / " << test.name
					<< " (iteration " << iteration << '/' << options.repeat
					<< ", seed " << active_run_context.seed << "):\n       "
					<< exception.what() << '\n';
			}
			catch(...)
			{
				++failures;
				std::cerr << "[FAIL] " << test.suite << " / " << test.name
					<< " (iteration " << iteration << '/' << options.repeat
					<< ", seed " << active_run_context.seed
					<< "):\n       unknown exception\n";
			}
			++completed;
			if(failures != 0 and options.fail_fast)
				break;
		}
		if(failures != 0 and options.fail_fast)
			break;
	}
	active_run_context = {};

	std::cout << (completed - failures) << '/' << completed
		<< (options.repeat == 1 ? " tests passed\n" : " runs passed\n");
	return failures == 0 ? 0 : 1;
}

} // namespace detail

inline int run(int argc, const char *const argv[]) noexcept
{
	try {
		return detail::run_with_options(detail::parse_options(argc, argv));
	}
	catch(const std::exception &exception)
	{
		std::cerr << "test option error: " << exception.what() << '\n';
		return 2;
	}
}

} // namespace empp_test

#define EMPP_TEST_CONCAT_IMPL(left, right) left##right
#define EMPP_TEST_CONCAT(left, right) EMPP_TEST_CONCAT_IMPL(left, right)

#define EMPP_TEST(suite_name, test_name) \
	static void EMPP_TEST_CONCAT(empp_test_function_, __LINE__)(); \
	static ::empp_test::registrar EMPP_TEST_CONCAT(empp_test_registrar_, __LINE__)( \
		suite_name, test_name, &EMPP_TEST_CONCAT(empp_test_function_, __LINE__) \
	); \
	static void EMPP_TEST_CONCAT(empp_test_function_, __LINE__)()

#define EMPP_REQUIRE(expression) \
	::empp_test::require(static_cast<bool>(expression), #expression)

#define EMPP_REQUIRE_EQ(actual, expected) \
	::empp_test::require_equal((actual), (expected), #actual, #expected)

#define EMPP_REQUIRE_THROWS(exception_type, expression) \
	::empp_test::require_throws<exception_type>([&] { expression; }, #expression)

#define EMPP_REQUIRE_SYSTEM_ERROR(error_code, expression) \
	::empp_test::require_system_error((error_code), [&] { expression; }, #expression)

#endif // LIBEMPP_TEST_H
