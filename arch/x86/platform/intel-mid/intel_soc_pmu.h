/*
 * intel_soc_pmu.h
 * Copyright (c) 2012, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#ifndef _MID_PMU_H_
#define _MID_PMU_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/jhash.h>
#include <linux/suspend.h>
#include <linux/workqueue.h>
#include <linux/nmi.h>
#include <linux/pm_qos.h>
#include <linux/pm_wakeup.h>
#include <asm/apic.h>
#include <asm/intel_scu_ipc.h>
#include <linux/intel_mid_pm.h>

#include "intel_soc_mdfld.h"
#include "intel_soc_clv.h"
#include "intel_soc_mrfld.h"

#define MID_PMU_MFLD_DRV_DEV_ID                 0x0828
#define MID_PMU_CLV_DRV_DEV_ID			0x08EC
#define MID_PMU_MRFL_DRV_DEV_ID			0x11A1

#define MID_MRFL_HDMI_DRV_DEV_ID		0x11A6

/* SRAM address where PANIC START is written */
#define PMU_PANIC_EMMC_UP_ADDR			0xFFFF3080
#define PMU_PANIC_EMMC_UP_REQ_CMD		0xDEADBEEF

#define MAX_DEVICES	(PMU1_MAX_DEVS + PMU2_MAX_DEVS)
#define PMU_MAX_LSS_SHARE 4

#define PMU2_BUSY_TIMEOUT			500000
#define HSU0_PCI_ID				0x81c
#define HSU1_PCI_ID				0x81b
#define HSI_PCI_ID				0x833

#define MODE_ID_MAGIC_NUM			1

#define   LOG_ID_MASK				0x7F
#define   SUB_CLASS_MASK			0xFF00


/* Definition for C6 Offload MSR Address */
#define MSR_C6OFFLOAD_CTL_REG			0x120

#define MSR_C6OFFLOAD_SET_LOW			1
#define MSR_C6OFFLOAD_SET_HIGH			0

#define C6OFFLOAD_BIT_MASK			0x2
#define C6OFFLOAD_BIT				0x2

#define PMU_DRV_NAME				"intel_pmu_driver"

#define MID_PCI_INDEX_HASH_BITS		8 /*size 256*/
#define MID_PCI_INDEX_HASH_SIZE		(1<<MID_PCI_INDEX_HASH_BITS)
#define MID_PCI_INDEX_HASH_MASK		(MID_PCI_INDEX_HASH_SIZE-1)

/* some random number for initvalue */
#define	MID_PCI_INDEX_HASH_INITVALUE	0x27041975

/*
 * Values for programming the PM_CMD register based on the PM
 * architecture speci
*/

#define S5_VALUE	0x309D2601
#define S0I1_VALUE	0X30992601
#define LPMP3_VALUE	0X40492601
#define S0I3_VALUE	0X309B2601
#define FAST_ON_OFF_VALUE	0X309E2601
#define INTERACTIVE_VALUE	0X00002201
#define INTERACTIVE_IOC_VALUE	0X00002301

#define WAKE_ENABLE_0		0xffffffff
#define WAKE_ENABLE_1		0xffffffff
#define INVALID_WAKE_SRC	0xFFFF

#define LOG_SS_MASK		0x80

#define D0I0_MASK		0
#define D0I1_MASK		1
#define D0I2_MASK		2
#define D0I3_MASK		3

#define BITS_PER_LSS		2
#define MAX_LSS_POSSIBLE	64
#define SS_IDX_MASK		0x3
#define SS_POS_MASK		0xF

#define SSMSK(mask, lss) ((mask) << ((lss) * 2))
#define SSWKC(lss) (1 << (lss))

/* North Complex Power management */
#define OSPM_PUNIT_PORT         0x04
#define OSPM_OSPMBA             0x78
#define OSPM_PM_SSC             0x20
#define OSPM_PM_SSS             0x30

#define OSPM_APMBA              0x7a
#define APM_CMD                 0x0
#define APM_STS                 0x04
#define PM_CMD_D3_COLD		(0x1 << 21)

/* Size of command logging array */
#define LOG_SIZE	5

enum sys_state {
	SYS_STATE_S0I0,
	SYS_STATE_S0I1,
	SYS_STATE_S0I1_LPMP3,
	SYS_STATE_S0I1_PSH,
	SYS_STATE_S0I1_DISP,
	SYS_STATE_S0I1_LPMP3_PSH,
	SYS_STATE_S0I1_LPMP3_DISP,
	SYS_STATE_S0I1_PSH_DISP,
	SYS_STATE_S0I1_LPMP3_PSH_DISP,
	SYS_STATE_S0I2,
	SYS_STATE_S0I3,
	SYS_STATE_S0I3_PSH_RET,
	SYS_STATE_S3,
	SYS_STATE_S5,
	SYS_STATE_MAX
};

enum int_status {
	INVALID_INT = 0,
	CMD_COMPLETE_INT = 1,
	CMD_ERROR_INT = 2,
	WAKE_RECEIVED_INT = 3,
	SUBSYS_POW_ERR_INT = 4,
	S0ix_MISS_INT = 5,
	NO_ACKC6_INT = 6,
	TRIGGERERR = 7,
	INVALID_SRC_INT
};

enum pmu_number {
	PMU_NUM_1,
	PMU_NUM_2,
	PMU_MAX_DEVS
};

enum pmu_ss_state {
	SS_STATE_D0I0 = 0,
	SS_STATE_D0I1 = 1,
	SS_STATE_D0I2 = 2,
	SS_STATE_D0I3 = 3
};

enum pmu_mrfl_nc_device_name {
	GFXSLC = 0,
	GSDKCK,
	GRSCD,
	VED,
	VEC,
	DPA,
	DPB,
	DPC,
	VSP,
	ISP,
	MIO,
	HDMIO,
	GFXSLCLDO
};


struct pmu_ss_states {
	unsigned long pmu1_states;
	unsigned long pmu2_states[4];
};

struct pci_dev_info {
	u8 ss_pos;
	u8 ss_idx;
	u8 pmu_num;

	u32 log_id;
	u32 cap;
	struct pci_dev *drv[PMU_MAX_LSS_SHARE];
	pci_power_t power_state[PMU_MAX_LSS_SHARE];
};

struct pmu_wake_ss_states {
	unsigned long wake_enable[2];
	unsigned long pmu1_wake_states;
	unsigned long pmu2_wake_states[4];
};

struct pmu_suspend_config {
	struct pmu_ss_states ss_state;
	struct pmu_wake_ss_states wake_state;
};

struct pci_dev_index {
	struct pci_dev	*pdev;
	u8		index;
};

/* PMU register interface */
struct mrst_pmu_reg {
	u32 pm_sts;             /* 0x00 */
	u32 pm_cmd;             /* 0x04 */
	u32 pm_ics;             /* 0x08 */
	u32 _resv1;
	u32 pm_wkc[2];          /* 0x10 */
	u32 pm_wks[2];          /* 0x18 */
	u32 pm_ssc[4];          /* 0x20 */
	u32 pm_sss[4];          /* 0x30 */
	u32 pm_wssc[4];         /* 0x40 */
	u32 pm_c3c4;            /* 0x50 */
	u32 pm_c5c6;            /* 0x54 */
	u32 pm_msic;            /* 0x58 */
};

struct mid_pmu_cmd_log {
	struct timespec ts;
	u32 command;
	struct pmu_ss_states pm_ssc;
};

struct mid_pmu_irq_log {
	struct timespec ts;
	u32 status;
};

struct mid_pmu_ipc_log {
	struct timespec ts;
	u32 command;
};

struct mid_pmu_pmu_irq_log {
	struct timespec ts;
	u8 status;
};

struct mid_pmu_ipc_irq_log {
	struct timespec ts;
};

union pmu_pm_status {
	struct {
		u32 pmu_rev:8;
		u32 pmu_busy:1;
		u32 mode_id:4;
		u32 Reserved:19;
	} pmu_status_parts;
	u32 pmu_status_value;
};

union pmu_pm_ics {
	struct {
		u32 int_status:8;
		u32 int_enable:1;
		u32 int_pend:1;
		/* New bit added in TNG to indicate device wakes*/
		u32 sw_int_status:1;
		u32 reserved:21;
	} pmu_pm_ics_parts;
	u32 pmu_pm_ics_value;
};

struct intel_mid_base_addr {
	u32 *pm_table_base;
	u32 __iomem *offload_reg;
};

#define MAX_PMU_LOG_STATES	(S0I3_STATE_IDX - C4_STATE_IDX + 1)

struct mid_pmu_stats {
	u64 err_count[3];
	u64 count;
	u64 time;
	u64 last_entry;
	u64 last_try;
	u64 first_entry;
	u32 demote_count[MAX_PMU_LOG_STATES];
	u32 display_blocker_count;
	u32 camera_blocker_count;
	u32 blocker_count[MAX_LSS_POSSIBLE];
};

struct device_residency {
	u64 d0i0_entry;
	u64 d0i3_entry;
	u64 d0i0_acc;
	u64 d0i3_acc;
	u64 start;
	pci_power_t state;
};

struct mid_pmu_dev {
	bool suspend_started;
	bool shutdown_started;
	bool camera_off;
	bool display_off;

	u32 apm_base;
	u32 ospm_base;
	u32 pmu1_max_devs;
	u32 pmu2_max_devs;
	u32 ss_per_reg;
	u32 d0ix_stat[MAX_LSS_POSSIBLE][SS_STATE_D0I3+1];
	u32 num_wakes[MAX_DEVICES][SYS_STATE_MAX];
	u32 ignore_lss[4];
	u32 os_sss[4];
#ifdef CONFIG_PM_DEBUG
	u32 cstate_ignore;
	struct pm_qos_request *cstate_qos;
#endif

	u32 __iomem *emergency_emmc_up_addr;
	u64 pmu_init_time;

	u32 d0i0_count[MAX_LSS_POSSIBLE];
	u64 d0i0_prev_time[MAX_LSS_POSSIBLE];
	u64 d0i0_time[MAX_LSS_POSSIBLE];

	u32 nc_d0i0_count[OSPM_MAX_POWER_ISLANDS];
	u64 nc_d0i0_time[OSPM_MAX_POWER_ISLANDS];
	u64 nc_d0i0_prev_time[OSPM_MAX_POWER_ISLANDS];

	int cmd_error_int;
	int s0ix_possible;
	int s0ix_entered;

#ifdef LOG_PMU_EVENTS
	int cmd_log_idx;
	int ipc_log_idx;
	int ipc_irq_log_idx;
	int pmu_irq_log_idx;
#endif

	enum sys_state  pmu_current_state;

	struct pci_dev_info pci_devs[MAX_DEVICES];
	struct pci_dev_index
		pci_dev_hash[MID_PCI_INDEX_HASH_SIZE];
	struct intel_mid_base_addr base_addr;
	struct mrst_pmu_reg	__iomem *pmu_reg;
	struct semaphore scu_ready_sem;
	struct mid_pmu_stats pmu_stats[SYS_STATE_MAX];
	struct device_residency pmu_dev_res[MAX_DEVICES];
	struct delayed_work log_work;
	struct pm_qos_request *s3_restrict_qos;

#ifdef LOG_PMU_EVENTS
	struct mid_pmu_cmd_log cmd_log[LOG_SIZE];
	struct mid_pmu_ipc_log ipc_log[LOG_SIZE];
	struct mid_pmu_ipc_irq_log ipc_irq_log[LOG_SIZE];
	struct mid_pmu_pmu_irq_log pmu_irq_log[LOG_SIZE];
#endif
	struct wakeup_source *pmu_wake_lock;

	struct pmu_suspend_config *ss_config;
	struct pci_dev *pmu_dev;
	struct pm_qos_request *nc_restrict_qos;

	spinlock_t nc_ready_lock;

	int s3_hint;
};

struct platform_pmu_ops {
	int (*init)(void);
	void (*prepare)(int);
	bool (*enter)(int);
	void (*wakeup)(void);
	void (*remove)(void);
	pci_power_t (*pci_choose_state) (int);
	void (*set_power_state_ops) (int);
	void (*set_s0ix_complete) (void);
	void (*set_s0i1_disp_vote) (bool);
	int (*nc_set_power_state) (int, int, int, int *);
	bool (*check_nc_sc_status) (void);
};

extern char s0ix[5];
extern struct platform_pmu_ops mfld_pmu_ops;
extern struct platform_pmu_ops clv_pmu_ops;
extern struct platform_pmu_ops mrfld_pmu_ops;
extern struct platform_pmu_ops *get_platform_ops(void);
extern void mfld_s0ix_sram_save_cleanup(void);
extern void pmu_stats_init(void);
extern void pmu_s3_stats_update(int enter);
extern void pmu_stats_finish(void);
extern void mfld_s0ix_sram_restore(u32 s0ix);
extern void pmu_stat_error(u8 err_type);
extern void pmu_stat_end(void);
extern void pmu_stat_start(enum sys_state type);
extern int pmu_pci_to_indexes(struct pci_dev *pdev, int *index,
				int *pmu_num, int *ss_idx, int *ss_pos);
extern struct mid_pmu_dev *mid_pmu_cxt;
extern void platform_set_pmu_ops(void);
extern void pmu_read_sss(struct pmu_ss_states *pm_ssc);
extern int pmu_issue_interactive_command(struct pmu_ss_states *pm_ssc,
				bool ioc, bool d3_cold);
extern int _pmu2_wait_not_busy(void);
extern u32 get_s0ix_val_set_pm_ssc(int);
extern int pmu_get_wake_source(void);
extern bool pmu_initialized;
extern struct platform_pmu_ops *pmu_ops;
extern void platform_update_all_lss_states(struct pmu_ss_states *, int *);
extern int set_extended_cstate_mode(const char *val, struct kernel_param *kp);
extern int get_extended_cstate_mode(char *buffer, struct kernel_param *kp);
extern int byt_pmu_nc_set_power_state(int islands, int state_type, int reg);
extern int byt_pmu_nc_get_power_state(int islands, int reg);
extern void pmu_set_interrupt_enable(void);
extern void pmu_clear_interrupt_enable(void);

#ifdef LOG_PMU_EVENTS
extern void pmu_log_pmu_irq(int status);
extern void pmu_log_command(u32 command, struct pmu_ss_states *pm_ssc);
extern void pmu_dump_logs(void);
#endif

/* Accessor function for pci_devs start */
static inline void pmu_stat_clear(void)
{
	mid_pmu_cxt->pmu_current_state = SYS_STATE_S0I0;
}

static inline struct pci_dev *get_mid_pci_drv(int lss_index, int i)
{
	return mid_pmu_cxt->pci_devs[lss_index].drv[i];
}

static inline pci_power_t get_mid_pci_power_state(int lss_index, int i)
{
	return mid_pmu_cxt->pci_devs[lss_index].power_state[i];
}

static inline u8 get_mid_pci_ss_idx(int lss_index)
{
	return mid_pmu_cxt->pci_devs[lss_index].ss_idx & SS_IDX_MASK;
}

static inline u8 get_mid_pci_ss_pos(int lss_index)
{
	return mid_pmu_cxt->pci_devs[lss_index].ss_pos & SS_POS_MASK;
}

static inline u8 get_mid_pci_pmu_num(int lss_index)
{
	return mid_pmu_cxt->pci_devs[lss_index].pmu_num;
}

static inline void set_mid_pci_drv(int lss_index,
					int i, struct pci_dev *pdev)
{
	mid_pmu_cxt->pci_devs[lss_index].drv[i] = pdev;
}

static inline void set_mid_pci_power_state(int lss_index,
					int i, pci_power_t state)
{
	mid_pmu_cxt->pci_devs[lss_index].power_state[i] = state;
}

static inline void set_mid_pci_ss_idx(int lss_index, u8 ss_idx)
{
	mid_pmu_cxt->pci_devs[lss_index].ss_idx = ss_idx;
}

static inline void set_mid_pci_ss_pos(int lss_index, u8 ss_pos)
{
	mid_pmu_cxt->pci_devs[lss_index].ss_pos = ss_pos;
}

static inline void set_mid_pci_pmu_num(int lss_index, u8 pmu_num)
{
	mid_pmu_cxt->pci_devs[lss_index].pmu_num = pmu_num;
}

static inline void set_mid_pci_log_id(int lss_index, u32 log_id)
{
	mid_pmu_cxt->pci_devs[lss_index].log_id = log_id;
}

static inline void set_mid_pci_cap(int lss_index, u32 cap)
{
	mid_pmu_cxt->pci_devs[lss_index].cap = cap;
}

static inline u32 get_d0ix_stat(int lss_index, int state)
{
	return mid_pmu_cxt->d0ix_stat[lss_index][state];
}

static inline void inc_d0ix_stat(int lss_index, int state)
{
	mid_pmu_cxt->d0ix_stat[lss_index][state]++;
}

static inline void clear_d0ix_stats(void)
{
	memset(mid_pmu_cxt->d0ix_stat, 0, sizeof(mid_pmu_cxt->d0ix_stat));
}

/* Accessor functions for pci_devs end */

static inline bool nc_device_state(void)
{
	return !mid_pmu_cxt->display_off || !mid_pmu_cxt->camera_off;
}

#ifdef CONFIG_X86_INTEL_OSC_CLK
extern int ccu_osc_clk_init(void __iomem *ccubase);
extern int ccu_osc_clk_uninit(void);
#else
static inline int ccu_osc_clk_init(void __iomem *ccubase) {return 0; }
static inline int ccu_osc_clk_uninit(void) {return 0; }
#endif /* CONFIG_X86_INTEL_OSC_CLK */
#endif
