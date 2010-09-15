/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
cp15_registers.h

DESCRIPTION: define macros for reading and writing to the cp registers
for the ARMv7

REV/DATE:  Fri Mar 18 15:54:32 EST 2005
*/

#ifndef __cp15_registers__
#define __cp15_registers__

#include "mcrmrc.h"

#define WCP15_SDER(reg)			MCR15(reg, 0, c1, c1, 1)
/*
* Performance Monitor Registers
*/
#define WCP15_PMACTLR(reg)		MCR15(reg, 0, c9, c15, 5)
#define WCP15_PMCCNTCR(reg)		MCR15(reg, 0, c9, c15, 2)
#define WCP15_PMCCNTR(reg)		MCR15(reg, 0, c9, c13, 0)
#define WCP15_PMCCNTSR(reg)		MCR15(reg, 0, c9, c13, 3)
#define WCP15_PMCNTENCLR(reg)		MCR15(reg, 0, c9, c12, 2)
#define WCP15_PMCNTENSET(reg)		MCR15(reg, 0, c9, c12, 1)
#define WCP15_PMCR(reg)			MCR15(reg, 0, c9, c12, 0)
#define WCP15_PMINTENCLR(reg)		MCR15(reg, 0, c9, c14, 2)
#define WCP15_PMINTENSET(reg)		MCR15(reg, 0, c9, c14, 1)
#define WCP15_PMOVSR(reg)		MCR15(reg, 0, c9, c12, 3)
#define WCP15_PMRLDR(reg)		MCR15(reg, 0, c9, c15, 4)
#define WCP15_PMSELR(reg)		MCR15(reg, 0, c9, c12, 5)
#define WCP15_PMSWINC(reg)		MCR15(reg, 0, c9, c12, 4)
#define WCP15_PMUSERENR(reg)		MCR15(reg, 0, c9, c14, 0)
#define WCP15_PMXEVCNTCR(reg)		MCR15(reg, 0, c9, c15, 0)
#define WCP15_PMXEVCNTR(reg)		MCR15(reg, 0, c9, c13, 2)
#define WCP15_PMXEVCNTSR(reg)		MCR15(reg, 0, c9, c15, 1)
#define WCP15_PMXEVTYPER(reg)		MCR15(reg, 0, c9, c13, 1)
#define WCP15_LPM0EVTYPER(reg)		MCR15(reg, 0, c15, c0, 0)
#define WCP15_LPM1EVTYPER(reg)		MCR15(reg, 1, c15, c0, 0)
#define WCP15_LPM2EVTYPER(reg)		MCR15(reg, 2, c15, c0, 0)
#define WCP15_LPM3EVTYPER(reg)		MCR15(reg, 3, c15, c0, 0)
#define WCP15_L2LPMEVTYPER(reg)		MCR15(reg, 3, c15, c2, 0)
#define WCP15_VLPMEVTYPER(reg)		MCR15(reg, 7, c11, c0, 0)
#define WCP15_L2VR3F1(reg)		MCR15(reg, 3, c15, c15, 1)

/*
* READ the registers
*/
#define RCP15_SDER(reg)			MRC15(reg, 0, c1, c1, 1)
/*
* Performance Monitor Registers
*/
#define RCP15_PMACTLR(reg)		MRC15(reg, 0, c9, c15, 5)
#define RCP15_PMCCNTCR(reg)		MRC15(reg, 0, c9, c15, 2)
#define RCP15_PMCCNTR(reg)		MRC15(reg, 0, c9, c13, 0)
#define RCP15_PMCCNTSR(reg)		MRC15(reg, 0, c9, c13, 3)
#define RCP15_PMCNTENCLR(reg)		MRC15(reg, 0, c9, c12, 2)
#define RCP15_PMCNTENSET(reg)		MRC15(reg, 0, c9, c12, 1)
#define RCP15_PMCR(reg)			MRC15(reg, 0, c9, c12, 0)
#define RCP15_PMINTENCLR(reg)		MRC15(reg, 0, c9, c14, 2)
#define RCP15_PMINTENSET(reg)		MRC15(reg, 0, c9, c14, 1)
#define RCP15_PMOVSR(reg)		MRC15(reg, 0, c9, c12, 3)
#define RCP15_PMRLDR(reg)		MRC15(reg, 0, c9, c15, 4)
#define RCP15_PMSELR(reg)		MRC15(reg, 0, c9, c12, 5)
#define RCP15_PMSWINC(reg)		MRC15(reg, 0, c9, c12, 4)
#define RCP15_PMUSERENR(reg)		MRC15(reg, 0, c9, c14, 0)
#define RCP15_PMXEVCNTCR(reg)		MRC15(reg, 0, c9, c15, 0)
#define RCP15_PMXEVCNTR(reg)		MRC15(reg, 0, c9, c13, 2)
#define RCP15_PMXEVCNTSR(reg)		MRC15(reg, 0, c9, c15, 1)
#define RCP15_PMXEVTYPER(reg)		MRC15(reg, 0, c9, c13, 1)
#define RCP15_LPM0EVTYPER(reg)		MRC15(reg, 0, c15, c0, 0)
#define RCP15_LPM1EVTYPER(reg)		MRC15(reg, 1, c15, c0, 0)
#define RCP15_LPM2EVTYPER(reg)		MRC15(reg, 2, c15, c0, 0)
#define RCP15_LPM3EVTYPER(reg)		MRC15(reg, 3, c15, c0, 0)
#define RCP15_L2LPMEVTYPER(reg)		MRC15(reg, 3, c15, c2, 0)
#define RCP15_VLPMEVTYPER(reg)		MRC15(reg, 7, c11, c0, 0)
#define RCP15_CONTEXTIDR(reg)		MRC15(reg, 0, c13, c0, 1)
#define RCP15_L2CR0(reg)		MRC15(reg, 3, c15, c0, 1)
#define RCP15_L2VR3F1(reg)		MRC15(reg, 3, c15, c15, 1)

#endif

