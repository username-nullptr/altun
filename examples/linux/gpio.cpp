// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <altun/linux/subsys/gpio.h>

#include <iostream>
#include <limits>
#include <string_view>

unsigned long parse_decimal(const char *text, unsigned long max)
{
	size_t end = 0;
	const auto value = std::stoul(text, &end);
	if( text[end] != '\0' or value > max )
		throw std::out_of_range(text);
	return value;
}

altun::subsys::gpio::edge_t parse_edge(std::string_view text)
{
	using edge = altun::subsys::gpio::edge_t;
	if( text == "rising" )
		return edge::rising;
	if( text == "falling" )
		return edge::falling;
	if( text == "both" )
		return edge::both;
	throw std::invalid_argument("edge must be rising, falling, or both");
}

int main(int argc, const char *argv[])
{
	if( argc < 4 or argc > 6 )
	{
		std::cerr << "Usage:\n"
			"  gpio <chip> <line> out <0|1>\n"
			"  gpio <chip> <line> in [rising|falling|both] [timeout-ms]\n";
		return 1;
	}

	try {
		using gpio = altun::subsys::basic_gpio<asio::io_context::executor_type>;
		asio::io_context context;
		gpio::node_t node;
		node.chip = argv[1];
		node.line = static_cast<gpio::line_t>(parse_decimal(
			argv[2], std::numeric_limits<gpio::line_t>::max()
		));

		const std::string_view operation = argv[3];
		if( operation == "out" )
		{
			if( argc != 5 or (std::string_view(argv[4]) != "0" and
				std::string_view(argv[4]) != "1") )
				throw std::invalid_argument("output value must be 0 or 1");
			node.direction = gpio::direction_t::output;
			node.initial_value = std::string_view(argv[4]) == "1";
		}
		else if( operation == "in" )
		{
			node.direction = gpio::direction_t::input;
			if( argc >= 5 )
				node.edge = parse_edge(argv[4]);
		}
		else
			throw std::invalid_argument("operation must be in or out");

		gpio line(node, context);
		std::cout << "GPIO backend: " << gpio::backend_name() << '\n';
		if( node.direction == gpio::direction_t::output or
			node.edge == gpio::edge_t::none )
		{
			const auto value = line.get();
			if( not value )
			{
				std::cerr << "GPIO read failed: " << value.error().message() << '\n';
				return 1;
			}
			std::cout << (node.direction == gpio::direction_t::output ?
				"Output value: " : "Input value: ") << *value << '\n';
		}
		else
		{
			const auto timeout = argc == 6 ? parse_decimal(
				argv[5], static_cast<unsigned long>(
					std::numeric_limits<int64_t>::max() / 1'000'000
				)
			) : 10'000UL;
			gpio::event_t event;
			std::error_code event_error;
			using namespace riwo::operators;
			line.wait_event(event,
				[&event_error, &context](std::error_code error) {
					event_error = error;
					context.stop();
				} | gpio::duration_t(timeout));
			context.run();
			if( event_error == std::errc::timed_out )
			{
				std::cout << "Timed out\n";
				return 2;
			}
			if( event_error )
				throw std::system_error(event_error);
			std::cout << (event.edge == gpio::edge_t::rising ? "rising" : "falling")
				<< " edge at " << event.timestamp_ns << " ns\n";
		}
	}
	catch(const std::exception &exception)
	{
		std::cerr << "GPIO failed: " << exception.what() << '\n';
		return 1;
	}
	return 0;
}
