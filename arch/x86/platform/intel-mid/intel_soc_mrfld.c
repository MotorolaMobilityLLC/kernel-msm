/*
 * intel_soc_mrfld.c - This driver provides utility api's for merrifield
 * platform
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

u8 __iomem *s0ix_counters;

int s0ix_counter_reg_map[] = {0x0, 0xAC, 0xB0, 0xA8, 0xA4, 0xC0,
	0xBC, 0xB8, 0xB4, 0x8C, 0x90, 0x98};

int s0ix_residency_reg_map[] = {0x0, 0xD8, 0xE0, 0xD0, 0xC8, 0x100,
	0xF8, 0xF0, 0xE8, 0x68, 0x70, 0x80};

/* list of north complex devices */
char *mrfl_nc_devices[] = {
	"GFXSLC",
	"GSDKCK",
	"GRSCD",
	"VED",
	"VEC",
	"DPA",
	"DPB",
	"DPC",
	"VSP",
	"ISP",
	"MIO",
	"HDMIO",
	"GFXSLCLDO"
};

int mrfl_no_of_nc_devices =
	sizeof(mrfl_nc_devices)/sizeof(mrfl_nc_devices[0]);

static int mrfld_pmu_init(void)
{
	mid_pmu_cxt->s3_hint = MRFLD_S3_HINT;


	/* Put all unused LSS in D0i3 */
	mid_pmu_cxt->os_sss[0] = (SSMSK(D0I3_MASK, PMU_RESERVED_LSS_03)	|
				SSMSK(D0I3_MASK, PMU_HSI_LSS_05)	|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_07)	|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_11)	|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_12)	|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_13)	|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_14)	|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_15));

	/* Put LSS8 as unused on PRh */
	if (INTEL_MID_BOARD(3, PHONE, MRFL, BB, PRO, PRHA)) {
		mid_pmu_cxt->os_sss[0] |= \
			SSMSK(D0I3_MASK, PMU_USB_MPH_LSS_08);
	}

	mid_pmu_cxt->os_sss[1] = (SSMSK(D0I3_MASK, PMU_RESERVED_LSS_16-16)|
				SSMSK(D0I3_MASK, PMU_SSP3_LSS_17-16)|
				SSMSK(D0I3_MASK, PMU_SSP6_LSS_19-16)|
				SSMSK(D0I3_MASK, PMU_USB_OTG_LSS_28-16)|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_29-16)|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_30-16));

	/* Excpet for LSS 35 keep all in D0i3 */
	mid_pmu_cxt->os_sss[2] = 0xFFFFFFFF;
	mid_pmu_cxt->os_sss[3] = 0xFFFFFFFF;

	mid_pmu_cxt->os_sss[2] &= ~SSMSK(D0I3_MASK, PMU_SSP4_LSS_35-32);

	s0ix_counters = devm_ioremap_nocache(&mid_pmu_cxt->pmu_dev->dev,
		S0IX_COUNTERS_BASE, S0IX_COUNTERS_SIZE);
	if (!s0ix_counters)
		goto err;

	/* Keep PSH LSS's 00, 33, 34 in D0i0 if PM is disabled */
	if (!enable_s0ix && !enable_s3) {
		mid_pmu_cxt->os_sss[2] &=
				~SSMSK(D0I3_MASK, PMU_I2C8_LSS_33-32);
		mid_pmu_cxt->os_sss[2] &=
				~SSMSK(D0I3_MASK, PMU_I2C9_LSS_34-32);
	} else {
		mid_pmu_cxt->os_sss[0] |= SSMSK(D0I3_MASK, PMU_PSH_LSS_00);
	}

	/* Disable the Interrupt Enable bit in PM ICS register */
	pmu_clear_interrupt_enable();

	return PMU_SUCCESS;

err:
	pr_err("Cannot map memory to read S0ix residency and count\n");
	return PMU_FAILED;
}

/* This function checks north complex (NC) and
 * south complex (SC) device status in MRFLD.
 * returns TRUE if all NC and SC devices are in d0i3
 * else FALSE.
 */
static bool mrfld_nc_sc_status_check(void)
{
	int i;
	u32 val, nc_pwr_sts;
	struct pmu_ss_states cur_pmsss;
	bool nc_status, sc_status;

	/* assuming nc and sc are good */
	nc_status = true;
	sc_status = true;

	/* Check south complex device status */
	pmu_read_sss(&cur_pmsss);

	if (!(((cur_pmsss.pmu2_states[0] & S0IX_TARGET_SSS0_MASK) ==
					 S0IX_TARGET_SSS0) &&
		((cur_pmsss.pmu2_states[1] & S0IX_TARGET_SSS1_MASK) ==
					 S0IX_TARGET_SSS1) &&
		((cur_pmsss.pmu2_states[2] & S0IX_TARGET_SSS2_MASK) ==
					 S0IX_TARGET_SSS2) &&
		((cur_pmsss.pmu2_states[3] & S0IX_TARGET_SSS3_MASK) ==
					 (S0IX_TARGET_SSS3)))) {
		sc_status = false;
		pr_warn("SC device/devices not in d0i3!!\n");
		for (i = 0; i < 4; i++)
			pr_warn("pmu2_states[%d] = %08lX\n", i,
					cur_pmsss.pmu2_states[i]);
	}

	if (sc_status) {
		/* Check north complex status */
		nc_pwr_sts =
			 intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS);
		/* loop through the status to see if any of nc power island
		 * is not in D0i3 state
		 */
		for (i = 0; i < mrfl_no_of_nc_devices; i++) {
			val = nc_pwr_sts & 3;
			if (val != 3) {
				nc_status = false;
				pr_warn("NC device (%s) is not in d0i3!!\n",
							mrfl_nc_devices[i]);
				pr_warn("nc_pm_sss = %08X\n", nc_pwr_sts);
				break;
			}
			nc_pwr_sts >>= BITS_PER_LSS;
		}
	}

	return nc_status & sc_status;
}

/* FIXME: Need to start the counter only if debug is
 * needed. This will save SCU cycles if debug is
 * disabled
 */
static int __init start_scu_s0ix_res_counters(void)
{
	int ret;

	ret = intel_scu_ipc_simple_command(START_RES_COUNTER, 0);
	if (ret) {
		pr_err("IPC command to start res counter failed\n");
		BUG();
		return ret;
	}
	return 0;
}
late_initcall(start_scu_s0ix_res_counters);

void platform_update_all_lss_states(struct pmu_ss_states *pmu_config,
					int *PCIALLDEV_CFG)
{
	/* Overwrite the pmu_config values that we get */
	pmu_config->pmu2_states[0] =
				(SSMSK(D0I3_MASK, PMU_RESERVED_LSS_03)	|
				SSMSK(D0I3_MASK, PMU_HSI_LSS_05)	|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_07)	|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_11)	|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_12)	|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_13)	|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_14)	|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_15));

	/* Put LSS8 as unused on PRh */
	if (INTEL_MID_BOARD(3, PHONE, MRFL, BB, PRO, PRHA)) {
		pmu_config->pmu2_states[0] |= \
			SSMSK(D0I3_MASK, PMU_USB_MPH_LSS_08);
	}

	pmu_config->pmu2_states[1] =
				(SSMSK(D0I3_MASK, PMU_RESERVED_LSS_16-16)|
				SSMSK(D0I3_MASK, PMU_SSP3_LSS_17-16)|
				SSMSK(D0I3_MASK, PMU_USB_OTG_LSS_28-16)	|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_29-16)|
				SSMSK(D0I3_MASK, PMU_RESERVED_LSS_30-16));
	if (platform_is(INTEL_ATOM_MRFLD))
		pmu_config->pmu2_states[1] |=
				SSMSK(D0I3_MASK, PMU_SSP6_LSS_19-16);

	pmu_config->pmu2_states[0] &= ~IGNORE_SSS0;
	pmu_config->pmu2_states[1] &= ~IGNORE_SSS1;
	pmu_config->pmu2_states[2] = ~IGNORE_SSS2;
	pmu_config->pmu2_states[3] = ~IGNORE_SSS3;

	/* Excpet for LSS 35 keep all in D0i3 */
	pmu_config->pmu2_states[2] &= ~SSMSK(D0I3_MASK, PMU_SSP4_LSS_35-32);

	/* Keep PSH LSS's 00, 33, 34 in D0i0 if PM is disabled */
	if (!enable_s0ix && !enable_s3) {
		pmu_config->pmu2_states[2] &=
				~SSMSK(D0I3_MASK, PMU_I2C8_LSS_33-32);
		pmu_config->pmu2_states[2] &=
				~SSMSK(D0I3_MASK, PMU_I2C9_LSS_34-32);
	} else {
		pmu_config->pmu2_states[0] |= SSMSK(D0I3_MASK, PMU_PSH_LSS_00);
	}
}

/*
 * In MDFLD and CLV this callback is used to issue
 * PM_CMD which is not required in MRFLD
 */
static bool mrfld_pmu_enter(int s0ix_state)
{
	mid_pmu_cxt->s0ix_entered = s0ix_state;
	if (s0ix_state == MID_S3_STATE) {
		mid_pmu_cxt->pmu_current_state = SYS_STATE_S3;
		pmu_set_interrupt_enable();
	}

	return true;
}

/**
 *      platform_set_pmu_ops - Set the global pmu method table.
 *      @ops:   Pointer to ops structure.
 */
void platform_set_pmu_ops(void)
{
	pmu_ops = &mrfld_pmu_ops;
}

/*
 * As of now since there is no sequential mapping between
 * LSS abd WKS bits the following two calls are dummy
 */

bool mid_pmu_is_wake_source(u32 lss_number)
{
	return false;
}

/* return the last wake source id, and make statistics about wake sources */
int pmu_get_wake_source(void)
{
	return INVALID_WAKE_SRC;
}


int set_extended_cstate_mode(const char *val, struct kernel_param *kp)
{
	return 0;
}

int get_extended_cstate_mode(char *buffer, struct kernel_param *kp)
{
	const char *default_string = "not supported";
	strcpy(buffer, default_string);
	return strlen(default_string);
}

static int wait_for_nc_pmcmd_complete(int verify_mask,
				int status_mask, int state_type , int reg)
{
	int pwr_sts;
	int count = 0;

	while (true) {
		pwr_sts = intel_mid_msgbus_read32(PUNIT_PORT, reg);
		pwr_sts = pwr_sts >> SSS_SHIFT;
		if (state_type == OSPM_ISLAND_DOWN ||
					state_type == OSPM_ISLAND_SR) {
			if ((pwr_sts & status_mask) ==
						(verify_mask & status_mask))
				break;
			else
				udelay(10);
		} else if (state_type == OSPM_ISLAND_UP) {
			if ((~pwr_sts & status_mask)  ==
						(~verify_mask & status_mask))
				break;
			else
				udelay(10);
		}

		count++;
		if (count > 500000) {
			pr_err("PUnit Timeout, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
				verify_mask, status_mask, state_type, reg,
				intel_mid_msgbus_read32(PUNIT_PORT, reg));
			panic("punit timeout");
		}
	}
	return 0;
}

static int mrfld_nc_set_power_state(int islands, int state_type,
							int reg, int *change)
{
	u32 pwr_sts = 0;
	u32 pwr_mask = 0;
	int i, lss, mask;
	int ret = 0;
	int status_mask = 0;

	*change = 0;
	pwr_sts = intel_mid_msgbus_read32(PUNIT_PORT, reg);
	pwr_mask = pwr_sts;

	for (i = 0; i < OSPM_MAX_POWER_ISLANDS; i++) {
		lss = islands & (0x1 << i);
		if (lss) {
			mask = D0I3_MASK << (BITS_PER_LSS * i);
			status_mask = status_mask | mask;
			if (state_type == OSPM_ISLAND_DOWN) {
				pwr_mask |= mask;
				mid_pmu_cxt->nc_d0i0_time[i] +=
					(cpu_clock(0) - mid_pmu_cxt->nc_d0i0_prev_time[i]);
			} else if (state_type == OSPM_ISLAND_UP) {
				mid_pmu_cxt->nc_d0i0_count[i]++;
				pwr_mask &= ~mask;
				mid_pmu_cxt->nc_d0i0_prev_time[i] = cpu_clock(0);
			/* Soft reset case */
			} else if (state_type == OSPM_ISLAND_SR) {
				pwr_mask &= ~mask;
				mask = SR_MASK << (BITS_PER_LSS * i);
				pwr_mask |= mask;
			}
		}
	}

	if (pwr_mask != pwr_sts) {
		intel_mid_msgbus_write32(PUNIT_PORT, reg, pwr_mask);
		ret = wait_for_nc_pmcmd_complete(pwr_mask,
					status_mask, state_type, reg);
		if (!ret)
			*change = 1;
		if (nc_report_power_state)
			nc_report_power_state(pwr_mask, reg);
	}

	return ret;
}

/* Provide s0i1-display vote to display driver. We add this operation in pmu
 * driver to sync operation with display island power on/off with touching
 * the same register.
 * register also defined:  linux/modules/intel_media/display/tng/drv/pmu_tng.h */
#define DSP_SS_PM 0x36
#define PUNIT_DSPSSPM_ENABLE_S0i1_DISPLAY     (1<<8)
static void set_s0i1_disp_vote(bool enable)
{
	u32 dsp_ss_pm_val = intel_mid_msgbus_read32(PUNIT_PORT, DSP_SS_PM);

	if (enable)
		dsp_ss_pm_val |= PUNIT_DSPSSPM_ENABLE_S0i1_DISPLAY;
	else
		dsp_ss_pm_val &= ~PUNIT_DSPSSPM_ENABLE_S0i1_DISPLAY;

	intel_mid_msgbus_write32(PUNIT_PORT, DSP_SS_PM, dsp_ss_pm_val);
}

void s0ix_complete(void)
{
	if (mid_pmu_cxt->s0ix_entered) {
		log_wakeup_irq();

		if (mid_pmu_cxt->s0ix_entered == SYS_STATE_S3)
			pmu_clear_interrupt_enable();

		mid_pmu_cxt->pmu_current_state	=
		mid_pmu_cxt->s0ix_entered	= 0;
	}
}

bool could_do_s0ix(void)
{
	bool ret = false;
	if (unlikely(!pmu_initialized))
		goto ret;

	/* dont do s0ix if suspend in progress */
	if (unlikely(mid_pmu_cxt->suspend_started))
		goto ret;

	/* dont do s0ix if shutdown in progress */
	if (unlikely(mid_pmu_cxt->shutdown_started))
		goto ret;

	if (nc_device_state())
		goto ret;

	ret = true;
ret:
	return ret;
}
EXPORT_SYMBOL(could_do_s0ix);


struct platform_pmu_ops mrfld_pmu_ops = {
	.init	 = mrfld_pmu_init,
	.enter	 = mrfld_pmu_enter,
	.set_s0ix_complete = s0ix_complete,
	.set_s0i1_disp_vote = set_s0i1_disp_vote,
	.nc_set_power_state = mrfld_nc_set_power_state,
	.check_nc_sc_status = mrfld_nc_sc_status_check,
};
