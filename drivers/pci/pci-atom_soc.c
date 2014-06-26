/*
 * pci-atom_soc.c - register Intel MID PCI plaform ops
 *
 * Copyright (c) 2013, Intel Corporation.
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
 */

#include <linux/init.h>
#include <linux/intel_mid_pm.h>
#include <linux/kernel.h>

#include "pci.h"

static bool mid_pci_power_manageable(struct pci_dev *dev)
{
	return true;
}

static pci_power_t mid_pci_choose_state(struct pci_dev *pdev)
{
	return PCI_D3hot;
}

static int mid_pci_sleep_wake(struct pci_dev *dev, bool enable)
{
	return 0;
}

static int mid_pci_run_wake(struct pci_dev *dev, bool enable)
{
	return 0;
}

static struct pci_platform_pm_ops mid_pci_platform_pm = {
	.is_manageable = mid_pci_power_manageable,
	.choose_state = mid_pci_choose_state,
	.sleep_wake = mid_pci_sleep_wake,
	.run_wake = mid_pci_run_wake,
	.set_state = pmu_pci_set_power_state,
	.choose_state = pmu_pci_choose_state,
};

/**
 * mid_pci_init - It registers callback function for all the PCI devices
 * for platform specific device power on/shutdown acticities.
 */
static int __init mid_pci_init(void)
{
	if (boot_cpu_data.x86 != 6)
		return 0;

	/*
	 * n.b. this model check does not uniquely identify the platform,
	 * and additional checks are necessary inside the pmu driver
	 */
	switch (boot_cpu_data.x86_model) {
	case INTEL_ATOM_MFLD:
	case INTEL_ATOM_CLV:
	case INTEL_ATOM_MRFLD:
	case INTEL_ATOM_MOORFLD:
		pci_set_platform_pm(&mid_pci_platform_pm);
		break;
	}

	return 0;
}
arch_initcall(mid_pci_init);
