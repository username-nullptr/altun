// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "bus.h"

#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>

namespace
{

struct test_pwmchip_waveform
{
	std::uint32_t hwpwm;
	std::uint32_t pad;
	std::uint64_t period_length_ns;
	std::uint64_t duty_length_ns;
	std::uint64_t duty_offset_ns;
};

constexpr unsigned long test_pwm_ioctl_request = _IO(0x75, 1);
constexpr unsigned long test_pwm_ioctl_free = _IO(0x75, 2);
constexpr unsigned long test_pwm_ioctl_round_waveform =
	_IOWR(0x75, 3, test_pwmchip_waveform);
constexpr unsigned long test_pwm_ioctl_get_waveform =
	_IOWR(0x75, 4, test_pwmchip_waveform);
constexpr unsigned long test_pwm_ioctl_set_exact_waveform =
	_IOW(0x75, 6, test_pwmchip_waveform);

} // namespace

using namespace empp_test_support;

extern "C" int ioctl(int descriptor, unsigned long request, ...) noexcept
{
	va_list arguments;
	va_start(arguments, request);
	std::scoped_lock lock(virtual_ioctl_mutex);

	if(request == I2C_TIMEOUT)
	{
		auto &device = virtual_i2c_devices[descriptor];
		device.timeout_units = va_arg(arguments, unsigned long);
		va_end(arguments);
		return 0;
	}
	if(request == I2C_SLAVE)
	{
		auto &device = virtual_i2c_devices[descriptor];
		device.slave_address = va_arg(arguments, unsigned long);
		va_end(arguments);
		return 0;
	}
	if(request == I2C_RDWR)
	{
		auto &device = virtual_i2c_devices[descriptor];
		auto *transfer = va_arg(arguments, i2c_rdwr_ioctl_data*);
		va_end(arguments);
		if(not transfer or not transfer->msgs or transfer->nmsgs == 0)
		{
			errno = EINVAL;
			return -1;
		}

		device.message_address = transfer->msgs[0].addr;
		if(transfer->nmsgs == 1)
		{
			const auto &message = transfer->msgs[0];
			device.written_frame.assign(message.buf, message.buf + message.len);
		}
		else if(transfer->nmsgs == 2)
		{
			const auto &register_message = transfer->msgs[0];
			auto &read_message = transfer->msgs[1];
			device.register_bytes.assign(register_message.buf,
				register_message.buf + register_message.len);
			const auto &read_data = device.read_sequence.empty() ?
				device.read_data : device.read_sequence.front();
			for(std::size_t index = 0; index < read_message.len; ++index)
			{
				read_message.buf[index] = index < read_data.size() ?
					read_data[index] : 0;
			}
			if(not device.read_sequence.empty())
				device.read_sequence.pop_front();
			++device.read_count;
		}
		return static_cast<int>(transfer->nmsgs);
	}
	if(request == test_pwm_ioctl_request)
	{
		auto &device = virtual_pwm_devices[descriptor];
		const auto channel = va_arg(arguments, unsigned long);
		va_end(arguments);
		if(device.requested)
		{
			errno = EBUSY;
			return -1;
		}
		device.requested = true;
		device.channel = static_cast<std::uint32_t>(channel);
		return 0;
	}
	if(request == test_pwm_ioctl_free)
	{
		auto &device = virtual_pwm_devices[descriptor];
		const auto channel = va_arg(arguments, unsigned long);
		va_end(arguments);
		if(not device.requested or channel != device.channel)
		{
			errno = EINVAL;
			return -1;
		}
		device.requested = false;
		++device.free_count;
		return 0;
	}
	if(request == test_pwm_ioctl_get_waveform or
		request == test_pwm_ioctl_round_waveform or
		request == test_pwm_ioctl_set_exact_waveform)
	{
		auto &device = virtual_pwm_devices[descriptor];
		auto *waveform = va_arg(arguments, test_pwmchip_waveform*);
		va_end(arguments);
		if(not waveform or not device.requested or
			waveform->hwpwm != device.channel)
		{
			errno = EINVAL;
			return -1;
		}
		if(request == test_pwm_ioctl_get_waveform)
		{
			waveform->period_length_ns = device.period_ns;
			waveform->duty_length_ns = device.duty_cycle_ns;
			waveform->duty_offset_ns = device.duty_offset_ns;
		}
		else if(request == test_pwm_ioctl_round_waveform)
			++device.round_count;
		else
		{
			device.period_ns = waveform->period_length_ns;
			device.duty_cycle_ns = waveform->duty_length_ns;
			device.duty_offset_ns = waveform->duty_offset_ns;
			++device.set_count;
		}
		return 0;
	}
	if(request == SPI_IOC_WR_MODE32)
	{
		auto *mode = va_arg(arguments, std::uint32_t*);
		va_end(arguments);
		if(not mode)
		{
			errno = EINVAL;
			return -1;
		}
		virtual_spi_devices[descriptor].mode = *mode;
		return 0;
	}
	if(request == SPI_IOC_WR_BITS_PER_WORD)
	{
		auto *bits = va_arg(arguments, std::uint8_t*);
		va_end(arguments);
		if(not bits)
		{
			errno = EINVAL;
			return -1;
		}
		virtual_spi_devices[descriptor].bits_per_word = *bits;
		return 0;
	}
	if(request == SPI_IOC_WR_MAX_SPEED_HZ)
	{
		auto *speed = va_arg(arguments, std::uint32_t*);
		va_end(arguments);
		if(not speed)
		{
			errno = EINVAL;
			return -1;
		}
		virtual_spi_devices[descriptor].max_speed_hz = *speed;
		return 0;
	}
	if(request == SPI_IOC_MESSAGE(1))
	{
		auto *transfer = va_arg(arguments, spi_ioc_transfer*);
		va_end(arguments);
		if(not transfer)
		{
			errno = EINVAL;
			return -1;
		}

		auto &device = virtual_spi_devices[descriptor];
		device.transfer_speed_hz = transfer->speed_hz;
		device.delay_usecs = transfer->delay_usecs;
		device.transfer_bits_per_word = transfer->bits_per_word;
		device.cs_change = transfer->cs_change;
		if(transfer->tx_buf != 0)
		{
			const auto *data = reinterpret_cast<const std::uint8_t*>(
				static_cast<std::uintptr_t>(transfer->tx_buf)
			);
			device.written_data.assign(data, data + transfer->len);
		}
		else
			device.written_data.clear();

		if(transfer->rx_buf != 0)
		{
			auto *data = reinterpret_cast<std::uint8_t*>(
				static_cast<std::uintptr_t>(transfer->rx_buf)
			);
			const auto &read_data = device.read_sequence.empty() ?
				device.read_data : device.read_sequence.front();
			for(std::size_t index = 0; index < transfer->len; ++index)
			{
				data[index] = index < read_data.size() ?
					read_data[index] : 0;
			}
			if(not device.read_sequence.empty())
				device.read_sequence.pop_front();
			++device.read_count;
		}
		return static_cast<int>(transfer->len);
	}

	va_end(arguments);
	errno = ENOTTY;
	return -1;
}
