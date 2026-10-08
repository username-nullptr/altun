// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#include "types.h"

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

namespace libempp::storage
{

riwo::optional<sector_t> partition_info::end_sector() const noexcept
{
	if( size_sectors == 0 or
		start_sector > std::numeric_limits<sector_t>::max() - size_sectors + 1 )
		return riwo::nullopt;
	return start_sector + size_sectors - 1;
}

riwo::optional<uint64_t> partition_info::size_bytes(uint32_t sector_size) const noexcept
{
	if( sector_size == 0 or
		size_sectors > std::numeric_limits<uint64_t>::max() / sector_size )
		return riwo::nullopt;
	return size_sectors * sector_size;
}

} //namespace libempp::storage


#endif //__linux__
