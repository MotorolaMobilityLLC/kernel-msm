/*
 * intel_psh_ipc.c: Driver for the Intel PSH IPC mechanism
 *
 * (C) Copyright 2012 Intel Corporation
 * Author: Yang Bin (bin.yang@intel.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <asm/intel_psh_ipc.h>
#include <asm/intel-mid.h>
#include <linux/fs.h>
#include <linux/intel_mid_pm.h>

#define PSH_ERR(fmt, arg...)	dev_err(&ipc_ctrl.pdev->dev, fmt, ##arg)
#define PSH_DBG(fmt, arg...)	dev_dbg(&ipc_ctrl.pdev->dev, fmt, ##arg)

#define STATUS_PSH2IA(x)	(1 << ((x) + 6))
#define FLAG_BIND		(1 << 0)

#define IS_A_STEP		(ipc_ctrl.reg_map == 0)

#define PIMR_A_STEP(x)		(ipc_ctrl.psh_regs->psh_regs_a_step.pimr##x)
#define PIMR_B_STEP(x)		(ipc_ctrl.psh_regs->psh_regs_b_step.pimr##x)

#define PIMR_ADDR(x)		((ipc_ctrl.reg_map & 1) ?	\
				&PIMR_B_STEP(x) : &PIMR_A_STEP(x))

#define PSH_REG_A_STEP(x)	(ipc_ctrl.psh_regs->psh_regs_a_step.x)
#define PSH_REG_B_STEP(x)	(ipc_ctrl.psh_regs->psh_regs_b_step.x)

#define PSH_REG_ADDR(x)		((ipc_ctrl.reg_map & 1) ?	\
				&PSH_REG_B_STEP(x) : &PSH_REG_A_STEP(x))

#define PSH_CH_HANDLE(x)	(ipc_ctrl.channel_handle[x])
#define PSH_CH_DATA(x)		(ipc_ctrl.channel_data[x])
#define PSH_CH_FLAG(x)		(ipc_ctrl.flags[x])

/* PSH registers */
union psh_registers {
	/* reg mem map A */
	struct {
		u32		csr;	/* 00h */
		u32		res1;	/* padding */
		u32		pisr;	/* 08h */
		u32		pimr0;	/* 0Ch */
		u32		pimr1;	/* 10h */
		u32		pimr2;	/* 14h */
		u32		pimr3;	/* 18h */
		u32		pmctl;	/* 1Ch */
		u32		pmstat;	/* 20h */
		u32		res2;	/* padding */
		struct psh_msg	ia2psh[NUM_IA2PSH_IPC];/* 28h ~ 44h + 3 */
		struct psh_msg	cry2psh;/* 48h ~ 4Ch + 3 */
		struct psh_msg	scu2psh;/* 50h ~ 54h + 3 */
		u32		res3[2];/* padding */
		struct psh_msg	psh2ia[NUM_PSH2IA_IPC];/* 60h ~ 7Ch + 3 */
		struct psh_msg	psh2cry;/* 80h ~ 84h + 3 */
		struct psh_msg  psh2scu;/* 88h */
		u32		msi_dir;/* 90h */
		u32		res4[3];
		u32		scratchpad[2];/* A0 */
	} __packed psh_regs_a_step;
	/* reg mem map B */
	struct {
		u32		pimr0;		/* 00h */
		u32		csr;		/* 04h */
		u32		pmctl;		/* 08h */
		u32		pmstat;		/* 0Ch */
		u32		psh_msi_direct;	/* 10h */
		u32		res1[59];	/* 14h ~ FCh + 3, padding */
		u32		pimr3;		/* 100h */
		struct psh_msg	scu2psh;	/* 104h ~ 108h + 3 */
		struct psh_msg	psh2scu;	/* 10Ch ~ 110h + 3 */
		u32		res2[187];	/* 114h ~ 3FCh + 3, padding */
		u32		pisr;		/* 400h */
		u32		scratchpad[2];	/* 404h ~ 407h */
		u32		res3[61];	/* 40Ch ~ 4FCh + 3, padding */
		u32		pimr1;		/* 500h */
		struct psh_msg	ia2psh[NUM_IA2PSH_IPC];	/* 504h ~ 520h + 3 */
		struct psh_msg	psh2ia[NUM_PSH2IA_IPC];	/* 524h ~ 540h + 3 */
		u32		res4[175];	/* 544h ~ 7FCh + 3, padding */
		u32		pimr2;		/* 800h */
		struct psh_msg	cry2psh;	/* 804h ~ 808h + 3 */
		struct psh_msg	psh2cry;	/* 80Ch ~ 810h + 3 */
	} __packed psh_regs_b_step;
} __packed;

static struct ipc_controller_t {
	int			reg_map;
	int			initialized;
	struct pci_dev		*pdev;
	spinlock_t		lock;
	int			flags[NUM_ALL_CH];
	union psh_registers	*psh_regs;
	struct semaphore	ch_lock[NUM_ALL_CH];
	struct mutex		psh_mutex;
	psh_channel_handle_t	channel_handle[NUM_PSH2IA_IPC];
	void			*channel_data[NUM_PSH2IA_IPC];
} ipc_ctrl;


/**
 * intel_ia2psh_command - send IA to PSH command
 * Send ia2psh command and return psh message and status
 *
 * @in: input psh message
 * @out: output psh message
 * @ch: psh channel
 * @timeout: timeout for polling busy bit, in us
 */
int intel_ia2psh_command(struct psh_msg *in, struct psh_msg *out,
			 int ch, int timeout)
{
	int ret = 0;
	u32 status;

	might_sleep();

	if (!ipc_ctrl.initialized)
		return -ENODEV;

	if (ch < PSH_SEND_CH0 || ch > PSH_SEND_CH0 + NUM_IA2PSH_IPC - 1
		|| in == NULL)
		return -EINVAL;

	if (in->msg & CHANNEL_BUSY)
		return -EINVAL;

	pm_runtime_get_sync(&ipc_ctrl.pdev->dev);
	down(&ipc_ctrl.ch_lock[ch]);

	in->msg |= CHANNEL_BUSY;
	/* Check if channel is ready for IA sending command */

	if (IS_A_STEP) {
		/* PSH_CSR_WORKAROUND */
		int tm = 10000;

		/* wait either D0i0 got ack'ed by PSH, or scratchpad set */
		usleep_range(1000, 2000);
		while (readl(PSH_REG_ADDR(scratchpad[0])) && --tm)
			usleep_range(100, 101);
		if (!tm)
			PSH_ERR("psh wait for scratchpad timeout\n");

		tm = 10000;
		while ((readl(PSH_REG_ADDR(ia2psh[ch].msg)) & CHANNEL_BUSY)
				&& --tm)
			usleep_range(100, 101);
		if (!tm) {
			PSH_ERR("psh ch[%d] wait for busy timeout\n", ch);
			ret = -EBUSY;
			goto end;
		}
	} else {
		if (readl(PSH_REG_ADDR(ia2psh[ch].msg)) & CHANNEL_BUSY) {
			ret = -EBUSY;
			goto end;
		}
	}

	writel(in->param, PSH_REG_ADDR(ia2psh[ch].param));
	writel(in->msg, PSH_REG_ADDR(ia2psh[ch].msg));

	/* Input timeout is zero, do not check channel status */
	if (timeout == 0)
		goto end;

	/* Input timeout is nonzero, check channel status */
	while (((status = readl(PSH_REG_ADDR(ia2psh[ch].msg))) & CHANNEL_BUSY)
		&& timeout) {
		usleep_range(100, 101);
		timeout -= 100;
	}

	if (timeout <= 0) {
		ret = -ETIMEDOUT;
		PSH_ERR("ia2psh channel %d is always busy!\n", ch);
		goto end;
	} else {
		if (out == NULL)
			goto end;

		out->param = readl(PSH_REG_ADDR(ia2psh[ch].param));
		out->msg = status;
	}

end:
	up(&ipc_ctrl.ch_lock[ch]);
	pm_runtime_put(&ipc_ctrl.pdev->dev);

	return ret;
}
EXPORT_SYMBOL(intel_ia2psh_command);

/**
 * intel_psh_ipc_bind - bind a handler to a psh channel
 *
 * @ch: psh channel
 * @handle: handle function called when IA received psh interrupt
 * @data: data passed to handle
 */
int intel_psh_ipc_bind(int ch, psh_channel_handle_t handle, void *data)
{
	unsigned long flags;

	if (!ipc_ctrl.initialized)
		return -ENODEV;

	if (!handle || ch < PSH_RECV_CH0
			|| ch > PSH_RECV_CH0 + NUM_PSH2IA_IPC - 1)
		return -EINVAL;

	mutex_lock(&ipc_ctrl.psh_mutex);
	down(&ipc_ctrl.ch_lock[ch]);
	if (PSH_CH_HANDLE(ch - PSH_RECV_CH0) != NULL) {
		up(&ipc_ctrl.ch_lock[ch]);
		mutex_unlock(&ipc_ctrl.psh_mutex);
		return -EBUSY;
	} else {
		PSH_CH_DATA(ch - PSH_RECV_CH0) = data;
		PSH_CH_HANDLE(ch - PSH_RECV_CH0) = handle;
	}
	up(&ipc_ctrl.ch_lock[ch]);

	pm_runtime_get_sync(&ipc_ctrl.pdev->dev);
	spin_lock_irqsave(&ipc_ctrl.lock, flags);
	PSH_CH_FLAG(ch) |= FLAG_BIND;
	writel(readl(PIMR_ADDR(1)) | (1 << (ch - PSH_RECV_CH0)), PIMR_ADDR(1));
	spin_unlock_irqrestore(&ipc_ctrl.lock, flags);
	pm_runtime_put(&ipc_ctrl.pdev->dev);
	mutex_unlock(&ipc_ctrl.psh_mutex);

	return 0;
}
EXPORT_SYMBOL(intel_psh_ipc_bind);

/**
 * intel_psh_ipc_unbind - unbind a handler to a psh channel
 *
 * @ch: psh channel
 */
void intel_psh_ipc_unbind(int ch)
{
	unsigned long flags;

	if (!ipc_ctrl.initialized)
		return;

	if (ch < PSH_RECV_CH0 || ch > PSH_RECV_CH0 + NUM_PSH2IA_IPC - 1)
		return;

	if (!(PSH_CH_FLAG(ch) & FLAG_BIND))
		return;

	mutex_lock(&ipc_ctrl.psh_mutex);
	pm_runtime_get_sync(&ipc_ctrl.pdev->dev);
	spin_lock_irqsave(&ipc_ctrl.lock, flags);
	PSH_CH_FLAG(ch) &= ~FLAG_BIND;
	writel(readl(PIMR_ADDR(1)) & (~(1 << (ch - PSH_RECV_CH0))),
						PIMR_ADDR(1));
	spin_unlock_irqrestore(&ipc_ctrl.lock, flags);
	pm_runtime_put(&ipc_ctrl.pdev->dev);

	down(&ipc_ctrl.ch_lock[ch]);
	PSH_CH_HANDLE(ch - PSH_RECV_CH0) = NULL;
	up(&ipc_ctrl.ch_lock[ch]);
	mutex_unlock(&ipc_ctrl.psh_mutex);
}
EXPORT_SYMBOL(intel_psh_ipc_unbind);

void intel_psh_ipc_disable_irq(void)
{
	disable_irq(ipc_ctrl.pdev->irq);
}
EXPORT_SYMBOL(intel_psh_ipc_disable_irq);

void intel_psh_ipc_enable_irq(void)
{
	enable_irq(ipc_ctrl.pdev->irq);
}
EXPORT_SYMBOL(intel_psh_ipc_enable_irq);

static void psh_recv_handle(int i)
{
	int msg, param;

	down(&ipc_ctrl.ch_lock[i + PSH_RECV_CH0]);

	msg = readl(PSH_REG_ADDR(psh2ia[i].msg)) & (~CHANNEL_BUSY);
	param = readl(PSH_REG_ADDR(psh2ia[i].param));

	if (PSH_CH_HANDLE(i) == NULL) {
		PSH_ERR("Ignore message from channel %d\n", i+PSH_RECV_CH0);
		goto end;
	}

	/* write back to clear the busy bit */
	writel(msg, PSH_REG_ADDR(psh2ia[i].msg));
	PSH_CH_HANDLE(i)(msg, param, PSH_CH_DATA(i));
end:
	up(&ipc_ctrl.ch_lock[i+PSH_RECV_CH0]);
}

static irqreturn_t psh_ipc_irq(int irq, void *data)
{
	int i;
	u32 status;

	pm_runtime_get_sync(&ipc_ctrl.pdev->dev);
	status = readl(PSH_REG_ADDR(pisr));

	for (i = 0; i < NUM_PSH2IA_IPC; i++) {
		if (status & STATUS_PSH2IA(i))
			psh_recv_handle(i);
	}

	pm_runtime_put(&ipc_ctrl.pdev->dev);
	return IRQ_HANDLED;
}

static void psh_regs_dump(void)
{
	int i;

	pm_runtime_get_sync(&ipc_ctrl.pdev->dev);
	PSH_ERR("\n<-------------start------------>\n");

	PSH_ERR("csr:\t%#x\n", readl(PSH_REG_ADDR(csr)));
	PSH_ERR("pisr:\t%#x\n", readl(PSH_REG_ADDR(pisr)));

	PSH_ERR("pimr0:\t%#x\n", readl(PIMR_ADDR(0)));
	PSH_ERR("pimr1:\t%#x\n", readl(PIMR_ADDR(1)));
	PSH_ERR("pimr2:\t%#x\n", readl(PIMR_ADDR(2)));
	PSH_ERR("pimr3:\t%#x\n", readl(PIMR_ADDR(3)));

	PSH_ERR("pmctl:\t%#x\n", readl(PSH_REG_ADDR(pmctl)));
	PSH_ERR("pmstat:\t%#x\n", readl(PSH_REG_ADDR(pmstat)));
	PSH_ERR("scratchpad0:\t%#x\n", readl(PSH_REG_ADDR(scratchpad[0])));
	PSH_ERR("scratchpad1:\t%#x\n", readl(PSH_REG_ADDR(scratchpad[1])));

	for (i = 0; i < NUM_IA2PSH_IPC; i++) {
		PSH_ERR("ia2psh[%d].msg:\t%#x\n", i,
				readl(PSH_REG_ADDR(ia2psh[i].msg)));
		PSH_ERR("ia2psh[%d].param:\t%#x\n", i,
				readl(PSH_REG_ADDR(ia2psh[i].param)));
	}

	PSH_ERR("cry2psh.msg:\t%#x\n", readl(PSH_REG_ADDR(cry2psh.msg)));
	PSH_ERR("cry2psh.param:\t%#x\n", readl(PSH_REG_ADDR(cry2psh.param)));
	PSH_ERR("scu2psh.msg:\t%#x\n", readl(PSH_REG_ADDR(scu2psh.msg)));
	PSH_ERR("scu2psh.param:\t%#x\n", readl(PSH_REG_ADDR(scu2psh.param)));

	for (i = 0; i < NUM_PSH2IA_IPC; i++) {
		PSH_ERR("psh2ia[%d].msg:\t%#x\n", i,
				readl(PSH_REG_ADDR(psh2ia[i].msg)));
		PSH_ERR("psh2ia[%d].param:\t%#x\n", i,
				readl(PSH_REG_ADDR(psh2ia[i].param)));
	}

	PSH_ERR("psh2cry.msg:\t%#x\n", readl(PSH_REG_ADDR(psh2cry.msg)));
	PSH_ERR("psh2cry.param:\t%#x\n", readl(PSH_REG_ADDR(psh2cry.param)));

	PSH_ERR("\n<-------------end------------>\n");
	pm_runtime_put(&ipc_ctrl.pdev->dev);
}

static struct psh_msg psh_dbg_msg;
static int psh_ch;

static ssize_t psh_msg_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"\nLast ia2psh command with msg: %#x\nparam: %#x\n",
			psh_dbg_msg.msg, psh_dbg_msg.param);
}

static ssize_t psh_msg_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t size)
{
	int ret;
	u32 msg, param;

	memset(&psh_dbg_msg, 0, sizeof(psh_dbg_msg));

	ret = sscanf(buf, "%x %x", &msg, &param);
	if (ret != 2) {
		PSH_ERR("Input two arguments as psh msg and param\n");
		return -EINVAL;
	}

	psh_dbg_msg.msg = msg;
	psh_dbg_msg.param = param;

	return size;
}

static ssize_t psh_ch_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"\nLast psh channel: %d\n", psh_ch);
}

static ssize_t psh_ch_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t size)
{
	int ret;

	ret = sscanf(buf, "%d", &psh_ch);
	if (ret != 1) {
		PSH_ERR("Input one argument as psh channel\n");
		return -EINVAL;
	}

	return size;
}

static ssize_t psh_send_cmd_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t size)
{
	int psh_dbg_err;
	struct psh_msg out_msg;

	memset(&out_msg, 0, sizeof(out_msg));

	psh_dbg_err = intel_ia2psh_command(&psh_dbg_msg, &out_msg,
					psh_ch, 3000000);
	if (psh_dbg_err) {
		PSH_ERR("Send ia2psh command failed, err %d\n", psh_dbg_err);
		psh_regs_dump();
		return psh_dbg_err;
	}

	return size;
}

static DEVICE_ATTR(psh_msg, S_IRUSR | S_IWUSR, psh_msg_show, psh_msg_store);
static DEVICE_ATTR(psh_ch, S_IRUSR | S_IWUSR, psh_ch_show, psh_ch_store);
static DEVICE_ATTR(ia2psh_cmd, S_IWUSR, NULL, psh_send_cmd_store);

static struct attribute *psh_attrs[] = {
	&dev_attr_psh_msg.attr,
	&dev_attr_psh_ch.attr,
	&dev_attr_ia2psh_cmd.attr,
	NULL,
};

static struct attribute_group psh_attr_group = {
	.name = "psh_debug",
	.attrs = psh_attrs,
};

static int intel_psh_debug_sysfs_create(struct pci_dev *pdev)
{
	return sysfs_create_group(&pdev->dev.kobj, &psh_attr_group);
}

static void pmic_sysfs_remove(struct pci_dev *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &psh_attr_group);
}

#ifdef CONFIG_PM
static int psh_ipc_suspend_noirq(struct device *dev)
{
	int i;
	int ret = 0;

	for (i = 0; i < NUM_ALL_CH; i++) {
		if (down_trylock(&ipc_ctrl.ch_lock[i])) {
			ret = -EBUSY;
			break;
		}
	}

	if (ret) {
		for (; i > 0; i--)
			up(&ipc_ctrl.ch_lock[i - 1]);
	}

	return ret;
}

static int psh_ipc_resume_noirq(struct device *dev)
{
	int i;

	for (i = 0; i < NUM_ALL_CH; i++)
		up(&ipc_ctrl.ch_lock[i]);

	return 0;
}

#else

#define psh_ipc_suspend_noirq	NULL
#define psh_ipc_resume_noirq	NULL

#endif

#ifdef CONFIG_PM_RUNTIME
static int psh_ipc_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "runtime suspend called\n");
	return 0;
}

static int psh_ipc_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "runtime resume called\n");
	return 0;
}

#else

#define psh_ipc_runtime_suspend	NULL
#define psh_ipc_runtime_resume	NULL

#endif

static int psh_ipc_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int i, ret;
	unsigned long start, len;

	ipc_ctrl.pdev = pci_dev_get(pdev);
	ret = pci_enable_device(pdev);
	if (ret)
		goto err1;

	start = pci_resource_start(pdev, 0);
	len = pci_resource_len(pdev, 0);
	if (!start || !len) {
		ret = -ENODEV;
		goto err1;
	}

	ret = pci_request_regions(pdev, "intel_psh_ipc");
	if (ret)
		goto err1;

	switch (intel_mid_identify_cpu()) {
	case INTEL_MID_CPU_CHIP_TANGIER:
		if (intel_mid_soc_stepping() == 0)
			ipc_ctrl.reg_map = 0;
		else
			ipc_ctrl.reg_map = 1;
		break;
	case INTEL_MID_CPU_CHIP_ANNIEDALE:
		ipc_ctrl.reg_map = 1;
		break;
	default:
		dev_err(&pdev->dev, "error register map\n");
		ret = -EINVAL;
		goto err2;
		break;
	}

	ipc_ctrl.psh_regs = (union psh_registers *)ioremap_nocache(start, len);
	if (!ipc_ctrl.psh_regs) {
		ret = -ENOMEM;
		goto err2;
	}

	ret = request_threaded_irq(pdev->irq, NULL, psh_ipc_irq, IRQF_ONESHOT,
			"intel_psh_ipc", NULL);
	if (ret) {
		dev_err(&pdev->dev, "Unable to register irq %d\n", pdev->irq);
		goto err3;
	}

	irq_set_irq_wake(pdev->irq, 1);

	spin_lock_init(&ipc_ctrl.lock);
	mutex_init(&ipc_ctrl.psh_mutex);

	for (i = 0; i < NUM_ALL_CH; i++)
		sema_init(&ipc_ctrl.ch_lock[i], 1);

	intel_psh_devices_create();

	intel_psh_debug_sysfs_create(pdev);

	ipc_ctrl.initialized = 1;

	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_allow(&pdev->dev);

	return 0;

err3:
	iounmap(ipc_ctrl.psh_regs);
err2:
	pci_release_regions(pdev);
err1:
	pci_dev_put(pdev);

	return ret;
}

static void psh_ipc_remove(struct pci_dev *pdev)
{
	pm_runtime_forbid(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);
	free_irq(pdev->irq, NULL);
	iounmap(ipc_ctrl.psh_regs);
	pci_release_regions(pdev);
	pci_dev_put(pdev);
	intel_psh_devices_destroy();
	pmic_sysfs_remove(pdev);
	ipc_ctrl.initialized = 0;
}

static const struct dev_pm_ops psh_ipc_drv_pm_ops = {
	.suspend_noirq		= psh_ipc_suspend_noirq,
	.resume_noirq		= psh_ipc_resume_noirq,
	.runtime_suspend	= psh_ipc_runtime_suspend,
	.runtime_resume		= psh_ipc_runtime_resume,
};

static DEFINE_PCI_DEVICE_TABLE(pci_ids) = {
	{PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x11a3)},
	{ 0,}
};
MODULE_DEVICE_TABLE(pci, pci_ids);

static struct pci_driver psh_ipc_driver = {
	.name = "intel_psh_ipc",
	.driver = {
		.pm = &psh_ipc_drv_pm_ops,
	},
	.id_table = pci_ids,
	.probe = psh_ipc_probe,
	.remove = psh_ipc_remove,
};

static int __init psh_ipc_init(void)
{
	return  pci_register_driver(&psh_ipc_driver);
}

static void __exit psh_ipc_exit(void)
{
	pci_unregister_driver(&psh_ipc_driver);
}

MODULE_AUTHOR("bin.yang@intel.com");
MODULE_DESCRIPTION("Intel PSH IPC driver");
MODULE_LICENSE("GPL v2");

fs_initcall(psh_ipc_init);
module_exit(psh_ipc_exit);
