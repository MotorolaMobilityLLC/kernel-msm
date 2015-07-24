/*
 * Copyright (C) 2015 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/mtd/mtd.h>
#include <linux/vmalloc.h>

#define APBA_FIRMWARE_NAME ("apba.bin")
#define APBA_NUM_GPIOS (8)
#define APBA_MAX_SEQ   (APBA_NUM_GPIOS*3*2)

struct apba_seq {
	u32 val[APBA_MAX_SEQ];
	size_t len;
};

struct apba_ctrl {
	struct device *dev;
	struct clk *mclk;
	int gpio_cnt;
	int gpios[APBA_NUM_GPIOS];
	int int_index;
	int irq;
	struct apba_seq enable_seq;
	struct apba_seq disable_seq;
	struct apba_seq wake_seq;
} *g_ctrl;

static inline struct apba_ctrl *apba_sysfs_to_ctrl(struct device *dev)
{
	return g_ctrl;
}

static struct mtd_info *mtd_info;

static int apba_mtd_erase(struct mtd_info *mtd_info,
	 unsigned int start, unsigned int len)
{
	int err;
	struct erase_info ei = {0};

	ei.addr = start;
	ei.len = len;
	ei.mtd = mtd_info;
	err = mtd_info->_erase(mtd_info, &ei);
	return err;
}

static int apba_init_mtd_module(void)
{
	int num;

	for (num = 0; num < 16; num++) {
		mtd_info = get_mtd_device(NULL, num);
		if (IS_ERR(mtd_info)) {
			pr_debug("%s: No device for num %d\n", __func__, num);
			continue;
		}

		if (mtd_info->type == MTD_ABSENT) {
			put_mtd_device(mtd_info);
			continue;
		}

		if (strcmp(mtd_info->name, "apba")) {
			put_mtd_device(mtd_info);
			continue;
		}

		pr_debug("%s: MTD name: %s\n", __func__, mtd_info->name);
		pr_debug("%s: MTD type: %d\n", __func__, mtd_info->type);
		pr_debug("%s: MTD total size : %ld bytes\n", __func__,
			 (long)mtd_info->size);
		pr_debug("%s: MTD erase size : %ld bytes\n", __func__,
			 (long)mtd_info->erasesize);
		return 0;
	}

	mtd_info = NULL;
	return -ENXIO;
}

static int apba_parse_seq(struct device *dev, const char *name,
	struct apba_seq *seq)
{
	int ret;
	int cnt = 0;
	struct property *pp = of_find_property(dev->of_node, name, &cnt);

	cnt /= sizeof(u32);
	if (!pp || cnt == 0 || cnt > seq->len || cnt % 3) {
		pr_err("%s:%d, error reading property %s, cnt = %d\n",
			__func__, __LINE__, name, cnt);
		ret = -EINVAL;
	} else {
		ret = of_property_read_u32_array(dev->of_node, name,
			seq->val, cnt);
		if (ret) {
			pr_err("%s:%d, unable to read %s, ret = %d\n",
				__func__, __LINE__, name, ret);
		} else {
			seq->len = cnt;
		}
	}

	return ret;
}

static void apba_seq(struct apba_ctrl *ctrl, struct apba_seq *seq)
{
	size_t i;

	for (i = 0; i < seq->len; i += 3) {
		u32 index = seq->val[i];
		int value = (int)seq->val[i+1];
		unsigned long delay = (unsigned long)seq->val[i+2];

		/* Set a gpio (if valid). */
		if (index < ARRAY_SIZE(ctrl->gpios)) {
			int gpio = ctrl->gpios[index];

			if (gpio_is_valid(gpio)) {
				pr_debug("%s: set gpio=%d, value=%u\n",
					__func__, gpio, value);
				gpio_set_value(gpio, value);
			}
		}

		/* Delay (if valid). */
		if (delay) {
			usleep_range(delay * 1000, delay * 1000);
			pr_debug("%s: delay=%lu\n",
				__func__, delay);
		}
	}
}

static void apba_enable(struct apba_ctrl *ctrl, bool enable)
{
	if (enable)
		apba_seq(ctrl, &ctrl->enable_seq);
	else
		apba_seq(ctrl, &ctrl->disable_seq);
}

static int apba_erase_firmware(struct apba_ctrl *ctrl)
{
	int err = 0;

	if (!ctrl)
		return -EINVAL;

	apba_enable(ctrl, false);
	err = apba_init_mtd_module();
	if (err < 0) {
		pr_err("%s: mtd init module failed err=%d\n", __func__, err);
		goto no_mtd;
	}

	/* Erase the flash */
	err = apba_mtd_erase(mtd_info, 0, mtd_info->size);
	if (err < 0) {
		pr_err("%s: mtd erase failed err=%d\n", __func__, err);
		goto cleanup;
	}

cleanup:
	put_mtd_device(mtd_info);
	apba_enable(ctrl, true);
no_mtd:
	return err;
}

static ssize_t apba_erase_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t apba_erase_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct apba_ctrl *ctrl = platform_get_drvdata(pdev);
	int err;
	unsigned long val;

	if (kstrtoul(buf, 10, &val) < 0)
		return -EINVAL;
	else if (val != 1)
		return -EINVAL;

	err = apba_erase_firmware(ctrl);
	if (err < 0)
		pr_err("%s: flashing erase err=%d\n", __func__, err);

	return count;
}

static DEVICE_ATTR(erase_firmware, S_IRUGO | S_IWUSR,
	apba_erase_firmware_show, apba_erase_firmware_store);

static int apba_flash_firmware(const struct firmware *fw,
		 struct apba_ctrl *ctrl)
{
	int err = 0;
	unsigned char *data = NULL;
	size_t retlen = 0;

	if (!fw || !ctrl)
		return -EINVAL;

	apba_enable(ctrl, false);
	err = apba_init_mtd_module();
	if (err < 0) {
		pr_err("%s: mtd init module failed err=%d\n", __func__, err);
		goto no_mtd;
	}

	/* Erase the flash */
	err = apba_mtd_erase(mtd_info, 0, mtd_info->size);
	if (err < 0) {
		pr_err("%s: mtd erase failed err=%d\n", __func__, err);
		goto cleanup;
	}

	data = vmalloc(fw->size);
	if (!data) {
		err = -ENOMEM;
		goto cleanup;
	}

	memcpy((char *)data, (char *)fw->data, fw->size);
	err = mtd_info->_write(mtd_info, 0, fw->size, &retlen, data);

	/* FIXME: temp delay for regulator enable from rpm */
	if (!err)
		msleep(25000);

	vfree(data);
cleanup:
	put_mtd_device(mtd_info);
	apba_enable(ctrl, true);
no_mtd:
	return err;
}

static ssize_t apba_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t apba_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct apba_ctrl *ctrl = platform_get_drvdata(pdev);
	int err;
	const struct firmware *fw = NULL;
	unsigned long val;

	if (kstrtoul(buf, 10, &val) < 0)
		return -EINVAL;
	else if (val != 1)
		return -EINVAL;

	err = request_firmware(&fw, APBA_FIRMWARE_NAME, ctrl->dev);
	if (err < 0)
		return err;

	if (!fw || !fw->size)
		return -EINVAL;

	pr_debug("%s: size=%zu data=%p\n", __func__, fw->size, fw->data);

	err = apba_flash_firmware(fw, ctrl);
	if (err < 0)
		pr_err("%s: flashing failed err=%d\n", __func__, err);

	release_firmware(fw);
	return count;
}

static DEVICE_ATTR(flash_firmware, S_IRUGO | S_IWUSR,
	apba_firmware_show, apba_firmware_store);

static void apba_firmware_callback(const struct firmware *fw,
					 void *context)
{
	struct apba_ctrl *ctrl = (struct apba_ctrl *)context;
	int err;

	if (!ctrl) {
		pr_err("%s: invalid ctrl\n", __func__);
		return;
	}

	if (!fw) {
		pr_err("%s: no firmware available\n", __func__);
		apba_enable(ctrl, true);
		return;
	}

	pr_debug("%s: size=%zu data=%p\n", __func__, fw->size, fw->data);

	/* TODO: extract the version from the binary, check the version,
		  apply the firmware. */

	err = apba_flash_firmware(fw, ctrl);
	if (err < 0)
		pr_err("%s: flashing failed err=%d\n", __func__, err);

	/* TODO: notify system, in case of error */

	release_firmware(fw);
}

static irqreturn_t apba_isr(int irq, void *data)
{
	struct apba_ctrl *ctrl = data;
	int value = gpio_get_value(ctrl->gpios[ctrl->int_index]);

	pr_debug("%s: ctrl=%p, value=%d\n", __func__, ctrl, value);

	return IRQ_HANDLED;
}

static int apba_int_setup(struct apba_ctrl *ctrl,
	struct device *dev)
{
	int ret;
	int gpio;
	unsigned int flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
			     IRQF_ONESHOT;

	ret = of_property_read_u32(dev->of_node,
		"mmi,int-index", &ctrl->int_index);
	if (ret) {
		dev_warn(dev, "%s:%d failed to read int index.\n",
			__func__, __LINE__);
	}

	gpio = ctrl->gpios[ctrl->int_index];
	ctrl->irq = gpio_to_irq(gpio);
	dev_dbg(dev, "%s: irq: gpio=%d irq=%d\n",
		__func__, gpio, ctrl->irq);

	ret = request_threaded_irq(ctrl->irq, NULL /* handler */,
		apba_isr, flags, "apba_ctrl", ctrl);
	if (ret) {
		dev_err(dev, "%s:%d irq request failed: %d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	enable_irq_wake(ctrl->irq);

	return ret;
}

static int apba_gpio_setup(struct apba_ctrl *ctrl,
	struct device *dev)
{
	int i;
	int gpio_cnt = of_gpio_count(dev->of_node);
	const char *label_prop = "mmi,gpio-labels";
	int label_cnt = of_property_count_strings(dev->of_node, label_prop);

	if (gpio_cnt > ARRAY_SIZE(ctrl->gpios)) {
		dev_err(dev, "%s:%d gpio count is greater than %zu.\n",
			__func__, __LINE__, ARRAY_SIZE(ctrl->gpios));
		return -EINVAL;
	}

	if (label_cnt != gpio_cnt) {
		dev_err(dev, "%s:%d label count does not match gpio count.\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < gpio_cnt; i++) {
		enum of_gpio_flags flags = 0;
		int gpio;
		const char *label = NULL;

		gpio = of_get_gpio_flags(dev->of_node, i, &flags);
		if (gpio < 0) {
			dev_err(dev, "%s:%d of_get_gpio failed: %d\n",
				__func__, __LINE__, gpio);
			return gpio;
		}

		if (i < label_cnt)
			of_property_read_string_index(dev->of_node, label_prop,
				i, &label);

		gpio_request_one(gpio, flags, label);
#ifdef DEBUG
		gpio_export(gpio, true);
		gpio_export_link(dev, label, gpio);
#endif

		dev_dbg(dev, "%s: gpio=%d, flags=0x%x, label=%s\n",
			__func__, gpio, flags, label);

		ctrl->gpio_cnt = gpio_cnt;
		ctrl->gpios[i] = gpio;
	}

	return 0;
}

static int apba_ctrl_probe(struct platform_device *pdev)
{
	struct apba_ctrl *ctrl;
	int ret;

	if (!pdev->dev.of_node) {
		/* Platform data not currently supported */
		dev_err(&pdev->dev, "%s: of devtree not found\n", __func__);
		return -EINVAL;
	}

	ctrl = devm_kzalloc(&pdev->dev, sizeof(*ctrl),
		GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	ctrl->dev = &pdev->dev;

	ctrl->mclk = devm_clk_get(&pdev->dev, "apba_mclk");
	if (IS_ERR(ctrl->mclk)) {
		dev_err(&pdev->dev, "%s: failed to get clock.\n", __func__);
		return PTR_ERR(ctrl->mclk);
	}

	ret = clk_prepare_enable(ctrl->mclk);
	if (ret) {
		dev_err(&pdev->dev, "%s:%d: failed to prepare clock.\n",
			__func__, __LINE__);
		return ret;
	}

	ret = apba_gpio_setup(ctrl, &pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "%s:%d: failed to read gpios.\n",
			__func__, __LINE__);
		return ret;
	}

	ret = apba_int_setup(ctrl, &pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "%s:%d: failed to setup interrupt.\n",
			__func__, __LINE__);
		return ret;
	}

	ctrl->enable_seq.len = ARRAY_SIZE(ctrl->enable_seq.val);
	ret = apba_parse_seq(&pdev->dev, "mmi,enable-seq",
		&ctrl->enable_seq);
	if (ret) {
		dev_err(&pdev->dev, "%s:%d failed to read enable sequence.\n",
			__func__, __LINE__);
		ctrl->enable_seq.len = 0;
		return ret;
	}

	ctrl->disable_seq.len = ARRAY_SIZE(ctrl->disable_seq.val);
	ret = apba_parse_seq(&pdev->dev, "mmi,disable-seq",
		&ctrl->disable_seq);
	if (ret) {
		dev_err(&pdev->dev, "%s:%d failed to read disable sequence.\n",
			__func__, __LINE__);
		ctrl->disable_seq.len = 0;
		return ret;
	}

	ctrl->wake_seq.len = ARRAY_SIZE(ctrl->wake_seq.val);
	ret = apba_parse_seq(&pdev->dev, "mmi,wake-seq",
		&ctrl->wake_seq);
	if (ret) {
		dev_err(&pdev->dev, "%s:%d failed to read wake sequence.\n",
			__func__, __LINE__);
		ctrl->wake_seq.len = 0;
		return ret;
	}

	ret = request_firmware_nowait(THIS_MODULE, true, APBA_FIRMWARE_NAME,
	    &pdev->dev, GFP_KERNEL, ctrl, apba_firmware_callback);
	if (ret) {
		dev_err(&pdev->dev, "%s:%d failed to request firmware.\n",
			__func__, __LINE__);
		return ret;
	}

	device_create_file(ctrl->dev, &dev_attr_erase_firmware);
	device_create_file(ctrl->dev, &dev_attr_flash_firmware);
	g_ctrl = ctrl;

	platform_set_drvdata(pdev, ctrl);

	return 0;
}

static int apba_ctrl_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id apba_ctrl_match[] = {
	{.compatible = "mmi,apba-ctrl",},
	{},
};
MODULE_DEVICE_TABLE(of, apba_ctrl_match);

static const struct platform_device_id apba_ctrl_id_table[] = {
	{"apba_ctrl", 0},
	{},
};
MODULE_DEVICE_TABLE(of, apba_ctrl_id_table);

static struct platform_driver apba_ctrl_driver = {
	.driver = {
		.name = "apba_ctrl",
		.owner = THIS_MODULE,
		.of_match_table = apba_ctrl_match,
	},
	.probe = apba_ctrl_probe,
	.remove = apba_ctrl_remove,
	.id_table = apba_ctrl_id_table,
};

static int __init apba_ctrl_init(void)
{
	return platform_driver_register(&apba_ctrl_driver);
}

static void __exit apba_ctrl_exit(void)
{
	platform_driver_unregister(&apba_ctrl_driver);
}

module_init(apba_ctrl_init);
module_exit(apba_ctrl_exit);

MODULE_ALIAS("platform:apba_ctrl");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola");
MODULE_DESCRIPTION("UniPro APBA Control driver");
