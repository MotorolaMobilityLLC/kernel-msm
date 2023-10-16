// SPDX-License-Identifier: GPL-2.0-only
/*
 * Common crypto library for storage encryption.
 *
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/crypto-qti-common.h>
#include <linux/module.h>
#include "crypto-qti-ice-regs.h"
#include "crypto-qti-platform.h"

#include <linux/of.h>
#include <linux/blkdev.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/kobject.h>
#include <linux/genhd.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/stringify.h>
#include <linux/mutex.h>
#include <linux/qseecom_kernel.h>

#if IS_ENABLED(CONFIG_QTI_CRYPTO_FDE)
#define CRYPTO_ICE_TYPE_NAME_LEN        8
#define CRYPTO_ICE_ENCRYPT              0x1
#define CRYPTO_ICE_DECRYPT              0x2
#define CRYPTO_SECT_LEN_IN_BYTE         512
#define TOTAL_NUMBER_ICE_SLOTS          32
#define CRYPTO_ICE_UDEV_PARTITION_NAME  96

#define CRYPTO_ICE_FDE_KEY_INDEX        31
#define CRYPTO_UD_VOLNAME               "userdata"
#define CRYPTO_ICE_HASH_SIZE            32
#define CRYPTO_ICE_KEY_ID_SIZE          32

#define CRYPTO_ICE_FDE_UFS_KEYID       "UFS ICE Full Disk Encryption    "
#define CRYPTO_ICE_FDE_EMMC_KEYID      "SDCC ICE Full Disk Encryption   "

/* add the legacy key_id to support ICE OTA */
#define CRYPTO_ICE_FDE_UFS_LEGACY_KEYID       "UFS ICE Full Disk Encryption"
#define CRYPTO_ICE_FDE_EMMC_LEGACY_KEYID      "SDCC ICE Full Disk Encryption"

#define CRYPTO_ICE_UFS_DEV_NAME         "ufshcd-qcom"
#define CRYPTO_ICE_EMMC_DEV_NAME        "sdhci_msm"
#define QSEECOM_KEY_ID_EXISTS           -65

#define _INIT_ATTRIBUTE(_name, _mode) \
	{ \
		.name = __stringify(_name), \
		.mode = (_mode), \
	}

#define PART_CFG_ATTR_RW(_name) \
	struct kobj_attribute part_cfg_attr_##_name = { \
		.attr = _INIT_ATTRIBUTE(_name, 0660), \
		.show = crypto_qti_ice_part_cfg_show, \
		.store = crypto_qti_ice_part_cfg_store, \
	}

/* Set the "add_partition" node attributes, allow user to add partitions */
#define __ATTR_WR_PARTITIONS(_name) {                      \
	.attr  = { .name = __stringify(_name), .mode = 0220 }, \
	.store = _name##_store,                                \
}

/*
 * Encapsulates configuration information for each supported partition
 */
struct ice_part_cfg {
	struct list_head list;
	char volname[PARTITION_META_INFO_VOLNAMELTH + 1]; /* NULL terminated */
	uint32_t key_slot; /*ICE keyslot 0..31 */
	uint32_t add_partition_result; /*Add partition operation res*/
	bool encr_bypass;
	bool decr_bypass;
	struct kobject kobj;
	struct kobj_type kobj_type;
};

struct ice_clk_info {
	struct list_head list;
	struct clk *clk;
	const char *name;
	u32 max_freq;
	u32 min_freq;
	u32 curr_freq;
	bool enabled;
};

static LIST_HEAD(ice_devices);

/*
 * ICE HW device structure.
 */
struct ice_device {
	struct list_head	list;
	struct kset			*fde_partitions;
	struct mutex		mutex;
	struct device		*pdev;
	dev_t			device_no;
	void __iomem		*mmio;
	int			irq;
	bool			is_ice_enabled;
	ice_error_cb		error_cb;
	void			*host_controller_data; /* UFS/EMMC/other? */
	struct list_head	clk_list_head;
	u32			ice_hw_version;
	bool			is_ice_clk_available;
	char			ice_instance_type[CRYPTO_ICE_TYPE_NAME_LEN];
	struct regulator	*reg;
	bool			is_regulator_available;

	bool			sysfs_groups_created;

	/* Partition list for full disk encryption */
	struct list_head	part_cfg_list;
	struct kobj_type	partitions_kobj_type;
	u32					num_fde_slots;
	u32					num_fde_slots_in_use;
};

static int crypto_qti_ice_init(struct ice_device *ice_dev,
			       void *host_controller_data,
			       ice_error_cb error_cb);
#endif /* CONFIG_QTI_CRYPTO_FDE */

static int ice_check_fuse_setting(void __iomem *ice_mmio)
{
	uint32_t regval;
	uint32_t version, major, minor;

	version = ice_readl(ice_mmio, ICE_REGS_VERSION);
	major = (version & ICE_CORE_MAJOR_REV_MASK) >>
			ICE_CORE_MAJOR_REV;
	minor = (version & ICE_CORE_MINOR_REV_MASK) >>
			ICE_CORE_MINOR_REV;

	/* Check fuse setting is not supported on ICE 3.2 onwards */
	if ((major == 0x03) && (minor >= 0x02))
		return 0;
	regval = ice_readl(ice_mmio, ICE_REGS_FUSE_SETTING);
	regval &= (ICE_FUSE_SETTING_MASK |
		ICE_FORCE_HW_KEY0_SETTING_MASK |
		ICE_FORCE_HW_KEY1_SETTING_MASK);

	if (regval) {
		pr_err("%s: error: ICE_ERROR_HW_DISABLE_FUSE_BLOWN\n", __func__);
		return -EPERM;
	}
	return 0;
}

static int ice_check_version(void __iomem *ice_mmio)
{
	uint32_t version, major, minor, step;

	version = ice_readl(ice_mmio, ICE_REGS_VERSION);
	major = (version & ICE_CORE_MAJOR_REV_MASK) >> ICE_CORE_MAJOR_REV;
	minor = (version & ICE_CORE_MINOR_REV_MASK) >> ICE_CORE_MINOR_REV;
	step = (version & ICE_CORE_STEP_REV_MASK) >> ICE_CORE_STEP_REV;

	if (major < ICE_CORE_CURRENT_MAJOR_VERSION) {
		pr_err("%s: Unknown ICE device at %lu, rev %d.%d.%d\n",
			__func__, (unsigned long)ice_mmio,
				major, minor, step);
		return -ENODEV;
	}

	return 0;
}

int crypto_qti_init_crypto(void *mmio_data)
{
	int err = 0;
	void __iomem *ice_mmio = (void __iomem *) mmio_data;

	err = ice_check_version(ice_mmio);
	if (err) {
		pr_err("%s: check version failed, err %d\n", __func__, err);
		return err;
	}

	err = ice_check_fuse_setting(ice_mmio);
	if (err)
		pr_err("%s: check fuse failed, err %d\n", __func__, err);

	return err;
}
EXPORT_SYMBOL(crypto_qti_init_crypto);

static void ice_low_power_and_optimization_enable(void __iomem *ice_mmio)
{
	uint32_t regval;

	regval = ice_readl(ice_mmio, ICE_REGS_ADVANCED_CONTROL);
	/* Enable low power mode sequence
	 * [0]-0,[1]-0,[2]-0,[3]-7,[4]-0,[5]-0,[6]-0,[7]-0,
	 * Enable CONFIG_CLK_GATING, STREAM2_CLK_GATING and STREAM1_CLK_GATING
	 */
	regval |= 0x7000;
	/* Optimization enable sequence
	 */
	regval |= 0xD807100;
	ice_writel(ice_mmio, regval, ICE_REGS_ADVANCED_CONTROL);
	/*
	 * Memory barrier - to ensure write completion before next transaction
	 */
	wmb();
}

static int ice_wait_bist_status(void __iomem *ice_mmio)
{
	int count;
	uint32_t regval;

	for (count = 0; count < QTI_ICE_MAX_BIST_CHECK_COUNT; count++) {
		regval = ice_readl(ice_mmio, ICE_REGS_BIST_STATUS);
		if (!(regval & ICE_BIST_STATUS_MASK))
			break;
		udelay(50);
	}

	if (regval) {
		pr_err("%s: wait bist status failed, reg %d\n", __func__, regval);
		return -ETIMEDOUT;
	}

	return 0;
}

int crypto_qti_enable(void *mmio_data)
{
	int err = 0;
	void __iomem *ice_mmio = (void __iomem *) mmio_data;

	ice_low_power_and_optimization_enable(ice_mmio);
	err = ice_wait_bist_status(ice_mmio);
	if (err)
		return err;

	return err;
}
EXPORT_SYMBOL(crypto_qti_enable);

void crypto_qti_disable(void)
{
	crypto_qti_disable_platform();
}
EXPORT_SYMBOL(crypto_qti_disable);

int crypto_qti_resume(void *mmio_data)
{
	void __iomem *ice_mmio = (void __iomem *) mmio_data;

	return ice_wait_bist_status(ice_mmio);
}
EXPORT_SYMBOL(crypto_qti_resume);

static void ice_dump_test_bus(void __iomem *ice_mmio)
{
	uint32_t regval = 0x1;
	uint32_t val;
	uint8_t bus_selector;
	uint8_t stream_selector;

	pr_err("ICE TEST BUS DUMP:\n");

	for (bus_selector = 0; bus_selector <= 0xF;  bus_selector++) {
		regval = 0x1;	/* enable test bus */
		regval |= bus_selector << 28;
		if (bus_selector == 0xD)
			continue;
		ice_writel(ice_mmio, regval, ICE_REGS_TEST_BUS_CONTROL);
		/*
		 * make sure test bus selector is written before reading
		 * the test bus register
		 */
		wmb();
		val = ice_readl(ice_mmio, ICE_REGS_TEST_BUS_REG);
		pr_err("ICE_TEST_BUS_CONTROL: 0x%08x | ICE_TEST_BUS_REG: 0x%08x\n",
			regval, val);
	}

	pr_err("ICE TEST BUS DUMP (ICE_STREAM1_DATAPATH_TEST_BUS):\n");
	for (stream_selector = 0; stream_selector <= 0xF; stream_selector++) {
		regval = 0xD0000001;	/* enable stream test bus */
		regval |= stream_selector << 16;
		ice_writel(ice_mmio, regval, ICE_REGS_TEST_BUS_CONTROL);
		/*
		 * make sure test bus selector is written before reading
		 * the test bus register
		 */
		wmb();
		val = ice_readl(ice_mmio, ICE_REGS_TEST_BUS_REG);
		pr_err("ICE_TEST_BUS_CONTROL: 0x%08x | ICE_TEST_BUS_REG: 0x%08x\n",
			regval, val);
	}
}


int crypto_qti_debug(const struct ice_mmio_data *mmio_data)
{
	void __iomem *ice_mmio =  mmio_data->ice_base_mmio;

	pr_err("ICE Control: 0x%08x | ICE Reset: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_CONTROL),
		ice_readl(ice_mmio, ICE_REGS_RESET));

	pr_err("ICE Version: 0x%08x | ICE FUSE:	0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_VERSION),
		ice_readl(ice_mmio, ICE_REGS_FUSE_SETTING));

	pr_err("%s: ICE Param1: 0x%08x | ICE Param2:  0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_PARAMETERS_1),
		ice_readl(ice_mmio, ICE_REGS_PARAMETERS_2));

	pr_err("%s: ICE Param3: 0x%08x | ICE Param4:  0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_PARAMETERS_3),
		ice_readl(ice_mmio, ICE_REGS_PARAMETERS_4));

	pr_err("%s: ICE Param5: 0x%08x | ICE IRQ STTS:  0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_PARAMETERS_5),
		ice_readl(ice_mmio, ICE_REGS_NON_SEC_IRQ_STTS));

	pr_err("%s: ICE IRQ MASK: 0x%08x | ICE IRQ CLR:	0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_NON_SEC_IRQ_MASK),
		ice_readl(ice_mmio, ICE_REGS_NON_SEC_IRQ_CLR));

	pr_err("%s: ICE INVALID CCFG ERR STTS: 0x%08x\n",
		ice_readl(ice_mmio, ICE_INVALID_CCFG_ERR_STTS));

	pr_err("%s: ICE BIST Sts: 0x%08x | ICE Bypass Sts:  0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_BIST_STATUS),
		ice_readl(ice_mmio, ICE_REGS_BYPASS_STATUS));

	pr_err("%s: ICE ADV CTRL: 0x%08x | ICE ENDIAN SWAP:	0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_ADVANCED_CONTROL),
		ice_readl(ice_mmio, ICE_REGS_ENDIAN_SWAP));

	pr_err("%s: ICE_STM1_ERR_SYND1: 0x%08x | ICE_STM1_ERR_SYND2: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM1_ERROR_SYNDROME1),
		ice_readl(ice_mmio, ICE_REGS_STREAM1_ERROR_SYNDROME2));

	pr_err("%s: ICE_STM2_ERR_SYND1: 0x%08x | ICE_STM2_ERR_SYND2: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM2_ERROR_SYNDROME1),
		ice_readl(ice_mmio, ICE_REGS_STREAM2_ERROR_SYNDROME2));

	pr_err("%s: ICE_STM1_COUNTER1: 0x%08x | ICE_STM1_COUNTER2: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM1_COUNTERS1),
		ice_readl(ice_mmio, ICE_REGS_STREAM1_COUNTERS2));

	pr_err("%s: ICE_STM1_COUNTER3: 0x%08x | ICE_STM1_COUNTER4: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM1_COUNTERS3),
		ice_readl(ice_mmio, ICE_REGS_STREAM1_COUNTERS4));

	pr_err("%s: ICE_STM2_COUNTER1: 0x%08x | ICE_STM2_COUNTER2: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM2_COUNTERS1),
		ice_readl(ice_mmio, ICE_REGS_STREAM2_COUNTERS2));

	pr_err("%s: ICE_STM2_COUNTER3: 0x%08x | ICE_STM2_COUNTER4: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM2_COUNTERS3),
		ice_readl(ice_mmio, ICE_REGS_STREAM2_COUNTERS4));

	pr_err("%s: ICE_STM1_CTR5_MSB: 0x%08x | ICE_STM1_CTR5_LSB: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM1_COUNTERS5_MSB),
		ice_readl(ice_mmio, ICE_REGS_STREAM1_COUNTERS5_LSB));

	pr_err("%s: ICE_STM1_CTR6_MSB: 0x%08x | ICE_STM1_CTR6_LSB: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM1_COUNTERS6_MSB),
		ice_readl(ice_mmio, ICE_REGS_STREAM1_COUNTERS6_LSB));

	pr_err("%s: ICE_STM1_CTR7_MSB: 0x%08x | ICE_STM1_CTR7_LSB: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM1_COUNTERS7_MSB),
		ice_readl(ice_mmio, ICE_REGS_STREAM1_COUNTERS7_LSB));

	pr_err("%s: ICE_STM1_CTR8_MSB: 0x%08x | ICE_STM1_CTR8_LSB: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM1_COUNTERS8_MSB),
		ice_readl(ice_mmio, ICE_REGS_STREAM1_COUNTERS8_LSB));

	pr_err("%s: ICE_STM1_CTR9_MSB: 0x%08x | ICE_STM1_CTR9_LSB: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM1_COUNTERS9_MSB),
		ice_readl(ice_mmio, ICE_REGS_STREAM1_COUNTERS9_LSB));

	pr_err("%s: ICE_STM2_CTR5_MSB: 0x%08x | ICE_STM2_CTR5_LSB: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM2_COUNTERS5_MSB),
		ice_readl(ice_mmio, ICE_REGS_STREAM2_COUNTERS5_LSB));

	pr_err("%s: ICE_STM2_CTR6_MSB: 0x%08x | ICE_STM2_CTR6_LSB: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM2_COUNTERS6_MSB),
		ice_readl(ice_mmio, ICE_REGS_STREAM2_COUNTERS6_LSB));

	pr_err("%s: ICE_STM2_CTR7_MSB: 0x%08x | ICE_STM2_CTR7_LSB: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM2_COUNTERS7_MSB),
		ice_readl(ice_mmio, ICE_REGS_STREAM2_COUNTERS7_LSB));

	pr_err("%s: ICE_STM2_CTR8_MSB: 0x%08x | ICE_STM2_CTR8_LSB: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM2_COUNTERS8_MSB),
		ice_readl(ice_mmio, ICE_REGS_STREAM2_COUNTERS8_LSB));

	pr_err("%s: ICE_STM2_CTR9_MSB: 0x%08x | ICE_STM2_CTR9_LSB: 0x%08x\n",
		ice_readl(ice_mmio, ICE_REGS_STREAM2_COUNTERS9_MSB),
		ice_readl(ice_mmio, ICE_REGS_STREAM2_COUNTERS9_LSB));

	ice_dump_test_bus(ice_mmio);

	return 0;
}
EXPORT_SYMBOL(crypto_qti_debug);

int crypto_qti_keyslot_program(const struct ice_mmio_data *mmio_data,
			       const struct blk_crypto_key *key,
			       unsigned int slot,
			       u8 data_unit_mask, int capid)
{
	int err = 0;

	err = crypto_qti_program_key(mmio_data, key, slot,
				data_unit_mask, capid);
	if (err) {
		pr_err("%s: program key failed with error %d\n", __func__, err);
		err = crypto_qti_invalidate_key(mmio_data, slot);
		if (err) {
			pr_err("%s: invalidate key failed with error %d\n", __func__, err);
			return err;
		}
	}

	return err;
}
EXPORT_SYMBOL(crypto_qti_keyslot_program);

int crypto_qti_keyslot_evict(const struct ice_mmio_data *mmio_data,
							 unsigned int slot)
{
	int err = 0;

	err = crypto_qti_invalidate_key(mmio_data, slot);
	if (err) {
		pr_err("%s: invalidate key failed with error %d\n",
			__func__, err);
	}

	return err;
}
EXPORT_SYMBOL(crypto_qti_keyslot_evict);

int crypto_qti_derive_raw_secret(const u8 *wrapped_key,
				 unsigned int wrapped_key_size, u8 *secret,
				 unsigned int secret_size)
{
	int err = 0;

	if (wrapped_key_size <= RAW_SECRET_SIZE) {
		pr_err("%s: Invalid wrapped_key_size: %u\n",
				__func__, wrapped_key_size);
		err = -EINVAL;
		return err;
	}
	if (secret_size != RAW_SECRET_SIZE) {
		pr_err("%s: Invalid secret size: %u\n", __func__, secret_size);
		err = -EINVAL;
		return err;
	}

	if (wrapped_key_size > 64)
		err = crypto_qti_derive_raw_secret_platform(wrapped_key,
				wrapped_key_size, secret, secret_size);
	else
		memcpy(secret, wrapped_key, secret_size);

	return err;
}
EXPORT_SYMBOL(crypto_qti_derive_raw_secret);

#if IS_ENABLED(CONFIG_QTI_CRYPTO_FDE)
static int get_key_id(char *key_id, uint32_t usage, bool hash_enable)
{
	/* only legacy hash will get the legacy key id, otherwise return new key id */
	if (hash_enable) {
		switch (usage) {
		case QSEECOM_KM_USAGE_UFS_ICE_DISK_ENCRYPTION:
			memcpy((void *)key_id,	(void *)CRYPTO_ICE_FDE_UFS_LEGACY_KEYID,
						strlen(CRYPTO_ICE_FDE_UFS_LEGACY_KEYID));
			break;
		case QSEECOM_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION:
			memcpy((void *)key_id,	(void *)CRYPTO_ICE_FDE_EMMC_LEGACY_KEYID,
						strlen(CRYPTO_ICE_FDE_EMMC_LEGACY_KEYID));
			break;
		default:
			pr_err("unsupported usage %d\n", usage);
			return -EINVAL;
		}
	} else {
		switch (usage) {
		case QSEECOM_KM_USAGE_UFS_ICE_DISK_ENCRYPTION:
			memcpy((void *)key_id,	(void *)CRYPTO_ICE_FDE_UFS_KEYID,
						strlen(CRYPTO_ICE_FDE_UFS_KEYID));
			break;
		case QSEECOM_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION:
			memcpy((void *)key_id,	(void *)CRYPTO_ICE_FDE_EMMC_KEYID,
						strlen(CRYPTO_ICE_FDE_EMMC_KEYID));
			break;
		default:
			pr_err("unsupported usage %d\n", usage);
			return -EINVAL;
		}
	}

	return 0;
}

static struct ice_part_cfg *crypto_qti_ice_get_part_cfg(struct ice_device *ice_dev,
							char const * const volname)
{
	struct ice_part_cfg *part_cfg = NULL;

	if (!ice_dev) {
		pr_err_ratelimited("%s: %s: no ICE device\n", __func__, volname);
		return NULL;
	}

	list_for_each_entry(part_cfg, &ice_dev->part_cfg_list, list) {
		if (!strcmp(part_cfg->volname, volname))
			return part_cfg;
	}

	return NULL;
}

/*
 * Stub function to satisfy kobject lifecycle
 */
static inline void crypto_qti_ice_release_kobj(struct kobject *kobj)
{
	char const *name = kobject_name(kobj);

	if (name)
		pr_debug("Releasing %s\n", name);

	/* Nothing to do */
}

static inline struct ice_part_cfg *to_ice_part_cfg(struct kobject *kobject)
{
	return container_of(kobject, struct ice_part_cfg, kobj);
}

/*
 * Attribute "show" method for per-partition attributes.
 */
static ssize_t crypto_qti_ice_part_cfg_show(struct kobject *kobj,
					    struct kobj_attribute *attr,
					    char *buf)
{
	struct ice_part_cfg *cfg = to_ice_part_cfg(kobj);

	if (!strcmp(attr->attr.name, "decr_bypass"))
		return scnprintf(buf, PAGE_SIZE, "%d\n", cfg->decr_bypass);
	if (!strcmp(attr->attr.name, "encr_bypass"))
		return scnprintf(buf, PAGE_SIZE, "%d\n", cfg->encr_bypass);
	if (!strcmp(attr->attr.name, "add_partition_result"))
		return scnprintf(buf, PAGE_SIZE, "%d\n", cfg->add_partition_result);

	pr_err("%s: Unhandled attribute %s\n", __func__, buf);
	return -EFAULT;
}

/*
 * Attribute "store" method for per-partition attributes.
 */
static ssize_t crypto_qti_ice_part_cfg_store(struct kobject *kobj,
					     struct kobj_attribute *attr,
					     const char *buf,
					     size_t count)
{
	struct ice_part_cfg *cfg = to_ice_part_cfg(kobj);
	int op_result = -EFAULT;

	/*
	 * the decr_bypass/encr_bypass attribute can be configured
	 * from userspace side, only "0" or "1" are accepted.
	 */
	if (count > 2)
		return -EINVAL;

	if (!strcmp(attr->attr.name, "decr_bypass"))
		op_result = kstrtobool(buf, &cfg->decr_bypass);
	else if (!strcmp(attr->attr.name, "encr_bypass"))
		op_result = kstrtobool(buf, &cfg->encr_bypass);
	else
		pr_err("%s: Unhandled attribute %s\n", __func__, buf);

	/* Notify userspace of the change */
	if (!op_result) {
		pr_info("%s: Change %s:%s=%s\n", __func__, cfg->volname,
			attr->attr.name, buf);
		kobject_uevent(kobj, KOBJ_CHANGE);
		return count;
	}

	pr_err("%s: Failed %s:%s=%s : %d\n", __func__, cfg->volname,
			attr->attr.name, buf, op_result);
	return op_result;
}

/*
 * crypto_qti_ice_part_cfg_show and crypto_qti_ice_part_cfg_store must ahead of referring to them
 */
static PART_CFG_ATTR_RW(add_partition_result);
static PART_CFG_ATTR_RW(decr_bypass);
static PART_CFG_ATTR_RW(encr_bypass);

static struct attribute *ice_part_cfg_attrs[] = {
	&part_cfg_attr_add_partition_result.attr,
	&part_cfg_attr_decr_bypass.attr,
	&part_cfg_attr_encr_bypass.attr,
	NULL,
};

/**
 * Add a new partition for the ICE to manage
 *
 * Full encryption/decryption is enabled by default!
 *
 * @ice_dev:        ICE device node
 * @new_volname:    Null-terminated volume name partition
 * @slot:           the ICE keyslot number
 * @new_key         0 is key exist, 1 is new key
 */
static int crypto_qti_ice_add_new_partition(struct ice_device *ice_dev,
					    const char * const new_volname,
					    unsigned int slot,
					    bool new_key)
{
	struct list_head *new_pos = NULL;
	struct ice_part_cfg *elem = NULL;
	int rc = -EINVAL;
	char *envp[2] = {0};
	char part_name[CRYPTO_ICE_UDEV_PARTITION_NAME] = {0};

	/* Check if the partition is already in the list */
	list_for_each(new_pos, &ice_dev->part_cfg_list) {
		elem = list_entry(new_pos, struct ice_part_cfg, list);
		if (!strcmp(new_volname, elem->volname)) {
			rc = 0;
			goto out; /* Already in list, bail */
		}
	}

	dev_info(ice_dev->pdev, "Adding %s to ICE partition list\n", new_volname);

	/* Didn't find it, add new entry at the end */
	elem = kzalloc(sizeof(struct ice_part_cfg), GFP_KERNEL);
	if (!elem) {
		rc = -ENOMEM;
		dev_err(ice_dev->pdev,
			"%s: Error %d allocating memory for partition %s\n",
			__func__, rc, new_volname);
		goto out;
	}

	/* Add the new partition to the KSet */
	elem->kobj.kset = ice_dev->fde_partitions;
	/* Set up the sysfs node */
	elem->kobj_type.release = crypto_qti_ice_release_kobj;
	elem->kobj_type.sysfs_ops = &kobj_sysfs_ops;
	elem->kobj_type.default_attrs = ice_part_cfg_attrs;
	rc = kobject_init_and_add(&elem->kobj, &elem->kobj_type,
		NULL, new_volname);
	if (rc) {
		dev_err(ice_dev->pdev, "%s: Error %d adding sysfs for %s\n",
			__func__, rc, new_volname);
		kobject_put(&elem->kobj);
		kfree(elem);
		goto out;
	}

	/* Null terminated */
	strscpy(elem->volname, new_volname, PARTITION_META_INFO_VOLNAMELTH + 1);

	/* Default - encryption is enabled */
	elem->decr_bypass = false;
	elem->encr_bypass = false;

	/* Start using key slots from the end */
	if (ice_dev->num_fde_slots_in_use >= TOTAL_NUMBER_ICE_SLOTS) {
		/* Shouldn't be here , just some defensive check */
		rc = -ENOMEM;
		dev_err(ice_dev->pdev, "%s: Allocating more slots than available %s\n", __func__);
		kobject_put(&elem->kobj);
		kfree(elem);
		goto out;
	}

	elem->key_slot = slot;
	elem->add_partition_result = new_key;
	list_add_tail(&elem->list, new_pos);
	/* Generate the UEvent with the partition name to notify userpsace */
	snprintf(part_name, CRYPTO_ICE_UDEV_PARTITION_NAME, "FDE_PARTITION=%s", new_volname);
	envp[0] = part_name;
	envp[1] = NULL;
	kobject_uevent_env(&elem->kobj, KOBJ_ADD, envp);
out:
	return rc;
}

/*
 * Attribute "store" method for add_partition
 *
 * Add a partition to the list with default configuration.
 */
static ssize_t add_partition_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf,
				   size_t count)
{
	struct ice_device *ice_dev = dev_get_drvdata(dev);
	ssize_t ret = count;
	char label[PARTITION_META_INFO_VOLNAMELTH + 1] = {0};
	char key_id[CRYPTO_ICE_KEY_ID_SIZE] = {0};
	char *flag = "-hash";
	char *hash_index = NULL;
	bool hash_enable = false;
	size_t partition_count = count;
	int rc = 0;
	struct list_head *new_pos = NULL;
	struct ice_part_cfg *elem = NULL;
	unsigned int slot = 0;
	bool new_key_generated = false;
	int key_res = 0;

	/* The count of the buf include hash size and flag size */
	if (count > PARTITION_META_INFO_VOLNAMELTH + CRYPTO_ICE_HASH_SIZE + strlen(flag)) {
		dev_err(dev, "Invalid partition '%s' (%u)\n", buf, count);
		return -EINVAL;
	}

	if (!buf) {
		dev_err(dev, "Invalid buf\n");
		return -EINVAL;
	}

	if (!ice_dev) {
		dev_err(dev, "Invalid ICE device!\n");
		return -ENODEV;
	}

	/*
	 * check if the userspace input the hash to generate ICE key
	 * -hash means input hash with legacy key id
	 */
	hash_index = strnstr(buf, flag, count);
	if (hash_index) {
		/*
		 * It must follow the right format to input partition name and hash,
		 * partition name can't be null.
		 */
		if (hash_index > buf + 1) {
			hash_enable = true;
			/* skip the blank and get the partition name count */
			partition_count = hash_index - buf - 1;
			/* skip to the header pointer of hash */
			hash_index += strlen(flag) + 1;
			/* exclude "\r" that in the end of input buf, and check hash size */
			if (strlen(hash_index) - 1 != CRYPTO_ICE_HASH_SIZE) {
				dev_err(dev, "Invalid hash\n");
				return -EINVAL;
			}
		} else {
			dev_err(dev, "Invalid partition\n");
			return -EINVAL;
		}
	}

	if (!hash_enable) {
		/* Copy into a temporary buffer, stripping out newlines */
		partition_count = strcspn(buf, "\n\r");
		if (!partition_count) {
			dev_err(dev, "Invalid partition '%s' (%u)\n", buf, partition_count);
			return -EINVAL;
		}
	}

	/* copy the partition name */
	memcpy(label, buf, partition_count);
	label[partition_count] = '\0';

	mutex_lock(&ice_dev->mutex);
	/* Check if the partition is already in the list */
	list_for_each(new_pos, &ice_dev->part_cfg_list)
	{
		elem = list_entry(new_pos, struct ice_part_cfg, list);
		if (!strcmp(label, elem->volname)) {
			dev_info(dev, "partition exist already, needn't to add\n");
			ret = -EINVAL; /* Already in the list , return */
			goto out;
		}
	}

	/* New partition , check if we have a free slot available */
	if (ice_dev->num_fde_slots_in_use >= ice_dev->num_fde_slots) {
		dev_err(dev, "All ICE slots are in use!\n");
		ret = -EINVAL; /* Already in the list , return */
		goto out;
	}

#if IS_ENABLED(CONFIG_QTI_CRYPTO_LEGACY_KEY_FDE)
	/* Don't increment , using same slot for all partitions */
	slot = CRYPTO_ICE_FDE_KEY_INDEX;
#else
	/* Increment number of slots in use */
	++ice_dev->num_fde_slots_in_use;
	slot = TOTAL_NUMBER_ICE_SLOTS - ice_dev->num_fde_slots_in_use;
#endif

	/*
	 * First need to generate/restore key and set it into the ice slot
	 * Request is accroding to storage type UFS/eMMC
	 * The unique key is generated via partition name , or legacy key if configured
	 */
	if (strcmp(ice_dev->ice_instance_type, "ufs") == 0) {
#if IS_ENABLED(CONFIG_QTI_CRYPTO_LEGACY_KEY_FDE)
		/* In legacy mode, only one common key is needed */
		if (list_empty(&ice_dev->part_cfg_list)) {
			/* get the key_id that depends on if inhash_enable or not */
			if (get_key_id(key_id, QSEECOM_KM_USAGE_UFS_ICE_DISK_ENCRYPTION,
					hash_enable)) {
				dev_err(dev, "Fail to get the ice key_id\n");
				ret = -EINVAL;
				goto out;
			}

			key_res = qseecom_create_key_in_slot(
				QSEECOM_KM_USAGE_UFS_ICE_DISK_ENCRYPTION,
				slot, key_id, hash_index);
		} else {
			/* Key is already generated and set,contune */
			key_res = QSEECOM_KEY_ID_EXISTS;
		}
#else
		key_res = qseecom_create_key_in_slot(
			QSEECOM_KM_USAGE_UFS_ICE_DISK_ENCRYPTION,
			slot, label, NULL);
#endif
	} else if (strcmp(ice_dev->ice_instance_type, "sdcc") == 0) {
#if IS_ENABLED(CONFIG_QTI_CRYPTO_LEGACY_KEY_FDE)
		/* In legacy mode, only one common key is needed */
		if (list_empty(&ice_dev->part_cfg_list)) {
			/* get the key_id that depends on if inhash_enable or not */
			if (get_key_id(key_id, QSEECOM_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION,
					hash_enable)) {
				dev_err(dev, "Fail to get the ice key_id\n");
				ret = -EINVAL;
				goto out;
			}

			key_res = qseecom_create_key_in_slot(
				QSEECOM_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION,
				slot, key_id, hash_index);
		} else {
			/* Key is already generated and set,contune */
			key_res = QSEECOM_KEY_ID_EXISTS;
		}
#else
		key_res = qseecom_create_key_in_slot(
			QSEECOM_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION,
			slot, label, NULL);
#endif
	} else {
		dev_err(dev, "Not supported storage type!\n");
		ret = -EINVAL; /* Already in the list , return */
		goto out;
	}

	/* Check the qseecom_create_key_in_slot result */
	if (key_res) {
		if (key_res == QSEECOM_KEY_ID_EXISTS) {
			dev_info(dev, "ICE key exist\n");
			new_key_generated = true;
		} else {
			dev_err(dev, "Failed to generate and set key!\n");
#ifndef CONFIG_QTI_CRYPTO_LEGACY_KEY_FDE
			/* Take the slot back */
			--ice_dev->num_fde_slots_in_use;
#endif
			ret = -EINVAL;
			goto out;
		}
	}

	/* Error logged in function */
	rc = crypto_qti_ice_add_new_partition(ice_dev, label, slot, new_key_generated);
	if (rc)
		ret = rc;

out:
	mutex_unlock(&ice_dev->mutex);
	return ret;
}

/* add_partition_store must ahead of referring it */
static struct device_attribute dev_attr_add_partition =  __ATTR_WR_PARTITIONS(add_partition);

static struct attribute *ice_attrs[] = {
	&dev_attr_add_partition.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ice);

unsigned int crypto_qti_ice_get_num_fde_slots(void)
{
	struct ice_device *ice_dev = NULL;

	ice_dev = list_first_entry_or_null(&ice_devices, struct ice_device, list);
	if (ice_dev)
		return ice_dev->num_fde_slots;
	else
		return 0;
}
EXPORT_SYMBOL(crypto_qti_ice_get_num_fde_slots);

static int crypto_qti_ice_get_vreg(struct ice_device *ice_dev)
{
	int ret = 0;

	if (!ice_dev->is_regulator_available)
		return ret;

	if (ice_dev->reg)
		return ret;

	ice_dev->reg = devm_regulator_get(ice_dev->pdev, "vdd-hba");
	if (IS_ERR(ice_dev->reg)) {
		ret = PTR_ERR(ice_dev->reg);
		dev_err(ice_dev->pdev, "%s: %s get failed, err=%d\n", __func__,
			"vdd-hba-supply", ret);
	}
	return ret;
}

/* configure setting to partition */
static int crypto_qti_ice_setting_config(struct request *req,
					 struct ice_crypto_setting *crypto_data,
					 struct ice_data_setting *setting,
					 bool encr_bypass,
					 bool decr_bypass)
{
	if (!setting)
		return -EINVAL;

	if ((short)(crypto_data->key_index) >= 0) {
		memcpy(&setting->crypto_data, crypto_data,
				sizeof(setting->crypto_data));

		if (rq_data_dir(req) == WRITE) {
			setting->encr_bypass = encr_bypass;
		} else if (rq_data_dir(req) == READ) {
			setting->decr_bypass = decr_bypass;
		} else {
			/* Should I say BUG_ON */
			pr_err("%s unhandled request 0x%x\n", __func__, req->cmd_flags);
			setting->encr_bypass = true;
			setting->decr_bypass = true;
		}
	}

	return 0;
}

static void crypto_qti_ice_disable_intr(struct ice_device *ice_dev)
{
	unsigned int reg;

	reg = crypto_qti_ice_readl(ice_dev, ICE_REGS_NON_SEC_IRQ_MASK);
	reg |= ICE_NON_SEC_IRQ_MASK;
	crypto_qti_ice_writel(ice_dev, reg, ICE_REGS_NON_SEC_IRQ_MASK);
	/*
	 * Ensure previous instructions was completed before issuing next
	 * ICE initialization/optimization instruction
	 */
	mb();
}

static int crypto_qti_ice_parse_ice_instance_type(struct platform_device *pdev,
						  struct ice_device *ice_dev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	const char *type;

	ret = of_property_read_string_index(np, "qcom,instance-type", 0, &type);
	if (ret) {
		pr_err("%s: Could not get ICE instance type\n", __func__);
		return ret;
	}
	strscpy(ice_dev->ice_instance_type, type, CRYPTO_ICE_TYPE_NAME_LEN);
	return 0;
}

static void crypto_qti_ice_parse_number_of_fde_slots(struct platform_device *pdev,
						     struct ice_device *ice_dev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	u32 num_of_slots = 0;

	ret =  of_property_read_u32(np, "qcom,num-fde-slots", &num_of_slots);
	if (ret) {
		/* Backwards compatibility , assume one slot if DTS property is not deifined */
		pr_info("%s: No num of  FDE slots defined in dts,using 1 slot\n", __func__);
		ice_dev->num_fde_slots = 1;
		return;
	}
	if (num_of_slots >= TOTAL_NUMBER_ICE_SLOTS) {
		/* Limit the number of slots */
		pr_info("%s: Limiting num of FDE slots\n", __func__);
		ice_dev->num_fde_slots = TOTAL_NUMBER_ICE_SLOTS - 1;
	} else {
		ice_dev->num_fde_slots = num_of_slots;
	}
}

static int crypto_qti_ice_parse_clock_info(struct platform_device *pdev,
					   struct ice_device *ice_dev)
{
	int cnt, i, len;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	char *name;
	struct ice_clk_info *clki;
	u32 *clkfreq = NULL;
	int ret = -EINVAL;

	if (!np)
		goto out;

	cnt = of_property_count_strings(np, "clock-names");
	if (cnt <= 0) {
		dev_info(dev, "%s: Unable to find clocks, assuming enabled\n", __func__);
		ret = cnt;
		goto out;
	}

	if (!of_get_property(np, "qcom,op-freq-hz", &len)) {
		dev_info(dev, "qcom,op-freq-hz property not specified\n");
		goto out;
	}

	len = len/sizeof(*clkfreq);
	if (len != cnt)
		goto out;

	clkfreq = devm_kzalloc(dev, len * sizeof(*clkfreq), GFP_KERNEL);
	if (!clkfreq) {
		ret = -ENOMEM;
		goto out;
	}
	ret = of_property_read_u32_array(np, "qcom,op-freq-hz", clkfreq, len);

	INIT_LIST_HEAD(&ice_dev->clk_list_head);

	for (i = 0; i < cnt; i++) {
		ret = of_property_read_string_index(np, "clock-names", i, (const char **)&name);
		if (ret)
			goto out;

		clki = devm_kzalloc(dev, sizeof(*clki), GFP_KERNEL);
		if (!clki) {
			ret = -ENOMEM;
			goto out;
		}
		clki->max_freq = clkfreq[i];
		clki->name = kstrdup(name, GFP_KERNEL);
		list_add_tail(&clki->list, &ice_dev->clk_list_head);
	}
out:
	return ret;
}

static int crypto_qti_ice_get_dts_data(struct platform_device *pdev,
				       struct ice_device *ice_dev)
{
	int ret = -EINVAL;

	ice_dev->mmio = NULL;
	if (!of_parse_phandle(pdev->dev.of_node, "vdd-hba-supply", 0)) {
		pr_err("%s: No vdd-hba-supply regulator, assuming not needed\n", __func__);
		ice_dev->is_regulator_available = false;
	} else {
		ice_dev->is_regulator_available = true;
	}
	ice_dev->is_ice_clk_available = of_property_read_bool((&pdev->dev)->of_node,
								"qcom,enable-ice-clk");

	if (ice_dev->is_ice_clk_available) {
		ret = crypto_qti_ice_parse_clock_info(pdev, ice_dev);
		if (ret) {
			pr_err("%s: crypto_qti_ice_parse_clock_info failed (%d)\n", __func__, ret);
			return ret;
		}
	}

	ret = crypto_qti_ice_parse_ice_instance_type(pdev, ice_dev);
	if (ret) {
		pr_err("%s: fail to parse ice instance type (%d)\n", __func__, ret);
		return ret;
	}

	crypto_qti_ice_parse_number_of_fde_slots(pdev, ice_dev);

	return ret;
}

/*
 * ICE HW instance can exist in UFS or eMMC based storage HW
 * Userspace does not know what kind of ICE it is dealing with.
 * Though userspace can find which storage device it is booting
 * from but all kind of storage types dont support ICE from
 * beginning. So ICE device is created for user space to ping
 * if ICE exist for that kind of storage
 */
static const struct file_operations crypto_qti_ice_fops = {
	.owner = THIS_MODULE,
};

/**
 * Remove all sysfs resources associated with the driver
 *
 * @ice_dev:	ICE device node
 */
static void crypto_qti_ice_cleanup_sysfs(struct ice_device *ice_dev)
{
	struct list_head *pos = NULL;
	struct list_head *n = NULL;
	struct ice_part_cfg *elem = NULL;

	/* Platform device attributes */
	if (ice_dev->sysfs_groups_created) {
		sysfs_remove_groups(&ice_dev->pdev->kobj, ice_groups);
		ice_dev->sysfs_groups_created = false;
	}

	/* Partition configuration list and sysfs nodes */
	list_for_each_safe(pos, n, &ice_dev->part_cfg_list) {
		elem = list_entry(pos, struct ice_part_cfg, list);
		dev_dbg(ice_dev->pdev, "Removing %s", elem->volname);

		if (elem->kobj.state_in_sysfs)
			kobject_put(&elem->kobj);

		list_del(pos);
		kfree(elem);
	}
	if (ice_dev->fde_partitions != NULL)
		kset_unregister(ice_dev->fde_partitions);
}

static int crypto_qti_ice_probe(struct platform_device *pdev)
{
	struct ice_device *ice_dev;
	int rc = -EINVAL;

	if (!pdev) {
		pr_err("%s: Invalid platform_device passed\n", __func__);
		goto out;
	}

	ice_dev = kzalloc(sizeof(struct ice_device), GFP_KERNEL);

	if (!ice_dev) {
		rc = -ENOMEM;
		pr_err("%s: Error %d allocating memory for ICE device:\n", __func__, rc);
		goto out;
	}

	/* Initialize device data to a known state */
	INIT_LIST_HEAD(&ice_dev->part_cfg_list);
	ice_dev->pdev = &pdev->dev;
	if (!ice_dev->pdev) {
		rc = -EINVAL;
		pr_err("%s: Invalid device passed in platform_device\n", __func__);
		goto err_ice_dev;
	}

	if (pdev->dev.of_node)
		rc = crypto_qti_ice_get_dts_data(pdev, ice_dev);
	else {
		rc = -EINVAL;
		pr_err("%s: ICE device node not found\n", __func__);
	}

	if (rc)
		goto err_ice_dev;

	/*
	 * If ICE is enabled here, it would be waste of power.
	 * We would enable ICE when first request for crypto
	 * operation arrives.
	 */
	rc = crypto_qti_ice_init(ice_dev, NULL, NULL);
	if (rc) {
		pr_err("ice_init failed.\n");
		goto err_ice_dev;
	}
	mutex_init(&ice_dev->mutex);
	ice_dev->is_ice_enabled = true;
	platform_set_drvdata(pdev, ice_dev);
	list_add_tail(&ice_dev->list, &ice_devices);

	dev_info(&pdev->dev, "Initialized OK\n");

	goto out;

err_ice_dev:
	crypto_qti_ice_cleanup_sysfs(ice_dev);

	kfree(ice_dev);
out:
	return rc;
}

static int crypto_qti_ice_remove(struct platform_device *pdev)
{
	struct ice_device *ice_dev;

	ice_dev = (struct ice_device *)platform_get_drvdata(pdev);

	if (!ice_dev)
		return 0;

	mutex_destroy(&ice_dev->mutex);
	crypto_qti_ice_cleanup_sysfs(ice_dev);

	crypto_qti_ice_disable_intr(ice_dev);

	device_init_wakeup(&pdev->dev, false);
	if (ice_dev->mmio)
		iounmap(ice_dev->mmio);

	list_del_init(&ice_dev->list);
	kfree(ice_dev);

	return -EINVAL;
}

int crypto_qti_ice_config_start(struct request *req, struct ice_data_setting *setting)
{
	struct ice_crypto_setting ice_data = {0};
	struct ice_device *ice_dev;
	struct ice_part_cfg *part_cfg;
	int ret = -EINVAL;

	if (!req)
		goto out;

	/*
	 * It is not an error to have a request with no  bio
	 * Such requests must bypass ICE. So first set bypass and then
	 * return if bio is not available in request
	 */
	if (setting) {
		setting->encr_bypass = true;
		setting->decr_bypass = true;
	}

	ice_dev = list_first_entry_or_null(&ice_devices, struct ice_device, list);
	if (req->part && req->part->bd_meta_info && req->part->bd_meta_info->volname[0]) {
		part_cfg = crypto_qti_ice_get_part_cfg(ice_dev,
			req->part->bd_meta_info->volname);
		if (part_cfg) {
			ice_data.key_index = part_cfg->key_slot;
			/* the partition info has remove __sector/nr_sects/start_sect, so don't
			 * try to check the sector. TBD
			 */
			ret = crypto_qti_ice_setting_config(req,
						&ice_data, setting,
						part_cfg->encr_bypass,
						part_cfg->decr_bypass);
		}
	}

out:
	return ret;
}
EXPORT_SYMBOL(crypto_qti_ice_config_start);

static int crypto_qti_ice_enable_clocks(struct ice_device *ice, bool enable)
{
	int ret = 0;
	struct ice_clk_info *clki = NULL;
	struct device *dev = ice->pdev;
	struct list_head *head = &ice->clk_list_head;

	if (!head || list_empty(head)) {
		dev_err(dev, "%s:ICE Clock list null/empty\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	if (!ice->is_ice_clk_available) {
		dev_err(dev, "%s:ICE Clock not available\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	list_for_each_entry(clki, head, list) {
		if (!clki->name)
			continue;

		if (enable)
			ret = clk_prepare_enable(clki->clk);
		else
			clk_disable_unprepare(clki->clk);

		if (ret) {
			dev_err(dev, "Unable to %s ICE core clk\n", enable?"enable":"disable");
			goto out;
		}
	}
out:
	return ret;
}

static struct ice_device *crypto_qti_get_ice_device_from_storage_type(const char *storage_type)
{
	struct ice_device *ice_dev = NULL;

	if (list_empty(&ice_devices)) {
		ice_dev = ERR_PTR(-EPROBE_DEFER);
		goto out;
	}

	list_for_each_entry(ice_dev, &ice_devices, list) {
		if (!strcmp(ice_dev->ice_instance_type, storage_type)) {
			pr_debug("%s: ice device %pK\n", __func__, ice_dev);
			return ice_dev;
		}
	}
out:
	return NULL;
}

static int crypto_qti_ice_enable_setup(struct ice_device *ice_dev)
{
	int ret = -EINVAL;

	/* Setup Regulator */
	if (ice_dev->is_regulator_available) {
		if (crypto_qti_ice_get_vreg(ice_dev)) {
			pr_err("%s: Could not get regulator\n", __func__);
			goto out;
		}
		ret = regulator_enable(ice_dev->reg);
		if (ret) {
			pr_err("%s:%pK: Could not enable regulator\n", __func__, ice_dev);
			goto out;
		}
	} else {
		pr_info("%s: No need to get regulator\n", __func__);
		ret = 0;
	}

	/* Setup Clocks */
	if (crypto_qti_ice_enable_clocks(ice_dev, true)) {
		pr_err("%s:%pK:%s Could not enable clocks\n", __func__,
				ice_dev, ice_dev->ice_instance_type);
		if (ice_dev->is_regulator_available) {
			if (crypto_qti_ice_get_vreg(ice_dev)) {
				pr_err("%s: Could not get regulator\n", __func__);
				goto out;
			}
			ret = regulator_disable(ice_dev->reg);
			if (ret)
				pr_err("%s:%pK: Could not disable regulator\n", __func__, ice_dev);
		}
	}

out:
	return ret;
}

static int crypto_qti_ice_disable_setup(struct ice_device *ice_dev)
{
	int ret;

	/* Setup Clocks */
	ret = crypto_qti_ice_enable_clocks(ice_dev, false);
	if (ret) {
		pr_err("%s:%pK:%s Could not disable clocks\n", __func__,
				ice_dev, ice_dev->ice_instance_type);
		goto out;
	}

	/* Setup Regulator */
	if (ice_dev->is_regulator_available) {
		ret = crypto_qti_ice_get_vreg(ice_dev);
		if (ret) {
			pr_err("%s: Could not get regulator\n", __func__);
			goto out;
		}
		ret = regulator_disable(ice_dev->reg);
		if (ret) {
			pr_err("%s:%pK: Could not disable regulator\n", __func__, ice_dev);
			goto out;
		}
	}
out:
	return ret;
}

static int crypto_qti_ice_init_clocks(struct ice_device *ice)
{
	int ret = -EINVAL;
	struct ice_clk_info *clki = NULL;
	struct device *dev = ice->pdev;
	struct list_head *head = &ice->clk_list_head;

	if (!head || list_empty(head)) {
		dev_err(dev, "%s:ICE Clock list null/empty\n", __func__);
		goto out;
	}

	list_for_each_entry(clki, head, list) {
		if (!clki->name)
			continue;

		clki->clk = devm_clk_get(dev, clki->name);
		if (IS_ERR(clki->clk)) {
			ret = PTR_ERR(clki->clk);
			dev_err(dev, "%s: %s clk get failed, %d\n",
					__func__, clki->name, ret);
			goto out;
		}

		/* Not all clocks would have a rate to be set */
		ret = 0;
		if (clki->max_freq) {
			ret = clk_set_rate(clki->clk, clki->max_freq);
			if (ret) {
				dev_err(dev,
				"%s: %s clk set rate(%dHz) failed, %d\n",
						__func__, clki->name,
				clki->max_freq, ret);
				goto out;
			}
			clki->curr_freq = clki->max_freq;
			dev_dbg(dev, "%s: clk: %s, rate: %lu\n", __func__,
				clki->name, clk_get_rate(clki->clk));
		}
	}
out:
	return ret;
}

static int crypto_qti_ice_finish_init(struct ice_device *ice_dev)
{
	int err = 0;

	if (!ice_dev) {
		pr_err("%s: Null data received\n", __func__);
		err = -ENODEV;
		goto out;
	}

	if (ice_dev->is_ice_clk_available) {
		err = crypto_qti_ice_init_clocks(ice_dev);
		if (err)
			goto out;
	}
out:
	return err;
}

static int crypto_qti_ice_init(struct ice_device *ice_dev,
			       void *host_controller_data,
			       ice_error_cb error_cb)
{
	/*
	 * A completion event for host controller would be triggered upon
	 * initialization completion
	 * When ICE is initialized, it would put ICE into Global Bypass mode
	 * When any request for data transfer is received, it would enable
	 * the ICE for that particular request
	 */
	ice_dev->error_cb = error_cb;
	ice_dev->host_controller_data = host_controller_data;

	return crypto_qti_ice_finish_init(ice_dev);
}

int crypto_qti_ice_init_fde_node(struct device *dev)
{

	struct ice_device *ice_dev = NULL;
	int rc = -EINVAL;

	if (!dev) {
		pr_err("%s: Invalid platform_device passed\n", __func__);
		rc = -EINVAL;
		goto out;
	}

	/* Only one ice device is supported , remove the extra one(DTS has more than one node) */

	if (!strcmp(CRYPTO_ICE_UFS_DEV_NAME, dev_driver_string(dev))) {
		/* ufs found , remove eMMC node if exists */
		ice_dev = crypto_qti_get_ice_device_from_storage_type("sdcc");
		if (ice_dev) {
			mutex_destroy(&ice_dev->mutex);

			if (ice_dev->mmio)
				iounmap(ice_dev->mmio);

			list_del(&ice_dev->list);
			kfree(ice_dev);
		}
		/* Get UFS instance */
		ice_dev = crypto_qti_get_ice_device_from_storage_type("ufs");
		if (ice_dev == NULL) {
			pr_err("ice_dev NULL !\n");
			rc = -EINVAL;
			goto out;
		}
	}

	if (!strcmp(CRYPTO_ICE_EMMC_DEV_NAME, dev_driver_string(dev))) {
		ice_dev = crypto_qti_get_ice_device_from_storage_type("ufs");
		if (ice_dev) {
			/* eMMC found , remove UFS node if exists */
			mutex_destroy(&ice_dev->mutex);
			if (ice_dev->mmio)
				iounmap(ice_dev->mmio);

			list_del(&ice_dev->list);
			kfree(ice_dev);
		}

		/* Get eMMC instance */
		ice_dev = crypto_qti_get_ice_device_from_storage_type("sdcc");
		if (ice_dev == NULL) {
			pr_err("ice_dev NULL !\n");
			rc = -EINVAL;
			goto out;
		}
	}

	if (!ice_dev) {
		pr_err("%s: No ice device found\n", __func__);
		rc = -EINVAL;
		goto out;
	}

	/* Create a Kset for the encrypted partitions */
	ice_dev->fde_partitions = kset_create_and_add("fde_partitions", NULL, &ice_dev->pdev->kobj);
	if (ice_dev->fde_partitions == NULL) {
		rc = -EINVAL;
		pr_err("%s: Failed to create partitons KSet\n", __func__);
		goto err_ice_dev;
	}


	/* Set up platform device attributes in sysfs */
	rc = sysfs_create_groups(&ice_dev->pdev->kobj, ice_groups);
	if (rc) {
		pr_err("%s: Failed to add pdev attributes: %d\n", __func__, rc);
		goto err_ice_dev;
	} else {
		ice_dev->sysfs_groups_created = true;
	}

	/* Set up partition config folder in sysfs */
	ice_dev->partitions_kobj_type.release = crypto_qti_ice_release_kobj;

	goto out;

err_ice_dev:
	crypto_qti_ice_cleanup_sysfs(ice_dev);
out:
	return rc;
}
EXPORT_SYMBOL(crypto_qti_ice_init_fde_node);

int crypto_qti_ice_setup_ice_hw(const char *storage_type, int enable)
{
	int ret = -EINVAL;
	struct ice_device *ice_dev = NULL;

	ice_dev = crypto_qti_get_ice_device_from_storage_type(storage_type);
	if (ice_dev == ERR_PTR(-EPROBE_DEFER))
		return -EPROBE_DEFER;

	if (!ice_dev || !ice_dev->is_ice_enabled)
		return ret;
	if (enable)
		return crypto_qti_ice_enable_setup(ice_dev);
	else
		return crypto_qti_ice_disable_setup(ice_dev);
}
EXPORT_SYMBOL(crypto_qti_ice_setup_ice_hw);

#else /* dumy ufsice driver */

static int crypto_qti_ice_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "Proxy probing\n");
	return 0;
}

static int crypto_qti_ice_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "Proxy removing\n");
	return 0;
}

#endif /* CONFIG_QTI_CRYPTO_FDE */

/* Following struct is required to match device with driver from dts file */

static const struct of_device_id crypto_qti_ice_match[] = {
	{ .compatible = "qcom,ice" },
	{},
};
MODULE_DEVICE_TABLE(of, crypto_qti_ice_match);

static struct platform_driver crypto_qti_ice_driver = {
	.probe          = crypto_qti_ice_probe,
	.remove         = crypto_qti_ice_remove,
	.driver         = {
		.name   = "qcom_ice",
		.of_match_table = crypto_qti_ice_match,
	},
};
module_platform_driver(crypto_qti_ice_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Common crypto library for storage encryption");
