#ifndef __LPM_DBG_MMI_EVENT_H__
#define __LPM_DBG_MMI_EVENT_H__

#include <linux/platform_device.h>
#include <linux/notifier.h>

typedef enum {
	SLEEP_TIME_TYPE_NONE = 0,
	SLEEP_TIME_TYPE_AP,
	SLEEP_TIME_TYPE_MD,
	SLEEP_TIME_TYPE_SCP,
	SLEEP_TIME_TYPE_MAX = SLEEP_TIME_TYPE_SCP,
} SLEEP_TIME_TYPE;

int mmi_lpm_dbg_register_notifier(struct notifier_block *nb);

#endif/*__LPM_DBG_MMI_EVENT_H__*/
