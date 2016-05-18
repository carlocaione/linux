/*
 * Amlogic Secure Monitor driver
 *
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

#include <stdarg.h>
#include <asm/cacheflush.h>
#include <asm/compiler.h>
#include <linux/arm-smccc.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/smp.h>

#include <linux/firmware/meson/meson_sm.h>

#define SM_MEM_SIZE	0x1000

/*
 * To read from / write to the secure monitor we use two bounce buffers. The
 * physical addresses of the two buffers are obtained by querying the secure
 * monitor itself.
 */

static u32 sm_phy_in_base;
static u32 sm_phy_out_base;

static void __iomem *sm_sharemem_in_base;
static void __iomem *sm_sharemem_out_base;

struct meson_sm_data {
	u32 cmd;
	u32 arg0;
	u32 arg1;
	u32 arg2;
	u32 arg3;
	u32 arg4;
	u32 ret;
};

static void __meson_sm_call(void *info)
{
	struct meson_sm_data *data = info;
	struct arm_smccc_res res;

	arm_smccc_smc(data->cmd,
		      data->arg0, data->arg1, data->arg2,
		      data->arg3, data->arg4, 0, 0, &res);
	data->ret = res.a0;
}

u32 meson_sm_call(u32 cmd, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4)
{
	struct meson_sm_data data;

	data.cmd = cmd;
	data.arg0 = arg0;
	data.arg1 = arg1;
	data.arg2 = arg2;
	data.arg3 = arg3;
	data.arg4 = arg4;
	data.ret = 0;

	__meson_sm_call(&data);

	return data.ret;
}

u32 meson_sm_call_read(void *buffer, u32 cmd, u32 arg0, u32 arg1,
		       u32 arg2, u32 arg3, u32 arg4)
{
	u32 size;

	size = meson_sm_call(cmd, arg0, arg1, arg2, arg3, arg4);

	if (!size || size > SM_MEM_SIZE)
		return -EINVAL;

	memcpy(buffer, sm_sharemem_out_base, size);
	return size;
}

u32 meson_sm_call_write(void *buffer, unsigned int b_size, u32 cmd, u32 arg0,
			u32 arg1, u32 arg2, u32 arg3, u32 arg4)
{
	u32 size;

	if (b_size > SM_MEM_SIZE)
		return -EINVAL;

	memcpy(sm_sharemem_in_base, buffer, b_size);

	size = meson_sm_call(cmd, arg0, arg1, arg2, arg3, arg4);

	if (!size)
		return -EINVAL;

	return size;
}

static int meson_sm_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int cmd_in, cmd_out;

	if (of_property_read_u32(np, "amlogic,sm-cmd-input-base", &cmd_in))
		return -EINVAL;

	if (of_property_read_u32(np, "amlogic,sm-cmd-output-base", &cmd_out))
		return -EINVAL;

	sm_phy_in_base = meson_sm_call(cmd_in, 0, 0, 0, 0, 0);
	sm_sharemem_in_base = ioremap_cache(sm_phy_in_base, SM_MEM_SIZE);
	if (!sm_sharemem_in_base)
		return -EINVAL;

	sm_phy_out_base = meson_sm_call(cmd_out, 0, 0, 0, 0, 0);
	sm_sharemem_out_base = ioremap_cache(sm_phy_out_base, SM_MEM_SIZE);
	if (!sm_sharemem_out_base) {
		iounmap(sm_sharemem_in_base);
		return -EINVAL;
	}

	return 0;
}

static const struct of_device_id meson_sm_ids[] = {
	{ .compatible = "amlogic,meson-sm" },
	{ /* sentinel */},
};
MODULE_DEVICE_TABLE(of, meson_sm_ids);

static struct platform_driver meson_sm_platform_driver = {
	.probe	= meson_sm_probe,
	.driver	= {
		.name		= "secmon",
		.of_match_table	= meson_sm_ids,
	},
};
module_platform_driver(meson_sm_platform_driver);

MODULE_AUTHOR("Carlo Caione <carlo@endlessm.com>");
MODULE_DESCRIPTION("Amlogic secure monitor driver");
MODULE_LICENSE("GPL");
