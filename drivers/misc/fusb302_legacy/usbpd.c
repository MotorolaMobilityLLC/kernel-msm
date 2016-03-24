 /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  *
  * Software License Agreement:
  *
  * The software supplied herewith by Fairchild Semiconductor (the Company)
  * is supplied to you, the Company's customer, for exclusive use with its
  * USB Type C / USB PD products.  The software is owned by the Company and/or
  * its supplier, and is protected under applicable copyright laws.
  * All rights are reserved. Any use in violation of the foregoing restrictions
  * may subject the user to criminal sanctions under applicable laws, as well
  * as to civil liability for the breach of the terms and conditions of this
  * license.
  *
  * THIS SOFTWARE IS PROVIDED IN AN AS IS CONDITION. NO WARRANTIES,
  * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
  * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
  * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
  * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
  * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
  *
  *****************************************************************************/
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/kthread.h>

//#include "callback_defs.h"
//#include "vdm_manager.h"
//#include "vdm_types.h"
//#include "vdm_bitfield_translators.h"
#include "usbpd.h"
#include "fusb302.h"

#define FUSB302_DEBUG

#ifdef FUSB302_DEBUG
#define FUSB_LOG(fmt, args...)  pr_debug("[fusbpd]" fmt, ##args)
#else
#define FUSB_LOG(fmt, args...)
#endif

/////////////////////////////////////////////////////////////////////////////
//      Variables for use with the USB PD state machine
/////////////////////////////////////////////////////////////////////////////
extern FUSB300reg_t Registers;	// Variable holding the current status of the FUSB300 registers
extern bool USBPDActive;	// Variable to indicate whether the USB PD state machine is active or not
extern bool USBPDEnabled;	// Variable to indicate whether USB PD is enabled (by the host)
extern u32 PRSwapTimer;		// Timer used to bail out of a PR_Swap from the Type-C side if necessary
extern USBTypeCPort PortType;	// Variable indicating which type of port we are implementing
extern bool blnCCPinIsCC1;	// Flag to indicate if the CC1 pin has been detected as the CC pin
extern bool blnCCPinIsCC2;	// Flag to indicate if the CC2 pin has been detected as the CC pin
extern bool blnSMEnabled;	// Variable to indicate whether the 300 state machine is enabled
extern ConnectionState ConnState;	// Variable indicating the current Type-C connection state

extern bool state_changed;
extern int VBUS_5V_EN;
extern int VBUS_12V_EN;

// Debugging Variables
static u8 USBPDBuf[PDBUFSIZE];	// Circular buffer of all USB PD messages transferred
static u8 USBPDBufStart;	// Pointer to the first byte of the first message
static u8 USBPDBufEnd;		// Pointer to the last byte of the last message
static bool USBPDBufOverflow;	// Flag to indicate that there was a buffer overflow since last read

// Device Policy Manager Variables
bool USBPDTxFlag;		// Flag to indicate that we need to send a message (set by device policy manager)

sopMainHeader_t PDTransmitHeader;	// Defintion of the PD packet to send
sopMainHeader_t CapsHeaderSink;	// Definition of the sink capabilities of the device
sopMainHeader_t CapsHeaderSource;	// Definition of the source capabilities of the device
sopMainHeader_t CapsHeaderReceived;	// Last capabilities header received (source or sink)
doDataObject_t PDTransmitObjects[7];	// Data objects to send
doDataObject_t CapsSink[7];	// Power object definitions of the sink capabilities of the device
doDataObject_t CapsSource[7];	// Power object definitions of the source capabilities of the device
doDataObject_t CapsReceived[7];	// Last power objects received (source or sink)
doDataObject_t USBPDContract;	// Current USB PD contract (request object)
doDataObject_t SinkRequest;	// Sink request message
u32 SinkRequestMaxVoltage;	// Maximum voltage that the sink will request
u32 SinkRequestMaxPower;	// Maximum power the sink will request (used to calculate current as well)
u32 SinkRequestOpPower;		// Operating power the sink will request (used to calculate current as well)
bool SinkGotoMinCompatible;	// Whether the sink will respond to the GotoMin command
bool SinkUSBSuspendOperation;	// Whether the sink wants to continue operation during USB suspend
bool SinkUSBCommCapable;	// Whether the sink is USB communications capable
bool SourceCapsUpdated;		// Flag to indicate whether we have updated the source caps (for the GUI)

// Policy Variables
PolicyState_t PolicyState;	// State variable for Policy Engine
static u8 PolicySubIndex;	// Sub index for policy states
static bool PolicyIsSource;	// Flag to indicate whether we are acting as a source or a sink
static bool PolicyIsDFP;	// Flag to indicate whether we are acting as a UFP or DFP
static bool PolicyHasContract;	// Flag to indicate whether there is a contract in place
static u8 CollisionCounter;	// Collision counter for the policy engine
static u8 HardResetCounter;	// The number of times a hard reset has been generated
static u8 CapsCounter;		// Number of capabilities messages sent
static u32 PolicyStateTimer;	// Multi-function timer for the different policy states
static u32 NoResponseTimer;	// Policy engine no response timer
static sopMainHeader_t PolicyRxHeader;	// Header object for USB PD messages received
static sopMainHeader_t PolicyTxHeader;	// Header object for USB PD messages to send
static doDataObject_t PolicyRxDataObj[7];	// Buffer for data objects received
static doDataObject_t PolicyTxDataObj[7];	// Buffer for data objects to send

// Protocol Variables
ProtocolState_t ProtocolState;	// State variable for Protocol Layer
PDTxStatus_t PDTxStatus;	// Status variable for current transmission
static u8 MessageIDCounter;	// Current Tx message ID counter
static u8 MessageID;		// Last received message ID
static bool ProtocolMsgRx;	// Flag to indicate if we have received a packet
static bool ProtocolMsgTx;
static u8 ProtocolTxBytes;	// Number of bytes for the Tx FIFO
static u8 ProtocolTxBuffer[64];	// Buffer for FUSB300 Tx FIFO (same size as the 300 tx buffer)
static u8 ProtocolRxBuffer[64];	// Buffer for FUSB300 Rx FIFO (same size as the 300 rx buffer)
static u16 ProtocolTimer;	// Multi-function timer for the different protocol states
static u8 ProtocolCRC[4];

// VDM Manager object
VdmManager vdmm;

#define FUSB_MS_TO_NS(x) (x * 1000 * 1000)
#define Delay10us(x) udelay(x*10);

extern void wake_up_statemachine(void);

static struct hrtimer protocol_hrtimer;
static struct hrtimer policystate_hrtimer;
static struct hrtimer noresponse_hrtimer;
static struct hrtimer prswap_hrtimer;

void set_policy_state(PolicyState_t st)
{
	PolicyState = st;
	state_changed = true;
	FUSB_LOG("set PolicyState=%d\n", st);
}

void set_protocol_state(ProtocolState_t st)
{
	ProtocolState = st;
	state_changed = true;
	FUSB_LOG("set ProtocolState =%d\n", st);
}

void set_pdtx_state(PDTxStatus_t st)
{
	PDTxStatus = st;
	state_changed = true;
	FUSB_LOG("set PDTxStatus=%d\n", st);
}

void set_policy_subindex(u8 index)
{
	PolicySubIndex = index;
	state_changed = true;
}

void increase_policy_subindex(void)
{
	PolicySubIndex++;
	state_changed = true;
}

void usbpd_start_timer(struct hrtimer *timer, int ms)
{
	ktime_t ktime;
	ktime = ktime_set(0, FUSB_MS_TO_NS(ms));
	hrtimer_start(timer, ktime, HRTIMER_MODE_REL);
}

enum hrtimer_restart pd_func_hrtimer(struct hrtimer *timer)
{
	if (timer == &protocol_hrtimer)
		ProtocolTimer = 0;
	else if (timer == &policystate_hrtimer)
		PolicyStateTimer = 0;
	else if (timer == &noresponse_hrtimer)
		NoResponseTimer = 0;
	else if (timer == &prswap_hrtimer)
		PRSwapTimer = 0;

	FUSB_LOG
	    ("%s, ProtocolTimer=%d, PolicyStateTimer=%d, NoResponseTimer=%d, PRSwapTimer=%d\n",
	     __func__, ProtocolTimer, PolicyStateTimer, NoResponseTimer,
	     PRSwapTimer);

	wake_up_statemachine();

	return HRTIMER_NORESTART;
}

int FUSB300WriteFIFO(unsigned char length, unsigned char *data)
{
	unsigned char llen = length;
	unsigned char i = 0;
	int ret = 0;
	char log[200];
	//FUSB_LOG("%s, length=%d\n", __func__, length);
	ret = sprintf(log, ">>>>");
	for (i = 0; i < length; i++)
		ret += sprintf(log + ret, " %2x", data[i]);
	FUSB_LOG("%s\n", log);

	ret = 0;
	i = 0;

	while (llen > 4) {
		ret = FUSB300Write(regFIFO, 4, &data[i << 2]);
		i++;
		llen -= 4;
	}
	if (llen > 0)
		ret = FUSB300Write(regFIFO, llen, &data[i << 2]);
	return ret;
}

int FUSB300ReadFIFO(unsigned char length, unsigned char *data)
{
	unsigned char llen = length;
	unsigned char i = 0;
	int ret = 0;
	char log[200];
	//FUSB_LOG("%s, length=%d\n", __func__, length);

	while (llen > 8) {
		ret = FUSB300Read(regFIFO, 8, &data[i << 3]);
		i++;
		llen -= 8;
	}
	if (llen > 0)
		ret = FUSB300Read(regFIFO, llen, &data[i << 3]);

	ret = sprintf(log, "<<<<<");
	for (i = 0; i < length; i++)
		ret += sprintf(log + ret, " %2x", data[i]);
	FUSB_LOG("%s\n", log);

	return ret;
}

void InitializeUSBPDVariables(void)
{
	USBPDBufStart = 0;	// Reset the starting pointer for the circular buffer
	USBPDBufEnd = 0;	// Reset the ending pointer for the circular buffer
	USBPDBufOverflow = false;	// Clear the overflow flag for the circular buffer
	SinkRequestMaxVoltage = 240;	// Maximum voltage that the sink will request (12V)
	SinkRequestMaxPower = 1000;	// Maximum power the sink will request (0.5W, used to calculate current as well)
	SinkRequestOpPower = 1000;	// Operating power the sink will request (0.5W, sed to calculate current as well)
	SinkGotoMinCompatible = false;	// Whether the sink will respond to the GotoMin command
	SinkUSBSuspendOperation = false;	// Whether the sink wants to continue operation during USB suspend
	SinkUSBCommCapable = false;	// Whether the sink is USB communications capable
	SourceCapsUpdated = false;	// Set the flag to indicate to the GUI that our source caps have been updated

	CapsHeaderSource.NumDataObjects = 2;	// Set the number of power objects to 2
	CapsHeaderSource.PortDataRole = 0;	// Set the data role to UFP by default
	CapsHeaderSource.PortPowerRole = 1;	// By default, set the device to be a source
	CapsHeaderSource.SpecRevision = 1;	// Set the spec revision to 2.0
	CapsSource[0].FPDOSupply.Voltage = 100;	// Set 5V for the first supply option
	CapsSource[0].FPDOSupply.MaxCurrent = 100;	// Set 1000mA for the first supply option
	CapsSource[0].FPDOSupply.PeakCurrent = 0;	// Set peak equal to max
	CapsSource[0].FPDOSupply.DataRoleSwap = true;	// By default, don't enable DR_SWAP
	CapsSource[0].FPDOSupply.USBCommCapable = false;	// By default, USB communications is not allowed
	CapsSource[0].FPDOSupply.ExternallyPowered = true;	// By default, state that we are externally powered
	CapsSource[0].FPDOSupply.USBSuspendSupport = false;	// By default, don't allow  USB Suspend
	CapsSource[0].FPDOSupply.DualRolePower = true;	// By default, don't enable PR_SWAP
	CapsSource[0].FPDOSupply.SupplyType = 0;	// Fixed supply

	CapsSource[1].FPDOSupply.Voltage = 240;	// Set 12V for the second supply option
	CapsSource[1].FPDOSupply.MaxCurrent = 150;	// Set 1500mA for the second supply option
	CapsSource[1].FPDOSupply.PeakCurrent = 0;	// Set peak equal to max
	CapsSource[1].FPDOSupply.DataRoleSwap = 0;	// Not used... set to zero
	CapsSource[1].FPDOSupply.USBCommCapable = 0;	// Not used... set to zero
	CapsSource[1].FPDOSupply.ExternallyPowered = 0;	// Not used... set to zero
	CapsSource[1].FPDOSupply.USBSuspendSupport = 0;	// Not used... set to zero
	CapsSource[1].FPDOSupply.DualRolePower = 0;	// Not used... set to zero
	CapsSource[1].FPDOSupply.SupplyType = 0;	// Fixed supply

	CapsHeaderSink.NumDataObjects = 2;	// Set the number of power objects to 2
	CapsHeaderSink.PortDataRole = 0;	// Set the data role to UFP by default
	CapsHeaderSink.PortPowerRole = 0;	// By default, set the device to be a sink
	CapsHeaderSink.SpecRevision = 1;	// Set the spec revision to 2.0
	CapsSink[0].FPDOSink.Voltage = 100;	// Set 5V for the first supply option
	CapsSink[0].FPDOSink.OperationalCurrent = 10;	// Set that our device will consume 100mA for this object
	CapsSink[0].FPDOSink.DataRoleSwap = 0;	// By default, don't enable DR_SWAP
	CapsSink[0].FPDOSink.USBCommCapable = 0;	// By default, USB communications is not allowed
	CapsSink[0].FPDOSink.ExternallyPowered = 0;	// By default, we are not externally powered
	CapsSink[0].FPDOSink.HigherCapability = false;	// By default, don't require more than vSafe5V
	CapsSink[0].FPDOSink.DualRolePower = 0;	// By default, don't enable PR_SWAP

	CapsSink[1].FPDOSink.Voltage = 240;	// Set 12V for the second supply option
	CapsSink[1].FPDOSink.OperationalCurrent = 10;	// Set that our device will consume 100mA for this object
	CapsSink[1].FPDOSink.DataRoleSwap = 0;	// Not used
	CapsSink[1].FPDOSink.USBCommCapable = 0;	// Not used
	CapsSink[1].FPDOSink.ExternallyPowered = 0;	// Not used
	CapsSink[1].FPDOSink.HigherCapability = 0;	// Not used
	CapsSink[1].FPDOSink.DualRolePower = 0;	// Not used

//    initialize(&vdmm);                                                          // Initialize VDM Manager memory

	hrtimer_init(&protocol_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	protocol_hrtimer.function = pd_func_hrtimer;

	hrtimer_init(&policystate_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	policystate_hrtimer.function = pd_func_hrtimer;

	hrtimer_init(&noresponse_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	noresponse_hrtimer.function = pd_func_hrtimer;

	hrtimer_init(&prswap_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	prswap_hrtimer.function = pd_func_hrtimer;
}

// ##################### USB PD Enable / Disable Routines ################### //

void USBPDEnable(bool FUSB300Update, bool TypeCDFP)
{
	u8 data[5];
	if (USBPDEnabled == true) {
		if (blnCCPinIsCC1)	// If the CC pin is on CC1
			Registers.Switches.TXCC1 = true;	// Enable the BMC transmitter on CC1
		else if (blnCCPinIsCC2)	// If the CC pin is on CC2
			Registers.Switches.TXCC2 = true;	// Enable the BMC transmitter on CC2
		if (blnCCPinIsCC1 || blnCCPinIsCC2)	// If we know what pin the CC signal is...
		{
			USBPDActive = true;	// Set the active flag
			ResetProtocolLayer(false);	// Reset the protocol layer by default
			NoResponseTimer = USHRT_MAX;	// Disable the no response timer by default
			PolicyIsSource = TypeCDFP;	// Set whether we should be initially a source or sink
			PolicyIsDFP = TypeCDFP;	// Set the initial data port direction
			if (PolicyIsSource)	// If we are a source...
			{
				set_policy_state(peSourceStartup);	// initialize the policy engine state to source startup
				Registers.Switches.POWERROLE = 1;	// Initialize to a SRC
				Registers.Switches.DATAROLE = 1;	// Initialize to a DFP
			} else	// Otherwise we are a sink...
			{
				set_policy_state(peSinkStartup);	// initialize the policy engine state to sink startup
				Registers.Switches.POWERROLE = 0;	// Initialize to a SNK
				Registers.Switches.DATAROLE = 0;	// Initialize to a UFP
			}
			Registers.Switches.AUTO_CRC = 1;
			Registers.Power.PWR |= 0x08;	// Enable the internal oscillator for USB PD
			Registers.Control.AUTO_PRE = 0;	// Disable AUTO_PRE since we are going to use AUTO_CRC
			Registers.Control.N_RETRIES = 2;	// Set the number of retries to 0 (handle all retries in firmware for now)
			Registers.Control.AUTO_RETRY = 1;	// Enable AUTO_RETRY to use the I_TXSENT interrupt
			Registers.Slice.SDAC = SDAC_DEFAULT;	// Set the SDAC threshold ~0.544V
			data[0] = Registers.Slice.byte;	// Set the slice byte (building one transaction)
			data[1] = Registers.Control.byte[0] | 0x40;	// Set the Control0 byte and set the TX_FLUSH bit (auto-clears)
			data[2] = Registers.Control.byte[1] | 0x04;	// Set the Control1 byte and set the RX_FLUSH bit (auto-clears)
			data[3] = Registers.Control.byte[2];
			data[4] = Registers.Control.byte[3];
			FUSB300Write(regSlice, 5, &data[0]);	// Commit the slice and control registers
			if (FUSB300Update) {
				FUSB300Write(regPower, 1, &Registers.Power.byte);	// Commit the power setting
				FUSB300Write(regSwitches1, 1, &Registers.Switches.byte[1]);	// Commit the switch1 setting
			}
			StoreUSBPDToken(true, pdtAttach);	// Store the PD attach token
			//           InitializeUSBPDTimers(true);                                        // Enable the USB PD timers
		}
	}
}

void USBPDDisable(bool FUSB300Update)
{
	if (USBPDActive == true)	// If we were previously active...
		StoreUSBPDToken(true, pdtDetach);	// Store the PD detach token
	USBPDActive = false;	// Clear the USB PD active flag
	set_protocol_state(PRLDisabled);	// Set the protocol layer state to disabled
	set_policy_state(peDisabled);	// Set the policy engine state to disabled
	set_pdtx_state(txIdle);	// Reset the transmitter status
	PolicyIsSource = false;	// Clear the is source flag until we connect again
	PolicyHasContract = false;	// Clear the has contract flag
	SourceCapsUpdated = true;	// Set the source caps updated flag to trigger an update of the GUI
	if (FUSB300Update) {
		Registers.Switches.byte[1] = 0;	// Disable the BMC transmitter (both CC1 & CC2)
		Registers.Power.PWR &= 0x07;	// Disable the internal oscillator
		FUSB300Write(regPower, 1, &Registers.Power.byte);	// Commit the power setting
		FUSB300Write(regSwitches1, 1, &Registers.Switches.byte[1]);	// Commit the switch setting
	}
}

// ##################### USB PD Policy Engine Routines ###################### //

void USBPDPolicyEngine(void)
{
	//FUSB_LOG("PolicyState=%d\n", PolicyState);
	switch (PolicyState) {
	case peErrorRecovery:
		PolicyErrorRecovery();
		break;
		// ###################### Source States  ##################### //
	case peSourceSendHardReset:
		PolicySourceSendHardReset();
		break;
	case peSourceSendSoftReset:
		PolicySourceSendSoftReset();
		break;
	case peSourceSoftReset:
		PolicySourceSoftReset();
		break;
	case peSourceStartup:
		PolicySourceStartup();
		break;
	case peSourceDiscovery:
		PolicySourceDiscovery();
		break;
	case peSourceSendCaps:
		PolicySourceSendCaps();
		break;
	case peSourceDisabled:
		PolicySourceDisabled();
		break;
	case peSourceTransitionDefault:
		PolicySourceTransitionDefault();
		break;
	case peSourceNegotiateCap:
		PolicySourceNegotiateCap();
		break;
	case peSourceCapabilityResponse:
		PolicySourceCapabilityResponse();
		break;
	case peSourceTransitionSupply:
		PolicySourceTransitionSupply();
		break;
	case peSourceReady:
		PolicySourceReady();
		break;
	case peSourceGiveSourceCaps:
		PolicySourceGiveSourceCap();
		break;
	case peSourceGetSinkCaps:
		PolicySourceGetSinkCap();
		break;
	case peSourceSendPing:
		PolicySourceSendPing();
		break;
	case peSourceGotoMin:
		PolicySourceGotoMin();
		break;
	case peSourceGiveSinkCaps:
		PolicySourceGiveSinkCap();
		break;
	case peSourceGetSourceCaps:
		PolicySourceGetSourceCap();
		break;
	case peSourceSendDRSwap:
		PolicySourceSendDRSwap();
		break;
	case peSourceEvaluateDRSwap:
		PolicySourceEvaluateDRSwap();
		break;
	case peSourceSendVCONNSwap:
		PolicySourceSendVCONNSwap();
		break;
	case peSourceSendPRSwap:
		PolicySourceSendPRSwap();
		break;
	case peSourceEvaluatePRSwap:
		PolicySourceEvaluatePRSwap();
		break;
		// ###################### Sink States  ####################### //
	case peSinkStartup:
		PolicySinkStartup();
		break;
	case peSinkSendHardReset:
		PolicySinkSendHardReset();
		break;
	case peSinkSoftReset:
		PolicySinkSoftReset();
		break;
	case peSinkSendSoftReset:
		PolicySinkSendSoftReset();
		break;
	case peSinkTransitionDefault:
		PolicySinkTransitionDefault();
		break;
	case peSinkDiscovery:
		PolicySinkDiscovery();
		break;
	case peSinkWaitCaps:
		PolicySinkWaitCaps();
		break;
	case peSinkEvaluateCaps:
		PolicySinkEvaluateCaps();
		break;
	case peSinkSelectCapability:
		PolicySinkSelectCapability();
		break;
	case peSinkTransitionSink:
		PolicySinkTransitionSink();
		break;
	case peSinkReady:
		PolicySinkReady();
		break;
	case peSinkGiveSinkCap:
		PolicySinkGiveSinkCap();
		break;
	case peSinkGetSourceCap:
		PolicySinkGetSourceCap();
		break;
	case peSinkGetSinkCap:
		PolicySinkGetSinkCap();
		break;
	case peSinkGiveSourceCap:
		PolicySinkGiveSourceCap();
		break;
	case peSinkSendDRSwap:
		PolicySinkSendDRSwap();
		break;
	case peSinkEvaluateDRSwap:
		PolicySinkEvaluateDRSwap();
		break;
	case peSinkEvaluateVCONNSwap:
		PolicySinkEvaluateVCONNSwap();
		break;
	case peSinkSendPRSwap:
		PolicySinkSendPRSwap();
		break;
	case peSinkEvaluatePRSwap:
		PolicySinkEvaluatePRSwap();
		break;
	default:
		break;
	}
	USBPDTxFlag = false;	// Clear the transmit flag after going through the loop to avoid sending a message inadvertantly
}

// ############################# Source States  ############################# //

void PolicyErrorRecovery(void)
{
	SetStateErrorRecovery();
}

void PolicySourceSendHardReset(void)
{
	PolicySendHardReset(peSourceTransitionDefault, tPSHardReset);
}

void PolicySourceSoftReset(void)
{
	PolicySendCommand(CMTAccept, peSourceSendCaps, 0);
}

void PolicySourceSendSoftReset(void)
{
	switch (PolicySubIndex) {
	case 0:
		if (PolicySendCommand(CMTSoftReset, peSourceSendSoftReset, 1) == STAT_SUCCESS)	// Send the soft reset command to the protocol layer
		{
			PolicyStateTimer = tSenderResponse;	// Start the sender response timer to wait for an accept message once successfully sent
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
		}
		break;
	default:
		if (ProtocolMsgRx)	// If we have received a message
		{
			ProtocolMsgRx = false;	// Reset the message received flag since we've handled it here
			if ((PolicyRxHeader.NumDataObjects == 0) && (PolicyRxHeader.MessageType == CMTAccept))	// And it was the Accept...
			{
				set_policy_state(peSourceSendCaps);	// Go to the send caps state
			} else	// Otherwise it was a message that we didn't expect, so...
				set_policy_state(peSourceSendHardReset);	// Go to the hard reset state
			set_policy_subindex(0);	// Reset the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		} else if (!PolicyStateTimer)	// If we didn't get a response to our request before timing out...
		{
			set_policy_state(peSourceSendHardReset);	// Go to the hard reset state
			set_policy_subindex(0);	// Reset the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
		break;
	}
}

void PolicySourceStartup(void)
{
	PolicyIsSource = true;	// Set the flag to indicate that we are a source (PRSwaps)
	ResetProtocolLayer(true);	// Reset the protocol layer
	PRSwapTimer = 0;	// Clear the swap timer
	CapsCounter = 0;	// Clear the caps counter
	CollisionCounter = 0;	// Reset the collision counter
	PolicyStateTimer = 0;	// Reset the policy state timer
	set_policy_state(peSourceSendCaps);	// Go to the source caps
	set_policy_subindex(0);	// Reset the sub index
}

void PolicySourceDiscovery(void)
{
	switch (PolicySubIndex) {
	case 0:
		PolicyStateTimer = tTypeCSendSourceCap;	// Initialize the SourceCapabilityTimer
		usbpd_start_timer(&policystate_hrtimer, PolicyStateTimer);
		increase_policy_subindex();	// Increment the sub index
		break;
	default:
		if ((HardResetCounter > nHardResetCount)
		    && (NoResponseTimer == 0)) {
			if (PolicyHasContract)	// If we previously had a contract in place...
				set_policy_state(peErrorRecovery);	// Go to the error recovery state since something went wrong
			else	// Otherwise...
				set_policy_state(peSourceDisabled);	// Go to the disabled state since we are assuming that there is no PD sink attached
			set_policy_subindex(0);	// Reset the sub index for the next state
		} else if (PolicyStateTimer == 0)	// Once the timer expires...
		{
			if (CapsCounter > nCapsCount)	// If we have sent the maximum number of capabilities messages...
				set_policy_state(peSourceDisabled);	// Go to the disabled state, no PD sink connected
			else	// Otherwise...
				set_policy_state(peSourceSendCaps);	// Go to the send source caps state to send a source caps message
			set_policy_subindex(0);	// Reset the sub index for the next state
		}
		break;
	}
}

void PolicySourceSendCaps(void)
{
	if ((HardResetCounter > nHardResetCount) && (NoResponseTimer == 0))	// Check our higher level timeout
	{
		if (PolicyHasContract)	// If USB PD was previously established...
			set_policy_state(peErrorRecovery);	// Need to go to the error recovery state
		else		// Otherwise...
			set_policy_state(peSourceDisabled);	// We are disabling PD and leaving the Type-C connections alone
	} else			// If we haven't timed out and maxed out on hard resets...
	{
		switch (PolicySubIndex) {
		case 0:
			if (PolicySendData
			    (DMTSourceCapabilities,
			     CapsHeaderSource.NumDataObjects, &CapsSource[0],
			     peSourceSendCaps, 1) == STAT_SUCCESS) {
				HardResetCounter = 0;	// Clear the hard reset counter
				CapsCounter = 0;	// Clear the caps counter
				NoResponseTimer = USHRT_MAX;	// Stop the no response timer
				PolicyStateTimer = tSenderResponse;	// Set the sender response timer
				usbpd_start_timer(&policystate_hrtimer,
						  PolicyStateTimer);
			}
			break;
		default:
			if (ProtocolMsgRx)	// If we have received a message
			{
				ProtocolMsgRx = false;	// Reset the message ready flag since we're handling it here
				if ((PolicyRxHeader.NumDataObjects == 1) && (PolicyRxHeader.MessageType == DMTRequest))	// Was this a valid request message?
					set_policy_state(peSourceNegotiateCap);	// If so, go to the negotiate capabilities state
				else	// Otherwise it was a message that we didn't expect, so...
					set_policy_state(peSourceSendHardReset);	// Go onto issuing a hard reset
				set_policy_subindex(0);	// Reset the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
			} else if (!PolicyStateTimer)	// If we didn't get a response to our request before timing out...
			{
				ProtocolMsgRx = false;	// Reset the message ready flag since we've timed out
				set_policy_state(peSourceSendHardReset);	// Go to the hard reset state
				set_policy_subindex(0);	// Reset the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
			}
			break;
		}
	}
}

void PolicySourceDisabled(void)
{
	USBPDContract.object = 0;	// Clear the USB PD contract (output power to 5V default)
	// Wait for a hard reset or detach...
}

void PolicySourceTransitionDefault(void)
{
	switch (PolicySubIndex) {
	case 0:
		VBUS_5V_EN = 0;	// Disable the 5V source
		VBUS_12V_EN = 0;	// Disable the 12V source
		USBPDContract.object = 0;	// Clear the USB PD contract (output power to 5V default)
		PolicyStateTimer = tPSSourceOffNom;	// Initialize the tPSTransition timer as a starting point for allowing the voltage to drop to 0
		usbpd_start_timer(&policystate_hrtimer, PolicyStateTimer);
		increase_policy_subindex();
		// Adjust output if necessary and start timer prior to going to startup state?
		break;
	case 1:
		if (PolicyStateTimer == 0)	// Once the timer expires...
		{
			VBUS_5V_EN = 1;	// Enable the 5V source
			PolicyStateTimer = tTypeCSendSourceCap;	// Set the timer to allow for the sink to detect VBUS
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
			increase_policy_subindex();	// Move onto the next state
		}
		break;
	default:
		if (PolicyStateTimer == 0) {
			NoResponseTimer = tNoResponse;	// Initialize the no response timer
			usbpd_start_timer(&noresponse_hrtimer, NoResponseTimer);
			set_policy_state(peSourceStartup);	// Go to the startup state
			set_policy_subindex(0);	// Reset the sub index
		}
		break;
	}
}

void PolicySourceNegotiateCap(void)
{
	// This state evaluates if the sink request can be met or not and sets the next state accordingly
	bool reqAccept = false;	// Set a flag that indicates whether we will accept or reject the request
	u8 objPosition;		// Get the requested object position
	objPosition = PolicyRxDataObj[0].FVRDO.ObjectPosition;	// Get the object position reference
	if ((objPosition > 0) && (objPosition <= CapsHeaderSource.NumDataObjects))	// Make sure the requested object number if valid, continue validating request
	{
		if (PolicyRxDataObj[0].FVRDO.OpCurrent <= CapsSource[objPosition - 1].FPDOSupply.MaxCurrent)	// Ensure the default power/current request is available
			reqAccept = true;	// If the source can supply the request, set the flag to respond
	}
	if (reqAccept)		// If we have received a valid request...
	{
		set_policy_state(peSourceTransitionSupply);	// Go to the transition supply state

	} else			// Otherwise the request was invalid...
		set_policy_state(peSourceCapabilityResponse);	// Go to the capability response state to send a reject/wait message
}

void PolicySourceTransitionSupply(void)
{
	u8 objPosition;
	FUSB_LOG("VBUS_12V_EN=%d, VBUS_5V_EN=%d, PolicySubIndex=%d\n",
		 VBUS_12V_EN, VBUS_5V_EN, PolicySubIndex);
	switch (PolicySubIndex) {
	case 0:
		PolicySendCommand(CMTAccept, peSourceTransitionSupply, 1);	// Send the Accept message
		break;
	case 1:
		PolicyStateTimer = tSnkTransition;	// Initialize the timer to allow for the sink to transition
		usbpd_start_timer(&policystate_hrtimer, PolicyStateTimer);
		increase_policy_subindex();	// Increment to move to the next sub state
		break;
	case 2:
		if (!PolicyStateTimer) {	// If the timer has expired (the sink is ready)...
			increase_policy_subindex();	// Increment to move to the next sub state
		}
		break;
	case 3:
		PolicyHasContract = true;	// Set the flag to indicate that a contract is in place
		USBPDContract.object = PolicyRxDataObj[0].object;	// Set the contract to the sink request
		objPosition = USBPDContract.FVRDO.ObjectPosition;
		if ((CapsSource[objPosition - 1].FPDOSupply.SupplyType == 0)
		    && (CapsSource[objPosition - 1].FPDOSupply.Voltage ==
			240)) {
			if (VBUS_12V_EN)	// If we already have the 12V enabled
			{
				set_policy_subindex(5);	// go directly to sending the PS_RDY
			} else	// Otherwise we need to transition the supply
			{
				VBUS_5V_EN = 1;	// Keep the 5V source enabled so that we don't droop the line
				VBUS_12V_EN = 1;	// Enable the 12V source (takes 4.3ms to enable, 3.0ms to rise)
				PolicyStateTimer = tFPF2498Transition;	// Set the policy state timer to allow the load switch time to turn off so we don't short our supplies
				usbpd_start_timer(&policystate_hrtimer,
						  PolicyStateTimer);
				increase_policy_subindex();	// Move onto the next state to turn on our 12V supply
			}
		} else {
			if (VBUS_12V_EN) {
				VBUS_5V_EN = 1;	// Ensure that the 5V source is enabled
				VBUS_12V_EN = 0;	// disable the 12V source (takes 600us to disable, 2ms for the output to fall)
				PolicyStateTimer = tFPF2498Transition;	// Set the policy state timer to allow the load switch time to turn off so we don't short our supplies
				usbpd_start_timer(&policystate_hrtimer,
						  PolicyStateTimer);
				increase_policy_subindex();	// Move onto the next state to turn on our 12V supply
			} else {
				set_policy_subindex(5);	// go directly to sending the PS_RDY
			}
		}
		break;
	case 4:
		// Validate the output is ready prior to sending the ready message (only using a timer for now, could validate using an ADC as well)
		if (PolicyStateTimer == 0) {
			increase_policy_subindex();	// Increment to move to the next sub state
		}
		break;
	default:
		PolicySendCommand(CMTPS_RDY, peSourceReady, 0);	// Send the PS_RDY message and move onto the Source Ready state
		break;
	}
}

void PolicySourceCapabilityResponse(void)
{
	if (PolicyHasContract)	// If we currently have a contract, issue the reject and move back to the ready state
		PolicySendCommand(CMTReject, peSourceReady, 0);	// Send the message and continue onto the ready state
	else			// If there is no contract in place, issue a hard reset
		PolicySendCommand(CMTReject, peSourceSendHardReset, 0);	// Send the message and continue onto the hard reset state after success
}

void PolicySourceReady(void)
{
	if (ProtocolMsgRx)	// Have we received a message from the sink?
	{
		ProtocolMsgRx = false;	// Reset the message received flag since we're handling it here
		if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
		{
			switch (PolicyRxHeader.MessageType)	// Determine which command was received
			{
			case CMTGetSourceCap:
				set_policy_state(peSourceGiveSourceCaps);	// Send out the caps
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTGetSinkCap:	// If we receive a get sink capabilities message...
				set_policy_state(peSourceGiveSinkCaps);	// Go evaluate whether we are going to send sink caps or reject
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTDR_Swap:	// If we get a DR_Swap message...
				set_policy_state(peSourceEvaluateDRSwap);	// Go evaluate whether we are going to accept or reject the swap
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTPR_Swap:
				set_policy_state(peSourceEvaluatePRSwap);	// Go evaluate whether we are going to accept or reject the swap
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTSoftReset:
				set_policy_state(peSourceSoftReset);	// Go to the soft reset state
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			default:	// Send a reject message for all other commands
				break;
			}
		} else		// If we received a data message... for now just send a soft reset
		{
			switch (PolicyRxHeader.MessageType) {
			case DMTRequest:
				set_policy_state(peSourceNegotiateCap);	// If we've received a request object, go to the negotiate capabilities state
				break;
			case DMTVenderDefined:
				//convertAndProcessVdmMessage();
				break;
			default:	// Otherwise we've received a message we don't know how to handle yet
				break;
			}
			set_policy_subindex(0);	// Clear the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
	} else if (USBPDTxFlag)	// Has the device policy manager requested us to send a message?
	{
		if (PDTransmitHeader.NumDataObjects == 0) {
			switch (PDTransmitHeader.MessageType)	// Determine which command we need to send
			{
			case CMTGetSinkCap:
				set_policy_state(peSourceGetSinkCaps);	// Go to the get sink caps state
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTGetSourceCap:
				set_policy_state(peSourceGetSourceCaps);	// Go to the get source caps state
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTPing:
				set_policy_state(peSourceSendPing);	// Go to the send ping state
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTGotoMin:
				set_policy_state(peSourceGotoMin);	// Go to the source goto min state
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTPR_Swap:
				if (PortType == USBTypeC_DRP)	// Only send if we are configured as a DRP
				{
					set_policy_state(peSourceSendPRSwap);	// Issue a DR_Swap message
					set_policy_subindex(0);	// Clear the sub index
					set_pdtx_state(txIdle);	// Clear the transmitter status
				}
				break;
			case CMTDR_Swap:
				if (PortType == USBTypeC_DRP)	// Only send if we are configured as a DRP
				{
					set_policy_state(peSourceSendDRSwap);	// Issue a DR_Swap message
					set_policy_subindex(0);	// Clear the sub index
					set_pdtx_state(txIdle);	// Clear the transmitter status
				}
				break;
			case CMTVCONN_Swap:
				set_policy_state(peSourceSendVCONNSwap);	// Issue a VCONN_Swap message
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTSoftReset:
				set_policy_state(peSourceSendSoftReset);	// Go to the soft reset state
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			default:	// Don't send any commands we don't know how to handle yet
				break;
			}
		} else {
			switch (PDTransmitHeader.MessageType) {
			case DMTSourceCapabilities:
				set_policy_state(peSourceSendCaps);
				set_policy_subindex(0);
				set_pdtx_state(txIdle);
				break;
			case DMTVenderDefined:
				// not worrying about transitioning states right now - TODO
				set_policy_subindex(0);	// Do I need this here? - Gabe
				doVdmCommand();
				break;
			default:
				break;
			}
		}
	}
}

void PolicySourceGiveSourceCap(void)
{
	PolicySendData(DMTSourceCapabilities, CapsHeaderSource.NumDataObjects,
		       &CapsSource[0], peSourceReady, 0);
}

void PolicySourceGetSourceCap(void)
{
	PolicySendCommand(CMTGetSourceCap, peSourceReady, 0);
}

void PolicySourceGetSinkCap(void)
{
	switch (PolicySubIndex) {
	case 0:
		if (PolicySendCommand(CMTGetSinkCap, peSourceGetSinkCaps, 1) == STAT_SUCCESS)	// Send the get sink caps command upon entering state
		{
			PolicyStateTimer = tSenderResponse;	// Start the sender response timer upon receiving the good CRC message
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
		}
		break;
	default:
		if (ProtocolMsgRx)	// If we have received a message
		{
			ProtocolMsgRx = false;	// Reset the message ready flag since we're handling it here
			if ((PolicyRxHeader.NumDataObjects > 0)
			    && (PolicyRxHeader.MessageType ==
				DMTSinkCapabilities)) {
				UpdateCapabilitiesRx(false);
				set_policy_state(peSourceReady);	// Go onto the source ready state
			} else	// If we didn't receive a valid sink capabilities message...
			{
				set_policy_state(peSourceSendHardReset);	// Go onto issuing a hard reset
			}
			set_policy_subindex(0);	// Reset the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		} else if (!PolicyStateTimer)	// If we didn't get a response to our request before timing out...
		{
			set_policy_state(peSourceSendHardReset);	// Go to the hard reset state
			set_policy_subindex(0);	// Reset the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
		break;
	}
}

void PolicySourceGiveSinkCap(void)
{
	if (PortType == USBTypeC_DRP)
		PolicySendData(DMTSinkCapabilities,
			       CapsHeaderSink.NumDataObjects, &CapsSink[0],
			       peSourceReady, 0);
	else
		PolicySendCommand(CMTReject, peSourceReady, 0);	// Send the reject message and continue onto the ready state
}

void PolicySourceSendPing(void)
{
	PolicySendCommand(CMTPing, peSourceReady, 0);
}

void PolicySourceGotoMin(void)
{
	if (ProtocolMsgRx) {
		ProtocolMsgRx = false;	// Reset the message ready flag since we're handling it here
		if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a control message...
		{
			switch (PolicyRxHeader.MessageType)	// Determine the message type
			{
			case CMTSoftReset:
				set_policy_state(peSourceSoftReset);	// Go to the soft reset state if we received a reset command
				set_policy_subindex(0);	// Reset the sub index
				set_pdtx_state(txIdle);	// Reset the transmitter status
				break;
			default:	// If we receive any other command (including Reject & Wait), just go back to the ready state without changing
				break;
			}
		}
	} else {
		switch (PolicySubIndex) {
		case 0:
			PolicySendCommand(CMTGotoMin, peSourceGotoMin, 1);	// Send the GotoMin message
			break;
		case 1:
			PolicyStateTimer = tSnkTransition;	// Initialize the timer to allow for the sink to transition
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
			increase_policy_subindex();	// Increment to move to the next sub state
			break;
		case 2:
			if (!PolicyStateTimer)	// If the timer has expired (the sink is ready)...
				increase_policy_subindex();	// Increment to move to the next sub state
			break;
		case 3:
			// Adjust the power supply if necessary...
			increase_policy_subindex();	// Increment to move to the next sub state
			break;
		case 4:
			// Validate the output is ready prior to sending the ready message
			increase_policy_subindex();	// Increment to move to the next sub state
			break;
		default:
			PolicySendCommand(CMTPS_RDY, peSourceReady, 0);	// Send the PS_RDY message and move onto the Source Ready state
			break;
		}
	}
}

void PolicySourceSendDRSwap(void)
{
	u8 Status;
	switch (PolicySubIndex) {
	case 0:
		Status = PolicySendCommandNoReset(CMTDR_Swap, peSourceSendDRSwap, 1);	// Send the DR_Swap message
		if (Status == STAT_SUCCESS)	// If we received the good CRC message...
		{
			PolicyStateTimer = tSenderResponse;	// Initialize for SenderResponseTimer
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
		} else if (Status == STAT_ERROR)	// If there was an error...
			set_policy_state(peErrorRecovery);	// Go directly to the error recovery state
		break;
	default:
		if (ProtocolMsgRx) {
			ProtocolMsgRx = false;	// Reset the message ready flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a control message...
			{
				switch (PolicyRxHeader.MessageType)	// Determine the message type
				{
				case CMTAccept:
					PolicyIsDFP = !PolicyIsDFP;	// Flip the current data role
					Registers.Switches.DATAROLE = PolicyIsDFP;	// Update the data role
					FUSB300Write(regSwitches1, 1, &Registers.Switches.byte[1]);	// Commit the data role in the 302 for the auto CRC
					set_policy_state(peSourceReady);	// Source ready state
					break;
				case CMTSoftReset:
					set_policy_state(peSourceSoftReset);	// Go to the soft reset state if we received a reset command
					break;
				default:	// If we receive any other command (including Reject & Wait), just go back to the ready state without changing
					set_policy_state(peSourceReady);	// Go to the source ready state
					break;
				}
			} else	// Otherwise we received a data message...
			{
				set_policy_state(peSourceReady);	// Go to the sink ready state if we received a unexpected data message (ignoring message)
			}
			set_policy_subindex(0);	// Reset the sub index
			set_pdtx_state(txIdle);	// Reset the transmitter status
		} else if (PolicyStateTimer == 0)	// If the sender response timer times out...
		{
			set_policy_state(peSourceReady);	// Go to the source ready state if the SenderResponseTimer times out
			set_policy_subindex(0);	// Reset the sub index
			set_pdtx_state(txIdle);	// Reset the transmitter status
		}
		break;
	}
}

void PolicySourceEvaluateDRSwap(void)
{
	u8 Status;
	if (PortType != USBTypeC_DRP)	// For ease, just determine Accept/Reject based on PortType
	{
		PolicySendCommand(CMTReject, peSourceReady, 0);	// Send the reject if we are not a DRP
	} else			// If we are a DRP, follow through with the swap
	{
		Status = PolicySendCommandNoReset(CMTAccept, peSourceReady, 0);	// Send the Accept message and wait for the good CRC
		if (Status == STAT_SUCCESS)	// If we received the good CRC...
		{
			PolicyIsDFP = !PolicyIsDFP;	// We're not really doing anything except flipping the bit
			Registers.Switches.DATAROLE = PolicyIsDFP;	// Update the data role
			FUSB300Write(regSwitches1, 1, &Registers.Switches.byte[1]);	// Commit the data role in the 302 for the auto CRC
		} else if (Status == STAT_ERROR)	// If we didn't receive the good CRC...
		{
			set_policy_state(peErrorRecovery);	// Go to the error recovery state
			set_policy_subindex(0);	// Clear the sub-index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
	}
}

void PolicySourceSendVCONNSwap(void)
{
	switch (PolicySubIndex) {
	case 0:
		if (PolicySendCommand(CMTVCONN_Swap, peSourceSendVCONNSwap, 1) == STAT_SUCCESS)	// Send the VCONN_Swap message and wait for the good CRC
		{
			PolicyStateTimer = tSenderResponse;	// Once we receive the good CRC, set the sender response timer
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
		}
		break;
	case 1:
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = false;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTAccept:	// If we get the Accept message...
					increase_policy_subindex();	// Increment the subindex to move onto the next step
					break;
				case CMTWait:	// If we get either the reject or wait message...
				case CMTReject:
					set_policy_state(peSourceReady);	// Go back to the source ready state
					set_policy_subindex(0);	// Clear the sub index
					set_pdtx_state(txIdle);	// Clear the transmitter status
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the SenderResponseTimer times out...
		{
			set_policy_state(peSourceReady);	// Go back to the source ready state
			set_policy_subindex(0);	// Clear the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
		break;
	case 2:
		if (Registers.Switches.VCONN_CC1 || Registers.Switches.VCONN_CC2)	// If we are currently sourcing VCONN...
		{
			PolicyStateTimer = tVCONNSourceOn;	// Enable the VCONNOnTimer and wait for a PS_RDY message
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
			increase_policy_subindex();	// Increment the subindex to move to waiting for a PS_RDY message
		} else		// Otherwise we need to start sourcing VCONN
		{
			if (blnCCPinIsCC1)	// If the CC pin is CC1...
				Registers.Switches.VCONN_CC2 = 1;	// Enable VCONN for CC2
			else	// Otherwise the CC pin is CC2
				Registers.Switches.VCONN_CC1 = 1;	// so enable VCONN on CC1
			FUSB300Write(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the register setting to the FUSB302
			PolicyStateTimer = tFPF2498Transition;	// Allow time for the FPF2498 to enable...
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
			set_policy_subindex(4);	// Skip the next state and move onto sending the PS_RDY message after the timer expires            }
		}
		break;
	case 3:
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = false;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTPS_RDY:	// If we get the PS_RDY message...
					Registers.Switches.VCONN_CC1 = 0;	// Disable the VCONN source
					Registers.Switches.VCONN_CC2 = 0;	// Disable the VCONN source
					FUSB300Write(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the register setting to the FUSB302
					set_policy_state(peSourceReady);	// Move onto the Sink Ready state
					set_policy_subindex(0);	// Clear the sub index
					set_pdtx_state(txIdle);	// Clear the transmitter status
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the VCONNOnTimer times out...
		{
			set_policy_state(peSourceSendHardReset);	// Issue a hard reset
			set_policy_subindex(0);	// Clear the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
		break;
	default:
		if (!PolicyStateTimer) {
			PolicySendCommand(CMTPS_RDY, peSourceReady, 0);	// Send the Accept message and wait for the good CRC
		}
		break;
	}
}

void PolicySourceSendPRSwap(void)
{
	u8 Status;
	switch (PolicySubIndex) {
	case 0:		// Send the PRSwap command
		if (PolicySendCommand(CMTPR_Swap, peSourceSendPRSwap, 1) == STAT_SUCCESS)	// Send the PR_Swap message and wait for the good CRC
		{
			PolicyStateTimer = tSenderResponse;	// Once we receive the good CRC, set the sender response timer
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
		}
		break;
	case 1:		// Require Accept message to move on or go back to ready state
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = false;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTAccept:	// If we get the Accept message...
					PRSwapTimer = tPRSwapBailout;	// Initialize the PRSwapTimer to indicate we are in the middle of a swap
					usbpd_start_timer(&prswap_hrtimer,
							  PRSwapTimer);
					PolicyStateTimer = tSnkTransition;	// Start the sink transition timer
					usbpd_start_timer(&policystate_hrtimer,
							  PolicyStateTimer);
					increase_policy_subindex();	// Increment the subindex to move onto the next step
					break;
				case CMTWait:	// If we get either the reject or wait message...
				case CMTReject:
					set_policy_state(peSourceReady);	// Go back to the source ready state
					set_policy_subindex(0);	// Clear the sub index
					set_pdtx_state(txIdle);	// Clear the transmitter status
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the SenderResponseTimer times out...
		{
			set_policy_state(peSourceReady);	// Go back to the source ready state
			set_policy_subindex(0);	// Clear the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
		break;
	case 2:		// Wait for tSnkTransition and then turn off power (and Rd on/Rp off)
		if (!PolicyStateTimer) {
			RoleSwapToAttachedSink();	// Initiate the Type-C state machine for a power role swap
			PolicyStateTimer = tPSSourceOffNom;	// Allow the output voltage to fall before sending the PS_RDY message
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
			increase_policy_subindex();	// Increment the sub-index to move onto the next state
		}
		break;
	case 3:		// Allow time for the supply to fall and then send the PS_RDY message
		if (!PolicyStateTimer) {
			Status =
			    PolicySendCommandNoReset(CMTPS_RDY,
						     peSourceSendPRSwap, 4);
			if (Status == STAT_SUCCESS)	// If we successfully sent the PS_RDY command and received the goodCRC
			{
				PolicyStateTimer = tPSSourceOnMax;	// Start the PSSourceOn timer to allow time for the new supply to come up
				usbpd_start_timer(&policystate_hrtimer,
						  PolicyStateTimer);
			} else if (Status == STAT_ERROR)
				set_policy_state(peErrorRecovery);	// If we get an error, go to the error recovery state
		}
		break;
	default:		// Wait to receive a PS_RDY message from the new DFP
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = false;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTPS_RDY:	// If we get the PS_RDY message...
					set_policy_state(peSinkStartup);	// Go to the sink startup state
					set_policy_subindex(0);	// Clear the sub index
					PolicyStateTimer = 0;	// Clear the policy state timer
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the PSSourceOn times out...
		{
			set_policy_state(peErrorRecovery);	// Go to the error recovery state
			set_policy_subindex(0);	// Clear the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
		break;
	}
}

void PolicySourceEvaluatePRSwap(void)
{
	u8 Status;
	switch (PolicySubIndex) {
	case 0:		// Send either the Accept or Reject command
		if (PortType != USBTypeC_DRP)	// For ease, just determine Accept/Reject based on PortType
		{
			PolicySendCommand(CMTReject, peSourceReady, 0);	// Send the reject if we are not a DRP
		} else {
			if (PolicySendCommand(CMTAccept, peSourceEvaluatePRSwap, 1) == STAT_SUCCESS)	// Send the Accept message and wait for the good CRC
			{
				PRSwapTimer = tPRSwapBailout;	// Initialize the PRSwapTimer to indicate we are in the middle of a swap
				usbpd_start_timer(&prswap_hrtimer, PRSwapTimer);
				RoleSwapToAttachedSink();	// If we have received the good CRC... initiate the Type-C state machine for a power role swap
				PolicyStateTimer = tPSSourceOffNom;	// Allow the output voltage to fall before sending the PS_RDY message
				usbpd_start_timer(&policystate_hrtimer,
						  PolicyStateTimer);
			}
		}
		break;
	case 1:		// Allow time for the supply to fall and then send the PS_RDY message
		if (!PolicyStateTimer) {
			Status = PolicySendCommandNoReset(CMTPS_RDY, peSourceEvaluatePRSwap, 2);	// Send the PS_RDY message
			if (Status == STAT_SUCCESS)	// If we successfully sent the PS_RDY command and received the goodCRC
			{
				PolicyStateTimer = tPSSourceOnMax;	// Start the PSSourceOn timer to allow time for the new supply to come up
				usbpd_start_timer(&policystate_hrtimer,
						  PolicyStateTimer);
			} else if (Status == STAT_ERROR)
				set_policy_state(peErrorRecovery);	// If we get an error, go to the error recovery state
		}
		break;
	default:		// Wait to receive a PS_RDY message from the new DFP
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = false;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTPS_RDY:	// If we get the PS_RDY message...
					set_policy_state(peSinkStartup);	// Go to the sink startup state
					set_policy_subindex(0);	// Clear the sub index
					PolicyStateTimer = 0;	// Clear the policy state timer
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the PSSourceOn times out...
		{
			set_policy_state(peErrorRecovery);	// Go to the error recovery state
			set_policy_subindex(0);	// Clear the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
		break;
	}
}

// ############################## Sink States  ############################## //

void PolicySinkSendHardReset(void)
{
	PolicySendHardReset(peSinkTransitionDefault, 0);
}

void PolicySinkSoftReset(void)
{
	if (PolicySendCommand(CMTAccept, peSinkWaitCaps, 0) == STAT_SUCCESS) {
		PolicyStateTimer = tSinkWaitCap;
		usbpd_start_timer(&policystate_hrtimer, PolicyStateTimer);
	}
}

void PolicySinkSendSoftReset(void)
{
	switch (PolicySubIndex) {
	case 0:
		if (PolicySendCommand(CMTSoftReset, peSinkSendSoftReset, 1) == STAT_SUCCESS)	// Send the soft reset command to the protocol layer
		{
			PolicyStateTimer = tSenderResponse;	// Start the sender response timer to wait for an accept message once successfully sent
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
		}
		break;
	default:
		if (ProtocolMsgRx)	// If we have received a message
		{
			ProtocolMsgRx = false;	// Reset the message received flag since we've handled it here
			if ((PolicyRxHeader.NumDataObjects == 0) && (PolicyRxHeader.MessageType == CMTAccept))	// And it was the Accept...
			{
				set_policy_state(peSinkWaitCaps);	// Go to the wait for capabilities state
				PolicyStateTimer = tSinkWaitCap;	// Set the state timer to tSinkWaitCap
				usbpd_start_timer(&policystate_hrtimer,
						  PolicyStateTimer);
			} else	// Otherwise it was a message that we didn't expect, so...
				set_policy_state(peSinkSendHardReset);	// Go to the hard reset state
			set_policy_subindex(0);	// Reset the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		} else if (!PolicyStateTimer)	// If we didn't get a response to our request before timing out...
		{
			set_policy_state(peSinkSendHardReset);	// Go to the hard reset state
			set_policy_subindex(0);	// Reset the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
		break;
	}
}

void PolicySinkTransitionDefault(void)
{
	USBPDContract.object = 0;	// Clear the USB PD contract (output power to 5V default)
	NoResponseTimer = tNoResponse;	// Initialize the no response timer
	usbpd_start_timer(&noresponse_hrtimer, NoResponseTimer);
	PRSwapTimer = tNoResponse;	// Set the swap timer to not detach upon VBUS going away
	usbpd_start_timer(&prswap_hrtimer, PRSwapTimer);
	// Adjust sink power if necessary and start timer prior to going to startup state?
	set_policy_state(peSinkStartup);	// Go to the startup state
	set_policy_subindex(0);	// Clear the sub index
	set_pdtx_state(txIdle);	// Reset the transmitter status
	ResetProtocolLayer(true);
}

void PolicySinkStartup(void)
{
	PolicyIsSource = false;	// Clear the flag to indicate that we are a sink (for PRSwaps)
	ResetProtocolLayer(true);	// Reset the protocol layer
	CapsCounter = 0;	// Clear the caps counter
	CollisionCounter = 0;	// Reset the collision counter
	PolicyStateTimer = 0;	// Reset the policy state timer
	set_policy_state(peSinkDiscovery);	// Go to the sink discovery state
	set_policy_subindex(0);	// Reset the sub index
}

void PolicySinkDiscovery(void)
{
	if (Registers.Status.VBUSOK) {
		PRSwapTimer = 0;	// Clear the swap timer
		set_policy_state(peSinkWaitCaps);
		set_policy_subindex(0);
		PolicyStateTimer = tSinkWaitCap;
		//usbpd_start_timer(&policystate_hrtimer, PolicyStateTimer);
	} else if (NoResponseTimer == 0) {
		PRSwapTimer = 0;	// Clear the swap timer
		set_policy_state(peErrorRecovery);
		set_policy_subindex(0);
	}
}

void PolicySinkWaitCaps(void)
{
	if (ProtocolMsgRx)	// If we have received a message...
	{
		ProtocolMsgRx = false;	// Reset the message ready flag since we're handling it here
		if ((PolicyRxHeader.NumDataObjects > 0) && (PolicyRxHeader.MessageType == DMTSourceCapabilities))	// Have we received a valid source cap message?
		{
			//FUSB_LOG("policySinkWaitCaps: ----------------------\n");
			UpdateCapabilitiesRx(true);	// Update the received capabilities
			set_policy_state(peSinkEvaluateCaps);	// Set the evaluate source capabilities state
		} else if ((PolicyRxHeader.NumDataObjects == 0)
			   && (PolicyRxHeader.MessageType == CMTSoftReset)) {
			set_policy_state(peSinkSoftReset);	// Go to the soft reset state
		}
		set_policy_subindex(0);	// Reset the sub index
	} else if ((NoResponseTimer == 0)
		   && (HardResetCounter > nHardResetCount)) {
		set_policy_state(peErrorRecovery);
		set_policy_subindex(0);
	} else if (PolicyStateTimer == 0) {
		set_policy_state(peSinkSendHardReset);
		set_policy_subindex(0);
	}
}

void PolicySinkEvaluateCaps(void)
{
	// Due to latency with the PC and evaluating capabilities, we are always going to select the first one by default (5V default)
	// This will allow the software time to determine if they want to select one of the other capabilities (user selectable)
	// If we want to automatically show the selection of a different capabilities message, we need to build in the functionality here
	// The evaluate caps
	int i, reqPos;
	unsigned int objVoltage, objCurrent, objPower, MaxPower, SelVoltage,
	    ReqCurrent;
	NoResponseTimer = USHRT_MAX;	// Stop the no response timer
	HardResetCounter = 0;	// Reset the hard reset counter
	SelVoltage = 0;
	MaxPower = 0;
	reqPos = 0;		// Select nothing in case there is an error...
	for (i = 0; i < CapsHeaderReceived.NumDataObjects; i++)	// Going to select the highest power object that we are compatible with
	{
		switch (CapsReceived[i].PDO.SupplyType) {
		case pdoTypeFixed:
			objVoltage = CapsReceived[i].FPDOSupply.Voltage;	// Get the output voltage of the fixed supply
			if (objVoltage > SinkRequestMaxVoltage)	// If the voltage is greater than our limit...
				objPower = 0;	// Set the power to zero to ignore the object
			else	// Otherwise...
			{
				objCurrent =
				    CapsReceived[i].FPDOSupply.MaxCurrent;
				objPower = objVoltage * objCurrent;	// Calculate the power for comparison
			}
			//FUSB_LOG("objVoltage=%d\n, objCurrent=%d, objPower=%d, i=%d\n",
			//    objVoltage, objCurrent, objPower, i);
			break;
		case pdoTypeVariable:
			objVoltage = CapsReceived[i].VPDO.MaxVoltage;	// Grab the maximum voltage of the variable supply
			if (objVoltage > SinkRequestMaxVoltage)	// If the max voltage is greater than our limit...
				objPower = 0;	// Set the power to zero to ignore the object
			else	// Otherwise...
			{
				objVoltage = CapsReceived[i].VPDO.MinVoltage;	// Get the minimum output voltage of the variable supply
				objCurrent = CapsReceived[i].VPDO.MaxCurrent;	// Get the maximum output current of the variable supply
				objPower = objVoltage * objCurrent;	// Calculate the power for comparison (based on min V/max I)
			}
			break;
		case pdoTypeBattery:	// We are going to ignore battery powered sources for now
		default:	// We are also ignoring undefined supply types
			objPower = 0;	// Set the object power to zero so we ignore for now
			break;
		}
		if (objPower > MaxPower)	// If the current object has power greater than the previous objects
		{
			MaxPower = objPower;	// Store the objects power
			SelVoltage = objVoltage;	// Store the objects voltage (used for calculations)
			reqPos = i + 1;	// Store the position of the object
		}
	}
	FUSB_LOG("reqPos=%d, selVoltage=%d\n", reqPos, SelVoltage);
	if ((reqPos > 0) && (SelVoltage > 0)) {
		SinkRequest.FVRDO.ObjectPosition = reqPos & 0x07;	// Set the object position selected
		SinkRequest.FVRDO.GiveBack = SinkGotoMinCompatible;	// Set whether we will respond to the GotoMin message
		SinkRequest.FVRDO.NoUSBSuspend = SinkUSBSuspendOperation;	// Set whether we want to continue pulling power during USB Suspend
		SinkRequest.FVRDO.USBCommCapable = SinkUSBCommCapable;	// Set whether USB communications is active
		ReqCurrent = SinkRequestOpPower / SelVoltage;	// Calculate the requested operating current
		SinkRequest.FVRDO.OpCurrent = (ReqCurrent & 0x3FF);	// Set the current based on the selected voltage (in 10mA units)
		ReqCurrent = SinkRequestMaxPower / SelVoltage;	// Calculate the requested maximum current
		SinkRequest.FVRDO.MinMaxCurrent = (ReqCurrent & 0x3FF);	// Set the min/max current based on the selected voltage (in 10mA units)
		if (SinkGotoMinCompatible)	// If the give back flag is set...
			SinkRequest.FVRDO.CapabilityMismatch = false;	// There can't be a capabilities mismatch
		else		// Otherwise...
		{
			if (MaxPower < SinkRequestMaxPower)	// If the max power available is less than the max power requested...
				SinkRequest.FVRDO.CapabilityMismatch = true;	// flag the source that we need more power
			else	// Otherwise...
				SinkRequest.FVRDO.CapabilityMismatch = false;	// there is no mismatch in the capabilities
		}
		set_policy_state(peSinkSelectCapability);	// Go to the select capability state
		set_policy_subindex(0);	// Reset the sub index
		PolicyStateTimer = tSenderResponse;	// Initialize the sender response timer
		usbpd_start_timer(&policystate_hrtimer, PolicyStateTimer);
	} else {
		// For now, we are just going to go back to the wait state instead of sending a reject or reset (may change in future)
		set_policy_state(peSinkWaitCaps);	// Go to the wait for capabilities state
		PolicyStateTimer = tSinkWaitCap;	// Set the state timer to tSinkWaitCap
		usbpd_start_timer(&policystate_hrtimer, PolicyStateTimer);
	}
}

void PolicySinkSelectCapability(void)
{
	unsigned char oldPolicySubIndex;
	do {
		oldPolicySubIndex = PolicySubIndex;
		switch (PolicySubIndex) {
		case 0:
			if (PolicySendData
			    (DMTRequest, 1, &SinkRequest,
			     peSinkSelectCapability, 1) == STAT_SUCCESS) {
				PolicyStateTimer = tSenderResponse;
				usbpd_start_timer(&policystate_hrtimer,
						  PolicyStateTimer);
			}
			break;
		default:
			if (ProtocolMsgRx) {
				ProtocolMsgRx = false;	// Reset the message ready flag since we're handling it here
				if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a control message...
				{
					switch (PolicyRxHeader.MessageType)	// Determine the message type
					{
					case CMTAccept:
						PolicyHasContract = true;	// Set the flag to indicate that a contract is in place
						USBPDContract.object = SinkRequest.object;	// Set the actual contract that is in place
						PolicyStateTimer = tPSTransition;	// Set the transition timer here
						usbpd_start_timer
						    (&policystate_hrtimer,
						     PolicyStateTimer);
						set_policy_state(peSinkTransitionSink);	// Go to the transition state if the source accepted the request
						break;
					case CMTWait:
					case CMTReject:
						set_policy_state(peSinkReady);	// Go to the sink ready state if the source rejects or has us wait
						break;
					case CMTSoftReset:
						set_policy_state(peSinkSoftReset);	// Go to the soft reset state if we received a reset command
						break;
					default:
						set_policy_state(peSinkSendSoftReset);	// We are going to send a reset message for all other commands received
						break;
					}
				} else	// Otherwise we received a data message...
				{
					switch (PolicyRxHeader.MessageType) {
					case DMTSourceCapabilities:	// If we received a new source capabilities message
						UpdateCapabilitiesRx(true);	// Update the received capabilities
						set_policy_state(peSinkEvaluateCaps);	// Go to the evaluate caps state
						break;
					default:
						set_policy_state(peSinkSendSoftReset);	// Send a soft reset to get back to a known state
						break;
					}
				}
				set_policy_subindex(0);	// Reset the sub index
				set_pdtx_state(txIdle);	// Reset the transmitter status
			} else if (PolicyStateTimer == 0)	// If the sender response timer times out...
			{
				set_policy_state(peSinkSendHardReset);	// Go to the hard reset state
				set_policy_subindex(0);	// Reset the sub index
				set_pdtx_state(txIdle);	// Reset the transmitter status
			}
			break;
		}
	} while ((PolicyState == peSinkSelectCapability)
		 && (oldPolicySubIndex < PolicySubIndex));
}

void PolicySinkTransitionSink(void)
{
	if (ProtocolMsgRx) {
		ProtocolMsgRx = false;	// Reset the message ready flag since we're handling it here
		if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a control message...
		{
			switch (PolicyRxHeader.MessageType)	// Determine the message type
			{
			case CMTPS_RDY:
				set_policy_state(peSinkReady);	// Go to the ready state
				break;
			case CMTSoftReset:
				set_policy_state(peSinkSoftReset);	// Go to the soft reset state if we received a reset command
				break;
			default:
//                    set_policy_state(peSinkSendSoftReset);                          // We are going to send a reset message for all other commands received
				break;
			}
		} else		// Otherwise we received a data message...
		{
			switch (PolicyRxHeader.MessageType)	// Determine the message type
			{
			case DMTSourceCapabilities:	// If we received new source capabilities...
				UpdateCapabilitiesRx(true);	// Update the source capabilities
				set_policy_state(peSinkEvaluateCaps);	// And go to the evaluate capabilities state
				break;
			default:	// If we receieved an unexpected data message...
				set_policy_state(peSinkSendSoftReset);	// Send a soft reset to get back to a known state
				break;
			}
		}
		set_policy_subindex(0);	// Reset the sub index
		set_pdtx_state(txIdle);	// Reset the transmitter status
	} else if (PolicyStateTimer == 0)	// If the PSTransitionTimer times out...
	{
		set_policy_state(peSinkSendHardReset);	// Issue a hard reset
		set_policy_subindex(0);	// Reset the sub index
		set_pdtx_state(txIdle);	// Reset the transmitter status
	}
}

void PolicySinkReady(void)
{
	if (ProtocolMsgRx)	// Have we received a message from the source?
	{
		ProtocolMsgRx = false;	// Reset the message received flag since we're handling it here
		if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
		{
			switch (PolicyRxHeader.MessageType)	// Determine which command was received
			{
			case CMTGotoMin:
				set_policy_state(peSinkTransitionSink);	// Go to transitioning the sink power
				PolicyStateTimer = tPSTransition;	// Set the transition timer here
				usbpd_start_timer(&policystate_hrtimer,
						  PolicyStateTimer);
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTGetSinkCap:
				set_policy_state(peSinkGiveSinkCap);	// Go to sending the sink caps state
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTGetSourceCap:
				set_policy_state(peSinkGiveSourceCap);	// Go to sending the source caps if we are dual-role
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTDR_Swap:	// If we get a DR_Swap message...
				set_policy_state(peSinkEvaluateDRSwap);	// Go evaluate whether we are going to accept or reject the swap
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTPR_Swap:
				set_policy_state(peSinkEvaluatePRSwap);	// Go evaluate whether we are going to accept or reject the swap
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTVCONN_Swap:	// If we get a VCONN_Swap message...
				set_policy_state(peSinkEvaluateVCONNSwap);	// Go evaluate whether we are going to accept or reject the swap
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTSoftReset:
				set_policy_state(peSinkSoftReset);	// Go to the soft reset state
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			default:	// For all other commands received, simply ignore them
				break;
			}
		} else {
			switch (PolicyRxHeader.MessageType) {
			case DMTSourceCapabilities:
				UpdateCapabilitiesRx(true);	// Update the received capabilities
				set_policy_state(peSinkEvaluateCaps);	// Go to the evaluate capabilities state
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case DMTVenderDefined:
				//convertAndProcessVdmMessage();
				break;
			default:	// If we get something we are not expecting... simply ignore them
				break;
			}
		}
		set_policy_subindex(0);	// Reset the sub index
		set_pdtx_state(txIdle);	// Reset the transmitter status
	} else if (USBPDTxFlag)	// Has the device policy manager requested us to send a message?
	{
		if (PDTransmitHeader.NumDataObjects == 0) {
			switch (PDTransmitHeader.MessageType) {
			case CMTGetSourceCap:
				set_policy_state(peSinkGetSourceCap);	// Go to retrieve the source caps state
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTGetSinkCap:
				set_policy_state(peSinkGetSinkCap);	// Go to retrieve the sink caps state
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			case CMTDR_Swap:
				if (PortType == USBTypeC_DRP)	// Only send if we are configured as a DRP
				{
					set_policy_state(peSinkSendDRSwap);	// Issue a DR_Swap message
					set_policy_subindex(0);	// Clear the sub index
					set_pdtx_state(txIdle);	// Clear the transmitter status
				}
				break;
			case CMTPR_Swap:
				if (PortType == USBTypeC_DRP)	// Only send if we are configured as a DRP
				{
					set_policy_state(peSinkSendPRSwap);	// Issue a PR_Swap message
					set_policy_subindex(0);	// Clear the sub index
					set_pdtx_state(txIdle);	// Clear the transmitter status
				}
				break;
			case CMTSoftReset:
				set_policy_state(peSinkSendSoftReset);	// Go to the send soft reset state
				set_policy_subindex(0);	// Clear the sub index
				set_pdtx_state(txIdle);	// Clear the transmitter status
				break;
			default:
				break;
			}
		} else {
			switch (PDTransmitHeader.MessageType) {
			case DMTRequest:
				SinkRequest.object = PDTransmitObjects[0].object;	// Set the actual object to request
				set_policy_state(peSinkSelectCapability);	// Go to the select capability state
				set_policy_subindex(0);	// Reset the sub index
				PolicyStateTimer = tSenderResponse;	// Initialize the sender response timer
				usbpd_start_timer(&policystate_hrtimer,
						  PolicyStateTimer);
				break;
			case DMTVenderDefined:
				// not worrying about transitioning states right now - TODO
				set_policy_subindex(0);	// Do I need this here? - Gabe
				doVdmCommand();
				break;
			default:
				break;
			}
		}
	}
}

void PolicySinkGiveSinkCap(void)
{
	PolicySendData(DMTSinkCapabilities, CapsHeaderSink.NumDataObjects,
		       &CapsSink[0], peSinkReady, 0);
}

void PolicySinkGetSinkCap(void)
{
	PolicySendCommand(CMTGetSinkCap, peSinkReady, 0);
}

void PolicySinkGiveSourceCap(void)
{
	if (PortType == USBTypeC_DRP)
		PolicySendData(DMTSourceCapabilities,
			       CapsHeaderSource.NumDataObjects, &CapsSource[0],
			       peSinkReady, 0);
	else
		PolicySendCommand(CMTReject, peSinkReady, 0);	// Send the reject message and continue onto the ready state
}

void PolicySinkGetSourceCap(void)
{
	PolicySendCommand(CMTGetSourceCap, peSinkReady, 0);
}

void PolicySinkSendDRSwap(void)
{
	u8 Status;
	switch (PolicySubIndex) {
	case 0:
		Status = PolicySendCommandNoReset(CMTDR_Swap, peSinkSendDRSwap, 1);	// Send the DR_Swap command
		if (Status == STAT_SUCCESS)	// If we received a good CRC message...
		{
			PolicyStateTimer = tSenderResponse;	// Initialize for SenderResponseTimer if we received the GoodCRC
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
		} else if (Status == STAT_ERROR)	// If there was an error...
			set_policy_state(peErrorRecovery);	// Go directly to the error recovery state
		break;
	default:
		if (ProtocolMsgRx) {
			ProtocolMsgRx = false;	// Reset the message ready flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a control message...
			{
				switch (PolicyRxHeader.MessageType)	// Determine the message type
				{
				case CMTAccept:
					PolicyIsDFP = !PolicyIsDFP;	// Flip the current data role
					Registers.Switches.DATAROLE = PolicyIsDFP;	// Update the data role
					FUSB300Write(regSwitches1, 1, &Registers.Switches.byte[1]);	// Commit the data role in the 302 for the auto CRC
					set_policy_state(peSinkReady);	// Sink ready state
					break;
				case CMTSoftReset:
					set_policy_state(peSinkSoftReset);	// Go to the soft reset state if we received a reset command
					break;
				default:	// If we receive any other command (including Reject & Wait), just go back to the ready state without changing
					set_policy_state(peSinkReady);	// Go to the sink ready state
					break;
				}
			} else	// Otherwise we received a data message...
			{
				set_policy_state(peSinkReady);	// Go to the sink ready state if we received a unexpected data message (ignoring message)
			}
			set_policy_subindex(0);	// Reset the sub index
			set_pdtx_state(txIdle);	// Reset the transmitter status
		} else if (PolicyStateTimer == 0)	// If the sender response timer times out...
		{
			set_policy_state(peSinkReady);	// Go to the sink ready state if the SenderResponseTimer times out
			set_policy_subindex(0);	// Reset the sub index
			set_pdtx_state(txIdle);	// Reset the transmitter status
		}
		break;
	}
}

void PolicySinkEvaluateDRSwap(void)
{
	u8 Status;
	if (PortType != USBTypeC_DRP)	// For ease, just determine Accept/Reject based on PortType
	{
		PolicySendCommand(CMTReject, peSinkReady, 0);	// Send the reject if we are not a DRP
	} else			// If we are a DRP, follow through with the swap
	{
		Status = PolicySendCommandNoReset(CMTAccept, peSinkReady, 0);	// Send the Accept message and wait for the good CRC
		if (Status == STAT_SUCCESS)	// If we received the good CRC...
		{
			PolicyIsDFP = !PolicyIsDFP;	// We're not really doing anything except flipping the bit
			Registers.Switches.DATAROLE = PolicyIsDFP;	// Update the data role
			FUSB300Write(regSwitches1, 1, &Registers.Switches.byte[1]);	// Commit the data role in the 302 for the auto CRC
		} else if (Status == STAT_ERROR)	// If we didn't receive the good CRC...
		{
			set_policy_state(peErrorRecovery);	// Go to the error recovery state
			set_policy_subindex(0);	// Clear the sub-index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
	}
}

void PolicySinkEvaluateVCONNSwap(void)
{
	switch (PolicySubIndex) {
	case 0:
		PolicySendCommand(CMTAccept, peSinkEvaluateVCONNSwap, 1);	// Send the Accept message and wait for the good CRC
		break;
	case 1:
		if (Registers.Switches.VCONN_CC1 || Registers.Switches.VCONN_CC2)	// If we are currently sourcing VCONN...
		{
			PolicyStateTimer = tVCONNSourceOn;	// Enable the VCONNOnTimer and wait for a PS_RDY message
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
			increase_policy_subindex();	// Increment the subindex to move to waiting for a PS_RDY message
		} else		// Otherwise we need to start sourcing VCONN
		{
			if (blnCCPinIsCC1)	// If the CC pin is CC1...
			{
				Registers.Switches.VCONN_CC2 = 1;	// Enable VCONN for CC2
				Registers.Switches.PDWN2 = 0;	// Disable the pull-down on CC2 to avoid sinking unnecessary current
			} else	// Otherwise the CC pin is CC2
			{
				Registers.Switches.VCONN_CC1 = 1;	// Enable VCONN for CC1
				Registers.Switches.PDWN1 = 0;	// Disable the pull-down on CC1 to avoid sinking unnecessary current
			}
			FUSB300Write(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the register setting to the FUSB302
			PolicyStateTimer = tFPF2498Transition;	// Allow time for the FPF2498 to enable...
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
			set_policy_subindex(3);	// Skip the next state and move onto sending the PS_RDY message after the timer expires            }
		}
		break;
	case 2:
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = false;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTPS_RDY:	// If we get the PS_RDY message...
					Registers.Switches.VCONN_CC1 = 0;	// Disable the VCONN source
					Registers.Switches.VCONN_CC2 = 0;	// Disable the VCONN source
					Registers.Switches.PDWN1 = 1;	// Ensure the pull-down on CC1 is enabled
					Registers.Switches.PDWN2 = 1;	// Ensure the pull-down on CC2 is enabled
					FUSB300Write(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the register setting to the FUSB302
					set_policy_state(peSinkReady);	// Move onto the Sink Ready state
					set_policy_subindex(0);	// Clear the sub index
					set_pdtx_state(txIdle);	// Clear the transmitter status
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the VCONNOnTimer times out...
		{
			set_policy_state(peSourceSendHardReset);	// Issue a hard reset
			set_policy_subindex(0);	// Clear the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
		break;
	default:
		if (!PolicyStateTimer) {
			PolicySendCommand(CMTPS_RDY, peSinkReady, 0);	// Send the Accept message and wait for the good CRC
		}
		break;
	}
}

void PolicySinkSendPRSwap(void)
{
	u8 Status;
	switch (PolicySubIndex) {
	case 0:		// Send the PRSwap command
		if (PolicySendCommand(CMTPR_Swap, peSinkSendPRSwap, 1) == STAT_SUCCESS)	// Send the PR_Swap message and wait for the good CRC
		{
			PolicyStateTimer = tSenderResponse;	// Once we receive the good CRC, set the sender response timer
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
		}
		break;
	case 1:		// Require Accept message to move on or go back to ready state
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = false;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTAccept:	// If we get the Accept message...
					PRSwapTimer = tPRSwapBailout;	// Initialize the PRSwapTimer to indicate we are in the middle of a swap
					usbpd_start_timer(&prswap_hrtimer,
							  PRSwapTimer);
					PolicyStateTimer = tPSSourceOffMax;	// Start the sink transition timer
					usbpd_start_timer(&policystate_hrtimer,
							  PolicyStateTimer);
					increase_policy_subindex();	// Increment the subindex to move onto the next step
					break;
				case CMTWait:	// If we get either the reject or wait message...
				case CMTReject:
					set_policy_state(peSinkReady);	// Go back to the sink ready state
					set_policy_subindex(0);	// Clear the sub index
					set_pdtx_state(txIdle);	// Clear the transmitter status
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the SenderResponseTimer times out...
		{
			set_policy_state(peSinkReady);	// Go back to the sink ready state
			set_policy_subindex(0);	// Clear the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
		break;
	case 2:		// Wait for a PS_RDY message to be received to indicate that the original source is no longer supplying VBUS
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = false;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTPS_RDY:	// If we get the PS_RDY message...
					RoleSwapToAttachedSource();	// Initiate the Type-C state machine for a power role swap
					PolicyStateTimer = tPSSourceOnNom;	// Allow the output voltage to rise before sending the PS_RDY message
					usbpd_start_timer(&policystate_hrtimer,
							  PolicyStateTimer);
					increase_policy_subindex();	// Increment the sub-index to move onto the next state
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the PSSourceOn times out...
		{
			set_policy_state(peErrorRecovery);	// Go to the error recovery state
			set_policy_subindex(0);	// Clear the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
		break;
	default:		// Allow time for the supply to rise and then send the PS_RDY message
		if (!PolicyStateTimer) {
			Status = PolicySendCommandNoReset(CMTPS_RDY, peSourceStartup, 0);	// When we get the good CRC, we move onto the source startup state to complete the swap
			if (Status == STAT_ERROR)
				set_policy_state(peErrorRecovery);	// If we get an error, go to the error recovery state
		}
		break;
	}
}

void PolicySinkEvaluatePRSwap(void)
{
	u8 Status;
	switch (PolicySubIndex) {
	case 0:		// Send either the Accept or Reject command
		if (PortType != USBTypeC_DRP)	// For ease, just determine Accept/Reject based on PortType
		{
			PolicySendCommand(CMTReject, peSinkReady, 0);	// Send the reject if we are not a DRP
		} else {
			if (PolicySendCommand(CMTAccept, peSinkEvaluatePRSwap, 1) == STAT_SUCCESS)	// Send the Accept message and wait for the good CRC
			{
				PRSwapTimer = tPRSwapBailout;	// Initialize the PRSwapTimer to indicate we are in the middle of a swap
				usbpd_start_timer(&prswap_hrtimer, PRSwapTimer);
				PolicyStateTimer = tPSSourceOffMax;	// Start the sink transition timer
				usbpd_start_timer(&policystate_hrtimer,
						  PolicyStateTimer);
			}
		}
		break;
	case 1:		// Wait for the PS_RDY command to come in and indicate the source has turned off VBUS
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = false;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTPS_RDY:	// If we get the PS_RDY message...
					RoleSwapToAttachedSource();	// Initiate the Type-C state machine for a power role swap
					PolicyStateTimer = tPSSourceOnNom;	// Allow the output voltage to rise before sending the PS_RDY message
					usbpd_start_timer(&policystate_hrtimer,
							  PolicyStateTimer);
					increase_policy_subindex();	// Increment the sub-index to move onto the next state
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the PSSourceOn times out...
		{
			set_policy_state(peErrorRecovery);	// Go to the error recovery state
			set_policy_subindex(0);	// Clear the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
		break;
	default:		// Wait for VBUS to rise and then send the PS_RDY message
		if (!PolicyStateTimer) {
			Status = PolicySendCommandNoReset(CMTPS_RDY, peSourceStartup, 0);	// When we get the good CRC, we move onto the source startup state to complete the swap
			if (Status == STAT_ERROR)
				set_policy_state(peErrorRecovery);	// If we get an error, go to the error recovery state
		}
		break;
	}
}

// ########################## General PD Messaging ########################## //

bool PolicySendHardReset(PolicyState_t nextState, u32 delay)
{
	bool Success = false;
	switch (PolicySubIndex) {
	case 0:
		switch (PDTxStatus) {
		case txReset:
		case txWait:
			// Do nothing until the protocol layer finishes generating the hard reset signaling
			// The next state should be either txCollision or txSuccess
			break;
		case txSuccess:
			PolicyStateTimer = delay;	// Set the amount of time prior to proceeding to the next state
			usbpd_start_timer(&policystate_hrtimer,
					  PolicyStateTimer);
			increase_policy_subindex();	// Move onto the next state
			Success = true;
			break;
		default:	// None of the other states should actually occur, so...
			set_pdtx_state(txReset);	// Set the transmitter status to resend a hard reset
			break;
		}
		break;
	default:
		if (PolicyStateTimer == 0)	// Once tPSHardReset has elapsed...
		{
			HardResetCounter++;	// Increment the hard reset counter once successfully sent
			set_policy_state(nextState);	// Go to the state to transition to the default sink state
			set_policy_subindex(0);	// Clear the sub index
			set_pdtx_state(txIdle);	// Clear the transmitter status
		}
		break;
	}
	return Success;
}

u8 PolicySendCommand(u8 Command, PolicyState_t nextState, u8 subIndex)
{
	u8 Status = STAT_BUSY;
	if (ProtocolMsgTx == true) {
		set_policy_state(nextState);	// Go to the next state requested
		set_policy_subindex(subIndex);
		set_pdtx_state(txIdle);	// Reset the transmitter status
		Status = STAT_SUCCESS;
		ProtocolMsgTx = false;
	} else {
		switch (PDTxStatus) {
		case txIdle:
			PolicyTxHeader.word = 0;	// Clear the word to initialize for each transaction
			PolicyTxHeader.NumDataObjects = 0;	// Clear the number of objects since this is a command
			PolicyTxHeader.MessageType = Command & 0x0F;	// Sets the message type to the command passed in
			PolicyTxHeader.PortDataRole = PolicyIsDFP;	// Set whether the port is acting as a DFP or UFP
			PolicyTxHeader.PortPowerRole = PolicyIsSource;	// Set whether the port is serving as a power source or sink
			PolicyTxHeader.SpecRevision = USBPDSPECREV;	// Set the spec revision
			set_pdtx_state(txSend);	// Indicate to the Protocol layer that there is something to send
			break;
		case txSend:
		case txBusy:
		case txWait:
			// Waiting for GoodCRC or timeout of the protocol
			// May want to put in a second level timeout in case there's an issue with the protocol getting hung
			break;
		case txSuccess:
			set_policy_state(nextState);	// Go to the next state requested
			set_policy_subindex(subIndex);
			set_pdtx_state(txIdle);	// Reset the transmitter status
			Status = STAT_SUCCESS;
			break;
		case txError:	// Didn't receive a GoodCRC message...
			if (PolicyState == peSourceSendSoftReset)	// If as a source we already failed at sending a soft reset...
				set_policy_state(peSourceSendHardReset);	// Move onto sending a hard reset (source)
			else if (PolicyState == peSinkSendSoftReset)	// If as a sink we already failed at sending a soft reset...
				set_policy_state(peSinkSendHardReset);	// Move onto sending a hard reset (sink)
			else if (PolicyIsSource)	// Otherwise, if we are a source...
				set_policy_state(peSourceSendSoftReset);	// Attempt to send a soft reset (source)
			else	// We are a sink, so...
				set_policy_state(peSinkSendSoftReset);	// Attempt to send a soft reset (sink)
			set_policy_subindex(0);	// Reset the sub index
			set_pdtx_state(txIdle);	// Reset the transmitter status
			Status = STAT_ERROR;
			break;
		case txCollision:
			CollisionCounter++;	// Increment the collision counter
			if (CollisionCounter > nRetryCount)	// If we have already retried two times
			{
				if (PolicyIsSource)
					set_policy_state(peSourceSendHardReset);	// Go to the source hard reset state
				else
					set_policy_state(peSinkSendHardReset);	// Go to the sink hard reset state
				set_policy_subindex(0);	// Reset the sub index
				set_pdtx_state(txReset);	// Set the transmitter status to send a hard reset
				Status = STAT_ERROR;
			} else	// Otherwise we are going to try resending the soft reset
				set_pdtx_state(txIdle);	// Clear the transmitter status for the next operation
			break;
		default:	// For an undefined case, reset everything (shouldn't get here)
			if (PolicyIsSource)
				set_policy_state(peSourceSendHardReset);	// Go to the source hard reset state
			else
				set_policy_state(peSinkSendHardReset);	// Go to the sink hard reset state
			set_policy_subindex(0);	// Reset the sub index
			set_pdtx_state(txReset);	// Set the transmitter status to send a hard reset
			Status = STAT_ERROR;
			break;
		}
	}
	return Status;
}

u8 PolicySendCommandNoReset(u8 Command, PolicyState_t nextState, u8 subIndex)
{
	u8 Status = STAT_BUSY;
	switch (PDTxStatus) {
	case txIdle:
		PolicyTxHeader.word = 0;	// Clear the word to initialize for each transaction
		PolicyTxHeader.NumDataObjects = 0;	// Clear the number of objects since this is a command
		PolicyTxHeader.MessageType = Command & 0x0F;	// Sets the message type to the command passed in
		PolicyTxHeader.PortDataRole = PolicyIsDFP;	// Set whether the port is acting as a DFP or UFP
		PolicyTxHeader.PortPowerRole = PolicyIsSource;	// Set whether the port is serving as a power source or sink
		PolicyTxHeader.SpecRevision = USBPDSPECREV;	// Set the spec revision
		set_pdtx_state(txSend);	// Indicate to the Protocol layer that there is something to send
		break;
	case txSend:
	case txBusy:
	case txWait:
		// Waiting for GoodCRC or timeout of the protocol
		// May want to put in a second level timeout in case there's an issue with the protocol getting hung
		break;
	case txSuccess:
		set_policy_state(nextState);	// Go to the next state requested
		set_policy_subindex(subIndex);
		set_pdtx_state(txIdle);	// Reset the transmitter status
		Status = STAT_SUCCESS;
		break;
	default:		// For all error cases (and undefined),
		set_policy_state(peErrorRecovery);	// Go to the error recovery state
		set_policy_subindex(0);	// Reset the sub index
		set_pdtx_state(txReset);	// Set the transmitter status to send a hard reset
		Status = STAT_ERROR;
		break;
	}
	return Status;
}

u8 PolicySendData(u8 MessageType, u8 NumDataObjects,
		  doDataObject_t * DataObjects, PolicyState_t nextState,
		  u8 subIndex)
{
	u8 Status = STAT_BUSY;
	int i;
	if (ProtocolMsgTx == true) {
		set_policy_state(nextState);	// Go to the next state requested
		set_policy_subindex(subIndex);
		set_pdtx_state(txIdle);	// Reset the transmitter status
		Status = STAT_SUCCESS;
		ProtocolMsgTx = false;
	} else {
		switch (PDTxStatus) {
		case txIdle:
			if (NumDataObjects > 7)
				NumDataObjects = 7;
			PolicyTxHeader.word = 0x0000;	// Clear the word to initialize for each transaction
			PolicyTxHeader.NumDataObjects = NumDataObjects;	// Set the number of data objects to send
			PolicyTxHeader.MessageType = MessageType & 0x0F;	// Sets the message type to the what was passed in
			PolicyTxHeader.PortDataRole = PolicyIsDFP;	// Set whether the port is acting as a DFP or UFP
			PolicyTxHeader.PortPowerRole = PolicyIsSource;	// Set whether the port is serving as a power source or sink
			PolicyTxHeader.SpecRevision = USBPDSPECREV;	// Set the spec revision
			for (i = 0; i < NumDataObjects; i++)	// Loop through all of the data objects sent
				PolicyTxDataObj[i].object = DataObjects[i].object;	// Set each buffer object to send for the protocol layer
			if (PolicyState == peSourceSendCaps)	// If we are in the send source caps state...
				CapsCounter++;	// Increment the capabilities counter
			set_pdtx_state(txSend);	// Indicate to the Protocol layer that there is something to send
			break;
		case txSend:
		case txBusy:
		case txWait:
			// Waiting for GoodCRC or timeout of the protocol
			// May want to put in a second level timeout in case there's an issue with the protocol getting hung
			break;
		case txSuccess:
			set_policy_state(nextState);	// Go to the next state requested
			set_policy_subindex(subIndex);
			set_pdtx_state(txIdle);	// Reset the transmitter status
			Status = STAT_SUCCESS;
			break;
		case txError:	// Didn't receive a GoodCRC message...
			if (PolicyState == peSourceSendCaps)	// If we were in the send source caps state when the error occurred...
				set_policy_state(peSourceDiscovery);	// Go to the discovery state
			else if (PolicyIsSource)	// Otherwise, if we are a source...
				set_policy_state(peSourceSendSoftReset);	// Attempt to send a soft reset (source)
			else	// Otherwise...
				set_policy_state(peSinkSendSoftReset);	// Go to the soft reset state
			set_policy_subindex(0);	// Reset the sub index
			set_pdtx_state(txIdle);	// Reset the transmitter status
			Status = STAT_ERROR;
			break;
		case txCollision:
			CollisionCounter++;	// Increment the collision counter
			if (CollisionCounter > nRetryCount)	// If we have already retried two times
			{
				if (PolicyIsSource)
					set_policy_state(peSourceSendHardReset);	// Go to the source hard reset state
				else
					set_policy_state(peSinkSendHardReset);	// Go to the sink hard reset state
				set_pdtx_state(txReset);	// Set the transmitter status to send a hard reset
				Status = STAT_ERROR;
			} else	// Otherwise we are going to try resending the soft reset
				set_pdtx_state(txIdle);	// Clear the transmitter status for the next operation
			break;
		default:	// For undefined cases, reset everything
			if (PolicyIsSource)
				set_policy_state(peSourceSendHardReset);	// Go to the source hard reset state
			else
				set_policy_state(peSinkSendHardReset);	// Go to the sink hard reset state
			set_policy_subindex(0);	// Reset the sub index
			set_pdtx_state(txReset);	// Set the transmitter status to send a hard reset
			Status = STAT_ERROR;
			break;
		}
	}
	return Status;
}

u8 PolicySendDataNoReset(u8 MessageType, u8 NumDataObjects,
			 doDataObject_t * DataObjects, PolicyState_t nextState,
			 u8 subIndex)
{
	u8 Status = STAT_BUSY;
	int i;
	switch (PDTxStatus) {
	case txIdle:
		if (NumDataObjects > 7)
			NumDataObjects = 7;
		PolicyTxHeader.word = 0x0000;	// Clear the word to initialize for each transaction
		PolicyTxHeader.NumDataObjects = NumDataObjects;	// Set the number of data objects to send
		PolicyTxHeader.MessageType = MessageType & 0x0F;	// Sets the message type to the what was passed in
		PolicyTxHeader.PortDataRole = PolicyIsDFP;	// Set whether the port is acting as a DFP or UFP
		PolicyTxHeader.PortPowerRole = PolicyIsSource;	// Set whether the port is serving as a power source or sink
		PolicyTxHeader.SpecRevision = USBPDSPECREV;	// Set the spec revision
		for (i = 0; i < NumDataObjects; i++)	// Loop through all of the data objects sent
			PolicyTxDataObj[i].object = DataObjects[i].object;	// Set each buffer object to send for the protocol layer
		if (PolicyState == peSourceSendCaps)	// If we are in the send source caps state...
			CapsCounter++;	// Increment the capabilities counter
		set_pdtx_state(txSend);	// Indicate to the Protocol layer that there is something to send
		break;
	case txSend:
	case txBusy:
	case txWait:
		// Waiting for GoodCRC or timeout of the protocol
		// May want to put in a second level timeout in case there's an issue with the protocol getting hung
		break;
	case txSuccess:
		set_policy_state(nextState);	// Go to the next state requested
		set_policy_subindex(subIndex);
		set_pdtx_state(txIdle);	// Reset the transmitter status
		Status = STAT_SUCCESS;
		break;
	default:		// For error cases (and undefined), ...
		set_policy_state(peErrorRecovery);	// Go to the error recovery state
		set_policy_subindex(0);	// Reset the sub index
		set_pdtx_state(txReset);	// Set the transmitter status to send a hard reset
		Status = STAT_ERROR;
		break;
	}
	return Status;
}

void UpdateCapabilitiesRx(bool IsSourceCaps)
{
	int i;
	SourceCapsUpdated = IsSourceCaps;	// Set the flag to indicate that the received capabilities are valid
	CapsHeaderReceived.word = PolicyRxHeader.word;	// Store the header for the latest capabilities received
	//FUSB_LOG("header=0x%4x\n", PolicyRxHeader.word);

	for (i = 0; i < CapsHeaderReceived.NumDataObjects; i++) {	// Loop through all of the received data objects
		CapsReceived[i].object = PolicyRxDataObj[i].object;	// Store each capability
		//FUSB_LOG("i=%d, object=0x%x\n", i, PolicyRxDataObj[i].object);
	}
	for (i = CapsHeaderReceived.NumDataObjects; i < 7; i++)	// Loop through all of the invalid objects
		CapsReceived[i].object = 0;	// Clear each invalid object
}

// ##################### USB PD Protocol Layer Routines ##################### //

void USBPDProtocol(void)
{
	//FUSB_LOG("ProtocolState =%d\n", ProtocolState);
	//FUSB_LOG("PDtxState=%d\n", PDTxStatus);
	if (Registers.Status.I_HARDRST) {
		ResetProtocolLayer(true);	// Reset the protocol layer
		if (PolicyIsSource)	// If we are the source...
			set_policy_state(peSourceTransitionDefault);	// set the source transition to default
		else		// Otherwise we are the sink...
			set_policy_state(peSinkTransitionDefault);	// so set the sink transition to default
		set_policy_subindex(0);
		StoreUSBPDToken(false, pdtHardReset);	// Store the hard reset
	} else {
		switch (ProtocolState) {
		case PRLReset:
			ProtocolSendHardReset();	// Send a Hard Reset sequence
			set_pdtx_state(txWait);	// Set the transmission status to wait to signal the policy engine
			set_protocol_state(PRLResetWait);	// Go to the next state to wait for the reset signaling to complete
			ProtocolTimer = tBMCTimeout;	// Set a timeout so that we don't hang waiting for the reset to complete
			usbpd_start_timer(&protocol_hrtimer, ProtocolTimer);
			break;
		case PRLResetWait:	// Wait for the reset signaling to complete
			ProtocolResetWait();
			break;
		case PRLTxSendingMessage:	// We have attempted to transmit and are waiting for it to complete or detect a collision
			ProtocolSendingMessage();	// Determine which state we should go to next
		case PRLIdle:	// Waiting to send or receive a message
			ProtocolIdle();
			break;
		case PRLTxVerifyGoodCRC:	// Wait for message to be received and handle...
			ProtocolVerifyGoodCRC();
			break;
		default:	// In the disabled state, don't do anything
			break;
		}
	}
}

void ProtocolIdle(void)
{
	if (PDTxStatus == txReset)	// If we need to send a hard reset...
		set_protocol_state(PRLReset);	// Set the protocol state to send it
	else if (Registers.Status.I_GCRCSENT || (Registers.Status.RX_EMPTY == 0))	// Otherwise check to see if we have received a message and sent a GoodCRC in response
	{
		ProtocolGetRxPacket();	// Grab the received message to pass up to the policy engine
		set_pdtx_state(txIdle);	// Reset the transmitter status if we are receiving data (discard any packet to send)
	} else if (PDTxStatus == txSend)	// Otherwise check to see if there has been a request to send data...
	{
		ProtocolTransmitMessage();	// If so, send the message
	}
}

void ProtocolResetWait(void)
{
	if (Registers.Status.I_HARDSENT)	// Wait for the reset sequence to complete
	{
		set_protocol_state(PRLIdle);	// Have the protocol state go to idle
		set_pdtx_state(txSuccess);	// Alert the policy engine that the reset signaling has completed
	} else if (ProtocolTimer == 0)	// Wait for the BMCTimeout period before stating success in case the interrupts don't line up
	{
		set_protocol_state(PRLIdle);	// Have the protocol state go to idle
		set_pdtx_state(txSuccess);	// Assume that we have successfully sent a hard reset for now (may change in future)
	}
}

void ProtocolGetRxPacket(void)
{
	int i, j;
	u8 data[3];
	FUSB300ReadFIFO(3, &data[0]);	// Read the Rx token and two header bytes
	PolicyRxHeader.byte[0] = data[1];
	PolicyRxHeader.byte[1] = data[2];
	// Only setting the Tx header here so that we can store what we expect was sent in our PD buffer for the GUI
	/*
	   PolicyTxHeader.word = 0;                                                    // Clear the word to initialize for each transaction
	   PolicyTxHeader.NumDataObjects = 0;                                          // Clear the number of objects since this is a command
	   PolicyTxHeader.MessageType = CMTGoodCRC;                                    // Sets the message type to GoodCRC
	   PolicyTxHeader.PortDataRole = PolicyIsDFP;                                  // Set whether the port is acting as a DFP or UFP
	   PolicyTxHeader.PortPowerRole = PolicyIsSource;                              // Set whether the port is serving as a power source or sink
	   PolicyTxHeader.SpecRevision = USBPDSPECREV;                                 // Set the spec revision
	   PolicyTxHeader.MessageID = PolicyRxHeader.MessageID;                        // Update the message ID for the return packet
	 */
	if ((data[0] & 0xE0) == 0xE0) {
		if ((PolicyRxHeader.NumDataObjects == 0)
		    && (PolicyRxHeader.MessageType == CMTSoftReset)) {
			MessageIDCounter = 0;	// Clear the message ID counter for tx
			MessageID = 0xFF;	// Reset the message ID (always alert policy engine of soft reset)
			ProtocolMsgRx = true;	// Set the flag to pass the message to the policy engine
			SourceCapsUpdated = true;	// Set the source caps updated flag to indicate to the GUI to update the display
		} else if (PolicyRxHeader.MessageID != MessageID)	// If the message ID does not match the stored...
		{
			MessageID = PolicyRxHeader.MessageID;	// Update the stored message ID
			ProtocolMsgRx = true;	// Set the flag to pass the message to the policy engine
		}
	}
	if (PolicyRxHeader.NumDataObjects > 0)	// Did we receive a data message? If so, we want to retrieve the data
	{
		FUSB300ReadFIFO((PolicyRxHeader.NumDataObjects << 2), &ProtocolRxBuffer[0]);	// Grab the data from the FIFO
		for (i = 0; i < PolicyRxHeader.NumDataObjects; i++)	// Load the FIFO data into the data objects (loop through each object)
		{
			for (j = 0; j < 4; j++) {	// Loop through each byte in the object
				PolicyRxDataObj[i].byte[j] = ProtocolRxBuffer[j + (i << 2)];	// Store the actual bytes
			}
		}
	}
	FUSB300ReadFIFO(4, &ProtocolCRC[0]);	// Read out the 4 CRC bytes to move the address to the next packet beginning
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the status bytes to update the ACTIVITY flag (should be clear)
	FUSB_LOG("--------****---------40H=%2x, 41H=%2x\n",
		 Registers.Status.byte[4], Registers.Status.byte[5]);
	//ProtocolFlushRxFIFO();
	//StoreUSBPDMessage(PolicyRxHeader, &PolicyRxDataObj[0], false, data[0]);     // Store the received PD message for the device policy manager (VB GUI)
	//if ((data[0] & 0xE0) == 0xE0)                                               // Only store that we send a GoodCRC if we actually send a GoodCRC
	//    StoreUSBPDMessage(PolicyTxHeader, &PolicyTxDataObj[0], true, 0xE0);     // Store the GoodCRC message that we have sent (SOP)
}

void ProtocolTransmitMessage(void)
{
	int i, j;
	ProtocolFlushTxFIFO();	// Flush the Tx FIFO
	ProtocolLoadSOP();
	if ((PolicyTxHeader.NumDataObjects == 0)
	    && (PolicyTxHeader.MessageType == CMTSoftReset)) {
		MessageIDCounter = 0;	// Clear the message ID counter if transmitting a soft reset
		MessageID = 0xFF;	// Reset the message ID if transmitting a soft reset
		SourceCapsUpdated = true;	// Set the flag to indicate to the GUI to update the display
	}
	PolicyTxHeader.MessageID = MessageIDCounter;	// Update the tx message id to send
	ProtocolTxBuffer[ProtocolTxBytes++] = PACKSYM | (2 + (PolicyTxHeader.NumDataObjects << 2));	// Load the PACKSYM token with the number of bytes in the packet
	ProtocolTxBuffer[ProtocolTxBytes++] = PolicyTxHeader.byte[0];	// Load in the first byte of the header
	ProtocolTxBuffer[ProtocolTxBytes++] = PolicyTxHeader.byte[1];	// Load in the second byte of the header
	if (PolicyTxHeader.NumDataObjects > 0)	// If this is a data object...
	{
		for (i = 0; i < PolicyTxHeader.NumDataObjects; i++)	// Load the FIFO data into the data objects (loop through each object)
		{
			for (j = 0; j < 4; j++)	// Loop through each byte in the object
				ProtocolTxBuffer[ProtocolTxBytes++] = PolicyTxDataObj[i].byte[j];	// Load the actual bytes
		}
	}
	ProtocolLoadEOP();	// Load the CRC, EOP and stop sequence
	FUSB300WriteFIFO(ProtocolTxBytes, &ProtocolTxBuffer[0]);	// Commit the FIFO to the FUSB300
	Registers.Control.TX_START = 1;	// Set the bit to enable the transmitter
	FUSB300Write(regControl0, 1, &Registers.Control.byte[0]);	// Commit TX_START to the FUSB300
	Registers.Control.TX_START = 0;	// Clear this bit, to avoid inadvertently resetting
	set_pdtx_state(txBusy);	// Set the transmitter status to busy
	set_protocol_state(PRLTxSendingMessage);	// Set the protocol state to wait for the transmission to complete
	ProtocolTimer = tBMCTimeout;	// Set the protocol timer for ~2.5ms to allow the BMC to finish transmitting before timing out
	usbpd_start_timer(&protocol_hrtimer, ProtocolTimer);
	StoreUSBPDMessage(PolicyTxHeader, &PolicyTxDataObj[0], true, 0xE0);	// Store all messages that we attempt to send for debugging (SOP)
}

void ProtocolSendingMessage(void)
{
	FUSB_LOG("enter %s\n", __func__);
	if (Registers.Status.I_TXSENT) {
		ProtocolFlushTxFIFO();	// Flush the Tx FIFO
		ProtocolVerifyGoodCRC();
	} else if (Registers.Status.I_COLLISION)	// If there was a collision on the bus...
	{
		ProtocolFlushTxFIFO();	// Discard the message and flush the Tx FIFO (enable the auto preamble)
		set_pdtx_state(txCollision);	// Indicate to the policy engine that there was a collision with the last transmission
		ProtocolTimer = tBMCTimeout;	// Set a timeout so that we don't hang waiting for a packet
		usbpd_start_timer(&protocol_hrtimer, ProtocolTimer);
		set_protocol_state(PRLRxWait);	// Go to the RxWait state to receive whatever message is incoming...
	} else if (ProtocolTimer == 0)	// If we have timed out waiting for the transmitter to complete...
	{
		ProtocolFlushTxFIFO();	// Discard the message and flush the Tx FIFO
		ProtocolFlushRxFIFO();	// Flush the Rx FIFO
		set_pdtx_state(txError);	// Set the transmission status to error to signal the policy engine
		set_protocol_state(PRLIdle);	// Set the state variable to the idle state
	}
}

void ProtocolVerifyGoodCRC(void)
{
	int i, j;
	u8 data[3];
	FUSB300ReadFIFO(3, &data[0]);	// Read the Rx token and two header bytes
	PolicyRxHeader.byte[0] = data[1];
	PolicyRxHeader.byte[1] = data[2];
	if ((PolicyRxHeader.NumDataObjects == 0)
	    && (PolicyRxHeader.MessageType == CMTGoodCRC)) {
		FUSB300ReadFIFO(4, &ProtocolCRC[0]);	// Flush the Rx FIFO to make sure it's clean upon the next read
		if (PolicyRxHeader.MessageID != MessageIDCounter)	// If the message ID doesn't match...
		{
			StoreUSBPDToken(false, pdtBadMessageID);	// Store that there was a bad message ID received in the buffer
			set_pdtx_state(txError);	// Set the transmission status to error to signal the policy engine
			set_protocol_state(PRLIdle);	// Set the state variable to the idle state
		} else		// Otherwise, we've received a good CRC response to our message sent
		{
			MessageIDCounter++;	// Increment the message ID counter
			MessageIDCounter &= 0x07;	// Rollover the counter so that it fits
			set_protocol_state(PRLIdle);	// Set the idle state
			set_pdtx_state(txSuccess);	// Set the transmission status to success to signal the policy engine
			ProtocolMsgTx = true;
			StoreUSBPDMessage(PolicyRxHeader, &PolicyRxDataObj[0], false, data[0]);	// Store the received PD message for the device policy manager (VB GUI)
		}
	} else {
		set_protocol_state(PRLIdle);	// Set the idle protocol state (let the policy engine decide next steps)
		set_pdtx_state(txError);	// Flag the policy engine that we didn't successfully transmit
		if ((PolicyRxHeader.NumDataObjects == 0)
		    && (PolicyRxHeader.MessageType == CMTSoftReset)) {
			FUSB300ReadFIFO(4, &ProtocolCRC[0]);	// Read out the 4 CRC bytes to move the address to the next packet beginning
			MessageIDCounter = 0;	// Clear the message ID counter for tx
			MessageID = 0xFF;	// Reset the message ID (always alert policy engine of soft reset)
			ProtocolMsgRx = true;	// Set the flag to pass the message to the policy engine
			SourceCapsUpdated = true;	// Set the flag to indicate to the GUI to update the display
		} else if (PolicyRxHeader.MessageID != MessageID)	// If the message ID does not match the stored...
		{
			FUSB300ReadFIFO(4, &ProtocolCRC[0]);	// Read out the 4 CRC bytes to move the address to the next packet beginning
			MessageID = PolicyRxHeader.MessageID;	// Update the stored message ID
			ProtocolMsgRx = true;	// Set the flag to pass the message to the policy engine
		}
		if (PolicyRxHeader.NumDataObjects > 0)	// If this is a data message, grab the data objects
		{
			FUSB300ReadFIFO(PolicyRxHeader.NumDataObjects << 2, &ProtocolRxBuffer[0]);	// Grab the data from the FIFO
			for (i = 0; i < PolicyRxHeader.NumDataObjects; i++)	// Load the FIFO data into the data objects (loop through each object)
			{
				for (j = 0; j < 4; j++)	// Loop through each byte in the object
					PolicyRxDataObj[i].byte[j] = ProtocolRxBuffer[j + (i << 2)];	// Store the actual bytes
			}
		}
		StoreUSBPDMessage(PolicyRxHeader, &PolicyRxDataObj[0], false, data[0]);	// Store the received PD message for the device policy manager (VB GUI)
	}
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the status bytes to update the ACTIVITY flag (should be clear)
	FUSB_LOG("--------***---------40H=%2x, 41H=%2x\n",
		 Registers.Status.byte[4], Registers.Status.byte[5]);
}

void ProtocolLoadSOP(void)
{
	ProtocolTxBytes = 0;	// Clear the Tx byte counter
	//USBPDTxBuffer[ProtocolTxBytes++] = TXON;                                  // Load the PD start sequence (turn on the transmitter) (NO COLLISION DETECTION?)
	ProtocolTxBuffer[ProtocolTxBytes++] = SOP1;	// Load in the Sync-1 pattern
	ProtocolTxBuffer[ProtocolTxBytes++] = SOP1;	// Load in the Sync-1 pattern
	ProtocolTxBuffer[ProtocolTxBytes++] = SOP1;	// Load in the Sync-1 pattern
	ProtocolTxBuffer[ProtocolTxBytes++] = SOP2;	// Load in the Sync-2 pattern
}

void ProtocolLoadEOP(void)
{
	ProtocolTxBuffer[ProtocolTxBytes++] = JAM_CRC;	// Load in the token to calculate and add the CRC
	ProtocolTxBuffer[ProtocolTxBytes++] = EOP;	// Load in the EOP pattern
	ProtocolTxBuffer[ProtocolTxBytes++] = TXOFF;	// Load in the PD stop sequence (turn off the transmitter)
}

void ProtocolSendHardReset(void)
{
	u8 data;
	data = Registers.Control.byte[3] | 0x40;	// Set the send hard reset bit
	FUSB300Write(regControl3, 1, &data);	// Send the hard reset
	StoreUSBPDToken(true, pdtHardReset);	// Store the hard reset
}

void ProtocolFlushRxFIFO(void)
{
	u8 data;
	data = Registers.Control.byte[1];	// Grab the current control word
	data |= 0x04;		// Set the RX_FLUSH bit (auto-clears)
	FUSB300Write(regControl1, 1, &data);	// Commit the flush to the FUSB300
}

void ProtocolFlushTxFIFO(void)
{
	u8 data;
	data = Registers.Control.byte[0];	// Grab the current control word
	data |= 0x40;		// Set the TX_FLUSH bit (auto-clears)
	FUSB300Write(regControl0, 1, &data);	// Commit the flush to the FUSB300
}

void ResetProtocolLayer(bool ResetPDLogic)
{
	int i;
	u8 data = 0x02;
	if (ResetPDLogic)
		FUSB300Write(regReset, 1, &data);	// Reset the PD logic
	ProtocolFlushRxFIFO();	// Flush the Rx FIFO
	ProtocolFlushTxFIFO();	// Flush the Tx FIFO (enable Auto Preamble)
	set_protocol_state(PRLIdle);	// Initialize the protocol layer to the idle state
	set_pdtx_state(txIdle);	// Initialize the transmitter status
	ProtocolTimer = 0;	// Reset the protocol state timer
	ProtocolTxBytes = 0;	// Clear the byte count for the Tx FIFO
	MessageIDCounter = 0;	// Clear the message ID counter
	MessageID = 0xFF;	// Reset the message ID (invalid value to indicate nothing received yet)
	ProtocolMsgRx = false;	// Reset the message ready flag
	ProtocolMsgTx = false;
	USBPDTxFlag = false;	// Clear the flag to make sure we don't send something by accident
	PolicyHasContract = false;	// Clear the flag that indicates we have a PD contract
	USBPDContract.object = 0;	// Clear the actual USBPD contract request object
	SourceCapsUpdated = true;	// Update the source caps flag to trigger an update of the GUI
	CapsHeaderReceived.word = 0;	// Clear any received capabilities messages
	for (i = 0; i < 7; i++)	// Loop through all the received capabilities objects
		CapsReceived[i].object = 0;	// Clear each object
}

// ####################### USB PD Debug Buffer Routines ##################### //

bool StoreUSBPDToken(bool transmitter, USBPD_BufferTokens_t token)
{
	u8 header1 = 1;		// Declare and set the message size
	if (ClaimBufferSpace(2) == false)	// Attempt to claim the number of bytes required in the buffer
		return false;	// If there was an error, return that we failed
	if (transmitter)	// If we are the transmitter...
		header1 |= 0x40;	// set the transmitter bit
	USBPDBuf[USBPDBufEnd++] = header1;	// Set the first header byte (Token type, direction and size)
	USBPDBufEnd %= PDBUFSIZE;	// Wrap the pointer if it is too large
	token &= 0x0F;		// Build the 2nd header byte
	USBPDBuf[USBPDBufEnd++] = token;	// Set the second header byte (actual token)
	USBPDBufEnd %= PDBUFSIZE;	// Wrap the pointer if it is too large
	return true;
}

bool StoreUSBPDMessage(sopMainHeader_t Header, doDataObject_t * DataObject,
		       bool transmitter, u8 SOPToken)
{
	int i, j, required;
	u8 header1;
	required = Header.NumDataObjects * 4 + 2 + 2;	// Determine how many bytes are needed for the buffer
	if (ClaimBufferSpace(required) == false)	// Attempt to claim the number of bytes required in the buffer
		return false;	// If there was an error, return that we failed
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
	return true;
}

u8 GetNextUSBPDMessageSize(void)
{
	u8 numBytes;
	if (USBPDBufStart == USBPDBufEnd)	// If the start and end are equal, the buffer is empty
		numBytes = 0;	// Clear the number of bytes so that we return 0
	else			// otherwise there is data in the buffer...
		numBytes = (USBPDBuf[USBPDBufStart] & 0x1F) + 1;	// Get the number of bytes associated with the message
	return numBytes;
}

u8 GetUSBPDBufferNumBytes(void)
{
	u8 bytes;
	if (USBPDBufStart == USBPDBufEnd)	// If the buffer is empty (using the keep one slot open approach)
		bytes = 0;	// return 0
	else if (USBPDBufEnd > USBPDBufStart)	// If the buffer hasn't wrapped...
		bytes = USBPDBufEnd - USBPDBufStart;	// simply subtract the end from the beginning
	else			// Otherwise it has wrapped...
		bytes = USBPDBufEnd + (PDBUFSIZE - USBPDBufStart);	// calculate the available this way
	return bytes;
}

bool ClaimBufferSpace(int intReqSize)
{
	int available;
	u8 numBytes;
	if (intReqSize >= PDBUFSIZE)	// If we cannot claim enough space...
		return false;	// Get out of here
	if (USBPDBufStart == USBPDBufEnd)	// If the buffer is empty (using the keep one slot open approach)
		available = PDBUFSIZE;	// Buffer is empty...
	else if (USBPDBufStart > USBPDBufEnd)	// If the buffer has wrapped...
		available = USBPDBufStart - USBPDBufEnd;	// calculate this way
	else			// Otherwise
		available = PDBUFSIZE - (USBPDBufEnd - USBPDBufStart);	// calculate the available this way
	do {
		if (intReqSize >= available)	// If we don't have enough room in the buffer, we need to make room (always keep 1 spot open)
		{
			USBPDBufOverflow = true;	// Set the overflow flag to alert the GUI that we are overwriting data
			numBytes = GetNextUSBPDMessageSize();	// Get the size of the next USB PD message in the buffer
			if (numBytes == 0)	// If the buffer is empty...
				return false;	// Return false since the data cannot fit in the available buffer size (nothing written)
			available += numBytes;	// Add the recovered bytes to the number available
			USBPDBufStart += numBytes;	// Adjust the pointer to the new starting address
			USBPDBufStart %= PDBUFSIZE;	// Wrap the pointer if necessary
		} else
			break;
	} while (1);		// Loop until we have enough bytes
	return true;
}

// ##################### PD Status Routines #################### //

void GetUSBPDStatus(unsigned char abytData[])
{
	int i, j;
	int intIndex = 0;
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

u8 GetUSBPDStatusOverview(void)
{
	unsigned char status = 0;
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
	SourceCapsUpdated = false;
	if (USBPDBufOverflow)
		status |= 0x80;
	return status;
}

u8 ReadUSBPDBuffer(unsigned char *pData, unsigned char bytesAvail)
{
	u8 i, msgSize, bytesRead;
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

void EnableUSBPD(void)
{
	bool enabled = blnSMEnabled;	// Store the current 300 state machine enabled state
	if (USBPDEnabled)	// If we are already enabled...
		return;		// return since we don't have to do anything
	else {
		DisableFUSB300StateMachine();	// Disable the FUSB300 state machine to get to a known state
		USBPDEnabled = true;	// Set the USBPD state machine to enabled
		if (enabled)	// If we were previously enabled...
			EnableFUSB300StateMachine();	// Re-enable the FUSB300 state machine
	}
}

void DisableUSBPD(void)
{
	bool enabled = blnSMEnabled;	// Store the current 300 state machine enabled state
	if (!USBPDEnabled)	// If we are already disabled...
		return;		// return since we don't need to do anything
	else {
		DisableFUSB300StateMachine();	// Disable the FUSB300 state machine to get to a known state
		USBPDEnabled = false;	// Set the USBPD state machine to enabled
		if (enabled)	// If we were previously enabled...
			EnableFUSB300StateMachine();	// Re-enable the state machine
	}
}

void SendUSBPDMessage(unsigned char *abytData)
{
	int i, j;
	PDTransmitHeader.byte[0] = *abytData++;	// Set the 1st PD header byte
	PDTransmitHeader.byte[1] = *abytData++;	// Set the 2nd PD header byte
	for (i = 0; i < PDTransmitHeader.NumDataObjects; i++)	// Loop through all the data objects
	{
		for (j = 0; j < 4; j++)	// Loop through each byte of the object
		{
			PDTransmitObjects[i].byte[j] = *abytData++;	// Set the actual bytes
		}
	}
	USBPDTxFlag = true;	// Set the flag to send when appropriate
}

void WriteSourceCapabilities(unsigned char *abytData)
{
	int i, j;
	sopMainHeader_t Header;
	Header.byte[0] = *abytData++;	// Set the 1st PD header byte
	Header.byte[1] = *abytData++;	// Set the 2nd PD header byte
	if ((Header.NumDataObjects > 0) && (Header.MessageType == DMTSourceCapabilities))	// Only do anything if we decoded a source capabilities message
	{
		CapsHeaderSource.word = Header.word;	// Set the actual caps source header
		for (i = 0; i < CapsHeaderSource.NumDataObjects; i++)	// Loop through all the data objects
		{
			for (j = 0; j < 4; j++)	// Loop through each byte of the object
				CapsSource[i].byte[j] = *abytData++;	// Set the actual bytes
		}
		if (PolicyIsSource)	// If we are currently acting as the source...
		{
			PDTransmitHeader.word = CapsHeaderSource.word;	// Set the message type to capabilities to trigger sending the caps (only need the header to trigger)
			USBPDTxFlag = true;	// Set the flag to send the new caps when appropriate...
			SourceCapsUpdated = true;	// Set the flag to indicate to the software that the source caps were updated
		}
	}
}

void ReadSourceCapabilities(unsigned char *abytData)
{
	int i, j;
	*abytData++ = CapsHeaderSource.byte[0];
	*abytData++ = CapsHeaderSource.byte[1];
	for (i = 0; i < CapsHeaderSource.NumDataObjects; i++) {
		for (j = 0; j < 4; j++)
			*abytData++ = CapsSource[i].byte[j];
	}
}

void WriteSinkCapabilities(unsigned char *abytData)
{
	int i, j;
	sopMainHeader_t Header;
	Header.byte[0] = *abytData++;	// Set the 1st PD header byte
	Header.byte[1] = *abytData++;	// Set the 2nd PD header byte
	if ((Header.NumDataObjects > 0) && (Header.MessageType == DMTSinkCapabilities))	// Only do anything if we decoded a source capabilities message
	{
		CapsHeaderSink.word = Header.word;	// Set the actual caps sink header
		for (i = 0; i < CapsHeaderSink.NumDataObjects; i++)	// Loop through all the data objects
		{
			for (j = 0; j < 4; j++)	// Loop through each byte of the object
				CapsSink[i].byte[j] = *abytData++;	// Set the actual bytes
		}
		// We could also trigger sending the caps or re-evaluating, but we don't do anything with this info here...
	}
}

void ReadSinkCapabilities(unsigned char *abytData)
{
	int i, j;
	*abytData++ = CapsHeaderSink.byte[0];
	*abytData++ = CapsHeaderSink.byte[1];
	for (i = 0; i < CapsHeaderSink.NumDataObjects; i++) {
		for (j = 0; j < 4; j++)
			*abytData++ = CapsSink[i].byte[j];
	}
}

void WriteSinkRequestSettings(unsigned char *abytData)
{
	u32 uintPower;
	SinkGotoMinCompatible = *abytData & 0x01 ? true : false;
	SinkUSBSuspendOperation = *abytData & 0x02 ? true : false;
	SinkUSBCommCapable = *abytData++ & 0x04 ? true : false;
	SinkRequestMaxVoltage = (u32) * abytData++;
	SinkRequestMaxVoltage |= ((u32) (*abytData++) << 8);	// Voltage resolution is 50mV
	uintPower = (u32) * abytData++;
	uintPower |= ((u32) (*abytData++) << 8);
	uintPower |= ((u32) (*abytData++) << 16);
	uintPower |= ((u32) (*abytData++) << 24);
	SinkRequestOpPower = uintPower;	// Power resolution is 0.5mW
	uintPower = (u32) * abytData++;
	uintPower |= ((u32) (*abytData++) << 8);
	uintPower |= ((u32) (*abytData++) << 16);
	uintPower |= ((u32) (*abytData++) << 24);
	SinkRequestMaxPower = uintPower;	// Power resolution is 0.5mW
	// We could try resetting and re-evaluating the source caps here, but lets not do anything until requested by the user (soft reset or detach)
}

void ReadSinkRequestSettings(unsigned char *abytData)
{
	*abytData = SinkGotoMinCompatible ? 0x01 : 0;
	*abytData |= SinkUSBSuspendOperation ? 0x02 : 0;
	*abytData++ |= SinkUSBCommCapable ? 0x04 : 0;
	*abytData++ = (u8) (SinkRequestMaxVoltage & 0xFF);
	*abytData++ = (u8) ((SinkRequestMaxVoltage & 0xFF) >> 8);
	*abytData++ = (u8) (SinkRequestOpPower & 0xFF);
	*abytData++ = (u8) ((SinkRequestOpPower >> 8) & 0xFF);
	*abytData++ = (u8) ((SinkRequestOpPower >> 16) & 0xFF);
	*abytData++ = (u8) ((SinkRequestOpPower >> 24) & 0xFF);
	*abytData++ = (u8) (SinkRequestMaxPower & 0xFF);
	*abytData++ = (u8) ((SinkRequestMaxPower >> 8) & 0xFF);
	*abytData++ = (u8) ((SinkRequestMaxPower >> 16) & 0xFF);
	*abytData++ = (u8) ((SinkRequestMaxPower >> 24) & 0xFF);
}

void SendUSBPDHardReset(void)
{
	if (PolicyIsSource)	// If we are the source...
		set_policy_state(peSourceSendHardReset);	// set the source state to send a hard reset
	else			// Otherwise we are the sink...
		set_policy_state(peSinkSendHardReset);	// so set the sink state to send a hard reset
	set_policy_subindex(0);
	set_pdtx_state(txIdle);	// Reset the transmitter status
	//mPORTASetBits(BIT_0|BIT_1);
}

void InitializeVdmManager()
{
	initialize(&vdmm);
#if 0
	// configure callbacks
	vdmm.req_id_info = sim_RequestIdentityInfo;
	vdmm.req_svid_info = sim_RequestSvidInfo;
	vdmm.req_modes_info = sim_RequestModesInfo;
	vdmm.enter_mode_result = sim_EnterModeResult;
	vdmm.exit_mode_result = sim_ExitModeResult;
	vdmm.inform_id = sim_InformIdentity;
	vdmm.inform_svids = sim_InformSvids;
	vdmm.inform_modes = sim_InformModes;
	vdmm.inform_attention = sim_InformAttention;
	vdmm.req_mode_entry = sim_ModeEntryRequest;
	vdmm.req_mode_exit = sim_ModeExitRequest;
#endif
}

void convertAndProcessVdmMessage()
{
	int i, j;
	// form the word arrays that VDM block expects
	// note: may need to rethink the interface. but this is quicker to develop right now.
	u32 vdm_arr[7];
	for (i = 0; i < PolicyRxHeader.NumDataObjects; i++) {
		vdm_arr[i] = 0;
		for (j = 0; j < 4; j++) {
			vdm_arr[i] <<= 8;	// make room for next byte
			vdm_arr[i] &= 0xFFFFFF00;	// clear bottom byte
			vdm_arr[i] |= (PolicyRxDataObj[i].byte[j]);	// OR next byte in
		}
	}

	// note: assuming only SOP for now. TODO
	processVdmMessage(&vdmm, SOP, vdm_arr, PolicyRxHeader.NumDataObjects);
}

void sendVdmMessage(VdmManager * vdmm, SopType sop, u32 * arr,
		    unsigned int length)
{
	unsigned int i, j;

	// set up transmit header
	PolicyTxHeader.MessageType = DMTVenderDefined;
	PolicyTxHeader.NumDataObjects = length;

	for (i = 0; i < PolicyTxHeader.NumDataObjects; i++) {
		for (j = 0; j < 4; j++) {
			PolicyTxDataObj[i].byte[j] = (arr[i] >> (8 * j));
		}
	}

	set_pdtx_state(txSend);
}

void doVdmCommand()
{
	u32 svdm_header_bits;
	// StructuredVdmHeader svdm_header;
	// unsigned int i;
	unsigned int command;
	unsigned int svid;

	svdm_header_bits = 0;	/*
				   for (i = 0; i < 4; i++) {
				   svdm_header_bits <<= 8;
				   svdm_header_bits |= PDTransmitObjects[0].byte[i];
				   } */

	//svdm_header = getStructuredVdmHeader(svdm_header_bits);

	command = PDTransmitObjects[0].byte[0] & 0x1F;
	svid = 0;
	svid |= (PDTransmitObjects[0].byte[3] << 8);
	svid |= (PDTransmitObjects[0].byte[2] << 0);

	if (command == 1) {
		discoverIdentities(&vdmm, SOP);
	} else if (command == DISCOVER_SVIDS) {
		discoverSvids(&vdmm, SOP);
	} else if (command == DISCOVER_MODES) {
		discoverModes(&vdmm, SOP, svid);
	}

}
