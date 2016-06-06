/****************************************************************************
 * FileName:        TypeC.c
 * Processor:       PIC32MX250F128B
 * Compiler:        MPLAB XC32
 * Company:         Fairchild Semiconductor
 *
 * Author           Date          Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * M. Smith         12/04/2014    Initial Version
 *
 *
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

#include "TypeC.h"
#include "fusb30X.h"

#include "PDPolicy.h"

#ifdef FSC_HAVE_VDM
#include "vdm/vdm_config.h"
#endif // FSC_HAVE_VDM

#ifdef FSC_DEBUG
#include "Log.h"
#endif // FSC_DEBUG

/////////////////////////////////////////////////////////////////////////////
//      Variables accessible outside of the TypeC state machine
/////////////////////////////////////////////////////////////////////////////

DeviceReg_t Registers = { {0} };	// Variable holding the current status of the device registers

FSC_BOOL USBPDActive;		// Variable to indicate whether the USB PD state machine is active or not
FSC_BOOL USBPDEnabled;		// Variable to indicate whether USB PD is enabled (by the host)
SourceOrSink sourceOrSink;	// Are we currently a source or a sink?
FSC_BOOL g_Idle;		// Set to be woken by interrupt_n

USBTypeCPort PortType;		// Variable indicating which type of port we are implementing
FSC_BOOL blnCCPinIsCC1;		// Flag to indicate if the CC1 pin has been detected as the CC pin
FSC_BOOL blnCCPinIsCC2;		// Flag to indicate if the CC2 pin has been detected as the CC pin
FSC_BOOL blnSMEnabled = FALSE;	// Flag to indicate whether the TypeC state machine is enabled
ConnectionState ConnState;	// Variable indicating the current connection state
FSC_U8 TypeCSubState = 0;	// Substate to allow for non-blocking checks
FSC_U8 DetachThreshold;		// MDAC value for Type-C detach threshold

#ifdef FSC_DTS
FSC_BOOL DTSMode;		// Flag to indicate if we are in debug mode
#endif /* FSC_DTS */

#ifdef FSC_DEBUG
StateLog TypeCStateLog;		// Log for tracking state transitions and times
volatile FSC_U16 Timer_S;	// Tracks seconds elapsed for log timestamp
volatile FSC_U16 Timer_tms;	// Tracks tenths of milliseconds elapsed for log timestamp
#endif // FSC_DEBUG
/////////////////////////////////////////////////////////////////////////////
//      Variables accessible only inside TypeC state machine
/////////////////////////////////////////////////////////////////////////////

#ifdef FSC_HAVE_DRP
static FSC_BOOL blnSrcPreferred;	// Flag to indicate whether we prefer the Src role when in DRP
static FSC_BOOL blnSnkPreferred;	// Flag to indicate whether we prefer the Snk role when in DRP
#endif // FSC_HAVE_DRP

#ifdef FSC_HAVE_ACCMODE
FSC_BOOL blnAccSupport;		// Flag to indicate whether the port supports accessories
FSC_BOOL poweredAccSupport;
#endif // FSC_HAVE_ACCMODE

FSC_U16 StateTimer;		// Timer used to validate proceeding to next state
FSC_U16 PDDebounceTimer;	// Timer used for first level debouncing
FSC_U16 CCDebounceTimer;	// Timer used for second level debouncing
FSC_U16 PDFilterTimer;		// Timer used to ignore traffic less than tPDDebounce
CCTermType CCTermPrevious;	// Active CC1 termination value
CCTermType CCTermCCDebounce;	// Debounced CC1 termination value
CCTermType CCTermPDDebounce;
CCTermType CCTermPDDebouncePrevious;
CCTermType VCONNTerm;

USBTypeCCurrent SinkCurrent;	// Variable to indicate the current capability we have received
FSC_U8 loopCounter = 0;		// Used to count the number of Unattach<->AttachWait loops
static USBTypeCCurrent toggleCurrent;	// Current used for toggle state machine
USBTypeCCurrent SourceCurrent;	// Variable to indicate the current capability we are broadcasting

/////////////////////////////////////////////////////////////////////////////
// Tick at 100us
/////////////////////////////////////////////////////////////////////////////
void TypeCTick(void)
{
	if ((StateTimer < T_TIMER_DISABLE) && (StateTimer > 0))
		StateTimer--;
	if ((PDDebounceTimer < T_TIMER_DISABLE) && (PDDebounceTimer > 0))
		PDDebounceTimer--;
	if ((CCDebounceTimer < T_TIMER_DISABLE) && (CCDebounceTimer > 0))
		CCDebounceTimer--;
	if ((PDFilterTimer < T_TIMER_DISABLE) && (PDFilterTimer > 0))
		PDFilterTimer--;
}

/////////////////////////////////////////////////////////////////////////////
// Tick at 0.1ms
/////////////////////////////////////////////////////////////////////////////
#ifdef FSC_DEBUG
void LogTickAt100us(void)
{
	Timer_tms++;
	if (Timer_tms == 10000) {
		Timer_S++;
		Timer_tms = 0;
	}
}
#endif // FSC_DEBUG

// HEADER: Reads all the initial values from the 302
void InitializeRegisters(void)
{
	DeviceRead(regDeviceID, 1, &Registers.DeviceID.byte);
	DeviceRead(regSwitches0, 1, &Registers.Switches.byte[0]);
	DeviceRead(regSwitches1, 1, &Registers.Switches.byte[1]);
	DeviceRead(regMeasure, 1, &Registers.Measure.byte);
	DeviceRead(regSlice, 1, &Registers.Slice.byte);
	DeviceRead(regControl0, 1, &Registers.Control.byte[0]);
	DeviceRead(regControl1, 1, &Registers.Control.byte[1]);
	DeviceRead(regControl2, 1, &Registers.Control.byte[2]);
	DeviceRead(regControl3, 1, &Registers.Control.byte[3]);
	DeviceRead(regMask, 1, &Registers.Mask.byte);
	DeviceRead(regPower, 1, &Registers.Power.byte);
	DeviceRead(regReset, 1, &Registers.Reset.byte);
	DeviceRead(regOCPreg, 1, &Registers.OCPreg.byte);
	DeviceRead(regMaska, 1, &Registers.MaskAdv.byte[0]);
	DeviceRead(regMaskb, 1, &Registers.MaskAdv.byte[1]);
	DeviceRead(regStatus0a, 1, &Registers.Status.byte[0]);
	DeviceRead(regStatus1a, 1, &Registers.Status.byte[1]);
	DeviceRead(regInterrupta, 1, &Registers.Status.byte[2]);
	DeviceRead(regInterruptb, 1, &Registers.Status.byte[3]);
	DeviceRead(regStatus0, 1, &Registers.Status.byte[4]);
	DeviceRead(regStatus1, 1, &Registers.Status.byte[5]);
	DeviceRead(regInterrupt, 1, &Registers.Status.byte[6]);
}

/*******************************************************************************
 * Function:        InitializeTypeCVariables
 * Input:           None
 * Return:          None
 * Description:     Initializes the TypeC state machine variables
 ******************************************************************************/
void InitializeTypeCVariables(void)
{
	InitializeRegisters();	// Copy 302 registers to register struct

	Registers.Mask.byte = 0xFF;	// Mask all before global unmask
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0xFF;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 1;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);

	Registers.Control.INT_MASK = 0;	// Enable interrupt Pin
	DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);

	Registers.Control.TOG_RD_ONLY = 1;	// Do not stop toggle for Ra
	DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);

//    Registers.Switches.SPECREV = 0b10;                                          // Set Spec Rev to v3.0
//    DeviceWrite(regSwitches1, 1, &Registers.Switches.byte[1]);

	SourceCurrent = utcc1p5A;	// Set 1.5A host current
	updateSourceCurrent();

	blnSMEnabled = FALSE;	// Enable the TypeC state machine by default
	DetachThreshold = VBUS_MDAC_3P78;	// Default to 5V detach threshold

#ifdef FSC_DTS
	DTSMode = FALSE;
#endif /* FSC_DTS */

#ifdef FSC_HAVE_ACCMODE
	blnAccSupport = TRUE;	// Enable accessory support by default
	poweredAccSupport = TRUE;
	Registers.Control4.TOG_USRC_EXIT = 1;	// Enable Ra-Ra Toggle Exit
	DeviceWrite(regControl4, 1, &Registers.Control4.byte);
#endif // FSC_HAVE_ACCMODE

#ifdef FSC_HAVE_DRP
	blnSrcPreferred = FALSE;	// Clear the source preferred flag by default
	blnSnkPreferred = FALSE;	// Clear the sink preferred flag by default
#endif // FSC_HAVE_DRP

	g_Idle = FALSE;

#ifdef FSC_HAVE_DRP
	PortType = USBTypeC_DRP;
#elif FSC_HAVE_SRC
	PortType = USBTypeC_Source;
#elif FSC_HAVE_SNK
	PortType = USBTypeC_Sink;
#endif // FSC_HAVE_DRP / FSC_HAVE_SRC / FSC_HAVE_SNK

	ConnState = Disabled;	// Initialize to the disabled state    
	blnCCPinIsCC1 = FALSE;	// Clear the flag to indicate CC1 is CC
	blnCCPinIsCC2 = FALSE;	// Clear the flag to indicate CC2 is CC
	StateTimer = T_TIMER_DISABLE;	// Disable the state timer
	PDDebounceTimer = T_TIMER_DISABLE;	// Disable the 1st debounce timer
	CCDebounceTimer = T_TIMER_DISABLE;	// Disable the 2nd debounce timer
	resetDebounceVariables();	// Set all debounce variables to undefined

#ifdef FSC_HAVE_SNK
	SinkCurrent = utccNone;	// Clear the current advertisement initially
#endif // FSC_HAVE_SNK

	USBPDActive = FALSE;	// Clear the USB PD active flag
	USBPDEnabled = TRUE;	// Set the USB PD enabled flag until enabled by the host
	IsHardReset = FALSE;	// Initialize to no Hard Reset
	TypeCSubState = 0;	// Initialize substate to 0
	toggleCurrent = utccDefault;	// Initialise toggle current to default
}

void InitializeTypeC(void)
{
#ifdef FSC_DEBUG
	Timer_tms = 0;
	Timer_S = 0;
	InitializeStateLog(&TypeCStateLog);	// Initialize log
#endif // FSC_DEBUG

	SetStateDelayUnattached();
}

void DisableTypeCStateMachine(void)
{
	blnSMEnabled = FALSE;
}

void EnableTypeCStateMachine(void)
{
	blnSMEnabled = TRUE;

#ifdef FSC_DEBUG
	Timer_tms = 0;
	Timer_S = 0;
#endif // FSC_DEBUG
}

/*******************************************************************************
 * Function:        StateMachineTypeC
 * Input:           None
 * Return:          None
 * Description:     This is the state machine for the entire USB PD
 *                  This handles all the communication between the master and
 *                  slave.  This function is called by the Timer3 ISR on a
 *                  sub-interval of the 1/4 UI in order to meet timing
 *                  requirements.
 ******************************************************************************/
void StateMachineTypeC(void)
{
#ifdef  FSC_INTERRUPT_TRIGGERED
	do {
#endif // FSC_INTERRUPT_TRIGGERED
		if (!blnSMEnabled)
			return;

		if (platform_get_device_irq_state()) {
			DeviceRead(regInterrupta, 5, &Registers.Status.byte[2]);	// Read the interrupta, interruptb, status0, status1 and interrupt registers
		}

		if (USBPDActive)	// Only call the USB PD routines if we have enabled the block
		{

			USBPDProtocol();	// Call the protocol state machine to handle any timing critical operations

			USBPDPolicyEngine();	// Once we have handled any Type-C and protocol events, call the USB PD Policy Engine

		}

		switch (ConnState) {
		case Disabled:
			StateMachineDisabled();
			break;
		case ErrorRecovery:
			StateMachineErrorRecovery();
			break;
		case Unattached:
			StateMachineUnattached();
			break;
#ifdef FSC_HAVE_SNK
		case AttachWaitSink:
			StateMachineAttachWaitSink();
			break;
		case AttachedSink:
			StateMachineAttachedSink();
			break;
#ifdef FSC_HAVE_DRP
		case TryWaitSink:
			StateMachineTryWaitSink();
			break;
#endif // FSC_HAVE_DRP
#endif // FSC_HAVE_SNK
#if (defined(FSC_HAVE_DRP) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)))
		case TrySink:
			stateMachineTrySink();
			break;
#endif /* (defined(FSC_HAVE_DRP) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))) */
#ifdef FSC_HAVE_SRC
		case AttachWaitSource:
			StateMachineAttachWaitSource();
			break;
		case AttachedSource:
			StateMachineAttachedSource();
			break;
#ifdef FSC_HAVE_DRP
		case TryWaitSource:
			stateMachineTryWaitSource();
			break;
		case TrySource:
			StateMachineTrySource();
			break;
#endif // FSC_HAVE_DRP
		case UnattachedSource:
			stateMachineUnattachedSource();
			break;
#endif // FSC_HAVE_SRC
#ifdef FSC_HAVE_ACCMODE
		case AudioAccessory:
			StateMachineAudioAccessory();
			break;
		case DebugAccessorySource:
			StateMachineDebugAccessorySource();
			break;
#ifdef FSC_HAVE_SNK
		case DebugAccessorySink:
			StateMachineDebugAccessorySink();
			break;
#endif /* FSC_HAVE_SNK */
#endif // FSC_HAVE_ACCMODE
#if (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
		case AttachWaitAccessory:
			StateMachineAttachWaitAccessory();
			break;
		case UnsupportedAccessory:
			StateMachineUnsupportedAccessory();
			break;
		case PoweredAccessory:
			StateMachinePoweredAccessory();
			break;
#endif /* (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)) */
		case DelayUnattached:
			StateMachineDelayUnattached();
			break;
#ifdef FSC_DTS
		case AttachWaitDebSink:
			StateMachineAttachWaitDebugSink();
			break;
		case AttachedDebSink:
			StateMachineAttachedDebugSink();
			break;
		case AttachWaitDebSource:
			StateMachineAttachWaitDebugSource();
			break;
		case AttachedDebSource:
			StateMachineAttachedDebugSource();
			break;
		case TryDebSource:
			StateMachineTryDebugSource();
			break;
		case TryWaitDebSink:
			StateMachineTryWaitDebugSink();
			break;
		case UnattachedDebSource:
			StateMachineUnattachedDebugSource();
			break;
#endif /* FSC_DTS */
		default:
			SetStateDelayUnattached();	// We shouldn't get here, so go to the unattached state just in case
			break;
		}
		Registers.Status.Interrupt1 = 0;	// Clear the interrupt register once we've gone through the state machines
		Registers.Status.InterruptAdv = 0;	// Clear the advanced interrupt registers once we've gone through the state machines

#ifdef  FSC_INTERRUPT_TRIGGERED

	} while (g_Idle == FALSE);
#endif // FSC_INTERRUPT_TRIGGERED
}

void StateMachineDisabled(void)
{
	// Do nothing until directed to go to some other state...
}

void StateMachineErrorRecovery(void)
{
	if (StateTimer == 0) {
		SetStateDelayUnattached();
	}
}

void StateMachineDelayUnattached(void)
{
	if (StateTimer == 0) {
		SetStateUnattached();
	}
}

void StateMachineUnattached(void)
{

	if (Registers.Status.I_TOGDONE) {
		DeviceRead(regStatus1a, 1, &Registers.Status.byte[1]);	// Read TOGSS

		switch (Registers.Status.TOGSS) {
#ifdef FSC_HAVE_SNK
		case 0b101:	// Rp detected on CC1
			blnCCPinIsCC1 = TRUE;
			blnCCPinIsCC2 = FALSE;
#ifdef FSC_DTS
			if (DTSMode == TRUE) {
				SetStateAttachWaitDebugSink();
			} else
#endif /* FSC_DTS */
			{
				SetStateAttachWaitSink();	// Go to the AttachWaitSnk state
			}
			break;
		case 0b110:	// Rp detected on CC2
			blnCCPinIsCC1 = FALSE;
			blnCCPinIsCC2 = TRUE;
#ifdef FSC_DTS
			if (DTSMode == TRUE) {
				SetStateAttachWaitDebugSink();
			} else
#endif /* FSC_DTS */
			{
				SetStateAttachWaitSink();	// Go to the AttachWaitSnk state
			}
			break;
#endif // FSC_HAVE_SNK
#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
		case 0b001:	// Rd detected on CC1
			blnCCPinIsCC1 = TRUE;
			blnCCPinIsCC2 = FALSE;
#ifdef FSC_DTS
			if (DTSMode == TRUE) {
				SetStateAttachWaitDebugSource();
			} else
#endif /* FSC_DTS */
			{
#if defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)
				if ((PortType == USBTypeC_Sink) && (blnAccSupport))	// If we are configured as a sink and support accessories...
				{
					checkForAccessory();	// Go to the AttachWaitAcc state
				}
#endif // FSC_HAVE_SNK && FSC_HAVE_ACCMODE
#if defined(FSC_HAVE_SRC) && defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)
				else	// Otherwise we must be configured as a source or DRP
#endif // FSC_HAVE_SRC || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE)
#ifdef FSC_HAVE_SRC
				{
					SetStateAttachWaitSource();	// So go to the AttachWaitSrc state
				}
#endif // FSC_HAVE_SRC
			}
			break;
		case 0b010:	// Rd detected on CC2
			blnCCPinIsCC1 = FALSE;
			blnCCPinIsCC2 = TRUE;
#ifdef FSC_DTS
			if (DTSMode == TRUE) {
				SetStateAttachWaitDebugSource();
			} else
#endif /* FSC_DTS */
			{
#if defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)
				if ((PortType == USBTypeC_Sink) && (blnAccSupport))	// If we are configured as a sink and support accessories...
				{
					checkForAccessory();	// Go to the AttachWaitAcc state
				}
#endif // FSC_HAVE_SNK && FSC_HAVE_ACCMODE
#if defined(FSC_HAVE_SRC) && defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)
				else	// Otherwise we must be configured as a source or DRP
#endif // SRC + SNK + ACC
#ifdef FSC_HAVE_SRC
				{
					SetStateAttachWaitSource();	// So go to the AttachWaitSrc state
				}
#endif // FSC_HAVE_SRC
			}
			break;
		case 0b111:	// Ra detected on both CC1 and CC2
			blnCCPinIsCC1 = FALSE;
			blnCCPinIsCC2 = FALSE;
#ifdef FSC_DTS
			if (DTSMode == FALSE)
#endif /* FSC_DTS */
			{
#if defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)
				if ((PortType == USBTypeC_Sink) && (blnAccSupport))	// If we are configured as a sink and support accessories...
				{
					SetStateAttachWaitAccessory();	// Go to the AttachWaitAcc state
				}
#endif /* defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE) */
#if defined(FSC_HAVE_SRC) && defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)
				else	// Otherwise we must be configured as a source or DRP
#endif // SRC + SNK + ACC
#if defined(FSC_HAVE_SRC)
				if (PortType != USBTypeC_Sink) {
					SetStateAttachWaitSource();	// So go to the AttachWaitSource state
				}
#endif // FSC_HAVE_SRC
			}
			break;
#endif // FSC_HAVE_SRC
		default:	// Shouldn't get here, but just in case reset everything...
			Registers.Control.TOGGLE = 0;	// Disable the toggle in order to clear...
			DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);	// Commit the control state
			platform_delay_10us(1);
			Registers.Control.TOGGLE = 1;	// Re-enable the toggle state machine... (allows us to get another I_TOGDONE interrupt)
			DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);	// Commit the control state
			break;
		}
	}
#ifdef FPGA_BOARD
	rand();			// Desynchronise toggle state machine phases
#endif /* FPGA_BOARD */
}

#ifdef FSC_HAVE_SNK
void StateMachineAttachWaitSink(void)
{
	if ((VCONNTerm >= CCTypeRdUSB) && (VCONNTerm <= CCTypeRd3p0)
	    && (CCTermPrevious == CCTypeOpen)) {
		ToggleMeasure();
		updateVCONNSink();
	}
	debounceCC();

	if (CCTermCCDebounce != CCTypeUndefined)	// Check VCONN before attaching
	{
		updateVCONNSink();
	}

	if ((CCTermPDDebounce == CCTypeOpen) && (VCONNTerm == CCTypeOpen))	// If we have detected SNK.Open for atleast tPDDebounce on both pins...
	{
#ifdef FSC_HAVE_DRP
		if (PortType == USBTypeC_DRP) {
			SetStateUnattachedSource();	// Go to the unattached state
		} else
#endif // FSC_HAVE_DRP
		{
			SetStateDelayUnattached();
		}
	}
#ifdef FSC_HAVE_ACCMODE
	else if (blnAccSupport
		 && (CCTermCCDebounce >= CCTypeRdUSB)
		 && (CCTermCCDebounce < CCTypeUndefined)
		 && (VCONNTerm >= CCTypeRdUSB) && (VCONNTerm < CCTypeUndefined))	// If they are both Rd, it's a debug accessory
	{
		SetStateDebugAccessorySink();
	}
#endif /* FSC_HAVE_ACCMODE */
	else if (isVBUSOverVoltage(VBUS_MDAC_4P62))	// If we have detected VBUS and we have detected an Rp for >tCCDebounce...
	{
		if ((CCTermCCDebounce > CCTypeOpen)
		    && (CCTermCCDebounce < CCTypeUndefined)
		    && (VCONNTerm == CCTypeOpen))	// If exactly one CC is open
		{
#ifdef FSC_HAVE_DRP
			if ((PortType == USBTypeC_DRP) && blnSrcPreferred)	// If we are configured as a DRP and prefer the source role...
				SetStateTrySource();	// Go to the Try.Src state
			else	// Otherwise we are free to attach as a sink
#endif // FSC_HAVE_DRP
			{
				SetStateAttachedSink();	// Go to the Attached.Snk state               
			}
		}
	}
}
#endif // FSC_HAVE_SNK

#ifdef FSC_HAVE_SRC
void StateMachineAttachWaitSource(void)
{
	debounceCC();

	if (CCTermCCDebounce != CCTypeUndefined)	// Check VCONN before attaching
	{
		updateVCONNSource();
	}
#ifdef FSC_HAVE_ACCMODE
	if (blnAccSupport)	// If accessory support is enabled
	{
		if ((CCTermCCDebounce == CCTypeRa) && (VCONNTerm == CCTypeRa))	// If both pins are Ra, it's an audio accessory
			SetStateAudioAccessory();
		else if ((CCTermCCDebounce >= CCTypeRdUSB)
			 && (CCTermCCDebounce < CCTypeUndefined)
			 && (VCONNTerm >= CCTypeRdUSB)
			 && (VCONNTerm < CCTypeUndefined)
			 && VbusVSafe0V())	// If both pins are Rd, it's a debug accessory
		{
			SetStateDebugAccessorySource();
		}
	}
#endif // FSC_HAVE_ACCMODE

	if (((CCTermCCDebounce >= CCTypeRdUSB) && (CCTermCCDebounce < CCTypeUndefined)) && ((VCONNTerm == CCTypeOpen) || (VCONNTerm == CCTypeRa)))	// If CC1 is Rd and CC2 is not...
	{
		if (VbusVSafe0V()) {	// Check for VBUS to be at VSafe0V first...
#ifdef FSC_HAVE_DRP
			if (blnSnkPreferred) {
				SetStateTrySink();
			} else
#endif // FSC_HAVE_DRP
			{
				SetStateAttachedSource();	// Go to the Attached.Src state
			}	// Go to the Attached.Src state
		}
	} else if ((CCTermPrevious == CCTypeOpen) || (CCTermPrevious == CCTypeRa))	// If our debounced signal is open or Ra, go to the unattached state
		SetStateDelayUnattached();
}
#endif // FSC_HAVE_SRC

#ifdef FSC_HAVE_SNK
void StateMachineAttachedSink(void)
{
	if (Registers.Status.I_COMP_CHNG == 1) {
		if ((!IsPRSwap) && (IsHardReset == FALSE) && !isVBUSOverVoltage(DetachThreshold))	// If VBUS is removed and we are not in the middle of a power role swap...   
		{
			SetStateDelayUnattached();	// Go to the unattached state
		}
	}

	/* Can not check CC without generating extra interrupts */
	//debounceCC();
	//UpdateSinkCurrent();                                   // Update the advertised current
}
#endif // FSC_HAVE_SNK

#ifdef FSC_HAVE_SRC
void StateMachineAttachedSource(void)
{
	switch (TypeCSubState) {
	case 0:

		if (Registers.Status.I_COMP_CHNG == 1) {
			CCTermPrevious = DecodeCCTermination();
		}

		if ((CCTermPrevious == CCTypeOpen) && (!IsPRSwap))	// If the debounced CC pin is detected as open and we aren't in the middle of a PR_Swap
		{
#ifdef FSC_HAVE_DRP
			if ((PortType == USBTypeC_DRP) && blnSrcPreferred)	// Check to see if we need to go to the TryWait.SNK state...
				SetStateTryWaitSink();
			else	// Otherwise we are going to the unattached state
#endif // FSC_HAVE_DRP
			{
				platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);	// Disable the vbus outputs
				platform_notify_cc_orientation(NONE);
				USBPDDisable(TRUE);	// Disable the USB PD state machine
				Registers.Switches.byte[0] = 0x00;	// Disabled until vSafe0V
				DeviceWrite(regSwitches0, 1,
					    &Registers.Switches.byte[0]);
				TypeCSubState++;
				g_Idle = FALSE;
				platform_set_vbus_discharge(TRUE);	// Don't idle so we can reenter into the next substate
			}
		}
		if ((StateTimer == 0) && (loopCounter != 0))	// Reset loop counter and go Idle
		{
			loopCounter = 0;
#ifdef FSC_INTERRUPT_TRIGGERED
			if ((PolicyState == peSourceReady)
			    || (USBPDEnabled == FALSE)) {
				g_Idle = TRUE;	// Mask for COMP
				platform_enable_timer(FALSE);
			}
#endif // FSC_INTERRUPT_TRIGGERED
		}

		break;
	case 1:
		if (VbusVSafe0V()) {
			platform_set_vbus_discharge(FALSE);
			SetStateDelayUnattached();
		}
		break;
	default:
		SetStateErrorRecovery();
		break;
	}
}
#endif // FSC_HAVE_SRC

#ifdef FSC_HAVE_DRP
void StateMachineTryWaitSink(void)
{
	debounceCC();

	if (CCTermPDDebounce == CCTypeOpen)	// If tDRPTryWait has expired and we detected open
		SetStateDelayUnattached();	// Go to the unattached state
	else if (Registers.Status.VBUSOK)	// If we have detected VBUS and we have detected an Rp for >tCCDebounce...
	{
		if ((CCTermCCDebounce > CCTypeOpen) && (CCTermCCDebounce < CCTypeUndefined))	// If Rp is detected on CC1
		{
			SetStateAttachedSink();	// Go to the Attached.Snk state
		}
	}
}
#endif // FSC_HAVE_DRP

#ifdef FSC_HAVE_DRP
void StateMachineTrySource(void)
{
	debounceCC();

	if ((CCTermPDDebounce > CCTypeRa) && (CCTermPDDebounce < CCTypeUndefined) && ((VCONNTerm == CCTypeOpen) || (VCONNTerm == CCTypeRa)))	// If the CC1 pin is Rd for at least tPDDebounce
	{
		SetStateAttachedSource();	// Go to the Attached.Src state
	} else if ((StateTimer == 0) && (CCTermPDDebounce == CCTypeOpen))	// If we haven't detected Rd
	{
		SetStateTryWaitSink();	// Move onto the TryWait.Snk state to not get stuck in here
	}
}
#endif // FSC_HAVE_DRP

#ifdef FSC_HAVE_ACCMODE
void StateMachineDebugAccessorySource(void)
{
	debounceCC();

	if (CCTermPrevious == CCTypeOpen)	// If we have detected an open
	{
		SetStateDelayUnattached();
	} else
	    if (((CCTermPDDebounce >= CCTypeRdUSB)
		 && (CCTermPDDebounce < CCTypeUndefined))
		&& (VCONNTerm >= CCTypeRdUSB) && (VCONNTerm < CCTypeUndefined)) {
		if (CCTermPDDebounce > VCONNTerm) {
			USBPDEnable(TRUE, SOURCE);
		} else if (VCONNTerm > CCTermPDDebounce) {
			ToggleMeasure();
			USBPDEnable(TRUE, SOURCE);
		}
	}
#ifdef FSC_INTERRUPT_TRIGGERED
	if (StateTimer == 0) {
		g_Idle = TRUE;
		platform_enable_timer(FALSE);
	}
#endif
}

void StateMachineAudioAccessory(void)
{
	debounceCC();

	if (CCTermCCDebounce == CCTypeOpen)	// If we have detected an open for > tCCDebounce 
	{
		SetStateDelayUnattached();
	}
#ifdef FSC_INTERRUPT_TRIGGERED
//    if((CCTermPrevious == CCTypeOpen) && (g_Idle == TRUE))
//    {
//        g_Idle = FALSE;                                                             // Mask comp - debounce CC as open
//        Registers.Mask.M_COMP_CHNG = 1;
//        DeviceWrite(regMask, 1, &Registers.Mask.byte);
//        platform_enable_timer(TRUE);
//    }
//    if((CCTermPDDebounce == CCTypeRa) && (VCONNTerm == CCTypeRa))      // && because one Pin will always stay detected as Ra
//    {
//        g_Idle = TRUE;                                                          // Idle until COMP because CC timer has reset to Ra
//        Registers.Mask.M_COMP_CHNG = 0;
//        DeviceWrite(regMask, 1, &Registers.Mask.byte);
//        platform_enable_timer(FALSE);
//    }
#endif // FSC_INTERRUPT_TRIGGERED
}
#endif // FSC_HAVE_ACCMODE

#if (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void StateMachineAttachWaitAccessory(void)
{
	debounceCC();

	if ((CCTermCCDebounce == CCTypeRa) && (VCONNTerm == CCTypeRa))	// If they are both Ra, it's an audio accessory
	{
		SetStateAudioAccessory();
	} else if (CCTermPrevious == CCTypeOpen)	// If CC is open detach
	{
		SetStateDelayUnattached();
	} else if (poweredAccSupport && (CCTermCCDebounce >= CCTypeRdUSB)
		   && (CCTermCCDebounce < CCTypeUndefined)
		   && (VCONNTerm == CCTypeRa))	// If CC1 is Rd and CC2 is Ra, it's a powered accessory (CC1 is CC)
	{
		SetStatePoweredAccessory();
	} else if ((CCTermCCDebounce >= CCTypeRdUSB)
		   && (CCTermCCDebounce < CCTypeUndefined)
		   && (VCONNTerm == CCTypeOpen)) {
		SetStateTrySink();
	}
}

void StateMachinePoweredAccessory(void)
{
	debounceCC();

	if (CCTermPrevious == CCTypeOpen)	// Transition to Unattached.Snk when monitored CC pin is Open
	{
		SetStateDelayUnattached();
	}
#ifdef FSC_HAVE_VDM
	else if (mode_entered == TRUE) {
		StateTimer = T_TIMER_DISABLE;	// Disable tAMETimeout if we enter a mode
		loopCounter = 0;
#ifdef FSC_INTERRUPT_TRIGGERED
		if (PolicyState == peSourceReady) {
			g_Idle = TRUE;
			platform_enable_timer(FALSE);
			Registers.Mask.M_COMP_CHNG = 0;	// Unmask COMP register to detect detach
			DeviceWrite(regMask, 1, &Registers.Mask.byte);

		}
#endif //FSC_INTERRUPT_TRIGGERED
	}
#endif /* FSC_HAVE_VDM */
	else if (StateTimer == 0)	// If we have timed out (tAMETimeout) and haven't entered an alternate mode...
	{
		if (PolicyHasContract) {
			SetStateUnsupportedAccessory();	// Go to the Unsupported.Accessory state
		} else {
			SetStateTrySink();
		}
	}
}

void StateMachineUnsupportedAccessory(void)
{
	debounceCC();

	if (CCTermPrevious == CCTypeOpen)	// Transition to Unattached.Snk when monitored CC pin is Open
	{
		SetStateDelayUnattached();
	}
}
#endif /* (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE) */

#if (defined(FSC_HAVE_DRP) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)))
void stateMachineTrySink(void)
{
	switch (TypeCSubState) {
	case 0:
		if (StateTimer == 0) {
			StateTimer = tDRPTryWait;
			TypeCSubState++;
		}
		break;
	case 1:
		debounceCC();

		if (Registers.Status.VBUSOK) {
			if ((CCTermPDDebounce >= CCTypeRdUSB) && (CCTermPDDebounce < CCTypeUndefined))	// If the CC1 pin is Rd for atleast tPDDebounce...
			{
				SetStateAttachedSink();	// Go to the Attached.Src state
			}
		}
#ifdef FSC_HAVE_ACCMODE
		if ((StateTimer == 0) && (PortType == USBTypeC_Sink)) {
			SetStateUnsupportedAccessory();
		}
#endif /* FSC_HAVE_ACCMODE */
#ifdef FSC_HAVE_DRP
		if ((PortType == USBTypeC_DRP)
		    && (CCTermPDDebounce == CCTypeOpen)) {
			SetStateTryWaitSource();
		}
#endif /* FSC_HAVE_DRP */
		break;
	default:
		TypeCSubState = 0;
		break;
	}

}
#endif /* (defined(FSC_HAVE_DRP) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))) */

#ifdef FSC_HAVE_DRP
void stateMachineTryWaitSource(void)
{
	debounceCC();

	if (VbusVSafe0V()) {
		if (((CCTermPDDebounce >= CCTypeRdUSB) && (CCTermPDDebounce < CCTypeUndefined)) && ((VCONNTerm == CCTypeRa) || VCONNTerm == CCTypeOpen))	// If the CC1 pin is Rd for atleast tPDDebounce...
		{
			SetStateAttachedSource();	// Go to the Attached.Src state
		}
	}

	/* After tDRPTry transition to Unattached.SNK if Rd is not detected on exactly one CC pin */
	if (StateTimer == 0) {
		if (!
		    ((CCTermPrevious >= CCTypeRdUSB)
		     && (CCTermPrevious < CCTypeUndefined))
&& ((VCONNTerm == CCTypeRa) || VCONNTerm == CCTypeOpen))	// Not (Rd only on CC)
		{
			SetStateDelayUnattached();
		}
	}
}
#endif // FSC_HAVE_DRP

#ifdef FSC_HAVE_SRC
void stateMachineUnattachedSource(void)
{
	debounceCC();
	updateVCONNSource();

#ifdef FSC_HAVE_ACCMODE
	if (blnAccSupport && (CCTermPrevious == CCTypeRa)
	    && (VCONNTerm == CCTypeRa)) {
		SetStateAttachWaitSource();
	}
#endif /* FSC_HAVE_ACCMODE */

	if ((CCTermPrevious >= CCTypeRdUSB)
	    && (CCTermPrevious < CCTypeUndefined)
	    && ((VCONNTerm == CCTypeRa) || (VCONNTerm == CCTypeOpen)))	// If the CC1 pin is Rd for at least tPDDebounce
	{
		blnCCPinIsCC1 = TRUE;	// The CC pin is CC1
		blnCCPinIsCC2 = FALSE;
		SetStateAttachWaitSource();	// Go to the Attached.Src state
	} else if ((VCONNTerm >= CCTypeRdUSB) && (VCONNTerm < CCTypeUndefined)
		   && ((CCTermPrevious == CCTypeRa) || CCTermPrevious == CCTypeOpen))	// If the CC2 pin is Rd for atleast tPDDebounce...
	{
		blnCCPinIsCC1 = FALSE;	// The CC pin is CC2
		blnCCPinIsCC2 = TRUE;
		SetStateAttachWaitSource();	// Go to the Attached.Src state
	}

	if (StateTimer == 0) {
		SetStateDelayUnattached();
	}
}
#endif // FSC_HAVE_SRC

#if (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void StateMachineDebugAccessorySink(void)
{
	debounceCC();

	if (!isVBUSOverVoltage(DetachThreshold)) {
		SetStateDelayUnattached();
	} else
	    if (((CCTermPDDebounce >= CCTypeRdUSB)
		 && (CCTermPDDebounce < CCTypeUndefined))
		&& (VCONNTerm >= CCTypeRdUSB) && (VCONNTerm < CCTypeUndefined)) {
		if (CCTermPDDebounce > VCONNTerm) {
			USBPDEnable(TRUE, SINK);
		} else if (VCONNTerm > CCTermPDDebounce) {
			ToggleMeasure();
			USBPDEnable(TRUE, SINK);
		}
	}
#ifdef FSC_INTERRUPT_TRIGGERED
	if (StateTimer == 0) {
		g_Idle = TRUE;
		platform_enable_timer(FALSE);
	}
#endif
}
#endif /* (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)) */

#ifdef FSC_DTS
void StateMachineAttachWaitDebugSink(void)
{
	debounceCC();

	if ((CCTermPDDebounce == CCTypeOpen) || (VCONNTerm == CCTypeOpen)) {
#ifdef FSC_HAVE_DRP
		if (PortType == USBTypeC_DRP) {
			SetStateUnattachedDebugSource();
		} else
#endif // FSC_HAVE_DRP
		{
			SetStateDelayUnattached();
		}
	} else if (Registers.Status.VBUSOK) {
		if ((CCTermCCDebounce > CCTypeOpen)
		    && (CCTermCCDebounce < CCTypeUndefined)
		    && (VCONNTerm > CCTypeOpen)
		    && (VCONNTerm < CCTypeUndefined)) {
#ifdef FSC_HAVE_DRP
			if (PortType == USBTypeC_DRP)
				SetStateTryDebugSource();
			else
#endif // FSC_HAVE_DRP
			{
				SetStateAttachedDebugSink();
			}
		}
	}
}

void StateMachineAttachedDebugSink(void)
{
	debounceCC();

	if (!isVBUSOverVoltage(DetachThreshold)) {
		SetStateDelayUnattached();
	} else
	    if (((CCTermPDDebounce >= CCTypeRdUSB)
		 && (CCTermPDDebounce < CCTypeUndefined))
		&& (VCONNTerm >= CCTypeRdUSB) && (VCONNTerm < CCTypeUndefined)) {
		if (CCTermPDDebounce > VCONNTerm) {
			USBPDEnable(TRUE, SINK);
		} else if (VCONNTerm > CCTermPDDebounce) {
			ToggleMeasure();
			USBPDEnable(TRUE, SINK);
		}
	}
#ifdef FSC_INTERRUPT_TRIGGERED
	if (StateTimer == 0) {
		g_Idle = TRUE;
		platform_enable_timer(FALSE);
	}
#endif
}

void StateMachineAttachWaitDebugSource(void)
{
	debounceCC();

	if (((CCTermCCDebounce >= CCTypeRdUSB)
	     && (CCTermCCDebounce < CCTypeUndefined))
	    && (VCONNTerm >= CCTypeRdUSB) && (VCONNTerm < CCTypeUndefined)) {
		if (VbusVSafe0V()) {
			{
				SetStateAttachedDebugSource();
			}
		}
	} else if ((CCTermPrevious == CCTypeOpen) || (VCONNTerm == CCTypeOpen)) {
		SetStateDelayUnattached();
	}
}

void StateMachineAttachedDebugSource(void)
{
	debounceCC();

	if ((CCTermPrevious == CCTypeOpen) || (VCONNTerm == CCTypeOpen)) {
		SetStateDelayUnattached();
	} else
	    if (((CCTermPDDebounce >= CCTypeRdUSB)
		 && (CCTermPDDebounce < CCTypeUndefined))
		&& (VCONNTerm >= CCTypeRdUSB) && (VCONNTerm < CCTypeUndefined)) {
		if (CCTermPDDebounce > VCONNTerm) {
			USBPDEnable(TRUE, SOURCE);
		} else if (VCONNTerm > CCTermPDDebounce) {
			ToggleMeasure();
			USBPDEnable(TRUE, SOURCE);
		}
	}
#ifdef FSC_INTERRUPT_TRIGGERED
	if (StateTimer == 0) {
		g_Idle = TRUE;
		platform_enable_timer(FALSE);
	}
#endif
}

void StateMachineTryDebugSource(void)
{
	debounceCC();

	if (CCTermPDDebounce != CCTypeUndefined)	// Check VCONN before attaching
	{
		updateVCONNSource();
	}

	if ((CCTermPDDebounce > CCTypeRa)
	    && (CCTermPDDebounce < CCTypeUndefined)
	    && ((VCONNTerm > CCTypeRa) && (VCONNTerm < CCTypeUndefined))) {
		SetStateAttachedDebugSource();
	} else if ((StateTimer == 0) && (CCTermPrevious < CCTypeRdUSB)
		   && (VCONNTerm < CCTypeRdUSB)) {
		SetStateTryWaitDebugSink();
	}
}

void StateMachineTryWaitDebugSink(void)
{
	debounceCC();

	if ((CCTermPDDebounce == CCTypeOpen) || (VCONNTerm == CCTypeOpen)) {
		SetStateDelayUnattached();
	} else if (Registers.Status.VBUSOK) {
		if ((CCTermCCDebounce > CCTypeOpen)
		    && (CCTermCCDebounce < CCTypeUndefined)
		    && (VCONNTerm > CCTypeOpen)
		    && (VCONNTerm < CCTypeUndefined)) {
			SetStateAttachedDebugSink();
		}
	}
}

void StateMachineUnattachedDebugSource(void)
{
	debounceCC();
	updateVCONNSource();

	if ((CCTermPrevious >= CCTypeRdUSB)
	    && (CCTermPrevious < CCTypeUndefined)
	    && (VCONNTerm >= CCTypeRdUSB) && (VCONNTerm < CCTypeUndefined)) {
		SetStateAttachWaitDebugSource();

	}

	if (StateTimer == 0) {
		SetStateDelayUnattached();
	}
}
#endif /* FSC_DTS */

/////////////////////////////////////////////////////////////////////////////
//                      State Machine Configuration
/////////////////////////////////////////////////////////////////////////////
void SetStateDisabled(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;
	platform_enable_timer(TRUE);
#endif // FSC_INTERRUPT_TRIGGERED

	ConnState = Disabled;	// Set the state machine variable to Disabled
	StateTimer = T_TIMER_DISABLE;	// Disable the state timer (not used in this state)

	clearState();
}

void SetStateErrorRecovery(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;
	platform_enable_timer(TRUE);
#endif // FSC_INTERRUPT_TRIGGERED                                      // Clear the CC2 pin flag
	ConnState = ErrorRecovery;	// Set the state machine variable to ErrorRecovery
	StateTimer = tErrorRecovery;	// Load the tErrorRecovery duration into the state transition timer

	clearState();
}

void SetStateDelayUnattached(void)
{
#ifndef FPGA_BOARD
	SetStateUnattached();
	return;
#else
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;
	platform_enable_timer(TRUE);
#endif /* INTERRUPT */
	// This state is only here because of the precision timing source we have with the FPGA
	// We are trying to avoid having the toggle state machines in sync with each other
	// Causing the tDRPAdvert period to overlap causing the devices to not attach for a period of time
	ConnState = DelayUnattached;	// Set the state machine variable to delayed unattached
	StateTimer = rand() % 64;	// Set the state timer to a random value to not synchronize the toggle start (use a multiple of RAND_MAX+1 as the modulus operator)

#endif /* FPGA_BOARD */
}

void SetStateUnattached(void)
{

#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = TRUE;
	platform_enable_timer(FALSE);
#endif // FSC_INTERRUPT_TRIGGERED
	// This function configures the Toggle state machine in the device to handle all of the unattached states.
	// This allows for the MCU to be placed in a low power mode until the device wakes it up upon detecting something
	ConnState = Unattached;

	clearState();

	Registers.Measure.MEAS_VBUS = 0;
	DeviceWrite(regMeasure, 1, &Registers.Measure.byte);

	Registers.MaskAdv.M_TOGDONE = 0;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);

//    Registers.Switches.byte[0] = 0x03;                               // Enable the pull-downs on the CC pins
//    DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);     // Commit the switch state

	if (Registers.Control.HOST_CUR != toggleCurrent)	// Host current must be set to default for Toggle Functionality (or 1.5A for illegal cable)
	{
		Registers.Control.HOST_CUR = toggleCurrent;
		DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);
	}

	if (PortType == USBTypeC_DRP)	// If we are a DRP
		Registers.Control.MODE = 0b01;	// We need to enable the toggling functionality for Rp/Rd
#ifdef FSC_HAVE_ACCMODE
	else if ((PortType == USBTypeC_Sink) && (blnAccSupport))	// If we are a sink supporting accessories
		Registers.Control.MODE = 0b01;	// We need to enable the toggling functionality for Rp/Rd
#endif // FSC_HAVE_ACCMODE
	else if (PortType == USBTypeC_Source)	// If we are strictly a Source
		Registers.Control.MODE = 0b11;	// We just need to look for Rd
	else			// Otherwise we are a UFP
		Registers.Control.MODE = 0b10;	// So we need to only look for Rp

	Registers.Control.TOGGLE = 1;	// Enable the toggle
	platform_delay_10us(1);	// Delay before re-enabling toggle
	DeviceWrite(regControl0, 3, &Registers.Control.byte[0]);	// Commit the control state 

	StateTimer = T_TIMER_DISABLE;	// Disable the state timer, not used in this state
}

#ifdef FSC_HAVE_SNK
void SetStateAttachWaitSink(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;		// Mask all
	platform_enable_timer(TRUE);
#endif

	Registers.Control.TOGGLE = 0;	// Disable the toggle
	DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);	// Commit the toggle state

	ConnState = AttachWaitSink;	// Set the state machine variable to AttachWait.Snk
	setStateSink();
	blnCCPinIsCC1 = FALSE;	// Clear orientation to handle Rd-Rd case
	blnCCPinIsCC2 = FALSE;
	StateTimer = T_TIMER_DISABLE;
}

void SetStateDebugAccessorySink(void)
{
	loopCounter = 0;

	ConnState = DebugAccessorySink;
	setStateSink();

	StateTimer = tOrientedDebug;
}
#endif // FSC_HAVE_SNK

#ifdef FSC_HAVE_SRC
void SetStateAttachWaitSource(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;
	platform_enable_timer(TRUE);
#endif
	if (loopCounter++ > MAX_CABLE_LOOP)	// Swap toggle state machine current if looping
	{
		toggleCurrentSwap();
		loopCounter = 0;
	}

	platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);	// Disable the vbus outputs
	Registers.Control.TOGGLE = 0;	// Disable the toggle
	DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);	// Commit the toggle

	ConnState = AttachWaitSource;	// Set the state machine variable to AttachWait.Src

	setStateSource(FALSE);

	StateTimer = T_TIMER_DISABLE;	// Disable the state timer, not used in this state
}
#endif // FSC_HAVE_SRC

#ifdef FSC_HAVE_ACCMODE
void SetStateAttachWaitAccessory(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;		// Mask all
	Registers.MaskAdv.M_TOGDONE = 1;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	platform_enable_timer(TRUE);
#endif
	if (loopCounter++ > MAX_CABLE_LOOP)	// Swap toggle state machine current if looping
	{
		toggleCurrentSwap();
		loopCounter = 0;
	}

	platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);	// Disable the vbus outputs
	ConnState = AttachWaitAccessory;	// Set the state machine variable to AttachWait.Accessory

	Registers.Control.TOGGLE = 0;	// Disable the toggle
	DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);	// Commit the toggle

	setStateSource(FALSE);

	StateTimer = T_TIMER_DISABLE;	// Disable the state timer, not used in this state
}
#endif // FSC_HAVE_ACCMODE

#ifdef FSC_HAVE_SRC
void SetStateAttachedSource(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED

#endif // FSC_INTERRUPT_TRIGGERED
	Registers.Mask.M_COMP_CHNG = 0;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);

	platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE, TRUE);	// Enable only the 5V output
	ConnState = AttachedSource;	// Set the state machine variable to Attached.Src
	TypeCSubState = 0;

	setStateSource(TRUE);

	platform_notify_cc_orientation(blnCCPinIsCC2);

	USBPDEnable(TRUE, TRUE);	// Enable the USB PD state machine if applicable (no need to write to Device again), set as DFP
	StateTimer = tIllegalCable;	// Start dangling illegal cable timeout
}
#endif // FSC_HAVE_SRC

#ifdef FSC_HAVE_SNK
void SetStateAttachedSink(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = TRUE;
	platform_enable_timer(FALSE);
#endif

	Registers.Measure.MEAS_VBUS = 1;
	Registers.Measure.MDAC = DetachThreshold;
	Registers.Mask.M_COMP_CHNG = 0;
	/* TODO: PACK Register Map Because: 
	 * Large block writes are faster if there is a delay between I2C packets */
	DeviceWrite(regMeasure, 1, &Registers.Measure.byte);
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	loopCounter = 0;

	ConnState = AttachedSink;	// Set the state machine variable to Attached.Sink
	setStateSink();
	platform_notify_cc_orientation(blnCCPinIsCC2);

	USBPDEnable(TRUE, FALSE);	// Enable the USB PD state machine (no need to write Device again since we are doing it here)
	StateTimer = T_TIMER_DISABLE;	// Disable the state timer, not used in this state
}
#endif // FSC_HAVE_SNK

#ifdef FSC_HAVE_DRP
void RoleSwapToAttachedSink(void)
{
	ConnState = AttachedSink;	// Set the state machine variable to Attached.Sink   
	sourceOrSink = SINK;
	Registers.Measure.MEAS_VBUS = 1;
	Registers.Measure.MDAC = DetachThreshold;
	Registers.Mask.M_COMP_CHNG = 0;
	/* TODO: PACK Register Map Because: 
	 * Large block writes are faster if there is a delay between I2C packets */
	DeviceWrite(regMeasure, 1, &Registers.Measure.byte);
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	if (blnCCPinIsCC1)	// If the CC pin is CC1...
	{
		// Maintain VCONN
		Registers.Switches.PU_EN1 = 0;	// Disable the pull-up on CC1
		Registers.Switches.PDWN1 = 1;	// Enable the pull-down on CC1
	} else {
		// Maintain VCONN
		Registers.Switches.PU_EN2 = 0;	// Disable the pull-up on CC2
		Registers.Switches.PDWN2 = 1;	// Enable the pull-down on CC2                                   
	}
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the switch state
	SinkCurrent = utccNone;	// Set the current advertisment variable to none until we determine what the current is
	StateTimer = T_TIMER_DISABLE;	// Disable the state timer, not used in this state
	PDDebounceTimer = tPDDebounce;	// Set the debounce timer to tPDDebounceMin for detecting changes in advertised current
	CCDebounceTimer = tCCDebounce;	// Disable the 2nd level debounce timer, not used in this state                                      
	PDFilterTimer = T_TIMER_DISABLE;	// Disable PD filter timer
#ifdef FSC_DEBUG
	WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif // FSC_DEBUG
}
#endif // FSC_HAVE_DRP

#ifdef FSC_HAVE_DRP
void RoleSwapToAttachedSource(void)
{
	Registers.Measure.MEAS_VBUS = 0;
	DeviceWrite(regMeasure, 1, &Registers.Measure.byte);
	platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE, TRUE);	// Enable only the 5V output
	ConnState = AttachedSource;	// Set the state machine variable to Attached.Src
	TypeCSubState = 0;
	sourceOrSink = SOURCE;
	resetDebounceVariables();
	updateSourceMDACHigh();	// Set MDAC
	if (blnCCPinIsCC1)	// If the CC pin is CC1...
	{
		Registers.Switches.PU_EN1 = 1;	// Enable the pull-up on CC1
		Registers.Switches.PDWN1 = 0;	// Disable the pull-down on CC1
	} else {
		Registers.Switches.PU_EN2 = 1;	// Enable the pull-up on CC2
		Registers.Switches.PDWN2 = 0;	// Disable the pull-down on CC2
	}
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the switch state
	Registers.Mask.M_COMP_CHNG = 0;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	SinkCurrent = utccNone;	// Set the Sink current to none (not used in Src)
	StateTimer = T_TIMER_DISABLE;	// Disable the state timer, not used in this state
	PDDebounceTimer = tPDDebounce;	// Set the debounce timer to tPDDebounceMin for detecting a detach
	CCDebounceTimer = tCCDebounce;	// Disable the 2nd level debouncing, not needed in this state                                      // Disable the toggle timer, not used in this state
	PDFilterTimer = T_TIMER_DISABLE;	// Disable PD filter timer
#ifdef FSC_DEBUG
	WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif // FSC_DEBUG
}
#endif // FSC_HAVE_DRP

#ifdef FSC_HAVE_DRP
void SetStateTryWaitSink(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;		// Mask all
	Registers.Mask.byte = 0xFF;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0xFF;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 1;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
	platform_enable_timer(TRUE);
#endif
	USBPDDisable(TRUE);

	ConnState = TryWaitSink;	// Set the state machine variable to TryWait.Snk

	setStateSink();

	StateTimer = T_TIMER_DISABLE;	// Set the state timer to disabled
}
#endif // FSC_HAVE_DRP

#ifdef FSC_HAVE_DRP
void SetStateTrySource(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;
	platform_enable_timer(TRUE);
#endif
	platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);	// Disable the vbus outputs
	ConnState = TrySource;	// Set the state machine variable to Try.Src

	setStateSource(FALSE);

	StateTimer = tDRPTry;	// Set the state timer to disabled

}
#endif // FSC_HAVE_DRP

#if (defined(FSC_HAVE_DRP) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)))
void SetStateTrySink(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;
	platform_enable_timer(TRUE);
#endif

	ConnState = TrySink;	// Set the state machine variable to Try.Snk
	TypeCSubState = 0;
	USBPDDisable(TRUE);
	setStateSink();

	StateTimer = tDRPTry;
}
#endif /* (defined(FSC_HAVE_DRP) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))) */

#ifdef FSC_HAVE_DRP
void SetStateTryWaitSource(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;
	platform_enable_timer(TRUE);
#endif
	platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);	// Disable the vbus outputs

	ConnState = TryWaitSource;	// Set the state machine variable to AttachWait.Src

	setStateSource(FALSE);

	StateTimer = tDRPTry;	// Disable the state timer, not used in this state
}
#endif // FSC_HAVE_DRP

#ifdef FSC_HAVE_ACCMODE
void SetStateDebugAccessorySource(void)
{				/* FUSB302(B) does not support Debug Source orientation */
	Registers.Mask.M_COMP_CHNG = 0;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);

	loopCounter = 0;

	platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE, TRUE);	// Enable only the 5V output
	ConnState = DebugAccessorySource;	// Set the state machine variable to Debug.Accessory

	setStateSource(FALSE);

	StateTimer = tOrientedDebug;	// Disable the state timer, not used in this state
}

void SetStateAudioAccessory(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;
	platform_enable_timer(TRUE);
#endif
	loopCounter = 0;

	platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);	// Disable the vbus outputs
	ConnState = AudioAccessory;	// Set the state machine variable to Audio.Accessory

	setStateSource(FALSE);

	StateTimer = T_TIMER_DISABLE;	// Disable the state timer, not used in this state
}
#endif /* FSC_HAVE_ACCMODE */

#if (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void SetStatePoweredAccessory(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;
	platform_enable_timer(TRUE);
#endif

	platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);	// Disable the vbus outputs
	//platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE, TRUE);                      // Enable the 5V output for debugging
	ConnState = PoweredAccessory;	// Set the state machine variable to powered.accessory
	setStateSource(TRUE);
	if (SourceCurrent == utccDefault)	// If the current is default set it to 1.5A advert (Must be 1.5 or 3.0)
	{
		Registers.Control.HOST_CUR = 0b10;
		DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);
	}

	platform_notify_cc_orientation(blnCCPinIsCC2);

	USBPDEnable(TRUE, TRUE);

	StateTimer = tAMETimeout;	//T_TIMER_DISABLE; //tAMETimeout;                 // Set the state timer to tAMETimeout (need to enter alternate mode by this time)
}

void SetStateUnsupportedAccessory(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = TRUE;		// Mask for COMP
	Registers.Mask.M_COMP_CHNG = 0;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	platform_enable_timer(FALSE);
#endif
	platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);	// Disable the vbus outputs
	ConnState = UnsupportedAccessory;	// Set the state machine variable to unsupported.accessory
	setStateSource(FALSE);
	Registers.Control.HOST_CUR = 0b01;	// Must advertise default current
	DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);
	USBPDDisable(TRUE);

	StateTimer = T_TIMER_DISABLE;	// Disable the state timer, not used in this state

	platform_notify_unsupported_accessory();
}
#endif /* (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)) */

#ifdef FSC_HAVE_SRC
void SetStateUnattachedSource(void)	// Currently only implemented for transitioning from AttachWaitSnk to Unattached for DRP
{

#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;
	platform_enable_timer(TRUE);
#endif
	platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);	// Disable the vbus outputs
	ConnState = UnattachedSource;	// Set the state machine variable to unattached
	blnCCPinIsCC1 = FALSE;	// Clear the CC1 pin flag 
	blnCCPinIsCC2 = FALSE;	// Clear the CC2 pin flag

	setStateSource(FALSE);

	USBPDDisable(TRUE);	// Disable the USB PD state machine (no need to write Device again since we are doing it here)

	StateTimer = tTOG2;	// Disable the state timer, not used in this state
}
#endif // FSC_HAVE_SRC

#ifdef FSC_DTS
void SetStateAttachWaitDebugSink(void)
{
	Registers.Mask.M_VBUSOK = 0;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.Control.TOGGLE = 0;	// Disable the toggle
	DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);	// Commit the toggle state

	ConnState = AttachWaitDebSink;
	setStateSink();

	StateTimer = T_TIMER_DISABLE;
}

void SetStateAttachedDebugSink(void)
{
	loopCounter = 0;

	ConnState = AttachedDebSink;
	setStateSink();

	StateTimer = tOrientedDebug;
}

void SetStateAttachWaitDebugSource(void)
{
	Registers.Mask.M_VBUSOK = 0;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.Control.TOGGLE = 0;	// Disable the toggle
	DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);	// Commit the toggle state

	ConnState = AttachWaitDebSource;
	setStateSource(FALSE);

	StateTimer = T_TIMER_DISABLE;
}

void SetStateAttachedDebugSource(void)
{
	loopCounter = 0;
	platform_set_vbus_lvl_enable(VBUS_LVL_5V, TRUE, TRUE);
	ConnState = AttachedDebSource;
	setStateSource(FALSE);

	StateTimer = tOrientedDebug;
}

void SetStateTryDebugSource(void)
{
	ConnState = TryDebSource;
	setStateSource(FALSE);

	StateTimer = tDRPTry;
}

void SetStateTryWaitDebugSink(void)
{
	ConnState = TryWaitDebSink;
	setStateSink();

	StateTimer = T_TIMER_DISABLE;
}

void SetStateUnattachedDebugSource(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;
	platform_enable_timer(TRUE);
#endif
	ConnState = UnattachedDebSource;	// Set the state machine variable to unattached
	blnCCPinIsCC1 = FALSE;	// Clear the CC1 pin flag 
	blnCCPinIsCC2 = FALSE;	// Clear the CC2 pin flag

	setStateSource(FALSE);

	StateTimer = tTOG2;	// Disable the state timer, not used in this state
}
#endif /* FSC_DTS */

/////////////////////////////////////////////////////////////////////////////
//                        Type C Support Routines
/////////////////////////////////////////////////////////////////////////////

void updateSourceCurrent(void)
{
	switch (SourceCurrent) {
	case utccDefault:
		Registers.Control.HOST_CUR = 0b01;	// Set the host current to reflect the default USB power
		break;
	case utcc1p5A:
		Registers.Control.HOST_CUR = 0b10;	// Set the host current to reflect 1.5A
		break;
	case utcc3p0A:
		Registers.Control.HOST_CUR = 0b11;	// Set the host current to reflect 3.0A
		break;
	default:		// This assumes that there is no current being advertised
		Registers.Control.HOST_CUR = 0b00;	// Set the host current to disabled
		break;
	}
	DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);	// Commit the host current
}

void updateSourceMDACHigh(void)
{
	switch (Registers.Control.HOST_CUR) {
	case 0b01:
		Registers.Measure.MDAC = MDAC_1P596V;	// Set up DAC threshold to 1.6V (default USB current advertisement)
		break;
	case 0b10:
		Registers.Measure.MDAC = MDAC_1P596V;	// Set up DAC threshold to 1.6V
		break;
	case 0b11:
		Registers.Measure.MDAC = MDAC_2P604V;	// Set up DAC threshold to 2.6V
		break;
	default:		// This assumes that there is no current being advertised
		Registers.Measure.MDAC = MDAC_1P596V;	// Set up DAC threshold to 1.6V (default USB current advertisement)
		break;
	}
	DeviceWrite(regMeasure, 1, &Registers.Measure.byte);	// Commit the DAC threshold
}

void updateSourceMDACLow(void)
{
	switch (Registers.Control.HOST_CUR) {
	case 0b01:
		Registers.Measure.MDAC = MDAC_0P210V;	// Set up DAC threshold to 1.6V (default USB current advertisement)
		break;
	case 0b10:
		Registers.Measure.MDAC = MDAC_0P420V;	// Set up DAC threshold to 1.6V
		break;
	case 0b11:
		Registers.Measure.MDAC = MDAC_0P798V;	// Set up DAC threshold to 2.6V
		break;
	default:		// This assumes that there is no current being advertised
		Registers.Measure.MDAC = MDAC_1P596V;	// Set up DAC threshold to 1.6V (default USB current advertisement)
		break;
	}
	DeviceWrite(regMeasure, 1, &Registers.Measure.byte);	// Commit the DAC threshold
}

void ToggleMeasure(void)
{
	if (Registers.Switches.MEAS_CC2 == 1) {
		Registers.Switches.MEAS_CC1 = 1;	// Set CC1 to measure
		Registers.Switches.MEAS_CC2 = 0;	// Clear CC2 from measuring
	} else if (Registers.Switches.MEAS_CC1 == 1) {
		Registers.Switches.MEAS_CC1 = 0;	// Clear CC1 from measuring
		Registers.Switches.MEAS_CC2 = 1;	// Set CC2 to measure
	}

	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Set the switch to measure
}

CCTermType DecodeCCTermination(void)
{
	switch (sourceOrSink) {
#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
	case SOURCE:
		return DecodeCCTerminationSource();
#endif // FSC_HAVE_SRC || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE)
#ifdef FSC_HAVE_SNK
	case SINK:
		return DecodeCCTerminationSink();
#endif // FSC_HAVE_SNK
	default:
		return CCTypeUndefined;
	}
}

#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
CCTermType DecodeCCTerminationSource(void)
{
	regMeasure_t saved_measure;
	CCTermType Termination = CCTypeUndefined;	// By default set it to undefined
	saved_measure = Registers.Measure;

	updateSourceMDACHigh();
	platform_delay_10us(25);	// Delay to allow measurement to settle
	DeviceRead(regStatus0, 1, &Registers.Status.byte[4]);

	if (Registers.Status.COMP == 1) {
		Termination = CCTypeOpen;
		return Termination;
	}
	// Optimisation to assume CC pin in Attached Source is either Open or Rd - after dangling 10k cable detection
	else if (((ConnState == AttachedSource)
		  || (ConnState == PoweredAccessory)
		  || (ConnState == UnsupportedAccessory)
		  || (ConnState == DebugAccessorySource)
		  || (ConnState == AudioAccessory)
		  || (ConnState == AttachedDebSource))
		 && ((Registers.Switches.MEAS_CC1 && blnCCPinIsCC1)
		     || (Registers.Switches.MEAS_CC2 && blnCCPinIsCC2))
		 && (loopCounter == 0)) {
		switch (Registers.Control.HOST_CUR) {
		case 0b01:
			Termination = CCTypeRdUSB;
			break;
		case 0b10:
			Termination = CCTypeRd1p5;
			break;
		case 0b11:
			Termination = CCTypeRd3p0;
			break;
		case 0b00:
			break;
		}
		return Termination;
	}

	updateSourceMDACLow();
	platform_delay_10us(25);	// Delay to allow measurement to settle
	DeviceRead(regStatus0, 1, &Registers.Status.byte[4]);

	if (Registers.Status.COMP == 0) {
		Termination = CCTypeRa;
	} else {
		switch (Registers.Control.HOST_CUR) {
		case 0b01:
			Termination = CCTypeRdUSB;
			break;
		case 0b10:
			Termination = CCTypeRd1p5;
			break;
		case 0b11:
			Termination = CCTypeRd3p0;
			break;
		case 0b00:
			break;
		}
	}
	Registers.Measure = saved_measure;
	DeviceWrite(regMeasure, 1, &Registers.Measure.byte);
	return Termination;	// Return the termination type
}
#endif // FSC_HAVE_SRC || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE)

#ifdef FSC_HAVE_SNK
CCTermType DecodeCCTerminationSink(void)	// Port asserts Rd
{
	CCTermType Termination;
	platform_delay_10us(25);	// Delay to allow measurement to settle
	DeviceRead(regStatus0, 1, &Registers.Status.byte[4]);

	switch (Registers.Status.BC_LVL)	// Determine which level
	{
	case 0b00:		// If BC_LVL is lowest it's open
		Termination = CCTypeOpen;
		break;
	case 0b01:		// If BC_LVL is 1, it's default
		Termination = CCTypeRdUSB;
		break;
	case 0b10:		// If BC_LVL is 2, it's vRd1p5
		Termination = CCTypeRd1p5;
		break;
	default:		// Otherwise it's vRd3p0
		Termination = CCTypeRd3p0;
		break;
	}
	return Termination;	// Return the termination type
}
#endif // FSC_HAVE_SNK

#ifdef FSC_HAVE_SNK
void UpdateSinkCurrent(void)
{
	switch (Registers.Status.BC_LVL) {
	case 0b01:		// If we detect the default...
		SinkCurrent = utccDefault;
		break;
	case 0b10:		// If we detect 1.5A
		SinkCurrent = utcc1p5A;
		break;
	case 0b11:		// If we detect 3.0A
		SinkCurrent = utcc3p0A;
		break;
	case 0b00:
	default:
		SinkCurrent = utccNone;
		break;
	}
}
#endif // FSC_HAVE_SNK

/////////////////////////////////////////////////////////////////////////////
//                     Externally Accessible Routines
/////////////////////////////////////////////////////////////////////////////
#ifdef FSC_DEBUG

void ConfigurePortType(FSC_U8 Control)
{
	FSC_U8 value;
	FSC_BOOL setUnattached = FALSE;
	DisableTypeCStateMachine();
	value = Control & 0x03;
	if (PortType != value) {
		switch (value) {
		case 1:
#ifdef FSC_HAVE_SRC
			PortType = USBTypeC_Source;
#endif // FSC_HAVE_SRC
			break;
		case 2:
#ifdef FSC_HAVE_DRP
			PortType = USBTypeC_DRP;
#endif // FSC_HAVE_DRP
			break;
		default:
#ifdef FSC_HAVE_SNK
			PortType = USBTypeC_Sink;
#endif // FSC_HAVE_SNK
			break;
		}

		setUnattached = TRUE;
	}
#ifdef FSC_HAVE_ACCMODE
	if (((Control & 0x04) >> 2) != blnAccSupport) {
		if (Control & 0x04) {
			blnAccSupport = TRUE;
			Registers.Control4.TOG_USRC_EXIT = 1;
		} else {
			blnAccSupport = FALSE;
			Registers.Control4.TOG_USRC_EXIT = 0;
		}
		DeviceWrite(regControl4, 1, &Registers.Control4.byte);
		setUnattached = TRUE;
	}
#endif // FSC_HAVE_ACCMODE

#ifdef FSC_HAVE_DRP
	if (((Control & 0x08) >> 3) != blnSrcPreferred) {
		if (Control & 0x08) {
			blnSrcPreferred = TRUE;
		} else {
			blnSrcPreferred = FALSE;
		}
		setUnattached = TRUE;
	}

	if (((Control & 0x40) >> 5) != blnSnkPreferred) {
		if (Control & 0x40) {
			blnSnkPreferred = TRUE;
		} else {
			blnSnkPreferred = FALSE;
		}
		setUnattached = TRUE;
	}
#endif // FSC_HAVE_DRP

	if (setUnattached) {
		SetStateDelayUnattached();
	}
#ifdef FSC_HAVE_SRC
	value = (Control & 0x30) >> 4;
	if (SourceCurrent != value) {
		switch (value) {
		case 1:
			SourceCurrent = utccDefault;
			break;
		case 2:
			SourceCurrent = utcc1p5A;
			break;
		case 3:
			SourceCurrent = utcc3p0A;
			break;
		default:
			SourceCurrent = utccNone;
			break;
		}
		updateSourceCurrent();
	}
#endif // FSC_HAVE_SRC

	if (Control & 0x80)
		EnableTypeCStateMachine();
}

#ifdef FSC_HAVE_SRC
void UpdateCurrentAdvert(FSC_U8 Current)
{
	switch (Current) {
	case 1:
		SourceCurrent = utccDefault;
		break;
	case 2:
		SourceCurrent = utcc1p5A;
		break;
	case 3:
		SourceCurrent = utcc3p0A;
		break;
	default:
		SourceCurrent = utccNone;
		break;
	}
	updateSourceCurrent();
}
#endif // FSC_HAVE_SRC

void GetDeviceTypeCStatus(FSC_U8 abytData[])
{
	FSC_S32 intIndex = 0;
	abytData[intIndex++] = GetTypeCSMControl();	// Grab a snapshot of the top level control
	abytData[intIndex++] = ConnState & 0xFF;	// Get the current state
	abytData[intIndex++] = GetCCTermination();	// Get the current CC termination
	abytData[intIndex++] = SinkCurrent;	// Set the sink current capability detected
}

FSC_U8 GetTypeCSMControl(void)
{
	FSC_U8 status = 0;
	status |= (PortType & 0x03);	// Set the type of port that we are configured as
	switch (PortType)	// Set the port type that we are configured as
	{
#ifdef FSC_HAVE_SRC
	case USBTypeC_Source:
		status |= 0x01;	// Set Source type
		break;
#endif // FSC_HAVE_SRC
#ifdef FSC_HAVE_DRP
	case USBTypeC_DRP:
		status |= 0x02;	// Set DRP type
		break;
#endif // FSC_HAVE_DRP
	default:		// If we are not DRP or Source, we are Sink which is a value of zero as initialized
		break;
	}

#ifdef FSC_HAVE_ACCMODE
	if (blnAccSupport)	// Set the flag if we support accessories 
		status |= 0x04;
#endif // FSC_HAVE_ACCMODE

#ifdef FSC_HAVE_DRP
	if (blnSrcPreferred)	// Set the flag if we prefer Source mode (as a DRP)
		status |= 0x08;
	if (blnSnkPreferred)
		status |= 0x40;
#endif // FSC_HAVE_DRP

	status |= (SourceCurrent << 4);

	if (blnSMEnabled)	// Set the flag if the state machine is enabled
		status |= 0x80;
	return status;
}

FSC_U8 GetCCTermination(void)
{
	FSC_U8 status = 0;

	status |= (CCTermPrevious & 0x07);	// Set the current CC1 termination
	status |= ((VCONNTerm & 0x07) << 4);	// Set the current CC2 termination

	return status;
}
#endif // FSC_DEBUG

/////////////////////////////////////////////////////////////////////////////
//                        Device I2C Routines
/////////////////////////////////////////////////////////////////////////////
FSC_BOOL VbusVSafe0V(void)
{
	return !isVBUSOverVoltage(VBUS_MDAC_0P84V);
}

FSC_BOOL isVSafe5V(void)	// Returns true when Vbus > ~4.6V
{
	return isVBUSOverVoltage(VBUS_MDAC_4P62);
}

FSC_BOOL isVBUSOverVoltage(FSC_U8 vbusMDAC)
{
	regMeasure_t measure;

	FSC_U8 val;
	FSC_BOOL ret;

	measure = Registers.Measure;
	measure.MEAS_VBUS = 1;	// Measure VBUS
	measure.MDAC = vbusMDAC;
	DeviceWrite(regMeasure, 1, &measure.byte);	// Write it into device

	platform_delay_10us(35);	// Delay to allow measurement to settle

	DeviceRead(regStatus0, 1, &val);	// get COMP result
	val &= 0x20;		// COMP = bit 5 of status0 (Device specific?)

	if (val)
		ret = TRUE;	// Determine return value based on COMP
	else
		ret = FALSE;

	// restore register values
	DeviceWrite(regMeasure, 1, &Registers.Measure.byte);
	return ret;
}

void DetectCCPinSource(void)	//Patch for directly setting states
{
	CCTermType CCTerm;

	if (Registers.DeviceID.VERSION_ID == VERSION_302B) {
		Registers.Switches.byte[0] = 0xC4;	// Enable CC pull-ups and CC1 measure
	} else {
		Registers.Switches.byte[0] = 0x44;	// Enable CC1 pull-up and measure
	}
	DeviceWrite(regSwitches0, 1, &(Registers.Switches.byte[0]));
	CCTerm = DecodeCCTermination();
	if ((CCTerm >= CCTypeRdUSB) && (CCTerm < CCTypeUndefined)) {
		blnCCPinIsCC1 = TRUE;	// The CC pin is CC1
		blnCCPinIsCC2 = FALSE;
		return;
	}

	if (Registers.DeviceID.VERSION_ID == VERSION_302B) {
		Registers.Switches.byte[0] = 0xC8;	// Enable CC pull-ups and CC2 measure
	} else {
		Registers.Switches.byte[0] = 0x88;	// Enable CC2 pull-up and measure
	}
	DeviceWrite(regSwitches0, 1, &(Registers.Switches.byte[0]));
	CCTerm = DecodeCCTermination();
	if ((CCTerm >= CCTypeRdUSB) && (CCTerm < CCTypeUndefined)) {

		blnCCPinIsCC1 = FALSE;	// The CC pin is CC2
		blnCCPinIsCC2 = TRUE;
		return;
	}
}

void DetectCCPinSink(void)	//Patch for directly setting states
{
	CCTermType CCTerm;

	Registers.Switches.byte[0] = 0x07;
	DeviceWrite(regSwitches0, 1, &(Registers.Switches.byte[0]));
	CCTerm = DecodeCCTermination();
	if ((CCTerm > CCTypeRa) && (CCTerm < CCTypeUndefined)) {
		blnCCPinIsCC1 = TRUE;	// The CC pin is CC1
		blnCCPinIsCC2 = FALSE;
		return;
	}

	Registers.Switches.byte[0] = 0x0B;
	DeviceWrite(regSwitches0, 1, &(Registers.Switches.byte[0]));
	CCTerm = DecodeCCTermination();
	if ((CCTerm > CCTypeRa) && (CCTerm < CCTypeUndefined)) {

		blnCCPinIsCC1 = FALSE;	// The CC pin is CC2
		blnCCPinIsCC2 = TRUE;
		return;
	}
}

void resetDebounceVariables(void)
{
	CCTermPrevious = CCTypeUndefined;
	CCTermCCDebounce = CCTypeUndefined;
	CCTermPDDebounce = CCTypeUndefined;
	CCTermPDDebouncePrevious = CCTypeUndefined;
	VCONNTerm = CCTypeUndefined;
}

////////////////////////////////////////////////////////////////////////////
//                     
////////////////////////////////////////////////////////////////////////////
#ifdef FSC_DEBUG
FSC_BOOL GetLocalRegisters(FSC_U8 * data, FSC_S32 size)	// Returns local registers as data array
{
	if (size != 23)
		return FALSE;

	data[0] = Registers.DeviceID.byte;
	data[1] = Registers.Switches.byte[0];
	data[2] = Registers.Switches.byte[1];
	data[3] = Registers.Measure.byte;
	data[4] = Registers.Slice.byte;
	data[5] = Registers.Control.byte[0];
	data[6] = Registers.Control.byte[1];
	data[7] = Registers.Control.byte[2];
	data[8] = Registers.Control.byte[3];
	data[9] = Registers.Mask.byte;
	data[10] = Registers.Power.byte;
	data[11] = Registers.Reset.byte;
	data[12] = Registers.OCPreg.byte;
	data[13] = Registers.MaskAdv.byte[0];
	data[14] = Registers.MaskAdv.byte[1];
	data[15] = Registers.Control4.byte;
	data[16] = Registers.Status.byte[0];
	data[17] = Registers.Status.byte[1];
	data[18] = Registers.Status.byte[2];
	data[19] = Registers.Status.byte[3];
	data[20] = Registers.Status.byte[4];
	data[21] = Registers.Status.byte[5];
	data[22] = Registers.Status.byte[6];

	return TRUE;
}

FSC_BOOL GetStateLog(FSC_U8 * data)
{				// Loads log into byte array
	FSC_S32 i;
	FSC_S32 entries = TypeCStateLog.Count;
	FSC_U16 state_temp;
	FSC_U16 time_tms_temp;
	FSC_U16 time_s_temp;

	for (i = 0; ((i < entries) && (i < 12)); i++) {
		ReadStateLog(&TypeCStateLog, &state_temp, &time_tms_temp,
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
#endif // FSC_DEBUG

void debounceCC(void)
{
	//PD Debounce (filter)
	CCTermType CCTermCurrent = DecodeCCTermination();	// Grab the latest CC termination value

	if (CCTermPrevious != CCTermCurrent)	// Check to see if the value has changed...
	{
		CCTermPrevious = CCTermCurrent;	// If it has, update the value
		PDDebounceTimer = tPDDebounce;	// Restart the debounce timer with tPDDebounce (wait 10ms before detach)
		if (PDFilterTimer == T_TIMER_DISABLE)	// Start debounce filter if it is not already enabled
		{
			PDFilterTimer = tPDDebounce;
		}
	}

	if (PDDebounceTimer == 0)	// Check to see if our debounce timer has expired...
	{
		CCTermPDDebounce = CCTermPrevious;	// Update the CC debounced values
		PDDebounceTimer = T_TIMER_DISABLE;
		PDFilterTimer = T_TIMER_DISABLE;
	}
	if (PDFilterTimer == 0) {
		CCDebounceTimer = tCCDebounce;
	}
	//CC debounce
	if (CCTermPDDebouncePrevious != CCTermPDDebounce)	//If the PDDebounce values have changed
	{
		CCTermPDDebouncePrevious = CCTermPDDebounce;	//Update the previous value
		CCDebounceTimer = tCCDebounce - tPDDebounce;	// reset the tCCDebounce timers
		CCTermCCDebounce = CCTypeUndefined;	// Set CC debounce values to undefined while it is being debounced
	}
	if (CCDebounceTimer == 0) {
		CCTermCCDebounce = CCTermPDDebouncePrevious;	// Update the CC debounced values
		CCDebounceTimer = T_TIMER_DISABLE;
		PDFilterTimer = T_TIMER_DISABLE;
	}
}

#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void updateVCONNSource(void)	// Assumes PUs have been set
{
	FSC_U8 saveRegister = Registers.Switches.byte[0];	// Save current Switches
	ToggleMeasure();	// Toggle measure to VCONN

	if (Registers.DeviceID.VERSION_ID == VERSION_302A)	// Toggle PU if 302A
	{
		if (blnCCPinIsCC1) {
			Registers.Switches.PU_EN1 = 0;
			Registers.Switches.PU_EN2 = 1;
		} else {
			Registers.Switches.PU_EN1 = 1;
			Registers.Switches.PU_EN2 = 0;
		}

		DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
	}

	VCONNTerm = DecodeCCTermination();

	Registers.Switches.byte[0] = saveRegister;	// Restore Switches
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
}

void setStateSource(FSC_BOOL vconn)
{
	sourceOrSink = SOURCE;
	resetDebounceVariables();
	updateSourceCurrent();
	updateSourceMDACHigh();
	Registers.Power.PWR = 0x7;	// Enable everything except internal oscillator
	DeviceWrite(regPower, 1, &Registers.Power.byte);	// Commit the power state

	if ((blnCCPinIsCC1 == FALSE) && (blnCCPinIsCC2 == FALSE))	//For automated testing
	{
		DetectCCPinSource();
	}
	if (blnCCPinIsCC1)	// If we detected CC1 as an Rd
	{
		if (Registers.DeviceID.VERSION_ID == VERSION_302B) {
			Registers.Switches.byte[0] = 0xC4;	// Enable CC pull-ups and CC1 measure
		} else {
			Registers.Switches.byte[0] = 0x44;	// Enable CC1 pull-up and measure
		}
		if (vconn) {
			Registers.Switches.VCONN_CC2 = 1;
		}
	} else {
		if (Registers.DeviceID.VERSION_ID == VERSION_302B) {
			Registers.Switches.byte[0] = 0xC8;	// Enable CC pull-ups and CC2 measure
		} else {
			Registers.Switches.byte[0] = 0x88;	// Enable CC2 pull-up and measure
		}
		if (vconn) {
			Registers.Switches.VCONN_CC1 = 1;
		}
	}
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the switch state
	if (!vconn) {
		updateVCONNSource();
	}
	SinkCurrent = utccNone;	// Not used in accessories
	PDFilterTimer = T_TIMER_DISABLE;	// Disable PD filter timer
	PDDebounceTimer = tPDDebounce;	// Once in this state, we are waiting for the lines to be stable for tCCDebounce before changing states
	CCDebounceTimer = tCCDebounce;	// Disable the 2nd level debouncing initially to force completion of a 1st level debouncing                                        // Once we are in the audio.accessory state, we are going to stop toggling and only monitor CC1
#ifdef FSC_DEBUG
	WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif // FSC_DEBUG
}
#endif // FSC_HAVE_SRC || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE))

#ifdef FSC_HAVE_SNK
void setStateSink(void)
{
	platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);	// Disable the vbus outputs

	sourceOrSink = SINK;
	resetDebounceVariables();

	Registers.Power.PWR = 0x7;	// Enable everything except internal oscillator
	DeviceWrite(regPower, 1, &Registers.Power.byte);	// Commit the power state

	if ((blnCCPinIsCC1 == FALSE) && (blnCCPinIsCC2 == FALSE))	//For automated testing
	{
		DetectCCPinSink();
	}
	if (blnCCPinIsCC1)	// If we detected CC1 as an Rd
	{
		Registers.Switches.byte[0] = 0x07;	// Enable the pull-downs on the CC pins
	} else {
		Registers.Switches.byte[0] = 0x0B;	// Enable the pull-downs on the CC pins                    
	}

	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the switch state 
	updateVCONNSink();

	PDDebounceTimer = tPDDebounce;	// Set the tPDDebounce for validating signals to transition to
	CCDebounceTimer = tCCDebounce;	// Disable the 2nd level debouncing until the first level has been debounced                              // Set the toggle timer to look at each pin for tDeviceToggle duration
	PDFilterTimer = T_TIMER_DISABLE;	// Disable PD filter timer
#ifdef FSC_DEBUG
	WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif // FSC_DEBUG
}

void updateVCONNSink(void)	// Assumes Rd has been set
{
	ToggleMeasure();

	VCONNTerm = DecodeCCTermination();

	ToggleMeasure();
}

#endif // FSC_HAVE_SNK

void clearState(void)
{
	platform_set_vbus_lvl_enable(VBUS_LVL_ALL, FALSE, FALSE);	// Disable the vbus outputs
	USBPDDisable(TRUE);
	Registers.Mask.byte = 0xFF;	// Mask all
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0xFF;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 1;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
	Registers.Power.PWR = 0x1;	// Enter low power state
	Registers.Control.TOGGLE = 0;	// Disable the toggle state machine
	Registers.Control.HOST_CUR = 0b00;	// Disable the currents for the pull-ups (not used for UFP)
	DeviceWrite(regPower, 1, &Registers.Power.byte);	// Commit the power state
	DeviceWrite(regControl0, 3, &Registers.Control.byte[0]);	// Commit the control state
	if (ConnState != Unattached) {
		Registers.Switches.byte[0] = 0x00;	// Disable all pull-ups and pull-downs on the CC pins and disable the BMC transmitters
		DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the switch state
	}
	resetDebounceVariables();
	blnCCPinIsCC1 = FALSE;	// Clear the CC1 pin flag
	blnCCPinIsCC2 = FALSE;	// Clear the CC2 pin flag
	PDDebounceTimer = T_TIMER_DISABLE;	// Disable the 1st level debounce timer
	CCDebounceTimer = T_TIMER_DISABLE;	// Disable the 2nd level debounce timer                                    // Disable the toggle timer
	PDFilterTimer = T_TIMER_DISABLE;	// Disable PD filter timer
	platform_notify_cc_orientation(NONE);
#ifdef FSC_DEBUG
	WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
#endif // FSC_DEBUG
}

#ifdef FSC_HAVE_ACCMODE
void checkForAccessory(void)	// TODO
{
	Registers.Control.TOGGLE = 0;
	DeviceWrite(regControl2, 1, &Registers.Control.byte[2]);

	debounceCC();
	updateVCONNSource();

	SetStateAttachWaitAccessory();
}
#endif // FSC_HAVE_ACCMODE

#ifdef FSC_DEBUG
void ProcessTypeCPDStatus(FSC_U8 * MsgBuffer, FSC_U8 * retBuffer)
{
	if (MsgBuffer[1] != 1)	// Check to see that the version is 1
		retBuffer[1] = 0x01;	// If it wasn't one, return that the version is not recognized
	else {
		GetDeviceTypeCStatus((FSC_U8 *) & retBuffer[4]);	// Return the status of the USB Type C state machine
		GetUSBPDStatus((FSC_U8 *) & retBuffer[8]);	// Return the status of the USB PD state machine
	}
}

void ProcessTypeCPDControl(FSC_U8 * MsgBuffer, FSC_U8 * retBuffer)
{
	if (MsgBuffer[1] != 0) {
		retBuffer[1] = 0x01;	// Return that the version is not recognized
		return;
	}
	switch (MsgBuffer[4])	// Determine command type
	{
	case 0x01:		// Reset the state machine
		DisableTypeCStateMachine();
		EnableTypeCStateMachine();
		break;
	case 0x02:		// Disable state machine
		DisableTypeCStateMachine();
		break;
	case 0x03:		// Enable state machine
		EnableTypeCStateMachine();
		break;
	case 0x04:		// Configure port type
		ConfigurePortType(MsgBuffer[5]);
		break;
#ifdef FSC_HAVE_SRC
	case 0x05:		// Update current advertisement
		UpdateCurrentAdvert(MsgBuffer[5]);
		break;
#endif // FSC_HAVE_SRC
	case 0x06:		// Enable USB PD
		EnableUSBPD();
		break;
	case 0x07:		// Disable USB PD
		DisableUSBPD();
		break;
	case 0x08:		// Send USB PD Command/Message
		SendUSBPDMessage((FSC_U8 *) & MsgBuffer[5]);
		break;
#ifdef FSC_HAVE_SRC
	case 0x09:		// Update the source capabilities
		WriteSourceCapabilities((FSC_U8 *) & MsgBuffer[5]);
		break;
	case 0x0A:		// Read the source capabilities
		retBuffer[4] = MsgBuffer[4];	// Echo back the actual command for verification
		ReadSourceCapabilities((FSC_U8 *) & retBuffer[5]);
		break;
#endif // FSC_HAVE_SRC
#ifdef FSC_HAVE_SNK
	case 0x0B:		// Update the sink capabilities
		WriteSinkCapabilities((FSC_U8 *) & MsgBuffer[5]);
		break;
#endif // FSC_HAVE_SNK
	case 0x0C:		// Read the sink capabilities
		retBuffer[4] = MsgBuffer[4];	// Echo back the actual command for verification
		ReadSinkCapabilities((FSC_U8 *) & retBuffer[5]);
		break;
#ifdef FSC_HAVE_SNK
	case 0x0D:		// Update the default sink settings
		WriteSinkRequestSettings((FSC_U8 *) & MsgBuffer[5]);
		break;
	case 0x0E:		// Read the default sink settings
		retBuffer[4] = MsgBuffer[4];	// Echo back the actual command for verification
		ReadSinkRequestSettings((FSC_U8 *) & retBuffer[5]);
		break;
#endif // FSC_HAVE_SNK
	case 0x0F:		// Send USB PD Hard Reset
		retBuffer[4] = MsgBuffer[4];	// Echo back the actual command for verification
		SendUSBPDHardReset();	// Send a USB PD Hard Reset
		break;

#ifdef FSC_HAVE_VDM
	case 0x10:
		retBuffer[4] = MsgBuffer[4];	// Echo back the actual command for verification
		ConfigureVdmResponses((FSC_U8 *) & MsgBuffer[5]);	// Configure SVIDs/Modes
		break;
	case 0x11:
		retBuffer[4] = MsgBuffer[4];	// Echo back the actual command for verification
		ReadVdmConfiguration((FSC_U8 *) & retBuffer[5]);	// Read SVIDs/Modes
		break;
#endif // FSC_HAVE_VDM

#ifdef FSC_HAVE_DP
	case 0x12:
		retBuffer[4] = MsgBuffer[4];
		WriteDpControls((FSC_U8 *) & MsgBuffer[5]);
		break;
	case 0x13:
		retBuffer[4] = MsgBuffer[4];
		ReadDpControls((FSC_U8 *) & retBuffer[5]);
		break;
	case 0x14:
		retBuffer[4] = MsgBuffer[4];
		ReadDpStatus((FSC_U8 *) & retBuffer[5]);
		break;
#endif // FSC_HAVE_DP

	default:
		break;
	}
}

void ProcessLocalRegisterRequest(FSC_U8 * MsgBuffer, FSC_U8 * retBuffer)	// Send local register values
{
	if (MsgBuffer[1] != 0) {
		retBuffer[1] = 0x01;	// Return that the version is not recognized
		return;
	}

	GetLocalRegisters(&retBuffer[3], 23);	// Writes local registers to send buffer [3] - [25]
}

void ProcessSetTypeCState(FSC_U8 * MsgBuffer, FSC_U8 * retBuffer)	// Set state machine
{
	ConnectionState state = (ConnectionState) MsgBuffer[3];

	if (MsgBuffer[1] != 0) {
		retBuffer[1] = 0x01;	// Return that the version is not recognized
		return;
	}

	switch (state) {
	case (Disabled):
		SetStateDisabled();
		break;
	case (ErrorRecovery):
		SetStateErrorRecovery();
		break;
	case (Unattached):
		SetStateUnattached();
		break;
#ifdef FSC_HAVE_SNK
	case (AttachWaitSink):
		SetStateAttachWaitSink();
		break;
	case (AttachedSink):
		SetStateAttachedSink();
		break;
#ifdef FSC_HAVE_DRP
	case (TryWaitSink):
		SetStateTryWaitSink();
		break;
	case (TrySink):
		SetStateTrySink();
		break;
#endif // FSC_HAVE_DRP
#endif // FSC_HAVE_SNK
#ifdef FSC_HAVE_SRC
	case (AttachWaitSource):
		SetStateAttachWaitSource();
		break;
	case (AttachedSource):
		SetStateAttachedSource();
		break;
#ifdef FSC_HAVE_DRP
	case (TrySource):
		SetStateTrySource();
		break;
	case (TryWaitSource):
		SetStateTryWaitSource();
		break;
#endif // FSC_HAVE_DRP
	case (UnattachedSource):
		SetStateUnattachedSource();
		break;
#endif // FSC_HAVE_SRC
#ifdef FSC_HAVE_ACCMODE
	case (AudioAccessory):
		SetStateAudioAccessory();
		break;
	case (DebugAccessorySource):
		SetStateDebugAccessorySource();
		break;
#endif // FSC_HAVE_ACCMODE
#if (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
	case (AttachWaitAccessory):
		SetStateAttachWaitAccessory();
		break;
	case (PoweredAccessory):
		SetStatePoweredAccessory();
		break;
	case (UnsupportedAccessory):
		SetStateUnsupportedAccessory();
		break;
#endif /* (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)) */
	case (DelayUnattached):
		SetStateDelayUnattached();
		break;
	default:
		SetStateDelayUnattached();
		break;
	}
}

/*
 * Buffer[3] = # of log entries(0-12)
 * Buffer[4+] = entries
 * Format (5 byte):
 * [command][s(h)][s(l)][tms(h)][tms(l)]
*/
void ProcessReadTypeCStateLog(FSC_U8 * MsgBuffer, FSC_U8 * retBuffer)
{
	if (MsgBuffer[1] != 0) {
		retBuffer[1] = 0x01;	// Return that the version is not recognized
		return;
	}

	GetStateLog(&retBuffer[3]);	// Designed for 64 byte buffer
}

#endif // FSC_DEBUG

void toggleCurrentSwap(void)
{
	if (toggleCurrent == utccDefault) {
		toggleCurrent = utcc1p5A;
	} else {
		toggleCurrent = utccDefault;
	}
}
