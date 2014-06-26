/*
 * Clovertrail / Merrifield NVRAM implementation for modem
 *
 * Copyright (C) 2013 Intel Corp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <asm/intel_mid_rpmsg.h>
#include <asm/intel_scu_ipc.h>
#include <linux/platform_data/intel_mid_remoteproc.h>

#define NVRAM_MAX_SIZE	240	/* NVRAM maximum size (in bytes) for MRFLD. */

/* NVRAM access */
static u32 nvram_size;
static phys_addr_t nvram_addr;

static int platform_type;	/* Identifies the platform. */
static void *nvram_ptr;

#define DRIVER_NAME "modem_nvram"

static struct rpmsg_instance *modem_nvram_instance;

/* size interface */
static ssize_t size_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", nvram_size);
}
static struct kobj_attribute size_attribute =
	__ATTR(size, S_IRUSR|S_IRGRP, size_show, NULL);

/* dump interface */
static ssize_t dump_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	void __iomem *nv_base;

	if (((platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE)) &&
		(nvram_addr == 0)) {
		memcpy(buf, nvram_ptr, nvram_size);
	} else {
		nv_base = ioremap_nocache(nvram_addr, nvram_size);
		if (nv_base != NULL) {
			memcpy(buf, nv_base, nvram_size);
			iounmap(nv_base);
		} else
			pr_err("%s : ioremap error\n", __func__);
	}

	pr_debug("%s : %d NVRAM bytes dumped\n", __func__, nvram_size);

	return nvram_size;
}

static ssize_t dump_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	u32 sptr_dw_mask;
	int dptr_reg;
	void __iomem *nv_base;
	int ret;

	if (count == 0)
		return 0;

	if (((platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE)) &&
		(nvram_addr == 0)) {
		count = min((size_t)nvram_size, count);
		memcpy(nvram_ptr, buf, count);
	} else {
		nv_base = ioremap_nocache(nvram_addr, nvram_size);
		if (nv_base != NULL) {
			count = min((size_t)nvram_size, count);
			memcpy(nv_base, buf, count);

			if ((platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
			(platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE)) {
				sptr_dw_mask = 0xFFFFFFFF;
				/* Modem data lies on region 4, 5, 6 and 7 */
				for (dptr_reg = 4; dptr_reg <= 7; dptr_reg++) {
					pr_info("%s : ipc_write_osnib update osnib region %d\n",
							__func__, dptr_reg);
					ret = rpmsg_send_raw_command(
						modem_nvram_instance,
						RP_WRITE_OSNIB,
						0,
						NULL,
						NULL,
						0,
						0,
						sptr_dw_mask,
						dptr_reg);
					if (ret < 0) {
						pr_err("%s : ipc_write_osnib failed (%d)!!\n",
								__func__, ret);
					}
				}
			} else {
				ret = rpmsg_send_simple_command(
						modem_nvram_instance,
						IPCMSG_STORE_NV_DATA,
						0);
				if (ret)
					pr_err("%s : rpmsg_send_simple_command failed (%d)\n",
								__func__, ret);
			}
			iounmap(nv_base);
		} else
			pr_err("%s : ioremap error\n", __func__);
	}

	pr_info("%s : %zd bytes stored in NVRAM\n", __func__, count);

	return count;
}

static ssize_t dump_clear(void)
{
	int dptr_reg;
	u32 sptr_dw_mask;
	void __iomem *nv_base;
	int ret;

	if (((platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE)) &&
		(nvram_addr == 0)) {
		memset(nvram_ptr, 0, nvram_size);
	} else {
		nv_base = ioremap_nocache(nvram_addr, nvram_size);
		if (nv_base != NULL) {
			memset(nv_base, 0, nvram_size);
			if ((platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
			(platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE)) {
				sptr_dw_mask = 0xFFFFFFFF;
				/* Modem data lies on regions 4, 5, 6 and 7 */
				for (dptr_reg = 4; dptr_reg <= 7; dptr_reg++) {
					ret = rpmsg_send_raw_command(
						modem_nvram_instance,
						RP_WRITE_OSNIB,
						0, NULL, NULL, 0, 0,
						sptr_dw_mask, dptr_reg);
					if (ret < 0) {
						pr_err("%s : ipc_write_osnib failed (%d)!!\n",
								__func__, ret);
					}
				}
			} else {
				ret = rpmsg_send_simple_command(
							modem_nvram_instance,
							IPCMSG_STORE_NV_DATA,
							0);

				if (ret)
					pr_err("%s : rpmsg_send_simple_command failed (%d)\n",
								__func__, ret);
			}
			iounmap(nv_base);
		} else
			pr_err("%s : ioremap error\n", __func__);
	}

	pr_info("%s : NVRAM cleared\n", __func__);

	return 0;
}

static struct kobj_attribute dump_attribute =
	__ATTR(dump, S_IRGRP|S_IWGRP, dump_show, dump_store);

/* clear interface */
static ssize_t clear_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	pr_debug("%s : clearing NVRAM buffer", __func__);
	dump_clear();
	return count;
}
static struct kobj_attribute clear_attribute =
	__ATTR(clear, S_IWGRP, NULL, clear_store);

static struct attribute *nvram_attrs[] = {
	&size_attribute.attr,
	&dump_attribute.attr,
	&clear_attribute.attr,
	NULL,
};

static struct attribute_group nvram_attr_group = {
	.attrs = nvram_attrs,
};

static int modem_nvram_reboot_notify(struct notifier_block *notifier,
				     unsigned long what, void *data)
{
	/* Always take the same action : clear the zone */
	/* The NVMRAM will be cleared on a clean shutdown, */
	/* whatever the reason. Its goal is to keep data on spurious shutdown*/
	/* like Watchdog reboot */
	switch (what) {
	case SYS_RESTART:
	case SYS_HALT:
	case SYS_POWER_OFF:
		pr_info("%s : Clearing NVRAM on reboot notification.",
								__func__);
		dump_clear();
		break;
	default:
		pr_err("%s : Unknown reboot notification. Clearing NVRAM.",
								__func__);
		dump_clear();
		break;
	}

	return NOTIFY_DONE;
}

static int modem_nvram_rpmsg_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;

	if (rpdev == NULL) {
		pr_err("%s : rpmsg channel not created\n", __func__);
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed modem NVRAM rpmsg device\n");

	/* Allocate rpmsg instance for driver*/
	alloc_rpmsg_instance(rpdev, &modem_nvram_instance);
	if (!modem_nvram_instance) {
		dev_err(&rpdev->dev, "kzalloc modem nvram instance failed\n");
		ret = -ENOMEM;
		goto out;
	}
	/* Initialize rpmsg instance */
	init_rpmsg_instance(modem_nvram_instance);
out:
	return ret;
}

static void modem_nvram_rpmsg_remove(struct rpmsg_channel *rpdev)
{
	free_rpmsg_instance(rpdev, &modem_nvram_instance);
	dev_info(&rpdev->dev, "Removed modem NVRAM rpmsg device\n");
}

static void modem_nvram_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
					int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
		       data, len,  true);
}

static struct rpmsg_device_id modem_nvram_rpmsg_id_table[] = {
	{ .name	= "rpmsg_modem_nvram" },
	{ },
};

MODULE_DEVICE_TABLE(rpmsg, modem_nvram_rpmsg_id_table);

static struct rpmsg_driver modem_nvram_rpmsg = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= modem_nvram_rpmsg_id_table,
	.probe		= modem_nvram_rpmsg_probe,
	.callback	= modem_nvram_rpmsg_cb,
	.remove		= modem_nvram_rpmsg_remove,
};

static struct notifier_block modem_nvram_reboot = {
	.notifier_call	= modem_nvram_reboot_notify,
	.next		= NULL,
	.priority	= 0,
};

static struct kobject *modem_nvram_kobj;

static int __init modem_nvram_init(void)
{
	int retval;

	retval = register_rpmsg_driver(&modem_nvram_rpmsg);
	if (retval) {
		pr_err("%s : register_rpmsg_driver error (%d)",
						__func__, retval);
		return retval;
	}

	/* get NVRAM parameters */

	platform_type = intel_mid_identify_cpu();

	nvram_size = intel_scu_ipc_get_nvram_size();
	nvram_addr = intel_scu_ipc_get_nvram_addr();

	if (((platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE)) &&
		(nvram_addr == 0)) {
		pr_info("%s : Using local NVRAM storage\n",
				__func__);
		nvram_size = NVRAM_MAX_SIZE;
		nvram_ptr = kzalloc(nvram_size, GFP_KERNEL);
		if (nvram_ptr == NULL) {
			pr_err("%s : failed to allocate memory for nvram buffer!\n",
								__func__);
			retval = -ENOMEM;
			goto exit;
		}
		pr_info("Modem NVRAM: PTR: 0x%p\n", nvram_ptr);
	} else {
		pr_info("Modem NVRAM: ADDR: %pa\n", &nvram_addr);
	}

	pr_info("Modem NVRAM: SIZE: 0x%x\n", nvram_size);

	if ((((platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE)) ||
		(nvram_addr != 0)) && (nvram_size > 0)) {
		if (register_reboot_notifier(&modem_nvram_reboot))
			pr_err("%s : can't register reboot_notifier\n",
								__func__);
		modem_nvram_kobj = kobject_create_and_add(DRIVER_NAME,
								kernel_kobj);
		if (!modem_nvram_kobj) {
			retval = -ENOMEM;
			goto error;
		}

		retval = sysfs_create_group(modem_nvram_kobj,
					&nvram_attr_group);
		if (retval) {
			retval = -ENODEV;
			kobject_put(modem_nvram_kobj);
			goto error;
		}

	} else {
		pr_err("NVRAM not initialized. Aborting.\n");
		retval = -ENODEV;
		goto error;
	}

	return 0;

error:
	unregister_reboot_notifier(&modem_nvram_reboot);
exit:
	unregister_rpmsg_driver(&modem_nvram_rpmsg);
	return retval;
}

static void __exit modem_nvram_exit(void)
{
	if ((platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE))
		kfree(nvram_ptr);

	kobject_put(modem_nvram_kobj);
	unregister_rpmsg_driver(&modem_nvram_rpmsg);
	unregister_reboot_notifier(&modem_nvram_reboot);
	sysfs_remove_group(modem_nvram_kobj, &nvram_attr_group);
}

module_init(modem_nvram_init);
module_exit(modem_nvram_exit);

MODULE_DESCRIPTION("Intel modem NVRAM driver");
MODULE_LICENSE("GPL");
