/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 * Copyright (c) 2011-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Woodside Networks, Inc proprietary. All rights reserved
 * sysStartup.h: System startup header file.
 * Author:  V. K. Kandarpa
 * Date:    01/29/2002
 *
 * History:-
 * Date     Modified by         Modification Information
 * --------------------------------------------------------------------------
 *
 */

# ifndef __SYSSTARTUP_H
# define __SYSSTARTUP_H

#include "sirParams.h"

/* Defines */

/* Function */

extern void sysMACCleanup(void *);
extern tSirRetStatus sysBbtProcessMessageCore(struct sAniSirGlobal *, tpSirMsgQ,
                                               tANI_U32, tANI_U32);


# endif /* __SYSSTARTUP_H */
