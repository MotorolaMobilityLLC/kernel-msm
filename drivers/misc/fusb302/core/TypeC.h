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
// Units are in ms * 1 to be ticked by a 1ms timer.
#define tAMETimeout     900 * 1
#define tCCDebounce     120 * 1
#define tPDDebounce     15 * 1
#define tDRPTry         90 * 1
#define tDRPTryWait     600 * 1
#define tErrorRecovery  30 * 1

#define tDeviceToggle   3 * 1	// Duration in ms to wait before checking other CC pin for the device
#define tTOG2           30 * 1	//When TOGGLE=1, time at which internal versions of PU_EN1=1 or PU_EN2=1 and PWDN1=PDWN2=0 selected to present externally as a DFP in the DRP toggle
#define tIllegalCable   150 * 1
#define tOrientedDebug  100 * 1	// Time alloted for detection DegugAccessory orientation

#define T_TIMER_DISABLE (0xFFFF)
#define SLEEP_DELAY     80	// *10 us
#define MAX_CABLE_LOOP  2

// EXTERNS
extern DeviceReg_t Registers;	// Variable holding the current status of the device registers
extern FSC_BOOL USBPDActive;	// Variable to indicate whether the USB PD state machine is active or not
extern FSC_BOOL USBPDEnabled;	// Variable to indicate whether USB PD is enabled (by the host)
extern USBTypeCPort PortType;	// Variable indicating which type of port we are implementing
extern FSC_BOOL blnCCPinIsCC1;	// Flag to indicate if the CC1 pin has been detected as the CC pin
extern FSC_BOOL blnCCPinIsCC2;	// Flag to indicate if the CC2 pin has been detected as the CC pin
extern FSC_BOOL blnSMEnabled;	// Variable to indicate whether the 300 state machine is enabled
extern ConnectionState ConnState;	// Variable indicating the current Type-C connection state
extern FSC_BOOL IsHardReset;	// Variable indicating that a Hard Reset is occurring
extern FSC_BOOL IsPRSwap;
extern FSC_BOOL PolicyHasContract;	// Indicates that policy layer has a PD contract
extern PolicyState_t PolicyState;
extern FSC_BOOL mode_entered;
/////////////////////////////////////////////////////////////////////////////
//                            LOCAL PROTOTYPES
/////////////////////////////////////////////////////////////////////////////
void TypeCTick(void);

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
#endif // FSC_HAVE_SNK

#if (defined(FSC_HAVE_DRP) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)))
void stateMachineTrySink(void);
#endif /* (defined(FSC_HAVE_DRP) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))) */

#ifdef FSC_HAVE_SRC
void StateMachineAttachWaitSource(void);
void stateMachineTryWaitSource(void);
void stateMachineUnattachedSource(void);	// Temporary software workaround for entering DRP in Source mode - not part of GUI
void StateMachineAttachedSource(void);
void StateMachineTrySource(void);
#endif // FSC_HAVE_SRC

#ifdef FSC_HAVE_ACCMODE
void StateMachineAttachWaitAccessory(void);
void StateMachineDebugAccessorySource(void);
void StateMachineAudioAccessory(void);
void StateMachinePoweredAccessory(void);
void StateMachineUnsupportedAccessory(void);
#ifdef FSC_HAVE_SNK
void StateMachineDebugAccessorySink(void);
#endif /* FSC_HAVE_SNK */
#endif // FSC_HAVE_ACCMODE

#ifdef FSC_DTS
void StateMachineAttachWaitDebugSink(void);
void StateMachineAttachedDebugSink(void);
void StateMachineAttachWaitDebugSource(void);
void StateMachineAttachedDebugSource(void);
void StateMachineTryDebugSource(void);
void StateMachineTryWaitDebugSink(void);
void StateMachineUnattachedDebugSource(void);
#endif /* FSC_DTS */

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
#endif // FSC_HAVE_SNK

#if (defined(FSC_HAVE_DRP) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)))
void SetStateTrySink(void);
#endif /* (defined(FSC_HAVE_DRP) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))) */

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
void SetStateDebugAccessorySource(void);
void SetStateAudioAccessory(void);
void SetStateDebugAccessorySink(void);
#endif // FSC_HAVE_ACCMODE

#if (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void SetStateAttachWaitAccessory(void);
void SetStateUnsupportedAccessory(void);
void SetStatePoweredAccessory(void);
#endif /* (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE)) */

#ifdef FSC_DTS
void SetStateAttachWaitDebugSink(void);
void SetStateAttachedDebugSink(void);
void SetStateAttachWaitDebugSource(void);
void SetStateAttachedDebugSource(void);
void SetStateTryDebugSource(void);
void SetStateTryWaitDebugSink(void);
void SetStateUnattachedDebugSource(void);
#endif /* FSC_DTS */

void updateSourceCurrent(void);
void updateSourceMDACHigh(void);
void updateSourceMDACLow(void);

void ToggleMeasure(void);

CCTermType DecodeCCTermination(void);
#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
CCTermType DecodeCCTerminationSource(void);
#endif // FSC_HAVE_SRC || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE)
#ifdef FSC_HAVE_SNK
CCTermType DecodeCCTerminationSink(void);
#endif // FSC_HAVE_SNK

void UpdateSinkCurrent(void);
FSC_BOOL VbusVSafe0V(void);

#ifdef FSC_HAVE_SNK
FSC_BOOL VbusUnder5V(void);
#endif // FSC_HAVE_SNK

FSC_BOOL isVSafe5V(void);
FSC_BOOL isVBUSOverVoltage(FSC_U8 vbusMDAC);
void resetDebounceVariables(void);
void setDebounceVariables(CCTermType term);
void setDebounceVariables(CCTermType term);
void debounceCC(void);

#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void setStateSource(FSC_BOOL vconn);
void DetectCCPinSource(void);
void updateVCONNSource(void);
void updateVCONNSource(void);
#endif // FSC_HAVE_SRC || (FSC_HAVE_SNK && FSC_HAVE_ACCMODE)

#ifdef FSC_HAVE_SNK
void setStateSink(void);
void DetectCCPinSink(void);
void updateVCONNSource(void);
void updateVCONNSink(void);
#endif // FSC_HAVE_SNK

void clearState(void);

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

void toggleCurrentSwap(void);

#endif /* __FSC_TYPEC_H__ */
