/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

//------------------------------------------------------------------------------
// <copyright file="targaddrs.h" company="Atheros">
//    Copyright (c) 2010 Atheros Corporation.  All rights reserved.
// $ATH_LICENSE_HOSTSDK0_C$
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================

#ifndef __TARGADDRS_H__
#define __TARGADDRS_H__

#if defined(ATH_TARGET)
#include "soc_addrs.h"
#endif

#if !defined(ATH_TARGET)
#include "athstartpack.h"
#endif

/*
 * SOC option bits, to enable/disable various features.
 * By default, all option bits are 0.
 * AR6004: These bits can be set in LOCAL_SCRATCH register 0.
 * AR9888: These bits can be set in soc_core register SCRATCH_0.
 */
#define SOC_OPTION_BMI_DISABLE      0x01 /* Disable BMI comm with Host */
#define SOC_OPTION_SERIAL_ENABLE    0x02 /* Enable serial port msgs */
#define SOC_OPTION_WDT_DISABLE      0x04 /* WatchDog Timer override */
#define SOC_OPTION_SLEEP_DISABLE    0x08 /* Disable system sleep */
#define SOC_OPTION_STOP_BOOT        0x10 /* Stop boot processes (for ATE) */
#define SOC_OPTION_ENABLE_NOANI     0x20 /* Operate without ANI */
#define SOC_OPTION_DSET_DISABLE     0x40 /* Ignore DataSets */
#define SOC_OPTION_IGNORE_FLASH     0x80 /* Ignore flash during bootup */

/*
 * xxx_HOST_INTEREST_ADDRESS is the address in Target RAM of the
 * host_interest structure.  It must match the address of the _host_interest
 * symbol (see linker script).
 *
 * Host Interest is shared between Host and Target in order to coordinate
 * between the two, and is intended to remain constant (with additions only
 * at the end) across software releases.
 *
 * All addresses are available here so that it's possible to
 * write a single binary that works with all Target Types.
 * May be used in assembler code as well as C.
 */
#define AR6002_HOST_INTEREST_ADDRESS    0x00500400
#define AR6003_HOST_INTEREST_ADDRESS    0x00540600
#define AR6004_HOST_INTEREST_ADDRESS    0x00400800
#define AR9888_HOST_INTEREST_ADDRESS    0x00400800
#define AR900B_HOST_INTEREST_ADDRESS    0x00400800
#define AR6320_HOST_INTEREST_ADDRESS    0x00400800
#define AR6004_SOC_RESET_ADDRESS        0X00004000
#define AR6004_SOC_RESET_CPU_INIT_RESET_MASK        0X00000800
#if defined(AR6006_MEMORY_NEW_ARCH)
#define AR6006_HOST_INTEREST_ADDRESS                0x00428800
#else
#define AR6006_HOST_INTEREST_ADDRESS                0x00400800
#endif
#define AR6006_SOC_RESET_ADDRESS                    0X00004000
#define AR6006_SOC_RESET_CPU_INIT_RESET_MASK        0X00000800



#define HOST_INTEREST_MAX_SIZE          0x200

#if !defined(__ASSEMBLER__)
struct register_dump_s;
struct dbglog_hdr_s;

/*
 * These are items that the Host may need to access
 * via BMI or via the Diagnostic Window. The position
 * of items in this structure must remain constant
 * across firmware revisions!
 *
 * Types for each item must be fixed size across
 * target and host platforms.
 *
 * More items may be added at the end.
 */
PREPACK64 struct host_interest_s {
    /*
     * Pointer to application-defined area, if any.
     * Set by Target application during startup.
     */
    A_UINT32               hi_app_host_interest;                      /* 0x00 */

    /* Pointer to register dump area, valid after Target crash. */
    A_UINT32               hi_failure_state;                          /* 0x04 */

    /* Pointer to debug logging header */
    A_UINT32               hi_dbglog_hdr;                             /* 0x08 */

    /* Save SW ROM version */
    A_UINT32               hi_sw_rom_version;                         /* 0x0c */

    /*
     * General-purpose flag bits, similar to SOC_OPTION_* flags.
     * Can be used by application rather than by OS.
     */
    A_UINT32               hi_option_flag;                            /* 0x10 */

    /*
     * Boolean that determines whether or not to
     * display messages on the serial port.
     */
    A_UINT32               hi_serial_enable;                          /* 0x14 */

    /* Start address of DataSet index, if any */
    A_UINT32               hi_dset_list_head;                         /* 0x18 */

    /* Override Target application start address */
    A_UINT32               hi_app_start;                              /* 0x1c */

    /* Clock and voltage tuning */
    A_UINT32               hi_skip_clock_init;                        /* 0x20 */
    A_UINT32               hi_core_clock_setting;                     /* 0x24 */
    A_UINT32               hi_cpu_clock_setting;                      /* 0x28 */
    A_UINT32               hi_system_sleep_setting;                   /* 0x2c */
    A_UINT32               hi_xtal_control_setting;                   /* 0x30 */
    A_UINT32               hi_pll_ctrl_setting_24ghz;                 /* 0x34 */
    A_UINT32               hi_pll_ctrl_setting_5ghz;                  /* 0x38 */
    A_UINT32               hi_ref_voltage_trim_setting;               /* 0x3c */
    A_UINT32               hi_clock_info;                             /* 0x40 */

    /* Host uses BE CPU or not */
    A_UINT32               hi_be;                                     /* 0x44 */

    A_UINT32               hi_stack;  /* normal stack */              /* 0x48 */
    A_UINT32               hi_err_stack; /* error stack */            /* 0x4c */
    A_UINT32               hi_desired_cpu_speed_hz;                   /* 0x50 */

    /* Pointer to Board Data  */
    A_UINT32               hi_board_data;                             /* 0x54 */

    /*
     * Indication of Board Data state:
     *    0: board data is not yet initialized.
     *    1: board data is initialized; unknown size
     *   >1: number of bytes of initialized board data (varies with board type)
     */
    A_UINT32               hi_board_data_initialized;                 /* 0x58 */

    A_UINT32               hi_dset_RAM_index_table;                   /* 0x5c */

    A_UINT32               hi_desired_baud_rate;                      /* 0x60 */
    A_UINT32               hi_dbglog_config;                          /* 0x64 */
    A_UINT32               hi_end_RAM_reserve_sz;                     /* 0x68 */
    A_UINT32               hi_mbox_io_block_sz;                       /* 0x6c */

    A_UINT32               hi_num_bpatch_streams;                     /* 0x70 -- unused */
    A_UINT32               hi_mbox_isr_yield_limit;                   /* 0x74 */

    A_UINT32               hi_refclk_hz;                              /* 0x78 */
    A_UINT32               hi_ext_clk_detected;                       /* 0x7c */
    A_UINT32               hi_dbg_uart_txpin;                         /* 0x80 */
    A_UINT32               hi_dbg_uart_rxpin;                         /* 0x84 */
    A_UINT32               hi_hci_uart_baud;                          /* 0x88 */
    A_UINT32               hi_hci_uart_pin_assignments;               /* 0x8C */
        /* NOTE: byte [0] = tx pin, [1] = rx pin, [2] = rts pin, [3] = cts pin */
    A_UINT32               hi_hci_uart_baud_scale_val;                /* 0x90 */
    A_UINT32               hi_hci_uart_baud_step_val;                 /* 0x94 */

    A_UINT32               hi_allocram_start;                         /* 0x98 */
    A_UINT32               hi_allocram_sz;                            /* 0x9c */
    A_UINT32               hi_hci_bridge_flags;                       /* 0xa0 */
    A_UINT32               hi_hci_uart_support_pins;                  /* 0xa4 */
        /* NOTE: byte [0] = RESET pin (bit 7 is polarity), bytes[1]..bytes[3] are for future use */
    A_UINT32               hi_hci_uart_pwr_mgmt_params;               /* 0xa8 */
        /* 0xa8 - [1]: 0 = UART FC active low, 1 = UART FC active high
         *        [31:16]: wakeup timeout in ms
         */
    /* Pointer to extended board Data  */
    A_UINT32               hi_board_ext_data;                         /* 0xac */
    A_UINT32               hi_board_ext_data_config;                  /* 0xb0 */
        /*
         * Bit [0]  :   valid
         * Bit[31:16:   size
         */
   /*
     * hi_reset_flag is used to do some stuff when target reset.
     * such as restore app_start after warm reset or
     * preserve host Interest area, or preserve ROM data, literals etc.
     */
    A_UINT32                hi_reset_flag;                            /* 0xb4 */
    /* indicate hi_reset_flag is valid */
    A_UINT32                hi_reset_flag_valid;                      /* 0xb8 */
    A_UINT32               hi_hci_uart_pwr_mgmt_params_ext;           /* 0xbc */
        /* 0xbc - [31:0]: idle timeout in ms
         */
        /* ACS flags */
    A_UINT32               hi_acs_flags;                              /* 0xc0 */
    A_UINT32               hi_console_flags;                          /* 0xc4 */
    A_UINT32               hi_nvram_state;                            /* 0xc8 */
    A_UINT32               hi_option_flag2;                           /* 0xcc */

    /* If non-zero, override values sent to Host in WMI_READY event. */
    A_UINT32               hi_sw_version_override;                    /* 0xd0 */
    A_UINT32               hi_abi_version_override;                   /* 0xd4 */

    /* Percentage of high priority RX traffic to total expected RX traffic -
     * applicable only to ar6004 */
    A_UINT32               hi_hp_rx_traffic_ratio;                    /* 0xd8 */

    /* test applications flags */
    A_UINT32               hi_test_apps_related    ;                  /* 0xdc */
    /* location of test script */
    A_UINT32               hi_ota_testscript;                         /* 0xe0 */
    /* location of CAL data */
    A_UINT32               hi_cal_data;                               /* 0xe4 */

    /* Number of packet log buffers */
    A_UINT32               hi_pktlog_num_buffers;                     /* 0xe8 */

    /* wow extension configuration */
    A_UINT32               hi_wow_ext_config;                         /* 0xec */
    A_UINT32               hi_pwr_save_flags;                         /* 0xf0 */

    /* Spatial Multiplexing Power Save (SMPS) options */
    A_UINT32               hi_smps_options;                           /* 0xf4 */

    /* Interconnect-specific state */
    A_UINT32               hi_interconnect_state;                     /* 0xf8 */

    /* Coex configuration flags */
    A_UINT32               hi_coex_config;                           /* 0xfc */

    /* Early allocation support */
    A_UINT32               hi_early_alloc;                            /* 0x100 */

    /* FW swap field */
    /* Bits of this 32bit word will be used to pass specific swap
        instruction to FW */
    /* Bit 0 -- AP Nart descriptor no swap. When this bit is set
            FW will not swap TX descriptor. Meaning packets are formed
            on the target processor.*/
    /* Bit 1 -- TBD */

    A_UINT32               hi_fw_swap;                              /* 0x104 */

    /* global arenas pointer address, used by host driver debug */
    A_UINT32               hi_dynamic_mem_arenas_addr;              /* 0x108 */

    /* allocated bytes of DRAM use by allocated */
    A_UINT32               hi_dynamic_mem_allocated;                /* 0x10C */

    /* remaining bytes of DRAM */
    A_UINT32               hi_dynamic_mem_remaining;                /* 0x110 */

    /* memory track count, configured by host */
    A_UINT32               hi_dynamic_mem_track_max;                /* 0x114 */

    /* minidump buffer */
    A_UINT32               hi_minidump;                             /* 0x118 */

    /* bdata's sig and key addr */
    A_UINT32               hi_bd_sig_key;                           /* 0x11c */

} POSTPACK64;

/* bitmap for hi_test_apps_related */
#define HI_TEST_APPS_TESTSCRIPT_LOADED   0x00000001
#define HI_TEST_APPS_CAL_DATA_AVAIL      0x00000002

/* Bits defined in hi_option_flag */
#define HI_OPTION_TIMER_WAR         0x01 /* Enable timer workaround */
#define HI_OPTION_BMI_CRED_LIMIT    0x02 /* Limit BMI command credits */
#define HI_OPTION_RELAY_DOT11_HDR   0x04 /* Relay Dot11 hdr to/from host */
#define HI_OPTION_MAC_ADDR_METHOD   0x08 /* MAC addr method 0-locally administred 1-globally unique addrs */
#define HI_OPTION_FW_BRIDGE         0x10 /* Firmware Bridging */
#define HI_OPTION_ENABLE_PROFILE    0x20 /* Enable CPU profiling */
#define HI_OPTION_DISABLE_DBGLOG    0x40 /* Disable debug logging */
#define HI_OPTION_SKIP_ERA_TRACKING 0x80 /* Skip Era Tracking */
#define HI_OPTION_PAPRD_DISABLE     0x100 /* Disable PAPRD (debug) */
#define HI_OPTION_NUM_DEV_LSB       0x200
#define HI_OPTION_NUM_DEV_MSB       0x800
#define HI_OPTION_DEV_MODE_LSB      0x1000
#define HI_OPTION_DEV_MODE_MSB      0x8000000
#define HI_OPTION_NO_LFT_STBL       0x10000000 /* Disable LowFreq Timer Stabilization */
#define HI_OPTION_SKIP_REG_SCAN     0x20000000 /* Skip regulatory scan */
#define HI_OPTION_INIT_REG_SCAN     0x40000000 /* Do regulatory scan during init before
                                                * sending WMI ready event to host */
#define HI_OPTION_SKIP_MEMMAP       0x80000000 /* REV6: Do not adjust memory map */

#define HI_OPTION_MAC_ADDR_METHOD_SHIFT 3

/* 2 bits of hi_option_flag are used to represent 3 modes */
#define HI_OPTION_FW_MODE_IBSS    0x0 /* IBSS Mode */
#define HI_OPTION_FW_MODE_BSS_STA 0x1 /* STA Mode */
#define HI_OPTION_FW_MODE_AP      0x2 /* AP Mode */
#define HI_OPTION_FW_MODE_BT30AMP 0x3 /* BT30 AMP Mode */

/* 2 bits of hi_option flag are usedto represent 4 submodes */
#define HI_OPTION_FW_SUBMODE_NONE    0x0  /* Normal mode */
#define HI_OPTION_FW_SUBMODE_P2PDEV  0x1  /* p2p device mode */
#define HI_OPTION_FW_SUBMODE_P2PCLIENT 0x2 /* p2p client mode */
#define HI_OPTION_FW_SUBMODE_P2PGO   0x3 /* p2p go mode */

/* Num dev Mask */
#define HI_OPTION_NUM_DEV_MASK    0x7
#define HI_OPTION_NUM_DEV_SHIFT   0x9

/* firmware bridging */
#define HI_OPTION_FW_BRIDGE_SHIFT 0x04

/* Fw Mode/SubMode Mask
|-------------------------------------------------------------------------------|
|   SUB   |   SUB   |   SUB   |  SUB    |         |         |         |         |
| MODE[3] | MODE[2] | MODE[1] | MODE[0] | MODE[3] | MODE[2] | MODE[1] | MODE[0] |
|   (2)   |   (2)   |   (2)   |   (2)   |   (2)   |   (2)   |   (2)   |   (2)   |
|-------------------------------------------------------------------------------|
*/
#define HI_OPTION_FW_MODE_BITS         0x2
#define HI_OPTION_FW_MODE_MASK         0x3
#define HI_OPTION_FW_MODE_SHIFT        0xC
#define HI_OPTION_ALL_FW_MODE_MASK     0xFF

#define HI_OPTION_FW_SUBMODE_BITS      0x2
#define HI_OPTION_FW_SUBMODE_MASK      0x3
#define HI_OPTION_FW_SUBMODE_SHIFT     0x14
#define HI_OPTION_ALL_FW_SUBMODE_MASK  0xFF00
#define HI_OPTION_ALL_FW_SUBMODE_SHIFT 0x8


/* hi_option_flag2 options */
#define HI_OPTION_OFFLOAD_AMSDU     0x01
#define HI_OPTION_DFS_SUPPORT       0x02 /* Enable DFS support */
#define HI_OPTION_ENABLE_RFKILL     0x04 /* RFKill Enable Feature*/
#define HI_OPTION_RADIO_RETENTION_DISABLE     0x08 /* Disable radio retention */
#define HI_OPTION_EARLY_CFG_DONE    0x10 /* Early configuration is complete */

#define HI_OPTION_RF_KILL_SHIFT     0x2
#define HI_OPTION_RF_KILL_MASK      0x1

/* AR9888 1.0 only. Enable/disable CDC max perf support from host */
#define HI_OPTION_DISABLE_CDC_MAX_PERF_WAR  0x20
#define CDC_MAX_PERF_WAR_ENABLED()    \
        (!(HOST_INTEREST->hi_option_flag2 & HI_OPTION_DISABLE_CDC_MAX_PERF_WAR))

#define HI_OPTION_USE_EXT_LDO       0x40 /* use LDO27 for 1.1V instead of PMU */
#define HI_OPTION_DBUART_SUPPORT    0x80 /* Enable uart debug support */
#define HT_OPTION_GPIO_WAKEUP_SUPPORT    0x200 /* GPIO wake up support */

#define GPIO_WAKEUP_ENABLED() \
    (HOST_INTEREST->hi_option_flag2 & HT_OPTION_GPIO_WAKEUP_SUPPORT)

/* hi_reset_flag */
#define HI_RESET_FLAG_PRESERVE_APP_START         0x01  /* preserve App Start address */
#define HI_RESET_FLAG_PRESERVE_HOST_INTEREST     0x02  /* preserve host interest */
#define HI_RESET_FLAG_PRESERVE_ROMDATA           0x04  /* preserve ROM data */
#define HI_RESET_FLAG_PRESERVE_NVRAM_STATE       0x08
#define HI_RESET_FLAG_PRESERVE_BOOT_INFO         0x10
#define HI_RESET_FLAG_WARM_RESET	0x20

/* define hi_fw_swap bits */
#define HI_DESC_IN_FW_BIT       0x01

#define HI_RESET_FLAG_IS_VALID  0x12345678  /* indicate the reset flag is valid */

#define ON_RESET_FLAGS_VALID() \
        (HOST_INTEREST->hi_reset_flag_valid == HI_RESET_FLAG_IS_VALID)

#define RESET_FLAGS_VALIDATE()  \
        (HOST_INTEREST->hi_reset_flag_valid = HI_RESET_FLAG_IS_VALID)

#define RESET_FLAGS_INVALIDATE() \
        (HOST_INTEREST->hi_reset_flag_valid = 0)

#define ON_RESET_PRESERVE_APP_START() \
        (HOST_INTEREST->hi_reset_flag & HI_RESET_FLAG_PRESERVE_APP_START)

#define ON_RESET_PRESERVE_NVRAM_STATE() \
        (HOST_INTEREST->hi_reset_flag & HI_RESET_FLAG_PRESERVE_NVRAM_STATE)

#define ON_RESET_PRESERVE_HOST_INTEREST() \
        (HOST_INTEREST->hi_reset_flag & HI_RESET_FLAG_PRESERVE_HOST_INTEREST)

#define ON_RESET_PRESERVE_ROMDATA() \
        (HOST_INTEREST->hi_reset_flag & HI_RESET_FLAG_PRESERVE_ROMDATA)

#define ON_RESET_PRESERVE_BOOT_INFO() \
        (HOST_INTEREST->hi_reset_flag & HI_RESET_FLAG_PRESERVE_BOOT_INFO)

#define ON_RESET_WARM_RESET() \
        (HOST_INTEREST->hi_reset_flag & HI_RESET_FLAG_WARM_RESET)

/* host CPU endianness */
#define HOST_ON_BE_CPU() \
        (HOST_INTEREST->hi_be)

/* AP nart no swap descriptor flag. Decsriptors are created on the target processor. */
#define DESC_IN_FW() \
        (HOST_INTEREST->hi_fw_swap & HI_DESC_IN_FW_BIT)

#define HI_ACS_FLAGS_SDIO_SWAP_MAILBOX_SET          (1 << 0)
#define HI_ACS_FLAGS_SDIO_REDUCE_TX_COMPL_SET       (1 << 1)
#define HI_ACS_FLAGS_ALT_DATA_CREDIT_SIZE           (1 << 2)

#define HI_ACS_FLAGS_SDIO_SWAP_MAILBOX_FW_ACK       (1 << 16)
#define HI_ACS_FLAGS_SDIO_REDUCE_TX_COMPL_FW_ACK    (1 << 17)

/* CONSOLE FLAGS
 *
 * Bit Range  Meaning
 * ---------  --------------------------------
 *   2..0     UART ID (0 = Default)
 *    3       Baud Select (0 = 9600, 1 = 115200)
 *   30..4    Reserved
 *    31      Enable Console
 *
 * */

#define HI_CONSOLE_FLAGS_ENABLE       (1 << 31)
#define HI_CONSOLE_FLAGS_UART_MASK    (0x7)
#define HI_CONSOLE_FLAGS_UART_SHIFT   0
#define HI_CONSOLE_FLAGS_BAUD_SELECT  (1 << 3)

/* SM power save options */
#define HI_SMPS_ALLOW_MASK            (0x00000001)
#define HI_SMPS_MODE_MASK             (0x00000002)
#define HI_SMPS_MODE_STATIC           (0x00000000)
#define HI_SMPS_MODE_DYNAMIC          (0x00000002)
#define HI_SMPS_DISABLE_AUTO_MODE     (0x00000004)
#define HI_SMPS_DATA_THRESH_MASK      (0x000007f8)
#define HI_SMPS_DATA_THRESH_SHIFT     (3)
#define HI_SMPS_RSSI_THRESH_MASK      (0x0007f800)
#define HI_SMPS_RSSI_THRESH_SHIFT     (11)
#define HI_SMPS_LOWPWR_CM_MASK        (0x00380000)
#define HI_SMPS_LOWPWR_CM_SHIFT       (15)
#define HI_SMPS_HIPWR_CM_MASK         (0x03c00000)
#define HI_SMPS_HIPWR_CM_SHIFT        (19)

#define HOST_INTEREST_SMPS_GET_MODE()     (HOST_INTEREST->hi_smps_options & HI_SMPS_MODE_MASK)
#define HOST_INTEREST_SMPS_GET_DATA_THRESH() ((HOST_INTEREST->hi_smps_options & HI_SMPS_DATA_THRESH_MASK) >> HI_SMPS_DATA_THRESH_SHIFT)
#define HOST_INTEREST_SMPS_SET_DATA_THRESH(x) (((x) << HI_SMPS_DATA_THRESH_SHIFT) & HI_SMPS_DATA_THRESH_MASK)
#define HOST_INTEREST_SMPS_GET_RSSI_THRESH() ((HOST_INTEREST->hi_smps_options & HI_SMPS_RSSI_THRESH_MASK) >> HI_SMPS_RSSI_THRESH_SHIFT)
#define HOST_INTEREST_SMPS_SET_RSSI_THRESH(x) (((x) << HI_SMPS_RSSI_THRESH_SHIFT) & HI_SMPS_RSSI_THRESH_MASK)
#define HOST_INTEREST_SMPS_SET_LOWPWR_CM()   ((HOST_INTEREST->hi_smps_options & HI_SMPS_LOWPWR_CM_MASK) >> HI_SMPS_LOWPWR_CM_SHIFT)
#define HOST_INTEREST_SMPS_SET_HIPWR_CM() ((HOST_INTEREST->hi_smps_options << HI_SMPS_HIPWR_CM_MASK) & HI_SMPS_HIPWR_CM_SHIFT)
#define HOST_INTEREST_SMPS_IS_AUTO_MODE_DISABLED() (HOST_INTEREST->hi_smps_options & HI_SMPS_DISABLE_AUTO_MODE)


/* WOW Extension configuration
 *
 * Bit Range  Meaning
 * ---------  --------------------------------
 *   8..0     Size of each WOW pattern (max 511)
 *   15..9    Number of patterns per list (max 127)
 *   17..16   Number of lists (max 4)
 *   30..18   Reserved
 *   31       Enabled
 *
 *  set values (except enable) to zeros for default settings
 *
 * */

#define HI_WOW_EXT_ENABLED_MASK        (1 << 31)
#define HI_WOW_EXT_NUM_LIST_SHIFT      16
#define HI_WOW_EXT_NUM_LIST_MASK       (0x3 << HI_WOW_EXT_NUM_LIST_SHIFT)
#define HI_WOW_EXT_NUM_PATTERNS_SHIFT  9
#define HI_WOW_EXT_NUM_PATTERNS_MASK   (0x7F << HI_WOW_EXT_NUM_PATTERNS_SHIFT)
#define HI_WOW_EXT_PATTERN_SIZE_SHIFT  0
#define HI_WOW_EXT_PATTERN_SIZE_MASK   (0x1FF << HI_WOW_EXT_PATTERN_SIZE_SHIFT)

#define HI_WOW_EXT_MAKE_CONFIG(num_lists,count,size) \
    ((((num_lists) << HI_WOW_EXT_NUM_LIST_SHIFT) & HI_WOW_EXT_NUM_LIST_MASK)     | \
    (((count) << HI_WOW_EXT_NUM_PATTERNS_SHIFT) & HI_WOW_EXT_NUM_PATTERNS_MASK)  | \
    (((size) << HI_WOW_EXT_PATTERN_SIZE_SHIFT) & HI_WOW_EXT_PATTERN_SIZE_MASK))

#define HI_WOW_EXT_GET_NUM_LISTS(config)     \
                        (((config) & HI_WOW_EXT_NUM_LIST_MASK) >> HI_WOW_EXT_NUM_LIST_SHIFT)
#define HI_WOW_EXT_GET_NUM_PATTERNS(config)  \
                        (((config) & HI_WOW_EXT_NUM_PATTERNS_MASK) >> HI_WOW_EXT_NUM_PATTERNS_SHIFT)
#define HI_WOW_EXT_GET_PATTERN_SIZE(config)  \
                        (((config) & HI_WOW_EXT_PATTERN_SIZE_MASK) >> HI_WOW_EXT_PATTERN_SIZE_SHIFT)

/*
 * Early allocation configuration
 * Support RAM bank configuration before BMI done and this eases the memory
 * allocation at very early stage
 * Bit Range  Meaning
 * ---------  ----------------------------------
 * [0:3]      number of bank assigned to be IRAM
 * [4:15]     reserved
 * [16:31]    magic number
 *
 * Note:
 * 1. target firmware would check magic number and if it's a match, firmware
 *    would consider the bits[0:15] are valid and base on that to calculate
 *    the end of DRAM. Early allocation would be located at that area and
 *    may be reclaimed when necesary
 * 2. if no magic number is found, early allocation would happen at "_end"
 *    symbol of ROM which is located before the app-data and might NOT be
 *    re-claimable. If this is adopted, link script should keep this in
 *    mind to avoid data corruption.
 */
#define HI_EARLY_ALLOC_MAGIC                   0x6d8a
#define HI_EARLY_ALLOC_MAGIC_MASK              0xffff0000
#define HI_EARLY_ALLOC_MAGIC_SHIFT             16
#define HI_EARLY_ALLOC_IRAM_BANKS_MASK         0x0000000f
#define HI_EARLY_ALLOC_IRAM_BANKS_SHIFT        0

#define HI_EARLY_ALLOC_VALID() \
               ((((HOST_INTEREST->hi_early_alloc) & HI_EARLY_ALLOC_MAGIC_MASK) >> HI_EARLY_ALLOC_MAGIC_SHIFT) \
                               == (HI_EARLY_ALLOC_MAGIC))
#define HI_EARLY_ALLOC_GET_IRAM_BANKS() \
               (((HOST_INTEREST->hi_early_alloc) & HI_EARLY_ALLOC_IRAM_BANKS_MASK) >> HI_EARLY_ALLOC_IRAM_BANKS_SHIFT)

/*
 * Intended for use by Host software, this macro returns the Target RAM
 * address of any item in the host_interest structure.
 * Example: target_addr = AR6002_HOST_INTEREST_ITEM_ADDRESS(hi_board_data);
 */
#define AR6002_HOST_INTEREST_ITEM_ADDRESS(item) \
    (A_UINT32)((size_t)&((((struct host_interest_s *)(AR6002_HOST_INTEREST_ADDRESS))->item)))

#define AR6003_HOST_INTEREST_ITEM_ADDRESS(item) \
    (A_UINT32)((size_t)&((((struct host_interest_s *)(AR6003_HOST_INTEREST_ADDRESS))->item)))

#define AR6004_HOST_INTEREST_ITEM_ADDRESS(item) \
    (A_UINT32)((size_t)&((((struct host_interest_s *)(AR6004_HOST_INTEREST_ADDRESS))->item)))

#define AR6006_HOST_INTEREST_ITEM_ADDRESS(item) \
    (A_UINT32)((size_t)&((((struct host_interest_s *)(AR6006_HOST_INTEREST_ADDRESS))->item)))

#define AR9888_HOST_INTEREST_ITEM_ADDRESS(item) \
    (A_UINT32)((size_t)&((((struct host_interest_s *)(AR9888_HOST_INTEREST_ADDRESS))->item)))

#define AR6320_HOST_INTEREST_ITEM_ADDRESS(item) \
    (A_UINT32)((size_t)&((((struct host_interest_s *)(AR6320_HOST_INTEREST_ADDRESS))->item)))

#define AR900B_HOST_INTEREST_ITEM_ADDRESS(item) \
    (A_UINT32)((size_t)&((((struct host_interest_s *)(AR900B_HOST_INTEREST_ADDRESS))->item)))

#define HOST_INTEREST_DBGLOG_IS_ENABLED() \
        (!((volatile A_UINT32)HOST_INTEREST->hi_option_flag & HI_OPTION_DISABLE_DBGLOG))

#define HOST_INTEREST_PKTLOG_IS_ENABLED() \
        (((volatile A_UINT32)HOST_INTEREST->hi_pktlog_num_buffers))

#define HOST_INTEREST_PROFILE_IS_ENABLED() \
        ((volatile A_UINT32)HOST_INTEREST->hi_option_flag & HI_OPTION_ENABLE_PROFILE)

#define LF_TIMER_STABILIZATION_IS_ENABLED() \
        (!((volatile A_UINT32)HOST_INTEREST->hi_option_flag & HI_OPTION_NO_LFT_STBL))

#define IS_AMSDU_OFFLAOD_ENABLED() \
        (((volatile A_UINT32)HOST_INTEREST->hi_option_flag2 & HI_OPTION_OFFLOAD_AMSDU))

#define HOST_INTEREST_DFS_IS_ENABLED() \
        (((volatile A_UINT32)HOST_INTEREST->hi_option_flag2 & HI_OPTION_DFS_SUPPORT))

#define HOST_INTEREST_EARLY_CFG_DONE() \
        (((volatile A_UINT32)HOST_INTEREST->hi_option_flag2 & HI_OPTION_EARLY_CFG_DONE))

/*power save flag bit definitions*/
#define HI_PWR_SAVE_LPL_ENABLED   0x1
/*b1-b3 reserved*/
/*b4-b5 : dev0 LPL type : 0 - none
                          1- Reduce Pwr Search
                          2- Reduce Pwr Listen*/
/*b6-b7 : dev1 LPL type and so on for Max 8 devices*/
#define HI_PWR_SAVE_LPL_DEV0_LSB   4
#define HI_PWR_SAVE_LPL_DEV_MASK   0x3
/*power save related utility macros*/
#define HI_LPL_ENABLED() \
        ((HOST_INTEREST->hi_pwr_save_flags & HI_PWR_SAVE_LPL_ENABLED))
#define HI_DEV_LPL_TYPE_GET(_devix)   \
        (HOST_INTEREST->hi_pwr_save_flags & \
          ((HI_PWR_SAVE_LPL_DEV_MASK) << \
           (HI_PWR_SAVE_LPL_DEV0_LSB + \
            (_devix)*2)))

#define HOST_INTEREST_SMPS_IS_ALLOWED() \
        ((HOST_INTEREST->hi_smps_options & HI_SMPS_ALLOW_MASK))

/* Convert a Target virtual address into a Target physical address */
#define AR6002_VTOP(vaddr) ((vaddr) & 0x001fffff)
#define AR6003_VTOP(vaddr) ((vaddr) & 0x001fffff)
#define AR6004_VTOP(vaddr) (vaddr)
#define AR6006_VTOP(vaddr) (vaddr)
#define AR9888_VTOP(vaddr) (vaddr)
#define AR6320_VTOP(vaddr) (vaddr)
#define AR900B_VTOP(vaddr) (vaddr)
#define TARG_VTOP(TargetType, vaddr) \
        (((TargetType) == TARGET_TYPE_AR6002) ? AR6002_VTOP(vaddr) : \
        (((TargetType) == TARGET_TYPE_AR6003) ? AR6003_VTOP(vaddr) : \
        (((TargetType) == TARGET_TYPE_AR6004) ? AR6004_VTOP(vaddr) : \
        (((TargetType) == TARGET_TYPE_AR6006) ? AR6006_VTOP(vaddr) : \
        (((TargetType) == TARGET_TYPE_AR9888) ? AR9888_VTOP(vaddr) : \
        (((TargetType) == TARGET_TYPE_AR6320) ? AR6320_VTOP(vaddr) : \
        (((TargetType) == TARGET_TYPE_AR900B) ? AR900B_VTOP(vaddr) : \
           0)))))))

#define HOST_INTEREST_ITEM_ADDRESS(TargetType, item) \
        (((TargetType) == TARGET_TYPE_AR6002) ? AR6002_HOST_INTEREST_ITEM_ADDRESS(item) : \
        (((TargetType) == TARGET_TYPE_AR6003) ? AR6003_HOST_INTEREST_ITEM_ADDRESS(item) : \
        (((TargetType) == TARGET_TYPE_AR6004) ? AR6004_HOST_INTEREST_ITEM_ADDRESS(item) : \
        (((TargetType) == TARGET_TYPE_AR6006) ? AR6006_HOST_INTEREST_ITEM_ADDRESS(item) : \
        (((TargetType) == TARGET_TYPE_AR9888) ? AR9888_HOST_INTEREST_ITEM_ADDRESS(item) : \
        (((TargetType) == TARGET_TYPE_AR6320) ? AR6320_HOST_INTEREST_ITEM_ADDRESS(item) : \
        (((TargetType) == TARGET_TYPE_AR6320V2) ? AR6320_HOST_INTEREST_ITEM_ADDRESS(item) : \
        (((TargetType) == TARGET_TYPE_AR900B) ? AR900B_HOST_INTEREST_ITEM_ADDRESS(item) : \
           0))))))))

#define AR6002_BOARD_DATA_SZ 768
#define AR6002_BOARD_EXT_DATA_SZ 0
#define AR6003_BOARD_DATA_SZ 1024
/* Reserve 1024 bytes for extended board data */
#if defined(AR6002_REV43)
#define AR6003_BOARD_EXT_DATA_SZ 1024
#else
#define AR6003_BOARD_EXT_DATA_SZ 768
#endif
#define AR6004_BOARD_DATA_SZ     7168
#define AR6004_BOARD_EXT_DATA_SZ 0
#define AR9888_BOARD_DATA_SZ     7168
#define AR9888_BOARD_EXT_DATA_SZ 0
#define AR6320_BOARD_DATA_SZ     8192
#define AR6320_BOARD_EXT_DATA_SZ 0
#define AR900B_BOARD_DATA_SZ     7168
#define AR900B_BOARD_EXT_DATA_SZ 0

#define AR6003_REV3_APP_START_OVERRIDE          0x946100
#define AR6003_REV3_APP_LOAD_ADDRESS            0x545000
#define AR6003_REV3_BOARD_EXT_DATA_ADDRESS      0x542330
#define AR6003_REV3_DATASET_PATCH_ADDRESS       0x57FF74
#define AR6003_REV3_RAM_RESERVE_SIZE            4096

#define AR6004_REV1_BOARD_DATA_ADDRESS          0x423900
#define AR6004_REV1_RAM_RESERVE_SIZE            19456
#define AR6004_REV1_DATASET_PATCH_ADDRESS       0x425294

#define AR6004_REV2_BOARD_DATA_ADDRESS          0x426400
#define AR6004_REV2_RAM_RESERVE_SIZE            7168
#define AR6004_REV2_DATASET_PATCH_ADDRESS       0x435294

#define AR6004_REV5_BOARD_DATA_ADDRESS          0x436400
#define AR6004_REV5_RAM_RESERVE_SIZE            7168
#define AR6004_REV5_DATASET_PATCH_ADDRESS       0x437860

/* Reserve 4K for OTA test script */
#define AR6004_REV1_RAM_RESERVE_SIZE_FOR_TEST_SCRIPT    4096
#define AR6004_REV1_TEST_SCRIPT_ADDRESS       0x422900

/* # of A_UINT32 entries in targregs, used by DIAG_FETCH_TARG_REGS */
#define AR6003_FETCH_TARG_REGS_COUNT 64
#define AR6004_FETCH_TARG_REGS_COUNT 64
#define AR9888_FETCH_TARG_REGS_COUNT 64
#define AR6320_FETCH_TARG_REGS_COUNT 64
#define AR900B_FETCH_TARG_REGS_COUNT 64

#endif /* !__ASSEMBLER__ */

#ifndef ATH_TARGET
#include "athendpack.h"
#endif

#endif /* __TARGADDRS_H__ */
