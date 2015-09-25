/*
 * Copyright (C) 2013 - 2015 Motorola Mobility LLC
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

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <soc/qcom/socinfo.h>
#include <linux/pstore.h>

struct mmi_msm_bin {
	int	set;
	int	speed;
	int	pvs;
	int	ver;
};

#define MMI_MSM_BIN_INVAL	INT_MAX
#define ACPU_BIN_SET		BIT(0)

static struct mmi_msm_bin mmi_msm_bin_info;
static DEFINE_SPINLOCK(mmi_msm_bin_lock);

static inline void mmi_panic_annotate(const char *str)
{
	pstore_annotate(str);
}

static void __init mmi_msm_annotate_socinfo(void)
{
	char socinfo[32];

	snprintf(socinfo, sizeof(socinfo), "socinfo: id=%u, ",
			socinfo_get_id());
	mmi_panic_annotate(socinfo);

	snprintf(socinfo, sizeof(socinfo), "ver=%u.%u, ",
			SOCINFO_VERSION_MAJOR(socinfo_get_version()),
			SOCINFO_VERSION_MINOR(socinfo_get_version()));
	mmi_panic_annotate(socinfo);

	snprintf(socinfo, sizeof(socinfo), "raw_id=%u, ",
			socinfo_get_raw_id());
	mmi_panic_annotate(socinfo);

	snprintf(socinfo, sizeof(socinfo), "raw_ver=%u, ",
			socinfo_get_raw_version());
	mmi_panic_annotate(socinfo);

	snprintf(socinfo, sizeof(socinfo), "hw_plat=%u, ",
			socinfo_get_platform_type());
	mmi_panic_annotate(socinfo);

	snprintf(socinfo, sizeof(socinfo), "hw_plat_ver=%u, ",
			socinfo_get_platform_version());
	mmi_panic_annotate(socinfo);

	snprintf(socinfo, sizeof(socinfo), "hw_plat_subtype=%u\n",
			socinfo_get_platform_subtype());
	mmi_panic_annotate(socinfo);
}

static ssize_t mmi_acpu_proc_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	void *data = PDE_DATA(file_inode(file));
	char local_buf[8];
	int len = snprintf(local_buf, 2, "%1p", data);

	return simple_read_from_buffer(buf, count, ppos, local_buf, len);
}

static const struct file_operations mmi_acpu_proc_fops = {
	.read = mmi_acpu_proc_read,
	.llseek = default_llseek,
};

void mmi_acpu_bin_set(int *speed, int *pvs, int *ver)
{
	unsigned long flags;

	spin_lock_irqsave(&mmi_msm_bin_lock, flags);
	if (mmi_msm_bin_info.set & ACPU_BIN_SET) {
		spin_unlock_irqrestore(&mmi_msm_bin_lock, flags);
		return;
	}
	mmi_msm_bin_info.speed = speed ? *speed : MMI_MSM_BIN_INVAL;
	mmi_msm_bin_info.pvs = pvs ? *pvs : MMI_MSM_BIN_INVAL;
	mmi_msm_bin_info.ver = ver ? *ver : MMI_MSM_BIN_INVAL;
	mmi_msm_bin_info.set |= ACPU_BIN_SET;
	spin_unlock_irqrestore(&mmi_msm_bin_lock, flags);
}

static void __init mmi_msm_acpu_bin_export(void)
{
	struct proc_dir_entry *proc;
	unsigned long flags;
	char acpu[64];

	spin_lock_irqsave(&mmi_msm_bin_lock, flags);
	if (!(mmi_msm_bin_info.set & ACPU_BIN_SET)) {
		spin_unlock_irqrestore(&mmi_msm_bin_lock, flags);
		pr_err("ACPU Bin is not available.\n");
		return;
	}
	spin_unlock_irqrestore(&mmi_msm_bin_lock, flags);

	mmi_panic_annotate("ACPU: ");
	if (mmi_msm_bin_info.speed != MMI_MSM_BIN_INVAL) {
		snprintf(acpu, sizeof(acpu), "Speed bin %d ",
				mmi_msm_bin_info.speed);
		mmi_panic_annotate(acpu);
	}
	if (mmi_msm_bin_info.pvs != MMI_MSM_BIN_INVAL) {
		proc = proc_create_data("cpu/msm_acpu_pvs",
			(S_IFREG | S_IRUGO), NULL,
			&mmi_acpu_proc_fops, (void *)(uintptr_t)mmi_msm_bin_info.pvs);
		if (!proc)
			pr_err("Failed to create /proc/cpu/msm_acpu_pvs.\n");
		else
			proc_set_size(proc, 1);
		snprintf(acpu, sizeof(acpu), "PVS bin %d ",
				mmi_msm_bin_info.pvs);
		mmi_panic_annotate(acpu);
	}
	if (mmi_msm_bin_info.ver != MMI_MSM_BIN_INVAL) {
		snprintf(acpu, sizeof(acpu), "PVS version %d ",
				mmi_msm_bin_info.ver);
		mmi_panic_annotate(acpu);
	}
	mmi_panic_annotate("\n");
}

static int __init init_mmi_soc_info(void)
{
	mmi_msm_annotate_socinfo();
	mmi_msm_acpu_bin_export();
	return 0;
}
module_init(init_mmi_soc_info);
MODULE_DESCRIPTION("Motorola Mobility LLC. SOC Info");
MODULE_LICENSE("GPL v2");
