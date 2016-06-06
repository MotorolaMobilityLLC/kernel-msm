/*********************************************************************
* FileName:        PDProtocol.h
* Dependencies:    See INCLUDES section below
* Processor:       PIC32
* Compiler:        XC32
* Company:         Fairchild Semiconductor
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

#ifndef _PDPROTOCOL_H_
#define	_PDPROTOCOL_H_

/////////////////////////////////////////////////////////////////////////////
//                              Required headers
/////////////////////////////////////////////////////////////////////////////
#include "platform.h"
#include "PD_Types.h"

#ifdef FSC_DEBUG
#include "Log.h"
#endif // FSC_DEBUG

// EXTERNS
extern ProtocolState_t ProtocolState;	// State variable for Protocol Layer
extern PDTxStatus_t PDTxStatus;	// Status variable for current transmission
extern FSC_BOOL ProtocolMsgRx;	// Flag to indicate if we have received a packet
extern SopType ProtocolMsgRxSop;	// SOP type of message received

#ifdef FSC_DEBUG
extern StateLog PDStateLog;
#endif // FSC_DEBUG

// Device FIFO Token Definitions
#define TXON                    0xA1
#define SOP1                    0x12	// TODO - SOPn and SYNCn_TOKEN appear to
#define SOP2                    0x13	//        be repeats???
#define SOP3                    0x1B
#define SYNC1_TOKEN             0x12
#define SYNC2_TOKEN             0x13
#define SYNC3_TOKEN             0x1B
#define RESET1                  0x15
#define RESET2                  0x16
#define PACKSYM                 0x80
#define JAM_CRC                 0xFF
#define EOP                     0x14
#define TXOFF                   0xFE

/////////////////////////////////////////////////////////////////////////////
//                            LOCAL PROTOTYPES
/////////////////////////////////////////////////////////////////////////////
void InitializePDProtocolVariables(void);
void UpdateCapabilitiesRx(FSC_BOOL IsSourceCaps);
void USBPDProtocol(void);
void ProtocolIdle(void);
void ProtocolResetWait(void);
void ProtocolRxWait(void);
void ProtocolGetRxPacket(void);
void ProtocolTransmitMessage(void);
void ProtocolSendingMessage(void);
void ProtocolWaitForPHYResponse(void);
void ProtocolVerifyGoodCRC(void);
void ProtocolSendGoodCRC(SopType sop);
void ProtocolLoadSOP(void);
void ProtocolLoadEOP(void);
void ProtocolSendHardReset(void);
void ProtocolFlushRxFIFO(void);
void ProtocolFlushTxFIFO(void);
void ResetProtocolLayer(FSC_BOOL ResetPDLogic);

#ifdef FSC_DEBUG
FSC_BOOL StoreUSBPDToken(FSC_BOOL transmitter, USBPD_BufferTokens_t token);
FSC_BOOL StoreUSBPDMessage(sopMainHeader_t Header, doDataObject_t * DataObject,
			   FSC_BOOL transmitter, FSC_U8 SOPType);
FSC_U8 GetNextUSBPDMessageSize(void);
FSC_U8 GetUSBPDBufferNumBytes(void);
FSC_BOOL ClaimBufferSpace(FSC_S32 intReqSize);
void GetUSBPDStatus(FSC_U8 abytData[]);
FSC_U8 GetUSBPDStatusOverview(void);
FSC_U8 ReadUSBPDBuffer(FSC_U8 * pData, FSC_U8 bytesAvail);
void SendUSBPDMessage(FSC_U8 * abytData);
void SendUSBPDHardReset(void);

void manualRetriesTakeTwo(void);
void setManualRetries(FSC_U8 mode);
FSC_U8 getManualRetries(void);

#endif // FSC_DEBUG

#endif /* _PDPROTOCOL_H_ */
