// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_STORAGE_H
#define ALTUN_LINUX_STORAGE_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/storage/block_device.h>
#include <altun/linux/storage/information.h>
#include <altun/linux/storage/partition.h>
#include <altun/linux/storage/format.h>
#include <altun/linux/storage/error.h>
#include <altun/linux/storage/types.h>
#include <altun/linux/storage/mount.h>
#include <altun/linux/storage/space.h>

#endif //__linux__
#endif // ALTUN_LINUX_STORAGE_H
