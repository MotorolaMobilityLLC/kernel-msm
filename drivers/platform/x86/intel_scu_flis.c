/* intel_scu_flis.c SCU FLIS INTERFACES
 *
 * Copyright (c) 2012,  Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/rpmsg.h>
#include <asm/intel-mid.h>
#include <asm/intel_scu_ipc.h>
#include <asm/intel_scu_flis.h>
#include <asm/intel_mid_rpmsg.h>
#include <asm/intel_mid_remoteproc.h>
#include <linux/platform_data/intel_mid_remoteproc.h>

static struct rpmsg_instance *flis_instance;

static u32 shim_flis_addr;
static u32 shim_offset;
static u32 shim_data;
static char shim_ops[OPS_STR_LEN];

static u32 param_type;	/* flis param type: PULL/PIN DIRECTION/OPEN_DRAIN */
static u32 param_value;	/* value of certain flis param */
static unsigned int pin_name;
static char ops[OPS_STR_LEN];

static int platform;

struct intel_scu_flis_info {
	struct pinstruct_t *pin_t;
	struct pin_mmio_flis_t *mmio_flis_t;
	int pin_num;
	int initialized;
	void *flis_base;
	u32 flis_paddr;
	bool shim_access;
};

static struct intel_scu_flis_info flis_info;

static DEFINE_SPINLOCK(mmio_flis_lock);

u32 get_flis_value(u32 offset)
{
	struct intel_scu_flis_info *isfi = &flis_info;
	u32 __iomem *mem;

	if (!isfi->initialized || !isfi->flis_base)
		return -ENODEV;

	mem = (void __iomem *)(isfi->flis_base + offset);

	return readl(mem);
}
EXPORT_SYMBOL(get_flis_value);

void set_flis_value(u32 value, u32 offset)
{
	struct intel_scu_flis_info *isfi = &flis_info;
	u32 __iomem *mem;
	unsigned long flags;

	if (!isfi->initialized || !isfi->flis_base)
		return;

	/*
	 * There is one security region for Merrifield FLIS, which
	 * are read only to OS side. Use IPC when write access is needed.
	 */
	if ((platform == INTEL_MID_CPU_CHIP_TANGIER ||
		platform == INTEL_MID_CPU_CHIP_ANNIEDALE)
			&& offset >= I2C_FLIS_START
			&& offset <= I2C_FLIS_END) {
		/* IPC call should not be called in atomic context */
		might_sleep();
		rpmsg_send_generic_raw_command(RP_INDIRECT_WRITE, 0,
					(u8 *)&value, 4,
					NULL, 0,
					isfi->flis_paddr + offset, 0);

	} else {
		mem = (void __iomem *)(isfi->flis_base + offset);
		spin_lock_irqsave(&mmio_flis_lock, flags);
		writel(value, mem);
		spin_unlock_irqrestore(&mmio_flis_lock, flags);
	}
}
EXPORT_SYMBOL(set_flis_value);

/* directly write to flis address */
int intel_scu_ipc_write_shim(u32 data, u32 flis_addr, u32 offset)
{
	struct intel_scu_flis_info *isfi = &flis_info;
	int ret;
	u32 ipc_wbuf[3];

	if (!isfi->shim_access)
		return -EINVAL;

	/* offset 0xff means the flis is reserved, just return 0*/
	if (offset == 0xFF)
		return 0;

	ipc_wbuf[0] = flis_addr; /* wbuf[0]: flis address */
	ipc_wbuf[1] = offset;	/* wbuf[1]: register offset */
	ipc_wbuf[2] = data;	/* wbuf[2]: data */

	ret = rpmsg_send_command(flis_instance,	IPCMSG_SHIM_CONFIG,
				IPC_CMD_SHIM_WR, (u8 *)ipc_wbuf, NULL, 12, 0);
	if (ret)
		pr_err("%s: failed to write shim, flis addr: 0x%x, offset: 0x%x\n",
			__func__, flis_addr, offset);

	return ret;
}
EXPORT_SYMBOL(intel_scu_ipc_write_shim);

/* directly read from flis address */
int intel_scu_ipc_read_shim(u32 *data, u32 flis_addr, u32 offset)
{
	struct intel_scu_flis_info *isfi = &flis_info;
	int ret;
	u32 ipc_wbuf[2];

	if (!isfi->shim_access)
		return -EINVAL;

	/* offset 0xff means the flis is reserved, just return 0 */
	if (offset == 0xFF)
		return 0;

	ipc_wbuf[0] = flis_addr;
	ipc_wbuf[1] = offset;

	ret = rpmsg_send_command(flis_instance,	IPCMSG_SHIM_CONFIG,
				IPC_CMD_SHIM_RD, (u8 *)ipc_wbuf, data, 8, 1);
	if (ret)
		pr_err("%s: failed to read shim, flis addr: 0x%x, offset: 0x%x\n",
			__func__, flis_addr, offset);

	return ret;
}
EXPORT_SYMBOL(intel_scu_ipc_read_shim);

int intel_scu_ipc_update_shim(u32 data, u32 mask, u32 flis_addr, u32 offset)
{
	struct intel_scu_flis_info *isfi = &flis_info;
	u32 tmp = 0;
	int ret;

	if (!isfi->shim_access)
		return -EINVAL;

	ret = intel_scu_ipc_read_shim(&tmp, flis_addr, offset);
	if (ret) {
		pr_err("read shim failed, addr = 0x%x, off = 0x%x\n",
			flis_addr, offset);
		return ret;
	}

	tmp &= ~mask;
	tmp |= (data & mask);

	ret = intel_scu_ipc_write_shim(tmp, flis_addr, offset);
	if (ret) {
		pr_err("write shim failed, addr = 0x%x, off = 0x%x\n",
			flis_addr, offset);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(intel_scu_ipc_update_shim);

/**
 * config_pin_flis -- configure pin mux,
 *		      pull direction and strength and open-drain enable.
 *
 * @name: pin name
 * @param: flis param
 * @val: value to be set
 *
 * example:
 * config pull up/down:
 *	config_pin_flis(i2s_2_clk, PULL, UP_20K);
 *	config_pin_flis(i2s_2_clk, PULL, DOWN_20K);
 *
 * config pin mux:
 *	config_pin_flis(i2s_2_clk, MUX, MUX_EN_INPUT_EN);
 *	config_pin_flis(i2s_2_clk, MUX, INPUT_EN);
 *	config_pin_flis(i2s_2_clk, MUX, MUX_EN_OUTPUT_EN);
 *	config_pin_flis(i2s_2_clk, MUX, OUTPUT_EN);
 *
 * config pin open-drain:
 *	config_pin_flis(i2s_2_clk, OPEN_DRAIN, OD_ENABLE);
 *	config_pin_flis(i2s_2_clk, OPEN_DRAIN, OD_DISABLE);
 *
 */
int config_pin_flis(unsigned int name, enum flis_param_t param, u32 val)
{
	u32 flis_addr, off, data, mask;
	int ret;
	int pos;
	struct intel_scu_flis_info *isfi = &flis_info;
	struct pin_mmio_flis_t *mmft;
	u32 old_val;

	if (!isfi->initialized)
		return -ENODEV;

	if (name < 0 || name >= isfi->pin_num)
		return -EINVAL;

	if (platform == INTEL_MID_CPU_CHIP_CLOVERVIEW) {
		/* Check if the pin is configurable */
		if (isfi->pin_t[name].valid == false)
			return -EINVAL;

		flis_addr = isfi->pin_t[name].bus_address;

		switch (param) {
		case PULL:
			off = isfi->pin_t[name].pullup_offset;
			pos = isfi->pin_t[name].pullup_lsb_pos;
			mask = (PULL_MASK << pos);
			break;
		case MUX:
			off = isfi->pin_t[name].direction_offset;
			pos = isfi->pin_t[name].direction_lsb_pos;
			mask = (MUX_MASK << pos);
			break;
		case OPEN_DRAIN:
			off = isfi->pin_t[name].open_drain_offset;
			pos = isfi->pin_t[name].open_drain_bit;
			mask = (OPEN_DRAIN_MASK << pos);
			break;
		default:
			pr_err("Please specify valid flis param\n");
			return -EINVAL;
		}

		data = (val << pos);
		pr_debug("addr = 0x%x, off = 0x%x, pos = %d, mask = 0x%x, data = 0x%x\n",
				flis_addr, off, pos, mask, data);

		ret = intel_scu_ipc_update_shim(data, mask, flis_addr, off);
		if (ret) {
			pr_err("update shim failed\n");
			return ret;
		}
	} else if (platform == INTEL_MID_CPU_CHIP_TANGIER ||
		platform == INTEL_MID_CPU_CHIP_ANNIEDALE) {
		mmft = isfi->mmio_flis_t;
		off = mmft[name].offset;

		/* Check if the FLIS is writable by mmio access */
		if (!(mmft[name].access_ctrl & writable))
			return -EINVAL;

		old_val = get_flis_value(off);

		switch (param) {
		case PULL:
			mask = PULL_MASK;
			break;
		case MUX:
			mask = MUX_MASK;
			break;
		case OPEN_DRAIN:
			mask = OPEN_DRAIN_MASK;
			break;
		default:
			pr_err("Please specify valid flis param\n");
			return -EINVAL;
		}

		set_flis_value((old_val & ~mask) | val, off);

	} else
		return -EINVAL;


	return 0;
}
EXPORT_SYMBOL_GPL(config_pin_flis);

int get_pin_flis(unsigned int name, enum flis_param_t param, u32 *val)
{
	u32 flis_addr, off;
	u32 data = 0;
	int ret;
	int pos;
	u32 mask;
	struct intel_scu_flis_info *isfi = &flis_info;
	struct pin_mmio_flis_t *mmft;
	u32 old_val;

	if (!isfi->initialized)
		return -ENODEV;

	if (name < 0 || name >= isfi->pin_num)
		return -EINVAL;

	if (platform == INTEL_MID_CPU_CHIP_CLOVERVIEW) {
		if (isfi->pin_t[name].valid == false)
			return -EINVAL;

		flis_addr = isfi->pin_t[name].bus_address;

		switch (param) {
		case PULL:
			off = isfi->pin_t[name].pullup_offset;
			pos = isfi->pin_t[name].pullup_lsb_pos;
			mask = PULL_MASK;
			break;
		case MUX:
			off = isfi->pin_t[name].direction_offset;
			pos = isfi->pin_t[name].direction_lsb_pos;
			mask = MUX_MASK;
			break;
		case OPEN_DRAIN:
			off = isfi->pin_t[name].open_drain_offset;
			pos = isfi->pin_t[name].open_drain_bit;
			mask = OPEN_DRAIN_MASK;
			break;
		default:
			pr_err("Please specify valid flis param\n");
			return -EINVAL;
		}

		ret = intel_scu_ipc_read_shim(&data, flis_addr, off);
		if (ret) {
			pr_err("read shim failed, addr = 0x%x, off = 0x%x\n",
				flis_addr, off);
			return ret;
		}

		*val = (data >> pos) & mask;

		pr_debug("read: data = 0x%x, val = 0x%x\n", data, *val);
	} else if (platform == INTEL_MID_CPU_CHIP_TANGIER ||
		platform == INTEL_MID_CPU_CHIP_ANNIEDALE) {
		mmft = isfi->mmio_flis_t;
		off = mmft[name].offset;

		old_val = get_flis_value(off);

		switch (param) {
		case PULL:
			mask = PULL_MASK;
			break;
		case MUX:
			mask = MUX_MASK;
			break;
		case OPEN_DRAIN:
			mask = OPEN_DRAIN_MASK;
			break;
		default:
			pr_err("Please specify valid flis param\n");
			return -EINVAL;
		}

		*val = (old_val & mask);

	} else
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL_GPL(get_pin_flis);

static void flis_generic_store(const char *buf, int type)
{
	u32 tmp;
	int ret;

	/* use decimal for pin number */
	if (type == DBG_PIN_NAME)
		ret = sscanf(buf, "%d", &tmp);
	else
		ret = sscanf(buf, "%x", &tmp);

	if (ret != 1)
		return;

	switch (type) {
	case DBG_SHIM_FLIS_ADDR:
		shim_flis_addr = tmp;
		break;
	case DBG_SHIM_OFFSET:
		shim_offset = tmp;
		break;
	case DBG_SHIM_DATA:
		shim_data = tmp;
		break;
	case DBG_PARAM_VAL:
		param_value = tmp;
		break;
	case DBG_PARAM_TYPE:
		param_type = tmp;
		break;
	case DBG_PIN_NAME:
		pin_name = tmp;
		break;
	default:
		break;
	}
}

#ifdef CONFIG_X86_CTP
static ssize_t shim_flis_addr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	flis_generic_store(buf, DBG_SHIM_FLIS_ADDR);
	return size;
}

static ssize_t shim_flis_addr_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", shim_flis_addr);
}
#endif

static ssize_t shim_offset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	flis_generic_store(buf, DBG_SHIM_OFFSET);
	return size;
}

static ssize_t shim_offset_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", shim_offset);
}

static ssize_t shim_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	flis_generic_store(buf, DBG_SHIM_DATA);
	return size;
}

static ssize_t shim_data_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", shim_data);
}

static ssize_t shim_ops_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;

	memset(shim_ops, 0, sizeof(shim_ops));

	ret = sscanf(buf, "%9s", shim_ops);
	if (ret != 1)
		return -EINVAL;

	ret = 0;
	if (!strncmp("get", shim_ops, OPS_STR_LEN)) {
		if (platform == INTEL_MID_CPU_CHIP_CLOVERVIEW) {
			ret = intel_scu_ipc_read_shim(&shim_data,
					shim_flis_addr,	shim_offset);
		} else {
			/* use the same variable name to be compatible */
			shim_data = get_flis_value(shim_offset);
		}
	} else if (!strncmp("set", shim_ops, OPS_STR_LEN)) {
		if (platform == INTEL_MID_CPU_CHIP_CLOVERVIEW) {
			ret = intel_scu_ipc_write_shim(shim_data,
					shim_flis_addr,	shim_offset);
		} else {
			set_flis_value(shim_data, shim_offset);
		}
	} else {
		dev_err(dev, "Not supported ops\n");
		ret = -EINVAL;
	}

	if (ret) {
		dev_err(dev, "get/set flis error, ret = %d\n", ret);
		return ret;
	}

	return size;
}

static ssize_t param_val_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", param_value);
}

static ssize_t param_val_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	flis_generic_store(buf, DBG_PARAM_VAL);
	return size;
}

static ssize_t flis_param_type_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", param_type);
}

static ssize_t flis_param_type_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	flis_generic_store(buf, DBG_PARAM_TYPE);
	return size;
}

static ssize_t pinname_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", pin_name);
}

static ssize_t pinname_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	flis_generic_store(buf, DBG_PIN_NAME);
	return size;
}

static ssize_t ops_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;

	memset(ops, 0, sizeof(ops));

	ret = sscanf(buf, "%9s", ops);
	if (ret != 1) {
		dev_err(dev, "input error\n");
		return -EINVAL;
	}

	if (!strncmp("get", ops, OPS_STR_LEN))
		ret = get_pin_flis(pin_name, param_type, &param_value);
	else if (!strncmp("set", ops, OPS_STR_LEN))
		ret = config_pin_flis(pin_name, param_type, param_value);
	else {
		dev_err(dev, "wrong ops\n");
		ret = -EINVAL;
	}

	if (ret) {
		dev_err(dev, "Access flis error, ret = %d\n", ret);
		return ret;
	}

	return size;
}

#ifdef CONFIG_X86_CTP
static DEVICE_ATTR(flis_addr, S_IRUSR|S_IWUSR,
		shim_flis_addr_show, shim_flis_addr_store);
#endif
static DEVICE_ATTR(offset, S_IRUSR|S_IWUSR,
		shim_offset_show, shim_offset_store);
static DEVICE_ATTR(data, S_IRUSR|S_IWUSR, shim_data_show, shim_data_store);
static DEVICE_ATTR(flis_ops, S_IWUSR, NULL, shim_ops_store);

static struct attribute *flis_attrs[] = {
#ifdef CONFIG_X86_CTP
	&dev_attr_flis_addr.attr,
#endif
	&dev_attr_offset.attr,
	&dev_attr_data.attr,
	&dev_attr_flis_ops.attr,
	NULL,
};

static struct attribute_group flis_attr_group = {
	.name = "flis_debug",
	.attrs = flis_attrs,
};

static DEVICE_ATTR(pin_name, S_IRUSR|S_IWUSR, pinname_show, pinname_store);
static DEVICE_ATTR(flis_param, S_IRUSR|S_IWUSR, flis_param_type_show,
						flis_param_type_store);
static DEVICE_ATTR(val, S_IRUSR|S_IWUSR, param_val_show, param_val_store);
static DEVICE_ATTR(ops, S_IWUSR, NULL, ops_store);

static struct attribute *pin_config_attrs[] = {
	&dev_attr_pin_name.attr,
	&dev_attr_flis_param.attr,
	&dev_attr_val.attr,
	&dev_attr_ops.attr,
	NULL,
};

static struct attribute_group pin_config_attr_group = {
	.name = "pin_config_debug",
	.attrs = pin_config_attrs,
};

static int scu_flis_probe(struct platform_device *pdev)
{
	int ret;
	struct intel_scu_flis_info *isfi = &flis_info;
	struct intel_scu_flis_platform_data *pdata = pdev->dev.platform_data;

	if (!pdata) {
		dev_err(&pdev->dev, "No platform data\n");
		ret = -EINVAL;
		goto out;
	}

	platform = intel_mid_identify_cpu();
	isfi->pin_t = pdata->pin_t;
	isfi->pin_num = pdata->pin_num;
	isfi->shim_access = pdata->shim_access;
	isfi->mmio_flis_t = pdata->mmio_flis_t;
	if (pdata->mmio_flis_t && pdata->flis_base) {
		isfi->flis_paddr = pdata->flis_base;
		isfi->flis_base = ioremap_nocache(pdata->flis_base,
					pdata->flis_len);
		if (!isfi->flis_base) {
			dev_err(&pdev->dev, "error mapping flis base\n");
			ret = -EFAULT;
			goto out;
		}
	}

	if ((isfi->pin_t || isfi->mmio_flis_t)&&isfi->pin_num)
		isfi->initialized = 1;

	ret = sysfs_create_group(&pdev->dev.kobj, &flis_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create flis sysfs interface\n");
		goto err1;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &pin_config_attr_group);
	if (ret) {
		dev_err(&pdev->dev,
				"Failed to create pin config sysfs interface\n");
		goto err2;
	}

	dev_info(&pdev->dev, "scu flis probed\n");
	return 0;

err2:
	sysfs_remove_group(&pdev->dev.kobj, &flis_attr_group);
err1:
	if (pdata->flis_base)
		iounmap(isfi->flis_base);
out:
	isfi->initialized = 0;
	return ret;
}

static int scu_flis_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &pin_config_attr_group);
	sysfs_remove_group(&pdev->dev.kobj, &flis_attr_group);

	return 0;
}

static struct platform_driver scu_flis_driver = {
	.driver = {
		   .name = "intel_scu_flis",
		   .owner = THIS_MODULE,
		   },
	.probe = scu_flis_probe,
	.remove = scu_flis_remove,
};

static int scu_flis_module_init(void)
{
	return platform_driver_register(&scu_flis_driver);
}

static void scu_flis_module_exit(void)
{
	platform_driver_unregister(&scu_flis_driver);
}

static int flis_rpmsg_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;

	if (rpdev == NULL) {
		pr_err("rpmsg channel not created\n");
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed flis rpmsg device\n");

	/* Allocate rpmsg instance for flis*/
	ret = alloc_rpmsg_instance(rpdev, &flis_instance);
	if (!flis_instance) {
		dev_err(&rpdev->dev, "kzalloc flis instance failed\n");
		goto out;
	}

	/* Initialize rpmsg instance */
	init_rpmsg_instance(flis_instance);

	ret = scu_flis_module_init();
	if (ret)
		free_rpmsg_instance(rpdev, &flis_instance);

out:
	return ret;
}

static void flis_rpmsg_remove(struct rpmsg_channel *rpdev)
{
	scu_flis_module_exit();
	free_rpmsg_instance(rpdev, &flis_instance);
	dev_info(&rpdev->dev, "Removed flis rpmsg device\n");
}

static void flis_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
					int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
		       data, len,  true);
}

static struct rpmsg_device_id flis_rpmsg_id_table[] = {
	{ .name	= "rpmsg_flis" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, flis_rpmsg_id_table);

static struct rpmsg_driver flis_rpmsg = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= flis_rpmsg_id_table,
	.probe		= flis_rpmsg_probe,
	.callback	= flis_rpmsg_cb,
	.remove		= flis_rpmsg_remove,
};

static int __init flis_rpmsg_init(void)
{
	return register_rpmsg_driver(&flis_rpmsg);
}

static void __exit flis_rpmsg_exit(void)
{
	return unregister_rpmsg_driver(&flis_rpmsg);
}

fs_initcall(flis_rpmsg_init);
module_exit(flis_rpmsg_exit);

MODULE_AUTHOR("Ning Li <ning.li@intel.com>");
MODULE_DESCRIPTION("Intel FLIS Access Driver");
MODULE_LICENSE("GPL v2");
