#ifndef __INTEL_MID_PMIC_H__
#define __INTEL_MID_PMIC_H__

#ifdef CONFIG_INTEL_MID_PMIC
int intel_mid_pmic_readb(int reg);
int intel_mid_pmic_writeb(int reg, u8 val);
int intel_mid_pmic_setb(int reg, u8 mask);
int intel_mid_pmic_clearb(int reg, u8 mask);
int intel_mid_pmic_update(int reg, u8 val, u8 mask);
int intel_mid_pmic_set_pdata(const char *name, void *data, int len);
struct device *intel_mid_pmic_dev(void);
#else
static inline int intel_mid_pmic_readb(int reg) { return 0; }
static inline int intel_mid_pmic_writeb(int reg, u8 val) { return 0; }
static inline int intel_mid_pmic_setb(int reg, u8 mask) { return 0; }
static inline int intel_mid_pmic_clearb(int reg, u8 mask) { return 0; }
static inline int intel_mid_pmic_set_pdata(const char *name,
				void *data, int len) {
	return 0;
}
static inline struct device *intel_mid_pmic_dev(void) { return NULL; }
#endif
#endif

