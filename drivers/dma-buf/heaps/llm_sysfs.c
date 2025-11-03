// SPDX-License-Identifier: GPL-2.0-only
 /* Copyright 2025 Moto LLC */


#define pr_fmt(fmt) "llm: " fmt


#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/bvec.h>
#include <linux/kthread.h>
#include <linux/ctype.h>
#include "llm_heap.h"
#include "llm.h"



#define LLMHEAP_IOCTL_LOAD_FILE  _IOWR('l', 0, struct llm_load_dma_buf)
#define LLMHEAP_IOCTL_DEPOSIT_FILE  _IOWR('l', 1, struct llm_file_info)

DEFINE_STATIC_KEY_TRUE(llmheap_enable);
DEFINE_STATIC_KEY_FALSE(cache_enable);


static int llmheap_open(struct inode *inode, struct file *file)
{
	if (!llmheap_enabled()) {
		pr_err("ERROR!!! call %s when llmheap is disable!\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static long llmheap_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

    switch (cmd) {
		case LLMHEAP_IOCTL_LOAD_FILE:
            return llmheap_handle_load_data(arg);
		case LLMHEAP_IOCTL_DEPOSIT_FILE:
			return llmheap_handle_deposit_file_info(arg);
		default:
			return -EINVAL;
	}
}

static ssize_t drop_caches_store(struct device *dev, struct device_attribute *attr,
             const char *buf, size_t count)

{
    int opt, ret;
    ret = kstrtoint(buf, 10, &opt);
    if ((ret == 0)  && ( opt ==3 ))
    {
        llm_cache_drop_all();
    }
    return count;
}
static DEVICE_ATTR_WO(drop_caches);

static ssize_t enabled_store(struct device *dev, struct device_attribute *attr,
             const char *buf, size_t count)
{
    int opt,ret;
    ret = kstrtoint(buf, 10, &opt);
    if (ret == 0)
    {
        if ((opt) && !llmheap_enabled())
            static_branch_enable(&llmheap_enable);
        else if ((!opt ) && llmheap_enabled())
            static_branch_disable(&llmheap_enable);
    }
    return count;
}

static ssize_t enabled_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", llmheap_enabled());
}

static DEVICE_ATTR_RW(enabled);



static ssize_t cache_store(struct device *dev, struct device_attribute *attr,
             const char *buf, size_t count)
{
    int opt,ret;
    ret = kstrtoint(buf, 10, &opt);
    if (ret == 0)
    {
        if ((opt) && !cache_enabled())
            static_branch_enable(&cache_enable);
        else if ((!opt ) && cache_enabled())
        {
            static_branch_disable(&cache_enable);
            llm_cache_drop_all();
        }
    }
    return count;
}

static ssize_t cache_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", cache_enabled());
}

static DEVICE_ATTR_RW(cache);



static ssize_t cache_stats_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    return cache_stat_info(buf,  PAGE_SIZE);
}
static DEVICE_ATTR_ADMIN_RO(cache_stats);



static struct attribute *llmheap_attrs[] = {
    &dev_attr_drop_caches.attr,
    &dev_attr_enabled.attr,
    &dev_attr_cache_stats.attr,
    &dev_attr_cache.attr,
	NULL
};

static struct attribute_group llmheap_attr_group = {
	.attrs = llmheap_attrs,
};

static const struct attribute_group *llmheap_attr_groups[] = {
    &llmheap_attr_group,
    NULL,
};


static const struct file_operations llmheap_fops = {
	.owner = THIS_MODULE,
	.open = llmheap_open,
	.unlocked_ioctl = llmheap_ioctl,
};

static struct miscdevice llmheap_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "llm_heap",
	.fops = &llmheap_fops,
    .mode = 0660,
    .groups = llmheap_attr_groups,
};

int llm_heap_fs_init(void)
{
    int ret;

    ret = misc_register(&llmheap_misc_device);
	if (ret) {
		pr_err("Failed to register llmheap\n");
		return ret;
	}

    return 0;
}
void llm_heap_fs_deinit(void)
{
    misc_deregister(&llmheap_misc_device);
}
