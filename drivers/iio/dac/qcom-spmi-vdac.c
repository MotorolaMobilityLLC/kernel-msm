// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/consumer.h>

#define VDAC_ENABLE_CTL				0x46
#define VDAC_BIN_CODE_LB			0x48
#define VDAC_RANGE_CTL				0x62

#define VDAC_FULL_RANGE				0x00
#define VDAC_HALF_RANGE				0x01

#define VDAC_ENABLE_STATE_ON			0x80
#define VDAC_ENABLE_STATE_OFF			0x00

#define VADC_MISC_VAL_PREPARE_READ		0x03
#define VADC_MISC_VAL_RESET_READ		0x00

#define VDAC_DAC_CODE_MIN			0x00
#define VDAC_DAC_CODE_MAX_7BIT			0x7F

/**
 * struct vdac_data - VDAC device-specific data
 * @base: Base address of VDAC registers
 * @misc_base: Base address of misc register for ATEST switching
 */
struct vdac_data {
	u32 base;
	u32 misc_base;
};

static const struct vdac_data pmar2230_vdac_data = {
	.base = 0x9600,      /* VDAC base address */
	.misc_base = 0x947,  /* SDAM base (0x900) + misc offset (0x47) */
};

/**
 * struct vdac_chip - VDAC device state
 * @regmap: Register map for SPMI access
 * @dev: Device pointer
 * @data: Device-specific configuration data from match table
 * @iio_chn_vdac_p: Positive VADC channel (ATEST1)
 * @iio_chn_vdac_n: Negative VADC channel (ATEST2)
 * @base: Base register address
 * @misc_base: Base address of misc register for ATEST switching
 * @is_enabled: Current enable state
 * @vout_range: Current voltage range (full/half)
 * @lock: Mutex for state protection
 */

struct vdac_chip {
	struct regmap *regmap;
	struct device *dev;
	const struct vdac_data *data;
	struct iio_channel *iio_chn_vdac_p;
	struct iio_channel *iio_chn_vdac_n;
	u32 base;
	u32 misc_base;
	u8 is_enabled;
	int vout_range;
	struct mutex lock;
};

static ssize_t vdac_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct vdac_chip *vdac = iio_priv(indio_dev);

	guard(mutex)(&vdac->lock);

	return scnprintf(buf, PAGE_SIZE, "%d\n", vdac->is_enabled);
}

static ssize_t vdac_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct vdac_chip *vdac = iio_priv(indio_dev);
	int val, ret;
	u8 new_state;

	guard(mutex)(&vdac->lock);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	new_state = !!val;

	if (vdac->is_enabled == new_state) {
		dev_dbg(dev, "VDAC already %s\n", new_state ? "enabled" : "disabled");
		return len;
	}

	ret = regmap_write(vdac->regmap, vdac->base + VDAC_ENABLE_CTL,
			   new_state ? VDAC_ENABLE_STATE_ON : VDAC_ENABLE_STATE_OFF);

	if (ret) {
		dev_err(dev, "Error in %s VDAC module: %d\n",
			new_state ? "enabling" : "disabling", ret);
		return ret;
	}
	vdac->is_enabled = new_state;

	return len;
}

IIO_DEVICE_ATTR(enable, 0644, vdac_enable_show, vdac_enable_store, 0);

static int vdac_route_output_to_adc(struct vdac_chip *vdac, bool enable)
{
	u8 misc_val = enable ? VADC_MISC_VAL_PREPARE_READ : VADC_MISC_VAL_RESET_READ;
	int ret;

	ret = regmap_write(vdac->regmap, vdac->misc_base, misc_val);
	if (ret < 0) {
		dev_err(vdac->dev, "Failed to %s ATEST routing: %d\n",
			enable ? "enable" : "disable", ret);
		return ret;
	}

	dev_dbg(vdac->dev, "ATEST routing %s (misc_base=0x%x, val=0x%02x)\n",
		enable ? "enabled" : "disabled", vdac->misc_base, misc_val);
	return 0;
}

static ssize_t vdac_adc_readback_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct vdac_chip *vdac = iio_priv(indio_dev);
	int volt_diff = 0;
	int vdac_p_ref, vdac_n_ref, ret;

	guard(mutex)(&vdac->lock);

	if (!vdac->is_enabled) {
		dev_err(dev, "VDAC must be enabled for ADC readback\n");
		return -EIO;
	}

	ret = vdac_route_output_to_adc(vdac, true);
	if (ret < 0) {
		dev_err(dev, "Failed to enable ATEST routing: %d\n", ret);
		return ret;
	}

	ret = iio_read_channel_processed(vdac->iio_chn_vdac_p, &vdac_p_ref);
	if (ret < 0) {
		dev_err(dev, "Failed to read VDAC positive ADC channel: %d\n", ret);
		goto cleanup;
	}

	ret = iio_read_channel_processed(vdac->iio_chn_vdac_n, &vdac_n_ref);
	if (ret < 0) {
		dev_err(dev, "Failed to read VDAC negative ADC channel: %d\n", ret);
		goto cleanup;
	}

	volt_diff = vdac_p_ref - vdac_n_ref;

	dev_dbg(dev, "VDAC readback: P=%d mV, N=%d mV, diff(P-N)=%d mV\n",
			vdac_p_ref, vdac_n_ref, volt_diff);
	ret = 0;

cleanup:
	if (vdac_route_output_to_adc(vdac, false) < 0)
		dev_warn(dev, "Failed to disable ATEST routing\n");
	return (ret < 0) ? ret : scnprintf(buf, PAGE_SIZE, "%d\n", volt_diff);

}

IIO_DEVICE_ATTR(adc_readback, 0644, vdac_adc_readback_show, NULL, 0);

static int vdac_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan,
		int val, int val2, long mask)
{
	struct vdac_chip *vdac = iio_priv(indio_dev);
	int ret = 0;

	guard(mutex)(&vdac->lock);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		if (val < VDAC_DAC_CODE_MIN || val > VDAC_DAC_CODE_MAX_7BIT) {
			dev_err(vdac->dev, "DAC value %d out of range (0-127)\n", val);
			return -EINVAL;
		}
		if (!vdac->is_enabled) {
			ret = regmap_write(vdac->regmap, vdac->base + VDAC_ENABLE_CTL,
					   VDAC_ENABLE_STATE_ON);
			if (ret) {
				dev_err(vdac->dev, "Error auto-enabling VDAC module: %d\n", ret);
				return ret;
			}
			vdac->is_enabled = 1;
			dev_dbg(vdac->dev, "VDAC auto-enabled for DAC write\n");
		}

		ret = regmap_write(vdac->regmap, vdac->base + VDAC_BIN_CODE_LB, val);
		if (ret) {
			dev_err(vdac->dev, "Error in setting the VDAC voltage: %d\n", ret);
			return ret;
		}

		dev_dbg(vdac->dev, "VDAC value set to %d\n", val);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int vdac_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan,
		int *val, int *val2, long mask)
{
	struct vdac_chip *vdac = iio_priv(indio_dev);
	int ret = 0;

	guard(mutex)(&vdac->lock);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = regmap_read(vdac->regmap, vdac->base + VDAC_BIN_CODE_LB, val);
		if (ret) {
			dev_err(vdac->dev, "Error in reading the VDAC raw value %d\n", ret);
			return ret;
		}
		return IIO_VAL_INT;

	default:
		return -EINVAL;
	}
}

static ssize_t vdac_range_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct vdac_chip *vdac = iio_priv(indio_dev);
	bool full_range;

	guard(mutex)(&vdac->lock);

	full_range = (vdac->vout_range == VDAC_FULL_RANGE);

	return scnprintf(buf, PAGE_SIZE, "%s\n", full_range ? "full" : "half");
}

static ssize_t vdac_range_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct vdac_chip *vdac = iio_priv(indio_dev);
	int ret = 0;

	guard(mutex)(&vdac->lock);

	if (vdac->is_enabled) {
		dev_err(dev, "VDAC is enabled, cannot change range\n");
		return -EBUSY;
	}

	if (sysfs_streq(buf, "half")) {
		if (vdac->vout_range == VDAC_HALF_RANGE) {
			dev_dbg(dev, "VDAC already in half range\n");
			return len;
		}
		ret = regmap_write(vdac->regmap, vdac->base + VDAC_RANGE_CTL, VDAC_HALF_RANGE);
		if (ret) {
			dev_err(dev, "Failed to set VDAC to half range: %d\n", ret);
			return ret;
		}
		vdac->vout_range = VDAC_HALF_RANGE;
	} else if (sysfs_streq(buf, "full")) {
		if (vdac->vout_range == VDAC_FULL_RANGE) {
			dev_dbg(dev, "VDAC already in full range\n");
			return len;
		}
		ret = regmap_write(vdac->regmap, vdac->base + VDAC_RANGE_CTL, VDAC_FULL_RANGE);
		if (ret) {
			dev_err(dev, "Failed to set VDAC to full range: %d\n", ret);
			return ret;
		}
		vdac->vout_range = VDAC_FULL_RANGE;
	} else {
		dev_err(dev, "Invalid range specified. Use 'full' or 'half'\n");
		return -EINVAL;
	}

	return len;
}

IIO_DEVICE_ATTR(range, 0644, vdac_range_show, vdac_range_store, 0);

static struct attribute *vdac_attributes[] = {
	&iio_dev_attr_enable.dev_attr.attr,
	&iio_dev_attr_adc_readback.dev_attr.attr,
	&iio_dev_attr_range.dev_attr.attr,
	NULL,
};

static const struct attribute_group vdac_attribute_group = {
	.attrs = vdac_attributes,
};

static const struct iio_info vdac_info = {
	.write_raw = vdac_write_raw,
	.read_raw = vdac_read_raw,
	.attrs = &vdac_attribute_group,
};

static struct iio_chan_spec vdac_channels[] = {
	{
		.type = IIO_VOLTAGE,
		.channel = 0,
		.datasheet_name = "dppd_vdac",
		.indexed = 0,
		.output = 1,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
	},
};

static int qcom_spmi_vdac_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct vdac_chip *vdac = iio_priv(indio_dev);
	int ret;

	mutex_lock(&vdac->lock);

	if (vdac->is_enabled) {
		ret = regmap_write(vdac->regmap, vdac->base + VDAC_ENABLE_CTL,
				   VDAC_ENABLE_STATE_OFF);
		if (ret)
			dev_warn(&pdev->dev, "Failed to disable VDAC during removal: %d\n", ret);
		else
			vdac->is_enabled = 0;
		dev_dbg(&pdev->dev, "VDAC disabled during driver removal\n");
	}

	mutex_unlock(&vdac->lock);
	mutex_destroy(&vdac->lock);

	return 0;
}

static int qcom_spmi_vdac_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct iio_dev *indio_dev;
	struct vdac_chip *vdac;
	struct regmap *regmap;
	const struct vdac_data *data;
	int ret;

	regmap = dev_get_regmap(dev->parent, NULL);
	if (!regmap)
		return -ENODEV;

	data = device_get_match_data(&pdev->dev);
	if (!data)
		return -EINVAL;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*vdac));
	if (!indio_dev)
		return -ENOMEM;

	vdac = iio_priv(indio_dev);
	vdac->regmap = regmap;
	vdac->dev = dev;
	vdac->data = data;
	vdac->vout_range = VDAC_FULL_RANGE;
	vdac->is_enabled = 0;
	mutex_init(&vdac->lock);

	vdac->base = data->base;
	vdac->misc_base = data->misc_base;

	vdac->iio_chn_vdac_p = devm_iio_channel_get(dev, "ATEST1");
	if (IS_ERR(vdac->iio_chn_vdac_p)) {
		dev_err(dev, "Failed to get IIO channel ATEST1\n");
		return PTR_ERR(vdac->iio_chn_vdac_p);
	}

	vdac->iio_chn_vdac_n = devm_iio_channel_get(dev, "ATEST2");
	if (IS_ERR(vdac->iio_chn_vdac_n)) {
		dev_err(dev, "Failed to get IIO channel ATEST2\n");
		return PTR_ERR(vdac->iio_chn_vdac_n);
	}

	indio_dev->dev.parent = dev;
	indio_dev->dev.of_node = node;
	indio_dev->name = pdev->name;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &vdac_info;
	indio_dev->channels = vdac_channels;
	indio_dev->num_channels = ARRAY_SIZE(vdac_channels);

	ret = devm_iio_device_register(dev, indio_dev);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to register IIO device\n");
		mutex_destroy(&vdac->lock);
		return ret;
	}

	return 0;
}

static const struct of_device_id vdac_match_table[] = {
	{ .compatible = "qcom,spmi-vdac", .data = &pmar2230_vdac_data },
	{ }
};
MODULE_DEVICE_TABLE(of, vdac_match_table);

static struct platform_driver vdac_driver = {
	.driver = {
		.name = "qcom-spmi-vdac",
		.of_match_table = vdac_match_table,
	},
	.probe = qcom_spmi_vdac_probe,
	.remove = qcom_spmi_vdac_remove,
};
module_platform_driver(vdac_driver);

MODULE_DESCRIPTION("Qualcomm Technologies Inc. PMIC5 VDAC driver");
MODULE_LICENSE("GPL");
