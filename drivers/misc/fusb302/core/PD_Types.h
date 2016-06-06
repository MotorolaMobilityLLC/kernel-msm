#ifndef __USBPD_TYPES_H__
#define __USBPD_TYPES_H__
#include <linux/completion.h>
#include <linux/atomic.h>
#include "platform.h"

#ifdef FSC_DEBUG
#define PDBUFSIZE               128
#endif // FSC_DEBUG

#define USBPDSPECREV            1	// Revision 1.0

#define STAT_BUSY               0
#define STAT_SUCCESS            1
#define STAT_ERROR              2

// Device FIFO Token Definitions
#define TXON                    0xA1
#define SYNC1_TOKEN             0x12
#define SYNC2_TOKEN             0x13
#define SYNC3_TOKEN             0x1B
#define RESET1                  0x15
#define RESET2                  0x16
#define PACKSYM                 0x80
#define JAM_CRC                 0xFF
#define EOP                     0x14
#define TXOFF                   0xFE

// USB PD Header Message Definitions
#define PDPortRoleSink          0
#define PDPortRoleSource        1
#define PDSpecRev1p0            0
#define PDSpecRev2p0            1
#define PDDataRoleUFP           0
#define PDDataRoleDFP           1
#define PDCablePlugSource       0
#define PDCablePlugPlug         1

// USB PD Control Message Types
#define CMTGoodCRC              0b0001
#define CMTGotoMin              0b0010
#define CMTAccept               0b0011
#define CMTReject               0b0100
#define CMTPing                 0b0101
#define CMTPS_RDY               0b0110
#define CMTGetSourceCap         0b0111
#define CMTGetSinkCap           0b1000
#define CMTDR_Swap              0b1001
#define CMTPR_Swap              0b1010
#define CMTVCONN_Swap           0b1011
#define CMTWait                 0b1100
#define CMTSoftReset            0b1101

// USB PD Data Message Types
#define DMTSourceCapabilities   0b0001
#define DMTRequest              0b0010
#define DMTBIST                 0b0011
#define DMTSinkCapabilities     0b0100
#define DMTVenderDefined        0b1111

// BIST Data Objects
#define BDO_BIST_Receiver_Mode  0b0000	// Not Implemented
#define BDO_BIST_Transmit_Mode  0b0001	// Not Implemented
#define BDO_Returned_BIST_Counters  0b0010	// Not Implemented
#define BDO_BIST_Carrier_Mode_0 0b0011	// Not Implemented
#define BDO_BIST_Carrier_Mode_1 0b0100	// Not Implemented
#define BDO_BIST_Carrier_Mode_2 0b0101	// Implemented
#define BDO_BIST_Carrier_Mode_3 0b0110	// Not Implemented
#define BDO_BIST_Eye_Pattern    0b0111	// Not Implemented
#define BDO_BIST_Test_Data      0b1000	// Implemented

// USB PD Timing Parameters
/*Units are in ms to be ticked by a 1ms timer.*/
#define TICK_SCALE_TO_MS        1

#define tNoResponse             (5000    * TICK_SCALE_TO_MS)
#define tSenderResponse         (25      * TICK_SCALE_TO_MS)
#define tTypeCSendSourceCap     (150     * TICK_SCALE_TO_MS)
#define tSinkWaitCap            (500     * TICK_SCALE_TO_MS)
#define tTypeCSinkWaitCap       (500     * TICK_SCALE_TO_MS)
#define tSrcTransition          (30      * TICK_SCALE_TO_MS)
#define tPSHardReset            (30      * TICK_SCALE_TO_MS)
#define tPSHardResetMax         (35      * TICK_SCALE_TO_MS)
#define tPSTransition           (500     * TICK_SCALE_TO_MS)
#define tPSSourceOff            (835     * TICK_SCALE_TO_MS)
#define tPSSourceOn             (435     * TICK_SCALE_TO_MS)
#define tVCONNSourceOn          (90      * TICK_SCALE_TO_MS)
#define tBISTContMode           (50      * TICK_SCALE_TO_MS)
#define tSwapSourceStart        (25      * TICK_SCALE_TO_MS)
#define tSrcRecover             (675     * TICK_SCALE_TO_MS)
#define tSrcRecoverMax          (1000    * TICK_SCALE_TO_MS)
#define tGoodCRCDelay           (1       * TICK_SCALE_TO_MS)
#define t5To12VTransition       (8       * TICK_SCALE_TO_MS)
#define tSafe0V                 (650     * TICK_SCALE_TO_MS)
#define tSrcTurnOn              (275     * TICK_SCALE_TO_MS)
#define tFPF2498Transition      (20      * TICK_SCALE_TO_MS)
#define tSourceRiseTimeout      (350     * TICK_SCALE_TO_MS)
#define tHardResetOverhead      (0       * TICK_SCALE_TO_MS)

#define nHardResetCount         2
#define nRetryCount             3
#define nCapsCount              50

typedef union {
	FSC_U16 word;
	FSC_U8 byte[2] __PACKED;

	struct {
		// Switches0
		unsigned MessageType:4;	// 0-3      : Message Type
		unsigned Reserved0:1;	// 4        : Reserved
		unsigned PortDataRole:1;	// 5        : Port Data Role
		unsigned SpecRevision:2;	// 6-7      : Specification Revision
		unsigned PortPowerRole:1;	// 8        : Port Power Role
		unsigned MessageID:3;	// 9-11     : Message ID
		unsigned NumDataObjects:3;	// 12-14    : Number of Data Objects
		unsigned Reserved1:1;	// 15       : Reserved
	};
} sopMainHeader_t;

typedef union {
	FSC_U16 word;
	FSC_U8 byte[2] __PACKED;
	struct {
		// Switches0
		unsigned MessageType:4;	// 0-3      : Message Type
		unsigned Reserved0:1;	// 4        : Reserved
		unsigned Reserved1:1;	// 5        : Reserved
		unsigned SpecRevision:2;	// 6-7      : Specification Revision
		unsigned CablePlug:1;	// 8        : Cable Plug
		unsigned MessageID:3;	// 9-11     : Message ID
		unsigned NumDataObjects:3;	// 12-14    : Number of Data Objects
		unsigned Reserved2:1;	// 15       : Reserved
	};
} sopPlugHeader_t;

typedef union {
	FSC_U32 object;
	FSC_U16 word[2] __PACKED;
	FSC_U8 byte[4] __PACKED;
	struct {
		unsigned:30;
		unsigned SupplyType:2;
	} PDO;			// General purpose Power Data Object
	struct {
		unsigned MaxCurrent:10;	// Max current in 10mA units
		unsigned Voltage:10;	// Voltage in 50mV units
		unsigned PeakCurrent:2;	// Peak current (divergent from Ioc ratings)
		unsigned Reserved:3;	// Reserved
		unsigned DataRoleSwap:1;	// Data role swap (supports DR_Swap message)
		unsigned USBCommCapable:1;	// USB communications capable (is communication available)
		unsigned ExternallyPowered:1;	// Externally powered (set when AC supply is supporting 100% of power)
		unsigned USBSuspendSupport:1;	// USB Suspend Supported
		unsigned DualRolePower:1;	// Dual-Role power (supports PR_Swap message)
		unsigned SupplyType:2;	// (Fixed Supply)
	} FPDOSupply;		// Fixed Power Data Object for Supplies
	struct {
		unsigned OperationalCurrent:10;	// Operational current in 10mA units
		unsigned Voltage:10;	// Voltage in 50mV units
		unsigned Reserved:5;	// Reserved
		unsigned DataRoleSwap:1;	// Data role swap (supports DR_Swap message)
		unsigned USBCommCapable:1;	// USB communications capable (is communication available)
		unsigned ExternallyPowered:1;	// Externally powered (set when AC supply is supporting 100% of power)
		unsigned HigherCapability:1;	// Whether the sink needs more than vSafe5V to provide full functionality
		unsigned DualRolePower:1;	// Dual-Role power (supports PR_Swap message)
		unsigned SupplyType:2;	// (Fixed Supply)
	} FPDOSink;		// Fixed Power Data Object for Sinks
	struct {
		unsigned MaxCurrent:10;	// Max current in 10mA units
		unsigned MinVoltage:10;	// Minimum Voltage in 50mV units
		unsigned MaxVoltage:10;	// Maximum Voltage in 50mV units
		unsigned SupplyType:2;	// (Variable Supply)
	} VPDO;			// Variable Power Data Object
	struct {
		unsigned MaxPower:10;	// Max power in 250mW units
		unsigned MinVoltage:10;	// Minimum Voltage in 50mV units
		unsigned MaxVoltage:10;	// Maximum Voltage in 50mV units
		unsigned SupplyType:2;	// (Battery Supply)
	} BPDO;			// Battery Power Data Object
	struct {
		unsigned MinMaxCurrent:10;	// Min/Max current in 10mA units
		unsigned OpCurrent:10;	// Operating current in 10mA units
		unsigned Reserved0:4;	// Reserved - set to zero
		unsigned NoUSBSuspend:1;	// Set when the sink wants to continue the contract during USB suspend (i.e. charging battery)
		unsigned USBCommCapable:1;	// USB communications capable (is communication available)
		unsigned CapabilityMismatch:1;	// Set if the sink cannot satisfy its power requirements from capabilities offered
		unsigned GiveBack:1;	// Whether the sink will respond to the GotoMin message
		unsigned ObjectPosition:3;	// Which object in the source capabilities is being requested
		unsigned Reserved1:1;	// Reserved - set to zero
	} FVRDO;		// Fixed/Variable Request Data Object
	struct {
		unsigned MinMaxPower:10;	// Min/Max power in 250mW units
		unsigned OpPower:10;	// Operating power in 250mW units
		unsigned Reserved0:4;	// Reserved - set to zero
		unsigned NoUSBSuspend:1;	// Set when the sink wants to continue the contract during USB suspend (i.e. charging battery)
		unsigned USBCommCapable:1;	// USB communications capable (is communication available)
		unsigned CapabilityMismatch:1;	// Set if the sink cannot satisfy its power requirements from capabilities offered
		unsigned GiveBack:1;	// Whether the sink will respond to the GotoMin message
		unsigned ObjectPosition:3;	// Which object in the source capabilities is being requested
		unsigned Reserved1:1;	// Reserved - set to zero
	} BRDO;			// Battery Request Data Object
	struct {
		unsigned VendorDefined:15;	// Defined by the vendor
		unsigned VDMType:1;	// Unstructured or structured message header
		unsigned VendorID:16;	// Unique 16-bit unsigned integer assigned by the USB-IF
	} UVDM;
	struct {
		unsigned Command:5;	//
		unsigned Reserved0:1;	// Reserved - set to zero
		unsigned CommandType:2;	// Init, ACK, NAK, BUSY...
		unsigned ObjPos:3;	//
		unsigned Reserved1:2;	// Reserved - set to zero
		unsigned Version:2;	// Structured VDM version
		unsigned VDMType:1;	// Unstructured or structured message header
		unsigned SVID:16;	// Unique 16-bit unsigned integer assigned by the USB-IF
	} SVDM;
	struct {
		unsigned Command:5;
		unsigned Reserverd2:2;
		unsigned CommandStatus:1;
		unsigned ModeObjPos:3;
		unsigned Reserverd1:2;
		unsigned UVDMVersion:2;
		unsigned VDMType:1;
		unsigned VendorID:16;
	} UVDMReqRsp;
	struct {
		unsigned Current:22;
		unsigned Unused:10;
	} UVDMDO;
	struct {
		unsigned MaxCurrentLimit:10;
		unsigned MiniCurrentLimit:10;
		unsigned Voltage:10;
		unsigned ModeID:2;
	} ModeInfo;
} doDataObject_t;

typedef enum {
	pdtNone = 0,		// Reserved token (nothing)
	pdtAttach,		// USB PD attached
	pdtDetach,		// USB PD detached
	pdtHardReset,		// USB PD hard reset
	pdtBadMessageID,	// An incorrect message ID was received
} USBPD_BufferTokens_t;

typedef enum {
	peDisabled = 0,		// Policy engine is disabled
	FIRST_PE_ST = peDisabled,	// mark lowest value in enum
	peErrorRecovery,	// Error recovery state
	peSourceHardReset,	// Received a hard reset
	peSourceSendHardReset,	// Source send a hard reset
	peSourceSoftReset,	// Received a soft reset
	peSourceSendSoftReset,	// Send a soft reset
	peSourceStartup,	// Initial state
	peSourceSendCaps,	// Send the source capabilities
	peSourceDiscovery,	// Waiting to detect a USB PD sink
	peSourceDisabled,	// Disabled state

	peSourceTransitionDefault,	// Transition to default 5V state /10
	peSourceNegotiateCap,	// Negotiate capability and PD contract
	peSourceCapabilityResponse,	// Respond to a request message with a reject/wait
	peSourceTransitionSupply,	// Transition the power supply to the new setting (accept request)
	peSourceReady,		// Contract is in place and output voltage is stable
	peSourceGiveSourceCaps,	// State to resend source capabilities
	peSourceGetSinkCaps,	// State to request the sink capabilities
	peSourceSendPing,	// State to send a ping message
	peSourceGotoMin,	// State to send the gotoMin and ready the power supply
	peSourceGiveSinkCaps,	// State to send the sink capabilities if dual-role

	peSourceGetSourceCaps,	// State to request the source caps from the UFP /20
	peSourceSendDRSwap,	// State to send a DR_Swap message
	peSourceEvaluateDRSwap,	// Evaluate whether we are going to accept or reject the swap

	peSinkHardReset,	// Received a hard reset
	peSinkSendHardReset,	// Sink send hard reset
	peSinkSoftReset,	// Sink soft reset
	peSinkSendSoftReset,	// Sink send soft reset
	peSinkTransitionDefault,	// Transition to the default state
	peSinkStartup,		// Initial sink state           
	peSinkDiscovery,	// Sink discovery state

	peSinkWaitCaps,		// Sink wait for capabilities state //30
	peSinkEvaluateCaps,	// Sink state to evaluate the received source capabilities
	peSinkSelectCapability,	// Sink state for selecting a capability
	peSinkTransitionSink,	// Sink state for transitioning the current power
	peSinkReady,		// Sink ready state
	peSinkGiveSinkCap,	// Sink send capabilities state
	peSinkGetSourceCap,	// Sink get source capabilities state
	peSinkGetSinkCap,	// Sink state to get the sink capabilities of the connected source
	peSinkGiveSourceCap,	// Sink state to send the source capabilities if dual-role           
	peSinkSendDRSwap,	// State to send a DR_Swap message 

	peSinkEvaluateDRSwap,	// Evaluate whether we are going to accept or reject the swap //40
	// Adding new commands to the end here to avoid messing up the ones above with the older software (backwards compatibility)
	peSourceSendVCONNSwap,	// Initiate a VCONN swap sequence
	peSinkEvaluateVCONNSwap,	// Evaluate whether we are going to accept or reject the swap
	peSourceSendPRSwap,	// Initiate a PR_Swap sequence
	peSourceEvaluatePRSwap,	// Evaluate whether we are going to accept or reject the swap
	peSinkSendPRSwap,	// Initiate a PR_Swap sequence
	peSinkEvaluatePRSwap,	// Evaluate whether we are going to accept or reject the swap
	// Adding in VDM states
	peGiveVdm,
	// ---------- UFP VDM State Diagram ----------
	peUfpVdmGetIdentity,	// Requesting Identity information from DPM           
	FIRST_VDM_STATE = peUfpVdmGetIdentity,

	peUfpVdmSendIdentity,	// Sending Discover Identity ACK //50
	peUfpVdmGetSvids,	// Requesting SVID info from DPM
	peUfpVdmSendSvids,	// Sending Discover SVIDs ACK
	peUfpVdmGetModes,	// Requesting Mode info from DPM
	peUfpVdmSendModes,	// Sending Discover Modes ACK
	peUfpVdmEvaluateModeEntry,	// Requesting DPM to evaluate request to enter a mode, and enter if OK
	peUfpVdmModeEntryNak,	// Sending Enter Mode NAK response
	peUfpVdmModeEntryAck,	// Sending Enter Mode ACK response
	peUfpVdmModeExit,	// Requesting DPM to evalute request to exit mode
	peUfpVdmModeExitNak,	// Sending Exit Mode NAK reponse //60

	peUfpVdmModeExitAck,	// Sending Exit Mode ACK Response //60
	// ---------- UFP VDM Attention State Diagram ----------
	peUfpVdmAttentionRequest,	// Sending Attention Command
	// ---------- DFP to UFP VDM Discover Identity State Diagram ----------
	peDfpUfpVdmIdentityRequest,	// Sending Identity Request
	peDfpUfpVdmIdentityAcked,	// Inform DPM of Identity
	peDfpUfpVdmIdentityNaked,	// Inform DPM of result
	// ---------- DFP to Cable Plug VDM Discover Identity State Diagram ----------
	peDfpCblVdmIdentityRequest,	// Sending Identity Request
	peDfpCblVdmIdentityAcked,	// Inform DPM
	peDfpCblVdmIdentityNaked,	// Inform DPM
	// ---------- DFP VDM Discover SVIDs State Diagram ----------
	peDfpVdmSvidsRequest,	// Sending Discover SVIDs request
	peDfpVdmSvidsAcked,	// Inform DPM

	peDfpVdmSvidsNaked,	// Inform DPM //70
	// ---------- DFP VDM Discover Modes State Diagram ----------
	peDfpVdmModesRequest,	// Sending Discover Modes request
	peDfpVdmModesAcked,	// Inform DPM
	peDfpVdmModesNaked,	// Inform DPM
	// ---------- DFP VDM Enter Mode State Diagram ----------
	peDfpVdmModeEntryRequest,	// Sending Mode Entry request
	peDfpVdmModeEntryAcked,	// Inform DPM
	peDfpVdmModeEntryNaked,	// Inform DPM
	// ---------- DFP VDM Exit Mode State Diagram ----------
	peDfpVdmModeExitRequest,	// Sending Exit Mode request
	peDfpVdmExitModeAcked,	// Inform DPM
	// if Exit Mode not Acked, go to Hard Reset state
	// ---------- Source Startup VDM Discover Identity State Diagram ----------
	peSrcVdmIdentityRequest,	// sending Discover Identity request

	peSrcVdmIdentityAcked,	// inform DPM //80
	peSrcVdmIdentityNaked,	// inform DPM
	// ---------- DFP VDM Attention State Diagram ----------
	peDfpVdmAttentionRequest,	// Attention Request received
	// ---------- Cable Ready VDM State Diagram ----------
	peCblReady,		// Cable power up state?
	// ---------- Cable Discover Identity VDM State Diagram ----------
	peCblGetIdentity,	// Discover Identity request received
	peCblGetIdentityNak,	// Respond with NAK
	peCblSendIdentity,	// Respond with Ack
	// ---------- Cable Discover SVIDs VDM State Diagram ----------
	peCblGetSvids,		// Discover SVIDs request received
	peCblGetSvidsNak,	// Respond with NAK
	peCblSendSvids,		// Respond with ACK

	// ---------- Cable Discover Modes VDM State Diagram ----------
	peCblGetModes,		// Discover Modes request received           //90
	peCblGetModesNak,	// Respond with NAK
	peCblSendModes,		// Respond with ACK
	// ---------- Cable Enter Mode VDM State Diagram ----------
	peCblEvaluateModeEntry,	// Enter Mode request received
	peCblModeEntryAck,	// Respond with NAK
	peCblModeEntryNak,	// Respond with ACK
	// ---------- Cable Exit Mode VDM State Diagram ----------
	peCblModeExit,		// Exit Mode request received
	peCblModeExitAck,	// Respond with NAK
	peCblModeExitNak,	// Respond with ACK           
	// ---------- DP States ----------
	peDpRequestStatus,	// Requesting PP Status                          

	// ---------- BIST Receive Mode --------------------- //
	PE_BIST_Receive_Mode,	// Bist Receive Mode            //100
	PE_BIST_Frame_Received,	// Test Frame received by Protocol layer
	// ---------- BIST Carrier Mode and Eye Pattern ----- //
	PE_BIST_Carrier_Mode_2,	// BIST Carrier Mode 2
	LAST_VDM_STATE = PE_BIST_Carrier_Mode_2,
	peSourceWaitNewCapabilities,	// Wait for new Source Capabilities from Policy Manager
	LAST_PE_ST = peSourceWaitNewCapabilities,	// mark the last valid enum value
	PE_BIST_Test_Data,	// BIST Test Data State
	peSourceEvaluateVCONNSwap,
#ifdef FSC_DEBUG
	dbgGetRxPacket,		// Debug point for peeking at Rx packet handling in ProtocolGetRxPacket()
	dbgSendTxPacket,	// Debug point for peeking at Tx packet handling in ProtocolTransmitMessage()
#endif // FSC_DEBUG
} PolicyState_t;

typedef enum {
	PRLDisabled = 0,	// Protocol state machine is disabled
	PRLIdle,		// Ready to receive or accept tx requests (after tx reset state, reset retry counter)
	PRLReset,		// Received a soft reset request message or exit from a hard reset
	PRLResetWait,		// Waiting for the hard reset to complete
	PRLRxWait,		// Actively receiving a message
	PRLTxSendingMessage,	// Pass the message to the FUSB300 and wait for TX_EMPTY or I_COLLISION
	PRLTxWaitForPHYResponse,	// Wait for activity on the CC line or a timeout
	PRLTxVerifyGoodCRC,	// Verify the good CRC message (combine retry, tx error, message id and message sent)

	// ------- BIST Receiver Test -------- //
	PRL_BIST_Rx_Reset_Counter,	// Reset BISTErrorCounter and preload PRBS
	PRL_BIST_Rx_Test_Frame,	// Wait for test Frame form PHY
	PRL_BIST_Rx_Error_Count,	// Construct and send BIST error count message to PHY
	PRL_BIST_Rx_Inform_Policy,	// Inform policy engine error count has been sent
} ProtocolState_t;

typedef enum {
	txIdle = 0,
	txReset,
	txSend,
	txBusy,
	txWait,
	txSuccess,
	txError,
	txCollision
} PDTxStatus_t;

typedef enum {
	pdoTypeFixed = 0,
	pdoTypeBattery,
	pdoTypeVariable,
	pdoTypeReserved
} pdoSupplyType;

typedef enum {
	AUTO_VDM_INIT,
	AUTO_VDM_DISCOVER_ID_PP,
	AUTO_VDM_DISCOVER_SVIDS_PP,
	AUTO_VDM_DISCOVER_MODES_PP,
	AUTO_VDM_ENTER_MODE_PP,
	AUTO_VDM_DP_GET_STATUS,
	AUTO_VDM_DONE
} VdmDiscoveryState_t;

typedef enum {
	SOP_TYPE_SOP,
	SOP_TYPE_SOP1,
	SOP_TYPE_SOP2,
	SOP_TYPE_SOP1_DEBUG,
	SOP_TYPE_SOP2_DEBUG,
	SOP_TYPE_ERROR = 0xFF
} SopType;

typedef struct {
	struct completion	complete;
	atomic_t		pending;
} ReqContextType;
#endif // __USBPD_TYPES_H__
