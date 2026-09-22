# Storage

[Back to the Linux module guide](../linux.md) · [Back to the documentation index](../README.md)

Block-device discovery, read-only I/O, filesystems, partitions, and mounts are provided by `libempp::storage` and use the `empp.linux` link target. Include the `<libempp/linux/storage.h>` aggregate header or individual component headers as needed:

| Capability | Header | Modifies the target? |
| --- | --- | --- |
| Device enumeration, stable identity, geometry, and reads at an offset | `storage/block_device.h` | No; the descriptor is always read-only |
| Filesystem, disk, and partition inspection | `storage/information.h` | No |
| Mounted-filesystem capacity | `storage/space.h` | No |
| Formatting | `storage/format.h` | Yes |
| Replacing, adding, expanding, and deleting partitions | `storage/partition.h` | Yes |
| Mount-table queries, mounting, and unmounting | `storage/mount.h` | Queries do not; mounting and unmounting do |

Every operation returns `libgs::sys_expected<T>`. An absent value in a successful result is represented by `libgs::optional<T>` rather than an empty string, empty object, or Boolean sentinel.

> Formatting and `replace_partition_table()` destroy data. For real media, obtain a `device_info` first and use the overload that accepts it. That overload revalidates major/minor numbers, sysfs path, and a non-empty serial number before and after the operation, reducing the risk that the same `/dev/sdX` name points to a different device after a hot plug.

## Device discovery, identity, and read-only access

`enumerate_devices()` returns a unified `device_info` containing the device node, sysfs path, major/minor numbers, parent disk, bus, model, serial number, partition/removable/read-only flags, and, when the device can be opened, capacity and block sizes. `resolve_device()` resolves aliases such as `/dev/disk/by-id/*` into the same structure.

```cpp
#include <libempp/linux/storage.h>

namespace storage = libempp::storage;

auto devices = storage::enumerate_devices();
if(not devices)
    return devices.error().value();

for(const auto &candidate : *devices)
{
    if(candidate.removable and not candidate.partition)
        std::cout << candidate.device << ' ' << candidate.serial << '\n';
}

auto selected = storage::resolve_device("/dev/disk/by-id/example");
if(not selected)
    return selected.error().value();
```

Access a block device through a read-only RAII object. Opening from `device_info` validates the selected identity first:

```cpp
auto opened = storage::block_device::open(*selected);
if(not opened)
    return opened.error().value();

auto info = (*opened)->info();
if(not info)
    return info.error().value();

std::vector<std::byte> data(info->geometry.logical_block_size);
auto transferred = (*opened)->read_some_at(0, libgs::buffer(data));
```

The offset and return value of `read_some_at()` are in bytes. Short reads are allowed, and the shared file offset is not changed. After a hot unplug, discard the object, re-enumerate, and select the device again; do not continue using only the previous device-node name.

## Inspecting filesystems, partitions, and space

```cpp
auto disk = storage::inspect_disk(*selected);
if(not disk)
    return disk.error().value();

if(disk->table)
    std::cout << disk->table->type << ' ' << disk->size_bytes << " bytes\n";

for(const auto &partition : disk->partitions)
{
    if(partition.device)
        std::cout << *partition.device << '\n';
    if(partition.filesystem)
        std::cout << partition.filesystem->type << '\n';
}
```

`inspect_filesystem()` returns `result_t<optional<filesystem_info>>`; success with an empty optional means no signature was found. An empty `disk_info::table` means no partition table exists. Image files do not have real partition nodes, so `partition_info::device` may be empty. `space(path)` uses `statvfs` to return capacity and inode statistics.

Pass a `path_t` directly when inspecting a regular image file. For a real block device, prefer `device_info` so identity changes can also be detected during inspection.

## Modifying a partition table

Modify real disks through `device_info`; image tests may continue to pass a file path:

```cpp
storage::partition_spec data;
data.start_sector = 2048;
data.size_sectors = 0; // Use all remaining space.
data.type = "83";

auto disk = storage::replace_partition_table(
    *selected, storage::partition_table_type::dos, {data});
auto created = storage::create_partition(*selected, {.size_sectors = 131072});
auto expanded = storage::expand_partition(*selected, 1);
auto deleted = storage::delete_partition(*selected, 2);
```

Partition operations start `sfdisk` with a separate argument vector and do not invoke a shell. Before an operation, they verify that neither the whole disk nor its partitions are mounted. Afterward, they verify device identity and the kernel-visible layout.

- `replace_partition_table()` replaces the entire layout; an empty list leaves an empty partition table.
- `create_partition()` requires an existing GPT or DOS partition table.
- Only GPT accepts `partition_spec::name`.
- `expand_partition()` refuses to shrink a partition and does not expand the filesystem inside it.
- `delete_partition()` removes only the table entry; it does not erase the former data region.
- `partition_options::timeout` and `poll_interval` control the tool-execution and kernel-layout convergence limits.

## Formatting

```cpp
auto partition = storage::resolve_device("/dev/sdb1");
if(not partition)
    return partition.error().value();

storage::format_options options;
options.type = storage::filesystem_type::ext4;
options.label = "DATA";
options.mode = storage::format_mode::quick;

auto formatted = storage::format(*partition, options);
```

exFAT, ext4, and NTFS are supported. A block device must be writable and unmounted; regular files can be used for image tests. Before returning success, the operation probes the filesystem again and verifies its type and non-empty label. exFAT supports quick mode only. At runtime, the corresponding `mkfs.exfat`, `mkfs.ext4`, or `mkfs.ntfs` command must be installed; a missing tool returns `ENOENT`. The default timeout is two minutes.

## Mounting, unmounting, and the mount table

```cpp
#include <sys/mount.h>

storage::mount_options options;
options.flags = MS_NODEV | MS_NOSUID | MS_SYNCHRONOUS;
auto mounted = storage::mount(*partition, "/media/sdcard0", options);

auto by_target = storage::find_mount_by_target("/media/sdcard0");
auto by_disk = storage::find_mounts_by_device(*selected);
auto unmounted = storage::unmount_device(*selected);
```

The library first tries `mount(2)` and invokes the user-space `mount` helper when required. Before returning success, it reads `/proc/self/mountinfo` and verifies device identity. `unmount_target()` handles one mount point. `unmount_device()` unmounts a disk and its partitions from deepest to shallowest path. The library neither creates nor removes mount directories and does not modify `/etc/fstab`.

## Dependencies, permissions, and errors

- Building requires `libudev` and `libblkid`; partition changes require `sfdisk` at runtime.
- Partitioning, formatting, mounting, and unmounting normally require root or `CAP_SYS_ADMIN`.
- `storage::errc` represents library-level errors such as helper failure, timeout, validation failure, and device-identity changes. errno-style failures preserve the system error code.
- Production logs should retain at least the operation, device node, major/minor numbers, stable serial number, and `error()` value.
