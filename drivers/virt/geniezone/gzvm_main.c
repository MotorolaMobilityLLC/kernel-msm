// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/kdev_t.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/soc/mediatek/gzvm_drv.h>

static struct gzvm_driver gzvm_drv = {
	.drv_version = {
		.major = GZVM_DRV_MAJOR_VERSION,
		.minor = GZVM_DRV_MINOR_VERSION,
		.sub = 0,
	},
};

static ssize_t demand_paging_batch_pages_show(struct kobject *kobj,
					      struct kobj_attribute *attr,
					      char *buf)
{
	return sprintf(buf, "%u\n", gzvm_drv.demand_paging_batch_pages);
}

static ssize_t demand_paging_batch_pages_store(struct kobject *kobj,
					       struct kobj_attribute *attr,
					       const char *buf, size_t count)
{
	int ret;
	u32 temp;

	ret = kstrtoint(buf, 10, &temp);
	if (ret < 0)
		return ret;

	if (temp == 0 || (PMD_SIZE % (PAGE_SIZE * temp)) != 0)
		return -EINVAL;

	gzvm_drv.demand_paging_batch_pages = temp;

	return count;
}

static ssize_t destroy_batch_pages_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%u\n", gzvm_drv.destroy_batch_pages);
}

static ssize_t destroy_batch_pages_store(struct kobject *kobj,
					 struct kobj_attribute *attr,
					 const char *buf, size_t count)
{
	int ret;
	u32 temp;

	ret = kstrtoint(buf, 10, &temp);
	if (ret < 0)
		return ret;

	// destroy page batch size should be power of 2
	if ((temp & (temp - 1)) != 0)
		return -EINVAL;

	gzvm_drv.destroy_batch_pages = temp;

	return count;
}

/* /sys/kernel/gzvm/demand_paging_batch_pages */
static struct kobj_attribute demand_paging_batch_pages_attr = {
		.attr = {
			.name = "demand_paging_batch_pages",
			.mode = 0660,
		},
		.show = demand_paging_batch_pages_show,
		.store = demand_paging_batch_pages_store,
};

/* /sys/kernel/gzvm/destroy_batch_pages */
static struct kobj_attribute destroy_batch_pages_attr = {
		.attr = {
			.name = "destroy_batch_pages",
			.mode = 0660,
		},
		.show = destroy_batch_pages_show,
		.store = destroy_batch_pages_store,
};

static int gzvm_drv_sysfs_init(void)
{
	int ret = 0;

	gzvm_drv.sysfs_root_dir = kobject_create_and_add("gzvm", kernel_kobj);

	if (!gzvm_drv.sysfs_root_dir)
		return -ENOMEM;

	ret = sysfs_create_file(gzvm_drv.sysfs_root_dir,
				&demand_paging_batch_pages_attr.attr);
	if (ret)
		pr_debug("failed to create demand_batch_pages in /sys/kernel/gzvm\n");

	ret = sysfs_create_file(gzvm_drv.sysfs_root_dir,
				&destroy_batch_pages_attr.attr);
	if (ret)
		pr_debug("failed to create destroy_batch_pages in /sys/kernel/gzvm\n");

	return ret;
}

static void gzvm_drv_sysfs_exit(void)
{
	kobject_del(gzvm_drv.sysfs_root_dir);
}

static int gzvm_drv_debug_init(void)
{
	if (!debugfs_initialized()) {
		pr_warn("debugfs not initialized!\n");
		return 0;
	}

	gzvm_drv.gzvm_debugfs_dir = debugfs_create_dir("gzvm", NULL);
	if (!gzvm_drv.gzvm_debugfs_dir)
		return -ENOMEM;

	return 0;
}

static void gzvm_drv_debug_exit(void)
{
	debugfs_remove_recursive(gzvm_drv.gzvm_debugfs_dir);
}

/**
 * gzvm_err_to_errno() - Convert geniezone return value to standard errno
 *
 * @err: Return value from geniezone function return
 *
 * Return: Standard errno
 */
int gzvm_err_to_errno(unsigned long err)
{
	int gz_err = (int)err;

	switch (gz_err) {
	case 0:
		return 0;
	case ERR_NO_MEMORY:
		return -ENOMEM;
	case ERR_INVALID_ARGS:
		return -EINVAL;
	case ERR_NOT_SUPPORTED:
		fallthrough;
	case ERR_NOT_IMPLEMENTED:
		return -EOPNOTSUPP;
	case ERR_FAULT:
		return -EFAULT;
	case ERR_BUSY:
		return -EAGAIN;
	default:
		break;
	}

	return -EINVAL;
}

/**
 * gzvm_dev_ioctl_check_extension() - Check if given capability is support
 *				      or not
 *
 * @gzvm: Pointer to struct gzvm
 * @args: Pointer in u64 from userspace
 *
 * Return:
 * * 0			- Supported, no error
 * * -EOPNOTSUPP	- Unsupported
 * * -EFAULT		- Failed to get data from userspace
 */
long gzvm_dev_ioctl_check_extension(struct gzvm *gzvm, unsigned long args)
{
	__u64 cap;
	void __user *argp = (void __user *)args;

	if (copy_from_user(&cap, argp, sizeof(uint64_t)))
		return -EFAULT;
	return gzvm_arch_check_extension(gzvm, cap, argp);
}

static long gzvm_dev_ioctl(struct file *filp, unsigned int cmd,
			   unsigned long user_args)
{
	switch (cmd) {
	case GZVM_CREATE_VM:
		return gzvm_dev_ioctl_create_vm(&gzvm_drv, user_args);
	case GZVM_CHECK_EXTENSION:
		if (!user_args)
			return -EINVAL;
		return gzvm_dev_ioctl_check_extension(NULL, user_args);
	default:
		break;
	}

	return -ENOTTY;
}

static int gzvm_dev_open(struct inode *inode, struct file *file)
{
	/*
	 * Reference count to prevent this module is unload without destroying
	 * VM
	 */
	try_module_get(THIS_MODULE);
	return 0;
}

static int gzvm_dev_release(struct inode *inode, struct file *file)
{
	module_put(THIS_MODULE);
	return 0;
}

static const struct file_operations gzvm_chardev_ops = {
	.unlocked_ioctl = gzvm_dev_ioctl,
	.llseek		= noop_llseek,
	.open		= gzvm_dev_open,
	.release	= gzvm_dev_release,
};

static struct miscdevice gzvm_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = KBUILD_MODNAME,
	.fops = &gzvm_chardev_ops,
};

static int gzvm_query_hyp_batch_pages(void)
{
	struct gzvm_enable_cap cap = {0};
	int ret;

	gzvm_drv.demand_paging_batch_pages = GZVM_DRV_DEMAND_PAGING_BATCH_PAGES;
	cap.cap = GZVM_CAP_QUERY_HYP_BATCH_PAGES;

	ret = gzvm_arch_query_hyp_batch_pages(&cap, NULL);
	if (!ret)
		gzvm_drv.demand_paging_batch_pages = cap.args[0];

	/*
	 * We have initialized demand_paging_batch_pages, and to maintain
	 * compatibility with older GZ version, we can ignore the return value.
	 */
	if (ret == -EINVAL)
		return 0;
	return ret;
}

static int gzvm_query_destroy_batch_pages(void)
{
	int ret;
	struct gzvm_enable_cap cap = {0};

	gzvm_drv.destroy_batch_pages = GZVM_DRV_DESTROY_PAGING_BATCH_PAGES;
	cap.cap = GZVM_CAP_QUERY_DESTROY_BATCH_PAGES;

	ret = gzvm_arch_query_destroy_batch_pages(&cap, NULL);
	if (!ret)
		gzvm_drv.destroy_batch_pages = cap.args[0];
	return ret;
}

static int gzvm_drv_probe(struct platform_device *pdev)
{
	int ret;

	if (gzvm_arch_probe(gzvm_drv.drv_version, &gzvm_drv.hyp_version) != 0) {
		dev_err(&pdev->dev, "Not found available conduit\n");
		return -ENODEV;
	}

	pr_debug("Found GenieZone hypervisor version %u.%u.%llu\n",
		 gzvm_drv.hyp_version.major, gzvm_drv.hyp_version.minor,
		 gzvm_drv.hyp_version.sub);

	ret = gzvm_arch_drv_init();
	if (ret)
		return ret;

	ret = misc_register(&gzvm_dev);
	if (ret)
		return ret;

	ret = gzvm_drv_irqfd_init();
	if (ret)
		return ret;

	ret = gzvm_drv_debug_init();
	if (ret)
		return ret;

	ret = gzvm_drv_sysfs_init();
	if (ret)
		return ret;

	ret = gzvm_query_hyp_batch_pages();
	if (ret)
		return ret;

	ret = gzvm_query_destroy_batch_pages();
	if (ret)
		return ret;

	return 0;
}

static int gzvm_drv_remove(struct platform_device *pdev)
{
	gzvm_drv_irqfd_exit();
	misc_deregister(&gzvm_dev);
	gzvm_drv_debug_exit();
	gzvm_drv_sysfs_exit();
	return 0;
}

static const struct of_device_id gzvm_of_match[] = {
	{ .compatible = "mediatek,geniezone" },
	{/* sentinel */},
};

static struct platform_driver gzvm_driver = {
	.probe = gzvm_drv_probe,
	.remove = gzvm_drv_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = gzvm_of_match,
	},
};

module_platform_driver(gzvm_driver);

MODULE_DEVICE_TABLE(of, gzvm_of_match);
MODULE_AUTHOR("MediaTek");
MODULE_DESCRIPTION("GenieZone interface for VMM");
MODULE_LICENSE("GPL");
