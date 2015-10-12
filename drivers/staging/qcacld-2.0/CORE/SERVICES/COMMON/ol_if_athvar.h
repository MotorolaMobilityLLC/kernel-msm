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

/*
 * Defintions for the Atheros Wireless LAN controller driver.
 */
#ifndef _DEV_OL_ATH_ATHVAR_H
#define _DEV_OL_ATH_ATHVAR_H

#include <osdep.h>
#include <a_types.h>
#include <osapi_linux.h>
#include "adf_os_types.h"
#include "adf_os_lock.h"
#include "wmi_unified_api.h"
#include "htc_api.h"
#include "bmi_msg.h"
#include "ol_txrx_api.h"
#include "ol_txrx_ctrl_api.h"
#include "ol_txrx_osif_api.h"
#include "ol_params.h"
#include <wdi_event_api.h>

#ifdef CONFIG_CNSS
#include <net/cnss.h>
#endif


#include "ol_ctrl_addba_api.h"
typedef void * hif_handle_t;

struct ol_version {
    u_int32_t    host_ver;
    u_int32_t    target_ver;
    u_int32_t    wlan_ver;
    u_int32_t    wlan_ver_1;
    u_int32_t    abi_ver;
};

typedef enum _ATH_BIN_FILE {
    ATH_OTP_FILE,
    ATH_FIRMWARE_FILE,
    ATH_PATCH_FILE,
    ATH_BOARD_DATA_FILE,
    ATH_FLASH_FILE,
    ATH_SETUP_FILE,
} ATH_BIN_FILE;

typedef enum _ol_target_status  {
     OL_TRGET_STATUS_CONNECTED = 0,    /* target connected */
     OL_TRGET_STATUS_RESET,        /* target got reset */
     OL_TRGET_STATUS_EJECT,        /* target got ejected */
     OL_TRGET_STATUS_SUSPEND   /*target got suspend*/
} ol_target_status;

enum ol_ath_tx_ecodes  {
    TX_IN_PKT_INCR=0,
    TX_OUT_HDR_COMPL,
    TX_OUT_PKT_COMPL,
    PKT_ENCAP_FAIL,
    TX_PKT_BAD,
    RX_RCV_MSG_RX_IND,
    RX_RCV_MSG_PEER_MAP,
    RX_RCV_MSG_TYPE_TEST
} ;

#ifdef HIF_SDIO
#define MAX_FILE_NAME     20
struct ol_fw_files {
    char image_file[MAX_FILE_NAME];
    char board_data[MAX_FILE_NAME];
    char otp_data[MAX_FILE_NAME];
    char utf_file[MAX_FILE_NAME];
    char utf_board_data[MAX_FILE_NAME];
    char setup_file[MAX_FILE_NAME];
    char epping_file[MAX_FILE_NAME];
};
#endif

#ifndef ATH_CAP_DCS_CWIM
#define ATH_CAP_DCS_CWIM 0x1
#define ATH_CAP_DCS_WLANIM 0x2
#endif
/*
 * structure to hold the packet error count for CE and hif layer
*/
struct ol_ath_stats {
    int hif_pipe_no_resrc_count;
    int ce_ring_delta_fail_count;
};

#ifdef HIF_USB
/* Magic patterns for FW to report crash information (Rome USB) */
#define FW_ASSERT_PATTERN       0x0000c600
#define FW_REG_PATTERN          0x0000d600
#define FW_REG_END_PATTERN      0x0000e600
#define FW_RAMDUMP_PATTERN      0x0000f600
#define FW_RAMDUMP_END_PATTERN  0x0000f601
#define FW_RAMDUMP_PATTERN_MASK 0xfffffff0

#define FW_REG_DUMP_CNT       60

/* FW RAM segments (Rome USB) */
enum {
    FW_RAM_SEG_DRAM,
    FW_RAM_SEG_IRAM,
    FW_RAM_SEG_AXI,
    FW_RAM_SEG_CNT
};

/* Allocate 384K memory to save each segment of ram dump */
#define FW_RAMDUMP_SEG_SIZE     393216

/* structure to save RAM dump information */
struct fw_ramdump {
    A_UINT32 start_addr;
    A_UINT32 length;
    A_UINT8 *mem;
};
#endif

struct ol_softc {
    /*
     * handle for code that uses the osdep.h version of OS
     * abstraction primitives
     */
    osdev_t         sc_osdev;

    /*
     * handle for code that uses adf version of OS
     * abstraction primitives
     */
    adf_os_device_t   adf_dev;

    struct ol_version      version;

    /* Packet statistics */
    struct ol_ath_stats     pkt_stats;

    u_int32_t target_type;  /* A_TARGET_TYPE_* */
    u_int32_t target_fw_version;
    u_int32_t target_version;
    u_int32_t target_revision;
    u_int8_t  crm_version_string[64];  /* store pHalStartRsp->startRspParams.wcnssCrmVersionString */
    u_int8_t  wlan_version_string[64]; /* store pHalStartRsp->startRspParams.wcnssWlanVersionString */
    ol_target_status  target_status; /* target status */
    bool             is_sim;   /* is this a simulator */
    u_int8_t *cal_in_flash; /* calibration data is stored in flash */
    void *cal_mem; /* virtual address for the calibration data on the flash */

    WLAN_INIT_STATUS    wlan_init_status; /* status of target init */

    /* BMI info */
    void            *bmi_ol_priv; /* OS-dependent private info for BMI */
    bool            bmiDone;
    bool            bmiUADone;
    u_int8_t        *pBMICmdBuf;
    dma_addr_t      BMICmd_pa;
    OS_DMA_MEM_CONTEXT(bmicmd_dmacontext)

    u_int8_t        *pBMIRspBuf;
    dma_addr_t      BMIRsp_pa;
    u_int32_t       last_rxlen; /* length of last response */
    OS_DMA_MEM_CONTEXT(bmirsp_dmacontext)

    void            *MSI_magic;
    dma_addr_t      MSI_magic_dma;
    OS_DMA_MEM_CONTEXT(MSI_dmacontext)

    /* Handles for Lower Layers : filled in at init time */
    hif_handle_t            hif_hdl;
#if defined(HIF_PCI)
    struct hif_pci_softc    *hif_sc;
#elif defined(HIF_USB)
    struct hif_usb_softc    *hif_sc;
#else
    struct ath_hif_sdio_softc    *hif_sc;
#endif

    /* HTC handles */
    void                    *htc_handle;

    bool                   IdlePowerSave;
    int                    ProtocolPowerSave;
    A_BOOL                 fEnableBeaconEarlyTermination;
    u_int8_t               bcnEarlyTermWakeInterval;

    /* ol data path handle */
    ol_txrx_pdev_handle     pdev_txrx_handle;

    /* UTF event information */
    struct {
        u_int8_t            *data;
        u_int32_t           length;
        adf_os_size_t       offset;
        u_int8_t            currentSeq;
        u_int8_t            expectedSeq;
    } utf_event_info;

    struct ol_wow_info      *scn_wowInfo;

#ifdef PERE_IP_HDR_ALIGNMENT_WAR
    bool                    host_80211_enable; /* Enables native-wifi mode on host */
#endif
    bool                    enableuartprint;    /* enable uart/serial prints from target */
    bool                    enablefwlog;        /* enable fwlog */
    /* enable FW self-recovery for Rome USB */
    bool                    enableFwSelfRecovery;
#ifdef HIF_USB
    /* structure to save FW RAM dump (Rome USB) */
    struct fw_ramdump       *ramdump[FW_RAM_SEG_CNT];
    A_UINT8                 ramdump_index;
    bool                    fw_ram_dumping;
#endif

    bool                    enablesinglebinary; /* Use single binary for FW */
    HAL_REG_CAPABILITIES hal_reg_capabilities;
    struct ol_regdmn *ol_regdmn_handle;
    u_int8_t                bcn_mode;
    u_int8_t                arp_override;
    /*
     * Includes host side stack level stats +
     * radio level athstats
     */
    struct wlan_dbg_stats                  ath_stats;
    int16_t               chan_nf;            /* noise_floor */
    u_int32_t               min_tx_power;
    u_int32_t               max_tx_power;
    u_int32_t               txpowlimit2G;
    u_int32_t               txpowlimit5G;
    u_int32_t               txpower_scale;
    u_int32_t               chan_tx_pwr;
    u_int32_t               vdev_count;
    u_int32_t               max_bcn_ie_size;
    adf_os_spinlock_t       scn_lock;

    u_int8_t                vow_extstats;

    u_int8_t                scn_dcs; /* if dcs enabled or not*/
    wdi_event_subscribe     scn_rx_peer_invalid_subscriber;
    u_int8_t                proxy_sta;
    u_int8_t                bcn_enabled;
    u_int8_t                dtcs; /* Dynamic Tx Chainmask Selection enabled/disabled */
    u_int32_t               set_ht_vht_ies:1; /* true if vht ies are set on target */
    bool                    scn_cwmenable;    /*CWM enable/disable state*/
    u_int8_t                max_no_of_peers;
#ifdef CONFIG_CNSS
    struct cnss_fw_files fw_files;
#elif defined(HIF_SDIO)
    struct ol_fw_files fw_files;
#endif
#if defined(CONFIG_CNSS) || defined(HIF_SDIO)
    void *ramdump_base;
    unsigned long ramdump_address;
    unsigned long ramdump_size;
#endif
    bool enable_self_recovery;
#ifdef WLAN_FEATURE_LPSS
    bool                    enablelpasssupport;
#endif
#ifdef FEATURE_SECURE_FIRMWARE
    bool enable_fw_hash_check;
#endif

#ifdef FEATURE_RUNTIME_PM
    bool enable_runtime_pm;
    u_int32_t runtime_pm_delay;
#endif
};

#ifdef PERE_IP_HDR_ALIGNMENT_WAR
#define ol_scn_host_80211_enable_get(_ol_pdev_hdl) \
    ((struct ol_softc *)(_ol_pdev_hdl))->host_80211_enable
#endif

struct bcn_buf_entry {
	A_BOOL                        is_dma_mapped;
	adf_nbuf_t                    bcn_buf;
	TAILQ_ENTRY(bcn_buf_entry)    deferred_bcn_list_elem;
};

struct ol_ath_vap_net80211 {
	struct ol_softc    *av_sc;     /* back pointer to softc */
	ol_txrx_vdev_handle av_txrx_handle;    /* ol data path handle */
	u_int32_t           av_if_id;   /* interface id */
	u_int64_t           av_tsfadjust;       /* Adjusted TSF, host endian */
	bool                av_beacon_offload;  /* Handle beacons in FW */
	adf_nbuf_t          av_wbuf;            /* Beacon buffer */
	A_BOOL              is_dma_mapped;
	os_timer_t          av_timer;
	bool                av_ol_resmgr_wait;  /* UMAC waits for target */
	/*   event to bringup vap*/
	adf_os_spinlock_t   avn_lock;
	TAILQ_HEAD(, bcn_buf_entry)     deferred_bcn_list;
	os_timer_t          av_target_stop_timer;
	bool                av_set_target_stopping;
	bool                av_target_stopped;
};
#define OL_ATH_VAP_NET80211(_vap)      ((struct ol_ath_vap_net80211 *)(_vap))

struct ol_ath_node_net80211 {
    ol_txrx_peer_handle         an_txrx_handle;    /* ol data path handle */
};

#define OL_ATH_NODE_NET80211(_ni)      ((struct ol_ath_node_net80211 *)(_ni))

#define UAPSD_SRV_INTERVAL_DEFAULT_BK_BE_VI     300   /* Default U-APSD Service Interval in msec for BK, BE and VI */
#define UAPSD_SRV_INTERVAL_DEFAULT_VO           20    /* Default U-APSD Service Interval in msec for VO */
#define UAPSD_SUS_INTERVAL_DEFAULT              2000  /* Default U-APSD Suspend Interval in msec for BK, BE and VI */
#define UAPSD_DELAY_INTERVAL_DEFAULT            3000  /* Default U-APSD Delay Interval in msec for BK, BE and VI */
#define UAPSD_USER_PRIO_BE                      0
#define UAPSD_USER_PRIO_BK                      2
#define UAPSD_USER_PRIO_VI                      5
#define UAPSD_USER_PRIO_VO                      7

#define SIR_MAC_DS_PARAM_SET_EID                3
#define SIR_MAC_EDCA_PARAM_SET_EID              12
#define SIR_MAC_CHNL_SWITCH_ANN_EID             37
#define SIR_MAC_QUIET_EID                       40
#define SIR_MAC_ERP_INFO_EID                    42
#define SIR_MAC_QOS_CAPABILITY_EID              46
#define SIR_MAC_HT_INFO_EID                     61

 void ol_target_failure(void *instance, A_STATUS status);

int ol_asf_adf_attach(struct ol_softc *scn);

int ol_ath_detach(struct ol_softc *scn, int force);
void ol_ath_utf_detach(struct ol_softc *scn);
#ifdef QVIT
void ol_ath_qvit_detach(struct ol_softc *scn);
void ol_ath_qvit_attach(struct ol_softc *scn);
#endif

int ol_ath_resume(struct ol_softc *scn);

int ol_ath_suspend(struct ol_softc *scn);

int ol_ath_cwm_attach(struct ol_softc *scn);

u_int8_t *ol_ath_vap_get_myaddr(struct ol_softc *scn, u_int8_t vdev_id);

void ol_ath_utf_attach(struct ol_softc *scn);

void ol_ath_vap_send_hdr_complete(void *ctx, HTC_PACKET_QUEUE *htc_pkt_list);


void ol_rx_indicate(void *ctx, adf_nbuf_t wbuf);

void ol_rx_handler(void *ctx, HTC_PACKET *htc_packet);

void ol_ath_beacon_stop(struct ol_softc *scn,
                   struct ol_ath_vap_net80211 *avn);

u_int32_t host_interest_item_address(u_int32_t target_type, u_int32_t item_offset);

int
ol_ath_set_config_param(struct ol_softc *scn, ol_ath_param_t param, void *buff);

int
ol_ath_get_config_param(struct ol_softc *scn, ol_ath_param_t param, void *buff);

int
ol_hal_set_config_param(struct ol_softc *scn, ol_hal_param_t param, void *buff);

int
ol_hal_get_config_param(struct ol_softc *scn, ol_hal_param_t param, void *buff);

void ol_ath_host_config_update(struct ol_softc *scn);

int ol_ath_suspend_target(struct ol_softc *scn, int disable_target_intr);
int ol_ath_resume_target(struct ol_softc *scn);

int wmi_unified_pdev_get_tpc_config(wmi_unified_t wmi_handle, u_int32_t param);
void ol_get_wlan_dbg_stats(struct ol_softc *scn, struct wlan_dbg_stats *dbg_stats);

int
wmi_unified_node_set_param(wmi_unified_t wmi_handle, u_int8_t *peer_addr,u_int32_t param_id,
        u_int32_t param_val,u_int32_t vdev_id);


#ifdef BIG_ENDIAN_HOST
     /* This API is used in copying in elements to WMI message,
        since WMI message uses multilpes of 4 bytes, This API
        converts length into multiples of 4 bytes, and performs copy
     */
#define OL_IF_MSG_COPY_CHAR_ARRAY(destp, srcp, len)  do { \
      int j; \
      u_int32_t *src, *dest; \
      src = (u_int32_t *)srcp; \
      dest = (u_int32_t *)destp; \
      for(j=0; j < roundup(len, sizeof(u_int32_t))/4; j++) { \
          *(dest+j) = adf_os_le32_to_cpu(*(src+j)); \
      } \
   } while(0)

#else

#define OL_IF_MSG_COPY_CHAR_ARRAY(destp, srcp, len)  do { \
    OS_MEMCPY(destp, srcp, len); \
   } while(0)

#endif

/* Keep Alive KeepAliveParam. */
typedef struct
{
    u_int8_t             keepAliveEnable;//Enable or Disable
    u_int32_t            keepAliveMethod;// Type of frame which need to send for keep alive purpose
    u_int32_t            keepAliveInterval;//Interval in Seconds
    u_int8_t             hostIpv4Addr[4]; //Used only when method type is Arp
    u_int8_t             destIpv4Addr[4];//Used only when method type is Arp
    u_int8_t             destMacAddr[6];//Used only when method type is Arp
} KeepAliveParam, *pKeepAliveParam;

#endif /* _DEV_OL_ATH_ATHVAR_H  */
