#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

static const struct of_device_id test_dt_ids[] = {
	{ .compatible = "pwm-test-irq" },
	{ }
};
MODULE_DEVICE_TABLE(of, test_dt_ids);

static irqreturn_t test_irq_handler(int irq, void *dev_instance)
{
	printk(KERN_EMERG "--> *\n");
	return IRQ_HANDLED;
}

static int test_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int irq, err;

	irq = irq_of_parse_and_map(dev->of_node, 0);
	if (!irq) {
		dev_err(dev, "failed to get irq\n");
		return -ENODEV;
	}

	/* Register interrupt handler for device */
	err = devm_request_irq(dev, irq, test_irq_handler, 0,
			       "Test", NULL);
	if (err) {
		dev_err(dev, "could not allocate IRQ\n");
		return -EINVAL;
	}

	return 0;
}

static struct platform_driver test_driver = {
	.driver = {
		.name = "test-irq",
		.of_match_table = test_dt_ids,
	},
	.probe = test_probe,
};
module_platform_driver(test_driver);

MODULE_AUTHOR("Beniamino Galvani <b.galvani@gmail.com>");
MODULE_DESCRIPTION("IRQ test driver");
MODULE_LICENSE("GPL v2");
