/****************************************************************************
 * FileName:        PDProtocol.c
 * Processor:       PIC32MX250F128B
 * Compiler:        MPLAB XC32
 * Company:         Fairchild Semiconductor
 *
 * Software License Agreement:
 *
 * The software supplied herewith by Fairchild Semiconductor (the Company)
 * is supplied to you, the Company's customer, for exclusive use with its
 * USB Type C / USB PD products.  The software is owned by the Company and/or
 * its supplier, and is protected under applicable copyright laws.
 * All rights are reserved. Any use in violation of the foregoing restrictions
 * may subject the user to criminal sanctions under applicable laws, as well
 * as to civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN AS IS CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 *****************************************************************************/
#include "PDProtocol.h"
#include "PDPolicy.h"
#include "TypeC.h"
#include "fusb30X.h"
#include "vdm/vdm_callbacks.h"
#include "vdm/vdm_callbacks_defs.h"
#include "vdm/vdm.h"
#include "vdm/vdm_types.h"
#include "vdm/bitfield_translators.h"

#include "platform.h"
#include "PD_Types.h"

/////////////////////////////////////////////////////////////////////////////
//      Variables for use with the USB PD state machine
/////////////////////////////////////////////////////////////////////////////

extern BOOL g_Idle;		// Puts state machine into Idle state
extern volatile UINT16 Timer_S;	// Tracks seconds elapsed for log timestamp
extern volatile UINT16 Timer_tms;	// Tracks tenths of milliseconds elapsed for log timestamp
extern StateLog PDStateLog;	// Log for tracking state transitions and times

// Debugging Variables
static UINT8 USBPDBuf[PDBUFSIZE];	// Circular buffer of all USB PD messages transferred
static UINT8 USBPDBufStart;	// Pointer to the first byte of the first message
static UINT8 USBPDBufEnd;	// Pointer to the last byte of the last message
static BOOL USBPDBufOverflow;	// Flag to indicate that there was a buffer overflow since last read

// Protocol Variables
ProtocolState_t ProtocolState;	// State variable for Protocol Layer
PDTxStatus_t PDTxStatus;	// Status variable for current transmission
static UINT8 MessageIDCounter;	// Current Tx message ID counter for SOP
static UINT8 MessageID;		// Last received message ID
BOOL ProtocolMsgRx;		// Flag to indicate if we have received a packet
SopType ProtocolMsgRxSop;	// SOP type of message received
static UINT8 ProtocolTxBytes;	// Number of bytes for the Tx FIFO
static UINT8 ProtocolTxBuffer[FSC_HOSTCOMM_BUFFER_SIZE];	// Buffer for device Tx FIFO
static UINT8 ProtocolRxBuffer[FSC_HOSTCOMM_BUFFER_SIZE];	// Buffer for device Rx FIFO
static UINT16 ProtocolTimer;	// Multi-function timer for the different protocol states
static UINT8 ProtocolCRC[4];
static UINT8 BISTErrorCounter;
BOOL ProtocolCheckRxBeforeTx;
UINT8 manualRetries = 0;	// Set to 1 to enable manual retries (instead of automatic)
UINT8 nTries = 4;		// Number of tries (1 + 3 retries)

/////////////////////////////////////////////////////////////////////////////
//                  Timer Interrupt service routine
/////////////////////////////////////////////////////////////////////////////
void ProtocolTickAt100us(void)
{
	if (!USBPDActive)
		return;

	if (ProtocolTimer)	// If the Protocol timer is greater than zero...
		ProtocolTimer--;	// Decrement it
}

void InitializePDProtocolVariables(void)
{
}

// ##################### USB PD Protocol Layer Routines ##################### //

void USBPDProtocol(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	if (g_Idle == TRUE) {
		g_Idle = FALSE;	// Run continuously (unmask all)
		Registers.Mask.byte = 0x20;
		DeviceWrite(regMask, 1, &Registers.Mask.byte);
		Registers.MaskAdv.byte[0] = 0x00;
		DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
		Registers.MaskAdv.M_GCRCSENT = 0;
		DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
		platform_enable_timer(TRUE);
	}
#endif
	if (Registers.Status.I_HARDRST) {
		ResetProtocolLayer(TRUE);	// Reset the protocol layer
		if (PolicyIsSource)	// If we are the source...
			PolicyState = peSourceTransitionDefault;	// set the source transition to default
		else		// Otherwise we are the sink...
			PolicyState = peSinkTransitionDefault;	// so set the sink transition to default
		PolicySubIndex = 0;
		StoreUSBPDToken(FALSE, pdtHardReset);	// Store the hard reset
	} else {
		switch (ProtocolState) {
		case PRLReset:
			ProtocolSendHardReset();	// Send a Hard Reset sequence
			PDTxStatus = txWait;	// Set the transmission status to wait to signal the policy engine
			ProtocolState = PRLResetWait;	// Go to the next state to wait for the reset signaling to complete
			ProtocolTimer = tBMCTimeout;	// Set a timeout so that we don't hang waiting for the reset to complete
			break;
		case PRLResetWait:	// Wait for the reset signaling to complete
			ProtocolResetWait();
			break;
		case PRLIdle:	// Waiting to send or receive a message
			ProtocolIdle();
			break;
		case PRLTxSendingMessage:	// We have attempted to transmit and are waiting for it to complete or detect a collision
			ProtocolSendingMessage();	// Determine which state we should go to next
			break;
		case PRLTxVerifyGoodCRC:	// Wait for message to be received and handle...
			ProtocolVerifyGoodCRC();
			break;
		case PRL_BIST_Rx_Reset_Counter:	// Reset BISTErrorCounter and preload PRBS
			protocolBISTRxResetCounter();
			break;
		case PRL_BIST_Rx_Test_Frame:	// Wait for test Frame form PHY
			protocolBISTRxTestFrame();
			break;
		case PRL_BIST_Rx_Error_Count:	// Construct and send BIST error count message to PHY
			protocolBISTRxErrorCount();
			break;
		case PRL_BIST_Rx_Inform_Policy:	// Inform policy engine error count has been sent
			protocolBISTRxInformPolicy();
			break;
		case PRLDisabled:	// In the disabled state, don't do anything
			break;
		default:
			break;
		}
	}
}

void ProtocolIdle(void)
{
	if (PDTxStatus == txReset)	// If we need to send a hard reset...
		ProtocolState = PRLReset;	// Set the protocol state to send it
	else if (!Registers.Status.RX_EMPTY)	// Otherwise check to see if we have received a message and sent a GoodCRC in response
	{
		ProtocolGetRxPacket();	// Grab the received message to pass up to the policy engine
		PDTxStatus = txIdle;	// Reset the transmitter status if we are receiving data (discard any packet to send)
		Registers.Status.I_GCRCSENT = 0;
	} else if (PDTxStatus == txSend)	// Otherwise check to see if there has been a request to send data...
	{
		ProtocolTransmitMessage();	// If so, send the message
	}
}

void ProtocolResetWait(void)
{
	if (Registers.Status.I_HARDSENT)	// Wait for the reset sequence to complete
	{
		ProtocolState = PRLIdle;	// Have the protocol state go to idle
		PDTxStatus = txSuccess;	// Alert the policy engine that the reset signaling has completed
	} else if (ProtocolTimer == 0)	// Wait for the BMCTimeout period before stating success in case the interrupts don't line up
	{
		ProtocolState = PRLIdle;	// Have the protocol state go to idle
		PDTxStatus = txSuccess;	// Assume that we have successfully sent a hard reset for now (may change in future)
	}
}

void ProtocolGetRxPacket(void)
{
	UINT32 i, j;
	UINT8 data[3];
	SopType rx_sop;
	UINT8 sop_token;

	// tmsTemp is used to get a start-of-function timestamp for getting timing data for debugging
	UINT16 tmsTemp = Timer_tms;

	DeviceRead(regFIFO, 3, &data[0]);	// Read the Rx token and two header bytes
	PolicyRxHeader.byte[0] = data[1];
	PolicyRxHeader.byte[1] = data[2];
	// Only setting the Tx header here so that we can store what we expect was sent in our PD buffer for the GUI
	PolicyTxHeader.word = 0;	// Clear the word to initialize for each transaction
	PolicyTxHeader.NumDataObjects = 0;	// Clear the number of objects since this is a command
	PolicyTxHeader.MessageType = CMTGoodCRC;	// Sets the message type to GoodCRC
	PolicyTxHeader.PortDataRole = PolicyIsDFP;	// Set whether the port is acting as a DFP or UFP
	PolicyTxHeader.PortPowerRole = PolicyIsSource;	// Set whether the port is serving as a power source or sink
	PolicyTxHeader.SpecRevision = USBPDSPECREV;	// Set the spec revision
	PolicyTxHeader.MessageID = PolicyRxHeader.MessageID;	// Update the message ID for the return packet

	// figure out what SOP* the data came in on
	rx_sop = TokenToSopType(data[0]);

	// only SOP supported today
	if (rx_sop == SOP_TYPE_SOP) {
		//ProtocolFlushTxFIFO();                                                  // Flush the Tx FIFO to make sure the correct data is in there (enable Auto Preamble)
		//ProtocolSendGoodCRC(rx_sop);                                                      // Send the GoodCRC message immediately to meet timing
		if ((PolicyRxHeader.NumDataObjects == 0)
		    && (PolicyRxHeader.MessageType == CMTSoftReset)) {
			MessageIDCounter = 0;	// Clear the message ID counter for tx
			MessageID = 0xFF;	// Reset the message ID (always alert policy engine of soft reset)
			ProtocolMsgRxSop = rx_sop;
			ProtocolMsgRx = TRUE;	// Set the flag to pass the message to the policy engine
			SourceCapsUpdated = TRUE;	// Set the source caps updated flag to indicate to the GUI to update the display
		} else if (PolicyRxHeader.MessageID != MessageID)	// If the message ID does not match the stored...
		{
			MessageID = PolicyRxHeader.MessageID;	// Update the stored message ID
			ProtocolMsgRxSop = rx_sop;
			ProtocolMsgRx = TRUE;	// Set the flag to pass the message to the policy engine
		}

		if (PolicyRxHeader.NumDataObjects > 0)	// Did we receive a data message? If so, we want to retrieve the data
		{
			DeviceRead(regFIFO, (PolicyRxHeader.NumDataObjects << 2), &ProtocolRxBuffer[0]);	// Grab the data from the FIFO
			for (i = 0; i < PolicyRxHeader.NumDataObjects; i++)	// Load the FIFO data into the data objects (loop through each object)
			{
				for (j = 0; j < 4; j++)	// Loop through each byte in the object
					PolicyRxDataObj[i].byte[j] = ProtocolRxBuffer[j + (i << 2)];	// Store the actual bytes
			}
		}
	} else {
		if (PolicyRxHeader.NumDataObjects > 0)	// Did we receive a data message? If so, we want to retrieve the data
		{
			DeviceRead(regFIFO, (PolicyRxHeader.NumDataObjects << 2), &ProtocolRxBuffer[0]);	// Grab the data from the FIFO
		}
	}
	DeviceRead(regFIFO, 4, &ProtocolCRC[0]);	// Read out the 4 CRC bytes to move the address to the next packet beginning
	DeviceRead(regStatus0, 2, &Registers.Status.byte[4]);	// Read the status bytes to update the ACTIVITY flag (should be clear)

	if (rx_sop == SOP_TYPE_SOP1) {
		PolicyRxHeader.word |= 0x8000;
		PolicyTxHeader.word |= 0x8000;
	}
	if (rx_sop == SOP_TYPE_SOP2) {
		PolicyRxHeader.word |= 0x0010;
		PolicyTxHeader.word |= 0x0010;
	}

	StoreUSBPDMessage(PolicyRxHeader, &PolicyRxDataObj[0], FALSE, data[0]);	// Store the received PD message for the device policy manager (VB GUI)
	if (rx_sop == SOP_TYPE_SOP)
		sop_token = 0xE0;
	if (rx_sop == SOP_TYPE_SOP1)
		sop_token = 0xC0;
	if (rx_sop == SOP_TYPE_SOP2)
		sop_token = 0xA0;
	if (rx_sop == SOP_TYPE_SOP)	// Only store that we send a GoodCRC if we actually send a GoodCRC
		StoreUSBPDMessage(PolicyTxHeader, &PolicyTxDataObj[0], TRUE, sop_token);	// Store the GoodCRC message that we have sent (SOP)

	/*
	   Special debug case where PD state log will provide the time elapsed in this function,
	   and the number of I2C bytes read during this period.
	 */
	WriteStateLog(&PDStateLog, dbgGetRxPacket, Timer_tms - tmsTemp,
		      PolicyRxHeader.NumDataObjects * 4 + 3 + 6);
}

void ProtocolTransmitMessage(void)
{
	UINT32 i, j;
	UINT8 sop_token;
	sopMainHeader_t temp_PolicyTxHeader;
	ProtocolFlushTxFIFO();	// Flush the Tx FIFO and disable the auto preamble

	ProtocolLoadSOP();
	sop_token = 0xE0;

	temp_PolicyTxHeader.word = PolicyTxHeader.word;
	temp_PolicyTxHeader.word &= 0x7FFF;
	temp_PolicyTxHeader.word &= 0xFFEF;

	if ((temp_PolicyTxHeader.NumDataObjects == 0)
	    && (temp_PolicyTxHeader.MessageType == CMTSoftReset)) {
		MessageIDCounter = 0;	// Clear the message ID counter if transmitting a soft reset
		MessageID = 0xFF;	// Reset the message ID if transmitting a soft reset
		SourceCapsUpdated = TRUE;	// Set the flag to indicate to the GUI to update the display
	}
	temp_PolicyTxHeader.MessageID = MessageIDCounter;	// Update the tx message id to send

	ProtocolTxBuffer[ProtocolTxBytes++] = PACKSYM | (2 + (temp_PolicyTxHeader.NumDataObjects << 2));	// Load the PACKSYM token with the number of bytes in the packet
	ProtocolTxBuffer[ProtocolTxBytes++] = temp_PolicyTxHeader.byte[0];	// Load in the first byte of the header
	ProtocolTxBuffer[ProtocolTxBytes++] = temp_PolicyTxHeader.byte[1];	// Load in the second byte of the header
	if (temp_PolicyTxHeader.NumDataObjects > 0)	// If this is a data object...
	{
		for (i = 0; i < temp_PolicyTxHeader.NumDataObjects; i++)	// Load the FIFO data into the data objects (loop through each object)
		{
			for (j = 0; j < 4; j++)	// Loop through each byte in the object
				ProtocolTxBuffer[ProtocolTxBytes++] = PolicyTxDataObj[i].byte[j];	// Load the actual bytes
		}
	}
	ProtocolLoadEOP();	// Load the CRC, EOP and stop sequence
	if (manualRetries) {
		manualRetriesTakeTwo();
	} else {
		DeviceWrite(regFIFO, ProtocolTxBytes, &ProtocolTxBuffer[0]);	// Commit the FIFO to the device

		/* sometimes it's important to check for a received message before sending */
		if (ProtocolCheckRxBeforeTx) {
			ProtocolCheckRxBeforeTx = FALSE;	// self-clear - one-time deal
			DeviceRead(regInterruptb, 1, &Registers.Status.byte[3]);
			if (Registers.Status.I_GCRCSENT) {
				/* if a message was received, bail */
				Registers.Status.I_GCRCSENT = 0;
				ProtocolFlushTxFIFO();
				PDTxStatus = txError;
				return;
			}
		}

		Registers.Control.TX_START = 1;	// Set the bit to enable the transmitter
		DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);	// Commit TX_START to the device
		Registers.Control.TX_START = 0;	// Clear this bit, to avoid inadvertently resetting

		PDTxStatus = txBusy;	// Set the transmitter status to busy
		ProtocolState = PRLTxSendingMessage;	// Set the protocol state to wait for the transmission to complete
		ProtocolTimer = tBMCTimeout;	// Set the protocol timer for ~2.5ms to allow the BMC to finish transmitting before timing out
	}
	StoreUSBPDMessage(temp_PolicyTxHeader, &PolicyTxDataObj[0], TRUE, sop_token);	// Store all messages that we attempt to send for debugging (SOP)

}

void ProtocolSendingMessage(void)
{
	if (Registers.Status.I_TXSENT) {
		ProtocolFlushTxFIFO();	// Flush the Tx FIFO
		ProtocolVerifyGoodCRC();
	} else if (Registers.Status.I_COLLISION)	// If there was a collision on the bus...
	{
		/* only really care about collisions for firmware retries */
		if (manualRetries) {
			ProtocolFlushTxFIFO();	// Discard the message and flush the Tx FIFO
			PDTxStatus = txCollision;	// Indicate to the policy engine that there was a collision with the last transmission
			ProtocolTimer = tBMCTimeout;	// Set a timeout so that we don't hang waiting for a packet
			ProtocolState = PRLRxWait;	// Go to the RxWait state to receive whatever message is incoming...
		}
	} else if (ProtocolTimer == 0)	// If we have timed out waiting for the transmitter to complete...
	{
		ProtocolFlushTxFIFO();	// Discard the message and flush the Tx FIFO
		ProtocolFlushRxFIFO();	// Flush the Rx FIFO
		PDTxStatus = txError;	// Set the transmission status to error to signal the policy engine
		ProtocolState = PRLIdle;	// Set the state variable to the idle state
	}
}

void ProtocolVerifyGoodCRC(void)
{
	UINT32 i, j;
	UINT8 data[3];
	SopType s;

	DeviceRead(regFIFO, 3, &data[0]);	// Read the Rx token and two header bytes
	PolicyRxHeader.byte[0] = data[1];
	PolicyRxHeader.byte[1] = data[2];
	if ((PolicyRxHeader.NumDataObjects == 0)
	    && (PolicyRxHeader.MessageType == CMTGoodCRC)) {
		UINT8 MIDcompare;
		switch (TokenToSopType(data[0])) {
		case SOP_TYPE_SOP:
			MIDcompare = MessageIDCounter;
			break;
		default:
			MIDcompare = -1;
			break;
		}

		if (PolicyRxHeader.MessageID != MIDcompare)	// If the message ID doesn't match...
		{
			DeviceRead(regFIFO, 4, &ProtocolCRC[0]);	// Read out the 4 CRC bytes to move the address to the next packet beginning
			StoreUSBPDToken(FALSE, pdtBadMessageID);	// Store that there was a bad message ID received in the buffer
			PDTxStatus = txError;	// Set the transmission status to error to signal the policy engine
			ProtocolState = PRLIdle;	// Set the state variable to the idle state
		} else		// Otherwise, we've received a good CRC response to our message sent
		{
			switch (TokenToSopType(data[0])) {
			case SOP_TYPE_SOP:
				MessageIDCounter++;	// Increment the message ID counter
				MessageIDCounter &= 0x07;	// Rollover the counter so that it fits
				break;
			default:
				// nope
				break;
			}

			ProtocolState = PRLIdle;	// Set the idle state
			PDTxStatus = txSuccess;	// Set the transmission status to success to signal the policy engine
			DeviceRead(regFIFO, 4, &ProtocolCRC[0]);	// Read out the 4 CRC bytes to move the address to the next packet beginning
			StoreUSBPDMessage(PolicyRxHeader, &PolicyRxDataObj[0], FALSE, data[0]);	// Store the received PD message for the device policy manager (VB GUI)
		}
	} else {
		ProtocolState = PRLIdle;	// Set the idle protocol state (let the policy engine decide next steps)
		PDTxStatus = txError;	// Flag the policy engine that we didn't successfully transmit

		s = TokenToSopType(data[0]);
		if ((PolicyRxHeader.NumDataObjects == 0)
		    && (PolicyRxHeader.MessageType == CMTSoftReset)) {
			DeviceRead(regFIFO, 4, &ProtocolCRC[0]);	// Read out the 4 CRC bytes to move the address to the next packet beginning

			MessageIDCounter = 0;	// Clear the message ID counter for tx
			MessageID = 0xFF;	// Reset the message ID (always alert policy engine of soft reset)
			ProtocolMsgRx = TRUE;	// Set the flag to pass the message to the policy engine
			ProtocolMsgRxSop = s;
			SourceCapsUpdated = TRUE;	// Set the flag to indicate to the GUI to update the display
		} else if (PolicyRxHeader.MessageID != MessageID)	// If the message ID does not match the stored...
		{
			DeviceRead(regFIFO, 4, &ProtocolCRC[0]);	// Read out the 4 CRC bytes to move the address to the next packet beginning
			MessageID = PolicyRxHeader.MessageID;	// Update the stored message ID
			ProtocolMsgRx = TRUE;	// Set the flag to pass the message to the policy engine
			ProtocolMsgRxSop = s;
		}
		if (PolicyRxHeader.NumDataObjects > 0)	// If this is a data message, grab the data objects
		{
			DeviceRead(regFIFO, PolicyRxHeader.NumDataObjects << 2, &ProtocolRxBuffer[0]);	// Grab the data from the FIFO
			for (i = 0; i < PolicyRxHeader.NumDataObjects; i++)	// Load the FIFO data into the data objects (loop through each object)
			{
				for (j = 0; j < 4; j++)	// Loop through each byte in the object
					PolicyRxDataObj[i].byte[j] = ProtocolRxBuffer[j + (i << 2)];	// Store the actual bytes
			}
		}
		StoreUSBPDMessage(PolicyRxHeader, &PolicyRxDataObj[0], FALSE, data[0]);	// Store the received PD message for the device policy manager (VB GUI)
	}

}

void ProtocolSendGoodCRC(SopType sop)
{
	if (sop == SOP_TYPE_SOP) {
		ProtocolLoadSOP();	// Initialize and load the start sequence
	} else {
		return;		// only supporting SOPs today!
	}

	ProtocolTxBuffer[ProtocolTxBytes++] = PACKSYM | 0x02;	// Load in the PACKSYM token with the number of data bytes in the packet
	ProtocolTxBuffer[ProtocolTxBytes++] = PolicyTxHeader.byte[0];	// Load in the first byte of the header
	ProtocolTxBuffer[ProtocolTxBytes++] = PolicyTxHeader.byte[1];	// Load in the second byte of the header
	ProtocolLoadEOP();	// Load the CRC, EOP and stop sequence
	DeviceWrite(regFIFO, ProtocolTxBytes, &ProtocolTxBuffer[0]);	// Commit the FIFO to the device
	DeviceRead(regStatus0, 2, &Registers.Status.byte[4]);	// Read the status bytes to update the ACTIVITY flag (should be set)
}

void ProtocolLoadSOP(void)
{
	ProtocolTxBytes = 0;	// Clear the Tx byte counter
	ProtocolTxBuffer[ProtocolTxBytes++] = SYNC1_TOKEN;	// Load in the Sync-1 pattern
	ProtocolTxBuffer[ProtocolTxBytes++] = SYNC1_TOKEN;	// Load in the Sync-1 pattern
	ProtocolTxBuffer[ProtocolTxBytes++] = SYNC1_TOKEN;	// Load in the Sync-1 pattern
	ProtocolTxBuffer[ProtocolTxBytes++] = SYNC2_TOKEN;	// Load in the Sync-2 pattern
//    current_sop = SOP_TYPE_SOP;
}

void ProtocolLoadEOP(void)
{
	ProtocolTxBuffer[ProtocolTxBytes++] = JAM_CRC;	// Load in the token to calculate and add the CRC
	ProtocolTxBuffer[ProtocolTxBytes++] = EOP;	// Load in the EOP pattern
	ProtocolTxBuffer[ProtocolTxBytes++] = TXOFF;	// Load in the PD stop sequence (turn off the transmitter)
}

void ProtocolSendHardReset(void)
{
	UINT8 data;
	data = Registers.Control.byte[3] | 0x40;	// Set the send hard reset bit
	DeviceWrite(regControl3, 1, &data);	// Send the hard reset
	StoreUSBPDToken(TRUE, pdtHardReset);	// Store the hard reset
}

void ProtocolFlushRxFIFO(void)
{
	UINT8 data;
	data = Registers.Control.byte[1];	// Grab the current control word
	data |= 0x04;		// Set the RX_FLUSH bit (auto-clears)
	DeviceWrite(regControl1, 1, &data);	// Commit the flush to the device
}

void ProtocolFlushTxFIFO(void)
{
	UINT8 data;
	data = Registers.Control.byte[0];	// Grab the current control word
	data |= 0x40;		// Set the TX_FLUSH bit (auto-clears)
	DeviceWrite(regControl0, 1, &data);	// Commit the flush to the device
}

void ResetProtocolLayer(BOOL ResetPDLogic)
{
	UINT32 i;
	UINT8 data = 0x02;
	if (ResetPDLogic)
		DeviceWrite(regReset, 1, &data);	// Reset the PD logic
	ProtocolFlushRxFIFO();	// Flush the Rx FIFO
	ProtocolFlushTxFIFO();	// Flush the Tx FIFO
	ProtocolState = PRLIdle;	// Initialize the protocol layer to the idle state
	PDTxStatus = txIdle;	// Initialize the transmitter status
	ProtocolTimer = 0;	// Reset the protocol state timer
	VdmTimer = 0;
	VdmTimerStarted = FALSE;
	ProtocolTxBytes = 0;	// Clear the byte count for the Tx FIFO
	MessageIDCounter = 0;	// Clear the message ID counters
	MessageID = 0xFF;	// Reset the message ID (invalid value to indicate nothing received yet)
	ProtocolMsgRx = FALSE;	// Reset the message ready flag
	ProtocolMsgRxSop = SOP_TYPE_SOP;
	USBPDTxFlag = FALSE;	// Clear the flag to make sure we don't send something by accident
	PolicyHasContract = FALSE;	// Clear the flag that indicates we have a PD contract
	USBPDContract.object = 0;	// Clear the actual USBPD contract request object
	SourceCapsUpdated = TRUE;	// Update the source caps flag to trigger an update of the GUI
	CapsHeaderReceived.word = 0;	// Clear any received capabilities messages
	for (i = 0; i < 7; i++)	// Loop through all the received capabilities objects
		CapsReceived[i].object = 0;	// Clear each object
}

// ------- BIST Receiver Test -------- //

void protocolBISTRxResetCounter(void)	// Reset BISTErrorCounter and preload PRBS
{
	BISTErrorCounter = 0;	// Request Physical Layer to reset BISTErrorCounter and preload the PRBS
	// Preload PRBS
	ProtocolState = PRL_BIST_Rx_Test_Frame;	// Transition to PRL_BIST_Rx_Test_Frame when BISTErrorCounter has been reset
}

void protocolBISTRxTestFrame(void)	// Wait for test Frame form PHY
{
	// Wait for the Physical Layer to receive the next test frame
	// Transition to PRL_BIST_Rx_Error_count when current value of BISTErrorCounter is received from PHY layer
}

void protocolBISTRxErrorCount(void)	// Construct and send BIST error count message to PHY
{
	// Construct a BIST message with BIST Data Object of Returned BIST Counters using BISTErrorCounter value from PHY layer
	// Pass BIST Message to PHY layer for transmission
	// Transition to PRL_BIST_Rx_Inform_Policy when BIST message has been sent
}

void protocolBISTRxInformPolicy(void)	// Inform policy engine error count has been sent
{
	// Inform policy engine that BIST message containing BISTErrorCounter has been sent
	// Transition to PRL_BIST_Rx_Test_Frame when policy engine has been informed
}

// ####################### USB PD Debug Buffer Routines ##################### //

BOOL StoreUSBPDToken(BOOL transmitter, USBPD_BufferTokens_t token)
{
	UINT8 header1 = 1;	// Declare and set the message size
	if (ClaimBufferSpace(2) == FALSE)	// Attempt to claim the number of bytes required in the buffer
		return FALSE;	// If there was an error, return that we failed
	if (transmitter)	// If we are the transmitter...
		header1 |= 0x40;	// set the transmitter bit
	USBPDBuf[USBPDBufEnd++] = header1;	// Set the first header byte (Token type, direction and size)
	USBPDBufEnd %= PDBUFSIZE;	// Wrap the pointer if it is too large
	token &= 0x0F;		// Build the 2nd header byte
	USBPDBuf[USBPDBufEnd++] = token;	// Set the second header byte (actual token)
	USBPDBufEnd %= PDBUFSIZE;	// Wrap the pointer if it is too large
	return TRUE;
}

BOOL StoreUSBPDMessage(sopMainHeader_t Header, doDataObject_t * DataObject,
		       BOOL transmitter, UINT8 SOPToken)
{
	UINT32 i, j, required;
	UINT8 header1;
	required = Header.NumDataObjects * 4 + 2 + 2;	// Determine how many bytes are needed for the buffer
	if (ClaimBufferSpace(required) == FALSE)	// Attempt to claim the number of bytes required in the buffer
		return FALSE;	// If there was an error, return that we failed
	header1 = (0x1F & (required - 1)) | 0x80;
	if (transmitter)	// If we were the transmitter
		header1 |= 0x40;	// Set the flag to indicate to the host
	USBPDBuf[USBPDBufEnd++] = header1;	// Set the first header byte (PD message flag, direction and size)
	USBPDBufEnd %= PDBUFSIZE;	// Wrap the pointer if it is too large
	SOPToken &= 0xE0;	// Build the 2nd header byte
	SOPToken >>= 5;		// Shift the token into place
	USBPDBuf[USBPDBufEnd++] = SOPToken;	// Set the second header byte (PD message type)
	USBPDBufEnd %= PDBUFSIZE;	// Wrap the pointer if it is too large
	USBPDBuf[USBPDBufEnd++] = Header.byte[0];	// Set the first byte and increment the pointer
	USBPDBufEnd %= PDBUFSIZE;	// Wrap the pointer if it is too large
	USBPDBuf[USBPDBufEnd++] = Header.byte[1];	// Set the second byte and increment the pointer
	USBPDBufEnd %= PDBUFSIZE;	// Wrap the pointer if it is too large
	for (i = 0; i < Header.NumDataObjects; i++)	// Loop through all the data objects
	{
		for (j = 0; j < 4; j++) {
			USBPDBuf[USBPDBufEnd++] = DataObject[i].byte[j];	// Set the byte of the data object and increment the pointer
			USBPDBufEnd %= PDBUFSIZE;	// Wrap the pointer if it is too large
		}
	}
	return TRUE;
}

UINT8 GetNextUSBPDMessageSize(void)
{
	UINT8 numBytes;
	if (USBPDBufStart == USBPDBufEnd)	// If the start and end are equal, the buffer is empty
		numBytes = 0;	// Clear the number of bytes so that we return 0
	else			// otherwise there is data in the buffer...
		numBytes = (USBPDBuf[USBPDBufStart] & 0x1F) + 1;	// Get the number of bytes associated with the message
	return numBytes;
}

UINT8 GetUSBPDBufferNumBytes(void)
{
	UINT8 bytes;
	if (USBPDBufStart == USBPDBufEnd)	// If the buffer is empty (using the keep one slot open approach)
		bytes = 0;	// return 0
	else if (USBPDBufEnd > USBPDBufStart)	// If the buffer hasn't wrapped...
		bytes = USBPDBufEnd - USBPDBufStart;	// simply subtract the end from the beginning
	else			// Otherwise it has wrapped...
		bytes = USBPDBufEnd + (PDBUFSIZE - USBPDBufStart);	// calculate the available this way
	return bytes;
}

BOOL ClaimBufferSpace(INT32 intReqSize)
{
	INT32 available;
	UINT8 numBytes;
	if (intReqSize >= PDBUFSIZE)	// If we cannot claim enough space...
		return FALSE;	// Get out of here
	if (USBPDBufStart == USBPDBufEnd)	// If the buffer is empty (using the keep one slot open approach)
		available = PDBUFSIZE;	// Buffer is empty...
	else if (USBPDBufStart > USBPDBufEnd)	// If the buffer has wrapped...
		available = USBPDBufStart - USBPDBufEnd;	// calculate this way
	else			// Otherwise
		available = PDBUFSIZE - (USBPDBufEnd - USBPDBufStart);	// calculate the available this way
	do {
		if (intReqSize >= available)	// If we don't have enough room in the buffer, we need to make room (always keep 1 spot open)
		{
			USBPDBufOverflow = TRUE;	// Set the overflow flag to alert the GUI that we are overwriting data
			numBytes = GetNextUSBPDMessageSize();	// Get the size of the next USB PD message in the buffer
			if (numBytes == 0)	// If the buffer is empty...
				return FALSE;	// Return FALSE since the data cannot fit in the available buffer size (nothing written)
			available += numBytes;	// Add the recovered bytes to the number available
			USBPDBufStart += numBytes;	// Adjust the pointer to the new starting address
			USBPDBufStart %= PDBUFSIZE;	// Wrap the pointer if necessary
		} else
			break;
	} while (1);		// Loop until we have enough bytes
	return TRUE;
}

// ##################### USB HID Commmunication Routines #################### //

void GetUSBPDStatus(UINT8 abytData[])
{
	UINT32 i, j;
	UINT32 intIndex = 0;
	abytData[intIndex++] = GetUSBPDStatusOverview();	// Grab a snapshot of the top level status
	abytData[intIndex++] = GetUSBPDBufferNumBytes();	// Get the number of bytes in the PD buffer
	abytData[intIndex++] = PolicyState;	// Get the current policy engine state
	abytData[intIndex++] = PolicySubIndex;	// Get the current policy sub index
	abytData[intIndex++] = (ProtocolState << 4) | PDTxStatus;	// Get the protocol state and transmitter status
	for (i = 0; i < 4; i++)
		abytData[intIndex++] = USBPDContract.byte[i];	// Get each byte of the current contract
	if (PolicyIsSource) {
		abytData[intIndex++] = CapsHeaderSource.byte[0];	// Get the first byte of the received capabilities header
		abytData[intIndex++] = CapsHeaderSource.byte[1];	// Get the second byte of the received capabilities header
		for (i = 0; i < 7; i++)	// Loop through each data object
		{
			for (j = 0; j < 4; j++)	// Loop through each byte of the data object
				abytData[intIndex++] = CapsSource[i].byte[j];	// Get each byte of each power data object
		}
	} else {
		abytData[intIndex++] = CapsHeaderReceived.byte[0];	// Get the first byte of the received capabilities header
		abytData[intIndex++] = CapsHeaderReceived.byte[1];	// Get the second byte of the received capabilities header
		for (i = 0; i < 7; i++)	// Loop through each data object
		{
			for (j = 0; j < 4; j++)	// Loop through each byte of the data object
				abytData[intIndex++] = CapsReceived[i].byte[j];	// Get each byte of each power data object
		}
	}

//    abytData[43] = ProtocolTxBytes;
//    for (i=0;i<14;i++)
//    {
//        abytData[42+i] = ProtocolTxBuffer[i];
//    }

	// We are going to return the Registers for now for debugging purposes
	// These will be removed eventually and a new command will likely be added
	// For now, offset the registers by 16 from the beginning to get them out of the way
	intIndex = 44;
	abytData[intIndex++] = Registers.DeviceID.byte;	// 52
	abytData[intIndex++] = Registers.Switches.byte[0];	// 53
	abytData[intIndex++] = Registers.Switches.byte[1];
	abytData[intIndex++] = Registers.Measure.byte;
	abytData[intIndex++] = Registers.Slice.byte;
	abytData[intIndex++] = Registers.Control.byte[0];	// 57
	abytData[intIndex++] = Registers.Control.byte[1];
	abytData[intIndex++] = Registers.Mask.byte;
	abytData[intIndex++] = Registers.Power.byte;
	abytData[intIndex++] = Registers.Status.byte[4];	// Status0 - 61
	abytData[intIndex++] = Registers.Status.byte[5];	// Status1 - 62
	abytData[intIndex++] = Registers.Status.byte[6];	// Interrupt1 - 63
}

UINT8 GetUSBPDStatusOverview(void)
{
	UINT8 status = 0;
	if (USBPDEnabled)
		status |= 0x01;
	if (USBPDActive)
		status |= 0x02;
	if (PolicyIsSource)
		status |= 0x04;
	if (PolicyIsDFP)
		status |= 0x08;
	if (PolicyHasContract)
		status |= 0x10;
	if (SourceCapsUpdated)
		status |= 0x20;
	SourceCapsUpdated = FALSE;
	if (USBPDBufOverflow)
		status |= 0x80;
	return status;
}

UINT8 ReadUSBPDBuffer(UINT8 * pData, UINT8 bytesAvail)
{
	UINT8 i, msgSize, bytesRead;
	bytesRead = 0;
	do {
		msgSize = GetNextUSBPDMessageSize();	// Grab the next message size
//        *pData++ = USBPDBufStart;                                               // 6
//        *pData++ = USBPDBufEnd;                                                 // 7
//        *pData++ = (USBPDBufStart + 1) % PDBUFSIZE;                             // 8
//       *pData++ = USBPDBuf[(USBPDBufStart + 1) % PDBUFSIZE];                   // 9
//        *pData++ = (USBPDBuf[(USBPDBufStart + 1) % PDBUFSIZE] & 0x70) >> 2;     // 10
//        *pData++ = ((USBPDBuf[(USBPDBufStart + 1) % PDBUFSIZE] & 0x70) >> 2) + 2; // 11
//        for (i=0; i<32; i++)
//            *pData++ = USBPDBuf[i];
		if ((msgSize != 0) && (msgSize <= bytesAvail))	// If there is data and the message will fit...
		{
			for (i = 0; i < msgSize; i++)	// Loop through all of the bytes for the message
			{
				*pData++ = USBPDBuf[USBPDBufStart++];	// Retrieve the bytes, increment both pointers
				USBPDBufStart %= PDBUFSIZE;	// Wrap the start pointer if necessary
			}
			bytesAvail -= msgSize;	// Decrement the number of bytes available
			bytesRead += msgSize;	// Increment the number of bytes read
		} else		// If there is no data or the message won't fit...
			break;	// Break out of the loop
	} while (1);
	return bytesRead;
}

void SendUSBPDMessage(UINT8 * abytData)
{
	UINT32 i, j;
	PDTransmitHeader.byte[0] = *abytData++;	// Set the 1st PD header byte
	PDTransmitHeader.byte[1] = *abytData++;	// Set the 2nd PD header byte
	for (i = 0; i < PDTransmitHeader.NumDataObjects; i++)	// Loop through all the data objects
	{
		for (j = 0; j < 4; j++)	// Loop through each byte of the object
		{
			PDTransmitObjects[i].byte[j] = *abytData++;	// Set the actual bytes
		}
	}
	USBPDTxFlag = TRUE;	// Set the flag to send when appropriate
}

void manualRetriesTakeTwo(void)
{
	UINT8 tries = nTries;
	// Mask for only I_RETRYFAIL and I_TXSENT and I_COLLISION
	Registers.Mask.byte = ~0x02;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = ~0x14;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 1;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);

	// Make sure interrupts are cleared
	DeviceRead(regInterrupt, 1, &Registers.Status.byte[6]);
	DeviceRead(regInterrupta, 1, &Registers.Status.byte[2]);
	DeviceRead(regInterruptb, 1, &Registers.Status.byte[3]);
	Registers.Status.I_TXSENT = 0;	// Clear interrupt
	Registers.Status.I_RETRYFAIL = 0;	// Clear interrupt
	Registers.Status.I_COLLISION = 0;	// Clear interrupt

	// Load TxFIFO
	DeviceWrite(regFIFO, ProtocolTxBytes, &ProtocolTxBuffer[0]);	// Commit the FIFO to the device

	while (tries) {
		// Write start
		Registers.Control.TX_START = 1;	// Set the bit to enable the transmitter
		DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);	// Commit TX_START to the device
		Registers.Control.TX_START = 0;	// Clear this bit, to avoid inadvertently resetting

		// Wait until we get a good CRC or timeout
		while (!platform_get_device_irq_state()) ;	// Loops until I_TxSent or I_RETRYFAIL or I_COLLISION
		DeviceRead(regInterrupt, 1, &Registers.Status.byte[6]);	// Read to clear interrupt register

		DeviceRead(regInterrupta, 1, &Registers.Status.byte[2]);

		if (Registers.Status.I_TXSENT) {
			//Success!
			Registers.Status.I_TXSENT = 0;	// Clear interrupt
			ProtocolVerifyGoodCRC();
			ProtocolState = PRLIdle;	// Set the idle state
			PDTxStatus = txSuccess;	// Set the transmission status to success to signal the policy engine
			tries = 0;
		} else if (Registers.Status.I_RETRYFAIL) {
			Registers.Status.I_RETRYFAIL = 0;	// Clear interrupt
			tries--;	// Decrement Retries
			if (!tries)	// If no more retries, set as failure
			{
				// Failure :(
				ProtocolState = PRLIdle;	// Set the idle state
				PDTxStatus = txError;	// Set the transmission status to error to signal the policy engine
			} else {
				// Load TxFIFO
				DeviceWrite(regFIFO, ProtocolTxBytes, &ProtocolTxBuffer[0]);	// Commit the FIFO to the device
			}
		} else if (Registers.Status.I_COLLISION)	// Must be I_COLLISION
		{
			Registers.Status.I_COLLISION = 0;	// Clear interrupt
			DeviceRead(regStatus0, 1, &Registers.Status.byte[4]);	// Wait for status to go low
//            while(Registers.Status.ACTIVITY)
//            {
//                DeviceRead(regStatus0, 1, &Registers.Status.byte[4]);
//            }
		}
	}

	// Re-enable Masks
	Registers.Mask.byte = 0x20;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0x00;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 0;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
}

void setManualRetries(UINT8 mode)
{
	manualRetries = mode;
}

UINT8 getManualRetries(void)
{
	return manualRetries;
}
