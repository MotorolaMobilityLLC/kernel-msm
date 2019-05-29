/*  Himax Android Driver Sample Code for HX83112 chipset

    Copyright (C) 2018 Himax Corporation.

    This software is licensed under the terms of the GNU General Public
    License version 2,  as published by the Free Software Foundation,  and
    may be copied,  distributed,  and modified under those terms.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

*/

#include "himax_ic_HX83112.h"
extern struct himax_ts_data *private_ts;
extern struct himax_core_fp g_core_fp;
extern struct ic_operation *pic_op;
extern struct fw_operation *pfw_op;
#ifdef HX_ZERO_FLASH
extern struct zf_operation *pzf_op;
#endif
#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
#if defined(HX_EN_DYNAMIC_NAME)
    extern char *i_CTPM_firmware_name;
    extern struct himax_ic_data *ic_data;
#endif
#endif
extern struct himax_chip_detect *g_core_chip_dt;

extern uint8_t g_hx_ic_dt_num;
extern int i2c_error_count;
extern unsigned char IC_TYPE;
extern unsigned char IC_CHECKSUM;
extern bool DSRAM_Flag;

extern unsigned long     FW_VER_MAJ_FLASH_ADDR;
extern unsigned long     FW_VER_MIN_FLASH_ADDR;
extern unsigned long     CFG_VER_MAJ_FLASH_ADDR;
extern unsigned long     CFG_VER_MIN_FLASH_ADDR;
extern unsigned long     CID_VER_MAJ_FLASH_ADDR;
extern unsigned long     CID_VER_MIN_FLASH_ADDR;

extern unsigned long     FW_VER_MAJ_FLASH_LENG;
extern unsigned long     FW_VER_MIN_FLASH_LENG;
extern unsigned long     CFG_VER_MAJ_FLASH_LENG;
extern unsigned long     CFG_VER_MIN_FLASH_LENG;
extern unsigned long     CID_VER_MAJ_FLASH_LENG;
extern unsigned long     CID_VER_MIN_FLASH_LENG;

#ifdef HX_AUTO_UPDATE_FW
extern int g_i_FW_VER;
extern int g_i_CFG_VER;
extern int g_i_CID_MAJ;
extern int g_i_CID_MIN;
extern unsigned char *i_CTPM_FW;
#endif

#ifdef HX_TP_PROC_2T2R
    extern bool Is_2T2R;
#endif

#ifdef HX_USB_DETECT_GLOBAL
extern void himax_cable_detect_func (bool force_renew);
#endif


static void hx83112_chip_init (void)
{

    private_ts->chip_cell_type = CHIP_IS_IN_CELL;
    I ("%s:IC cell type = %d\n",  __func__,  private_ts->chip_cell_type);
    IC_CHECKSUM             = HX_TP_BIN_CHECKSUM_CRC;
    /*Himax: Set FW and CFG Flash Address*/
    FW_VER_MAJ_FLASH_ADDR   = 49157;  /*0x00C005*/
    FW_VER_MAJ_FLASH_LENG   = 1;
    FW_VER_MIN_FLASH_ADDR   = 49158;  /*0x00C006*/
    FW_VER_MIN_FLASH_LENG   = 1;
    CFG_VER_MAJ_FLASH_ADDR  = 49408;  /*0x00C100*/
    CFG_VER_MAJ_FLASH_LENG  = 1;
    CFG_VER_MIN_FLASH_ADDR  = 49409;  /*0x00C101*/
    CFG_VER_MIN_FLASH_LENG  = 1;
    CID_VER_MAJ_FLASH_ADDR  = 49154;  /*0x00C002*/
    CID_VER_MAJ_FLASH_LENG  = 1;
    CID_VER_MIN_FLASH_ADDR  = 49155;  /*0x00C003*/
    CID_VER_MIN_FLASH_LENG  = 1;
}

static bool hx83112_sense_off(bool check_en)
{
    uint8_t cnt = 0;
    uint8_t tmp_data[DATA_LEN_4];

    do {
        if (cnt == 0 || (tmp_data[0] != 0xA5 && tmp_data[0] != 0x00 && tmp_data[0] != 0x87)) {
         g_core_fp.fp_register_write(pfw_op->addr_ctrl_fw_isr, DATA_LEN_4, pfw_op->data_fw_stop, 0);
        }
        msleep(20);

        /* check fw status */
        g_core_fp.fp_register_read(pic_op->addr_cs_central_state, ADDR_LEN_4, tmp_data, 0);

        if (tmp_data[0] != 0x05) {
         I("%s: Do not need wait FW, Status = 0x%02X!\n", __func__, tmp_data[0]);
         break;
        }

        g_core_fp.fp_register_read(pfw_op->addr_ctrl_fw_isr, 4, tmp_data, false);
        I("%s: cnt = %d, data[0] = 0x%02X!\n", __func__, cnt, tmp_data[0]);
    } while (tmp_data[0] != 0x87 && (++cnt < 50) && check_en == true);

    cnt = 0;

    do {
        /*===========================================
         I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
        ===========================================*/
        tmp_data[0] = pic_op->data_i2c_psw_lb[0];

        if (himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
            E("%s: i2c access fail!\n", __func__);
            return false;
        }

        /*===========================================
         I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
        ===========================================*/
        tmp_data[0] = pic_op->data_i2c_psw_ub[0];

        if (himax_bus_write(pic_op->adr_i2c_psw_ub[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
            E("%s: i2c access fail!\n", __func__);
            return false;
        }

        /*===========================================
         I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x00
        ===========================================*/
        tmp_data[0] = 0x00;

        if (himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
            E("%s: i2c access fail!\n", __func__);
            return false;
        }

        /*===========================================
         I2C_password[7:0] set Enter safe mode : 0x31 ==> 0x27
        ===========================================*/
        tmp_data[0] = pic_op->data_i2c_psw_lb[0];

        if (himax_bus_write(pic_op->adr_i2c_psw_lb[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
            E("%s: i2c access fail!\n", __func__);
            return false;
        }

        /*===========================================
         I2C_password[15:8] set Enter safe mode :0x32 ==> 0x95
        ===========================================*/
        tmp_data[0] = pic_op->data_i2c_psw_ub[0];

        if (himax_bus_write(pic_op->adr_i2c_psw_ub[0], tmp_data, 1, HIMAX_I2C_RETRY_TIMES) < 0) {
            E("%s: i2c access fail!\n", __func__);
            return false;
        }

        /* ======================
         Check enter_save_mode
         ======================*/
        g_core_fp.fp_register_read(pic_op->addr_cs_central_state, ADDR_LEN_4, tmp_data, 0);
        I("%s: Check enter_save_mode data[0]=%X \n", __func__, tmp_data[0]);

        if (tmp_data[0] == 0x0C) {
            /*=====================================
             Reset TCON
            =====================================*/
            g_core_fp.fp_register_write(pic_op->addr_tcon_on_rst, DATA_LEN_4, pic_op->data_rst, 0);
            msleep(1);
            tmp_data[3] = pic_op->data_rst[3];
            tmp_data[2] = pic_op->data_rst[2];
            tmp_data[1] = pic_op->data_rst[1];
            tmp_data[0] = pic_op->data_rst[0] | 0x01;
            g_core_fp.fp_register_write(pic_op->addr_tcon_on_rst, DATA_LEN_4, tmp_data, 0);
            /*=====================================
             Reset ADC
            =====================================*/
            g_core_fp.fp_register_write(pic_op->addr_adc_on_rst, DATA_LEN_4, pic_op->data_rst, 0);
            msleep(1);
            tmp_data[3] = pic_op->data_rst[3];
            tmp_data[2] = pic_op->data_rst[2];
            tmp_data[1] = pic_op->data_rst[1];
            tmp_data[0] = pic_op->data_rst[0] | 0x01;
            g_core_fp.fp_register_write(pic_op->addr_adc_on_rst, DATA_LEN_4, tmp_data, 0);
            return true;
        } else {
            msleep(10);
#ifdef HX_RST_PIN_FUNC
            g_core_fp.fp_ic_reset(false, false);
#endif
        }
    } while (cnt++ < 15);

    return false;
}

#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
#if defined(HX_EN_DYNAMIC_NAME)
static void hx83112a_read_ic_ver(void)
{
    uint8_t tmp_data[DATA_LEN_4];
    uint8_t tmp_addr[DATA_LEN_4];

    /* Enable read DD */
    himax_in_parse_assign_cmd(hx83112_ic_osc_en, tmp_addr, sizeof(tmp_addr));
    g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
    tmp_data[0] = 0x01;
    g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

    himax_in_parse_assign_cmd(hx83112_ic_osc_pw, tmp_addr, sizeof(tmp_addr));
    g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
    tmp_data[0] = 0xA5;
    g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

    tmp_data[0] = 0x00;
    tmp_data[1] = 0x83;
    tmp_data[2] = 0x11;
    tmp_data[3] = 0x2A;
    himax_in_parse_assign_cmd(hx83112_ic_b9_en, tmp_addr, sizeof(tmp_addr));
    g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

    tmp_data[0] = 0x00;
    tmp_data[1] = 0x55;
    tmp_data[2] = 0xAA;
    tmp_data[3] = 0x00;
    himax_in_parse_assign_cmd(hx83112_ic_eb_en, tmp_addr, sizeof(tmp_addr));
    g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

    /* Read DD data */
    himax_in_parse_assign_cmd(hx83112_cb_ic_fw, tmp_addr, sizeof(tmp_addr));
    g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
    I("CB=%X\n", tmp_data[0]);
    ic_data->vendor_old_ic_ver = tmp_data[0];

    himax_in_parse_assign_cmd(hx83112_e8_ic_fw, tmp_addr, sizeof(tmp_addr));
    g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
    I("E8=%X\n", tmp_data[0]);
    ic_data->vendor_ic_ver = tmp_data[0];

    /* Disable read DD */
    himax_in_parse_assign_cmd(hx83112_ic_osc_en, tmp_addr, sizeof(tmp_addr));
    g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
    tmp_data[0] = 0x00;
    g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);
}

static void hx83112b_read_ic_ver(void)
{
    uint8_t tmp_data[DATA_LEN_4];
    uint8_t tmp_addr[DATA_LEN_4];

    /* Enable read DD */
    himax_in_parse_assign_cmd(hx83112_ic_osc_pw, tmp_addr, sizeof(tmp_addr));
    g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
    tmp_data[0] = 0xA5;
    g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

    tmp_data[0] = 0x00;
    tmp_data[1] = 0x83;
    tmp_data[2] = 0x11;
    tmp_data[3] = 0x2B;
    himax_in_parse_assign_cmd(hx83112_ic_b9_en, tmp_addr, sizeof(tmp_addr));
    g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

    /* Read DD data */
    himax_in_parse_assign_cmd(hx83112_cb_ic_fw, tmp_addr, sizeof(tmp_addr));
    g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
    I("CB=%X\n", tmp_data[0]);
    ic_data->vendor_old_ic_ver = tmp_data[0];

    himax_in_parse_assign_cmd(hx83112_e8_ic_fw, tmp_addr, sizeof(tmp_addr));
    g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
    I("E8=%X\n", tmp_data[0]);
    ic_data->vendor_ic_ver = tmp_data[0];
}

static void hx83112e_read_ic_ver(void)
{
    uint8_t tmp_data[DATA_LEN_4];
    uint8_t tmp_addr[DATA_LEN_4];

    /* Enable read DD */
    himax_in_parse_assign_cmd(hx83112_ic_osc_pw, tmp_addr, sizeof(tmp_addr));
    g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
    tmp_data[0] = 0xA5;
    g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

    tmp_data[0] = 0x00;
    tmp_data[1] = 0x83;
    tmp_data[2] = 0x11;
    tmp_data[3] = 0x2E;
    himax_in_parse_assign_cmd(hx83112_ic_b9_en, tmp_addr, sizeof(tmp_addr));
    g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, tmp_data, false);

    /* Read DD data */
    himax_in_parse_assign_cmd(hx83112_cb_ic_fw, tmp_addr, sizeof(tmp_addr));
    g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
    I("CB=%X\n", tmp_data[0]);
    ic_data->vendor_old_ic_ver = tmp_data[0];

    himax_in_parse_assign_cmd(hx83112_e8_ic_fw, tmp_addr, sizeof(tmp_addr));
    g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
    I("E8=%X\n", tmp_data[0]);
    ic_data->vendor_ic_ver = tmp_data[0];
}

static void hx83112_dynamic_fw_name(uint8_t ic_name)
{
    char firmware_name[64];

    if (ic_name == 0x2a) {
        hx83112a_read_ic_ver();
    } else if (ic_name == 0x2b) {
        hx83112b_read_ic_ver();
    } else {
        hx83112e_read_ic_ver();
    }

    if (i_CTPM_firmware_name != NULL) {
        kfree(i_CTPM_firmware_name);
        i_CTPM_firmware_name = NULL;
    }
    memset(firmware_name, 0x00, sizeof(firmware_name));

    if ((ic_data->vendor_ic_ver == 0x13) || (ic_data->vendor_ic_ver == 0x14) || (ic_data->vendor_ic_ver == 0x15)
            || (ic_data->vendor_ic_ver == 0x16) || (ic_data->vendor_ic_ver == 0x23)) {
        ic_data->vendor_semifac = 2;
    } else if ((ic_data->vendor_old_ic_ver == 0x44) || (ic_data->vendor_old_ic_ver == 0x77)
            || (ic_data->vendor_ic_ver == 0x03) || (ic_data->vendor_ic_ver == 0x04)) {
        ic_data->vendor_semifac = 1;
    } else {
        ic_data->vendor_semifac = 0;
    }
    memcpy(firmware_name, "Himax_firmware.bin", sizeof(char)*strlen("Himax_firmware.bin"));

    i_CTPM_firmware_name = kzalloc((sizeof(char)*(strlen(firmware_name)+1)), GFP_KERNEL);
    if (i_CTPM_firmware_name != NULL) {
        memcpy(i_CTPM_firmware_name, firmware_name, (sizeof(char)*(strlen(firmware_name)+1)));
    }
    I("i_CTPM_firmware_name : %s \n", i_CTPM_firmware_name);
}
#endif
#endif

static void hx83112_func_re_init (void)
{
    g_core_fp.fp_sense_off = hx83112_sense_off;
    g_core_fp.fp_chip_init = hx83112_chip_init;
}

static void hx83112_reg_re_init (void)
{
}

static bool hx83112_chip_detect (void)
{
    uint8_t tmp_data[DATA_LEN_4];
    bool ret_data = false;
    int i = 0;
    himax_mcu_in_cmd_struct_init();
    himax_mcu_in_cmd_init();

    hx83112_reg_re_init();
    hx83112_func_re_init();

    g_core_fp.fp_sense_off(false);

    for (i = 0; i < 5; i++) {
        g_core_fp.fp_register_read (pfw_op->addr_icid_addr,  DATA_LEN_4,  tmp_data,  false);
        I("%s:Read driver IC ID = %X, %X, %X\n", __func__, tmp_data[3], tmp_data[2], tmp_data[1]);

        if ((tmp_data[3] == 0x83) && (tmp_data[2] == 0x11) && ((tmp_data[1] == 0x2a) || (tmp_data[1] == 0x2b) || (tmp_data[1] == 0x2e))) {
            if (tmp_data[1] == 0x2a) {
                strlcpy (private_ts->chip_name,  HX_83112A_SERIES_PWON,  30);
            } else if (tmp_data[1] == 0x2b) {
                strlcpy (private_ts->chip_name,  HX_83112B_SERIES_PWON,  30);
            }

            I("%s:IC name = %s\n", __func__, private_ts->chip_name);

            I("Himax IC package %x%x%x in\n",  tmp_data[3],  tmp_data[2],  tmp_data[1]);
            ret_data = true;
            break;
        } else {
            ret_data = false;
            E("%s:Read driver ID register Fail:\n", __func__);
            E("Could NOT find Himax Chipset \n");
            E("Please check 1.VCCD,VCCA,VSP,VSN \n");
            E("2. LCM_RST,TP_RST \n");
            E("3. Power On Sequence \n");
        }
    }

#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
#if defined(HX_EN_DYNAMIC_NAME)
    hx83112_dynamic_fw_name(tmp_data[1]);
#endif
#endif

    return ret_data;
}

static int himax_hx83112_probe(void)
{
    I("%s:Enter\n", __func__);
    if(g_core_chip_dt == NULL) {
        g_core_chip_dt = kzalloc(sizeof(struct himax_chip_detect) * HX_DRIVER_MAX_IC_NUM, GFP_KERNEL);
        I("%s:1st alloc g_core_chip_dt \n", __func__);
    }
    if(g_hx_ic_dt_num < HX_DRIVER_MAX_IC_NUM) {
        g_core_chip_dt[g_hx_ic_dt_num].fp_chip_detect = hx83112_chip_detect;
        g_hx_ic_dt_num ++;
    }

    return 0;
}

static int himax_hx83112_remove(void)
{
    g_core_chip_dt[g_hx_ic_dt_num].fp_chip_detect = NULL;
    g_core_fp.fp_chip_init = NULL;
    return 0;
}

static int __init himax_hx83112_init(void)
{
    int ret = 0;

    I("%s\n", __func__);
    ret = himax_hx83112_probe();
    return 0;
}

static void __exit himax_hx83112_exit(void)
{
    himax_hx83112_remove();
}

module_init(himax_hx83112_init);
module_exit(himax_hx83112_exit);

MODULE_DESCRIPTION("HIMAX HX83112 touch driver");
MODULE_LICENSE("GPL");


