#ifndef USER_SYSFS_PRIVATE_H
#define USER_SYSFS_PRIVATE_H

extern void log_suspend_enter(void);
extern void log_suspend_exit(int error);

/* drivers/soc/qcom/rpm_stats.c */
extern void msm_rpmstats_get_reverved(u32 reserved[][4]);

extern void tlmm_before_sleep_set_configs(void);
extern void tlmm_before_sleep_save_configs(void);

/* drivers/regulator/core.c */
extern void vreg_before_sleep_save_configs(void);
extern int vreg_dump_info(char *buf);
extern int vreg_before_sleep_dump_info(char *buf);

/* drivers/pinctrl/pinctrl-msm-tlmm.c */
extern int tlmm_get_inout(unsigned gpio);
extern int tlmm_set_inout(unsigned gpio, unsigned val);
extern int tlmm_set_config(unsigned config);
extern int tlmm_get_config(unsigned gpio, unsigned *cfg);

#endif
