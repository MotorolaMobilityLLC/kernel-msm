/*
 * intel_scu_ipc.c: Driver for the Intel SCU IPC mechanism
 *
 * (C) Copyright 2008-2010 Intel Corporation
 * Author: Sreedhara DS (sreedhara.ds@intel.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * SCU running in ARC processor communicates with other entity running in IA
 * core through IPC mechanism which in turn messaging between IA core ad SCU.
 * SCU has two IPC mechanism IPC-1 and IPC-2. IPC-1 is used between IA32 and
 * SCU where IPC-2 is used between P-Unit and SCU. This driver delas with
 * IPC-1 Driver provides an API for power control unit registers (e.g. MSIC)
 * along with other APIs.
 */
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/pm.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <asm/intel-mid.h>
#include <asm/intel_scu_ipc.h>
#include <linux/pm_qos.h>
#include <linux/intel_mid_pm.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/sched.h>
#include <linux/atomic.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/wakelock.h>

enum {
	SCU_IPC_LINCROFT,
	SCU_IPC_PENWELL,
	SCU_IPC_CLOVERVIEW,
	SCU_IPC_TANGIER,
};

/* intel scu ipc driver data*/
struct intel_scu_ipc_pdata_t {
	u32 ipc_base;
	u32 i2c_base;
	u32 ipc_len;
	u32 i2c_len;
};

static struct intel_scu_ipc_pdata_t intel_scu_ipc_pdata[] = {
	[SCU_IPC_LINCROFT] = {
		.ipc_base = 0xff11c000,
		.i2c_base = 0xff12b000,
		.ipc_len = 0x100,
		.i2c_len = 0x10,
	},
	[SCU_IPC_PENWELL] = {
		.ipc_base = 0xff11c000,
		.i2c_base = 0xff12b000,
		.ipc_len = 0x100,
		.i2c_len = 0x10,
	},
	[SCU_IPC_CLOVERVIEW] = {
		.ipc_base = 0xff11c000,
		.i2c_base = 0xff12b000,
		.ipc_len = 0x100,
		.i2c_len = 0x10,
	},
	[SCU_IPC_TANGIER] = {
		.ipc_base = 0xff009000,
		.i2c_base  = 0xff00d000,
		.ipc_len  = 0x100,
		.i2c_len = 0x10,
	},
};
static int  scu_ipc_pm_callback(struct notifier_block *nb,
					unsigned long action,
					void *ignored);

static struct notifier_block scu_ipc_pm_notifier = {
	.notifier_call = scu_ipc_pm_callback,
	.priority = 1,
};

/*
 * IPC register summary
 *
 * IPC register blocks are memory mapped at fixed address of 0xFF11C000
 * To read or write information to the SCU, driver writes to IPC-1 memory
 * mapped registers (base address 0xFF11C000). The following is the IPC
 * mechanism
 *
 * 1. IA core cDMI interface claims this transaction and converts it to a
 *    Transaction Layer Packet (TLP) message which is sent across the cDMI.
 *
 * 2. South Complex cDMI block receives this message and writes it to
 *    the IPC-1 register block, causing an interrupt to the SCU
 *
 * 3. SCU firmware decodes this interrupt and IPC message and the appropriate
 *    message handler is called within firmware.
 */

#define IPC_STATUS_ADDR		0X04
#define IPC_SPTR_ADDR		0x08
#define IPC_DPTR_ADDR		0x0C
#define IPC_READ_BUFFER		0x90
#define IPC_WRITE_BUFFER	0x80
#define IPC_IOC			0x100

struct intel_ipc_controller {
	struct pci_dev *pdev;
	void __iomem *ipc_base;
	void __iomem *i2c_base;
	int ioc;
	int cmd;
	struct completion cmd_complete;
};

static struct intel_ipc_controller  ipcdev; /* Only one for now */

static int platform; /* Platform type */

static char *ipc_err_sources[] = {
	[IPC_ERR_NONE] =
		"no error",
	[IPC_ERR_CMD_NOT_SUPPORTED] =
		"command not supported",
	[IPC_ERR_CMD_NOT_SERVICED] =
		"command not serviced",
	[IPC_ERR_UNABLE_TO_SERVICE] =
		"unable to service",
	[IPC_ERR_CMD_INVALID] =
		"command invalid",
	[IPC_ERR_CMD_FAILED] =
		"command failed",
	[IPC_ERR_EMSECURITY] =
		"Invalid Battery",
	[IPC_ERR_UNSIGNEDKERNEL] =
		"Unsigned kernel",
};

#define IPC_I2C_CNTRL_ADDR	0
#define I2C_DATA_ADDR		0x04

static struct wake_lock ipc_wake_lock;

/* PM Qos struct */
static struct pm_qos_request *qos;

/* Suspend status*/
static bool suspend_status;
static DEFINE_MUTEX(scu_suspend_lock);

/* Suspend status get */
bool suspend_in_progress(void)
{
	return suspend_status;
}

/* Suspend status set */
void set_suspend_status(bool status)
{
	mutex_lock(&scu_suspend_lock);
	suspend_status = status;
	mutex_unlock(&scu_suspend_lock);
}

/* IPC PM notifier callback */
static int scu_ipc_pm_callback(struct notifier_block *nb,
					unsigned long action,
					void *ignored)
{
	switch (action) {
	case PM_SUSPEND_PREPARE:
		set_suspend_status(true);
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		set_suspend_status(false);
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

/*
 * Command Register (Write Only):
 * A write to this register results in an interrupt to the SCU core processor
 * Format:
 * |rfu2(8) | size(8) | command id(4) | rfu1(3) | ioc(1) | command(8)|
 */
void intel_scu_ipc_send_command(u32 cmd) /* Send ipc command */
{
	ipcdev.cmd = cmd;
	INIT_COMPLETION(ipcdev.cmd_complete);

	if (system_state == SYSTEM_RUNNING && !suspend_in_progress()) {
		ipcdev.ioc = 1;
		writel(cmd | IPC_IOC, ipcdev.ipc_base);
	} else {
		ipcdev.ioc = 0;
		writel(cmd, ipcdev.ipc_base);
	}
}

/*
 * IPC Write Buffer (Write Only):
 * 16-byte buffer for sending data associated with IPC command to
 * SCU. Size of the data is specified in the IPC_COMMAND_REG register
 */
static inline void ipc_data_writel(u32 data, u32 offset) /* Write ipc data */
{
	writel(data, ipcdev.ipc_base + IPC_WRITE_BUFFER + offset);
}

/*
 * Status Register (Read Only):
 * Driver will read this register to get the ready/busy status of the IPC
 * block and error status of the IPC command that was just processed by SCU
 * Format:
 * |rfu3(8)|error code(8)|initiator id(8)|cmd id(4)|rfu1(2)|error(1)|busy(1)|
 */

static inline u32 ipc_read_status(void)
{
	return __raw_readl(ipcdev.ipc_base + IPC_STATUS_ADDR);
}

static inline u8 ipc_data_readb(u32 offset) /* Read ipc byte data */
{
	return readb(ipcdev.ipc_base + IPC_READ_BUFFER + offset);
}

static inline u32 ipc_data_readl(u32 offset) /* Read ipc u32 data */
{
	return readl(ipcdev.ipc_base + IPC_READ_BUFFER + offset);
}

int intel_scu_ipc_check_status(void)
{
	int i;
	int ret = 0;
	int status;
	int loop_count = 3000000;

	if (ipcdev.ioc && (system_state == SYSTEM_RUNNING) &&
			(!suspend_in_progress())) {
		if (0 == wait_for_completion_timeout(
				&ipcdev.cmd_complete, 3 * HZ))
			ret = -ETIMEDOUT;
	} else {
		while ((ipc_read_status() & 1) && --loop_count)
			udelay(1);
		if (loop_count == 0)
			ret = -ETIMEDOUT;
	}

	status = ipc_read_status();
	if (ret == -ETIMEDOUT)
		dev_err(&ipcdev.pdev->dev,
			"IPC timed out, IPC_STS=0x%x, IPC_CMD=0x%x\n",
			status, ipcdev.cmd);

	if (status & 0x2) {
		ret = -EIO;
		i = (status >> 16) & 0xFF;
		if (i < ARRAY_SIZE(ipc_err_sources))
			dev_err(&ipcdev.pdev->dev,
				"IPC failed: %s, IPC_STS=0x%x, IPC_CMD=0x%x\n",
				ipc_err_sources[i], status, ipcdev.cmd);
		else
			dev_err(&ipcdev.pdev->dev,
				"IPC failed: unknown error, IPC_STS=0x%x, "
				"IPC_CMD=0x%x\n", status, ipcdev.cmd);
		if ((i == IPC_ERR_UNSIGNEDKERNEL) || (i == IPC_ERR_EMSECURITY))
			ret = -EACCES;
	}

	return ret;
}

void intel_scu_ipc_lock(void)
{
	/* Prevent C-states beyond C6 */
	pm_qos_update_request(qos, CSTATE_EXIT_LATENCY_S0i1 - 1);

	/* Prevent S3 */
	mutex_lock(&scu_suspend_lock);

	if (!suspend_in_progress())
		wake_lock(&ipc_wake_lock);
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_lock);

void intel_scu_ipc_unlock(void)
{
	/* Re-enable S3 */
	if (!suspend_in_progress())
		wake_unlock(&ipc_wake_lock);

	mutex_unlock(&scu_suspend_lock);

	/* Re-enable Deeper C-states beyond C6 */
	pm_qos_update_request(qos, PM_QOS_DEFAULT_VALUE);
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_unlock);

/**
 *	intel_scu_ipc_simple_command - send a simple command
 *	@cmd: command
 *	@sub: sub type
 *
 *	Issue a simple command to the SCU. Do not use this interface if
 *	you must then access data as any data values may be overwritten
 *	by another SCU access by the time this function returns.
 *
 *	This function may sleep. Locking for SCU accesses is handled for
 *	the caller.
 */
int intel_scu_ipc_simple_command(int cmd, int sub)
{
	int err;

	if (ipcdev.pdev == NULL)
		return -ENODEV;

	intel_scu_ipc_lock();
	intel_scu_ipc_send_command(sub << 12 | cmd);
	err = intel_scu_ipc_check_status();
	intel_scu_ipc_unlock();
	return err;
}
EXPORT_SYMBOL(intel_scu_ipc_simple_command);

/**
 * intel_scu_ipc_raw_cmd - raw ipc command with data
 * @cmd: command
 * @sub: sub type
 * @in: input data
 * @inlen: input length in bytes
 * @out: output data
 * @outlen: output length in dwords
 * @sptr: data writing to SPTR register
 * @dptr: data writing to DPTR register
 *
 * Issue a command to the SCU which involves data transfers. Do the
 * data copies under the lock but leave it for the caller to interpret
 * Note: This function should be called with the holding of ipclock
 */
int intel_scu_ipc_raw_cmd(u32 cmd, u32 sub, u8 *in, u32 inlen, u32 *out,
		u32 outlen, u32 dptr, u32 sptr)
{
	int i, err;
	u32 wbuf[4] = { 0 };

	if (ipcdev.pdev == NULL)
		return -ENODEV;

	if (inlen > 16)
		return -EINVAL;

	memcpy(wbuf, in, inlen);

	writel(dptr, ipcdev.ipc_base + IPC_DPTR_ADDR);
	writel(sptr, ipcdev.ipc_base + IPC_SPTR_ADDR);

	/**
	 * SRAM controller doesn't support 8bit write, it only supports
	 * 32bit write, so we have to write into the WBUF in 32bit,
	 * and SCU FW will use the inlen to determine the actual input
	 * data length in the WBUF.
	 */
	for (i = 0; i < ((inlen + 3) / 4); i++)
		ipc_data_writel(wbuf[i], 4 * i);

	/**
	 * Watchdog IPC command is an exception here using double word
	 * as the unit of input data size because of historical reasons
	 * and SCU FW is doing so.
	 */
	if ((cmd & 0xFF) == IPCMSG_WATCHDOG_TIMER)
		inlen = (inlen + 3) / 4;
	/*
	 *  In case of 3 pmic writes or read-modify-writes
	 *  there are holes in the middle of the buffer which are
	 *  ignored by SCU. These bytes should not be included into
	 *  size of the ipc msg. Holes are as follows:
	 *  write: wbuf[6 & 7]
	 *  read-modifu-write: wbuf[6 & 7 & 11]
	 */
	else if ((cmd & 0xFF) == IPCMSG_PCNTRL) {
		if (sub == IPC_CMD_PCNTRL_W && inlen == 11)
			inlen -= 2;
		else if (sub == IPC_CMD_PCNTRL_M && inlen == 15)
			inlen -= 3;
	}
	intel_scu_ipc_send_command((inlen << 16) | (sub << 12) | cmd);
	err = intel_scu_ipc_check_status();

	for (i = 0; i < outlen; i++)
		*out++ = ipc_data_readl(4 * i);

	return err;
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_raw_cmd);

int intel_scu_ipc_command(u32 cmd, u32 sub, u8 *in, u32 inlen,
		u32 *out, u32 outlen)
{
	int ret;
	intel_scu_ipc_lock();
	ret = intel_scu_ipc_raw_cmd(cmd, sub, in, inlen, out, outlen, 0, 0);
	intel_scu_ipc_unlock();
	return ret;
}
EXPORT_SYMBOL_GPL(intel_scu_ipc_command);

/*
 * Interrupt handler gets called when ioc bit of IPC_COMMAND_REG set to 1
 * When ioc bit is set to 1, caller api must wait for interrupt handler called
 * which in turn unlocks the caller api.
 *
 * This is edge triggered so we need take no action to clear anything
 */
static irqreturn_t ioc(int irq, void *dev_id)
{
	complete(&ipcdev.cmd_complete);
	return IRQ_HANDLED;
}

/**
 *	ipc_probe - probe an Intel SCU IPC
 *	@dev: the PCI device matching
 *	@id: entry in the match table
 *
 *	Enable and install an intel SCU IPC. This appears in the PCI space
 *	but uses some hard coded addresses as well.
 */
static int ipc_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int err, pid;
	struct intel_scu_ipc_pdata_t *pdata;
	resource_size_t pci_resource;

	if (ipcdev.pdev)		/* We support only one SCU */
		return -EBUSY;

	pid = id->driver_data;
	pdata = &intel_scu_ipc_pdata[pid];

	ipcdev.pdev = pci_dev_get(dev);

	err = pci_enable_device(dev);
	if (err)
		return err;

	err = pci_request_regions(dev, "intel_scu_ipc");
	if (err)
		return err;

	pci_resource = pci_resource_start(dev, 0);
	if (!pci_resource)
		return -ENOMEM;

	init_completion(&ipcdev.cmd_complete);

	if (request_irq(dev->irq, ioc, IRQF_NO_SUSPEND, "intel_scu_ipc",
		&ipcdev))
		return -EBUSY;

	ipcdev.ipc_base = ioremap_nocache(pdata->ipc_base, pdata->ipc_len);
	if (!ipcdev.ipc_base)
		return -ENOMEM;

	ipcdev.i2c_base = ioremap_nocache(pdata->i2c_base, pdata->i2c_len);
	if (!ipcdev.i2c_base) {
		iounmap(ipcdev.ipc_base);
		return -ENOMEM;
	}

	intel_scu_devices_create();

	return 0;
}

/**
 *	ipc_remove - remove a bound IPC device
 *	@pdev: PCI device
 *
 *	In practice the SCU is not removable but this function is also
 *	called for each device on a module unload or cleanup which is the
 *	path that will get used.
 *
 *	Free up the mappings and release the PCI resources
 */
static void ipc_remove(struct pci_dev *pdev)
{
	free_irq(pdev->irq, &ipcdev);
	pci_release_regions(pdev);
	pci_dev_put(ipcdev.pdev);
	iounmap(ipcdev.ipc_base);
	iounmap(ipcdev.i2c_base);
	ipcdev.pdev = NULL;
	intel_scu_devices_destroy();
}

static DEFINE_PCI_DEVICE_TABLE(pci_ids) = {
	{PCI_VDEVICE(INTEL, 0x080e), SCU_IPC_PENWELL},
	{PCI_VDEVICE(INTEL, 0x082a), SCU_IPC_LINCROFT},
	{PCI_VDEVICE(INTEL, 0x08ea), SCU_IPC_CLOVERVIEW},
	{PCI_VDEVICE(INTEL, 0x11a0), SCU_IPC_TANGIER},
	{ 0,}
};
MODULE_DEVICE_TABLE(pci, pci_ids);

static struct pci_driver ipc_driver = {
	.name = "intel_scu_ipc",
	.id_table = pci_ids,
	.probe = ipc_probe,
	.remove = ipc_remove,
};

static int intel_scu_ipc_init(void)
{
	platform = intel_mid_identify_cpu();
	if (platform == 0)
		return -ENODEV;

	qos = kzalloc(sizeof(struct pm_qos_request), GFP_KERNEL);
	if (!qos)
		return -ENOMEM;

	pm_qos_add_request(qos, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);

	register_pm_notifier(&scu_ipc_pm_notifier);

	wake_lock_init(&ipc_wake_lock, WAKE_LOCK_SUSPEND, "intel_scu_ipc");

	return  pci_register_driver(&ipc_driver);
}

static void __exit intel_scu_ipc_exit(void)
{
	pm_qos_remove_request(qos);

	pci_unregister_driver(&ipc_driver);
}

MODULE_AUTHOR("Sreedhara DS <sreedhara.ds@intel.com>");
MODULE_DESCRIPTION("Intel SCU IPC driver");
MODULE_LICENSE("GPL");

fs_initcall(intel_scu_ipc_init);
module_exit(intel_scu_ipc_exit);
