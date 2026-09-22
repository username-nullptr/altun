// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include <libempp/linux/serial_port_binding.h>
#include <iostream>
#include <limits>

int main(int argc, const char *argv[])
{
	if(argc < 2)
	{
		std::cerr << "Usage: serial_port_binding <model-pattern> [baud-rate]\n";
		return 1;
	}
	libempp::serial_port_options options;
	if(argc > 2)
	{
		try {
			size_t end = 0;
			const auto baud_rate = std::stoul(argv[2], &end);

			if(argv[2][end] != '\0' or baud_rate > std::numeric_limits<uint32_t>::max())
				throw std::out_of_range(argv[2]);

			options.baud_rate = static_cast<uint32_t>(baud_rate);
		}
		catch(const std::exception &exception)
		{
			std::cerr << "Invalid baud rate: " << exception.what() << '\n';
			return 1;
		}
	}
	libempp::serial_port_binding binding;
	binding.opened.connect([](std::string_view port) {
		libempp_log_info("Opened {}", port);
	});
	binding.closed.connect([](std::string_view port, const std::error_code &error) {
		libempp_log_info("Closed {}: {}", port, error.message());
	});
	binding.error.connect([](std::string_view port, const std::error_code &error) {
		libempp_log_error("{}: {}", port, error.message());
	});
	binding.received.connect([](const libempp::serial_port_binding::io_context_ptr &context)
	{
		auto payload = context->take_payload<std::string>();
		libempp_log_info("{} received {} byte(s): {}",
			context->port(), payload.size(), payload
		);
		std::error_code error;
		context->write("ACK\n", error);
		if(error)
			libempp_log_error("Reply failed: {}", error.message());
	});

	auto rule = binding.make_rule (
		libempp::udev::prop_key::id_model, argv[1], options
	);
	rule->open();
	return libgs::exec();
}
