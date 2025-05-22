#ifndef __LPM_DBG_MMI_EVENT_H__
#define __LPM_DBG_MMI_EVENT_H__

#include <linux/platform_device.h>

typedef enum {
	SLEEP_TIME_TYPE_NONE = 0,
	SLEEP_TIME_TYPE_AP,
	SLEEP_TIME_TYPE_MD,
	SLEEP_TIME_TYPE_SCP,
	SLEEP_TIME_TYPE_MAX = SLEEP_TIME_TYPE_SCP,
} SLEEP_TIME_TYPE;

struct mmi_lpm_dbg_ops {
	int (*store_sleep_time)(SLEEP_TIME_TYPE type, unsigned long long time);
	int (*check_scp_sleep_status)(int sleep_count);
	int (*check_md_sleep_status)(int duration);
};

int mmi_lpm_dbg_ops_register(struct mmi_lpm_dbg_ops *ops);
int mmi_lpm_dbg_store_sleep_time(SLEEP_TIME_TYPE type, unsigned long long time);

#endif/*__LPM_DBG_MMI_EVENT_H__*/
