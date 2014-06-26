/*
 * intel_scu_ipc.c: Driver for the Intel SCU IPC mechanism
 *
 * (C) Copyright 2008-2010 Intel Corporation
 * Author: Sreedhara DS (sreedhara.ds@intel.com)
 * (C) Copyright 2010 Intel Corporation
 * Author: Sudha Krishnakumar (sudha.krishnakumar@intel.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This driver provides ioctl interfaces to call intel scu ipc driver api
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/io.h>
#include <linux/rpmsg.h>
#include <asm/intel_scu_ipc.h>
#include <asm/intel_scu_pmic.h>
#include <asm/intel_scu_ipcutil.h>
#include <asm/intel-mid.h>
#include <asm/intel_mid_rpmsg.h>
#include <linux/platform_data/intel_mid_remoteproc.h>
#include <linux/pm_runtime.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#define MAX_FW_SIZE 264192

#define PMIT_RESET1_OFFSET		14
#define PMIT_RESET2_OFFSET		15

#define IPC_RESIDENCY_CMD_ID_START	0
#define IPC_RESIDENCY_CMD_ID_DUMP	2

#define SRAM_ADDR_S0IX_RESIDENCY	0xFFFF71E0
#define ALL_RESIDENCY_DATA_SIZE		12

#define DUMP_OSNIB

#define OSHOB_EXTEND_DESC_SIZE	52  /* OSHOB header+osnib+oem info: 52 bytes.*/

#define OSHOB_HEADER_MAGIC_SIZE	4   /* Size (bytes) of magic number in OSHOB */
				    /* header.                               */

#define OSHOB_MAGIC_NUMBER	"$OH$"	/* If found when reading the first   */
					/* 4 bytes of the OSOHB zone, it     */
					/* means that the new extended OSHOB */
					/* is going to be used.              */

#define OSHOB_REV_MAJ_DEFAULT	0	/* Default revision number of OSHOB. */
#define OSHOB_REV_MIN_DEFAULT	1	/* If 0.1 the default OSHOB is used  */
					/* instead of the extended one.      */

/* Defines for the SCU buffer included in OSHOB structure. */
#define OSHOB_SCU_BUF_BASE_DW_SIZE	1   /* In dwords. By default SCU     */
					    /* buffer size is 1 dword.       */

#define OSHOB_SCU_BUF_MRFLD_DW_SIZE (4*OSHOB_SCU_BUF_BASE_DW_SIZE)
					    /* In dwords. On Merrifield the  */
					    /* SCU trace buffer size is      */
					    /* 4 dwords.                     */
#define OSHOB_DEF_FABRIC_ERR_MRFLD_SIZE   50	/* In DWORDS. For Merrifield.*/
					/* Fabric error log size (in DWORDS).*/
					/* From offs 0x44 to 0x10C.          */
					/* Used in default OSHOB.            */

#define OSNIB_SIZE		32	/* Size (bytes) of the default OSNIB.*/

#define OSNIB_INTEL_RSVD_SIZE	24	/* Size (bytes) of Intel RESERVED in */
					/* OSNIB.                            */
#define OSNIB_OEM_RSVD_SIZE	96	/* Size (bytes) of OEM RESERVED      */
					/* in OSNIB.                         */

#define OSNIB_NVRAM_SIZE	128	/* Size (bytes) of NVRAM             */
					/* in OSNIB.                         */

#define OSHOB_DEF_FABRIC_ERR_SIZE   50	/* In DWORDS.                        */
					/* Fabric error log size (in DWORDS).*/
					/* From offs 0x44 to 0x10C.          */
					/* Used in default OSHOB.            */

#define OSHOB_FABRIC_ERROR1_SIZE  12    /* 1st part of Fabric error dump.    */
					/* Used in extended OSHOB.           */

#define OSHOB_FABRIC_ERROR2_SIZE  9     /* 2nd part of Fabric error dump.    */
					/* Used in extended OSHOB.           */

#define OSHOB_RESERVED_DEBUG_SIZE 5     /* Reserved for debug                */

/* Size (bytes) of the default OSHOB structure. Includes the default OSNIB   */
/* size.                                                                     */
#define OSHOB_SIZE	(68 + (4*OSHOB_SCU_BUF_BASE_DW_SIZE) + \
			    (4*OSHOB_DEF_FABRIC_ERR_SIZE))	/* In bytes. */

#define OSHOB_MRFLD_SIZE (68 + (4*OSHOB_SCU_BUF_MRFLD_DW_SIZE) + \
			    (4*OSHOB_DEF_FABRIC_ERR_MRFLD_SIZE))/* In bytes. */

/* SCU buffer size is give in dwords. So it is x4 here to get the total      */
/* number of bytes.                                                          */

#define SCU_TRACE_HEADER_SIZE    16     /* SCU trace header                  */

#define CHAABI_DEBUG_DATA_SIZE   5      /* Reserved for chaabi debug         */

#define OSHOB_RESERVED_SIZE      184    /* Reserved                          */


struct chip_reset_event {
	int id;
	const char *reset_ev1_name;
	const char *reset_ev2_name;
};

static struct chip_reset_event chip_reset_events[] = {
	{ INTEL_MID_CPU_CHIP_ANNIEDALE, "RESETSRC0", "RESETSRC1" },
	{ INTEL_MID_CPU_CHIP_TANGIER, "RESETSRC0", "RESETSRC1" },
	{ INTEL_MID_CPU_CHIP_CLOVERVIEW, "RESETIRQ1", "RESETIRQ2" },
	{ INTEL_MID_CPU_CHIP_PENWELL, "RESETIRQ1", "RESETIRQ2" },
};

struct osnib_target_os {
	const char *target_os_name;
	int id;
};

static struct osnib_target_os osnib_target_oses[] = {
	{ "main", SIGNED_MOS_ATTR },
	{ "charging", SIGNED_COS_ATTR  },
	{ "recovery", SIGNED_RECOVERY_ATTR },
	{ "fastboot", SIGNED_POS_ATTR },
	{ "factory", SIGNED_FACTORY_ATTR },
	{ "factory2", SIGNED_FACTORY2_ATTR },
};


struct osnib_wake_src {
	u8 id;
	const char *wakesrc_name;
};

static struct osnib_wake_src osnib_wake_srcs[] = {
	{ WAKE_BATT_INSERT, "battery inserted" },
	{ WAKE_PWR_BUTTON_PRESS, "power button pressed" },
	{ WAKE_RTC_TIMER, "rtc timer" },
	{ WAKE_USB_CHRG_INSERT, "usb charger inserted" },
	{ WAKE_RESERVED, "reserved" },
	{ WAKE_REAL_RESET, "real reset" },
	{ WAKE_COLD_BOOT, "cold boot" },
	{ WAKE_UNKNOWN, "unknown" },
	{ WAKE_KERNEL_WATCHDOG_RESET, "kernel watchdog reset" },
	{ WAKE_SECURITY_WATCHDOG_RESET, "security watchdog reset" },
	{ WAKE_WATCHDOG_COUNTER_EXCEEDED, "watchdog counter exceeded" },
	{ WAKE_POWER_SUPPLY_DETECTED, "power supply detected" },
	{ WAKE_FASTBOOT_BUTTONS_COMBO, "fastboot combo" },
	{ WAKE_NO_MATCHING_OSIP_ENTRY, "no matching osip entry" },
	{ WAKE_CRITICAL_BATTERY, "critical battery" },
	{ WAKE_INVALID_CHECKSUM, "invalid checksum" },
	{ WAKE_FORCED_RESET, "forced reset"},
	{ WAKE_ACDC_CHRG_INSERT, "ac charger inserted" },
	{ WAKE_PMIC_WATCHDOG_RESET, "pmic watchdog reset" },
	{ WAKE_PLATFORM_WATCHDOG_RESET, "HWWDT reset platform" },
	{ WAKE_SC_WATCHDOG_RESET, "HWWDT reset SC" },
};


/* OSNIB allocation. */
struct scu_ipc_osnib {
	u8 target_mode;        /* Target mode.                      */
	u8 wd_count;           /* Software watchdog.                */
	u8 alarm;              /* RTC alarm.                        */
	u8 wakesrc;            /* WAKESRC.                          */
	u8 reset_ev1;          /* RESETIRQ1 or RESETSRC0.           */
	u8 reset_ev2;          /* RESETIRQ2 or RESETSRC1.           */
	u8 spare;              /* Spare.                            */
	u8 intel_reserved[OSNIB_INTEL_RSVD_SIZE]; /* INTEL RESERVED */
			       /* (offsets 7 to 30).                */
	u8 checksum;           /* CHECKSUM.                         */
	u8 oem_reserved[OSNIB_OEM_RSVD_SIZE];     /* OEM RESERVED   */
			       /* (offsets 32 to 127).              */
	u8 nvram[OSNIB_NVRAM_SIZE];               /* NVRAM          */
			       /* (offsets 128 to 255).             */
};

/* Default OSHOB allocation. */
struct scu_ipc_oshob {
	u32 scutxl;             /* SCUTxl offset position.      */
	u32 iatxl;              /* IATxl offset.                */
	u32 bocv;               /* BOCV offset.                 */
	u8 osnibr[OSNIB_SIZE];  /* OSNIB area offset.           */
	u32 pmit;               /* PMIT offset.                 */
	u32 pemmcmhki;          /* PeMMCMHKI offset.            */
	u32 osnibw_ptr;         /* OSNIB Write at offset 0x34.  */
	u32 fab_err_log[OSHOB_DEF_FABRIC_ERR_SIZE]; /* Fabric   */
				/* error log buffer.            */
};

/* Extended OSHOB allocation. version 1.3 */
struct scu_ipc_oshob_extend {
	u32 magic;              /* MAGIC number.                           */
	u8  rev_major;          /* Revision major.                         */
	u8  rev_minor;          /* Revision minor.                         */
	u16 oshob_size;         /* OSHOB size.                             */
	u32 nvram_addr;         /* NVRAM phys addres                       */
	u32 scutxl;             /* SCUTxl offset position.                 */
				/* If on MRFLD platform, next param may be */
				/* shifted by                              */
				/* (OSHOB_SCU_BUF_MRFLD_DW_SIZE - 1) bytes.*/
	u32 iatxl;              /* IATxl.                                  */
	u32 bocv;               /* BOCV.                                   */

	u16 intel_size;         /* Intel size (in OSNIB area).             */
	u16 oem_size;           /* OEM size (of OEM area).                 */
	u32 r_intel_ptr;        /* Read Intel pointer.                     */
	u32 w_intel_ptr;        /* Write Intel pointer.                    */
	u32 r_oem_ptr;          /* Read OEM pointer.                       */
	u32 w_oem_ptr;          /* Write OEM pointer.                      */

	u32 pmit;               /* PMIT.                       */
	u32 pemmcmhki;          /* PeMMCMHKI.                  */

	/* OSHOB as defined for CLOVERVIEW */
	u32 nvram_size;         /* NVRAM max size in bytes     */
	u32 fabricerrlog1[OSHOB_FABRIC_ERROR1_SIZE]; /* fabric error data */
	u8  vrtc_alarm_dow;     /* Alarm sync                  */
	u8  vrtc_alarm_dom;     /* Alarm sync                  */
	u8  vrtc_alarm_month;   /* Alarm sync                  */
	u8  vrtc_alarm_year;    /* Alarm sync                  */
	u32 reserved_debug[OSHOB_RESERVED_DEBUG_SIZE];/* Reserved Debug data */
	u32 reserved2;          /* Reserved                    */
	u32 fabricerrlog2[OSHOB_FABRIC_ERROR2_SIZE]; /* fabric error data2 */
	u32 sculogbufferaddr;   /* phys addr of scu log buffer   */
	u32 sculogbuffersize;   /* size of scu log buffer      */
};

/* Extended OSHOB allocation. version 1.4. */
struct scu_ipc_oshob_extend_v14 {
	u32 magic;              /* MAGIC number.                           */
	u8  rev_major;          /* Revision major.                         */
	u8  rev_minor;          /* Revision minor.                         */
	u16 oshob_size;         /* OSHOB size.                             */

	u32 scutxl;             /* SCUTxl offset position.                 */
				/* If on MRFLD platform, next param may be */
				/* shifted by                              */
				/* (OSHOB_SCU_BUF_MRFLD_DW_SIZE - 1) bytes.*/
	u32 iatxl;              /* IATxl.                                  */
	u32 bocv;               /* BOCV.                                   */

	u32 osnib_ptr;          /* The unique OSNIB pointer.               */

	u32 pmit;               /* PMIT.                                   */
	u8  scutraceheader[SCU_TRACE_HEADER_SIZE];   /* SCU trace header   */
	u32 fabricerrlog[OSHOB_DEF_FABRIC_ERR_SIZE]; /* fabric error data  */
	u32 chaabidebugdata[CHAABI_DEBUG_DATA_SIZE]; /* fabric error data  */
	u32 pmuemergency;       /* pmu emergency                           */
	u32 sculogbufferaddr;   /* scu log buffer address                  */
	u32 sculogbuffersize;   /* size of scu log buffer                  */
	u32 oshob_reserved[OSHOB_RESERVED_SIZE];     /* oshob reserved     */
};

struct scu_ipc_oshob_info {
	u8      oshob_majrev;   /* Major revision number of OSHOB structure. */
	u8      oshob_minrev;   /* Minor revision number of OSHOB structure. */
	u16     oshob_size;     /* Total size (bytes) of OSHOB structure.    */
	u32     scu_trace[OSHOB_SCU_BUF_BASE_DW_SIZE*4]; /* SCU trace buffer.*/
				/* Set to max SCU buffer size (dwords) to    */
				/* adapt to MRFLD. On other platforms, only  */
				/* the first dword is stored and read.       */
	u32     ia_trace;       /* IA trace buffer.                          */
	u16     osnib_size;     /* Total size (bytes) of OSNIB structure.    */
	u16     oemnib_size;    /* Total size (bytes) of OEMNIB area.        */
	u32     scu_trace_size; /* SCU extended trace buffer size            */
	u32     nvram_size;     /* NV ram size in bytes                      */

	phys_addr_t   oshob_base;     /* Base address of OSHOB. Use ioremap  */
				      /* to remap for access.                */
	phys_addr_t   osnibr_ptr;     /* Pointer to Intel read zone.         */
	phys_addr_t   osnibw_ptr;     /* Pointer to Intel write zone.        */
	phys_addr_t   oemnibr_ptr;    /* Pointer to OEM read zone.           */
	phys_addr_t   oemnibw_ptr;    /* Pointer to OEM write zone.          */
	phys_addr_t   scu_trace_buf;  /* SCU extended trace buffer           */
	phys_addr_t   nvram_addr;     /* NV ram phys addr                    */

	int (*scu_ipc_write_osnib)(u8 *data, int len, int offset);
	int (*scu_ipc_read_osnib)(u8 *data, int len, int offset);

	int platform_type;     /* Identifies the platform (list of supported */
			       /* platforms is given in intel-mid.h).        */

	u16 offs_add;          /* The additional shift bytes to consider     */
			       /* giving the offset at which the OSHOB params*/
			       /* will be read. If MRFLD it must be set to   */
			       /* take into account the extra SCU dwords.    */

};

/* Structure for OSHOB info */
struct scu_ipc_oshob_info *oshob_info;

static struct rpmsg_instance *ipcutil_instance;

/* Mode for Audio clock */
static DEFINE_MUTEX(osc_clk0_lock);
static unsigned int osc_clk0_mode;

int intel_scu_ipc_osc_clk(u8 clk, unsigned int khz)
{
	/* SCU IPC COMMAND(osc clk on/off) definition:
	 * ipc_wbuf[0] = clock to act on {0, 1, 2, 3}
	 * ipc_wbuf[1] =
	 * bit 0 - 1:on  0:off
	 * bit 1 - if 1, read divider setting from bits 3:2 as follows:
	 * bit [3:2] - 00: clk/1, 01: clk/2, 10: clk/4, 11: reserved
	 */
	unsigned int base_freq;
	unsigned int div;
	u8 ipc_wbuf[2];
	int ipc_ret;

	if (clk > 3)
		return -EINVAL;

	ipc_wbuf[0] = clk;
	ipc_wbuf[1] = 0;
	if (khz) {
#ifdef CONFIG_CTP_CRYSTAL_38M4
		base_freq = 38400;
#else
		base_freq = 19200;
#endif
		div = fls(base_freq / khz) - 1;
		if (div >= 3 || (1 << div) * khz != base_freq)
			return -EINVAL;	/* Allow only exact frequencies */
		ipc_wbuf[1] = 0x03 | (div << 2);
	}

	ipc_ret = rpmsg_send_command(ipcutil_instance,
		RP_OSC_CLK_CTRL, 0, ipc_wbuf, NULL, 2, 0);
	if (ipc_ret != 0)
		pr_err("%s: failed to set osc clk(%d) output\n", __func__, clk);

	return ipc_ret;
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_osc_clk);

/*
 * OSC_CLK_AUDIO is connected to the MSIC as well as Audience, so it should be
 * turned on if any one of them requests it to be on and it should be turned off
 * only if no one needs it on.
 */
int intel_scu_ipc_set_osc_clk0(unsigned int enable, enum clk0_mode mode)
{
	int ret = 0, clk_enable;
	static const unsigned int clk_khz = 19200;

	pr_info("set_clk0 request %s for Mode 0x%x\n",
				enable ? "ON" : "OFF", mode);
	mutex_lock(&osc_clk0_lock);
	if (mode == CLK0_QUERY) {
		ret = osc_clk0_mode;
		goto out;
	}
	if (enable) {
		/* if clock is already on, just add new user */
		if (osc_clk0_mode) {
			osc_clk0_mode |= mode;
			goto out;
		}
		osc_clk0_mode |= mode;
		pr_info("set_clk0: enabling clk, mode 0x%x\n", osc_clk0_mode);
		clk_enable = 1;
	} else {
		osc_clk0_mode &= ~mode;
		pr_info("set_clk0: disabling clk, mode 0x%x\n", osc_clk0_mode);
		/* others using the clock, cannot turn it of */
		if (osc_clk0_mode)
			goto out;
		clk_enable = 0;
	}
	pr_info("configuring OSC_CLK_AUDIO now\n");
	ret = intel_scu_ipc_osc_clk(OSC_CLK_AUDIO, clk_enable ? clk_khz : 0);
out:
	mutex_unlock(&osc_clk0_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_set_osc_clk0);

#define MSIC_VPROG1_CTRL        0xD6
#define MSIC_VPROG2_CTRL        0xD7

#define MSIC_VPROG2_ON          0x36 /*1.200V and Auto mode*/
#define MSIC_VPROG1_ON          0xF6 /*2.800V and Auto mode*/
#define MSIC_VPROG_OFF          0x24 /*1.200V and OFF*/

/* Defines specific of MRFLD platform (CONFIG_X86_MRFLD). */
#define MSIC_VPROG1_MRFLD_CTRL	0xAC
#define MSIC_VPROG2_MRFLD_CTRL	0xAD
#define MSIC_VPROG3_MRFLD_CTRL	0xAE

#define MSIC_VPROG1_MRFLD_ON	0xC1	/* 2.80V and Auto mode */
#define MSIC_VPROG2_MRFLD_ON	0xC1	/* 2.80V and Auto mode */
#define MSIC_VPROG3_MRFLD_ON	0x01	/* 1.05V and Auto mode */
#define MSIC_VPROG_MRFLD_OFF	0	/* OFF */
/* End of MRFLD specific.*/

/* Helpers to turn on/off msic vprog1, vprog2 and vprog3 */
int intel_scu_ipc_msic_vprog1(int on)
{
	if ((oshob_info->platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(oshob_info->platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE))
		return intel_scu_ipc_iowrite8(MSIC_VPROG1_MRFLD_CTRL,
			on ? MSIC_VPROG1_MRFLD_ON : MSIC_VPROG_MRFLD_OFF);
	else
		return intel_scu_ipc_iowrite8(MSIC_VPROG1_CTRL,
			on ? MSIC_VPROG1_ON : MSIC_VPROG_OFF);
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_msic_vprog1);

int intel_scu_ipc_msic_vprog2(int on)
{
	if ((oshob_info->platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(oshob_info->platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE))
		return intel_scu_ipc_iowrite8(MSIC_VPROG2_MRFLD_CTRL,
			on ? MSIC_VPROG2_MRFLD_ON : MSIC_VPROG_MRFLD_OFF);
	else
		return intel_scu_ipc_iowrite8(MSIC_VPROG2_CTRL,
			on ? MSIC_VPROG2_ON : MSIC_VPROG_OFF);
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_msic_vprog2);

int intel_scu_ipc_msic_vprog3(int on)
{
	if (oshob_info->platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE)
		return intel_scu_ipc_iowrite8(MSIC_VPROG3_MRFLD_CTRL,
			on ? MSIC_VPROG3_MRFLD_ON : MSIC_VPROG_MRFLD_OFF);
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_msic_vprog3);

/**
 *	scu_reg_access		-	implement register access ioctls
 *	@cmd: command we are doing (read/write/update)
 *	@data: kernel copy of ioctl data
 *
 *	Allow the user to perform register accesses on the SCU via the
 *	kernel interface
 */

static int scu_reg_access(u32 cmd, struct scu_ipc_data  *data)
{
	int ret;

	if (data->count == 0 || data->count > 5)
		return -EINVAL;

	switch (cmd) {
	case INTEL_SCU_IPC_REGISTER_READ:
		ret = intel_scu_ipc_readv(data->addr, data->data, data->count);
		break;
	case INTEL_SCU_IPC_REGISTER_WRITE:
		ret = intel_scu_ipc_writev(data->addr, data->data, data->count);
		break;
	case INTEL_SCU_IPC_REGISTER_UPDATE:
		ret = intel_scu_ipc_update_register(data->addr[0],
							data->data[0],
							data->mask);
		break;
	default:
		return -ENOTTY;
	}
	return ret;
}

#define check_pmdb_sub_cmd(x)	(x == PMDB_SUB_CMD_R_OTPCTL || \
		x == PMDB_SUB_CMD_R_WMDB || x == PMDB_SUB_CMD_W_WMDB || \
		x == PMDB_SUB_CMD_R_OTPDB || x == PMDB_SUB_CMD_W_OTPDB)
#define pmdb_sub_cmd_is_read(x)	(x == PMDB_SUB_CMD_R_OTPCTL || \
		x == PMDB_SUB_CMD_R_WMDB || x == PMDB_SUB_CMD_R_OTPDB)

static int check_pmdb_buffer(struct scu_ipc_pmdb_buffer *p_buf)
{
	int size;

	switch (p_buf->sub) {
	case PMDB_SUB_CMD_R_WMDB:
	case PMDB_SUB_CMD_W_WMDB:
		size = PMDB_WMDB_SIZE;
		break;
	case PMDB_SUB_CMD_R_OTPDB:
	case PMDB_SUB_CMD_W_OTPDB:
		size = PMDB_OTPDB_SIZE;
		break;
	case PMDB_SUB_CMD_R_OTPCTL:
		size = PMDB_OTPCTL_SIZE;
		break;
	default:
		size = 0;
	}

	return check_pmdb_sub_cmd(p_buf->sub) &&
		(p_buf->count + p_buf->offset < size) &&
		(p_buf->count % 4 == 0);
}

/**
 *	scu_pmdb_access	-	access PMDB data through SCU IPC cmds
 *	@p_buf: PMDB access buffer, it describe the data to write/read.
 *		p_buf->sub - SCU IPC sub cmd of PMDB access,
 *			this sub cmd distinguish different componet
 *			in PMDB which to be accessd. (WMDB, OTPDB, OTPCTL)
 *		p_buf->count - access data's count;
 *		p_buf->offset - access data's offset for each component in PMDB;
 *		p_buf->data - data to write/read.
 *
 *	Write/read data to/from PMDB.
 *
 */
static int scu_pmdb_access(struct scu_ipc_pmdb_buffer *p_buf)
{
	int i, offset, ret = -EINVAL;
	u8 *p_data;

	if (!check_pmdb_buffer(p_buf)) {
		pr_err("Invalid PMDB buffer!\n");
		return -EINVAL;
	}

	/* 1. we use rpmsg_send_raw_command() IPC cmd interface
	 *    to access PMDB data. Each call of rpmsg_send_raw_command()
	 *    can only access at most PMDB_ACCESS_SIZE bytes' data.
	 * 2. There are two kinds of pmdb sub commands, read command
	 *    and write command. For read command, we must transport
	 *    in and out buffer to rpmsg_send_raw_command(), because
	 *    in buffer length is pass as access length which must
	 *    be transported to SCU.
	 */
	p_data = p_buf->data;
	offset = p_buf->offset;
	for (i = 0; i < p_buf->count/PMDB_ACCESS_SIZE; i++) {
		if (pmdb_sub_cmd_is_read(p_buf->sub))
			ret = rpmsg_send_raw_command(ipcutil_instance,
					RP_PMDB, p_buf->sub,
					p_data, (u32 *)p_data,
					PMDB_ACCESS_SIZE, PMDB_ACCESS_SIZE / 4,
					0, offset);
		else
			ret = rpmsg_send_raw_command(ipcutil_instance,
					RP_PMDB, p_buf->sub,
					p_data, NULL, PMDB_ACCESS_SIZE,
					0, 0, offset);
		if (ret < 0) {
			pr_err("intel_scu_ipc_raw_cmd failed!\n");
			return ret;
		}
		offset += PMDB_ACCESS_SIZE;
		p_data += PMDB_ACCESS_SIZE;
	}
	if (p_buf->count % PMDB_ACCESS_SIZE > 0) {
		if (pmdb_sub_cmd_is_read(p_buf->sub))
			ret = rpmsg_send_raw_command(ipcutil_instance,
					RP_PMDB, p_buf->sub,
					p_data, (u32 *)p_data,
					p_buf->count % PMDB_ACCESS_SIZE,
					(p_buf->count % PMDB_ACCESS_SIZE) / 4,
					0, offset);
		else
			ret = rpmsg_send_raw_command(ipcutil_instance,
					RP_PMDB, p_buf->sub,
					p_data, NULL,
					p_buf->count % PMDB_ACCESS_SIZE,
					0, 0, offset);
		if (ret < 0) {
			pr_err("intel_scu_ipc_raw_cmd failed!\n");
			return ret;
		}
	}

	return 0;
}

static int do_pmdb_user_buf_access(void __user *argp)
{
	int ret;
	struct scu_ipc_pmdb_buffer *p_buf;

	p_buf = kzalloc(sizeof(struct scu_ipc_pmdb_buffer), GFP_KERNEL);
	if (p_buf == NULL) {
		pr_err("failed to allocate memory for pmdb buffer!\n");
		return -ENOMEM;
	}

	ret = copy_from_user(p_buf, argp, sizeof(struct scu_ipc_pmdb_buffer));
	if (ret < 0) {
		pr_err("copy from user failed!!\n");
		goto err;
	}

	ret = scu_pmdb_access(p_buf);
	if (ret < 0) {
		pr_err("scu_pmdb_access error!\n");
		goto err;
	}

	if (pmdb_sub_cmd_is_read(p_buf->sub)) {
		ret = copy_to_user(argp + 3 * sizeof(u32),
					p_buf->data, p_buf->count);
		if (ret < 0)
			pr_err("copy to user failed!!\n");
	}

err:
	kfree(p_buf);
	return ret;
}

/**
 *	scu_ipc_ioctl		-	control ioctls for the SCU
 *	@fp: file handle of the SCU device
 *	@cmd: ioctl coce
 *	@arg: pointer to user passed structure
 *
 *	Support the I/O and firmware flashing interfaces of the SCU
 */
static long scu_ipc_ioctl(struct file *fp, unsigned int cmd,
							unsigned long arg)
{
	int ret = -EINVAL;
	struct scu_ipc_data  data;
	void __user *argp = (void __user *)arg;

	/* Only IOCTL cmd allowed to pass through without capability check */
	/* is getting fw version info, all others need to check to prevent */
	/* arbitrary access to all sort of bit of the hardware exposed here*/

	if ((cmd != INTEL_SCU_IPC_FW_REVISION_GET &&
		cmd != INTEL_SCU_IPC_FW_REVISION_EXT_GET &&
		cmd != INTEL_SCU_IPC_S0IX_RESIDENCY) &&
		!capable(CAP_SYS_RAWIO))
		return -EPERM;

	switch (cmd) {
	case INTEL_SCU_IPC_S0IX_RESIDENCY:
	{
		void __iomem *s0ix_residencies_addr;
		u8 dump_results[ALL_RESIDENCY_DATA_SIZE] = {0};
		u32 cmd_id;

		if (copy_from_user(&cmd_id, argp, sizeof(u32))) {
			pr_err("copy from user failed!!\n");
			return -EFAULT;
		}

		/* Check get residency counter valid cmd range */

		if (cmd_id > IPC_RESIDENCY_CMD_ID_DUMP) {
			pr_err("invalid si0x residency sub-cmd id!\n");
			return -EINVAL;
		}

		ret = rpmsg_send_simple_command(ipcutil_instance,
					RP_S0IX_COUNTER, cmd_id);

		if (ret < 0) {
			pr_err("ipc_get_s0ix_counter failed!\n");
			return ret;
		}

		if (cmd_id == IPC_RESIDENCY_CMD_ID_DUMP) {
			s0ix_residencies_addr = ioremap_nocache(
				SRAM_ADDR_S0IX_RESIDENCY,
				ALL_RESIDENCY_DATA_SIZE);

			if (!s0ix_residencies_addr) {
				pr_err("ioremap SRAM address failed!!\n");
				return -EFAULT;
			}

			memcpy(&dump_results[0], s0ix_residencies_addr,
				ALL_RESIDENCY_DATA_SIZE);

			iounmap(s0ix_residencies_addr);
			ret = copy_to_user(argp, &dump_results[0],
					ALL_RESIDENCY_DATA_SIZE);
		}

		break;
	}
	case INTEL_SCU_IPC_READ_RR_FROM_OSNIB:
	{
		u8 reboot_reason;
		ret = intel_scu_ipc_read_osnib_rr(&reboot_reason);
		if (ret < 0)
			return ret;
		ret = copy_to_user(argp, &reboot_reason, 1);
		break;
	}
	case INTEL_SCU_IPC_WRITE_RR_TO_OSNIB:
	{
		u8 data;

		ret = copy_from_user(&data, (u8 *)arg, 1);
		if (ret < 0) {
			pr_err("copy from user failed!!\n");
			return ret;
		}
		ret = intel_scu_ipc_write_osnib_rr(data);
		break;
	}
	case INTEL_SCU_IPC_WRITE_ALARM_FLAG_TO_OSNIB:
	{
		u8 flag, data;
		ret = copy_from_user(&flag, (u8 *)arg, 1);
		if (ret < 0) {
			pr_err("copy from user failed!!\n");
			return ret;
		}

		ret = oshob_info->scu_ipc_read_osnib(
				&data,
				1,
				offsetof(struct scu_ipc_osnib, alarm));

		if (ret < 0)
			return ret;
		if (flag) {
			data = data | 0x1; /* set alarm flag */
			pr_info("scu_ipc_ioctl: set alarm flag\n");
		} else {
			data = data & 0xFE; /* clear alarm flag */
			pr_info("scu_ipc_ioctl: clear alarm flag\n");
		}

		ret = oshob_info->scu_ipc_write_osnib(
				&data,
				1,
				offsetof(struct scu_ipc_osnib, alarm));

		break;
	}
	case INTEL_SCU_IPC_READ_VBATTCRIT:
	{
		u32 value = 0;

		pr_info("cmd = INTEL_SCU_IPC_READ_VBATTCRIT");
		ret = intel_scu_ipc_read_mip((u8 *)&value, 4, 0x318, 1);
		if (ret < 0)
			return ret;
		pr_info("VBATTCRIT VALUE = %x\n", value);
		ret = copy_to_user(argp, &value, 4);
		break;
	}
	case INTEL_SCU_IPC_FW_REVISION_GET:
	case INTEL_SCU_IPC_FW_REVISION_EXT_GET:
	{
		struct scu_ipc_version version;

		if (copy_from_user(&version, argp, sizeof(u32)))
			return -EFAULT;

		if (version.count > 16)
			return -EINVAL;

		ret = rpmsg_send_command(ipcutil_instance, RP_GET_FW_REVISION,
			cmd & 0x1, NULL, (u32 *)version.data, 0, 4);
		if (ret < 0)
			return ret;

		if (copy_to_user(argp + sizeof(u32),
					version.data, version.count))
			ret = -EFAULT;
		break;
	}
	case INTEL_SCU_IPC_OSC_CLK_CNTL:
	{
		struct osc_clk_t osc_clk;

		if (copy_from_user(&osc_clk, argp, sizeof(struct osc_clk_t)))
			return -EFAULT;

		ret = intel_scu_ipc_osc_clk(osc_clk.id, osc_clk.khz);
		if (ret)
			pr_err("%s: failed to set osc clk\n", __func__);

		break;
	}
	case INTEL_SCU_IPC_PMDB_ACCESS:
	{
		ret = do_pmdb_user_buf_access(argp);

		break;
	}
	default:
		if (copy_from_user(&data, argp, sizeof(struct scu_ipc_data)))
			return -EFAULT;
		ret = scu_reg_access(cmd, &data);
		if (ret < 0)
			return ret;
		if (copy_to_user(argp, &data, sizeof(struct scu_ipc_data)))
			return -EFAULT;
		return 0;
	}

	return ret;
}

phys_addr_t intel_scu_ipc_get_oshob_base(void)
{
	if (oshob_info == NULL)
		return 0;

	return oshob_info->oshob_base;
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_get_oshob_base);

int intel_scu_ipc_get_oshob_size(void)
{
	if (oshob_info == NULL)
		return 0;

	return oshob_info->oshob_size;
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_get_oshob_size);

int intel_scu_ipc_read_oshob(u8 *data, int len, int offset)
{
	int ret = 0, i;
	void __iomem *oshob_addr;
	u8 *ptr = data;

	oshob_addr = ioremap_nocache(
				    oshob_info->oshob_base,
				    oshob_info->oshob_size);

	if (!oshob_addr) {
		pr_err("ipc_read_oshob: addr ioremap failed!\n");
		ret = -ENOMEM;
		goto exit;
	}

	for (i = 0; i < len; i = i+1) {
		*ptr = readb(oshob_addr + offset + i);
		pr_debug("addr(remapped)=%p, offset=%2x, value=%2x\n",
			(oshob_addr + i),
			offset + i, *ptr);
		ptr++;
	}

	iounmap(oshob_addr);
exit:
	return ret;
}

EXPORT_SYMBOL_GPL(intel_scu_ipc_read_oshob);

/* This function is used for the default OSNIB. */
int intel_scu_ipc_read_osnib(u8 *data, int len, int offset)
{
	int i, ret = 0;
	phys_addr_t osnibw_ptr;
	u8 *ptr, check = 0;
	u16 struct_offs;
	void __iomem *oshob_addr, *osnibr_addr, *osnibw_addr;

	pr_debug("OSHOB base addr value is %pa\n", &oshob_info->oshob_base);
	oshob_addr = ioremap_nocache(oshob_info->oshob_base,
				     oshob_info->oshob_size);
	if (!oshob_addr) {
		pr_err("ioremap failed!\n");
		ret = -ENOMEM;
		goto exit;
	}

	struct_offs = offsetof(struct scu_ipc_oshob, osnibr) +
			    oshob_info->offs_add;
	osnibr_addr = oshob_addr + struct_offs;

	if (!osnibr_addr) {
		pr_err("Bad osnib address!\n");
		ret = -EFAULT;
		iounmap(oshob_addr);
		goto exit;
	}

	pr_debug("OSNIB read addr (remapped) is %p\n", osnibr_addr);

	/* Make a chksum verification for osnib */
	for (i = 0; i < oshob_info->osnib_size; i++)
		check += readb(osnibr_addr + i);
	if (check) {
		pr_err("WARNING!!! osnib chksum verification faild, reset all osnib data!\n");
		struct_offs = offsetof(struct scu_ipc_oshob, osnibw_ptr) +
				    oshob_info->offs_add;
		osnibw_ptr = readl(oshob_addr + struct_offs);
		osnibw_addr = ioremap_nocache(
					osnibw_ptr, oshob_info->osnib_size);
		if (osnibw_addr) {
			for (i = 0; i < oshob_info->osnib_size; i++)
				writeb(0, osnibw_addr + i);
			rpmsg_send_raw_command(ipcutil_instance,
				RP_WRITE_OSNIB, 0,
				NULL, NULL, 0, 0,
				0xFFFFFFFF, 0);
			iounmap(osnibw_addr);
		}
	}

	ptr = data;
	for (i = 0; i < len; i++) {
		*ptr = readb(osnibr_addr + offset + i);
		pr_debug("addr(remapped)=%p offset=%2x, value=%2x\n",
			(osnibr_addr+offset+i), offset+i, *ptr);
		ptr++;
	}

	iounmap(oshob_addr);
exit:
	return ret;
}

static u32 invalid_checksum;

/* This function is used for the default OSNIB. */
int intel_scu_ipc_write_osnib(u8 *data, int len, int offset)
{
	int i;
	int ret = 0;
	phys_addr_t osnibw_ptr;
	u8 osnib_data[oshob_info->osnib_size];
	u8 check = 0, chksum = 0;
	u16 struct_offs;
	void __iomem *oshob_addr, *osnibw_addr, *osnibr_addr;

	pr_debug("OSHOB base addr value is %pa\n", &oshob_info->oshob_base);

	rpmsg_global_lock();

	oshob_addr = ioremap_nocache(oshob_info->oshob_base,
				     oshob_info->oshob_size);
	if (!oshob_addr) {
		pr_err("ioremap failed!\n");
		ret = -ENOMEM;
		goto exit;
	}

	/*Dump osnib data for generate chksum */
	struct_offs = offsetof(struct scu_ipc_oshob, osnibr) +
			    oshob_info->offs_add;
	osnibr_addr = oshob_addr + struct_offs;

	pr_debug("OSNIB read addr (remapped) in OSHOB at %p\n",
						osnibr_addr);

	for (i = 0; i < oshob_info->osnib_size; i++) {
		osnib_data[i] = readb(osnibr_addr + i);
		check += osnib_data[i];
	}
	memcpy(osnib_data + offset, data, len);

	if (check) {
		pr_err("WARNING!!! OSNIB data chksum verification FAILED!\n");
	} else {
		/* generate chksum */
		for (i = 0; i < oshob_info->osnib_size - 1; i++)
			chksum += osnib_data[i];
		osnib_data[oshob_info->osnib_size - 1] = ~chksum + 1;

		if (invalid_checksum)
			osnib_data[oshob_info->osnib_size - 1] = ~chksum;
	}

	struct_offs = offsetof(struct scu_ipc_oshob, osnibw_ptr) +
			    oshob_info->offs_add;
	osnibw_ptr = readl(oshob_addr + struct_offs);
	if (osnibw_ptr == 0) { /* workaround here for BZ 2914 */
		osnibw_ptr = 0xFFFF3400;
		pr_err("ERR: osnibw ptr from oshob is 0, manually set it here\n");
	}

	pr_debug("POSNIB write address: %pa\n", &osnibw_ptr);

	osnibw_addr = ioremap_nocache(osnibw_ptr, oshob_info->osnib_size);
	if (!osnibw_addr) {
		pr_err("ioremap failed!\n");
		ret = -ENOMEM;
		goto unmap_oshob_addr;
	}

	for (i = 0; i < oshob_info->osnib_size; i++)
		writeb(*(osnib_data + i), (osnibw_addr + i));

	ret = rpmsg_send_raw_command(ipcutil_instance,
			RP_WRITE_OSNIB, 0,
			NULL, NULL, 0, 0,
			0xFFFFFFFF, 0);
	if (ret < 0)
		pr_err("ipc_write_osnib failed!!\n");

	iounmap(osnibw_addr);

unmap_oshob_addr:
	iounmap(oshob_addr);
exit:
	rpmsg_global_unlock();

	return ret;
}

/* This function is used for the extended OSHOB/OSNIB. */
int intel_scu_ipc_read_osnib_extend(u8 *data, int len, int offset)
{
	int i, ret = 0;
	u8 *ptr, check = 0;
	void __iomem *oshob_addr, *osnibr_addr, *osnibw_addr;
	u32 sptr_dw_mask;

	oshob_addr = ioremap_nocache(oshob_info->oshob_base,
				     oshob_info->oshob_size);
	if (!oshob_addr) {
		pr_err("ipc_read_osnib_extend: ioremap failed!\n");
		ret = -ENOMEM;
		goto exit;
	}

	pr_debug("ipc_read_osnib_extend: OSNIB addr=%pa size %d\n",
		 &oshob_info->osnibr_ptr, oshob_info->osnib_size);

	osnibr_addr = ioremap_nocache(oshob_info->osnibr_ptr,
				      oshob_info->osnib_size);

	if (!osnibr_addr) {
		pr_err("ipc_read_osnib_extend: ioremap of osnib failed!\n");
		ret = -ENOMEM;
		goto unmap_oshob_addr;
	}

	/* Make a chksum verification for osnib */
	for (i = 0; i < oshob_info->osnib_size; i++)
		check += readb(osnibr_addr + i);

	if (check) {
		pr_err("ipc_read_osnib_extend: WARNING!!! osnib chksum verification faild, reset all osnib data!\n");
		pr_debug("ipc_read_osnib_extend: remap osnibw ptr addr=%pa size %d\n",
			&oshob_info->osnibw_ptr, oshob_info->osnib_size);

		osnibw_addr = ioremap_nocache(oshob_info->osnibw_ptr,
					      oshob_info->osnib_size);
		if (!osnibw_addr) {
			pr_err("ipc_read_osnib_extend: cannot remap osnib write ptr\n");
			goto unmap_oshob_addr;
		}

		for (i = 0; i < oshob_info->osnib_size; i++)
			writeb(0, osnibw_addr + i);

		/* Send command. The mask to be written identifies which      */
		/* double words of the OSNIB osnib_size bytes will be written.*/
		/* So the mask is coded on 4 bytes.                           */
		sptr_dw_mask = 0xFFFFFFFF;
		rpmsg_send_raw_command(ipcutil_instance,
			RP_WRITE_OSNIB,
			0, NULL, NULL, 0, 0, sptr_dw_mask, 0);
		iounmap(osnibw_addr);
	}

	ptr = data;
	pr_debug("ipc_read_osnib_extend: OSNIB content:\n");
	for (i = 0; i < len; i++) {
		*ptr = readb(osnibr_addr + offset + i);
		pr_debug("addr(remapped)=%p, offset=%2x, value=%2x\n",
			(osnibr_addr+offset+i), offset+i, *ptr);
		ptr++;
	}

	iounmap(osnibr_addr);

unmap_oshob_addr:
	iounmap(oshob_addr);
exit:
	return ret;
}

/* This function is used for the extended OSHOB/OSNIB. */
int intel_scu_ipc_write_osnib_extend(u8 *data, int len, int offset)
{
	int i;
	int ret = 0;
	u8 *posnib_data, *ptr;
	u8 check = 0, chksum = 0;
	void __iomem *oshob_addr, *osnibw_addr, *osnibr_addr;
	u32 sptr_dw_mask;

	rpmsg_global_lock();

	pr_debug("ipc_write_osnib_extend: remap OSHOB addr %pa size %d\n",
		 &oshob_info->oshob_base, oshob_info->oshob_size);

	oshob_addr = ioremap_nocache(oshob_info->oshob_base,
				     oshob_info->oshob_size);
	if (!oshob_addr) {
		pr_err("ipc_write_osnib_extend: ioremap failed!\n");
		ret = -ENOMEM;
		goto exit;
	}

	osnibr_addr = ioremap_nocache(oshob_info->osnibr_ptr,
				      oshob_info->osnib_size);

	if (!osnibr_addr) {
		pr_err("ipc_write_osnib_extend: ioremap of osnib failed!\n");
		ret = -ENOMEM;
		goto unmap_oshob_addr;
	}

	/* Dump osnib data for generate chksum */
	posnib_data = kzalloc(oshob_info->osnib_size, GFP_KERNEL);

	if (posnib_data == NULL) {
		pr_err("ipc_write_osnib_extend: The buffer for getting OSNIB is NULL\n");
		ret = -EFAULT;
		iounmap(osnibr_addr);
		goto unmap_oshob_addr;
	}

	ptr = posnib_data;
	for (i = 0; i < oshob_info->osnib_size; i++) {
		*ptr = readb(osnibr_addr + i);
		check += *ptr;
		ptr++;
	}

	memcpy(posnib_data + offset, data, len);

	if (check) {
		pr_err("ipc_write_osnib_extend: WARNING!!! OSNIB data chksum verification FAILED!\n");
	} else {
		/* generate chksum.  */
		pr_debug("ipc_write_osnib_extend: generating checksum\n");
		for (i = 0; i < oshob_info->osnib_size - 1; i++)
			chksum += *(posnib_data + i);
		/* Fill checksum at the CHECKSUM offset place in OSNIB. */
		*(posnib_data +
		    offsetof(struct scu_ipc_osnib, checksum)) = ~chksum + 1;

		if (invalid_checksum)
			*(posnib_data +
			    offsetof(struct scu_ipc_osnib, checksum)) = ~chksum;
	}

	pr_debug("ipc_write_osnib_extend: osnibw ptr addr=%pa size %d\n",
		 &oshob_info->osnibw_ptr, oshob_info->osnib_size);

	osnibw_addr = ioremap_nocache(oshob_info->osnibw_ptr,
				      oshob_info->osnib_size);
	if (!osnibw_addr) {
		pr_err("scu_ipc_write_osnib_extend: ioremap failed!\n");
		ret = -ENOMEM;
		goto exit_osnib;
	}

	for (i = 0; i < oshob_info->osnib_size; i++)
		writeb(*(posnib_data + i), (osnibw_addr + i));

	/* Send command. The mask to be written identifies which            */
	/* double words of the OSNIB osnib_size bytes will be written.*/
	/* So the mask is coded on 4 bytes.                                 */
	sptr_dw_mask = 0xFFFFFFFF;
	ret = rpmsg_send_raw_command(ipcutil_instance,
			RP_WRITE_OSNIB, 0, NULL, NULL,
			0, 0, sptr_dw_mask, 0);
	if (ret < 0)
		pr_err("scu_ipc_write_osnib_extend: ipc_write_osnib failed!!\n");

	iounmap(osnibw_addr);

exit_osnib:
	iounmap(osnibr_addr);

	kfree(posnib_data);

unmap_oshob_addr:
	iounmap(oshob_addr);
exit:
	rpmsg_global_unlock();

	return ret;
}

/*
 * This writes the reboot reason in the OSNIB (factor and avoid any overlap)
 */
int intel_scu_ipc_write_osnib_rr(u8 rr)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(osnib_target_oses); i++) {
		if (osnib_target_oses[i].id == rr) {
			pr_info("intel_scu_ipc_write_osnib_rr: reboot reason: %s\n",
				osnib_target_oses[i].target_os_name);
			return oshob_info->scu_ipc_write_osnib(
				&rr,
				1,
				offsetof(struct scu_ipc_osnib, target_mode));
		}
	}

	pr_warn("intel_scu_ipc_write_osnib_rr: reboot reason [0x%x] not found\n",
			rr);
	return -1;
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_write_osnib_rr);

/*
 * This reads the reboot reason from the OSNIB (factor)
 */
int intel_scu_ipc_read_osnib_rr(u8 *rr)
{
	pr_debug("intel_scu_ipc_read_osnib_rr: read reboot reason\n");
	return oshob_info->scu_ipc_read_osnib(
			rr,
			1,
			offsetof(struct scu_ipc_osnib, target_mode));
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_read_osnib_rr);


int intel_scu_ipc_read_oshob_extend_param(void __iomem *poshob_addr)
{
	u16 struct_offs;
	int buff_size;

	/* Get defined OSNIB space size. */
	oshob_info->osnib_size = readw(
			    poshob_addr +
			    offsetof(struct scu_ipc_oshob_extend, intel_size));

	if (oshob_info->osnib_size == 0) {
		pr_err("ipc_read_oshob_extend_param: OSNIB size is null!\n");
		return -EFAULT;
	}

	/* Get defined OEM space size. */
	oshob_info->oemnib_size = readw(
			    poshob_addr +
			    offsetof(struct scu_ipc_oshob_extend, oem_size));

	if (oshob_info->oemnib_size == 0) {
		pr_err("ipc_read_oshob_extend_param: OEMNIB size is null!\n");
		return -EFAULT;
	}

	/* Set SCU and IA trace buffers. Size calculated in bytes here. */
	if ((oshob_info->platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(oshob_info->platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE))
		buff_size = OSHOB_SCU_BUF_MRFLD_DW_SIZE*4;
	else
		buff_size = OSHOB_SCU_BUF_BASE_DW_SIZE*4;

	intel_scu_ipc_read_oshob(
		(u8 *)(oshob_info->scu_trace),
		buff_size,
		offsetof(struct scu_ipc_oshob_extend, scutxl));

	struct_offs = offsetof(struct scu_ipc_oshob_extend, iatxl) +
			    oshob_info->offs_add;
	oshob_info->ia_trace = readl(poshob_addr + struct_offs);

	/* Set pointers */
	struct_offs = offsetof(struct scu_ipc_oshob_extend, r_intel_ptr) +
			    oshob_info->offs_add;
	oshob_info->osnibr_ptr = readl(poshob_addr + struct_offs);

	if (!oshob_info->osnibr_ptr) {
		pr_err("ipc_read_oshob_extend_param: R_INTEL_POINTER is NULL!\n");
		return -ENOMEM;
	}

	struct_offs = offsetof(struct scu_ipc_oshob_extend, w_intel_ptr) +
			    oshob_info->offs_add;
	oshob_info->osnibw_ptr = readl(poshob_addr + struct_offs);

	if (oshob_info->osnibw_ptr == 0) {
		/* workaround here for BZ 2914 */
		oshob_info->osnibw_ptr = 0xFFFF3400;
		pr_err(
		    "ipc_read_oshob_extend_param: ERR: osnibw from oshob is 0, manually set it here\n");
	}

	pr_info("(extend oshob) osnib read ptr = %pa\n",
		&oshob_info->osnibr_ptr);
	pr_info("(extend oshob) osnib write ptr = %pa\n",
		&oshob_info->osnibw_ptr);

	struct_offs = offsetof(struct scu_ipc_oshob_extend, r_oem_ptr) +
			    oshob_info->offs_add;
	oshob_info->oemnibr_ptr = readl(poshob_addr + struct_offs);

	if (!oshob_info->oemnibr_ptr) {
		pr_err("ipc_read_oshob_extend_param: R_OEM_POINTER is NULL!\n");
		return -ENOMEM;
	}

	struct_offs = offsetof(struct scu_ipc_oshob_extend, w_oem_ptr) +
			    oshob_info->offs_add;
	oshob_info->oemnibw_ptr = readl(poshob_addr + struct_offs);

	if (!oshob_info->oemnibw_ptr) {
		pr_err("ipc_read_oshob_extend_param: W_OEM_POINTER is NULL!\n");
		return -ENOMEM;
	}

	oshob_info->scu_ipc_write_osnib =
					&intel_scu_ipc_write_osnib_extend;
	oshob_info->scu_ipc_read_osnib =
					&intel_scu_ipc_read_osnib_extend;

	pr_info(
		"Using extended oshob structure size = %d bytes\n",
		oshob_info->oshob_size);
	pr_info(
		"OSNIB Intel size = %d bytes OEMNIB size = %d bytes\n",
		oshob_info->osnib_size, oshob_info->oemnib_size);

	if (oshob_info->platform_type == INTEL_MID_CPU_CHIP_CLOVERVIEW) {
		if ((oshob_info->oshob_majrev >= 1) &&
		    (oshob_info->oshob_minrev >= 1)) {
			/* CLVP and correct version of the oshob. */
			oshob_info->scu_trace_buf =
				readl(poshob_addr +
				      offsetof(struct scu_ipc_oshob_extend,
					       sculogbufferaddr));
			oshob_info->scu_trace_size =
				readl(poshob_addr +
				      offsetof(struct scu_ipc_oshob_extend,
					       sculogbuffersize));
		}
		if ((oshob_info->oshob_majrev >= 1) &&
		    (oshob_info->oshob_minrev >= 3)) {
			/* CLVP and correct version of the oshob. */
			oshob_info->nvram_addr =
				readl(poshob_addr +
				      offsetof(struct scu_ipc_oshob_extend,
					       nvram_addr));
			oshob_info->nvram_size =
				readl(poshob_addr +
				      offsetof(struct scu_ipc_oshob_extend,
					       nvram_size));
		}
	}
	return 0;
}

int intel_scu_ipc_read_oshob_extend_param_v14(void __iomem *poshob_addr)
{
	u16 struct_offs;
	int buff_size;

	/* set intel OSNIB space size. */
	oshob_info->osnib_size = OSNIB_SIZE;

	/* set OEM OSNIB space size. */
	oshob_info->oemnib_size = OSNIB_OEM_RSVD_SIZE;

	/* Set SCU and IA trace buffers. Size calculated in bytes here. */
	if (oshob_info->platform_type == INTEL_MID_CPU_CHIP_TANGIER)
		buff_size = OSHOB_SCU_BUF_MRFLD_DW_SIZE*4;
	else
		buff_size = OSHOB_SCU_BUF_BASE_DW_SIZE*4;

	intel_scu_ipc_read_oshob(
		(u8 *)(oshob_info->scu_trace),
		buff_size,
		offsetof(struct scu_ipc_oshob_extend_v14, scutxl));

	struct_offs = offsetof(struct scu_ipc_oshob_extend_v14, iatxl) +
			    oshob_info->offs_add;
	oshob_info->ia_trace = readl(poshob_addr + struct_offs);

	/* Set pointers */
	struct_offs = offsetof(struct scu_ipc_oshob_extend_v14, osnib_ptr) +
			    oshob_info->offs_add;
	oshob_info->osnibr_ptr = readl(poshob_addr + struct_offs);

	if (!oshob_info->osnibr_ptr) {
		pr_err("ipc_read_oshob_extend_param_v14: R_INTEL_POINTER is NULL!\n");
		return -ENOMEM;
	}

	/* write and read pointer are the same */
	oshob_info->osnibw_ptr = oshob_info->osnibr_ptr;

	pr_info("(latest extend oshob) osnib ptr = %pa\n",
		&oshob_info->osnibr_ptr);

	/* OEM NIB point at offset OSNIB_SIZE */
	oshob_info->oemnibr_ptr = oshob_info->osnibr_ptr + OSNIB_SIZE;

	/* write and read pinter are the same */
	oshob_info->oemnibw_ptr = oshob_info->oemnibr_ptr;

	/* we use tha same function for all extended OSHOB structure */
	oshob_info->scu_ipc_write_osnib =
					&intel_scu_ipc_write_osnib_extend;
	oshob_info->scu_ipc_read_osnib =
					&intel_scu_ipc_read_osnib_extend;

	pr_info(
		"Using latest extended oshob structure size = %d bytes\n",
		oshob_info->oshob_size);
	pr_info(
		"OSNIB Intel size = %d bytes OEMNIB size = %d bytes\n",
		oshob_info->osnib_size, oshob_info->oemnib_size);

	struct_offs = offsetof(struct scu_ipc_oshob_extend_v14,
			    sculogbufferaddr) + oshob_info->offs_add;
	oshob_info->scu_trace_buf = readl(poshob_addr + struct_offs);

	struct_offs = offsetof(struct scu_ipc_oshob_extend_v14,
			    sculogbuffersize) + oshob_info->offs_add;
	oshob_info->scu_trace_size = readl(poshob_addr + struct_offs);

	/* NVRAM after Intel and OEM OSNIB */
	oshob_info->nvram_addr = oshob_info->oemnibr_ptr + OSNIB_OEM_RSVD_SIZE;
	oshob_info->nvram_size = OSNIB_NVRAM_SIZE;

	return 0;
}

int intel_scu_ipc_read_oshob_def_param(void __iomem *poshob_addr)
{
	u16 struct_offs;
	int ret = 0;
	int buff_size;

	oshob_info->oshob_majrev = OSHOB_REV_MAJ_DEFAULT;
	oshob_info->oshob_minrev = OSHOB_REV_MIN_DEFAULT;
	oshob_info->osnib_size = OSNIB_SIZE;
	oshob_info->oemnib_size = 0;

	/* Set OSHOB total size */
	if ((oshob_info->platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(oshob_info->platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE))
		oshob_info->oshob_size = OSHOB_MRFLD_SIZE;
	else
		oshob_info->oshob_size = OSHOB_SIZE;

	/* Set SCU and IA trace buffers. Size calculated in bytes here. */
	if ((oshob_info->platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(oshob_info->platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE))
		buff_size = OSHOB_SCU_BUF_MRFLD_DW_SIZE*4;
	else
		buff_size = OSHOB_SCU_BUF_BASE_DW_SIZE*4;

	ret = intel_scu_ipc_read_oshob(
		(u8 *)(oshob_info->scu_trace),
		buff_size,
		offsetof(struct scu_ipc_oshob, scutxl));

	if (ret != 0) {
		pr_err("Cannot get scutxl data from OSHOB\n");
		return ret;
	}

	struct_offs = offsetof(struct scu_ipc_oshob, iatxl) +
			    oshob_info->offs_add;
	oshob_info->ia_trace = readl(poshob_addr + struct_offs);

	oshob_info->scu_ipc_write_osnib =
					&intel_scu_ipc_write_osnib;
	oshob_info->scu_ipc_read_osnib =
					&intel_scu_ipc_read_osnib;

	struct_offs = offsetof(struct scu_ipc_oshob, osnibr) +
			    oshob_info->offs_add;
	oshob_info->osnibr_ptr = (unsigned long)(poshob_addr + struct_offs);

	pr_info("Using default oshob structure size = %d bytes\n",
		oshob_info->oshob_size);

	pr_debug("Using default oshob structure OSNIB read ptr %pa\n",
		 &oshob_info->osnibr_ptr);

	return ret;
}

int intel_scu_ipc_read_oshob_info(void)
{
	int i, ret = 0;
	u32 oshob_base = 0;
	void __iomem *oshob_addr;
	unsigned char oshob_magic[4];

	/* Notice that SCU still returns address coded in 4 bytes. */
	ret = rpmsg_send_command(ipcutil_instance,
				 RP_GET_HOBADDR, 0, NULL, &oshob_base, 0, 1);

	if (ret < 0) {
		pr_err("ipc_read_oshob cmd failed!!\n");
		goto exit;
	}

	/* At this stage, we still don't know which OSHOB type (default or  */
	/* extended) can be used, and the size of resource to be remapped   */
	/* depends on the type of OSHOB structure to be used.               */
	/* So just remap the minimum size to get the needed bytes of the    */
	/* OSHOB zone.                                                      */
	oshob_addr = ioremap_nocache(oshob_base, OSHOB_EXTEND_DESC_SIZE);

	if (!oshob_addr) {
		pr_err("oshob addr ioremap failed!\n");
		ret = -ENOMEM;
		goto exit;
	}

	pr_info("(oshob) base addr = 0x%x\n", oshob_base);

	/* Store base address. */
	oshob_info->oshob_base = oshob_base;

	oshob_info->platform_type = intel_mid_identify_cpu();

	/*
	 * Buffer is allocated using kmalloc. Memory is not initialized and
	 * these fields are not updated in all the branches.
	 */
	oshob_info->scu_trace_buf = 0;
	oshob_info->scu_trace_size = 0;
	oshob_info->nvram_addr = 0;
	oshob_info->nvram_size = 0;

	if ((oshob_info->platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(oshob_info->platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE)) {
		pr_info("(oshob) identified platform = INTEL_MID_CPU_CHIP_TANGIER|ANNIEDALE\n");

		/* By default we already have 1 dword reserved in the OSHOB */
		/* structures for SCU buffer. For Merrifield, SCU size to   */
		/* consider is OSHOB_SCU_BUF_MRFLD_DW_SIZE dwords. So with  */
		/* Merrifield, when calculating structures offsets, we have */
		/* to add (OSHOB_SCU_BUF_MRFLD_DW_SIZE - 1) dwords, with    */
		/* the offsets calculated in bytes.                         */
		oshob_info->offs_add = (OSHOB_SCU_BUF_MRFLD_DW_SIZE - 1)*4;
	} else
		oshob_info->offs_add = 0;

	pr_debug("(oshob) additional offset = 0x%x\n", oshob_info->offs_add);

	/* Extract magic number that will help identifying the good OSHOB  */
	/* that is going to be used.                                       */
	for (i = 0; i < OSHOB_HEADER_MAGIC_SIZE; i = i+1)
		oshob_magic[i] = readb(oshob_addr + i);

	pr_debug("(oshob) OSHOB magic = %x %x %x %x\n",
		oshob_magic[0], oshob_magic[1], oshob_magic[2], oshob_magic[3]);

	if (strncmp(oshob_magic, OSHOB_MAGIC_NUMBER,
		    OSHOB_HEADER_MAGIC_SIZE) == 0) {
		/* Get OSHOB version and size which are commoon to all */
		/* extended OSHOB structure. */
		oshob_info->oshob_majrev = readb(oshob_addr +
			offsetof(struct scu_ipc_oshob_extend, rev_major));
		oshob_info->oshob_minrev = readb(oshob_addr +
			offsetof(struct scu_ipc_oshob_extend, rev_minor));
		oshob_info->oshob_size = readw(oshob_addr +
			offsetof(struct scu_ipc_oshob_extend, oshob_size));

		pr_info("(oshob) oshob version = %x.%x\n",
			oshob_info->oshob_majrev, oshob_info->oshob_minrev);

		if ((oshob_info->oshob_majrev >= 1) &&
		    (oshob_info->oshob_minrev >= 4)) {
			if (intel_scu_ipc_read_oshob_extend_param_v14(
					oshob_addr) != 0) {
				ret = -EFAULT;
				goto unmap_oshob;
			}
		} else {
			if (intel_scu_ipc_read_oshob_extend_param(
					oshob_addr) != 0) {
				ret = -EFAULT;
				goto unmap_oshob;
			}
		}

		if ((oshob_info->platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(oshob_info->platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE)) {
			pr_info("(extend oshob) SCU buffer size is %d bytes\n",
				OSHOB_SCU_BUF_MRFLD_DW_SIZE*4);
		} else {
			pr_debug("(extend oshob) SCU buffer size is %d bytes\n",
				OSHOB_SCU_BUF_BASE_DW_SIZE*4);
		}
	} else {
		ret = intel_scu_ipc_read_oshob_def_param(oshob_addr);

		if ((oshob_info->platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(oshob_info->platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE)) {
			pr_info("(default oshob) SCU buffer size is %d bytes\n",
				OSHOB_SCU_BUF_MRFLD_DW_SIZE*4);
		} else {
			pr_debug("(default oshob) SCU buffer size is %d bytes\n",
				OSHOB_SCU_BUF_BASE_DW_SIZE*4);
		}
	}

unmap_oshob:
	iounmap(oshob_addr);

exit:
	return ret;
}

/*
 * This writes the OEMNIB buffer in the internal RAM of the SCU.
 */
int intel_scu_ipc_write_oemnib(u8 *oemnib, int len, int offset)
{
	int i;
	int ret = 0;
	void __iomem *oshob_addr, *oemnibw_addr;
	u32 sptr_dw_mask;

	if (oemnib == NULL) {
		pr_err("ipc_write_oemnib: passed buffer for writting OEMNIB is NULL\n");
		return -EINVAL;
	}

	rpmsg_global_lock();

	pr_debug("ipc_write_oemnib: OSHOB addr %pa size %d\n",
		&oshob_info->oshob_base, oshob_info->oshob_size);

	oshob_addr = ioremap_nocache(oshob_info->oshob_base,
				     oshob_info->oshob_size);
	if (!oshob_addr) {
		pr_err("ipc_write_oemnib: ioremap failed!\n");
		ret = -ENOMEM;
		goto exit;
	}

	if ((len == 0) || (len > oshob_info->oemnib_size)) {
		pr_err(
			"ipc_write_oemnib: bad OEMNIB data length (%d) to write (max=%d bytes)\n",
			    len, oshob_info->oemnib_size);
		ret = -EINVAL;
		goto unmap_oshob_addr;
	}

	/* offset shall start at 0 from the OEMNIB base address and shall */
	/* not exceed the OEMNIB allowed size.                            */
	if ((offset < 0) || (offset >= oshob_info->oemnib_size) ||
	    (len + offset > oshob_info->oemnib_size)) {
		pr_err(
			"ipc_write_oemnib: Bad OEMNIB data offset/len for writing (offset=%d , len=%d)\n",
			offset, len);
		ret = -EINVAL;
		goto unmap_oshob_addr;
	}

	pr_debug("ipc_write_oemnib: POEMNIB oemnibw ptr %pa size %d\n",
		&oshob_info->oemnibw_ptr, oshob_info->oemnib_size);

	oemnibw_addr = ioremap_nocache(oshob_info->oemnibw_ptr,
				       oshob_info->oemnib_size);
	if (!oemnibw_addr) {
		pr_err("ipc_write_oemnib: ioremap failed!\n");
		ret = -ENOMEM;
		goto unmap_oshob_addr;
	}

	for (i = 0; i < len; i++)
		writeb(*(oemnib + i), (oemnibw_addr + offset + i));

	/* Send command. The mask to be written identifies which double */
	/* words of the OSNIB oemnib_size bytes will be written.        */
	/* So the mask is coded on 4 bytes.                             */
	sptr_dw_mask = 0xFFFFFFFF;
	if ((oshob_info->oshob_majrev >= 1) &&
	    (oshob_info->oshob_minrev >= 4)) {
		sptr_dw_mask = 0xFFFFFFFF;
		/* OEM NIB lies on region 1, 2, and 3 */
		ret = rpmsg_send_raw_command(ipcutil_instance,
			RP_WRITE_OSNIB, 0, NULL, NULL,
			0, 0, sptr_dw_mask, 1);
		if (ret < 0) {
			pr_err("ipc_write_oemnib: ipc_write_osnib failed!!\n");
			goto unmap_oemnibw_addr;
		}
		ret = rpmsg_send_raw_command(ipcutil_instance,
			RP_WRITE_OSNIB, 0, NULL, NULL,
			0, 0, sptr_dw_mask, 2);
		if (ret < 0) {
			pr_err("ipc_write_oemnib: ipc_write_osnib failed!!\n");
			goto unmap_oemnibw_addr;
		}
		ret = rpmsg_send_raw_command(ipcutil_instance,
			RP_WRITE_OSNIB, 0, NULL, NULL,
			0, 0, sptr_dw_mask, 3);
		if (ret < 0) {
			pr_err("ipc_write_oemnib: ipc_write_osnib failed!!\n");
			goto unmap_oemnibw_addr;
		}
	} else {
		ret = rpmsg_send_raw_command(ipcutil_instance,
			RP_WRITE_OEMNIB, 0, NULL, NULL,
			0, 0, sptr_dw_mask, 0);
		if (ret < 0) {
			pr_err("ipc_write_oemnib: ipc_write_osnib failed!!\n");
			goto unmap_oemnibw_addr;
		}
	}

unmap_oemnibw_addr:
	iounmap(oemnibw_addr);

unmap_oshob_addr:
	iounmap(oshob_addr);
exit:
	rpmsg_global_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_write_oemnib);

/*
 * This reads the OEMNIB from the internal RAM of the SCU.
 */
static int intel_scu_ipc_read_oemnib(u8 *oemnib, int len, int offset)
{
	int i, ret = 0;
	u8 *ptr;
	void __iomem *oshob_addr, *oemnibr_addr;

	if (oemnib == NULL) {
		pr_err("ipc_read_oemnib: passed buffer for reading OEMNIB is NULL\n");
		return -EINVAL;
	}

	pr_debug("ipc_read_oemnib: OSHOB base addr %pa size %d\n",
		&oshob_info->oshob_base, oshob_info->oshob_size);

	oshob_addr = ioremap_nocache(oshob_info->oshob_base,
				     oshob_info->oshob_size);
	if (!oshob_addr) {
		pr_err("ipc_read_oemnib: ioremap failed!\n");
		ret = -ENOMEM;
		goto exit;
	}

	if ((len == 0) || (len > oshob_info->oemnib_size)) {
		pr_err("ipc_read_oemnib: Bad OEMNIB data length (%d) to be read (max=%d bytes)\n",
			    len, oshob_info->oemnib_size);
		ret = -EINVAL;
		goto unmap_oshob_addr;
	}

	/* offset shall start at 0 from the OEMNIB base address and shall */
	/* not exceed the OEMNIB allowed size.                            */
	if ((offset < 0) || (offset >= oshob_info->oemnib_size) ||
	    (len + offset > oshob_info->oemnib_size)) {
		pr_err(
		"ipc_read_oemnib: Bad OEMNIB data offset/len to read (offset=%d ,len=%d)\n",
		offset, len);
		ret = -EINVAL;
		goto unmap_oshob_addr;
	}

	pr_debug("ipc_read_oemnib: POEMNIB oemnibr ptr %pa size %d\n",
		 &oshob_info->oemnibr_ptr, oshob_info->oemnib_size);

	oemnibr_addr = ioremap_nocache(oshob_info->oemnibr_ptr,
				       oshob_info->oemnib_size);

	if (!oemnibr_addr) {
		pr_err("ipc_read_oemnib: ioremap of oemnib failed!\n");
		ret = -ENOMEM;
		goto unmap_oshob_addr;
	}

	ptr = oemnib;
	pr_info("ipc_read_oemnib: OEMNIB content:\n");
	for (i = 0; i < len; i++) {
		*ptr = readb(oemnibr_addr + offset + i);
		pr_debug("addr(remapped)=%p, offset=%2x, value=%2x\n",
			(oemnibr_addr+offset+i), offset+i, *ptr);
		ptr++;
	}

	iounmap(oemnibr_addr);

unmap_oshob_addr:
	iounmap(oshob_addr);
exit:
	return ret;
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_read_oemnib);

#ifdef DUMP_OSNIB
/*
 * This reads the PMIT from the OSHOB (pointer to interrupt tree)
 */
static int intel_scu_ipc_read_oshob_it_tree(u32 *ptr)
{
	u16 struct_offs;

	pr_debug("intel_scu_ipc_read_oshob_it_tree: read IT tree\n");

	if ((oshob_info->oshob_majrev == OSHOB_REV_MAJ_DEFAULT) &&
	    (oshob_info->oshob_minrev == OSHOB_REV_MIN_DEFAULT)) {
		struct_offs = offsetof(struct scu_ipc_oshob, pmit) +
				    oshob_info->offs_add;
	} else if ((oshob_info->oshob_majrev >= 1) &&
		   (oshob_info->oshob_minrev >= 4)) {
		struct_offs = offsetof(struct scu_ipc_oshob_extend_v14, pmit) +
				    oshob_info->offs_add;
	} else {
		struct_offs = offsetof(struct scu_ipc_oshob_extend, pmit) +
				    oshob_info->offs_add;
	}
	return intel_scu_ipc_read_oshob(
			(u8 *) ptr,
			4,
			struct_offs);
}
#endif

/*
 * This reads the RESETIRQ1 or RESETSRC0 from the OSNIB
 */
#ifdef DUMP_OSNIB
static int intel_scu_ipc_read_osnib_reset_ev1(u8 *rev1)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(chip_reset_events); i++) {
		if (chip_reset_events[i].id == oshob_info->platform_type) {
			pr_debug(
				"intel_scu_ipc_read_osnib_rst_ev1: read %s\n",
				chip_reset_events[i].reset_ev1_name);

			return oshob_info->scu_ipc_read_osnib(
				    rev1,
				    1,
				    offsetof(struct scu_ipc_osnib, reset_ev1));
		}
	}

	pr_err("intel_scu_ipc_read_osnib_reset_ev1: param not found\n");
	return -EFAULT;
}
#endif

/*
 * This reads the RESETIRQ2 or RESETSRC1 from the OSNIB
 */
#ifdef DUMP_OSNIB
static int intel_scu_ipc_read_osnib_reset_ev2(u8 *rev2)
{
	int i;

	for (i = 0 ; i < ARRAY_SIZE(chip_reset_events); i++) {
		if (chip_reset_events[i].id == oshob_info->platform_type) {
			pr_debug(
				"intel_scu_ipc_read_osnib_rst_ev2: read %s\n",
				chip_reset_events[i].reset_ev2_name);

			return oshob_info->scu_ipc_read_osnib(
				rev2,
				1,
				offsetof(struct scu_ipc_osnib, reset_ev2));
		}
	}

	pr_err("intel_scu_ipc_read_osnib_reset_ev2: param not found\n");
	return -EFAULT;
}
#endif

/*
 * This reads the WD from the OSNIB
 */
int intel_scu_ipc_read_osnib_wd(u8 *wd)
{
	pr_debug("intel_scu_ipc_read_osnib_wd: read WATCHDOG\n");

	return oshob_info->scu_ipc_read_osnib(
			wd,
			1,
			offsetof(struct scu_ipc_osnib, wd_count));
}

/*
 * This writes the WD in the OSNIB
 */
int intel_scu_ipc_write_osnib_wd(u8 *wd)
{
	pr_info("intel_scu_ipc_write_osnib_wd: write WATCHDOG %x\n", *wd);

	return oshob_info->scu_ipc_write_osnib(
			wd,
			1,
			offsetof(struct scu_ipc_osnib, wd_count));
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_write_osnib_wd);

/*
 * Get SCU trace buffer physical address if available
 */
phys_addr_t intel_scu_ipc_get_scu_trace_buffer(void)
{
	if (oshob_info == NULL)
		return 0;
	return oshob_info->scu_trace_buf;
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_get_scu_trace_buffer);

/*
 * Get SCU trace buffer size
 */
u32 intel_scu_ipc_get_scu_trace_buffer_size(void)
{
	if (oshob_info == NULL)
		return 0;
	return oshob_info->scu_trace_size;
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_get_scu_trace_buffer_size);

/*
 * Get nvram size
 */
u32 intel_scu_ipc_get_nvram_size(void)
{
	if (oshob_info == NULL)
		return 0;
	return oshob_info->nvram_size;
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_get_nvram_size);

/*
 * Get nvram addr
 */
phys_addr_t intel_scu_ipc_get_nvram_addr(void)
{
	if (oshob_info == NULL)
		return 0;
	return oshob_info->nvram_addr;
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_get_nvram_addr);

/*
 * Get SCU fabric error buffer1 offset
 */
u32 intel_scu_ipc_get_fabricerror_buf1_offset(void)
{
	if (oshob_info == NULL)
		return 0;

	if (oshob_info->platform_type == INTEL_MID_CPU_CHIP_CLOVERVIEW)
		return offsetof(struct scu_ipc_oshob_extend, fabricerrlog1);
	else if ((oshob_info->platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(oshob_info->platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE))
		if ((oshob_info->oshob_majrev >= 1) &&
		    (oshob_info->oshob_minrev >= 4)) {
			return offsetof(struct scu_ipc_oshob_extend_v14,
					fabricerrlog) + oshob_info->offs_add;
		} else {
			return offsetof(struct scu_ipc_oshob,
					fab_err_log) + oshob_info->offs_add;
		}
	else {
		pr_err("scu_ipc_get_fabricerror_buf_offset: platform not recognized!\n");
		return 0;
	}
}

/*
 * Get SCU fabric error buffer2 offset
 */
u32 intel_scu_ipc_get_fabricerror_buf2_offset(void)
{
	if (oshob_info == NULL)
		return 0;

	if (oshob_info->platform_type == INTEL_MID_CPU_CHIP_CLOVERVIEW)
		return offsetof(struct scu_ipc_oshob_extend, fabricerrlog2);
	else {
		pr_warn("scu_ipc_get_fabricerror_buf2_offset: not supported for this platform!\n");
		return 0;
	}
}


/*
 * This reads the ALARM from the OSNIB
 */
#ifdef DUMP_OSNIB
static int intel_scu_ipc_read_osnib_alarm(u8 *alarm)
{
	pr_debug("intel_scu_ipc_read_osnib_alarm: read ALARM\n");

	return oshob_info->scu_ipc_read_osnib(
			alarm,
			1,
			offsetof(struct scu_ipc_osnib, alarm));
}
#endif

/*
 * This reads the WAKESRC from the OSNIB
 */
#ifdef DUMP_OSNIB
static int intel_scu_ipc_read_osnib_wakesrc(u8 *wksrc)
{
	pr_debug("intel_scu_ipc_read_osnib_wakesrc: read WAKESRC\n");

	return oshob_info->scu_ipc_read_osnib(
			wksrc,
			1,
			offsetof(struct scu_ipc_osnib, wakesrc));
}
#endif


#define OEMNIB_BUF_DESC_LEN	4096

#ifdef CONFIG_DEBUG_FS
static int intel_scu_ipc_oshob_stat(struct seq_file *m, void *unused)
{
	void __iomem *osnib;
	int i, count;
	int ret = 0;

	u32 value;
	if ((oshob_info->oshob_majrev == OSHOB_REV_MAJ_DEFAULT) &&
	     (oshob_info->oshob_minrev == OSHOB_REV_MIN_DEFAULT)) {
		seq_printf(m, "DEFAULT OSHOB\n");
		seq_printf(m, "OSHOB size : %d\n", oshob_info->oshob_size);
		if ((oshob_info->platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(oshob_info->platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE)) {
			seq_printf(m, "SCU trace : ");

			for (i = 0; i < OSHOB_SCU_BUF_MRFLD_DW_SIZE; i++)
				seq_printf(m, "%x ", oshob_info->scu_trace[i]);

			seq_printf(m, "\n");
		} else
			seq_printf(m, "SCU trace : %x\n",
					oshob_info->scu_trace[0]);

		seq_printf(m, "IA trace  : %x\n", oshob_info->ia_trace);
	} else {
		seq_printf(m, "EXTENDED OSHOB v%d.%d\n",
						oshob_info->oshob_majrev,
						oshob_info->oshob_minrev);
		seq_printf(m, "OSHOB size : %d\n\n", oshob_info->oshob_size);
		if ((oshob_info->platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(oshob_info->platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE)) {
			seq_printf(m, "SCU trace : ");

			for (i = 0; i < OSHOB_SCU_BUF_MRFLD_DW_SIZE; i++)
				seq_printf(m, "%x ", oshob_info->scu_trace[i]);

			seq_printf(m, "\n");
		} else
			seq_printf(m, "SCU trace : %x\n",
					oshob_info->scu_trace[0]);

		seq_printf(m, "IA trace  : %x\n\n", oshob_info->ia_trace);

		seq_printf(m, "OSNIB size : %d\n", oshob_info->osnib_size);
		seq_printf(m, "OSNIB  read address  : %pa\n",
			   &oshob_info->osnibr_ptr);
		seq_printf(m, "OSNIB  write address : %pa\n",
			   &oshob_info->osnibw_ptr);
		/* Dump OSNIB */
		osnib = ioremap_nocache(oshob_info->osnibr_ptr,
						oshob_info->osnib_size);
		if (!osnib) {
			pr_err("Cannot remap OSNIB\n");
			ret = -ENOMEM;
			return ret;
		}

		i = 0;
		count = 0; /* used for fancy presentation */
		while (i < oshob_info->osnib_size) {
			if (count%4 == 0) {
				phys_addr_t cur = oshob_info->osnibr_ptr+i;
				seq_printf(m, "\nOSNIB[%pa] ", &cur);
			}

			value = readl(osnib+i);
			seq_printf(m, "%08x ", value);
			i += 4;
			count++;
		}
		seq_printf(m, "\n\n");
		iounmap(osnib);

		seq_printf(m, "OEMNIB size : %d\n", oshob_info->oemnib_size);
		seq_printf(m, "OEMNIB read address  : %pa\n",
			   &oshob_info->oemnibr_ptr);
		seq_printf(m, "OEMNIB write address : %pa\n",
			   &oshob_info->oemnibw_ptr);
		seq_printf(m, "\n\n");
	}
	return 0;
}

static int intel_scu_ipc_oemnib_stat(struct seq_file *m, void *unused)
{
	void __iomem *oemnib;
	int i, count;
	u32 value;

	/* Dump OEMNIB */
	oemnib = ioremap_nocache(oshob_info->oemnibr_ptr,
				oshob_info->oemnib_size);

	if (!oemnib) {
		pr_err("Cannot remap OEMNIB\n");
		return -ENOMEM;
	}

	i = 0;
	count = 0; /* used for fancy presentation */
	while (i < oshob_info->oemnib_size) {
		if (count%4 == 0) {
			phys_addr_t cur = oshob_info->oemnibr_ptr+i;
			seq_printf(m, "\nOEMNIB[%pa] ", &cur);
		}

		value = readl(oemnib+i);
		seq_printf(m, "%08x ", value);
		i += 4;
		count++;
	}
	seq_printf(m, "\n\n");
	iounmap(oemnib);

	return 0;
}

static int intel_scu_ipc_oshob_open(struct inode *inode, struct file *file)
{
	return single_open(file, intel_scu_ipc_oshob_stat, NULL);
}

static int intel_scu_ipc_oemnib_open(struct inode *inode, struct file *file)
{
	return single_open(file, intel_scu_ipc_oemnib_stat, NULL);
}


/*
*	debugfs interface: the "oemnib_write" stores the OEMNIB part of OSNIB,
*       starting at offset ppos.
*/
static ssize_t intel_scu_ipc_oemnib_write(struct file *file,
					  const char __user *buf,
					    size_t count, loff_t *ppos)
{
	int ret, i;
	u8 *posnib_data, *ptr;
	char *ptrchar, *temp;

	if ((oshob_info->oshob_majrev == OSHOB_REV_MAJ_DEFAULT) &&
	    (oshob_info->oshob_minrev == OSHOB_REV_MIN_DEFAULT)) {
		/* OEMNIB only usable with extended OSHOB structure. */
		pr_err(
		"Write OEMNIB: OEMNIB only usable with extended OSHOB structure.\n");
		return -EFAULT;
	}

	pr_info("Write OEMNIB: number bytes = %zd\n", count);

	/* Note: when the string is passed through debugfs interface, the  */
	/* real count value includes the end of line \n. So we must take   */
	/* care to consider count - 1 as the real number of OEM bytes.     */

	if (buf == NULL) {
		pr_err("Write OEMNIB: The passed OEMNIB buffer is NULL\n");
		return -EINVAL;
	}

	if (count == 0) {
		pr_err("Write OEMNIB: The OEMNIB data length to write is NULL\n");
		return -EINVAL;
	}

	posnib_data = kzalloc(count - 1, GFP_KERNEL);

	if (posnib_data == NULL) {
		pr_err("Write OEMNIB: Cannot allocate buffer for writting OEMNIB\n");
		return -ENOMEM;
	}

	memset(posnib_data, 0, count - 1);

	temp = kzalloc(count - 1, GFP_KERNEL);

	if (temp == NULL) {
		pr_err(
		"Write OEMNIB: Cannot allocate temp buffer for writting OEMNIB\n");
		return -ENOMEM;
	}

	memset(temp, 0, count - 1);

	if (copy_from_user(temp, buf, count - 1)) {
		pr_err(
		"Write OEMNIB: Cannot transfer from user buf to OEMNIB buf\n");
		kfree(posnib_data);
		return -EFAULT;
	}

	ptrchar = temp;
	ptr = posnib_data;

	for (i = 0; i <= count - 1; i++) {
		if (*ptrchar >= '0' && *ptrchar <= '9')
			*ptr = *ptrchar - '0';
		if (*ptrchar >= 'A' && *ptrchar <= 'F')
			*ptr = *ptrchar - 'A' + 10;
		if (*ptrchar >= 'a' && *ptrchar <= 'f')
			*ptr = *ptrchar - 'a' + 10;

		ptrchar++;
		ptr++;
	}

	ret = intel_scu_ipc_write_oemnib(posnib_data, count - 1, *ppos);

	if (ret < 0) {
		pr_err("Write OEMNIB: ipc write of OEMNIB failed!!\n");
		kfree(posnib_data);
		return ret;
	}

	kfree(posnib_data);
	kfree(temp);

	pr_info("Write OEMNIB: OEMNIB updated: count=%zd bytes\n", count);

	return count;
}

/* Attach the debugfs operations methods */
static const struct file_operations scu_ipc_oemnib_fops = {
	.owner = THIS_MODULE,
	.open = intel_scu_ipc_oemnib_open,
	.read = seq_read,
	.write = intel_scu_ipc_oemnib_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations scu_ipc_oshob_fops = {
	.owner = THIS_MODULE,
	.open = intel_scu_ipc_oshob_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dentry *scu_ipc_oemnib_dir;
static struct dentry *scu_ipc_oemnib_file;
static struct dentry *scu_ipc_oshob_file;

/*
*	debugfs interface: init interface.
*/
static int intel_mid_scu_ipc_oemnib_debugfs_init(void)
{
	/* Create debugfs directory /sys/kernel/debug/intel_scu_oshob */
	scu_ipc_oemnib_dir = debugfs_create_dir("intel_scu_oshob", NULL);

	if (!scu_ipc_oemnib_dir) {
		pr_err("cannot create OSHOB debugfs directory\n");
		return -1;
	}

	/* Add operations /sys/kernel/debug/intel_scu_oshob to control */
	/* the OEM.                                                     */
	scu_ipc_oemnib_file = debugfs_create_file("oemnib_debug",
				S_IFREG | S_IRUSR | S_IWUSR,
				scu_ipc_oemnib_dir,
				NULL, &scu_ipc_oemnib_fops);

	if (!scu_ipc_oemnib_file) {
		pr_err("cannot create OEMNIB debugfs file\n");
		debugfs_remove(scu_ipc_oemnib_dir);
		return -1;
	}

	/* Add operations /sys/kernel/debug/intel_scu_oshob to debug OSHOB */
	/* content.                                                         */
	scu_ipc_oshob_file = debugfs_create_file("oshob_dump",
				S_IFREG | S_IRUSR | S_IWUSR,
				scu_ipc_oemnib_dir, NULL, &scu_ipc_oshob_fops);

	if (!scu_ipc_oshob_file) {
		pr_err("cannot create OSHOB debugfs file\n");
		debugfs_remove_recursive(scu_ipc_oemnib_dir);
		return -1;
	}

	return 0;
}

/*
*	debugfs interface: exit interface.
*/
static void intel_mid_scu_ipc_oemnib_debugfs_exit(void)
{
	debugfs_remove_recursive(scu_ipc_oemnib_dir);
}

#define IPC_CMD_RXTX_BUF_SIZE 16
#define IPC_CMD_INPUT_ENTRY_SIZE 16

struct scu_ipc_cmd {
	u32 sptr;
	u32 dptr;
	u8 cmd;
	u8 cmdid;
	u8 wbuf[IPC_CMD_RXTX_BUF_SIZE];
	u32 rbuf[IPC_CMD_RXTX_BUF_SIZE / sizeof(u32)];
	u8 inlen;
	u8 outlen;
};

static struct scu_ipc_cmd ipc_cmd;

static ssize_t intel_scu_ipc_trigger_write(struct file *file,
					  const char __user *buf,
					    size_t count, loff_t *ppos)
{
	int ret;

	if (ipc_cmd.inlen > IPC_CMD_RXTX_BUF_SIZE ||
	    ipc_cmd.outlen > IPC_CMD_RXTX_BUF_SIZE / sizeof(u32)) {
		pr_err("Given RX/TX length is too big");
		return -EFAULT;
	}

	ret = rpmsg_send_generic_raw_command(ipc_cmd.cmd, ipc_cmd.cmdid,
					     ipc_cmd.wbuf, ipc_cmd.inlen,
					     ipc_cmd.rbuf, ipc_cmd.outlen,
					     ipc_cmd.dptr, ipc_cmd.sptr);
	if (ret) {
		pr_err("Failed to send ipc command");
		return ret;
	}

	return count;
}

static int intel_scu_ipc_rwbuf_show(struct seq_file *m, void *unused)
{
	int i, ret = 0;
	u8 *buf = (u8 *)m->private;

	for (i = 0; i < IPC_CMD_RXTX_BUF_SIZE; i++) {
		ret = seq_printf(m, "%02d:0x%02x\n", i, buf[i]);
		if (ret) {
			pr_err("Failed to perform sequential print");
			return ret;
		}
	}

	return ret;
}

static int intel_scu_ipc_rbuf_open(struct inode *inode, struct file *file)
{
	return single_open(file, intel_scu_ipc_rwbuf_show, &ipc_cmd.rbuf);
}

static int intel_scu_ipc_wbuf_open(struct inode *inode, struct file *file)
{
	return single_open(file, intel_scu_ipc_rwbuf_show, &ipc_cmd.wbuf);
}

static ssize_t intel_scu_ipc_wbuf_write(struct file *file,
					  const char __user *buf,
					    size_t count, loff_t *ppos)
{
	int ret;
	unsigned long idx, val;
	char tmp[IPC_CMD_INPUT_ENTRY_SIZE] = {0}; /* "01:0xff" */

	if (!count || count > sizeof(tmp))
		return -EFAULT;

	ret = copy_from_user(&tmp, buf, count);
	if (ret) {
		pr_err("Failed to copy from user space");
		return ret;
	}

	tmp[2] = 0; /* "01\0" */
	ret = kstrtoul(tmp, 10, &idx);
	if (ret) {
		pr_err("Given index is invalid");
		return -EFAULT;
	}
	if (idx + 1 > IPC_CMD_RXTX_BUF_SIZE || idx < 0) {
		pr_err("Given index is out of range. Should be 0...15");
		return -EFAULT;
	}

	tmp[7] = 0; /* "01\00xff\0" */
	ret = kstrtoul(&tmp[3], 16, &val);
	if (ret)
		return -EFAULT;
	if (val > 0xff || val < 0x00)
		return -EFAULT;

	ipc_cmd.wbuf[idx] = val;

	return count;
}

static const struct file_operations scu_ipc_trigger_fops = {
	.owner = THIS_MODULE,
	.write = intel_scu_ipc_trigger_write,
};

static const struct file_operations scu_ipc_rbuf_fops = {
	.owner = THIS_MODULE,
	.open = intel_scu_ipc_rbuf_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations scu_ipc_wbuf_fops = {
	.owner = THIS_MODULE,
	.open = intel_scu_ipc_wbuf_open,
	.read = seq_read,
	.write = intel_scu_ipc_wbuf_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dentry *scu_ipc_cmd_dir, *scu_ipc_cmd_file,
	*scu_ipc_cmdid_file, *scu_ipc_trigger_file, *scu_ipc_rbuf_file,
	*scu_ipc_wbuf_file, *scu_ipc_sptr_file, *scu_ipc_dptr_file,
	*scu_ipc_inlen_file, *scu_ipc_outlen_file;

static int intel_mid_scu_ipc_cmd_debugfs_init(void)
{
	scu_ipc_cmd_dir = debugfs_create_dir("intel_scu_ipc_cmd", NULL);
	if (!scu_ipc_cmd_dir) {
		pr_err("cannot create ipc cmd debugfs directory\n");
		return -ENOMEM;
	}

	scu_ipc_cmd_file = debugfs_create_x8("ipc_cmd", S_IWUSR | S_IRUSR,
						scu_ipc_cmd_dir, &ipc_cmd.cmd);
	if (!scu_ipc_cmd_file) {
		pr_err("cannot create ipc cmd debugfs file\n");
		goto err;
	}

	scu_ipc_cmdid_file = debugfs_create_x8("ipc_cmdid", S_IWUSR | S_IRUSR,
						scu_ipc_cmd_dir,
					       &ipc_cmd.cmdid);
	if (!scu_ipc_cmdid_file) {
		pr_err("cannot create ipc cmdid debugfs file\n");
		goto err;
	}

	scu_ipc_trigger_file = debugfs_create_file("ipc_trigger", S_IWUSR,
						   scu_ipc_cmd_dir, NULL,
						   &scu_ipc_trigger_fops);
	if (!scu_ipc_trigger_file) {
		pr_err("cannot create ipc trigger debugfs file\n");
		goto err;
	}

	scu_ipc_wbuf_file = debugfs_create_file("ipc_wbuf", S_IWUSR | S_IRUSR,
						scu_ipc_cmd_dir, NULL,
						&scu_ipc_wbuf_fops);
	if (!scu_ipc_wbuf_file) {
		pr_err("cannot create ipc wbuf debugfs file\n");
		goto err;
	}

	scu_ipc_rbuf_file = debugfs_create_file("ipc_rbuf", S_IWUSR | S_IRUSR,
						scu_ipc_cmd_dir, NULL,
						&scu_ipc_rbuf_fops);
	if (!scu_ipc_rbuf_file) {
		pr_err("cannot create ipc rbuf debugfs file\n");
		goto err;
	}

	scu_ipc_sptr_file = debugfs_create_x32("ipc_sptr", S_IWUSR | S_IRUSR,
					       scu_ipc_cmd_dir, &ipc_cmd.sptr);
	if (!scu_ipc_sptr_file) {
		pr_err("cannot create ipc sptr debugfs file\n");
		goto err;
	}

	scu_ipc_dptr_file = debugfs_create_x32("ipc_dptr", S_IWUSR | S_IRUSR,
					       scu_ipc_cmd_dir, &ipc_cmd.dptr);
	if (!scu_ipc_dptr_file) {
		pr_err("cannot create ipc dptr debugfs file\n");
		goto err;
	}

	scu_ipc_inlen_file = debugfs_create_u8("ipc_inlen", S_IWUSR | S_IRUSR,
					     scu_ipc_cmd_dir, &ipc_cmd.inlen);
	if (!scu_ipc_inlen_file) {
		pr_err("cannot create ipc inlen debugfs file\n");
		goto err;
	}

	scu_ipc_outlen_file = debugfs_create_u8("ipc_outlen", S_IWUSR | S_IRUSR,
					     scu_ipc_cmd_dir, &ipc_cmd.outlen);
	if (!scu_ipc_outlen_file) {
		pr_err("cannot create ipc outlen debugfs file\n");
		goto err;
	}

	return 0;

err:
	debugfs_remove_recursive(scu_ipc_cmd_dir);
	return -ENOMEM;
}

static void intel_mid_scu_ipc_cmd_debugfs_exit(void)
{
	debugfs_remove_recursive(scu_ipc_cmd_dir);
}

#ifdef DUMP_OSNIB

static ssize_t intel_scu_ipc_osnib_read_reset_event(
	struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	loff_t pos = *ppos;
	u8 rev[1] = {0};
	int ret, i;

	if (pos > 0)
		return 0;

	for (i = 0; i < ARRAY_SIZE(chip_reset_events); i++) {
		if (chip_reset_events[i].id == oshob_info->platform_type) {
			if (strcmp(
				file->f_path.dentry->d_name.name,
				chip_reset_events[i].reset_ev1_name) == 0)
				ret = intel_scu_ipc_read_osnib_reset_ev1(rev);
			else
				ret = intel_scu_ipc_read_osnib_reset_ev2(rev);

			if (ret != 0) {
				pr_err("%s: cannot read %s, ret=%d",
					__func__,
					file->f_path.dentry->d_name.name,
					ret);
				return ret;
			}

			/*
			*  buf is allocated by the kernel (4ko) and we will
			*  never write more than 6 bytes so no need to check
			*/
			ret = sprintf(buf, "0x%x\n", rev[0]);
			if (ret < 0) {
				pr_err(
					"%s: cannot convert the value, ret = %d",
					__func__,
					ret);
				return ret;
			}

			*ppos += ret;
			return ret;
		}
	}

	pr_err("%s: param not found\n", __func__);
	return -EFAULT;
}

/* Attach the debugfs operations methods */
static const struct file_operations scu_ipc_osnib_reset_event_fops = {
	.owner = THIS_MODULE,
	.read  = intel_scu_ipc_osnib_read_reset_event,
};

static struct dentry *scu_ipc_osnib_dir;
static struct dentry *scu_ipc_osnib_file_reset_ev1;
static struct dentry *scu_ipc_osnib_file_reset_ev2;

static ssize_t intel_scu_ipc_osnib_read_checksum(
	struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	loff_t pos = *ppos;
	u8 checksum = 0;
	int ret;

	if (pos > 0)
		return 0;

	ret = oshob_info->scu_ipc_read_osnib(
	    &checksum,
	    1,
	    offsetof(struct scu_ipc_osnib, checksum));

	if (ret != 0) {
		pr_err("%s: cannot read CHECKSUM, ret=%d", __func__, ret);
		return ret;
	}

	/*
	*  buf is allocated by the kernel (4ko) and we will
	*  never write more than 6 bytes so no need to check
	*/
	ret = sprintf(buf, "0x%x\n", checksum);
	if (ret < 0) {
		pr_err("%s: cannot convert the value, ret = %d", __func__, ret);
		return ret;
	}

	*ppos += ret;
	return ret;
}

static const struct file_operations scu_ipc_osnib_checksum_fops = {
	.owner = THIS_MODULE,
	.read  = intel_scu_ipc_osnib_read_checksum,
};

static struct dentry *scu_ipc_osnib_file_checksum;

/*
*	debugfs interface: init interface.
*/
static int intel_mid_scu_ipc_osnib_debugfs_init(void)
{
	int i;
	int ret = 0;
	bool found = false;

	/* Create debugfs directory /sys/kernel/debug/intel_scu_osnib */
	scu_ipc_osnib_dir = debugfs_create_dir("intel_scu_osnib", NULL);

	if (!scu_ipc_osnib_dir) {
		pr_err("%s: cannot create OSNIB debugfs directory\n", __func__);
		return -1;
	}

	scu_ipc_osnib_file_checksum = debugfs_create_file(
		"CHECKSUM",
		S_IFREG | S_IRUSR,
		scu_ipc_osnib_dir,
		NULL,
		&scu_ipc_osnib_checksum_fops);

	if (!scu_ipc_osnib_file_checksum) {
		pr_err("%s: cannot create CHECKSUM debugfs file\n", __func__);
		ret = -1;
	}

	if (!debugfs_create_bool("invalid_checksum", S_IFREG | S_IRUSR | S_IWUSR,
		scu_ipc_osnib_dir, &invalid_checksum)) {
		pr_err("%s: cannot create invalid_checksum debugfs file\n", __func__);
		ret = -1;
	}

	for (i = 0; i < ARRAY_SIZE(chip_reset_events); i++) {
		if (chip_reset_events[i].id == oshob_info->platform_type) {

			scu_ipc_osnib_file_reset_ev1 = debugfs_create_file(
					chip_reset_events[i].reset_ev1_name,
					S_IFREG | S_IRUSR,
					scu_ipc_osnib_dir,
					NULL, &scu_ipc_osnib_reset_event_fops);

			if (!scu_ipc_osnib_file_reset_ev1) {
				pr_err("%s: cannot create %s debugfs file\n",
					__func__,
					chip_reset_events[i].reset_ev1_name);
				ret = -1;
			}

			scu_ipc_osnib_file_reset_ev2 = debugfs_create_file(
					chip_reset_events[i].reset_ev2_name,
					S_IFREG | S_IRUSR,
					scu_ipc_osnib_dir,
					NULL, &scu_ipc_osnib_reset_event_fops);

			if (!scu_ipc_osnib_file_reset_ev2) {
				pr_err("%s: cannot create %s debugfs file\n",
					__func__,
					chip_reset_events[i].reset_ev1_name);
				ret = -1;
			}

			found = true;
			break;
		}
	}

	if (!found) {
		pr_err("%s: param not found\n", __func__);
		ret = -EFAULT;
	}

	return ret;
}

/*
*	debugfs interface: exit interface.
*/
static void intel_mid_scu_ipc_osnib_debugfs_exit(void)
{
	debugfs_remove_recursive(scu_ipc_osnib_dir);
}

#endif /* DUMP_OSNIB */

#endif /* CONFIG_DEBUG_FS */

static const struct file_operations scu_ipc_fops = {
	.unlocked_ioctl = scu_ipc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = scu_ipc_ioctl,
#endif
};

static struct miscdevice scu_ipcutil = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mid_ipc",
	.fops = &scu_ipc_fops,
};

static int oshob_init(void)
{
	int ret, i;
	u16 struct_offs;

#ifdef DUMP_OSNIB
	u8 rr, reset_ev1, reset_ev2, wd, alarm, wakesrc, *ptr;
	int rr_found = 0, wksrc_found = 0;
	u32 pmit, scu_trace[OSHOB_SCU_BUF_BASE_DW_SIZE*4], ia_trace;
	int buff_size;
#endif

	/* Identify the type and size of OSHOB to be used. */
	ret = intel_scu_ipc_read_oshob_info();

	if (ret != 0) {
		pr_err("Cannot init ipc module: oshob info not read\n");
		goto exit;
	}

#ifdef DUMP_OSNIB
	/* Dumping reset events from the interrupt tree */
	ret = intel_scu_ipc_read_oshob_it_tree(&pmit);

	if (ret != 0) {
		pr_err("Cannot read interrupt tree\n");
		goto exit;
	}

	ptr = ioremap_nocache(pmit + PMIT_RESET1_OFFSET, 2);

	if (!ptr) {
		pr_err("Cannot remap PMIT\n");
		ret = -ENOMEM;
		goto exit;
	}

	pr_debug("PMIT addr 0x%8x remapped to 0x%p\n", pmit, ptr);

	reset_ev1 = readb(ptr);
	reset_ev2 = readb(ptr+1);
	for (i = 0; i < ARRAY_SIZE(chip_reset_events); i++) {
		if (chip_reset_events[i].id == oshob_info->platform_type) {
			pr_warn("[BOOT] %s=0x%02x %s=0x%02x (PMIT interrupt tree)\n",
				chip_reset_events[i].reset_ev1_name,
				reset_ev1,
				chip_reset_events[i].reset_ev2_name,
				reset_ev2);
		}
	}
	iounmap(ptr);

	/* Dumping OSHOB content */
	if ((oshob_info->oshob_majrev == OSHOB_REV_MAJ_DEFAULT) &&
	    (oshob_info->oshob_minrev == OSHOB_REV_MIN_DEFAULT)) {
		/* Use default OSHOB here. Calculate in bytes here. */
		if ((oshob_info->platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(oshob_info->platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE))
			buff_size = OSHOB_SCU_BUF_MRFLD_DW_SIZE*4;
		else
			buff_size = OSHOB_SCU_BUF_BASE_DW_SIZE*4;

		ret = intel_scu_ipc_read_oshob(
			(u8 *)(scu_trace),
			buff_size,
			offsetof(struct scu_ipc_oshob, scutxl));

		if (ret != 0) {
			pr_err("Cannot read SCU data\n");
			goto exit;
		}

		struct_offs = offsetof(struct scu_ipc_oshob, iatxl) +
				oshob_info->offs_add;
		ret = intel_scu_ipc_read_oshob(
			    (u8 *)(&ia_trace),
			    4,
			    struct_offs);

		if (ret != 0) {
			pr_err("Cannot read IA data\n");
			goto exit;
		}
	    } else {
		/* Use extended OSHOB here. Calculate in bytes here. */
		if ((oshob_info->platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(oshob_info->platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE))
			buff_size = OSHOB_SCU_BUF_MRFLD_DW_SIZE*4;
		else
			buff_size = OSHOB_SCU_BUF_BASE_DW_SIZE*4;

		if ((oshob_info->oshob_majrev >= 1) &&
		    (oshob_info->oshob_minrev >= 4)) {
			ret = intel_scu_ipc_read_oshob(
				(u8 *)(scu_trace),
				buff_size,
				offsetof(struct scu_ipc_oshob_extend_v14,
								scutxl));
		} else {
			ret = intel_scu_ipc_read_oshob(
				(u8 *)(scu_trace),
				buff_size,
				offsetof(struct scu_ipc_oshob_extend, scutxl));
		}

		if (ret != 0) {
			pr_err("Cannot read SCU data\n");
			goto exit;
		}

		if ((oshob_info->oshob_majrev >= 1) &&
		    (oshob_info->oshob_minrev >= 4)) {
			struct_offs = offsetof(struct scu_ipc_oshob_extend_v14,
						iatxl) + oshob_info->offs_add;
		} else {
			struct_offs = offsetof(struct scu_ipc_oshob_extend,
						iatxl) + oshob_info->offs_add;
		}

		ret = intel_scu_ipc_read_oshob(
				(u8 *)(&ia_trace),
				4,
				struct_offs);

		if (ret != 0) {
			pr_err("Cannot read IA data\n");
			goto exit;
		}
	}

	if ((oshob_info->platform_type == INTEL_MID_CPU_CHIP_TANGIER) ||
		(oshob_info->platform_type == INTEL_MID_CPU_CHIP_ANNIEDALE)) {
		for (i = 0; i < OSHOB_SCU_BUF_MRFLD_DW_SIZE; i++)
			pr_warn("[BOOT] SCU_TR[%d]=0x%08x\n", i, scu_trace[i]);
	} else
		pr_warn("[BOOT] SCU_TR=0x%08x (oshob)\n", scu_trace[0]);

	pr_warn("[BOOT] IA_TR=0x%08x (oshob)\n", ia_trace);

	/* Dumping OSNIB content */
	ret = 0;
	ret |= intel_scu_ipc_read_osnib_rr(&rr);
	ret |= intel_scu_ipc_read_osnib_reset_ev1(&reset_ev1);
	ret |= intel_scu_ipc_read_osnib_reset_ev2(&reset_ev2);
	ret |= intel_scu_ipc_read_osnib_wd(&wd);
	ret |= intel_scu_ipc_read_osnib_alarm(&alarm);
	ret |= intel_scu_ipc_read_osnib_wakesrc(&wakesrc);

	if (ret) {
		pr_err("Cannot read OSNIB content\n");
		goto exit;
	}

	for (i = 0; i < ARRAY_SIZE(osnib_target_oses); i++) {
		if (osnib_target_oses[i].id == rr) {
			pr_warn("[BOOT] RR=[%s] WD=0x%02x ALARM=0x%02x (osnib)\n",
				osnib_target_oses[i].target_os_name, wd, alarm);
			rr_found++;
			break;
		}
	}

	if (!rr_found)
		pr_warn("[BOOT] RR=[UNKNOWN 0x%02x] WD=0x%02x ALARM=0x%02x (osnib)\n",
			rr, wd, alarm);

	pr_warn("[BOOT] WD[3..0] bits %scleared by IA FW (osnib)\n",
		(wd & 0x0F) ? "NOT " : "");

	for (i = 0; i < ARRAY_SIZE(osnib_wake_srcs); i++) {
		if (osnib_wake_srcs[i].id == wakesrc) {
			pr_warn("[BOOT] WAKESRC=[%s] (osnib)\n",
				osnib_wake_srcs[i].wakesrc_name);
			wksrc_found++;
			break;
		}
	}

	if (!wksrc_found)
		pr_warn("[BOOT] WAKESRC=[UNKNOWN 0x%02x] (osnib)\n", wakesrc);

	for (i = 0; i < ARRAY_SIZE(chip_reset_events); i++) {
		if (chip_reset_events[i].id == oshob_info->platform_type) {
			pr_warn("[BOOT] %s=0x%02x %s=0x%02x (osnib)\n",
				chip_reset_events[i].reset_ev1_name,
				reset_ev1,
				chip_reset_events[i].reset_ev2_name,
				reset_ev2);
			break;
		}
	}

#endif /* DUMP_OSNIB */

#ifdef CONFIG_DEBUG_FS
	if (oshob_info->oshob_majrev != OSHOB_REV_MAJ_DEFAULT) {
		/* OEMNIB only usable with extended OSHOB structure. */
		ret = intel_mid_scu_ipc_oemnib_debugfs_init();

		if (ret != 0) {
			pr_err("Cannot register OEMNIB interface to debugfs\n");
			goto exit;
		} else {
			pr_info("OEMNIB interface registered to debugfs\n");
		}
	}
	ret = intel_mid_scu_ipc_cmd_debugfs_init();
	if (ret) {
		pr_err("Cannot register ipc cmd interface to debugfs");
		goto exit;
	}

#ifdef DUMP_OSNIB
	if (intel_mid_scu_ipc_osnib_debugfs_init() != 0)
		pr_err("Problem when register OSNIB interface to debugfs\n");
	else
		pr_info("OSNIB interface registered to debugfs\n");
#endif /* DUMP_OSNIB */
#endif /* CONFIG_DEBUG_FS */

exit:
	return ret;
}

static int ipcutil_rpmsg_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;

	oshob_info = kmalloc(sizeof(struct scu_ipc_oshob_info), GFP_KERNEL);
	if (oshob_info == NULL) {
		pr_err(
		"Cannot init ipc module: oshob info struct not allocated\n");
		return -ENOMEM;
	}

	if (rpdev == NULL) {
		pr_err("ipcutil rpmsg channel not created\n");
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed ipcutil rpmsg device\n");

	/* Allocate rpmsg instance for mip*/
	ret = alloc_rpmsg_instance(rpdev, &ipcutil_instance);
	if (!ipcutil_instance) {
		dev_err(&rpdev->dev, "kzalloc ipcutil instance failed\n");
		goto out;
	}

	/* Initialize rpmsg instance */
	init_rpmsg_instance(ipcutil_instance);

	ret = oshob_init();
	if (ret)
		goto misc_err;

	ret = misc_register(&scu_ipcutil);
	if (ret) {
		pr_err("misc register failed\n");
		goto misc_err;
	}

	return ret;

misc_err:
	free_rpmsg_instance(rpdev, &ipcutil_instance);
out:
	kfree(oshob_info);
	return ret;
}

static void ipcutil_rpmsg_remove(struct rpmsg_channel *rpdev)
{
#ifdef CONFIG_DEBUG_FS
	if (oshob_info->oshob_majrev != OSHOB_REV_MAJ_DEFAULT) {
		/* OEMNIB only usable with extended OSHOB structure. */
		/* unregister from debugfs.                     */
		intel_mid_scu_ipc_oemnib_debugfs_exit();
	}
	intel_mid_scu_ipc_cmd_debugfs_exit();

#ifdef DUMP_OSNIB
	intel_mid_scu_ipc_osnib_debugfs_exit();
#endif /* DUMP_OSNIB */
#endif /* CONFIG_DEBUG_FS */

	kfree(oshob_info);

	/* unregister scu_ipc_ioctl from sysfs. */
	misc_deregister(&scu_ipcutil);
	free_rpmsg_instance(rpdev, &ipcutil_instance);
}

static void ipcutil_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
					int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
		       data, len,  true);
}

static struct rpmsg_device_id ipcutil_rpmsg_id_table[] = {
	{ .name	= "rpmsg_ipc_util" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, ipcutil_rpmsg_id_table);

static struct rpmsg_driver ipcutil_rpmsg = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= ipcutil_rpmsg_id_table,
	.probe		= ipcutil_rpmsg_probe,
	.callback	= ipcutil_rpmsg_cb,
	.remove		= ipcutil_rpmsg_remove,
};

static int __init ipcutil_rpmsg_init(void)
{
	return register_rpmsg_driver(&ipcutil_rpmsg);
}

static void __exit ipcutil_rpmsg_exit(void)
{
	unregister_rpmsg_driver(&ipcutil_rpmsg);
}

rootfs_initcall(ipcutil_rpmsg_init);
module_exit(ipcutil_rpmsg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Utility driver for intel scu ipc");
MODULE_AUTHOR("Sreedhara <sreedhara.ds@intel.com>");
