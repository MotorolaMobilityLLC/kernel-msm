/* drivers/misc/ir2e71y/ir2e71y_dbg.h  (Display Driver)
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

#ifndef IR2E71Y_DBG_H
#define IR2E71Y_DBG_H



#include <linux/tty.h>

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#if defined(CONFIG_ANDROID_ENGINEERING)
    #define IR2E71Y_LOG_ENABLE
#endif /* CONFIG_ANDROID_ENGINEERING */



extern unsigned short ir2e71y_log_lv;
#ifdef IR2E71Y_LOG_ENABLE
    #define IR2E71Y_SET_LOG_LV(lv) ir2e71y_log_lv = lv;
    #define IR2E71Y_PRINTK(lv, fmt, args...) \
            if ((ir2e71y_log_lv & lv) != 0) { \
                printk(fmt, ## args); \
            }
#else   /* IR2E71Y_LOG_ENABLE */
    #define IR2E71Y_SET_LOG_LV(lv)
    #define IR2E71Y_PRINTK(lv, fmt, args...)
#endif   /* IR2E71Y_LOG_ENABLE */

#define IR2E71Y_LOG_LV_ERR               (0x01)
#define IR2E71Y_LOG_LV_WARN              (0x02)
#define IR2E71Y_LOG_LV_DEBUG             (0x04)
#define IR2E71Y_LOG_LV_TRACE             (0x08)
#define IR2E71Y_LOG_LV_INFO              (0x10)
#define IR2E71Y_LOG_LV_PERFORM           (0x20)
#define IR2E71Y_LOG_LV_PERFORM_DEBUG     (0x40)
#define IR2E71Y_LOG_LV_I2CLOG            (0x80)
#define IR2E71Y_LOG_LV_DSILOG            (0x100)
#define IR2E71Y_LOG_LV_WAITLOG           (0x200)

#if defined(CONFIG_ANDROID_ENGINEERING)
    #define IR2E71Y_ERR(fmt, args...) \
    IR2E71Y_PRINTK(IR2E71Y_LOG_LV_ERR, KERN_ERR "[IR2E71Y_ERROR][%s] " fmt "\n", __func__, ## args)
#else   /* defined (CONFIG_ANDROID_ENGINEERING) */
    #define IR2E71Y_ERR(fmt, args...) printk(KERN_ERR "[IR2E71Y_ERROR][%s] " fmt "\n", __func__, ## args); \
        IR2E71Y_PRINTK(IR2E71Y_LOG_LV_ERR, KERN_ERR "[IR2E71Y_ERROR][%s] " fmt "\n", __func__, ## args);
#endif  /* defined (CONFIG_ANDROID_ENGINEERING) */

#define IR2E71Y_TRACE(fmt, args...) \
        IR2E71Y_PRINTK(IR2E71Y_LOG_LV_TRACE, KERN_INFO "[IR2E71Y_TRACE][%s] " fmt "\n", __func__, ## args)

#define IR2E71Y_DEBUG(fmt, args...) \
        IR2E71Y_PRINTK(IR2E71Y_LOG_LV_DEBUG, KERN_DEBUG "[IR2E71Y_DEBUG][%s] " fmt "\n", __func__, ## args)

#define IR2E71Y_WARN(fmt, args...) \
        IR2E71Y_PRINTK(IR2E71Y_LOG_LV_WARN, KERN_WARNING "[IR2E71Y_WARNING][%s] " fmt "\n", __func__, ## args)

#define IR2E71Y_INFO(fmt, args...) \
        IR2E71Y_PRINTK(IR2E71Y_LOG_LV_INFO, KERN_INFO "[IR2E71Y_INFO][%s] " fmt "\n", __func__, ## args)

#define IR2E71Y_I2CLOG(fmt, args...) \
        IR2E71Y_PRINTK(IR2E71Y_LOG_LV_I2CLOG, KERN_DEBUG "[IR2E71Y_I2CLOG][%s] " fmt "\n", __func__, ## args)

#define IR2E71Y_DSI_LOG(fmt, args...) \
        IR2E71Y_PRINTK(IR2E71Y_LOG_LV_DSILOG, KERN_DEBUG "[IR2E71Y_DSI][%s] " fmt "\n", __func__, ## args)

#define IR2E71Y_WAIT_LOG(fmt, args...) \
        IR2E71Y_PRINTK(IR2E71Y_LOG_LV_WAITLOG, KERN_DEBUG "[IR2E71Y_WAIT][%s] " fmt "\n", __func__, ## args)

#if defined(CONFIG_ANDROID_ENGINEERING)
    #define IR2E71Y_PERFORMANCE(fmt, args...) \
            IR2E71Y_PRINTK(IR2E71Y_LOG_LV_PERFORM, ",[IR2E71Y_PERFORM]" fmt "\n", ## args)
    #define IR2E71Y_PERFORMANCE_DEBUG(fmt, args...) \
            IR2E71Y_PRINTK(IR2E71Y_LOG_LV_PERFORM_DEBUG, ",[IR2E71Y_PERFORM_DEBUG]" fmt "\n", ## args)
#else /* CONFIG_ANDROID_ENGINEERING */
    #define IR2E71Y_PERFORMANCE(fmt, args...)
    #define IR2E71Y_PERFORMANCE_DEBUG(fmt, args...)
#endif /* CONFIG_ANDROID_ENGINEERING */


#if defined(CONFIG_ANDROID_ENGINEERING)
#define IR2E71Y_DBG_RECOVERY_ERROR_NONE                  (0)
#define IR2E71Y_DBG_RECOVERY_ERROR_PSALS                 (3)
#define IR2E71Y_DBG_BDIC_ERROR_DCDC1                     (5)
#endif /* CONFIG_ANDROID_ENGINEERING */

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */

#endif /* IR2E71Y_DBG_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
