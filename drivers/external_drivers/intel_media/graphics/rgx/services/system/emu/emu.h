/*************************************************************************/ /*!
@File
@Title          Emulator PCI header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Defines emulator PCI registers
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

/* Interface Control Register (ICR) on base register 0 */
#define EMULATOR_RGX_ICR_PCI_BASENUM					0
#define EMULATOR_RGX_ICR_PCI_OFFSET						(EMULATOR_RGX_ICR_PCI_BASENUM + PCI_BASEREG_OFFSET_DWORDS)
#define EMULATOR_RGX_ICR_PCI_REGION_SIZE				0x10000

#define EMULATOR_RGX_ICR_REG_PSTD_CMD_STATUS			0x4038

#define EMULATOR_RGX_ICR_REG_IRQ_CTRL					0x4048
#define EMULATOR_RGX_ICR_REG_IRQ_CTRL_IRQ_EN			0x1
#define EMULATOR_RGX_ICR_REG_IRQ_CTRL_IRQ_HINLO			0x2

#define EMULATOR_RGX_ICR_REG_CORE_REVISION				0x40F8


/* RGX reg on base register 1 */
#define EMULATOR_RGX_REG_PCI_BASENUM					1
#define EMULATOR_RGX_REG_PCI_OFFSET						(EMULATOR_RGX_REG_PCI_BASENUM + PCI_BASEREG_OFFSET_DWORDS)

#define EMULATOR_RGX_REG_OFFSET							0x0
#define EMULATOR_RGX_REG_SIZE							0x20000

#define EMULATOR_RGX_REG_WRAPPER_OFFSET					0x20000
#define EMULATOR_RGX_REG_WRAPPER_SIZE					0x4000

/* RGX mem (including HP mapping) on base register 2 */
#define EMULATOR_RGX_MEM_PCI_BASENUM					2
#define EMULATOR_RGX_MEM_PCI_OFFSET						(EMULATOR_RGX_MEM_PCI_BASENUM + PCI_BASEREG_OFFSET_DWORDS)

#define EMULATOR_RGX_MEM_REGION_SIZE					0x20000000

#define EMULATOR_LOCALMEM_FOR_RGX_RESERVE_SIZE			(224*1024*1024)
