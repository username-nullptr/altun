# Storage

[Linux module](../linux.md) · [Documentation](../README.md)

Storage APIs live in `libempp::storage`, link with `empp.linux`, and return `riwo::sys_expected<T>`. A successful lookup with no result uses `riwo::optional<T>`.

| Header | Capability | Writes storage? |
| --- | --- | :---: |
| `storage/block_device.h` | Enumerate, resolve, identify, and read block devices | No |
| `storage/information.h` | Inspect filesystems, disks, and partitions | No |
| `storage/space.h` | Query filesystem capacity | No |
| `storage/partition.h` | Replace, add, expand, or delete partitions | Yes |
| `storage/format.h` | Create a filesystem | Yes |
| `storage/mount.h` | Query, mount, or unmount | Mount state only |

## Identify and inspect

Use `device_info` for real devices so mutating operations can validate major/minor numbers, sysfs path, and serial identity. Path overloads are useful for regular image files.

```cpp
#include <libempp/linux/storage.h>

namespace storage = libempp::storage;

auto selected = storage::resolve_device("/dev/disk/by-id/example");
if(not selected)
    return selected.error().value();

auto disk = storage::inspect_disk(*selected);
auto filesystem = storage::inspect_filesystem(*selected);
auto mounts = storage::find_mounts_by_device(*selected);
```

`enumerate_devices()` returns device nodes, stable identity, parent disk, model, serial, flags, and available geometry. `inspect_filesystem()` returns an empty optional when no signature exists; `disk_info::table` is empty when no partition table exists.

Read-only access uses `block_device`:

```cpp
auto opened = storage::block_device::open(*selected);
if(not opened)
    return opened.error().value();

auto info = (*opened)->info();
if(not info)
    return info.error().value();

std::vector<std::byte> data(info->geometry.logical_block_size);
auto count = (*opened)->read_some_at(0, riwo::buffer(data));
```

Offsets and counts are bytes; short reads are valid. Re-resolve a device after hot unplug instead of trusting an old `/dev` name.

## Partition tables

```cpp
storage::partition_spec data;
data.start_sector = 2048;
data.size_sectors = 0; // Remaining space.
data.type = "83";

auto disk = storage::replace_partition_table(
    *selected, storage::partition_table_type::dos, {data});
auto created = storage::create_partition(*selected, {.size_sectors = 131072});
auto expanded = storage::expand_partition(*selected, 1);
auto deleted = storage::delete_partition(*selected, 2);
```

`replace_partition_table()` replaces the complete layout. `expand_partition()` does not resize the filesystem; `delete_partition()` removes only the table entry. Operations reject mounted targets and use `sfdisk` at runtime.

## Format and mount

```cpp
#include <sys/mount.h>

auto partition = storage::resolve_device("/dev/disk/by-id/example-part1");
if(not partition)
    return partition.error().value();

storage::format_options format;
format.type = storage::filesystem_type::ext4;
format.label = "DATA";
auto formatted = storage::format(*partition, format);

storage::mount_options mount;
mount.flags = MS_NODEV | MS_NOSUID;
auto mounted = storage::mount(*partition, "/media/data", mount);
auto unmounted = storage::unmount_target("/media/data");
```

Formatting supports exFAT, ext4, and NTFS and requires the matching `mkfs.*` executable. The library does not create mount directories or edit `/etc/fstab`. `unmount_device()` unmounts a disk and its partitions from deepest to shallowest mount path.

> Partitioning and formatting destroy data. Resolve a stable device identity, verify its serial and major/minor numbers, confirm it is not mounted or in use, and use the `device_info` overload. These operations normally require root or `CAP_SYS_ADMIN`.

`storage::errc` reports validation, helper, timeout, and device-identity failures; system failures preserve their `std::error_code`. See [`examples/linux/block.cpp`](../../examples/linux/block.cpp) and [`examples/linux/storage.cpp`](../../examples/linux/storage.cpp) for read-only programs.
