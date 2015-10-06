/*********************************************************************
 * FileName:        TypeC.h
 * Dependencies:    See INCLUDES section below
 * Processor:       PIC32
 * Compiler:        XC32
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
 * The software supplied herewith by Fairchild Semiconductor (the “Company”)
 * is supplied to you, the Company's customer, for exclusive use with its
 * USB Type C / USB PD products.  The software is owned by the Company and/or
 * its supplier, and is protected under applicable copyright laws.
 * All rights are reserved. Any use in violation of the foregoing restrictions
 * may subject the user to criminal sanctions under applicable laws, as well
 * as to civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 ********************************************************************/

#ifndef __FSC_TYPEC_H__
#define	__FSC_TYPEC_H__

/////////////////////////////////////////////////////////////////////////////
//                              Required headers
/////////////////////////////////////////////////////////////////////////////
#include "platform.h"
#include "fusb30X.h"
#include "PDPolicy.h"
#include "PDProtocol.h"
#include "TypeC_Types.h"

// Type C Timing Parameters
// Units are in ms * 10 to be ticked by a 0.1ms timer.
#define tAMETimeout     900 * 10
#define tCCDebounce     120 * 10
#define tPDDebounce     15 * 10
#define tDRPTry         125 * 10
#define tDRPTryWait     600 * 10
#define tErrorRecovery  30 * 10

#define tDeviceToggle   3 * 10	// Duration in ms to wait before checking other CC pin for the device
#define tTOG2           30 * 10	//When TOGGLE=1, time at which internal versions of PU_EN1=1 or PU_EN2=1 and PWDN1=PDWN2=0 selected to present externally as a DFP in the DRP toggle

#define T_TIMER_DISABLE (0xFFFF)
#define SLEEP_DELAY     2

typedef enum {
	Source = 0,
	Sink
} SourceOrSink;

// EXTERNS
extern DeviceReg_t Registers;	// Variable holding the current status of the device registers
extern BOOL USBPDActive;	// Variable to indicate whether the USB PD state machine is active or not
extern BOOL USBPDEnabled;	// Variable to indicate whether USB PD is enabled (by the host)
extern UINT32 PRSwapTimer;	// Timer used to bail out of a PR_Swap from the Type-C side if necessary
extern USBTypeCPort PortType;	// Variable indicating which type of port we are implementing
extern BOOL blnCCPinIsCC1;	// Flag to indicate if the CC1 pin has been detected as the CC pin
extern BOOL blnCCPinIsCC2;	// Flag to indicate if the CC2 pin has been detected as the CC pin
extern BOOL blnSMEnabled;	// Variable to indicate whether the 300 state machine is enabled
extern ConnectionState ConnState;	// Variable indicating the current Type-C connection state
extern BOOL IsHardReset;	// Variable indicating that a Hard Reset is occurring
extern BOOL PolicyHasContract;	// Indicates that policy layer has a PD contract

/////////////////////////////////////////////////////////////////////////////
//                            LOCAL PROTOTYPES
/////////////////////////////////////////////////////////////////////////////
void TypeCTickAt100us(void);
void LogTickAt100us(void);

void InitializeRegisters(void);
void InitializeTypeCInterrupt(BOOL blnEnable);
void InitializeTypeCVariables(void);
void InitializeTypeC(void);
void DisableTypeCStateMachine(void);
void EnableTypeCStateMachine(void);
void StateMachineTypeC(void);
void StateMachineDisabled(void);
void StateMachineErrorRecovery(void);
void StateMachineDelayUnattached(void);
void StateMachineUnattached(void);
void StateMachineAttachWaitSink(void);
void StateMachineAttachWaitSource(void);
void StateMachineAttachWaitAccessory(void);
void StateMachineAttachedSink(void);
void StateMachineAttachedSource(void);
void StateMachineTryWaitSink(void);
void StateMachineTrySource(void);
void StateMachineDebugAccessory(void);
void StateMachineAudioAccessory(void);
void StateMachinePoweredAccessory(void);
void StateMachineUnsupportedAccessory(void);
void stateMachineTrySink(void);
void stateMachineTryWaitSource(void);
void stateMachineUnattachedSource(void);	// Temporary software workaround for entering DRP in Source mode - not part of GUI
void SetStateDisabled(void);
void SetStateErrorRecovery(void);
void SetStateDelayUnattached(void);
void SetStateUnattached(void);
void SetStateAttachWaitSink(void);
void SetStateAttachWaitSource(void);
void SetStateAttachWaitAccessory(void);
void SetStateAttachedSource(void);
void SetStateAttachedSink(void);
void RoleSwapToAttachedSink(void);
void RoleSwapToAttachedSource(void);
void SetStateTryWaitSink(void);
void SetStateTrySource(void);
void SetStateDebugAccessory(void);
void SetStateAudioAccessory(void);
void SetStatePoweredAccessory(void);
void SetStateUnsupportedAccessory(void);
void SetStateTrySink(void);
void SetStateTryWaitSource(void);
void SetStateUnattachedSource(void);	// Temporary software workaround for entering DRP in Source mode - not part of GUI
void updateSourceCurrent(void);
void updateSourceMDACHigh(void);
void updateSourceMDACLow(void);
void ToggleMeasureCC1(void);
void ToggleMeasureCC2(void);
CCTermType DecodeCCTermination(void);
CCTermType DecodeCCTerminationSource(void);
CCTermType DecodeCCTerminationSink(void);
void UpdateSinkCurrent(CCTermType Termination);
void ConfigurePortType(UINT8 Control);
void UpdateCurrentAdvert(UINT8 Current);
void GetDeviceTypeCStatus(UINT8 abytData[]);
UINT8 GetTypeCSMControl(void);
UINT8 GetCCTermination(void);
BOOL VbusVSafe0V(void);
BOOL VbusUnder5V(void);
void DetectCCPinSource(void);
void DetectCCPinSink(void);
void resetDebounceVariables(void);
void setDebounceVariablesCC1(CCTermType term);
void setDebounceVariablesCC2(CCTermType term);
void debounceCC(void);
void debounceSink(void);
void peekCC1Source(void);
void peekCC2Source(void);
void peekCC1Sink(void);
void peekCC2Sink(void);
void checkForAccessory(void);

BOOL GetLocalRegisters(UINT8 * data, INT32 size);	//Returns local registers as data array
BOOL GetStateLog(UINT8 * data);	// Returns up to last 12 logs

void setAlternateModes(UINT8 mode);
UINT8 getAlternateModes(void);

void ProcessTypeCPDStatus(UINT8 * MsgBuffer, UINT8 * retBuffer);
void ProcessTypeCPDControl(UINT8 * MsgBuffer, UINT8 * retBuffer);
void ProcessLocalRegisterRequest(UINT8 * MsgBuffer, UINT8 * retBuffer);

void ProcessSetTypeCState(UINT8 * MsgBuffer, UINT8 * retBuffer);
void ProcessReadTypeCStateLog(UINT8 * MsgBuffer, UINT8 * retBuffer);

#endif /* __FSC_TYPEC_H__ */
