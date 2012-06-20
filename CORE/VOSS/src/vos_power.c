/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

/*!
  @file
  vos_power.c

  @brief
  This is the interface to VOSS power APIs using for power management 
  of the WLAN Libra module from the MSM PMIC. These implementation of 
  these APIs is very target dependent, also these APIs should only be
  used when the WLAN Libra module is powered from the MSM PMIC and not
  from an external independent power source

*/

/*===========================================================================

  Copyright (c) 2008 QUALCOMM Incorporated. All Rights Reserved

  Qualcomm Proprietary

  Export of this technology or software is regulated by the U.S. Government.
  Diversion contrary to U.S. law prohibited.

  All ideas, data and information contained in or disclosed by
  this document are confidential and proprietary information of
  QUALCOMM Incorporated and all rights therein are expressly reserved.
  By accepting this material the recipient agrees that this material
  and the information contained therein are held in confidence and in
  trust and will not be used, copied, reproduced in whole or in part,
  nor its contents revealed in any manner to others without the express
  written permission of QUALCOMM Incorporated.

===========================================================================*/

/*===========================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

$Header: $

when       who     what, where, why
--------   ---     ----------------------------------------------------------

===========================================================================*/

/*===========================================================================

                           INCLUDE FILES

===========================================================================*/
#include <vos_power.h>

#ifdef ANI_BUS_TYPE_SDIO
#include <libra_sdioif.h>
#endif

#ifdef MSM_PLATFORM
#include <mach/mpp.h>
#include <mach/vreg.h>
#include <linux/err.h>
#include <linux/delay.h>

#ifdef MSM_PLATFORM_7x30
#include <mach/irqs-7x30.h>
#include <linux/mfd/pmic8058.h>
#include <mach/rpc_pmapp.h>
#include <mach/pmic.h>
#endif

#ifdef MSM_PLATFORM_8660
#include <qcomwlan_pwrif.h>
#endif

#ifdef MSM_PLATFORM_7x27A
#include <linux/qcomwlan7x27a_pwrif.h>
#endif

#endif //MSM_PLATFORM

#include <vos_sched.h>

//To work around issues of fail to turn WIFI back on after turning it off
#define VOS_PWR_WIFI_ON_OFF_HACK
#ifdef VOS_PWR_WIFI_ON_OFF_HACK
#define VOS_PWR_SLEEP(ms) msleep(ms)
#else
#define VOS_PWR_SLEEP(ms)
#endif

/*===========================================================================

                        DEFINITIONS AND TYPES

===========================================================================*/

#define CHIP_POWER_ON         1
#define CHIP_POWER_OFF        0

// SDIO Config Cycle Clock Frequency
#define WLAN_LOW_SD_CONFIG_CLOCK_FREQ 400000

#ifdef MSM_PLATFORM_7x30

#define PM8058_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio + NR_GPIO_IRQS)

static const char* id = "WLAN";

struct wlan_pm8058_gpio {
    int gpio_num;
    struct pm_gpio gpio_cfg;
};


//PMIC8058 GPIO COnfiguration for MSM7x30 //ON
static struct wlan_pm8058_gpio wlan_gpios_reset[] = {
    {PM8058_GPIO_PM_TO_SYS(22),{PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, 0, PM_GPIO_PULL_NO, 2, PM_GPIO_STRENGTH_LOW, PM_GPIO_FUNC_NORMAL, 0}},
};

//OFF
static struct wlan_pm8058_gpio wlan_gpios_reset_out[] = {
    {PM8058_GPIO_PM_TO_SYS(22),{PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, 1, PM_GPIO_PULL_NO, 2, PM_GPIO_STRENGTH_HIGH, PM_GPIO_FUNC_NORMAL, 0}},
};


/* Routine to power on/off Volans chip */
int vos_chip_power_qrf8600(int on)
{
    struct vreg *vreg_wlan = NULL;
    struct vreg *vreg_gp16 = NULL;
    struct vreg *vreg_gp15 = NULL;
    struct vreg *vreg_s2 = NULL;
    int rc = 0;

VOS_PWR_SLEEP(100);
    //2.9v PA from LDO13
    vreg_wlan = vreg_get(NULL, "wlan");
    if (IS_ERR(vreg_wlan)) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: wlan vreg get failed (%ld)\n",
                             __func__, PTR_ERR(vreg_wlan));
        return PTR_ERR(vreg_wlan);
    }

    //1.2V AON from LDO24
    vreg_gp16 = vreg_get(NULL, "gp16");
    if (IS_ERR(vreg_gp16))  {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: gp16 vreg get failed (%ld)\n",
                             __func__, PTR_ERR(vreg_gp16));
        return PTR_ERR(vreg_gp16);
    }

    //1.2V sw from LDO22
    vreg_gp15 = vreg_get(NULL, "gp15");
    if (IS_ERR(vreg_gp15))  {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: gp15 vreg get failed (%ld)\n",
                             __func__, PTR_ERR(vreg_gp15));
        return PTR_ERR(vreg_gp15);
    }

    //1.3V sw from S2
    vreg_s2 = vreg_get(NULL, "s2");
    if (IS_ERR(vreg_s2)) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: s2 vreg get failed (%ld)\n",
                             __func__, PTR_ERR(vreg_s2));
        return PTR_ERR(vreg_s2);
    }

    if (on) {
        /* Program GPIO 23 to de-assert (drive 1) external_por_n (default 0x00865a05 */
        rc = pm8xxx_gpio_config(wlan_gpios_reset[0].gpio_num, &wlan_gpios_reset[0].gpio_cfg);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: pmic gpio %d config failed (%d)\n",
                            __func__, wlan_gpios_reset[0].gpio_num, rc);
            return -EIO;
        }
        VOS_PWR_SLEEP(300);
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "WLAN put in reset mode \n");
#if 0
        rc = pmapp_clock_vote("wlan", PMAPP_CLOCK_ID_A0, PMAPP_CLOCK_VOTE_ON);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: Voting TCXO to ON failed. (%d)\n",__func__, rc);
            return -EIO;
        }
#endif
        /* Configure TCXO to be slave to WLAN_CLK_PWR_REQ */
        rc = pmapp_clock_vote(id, PMAPP_CLOCK_ID_A0, PMAPP_CLOCK_VOTE_PIN_CTRL);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: Configuring TCXO to Pin controllable failed. (%d)\n",__func__, rc);
            return -EIO;
        }

        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "TCXO configured to be slave to WLAN_CLK_PWR-REQ \n");
        printk(KERN_ERR "TCXO is now slave to clk_pwr_req \n");

        //Wait 10msec
        msleep(10);

        /* Enable L13 to output 2.9V (default 2.9V) */
        rc = vreg_set_level(vreg_wlan, 3050);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: wlan vreg set level failed (%d)\n", __func__, rc);
            return -EIO;
        }

        rc = vreg_enable(vreg_wlan);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: wlan vreg enable failed. (%d)\n",__func__, rc);
            return -EIO;
        }

        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "2.9V Power Supply Enabled \n");
        printk(KERN_ERR "3.05V Supply Enabled \n");

        /* Enable L24 to output 1.2V AON(default 1.3V) */
        rc = vreg_set_level(vreg_gp16, 1200);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: gp16 vreg set level failed (%d)\n", __func__, rc);
            return -EIO;
        }

        rc = vreg_enable(vreg_gp16);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: gp16 vreg enable failed. (%d)\n",__func__, rc);
            return -EIO;
        }
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "1.2V AON Power Supply Enabled \n");
        printk(KERN_ERR "1.2V AON Supply Enabled \n");

        //Wait 300usec
        msleep(1);

        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "1.2V AON Power Supply Enabled \n");

        rc = pm8xxx_gpio_config(wlan_gpios_reset_out[0].gpio_num, &wlan_gpios_reset_out[0].gpio_cfg);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: pmic gpio %d config failed (%d)\n",
                            __func__, wlan_gpios_reset_out[0].gpio_num, rc);
            return -EIO;
        }

        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "WLAN is out of reset now \n");
        printk(KERN_ERR "WLAN is out of reset \n");

        /* Wait 500usec */
        msleep(1);

        /* TODO: Replace the code to enable 1.2V sw and 1.3V sw once we have the API to set these power supplies Pin controllable */

        /* Enable 1.2 switcheable power supply */
        rc = vreg_set_level(vreg_gp15, 1200);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: gp15 vreg set level failed (%d)\n", __func__, rc);
            return -EIO;
        }

#ifdef WLAN_FEATURE_VOS_POWER_VOTED_SUPPLY
        rc = vreg_enable(vreg_gp15);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: gp15 vreg enable failed. (%d)\n",__func__, rc);
            return -EIO;
        }
#else        
        /* 1.2v switchable supply is following the clk_pwr_req signal */
        rc = pmapp_vreg_pincntrl_vote(id, PMAPP_VREG_LDO22, PMAPP_CLOCK_ID_A0, VOS_TRUE);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: gp15 vreg enable failed. (%d)\n",__func__, rc);
            return -EIO;
        }
        vos_sleep(5);
#endif

        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "1.2V sw Power Supply is enabled \n");
        printk(KERN_ERR "1.2V sw is enabled \n");

        /* Enable 1.3 switcheable power supply */
        rc = pmapp_vreg_level_vote(id, PMAPP_VREG_S2, 1300);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: s2 vreg set level failed (%d)\n", __func__, rc);
            return -EIO;
        }
        VOS_PWR_SLEEP(300);

#ifdef WLAN_FEATURE_VOS_POWER_VOTED_SUPPLY
        rc = vreg_enable(vreg_s2);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: s2 vreg enable failed. .(%d)\n",__func__, rc);
            return -EIO;
        }
        msleep(1);
#else        
        /* 1.3v switchable supply is following the clk_pwr_req signal */
        rc = pmapp_vreg_pincntrl_vote(id, PMAPP_VREG_S2, PMAPP_CLOCK_ID_A0, VOS_TRUE);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: s2 vreg enable failed. (%d)\n",__func__, rc);
            return -EIO;
        }
        vos_sleep(5);
#endif

        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "1.3V sw Power Supply is enabled \n");
        printk(KERN_ERR "1.3V sw is enabled \n");

    } else {

        /**
        Assert the external POR to keep the chip in reset state. When the chip is turned
        on the next time, it won't be detected by the bus driver until we deassert it.
        This is to work-around the issue where it fails sometimes when turning WIFI off and on
        though GUI. The theory is that, even though we vote off 1.2V AON, it may still on because
        it is shared by other components. When the next time to turn on WIFI, polling is turned on
        first and when librasdioif.ko is loaded, the card is detected right away before wlan driver loads. 
        The bus driver may have finished configuration of the device. When WLAN driver loads, 
        it resets the device that causes issues when the bus driver tries to assess the chip later.
        This setting draws more power after the driver is unloaded.

        The load sequence is
        1. Enable polling
        2. insmod librasdioif.ko (if card detected, stop polling)
        3. insmod libra.ko (reset the chip)
        4. stop polling
        **/
        /* Program GPIO 23 to de-assert (drive 1) external_por_n to prevent chip detection
           until it is asserted.
        */
        rc = pm8xxx_gpio_config(wlan_gpios_reset[0].gpio_num, &wlan_gpios_reset[0].gpio_cfg);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: pmic gpio %d config failed (%d)\n",
                            __func__, wlan_gpios_reset[0].gpio_num, rc);
            return -EIO;
        }

#ifdef WLAN_FEATURE_VOS_POWER_VOTED_SUPPLY
        /* TODO: Remove the code to disable 1.2V sw and 1.3V sw once we have the API to set these power supplies Pin controllable */
        printk(KERN_ERR "power down switchable\n");
        rc = pmapp_vreg_level_vote(id, PMAPP_VREG_S2, 0);
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: s2 vreg set level failed (%d)\n", __func__, rc);
            return -EIO;
        }
        
        /* 1.3V sw */    
        rc = vreg_disable(vreg_s2); 
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: s2 vreg disable failed (%d)\n", __func__, rc);
            return -EIO;
        }

       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "1.3V sw is disabled \n");

        /* 1.2V sw */
        rc = vreg_disable(vreg_gp15); 
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: gp15 vreg disable failed (%d)\n", __func__, rc);
            return -EIO;
        }

        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "1.2V sw is disabled \n");
#endif //#ifdef WLAN_FEATURE_VOS_POWER_VOTED_SUPPLY

        /* 1.2V AON */
        rc = vreg_disable(vreg_gp16); 
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: gp16 vreg disable failed (%d)\n", __func__, rc);
            return -EIO;
        }

        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "1.2V AON is disabled \n");

#ifdef VOLANS_2_0
        /* Cannot turn off 2.9V due to the PAD issue on Volans */

        /* 2.9V */
        rc = vreg_disable(vreg_wlan); 
        if (rc) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: wlan vreg disable failed (%d)\n", __func__, rc);
            return -EIO;
        }

        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO, "2.9V is disabled \n");
#endif
    }

    return rc;
}
#endif

#ifdef MSM_PLATFORM_7x27_FFA

#define MPP_4_CHIP_PWD_L 3 //MPP4 is hooked to Deep Sleep Signal 

//Helper routine to power up Libra keypad on the 7x27 FFA
int vos_chip_power_7x27_keypad( int on )
{
   struct vreg *vreg_wlan, *vreg_bt = NULL;
   int rc = 0;

   vreg_wlan = vreg_get(NULL, "wlan");
   if (IS_ERR(vreg_wlan)) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: wlan vreg get failed (%ld)",
         __func__, PTR_ERR(vreg_wlan));
      return PTR_ERR(vreg_wlan);
   }

   vreg_bt = vreg_get(NULL, "gp6");
   if (IS_ERR(vreg_bt)) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: gp6 vreg get failed (%ld)",
                __func__, PTR_ERR(vreg_bt));
      return PTR_ERR(vreg_bt);
   }

   if(on) {

      /* units of mV, steps of 50 mV */
      rc = vreg_set_level(vreg_bt, 2600);
      if (rc) {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: vreg set level failed (%d)",__func__, rc);
         return -EIO;
      }
      rc = vreg_enable(vreg_bt);
      if (rc) {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: vreg enable failed (%d)",__func__, rc);
         return -EIO;
      }

      //Set TCXO to 1.8v.
      rc = vreg_set_level(vreg_wlan, 1800);
      if (rc) {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: vreg set level failed (%d)", __func__, rc);
         return -EIO;
      }

      rc = vreg_enable(vreg_wlan);
      if (rc) {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: wlan vreg enable failed (%d)",__func__, rc);
         return -EIO;
      }

      msleep(100);

      // Pull MPP4 high to turn on various supply voltages.
      rc = mpp_config_digital_out(MPP_4_CHIP_PWD_L, 
         MPP_CFG(MPP_DLOGIC_LVL_MSMP, MPP_DLOGIC_OUT_CTRL_HIGH));
      if (rc) {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: MPP_4 pull high failed (%d)",__func__, rc);
         return -EIO;
      }

      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: Enabled power supply for WLAN", __func__);
 
      msleep(500);
   }
   else 
   {

       // Pull MPP4 low to place the chip in reset.
       rc = mpp_config_digital_out(MPP_4_CHIP_PWD_L, 
          MPP_CFG(MPP_DLOGIC_LVL_MSMP, MPP_DLOGIC_OUT_CTRL_LOW));
       if (rc) {
          VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: MPP_4 pull low failed (%d)",__func__, rc);
          return -EIO;
       }

       msleep(100);

      /* Turn off 2.6V */
      rc = vreg_disable(vreg_bt);
      if (rc) {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: vreg disable failed (%d)",__func__, rc);
         return -EIO;
      }

      /* Turn off 1.8V */
      rc = vreg_disable(vreg_wlan);
      if (rc) {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: wlan vreg disable failed (%d)",__func__, rc);
         return -EIO;
      }

      msleep(100);

      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: Disabled power supply for WLAN", __func__);
   }

   return 0;
}
#endif

/*===========================================================================

                    FUNCTION PROTOTYPES

===========================================================================*/

/**
  @brief vos_chipPowerUp() - This API will power up the Libra chip

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  The Libra SDIO core will have been initialized if the operation completes
  successfully

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipPowerUp
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{

#ifdef MSM_PLATFORM_8660
   if(vos_chip_power_qrf8615(CHIP_POWER_ON))
      return VOS_STATUS_E_FAILURE;
#endif

#ifdef MSM_PLATFORM_7x30
   if(vos_chip_power_qrf8600(CHIP_POWER_ON))
      return VOS_STATUS_E_FAILURE;
#endif

#ifdef MSM_PLATFORM_7x27_FFA
   if(vos_chip_power_7x27_keypad(CHIP_POWER_ON))
      return VOS_STATUS_E_FAILURE;
#endif

#ifdef MSM_PLATFORM_7x27A
   if(chip_power_qrf6285(CHIP_POWER_ON))
      return VOS_STATUS_E_FAILURE;
#endif

   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipPowerDown() - This API will power down the Libra chip

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipPowerDown
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{

#ifdef ANI_BUS_TYPE_SDIO
#ifdef MSM_PLATFORM
   struct sdio_func *sdio_func_dev = NULL;

   // Get the SDIO func device
   sdio_func_dev = libra_getsdio_funcdev();
   if (sdio_func_dev == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL, "%s: Libra WLAN sdio device is NULL \n"
             "exiting", __func__);
   }
   else 
   {
      // Set sdio clock to lower config cycle frequency before chip power down.
      // Setting this low freq, will also internally in msm_sdcc disable power save.
      // Once the card is power down and powered up and detected
      // after the config cycles the clock freq will be set back up
      // to our capability of 50MHz
      libra_sdio_set_clock(sdio_func_dev, WLAN_LOW_SD_CONFIG_CLOCK_FREQ);
   }
#endif
#endif

#ifdef MSM_PLATFORM_8660
   if(vos_chip_power_qrf8615(CHIP_POWER_OFF))
      return VOS_STATUS_E_FAILURE;
#endif

#ifdef MSM_PLATFORM_7x30
///#ifndef VOS_PWR_WIFI_ON_OFF_HACK
   if(vos_chip_power_qrf8600(CHIP_POWER_OFF))
      return VOS_STATUS_E_FAILURE;
///#endif
#endif

#ifdef MSM_PLATFORM_7x27_FFA
   if(vos_chip_power_7x27_keypad(CHIP_POWER_OFF))
      return VOS_STATUS_E_FAILURE;
#endif

#ifdef MSM_PLATFORM_7x27A
   if(chip_power_qrf6285(CHIP_POWER_OFF))
      return VOS_STATUS_E_FAILURE;
#endif

   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipReset() - This API will reset the Libra chip

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  A hard reset will involve a powerDown followed by a PowerUp; a soft reset
  can potentially be accomplished by writing to some device registers

  The Libra SDIO core will have been initialized if the operation completes
  successfully

  @param status [out] : whether this operation will complete sync or async
  @param soft [in] : VOS_TRUE if a soft reset is desired 
                     VOS_FALSE for a hard reset i.e. powerDown followed by powerUp
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_NOSUPPORT - soft reset asked for but not supported
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipReset
(
  vos_call_status_type* status,
  v_BOOL_t              soft,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data,
  vos_chip_reset_reason_type   reason
)
{
   VOS_STATUS vstatus;
   vstatus = vos_watchdog_chip_reset(reason);
   return vstatus;
}


/**
  @brief vos_chipVoteOnPASupply() - This API will power up the PA supply

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOnPASupply
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}


/**
  @brief vos_chipVoteOffPASupply() - This API will vote to turn off the 
  PA supply. Even if we succeed in voting, there is a chance PA supply will not 
  be turned off. This will be treated the same as a failure.

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately) could be 
                         because the voting algorithm decided not to power down PA  
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOffPASupply
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}


/**
  @brief vos_chipAssertDeepSleep() - This API will assert the deep 
  sleep signal to Libra

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipAssertDeepSleep
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
#ifdef FIXME_VOLANS
#ifdef MSM_PLATFORM_7x27_FFA
   int rc = mpp_config_digital_out(MPP_4_CHIP_PWD_L, 
      MPP_CFG(MPP_DLOGIC_LVL_MSMP, MPP_DLOGIC_OUT_CTRL_LOW));
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: Failed to pull high MPP_4_CHIP_PWD_L (%d)",
                __func__, rc);
      return VOS_STATUS_E_FAILURE;
   }

#endif

#ifdef MSM_PLATFORM_7x30
   // Configure GPIO 23 for Deep Sleep
   int rc = pm8xxx_gpio_config(wlan_gpios_reset_out[0].gpio_num, &wlan_gpios_reset_out[0].gpio_cfg);
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: pmic GPIO %d config failed (%d)",
         __func__, wlan_gpios_reset_out[0].gpio_num, rc);
      return VOS_STATUS_E_FAILURE;
   }
#endif
#endif //FIXME_VOLANS

   return VOS_STATUS_SUCCESS;
}


/**
  @brief vos_chipDeAssertDeepSleep() - This API will de-assert the deep sleep
  signal to Libra

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipDeAssertDeepSleep
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
#ifdef FIXME_VOLANS
#ifdef MSM_PLATFORM_7x27_FFA
   int rc = mpp_config_digital_out(MPP_4_CHIP_PWD_L, 
      MPP_CFG(MPP_DLOGIC_LVL_MSMP, MPP_DLOGIC_OUT_CTRL_HIGH));
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: Failed to pull high MPP_4_CHIP_PWD_L (%d)",
         __func__, rc);
      return VOS_STATUS_E_FAILURE;
   }


#endif

#ifdef MSM_PLATFORM_7x30
   // Configure GPIO 23 for Deep Sleep
   int rc = pm8xxx_gpio_config(wlan_gpios_reset[2].gpio_num, &wlan_gpios_reset[2].gpio_cfg);
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: pmic GPIO %d config failed (%d)",
                __func__, wlan_gpios_reset[2].gpio_num, rc);
      return VOS_STATUS_E_FAILURE;
   }
#endif
#endif //FIXME_VOLANS
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipExitDeepSleepVREGHandler() - This API will initialize the required VREG
  after exit from deep sleep.

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipExitDeepSleepVREGHandler
(
   vos_call_status_type* status,
   vos_power_cb_type     callback,
   v_PVOID_t             user_data
)
{
#ifdef FIXME_VOLANS
#ifdef MSM_PLATFORM_7x27_FFA
   struct vreg *vreg_wlan;
   int rc;

   vreg_wlan = vreg_get(NULL, "wlan");
   if (IS_ERR(vreg_wlan)) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: wlan vreg get failed (%ld)",
            __func__, PTR_ERR(vreg_wlan));
      return VOS_STATUS_E_FAILURE;
   }

   rc = vreg_set_level(vreg_wlan, 1800);
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: wlan vreg set level failed (%d)",
            __func__, rc);
      return VOS_STATUS_E_FAILURE;
   }

   rc = vreg_enable(vreg_wlan);
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: wlan vreg enable failed (%d)",
            __func__, rc);
      return VOS_STATUS_E_FAILURE;
   }

   msleep(500);

   rc = vreg_set_level(vreg_wlan, 2600);
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: wlan vreg set level failed (%d)",
            __func__, rc);
      return VOS_STATUS_E_FAILURE;
   }

   msleep(500);

   *status = VOS_CALL_SYNC;

#endif

#ifdef MSM_PLATFORM_7x30
   VOS_STATUS vosStatus;
   vos_call_status_type callType;

   vosStatus = vos_chipVoteOnBBAnalogSupply(&callType, NULL, NULL);
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
   msleep(500);

#endif
#endif //FIXME_VOLANS
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipVoteOnRFSupply() - This API will power up the RF supply

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOnRFSupply
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
#ifdef FIXME_VOLANS
#ifdef MSM_PLATFORM_7x30
   int rc;
   struct vreg *vreg_s2 = NULL;
   struct vreg *vreg_s4 = NULL;

   //1.3v RF;
   vreg_s2 = vreg_get(NULL, "s2");
   if (IS_ERR(vreg_s2)) {
      printk(KERN_ERR "%s: s2 vreg get failed (%ld)\n",
         __func__, PTR_ERR(vreg_s2));
      return VOS_STATUS_E_FAILURE;
   }

   //2.2v RF
   vreg_s4 = vreg_get(NULL, "s4");
   if (IS_ERR(vreg_s4)) {
      printk(KERN_ERR "%s: s4 vreg get failed (%ld)\n",
         __func__, PTR_ERR(vreg_s4));
      return VOS_STATUS_E_FAILURE;
   }

   rc = pmapp_vreg_level_vote(id, PMAPP_VREG_S2, 1300);
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL, "%s: s2 vreg vote "
          "level failed (%d)",__func__, rc);
      return VOS_STATUS_E_FAILURE;
   }

   rc = vreg_enable(vreg_s2);
   if (rc) {
      printk(KERN_ERR "%s: s2 vreg enable failed (%d)\n", __func__, rc);
      return VOS_STATUS_E_FAILURE;
   }

   rc = pmapp_vreg_level_vote(id, PMAPP_VREG_S4, 2200);
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL, "%s: s4 vreg vote "
          "level failed (%d)",__func__, rc);
      return VOS_STATUS_E_FAILURE;
   }

   rc = vreg_enable(vreg_s4);
   if (rc) {
      printk(KERN_ERR "%s: s4 vreg enable failed (%d)\n", __func__, rc);
      return VOS_STATUS_E_FAILURE;
   }

   return VOS_STATUS_SUCCESS;

#endif //MSM_PLATFORM_7x30
#endif //FIXME_VOLANS
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipVoteOffRFSupply() - This API will vote to turn off the 
  RF supply. Even if we succeed in voting, there is a chance RF supply will not 
  be turned off as RF rails could be shared with other modules (outside WLAN)

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately) could be 
                         because the voting algorithm decided not to power down PA  
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOffRFSupply
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
#ifdef FIXME_VOLANS
#ifdef MSM_PLATFORM_7x30

   int rc;
   struct vreg *vreg_s2;
   struct vreg *vreg_s4;

   //1.3v RF
   vreg_s2 = vreg_get(NULL, "s2");
   if (IS_ERR(vreg_s2)) {
      printk(KERN_ERR "%s: s2 vreg get failed (%ld)\n",
         __func__, PTR_ERR(vreg_s2));
      return VOS_STATUS_E_FAILURE;
   }

   //2.2v RF
   vreg_s4 = vreg_get(NULL, "s4");
   if (IS_ERR(vreg_s4)) {
      printk(KERN_ERR "%s: s4 vreg get failed (%ld)\n",
         __func__, PTR_ERR(vreg_s4));
      return VOS_STATUS_E_FAILURE;
   }

   rc = pmapp_vreg_level_vote(id, PMAPP_VREG_S2, 0);
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN, "%s: s2 vreg vote "
          "level failed (%d)",__func__, rc);
   }

   rc = vreg_disable(vreg_s2);
   if (rc) {
      printk(KERN_ERR "%s: s2 vreg disable failed (%d)\n", __func__, rc);
   }

   rc = pmapp_vreg_level_vote(id, PMAPP_VREG_S4, 0);
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN, "%s: s4 vreg vote "
          "level failed (%d)",__func__, rc);
   }

   rc = vreg_disable(vreg_s4); 
   if (rc) {
      printk(KERN_ERR "%s: s4 vreg disable failed (%d)\n", __func__, rc);
   }

   return VOS_STATUS_SUCCESS;

#endif //MSM_PLATFORM_7x30
#endif //FIXME_VOLANS
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipVoteOnBBAnalogSupply() - This API will power up the I/P voltage
  used by Base band Analog.

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOnBBAnalogSupply
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
#ifdef FIXME_VOLANS
#ifdef MSM_PLATFORM_7x30
   struct vreg *vreg_wlan2 = NULL;
   int rc;

   //2.5v Analog from LDO19
   vreg_wlan2 = vreg_get(NULL, "wlan2");
   if (IS_ERR(vreg_wlan2)) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL, "%s: wlan2 vreg get "
          "failed (%ld)", __func__, PTR_ERR(vreg_wlan2));
      return VOS_STATUS_E_FAILURE;
   }

   rc = vreg_set_level(vreg_wlan2, 2500);
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL, "%s: wlan2 vreg set "
          "level failed (%d)",__func__, rc);
      return VOS_STATUS_E_FAILURE;
   }

   rc = vreg_enable(vreg_wlan2);
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL, "%s: wlan2 vreg enable "
          "failed (%d)", __func__, rc);
      return VOS_STATUS_E_FAILURE;
   }
#endif
#endif //FIXME_VOLANS
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipVoteOffBBAnalogSupply() - This API will vote off the BB Analog supply.

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately) could be 
                         because the voting algorithm decided not to power down PA  
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOffBBAnalogSupply
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
#ifdef FIXME_VOLANS
#ifdef MSM_PLATFORM_7x30
   struct vreg *vreg_wlan2 = NULL;
   int rc;

   //2.5v Analog from LDO19
   vreg_wlan2 = vreg_get(NULL, "wlan2");
   if (IS_ERR(vreg_wlan2)) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL, "%s: wlan2 vreg get "
          "failed (%ld)", __func__, PTR_ERR(vreg_wlan2));
      return VOS_STATUS_E_FAILURE;
   }

   rc = vreg_disable(vreg_wlan2);
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL, "%s: wlan2 vreg disable "
          "failed (%d)", __func__, rc);
      return VOS_STATUS_E_FAILURE;
   }
#endif
#endif //FIXME_VOLANS
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipVoteOnXOBuffer() - This API will vote to turn on the XO buffer from
  PMIC. This API will be used when Libra uses the TCXO from PMIC on the MSM

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately)       
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOnXOBuffer
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipVoteOffXOBuffer() - This API will vote off PMIC XO buffer.

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately) could be 
                         because the voting algorithm decided not to power down PA  
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteOffXOBuffer
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data
)
{
   return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_chipVoteXOCore - This API will vote PMIC XO Core.

  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with
  @param force_enable [in] : Force enable XO CORE or not

  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately) could be 
                         because the voting algorithm decided not to power down PA  
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteXOCore
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data,
  v_BOOL_t              force_enable
)
{
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
    static v_BOOL_t is_vote_on;

#if defined(MSM_PLATFORM_8660) || defined(MSM_PLATFORM_7x30)
    int rc;
#endif

   /* The expectation is the is_vote_on should always have value 1 or 0.  This funcn should
    * be called alternately with 1 and 0 passed to it.
    */
   if ((force_enable && is_vote_on) || (!force_enable && !is_vote_on)) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "Force XO Core %s called when already %s - directly return success %d", 
         force_enable ? "enable" : "disable", is_vote_on ? "enable" : "disable", 
         is_vote_on);
      goto success;
   }    

#ifdef MSM_PLATFORM_7x30
   rc = pmic_xo_core_force_enable(force_enable);
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
         "%s: pmic_xo_core_force_enable %s failed (%d)",__func__, 
         force_enable ? "enable" : "disable",rc);
      return VOS_STATUS_E_FAILURE;
   }
#endif

#ifdef MSM_PLATFORM_8660
   rc = qcomwlan_pmic_xo_core_force_enable(force_enable);
   if (rc) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
         "%s: pmic_xo_core_force_enable %s failed (%d)",__func__, 
         force_enable ? "enable" : "disable",rc);
      return VOS_STATUS_E_FAILURE;
   }
#endif
    is_vote_on=force_enable;

success:
   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN, "XO CORE ON vote %s successfully!",
                force_enable ? "enable" : "disable");

#endif /* FEATURE_WLAN_NON_INTEGRATED_SOC */

   return VOS_STATUS_SUCCESS;
}


/**
  @brief vos_chipVoteFreqFor1p3VSupply() - This API will vote for frequency for 1.3V RF supply.
  
  This operation may be asynchronous. If so, the supplied callback will
  be invoked when operation is complete with the result. The callback will 
  be called with the user supplied data. If the operation is known to be 
  sync, there is no need to supply a callback and user data.

  EVM issue is observed with 1.6Mhz freq for 1.3V supply in wlan standalone case.
  During concurrent operation (e.g. WLAN and WCDMA) this issue is not observed. 
  To workaround, wlan will vote for 3.2Mhz during startup and will vote for 1.6Mhz
  during exit.
   
  @param status [out] : whether this operation will complete sync or async
  @param callback [in] : user supplied callback invoked when operation completes
  @param user_data [in] : user supplied context callback is called with
  @param freq [in]     :  Frequency for 1.3V Supply for which WLAN driver needs to vote for.
  @return 
  VOS_STATUS_E_INVAL - status is NULL 
  VOS_STATUS_E_FAULT - the operation needs to complete async and a callback 
                       and user_data has not been specified (status will be
                       set to VOS_CALL_ASYNC) 
  VOS_STATUS_E_ALREADY - operation needs to complete async but another request
                         is already in progress (status will be set to VOS_CALL_ASYNC)  
  VOS_STATUS_E_FAILURE - operation failed (status will be set appropriately) could be 
                         because the voting algorithm decided not to power down PA  
  VOS_STATUS_SUCCESS - operation completed successfully if status is SYNC (will be set)
                       OR operation started successfully if status is ASYNC (will be set)

*/
VOS_STATUS vos_chipVoteFreqFor1p3VSupply
(
  vos_call_status_type* status,
  vos_power_cb_type     callback,
  v_PVOID_t             user_data,
  v_U32_t               freq
)
{


#ifdef MSM_PLATFORM_8660
    if(freq == VOS_NV_FREQUENCY_FOR_1_3V_SUPPLY_3P2MH)
        {
            if(qcomwlan_freq_change_1p3v_supply(RPM_VREG_FREQ_3p20))
                return VOS_STATUS_E_FAILURE;
        }
    else
        {
            if(qcomwlan_freq_change_1p3v_supply(RPM_VREG_FREQ_1p60))
                return VOS_STATUS_E_FAILURE;
        }
#endif

    return VOS_STATUS_SUCCESS;
}
