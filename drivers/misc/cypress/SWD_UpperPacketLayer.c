/******************************************************************************
* File Name: Swd_UpperPacketLayer.c
* Version 1.0
*
* Description:
*  This file provides the source code for function associated with the upper
*  packet layer of the SWD protocol.
*
* Owner:
*	Tushar Rastogi, Application Engineer (tusr@cypress.com)
*
* Related Document:
*	AN84858 - PSoC 4 Programming using an External Microcontroller (HSSP)
*
* Hardware Dependency:
*   PSoC 5LP Development Kit - CY8CKIT-050
*
* Code Tested With:
*	PSoC Creator 2.2
*	ARM GCC 4.4.1
*	CY8CKIT-050
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

/******************************************************************************
*   Header file Inclusion
******************************************************************************/
#include "SWD_PacketLayer.h"
#include "SWD_UpperPacketLayer.h"

/******************************************************************************
*   Function Definitions
******************************************************************************/

/******************************************************************************
* Function Name: Read_DAP
*******************************************************************************
*
* Summary:
*  Reads the Swd_DAP_Register and stores the returned 32 bit value
*
* Parameters:
*  swd_DAP_Register - MACROS for reading Debug Port and Access Port Register
*  data_32          - 32-bit variable containing the returned value
*
* Return:
*  None
*
******************************************************************************/
void Read_DAP(unsigned char swd_DAP_Register, unsigned long *data_32)
{
	swd_PacketHeader = swd_DAP_Register;

	Swd_ReadPacket();
	if (swd_PacketAck != SWD_OK_ACK)
		return;

	*data_32 =
	    (((unsigned long)swd_PacketData[3] << 24) |
	     ((unsigned long)swd_PacketData[2] << 16) | ((unsigned long)
							 swd_PacketData[1] << 8)
	     | ((unsigned long)swd_PacketData[0]));
}

/******************************************************************************
* Function Name: Write_DAP
*******************************************************************************
*
* Summary:
*  Writes a 32-bit value to swd_DAP_Register
*
* Parameters:
*  swd_DAP_Register - MACROS for writing to Debug Port and Access Port Register
*  data_32          - 32-bit variable containing the value to be written
*
* Return:
*  None
*
******************************************************************************/
void Write_DAP(unsigned char swd_DAP_Register, unsigned long data_32)
{
	swd_PacketHeader = swd_DAP_Register;

	swd_PacketData[3] = (unsigned char)(data_32 >> 24);
	swd_PacketData[2] = (unsigned char)(data_32 >> 16);
	swd_PacketData[1] = (unsigned char)(data_32 >> 8);
	swd_PacketData[0] = (unsigned char)(data_32);

	Swd_WritePacket();
}

/******************************************************************************
* Function Name: Read_IO
*******************************************************************************
*
* Summary:
*  Reads 32-bit date from specified address of CPU address space.
*  Returns OK ACK if all SWD transactions succeeds (ACKed).
*
* Parameters:
*  addr_32 - Address from which the data has to be read from CPU address space
*  data_32 - 32-bit variable containing the read value
*
* Return:
*  Swd_PacketAck - Acknowledge packet for the last SWD transaction
*
*******************************************************************************/
unsigned char Read_IO(unsigned long addr_32, unsigned long *data_32)
{
	Write_DAP(DPACC_AP_TAR_WRITE, addr_32);
	if (swd_PacketAck != SWD_OK_ACK)
		return swd_PacketAck;

	Read_DAP(DPACC_AP_DRW_READ, data_32);
	if (swd_PacketAck != SWD_OK_ACK)
		return swd_PacketAck;

	Read_DAP(DPACC_AP_DRW_READ, data_32);
	if (swd_PacketAck != SWD_OK_ACK)
		return swd_PacketAck;

	return swd_PacketAck;
}

/******************************************************************************
* Function Name: Write_IO
*******************************************************************************
*
* Summary:
*  Writes 32-bit data into specified address of CPU address space.
*  Returns “true” if all SWD transactions succeeded (ACKed).
*
* Parameters:
*  addr_32 - Address at which the data has to be written in CPU address space
*  data_32 - 32-bit variable containing the value to be written
*
* Return:
*  swd_PacketAck - Acknowledge packet for the last SWD transaction
*
******************************************************************************/
unsigned char Write_IO(unsigned long addr_32, unsigned long data_32)
{
	Write_DAP(DPACC_AP_TAR_WRITE, addr_32);
	if (swd_PacketAck != SWD_OK_ACK)
		return swd_PacketAck;

	Write_DAP(DPACC_AP_DRW_WRITE, data_32);
	if (swd_PacketAck != SWD_OK_ACK)
		return swd_PacketAck;

	return swd_PacketAck;
}

/* [] END OF FILE */
