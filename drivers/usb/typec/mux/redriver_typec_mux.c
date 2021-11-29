// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for OnSemi Type-C DP ALT Mode Redriver MUX
 *
 * Copyright (C) 2021 Motorola, Inc.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/usb/pd.h>
#include <linux/usb/typec_mux.h>
#include <linux/usb/typec_dp.h>
#include <linux/usb/tcpm.h>

#define MOTO_ALTMODE(fmt, ...) pr_err("MMI_DETECT: MUX[%s]: " fmt "\n", __func__, ##__VA_ARGS__)

struct tcpm_port;

struct redriver_mux_data {
	struct device *dev;
	struct mutex lock; /* protects the cached conf register */
	struct typec_switch *sw;
	struct typec_mux *mux;
	void *redriver_dev;
	int (*set_mux)(void *, u8);
	u8 conf;
};

static struct redriver_mux_data mqd;

int register_dp_altmode_mux_control(void *redriver_dev, int (*set)(void *, u8))
{
	mqd.redriver_dev = redriver_dev;
	mqd.set_mux = set;
	MOTO_ALTMODE("called");

	return 0;
}
EXPORT_SYMBOL_GPL(register_dp_altmode_mux_control);

static int redriver_mux_set(struct typec_mux *mux,
					struct typec_mux_state *state)
{
	struct redriver_mux_data *pi = typec_mux_get_drvdata(mux);
	u8 new_conf;

	if (!pi->set_mux || !state)
		return -EINVAL;

	mutex_lock(&pi->lock);
	new_conf = state->mode;
	pi->set_mux(pi->redriver_dev, new_conf);
	MOTO_ALTMODE("conf=%#x->%#x", pi->conf, new_conf);
	pi->conf = new_conf;
	mutex_unlock(&pi->lock);

	return 0;
}

static int redriver_mux_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct typec_mux_desc mux_desc = { };
	int ret;

	MOTO_ALTMODE("called; fwnode %p", dev->fwnode);
	mqd.dev = dev;
	mutex_init(&mqd.lock);

	mux_desc.drvdata = &mqd;
	mux_desc.fwnode = dev->fwnode;
	mux_desc.set = redriver_mux_set;

	mqd.mux = typec_mux_register(dev, &mux_desc);
	if (IS_ERR(mqd.mux)) {
		typec_switch_unregister(mqd.sw);
		ret = PTR_ERR(mqd.mux);
		dev_err(dev, "Error registering typec mux: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, &mqd);
	MOTO_ALTMODE("completed; sw=%p, mux=%p", mqd.sw, mqd.mux);

	return 0;
}

static int redriver_mux_remove(struct platform_device *pdev)
{
	struct redriver_mux_data *pi = platform_get_drvdata(pdev);

	typec_mux_unregister(pi->mux);
	typec_switch_unregister(pi->sw);

	return 0;
}

static const struct of_device_id redriver_mux_table[] = {
	{ .compatible = "onsemi,redriver-typec-mux" },
	{ }
};
MODULE_DEVICE_TABLE(of, redriver_mux_table);

static struct platform_driver redriver_mux_driver = {
	.driver = {
		.name = "redriver_typec-mux",
		.of_match_table = redriver_mux_table,
	},
	.probe = redriver_mux_probe,
	.remove = redriver_mux_remove,
};
module_platform_driver(redriver_mux_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("OnSemi Type-C Redriver MUX driver");
