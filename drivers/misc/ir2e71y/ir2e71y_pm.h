/* drivers/misc/ir2e71y/ir2e71y_pm.h  (Display Driver)
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

#ifndef IR2E71Y_PM_H
#define IR2E71Y_PM_H

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include "ir2e71y_context.h"

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define IR2E71Y_DEV_TYPE_NONE            (0x00000000)
#define IR2E71Y_DEV_TYPE_LCD             (0x00000001)
#define IR2E71Y_DEV_TYPE_BKL             (0x00000002)
#define IR2E71Y_DEV_TYPE_LED             (0x00000004)
#define IR2E71Y_DEV_TYPE_ALS             (0x00000008)
#define IR2E71Y_DEV_TYPE_PS              (0x00000010)
#define IR2E71Y_DEV_TYPE_ALS_APP         (0x00000100)
#define IR2E71Y_DEV_TYPE_CAMERA          (0x00000200)
#define IR2E71Y_DEV_TYPE_KEYLED          (0x00000400)
#define IR2E71Y_DEV_TYPE_DIAG            (0x00000800)
#define IR2E71Y_DEV_TYPE_SENSORHUB       (0x00001000)
#define IR2E71Y_DEV_TYPE_LED_AUTO_LOW    (0x00002000)
#define IR2E71Y_DEV_TYPE_TP              (0x00004000)
#define IR2E71Y_DEV_TYPE_RECOVERY        (0x01000000)
#define IR2E71Y_DEV_TYPE_DEBUG           (0x10000000)

#define IR2E71Y_DEV_TYPE_BDIC_MASK   (IR2E71Y_DEV_TYPE_LCD       | \
                                     IR2E71Y_DEV_TYPE_BKL       | \
                                     IR2E71Y_DEV_TYPE_LED       | \
                                     IR2E71Y_DEV_TYPE_ALS       | \
                                     IR2E71Y_DEV_TYPE_PS        | \
                                     IR2E71Y_DEV_TYPE_KEYLED    | \
                                     IR2E71Y_DEV_TYPE_RECOVERY  | \
                                     IR2E71Y_DEV_TYPE_TP        | \
                                     IR2E71Y_DEV_TYPE_DEBUG)

#define IR2E71Y_DEV_TYPE_LPSOC_MASK   (IR2E71Y_DEV_TYPE_LED | IR2E71Y_DEV_TYPE_KEYLED)

#define IR2E71Y_DEV_TYPE_PS_MASK     (IR2E71Y_DEV_TYPE_PS)

#define IR2E71Y_DEV_TYPE_ALS_MASK    (IR2E71Y_DEV_TYPE_LCD            | \
                                     IR2E71Y_DEV_TYPE_BKL            | \
                                     IR2E71Y_DEV_TYPE_ALS_APP        | \
                                     IR2E71Y_DEV_TYPE_CAMERA         | \
                                     IR2E71Y_DEV_TYPE_KEYLED         | \
                                     IR2E71Y_DEV_TYPE_DIAG           | \
                                     IR2E71Y_DEV_TYPE_SENSORHUB      | \
                                     IR2E71Y_DEV_TYPE_LED_AUTO_LOW   | \
                                     IR2E71Y_DEV_TYPE_DEBUG)

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */
enum {
    IR2E71Y_DEV_STATE_OFF = 0,
    IR2E71Y_DEV_STATE_ON,
    NUM_IR2E71Y_DEV_STATE
};

enum {
    IR2E71Y_DEV_REQ_INIT = 0,
    IR2E71Y_DEV_REQ_OFF,
    IR2E71Y_DEV_REQ_ON,
    NUM_IR2E71Y_DEV_REQ
};

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                    */
/* ------------------------------------------------------------------------- */
void ir2e71y_pm_API_init(struct ir2e71y_boot_context *boot_ctx);
int ir2e71y_pm_API_bdic_power_manager(int user, int onoff);
int ir2e71y_pm_API_lpsoc_power_manager(int user, int onoff);

void ir2e71y_pm_API_bdic_shutdown(void);
void ir2e71y_pm_API_bdic_resume(void);
#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
int ir2e71y_pm_API_ps_user_manager(int user, int onoff);
int ir2e71y_pm_API_als_user_manager(int user, int onoff);
int ir2e71y_pm_API_is_ps_active(void);
int ir2e71y_pm_API_is_als_active(void);
int ir2e71y_pm_API_psals_power_off(void);
int ir2e71y_pm_API_psals_error_power_recovery(void);
int ir2e71y_pm_API_panel_power_manager(int user, int onoff, int mode);
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */

#ifdef IR2E71Y_ALS_INT
int ir2e71y_pm_API_is_active_als_user(unsigned int user);
#endif /* IR2E71Y_ALS_INT */

#if defined(CONFIG_ANDROID_ENGINEERING)
void ir2e71y_pm_API_power_manager_users_dump(void);
#endif /* CONFIG_ANDROID_ENGINEERING */

#endif /* IR2E71Y_PM_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
