/****************************************************************************
 * FileName:        PDPolicy.c
 * Processor:       PIC32MX250F128B
 * Compiler:        MPLAB XC32
 * Company:         Fairchild Semiconductor
 *
 * Software License Agreement:
 *
 * The software supplied herewith by Fairchild Semiconductor (the ?Company?)
 * is supplied to you, the Company's customer, for exclusive use with its
 * USB Type C / USB PD products.  The software is owned by the Company and/or
 * its supplier, and is protected under applicable copyright laws.
 * All rights are reserved. Any use in violation of the foregoing restrictions
 * may subject the user to criminal sanctions under applicable laws, as well
 * as to civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN ?AS IS? CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 *****************************************************************************/

#ifdef FSC_DEBUG
#include "Log.h"
#endif // FSC_DEBUG

#include "PD_Types.h"
#include "PDPolicy.h"
#include "PDProtocol.h"
#include "TypeC.h"
#include "fusb30X.h"

#ifdef FSC_HAVE_VDM
#include "vdm/vdm_callbacks.h"
#include "vdm/vdm_callbacks_defs.h"
#include "vdm/vdm.h"
#include "vdm/vdm_types.h"
#include "vdm/bitfield_translators.h"
#include "vdm/DisplayPort/dp_types.h"
#include "vdm/DisplayPort/dp.h"
#include "vdm/DisplayPort/interface_dp.h"
#endif // FSC_HAVE_VDM

/////////////////////////////////////////////////////////////////////////////
//      Variables for use with the USB PD state machine
/////////////////////////////////////////////////////////////////////////////
#ifdef FSC_DEBUG
StateLog PDStateLog;		// Log for tracking state transitions and times
extern volatile FSC_U16 Timer_S;	// Tracks seconds elapsed for log timestamp
extern volatile FSC_U16 Timer_tms;	// Tracks tenths of milliseconds elapsed for log timestamp

extern FSC_U8 manualRetries;	// Set to 1 to enable manual retries
extern FSC_U8 nTries;		// Number of tries for manual retry
#endif // FSC_DEBUG

extern FSC_BOOL g_Idle;		// Puts state machine into Idle state
extern USBTypeCCurrent SourceCurrent;	// TypeC advertised current
extern FSC_U8 DetachThreshold;

// Device Policy Manager Variables 
FSC_BOOL USBPDTxFlag;		// Flag to indicate that we need to send a message (set by device policy manager)
FSC_BOOL IsHardReset;		// Variable indicating that a Hard Reset is occurring
FSC_BOOL IsPRSwap;		// Variable indicating that a PRSwap is occurring
FSC_BOOL IsVCONNSource;		// Indicates who is the VCONN source

sopMainHeader_t PDTransmitHeader = { 0 };	// Definition of the PD packet to send
sopMainHeader_t CapsHeaderSink = { 0 };	// Definition of the sink capabilities of the device
sopMainHeader_t CapsHeaderSource = { 0 };	// Definition of the source capabilities of the device
sopMainHeader_t CapsHeaderReceived = { 0 };	// Last capabilities header received (source or sink)
doDataObject_t PDTransmitObjects[7] = { {0} };	// Data objects to send
doDataObject_t CapsSink[7] = { {0} };	// Power object definitions of the sink capabilities of the device
doDataObject_t CapsSource[7] = { {0} };	// Power object definitions of the source capabilities of the device
doDataObject_t CapsReceived[7] = { {0} };	// Last power objects received (source or sink)
doDataObject_t USBPDContract = { 0 };	// Current USB PD contract (request object)
doDataObject_t SinkRequest = { 0 };	// Sink request message

FSC_U32 SinkRequestMaxVoltage;	// Maximum voltage that the sink will request
FSC_U32 SinkRequestMaxPower;	// Maximum power the sink will request (used to calculate current as well)
FSC_U32 SinkRequestOpPower;	// Operating power the sink will request (used to calculate current as well)
FSC_BOOL SinkGotoMinCompatible;	// Whether the sink will respond to the GotoMin command
FSC_BOOL SinkUSBSuspendOperation;	// Whether the sink wants to continue operation during USB suspend
FSC_BOOL SinkUSBCommCapable;	// Whether the sink is USB communications capable

doDataObject_t PartnerCaps = { 0 };	// Partner's Sink Capabilities

#ifdef FSC_DEBUG
FSC_BOOL SourceCapsUpdated;	// Flag to indicate whether we have updated the source caps (for the GUI)
#endif // FSC_DEBUG

// Policy Variables
// removing static qualifier so PolicyState is visible to other code blocks.
// re-org coming soon!
PolicyState_t PolicyState;	// State variable for Policy Engine
PolicyState_t LastPolicyState;	// State variable for Policy Engine
FSC_U8 PolicySubIndex;		// Sub index for policy states
FSC_BOOL PolicyIsSource;	// Flag to indicate whether we are acting as a source or a sink
FSC_BOOL PolicyIsDFP;		// Flag to indicate whether we are acting as a UFP or DFP
FSC_BOOL PolicyHasContract;	// Flag to indicate whether there is a contract in place
FSC_U32 VbusTransitionTime;	// Time to wait for VBUS switch to transition
static FSC_U8 CollisionCounter;	// Collision counter for the policy engine
static FSC_U8 HardResetCounter;	// The number of times a hard reset has been generated
static FSC_U8 CapsCounter;	// Number of capabilities messages sent
volatile FSC_U32 PolicyStateTimer;	// Multi-function timer for the different policy states
volatile FSC_U32 NoResponseTimer;	// Policy engine no response timer
volatile static FSC_U32 SwapSourceStartTimer;	// Delay after power role swap before starting source PD
sopMainHeader_t PolicyRxHeader = { 0 };	// Header object for USB PD messages received
sopMainHeader_t PolicyTxHeader = { 0 };	// Header object for USB PD messages to send
doDataObject_t PolicyRxDataObj[7] = { {0} };	// Buffer for data objects received
doDataObject_t PolicyTxDataObj[7] = { {0} };	// Buffer for data objects to send

static FSC_BOOL isContractValid;	// Is PD Contract Valid

#ifdef FSC_HAVE_VDM
// VDM Manager object
extern VdmManager vdmm;
VdmDiscoveryState_t AutoVdmState;

FSC_U32 vdm_msg_length;
doDataObject_t vdm_msg_obj[7] = { {0} };

PolicyState_t vdm_next_ps;
FSC_BOOL sendingVdmData;

volatile FSC_U32 VdmTimer;
FSC_BOOL VdmTimerStarted;

FSC_U16 auto_mode_disc_tracker;

extern FSC_BOOL mode_entered;
extern SvidInfo core_svid_info;
#endif // FSC_HAVE_VDM

#ifdef FSC_HAVE_DP
extern FSC_U32 DpModeEntered;
extern FSC_S32 AutoModeEntryObjPos;
#endif // FSC_HAVE_DP

extern FSC_BOOL ProtocolCheckRxBeforeTx;
extern FSC_U8 loopCounter;	// Used to count the number of Unattach<->AttachWait loops

/////////////////////////////////////////////////////////////////////////////
//                  Timer Interrupt service routine
/////////////////////////////////////////////////////////////////////////////
void PolicyTick(void)
{
	if (!USBPDActive)
		return;

	if (PolicyStateTimer)	// If the PolicyStateTimer is greater than zero...
		PolicyStateTimer--;	// Decrement it
	if ((NoResponseTimer < T_TIMER_DISABLE) && (NoResponseTimer > 0))	// If the timer is enabled and hasn't expired
		NoResponseTimer--;	// Decrement it
	if (SwapSourceStartTimer)
		SwapSourceStartTimer--;

#ifdef FSC_HAVE_VDM
	if (VdmTimer)
		VdmTimer--;
#endif // FSC_HAVE_VDM
}

void InitializePDPolicyVariables(void)
{
	SwapSourceStartTimer = 0;

#ifdef FSC_HAVE_SNK
	SinkRequestMaxVoltage = 180;	// Maximum voltage that the sink will request (9V)
	SinkRequestMaxPower = 1800;	// Maximum power the sink will request (0.5W, used to calculate current as well)
	SinkRequestOpPower = 1800;	// Operating power the sink will request (0.5W, sed to calculate current as well)
	SinkGotoMinCompatible = FALSE;	// Whether the sink will respond to the GotoMin command
	SinkUSBSuspendOperation = FALSE;	// Whether the sink wants to continue operation during USB suspend
	SinkUSBCommCapable = FALSE;	// Whether the sink is USB communications capable

	CapsHeaderSink.NumDataObjects = 2;	// Set the number of power objects to 2
	CapsHeaderSink.PortDataRole = 0;	// Set the data role to UFP by default
	CapsHeaderSink.PortPowerRole = 0;	// By default, set the device to be a sink
	CapsHeaderSink.SpecRevision = 1;	// Set the spec revision to 2.0
	CapsHeaderSink.Reserved0 = 0;
	CapsHeaderSink.Reserved1 = 0;
	CapsSink[0].FPDOSink.Voltage = 100;	// Set 5V for the first supply option
	CapsSink[0].FPDOSink.OperationalCurrent = 10;	// Set that our device will consume 100mA for this object
	CapsSink[0].FPDOSink.DataRoleSwap = 1;	// By default, enable DR_SWAP
	CapsSink[0].FPDOSink.USBCommCapable = 0;	// By default, USB communications is not allowed
	CapsSink[0].FPDOSink.ExternallyPowered = 1;	// By default, we are externally powered
	CapsSink[0].FPDOSink.HigherCapability = FALSE;	// By default, don't require more than vSafe5V
	CapsSink[0].FPDOSink.DualRolePower = 1;	// By default, enable PR_SWAP
	CapsSink[0].FPDOSink.Reserved = 0;

	CapsSink[1].FPDOSink.Voltage = 180;	// Set 12V for the second supply option
	CapsSink[1].FPDOSink.OperationalCurrent = 10;	// Set that our device will consume 100mA for this object
	CapsSink[1].FPDOSink.DataRoleSwap = 0;	// Not used
	CapsSink[1].FPDOSink.USBCommCapable = 0;	// Not used
	CapsSink[1].FPDOSink.ExternallyPowered = 0;	// Not used
	CapsSink[1].FPDOSink.HigherCapability = 0;	// Not used
	CapsSink[1].FPDOSink.DualRolePower = 0;	// Not used
#endif // FSC_HAVE_SNK

#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))

#ifdef FM150911A
	CapsHeaderSource.NumDataObjects = 1;	// Set the number of power objects to 2
#else
	CapsHeaderSource.NumDataObjects = 1;	// Set the number of power objects to 1
#endif // FM150911A
	CapsHeaderSource.PortDataRole = 0;	// Set the data role to UFP by default
	CapsHeaderSource.PortPowerRole = 1;	// By default, set the device to be a source
	CapsHeaderSource.SpecRevision = 1;	// Set the spec revision to 2.0
	CapsHeaderSource.Reserved0 = 0;
	CapsHeaderSource.Reserved1 = 0;

	CapsSource[0].FPDOSupply.Voltage = 100;	// Set 5V for the first supply option
	CapsSource[0].FPDOSupply.MaxCurrent = 150;	// Set 1.5A for the first supply option
	CapsSource[0].FPDOSupply.PeakCurrent = 0;	// Set peak equal to max
	CapsSource[0].FPDOSupply.DataRoleSwap = TRUE;	// By default, don't enable DR_SWAP
	CapsSource[0].FPDOSupply.USBCommCapable = FALSE;	// By default, USB communications is not allowed
	CapsSource[0].FPDOSupply.ExternallyPowered = TRUE;	// By default, state that we are externally powered
	CapsSource[0].FPDOSupply.USBSuspendSupport = FALSE;	// By default, allow  USB Suspend
	CapsSource[0].FPDOSupply.DualRolePower = TRUE;	// By default, enable PR_SWAP
	CapsSource[0].FPDOSupply.SupplyType = 0;	// Fixed supply
	CapsSource[0].FPDOSupply.Reserved = 0;	// Clearing reserved bits  

#ifdef FM150911A
	CapsSource[1].FPDOSupply.Voltage = 240;	// Set 12V for the second supply option
	CapsSource[1].FPDOSupply.MaxCurrent = 150;	// Set 500mA for the second supply option
	CapsSource[1].FPDOSupply.PeakCurrent = 0;	// Set peak equal to max
	CapsSource[1].FPDOSupply.DataRoleSwap = 0;	// Not used... set to zero
	CapsSource[1].FPDOSupply.USBCommCapable = 0;	// Not used... set to zero
	CapsSource[1].FPDOSupply.ExternallyPowered = 0;	// Not used... set to zero
	CapsSource[1].FPDOSupply.USBSuspendSupport = 0;	// Not used... set to zero
	CapsSource[1].FPDOSupply.DualRolePower = 0;	// Allows PR_SWAP
	CapsSource[1].FPDOSupply.SupplyType = 0;	// Fixed supply
#endif // FM150911A
#endif // FSC_HAVE_SRC

#ifdef FSC_DEBUG
	SourceCapsUpdated = FALSE;	// Set the flag to indicate to the GUI that our source caps have been updated
#endif // FSC_DEBUG

	VbusTransitionTime = tFPF2498Transition;	// Default VBUS transition time 20ms

#ifdef FSC_HAVE_VDM
	InitializeVdmManager();	// Initialize VDM Manager
	vdmInitDpm();
	AutoVdmState = AUTO_VDM_INIT;
	auto_mode_disc_tracker = 0;
#endif // FSC_HAVE_VDM

#ifdef FSC_HAVE_DP
	AutoModeEntryObjPos = -1;
#endif // FSC_HAVE_DP

	ProtocolCheckRxBeforeTx = FALSE;
	isContractValid = FALSE;

	Registers.Slice.SDAC = SDAC_DEFAULT;	// Set the SDAC threshold to "default"
	Registers.Slice.SDAC_HYS = 0b01;	// Set hysteresis to 85mV
	DeviceWrite(regSlice, 1, &Registers.Slice.byte);

#ifdef FSC_DEBUG
	InitializeStateLog(&PDStateLog);
#endif // FSC_DEBUG
}

// ##################### USB PD Enable / Disable Routines ################### //

void USBPDEnable(FSC_BOOL DeviceUpdate, SourceOrSink TypeCDFP)
{
	FSC_U8 data[5];
	IsHardReset = FALSE;
	IsPRSwap = FALSE;
	HardResetCounter = 0;

	if (USBPDEnabled == TRUE) {
		if (blnCCPinIsCC1) {	// If the CC pin is on CC1
			Registers.Switches.TXCC1 = 1;	// Enable the BMC transmitter on CC1
			Registers.Switches.MEAS_CC1 = 1;

			Registers.Switches.TXCC2 = 0;	// Disable the BMC transmitter on CC2
			Registers.Switches.MEAS_CC2 = 0;
		} else if (blnCCPinIsCC2) {	// If the CC pin is on CC2
			Registers.Switches.TXCC2 = 1;	// Enable the BMC transmitter on CC2
			Registers.Switches.MEAS_CC2 = 1;

			Registers.Switches.TXCC1 = 0;	// Disable the BMC transmitter on CC1
			Registers.Switches.MEAS_CC1 = 0;
		}
		if (blnCCPinIsCC1 || blnCCPinIsCC2)	// If we know what pin the CC signal is...
		{
			USBPDActive = TRUE;	// Set the active flag
			ResetProtocolLayer(FALSE);	// Reset the protocol layer by default
			NoResponseTimer = T_TIMER_DISABLE;	// Disable the no response timer by default
			PolicyIsSource = TypeCDFP;	// Set whether we should be initially a source or sink
			PolicyIsDFP = TypeCDFP;
			IsVCONNSource = TypeCDFP;
			// Set the initial data port direction
			if (PolicyIsSource)	// If we are a source...
			{
				PolicyState = peSourceStartup;	// initialize the policy engine state to source startup
				PolicySubIndex = 0;
				LastPolicyState = peDisabled;
				Registers.Switches.POWERROLE = 1;	// Initialize to a SRC
				Registers.Switches.DATAROLE = 1;	// Initialize to a DFP
			} else	// Otherwise we are a sink...
			{
				PolicyState = peSinkStartup;	// initialize the policy engine state to sink startup
				PolicySubIndex = 0;
				PolicyStateTimer = 0;
				LastPolicyState = peDisabled;
				Registers.Switches.POWERROLE = 0;	// Initialize to a SNK
				Registers.Switches.DATAROLE = 0;	// Initialize to a UFP

				Registers.Control.ENSOP1 = 0;
				Registers.Control.ENSOP1DP = 0;
				Registers.Control.ENSOP2 = 0;
				Registers.Control.ENSOP2DB = 0;
			}
			Registers.Switches.AUTO_CRC = 0;	// Disable auto CRC until startup
			Registers.Control.AUTO_PRE = 0;	// Disable AUTO_PRE since we are going to use AUTO_CRC
			Registers.Control.N_RETRIES = 3;	// Set the number of retries to 3
			Registers.Power.PWR = 0xF;	// Enable the internal oscillator for USB PD
			DeviceWrite(regPower, 1, &Registers.Power.byte);	// Commit the power setting
#ifdef FSC_DEBUG
			if (manualRetries) {
				Registers.Control.N_RETRIES = 0;	// Set the number of retries to 0
			}
#endif // FSC_DEBUG

			Registers.Control.AUTO_RETRY = 1;	// Enable AUTO_RETRY to use the I_TXSENT interrupt - needed for auto-CRC to work

			data[0] = Registers.Slice.byte;	// Set the slice byte (building one transaction)
			data[1] = Registers.Control.byte[0] | 0x40;	// Set the Control0 byte and set the TX_FLUSH bit (auto-clears)
			data[2] = Registers.Control.byte[1] | 0x04;	// Set the Control1 byte and set the RX_FLUSH bit (auto-clears)
			data[3] = Registers.Control.byte[2];
			data[4] = Registers.Control.byte[3];
			DeviceWrite(regControl0, 4, &data[1]);
			if (DeviceUpdate) {
				DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);	// Commit the switch1 setting
			}
			Registers.Power.PWR = 0xF;	// Enable the internal oscillator for USB PD
			DeviceWrite(regPower, 1, &Registers.Power.byte);	// Commit the power setting

#ifdef FSC_DEBUG
			StoreUSBPDToken(TRUE, pdtAttach);	// Store the PD attach token
#endif // FSC_DEBUG
		}
#ifdef FSC_INTERRUPT_TRIGGERED
		g_Idle = FALSE;	// Go into active mode
		platform_enable_timer(TRUE);
#endif // FSC_INTERRUPT_TRIGGERED

#ifdef FSC_HAVE_VDM
		AutoVdmState = AUTO_VDM_INIT;
#endif /* FSC_HAVE_VDM */
	}
}

void USBPDDisable(FSC_BOOL DeviceUpdate)
{
	IsHardReset = FALSE;
	IsPRSwap = FALSE;
#ifdef FSC_DEBUG
	if (USBPDActive == TRUE)	// If we were previously active...
		StoreUSBPDToken(TRUE, pdtDetach);	// Store the PD detach token
	SourceCapsUpdated = TRUE;	// Set the source caps updated flag to trigger an update of the GUI
#endif // FSC_DEBUG
	USBPDActive = FALSE;	// Clear the USB PD active flag
	ProtocolState = PRLDisabled;	// Set the protocol layer state to disabled
	PolicyState = peDisabled;	// Set the policy engine state to disabled
	PDTxStatus = txIdle;	// Reset the transmitter status
	PolicyIsSource = FALSE;	// Clear the is source flag until we connect again
	PolicyHasContract = FALSE;	// Clear the has contract flag
	DetachThreshold = VBUS_MDAC_3P78;	// Default to 5V detach threshold
	platform_notify_pd_contract(FALSE);

	if (DeviceUpdate) {
		Registers.Switches.TXCC1 = 0;	// Disable the BMC transmitter (both CC1 & CC2)
		Registers.Switches.TXCC2 = 0;
		Registers.Switches.AUTO_CRC = 0;	// turn off Auto CRC
		DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);	// Commit the switch setting
	}
	Registers.Power.PWR = 0x7;	// Disable the internal oscillator for USB PD
	DeviceWrite(regPower, 1, &Registers.Power.byte);	// Commit the power setting
	ProtocolFlushRxFIFO();
	ProtocolFlushTxFIFO();

#ifdef FSC_INTERRUPT_TRIGGERED
	Registers.Mask.M_COLLISION = 1;	// Mask PD Interrupts
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.M_RETRYFAIL = 1;
	Registers.MaskAdv.M_TXSENT = 1;
	Registers.MaskAdv.M_HARDRST = 1;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 1;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
#endif // FSC_INTERRUPT_TRIGGERED

}

// ##################### USB PD Policy Engine Routines ###################### //

void USBPDPolicyEngine(void)
{
#ifdef FSC_DEBUG
	if (LastPolicyState != PolicyState)	// Log Policy State for Debugging
	{
		WriteStateLog(&PDStateLog, PolicyState, Timer_tms, Timer_S);
	}
#endif // FSC_DEBUG

	LastPolicyState = PolicyState;

	switch (PolicyState) {
	case peDisabled:
		break;
	case peErrorRecovery:
		PolicyErrorRecovery();
		break;
		// ###################### Source States  ##################### //
#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
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
	case peSourceWaitNewCapabilities:
		PolicySourceWaitNewCapabilities();
		break;
	case peSourceEvaluateVCONNSwap:
		PolicySourceEvaluateVCONNSwap();
		break;
#endif // FSC_HAVE_SRC
		// ###################### Sink States  ####################### //
#ifdef FSC_HAVE_SNK
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
#endif // FSC_HAVE_SNK

#ifdef FSC_HAVE_VDM
	case peGiveVdm:
		PolicyGiveVdm();
		break;
#endif // FSC_HAVE_VDM

		// ---------- BIST Receive Mode --------------------- //
	case PE_BIST_Receive_Mode:	// Bist Receive Mode
		policyBISTReceiveMode();
		break;
	case PE_BIST_Frame_Received:	// Test Frame received by Protocol layer
		policyBISTFrameReceived();
		break;

		// ---------- BIST Carrier Mode and Eye Pattern ----- //
	case PE_BIST_Carrier_Mode_2:	// BIST Carrier Mode 2
		policyBISTCarrierMode2();
		break;

	case PE_BIST_Test_Data:
		policyBISTTestData();
		break;

	default:
#ifdef FSC_HAVE_VDM
		if ((PolicyState >= FIRST_VDM_STATE)
		    && (PolicyState <= LAST_VDM_STATE)) {
			// valid VDM state
			PolicyVdm();
		} else
#endif // FSC_HAVE_VDM
		{
			// invalid state, reset
			PolicyInvalidState();
		}
		break;
	}
}

// ############################# Source States  ############################# //

void PolicyErrorRecovery(void)
{
	SetStateErrorRecovery();
}

#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void PolicySourceSendHardReset(void)
{
	PolicySendHardReset(peSourceTransitionDefault, 0);
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
		}
		break;
	default:
		if (ProtocolMsgRx)	// If we have received a message
		{
			ProtocolMsgRx = FALSE;	// Reset the message received flag since we've handled it here
			if ((PolicyRxHeader.NumDataObjects == 0) && (PolicyRxHeader.MessageType == CMTAccept))	// And it was the Accept...
			{
				PolicyState = peSourceSendCaps;	// Go to the send caps state 
			} else	// Otherwise it was a message that we didn't expect, so...
				PolicyState = peSourceSendHardReset;	// Go to the hard reset state
			PolicySubIndex = 0;	// Reset the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
		} else if (!PolicyStateTimer)	// If we didn't get a response to our request before timing out...
		{
			PolicyState = peSourceSendHardReset;	// Go to the hard reset state
			PolicySubIndex = 0;	// Reset the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
		}
		break;
	}
}

void PolicySourceStartup(void)
{
#ifdef FSC_HAVE_VDM
	FSC_S32 i;
#endif // FSC_HAVE_VDM

// Set masks for PD
	Registers.Mask.M_COLLISION = 0;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.M_RETRYFAIL = 0;
	Registers.MaskAdv.M_HARDSENT = 0;
	Registers.MaskAdv.M_TXSENT = 0;
	Registers.MaskAdv.M_HARDRST = 0;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 0;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);

	if (Registers.DeviceID.VERSION_ID == VERSION_302B) {
		if (Registers.Control.BIST_TMODE == 1) {
			Registers.Control.BIST_TMODE = 0;	// Disable auto-flush RxFIFO
			DeviceWrite(regControl3, 1, &Registers.Control.byte[3]);
		}
	} else {
		if (Registers.Control.RX_FLUSH == 1)	// Disable Rx flushing if it has been enabled
		{
			Registers.Control.RX_FLUSH = 0;
		}
	}

	switch (PolicySubIndex) {
	case 0:
		USBPDContract.object = 0;	// Clear the USB PD contract (output power to 5V default)
		PartnerCaps.object = 0;	// Clear partner sink caps
		IsPRSwap = FALSE;
		PolicyIsSource = TRUE;	// Set the flag to indicate that we are a source (PRSwaps)
		Registers.Switches.POWERROLE = PolicyIsSource;
		DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);
		ResetProtocolLayer(TRUE);	// Reset the protocol layer
		CapsCounter = 0;	// Clear the caps counter
		CollisionCounter = 0;	// Reset the collision counter
		PolicySubIndex++;
		break;
	case 1:
		if (isVBUSOverVoltage(VBUS_MDAC_4P62) && (SwapSourceStartTimer == 0))	// Wait until we reach vSafe0V and delay if coming from PR Swap
		{
			PolicySubIndex++;
		}
		break;
	case 2:
		PolicyStateTimer = 0;	// Reset the policy state timer
		PolicyState = peSourceSendCaps;	// Go to the source caps
		PolicySubIndex = 0;	// Reset the sub index

#ifdef FSC_HAVE_VDM
		mode_entered = FALSE;

		auto_mode_disc_tracker = 0;

		core_svid_info.num_svids = 0;
		for (i = 0; i < MAX_NUM_SVIDS; i++) {
			core_svid_info.svids[i] = 0;
		}
#endif // FSC_HAVE_VDM

#ifdef FSC_HAVE_DP
		AutoModeEntryObjPos = -1;

		resetDp();
#endif // FSC_HAVE_DP

		break;
	default:
		PolicySubIndex = 0;
		break;
	}
}

void PolicySourceDiscovery(void)
{
	switch (PolicySubIndex) {
	case 0:
		PolicyStateTimer = tTypeCSendSourceCap;	// Initialize the SourceCapabilityTimer
		PolicySubIndex++;	// Increment the sub index
		break;
	default:
		if ((HardResetCounter > nHardResetCount) && (NoResponseTimer == 0) && (PolicyHasContract == TRUE)) {	// If we previously had a contract in place...
			PolicyState = peErrorRecovery;	// Go to the error recovery state since something went wrong
			PolicySubIndex = 0;
		} else if ((HardResetCounter > nHardResetCount) && (NoResponseTimer == 0) && (PolicyHasContract == FALSE)) {	// Otherwise...
			PolicyState = peSourceDisabled;	// Go to the disabled state since we are assuming that there is no PD sink attached
			PolicySubIndex = 0;	// Reset the sub index for the next state
		}
		if (PolicyStateTimer == 0)	// Once the timer expires...
		{
			if (CapsCounter > nCapsCount)	// If we have sent the maximum number of capabilities messages...
				PolicyState = peSourceDisabled;	// Go to the disabled state, no PD sink connected
			else	// Otherwise...
				PolicyState = peSourceSendCaps;	// Go to the send source caps state to send a source caps message
			PolicySubIndex = 0;	// Reset the sub index for the next state
		}
		break;
	}
}

void PolicySourceSendCaps(void)
{
	if ((HardResetCounter > nHardResetCount) && (NoResponseTimer == 0))	// Check our higher level timeout
	{
		if (PolicyHasContract)	// If USB PD was previously established...
			PolicyState = peErrorRecovery;	// Need to go to the error recovery state
		else		// Otherwise...
			PolicyState = peSourceDisabled;	// We are disabling PD and leaving the Type-C connections alone
	} else			// If we haven't timed out and maxed out on hard resets...
	{
		switch (PolicySubIndex) {
		case 0:
			if (PolicySendData
			    (DMTSourceCapabilities,
			     CapsHeaderSource.NumDataObjects, &CapsSource[0],
			     peSourceSendCaps, 1,
			     SOP_TYPE_SOP) == STAT_SUCCESS) {
				HardResetCounter = 0;	// Clear the hard reset counter
				CapsCounter = 0;	// Clear the caps counter
				NoResponseTimer = T_TIMER_DISABLE;	// Stop the no response timer
				PolicyStateTimer = tSenderResponse - tHardResetOverhead;	// Set the sender response timer
			}
			break;
		default:
			if (ProtocolMsgRx)	// If we have received a message
			{
				ProtocolMsgRx = FALSE;	// Reset the message ready flag since we're handling it here
				if ((PolicyRxHeader.NumDataObjects == 1) && (PolicyRxHeader.MessageType == DMTRequest))	// Was this a valid request message?
					PolicyState = peSourceNegotiateCap;	// If so, go to the negotiate capabilities state
				else	// Otherwise it was a message that we didn't expect, so...
					PolicyState = peSourceSendSoftReset;	// Go onto issuing a soft reset
				PolicySubIndex = 0;	// Reset the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
			} else if (!PolicyStateTimer)	// If we didn't get a response to our request before timing out...
			{
				ProtocolMsgRx = FALSE;	// Reset the message ready flag since we've timed out
				PolicyState = peSourceSendHardReset;	// Go to the hard reset state
				PolicySubIndex = 0;	// Reset the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
			}
			break;
		}
	}
}

void PolicySourceDisabled(void)
{
	USBPDContract.object = 0;	// Clear the USB PD contract (output power to 5V default)
	// Wait for a hard reset or detach...
#ifdef FSC_INTERRUPT_TRIGGERED
	if (loopCounter == 0) {
		g_Idle = TRUE;	// Idle until COMP or HARDRST or GCRCSENT
		platform_enable_timer(FALSE);
	}
#endif // FSC_INTERRUPT_TRIGGERED
}

void PolicySourceTransitionDefault(void)
{
	switch (PolicySubIndex) {
	case 0:
		if (PolicyStateTimer == 0) {
			PolicyHasContract = FALSE;
			DetachThreshold = VBUS_MDAC_3P78;	// Default to 5V detach threshold
			platform_notify_pd_contract(FALSE);
			PolicySubIndex++;
		}
		break;
	case 1:
		platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);	// Disable VBUS output
		platform_set_vbus_discharge(TRUE);	// Enabled VBUS discharge path
		if (!PolicyIsDFP)	// Make sure date role is DFP
		{
			PolicyIsDFP = TRUE;;	// Set the current data role
			Registers.Switches.DATAROLE = PolicyIsDFP;	// Update the data role
			DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);	// Commit the data role in the 302
		}
		if (IsVCONNSource)	// Disable VCONN if VCONN Source
		{
			Registers.Switches.VCONN_CC1 = 0;
			Registers.Switches.VCONN_CC2 = 0;
			DeviceWrite(regSwitches0, 1,
				    &Registers.Switches.byte[0]);
		}
		PolicySubIndex++;
		// Adjust output if necessary and start timer prior to going to startup state?
		break;
	case 2:
		if (VbusVSafe0V())	// Allow the voltage to drop to 0
		{
			platform_set_vbus_discharge(FALSE);	// Disable VBUS discharge path
			PolicyStateTimer = tSrcRecover;
			PolicySubIndex++;
		}
		break;
	case 3:
		if (PolicyStateTimer == 0)	// Wait tSrcRecover to turn VBUS on
		{
			PolicySubIndex++;
		}
		break;
	default:
		platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE, FALSE);	// Enable the 5V source
		if (blnCCPinIsCC1) {
			Registers.Switches.VCONN_CC2 = 1;
		} else {
			Registers.Switches.VCONN_CC1 = 1;
		}
		DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
		IsVCONNSource = TRUE;
		NoResponseTimer = tNoResponse;	// Initialize the no response timer
		PolicyState = peSourceStartup;	// Go to the startup state
		PolicySubIndex = 0;
		break;
	}
}

void PolicySourceNegotiateCap(void)
{
	// This state evaluates if the sink request can be met or not and sets the next state accordingly
	FSC_BOOL reqAccept = FALSE;	// Set a flag that indicates whether we will accept or reject the request
	FSC_U8 objPosition;	// Get the requested object position
	objPosition = PolicyRxDataObj[0].FVRDO.ObjectPosition;	// Get the object position reference
	if ((objPosition > 0) && (objPosition <= CapsHeaderSource.NumDataObjects))	// Make sure the requested object number if valid, continue validating request
	{
		if ((PolicyRxDataObj[0].FVRDO.OpCurrent <= CapsSource[objPosition - 1].FPDOSupply.MaxCurrent))	// Ensure the default power/current request is available
			reqAccept = TRUE;	// If the source can supply the request, set the flag to respond
	}
	if (reqAccept)		// If we have received a valid request...
	{
		PolicyState = peSourceTransitionSupply;	// Go to the transition supply state

	} else			// Otherwise the request was invalid...
		PolicyState = peSourceCapabilityResponse;	// Go to the capability response state to send a reject/wait message
}

void PolicySourceTransitionSupply(void)
{
	FSC_U8 sourceVoltage = 0;
	switch (PolicySubIndex) {
	case 0:
		PolicySendCommand(CMTAccept, peSourceTransitionSupply, 1);	// Send the Accept message
		break;
	case 1:
		PolicyStateTimer = tSrcTransition;	// Initialize the timer to allow for the sink to transition
		PolicySubIndex++;	// Increment to move to the next sub state
		break;
	case 2:
		if (!PolicyStateTimer)	// If the timer has expired (the sink is ready)...
			PolicySubIndex++;	// Increment to move to the next sub state
		break;
	case 3:
		PolicyHasContract = TRUE;	// Set the flag to indicate that a contract is in place
		USBPDContract.object = PolicyRxDataObj[0].object;	// Set the contract to the sink request

		//TODO: More robust selection of PDO
		sourceVoltage =
		    CapsSource[USBPDContract.FVRDO.ObjectPosition -
			       1].FPDOSupply.Voltage;
		if (sourceVoltage == 100)	// If the chosen object is 5V
		{
			if (platform_get_vbus_lvl_enable(VBUS_LVL_5V))	// If the supply is already enabled, go to PS_READY
			{
				PolicySubIndex = 5;
			} else {
				platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE,
							     FALSE);
				PolicyStateTimer = t5To12VTransition;	// Set the policy state timer to allow the load switch time to turn off so we don't short our supplies
				PolicySubIndex++;
			}
		}
#ifdef FM150911A
		else if (sourceVoltage == 240)	// If the chosen object is 12V
		{
			if (platform_get_vbus_lvl_enable(VBUS_LVL_12V))	// If the supply is already enabled, go to PS_READY
			{
				PolicySubIndex = 5;
			} else {
				platform_set_vbus_lvl_enable(VBUS_LVL_12V, TRUE,
							     FALSE);
				PolicyStateTimer = t5To12VTransition;	// Set the policy state timer to allow the load switch time to turn off so we don't short our supplies
				PolicySubIndex++;
			}
		}
#endif // FM150911A
		else		// Default to vSafe5V
		{
			if (platform_get_vbus_lvl_enable(VBUS_LVL_5V))	// If the supply is already enabled, go to PS_READY
			{
				PolicySubIndex = 5;
			} else {
				platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE,
							     FALSE);
				PolicyStateTimer = t5To12VTransition;	// Set the policy state timer to allow the load switch time to turn off so we don't short our supplies
				PolicySubIndex++;
			}
		}
		break;
	case 4:
		// Validate the output is ready prior to sending the ready message (only using a timer for now, could validate using an ADC as well)
		if (PolicyStateTimer == 0) {
			if (CapsSource[USBPDContract.FVRDO.ObjectPosition - 1].
			    FPDOSupply.Voltage == 100) {
				platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE, TRUE);	// Disable the "other" vbus outputs
			}
#ifdef FM150911A
			else if (CapsSource
				 [USBPDContract.FVRDO.ObjectPosition -
				  1].FPDOSupply.Voltage == 240) {
				platform_set_vbus_lvl_enable(VBUS_LVL_12V, TRUE, TRUE);	// Disable the "other" vbus outputs
			}
#endif // FM150911A
			else {
				platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE, TRUE);	// Disable the "other" vbus outputs
			}

			PolicyStateTimer = tSourceRiseTimeout;	// Source rise timeout 
			PolicySubIndex++;	// Increment to move to the next sub state
		}
		break;
	case 5:
		if (CapsSource[USBPDContract.FVRDO.ObjectPosition - 1].
		    FPDOSupply.Voltage == 100) {
			if ((!isVBUSOverVoltage(VBUS_MDAC_5P04) && isVBUSOverVoltage(VBUS_MDAC_4P62)) || (PolicyStateTimer == 0))	// Check that VBUS is between 4.6 and 5.5 V
			{
				DetachThreshold = VBUS_MDAC_3P78;	// 5V detach threshold
				PolicySubIndex++;
			}
		}
#ifdef FM150911A
		else if (CapsSource[USBPDContract.FVRDO.ObjectPosition - 1].
			 FPDOSupply.Voltage == 240) {
			if ((isVBUSOverVoltage(VBUS_MDAC_11P76)) || (PolicyStateTimer == 0))	// Check that VBUS is over 11.8V
			{
				DetachThreshold = VBUS_MDAC_9P66;	// 12V detach threshold
				PolicySubIndex++;
			}
		}
#endif // FM150911A
		else if (PolicyStateTimer == 0) {
			PolicySubIndex++;
		}
		break;
	default:
		if (PolicySendCommand(CMTPS_RDY, peSourceReady, 0) == STAT_SUCCESS)	// Send the PS_RDY message and move onto the Source Ready state
		{
			if (Registers.Control.HOST_CUR == 0b01)	// Host current must be set to 1.5A or 3.0A in explicit contract
			{
				Registers.Control.HOST_CUR = 0b10;
				DeviceWrite(regControl0, 1,
					    &Registers.Control.byte[0]);
			}
			platform_notify_pd_contract(TRUE);
		}
		break;
	}
}

void PolicySourceCapabilityResponse(void)
{
	if (PolicyHasContract)	// If we currently have a contract, issue the reject and move back to the ready state
	{
		if (isContractValid) {
			PolicySendCommand(CMTReject, peSourceReady, 0);	// Send the message and continue onto the ready state
		} else {
			PolicySendCommand(CMTReject, peSourceSendHardReset, 0);	// Send the message and continue onto the ready state
		}
	} else			// If there is no contract in place, issue a hard reset
	{
		PolicySendCommand(CMTReject, peSourceWaitNewCapabilities, 0);	// Send Reject and continue onto the Source Wait New Capabilities state after success
	}
}

void PolicySourceReady(void)
{
	if (ProtocolMsgRx)	// Have we received a message from the sink?
	{
		ProtocolMsgRx = FALSE;	// Reset the message received flag since we're handling it here
		if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
		{
			switch (PolicyRxHeader.MessageType)	// Determine which command was received
			{
			case CMTGetSourceCap:
				PolicyState = peSourceGiveSourceCaps;	// Send out the caps
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTGetSinkCap:	// If we receive a get sink capabilities message...
				PolicyState = peSourceGiveSinkCaps;	// Go evaluate whether we are going to send sink caps or reject
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTDR_Swap:	// If we get a DR_Swap message...
				PolicyState = peSourceEvaluateDRSwap;	// Go evaluate whether we are going to accept or reject the swap
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTPR_Swap:
				PolicyState = peSourceEvaluatePRSwap;	// Go evaluate whether we are going to accept or reject the swap
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTVCONN_Swap:	// If we get a VCONN_Swap message...
				PolicyState = peSourceEvaluateVCONNSwap;	// Go evaluate whether we are going to accept or reject the swap
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTSoftReset:
				PolicyState = peSourceSoftReset;	// Go to the soft reset state
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			default:	// Send a reject message for all other commands
				break;
			}
		} else		// If we received a data message... for now just send a soft reset
		{
			switch (PolicyRxHeader.MessageType) {
			case DMTRequest:
				PolicyState = peSourceNegotiateCap;	// If we've received a request object, go to the negotiate capabilities state
				break;
#ifdef FSC_HAVE_VDM
			case DMTVenderDefined:
				convertAndProcessVdmMessage(ProtocolMsgRxSop);
				break;
#endif // FSC_HAVE_VDM
			case DMTBIST:
				processDMTBIST();
				break;
			default:	// Otherwise we've received a message we don't know how to handle yet
				break;
			}
			PolicySubIndex = 0;	// Clear the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
		}
	} else if (USBPDTxFlag)	// Has the device policy manager requested us to send a message?
	{
		if (PDTransmitHeader.NumDataObjects == 0) {
			switch (PDTransmitHeader.MessageType)	// Determine which command we need to send
			{
			case CMTGetSinkCap:
				PolicyState = peSourceGetSinkCaps;	// Go to the get sink caps state
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTGetSourceCap:
				PolicyState = peSourceGetSourceCaps;	// Go to the get source caps state
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTPing:
				PolicyState = peSourceSendPing;	// Go to the send ping state
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTGotoMin:
				PolicyState = peSourceGotoMin;	// Go to the source goto min state
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
#ifdef FSC_HAVE_DRP
			case CMTPR_Swap:
				PolicyState = peSourceSendPRSwap;	// Issue a PR_Swap message
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
#endif // FSC_HAVE_DRP
			case CMTDR_Swap:
				PolicyState = peSourceSendDRSwap;	// Issue a DR_Swap message
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTVCONN_Swap:
				PolicyState = peSourceSendVCONNSwap;	// Issue a VCONN_Swap message
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTSoftReset:
				PolicyState = peSourceSendSoftReset;	// Go to the soft reset state
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			default:	// Don't send any commands we don't know how to handle yet
				break;
			}
		} else {
			switch (PDTransmitHeader.MessageType) {
			case DMTSourceCapabilities:
				PolicyState = peSourceSendCaps;
				PolicySubIndex = 0;
				PDTxStatus = txIdle;
				break;
			case DMTVenderDefined:
				PolicySubIndex = 0;
#ifdef FSC_HAVE_VDM
				doVdmCommand();
#endif // FSC_HAVE_VDM
				break;
			default:
				break;
			}
		}
		USBPDTxFlag = FALSE;
	} else if (PartnerCaps.object == 0) {
		PolicyState = peSourceGetSinkCaps;	// Go to the get sink caps state
		PolicySubIndex = 0;	// Clear the sub index
		PDTxStatus = txIdle;	// Clear the transmitter status
	}
#ifdef FSC_HAVE_VDM
	else if (PolicyIsDFP && (AutoVdmState != AUTO_VDM_DONE)) {
		autoVdmDiscovery();
	}
#endif // FSC_HAVE_VDM
	else {
#ifdef FSC_INTERRUPT_TRIGGERED
		if (loopCounter == 0) {
			g_Idle = TRUE;	// Idle until COMP or HARDRST or GCRCSENT
			platform_enable_timer(FALSE);
		}
#endif // FSC_INTERRUPT_TRIGGERED
	}

}

void PolicySourceGiveSourceCap(void)
{
	PolicySendData(DMTSourceCapabilities, CapsHeaderSource.NumDataObjects,
		       &CapsSource[0], peSourceReady, 0, SOP_TYPE_SOP);
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
			PolicyStateTimer = tSenderResponse;	// Start the sender response timer upon receiving the good CRC message
		break;
	default:
		if (ProtocolMsgRx)	// If we have received a message
		{
			ProtocolMsgRx = FALSE;	// Reset the message ready flag since we're handling it here
			if ((PolicyRxHeader.NumDataObjects > 0)
			    && (PolicyRxHeader.MessageType ==
				DMTSinkCapabilities)) {
				UpdateCapabilitiesRx(FALSE);
				PolicyState = peSourceReady;	// Go onto the source ready state
			} else	// If we didn't receive a valid sink capabilities message...
			{
				PolicyState = peSourceSendHardReset;	// Go onto issuing a hard reset
			}
			PolicySubIndex = 0;	// Reset the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
		} else if (!PolicyStateTimer)	// If we didn't get a response to our request before timing out...
		{
			PolicyState = peSourceSendHardReset;	// Go to the hard reset state
			PolicySubIndex = 0;	// Reset the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
		}
		break;
	}
}

void PolicySourceGiveSinkCap(void)
{
#ifdef FSC_HAVE_DRP
	if (PortType == USBTypeC_DRP)
		PolicySendData(DMTSinkCapabilities,
			       CapsHeaderSink.NumDataObjects, &CapsSink[0],
			       peSourceReady, 0, SOP_TYPE_SOP);
	else
#endif // FSC_HAVE_DRP
		PolicySendCommand(CMTReject, peSourceReady, 0);	// Send the reject message and continue onto the ready state
}

void PolicySourceSendPing(void)
{
	PolicySendCommand(CMTPing, peSourceReady, 0);
}

void PolicySourceGotoMin(void)
{
	if (ProtocolMsgRx) {
		ProtocolMsgRx = FALSE;	// Reset the message ready flag since we're handling it here
		if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a control message...
		{
			switch (PolicyRxHeader.MessageType)	// Determine the message type
			{
			case CMTSoftReset:
				PolicyState = peSourceSoftReset;	// Go to the soft reset state if we received a reset command
				PolicySubIndex = 0;	// Reset the sub index
				PDTxStatus = txIdle;	// Reset the transmitter status
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
			PolicyStateTimer = tSrcTransition;	// Initialize the timer to allow for the sink to transition
			PolicySubIndex++;	// Increment to move to the next sub state
			break;
		case 2:
			if (!PolicyStateTimer)	// If the timer has expired (the sink is ready)...
				PolicySubIndex++;	// Increment to move to the next sub state
			break;
		case 3:
			// Adjust the power supply if necessary...
			PolicySubIndex++;	// Increment to move to the next sub state
			break;
		case 4:
			// Validate the output is ready prior to sending the ready message
			PolicySubIndex++;	// Increment to move to the next sub state
			break;
		default:
			PolicySendCommand(CMTPS_RDY, peSourceReady, 0);	// Send the PS_RDY message and move onto the Source Ready state
			break;
		}
	}
}

void PolicySourceSendDRSwap(void)
{
	FSC_U8 Status;
	switch (PolicySubIndex) {
	case 0:
		Status = PolicySendCommandNoReset(CMTDR_Swap, peSourceSendDRSwap, 1);	// Send the DR_Swap message
		if (Status == STAT_SUCCESS)	// If we received the good CRC message...
			PolicyStateTimer = tSenderResponse;	// Initialize for SenderResponseTimer
		else if (Status == STAT_ERROR)	// If there was an error...
			PolicyState = peErrorRecovery;	// Go directly to the error recovery state
		break;
	default:
		if (ProtocolMsgRx) {
			ProtocolMsgRx = FALSE;	// Reset the message ready flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a control message...
			{
				switch (PolicyRxHeader.MessageType)	// Determine the message type
				{
				case CMTAccept:
					PolicyIsDFP = (PolicyIsDFP == TRUE) ? FALSE : TRUE;	// Flip the current data role
					Registers.Switches.DATAROLE = PolicyIsDFP;	// Update the data role
					DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);	// Commit the data role in the 302 for the auto CRC
					PolicyState = peSourceReady;	// Source ready state
					break;
				case CMTSoftReset:
					PolicyState = peSourceSoftReset;	// Go to the soft reset state if we received a reset command
					break;
				default:	// If we receive any other command (including Reject & Wait), just go back to the ready state without changing
					PolicyState = peSourceReady;	// Go to the source ready state
					break;
				}
			} else	// Otherwise we received a data message...
			{
				PolicyState = peSourceReady;	// Go to the sink ready state if we received a unexpected data message (ignoring message)
			}
			PolicySubIndex = 0;	// Reset the sub index
			PDTxStatus = txIdle;	// Reset the transmitter status
		} else if (PolicyStateTimer == 0)	// If the sender response timer times out...
		{
			PolicyState = peSourceReady;	// Go to the source ready state if the SenderResponseTimer times out
			PolicySubIndex = 0;	// Reset the sub index
			PDTxStatus = txIdle;	// Reset the transmitter status
		}
		break;
	}
}

void PolicySourceEvaluateDRSwap(void)
{
	FSC_U8 Status;

#ifdef FSC_HAVE_VDM
	if (mode_entered == TRUE)	// If were are in modal operation, send a hard reset
	{
		PolicyState = peSourceSendHardReset;
		PolicySubIndex = 0;
		return;
	}
#endif // FSC_HAVE_VDM

	Status = PolicySendCommandNoReset(CMTAccept, peSourceReady, 0);	// Send the Accept message and wait for the good CRC
	if (Status == STAT_SUCCESS)	// If we received the good CRC...
	{
		PolicyIsDFP = (PolicyIsDFP == TRUE) ? FALSE : TRUE;	// We're not really doing anything except flipping the bit
		Registers.Switches.DATAROLE = PolicyIsDFP;	// Update the data role
		DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);	// Commit the data role in the 302 for the auto CRC
	} else if (Status == STAT_ERROR)	// If we didn't receive the good CRC...
	{
		PolicyState = peErrorRecovery;	// Go to the error recovery state
		PolicySubIndex = 0;	// Clear the sub-index
		PDTxStatus = txIdle;	// Clear the transmitter status
	}
}

void PolicySourceSendVCONNSwap(void)
{
	switch (PolicySubIndex) {
	case 0:
		if (PolicySendCommand(CMTVCONN_Swap, peSourceSendVCONNSwap, 1) == STAT_SUCCESS)	// Send the VCONN_Swap message and wait for the good CRC
			PolicyStateTimer = tSenderResponse;	// Once we receive the good CRC, set the sender response timer
		break;
	case 1:
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = FALSE;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTAccept:	// If we get the Accept message...
					PolicySubIndex++;	// Increment the subindex to move onto the next step
					break;
				case CMTWait:	// If we get either the reject or wait message...
				case CMTReject:
					PolicyState = peSourceReady;	// Go back to the source ready state
					PolicySubIndex = 0;	// Clear the sub index
					PDTxStatus = txIdle;	// Clear the transmitter status
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the SenderResponseTimer times out...
		{
			PolicyState = peSourceReady;	// Go back to the source ready state
			PolicySubIndex = 0;	// Clear the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
		}
		break;
	case 2:
		if (IsVCONNSource)	// If we are currently sourcing VCONN...
		{
			PolicyStateTimer = tVCONNSourceOn;	// Enable the VCONNOnTimer and wait for a PS_RDY message
			PolicySubIndex++;	// Increment the subindex to move to waiting for a PS_RDY message
		} else		// Otherwise we need to start sourcing VCONN
		{
			if (blnCCPinIsCC1)	// If the CC pin is CC1...
				Registers.Switches.VCONN_CC2 = 1;	// Enable VCONN for CC2
			else	// Otherwise the CC pin is CC2
				Registers.Switches.VCONN_CC1 = 1;	// so enable VCONN on CC1
			DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the register setting to the device
			IsVCONNSource = TRUE;
			PolicyStateTimer = VbusTransitionTime;	// Allow time for the FPF2498 to enable...
			PolicySubIndex = 4;	// Skip the next state and move onto sending the PS_RDY message after the timer expires            }
		}
		break;
	case 3:
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = FALSE;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTPS_RDY:	// If we get the PS_RDY message...
					Registers.Switches.VCONN_CC1 = 0;	// Disable the VCONN source
					Registers.Switches.VCONN_CC2 = 0;	// Disable the VCONN source
					DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the register setting to the device
					IsVCONNSource = FALSE;
					PolicyState = peSourceReady;	// Move onto the Sink Ready state
					PolicySubIndex = 0;	// Clear the sub index
					PDTxStatus = txIdle;	// Clear the transmitter status
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the VCONNOnTimer times out...
		{
			PolicyState = peSourceSendHardReset;	// Issue a hard reset
			PolicySubIndex = 0;	// Clear the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
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
#ifdef FSC_HAVE_DRP
	FSC_U8 Status;
	switch (PolicySubIndex) {
	case 0:		// Send the PRSwap command
		if (PolicySendCommand(CMTPR_Swap, peSourceSendPRSwap, 1) == STAT_SUCCESS)	// Send the PR_Swap message and wait for the good CRC
			PolicyStateTimer = tSenderResponse;	// Once we receive the good CRC, set the sender response timer
		break;
	case 1:		// Require Accept message to move on or go back to ready state
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = FALSE;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTAccept:	// If we get the Accept message...
					IsPRSwap = TRUE;
					PolicyHasContract = FALSE;
					DetachThreshold = VBUS_MDAC_3P78;	// Default to 5V detach threshold
					platform_notify_pd_contract(FALSE);
					PolicyStateTimer = tSrcTransition;	// Start the sink transition timer
					PolicySubIndex++;	// Increment the subindex to move onto the next step
					break;
				case CMTWait:	// If we get either the reject or wait message...
				case CMTReject:
					PolicyState = peSourceReady;	// Go back to the source ready state
					PolicySubIndex = 0;	// Clear the sub index
					IsPRSwap = FALSE;
					PDTxStatus = txIdle;	// Clear the transmitter status
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the SenderResponseTimer times out...
		{
			PolicyState = peSourceReady;	// Go back to the source ready state
			PolicySubIndex = 0;	// Clear the sub index
			IsPRSwap = FALSE;
			PDTxStatus = txIdle;	// Clear the transmitter status
		}
		break;
	case 2:		// Wait for tSrcTransition and then turn off power (and Rd on/Rp off)
		if (!PolicyStateTimer) {
			platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);	// Disable VBUS output
			platform_set_vbus_discharge(TRUE);	// Enabled VBUS discharge path
			PolicySubIndex++;	// Increment the sub-index to move onto the next state
		}
		break;
	case 3:
		if (VbusVSafe0V()) {
			RoleSwapToAttachedSink();
			platform_set_vbus_discharge(FALSE);	// Disable VBUS discharge path
			PolicyIsSource = FALSE;
			Registers.Switches.POWERROLE = PolicyIsSource;
			DeviceWrite(regSwitches1, 1,
				    &Registers.Switches.byte[1]);
			PolicySubIndex++;
		}
		break;
	case 4:		// Allow time for the supply to fall and then send the PS_RDY message
		Status =
		    PolicySendCommandNoReset(CMTPS_RDY, peSourceSendPRSwap, 5);
		if (Status == STAT_SUCCESS)	// If we successfully sent the PS_RDY command and received the goodCRC
			PolicyStateTimer = tPSSourceOn;	// Start the PSSourceOn timer to allow time for the new supply to come up
		else if (Status == STAT_ERROR)
			PolicyState = peErrorRecovery;	// If we get an error, go to the error recovery state
		break;
	case 5:		// Wait to receive a PS_RDY message from the new DFP
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = FALSE;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTPS_RDY:	// If we get the PS_RDY message...
					PolicySubIndex++;	// Clear the sub index
					PolicyStateTimer = tGoodCRCDelay;	// Make sure GoodCRC has time to send
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the PSSourceOn times out...
		{
			PolicyState = peErrorRecovery;	// Go to the error recovery state
			PolicySubIndex = 0;	// Clear the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
		}
		break;
	default:
		if (PolicyStateTimer == 0) {
			PolicyState = peSinkStartup;	// Go to the sink startup state
			PolicySubIndex = 0;	// Clear the sub index
			PolicyStateTimer = 0;	// Make sure GoodCRC has time to send
		}
		break;
	}
#endif // FSC_HAVE_DRP
}

void PolicySourceEvaluatePRSwap(void)
{
#ifdef FSC_HAVE_DRP
	FSC_U8 Status;
	switch (PolicySubIndex) {
	case 0:		// Send either the Accept or Reject command
		if ((CapsSource[0].FPDOSupply.DualRolePower == FALSE) || (PartnerCaps.FPDOSink.DualRolePower == FALSE) ||	// Determine Accept/Reject based on DualRolePower bit in current PDO
		    ((CapsSource[0].FPDOSupply.ExternallyPowered == TRUE) &&	// Must also reject if we are externally powered and partner is not
		     (PartnerCaps.FPDOSink.SupplyType == pdoTypeFixed)
		     && (PartnerCaps.FPDOSink.ExternallyPowered == FALSE))) {
			PolicySendCommand(CMTReject, peSourceReady, 0);	// Send the reject if we are not a DRP
		} else {
			if (PolicySendCommand(CMTAccept, peSourceEvaluatePRSwap, 1) == STAT_SUCCESS)	// Send the Accept message and wait for the good CRC
			{
				IsPRSwap = TRUE;
				PolicyHasContract = FALSE;
				DetachThreshold = VBUS_MDAC_3P78;	// Default to 5V detach threshold
				platform_notify_pd_contract(FALSE);
				RoleSwapToAttachedSink();
				PolicyStateTimer = tSrcTransition;
			}
		}
		break;
	case 1:
		if (PolicyStateTimer == 0) {
			platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);	// Disable VBUS output
			platform_set_vbus_discharge(TRUE);	// Enabled VBUS discharge path
			PolicySubIndex++;
		}
		break;
	case 2:
		if (VbusVSafe0V())	// Allow time for the supply to fall and then send the PS_RDY message
		{
			PolicyStateTimer = tSrcTransition;	// Allow some extra time for VBUS to discharge
			PolicyIsSource = FALSE;
			Registers.Switches.POWERROLE = PolicyIsSource;
			DeviceWrite(regSwitches1, 1,
				    &Registers.Switches.byte[1]);
			PolicySubIndex++;
		}
		break;
	case 3:
		if (PolicyStateTimer == 0) {
			platform_set_vbus_discharge(FALSE);	// Disable VBUS discharge path
			PolicySubIndex++;
		}
		break;
	case 4:
		Status = PolicySendCommandNoReset(CMTPS_RDY, peSourceEvaluatePRSwap, 5);	// Send the PS_RDY message
		if (Status == STAT_SUCCESS)	// If we successfully sent the PS_RDY command and received the goodCRC
			PolicyStateTimer = tPSSourceOn;	// Start the PSSourceOn timer to allow time for the new supply to come up
		else if (Status == STAT_ERROR)
			PolicyState = peErrorRecovery;	// If we get an error, go to the error recovery state
		break;
	case 5:		// Wait to receive a PS_RDY message from the new DFP
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = FALSE;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTPS_RDY:	// If we get the PS_RDY message...
					PolicySubIndex++;	// Increment the sub index
					IsPRSwap = FALSE;
					PolicyStateTimer = tGoodCRCDelay;	// Make sure GoodCRC has time to send
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the PSSourceOn times out...
		{
			IsPRSwap = FALSE;
			PolicyState = peErrorRecovery;	// Go to the error recovery state
			PolicySubIndex = 0;	// Clear the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
		}
		break;
	default:
		if (PolicyStateTimer == 0) {
			PolicyState = peSinkStartup;	// Go to the sink startup state
			PolicySubIndex = 0;	// Clear the sub index
			PolicyStateTimer = 0;
		}
		break;
	}
#else
	PolicySendCommand(CMTReject, peSourceReady, 0);	// Send the reject if we are not a DRP
#endif // FSC_HAVE_DRP
}

void PolicySourceWaitNewCapabilities(void)	// TODO: DPM integration
{
#ifdef FSC_INTERRUPT_TRIGGERED
	if (loopCounter == 0) {
		g_Idle = TRUE;	// Wait for COMP or HARDRST or GCRCSENT
		Registers.Mask.M_COMP_CHNG = 0;
		DeviceWrite(regMask, 1, &Registers.Mask.byte);
		Registers.MaskAdv.M_HARDRST = 0;
		DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
		Registers.MaskAdv.M_GCRCSENT = 0;
		DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
		platform_enable_timer(FALSE);
	}
#endif // FSC_INTERRUPT_TRIGGERED
	switch (PolicySubIndex) {
	case 0:
		// Wait for Policy Manager to change source capabilities
		break;
	default:
		// Transition to peSourceSendCapabilities
		PolicyState = peSourceSendCaps;
		PolicySubIndex = 0;
		break;
	}
}
#endif // FSC_HAVE_SRC

void PolicySourceEvaluateVCONNSwap(void)
{
	switch (PolicySubIndex) {
	case 0:
		PolicySendCommand(CMTAccept, peSourceEvaluateVCONNSwap, 1);	// Send the Accept message and wait for the good CRC
		break;
	case 1:
		if (IsVCONNSource)	// If we are currently sourcing VCONN...
		{
			PolicyStateTimer = tVCONNSourceOn;	// Enable the VCONNOnTimer and wait for a PS_RDY message
			PolicySubIndex++;	// Increment the subindex to move to waiting for a PS_RDY message
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
			DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the register setting to the device
			IsVCONNSource = TRUE;
			PolicyStateTimer = VbusTransitionTime;	// Allow time for the FPF2498 to enable...
			PolicySubIndex = 3;	// Skip the next state and move onto sending the PS_RDY message after the timer expires            }
		}
		break;
	case 2:
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = FALSE;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTPS_RDY:	// If we get the PS_RDY message...
					Registers.Switches.VCONN_CC1 = 0;	// Disable the VCONN source
					Registers.Switches.VCONN_CC2 = 0;	// Disable the VCONN source
					Registers.Switches.PDWN1 = 1;	// Ensure the pull-down on CC1 is enabled
					Registers.Switches.PDWN2 = 1;	// Ensure the pull-down on CC2 is enabled
					DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the register setting to the device
					IsVCONNSource = FALSE;
					PolicyState = peSourceReady;	// Move onto the Sink Ready state
					PolicySubIndex = 0;	// Clear the sub index
					PDTxStatus = txIdle;	// Clear the transmitter status
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the VCONNOnTimer times out...
		{
			PolicyState = peSourceSendHardReset;	// Issue a hard reset
			PolicySubIndex = 0;	// Clear the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
		}
		break;
	default:
		if (!PolicyStateTimer) {
			PolicySendCommand(CMTPS_RDY, peSourceReady, 0);	// Send the Accept message and wait for the good CRC
		}
		break;
	}
}

// ############################## Sink States  ############################## //

#ifdef FSC_HAVE_SNK
void PolicySinkSendHardReset(void)
{
	IsHardReset = TRUE;
	PolicySendHardReset(peSinkTransitionDefault, 0);
}

void PolicySinkSoftReset(void)
{
	if (PolicySendCommand(CMTAccept, peSinkWaitCaps, 0) == STAT_SUCCESS)
		PolicyStateTimer = tSinkWaitCap;
}

void PolicySinkSendSoftReset(void)
{
	switch (PolicySubIndex) {
	case 0:
		if (PolicySendCommand(CMTSoftReset, peSinkSendSoftReset, 1) == STAT_SUCCESS)	// Send the soft reset command to the protocol layer
		{
			PolicyStateTimer = tSenderResponse;	// Start the sender response timer to wait for an accept message once successfully sent
		}
		break;
	default:
		if (ProtocolMsgRx)	// If we have received a message
		{
			ProtocolMsgRx = FALSE;	// Reset the message received flag since we've handled it here
			if ((PolicyRxHeader.NumDataObjects == 0) && (PolicyRxHeader.MessageType == CMTAccept))	// And it was the Accept...
			{
				PolicyState = peSinkWaitCaps;	// Go to the wait for capabilities state
				PolicyStateTimer = tSinkWaitCap;	// Set the state timer to tSinkWaitCap
			} else	// Otherwise it was a message that we didn't expect, so...
				PolicyState = peSinkSendHardReset;	// Go to the hard reset state
			PolicySubIndex = 0;	// Reset the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
		} else if (!PolicyStateTimer)	// If we didn't get a response to our request before timing out...
		{
			PolicyState = peSinkSendHardReset;	// Go to the hard reset state
			PolicySubIndex = 0;	// Reset the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
		}
		break;
	}
}

void PolicySinkTransitionDefault(void)
{
	switch (PolicySubIndex) {
	case 0:
		IsHardReset = TRUE;
		PolicyHasContract = FALSE;
		DetachThreshold = VBUS_MDAC_3P78;	// Default to 5V detach threshold
		platform_notify_pd_contract(FALSE);
		Registers.Switches.AUTO_CRC = 0;	// turn off Auto CRC
		DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);
		PolicyStateTimer = tPSHardResetMax + tSafe0V;	// Timeout wait for vSafe0V
		NoResponseTimer = tNoResponse;	// Initialize the no response timer
		if (PolicyIsDFP)	// Make sure data role is UFP
		{
			PolicyIsDFP = FALSE;	// Set the current data role
			Registers.Switches.DATAROLE = PolicyIsDFP;	// Update the data role
			DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);	// Commit the data role in the 302
		}
		if (IsVCONNSource)	// Disable VCONN if VCONN Source
		{
			Registers.Switches.VCONN_CC1 = 0;
			Registers.Switches.VCONN_CC2 = 0;
			DeviceWrite(regSwitches0, 1,
				    &Registers.Switches.byte[0]);
			IsVCONNSource = FALSE;
		}
		PolicySubIndex++;
		break;
	case 1:
		if (VbusVSafe0V()) {
			PolicySubIndex++;
			PolicyStateTimer = tSrcRecoverMax + tSrcTurnOn;	// Timeout wait for vSafe5V
		} else if (PolicyStateTimer == 0)	// Break out if we never see 0V
		{
			if (PolicyHasContract) {
				PolicyState = peErrorRecovery;
				PolicySubIndex = 0;
			} else {
				PolicyState = peSinkStartup;
				PolicySubIndex = 0;
				PolicyStateTimer = 0;
			}
		}
		break;
	case 2:
		if (isVBUSOverVoltage(VBUS_MDAC_4P20)) {
			PolicySubIndex++;
		} else if (PolicyStateTimer == 0)	// Break out if we never see 0V
		{
			if (PolicyHasContract) {
				PolicyState = peErrorRecovery;
				PolicySubIndex = 0;
			} else {
				PolicyState = peSinkStartup;
				PolicySubIndex = 0;
				PolicyStateTimer = 0;
			}
		}
		break;
	default:
		PolicyState = peSinkStartup;	// Go to the startup state
		PolicySubIndex = 0;	// Clear the sub index
		PolicyStateTimer = 0;
		PDTxStatus = txIdle;	// Reset the transmitter status
		break;
	}

}

void PolicySinkStartup(void)
{
#ifdef FSC_HAVE_VDM
	FSC_S32 i;
#endif // FSC_HAVE_VDM
	// Set masks for PD
	Registers.Mask.M_COLLISION = 0;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.M_RETRYFAIL = 0;
	Registers.MaskAdv.M_HARDSENT = 0;
	Registers.MaskAdv.M_TXSENT = 0;
	Registers.MaskAdv.M_HARDRST = 0;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 0;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);

	if (Registers.DeviceID.VERSION_ID == VERSION_302B) {
		if (Registers.Control.BIST_TMODE == 1) {
			Registers.Control.BIST_TMODE = 0;	// Disable auto-flush RxFIFO
			DeviceWrite(regControl3, 1, &Registers.Control.byte[3]);
		}
	} else {
		if (Registers.Control.RX_FLUSH == 1)	// Disable Rx flushing if it has been enabled
		{
			Registers.Control.RX_FLUSH = 0;
			DeviceWrite(regControl1, 1, &Registers.Control.byte[1]);
		}
	}

	USBPDContract.object = 0;	// Clear the USB PD contract (output power to 5V default)
	PartnerCaps.object = 0;	// Clear partner sink caps
	IsPRSwap = FALSE;
	PolicyIsSource = FALSE;	// Clear the flag to indicate that we are a sink (for PRSwaps)
	Registers.Switches.POWERROLE = PolicyIsSource;
	DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);
	Registers.Switches.AUTO_CRC = 1;	// turn on Auto CRC
	DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);
	ResetProtocolLayer(TRUE);	// Reset the protocol layer

	CapsCounter = 0;	// Clear the caps counter
	CollisionCounter = 0;	// Reset the collision counter
	PolicyStateTimer = 0;	// Reset the policy state timer
	PolicyState = peSinkDiscovery;	// Go to the sink discovery state
	PolicySubIndex = 0;	// Reset the sub index

#ifdef FSC_HAVE_VDM
	auto_mode_disc_tracker = 0;

	mode_entered = FALSE;

	core_svid_info.num_svids = 0;
	for (i = 0; i < MAX_NUM_SVIDS; i++) {
		core_svid_info.svids[i] = 0;
	}
#endif // FSC_HAVE_VDM

#ifdef FSC_HAVE_DP
	AutoModeEntryObjPos = -1;

	resetDp();
#endif // FSC_HAVE_DP
}

void PolicySinkDiscovery(void)
{

	IsHardReset = FALSE;
	PolicyState = peSinkWaitCaps;
	PolicySubIndex = 0;
	PolicyStateTimer = tTypeCSinkWaitCap;
}

void PolicySinkWaitCaps(void)
{
	if (ProtocolMsgRx)	// If we have received a message...
	{
		ProtocolMsgRx = FALSE;	// Reset the message ready flag since we're handling it here
		if ((PolicyRxHeader.NumDataObjects > 0) && (PolicyRxHeader.MessageType == DMTSourceCapabilities))	// Have we received a valid source cap message?
		{
			UpdateCapabilitiesRx(TRUE);	// Update the received capabilities
			PolicyState = peSinkEvaluateCaps;	// Set the evaluate source capabilities state
		} else if ((PolicyRxHeader.NumDataObjects == 0)
			   && (PolicyRxHeader.MessageType == CMTSoftReset)) {
			PolicyState = peSinkSoftReset;	// Go to the soft reset state
		}
		PolicySubIndex = 0;	// Reset the sub index
	} else if ((PolicyHasContract == TRUE) && (NoResponseTimer == 0)
		   && (HardResetCounter > nHardResetCount)) {
		PolicyState = peErrorRecovery;
		PolicySubIndex = 0;
	} else if ((PolicyStateTimer == 0)
		   && (HardResetCounter <= nHardResetCount)) {
		PolicyState = peSinkSendHardReset;
		PolicySubIndex = 0;
	} else if ((PolicyHasContract == FALSE) && (NoResponseTimer == 0)
		   && (HardResetCounter > nHardResetCount)) {
#ifdef FSC_INTERRUPT_TRIGGERED
		g_Idle = TRUE;	// Wait for VBUSOK or HARDRST or GCRCSENT
		platform_enable_timer(FALSE);
#endif // FSC_INTERRUPT_TRIGGERED
	}
}

void PolicySinkEvaluateCaps(void)
{
	// Due to latency with the PC and evaluating capabilities, we are always going to select the first one by default (5V default)
	// This will allow the software time to determine if they want to select one of the other capabilities (user selectable)
	// If we want to automatically show the selection of a different capabilities message, we need to build in the functionality here
	// The evaluate caps
	FSC_S32 i, reqPos;
	FSC_U32 objVoltage = 0;
	FSC_U32 objCurrent, objPower, MaxPower, SelVoltage, ReqCurrent;
	objCurrent = 0;
	NoResponseTimer = T_TIMER_DISABLE;	// Stop the no response timer
	HardResetCounter = 0;	// Reset the hard reset counter                                                        // Indicate Hard Reset is over
	SelVoltage = 0;
	MaxPower = 0;
	reqPos = 0;		// Select nothing in case there is an error...
	for (i = 0; i < CapsHeaderReceived.NumDataObjects; i++)	// Going to select the highest power object that we are compatible with
	{
		switch (CapsReceived[i].PDO.SupplyType) {
		case pdoTypeFixed:
			objVoltage = CapsReceived[i].FPDOSupply.Voltage;	// Get the output voltage of the fixed supply
			if (objVoltage > SinkRequestMaxVoltage)	// If the voltage is greater than our limit...
				continue;	// Set the power to zero to ignore the object
			else	// Otherwise...
			{
				objCurrent =
				    CapsReceived[i].FPDOSupply.MaxCurrent;
				objPower = objVoltage * objCurrent;	// Calculate the power for comparison
			}
			break;
		case pdoTypeVariable:
			objVoltage = CapsReceived[i].VPDO.MaxVoltage;	// Grab the maximum voltage of the variable supply
			if (objVoltage > SinkRequestMaxVoltage)	// If the max voltage is greater than our limit...
				continue;	// Set the power to zero to ignore the object
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
		if (objPower >= MaxPower)	// If the current object has power greater than or equal the previous objects
		{
			MaxPower = objPower;	// Store the objects power
			SelVoltage = objVoltage;	// Store the objects voltage (used for calculations)
			reqPos = i + 1;	// Store the position of the object
		}
	}
	if ((reqPos > 0) && (SelVoltage > 0)) {
		PartnerCaps.object = CapsReceived[0].object;
		SinkRequest.FVRDO.ObjectPosition = reqPos & 0x07;	// Set the object position selected
		SinkRequest.FVRDO.GiveBack = SinkGotoMinCompatible;	// Set whether we will respond to the GotoMin message
		SinkRequest.FVRDO.NoUSBSuspend = SinkUSBSuspendOperation;	// Set whether we want to continue pulling power during USB Suspend
		SinkRequest.FVRDO.USBCommCapable = SinkUSBCommCapable;	// Set whether USB communications is active
		ReqCurrent = SinkRequestOpPower / SelVoltage;	// Calculate the requested operating current
		SinkRequest.FVRDO.OpCurrent = (ReqCurrent & 0x3FF);	// Set the current based on the selected voltage (in 10mA units)
		ReqCurrent = SinkRequestMaxPower / SelVoltage;	// Calculate the requested maximum current
		SinkRequest.FVRDO.MinMaxCurrent = (ReqCurrent & 0x3FF);	// Set the min/max current based on the selected voltage (in 10mA units)
		if (SinkGotoMinCompatible)	// If the give back flag is set...
			SinkRequest.FVRDO.CapabilityMismatch = FALSE;	// There can't be a capabilities mismatch
		else		// Otherwise...
		{
			if (objCurrent < ReqCurrent)	// If the max power available is less than the max power requested...
			{
				SinkRequest.FVRDO.CapabilityMismatch = TRUE;	// flag the source that we need more power
				SinkRequest.FVRDO.MinMaxCurrent = objCurrent;	// Set operating power to max available
				SinkRequest.FVRDO.OpCurrent = objCurrent;
			} else	// Otherwise...
			{
				SinkRequest.FVRDO.CapabilityMismatch = FALSE;	// there is no mismatch in the capabilities
			}
		}
		PolicyState = peSinkSelectCapability;	// Go to the select capability state
		PolicySubIndex = 0;	// Reset the sub index
		PolicyStateTimer = tSenderResponse;	// Initialize the sender response timer
	} else {
		// For now, we are just going to go back to the wait state instead of sending a reject or reset (may change in future)
		PolicyState = peSinkWaitCaps;	// Go to the wait for capabilities state
		PolicyStateTimer = tTypeCSinkWaitCap;	// Set the state timer to tSinkWaitCap
	}
}

void PolicySinkSelectCapability(void)
{
	switch (PolicySubIndex) {
	case 0:
		if (PolicySendData
		    (DMTRequest, 1, &SinkRequest, peSinkSelectCapability, 1,
		     SOP_TYPE_SOP) == STAT_SUCCESS) {
			NoResponseTimer = tSenderResponse;	// If there is a good CRC start retry timer
		}
		break;
	case 1:
		if (ProtocolMsgRx) {
			ProtocolMsgRx = FALSE;	// Reset the message ready flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a control message...
			{
				switch (PolicyRxHeader.MessageType)	// Determine the message type
				{
				case CMTAccept:
					PolicyHasContract = TRUE;	// Set the flag to indicate that a contract is in place
					USBPDContract.object = SinkRequest.object;	// Set the actual contract that is in place
					PolicyStateTimer = tPSTransition;	// Set the transition timer here
					PolicyState = peSinkTransitionSink;	// Go to the transition state if the source accepted the request
					break;
				case CMTWait:
				case CMTReject:
					if (PolicyHasContract) {
						PolicyState = peSinkReady;	// Go to the sink ready state if the source rejects or has us wait
					} else {
						PolicyState = peSinkWaitCaps;	// If we didn't have a contract, go wait for new source caps
						HardResetCounter = nHardResetCount + 1;	// Make sure we don't send hard reset to prevent infinite loop
					}
					break;
				case CMTSoftReset:
					PolicyState = peSinkSoftReset;	// Go to the soft reset state if we received a reset command
					break;
				default:
					PolicyState = peSinkSendSoftReset;	// We are going to send a reset message for all other commands received
					break;
				}
			} else	// Otherwise we received a data message...
			{
				switch (PolicyRxHeader.MessageType) {
				case DMTSourceCapabilities:	// If we received a new source capabilities message
					UpdateCapabilitiesRx(TRUE);	// Update the received capabilities
					PolicyState = peSinkEvaluateCaps;	// Go to the evaluate caps state
					break;
				default:
					PolicyState = peSinkSendSoftReset;	// Send a soft reset to get back to a known state
					break;
				}
			}
			PolicySubIndex = 0;	// Reset the sub index
			PDTxStatus = txIdle;	// Reset the transmitter status
		} else if (PolicyStateTimer == 0)	// If the sender response timer times out...
		{
			PolicyState = peSinkSendHardReset;	// Go to the hard reset state
			PolicySubIndex = 0;	// Reset the sub index
			PDTxStatus = txIdle;	// Reset the transmitter status
		}
		break;
	}
}

void PolicySinkTransitionSink(void)
{
	if (ProtocolMsgRx) {
		ProtocolMsgRx = FALSE;	// Reset the message ready flag since we're handling it here
		if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a control message...
		{
			switch (PolicyRxHeader.MessageType)	// Determine the message type
			{
			case CMTPS_RDY:
				PolicyState = peSinkReady;	// Go to the ready state
				platform_notify_pd_contract(TRUE);
				break;
			case CMTSoftReset:
				PolicyState = peSinkSoftReset;	// Go to the soft reset state if we received a reset command
				break;
			default:
				PolicyState = peSinkSendSoftReset;	// We are going to send a reset message for all other commands received
				break;
			}
		} else		// Otherwise we received a data message...
		{
			switch (PolicyRxHeader.MessageType)	// Determine the message type
			{
			case DMTSourceCapabilities:	// If we received new source capabilities...
				UpdateCapabilitiesRx(TRUE);	// Update the source capabilities
				PolicyState = peSinkEvaluateCaps;	// And go to the evaluate capabilities state
				break;
			default:	// If we receieved an unexpected data message...
				PolicyState = peSinkSendSoftReset;	// Send a soft reset to get back to a known state
				break;
			}
		}
		PolicySubIndex = 0;	// Reset the sub index
		PDTxStatus = txIdle;	// Reset the transmitter status
	} else if (PolicyStateTimer == 0)	// If the PSTransitionTimer times out...
	{
		PolicyState = peSinkSendHardReset;	// Issue a hard reset
		PolicySubIndex = 0;	// Reset the sub index
		PDTxStatus = txIdle;	// Reset the transmitter status
	}
}

void PolicySinkReady(void)
{
	if (ProtocolMsgRx)	// Have we received a message from the source?
	{
		ProtocolMsgRx = FALSE;	// Reset the message received flag since we're handling it here
		if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
		{
			switch (PolicyRxHeader.MessageType)	// Determine which command was received
			{
			case CMTGotoMin:
				PolicyState = peSinkTransitionSink;	// Go to transitioning the sink power
				PolicyStateTimer = tPSTransition;	// Set the transition timer here
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTGetSinkCap:
				PolicyState = peSinkGiveSinkCap;	// Go to sending the sink caps state
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTGetSourceCap:
				PolicyState = peSinkGiveSourceCap;	// Go to sending the source caps if we are dual-role
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTDR_Swap:	// If we get a DR_Swap message...
				PolicyState = peSinkEvaluateDRSwap;	// Go evaluate whether we are going to accept or reject the swap
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTPR_Swap:
				PolicyState = peSinkEvaluatePRSwap;	// Go evaluate whether we are going to accept or reject the swap
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTVCONN_Swap:	// If we get a VCONN_Swap message...
				PolicyState = peSinkEvaluateVCONNSwap;	// Go evaluate whether we are going to accept or reject the swap
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTSoftReset:
				PolicyState = peSinkSoftReset;	// Go to the soft reset state
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			default:	// For all other commands received, simply ignore them
				PolicySubIndex = 0;	// Reset the sub index
				PDTxStatus = txIdle;	// Reset the transmitter status
				break;
			}
		} else {
			switch (PolicyRxHeader.MessageType) {
			case DMTSourceCapabilities:
				UpdateCapabilitiesRx(TRUE);	// Update the received capabilities
				PolicyState = peSinkEvaluateCaps;	// Go to the evaluate capabilities state
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
#ifdef FSC_HAVE_VDM
			case DMTVenderDefined:
				convertAndProcessVdmMessage(ProtocolMsgRxSop);
				break;
#endif // FSC_HAVE_VDM
			case DMTBIST:
				processDMTBIST();
				break;
			default:	// If we get something we are not expecting... simply ignore them
				PolicySubIndex = 0;	// Reset the sub index
				PDTxStatus = txIdle;	// Reset the transmitter status
				break;
			}
		}
	} else if (USBPDTxFlag)	// Has the device policy manager requested us to send a message?
	{
		if (PDTransmitHeader.NumDataObjects == 0) {
			switch (PDTransmitHeader.MessageType) {
			case CMTGetSourceCap:
				PolicyState = peSinkGetSourceCap;	// Go to retrieve the source caps state
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTGetSinkCap:
				PolicyState = peSinkGetSinkCap;	// Go to retrieve the sink caps state
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			case CMTDR_Swap:
				PolicyState = peSinkSendDRSwap;	// Issue a DR_Swap message
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
#ifdef FSC_HAVE_DRP
			case CMTPR_Swap:
				PolicyState = peSinkSendPRSwap;	// Issue a PR_Swap message
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
#endif // FSC_HAVE_DRP
			case CMTSoftReset:
				PolicyState = peSinkSendSoftReset;	// Go to the send soft reset state
				PolicySubIndex = 0;	// Clear the sub index
				PDTxStatus = txIdle;	// Clear the transmitter status
				break;
			default:
				break;
			}
		} else {
			switch (PDTransmitHeader.MessageType) {
			case DMTRequest:
				SinkRequest.object = PDTransmitObjects[0].object;	// Set the actual object to request
				PolicyState = peSinkSelectCapability;	// Go to the select capability state
				PolicySubIndex = 0;	// Reset the sub index
				PolicyStateTimer = tSenderResponse;	// Initialize the sender response timer
				break;
			case DMTVenderDefined:
#ifdef FSC_HAVE_VDM
				doVdmCommand();
#endif // FSC_HAVE_VDM
				break;
			default:
				break;
			}
		}
		USBPDTxFlag = FALSE;
	}
#ifdef FSC_HAVE_VDM
	else if (PolicyIsDFP && (AutoVdmState != AUTO_VDM_DONE)) {
		autoVdmDiscovery();
	}
#endif // FSC_HAVE_VDM
	else {
#ifdef FSC_INTERRUPT_TRIGGERED
		g_Idle = TRUE;	// Wait for VBUSOK or HARDRST or GCRCSENT
		platform_enable_timer(FALSE);
#endif // FSC_INTERRUPT_TRIGGERED
	}
}

void PolicySinkGiveSinkCap(void)
{
	PolicySendData(DMTSinkCapabilities, CapsHeaderSink.NumDataObjects,
		       &CapsSink[0], peSinkReady, 0, SOP_TYPE_SOP);
}

void PolicySinkGetSinkCap(void)
{
	PolicySendCommand(CMTGetSinkCap, peSinkReady, 0);
}

void PolicySinkGiveSourceCap(void)
{
#ifdef FSC_HAVE_DRP
	if (PortType == USBTypeC_DRP)
		PolicySendData(DMTSourceCapabilities,
			       CapsHeaderSource.NumDataObjects, &CapsSource[0],
			       peSinkReady, 0, SOP_TYPE_SOP);
	else
#endif // FSC_HAVE_DRP
		PolicySendCommand(CMTReject, peSinkReady, 0);	// Send the reject message and continue onto the ready state
}

void PolicySinkGetSourceCap(void)
{
	PolicySendCommand(CMTGetSourceCap, peSinkReady, 0);
}

void PolicySinkSendDRSwap(void)
{
	FSC_U8 Status;
	switch (PolicySubIndex) {
	case 0:
		Status = PolicySendCommandNoReset(CMTDR_Swap, peSinkSendDRSwap, 1);	// Send the DR_Swap command
		if (Status == STAT_SUCCESS)	// If we received a good CRC message...
			PolicyStateTimer = tSenderResponse;	// Initialize for SenderResponseTimer if we received the GoodCRC
		else if (Status == STAT_ERROR)	// If there was an error...
			PolicyState = peErrorRecovery;	// Go directly to the error recovery state
		break;
	default:
		if (ProtocolMsgRx) {
			ProtocolMsgRx = FALSE;	// Reset the message ready flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a control message...
			{
				switch (PolicyRxHeader.MessageType)	// Determine the message type
				{
				case CMTAccept:
					PolicyIsDFP = (PolicyIsDFP == TRUE) ? FALSE : TRUE;	// Flip the current data role

					Registers.Switches.DATAROLE = PolicyIsDFP;	// Update the data role
					DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);	// Commit the data role in the 302 for the auto CRC
					PolicyState = peSinkReady;	// Sink ready state
					break;
				case CMTSoftReset:
					PolicyState = peSinkSoftReset;	// Go to the soft reset state if we received a reset command
					break;
				default:	// If we receive any other command (including Reject & Wait), just go back to the ready state without changing
					PolicyState = peSinkReady;	// Go to the sink ready state
					break;
				}
			} else	// Otherwise we received a data message...
			{
				PolicyState = peSinkReady;	// Go to the sink ready state if we received a unexpected data message (ignoring message)
			}
			PolicySubIndex = 0;	// Reset the sub index
			PDTxStatus = txIdle;	// Reset the transmitter status
		} else if (PolicyStateTimer == 0)	// If the sender response timer times out...
		{
			PolicyState = peSinkReady;	// Go to the sink ready state if the SenderResponseTimer times out
			PolicySubIndex = 0;	// Reset the sub index
			PDTxStatus = txIdle;	// Reset the transmitter status
		}
		break;
	}
}

void PolicySinkEvaluateDRSwap(void)
{
	FSC_U8 Status;

#ifdef FSC_HAVE_VDM
	if (mode_entered == TRUE)	// If were are in modal operation, send a hard reset
	{
		PolicyState = peSinkSendHardReset;
		PolicySubIndex = 0;
		return;
	}
#endif // FSC_HAVE_VDM

	Status = PolicySendCommandNoReset(CMTAccept, peSinkReady, 0);	// Send the Accept message and wait for the good CRC
	if (Status == STAT_SUCCESS)	// If we received the good CRC...
	{
		PolicyIsDFP = (PolicyIsDFP == TRUE) ? FALSE : TRUE;	// We're not really doing anything except flipping the bit
		Registers.Switches.DATAROLE = PolicyIsDFP;	// Update the data role
		DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);	// Commit the data role in the 302 for the auto CRC
	} else if (Status == STAT_ERROR)	// If we didn't receive the good CRC...
	{
		PolicyState = peErrorRecovery;	// Go to the error recovery state
		PolicySubIndex = 0;	// Clear the sub-index
		PDTxStatus = txIdle;	// Clear the transmitter status
	}
}

void PolicySinkEvaluateVCONNSwap(void)
{
	switch (PolicySubIndex) {
	case 0:
		PolicySendCommand(CMTAccept, peSinkEvaluateVCONNSwap, 1);	// Send the Accept message and wait for the good CRC
		break;
	case 1:
		if (IsVCONNSource)	// If we are currently sourcing VCONN...
		{
			PolicyStateTimer = tVCONNSourceOn;	// Enable the VCONNOnTimer and wait for a PS_RDY message
			PolicySubIndex++;	// Increment the subindex to move to waiting for a PS_RDY message
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
			DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the register setting to the device
			IsVCONNSource = TRUE;
			PolicyStateTimer = VbusTransitionTime;	// Allow time for the FPF2498 to enable...
			PolicySubIndex = 3;	// Skip the next state and move onto sending the PS_RDY message after the timer expires            }
		}
		break;
	case 2:
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = FALSE;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTPS_RDY:	// If we get the PS_RDY message...
					Registers.Switches.VCONN_CC1 = 0;	// Disable the VCONN source
					Registers.Switches.VCONN_CC2 = 0;	// Disable the VCONN source
					Registers.Switches.PDWN1 = 1;	// Ensure the pull-down on CC1 is enabled
					Registers.Switches.PDWN2 = 1;	// Ensure the pull-down on CC2 is enabled
					DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the register setting to the device
					IsVCONNSource = FALSE;
					PolicyState = peSinkReady;	// Move onto the Sink Ready state
					PolicySubIndex = 0;	// Clear the sub index
					PDTxStatus = txIdle;	// Clear the transmitter status
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the VCONNOnTimer times out...
		{
			PolicyState = peSourceSendHardReset;	// Issue a hard reset
			PolicySubIndex = 0;	// Clear the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
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
#ifdef FSC_HAVE_DRP
	FSC_U8 Status;
	switch (PolicySubIndex) {
	case 0:		// Send the PRSwap command
		if (PolicySendCommand(CMTPR_Swap, peSinkSendPRSwap, 1) == STAT_SUCCESS)	// Send the PR_Swap message and wait for the good CRC
			PolicyStateTimer = tSenderResponse;	// Once we receive the good CRC, set the sender response timer
		break;
	case 1:		// Require Accept message to move on or go back to ready state
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = FALSE;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTAccept:	// If we get the Accept message...
					IsPRSwap = TRUE;
					PolicyHasContract = FALSE;
					DetachThreshold = VBUS_MDAC_3P78;	// Default to 5V detach threshold
					platform_notify_pd_contract(FALSE);
					PolicyStateTimer = tPSSourceOff;	// Start the sink transition timer
					PolicySubIndex++;	// Increment the subindex to move onto the next step
					break;
				case CMTWait:	// If we get either the reject or wait message...
				case CMTReject:
					PolicyState = peSinkReady;	// Go back to the sink ready state
					PolicySubIndex = 0;	// Clear the sub index
					IsPRSwap = FALSE;
					PDTxStatus = txIdle;	// Clear the transmitter status
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the SenderResponseTimer times out...
		{
			PolicyState = peSinkReady;	// Go back to the sink ready state
			PolicySubIndex = 0;	// Clear the sub index
			IsPRSwap = FALSE;
			PDTxStatus = txIdle;	// Clear the transmitter status
		}
		break;
	case 2:		// Wait for a PS_RDY message to be received to indicate that the original source is no longer supplying VBUS
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = FALSE;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTPS_RDY:	// If we get the PS_RDY message...
					RoleSwapToAttachedSource();	// Initiate the Type-C state machine for a power role swap
					PolicyIsSource = TRUE;
					Registers.Switches.POWERROLE =
					    PolicyIsSource;
					DeviceWrite(regSwitches1, 1,
						    &Registers.Switches.
						    byte[1]);
					PolicyStateTimer = 0;
					PolicySubIndex++;	// Increment the sub-index to move onto the next state
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the PSSourceOn times out...
		{
			PolicyState = peErrorRecovery;	// Go to the error recovery state
			PolicySubIndex = 0;	// Clear the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
		}
		break;
	default:		// Allow time for the supply to rise and then send the PS_RDY message
		if (isVBUSOverVoltage(VBUS_MDAC_4P20)) {
			Status = PolicySendCommandNoReset(CMTPS_RDY, peSourceStartup, 0);	// When we get the good CRC, we move onto the source startup state to complete the swap
			if (Status == STAT_ERROR)
				PolicyState = peErrorRecovery;	// If we get an error, go to the error recovery state
			SwapSourceStartTimer = tSwapSourceStart;
		}
		break;
	}
#endif // FSC_HAVE_DRP
}

void PolicySinkEvaluatePRSwap(void)
{
#ifdef FSC_HAVE_DRP
	FSC_U8 Status;
	switch (PolicySubIndex) {
	case 0:		// Send either the Accept or Reject command
		if ((PartnerCaps.FPDOSupply.SupplyType == pdoTypeFixed) && (PartnerCaps.FPDOSupply.DualRolePower == FALSE))	// Determine Accept/Reject based on partner's Dual Role Power
		{
			PolicySendCommand(CMTReject, peSinkReady, 0);	// Send the reject if we are not a DRP
		} else {
			if (PolicySendCommand(CMTAccept, peSinkEvaluatePRSwap, 1) == STAT_SUCCESS)	// Send the Accept message and wait for the good CRC
			{
				IsPRSwap = TRUE;
				PolicyHasContract = FALSE;
				DetachThreshold = VBUS_MDAC_3P78;	// Default to 5V detach threshold
				platform_notify_pd_contract(FALSE);
				PolicyStateTimer = tPSSourceOff;	// Start the sink transition timer
			}
		}
		break;
	case 1:		// Wait for the PS_RDY command to come in and indicate the source has turned off VBUS
		if (ProtocolMsgRx)	// Have we received a message from the source?
		{
			ProtocolMsgRx = FALSE;	// Reset the message received flag since we're handling it here
			if (PolicyRxHeader.NumDataObjects == 0)	// If we have received a command
			{
				switch (PolicyRxHeader.MessageType)	// Determine which command was received
				{
				case CMTPS_RDY:	// If we get the PS_RDY message...
					RoleSwapToAttachedSource();	// Initiate the Type-C state machine for a power role swap
					PolicyIsSource = TRUE;
					Registers.Switches.POWERROLE =
					    PolicyIsSource;
					DeviceWrite(regSwitches1, 1,
						    &Registers.Switches.
						    byte[1]);
					PolicyStateTimer = 0;	// Deprecated to use VBUS voltage
					PolicySubIndex++;	// Increment the sub-index to move onto the next state
					break;
				default:	// For all other commands received, simply ignore them
					break;
				}
			}
		} else if (!PolicyStateTimer)	// If the PSSourceOn times out...
		{
			PolicyState = peErrorRecovery;	// Go to the error recovery state
			PolicySubIndex = 0;	// Clear the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
		}
		break;
	default:		// Wait for VBUS to rise and then send the PS_RDY message
		if (isVBUSOverVoltage(VBUS_MDAC_4P20)) {
			Status = PolicySendCommandNoReset(CMTPS_RDY, peSourceStartup, 0);	// When we get the good CRC, we move onto the source startup state to complete the swap
			if (Status == STAT_ERROR)
				PolicyState = peErrorRecovery;	// If we get an error, go to the error recovery state
			IsPRSwap = FALSE;
			SwapSourceStartTimer = tSwapSourceStart;
		}
		break;
	}
#else
	PolicySendCommand(CMTReject, peSinkReady, 0);	// Send the reject if we are not a DRP
#endif // FSC_HAVE_DRP
}
#endif // FSC_HAVE_SNK

#ifdef FSC_HAVE_VDM

void PolicyGiveVdm(void)
{

	if (ProtocolMsgRx && PolicyRxHeader.MessageType == DMTVenderDefined)	// Have we received a VDM message
	{
		sendVdmMessageFailed();	// if we receive anything, kick out of here (interruptible)
		PolicySubIndex = 0;	// Reset the sub index
		PDTxStatus = txIdle;	// Reset the transmitter status
	} else if (sendingVdmData) {

		FSC_U8 result =
		    PolicySendData(DMTVenderDefined, vdm_msg_length,
				   vdm_msg_obj, vdm_next_ps, 0, SOP_TYPE_SOP);
		if (result == STAT_SUCCESS) {
			if (expectingVdmResponse()) {
				startVdmTimer(PolicyState);
			} else {
				resetPolicyState();
			}
			sendingVdmData = FALSE;
		} else if (result == STAT_ERROR) {
			sendVdmMessageFailed();
			sendingVdmData = FALSE;
		}
	} else {
		sendVdmMessageFailed();
	}

	if (VdmTimerStarted && (VdmTimer == 0)) {
		vdmMessageTimeout();
	}
}

void PolicyVdm(void)
{

	FSC_U8 result;

	if (ProtocolMsgRx)	// Have we received a message from the source?
	{
		ProtocolMsgRx = FALSE;	// Reset the message received flag since we're handling it here
		if (PolicyRxHeader.NumDataObjects != 0)	// If we have received a command
		{
			switch (PolicyRxHeader.MessageType) {
			case DMTVenderDefined:
				convertAndProcessVdmMessage(ProtocolMsgRxSop);
				break;
			default:	// If we get something we are not expecting... simply ignore them
				resetPolicyState();	// if not a VDM message, kick out of VDM state (interruptible)
				ProtocolMsgRx = TRUE;	// reset flag so other state can see the message and process
				break;
			}
		} else {
			resetPolicyState();	// if not a VDM message, kick out of VDM state (interruptible)
			ProtocolMsgRx = TRUE;	// reset flag so other state can see the message and process
		}
		PolicySubIndex = 0;	// Reset the sub index
		PDTxStatus = txIdle;	// Reset the transmitter status
	} else {
		if (sendingVdmData) {
			result =
			    PolicySendData(DMTVenderDefined, vdm_msg_length,
					   vdm_msg_obj, vdm_next_ps, 0,
					   SOP_TYPE_SOP);
			if (result == STAT_SUCCESS || result == STAT_ERROR) {
				sendingVdmData = FALSE;
			}
		}
	}

	if (VdmTimerStarted && (VdmTimer == 0)) {
		if (PolicyState == peDfpUfpVdmIdentityRequest) {
			AutoVdmState = AUTO_VDM_DONE;
		}
		vdmMessageTimeout();
	}
}

#endif // FSC_HAVE_VDM

void PolicyInvalidState(void)
{
	// reset if we get to an invalid state
	if (PolicyIsSource) {
		PolicyState = peSourceSendHardReset;
	} else {
		PolicyState = peSinkSendHardReset;
	}
}

// ########################## General PD Messaging ########################## //

FSC_BOOL PolicySendHardReset(PolicyState_t nextState, FSC_U32 delay)
{
	FSC_BOOL Success = FALSE;
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
			PolicySubIndex++;	// Move onto the next state
			Success = TRUE;
			break;
		default:	// None of the other states should actually occur, so...
			PDTxStatus = txReset;	// Set the transmitter status to resend a hard reset
			break;
		}
		break;
	default:
		if (PolicyStateTimer == 0)	// Once tPSHardReset has elapsed...
		{
			PolicyStateTimer = tPSHardReset - tHardResetOverhead;
			HardResetCounter++;	// Increment the hard reset counter once successfully sent
			PolicyState = nextState;	// Go to the state to transition to the default sink state
			PolicySubIndex = 0;	// Clear the sub index
			PDTxStatus = txIdle;	// Clear the transmitter status
		}
		break;
	}
	return Success;
}

FSC_U8 PolicySendCommand(FSC_U8 Command, PolicyState_t nextState,
			 FSC_U8 subIndex)
{
	FSC_U8 Status = STAT_BUSY;
	switch (PDTxStatus) {
	case txIdle:
		PolicyTxHeader.word = 0;	// Clear the word to initialize for each transaction
		PolicyTxHeader.NumDataObjects = 0;	// Clear the number of objects since this is a command
		PolicyTxHeader.MessageType = Command & 0x0F;	// Sets the message type to the command passed in
		PolicyTxHeader.PortDataRole = PolicyIsDFP;	// Set whether the port is acting as a DFP or UFP
		PolicyTxHeader.PortPowerRole = PolicyIsSource;	// Set whether the port is serving as a power source or sink
		PolicyTxHeader.SpecRevision = USBPDSPECREV;	// Set the spec revision
		PDTxStatus = txSend;	// Indicate to the Protocol layer that there is something to send
		break;
	case txSend:
	case txBusy:
	case txWait:
		// Waiting for GoodCRC or timeout of the protocol
		// May want to put in a second level timeout in case there's an issue with the protocol getting hung
		break;
	case txSuccess:
		PolicyState = nextState;	// Go to the next state requested
		PolicySubIndex = subIndex;
		PDTxStatus = txIdle;	// Reset the transmitter status
		Status = STAT_SUCCESS;
		break;
	case txError:		// Didn't receive a GoodCRC message...
		if (PolicyState == peSourceSendSoftReset)	// If as a source we already failed at sending a soft reset...
			PolicyState = peSourceSendHardReset;	// Move onto sending a hard reset (source)
		else if (PolicyState == peSinkSendSoftReset)	// If as a sink we already failed at sending a soft reset...
			PolicyState = peSinkSendHardReset;	// Move onto sending a hard reset (sink)
		else if (PolicyIsSource)	// Otherwise, if we are a source...
			PolicyState = peSourceSendSoftReset;	// Attempt to send a soft reset (source)
		else		// We are a sink, so...
			PolicyState = peSinkSendSoftReset;	// Attempt to send a soft reset (sink)
		PolicySubIndex = 0;	// Reset the sub index
		PDTxStatus = txIdle;	// Reset the transmitter status
		Status = STAT_ERROR;
		break;
	case txCollision:
		CollisionCounter++;	// Increment the collision counter
		if (CollisionCounter > nRetryCount)	// If we have already retried two times
		{
			if (PolicyIsSource)
				PolicyState = peSourceSendHardReset;	// Go to the source hard reset state
			else
				PolicyState = peSinkSendHardReset;	// Go to the sink hard reset state
			PolicySubIndex = 0;	// Reset the sub index
			PDTxStatus = txReset;	// Set the transmitter status to send a hard reset
			Status = STAT_ERROR;
		} else		// Otherwise we are going to try resending the soft reset
			PDTxStatus = txIdle;	// Clear the transmitter status for the next operation
		break;
	default:		// For an undefined case, reset everything (shouldn't get here)
		if (PolicyIsSource)
			PolicyState = peSourceSendHardReset;	// Go to the source hard reset state
		else
			PolicyState = peSinkSendHardReset;	// Go to the sink hard reset state
		PolicySubIndex = 0;	// Reset the sub index
		PDTxStatus = txReset;	// Set the transmitter status to send a hard reset
		Status = STAT_ERROR;
		break;
	}
	return Status;
}

FSC_U8 PolicySendCommandNoReset(FSC_U8 Command, PolicyState_t nextState,
				FSC_U8 subIndex)
{
	FSC_U8 Status = STAT_BUSY;
	switch (PDTxStatus) {
	case txIdle:
		PolicyTxHeader.word = 0;	// Clear the word to initialize for each transaction
		PolicyTxHeader.NumDataObjects = 0;	// Clear the number of objects since this is a command
		PolicyTxHeader.MessageType = Command & 0x0F;	// Sets the message type to the command passed in
		PolicyTxHeader.PortDataRole = PolicyIsDFP;	// Set whether the port is acting as a DFP or UFP
		PolicyTxHeader.PortPowerRole = PolicyIsSource;	// Set whether the port is serving as a power source or sink
		PolicyTxHeader.SpecRevision = USBPDSPECREV;	// Set the spec revision
		PDTxStatus = txSend;	// Indicate to the Protocol layer that there is something to send
		break;
	case txSend:
	case txBusy:
	case txWait:
		// Waiting for GoodCRC or timeout of the protocol
		// May want to put in a second level timeout in case there's an issue with the protocol getting hung
		break;
	case txSuccess:
		PolicyState = nextState;	// Go to the next state requested
		PolicySubIndex = subIndex;
		PDTxStatus = txIdle;	// Reset the transmitter status
		Status = STAT_SUCCESS;
		break;
	default:		// For all error cases (and undefined),
		PolicyState = peErrorRecovery;	// Go to the error recovery state
		PolicySubIndex = 0;	// Reset the sub index
		PDTxStatus = txReset;	// Set the transmitter status to send a hard reset
		Status = STAT_ERROR;
		break;
	}
	return Status;
}

FSC_U8 PolicySendData(FSC_U8 MessageType, FSC_U8 NumDataObjects,
		      doDataObject_t * DataObjects, PolicyState_t nextState,
		      FSC_U8 subIndex, SopType sop)
{
	FSC_U8 Status = STAT_BUSY;
	FSC_U32 i;
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
		PDTxStatus = txSend;	// Indicate to the Protocol layer that there is something to send
		break;
	case txSend:
	case txBusy:
	case txWait:
	case txCollision:
		// Waiting for GoodCRC or timeout of the protocol
		// May want to put in a second level timeout in case there's an issue with the protocol getting hung
		break;
	case txSuccess:
		PolicyState = nextState;	// Go to the next state requested
		PolicySubIndex = subIndex;
		PDTxStatus = txIdle;	// Reset the transmitter status
		Status = STAT_SUCCESS;
		break;
	case txError:		// Didn't receive a GoodCRC message...
		if (PolicyState == peSourceSendCaps)	// If we were in the send source caps state when the error occurred...
			PolicyState = peSourceDiscovery;	// Go to the discovery state
		else if (PolicyIsSource)	// Otherwise, if we are a source...
			PolicyState = peSourceSendSoftReset;	// Attempt to send a soft reset (source)
		else		// Otherwise...
			PolicyState = peSinkSendSoftReset;	// Go to the soft reset state
		PolicySubIndex = 0;	// Reset the sub index
		PDTxStatus = txIdle;	// Reset the transmitter status
		Status = STAT_ERROR;
		break;
	default:		// For undefined cases, reset everything
		if (PolicyIsSource)
			PolicyState = peSourceSendHardReset;	// Go to the source hard reset state
		else
			PolicyState = peSinkSendHardReset;	// Go to the sink hard reset state
		PolicySubIndex = 0;	// Reset the sub index
		PDTxStatus = txReset;	// Set the transmitter status to send a hard reset
		Status = STAT_ERROR;
		break;
	}
	return Status;
}

FSC_U8 PolicySendDataNoReset(FSC_U8 MessageType, FSC_U8 NumDataObjects,
			     doDataObject_t * DataObjects,
			     PolicyState_t nextState, FSC_U8 subIndex)
{
	FSC_U8 Status = STAT_BUSY;
	FSC_U32 i;
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
		PDTxStatus = txSend;	// Indicate to the Protocol layer that there is something to send
		break;
	case txSend:
	case txBusy:
	case txWait:
		// Waiting for GoodCRC or timeout of the protocol
		// May want to put in a second level timeout in case there's an issue with the protocol getting hung
		break;
	case txSuccess:
		PolicyState = nextState;	// Go to the next state requested
		PolicySubIndex = subIndex;
		PDTxStatus = txIdle;	// Reset the transmitter status
		Status = STAT_SUCCESS;
		break;
	default:		// For error cases (and undefined), ...
		PolicyState = peErrorRecovery;	// Go to the error recovery state
		PolicySubIndex = 0;	// Reset the sub index
		PDTxStatus = txReset;	// Set the transmitter status to send a hard reset
		Status = STAT_ERROR;
		break;
	}
	return Status;
}

void UpdateCapabilitiesRx(FSC_BOOL IsSourceCaps)
{
	FSC_U32 i;
#ifdef FSC_DEBUG
	SourceCapsUpdated = IsSourceCaps;	// Set the flag to indicate that the received capabilities are valid
#endif // FSC_DEBUG
	CapsHeaderReceived.word = PolicyRxHeader.word;	// Store the header for the latest capabilities received
	for (i = 0; i < CapsHeaderReceived.NumDataObjects; i++)	// Loop through all of the received data objects
		CapsReceived[i].object = PolicyRxDataObj[i].object;	// Store each capability
	for (i = CapsHeaderReceived.NumDataObjects; i < 7; i++)	// Loop through all of the invalid objects
		CapsReceived[i].object = 0;	// Clear each invalid object
	PartnerCaps.object = CapsReceived[0].object;
}

// ---------- BIST Receive Mode --------------------- //

void policyBISTReceiveMode(void)	// Not Implemented
{
	// Tell protocol layer to go to BIST Receive Mode
	// Go to BIST_Frame_Received if a test frame is received
	// Transition to SRC_Transition_to_Default, SNK_Transition_to_Default, or CBL_Ready when Hard_Reset received
}

void policyBISTFrameReceived(void)	// Not Implemented
{
	// Consume BIST Transmit Test Frame if received
	// Transition back to BIST_Frame_Received when a BIST Test Frame has been received
	// Transition to SRC_Transition_to_Default, SNK_Transition_to_Default, or CBL_Ready when Hard_Reset received
}

// ---------- BIST Carrier Mode and Eye Pattern ----- //

void policyBISTCarrierMode2(void)
{
	switch (PolicySubIndex) {
	default:
	case 0:
		Registers.Control.BIST_MODE2 = 1;	// Tell protocol layer to go to BIST_Carrier_Mode_2
		DeviceWrite(regControl1, 1, &Registers.Control.byte[1]);
		Registers.Control.TX_START = 1;	// Set the bit to enable the transmitter
		DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);	// Commit TX_START to the device
		Registers.Control.TX_START = 0;	// Clear this bit (write clear)
		PolicyStateTimer = tBISTContMode;	// Initialize and run BISTContModeTimer
		PolicySubIndex = 1;
		break;
	case 1:
		if (PolicyStateTimer == 0)	// Transition to SRC_Transition_to_Default, SNK_Transition_to_Default, or CBL_Ready when BISTContModeTimer times out
		{
			Registers.Control.BIST_MODE2 = 0;	// Disable BIST_Carrier_Mode_2 (PD_RESET does not do this)
			DeviceWrite(regControl1, 1, &Registers.Control.byte[1]);
			PolicyStateTimer = tGoodCRCDelay;	// Delay for >200us to allow preamble to finish
			PolicySubIndex++;
		}
		break;
	case 2:
		if (PolicyStateTimer == 0)	// Transition to SRC_Transition_to_Default, SNK_Transition_to_Default, or CBL_Ready when BISTContModeTimer times out
		{
			if (PolicyIsSource)	// If we are the source...
			{
#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
				PolicyState = peSourceSendHardReset;	// This will hard reset then transition to default
				PolicySubIndex = 0;
#endif // FSC_HAVE_SRC
			} else	// Otherwise we are the sink...
			{
#ifdef FSC_HAVE_SNK
				PolicyState = peSinkSendHardReset;	// This will hard reset then transition to default
				PolicySubIndex = 0;
#endif // FSC_HAVE_SNK
			}
		}
		break;
	}

}

void policyBISTTestData(void)
{
	if (Registers.DeviceID.VERSION_ID == VERSION_302B) {
		// Do Nothing
	} else {
		DeviceWrite(regControl1, 1, &Registers.Control.byte[1]);
	}
}

#ifdef FSC_HAVE_VDM

void InitializeVdmManager(void)
{
	initializeVdm();

	// configure callbacks
	vdmm.req_id_info = &vdmRequestIdentityInfo;
	vdmm.req_svid_info = &vdmRequestSvidInfo;
	vdmm.req_modes_info = &vdmRequestModesInfo;
	vdmm.enter_mode_result = &vdmEnterModeResult;
	vdmm.exit_mode_result = &vdmExitModeResult;
	vdmm.inform_id = &vdmInformIdentity;
	vdmm.inform_svids = &vdmInformSvids;
	vdmm.inform_modes = &vdmInformModes;
	vdmm.inform_attention = &vdmInformAttention;
	vdmm.req_mode_entry = &vdmModeEntryRequest;
	vdmm.req_mode_exit = &vdmModeExitRequest;
}

void convertAndProcessVdmMessage(SopType sop)
{
	FSC_U32 i;
	// form the word arrays that VDM block expects
	// note: may need to rethink the interface. but this is quicker to develop right now
	FSC_U32 vdm_arr[7];
	for (i = 0; i < PolicyRxHeader.NumDataObjects; i++) {
		vdm_arr[i] = 0;
		vdm_arr[i] = PolicyRxDataObj[i].object;
	}
	processVdmMessage(sop, vdm_arr, PolicyRxHeader.NumDataObjects);
}

void sendVdmMessage(SopType sop, FSC_U32 * arr, FSC_U32 length,
		    PolicyState_t next_ps)
{
	FSC_U32 i;
	// 'cast' to type that PolicySendData expects
	// didn't think this was necessary, but it fixed some problems. - Gabe
	vdm_msg_length = length;
	vdm_next_ps = next_ps;
	for (i = 0; i < vdm_msg_length; i++) {
		vdm_msg_obj[i].object = arr[i];
	}
	sendingVdmData = TRUE;
	ProtocolCheckRxBeforeTx = TRUE;
	VdmTimerStarted = FALSE;
	PolicyState = peGiveVdm;
}

void doVdmCommand(void)
{
	FSC_U32 command;
	FSC_U32 svid;
	FSC_U32 mode_index;
	SopType sop;

	command = PDTransmitObjects[0].byte[0] & 0x1F;
	svid = 0;
	svid |= (PDTransmitObjects[0].byte[3] << 8);
	svid |= (PDTransmitObjects[0].byte[2] << 0);

	mode_index = 0;
	mode_index = PDTransmitObjects[0].byte[1] & 0x7;

	// only SOP today
	sop = SOP_TYPE_SOP;

#ifdef FSC_HAVE_DP
	if (svid == DP_SID) {
		if (command == DP_COMMAND_STATUS) {
			requestDpStatus();
		} else if (command == DP_COMMAND_CONFIG) {
			DisplayPortConfig_t temp;
			temp.word = PDTransmitObjects[1].object;
			requestDpConfig(temp);
		}
	}
#endif // FSC_HAVE_DP

	if (command == DISCOVER_IDENTITY) {
		requestDiscoverIdentity(sop);
	} else if (command == DISCOVER_SVIDS) {
		requestDiscoverSvids(sop);
	} else if (command == DISCOVER_MODES) {
		requestDiscoverModes(sop, svid);
	} else if (command == ENTER_MODE) {
		requestEnterMode(sop, svid, mode_index);
	} else if (command == EXIT_MODE) {
		requestExitMode(sop, svid, mode_index);
	}

}

// this function assumes we're already in either Source or Sink Ready states!
void autoVdmDiscovery(void)
{
	if (PDTxStatus == txIdle) {	// wait for protocol layer to become idle
		switch (AutoVdmState) {
		case AUTO_VDM_INIT:
		case AUTO_VDM_DISCOVER_ID_PP:
			requestDiscoverIdentity(SOP_TYPE_SOP);
			AutoVdmState = AUTO_VDM_DISCOVER_SVIDS_PP;
			break;
		case AUTO_VDM_DISCOVER_SVIDS_PP:
			requestDiscoverSvids(SOP_TYPE_SOP);
			AutoVdmState = AUTO_VDM_DISCOVER_MODES_PP;
			break;
		case AUTO_VDM_DISCOVER_MODES_PP:
			if (auto_mode_disc_tracker == core_svid_info.num_svids) {
				AutoVdmState = AUTO_VDM_ENTER_MODE_PP;
				auto_mode_disc_tracker = 0;
			} else {
				requestDiscoverModes(SOP_TYPE_SOP,
						     core_svid_info.
						     svids
						     [auto_mode_disc_tracker]);
				auto_mode_disc_tracker++;
			}
			break;
		case AUTO_VDM_ENTER_MODE_PP:
			if (AutoModeEntryObjPos > 0) {
				requestEnterMode(SOP_TYPE_SOP, SVID_AUTO_ENTRY,
						 AutoModeEntryObjPos);
				AutoVdmState = AUTO_VDM_DP_GET_STATUS;
			} else {
				AutoVdmState = AUTO_VDM_DONE;
			}
			break;
		case AUTO_VDM_DP_GET_STATUS:
			if (DpModeEntered) {
				requestDpStatus();
			}
			AutoVdmState = AUTO_VDM_DONE;
			break;
		default:
			AutoVdmState = AUTO_VDM_DONE;
			break;
		}
	}
}

#endif // FSC_HAVE_VDM

// This function is FUSB302 specific
SopType TokenToSopType(FSC_U8 data)
{
	SopType ret;
	// figure out what SOP* the data came in on
	if ((data & 0b11100000) == 0b11100000) {
		ret = SOP_TYPE_SOP;
	} else if ((data & 0b11100000) == 0b11000000) {
		ret = SOP_TYPE_SOP1;
	} else if ((data & 0b11100000) == 0b10100000) {
		ret = SOP_TYPE_SOP2;
	} else if ((data & 0b11100000) == 0b10000000) {
		ret = SOP_TYPE_SOP1_DEBUG;
	} else if ((data & 0b11100000) == 0b01100000) {
		ret = SOP_TYPE_SOP2_DEBUG;
	} else {
		ret = SOP_TYPE_ERROR;
	}
	return ret;
}

void resetLocalHardware(void)
{
	FSC_U8 data = 0x20;
	DeviceWrite(regReset, 1, &data);	// Reset PD

	DeviceRead(regSwitches1, 1, &Registers.Switches.byte[1]);	// Re-read PD Registers
	DeviceRead(regSlice, 1, &Registers.Slice.byte);
	DeviceRead(regControl0, 1, &Registers.Control.byte[0]);
	DeviceRead(regControl1, 1, &Registers.Control.byte[1]);
	DeviceRead(regControl3, 1, &Registers.Control.byte[3]);
	DeviceRead(regMask, 1, &Registers.Mask.byte);
	DeviceRead(regMaska, 1, &Registers.MaskAdv.byte[0]);
	DeviceRead(regMaskb, 1, &Registers.MaskAdv.byte[1]);
	DeviceRead(regStatus0a, 2, &Registers.Status.byte[0]);
	DeviceRead(regStatus0, 2, &Registers.Status.byte[4]);
}

void processDMTBIST(void)
{
	FSC_U8 bdo = PolicyRxDataObj[0].byte[3] >> 4;

	switch (bdo) {
	case BDO_BIST_Carrier_Mode_2:
		if (CapsSource[USBPDContract.FVRDO.ObjectPosition - 1].FPDOSupply.Voltage == 100)	// Only enter BIST for 5V contract
		{
			PolicyState = PE_BIST_Carrier_Mode_2;
			PolicySubIndex = 0;
			ProtocolState = PRLIdle;
		}
		break;
	default:
	case BDO_BIST_Test_Data:
		if (CapsSource[USBPDContract.FVRDO.ObjectPosition - 1].FPDOSupply.Voltage == 100)	// Only enter BIST for 5V contract
		{
			if (Registers.DeviceID.VERSION_ID == VERSION_302B) {
				Registers.Control.BIST_TMODE = 1;	// Auto-flush RxFIFO
				DeviceWrite(regControl3, 1,
					    &Registers.Control.byte[3]);
			} else {
				Registers.Mask.byte = 0xFF;	// Mask for VBUS and Hard Reset
				Registers.Mask.M_VBUSOK = 0;
				Registers.Mask.M_COMP_CHNG = 0;
				DeviceWrite(regMask, 1, &Registers.Mask.byte);
				Registers.MaskAdv.byte[0] = 0xFF;
				Registers.MaskAdv.M_HARDRST = 0;
				DeviceWrite(regMaska, 1,
					    &Registers.MaskAdv.byte[0]);
				Registers.MaskAdv.M_GCRCSENT = 1;
				DeviceWrite(regMaskb, 1,
					    &Registers.MaskAdv.byte[1]);

				Registers.Control.RX_FLUSH = 1;	// Enable RxFIFO flushing
			}
			PolicyState = PE_BIST_Test_Data;
			ProtocolState = PRLDisabled;	// Disable Protocol layer so we don't read FIFO
		}
		break;
	}
}

#ifdef FSC_DEBUG
void SendUSBPDHardReset(void)
{
	if (PolicyIsSource)	// If we are the source...
		PolicyState = peSourceSendHardReset;	// set the source state to send a hard reset
	else			// Otherwise we are the sink...
		PolicyState = peSinkSendHardReset;	// so set the sink state to send a hard reset
	PolicySubIndex = 0;
	PDTxStatus = txIdle;	// Reset the transmitter status
}

#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void WriteSourceCapabilities(FSC_U8 * abytData)
{
	FSC_U32 i, j;
	sopMainHeader_t Header = { 0 };
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
			USBPDTxFlag = TRUE;	// Set the flag to send the new caps when appropriate...
			SourceCapsUpdated = TRUE;	// Set the flag to indicate to the software that the source caps were updated
		}
	}
}
#endif // FSC_HAVE_SRC

void ReadSourceCapabilities(FSC_U8 * abytData)
{
	FSC_U32 i, j;
	*abytData++ = CapsHeaderSource.byte[0];
	*abytData++ = CapsHeaderSource.byte[1];
	for (i = 0; i < CapsHeaderSource.NumDataObjects; i++) {
		for (j = 0; j < 4; j++)
			*abytData++ = CapsSource[i].byte[j];
	}
}

#ifdef FSC_HAVE_SNK
void WriteSinkCapabilities(FSC_U8 * abytData)
{
	FSC_U32 i, j;
	sopMainHeader_t Header = { 0 };
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

void WriteSinkRequestSettings(FSC_U8 * abytData)
{
	FSC_U32 uintPower;
	SinkGotoMinCompatible = *abytData & 0x01 ? TRUE : FALSE;
	SinkUSBSuspendOperation = *abytData & 0x02 ? TRUE : FALSE;
	SinkUSBCommCapable = *abytData++ & 0x04 ? TRUE : FALSE;
	SinkRequestMaxVoltage = (FSC_U32) * abytData++;
	SinkRequestMaxVoltage |= ((FSC_U32) (*abytData++) << 8);	// Voltage resolution is 50mV
	uintPower = (FSC_U32) * abytData++;
	uintPower |= ((FSC_U32) (*abytData++) << 8);
	uintPower |= ((FSC_U32) (*abytData++) << 16);
	uintPower |= ((FSC_U32) (*abytData++) << 24);
	SinkRequestOpPower = uintPower;	// Power resolution is 0.5mW
	uintPower = (FSC_U32) * abytData++;
	uintPower |= ((FSC_U32) (*abytData++) << 8);
	uintPower |= ((FSC_U32) (*abytData++) << 16);
	uintPower |= ((FSC_U32) (*abytData++) << 24);
	SinkRequestMaxPower = uintPower;	// Power resolution is 0.5mW
	// We could try resetting and re-evaluating the source caps here, but lets not do anything until requested by the user (soft reset or detach)
}

void ReadSinkRequestSettings(FSC_U8 * abytData)
{
	*abytData = SinkGotoMinCompatible ? 0x01 : 0;
	*abytData |= SinkUSBSuspendOperation ? 0x02 : 0;
	*abytData++ |= SinkUSBCommCapable ? 0x04 : 0;
	*abytData++ = (FSC_U8) (SinkRequestMaxVoltage & 0xFF);
	*abytData++ = (FSC_U8) ((SinkRequestMaxVoltage >> 8) & 0xFF);
	*abytData++ = (FSC_U8) (SinkRequestOpPower & 0xFF);
	*abytData++ = (FSC_U8) ((SinkRequestOpPower >> 8) & 0xFF);
	*abytData++ = (FSC_U8) ((SinkRequestOpPower >> 16) & 0xFF);
	*abytData++ = (FSC_U8) ((SinkRequestOpPower >> 24) & 0xFF);
	*abytData++ = (FSC_U8) (SinkRequestMaxPower & 0xFF);
	*abytData++ = (FSC_U8) ((SinkRequestMaxPower >> 8) & 0xFF);
	*abytData++ = (FSC_U8) ((SinkRequestMaxPower >> 16) & 0xFF);
	*abytData++ = (FSC_U8) ((SinkRequestMaxPower >> 24) & 0xFF);
}
#endif // FSC_HAVE_SNK

void ReadSinkCapabilities(FSC_U8 * abytData)
{
	FSC_U32 i, j;
	*abytData++ = CapsHeaderSink.byte[0];
	*abytData++ = CapsHeaderSink.byte[1];
	for (i = 0; i < CapsHeaderSink.NumDataObjects; i++) {
		for (j = 0; j < 4; j++)
			*abytData++ = CapsSink[i].byte[j];
	}
}

void EnableUSBPD(void)
{
	if (!USBPDEnabled)	// If we are not already enabled...
	{
		USBPDEnabled = TRUE;	// Set the USBPD state machine to enabled                                                               // return since we don't have to do anything
	}
}

void DisableUSBPD(void)
{
	if (USBPDEnabled)	// If we are already disabled...
	{
		USBPDEnabled = FALSE;	// Set the USBPD state machine to enabled
	}
}

FSC_BOOL GetPDStateLog(FSC_U8 * data)
{				// Loads log into byte array
	FSC_U32 i;
	FSC_U32 entries = PDStateLog.Count;
	FSC_U16 state_temp;
	FSC_U16 time_tms_temp;
	FSC_U16 time_s_temp;

	for (i = 0; ((i < entries) && (i < 12)); i++) {
		ReadStateLog(&PDStateLog, &state_temp, &time_tms_temp,
			     &time_s_temp);

		data[i * 5 + 1] = state_temp;
		data[i * 5 + 2] = (time_tms_temp >> 8);
		data[i * 5 + 3] = (FSC_U8) time_tms_temp;
		data[i * 5 + 4] = (time_s_temp) >> 8;
		data[i * 5 + 5] = (FSC_U8) time_s_temp;
	}

	data[0] = i;		// Send number of log packets

	return TRUE;
}

void ProcessReadPDStateLog(FSC_U8 * MsgBuffer, FSC_U8 * retBuffer)
{
	if (MsgBuffer[1] != 0) {
		retBuffer[1] = 0x01;	// Return that the version is not recognized
		return;
	}

	GetPDStateLog(&retBuffer[3]);	// Designed for 64 byte buffer
}

void ProcessPDBufferRead(FSC_U8 * MsgBuffer, FSC_U8 * retBuffer)
{
	if (MsgBuffer[1] != 0)
		retBuffer[1] = 0x01;	// Return that the version is not recognized
	else {
		retBuffer[4] = GetUSBPDBufferNumBytes();	// Return the total number of bytes in the buffer
		retBuffer[5] = ReadUSBPDBuffer((FSC_U8 *) & retBuffer[6], 58);	// Return the number of bytes read and return the data
	}
}

#endif // FSC_DEBUG

void SetVbusTransitionTime(FSC_U32 time_ms)
{
	VbusTransitionTime = time_ms * TICK_SCALE_TO_MS;
}
