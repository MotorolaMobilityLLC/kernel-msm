/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file sysEntryFunc.h contains module entry functions definitions
 * Author:      V. K. Kandarpa
 * Date:        04/13/2002
 * History:-
 * Date         Modified by    Modification Information
 * --------------------------------------------------------------------
 */
#ifndef __SYS_ENTRY_FUNC_H
#define __SYS_ENTRY_FUNC_H

#include "aniGlobal.h"

extern tSirRetStatus sysInitGlobals(tpAniSirGlobal);
extern void sysBbtEntry(tANI_U32 dummy);
extern void sysSchEntry(tANI_U32 dummy);
extern void sysPmmEntry(tANI_U32 dummy);
extern void sysDphEntry(tANI_U32 dummy);
extern void sysLimEntry(tANI_U32 dummy);
extern void sysMmhEntry(tANI_U32 dummy);
extern void sysMntEntry(tANI_U32 dummy);
extern void sysHalEntry(tANI_U32 dummy);
extern void sysNimPttEntry(tANI_U32 dummy);

#endif // __SYS_ENTRY_FUNC_H
