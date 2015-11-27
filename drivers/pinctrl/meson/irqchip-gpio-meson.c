/*
 * GPIO IRQ driver for Amlogic Meson SoCs
 *
 * Copyright (C) 2015 Endless Mobile, Inc.
 * Author: Carlo Caione <carlo@endlessm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Amlogic Meson SoCs have only a limited number of IRQs on the GIC side that
 * can be used for the GPIOs.
 *
 * GPIO# -> [mux] -> [polarity] -> [filter] -> [edge select] -> GIC IRQ#
 *
 * The GPIO used to trigger the IRQ is chosen by filling a bitmask in the 'mux'
 * registers.
 *
 * The bitmask position determines the IRQ
 *
 * GPIO -> [mux1 [7:0]]   -> ... -> GIC / GPIO IRQ0
 * GPIO -> [mux1 [15:8]]  -> ... -> GIC / GPIO IRQ1
 * ...
 * GPIO -> [mux2 [23:16]] -> ... -> GIC / GPIO IRQ6
 * ...
 *
 * The bitmask value determines the GPIO used to trigger the IRQ
 *
 * GPIOX_21 -> 118 in the mux# bitmask register
 * ...
 * GPIOH_9  -> 23 in the mux# bitmask register
 * ...
 *
 */

#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/of_irq.h>

#include "pinctrl-meson.h"

#define REG_EDGE_POL		0x00
#define REG_GPIO_SEL0		0x04
#define REG_GPIO_SEL1		0x08
#define REG_FILTER		0x0c

#define IRQ_FREE		(-1)

#define REG_EDGE_POL_MASK(x)	(BIT(x) | BIT(16 + (x)))
#define REG_EDGE_POL_EDGE(x)	BIT(x)
#define REG_EDGE_POL_LOW(x)	BIT(16 + (x))

static int meson_get_gic_irq(struct meson_pinctrl *pc, int hwirq)
{
	int i = 0;

	for (i = 0; i < pc->num_gic_irqs; i++) {
		if (pc->irq_map[i] == hwirq)
			return i;
	}

	return -1;
}

static int meson_irq_set_type(struct irq_data *data, unsigned int type)
{
	struct meson_pinctrl *pc = irq_data_get_irq_chip_data(data);
	u32 val = 0;
	int index;

	dev_dbg(pc->dev, "set type of hwirq %lu to %u\n", data->hwirq, type);
	spin_lock(&pc->lock);
	index = meson_get_gic_irq(pc, data->hwirq);

	if (index < 0) {
		spin_unlock(&pc->lock);
		dev_err(pc->dev, "hwirq %lu not allocated\n", data->hwirq);
		return -EINVAL;
	}

	if (type == IRQ_TYPE_EDGE_FALLING || type == IRQ_TYPE_EDGE_RISING)
		val |= REG_EDGE_POL_EDGE(index);
	if (type == IRQ_TYPE_EDGE_FALLING || type == IRQ_TYPE_LEVEL_LOW)
		val |= REG_EDGE_POL_LOW(index);

	regmap_update_bits(pc->reg_irq, REG_EDGE_POL, REG_EDGE_POL_MASK(index),
			   val);
	spin_unlock(&pc->lock);

	if (type == IRQ_TYPE_LEVEL_LOW)
		type = IRQ_TYPE_LEVEL_HIGH;
	else if (type == IRQ_TYPE_EDGE_FALLING)
		type = IRQ_TYPE_EDGE_RISING;

	return irq_chip_set_type_parent(data, type);
}

int meson_irq_request_resources(struct irq_data *data)
{
	struct meson_pinctrl *pc = irq_data_get_irq_chip_data(data);
	struct meson_domain *domain;
	struct meson_bank *bank;

	if (meson_get_domain_and_bank(pc, data->hwirq, &domain, &bank))
		return -EINVAL;

	if (gpiochip_lock_as_irq(&domain->chip, data->hwirq))
		return -EINVAL;

	return 0;
}

void meson_irq_release_resources(struct irq_data *data)
{
	struct meson_pinctrl *pc = irq_data_get_irq_chip_data(data);
	struct meson_domain *domain;
	struct meson_bank *bank;

	if (meson_get_domain_and_bank(pc, data->hwirq, &domain, &bank))
		return;

	gpiochip_unlock_as_irq(&domain->chip, data->hwirq);
}

static struct irq_chip meson_irq_chip = {
	.name			= "meson-gpio-irqchip",
	.irq_mask		= irq_chip_mask_parent,
	.irq_unmask		= irq_chip_unmask_parent,
	.irq_eoi		= irq_chip_eoi_parent,
	.irq_set_type		= meson_irq_set_type,
	.irq_retrigger		= irq_chip_retrigger_hierarchy,
	.irq_set_affinity	= irq_chip_set_affinity_parent,
	.irq_request_resources	= meson_irq_request_resources,
	.irq_release_resources	= meson_irq_release_resources,
};

static int meson_map_gic_irq(struct irq_domain *irq_domain,
			     irq_hw_number_t hwirq)
{
	struct meson_pinctrl *pc = irq_domain->host_data;
	struct meson_domain *domain;
	struct meson_bank *bank;
	int index, reg, ret;

	ret = meson_get_domain_and_bank(pc, hwirq, &domain, &bank);
	if (ret)
		return ret;

	spin_lock(&pc->lock);

	index = meson_get_gic_irq(pc, IRQ_FREE);
	if (index < 0) {
		spin_unlock(&pc->lock);
		dev_err(pc->dev, "no free GIC interrupt found");
		return -ENOSPC;
	}

	dev_dbg(pc->dev, "found free GIC interrupt %d\n", index);
	pc->irq_map[index] = hwirq;

	/* Setup IRQ mapping */
	reg = index < 4 ? REG_GPIO_SEL0 : REG_GPIO_SEL1;
	regmap_update_bits(pc->reg_irq, reg, 0xff << (index % 4) * 8,
			  (bank->irq + hwirq - bank->first) << (index % 4) * 8);

	/* Set filter to the default, undocumented value of 7 */
	regmap_update_bits(pc->reg_irq, REG_FILTER, 0xf << index * 4,
			   7 << index * 4);

	spin_unlock(&pc->lock);

	return index;
}

static int meson_irq_domain_alloc(struct irq_domain *domain, unsigned int irq,
				  unsigned int nr_irqs, void *arg)
{
	struct meson_pinctrl *pc = domain->host_data;
	struct irq_fwspec *irq_data = arg;
	struct irq_fwspec gic_data;
	irq_hw_number_t hwirq;
	int index, ret, i;

	if (irq_data->param_count != 2)
		return -EINVAL;

	hwirq = irq_data->param[0];
	dev_dbg(pc->dev, "%s irq %d, nr %d, hwirq %lu\n",
			__func__, irq, nr_irqs, hwirq);

	for (i = 0; i < nr_irqs; i++) {
		index = meson_map_gic_irq(domain, hwirq + i);
		if (index < 0)
			return index;

		irq_domain_set_hwirq_and_chip(domain, irq + i,
					      hwirq + i,
					      &meson_irq_chip,
					      pc);

		gic_data.param_count = 3;
		gic_data.fwnode = domain->parent->fwnode;
		gic_data.param[0] = 0; /* SPI */
		gic_data.param[1] = pc->gic_irqs[index];
		gic_data.param[1] = IRQ_TYPE_EDGE_RISING;

		ret = irq_domain_alloc_irqs_parent(domain, irq + i, nr_irqs,
						   &gic_data);
	}

	return 0;
}

static void meson_irq_domain_free(struct irq_domain *domain, unsigned int irq,
				  unsigned int nr_irqs)
{
	struct meson_pinctrl *pc = domain->host_data;
	struct irq_data *irq_data;
	int index, i;

	spin_lock(&pc->lock);
	for (i = 0; i < nr_irqs; i++) {
		irq_data = irq_domain_get_irq_data(domain, irq + i);
		index = meson_get_gic_irq(pc, irq_data->hwirq);
		if (index < 0)
			continue;
		pc->irq_map[index] = IRQ_FREE;
	}
	spin_unlock(&pc->lock);

	irq_domain_free_irqs_parent(domain, irq, nr_irqs);
}

static int meson_irq_domain_translate(struct irq_domain *d,
				      struct irq_fwspec *fwspec,
				      unsigned long *hwirq,
				      unsigned int *type)
{
	if (is_of_node(fwspec->fwnode)) {
		if (fwspec->param_count != 2)
			return -EINVAL;

		*hwirq = fwspec->param[0];
		*type = fwspec->param[1];

		return 0;
	}

	return -EINVAL;
}

static struct irq_domain_ops meson_irq_domain_ops = {
	.alloc		= meson_irq_domain_alloc,
	.free		= meson_irq_domain_free,
	.translate	= meson_irq_domain_translate,
};

int meson_gpio_irq_init(struct meson_pinctrl *pc)
{
	struct device_node *node = pc->dev->of_node;
	struct device_node *parent_node;
	struct irq_domain *parent_domain;
	const __be32 *irqs;
	int i, size;

	parent_node = of_irq_find_parent(node);
	if (!parent_node) {
		dev_err(pc->dev, "can't find parent interrupt controller\n");
		return -EINVAL;
	}

	parent_domain = irq_find_host(parent_node);
	if (!parent_domain) {
		dev_err(pc->dev, "can't find parent IRQ domain\n");
		return -EINVAL;
	}

	pc->reg_irq = meson_map_resource(pc, node, "irq");
	if (!pc->reg_irq) {
		dev_err(pc->dev, "can't find irq registers\n");
		return -EINVAL;
	}

	irqs = of_get_property(node, "amlogic,irqs-gpio", &size);
	if (!irqs) {
		dev_err(pc->dev, "no parent interrupts specified\n");
		return -EINVAL;
	}
	pc->num_gic_irqs = size / sizeof(__be32);

	pc->irq_map = devm_kmalloc(pc->dev, sizeof(int) * pc->num_gic_irqs,
				   GFP_KERNEL);
	if (!pc->irq_map)
		return -ENOMEM;

	pc->gic_irqs = devm_kzalloc(pc->dev, sizeof(int) * pc->num_gic_irqs,
				    GFP_KERNEL);
	if (!pc->gic_irqs)
		return -ENOMEM;

	for (i = 0; i < pc->num_gic_irqs; i++) {
		pc->irq_map[i] = IRQ_FREE;
		of_property_read_u32_index(node, "amlogic,irqs-gpio", i,
					   &pc->gic_irqs[i]);
	}

	pc->irq_domain = irq_domain_add_hierarchy(parent_domain, 0,
						  pc->data->last_pin,
						  node, &meson_irq_domain_ops,
						  pc);
	if (!pc->irq_domain)
		return -EINVAL;

	return 0;
}
