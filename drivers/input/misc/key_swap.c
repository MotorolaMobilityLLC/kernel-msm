/* drivers/input/misc/key_swap.c
 *
 * Copyright (C) 2018 Motorola, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/extcon.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#define FLIP_GPIO_NAME "key_swap,flip-gpio"

#if 0
#ifdef pr_debug
#undef pr_debug
#define pr_debug pr_err
#endif
#endif
#define MYNAME "key-swap"

struct filter_action {
	unsigned int code;		/* trigger key code */
	unsigned int swap_code;		/* swap key code; if swap code is 0, then suppress */
	bool (*condition)(void); 	/* check condition func returns true/false */
					/* True - do action, False - ignore */
	struct list_head link;
};

struct key_swap_data {
	bool registered;
	bool dopoll;
	atomic_t flip_state;
	int trigger_state;
	unsigned int gpion;
	struct extcon_dev *flip_dev;
	struct notifier_block flip_nb;
	struct list_head fa;
	int (*query_flip_state)(void);
};

static struct key_swap_data *kfd;

static int query_gpio(void)
{
	return gpio_get_value(kfd->gpion);
}

static int query_extcon_device(void)
{
	return extcon_get_state(kfd->flip_dev, EXTCON_MECHANICAL);
}

static irqreturn_t signal_gpio_irq(int irq, void *data)
{
	atomic_set(&kfd->flip_state, kfd->query_flip_state());
	pr_debug("flip state set %d\n", atomic_read(&kfd->flip_state));
	return IRQ_HANDLED;
}

unsigned int key_swap_algo(unsigned int code)
{
	struct filter_action *act;
	bool condvar = true;

	if (!kfd)
		goto pass_thru;

	list_for_each_entry(act, &kfd->fa, link) {
		if (code != act->code)
			continue;

		pr_debug("match: code = %u\n", code);

		if (act->condition) {
			condvar = act->condition();
			pr_debug("swap condition: %d\n", condvar);
		}

		if (!condvar)
			continue;

		if (act->swap_code) {
			pr_debug("swapping: %u -> %u\n", code, act->swap_code);
			code = act->swap_code;
			break;
		}
	}
pass_thru:
	return code;
}
EXPORT_SYMBOL_GPL(key_swap_algo);

static bool flip_is_closed(void)
{
	int state = kfd->dopoll ? gpio_get_value(kfd->gpion) :
			atomic_read(&kfd->flip_state);
	pr_debug("state = %d\n", state);
	return state == 1;
}

static int flip_state_changed(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct key_swap_data *kfd =
		container_of(nb, struct key_swap_data, flip_nb);
	atomic_set(&kfd->flip_state, (int)event);
	pr_debug("flip state change notification = %d\n", (int)event);
	return NOTIFY_DONE;
}

static int key_swap_dt(struct device *device)
{
	int rc;
	struct device_node *np = NULL;
	struct filter_action *fa;

	if (of_property_read_bool(device->of_node, "extcon")) {
		/* use extcon as signalling device */
		kfd->flip_dev = extcon_get_edev_by_phandle(device, 0);
		if (!kfd->flip_dev) {
			pr_err("flip detection not found\n");
			goto check_gpio_next;
		}

		kfd->flip_nb.notifier_call = flip_state_changed;
		rc = extcon_register_notifier(kfd->flip_dev,
				EXTCON_MECHANICAL, &kfd->flip_nb);
		if (rc) {
			pr_err("error registering notifier %d\n", rc);
			goto check_gpio_next;
		}

		kfd->query_flip_state = query_extcon_device;
		pr_info("use extcon to monitor flip state\n");
	}

check_gpio_next:

	if (!kfd->query_flip_state) {
		kfd->gpion = of_get_gpio(device->of_node, 0);
		if (!gpio_is_valid(kfd->gpion)) {
			pr_err("invalid flip state gpio\n");
			goto read_actions;
		}

		rc = gpio_request_one(kfd->gpion, GPIOF_DIR_IN, FLIP_GPIO_NAME);
		if (rc) {
			pr_err("request gpio failed, rc = %d\n", rc);
			goto read_actions;
		}

		kfd->dopoll = of_property_read_bool(device->of_node, "key_swap,poll");
		kfd->query_flip_state = query_gpio;

		if (!kfd->dopoll) {
			rc = devm_request_irq(device, gpio_to_irq(kfd->gpion),
				signal_gpio_irq,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				FLIP_GPIO_NAME "-irq", NULL);
			if (rc) {
				pr_err("request irq failed, rc = %d; default to polling\n", rc);
				kfd->dopoll = true;
			}
		}

		if (!rc)
			pr_info("use GPIO %d to monitor flip state\n", kfd->gpion);
	}

read_actions:

	while ((np = of_get_next_child(device->of_node, np))) {
		fa = kzalloc(sizeof(*fa), GFP_KERNEL);
		if (!fa) {
			rc = -ENOMEM;
			break;
		}
		INIT_LIST_HEAD(&fa->link);
		rc = of_property_read_u32(np, "code", &fa->code);
		if (rc)
			break;
#if 0
	const char *tname = NULL;
		rc = of_property_read_string(np, "action-type", &tname);
		if (rc)
			break;

		if (!tname) {
			pr_err("missing action-type parameter\n");
			continue;
		}

		if (!strncmp(tname, "swap", 4)) {
#endif
			rc = of_property_read_u32(np, "swap", &fa->swap_code);
			if (rc)
				break;
			fa->condition = flip_is_closed;
#if 0
		}
#endif
		pr_debug("will swap %d with %d\n", fa->code, fa->swap_code);
		list_add(&fa->link, &kfd->fa);
	}

	return rc;
}

static int key_swap_probe(struct platform_device *pdev)
{
	int rc;

	kfd = kzalloc(sizeof(*kfd), GFP_KERNEL);
	if (!kfd)
		return -ENOMEM;

	INIT_LIST_HEAD(&kfd->fa);

	rc = key_swap_dt(&pdev->dev);
	if (rc) {
		pr_err("dt parse error: rc = %d\n", rc);
		goto exit_with_error;
	}

	atomic_set(&kfd->flip_state, kfd->query_flip_state());
	pr_debug("flip state at probe time = %d\n", atomic_read(&kfd->flip_state));

	platform_set_drvdata(pdev, kfd);

	return 0;

exit_with_error:
	kfree(kfd);
	return -EFAULT;
}

static int key_swap_remove(struct platform_device *pdev)
{
	struct filter_action *act, *next;

	platform_set_drvdata(pdev, NULL);
	list_for_each_entry_safe(act, next, &kfd->fa, link) {
		list_del(&act->link);
		kfree(act);
	}
	kfree(kfd);
	return 0;
}

static const struct of_device_id key_swap_of_match[] = {
	{ .compatible = "mmi,key_swap", },
	{}
};
MODULE_DEVICE_TABLE(of, key_swap_of_match);

struct platform_driver key_swap_driver = {
	.driver = {
		.name = MYNAME,
		.owner = THIS_MODULE,
		.of_match_table = key_swap_of_match,
	},
	.probe = key_swap_probe,
	.remove = key_swap_remove,
};

static int __init key_swap_init(void)
{
	pr_debug("enter\n");
	return platform_driver_register(&key_swap_driver);
}

static void __exit key_swap_exit(void)
{
	platform_driver_unregister(&key_swap_driver);
}

late_initcall(key_swap_init);
module_exit(key_swap_exit);

MODULE_DESCRIPTION("MMI Key code swapping driver");
MODULE_LICENSE("GPL v2");
