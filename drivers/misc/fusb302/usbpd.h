#ifndef USBPD_H
#define	USBPD_H

    #include "vdm_types.h"
    #include "vdm_manager.h"

    #define PDBUFSIZE               128
    #define USBPDSPECREV            1           // Revision 1.0

    #define STAT_BUSY               0
    #define STAT_SUCCESS            1
    #define STAT_ERROR              2

    // FUSB300 FIFO Token Definitions
    #define TXON                    0xA1
    #define SOP1                    0x12
    #define SOP2                    0x13
    #define SOP3                    0x1B
    #define RESET1                  0x15
    #define RESET2                  0x16
    #define PACKSYM                 0x80
    #define JAM_CRC                 0xFF
    #define EOP                     0x14
    #define TXOFF                   0xFE

    // USB PD Header Message Defintions
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

    // USB PD Timing Parameters
    #define tNoResponse             5000 * 40               // Defined in 1ms
    #define tSenderResponse         30 * 40                 // Defined in 1ms
    #define tTypeCSendSourceCap     150 * 40                // Defined in 1ms
    #define tSinkWaitCap            2300 * 40               // Defined in 1ms
    #define tSnkTransition          35                 // Defined in 1ms
    #define tPSHardReset            15 * 40                 // Defined in 1ms
    #define tPSTransition           500 * 40                // Defined in 1ms
    #define tPSSourceOffMin         750 * 40                // Defined in 1ms
    #define tPSSourceOffMax         920 * 40                // Defined in 1ms
    #define tPSSourceOffNom         800 * 40                // Defined in 1ms
    #define tPSSourceOnMin          390 * 40                // Defined in 1ms
    #define tPSSourceOnMax          480 * 40                // Defined in 1ms
    #define tPSSourceOnNom          445 * 40                // Defined in 1ms
    #define tVCONNSourceOn          100 * 40                // Defined in 1ms
    #define tReceive                200                     // Defined in 25µs
    #define tBMCTimeout             200                     // Defined in 25µs (not defined by spec)
    #define tFPF2498Transition      20                 // Defined in 1ms (not defined by spec)
    #define tPRSwapBailout          5000 * 40               // Defined in 1ms (not defined by spec)

    #define nHardResetCount         2
    #define nRetryCount             2
    #define nCapsCount              50

    typedef enum {
        pdtNone = 0,                // Reserved token (nothing)
        pdtAttach,                  // USB PD attached
        pdtDetach,                  // USB PD detached
        pdtHardReset,               // USB PD hard reset
        pdtBadMessageID,            // An incorrect message ID was received
    } USBPD_BufferTokens_t;

    typedef enum {
        peDisabled = 0,             // Policy engine is disabled
        peErrorRecovery,            // Error recovery state
        peSourceHardReset,          // Received a hard reset
        peSourceSendHardReset,      // Source send a hard reset
        peSourceSoftReset,          // Received a soft reset
        peSourceSendSoftReset,      // Send a soft reset
        peSourceStartup,            // Initial state
        peSourceSendCaps,           // Send the source capabilities
        peSourceDiscovery,          // Waiting to detect a USB PD sink
        peSourceDisabled,           // Disabled state
        peSourceTransitionDefault,  // Transition to default 5V state
        peSourceNegotiateCap,       // Negotiate capability and PD contract
        peSourceCapabilityResponse, // Respond to a request message with a reject/wait
        peSourceTransitionSupply,   // Transition the power supply to the new setting (accept request)
        peSourceReady,              // Contract is in place and output voltage is stable
        peSourceGiveSourceCaps,     // State to resend source capabilities
        peSourceGetSinkCaps,        // State to request the sink capabilities
        peSourceSendPing,           // State to send a ping message
        peSourceGotoMin,            // State to send the gotoMin and ready the power supply
        peSourceGiveSinkCaps,       // State to send the sink capabilities if dual-role
        peSourceGetSourceCaps,      // State to request the source caps from the UFP
        peSourceSendDRSwap,         // State to send a DR_Swap message
        peSourceEvaluateDRSwap,     // Evaluate whether we are going to accept or reject the swap
        peSinkHardReset,            // Received a hard reset
        peSinkSendHardReset,        // Sink send hard reset
        peSinkSoftReset,            // Sink soft reset
        peSinkSendSoftReset,        // Sink send soft reset
        peSinkTransitionDefault,    // Transition to the default state
        peSinkStartup,              // Initial sink state
        peSinkDiscovery,            // Sink discovery state 
        peSinkWaitCaps,             // Sink wait for capabilities state
        peSinkEvaluateCaps,         // Sink state to evaluate the received source capabilities
        peSinkSelectCapability,     // Sink state for selecting a capability
        peSinkTransitionSink,       // Sink state for transitioning the current power
        peSinkReady,                // Sink ready state
        peSinkGiveSinkCap,          // Sink send capabilities state
        peSinkGetSourceCap,         // Sink get source capabilities state
        peSinkGetSinkCap,           // Sink state to get the sink capabilities of the connected source
        peSinkGiveSourceCap,        // Sink state to send the source capabilities if dual-role
        peSinkSendDRSwap,           // State to send a DR_Swap message
        peSinkEvaluateDRSwap,       // Evaluate whether we are going to accept or reject the swap
        // Adding new commands to the end here to avoid messing up the ones above with the older software (backwards compatibility
        peSourceSendVCONNSwap,      // Initiate a VCONN swap sequence
        peSinkEvaluateVCONNSwap,    // Evaluate whether we are going to accept or reject the swap
        peSourceSendPRSwap,         // Initiate a PR_Swap sequence
        peSourceEvaluatePRSwap,     // Evaluate whether we are going to accept or reject the swap
        peSinkSendPRSwap,           // Initiate a PR_Swap sequence
        peSinkEvaluatePRSwap,       // Evaluate whether we are going to accept or reject the swap
    } PolicyState_t;

    typedef enum {
        PRLDisabled = 0,            // Protocol state machine is disabled
        PRLIdle,                    // Ready to receive or accept tx requests (after tx reset state, reset retry counter)
        PRLReset,                   // Received a soft reset request message or exit from a hard reset
        PRLResetWait,               // Waiting for the hard reset to complete
        PRLRxWait,                  // Actively receiving a message
        PRLTxSendingMessage,        // Pass the message to the FUSB300 and wait for TX_EMPTY or I_COLLISION
        PRLTxWaitForPHYResponse,    // Wait for activity on the CC line or a timeout
        PRLTxVerifyGoodCRC          // Verify the good CRC message (combine retry, tx error, message id and message sent)
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

    typedef union {
        u16 word;
        u8 byte[2];
        struct {
            // Switches0
            unsigned MessageType:4;         // 0-3      : Message Type
            unsigned :1;                    // 4        : Reserved
            unsigned PortDataRole:1;        // 5        : Port Data Role
            unsigned SpecRevision:2;        // 6-7      : Specification Revision
            unsigned PortPowerRole:1;       // 8        : Port Power Role
            unsigned MessageID:3;           // 9-11     : Message ID
            unsigned NumDataObjects:3;      // 12-14    : Number of Data Objects
            unsigned :1;                    // 15       : Reserved
        };
    } sopMainHeader_t;

    typedef union {
        u16 word;
        u8 byte[2];
        struct {
            // Switches0
            unsigned MessageType:4;         // 0-3      : Message Type
            unsigned :1;                    // 4        : Reserved
            unsigned :1;                    // 5        : Reserved
            unsigned SpecRevision:2;        // 6-7      : Specification Revision
            unsigned CablePlug:1;           // 8        : Cable Plug
            unsigned MessageID:3;           // 9-11     : Message ID
            unsigned NumDataObjects:3;      // 12-14    : Number of Data Objects
            unsigned :1;                    // 15       : Reserved
        };
    } sopPlugHeader_t;

    typedef union {
        u32 object;
        u16 word[2];
        u8 byte[4];
        struct {
            unsigned :31;
            unsigned SupplyType:2;
        } PDO;          // General purpose Power Data Object
        struct {
            unsigned MaxCurrent:10;         // Max current in 10mA units
            unsigned Voltage:10;            // Voltage in 50mV units
            unsigned PeakCurrent:2;         // Peak current (divergent from Ioc ratings)
            unsigned :3;                    // Reserved
            unsigned DataRoleSwap:1;        // Data role swap (supports DR_Swap message)
            unsigned USBCommCapable:1;      // USB communications capable (is communication available)
            unsigned ExternallyPowered:1;   // Externally powered (set when AC supply is supporting 100% of power)
            unsigned USBSuspendSupport:1;   // USB Suspend Supported
            unsigned DualRolePower:1;       // Dual-Role power (supports PR_Swap message)
            unsigned SupplyType:2;          // (Fixed Supply)
        } FPDOSupply;   // Fixed Power Data Object for Supplies
        struct {
            unsigned OperationalCurrent:10; // Operational current in 10mA units
            unsigned Voltage:10;            // Voltage in 50mV units
            unsigned :5;                    // Reserved
            unsigned DataRoleSwap:1;        // Data role swap (supports DR_Swap message)
            unsigned USBCommCapable:1;      // USB communications capable (is communication available)
            unsigned ExternallyPowered:1;   // Externally powered (set when AC supply is supporting 100% of power)
            unsigned HigherCapability:1;    // Whether the sink needs more than vSafe5V to provide full functionality
            unsigned DualRolePower:1;       // Dual-Role power (supports PR_Swap message)
            unsigned SupplyType:2;          // (Fixed Supply)
        } FPDOSink;     // Fixed Power Data Object for Sinks
        struct {
            unsigned MaxCurrent:10;         // Max current in 10mA units
            unsigned MinVoltage:10;         // Minimum Voltage in 50mV units
            unsigned MaxVoltage:10;         // Maximum Voltage in 50mV units
            unsigned SupplyType:2;          // (Variable Supply)
        } VPDO;         // Variable Power Data Object
        struct {
            unsigned MaxPower:10;           // Max power in 250mW units
            unsigned MinVoltage:10;         // Minimum Voltage in 50mV units
            unsigned MaxVoltage:10;         // Maximum Voltage in 50mV units
            unsigned SupplyType:2;          // (Battery Supply)
        } BPDO;         // Battery Power Data Object
        struct {
            unsigned MinMaxCurrent:10;      // Min/Max current in 10mA units
            unsigned OpCurrent:10;          // Operating current in 10mA units
            unsigned :4;                    // Reserved - set to zero
            unsigned NoUSBSuspend:1;        // Set when the sink wants to continue the contract during USB suspend (i.e. charging battery)
            unsigned USBCommCapable:1;      // USB communications capable (is communication available)
            unsigned CapabilityMismatch:1;  // Set if the sink cannot satisfy its power requirements from capabilities offered
            unsigned GiveBack:1;            // Whether the sink will respond to the GotoMin message
            unsigned ObjectPosition:3;      // Which object in the source capabilities is being requested
            unsigned :1;                    // Reserved - set to zero
        } FVRDO;        // Fixed/Variable Request Data Object
        struct {
            unsigned MinMaxPower:10;        // Min/Max power in 250mW units
            unsigned OpPower:10;            // Operating power in 250mW units
            unsigned :4;                    // Reserved - set to zero
            unsigned NoUSBSuspend:1;        // Set when the sink wants to continue the contract during USB suspend (i.e. charging battery)
            unsigned USBCommCapable:1;      // USB communications capable (is communication available)
            unsigned CapabilityMismatch:1;  // Set if the sink cannot satisfy its power requirements from capabilities offered
            unsigned GiveBack:1;            // Whether the sink will respond to the GotoMin message
            unsigned ObjectPosition:3;      // Which object in the source capabilities is being requested
            unsigned :1;                    // Reserved - set to zero
        } BRDO;         // Battery Request Data Object
        struct {
            unsigned VendorDefined:15;      // Defined by the vendor
            unsigned VDMType:1;             // Unstructured or structured message header
            unsigned VendorID:16;           // Unique 16-bit unsigned integer assigned by the USB-IF
        } UVDM;
        struct {
            unsigned Command:5;             //
            unsigned :1;                    // Reserved - set to zero
            unsigned CommandType:2;         // Init, ACK, NAK, BUSY...
            unsigned ObjectPosition:3;      //
            unsigned :2;                    // Reserved - set to zero
            unsigned Version:2;             // Structured VDM version
            unsigned VDMType:1;             // Unstructured or structured message header
            unsigned VendorID:16;           // Unique 16-bit unsigned integer assigned by the USB-IF
        } SVDM;
    } doDataObject_t;

    /////////////////////////////////////////////////////////////////////////////
    //                            LOCAL PROTOTYPES
    /////////////////////////////////////////////////////////////////////////////

    void InitializeUSBPDVariables(void);
    void USBPDEnable(bool FUSB300Update, bool TypeCDFP);
    void USBPDDisable(bool FUSB300Update);
    void USBPDPolicyEngine(void);

    void PolicyErrorRecovery(void);
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
    void PolicySourceGetSinkCap(void);
    void PolicySourceSendPing(void);
    void PolicySourceGotoMin(void);
    void PolicySourceGiveSinkCap(void);
    void PolicySourceSendDRSwap(void);
    void PolicySourceEvaluateDRSwap(void);
    void PolicySourceSendVCONNSwap(void);
    void PolicySourceSendPRSwap(void);
    void PolicySourceEvaluatePRSwap(void);
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
    bool PolicySendHardReset(PolicyState_t nextState, u32 delay);
    u8 PolicySendCommand(u8 Command, PolicyState_t nextState, u8 subIndex);
    u8 PolicySendCommandNoReset(u8 Command, PolicyState_t nextState, u8 subIndex);
    u8 PolicySendData(u8 MessageType, u8 NumDataObjects, doDataObject_t* DataObjects, PolicyState_t nextState, u8 subIndex);
    u8 PolicySendDataNoReset(u8 MessageType, u8 NumDataObjects, doDataObject_t* DataObjects, PolicyState_t nextState, u8 subIndex);
    void UpdateCapabilitiesRx(bool IsSourceCaps);
    void USBPDProtocol(void);
    void ProtocolIdle(void);
    void ProtocolResetWait(void);
    void ProtocolRxWait(void);
    void ProtocolGetRxPacket(void);
    void ProtocolTransmitMessage(void);
    void ProtocolSendingMessage(void);
    void ProtocolWaitForPHYResponse(void);
    void ProtocolVerifyGoodCRC(void);
    void ProtocolTxRetryCounter(void);
    void ProtocolSendGoodCRC(void);
    void ProtocolLoadSOP(void);
    void ProtocolLoadEOP(void);
    void ProtocolSendHardReset(void);
    void ProtocolFlushRxFIFO(void);
    void ProtocolFlushTxFIFO(void);
    void ResetProtocolLayer(bool ResetPDLogic);
    bool StoreUSBPDToken(bool transmitter, USBPD_BufferTokens_t token);
    bool StoreUSBPDMessage(sopMainHeader_t Header, doDataObject_t* DataObject, bool transmitter, u8 SOPType);
    u8 GetNextUSBPDMessageSize(void);
    u8 GetUSBPDBufferNumBytes(void);
    bool ClaimBufferSpace(int intReqSize);
    void GetUSBPDStatus(unsigned char abytData[]);
    u8 GetUSBPDStatusOverview(void);
    u8 ReadUSBPDBuffer(unsigned char* pData, unsigned char bytesAvail);
    void SendUSBPDMessage(unsigned char* abytData);
    void WriteSourceCapabilities(unsigned char* abytData);
    void ReadSourceCapabilities(unsigned char* abytData);
    void WriteSinkCapabilities(unsigned char* abytData);
    void ReadSinkCapabilities(unsigned char* abytData);
    void WriteSinkRequestSettings(unsigned char* abytData);
    void ReadSinkRequestSettings(unsigned char* abytData);
	void SendUSBPDHardReset(void);
	
    // shim functions for VDM code
    extern VdmManager vdmm;
    void InitializeVdmManager(void);
    void convertAndProcessVdmMessage(void);
    void sendVdmMessage(VdmManager* vdmm, SopType sop, u32* arr, unsigned int length);
    void doVdmCommand(void);
    void doDiscoverIdentity(void);
    void doDiscoverSvids(void);

    
#endif	/* USBPD_H */

