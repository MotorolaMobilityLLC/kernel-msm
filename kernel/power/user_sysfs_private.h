#ifndef USER_SYSFS_PRIVATE_H
#define USER_SYSFS_PRIVATE_H

extern void tlmm_before_sleep_set_configs(void);
extern void tlmm_before_sleep_save_configs(void);

/* drivers/base/power/wakeup.c */
extern int wakelock_dump_active_info(char *buf, int size);

/* drivers/regulator/core.c */
extern void vreg_before_sleep_save_configs(void);
extern int vreg_dump_info(char *buf);
extern int vreg_before_sleep_dump_info(char *buf);

/* drivers/pinctrl/qcom/pinctrl-msm.c */
extern int tlmm_get_inout(unsigned gpio);
extern int tlmm_set_inout(unsigned gpio, unsigned val);
extern int tlmm_set_config(unsigned config);
extern int tlmm_get_config(unsigned gpio, unsigned *cfg);

#endif
