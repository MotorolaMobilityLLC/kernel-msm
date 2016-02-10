/*
 * Copyright (c) 2004-2010, 2013-2015 The Linux Foundation. All rights reserved.
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

#ifndef __WLANDEFS_H__
#define __WLANDEFS_H__

#include <a_osapi.h> /* A_COMPILE_TIME_ASSERT */
#include <osdep.h>

/*
 * This file contains WLAN definitions that may be used across both
 * Host and Target software.
 */


/*
 * MAX_SPATIAL_STREAM should be defined in a fwconfig_xxx.h file,
 * but for now provide a default value here in case it's not defined
 * in the fwconfig_xxx.h file.
 */
#ifndef MAX_SPATIAL_STREAM
#define MAX_SPATIAL_STREAM 3
#endif

/*
 * MAX_SPATIAL_STREAM_ANY -
 * what is the largest number of spatial streams that any target supports
 */
#define MAX_SPATIAL_STREAM_ANY 4

#ifndef CONFIG_160MHZ_SUPPORT
#define CONFIG_160MHZ_SUPPORT 0 /* default: 160 MHz channels not supported */
#endif

typedef enum {
    MODE_11A        = 0,   /* 11a Mode */
    MODE_11G        = 1,   /* 11b/g Mode */
    MODE_11B        = 2,   /* 11b Mode */
    MODE_11GONLY    = 3,   /* 11g only Mode */
    MODE_11NA_HT20   = 4,  /* 11a HT20 mode */
    MODE_11NG_HT20   = 5,  /* 11g HT20 mode */
    MODE_11NA_HT40   = 6,  /* 11a HT40 mode */
    MODE_11NG_HT40   = 7,  /* 11g HT40 mode */
    MODE_11AC_VHT20 = 8,
    MODE_11AC_VHT40 = 9,
    MODE_11AC_VHT80 = 10,
    MODE_11AC_VHT20_2G = 11,
    MODE_11AC_VHT40_2G = 12,
    MODE_11AC_VHT80_2G = 13,
#if CONFIG_160MHZ_SUPPORT
    MODE_11AC_VHT80_80 = 14,
    MODE_11AC_VHT160   = 15,
#endif

    MODE_UNKNOWN,
    MODE_UNKNOWN_NO_160MHZ_SUPPORT = 14,
    MODE_UNKNOWN_160MHZ_SUPPORT = 16,

    MODE_MAX        = MODE_UNKNOWN,
    MODE_MAX_NO_160_MHZ_SUPPORT = MODE_UNKNOWN_NO_160MHZ_SUPPORT,
    MODE_MAX_160_MHZ_SUPPORT    = MODE_UNKNOWN_160MHZ_SUPPORT,

} WLAN_PHY_MODE;

#if CONFIG_160MHZ_SUPPORT == 0
A_COMPILE_TIME_ASSERT(
    mode_unknown_value_consistency_Check,
    MODE_UNKNOWN == MODE_UNKNOWN_NO_160MHZ_SUPPORT);
#else
A_COMPILE_TIME_ASSERT(
    mode_unknown_value_consistency_Check,
    MODE_UNKNOWN == MODE_UNKNOWN_160MHZ_SUPPORT);
#endif

typedef enum {
    VHT_MODE_NONE = 0,  /* NON VHT Mode, e.g., HT, DSSS, CCK */
    VHT_MODE_20M = 1,
    VHT_MODE_40M = 2,
    VHT_MODE_80M = 3,
    VHT_MODE_160M = 4
} VHT_OPER_MODE;

typedef enum {
    WLAN_11A_CAPABILITY   = 1,
    WLAN_11G_CAPABILITY   = 2,
    WLAN_11AG_CAPABILITY  = 3,
}WLAN_CAPABILITY;

#if defined(CONFIG_AR900B_SUPPORT) || defined(AR900B)
#define A_RATEMASK A_UINT64
#else
#define A_RATEMASK A_UINT32
#endif

#define A_RATEMASK_NUM_OCTET (sizeof (A_RATEMASK))
#define A_RATEMASK_NUM_BITS ((sizeof (A_RATEMASK)) << 3)


#if CONFIG_160MHZ_SUPPORT
#define IS_MODE_VHT(mode) (((mode) == MODE_11AC_VHT20) || \
        ((mode) == MODE_11AC_VHT40)     || \
        ((mode) == MODE_11AC_VHT80)     || \
        ((mode) == MODE_11AC_VHT80_80) || \
        ((mode) == MODE_11AC_VHT160))
#else
#define IS_MODE_VHT(mode) (((mode) == MODE_11AC_VHT20) || \
        ((mode) == MODE_11AC_VHT40) || \
        ((mode) == MODE_11AC_VHT80))
#endif

#define IS_MODE_VHT_2G(mode) (((mode) == MODE_11AC_VHT20_2G) || \
        ((mode) == MODE_11AC_VHT40_2G) || \
        ((mode) == MODE_11AC_VHT80_2G))


#define IS_MODE_11A(mode)       (((mode) == MODE_11A) || \
                                 ((mode) == MODE_11NA_HT20) || \
                                 ((mode) == MODE_11NA_HT40) || \
                                 (IS_MODE_VHT(mode)))

#define IS_MODE_11B(mode)       ((mode) == MODE_11B)
#define IS_MODE_11G(mode)       (((mode) == MODE_11G) || \
                                 ((mode) == MODE_11GONLY) || \
                                 ((mode) == MODE_11NG_HT20) || \
                                 ((mode) == MODE_11NG_HT40) || \
                                 (IS_MODE_VHT_2G(mode)))
#define IS_MODE_11GN(mode)      (((mode) == MODE_11NG_HT20) || \
                                 ((mode) == MODE_11NG_HT40))
#define IS_MODE_11GONLY(mode)   ((mode) == MODE_11GONLY)


enum {
    REGDMN_MODE_11A              = 0x00000001,  /* 11a channels */
    REGDMN_MODE_TURBO            = 0x00000002,  /* 11a turbo-only channels */
    REGDMN_MODE_11B              = 0x00000004,  /* 11b channels */
    REGDMN_MODE_PUREG            = 0x00000008,  /* 11g channels (OFDM only) */
    REGDMN_MODE_11G              = 0x00000008,  /* XXX historical */
    REGDMN_MODE_108G             = 0x00000020,  /* 11g+Turbo channels */
    REGDMN_MODE_108A             = 0x00000040,  /* 11a+Turbo channels */
    REGDMN_MODE_XR               = 0x00000100,  /* XR channels */
    REGDMN_MODE_11A_HALF_RATE    = 0x00000200,  /* 11A half rate channels */
    REGDMN_MODE_11A_QUARTER_RATE = 0x00000400,  /* 11A quarter rate channels */
    REGDMN_MODE_11NG_HT20        = 0x00000800,  /* 11N-G HT20 channels */
    REGDMN_MODE_11NA_HT20        = 0x00001000,  /* 11N-A HT20 channels */
    REGDMN_MODE_11NG_HT40PLUS    = 0x00002000,  /* 11N-G HT40 + channels */
    REGDMN_MODE_11NG_HT40MINUS   = 0x00004000,  /* 11N-G HT40 - channels */
    REGDMN_MODE_11NA_HT40PLUS    = 0x00008000,  /* 11N-A HT40 + channels */
    REGDMN_MODE_11NA_HT40MINUS   = 0x00010000,  /* 11N-A HT40 - channels */
    REGDMN_MODE_11AC_VHT20       = 0x00020000,  /* 5Ghz, VHT20 */
    REGDMN_MODE_11AC_VHT40PLUS   = 0x00040000,  /* 5Ghz, VHT40 + channels */
    REGDMN_MODE_11AC_VHT40MINUS  = 0x00080000,  /* 5Ghz  VHT40 - channels */
    REGDMN_MODE_11AC_VHT80       = 0x000100000, /* 5Ghz, VHT80 channels */
    REGDMN_MODE_11AC_VHT20_2G    = 0x000200000, /* 2Ghz, VHT20 */
    REGDMN_MODE_11AC_VHT40_2G    = 0x000400000, /* 2Ghz, VHT40 */
    REGDMN_MODE_11AC_VHT80_2G    = 0x000800000, /* 2Ghz, VHT80 */
    REGDMN_MODE_11AC_VHT160      = 0x001000000, /* 5Ghz, VHT160 */
    REGDMN_MODE_11AC_VHT40_2GPLUS  = 0x002000000, /* 2Ghz, VHT40+ */
    REGDMN_MODE_11AC_VHT40_2GMINUS = 0x004000000, /* 2Ghz, VHT40- */
    REGDMN_MODE_11AC_VHT80_80      = 0x008000000, /* 5GHz, VHT80+80 */
};

#define REGDMN_MODE_ALL       (0xFFFFFFFF)       /* REGDMN_MODE_ALL is defined out of the enum
						  * to prevent the ARM compile "warning #66:
						  * enumeration value is out of int range"
						  * Anyway, this is a BIT-OR of all possible values.
						  */

#define REGDMN_CAP1_CHAN_HALF_RATE        0x00000001
#define REGDMN_CAP1_CHAN_QUARTER_RATE     0x00000002
#define REGDMN_CAP1_CHAN_HAL49GHZ         0x00000004


/* regulatory capabilities */
#define REGDMN_EEPROM_EEREGCAP_EN_FCC_MIDBAND   0x0040
#define REGDMN_EEPROM_EEREGCAP_EN_KK_U1_EVEN    0x0080
#define REGDMN_EEPROM_EEREGCAP_EN_KK_U2         0x0100
#define REGDMN_EEPROM_EEREGCAP_EN_KK_MIDBAND    0x0200
#define REGDMN_EEPROM_EEREGCAP_EN_KK_U1_ODD     0x0400
#define REGDMN_EEPROM_EEREGCAP_EN_KK_NEW_11A    0x0800

typedef struct {
    A_UINT32 tlv_header;     /* TLV tag and len; tag equals WMI_TLVTAG_STRUC_HAL_REG_CAPABILITIES */
    A_UINT32 eeprom_rd;      //regdomain value specified in EEPROM
    A_UINT32 eeprom_rd_ext;  //regdomain
    A_UINT32 regcap1;        // CAP1 capabilities bit map.
    A_UINT32 regcap2;        // REGDMN EEPROM CAP.
    A_UINT32 wireless_modes; // REGDMN MODE
    A_UINT32 low_2ghz_chan;
    A_UINT32 high_2ghz_chan;
    A_UINT32 low_5ghz_chan;
    A_UINT32 high_5ghz_chan;
} HAL_REG_CAPABILITIES;

typedef enum {
    WHAL_REG_EXT_FCC_MIDBAND            = 0,
    WHAL_REG_EXT_JAPAN_MIDBAND          = 1,
    WHAL_REG_EXT_FCC_DFS_HT40           = 2,
    WHAL_REG_EXT_JAPAN_NONDFS_HT40      = 3,
    WHAL_REG_EXT_JAPAN_DFS_HT40         = 4,
    WHAL_REG_EXT_FCC_CH_144             = 5,
} WHAL_REG_EXT_BITMAP;

/*
 * Used to update rate-control logic with the status of the tx-completion.
 * In host-based implementation of the rate-control feature, this struture is used to
 * create the payload for HTT message/s from target to host.
 */

typedef struct {
    A_UINT8 rateCode;
    A_UINT8 flags;
}RATE_CODE;

typedef struct {
    RATE_CODE ptx_rc; /* rate code, bw, chain mask sgi */
    A_UINT8 reserved[2];
    A_UINT32 flags;       /* Encodes information such as excessive
                             retransmission, aggregate, some info
                             from .11 frame control,
                             STBC, LDPC, (SGI and Tx Chain Mask
                             are encoded in ptx_rc->flags field),
                             AMPDU truncation (BT/time based etc.),
                             RTS/CTS attempt  */
    A_UINT32 num_enqued;  /* # of MPDUs (for non-AMPDU 1) for this rate */
    A_UINT32 num_retries; /* Total # of transmission attempt for this rate */
    A_UINT32 num_failed;  /* # of failed MPDUs in A-MPDU, 0 otherwise */
    A_UINT32 ack_rssi;    /* ACK RSSI: b'7..b'0 avg RSSI across all chain */
    A_UINT32 time_stamp ; /* ACK timestamp (helps determine age) */
    A_UINT32 is_probe;    /* Valid if probing. Else, 0 */
    A_UINT32 ba_win_size; /* b'7..b0, block Ack Window size, b'31..b8 Resvd */
    A_UINT32 failed_ba_bmap_0_31;  /* failed BA bitmap 0..31 */
    A_UINT32 failed_ba_bmap_32_63; /* failed BA bitmap 32..63 */
    A_UINT32 bmap_tried_0_31;      /* enqued bitmap 0..31 */
    A_UINT32 bmap_tried_32_63;     /* enqued bitmap 32..63 */
} RC_TX_DONE_PARAMS;


#define RC_SET_TX_DONE_INFO(_dst, _rc, _f, _nq, _nr, _nf, _rssi, _ts) \
    do {                                                              \
        (_dst).ptx_rc.rateCode = (_rc).rateCode;                      \
        (_dst).ptx_rc.flags    = (_rc).flags;                         \
        (_dst).flags           = (_f);                                \
        (_dst).num_enqued      = (_nq);                               \
        (_dst).num_retries     = (_nr);                               \
        (_dst).num_failed      = (_nf);                               \
        (_dst).ack_rssi        = (_rssi);                             \
        (_dst).time_stamp      = (_ts);                               \
    } while (0)

#define RC_SET_TXBF_DONE_INFO(_dst, _f)                                 \
    do {                                                                \
        (_dst).flags           |= (_f);                                 \
    } while (0)

/* NOTE: NUM_DYN_BW and NUM_SCHED_ENTRIES cannot be changed without breaking WMI Compatibility */
#define NUM_SCHED_ENTRIES           2
#define NUM_DYN_BW_MAX              4

/* Some products only use 20/40/80; some use 20/40/80/160 */
#ifndef NUM_DYN_BW
#define NUM_DYN_BW                  3 /* default: support up through 80 MHz */
#endif

#define NUM_DYN_BW_MASK             0x3

#define PROD_SCHED_BW_ENTRIES       (NUM_SCHED_ENTRIES * NUM_DYN_BW)
typedef A_UINT8 A_RATE;

#if NUM_DYN_BW  > 4
// Extend rate table module first
#error "Extend rate table module first"
#endif

#define MAX_IBSS_PEERS 32

#if defined(CONFIG_AR900B_SUPPORT) || defined(AR900B)
typedef struct{
    A_UINT32    psdu_len    [NUM_DYN_BW * NUM_SCHED_ENTRIES];
    A_UINT16    flags[NUM_SCHED_ENTRIES][NUM_DYN_BW];
    A_RATE      rix[NUM_SCHED_ENTRIES][NUM_DYN_BW];
    A_UINT8     tpc[NUM_SCHED_ENTRIES][NUM_DYN_BW];
    A_UINT32    antmask[NUM_SCHED_ENTRIES];
    A_UINT8     num_mpdus   [NUM_DYN_BW * NUM_SCHED_ENTRIES];
    A_UINT16    txbf_cv_len;
    A_UINT32    txbf_cv_ptr;
    A_UINT16    txbf_flags;
    A_UINT16    txbf_cv_size;
    A_UINT8     txbf_nc_idx;
    A_UINT8     tries[NUM_SCHED_ENTRIES];
    A_UINT8     bw_mask[NUM_SCHED_ENTRIES];
    A_UINT8     max_bw[NUM_SCHED_ENTRIES];
    A_UINT8     num_sched_entries;
    A_UINT8     paprd_mask;
    A_UINT8     rts_rix;
    A_UINT8     sh_pream;
    A_UINT8     min_spacing_1_4_us;
    A_UINT8     fixed_delims;
    A_UINT8     bw_in_service;
    A_RATE      probe_rix;
    A_UINT8     num_valid_rates;
    A_UINT8     rtscts_tpc;
    A_UINT8     dd_profile;
} RC_TX_RATE_SCHEDULE;

#else
typedef struct{
    A_UINT32    psdu_len    [NUM_DYN_BW * NUM_SCHED_ENTRIES];
    A_UINT16    flags       [NUM_DYN_BW * NUM_SCHED_ENTRIES];
    A_RATE      rix         [NUM_DYN_BW * NUM_SCHED_ENTRIES];
    A_UINT8     tpc         [NUM_DYN_BW * NUM_SCHED_ENTRIES];
    A_UINT8     num_mpdus   [NUM_DYN_BW * NUM_SCHED_ENTRIES];
    A_UINT32    antmask     [NUM_SCHED_ENTRIES];
    A_UINT32    txbf_cv_ptr;
    A_UINT16    txbf_cv_len;
    A_UINT8     tries       [NUM_SCHED_ENTRIES];
    A_UINT8     num_valid_rates;
    A_UINT8     paprd_mask;
    A_UINT8     rts_rix;
    A_UINT8     sh_pream;
    A_UINT8     min_spacing_1_4_us;
    A_UINT8     fixed_delims;
    A_UINT8     bw_in_service;
    A_RATE      probe_rix;
} RC_TX_RATE_SCHEDULE;
#endif

typedef struct{
    A_UINT16    flags       [NUM_DYN_BW * NUM_SCHED_ENTRIES];
    A_RATE      rix         [NUM_DYN_BW * NUM_SCHED_ENTRIES];
#ifdef DYN_TPC_ENABLE
    A_UINT8     tpc         [NUM_DYN_BW * NUM_SCHED_ENTRIES];
#endif
#ifdef SECTORED_ANTENNA
    A_UINT32    antmask     [NUM_SCHED_ENTRIES];
#endif
    A_UINT8     tries       [NUM_SCHED_ENTRIES];
    A_UINT8     num_valid_rates;
    A_UINT8     rts_rix;
    A_UINT8     sh_pream;
    A_UINT8     bw_in_service;
    A_RATE      probe_rix;
    A_UINT8     dd_profile;
} RC_TX_RATE_INFO;

/*
 * Temporarily continue to provide the WHAL_RC_INIT_RC_MASKS def in wlan_defs.h
 * for older targets.
 * The WHAL_RX_INIT_RC_MASKS macro def needs to be moved into ratectrl_11ac.h
 * for all targets, but until this is complete, the WHAL_RC_INIT_RC_MASKS def
 * will be maintained here in its old location.
 */
#if CONFIG_160MHZ_SUPPORT == 0
#define WHAL_RC_INIT_RC_MASKS(_rm) do {                                     \
        _rm[WHAL_RC_MASK_IDX_NON_HT] = A_RATEMASK_OFDM_CCK;                 \
        _rm[WHAL_RC_MASK_IDX_HT_20] = A_RATEMASK_HT_20;                     \
        _rm[WHAL_RC_MASK_IDX_HT_40] = A_RATEMASK_HT_40;                     \
        _rm[WHAL_RC_MASK_IDX_VHT_20] = A_RATEMASK_VHT_20;                   \
        _rm[WHAL_RC_MASK_IDX_VHT_40] = A_RATEMASK_VHT_40;                   \
        _rm[WHAL_RC_MASK_IDX_VHT_80] = A_RATEMASK_VHT_80;                   \
        } while (0)
#endif

/**
 * strucutre describing host memory chunk.
 */
typedef struct {
   A_UINT32   tlv_header;     /* TLV tag and len; tag equals WMITLV_TAG_STRUC_wlan_host_memory_chunk */
   /** id of the request that is passed up in service ready */
   A_UINT32 req_id;
   /** the physical address the memory chunk */
   A_UINT32 ptr;
   /** size of the chunk */
   A_UINT32 size;
} wlan_host_memory_chunk;

#define NUM_UNITS_IS_NUM_VDEVS        0x1
#define NUM_UNITS_IS_NUM_PEERS        0x2
#define NUM_UNITS_IS_NUM_ACTIVE_PEERS 0x4
/* request host to allocate memory contiguously */
#define REQ_TO_HOST_FOR_CONT_MEMORY   0x8

/**
 * structure used by FW for requesting host memory
 */
typedef struct {
    A_UINT32    tlv_header;     /* TLV tag and len; tag equals WMI_TLVTAG_STRUC_wlan_host_mem_req */

    /** ID of the request */
    A_UINT32    req_id;
    /** size of the  of each unit */
    A_UINT32    unit_size;
    /**
     * flags to  indicate that
     * the number units is dependent
     * on number of resources(num vdevs num peers .. etc)
     */
    A_UINT32    num_unit_info;
    /*
     * actual number of units to allocate . if flags in the num_unit_info
     * indicate that number of units is tied to number of a particular
     * resource to allocate then  num_units filed is set to 0 and host
     * will derive the number units from number of the resources it is
     * requesting.
     */
    A_UINT32    num_units;
} wlan_host_mem_req;

typedef enum {
    IGNORE_DTIM = 0x01,
    NORMAL_DTIM = 0x02,
    STICK_DTIM  = 0x03,
    AUTO_DTIM   = 0x04,
} BEACON_DTIM_POLICY;

/* During test it is observed that 6 * 400 = 2400 can
 * be alloced in addition to CFG_TGT_NUM_MSDU_DESC.
 * If there is any change memory requirement, this number
 * needs to be revisited. */
#define TOTAL_VOW_ALLOCABLE 2400
#define VOW_DESC_GRAB_MAX 800

#define VOW_GET_NUM_VI_STA(vow_config) (((vow_config) & 0xffff0000) >> 16)
#define VOW_GET_DESC_PER_VI_STA(vow_config) ((vow_config) & 0x0000ffff)

/***TODO!!! Get these values dynamically in WMI_READY event and use it to calculate the mem req*/
/* size in bytes required for msdu descriptor. If it changes, this should be updated. LARGE_AP
 * case is not considered. LARGE_AP is disabled when VoW is enabled.*/
#define MSDU_DESC_SIZE 20

/* size in bytes required to support a peer in target.
 * This obtained by considering Two tids per peer.
 * peer structure = 168 bytes
 * tid = 96 bytes (per sta 2 means we need 192 bytes)
 * peer_cb = 16 * 2
 * key = 52 * 2
 * AST = 12 * 2
 * rate, reorder.. = 384
 * smart antenna = 50
 */
#define MEMORY_REQ_FOR_PEER 800
/*
 * NB: it is important to keep all the fields in the structure dword long
 * so that it is easy to handle the statistics in BE host.
 */

struct wlan_dbg_tx_stats {
    /* Num HTT cookies queued to dispatch list */
    A_INT32 comp_queued;
    /* Num HTT cookies dispatched */
    A_INT32 comp_delivered;
    /* Num MSDU queued to WAL */
    A_INT32 msdu_enqued;
    /* Num MPDU queue to WAL */
    A_INT32 mpdu_enqued;
    /* Num MSDUs dropped by WMM limit */
    A_INT32 wmm_drop;
    /* Num Local frames queued */
    A_INT32 local_enqued;
    /* Num Local frames done */
    A_INT32 local_freed;
    /* Num queued to HW */
    A_INT32 hw_queued;
    /* Num PPDU reaped from HW */
    A_INT32 hw_reaped;
    /* Num underruns */
    A_INT32 underrun;
#if defined(AR900B)
    /* HW Paused. */
    A_UINT32 hw_paused;
#endif
    /* Num PPDUs cleaned up in TX abort */
    A_INT32 tx_abort;
    /* Num MPDUs requed by SW */
    A_INT32 mpdus_requed;
    /* excessive retries */
    A_UINT32 tx_ko;
#if defined(AR900B)
    A_UINT32 tx_xretry;
#endif
    /* data hw rate code */
    A_UINT32 data_rc;
    /* Scheduler self triggers */
    A_UINT32 self_triggers;
    /* frames dropped due to excessive sw retries */
    A_UINT32 sw_retry_failure;
    /* illegal rate phy errors  */
    A_UINT32 illgl_rate_phy_err;
    /* wal pdev continous xretry */
    A_UINT32 pdev_cont_xretry;
    /* wal pdev continous xretry */
    A_UINT32 pdev_tx_timeout;
    /* wal pdev resets  */
    A_UINT32 pdev_resets;
    /* frames dropped due to non-availability of stateless TIDs */
    A_UINT32 stateless_tid_alloc_failure;
    /* PhY/BB underrun */
    A_UINT32 phy_underrun;
    /* MPDU is more than txop limit */
    A_UINT32 txop_ovf;
#if defined(AR900B)
    /* Number of Sequences posted */
    A_UINT32 seq_posted;
    /* Number of Sequences failed queueing */
    A_UINT32 seq_failed_queueing;
    /* Number of Sequences completed */
    A_UINT32 seq_completed;
    /* Number of Sequences restarted */
    A_UINT32 seq_restarted;
    /* Number of MU Sequences posted */
    A_UINT32 mu_seq_posted;
    /* Num MPDUs flushed by SW, HWPAUSED, SW TXABORT (Reset,channel change) */
    A_INT32 mpdus_sw_flush;
    /* Num MPDUs filtered by HW, all filter condition (TTL expired) */
    A_INT32 mpdus_hw_filter;
    /* Num MPDUs truncated by PDG (TXOP, TBTT, PPDU_duration based on rate, dyn_bw) */
    A_INT32 mpdus_truncated;
    /* Num MPDUs that was tried but didn't receive ACK or BA */
    A_INT32 mpdus_ack_failed;
    /* Num MPDUs that was dropped du to expiry. */
    A_INT32 mpdus_expired;
    /* Num mc drops */
    //A_UINT32 mc_drop;
#endif
};

struct wlan_dbg_rx_stats {
    /* Cnts any change in ring routing mid-ppdu */
    A_INT32 mid_ppdu_route_change;
    /* Total number of statuses processed */
    A_INT32 status_rcvd;
    /* Extra frags on rings 0-3 */
    A_INT32 r0_frags;
    A_INT32 r1_frags;
    A_INT32 r2_frags;
    A_INT32 r3_frags;
    /* MSDUs / MPDUs delivered to HTT */
    A_INT32 htt_msdus;
    A_INT32 htt_mpdus;
    /* MSDUs / MPDUs delivered to local stack */
    A_INT32 loc_msdus;
    A_INT32 loc_mpdus;
    /* AMSDUs that have more MSDUs than the status ring size */
    A_INT32 oversize_amsdu;
    /* Number of PHY errors */
    A_INT32 phy_errs;
    /* Number of PHY errors drops */
    A_INT32 phy_err_drop;
    /* Number of mpdu errors - FCS, MIC, ENC etc. */
    A_INT32 mpdu_errs;
#if defined(AR900B)
    /* Number of rx overflow errors. */
    A_INT32 rx_ovfl_errs;
#endif
};


struct wlan_dbg_mem_stats {
    A_UINT32 iram_free_size;
    A_UINT32 dram_free_size;
};

struct wlan_dbg_peer_stats {

	A_INT32 dummy; /* REMOVE THIS ONCE REAL PEER STAT COUNTERS ARE ADDED */
};

typedef struct {
    A_UINT32 mcs[10];
    A_UINT32 sgi[10];
    A_UINT32 nss[4];
    A_UINT32 nsts;
    A_UINT32 stbc[10];
    A_UINT32 bw[3];
    A_UINT32 pream[6];
    A_UINT32 ldpc;
    A_UINT32 txbf;
    A_UINT32 mgmt_rssi;
    A_UINT32 data_rssi;
    A_UINT32 rssi_chain0;
    A_UINT32 rssi_chain1;
    A_UINT32 rssi_chain2;
/*
 * TEMPORARY: leave rssi_chain3 in place for AR900B builds until code using
 * rssi_chain3 has been converted to use wlan_dbg_rx_rate_info_v2_t.
 * At that time, this rssi_chain3 field will be deleted.
 */
#if defined(AR900B)
    A_UINT32 rssi_chain3;
#endif
} wlan_dbg_rx_rate_info_t ;

typedef struct {
    A_UINT32 mcs[10];
    A_UINT32 sgi[10];
/*
 * TEMPORARY: leave nss conditionally defined, until all code that
 * requires nss[4] is converted to use wlan_dbg_tx_rate_info_v2_t.
 * At that time, this nss array will be made length = 3 unconditionally.
 */
#if defined(CONFIG_AR900B_SUPPORT) || defined(AR900B)
    A_UINT32 nss[4];
#else
    A_UINT32 nss[3];
#endif
    A_UINT32 stbc[10];
    A_UINT32 bw[3];
    A_UINT32 pream[4];
    A_UINT32 ldpc;
    A_UINT32 rts_cnt;
    A_UINT32 ack_rssi;
} wlan_dbg_tx_rate_info_t ;

#define WLAN_MAX_MCS 10

typedef struct {
    A_UINT32 mcs[WLAN_MAX_MCS];
    A_UINT32 sgi[WLAN_MAX_MCS];
    A_UINT32 nss[MAX_SPATIAL_STREAM_ANY];
    A_UINT32 nsts;
    A_UINT32 stbc[WLAN_MAX_MCS];
    A_UINT32 bw[NUM_DYN_BW_MAX];
    A_UINT32 pream[6];
    A_UINT32 ldpc;
    A_UINT32 txbf;
    A_UINT32 mgmt_rssi;
    A_UINT32 data_rssi;
    A_UINT32 rssi_chain0;
    A_UINT32 rssi_chain1;
    A_UINT32 rssi_chain2;
    A_UINT32 rssi_chain3;
    A_UINT32 reserved[8];
} wlan_dbg_rx_rate_info_v2_t ;

typedef struct {
    A_UINT32 mcs[WLAN_MAX_MCS];
    A_UINT32 sgi[WLAN_MAX_MCS];
    A_UINT32 nss[MAX_SPATIAL_STREAM_ANY];
    A_UINT32 stbc[WLAN_MAX_MCS];
    A_UINT32 bw[NUM_DYN_BW_MAX];
    A_UINT32 pream[4];
    A_UINT32 ldpc;
    A_UINT32 rts_cnt;
    A_UINT32 ack_rssi;
    A_UINT32 reserved[8];
} wlan_dbg_tx_rate_info_v2_t ;

#define WHAL_DBG_PHY_ERR_MAXCNT 18
#define WHAL_DBG_SIFS_STATUS_MAXCNT 8
#define WHAL_DBG_SIFS_ERR_MAXCNT 8
#define WHAL_DBG_CMD_RESULT_MAXCNT 10
#define WHAL_DBG_CMD_STALL_ERR_MAXCNT 4
#define WHAL_DBG_FLUSH_REASON_MAXCNT 40

typedef enum {
    WIFI_URRN_STATS_FIRST_PKT,
    WIFI_URRN_STATS_BETWEEN_MPDU,
    WIFI_URRN_STATS_WITHIN_MPDU,
    WHAL_MAX_URRN_STATS
} wifi_urrn_type_t;

typedef struct wlan_dbg_txbf_snd_stats {
    A_UINT32 cbf_20[4];
    A_UINT32 cbf_40[4];
    A_UINT32 cbf_80[4];
    A_UINT32 sounding[9];
}wlan_dbg_txbf_snd_stats_t;

typedef struct wlan_dbg_wifi2_error_stats {
    A_UINT32 urrn_stats[WHAL_MAX_URRN_STATS];
    A_UINT32 flush_errs[WHAL_DBG_FLUSH_REASON_MAXCNT];
    A_UINT32 schd_stall_errs[WHAL_DBG_CMD_STALL_ERR_MAXCNT];
    A_UINT32 schd_cmd_result[WHAL_DBG_CMD_RESULT_MAXCNT];
    A_UINT32 sifs_status[WHAL_DBG_SIFS_STATUS_MAXCNT];
    A_UINT8  phy_errs[WHAL_DBG_PHY_ERR_MAXCNT];
    A_UINT32 rx_rate_inval;
}wlan_dbg_wifi2_error_stats_t;

typedef struct wlan_dbg_wifi2_error2_stats {
    A_UINT32 schd_errs[WHAL_DBG_CMD_STALL_ERR_MAXCNT];
    A_UINT32 sifs_errs[WHAL_DBG_SIFS_ERR_MAXCNT];
}wlan_dbg_wifi2_error2_stats_t;

#define WLAN_DBG_STATS_SIZE_TXBF_VHT 10
#define WLAN_DBG_STATS_SIZE_TXBF_HT 8
#define WLAN_DBG_STATS_SIZE_TXBF_OFDM 8
#define WLAN_DBG_STATS_SIZE_TXBF_CCK 7

typedef struct wlan_dbg_txbf_data_stats {
    A_UINT32 tx_txbf_vht[WLAN_DBG_STATS_SIZE_TXBF_VHT];
    A_UINT32 rx_txbf_vht[WLAN_DBG_STATS_SIZE_TXBF_VHT];
    A_UINT32 tx_txbf_ht[WLAN_DBG_STATS_SIZE_TXBF_HT];
    A_UINT32 tx_txbf_ofdm[WLAN_DBG_STATS_SIZE_TXBF_OFDM];
    A_UINT32 tx_txbf_cck[WLAN_DBG_STATS_SIZE_TXBF_CCK];
} wlan_dbg_txbf_data_stats_t;

struct wlan_dbg_tx_mu_stats {
    A_UINT32 mu_sch_nusers_2;
    A_UINT32 mu_sch_nusers_3;
    A_UINT32 mu_mpdus_queued_usr[4];
    A_UINT32 mu_mpdus_tried_usr[4];
    A_UINT32 mu_mpdus_failed_usr[4];
    A_UINT32 mu_mpdus_requeued_usr[4];
    A_UINT32 mu_err_no_ba_usr[4];
    A_UINT32 mu_mpdu_underrun_usr[4];
    A_UINT32 mu_ampdu_underrun_usr[4];
};

struct wlan_dbg_tx_selfgen_stats {
    A_UINT32 su_ndpa;
    A_UINT32 su_ndp;
    A_UINT32 mu_ndpa;
    A_UINT32 mu_ndp;
    A_UINT32 mu_brpoll_1;
    A_UINT32 mu_brpoll_2;
    A_UINT32 mu_bar_1;
    A_UINT32 mu_bar_2;
    A_UINT32 cts_burst;
    A_UINT32 su_ndp_err;
    A_UINT32 su_ndpa_err;
    A_UINT32 mu_ndp_err;
    A_UINT32 mu_brp1_err;
    A_UINT32 mu_brp2_err;
};

typedef struct wlan_dbg_sifs_resp_stats {
    A_UINT32 ps_poll_trigger;       /* num ps-poll trigger frames */
    A_UINT32 uapsd_trigger;         /* num uapsd trigger frames */
    A_UINT32 qb_data_trigger[2];    /* num data trigger frames; idx 0: explicit and idx 1: implicit */
    A_UINT32 qb_bar_trigger[2];     /* num bar trigger frames;  idx 0: explicit and idx 1: implicit */
    A_UINT32 sifs_resp_data;        /* num ppdus transmitted at SIFS interval */
    A_UINT32 sifs_resp_err;         /* num ppdus failed to meet SIFS resp timing */
} wlan_dgb_sifs_resp_stats_t;



/** wlan_dbg_wifi2_error_stats_t is not grouped with the
 *  following structure as it is allocated differently and only
 *  belongs to whal
 */
typedef struct wlan_dbg_stats_wifi2 {
    wlan_dbg_txbf_snd_stats_t txbf_snd_info;
    wlan_dbg_txbf_data_stats_t txbf_data_info;
    struct wlan_dbg_tx_selfgen_stats tx_selfgen;
    struct wlan_dbg_tx_mu_stats tx_mu;
    wlan_dgb_sifs_resp_stats_t sifs_resp_info;
} wlan_dbg_wifi2_stats_t;

typedef struct {
    wlan_dbg_rx_rate_info_t rx_phy_info;
    wlan_dbg_tx_rate_info_t tx_rate_info;
} wlan_dbg_rate_info_t;

typedef struct {
    wlan_dbg_rx_rate_info_v2_t rx_phy_info;
    wlan_dbg_tx_rate_info_v2_t tx_rate_info;
} wlan_dbg_rate_info_v2_t;

struct wlan_dbg_stats {
    struct wlan_dbg_tx_stats tx;
    struct wlan_dbg_rx_stats rx;
#if defined(AR900B)
    struct wlan_dbg_mem_stats mem;
#endif
    struct wlan_dbg_peer_stats peer;
};

#define DBG_STATS_MAX_HWQ_NUM 10
#define DBG_STATS_MAX_TID_NUM 20
#define DBG_STATS_MAX_CONG_NUM 16
struct wlan_dbg_txq_stats {
    A_UINT16 num_pkts_queued[DBG_STATS_MAX_HWQ_NUM];
    A_UINT16 tid_hw_qdepth[DBG_STATS_MAX_TID_NUM];//WAL_MAX_TID is 20
    A_UINT16 tid_sw_qdepth[DBG_STATS_MAX_TID_NUM];//WAL_MAX_TID is 20
};

struct wlan_dbg_tidq_stats{
    A_UINT32 wlan_dbg_tid_txq_status;
    struct wlan_dbg_txq_stats txq_st;
};

#endif /* __WLANDEFS_H__ */
