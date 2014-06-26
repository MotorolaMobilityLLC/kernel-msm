/*
 *  pti.c - PTI driver for cJTAG data extration
 *
 *  Copyright (C) Intel 2010
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The PTI (Parallel Trace Interface) driver directs trace data routed from
 * various parts in the system out through the Intel Penwell PTI port and
 * out of the mobile device for analysis with a debugging tool
 * (Lauterbach, Fido). This is part of a solution for the MIPI P1149.7,
 * compact JTAG, standard.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/console.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/miscdevice.h>
#include <linux/pti.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <asm/intel_scu_ipc.h>

#ifdef CONFIG_INTEL_PTI_STM
#include "stm.h"
#endif

#define DRIVERNAME		"pti"
#define PCINAME			"pciPTI"
#define TTYNAME			"ttyPTI"
#define CHARNAME		"pti"
#define PTITTY_MINOR_START	0
#define PTITTY_MINOR_NUM	2
#define MAX_APP_IDS		16   /* 128 channel ids / u8 bit size */
#define MAX_OS_IDS		16   /* 128 channel ids / u8 bit size */
#define MAX_MODEM_IDS		16   /* 128 channel ids / u8 bit size */
#define MODEM_BASE_ID		71   /* modem master ID address    */
#define CONTROL_ID		72   /* control master ID address  */
#define CONSOLE_ID		73   /* console master ID address  */
#define OS_BASE_ID		74   /* base OS master ID address  */
#define APP_BASE_ID		80   /* base App master ID address */
#define CONTROL_FRAME_LEN	32   /* PTI control frame maximum size */
#define USER_COPY_SIZE		8192 /* 8Kb buffer for user space copy */
#define APERTURE_14		0x3800000 /* offset to first OS write addr */
#define APERTURE_LEN		0x400000  /* address length */

#define SMIP_PTI_OFFSET	0x30C  /* offset to PTI config in MIP header */
#define SMIP_PTI_EN	(1<<7) /* PTI enable bit in PTI configuration */

#define PTI_PNW_PCI_ID			0x082B
#define PTI_CLV_PCI_ID			0x0900
#define PTI_TNG_PCI_ID			0x119F

#define INTEL_PTI_PCI_DEVICE(dev, info) {	\
	.vendor = PCI_VENDOR_ID_INTEL,		\
	.device = dev,				\
	.subvendor = PCI_ANY_ID,		\
	.subdevice = PCI_ANY_ID,		\
	.driver_data = (unsigned long) info }

struct pti_device_info {
	u8 pci_bar;
	u8 scu_secure_mode:1;
	u8 has_d8_d16_support:1;
};

static const struct pti_device_info intel_pti_pnw_info = {
	.pci_bar = 1,
	.scu_secure_mode = 0,
	.has_d8_d16_support = 0,
};

static const struct pti_device_info intel_pti_clv_info = {
	.pci_bar = 1,
	.scu_secure_mode = 1,
	.has_d8_d16_support = 0,
};

static const struct pti_device_info intel_pti_tng_info = {
	.pci_bar = 2,
	.scu_secure_mode = 0,
	.has_d8_d16_support = 1,
};

static DEFINE_PCI_DEVICE_TABLE(pci_ids) = {
	INTEL_PTI_PCI_DEVICE(PTI_PNW_PCI_ID, &intel_pti_pnw_info),
	INTEL_PTI_PCI_DEVICE(PTI_CLV_PCI_ID, &intel_pti_clv_info),
	INTEL_PTI_PCI_DEVICE(PTI_TNG_PCI_ID, &intel_pti_tng_info),
	{0}
};

#define GET_PCI_BAR(pti_dev) (pti_dev->pti_dev_info->pci_bar)
#define HAS_SCU_SECURE_MODE(pti_dev) (pti_dev->pti_dev_info->scu_secure_mode)
#define HAS_D8_D16_SUPPORT(pti_dev) (pti_dev->pti_dev_info->has_d8_d16_support)

struct pti_tty {
	struct pti_masterchannel *mc;
};

struct pti_dev {
	struct tty_port port[PTITTY_MINOR_NUM];
	unsigned long pti_addr;
	unsigned long aperture_base;
	void __iomem *pti_ioaddr;
	u8 ia_app[MAX_APP_IDS];
	u8 ia_os[MAX_OS_IDS];
	u8 ia_modem[MAX_MODEM_IDS];
	struct pti_device_info *pti_dev_info;
#ifdef CONFIG_INTEL_PTI_STM
	struct stm_dev stm;
#endif
};

static unsigned int stm_enabled;
module_param(stm_enabled, uint, 0600);
MODULE_PARM_DESC(stm_enabled, "set to 1 to enable stm");

/*
 * This protects access to ia_app, ia_os, and ia_modem,
 * which keeps track of channels allocated in
 * an aperture write id.
 */
static DEFINE_SPINLOCK(cid_lock);

static struct tty_driver *pti_tty_driver;
static struct pti_dev *drv_data;

static unsigned int pti_console_channel;
static unsigned int pti_control_channel;

/**
 *  pti_write_to_aperture()- The private write function to PTI HW.
 *
 *  @mc: The 'aperture'. It's part of a write address that holds
 *       a master and channel ID.
 *  @buf: Data being written to the HW that will ultimately be seen
 *        in a debugging tool (Fido, Lauterbach).
 *  @len: Size of buffer.
 *  @eom: End Of Message indication. If true, DTS shall be used for
 *        the last bytes of the message.
 *
 *  Since each aperture is specified by a unique
 *  master/channel ID, no two processes will be writing
 *  to the same aperture at the same time so no lock is required. The
 *  PTI-Output agent will send these out in the order that they arrived, and
 *  thus, it will intermix these messages. The debug tool can then later
 *  regroup the appropriate message segments together reconstituting each
 *  message.
 */
static void pti_write_to_aperture(struct pti_masterchannel *mc,
				  u8 *buf,
				  int len,
				  bool eom)
{
	int dwordcnt;
	int final;
	int i;
	u32 ptiword;
	u16 ptishort;
	u8  ptibyte;
	u32 __iomem *aperture;
	u8 *p = buf;

	/*
	 * calculate the aperture offset from the base using the master and
	 * channel id's.
	 */
	aperture = drv_data->pti_ioaddr + (mc->master << 15)
		+ (mc->channel << 8);

	dwordcnt = len >> 2;
	final = len - (dwordcnt << 2);	    /* final = trailing bytes    */
	if (final == 0 && dwordcnt != 0) {  /* always need a final dword */
		final += 4;
		dwordcnt--;
	}

	for (i = 0; i < dwordcnt; i++) {
		ptiword = be32_to_cpu(*(u32 *)p);
		p += 4;
		iowrite32(ptiword, aperture);
	}

	if (!HAS_D8_D16_SUPPORT(drv_data)) {
		aperture += eom ? PTI_LASTDWORD_DTS : 0; /* DTS signals EOM */
		ptiword = 0;
		for (i = 0; i < final; i++)
			ptiword |= *p++ << (24-(8*i));
		iowrite32(ptiword, aperture);
	} else {
		switch (final) {

		case 3:
			ptishort = be16_to_cpu(*(u16 *)p);
			p += 2;
			iowrite16(ptishort, aperture);
			/* fall-through */
		case 1:
			ptibyte = *(u8 *)p;
			aperture += eom ? PTI_LASTDWORD_DTS : 0;
			iowrite8(ptibyte, aperture);
			break;
		case 2:
			ptishort = be16_to_cpu(*(u16 *)p);
			aperture += eom ? PTI_LASTDWORD_DTS : 0;
			iowrite16(ptishort, aperture);
			break;
		case 4:
			ptiword = be32_to_cpu(*(u32 *)p);
			aperture += eom ? PTI_LASTDWORD_DTS : 0;
			iowrite32(ptiword, aperture);
			break;
		default:
			break;
		}
	}

	return;
}

/**
 *  pti_control_frame_built_and_sent()- control frame build and send function.
 *
 *  @mc:          The master / channel structure on which the function
 *                built a control frame.
 *  @thread_name: The thread name associated with the master / channel or
 *                'NULL' if using the 'current' global variable.
 *
 *  To be able to post process the PTI contents on host side, a control frame
 *  is added before sending any PTI content. So the host side knows on
 *  each PTI frame the name of the thread using a dedicated master / channel.
 *  The thread name is retrieved from 'current' global variable if 'thread_name'
 *  is 'NULL', else it is retrieved from 'thread_name' parameter.
 *  This function builds this frame and sends it to a master ID CONTROL_ID.
 *  The overhead is only 32 bytes since the driver only writes to HW
 *  in 32 byte chunks.
 */
static void pti_control_frame_built_and_sent(struct pti_masterchannel *mc,
					     const char *thread_name)
{
	/*
	 * Since we access the comm member in current's task_struct, we only
	 * need to be as large as what 'comm' in that structure is.
	 */
	char comm[TASK_COMM_LEN];
	struct pti_masterchannel mccontrol = {.master = CONTROL_ID,
					      .channel = 0};
	const char *thread_name_p;
	const char *control_format = "%3d %3d %s";
	u8 control_frame[CONTROL_FRAME_LEN];

	if (!thread_name) {
		if (in_irq())
			strncpy(comm, "hardirq", sizeof(comm));
		else if (in_softirq())
			strncpy(comm, "softirq", sizeof(comm));
		else
			strncpy(comm, current->comm, sizeof(comm));

		/* Absolutely ensure our buffer is zero terminated. */
		comm[TASK_COMM_LEN-1] = 0;
		thread_name_p = comm;
	} else {
		thread_name_p = thread_name;
	}

	mccontrol.channel = pti_control_channel;
	pti_control_channel = (pti_control_channel + 1) & 0x7f;

	snprintf(control_frame, CONTROL_FRAME_LEN, control_format, mc->master,
		mc->channel, thread_name_p);
	pti_write_to_aperture(&mccontrol, control_frame,
			      strlen(control_frame), true);
}

/**
 *  pti_write_full_frame_to_aperture()- high level function to
 *					write to PTI.
 *
 *  @mc:  The 'aperture'. It's part of a write address that holds
 *        a master and channel ID.
 *  @buf: Data being written to the HW that will ultimately be seen
 *        in a debugging tool (Fido, Lauterbach).
 *  @len: Size of buffer.
 *
 *  All threads sending data (either console, user space application, ...)
 *  are calling the high level function to write to PTI meaning that it is
 *  possible to add a control frame before sending the content.
 */
static void pti_write_full_frame_to_aperture(struct pti_masterchannel *mc,
						const unsigned char *buf,
						int len)
{
	pti_control_frame_built_and_sent(mc, NULL);
	pti_write_to_aperture(mc, (u8 *)buf, len, true);
}

/**
 * get_id()- Allocate a master and channel ID.
 *
 * @id_array:    an array of bits representing what channel
 *               id's are allocated for writing.
 * @array_size:  array size in bytes
 * @base_id:     The starting SW channel ID, based on the Intel
 *               PTI arch.
 * @thread_name: The thread name associated with the master / channel or
 *               'NULL' if using the 'current' global variable.
 *
 * Returns:
 *	pti_masterchannel struct with master, channel ID address
 *	0 for error
 *
 * Each bit in the arrays ia_app and ia_os correspond to a master and
 * channel id. The bit is one if the id is taken and 0 if free. For
 * every master there are 128 channel id's.
 */
static struct pti_masterchannel *get_id(u8 *id_array,
					int array_size,
					int base_id,
					const char *thread_name)
{
	struct pti_masterchannel *mc;
	unsigned long flags;
	unsigned long *addr = (unsigned long *)id_array;
	unsigned long num_bits = array_size*8, n;

	/* Allocate memory with GFP_ATOMIC flag because this API
	 * can be called in interrupt context.
	 */
	mc = kmalloc(sizeof(struct pti_masterchannel), GFP_ATOMIC);
	if (mc == NULL)
		return NULL;

	/* Find the first available channel ID (first zero bit) in the
	 * bitdfield and toggle the corresponding bit to reserve it.
	 * This must be done under spinlock with interrupts disabled
	 * to ensure there is no concurrent access to the bitfield.
	 */
	spin_lock_irqsave(&cid_lock, flags);
	n = find_first_zero_bit(addr, num_bits);
	if (n >= num_bits) {
		kfree(mc);
		spin_unlock_irqrestore(&cid_lock, flags);
		return NULL;
	}
	change_bit(n, addr);
	spin_unlock_irqrestore(&cid_lock, flags);

	mc->master  = base_id;
	mc->channel = n;

	/* write new master Id / channel Id allocation to channel control */
	pti_control_frame_built_and_sent(mc, thread_name);
	return mc;
}

/*
 * The following three functions:
 * pti_request_mastercahannel(), mipi_release_masterchannel()
 * and pti_writedata() are an API for other kernel drivers to
 * access PTI.
 */

/**
 * pti_request_masterchannel()- Kernel API function used to allocate
 *				a master, channel ID address
 *				to write to PTI HW.
 *
 * @type:        0- request Application  master, channel aperture ID
 *                  write address.
 *               1- request OS master, channel aperture ID write
 *                  address.
 *               2- request Modem master, channel aperture ID
 *                  write address.
 *               Other values, error.
 * @thread_name: The thread name associated with the master / channel or
 *               'NULL' if using the 'current' global variable.
 *
 * Returns:
 *	pti_masterchannel struct
 *	0 for error
 */
struct pti_masterchannel *pti_request_masterchannel(u8 type,
						    const char *thread_name)
{
	struct pti_masterchannel *mc;

	if (drv_data == NULL)
		return NULL;

	switch (type) {

	case 0:
		mc = get_id(drv_data->ia_app, MAX_APP_IDS,
			    APP_BASE_ID, thread_name);
		break;

	case 1:
		mc = get_id(drv_data->ia_os, MAX_OS_IDS,
			    OS_BASE_ID, thread_name);
		break;

	case 2:
		mc = get_id(drv_data->ia_modem, MAX_MODEM_IDS,
			    MODEM_BASE_ID, thread_name);
		break;
	default:
		mc = NULL;
	}

	return mc;
}
EXPORT_SYMBOL_GPL(pti_request_masterchannel);

/**
 * pti_release_masterchannel()- Kernel API function used to release
 *				a master, channel ID address
 *				used to write to PTI HW.
 *
 * @mc: master, channel apeture ID address to be released.  This
 *      will de-allocate the structure via kfree().
 */
void pti_release_masterchannel(struct pti_masterchannel *mc)
{
	u8 master, channel;

	if (mc) {
		master = mc->master;
		channel = mc->channel;

		switch (master) {

		/* Note that clear_bit is atomic, so there is no need
		 * to use cid_lock here to protect the bitfield
		 */

		case APP_BASE_ID:
			clear_bit(mc->channel,
				  (unsigned long *)drv_data->ia_app);
			break;

		case OS_BASE_ID:
			clear_bit(mc->channel,
				  (unsigned long *)drv_data->ia_os);
			break;

		case MODEM_BASE_ID:
			clear_bit(mc->channel,
				  (unsigned long *)drv_data->ia_modem);
			break;

		default:
			pr_err("%s(%d) : Invalid master ID!\n",
			       __func__, __LINE__);
			break;
		}

		kfree(mc);
	}
}
EXPORT_SYMBOL_GPL(pti_release_masterchannel);

/**
 * pti_writedata()- Kernel API function used to write trace
 *                  debugging data to PTI HW.
 *
 * @mc:    Master, channel aperture ID address to write to.
 *         Null value will return with no write occurring.
 * @buf:   Trace debuging data to write to the PTI HW.
 *         Null value will return with no write occurring.
 * @count: Size of buf. Value of 0 or a negative number will
 *         return with no write occuring.
 * @eom:   End Of Message indication. If true, DTS shall be used
 *         for last bytes of the message.
 */
void pti_writedata(struct pti_masterchannel *mc, u8 *buf, int count, bool eom)
{
	/*
	 * since this function is exported, this is treated like an
	 * API function, thus, all parameters should
	 * be checked for validity.
	 */
	if ((mc != NULL) && (buf != NULL) && (count > 0))
		pti_write_to_aperture(mc, buf, count, eom);
	return;
}
EXPORT_SYMBOL_GPL(pti_writedata);

/*
 * for the tty_driver_*() basic function descriptions, see tty_driver.h.
 * Specific header comments made for PTI-related specifics.
 */

/**
 * pti_tty_driver_open()- Open an Application master, channel aperture
 * ID to the PTI device via tty device.
 *
 * @tty: tty interface.
 * @filp: filp interface pased to tty_port_open() call.
 *
 * Returns:
 *	int, 0 for success
 *	otherwise, fail value
 *
 * The main purpose of using the tty device interface is for
 * each tty port to have a unique PTI write aperture.  In an
 * example use case, ttyPTI0 gets syslogd and an APP aperture
 * ID and ttyPTI1 is where the n_tracesink ldisc hooks to route
 * modem messages into PTI.  Modem trace data does not have to
 * go to ttyPTI1, but ttyPTI0 and ttyPTI1 do need to be distinct
 * master IDs.  These messages go through the PTI HW and out of
 * the handheld platform and to the Fido/Lauterbach device.
 */
static int pti_tty_driver_open(struct tty_struct *tty, struct file *filp)
{
	/*
	 * we actually want to allocate a new channel per open, per
	 * system arch.  HW gives more than plenty channels for a single
	 * system task to have its own channel to write trace data. This
	 * also removes a locking requirement for the actual write
	 * procedure.
	 */
	return tty_port_open(tty->port, tty, filp);
}

/**
 * pti_tty_driver_close()- close tty device and release Application
 * master, channel aperture ID to the PTI device via tty device.
 *
 * @tty: tty interface.
 * @filp: filp interface pased to tty_port_close() call.
 *
 * The main purpose of using the tty device interface is to route
 * syslog daemon messages to the PTI HW and out of the handheld platform
 * and to the Fido/Lauterbach device.
 */
static void pti_tty_driver_close(struct tty_struct *tty, struct file *filp)
{
	tty_port_close(tty->port, tty, filp);
}

/**
 * pti_tty_install()- Used to set up specific master-channels
 *		      to tty ports for organizational purposes when
 *		      tracing viewed from debuging tools.
 *
 * @driver: tty driver information.
 * @tty: tty struct containing pti information.
 *
 * Returns:
 *	0 for success
 *	otherwise, error
 */
static int pti_tty_install(struct tty_driver *driver, struct tty_struct *tty)
{
	int idx = tty->index;
	struct pti_tty *pti_tty_data;
	int ret = tty_standard_install(driver, tty);

	if (ret == 0) {
		pti_tty_data = kmalloc(sizeof(struct pti_tty), GFP_KERNEL);
		if (pti_tty_data == NULL)
			return -ENOMEM;

		if (idx == PTITTY_MINOR_START)
			pti_tty_data->mc = pti_request_masterchannel(0, NULL);
		else
			pti_tty_data->mc = pti_request_masterchannel(2, NULL);

		if (pti_tty_data->mc == NULL) {
			kfree(pti_tty_data);
			return -ENXIO;
		}
		tty->driver_data = pti_tty_data;
	}

	return ret;
}

/**
 * pti_tty_cleanup()- Used to de-allocate master-channel resources
 *		      tied to tty's of this driver.
 *
 * @tty: tty struct containing pti information.
 */
static void pti_tty_cleanup(struct tty_struct *tty)
{
	struct pti_tty *pti_tty_data = tty->driver_data;
	if (pti_tty_data == NULL)
		return;
	pti_release_masterchannel(pti_tty_data->mc);
	kfree(pti_tty_data);
	tty->driver_data = NULL;
}

/**
 * pti_tty_driver_write()-  Write trace debugging data through the char
 * interface to the PTI HW.  Part of the misc device implementation.
 *
 * @filp: Contains private data which is used to obtain
 *        master, channel write ID.
 * @data: trace data to be written.
 * @len:  # of byte to write.
 *
 * Returns:
 *	int, # of bytes written
 *	otherwise, error
 */
static int pti_tty_driver_write(struct tty_struct *tty,
	const unsigned char *buf, int len)
{
	struct pti_tty *pti_tty_data = tty->driver_data;
	if ((pti_tty_data != NULL) && (pti_tty_data->mc != NULL)) {
		pti_write_to_aperture(pti_tty_data->mc, (u8 *)buf, len, true);
		return len;
	}
	/*
	 * we can't write to the pti hardware if the private driver_data
	 * and the mc address is not there.
	 */
	else
		return -EFAULT;
}

/**
 * pti_tty_write_room()- Always returns 2048.
 *
 * @tty: contains tty info of the pti driver.
 */
static int pti_tty_write_room(struct tty_struct *tty)
{
	return 2048;
}

/**
 * pti_char_open()- Open an Application master, channel aperture
 * ID to the PTI device. Part of the misc device implementation.
 *
 * @inode: not used.
 * @filp:  Output- will have a masterchannel struct set containing
 *                 the allocated application PTI aperture write address.
 *
 * Returns:
 *	int, 0 for success
 *	otherwise, a fail value
 */
static int pti_char_open(struct inode *inode, struct file *filp)
{
	struct pti_masterchannel *mc;

	/*
	 * We really do want to fail immediately if
	 * pti_request_masterchannel() fails,
	 * before assigning the value to filp->private_data.
	 * Slightly easier to debug if this driver needs debugging.
	 */
	mc = pti_request_masterchannel(0, NULL);
	if (mc == NULL)
		return -ENOMEM;
	filp->private_data = mc;
	return 0;
}

/**
 * pti_char_release()-  Close a char channel to the PTI device. Part
 * of the misc device implementation.
 *
 * @inode: Not used in this implementaiton.
 * @filp:  Contains private_data that contains the master, channel
 *         ID to be released by the PTI device.
 *
 * Returns:
 *	always 0
 */
static int pti_char_release(struct inode *inode, struct file *filp)
{
	pti_release_masterchannel(filp->private_data);
	filp->private_data = NULL;
	return 0;
}

/**
 * pti_char_write()-  Write trace debugging data through the char
 * interface to the PTI HW.  Part of the misc device implementation.
 *
 * @filp:  Contains private data which is used to obtain
 *         master, channel write ID.
 * @data:  trace data to be written.
 * @len:   # of byte to write.
 * @ppose: Not used in this function implementation.
 *
 * Returns:
 *	int, # of bytes written
 *	otherwise, error value
 *
 * Notes: From side discussions with Alan Cox and experimenting
 * with PTI debug HW like Nokia's Fido box and Lauterbach
 * devices, 8192 byte write buffer used by USER_COPY_SIZE was
 * deemed an appropriate size for this type of usage with
 * debugging HW.
 */
static ssize_t pti_char_write(struct file *filp, const char __user *data,
			      size_t len, loff_t *ppose)
{
	struct pti_masterchannel *mc;
	void *kbuf;
	const char __user *tmp;
	size_t size = USER_COPY_SIZE;
	size_t n = 0;

	tmp = data;
	mc = filp->private_data;

	kbuf = kmalloc(size, GFP_KERNEL);
	if (kbuf == NULL)  {
		pr_err("%s(%d): buf allocation failed\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	do {
		if (len - n > USER_COPY_SIZE)
			size = USER_COPY_SIZE;
		else
			size = len - n;

		if (copy_from_user(kbuf, tmp, size)) {
			kfree(kbuf);
			return n ? n : -EFAULT;
		}

		pti_write_to_aperture(mc, kbuf, size, true);
		n  += size;
		tmp += size;

	} while (len > n);

	kfree(kbuf);
	return len;
}

static const struct tty_operations pti_tty_driver_ops = {
	.open		= pti_tty_driver_open,
	.close		= pti_tty_driver_close,
	.write		= pti_tty_driver_write,
	.write_room	= pti_tty_write_room,
	.install	= pti_tty_install,
	.cleanup	= pti_tty_cleanup
};

static const struct file_operations pti_char_driver_ops = {
	.owner		= THIS_MODULE,
	.write		= pti_char_write,
	.open		= pti_char_open,
	.release	= pti_char_release,
};

static struct miscdevice pti_char_driver = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= CHARNAME,
	.fops		= &pti_char_driver_ops
};

/**
 * pti_console_write()-  Write to the console that has been acquired.
 *
 * @c:   Not used in this implementaiton.
 * @buf: Data to be written.
 * @len: Length of buf.
 */
static void pti_console_write(struct console *c, const char *buf, unsigned len)
{
	static struct pti_masterchannel mc = {.master  = CONSOLE_ID,
					      .channel = 0};

	mc.channel = pti_console_channel;
	pti_console_channel = (pti_console_channel + 1) & 0x7f;

	pti_write_full_frame_to_aperture(&mc, buf, len);
}

/**
 * pti_console_device()-  Return the driver tty structure and set the
 *			  associated index implementation.
 *
 * @c:     Console device of the driver.
 * @index: index associated with c.
 *
 * Returns:
 *	always value of pti_tty_driver structure when this function
 *	is called.
 */
static struct tty_driver *pti_console_device(struct console *c, int *index)
{
	*index = c->index;
	return pti_tty_driver;
}

/**
 * pti_console_setup()-  Initialize console variables used by the driver.
 *
 * @c:     Not used.
 * @opts:  Not used.
 *
 * Returns:
 *	always 0.
 */
static int pti_console_setup(struct console *c, char *opts)
{
	pti_console_channel = 0;
	pti_control_channel = 0;
	return 0;
}

/*
 * pti_console struct, used to capture OS printk()'s and shift
 * out to the PTI device for debugging.  This cannot be
 * enabled upon boot because of the possibility of eating
 * any serial console printk's (race condition discovered).
 * The console should be enabled upon when the tty port is
 * used for the first time.  Since the primary purpose for
 * the tty port is to hook up syslog to it, the tty port
 * will be open for a really long time.
 */
static struct console pti_console = {
	.name		= TTYNAME,
	.write		= pti_console_write,
	.device		= pti_console_device,
	.setup		= pti_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= 0,
};

/**
 * pti_port_activate()- Used to start/initialize any items upon
 * first opening of tty_port().
 *
 * @port- The tty port number of the PTI device.
 * @tty-  The tty struct associated with this device.
 *
 * Returns:
 *	always returns 0
 *
 * Notes: The primary purpose of the PTI tty port 0 is to hook
 * the syslog daemon to it; thus this port will be open for a
 * very long time.
 */
static int pti_port_activate(struct tty_port *port, struct tty_struct *tty)
{
	if (port->tty->index == PTITTY_MINOR_START)
		console_start(&pti_console);
	return 0;
}

/**
 * pti_port_shutdown()- Used to stop/shutdown any items upon the
 * last tty port close.
 *
 * @port- The tty port number of the PTI device.
 *
 * Notes: The primary purpose of the PTI tty port 0 is to hook
 * the syslog daemon to it; thus this port will be open for a
 * very long time.
 */
static void pti_port_shutdown(struct tty_port *port)
{
	if (port->tty->index == PTITTY_MINOR_START)
		console_stop(&pti_console);
}

static const struct tty_port_operations tty_port_ops = {
	.activate = pti_port_activate,
	.shutdown = pti_port_shutdown,
};


#ifdef CONFIG_INTEL_SCU_IPC
/**
 * pti_scu_check()- Used to check whether the PTI is enabled on SCU
 *
 * Returns:
 *	0 if PTI is enabled
 *	otherwise, error value
 */
static int pti_scu_check(void)
{
	int retval;
	u8 smip_pti;

	retval = intel_scu_ipc_read_mip(&smip_pti, 1, SMIP_PTI_OFFSET, 1);
	if (retval) {
		pr_err("%s(%d): Mip read failed (retval = %d)\n",
		       __func__, __LINE__, retval);
		return retval;
	}
	if (!(smip_pti & SMIP_PTI_EN)) {
		pr_info("%s(%d): PTI disabled in MIP header\n",
			__func__, __LINE__);
		return -EPERM;
	}

	return 0;
}
#endif /* CONFIG_INTEL_SCU_IPC */

/*
 * Note the _probe() call sets everything up and ties the char and tty
 * to successfully detecting the PTI device on the pci bus.
 */

/**
 * pti_pci_probe()- Used to detect pti on the pci bus and set
 *		    things up in the driver.
 *
 * @pdev- pci_dev struct values for pti.
 * @ent-  pci_device_id struct for pti driver.
 *
 * Returns:
 *	0 for success
 *	otherwise, error
 */
static int pti_pci_probe(struct pci_dev *pdev,
		const struct pci_device_id *ent)
{
	unsigned int a;
	int retval = -EINVAL;

	dev_dbg(&pdev->dev, "%s %s(%d): PTI PCI ID %04x:%04x\n", __FILE__,
			__func__, __LINE__, pdev->vendor, pdev->device);

	retval = misc_register(&pti_char_driver);
	if (retval) {
		pr_err("%s(%d): CHAR registration failed of pti driver\n",
			__func__, __LINE__);
		pr_err("%s(%d): Error value returned: %d\n",
			__func__, __LINE__, retval);
		goto err;
	}

	retval = pci_enable_device(pdev);
	if (retval != 0) {
		dev_err(&pdev->dev,
			"%s: pci_enable_device() returned error %d\n",
			__func__, retval);
		goto err_unreg_misc;
	}

	drv_data = kzalloc(sizeof(*drv_data), GFP_KERNEL);
	if (drv_data == NULL) {
		retval = -ENOMEM;
		dev_err(&pdev->dev,
			"%s(%d): kmalloc() returned NULL memory.\n",
			__func__, __LINE__);
		goto err_disable_pci;
	}

	drv_data->pti_dev_info = (struct pti_device_info *)ent->driver_data;

#ifdef CONFIG_INTEL_SCU_IPC
	if (HAS_SCU_SECURE_MODE(drv_data)) {
		retval = pti_scu_check();
		if (retval != 0)
			goto err_free_dd;
	}
#endif /* CONFIG_INTEL_SCU_IPC */

	drv_data->pti_addr = pci_resource_start(pdev, GET_PCI_BAR(drv_data));

	retval = pci_request_region(pdev, GET_PCI_BAR(drv_data),
				    dev_name(&pdev->dev));
	if (retval != 0) {
		dev_err(&pdev->dev,
			"%s(%d): pci_request_region() returned error %d\n",
			__func__, __LINE__, retval);
		goto err_free_dd;
	}
	drv_data->aperture_base = drv_data->pti_addr+APERTURE_14;
	drv_data->pti_ioaddr =
		ioremap_nocache((u32)drv_data->aperture_base,
		APERTURE_LEN);
	if (!drv_data->pti_ioaddr) {
		retval = -ENOMEM;
		goto err_rel_reg;
	}

#ifdef CONFIG_INTEL_PTI_STM
	/* Initialize STM resources */
	if ((stm_enabled) && (stm_dev_init(pdev, &drv_data->stm) != 0)) {
		retval = -ENOMEM;
		goto err_rel_reg;
	}
#endif

	pci_set_drvdata(pdev, drv_data);

	for (a = 0; a < PTITTY_MINOR_NUM; a++) {
		struct tty_port *port = &drv_data->port[a];
		tty_port_init(port);
		port->ops = &tty_port_ops;

		tty_port_register_device(port, pti_tty_driver, a, &pdev->dev);
	}

	register_console(&pti_console);

	return 0;
err_rel_reg:
	pci_release_region(pdev, GET_PCI_BAR(drv_data));
err_free_dd:
	kfree(drv_data);
	drv_data = NULL;
err_disable_pci:
	pci_disable_device(pdev);
err_unreg_misc:
	misc_deregister(&pti_char_driver);
err:
	return retval;
}

/**
 * pti_pci_remove()- Driver exit method to remove PTI from
 *		   PCI bus.
 * @pdev: variable containing pci info of PTI.
 */
static void pti_pci_remove(struct pci_dev *pdev)
{
	struct pti_dev *drv_data = pci_get_drvdata(pdev);
	unsigned int a;

	unregister_console(&pti_console);

	for (a = 0; a < PTITTY_MINOR_NUM; a++) {
		tty_unregister_device(pti_tty_driver, a);
		tty_port_destroy(&drv_data->port[a]);
	}

#ifdef CONFIG_INTEL_PTI_STM
	if (stm_enabled)
		stm_dev_clean(pdev, &drv_data->stm);
#endif
	iounmap(drv_data->pti_ioaddr);
	pci_release_region(pdev, GET_PCI_BAR(drv_data));
	pci_set_drvdata(pdev, NULL);
	kfree(drv_data);
	pci_disable_device(pdev);

	misc_deregister(&pti_char_driver);
}

static struct pci_driver pti_pci_driver = {
	.name		= PCINAME,
	.id_table	= pci_ids,
	.probe		= pti_pci_probe,
	.remove		= pti_pci_remove,
};

/**
 *
 * pti_init()- Overall entry/init call to the pti driver.
 *             It starts the registration process with the kernel.
 *
 * Returns:
 *	int __init, 0 for success
 *	otherwise value is an error
 *
 */
static int __init pti_init(void)
{
	int retval = -EINVAL;

	/* First register module as tty device */

	pti_tty_driver = alloc_tty_driver(PTITTY_MINOR_NUM);
	if (pti_tty_driver == NULL) {
		pr_err("%s(%d): Memory allocation failed for ptiTTY driver\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	pti_tty_driver->driver_name		= DRIVERNAME;
	pti_tty_driver->name			= TTYNAME;
	pti_tty_driver->major			= 0;
	pti_tty_driver->minor_start		= PTITTY_MINOR_START;
	pti_tty_driver->type			= TTY_DRIVER_TYPE_SYSTEM;
	pti_tty_driver->subtype			= SYSTEM_TYPE_SYSCONS;
	pti_tty_driver->flags			= TTY_DRIVER_REAL_RAW |
						  TTY_DRIVER_DYNAMIC_DEV;
	pti_tty_driver->init_termios		= tty_std_termios;

	tty_set_operations(pti_tty_driver, &pti_tty_driver_ops);

	retval = tty_register_driver(pti_tty_driver);
	if (retval) {
		pr_err("%s(%d): TTY registration failed of pti driver\n",
			__func__, __LINE__);
		pr_err("%s(%d): Error value returned: %d\n",
			__func__, __LINE__, retval);

		goto put_tty;
	}

	retval = pci_register_driver(&pti_pci_driver);
	if (retval) {
		pr_err("%s(%d): PCI registration failed of pti driver\n",
			__func__, __LINE__);
		pr_err("%s(%d): Error value returned: %d\n",
			__func__, __LINE__, retval);
		goto unreg_tty;
	}

	return 0;
unreg_tty:
	tty_unregister_driver(pti_tty_driver);
put_tty:
	put_tty_driver(pti_tty_driver);
	pti_tty_driver = NULL;
	return retval;
}

/**
 * pti_exit()- Unregisters this module as a tty and pci driver.
 */
static void __exit pti_exit(void)
{
	tty_unregister_driver(pti_tty_driver);
	pci_unregister_driver(&pti_pci_driver);
	put_tty_driver(pti_tty_driver);
}

module_init(pti_init);
module_exit(pti_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ken Mills, Jay Freyensee");
MODULE_DESCRIPTION("PTI Driver");

