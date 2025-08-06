/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _DT_BINDINGS_QCOM_QPT_CHANNELS_H
#define _DT_BINDINGS_QCOM_QPT_CHANNELS_H

#define QPT_GEN1_CHAN_ID(sid, reg_id) (sid << 8 | reg_id)

#define QPT_MAYA_SID	0
#define QPT_F10_B_SID	1
#define QPT_F10_D_SID	3
#define QPT_F10_F_SID	5
#define QPT_F10_G_SID	6
#define QPT_F10_I_SID	8
#define QPT_F4_J_SID	9


#define QPT_S1_REG	0x9b
#define QPT_S2_REG	0x9e
#define QPT_S3_REG	0xa1
#define QPT_S4_REG	0xa4
#define QPT_S5_REG	0xa7
#define QPT_S6_REG	0xaa
#define QPT_S7_REG	0xad
#define QPT_S8_REG	0xb0
#define QPT_S9_REG	0xb3
#define QPT_S10_REG	0xb6
#define QPT_BOB1_REG	0xe5
#define QPT_BOB2_REG	0xe7

#endif /* _DT_BINDINGS_QCOM_QPT_CHANNELS_H */
