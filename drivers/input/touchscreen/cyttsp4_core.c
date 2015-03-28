/* add cypress new driver ttda-02.03.01.476713 */
/* add the log dynamic control */

/*
 * cyttsp4_core.c
 * Cypress TrueTouch(TM) Standard Product V4 Core driver module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * Author: Aleksej Makarov <aleksej.makarov@sonyericsson.com>
 * Modified by: Cypress Semiconductor to add device functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include <linux/cyttsp4_bus.h>

#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

/* delete the fb_call set in core driver */

/*Add file head*/
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include "cyttsp4_i2c.h"

#include <linux/cyttsp4_core.h>
#include "cyttsp4_regs.h"
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif
#include "cyttsp4_device_access.h"
#include <linux/hw_tp_common.h>
#define HUAWEI_SET_FINGER_MODE_BY_DEFAULT

#define CY_CORE_READTIMES 3
/* Timeout in ms. */
#define CY_CORE_REQUEST_EXCLUSIVE_TIMEOUT	5000	//hyuc
#define CY_CORE_SLEEP_REQUEST_EXCLUSIVE_TIMEOUT	5000
#define CY_CORE_WAIT_SYSINFO_MODE_TIMEOUT	2000
/*Modify calibration time from 1s to 5s */
#define CY_CORE_MODE_CHANGE_TIMEOUT		5000
#define CY_CORE_RESET_AND_WAIT_TIMEOUT		1000	//hyuc
#define CY_CORE_WAKEUP_TIMEOUT			1000	//hyuc

#define CY_CORE_STARTUP_RETRY_COUNT		3
static atomic_t holster_status_flag;
unsigned long holster_status = 0;	//holster status remember
/* add a parameter for the module */
int cyttsp_debug_mask = TP_INFO;
module_param_named(cyttsp_debug_mask, cyttsp_debug_mask, int, 0664);

#define IS_DEEP_SLEEP_CONFIGURED(x) \
		((x) == 0 || (x) == 0xFF)

#define GESTURE_TO_APP(_x) ((_x)|(u16)(BIT(8)))
#define GESTURE_FROM_APP(_x) ((_x)&0xFF)

#define IS_TMO(t)	((t) == 0)

#define PUT_FIELD16(si, val, addr) \
do { \
	if (IS_LITTLEENDIAN((si)->si_ptrs.cydata->device_info)) \
		put_unaligned_le16(val, addr); \
	else \
		put_unaligned_be16(val, addr); \
} while (0)

#define GET_FIELD16(si, addr) \
({ \
	u16 __val; \
	if (IS_LITTLEENDIAN((si)->si_ptrs.cydata->device_info)) \
		__val = get_unaligned_le16(addr); \
	else \
		__val = get_unaligned_be16(addr); \
	__val; \
})
/*the code aim to reset the tp ic when retry > 0,when retry < 0,it will goto exit_label*/
#define RETRY_OR_EXIT(retry_cnt, retry_label, exit_label) \
do { \
	if (retry_cnt > 0) \
		goto retry_label; \
	goto exit_label; \
} while (0)
static const u8 security_key[] = {
	0xA5, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD, 0x5A
};

static const u8 ldr_exit[] = {
	0xFF, 0x01, 0x3B, 0x00, 0x00, 0x4F, 0x6D, 0x17
};

static const u8 ldr_err_app[] = {
	0x01, 0x02, 0x00, 0x00, 0x55, 0xDD, 0x17
};

MODULE_FIRMWARE(CY_FW_FILE_NAME);

const char *cy_driver_core_name = CYTTSP4_CORE_NAME;
const char *cy_driver_core_version = CY_DRIVER_VERSION;
const char *cy_driver_core_date = CY_DRIVER_DATE;

enum cyttsp4_sleep_state {
	SS_SLEEP_OFF,
	SS_SLEEP_ON,
	SS_SLEEPING,
	SS_WAKING,
};

enum cyttsp4_startup_state {
	STARTUP_NONE,
	STARTUP_QUEUED,
	STARTUP_RUNNING,
};

enum cyttsp4_opmode {
	OPMODE_NONE,
	OPMODE_FINGER,
	OPMODE_GLOVE,
};

struct cyttsp4_core_data {
	struct device *dev;
	struct cyttsp4_core *core;
	struct list_head atten_list[CY_ATTEN_NUM_ATTEN];
	struct mutex system_lock;
	struct mutex adap_lock;
	enum cyttsp4_mode mode;
	enum cyttsp4_sleep_state sleep_state;
	enum cyttsp4_startup_state startup_state;
	enum cyttsp4_opmode opmode;
	int int_status;
	int cmd_toggle;
	spinlock_t spinlock;
	struct cyttsp4_core_platform_data *pdata;
	wait_queue_head_t wait_q;
	int irq;
	struct work_struct startup_work;
	struct cyttsp4_sysinfo sysinfo;
	void *exclusive_dev;
	int exclusive_waits;
	atomic_t ignore_irq;
	bool irq_enabled;
	bool irq_wake;
	bool wake_initiated_by_device;
	bool invalid_touch_app;
	int max_xfer;
	int apa_mc_en;
	int glove_en;
	int stylus_en;
	int proximity_en;
	u8 default_scantype;
	u8 easy_wakeup_gesture;
	u8 easy_wakeup_supported_gestures;
	unsigned int active_refresh_cycle_ms;
	u8 heartbeat_count;
#ifdef VERBOSE_DEBUG
	u8 pr_buf[CY_MAX_PRBUF_SIZE];
#endif
	struct work_struct watchdog_work;
	struct timer_list watchdog_timer;
	/* delete the fb_call set in core driver */
};

struct atten_node {
	struct list_head node;
	int (*func) (struct cyttsp4_device *);
	struct cyttsp4_device *ttsp;
	int mode;
};

static struct device *core_dev_ptr = NULL;
int set_holster_sensitivity(struct cyttsp4_core_data *cd, unsigned long status);

static int _cyttsp4_put_device_into_deep_sleep(struct cyttsp4_core_data *cd,
					       u8 hst_mode_reg);

static inline u32 merge_bytes(u8 high, u8 low)
{
	return (high << 8) + low;
}

#ifdef VERBOSE_DEBUG
void cyttsp4_pr_buf(struct device *dev, u8 * pr_buf, u8 * dptr, int size,
		    const char *data_name)
{
	int i, k;
	const char fmt[] = "%02X ";
	int max;

	if (!size)
		return;

	max = (CY_MAX_PRBUF_SIZE - 1) - sizeof(CY_PR_TRUNCATED);

	pr_buf[0] = 0;
	for (i = k = 0; i < size && k < max; i++, k += 3)
		scnprintf(pr_buf + k, CY_MAX_PRBUF_SIZE, fmt, dptr[i]);

	tp_log_debug("%s:  %s[0..%d]=%s%s\n", __func__, data_name, size - 1,
		     pr_buf, size <= max ? "" : CY_PR_TRUNCATED);
}

EXPORT_SYMBOL_GPL(cyttsp4_pr_buf);
#endif

static inline int cyttsp4_adap_read(struct cyttsp4_core_data *cd, u16 addr,
				    void *buf, int size)
{
	return cd->core->adap->read(cd->core->adap, addr, buf, size,
				    cd->max_xfer);
}

static inline int cyttsp4_adap_write(struct cyttsp4_core_data *cd, u16 addr,
				     const void *buf, int size)
{
	return cd->core->adap->write(cd->core->adap, addr, buf, size,
				     cd->max_xfer);
}

/* cyttsp4_platform_detect_read()
 *
 * This function is passed to platform detect
 * function to perform a read operation
 */
#ifdef CYTTSP4_DETECT_HW
static int cyttsp4_platform_detect_read(struct device *dev, u16 addr,
					void *buf, int size)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	return cd->core->adap->read(cd->core->adap, addr, buf, size,
				    cd->max_xfer);
}
#endif

static u16 cyttsp4_calc_partial_app_crc(const u8 * data, int size, u16 crc)
{
	int i, j;

	for (i = 0; i < size; i++) {
		crc ^= ((u16) data[i] << 8);
		for (j = 8; j > 0; j--)
			if (crc & 0x8000)
				crc = (crc << 1) ^ 0x1021;
			else
				crc <<= 1;
	}

	return crc;
}

static inline u16 cyttsp4_calc_app_crc(const u8 * data, int size)
{
	return cyttsp4_calc_partial_app_crc(data, size, 0xFFFF);
}

static const u8 *cyttsp4_get_security_key_(struct cyttsp4_device *ttsp,
					   int *size)
{
	if (size)
		*size = sizeof(security_key);

	return security_key;
}

static inline void cyttsp4_get_touch_axis(struct cyttsp4_core_data *cd,
					  int *axis, int size, int max,
					  u8 * xy_data, int bofs)
{
	int nbyte;
	int next;

	for (nbyte = 0, *axis = 0, next = 0; nbyte < size; nbyte++) {
		tp_log_vdebug("%s: *axis=%02X(%d) size=%d max=%08X xy_data=%p"
			      " xy_data[%d]=%02X(%d) bofs=%d\n",
			      __func__, *axis, *axis, size, max, xy_data, next,
			      xy_data[next], xy_data[next], bofs);
		*axis = (*axis * 256) + (xy_data[next] >> bofs);
		next++;
	}

	*axis &= max - 1;

	tp_log_vdebug("%s: *axis=%02X(%d) size=%d max=%08X xy_data=%p"
		      " xy_data[%d]=%02X(%d)\n",
		      __func__, *axis, *axis, size, max, xy_data, next,
		      xy_data[next], xy_data[next]);
}


/*
 * cyttsp4_get_touch_record_()
 *
 * Fills touch info for a touch record specified by rec_no
 * Should only be called in Operational mode IRQ attention and
 * rec_no should be less than the number of current touch records
 */
static void cyttsp4_get_touch_record_(struct cyttsp4_device *ttsp,
				      int rec_no, int *rec_abs)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	u8 *xy_data = si->xy_data + (rec_no * si->si_ofs.tch_rec_size);
	enum cyttsp4_tch_abs abs;

	memset(rec_abs, 0, CY_TCH_NUM_ABS * sizeof(int));
	for (abs = CY_TCH_X; abs < CY_TCH_NUM_ABS; abs++) {
		cyttsp4_get_touch_axis(cd, &rec_abs[abs],
				       si->si_ofs.tch_abs[abs].size,
				       si->si_ofs.tch_abs[abs].max,
				       xy_data + si->si_ofs.tch_abs[abs].ofs,
				       si->si_ofs.tch_abs[abs].bofs);
		tp_log_vdebug("%s: get %s=%04X(%d)\n", __func__,
			      cyttsp4_tch_abs_string[abs],
			      rec_abs[abs], rec_abs[abs]);
	}
}

static int cyttsp4_load_status_and_touch_regs(struct cyttsp4_core_data *cd,
					      bool optimize)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	int first_read_len;
	int second_read_off;
	int num_read_rec;
	u8 num_cur_rec;
	u8 hst_mode;
	u8 rep_len;
	u8 rep_stat;
	u8 tt_stat;
	int rc;

	if (!si->xy_mode) {
		tp_log_err("%s: NULL xy_mode pointer\n", __func__);
		return -EINVAL;
	}

	first_read_len = si->si_ofs.rep_hdr_size;
	/* Read one touch record additionally */
	if (optimize)
		first_read_len += si->si_ofs.tch_rec_size;

	rc = cyttsp4_adap_read(cd, si->si_ofs.rep_ofs,
			       &si->xy_mode[si->si_ofs.rep_ofs],
			       first_read_len);
	if (rc < 0) {
		tp_log_err("%s: fail read mode regs r=%d\n", __func__, rc);
		return rc;
	}

	/* print xy data */
	cyttsp4_pr_buf(dev, cd->pr_buf, si->xy_mode,
		       si->si_ofs.mode_size, "xy_mode");

	hst_mode = si->xy_mode[CY_REG_BASE];
	rep_len = si->xy_mode[si->si_ofs.rep_ofs];
	rep_stat = si->xy_mode[si->si_ofs.rep_ofs + 1];
	tt_stat = si->xy_mode[si->si_ofs.tt_stat_ofs];
	tp_log_vdebug("%s: %s%02X %s%d %s%02X %s%02X\n", __func__,
		      "hst_mode=", hst_mode, "rep_len=", rep_len,
		      "rep_stat=", rep_stat, "tt_stat=", tt_stat);

	num_cur_rec = GET_NUM_TOUCH_RECORDS(tt_stat);
	tp_log_vdebug("%s: num_cur_rec=%d\n", __func__, num_cur_rec);

	if (rep_len == 0 && num_cur_rec > 0) {
		tp_log_err("%s: report length error rep_len=%d num_rec=%d\n",
			   __func__, rep_len, num_cur_rec);
		return -EIO;
	}

	num_read_rec = num_cur_rec;
	second_read_off = si->si_ofs.tt_stat_ofs + 1;
	if (optimize) {
		num_read_rec--;
		second_read_off += si->si_ofs.tch_rec_size;
	}

	if (num_read_rec <= 0)
		goto exit_print;

	rc = cyttsp4_adap_read(cd, second_read_off,
			       &si->xy_mode[second_read_off],
			       num_read_rec * si->si_ofs.tch_rec_size);
	if (rc < 0) {
		tp_log_err("%s: read fail on touch regs r=%d\n", __func__, rc);
		return rc;
	}

exit_print:
	/* print xy data */
	cyttsp4_pr_buf(dev, cd->pr_buf, si->xy_data,
		       num_cur_rec * si->si_ofs.tch_rec_size, "xy_data");

	return 0;
}

static int cyttsp4_handshake(struct cyttsp4_core_data *cd, u8 mode)
{
	u8 cmd = mode ^ CY_HST_TOGGLE;
	int rc;

	if (mode & CY_HST_MODE_CHANGE) {
		tp_log_err("%s: Host mode change bit set, NO handshake\n",
			   __func__);
		return 0;
	}

	rc = cyttsp4_adap_write(cd, CY_REG_BASE, &cmd, sizeof(cmd));
	if (rc < 0)
		tp_log_err("%s: bus write fail on handshake (ret=%d)\n",
			   __func__, rc);

	return rc;
}

static int cyttsp4_toggle_low_power_(struct cyttsp4_core_data *cd, u8 mode)
{
	u8 cmd = mode ^ CY_HST_LOWPOW;

	int rc = cyttsp4_adap_write(cd, CY_REG_BASE, &cmd, sizeof(cmd));
	if (rc < 0)
		tp_log_err("%s: bus write fail on toggle low power (ret=%d)\n",
			   __func__, rc);
	return rc;
}

static int cyttsp4_toggle_low_power(struct cyttsp4_core_data *cd, u8 mode)
{
	int rc;

	mutex_lock(&cd->system_lock);
	rc = cyttsp4_toggle_low_power_(cd, mode);
	mutex_unlock(&cd->system_lock);

	return rc;
}

static int cyttsp4_power_state(struct cyttsp4_core_data *cd)
{
	int ret = 0;

	mutex_lock(&cd->system_lock);
	if (cd->sleep_state == SS_SLEEP_ON)
		ret = 0;
	else
		ret = 1;
	mutex_unlock(&cd->system_lock);

	return ret;
}


static int cyttsp4_hw_soft_reset_(struct cyttsp4_core_data *cd)
{
	u8 cmd = CY_HST_RESET;

	int rc = cyttsp4_adap_write(cd, CY_REG_BASE, &cmd, sizeof(cmd));
	if (rc < 0) {
		tp_log_err("%s: FAILED to execute SOFT reset\n", __func__);
		return rc;
	}
	tp_log_debug("%s: execute SOFT reset\n", __func__);
	return 0;
}

static int cyttsp4_hw_soft_reset(struct cyttsp4_core_data *cd)
{
	int rc;

	mutex_lock(&cd->system_lock);
	rc = cyttsp4_hw_soft_reset_(cd);
	mutex_unlock(&cd->system_lock);

	return rc;
}

static int cyttsp4_hw_hard_reset_(struct cyttsp4_core_data *cd)
{
	if (cd->pdata->xres) {
		cd->pdata->xres(cd->pdata, cd->dev);
		tp_log_debug("%s: execute HARD reset\n", __func__);
		return 0;
	}
	tp_log_err("%s: FAILED to execute HARD reset\n", __func__);
	return -ENOSYS;
}

static int cyttsp4_hw_hard_reset(struct cyttsp4_core_data *cd)
{
	int rc;

	mutex_lock(&cd->system_lock);
	rc = cyttsp4_hw_hard_reset_(cd);
	mutex_unlock(&cd->system_lock);

	return rc;
}

static int cyttsp4_hw_reset_(struct cyttsp4_core_data *cd)
{
	int rc = cyttsp4_hw_hard_reset_(cd);
	if (rc == -ENOSYS)
		rc = cyttsp4_hw_soft_reset_(cd);
	return rc;
}

static inline int cyttsp4_bits_2_bytes(int nbits, u32 * max)
{
	*max = 1 << nbits;
	return (nbits + 7) / 8;
}

static int cyttsp4_si_data_offsets(struct cyttsp4_core_data *cd)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	int rc = cyttsp4_adap_read(cd, CY_REG_BASE, &si->si_data,
				   sizeof(si->si_data));
	if (rc < 0) {
		tp_log_err("%s: fail read sysinfo data offsets r=%d\n",
			   __func__, rc);
		return rc;
	}

	/* Print sysinfo data offsets */
	cyttsp4_pr_buf(cd->dev, cd->pr_buf, (u8 *) & si->si_data,
		       sizeof(si->si_data), "sysinfo_data_offsets");

	/* convert sysinfo data offset bytes into integers */

	si->si_ofs.map_sz = merge_bytes(si->si_data.map_szh,
					si->si_data.map_szl);
	si->si_ofs.map_sz = merge_bytes(si->si_data.map_szh,
					si->si_data.map_szl);
	si->si_ofs.cydata_ofs = merge_bytes(si->si_data.cydata_ofsh,
					    si->si_data.cydata_ofsl);
	si->si_ofs.test_ofs = merge_bytes(si->si_data.test_ofsh,
					  si->si_data.test_ofsl);
	si->si_ofs.pcfg_ofs = merge_bytes(si->si_data.pcfg_ofsh,
					  si->si_data.pcfg_ofsl);
	si->si_ofs.opcfg_ofs = merge_bytes(si->si_data.opcfg_ofsh,
					   si->si_data.opcfg_ofsl);
	si->si_ofs.ddata_ofs = merge_bytes(si->si_data.ddata_ofsh,
					   si->si_data.ddata_ofsl);
	si->si_ofs.mdata_ofs = merge_bytes(si->si_data.mdata_ofsh,
					   si->si_data.mdata_ofsl);
	return rc;
}

static int cyttsp4_si_get_cydata(struct cyttsp4_core_data *cd)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	int read_offset;
	int mfgid_sz, calc_mfgid_sz;
	void *p;
	int rc;

	si->si_ofs.cydata_size = si->si_ofs.test_ofs - si->si_ofs.cydata_ofs;
	tp_log_debug("%s: cydata size: %d\n", __func__, si->si_ofs.cydata_size);

	if (si->si_ofs.cydata_size <= 0)
		return -EINVAL;

	p = krealloc(si->si_ptrs.cydata, si->si_ofs.cydata_size, GFP_KERNEL);
	if (p == NULL) {
		tp_log_err("%s: fail alloc cydata memory\n", __func__);
		return -ENOMEM;
	}
	si->si_ptrs.cydata = p;

	read_offset = si->si_ofs.cydata_ofs;

	/* Read the CYDA registers up to MFGID field */
	rc = cyttsp4_adap_read(cd, read_offset, si->si_ptrs.cydata,
			       offsetof(struct cyttsp4_cydata, mfgid_sz)
			       +sizeof(si->si_ptrs.cydata->mfgid_sz));
	if (rc < 0) {
		tp_log_err("%s: fail read cydata r=%d\n", __func__, rc);
		return rc;
	}

	/* Check MFGID size */
	mfgid_sz = si->si_ptrs.cydata->mfgid_sz;
	calc_mfgid_sz = si->si_ofs.cydata_size - sizeof(struct cyttsp4_cydata);
	if (mfgid_sz != calc_mfgid_sz) {
		tp_log_err
		    ("%s: mismatch in MFGID size, reported:%d calculated:%d\n",
		     __func__, mfgid_sz, calc_mfgid_sz);
		return -EINVAL;
	}

	read_offset += offsetof(struct cyttsp4_cydata, mfgid_sz)
	    +sizeof(si->si_ptrs.cydata->mfgid_sz);

	/* Read the CYDA registers for MFGID field */
	rc = cyttsp4_adap_read(cd, read_offset, si->si_ptrs.cydata->mfg_id,
			       si->si_ptrs.cydata->mfgid_sz);
	if (rc < 0) {
		tp_log_err("%s: fail read cydata r=%d\n", __func__, rc);
		return rc;
	}

	read_offset += si->si_ptrs.cydata->mfgid_sz;

	/* Read the rest of the CYDA registers */
	rc = cyttsp4_adap_read(cd, read_offset, &si->si_ptrs.cydata->cyito_idh,
			       sizeof(struct cyttsp4_cydata)
			       - offsetof(struct cyttsp4_cydata, cyito_idh));
	if (rc < 0) {
		tp_log_err("%s: fail read cydata r=%d\n", __func__, rc);
		return rc;
	}

	cyttsp4_pr_buf(cd->dev, cd->pr_buf, (u8 *) si->si_ptrs.cydata,
		       si->si_ofs.cydata_size - mfgid_sz, "sysinfo_cydata");
	cyttsp4_pr_buf(cd->dev, cd->pr_buf, si->si_ptrs.cydata->mfg_id,
		       mfgid_sz, "sysinfo_cydata_mfgid");
	return rc;
}

static int cyttsp4_si_get_test_data(struct cyttsp4_core_data *cd)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	void *p;
	int rc;

	si->si_ofs.test_size = si->si_ofs.pcfg_ofs - si->si_ofs.test_ofs;

	if (si->si_ofs.test_size <= 0)
		return -EINVAL;

	p = krealloc(si->si_ptrs.test, si->si_ofs.test_size, GFP_KERNEL);
	if (p == NULL) {
		tp_log_err("%s: fail alloc test memory\n", __func__);
		return -ENOMEM;
	}
	si->si_ptrs.test = p;

	rc = cyttsp4_adap_read(cd, si->si_ofs.test_ofs, si->si_ptrs.test,
			       si->si_ofs.test_size);
	if (rc < 0) {
		tp_log_err("%s: fail read test data r=%d\n", __func__, rc);
		return rc;
	}

	cyttsp4_pr_buf(cd->dev, cd->pr_buf,
		       (u8 *) si->si_ptrs.test, si->si_ofs.test_size,
		       "sysinfo_test_data");
	if (si->si_ptrs.test->post_codel & CY_POST_CODEL_WDG_RST)
		tp_log_info("%s: %s codel=%02X\n",
			    __func__, "Reset was a WATCHDOG RESET",
			    si->si_ptrs.test->post_codel);

	if (!(si->si_ptrs.test->post_codel & CY_POST_CODEL_CFG_DATA_CRC_FAIL))
		tp_log_info("%s: %s codel=%02X\n", __func__,
			    "Config Data CRC FAIL",
			    si->si_ptrs.test->post_codel);

	if (!(si->si_ptrs.test->post_codel & CY_POST_CODEL_PANEL_TEST_FAIL))
		tp_log_info("%s: %s codel=%02X\n",
			    __func__, "PANEL TEST FAIL",
			    si->si_ptrs.test->post_codel);

	tp_log_debug("%s: SCANNING is %s codel=%02X\n",
		     __func__, si->si_ptrs.test->post_codel & 0x08 ?
		     "ENABLED" : "DISABLED", si->si_ptrs.test->post_codel);
	return rc;
}

static int cyttsp4_si_get_pcfg_data(struct cyttsp4_core_data *cd)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	void *p;
	int rc;

	tp_log_debug("%s: get pcfg data\n", __func__);
	si->si_ofs.pcfg_size = si->si_ofs.opcfg_ofs - si->si_ofs.pcfg_ofs;

	if (si->si_ofs.pcfg_size <= 0)
		return -EINVAL;

	p = krealloc(si->si_ptrs.pcfg, si->si_ofs.pcfg_size, GFP_KERNEL);
	if (p == NULL) {
		rc = -ENOMEM;
		tp_log_err("%s: fail alloc pcfg memory r=%d\n", __func__, rc);
		return rc;
	}
	si->si_ptrs.pcfg = p;

	rc = cyttsp4_adap_read(cd, si->si_ofs.pcfg_ofs, si->si_ptrs.pcfg,
			       si->si_ofs.pcfg_size);
	if (rc < 0) {
		tp_log_err("%s: fail read pcfg data r=%d\n", __func__, rc);
		return rc;
	}

	si->si_ofs.max_x = merge_bytes((si->si_ptrs.pcfg->res_xh
					& CY_PCFG_RESOLUTION_X_MASK),
				       si->si_ptrs.pcfg->res_xl);
	si->si_ofs.x_origin =
	    ! !(si->si_ptrs.pcfg->res_xh & CY_PCFG_ORIGIN_X_MASK);
	si->si_ofs.max_y =
	    merge_bytes((si->si_ptrs.pcfg->res_yh & CY_PCFG_RESOLUTION_Y_MASK),
			si->si_ptrs.pcfg->res_yl);
	si->si_ofs.y_origin =
	    ! !(si->si_ptrs.pcfg->res_yh & CY_PCFG_ORIGIN_Y_MASK);
	si->si_ofs.max_p =
	    merge_bytes(si->si_ptrs.pcfg->max_zh, si->si_ptrs.pcfg->max_zl);

	cyttsp4_pr_buf(cd->dev, cd->pr_buf,
		       (u8 *) si->si_ptrs.pcfg,
		       si->si_ofs.pcfg_size, "sysinfo_pcfg_data");
	return rc;
}

static int cyttsp4_si_get_opcfg_data(struct cyttsp4_core_data *cd)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	int i;
	enum cyttsp4_tch_abs abs;
	void *p;
	int rc;

	tp_log_debug("%s: get opcfg data\n", __func__);
	si->si_ofs.opcfg_size = si->si_ofs.ddata_ofs - si->si_ofs.opcfg_ofs;

	if (si->si_ofs.opcfg_size <= 0)
		return -EINVAL;

	p = krealloc(si->si_ptrs.opcfg, si->si_ofs.opcfg_size, GFP_KERNEL);
	if (p == NULL) {
		tp_log_err("%s: fail alloc opcfg memory\n", __func__);
		rc = -ENOMEM;
		goto cyttsp4_si_get_opcfg_data_exit;
	}
	si->si_ptrs.opcfg = p;

	rc = cyttsp4_adap_read(cd, si->si_ofs.opcfg_ofs, si->si_ptrs.opcfg,
			       si->si_ofs.opcfg_size);
	if (rc < 0) {
		tp_log_err("%s: fail read opcfg data r=%d\n", __func__, rc);
		goto cyttsp4_si_get_opcfg_data_exit;
	}
	si->si_ofs.cmd_ofs = si->si_ptrs.opcfg->cmd_ofs;
	si->si_ofs.rep_ofs = si->si_ptrs.opcfg->rep_ofs;
	si->si_ofs.rep_sz = (si->si_ptrs.opcfg->rep_szh * 256) +
	    si->si_ptrs.opcfg->rep_szl;
	si->si_ofs.num_btns = si->si_ptrs.opcfg->num_btns;
	si->si_ofs.num_btn_regs = (si->si_ofs.num_btns +
				   CY_NUM_BTN_PER_REG - 1) / CY_NUM_BTN_PER_REG;
	si->si_ofs.tt_stat_ofs = si->si_ptrs.opcfg->tt_stat_ofs;
	si->si_ofs.obj_cfg0 = si->si_ptrs.opcfg->obj_cfg0;
	si->si_ofs.max_tchs = si->si_ptrs.opcfg->max_tchs & CY_BYTE_OFS_MASK;
	si->si_ofs.tch_rec_size = si->si_ptrs.opcfg->tch_rec_size &
	    CY_BYTE_OFS_MASK;

	/* Get the old touch fields */
	for (abs = CY_TCH_X; abs < CY_NUM_TCH_FIELDS; abs++) {
		si->si_ofs.tch_abs[abs].ofs =
		    si->si_ptrs.opcfg->tch_rec_old[abs].loc & CY_BYTE_OFS_MASK;
		si->si_ofs.tch_abs[abs].size =
		    cyttsp4_bits_2_bytes
		    (si->si_ptrs.opcfg->tch_rec_old[abs].size,
		     &si->si_ofs.tch_abs[abs].max);
		si->si_ofs.tch_abs[abs].bofs =
		    (si->si_ptrs.opcfg->tch_rec_old[abs].loc &
		     CY_BOFS_MASK) >> CY_BOFS_SHIFT;
	}

	/* button fields */
	si->si_ofs.btn_rec_size = si->si_ptrs.opcfg->btn_rec_size;
	si->si_ofs.btn_diff_ofs = si->si_ptrs.opcfg->btn_diff_ofs;
	si->si_ofs.btn_diff_size = si->si_ptrs.opcfg->btn_diff_size;

	if (IS_TTSP_VER_GE(si, 2, 3)) {
		/* Get the extended touch fields */
		for (i = 0; i < CY_NUM_EXT_TCH_FIELDS; abs++, i++) {
			si->si_ofs.tch_abs[abs].ofs =
			    si->si_ptrs.opcfg->tch_rec_new[i].loc &
			    CY_BYTE_OFS_MASK;
			si->si_ofs.tch_abs[abs].size =
			    cyttsp4_bits_2_bytes
			    (si->si_ptrs.opcfg->tch_rec_new[i].size,
			     &si->si_ofs.tch_abs[abs].max);
			si->si_ofs.tch_abs[abs].bofs =
			    (si->si_ptrs.opcfg->tch_rec_new[i].loc
			     & CY_BOFS_MASK) >> CY_BOFS_SHIFT;
		}
	}

	if (IS_TTSP_VER_GE(si, 2, 4)) {
		si->si_ofs.noise_data_ofs = si->si_ptrs.opcfg->noise_data_ofs;
		si->si_ofs.noise_data_sz = si->si_ptrs.opcfg->noise_data_sz;
	}

	for (abs = 0; abs < CY_TCH_NUM_ABS; abs++) {
		tp_log_debug("%s: tch_rec_%s\n", __func__,
			     cyttsp4_tch_abs_string[abs]);
		tp_log_debug("%s:     ofs =%2d\n", __func__,
			     si->si_ofs.tch_abs[abs].ofs);
		tp_log_debug("%s:     siz =%2d\n", __func__,
			     si->si_ofs.tch_abs[abs].size);
		tp_log_debug("%s:     max =%2d\n", __func__,
			     si->si_ofs.tch_abs[abs].max);
		tp_log_debug("%s:     bofs=%2d\n", __func__,
			     si->si_ofs.tch_abs[abs].bofs);
	}

	si->si_ofs.mode_size = si->si_ofs.tt_stat_ofs + 1;
	si->si_ofs.data_size = si->si_ofs.max_tchs *
	    si->si_ptrs.opcfg->tch_rec_size;
	si->si_ofs.rep_hdr_size = si->si_ofs.mode_size - si->si_ofs.rep_ofs;

	cyttsp4_pr_buf(cd->dev, cd->pr_buf, (u8 *) si->si_ptrs.opcfg,
		       si->si_ofs.opcfg_size, "sysinfo_opcfg_data");

cyttsp4_si_get_opcfg_data_exit:
	return rc;
}

static int cyttsp4_si_get_ddata(struct cyttsp4_core_data *cd)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	void *p;
	int rc;

	tp_log_debug("%s: get ddata data\n", __func__);
	si->si_ofs.ddata_size = si->si_ofs.mdata_ofs - si->si_ofs.ddata_ofs;

	if (si->si_ofs.ddata_size <= 0)
		return -EINVAL;

	p = krealloc(si->si_ptrs.ddata, si->si_ofs.ddata_size, GFP_KERNEL);
	if (p == NULL) {
		tp_log_err("%s: fail alloc ddata memory\n", __func__);
		return -ENOMEM;
	}
	si->si_ptrs.ddata = p;

	rc = cyttsp4_adap_read(cd, si->si_ofs.ddata_ofs, si->si_ptrs.ddata,
			       si->si_ofs.ddata_size);
	if (rc < 0)
		tp_log_err("%s: fail read ddata data r=%d\n", __func__, rc);
	else
		cyttsp4_pr_buf(cd->dev, cd->pr_buf,
			       (u8 *) si->si_ptrs.ddata,
			       si->si_ofs.ddata_size, "sysinfo_ddata");
	return rc;
}

static int cyttsp4_si_get_mdata(struct cyttsp4_core_data *cd)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	void *p;
	int rc;

	tp_log_debug("%s: get mdata data\n", __func__);
	si->si_ofs.mdata_size = si->si_ofs.map_sz - si->si_ofs.mdata_ofs;

	if (si->si_ofs.mdata_size <= 0)
		return -EINVAL;

	p = krealloc(si->si_ptrs.mdata, si->si_ofs.mdata_size, GFP_KERNEL);
	if (p == NULL) {
		tp_log_err("%s: fail alloc mdata memory\n", __func__);
		return -ENOMEM;
	}
	si->si_ptrs.mdata = p;

	rc = cyttsp4_adap_read(cd, si->si_ofs.mdata_ofs, si->si_ptrs.mdata,
			       si->si_ofs.mdata_size);
	if (rc < 0)
		tp_log_err("%s: fail read mdata data r=%d\n", __func__, rc);
	else
		cyttsp4_pr_buf(cd->dev, cd->pr_buf,
			       (u8 *) si->si_ptrs.mdata,
			       si->si_ofs.mdata_size, "sysinfo_mdata");
	return rc;
}

static int cyttsp4_si_get_btn_data(struct cyttsp4_core_data *cd)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	int btn;
	int num_defined_keys;
	u16 *key_table;
	void *p;
	int rc = 0;

	tp_log_debug("%s: get btn data\n", __func__);

	if (!si->si_ofs.num_btns) {
		si->si_ofs.btn_keys_size = 0;
		kfree(si->btn);
		si->btn = NULL;
		return rc;
	}

	si->si_ofs.btn_keys_size = si->si_ofs.num_btns *
	    sizeof(struct cyttsp4_btn);

	if (si->si_ofs.btn_keys_size <= 0)
		return -EINVAL;

	p = krealloc(si->btn, si->si_ofs.btn_keys_size,
		     GFP_KERNEL | __GFP_ZERO);
	if (p == NULL) {
		tp_log_err("%s: %s\n", __func__, "fail alloc btn_keys memory");
		return -ENOMEM;
	}
	si->btn = p;

	if (cd->pdata->sett[CY_IC_GRPNUM_BTN_KEYS] == NULL)
		num_defined_keys = 0;
	else if (cd->pdata->sett[CY_IC_GRPNUM_BTN_KEYS]->data == NULL)
		num_defined_keys = 0;
	else
		num_defined_keys = cd->pdata->sett[CY_IC_GRPNUM_BTN_KEYS]->size;

	for (btn = 0; btn < si->si_ofs.num_btns
	     && btn < num_defined_keys; btn++) {
		key_table =
		    (u16 *) cd->pdata->sett[CY_IC_GRPNUM_BTN_KEYS]->data;
		si->btn[btn].key_code = key_table[btn];
		si->btn[btn].state = CY_BTN_RELEASED;
		si->btn[btn].enabled = true;
	}
	for (; btn < si->si_ofs.num_btns; btn++) {
		si->btn[btn].key_code = KEY_RESERVED;
		si->btn[btn].state = CY_BTN_RELEASED;
		si->btn[btn].enabled = true;
	}

	return rc;
}

static int cyttsp4_si_get_op_data_ptrs(struct cyttsp4_core_data *cd)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	void *p;
	int size;

	p = krealloc(si->xy_mode, si->si_ofs.mode_size +
		     si->si_ofs.data_size, GFP_KERNEL | __GFP_ZERO);
	if (p == NULL)
		return -ENOMEM;
	si->xy_mode = p;
	si->xy_data = &si->xy_mode[si->si_ofs.tt_stat_ofs + 1];

	size = si->si_ofs.btn_rec_size * si->si_ofs.num_btns;
	if (!size)
		return 0;

	p = krealloc(si->btn_rec_data, size, GFP_KERNEL | __GFP_ZERO);
	if (p == NULL)
		return -ENOMEM;
	si->btn_rec_data = p;

	return 0;
}

static void cyttsp4_si_put_log_data(struct cyttsp4_core_data *cd)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	tp_log_debug("%s: cydata_ofs =%4d siz=%4d\n", __func__,
		     si->si_ofs.cydata_ofs, si->si_ofs.cydata_size);
	tp_log_debug("%s: test_ofs   =%4d siz=%4d\n", __func__,
		     si->si_ofs.test_ofs, si->si_ofs.test_size);
	tp_log_debug("%s: pcfg_ofs   =%4d siz=%4d\n", __func__,
		     si->si_ofs.pcfg_ofs, si->si_ofs.pcfg_size);
	tp_log_debug("%s: opcfg_ofs  =%4d siz=%4d\n", __func__,
		     si->si_ofs.opcfg_ofs, si->si_ofs.opcfg_size);
	tp_log_debug("%s: ddata_ofs  =%4d siz=%4d\n", __func__,
		     si->si_ofs.ddata_ofs, si->si_ofs.ddata_size);
	tp_log_debug("%s: mdata_ofs  =%4d siz=%4d\n", __func__,
		     si->si_ofs.mdata_ofs, si->si_ofs.mdata_size);

	tp_log_debug("%s: cmd_ofs       =%4d\n", __func__, si->si_ofs.cmd_ofs);
	tp_log_debug("%s: rep_ofs       =%4d\n", __func__, si->si_ofs.rep_ofs);
	tp_log_debug("%s: rep_sz        =%4d\n", __func__, si->si_ofs.rep_sz);
	tp_log_debug("%s: num_btns      =%4d\n", __func__, si->si_ofs.num_btns);
	tp_log_debug("%s: num_btn_regs  =%4d\n", __func__,
		     si->si_ofs.num_btn_regs);
	tp_log_debug("%s: tt_stat_ofs   =%4d\n", __func__,
		     si->si_ofs.tt_stat_ofs);
	tp_log_debug("%s: tch_rec_size   =%4d\n", __func__,
		     si->si_ofs.tch_rec_size);
	tp_log_debug("%s: max_tchs      =%4d\n", __func__, si->si_ofs.max_tchs);
	tp_log_debug("%s: mode_size     =%4d\n", __func__,
		     si->si_ofs.mode_size);
	tp_log_debug("%s: data_size     =%4d\n", __func__,
		     si->si_ofs.data_size);
	tp_log_debug("%s: rep_hdr_size  =%4d\n", __func__,
		     si->si_ofs.rep_hdr_size);
	tp_log_debug("%s: map_sz        =%4d\n", __func__, si->si_ofs.map_sz);

	tp_log_debug("%s: btn_rec_size   =%2d\n", __func__,
		     si->si_ofs.btn_rec_size);
	tp_log_debug("%s: btn_diff_ofs  =%2d\n", __func__,
		     si->si_ofs.btn_diff_ofs);
	tp_log_debug("%s: btn_diff_size  =%2d\n", __func__,
		     si->si_ofs.btn_diff_size);

	tp_log_debug("%s: max_x    = 0x%04X (%d)\n", __func__,
		     si->si_ofs.max_x, si->si_ofs.max_x);
	tp_log_debug("%s: x_origin = %d (%s)\n", __func__,
		     si->si_ofs.x_origin,
		     si->si_ofs.x_origin == CY_NORMAL_ORIGIN ?
		     "left corner" : "right corner");
	tp_log_debug("%s: max_y    = 0x%04X (%d)\n", __func__,
		     si->si_ofs.max_y, si->si_ofs.max_y);
	tp_log_debug("%s: y_origin = %d (%s)\n", __func__,
		     si->si_ofs.y_origin,
		     si->si_ofs.y_origin == CY_NORMAL_ORIGIN ?
		     "upper corner" : "lower corner");
	tp_log_debug("%s: max_p    = 0x%04X (%d)\n", __func__,
		     si->si_ofs.max_p, si->si_ofs.max_p);

	tp_log_debug("%s: xy_mode=%p xy_data=%p\n", __func__,
		     si->xy_mode, si->xy_data);
}

static int cyttsp4_get_sysinfo_regs(struct cyttsp4_core_data *cd)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	int rc;

	rc = cyttsp4_si_data_offsets(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp4_si_get_cydata(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp4_si_get_test_data(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp4_si_get_pcfg_data(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp4_si_get_opcfg_data(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp4_si_get_ddata(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp4_si_get_mdata(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp4_si_get_btn_data(cd);
	if (rc < 0)
		return rc;

	rc = cyttsp4_si_get_op_data_ptrs(cd);
	if (rc < 0) {
		tp_log_err("%s: failed to get_op_data\n", __func__);
		return rc;
	}

	cyttsp4_si_put_log_data(cd);

	/* provide flow control handshake */
	rc = cyttsp4_handshake(cd, si->si_data.hst_mode);
	if (rc < 0)
		tp_log_err("%s: handshake fail on sysinfo reg\n", __func__);

	mutex_lock(&cd->system_lock);
	si->ready = true;
	mutex_unlock(&cd->system_lock);
	return rc;
}

static void cyttsp4_queue_startup_(struct cyttsp4_core_data *cd)
{
	if (cd->startup_state == STARTUP_NONE) {
		cd->startup_state = STARTUP_QUEUED;
		schedule_work(&cd->startup_work);
		tp_log_info("%s: cyttsp4_startup queued\n", __func__);
	} else {
		tp_log_debug("%s: startup_state = %d\n", __func__,
			     cd->startup_state);
	}
}

static void cyttsp4_queue_startup(struct cyttsp4_core_data *cd)
{
	tp_log_debug("%s: enter\n", __func__);
	mutex_lock(&cd->system_lock);
	cyttsp4_queue_startup_(cd);
	mutex_unlock(&cd->system_lock);
}

static void call_atten_cb(struct cyttsp4_core_data *cd,
			  enum cyttsp4_atten_type type, int mode)
{
	struct atten_node *atten, *atten_n;

	tp_log_vdebug("%s: check list type=%d mode=%d\n", __func__, type, mode);
	spin_lock(&cd->spinlock);
	list_for_each_entry_safe(atten, atten_n, &cd->atten_list[type], node) {
		if (!mode || atten->mode & mode) {
			spin_unlock(&cd->spinlock);
			tp_log_vdebug("%s: attention for '%s'", __func__,
				      dev_name(&atten->ttsp->dev));
			atten->func(atten->ttsp);
			spin_lock(&cd->spinlock);
		}
	}
	spin_unlock(&cd->spinlock);
}


static irqreturn_t cyttsp4_hard_irq(int irq, void *handle)
{
	struct cyttsp4_core_data *cd = handle;

	/*
	 * Check whether this IRQ should be ignored (external)
	 * This should be the very first thing to check since
	 * ignore_irq may be set for a very short period of time
	 */
	if (atomic_read(&cd->ignore_irq)) {
		tp_log_debug("%s: Ignoring IRQ\n", __func__);
		return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

static irqreturn_t cyttsp4_irq(int irq, void *handle)
{
	struct cyttsp4_core_data *cd = handle;
	enum cyttsp4_mode cur_mode;
	u8 cmd_ofs = cd->sysinfo.si_ofs.cmd_ofs;
	bool command_complete = false;
	u8 mode[4] = { 0 };
	int rc;
	u8 cat_masked_cmd;

	tp_log_debug("%s int:0x%x\n", __func__, cd->int_status);

	mutex_lock(&cd->system_lock);

	rc = cyttsp4_adap_read(cd, CY_REG_BASE, mode, sizeof(mode));
	if (rc) {
		tp_log_err("%s: Fail read adapter r=%d\n", __func__, rc);
		goto cyttsp4_irq_exit;
	}
	tp_log_vdebug("%s mode[0-2]:0x%X 0x%X 0x%X 0x%X\n", __func__,
		      mode[0], mode[1], mode[2], mode[3]);

	if (IS_BOOTLOADER(mode[0], mode[1])) {
		cur_mode = CY_MODE_BOOTLOADER;
		tp_log_vdebug("%s: bl running\n", __func__);
		call_atten_cb(cd, CY_ATTEN_IRQ, cur_mode);

		/* switch to bootloader */
		if (cd->mode != CY_MODE_BOOTLOADER)
			tp_log_vdebug("%s: restart switch to bl m=%d -> m=%d\n",
				      __func__, cd->mode, cur_mode);

		/* catch operation->bl glitch */
		if (cd->mode != CY_MODE_BOOTLOADER
		    && cd->mode != CY_MODE_UNKNOWN) {
			/* Incase startup_state do not let startup_() */
			cd->mode = CY_MODE_UNKNOWN;
			cyttsp4_queue_startup_(cd);
			goto cyttsp4_irq_exit;
		}

		/* Recover if stuck in bootloader idle mode */
		if (cd->mode == CY_MODE_BOOTLOADER) {
			if (IS_BOOTLOADER_IDLE(mode[0], mode[1])) {
				if (cd->heartbeat_count > 3) {
					cd->heartbeat_count = 0;
					cyttsp4_queue_startup_(cd);
					goto cyttsp4_irq_exit;
				}
				cd->heartbeat_count++;
			}
		}

		cd->mode = cur_mode;
		/* Signal bootloader heartbeat heard */
		wake_up(&cd->wait_q);
		goto cyttsp4_irq_exit;
	}

	switch (mode[0] & CY_HST_DEVICE_MODE) {
	case CY_HST_OPERATE:
		cur_mode = CY_MODE_OPERATIONAL;
		tp_log_vdebug("%s: operational\n", __func__);
		break;
		/* Too much log,use vdebug to control it */
	case CY_HST_CAT:
		cur_mode = CY_MODE_CAT;
		/* set the start sensor mode state. */
		cat_masked_cmd = mode[2] & CY_CMD_MASK;

		/* Get the Debug info for the interrupt. */
		if (cat_masked_cmd != CY_CMD_CAT_NULL &&
		    cat_masked_cmd !=
		    CY_CMD_CAT_RETRIEVE_PANEL_SCAN &&
		    cat_masked_cmd != CY_CMD_CAT_EXEC_PANEL_SCAN)
			tp_log_vdebug("%s: cyttsp4_CaT_IRQ=%02X %02X %02X\n",
				      __func__, mode[0], mode[1], mode[2]);
		tp_log_vdebug("%s: CaT\n", __func__);
		break;
	case CY_HST_SYSINFO:
		cur_mode = CY_MODE_SYSINFO;
		tp_log_vdebug("%s: sysinfo\n", __func__);
		break;
	default:
		cur_mode = CY_MODE_UNKNOWN;
		tp_log_err("%s: unknown HST mode 0x%02X\n", __func__, mode[0]);
		break;
	}

	/* Check whether this IRQ should be ignored (internal) */
	if (cd->int_status & CY_INT_IGNORE) {
		if (IS_DEEP_SLEEP_CONFIGURED(cd->easy_wakeup_gesture)) {
			/* Put device back to sleep on premature wakeup */
			tp_log_debug("%s: Put device back to sleep\n",
				     __func__);
			_cyttsp4_put_device_into_deep_sleep(cd, mode[0]);
			goto cyttsp4_irq_exit;
		}
		/* Check for Wait for Event command */
		if ((mode[cmd_ofs] & CY_CMD_MASK) == CY_CMD_OP_WAIT_FOR_EVENT
		    && mode[cmd_ofs] & CY_CMD_COMPLETE) {
			tp_log_debug("%s: Wake up gesture detected!\n",
				     __func__);
			cd->wake_initiated_by_device = 1;
			cd->sysinfo.wakeup_event_id = mode[3];
			/*G750-C00 should not report EV_ABS event, so delete it */
			call_atten_cb(cd, CY_ATTEN_HOST_WAKE, 0);
			goto cyttsp4_irq_handshake;
		}
	}

	/* Check for wake up interrupt */
	if (cd->int_status & CY_INT_AWAKE) {
		cd->int_status &= ~CY_INT_AWAKE;
		wake_up(&cd->wait_q);
		tp_log_debug("%s: Received wake up interrupt\n", __func__);
		goto cyttsp4_irq_handshake;
	}

	/* Expecting mode change interrupt */
	if ((cd->int_status & CY_INT_MODE_CHANGE)
	    && (mode[0] & CY_HST_MODE_CHANGE) == 0) {
		cd->int_status &= ~CY_INT_MODE_CHANGE;
		tp_log_vdebug("%s: finish mode switch m=%d -> m=%d\n",
			      __func__, cd->mode, cur_mode);
		cd->mode = cur_mode;
		wake_up(&cd->wait_q);
		goto cyttsp4_irq_handshake;
	}

	/* compare current core mode to current device mode */
	tp_log_debug("%s: cd->mode=%d cur_mode=%d\n",
		     __func__, cd->mode, cur_mode);
	if ((mode[0] & CY_HST_MODE_CHANGE) == 0 && cd->mode != cur_mode) {
		/* Unexpected mode change occurred */
		tp_log_err("%s %d->%d 0x%x\n", __func__, cd->mode,
			   cur_mode, cd->int_status);
		tp_log_debug("%s: Unexpected mode change, startup\n", __func__);
		cyttsp4_queue_startup_(cd);
		goto cyttsp4_irq_exit;
	}

	/* Expecting command complete interrupt */
	tp_log_vdebug("%s: command byte:0x%x, toggle:0x%x\n",
		      __func__, mode[cmd_ofs], cd->cmd_toggle);
	if ((cd->int_status & CY_INT_EXEC_CMD)
	    && mode[cmd_ofs] & CY_CMD_COMPLETE) {
		command_complete = true;
		cd->int_status &= ~CY_INT_EXEC_CMD;
		tp_log_vdebug("%s: Received command complete interrupt\n",
			      __func__);
		wake_up(&cd->wait_q);
		/*
		 * It is possible to receive a single interrupt for
		 * command complete and touch/button status report.
		 * Continue processing for a possible status report.
		 */
	}

	/* Copy the mode registers */
	if (cd->sysinfo.xy_mode)
		memcpy(cd->sysinfo.xy_mode, mode, sizeof(mode));

	/* This should be status report, read status and touch regs */
	if (cd->mode == CY_MODE_OPERATIONAL) {
		tp_log_vdebug("%s: Read status and touch registers\n",
			      __func__);
		rc = cyttsp4_load_status_and_touch_regs(cd, !command_complete);
		if (rc < 0)
			tp_log_err("%s: fail read mode/touch regs r=%d\n",
				   __func__, rc);
	}

	/* attention IRQ */
	call_atten_cb(cd, CY_ATTEN_IRQ, cd->mode);

cyttsp4_irq_handshake:
	/* handshake the event */
	tp_log_vdebug("%s: Handshake mode=0x%02X r=%d\n",
		      __func__, mode[0], rc);
	rc = cyttsp4_handshake(cd, mode[0]);
	if (rc < 0)
		tp_log_err("%s: Fail handshake mode=0x%02X r=%d\n",
			   __func__, mode[0], rc);

	/*
	 * a non-zero udelay period is required for using
	 * IRQF_TRIGGER_LOW in order to delay until the
	 * device completes isr deassert
	 */
	udelay(cd->pdata->level_irq_udelay);

cyttsp4_irq_exit:
	mutex_unlock(&cd->system_lock);
	tp_log_debug("%s: irq done\n", __func__);
	return IRQ_HANDLED;
}


static void cyttsp4_start_wd_timer(struct cyttsp4_core_data *cd)
{
	if (!CY_WATCHDOG_TIMEOUT)
		return;

	mod_timer(&cd->watchdog_timer, jiffies +
		  msecs_to_jiffies(CY_WATCHDOG_TIMEOUT));
}

static void cyttsp4_stop_wd_timer(struct cyttsp4_core_data *cd)
{
	if (!CY_WATCHDOG_TIMEOUT)
		return;

	/*
	 * Ensure we wait until the watchdog timer
	 * running on a different CPU finishes
	 */
	del_timer_sync(&cd->watchdog_timer);
	cancel_work_sync(&cd->watchdog_work);
	del_timer_sync(&cd->watchdog_timer);
}

static void cyttsp4_watchdog_timer(unsigned long handle)
{
	struct cyttsp4_core_data *cd = (struct cyttsp4_core_data *)handle;

	tp_log_debug("%s: Timer triggered\n", __func__);

	if (!cd)
		return;

	if (!work_pending(&cd->watchdog_work))
		schedule_work(&cd->watchdog_work);

	return;
}

static int cyttsp4_write_(struct cyttsp4_device *ttsp, int mode, u16 addr,
			  const void *buf, int size)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	int rc = 0;

	mutex_lock(&cd->adap_lock);
	if (mode != cd->mode) {
		tp_log_debug("%s: %s (having %x while %x requested)\n",
			     __func__, "attempt to write in missing mode",
			     cd->mode, mode);
		rc = -EACCES;
		goto exit;
	}
	rc = cyttsp4_adap_write(cd, addr, buf, size);
exit:
	mutex_unlock(&cd->adap_lock);
	return rc;
}

static int cyttsp4_read_(struct cyttsp4_device *ttsp, int mode, u16 addr,
			 void *buf, int size)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	int rc = 0;

	mutex_lock(&cd->adap_lock);
	if (mode != cd->mode) {
		tp_log_debug("%s: %s (having %x while %x requested)\n",
			     __func__, "attempt to read in missing mode",
			     cd->mode, mode);
		rc = -EACCES;
		goto exit;
	}
	rc = cyttsp4_adap_read(cd, addr, buf, size);
exit:
	mutex_unlock(&cd->adap_lock);
	return rc;
}

static int cyttsp4_subscribe_attention_(struct cyttsp4_device *ttsp,
					enum cyttsp4_atten_type type,
					int (*func) (struct cyttsp4_device *),
					int mode)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	struct atten_node *atten, *atten_new;

	atten_new = kzalloc(sizeof(*atten_new), GFP_KERNEL);
	if (!atten_new) {
		tp_log_err("%s: Fail alloc atten node\n", __func__);
		return -ENOMEM;
	}

	tp_log_debug("%s from '%s'\n", __func__, dev_name(cd->dev));

	spin_lock(&cd->spinlock);
	list_for_each_entry(atten, &cd->atten_list[type], node) {
		if (atten->ttsp == ttsp && atten->mode == mode) {
			spin_unlock(&cd->spinlock);
			kfree(atten_new);
			tp_log_debug("%s: %s=%p %s=%d\n",
				     __func__,
				     "already subscribed attention",
				     ttsp, "mode", mode);

			return 0;
		}
	}

	atten_new->ttsp = ttsp;
	atten_new->mode = mode;
	atten_new->func = func;

	list_add(&atten_new->node, &cd->atten_list[type]);
	spin_unlock(&cd->spinlock);

	return 0;
}

static int cyttsp4_unsubscribe_attention_(struct cyttsp4_device *ttsp,
					  enum cyttsp4_atten_type type,
					  int (*func) (struct cyttsp4_device *),
					  int mode)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	struct atten_node *atten, *atten_n;

	spin_lock(&cd->spinlock);
	list_for_each_entry_safe(atten, atten_n, &cd->atten_list[type], node) {
		if (atten->ttsp == ttsp && atten->mode == mode) {
			list_del(&atten->node);
			spin_unlock(&cd->spinlock);
			kfree(atten);
			tp_log_debug("%s: %s=%p %s=%d\n",
				     __func__,
				     "unsub for atten->ttsp", atten->ttsp,
				     "atten->mode", atten->mode);
			return 0;
		}
	}
	spin_unlock(&cd->spinlock);

	return -ENODEV;
}

static int request_exclusive(struct cyttsp4_core_data *cd, void *ownptr,
			     int timeout_ms)
{
	int t = msecs_to_jiffies(timeout_ms);
	bool with_timeout = (timeout_ms != 0);

	mutex_lock(&cd->system_lock);
	if (!cd->exclusive_dev && cd->exclusive_waits == 0) {
		cd->exclusive_dev = ownptr;
		goto exit;
	}

	cd->exclusive_waits++;
wait:
	mutex_unlock(&cd->system_lock);
	if (with_timeout) {
		t = wait_event_timeout(cd->wait_q, !cd->exclusive_dev, t);
		if (IS_TMO(t)) {
			tp_log_err("%s: tmo waiting exclusive access\n",
				   __func__);
			mutex_lock(&cd->system_lock);
			cd->exclusive_waits--;
			mutex_unlock(&cd->system_lock);
			return -ETIME;
		}
	} else {
		wait_event(cd->wait_q, !cd->exclusive_dev);
	}
	mutex_lock(&cd->system_lock);
	if (cd->exclusive_dev)
		goto wait;
	cd->exclusive_dev = ownptr;
	cd->exclusive_waits--;
exit:
	mutex_unlock(&cd->system_lock);
	tp_log_debug("%s: request_exclusive ok=%p\n", __func__, ownptr);

	return 0;
}

static int cyttsp4_request_exclusive_(struct cyttsp4_device *ttsp,
				      int timeout_ms)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	return request_exclusive(cd, (void *)ttsp, timeout_ms);
}

/*
 * returns error if was not owned
 */
static int release_exclusive(struct cyttsp4_core_data *cd, void *ownptr)
{
	mutex_lock(&cd->system_lock);
	if (cd->exclusive_dev != ownptr) {
		mutex_unlock(&cd->system_lock);
		return -EINVAL;
	}

	tp_log_debug("%s: exclusive_dev %p freed\n",
		     __func__, cd->exclusive_dev);
	cd->exclusive_dev = NULL;
	wake_up(&cd->wait_q);
	mutex_unlock(&cd->system_lock);
	return 0;
}

static int cyttsp4_release_exclusive_(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	return release_exclusive(cd, (void *)ttsp);
}

static int cyttsp4_reset_checkout(struct cyttsp4_core_data *cd)
{
	int rc;
	int i;
	u8 buf;

	/* reset hardware */
	mutex_lock(&cd->system_lock);
	tp_log_debug("%s: reset hw...\n", __func__);
	rc = cyttsp4_hw_reset_(cd);
	cd->mode = CY_MODE_UNKNOWN;
	mutex_unlock(&cd->system_lock);
	if (rc < 0) {
		tp_log_err("%s: %s adap='%s' r=%d\n", __func__,
			   "Fail hw reset", cd->core->adap->id, rc);
		return rc;
	}

	tp_log_debug("%s: msleep 70 ms\n", __func__);
	msleep(70);
	tp_log_debug("%s: begin to check cypress device\n", __func__);
	for (i = 0; i < CY_CORE_READTIMES; i++) {
		rc = cyttsp4_adap_read(cd, CY_REG_BASE, &buf, sizeof(buf));
		tp_log_debug("%s: rc =%d\n", __func__, rc);
		if (rc < 0) {
			tp_log_err
			    ("%s:I2C cannot communicate, no cypress device, exit!!!\n",
			     __func__);
		} else {
			tp_log_debug("%s: find cypress device!!!\n", __func__);
			return rc;
		}
	}
	return rc;
}


static int cyttsp4_wait_bl_heartbeat(struct cyttsp4_core_data *cd)
{
	long t;
	int rc = 0;

	/* wait heartbeat */
	tp_log_debug("%s: wait heartbeat...\n", __func__);
	t = wait_event_timeout(cd->wait_q, cd->mode == CY_MODE_BOOTLOADER,
			       msecs_to_jiffies
			       (CY_CORE_RESET_AND_WAIT_TIMEOUT));
	if (IS_TMO(t)) {
		tp_log_err("%s: tmo waiting bl heartbeat cd->mode=%d\n",
			   __func__, cd->mode);
		rc = -ETIME;
	}

	return rc;
}

static int cyttsp4_wait_sysinfo_mode(struct cyttsp4_core_data *cd)
{
	long t;

	tp_log_debug("%s: wait sysinfo...\n", __func__);

	t = wait_event_timeout(cd->wait_q, cd->mode == CY_MODE_SYSINFO,
			       msecs_to_jiffies
			       (CY_CORE_WAIT_SYSINFO_MODE_TIMEOUT));
	if (IS_TMO(t)) {
		tp_log_err("%s: tmo waiting exit bl cd->mode=%d\n",
			   __func__, cd->mode);
		mutex_lock(&cd->system_lock);
		cd->int_status &= ~CY_INT_MODE_CHANGE;
		mutex_unlock(&cd->system_lock);
		return -ETIME;
	}

	return 0;
}

static int cyttsp4_reset_and_wait(struct cyttsp4_core_data *cd)
{
	int rc;

	/* reset hardware */
	mutex_lock(&cd->system_lock);
	tp_log_debug("%s: reset hw...\n", __func__);
	rc = cyttsp4_hw_reset_(cd);
	cd->mode = CY_MODE_UNKNOWN;
	mutex_unlock(&cd->system_lock);
	if (rc < 0) {
		tp_log_err("%s: %s adap='%s' r=%d\n", __func__,
			   "Fail hw reset", cd->core->adap->id, rc);
		return rc;
	}

	return cyttsp4_wait_bl_heartbeat(cd);
}

/*
 * returns err if refused or timeout; block until mode change complete
 * bit is set (mode change interrupt)
 */
static int set_mode(struct cyttsp4_core_data *cd, int new_mode)
{
	u8 new_dev_mode;
	u8 mode = 0;
	long t;
	int rc;

	switch (new_mode) {
	case CY_MODE_OPERATIONAL:
		new_dev_mode = CY_HST_OPERATE;
		break;
	case CY_MODE_SYSINFO:
		new_dev_mode = CY_HST_SYSINFO;
		break;
	case CY_MODE_CAT:
		new_dev_mode = CY_HST_CAT;
		break;
	default:
		tp_log_err("%s: invalid mode: %02X(%d)\n",
			   __func__, new_mode, new_mode);
		return -EINVAL;
	}

	/* change mode */
	tp_log_debug("%s: %s=%p new_dev_mode=%02X new_mode=%d\n",
		     __func__, "have exclusive", cd->exclusive_dev,
		     new_dev_mode, new_mode);

	mutex_lock(&cd->system_lock);
	rc = cyttsp4_adap_read(cd, CY_REG_BASE, &mode, sizeof(mode));
	if (rc < 0) {
		mutex_unlock(&cd->system_lock);
		tp_log_err("%s: Fail read mode r=%d\n", __func__, rc);
		goto exit;
	}

	/* Clear device mode bits and set to new mode */
	mode &= ~CY_HST_DEVICE_MODE;
	mode |= new_dev_mode | CY_HST_MODE_CHANGE;

	cd->int_status |= CY_INT_MODE_CHANGE;
	rc = cyttsp4_adap_write(cd, CY_REG_BASE, &mode, sizeof(mode));
	mutex_unlock(&cd->system_lock);
	if (rc < 0) {
		tp_log_err("%s: Fail write mode change r=%d\n", __func__, rc);
		goto exit;
	}

	/* wait for mode change done interrupt */
	t = wait_event_timeout(cd->wait_q,
			       (cd->int_status & CY_INT_MODE_CHANGE) == 0,
			       msecs_to_jiffies(CY_CORE_MODE_CHANGE_TIMEOUT));
	tp_log_debug("%s: back from wait t=%ld cd->mode=%d\n",
		     __func__, t, cd->mode);

	if (IS_TMO(t)) {
		tp_log_err("%s: %s\n", __func__, "tmo waiting mode change");
		mutex_lock(&cd->system_lock);
		cd->int_status &= ~CY_INT_MODE_CHANGE;
		mutex_unlock(&cd->system_lock);
		rc = -EINVAL;
	}

exit:
	return rc;
}

/*
 * returns err if refused or timeout(core uses fixed timeout period) occurs;
 * blocks until ISR occurs
 */
static int cyttsp4_request_reset_(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	int rc;

	mutex_lock(&cd->system_lock);
	cd->sysinfo.ready = false;
	mutex_unlock(&cd->system_lock);

	rc = cyttsp4_reset_and_wait(cd);
	if (rc < 0)
		tp_log_err("%s: Error on h/w reset r=%d\n", __func__, rc);

	return rc;
}

/*
 * returns err if refused ; if no error then restart has completed
 * and system is in normal operating mode
 */
static int cyttsp4_request_restart_(struct cyttsp4_device *ttsp, bool wait)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);

	mutex_lock(&cd->system_lock);
	cd->sysinfo.ready = false;
	mutex_unlock(&cd->system_lock);

	cyttsp4_queue_startup(cd);

	if (wait)
		wait_event(cd->wait_q, cd->startup_state == STARTUP_NONE);

	return 0;
}

static int cyttsp4_request_set_mode_(struct cyttsp4_device *ttsp, int mode)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	int rc;

	rc = set_mode(cd, mode);
	if (rc < 0)
		tp_log_err("%s: fail set_mode=%02X(%d)\n",
			   __func__, cd->mode, cd->mode);

	return rc;
}

/*
 * returns NULL if sysinfo has not been acquired from the device yet
 */
static struct cyttsp4_sysinfo *cyttsp4_request_sysinfo_(struct cyttsp4_device
							*ttsp)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	bool ready;

	mutex_lock(&cd->system_lock);
	ready = cd->sysinfo.ready;
	mutex_unlock(&cd->system_lock);
	if (ready)
		return &cd->sysinfo;

	return NULL;
}

static struct cyttsp4_loader_platform_data *cyttsp4_request_loader_pdata_(struct
									  cyttsp4_device
									  *ttsp)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	return cd->pdata->loader_pdata;
}

static int cyttsp4_request_handshake_(struct cyttsp4_device *ttsp, u8 mode)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	int rc;

	rc = cyttsp4_handshake(cd, mode);
	if (rc < 0)
		tp_log_err("%s: Fail handshake r=%d\n", __func__, rc);

	return rc;
}

static int cyttsp4_request_toggle_lowpower_(struct cyttsp4_device *ttsp,
					    u8 mode)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	int rc = cyttsp4_toggle_low_power(cd, mode);
	if (rc < 0)
		tp_log_err("%s: Fail toggle low power r=%d\n", __func__, rc);
	return rc;
}

static int cyttsp4_request_power_state_(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	int rc = cyttsp4_power_state(cd);
	if (rc == 0)
		tp_log_debug("%s: device is in sleeping state r=%d\n",
			     __func__, rc);
	return rc;
}

static int _cyttsp4_wait_cmd_exec(struct cyttsp4_core_data *cd, int timeout_ms)
{
	int rc;

	rc = wait_event_timeout(cd->wait_q,
				(cd->int_status & CY_INT_EXEC_CMD) == 0,
				msecs_to_jiffies(timeout_ms));
	if (IS_TMO(rc)) {
		tp_log_err("%s: Command execution timed out\n", __func__);
		cd->int_status &= ~CY_INT_EXEC_CMD;
		return -ETIME;
	}
	return 0;
}

static int _get_cmd_offs(struct cyttsp4_core_data *cd, u8 mode)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	int cmd_ofs;

	switch (mode) {
	case CY_MODE_CAT:
		cmd_ofs = CY_REG_CAT_CMD;
		break;
	case CY_MODE_OPERATIONAL:
		cmd_ofs = si->si_ofs.cmd_ofs;
		break;
	default:
		tp_log_err("%s: Unsupported mode %x for exec cmd\n",
			   __func__, mode);
		return -EACCES;
	}

	return cmd_ofs;
}

/*
 * Send command to device for CAT and OP modes
 * return negative value on error, 0 on success
 */
static int _cyttsp4_exec_cmd(struct cyttsp4_core_data *cd, u8 mode,
			     u8 * cmd_buf, u32 cmd_size)
{
	int cmd_ofs;
	int cmd_param_ofs;
	u8 command;
	u8 *cmd_param_buf;
	u32 cmd_param_size;
	int rc;

	if (mode != cd->mode) {
		tp_log_err("%s: %s (having %x while %x requested)\n",
			   __func__, "attempt to exec cmd in missing mode",
			   cd->mode, mode);
		return -EACCES;
	}

	cmd_ofs = _get_cmd_offs(cd, mode);
	if (cmd_ofs < 0)
		return -EACCES;

	cmd_param_ofs = cmd_ofs + 1;
	cmd_param_buf = cmd_buf + 1;
	cmd_param_size = cmd_size - 1;

	/* Check if complete is set, so write new command */
	rc = cyttsp4_adap_read(cd, cmd_ofs, &command, 1);
	if (rc < 0) {
		tp_log_err("%s: Error on read r=%d\n", __func__, rc);
		return rc;
	}

	cd->cmd_toggle = GET_TOGGLE(command);
	cd->int_status |= CY_INT_EXEC_CMD;

	if ((command & CY_CMD_COMPLETE_MASK) == 0)
		return -EBUSY;

	/*
	 * Write new command
	 * Only update command bits 0:5
	 * Clear command complete bit & toggle bit
	 */
	cmd_buf[0] = cmd_buf[0] & CY_CMD_MASK;
	/* Write command parameters first */
	if (cmd_size > 1) {
		rc = cyttsp4_adap_write(cd, cmd_param_ofs, cmd_param_buf,
					cmd_param_size);
		if (rc < 0) {
			tp_log_err
			    ("%s: Error on write command parameters r=%d\n",
			     __func__, rc);
			return rc;
		}
	}
	/* Write the command */
	rc = cyttsp4_adap_write(cd, cmd_ofs, cmd_buf, 1);
	if (rc < 0) {
		tp_log_err("%s: Error on write command r=%d\n", __func__, rc);
		return rc;
	}

	return 0;
}

static int cyttsp4_exec_cmd(struct cyttsp4_core_data *cd, u8 mode,
			    u8 * cmd_buf, u32 cmd_size, u8 * return_buf,
			    u32 return_buf_size, int timeout_ms)
{
	int cmd_ofs;
	int cmd_return_ofs;
	int rc;

	mutex_lock(&cd->system_lock);
	rc = _cyttsp4_exec_cmd(cd, mode, cmd_buf, cmd_size);
	mutex_unlock(&cd->system_lock);

	if (rc == -EBUSY) {
		rc = _cyttsp4_wait_cmd_exec(cd, CY_COMMAND_COMPLETE_TIMEOUT);
		if (rc)
			return rc;
		mutex_lock(&cd->system_lock);
		rc = _cyttsp4_exec_cmd(cd, mode, cmd_buf, cmd_size);
		mutex_unlock(&cd->system_lock);
	}

	if (rc < 0)
		return rc;

	if (timeout_ms == 0)
		return 0;

	/*
	 * Wait command to be completed
	 */
	rc = _cyttsp4_wait_cmd_exec(cd, timeout_ms);
	if (rc < 0)
		return rc;

	if (return_buf_size == 0 || return_buf == NULL)
		return 0;

	mutex_lock(&cd->system_lock);
	cmd_ofs = _get_cmd_offs(cd, mode);
	mutex_unlock(&cd->system_lock);
	if (cmd_ofs < 0)
		return -EACCES;

	cmd_return_ofs = cmd_ofs + 1;

	rc = cyttsp4_adap_read(cd, cmd_return_ofs, return_buf, return_buf_size);
	if (rc < 0) {
		tp_log_err("%s: Error on read 3 r=%d\n", __func__, rc);
		return rc;
	}

	return 0;
}

static int cyttsp4_request_exec_cmd_(struct cyttsp4_device *ttsp, u8 mode,
				     u8 * cmd_buf, u32 cmd_size,
				     u8 * return_buf, u32 return_buf_size,
				     int timeout_ms)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	return cyttsp4_exec_cmd(cd, mode, cmd_buf, cmd_size,
				return_buf, return_buf_size, timeout_ms);
}

static int cyttsp4_get_parameter(struct cyttsp4_core_data *cd, u8 param_id,
				 u32 * param_value)
{
	u8 command_buf[CY_CMD_OP_GET_PARAM_CMD_SZ];
	u8 return_buf[CY_CMD_OP_GET_PARAM_RET_SZ];
	u8 param_size;
	u8 *value_buf;
	int rc;

	command_buf[0] = CY_CMD_OP_GET_PARA;
	command_buf[1] = param_id;
	rc = cyttsp4_exec_cmd(cd, CY_MODE_OPERATIONAL,
			      command_buf, CY_CMD_OP_GET_PARAM_CMD_SZ,
			      return_buf, CY_CMD_OP_GET_PARAM_RET_SZ,
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: Unable to execute get parameter command.\n",
			   __func__);
		return rc;
	}

	if (return_buf[0] != param_id) {
		tp_log_err("%s: Fail to execute get parameter command.\n",
			   __func__);
		return -EIO;
	}

	param_size = return_buf[1];
	value_buf = &return_buf[2];

	*param_value = 0;
	while (param_size--)
		*param_value += *(value_buf++) << (8 * param_size);

	return 0;
}

static int cyttsp4_set_parameter(struct cyttsp4_core_data *cd, u8 param_id,
				 u8 param_size, u32 param_value)
{
	u8 command_buf[CY_CMD_OP_SET_PARAM_CMD_SZ];
	u8 return_buf[CY_CMD_OP_SET_PARAM_RET_SZ];
	int rc;

	command_buf[0] = CY_CMD_OP_SET_PARA;
	command_buf[1] = param_id;
	command_buf[2] = param_size;

	if (param_size == 1) {
		command_buf[3] = (u8) param_value;
	} else if (param_size == 2) {
		command_buf[3] = (u8) (param_value >> 8);
		command_buf[4] = (u8) param_value;
	} else if (param_size == 4) {
		command_buf[3] = (u8) (param_value >> 24);
		command_buf[4] = (u8) (param_value >> 16);
		command_buf[5] = (u8) (param_value >> 8);
		command_buf[6] = (u8) param_value;
	} else {
		tp_log_err("%s: Invalid parameter size %d\n",
			   __func__, param_size);
		return -EINVAL;
	}

	rc = cyttsp4_exec_cmd(cd, CY_MODE_OPERATIONAL,
			      command_buf, 3 + param_size,
			      return_buf, CY_CMD_OP_SET_PARAM_RET_SZ,
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: Unable to execute set parameter command.\n",
			   __func__);
		return rc;
	}

	if (return_buf[0] != param_id || return_buf[1] != param_size) {
		tp_log_err("%s: Fail to execute set parameter command.\n",
			   __func__);
		return -EIO;
	}

	return 0;
}

static int cyttsp4_get_scantype(struct cyttsp4_core_data *cd, u8 * scantype)
{
	int rc;
	u32 value;

	rc = cyttsp4_get_parameter(cd, CY_RAM_ID_SCAN_TYPE, &value);
	if (!rc)
		*scantype = (u8) value;

	return rc;
}

static int cyttsp4_set_scantype(struct cyttsp4_core_data *cd, u8 scantype)
{
	int rc;

	rc = cyttsp4_set_parameter(cd, CY_RAM_ID_SCAN_TYPE, 1, scantype);

	return rc;
}

static u8 _cyttsp4_generate_new_scantype(struct cyttsp4_core_data *cd)
{
	u8 new_scantype = cd->default_scantype;

	if (cd->apa_mc_en)
		new_scantype |= CY_SCAN_TYPE_APA_MC;
	if (cd->glove_en)
		new_scantype |= CY_SCAN_TYPE_GLOVE;
	if (cd->stylus_en)
		new_scantype |= CY_SCAN_TYPE_STYLUS;
	if (cd->proximity_en)
		new_scantype |= CY_SCAN_TYPE_PROXIMITY;

	return new_scantype;
}

static int cyttsp4_set_new_scan_type(struct cyttsp4_core_data *cd,
				     u8 scan_type, bool enable)
{
	int inc = enable ? 1 : -1;
	int *en;
	int rc;
	u8 new_scantype;

	switch (scan_type) {
	case CY_ST_GLOVE:
		en = &cd->glove_en;
		break;
	case CY_ST_STYLUS:
		en = &cd->stylus_en;
		break;
	case CY_ST_PROXIMITY:
		en = &cd->proximity_en;
		break;
	case CY_ST_APA_MC:
		en = &cd->apa_mc_en;
		break;
	default:
		return -EINVAL;
	}

	*en += inc;

	new_scantype = _cyttsp4_generate_new_scantype(cd);

	rc = cyttsp4_set_scantype(cd, new_scantype);
	if (rc)
		*en -= inc;

	return rc;
}

static int cyttsp4_request_enable_scan_type_(struct cyttsp4_device *ttsp,
					     u8 scan_type)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);

	return cyttsp4_set_new_scan_type(cd, scan_type, true);
}

static int cyttsp4_request_disable_scan_type_(struct cyttsp4_device *ttsp,
					      u8 scan_type)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);

	return cyttsp4_set_new_scan_type(cd, scan_type, false);
}

static int cyttsp4_read_config_block(struct cyttsp4_core_data *cd, u8 ebid,
				     u16 row, u8 * data, u16 length)
{
	u8 command_buf[CY_CMD_CAT_READ_CFG_BLK_CMD_SZ];
	u8 *return_buf;
	int return_buf_sz;
	u16 crc;
	int rc;

	/* Allocate buffer for read config block command response
	 * Header(5) + Data(length) + CRC(2)
	 */
	return_buf_sz = CY_CMD_CAT_READ_CFG_BLK_RET_SZ + length;
	return_buf = kmalloc(return_buf_sz, GFP_KERNEL);
	if (!return_buf) {
		tp_log_err("%s: Cannot allocate buffer\n", __func__);
		rc = -ENOMEM;
		goto exit;
	}

	command_buf[0] = CY_CMD_CAT_READ_CFG_BLK;
	command_buf[1] = HI_BYTE(row);
	command_buf[2] = LO_BYTE(row);
	command_buf[3] = HI_BYTE(length);
	command_buf[4] = LO_BYTE(length);
	command_buf[5] = ebid;

	rc = cyttsp4_exec_cmd(cd, CY_MODE_CAT,
			      command_buf, CY_CMD_CAT_READ_CFG_BLK_CMD_SZ,
			      return_buf, return_buf_sz,
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc) {
		tp_log_err("%s: Error executing command r=%d\n", __func__, rc);
		goto free_buffer;
	}

	crc =
	    cyttsp4_calc_app_crc(&return_buf
				 [CY_CMD_CAT_READ_CFG_BLK_RET_HDR_SZ], length);

	/* Validate response */
	if (return_buf[0] != CY_CMD_STATUS_SUCCESS
	    || return_buf[1] != ebid || return_buf[2] != HI_BYTE(length)
	    || return_buf[3] != LO_BYTE(length)
	    || return_buf[CY_CMD_CAT_READ_CFG_BLK_RET_HDR_SZ
			  + length] != HI_BYTE(crc)
	    || return_buf[CY_CMD_CAT_READ_CFG_BLK_RET_HDR_SZ
			  + length + 1] != LO_BYTE(crc)) {
		tp_log_err("%s: Fail executing command\n", __func__);
		rc = -EINVAL;
		goto free_buffer;
	}

	memcpy(data, &return_buf[CY_CMD_CAT_READ_CFG_BLK_RET_HDR_SZ], length);

	cyttsp4_pr_buf(cd->dev, cd->pr_buf, data, length, "read_config_block");

free_buffer:
	kfree(return_buf);
exit:
	return rc;
}

static int cyttsp4_write_config_block(struct cyttsp4_core_data *cd, u8 ebid,
				      u16 row, const u8 * data, u16 length)
{
	u8 return_buf[CY_CMD_CAT_WRITE_CFG_BLK_RET_SZ];
	u8 *command_buf;
	int command_buf_sz;
	u16 crc;
	int rc;

	/* Allocate buffer for write config block command
	 * Header(6) + Data(length) + Security Key(8) + CRC(2)
	 */
	command_buf_sz = CY_CMD_CAT_WRITE_CFG_BLK_CMD_SZ + length
	    + sizeof(security_key);
	command_buf = kmalloc(command_buf_sz, GFP_KERNEL);
	if (!command_buf) {
		tp_log_err("%s: Cannot allocate buffer\n", __func__);
		rc = -ENOMEM;
		goto exit;
	}

	crc = cyttsp4_calc_app_crc(data, length);

	command_buf[0] = CY_CMD_CAT_WRITE_CFG_BLK;
	command_buf[1] = HI_BYTE(row);
	command_buf[2] = LO_BYTE(row);
	command_buf[3] = HI_BYTE(length);
	command_buf[4] = LO_BYTE(length);
	command_buf[5] = ebid;

	command_buf[CY_CMD_CAT_WRITE_CFG_BLK_CMD_HDR_SZ + length
		    + sizeof(security_key)] = HI_BYTE(crc);
	command_buf[CY_CMD_CAT_WRITE_CFG_BLK_CMD_HDR_SZ + 1 + length
		    + sizeof(security_key)] = LO_BYTE(crc);

	memcpy(&command_buf[CY_CMD_CAT_WRITE_CFG_BLK_CMD_HDR_SZ], data, length);
	memcpy(&command_buf[CY_CMD_CAT_WRITE_CFG_BLK_CMD_HDR_SZ + length],
	       security_key, sizeof(security_key));

	cyttsp4_pr_buf(cd->dev, cd->pr_buf, command_buf, command_buf_sz,
		       "write_config_block");

	rc = cyttsp4_exec_cmd(cd, CY_MODE_CAT,
			      command_buf, command_buf_sz,
			      return_buf, CY_CMD_CAT_WRITE_CFG_BLK_RET_SZ,
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc) {
		tp_log_err("%s: Error executing command r=%d\n", __func__, rc);
		goto free_buffer;
	}

	/* Validate response */
	if (return_buf[0] != CY_CMD_STATUS_SUCCESS
	    || return_buf[1] != ebid || return_buf[2] != HI_BYTE(length)
	    || return_buf[3] != LO_BYTE(length)) {
		tp_log_err("%s: Fail executing command\n", __func__);
		rc = -EINVAL;
		goto free_buffer;
	}

free_buffer:
	kfree(command_buf);
exit:
	return rc;
}

static int cyttsp4_get_config_row_size(struct cyttsp4_core_data *cd,
				       u16 * config_row_size)
{
	u8 command_buf[CY_CMD_CAT_GET_CFG_ROW_SIZE_CMD_SZ];
	u8 return_buf[CY_CMD_CAT_GET_CFG_ROW_SIZE_RET_SZ];
	int rc;

	command_buf[0] = CY_CMD_CAT_GET_CFG_ROW_SZ;

	rc = cyttsp4_exec_cmd(cd, CY_MODE_CAT,
			      command_buf, CY_CMD_CAT_GET_CFG_ROW_SIZE_CMD_SZ,
			      return_buf, CY_CMD_CAT_GET_CFG_ROW_SIZE_RET_SZ,
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc) {
		tp_log_err("%s: Error executing command r=%d\n", __func__, rc);
		goto exit;
	}

	*config_row_size = get_unaligned_be16(&return_buf[0]);

exit:
	return rc;
}

static int cyttsp4_request_config_row_size_(struct cyttsp4_device *ttsp,
					    u16 * config_row_size)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);

	return cyttsp4_get_config_row_size(cd, config_row_size);
}

static int cyttsp4_verify_config_block_crc(struct cyttsp4_core_data *cd,
					   u8 ebid, u16 * calc_crc,
					   u16 * stored_crc, bool * match)
{
	u8 command_buf[CY_CMD_CAT_VERIFY_CFG_BLK_CRC_CMD_SZ];
	u8 return_buf[CY_CMD_CAT_VERIFY_CFG_BLK_CRC_RET_SZ];
	int rc;

	command_buf[0] = CY_CMD_CAT_VERIFY_CFG_BLK_CRC;
	command_buf[1] = ebid;

	rc = cyttsp4_exec_cmd(cd, CY_MODE_CAT,
			      command_buf, CY_CMD_CAT_VERIFY_CFG_BLK_CRC_CMD_SZ,
			      return_buf, CY_CMD_CAT_VERIFY_CFG_BLK_CRC_RET_SZ,
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc) {
		tp_log_err("%s: Error executing command r=%d\n", __func__, rc);
		goto exit;
	}

	*calc_crc = get_unaligned_be16(&return_buf[1]);
	*stored_crc = get_unaligned_be16(&return_buf[3]);
	if (match)
		*match = !return_buf[0];
exit:
	return rc;
}

static int cyttsp4_get_config_block_crc(struct cyttsp4_core_data *cd,
					u8 ebid, u16 * crc)
{
	u8 command_buf[CY_CMD_OP_GET_CFG_BLK_CRC_CMD_SZ];
	u8 return_buf[CY_CMD_OP_GET_CFG_BLK_CRC_RET_SZ];
	int rc;

	command_buf[0] = CY_CMD_OP_GET_CRC;
	command_buf[1] = ebid;

	rc = cyttsp4_exec_cmd(cd, CY_MODE_OPERATIONAL,
			      command_buf, CY_CMD_OP_GET_CFG_BLK_CRC_CMD_SZ,
			      return_buf, CY_CMD_OP_GET_CFG_BLK_CRC_RET_SZ,
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc) {
		tp_log_err("%s: Error executing command r=%d\n", __func__, rc);
		goto exit;
	}

	/* Validate response */
	if (return_buf[0] != CY_CMD_STATUS_SUCCESS) {
		tp_log_err("%s: Fail executing command\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	*crc = get_unaligned_be16(&return_buf[1]);

exit:
	return rc;
}

static int cyttsp4_get_ttconfig_version(struct cyttsp4_core_data *cd,
					u16 * version)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	u8 data[CY_TTCONFIG_VERSION_OFFSET + CY_TTCONFIG_VERSION_SIZE];
	int rc;
	bool ready;

	mutex_lock(&cd->system_lock);
	ready = si->ready;
	mutex_unlock(&cd->system_lock);

	if (!ready) {
		rc = -ENODEV;
		goto exit;
	}

	rc = cyttsp4_read_config_block(cd, CY_TCH_PARM_EBID,
				       CY_TTCONFIG_VERSION_ROW, data,
				       sizeof(data));
	if (rc) {
		tp_log_err("%s: Error on read config block\n", __func__);
		goto exit;
	}

	*version = GET_FIELD16(si, &data[CY_TTCONFIG_VERSION_OFFSET]);

exit:
	return rc;
}

static int cyttsp4_get_config_length(struct cyttsp4_core_data *cd, u8 ebid,
				     u16 * length, u16 * max_length)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	u8 data[CY_CONFIG_LENGTH_INFO_SIZE];
	int rc;
	bool ready;

	mutex_lock(&cd->system_lock);
	ready = si->ready;
	mutex_unlock(&cd->system_lock);

	if (!ready) {
		rc = -ENODEV;
		goto exit;
	}

	rc = cyttsp4_read_config_block(cd, ebid, CY_CONFIG_LENGTH_INFO_OFFSET,
				       data, sizeof(data));
	if (rc) {
		tp_log_err("%s: Error on read config block\n", __func__);
		goto exit;
	}

	*length = GET_FIELD16(si, &data[CY_CONFIG_LENGTH_OFFSET]);
	*max_length = GET_FIELD16(si, &data[CY_CONFIG_MAXLENGTH_OFFSET]);

exit:
	return rc;
}

static int cyttsp4_write_config_common(struct cyttsp4_core_data *cd, u8 ebid,
				       u16 offset, u8 * data, u16 length)
{
	u16 cur_block, cur_off, end_block, end_off;
	int copy_len;
	u16 config_row_size = 0;
	u8 *row_data = NULL;
	int rc;

	rc = cyttsp4_get_config_row_size(cd, &config_row_size);
	if (rc) {
		tp_log_err("%s: Cannot get config row size\n", __func__);
		goto exit;
	}

	cur_block = offset / config_row_size;
	cur_off = offset % config_row_size;

	end_block = (offset + length) / config_row_size;
	end_off = (offset + length) % config_row_size;

	/* Check whether we need to fetch the whole block first */
	if (cur_off == 0)
		goto no_offset;

	row_data = kmalloc(config_row_size, GFP_KERNEL);
	if (!row_data) {
		tp_log_err("%s: Cannot allocate buffer\n", __func__);
		rc = -ENOMEM;
		goto exit;
	}

	copy_len = (cur_block == end_block) ?
	    length : config_row_size - cur_off;

	/* Read up to current offset, append the new data and write it back */
	rc = cyttsp4_read_config_block(cd, ebid, cur_block, row_data, cur_off);
	if (rc) {
		tp_log_err("%s: Error on read config block\n", __func__);
		goto free_row_data;
	}

	memcpy(&row_data[cur_off], data, copy_len);

	rc = cyttsp4_write_config_block(cd, ebid, cur_block, row_data,
					cur_off + copy_len);
	if (rc) {
		tp_log_err("%s: Error on initial write config block\n",
			   __func__);
		goto free_row_data;
	}

	data += copy_len;
	cur_off = 0;
	cur_block++;

no_offset:
	while (cur_block < end_block) {
		rc = cyttsp4_write_config_block(cd, ebid, cur_block, data,
						config_row_size);
		if (rc) {
			tp_log_err("%s: Error on write config block\n",
				   __func__);
			goto free_row_data;
		}

		data += config_row_size;
		cur_block++;
	}

	/* Last block */
	if (cur_block == end_block) {
		rc = cyttsp4_write_config_block(cd, ebid, end_block, data,
						end_off);
		if (rc) {
			tp_log_err("%s: Error on last write config block\n",
				   __func__);
			goto free_row_data;
		}
	}

free_row_data:
	kfree(row_data);
exit:
	return rc;
}

static int cyttsp4_write_config(struct cyttsp4_core_data *cd, u8 ebid,
				u16 offset, u8 * data, u16 length)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	u16 crc_new, crc_old;
	u16 crc_offset;
	u16 conf_len;
	u8 crc_data[2];
	int rc;
	bool ready;

	mutex_lock(&cd->system_lock);
	ready = si->ready;
	mutex_unlock(&cd->system_lock);

	if (!ready) {
		rc = -ENODEV;
		goto exit;
	}

	/* CRC is stored at config max length offset */
	rc = cyttsp4_get_config_length(cd, ebid, &conf_len, &crc_offset);
	if (rc) {
		tp_log_err("%s: Error on get config length\n", __func__);
		goto exit;
	}

	/* Allow CRC update also */
	if (offset + length > crc_offset + 2) {
		tp_log_err("%s: offset + length exceeds max length(%d)\n",
			   __func__, crc_offset + 2);
		rc = -EINVAL;
		goto exit;
	}

	rc = cyttsp4_write_config_common(cd, ebid, offset, data, length);
	if (rc) {
		tp_log_err("%s: Error on write config\n", __func__);
		goto exit;
	}

	/* Verify config block CRC */
	rc = cyttsp4_verify_config_block_crc(cd, ebid,
					     &crc_new, &crc_old, NULL);
	if (rc) {
		tp_log_err("%s: Error on verify config block crc\n", __func__);
		goto exit;
	}

	tp_log_debug("%s: crc_new:%04X crc_old:%04X\n",
		     __func__, crc_new, crc_old);

	if (crc_new == crc_old) {
		tp_log_debug("%s: Calculated crc matches stored crc\n",
			     __func__);
		goto exit;
	}

	PUT_FIELD16(si, crc_new, crc_data);

	rc = cyttsp4_write_config_common(cd, ebid, crc_offset, crc_data, 2);
	if (rc) {
		tp_log_err("%s: Error on write config crc\n", __func__);
		goto exit;
	}

exit:
	return rc;
}

static int cyttsp4_request_write_config_(struct cyttsp4_device *ttsp, u8 ebid,
					 u16 offset, u8 * data, u16 length)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);

	return cyttsp4_write_config(cd, ebid, offset, data, length);
}

#ifdef CYTTSP4_WATCHDOG_NULL_CMD
static void cyttsp4_watchdog_work(struct work_struct *work)
{
	struct cyttsp4_core_data *cd =
	    container_of(work, struct cyttsp4_core_data, watchdog_work);
	u8 cmd_buf[CY_CMD_OP_NULL_CMD_SZ];
	bool restart = false;
	int rc;

	rc = request_exclusive(cd, cd->core, 1);
	if (rc < 0) {
		tp_log_debug("%s: fail get exclusive ex=%p own=%p\n",
			     __func__, cd->exclusive_dev, cd->core);
		goto exit;
	}

	cmd_buf[0] = CY_CMD_OP_NULL;
	rc = cyttsp4_exec_cmd(cd, cd->mode,
			      cmd_buf, CY_CMD_OP_NULL_CMD_SZ,
			      NULL, CY_CMD_OP_NULL_RET_SZ,
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: Watchdog NULL cmd failed.\n", __func__);
		restart = true;
	}

	if (release_exclusive(cd, cd->core) < 0)
		tp_log_err("%s: fail to release exclusive\n", __func__);
	else
		tp_log_debug("%s: pass release exclusive\n", __func__);
exit:
	if (restart)
		cyttsp4_queue_startup(cd);
	else
		cyttsp4_start_wd_timer(cd);
}
#else
static void cyttsp4_watchdog_work(struct work_struct *work)
{
	struct cyttsp4_core_data *cd =
	    container_of(work, struct cyttsp4_core_data, watchdog_work);
	u8 mode[2] = { 0 };
	bool restart = false;
	int rc;

	if (cd == NULL) {
		tp_log_err("%s: NULL context pointer\n", __func__);
		return;
	}

	mutex_lock(&cd->system_lock);
	rc = cyttsp4_adap_read(cd, CY_REG_BASE, &mode, sizeof(mode));
	if (rc) {
		tp_log_err("%s: failed to access device r=%d\n", __func__, rc);
		restart = true;
		goto exit;
	}

	if (IS_BOOTLOADER(mode[0], mode[1])) {
		tp_log_err("%s: device found in bootloader mode\n", __func__);
		restart = true;
		goto exit;
	}
exit:
	if (restart)
		cyttsp4_queue_startup_(cd);
	else
		cyttsp4_start_wd_timer(cd);
	mutex_unlock(&cd->system_lock);
}
#endif

static int cyttsp4_request_stop_wd_(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_core *core = ttsp->core;
	struct cyttsp4_core_data *cd = dev_get_drvdata(&core->dev);
	cyttsp4_stop_wd_timer(cd);
	return 0;
}

static int _cyttsp4_put_device_into_deep_sleep(struct cyttsp4_core_data *cd,
					       u8 hst_mode_reg)
{
	int rc;

	hst_mode_reg |= CY_HST_SLEEP;

	tp_log_debug("%s: write DEEP SLEEP...\n", __func__);
	rc = cyttsp4_adap_write(cd, CY_REG_BASE, &hst_mode_reg,
				sizeof(hst_mode_reg));
	if (rc) {
		tp_log_err("%s: Fail write adapter r=%d\n", __func__, rc);
		return -EINVAL;
	}
	tp_log_debug("%s: write DEEP SLEEP succeeded\n", __func__);

	if (cd->pdata->power) {
		tp_log_debug("%s: Power down HW\n", __func__);
		rc = cd->pdata->power(cd->pdata, 0, cd->dev, &cd->ignore_irq);
	} else {
		tp_log_debug("%s: No power function\n", __func__);
		rc = 0;
	}
	if (rc < 0) {
		tp_log_err("%s: HW Power down fails r=%d\n", __func__, rc);
		return -EINVAL;
	}

	return 0;
}

static int _cyttsp4_put_device_into_easy_wakeup(struct cyttsp4_core_data *cd)
{
	u8 command_buf[CY_CMD_OP_WAIT_FOR_EVENT_CMD_SZ];
	int rc;

	if (!IS_TTSP_VER_GE(&cd->sysinfo, 2, 5))
		return -EINVAL;

	command_buf[0] = CY_CMD_OP_WAIT_FOR_EVENT;
	command_buf[1] = cd->easy_wakeup_gesture;

	rc = _cyttsp4_exec_cmd(cd, CY_MODE_OPERATIONAL, command_buf,
			       CY_CMD_OP_WAIT_FOR_EVENT_CMD_SZ);
	cd->int_status &= ~CY_INT_EXEC_CMD;
	if (rc)
		tp_log_err("%s: Error executing command r=%d\n", __func__, rc);

	tp_log_debug("%s:%d rc=%d\n", __func__, __LINE__, rc);

	return rc;
}

static int _cyttsp4_wait_for_refresh_cycle(struct cyttsp4_core_data *cd,
					   int cycle)
{
	int active_refresh_cycle_ms;

	if (cd->active_refresh_cycle_ms)
		active_refresh_cycle_ms = cd->active_refresh_cycle_ms;
	else
		active_refresh_cycle_ms = 20;

	msleep(cycle * active_refresh_cycle_ms);

	return 0;
}

static int _cyttsp4_put_device_into_sleep(struct cyttsp4_core_data *cd,
					  u8 hst_mode_reg)
{
	int rc;

	if (IS_DEEP_SLEEP_CONFIGURED(cd->easy_wakeup_gesture))
		rc = _cyttsp4_put_device_into_deep_sleep(cd, hst_mode_reg);
	else
		rc = _cyttsp4_put_device_into_easy_wakeup(cd);

	return rc;
}

static int cyttsp4_core_sleep_(struct cyttsp4_core_data *cd)
{
	u8 mode[2] = { 0 };
	int rc = 0;

	cyttsp4_stop_wd_timer(cd);

	/* Wait until currently running IRQ handler exits and disable IRQ */
	if (cd->irq_enabled) {
		cd->irq_enabled = false;
		disable_irq(cd->irq);
	}

	mutex_lock(&cd->system_lock);
	/* Already in sleep mode? */
	if (cd->sleep_state == SS_SLEEP_ON)
		goto exit;

	cd->sleep_state = SS_SLEEPING;

	rc = cyttsp4_adap_read(cd, CY_REG_BASE, &mode, sizeof(mode));
	if (rc) {
		tp_log_err("%s: Fail read adapter r=%d\n", __func__, rc);
		goto exit;
	}

	if (IS_BOOTLOADER(mode[0], mode[1])) {
		tp_log_err("%s: Device in BOOTLADER mode.\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	/* Deep sleep is only allowed in Operating mode */
	if (GET_HSTMODE(mode[0]) != CY_HST_OPERATE) {
		tp_log_err("%s: Device is not in Operating mode (%02X)\n",
			   __func__, GET_HSTMODE(mode[0]));
		mutex_unlock(&cd->system_lock);
		if (!cd->irq_enabled) {
			cd->irq_enabled = true;
			enable_irq(cd->irq);
		}
		/* Try switching to Operating mode */
		rc = set_mode(cd, CY_MODE_OPERATIONAL);
		if (cd->irq_enabled) {
			cd->irq_enabled = false;
			disable_irq(cd->irq);
		}
		mutex_lock(&cd->system_lock);
		if (rc < 0) {
			tp_log_err
			    ("%s: failed to set mode to Operational rc=%d\n",
			     __func__, rc);
			cyttsp4_queue_startup_(cd);
			rc = 0;
			goto exit;
		}

		/* Get the new host mode register value */
		rc = cyttsp4_adap_read(cd, CY_REG_BASE, &mode, sizeof(mode));
		if (rc) {
			tp_log_err("%s: Fail read adapter r=%d\n",
				   __func__, rc);
			goto exit;
		}
	}

	rc = _cyttsp4_put_device_into_sleep(cd, mode[0]);

exit:
	if (rc) {
		cd->sleep_state = SS_SLEEP_OFF;
		cyttsp4_start_wd_timer(cd);
	} else {
		cd->sleep_state = SS_SLEEP_ON;
		cd->int_status |= CY_INT_IGNORE;
	}

	mutex_unlock(&cd->system_lock);

	/* disable irq when tp go to sleep mode */
	if (cd->pdata->flags & CY_CORE_FLAG_WAKE_ON_GESTURE) {
		if (!cd->irq_enabled) {
			cd->irq_enabled = true;
			enable_irq(cd->irq);
		}
	}

	return rc;
}

static int cyttsp4_core_sleep(struct cyttsp4_core_data *cd)
{
	int rc;

	rc = request_exclusive(cd, cd->core,
			       CY_CORE_SLEEP_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: fail get exclusive ex=%p own=%p\n",
			   __func__, cd->exclusive_dev, cd->core);
		return 0;
	}

	rc = cyttsp4_core_sleep_(cd);

	if (release_exclusive(cd, cd->core) < 0)
		tp_log_err("%s: fail to release exclusive\n", __func__);
	else
		tp_log_debug("%s: pass release exclusive\n", __func__);

	/* Give time to FW to sleep */
	_cyttsp4_wait_for_refresh_cycle(cd, 2);

	return rc;
}

static int _cyttsp4_awake_device_from_deep_sleep(struct cyttsp4_core_data *cd,
						 int timeout_ms)
{
	struct device *dev = cd->dev;
	u8 mode = 0;
	int t;
	int rc;

	cd->int_status |= CY_INT_AWAKE;

	if (cd->pdata->power) {
		/* Wake up using platform power function */
		tp_log_debug("%s: Power up HW\n", __func__);
		rc = cd->pdata->power(cd->pdata, 1, dev, &cd->ignore_irq);
	} else {
		/* Initiate a read transaction to wake up */
		rc = cyttsp4_adap_read(cd, CY_REG_BASE, &mode, sizeof(mode));
	}
	if (rc < 0) {
		tp_log_err("%s: HW Power up fails r=%d\n", __func__, rc);
		/* Initiate another read transaction to wake up */
		rc = cyttsp4_adap_read(cd, CY_REG_BASE, &mode, sizeof(mode));
	} else
		tp_log_debug("%s: HW power up succeeds\n", __func__);
	mutex_unlock(&cd->system_lock);

	t = wait_event_timeout(cd->wait_q,
			       (cd->int_status & CY_INT_AWAKE) == 0,
			       msecs_to_jiffies(timeout_ms));
	mutex_lock(&cd->system_lock);
	if (IS_TMO(t)) {
		tp_log_debug("%s: TMO waiting for wakeup\n", __func__);
		cd->int_status &= ~CY_INT_AWAKE;
		/* Perform a read transaction to check if device is awake */
		rc = cyttsp4_adap_read(cd, CY_REG_BASE, &mode, sizeof(mode));
		if (rc < 0 || GET_HSTMODE(mode) != CY_HST_OPERATE) {
			tp_log_err("%s: Queueing startup\n", __func__);
			/* Try starting up */
			cyttsp4_queue_startup_(cd);
		}
	}

	return rc;
}

static int _cyttsp4_awake_device(struct cyttsp4_core_data *cd)
{
	int timeout_ms;

	if (cd->wake_initiated_by_device) {
		cd->wake_initiated_by_device = 0;
		/* To prevent sequential wake/sleep caused by ttsp modules */
		msleep(20);
		return 0;
	}

	if (IS_DEEP_SLEEP_CONFIGURED(cd->easy_wakeup_gesture))
		timeout_ms = CY_CORE_WAKEUP_TIMEOUT;
	else
		timeout_ms = CY_CORE_WAKEUP_TIMEOUT * 4;

	return _cyttsp4_awake_device_from_deep_sleep(cd, timeout_ms);
}

static int cyttsp4_core_wake_(struct cyttsp4_core_data *cd)
{
	int rc;

	/* Already woken? */
	mutex_lock(&cd->system_lock);
	if (cd->sleep_state == SS_SLEEP_OFF) {
		mutex_unlock(&cd->system_lock);
		return 0;
	}

	cd->int_status &= ~CY_INT_IGNORE;
	cd->sleep_state = SS_WAKING;

	rc = _cyttsp4_awake_device(cd);
	if (rc < 0) {
		tp_log_err("%s: _cyttsp4_awake_device faile, rc=%d\n", __func__,
			   rc);
	}

	cd->sleep_state = SS_SLEEP_OFF;
	mutex_unlock(&cd->system_lock);

	cyttsp4_start_wd_timer(cd);

	return 0;
}

static int cyttsp4_core_wake(struct cyttsp4_core_data *cd)
{
	int rc;

	rc = request_exclusive(cd, cd->core, CY_CORE_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: fail get exclusive ex=%p own=%p\n",
			   __func__, cd->exclusive_dev, cd->core);
		return 0;
	}

	rc = cyttsp4_core_wake_(cd);

	if (release_exclusive(cd, cd->core) < 0)
		tp_log_err("%s: fail to release exclusive\n", __func__);
	else
		tp_log_debug("%s: pass release exclusive\n", __func__);

	/* If a startup queued in wake, wait it to finish */
	wait_event_timeout(cd->wait_q, cd->startup_state == STARTUP_NONE,
			   msecs_to_jiffies(CY_CORE_RESET_AND_WAIT_TIMEOUT));

	return rc;
}

static int cyttsp4_get_ttconfig_info(struct cyttsp4_core_data *cd)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;
	u16 length, max_length;
	u16 version = 0;
	u16 crc = 0;
	int rc;

	return 0;

	rc = set_mode(cd, CY_MODE_CAT);
	if (rc < 0) {
		tp_log_err("%s: failed to set mode to CAT rc=%d\n",
			   __func__, rc);
		return rc;
	}

	rc = cyttsp4_get_ttconfig_version(cd, &version);
	if (rc < 0) {
		tp_log_err("%s: failed to get ttconfig version rc=%d\n",
			   __func__, rc);
		return rc;
	}

	rc = cyttsp4_get_config_length(cd, CY_TCH_PARM_EBID,
				       &length, &max_length);
	if (rc < 0) {
		tp_log_err("%s: failed to get ttconfig length rc=%d\n",
			   __func__, rc);
		return rc;
	}

	rc = set_mode(cd, CY_MODE_OPERATIONAL);
	if (rc < 0) {
		tp_log_err("%s: failed to set mode to Operational rc=%d\n",
			   __func__, rc);
		return rc;
	}

	rc = cyttsp4_get_config_block_crc(cd, CY_TCH_PARM_EBID, &crc);
	if (rc < 0) {
		tp_log_err("%s: failed to get ttconfig crc rc=%d\n",
			   __func__, rc);
		return rc;
	}

	si->ttconfig.version = version;
	si->ttconfig.length = length;
	si->ttconfig.max_length = max_length;
	si->ttconfig.crc = crc;

	tp_log_debug
	    ("%s: TT Config Version:%04X Length:%d Max Length:%d CRC:%04X\n",
	     __func__, si->ttconfig.version, si->ttconfig.length,
	     si->ttconfig.length, si->ttconfig.crc);

	return 0;
}

static int cyttsp4_get_active_refresh_cycle(struct cyttsp4_core_data *cd)
{
	int rc;
	u32 value;

	rc = cyttsp4_get_parameter(cd, CY_RAM_ID_REFRESH_INTERVAL, &value);
	if (!rc)
		cd->active_refresh_cycle_ms = (u8) value;

	return rc;
}

static int cyttsp4_set_initial_scantype(struct cyttsp4_core_data *cd)
{
	u8 new_scantype;
	int rc;

	rc = cyttsp4_get_scantype(cd, &cd->default_scantype);
	if (rc < 0) {
		tp_log_err("%s: failed to get scantype rc=%d\n", __func__, rc);
		goto exit;
	}

	/* Disable proximity sensing by default */
	cd->default_scantype &= ~CY_SCAN_TYPE_PROXIMITY;

	new_scantype = _cyttsp4_generate_new_scantype(cd);

	rc = cyttsp4_set_scantype(cd, new_scantype);
	if (rc < 0) {
		tp_log_err("%s: failed to set scantype rc=%d\n", __func__, rc);
		goto exit;
	}
exit:
	return rc;
}

static int cyttsp4_set_opmode(struct cyttsp4_core_data *cd)
{
	u8 cmd_buf[CY_CMD_OP_SET_PARAM_CMD_SZ];
	u8 return_buf[CY_CMD_OP_SET_PARAM_RET_SZ];
	u8 scan_type = 0;
	int rc;

	mutex_lock(&cd->system_lock);
	switch (cd->opmode) {
	case OPMODE_NONE:
		mutex_unlock(&cd->system_lock);
		return 0;
	case OPMODE_FINGER:
		scan_type = CY_OP_PARA_SCAN_TYPE_NORMAL;
		break;
	case OPMODE_GLOVE:
		scan_type = CY_OP_PARA_SCAN_TYPE_APAMC_MASK |
		    CY_OP_PARA_SCAN_TYPE_GLOVE_MASK;
		break;
	}
	mutex_unlock(&cd->system_lock);

	cmd_buf[0] = CY_CMD_OP_SET_PARA;
	cmd_buf[1] = CY_OP_PARA_SCAN_TYPE;
	cmd_buf[2] = CY_OP_PARA_SCAN_TYPE_SZ;
	cmd_buf[3] = scan_type;

	rc = cyttsp4_exec_cmd(cd, cd->mode,
			      cmd_buf, sizeof(cmd_buf),
			      return_buf, sizeof(return_buf),
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0)
		tp_log_err("%s: exec cmd error.\n", __func__);

	return rc;
}


static int cyttsp4_startup_(struct cyttsp4_core_data *cd)
{
	int retry = CY_CORE_STARTUP_RETRY_COUNT;
	int rc;
	bool detected = false;

	tp_log_debug("%s: enter...\n", __func__);

	cyttsp4_stop_wd_timer(cd);

reset:
	if (retry != CY_CORE_STARTUP_RETRY_COUNT)
		tp_log_debug("%s: Retry %d\n", __func__,
			     CY_CORE_STARTUP_RETRY_COUNT - retry);

	/* reset hardware and wait for heartbeat */
	rc = cyttsp4_reset_and_wait(cd);
	/* solve the ESD problem when reset suc but red register fail */
	if (rc < 0 || retry == 0) {
		if (rc < 0) {
			tp_log_err("%s: Error on h/w reset r=%d\n", __func__,
				   rc);
		}
		//RETRY_OR_EXIT(retry--, reset, exit);
		/*when the retry subtract to 0,retry does not need to subtract 1 */
		if (retry > 0) {
			retry--;
			goto reset;
		}
		/*Fixed the ESD question */
		tp_log_err("%s: Error on h/w reset r=%d\n", __func__, rc);
		gpio_set_value(cd->pdata->rst_gpio, 0);

		rc = regulator_disable(vbus_cypress);
		if (rc < 0) {
			tp_log_err("%s: failed to disable cypress vbus\n",
				   __func__);
			return -EINVAL;
		}
		mdelay(2000);

		rc = regulator_enable(vbus_cypress);
		if (rc < 0) {
			tp_log_err("%s: failed to enable cypress vdd\n",
				   __func__);
			return -EINVAL;
		}
		mdelay(100);

		gpio_set_value(cd->pdata->rst_gpio, 1);

		rc = cyttsp4_reset_and_wait(cd);
		if (rc < 0) {
			tp_log_err("%s: Error on h/w reset2 r=%d\n", __func__,
				   rc);
		}
	}

	detected = true;

	/* exit bl into sysinfo mode */
	tp_log_debug("%s: write exit ldr...\n", __func__);
	mutex_lock(&cd->system_lock);
	cd->int_status &= ~CY_INT_IGNORE;
	cd->int_status |= CY_INT_MODE_CHANGE;

	rc = cyttsp4_adap_write(cd, CY_REG_BASE, (u8 *) ldr_exit,
				sizeof(ldr_exit));
	mutex_unlock(&cd->system_lock);
	if (rc < 0) {
		tp_log_err("%s: Fail write adap='%s' r=%d\n",
			   __func__, cd->core->adap->id, rc);
		RETRY_OR_EXIT(retry--, reset, exit);
	}

	rc = cyttsp4_wait_sysinfo_mode(cd);
	if (rc < 0) {
		u8 buf[sizeof(ldr_err_app)];
		int rc1;

		/* Check for invalid/corrupted touch application */
		rc1 = cyttsp4_adap_read(cd, CY_REG_BASE, buf,
					sizeof(ldr_err_app));
		if (rc1) {
			tp_log_err("%s: Fail read adap='%s' r=%d\n",
				   __func__, cd->core->adap->id, rc1);
		} else if (!memcmp(buf, ldr_err_app, sizeof(ldr_err_app))) {
			tp_log_err("%s: Error launching touch application\n",
				   __func__);
			mutex_lock(&cd->system_lock);
			cd->invalid_touch_app = true;
			mutex_unlock(&cd->system_lock);
			goto exit_no_wd;
		}

		RETRY_OR_EXIT(retry--, reset, exit);
	}

	mutex_lock(&cd->system_lock);
	cd->invalid_touch_app = false;
	mutex_unlock(&cd->system_lock);

	/* read sysinfo data */
	tp_log_debug("%s: get sysinfo regs..\n", __func__);
	rc = cyttsp4_get_sysinfo_regs(cd);
	if (rc < 0) {
		tp_log_err("%s: failed to get sysinfo regs rc=%d\n",
			   __func__, rc);
		RETRY_OR_EXIT(retry--, reset, exit);
	}

	rc = set_mode(cd, CY_MODE_OPERATIONAL);
	if (rc < 0) {
		tp_log_err("%s: failed to set mode to operational rc=%d\n",
			   __func__, rc);
		RETRY_OR_EXIT(retry--, reset, exit);
	}

	rc = cyttsp4_set_initial_scantype(cd);
	if (rc < 0) {
		tp_log_err("%s: failed to get scantype rc=%d\n", __func__, rc);
		RETRY_OR_EXIT(retry--, reset, exit);
	}

	rc = cyttsp4_get_ttconfig_info(cd);
	if (rc < 0) {
		tp_log_err("%s: failed to get ttconfig info rc=%d\n",
			   __func__, rc);
		RETRY_OR_EXIT(retry--, reset, exit);
	}

	rc = cyttsp4_get_active_refresh_cycle(cd);
	if (rc < 0)
		tp_log_err("%s: failed to get refresh cycle time rc=%d\n",
			   __func__, rc);

	rc = cyttsp4_set_opmode(cd);
	if (rc < 0)
		tp_log_err("%s: failed to set opmode rc=%d\n", __func__, rc);

	/* attention startup */
	call_atten_cb(cd, CY_ATTEN_STARTUP, 0);

	/* restore to sleep if was suspended */
	mutex_lock(&cd->system_lock);
	if (cd->sleep_state == SS_SLEEP_ON) {
		cd->sleep_state = SS_SLEEP_OFF;
		mutex_unlock(&cd->system_lock);
		/* watchdog is restarted by cyttsp4_core_sleep_() on error */
		cyttsp4_core_sleep_(cd);
		goto exit_no_wd;
	}
	mutex_unlock(&cd->system_lock);

exit:
	cyttsp4_start_wd_timer(cd);

exit_no_wd:
	if (!detected)
		rc = -ENODEV;

	/* Required for signal to the TTHE */
	tp_log_info("%s: cyttsp4_exit startup r=%d...\n", __func__, rc);

	return rc;
}

static int cyttsp4_startup(struct cyttsp4_core_data *cd)
{
	int rc;

	mutex_lock(&cd->system_lock);
	cd->startup_state = STARTUP_RUNNING;
	mutex_unlock(&cd->system_lock);

	rc = request_exclusive(cd, cd->core, CY_CORE_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: fail get exclusive ex=%p own=%p\n",
			   __func__, cd->exclusive_dev, cd->core);
		goto exit;
	}

	rc = cyttsp4_startup_(cd);

	if (release_exclusive(cd, cd->core) < 0)
		/* Don't return fail code, mode is already changed. */
		tp_log_err("%s: fail to release exclusive\n", __func__);
	else
		tp_log_debug("%s: pass release exclusive\n", __func__);

exit:
	mutex_lock(&cd->system_lock);
	cd->startup_state = STARTUP_NONE;
	mutex_unlock(&cd->system_lock);

	/* Wake the waiters for end of startup */
	wake_up(&cd->wait_q);

	return rc;
}

static void cyttsp4_startup_work_function(struct work_struct *work)
{
	struct cyttsp4_core_data *cd = container_of(work,
						    struct cyttsp4_core_data,
						    startup_work);
	int rc;

	/*
	 * Force clear exclusive access
	 * startup queue is called for abnormal case,
	 * and when a this called access can be acquired in other context
	 */
	mutex_lock(&cd->system_lock);
	if (cd->exclusive_dev != cd->core)
		cd->exclusive_dev = NULL;
	mutex_unlock(&cd->system_lock);
	rc = cyttsp4_startup(cd);
	if (rc < 0)
		tp_log_err("%s: Fail queued startup r=%d\n", __func__, rc);
}

static void cyttsp4_free_si_ptrs(struct cyttsp4_core_data *cd)
{
	struct cyttsp4_sysinfo *si = &cd->sysinfo;

	if (!si)
		return;

	kfree(si->si_ptrs.cydata);
	kfree(si->si_ptrs.test);
	kfree(si->si_ptrs.pcfg);
	kfree(si->si_ptrs.opcfg);
	kfree(si->si_ptrs.ddata);
	kfree(si->si_ptrs.mdata);
	kfree(si->btn);
	kfree(si->xy_mode);
	kfree(si->btn_rec_data);
}

#if defined(CONFIG_PM_RUNTIME)
static int cyttsp4_core_rt_suspend(struct device *dev)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	int rc;

	rc = cyttsp4_core_sleep(cd);
	if (rc < 0) {
		tp_log_err("%s: Error on sleep\n", __func__);
		return -EAGAIN;
	}
	return 0;
}

static int cyttsp4_core_rt_resume(struct device *dev)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	int rc;

	/* enable irq before tp wakeup */
	if (!(cd->pdata->flags & CY_CORE_FLAG_WAKE_ON_GESTURE)) {
		if (!cd->irq_enabled) {
			cd->irq_enabled = true;
			enable_irq(cd->irq);
		}
	}

	rc = cyttsp4_core_wake(cd);
	if (rc < 0) {
		tp_log_err("%s: Error on wake\n", __func__);
		return -EAGAIN;
	}
	if (SET_AT_LATE_RESUME_NEEDED == atomic_read(&holster_status_flag)) {
		tp_log_info
		    ("%s: enter,set holster_sensitivity when waking up,new status is %ld \n",
		     __func__, holster_status);
		msleep(60);
		rc = set_holster_sensitivity(cd, holster_status);
		if (rc < 0) {
			tp_log_err
			    ("%s: Error,set holster_sensitivity when wake \n",
			     __func__);
		}
		atomic_set(&holster_status_flag, SET_AT_LATE_RESUME_NEED_NOT);
	}
	return 0;
}
#endif

/* temp comment the suspend and sleep */
#if defined(CONFIG_PM_SLEEP)
static int cyttsp4_core_suspend(struct device *dev)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);

	if (!(cd->pdata->flags & CY_CORE_FLAG_WAKE_ON_GESTURE))
		return 0;

	/*
	 * This will not prevent resume
	 * Required to prevent interrupts before i2c awake
	 */
	if (cd->irq_enabled) {
		cd->irq_enabled = false;
		disable_irq(cd->irq);
	}

	if (device_may_wakeup(dev)) {
		tp_log_debug("%s Device MAY wakeup\n", __func__);
		if (!enable_irq_wake(cd->irq))
			cd->irq_wake = 1;
	} else {
		tp_log_debug("%s Device may NOT wakeup\n", __func__);
	}

	return 0;

}

static int cyttsp4_core_resume(struct device *dev)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);

	if (!(cd->pdata->flags & CY_CORE_FLAG_WAKE_ON_GESTURE))
		return 0;

	if (!cd->irq_enabled) {
		cd->irq_enabled = true;
		enable_irq(cd->irq);
	}

	if (device_may_wakeup(dev)) {
		tp_log_debug("%s Device MAY wakeup\n", __func__);
		if (cd->irq_wake) {
			disable_irq_wake(cd->irq);
			cd->irq_wake = 0;
		}
	} else {
		tp_log_debug("%s Device may NOT wakeup\n", __func__);
	}

	return 0;

}
#endif

static const struct dev_pm_ops cyttsp4_core_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(cyttsp4_core_suspend, cyttsp4_core_resume)
	    SET_RUNTIME_PM_OPS(cyttsp4_core_rt_suspend, cyttsp4_core_rt_resume,
			       NULL)
};

/* delete the fb_call set in core driver */

/*
 * Show Firmware version via sysfs
 */
static ssize_t cyttsp4_ic_ver_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp4_cydata *cydata;

	mutex_lock(&cd->system_lock);
	if (!cd->sysinfo.ready) {
		if (cd->invalid_touch_app) {
			mutex_unlock(&cd->system_lock);
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
					"Corrupted Touch application!\n");
		} else {
			mutex_unlock(&cd->system_lock);
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
					"System Information not ready!\n");
		}
	}
	mutex_unlock(&cd->system_lock);

	cydata = cd->sysinfo.si_ptrs.cydata;

	return snprintf(buf, CY_MAX_PRBUF_SIZE,
			"%s: 0x%02X 0x%02X\n"
			"%s: 0x%02X\n"
			"%s: 0x%02X\n"
			"%s: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n"
			"%s: 0x%04X\n"
			"%s: 0x%02X\n"
			"%s: 0x%02X\n",
			"TrueTouch Product ID", cydata->ttpidh, cydata->ttpidl,
			"Firmware Major Version", cydata->fw_ver_major,
			"Firmware Minor Version", cydata->fw_ver_minor,
			"Revision Control Number", cydata->revctrl[0],
			cydata->revctrl[1], cydata->revctrl[2],
			cydata->revctrl[3], cydata->revctrl[4],
			cydata->revctrl[5], cydata->revctrl[6],
			cydata->revctrl[7], "TrueTouch Config Version",
			cd->sysinfo.ttconfig.version,
			"Bootloader Major Version", cydata->blver_major,
			"Bootloader Minor Version", cydata->blver_minor);
}

/*
 * Show TT Config version via sysfs
 */
static ssize_t cyttsp4_ttconfig_ver_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);

	return snprintf(buf, CY_MAX_PRBUF_SIZE, "0x%04X\n",
			cd->sysinfo.ttconfig.version);
}

/*
 * Show Driver version via sysfs
 */
static ssize_t cyttsp4_drv_ver_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return snprintf(buf, CY_MAX_PRBUF_SIZE,
			"Driver: %s\nVersion: %s\nDate: %s\n",
			cy_driver_core_name, cy_driver_core_version,
			cy_driver_core_date);
}

/*
 * HW reset via sysfs
 */
static ssize_t cyttsp4_hw_reset_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	int rc = 0;

	rc = cyttsp4_startup(cd);
	if (rc < 0)
		tp_log_err("%s: HW reset failed r=%d\n", __func__, rc);

	return size;
}

/*
 * Show IRQ status via sysfs
 */
static ssize_t cyttsp4_hw_irq_stat_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	int retval;

	if (cd->pdata->irq_stat) {
		retval = cd->pdata->irq_stat(cd->pdata, dev);
		switch (retval) {
		case 0:
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
					"Interrupt line is LOW.\n");
		case 1:
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
					"Interrupt line is HIGH.\n");
		default:
			return snprintf(buf, CY_MAX_PRBUF_SIZE,
					"Function irq_stat() returned %d.\n",
					retval);
		}
	}

	return snprintf(buf, CY_MAX_PRBUF_SIZE,
			"Function irq_stat() undefined.\n");
}

/*
 * Show IRQ enable/disable status via sysfs
 */
static ssize_t cyttsp4_drv_irq_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&cd->system_lock);
	if (cd->irq_enabled)
		ret = snprintf(buf, CY_MAX_PRBUF_SIZE,
			       "Driver interrupt is ENABLED\n");
	else
		ret = snprintf(buf, CY_MAX_PRBUF_SIZE,
			       "Driver interrupt is DISABLED\n");
	mutex_unlock(&cd->system_lock);

	return ret;
}

/*
 * Enable/disable IRQ via sysfs
 */
static ssize_t cyttsp4_drv_irq_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	unsigned long value;
	int retval;

	retval = kstrtoul(buf, 10, &value);
	if (retval < 0) {
		tp_log_err("%s: Invalid value\n", __func__);
		goto cyttsp4_drv_irq_store_error_exit;
	}

	mutex_lock(&cd->system_lock);
	switch (value) {
	case 0:
		if (cd->irq_enabled) {
			cd->irq_enabled = false;
			/* Disable IRQ */
			disable_irq_nosync(cd->irq);
			tp_log_info("%s: Driver IRQ now disabled\n", __func__);
		} else
			tp_log_info("%s: Driver IRQ already disabled\n",
				    __func__);
		break;

	case 1:
		if (cd->irq_enabled == false) {
			cd->irq_enabled = true;
			/* Enable IRQ */
			enable_irq(cd->irq);
			tp_log_info("%s: Driver IRQ now enabled\n", __func__);
		} else
			tp_log_info("%s: Driver IRQ already enabled\n",
				    __func__);
		break;

	default:
		tp_log_err("%s: Invalid value\n", __func__);
	}
	mutex_unlock(&(cd->system_lock));

cyttsp4_drv_irq_store_error_exit:

	return size;
}

/*
 * Debugging options via sysfs
 */
static ssize_t cyttsp4_drv_debug_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	unsigned long value = 0;
	int rc = 0;

	rc = kstrtoul(buf, 10, &value);
	if (rc < 0) {
		tp_log_err("%s: Invalid value\n", __func__);
		goto cyttsp4_drv_debug_store_exit;
	}

	switch (value) {
	case CY_DBG_SUSPEND:
		tp_log_info("%s: SUSPEND (cd=%p)\n", __func__, cd);
		rc = cyttsp4_core_sleep(cd);
		if (rc)
			tp_log_err("%s: Suspend failed rc=%d\n", __func__, rc);
		else
			tp_log_info("%s: Suspend succeeded\n", __func__);
		break;

	case CY_DBG_RESUME:
		tp_log_info("%s: RESUME (cd=%p)\n", __func__, cd);
		rc = cyttsp4_core_wake(cd);
		if (rc)
			tp_log_err("%s: Resume failed rc=%d\n", __func__, rc);
		else
			tp_log_info("%s: Resume succeeded\n", __func__);
		break;
	case CY_DBG_SOFT_RESET:
		tp_log_info("%s: SOFT RESET (cd=%p)\n", __func__, cd);
		rc = cyttsp4_hw_soft_reset(cd);
		break;
	case CY_DBG_RESET:
		tp_log_info("%s: HARD RESET (cd=%p)\n", __func__, cd);
		rc = cyttsp4_hw_hard_reset(cd);
		break;
	default:
		tp_log_err("%s: Invalid value\n", __func__);
	}

cyttsp4_drv_debug_store_exit:
	return size;
}

/*
 * Show system status on deep sleep status via sysfs
 */
static ssize_t cyttsp4_sleep_status_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&cd->system_lock);
	if (cd->sleep_state == SS_SLEEP_ON)
		ret = snprintf(buf, CY_MAX_PRBUF_SIZE,
			       "Deep Sleep is ENABLED\n");
	else
		ret = snprintf(buf, CY_MAX_PRBUF_SIZE,
			       "Deep Sleep is DISABLED\n");
	mutex_unlock(&cd->system_lock);

	return ret;
}

static ssize_t cyttsp4_easy_wakeup_gesture_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	ssize_t ret;

	mutex_lock(&cd->system_lock);
	ret = snprintf(buf, CY_MAX_PRBUF_SIZE, "%d\n", cd->easy_wakeup_gesture);
	mutex_unlock(&cd->system_lock);
	return ret;
}

static ssize_t cyttsp4_easy_wakeup_gesture_store(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t size)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	unsigned long value;
	int ret;

	ret = kstrtoul(buf, 10, &value);
	if (ret < 0)
		return ret;

	if ((value > 0xFF || value < 0) ||
	    (!(value & cd->easy_wakeup_supported_gestures) && value)) {
		tp_log_err("%s: Invalid value:%ld", __func__, value);
		return -EINVAL;
	}

	pm_runtime_get_sync(dev);

	if (value != 0xFF && value != 0)
		cd->pdata->flags = CY_CORE_FLAG_WAKE_ON_GESTURE;
	else
		cd->pdata->flags = CY_CORE_FLAG_NONE;

	mutex_lock(&cd->system_lock);
	if (cd->sysinfo.ready && IS_TTSP_VER_GE(&cd->sysinfo, 2, 5))
		cd->easy_wakeup_gesture = (u8) GESTURE_FROM_APP(value);
	else
		ret = -ENODEV;
	mutex_unlock(&cd->system_lock);

	pm_runtime_put(dev);

	if (ret)
		return ret;

	return size;
}

static ssize_t cyttsp4_ts_easy_wakeup_gesture_show(struct kobject *dev,
						   struct kobj_attribute *attr,
						   char *buf)
{
	struct device *cdev = core_dev_ptr;
	if (!cdev) {
		tp_log_err("%s: device is null", __func__);
		return -EINVAL;
	}

	return cyttsp4_easy_wakeup_gesture_show(cdev, NULL, buf);
}

static ssize_t cyttsp4_ts_easy_wakeup_gesture_store(struct kobject *dev,
						    struct kobj_attribute *attr,
						    const char *buf,
						    size_t size)
{
	struct device *cdev = core_dev_ptr;
	if (!cdev) {
		tp_log_err("%s: device is null", __func__);
		return -EINVAL;
	}

	return cyttsp4_easy_wakeup_gesture_store(cdev, NULL, buf, size);
}

static ssize_t cyttsp4_ts_supported_gesture_show(struct kobject *dev,
						 struct kobj_attribute *attr,
						 char *buf)
{
	struct device *cdev = core_dev_ptr;
	struct cyttsp4_core_data *cd = dev_get_drvdata(cdev);
	if (!cdev) {
		tp_log_err("%s: device is null", __func__);
		return -EINVAL;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n",
			cd->easy_wakeup_supported_gestures);
}


static ssize_t cyttsp4_signal_disparity_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	u8 cmd_buf[CY_CMD_OP_GET_PARAM_CMD_SZ];
	u8 return_buf[CY_CMD_OP_GET_PARAM_RET_SZ];
	int rc;

	cmd_buf[0] = CY_CMD_OP_GET_PARA;
	cmd_buf[1] = CY_OP_PARA_SCAN_TYPE;

	pm_runtime_get_sync(dev);

	rc = request_exclusive(cd, cd->core, CY_CORE_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: fail get exclusive ex=%p own=%p\n",
			   __func__, cd->exclusive_dev, cd->core);
		goto exit_put;
	}

	rc = cyttsp4_exec_cmd(cd, CY_MODE_OPERATIONAL,
			      cmd_buf, sizeof(cmd_buf),
			      return_buf, sizeof(return_buf),
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: exec cmd error.\n", __func__);
		goto exit_release;
	}

	tp_log_debug("%s: return_buf=0x%x,0x%x,0x%x\n", __func__,
		     return_buf[0], return_buf[1], return_buf[2]);

	if (return_buf[0] != CY_OP_PARA_SCAN_TYPE) {
		tp_log_err("%s: return data error.\n", __func__);
		rc = -EINVAL;
		goto exit_release;
	}

	rc = return_buf[2];

exit_release:
	if (release_exclusive(cd, cd->core) < 0)
		tp_log_err("%s: fail to release exclusive\n", __func__);
exit_put:
	pm_runtime_put(dev);
	return scnprintf(buf, CY_MAX_PRBUF_SIZE, "%d\n", rc);
}

static ssize_t cyttsp4_signal_disparity_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t size)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	unsigned long disparity_val;
	u8 cmd_buf[CY_CMD_OP_SET_PARAM_CMD_SZ];
	u8 return_buf[CY_CMD_OP_SET_PARAM_RET_SZ];
	u8 scan_type;
	int rc, retry;

	rc = kstrtoul(buf, 10, &disparity_val);
	if (rc < 0) {
		tp_log_err("%s: Invalid value.\n", __func__);
		goto exit;
	}

	mutex_lock(&cd->system_lock);
	switch (disparity_val) {
	case CY_SIGNAL_DISPARITY_NONE:
		cd->opmode = OPMODE_FINGER;
		scan_type = CY_OP_PARA_SCAN_TYPE_NORMAL;
		break;
	case CY_SIGNAL_DISPARITY_SENSITIVITY:
		cd->opmode = OPMODE_GLOVE;
		scan_type = CY_OP_PARA_SCAN_TYPE_APAMC_MASK |
		    CY_OP_PARA_SCAN_TYPE_GLOVE_MASK;
		break;
	default:
		mutex_unlock(&cd->system_lock);
		tp_log_err("%s: Invalid signal disparity=%d\n", __func__,
			   (int)disparity_val);
		rc = -EINVAL;
		goto exit;
	}
	mutex_unlock(&cd->system_lock);

	cmd_buf[0] = CY_CMD_OP_SET_PARA;
	cmd_buf[1] = CY_OP_PARA_SCAN_TYPE;
	cmd_buf[2] = CY_OP_PARA_SCAN_TYPE_SZ;
	cmd_buf[3] = scan_type;
	for (retry = 0; retry < 10; retry++) {
		if (cd->sleep_state == SS_SLEEP_OFF)
			break;
		else {
			tp_log_info
			    ("%s,need delay 50 ms to write this command.\n",
			     __func__);
			msleep(50);
		}
	}

	pm_runtime_get_sync(dev);

	rc = request_exclusive(cd, cd->core, CY_CORE_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: fail get exclusive ex=%p own=%p\n",
			   __func__, cd->exclusive_dev, cd->core);
		goto exit_put;
	}

	rc = cyttsp4_exec_cmd(cd, CY_MODE_OPERATIONAL,
			      cmd_buf, sizeof(cmd_buf),
			      return_buf, sizeof(return_buf),
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: exec cmd error.\n", __func__);
		goto exit_release;
	}

	rc = size;
	tp_log_debug("%s: return_buf=0x%x,0x%x\n", __func__,
		     return_buf[0], return_buf[1]);
exit_release:
	if (release_exclusive(cd, cd->core) < 0)
		/* Don't return fail code, mode is already changed. */
		tp_log_err("%s: fail to release exclusive\n", __func__);
	else
		tp_log_debug("%s: pass release exclusive\n", __func__);
exit_put:
	pm_runtime_put(dev);
exit:
	return rc;
}

/*this function show and its store is copy from cyttsp4_signal_disparity_show 
  and cyttsp4_signal_disparity_store,just change the cmd value*/
static ssize_t holster_set_sensitivity_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	u8 cmd_buf[CY_CMD_OP_GET_PARAM_CMD_SZ];
	u8 return_buf[CY_CMD_OP_GET_PARAM_RET_SZ];
	int rc;

	cmd_buf[0] = CY_CMD_OP_GET_PARA;
	cmd_buf[1] = 0x5a;	//  can name it CY_OP_PARA_COVER_MODE;

	pm_runtime_get_sync(dev);

	rc = request_exclusive(cd, cd->core, CY_CORE_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: fail get exclusive ex=%p own=%p\n",
			   __func__, cd->exclusive_dev, cd->core);
		goto exit_put;
	}

	rc = cyttsp4_exec_cmd(cd, CY_MODE_OPERATIONAL,
			      cmd_buf, sizeof(cmd_buf),
			      return_buf, sizeof(return_buf),
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: exec cmd error.\n", __func__);
		goto exit_release;
	}

	tp_log_info("%s: return_buf=0x%x,0x%x,0x%x\n", __func__,
		    return_buf[0], return_buf[1], return_buf[2]);

	if (return_buf[0] != 0x5a) {
		tp_log_err("%s: return data error.\n", __func__);
		rc = -EINVAL;
		goto exit_release;
	}

	rc = return_buf[2];

exit_release:
	if (release_exclusive(cd, cd->core) < 0)
		tp_log_err("%s: fail to release exclusive\n", __func__);
exit_put:
	pm_runtime_put(dev);
	return scnprintf(buf, CY_MAX_PRBUF_SIZE, "%d\n", rc);
}

/*For this store,echo 2 will close this feature,echo 1 will open this feature,
  for framework to control easy, echo 1 will open,echo 0 will close,we change
  the value in driver*/
static ssize_t holster_set_sensitivity_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t size)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	unsigned long cover_mode_val;
	u8 cmd_buf[CY_CMD_OP_SET_PARAM_CMD_SZ];
	u8 return_buf[CY_CMD_OP_SET_PARAM_RET_SZ];
	int rc;

	rc = kstrtoul(buf, 10, &cover_mode_val);
	if (rc < 0) {
		tp_log_err("%s: Invalid value.\n", __func__);
		goto exit;
	}
	if (cover_mode_val != 1 && cover_mode_val != 0) {
		tp_log_err("%s: Invalid cover mode=%ld\n", __func__,
			   cover_mode_val);
		rc = -EINVAL;
		goto exit;
	}

	if (0 == cover_mode_val)
		cover_mode_val = 2;	//value 2 will close this feature
	holster_status = cover_mode_val;
	cmd_buf[0] = CY_CMD_OP_SET_PARA;
	cmd_buf[1] = 0x5a;	// can name it CY_OP_PARA_COVER_MODE
	cmd_buf[2] = 0x01;	// can name it CY_OP_PARA_COVER_MODE_SZ;
	cmd_buf[3] = (u8) cover_mode_val;

	mutex_lock(&cd->system_lock);
	if (cd->sleep_state == SS_SLEEP_OFF) {
		atomic_set(&holster_status_flag, SET_AT_LATE_RESUME_NEED_NOT);
		tp_log_info("%s,TP is awake now,operate directly\n", __func__);
	} else {
		atomic_set(&holster_status_flag, SET_AT_LATE_RESUME_NEEDED);
		tp_log_info
		    ("%s,TP is not awake,remember this status and do it at late_resume\n",
		     __func__);
		rc = size;
		mutex_unlock(&cd->system_lock);
		goto exit;
	}
	mutex_unlock(&cd->system_lock);


	pm_runtime_get_sync(dev);

	rc = request_exclusive(cd, cd->core, CY_CORE_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: fail get exclusive ex=%p own=%p\n",
			   __func__, cd->exclusive_dev, cd->core);
		goto exit_put;
	}

	rc = cyttsp4_exec_cmd(cd, CY_MODE_OPERATIONAL,
			      cmd_buf, sizeof(cmd_buf),
			      return_buf, sizeof(return_buf),
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: exec cmd error.\n", __func__);
		goto exit_release;
	}

	rc = size;
	tp_log_info("%s: return_buf=0x%x,0x%x,cmd = %ld\n", __func__,
		    return_buf[0], return_buf[1], cover_mode_val);
exit_release:
	if (release_exclusive(cd, cd->core) < 0)
		/* Don't return fail code, mode is already changed. */
		tp_log_err("%s: fail to release exclusive\n", __func__);
	else
		tp_log_debug("%s: pass release exclusive\n", __func__);
exit_put:
	pm_runtime_put(dev);
exit:
	return rc;
}

int set_holster_sensitivity(struct cyttsp4_core_data *cd, unsigned long status)
{
	unsigned long cover_mode_val;
	u8 cmd_buf[CY_CMD_OP_SET_PARAM_CMD_SZ];
	u8 return_buf[CY_CMD_OP_SET_PARAM_RET_SZ];
	int rc;

	cover_mode_val = status;

	cmd_buf[0] = CY_CMD_OP_SET_PARA;
	cmd_buf[1] = 0x5a;	// can name it CY_OP_PARA_COVER_MODE
	cmd_buf[2] = 0x01;	// can name it CY_OP_PARA_COVER_MODE_SZ;
	cmd_buf[3] = (u8) cover_mode_val;

	rc = request_exclusive(cd, cd->core, CY_CORE_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: fail get exclusive ex=%p own=%p\n",
			   __func__, cd->exclusive_dev, cd->core);
		goto exit;
	}

	rc = cyttsp4_exec_cmd(cd, CY_MODE_OPERATIONAL,
			      cmd_buf, sizeof(cmd_buf),
			      return_buf, sizeof(return_buf),
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: exec cmd error.\n", __func__);
		goto exit_release;
	}

	tp_log_info("%s: return_buf=0x%x,0x%x,cmd = %ld\n", __func__,
		    return_buf[0], return_buf[1], cover_mode_val);
exit_release:
	if (release_exclusive(cd, cd->core) < 0)
		/* Don't return fail code, mode is already changed. */
		tp_log_err("%s: fail to release exclusive\n", __func__);
	else
		tp_log_debug("%s: pass release exclusive\n", __func__);
exit:
	return rc;
}

/*
 * Show finger threshold value via sysfs
 */
static ssize_t cyttsp4_finger_threshold_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	u8 cmd_buf[CY_CMD_OP_GET_PARAM_CMD_SZ];
	u8 return_buf[CY_CMD_OP_GET_PARAM_RET_SZ];
	u8 finger_threshold_h, finger_threshold_l;
	int rc;

	cmd_buf[0] = CY_CMD_OP_GET_PARA;
	cmd_buf[1] = CY_OP_PARA_FINGER_THRESHOLD;

	pm_runtime_get_sync(dev);
	rc = request_exclusive(cd, cd->core, CY_CORE_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: fail get exclusive ex=%p own=%p\n",
			   __func__, cd->exclusive_dev, cd->core);
		goto exit_put;
	}

	rc = cyttsp4_exec_cmd(cd, CY_MODE_OPERATIONAL,
			      cmd_buf, sizeof(cmd_buf),
			      return_buf, sizeof(return_buf),
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: exec cmd error.\n", __func__);
		goto exit_release;
	}

	tp_log_debug("%s: return_buf=0x%x,0x%x,0x%x,0x%x\n", __func__,
		     return_buf[0], return_buf[1], return_buf[2],
		     return_buf[3]);

	if (return_buf[0] != CY_OP_PARA_FINGER_THRESHOLD) {
		tp_log_err("%s: return data error.\n", __func__);
		rc = -EINVAL;
		goto exit_release;
	}

	finger_threshold_h = return_buf[2];
	finger_threshold_l = return_buf[3];
	rc = merge_bytes(finger_threshold_h, finger_threshold_l);

exit_release:
	if (release_exclusive(cd, cd->core) < 0)
		tp_log_err("%s: fail to release exclusive\n", __func__);
exit_put:
	pm_runtime_put(dev);
	return scnprintf(buf, CY_MAX_PRBUF_SIZE, "%d\n", rc);
}

/*
 * change finger threshold via sysfs
 */
static ssize_t cyttsp4_finger_threshold_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t size)
{
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);
	unsigned long threshold_val;
	u8 cmd_buf[CY_CMD_OP_SET_PARAM_CMD_SZ];
	u8 return_buf[CY_CMD_OP_SET_PARAM_RET_SZ];
	int rc;

	rc = kstrtoul(buf, 10, &threshold_val);
	if (rc < 0) {
		tp_log_err("%s: Invalid value.\n", __func__);
		goto exit;
	}

	if ((threshold_val < CY_OP_PARA_FINGER_THRESHOLD_MIN_VAL) ||
	    (threshold_val > CY_OP_PARA_FINGER_THRESHOLD_MAX_VAL)) {
		tp_log_err("%s: Invalid value, value=%d.\n", __func__,
			   (int)threshold_val);
		rc = -EINVAL;
		goto exit;
	}

	cmd_buf[0] = CY_CMD_OP_SET_PARA;
	cmd_buf[1] = CY_OP_PARA_FINGER_THRESHOLD;
	cmd_buf[2] = CY_OP_PARA_FINGER_THRESHOLD_SZ;
	cmd_buf[3] = (u8) ((threshold_val >> 8) & 0xFF);
	cmd_buf[4] = (u8) (threshold_val & 0xFF);

	pm_runtime_get_sync(dev);
	rc = request_exclusive(cd, cd->core, CY_CORE_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: fail get exclusive ex=%p own=%p\n",
			   __func__, cd->exclusive_dev, cd->core);
		goto exit_put;
	}

	rc = cyttsp4_exec_cmd(cd, CY_MODE_OPERATIONAL,
			      cmd_buf, sizeof(cmd_buf),
			      return_buf, sizeof(return_buf),
			      CY_COMMAND_COMPLETE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: exec cmd error.\n", __func__);
		goto exit_release;
	}

	rc = size;
	tp_log_debug("%s: return_buf=0x%x,0x%x\n", __func__,
		     return_buf[0], return_buf[1]);

exit_release:
	if (release_exclusive(cd, cd->core) < 0)
		tp_log_err("%s: fail to release exclusive\n", __func__);
exit_put:
	pm_runtime_put(dev);
exit:
	return rc;
}

static struct device_attribute attributes[] = {
	__ATTR(ic_ver, S_IRUGO, cyttsp4_ic_ver_show, NULL),
	__ATTR(ttconfig_ver, S_IRUGO, cyttsp4_ttconfig_ver_show, NULL),
	__ATTR(drv_ver, S_IRUGO, cyttsp4_drv_ver_show, NULL),
	__ATTR(hw_reset, S_IWUSR, NULL, cyttsp4_hw_reset_store),
	__ATTR(hw_irq_stat, S_IRUSR, cyttsp4_hw_irq_stat_show, NULL),
	__ATTR(drv_irq, S_IRUSR | S_IWUSR, cyttsp4_drv_irq_show,
	       cyttsp4_drv_irq_store),
	__ATTR(drv_debug, S_IWUSR, NULL, cyttsp4_drv_debug_store),
	__ATTR(sleep_status, S_IRUSR, cyttsp4_sleep_status_show, NULL),
	__ATTR(easy_wakeup_gesture, S_IRUSR | S_IWUSR,
	       cyttsp4_easy_wakeup_gesture_show,
	       cyttsp4_easy_wakeup_gesture_store),
	__ATTR(signal_disparity, S_IRUGO | (S_IWUSR | S_IWGRP | S_IROTH),
	       cyttsp4_signal_disparity_show, cyttsp4_signal_disparity_store),
	__ATTR(holster_sensitivity, S_IRUGO | (S_IWUSR | S_IWGRP | S_IROTH),
	       holster_set_sensitivity_show, holster_set_sensitivity_store),
	__ATTR(finger_threshold, S_IRUGO | (S_IWUSR | S_IWGRP | S_IROTH),
	       cyttsp4_finger_threshold_show, cyttsp4_finger_threshold_store),
};

static int add_sysfs_interfaces(struct cyttsp4_core_data *cd,
				struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto undo;

	return 0;
undo:
	for (i--; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	tp_log_err("%s: failed to create sysfs interface\n", __func__);
	return -ENODEV;
}

static void remove_sysfs_interfaces(struct cyttsp4_core_data *cd,
				    struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
}

/*link to "/sys/touch_screen"*/
static int add_sensitivity_sysfs_interfaces(struct device *dev)
{
	int error = 0;
	struct kobject *properties_kobj;

	properties_kobj = tp_get_touch_screen_obj();
	if (NULL == properties_kobj) {
		tp_log_info("%s: failed to get kboj! \n", __func__);
		return -ENODEV;
	}

	error = sysfs_create_link(properties_kobj, &dev->kobj, "glove_func");
	if (error) {
		tp_log_err
		    ("%s: failed to create glove sysfs_link ,error = %d\n",
		     __func__, error);
		goto undo;
	}
	return 0;
undo:
	return -ENODEV;
}


static struct kobj_attribute easy_wakeup_gesture_attr = {
	.attr = {.name = "easy_wakeup_gesture",.mode = 0664},
	.show = cyttsp4_ts_easy_wakeup_gesture_show,
	.store = cyttsp4_ts_easy_wakeup_gesture_store,
};

static struct kobj_attribute easy_wakeup_supported_gestures_attr = {
	.attr = {.name = "easy_wakeup_supported_gestures",.mode = 0444},
	.show = cyttsp4_ts_supported_gesture_show,
	.store = NULL,
};

static int add_easy_wakeup_interfaces(struct device *dev)
{
	int error = 0;
	struct kobject *properties_kobj = NULL;

	properties_kobj = tp_get_touch_screen_obj();
	if (NULL == properties_kobj) {
		tp_log_err("%s: Error, get kobj failed!\n", __func__);
		return -1;
	}
	/*add the node touch_mmi_test for mmi test apk */
	error =
	    sysfs_create_file(properties_kobj, &easy_wakeup_gesture_attr.attr);
	if (error) {
		kobject_put(properties_kobj);
		tp_log_err("%s: easy_wakeup_gesture create file error\n",
			   __func__);
		return -ENODEV;
	}

	error =
	    sysfs_create_file(properties_kobj,
			      &easy_wakeup_supported_gestures_attr.attr);
	if (error) {
		kobject_put(properties_kobj);
		tp_log_err
		    ("%s: easy_wakeup_supported_gestures create file error\n",
		     __func__);
		return -ENODEV;
	}

	return 0;
}


static int cyttsp4_core_probe(struct cyttsp4_core *core)
{
	struct cyttsp4_core_data *cd;
	struct device *dev = &core->dev;
	struct cyttsp4_core_platform_data *pdata = dev_get_platdata(dev);
	enum cyttsp4_atten_type type;
	unsigned long irq_flags;
	int rc = 0;

	tp_log_info("%s: startup\n", __func__);
	tp_log_debug("%s: debug on\n", __func__);
	tp_log_debug("%s: verbose debug on\n", __func__);

	if (pdata == NULL) {
		tp_log_err("%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}

	/* get context and debug print buffers */
	cd = kzalloc(sizeof(*cd), GFP_KERNEL);
	if (cd == NULL) {
		tp_log_err("%s: Error, kzalloc\n", __func__);
		rc = -ENOMEM;
		goto error_alloc_data;
	}

	/* Initialize device info */
	cd->core = core;
	cd->dev = dev;
	cd->pdata = pdata;
#ifdef HUAWEI_SET_FINGER_MODE_BY_DEFAULT
	/* Initialize with Finger mode,not glove mode */
	cd->opmode = OPMODE_FINGER;
#endif
	cd->max_xfer = CY_DEFAULT_ADAP_MAX_XFER;
	if (pdata->max_xfer_len) {
		if (pdata->max_xfer_len < CY_ADAP_MIN_XFER) {
			tp_log_err("%s: max_xfer_len invalid (min=%d)\n",
				   __func__, CY_ADAP_MIN_XFER);
			rc = -EINVAL;
			goto error_max_xfer;
		}
		cd->max_xfer = pdata->max_xfer_len;
		tp_log_debug("%s: max_xfer set to %d\n",
			     __func__, cd->max_xfer);
	}

	/* Initialize mutexes and spinlocks */
	mutex_init(&cd->system_lock);
	mutex_init(&cd->adap_lock);
	spin_lock_init(&cd->spinlock);

	/* Initialize attention lists */
	for (type = 0; type < CY_ATTEN_NUM_ATTEN; type++)
		INIT_LIST_HEAD(&cd->atten_list[type]);

	/* Initialize wait queue */
	init_waitqueue_head(&cd->wait_q);

	/* Initialize works */
	INIT_WORK(&cd->startup_work, cyttsp4_startup_work_function);
	INIT_WORK(&cd->watchdog_work, cyttsp4_watchdog_work);

	/* Initialize IRQ */
	cd->irq = gpio_to_irq(pdata->irq_gpio);
	if (cd->irq < 0) {
		rc = -EINVAL;
		goto error_gpio_irq;
	}
	cd->irq_enabled = true;

	dev_set_drvdata(dev, cd);

	/* Call platform init function */
	if (cd->pdata->init) {
		tp_log_debug("%s: Init HW\n", __func__);
		rc = cd->pdata->init(cd->pdata, 1, cd->dev);
	} else {
		tp_log_info("%s: No HW INIT function\n", __func__);
		rc = 0;
	}
	if (rc < 0) {
		tp_log_err("%s: HW Init fail r=%d\n", __func__, rc);
	}

	rc = cyttsp4_reset_checkout(cd);
	if (rc < 0) {
		tp_log_err("%s: there is no cypress device!!! rc=%d\n",
			   __func__, rc);
		goto error_request_irq;
	}

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
	set_hw_dev_flag(DEV_I2C_TOUCH_PANEL);
#endif

#ifdef CYTTSP4_DETECT_HW
	/* Call platform detect function */
	if (cd->pdata->detect) {
		tp_log_debug("%s: Detect HW\n", __func__);
		rc = cd->pdata->detect(cd->pdata, cd->dev,
				       cyttsp4_platform_detect_read);
		if (rc) {
			tp_log_info("%s: No HW detected\n", __func__);
			rc = -ENODEV;
			goto error_detect;
		}
	}
#endif

	/* delete the fb_call set in core driver */

	tp_log_debug("%s: initialize threaded irq=%d\n", __func__, cd->irq);
	if (cd->pdata->level_irq_udelay > 0)
		/* use level triggered interrupts */
		irq_flags = IRQF_TRIGGER_LOW | IRQF_ONESHOT;
	else
		/* use edge triggered interrupts */
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;

	rc = request_threaded_irq(cd->irq, cyttsp4_hard_irq, cyttsp4_irq,
				  irq_flags, dev_name(dev), cd);
	if (rc < 0) {
		tp_log_err("%s: Error, could not request irq\n", __func__);
		goto error_request_irq;
	}

	/* Setup watchdog timer */
	setup_timer(&cd->watchdog_timer, cyttsp4_watchdog_timer,
		    (unsigned long)cd);

	pm_runtime_enable(dev);

	/*
	 * call startup directly to ensure that the device
	 * is tested before leaving the probe
	 */
	tp_log_debug("%s: call startup\n", __func__);

	/* After probe if any function starts make the startup state as ILLEGAL */
	mutex_lock(&cd->system_lock);
	cd->startup_state = STARTUP_NONE;
	mutex_unlock(&cd->system_lock);
	tp_log_info("%s: schedule startup!\n", __func__);
	/* As the startup takes long time do it after */
	schedule_work(&cd->startup_work);

	if (IS_TTSP_VER_GE(&cd->sysinfo, 2, 5)) {
		cd->easy_wakeup_gesture = pdata->easy_wakeup_gesture;
		cd->easy_wakeup_supported_gestures =
		    pdata->easy_wakeup_supported_gestures;
	} else {
		cd->easy_wakeup_gesture = 0xFF;
		cd->easy_wakeup_supported_gestures = 0;
	}

	if (IS_TTSP_VER_GE(&cd->sysinfo, 2, 5))
		cd->easy_wakeup_gesture = pdata->easy_wakeup_gesture;
	else
		cd->easy_wakeup_gesture = 0xFF;

	tp_log_debug("%s: add sysfs interfaces\n", __func__);
	rc = add_sysfs_interfaces(cd, dev);
	if (rc < 0) {
		tp_log_err("%s: Error, fail sysfs init\n", __func__);
		goto error_startup;
	}

	/*whether support glove function */
	if (cd->pdata->use_configure_sensitivity) {
		rc = add_sensitivity_sysfs_interfaces(dev);
		if (rc < 0) {
			tp_log_err("%s: Error, fail sensitivity sysfs init\n",
				   __func__);
			goto error_startup;
		}
	}

	rc = add_easy_wakeup_interfaces(dev);
	if (rc < 0) {
		tp_log_err("%s: Error, fail easy wakeup init\n", __func__);
		goto error_startup;
	}

	core_dev_ptr = dev;

	device_init_wakeup(dev, 1);
	atomic_set(&holster_status_flag, SET_AT_LATE_RESUME_NEED_NOT);
	tp_log_info("%s: ok\n", __func__);
	return 0;

error_startup:
	cancel_work_sync(&cd->startup_work);
	cyttsp4_stop_wd_timer(cd);
	pm_runtime_disable(dev);
	cyttsp4_free_si_ptrs(cd);
	free_irq(cd->irq, cd);
error_request_irq:
#ifdef CYTTSP4_DETECT_HW
error_detect:
#endif
	if (pdata->init)
		pdata->init(pdata, 0, dev);
	dev_set_drvdata(dev, NULL);
error_gpio_irq:
error_max_xfer:
	kfree(cd);
error_alloc_data:
error_no_pdata:
	tp_log_err("%s failed.\n", __func__);
	return rc;
}

static int cyttsp4_core_release(struct cyttsp4_core *core)
{
	struct device *dev = &core->dev;
	struct cyttsp4_core_data *cd = dev_get_drvdata(dev);

	tp_log_debug("%s\n", __func__);

	/*
	 * Suspend the device before freeing the startup_work and stopping
	 * the watchdog since sleep function restarts watchdog on failure
	 */
	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);

	cancel_work_sync(&cd->startup_work);

	cyttsp4_stop_wd_timer(cd);

	remove_sysfs_interfaces(cd, dev);
	free_irq(cd->irq, cd);
	if (cd->pdata->init)
		cd->pdata->init(cd->pdata, 0, dev);
	/* delete the fb_call set in core driver */
	dev_set_drvdata(dev, NULL);
	cyttsp4_free_si_ptrs(cd);
	kfree(cd);
	return 0;
}

static struct cyttsp4_core_driver cyttsp4_core_driver = {
	.probe = cyttsp4_core_probe,
	.remove = cyttsp4_core_release,
	.subscribe_attention = cyttsp4_subscribe_attention_,
	.unsubscribe_attention = cyttsp4_unsubscribe_attention_,
	.request_exclusive = cyttsp4_request_exclusive_,
	.release_exclusive = cyttsp4_release_exclusive_,
	.request_reset = cyttsp4_request_reset_,
	.request_restart = cyttsp4_request_restart_,
	.request_set_mode = cyttsp4_request_set_mode_,
	.request_sysinfo = cyttsp4_request_sysinfo_,
	.request_loader_pdata = cyttsp4_request_loader_pdata_,
	.request_handshake = cyttsp4_request_handshake_,
	.request_exec_cmd = cyttsp4_request_exec_cmd_,
	.request_stop_wd = cyttsp4_request_stop_wd_,
	.request_toggle_lowpower = cyttsp4_request_toggle_lowpower_,
	.request_power_state = cyttsp4_request_power_state_,
	.request_config_row_size = cyttsp4_request_config_row_size_,
	.request_write_config = cyttsp4_request_write_config_,
	.request_enable_scan_type = cyttsp4_request_enable_scan_type_,
	.request_disable_scan_type = cyttsp4_request_disable_scan_type_,
	.get_security_key = cyttsp4_get_security_key_,
	.get_touch_record = cyttsp4_get_touch_record_,
	.write = cyttsp4_write_,
	.read = cyttsp4_read_,
	.driver = {
		   .name = CYTTSP4_CORE_NAME,
		   .bus = &cyttsp4_bus_type,
		   .owner = THIS_MODULE,
		   .pm = &cyttsp4_core_pm_ops,
		   },
};


static int __init cyttsp4_core_init(void)
{
	int rc = 0;

	rc = cyttsp4_register_core_driver(&cyttsp4_core_driver);
	tp_log_info("%s: Cypress TTSP v4 core driver (Built %s) rc=%d\n",
		    __func__, CY_DRIVER_DATE, rc);
	return rc;
}

module_init(cyttsp4_core_init);

static void __exit cyttsp4_core_exit(void)
{
	cyttsp4_unregister_core_driver(&cyttsp4_core_driver);
	tp_log_info("%s: module exit\n", __func__);
}

module_exit(cyttsp4_core_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard touchscreen core driver");
MODULE_AUTHOR("Aleksej Makarov <aleksej.makarov@sonyericsson.com>");

