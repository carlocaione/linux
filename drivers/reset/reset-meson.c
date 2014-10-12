/*
 * Copyright 2014 Carlo Caione <carlo@caione.org>
 *
 * based on
 * Steffen Trumtrar Reset Controller driver
 *
 * Copyright 2014 Steffen Trumtrar
 *
 * Steffen Trumtrar <s.trumtrar@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#define MESON_RST_OFFSET	0x00

struct meson_reset_data {
	spinlock_t			lock;
	void __iomem			*membase;
	struct reset_controller_dev	rcdev;
};

static int meson_reset_assert(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	struct meson_reset_data *data = container_of(rcdev,
						     struct meson_reset_data,
						     rcdev);
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&data->lock, flags);

	reg = readl(data->membase + MESON_RST_OFFSET);
	writel(reg | BIT(id), data->membase + MESON_RST_OFFSET);

	spin_unlock_irqrestore(&data->lock, flags);

	return 0;
}

static int meson_reset_deassert(struct reset_controller_dev *rcdev,
				unsigned long id)
{
	struct meson_reset_data *data = container_of(rcdev,
						     struct meson_reset_data,
						     rcdev);

	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&data->lock, flags);

	reg = readl(data->membase + MESON_RST_OFFSET);
	writel(reg & ~BIT(id), data->membase + MESON_RST_OFFSET);

	spin_unlock_irqrestore(&data->lock, flags);

	return 0;
}

static int meson_reset_dev(struct reset_controller_dev *rcdev, unsigned long id)
{
	int err;

	err = meson_reset_assert(rcdev, id);
	if (err)
		return err;

	return meson_reset_deassert(rcdev, id);
}

static struct reset_control_ops meson_reset_ops = {
	.assert		= meson_reset_assert,
	.deassert	= meson_reset_deassert,
	.reset		= meson_reset_dev,
};

static int meson_reset_probe(struct platform_device *pdev)
{
	struct meson_reset_data *data;
	struct resource *res;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->membase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(data->membase))
		return PTR_ERR(data->membase);

	spin_lock_init(&data->lock);

	platform_set_drvdata(pdev, data);

	data->rcdev.owner = THIS_MODULE;
	data->rcdev.nr_resets = BITS_PER_LONG;
	data->rcdev.ops = &meson_reset_ops;
	data->rcdev.of_node = pdev->dev.of_node;

	return reset_controller_register(&data->rcdev);
}

static int meson_reset_remove(struct platform_device *pdev)
{
	struct meson_reset_data *data = platform_get_drvdata(pdev);

	reset_controller_unregister(&data->rcdev);

	return 0;
}

static const struct of_device_id meson_reset_dt_ids[] = {
	{ .compatible = "amlogic,meson6-rst-mgr-ao", },
	{ /* sentinel */ },
};

static struct platform_driver meson_reset_driver = {
	.probe	= meson_reset_probe,
	.remove	= meson_reset_remove,
	.driver = {
		.name		= "meson-reset",
		.of_match_table	= meson_reset_dt_ids,
	},
};
module_platform_driver(meson_reset_driver);

MODULE_AUTHOR("Carlo Caione <carlo@caione.org>");
MODULE_DESCRIPTION("Meson Reset Controller Driver");
MODULE_LICENSE("GPL");
