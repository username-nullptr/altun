// SPDX-FileCopyrightText: 2025-2026 Xiaoqiang <username_nullptr@163.com>
// SPDX-License-Identifier: MIT

#ifndef ALTUN_LINUX_UDEV_PROPERTIES_H
#define ALTUN_LINUX_UDEV_PROPERTIES_H

#ifndef __linux__
# error "This module is only available for Linux."
#else //__linux__

#include <altun/linux/subsys/types.h>
#include <map>

namespace altun::udev
{

#define ALTUN_UDEV_BASIC_PROPERTY \
X_MACRO( action           , "ACTION"           ) \
X_MACRO( dev_links        , "DEVLINKS"         ) \
X_MACRO( dev_path         , "DEVPATH"          ) \
X_MACRO( dev_name         , "DEVNAME"          ) \
X_MACRO( dev_type         , "DEVTYPE"          ) \
X_MACRO( driver           , "DRIVER"           ) \
X_MACRO( major            , "MAJOR"            ) \
X_MACRO( minor            , "MINOR"            ) \
X_MACRO( modalias         , "MODALIAS"         ) \
X_MACRO( seq_num          , "SEQNUM"           ) \
X_MACRO( subsystem        , "SUBSYSTEM"        ) \
X_MACRO( usec_initialized , "USEC_INITIALIZED" ) \
X_MACRO( tags             , "TAGS"             ) \
X_MACRO( current_tags     , "CURRENT_TAGS"     )
// ... ...
struct basic_prop_key
{
#define X_MACRO(key,desc) \
	static constexpr const char *key = desc;
	ALTUN_UDEV_BASIC_PROPERTY
#undef X_MACRO
};

#define ALTUN_UDEV_HARDWARE_PROPERTY \
X_MACRO( id_model               , "ID_MODEL"               ) \
X_MACRO( id_model_enc           , "ID_MODEL_ENC"           ) \
X_MACRO( id_model_id            , "ID_MODEL_ID"            ) \
X_MACRO( id_model_from_database , "ID_MODEL_FROM_DATABASE" ) \
X_MACRO( id_serial              , "ID_SERIAL"              ) \
X_MACRO( id_serial_short        , "ID_SERIAL_SHORT"        ) \
X_MACRO( id_vendor              , "ID_VENDOR"              ) \
X_MACRO( id_vendor_enc          , "ID_VENDOR_ENC"          ) \
X_MACRO( id_vendor_id           , "ID_VENDOR_ID"           ) \
X_MACRO( id_revision            , "ID_REVISION"            ) \
X_MACRO( id_type                , "ID_TYPE"                ) \
X_MACRO( id_instance            , "ID_INSTANCE"            )
// ... ...
struct hardware_prop_key
{
#define X_MACRO(key,desc) \
	static constexpr const char *key = desc;
	ALTUN_UDEV_HARDWARE_PROPERTY
#undef X_MACRO
};

#define ALTUN_UDEV_BUS_PROPERTY \
X_MACRO( id_bus                  , "ID_BUS"                  ) \
X_MACRO( id_path                 , "ID_PATH"                 ) \
X_MACRO( id_path_tag             , "ID_PATH_TAG"             ) \
X_MACRO( id_for_seat             , "ID_FOR_SEAT"             ) \
X_MACRO( id_seat                 , "ID_SEAT"                 ) \
X_MACRO( id_auto_seat            , "ID_AUTOSEAT"             ) \
X_MACRO( id_vendor_from_database , "ID_VENDOR_FROM_DATABASE" ) \
X_MACRO( id_oui_from_database    , "ID_OUI_FROM_DATABASE"    ) \
X_MACRO( id_mm_candidate         , "ID_MM_CANDIDATE"         )
// ... ...
struct bus_prop_key
{
#define X_MACRO(key,desc) \
	static constexpr const char *key = desc;
	ALTUN_UDEV_BUS_PROPERTY
#undef X_MACRO
};

#define ALTUN_UDEV_USB_PROPERTY \
X_MACRO( id_usb_model               , "ID_USB_MODEL"               ) \
X_MACRO( id_usb_model_enc           , "ID_USB_MODEL_ENC"           ) \
X_MACRO( id_usb_model_id            , "ID_USB_MODEL_ID"            ) \
X_MACRO( id_usb_serial              , "ID_USB_SERIAL"              ) \
X_MACRO( id_usb_vendor              , "ID_USB_VENDOR"              ) \
X_MACRO( id_usb_vendor_enc          , "ID_USB_VENDOR_ENC"          ) \
X_MACRO( id_usb_vendor_id           , "ID_USB_VENDOR_ID"           ) \
X_MACRO( id_usb_revision            , "ID_USB_REVISION"            ) \
X_MACRO( id_usb_type                , "ID_USB_TYPE"                ) \
X_MACRO( id_usb_interfaces          , "ID_USB_INTERFACES"          ) \
X_MACRO( id_usb_interface_num       , "ID_USB_INTERFACE_NUM"       ) \
X_MACRO( id_usb_driver              , "ID_USB_DRIVER"              ) \
X_MACRO( id_usb_class_from_database , "ID_USB_CLASS_FROM_DATABASE" )
// ... ...
struct usb_prop_key
{
#define X_MACRO(key,desc) \
	static constexpr const char *key = desc;
	ALTUN_UDEV_USB_PROPERTY
#undef X_MACRO
};

#define ALTUN_UDEV_FILESYSTEM_PROPERTY \
X_MACRO( id_fs_usage      , "ID_FS_USAGE"     ) \
X_MACRO( id_fs_type       , "ID_FS_TYPE"      ) \
X_MACRO( id_fs_version    , "ID_FS_VERSION"   ) \
X_MACRO( id_fs_uuid       , "ID_FS_UUID"      ) \
X_MACRO( id_fs_uuid_enc   , "ID_FS_UUID_ENC"  ) \
X_MACRO( id_fs_uuid_sub   , "ID_FS_UUID_SUB"  ) \
X_MACRO( id_fs_label      , "ID_FS_LABEL"     ) \
X_MACRO( id_fs_label_enc  , "ID_FS_LABEL_ENC" ) \
X_MACRO( id_fs_block_size , "ID_FS_BLOCKSIZE" ) \
X_MACRO( id_fs_size       , "ID_FS_SIZE"      )
// ... ...
struct filesystem_prop_key
{
#define X_MACRO(key,desc) \
	static constexpr const char *key = desc;
	ALTUN_UDEV_FILESYSTEM_PROPERTY
#undef X_MACRO
};

#define ALTUN_UDEV_PARTITION_PROPERTY \
X_MACRO( id_part_table_type    , "ID_PART_TABLE_TYPE"   ) \
X_MACRO( id_part_table_uuid    , "ID_PART_TABLE_UUID"   ) \
X_MACRO( id_part_entry_scheme  , "ID_PART_ENTRY_SCHEME" ) \
X_MACRO( id_part_entry_uuid    , "ID_PART_ENTRY_UUID"   ) \
X_MACRO( id_part_entry_type    , "ID_PART_ENTRY_TYPE"   ) \
X_MACRO( id_part_entry_name    , "ID_PART_ENTRY_NAME"   ) \
X_MACRO( id_part_entry_number  , "ID_PART_ENTRY_NUMBER" ) \
X_MACRO( id_part_entry_offset  , "ID_PART_ENTRY_OFFSET" ) \
X_MACRO( id_part_entry_size    , "ID_PART_ENTRY_SIZE"   ) \
X_MACRO( id_part_entry_disk    , "ID_PART_ENTRY_DISK"   )
// ... ...
struct partition_prop_key
{
#define X_MACRO(key,desc) \
	static constexpr const char *key = desc;
	ALTUN_UDEV_PARTITION_PROPERTY
#undef X_MACRO
};

#define ALTUN_UDEV_INPUT_PROPERTY \
X_MACRO( id_input               , "ID_INPUT"               ) \
X_MACRO( id_input_key           , "ID_INPUT_KEY"           ) \
X_MACRO( id_input_keyboard      , "ID_INPUT_KEYBOARD"      ) \
X_MACRO( id_input_mouse         , "ID_INPUT_MOUSE"         ) \
X_MACRO( id_input_touchpad      , "ID_INPUT_TOUCHPAD"      ) \
X_MACRO( id_input_touchscreen   , "ID_INPUT_TOUCHSCREEN"   ) \
X_MACRO( id_input_tablet        , "ID_INPUT_TABLET"        ) \
X_MACRO( id_input_joystick      , "ID_INPUT_JOYSTICK"      ) \
X_MACRO( id_input_switch        , "ID_INPUT_SWITCH"        ) \
X_MACRO( id_input_accelerometer , "ID_INPUT_ACCELEROMETER" ) \
X_MACRO( id_input_pointingstick , "ID_INPUT_POINTINGSTICK" ) \
X_MACRO( id_input_trackball     , "ID_INPUT_TRACKBALL"     )
// ... ...
struct input_prop_key
{
#define X_MACRO(key,desc) \
	static constexpr const char *key = desc;
	ALTUN_UDEV_INPUT_PROPERTY
#undef X_MACRO
};

#define ALTUN_UDEV_NET_PROPERTY \
X_MACRO( interface                 , "INTERFACE"                 ) \
X_MACRO( if_index                  , "IFINDEX"                   ) \
X_MACRO( id_net_driver             , "ID_NET_DRIVER"             ) \
X_MACRO( id_net_label_onboard      , "ID_NET_LABEL_ONBOARD"      ) \
X_MACRO( id_net_link_file          , "ID_NET_LINK_FILE"          ) \
X_MACRO( id_net_name               , "ID_NET_NAME"               ) \
X_MACRO( id_net_name_mac           , "ID_NET_NAME_MAC"           ) \
X_MACRO( id_net_name_onboard       , "ID_NET_NAME_ONBOARD"       ) \
X_MACRO( id_net_name_onboard_label , "ID_NET_NAME_ONBOARD_LABEL" ) \
X_MACRO( id_net_name_path          , "ID_NET_NAME_PATH"          ) \
X_MACRO( id_net_name_slot          , "ID_NET_NAME_SLOT"          ) \
X_MACRO( id_net_naming_scheme      , "ID_NET_NAMING_SCHEME"      )
// ... ...
struct net_prop_key
{
#define X_MACRO(key,desc) \
	static constexpr const char *key = desc;
	ALTUN_UDEV_NET_PROPERTY
#undef X_MACRO
};

struct prop_key :
	basic_prop_key, hardware_prop_key,
	bus_prop_key, usb_prop_key,
	filesystem_prop_key, partition_prop_key,
	input_prop_key, net_prop_key {};

using properties_t = std::map<std::string_view,std::string_view>;

} //namespace altun::udev

#endif //__linux__
#endif //ALTUN_LINUX_UDEV_PROPERTIES_H
