/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This is the private header file for CFG module.
 *
 * Author:        Kevin Nguyen
 * Date:        03/20/02
 * History:-
 * 03/20/02        Created.
 * --------------------------------------------------------------------
 *
 */

#ifndef __CFGDEF_H
#define __CFGDEF_H

/*
 * CFG Control Flag definitions
 */
#define CFG_CTL_VALID         0x00010000
#define CFG_CTL_RE            0x00020000
#define CFG_CTL_WE            0x00040000
#define CFG_CTL_INT           0x00080000
#define CFG_CTL_SAVE          0x00100000
#define CFG_CTL_RESTART       0x00200000
#define CFG_CTL_RELOAD        0x00400000
#define CFG_CTL_NTF_PHY       0x00800000
#define CFG_CTL_NTF_MAC       0x01000000
#define CFG_CTL_NTF_LOG       0x02000000
#define CFG_CTL_NTF_HAL       0x04000000
#define CFG_CTL_NTF_DPH       0x08000000
#define CFG_CTL_NTF_ARQ       0x10000000
#define CFG_CTL_NTF_SCH       0x20000000
#define CFG_CTL_NTF_LIM       0x40000000
#define CFG_CTL_NTF_HDD       0x80000000
#define CFG_CTL_NTF_MASK      0xFFE00000

#define CFG_CTL_NTF_TFP       CFG_CTL_NTF_MAC
#define CFG_CTL_NTF_RHP       CFG_CTL_NTF_MAC
#define CFG_CTL_NTF_RFP       CFG_CTL_NTF_MAC
#define CFG_CTL_NTF_SP        CFG_CTL_NTF_MAC
#define CFG_CTL_NTF_HW        (CFG_CTL_NTF_MAC | CFG_CTL_NTF_PHY)

#define CFG_BUF_INDX_MASK     0x00000fff
#define CFG_SEM_INDX_MASK     0x0000f000
#define CFG_SEM_INDX_SHIFT    12


#endif /* __CFGDEF_H */




