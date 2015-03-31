/* add cypress new driver ttda-02.03.01.476713 */
/* add the log dynamic control */

/*
 * cyttsp4_platform.c
 * Cypress TrueTouch(TM) Standard Product V4 Platform Module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2013 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/delay.h>

/* cyttsp */
#include <linux/cyttsp4_bus.h>
#include <linux/cyttsp4_core.h>

#include "cyttsp4_regs.h"
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_FW_UPGRADE
#include <linux/cyttsp4_img.h>
static struct cyttsp4_touch_firmware cyttsp4_firmware = {
	.img = cyttsp4_img,
	.size = ARRAY_SIZE(cyttsp4_img),
	.ver = cyttsp4_ver,
	.vsize = ARRAY_SIZE(cyttsp4_ver),
};
#else
static struct cyttsp4_touch_firmware cyttsp4_firmware = {
	.img = NULL,
	.size = 0,
	.ver = NULL,
	.vsize = 0,
};
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_TTCONFIG_UPGRADE

#include <linux/Config_Ofilm.h>
static struct touch_settings cyttsp4_sett_ofilm_G750C_param_regs = {
	.data = (uint8_t *) & cyttsp4_ofilm_G750C_param_regs[0],
	.size = ARRAY_SIZE(cyttsp4_ofilm_G750C_param_regs),
	.tag = 0,
};

static struct touch_settings cyttsp4_sett_ofilm_G750C_param_size = {
	.data = (uint8_t *) & cyttsp4_ofilm_G750C_param_size[0],
	.size = ARRAY_SIZE(cyttsp4_ofilm_G750C_param_size),
	.tag = 0,
};

#include <linux/Config_Truly.h>
static struct touch_settings cyttsp4_sett_truly_G750C_param_regs = {
	.data = (uint8_t *) & cyttsp4_truly_G750C_param_regs[0],
	.size = ARRAY_SIZE(cyttsp4_truly_G750C_param_regs),
	.tag = 0,
};

static struct touch_settings cyttsp4_sett_truly_G750C_param_size = {
	.data = (uint8_t *) & cyttsp4_truly_G750C_param_size[0],
	.size = ARRAY_SIZE(cyttsp4_truly_G750C_param_size),
	.tag = 0,
};

static struct cyttsp4_touch_config cyttsp4_ofilm_G750C_ttconfig = {
	.param_regs = &cyttsp4_sett_ofilm_G750C_param_regs,
	.param_size = &cyttsp4_sett_ofilm_G750C_param_size,
	.fw_ver = ofilm_G750C_ttconfig_fw_ver,
	.fw_vsize = ARRAY_SIZE(ofilm_G750C_ttconfig_fw_ver),
};

static struct cyttsp4_touch_config cyttsp4_truly_G750C_ttconfig = {
	.param_regs = &cyttsp4_sett_truly_G750C_param_regs,
	.param_size = &cyttsp4_sett_truly_G750C_param_size,
	.fw_ver = truly_G750C_ttconfig_fw_ver,
	.fw_vsize = ARRAY_SIZE(truly_G750C_ttconfig_fw_ver),
};

/*G6 parameters*/
static struct touch_settings cyttsp4_sett_ofilm_G6_param_regs = {
	.data = (uint8_t *) & cyttsp4_ofilm_G6_param_regs[0],
	.size = ARRAY_SIZE(cyttsp4_ofilm_G6_param_regs),
	.tag = 0,
};

static struct touch_settings cyttsp4_sett_ofilm_G6_param_size = {
	.data = (uint8_t *) & cyttsp4_ofilm_G6_param_size[0],
	.size = ARRAY_SIZE(cyttsp4_ofilm_G6_param_size),
	.tag = 0,
};

static struct touch_settings cyttsp4_sett_truly_G6_param_regs = {
	.data = (uint8_t *) & cyttsp4_truly_G6_param_regs[0],
	.size = ARRAY_SIZE(cyttsp4_truly_G6_param_regs),
	.tag = 0,
};

static struct touch_settings cyttsp4_sett_truly_G6_param_size = {
	.data = (uint8_t *) & cyttsp4_truly_G6_param_size[0],
	.size = ARRAY_SIZE(cyttsp4_truly_G6_param_size),
	.tag = 0,
};

static struct cyttsp4_touch_config cyttsp4_ofilm_G6_ttconfig = {
	.param_regs = &cyttsp4_sett_ofilm_G6_param_regs,
	.param_size = &cyttsp4_sett_ofilm_G6_param_size,
	.fw_ver = ofilm_G6_ttconfig_fw_ver,
	.fw_vsize = ARRAY_SIZE(ofilm_G6_ttconfig_fw_ver),
};

static struct cyttsp4_touch_config cyttsp4_truly_G6_ttconfig = {
	.param_regs = &cyttsp4_sett_truly_G6_param_regs,
	.param_size = &cyttsp4_sett_truly_G6_param_size,
	.fw_ver = truly_G6_ttconfig_fw_ver,
	.fw_vsize = ARRAY_SIZE(truly_G6_ttconfig_fw_ver),
};

/*Mate2 parameters*/
static struct touch_settings cyttsp4_sett_ofilm_Mate2_param_regs = {
	.data = (uint8_t *) & cyttsp4_Mate2_ofilm_param_regs[0],
	.size = ARRAY_SIZE(cyttsp4_Mate2_ofilm_param_regs),
	.tag = 0,
};

static struct touch_settings cyttsp4_sett_ofilm_Mate2_param_size = {
	.data = (uint8_t *) & cyttsp4_Mate2_ofilm_param_size[0],
	.size = ARRAY_SIZE(cyttsp4_Mate2_ofilm_param_size),
	.tag = 0,
};

static struct cyttsp4_touch_config cyttsp4_ofilm_Mate2_ttconfig = {
	.param_regs = &cyttsp4_sett_ofilm_Mate2_param_regs,
	.param_size = &cyttsp4_sett_ofilm_Mate2_param_size,
	.fw_ver = ttconfig_Mate2_ofilm_fw_ver,
	.fw_vsize = ARRAY_SIZE(ttconfig_Mate2_ofilm_fw_ver),
};

static struct cyttsp4_touch_config cyttsp4_ttconfig = {
	.param_regs = NULL,
	.param_size = NULL,
	.fw_ver = NULL,
	.fw_vsize = 0,
};

struct cyttsp4_sett_ttconfig_map cyttsp4_sett_ttconfig_map[] = {
	[0] = {
	       .id = OFILM,
	       .product_family = "G750C",
	       .ttconfig_param = &cyttsp4_ofilm_G750C_ttconfig,
	       },
	[1] = {
	       .id = TRULY,
	       .product_family = "G750C",
	       .ttconfig_param = &cyttsp4_truly_G750C_ttconfig,
	       },
	[2] = {
	       .id = 6,		//just for temp huawei 0829
	       .product_family = "G750C",
	       .ttconfig_param = &cyttsp4_ofilm_G750C_ttconfig,
	       },
	[3] = {
	       .id = 7,		//just for temp huawei 0829
	       .product_family = "G750C",
	       .ttconfig_param = &cyttsp4_truly_G750C_ttconfig,
	       },
	/*MATE2 is using old regular */
	[4] = {
	       .id = 6,
	       .product_family = "Mate2",
	       .ttconfig_param = &cyttsp4_ofilm_Mate2_ttconfig,
	       },
	[5] = {
	       .id = OFILM,
	       .product_family = "G6",
	       .ttconfig_param = &cyttsp4_ofilm_G6_ttconfig,
	       },
	[6] = {
	       .id = TRULY,
	       .product_family = "G6",
	       .ttconfig_param = &cyttsp4_truly_G6_ttconfig,
	       },
	[7] = {
	       .ttconfig_param = NULL,
	       },
};


#else
static struct cyttsp4_touch_config cyttsp4_ttconfig = {
	.param_regs = NULL,
	.param_size = NULL,
	.fw_ver = NULL,
	.fw_vsize = 0,
};

struct cyttsp4_sett_ttconfig_map cyttsp4_sett_ttconfig_map[] = {
	[0] = {
	       .id = 0,
	       .ttconfig_param = NULL,
	       },
};
#endif

struct cyttsp4_loader_platform_data _cyttsp4_loader_platform_data = {
	.fw = &cyttsp4_firmware,
	.ttconfig = &cyttsp4_ttconfig,
	.ttconfig_map = cyttsp4_sett_ttconfig_map,
	.flags = CY_LOADER_FLAG_NONE,
};

int cyttsp4_xres(struct cyttsp4_core_platform_data *pdata, struct device *dev)
{
	int rst_gpio = pdata->rst_gpio;
	int rc = 0;

	gpio_set_value(rst_gpio, 1);
	msleep(20);
	gpio_set_value(rst_gpio, 0);
	msleep(40);
	gpio_set_value(rst_gpio, 1);
	msleep(20);
	tp_log_debug("%s: RESET CYTTSP gpio=%d r=%d\n", __func__,
		     pdata->rst_gpio, rc);
	return rc;
}

int cyttsp4_init(struct cyttsp4_core_platform_data *pdata,
		 int on, struct device *dev)
{
	int rst_gpio = pdata->rst_gpio;
	int irq_gpio = pdata->irq_gpio;
	int rc = 0;

	if (on) {
		rc = gpio_request(rst_gpio, "cyttsp4");
		if (rc < 0) {
			gpio_free(rst_gpio);
			rc = gpio_request(rst_gpio, "cyttsp4");
		}

		if (rc < 0) {
			tp_log_err("%s: Fail request gpio=%d\n", __func__,
				   rst_gpio);
		} else {
			rc = gpio_direction_output(rst_gpio, 1);
			if (rc < 0) {
				tp_log_err("%s: Fail set output gpio=%d\n",
					   __func__, rst_gpio);
				gpio_free(rst_gpio);
			} else {
				/* set irq gpio as no pull */
				//rc = gpio_tlmm_config(GPIO_CFG(irq_gpio,0,GPIO_CFG_INPUT,
				//                    GPIO_CFG_NO_PULL,GPIO_CFG_8MA),GPIO_CFG_ENABLE);
				//if( rc < 0 )
				//{
				//    tp_log_err("%s: Fail set gpio=%d as no pull\n", __func__, irq_gpio);
				//}
				rc = gpio_direction_input(irq_gpio);
			}
		}
	} else {
		gpio_free(rst_gpio);
		gpio_free(irq_gpio);
	}

	tp_log_info("%s: INIT CYTTSP RST gpio=%d and IRQ gpio=%d r=%d\n",
		    __func__, rst_gpio, irq_gpio, rc);
	return rc;
}

static int cyttsp4_wakeup(struct cyttsp4_core_platform_data *pdata,
			  struct device *dev, atomic_t * ignore_irq)
{
	int irq_gpio = pdata->irq_gpio;
	int rc = 0;

	if (ignore_irq)
		atomic_set(ignore_irq, 1);
	rc = gpio_direction_output(irq_gpio, 0);
	if (rc < 0) {
		if (ignore_irq)
			atomic_set(ignore_irq, 0);
		tp_log_err("%s: Fail set output gpio=%d\n", __func__, irq_gpio);
	} else {
		udelay(2000);
		rc = gpio_direction_input(irq_gpio);
		if (ignore_irq)
			atomic_set(ignore_irq, 0);
		if (rc < 0) {
			tp_log_err("%s: Fail set input gpio=%d\n",
				   __func__, irq_gpio);
		}
	}

	tp_log_info("%s: WAKEUP CYTTSP gpio=%d r=%d\n", __func__, irq_gpio, rc);
	return rc;
}

static int cyttsp4_sleep(struct cyttsp4_core_platform_data *pdata,
			 struct device *dev, atomic_t * ignore_irq)
{
	return 0;
}

int cyttsp4_power(struct cyttsp4_core_platform_data *pdata,
		  int on, struct device *dev, atomic_t * ignore_irq)
{
	if (on)
		return cyttsp4_wakeup(pdata, dev, ignore_irq);

	return cyttsp4_sleep(pdata, dev, ignore_irq);
}

int cyttsp4_irq_stat(struct cyttsp4_core_platform_data *pdata,
		     struct device *dev)
{
	return gpio_get_value(pdata->irq_gpio);
}

#ifdef CYTTSP4_DETECT_HW
int cyttsp4_detect(struct cyttsp4_core_platform_data *pdata,
		   struct device *dev, cyttsp4_platform_read read)
{
	int retry = 3;
	int rc;
	char buf[1];

	while (retry--) {
		/* Perform reset, wait for 100 ms and perform read */
		tp_log_debug("%s: Performing a reset\n", __func__);
		pdata->xres(pdata, dev);
		msleep(100);
		rc = read(dev, 0, buf, 1);
		if (!rc)
			return 0;

		tp_log_debug("%s: Read unsuccessful, try=%d\n",
			     __func__, 3 - retry);
	}

	return rc;
}
#endif

