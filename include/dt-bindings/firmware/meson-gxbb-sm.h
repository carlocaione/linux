/*
 * Copyright (C) 2016 Endless Mobile, Inc.
 * Author: Carlo Caione <carlo@endlessm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MESON_SM_GXBB_H_
#define _MESON_SM_GXBB_H_

#define SM_GET_SHARE_MEM_INPUT_BASE		0x82000020
#define SM_GET_SHARE_MEM_OUTPUT_BASE		0x82000021
#define SM_GET_REBOOT_REASON			0x82000022
#define SM_GET_SHARE_STORAGE_IN_BASE		0x82000023
#define SM_GET_SHARE_STORAGE_OUT_BASE		0x82000024
#define SM_GET_SHARE_STORAGE_BLOCK_BASE		0x82000025
#define SM_GET_SHARE_STORAGE_MESSAGE_BASE	0x82000026
#define SM_GET_SHARE_STORAGE_BLOCK_SIZE		0x82000027
#define SM_EFUSE_READ				0x82000030
#define SM_EFUSE_WRITE				0x82000031
#define SM_EFUSE_WRITE_PATTERN			0x82000032
#define SM_EFUSE_USER_MAX			0x82000033
#define SM_JTAG_ON				0x82000040
#define SM_JTAG_OFF				0x82000041
#define SM_SET_USB_BOOT_FUNC			0x82000043
#define SM_SECURITY_KEY_QUERY			0x82000060
#define SM_SECURITY_KEY_READ			0x82000061
#define SM_SECURITY_KEY_WRITE			0x82000062
#define SM_SECURITY_KEY_TELL			0x82000063
#define SM_SECURITY_KEY_VERIFY			0x82000064
#define SM_SECURITY_KEY_STATUS			0x82000065
#define SM_SECURITY_KEY_NOTIFY			0x82000066
#define SM_SECURITY_KEY_LIST			0x82000067
#define SM_SECURITY_KEY_REMOVE			0x82000068
#define SM_DEBUG_EFUSE_WRITE_PATTERN		0x820000F0
#define SM_DEBUG_EFUSE_READ_PATTERN		0x820000F1
#define SM_AML_DATA_PROCESS			0x820000FF

#endif /* _MESON_SM_GXBB_H_ */
