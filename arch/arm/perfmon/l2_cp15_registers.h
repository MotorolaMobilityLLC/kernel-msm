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

#ifndef __l2_cp15_registers__
#define __l2_cp15_registers__

#include "mcrmrc.h"

#define WCP15_SDER(reg)			MCR15(reg, 0, c1, c1, 1)
/*
* Performance Monitor Registers
*/
#define WCP15_L2MPCR(reg)		MCR15(reg, 3, c15, c0, 4)
#define WCP15_L2PMCCNTCR(reg)		MCR15(reg, 3, c15, c4, 4)
#define WCP15_L2PMCCNTR(reg)		MCR15(reg, 3, c15, c4, 5)
#define WCP15_L2PMCCNTSR(reg)		MCR15(reg, 3, c15, c4, 6)
#define WCP15_L2PMCNTENCLR(reg)		MCR15(reg, 3, c15, c4, 2)
#define WCP15_L2PMCNTENSET(reg)		MCR15(reg, 3, c15, c4, 3)
#define WCP15_L2PMCR(reg)		MCR15(reg, 3, c15, c4, 0)
#define WCP15_L2PMINTENCLR(reg)		MCR15(reg, 3, c15, c5, 0)
#define WCP15_L2PMINTENSET(reg)		MCR15(reg, 3, c15, c5, 1)
#define WCP15_L2PMOVSR(reg)		MCR15(reg, 3, c15, c4, 1)
#define WCP15_L2PMRLDR(reg)		MCR15(reg, 3, c15, c4, 7)
#define WCP15_L2PMSELR(reg)		MCR15(reg, 3, c15, c6, 0)
#define WCP15_L2PMXEVCNTCR(reg)		MCR15(reg, 3, c15, c6, 4)
#define WCP15_L2PMXEVCNTR(reg)		MCR15(reg, 3, c15, c6, 5)
#define WCP15_L2PMXEVCNTSR(reg)		MCR15(reg, 3, c15, c6, 6)
#define WCP15_L2PMXEVTYPER(reg)		MCR15(reg, 3, c15, c6, 7)
#define WCP15_L2PMXEVFILTER(reg)	MCR15(reg, 3, c15, c6, 3)
#define WCP15_L2PMEVTYPER0(reg)		MCR15(reg, 3, c15, c7, 0)
#define WCP15_L2PMEVTYPER1(reg)		MCR15(reg, 3, c15, c7, 1)
#define WCP15_L2PMEVTYPER2(reg)		MCR15(reg, 3, c15, c7, 2)
#define WCP15_L2PMEVTYPER3(reg)		MCR15(reg, 3, c15, c7, 3)
#define WCP15_L2PMEVTYPER4(reg)		MCR15(reg, 3, c15, c7, 4)
#define WCP15_L2VR3F1(reg)		MCR15(reg, 3, c15, c15, 1)

/*
* READ the registers
*/
#define RCP15_SDER(reg)			MRC15(reg, 0, c1, c1, 1)
/*
* Performance Monitor Registers
*/
#define RCP15_L2MPCR(reg)		MRC15(reg, 3, c15, c0, 4)
#define RCP15_L2PMCCNTCR(reg)		MRC15(reg, 3, c15, c4, 4)
#define RCP15_L2PMCCNTR(reg)		MRC15(reg, 3, c15, c4, 5)
#define RCP15_L2PMCCNTSR(reg)		MRC15(reg, 3, c15, c4, 6)
#define RCP15_L2PMCNTENCLR(reg)		MRC15(reg, 3, c15, c4, 2)
#define RCP15_L2PMCNTENSET(reg)		MRC15(reg, 3, c15, c4, 3)
#define RCP15_L2PMCR(reg)		MRC15(reg, 3, c15, c4, 0)
#define RCP15_L2PMINTENCLR(reg)		MRC15(reg, 3, c15, c5, 0)
#define RCP15_L2PMINTENSET(reg)		MRC15(reg, 3, c15, c5, 1)
#define RCP15_L2PMOVSR(reg)		MRC15(reg, 3, c15, c4, 1)
#define RCP15_L2PMRLDR(reg)		MRC15(reg, 3, c15, c4, 7)
#define RCP15_L2PMSELR(reg)		MRC15(reg, 3, c15, c6, 0)
#define RCP15_L2PMXEVCNTCR(reg)		MRC15(reg, 3, c15, c6, 4)
#define RCP15_L2PMXEVCNTR(reg)		MRC15(reg, 3, c15, c6, 5)
#define RCP15_L2PMXEVCNTSR(reg)		MRC15(reg, 3, c15, c6, 6)
#define RCP15_L2PMXEVTYPER(reg)		MRC15(reg, 3, c15, c6, 7)
#define RCP15_L2PMXEVFILTER(reg)	MRC15(reg, 3, c15, c6, 3)
#define RCP15_L2PMEVTYPER0(reg)		MRC15(reg, 3, c15, c7, 0)
#define RCP15_L2PMEVTYPER1(reg)		MRC15(reg, 3, c15, c7, 1)
#define RCP15_L2PMEVTYPER2(reg)		MRC15(reg, 3, c15, c7, 2)
#define RCP15_L2PMEVTYPER3(reg)		MRC15(reg, 3, c15, c7, 3)
#define RCP15_L2PMEVTYPER4(reg)		MRC15(reg, 3, c15, c7, 4)
#define RCP15_L2VR3F1(reg)		MRC15(reg, 3, c15, c15, 1)

#endif

