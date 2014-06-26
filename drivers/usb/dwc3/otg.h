/*
 * Intel Penwell USB OTG transceiver driver
 * Copyright (C) 2009 - 2010, Intel Corporation.
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

#ifndef __DWC3_OTG_H
#define __DWC3_OTG_H

#include <linux/usb.h>
#include <linux/device.h>
#include <linux/compiler.h>
#include <linux/power_supply.h>
#include <linux/usb/gadget.h>
#include <linux/usb/hcd.h>
#include <linux/usb/ulpi.h>


struct dwc_device_par {
	void __iomem *io_addr;
	int len;
};

#define DWC3_DEVICE_NAME "dwc3-device"
#define DWC3_HOST_NAME "dwc3-host"
#define GADGET_DEVID 1
#define HOST_DEVID 2
#define DRIVER_VERSION "0.1"

#ifdef CONFIG_USB_DWC3_OTG_DEBUG
#define DWC_OTG_DEBUG 1
#else
#define DWC_OTG_DEBUG 0
#endif

#define otg_dbg(d, fmt, args...)  \
	do { dev_dbg((d)->dev, \
			"%s(): " fmt , __func__, ## args); } while (0)
#define otg_vdbg(d, fmt, args...)  \
	do { if (DWC_OTG_DEBUG) dev_dbg((d)->dev, \
			"%s(): " fmt , __func__, ## args); } while (0)
#define otg_err(d, fmt, args...)  \
	do { dev_err((d)->dev, \
			"%s(): " fmt , __func__, ## args); } while (0)
#define otg_warn(d, fmt, args...)  \
	do { dev_warn((d)->dev, \
			"%s(): " fmt , __func__, ## args); } while (0)
#define otg_info(d, fmt, args...)  \
	do { dev_info((d)->dev, \
			"%s(): " fmt , __func__, ## args); } while (0)

#ifdef DEBUG
#define otg_write(o, reg, val)	do { \
		otg_dbg(o, "OTG_WRITE: reg=0x%05x, val=0x%08x\n", reg, val); \
		writel(val, ((void *)((o)->usb2_phy.io_priv)) + reg);	\
	} while (0)

#define otg_read(o, reg) ({ \
		u32 __r; \
		__r = readl(((void *)((o)->usb2_phy.io_priv)) + reg);	\
		otg_dbg(o, "OTG_READ: reg=0x%05x, val=0x%08x\n", reg, __r); \
		__r;							\
	})
#else
#define otg_write(o, reg, val) \
		writel(val, ((void *)((o)->usb2_phy.io_priv)) + reg);

#define otg_read(o, reg)	({ \
		readl(((void *)((o)->usb2_phy.io_priv)) + reg); \
	})
#endif

#define GUSB2PHYCFG0				0xc200
#define GUSB2PHYCFG_SUS_PHY                     0x40
#define GUSB2PHYCFG_PHYSOFTRST (1 << 31)
#define GUSB2PHYCFG_ULPI_AUTO_RESUME (1 << 15)

#define EXTEND_ULPI_REGISTER_ACCESS_MASK	0xC0
#define GUSB2PHYACC0	0xc280
#define GUSB2PHYACC0_DISULPIDRVR  (1 << 26)
#define GUSB2PHYACC0_NEWREGREQ  (1 << 25)
#define GUSB2PHYACC0_VSTSDONE  (1 << 24)
#define GUSB2PHYACC0_VSTSBSY  (1 << 23)
#define GUSB2PHYACC0_REGWR  (1 << 22)
#define GUSB2PHYACC0_REGADDR(v)  ((v & 0x3F) << 16)
#define GUSB2PHYACC0_EXTREGADDR(v)  ((v & 0x3F) << 8)
#define GUSB2PHYACC0_VCTRL(v)  ((v & 0xFF) << 8)
#define GUSB2PHYACC0_REGDATA(v)  (v & 0xFF)
#define GUSB2PHYACC0_REGDATA_MASK  0xFF

#define GUSB3PIPECTL0                           0xc2c0
#define GUSB3PIPECTL_SUS_EN                     0x20000
#define GUSB3PIPE_DISRXDETP3                    (1 << 28)
#define GUSB3PIPECTL_PHYSOFTRST (1 << 31)

#define GHWPARAMS6				0xc158
#define GHWPARAMS6_SRP_SUPPORT_ENABLED		0x0400
#define GHWPARAMS6_HNP_SUPPORT_ENABLED		0x0800
#define GHWPARAMS6_ADP_SUPPORT_ENABLED		0x1000

#define GUCTL 0xC12C
#define GUCTL_CMDEVADDR		(1 << 15)

#define GCTL 0xc110
#define GCTL_PRT_CAP_DIR 0x3000
#define GCTL_PRT_CAP_DIR_SHIFT 12
#define GCTL_PRT_CAP_DIR_HOST 1
#define GCTL_PRT_CAP_DIR_DEV 2
#define GCTL_PRT_CAP_DIR_OTG 3
#define GCTL_GBL_HIBERNATION_EN 0x2
#define GCTL_CORESOFTRESET (1 << 11)
#define GCTL_PWRDNSCALE(x) (x << 19)
#define GCTL_PWRDNSCALE_MASK (0x1fff << 19)

#define OCFG					0xcc00
#define OCFG_SRP_CAP				0x01
#define OCFG_SRP_CAP_SHIFT			0
#define OCFG_HNP_CAP				0x02
#define OCFG_HNP_CAP_SHIFT			1
#define OCFG_OTG_VERSION			0x04
#define OCFG_OTG_VERSION_SHIFT			2

#define GCTL					0xc110
#define OCTL					0xcc04
#define OCTL_HST_SET_HNP_EN			0x01
#define OCTL_HST_SET_HNP_EN_SHIFT		0
#define OCTL_DEV_SET_HNP_EN			0x02
#define OCTL_DEV_SET_HNP_EN_SHIFT		1
#define OCTL_TERM_SEL_DL_PULSE			0x04
#define OCTL_TERM_SEL_DL_PULSE_SHIFT		2
#define OCTL_SES_REQ				0x08
#define OCTL_SES_REQ_SHIFT			3
#define OCTL_HNP_REQ				0x10
#define OCTL_HNP_REQ_SHIFT			4
#define OCTL_PRT_PWR_CTL			0x20
#define OCTL_PRT_PWR_CTL_SHIFT			5
#define OCTL_PERI_MODE				0x40
#define OCTL_PERI_MODE_SHIFT			6

#define OEVT					0xcc08
#define OEVT_ERR				0x00000001
#define OEVT_ERR_SHIFT				0
#define OEVT_SES_REQ_SCS			0x00000002
#define OEVT_SES_REQ_SCS_SHIFT			1
#define OEVT_HST_NEG_SCS			0x00000004
#define OEVT_HST_NEG_SCS_SHIFT			2
#define OEVT_B_SES_VLD_EVT			0x00000008
#define OEVT_B_SES_VLD_EVT_SHIFT		3
#define OEVT_B_DEV_VBUS_CHNG_EVNT		0x00000100
#define OEVT_B_DEV_VBUS_CHNG_EVNT_SHIFT		8
#define OEVT_B_DEV_SES_VLD_DET_EVNT		0x00000200
#define OEVT_B_DEV_SES_VLD_DET_EVNT_SHIFT	9
#define OEVT_B_DEV_HNP_CHNG_EVNT		0x00000400
#define OEVT_B_DEV_HNP_CHNG_EVNT_SHIFT		10
#define OEVT_B_DEV_B_HOST_END_EVNT		0x00000800
#define OEVT_B_DEV_B_HOST_END_EVNT_SHIFT	11
#define OEVT_A_DEV_SESS_END_DET_EVNT		0x00010000
#define OEVT_A_DEV_SESS_END_DET_EVNT_SHIFT	16
#define OEVT_A_DEV_SRP_DET_EVNT			0x00020000
#define OEVT_A_DEV_SRP_DET_EVNT_SHIFT		17
#define OEVT_A_DEV_HNP_CHNG_EVNT		0x00040000
#define OEVT_A_DEV_HNP_CHNG_EVNT_SHIFT		18
#define OEVT_A_DEV_HOST_EVNT			0x00080000
#define OEVT_A_DEV_HOST_EVNT_SHIFT		19
#define OEVT_A_DEV_B_DEV_HOST_END_EVNT		0x00100000
#define OEVT_A_DEV_B_DEV_HOST_END_EVNT_SHIFT	20
#define OEVT_HOST_ROLE_REQ_INIT_EVNT            0x00400000
#define OEVT_HOST_ROLE_REQ_INIT_EVNT_SHIFT      22
#define OEVT_HOST_ROLE_REQ_CONFIRM_EVNT         0x00800000
#define OEVT_HOST_ROLE_REQ_CONFIRM_EVNT_SHIFT   23
#define OEVT_CONN_ID_STS_CHNG_EVNT		0x01000000
#define OEVT_CONN_ID_STS_CHNG_EVNT_SHIFT	24
#define OEVT_DEV_MOD_EVNT			0x80000000
#define OEVT_DEV_MOD_EVNT_SHIFT			31

#define OEVTEN					0xcc0c

#define OEVT_ALL (OEVT_CONN_ID_STS_CHNG_EVNT | \
		OEVT_HOST_ROLE_REQ_INIT_EVNT | \
		OEVT_HOST_ROLE_REQ_CONFIRM_EVNT | \
		OEVT_A_DEV_B_DEV_HOST_END_EVNT | \
		OEVT_A_DEV_HOST_EVNT | \
		OEVT_A_DEV_HNP_CHNG_EVNT | \
		OEVT_A_DEV_SRP_DET_EVNT | \
		OEVT_A_DEV_SESS_END_DET_EVNT | \
		OEVT_B_DEV_B_HOST_END_EVNT | \
		OEVT_B_DEV_HNP_CHNG_EVNT | \
		OEVT_B_DEV_SES_VLD_DET_EVNT | \
		OEVT_B_DEV_VBUS_CHNG_EVNT)

#define OSTS					0xcc10
#define OSTS_CONN_ID_STS			0x0001
#define OSTS_CONN_ID_STS_SHIFT			0
#define OSTS_A_SES_VLD				0x0002
#define OSTS_A_SES_VLD_SHIFT			1
#define OSTS_B_SES_VLD				0x0004
#define OSTS_B_SES_VLD_SHIFT			2
#define OSTS_XHCI_PRT_PWR			0x0008
#define OSTS_XHCI_PRT_PWR_SHIFT			3
#define OSTS_PERIP_MODE				0x0010
#define OSTS_PERIP_MODE_SHIFT			4
#define OSTS_OTG_STATES				0x0f00
#define OSTS_OTG_STATE_SHIFT			8

#define ADPCFG					0xcc20
#define ADPCFG_PRB_DSCHGS			0x0c000000
#define ADPCFG_PRB_DSCHG_SHIFT			26
#define ADPCFG_PRB_DELTAS			0x30000000
#define ADPCFG_PRB_DELTA_SHIFT			28
#define ADPCFG_PRB_PERS				0xc0000000
#define ADPCFG_PRB_PER_SHIFT			30

#define ADPCTL					0xcc24
#define ADPCTL_WB				0x01000000
#define ADPCTL_WB_SHIFT				24
#define ADPCTL_ADP_RES				0x02000000
#define ADPCTL_ADP_RES_SHIFT			25
#define ADPCTL_ADP_EN				0x04000000
#define ADPCTL_ADP_EN_SHIFT			26
#define ADPCTL_ENA_SNS				0x08000000
#define ADPCTL_ENA_SNS_SHIFT			27
#define ADPCTL_ENA_PRB				0x10000000
#define ADPCTL_ENA_PRB_SHIFT			28

#define ADPEVT					0xcc28
#define ADPEVT_RTIM_EVNTS			0x000007ff
#define ADPEVT_RTIM_EVNT_SHIFT			0
#define ADPEVT_ADP_RST_CMPLT_EVNT		0x02000000
#define ADPEVT_ADP_RST_CMPLT_EVNT_SHIFT		25
#define ADPEVT_ADP_TMOUT_EVNT			0x04000000
#define ADPEVT_ADP_TMOUT_EVNT_SHIFT		26
#define ADPEVT_ADP_SNS_EVNT			0x08000000
#define ADPEVT_ADP_SNS_EVNT_SHIFT		27
#define ADPEVT_ADP_PRB_EVNT			0x10000000
#define ADPEVT_ADP_PRB_EVNT_SHIFT		28

#define ADPEVTEN				0xcc2c
#define ADPEVTEN_ACC_DONE_EN			0x01000000
#define ADPEVTEN_ACC_DONE_EN_SHIFT		24
#define ADPEVTEN_ADP_RST_CMPLT_EVNT_EN		0x02000000
#define ADPEVTEN_ADP_RST_CMPLT_EVNT_EN_SHIFT	25
#define ADPEVTEN_ADP_TMOUT_EVNT_EN		0x04000000
#define ADPEVTEN_ADP_TMOUT_EVNT_EN_SHIFT	26
#define ADPEVTEN_ADP_SNS_EVNT_EN		0x08000000
#define ADPEVTEN_ADP_SNS_EVNT_EN_SHIFT		27
#define ADPEVTEN_ADP_PRB_EVNT_EN		0x10000000
#define ADPEVTEN_ADP_PRB_EVNT_EN_SHIFT		28

#define RID_A		0x01
#define RID_B		0x02
#define RID_C		0x03
#define RID_FLOAT	0x04
#define RID_GND		0x05
#define RID_UNKNOWN	0x00

/** The states for the OTG driver */
enum dwc_otg_state {
	DWC_STATE_INVALID = -1,

	/** The initial state, check the connector
	 * id status and determine what mode
	 * (A-device or B-device) to operate in. */
	DWC_STATE_B_IDLE = 0,

	/* A-Host states */
	DWC_STATE_A_PROBE,
	DWC_STATE_A_HOST,
	DWC_STATE_A_HNP_INIT,

	/* A-Peripheral states */
	DWC_STATE_A_PERIPHERAL,

	/* B-Peripheral states */
	DWC_STATE_B_SENSE,
	DWC_STATE_B_PROBE,
	DWC_STATE_B_PERIPHERAL,
	DWC_STATE_B_HNP_INIT,

	/* B-Host states */
	DWC_STATE_B_HOST,

	/* RSP */
	DWC_STATE_B_RSP_INIT,

	/* USB charger detection */
	DWC_STATE_CHARGER_DETECTION,

	/* VBUS */
	DWC_STATE_WAIT_VBUS_RAISE,
	DWC_STATE_WAIT_VBUS_FALL,

	/* Charging*/
	DWC_STATE_CHARGING,

	/* Exit */
	DWC_STATE_EXIT,
	DWC_STATE_TERMINATED
};

/** The main structure to keep track of OTG driver state. */
struct dwc_otg2 {
	/** OTG transceiver */
	struct usb_otg	otg;
	struct usb_phy	usb2_phy;
	struct usb_phy	usb3_phy;
	struct device		*dev;
	int irqnum;

	int main_wakeup_needed;
	struct task_struct *main_thread;
	wait_queue_head_t main_wq;

	spinlock_t lock;

	/* Events */
	u32 otg_events;
	u32 user_events;

	/** User space ID switch event */
#define USER_ID_A_CHANGE_EVENT 0x01
#define USER_ID_B_CHANGE_EVENT 0x02
	/** a_bus_drop event from userspace */
#define USER_A_BUS_DROP 0x40

	/* States */
	enum dwc_otg_state prev;
	enum dwc_otg_state state;
	struct platform_device *host;
	struct platform_device *gadget;

	/* Charger detection */
	struct power_supply_cable_props charging_cap;
	struct notifier_block nb;

	/* Interfaces between host/device driver */
	int (*start_host) (struct usb_hcd *hcd);
	int (*stop_host) (struct usb_hcd *hcd);
	int (*start_device)(struct usb_gadget *);
	int (*stop_device)(struct usb_gadget *);
	int (*vbus_draw) (struct usb_gadget *, unsigned ma);

	/* host driver suspend/resume flow callback which
	 * need host driver to register them if need call by
	 * otg driver.*/
	int (*suspend_host) (struct usb_hcd *hcd);
	int (*resume_host) (struct usb_hcd *hcd);

	/* Vendor driver private date */
	void *otg_data;
};

#define sleep_main_thread_until_condition_timeout(otg, condition, msecs) ({ \
		int __timeout = msecs;				\
		while (!(condition)) {				\
			otg_dbg(otg, "  ... sleeping for %d\n", __timeout); \
			__timeout = sleep_main_thread_timeout(otg, __timeout); \
			if (__timeout <= 0) {			\
				break;				\
			}					\
		}						\
		__timeout;					\
	})

#define sleep_main_thread_until_condition(otg, condition) ({	\
		int __rc = 0;					\
		do {						\
			__rc = sleep_main_thread_until_condition_timeout(otg, \
			condition, 50000); \
		} while (__rc == 0);				\
		__rc;						\
	})

#define VBUS_TIMEOUT	20
#define PCI_DEVICE_ID_DWC 0x119E
#define PCI_DEVICE_ID_DWC_VLV 0x0F37
#define PCI_DEVICE_ID_DWC_CHT 0x22B7

enum dwc3_otg_mode {
	DWC3_DEVICE_ONLY,
	DWC3_HOST_ONLY,
	DWC3_DRD,
};

enum driver_bus_type {
	DWC3_PLAT,
	DWC3_PCI,
};

struct dwc3_otg_hw_ops {
	enum dwc3_otg_mode mode;
	enum driver_bus_type bus;

	int (*set_power)(struct usb_phy *_otg, unsigned ma);
	int (*platform_init)(struct dwc_otg2 *otg);
	int (*otg_notifier_handler)(struct notifier_block *nb,
			unsigned long event, void *data);
	int (*prepare_start_peripheral)(struct dwc_otg2 *otg);
	int (*prepare_start_host)(struct dwc_otg2 *otg);
	int (*after_stop_peripheral)(struct dwc_otg2 *otg);
	int (*after_stop_host)(struct dwc_otg2 *otg);
	int (*b_idle)(struct dwc_otg2 *otg);
	int (*do_charging)(struct dwc_otg2 *otg);
	int (*notify_charger_type)(struct dwc_otg2 *otg,
			enum power_supply_charger_event event);
	enum power_supply_charger_cable_type
		(*get_charger_type)(struct dwc_otg2 *otg);
	int (*enable_vbus)(struct dwc_otg2 *otg, int enable);
	int (*get_id)(struct dwc_otg2 *otg);

	int (*idle)(struct dwc_otg2 *otg);
	int (*suspend)(struct dwc_otg2 *otg);
	int (*resume)(struct dwc_otg2 *otg);
};

#define OTG_USB2_0MA				0xfff0
#define OTG_USB2_100MA				0xfff1
#define OTG_USB3_150MA				0xfff2
#define OTG_USB2_500MA				0xfff3
#define OTG_USB3_900MA				0xfff4
#define OTG_DEVICE_SUSPEND			0xfffe
#define OTG_DEVICE_RESUME			0xffff

void dwc3_wakeup_otg_thread(struct dwc_otg2 *otg);
struct dwc_otg2 *dwc3_get_otg(void);
int dwc3_otg_register(struct dwc3_otg_hw_ops *pdata);
int dwc3_otg_unregister(struct dwc3_otg_hw_ops *pdata);
int dwc3_is_cht(void);
#endif /* __DWC3_OTG_H */
