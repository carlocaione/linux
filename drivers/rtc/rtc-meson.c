#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/reset.h>
#include <linux/types.h>

#define	MESON_RTC_ADDR0		0x00
#define	MESON_RTC_ADDR1		0x04
#define	MESON_RTC_ADDR2		0x08
#define	MESON_RTC_ADDR3		0x0c
#define	MESON_RTC_ADDR4		0x10

#define MESON_RTC_START_SER	BIT(17)
#define MESON_RTC_WAIT_SER	BIT(22)
#define MESON_RTC_SDI		BIT(2)
#define MESON_RTC_SEN		BIT(1)
#define MESON_RTC_SCLK		BIT(0)

#define MESON_RTC_S_READY	BIT(1)

#define MESON_RTC_SDO		BIT(0)

#define MESON_STATIC_BIAS_CUR	(0x5 << 1)
#define MESON_STATIC_VOLTAGE	(0x3 << 11)

#define MESON_ADDR_SIZE		3
#define MESON_DATA_SIZE		32

#define MESON_RTC_SI_RTC_COUNT	0

struct meson_rtc_dev {
	struct rtc_device *rtc;
	void __iomem *base;
	struct mutex lock;
	struct device *dev;
};

static void meson_rtc_reset_bus(struct meson_rtc_dev *rtc_dev)
{
	u32 tmp;

	tmp = readl(rtc_dev->base + MESON_RTC_ADDR0);
	tmp &= ~(MESON_RTC_SDI | MESON_RTC_SEN | MESON_RTC_SCLK);
	writel(tmp, rtc_dev->base + MESON_RTC_ADDR0);
}

static int meson_rtc_wait_bus(struct meson_rtc_dev *rtc_dev, unsigned int ms_timeout)
{
	const unsigned long timeout = jiffies + msecs_to_jiffies(ms_timeout);
	u32 reg;

	do {
		reg = readl(rtc_dev->base + MESON_RTC_ADDR1);
		reg &= MESON_RTC_S_READY;

		if (reg == MESON_RTC_S_READY)
			return 0;

	} while (time_before(jiffies, timeout));

	return -ETIMEDOUT;
}

static void meson_rtc_send_bit_bus(struct meson_rtc_dev *rtc_dev, unsigned int bit,
			       unsigned int val)
{
	u32 reg;

	reg = readl(rtc_dev->base + MESON_RTC_ADDR0);

	if (val)
		reg |= bit;
	else
		reg &= ~bit;

	writel(reg, rtc_dev->base + MESON_RTC_ADDR0);
}

static void meson_rtc_sclk_pulse(struct meson_rtc_dev *rtc_dev)
{
	udelay(5);
	meson_rtc_send_bit_bus(rtc_dev, MESON_RTC_SCLK, 0);
	udelay(5);
	meson_rtc_send_bit_bus(rtc_dev, MESON_RTC_SCLK, 1);
}

static void meson_rtc_send_bit_sdi_bus(struct meson_rtc_dev *rtc_dev, unsigned int val)
{
	meson_rtc_send_bit_bus(rtc_dev, MESON_RTC_SDI, val);
	meson_rtc_sclk_pulse(rtc_dev);
}

static void meson_rtc_send_data_bus(struct meson_rtc_dev *rtc_dev, unsigned long val,
			     unsigned int size)
{
	unsigned int cursor = 1 << (size - 1);

	while (cursor) {
		meson_rtc_send_bit_sdi_bus(rtc_dev, val & cursor);
		cursor >>= 1;
	}
}

static int meson_rtc_si0_write(struct meson_rtc_dev *rtc_dev, unsigned int addr, unsigned long data)
{
	int ret = 0;
	int trial = 3;

	mutex_lock(&rtc_dev->lock);

	meson_rtc_reset_bus(rtc_dev);
	while (meson_rtc_wait_bus(rtc_dev, 300)) {
		if (--trial) 
			ret = device_reset(rtc_dev->dev);
		else
			ret = -ETIMEDOUT;

		if (ret)
			goto out;
	}

	meson_rtc_send_bit_bus(rtc_dev, MESON_RTC_SEN, 1);
	meson_rtc_send_data_bus(rtc_dev, data, MESON_DATA_SIZE);
	meson_rtc_send_data_bus(rtc_dev, addr, MESON_ADDR_SIZE);
	meson_rtc_send_bit_bus(rtc_dev, MESON_RTC_SEN, 0);
	meson_rtc_send_bit_sdi_bus(rtc_dev, 1);
	meson_rtc_send_bit_bus(rtc_dev, MESON_RTC_SDI, 0);

out:
	mutex_unlock(&rtc_dev->lock);

	return ret;
}

static void meson_recv_data_bus(struct meson_rtc_dev *rtc_dev, unsigned int size,
				unsigned long *data)
{
	int i;
	u32 reg;

	for (i = 0; i < size; i++) {
		meson_rtc_sclk_pulse(rtc_dev);
		reg = readl(rtc_dev->base + MESON_RTC_ADDR1);
		*data <<= 1;
		*data |= reg & MESON_RTC_SDO;
	}
}

static int meson_rtc_si0_read(struct meson_rtc_dev *rtc_dev, unsigned int addr, unsigned long *data)
{
	int ret = 0;
	int trial = 3;

	mutex_lock(&rtc_dev->lock);

	meson_rtc_reset_bus(rtc_dev);
	while (meson_rtc_wait_bus(rtc_dev, 300)) {
		if (--trial)
			ret = device_reset(rtc_dev->dev);
		else
			ret = -ETIMEDOUT;

		if (ret)
			goto out;
	}

	meson_rtc_send_bit_bus(rtc_dev, MESON_RTC_SEN, 1);
	meson_rtc_send_data_bus(rtc_dev, addr, MESON_ADDR_SIZE);
	meson_rtc_send_bit_bus(rtc_dev, MESON_RTC_SEN, 0);
	meson_rtc_send_bit_sdi_bus(rtc_dev, 0);
	meson_rtc_send_bit_bus(rtc_dev, MESON_RTC_SDI, 0);

	meson_recv_data_bus(rtc_dev, MESON_DATA_SIZE, data);

out:
	mutex_unlock(&rtc_dev->lock);

	return ret;
}

void meson_rtc_si1_write(u16 data, struct meson_rtc_dev *rtc_dev)
{
	u32 tmp = 0;

	mutex_lock(&rtc_dev->lock);

	/* high bits */
	tmp = (data >> 8);
	writel(tmp, rtc_dev->base + MESON_RTC_ADDR4);

	/* low bits */
	tmp = readl(rtc_dev->base + MESON_RTC_ADDR0);
	tmp &= ~(0xff << 24);
	tmp |= (data & 0xff) << 24;

	/* start serializer */
	tmp |= MESON_RTC_START_SER;

	writel(tmp, rtc_dev->base + MESON_RTC_ADDR0);

	/* wait for serializer */
	while (readl(rtc_dev->base + MESON_RTC_ADDR0) & MESON_RTC_WAIT_SER)
		cpu_relax();

	mutex_unlock(&rtc_dev->lock);
}

static int meson_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long data = 0;
	unsigned int ret = 0;
	struct meson_rtc_dev *rtc_dev = dev_get_drvdata(dev);

	ret = meson_rtc_si0_read(rtc_dev, MESON_RTC_SI_RTC_COUNT, &data);
	if (!ret)
		rtc_time_to_tm(data, tm);

	return ret;
}

static int meson_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long data = 0;
	unsigned int ret = 0;
	struct meson_rtc_dev *rtc_dev = dev_get_drvdata(dev);

	rtc_tm_to_time(tm, &data);
	ret = meson_rtc_si0_write(rtc_dev, MESON_RTC_SI_RTC_COUNT, data);

	return ret;
}

static const struct rtc_class_ops meson_rtc_ops = {
	.read_time	= meson_rtc_read_time,
	.set_time	= meson_rtc_set_time,
};

static int meson_rtc_probe(struct platform_device *pdev)
{
	struct meson_rtc_dev *rtc_dev;
	struct resource *res;

	rtc_dev = devm_kzalloc(&pdev->dev, sizeof(*rtc_dev), GFP_KERNEL);
	if (!rtc_dev)
		return -ENOMEM;

	platform_set_drvdata(pdev, rtc_dev);
	rtc_dev->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rtc_dev->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rtc_dev->base))
		return PTR_ERR(rtc_dev->base);

	mutex_init(&rtc_dev->lock);

	meson_rtc_si1_write(MESON_STATIC_BIAS_CUR | MESON_STATIC_VOLTAGE,
			    rtc_dev);

	rtc_dev->rtc = devm_rtc_device_register(&pdev->dev, "rtc_meson",
						&meson_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc_dev->rtc)) {
		dev_err(&pdev->dev, "unable to register the device\n");
		return PTR_ERR(rtc_dev->rtc);
	}

	return 0;
}

static const struct of_device_id meson_rtc_dt_ids[] = {
	{ .compatible = "amlogic,meson6-rtc" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, meson_rtc_dt_ids);

static struct platform_driver meson_rtc_driver = {
	.probe		= meson_rtc_probe,
	.driver		= {
		.name		= "meson6-rtc",
		.of_match_table = meson_rtc_dt_ids,
	},
};

module_platform_driver(meson_rtc_driver);

MODULE_DESCRIPTION("Meson RTC driver");
MODULE_AUTHOR("Carlo Caione <carlo@caione.org>");
MODULE_LICENSE("GPL");
