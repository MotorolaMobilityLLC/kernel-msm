/******************************************************************************
* File Name: ProgrammingSteps.h
* Version 1.0
*
* Description:
*  This header file contains high level definitions and function declarations
*  associated with programming of the target PSoC 4 device
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

#ifndef __PROGRAMMINGSTEPS_H
#define __PROGRAMMINGSTEPS_H

#include "cycapsense_hssp.h"
/******************************************************************************
*   Macro definitions
******************************************************************************/

/* Return value definitions for high level Programming functions */
#ifndef FAILURE
#define FAILURE 0
#endif

#ifndef SUCCESS
#define SUCCESS (!FAILURE)
#endif

/* Return value of PollSromStatus function */
#define SROM_SUCCESS 0x01

/* Error codes returned by Swd_PacketAck whenever any top-level step returns a
   failure status*/

/* This bit field is set if programmer fails to acquire the device in 1.5 ms */
#define PORT_ACQUIRE_TIMEOUT_ERROR	0x10

/* This bit field is set if the SROM does not return the success status code
   within the SROM Polling timeout duration*/
#define SROM_TIMEOUT_ERROR		0x20

/* This bit field is set in case of JTAG ID mismatch or Flash data verification
   mismatch or Checksum data mismatch */
#define VERIFICATION_ERROR		0x40

/* This bit field is set if wrong transition of chip protection settings is
   detected */
#define TRANSITION_ERROR		0x80

/* Constants for Address Space of CPU */
#if defined CY8C40xx_FAMILY
#define CPUSS_SYSREQ			0x40100004
#define CPUSS_SYSARG			0x40100008
#else
#define CPUSS_SYSREQ			0x40000004
#define CPUSS_SYSARG			0x40000008
#endif
#define TEST_MODE			0x40030014
#define SRAM_PARAMS_BASE		0x20000100
#define SFLASH_MACRO			0x0FFFF000
#define SFLASH_CPUSS_PROTECTION		0x0FFFF07C

/* SROM Constants */
#define SROM_KEY1			0xB6
#define SROM_KEY2			0xD3
#define SROM_SYSREQ_BIT			0x80000000
#define SROM_PRIVILEGED_BIT		0x10000000
#define SROM_STATUS_SUCCEEDED		0xA0000000
#define SROM_STATUS_FAILED		0xF0000000

/* SROM Commands */
#define SROM_CMD_GET_SILICON_ID		0x00
#define SROM_CMD_LOAD_LATCH		0x04
#define SROM_CMD_PROGRAM_ROW		0x06
#define SROM_CMD_ERASE_ALL		0x0A
#define SROM_CMD_CHECKSUM		0x0B
#define SROM_CMD_WRITE_PROTECTION	0x0D
#if defined CY8C40xx_FAMILY
#define SROM_CMD_SET_IMO_48MHZ		0x15
#endif

/* Chip Level Protection */
#define CHIP_PROT_VIRGIN		0x00
#define CHIP_PROT_OPEN			0x01
#define CHIP_PROT_PROTECTED		0x02
#define CHIP_PROT_KILL			0x04

/* Mask for status code returned by polling SROM */
#define SROM_STATUS_SUCCESS_MASK	0xF0000000

/* Constant for computing checksum of entire flash */
#define CHECKSUM_ENTIRE_FLASH		0x8000

/* Constant DAP ID of ARM Cortex M0 CPU */
#define CM0_DAP_ID			0x0BC11477
#define CM0PLUS_DAP_ID			0x0BC11477

/* Constant maximum number of rows in PSoC 4 */
#define ROWS_PER_ARRAY			256

/******************************************************************************
*   Function Prototypes
******************************************************************************/

unsigned char DeviceAcquire(void);
unsigned char VerifySiliconId(struct hssp_data *d);
unsigned char EraseAllFlash(struct hssp_data *d);
unsigned char ChecksumPrivileged(struct hssp_data *d);
unsigned char ProgramFlash(struct hssp_data *d);
unsigned char VerifyFlash(struct hssp_data *d);
unsigned char ProgramProtectionSettings(struct hssp_data *d);
unsigned char VerifyProtectionSettings(struct hssp_data *d);
unsigned char VerifyChecksum(struct hssp_data *d);
unsigned char VerifySwRevision(struct hssp_data *d);
void ExitProgrammingMode(void);
unsigned char ReadHsspErrorStatus(void);
unsigned char ReadSromStatus(void);

#endif				/* __PROGRAMMINGSTEPS_H */

/* [] END OF FILE */
