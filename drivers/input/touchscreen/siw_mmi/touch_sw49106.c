/*
 * touch_sw49106.c - SiW touch driver glue for SW49106
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/firmware.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <asm/memory.h>

#include "siw_touch.h"
#include "siw_touch_hal.h"
#include "siw_touch_bus.h"

#define CHIP_ID				"4916"
#define CHIP_DEVICE_NAME		"SW49106"
#define CHIP_COMPATIBLE_NAME		"siw,sw49106"
#define CHIP_DEVICE_DESC		"SiW Touch SW49106 Driver"

#define CHIP_TYPE			CHIP_SW49106

#define CHIP_MODE_ALLOWED		(0 |	\
					LCD_MODE_BIT_U0 |	\
					LCD_MODE_BIT_U3 |	\
					LCD_MODE_BIT_STOP |	\
					0)

#define CHIP_FW_SIZE			(80<<10)

#define CHIP_FLAGS			(0 |	\
					TOUCH_SKIP_ESD_EVENT |	\
					0)

#define CHIP_IRQFLAGS			(IRQF_TRIGGER_FALLING | IRQF_ONESHOT)


#define CHIP_INPUT_ID_BUSTYPE		BUS_I2C
#define CHIP_INPUT_ID_VENDOR		0xABCD
#define CHIP_INPUT_ID_PRODUCT		0x9876
#define CHIP_INPUT_ID_VERSION		0x1234

#if defined(CONFIG_ARCH_EXYNOS5)
#define __CHIP_QUIRK_ADD		CHIP_QUIRK_NOT_SUPPORT_XFER
#else
#define __CHIP_QUIRK_ADD		0
#endif

#define CHIP_QUIRKS			(0 |	\
					CHIP_QUIRK_NOT_SUPPORT_ASC |	\
					CHIP_QUIRK_NOT_SUPPORT_WATCH |	\
					CHIP_QUIRK_NOT_SUPPORT_IME |	\
					__CHIP_QUIRK_ADD |	\
					0)

#define CHIP_BUS_TYPE			BUS_IF_I2C
#define	CHIP_BUF_SIZE			0
#define CHIP_SPI_MODE			-1
#define CHIP_BPW			-1
#define CHIP_MAX_FREQ			-1
#define CHIP_TX_HDR_SZ			I2C_BUS_TX_HDR_SZ
#define CHIP_RX_HDR_SZ			I2C_BUS_RX_HDR_SZ
#define CHIP_TX_DUMMY_SZ		I2C_BUS_TX_DUMMY_SZ
#define CHIP_RX_DUMMY_SZ		I2C_BUS_RX_DUMMY_SZ

#define CHIP_SENSELESS_MARGIN		(0x57)

static const struct siw_hal_reg_quirk chip_reg_quirks[] = {
	{ .old_addr = SPR_CHIP_TEST, .new_addr = 0x07E, },
	{ .old_addr = SPR_BOOT_CTL, .new_addr = 0x00E, },
	{ .old_addr = SPR_SRAM_CTL, .new_addr = 0x00F, },
	{ .old_addr = SPR_BOOT_STS, .new_addr = 0x010, },
	{ .old_addr = SPR_SUBDISP_STS, .new_addr = 0x01D, },
	{ .old_addr = SPR_CODE_OFFSET, .new_addr = 0x0AA, },
	{ .old_addr = CODE_ACCESS_ADDR, .new_addr = 0xFD0, },
	{ .old_addr = DATA_I2CBASE_ADDR, .new_addr = 0xFD1, },
	{ .old_addr = PRD_TCM_BASE_ADDR, .new_addr = 0xFD3, },
	{ .old_addr = SERIAL_DATA_OFFSET, .new_addr = 0x0AF, },
	/* */
	{ .old_addr = (1<<31), .new_addr = 0, },
	/* */
	{ .old_addr = TCI_DEBUG_FAIL_CTRL, .new_addr = 0xC70, },
	{ .old_addr = TCI_DEBUG_FAIL_STATUS, .new_addr = 0xC73, },
	{ .old_addr = TCI_DEBUG_FAIL_BUFFER, .new_addr = 0xC74, },
	/* */
	{ .old_addr = SWIPE_FAIL_DEBUG_R, .new_addr = 0xC75, },
	/* */
	{ .old_addr = PRD_SERIAL_TCM_OFFSET, .new_addr = 0x0B0, },
	{ .old_addr = PRD_TC_MEM_SEL, .new_addr = 0x84A, },
	{ .old_addr = PRD_M1_M2_RAW_OFFSET, .new_addr = 0x286, },
	{ .old_addr = ~0, .new_addr = ~0 },
};

#if defined(__SIW_CONFIG_OF)
/*
 * of_device_is_compatible(dev->of_node, CHIP_COMPATIBLE_NAME)
 */
static const struct of_device_id chip_match_ids[] = {
	{ .compatible = CHIP_COMPATIBLE_NAME },
	{ },
};
#else
/* Resolution
 *     <L0W59HRT> [16:9] [18:9]
 * X :	  1080     1080   1080
 * Y :	  2252     1920   2190
 */
enum CHIP_CAPABILITY {
	CHIP_MAX_X			= 1080,
	CHIP_MAX_Y			= 2252,
	CHIP_MAX_PRESSURE	= 255,
	CHIP_MAX_WIDTH		= 15,
	CHIP_MAX_ORI		= 1,
	CHIP_MAX_ID		= 10,
	/* */
	CHIP_HW_RST_DELAY	= 210,
	CHIP_SW_RST_DELAY	= 90,
};

#define CHIP_PIN_RESET			0
#define CHIP_PIN_IRQ			0
#define CHIP_PIN_MAKER			-1
#define CHIP_PIN_VDD			-1
#define CHIP_PIN_VIO			-1

#if (CHIP_PIN_RESET == 0) || (CHIP_PIN_IRQ == 0)
#endif
#endif	/* __SIW_CONFIG_OF */

/* use eg. cname=arc1 to change name */
static char chip_name[32] = CHIP_DEVICE_NAME;
module_param_string(cname, chip_name, sizeof(chip_name), 0);

/* use eg. dname=arc1 to change name */
static char chip_drv_name[32] = SIW_TOUCH_NAME;
module_param_string(dname, chip_drv_name, sizeof(chip_drv_name), 0);

/* use eg. iname=arc1 to change input name */
static char chip_idrv_name[32] = SIW_TOUCH_INPUT;
module_param_string(iname, chip_idrv_name, sizeof(chip_idrv_name), 0);

static const struct siw_touch_pdata chip_pdata = {
	/* Configuration */
	.chip_id		= CHIP_ID,
	.chip_name		= chip_name,
	.drv_name		= chip_drv_name,
	.idrv_name		= chip_idrv_name,
	.owner			= THIS_MODULE,
	.chip_type		= CHIP_TYPE,
	.mode_allowed		= CHIP_MODE_ALLOWED,
	.fw_size		= CHIP_FW_SIZE,
	.flags			= CHIP_FLAGS,
	.irqflags		= CHIP_IRQFLAGS,
	.quirks			= CHIP_QUIRKS,
	/* */
	.bus_info		= {
		.bus_type		= CHIP_BUS_TYPE,
		.buf_size		= CHIP_BUF_SIZE,
		.spi_mode		= CHIP_SPI_MODE,
		.bits_per_word		= CHIP_BPW,
		.max_freq		= CHIP_MAX_FREQ,
		.bus_tx_hdr_size	= CHIP_TX_HDR_SZ,
		.bus_rx_hdr_size	= CHIP_RX_HDR_SZ,
		.bus_tx_dummy_size	= CHIP_TX_DUMMY_SZ,
		.bus_rx_dummy_size	= CHIP_RX_DUMMY_SZ,
	},
#if defined(__SIW_CONFIG_OF)
	.of_match_table			= of_match_ptr(chip_match_ids),
#else
	.pins				= {
		.reset_pin		= CHIP_PIN_RESET,
		.reset_pin_pol		= OF_GPIO_ACTIVE_LOW,
		.irq_pin		= CHIP_PIN_IRQ,
		.maker_id_pin		= CHIP_PIN_MAKER,
		.vdd_pin		= CHIP_PIN_VDD,
		.vio_pin		= CHIP_PIN_VIO,
	},
	.caps				= {
		.max_x			= CHIP_MAX_X,
		.max_y			= CHIP_MAX_Y,
		.max_pressure		= CHIP_MAX_PRESSURE,
		.max_width		= CHIP_MAX_WIDTH,
		.max_orientation	 = CHIP_MAX_ORI,
		.max_id			= CHIP_MAX_ID,
		.hw_reset_delay		= CHIP_HW_RST_DELAY,
		.sw_reset_delay		= CHIP_SW_RST_DELAY,
	},
#endif
	/* Input Device ID */
	.i_id				= {
		.bustype		= CHIP_INPUT_ID_BUSTYPE,
		.vendor			= CHIP_INPUT_ID_VENDOR,
		.product		= CHIP_INPUT_ID_PRODUCT,
		.version		= CHIP_INPUT_ID_VERSION,
	},
	/* */
	.ops				= NULL,
	/* */
	.tci_info			= NULL,
	.tci_qcover_open	= NULL,
	.tci_qcover_close	= NULL,
	.swipe_ctrl			= NULL,
	.watch_win			= NULL,
	.reg_quirks			= (void *)chip_reg_quirks,
	.senseless_margin	= CHIP_SENSELESS_MARGIN,
};

static struct siw_touch_chip_data chip_data = {
	.pdata = &chip_pdata,
	.bus_drv = NULL,
};

siw_chip_module_init(CHIP_DEVICE_NAME,
		     chip_data,
		     CHIP_DEVICE_DESC,
		     "kimhh@siliconworks.co.kr");


__siw_setup_str("siw_chip_name=", siw_setup_chip_name, chip_name);
__siw_setup_str("siw_drv_name=", siw_setup_drv_name, chip_drv_name);
__siw_setup_str("siw_idrv_name=", siw_setup_idrv_name, chip_idrv_name);



