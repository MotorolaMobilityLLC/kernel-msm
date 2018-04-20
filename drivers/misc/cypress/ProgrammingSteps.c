/******************************************************************************
 * File Name: ProgrammingSteps.c
 * Version 1.0
 *
 * Description:
 *  This file provides the source code for the high level Programming functions
 *  used by the main code to program target PSoC 4
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
 * Any reproduction, modification, translation, compilation, or representation
 * of this software except as specified above is prohibited without the express
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
#include "ProgrammingSteps.h"
#include "SWD_PhysicalLayer.h"
#include "SWD_UpperPacketLayer.h"
#include "SWD_PacketLayer.h"
#include "DataFetch.h"
#include "Timeout.h"

/******************************************************************************
 *   Global Variable definitions
 ******************************************************************************/
unsigned long checksum_Privileged;
unsigned long statusCode;

unsigned char result;
unsigned char chipProtectionData_Chip;

enum Transition_mode { OPEN_XXX, VIRGIN_OPEN, PROT_XXX, WRONG_TRANSITION } flow;

/******************************************************************************
 *   Function Definitions
 ******************************************************************************/

/******************************************************************************
 * Function Name: PollSromStatus
 *******************************************************************************
 * Summary:
 *  Polls the SROM_SYSREQ_BIT and SROM_PRIVILEGED_BIT in the CPUSS_SYSREQ
 *  register till it is reset or a timeout condition occurred, whichever is
 *  earlier. For a SROM polling timeout error, the timeout error status bit is
 *  set in swd_PacketAck variable and CPUSS_SYSARG register is read to get the
 *  error status code. If timeout does not happen, the CPUSS_SYSARG register is
 *  read to determine if the task executed successfully.
 *
 * Parameters:
 *  None.
 *
 * Return:
 *  SUCCESS - SROM executed the task successfully
 *  FAILURE - SROM task is not executed successfully and a timeout error occured.
 *            The failure code is stored in the statusCode global variable.
 *
 * Note:
 *  This function is called after non volatile memory operations like Read,
 *  Write of Flash, to check if SROM task has been executed which is indicated
 *  by SUCCESS. The status is read from the CPUSS_SYSARG register.
 *
 ******************************************************************************/
unsigned char PollSromStatus(void)
{
	unsigned long time_elapsed = 0;

	do {
		/* Read CPUSS_SYSREQ register and check if SROM_SYSREQ_BIT and
		   SROM_PRIVILEGED_BIT are reset to 0 */
		Read_IO(CPUSS_SYSREQ, &statusCode);
		statusCode &= (SROM_SYSREQ_BIT | SROM_PRIVILEGED_BIT);
		time_elapsed++;
	} while ((statusCode != 0) && (time_elapsed <= SROM_POLLING_TIMEOUT));

	/* If time exceeds the timeout value, set the SROM_TIMEOUT_ERROR bit in
	   swd_PacketAck */
	if (time_elapsed > SROM_POLLING_TIMEOUT) {
		swd_PacketAck = swd_PacketAck | SROM_TIMEOUT_ERROR;
		Read_IO(CPUSS_SYSARG, &statusCode);
		return FAILURE;
	}

	/* Read CPUSS_SYSARG register to check if the SROM command executed
	   successfully else set SROM_TIMEOUT_ERROR in swd_PacketAck */
	Read_IO(CPUSS_SYSARG, &statusCode);

	if ((statusCode & SROM_STATUS_SUCCESS_MASK) != SROM_STATUS_SUCCEEDED) {
		swd_PacketAck = swd_PacketAck | SROM_TIMEOUT_ERROR;
		return FAILURE;
	}

	return SUCCESS;
}

#if defined CY8C40xx_FAMILY
/******************************************************************************
 * Function Name: SetIMO48MHz
 *******************************************************************************
 * Summary:
 * Set IMO to 48 MHz
 *
 * Parameters:
 *  None.
 *
 * Return:
 *  None
 *
 * Note:
 *  This function is required to be called before any flash operation.
 *  This function sets the IMO to 48 MHz before flash write/erase operations
 *  and is part of the device acquire routine.
 *
 ******************************************************************************/
void SetIMO48MHz(void)
{
	unsigned long parameter1 = 0;

	/* Load the Parameter1 with the SROM command to read silicon ID */
	parameter1 = (unsigned long)(((unsigned long)SROM_KEY1 << 0) +
			(((unsigned long)SROM_KEY2 +
			  (unsigned long)SROM_CMD_SET_IMO_48MHZ) <<
			 8));

	/* Write the command to CPUSS_SYSARG register */
	Write_IO(CPUSS_SYSARG, parameter1);
	Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_SET_IMO_48MHZ);
}
#endif
/******************************************************************************
 * Function Name: ReadSromStatus
 *******************************************************************************
 *
 * Summary:
 *  It reads the StatusCode global variable and returns LSB of this long variable
 *  to main.c.
 *
 * Parameters:
 *  None.
 *
 * Return:
 * LSB of statusCode - LSB of statusCode global variable contains the error code
 *
 * Note:
 * This function is called from main.c when SROM_TIMEOUT_ERROR bit is set in the
 * swd_PacketAck.
 *
 ******************************************************************************/

unsigned char ReadSromStatus(void)
{
	return (unsigned char)statusCode;
}

/******************************************************************************
 * Function Name: GetChipProtectionVal
 *******************************************************************************
 * Summary:
 *  This sub-routine is used to read the Chip Protection Setting by using SROM
 *  System Calls. System call to read Silicon Id returns Chip protection settings
 *  in the CPUSS_SYSREQ register. The location of the data is bit [15:12] in the
 *  32-bit register.
 *
 * Parameters:
 *  None.
 *
 * Return:
 *  chipProtectionData_Chip - 1 byte chip protection setting read from the chip
 *
 * Note:
 * This function is called in the "Step 3. Erase All Flash" to read the chip
 * protection settings to take decision whether to move the protection state
 *  to open and then erase the flash or directly erase the flash.
 *
 ******************************************************************************/
unsigned char GetChipProtectionVal(struct hssp_data *d)
{
	unsigned long parameter1 = 0;
	unsigned long chipProtData = 0;

	/* Load the Parameter1 with the SROM command to read silicon ID */
	parameter1 = (unsigned long)(((unsigned long)SROM_KEY1 << 0) +
			(((unsigned long)SROM_KEY2 +
			  (unsigned long)SROM_CMD_GET_SILICON_ID)
			 << 8));

	/* Write the command to CPUSS_SYSARG register */
	Write_IO(CPUSS_SYSARG, parameter1);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* Request system call by writing to CPUSS_SYSREQ register */
	Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_GET_SILICON_ID);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* Read status of the operation */
	result = PollSromStatus();
	if (result != SUCCESS)
		return FAILURE;

	/* Read CPUSS_SYSREQ register to get the current protection setting of
	   the chip */
	Read_IO(CPUSS_SYSREQ, &chipProtData);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	chipProtectionData_Chip = (unsigned char)(chipProtData >> 12);

	return SUCCESS;
}

/******************************************************************************
 * Function Name: GetTransitionMode
 *******************************************************************************
 *
 * Summary:
 *  It reads the chipProtectionData_Chip global variable which contains the Chip
 *  protection setting stored in the Chip and chipProtectionData_Hex from the
 *  hex file which contains the Chip protection setting stored in the HEX file.
 *  The function then validates if the two settings correspond to a valid
 *  transition.
 *
 * Parameters:
 *  None.
 *
 * Return:
 *  SUCCESS - Returns SUCCESS if the transition is valid.
 *  FAILURE - Returns Failure if the transition is invalid.
 *  Stores the transition in the global enum flow.
 *
 * Note:
 * This function is called in "Step 3. Erase All Flash" to take decision on
 * basis of the global enum flow.
 *
 ******************************************************************************/
unsigned char GetTransitionMode(struct hssp_data *d)
{
	unsigned char chipProtectionData_Hex;

	/* Get the chip protection setting in the HEX file */
	HEX_ReadChipProtectionData(&d->inf, &chipProtectionData_Hex);

	/* enum variable flow stores the transition (current protection
	   setting to setting in hex file) of the chip */
	flow = WRONG_TRANSITION;

	switch (chipProtectionData_Chip) {
		/* virgin to open protection setting is the only
		   allowed transition */
	case CHIP_PROT_VIRGIN:
		if (chipProtectionData_Hex == CHIP_PROT_OPEN)
			flow = VIRGIN_OPEN;
		else
			flow = WRONG_TRANSITION;
		break;

	/* All transitions from Open are allowed other than
	   transition to virgin mode */
	case CHIP_PROT_OPEN:
		if (chipProtectionData_Hex == CHIP_PROT_VIRGIN)
			flow = WRONG_TRANSITION;
		else
			flow = OPEN_XXX;
		break;

	/* Protected to Protected and Protected to Open are
	   the allowed transitions */
	case CHIP_PROT_PROTECTED:
		if ((chipProtectionData_Hex == CHIP_PROT_OPEN) ||
				(chipProtectionData_Hex == CHIP_PROT_PROTECTED))
			flow = PROT_XXX;
		else
			flow = WRONG_TRANSITION;
		break;

	default:
		flow = WRONG_TRANSITION;
		break;
	}

	/* Set TRANSITION_ERROR bit high in Swd_PacketAck to show wrong
	   transition error */
	if (flow == WRONG_TRANSITION) {
		swd_PacketAck = swd_PacketAck | TRANSITION_ERROR;
		return FAILURE;
	}

	return SUCCESS;
}

/******************************************************************************
 * Function Name: LoadLatch
 *******************************************************************************
 *
 * Summary:
 *  This function loads the page latch buffer with data to be programmed in to a
 *  row of flash. Data is loaded into the page latch buffer starting at the
 *  location specified by the SRAM_PARAMS_BASE input parameter. Data loaded into
 *  the page latch buffer will remain until a program operation is performed,
 *  which clears the page latch contents.
 *
 * Parameters:
 *  arrayID - Array Number of the flash
 *  rowData - Array containing 128 bytes of programming data
 *
 * Return:
 *  SUCCESS - Returns SUCCESS if Data is successfully latched
 *  FAILURE - Returns Failure if Data is not latched successfully
 *
 * Note:
 * This function is called in "Step 5. Program Flash" and
 * "Step 7. Program Protection Settings" to latch the programming data in SRAM.
 *
 ******************************************************************************/
unsigned char LoadLatch(struct hssp_data *d, unsigned char arrayID,
		unsigned char *rowData)
{
	unsigned long parameter1 = 0;
	unsigned long parameter2 = 0;
	unsigned char i = 0;

	/* Load parameter1 with the SROM command to load the page latch buffer
	   with programming data */
	parameter1 = ((unsigned long)SROM_KEY1 << 0) +
		(((unsigned long)SROM_KEY2 +
		  (unsigned long)SROM_CMD_LOAD_LATCH) << 8) +
		(0x00 << 16) + ((unsigned long)arrayID << 24);

	/* Number of Bytes to load minus 1 */
	parameter2 = (FLASH_ROW_BYTE_SIZE_HEX_FILE - 1);

	/* Write parameter1 in SRAM */
	Write_IO(SRAM_PARAMS_BASE + 0x00, parameter1);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* Write parameter2 in SRAM */
	Write_IO(SRAM_PARAMS_BASE + 0x04, parameter2);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* Put row data into SRAM buffer */
	for (i = 0; i < FLASH_ROW_BYTE_SIZE_HEX_FILE; i += 4) {
		parameter1 = (rowData[i] << 0) + (rowData[i + 1] << 8) +
			(rowData[i + 2] << 16) + (rowData[i + 3] << 24);
		/* Write parameter1 in SRAM */
		Write_IO(SRAM_PARAMS_BASE + 0x08 + i, parameter1);
		if (swd_PacketAck != SWD_OK_ACK)
			return FAILURE;
	}

	/*  Call "Load Latch" SROM API */

	/* Set location of parameters */
	Write_IO(CPUSS_SYSARG, SRAM_PARAMS_BASE);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* Request SROM operation */
	Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_LOAD_LATCH);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* Read status of the operation */
	result = PollSromStatus();
	if (result != SUCCESS)
		return FAILURE;

	return SUCCESS;
}

/******************************************************************************
 * Function Name: ChecksumAPI
 *******************************************************************************
 *
 * Summary:
 *  This function reads either the whole flash memory or a row of flash. When
 *  performing a checksum on the whole flash, the user code and the supervisory
 *  flash regions are included. When performing a checksum only on one row of
 *  flash, the flash row number is passed as a parameter. For computing Checksum
 *  of entire flash, ChecksumRow input parameter is loaded as macro
 *  CHECKSUM_ENTIRE_FLASH (0x8000).
 *
 * Parameters:
 *  checksumRow: Row number of flash for which checksum has to be calculated.
 *				To compute checksum of entire flash,
 *				this variable is set to
 *				CHECKSUM_ENTIRE_FLASH (0x8000).
 *
 *  checksum: This variable is loaded with the checksum after the execution of
 *			 the function.
 *
 * Return:
 *  SUCCESS - Returns SUCCESS if Checksum System call is successfully executed.
 *  FAILURE - Returns Failure if Checksum system call fails to execute.
 *
 * Note:
 * This function is called in "Step 4. Checksum Privileged Calculation" and
 * "Step 9. Verify Checksum" to calculate the checksum of flash privileged rows
 * and entire flash, respectively.
 *
 ******************************************************************************/
unsigned char ChecksumAPI(struct hssp_data *d, unsigned short checksumRow,
		unsigned long *checksum)
{
	unsigned long parameter1 = 0;
	unsigned long checksum_chip = 0;

	/* Load parameter1 with the SROM command to compute checksum of whole
	   flash */
	parameter1 = ((unsigned long)SROM_KEY1 << 00) +
		(((unsigned long)SROM_KEY2 +
		  (unsigned long)SROM_CMD_CHECKSUM) << 8) +
		(((unsigned long)checksumRow & 0x000000FF) << 16) +
		(((unsigned long)checksumRow & 0x0000FF00) << 16);

	/* Load CPUSS_SYSARG register with parameter1 command */
	Write_IO(CPUSS_SYSARG, parameter1);

	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* Request SROM operation */
	Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_CHECKSUM);

	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* Read status of the operation */
	result = PollSromStatus();

	if (result != SUCCESS)
		return FAILURE;

	/* Read CPUSS_SYSARG register to get the checksum value */
	Read_IO(CPUSS_SYSARG, &checksum_chip);

	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* 28-bit checksum */
	*checksum = (checksum_chip & 0x0FFFFFFF);

	return SUCCESS;
}

/******************************************************************************
 *Function Name: DeviceAcquire
 *******************************************************************************
 *
 * Summary:
 *  Acquire device with a alternate method which don't require >1.5Mhz SWD clock rate
 *
 *
 * Parameters:
 *  None
 *
 * Return:
 *  SUCCESS - Returns SUCCESS if the device is successfully acquired.
 *  FAILURE - Returns Failure if the function fails in any of the intermediate
 *			 step.
 *
 * Note:
 * This function has very strict timing requirements. The device must be
 * acquired as per the timing requirements given in PSoC 4 Device Programming
 * Specification Document.
 *
 ******************************************************************************/

#define DEVICE_ACQUIRE_TIMEOUT_ALTERNATE_METHOD  40

unsigned char DeviceAcquire(void)
{
	unsigned long chip_DAP_Id = 0;
	unsigned short total_packet_count = 0;
	unsigned long status = 0;
	unsigned long reset_address = 0;
	unsigned long psr_reg_val = 0;
	unsigned long params = 0;
	int i = 1;

	SetXresCmosOutput();
	SetXresHigh();

	SetSwdckCmosOutput();
	SetSwdckLow();

	SetSwdioCmosOutput();
	SetSwdioLow();

	/* Execute SWD connect sequence.
		100 ms time out below is worst case time out value */
	do {
		Swd_LineReset();
		Read_DAP(DPACC_DP_IDCODE_READ, &chip_DAP_Id);
		total_packet_count++;
	} while ((swd_PacketAck != SWD_OK_ACK) &&
			(total_packet_count < DEVICE_ACQUIRE_TIMEOUT_ALTERNATE_METHOD));


	/* Set PORT_ACQUIRE_TIMEOUT_ERROR bit in swd_PacketAck if time
	   exceeds 100 ms */
	if (total_packet_count == DEVICE_ACQUIRE_TIMEOUT_ALTERNATE_METHOD) {
		swd_PacketAck = swd_PacketAck | PORT_ACQUIRE_TIMEOUT_ERROR;
		return FAILURE;
	}

	/* Set VERIFICATION_ERROR bit in swd_PacketAck if the DAP_ID read
	   from chip does not match with the ARM CM0_DAP_ID (MACRO defined in
	   ProgrammingSteps.h file - 0x0BB11477) */
	if ((chip_DAP_Id != CM0_DAP_ID)  && (chip_DAP_Id != CM0PLUS_DAP_ID)) {
		swd_PacketAck = swd_PacketAck | VERIFICATION_ERROR;
		return FAILURE;
	}

	/* Clear WDATAERR if any previous firmware upgrade operation was aborted in the mddle
	and WDATAERR bit is set. This write is to the AP_ABORT register in Debug Port
	(APnDP bit - 0, Address is 2'b00, Access -W for the AP ABORT register */
	Write_DAP(DPACC_DP_ABORT_WRITE, 0x00000008);

	/* Set the CSYSPWRUPEQ(System power up request), CDBGPRWUPREQ(debug power up request),
	CDBGRSTREQ(Debug reset request) bits in the DP_CTRLSTAT regsiter) */
	Write_DAP(DPACC_DP_CTRLSTAT_WRITE, 0x54000000);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* Set the AP register bank selection */
	Write_DAP(DPACC_DP_SELECT_WRITE, 0x00000000);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* set the register access as word 4-byte access
	For device with CM0+ CPU, the HPROT[1] bit also needs to be set along with few other bits field */
	if (chip_DAP_Id == CM0PLUS_DAP_ID) {
		Write_DAP(DPACC_AP_CSW_WRITE, 0x03000042);
		if (swd_PacketAck != SWD_OK_ACK)
			return FAILURE;
	}

	/* Enable debug, and halt the CPU  */
	Write_IO(0xE000EDF0, 0xA05F0003);

	/* Verify the debug enable, cpu halt bits are set */
	Read_IO(0xE000EDF0, &status);
	if ((status & 0x00000003) != 0x00000003)
		return FAILURE;

	/* Enable Breakpoint unit */
	Write_IO(0xE0002000, 0x00000003);

	/* Get address at reset vector */
	Read_IO(0x00000004, &reset_address);

	/* Map the address bits to the breakpoint compare register bit map,
	Set the enable breakpoint bit, and the match bits */
	reset_address = (reset_address & 0x1FFFFFFC) | 0xC0000001;

	/* Update the breakpoint compare register */
	Write_IO(0xE0002008, reset_address);

	/* Issue software reset */
	Write_IO(0xE000ED0C, 0x05FA0004);

	/* Sufficient delay after reset for boot process, 5ms */
	while (i <= 50) {
		DelayHundredUs();
		i++;
	}

	/* Repeat a portion of the acquire sequence again */
	do {
		Swd_LineReset();
		Read_DAP(DPACC_DP_IDCODE_READ, &chip_DAP_Id);
		total_packet_count++;
	} while ((swd_PacketAck != SWD_OK_ACK) &&
			(total_packet_count < DEVICE_ACQUIRE_TIMEOUT_ALTERNATE_METHOD));


	/* Set PORT_ACQUIRE_TIMEOUT_ERROR bit in swd_PacketAck if time
	   exceeds 100 ms */
	if (total_packet_count == DEVICE_ACQUIRE_TIMEOUT_ALTERNATE_METHOD) {
		swd_PacketAck = swd_PacketAck | PORT_ACQUIRE_TIMEOUT_ERROR;
		return FAILURE;
	}

	/* Set VERIFICATION_ERROR bit in swd_PacketAck if the DAP_ID read
	   from chip does not match with the ARM CM0_DAP_ID (MACRO defined in
	   ProgrammingSteps.h file - 0x0BB11477) */
	if ((chip_DAP_Id != CM0_DAP_ID)  && (chip_DAP_Id != CM0PLUS_DAP_ID)) {
		swd_PacketAck = swd_PacketAck | VERIFICATION_ERROR;
		return FAILURE;
	}


	/* Set the CSYSPWRUPEQ(System power up request), CDBGPRWUPREQ(debug power up request),
	CDBGRSTREQ(Debug reset request) bits in the DP_CTRLSTAT regsiter) */
	Write_DAP(DPACC_DP_CTRLSTAT_WRITE, 0x54000000);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* Set the AP register bank selection */
	Write_DAP(DPACC_DP_SELECT_WRITE, 0x00000000);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* set the register access as word 4-byte access
	For device with CM0+ CPU, the HPROT[1] bit also needs to be set along with few other bits field */
	if (chip_DAP_Id == CM0PLUS_DAP_ID) {
		Write_DAP(DPACC_AP_CSW_WRITE, 0x03000042);
		if (swd_PacketAck != SWD_OK_ACK)
			return FAILURE;
	}

	/* Verify the debug enable, cpu halt bits are set */
	Read_IO(0xE000EDF0, &status);
	if ((status & 0x00000003) != 0x00000003)
		return FAILURE;

	/* Load infinite for loop code in SRAM address 0x20000300 */
	Write_IO(0x20000300, 0xE7FEE7FE);

	/* Load PC with address of infinite for loop SRAM address with thumb it(bit 0) set */
	Write_IO(0xE000EDF8, 0x20000301);
	Write_IO(0xE000EDF4, 0x0001000F);

	/* Load SP with top of the SRAM address - set for mimimum SRAM size device(2KB size) */
	Write_IO(0xE000EDF8, 0x20000800);
	Write_IO(0xE000EDF4, 0x00010011);

	/* Read xPSR regsiter, set the thumb bit, and store modified value to xPSR regsiter */
	Write_IO(0xE000EDF4, 0x00000010);
	Read_IO(0xE000EDF8, &psr_reg_val);
	psr_reg_val =  psr_reg_val | 0x01000000;
	Write_IO(0xE000EDF8, psr_reg_val);
	Write_IO(0xE000EDF4, 0x00010010);

	/* Disable breakpoint unit */
	Write_IO(0xE0002000, 0x00000002);

	/* Unhalt CPU */
	Write_IO(0xE000EDF0, 0xA05F0001);

	/* The following SROM call to set IMO to 48MHZ must be skipped for PSOC 4100M, 4200M devices
	Set "IMO=48MHZ" to enable Erase/Program/Write Flash operations. */
	params = ((unsigned long)SROM_KEY1 << 0) + (((unsigned long)SROM_KEY2 + (unsigned long)SROM_CMD_SET_IMO_48MHZ) << 8);

	/* Write Params in CPUSS_SYSARG */
	Write_IO(CPUSS_SYSARG, params);

	/* Request SROM call */
	Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_SET_IMO_48MHZ);

	/* Read status of the operation */
	result = PollSromStatus();
	if (result != SUCCESS)
		return FAILURE;


	return SUCCESS;

}

/******************************************************************************
 * Function Name: VerifySiliconId
 *******************************************************************************
 *
 * Summary:
 *  This is Step 2 of the programming sequence. In this Step, Silicon Id of the
 *  PSoC 4 device is read and matched with the silicon id stored in the Hex File
 *  to verify that the correct device is being programmed.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  SUCCESS - Returns SUCCESS if Silicon Id read from chip matches the Id in the
 *			 HEX File.
 *  FAILURE - Returns Failure if any of the intermediate step returns a fail
 *			 message.
 *
 * Note:
 *
 ******************************************************************************/
unsigned char VerifySiliconId(struct hssp_data *d)
{
	unsigned char i;
	unsigned long deviceSiliconID;
	unsigned long hexSiliconId = 0;
	unsigned long parameter1 = 0;
	unsigned long siliconIdData1 = 0;
	unsigned long siliconIdData2 = 0;

	/* Read and store Silicon ID from HEX file to hexSiliconId array */
	HEX_ReadSiliconId(&d->inf, &hexSiliconId);

	/* Load Parameter1 with the SROM command to read silicon ID from
	   PSoC 4 chip */
	parameter1 = (unsigned long)(((unsigned long)SROM_KEY1 << 0) +
			(((unsigned long)SROM_KEY2 +
			  (unsigned long)SROM_CMD_GET_SILICON_ID)
			 << 8));

	/* Load CPUSS_SYSARG register with parameter1 */
	Write_IO(CPUSS_SYSARG, parameter1);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* Request SROM operation */
	Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_GET_SILICON_ID);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* Read status of the operation */
	result = PollSromStatus();
	if (result != SUCCESS)
		return FAILURE;

	/* Read CPUSS_SYSARG and CPUSS_SYSREQ to read 4 bytes of silicon ID */
	Read_IO(CPUSS_SYSARG, &siliconIdData1);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	Read_IO(CPUSS_SYSREQ, &siliconIdData2);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/*
	   SiliconIdData2 (0th byte) = 4th byte of Device Silicon ID (MSB)
	   SiliconIdData1 (3rd byte) = 3rd byte of Device Silicon ID
	   SiliconIdData1 (1st byte) = 2nd byte of Device Silicon ID
	   SiliconIdData1 (2nd byte) = 1st byte of Device Silicon ID (LSB)
	 */
	deviceSiliconID = (((siliconIdData2 << 24) & 0xFF000000) +
			(siliconIdData1 & 0x00FF0000) +
			((siliconIdData1 << 8) & 0x0000FF00) +
			((siliconIdData1 >> 8) & 0x000000FF));

	/* Match the Silicon ID read from HEX file and PSoC 4 chip */
	for (i = 0; i < SILICON_ID_BYTE_LENGTH; i++) {
		if ((deviceSiliconID & 0xFF00FFFF) !=
				(hexSiliconId & 0xFF00FFFF)) {
			/* Set the VERIFICATION_ERROR bit in swd_PacketAck */
			swd_PacketAck = swd_PacketAck | VERIFICATION_ERROR;
			return FAILURE;
		}
	}
	return SUCCESS;
}

/******************************************************************************
 * Function Name: EraseAllFlash
 *******************************************************************************
 *
 * Summary:
 *  This is Step 3 of the programming sequence. In this Step, the whole user
 *  flash is erased. This function uses GetChipProtectionVal() and
 *  GetTransitionMode() API's to take the decision on the method to follow to
 *  erase the device.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  SUCCESS - Returns SUCCESS if function successfully erases complete user
 *			 flash region.
 *  FAILURE - Returns Failure if any of the intermediate step returns a fail
 *			 message.
 *
 * Note:
 *
 ******************************************************************************/
unsigned char EraseAllFlash(struct hssp_data *d)
{
	unsigned long parameter1 = 0;

	/* Get current chip protection setting */
	GetChipProtectionVal(d);

	/* Check if the Chip protection setting transition is valid */
	result = GetTransitionMode(d);
	if (result != SUCCESS)
		return FAILURE;

	/* If the transition is from open to any protection setting or from
	   virgin to open, call ERASE_ALL SROM command */
	if ((flow == OPEN_XXX) || (flow == VIRGIN_OPEN)) {
		parameter1 = (unsigned long)(((unsigned long)SROM_KEY1 << 0) +
				(((unsigned long)SROM_KEY2 +
				  (unsigned long)
				  SROM_CMD_ERASE_ALL) << 8));

		/* Load ERASE_ALL SROM command in parameter1 to SRAM */
		Write_IO(SRAM_PARAMS_BASE + 0x00, parameter1);
		if (swd_PacketAck != SWD_OK_ACK)
			return FAILURE;

		/* Set location of parameters */
		Write_IO(CPUSS_SYSARG, SRAM_PARAMS_BASE);
		if (swd_PacketAck != SWD_OK_ACK)
			return FAILURE;

		/* Request SROM call */
		Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_ERASE_ALL);
		if (swd_PacketAck != SWD_OK_ACK)
			return FAILURE;

		/* Read status of the operation */
		result = PollSromStatus();
		if (result != SUCCESS)
			return FAILURE;

	} else if (flow == PROT_XXX) {
		/* If the transition is from protected mode to open mode or
		   protected mode to protected mode only,
		   call ERASE_ALL SROM command */

		/* Move chip to open state: 0x01 corresponds to open state,
		   0x00 to macro 1 */
		parameter1 = ((unsigned long)SROM_KEY1 << 0) +
			(((unsigned long)SROM_KEY2 +
			  (unsigned long)SROM_CMD_WRITE_PROTECTION) << 8) +
			(0x01 << 16) + (0x00 << 24);

		/* Load the write protection command to SRAM */
		Write_IO(CPUSS_SYSARG, parameter1);
		if (swd_PacketAck != SWD_OK_ACK)
			return FAILURE;

		/* Request SROM call */
		Write_IO(CPUSS_SYSREQ,
				SROM_SYSREQ_BIT | SROM_CMD_WRITE_PROTECTION);
		if (swd_PacketAck != SWD_OK_ACK)
			return FAILURE;

		/* Read status of the operation */
		result = PollSromStatus();
		if (result != SUCCESS)
			return FAILURE;

		/* Re-acquire chip in OPEN mode */
		result = DeviceAcquire();
		if (result != SUCCESS)
			return FAILURE;
	}

	return SUCCESS;
}

/******************************************************************************
 * Function Name: ChecksumPrivileged
 *******************************************************************************
 *
 * Summary:
 *  This is Step 4 of the programming sequence. In this Step, checksum of the
 *  privileged rows is calulated using system call to determine checksum. This
 *  step uses ChecksumAPI() API to store the checksum of privileged rows in a
 *  Checksum_Privileged global variable. This variable is used in step 9 to
 *  calculate the checksum of user rows.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  SUCCESS - Returns SUCCESS if function successfully calculated the checksum
 *            of privileged rows.
 *  FAILURE - Returns Failure if any of the intermediate step returns a fail
 *			 message.
 *
 * Note:
 *
 ******************************************************************************/
unsigned char ChecksumPrivileged(struct hssp_data *d)
{
	result = ChecksumAPI(d, CHECKSUM_ENTIRE_FLASH, &checksum_Privileged);
	if (result != SUCCESS)
		return FAILURE;
	return SUCCESS;
}

/******************************************************************************
 * Function Name: ProgramFlash
 *******************************************************************************
 *
 * Summary:
 *  This is Step 5 of the programming sequence. In this Step, the whole user
 *  flash is re-programmed with the programming data in the HEX File. This
 *  function uses LoadLatch() API to latch the row data in SRAM page latch buffer
 *  which is then programmed to the specific row using system calls to program
 *  row.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  SUCCESS - Returns SUCCESS if function successfully programs entire flash
 *			 region.
 *  FAILURE - Returns Failure if any of the intermediate step returns a fail
 *			 message.
 *
 * Note:
 *
 ******************************************************************************/
unsigned char ProgramFlash(struct hssp_data *d)
{
	unsigned char arrayID = 0;
	unsigned char rowData[FLASH_ROW_BYTE_SIZE_HEX_FILE];
	unsigned short numOfFlashRows = 0;
	unsigned short rowCount = 0;
	unsigned long parameter1 = 0;

	/* Get the total number of flash rows in the Target PSoC 4 device */
	numOfFlashRows = GetFlashRowCount();

	/* Program all flash rows */
	for (rowCount = 0; rowCount < numOfFlashRows; rowCount++) {

		HEX_ReadRowData(&d->inf, rowCount, &rowData[0]);
		arrayID = rowCount / ROWS_PER_ARRAY;
		result = LoadLatch(d, arrayID, &rowData[0]);
		if (result != SUCCESS)
			return FAILURE;

		/* Load parameter1 with Program Row - SROM command */
		parameter1 = (unsigned long)(((unsigned long)SROM_KEY1 << 0) +
				(((unsigned long)SROM_KEY2 +
				  (unsigned long)
				  SROM_CMD_PROGRAM_ROW) << 8) +
				(((unsigned long)rowCount &
				  0x000000FF) << 16) +
				(((unsigned long)rowCount &
				  0x0000FF00) << 16));

		/* Write parameters in SRAM */
		Write_IO(SRAM_PARAMS_BASE + 0x00, parameter1);
		if (swd_PacketAck != SWD_OK_ACK)
			return FAILURE;

		/* Set location of parameters */
		Write_IO(CPUSS_SYSARG, SRAM_PARAMS_BASE);
		if (swd_PacketAck != SWD_OK_ACK)
			return FAILURE;

		/* Request SROM operation */
		Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_PROGRAM_ROW);
		if (swd_PacketAck != SWD_OK_ACK)
			return FAILURE;

		/* Read status of the operation */
		result = PollSromStatus();
		if (result != SUCCESS)
			return FAILURE;

	}

	return SUCCESS;
}

/******************************************************************************
 * Function Name: VerifySwRevision
 *******************************************************************************
 *
 * Summary:
 *  This is a way to check Sw revision stored in flash at SW_REVISION_OFFSET.
 *  This is a preliminary step to avoid reflashing with the same SW revision.
 *  The checksum verification may not work because the checksun is changed is
 *  a configuration is stored in the flash. In this Step, flash region is
 *  directly read using Read_IO API defined in SWD_UpperPacketLayer.h and
 *  compared with the HEX File.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  SUCCESS - Returns SUCCESS if function successfully verifies the software
 *		     revision with the HEX File.
 *  FAILURE - Returns Failure if any of the intermediate step returns a fail
 *			 message.
 *
 * Note:
 *
 ******************************************************************************/
unsigned char VerifySwRevision(struct hssp_data *d)
{
	unsigned long flashData = 0;
	unsigned short row;
	unsigned short offset;
	unsigned short HexSwRev;
	unsigned short ChipSwRev;
	unsigned char rowData[FLASH_ROW_BYTE_SIZE_HEX_FILE];

	/* Get the Sw rev row in the Target PSoC 4 device */
	row = SW_REVISION_OFFSET / FLASH_ROW_BYTE_SIZE_HEX_FILE;
	offset = SW_REVISION_OFFSET % FLASH_ROW_BYTE_SIZE_HEX_FILE;

	/* Extract row from the hex-file */
	HEX_ReadRowData(&d->inf, row, &rowData[0]);

	HexSwRev = (rowData[offset + 1]<<8) | rowData[offset];

	/* Read flash via AHB-interface */
	Read_IO(SW_REVISION_OFFSET, &flashData);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	ChipSwRev = (flashData >> ((offset % 4)*8)) & 0x00FF;
	ChipSwRev |= ((flashData >> (offset % 4 + 1) * 8) << 8) & 0xFF00;
	d->sw_rev = HexSwRev;

	pr_info("cycapsense_hssp: File SW rev 0x%x, Flash SW rev 0x%x\n",
			HexSwRev, ChipSwRev);

	if (HexSwRev != ChipSwRev) {
		swd_PacketAck = swd_PacketAck|VERIFICATION_ERROR;
		return FAILURE;
	}

	return SUCCESS;
}

/******************************************************************************
 * Function Name: VerifyFlash
 *******************************************************************************
 *
 * Summary:
 *  This is Step 6 of the programming sequence. This is an optional step as we
 *  verify the checksum explicitly. In this Step, flash region is directly read
 *  using Read_IO API defined in SWD_UpperPacketLayer.h and compared with the
 *  HEX File.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  SUCCESS - Returns SUCCESS if function successfully verifies the entire flash
 *		     with the HEX File.
 *  FAILURE - Returns Failure if any of the intermediate step returns a fail
 *			 message.
 *
 * Note:
 *
 ******************************************************************************/
unsigned char VerifyFlash(struct hssp_data *d)
{
	unsigned long flashData = 0;
	unsigned short numOfFlashRows = 0;
	unsigned short rowAddress = 0;
	unsigned short rowCount;
	unsigned char i;
	unsigned char rowData[FLASH_ROW_BYTE_SIZE_HEX_FILE];
	unsigned char chipData[FLASH_ROW_BYTE_SIZE_HEX_FILE];

	/* Get the total number of flash rows in the Target PSoC 4 device */
	numOfFlashRows = GetFlashRowCount();

	/* Read and Verify Flash rows */
	for (rowCount = 0; rowCount < numOfFlashRows; rowCount++) {
		/* Read row from hex file */
		/* linear address of row in flash */
		rowAddress = FLASH_ROW_BYTE_SIZE_HEX_FILE * rowCount;

		/* Extract 128-byte row from the hex-file from address:
		   “rowCount” into buffer - “rowData”. */
		HEX_ReadRowData(&d->inf, rowCount, &rowData[0]);

		/* Read row from chip */
		for (i = 0; i < FLASH_ROW_BYTE_SIZE_HEX_FILE; i += 4) {
			/* Read flash via AHB-interface */
			Read_IO(rowAddress + i, &flashData);
			if (swd_PacketAck != SWD_OK_ACK)
				return FAILURE;

			chipData[i + 0] = (flashData >> 0) & 0xFF;
			chipData[i + 1] = (flashData >> 8) & 0xFF;
			chipData[i + 2] = (flashData >> 16) & 0xFF;
			chipData[i + 3] = (flashData >> 24) & 0xFF;
		}

		/* Compare the row data of HEX file with chip data */
		for (i = 0; i < FLASH_ROW_BYTE_SIZE_HEX_FILE; i++) {
			if (chipData[i] != rowData[i]) {
				swd_PacketAck = swd_PacketAck |
					VERIFICATION_ERROR;
				return FAILURE;
			}
		}
	}
	return SUCCESS;
}

/******************************************************************************
 * Function Name: ProgramProtectionSettings
 *******************************************************************************
 *
 * Summary:
 *  This is Step 7 of the programming sequence. In this step, Chip protection
 *  settings and Row Protection settings are read from the HEX file and
 *  programmed to the specific loctions in the flash.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  SUCCESS - Returns SUCCESS if function successfully writes the protection
 *			 settings.
 *  FAILURE - Returns Failure if any of the intermediate step returns a fail
 *			 message.
 *
 * Note:
 *
 ******************************************************************************/
unsigned char ProgramProtectionSettings(struct hssp_data *d)
{
	unsigned char arrayID = 0;
	unsigned char rowProtectionByteSize = 0;
	unsigned char rowProtectionData[MAXIMUM_ROW_PROTECTION_BYTE_LENGTH];
	unsigned char chipProtectionData_Hex;
	unsigned short numOfFlashRows = 0;
	unsigned long parameter1 = 0;

	/* Get total number of flash rows to determine the size of
	   row protection data and arrayID */
	numOfFlashRows = GetFlashRowCount();

	rowProtectionByteSize = numOfFlashRows / 8;

	arrayID = numOfFlashRows / ROWS_PER_ARRAY;

	HEX_ReadChipProtectionData(&d->inf, &chipProtectionData_Hex);

	HEX_ReadRowProtectionData(&d->inf, rowProtectionByteSize,
			&rowProtectionData[0]);

	/* Load protection setting of current macro into volatile latch using
	   LoadLatch API */
	result = LoadLatch(d, arrayID, &rowProtectionData[0]);
	if (result != SUCCESS)
		return FAILURE;

	/* Program protection setting into supervisory row */
	parameter1 = (unsigned long)(((unsigned long)SROM_KEY1 << 0) +
			(((unsigned long)SROM_KEY2 +
			  (unsigned long)SROM_CMD_WRITE_PROTECTION)
			 << 8) +
			((unsigned long)chipProtectionData_Hex <<
			 16));

	/* Load parameter1 in CPUSS_SYSARG register */
	Write_IO(CPUSS_SYSARG, parameter1);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* Request SROM call */
	Write_IO(CPUSS_SYSREQ, SROM_SYSREQ_BIT | SROM_CMD_WRITE_PROTECTION);

	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	/* Read status of the operation */
	result = PollSromStatus();
	if (result != SUCCESS)
		return FAILURE;

	return SUCCESS;
}

/******************************************************************************
 * Function Name: VerifyProtectionSettings
 *******************************************************************************
 *
 * Summary:
 *  This is Step 8 of the programming sequence. In this step, Chip protection
 *  settings and Row Protection settings are read from the HEX file and verified
 *  with the protection settings programmed in flash.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  SUCCESS - Returns SUCCESS if function successfully verifies the protection
 *			 settings.
 *  FAILURE - Returns Failure if any of the intermediate step returns a fail
 *			 message.
 *
 * Note:
 *
 ******************************************************************************/
unsigned char VerifyProtectionSettings(struct hssp_data *d)
{
	unsigned long protectionData = 0;
	unsigned long flashProtectionAddress = 0;
	unsigned short numOfFlashRows = 0;
	unsigned char chipProtectionData_Hex = 0;
	unsigned char rowProtectionByteSize = 0;
	unsigned char i;
	unsigned char rowProtectionData[MAXIMUM_ROW_PROTECTION_BYTE_LENGTH];
	unsigned char
		rowProtectionFlashData[MAXIMUM_ROW_PROTECTION_BYTE_LENGTH];

	numOfFlashRows = GetFlashRowCount();

	rowProtectionByteSize = numOfFlashRows / 8;

	flashProtectionAddress = SFLASH_MACRO;

	/* Read Protection settings from hex-file */
	HEX_ReadRowProtectionData(&d->inf, rowProtectionByteSize,
			&rowProtectionData[0]);

	/* Read Protection settings from silicon */
	for (i = 0; i < rowProtectionByteSize; i += 4) {

		Read_IO(flashProtectionAddress + i, &protectionData);
		if (swd_PacketAck != SWD_OK_ACK)
			return FAILURE;

		rowProtectionFlashData[i + 0] = (protectionData >> 0) & 0xFF;
		rowProtectionFlashData[i + 1] = (protectionData >> 8) & 0xFF;
		rowProtectionFlashData[i + 2] = (protectionData >> 16) & 0xFF;
		rowProtectionFlashData[i + 3] = (protectionData >> 24) & 0xFF;
	}

	/* Compare hex and silicon’s data */
	for (i = 0; i < rowProtectionByteSize; i++) {

		if (rowProtectionData[i] != rowProtectionFlashData[i]) {
			/* Set the verification error bit for Flash
			   protection data mismatch and return failure */
			swd_PacketAck = swd_PacketAck | VERIFICATION_ERROR;
			return FAILURE;
		}
	}

	/* Read Chip Level Protection from hex-file */
	HEX_ReadChipProtectionData(&d->inf, &chipProtectionData_Hex);

	/* Read Chip Level Protection from the silicon */
	Read_IO(SFLASH_CPUSS_PROTECTION, &protectionData);
	if (swd_PacketAck != SWD_OK_ACK)
		return FAILURE;

	chipProtectionData_Chip = (protectionData >> 24) & 0x0F;
	if (chipProtectionData_Chip == CHIP_PROT_VIRGIN)
		chipProtectionData_Chip = CHIP_PROT_OPEN;
	else if (chipProtectionData_Chip == CHIP_PROT_OPEN)
		chipProtectionData_Chip = CHIP_PROT_VIRGIN;

	/* Compare hex’s and silicon’s chip protection data */
	if (chipProtectionData_Chip != chipProtectionData_Hex) {
		/* Set the verification error bit for Flash protection data
		   mismatch and return failure */
		swd_PacketAck = swd_PacketAck | VERIFICATION_ERROR;
		return FAILURE;
	}

	return SUCCESS;
}

/******************************************************************************
 * Function Name: VerifyChecksum
 *******************************************************************************
 *
 * Summary:
 *  This is Step 9 of the programming sequence. In this step, Checksum of user
 *  data in flash is verified with the Checksum stored in the HEX File. This step
 *  uses the Checksum of privileged rows calculated in Step 4 get the checksum
 *  of user data in flash.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  SUCCESS - Returns SUCCESS if function successfully verifies the checksum.
 *  FAILURE - Returns Failure if any of the intermediate step returns a fail
 *			 message.
 *
 * Note:
 *
 ******************************************************************************/
unsigned char VerifyChecksum(struct hssp_data *d)
{
	unsigned long checksum_All = 0;
	unsigned short chip_Checksum = 0;
	unsigned short checksumData = 0;

	/* Read the checksum of entire flash */
	result = ChecksumAPI(d, CHECKSUM_ENTIRE_FLASH, &checksum_All);
	if (result != SUCCESS)
		return FAILURE;

	/* Calculate checksum of user flash */
	chip_Checksum = (unsigned short)checksum_All -
		(unsigned short)checksum_Privileged;
	d->chip_cs = chip_Checksum;

	/* Read checksum from hex file */
	HEX_ReadChecksumData(&d->inf, &checksumData);

	/* Compare the checksum data of silicon and hex file */
	if (chip_Checksum != checksumData) {
		swd_PacketAck = swd_PacketAck | VERIFICATION_ERROR;
		return FAILURE;
	}

	return SUCCESS;
}

/******************************************************************************
 * Function Name: ReadHsspErrorStatus
 *******************************************************************************
 *
 * Summary:
 *  Returns the HSSP Error status in case of FAILURE return in any one of the
 *  programming steps.
 *
 * Parameters:
 *  None.
 *
 * Return:
 *  swd_PacketAck - Each bit of this 8-bit return value has a specific meaning.
 *
 * Note:
 *  Refer to the application note pdf for details on the Error status bit
 *  definitions
 ******************************************************************************/
unsigned char ReadHsspErrorStatus(void)
{
	return swd_PacketAck;
}

/******************************************************************************
 * Function Name: ExitProgrammingMode
 *******************************************************************************
 *
 * Summary:
 *  Releases the target PSoC 4 device from Programming mode.
 *
 * Parameters:
 *  None.
 *
 * Return:
 *  None.
 *
 * Note:
 *
 ******************************************************************************/
void ExitProgrammingMode(void)
{
	/* Drive the SWDIO, SWDCK outputs low */
	SetSwdckLow();
	SetSwdioLow();

	/* Make SWDIO, SWDCK High-Z after completing Programming */
	SetSwdioHizInput();
	SetSwdckHizInput();

	/* Generate active low rest pulse for 100 uS */
	SetXresCmosOutput();
	SetXresLow();
	DelayHundredUs();
	SetXresHigh();

	/* Make XRES High-Z after generating the reset pulse */
	SetXresHizInput();
}

/* [] END OF FILE */
