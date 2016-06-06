/* drivers/misc/ir2e71y/ir2e71y_pm.c  (Display Driver)
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

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include "ir2e71y_define.h"
#include <linux/ir2e71y.h>

#include "ir2e71y_priv.h"
#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */

#include "ir2e71y_io.h"
#include "ir2e71y_main.h"
#include "ir2e71y_led.h"
#include "ir2e71y_func.h"
#include "ir2e71y_led.h"
#include "ir2e71y_pm.h"
#include "ir2e71y_dbg.h"

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define IR2E71Y_NAME "bl71y8"

#define GET_DEV_STATE_STR(n)    (((n) == IR2E71Y_DEV_STATE_OFF)    ? "OFF"    : \
                                (((n) == IR2E71Y_DEV_STATE_ON)     ? "ON"     : \
                                                                    "UNKNOWN"))

#define GET_DEV_REQ_STR(n)      (((n) == IR2E71Y_DEV_REQ_INIT)     ? "INIT"   : \
                                (((n) == IR2E71Y_DEV_REQ_OFF)      ? "OFF"    : \
                                (((n) == IR2E71Y_DEV_REQ_ON)       ? "ON"     : \
                                                                    "UNKNOWN")))

#define GET_PSALS_STATE_STR(n)  (((n) == IR2E71Y_PSALS_STATE_OFF)  ? "OFF"    : \
                                (((n) == IR2E71Y_PSALS_STATE_INIT) ? "INIT"   : \
                                (((n) == IR2E71Y_PSALS_STATE_ON)   ? "ON"     : \
                                                                    "UNKNOWN")))

#define GET_SENSOR_STATE_STR(n) (((n) == IR2E71Y_SENSOR_STATE_POWER_OFF)       ? "ALL_OFF"    : \
                                (((n) == IR2E71Y_SENSOR_STATE_POWER_ON)        ? "ON/OFF/OFF" : \
                                (((n) == IR2E71Y_SENSOR_STATE_PROX_ON_ALS_OFF) ? "ON/ON/OFF"  : \
                                (((n) == IR2E71Y_SENSOR_STATE_PROX_OFF_ALS_ON) ? "ON/OFF/ON"  : \
                                (((n) == IR2E71Y_SENSOR_STATE_PROX_ON_ALS_ON)  ? "ON/ON/ON"   : \
                                                                                "UNKNOWN")))))

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */
enum {
    IR2E71Y_PSALS_STATE_OFF = 0,
    IR2E71Y_PSALS_STATE_INIT,
    IR2E71Y_PSALS_STATE_ON
};

enum {
    IR2E71Y_SENSOR_STATE_POWER_OFF = 0,
    IR2E71Y_SENSOR_STATE_POWER_ON,
    IR2E71Y_SENSOR_STATE_PROX_ON_ALS_OFF,
    IR2E71Y_SENSOR_STATE_PROX_OFF_ALS_ON,
    IR2E71Y_SENSOR_STATE_PROX_ON_ALS_ON,
    NUM_IR2E71Y_SENSOR_STATE
};

struct ir2e71y_pm_context {
    struct ir2e71y_bdic_status bdic_status;
    struct ir2e71y_bdic_status  panel_status;
    struct ir2e71y_psals_status psals_status;
};

struct ir2e71y_pm_psals_state_row {
    int user;
    int onoff;
    int (* funcs[NUM_IR2E71Y_SENSOR_STATE])(void);
};

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_bdic_set_active(void);
static int ir2e71y_pm_bdic_set_standby(void);
#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
static int ir2e71y_pm_psals_power_manager(int user, int onoff);
static const struct ir2e71y_pm_psals_state_row *ir2e71y_pm_psals_find_state_table(int user, int onoff);
static int ir2e71y_pm_psals_power_on(void);
static int ir2e71y_pm_psals_power_off(void);
static int ir2e71y_pm_psals_init(void);
static int ir2e71y_pm_psals_deinit(void);
static int ir2e71y_pm_psals_ps_init_als_off(void);
static int ir2e71y_pm_psals_ps_init_als_on(void);
static int ir2e71y_pm_psals_ps_deinit_als_off(void);
static int ir2e71y_pm_psals_ps_deinit_als_on(void);
static int ir2e71y_pm_psals_als_init_ps_off(void);
static int ir2e71y_pm_psals_als_init_ps_on(void);
static int ir2e71y_pm_psals_als_deinit_ps_off(void);
static int ir2e71y_pm_psals_als_deinit_ps_on(void);
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */

static struct ir2e71y_pm_context ir2e71y_pm_ctx;
static struct ir2e71y_pm_context ir2e71y_pm_ctx_recovery;
struct ir2e71y_bdic_status lpsoc_status;

#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
static const struct ir2e71y_pm_psals_state_row
        ir2e71y_pm_psals_state_table[] = {
    {
        IR2E71Y_DEV_TYPE_ALS, IR2E71Y_DEV_REQ_INIT,
        {
            ir2e71y_pm_psals_init,
            NULL,
            ir2e71y_pm_psals_als_init_ps_on,
            NULL,
            NULL
        }
    },
    {
        IR2E71Y_DEV_TYPE_PS, IR2E71Y_DEV_REQ_ON,
        {
            ir2e71y_pm_psals_ps_init_als_off,
            ir2e71y_pm_psals_ps_init_als_off,
            NULL,
            ir2e71y_pm_psals_ps_init_als_on,
            NULL
        }
    },
    {
        IR2E71Y_DEV_TYPE_PS, IR2E71Y_DEV_REQ_OFF,
        {
            NULL,
            ir2e71y_pm_psals_deinit,
            ir2e71y_pm_psals_ps_deinit_als_off,
            NULL,
            ir2e71y_pm_psals_ps_deinit_als_on
        }
    },
    {
        IR2E71Y_DEV_TYPE_ALS, IR2E71Y_DEV_REQ_ON,
        {
            ir2e71y_pm_psals_als_init_ps_off,
            ir2e71y_pm_psals_als_init_ps_off,
            ir2e71y_pm_psals_als_init_ps_on,
            NULL,
            NULL
        }
    },
    {
        IR2E71Y_DEV_TYPE_ALS, IR2E71Y_DEV_REQ_OFF,
        {
            NULL,
            ir2e71y_pm_psals_deinit,
            NULL,
            ir2e71y_pm_psals_als_deinit_ps_off,
            ir2e71y_pm_psals_als_deinit_ps_on
        }
    }
};

#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */


/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_API_init                                                        */
/* ------------------------------------------------------------------------- */
void ir2e71y_pm_API_init(struct ir2e71y_boot_context *boot_ctx)
{
    memcpy(&(ir2e71y_pm_ctx.bdic_status), &(boot_ctx->bdic_status), sizeof(struct ir2e71y_bdic_status));
    memcpy(&(ir2e71y_pm_ctx.psals_status), &(boot_ctx->psals_status), sizeof(struct ir2e71y_psals_status));
    ir2e71y_pm_ctx.panel_status.power_status = IR2E71Y_DEV_STATE_OFF;
    ir2e71y_pm_ctx.panel_status.users = IR2E71Y_DEV_TYPE_NONE;

    IR2E71Y_DEBUG("ir2e71y_pm_ctx.psals_status.power_status=%d", ir2e71y_pm_ctx.psals_status.power_status);
    if (ir2e71y_pm_ctx.psals_status.power_status != IR2E71Y_SENSOR_STATE_POWER_OFF) {
        ir2e71y_API_psals_recovery_subscribe();
    }
    ir2e71y_pm_ctx_recovery.bdic_status.power_status  = IR2E71Y_DEV_STATE_OFF;
    ir2e71y_pm_ctx_recovery.bdic_status.users         = IR2E71Y_DEV_TYPE_NONE;

    ir2e71y_pm_ctx_recovery.psals_status.power_status = IR2E71Y_SENSOR_STATE_POWER_OFF;

    lpsoc_status.power_status  = IR2E71Y_DEV_STATE_OFF;
    lpsoc_status.users         = IR2E71Y_DEV_TYPE_NONE;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_API_bdic_power_manager                                          */
/* ------------------------------------------------------------------------- */
int ir2e71y_pm_API_bdic_power_manager(int user, int onoff)
{
    int ret;
    unsigned long users_wk;

    IR2E71Y_TRACE("in  [BDIC_PM]  users:0x%08X, power_status:%s, user:0x%08X, onoff:%s",
            (int)ir2e71y_pm_ctx.bdic_status.users,
            GET_DEV_STATE_STR(ir2e71y_pm_ctx.bdic_status.power_status),
            user,
            GET_DEV_REQ_STR(onoff));

    if ((user != (user & IR2E71Y_DEV_TYPE_BDIC_MASK)) || (user == IR2E71Y_DEV_TYPE_NONE)) {
        IR2E71Y_ERR("invalid user argument. user:0x%08X", user);
        return IR2E71Y_RESULT_FAILURE;
    }

    if ((onoff != IR2E71Y_DEV_REQ_OFF) &&
        (onoff != IR2E71Y_DEV_REQ_ON)) {
        IR2E71Y_ERR("invalid onoff argument. onoff:%d", onoff);
        return IR2E71Y_RESULT_FAILURE;
    }

    if (ir2e71y_API_get_bdic_is_exist() != IR2E71Y_BDIC_IS_EXIST) {
        IR2E71Y_ERR("bdic does not exist.");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (onoff == IR2E71Y_DEV_REQ_ON) {
        switch (ir2e71y_pm_ctx.bdic_status.power_status) {
        case IR2E71Y_DEV_STATE_OFF:
            ret = ir2e71y_pm_bdic_set_active();
            if (ret != IR2E71Y_RESULT_SUCCESS) {
                IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_pm_bdic_set_active.");
                return IR2E71Y_RESULT_FAILURE;
            }
            ir2e71y_pm_ctx.bdic_status.power_status = IR2E71Y_DEV_STATE_ON;
            break;
        }
    } else if (onoff == IR2E71Y_DEV_REQ_OFF) {
        switch (ir2e71y_pm_ctx.bdic_status.power_status) {
        case IR2E71Y_DEV_STATE_ON:
            users_wk = ir2e71y_pm_ctx.bdic_status.users;
            users_wk &= (unsigned long)(~(user & IR2E71Y_DEV_TYPE_BDIC_MASK));
            if (users_wk == IR2E71Y_DEV_TYPE_NONE) {
                ret = ir2e71y_pm_bdic_set_standby();
                if (ret != IR2E71Y_RESULT_SUCCESS) {
                    IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_pm_bdic_set_standby.");
                    return IR2E71Y_RESULT_FAILURE;
                }
                ir2e71y_pm_ctx.bdic_status.power_status = IR2E71Y_DEV_STATE_OFF;
            }
            break;
        }
    }

    if (onoff == IR2E71Y_DEV_REQ_ON) {
        ir2e71y_pm_ctx.bdic_status.users |= (unsigned long)(user & IR2E71Y_DEV_TYPE_BDIC_MASK);
    } else if (onoff == IR2E71Y_DEV_REQ_OFF) {
        ir2e71y_pm_ctx.bdic_status.users &= (unsigned long)(~(user & IR2E71Y_DEV_TYPE_BDIC_MASK));
    }


    IR2E71Y_TRACE("out [BDIC_PM]  users:0x%08X, power_status:%s",
            (int)ir2e71y_pm_ctx.bdic_status.users,
            GET_DEV_STATE_STR(ir2e71y_pm_ctx.bdic_status.power_status));

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_API_lpsoc_power_manager                                         */
/* ------------------------------------------------------------------------- */
int ir2e71y_pm_API_lpsoc_power_manager(int user, int onoff)
{
    unsigned long users_wk;

    IR2E71Y_TRACE("in  [LPSOC_PM]  users:0x%08X, power_status:%s, user:0x%08X, onoff:%s",
            (int)lpsoc_status.users,
            GET_DEV_STATE_STR(lpsoc_status.power_status),
            user,
            GET_DEV_REQ_STR(onoff));

    if ((user != (user & IR2E71Y_DEV_TYPE_LPSOC_MASK)) || (user == IR2E71Y_DEV_TYPE_NONE)) {
        IR2E71Y_ERR("invalid user argument. user:0x%08X", user);
        return IR2E71Y_RESULT_FAILURE;
    }

    if ((onoff != IR2E71Y_DEV_REQ_OFF) &&
        (onoff != IR2E71Y_DEV_REQ_ON)) {
        IR2E71Y_ERR("invalid onoff argument. onoff:%d", onoff);
        return IR2E71Y_RESULT_FAILURE;
    }

    if (onoff == IR2E71Y_DEV_REQ_ON) {
        switch (lpsoc_status.power_status) {
        case IR2E71Y_DEV_STATE_OFF:
            ir2e71y_bdic_API_lposc_on();
            lpsoc_status.power_status = IR2E71Y_DEV_STATE_ON;
            break;
        }
    } else if (onoff == IR2E71Y_DEV_REQ_OFF) {
        switch (lpsoc_status.power_status) {
        case IR2E71Y_DEV_STATE_ON:
            users_wk = lpsoc_status.users;
            users_wk &= (unsigned long)(~(user & IR2E71Y_DEV_TYPE_LPSOC_MASK));
            if (users_wk == IR2E71Y_DEV_TYPE_NONE) {
                ir2e71y_bdic_API_lposc_off();
                lpsoc_status.power_status = IR2E71Y_DEV_STATE_OFF;
            }
            break;
        }
    }

    if (onoff == IR2E71Y_DEV_REQ_ON) {
        lpsoc_status.users |= (unsigned long)(user & IR2E71Y_DEV_TYPE_LPSOC_MASK);
    } else if (onoff == IR2E71Y_DEV_REQ_OFF) {
        lpsoc_status.users &= (unsigned long)(~(user & IR2E71Y_DEV_TYPE_LPSOC_MASK));
    }


    IR2E71Y_TRACE("out [LPSOC_PM]  users:0x%08X, power_status:%s",
            (int)lpsoc_status.users,
            GET_DEV_STATE_STR(lpsoc_status.power_status));

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_API_bdic_shutdown                                               */
/* ------------------------------------------------------------------------- */
void ir2e71y_pm_API_bdic_shutdown(void)
{
    IR2E71Y_TRACE("in");

    ir2e71y_pm_ctx_recovery.bdic_status.power_status = ir2e71y_pm_ctx.bdic_status.power_status;
    ir2e71y_pm_ctx_recovery.bdic_status.users        = ir2e71y_pm_ctx.bdic_status.users;

    ir2e71y_pm_API_bdic_power_manager(IR2E71Y_DEV_TYPE_BDIC_MASK, IR2E71Y_DEV_REQ_OFF);

    ir2e71y_pm_ctx.bdic_status.power_status = IR2E71Y_DEV_STATE_OFF;

    IR2E71Y_TRACE("out");

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_API_bdic_resume                                                 */
/* ------------------------------------------------------------------------- */
void ir2e71y_pm_API_bdic_resume(void)
{
    IR2E71Y_TRACE("in");

    if (ir2e71y_pm_ctx_recovery.bdic_status.users != IR2E71Y_DEV_TYPE_NONE) {
        ir2e71y_pm_API_bdic_power_manager(ir2e71y_pm_ctx_recovery.bdic_status.users, IR2E71Y_DEV_REQ_ON);
    }

    ir2e71y_pm_ctx_recovery.bdic_status.power_status = IR2E71Y_DEV_STATE_OFF;
    ir2e71y_pm_ctx_recovery.bdic_status.users        = IR2E71Y_DEV_TYPE_NONE;

    IR2E71Y_TRACE("out");

    return;
}

#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_API_ps_user_manager                                             */
/* ------------------------------------------------------------------------- */
int ir2e71y_pm_API_ps_user_manager(int user, int onoff)
{
    int ret;

    IR2E71Y_TRACE("in     [PS_UM]    ps_um_status:%s, user:0x%08X, onoff:%s",
            GET_PSALS_STATE_STR(ir2e71y_pm_ctx.psals_status.ps_um_status),
            user,
            GET_DEV_REQ_STR(onoff));

    if ((user != (user & IR2E71Y_DEV_TYPE_PS_MASK)) || (user == IR2E71Y_DEV_TYPE_NONE)) {
        IR2E71Y_ERR("invalid user argument. user:0x%08X", user);
        return IR2E71Y_RESULT_FAILURE;
    }

    if ((onoff != IR2E71Y_DEV_REQ_OFF) &&
        (onoff != IR2E71Y_DEV_REQ_ON)) {
        IR2E71Y_ERR("invalid onoff argument. onoff:%d", onoff);
        return IR2E71Y_RESULT_FAILURE;
    }

    if (onoff == IR2E71Y_DEV_REQ_ON) {
        if (ir2e71y_pm_ctx.psals_status.ps_um_status == IR2E71Y_PSALS_STATE_OFF) {
            ret = ir2e71y_pm_API_bdic_power_manager(IR2E71Y_DEV_TYPE_PS, IR2E71Y_DEV_REQ_ON);
            if (ret != IR2E71Y_RESULT_SUCCESS) {
                IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_pm_API_bdic_power_manager.");
                return IR2E71Y_RESULT_FAILURE;
            }

            ret = ir2e71y_pm_psals_power_manager(IR2E71Y_DEV_TYPE_PS, IR2E71Y_DEV_REQ_ON);
            if (ret != IR2E71Y_RESULT_SUCCESS) {
                IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_pm_psals_power_manager.");
                return IR2E71Y_RESULT_FAILURE;
            }

            ir2e71y_pm_ctx.psals_status.ps_um_status = IR2E71Y_PSALS_STATE_ON;
        }
    } else if (onoff == IR2E71Y_DEV_REQ_OFF) {
        if (ir2e71y_pm_ctx.psals_status.ps_um_status == IR2E71Y_PSALS_STATE_ON) {
            ret = ir2e71y_pm_psals_power_manager(IR2E71Y_DEV_TYPE_PS, IR2E71Y_DEV_REQ_OFF);
            if (ret != IR2E71Y_RESULT_SUCCESS) {
                IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_pm_psals_power_manager.");
                return IR2E71Y_RESULT_FAILURE;
            }

            ret = ir2e71y_pm_API_bdic_power_manager(IR2E71Y_DEV_TYPE_PS, IR2E71Y_DEV_REQ_OFF);
            if (ret != IR2E71Y_RESULT_SUCCESS) {
                IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_pm_API_bdic_power_manager.");
                return IR2E71Y_RESULT_FAILURE;
            }

            ir2e71y_pm_ctx.psals_status.ps_um_status = IR2E71Y_PSALS_STATE_OFF;
        }
    }

#ifdef IR2E71Y_APPSBL
    ir2e71y_API_set_boot_context_psals(&ir2e71y_pm_ctx.psals_status);
#endif  /* IR2E71Y_APPSBL */

    IR2E71Y_TRACE("out    [PS_UM]    ps_um_status:%s",
            GET_PSALS_STATE_STR(ir2e71y_pm_ctx.psals_status.ps_um_status));

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_API_als_user_manager                                            */
/* ------------------------------------------------------------------------- */
int ir2e71y_pm_API_als_user_manager(int user, int onoff)
{
    int ret;
    unsigned long users_wk;

    IR2E71Y_TRACE("in    [ALS_UM]   als_users:0x%08X, als_um_status:%s, user:0x%08X, onoff:%s",
            (int)ir2e71y_pm_ctx.psals_status.als_users,
            GET_PSALS_STATE_STR(ir2e71y_pm_ctx.psals_status.als_um_status),
            user,
            GET_DEV_REQ_STR(onoff));

    if ((user != (user & IR2E71Y_DEV_TYPE_ALS_MASK)) || (user == IR2E71Y_DEV_TYPE_NONE)) {
        IR2E71Y_ERR("invalid user argument. user:0x%08X", user);
        return IR2E71Y_RESULT_FAILURE;
    }

    if ((onoff != IR2E71Y_DEV_REQ_INIT) &&
        (onoff != IR2E71Y_DEV_REQ_OFF) &&
        (onoff != IR2E71Y_DEV_REQ_ON)) {
        IR2E71Y_ERR("invalid onoff argument. onoff:%d", onoff);
        return IR2E71Y_RESULT_FAILURE;
    }

    switch (onoff) {
    case IR2E71Y_DEV_REQ_INIT:
        switch (ir2e71y_pm_ctx.psals_status.als_um_status) {
        case IR2E71Y_PSALS_STATE_OFF:
            ret = ir2e71y_pm_API_bdic_power_manager(IR2E71Y_DEV_TYPE_ALS, IR2E71Y_DEV_REQ_ON);
            if (ret != IR2E71Y_RESULT_SUCCESS) {
                IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_pm_API_bdic_power_manager.");
                return IR2E71Y_RESULT_FAILURE;
            }

            ret = ir2e71y_pm_psals_power_manager(IR2E71Y_DEV_TYPE_ALS, IR2E71Y_DEV_REQ_INIT);
            if (ret != IR2E71Y_RESULT_SUCCESS) {
                IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_pm_psals_power_manager.");
                return IR2E71Y_RESULT_FAILURE;
            }

            ir2e71y_pm_ctx.psals_status.als_um_status = IR2E71Y_PSALS_STATE_INIT;
            break;
        }
        break;

    case IR2E71Y_DEV_REQ_ON:
        switch (ir2e71y_pm_ctx.psals_status.als_um_status) {
        case IR2E71Y_PSALS_STATE_OFF:
            ret = ir2e71y_pm_API_bdic_power_manager(IR2E71Y_DEV_TYPE_ALS, IR2E71Y_DEV_REQ_ON);
            if (ret != IR2E71Y_RESULT_SUCCESS) {
                IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_pm_API_bdic_power_manager.");
                return IR2E71Y_RESULT_FAILURE;
            }

        case IR2E71Y_PSALS_STATE_INIT:
            ret = ir2e71y_pm_psals_power_manager(IR2E71Y_DEV_TYPE_ALS, IR2E71Y_DEV_REQ_ON);
            if (ret != IR2E71Y_RESULT_SUCCESS) {
                IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_pm_psals_power_manager.");
                return IR2E71Y_RESULT_FAILURE;
            }

            ir2e71y_pm_ctx.psals_status.als_um_status = IR2E71Y_PSALS_STATE_ON;
            break;
        }
        break;

    case IR2E71Y_DEV_REQ_OFF:
        switch (ir2e71y_pm_ctx.psals_status.als_um_status) {
        case IR2E71Y_PSALS_STATE_INIT:
        case IR2E71Y_PSALS_STATE_ON:
            users_wk = ir2e71y_pm_ctx.psals_status.als_users;
            users_wk &= (unsigned long)(~(user & IR2E71Y_DEV_TYPE_ALS_MASK));
            if (users_wk == IR2E71Y_DEV_TYPE_NONE) {
                ret = ir2e71y_pm_psals_power_manager(IR2E71Y_DEV_TYPE_ALS, IR2E71Y_DEV_REQ_OFF);
                if (ret != IR2E71Y_RESULT_SUCCESS) {
                    IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_pm_psals_power_manager.");
                    return IR2E71Y_RESULT_FAILURE;
                }

                ret = ir2e71y_pm_API_bdic_power_manager(IR2E71Y_DEV_TYPE_ALS, IR2E71Y_DEV_REQ_OFF);
                if (ret != IR2E71Y_RESULT_SUCCESS) {
                    IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_pm_API_bdic_power_manager.");
                    return IR2E71Y_RESULT_FAILURE;
                }

                ir2e71y_pm_ctx.psals_status.als_um_status = IR2E71Y_PSALS_STATE_OFF;
            }
            break;
        }
        break;
    }

    switch (onoff) {
    case IR2E71Y_DEV_REQ_INIT:
    case IR2E71Y_DEV_REQ_ON:
        ir2e71y_pm_ctx.psals_status.als_users |= (unsigned long)(user & IR2E71Y_DEV_TYPE_ALS_MASK);
        break;

    case IR2E71Y_DEV_REQ_OFF:
        ir2e71y_pm_ctx.psals_status.als_users &= (unsigned long)(~(user & IR2E71Y_DEV_TYPE_ALS_MASK));
        break;
    }

#ifdef IR2E71Y_APPSBL
    ir2e71y_API_set_boot_context_psals(&ir2e71y_pm_ctx.psals_status);
#endif  /* IR2E71Y_APPSBL */

    IR2E71Y_TRACE("out   [ALS_UM]   als_users:0x%08X, als_um_status:%s",
            (int)ir2e71y_pm_ctx.psals_status.als_users,
            GET_PSALS_STATE_STR(ir2e71y_pm_ctx.psals_status.als_um_status));

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_API_psals_power_off                                             */
/* ------------------------------------------------------------------------- */
int ir2e71y_pm_API_psals_power_off(void)
{
    IR2E71Y_TRACE("in");

    ir2e71y_pm_ctx_recovery.psals_status.power_status = ir2e71y_pm_ctx.psals_status.power_status;

    ir2e71y_pm_psals_power_manager(IR2E71Y_DEV_TYPE_ALS, IR2E71Y_DEV_REQ_OFF);
    ir2e71y_pm_psals_power_manager(IR2E71Y_DEV_TYPE_PS,  IR2E71Y_DEV_REQ_OFF);

    IR2E71Y_TRACE("out");

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_API_psals_error_power_recovery                                  */
/* ------------------------------------------------------------------------- */
int ir2e71y_pm_API_psals_error_power_recovery(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    if ((ir2e71y_pm_ctx_recovery.psals_status.power_status == IR2E71Y_SENSOR_STATE_PROX_ON_ALS_OFF) ||
        (ir2e71y_pm_ctx_recovery.psals_status.power_status == IR2E71Y_SENSOR_STATE_PROX_ON_ALS_ON)) {
        ret = ir2e71y_pm_psals_power_manager(IR2E71Y_DEV_TYPE_PS, IR2E71Y_DEV_REQ_ON);
        if (ret != IR2E71Y_RESULT_SUCCESS) {
            IR2E71Y_DEBUG("out ret = IR2E71Y_RESULT_FAILURE");
            return IR2E71Y_RESULT_FAILURE;
        }
    }

    if ((ir2e71y_pm_ctx_recovery.psals_status.power_status == IR2E71Y_SENSOR_STATE_PROX_OFF_ALS_ON) ||
        (ir2e71y_pm_ctx_recovery.psals_status.power_status == IR2E71Y_SENSOR_STATE_PROX_ON_ALS_ON)) {
        ret = ir2e71y_pm_psals_power_manager(IR2E71Y_DEV_TYPE_ALS, IR2E71Y_DEV_REQ_ON);
        if (ret != IR2E71Y_RESULT_SUCCESS) {
            IR2E71Y_DEBUG("out ret = IR2E71Y_RESULT_FAILURE");
            return IR2E71Y_RESULT_FAILURE;
        }
    }

    ir2e71y_pm_ctx_recovery.psals_status.power_status = IR2E71Y_SENSOR_STATE_POWER_OFF;

    IR2E71Y_TRACE("out ret = IR2E71Y_RESULT_SUCCESS");

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_API_is_ps_active                                                */
/* ------------------------------------------------------------------------- */
int ir2e71y_pm_API_is_ps_active(void)
{
    if (ir2e71y_pm_ctx.psals_status.ps_um_status == IR2E71Y_PSALS_STATE_ON) {
        return IR2E71Y_DEV_STATE_ON;
    } else {
        return IR2E71Y_DEV_STATE_OFF;
    }
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_API_is_als_active                                               */
/* ------------------------------------------------------------------------- */
int ir2e71y_pm_API_is_als_active(void)
{
    if (ir2e71y_pm_ctx.psals_status.als_um_status == IR2E71Y_PSALS_STATE_ON) {
        return IR2E71Y_DEV_STATE_ON;
    } else {
        return IR2E71Y_DEV_STATE_OFF;
    }
}

#ifdef IR2E71Y_ALS_INT
/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_API_is_active_als_user                                          */
/* ------------------------------------------------------------------------- */
int ir2e71y_pm_API_is_active_als_user(unsigned int user)
{
    if((user & ir2e71y_pm_ctx.psals_status.als_users) != 0) {
        return IR2E71Y_DEV_STATE_ON;
    } else {
        return IR2E71Y_DEV_STATE_OFF;
    }
}
#endif /* IR2E71Y_ALS_INT */

#if defined(CONFIG_ANDROID_ENGINEERING)
/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_API_power_manager_users_dump                                    */
/* ------------------------------------------------------------------------- */
void ir2e71y_pm_API_power_manager_users_dump(void)
{
    printk("[BL71Y8] [BDIC_PM]  users:0x%08X, power_status:%s\n",
            (int)ir2e71y_pm_ctx.bdic_status.users,
            GET_DEV_STATE_STR(ir2e71y_pm_ctx.bdic_status.power_status));

    printk("[BL71Y8] [PANEL_PM]  users:0x%08X, power_status:%s\n",
            (int)ir2e71y_pm_ctx.panel_status.users,
            GET_DEV_STATE_STR(ir2e71y_pm_ctx.panel_status.power_status));

    printk("[BL71Y8] [PSALS_PM] power_status(power/ps/als):%s\n",
            GET_SENSOR_STATE_STR(ir2e71y_pm_ctx.psals_status.power_status));

    printk("[BL71Y8] [PS_UM]    ps_um_status:%s\n",
            GET_PSALS_STATE_STR(ir2e71y_pm_ctx.psals_status.ps_um_status));

    printk("[BL71Y8] [ALS_UM]   als_users:0x%08X, als_um_status:%s\n",
            (int)ir2e71y_pm_ctx.psals_status.als_users,
            GET_PSALS_STATE_STR(ir2e71y_pm_ctx.psals_status.als_um_status));

    printk("[BL71Y8] [LPSOC_PM]  users:0x%08X, power_status:%s\n",
            (int)lpsoc_status.users,
            GET_DEV_STATE_STR(lpsoc_status.power_status));

    return;
}
#endif /* CONFIG_ANDROID_ENGINEERING */
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_bdic_set_active                                                 */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_bdic_set_active(void)
{
    int ret;

    IR2E71Y_TRACE("in bdic_status:%d", ir2e71y_pm_ctx.bdic_status.power_status);

    ret = ir2e71y_bdic_API_set_active();

    IR2E71Y_TRACE("out ret=%d", ret);

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_bdic_set_standby                                                */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_bdic_set_standby(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_API_set_standby();

    IR2E71Y_TRACE("out ret=%d", ret);

    return ret;
}

#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_psals_power_manager                                             */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_psals_power_manager(int user, int onoff)
{
    int ret;
    int (* func)(void);
    int next_status;
    int curr_status = ir2e71y_pm_ctx.psals_status.power_status;
    const struct ir2e71y_pm_psals_state_row *state_row = NULL;

    IR2E71Y_TRACE("in     [PSALS_PM] power_status(power/ps/als):%s, user:0x%08X, onoff:%s",
            GET_SENSOR_STATE_STR(ir2e71y_pm_ctx.psals_status.power_status),
            user,
            GET_DEV_REQ_STR(onoff));

    if ((user != IR2E71Y_DEV_TYPE_PS) &&
        (user != IR2E71Y_DEV_TYPE_ALS)) {
        IR2E71Y_ERR("invalid user argument. user:0x%08X", user);
        return IR2E71Y_RESULT_FAILURE;
    }

    if ((onoff != IR2E71Y_DEV_REQ_INIT) &&
        (onoff != IR2E71Y_DEV_REQ_OFF) &&
        (onoff != IR2E71Y_DEV_REQ_ON)) {
        IR2E71Y_ERR("invalid onoff argument. onoff:%d", onoff);
        return IR2E71Y_RESULT_FAILURE;
    }

    if (ir2e71y_API_get_bdic_is_exist() != IR2E71Y_BDIC_IS_EXIST) {
        IR2E71Y_ERR("bdic does not exist.");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_pm_ctx.bdic_status.power_status != IR2E71Y_DEV_STATE_ON) {
        IR2E71Y_WARN("bdic is not active.");
        return IR2E71Y_RESULT_FAILURE;
    }

    state_row = ir2e71y_pm_psals_find_state_table(user, onoff);
    if (state_row == NULL) {
        IR2E71Y_ERR("invalid arguments. user:0x%08X onoff:%d", user, onoff);
        return IR2E71Y_RESULT_FAILURE;
    }

    if (curr_status == IR2E71Y_SENSOR_STATE_POWER_OFF) {
        switch (onoff) {
        case IR2E71Y_DEV_REQ_INIT:
        case IR2E71Y_DEV_REQ_ON:
            ret = ir2e71y_pm_psals_power_on();
            if (ret != IR2E71Y_RESULT_SUCCESS) {
                IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_pm_psals_power_on.");
                return IR2E71Y_RESULT_FAILURE;
            }
            if (onoff == IR2E71Y_DEV_REQ_ON) {
                ir2e71y_IO_API_delay_us(10 * 1000);
            }
            break;
        }
    }

    func = state_row->funcs[curr_status];
    if (func != NULL) {
        next_status = func();
    } else {
        next_status = curr_status;
    }

    if (next_status != curr_status) {
        if (next_status == IR2E71Y_SENSOR_STATE_POWER_OFF) {
            ret = ir2e71y_pm_psals_power_off();
            if (ret != IR2E71Y_RESULT_SUCCESS) {
                IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_pm_psals_power_off.");
                return IR2E71Y_RESULT_FAILURE;
            }
        }
    }

    ir2e71y_pm_ctx.psals_status.power_status = next_status;

#ifdef IR2E71Y_APPSBL
    ir2e71y_API_set_boot_context_psals(&ir2e71y_pm_ctx.psals_status);
#endif  /* IR2E71Y_APPSBL */

    IR2E71Y_TRACE("out    [PSALS_PM] power_status(power/ps/als):%s",
            GET_SENSOR_STATE_STR(ir2e71y_pm_ctx.psals_status.power_status));

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_psals_find_state_table                                          */
/* ------------------------------------------------------------------------- */
static const struct ir2e71y_pm_psals_state_row *ir2e71y_pm_psals_find_state_table(int user, int onoff)
{
    unsigned int i;
    const struct ir2e71y_pm_psals_state_row *state_row = NULL;

    for (i = 0; i < ARRAY_SIZE(ir2e71y_pm_psals_state_table); i++) {
        if (((ir2e71y_pm_psals_state_table[i].user & user) == user) &&
             (ir2e71y_pm_psals_state_table[i].onoff        == onoff)) {
            state_row = &ir2e71y_pm_psals_state_table[i];
            break;
        }
    }

    return state_row;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_psals_power_on                                                  */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_psals_power_on(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_API_psals_power_on();
    if (ret == IR2E71Y_RESULT_SUCCESS) {
        if (ir2e71y_pm_ctx_recovery.psals_status.power_status == IR2E71Y_SENSOR_STATE_POWER_OFF) {
            ir2e71y_API_psals_recovery_subscribe();
        }
    } else {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_psals_power_on.");
    }

    IR2E71Y_TRACE("out ret=%d", ret);

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_psals_power_off                                                 */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_psals_power_off(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_API_psals_power_off();
    if (ret == IR2E71Y_RESULT_SUCCESS) {
        if (ir2e71y_pm_ctx_recovery.psals_status.power_status == IR2E71Y_SENSOR_STATE_POWER_OFF) {
            ir2e71y_API_psals_recovery_unsubscribe();
        }
    } else {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_pm_psals_power_off.");
    }


    IR2E71Y_TRACE("out ret=%d", ret);

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_psals_init                                                      */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_psals_init(void)
{
    IR2E71Y_TRACE("in");

    IR2E71Y_TRACE("out");

    return IR2E71Y_SENSOR_STATE_POWER_ON;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_psals_deinit                                                    */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_psals_deinit(void)
{
    IR2E71Y_TRACE("in");

    IR2E71Y_TRACE("out");

    return IR2E71Y_SENSOR_STATE_POWER_OFF;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_psals_ps_init_als_off                                           */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_psals_ps_init_als_off(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_API_psals_ps_init_als_off();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_psals_ps_init_als_off.");
    }

    IR2E71Y_TRACE("out");

    return IR2E71Y_SENSOR_STATE_PROX_ON_ALS_OFF;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_psals_ps_init_als_on                                            */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_psals_ps_init_als_on(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_API_psals_ps_init_als_on();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_psals_ps_init_als_on.");
    }

    IR2E71Y_TRACE("out");

    return IR2E71Y_SENSOR_STATE_PROX_ON_ALS_ON;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_psals_ps_deinit_als_off                                         */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_psals_ps_deinit_als_off(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_API_psals_ps_deinit_als_off();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_psals_ps_deinit_als_off.");
    }

    IR2E71Y_TRACE("out");

    return IR2E71Y_SENSOR_STATE_POWER_OFF;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_psals_ps_deinit_als_on                                          */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_psals_ps_deinit_als_on(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_API_psals_ps_deinit_als_on();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_psals_ps_deinit_als_on.");
    }

    IR2E71Y_TRACE("out");

    return IR2E71Y_SENSOR_STATE_PROX_OFF_ALS_ON;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_psals_als_init_ps_off                                           */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_psals_als_init_ps_off(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_API_psals_als_init_ps_off();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_psals_als_init_ps_off.");
    }

    IR2E71Y_TRACE("out");

    return IR2E71Y_SENSOR_STATE_PROX_OFF_ALS_ON;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_psals_als_init_ps_on                                            */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_psals_als_init_ps_on(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_API_psals_als_init_ps_on();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_psals_als_init_ps_on.");
    }

    IR2E71Y_TRACE("out");

    return IR2E71Y_SENSOR_STATE_PROX_ON_ALS_ON;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_psals_als_deinit_ps_off                                         */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_psals_als_deinit_ps_off(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_API_psals_als_deinit_ps_off();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_psals_als_deinit_ps_off.");
    }

    IR2E71Y_TRACE("out");

    return IR2E71Y_SENSOR_STATE_POWER_OFF;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_pm_psals_als_deinit_ps_on                                          */
/* ------------------------------------------------------------------------- */
static int ir2e71y_pm_psals_als_deinit_ps_on(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_API_psals_als_deinit_ps_on();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_psals_als_deinit_ps_on.");
    }

    IR2E71Y_TRACE("out");

    return IR2E71Y_SENSOR_STATE_PROX_ON_ALS_OFF;
}
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */

MODULE_DESCRIPTION("SHARP DISPLAY DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
