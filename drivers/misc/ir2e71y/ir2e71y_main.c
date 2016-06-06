/* drivers/misc/ir2e71y/ir2e71y_main.c  (Display Driver)
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
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/idr.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/ioctl.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "ir2e71y_define.h"
#include <linux/ir2e71y.h>
#include "ir2e71y_priv.h"

#include "ir2e71y_io.h"
#include "ir2e71y_main.h"
#include "ir2e71y_led.h"
#include "ir2e71y_func.h"
#include "ir2e71y_dbg.h"
#include "ir2e71y_pm.h"
#include "ir2e71y_main_ctrl.h"

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_LD_version_check(unsigned char version);
static int ir2e71y_bdic_LD_hw_init(void);
static int ir2e71y_bdic_PD_set_active(void);
static int ir2e71y_bdic_PD_set_standby(void);

static void ir2e71y_bdic_PD_hw_reset(void);
static int ir2e71y_bdic_PD_shutdown(void);
static int ir2e71y_bdic_PD_set_init(int *bdic_chipver);
static int ir2e71y_bdic_chk_access(void);
static int ir2e71y_bdic_get_chipver(int *bdic_chipver);
static int ir2e71y_bdic_seq_regset(const ir2e71y_bdicRegSetting_t *regtable, int size);
static void ir2e71y_bdic_set_default_sensor_param(struct ir2e71y_photo_sensor_adj *tmp_adj);

static int ir2e71y_bdic_IO_write_reg(unsigned char reg, unsigned char val);
static int ir2e71y_bdic_IO_multi_write_reg(unsigned char reg, unsigned char *wval, unsigned char size);
static int ir2e71y_bdic_IO_read_reg(unsigned char reg, unsigned char *val);
static int ir2e71y_bdic_IO_multi_read_reg(unsigned char reg, unsigned char *val, int size);
static int ir2e71y_bdic_IO_set_bit_reg(unsigned char reg, unsigned char val);
static int ir2e71y_bdic_IO_clr_bit_reg(unsigned char reg, unsigned char val);
static int ir2e71y_bdic_IO_msk_bit_reg(unsigned char reg, unsigned char val, unsigned char msk);
static int ir2e71y_bdic_IO_bank_set(unsigned char val);
#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
static int ir2e71y_bdic_IO_psals_write_reg(unsigned char reg, unsigned char val);
static int ir2e71y_bdic_IO_psals_msk_bit_reg(unsigned char reg, unsigned char val, unsigned char mask);
static int ir2e71y_bdic_IO_psals_read_reg(unsigned char reg, unsigned char *val);
static int ir2e71y_bdic_IO_psals_burst_write_reg(unsigned char *wval, unsigned char dataNum);
static int ir2e71y_bdic_IO_psals_burst_read_reg(unsigned char reg, unsigned char *rval, unsigned char dataNum);

static int ir2e71y_bdic_PD_slave_transfer(struct ir2e71y_bdic_i2c_msg *msg);
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
static int rst_gpio = 0;

#ifdef IR2E71Y_LOG_ENABLE
#if defined(CONFIG_ANDROID_ENGINEERING)
    unsigned short ir2e71y_log_lv = IR2E71Y_LOG_LV_ERR | IR2E71Y_LOG_LV_WARN;
#else  /* CONFIG_ANDROID_ENGINEERING */
    unsigned short ir2e71y_log_lv = IR2E71Y_LOG_LV_ERR;
#endif /* CONFIG_ANDROID_ENGINEERING */
#endif  /* IR2E71Y_LOG_ENABLE */

/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_boot_init                                                 */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_boot_init(int *chipver)
{
    int ret = 0;
    int ret2 = 0;
    int ver = 0;

    IR2E71Y_TRACE("in")

    ret = ir2e71y_bdic_chk_access();
    if (ret == IR2E71Y_BDIC_IS_EXIST) {
        ret2 = ir2e71y_bdic_get_chipver(&ver);
        if (ret2 != IR2E71Y_RESULT_SUCCESS) {
            goto exist_err;
        }
    } else {
        goto exist_err;
    }
    if (chipver != NULL) {
        *chipver = ver;
    }
    IR2E71Y_TRACE("out")
    return IR2E71Y_BDIC_IS_EXIST;

exist_err:
    ir2e71y_IO_API_Host_gpio_free(rst_gpio);
    IR2E71Y_ERR("bdic not exist.");
    return IR2E71Y_BDIC_IS_NOT_EXIST;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_boot_init_2nd                                              */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_boot_init_2nd(void)
{
    IR2E71Y_TRACE("in")
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_init2);
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_init3);
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_init4);

    IR2E71Y_TRACE("out")
    return IR2E71Y_BDIC_IS_EXIST;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_kerl_init                                                 */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_kerl_init(struct platform_device *pdev)
{
    int rc = IR2E71Y_RESULT_SUCCESS;
    struct resource *res;

    IR2E71Y_TRACE("in pdev = 0x%p", pdev);

    if (pdev) {
        if (&(pdev->dev) != NULL) {
            rst_gpio = of_get_named_gpio(pdev->dev.of_node, "bdic_rst_gpio", 0);
            if (!gpio_is_valid(rst_gpio)) {
                IR2E71Y_ERR("rst gpio not specified");
                rc = -1;
            } else {
                IR2E71Y_DEBUG("rst gpio succusess! rst_gpio=%d", rst_gpio);
                ir2e71y_IO_API_Host_gpio_request(rst_gpio, "BL_RST_N");
            }
        } else {
            IR2E71Y_ERR("pdev->dev is NULL");
        }
        res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
        if (!res) {
            IR2E71Y_ERR("irq resouce err!!");
            rc = -1;
        } else {
            ir2e71y_IO_API_set_irq_port(res->start, pdev);
        }
    } else {
        IR2E71Y_ERR("pdev is NULL");
    }
    IR2E71Y_TRACE("out rc = %d", rc);

    return rc;
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_shutdown                                                  */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_shutdown(void)
{
    IR2E71Y_TRACE("in");

    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_shutdown);
    ir2e71y_IO_API_usleep(100);
    ir2e71y_bdic_PD_shutdown();
    ir2e71y_IO_API_usleep(100);

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_set_active                                                */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_set_active(void)
{
    int ret;

    IR2E71Y_TRACE("in");
    ret = ir2e71y_bdic_PD_set_active();
    IR2E71Y_TRACE("out");

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_set_standby                                               */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_set_standby(void)
{
    int ret;

    IR2E71Y_TRACE("in");
    ret = ir2e71y_bdic_PD_set_standby();
    IR2E71Y_TRACE("out");

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_als_sensor_adjust                                         */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_als_sensor_adjust(struct ir2e71y_photo_sensor_adj *adj)
{
    ir2e71y_bdic_als_sensor_adjust[0].data = (unsigned char)(adj->als_adjust[1].als_adj0 & 0x00FF);
    ir2e71y_bdic_als_sensor_adjust[1].data = (unsigned char)(adj->als_adjust[1].als_adj0 >> 8);
    ir2e71y_bdic_als_sensor_adjust[2].data = (unsigned char)(adj->als_adjust[1].als_adj1 & 0x00FF);
    ir2e71y_bdic_als_sensor_adjust[3].data = (unsigned char)(adj->als_adjust[1].als_adj1 >> 8);
    ir2e71y_bdic_als_sensor_adjust[4].data = (adj->als_adjust[1].als_shift & 0x1F);
    ir2e71y_bdic_als_sensor_adjust[5].data = (unsigned char)(adj->als_adjust[1].als_adj0 & 0x00FF);
    ir2e71y_bdic_als_sensor_adjust[6].data = (unsigned char)(adj->als_adjust[1].als_adj0 >> 8);
    ir2e71y_bdic_als_sensor_adjust[7].data = (unsigned char)(adj->als_adjust[1].als_adj1 & 0x00FF);
    ir2e71y_bdic_als_sensor_adjust[8].data = (unsigned char)(adj->als_adjust[1].als_adj1 >> 8);
    ir2e71y_bdic_als_sensor_adjust[9].data = (adj->als_adjust[1].als_shift & 0x1F);
    ir2e71y_bdic_als_sensor_adjust[10].data = (unsigned char)(adj->als_adjust[1].als_adj0 & 0x00FF);
    ir2e71y_bdic_als_sensor_adjust[11].data = (unsigned char)(adj->als_adjust[1].als_adj0 >> 8);
    ir2e71y_bdic_als_sensor_adjust[12].data = (unsigned char)(adj->als_adjust[1].als_adj1 & 0x00FF);
    ir2e71y_bdic_als_sensor_adjust[13].data = (unsigned char)(adj->als_adjust[1].als_adj1 >> 8);
    ir2e71y_bdic_als_sensor_adjust[14].data = (adj->als_adjust[1].als_shift & 0x1F);

    ir2e71y_bdic_als_sensor_adjust[15].data = (unsigned char)(adj->als_adjust[0].als_adj0 & 0x00FF);
    ir2e71y_bdic_als_sensor_adjust[16].data = (unsigned char)(adj->als_adjust[0].als_adj0 >> 8);
    ir2e71y_bdic_als_sensor_adjust[17].data = (unsigned char)(adj->als_adjust[0].als_adj1 & 0x00FF);
    ir2e71y_bdic_als_sensor_adjust[18].data = (unsigned char)(adj->als_adjust[0].als_adj1 >> 8);
    ir2e71y_bdic_als_sensor_adjust[19].data = (adj->als_adjust[0].als_shift & 0x1F);

    ir2e71y_bdic_IO_bank_set(0x01);
    ir2e71y_IO_API_delay_us(10 * 1000);
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_als_sensor_adjust);
    ir2e71y_bdic_IO_bank_set(0x00);

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_check_sensor_param                                        */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_check_sensor_param(struct ir2e71y_photo_sensor_adj *adj_in,
                                        struct ir2e71y_photo_sensor_adj *adj_out)
{
    struct ir2e71y_photo_sensor_adj tmp_adj;
    int err_flg = 0;
    unsigned long chksum;

    IR2E71Y_TRACE("in")
    memcpy(&tmp_adj, adj_in, sizeof(struct ir2e71y_photo_sensor_adj));

    IR2E71Y_INFO(" ---before---");
    IR2E71Y_INFO(" chksum        = 0x%08x", (unsigned int)tmp_adj.chksum);
    IR2E71Y_INFO(" status        = 0x%02x", tmp_adj.status);
    IR2E71Y_INFO(" als_adj0      = 0x%04x", (unsigned short)tmp_adj.als_adjust[0].als_adj0);
    IR2E71Y_INFO(" als_adj1      = 0x%04x", (unsigned short)tmp_adj.als_adjust[0].als_adj1);
    IR2E71Y_INFO(" als_shift     = 0x%02x", tmp_adj.als_adjust[0].als_shift);
    IR2E71Y_INFO(" clear_offset  = 0x%02x", tmp_adj.als_adjust[0].clear_offset);
    IR2E71Y_INFO(" ir_offset     = 0x%02x", tmp_adj.als_adjust[0].ir_offset);
    IR2E71Y_INFO(" als_adj0      = 0x%04x", (unsigned short)tmp_adj.als_adjust[1].als_adj0);
    IR2E71Y_INFO(" als_adj1      = 0x%04x", (unsigned short)tmp_adj.als_adjust[1].als_adj1);
    IR2E71Y_INFO(" als_shift     = 0x%02x", tmp_adj.als_adjust[1].als_shift);
    IR2E71Y_INFO(" clear_offset  = 0x%02x", tmp_adj.als_adjust[1].clear_offset);
    IR2E71Y_INFO(" ir_offset     = 0x%02x", tmp_adj.als_adjust[1].ir_offset);
    IR2E71Y_INFO(" key_backlight = 0x%02x", tmp_adj.key_backlight);

    if (tmp_adj.status != IR2E71Y_ALS_SENSOR_ADJUST_STATUS_COMPLETED) {
        err_flg = 1;
        IR2E71Y_DEBUG(": status check error.");
    } else if (tmp_adj.als_adjust[0].als_shift > 0x1F) {
        err_flg = 2;
        IR2E71Y_ERR(": als_shift check error.");
    } else if (tmp_adj.als_adjust[1].als_shift > 0x1F) {
        err_flg = 3;
        IR2E71Y_ERR(": als_shift check error.");
    } else {
        chksum = (unsigned long)tmp_adj.status
                  + (unsigned long)tmp_adj.key_backlight
                  + tmp_adj.als_adjust[0].als_adj0
                  + tmp_adj.als_adjust[0].als_adj1
                  + (unsigned long)tmp_adj.als_adjust[0].als_shift
                  + (unsigned long)tmp_adj.als_adjust[0].clear_offset
                  + (unsigned long)tmp_adj.als_adjust[0].ir_offset
                  + tmp_adj.als_adjust[1].als_adj0
                  + tmp_adj.als_adjust[1].als_adj1
                  + (unsigned long)tmp_adj.als_adjust[1].als_shift
                  + (unsigned long)tmp_adj.als_adjust[1].clear_offset
                  + (unsigned long)tmp_adj.als_adjust[1].ir_offset;
        if (tmp_adj.chksum != chksum) {
            err_flg = 9;
            IR2E71Y_ERR(": chksum check error.");
            IR2E71Y_ERR(" chksum = 0x%08x", (unsigned int)tmp_adj.chksum);
            IR2E71Y_ERR(" result = 0x%08x", (unsigned int)chksum);
        }
    }
    if (err_flg != 0) {
        ir2e71y_bdic_set_default_sensor_param(&tmp_adj);
        tmp_adj.status = (unsigned char)err_flg;
    }
    memcpy(adj_out, &tmp_adj, sizeof(struct ir2e71y_photo_sensor_adj));

    IR2E71Y_INFO("---after---");
    IR2E71Y_INFO(" chksum        = 0x%08x", (unsigned int)tmp_adj.chksum);
    IR2E71Y_INFO(" status        = 0x%02x", tmp_adj.status);
    IR2E71Y_INFO(" als_adj0      = 0x%04x", (unsigned short)tmp_adj.als_adjust[0].als_adj0);
    IR2E71Y_INFO(" als_adj1      = 0x%04x", (unsigned short)tmp_adj.als_adjust[0].als_adj1);
    IR2E71Y_INFO(" als_shift     = 0x%02x", tmp_adj.als_adjust[0].als_shift);
    IR2E71Y_INFO(" clear_offset  = 0x%02x", tmp_adj.als_adjust[0].clear_offset);
    IR2E71Y_INFO(" ir_offset     = 0x%02x", tmp_adj.als_adjust[0].ir_offset);
    IR2E71Y_INFO(" als_adj0      = 0x%04x", (unsigned short)tmp_adj.als_adjust[1].als_adj0);
    IR2E71Y_INFO(" als_adj1      = 0x%04x", (unsigned short)tmp_adj.als_adjust[1].als_adj1);
    IR2E71Y_INFO(" als_shift     = 0x%02x", tmp_adj.als_adjust[1].als_shift);
    IR2E71Y_INFO(" clear_offset  = 0x%02x", tmp_adj.als_adjust[1].clear_offset);
    IR2E71Y_INFO(" ir_offset     = 0x%02x", tmp_adj.als_adjust[1].ir_offset);
    IR2E71Y_INFO(" key_backlight = 0x%02x", tmp_adj.key_backlight);
    IR2E71Y_TRACE("out")

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_seq_regset                                                */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_seq_regset(const ir2e71y_bdicRegSetting_t *regtable, int size)
{
    int ret = 0;

    ret = ir2e71y_bdic_seq_regset(regtable, size);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IO_write_reg                                              */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IO_write_reg(unsigned char reg, unsigned char val)
{
    int ret = 0;

    ret = ir2e71y_bdic_IO_write_reg(reg,val);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IO_multi_write_reg                                        */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IO_multi_write_reg(unsigned char reg, unsigned char *wval, unsigned char size)
{
    int ret = 0;

    ret = ir2e71y_bdic_IO_multi_write_reg(reg, wval, size);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IO_read_reg                                               */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IO_read_reg(unsigned char reg, unsigned char *val)
{
    int ret = 0;

    ret = ir2e71y_bdic_IO_read_reg(reg, val);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IO_multi_read_reg                                         */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IO_multi_read_reg(unsigned char reg, unsigned char *val, int size)
{
    int ret = 0;

    ret = ir2e71y_bdic_IO_multi_read_reg(reg, val, size);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IO_set_bit_reg                                            */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IO_set_bit_reg(unsigned char reg, unsigned char val)
{
    int ret = 0;

    ret = ir2e71y_bdic_IO_set_bit_reg(reg, val);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IO_clr_bit_reg                                            */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IO_clr_bit_reg(unsigned char reg, unsigned char val)
{
    int ret = 0;

    ret = ir2e71y_bdic_IO_clr_bit_reg(reg, val);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IO_bank_set                                               */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IO_bank_set(unsigned char val)
{
    int ret = 0;

    ret = ir2e71y_bdic_IO_bank_set(val);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IO_msk_bit_reg                                            */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IO_msk_bit_reg(unsigned char reg, unsigned char val, unsigned char msk)
{
    int ret = 0;

    ret = ir2e71y_bdic_IO_msk_bit_reg(reg, val, msk);
    return ret;
}

#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IO_i2c_transfer                                           */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IO_i2c_transfer(struct ir2e71y_bdic_i2c_msg *msg)
{
    int ret = 0;

    ret = ir2e71y_bdic_PD_slave_transfer(msg);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IO_psals_write_reg                                        */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IO_psals_write_reg(unsigned char reg, unsigned char val)
{
    int ret = 0;

    ret = ir2e71y_bdic_IO_psals_write_reg(reg, val);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IO_psals_read_reg                                         */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IO_psals_read_reg(unsigned char reg, unsigned char *val)
{
    int ret = 0;

    ret = ir2e71y_bdic_IO_psals_read_reg(reg, val);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IO_psals_msk_bit_reg                                      */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IO_psals_msk_bit_reg(unsigned char reg, unsigned char val, unsigned char mask)
{
    int ret = 0;

    ret = ir2e71y_bdic_IO_psals_msk_bit_reg(reg, val, mask);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IO_psals_burst_write_reg                                  */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IO_psals_burst_write_reg(unsigned char *wval, unsigned char dataNum)
{
    int ret = 0;

    ret = ir2e71y_bdic_IO_psals_burst_write_reg(wval, dataNum);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IO_psals_burst_read_reg                                   */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IO_psals_burst_read_reg(unsigned char reg, unsigned char *rval, unsigned char dataNum)
{
    int ret = 0;

    ret = ir2e71y_bdic_IO_psals_burst_read_reg(reg, rval, dataNum);
    return ret;
}
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */

/* ------------------------------------------------------------------------- */
/* Logical Driver                                                            */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_chk_access                                                    */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_chk_access(void)
{
    int ret;

    ret = ir2e71y_bdic_LD_hw_init();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        ir2e71y_IO_API_set_Host_gpio(rst_gpio, IR2E71Y_GPIO_CTL_LOW);
        return IR2E71Y_BDIC_IS_NOT_EXIST;
    }
    return IR2E71Y_BDIC_IS_EXIST;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_get_chipver                                                   */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_get_chipver(int *bdic_chipver)
{
    int ret = IR2E71Y_RESULT_SUCCESS;
    ret = ir2e71y_bdic_PD_set_init(bdic_chipver);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_version_check                                              */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_LD_version_check(unsigned char version)
{
    if ((version & IR2E71Y_BDIC_VERSION71) != IR2E71Y_BDIC_VERSION71) {
        return IR2E71Y_RESULT_FAILURE;
    }
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_hw_init                                                    */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_LD_hw_init(void)
{
    int ret;
    unsigned char version;

    ir2e71y_bdic_PD_hw_reset();

    ir2e71y_bdic_IO_bank_set(0x00);
    ret = ir2e71y_IO_API_bdic_i2c_read(BDIC_REG_VERSION, &version);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_FAILURE;
    }

    ret = ir2e71y_bdic_LD_version_check(version);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_FAILURE;
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* Phygical Driver                                                           */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_hw_reset                                                   */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_hw_reset(void)
{
    ir2e71y_IO_API_set_Host_gpio(rst_gpio, IR2E71Y_GPIO_CTL_LOW);
    ir2e71y_IO_API_delay_us(100);
    ir2e71y_IO_API_set_Host_gpio(rst_gpio, IR2E71Y_GPIO_CTL_HIGH);
    ir2e71y_IO_API_delay_us(100);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_shutdown                                                   */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_shutdown(void)
{
    ir2e71y_IO_API_set_Host_gpio(rst_gpio, IR2E71Y_GPIO_CTL_LOW);
    ir2e71y_IO_API_Host_gpio_free(rst_gpio);

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_set_init                                                   */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_set_init(int *bdic_chipver)
{
    int ret;
    unsigned char version = 0;

    IR2E71Y_TRACE("in");

    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_init1);
    ret = ir2e71y_bdic_IO_read_reg(BDIC_REG_VERSION, &version);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_FAILURE;
    }
    if (bdic_chipver != NULL) {
        *bdic_chipver = (int)IR2E71Y_BDIC_GET_CHIPVER(version);
    }
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_init12);

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/*---------------------------------------------------------------------------*/
/*ir2e71y_bdic_seq_regset                                                    */
/*---------------------------------------------------------------------------*/
static int ir2e71y_bdic_seq_regset(const ir2e71y_bdicRegSetting_t *regtable, int size)
{
    int i;
    int ret = IR2E71Y_RESULT_SUCCESS;
    ir2e71y_bdicRegSetting_t *tbl;
    unsigned char top_addr_bdic = 0x00;
    int cnt_bdic = 0;
    unsigned char bBuf_bdic[16];
#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
    int cnt_als = 0;
    unsigned char bBuf_als[16];
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */

    tbl = (ir2e71y_bdicRegSetting_t *)regtable;
    for (i = 0; i < size; i++) {
        if (((cnt_bdic > 0) && (tbl->flg != IR2E71Y_BDIC_STRM)) || (cnt_bdic == sizeof(bBuf_bdic))) {
            ret = ir2e71y_bdic_IO_multi_write_reg(top_addr_bdic, bBuf_bdic, cnt_bdic);
            cnt_bdic = 0;
            top_addr_bdic = 0x00;
        }
#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
        if (((cnt_als > 0) && (tbl->flg != IR2E71Y_ALS_STRM)) || (cnt_als == sizeof(bBuf_als))) {
            ret = ir2e71y_bdic_IO_psals_burst_write_reg(bBuf_als, (unsigned char)cnt_als);
            cnt_als = 0;
        }
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */
        switch (tbl->flg) {
        case IR2E71Y_BDIC_STR:
            ret = ir2e71y_bdic_IO_write_reg(tbl->addr, tbl->data);
            break;
        case IR2E71Y_BDIC_SET:
            ret = ir2e71y_bdic_IO_set_bit_reg(tbl->addr, tbl->data);
            break;
        case IR2E71Y_BDIC_CLR:
            ret = ir2e71y_bdic_IO_clr_bit_reg(tbl->addr, tbl->mask);
            break;
        case IR2E71Y_BDIC_RMW:
            ret = ir2e71y_bdic_IO_msk_bit_reg(tbl->addr, tbl->data, tbl->mask);
            break;
        case IR2E71Y_BDIC_STRM:
            if (cnt_bdic == 0) {
                top_addr_bdic = tbl->addr;
            }
            bBuf_bdic[cnt_bdic] = tbl->data;
            cnt_bdic++;
            if ((i + 1) == size) {
                ret = ir2e71y_bdic_IO_multi_write_reg(top_addr_bdic, bBuf_bdic, cnt_bdic);
                cnt_bdic = 0;
                top_addr_bdic = 0x00;
            }
            break;
        case IR2E71Y_BDIC_BANK:
            ret = ir2e71y_bdic_IO_bank_set(tbl->data);
            break;
        case IR2E71Y_BDIC_WAIT:
            ret = IR2E71Y_RESULT_SUCCESS;
            break;
#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
        case IR2E71Y_ALS_STR:
            ret = ir2e71y_bdic_IO_psals_write_reg(tbl->addr, tbl->data);
            break;
        case IR2E71Y_ALS_STRM:
        case IR2E71Y_ALS_STRMS:
            if (cnt_als == 0) {
                bBuf_als[cnt_als] = tbl->addr;
                cnt_als++;
            }
            bBuf_als[cnt_als] = tbl->data;
            cnt_als++;
            if ((i + 1) == size) {
                ret = ir2e71y_bdic_IO_psals_burst_write_reg(bBuf_als, (unsigned char)cnt_als);
            }
            break;
        case IR2E71Y_ALS_RMW:
            ret = ir2e71y_bdic_IO_psals_msk_bit_reg(tbl->addr, tbl->data, tbl->mask);
            break;
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */
        default:
            break;
        }
        if (ret != IR2E71Y_RESULT_SUCCESS) {
            IR2E71Y_ERR("bdic R/W Error addr=%02X, data=%02X, mask=%02X", tbl->addr, tbl->data, tbl->mask);
            return ret;
        }
        if (tbl->wait > 0) {
            if ((cnt_bdic > 0) && (tbl->flg == IR2E71Y_BDIC_STRM)) {
                ret = ir2e71y_bdic_IO_multi_write_reg(top_addr_bdic, bBuf_bdic, cnt_bdic);
                cnt_bdic = 0;
                top_addr_bdic = 0x00;
            }
#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
            if ((cnt_als > 0) && (tbl->flg == IR2E71Y_ALS_STRM)) {
                ret = ir2e71y_bdic_IO_psals_burst_write_reg(bBuf_als, (unsigned char)cnt_als);
                cnt_als = 0;
            }
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */
            ir2e71y_IO_API_delay_us(tbl->wait);
        }
        tbl++;
    }
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_set_active                                                 */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_set_active(void)
{
    IR2E71Y_TRACE("in");
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_active);
    IR2E71Y_TRACE("out");

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_set_standby                                                */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_set_standby(void)
{
    IR2E71Y_TRACE("in");
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_standby);
    IR2E71Y_TRACE("out");

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_set_default_sensor_param                                      */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_set_default_sensor_param(struct ir2e71y_photo_sensor_adj *tmp_adj)
{
    unsigned short tmp_param1;
    unsigned short tmp_param2;

    tmp_param1 = ((unsigned short)BDIC_REG_ALS_ADJ0_H_DEFAULT_A << 8);
    tmp_param2 = (unsigned short)BDIC_REG_ALS_ADJ0_L_DEFAULT_A;
    tmp_adj->als_adjust[0].als_adj0     = tmp_param1 | tmp_param2;
    tmp_param1 = ((unsigned short)BDIC_REG_ALS_ADJ1_H_DEFAULT_A << 8);
    tmp_param2 = (unsigned short)BDIC_REG_ALS_ADJ1_L_DEFAULT_A;
    tmp_adj->als_adjust[0].als_adj1     = tmp_param1 | tmp_param2;
    tmp_adj->als_adjust[0].als_shift    = BDIC_REG_ALS_SHIFT_DEFAULT_A;
    tmp_adj->als_adjust[0].clear_offset = BDIC_REG_CLEAR_OFFSET_DEFAULT_A;
    tmp_adj->als_adjust[0].ir_offset    = BDIC_REG_IR_OFFSET_DEFAULT_A;

    tmp_param1 = ((unsigned short)BDIC_REG_ALS_ADJ0_H_DEFAULT_B << 8);
    tmp_param2 = (unsigned short)BDIC_REG_ALS_ADJ0_L_DEFAULT_B;
    tmp_adj->als_adjust[1].als_adj0     = tmp_param1 | tmp_param2;
    tmp_param1 = ((unsigned short)BDIC_REG_ALS_ADJ1_H_DEFAULT_B << 8);
    tmp_param2 = (unsigned short)BDIC_REG_ALS_ADJ1_L_DEFAULT_B;
    tmp_adj->als_adjust[1].als_adj1     = tmp_param1 | tmp_param2;
    tmp_adj->als_adjust[1].als_shift    = BDIC_REG_ALS_SHIFT_DEFAULT_B;
    tmp_adj->als_adjust[1].clear_offset = BDIC_REG_CLEAR_OFFSET_DEFAULT_B;
    tmp_adj->als_adjust[1].ir_offset    = BDIC_REG_IR_OFFSET_DEFAULT_B;

    return;
}

/* ------------------------------------------------------------------------- */
/* Input/Output                                                              */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_IO_write_reg                                                  */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_IO_write_reg(unsigned char reg, unsigned char val)
{
    int ret;

#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
    if (ir2e71y_API_get_bdic_is_exist() != IR2E71Y_BDIC_IS_EXIST) {
        IR2E71Y_ERR("<ACCESS_ERR> ");
        return IR2E71Y_RESULT_SUCCESS;
    }
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */

    ret = ir2e71y_IO_API_bdic_i2c_write(reg, val);

    if (ret == IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_SUCCESS;
    } else if (ret == IR2E71Y_RESULT_FAILURE_I2C_TMO) {
        IR2E71Y_ERR("<TIME_OUT> ir2e71y_IO_API_bdic_i2c_write.");
        return IR2E71Y_RESULT_FAILURE_I2C_TMO;
    }

    IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_IO_API_bdic_i2c_write.");
    return IR2E71Y_RESULT_FAILURE;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_IO_multi_write_reg                                            */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_IO_multi_write_reg(unsigned char reg, unsigned char *wval, unsigned char size)
{
    int ret;

#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
    if (ir2e71y_API_get_bdic_is_exist() != IR2E71Y_BDIC_IS_EXIST) {
        IR2E71Y_ERR("<ACCESS_ERR> ");
        return IR2E71Y_RESULT_SUCCESS;
    }
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */

    ret = ir2e71y_IO_API_bdic_i2c_multi_write(reg, wval, size);

    if (ret == IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_SUCCESS;
    } else if (ret == IR2E71Y_RESULT_FAILURE_I2C_TMO) {
        IR2E71Y_ERR("<TIME_OUT> ir2e71y_IO_API_bdic_i2c_multi_write.");
        return IR2E71Y_RESULT_FAILURE_I2C_TMO;
    }

    IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_IO_API_bdic_i2c_multi_write.");
    return IR2E71Y_RESULT_FAILURE;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_IO_read_reg                                                   */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_IO_read_reg(unsigned char reg, unsigned char *val)
{
    int ret;

    if (val == NULL) {
        return IR2E71Y_RESULT_FAILURE;
    }

#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
    if (ir2e71y_API_get_bdic_is_exist() != IR2E71Y_BDIC_IS_EXIST) {
        IR2E71Y_ERR("<ACCESS_ERR> ");
        return IR2E71Y_RESULT_SUCCESS;
    }
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */

    ret = ir2e71y_IO_API_bdic_i2c_read(reg, val);
    if (ret == IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_SUCCESS;
    } else if (ret == IR2E71Y_RESULT_FAILURE_I2C_TMO) {
        return IR2E71Y_RESULT_FAILURE_I2C_TMO;
    }
    return IR2E71Y_RESULT_FAILURE;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_IO_multi_read_reg                                             */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_IO_multi_read_reg(unsigned char reg, unsigned char *val, int size)
{
    int ret;
    int maxreg;

    if (val == NULL) {
        IR2E71Y_ERR("<NULL_POINTER> val.");
        return IR2E71Y_RESULT_FAILURE;
    }

    if ((size < 1) || (size > 8)) {
        IR2E71Y_ERR("<INVALID_VALUE> size(%d).", size);
        return IR2E71Y_RESULT_FAILURE;
    }

    maxreg = (int)reg + (size - 1);
    if (maxreg > BDIC_REG_BANKSEL) {
        IR2E71Y_ERR("<OTHER> register address overflow.");
        return IR2E71Y_RESULT_FAILURE;
    }

    ret = ir2e71y_IO_API_bdic_i2c_multi_read(reg, val, size);

    if (ret == IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_SUCCESS;
    } else if (ret == IR2E71Y_RESULT_FAILURE_I2C_TMO) {
        IR2E71Y_ERR("<TIME_OUT> ir2e71y_IO_API_bdic_i2c_multi_read.");
        return IR2E71Y_RESULT_FAILURE_I2C_TMO;
    }

    IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_IO_API_bdic_i2c_multi_read.");
    return IR2E71Y_RESULT_FAILURE;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_IO_set_bit_reg                                                */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_IO_set_bit_reg(unsigned char reg, unsigned char val)
{
    int ret;

    ret = ir2e71y_IO_API_bdic_i2c_mask_write(reg, val, val);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_IO_clr_bit_reg                                                */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_IO_clr_bit_reg(unsigned char reg, unsigned char val)
{
    int ret;

    ret = ir2e71y_IO_API_bdic_i2c_mask_write(reg, 0x00, val);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_IO_bank_set                                                   */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_IO_bank_set(unsigned char val)
{
    int ret;

    ret = ir2e71y_bdic_IO_write_reg(BDIC_REG_BANKSEL, val);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_IO_msk_bit_reg                                                */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_IO_msk_bit_reg(unsigned char reg, unsigned char val, unsigned char msk)
{
    int ret;

    ret = ir2e71y_IO_API_bdic_i2c_mask_write(reg, val, msk);
    return ret;
}

#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_IO_psals_write_reg                                            */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_IO_psals_write_reg(unsigned char reg, unsigned char val)
{
    int ret = IR2E71Y_RESULT_SUCCESS;
    struct ir2e71y_bdic_i2c_msg msg;
    unsigned char wbuf[2];

    wbuf[0] = reg;
    wbuf[1] = val;

    msg.addr = IR2E71Y_BDIC_SENSOR_SLAVE_ADDR;
    msg.mode = IR2E71Y_BDIC_I2C_M_W;
    msg.wlen = 2;
    msg.rlen = 0;
    msg.wbuf = &wbuf[0];
    msg.rbuf = NULL;

    ret = ir2e71y_bdic_PD_slave_transfer(&msg);

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_IO_psals_read_reg                                             */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_IO_psals_read_reg(unsigned char reg, unsigned char *val)
{
    int ret = IR2E71Y_RESULT_SUCCESS;
    struct ir2e71y_bdic_i2c_msg msg;
    unsigned char wbuf[1];
    unsigned char rbuf[1];

    wbuf[0] = reg;
    rbuf[0] = 0x00;

    msg.addr = IR2E71Y_BDIC_SENSOR_SLAVE_ADDR;
    msg.mode = IR2E71Y_BDIC_I2C_M_R;
    msg.wlen = 1;
    msg.rlen = 1;
    msg.wbuf = &wbuf[0];
    msg.rbuf = &rbuf[0];

    ret = ir2e71y_bdic_PD_slave_transfer(&msg);
    if (ret == IR2E71Y_RESULT_SUCCESS) {
        *val = rbuf[0];
    }

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_IO_psals_msk_bit_reg                                          */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_IO_psals_msk_bit_reg(unsigned char reg, unsigned char val, unsigned char mask)
{
    int ret;
    unsigned char rval;
    unsigned char wval;

    ret = ir2e71y_bdic_IO_psals_read_reg(reg, &rval);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        return ret;
    }

    wval = (rval & ~mask) | (val & mask);

    if (rval == wval) {
        return IR2E71Y_RESULT_SUCCESS;
    }

    ret = ir2e71y_bdic_IO_psals_write_reg(reg, wval);

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_IO_psals_burst_write_reg                                      */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_IO_psals_burst_write_reg(unsigned char *wval, unsigned char dataNum)
{
    int ret = IR2E71Y_RESULT_SUCCESS;
    struct ir2e71y_bdic_i2c_msg msg;

    msg.addr = IR2E71Y_BDIC_SENSOR_SLAVE_ADDR;
    msg.mode = IR2E71Y_BDIC_I2C_M_W;
    msg.wlen = dataNum;
    msg.rlen = 0;
    msg.wbuf = wval;
    msg.rbuf = NULL;
    ret = ir2e71y_bdic_PD_slave_transfer(&msg);

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_IO_psals_burst_read_reg                                       */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_IO_psals_burst_read_reg(unsigned char reg, unsigned char *rval, unsigned char dataNum)
{
    int ret = IR2E71Y_RESULT_SUCCESS;
    struct ir2e71y_bdic_i2c_msg msg;
    unsigned char wbuf[1];

    wbuf[0] = reg;
    msg.addr = IR2E71Y_BDIC_SENSOR_SLAVE_ADDR;
    msg.mode = IR2E71Y_BDIC_I2C_M_R;
    msg.wlen = 1;
    msg.rlen = dataNum;
    msg.wbuf = &wbuf[0];
    msg.rbuf = rval;

    ret = ir2e71y_bdic_PD_slave_transfer(&msg);

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_slave_transfer                                             */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_slave_transfer(struct ir2e71y_bdic_i2c_msg *msg)
{
    int ret;
    switch (msg->mode) {
    case IR2E71Y_BDIC_I2C_M_W:
        ret = ir2e71y_IO_API_Host_i2c_send(msg->addr, (unsigned char *)msg->wbuf, msg->wlen);
        break;
    case IR2E71Y_BDIC_I2C_M_R:
        ret = ir2e71y_IO_API_Host_i2c_recv(msg->addr, (unsigned char *)msg->wbuf, msg->wlen, msg->rbuf, msg->rlen);
        break;
    case IR2E71Y_BDIC_I2C_M_R_MODE1:
    case IR2E71Y_BDIC_I2C_M_R_MODE2:
    case IR2E71Y_BDIC_I2C_M_R_MODE3:
        ret = IR2E71Y_RESULT_SUCCESS;
        break;
    default:
        IR2E71Y_ERR("<INVALID_VALUE> msg->mode(%d).", msg->mode);
        ret = IR2E71Y_RESULT_FAILURE;
        break;
    }
    return ret;
}
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */

MODULE_DESCRIPTION("SHARP DISPLAY DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
