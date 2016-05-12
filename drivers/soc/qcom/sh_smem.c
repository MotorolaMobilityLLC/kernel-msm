/* drivers/soc/qcom/sh_smem.c
 *
 * Copyright (C) 2016 Sharp Corporation
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>

#include <smd_private.h>

#include <soc/qcom/socinfo.h>
#include <soc/qcom/sh_smem.h>

static sharp_smem_common_type *p_sharp_smem_common_type = NULL;
static unsigned long *p_smem_sleep_power_collapse_disabled = NULL;

/*=============================================================================

FUNCTION sh_smem_get_common_address

=============================================================================*/
sharp_smem_common_type *sh_smem_get_common_address( void )
{
    if( of_board_is_sharp_eve() )
    {
        if (p_sharp_smem_common_type == NULL) {
            p_sharp_smem_common_type = smem_alloc(SMEM_ID_VENDOR0, SH_SMEM_COMMON_SIZE,0,0);
        }

        return p_sharp_smem_common_type;
    } else {
        return NULL;
    }
}
EXPORT_SYMBOL(sh_smem_get_common_address);

/*=============================================================================

FUNCTION sh_smem_get_sleep_power_collapse_disabled_address

DESCRIPTION

DEPENDENCIES
  None

RETURN VALUE

SIDE EFFECTS
  None

NOTE
  None

=============================================================================*/
unsigned long *sh_smem_get_sleep_power_collapse_disabled_address( void )
{
    if( of_board_is_sharp_eve() )
    {
        if (p_smem_sleep_power_collapse_disabled == NULL) {
            p_smem_sleep_power_collapse_disabled = smem_alloc(SMEM_SLEEP_POWER_COLLAPSE_DISABLED, sizeof(unsigned long),0,0);
        }

        return p_smem_sleep_power_collapse_disabled;
    } else {
        return NULL;
    }
}
EXPORT_SYMBOL(sh_smem_get_sleep_power_collapse_disabled_address);

