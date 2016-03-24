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
/*Units are in ms  to be ticked by a 1ms timer.*/
#define tAMETimeout     (900 * 1)
#define tCCDebounce     (120 * 1)
#define tPDDebounce     (15 * 1)
#define tDRPTry         (125 * 1)
#define tDRPTryWait     (600 * 1)
#define tErrorRecovery  (30 * 1)
/*Duration in ms to wait before checking other CC pin for the device*/
#define tDeviceToggle   (3 * 1)
/*
*When TOGGLE=1, time at which internal versions of PU_EN1=1
*or PU_EN2=1 and PWDN1=PDWN2=0 selected to present externally
*as a DFP in the DRP toggle
*/
#define tTOG2           (30 * 1)


#define T_TIMER_DISABLE (0xFFFF)
#define SLEEP_DELAY     100

typedef enum {
	Source = 0,
	Sink
} SourceOrSink;

// EXTERNS
extern DeviceReg_t Registers;	// Variable holding the current status of the device registers
extern FSC_BOOL USBPDActive;	// Variable to indicate whether the USB PD state machine is active or not
extern FSC_BOOL USBPDEnabled;	// Variable to indicate whether USB PD is enabled (by the host)
extern FSC_U32 PRSwapTimer;	// Timer used to bail out of a PR_Swap from the Type-C side if necessary
extern USBTypeCPort PortType;	// Variable indicating which type of port we are implementing
extern FSC_BOOL blnCCPinIsCC1;	// Flag to indicate if the CC1 pin has been detected as the CC pin
extern FSC_BOOL blnCCPinIsCC2;	// Flag to indicate if the CC2 pin has been detected as the CC pin
extern FSC_BOOL blnSMEnabled;	// Variable to indicate whether the 300 state machine is enabled
extern ConnectionState ConnState;	// Variable indicating the current Type-C connection state
extern FSC_BOOL IsHardReset;	// Variable indicating that a Hard Reset is occurring
extern FSC_BOOL IsPRSwap;
extern FSC_BOOL PolicyHasContract;	// Indicates that policy layer has a PD contract
extern PolicyState_t PolicyState;
extern FSC_BOOL gChargerAuthenticated;
extern FSC_U32 gChargerMaxCurrent;
extern FSC_U32 gRequestOpCurrent;
extern FSC_U32 gChargerOpCurrent;
extern struct power_supply usbc_psy;
extern struct power_supply switch_psy;
extern u16 SwitchState;
/////////////////////////////////////////////////////////////////////////////
//                            LOCAL PROTOTYPES
/////////////////////////////////////////////////////////////////////////////
void TypeCTickAt100us(void);

void InitializeRegisters(void);
void InitializeTypeCVariables(void);
void InitializeTypeC(void);

void DisableTypeCStateMachine(void);
void EnableTypeCStateMachine(void);

void StateMachineTypeC(void);
void StateMachineDisabled(void);
void StateMachineErrorRecovery(void);
void StateMachineDelayUnattached(void);
void StateMachineUnattached(void);

#ifdef FSC_HAVE_SNK
void StateMachineAttachWaitSink(void);
void StateMachineAttachedSink(void);
void StateMachineTryWaitSink(void);
void stateMachineTrySink(void);
#endif // FSC_HAVE_SNK

#ifdef FSC_HAVE_SRC
void StateMachineAttachWaitSource(void);
void stateMachineTryWaitSource(void);
void stateMachineUnattachedSource(void);	// Temporary software workaround for entering DRP in Source mode - not part of GUI
void StateMachineAttachedSource(void);
void StateMachineTrySource(void);
#endif // FSC_HAVE_SRC

#ifdef FSC_HAVE_ACCMODE
void StateMachineAttachWaitAccessory(void);
void StateMachineDebugAccessory(void);
void StateMachineAudioAccessory(void);
void StateMachinePoweredAccessory(void);
void StateMachineUnsupportedAccessory(void);
#endif // FSC_HAVE_ACCMODE

void SetStateErrorRecovery(void);
void SetStateDelayUnattached(void);
void SetStateUnattached(void);

#ifdef FSC_HAVE_SNK
void SetStateAttachWaitSink(void);
void SetStateAttachedSink(void);
#ifdef FSC_HAVE_DRP
void RoleSwapToAttachedSink(void);
#endif // FSC_HAVE_DRP
void SetStateTryWaitSink(void);
void SetStateTrySink(void);
#endif // FSC_HAVE_SNK

#ifdef FSC_HAVE_SRC
void SetStateAttachWaitSource(void);
void SetStateAttachedSource(void);
#ifdef FSC_HAVE_DRP
void RoleSwapToAttachedSource(void);
#endif // FSC_HAVE_DRP
void SetStateTrySource(void);
void SetStateTryWaitSource(void);
void SetStateUnattachedSource(void);	// Temporary software workaround for entering DRP in Source mode - not part of GUI
#endif // FSC_HAVE_SRC

#ifdef FSC_HAVE_ACCMODE
void SetStateAttachWaitAccessory(void);
void SetStateDebugAccessory(void);
void SetStateAudioAccessory(void);
void SetStatePoweredAccessory(void);
void SetStateUnsupportedAccessory(void);
#endif // FSC_HAVE_ACCMODE

void updateSourceCurrent(void);
void updateSourceMDACHigh(void);
void updateSourceMDACLow(void);

void ToggleMeasureCC1(void);
void ToggleMeasureCC2(void);

CCTermType DecodeCCTermination(void);
#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
CCTermType DecodeCCTerminationSource(void);
#endif // FSC_HAVE_SRC || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE)
#ifdef FSC_HAVE_SNK
CCTermType DecodeCCTerminationSink(void);
#endif // FSC_HAVE_SNK

void UpdateSinkCurrent(CCTermType Termination);
FSC_BOOL VbusVSafe0V(void);

#ifdef FSC_HAVE_SNK
FSC_BOOL VbusUnder5V(void);
#endif // FSC_HAVE_SNK

FSC_BOOL isVSafe5V(void);
FSC_BOOL isVBUSOverVoltage(FSC_U8 vbusMDAC);
void resetDebounceVariables(void);
void setDebounceVariablesCC1(CCTermType term);
void setDebounceVariablesCC2(CCTermType term);
void debounceCC(void);

#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void DetectCCPinSource(void);
void peekCC1Source(void);
void peekCC2Source(void);
#endif // FSC_HAVE_SRC || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE)

#ifdef FSC_HAVE_SNK
void DetectCCPinSink(void);
void peekCC1Sink(void);
void peekCC2Sink(void);
#endif // FSC_HAVE_SNK

#ifdef FSC_HAVE_ACCMODE
void checkForAccessory(void);
#endif // FSC_HAVE_ACCMODE

#ifdef FSC_DEBUG
void LogTickAt100us(void);
void SetStateDisabled(void);

void ConfigurePortType(FSC_U8 Control);
void UpdateCurrentAdvert(FSC_U8 Current);
void GetDeviceTypeCStatus(FSC_U8 abytData[]);
FSC_U8 GetTypeCSMControl(void);
FSC_U8 GetCCTermination(void);

FSC_BOOL GetLocalRegisters(FSC_U8 * data, FSC_S32 size);	//Returns local registers as data array
FSC_BOOL GetStateLog(FSC_U8 * data);	// Returns up to last 12 logs

void ProcessTypeCPDStatus(FSC_U8 * MsgBuffer, FSC_U8 * retBuffer);
void ProcessTypeCPDControl(FSC_U8 * MsgBuffer, FSC_U8 * retBuffer);
void ProcessLocalRegisterRequest(FSC_U8 * MsgBuffer, FSC_U8 * retBuffer);
void ProcessSetTypeCState(FSC_U8 * MsgBuffer, FSC_U8 * retBuffer);
void ProcessReadTypeCStateLog(FSC_U8 * MsgBuffer, FSC_U8 * retBuffer);
void setAlternateModes(FSC_U8 mode);
FSC_U8 getAlternateModes(void);
#endif // FSC_DEBUG
void StateMachineTypeCImp(void);
void WakeStateMachineTypeC(void);
#endif /* __FSC_TYPEC_H__ */
