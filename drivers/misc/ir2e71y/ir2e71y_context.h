/* drivers/misc/ir2e71y8/ir2e71y_context.h  (Display Driver)
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
};

#endif /* IR2E71Y_KERL_CONTEXT_H */
