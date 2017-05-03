#ifndef __LINUX_FOCALTECH_FLASH_H__
#define __LINUX_FOCALTECH_FLASH_H__

#include <linux/i2c.h>
#include <linux/input/focaltech_mmi.h>

extern int fts_ctpm_auto_upgrade(struct i2c_client *client,
				const char *fw_name,
				const struct ft_ts_platform_data *pdata);
#endif

