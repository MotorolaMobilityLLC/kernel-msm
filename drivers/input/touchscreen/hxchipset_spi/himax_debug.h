/*  Himax Android Driver Sample Code for debug nodes

    Copyright (C) 2018 Himax Corporation.

    This software is licensed under the terms of the GNU General Public
    License version 2, as published by the Free Software Foundation, and
    may be copied, distributed, and modified under those terms.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

*/
#ifndef H_HIMAX_DEBUG
#define H_HIMAX_DEBUG

#include "himax_platform.h"
#include "himax_common.h"


#ifdef HX_ESD_RECOVERY
    extern u8 HX_ESD_RESET_ACTIVATE;
    extern     int hx_EB_event_flag;
    extern     int hx_EC_event_flag;
    extern     int hx_ED_event_flag;
#endif

#define HIMAX_PROC_DEBUG_LEVEL_FILE     "debug_level"
#define HIMAX_PROC_VENDOR_FILE          "vendor"
#define HIMAX_PROC_ATTN_FILE            "attn"
#define HIMAX_PROC_INT_EN_FILE          "int_en"
#define HIMAX_PROC_LAYOUT_FILE          "layout"
#define HIMAX_PROC_CRC_TEST_FILE        "CRC_test"

static struct proc_dir_entry *himax_proc_debug_level_file;
static struct proc_dir_entry *himax_proc_vendor_file;
static struct proc_dir_entry *himax_proc_attn_file;
static struct proc_dir_entry *himax_proc_int_en_file;
static struct proc_dir_entry *himax_proc_layout_file;
static struct proc_dir_entry *himax_proc_CRC_test_file;

int himax_touch_proc_init(void);
void himax_touch_proc_deinit(void);
extern int himax_int_en_set(void);

#define HIMAX_PROC_REGISTER_FILE    "register"
struct  proc_dir_entry *himax_proc_register_file = NULL;
uint8_t byte_length = 0;
uint8_t register_command[4];
uint8_t cfg_flag = 0;

#define HIMAX_PROC_DIAG_FILE    "diag"
struct proc_dir_entry *himax_proc_diag_file = NULL;
#define HIMAX_PROC_DIAG_ARR_FILE    "diag_arr"
struct  proc_dir_entry *himax_proc_diag_arrange_file = NULL;
struct  file *diag_sram_fn;
uint8_t write_counter = 0;
uint8_t write_max_count = 30;
#define IIR_DUMP_FILE  "/sdcard/HX_IIR_Dump.txt"
#define DC_DUMP_FILE   "/sdcard/HX_DC_Dump.txt"
#define BANK_DUMP_FILE "/sdcard/HX_BANK_Dump.txt"
#ifdef HX_TP_PROC_2T2R
    static uint32_t *diag_mutual_2;
    int32_t *getMutualBuffer_2(void);
    void     setMutualBuffer_2(uint8_t x_num, uint8_t y_num);
#endif
int32_t *diag_mutual       = NULL;
int32_t *diag_mutual_new   = NULL;
int32_t *diag_mutual_old   = NULL;
uint8_t diag_max_cnt       = 0;
uint8_t hx_state_info[2]   = {0};
uint8_t diag_coor[128];
int32_t diag_self[100]     = {0};
int32_t diag_self_new[100] = {0};
int32_t diag_self_old[100] = {0};
int32_t *getMutualBuffer(void);
int32_t *getMutualNewBuffer(void);
int32_t *getMutualOldBuffer(void);
int32_t *getSelfBuffer(void);
int32_t *getSelfNewBuffer(void);
int32_t *getSelfOldBuffer(void);
void     setMutualBuffer(uint8_t x_num, uint8_t y_num);
void     setMutualNewBuffer(uint8_t x_num, uint8_t y_num);
void     setMutualOldBuffer(uint8_t x_num, uint8_t y_num);

#define HIMAX_PROC_DEBUG_FILE    "debug"
struct proc_dir_entry *himax_proc_debug_file = NULL;
#define HIMAX_PROC_FW_DEBUG_FILE    "FW_debug"
struct proc_dir_entry *himax_proc_fw_debug_file = NULL;
#define HIMAX_PROC_DD_DEBUG_FILE    "DD_debug"
struct proc_dir_entry *himax_proc_dd_debug_file = NULL;
bool    fw_update_complete = false;
int handshaking_result = 0;
unsigned char debug_level_cmd = 0;
uint8_t cmd_set[8];
uint8_t mutual_set_flag = 0;

#define HIMAX_PROC_FLASH_DUMP_FILE    "flash_dump"
struct proc_dir_entry *himax_proc_flash_dump_file = NULL;
static int Flash_Size = 131072;
static uint8_t *flash_buffer;
static uint8_t g_flash_cmd;
static uint8_t g_flash_progress;
static bool g_flash_dump_rst; /*Fail = 0, Pass = 1*/
void setFlashBuffer(void);

enum flash_dump_prog{
    START,
    ONGOING,
    FINISHED,
};

uint32_t **raw_data_array;
uint8_t X_NUM = 0, Y_NUM = 0;
uint8_t sel_type = 0x0D;

#define HIMAX_PROC_RESET_FILE        "reset"
struct proc_dir_entry *himax_proc_reset_file         = NULL;

#define HIMAX_PROC_SENSE_ON_OFF_FILE "SenseOnOff"
struct proc_dir_entry *himax_proc_SENSE_ON_OFF_file = NULL;

#ifdef HX_ESD_RECOVERY
    #define HIMAX_PROC_ESD_CNT_FILE "ESD_cnt"
    struct proc_dir_entry *himax_proc_ESD_cnt_file = NULL;
#endif

#ifdef HX_TP_PROC_GUEST_INFO
#define HIMAX_PROC_GUEST_INFO_FILE        "guest_info"
static struct proc_dir_entry *himax_proc_guest_info_file     = NULL;
#endif

#endif
