/* drivers/misc/ir2e71y/ir2e71y_context.h  (Display Driver)
 *
 * Copyright (C) 2016 SHARP CORPORATION
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef IR2E71Y_KERL_CONTEXT_H
#define IR2E71Y_KERL_CONTEXT_H
/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include "linux/ir2e71y.h"
#include "ir2e71y_priv.h"
struct ir2e71y_lcddr_phy_gmm_reg {
    unsigned char  status;
    unsigned short buf[60*3];
    unsigned char  applied_voltage[12];
    unsigned int   chksum;
};

struct shdisp_ledc_req {
    unsigned int red[2];
    unsigned int green[2];
    unsigned int blue[2];
    int led_mode;
    int on_count;
};

struct ir2e71y_ledc_status {
    int ledc_is_exist;
    int power_status;
    struct shdisp_ledc_req ledc_req;
};

struct ir2e71y_boot_context {
    int driver_is_initialized;
    unsigned short hw_handset;
    unsigned short hw_revision;
    unsigned char device_code;
    unsigned char disp_on_status;
    int handset_color;
    int upper_unit_is_connected;
    int main_disp_status;
    int is_trickled;
    struct ir2e71y_main_bkl_ctl main_bkl;
    struct ir2e71y_tri_led  tri_led;
    unsigned short vcom;
    unsigned short vcom_low;
    unsigned short vcom_nvram;
    struct ir2e71y_photo_sensor_adj photo_sensor_adj;
    struct ir2e71y_lcddr_phy_gmm_reg lcddr_phy_gmm;
    struct ir2e71y_ledc_status ledc_status;
    int bdic_is_exist;
    int bdic_chipver;
    struct ir2e71y_bdic_status bdic_status;
    struct ir2e71y_psals_status psals_status;
    unsigned long long pad1;
    char pad2[3];
    unsigned char pad3[4][3];
    unsigned short lut_status;
    struct ir2e71y_lut_data lut;
};

struct ir2e71y_kernel_context {
    int driver_is_open;
    int driver_open_cnt;
    int driver_is_initialized;
    int shutdown_in_progress;
    int thermal_status;
    int usb_chg_status;
    int main_disp_status;
#ifdef IR2E71Y_TRV_NM2
    int trv_status;
#endif /* IR2E71Y_TRV_NM2 */
    struct ir2e71y_main_bkl_ctl main_bkl;
    struct ir2e71y_tri_led tri_led;
#ifdef IR2E71Y_KEY_LED
    struct ir2e71y_key_bkl_ctl key_bkl_ctl;
#endif /* IR2E71Y_KEY_LED */
#ifdef IR2E71Y_SYSFS_LED
    struct ir2e71y_tri_led sysfs_led1;
#ifdef IR2E71Y_COLOR_LED_TWIN
    struct ir2e71y_tri_led sysfs_led2;
#endif /* IR2E71Y_COLOR_LED_TWIN */
#endif /* IR2E71Y_SYSFS_LED */
#ifdef IR2E71Y_LED_INT
    bool led_auto_low_enable;
#endif /* IR2E71Y_LED_INT */
    bool led_set_color_reject;
    int bdic_chipver;
    int bdic_is_exist;
    struct ir2e71y_boot_context boot_ctx;
};

#endif /* IR2E71Y_KERL_CONTEXT_H */
