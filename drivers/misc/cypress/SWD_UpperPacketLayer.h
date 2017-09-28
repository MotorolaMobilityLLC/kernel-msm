/******************************************************************************
* File Name: SWD_UpperPacketLayer.h
* Version 1.0
*
* Description:
*  This header file contains the constant definitions, function declarations
*  associated with the upper packet layer of the SWD protocol.
*
* Note:
*
*******************************************************************************
* Copyright (2013), Cypress Semiconductor Corporation.
*******************************************************************************
* This software is owned by Cypress Semiconductor Corporation (Cypress) and is
* protected by and subject to worldwide patent protection (United States and
* foreign), United States copyright laws and international treaty provisions.
* Cypress hereby grants to licensee a personal, non-exclusive, non-transferable
* license to copy, use, modify, create derivative works of, and compile the
* Cypress Source Code and derivative works for the sole purpose of creating
* custom software in support of licensee product to be used only in conjunction
* with a Cypress integrated circuit as specified in the applicable agreement.
* Any reproduction, modification, translation, compilation, or representation of
* this software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: CYPRESS MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH
* REGARD TO THIS MATERIAL, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* Cypress reserves the right to make changes without further notice to the
* materials described herein. Cypress does not assume any liability arising out
* of the application or use of any product or circuit described herein. Cypress
* does not authorize its products for use as critical components in life-support
* systems where a malfunction or failure may reasonably be expected to result in
* significant injury to the user. The inclusion of Cypress' product in a life-
* support systems application implies that the manufacturer assumes all risk of
* such use and in doing so indemnifies Cypress against all charges. Use may be
* limited by and subject to the applicable Cypress software license agreement.
******************************************************************************/

#ifndef __SWD_UPPERPACKETLAYER_H
#define __SWD_UPPERPACKETLAYER_H

/******************************************************************************
*   Constant definitions
******************************************************************************/

/* Macro Definitions for Debug and Access Port registers read and write Commands
   used in the functions defined in UpperPacketLayer.c */
#define DPACC_DP_CTRLSTAT_READ	0x8D
#define DPACC_DP_IDCODE_READ	0xA5
#define DPACC_AP_CSW_READ	0x87
#define DPACC_AP_TAR_READ	0xAF
#define DPACC_AP_DRW_READ	0x9F

#define DPACC_DP_ABORT_WRITE         0x81
#define DPACC_DP_CTRLSTAT_WRITE	0xA9
#define DPACC_DP_SELECT_WRITE	0xB1
#define DPACC_AP_CSW_WRITE	0xA3
#define DPACC_AP_TAR_WRITE	0x8B
#define DPACC_AP_DRW_WRITE	0xBB

/******************************************************************************
*   Function Prototypes
******************************************************************************/
void Read_DAP(unsigned char swd_DAP_Register, unsigned long *data_32);
void Write_DAP(unsigned char swd_DAP_Register, unsigned long data_32);
unsigned char Read_IO(unsigned long addr_32, unsigned long *data_32);
unsigned char Write_IO(unsigned long addr_32, unsigned long data_32);

#endif				/* __SWD_UPPERPACKETLAYER_H */

/* [] END OF FILE */
