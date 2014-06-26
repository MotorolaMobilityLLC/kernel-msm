/*
 * intel_soc_pmu.c - This driver provides interface to configure the 2 pmu's
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

#include "intel_soc_pmu.h"
#include <linux/cpuidle.h>
#include <linux/proc_fs.h>
#include <asm/stacktrace.h>
#include <asm/intel_mid_rpmsg.h>

#include <asm/hypervisor.h>
#include <asm/xen/hypercall.h>

#ifdef CONFIG_DRM_INTEL_MID
#define GFX_ENABLE
#endif

bool pmu_initialized;

DEFINE_MUTEX(pci_root_lock);

/* mid_pmu context structure */
struct mid_pmu_dev *mid_pmu_cxt;

struct platform_pmu_ops *pmu_ops;
/*
 * Locking strategy::
 *
 * one semaphore (scu_ready sem) is used for accessing busy bit,
 * issuing interactive cmd in the code.
 * The entry points in pmu driver are pmu_pci_set_power_state()
 * and PMU interrupt handler contexts, so here is the flow of how
 * the semaphore is used.
 *
 * In D0ix command case::
 * set_power_state process context:
 * set_power_state()->acquire_scu_ready_sem()->issue_interactive_cmd->
 * wait_for_interactive_complete->release scu_ready sem
 *
 * PMU Interrupt context:
 * pmu_interrupt_handler()->release interactive_complete->return
 *
 * In Idle handler case::
 * Idle context:
 * idle_handler()->try_acquire_scu_ready_sem->if acquired->
 * issue s0ix command->return
 *
 * PMU Interrupt context:
 * pmu_Interrupt_handler()->release scu_ready_sem->return
 *
 */

/* Maps pci power states to SCU D0ix mask */
static int pci_to_platform_state(pci_power_t pci_state)
{

	static int mask[]  = {D0I0_MASK, D0I1_MASK,
				D0I2_MASK, D0I3_MASK, D0I3_MASK};

	int state = D0I0_MASK;

	if (pci_state > 4)
		WARN(1, "%s: wrong pci_state received.\n", __func__);

	else
		state = mask[pci_state];

	return state;
}

/* Maps power states to pmu driver's internal indexes */
int mid_state_to_sys_state(int mid_state)
{
	int sys_state = 0;
	switch (mid_state) {
	case MID_S0I1_STATE:
		sys_state = SYS_STATE_S0I1;
		break;
	case MID_LPMP3_STATE:
		sys_state = SYS_STATE_S0I2;
		break;
	case MID_S0I3_STATE:
		sys_state = SYS_STATE_S0I3;
		break;
	case MID_S3_STATE:
		sys_state = SYS_STATE_S3;
		break;

	case C6_HINT:
		sys_state = SYS_STATE_S0I0;
	}

	return sys_state;
}

/* PCI Device Id structure */
static DEFINE_PCI_DEVICE_TABLE(mid_pm_ids) = {
	{PCI_VDEVICE(INTEL, MID_PMU_MFLD_DRV_DEV_ID), 0},
	{PCI_VDEVICE(INTEL, MID_PMU_CLV_DRV_DEV_ID), 0},
	{PCI_VDEVICE(INTEL, MID_PMU_MRFL_DRV_DEV_ID), 0},
	{}
};

MODULE_DEVICE_TABLE(pci, mid_pm_ids);

char s0ix[5] = "s0ix";

module_param_call(s0ix, set_extended_cstate_mode,
		get_extended_cstate_mode, NULL, 0644);

MODULE_PARM_DESC(s0ix,
	"setup extended c state s0ix mode [s0i3|s0i1|lmp3|"
				"i1i3|lpi1|lpi3|s0ix|none]");

/* LSS without driver are powered up on the resume from standby
 * preventing back to back S3/S0ix. IGNORE the PCI_DO transtion
 * for such devices.
 */
static inline bool pmu_power_down_lss_without_driver(int index,
			int sub_sys_index, int sub_sys_pos, pci_power_t state)
{
	/* Ignore NC devices */
	if (index < PMU1_MAX_DEVS)
		return false;

	/* Only ignore D0i0 */
	if (state != PCI_D0)
		return false;

	/* HSI not used in MRFLD. IGNORE HSI Transition to D0 for MRFLD.
	 * Sometimes it is turned ON during resume in the absence of a driver
	 */
	if (platform_is(INTEL_ATOM_MRFLD))
		return ((sub_sys_index == 0x0) && (sub_sys_pos == 0x5));

	/* For MOFD ignore D0i0 on LSS 5, 7 */
	if ((platform_is(INTEL_ATOM_MOORFLD)) && (sub_sys_index == 0x0))
		return ((sub_sys_pos == 0x5) || (sub_sys_pos == 0x7));

	return false;
}

/**
 * This function set all devices in d0i0 and deactivates pmu driver.
 * The function is used before IFWI update as it needs devices to be
 * in d0i0 during IFWI update. Reboot is needed to work pmu
 * driver properly again. After calling this function and IFWI
 * update, system is always rebooted as IFWI update function,
 * intel_scu_ipc_medfw_upgrade() is called from mrst_emergency_reboot().
 */
int pmu_set_devices_in_d0i0(void)
{
	int status;
	struct pmu_ss_states cur_pmssc;

	/* Ignore request until we have initialized */
	if (unlikely((!pmu_initialized)))
		return 0;

	cur_pmssc.pmu2_states[0] = D0I0_MASK;
	cur_pmssc.pmu2_states[1] = D0I0_MASK;
	cur_pmssc.pmu2_states[2] = D0I0_MASK;
	cur_pmssc.pmu2_states[3] = D0I0_MASK;

	/* Restrict platform Cx state to C6 */
	pm_qos_update_request(mid_pmu_cxt->s3_restrict_qos,
				(CSTATE_EXIT_LATENCY_S0i1-1));

	down(&mid_pmu_cxt->scu_ready_sem);

	mid_pmu_cxt->shutdown_started = true;

	/* Issue the pmu command to PMU 2
	 * flag is needed to distinguish between
	 * S0ix vs interactive command in pmu_sc_irq()
	 */
	status = pmu_issue_interactive_command(&cur_pmssc, false, false);

	if (unlikely(status != PMU_SUCCESS)) {	/* pmu command failed */
		printk(KERN_CRIT "%s: Failed to Issue a PM command to PMU2\n",
								__func__);
		mid_pmu_cxt->shutdown_started = false;

		/* allow s0ix now */
		pm_qos_update_request(mid_pmu_cxt->s3_restrict_qos,
						PM_QOS_DEFAULT_VALUE);
		goto unlock;
	}

	if (_pmu2_wait_not_busy()) {
		pmu_dump_logs();
		BUG();
	}

unlock:
	up(&mid_pmu_cxt->scu_ready_sem);
	return status;
}
EXPORT_SYMBOL(pmu_set_devices_in_d0i0);

static int _pmu_read_status(int type)
{
	u32 temp;
	union pmu_pm_status result;

	temp = readl(&mid_pmu_cxt->pmu_reg->pm_sts);

	/* extract the busy bit */
	result.pmu_status_value = temp;

	if (type == PMU_BUSY_STATUS)
		return result.pmu_status_parts.pmu_busy;
	else if (type == PMU_MODE_ID)
		return result.pmu_status_parts.mode_id;

	return 0;
}

int _pmu2_wait_not_busy(void)
{
	int pmu_busy_retry = PMU2_BUSY_TIMEOUT;

	/* wait 500ms that the latest pmu command finished */
	do {
		if (_pmu_read_status(PMU_BUSY_STATUS) == 0)
			return 0;

		udelay(1);
	} while (--pmu_busy_retry);

	WARN(1, "pmu2 busy!");

	return -EBUSY;
}

static int _pmu2_wait_not_busy_yield(void)
{
	int pmu_busy_retry = PMU2_BUSY_TIMEOUT;

	/* wait max 500ms that the latest pmu command finished */
	do {
		if (_pmu_read_status(PMU_BUSY_STATUS) == 0)
			return 0;

		usleep_range(10, 12);
		pmu_busy_retry -= 11;
	} while (pmu_busy_retry > 0);

	WARN(1, "pmu2 busy!");

	return -EBUSY;
}

static void pmu_write_subsys_config(struct pmu_ss_states *pm_ssc)
{
	/* South complex in Penwell has multiple registers for
	 * PM_SSC, etc.
	 */
	writel(pm_ssc->pmu2_states[0], &mid_pmu_cxt->pmu_reg->pm_ssc[0]);
	writel(pm_ssc->pmu2_states[1], &mid_pmu_cxt->pmu_reg->pm_ssc[1]);
	writel(pm_ssc->pmu2_states[2], &mid_pmu_cxt->pmu_reg->pm_ssc[2]);
	writel(pm_ssc->pmu2_states[3], &mid_pmu_cxt->pmu_reg->pm_ssc[3]);
}

void log_wakeup_irq(void)
{
	unsigned int irr = 0, vector = 0;
	int offset = 0, irq = 0;
	struct irq_desc *desc;
	const char *act_name;

	if ((mid_pmu_cxt->pmu_current_state != SYS_STATE_S3)
	    || !mid_pmu_cxt->suspend_started)
		return;

	for (offset = (FIRST_EXTERNAL_VECTOR/32);
	offset < (NR_VECTORS/32); offset++) {
		irr = apic->read(APIC_IRR + (offset * 0x10));

		while (irr) {
			vector = __ffs(irr);
			irr &= ~(1 << vector);
			irq = __this_cpu_read(
					vector_irq[vector + (offset * 32)]);
			if (irq < 0)
				continue;
			pr_info("wakeup from  IRQ %d\n", irq);

			desc = irq_to_desc(irq);

			if ((desc) && (desc->action)) {
				act_name = desc->action->name;
				pr_info("IRQ %d,action name:%s\n",
					irq,
					(act_name) ? (act_name) : "no action");
			}
		}
	}
	return;
}

static inline int pmu_interrupt_pending(void)
{
	u32 temp;
	union pmu_pm_ics result;

	/* read the pm interrupt status register */
	temp = readl(&mid_pmu_cxt->pmu_reg->pm_ics);
	result.pmu_pm_ics_value = temp;

	/* return the pm interrupt status int pending bit info */
	return result.pmu_pm_ics_parts.int_pend;
}

static inline void pmu_clear_pending_interrupt(void)
{
	u32 temp;

	/* read the pm interrupt status register */
	temp = readl(&mid_pmu_cxt->pmu_reg->pm_ics);

	/* write into the PM_ICS register */
	writel(temp, &mid_pmu_cxt->pmu_reg->pm_ics);
}

void pmu_set_interrupt_enable(void)
{
	u32 temp;
	union pmu_pm_ics result;

	/* read the pm interrupt status register */
	temp = readl(&mid_pmu_cxt->pmu_reg->pm_ics);
	result.pmu_pm_ics_value = temp;

	/* Set the interrupt enable bit */
	result.pmu_pm_ics_parts.int_enable = 1;

	temp = result.pmu_pm_ics_value;

	/* write into the PM_ICS register */
	writel(temp, &mid_pmu_cxt->pmu_reg->pm_ics);
}

void pmu_clear_interrupt_enable(void)
{
	u32 temp;
	union pmu_pm_ics result;

	/* read the pm interrupt status register */
	temp = readl(&mid_pmu_cxt->pmu_reg->pm_ics);
	result.pmu_pm_ics_value = temp;

	/* Clear the interrupt enable bit */
	result.pmu_pm_ics_parts.int_enable = 0;

	temp = result.pmu_pm_ics_value;

	/* write into the PM_ICS register */
	writel(temp, &mid_pmu_cxt->pmu_reg->pm_ics);
}

static inline int pmu_read_interrupt_status(void)
{
	u32 temp;
	union pmu_pm_ics result;

	/* read the pm interrupt status register */
	temp = readl(&mid_pmu_cxt->pmu_reg->pm_ics);

	result.pmu_pm_ics_value = temp;

	if (result.pmu_pm_ics_parts.int_status == 0)
		return PMU_FAILED;

	/* return the pm interrupt status int pending bit info */
	return result.pmu_pm_ics_parts.int_status;
}

/*This function is used for programming the wake capable devices*/
static void pmu_prepare_wake(int s0ix_state)
{

	struct pmu_ss_states cur_pmsss;

	/* setup the wake capable devices */
	if (s0ix_state == MID_S3_STATE) {
		writel(~IGNORE_S3_WKC0, &mid_pmu_cxt->pmu_reg->pm_wkc[0]);
		writel(~IGNORE_S3_WKC1, &mid_pmu_cxt->pmu_reg->pm_wkc[1]);
	}

	if (platform_is(INTEL_ATOM_MFLD) || platform_is(INTEL_ATOM_CLV)) {

		/* Re-program the sub systems state on wakeup as
		 * the current SSS
		 */
		pmu_read_sss(&cur_pmsss);

		writel(cur_pmsss.pmu2_states[0],
				&mid_pmu_cxt->pmu_reg->pm_wssc[0]);
		writel(cur_pmsss.pmu2_states[1],
				&mid_pmu_cxt->pmu_reg->pm_wssc[1]);
		writel(cur_pmsss.pmu2_states[2],
				&mid_pmu_cxt->pmu_reg->pm_wssc[2]);
		writel(cur_pmsss.pmu2_states[3],
				&mid_pmu_cxt->pmu_reg->pm_wssc[3]);
	}
}

int mid_s0ix_enter(int s0ix_state)
{
	int ret = 0;

	if (unlikely(!pmu_ops || !pmu_ops->enter))
		goto ret;

	/* check if we can acquire scu_ready_sem
	 * if we are not able to then do a c6 */
	if (down_trylock(&mid_pmu_cxt->scu_ready_sem))
		goto ret;

	/* If PMU is busy, we'll retry on next C6 */
	if (unlikely(_pmu_read_status(PMU_BUSY_STATUS))) {
		up(&mid_pmu_cxt->scu_ready_sem);
		pr_debug("mid_pmu_cxt->scu_read_sem is up\n");
		goto ret;
	}

	pmu_prepare_wake(s0ix_state);

	/* no need to proceed if schedule pending */
	if (unlikely(need_resched())) {
		pmu_stat_clear();
		/*set wkc to appropriate value suitable for s0ix*/
		writel(mid_pmu_cxt->ss_config->wake_state.wake_enable[0],
		       &mid_pmu_cxt->pmu_reg->pm_wkc[0]);
		writel(mid_pmu_cxt->ss_config->wake_state.wake_enable[1],
		       &mid_pmu_cxt->pmu_reg->pm_wkc[1]);
		up(&mid_pmu_cxt->scu_ready_sem);
		goto ret;
	}

	/* entry function for pmu driver ops */
	if (pmu_ops->enter(s0ix_state))
		ret = s0ix_state;
	else  {
		/*set wkc to appropriate value suitable for s0ix*/
		writel(mid_pmu_cxt->ss_config->wake_state.wake_enable[0],
		       &mid_pmu_cxt->pmu_reg->pm_wkc[0]);
		writel(mid_pmu_cxt->ss_config->wake_state.wake_enable[1],
		       &mid_pmu_cxt->pmu_reg->pm_wkc[1]);
	}

ret:
	return ret;
}

/**
 * pmu_sc_irq - pmu driver interrupt handler
 * Context: interrupt context
 */
static irqreturn_t pmu_sc_irq(int irq, void *ignored)
{
	int status;
	irqreturn_t ret = IRQ_NONE;
	int wake_source;

	/* check if interrup pending bit is set, if not ignore interrupt */
	if (unlikely(!pmu_interrupt_pending())) {
		goto ret_no_clear;
	}

	/* read the interrupt status */
	status = pmu_read_interrupt_status();
	if (unlikely(status == PMU_FAILED))
		dev_dbg(&mid_pmu_cxt->pmu_dev->dev, "Invalid interrupt source\n");

	switch (status) {
	case INVALID_INT:
		goto ret_no_clear;

	case CMD_COMPLETE_INT:
		break;

	case CMD_ERROR_INT:
		mid_pmu_cxt->cmd_error_int++;
		break;

	case SUBSYS_POW_ERR_INT:
	case NO_ACKC6_INT:
	case S0ix_MISS_INT:
		pmu_stat_error(status);
		break;

	case WAKE_RECEIVED_INT:
		wake_source = pmu_get_wake_source();
		trace_printk("wake_from_lss%d\n",
				wake_source);
		pmu_stat_end();
		break;
	case TRIGGERERR:
		pmu_dump_logs();
		WARN(1, "%s: TRIGGERERR caused, but proceeding...\n", __func__);
		break;
	}

	pmu_stat_clear();

	/* clear the interrupt pending bit */
	pmu_clear_pending_interrupt();

	if (pmu_ops->wakeup)
		pmu_ops->wakeup();

	if (platform_is(INTEL_ATOM_MFLD) ||
				platform_is(INTEL_ATOM_CLV)) {
		mid_pmu_cxt->s0ix_entered = 0;
		/* S0ix case release it */
		up(&mid_pmu_cxt->scu_ready_sem);
	}

	ret = IRQ_HANDLED;
ret_no_clear:
	/* clear interrupt enable bit */
	pmu_clear_interrupt_enable();

	return ret;
}

void pmu_set_s0ix_complete(void)
{
	if (pmu_ops->set_s0ix_complete)
		pmu_ops->set_s0ix_complete();
}
EXPORT_SYMBOL(pmu_set_s0ix_complete);

bool pmu_is_s0ix_in_progress(void)
{
	bool state = false;

	if (pmu_initialized && mid_pmu_cxt->s0ix_entered)
		state = true;

	return state;
}
EXPORT_SYMBOL(pmu_is_s0ix_in_progress);

static inline u32 find_index_in_hash(struct pci_dev *pdev, int *found)
{
	u32 h_index;
	int i;

	/* assuming pdev is not null */
	WARN_ON(pdev == NULL);

	/*assuming pdev pionter will not change from platfrom
	 *boot to shutdown*/
	h_index = jhash_1word((u32) (long) pdev,
		 MID_PCI_INDEX_HASH_INITVALUE) & MID_PCI_INDEX_HASH_MASK;

	/* assume not found */
	*found = 0;

	for (i = 0; i < MID_PCI_INDEX_HASH_SIZE; i++) {
		if (likely(mid_pmu_cxt->pci_dev_hash[h_index].pdev == pdev)) {
			*found = 1;
			break;
		}

		/* assume no deletions, hence there shouldn't be any
		 * gaps ie., NULL's */
		if (unlikely(mid_pmu_cxt->pci_dev_hash[h_index].pdev == NULL)) {
			/* found NULL, that means we wont have
			 * it in hash */
			break;
		}

		h_index = (h_index+1)%MID_PCI_INDEX_HASH_SIZE;
	}

	/* Assume hash table wont be full */
	WARN_ON(i == MID_PCI_INDEX_HASH_SIZE);

	return h_index;
}

static bool is_display_subclass(unsigned int sub_class)
{
	/* On MDFLD and CLV, we have display PCI device class 0x30000,
	 * On MRFLD, we have display PCI device class 0x38000
	 */

	if ((sub_class == 0x0 &&
		(platform_is(INTEL_ATOM_MFLD) ||
		platform_is(INTEL_ATOM_CLV))) ||
		(sub_class == 0x80 && (platform_is(INTEL_ATOM_MRFLD)
				|| platform_is(INTEL_ATOM_MOORFLD))))
		return true;

	return false;
}

static int get_pci_to_pmu_index(struct pci_dev *pdev)
{
	int pm, type;
	unsigned int base_class;
	unsigned int sub_class;
	u8 ss;
	int index = PMU_FAILED;
	u32 h_index;
	int found;

	h_index = find_index_in_hash(pdev, &found);

	if (found)
		return (int)mid_pmu_cxt->pci_dev_hash[h_index].index;

	/* if not found, h_index would be where
	 * we can insert this */

	base_class = pdev->class >> 16;
	sub_class  = (pdev->class & SUB_CLASS_MASK) >> 8;
	pm = pci_find_capability(pdev, PCI_CAP_ID_VNDR);

	/* read the logical sub system id & cap if present */
	pci_read_config_byte(pdev, pm + 4, &ss);

	type = ss & LOG_SS_MASK;
	ss = ss & LOG_ID_MASK;

	if ((base_class == PCI_BASE_CLASS_DISPLAY) &&
			is_display_subclass(sub_class))
		index = 1;
	else if ((base_class == PCI_BASE_CLASS_MULTIMEDIA) &&
			(sub_class == ISP_SUB_CLASS))
				index = ISP_POS;
	else if (type) {
		WARN_ON(ss >= MAX_LSS_POSSIBLE);
		index = mid_pmu_cxt->pmu1_max_devs + ss;
	}

	if (index != PMU_FAILED) {
		/* insert into hash table */
		mid_pmu_cxt->pci_dev_hash[h_index].pdev = pdev;

		/* assume index never exceeds 0xff */
		WARN_ON(index > 0xFF);

		mid_pmu_cxt->pci_dev_hash[h_index].index = (u8)index;

		if (index < mid_pmu_cxt->pmu1_max_devs) {
			set_mid_pci_ss_idx(index, 0);
			set_mid_pci_ss_pos(index, (u8)index);
			set_mid_pci_pmu_num(index, PMU_NUM_1);
		} else if (index >= mid_pmu_cxt->pmu1_max_devs &&
			   index < (mid_pmu_cxt->pmu1_max_devs +
						mid_pmu_cxt->pmu2_max_devs)) {
			set_mid_pci_ss_idx(index,
					(u8)(ss / mid_pmu_cxt->ss_per_reg));
			set_mid_pci_ss_pos(index,
					(u8)(ss % mid_pmu_cxt->ss_per_reg));
			set_mid_pci_pmu_num(index, PMU_NUM_2);
		} else {
			index = PMU_FAILED;
		}

		WARN_ON(index == PMU_FAILED);
	}

	return index;
}

static void get_pci_lss_info(struct pci_dev *pdev)
{
	int index, pm;
	unsigned int base_class;
	unsigned int sub_class;
	u8 ss, cap;
	int i;
	base_class = pdev->class >> 16;
	sub_class  = (pdev->class & SUB_CLASS_MASK) >> 8;

	pm = pci_find_capability(pdev, PCI_CAP_ID_VNDR);

	/* read the logical sub system id & cap if present */
	pci_read_config_byte(pdev, pm + 4, &ss);
	pci_read_config_byte(pdev, pm + 5, &cap);

	/* get the index for the copying of ss info */
	index = get_pci_to_pmu_index(pdev);

	if ((index == PMU_FAILED) || (index >= MAX_DEVICES))
		return;

	/* initialize gfx subsystem info */
	if ((base_class == PCI_BASE_CLASS_DISPLAY) &&
			is_display_subclass(sub_class)) {
		set_mid_pci_log_id(index, (u32)index);
		set_mid_pci_cap(index, PM_SUPPORT);
	} else if ((base_class == PCI_BASE_CLASS_MULTIMEDIA) &&
		(sub_class == ISP_SUB_CLASS)) {
			set_mid_pci_log_id(index, (u32)index);
			set_mid_pci_cap(index, PM_SUPPORT);
	} else if (ss && cap) {
		set_mid_pci_log_id(index, (u32)(ss & LOG_ID_MASK));
		set_mid_pci_cap(index, cap);
	}

	for (i = 0; i < PMU_MAX_LSS_SHARE &&
		get_mid_pci_drv(index, i); i++) {
		/* do nothing */
	}

	WARN_ON(i >= PMU_MAX_LSS_SHARE);

	if (i < PMU_MAX_LSS_SHARE) {
		set_mid_pci_drv(index, i, pdev);
		set_mid_pci_power_state(index, i, PCI_D3hot);
	}
}

static void pmu_enumerate(void)
{
	struct pci_dev *pdev = NULL;
	unsigned int base_class;

	for_each_pci_dev(pdev) {
		if ((platform_is(INTEL_ATOM_MRFLD) ||
			platform_is(INTEL_ATOM_MOORFLD)) &&
			pdev->device == MID_MRFL_HDMI_DRV_DEV_ID)
			continue;

		/* find the base class info */
		base_class = pdev->class >> 16;

		if (base_class == PCI_BASE_CLASS_BRIDGE)
			continue;

		get_pci_lss_info(pdev);
	}
}

void pmu_read_sss(struct pmu_ss_states *pm_ssc)
{
	pm_ssc->pmu2_states[0] =
			readl(&mid_pmu_cxt->pmu_reg->pm_sss[0]);
	pm_ssc->pmu2_states[1] =
			readl(&mid_pmu_cxt->pmu_reg->pm_sss[1]);
	pm_ssc->pmu2_states[2] =
			readl(&mid_pmu_cxt->pmu_reg->pm_sss[2]);
	pm_ssc->pmu2_states[3] =
			readl(&mid_pmu_cxt->pmu_reg->pm_sss[3]);
}


/*
 * For all devices in this lss, we check what is the weakest power state
 *
 * Thus we dont power down if another device needs more power
 */

static pci_power_t  pmu_pci_get_weakest_state_for_lss(int lss_index,
				struct pci_dev *pdev, pci_power_t state)
{
	int i;
	pci_power_t weakest = state;

	for (i = 0; i < PMU_MAX_LSS_SHARE; i++) {
		if (get_mid_pci_drv(lss_index, i) == pdev)
			set_mid_pci_power_state(lss_index, i, state);

		if (get_mid_pci_drv(lss_index, i) &&
			(get_mid_pci_power_state(lss_index, i) < weakest))
			weakest = get_mid_pci_power_state(lss_index, i);
	}
	return weakest;
}

int pmu_pci_to_indexes(struct pci_dev *pdev, int *index,
				int *pmu_num, int *ss_idx, int *ss_pos)
{
	int i;

	i = get_pci_to_pmu_index(pdev);
	if (i == PMU_FAILED)
		return PMU_FAILED;

	*index		= i;
	*ss_pos		= get_mid_pci_ss_pos(i);
	*ss_idx		= get_mid_pci_ss_idx(i);
	*pmu_num	= get_mid_pci_pmu_num(i);

	return PMU_SUCCESS;
}

static bool update_nc_device_states(int i, pci_power_t state)
{
	int status = 0;
	int islands = 0;
	int reg;

	/* store the display status */
	if (i == GFX_LSS_INDEX) {
		mid_pmu_cxt->display_off = (state != PCI_D0);
		return true;
	}

	/*Update the Camera status per ISP Driver Suspended/Resumed
	* ISP power islands are also updated accordingly, otherwise Dx state
	* in PMCSR refuses to change.
	*/
	else if (i == ISP_POS) {
		if (platform_is(INTEL_ATOM_MFLD) ||
				 platform_is(INTEL_ATOM_CLV)) {
			islands = APM_ISP_ISLAND | APM_IPH_ISLAND;
			reg = APM_REG_TYPE;
		} else if (platform_is(INTEL_ATOM_MRFLD) ||
				platform_is(INTEL_ATOM_MOORFLD)) {
			islands = TNG_ISP_ISLAND;
			reg = ISP_SS_PM0;
		} else
			return false;
		status = pmu_nc_set_power_state(islands,
			(state != PCI_D0) ?
			OSPM_ISLAND_DOWN : OSPM_ISLAND_UP,
			reg);
		if (status)
			return false;
		mid_pmu_cxt->camera_off = (state != PCI_D0);
		return true;
	}

	return false;
}

void init_nc_device_states(void)
{
#if !IS_ENABLED(CONFIG_VIDEO_ATOMISP)
	mid_pmu_cxt->camera_off = false;
#endif

#ifndef GFX_ENABLE
	/* If Gfx is disabled
	 * assume s0ix is not blocked
	 * from gfx side
	 */
	mid_pmu_cxt->display_off = true;
#endif

	return;
}

/* FIXME::Currently HSI Modem 7060 (BZ# 28529) is having a issue and
* it will not go to Low Power State on CVT. So Standby will not work
* if HSI is enabled.
* We can choose between Standby/HSI based on enable_stadby 1/0.
*/
unsigned int enable_standby __read_mostly;
module_param(enable_standby, uint, 0000);

/* FIXME:: We have issues with S0ix/S3 enabling by default
 * with display lockup, HSIC etc., so have a boot time option
 * to enable S0ix/S3
 */
unsigned int enable_s3 __read_mostly = 1;
int set_enable_s3(const char *val, struct kernel_param *kp)
{
	int rv = param_set_int(val, kp);
	if (rv)
		return rv;

	if (unlikely((!pmu_initialized)))
		return 0;

	if (platform_is(INTEL_ATOM_MRFLD) || platform_is(INTEL_ATOM_MOORFLD)) {
		if (!enable_s3)
			__pm_stay_awake(mid_pmu_cxt->pmu_wake_lock);
		else
			__pm_relax(mid_pmu_cxt->pmu_wake_lock);
	}

	return 0;
}
module_param_call(enable_s3, set_enable_s3, param_get_uint,
				&enable_s3, S_IRUGO | S_IWUSR);

/* FIXME:: We have issues with S0ix/S3 enabling by default
 * with display lockup, HSIC etc., so have a boot time option
 * to enable S0ix/S3
 */
unsigned int enable_s0ix __read_mostly = 1;
int set_enable_s0ix(const char *val, struct kernel_param *kp)
{
	int rv = param_set_int(val, kp);
	if (rv)
		return rv;

	if (unlikely((!pmu_initialized)))
		return 0;

	if (platform_is(INTEL_ATOM_MRFLD) || platform_is(INTEL_ATOM_MOORFLD)) {
		if (!enable_s0ix) {
			mid_pmu_cxt->cstate_ignore =
				~((1 << (CPUIDLE_STATE_MAX-1)) - 1);

			/* Ignore C2, C3, C5 states */
			mid_pmu_cxt->cstate_ignore |= (1 << 1);
			mid_pmu_cxt->cstate_ignore |= (1 << 2);
			mid_pmu_cxt->cstate_ignore |= (1 << 4);

			/* For now ignore C7, C8, C9 states */
			mid_pmu_cxt->cstate_ignore |= (1 << 6);
			mid_pmu_cxt->cstate_ignore |= (1 << 7);
			mid_pmu_cxt->cstate_ignore |= (1 << 8);

			/* Restrict platform Cx state to C6 */
			pm_qos_update_request(mid_pmu_cxt->cstate_qos,
						(CSTATE_EXIT_LATENCY_S0i1-1));
		} else {
			mid_pmu_cxt->cstate_ignore =
				~((1 << (CPUIDLE_STATE_MAX-1)) - 1);

			/* Ignore C2, C3, C5, C8 states */
			mid_pmu_cxt->cstate_ignore |= (1 << 1);
			mid_pmu_cxt->cstate_ignore |= (1 << 2);
			mid_pmu_cxt->cstate_ignore |= (1 << 4);
			mid_pmu_cxt->cstate_ignore |= (1 << 7);

			pm_qos_update_request(mid_pmu_cxt->cstate_qos,
							PM_QOS_DEFAULT_VALUE);
		}
	}

	return 0;
}
module_param_call(enable_s0ix, set_enable_s0ix, param_get_uint,
				&enable_s0ix, S_IRUGO | S_IWUSR);

unsigned int pmu_ignore_lss0 __read_mostly = IGNORE_SSS0;
module_param(pmu_ignore_lss0, uint, S_IRUGO | S_IWUSR);

unsigned int pmu_ignore_lss1 __read_mostly = IGNORE_SSS1;
module_param(pmu_ignore_lss1, uint, S_IRUGO | S_IWUSR);

unsigned int pmu_ignore_lss2 __read_mostly = IGNORE_SSS2;
module_param(pmu_ignore_lss2, uint, S_IRUGO | S_IWUSR);

unsigned int pmu_ignore_lss3 __read_mostly = IGNORE_SSS3;
module_param(pmu_ignore_lss3, uint, S_IRUGO | S_IWUSR);

int pmu_set_emmc_to_d0i0_atomic(void)
{
	u32 pm_cmd_val;
	u32 new_value;
	int sub_sys_pos, sub_sys_index;
	struct pmu_ss_states cur_pmssc;
	int status = 0;

	if (unlikely((!pmu_initialized)))
		return 0;

	/* LSS 01 is index = 0, pos = 1 */
	sub_sys_index	= EMMC0_LSS / mid_pmu_cxt->ss_per_reg;
	sub_sys_pos	= EMMC0_LSS % mid_pmu_cxt->ss_per_reg;

	memset(&cur_pmssc, 0, sizeof(cur_pmssc));

	/*
	 * Give time for possible previous PMU operation to finish in
	 * case where SCU is functioning normally. For SCU crashed case
	 * PMU may stay busy but check if the emmc is accessible.
	 */
	status = _pmu2_wait_not_busy();
	if (status) {
		dev_err(&mid_pmu_cxt->pmu_dev->dev,
			"PMU2 busy, ignoring as emmc might be already d0i0\n");
		status = 0;
	}

	pmu_read_sss(&cur_pmssc);

	/* set D0i0 the LSS bits */
	pm_cmd_val =
		(D0I3_MASK << (sub_sys_pos * BITS_PER_LSS));
	new_value = cur_pmssc.pmu2_states[sub_sys_index] &
						(~pm_cmd_val);
	if (new_value == cur_pmssc.pmu2_states[sub_sys_index])
		goto err;

	status = _pmu2_wait_not_busy();
	if (status)
		goto err;

	cur_pmssc.pmu2_states[sub_sys_index] = new_value;

	/* Request SCU for PM interrupt enabling */
	writel(PMU_PANIC_EMMC_UP_REQ_CMD, mid_pmu_cxt->emergency_emmc_up_addr);

	status = pmu_issue_interactive_command(&cur_pmssc, false, false);

	if (unlikely(status != PMU_SUCCESS)) {
		dev_dbg(&mid_pmu_cxt->pmu_dev->dev,
			 "Failed to Issue a PM command to PMU2\n");
		goto err;

	}

	/*
	 * Wait for interactive command to complete.
	 * If we dont wait, there is a possibility that
	 * the driver may access the device before its
	 * powered on in SCU.
	 *
	 */
	if (_pmu2_wait_not_busy()) {
		pmu_dump_logs();
		BUG();
	}

err:

	return status;
}


#define SAVED_HISTORY_ADDRESS_NUM	10
#define SAVED_HISTORY_NUM		20
#define PCI_MAX_RECORD_NUM		10

struct saved_nc_power_history {
	unsigned long long ts;
	unsigned short pci;
	unsigned short cpu:4;
	unsigned short state_type:8;
	unsigned short real_change:2;
	int reg_type;
	int islands;
	void *address[SAVED_HISTORY_ADDRESS_NUM];
};

static atomic_t saved_nc_power_history_current = ATOMIC_INIT(-1);
static struct saved_nc_power_history all_history[SAVED_HISTORY_NUM];
static struct saved_nc_power_history *get_new_record_history(void)
{
	unsigned int ret =
		atomic_add_return(1, &saved_nc_power_history_current);
	return &all_history[ret%SAVED_HISTORY_NUM];
}

static unsigned short pci_need_record[PCI_MAX_RECORD_NUM] = { 0x08c8, 0x0130, };
static int num_pci_need_record = 2;
module_param_array(pci_need_record, ushort, &num_pci_need_record, 0644);
MODULE_PARM_DESC(pci_need_record,
		"devices need be traced power state transition.");

static bool pci_need_record_power_state(struct pci_dev *pdev)
{
	int i;
	for (i = 0; i < num_pci_need_record; i++)
		if (pdev->device == pci_need_record[i])
			return true;

	return false;
}

static void print_saved_record(struct saved_nc_power_history *record)
{
	int i;
	unsigned long long ts = record->ts;
	unsigned long nanosec_rem = do_div(ts, 1000000000);

	printk(KERN_INFO "----\n");
	printk(KERN_INFO "ts[%5lu.%06lu] cpu[%d] is pci[%04x] reg_type[%d] "
			"state_type[%d] islands[%x] real_change[%d]\n",
		(unsigned long)ts,
		nanosec_rem / 1000,
		record->cpu,
		record->pci,
		record->reg_type,
		record->state_type,
		record->islands,
		record->real_change);
	for (i = 0; i < SAVED_HISTORY_ADDRESS_NUM; i++) {
		printk(KERN_INFO "%pf real_addr[%p]\n",
			record->address[i],
			record->address[i]);
	}
}

#ifdef CONFIG_FRAME_POINTER
size_t backtrace_safe(void **array, size_t max_size)
{
	unsigned long *bp;
	unsigned long *caller;
	unsigned int i;

	get_bp(bp);

	caller = (unsigned long *) *(bp+1);

	for (i = 0; i < max_size; i++)
		array[i] = 0;
	for (i = 0; i < max_size; i++) {
		array[i] = caller;
		bp = (unsigned long *) *bp;
		if (!object_is_on_stack(bp))
			break;
		caller = (unsigned long *) *(bp+1);
	}

	return i + 1;
}
#else
size_t backtrace_safe(void **array, size_t max_size)
{
	memset(array, 0, max_size);
	return 0;
}
#endif

void dump_nc_power_history(void)
{
	int i, start;
	unsigned int total = atomic_read(&saved_nc_power_history_current);

	start = total % SAVED_HISTORY_NUM;
	printk(KERN_INFO "<----current timestamp\n");
	printk(KERN_INFO "start[%d] saved[%d]\n",
			start, total);
	for (i = start; i >= 0; i--)
		print_saved_record(&all_history[i]);
	for (i = SAVED_HISTORY_NUM - 1; i > start; i--)
		print_saved_record(&all_history[i]);
}
EXPORT_SYMBOL(dump_nc_power_history);

static ssize_t debug_read_history(struct file *file, char __user *buffer,
			size_t count, loff_t *pos)
{
	dump_nc_power_history();

	return 0;
}

static ssize_t debug_write_read_history_entry(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	char buf[20] = "0";
	unsigned long len = min(sizeof(buf) - 1, count);
	u32 islands;
	u32 on;
	int ret;

	/*do nothing if platform is nether medfield or clv*/
	if (!platform_is(INTEL_ATOM_MFLD) && !platform_is(INTEL_ATOM_CLV))
		return count;

	if (copy_from_user(buf, buffer, len))
		return -1;

	buf[len] = 0;

	ret = sscanf(buf, "%x%x", &islands, &on);
	if (ret == 2)
		pmu_nc_set_power_state(islands, on, OSPM_REG_TYPE);

	return count;
}

static const struct file_operations proc_debug_operations = {
	.owner	= THIS_MODULE,
	.read	= debug_read_history,
	.write	= debug_write_read_history_entry,
};

static int __init debug_read_history_entry(void)
{
	struct proc_dir_entry *res = NULL;

	res = proc_create("debug_read_history", S_IRUGO | S_IWUSR, NULL,
		&proc_debug_operations);

	if (!res)
		return -ENOMEM;

	return 0;
}
device_initcall(debug_read_history_entry);

/**
 * pmu_nc_set_power_state - Callback function is used by all the devices
 * in north complex for a platform  specific device power on/shutdown.
 * Following assumptions are made by this function
 *
 * Every new request starts from scratch with no assumptions
 * on previous/pending request to Punit.
 * Caller is responsible to retry if request fails.
 * Avoids multiple requests to Punit if target state is
 * already in the expected state.
 * spin_locks guarantee serialized access to these registers
 * and avoid concurrent access from 2d/3d, VED, VEC, ISP & IPH.
 *
 */
int pmu_nc_set_power_state(int islands, int state_type, int reg)
{
	unsigned long flags;
	struct saved_nc_power_history *record = NULL;
	int ret = 0;
	int change;

	spin_lock_irqsave(&mid_pmu_cxt->nc_ready_lock, flags);

	record = get_new_record_history();
	record->cpu = raw_smp_processor_id();
	record->ts = cpu_clock(record->cpu);
	record->islands = islands;
	record->pci = 0;
	record->state_type = state_type;
	backtrace_safe(record->address, SAVED_HISTORY_ADDRESS_NUM);
	record->real_change = 0;
	record->reg_type = reg;

	if (pmu_ops->nc_set_power_state)	{
		ret = pmu_ops->nc_set_power_state(islands, state_type,
								reg, &change);
		if (change) {
			record->real_change = 1;
			record->ts = cpu_clock(record->cpu);
		}
	}

	spin_unlock_irqrestore(&mid_pmu_cxt->nc_ready_lock, flags);
	return ret;
}
EXPORT_SYMBOL(pmu_nc_set_power_state);

/**
 * pmu_nc_get_power_state - Callback function is used to
 * query power status of all the devices in north complex.
 * Following assumptions are made by this function
 *
 * Every new request starts from scratch with no assumptions
 * on previous/pending request to Punit.
 * Caller is responsible to retry if request fails.
 * Avoids multiple requests to Punit if target state is
 * already in the expected state.
 * spin_locks guarantee serialized access to these registers
 * and avoid concurrent access from 2d/3d, VED, VEC, ISP & IPH.
 *
 */
int pmu_nc_get_power_state(int island, int reg_type)
{
	u32 pwr_sts;
	unsigned long flags;
	int i, lss;
	int ret = -EINVAL;

	/*do nothing if platform is nether medfield or clv*/
	if (!platform_is(INTEL_ATOM_MFLD) && !platform_is(INTEL_ATOM_CLV))
		return 0;

	spin_lock_irqsave(&mid_pmu_cxt->nc_ready_lock, flags);

	switch (reg_type) {
	case APM_REG_TYPE:
		pwr_sts = inl(mid_pmu_cxt->apm_base + APM_STS);
		break;
	case OSPM_REG_TYPE:
		pwr_sts = inl(mid_pmu_cxt->ospm_base + OSPM_PM_SSS);
		break;
	default:
		pr_err("%s: invalid argument 'island': %d.\n",
				 __func__, island);
		goto unlock;
	}

	for (i = 0; i < OSPM_MAX_POWER_ISLANDS; i++) {
		lss = island & (0x1 << i);
		if (lss) {
			ret = (pwr_sts >> (2 * i)) & 0x3;
			break;
		}
	}

unlock:
	spin_unlock_irqrestore(&mid_pmu_cxt->nc_ready_lock, flags);
	return ret;
}
EXPORT_SYMBOL(pmu_nc_get_power_state);

void pmu_set_s0i1_disp_vote(bool enable)
{
	unsigned long flags;

	spin_lock_irqsave(&mid_pmu_cxt->nc_ready_lock, flags);

	if (pmu_ops->set_s0i1_disp_vote)
		pmu_ops->set_s0i1_disp_vote(enable);

	spin_unlock_irqrestore(&mid_pmu_cxt->nc_ready_lock, flags);
}
EXPORT_SYMBOL(pmu_set_s0i1_disp_vote);

/*
* update_dev_res - Calulates & Updates the device residency when
* a device state change occurs.
* Computation of respective device residency starts when
* its first state tranisition happens after the pmu driver
* is initialised.
*
*/
void update_dev_res(int index, pci_power_t state)
{
	if (state != PCI_D0) {
		if (mid_pmu_cxt->pmu_dev_res[index].start == 0) {
			mid_pmu_cxt->pmu_dev_res[index].start = cpu_clock(0);
			mid_pmu_cxt->pmu_dev_res[index].d0i3_entry =
				mid_pmu_cxt->pmu_dev_res[index].start;
				mid_pmu_cxt->pmu_dev_res[index].d0i0_acc = 0;
		} else{
			mid_pmu_cxt->pmu_dev_res[index].d0i3_entry =
							cpu_clock(0);
			mid_pmu_cxt->pmu_dev_res[index].d0i0_acc +=
			(mid_pmu_cxt->pmu_dev_res[index].d0i3_entry -
				 mid_pmu_cxt->pmu_dev_res[index].d0i0_entry);
		}
	} else {
		if (mid_pmu_cxt->pmu_dev_res[index].start == 0) {
			mid_pmu_cxt->pmu_dev_res[index].start =
						 cpu_clock(0);
			mid_pmu_cxt->pmu_dev_res[index].d0i0_entry
				= mid_pmu_cxt->pmu_dev_res[index].start;
			mid_pmu_cxt->pmu_dev_res[index].d0i3_acc = 0;
		} else {
			mid_pmu_cxt->pmu_dev_res[index].d0i0_entry =
						 cpu_clock(0);
			mid_pmu_cxt->pmu_dev_res[index].d0i3_acc +=
			(mid_pmu_cxt->pmu_dev_res[index].d0i0_entry -
			mid_pmu_cxt->pmu_dev_res[index].d0i3_entry);
		}
	}
	mid_pmu_cxt->pmu_dev_res[index].state = state;
}

/**
 * pmu_pci_set_power_state - Callback function is used by all the PCI devices
 *			for a platform  specific device power on/shutdown.
 *
 */
int __ref pmu_pci_set_power_state(struct pci_dev *pdev, pci_power_t state)
{
	u32 new_value;
	int i = 0;
	u32 pm_cmd_val, chk_val;
	int sub_sys_pos, sub_sys_index;
	int pmu_num;
	struct pmu_ss_states cur_pmssc;
	int status = 0;
	int retry_times = 0;
	ktime_t calltime, delta, rettime;
	struct saved_nc_power_history *record = NULL;
	bool d3_cold = false;

	/* Ignore callback from devices until we have initialized */
	if (unlikely((!pmu_initialized)))
		return 0;

	might_sleep();

	/* Try to acquire the scu_ready_sem, if not
	 * get blocked, until pmu_sc_irq() releases */
	down(&mid_pmu_cxt->scu_ready_sem);

	/*get LSS index corresponding to pdev, its position in
	 *32 bit register and its register numer*/
	status =
		pmu_pci_to_indexes(pdev, &i, &pmu_num,
				&sub_sys_index,  &sub_sys_pos);

	if (status)
		goto unlock;

	/* Ignore D0i0 requests for LSS that have no drivers */
	if (pmu_power_down_lss_without_driver(i, sub_sys_index,
						sub_sys_pos, state))
		goto unlock;

	if (pci_need_record_power_state(pdev)) {
		record = get_new_record_history();
		record->cpu = raw_smp_processor_id();
		record->ts = cpu_clock(record->cpu);
		record->islands = 0;
		record->reg_type = 0;
		record->pci = pdev->device;
		record->state_type = state;
		backtrace_safe(record->address, SAVED_HISTORY_ADDRESS_NUM);
		record->real_change = 0;
	}

	/* Ignore HDMI HPD driver d0ix on LSS 0 on MRFLD */
	if ((platform_is(INTEL_ATOM_MRFLD) || platform_is(INTEL_ATOM_MOORFLD)) &&
			pdev->device == MID_MRFL_HDMI_DRV_DEV_ID)
			goto unlock;

	/*in case a LSS is assigned to more than one pdev, we need
	  *to find the shallowest state the LSS should be put into*/
	state = pmu_pci_get_weakest_state_for_lss(i, pdev, state);

	/*If the LSS corresponds to northcomplex device, update
	  *the status and return*/
	if (update_nc_device_states(i, state)) {
		if (mid_pmu_cxt->pmu_dev_res[i].state == state)
			goto nc_done;
		else {
			if (i < MAX_DEVICES)
				update_dev_res(i, state);
			goto nc_done;
		}
	}

	/* initialize the current pmssc states */
	memset(&cur_pmssc, 0, sizeof(cur_pmssc));

	status = _pmu2_wait_not_busy();

	if (status)
		goto unlock;

	pmu_read_sss(&cur_pmssc);

	/* Read the pm_cmd val & update the value */
	pm_cmd_val =
		(D0I3_MASK << (sub_sys_pos * BITS_PER_LSS));

	/* First clear the LSS bits */
	new_value = cur_pmssc.pmu2_states[sub_sys_index] &
						(~pm_cmd_val);
	mid_pmu_cxt->os_sss[sub_sys_index] &= ~pm_cmd_val;

	if (state != PCI_D0) {
		pm_cmd_val =
			(pci_to_platform_state(state) <<
				(sub_sys_pos * BITS_PER_LSS));

		new_value |= pm_cmd_val;

		mid_pmu_cxt->os_sss[sub_sys_index] |= pm_cmd_val;
	}

	new_value &= ~mid_pmu_cxt->ignore_lss[sub_sys_index];

	/* nothing to do, so dont do it... */
	if (new_value == cur_pmssc.pmu2_states[sub_sys_index])
		goto unlock;

	cur_pmssc.pmu2_states[sub_sys_index] = new_value;

	/* Check if the state is D3_cold or D3_Hot in TNG platform*/
	if ((platform_is(INTEL_ATOM_MRFLD) || platform_is(INTEL_ATOM_MOORFLD))
		&& (state == PCI_D3cold))
		d3_cold = true;

	/* Issue the pmu command to PMU 2
	 * flag is needed to distinguish between
	 * S0ix vs interactive command in pmu_sc_irq()
	 */
	status = pmu_issue_interactive_command(&cur_pmssc, false, d3_cold);

	if (unlikely(status != PMU_SUCCESS)) {
		dev_dbg(&mid_pmu_cxt->pmu_dev->dev,
			 "Failed to Issue a PM command to PMU2\n");
		goto unlock;
	}

	calltime = ktime_get();
retry:
	/*
	 * Wait for interactive command to complete.
	 * If we dont wait, there is a possibility that
	 * the driver may access the device before its
	 * powered on in SCU.
	 *
	 */
	status = _pmu2_wait_not_busy_yield();
	if (unlikely(status)) {
		rettime = ktime_get();
		delta = ktime_sub(rettime, calltime);
		retry_times++;

		printk(KERN_CRIT "%s: D0ix transition failure: %04x %04X %s %20s:\n",
				__func__,
				pdev->vendor, pdev->device,
				dev_name(&pdev->dev),
				dev_driver_string(&pdev->dev));
		printk(KERN_CRIT "interrupt pending = %d\n",
				pmu_interrupt_pending());
		printk(KERN_CRIT "pmu_busy_status = %d\n",
				_pmu_read_status(PMU_BUSY_STATUS));
		printk(KERN_CRIT "suspend_started = %d\n",
				mid_pmu_cxt->suspend_started);
		printk(KERN_CRIT "shutdown_started = %d\n",
				mid_pmu_cxt->shutdown_started);
		printk(KERN_CRIT "camera_off = %d display_off = %d\n",
				mid_pmu_cxt->camera_off,
				mid_pmu_cxt->display_off);
		printk(KERN_CRIT "s0ix_possible = 0x%x\n",
				mid_pmu_cxt->s0ix_possible);
		printk(KERN_CRIT "s0ix_entered = 0x%x\n",
				mid_pmu_cxt->s0ix_entered);
		printk(KERN_CRIT "pmu_current_state = %d\n",
				mid_pmu_cxt->pmu_current_state);
		printk(KERN_CRIT "PMU is BUSY! retry_times[%d] total_delay[%lli]ms. Retry ...\n",
				retry_times, (long long) ktime_to_ms(delta));
		pmu_dump_logs();

		trigger_all_cpu_backtrace();
		if (retry_times < 60)
			goto retry;
		else
			BUG();
	}
	if (record) {
		record->real_change = 1;
		record->ts = cpu_clock(record->cpu);
	}

	if (pmu_ops->set_power_state_ops)
		pmu_ops->set_power_state_ops(state);

	/* update stats */
	inc_d0ix_stat((i-mid_pmu_cxt->pmu1_max_devs),
				pci_to_platform_state(state));

	/* D0i0 stats */
	{
		int lss = i-mid_pmu_cxt->pmu1_max_devs;
		if (state == PCI_D0) {
			mid_pmu_cxt->d0i0_count[lss]++;
			mid_pmu_cxt->d0i0_prev_time[lss] = cpu_clock(0);
		} else {
			mid_pmu_cxt->d0i0_time[lss] += (cpu_clock(0) -
						mid_pmu_cxt->d0i0_prev_time[lss]);
		}
	}

	/* check if tranisition to requested state has happened */
	pmu_read_sss(&cur_pmssc);

	chk_val = cur_pmssc.pmu2_states[sub_sys_index] &
		(D0I3_MASK << (sub_sys_pos * BITS_PER_LSS));
	new_value &= (D0I3_MASK << (sub_sys_pos * BITS_PER_LSS));

	if ((chk_val == new_value) && (i < MAX_DEVICES))
		update_dev_res(i, state);

	WARN_ON(chk_val != new_value);

nc_done:
#if !IS_ENABLED(CONFIG_VIDEO_ATOMISP)
	/* ATOMISP is always powered up on system-resume path. It needs
	 * to be turned off here if there is no driver to do it. */
	if (!mid_pmu_cxt->camera_off) {
		/* power down isp */
		pmu_nc_set_power_state(APM_ISP_ISLAND | APM_IPH_ISLAND,
				       OSPM_ISLAND_DOWN, APM_REG_TYPE);
		/* power down DPHY */
		new_value = intel_mid_msgbus_read32(0x09, 0x03);
		new_value |= 0x300;
		intel_mid_msgbus_write32(0x09, 0x03, new_value);
		mid_pmu_cxt->camera_off = true;
	}
#endif

unlock:
	up(&mid_pmu_cxt->scu_ready_sem);

	return status;
}

pci_power_t platfrom_pmu_choose_state(int lss)
{
	pci_power_t state = PCI_D3hot;

	if (pmu_ops->pci_choose_state)
		state = pmu_ops->pci_choose_state(lss);

	return state;
}

/* return platform specific deepest states that the device can enter */
pci_power_t pmu_pci_choose_state(struct pci_dev *pdev)
{
	int i;
	int sub_sys_pos, sub_sys_index;
	int status;
	int device_lss;
	int pmu_num;

	pci_power_t state = PCI_D3hot;

	if (pmu_initialized) {
		status =
		pmu_pci_to_indexes(pdev, &i, &pmu_num,
					&sub_sys_index,  &sub_sys_pos);

		if ((status == PMU_SUCCESS) &&
			(pmu_num == PMU_NUM_2)) {

			device_lss =
				(sub_sys_index * mid_pmu_cxt->ss_per_reg) +
								sub_sys_pos;

			state = platfrom_pmu_choose_state(device_lss);
		}
	}

	return state;
}

int pmu_issue_interactive_command(struct pmu_ss_states *pm_ssc, bool ioc,
					bool d3_cold)
{
	u32 command;

	if (_pmu2_wait_not_busy()) {
		dev_err(&mid_pmu_cxt->pmu_dev->dev,
			"SCU BUSY. Operation not permitted\n");
		return PMU_FAILED;
	}

	/* enable interrupts in PMU2 so that interrupts are
	 * propagated when ioc bit for a particular set
	 * command is set
	 */
	/* Enable the hardware interrupt */
	if (ioc)
		pmu_set_interrupt_enable();

	/* Configure the sub systems for pmu2 */
	pmu_write_subsys_config(pm_ssc);

	command = (ioc) ? INTERACTIVE_IOC_VALUE : INTERACTIVE_VALUE;

	 /* Special handling for PCI_D3cold in Tangier */
	if (d3_cold)
		command |= PM_CMD_D3_COLD;

	/* send interactive command to SCU */
	writel(command, &mid_pmu_cxt->pmu_reg->pm_cmd);

	pmu_log_command(command, pm_ssc);

	return 0;
}

/* Reads the status of each driver and updates the LSS values.
 * To be called with scu_ready_sem mutex held, and pmu_config
 * initialized with '0's
 */
static void update_all_lss_states(struct pmu_ss_states *pmu_config)
{
	int i;
	u32 PCIALLDEV_CFG[4] = {0, 0, 0, 0};

	if (platform_is(INTEL_ATOM_MFLD) || platform_is(INTEL_ATOM_CLV)) {
		for (i = 0; i < MAX_DEVICES; i++) {
			int pmu_num = get_mid_pci_pmu_num(i);
			struct pci_dev *pdev = get_mid_pci_drv(i, 0);

			if ((pmu_num == PMU_NUM_2) && pdev) {
				int ss_idx, ss_pos;
				pci_power_t state;

				ss_idx = get_mid_pci_ss_idx(i);
				ss_pos = get_mid_pci_ss_pos(i);
				state = pdev->current_state;
				/* The case of device not probed yet:
				 * Force D0i3 */
				if (state == PCI_UNKNOWN)
					state = pmu_pci_choose_state(pdev);

				/* By default its set to '0' hence
				 * no need to update PCI_D0 state
				 */
				state = pmu_pci_get_weakest_state_for_lss
							(i, pdev, state);

				pmu_config->pmu2_states[ss_idx] |=
				 (pci_to_platform_state(state) <<
					(ss_pos * BITS_PER_LSS));

				PCIALLDEV_CFG[ss_idx] |=
					(D0I3_MASK << (ss_pos * BITS_PER_LSS));
			}
		}
	}

	platform_update_all_lss_states(pmu_config, PCIALLDEV_CFG);
}

static int pmu_init(void)
{
	int status;
	struct pmu_ss_states pmu_config;
	struct pmu_suspend_config *ss_config;
	int ret = 0;
	int retry_times = 0;


	dev_dbg(&mid_pmu_cxt->pmu_dev->dev, "PMU Driver loaded\n");
	spin_lock_init(&mid_pmu_cxt->nc_ready_lock);

	/* enumerate the PCI configuration space */
	pmu_enumerate();

	/* initialize the stats for pmu driver */
	pmu_stats_init();

	/* register platform pmu ops */
	platform_set_pmu_ops();

	/* platform specific initialization */
	if (pmu_ops->init) {
		status = pmu_ops->init();
		if (status) {
			dev_dbg(&mid_pmu_cxt->pmu_dev->dev,
				"pmu_ops->init failed\n");
			goto out_err1;
		}
	}

	/* initialize the state variables here */
	ss_config = devm_kzalloc(&mid_pmu_cxt->pmu_dev->dev,
			sizeof(struct pmu_suspend_config), GFP_KERNEL);
	if (!ss_config) {
		dev_dbg(&mid_pmu_cxt->pmu_dev->dev,
			"Allocation of memory for ss_config has failed\n");
		status = -ENOMEM;
		goto out_err1;
	}

	memset(&pmu_config, 0, sizeof(pmu_config));

	ss_config->ss_state = pmu_config;

	/* initialize for the autonomous S0i3 */
	mid_pmu_cxt->ss_config = ss_config;

	/* setup the wake capable devices */
	mid_pmu_cxt->ss_config->wake_state.wake_enable[0] = WAKE_ENABLE_0;
	mid_pmu_cxt->ss_config->wake_state.wake_enable[1] = WAKE_ENABLE_1;

	/* setup the ignore lss list */
	mid_pmu_cxt->ignore_lss[0] = pmu_ignore_lss0;
	mid_pmu_cxt->ignore_lss[1] = pmu_ignore_lss1;
	mid_pmu_cxt->ignore_lss[2] = pmu_ignore_lss2;
	mid_pmu_cxt->ignore_lss[3] = pmu_ignore_lss3;

	/*set wkc to appropriate value suitable for s0ix*/
	writel(mid_pmu_cxt->ss_config->wake_state.wake_enable[0],
		       &mid_pmu_cxt->pmu_reg->pm_wkc[0]);
	writel(mid_pmu_cxt->ss_config->wake_state.wake_enable[1],
		       &mid_pmu_cxt->pmu_reg->pm_wkc[1]);

	/* Acquire the scu_ready_sem */
	down(&mid_pmu_cxt->scu_ready_sem);

	/* Now we have initialized the driver
	 * Allow drivers to get blocked in
	 * pmu_pci_set_power_state(), until we finish
	 * first interactive command.
	 */

	pmu_initialized = true;

	/* get the current status of each of the driver
	 * and update it in SCU
	 */
	update_all_lss_states(&pmu_config);

	/* In MOFD LSS 16 is used by PTI and LSS 15 is used by DFX
	 * and should not be powered down during init
	 */
	if (platform_is(INTEL_ATOM_MOORFLD)) {
		pmu_config.pmu2_states[0] &=
			~SSMSK(D0I3_MASK, 15);
		pmu_config.pmu2_states[1] &=
			~SSMSK(D0I3_MASK, 0);
	}

	status = pmu_issue_interactive_command(&pmu_config, false,
						false);
	if (status != PMU_SUCCESS) {
		dev_dbg(&mid_pmu_cxt->pmu_dev->dev,\
		 "Failure from pmu mode change to interactive."
		" = %d\n", status);
		status = PMU_FAILED;
		up(&mid_pmu_cxt->scu_ready_sem);
		goto out_err1;
	}

	/*
	 * Wait for interactive command to complete.
	 * If we dont wait, there is a possibility that
	 * the driver may access the device before its
	 * powered on in SCU.
	 *
	 */
retry:
	ret = _pmu2_wait_not_busy();
	if (unlikely(ret)) {
		retry_times++;
		if (retry_times < 60) {
			usleep_range(10, 500);
			goto retry;
		} else {
			pmu_dump_logs();
			BUG();
		}
	}

	/* In cases were gfx is not enabled
	 * this will enable s0ix immediately
	 */
	if (pmu_ops->set_power_state_ops)
		pmu_ops->set_power_state_ops(PCI_D3hot);

	up(&mid_pmu_cxt->scu_ready_sem);

	return PMU_SUCCESS;

out_err1:
	return status;
}

/**
 * mid_pmu_probe - This is the function where most of the PMU driver
 *		   initialization happens.
 */
static int
mid_pmu_probe(struct pci_dev *dev, const struct pci_device_id *pci_id)
{
	int ret;
	struct mrst_pmu_reg __iomem *pmu;
	u32 data;

	u32 dc_islands = (OSPM_DISPLAY_A_ISLAND |
			  OSPM_DISPLAY_B_ISLAND |
			  OSPM_DISPLAY_C_ISLAND |
			  OSPM_MIPI_ISLAND);
	u32 gfx_islands = (APM_VIDEO_DEC_ISLAND |
			   APM_VIDEO_ENC_ISLAND |
			   APM_GL3_CACHE_ISLAND |
			   APM_GRAPHICS_ISLAND);

	mid_pmu_cxt->pmu_wake_lock =
				wakeup_source_register("pmu_wake_lock");

	if (!mid_pmu_cxt->pmu_wake_lock) {
		pr_err("%s: unable to register pmu wake source.\n", __func__);
		return -ENOMEM;
	}

	/* Init the device */
	ret = pci_enable_device(dev);
	if (ret) {
		pr_err("Mid PM device cant be enabled\n");
		goto out_err0;
	}

	/* store the dev */
	mid_pmu_cxt->pmu_dev = dev;
	dev_warn(&dev->dev, "PMU DRIVER Probe called\n");

	ret = pci_request_regions(dev, PMU_DRV_NAME);
	if (ret < 0) {
		pr_err("pci request region has failed\n");
		goto out_err1;
	}

	mid_pmu_cxt->pmu1_max_devs = PMU1_MAX_DEVS;
	mid_pmu_cxt->pmu2_max_devs = PMU2_MAX_DEVS;
	mid_pmu_cxt->ss_per_reg = 16;

	/* Following code is used to map address required for NC PM
	 * which is not needed for all platforms
	 */
	if (platform_is(INTEL_ATOM_MFLD) || platform_is(INTEL_ATOM_CLV)) {
		data = intel_mid_msgbus_read32(OSPM_PUNIT_PORT, OSPM_APMBA);
		mid_pmu_cxt->apm_base = data & 0xffff;

		data = intel_mid_msgbus_read32(OSPM_PUNIT_PORT, OSPM_OSPMBA);
		mid_pmu_cxt->ospm_base = data & 0xffff;
	}

	/* Map the memory of pmu1 PMU reg base */
	pmu = pci_iomap(dev, 0, 0);
	if (pmu == NULL) {
		dev_dbg(&mid_pmu_cxt->pmu_dev->dev,
				"Unable to map the PMU2 address space\n");
		ret = -EIO;
		goto out_err2;
	}

	mid_pmu_cxt->pmu_reg = pmu;

	/* CCU is in same PCI device with PMU, offset is 0x800 */
	ccu_osc_clk_init((void __iomem *)pmu + 0x800);

	/* Map the memory of emergency emmc up */
	mid_pmu_cxt->emergency_emmc_up_addr =
		devm_ioremap_nocache(&mid_pmu_cxt->pmu_dev->dev, PMU_PANIC_EMMC_UP_ADDR, 4);
	if (!mid_pmu_cxt->emergency_emmc_up_addr) {
		dev_dbg(&mid_pmu_cxt->pmu_dev->dev,
		"Unable to map the emergency emmc up address space\n");
		ret = -ENOMEM;
		goto out_err3;
	}

	if (devm_request_irq(&mid_pmu_cxt->pmu_dev->dev, dev->irq, pmu_sc_irq,
			IRQF_NO_SUSPEND, PMU_DRV_NAME, NULL)) {
		dev_dbg(&mid_pmu_cxt->pmu_dev->dev, "Registering isr has failed\n");
		ret = -ENOENT;
		goto out_err3;
	}

	/* call pmu init() for initialization of pmu interface */
	ret = pmu_init();
	if (ret != PMU_SUCCESS) {
		dev_dbg(&mid_pmu_cxt->pmu_dev->dev, "PMU initialization has failed\n");
		goto out_err3;
	}
	dev_warn(&mid_pmu_cxt->pmu_dev->dev, "after pmu initialization\n");

	mid_pmu_cxt->pmu_init_time = cpu_clock(0);

#ifdef CONFIG_PM_DEBUG
	/*
	 * FIXME: Since S3 is not enabled yet we need to take
	 * a wake lock here. Else S3 will be triggered on display
	 * time out and platform will hang
	 */
	if ((platform_is(INTEL_ATOM_MRFLD) || platform_is(INTEL_ATOM_MOORFLD))
								 && !enable_s3)
		__pm_stay_awake(mid_pmu_cxt->pmu_wake_lock);
#endif
#ifdef CONFIG_XEN
	/* Force gfx subsystem to be powered up */
	pmu_nc_set_power_state(dc_islands, OSPM_ISLAND_UP, OSPM_REG_TYPE);
	pmu_nc_set_power_state(gfx_islands, OSPM_ISLAND_UP, APM_REG_TYPE);
#endif
	return 0;

out_err3:
	iounmap(mid_pmu_cxt->pmu_reg);
out_err2:
	pci_release_region(dev, 0);
out_err1:
	pci_disable_device(dev);
out_err0:
	wakeup_source_unregister(mid_pmu_cxt->pmu_wake_lock);

	return ret;
}

static void mid_pmu_remove(struct pci_dev *dev)
{
	/* Freeing up the irq */
	free_irq(dev->irq, &pmu_sc_irq);

	/* If CCU/OSC is inuse, don't remove PMU */
	if (ccu_osc_clk_uninit() < 0) {
		pr_warn("ccu/osc is in using. abort\n");
		return;
	}

	if (pmu_ops->remove)
		pmu_ops->remove();

	pci_iounmap(dev, mid_pmu_cxt->pmu_reg);

	/* disable the current PCI device */
	pci_release_region(dev, 0);
	pci_disable_device(dev);

	wakeup_source_unregister(mid_pmu_cxt->pmu_wake_lock);
}

static void mid_pmu_shutdown(struct pci_dev *dev)
{
	dev_dbg(&mid_pmu_cxt->pmu_dev->dev, "Mid PM mid_pmu_shutdown called\n");

	if (mid_pmu_cxt) {
		/* Restrict platform Cx state to C6 */
		pm_qos_update_request(mid_pmu_cxt->s3_restrict_qos,
					(CSTATE_EXIT_LATENCY_S0i1-1));

		down(&mid_pmu_cxt->scu_ready_sem);
		mid_pmu_cxt->shutdown_started = true;
		up(&mid_pmu_cxt->scu_ready_sem);
	}
}

static struct pci_driver driver = {
	.name = PMU_DRV_NAME,
	.id_table = mid_pm_ids,
	.probe = mid_pmu_probe,
	.remove = mid_pmu_remove,
	.shutdown = mid_pmu_shutdown
};

static int standby_enter(void)
{
	u32 temp = 0;
	int s3_state = mid_state_to_sys_state(MID_S3_STATE);

	if (mid_s0ix_enter(MID_S3_STATE) != MID_S3_STATE) {
		pmu_set_s0ix_complete();
		return -EINVAL;
	}

	/* time stamp for end of s3 entry */
	time_stamp_for_sleep_state_latency(s3_state, false, true);

#ifdef CONFIG_XEN
	HYPERVISOR_mwait_op(mid_pmu_cxt->s3_hint, 1, (void *) &temp, 1);
#else
	__monitor((void *) &temp, 0, 0);
	smp_mb();
	__mwait(mid_pmu_cxt->s3_hint, 1);
#endif
	/* time stamp for start of s3 exit */
	time_stamp_for_sleep_state_latency(s3_state, true, false);

	pmu_set_s0ix_complete();

	/*set wkc to appropriate value suitable for s0ix*/
	writel(mid_pmu_cxt->ss_config->wake_state.wake_enable[0],
		       &mid_pmu_cxt->pmu_reg->pm_wkc[0]);
	writel(mid_pmu_cxt->ss_config->wake_state.wake_enable[1],
		       &mid_pmu_cxt->pmu_reg->pm_wkc[1]);

	mid_pmu_cxt->camera_off = 0;
	mid_pmu_cxt->display_off = 0;

	if (platform_is(INTEL_ATOM_MRFLD) || platform_is(INTEL_ATOM_MOORFLD))
		up(&mid_pmu_cxt->scu_ready_sem);

	return 0;
}

static int mid_suspend_begin(suspend_state_t state)
{
	mid_pmu_cxt->suspend_started = true;
	pmu_s3_stats_update(1);

	/* Restrict to C6 during suspend */
	pm_qos_update_request(mid_pmu_cxt->s3_restrict_qos,
					(CSTATE_EXIT_LATENCY_S0i1-1));
	return 0;
}

static int mid_suspend_valid(suspend_state_t state)
{
	int ret = 0;

	switch (state) {
	case PM_SUSPEND_ON:
	case PM_SUSPEND_MEM:
		/* check if we are ready */
		if (likely(pmu_initialized))
			ret = 1;
	break;
	}

	return ret;
}

static int mid_suspend_prepare(void)
{
	return 0;
}

static int mid_suspend_prepare_late(void)
{
	return 0;
}

static int mid_suspend_enter(suspend_state_t state)
{
	int ret;

	if (state != PM_SUSPEND_MEM)
		return -EINVAL;

	/*FIXME:: On MOFD target mask is incorrect, hence avoid check for MOFD*/
	if (!platform_is(INTEL_ATOM_MOORFLD)) {
		/* one last check before entering standby */
		if (pmu_ops->check_nc_sc_status) {
			if (!(pmu_ops->check_nc_sc_status())) {
				trace_printk("Device d0ix status check failed! Aborting Standby entry!\n");
				WARN_ON(1);
			}
		}
	}

	trace_printk("s3_entry\n");
	ret = standby_enter();
	trace_printk("s3_exit %d\n", ret);
	if (ret != 0)
		dev_dbg(&mid_pmu_cxt->pmu_dev->dev,
				"Failed to enter S3 status: %d\n", ret);

	return ret;
}

static void mid_suspend_end(void)
{
	/* allow s0ix now */
	pm_qos_update_request(mid_pmu_cxt->s3_restrict_qos,
					PM_QOS_DEFAULT_VALUE);

	pmu_s3_stats_update(0);
	mid_pmu_cxt->suspend_started = false;
}

static const struct platform_suspend_ops mid_suspend_ops = {
	.begin = mid_suspend_begin,
	.valid = mid_suspend_valid,
	.prepare = mid_suspend_prepare,
	.prepare_late = mid_suspend_prepare_late,
	.enter = mid_suspend_enter,
	.end = mid_suspend_end,
};

/**
 * mid_pci_register_init - register the PMU driver as PCI device
 */
static int __init mid_pci_register_init(void)
{
	int ret;

	mid_pmu_cxt = kzalloc(sizeof(struct mid_pmu_dev), GFP_KERNEL);

	if (mid_pmu_cxt == NULL)
		return -ENOMEM;

	mid_pmu_cxt->s3_restrict_qos =
		kzalloc(sizeof(struct pm_qos_request), GFP_KERNEL);
	if (mid_pmu_cxt->s3_restrict_qos) {
		pm_qos_add_request(mid_pmu_cxt->s3_restrict_qos,
			 PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
	} else {
		return -ENOMEM;
	}

	init_nc_device_states();

	mid_pmu_cxt->nc_restrict_qos =
		kzalloc(sizeof(struct pm_qos_request), GFP_KERNEL);
	if (mid_pmu_cxt->nc_restrict_qos == NULL)
		return -ENOMEM;

	/* initialize the semaphores */
	sema_init(&mid_pmu_cxt->scu_ready_sem, 1);

	/* registering PCI device */
	ret = pci_register_driver(&driver);
	suspend_set_ops(&mid_suspend_ops);

	return ret;
}
fs_initcall(mid_pci_register_init);

void pmu_power_off(void)
{
	/* wait till SCU is ready */
	if (!_pmu2_wait_not_busy())
		writel(S5_VALUE, &mid_pmu_cxt->pmu_reg->pm_cmd);

	else {
		/* If PM_BUSY bit is not clear issue COLD_OFF */
		WARN(1, "%s: pmu busy bit not cleared.\n", __func__);
		rpmsg_send_generic_simple_command(IPCMSG_COLD_RESET, 1);
	}
}

static void __exit mid_pci_cleanup(void)
{
	if (mid_pmu_cxt) {
		if (mid_pmu_cxt->s3_restrict_qos)
			pm_qos_remove_request(mid_pmu_cxt->s3_restrict_qos);

		if (pm_qos_request_active(mid_pmu_cxt->nc_restrict_qos))
			pm_qos_remove_request(mid_pmu_cxt->nc_restrict_qos);
	}

	suspend_set_ops(NULL);

	/* registering PCI device */
	pci_unregister_driver(&driver);

	if (mid_pmu_cxt)
		pmu_stats_finish();

	kfree(mid_pmu_cxt);
}
module_exit(mid_pci_cleanup);
