#ifndef XHCI_USH_HSIC_PCI_h
#define XHCI_USH_HSIC_PCI_h

#include <linux/usb.h>
#include <linux/wakelock.h>

/* CHT ID MUX register in USH MMIO */
#define DUAL_ROLE_CFG0			0x80D8
#define SW_IDPIN_EN			(1 << 21)
#define SW_IDPIN			(1 << 20)

#define DUAL_ROLE_CFG1			0x80DC
#define SUS				(1 << 29)

#define HSIC_HUB_RESET_TIME   10
#define HSIC_ENABLE_SIZE      2
#define HSIC_DURATION_SIZE    7
#define HSIC_DELAY_SIZE       8

#define HSIC_AUTOSUSPEND                     0
#define HSIC_PORT_INACTIVITYDURATION              500
#define HSIC_BUS_INACTIVITYDURATION              500
#define HSIC_REMOTEWAKEUP                       1

#define USH_PCI_ID                     0x0F35
#define USH_REENUM_DELAY_FFRD8_PR0     600000
#define USH_REENUM_DELAY               20000

enum wlock_state {
	UNLOCKED,
	LOCKED
};

enum s3_state {
	RESUMED,
	RESUMING,
	SUSPENDED,
	SUSPENDING
};

struct ush_hsic_priv {
	struct delayed_work  hsic_aux;
	wait_queue_head_t    aux_wq;
	struct mutex         hsic_mutex;
	struct mutex         wlock_mutex;
	unsigned             hsic_mutex_init:1;
	unsigned             aux_wq_init:1;
	unsigned             hsic_aux_irq_enable:1;
	unsigned             hsic_wakeup_irq_enable:1;
	unsigned             hsic_aux_finish:1;
	unsigned             hsic_enable_created:1;
	unsigned             hsic_lock_init:1;
	unsigned             port_disconnect:1;

	unsigned             remoteWakeup_enable;
	unsigned             autosuspend_enable;
	unsigned             aux_gpio;
	unsigned             wakeup_gpio;
	unsigned             port_inactivityDuration;
	unsigned             bus_inactivityDuration;
	unsigned             reenumeration_delay;
	spinlock_t           hsic_lock;
	struct	wake_lock    resume_wake_lock;
	/* Root hub device */
	struct usb_device           *rh_dev;
	struct usb_device           *modem_dev;
	struct workqueue_struct     *work_queue;
	struct work_struct          wakeup_work;
	struct notifier_block       hsicdev_nb;
	struct notifier_block       hsic_pm_nb;
	struct notifier_block       hsic_s3_entry_nb;
	struct wake_lock            s3_wake_lock;
	enum wlock_state            s3_wlock_state;
	enum s3_state               s3_rt_state;
	int		hsic_port_num;
};

enum {
	PROBE,
	REMOVE
};

struct ush_hsic_pdata {
	unsigned                has_modem:1;     /* has modem or not */
	unsigned                enabled:1;       /* enable flag */
	unsigned		no_power_gate:1; /* no power gating on d3 */
	int                     aux_gpio;
	int                     wakeup_gpio;
	int                     reenum_delay;
	int			hsic_port_num;
};

static int hsic_notify(struct notifier_block *self,
		unsigned long action, void *dev);
#endif
