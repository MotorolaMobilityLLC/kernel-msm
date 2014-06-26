#ifndef __INTEL_MID_POWERBTN_H__
#define __INTEL_MID_POWERBTN_H__

struct intel_msic_power_btn_platform_data {
	u32 pbstat;
	u16 pb_level;
	u16 irq_lvl1_mask;
	int (*irq_ack)(struct intel_msic_power_btn_platform_data *);
};

#define MSIC_PB_LEN	1
#define MSIC_PWRBTNM	(1 << 0)

#define BCOVE_PBIRQ		0x02
#define BCOVE_PBIRQMASK		0x0d

#define SCOVE_PBIRQ		0x1002
#define SCOVE_PBIRQMASK		0x100d

#endif
