#include "AlternateModes.h"

BOOL startDRPSource = FALSE;
extern BOOL g_Idle;

void SetStateAlternateUnattached(void)
{
	if ((PortType == USBTypeC_DRP) || ((PortType == USBTypeC_Sink) && (blnAccSupport)))	// If we are a DRP or sink supporting accessories
	{
		SetStateAlternateDRP();
	} else if (PortType == USBTypeC_Source)	// If we are strictly a Source
	{
		SetStateAlternateUnattachedSource();
	} else			// Otherwise we are a sink
	{
		SetStateAlternateUnattachedSink();
	}
}

void StateMachineAlternateUnattached(void)
{
	if ((PortType == USBTypeC_DRP) || ((PortType == USBTypeC_Sink) && (blnAccSupport)))	// If we are a DRP or sink supporting accessories
	{
		StateMachineAlternateDRP();
	} else if (PortType == USBTypeC_Source)	// If we are strictly a Source
	{
		StateMachineAlternateUnattachedSource();
	} else			// Otherwise we are a sink
	{
		StateMachineAlternateUnattachedSink();
	}
}

void SetStateAlternateDRP(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;		// Run continuously (unmask all)
	Registers.Mask.byte = 0x20;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0x00;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 0;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
	platform_enable_timer(TRUE);
#endif
	platform_set_vbus_5v_enable(FALSE);	// Disable the 5V output...
	platform_set_vbus_lvl1_enable(FALSE);	// Disable the 12V output
	Registers.Switches.byte[0] = 0x00;
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
	if (ConnState == AttachedSource)	// If last state was Attached.Source
	{
		while (!VbusVSafe0V()) ;	// wait for Vbus to discharge
	}
	ConnState = Unattached;	// Set the state machine variable to unattached

	if ((blnCCPinIsCC1 && startDRPSource)
	    || (blnCCPinIsCC2 && (startDRPSource == 0))) {
		sourceOrSink = Source;	// This doesn't make sense for this state, so this will pertain to CC1
		Registers.Switches.byte[0] = 0x46;	// Enable Pullup1, Pulldown2, and Meas1
		DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the switch state
	} else {
		sourceOrSink = Sink;	// This doesn't make sense for this state, so this will pertain to CC1
		Registers.Switches.byte[0] = 0x89;	// Enable Pullup2, Pulldown1, and Meas1
		DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the switch state
	}
	startDRPSource = 0;
	Registers.Power.PWR = 0x7;	// Enable everything except internal oscillator
	DeviceWrite(regPower, 1, &Registers.Power.byte);	// Commit the power state
	updateSourceCurrent();	// Updates source current
	USBPDDisable(TRUE);	// Disable the USB PD state machine (no need to write Device again since we are doing it here)
	SinkCurrent = utccNone;
	resetDebounceVariables();
	blnCCPinIsCC1 = FALSE;	// Clear the CC1 pin flag 
	blnCCPinIsCC2 = FALSE;	// Clear the CC2 pin flag
	StateTimer = tAlternateDRPSwap;
	PDDebounce = T_TIMER_DISABLE;	// enable the 1st level debounce timer, not used in this state
	CCDebounce = T_TIMER_DISABLE;	// enable the 2nd level debounce timer, not used in this state
	ToggleTimer = tDeviceToggle;	// enable the toggle timer
	OverPDDebounce = tPDDebounce;	// enable PD filter timer
	WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
}

void StateMachineAlternateDRP(void)
{
	debounceCC();

	if (StateTimer == 0) {
		AlternateDRPSwap();
		StateTimer = tAlternateDRPSwap;
	}

	if ((CC1TermPrevious >= CCTypeRdUSB) && (CC1TermPrevious <= CCTypeRd3p0)
	    && (sourceOrSink == Source)) {
		blnCCPinIsCC1 = TRUE;
		blnCCPinIsCC2 = FALSE;
		if ((PortType == USBTypeC_Sink) && (blnAccSupport))	// If we are configured as a sink and support accessories...
			checkForAccessory();	// Go to the AttachWaitAcc state
		else		// Otherwise we must be configured as a source or DRP
			SetStateAttachWaitSource();
	}
	// NOTE: Remember sourceOrSink refers to CC1 in this funky state - CC2 is opposite
	else if ((CC2TermPrevious >= CCTypeRdUSB)
		 && (CC2TermPrevious <= CCTypeRd3p0)
		 && (sourceOrSink == Sink)) {
		blnCCPinIsCC1 = FALSE;
		blnCCPinIsCC2 = TRUE;
		if ((PortType == USBTypeC_Sink) && (blnAccSupport))	// If we are configured as a sink and support accessories...
			checkForAccessory();	// Go to the AttachWaitAcc state
		else		// Otherwise we must be configured as a source or DRP
			SetStateAttachWaitSource();
	} else if ((CC1TermPrevious >= CCTypeRdUSB)
		   && (CC1TermPrevious <= CCTypeRd3p0)
		   && (sourceOrSink == Sink)) {
		blnCCPinIsCC1 = TRUE;
		blnCCPinIsCC2 = FALSE;
		SetStateAttachWaitSink();
	}
	// NOTE: Remember sourceOrSink refers to CC1 in this funky state - CC2 is opposite
	else if ((CC2TermPrevious >= CCTypeRdUSB)
		 && (CC2TermPrevious <= CCTypeRd3p0)
		 && (sourceOrSink == Source)) {
		blnCCPinIsCC1 = FALSE;
		blnCCPinIsCC2 = TRUE;
		SetStateAttachWaitSink();
	}

}

void AlternateDRPSwap(void)
{
	if (sourceOrSink == Source) {
		Registers.Switches.byte[0] = 0x89;	// Enable Pullup2, Pulldown1, and Meas2
		DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the switch state
		sourceOrSink = Sink;
	} else {
		Registers.Switches.byte[0] = 0x46;	// Enable Pullup1, Pulldown2, and Meas1
		DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the switch state
		sourceOrSink = Source;
	}
}

void AlternateDRPSourceSinkSwap(void)
{
	if (ConnState == Unattached) {
		if (Registers.Switches.MEAS_CC2 == 1)	// CC2 is opposite in this state
		{
			if (sourceOrSink == Source) {
				sourceOrSink = Sink;
			} else {
				sourceOrSink = Source;
			}
		}
	}
}

void SetStateAlternateUnattachedSource(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = TRUE;		// Idle until COMP or BC_LVL
	Registers.Mask.byte = 0xFF;
	Registers.Mask.M_COMP_CHNG = 0;
	Registers.Mask.M_BC_LVL = 0;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0xFF;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 1;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
	platform_enable_timer(FALSE);
#endif
	if (PortType == USBTypeC_DRP) {
		startDRPSource = 1;
		SetStateAlternateDRP();
		return;
	}
	platform_set_vbus_5v_enable(FALSE);	// Disable the 5V output...
	platform_set_vbus_lvl1_enable(FALSE);	// Disable the 12V output
	Registers.Switches.byte[0] = 0x00;
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
	while (!VbusVSafe0V()) ;	// wait for vbus to discharge
	ConnState = UnattachedSource;	// Set the state machine variable to unattached
	sourceOrSink = Source;
	Registers.Switches.byte[0] = 0xC4;	// Enable both pull-ups and measure on CC1
	Registers.Power.PWR = 0x7;	// Enable everything except internal oscillator
	Registers.Control.HOST_CUR = 0b11;	// Set host current to 330uA
	DeviceWrite(regPower, 1, &Registers.Power.byte);	// Commit the power state
	DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);	// Commit host current
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the switch state
	USBPDDisable(TRUE);	// Disable the USB PD state machine (no need to write Device again since we are doing it here)
	SinkCurrent = utccNone;
	resetDebounceVariables();
	blnCCPinIsCC1 = FALSE;	// Clear the CC1 pin flag 
	blnCCPinIsCC2 = FALSE;	// Clear the CC2 pin flag
	StateTimer = T_TIMER_DISABLE;	// Disable the state timer, not used in this state
	PDDebounce = tPDDebounce;	// enable the 1st level debounce timer, not used in this state
	CCDebounce = tCCDebounce;	// enable the 2nd level debounce timer, not used in this state
	ToggleTimer = T_TIMER_DISABLE;	// disable the toggle timer
	DRPToggleTimer = T_TIMER_DISABLE;	// Timer to switch from unattachedSrc to unattachedSnk in DRP
	OverPDDebounce = T_TIMER_DISABLE;	// enable PD filter timer
	WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
}

void StateMachineAlternateUnattachedSource(void)	// CC1 and CC2 are shorted, so we just look for CC1 < 2.6V for Ra/Rd and > 0.2V for no Ra
{
	CCTermType previous = AlternateDecodeCCTerminationSource();

	if (blnAccSupport) {
		if (previous == CCTypeRa) {
			blnCCPinIsCC1 = FALSE;	// Setting both to false will have the next state figure it out
			blnCCPinIsCC2 = FALSE;
			SetStateAttachWaitSource();
			return;
		}
	}

	if ((previous == CCTypeRd3p0) || (previous == CCTypeRd1p5))	// If a CC pin is Rd
	{
		blnCCPinIsCC1 = FALSE;	// Setting both to false will have the next state figure it out
		blnCCPinIsCC2 = FALSE;
		SetStateAttachWaitSource();	// Go to the Attached.Src state
	} else if (previous == CCTypeRdUSB)	// Either Ra-Open or Rd-Ra
	{
		peekCC1Source();	// Connect if Rd on CC1
		if ((CC1TermPrevious == CCTypeRdUSB)
		    || (CC1TermPrevious == CCTypeRd1p5)
		    || (CC1TermPrevious == CCTypeRd3p0)) {
			blnCCPinIsCC1 = FALSE;
			blnCCPinIsCC2 = FALSE;
			SetStateAttachWaitSource();
		} else {
			peekCC2Source();	// Else connect if Rd on CC2
			if ((CC2TermPrevious == CCTypeRdUSB)
			    || (CC2TermPrevious == CCTypeRd1p5)
			    || (CC2TermPrevious == CCTypeRd3p0)) {
				blnCCPinIsCC1 = FALSE;
				blnCCPinIsCC2 = FALSE;
				SetStateAttachWaitSource();
			}
		}
	}
}

void StateMachineAlternateUnattachedSink(void)
{
	debounceCC();

	if ((CC1TermPrevious >= CCTypeRdUSB) && (CC1TermPrevious < CCTypeUndefined) && ((CC2TermPrevious == CCTypeRa) || CC2TermPrevious == CCTypeOpen))	// If the CC1 pin is Rd for atleast tPDDebounce...
	{
		blnCCPinIsCC1 = TRUE;	// The CC pin is CC1
		blnCCPinIsCC2 = FALSE;
		SetStateAttachWaitSink();	// Go to the Attached.Snk state
	} else if ((CC2TermPrevious >= CCTypeRdUSB) && (CC2TermPrevious < CCTypeUndefined) && ((CC1TermPrevious == CCTypeRa) || CC1TermPrevious == CCTypeOpen))	// If the CC2 pin is Rd for atleast tPDDebounce...
	{
		blnCCPinIsCC1 = FALSE;	// The CC pin is CC2
		blnCCPinIsCC2 = TRUE;
		SetStateAttachWaitSink();	// Go to the Attached.Snk state
	}
}

void SetStateAlternateUnattachedSink(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;		// run continuously
	Registers.Mask.byte = 0x20;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0x00;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 0;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
	platform_enable_timer(TRUE);
#endif
	platform_set_vbus_5v_enable(FALSE);	// Disable the 5V output...
	platform_set_vbus_lvl1_enable(FALSE);	// Disable the 12V output
	Registers.Switches.byte[0] = 0x00;
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);
	while (!VbusVSafe0V()) ;	// waitfor vbus to fall

	ConnState = Unattached;	// Set the state machine variable to unattached
	sourceOrSink = Sink;
	Registers.Switches.byte[0] = 0x07;	// Enable both pull-downs and measure on CC1
	Registers.Power.PWR = 0x7;	// Enable everything except internal oscillator
	DeviceWrite(regPower, 1, &Registers.Power.byte);	// Commit the power state
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the switch state
	USBPDDisable(TRUE);	// Disable the USB PD state machine (no need to write Device again since we are doing it here)
	SinkCurrent = utccNone;
	resetDebounceVariables();
	blnCCPinIsCC1 = FALSE;	// Clear the CC1 pin flag 
	blnCCPinIsCC2 = FALSE;	// Clear the CC2 pin flag
	StateTimer = T_TIMER_DISABLE;	// Disable the state timer, not used in this state
	PDDebounce = tPDDebounce;	// enable the 1st level debounce timer, not used in this state
	CCDebounce = tCCDebounce;	// enable the 2nd level debounce timer, not used in this state
	ToggleTimer = tDeviceToggle;	// disable the toggle timer
	DRPToggleTimer = tDeviceToggle;	// Timer to switch from unattachedSrc to unattachedSnk in DRP
	OverPDDebounce = T_TIMER_DISABLE;	// enable PD filter timer
	WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
}

void SetStateAlternateAudioAccessory(void)
{
#ifdef FSC_INTERRUPT_TRIGGERED
	g_Idle = FALSE;		// Run continuously (unmask all)
	Registers.Mask.byte = 0x20;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0x00;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 0;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
	platform_enable_timer(TRUE);
#endif
	platform_set_vbus_5v_enable(FALSE);	// Disable the 5V output...
	platform_set_vbus_lvl1_enable(FALSE);	// Disable the 12V output
	ConnState = AudioAccessory;	// Set the state machine variable to Audio.Accessory
	sourceOrSink = Source;
	Registers.Power.PWR = 0x7;	// Enable everything except internal oscillator
	DeviceWrite(regPower, 1, &Registers.Power.byte);	// Commit the power state
	Registers.Control.HOST_CUR = 0b11;	// Set host current to 330uA
	DeviceWrite(regControl0, 1, &Registers.Control.byte[0]);	// Commit host current
	Registers.Switches.byte[0] = 0xC4;	// Enable both pull-ups and measure on CC1
	DeviceWrite(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the switch state
	SinkCurrent = utccNone;	// Not used in accessories
	OverPDDebounce = T_TIMER_DISABLE;	// Disable PD filter timer
	StateTimer = T_TIMER_DISABLE;	// Disable the state timer, not used in this state
	PDDebounce = tCCDebounce;	// Once in this state, we are waiting for the lines to be stable for tCCDebounce before changing states
	CCDebounce = T_TIMER_DISABLE;	// Disable the 2nd level debouncing initially to force completion of a 1st level debouncing
	ToggleTimer = T_TIMER_DISABLE;	// Once we are in the audio.accessory state, we are going to stop toggling and only monitor CC1
	WriteStateLog(&TypeCStateLog, ConnState, Timer_tms, Timer_S);
}

CCTermType AlternateDecodeCCTerminationSource(void)
{
	CCTermType Termination = CCTypeUndefined;	// By default set it to undefined

	Registers.Measure.MDAC = MDAC_2P05V;	// Set up DAC threshold to 2.05V
	DeviceWrite(regMeasure, 1, &Registers.Measure.byte);	// Commit the DAC threshold
	platform_delay_10us(25);	// Delay to allow measurement to settle
	DeviceRead(regStatus0, 1, &Registers.Status.byte[4]);

	if (Registers.Status.COMP == 1) {
		Termination = CCTypeOpen;
		return Termination;
	} else if (Registers.Status.BC_LVL == 0) {
		Termination = CCTypeRa;
	} else if (Registers.Status.BC_LVL == 3) {
		Termination = CCTypeRd3p0;
	} else if (Registers.Status.BC_LVL == 2) {
		Termination = CCTypeRd1p5;
	} else {
		Termination = CCTypeRdUSB;
	}
	return Termination;	// Return the termination type
}
