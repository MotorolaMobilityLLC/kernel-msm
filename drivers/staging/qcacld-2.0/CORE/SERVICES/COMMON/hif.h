/*
 * Copyright (c) 2013-2015 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

//==============================================================================
// HIF specific declarations and prototypes
//
// Author(s): ="Atheros"
//==============================================================================
#ifndef _HIF_H_
#define _HIF_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Header files */
//#include "a_config.h"
#include "athdefs.h"
#include "a_types.h"
#include "osapi_linux.h"
#include "dl_list.h"

#define ENABLE_MBOX_DUMMY_SPACE_FEATURE 1

typedef struct htc_callbacks HTC_CALLBACKS;
typedef struct hif_device HIF_DEVICE;
typedef void __iomem *A_target_id_t;

#define HIF_TYPE_AR6002 2
#define HIF_TYPE_AR6003 3
#define HIF_TYPE_AR6004 5
#define HIF_TYPE_AR9888 6
#define HIF_TYPE_AR6320 7
#define HIF_TYPE_AR6320V2 8
/* For attaching Peregrine 2.0 board host_reg_tbl only */
#define HIF_TYPE_AR9888V2 8

/*
 * direction - Direction of transfer (HIF_READ/HIF_WRITE).
 */
#define HIF_READ                    0x00000001
#define HIF_WRITE                   0x00000002
#define HIF_DIR_MASK                (HIF_READ | HIF_WRITE)

/*
 *     type - An interface may support different kind of read/write commands.
 *            For example: SDIO supports CMD52/CMD53s. In case of MSIO it
 *            translates to using different kinds of TPCs. The command type
 *            is thus divided into a basic and an extended command and can
 *            be specified using HIF_BASIC_IO/HIF_EXTENDED_IO.
 */
#define HIF_BASIC_IO                0x00000004
#define HIF_EXTENDED_IO             0x00000008
#define HIF_TYPE_MASK               (HIF_BASIC_IO | HIF_EXTENDED_IO)

/*
 *     emode - This indicates the whether the command is to be executed in a
 *             blocking or non-blocking fashion (HIF_SYNCHRONOUS/
 *             HIF_ASYNCHRONOUS). The read/write data paths in HTC have been
 *             implemented using the asynchronous mode allowing the the bus
 *             driver to indicate the completion of operation through the
 *             registered callback routine. The requirement primarily comes
 *             from the contexts these operations get called from (a driver's
 *             transmit context or the ISR context in case of receive).
 *             Support for both of these modes is essential.
 */
#define HIF_SYNCHRONOUS             0x00000010
#define HIF_ASYNCHRONOUS            0x00000020
#define HIF_EMODE_MASK              (HIF_SYNCHRONOUS | HIF_ASYNCHRONOUS)

/*
 *     dmode - An interface may support different kinds of commands based on
 *             the tradeoff between the amount of data it can carry and the
 *             setup time. Byte and Block modes are supported (HIF_BYTE_BASIS/
 *             HIF_BLOCK_BASIS). In case of latter, the data is rounded off
 *             to the nearest block size by padding. The size of the block is
 *             configurable at compile time using the HIF_BLOCK_SIZE and is
 *             negotiated with the target during initialization after the
 *             AR6000 interrupts are enabled.
 */
#define HIF_BYTE_BASIS              0x00000040
#define HIF_BLOCK_BASIS             0x00000080
#define HIF_DMODE_MASK              (HIF_BYTE_BASIS | HIF_BLOCK_BASIS)

/*
 *     amode - This indicates if the address has to be incremented on AR6000
 *             after every read/write operation (HIF?FIXED_ADDRESS/
 *             HIF_INCREMENTAL_ADDRESS).
 */
#define HIF_FIXED_ADDRESS           0x00000100
#define HIF_INCREMENTAL_ADDRESS     0x00000200
#define HIF_AMODE_MASK              (HIF_FIXED_ADDRESS | HIF_INCREMENTAL_ADDRESS)

/*
 *      dummy - data written into the dummy space will not put into the final mbox FIFO
 */
#define HIF_DUMMY_SPACE_MASK              0xFFFF0000


#define HIF_WR_ASYNC_BYTE_FIX   \
    (HIF_WRITE | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | HIF_FIXED_ADDRESS)
#define HIF_WR_ASYNC_BYTE_INC   \
    (HIF_WRITE | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | HIF_INCREMENTAL_ADDRESS)
#define HIF_WR_ASYNC_BLOCK_INC  \
    (HIF_WRITE | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | HIF_INCREMENTAL_ADDRESS)
#define HIF_WR_SYNC_BYTE_FIX    \
    (HIF_WRITE | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | HIF_FIXED_ADDRESS)
#define HIF_WR_SYNC_BYTE_INC    \
    (HIF_WRITE | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | HIF_INCREMENTAL_ADDRESS)
#define HIF_WR_SYNC_BLOCK_INC  \
    (HIF_WRITE | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | HIF_INCREMENTAL_ADDRESS)
#define HIF_WR_ASYNC_BLOCK_FIX \
    (HIF_WRITE | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | HIF_FIXED_ADDRESS)
#define HIF_WR_SYNC_BLOCK_FIX  \
    (HIF_WRITE | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | HIF_FIXED_ADDRESS)
#define HIF_RD_SYNC_BYTE_INC    \
    (HIF_READ | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | HIF_INCREMENTAL_ADDRESS)
#define HIF_RD_SYNC_BYTE_FIX    \
    (HIF_READ | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | HIF_FIXED_ADDRESS)
#define HIF_RD_ASYNC_BYTE_FIX   \
    (HIF_READ | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | HIF_FIXED_ADDRESS)
#define HIF_RD_ASYNC_BLOCK_FIX  \
    (HIF_READ | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | HIF_FIXED_ADDRESS)
#define HIF_RD_ASYNC_BYTE_INC   \
    (HIF_READ | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BYTE_BASIS | HIF_INCREMENTAL_ADDRESS)
#define HIF_RD_ASYNC_BLOCK_INC  \
    (HIF_READ | HIF_ASYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | HIF_INCREMENTAL_ADDRESS)
#define HIF_RD_SYNC_BLOCK_INC  \
    (HIF_READ | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | HIF_INCREMENTAL_ADDRESS)
#define HIF_RD_SYNC_BLOCK_FIX  \
    (HIF_READ | HIF_SYNCHRONOUS | HIF_EXTENDED_IO | HIF_BLOCK_BASIS | HIF_FIXED_ADDRESS)

typedef enum {
    HIF_DEVICE_POWER_STATE = 0,
    HIF_DEVICE_GET_MBOX_BLOCK_SIZE,
    HIF_DEVICE_GET_MBOX_ADDR,
    HIF_DEVICE_GET_PENDING_EVENTS_FUNC,
    HIF_DEVICE_GET_IRQ_PROC_MODE,
    HIF_DEVICE_GET_RECV_EVENT_MASK_UNMASK_FUNC,
    HIF_DEVICE_POWER_STATE_CHANGE,
    HIF_DEVICE_GET_IRQ_YIELD_PARAMS,
    HIF_CONFIGURE_QUERY_SCATTER_REQUEST_SUPPORT,
    HIF_DEVICE_GET_OS_DEVICE,
    HIF_DEVICE_DEBUG_BUS_STATE,
    HIF_BMI_DONE,
    HIF_DEVICE_SET_TARGET_TYPE,
    HIF_DEVICE_SET_HTC_CONTEXT,
    HIF_DEVICE_GET_HTC_CONTEXT,
} HIF_DEVICE_CONFIG_OPCODE;

/*
 * HIF CONFIGURE definitions:
 *
 *   HIF_DEVICE_GET_MBOX_BLOCK_SIZE
 *   input : none
 *   output : array of 4 u_int32_ts
 *   notes: block size is returned for each mailbox (4)
 *
 *   HIF_DEVICE_GET_MBOX_ADDR
 *   input : none
 *   output : HIF_DEVICE_MBOX_INFO
 *   notes:
 *
 *   HIF_DEVICE_GET_PENDING_EVENTS_FUNC
 *   input : none
 *   output: HIF_PENDING_EVENTS_FUNC function pointer
 *   notes: this is optional for the HIF layer, if the request is
 *          not handled then it indicates that the upper layer can use
 *          the standard device methods to get pending events (IRQs, mailbox messages etc..)
 *          otherwise it can call the function pointer to check pending events.
 *
 *   HIF_DEVICE_GET_IRQ_PROC_MODE
 *   input : none
 *   output : HIF_DEVICE_IRQ_PROCESSING_MODE (interrupt processing mode)
 *   note: the hif layer interfaces with the underlying OS-specific bus driver. The HIF
 *         layer can report whether IRQ processing is requires synchronous behavior or
 *         can be processed using asynchronous bus requests (typically faster).
 *
 *   HIF_DEVICE_GET_RECV_EVENT_MASK_UNMASK_FUNC
 *   input :
 *   output : HIF_MASK_UNMASK_RECV_EVENT function pointer
 *   notes: this is optional for the HIF layer.  The HIF layer may require a special mechanism
 *          to mask receive message events.  The upper layer can call this pointer when it needs
 *          to mask/unmask receive events (in case it runs out of buffers).
 *
 *   HIF_DEVICE_POWER_STATE_CHANGE
 *
 *   input : HIF_DEVICE_POWER_CHANGE_TYPE
 *   output : none
 *   note: this is optional for the HIF layer.  The HIF layer can handle power on/off state change
 *         requests in an interconnect specific way.  This is highly OS and bus driver dependent.
 *         The caller must guarantee that no HIF read/write requests will be made after the device
 *         is powered down.
 *
 *   HIF_DEVICE_GET_IRQ_YIELD_PARAMS
 *
 *   input : none
 *   output : HIF_DEVICE_IRQ_YIELD_PARAMS
 *   note: This query checks if the HIF layer wishes to impose a processing yield count for the DSR handler.
 *   The DSR callback handler will exit after a fixed number of RX packets or events are processed.
 *   This query is only made if the device reports an IRQ processing mode of HIF_DEVICE_IRQ_SYNC_ONLY.
 *   The HIF implementation can ignore this command if it does not desire the DSR callback to yield.
 *   The HIF layer can indicate the maximum number of IRQ processing units (RX packets) before the
 *   DSR handler callback must yield and return control back to the HIF layer.  When a yield limit is
 *   used the DSR callback will not call HIFAckInterrupts() as it would normally do before returning.
 *   The HIF implementation that requires a yield count must call HIFAckInterrupt() when it is prepared
 *   to process interrupts again.
 *
 *   HIF_CONFIGURE_QUERY_SCATTER_REQUEST_SUPPORT
 *   input : none
 *   output : HIF_DEVICE_SCATTER_SUPPORT_INFO
 *   note:  This query checks if the HIF layer implements the SCATTER request interface.  Scatter requests
 *   allows upper layers to submit mailbox I/O operations using a list of buffers.  This is useful for
 *   multi-message transfers that can better utilize the bus interconnect.
 *
 *
 *   HIF_DEVICE_GET_OS_DEVICE
 *   intput : none
 *   output : HIF_DEVICE_OS_DEVICE_INFO;
 *   note: On some operating systems, the HIF layer has a parent device object for the bus.  This object
 *         may be required to register certain types of logical devices.
 *
 *   HIF_DEVICE_DEBUG_BUS_STATE
 *   input : none
 *   output : none
 *   note: This configure option triggers the HIF interface to dump as much bus interface state.  This
 *   configuration request is optional (No-OP on some HIF implementations)
 *
 *   HIF_DEVICE_SET_TARGET_TYPE
 *   input : TARGET_TYPE_*
 *   output : none
 *   note: Some HIF implementations may need to know TargetType in order to access
 *   Target registers or Host Interest Area.  (No-OP on some HIF implementations)
 */

typedef struct {
    u_int32_t    ExtendedAddress;  /* extended address for larger writes */
    u_int32_t    ExtendedSize;
} HIF_MBOX_PROPERTIES;

#define HIF_MBOX_FLAG_NO_BUNDLING   (1 << 0)   /* do not allow bundling over the mailbox */

typedef struct {
    u_int32_t MboxAddresses[4];  /* must be first element for legacy HIFs that return the address in
                                   and ARRAY of 32-bit words */

        /* the following describe extended mailbox properties */
    HIF_MBOX_PROPERTIES MboxProp[4];
        /* if the HIF supports the GMbox extended address region it can report it
         * here, some interfaces cannot support the GMBOX address range and not set this */
    u_int32_t GMboxAddress;
    u_int32_t GMboxSize;
    u_int32_t Flags;             /* flags to describe mbox behavior or usage */
} HIF_DEVICE_MBOX_INFO;

typedef enum {
    HIF_DEVICE_IRQ_SYNC_ONLY,   /* for HIF implementations that require the DSR to process all
                                   interrupts before returning */
    HIF_DEVICE_IRQ_ASYNC_SYNC,  /* for HIF implementations that allow DSR to process interrupts
                                   using ASYNC I/O (that is HIFAckInterrupt can be called at a
                                   later time */
} HIF_DEVICE_IRQ_PROCESSING_MODE;

typedef enum {
    HIF_DEVICE_POWER_UP,    /* HIF layer should power up interface and/or module */
    HIF_DEVICE_POWER_DOWN,  /* HIF layer should initiate bus-specific measures to minimize power */
    HIF_DEVICE_POWER_CUT    /* HIF layer should initiate bus-specific AND/OR platform-specific measures
                               to completely power-off the module and associated hardware (i.e. cut power supplies)
                            */
} HIF_DEVICE_POWER_CHANGE_TYPE;

typedef enum {
    HIF_DEVICE_STATE_ON,
    HIF_DEVICE_STATE_DEEPSLEEP,
    HIF_DEVICE_STATE_CUTPOWER,
    HIF_DEVICE_STATE_WOW
} HIF_DEVICE_STATE;

typedef struct {
    int     RecvPacketYieldCount; /* max number of packets to force DSR to return */
} HIF_DEVICE_IRQ_YIELD_PARAMS;


typedef struct _HIF_SCATTER_ITEM {
    u_int8_t     *pBuffer;             /* CPU accessible address of buffer */
    int          Length;              /* length of transfer to/from this buffer */
    void        *pCallerContexts[2];  /* space for caller to insert a context associated with this item */
} HIF_SCATTER_ITEM;

struct _HIF_SCATTER_REQ;

typedef void ( *HIF_SCATTER_COMP_CB)(struct _HIF_SCATTER_REQ *);

typedef enum _HIF_SCATTER_METHOD {
    HIF_SCATTER_NONE = 0,
    HIF_SCATTER_DMA_REAL,              /* Real SG support no restrictions */
    HIF_SCATTER_DMA_BOUNCE,            /* Uses SG DMA but HIF layer uses an internal bounce buffer */
} HIF_SCATTER_METHOD;

typedef struct _HIF_SCATTER_REQ {
    DL_LIST             ListLink;           /* link management */
    u_int32_t            Address;            /* address for the read/write operation */
    u_int32_t            Request;            /* request flags */
    u_int32_t            TotalLength;        /* total length of entire transfer */
    u_int32_t            CallerFlags;        /* caller specific flags can be stored here */
    HIF_SCATTER_COMP_CB CompletionRoutine;  /* completion routine set by caller */
    int            CompletionStatus;   /* status of completion */
    void                *Context;           /* caller context for this request */
    int                 ValidScatterEntries;  /* number of valid entries set by caller */
    HIF_SCATTER_METHOD  ScatterMethod;        /* scatter method handled by HIF */
    void                *HIFPrivate[4];     /* HIF private area */
    u_int8_t             *pScatterBounceBuffer;  /* bounce buffer for upper layers to copy to/from */
    HIF_SCATTER_ITEM    ScatterList[1];     /* start of scatter list */
} HIF_SCATTER_REQ;

typedef HIF_SCATTER_REQ * ( *HIF_ALLOCATE_SCATTER_REQUEST)(HIF_DEVICE *device);
typedef void ( *HIF_FREE_SCATTER_REQUEST)(HIF_DEVICE *device, HIF_SCATTER_REQ *request);
typedef int ( *HIF_READWRITE_SCATTER)(HIF_DEVICE *device, HIF_SCATTER_REQ *request);

typedef struct _HIF_DEVICE_SCATTER_SUPPORT_INFO {
        /* information returned from HIF layer */
    HIF_ALLOCATE_SCATTER_REQUEST    pAllocateReqFunc;
    HIF_FREE_SCATTER_REQUEST        pFreeReqFunc;
    HIF_READWRITE_SCATTER           pReadWriteScatterFunc;
    int                             MaxScatterEntries;
    int                             MaxTransferSizePerScatterReq;
} HIF_DEVICE_SCATTER_SUPPORT_INFO;

typedef struct {
    void    *pOSDevice;
} HIF_DEVICE_OS_DEVICE_INFO;

#ifdef CONFIG_ATH_PCIE_ACCESS_DEBUG
typedef struct _HID_ACCESS_LOG {
    A_UINT32     seqnum;
    bool         is_write;
    void         *addr;
    A_UINT32     value;
}HIF_ACCESS_LOG;
#endif

#define HIF_MAX_DEVICES                 1

struct htc_callbacks {
    void      *context;     /* context to pass to the dsrhandler
                               note : rwCompletionHandler is provided the context passed to HIFReadWrite  */
    int (* rwCompletionHandler)(void *rwContext, int status);
    int (* dsrHandler)(void *context);
};

typedef struct osdrv_callbacks {
    void      *context;     /* context to pass for all callbacks except deviceRemovedHandler
                               the deviceRemovedHandler is only called if the device is claimed */
    int (* deviceInsertedHandler)(void *context, void *hif_handle);
    int (* deviceRemovedHandler)(void *claimedContext, void *hif_handle);
    int (* deviceSuspendHandler)(void *context);
    int (* deviceResumeHandler)(void *context);
    int (* deviceWakeupHandler)(void *context);
    int (* devicePowerChangeHandler)(void *context, HIF_DEVICE_POWER_CHANGE_TYPE config);
} OSDRV_CALLBACKS;

#define HIF_OTHER_EVENTS     (1 << 0)   /* other interrupts (non-Recv) are pending, host
                                           needs to read the register table to figure out what */
#define HIF_RECV_MSG_AVAIL   (1 << 1)   /* pending recv packet */

typedef struct _HIF_PENDING_EVENTS_INFO {
    u_int32_t Events;
    u_int32_t LookAhead;
    u_int32_t AvailableRecvBytes;
} HIF_PENDING_EVENTS_INFO;

    /* function to get pending events , some HIF modules use special mechanisms
     * to detect packet available and other interrupts */
typedef int ( *HIF_PENDING_EVENTS_FUNC)(HIF_DEVICE              *device,
                                             HIF_PENDING_EVENTS_INFO *pEvents,
                                             void                    *AsyncContext);

#define HIF_MASK_RECV    TRUE
#define HIF_UNMASK_RECV  FALSE
    /* function to mask recv events */
typedef int ( *HIF_MASK_UNMASK_RECV_EVENT)(HIF_DEVICE  *device,
                                                bool    Mask,
                                                void   *AsyncContext);


/*
 * This API is used to perform any global initialization of the HIF layer
 * and to set OS driver callbacks (i.e. insertion/removal) to the HIF layer
 *
 */
int HIFInit(OSDRV_CALLBACKS *callbacks);

/* This API claims the HIF device and provides a context for handling removal.
 * The device removal callback is only called when the OSDRV layer claims
 * a device.  The claimed context must be non-NULL */
void HIFClaimDevice(HIF_DEVICE *device, void *claimedContext);
/* release the claimed device */
void HIFReleaseDevice(HIF_DEVICE *device);

/* This API allows the HTC layer to attach to the HIF device */
int HIFAttachHTC(HIF_DEVICE *device, HTC_CALLBACKS *callbacks);
/* This API detaches the HTC layer from the HIF device */
void     HIFDetachHTC(HIF_DEVICE *device);

/*
 * This API is used to provide the read/write interface over the specific bus
 * interface.
 * address - Starting address in the AR6000's address space. For mailbox
 *           writes, it refers to the start of the mbox boundary. It should
 *           be ensured that the last byte falls on the mailbox's EOM. For
 *           mailbox reads, it refers to the end of the mbox boundary.
 * buffer - Pointer to the buffer containg the data to be transmitted or
 *          received.
 * length - Amount of data to be transmitted or received.
 * request - Characterizes the attributes of the command.
 */
int
HIFReadWrite(HIF_DEVICE    *device,
             u_int32_t      address,
             u_char        *buffer,
             u_int32_t      length,
             u_int32_t      request,
             void          *context);

/*
 * This can be initiated from the unload driver context when the OSDRV layer has no more use for
 * the device.
 */
void HIFShutDownDevice(HIF_DEVICE *device);
void HIFSurpriseRemoved(HIF_DEVICE *device);

/*
 * This should translate to an acknowledgment to the bus driver indicating that
 * the previous interrupt request has been serviced and the all the relevant
 * sources have been cleared. HTC is ready to process more interrupts.
 * This should prevent the bus driver from raising an interrupt unless the
 * previous one has been serviced and acknowledged using the previous API.
 */
void HIFAckInterrupt(HIF_DEVICE *device);

void HIFMaskInterrupt(HIF_DEVICE *device);

void HIFUnMaskInterrupt(HIF_DEVICE *device);

int
HIFConfigureDevice(HIF_DEVICE *device, HIF_DEVICE_CONFIG_OPCODE opcode,
                   void *config, u_int32_t configLen);

/*
 * This API wait for the remaining MBOX messages to be drained
 * This should be moved to HTC AR6K layer
 */
int hifWaitForPendingRecv(HIF_DEVICE *device);

/****************************************************************/
/* BMI and Diag window abstraction                              */
/****************************************************************/

#define HIF_BMI_EXCHANGE_NO_TIMEOUT  ((u_int32_t)(0))

#define DIAG_TRANSFER_LIMIT 2048U /* maximum number of bytes that can be
                                    handled atomically by DiagRead/DiagWrite */

    /* API to handle HIF-specific BMI message exchanges, this API is synchronous
     * and only allowed to be called from a context that can block (sleep) */
int HIFExchangeBMIMsg(HIF_DEVICE *device,
                           u_int8_t    *pSendMessage,
                           u_int32_t   Length,
                           u_int8_t    *pResponseMessage,
                           u_int32_t   *pResponseLength,
                           u_int32_t   TimeoutMS);



    /*
     * APIs to handle HIF specific diagnostic read accesses. These APIs are
     * synchronous and only allowed to be called from a context that can block (sleep).
     * They are not high performance APIs.
     *
     * HIFDiagReadAccess reads a 4 Byte aligned/length value from a Target register
     * or memory word.
     *
     * HIFDiagReadMem reads an arbitrary length of arbitrarily aligned memory.
     */
int HIFDiagReadAccess(HIF_DEVICE *hifDevice, A_UINT32 address, A_UINT32 *data);
int HIFDiagReadMem(HIF_DEVICE *hif_device, A_UINT32 address, A_UINT8 *data, int nbytes);
void HIFDumpTargetMemory(HIF_DEVICE *hif_device, void *ramdump_base,
                           u_int32_t address, u_int32_t size);
    /*
     * APIs to handle HIF specific diagnostic write accesses. These APIs are
     * synchronous and only allowed to be called from a context that can block (sleep).
     * They are not high performance APIs.
     *
     * HIFDiagWriteAccess writes a 4 Byte aligned/length value to a Target register
     * or memory word.
     *
     * HIFDiagWriteMem writes an arbitrary length of arbitrarily aligned memory.
     */
int HIFDiagWriteAccess(HIF_DEVICE *hifDevice, A_UINT32 address, A_UINT32 data);
int HIFDiagWriteMem(HIF_DEVICE *hif_device, A_UINT32 address, A_UINT8 *data, int nbytes);
#if defined(HIF_PCI) && ! defined(A_SIMOS_DEVHOST)
/*
 * This API allows the Host to access Target registers of a given
 * A_target_id_t directly and relatively efficiently over PCIe.
 * This allows the Host to avoid extra overhead associated with
 * sending a message to firmware and waiting for a response message
 * from firmware, as is done on other interconnects.
 *
 * Yet there is some complexity with direct accesses because the
 * Target's power state is not known a priori. The Host must issue
 * special PCIe reads/writes in order to explicitly wake the Target
 * and to verify that it is awake and will remain awake.
 *
 * NB: Host endianness conversion is left for the caller to handle.
 *     These interfaces handle access; not interpretation.
 *
 * Usage:
 *   During initialization, use A_TARGET_ID to obtain an 'target ID'
 *   for use with these interfaces.
 *
 *   Use A_TARGET_READ and A_TARGET_WRITE to access Target space.
 *   These calls must be bracketed by A_TARGET_ACCESS_BEGIN and
 *   A_TARGET_ACCESS_END.  A single BEGIN/END pair is adequate for
 *   multiple READ/WRITE operations.
 *
 *   Use A_TARGET_ACCESS_BEGIN to put the Target in a state in
 *   which it is legal for the Host to directly access it. This
 *   may involve waking the Target from a low power state, which
 *   may take up to 2Ms!
 *
 *   Use A_TARGET_ACCESS_END to tell the Target that as far as
 *   this code path is concerned, it no longer needs to remain
 *   directly accessible.  BEGIN/END is under a reference counter;
 *   multiple code paths may issue BEGIN/END on a single targid.
 *
 *   For added efficiency, the Host may use A_TARGET_ACCESS_LIKELY.
 *   The LIKELY interface works just like A_TARGET_ACCESS_BEGIN,
 *   except that it may return before the Target is actually
 *   available. It's a vague indication that some Target accesses
 *   are expected "soon".  When the LIKELY API is used,
 *   A_TARGET_ACCESS_BEGIN must be used before any access.
 *
 *   There are several uses for the LIKELY/UNLIKELY API:
 *     -If there is some potential time before Target accesses
 *      and we want to get a head start on waking the Target
 *      (e.g. to overlap Target wake with Host-side malloc)
 *     -High-level code knows that it will call low-level
 *      functions that will use BEGIN/END, and we don't want
 *      to allow the Target to sleep until the entire sequence
 *      has completed.
 *
 *   A_TARGET_ACCESS_OK verifies that the Target can be
 *   accessed. In general, this should not be needed, but it
 *   may be useful for debugging or for special uses.
 *
 *   Note that there must be a matching END for each BEGIN
 *       AND   there must be a matching UNLIKELY for each LIKELY!
 *
 *   NB: This API is designed to allow some flexibility in tradeoffs
 *   between Target power utilization and Host efficiency and
 *   system performance.
 */

/*
 * For maximum performance and no power management, set this to 1.
 * For power management at the cost of performance, set this to 0.
 */
#define CONFIG_ATH_PCIE_MAX_PERF 0

/*
 * For keeping the target awake till the driver is
 * loaded, set this to 1
 */
#define CONFIG_ATH_PCIE_AWAKE_WHILE_DRIVER_LOAD 1

/*
 * When CONFIG_ATH_PCIE_MAX_PERF is 0:
 * To use LIKELY hints, set this to 1 (slightly better performance, more power)
 * To ignore "LIKELY" hints, set this to 0 (slightly worse performance, less power)
 */
#if defined(CONFIG_ATH_PCIE_MAX_PERF)
#define CONFIG_ATH_PCIE_ACCESS_LIKELY 0
#else
#define CONFIG_ATH_PCIE_ACCESS_LIKELY 1
#endif

/*
 * Enable/disable CDC max performance workaround
 * For max-performace set this to 0
 * To allow SoC to enter sleep set this to 1
 */
#define CONFIG_DISABLE_CDC_MAX_PERF_WAR 0

/*
 * PCI-E L1 ASPPM sub-states
 * To enable clock gating in L1 state, set this to 1. (less power, slightly more wakeup latency)
 * To disable clock gating in L1 state, set this to 0. (slighly more power)
 */
#define CONFIG_PCIE_ENABLE_L1_CLOCK_GATE 1

/*
 * PCIE_ACCESS_LOG_NUM specifies the number of
 * read/write records to store
 */
#ifdef CONFIG_ATH_PCIE_ACCESS_DEBUG
#define PCIE_ACCESS_LOG_NUM 500
#endif

/* 64-bit MSI support */
#define CONFIG_PCIE_64BIT_MSI 0

/* BAR0 ready checking for AR6320v2 */
#define PCIE_BAR0_READY_CHECKING 0

/* AXI gating when L1, L2 to reduce power consumption */
#define CONFIG_PCIE_ENABLE_AXI_CLK_GATE 0

extern A_target_id_t HIFGetTargetId(HIF_DEVICE *hifDevice);
extern int HIFTargetSleepStateAdjust(A_target_id_t targid, A_BOOL sleep_ok, A_BOOL wait_for_it);
extern void
HIFSetTargetSleep(HIF_DEVICE *hif_device, A_BOOL sleep_ok, A_BOOL wait_for_it);
extern A_BOOL HIFTargetForcedAwake(A_target_id_t targid);
extern void
HIFCancelDeferredTargetSleep(HIF_DEVICE *hif_device);

#define A_TARGET_ID(hifDevice)                HIFGetTargetId(hifDevice)

#if CONFIG_ATH_PCIE_MAX_PERF
#define A_TARGET_ACCESS_BEGIN(targid) \
        do {A_target_id_t unused = (A_target_id_t)(targid); unused = unused;} while(0)

#define A_TARGET_ACCESS_END(targid) \
        do {A_target_id_t unused = (A_target_id_t)(targid); unused = unused;} while(0)

#define A_TARGET_ACCESS_OK(targid) 1

#define A_TARGET_ACCESS_LIKELY(targid) \
        do {A_target_id_t unused = (A_target_id_t)(targid); unused = unused;} while(0)

#define A_TARGET_ACCESS_UNLIKELY(targid) \
        do {A_target_id_t unused = (A_target_id_t)(targid); unused = unused;} while(0)

#define A_TARGET_READ(targid, offset) \
        A_PCI_READ32(TARGID_TO_PCI_ADDR(targid)+(offset))

#if 0
#define A_TARGET_WRITE(targid, offset, value) \
        A_PCI_WRITE32(TARGID_TO_PCI_ADDR(targid)+(offset), (value))
#else /* WORKAROUND */
void WAR_PCI_WRITE32(char *addr, u32 offset, u32 value);
#define A_TARGET_WRITE(targid, offset, value) \
        WAR_PCI_WRITE32(TARGID_TO_PCI_ADDR(targid), (offset), (value))
#endif
#define A_TARGET_ACCESS_BEGIN_RET(targid) \
        do {A_target_id_t unused = (A_target_id_t)(targid); unused = unused;} while(0)

#define A_TARGET_ACCESS_BEGIN_RET_EXT(targid, val) \
        do {A_target_id_t unused = (A_target_id_t)(targid); unused = unused;} while(0)

#define A_TARGET_ACCESS_BEGIN_RET_PTR(targid) \
        do {A_target_id_t unused = (A_target_id_t)(targid); unused = unused;} while(0)

#define A_TARGET_ACCESS_END_RET(targid) \
        do {A_target_id_t unused = (A_target_id_t)(targid); unused = unused;} while(0)

#define A_TARGET_ACCESS_END_RET_EXT(targid, val) \
        do {A_target_id_t unused = (A_target_id_t)(targid); unused = unused;} while(0)

#define A_TARGET_ACCESS_END_RET_PTR(targid) \
        do {A_target_id_t unused = (A_target_id_t)(targid); unused = unused;} while(0)

#else /* CONFIG_ATH_PCIE_MAX_PERF */

void WAR_PCI_WRITE32(char *addr, u32 offset, u32 value);

#define A_TARGET_ACCESS_BEGIN_RET_EXT(targid, val) \
        if (!WLAN_IS_EPPING_ENABLED(vos_get_conparam()) && Q_TARGET_ACCESS_BEGIN(targid) < 0 ) \
             val = -1;

#define A_TARGET_ACCESS_BEGIN_RET(targid) \
        if (!WLAN_IS_EPPING_ENABLED(vos_get_conparam()) && Q_TARGET_ACCESS_BEGIN(targid) < 0) \
            return -1;

#define A_TARGET_ACCESS_BEGIN_RET_PTR(targid) \
        if (!WLAN_IS_EPPING_ENABLED(vos_get_conparam()) && Q_TARGET_ACCESS_BEGIN(targid) < 0) \
            return NULL;

#define A_TARGET_ACCESS_BEGIN(targid) \
        if(Q_TARGET_ACCESS_BEGIN(targid) < 0) \
            return;

#define Q_TARGET_ACCESS_BEGIN(targid) \
        HIFTargetSleepStateAdjust((targid), FALSE, TRUE)

#define A_TARGET_ACCESS_END_RET(targid) \
        if (!WLAN_IS_EPPING_ENABLED(vos_get_conparam()) && Q_TARGET_ACCESS_END(targid) < 0) \
            return -1;

#define A_TARGET_ACCESS_END_RET_EXT(targid, val) \
        if (!WLAN_IS_EPPING_ENABLED(vos_get_conparam()) && Q_TARGET_ACCESS_END(targid) < 0) \
           val = -1;

#define A_TARGET_ACCESS_END_RET_PTR(targid) \
        if (!WLAN_IS_EPPING_ENABLED(vos_get_conparam()) && Q_TARGET_ACCESS_END(targid) < 0) \
            return NULL;

#define A_TARGET_ACCESS_END(targid) \
        if (Q_TARGET_ACCESS_END(targid) < 0) \
            return;

#define Q_TARGET_ACCESS_END(targid) \
        HIFTargetSleepStateAdjust((targid), TRUE, FALSE)

#define A_TARGET_ACCESS_OK(targid)            HIFTargetForcedAwake(targid)

#if CONFIG_ATH_PCIE_ACCESS_LIKELY
#define A_TARGET_ACCESS_LIKELY(targid)        HIFTargetSleepStateAdjust((targid), FALSE, FALSE)
#define A_TARGET_ACCESS_UNLIKELY(targid)      HIFTargetSleepStateAdjust((targid), TRUE, FALSE)
#else /* CONFIG_ATH_PCIE_ACCESS_LIKELY */
#define A_TARGET_ACCESS_LIKELY(targid) \
        do {unsigned long unused = (unsigned long)(targid); unused = unused;} while(0)

#define A_TARGET_ACCESS_UNLIKELY(targid) \
        do {unsigned long unused = (unsigned long)(targid); unused = unused;} while(0)
#endif /* CONFIG_ATH_PCIE_ACCESS_LIKELY */

#ifdef CONFIG_ATH_PCIE_ACCESS_DEBUG
extern A_UINT32 HIFTargetReadChecked(A_target_id_t targid, A_UINT32 offset);
extern void HIFTargetWriteChecked(A_target_id_t targid, A_UINT32 offset, A_UINT32 value);
#define A_TARGET_READ(targid, offset)         HIFTargetReadChecked((targid), (offset))
#define A_TARGET_WRITE(targid, offset, value) HIFTargetWriteChecked((targid), (offset), (value))
#else /* CONFIG_ATH_PCIE_ACCESS_DEBUG */
#define A_TARGET_READ(targid, offset)         A_PCI_READ32(TARGID_TO_PCI_ADDR(targid)+(offset))
#if 0
#define A_TARGET_WRITE(targid, offset, value) \
        A_PCI_WRITE32(TARGID_TO_PCI_ADDR(targid)+(offset), (value))
#else /* WORKAROUND */
#define A_TARGET_WRITE(targid, offset, value) \
        WAR_PCI_WRITE32(TARGID_TO_PCI_ADDR(targid), (offset), (value))
#endif
#endif

#endif /* CONFIG_ATH_PCIE_MAX_PERF */

/*Macro to increment the HIF layer packet error count*/
#define OL_ATH_HIF_PKT_ERROR_COUNT_INCR(_hif_state,_hif_ecode);\
{\
  if(_hif_ecode==HIF_PIPE_NO_RESOURCE)(_hif_state->sc->ol_sc->pkt_stats.hif_pipe_no_resrc_count)+=1;\
}
/*
 * This macro implementation is exposed for efficiency only.
 * The implementation may change and callers should
 * consider the targid to be a completely opaque handle.
 */
#define TARGID_TO_PCI_ADDR(targid) (*((A_target_id_t *)(targid)))

void *HIFDeviceToOsDevice(HIF_DEVICE *hif_device);

#elif defined(A_SIMOS_DEVHOST)

struct ol_softc;

u_int32_t sim_target_register_read(struct ol_softc *scn, u_int32_t addr);
void sim_target_register_write(struct ol_softc *scn, u_int32_t addr, u_int32_t val);

#define A_TARGET_ID(hifDevice) 0
#define A_TARGET_READ(ar, addr) sim_target_register_read(ar, addr)
#define A_TARGET_WRITE(ar, addr, val) sim_target_register_write(ar, addr, val)
#define A_TARGET_ACCESS_BEGIN(targid)
#define A_TARGET_ACCESS_END(targid)

#define HIFDeviceToOsDevice(hif_device) NULL

#else

#define HIFDeviceToOsDevice(hif_device) NULL

#endif

#ifdef IPA_UC_OFFLOAD
/*
 * IPA micro controller data path offload feature enabled,
 * HIF should release copy engine related resource information to IPA UC
 * IPA UC will access hardware resource with released information
 */
void HIFIpaGetCEResource(HIF_DEVICE *hif_device,
                          A_UINT32 *ce_sr_base_paddr,
                          A_UINT32 *ce_sr_ring_size,
                          A_UINT32 *ce_reg_paddr);
#endif /* IPA_UC_OFFLOAD */

#ifdef FEATURE_RUNTIME_PM
/* Runtime power management API of HIF to control
 * runtime pm. During Runtime Suspend the get API
 * return -EAGAIN. The caller can queue the cmd or return.
 * The put API decrements the usage count.
 * The get API increments the usage count.
 * The API's are exposed to HTT and WMI Services only.
 */
int hif_pm_runtime_get(HIF_DEVICE *);
int hif_pm_runtime_put(HIF_DEVICE *);
#else
static inline int hif_pm_runtime_get(HIF_DEVICE *device) { return 0; }
static inline int hif_pm_runtime_put(HIF_DEVICE *device) { return 0; }
#endif
int hif_pm_runtime_prevent_suspend(void *ol_sc);
int hif_pm_runtime_allow_suspend(void *ol_sc);
int hif_pm_runtime_prevent_suspend_timeout(void *ol_sc, unsigned int delay);
#ifdef __cplusplus
}
#endif

#endif /* _HIF_H_ */
