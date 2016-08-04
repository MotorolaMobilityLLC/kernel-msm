/*********************************************************************
* FileName:        PDPolicy.h
* Dependencies:    See INCLUDES section below
* Processor:       PIC32
* Compiler:        XC32
* Company:         Fairchild Semiconductor
*
* Software License Agreement:
*
*The software supplied herewith by Fairchild Semiconductor(theCompany)
* is supplied to you, the Company's customer, for exclusive use with its
* USB Type C / USB PD products.  The software is owned by the Company and/or
* its supplier, and is protected under applicable copyright laws.
* All rights are reserved. Any use in violation of the foregoing restrictions
* may subject the user to criminal sanctions under applicable laws, as well
* as to civil liability for the breach of the terms and conditions of this
* license.
*
*THIS SOFTWARE IS PROVIDED IN AN AS IS CONDITION. NO WARRANTIES,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
* TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
* IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
* CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
*
********************************************************************/

#ifndef _PDPOLICY_H_
#define	_PDPOLICY_H_

/////////////////////////////////////////////////////////////////////////////
//                              Required headers
/////////////////////////////////////////////////////////////////////////////
#include "platform.h"
#include "PD_Types.h"

// EXTERNS
extern FSC_BOOL USBPDTxFlag;	// Flag to indicate that we need to send a message (set by device policy manager)
extern sopMainHeader_t PDTransmitHeader;	// Definition of the PD packet to send

#ifdef FSC_HAVE_SNK
extern sopMainHeader_t CapsHeaderSink;	// Definition of the sink capabilities of the device
extern doDataObject_t CapsSink[7];	// Power object definitions of the sink capabilities of the device

extern doDataObject_t SinkRequest;	// Sink request message
extern FSC_U32 SinkRequestMaxVoltage;	// Maximum voltage that the sink will request
extern FSC_U32 SinkRequestMaxPower;	// Maximum power the sink will request (used to calculate current as well)
extern FSC_U32 SinkRequestOpPower;	// Operating power the sink will request (used to calculate current as well)
extern FSC_BOOL SinkGotoMinCompatible;	// Whether the sink will respond to the GotoMin command
extern FSC_BOOL SinkUSBSuspendOperation;	// Whether the sink wants to continue operation during USB suspend
extern FSC_BOOL SinkUSBCommCapable;	// Whether the sink is USB communications capable
#endif // FSC_HAVE_SNK

#ifdef FSC_HAVE_SRC
extern sopMainHeader_t CapsHeaderSource;	// Definition of the source capabilities of the device
extern doDataObject_t CapsSource[7];	// Power object definitions of the source capabilities of the device
#endif // FSC_HAVE_SRC

extern sopMainHeader_t CapsHeaderReceived;	// Last capabilities header received (source or sink)
extern doDataObject_t PDTransmitObjects[7];	// Data objects to send
extern doDataObject_t CapsReceived[7];	// Last power objects received (source or sink)
extern doDataObject_t USBPDContract;	// Current USB PD contract (request object)

#ifdef FSC_DEBUG
extern FSC_BOOL SourceCapsUpdated;	// Flag to indicate whether we have updated the source caps (for the GUI)
#endif // FSC_DEBUG

extern PolicyState_t PolicyState;	// State variable for Policy Engine
extern FSC_U8 PolicySubIndex;	// Sub index for policy states
extern FSC_BOOL PolicyIsSource;	// Flag to indicate whether we are acting as a source or a sink
extern FSC_BOOL PolicyIsDFP;	// Flag to indicate whether we are acting as a UFP or DFP
extern FSC_BOOL PolicyHasContract;	// Flag to indicate whether there is a contract in place
extern FSC_U32 VbusTransitionTime;	// Time to wait for VBUS switch to transition

extern sopMainHeader_t PolicyRxHeader;	// Header object for USB PD messages received
extern sopMainHeader_t PolicyTxHeader;	// Header object for USB PD messages to send
extern doDataObject_t PolicyRxDataObj[7];	// Buffer for data objects received
extern doDataObject_t PolicyTxDataObj[7];	// Buffer for data objects to send

extern FSC_U32 NoResponseTimer;/* Policy engine no response timer*/
/*Multi-function timer for the different policy states*/
extern FSC_U32 PolicyStateTimer;
#ifdef FSC_HAVE_VDM
extern FSC_U32 VdmTimer;
extern FSC_BOOL VdmTimerStarted;
#endif // FSC_HAVE_VDM
extern ReqContextType coreReqCtx;
extern ReqContextType coreReqCurCtx;
/////////////////////////////////////////////////////////////////////////////
//                            LOCAL PROTOTYPES
/////////////////////////////////////////////////////////////////////////////
void PolicyTick(void);
void InitializePDPolicyVariables(void);
void USBPDEnable(FSC_BOOL DeviceUpdate, SourceOrSink TypeCDFP);
void USBPDDisable(FSC_BOOL DeviceUpdate);
void USBPDPolicyEngine(void);
void PolicyErrorRecovery(void);

#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void PolicySourceSendHardReset(void);
void PolicySourceSoftReset(void);
void PolicySourceSendSoftReset(void);
void PolicySourceStartup(void);
void PolicySourceDiscovery(void);
void PolicySourceSendCaps(void);
void PolicySourceDisabled(void);
void PolicySourceTransitionDefault(void);
void PolicySourceNegotiateCap(void);
void PolicySourceTransitionSupply(void);
void PolicySourceCapabilityResponse(void);
void PolicySourceReady(void);
void PolicySourceGiveSourceCap(void);
void PolicySourceGetSourceCap(void);
void PolicySourceGetSinkCap(void);
void PolicySourceSendPing(void);
void PolicySourceGotoMin(void);
void PolicySourceGiveSinkCap(void);
void PolicySourceSendDRSwap(void);
void PolicySourceEvaluateDRSwap(void);
void PolicySourceSendVCONNSwap(void);
void PolicySourceSendPRSwap(void);
void PolicySourceEvaluatePRSwap(void);
void PolicySourceWaitNewCapabilities(void);
void PolicySourceEvaluateVCONNSwap(void);
#endif // FSC_HAVE_SRC

#ifdef FSC_HAVE_SNK
void PolicySinkSendHardReset(void);
void PolicySinkSoftReset(void);
void PolicySinkSendSoftReset(void);
void PolicySinkTransitionDefault(void);
void PolicySinkStartup(void);
void PolicySinkDiscovery(void);
void PolicySinkWaitCaps(void);
void PolicySinkEvaluateCaps(void);
void PolicySinkSelectCapability(void);
void PolicySinkTransitionSink(void);
void PolicySinkReady(void);
void PolicySinkGiveSinkCap(void);
void PolicySinkGetSinkCap(void);
void PolicySinkGiveSourceCap(void);
void PolicySinkGetSourceCap(void);
void PolicySinkSendDRSwap(void);
void PolicySinkEvaluateDRSwap(void);
void PolicySinkEvaluateVCONNSwap(void);
void PolicySinkSendPRSwap(void);
void PolicySinkEvaluatePRSwap(void);
#endif

void PolicyInvalidState(void);
void policyBISTReceiveMode(void);
void policyBISTFrameReceived(void);
void policyBISTCarrierMode2(void);
void policyBISTTestData(void);

FSC_BOOL PolicySendHardReset(PolicyState_t nextState, FSC_U32 delay);
FSC_U8 PolicySendCommand(FSC_U8 Command, PolicyState_t nextState,
			 FSC_U8 subIndex);
FSC_U8 PolicySendCommandNoReset(FSC_U8 Command, PolicyState_t nextState,
				FSC_U8 subIndex);
FSC_U8 PolicySendData(FSC_U8 MessageType, FSC_U8 NumDataObjects,
		      doDataObject_t * DataObjects, PolicyState_t nextState,
		      FSC_U8 subIndex, SopType sop);
FSC_U8 PolicySendDataNoReset(FSC_U8 MessageType, FSC_U8 NumDataObjects,
			     doDataObject_t * DataObjects,
			     PolicyState_t nextState, FSC_U8 subIndex);
void UpdateCapabilitiesRx(FSC_BOOL IsSourceCaps);

void processDMTBIST(void);

#ifdef FSC_HAVE_VDM
// shim functions for VDM code
void InitializeVdmManager(void);
void convertAndProcessVdmMessage(SopType sop);
void sendVdmMessage(SopType sop, FSC_U32 * arr, FSC_U32 length,
		    PolicyState_t next_ps);
void doVdmCommand(void);
void doDiscoverIdentity(void);
void doDiscoverSvids(void);
void PolicyGiveVdm(void);
void PolicyVdm(void);
void autoVdmDiscovery(void);
void requestCurLimit(FSC_U16 currentLimit);
#endif // FSC_HAVE_VDM

SopType TokenToSopType(FSC_U8 data);

#ifdef FSC_DEBUG
#if defined(FSC_HAVE_SRC) || (defined(FSC_HAVE_SNK) && defined(FSC_HAVE_ACCMODE))
void WriteSourceCapabilities(FSC_U8 * abytData);
void ReadSourceCapabilities(FSC_U8 * abytData);
#endif // FSC_HAVE_SRC

#ifdef FSC_HAVE_SNK
void WriteSinkCapabilities(FSC_U8 * abytData);
void WriteSinkRequestSettings(FSC_U8 * abytData);
void ReadSinkRequestSettings(FSC_U8 * abytData);
#endif // FSC_HAVE_SNK

void ReadSinkCapabilities(FSC_U8 * abytData);

void EnableUSBPD(void);
void DisableUSBPD(void);
FSC_BOOL GetPDStateLog(FSC_U8 * data);
void ProcessReadPDStateLog(FSC_U8 * MsgBuffer, FSC_U8 * retBuffer);
void ProcessPDBufferRead(FSC_U8 * MsgBuffer, FSC_U8 * retBuffer);

#endif // FSC_DEBUG

void SetVbusTransitionTime(FSC_U32 time_ms);

#endif /* _PDPOLICY_H_ */
