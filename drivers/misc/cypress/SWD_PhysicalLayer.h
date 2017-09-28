/******************************************************************************
 * File Name: SWD_PhysicalLayer.h
 * Version 1.0
 *
 * Description:
 *  This header file contains the macro definitions, function declarations for
 *  the physical layer of the  SWD protocol. These include -
 *    a.) Setting a output pin high or low
 *    b.) Reading the logic level of an input pin
 *    c.) Configuring the drive mode of the programming pin
 *
 *  The pin manipulation routines are defined both as macros and functions.
 *  The macros are used in "Step 1. DeviceAcquire()" as the function has strict
 *  timing requirements for execution. Using the macros instead of fuctions
 *  reduces execution overhead.
 *
 *  The pin manipulation functions are used instead of macros in all other
 *  places to save code space.
 *
 * Note:
 *  The macros, functions defined here are specific to PSoC 5LP Host Programmer.
 *  Modify them as applicable for your Host Programmer.
 *
 *******************************************************************************
 * Copyright (2013), Cypress Semiconductor Corporation.
 *******************************************************************************
 * This software is owned by Cypress Semiconductor Corporation (Cypress) and is
 * protected by and subject to worldwide patent protection (United States and
 * foreign), United States copyright laws and international treaty provisions.
 * Cypress hereby grants to licensee a personal, non-exclusive, non-transferable
 * license to copy, use, modify, create derivative works of, and compile the
 * Cypress Source Code and derivative works for the sole purpose of creating
 * custom software in support of licensee product to be used only in conjunction
 * with a Cypress integrated circuit as specified in the applicable agreement.
 * Any reproduction, modification, translation, compilation, or representation of
 * this software except as specified above is prohibited without the express
 * written permission of Cypress.
 *
 * Disclaimer: CYPRESS MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH
 * REGARD TO THIS MATERIAL, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * Cypress reserves the right to make changes without further notice to the
 * materials described herein. Cypress does not assume any liability arising out
 * of the application or use of any product or circuit described herein. Cypress
 * does not authorize its products for use as critical components in life-support
 * systems where a malfunction or failure may reasonably be expected to result in
 * significant injury to the user. The inclusion of Cypress' product in a life-
 * support systems application implies that the manufacturer assumes all risk of
 * such use and in doing so indemnifies Cypress against all charges. Use may be
 * limited by and subject to the applicable Cypress software license agreement.
 ******************************************************************************/

#ifndef __SWD_PHYSICALLAYER_H
#define __SWD_PHYSICALLAYER_H

/* Host programmer registers, mask values are defined in "RegisterDefines.h" */

#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include "cycapsense_hssp.h"

extern struct cycapsense_ctrl_data *ctrl_data;

#define HSSP_CLK	(ctrl_data->hssp_d.c_gpio)
#define HSSP_DAT	(ctrl_data->hssp_d.d_gpio)
#define HSSP_RST	(ctrl_data->hssp_d.rst_gpio)
#define HSSP_CLK_OFF	(ctrl_data->hssp_d.c_offset)
#define HSSP_DAT_OFF	(ctrl_data->hssp_d.d_offset)
#define HSSP_CLK_GROUP_OFF	(ctrl_data->hssp_d.c_group_offset)
#define HSSP_DAT_GROUP_OFF	(ctrl_data->hssp_d.d_group_offset)
#define MSM_TLMM_BASE	(ctrl_data->hssp_d.gpio_base)

#define GPIO_IN_OUT(n) (MSM_TLMM_BASE + (0x1000 * n))

static inline void hssp_set_clk_gpio(int gpio, int offset, int value)
{
	if (ctrl_data->hssp_d.raw_mode) {
		writel_relaxed_no_log(value ? 2 : 0, (GPIO_IN_OUT(offset) + HSSP_CLK_GROUP_OFF));
	} else
		gpio_set_value(gpio, value);
}
static inline void hssp_set_dat_gpio(int gpio, int offset, int value)
{
	if (ctrl_data->hssp_d.raw_mode) {
		writel_relaxed_no_log(value ? 2 : 0, (GPIO_IN_OUT(offset) + HSSP_DAT_GROUP_OFF));
	} else
		gpio_set_value(gpio, value);
}


static inline int hssp_get_gpio(int gpio, int offset)
{
	if (ctrl_data->hssp_d.raw_mode)
		return !!(readl_relaxed_no_log((GPIO_IN_OUT(offset) + HSSP_DAT_GROUP_OFF)) & 0x1);
	else
		return gpio_get_value(gpio);
}

/***************************** USER ATTENTION REQUIRED ************************
 ***************************** HOST PROCESSOR SPECIFIC *************************
 **************** Macros for Host Pin Drive mode configuration *****************
 *
 * Uses the register definitions, mask values in "RegisterDefines.h" to
 * configure the pin drive mode
 *
 * SWDIO pin on host side - CMOS output (host writes data to target PSoC 4),
 *                          High Z digital input (host reads data from target
 *			   PSoC 4 or when host is not programming target
 *			   PSoC 4 (idle))
 *
 * SWDCK pin on host side - CMOS output (when host is programming target PSoC 4),
 *                          High Z digital input (when host is not programming
 *						   target PSoC 4 (idle))
 *
 * XRES pin on host side -  CMOS output (when host is programming target PSoC 4)
 *                          High Z digital input (when host is not programming
 *						   target PSoC 4 (idle))
 *
 * Modify these as applicable to your Host Programmer
 ******************************************************************************/

#define SWDIO_DRIVEMODE_HIGHZIN	SetSwdioHizInput()
#define SWDIO_DRIVEMODE_CMOSOUT	SetSwdioCmosOutput()

#define SWDCK_DRIVEMODE_HIGHZIN	SetSwdckHizInput()
#define SWDCK_DRIVEMODE_CMOSOUT	SetSwdckCmosOutput()

#define XRES_DRIVEMODE_CMOSOUT	SetXresCmosOutput()
#define XRES_DRIVEMODE_HIGHZIN	SetXresHizInput()

/***************************** USER ATTENTION REQUIRED ************************
 ***************************** HOST PROCESSOR SPECIFIC *************************
 **************** Macros for driving output pins on host side ******************
 *
 * Uses the register definitions, mask values in "RegisterDefines.h" to drive
 * output pins either to logic high (suffixed by 'HIGH') or
 * logic low (suffixed by 'LOW')
 *
 * Modify these as applicable to your Host Programmer
 ******************************************************************************/

#define SWDIO_OUTPUT_HIGH	SetSwdioHigh()
#define SWDIO_OUTPUT_LOW	SetSwdioLow()

#define SWDCK_OUTPUT_HIGH	SetSwdckHigh()
#define SWDCK_OUTPUT_LOW	SetSwdckLow()

#define XRES_OUTPUT_HIGH	SetXresHigh()
#define XRES_OUTPUT_LOW		SetXresLow()

/******************************************************************************
 *   Function Prototypes
 ******************************************************************************/
/* The below fuctions are for pin manipulation, and their definitions in
 *  SWD_PhysicalLayer.c are the same as the corresponding macro defitnions
 *  above. */

#define SetSwdckHigh()		hssp_set_clk_gpio(HSSP_CLK, HSSP_CLK_OFF, 1)
#define SetSwdckLow()		hssp_set_clk_gpio(HSSP_CLK, HSSP_CLK_OFF, 0)

#define SetSwdckCmosOutput()	gpio_direction_output(HSSP_CLK, 0)
#define SetSwdckHizInput()	gpio_direction_input(HSSP_CLK)

#define SetSwdioHigh()		hssp_set_dat_gpio(HSSP_DAT, HSSP_DAT_OFF, 1)
#define SetSwdioLow()		hssp_set_dat_gpio(HSSP_DAT, HSSP_DAT_OFF, 0)

#define SetSwdioCmosOutput()	gpio_direction_output(HSSP_DAT, 0)
#define ReadSwdio()		hssp_get_gpio(HSSP_DAT, HSSP_DAT_OFF)

#define SetSwdioHizInput()	gpio_direction_input(HSSP_DAT)

#define SetXresHigh()		gpio_set_value(HSSP_RST, 1)
#define SetXresLow()		gpio_set_value(HSSP_RST, 0)
#define SetXresCmosOutput()	gpio_direction_output(HSSP_RST, 0)
#define SetXresHizInput()	gpio_direction_input(HSSP_RST)

#endif				/* __SWD_PHYSICALLAYER_H */

/*[] END OF FILE */
