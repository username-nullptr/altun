// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_TEST_LINUX_BUS_TEST_SUPPORT_H
#define LIBEMPP_TEST_LINUX_BUS_TEST_SUPPORT_H

#include "../../test.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace empp_test_support
{

struct virtual_i2c_device
{
	unsigned long timeout_units = 0;
	unsigned long slave_address = 0;
	std::uint16_t message_address = 0;
	std::vector<std::uint8_t> register_bytes;
	std::vector<std::uint8_t> written_frame;
	std::vector<std::uint8_t> read_data;
	std::deque<std::vector<std::uint8_t>> read_sequence;
	std::size_t read_count = 0;
};

struct virtual_spi_device
{
	std::uint32_t mode = 0;
	std::uint32_t max_speed_hz = 0;
	std::uint8_t bits_per_word = 0;
	std::uint32_t transfer_speed_hz = 0;
	std::uint16_t delay_usecs = 0;
	std::uint8_t transfer_bits_per_word = 0;
	bool cs_change = false;
	std::vector<std::uint8_t> written_data;
	std::vector<std::uint8_t> read_data;
	std::deque<std::vector<std::uint8_t>> read_sequence;
	std::size_t read_count = 0;
};

struct virtual_pwm_device
{
	bool requested = false;
	std::uint32_t channel = 0;
	std::uint64_t period_ns = 0;
	std::uint64_t duty_cycle_ns = 0;
	std::uint64_t duty_offset_ns = 0;
	std::size_t round_count = 0;
	std::size_t set_count = 0;
	std::size_t free_count = 0;
};

inline std::mutex virtual_ioctl_mutex;
inline std::map<int, virtual_i2c_device> virtual_i2c_devices;
inline std::map<int, virtual_spi_device> virtual_spi_devices;
inline std::map<int, virtual_pwm_device> virtual_pwm_devices;

class temporary_file
{
public:
	temporary_file()
	{
		std::array<char, 32> pattern {};
		const std::string value = "/tmp/libempp-bus-XXXXXX";
		std::ranges::copy(value, pattern.begin());
		const int descriptor = ::mkstemp(pattern.data());
		if(descriptor < 0)
			empp_test::fail("mkstemp failed");
		::close(descriptor);
		m_path = pattern.data();
	}

	~temporary_file()
	{
		std::error_code ignored;
		std::filesystem::remove(m_path, ignored);
	}

	[[nodiscard]] const std::filesystem::path &path() const noexcept
	{
		return m_path;
	}

private:
	std::filesystem::path m_path;
};

inline void reset_virtual_i2c_devices()
{
	std::scoped_lock lock(virtual_ioctl_mutex);
	virtual_i2c_devices.clear();
}

inline void reset_virtual_spi_devices()
{
	std::scoped_lock lock(virtual_ioctl_mutex);
	virtual_spi_devices.clear();
}

inline void reset_virtual_pwm_devices()
{
	std::scoped_lock lock(virtual_ioctl_mutex);
	virtual_pwm_devices.clear();
}

} // namespace empp_test_support

#endif // LIBEMPP_TEST_LINUX_BUS_TEST_SUPPORT_H
