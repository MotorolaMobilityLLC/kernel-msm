/*
 * Core Source for:
 * Cypress TrueTouch(TM) Standard Product (TTSP) touchscreen drivers.
 * For use with Cypress Txx3xx parts.
 * Supported parts include:
 * CY8CTST341
 * CY8CTMA340
 *
 * Copyright (C) 2009-2011 Cypress Semiconductor, Inc.
 * Copyright (C) 2010-2012 Motorola Mobility, Inc.
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
 * Contact Cypress Semiconductor at www.cypress.com <kev@cypress.com>
 *
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include "cyttsp3_core.h"
#include <linux/version.h>	/* Required for kernel version checking */
#include <linux/firmware.h>	/* This enables firmware class loader code */
#include <linux/regulator/consumer.h>
#include <linux/types.h>

#define CY_USE_HW_RESET
#define CY3_NUM_RETRY	25

/* Soft reset here is for platforms with no hw reset line */
#ifdef CY_USE_HW_RESET
#define _cyttsp_soft_reset(ts)
#else
#define _cyttsp_soft_reset(ts) {\
	int rc;\
	u8 cmd = CY_SOFT_RESET_MODE;\
	rc = ttsp_write_block_data(ts, CY_REG_BASE, sizeof(cmd), &cmd);\
	if (rc < 0)\
		return rc;\
}
#endif

/* Bootloader File 0 offset */
#define CY_BL_FILE0       0x00
/* Bootloader command directive */
#define CY_BL_CMD         0xFF
/* Bootloader Exit and Verify Checksum command */
#define CY_BL_EXIT        0xA5
/* Bootloader number of command keys */
#define CY_NUM_BL_KEYS    8
/* Bootloader default command keys */
#define CY_BL_KEY0 0
#define CY_BL_KEY1 1
#define CY_BL_KEY2 2
#define CY_BL_KEY3 3
#define CY_BL_KEY4 4
#define CY_BL_KEY5 5
#define CY_BL_KEY6 6
#define CY_BL_KEY7 7

/* helpers */
#define GET_NUM_TOUCHES(x)          ((x) & 0x0F)
#define IS_LARGE_AREA(x)            (((x) & 0x10) >> 4)
#define IS_BAD_PKT(x)               ((x) & 0x20)
#define IS_VALID_APP(x)             ((x) & 0x01)
#define IS_OPERATIONAL_ERR(x)       ((x) & 0x3F)
#define GET_HSTMODE(reg)            ((reg & 0x70) >> 4)
#define IS_BOOTLOADERMODE(reg)      ((reg & 0x10) >> 4)
#define BL_WATCHDOG_DETECT(reg)     (reg & 0x02)

/* maximum number of concurrent tracks */
#define CY_NUM_TCH_ID               4
/* maximum number of track IDs */
#define CY_NUM_TRK_ID               16

#define CY_NTCH                     0 /* lift off */
#define CY_TCH                      1 /* touch down */
#define CY_SMALL_TOOL_WIDTH         10
#define CY_LARGE_TOOL_WIDTH         255
#define CY_REG_BASE                 0x00
#define CY_REG_OP_START             0x1B
#define CY_REG_OP_END               0x1F
#define CY_REG_SI_START             0x17
#define CY_REG_SI_END               0x1F
#define CY_REG_ACT_DIST             0x1E
#define CY_REG_ACT_INTRVL           0x1D
#define CY_REG_TCH_TMOUT            (CY_REG_ACT_INTRVL+1)
#define CY_REG_LP_INTRVL            (CY_REG_TCH_TMOUT+1)
#define CY_RW_REGID_MAX             0x1F
#define CY_RW_REG_DATA_MAX          0xFF
#define CY_MAXZ                     255
#define CY_DELAY_DFLT               20 /* ms */
#define CY_DELAY_MAX                (500/CY_DELAY_DFLT) /* half second */
#define CY_REBASELINE_MAX           7 /* worst case rebaseline - half seconds */
#define CY_ACT_DIST_DFLT            0xF0
#define CY_ACT_DIST_BITS            0x0F
#define CY_HNDSHK_BIT               0x80
#define CY_HST_MODE_CHANGE_BIT      0x08
/* device mode bits */
#define CY_OPERATE_MODE             0x00
#define CY_SYSINFO_MODE             0x10
/* power mode select bits */
#define CY_SOFT_RESET_MODE          0x01 /* return to Bootloader mode */
#define CY_DEEP_SLEEP_MODE          0x02
#define CY_LOW_POWER_MODE           0x04
/* abs settings */
#define CY_NUM_ABS_SET              5 /* number of abs values per setting */
/* abs value offset */
#define CY_SIGNAL_OST               0
#define CY_MIN_OST                  1
#define CY_MAX_OST                  2
#define CY_FUZZ_OST                 3
#define CY_FLAT_OST                 4
/* axis signal offset */
#define CY_ABS_X_OST                0
#define CY_ABS_Y_OST                1
#define CY_ABS_P_OST                2
#define CY_ABS_W_OST                3
#define CY_ABS_ID_OST               4
#define CY_IGNORE_VALUE             0xFFFF

#define HI_TRACKID(reg)        ((reg & 0xF0) >> 4)
#define LO_TRACKID(reg)        ((reg & 0x0F) >> 0)

#define CY_BL_PAGE_SIZE		16
#define CY_BL_NUM_PAGES		5
#define CY_BL_BUSY		0x80
#define CY_BL_READY_NO_APP	0x10
#define CY_BL_READY_APP		0x11
#define CY_BL_RUNNING		0x20
#define CY_BL_NOERR		0x00
#define CY_BL_MAX_DATA_LEN	(CY_BL_PAGE_SIZE * 2)
#define CY_BL_ENTER_CMD_SIZE	11
#define CY_BL_EXIT_CMD_SIZE	11
#define CY_BL_WR_BLK_CMD_SIZE	79
#define CY_BL_VERS_SIZE		9
#define CY_BL_FW_NAME_SIZE	NAME_MAX
#define CY_MAX_PRBUF_SIZE	PIPE_BUF
#define CY_IRQ_DEASSERT		1
#define CY_IRQ_ASSERT		0

struct cyttsp_vers {
	u8 tts_verh;
	u8 tts_verl;
	u8 app_idh;
	u8 app_idl;
	u8 app_verh;
	u8 app_verl;
	u8 cid[3];
};

struct cyttsp_bin_image {
	u8 vers_size;
	struct cyttsp_vers vers;
	u8 *img;
	u32 size;
};

struct cyttsp_settings {
	const uint8_t	*data;
	uint8_t			size;
	uint8_t			tag;
};

struct cyttsp_firmware {
	const uint8_t	*img;
	uint32_t		size;
	const uint8_t	*ver;
	uint8_t			vsize;
};

struct cyttsp_framework {
	const uint16_t	*abs;
	uint8_t			size;
	uint8_t			enable_vkeys;
};

struct cyttsp_platform_data {
	struct cyttsp_settings	*sett[256];
	struct cyttsp_firmware	*fw;
	struct cyttsp_framework	*frmwrk;

	int reset_gpio;
	u32 reset_gpio_flags;

	int irq_gpio;

	uint8_t			addr[2];
	uint16_t		flags;

	int			(*hw_reset)(void);
	int			(*hw_recov)(int);
	int			(*irq_stat)(void);
};

/* Kept for legacy flag tracking */
enum cyttsp_sett_flags {
	CY_USE_HNDSHK = 0x01, CY_USE_SLEEP = 0x02, CY_FORCE_LOAD = 0x04,
};

enum cyttsp_powerstate {
	CY_IDLE_STATE, /* IC cannot be reached */
	CY_READY_STATE, /* pre-operational; ready to go to ACTIVE */
	CY_ACTIVE_STATE, /* app is running, IC is scanning */
	CY_LOW_PWR_STATE, /* not currently used  */
	CY_SLEEP_STATE, /* app is running, IC is idle */
	CY_BL_STATE, /* bootloader is running */
	CY_LDR_STATE, /* loader is running */
	CY_SYSINFO_STATE, /* switching to sysinfo mode */
	CY_INVALID_STATE
/* always last in the list */
};

static char *cyttsp_powerstate_string[] = {
/* Order must match enum cyttsp_powerstate above */
"IDLE", "READY", "ACTIVE", "LOW_PWR", "SLEEP", "BOOTLOADER", "LOADER",
		"SYSINFO", "INVALID" };

enum cyttsp_ic_grpnum {
	CY_IC_GRPNUM_RESERVED = 0,
	CY_IC_GRPNUM_OP_TAG, /* Platform Data Operational tagged registers */
	CY_IC_GRPNUM_SI_TAG, /* Platform Data Sysinfo tagged registers */
	CY_IC_GRPNUM_BL_KEY, /* Platform Data Bootloader Keys */
	CY_IC_GRPNUM_OP_REG, /* general Operational registers read/write */
	CY_IC_GRPNUM_SI_REG, /* general Sysinfo registers read/write */
	CY_IC_GRPNUM_CT_REG, /* general Config/Test registers read/write */
	CY_IC_GRPNUM_BL_REG, /* general Bootloader registers read/write */
	CY_IC_GRPNUM_NUM
/* always last */
};

enum cyttsp_op_registers {
	CY_OP_REG_RESERVED = 0, CY_OP_REG_ACT_DIST,
};

enum cyttsp_si_registers {
	CY_SI_REG_RESERVED = 0,
	CY_SI_REG_ACT_INTRVL,
	CY_SI_REG_TCH_TMOUT,
	CY_SI_REG_LP_INTRVL,
};

/* Touch structure */
struct cyttsp_touch {
	__be16 x;
	__be16 y;
	u8 z;
}__attribute__((packed));

/* TrueTouch Standard Product Gen3 interface definition */
struct cyttsp_xydata {
	u8 hst_mode;
	u8 tt_mode;
	u8 tt_stat;
	struct cyttsp_touch tch1;
	u8 touch12_id;
	struct cyttsp_touch tch2;
	u8 gest_cnt;
	u8 gest_id;
	struct cyttsp_touch tch3;
	u8 touch34_id;
	struct cyttsp_touch tch4;
	u8 tt_undef[3];
	u8 act_dist;
	u8 tt_reserved;
}__attribute__((packed));

/* TTSP System Information interface definition */
struct cyttsp_sysinfo_data {
	u8 hst_mode;
	u8 mfg_cmd;
	u8 mfg_stat;
	u8 cid[3];
	u8 tt_undef1;
	u8 uid[8];
	u8 bl_verh;
	u8 bl_verl;
	u8 tts_verh;
	u8 tts_verl;
	u8 app_idh;
	u8 app_idl;
	u8 app_verh;
	u8 app_verl;
	u8 tt_undef[5];
	u8 scn_typ;
	u8 act_intrvl;
	u8 tch_tmout;
	u8 lp_intrvl;
};

/* TTSP Bootloader Register Map interface definition */
#define CY_BL_CHKSUM_OK 0x01
struct cyttsp_bootloader_data {
	u8 bl_file;
	u8 bl_status;
	u8 bl_error;
	u8 blver_hi;
	u8 blver_lo;
	u8 bld_blver_hi;
	u8 bld_blver_lo;
	u8 ttspver_hi;
	u8 ttspver_lo;
	u8 appid_hi;
	u8 appid_lo;
	u8 appver_hi;
	u8 appver_lo;
	u8 cid_0;
	u8 cid_1;
	u8 cid_2;
};

struct cyttsp_tch {
	struct cyttsp_touch *tch;
	u8 *id;
};

struct cyttsp_trk {
	bool tch;
	int abs[CY_NUM_ABS_SET];
};

struct cyttsp {
	struct device *dev;
	int irq;
	struct input_dev *input;
	struct mutex data_lock; /* Used to prevent concurrent access */
	struct mutex startup_mutex; /* protect power on sequence */
	struct workqueue_struct *cyttsp_wq;
	struct work_struct cyttsp_resume_startup_work;
	char phys[32];
	const struct bus_type *bus_type;
	struct cyttsp_platform_data *platform_data;
	struct cyttsp_bus_ops *bus_ops;
	struct cyttsp_xydata xy_data;
	struct cyttsp_bootloader_data bl_data;
	struct cyttsp_sysinfo_data sysinfo_data;
	struct cyttsp_trk prv_trk[CY_NUM_TRK_ID];
	struct cyttsp_tch tch_map[CY_NUM_TCH_ID];
	struct completion bl_int_running;
	struct completion si_int_running;
	struct completion op_int_running;
	struct completion ld_int_running;
	enum cyttsp_powerstate power_state;
	bool irq_enabled;
	bool waiting_for_fw;
	bool powered;
	bool was_suspended;
	char *fwname;
	u16 flags;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	int ic_grpnum;
	int ic_grpoffset;
	int ic_grpstart[CY_IC_GRPNUM_NUM];
	bool used_ic_reflash;
#endif
};

struct cyttsp_track_data {
	struct cyttsp_trk cur_trk[CY_NUM_TRK_ID];
};

static const u8 bl_command[] = { CY_BL_FILE0, CY_BL_CMD, CY_BL_EXIT, CY_BL_KEY0,
		CY_BL_KEY1, CY_BL_KEY2, CY_BL_KEY3, CY_BL_KEY4, CY_BL_KEY5,
		CY_BL_KEY6, CY_BL_KEY7 };

static int _cyttsp_startup(struct cyttsp *ts);
static int _cyttsp_wakeup(struct cyttsp *ts);

#define CYT_BRINGUP

#ifdef CYT_BRINGUP
#include "cyttsp3_dinara.h"

struct cyttsp_settings s1 = {
	.data = tsett1,
	.size = ARRAY_SIZE(tsett1),
	.tag = 3
};

struct cyttsp_settings s2 = {
	.data = tsett2,
	.size = ARRAY_SIZE(tsett2),
	.tag = 6
};

struct cyttsp_settings s3 = {
	.data = tsett3,
	.size = ARRAY_SIZE(tsett3),
	.tag = 0
};

struct cyttsp_framework frmw = {
	.abs = abs,
	.size = ARRAY_SIZE(abs),
	.enable_vkeys = 1
};

struct cyttsp_firmware fw = {
	.img = fw_img,
	.size = sizeof(fw_img),
	.ver = ver,
	.vsize = sizeof(ver)
};

static struct regulator *lvs1;
static struct regulator *l19;
static void init_platform_settings(struct device *dev, struct cyttsp_platform_data *pdata)
{
	int i;

	// These are hard-coded for now, as this driver is temporary and
	// only intended for bring-up
	
	lvs1 = regulator_get(dev, "8226_lvs1");
    if (IS_ERR(lvs1)) {
		pr_err("%s: regulator_get failed for lvs1\n", __func__);
	} else {
		regulator_enable(lvs1);
	}

	l19 = regulator_get(dev, "8226_l19");
    if (IS_ERR(l19)) {
		pr_err("%s: regulator_get failed for l19\n", __func__);
	} else {
		regulator_enable(l19);
	}

	pdata->reset_gpio = 16;
	pdata->irq_gpio = 17;

	for (i = 0; i < ARRAY_SIZE(pdata->sett); i++)
		pdata->sett[i] = NULL;

	pdata->sett[1] = &s1;
	pdata->sett[2] = &s2;
	pdata->sett[3] = &s3;

	pdata->frmwrk = &frmw;

	pdata->fw = &fw;

}

static void exit_platform_settings(struct device *dev)
{
	regulator_disable(l19);
	regulator_put(l19);
	regulator_disable(lvs1);
	regulator_put(lvs1);
}
#endif

static int hw_reset(struct cyttsp *ts) {
	gpio_set_value(ts->platform_data->reset_gpio, 0);
	msleep(9);
	gpio_set_value(ts->platform_data->reset_gpio, 1);
	return (0);
}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
static int cyttsp3_irq_stat(struct cyttsp *ts) {
	return (gpio_get_value(ts->platform_data->irq_gpio));
}
#endif

static void cyttsp_pr_state(struct cyttsp *ts) {
	pr_info(
			"%s: %s\n",
			__func__,
			ts->power_state <= CY_INVALID_STATE ?
			cyttsp_powerstate_string[ts->power_state] : "ERROR");
}

static int ttsp_read_block_data(struct cyttsp *ts, u8 command, u8 length,
		void *buf) {
	int retval;
	int tries;

	if (buf == NULL || length == 0) {
		pr_err("%s: No data passed\n", __func__);
		return -EINVAL;
	}

	for (tries = 0, retval = -1; (tries < CY3_NUM_RETRY) && (retval < 0);
			tries++) {
		retval = ts->bus_ops->read(ts->bus_ops, command, length, buf);
		if (retval < 0) {
			/*
			 * SPI can require retries based on data content
			 * I2C should not have errors
			 */
			msleep(CY_DELAY_DFLT);
		}
	}

	if (retval < 0) {
		pr_err("%s: bus read block data fail (ret=%d)\n",
				__func__, retval);
	}
	return retval;
}

static int ttsp_write_block_data(struct cyttsp *ts, u8 command, u8 length,
		const void *buf) {
	int retval;
	int tries;

	if (buf == NULL || length == 0) {
		pr_err("%s: No data passed\n", __func__);
		return -EINVAL;
	}

	for (tries = 0, retval = -1; (tries < CY3_NUM_RETRY) && (retval < 0);
			tries++) {
		retval = ts->bus_ops->write(ts->bus_ops, command, length, buf);
		if (retval < 0) {
			/*
			 * SPI can require retries based on data content
			 * I2C should not have errors
			 */
			msleep(CY_DELAY_DFLT);
		}
	}

	if (retval < 0) {
		pr_err("%s: bus write block data fail (ret=%d)\n",
				__func__, retval);
	}
	return retval;
}

static int _cyttsp_hndshk(struct cyttsp *ts, u8 hst_mode) {
	int retval;
	u8 cmd;

	cmd = hst_mode & CY_HNDSHK_BIT ?
			hst_mode & ~CY_HNDSHK_BIT : hst_mode | CY_HNDSHK_BIT;

	retval = ttsp_write_block_data(ts, CY_REG_BASE, sizeof(cmd),
			(u8 *) &cmd);

	if (retval < 0) {
		pr_err("%s: bus write fail on handshake (ret=%d)\n",
				__func__, retval);
	}

	return retval;
}

static int _cyttsp_load_bl_regs(struct cyttsp *ts) {
	int retval;

	memset(&(ts->bl_data), 0, sizeof(struct cyttsp_bootloader_data));

	retval = ttsp_read_block_data(ts, CY_REG_BASE, sizeof(ts->bl_data),
			&(ts->bl_data));

	if (retval < 0) {
		pr_err("%s: bus fail reading Bootloader regs (ret=%d)\n",
				__func__, retval);
		/*
		 * Calling process determines state change requirement
		 */
		goto cyttsp_load_bl_regs_exit;
	}

	if (IS_BOOTLOADERMODE(ts->bl_data.bl_status)) {
		cyttsp_dbg(
				ts,
				CY_DBG_LVL_2,
				"%s: Bootloader Regs:\n"
				"  file=%02X status=%02X error=%02X\n"
				"  BL Version:          0x%02X%02X\n"
				"  Build BL Version:    0x%02X%02X\n"
				"  TTSP Version:        0x%02X%02X\n"
				"  Application ID:      0x%02X%02X\n"
				"  Application Version: 0x%02X%02X\n"
				"  Custom ID:           0x%02X%02X%02X\n",
				__func__, ts->bl_data.bl_file,
				ts->bl_data.bl_status, ts->bl_data.bl_error,
				ts->bl_data.blver_hi, ts->bl_data.blver_lo,
				ts->bl_data.bld_blver_hi,
				ts->bl_data.bld_blver_lo,
				ts->bl_data.ttspver_hi, ts->bl_data.ttspver_lo,
				ts->bl_data.appid_hi, ts->bl_data.appid_lo,
				ts->bl_data.appver_hi, ts->bl_data.appver_lo,
				ts->bl_data.cid_0, ts->bl_data.cid_1,
				ts->bl_data.cid_2);
	} else {
		cyttsp_dbg(
				ts,
				CY_DBG_LVL_2,
				"%s: Not Bootloader mode:\n"
				"  mode=%02X status=%02X error=%02X\n",
				__func__, ts->bl_data.bl_file,
				ts->bl_data.bl_status, ts->bl_data.bl_error);
	}

	cyttsp_load_bl_regs_exit: return retval;
}

static int _cyttsp_exit_bl_mode(struct cyttsp *ts) {
	unsigned long timeout;
	unsigned long uretval;
	int retval = 0;
	int tries = 0;
	u8 bl_cmd[sizeof(bl_command)];

	memcpy(bl_cmd, bl_command, sizeof(bl_command));
	if (ts->platform_data->sett[CY_IC_GRPNUM_BL_KEY] != NULL
		&& ts->platform_data->sett[CY_IC_GRPNUM_BL_KEY]->data != NULL) {
		memcpy( &bl_cmd[sizeof(bl_command)
			- ts->platform_data->sett[CY_IC_GRPNUM_BL_KEY]->size],
			ts->platform_data->sett[CY_IC_GRPNUM_BL_KEY]->data,
			ts->platform_data->sett[CY_IC_GRPNUM_BL_KEY]->size);
	}

	cyttsp_dbg( ts,
		CY_DBG_LVL_3,
		"%s: bl_cmd= "
		"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
		__func__, bl_cmd[0], bl_cmd[1], bl_cmd[2], bl_cmd[3], bl_cmd[4],
		bl_cmd[5], bl_cmd[6], bl_cmd[7], bl_cmd[8], bl_cmd[9],
		bl_cmd[10]);

	timeout = msecs_to_jiffies(CY_DELAY_DFLT * CY_DELAY_MAX);
	INIT_COMPLETION(ts->bl_int_running);
	retval = ttsp_write_block_data(ts, CY_REG_BASE, sizeof(bl_cmd),
			(void *) bl_cmd);
	if (retval < 0) {
		pr_err("%s: bus write fail exit Bootloader mode (ret=%d)\n",
				__func__, retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp_pr_state(ts);
		goto _cyttsp_exit_bl_mode_exit;
	}

	mutex_unlock(&ts->data_lock);
	uretval = wait_for_completion_interruptible_timeout(&ts->bl_int_running,
			timeout);
	mutex_lock(&ts->data_lock);

	if (uretval == 0) {
		pr_err("%s: Switch Operational Mode Timeout waiting"
		" for ready interrupt - try reading regs\n", __func__);
	}

	cyttsp_dbg(ts, CY_DBG_LVL_3,
		"%s: Switch Operational Mode: ret=%d uretval=%lu timeout=%lu",
		__func__, retval, uretval, timeout);

	/* wait for TTSP Device to complete switch to Operational mode */
	tries = 0;
	do {
		retval = _cyttsp_load_bl_regs(ts);
		if (retval < 0) {
			pr_err("%s: Fail read bootloader regs r=%d\n",
					__func__, retval);
			break;
		} else if (!IS_BOOTLOADERMODE(ts->bl_data.bl_status))
			break;
		else
			msleep(CY_DELAY_DFLT);
	} while (tries++ < CY_DELAY_MAX);

	cyttsp_dbg(ts, CY_DBG_LVL_3,
			"%s: check bl ready tries=%d ret=%d stat=%02X\n",
			__func__, tries, retval, ts->bl_data.bl_status);

	if ((retval < 0) || (tries >= CY_DELAY_MAX)) {
		pr_err("%s: Fail exit bootloader tries=%d max_tries=%d r=%d\n",
				__func__, tries, CY_DELAY_MAX, retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp_pr_state(ts);
		if (tries >= CY_DELAY_MAX)
			retval = -ETIMEDOUT;
	} else {
		ts->power_state = CY_READY_STATE;
		cyttsp_pr_state(ts);
		retval = 0;
	}

	_cyttsp_exit_bl_mode_exit: return retval;
}

static int _cyttsp_set_operational_mode(struct cyttsp *ts) {
	unsigned long timeout;
	unsigned long uretval;
	int retval;
	int tries;
	u8 cmd = CY_OPERATE_MODE + CY_HST_MODE_CHANGE_BIT;

	/* switch to operational mode */
	ts->power_state = CY_READY_STATE;
	cyttsp_pr_state(ts);
	timeout = msecs_to_jiffies(CY_DELAY_DFLT * CY_DELAY_MAX);
	INIT_COMPLETION(ts->op_int_running);
	retval = ttsp_write_block_data(ts, CY_REG_BASE, sizeof(cmd), &cmd);
	if (retval < 0) {
		pr_err("%s: write fail set Operational mode (ret=%d)\n",
				__func__, retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp_pr_state(ts);
		goto _cyttsp_set_operational_mode_exit;
	}
	/* wait for interrupt to set ready completion */
	mutex_unlock(&ts->data_lock);
	uretval = wait_for_completion_interruptible_timeout(&ts->op_int_running,
			timeout);
	mutex_lock(&ts->data_lock);

	if (uretval == 0) {
		pr_err("%s: Switch Operational Mode Timeout waiting"
		" for ready interrupt - try reading regs\n", __func__);
	}

	cyttsp_dbg(ts, CY_DBG_LVL_3,
		"%s: Switch Operational Mode: ret=%d uretval=%lu timeout=%lu",
		__func__, retval, uretval, timeout);

	/* wait for TTSP Device to complete switch to Operational mode */
	tries = 0;
	do {
		memset(&ts->xy_data, 0, sizeof(struct cyttsp_xydata));
		retval = ttsp_read_block_data(ts, CY_REG_BASE,
				sizeof(struct cyttsp_xydata), &ts->xy_data);
		if (retval < 0) {
			pr_err("%s: Op info access err (ret=%d)\n",
					__func__, retval);
			break;
		} else {
			retval = _cyttsp_hndshk(ts, ts->xy_data.hst_mode);
			if (retval < 0) {
				/* print an error and keep running */
				pr_err("%s: Fail sending handshake r=%d\n",
						__func__, retval);
				/*
				 * Prevent driver below from mistaking
				 * a handshake failure for a read block failure
				 */
				retval = 0;
			}
			if (!(ts->xy_data.hst_mode & CY_HST_MODE_CHANGE_BIT)
				&& (GET_HSTMODE(ts->xy_data.hst_mode)
					== GET_HSTMODE(CY_OPERATE_MODE)))
				break;
			else
				msleep(CY_DELAY_DFLT);
		}
	} while (tries++ < CY_DELAY_MAX);

	cyttsp_dbg( ts,
		CY_DBG_LVL_3,
		"%s: check op ready hst_mode=%02X tries=%d ret=%d dist=%02X\n",
		__func__, ts->xy_data.hst_mode, tries, retval,
		ts->xy_data.act_dist);

	if ((retval < 0) || (tries >= CY_DELAY_MAX)) {
		pr_err( "%s: fail enter Operational mode "
			"tries=%d ret=%d dist=%02X "
			"host mode bytes=%02X %02X %02X\n",
			__func__, tries, retval, ts->xy_data.act_dist,
			ts->xy_data.hst_mode, ts->xy_data.tt_mode,
			ts->xy_data.tt_stat);
		if (tries >= CY_DELAY_MAX) {
			/*
			 * Do not put the driver in BL state since
			 * the driver will not recover from this.
			 * The error above will show debugger
			 * that the IC has reset somehow.
			 */
			ts->power_state = CY_IDLE_STATE;
			cyttsp_pr_state(ts);
			retval = -EIO;
		}
	}

	_cyttsp_set_operational_mode_exit: return retval;
}

static int _cyttsp_set_sysinfo_mode(struct cyttsp *ts) {
	unsigned long timeout;
	unsigned long uretval;
	int tries = 0;
	int retval = 0;
	u8 cmd = CY_SYSINFO_MODE + CY_HST_MODE_CHANGE_BIT;

	memset(&(ts->sysinfo_data), 0, sizeof(struct cyttsp_sysinfo_data));

	/* switch to sysinfo mode */
	ts->power_state = CY_SYSINFO_STATE;
	cyttsp_pr_state(ts);
	timeout = msecs_to_jiffies(
			CY_DELAY_DFLT * CY_DELAY_MAX * CY_REBASELINE_MAX);
	INIT_COMPLETION(ts->si_int_running);
	retval = ttsp_write_block_data(ts, CY_REG_BASE, sizeof(cmd), &cmd);
	if (retval < 0) {
		pr_err("%s: write fail set Sysinfo mode (ret=%d)\n",
				__func__, retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp_pr_state(ts);
		goto _cyttsp_set_sysinfo_mode_exit;
	}

	/* wait for interrupt to set ready completion */
	mutex_unlock(&ts->data_lock);
	uretval = wait_for_completion_interruptible_timeout(&ts->si_int_running,
			timeout);
	mutex_lock(&ts->data_lock);

	if (uretval == 0) {
		pr_err("%s: Switch Sysinfo Mode Timeout waiting"
		" for ready interrupt - try reading regs\n", __func__);
	}

	cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: Switch Sysinfo Mode:"
	" ret=%d uretval=%lu timeout=%lu tries=%d",
			__func__, retval, uretval, timeout, tries);

	/* wait for TTSP Device to complete switch to Sysinfo mode */
	tries = 0;
	do {
		/* read sysinfo registers */
		memset(&(ts->sysinfo_data), 0,
				sizeof(struct cyttsp_sysinfo_data));
		retval = ttsp_read_block_data(ts, CY_REG_BASE,
				sizeof(ts->sysinfo_data),
				&(ts->sysinfo_data));
		if (retval < 0) {
			pr_err("%s: SysInfo access err (ret=%d)\n", __func__,
					retval);
			break;
		} else {
			retval = _cyttsp_hndshk(ts, ts->sysinfo_data.hst_mode);
			if (retval < 0) {
				/* print an error and keep running */
				pr_err("%s: Fail sending handshake r=%d\n",
						__func__, retval);
				/*
				 * Prevent driver below from mistaking
				 * a handshake failure for a read block failure
				 */
				retval = 0;
			}
			if (!(ts->sysinfo_data.hst_mode
						& CY_HST_MODE_CHANGE_BIT)
				&& (GET_HSTMODE(ts->sysinfo_data.hst_mode)
					== GET_HSTMODE(CY_SYSINFO_MODE)))
				break;
			else
				msleep(CY_DELAY_DFLT);
		}
	} while (tries++ < (CY_DELAY_MAX * CY_REBASELINE_MAX));

	cyttsp_dbg(
			ts,
			CY_DBG_LVL_3,
			"%s: check sysinfo ready hst_mode=%02X tries=%d ret=%d"
			" app_verh=%02X app_verl=%02X\n",
			__func__, ts->xy_data.hst_mode, tries, retval,
			ts->sysinfo_data.app_verh, ts->sysinfo_data.app_verl);

	if ((retval < 0) || (tries >= (CY_DELAY_MAX * CY_REBASELINE_MAX))) {
		pr_err("%s: Fail switch to Sysinfo mode r=%d\n", __func__,
				retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp_pr_state(ts);
		if (tries >= (CY_DELAY_MAX * CY_REBASELINE_MAX))
			retval = -ETIMEDOUT;
	} else {
		pr_info(
				"%s: hm=%02X tv=%02X%02X ai=0x%02X%02X "
				"av=0x%02X%02X ci=0x%02X%02X%02X\n",
				__func__, ts->sysinfo_data.hst_mode,
				ts->sysinfo_data.tts_verh,
				ts->sysinfo_data.tts_verl,
				ts->sysinfo_data.app_idh,
				ts->sysinfo_data.app_idl,
				ts->sysinfo_data.app_verh,
				ts->sysinfo_data.app_verl,
				ts->sysinfo_data.cid[0],
				ts->sysinfo_data.cid[1],
				ts->sysinfo_data.cid[2]);
	}

	_cyttsp_set_sysinfo_mode_exit: return retval;
}

static int _cyttsp_set_sysinfo_regs(struct cyttsp *ts) {
	int retval = 0;
	u8 size;
	u8 tag;
	u8 reg_offset;
	u8 data_len;

	if ((ts->platform_data->sett[CY_IC_GRPNUM_SI_TAG] == NULL)
		|| (ts->platform_data->sett[CY_IC_GRPNUM_SI_TAG]->data
			== NULL)) {
		cyttsp_dbg(ts, CY_DBG_LVL_3,
				"%s: Sysinfo data table missing--%s",
				__func__, "IC defaults will be used\n");
		goto cyttsp_set_sysinfo_regs_exit;
	}
	size = ts->platform_data->sett[CY_IC_GRPNUM_SI_TAG]->size;
	tag = ts->platform_data->sett[CY_IC_GRPNUM_SI_TAG]->tag;
	reg_offset = CY_REG_SI_START + tag;
	data_len = size - tag; /* Overflows if tag > size; checked below */

	/* Do bounds checking on the data */
	if (reg_offset > CY_REG_SI_END) {
		pr_err("%s: Sysinfo reg data starts out of bounds--%s",
				__func__, "IC defaults will be used\n");
		goto cyttsp_set_sysinfo_regs_exit;
	} else if (tag > size) { /* overflow if (size - tag) is negative */
		pr_err("%s: Invalid sysinfo data length--%s",
				__func__, "IC defaults will be used\n");
		goto cyttsp_set_sysinfo_regs_exit;
	} else if ((reg_offset + data_len - 1) > CY_REG_SI_END) {
		pr_err("%s: Sysinfo reg data overflow--%s",
				__func__, "IC defaults will be used\n");
		goto cyttsp_set_sysinfo_regs_exit;
	}

	retval = ttsp_write_block_data(ts, reg_offset, data_len,
		&(ts->platform_data->sett[CY_IC_GRPNUM_SI_TAG]->data[tag]));

	if (retval < 0) {
		pr_err("%s: write fail on SysInfo Regs (ret=%d)\n",
				__func__, retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp_pr_state(ts);
	} else {
		/* wait for register setup */
		msleep(CY_DELAY_DFLT);
	}

	cyttsp_set_sysinfo_regs_exit: return retval;
}

static int _cyttsp_hard_reset(struct cyttsp *ts) {
	unsigned long timeout;
	unsigned long uretval;
	int retval = 0;

	ts->power_state = CY_BL_STATE;
	cyttsp_pr_state(ts);

	_cyttsp_soft_reset(ts); /* Does nothing if CY_USE_HW_RESET defined */

	retval = hw_reset(ts);
	if (retval < 0) {
		pr_err("%s: fail on hard reset (ret=%d)\n", __func__, retval);
		goto _cyttsp_hard_reset_exit;
	}

	/* wait for interrupt to set ready completion */
	timeout = msecs_to_jiffies(CY_DELAY_DFLT * CY_DELAY_MAX);
	INIT_COMPLETION(ts->bl_int_running);

	mutex_unlock(&ts->data_lock);
	uretval = wait_for_completion_interruptible_timeout(&ts->bl_int_running,
			timeout);
	mutex_lock(&ts->data_lock);

	if (uretval == 0) {
		pr_err("%s: Hard reset timeout waiting for heartbeat\n",
				__func__);
		/* do not return error; continue in case of missed interrupt */
	}

	cyttsp_dbg(ts, CY_DBG_LVL_2,
			"%s: Hard Reset: ret=%d uretval=%lu timeout=%lu\n",
			__func__, retval, uretval, timeout);

	_cyttsp_hard_reset_exit: return retval;
}

static int _cyttsp_set_operational_regs(struct cyttsp *ts) {
	int retval = 0;
	u8 size;
	u8 tag;
	u8 reg_offset;
	u8 data_len;

	if ((ts->platform_data->sett[CY_IC_GRPNUM_OP_TAG] == NULL)
		|| (ts->platform_data->sett[CY_IC_GRPNUM_OP_TAG]->data
			== NULL)) {
		cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: Op data table missing--%s",
				__func__, "IC defaults will be used\n");
		goto cyttsp_set_op_regs_exit;
	}
	size = ts->platform_data->sett[CY_IC_GRPNUM_OP_TAG]->size;
	tag = ts->platform_data->sett[CY_IC_GRPNUM_OP_TAG]->tag;
	reg_offset = CY_REG_OP_START + tag;
	data_len = size - tag; /* Overflows if tag > size; checked below */

	/* Do bounds checking on the data */
	if (reg_offset > CY_REG_OP_END) {
		pr_err("%s: Op reg data starts out of bounds--%s\n",
				__func__, "IC defaults will be used");
		goto cyttsp_set_op_regs_exit;
	} else if (tag > size) { /* overflow if (size - tag) is negative */
		pr_err("%s: Invalid op data length--%s\n",
				__func__, "IC defaults will be used");
		goto cyttsp_set_op_regs_exit;
	} else if ((reg_offset + data_len - 1) > CY_REG_OP_END) {
		pr_err("%s: Op reg data overflow--%s\n",
				__func__, "IC defaults will be used");
		goto cyttsp_set_op_regs_exit;
	}

	retval = ttsp_write_block_data(ts, reg_offset, data_len,
			&(ts->platform_data->sett[CY_IC_GRPNUM_OP_TAG]->data[tag]));

	cyttsp_dbg(ts, CY_DBG_LVL_2, "%s: Write OP regs ret=%d\n",
			__func__, retval);

	if (retval < 0) {
		pr_err("%s: bus write fail on Op Regs (ret=%d)\n", __func__, retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp_pr_state(ts);
	}

	cyttsp_set_op_regs_exit: return retval;
}

/* map pointers to touch information to allow loop on get xy_data */
static void _cyttsp_init_tch_map(struct cyttsp *ts) {
	ts->tch_map[0].tch = &ts->xy_data.tch1;
	ts->tch_map[0].id = &ts->xy_data.touch12_id;
	ts->tch_map[1].tch = &ts->xy_data.tch2;
	ts->tch_map[1].id = &ts->xy_data.touch12_id;
	ts->tch_map[2].tch = &ts->xy_data.tch3;
	ts->tch_map[2].id = &ts->xy_data.touch34_id;
	ts->tch_map[3].tch = &ts->xy_data.tch4;
	ts->tch_map[3].id = &ts->xy_data.touch34_id;
}

/* read xy_data for all current touches */
static int _cyttsp_xy_worker(struct cyttsp *ts) {
	int retval = 0;
	u8 cur_tch;
	u8 tch;
	u8 id;
	int t;
	int num_sent;
	int signal;
	struct cyttsp_trk cur_trk[CY_NUM_TRK_ID];

	cyttsp_dbg( ts, CY_DBG_LVL_3, "%s: Enter...\n",__func__);
	/*
	 * Get event data from CYTTSP device.
	 * The event data includes all data
	 * for all active touches.
	 */
	memset(&ts->xy_data, 0, sizeof(struct cyttsp_xydata));
	retval = ttsp_read_block_data(ts, CY_REG_BASE,
			sizeof(struct cyttsp_xydata),
			&ts->xy_data);
	if (retval < 0) {
		pr_err("%s: read fail on operational reg r=%d--%s\n",
				__func__, retval, "restart will be scheduled");
		ts->power_state = CY_IDLE_STATE;
		cyttsp_pr_state(ts);
		goto _cyttsp_xy_worker_exit;
	}

	cyttsp_dbg( ts, CY_DBG_LVL_3, "%s: Got data\n",__func__);
/*
	cyttsp_dbg( ts,
		CY_DBG_LVL_1,
		"%s:\n"
		"  hm=%02X tm=%02X ts=%02X\n"
		"  X1=%04X Y1=%04X Z1=%02X ID12=%02X\n"
		"  X2=%04X Y2=%04X Z2=%02X\n"
		"  X3=%04X Y3=%04X Z3=%02X ID34=%02X\n"
		"  X4=%04X Y4=%04X Z4=%02X AD=%02X\n",
		__func__, ts->xy_data.hst_mode, ts->xy_data.tt_mode,
		ts->xy_data.tt_stat, be16_to_cpu(ts->xy_data.tch1.x),
		be16_to_cpu(ts->xy_data.tch1.y), ts->xy_data.tch1.z,
		ts->xy_data.touch12_id, be16_to_cpu(ts->xy_data.tch2.x),
		be16_to_cpu(ts->xy_data.tch2.y), ts->xy_data.tch2.z,
		be16_to_cpu(ts->xy_data.tch3.x),
		be16_to_cpu(ts->xy_data.tch3.y), ts->xy_data.tch3.z,
		ts->xy_data.touch34_id, be16_to_cpu(ts->xy_data.tch4.x),
		be16_to_cpu(ts->xy_data.tch4.y), ts->xy_data.tch4.z,
		ts->xy_data.act_dist);
*/
	/* provide flow control handshake */
	retval = _cyttsp_hndshk(ts, ts->xy_data.hst_mode);
	if (retval < 0) {
		pr_err("%s: handshake fail on operational reg r=%d\n",
				__func__, retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp_pr_state(ts);
		goto _cyttsp_xy_worker_exit;
	}

	/* determine number of currently active touches */
	cur_tch = GET_NUM_TOUCHES(ts->xy_data.tt_stat);

	/* check for any error conditions */
	if (IS_BAD_PKT(ts->xy_data.tt_mode)) {
		/*
		 * Packet data isn't ready for some reason.
		 * This isn't normal, but because we did a handshake,
		 * we'll get another interrupt later with the actual data.
		 * We just want to report to the log for debugging later.
		 */
		pr_err("%s: Packet not ready detected\n", __func__);
		retval = 0;
		goto _cyttsp_xy_worker_exit;
	} else if (IS_BOOTLOADERMODE(ts->xy_data.tt_mode)) {
		pr_err("%s: BL mode detected in active state\n", __func__);
		if (BL_WATCHDOG_DETECT(ts->xy_data.tt_mode))
			pr_err("%s: BL watchdog timeout detected\n", __func__);
		ts->power_state = CY_BL_STATE;
		cyttsp_pr_state(ts);
		queue_work(ts->cyttsp_wq, &ts->cyttsp_resume_startup_work);
		pr_info("%s: startup queued\n", __func__);
		goto _cyttsp_xy_worker_exit;
	} else if (GET_HSTMODE(ts->xy_data.hst_mode)
			== GET_HSTMODE(CY_SYSINFO_MODE)) {
		pr_err("%s: got SysInfo interrupt; expected touch "
		"(hst_mode=%02X)\n", __func__, ts->xy_data.hst_mode);
		retval = 0;
		goto _cyttsp_xy_worker_exit;
	} else if (IS_LARGE_AREA(ts->xy_data.tt_stat) == 1) {
		/* terminate all active tracks */
		cur_tch = CY_NTCH;
		cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: Large area detected\n", __func__);
	} else if (cur_tch == (CY_NUM_TRK_ID - 1)) {
		/* terminate all active tracks */
		cur_tch = CY_NTCH;
		pr_err("%s: Num touch error detected\n", __func__);
	} else if (cur_tch > CY_NUM_TCH_ID) {
		/* set cur_tch to maximum */
		cyttsp_dbg(ts, CY_DBG_LVL_2, "%s: Number of Touches %d set to max %d\n",
				__func__, cur_tch, CY_NUM_TCH_ID);
		cur_tch = CY_NUM_TCH_ID;
	}

	/* clear current touch tracking structures */
	memset(cur_trk, CY_NTCH, sizeof(cur_trk));

	/* extract xy_data for all currently reported touches */
	for (tch = 0; tch < cur_tch; tch++) {
		id = tch & 0x01 ?
				LO_TRACKID(*(ts->tch_map[tch].id)) :
				HI_TRACKID(*(ts->tch_map[tch].id));
		cur_trk[id].tch = CY_TCH;
		cur_trk[id].abs[CY_ABS_X_OST] =
			be16_to_cpu((ts->tch_map[tch].tch)->x);
		cur_trk[id].abs[CY_ABS_Y_OST] =
			be16_to_cpu((ts->tch_map[tch].tch)->y);
		cur_trk[id].abs[CY_ABS_P_OST] =
			(ts->tch_map[tch].tch)->z;
		cur_trk[id].abs[CY_ABS_W_OST] = CY_SMALL_TOOL_WIDTH;
		cur_trk[id].abs[CY_ABS_ID_OST] = id;
	}

	/* provide input event signaling for each active touch */
	for (id = 0, num_sent = 0; id < CY_NUM_TRK_ID; id++) {
		if (cur_trk[id].tch) {
			t = cur_trk[id].abs[CY_ABS_ID_OST];
			if ((t < ts->platform_data->frmwrk->abs[(CY_ABS_ID_OST
					* CY_NUM_ABS_SET) + CY_MIN_OST])
			|| (t > ts->platform_data->frmwrk->abs[(CY_ABS_ID_OST
					* CY_NUM_ABS_SET) + CY_MAX_OST])) {
				pr_err("%s: Touch=%d has bad"
				" track_id=%d\n", __func__, id, t);
				goto _cyttsp_xy_worker_skip_track;
			}
			signal = ts->platform_data->frmwrk->abs[(CY_ABS_ID_OST
					* CY_NUM_ABS_SET) + 0];
			if (signal != CY_IGNORE_VALUE) {
				/* send 0 based track id's */
				t -= ts->platform_data->frmwrk->abs
					[(CY_ABS_ID_OST * CY_NUM_ABS_SET)
					+ CY_MIN_OST];
				input_report_abs(ts->input, signal, t);
			}

			signal = ts->platform_data->frmwrk->abs[(CY_ABS_X_OST
					* CY_NUM_ABS_SET) + 0];
			if (signal != CY_IGNORE_VALUE)
				input_report_abs(ts->input, signal,
						cur_trk[id].abs[CY_ABS_X_OST]);

			signal = ts->platform_data->frmwrk->abs[(CY_ABS_Y_OST
					* CY_NUM_ABS_SET) + 0];
			if (signal != CY_IGNORE_VALUE)
				input_report_abs(ts->input, signal,
						cur_trk[id].abs[CY_ABS_Y_OST]);

			signal = ts->platform_data->frmwrk->abs[(CY_ABS_P_OST
					* CY_NUM_ABS_SET) + 0];
			if (signal != CY_IGNORE_VALUE)
				input_report_abs(ts->input, signal,
						cur_trk[id].abs[CY_ABS_P_OST]);

			signal = ts->platform_data->frmwrk->abs[(CY_ABS_W_OST
					* CY_NUM_ABS_SET) + 0];
			if (signal != CY_IGNORE_VALUE)
				input_report_abs(ts->input, signal,
						cur_trk[id].abs[CY_ABS_W_OST]);
			num_sent++;
			input_mt_sync(ts->input);
			if (ts->platform_data->frmwrk->abs[(CY_ABS_P_OST * CY_NUM_ABS_SET)
					+ 0] == ABS_MT_TOUCH_MAJOR) {
				/* pre-Gingerbread */
				ts->prv_trk[id] = cur_trk[id];
				ts->prv_trk[id].abs[CY_ABS_ID_OST] = t;
			} else
				ts->prv_trk[id].tch = CY_NTCH;
			cyttsp_dbg( ts,
				CY_DBG_LVL_1,
				"%s: ID:%3d  down\n",
				__func__, id);
/*
			cyttsp_dbg( ts, CY_DBG_LVL_1, "%s: ID:%3d  X:%3d  Y:%3d  "
					"Z:%3d  W=%3d  T=%3d\n",
					__func__, id,
					cur_trk[id].abs[CY_ABS_X_OST],
					cur_trk[id].abs[CY_ABS_Y_OST],
					cur_trk[id].abs[CY_ABS_P_OST],
					cur_trk[id].abs[CY_ABS_W_OST],
					t);
*/
		} else if (ts->prv_trk[id].tch) {
			signal = ts->platform_data->frmwrk->abs[(CY_ABS_ID_OST
					* CY_NUM_ABS_SET) + 0];
			if (signal != CY_IGNORE_VALUE)
				input_report_abs(ts->input, signal,
						ts->prv_trk[id].abs[CY_ABS_ID_OST]);

			signal = ts->platform_data->frmwrk->abs[(CY_ABS_X_OST
					* CY_NUM_ABS_SET) + 0];
			if (signal != CY_IGNORE_VALUE)
				input_report_abs(ts->input, signal,
						ts->prv_trk[id].abs[CY_ABS_X_OST]);

			signal = ts->platform_data->frmwrk->abs[(CY_ABS_Y_OST
					* CY_NUM_ABS_SET) + 0];
			if (signal != CY_IGNORE_VALUE)
				input_report_abs(ts->input, signal,
						ts->prv_trk[id].abs[CY_ABS_Y_OST]);

			signal = ts->platform_data->frmwrk->abs[(CY_ABS_P_OST
					* CY_NUM_ABS_SET) + 0];
			if (signal != CY_IGNORE_VALUE)
				input_report_abs(ts->input, signal, CY_NTCH);

			signal = ts->platform_data->frmwrk->abs[(CY_ABS_W_OST
					* CY_NUM_ABS_SET) + 0];
			if (signal != CY_IGNORE_VALUE)
				input_report_abs(ts->input, signal, CY_NTCH);
			num_sent++;
			input_mt_sync(ts->input);
			ts->prv_trk[id].tch = CY_NTCH;
			cyttsp_dbg( ts,
				CY_DBG_LVL_1,
				"%s: ID:%3d  liftoff\n",
				__func__, id);
/*
			cyttsp_dbg( ts,
				CY_DBG_LVL_1,
				"%s: ID:%3d  X:%3d  Y:%3d  "
				"Z:%3d  W=%3d  T=%3d liftoff\n",
				__func__, id, ts->prv_trk[id].abs[CY_ABS_X_OST],
				ts->prv_trk[id].abs[CY_ABS_Y_OST], CY_NTCH,
				CY_NTCH,
				ts->prv_trk[id].abs[CY_ABS_ID_OST]);
*/
		}
		_cyttsp_xy_worker_skip_track: continue;
	}

	if (num_sent == 0) {
		/* in case of 0-touch; all liftoff; Gingerbread+ */
		input_mt_sync(ts->input);
	}

	input_sync(ts->input);
_cyttsp_xy_worker_exit:
		usleep(1000);
		cyttsp_dbg( ts, CY_DBG_LVL_1,"%s: Exiting...",__func__);
		return retval;
}

static int _cyttsp_wait_bl_terminate(struct cyttsp *ts, int timeout_ms) {
	unsigned long timeout;
	unsigned long uretval;
	int retval = 0;

	/* wait for interrupt to set ready completion */
	timeout = msecs_to_jiffies(timeout_ms);
	mutex_unlock(&ts->data_lock);
	uretval = wait_for_completion_interruptible_timeout(&ts->ld_int_running,
			timeout);
	mutex_lock(&ts->data_lock);

	cyttsp_dbg(ts, CY_DBG_LVL_3,
			"%s: Loader Terminate: ret=%d uretval=%lu timeout=%lu\n",
			__func__, retval, uretval, timeout);

	retval = _cyttsp_load_bl_regs(ts);
	if (retval < 0) {
		pr_err("%s: Load BL regs err r=%d\n", __func__, retval);
		goto _cyttsp_wait_bl_terminate_exit;
	}

	if ((ts->bl_data.bl_status == CY_BL_READY_APP)
			&& (ts->bl_data.bl_error == CY_BL_NOERR))
		retval = 0;
	else if (uretval == 0) {
		pr_err("%s: BL loader terminate timeout err"
		" ret=%d uretval=%lu timeout=%lu\n",
				__func__, retval, uretval, timeout);
		retval = -ETIME;
		/* Caller will check if app is valid */
	} else {
		pr_err(
				"%s: BL loader terminate status err"
				" r=%d bl_f=%02X bl_s=%02X bl_e=%02X\n",
				__func__, retval, ts->bl_data.bl_file, ts->bl_data.bl_status, ts->bl_data.bl_error);
		retval = -EIO;
	}

	_cyttsp_wait_bl_terminate_exit: return retval;
}

static int _cyttsp_wait_bl_ready(struct cyttsp *ts, int timeout_ms) {
	unsigned long timeout;
	unsigned long uretval;
	int retval = 0;

	/* wait for interrupt to set ready completion */
	timeout = msecs_to_jiffies(timeout_ms);

	mutex_unlock(&ts->data_lock);
	uretval = wait_for_completion_interruptible_timeout(&ts->ld_int_running,
			timeout);
	mutex_lock(&ts->data_lock);

	if (uretval == 0) {
		pr_err("%s: Loader timeout waiting for loader ready\n", __func__);
		/* do not return error; continue in case of missed interrupt */
	}

	cyttsp_dbg(ts, CY_DBG_LVL_3,
			"%s: Loader Start: ret=%d uretval=%lu timeout=%lu\n",
			__func__, retval, uretval, timeout);

	retval = _cyttsp_load_bl_regs(ts);
	if (retval < 0) {
		pr_err("%s: Load BL regs err r=%d\n", __func__, retval);
		goto _cyttsp_wait_bl_ready_exit;
	}

	if (!(ts->bl_data.bl_status & CY_BL_BUSY)
			&& (ts->bl_data.bl_error == CY_BL_RUNNING))
		retval = 0;
	else if (uretval == 0) {
		pr_err("%s: BL loader startup timeout err"
		" ret=%d uretval=%lu timeout=%lu\n",
				__func__, retval, uretval, timeout);
		retval = -ETIME;
	} else {
		pr_err(
				"%s: BL loader startup status err"
				" r=%d bl_f=%02X bl_s=%02X bl_e=%02X\n",
				__func__, retval, ts->bl_data.bl_file, ts->bl_data.bl_status, ts->bl_data.bl_error);
		retval = -EIO;
	}

	_cyttsp_wait_bl_ready_exit: return retval;
}

static int _cyttsp_wait_bl_ready_no_stat(struct cyttsp *ts, int timeout_ms) {
	unsigned long timeout;
	unsigned long uretval;
	int retval = 0;

	/* wait for interrupt to set ready completion */
	timeout = msecs_to_jiffies(timeout_ms);

	mutex_unlock(&ts->data_lock);
	uretval = wait_for_completion_interruptible_timeout(&ts->ld_int_running,
			timeout);
	mutex_lock(&ts->data_lock);

	if (uretval == 0) {
		pr_err("%s: Loader timeout waiting for block ready; t=%lu\n",
				__func__, timeout);
		/* do not return error; continue in case of missed interrupt */
	}

	cyttsp_dbg(ts, CY_DBG_LVL_3,
			"%s: Loader Block: ret=%d uretval=%lu timeout=%lu\n",
			__func__, retval, uretval, timeout);

	/*
	 * loader must wait until complete chunk is written
	 * before trying to read the status registers
	 * treat timed out loops as missed interrupts
	 * and continue to end of chunk
	 */

	return retval;
}

static int _cyttsp_wr_blk_chunks(struct cyttsp *ts, u8 cmd, u8 length,
		const u8 *values) {
	int retval = 0;
	int block = 1;

	u8 dataray[CY_BL_MAX_DATA_LEN];

	/* first page already includes the bl page offset */
	memcpy(dataray, values, CY_BL_PAGE_SIZE + 1);
	INIT_COMPLETION(ts->ld_int_running);
	retval = ttsp_write_block_data(ts, cmd, CY_BL_PAGE_SIZE + 1, dataray);
	if (retval < 0) {
		pr_err("%s: Write chunk err block=%d r=%d\n", __func__, 0, retval);
		goto _cyttsp_wr_blk_chunks_exit;
	}

	values += CY_BL_PAGE_SIZE + 1;
	length -= CY_BL_PAGE_SIZE + 1;

	/* remaining pages require bl page offset stuffing */
	while (length && (block < CY_BL_NUM_PAGES) && !(retval < 0)) {
		dataray[0] = CY_BL_PAGE_SIZE * block;

		retval = _cyttsp_wait_bl_ready_no_stat(ts, 100);
		if (retval < 0) {
			pr_err("%s: wait ready timeout block=%d length=%d\n",
					__func__, block, length);
			goto _cyttsp_wr_blk_chunks_exit;
		}

		memcpy(&dataray[1], values,
				length >= CY_BL_PAGE_SIZE ? CY_BL_PAGE_SIZE : length);
		INIT_COMPLETION(ts->ld_int_running);
		retval = ttsp_write_block_data(ts, cmd,
				length >= CY_BL_PAGE_SIZE ? CY_BL_PAGE_SIZE + 1 : length + 1,
				dataray);
		if (retval < 0) {
			pr_err("%s: Write chunk err block=%d r=%d\n",
					__func__, block, retval);
			goto _cyttsp_wr_blk_chunks_exit;
		}

		values += CY_BL_PAGE_SIZE;
		length = length >= CY_BL_PAGE_SIZE ? length - CY_BL_PAGE_SIZE : 0;
		block++;
	}

	retval = _cyttsp_wait_bl_ready_no_stat(ts, 200);
	if (retval < 0) {
		pr_err("%s: last wait ready timeout block=%d length=%d\n",
				__func__, block, length);
		goto _cyttsp_wr_blk_chunks_exit;
	}

	retval = _cyttsp_load_bl_regs(ts);
	if (retval < 0) {
		pr_err("%s: Load BL regs err r=%d\n", __func__, retval);
		goto _cyttsp_wr_blk_chunks_exit;
	}
	if ((ts->bl_data.bl_status == CY_BL_READY_NO_APP)
			&& (ts->bl_data.bl_error == CY_BL_RUNNING)) {
		retval = 0;
	} else if (ts->bl_data.bl_status == CY_BL_READY_APP) {
		retval = 0;
	} else {
		pr_err("%s: BL status fail bl_stat=0x%02X, bl_err=0x%02X\n",
				__func__, ts->bl_data.bl_status, ts->bl_data.bl_error);
		retval = -ETIMEDOUT;
	}

	_cyttsp_wr_blk_chunks_exit: return retval;
}

static int _cyttsp_load_app(struct cyttsp *ts, const u8 *fw, int fw_size) {
	int retval = 0;
	int loc = 0;

	ts->power_state = CY_LDR_STATE;
	cyttsp_pr_state(ts);

	/* send bootload initiation command */
	pr_info("%s: Send BL Loader Enter\n", __func__);
	INIT_COMPLETION(ts->ld_int_running);
	retval = ttsp_write_block_data(ts, CY_REG_BASE, CY_BL_ENTER_CMD_SIZE,
			(void *) (&fw[loc]));
	if (retval < 0) {
		pr_err("%s: BL loader fail startup r=%d\n", __func__, retval);
		goto loader_exit;
	}

	retval = _cyttsp_wait_bl_ready(ts, 10000);
	if (retval < 0) {
		pr_err("%s: BL loader startup err r=%d\n", __func__, retval);
		goto loader_exit;
	}

	retval = _cyttsp_load_bl_regs(ts);
	if (retval < 0) {
		pr_err("%s: BL loader read error r=%d\n", __func__, retval);
		goto loader_exit;
	}
	if ((ts->bl_data.bl_status & CY_BL_BUSY)
			|| (ts->bl_data.bl_error != CY_BL_RUNNING)) {
		/* signal a status err */
		pr_err(
				"%s: BL loader ready err on enter "
				"bl_file=%02X bl_status=%02X bl_error=%02X\n",
				__func__, ts->bl_data.bl_file, ts->bl_data.bl_status, ts->bl_data.bl_error);
		retval = -EIO;
		goto loader_exit;
	} else {
		/* point to next block */
		loc += CY_BL_ENTER_CMD_SIZE;
	}

	/* send bootload firmware load blocks */
	pr_info("%s: Send BL Loader Blocks\n", __func__);
	while ((fw_size - loc) > CY_BL_WR_BLK_CMD_SIZE) {
		cyttsp_dbg(
				ts,
				CY_DBG_LVL_2,
				"%s: BL loader block=%d"
				" f=%02X s=%02X e=%02X loc=%d\n",
				__func__, loc / CY_BL_WR_BLK_CMD_SIZE, ts->bl_data.bl_file, ts->bl_data.bl_status, ts->bl_data.bl_error, loc);
		retval = _cyttsp_wr_blk_chunks(ts, CY_REG_BASE, CY_BL_WR_BLK_CMD_SIZE,
				&fw[loc]);
		if (retval < 0) {
			pr_err("%s: BL loader fail"
			" r=%d block=%d loc=%d\n",
					__func__, retval, loc / CY_BL_WR_BLK_CMD_SIZE, loc);
			goto loader_exit;
		}

		retval = _cyttsp_load_bl_regs(ts);
		if (retval < 0) {
			pr_err("%s: BL Read error r=%d\n", __func__, retval);
			goto loader_exit;
		}
		if ((ts->bl_data.bl_status & CY_BL_BUSY)
				|| (ts->bl_data.bl_error != CY_BL_RUNNING)) {
			/* signal a status err */
			pr_err(
					"%s: BL READY ERR on write block"
					" bl_file=%02X bl_status=%02X bl_error=%02X\n",
					__func__, ts->bl_data.bl_file, ts->bl_data.bl_status, ts->bl_data.bl_error);
			retval = -EIO;
			goto loader_exit;
		} else {
			/* point to next block */
			loc += CY_BL_WR_BLK_CMD_SIZE;
		}
	}

	/* send bootload terminate command */
	pr_info("%s: Send BL Loader Terminate\n", __func__);
	if (loc == (fw_size - CY_BL_EXIT_CMD_SIZE)) {
		INIT_COMPLETION(ts->ld_int_running);
		retval = ttsp_write_block_data(ts, CY_REG_BASE, CY_BL_EXIT_CMD_SIZE,
				(void *) (&fw[loc]));
		if (retval) {
			pr_err("%s: BL fail Terminate r=%d\n", __func__, retval);
			goto loader_exit;
		}

		retval = _cyttsp_wait_bl_terminate(ts, 10000);
		if (retval < 0) {
			pr_err("%s: BL Loader Terminate err r=%d\n", __func__, retval);
			/*
			 * if app valid bit is set
			 * return success and let driver
			 * try a normal restart anyway
			 */
			if (IS_VALID_APP(ts->bl_data.bl_status)) {
				pr_err("%s: BL Loader Valid App indicated"
				" bl_s=%02X\n", __func__, ts->bl_data.bl_status);
				retval = 0;
			}
		}
	} else {
		pr_err("%s: FW size mismatch\n", __func__);
		retval = -EINVAL;
		goto loader_exit;
	}

	loader_exit: return retval;
}

static int _cyttsp_boot_loader(struct cyttsp *ts) {
	int retval = 0;
	bool new_vers = false;
	struct cyttsp_vers *fw;

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	/*
	 * TODO:
	 * Use a better method to prevent forced old firmware
	 * from being re-written by platform firmware
	 * when startup is called again.
	 */
	if (ts->used_ic_reflash)
		return 0;
#endif

	if (ts->power_state == CY_SLEEP_STATE) {
		pr_err("%s: cannot load firmware in sleep state\n", __func__);
	} else if ((ts->platform_data->fw->ver == NULL)
			|| (ts->platform_data->fw->img == NULL)) {
		cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: empty version list or no image\n",
				__func__);
	} else if (ts->platform_data->fw->vsize != CY_BL_VERS_SIZE) {
		pr_err("%s: bad fw version list size=%d\n",
				__func__, ts->platform_data->fw->vsize);
	} else {
		/* automatically update firmware if new version detected */
		fw = (struct cyttsp_vers *) ts->platform_data->fw->ver;
		new_vers = (((u16) fw->app_verh << 8) + (u16) fw->app_verl)
				> (((u16) ts->bl_data.appver_hi << 8)
						+ (u16) ts->bl_data.appver_lo);

		if (!IS_VALID_APP(ts->bl_data.bl_status) || new_vers) {
			retval = _cyttsp_load_app(ts, ts->platform_data->fw->img,
					ts->platform_data->fw->size);
			if (retval < 0) {
				pr_err("%s: bus fail on load fw\n", __func__);
				ts->power_state = CY_IDLE_STATE;
				cyttsp_pr_state(ts);
			} else {
				/* reset TTSP Device back to bootloader mode */
				retval = _cyttsp_hard_reset(ts);
				if (retval < 0) {
					pr_err("%s: Fail on hard reset r=%d\n", __func__, retval);
				}
			}
		}
	}

	return retval;
}

/* Driver version */
static ssize_t cyttsp_drv_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct cyttsp *ts = dev_get_drvdata(dev);

	return snprintf(buf, CY_MAX_PRBUF_SIZE,
			"Driver: %s\nVersion: %s\nDate: %s\n", ts->input->name,
			CY_DRIVER_VERSION, CY_DRIVER_DATE);
}
static DEVICE_ATTR(drv_ver, S_IRUGO, cyttsp_drv_ver_show, NULL);

/* Driver status */
static ssize_t cyttsp_drv_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct cyttsp *ts = dev_get_drvdata(dev);

	return snprintf(buf, CY_MAX_PRBUF_SIZE, "Driver state is %s\n",
			cyttsp_powerstate_string[ts->power_state]);
}
static DEVICE_ATTR(drv_stat, S_IRUGO, cyttsp_drv_stat_show, NULL);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
/* Group Number */
static ssize_t cyttsp_ic_grpnum_show(struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct cyttsp *ts = dev_get_drvdata(dev);

	return snprintf(buf, CY_MAX_PRBUF_SIZE, "Current Group: %d\n",
			ts->ic_grpnum);
}
static ssize_t cyttsp_ic_grpnum_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {
	struct cyttsp *ts = dev_get_drvdata(dev);
	unsigned long value = 0;
	int retval = 0;

	mutex_lock(&ts->data_lock);
	retval = strict_strtoul(buf, 10, &value);
	if (retval < 0) {
		pr_err("%s: Failed to convert value\n", __func__);
		goto cyttsp_ic_grpnum_store_error_exit;
	}

	if (value > 0xFF) {
		value = 0xFF;
		pr_err("%s: value is greater than max;"
		" set to %d\n", __func__, (int)value);
	}
	ts->ic_grpnum = value;

	cyttsp_ic_grpnum_store_error_exit: retval = size;
	mutex_unlock(&ts->data_lock);
	return retval;
}
static DEVICE_ATTR(ic_grpnum, S_IRUSR | S_IWUSR,
		cyttsp_ic_grpnum_show, cyttsp_ic_grpnum_store);

/* Group Offset */
static ssize_t cyttsp_ic_grpoffset_show(struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct cyttsp *ts = dev_get_drvdata(dev);

	return snprintf(buf, CY_MAX_PRBUF_SIZE, "Current Offset: %u\n",
			ts->ic_grpoffset);
}
static ssize_t cyttsp_ic_grpoffset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {
	struct cyttsp *ts = dev_get_drvdata(dev);
	unsigned long value;
	int retval = 0;

	mutex_lock(&ts->data_lock);
	retval = strict_strtoul(buf, 10, &value);
	if (retval < 0) {
		pr_err("%s: Failed to convert value\n", __func__);
		goto cyttsp_ic_grpoffset_store_error_exit;
	}

	if (value > 0xFF) {
		value = 0xFF;
		pr_err("%s: value is greater than max;"
		" set to %d\n", __func__, (int)value);
	}
	ts->ic_grpoffset = value;

	cyttsp_ic_grpoffset_store_error_exit: retval = size;
	mutex_unlock(&ts->data_lock);
	return retval;
}
static DEVICE_ATTR(ic_grpoffset, S_IRUSR | S_IWUSR,
		cyttsp_ic_grpoffset_show, cyttsp_ic_grpoffset_store);

/* Group Data */
static ssize_t _cyttsp_get_show_data(struct cyttsp *ts, size_t offset,
		size_t num_read, u8 *ic_buf, char *buf) {
	ssize_t uretval = 0;
	int retval = 0;

	if (num_read > 0) {
		retval = ttsp_read_block_data(ts,
				ts->ic_grpoffset + ts->ic_grpstart[ts->ic_grpnum], num_read,
				ic_buf);
		if (retval < 0) {
			pr_err("%s: Cannot read Group %d Data.\n", __func__, ts->ic_grpnum);
			uretval = snprintf(buf, CY_MAX_PRBUF_SIZE,
					"Cannot read Group %d Data.\n", ts->ic_grpnum);
		}
	}

	return uretval;
}
static ssize_t cyttsp_ic_grpdata_show(struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct cyttsp *ts = dev_get_drvdata(dev);
	int i;
	ssize_t uretval = 0;
	int retval = 0;
	int num_read = 0;
	ssize_t grpsize = 0;
	u8 ic_buf[sizeof(ts->xy_data)];

	cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: grpnum=%d grpoffset=%u\n",
			__func__, ts->ic_grpnum, ts->ic_grpoffset);

	mutex_lock(&ts->data_lock);

	switch (ts->ic_grpnum) {
	case CY_IC_GRPNUM_OP_TAG:
	case CY_IC_GRPNUM_SI_TAG:
	case CY_IC_GRPNUM_BL_KEY:
		if (ts->platform_data->sett[ts->ic_grpnum] == NULL) {
			pr_err("%s: Missing table for Group %d\n", __func__, ts->ic_grpnum);
			uretval = snprintf(buf, CY_MAX_PRBUF_SIZE,
					"Missing table for Group %d\n", ts->ic_grpnum);
			goto cyttsp_ic_grpdata_show_exit;
		}
		if (ts->platform_data->sett[ts->ic_grpnum]->data == NULL) {
			pr_err("%s: Missing table data for Group %d\n",
					__func__, ts->ic_grpnum);
			uretval = snprintf(buf, CY_MAX_PRBUF_SIZE,
					"Missing table for Group %d\n", ts->ic_grpnum);
			goto cyttsp_ic_grpdata_show_exit;
		}
		if ((ts->ic_grpoffset + num_read)
				>= ts->platform_data->sett[ts->ic_grpnum]->size) {
			/* TODO: Use size, not num, in error message */
			pr_err("%s: Offset %u exceeds group size of %d\n",
					__func__, ts->ic_grpoffset, ts->ic_grpnum);
			uretval = snprintf(buf, CY_MAX_PRBUF_SIZE,
					"Offset %u exceeds group size of %d\n", ts->ic_grpoffset,
					ts->ic_grpnum);
			goto cyttsp_ic_grpdata_show_exit;
		}
		num_read = ts->platform_data->sett[ts->ic_grpnum]->size
				- ts->ic_grpoffset;
		switch (ts->ic_grpnum) {
		case CY_IC_GRPNUM_OP_TAG:
			uretval = _cyttsp_get_show_data(ts,
					ts->ic_grpoffset + ts->ic_grpstart[ts->ic_grpnum], num_read,
					ic_buf, buf);
			if (uretval > 0)
				goto cyttsp_ic_grpdata_show_exit;
			break;
		case CY_IC_GRPNUM_SI_TAG:
			retval = _cyttsp_set_sysinfo_mode(ts);
			if (retval < 0) {
				pr_err("%s: Cannot access SI Group %d Data.\n",
						__func__, ts->ic_grpnum);
				uretval = snprintf(buf, CY_MAX_PRBUF_SIZE,
						"Cannot access SI Group %d Data.\n", ts->ic_grpnum);
				goto cyttsp_ic_grpdata_show_exit;
			}

			uretval = _cyttsp_get_show_data(ts,
					ts->ic_grpoffset + ts->ic_grpstart[ts->ic_grpnum], num_read,
					ic_buf, buf);

			retval = _cyttsp_set_operational_mode(ts);
			if (retval < 0) {
				pr_err("%s: Cannot restore op mode\n", __func__);
			} else {
				ts->power_state = CY_ACTIVE_STATE;
				cyttsp_pr_state(ts);
			}
			if (uretval > 0)
				goto cyttsp_ic_grpdata_show_exit;
			break;
		case CY_IC_GRPNUM_BL_KEY:
			memcpy(
					ic_buf,
					&ts->platform_data->sett[ts->ic_grpnum]->data[ts->ic_grpoffset],
					num_read);
			break;
		}
		break;

	case CY_IC_GRPNUM_OP_REG:
		grpsize = sizeof(struct cyttsp_xydata);
		if (ts->ic_grpoffset >= grpsize)
			goto cyttsp_ic_grpdata_show_error_exit;
		num_read = grpsize - ts->ic_grpoffset;
		uretval = _cyttsp_get_show_data(ts, ts->ic_grpoffset, num_read, ic_buf,
				buf);
		if (uretval > 0)
			goto cyttsp_ic_grpdata_show_exit;
		break;
	case CY_IC_GRPNUM_SI_REG:
		grpsize = sizeof(struct cyttsp_sysinfo_data);
		if (ts->ic_grpoffset >= grpsize)
			goto cyttsp_ic_grpdata_show_error_exit;
		num_read = grpsize - ts->ic_grpoffset;
		retval = _cyttsp_set_sysinfo_mode(ts);
		if (retval < 0) {
			pr_err("%s: Cannot access SI Group %d Data.\n",
					__func__, ts->ic_grpnum);
			uretval = snprintf(buf, CY_MAX_PRBUF_SIZE,
					"Cannot access SI Group %d Data.\n", ts->ic_grpnum);
			goto cyttsp_ic_grpdata_show_exit;
		}
		uretval = _cyttsp_get_show_data(ts, ts->ic_grpoffset, num_read, ic_buf,
				buf);

		retval = _cyttsp_set_operational_mode(ts);
		if (retval < 0) {
			pr_err("%s: Cannot restore op mode\n", __func__);
		} else {
			ts->power_state = CY_ACTIVE_STATE;
			cyttsp_pr_state(ts);
		}

		if (uretval > 0)
			goto cyttsp_ic_grpdata_show_exit;
		break;
	case CY_IC_GRPNUM_BL_REG:
		grpsize = sizeof(struct cyttsp_bootloader_data);
		if (ts->ic_grpoffset > grpsize)
			goto cyttsp_ic_grpdata_show_error_exit;
		num_read = grpsize - ts->ic_grpoffset;
		retval = _cyttsp_hard_reset(ts);
		if (retval < 0) {
			pr_err("%s: Cannot access BL Group %d Data.\n",
					__func__, ts->ic_grpnum);
			uretval = snprintf(buf, CY_MAX_PRBUF_SIZE,
					"Cannot access BL Group %d Data.\n", ts->ic_grpnum);
			goto cyttsp_ic_grpdata_show_exit;
		}
		uretval = _cyttsp_get_show_data(ts, ts->ic_grpoffset, num_read, ic_buf,
				buf);

		retval = _cyttsp_startup(ts);
		if (retval < 0) {
			pr_err("%s: Cannot restore op mode\n", __func__);
		}

		if (uretval > 0)
			goto cyttsp_ic_grpdata_show_exit;
		break;
	default:
		pr_err("%s: Group %d does not exist.\n", __func__, ts->ic_grpnum);
		uretval = snprintf(buf, CY_MAX_PRBUF_SIZE, "Group %d does not exist.\n",
				ts->ic_grpnum);
		goto cyttsp_ic_grpdata_show_exit;
	}

	snprintf(buf, CY_MAX_PRBUF_SIZE, "Group %d, Offset %u:\n", ts->ic_grpnum,
			ts->ic_grpoffset);
	for (i = 0; i < num_read; i++)
		snprintf(buf, CY_MAX_PRBUF_SIZE, "%s0x%02X\n", buf, ic_buf[i]);
	uretval = snprintf(buf, CY_MAX_PRBUF_SIZE, "%s(%d bytes)\n", buf, num_read);

	goto cyttsp_ic_grpdata_show_exit;

	cyttsp_ic_grpdata_show_error_exit:
	pr_err("%s: Offset %u exceeds group size of %d\n",
			__func__, ts->ic_grpoffset, grpsize);
	uretval = snprintf(buf, CY_MAX_PRBUF_SIZE,
			"Offset %u exceeds group size of %d\n", ts->ic_grpoffset, grpsize);

	cyttsp_ic_grpdata_show_exit: mutex_unlock(&ts->data_lock);
	return uretval;
}
static void _cyttsp_put_store_data(struct cyttsp *ts, size_t offset,
		size_t num_write, u8 *ic_buf) {
	int i;
	int retval;

	retval = ttsp_write_block_data(ts, offset, num_write, ic_buf);
	if (retval < 0)
		pr_err("%s: Err write numbytes=%d\n", __func__, num_write);
	else {
		for (i = 0; i < num_write; i++) {
			cyttsp_dbg(ts, CY_DBG_LVL_2, "%s: %02X\n", __func__, ic_buf[i]);
		}cyttsp_dbg(ts, CY_DBG_LVL_2, "%s: (%d bytes)\n", __func__, num_write);
	}

	return;
}
static ssize_t cyttsp_ic_grpdata_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {
	struct cyttsp *ts = dev_get_drvdata(dev);
	unsigned long value;
	int retval = 0;
	const char *pbuf = buf;
	int scan_read;
	int num_write = 0;
	char scan_buf[5];
	ssize_t grpsize = 0;
	u8 ic_buf[sizeof(ts->xy_data)];

	cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: grpnum=%d grpoffset=%u\n",
			__func__, ts->ic_grpnum, ts->ic_grpoffset);

	cyttsp_dbg(ts, CY_DBG_LVL_3,
			"%s: pbuf=%p buf=%p size=%d sizeof(scan_buf)=%d\n",
			__func__, pbuf, buf, size, sizeof(scan_buf));
	num_write = 0;
	while (pbuf <= ((buf + size) - (sizeof(scan_buf) - 1))) {
		while (((*pbuf == ' ') || (*pbuf == ',')) && (pbuf < ((buf + size) - 4)))
			pbuf++;
		if (pbuf <= ((buf + size) - (sizeof(scan_buf) - 1))) {
			memset(scan_buf, 0, sizeof(scan_buf));
			for (scan_read = 0; scan_read < sizeof(scan_buf) - 1; scan_read++)
				scan_buf[scan_read] = *pbuf++;
			retval = strict_strtoul(scan_buf, 16, &value);
			if (retval < 0) {
				pr_err("%s: Invalid data format. "
				"Use \"0xHH,...,0xHH\" instead.\n", __func__);
				goto cyttsp_ic_grpdata_store_exit;
			} else {
				ic_buf[num_write] = value;
				cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: ic_buf[%d] = 0x%02X\n",
						__func__, num_write, ic_buf[num_write]);
				num_write++;
			}
		} else
			break;
	}

	mutex_lock(&ts->data_lock);

	switch (ts->ic_grpnum) {
	case CY_IC_GRPNUM_OP_TAG:
	case CY_IC_GRPNUM_SI_TAG:
		if (ts->platform_data->sett[ts->ic_grpnum] == NULL) {
			pr_err("%s: Missing table for Group %d\n", __func__, ts->ic_grpnum);
			goto cyttsp_ic_grpdata_store_exit;
		}
		if (ts->platform_data->sett[ts->ic_grpnum] == NULL) {
			pr_err("%s: Missing table for Group %d\n", __func__, ts->ic_grpnum);
			goto cyttsp_ic_grpdata_store_exit;
		}
		if (ts->ic_grpoffset > ts->platform_data->sett[ts->ic_grpnum]->size) {
			/* TODO: Use size, not num, in error message */
			pr_err("%s: Offset %u exceeds group size of %d\n",
					__func__, ts->ic_grpoffset, ts->ic_grpnum);
			goto cyttsp_ic_grpdata_store_exit;
		}
		grpsize = ts->platform_data->sett[ts->ic_grpnum]->size;
		cyttsp_dbg(ts, CY_DBG_LVL_2,
				"%s: write %d bytes at grpnum=%d grpoffset=%u "
				"grpsize=%d\n",
				__func__, num_write, ts->ic_grpnum, ts->ic_grpoffset, grpsize);
		num_write =
				num_write > grpsize - ts->ic_grpoffset ?
						grpsize - ts->ic_grpoffset : num_write;
		switch (ts->ic_grpnum) {
		case CY_IC_GRPNUM_OP_TAG:
			_cyttsp_put_store_data(ts,
					ts->ic_grpoffset + ts->ic_grpstart[ts->ic_grpnum],
					num_write, ic_buf);
			break;
		case CY_IC_GRPNUM_SI_TAG:
			retval = _cyttsp_set_sysinfo_mode(ts);
			if (retval < 0) {
				pr_err("%s: Cannot write SI Group Data.\n", __func__);
				goto cyttsp_ic_grpdata_store_exit;
			}

			_cyttsp_put_store_data(ts,
					ts->ic_grpoffset + ts->ic_grpstart[ts->ic_grpnum],
					num_write, ic_buf);

			retval = _cyttsp_set_operational_mode(ts);
			if (retval < 0) {
				pr_err("%s: Cannot restore operating mode.\n", __func__);
			} else {
				ts->power_state = CY_ACTIVE_STATE;
				cyttsp_pr_state(ts);
			}
			break;
		}
		break;
	case CY_IC_GRPNUM_BL_KEY:
		pr_err("%s: Group=%d is read only\n", __func__, ts->ic_grpnum);
		break;
	case CY_IC_GRPNUM_OP_REG:
		grpsize = sizeof(struct cyttsp_xydata);
		/* TODO: Make sure data types match in print string*/
		if ((ts->ic_grpoffset + num_write) >= grpsize) {
			pr_err("%s: Trying to write %d bytes to offset %hu, "
			"which exceeds group size of %u\n",
					__func__, num_write, ts->ic_grpoffset, grpsize);
			goto cyttsp_ic_grpdata_store_exit;
		}
		_cyttsp_put_store_data(ts,
				ts->ic_grpoffset + ts->ic_grpstart[ts->ic_grpnum], num_write,
				ic_buf);
		break;
	case CY_IC_GRPNUM_SI_REG:
		grpsize = sizeof(struct cyttsp_sysinfo_data);
		/* TODO: Make sure data types match in print string*/
		if ((ts->ic_grpoffset + num_write) >= grpsize) {
			pr_err("%s: Trying to write %d bytes to offset %hu, "
			"which exceeds group size of %u\n",
					__func__, num_write, ts->ic_grpoffset, grpsize);
			goto cyttsp_ic_grpdata_store_exit;
		}
		retval = _cyttsp_set_sysinfo_mode(ts);
		if (retval < 0) {
			pr_err("%s: Cannot write SI Group Data.\n", __func__);
			goto cyttsp_ic_grpdata_store_exit;
		}

		_cyttsp_put_store_data(ts,
				ts->ic_grpoffset + ts->ic_grpstart[ts->ic_grpnum], num_write,
				ic_buf);

		retval = _cyttsp_set_operational_mode(ts);
		if (retval < 0) {
			pr_err("%s: Cannot restore operating mode.\n", __func__);
		} else {
			ts->power_state = CY_ACTIVE_STATE;
			cyttsp_pr_state(ts);
		}
		break;
	case CY_IC_GRPNUM_BL_REG:
		pr_err("%s: Group=%d is read only\n", __func__, ts->ic_grpnum);
		break;
	default:
		pr_err("%s: Group %d does not exist.\n", __func__, ts->ic_grpnum);
		break;
	}

	cyttsp_ic_grpdata_store_exit: mutex_unlock(&ts->data_lock);
	retval = size;
	return retval;
}
static DEVICE_ATTR(ic_grpdata, S_IRUSR | S_IWUSR,
		cyttsp_ic_grpdata_show, cyttsp_ic_grpdata_store);

static ssize_t cyttsp_drv_flags_show(struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct cyttsp *ts = dev_get_drvdata(dev);

	return snprintf(buf, CY_MAX_PRBUF_SIZE, "Current Driver Flags: 0x%04X\n",
			ts->flags);
}
static ssize_t cyttsp_drv_flags_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {
	struct cyttsp *ts = dev_get_drvdata(dev);
	unsigned long value = 0;
	ssize_t retval = 0;

	mutex_lock(&(ts->data_lock));
	retval = strict_strtoul(buf, 16, &value);
	if (retval < 0) {
		pr_err("%s: Failed to convert value\n", __func__);
		goto cyttsp_drv_flags_store_error_exit;
	}

	if (value > 0xFFFF) {
		pr_err("%s: value=%lu is greater than max;"
		" drv_flags=0x%04X\n", __func__, value, ts->flags);
	} else {
		ts->flags = value;
	}

	cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: drv_flags=0x%04X\n", __func__, ts->flags);

	cyttsp_drv_flags_store_error_exit: retval = size;
	mutex_unlock(&(ts->data_lock));
	return retval;
}
static DEVICE_ATTR(drv_flags, S_IRUSR | S_IWUSR,
		cyttsp_drv_flags_show, cyttsp_drv_flags_store);

static ssize_t cyttsp_hw_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {
	struct cyttsp *ts = dev_get_drvdata(dev);
	ssize_t retval = 0;

	mutex_lock(&(ts->data_lock));
	retval = _cyttsp_startup(ts);
	mutex_unlock(&(ts->data_lock));
	if (retval < 0) {
		pr_err("%s: fail hw_reset device restart r=%d\n", __func__, retval);
	}

	retval = size;
	return retval;
}
static DEVICE_ATTR(hw_reset, S_IWUSR, NULL, cyttsp_hw_reset_store);
/*
 static ssize_t cyttsp_hw_recov_store(struct device *dev,
 struct device_attribute *attr, const char *buf, size_t size)
 {
 struct cyttsp *ts = dev_get_drvdata(dev);
 unsigned long value = 0;
 ssize_t retval = 0;

 mutex_lock(&(ts->data_lock));
 retval = strict_strtoul(buf, 10, &value);
 if (retval < 0) {
 pr_err("%s: Failed to convert value\n", __func__);
 goto cyttsp_hw_recov_store_error_exit;
 }

 retval = hw_recov(ts);
 if (retval < 0) {
 pr_err("%s: fail hw_recov(value=%d) function r=%d\n",
 __func__, (int)value, retval);
 }

 cyttsp_hw_recov_store_error_exit:
 retval = size;
 mutex_unlock(&(ts->data_lock));
 return retval;
 }
 static DEVICE_ATTR(hw_recov, S_IWUSR, NULL, cyttsp_hw_recov_store);
 */
#endif

static ssize_t cyttsp_ic_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct cyttsp *ts = dev_get_drvdata(dev);

	return snprintf(buf, CY_MAX_PRBUF_SIZE, "Application Version: 0x%02X%02X\n"
			"TTSP Version:        0x%02X%02X\n"
			"Applicaton ID:       0x%02X%02X\n"
			"Custom ID:           0x%02X%02X%02X\n", ts->sysinfo_data.app_verh,
			ts->sysinfo_data.app_verl, ts->sysinfo_data.tts_verh,
			ts->sysinfo_data.tts_verl, ts->sysinfo_data.app_idh,
			ts->sysinfo_data.app_idl, ts->sysinfo_data.cid[0],
			ts->sysinfo_data.cid[1], ts->sysinfo_data.cid[2]);
}
static DEVICE_ATTR(ic_ver, S_IRUGO, cyttsp_ic_ver_show, NULL);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
static ssize_t cyttsp_ic_irqstat_show(struct device *dev,
		struct device_attribute *attr, char *buf) {
	int retval;
	struct cyttsp *ts = dev_get_drvdata(dev);

	retval = cyttsp3_irq_stat(ts);
	switch (retval) {
	case 0:
		return snprintf(buf, CY_MAX_PRBUF_SIZE, "Interrupt line is LOW.\n");
	case 1:
		return snprintf(buf, CY_MAX_PRBUF_SIZE, "Interrupt line is HIGH.\n");
	default:
		return snprintf(buf, CY_MAX_PRBUF_SIZE,
				"Function irq_stat() returned %d.\n", retval);
	}
}
static DEVICE_ATTR(hw_irqstat, S_IRUSR | S_IWUSR,
		cyttsp_ic_irqstat_show, NULL);
#endif

/* Disable Driver IRQ */
static ssize_t cyttsp_drv_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct cyttsp *ts = dev_get_drvdata(dev);

	return snprintf(buf, CY_MAX_PRBUF_SIZE, "Driver IRQ is %s\n",
			ts->irq_enabled ? "enabled" : "disabled");
}
static ssize_t cyttsp_drv_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {
	int retval = 0;
	struct cyttsp *ts = dev_get_drvdata(dev);
	unsigned long value;

	mutex_lock(&ts->data_lock);

	if (size > 2) {
		pr_err("%s: Err, data too large\n", __func__);
		retval = -EOVERFLOW;
		goto cyttsp_drv_irq_store_error_exit;
	}

	retval = strict_strtoul(buf, 10, &value);
	if (retval < 0) {
		pr_err("%s: Failed to convert value\n", __func__);
		goto cyttsp_drv_irq_store_error_exit;
	}

	if (ts->irq_enabled == false) {
		if (value == 1) {
			/* Enable IRQ */
			enable_irq(ts->irq);
			pr_info("%s: Driver IRQ now enabled\n", __func__);
			ts->irq_enabled = true;
		} else {
			pr_info("%s: Driver IRQ already disabled\n", __func__);
		}
	} else {
		if (value == 0) {
			/* Disable IRQ */
			disable_irq_nosync(ts->irq);
			pr_info("%s: Driver IRQ now disabled\n", __func__);
			ts->irq_enabled = false;
		} else {
			pr_info("%s: Driver IRQ already enabled\n", __func__);
		}
	}

	cyttsp_drv_irq_store_error_exit: mutex_unlock(&ts->data_lock);
	retval = size;
	return retval;
}
static DEVICE_ATTR(drv_irq, S_IRUSR | S_IWUSR,
		cyttsp_drv_irq_show, cyttsp_drv_irq_store);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
/* Force firmware upgrade */
static int _cyttsp_firmware_class_loader(struct cyttsp *ts, const u8 *data,
		int size) {
	int retval = 0;
	const u8 *img = data + data[0] + 1;
	int img_size = size - (data[0] + 1);

	retval = _cyttsp_load_bl_regs(ts);
	if (retval < 0) {
		pr_err("%s: Fail read bootloader\n", __func__);
		goto _cyttsp_firmware_class_loader_error;
	}

	retval = _cyttsp_hard_reset(ts);
	if (retval < 0) {
		pr_err("%s: Fail reset device on loader\n", __func__);
		goto _cyttsp_firmware_class_loader_error;
	}

	retval = _cyttsp_load_bl_regs(ts);
	if (retval < 0) {
		pr_err("%s: Fail read bootloader\n", __func__);
		goto _cyttsp_firmware_class_loader_error;
	}

	cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: call load_app\n", __func__);
	retval = _cyttsp_load_app(ts, img, img_size);
	if (retval < 0) {
		pr_err("%s: fail on load fw r=%d\n", __func__, retval);
		goto _cyttsp_firmware_class_loader_error;
	}

	goto _cyttsp_firmware_class_loader_exit;

	_cyttsp_firmware_class_loader_error: ts->power_state = CY_IDLE_STATE;
	cyttsp_pr_state(ts);
	_cyttsp_firmware_class_loader_exit: return retval;
}
static void cyttsp_firmware_cont(const struct firmware *fw, void *context) {
	int retval = 0;
	struct device *dev = context;
	struct cyttsp *ts = dev_get_drvdata(dev);

	if (fw == NULL) {
		pr_err("%s: firmware not found\n", __func__);
		goto cyttsp_firmware_cont_exit;
	}

	mutex_lock(&ts->data_lock);

	retval = _cyttsp_firmware_class_loader(ts, fw->data, fw->size);
	if (retval < 0) {
		/* mark an error and then try restarting anyway */
		pr_err("%s: Fail firmware class loader r=%d\n", __func__, retval);
	}

	ts->used_ic_reflash = true;

	retval = _cyttsp_startup(ts);
	if (retval < 0) {
		pr_info("%s: Fail _cyttsp_startup after fw load r=%d\n",
				__func__, retval);
	}

	mutex_unlock(&ts->data_lock);
	release_firmware(fw);

	cyttsp_firmware_cont_exit: ts->waiting_for_fw = false;
	return;
}
static ssize_t cyttsp_ic_reflash_show(struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct cyttsp *ts = dev_get_drvdata(dev);

	return snprintf(
			buf,
			CY_MAX_PRBUF_SIZE,
			"%s\n",
			ts->waiting_for_fw ?
					"Driver is waiting for firmware load" :
					"No firmware loading in progress");
}
static ssize_t cyttsp_ic_reflash_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {
	int i;
	int retval = 0;
	struct cyttsp *ts = dev_get_drvdata(dev);

	if (ts->waiting_for_fw) {
		pr_err("%s: Driver is already waiting for firmware\n", __func__);
		retval = -EALREADY;
		goto cyttsp_ic_reflash_store_exit;
	}

	/*
	 * must configure FW_LOADER in .config file
	 * CONFIG_HOTPLUG=y
	 * CONFIG_FW_LOADER=y
	 * CONFIG_FIRMWARE_IN_KERNEL=y
	 * CONFIG_EXTRA_FIRMWARE=""
	 * CONFIG_EXTRA_FIRMWARE_DIR=""
	 */

	if (size > CY_BL_FW_NAME_SIZE) {
		pr_err("%s: Filename too long\n", __func__);
		retval = -ENAMETOOLONG;
		goto cyttsp_ic_reflash_store_exit;
	} else {
		/*
		 * name string must be in alloc() memory
		 * or is lost on context switch
		 * strip off any line feed character(s)
		 * at the end of the buf string
		 */
		for (i = 0; buf[i]; i++) {
			if (buf[i] < ' ')
				ts->fwname[i] = 0;
			else
				ts->fwname[i] = buf[i];
		}
	}

	pr_info("%s: Enabling firmware class loader\n", __func__);

	retval = request_firmware_nowait(THIS_MODULE, FW_ACTION_NOHOTPLUG,
			(const char *) ts->fwname, ts->dev, GFP_KERNEL, ts->dev,
			cyttsp_firmware_cont);
	mutex_lock(&ts->data_lock);
	if (retval < 0) {
		pr_err("%s: Fail request firmware class file load\n", __func__);
		ts->waiting_for_fw = false;
		goto cyttsp_ic_reflash_store_exit;
	} else {
		ts->waiting_for_fw = true;
		retval = size;
	}
	mutex_unlock(&ts->data_lock);

	cyttsp_ic_reflash_store_exit: return retval;
}
static DEVICE_ATTR(ic_reflash, S_IRUSR | S_IWUSR,
		cyttsp_ic_reflash_show, cyttsp_ic_reflash_store);
#endif

#ifdef CONFIG_TOUCHSCREEN_DEBUG
/* Driver debugging */
static ssize_t cyttsp_drv_debug_show(struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct cyttsp *ts = dev_get_drvdata(dev);

	return snprintf(buf, CY_MAX_PRBUF_SIZE, "Debug Setting: %u\n",
			ts->bus_ops->tsdebug);
}
static ssize_t cyttsp_drv_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size) {
	struct cyttsp *ts = dev_get_drvdata(dev);
	int retval = 0;
	unsigned long value;

	retval = strict_strtoul(buf, 10, &value);
	if (retval < 0) {
		pr_err("%s: Failed to convert value\n", __func__);
		goto cyttsp_drv_debug_store_exit;
	}

	switch (value) {
	case CY_DBG_LVL_0:
	case CY_DBG_LVL_1:
	case CY_DBG_LVL_2:
	case CY_DBG_LVL_3:
		pr_info("%s: Debug setting=%d\n", __func__, (int)value);
		ts->bus_ops->tsdebug = value;
		break;
	default:
		ts->bus_ops->tsdebug = CY_DBG_LVL_MAX;
		pr_err("%s: Invalid debug setting; set to max=%d\n",
				__func__, ts->bus_ops->tsdebug);
		break;
	}

	retval = size;

	cyttsp_drv_debug_store_exit: return retval;
}
static DEVICE_ATTR(drv_debug, S_IRUSR | S_IWUSR,
		cyttsp_drv_debug_show, cyttsp_drv_debug_store);
#endif

static void cyttsp_ldr_init(struct cyttsp *ts) {
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if (device_create_file(ts->dev, &dev_attr_drv_debug))
		pr_err("%s: Cannot create drv_debug\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_drv_flags))
		pr_err("%s: Cannot create drv_flags\n", __func__);
#endif

	if (device_create_file(ts->dev, &dev_attr_drv_irq))
		pr_err("%s: Cannot create drv_irq\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_drv_stat))
		pr_err("%s: Cannot create drv_stat\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_drv_ver))
		pr_err("%s: Cannot create drv_ver\n", __func__);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if (device_create_file(ts->dev, &dev_attr_hw_irqstat))
		pr_err("%s: Cannot create hw_irqstat\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_hw_reset))
		pr_err("%s: Cannot create hw_reset\n", __func__);

	/*
	 if (device_create_file(ts->dev, &dev_attr_hw_recov))
	 pr_err("%s: Cannot create hw_recov\n", __func__);
	 */

	if (device_create_file(ts->dev, &dev_attr_ic_grpdata))
		pr_err("%s: Cannot create ic_grpdata\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_ic_grpnum))
		pr_err("%s: Cannot create ic_grpnum\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_ic_grpoffset))
		pr_err("%s: Cannot create ic_grpoffset\n", __func__);

	if (device_create_file(ts->dev, &dev_attr_ic_reflash))
		pr_err("%s: Cannot create ic_reflash\n", __func__);
#endif

	if (device_create_file(ts->dev, &dev_attr_ic_ver))
		pr_err("%s: Cannot create ic_ver\n", __func__);

	return;
}

static void cyttsp_ldr_free(struct cyttsp *ts) {
	device_remove_file(ts->dev, &dev_attr_drv_irq);
	device_remove_file(ts->dev, &dev_attr_drv_ver);
	device_remove_file(ts->dev, &dev_attr_drv_stat);
	device_remove_file(ts->dev, &dev_attr_ic_ver);
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	device_remove_file(ts->dev, &dev_attr_drv_debug);
	device_remove_file(ts->dev, &dev_attr_drv_flags);
	device_remove_file(ts->dev, &dev_attr_hw_irqstat);
	device_remove_file(ts->dev, &dev_attr_hw_reset);
	/*
	 device_remove_file(ts->dev, &dev_attr_hw_recov);
	 */
	device_remove_file(ts->dev, &dev_attr_ic_grpdata);
	device_remove_file(ts->dev, &dev_attr_ic_grpnum);
	device_remove_file(ts->dev, &dev_attr_ic_grpoffset);
	device_remove_file(ts->dev, &dev_attr_ic_reflash);
#endif
}

static int _cyttsp_resume_sleep(struct cyttsp *ts) {
	int retval = 0;
#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
	u8 sleep = CY_DEEP_SLEEP_MODE;

	cyttsp_dbg(ts, CY_DBG_LVL_3,
			"%s: Put the part back to sleep\n", __func__);

	retval = ttsp_write_block_data(ts, CY_REG_BASE, sizeof(sleep), &sleep);
	if (retval < 0) {
		pr_err("%s: Failed to write sleep bit r=%d\n",
				__func__, retval);
	}

	ts->power_state = CY_SLEEP_STATE;
	cyttsp_pr_state(ts);
#endif
	return retval;
}

static int _cyttsp_detect_hw(struct cyttsp *ts)
{
	int ret;

	ret = _cyttsp_hard_reset(ts);
	if (ret) {
		pr_err("%s: HW reset failed: %d\n", __func__, ret);
		return -ENODEV;
	}

	ret = _cyttsp_load_bl_regs(ts);
	if (ret) {
		pr_err("%s: BL regs read failed: %d\n", __func__, ret);
		return -ENODEV;
	}

	return ret;
}

static int _cyttsp_startup(struct cyttsp *ts) {
	int retval = 0;

	retval = _cyttsp_hard_reset(ts);
	if (retval < 0) {
		pr_err("%s: Fail on HW reset r=%d\n", __func__, retval);
		ts->power_state = CY_INVALID_STATE;
		cyttsp_pr_state(ts);
		goto _cyttsp_startup_exit;
	}

	retval = _cyttsp_load_bl_regs(ts);
	if (retval < 0) {
		pr_err("%s: Fail on bl regs read r=%d\n", __func__, retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp_pr_state(ts);
		goto _cyttsp_startup_exit;
	}

	retval = _cyttsp_boot_loader(ts);
	if (retval < 0) {
		pr_err("%s: Fail on boot loader r=%d\n", __func__, retval);
		goto _cyttsp_startup_exit;
		/* We are now in IDLE state */
	}

	retval = _cyttsp_exit_bl_mode(ts);
	if (retval < 0) {
		pr_err("%s: Fail on exit bootloader r=%d\n", __func__, retval);
		goto _cyttsp_startup_exit;
	}

	retval = _cyttsp_set_sysinfo_mode(ts);
	if (retval < 0) {
		pr_err("%s: Fail on set sysinfo mode r=%d\n", __func__, retval);
		goto _cyttsp_startup_exit;
	}

	retval = _cyttsp_set_sysinfo_regs(ts);
	if (retval < 0) {
		pr_err("%s: Fail on set sysinfo regs r=%d\n", __func__, retval);
		goto _cyttsp_startup_exit;
	}

	retval = _cyttsp_set_operational_mode(ts);
	if (retval < 0) {
		pr_err("%s: Fail on set op mode r=%d\n", __func__, retval);
		goto _cyttsp_startup_exit;
	}

	retval = _cyttsp_set_operational_regs(ts);
	if (retval < 0) {
		pr_err("%s: Fail on set op regs r=%d\n", __func__, retval);
		goto _cyttsp_startup_exit;
	}

	/* successful startup; switch to active state */
	ts->power_state = CY_ACTIVE_STATE;
	cyttsp_pr_state(ts);

	_cyttsp_startup_exit: if (ts->was_suspended) {
		ts->was_suspended = false;
		retval = _cyttsp_resume_sleep(ts);
		if (retval < 0) {
			pr_err("%s: Fail resume sleep, IC is active r=%d\n",
					__func__, retval);
		}
	}
	return retval;
}

static irqreturn_t cyttsp_irq(int irq, void *handle) {
	struct cyttsp *ts = handle;

	int retval;

	cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: GOT IRQ ps=%d\n",
			__func__, ts->power_state);

	mutex_lock(&ts->data_lock);

	cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: DO IRQ ps=%d\n",
			__func__, ts->power_state);

	switch (ts->power_state) {
	case CY_BL_STATE:
		complete(&ts->bl_int_running);
		udelay(2000);
		/* Work around extra level interrupts */
		break;
	case CY_SYSINFO_STATE:
		complete(&ts->si_int_running);
		udelay(100);
		/* Work around extra level interrupts */
		break;
	case CY_READY_STATE:
		complete(&ts->op_int_running);
		udelay(100);
		/* Work around extra level interrupts */
		break;
	case CY_LDR_STATE:
		complete(&ts->ld_int_running);
		udelay(2000);
		/* Work around extra level interrupts */
		break;
	case CY_SLEEP_STATE:
		pr_err("%s: Attempt to process touch after enter sleep or"
		" unexpected wake event\n", __func__);
		retval = _cyttsp_wakeup(ts); /* in case its really asleep */
		if (retval < 0) {
			ts->was_suspended = true;
			pr_err("%s: wakeup fail (retval=%d)\n", __func__, retval);
			cyttsp_pr_state(ts);
			memset(ts->prv_trk, CY_NTCH, sizeof(ts->prv_trk));
			queue_work(ts->cyttsp_wq, &ts->cyttsp_resume_startup_work);
			pr_info("%s: startup queued\n", __func__);
			break;
		}
		retval = _cyttsp_resume_sleep(ts);
		if (retval < 0) {
			ts->was_suspended = true;
			pr_err("%s: resume suspend fail (retval=%d)\n", __func__, retval);
			cyttsp_pr_state(ts);
			memset(ts->prv_trk, CY_NTCH, sizeof(ts->prv_trk));
			queue_work(ts->cyttsp_wq, &ts->cyttsp_resume_startup_work);
			pr_info("%s: startup queued\n", __func__);
			break;
		}
		break;
	case CY_IDLE_STATE:
		/* device now available; signal initialization */
		pr_info("%s: Received IRQ in IDLE state\n", __func__);
		/* Try to determine the IC's current state */
		retval = _cyttsp_load_bl_regs(ts);
		if (retval < 0) {
			pr_err("%s: Still unable to find IC after IRQ\n", __func__);
		} else if (IS_BOOTLOADERMODE(ts->bl_data.bl_status)) {
			pr_info("%s: BL mode found in IDLE state\n", __func__);
			memset(ts->prv_trk, CY_NTCH, sizeof(ts->prv_trk));
			queue_work(ts->cyttsp_wq, &ts->cyttsp_resume_startup_work);
			pr_info("%s: startup queued\n", __func__);
		} else {
			pr_info("%s: ACTIVE mode found in IDLE state\n", __func__);
			ts->power_state = CY_ACTIVE_STATE;
			cyttsp_pr_state(ts);
			retval = _cyttsp_xy_worker(ts);
			if (retval < 0) {
				ts->was_suspended = false;
				pr_err("%s: xy_worker IDLE fail (retval=%d)\n",
						__func__, retval);
				memset(ts->prv_trk, CY_NTCH, sizeof(ts->prv_trk));
				queue_work(ts->cyttsp_wq, &ts->cyttsp_resume_startup_work);
				pr_info("%s: startup queued\n", __func__);
				break;
			}
		}
		break;
	case CY_ACTIVE_STATE:
		retval = _cyttsp_xy_worker(ts);
		if (retval < 0) {
			ts->was_suspended = false;
			pr_err("%s: xy_worker ACTIVE fail (retval=%d)\n", __func__, retval);
			memset(ts->prv_trk, CY_NTCH, sizeof(ts->prv_trk));
			queue_work(ts->cyttsp_wq, &ts->cyttsp_resume_startup_work);
			pr_info("%s: startup queued\n", __func__);
			break;
		}
		break;
	default:
		break;
	}

	mutex_unlock(&ts->data_lock);
	cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: exiting...\n", __func__ );
	return IRQ_HANDLED;
}

static int cyttsp_power_on(struct cyttsp *ts) {
	int retval = 0;

	/* Communicate with the IC */
	mutex_lock(&ts->data_lock);
	retval = _cyttsp_startup(ts);
	mutex_unlock(&ts->data_lock);
	if (retval < 0) {
		pr_err("%s: startup fail (retval=%d)\n", __func__, retval);
		if (ts->power_state != CY_IDLE_STATE) {
			ts->power_state = CY_IDLE_STATE;
			cyttsp_pr_state(ts);
		}
	}

	return retval;
}

static void cyttsp_ts_work_func(struct work_struct *work) {
	struct cyttsp *ts =
			container_of(work, struct cyttsp, cyttsp_resume_startup_work);
	int retval = 0;

	mutex_lock(&ts->data_lock);

	retval = _cyttsp_startup(ts);
	if (retval < 0) {
		pr_err("%s: Startup failed with error code %d\n", __func__, retval);
		ts->power_state = CY_IDLE_STATE;
		cyttsp_pr_state(ts);
	}

	mutex_unlock(&ts->data_lock);

	return;
}

static int _cyttsp_wakeup(struct cyttsp *ts) {
	int retval = 0;

#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
	int tries;
	struct cyttsp_xydata xydata;

	cyttsp_dbg(ts, CY_DBG_LVL_3,
			"%s: default wake up device with a read\n",
			__func__);
	retval = ttsp_read_block_data(ts, CY_REG_BASE,
			sizeof(xydata), &xydata);
	if (retval < 0) {
		/* recover IC with hw reset if its comms failed */
		pr_err("%s: Bus resume fail -- using hardware reset"
				" (retval=%d)\n", __func__, retval);
		goto _cyttsp_wakeup_exit;
	}
	tries = 0;
	do {
		msleep(CY_DELAY_DFLT);
		retval = ttsp_read_block_data(ts, CY_REG_BASE,
				sizeof(xydata), &xydata);
	}while ((retval < 0) || ((!(retval < 0) && ((xydata.hst_mode &
		~(CY_HNDSHK_BIT | CY_HST_MODE_CHANGE_BIT))
		+ (xydata.tt_mode & ~0xC0) +
		xydata.tt_stat)) && (tries++ < CY_DELAY_MAX)));

	if (IS_BOOTLOADERMODE(ts->xy_data.tt_mode))
	ts->power_state = CY_IDLE_STATE;
	else if (GET_HSTMODE(xydata.hst_mode) == GET_HSTMODE(CY_OPERATE_MODE))
	ts->power_state = CY_ACTIVE_STATE;
	cyttsp_pr_state(ts);
	cyttsp_dbg(ts, CY_DBG_LVL_2,
			"%s: Wake up: %s hst_mode=%02X tt_mode=%02X "
			"tt_status=%02X tries=%d ret=%d\n",
			__func__, (retval < 0) ? "FAIL" : "PASS",
			xydata.hst_mode, xydata.tt_mode,
			xydata.tt_stat, tries, retval);
	if (gpio_get_value(ts->platform_data->irq_gpio) == 0) {
		_cyttsp_hndshk(ts, ts->xy_data.hst_mode);
		usleep(1000);
	}
	_cyttsp_wakeup_exit:
#endif
	return retval;
}

#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
int cyttsp_resume(void *handle)
{
	struct cyttsp *ts = handle;
	int retval = 0;

	cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: Resuming...", __func__);

	mutex_lock(&ts->data_lock);

	/* Workaround level interrupt unmasking issue */
	if (ts->irq_enabled) {
		disable_irq_nosync(ts->irq);
		udelay(5);
		enable_irq(ts->irq);
	} else {
		enable_irq(ts->irq);
		ts->irq_enabled = true;
	}

	switch (ts->power_state) {
		case CY_SLEEP_STATE:
		retval = _cyttsp_wakeup(ts);
		if (retval < 0) {
			ts->was_suspended = false;
			pr_err("%s: wakeup fail (retval=%d)\n",
					__func__, retval);
			cyttsp_pr_state(ts);
			memset(ts->prv_trk, CY_NTCH,
					sizeof(ts->prv_trk));
			queue_work(ts->cyttsp_wq,
					&ts->cyttsp_resume_startup_work);
			pr_info("%s: startup queued\n", __func__);
			break;
		}
		break;
		case CY_BL_STATE:
		case CY_IDLE_STATE:
		case CY_INVALID_STATE:
		pr_err("%s: Detecting wrong state, Re-start up\n", __func__);
		memset(ts->prv_trk, CY_NTCH, sizeof(ts->prv_trk));
		queue_work(ts->cyttsp_wq, &ts->cyttsp_resume_startup_work);
		pr_info("%s: startup queued\n", __func__);
		break;
		case CY_READY_STATE:
		case CY_ACTIVE_STATE:
		case CY_LDR_STATE:
		case CY_SYSINFO_STATE:
		default:
		pr_err("%s: Already in %s state\n", __func__,
				cyttsp_powerstate_string[ts->power_state]);
		break;
	}

	cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: Wake Up %s\n", __func__,
			(retval < 0) ? "FAIL" : "PASS");

	mutex_unlock(&ts->data_lock);
	return retval;
}
EXPORT_SYMBOL_GPL(cyttsp_resume);

int cyttsp_suspend(void *handle)
{
	int retval = 0;
	struct cyttsp *ts = handle;
	uint8_t sleep;

	cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: Suspending...\n", __func__);

	switch (ts->power_state) {
		case CY_ACTIVE_STATE:
		mutex_lock(&ts->data_lock);

		ts->waiting_for_fw = false;
		sleep = CY_DEEP_SLEEP_MODE;
		retval = ttsp_write_block_data(ts, CY_REG_BASE +
				offsetof(struct cyttsp_xydata, hst_mode),
				sizeof(sleep), &sleep);
		if (retval < 0) {
			pr_err("%s: Failed to write sleep bit\n", __func__);
		} else {
			ts->power_state = CY_SLEEP_STATE;
			cyttsp_pr_state(ts);
		}

		disable_irq_nosync(ts->irq);
		ts->irq_enabled = false;

		mutex_unlock(&ts->data_lock);
		break;
		case CY_SLEEP_STATE:
		pr_err("%s: already in Sleep state\n", __func__);
		break;
		case CY_LDR_STATE:
		case CY_SYSINFO_STATE:
		case CY_READY_STATE:
		retval = -EBUSY;
		pr_err("%s: Suspend Blocked while in %s state\n", __func__,
				cyttsp_powerstate_string[ts->power_state]);
		break;
		case CY_IDLE_STATE:
		case CY_LOW_PWR_STATE:
		case CY_BL_STATE:
		case CY_INVALID_STATE:
		default:
		pr_err("%s: Cannot enter suspend from %s state\n", __func__,
				cyttsp_powerstate_string[ts->power_state]);
		break;
	}

	return retval;
}
EXPORT_SYMBOL_GPL(cyttsp_suspend);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
void cyttsp_early_suspend(struct early_suspend *h)
{
	struct cyttsp *ts = container_of(h, struct cyttsp, early_suspend);
	int retval = 0;

	cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: EARLY SUSPEND ts=%p\n", __func__, ts);
	retval = cyttsp_suspend(ts);
	if (retval < 0) {
		pr_err("%s: Early suspend failed with error code %d\n",
				__func__, retval);
	}
}

void cyttsp_late_resume(struct early_suspend *h)
{
	struct cyttsp *ts = container_of(h, struct cyttsp, early_suspend);
	int retval = 0;

	cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: LATE RESUME ts=%p\n", __func__, ts);
	retval = cyttsp_resume(ts);
	if (retval < 0) {
		pr_err("%s: Late resume failed with error code %d\n",
				__func__, retval);
	}
}
#endif

void cyttsp_core_release(void *handle) {
	struct cyttsp *ts = handle;

	if (ts == NULL)
		return;
	else {
#ifdef CONFIG_HAS_EARLYSUSPEND
		unregister_early_suspend(&ts->early_suspend);
#endif
		cyttsp_ldr_free(ts);
		mutex_destroy(&ts->data_lock);
		mutex_destroy(&ts->startup_mutex);
		free_irq(ts->irq, ts);
		input_unregister_device(ts->input);
		if (ts->cyttsp_wq != NULL) {
			destroy_workqueue(ts->cyttsp_wq);
			ts->cyttsp_wq = NULL;
		}
		if (ts->fwname != NULL) {
			kfree(ts->fwname);
			ts->fwname = NULL;
		}
		kfree(ts);
		ts = NULL;
		return;
	}
}
EXPORT_SYMBOL_GPL(cyttsp_core_release);

static int cyttsp_open(struct input_dev *dev) {
	int retval = 0;

	struct cyttsp *ts = input_get_drvdata(dev);

	if (ts == NULL) {
		pr_err("%s: NULL context pointer\n", __func__);
		return -ENOMEM;
	}

	mutex_lock(&ts->startup_mutex);
	if (!ts->powered) {
		retval = cyttsp_power_on(ts);

		/* Powered if no hard failure */
		if (retval == 0)
			ts->powered = true;
		else
			ts->powered = false;

		pr_info("%s: Powered ON(%d) r=%d\n",
				__func__, (int)ts->powered, retval);
	}
	mutex_unlock(&ts->startup_mutex);

	return retval;
}

static void cyttsp_close(struct input_dev *dev) {
	/*
	 * close() normally powers down the device
	 * this call simply returns unless power
	 * to the device can be controlled by the driver
	 */
	return;
}

void *cyttsp_core_init(struct cyttsp_bus_ops *bus_ops, struct device *dev,
		int irq, char *name) {
	int i;
	int min = 0;
	int max = 0;
	int signal;
	int retval = 0;
	struct input_dev *input_device;
	struct cyttsp *ts = kzalloc(sizeof(*ts), GFP_KERNEL);

	if (ts == NULL) {
		pr_err("%s: Error, kzalloc\n", __func__);
		return ts;
	}

	ts->cyttsp_wq = create_singlethread_workqueue("cyttsp_resume_startup_w");
	if (ts->cyttsp_wq == NULL) {
		pr_err("%s: No memory for cyttsp_resume_startup_wq\n", __func__);
		goto err_alloc_wq_failed;
	}

	ts->fwname = kzalloc(CY_BL_FW_NAME_SIZE, GFP_KERNEL);
	if ((ts->fwname == NULL) || (dev == NULL) || (bus_ops == NULL)) {
		pr_err("%s: Error, dev, bus_ops, or fwname null\n", __func__);
		kfree(ts);
		ts = NULL;
		goto error_alloc_data;
	}

	ts->power_state = CY_INVALID_STATE;
	ts->waiting_for_fw = false;
	ts->powered = false;
	ts->was_suspended = false;
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	ts->ic_grpnum = 0;
	ts->ic_grpoffset = 0;
	memset(ts->ic_grpstart, 0, sizeof(ts->ic_grpstart));
	ts->ic_grpstart[CY_IC_GRPNUM_SI_TAG] = CY_REG_SI_START;
	ts->ic_grpstart[CY_IC_GRPNUM_OP_TAG] = CY_REG_OP_START;
	ts->used_ic_reflash = false;
#endif

	mutex_init(&ts->data_lock);
	mutex_init(&ts->startup_mutex);
	ts->dev = dev;

#ifdef CYT_BRINGUP
	ts->platform_data = devm_kzalloc(dev,
		sizeof(struct cyttsp_platform_data), GFP_KERNEL);
	if (!ts->platform_data) {
		dev_err(dev, "Failed to allocate memory\n");
		goto error_alloc_data;
	}
	init_platform_settings(dev, ts->platform_data);
#else
	ts->platform_data = dev->platform_data;
#endif

	ts->bus_ops = bus_ops;
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	ts->bus_ops->tsdebug = CY_DBG_LVL_0;
#endif
	init_completion(&ts->bl_int_running);
	init_completion(&ts->si_int_running);
	init_completion(&ts->op_int_running);
	init_completion(&ts->ld_int_running);
	ts->flags = ts->platform_data->flags;


	/* Create the input device and register it. */
	input_device = input_allocate_device();
	if (!input_device) {
		pr_err("%s: Error, failed to allocate input device\n", __func__);
		goto error_input_allocate_device;
	}

	ts->input = input_device;
	input_device->name = name;
	snprintf(ts->phys, sizeof(ts->phys), "%s", dev_name(dev));
	input_device->phys = ts->phys;
	input_device->dev.parent = ts->dev;
	ts->bus_type = bus_ops->dev->bus;
	input_device->open = cyttsp_open;
	input_device->close = cyttsp_close;
	input_set_drvdata(input_device, ts);
	dev_set_drvdata(dev, ts);

	_cyttsp_init_tch_map(ts);
	memset(ts->prv_trk, CY_NTCH, sizeof(ts->prv_trk));

	__set_bit(EV_ABS, input_device->evbit);
	__set_bit(INPUT_PROP_DIRECT, input_device->propbit);

	for (i = 0; i < (ts->platform_data->frmwrk->size / CY_NUM_ABS_SET); i++) {
		signal = ts->platform_data->frmwrk->abs[(i * CY_NUM_ABS_SET)
				+ CY_SIGNAL_OST];
		if (signal != CY_IGNORE_VALUE) {
			min = ts->platform_data->frmwrk->abs[(i * CY_NUM_ABS_SET)
					+ CY_MIN_OST];
			max = ts->platform_data->frmwrk->abs[(i * CY_NUM_ABS_SET)
					+ CY_MAX_OST];
			if (i == CY_ABS_ID_OST) {
				/* shift track ids down to start at 0 */
				max = max - min;
				min = min - min;
			}
			input_set_abs_params(
					input_device,
					signal,
					min,
					max,
					ts->platform_data->frmwrk->abs[(i * CY_NUM_ABS_SET)
							+ CY_FUZZ_OST],
					ts->platform_data->frmwrk->abs[(i * CY_NUM_ABS_SET)
							+ CY_FLAT_OST]);
		}
	}

	input_set_events_per_packet(input_device, 6 * CY_NUM_TCH_ID);

	if (ts->platform_data->frmwrk->enable_vkeys)
		input_set_capability(input_device, EV_KEY, KEY_PROG1);

	/*
	 * Enable interrupts
	 * Doing this here in case another driver tries to open the device
	 * as part of the input_register_device call,
	 * which will try to init the device before interrupts are requested.
	 */
	ts->irq = irq;
	if (ts->irq <= 0) {
		pr_err("%s: Error, failed to allocate irq\n", __func__);
		goto error_init;
	}

	/* Initalize all GPIOs */
	retval = gpio_request(ts->platform_data->reset_gpio, "touch_rst");
	if (retval) {
		pr_err("%s: failed to get reset: %d\n", __func__, retval);
		goto error_init;
	}

	retval = gpio_direction_output(ts->platform_data->reset_gpio, 1);
	if (retval) {
		pr_err("%s: Failed to setup %d(RST) for output.\n",
			__func__, ts->platform_data->reset_gpio);
		goto error_gpio_reset;
	}
	gpio_set_value(ts->platform_data->reset_gpio, 0);

	retval = gpio_request(ts->platform_data->irq_gpio, "touch_intr");
	if (retval) {
		pr_err("%s: failed to get irq: %d\n", __func__, retval);
		goto error_gpio_reset;
	}

	retval = gpio_direction_input(ts->platform_data->irq_gpio);
	if (retval) {
		pr_err("%s: Failed to setup %d(INT) irq for input.\n",
			__func__, ts->platform_data->irq_gpio);
		goto error_gpio_irq;
	}

	INIT_WORK(&ts->cyttsp_resume_startup_work, cyttsp_ts_work_func);
	cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: Initialize IRQ: %d, name: %s\n",
			__func__, ts->irq, ts->input->name);
	retval = request_threaded_irq(ts->irq, NULL, cyttsp_irq,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			ts->input->name, ts);
	if (retval < 0) {
		pr_err("%s: IRQ request failed r=%d\n", __func__, retval);
		ts->irq_enabled = false;
		goto error_request_irq;
	} else {
		cyttsp_dbg(ts, CY_DBG_LVL_3, "%s: IRQ is intialized",
					__func__);
		ts->irq_enabled = true;
	}

	/* I heard mutexes solved a problem once, lets add them.... */
	mutex_lock(&ts->data_lock);
	retval = _cyttsp_detect_hw(ts);
	mutex_unlock(&ts->data_lock);
	if (retval) {
		pr_err("%s: couldn't detect hardware\n", __func__);
		goto error_hw_detect;
	}

	/*
	 * Below this point, the cyttsp_open function may be called
	 * and forced to complete before returning.
	 */
	retval = input_register_device(input_device);
	if (retval < 0) {
		pr_err("%s: Error, failed to register input device r=%d\n",
				__func__, retval);
		goto error_hw_detect;
	}

	/* Add /sys files */
	cyttsp_ldr_init(ts);

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = cyttsp_early_suspend;
	ts->early_suspend.resume = cyttsp_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif
	goto no_error;

error_hw_detect:
	disable_irq(ts->irq);
	free_irq(ts->irq, ts);
error_request_irq:
error_gpio_irq:
	gpio_free(ts->platform_data->irq_gpio);
error_gpio_reset:
	gpio_free(ts->platform_data->reset_gpio);
error_init:
	input_free_device(input_device);
error_input_allocate_device:
#ifdef CYT_BRINGUP
	exit_platform_settings(dev);
#endif
	mutex_destroy(&ts->data_lock);
	mutex_destroy(&ts->startup_mutex);
	kfree(ts->fwname);
error_alloc_data:
	destroy_workqueue(ts->cyttsp_wq);
err_alloc_wq_failed:
	kfree(ts);
	ts = NULL;
no_error:
	return ts;
}
EXPORT_SYMBOL_GPL(cyttsp_core_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard touchscreen driver core");
MODULE_AUTHOR("Cypress");
