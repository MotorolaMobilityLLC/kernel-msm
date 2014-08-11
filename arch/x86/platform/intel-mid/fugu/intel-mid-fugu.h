/*
 * intel-mid-fugu.h: ASUSTek fugu specific setup code
 *
 * (C) Copyright 2014 ASUSTek Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _X86_INTEL_MID_FUGU_H
#define _X86_INTEL_MID_FUGU_H

/* Table signatures reserved by the SFI specification */
#define SFI_SIG_OEM1 "OEM1"

/* OEM1 table */
struct sfi_table_oem1 {
	struct sfi_table_header header;
	u8	ifwi_rc;		/* 0: shipping version,
					 * 0xff: factory version */
	u8      hardware_id;            /* hardware revision */
	u8      project_id;             /* project identification */
	u8      ram_id;                 /* ram size variants */
} __packed;
#endif /* _X86_INTEL_MID_FUGU_H */
