#ifndef _MMI_SYS_TEMP_IOCTL_H
#define _MMI_SYS_TEMP_IOCTL_H

#include <linux/ioctl.h>

#define MMI_SYS_TEMP_IOCTL_NAME "mmi_sys_temp"
#define MMI_SYS_TEMP_NAME_LENGTH 20

struct __attribute__((__packed__)) mmi_sys_temp_ioctl {
	int temperature;
	char name[MMI_SYS_TEMP_NAME_LENGTH];
};

enum {
	MMI_SYS_TEMP_SET = 0x00,
	MMI_SYS_TEMP_MAX_NUM,
};

#define MMI_SYS_TEMP_MAGIC_NUM 0xDF


#define MMI_SYS_TEMP_SET_TEMP _IOW(MMI_SYS_TEMP_MAGIC_NUM, \
				   MMI_SYS_TEMP_SET, \
				   struct mmi_sys_temp_ioctl)

#endif
