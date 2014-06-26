#ifndef _ASM_X86_INTEL_MIP_H_
#define  _ASM_X86_INTEL_MIP_H_

#include <asm/intel-mid.h>

/* SMIP property related definitions */
#define SCU_MIP_DEV_NAME		"intel_scu_mip"
#define SMIP_NUM_CONFIG_PROPS		6
#define SMIP_MAX_PROP_LEN		4

enum platform_prop {
	USB_COMPLIANCE,
	CHARGE_TERMINATION,
	SHUTDOWN_METHODOLOGY,
	MOS_TRANS_CAPACITY,
	NFC_RESV_CAPACITY,
	TEMP_CRIT_SHUTDOWN,
};

struct smip_platform_prop {
	unsigned int offset;
	unsigned int len;
	bool is_bit_field;
	unsigned int mask;
};

struct scu_mip_platform_data {
	struct smip_platform_prop smip_prop[SMIP_NUM_CONFIG_PROPS];
};

int get_smip_property_by_name(enum platform_prop);
#endif
