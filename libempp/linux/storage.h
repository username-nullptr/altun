// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef LIBEMPP_LINUX_STORAGE_H
#define LIBEMPP_LINUX_STORAGE_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <libempp/linux/storage/block_device.h>
#include <libempp/linux/storage/information.h>
#include <libempp/linux/storage/partition.h>
#include <libempp/linux/storage/format.h>
#include <libempp/linux/storage/error.h>
#include <libempp/linux/storage/types.h>
#include <libempp/linux/storage/mount.h>
#include <libempp/linux/storage/space.h>

#endif //__linux__
#endif // LIBEMPP_LINUX_STORAGE_H
