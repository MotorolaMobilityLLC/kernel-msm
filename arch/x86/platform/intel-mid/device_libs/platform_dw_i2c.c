/*
 * platform_dw_i2c.c: I2C platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/lnw_gpio.h>
#include <linux/gpio.h>
#include <asm/intel-mid.h>
#include <linux/intel_mid_pm.h>

struct i2c_pin_cfg {
	int scl_gpio;
	int scl_alt;
	int sda_gpio;
	int sda_alt;
};

enum {
	BOARD_NONE = 0,
	BOARD_VTB,
	BOARD_SALTBAY,
};

static struct i2c_pin_cfg dw_i2c_pin_cfgs[][10] = {
	[BOARD_NONE] =  {},
	[BOARD_VTB] =  {
		[1] = {27, 1, 26, 1},
	},
	[BOARD_SALTBAY] =  {
		[1] = {19, 1, 20, 1},
	},
};

int intel_mid_dw_i2c_abort(int busnum)
{
	int i;
	int ret = -EBUSY;
	struct i2c_pin_cfg *pins = &dw_i2c_pin_cfgs[BOARD_NONE][busnum];

	switch (intel_mid_identify_cpu()) {
	case INTEL_MID_CPU_CHIP_CLOVERVIEW:
		pins = &dw_i2c_pin_cfgs[BOARD_VTB][busnum];
		break;
	case INTEL_MID_CPU_CHIP_TANGIER:
		pins = &dw_i2c_pin_cfgs[BOARD_SALTBAY][busnum];
		break;
	default:
		break;
	}

	if (!pins->scl_gpio || !pins->sda_gpio) {
		pr_err("i2c-%d: recovery ignore\n", busnum);
		return 0;
	}
	pr_err("i2c-%d: try to abort xfer, scl_gpio %d, sda_gpio %d\n",
			busnum, pins->scl_gpio, pins->sda_gpio);
	gpio_request(pins->scl_gpio, "scl");
	gpio_request(pins->sda_gpio, "sda");
	lnw_gpio_set_alt(pins->scl_gpio, LNW_GPIO);
	lnw_gpio_set_alt(pins->sda_gpio, LNW_GPIO);
	gpio_direction_input(pins->scl_gpio);
	gpio_direction_input(pins->sda_gpio);
	usleep_range(10, 10);
	pr_err("i2c-%d: scl_gpio val %d, sda_gpio val %d\n",
			busnum,
			gpio_get_value(pins->scl_gpio) ? 1 : 0,
			gpio_get_value(pins->sda_gpio) ? 1 : 0);
	gpio_direction_output(pins->scl_gpio, 1);
	pr_err("i2c-%d: toggle begin\n", busnum);
	for (i = 0; i < 9; i++) {
		if (gpio_get_value(pins->sda_gpio)) {
			if (gpio_get_value(pins->scl_gpio)) {
				pr_err("i2c-%d: recovery success\n", busnum);
				break;
			} else {
				gpio_direction_output(pins->scl_gpio, 0);
				pr_err("i2c-%d: scl_gpio val 0, sda_gpio val 1\n",
					busnum);
			}
		}
		gpio_set_value(pins->scl_gpio, 0);
		usleep_range(10, 20);
		gpio_set_value(pins->scl_gpio, 1);
		usleep_range(10, 20);
		pr_err("i2c-%d: toggle SCL loop %d\n", busnum, i);
	}
	pr_err("i2c-%d: toggle end\n", busnum);
	gpio_direction_output(pins->scl_gpio, 1);
	gpio_direction_output(pins->sda_gpio, 0);
	gpio_set_value(pins->scl_gpio, 0);
	usleep_range(10, 20);
	gpio_set_value(pins->scl_gpio, 1);
	usleep_range(10, 20);
	gpio_set_value(pins->sda_gpio, 0);
	lnw_gpio_set_alt(pins->scl_gpio, pins->scl_alt);
	lnw_gpio_set_alt(pins->sda_gpio, pins->sda_alt);
	usleep_range(10, 10);
	gpio_free(pins->scl_gpio);
	gpio_free(pins->sda_gpio);

	return ret;
}
EXPORT_SYMBOL(intel_mid_dw_i2c_abort);

/* synchronization for sharing the I2C controller */
#define PUNIT_PORT	0x04
static DEFINE_SPINLOCK(msgbus_lock);
static struct pci_dev *pci_root;
#include <linux/pm_qos.h>
static struct pm_qos_request pm_qos;
int qos;

static int intel_mid_msgbus_init(void)
{
	pci_root = pci_get_bus_and_slot(0, PCI_DEVFN(0, 0));
	if (!pci_root) {
		pr_err("msgbus PCI handle NULL for I2C sharing\n");
		return -ENODEV;
	}

	pm_qos_add_request(&pm_qos, PM_QOS_CPU_DMA_LATENCY,
			PM_QOS_DEFAULT_VALUE);

	return 0;
}
fs_initcall(intel_mid_msgbus_init);

#define PUNIT_DOORBELL_OPCODE	(0xE0)
#define PUNIT_DOORBELL_REG	(0x0)
#define PUNIT_SEMAPHORE		(platform_is(INTEL_ATOM_BYT) ? 0x7 : 0x10E)

#define GET_SEM() (intel_mid_msgbus_read32(PUNIT_PORT, PUNIT_SEMAPHORE) & 0x1)

static void reset_semaphore(void)
{
	u32 data;

	data = intel_mid_msgbus_read32(PUNIT_PORT, PUNIT_SEMAPHORE);
	smp_mb();
	data = data & 0xfffffffe;
	intel_mid_msgbus_write32(PUNIT_PORT, PUNIT_SEMAPHORE, data);
	smp_mb();

	pm_qos_update_request(&pm_qos, PM_QOS_DEFAULT_VALUE);

}

int intel_mid_dw_i2c_acquire_ownership(void)
{
	u32 ret = 0;
	u32 data = 0; /* data sent to PUNIT */
	unsigned long irq_flags;
	u32 cmd;
	u32 cmdext;
	int timeout = 100;

	pm_qos_update_request(&pm_qos, CSTATE_EXIT_LATENCY_C1 - 1);

	/* host driver writes 0x2 to side band register 0x7 */
	intel_mid_msgbus_write32(PUNIT_PORT, PUNIT_SEMAPHORE, 0x2);
	smp_mb();

	/* host driver sends 0xE0 opcode to PUNIT and writes 0 register */
	cmd = (PUNIT_DOORBELL_OPCODE << 24) | (PUNIT_PORT << 16) |
	((PUNIT_DOORBELL_REG & 0xFF) << 8) | PCI_ROOT_MSGBUS_DWORD_ENABLE;
	cmdext = PUNIT_DOORBELL_REG & 0xffffff00;

	spin_lock_irqsave(&msgbus_lock, irq_flags);
	pci_write_config_dword(pci_root, PCI_ROOT_MSGBUS_DATA_REG, data);

	if (cmdext) {
		/* This resets to 0 automatically, no need to write 0 */
		pci_write_config_dword(pci_root, PCI_ROOT_MSGBUS_CTRL_EXT_REG,
				cmdext);
	}

	pci_write_config_dword(pci_root, PCI_ROOT_MSGBUS_CTRL_REG, cmd);
	spin_unlock_irqrestore(&msgbus_lock, irq_flags);

	/* host driver waits for bit 0 to be set in side band 0x7 */
	while (GET_SEM() != 0x1) {
		usleep_range(1000, 2000);
		timeout--;
		if (timeout <= 0) {
			pr_err("Timeout: semaphore timed out, reset sem\n");
			ret = -ETIMEDOUT;
			reset_semaphore();
			pr_err("PUNIT SEM: %d\n",
					intel_mid_msgbus_read32(PUNIT_PORT,
						PUNIT_SEMAPHORE));
			WARN_ON(1);
			return ret;
		}
	}
	smp_mb();

	pr_devel("i2c-semaphore: acquired i2c\n");

	return ret;
}
EXPORT_SYMBOL(intel_mid_dw_i2c_acquire_ownership);

int intel_mid_dw_i2c_release_ownership(void)
{
	reset_semaphore();

	pr_devel("i2c-semaphore: released i2c\n");

	return 0;
}
EXPORT_SYMBOL(intel_mid_dw_i2c_release_ownership);
