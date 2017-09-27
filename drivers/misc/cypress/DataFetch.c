/*****************************************************************************
* File Name: DataFetch.c
* Version 1.0
*
* Description:
*  This source file contains the functions to fetch data from the Hex image and
*  load in to the arrays used for Programming.
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
* Note:
*  The definitions of the functions used in this file will change based on the
*  interface used to get Hex file data.
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
#include "cycapsense_hssp.h"
#include "DataFetch.h"

/******************************************************************************
*   Global Constant declaration
******************************************************************************/

/******************************************************************************
*   Function Definitions
******************************************************************************/

/***************************** USER ATTENTION REQUIRED ************************
**************** DEPENDS ON INTERFACE USED TO GET HEX FILE DATA ***************
All the function definitions below should be modified based on the interface
used to get Hex file data to be programmed. If a protocol like I2C, UART is
used to get the hex file data, change the function definitions accordingly */

/******************************************************************************
* Function Name: HEX_ReadSiliconId
*******************************************************************************
*
* Summary:
*  Loads the Device Silicon ID data in to an array
*
* Parameters:
*  unsigned long *hexSiliconId - Address of variable which will store the
*						Silicon ID fetched
*
* Return:
*  None.
*
* Note:
*  Modify definition based on method of getting programming data
*
******************************************************************************/
void HEX_ReadSiliconId(struct hex_info *inf, unsigned long *hexSiliconId)
{
	unsigned char i;

	if (!inf || !inf->m_data)
		return;

	for (i = 0; i < SILICON_ID_BYTE_LENGTH; i++) {
		*hexSiliconId = *hexSiliconId |
		    ((unsigned long)(inf->m_data[2 + i]) << (8 * i));
	}
}

/******************************************************************************
* Function Name: HEX_ReadRowData
*******************************************************************************
*
* Summary:
*  Loads the row data from hex file in to an array
*
* Parameters:
*  unsigned short rowCount      - Flash row count for which the data has to be
*								  fetched
*  unsigned char * rowData      - Base address of array which will store the
*						 Flash row data fetched
*
* Return:
*  None.
*
* Note:
*  Modify definition based on the method of getting programming data
*
*  REMOVE THIS LOGIC WHILE PORTING THE HSSP CODE TO OTHER HOST PROGRAMMER
*
******************************************************************************/
void HEX_ReadRowData(struct hex_info *inf, unsigned short rowCount,
		     unsigned char *rowData)
{
	unsigned short i;	/* Maximum value of 'i' can be 256 */

	if (!inf || !inf->data)
		return;

	for (i = 0; i < FLASH_ROW_BYTE_SIZE_HEX_FILE; i++) {
		rowData[i] =
		    inf->data[i + rowCount * FLASH_ROW_BYTE_SIZE_HEX_FILE];
	}
}

/******************************************************************************
* Function Name: HEX_ReadChipProtectionData
*******************************************************************************
*
* Summary:
*  Loads the Chip Protection data in to an array
*
* Parameters:
*
*  unsigned char * chipProtectionData - Address of variable which will store
*					the Chip Protection data fetched
*
* Return:
*  None.
*
* Note:
*  Modify definition based on the method of getting programming data
*
******************************************************************************/
void HEX_ReadChipProtectionData(struct hex_info *inf,
				unsigned char *chipProtectionData)
{
	if (inf)
		*chipProtectionData = inf->cl_protection;
	else
		*chipProtectionData = 1;
}

/******************************************************************************
* Function Name: HEX_ReadRowProtectionData
*******************************************************************************
*
* Summary:
*  Loads the Flash Row Protection data in to an array
*
* Parameters:
*
*  unsigned char * rowProtectionData  - Base address of array which will store
*					 the Flash Row Protection data fetched
*  unsigned char rowProtectionByteSize - Size of flash row protection settings
*					in bytes
*
* Return:
*  None.
*
* Note:
*  Modify definition based on the method of getting programming data
*
******************************************************************************/
void HEX_ReadRowProtectionData(struct hex_info *inf,
			       unsigned char rowProtectionByteSize,
			       unsigned char *rowProtectionData)
{
	if (!inf || !inf->s_data)
		return;

	memcpy(rowProtectionData, inf->s_data, rowProtectionByteSize);
}

/******************************************************************************
* Function Name: HEX_ReadChecksumData
*******************************************************************************
*
* Summary:
*  Loads the Checksum data in to an array
*
* Parameters:
*  unsigned short * checksumData - Base address of the array which will store
*					   the Checksum data fetched
*
* Return:
*  None.
*
* Note:
*  Modify definition based on the method of getting programming data
*
******************************************************************************/
void HEX_ReadChecksumData(struct hex_info *inf, unsigned short *checksumData)
{
	if (!inf)
		return;
	*checksumData = inf->cs;
}

/******************************************************************************
* Function Name: GetFlashRowCount
*******************************************************************************
*
* Summary:
*  Returns the total number of Flash rows in the target PSoC 4 device
*
* Parameters:
*  None
*
* Return:
*  unsigned short
*   Total number of Flash rows in target PSoC 4
*
* Note:
*  The Flash row count is from the HexImage.h file. Modify the definition based
*  on the method of getting programming data
*
******************************************************************************/
unsigned short GetFlashRowCount(void)
{
	return NUMBER_OF_FLASH_ROWS_HEX_FILE;
}

/* [] END OF FILE */
