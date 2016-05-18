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

#ifndef _MESON_SM_H_
#define _MESON_SM_H_

u32 meson_sm_call(u32 cmd, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4);
u32 meson_sm_call_read(void *buffer, u32 cmd, u32 arg0, u32 arg1, u32 arg2,
		       u32 arg3, u32 arg4);
u32 meson_sm_call_write(void *buffer, unsigned int b_size, u32 cmd, u32 arg0,
			u32 arg1, u32 arg2, u32 arg3, u32 arg4);

#endif /* _MESON_SM_H_ */
