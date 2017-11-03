/*
 * siw_touch_hal.c - SiW touch hal driver
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
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <asm/memory.h>

#include "siw_touch.h"
#include "siw_touch_hal.h"
#include "siw_touch_bus.h"
#include "siw_touch_event.h"
#include "siw_touch_gpio.h"
#include "siw_touch_irq.h"
#include "siw_touch_sys.h"

#ifndef __weak
#define __weak __attribute__((weak))
#endif

enum {
	LPWG_SET_SKIP = -1,
};

struct lpwg_mode_ctrl {
	int clk;
	int qcover;
	int lpwg;
	int lcd;
};

/*
 * weak(dummy) function for ABT control
 * These are deactivated by enabling __SIW_SUPPORT_ABT
 * and the actual functions can be found in siw_touch_hal_abt.c
 */
int __weak siw_hal_abt_init(struct device *dev)
{
	t_dev_warn(dev, "ABT disabled\n");
	return 0;
}
int __weak siw_hal_abt_sysfs(struct device *dev, int on_off)
{
	t_dev_warn(dev, "ABT disabled\n");
	return 0;
}

/*
 * weak(dummy) function for PRD control
 * These are deactivated by enabling __SIW_SUPPORT_PRD
 * and the actual functions can be found in siw_touch_hal_prd.c
 */
int __weak siw_hal_prd_sysfs(struct device *dev, int on_off)
{
	t_dev_warn(dev, "PRD disabled\n");
	return 0;
}

#if defined(__SIW_SUPPORT_WATCH)
#define t_warn_weak_watch(_dev, fmt, args...)	\
		t_dev_warn(_dev, "Watch disabled: "fmt, ##args)
#else
#define t_warn_weak_watch(_dev, fmt, args...)	do { } while (0)
#endif
/*
 * weak(dummy) function for Watch control
 * These are deactivated by enabling __SIW_SUPPORT_WATCH
 * and the actual functions can be found in siw_touch_hal_watch.c
 */
int __weak siw_hal_watch_sysfs(struct device *dev, int on_off)
{
	t_warn_weak_watch(dev, "%s\n", __func__);
	return 0;
}
int __weak siw_hal_watch_init(struct device *dev)
{
	t_warn_weak_watch(dev, "%s\n", __func__);
	return 0;
}
int __weak siw_hal_watch_chk_font_status(struct device *dev)
{
	t_warn_weak_watch(dev, "%s\n", __func__);
	return 0;
}
int __weak siw_hal_watch_get_curr_time(struct device *dev, char *buf, int *len)
{
	t_warn_weak_watch(dev, "%s\n", __func__);
	return 0;
}
int __weak siw_hal_watch_display_off(struct device *dev)
{
	t_warn_weak_watch(dev, "%s\n", __func__);
	return 0;
}
int __weak siw_hal_watch_is_disp_waton(struct device *dev)
{
	t_warn_weak_watch(dev, "%s\n", __func__);
	return 0;
}
int __weak siw_hal_watch_is_rtc_run(struct device *dev)
{
	t_warn_weak_watch(dev, "%s\n", __func__);
	return 0;
}
void __weak siw_hal_watch_set_rtc_run(struct device *dev)
{
	t_warn_weak_watch(dev, "%s\n", __func__);
}
void __weak siw_hal_watch_set_rtc_clear(struct device *dev)
{
	t_warn_weak_watch(dev, "%s\n", __func__);
}
void __weak siw_hal_watch_set_font_empty(struct device *dev)
{
	t_warn_weak_watch(dev, "%s\n", __func__);
}
void __weak siw_hal_watch_set_cfg_blocked(struct device *dev)
{
	t_warn_weak_watch(dev, "%s\n", __func__);
}
void __weak siw_hal_watch_rtc_on(struct device *dev)
{
	t_warn_weak_watch(dev, "%s\n", __func__);
}


static int siw_hal_reset_ctrl(struct device *dev, int ctrl);

static int siw_hal_tc_driving(struct device *dev, int mode);


#define t_hal_bus_info(_dev, fmt, args...)	\
		__t_dev_info(_dev, "hal(bus) : " fmt, ##args)

#define t_hal_bus_err(_dev, fmt, args...)	\
		__t_dev_err(_dev, "hal(bus) : " fmt, ##args)

#define t_hal_bus_warn(_abt, fmt, args...)	\
		__t_dev_warn(_dev, "hal(bus) : " fmt, ##args)

#if defined(__SIW_CONFIG_SWIPE)
#define SWIPE_FAIL_NUM 7
static const char * const siw_hal_swipe_debug_str[SWIPE_FAIL_NUM] = {
	"ERROR",
	"1FINGER_FAST_RELEASE",
	"MULTI_FINGER",
	"FAST_SWIPE",
	"SLOW_SWIPE",
	"OUT_OF_AREA",
	"RATIO_FAIL",
};
#endif	/* __SIW_CONFIG_SWIPE */

static void siw_hal_deep_sleep(struct device *dev);

static int siw_hal_lpwg_mode(struct device *dev);

static void siw_hal_power_init(struct device *dev)
{
	siw_touch_power_init(dev);
}

static void siw_hal_power_free(struct device *dev)
{
	siw_touch_power_free(dev);
}

static void siw_hal_power_vdd(struct device *dev, int value)
{
	siw_touch_power_vdd(dev, value);
}

static void siw_hal_power_vio(struct device *dev, int value)
{
	siw_touch_power_vio(dev, value);
}

#define SIW_HAL_GPIO_RST		"siw_hal_reset"
#define SIW_HAL_GPIO_IRQ		"siw_hal_irq"
#define SIW_HAL_GPIO_MAKER		"siw_hal_maker_id"

static int __siw_hal_gpio_skip_reset(struct siw_ts *ts)
{
	struct device *dev = ts->dev;
	int reset_pin = touch_reset_pin(ts);

	if (touch_flags(ts) & TOUCH_SKIP_RESET_PIN)
		return 1;

	if (!gpio_is_valid(reset_pin)) {
		t_dev_err(dev, "reset_pin invalid, %d\n", reset_pin);
		return 1;
	}

	return 0;
}

static void siw_hal_set_gpio_reset(struct device *dev, int val)
{
	struct siw_ts *ts = to_touch_core(dev);
	int reset_pin = touch_reset_pin(ts);

	if (__siw_hal_gpio_skip_reset(ts))
		return;

	siw_touch_gpio_direction_output(dev,
					reset_pin, !!(val));
	t_dev_dbg_gpio(dev, "set %s(%d) : %d\n",
		       SIW_HAL_GPIO_RST,
		       reset_pin, !!(val));
}

static void siw_hal_init_gpio_reset(struct device *dev)
{
	struct siw_ts *ts = to_touch_core(dev);
	int reset_pin = touch_reset_pin(ts);
	int ret = 0;

	if (__siw_hal_gpio_skip_reset(ts))
		return;

	ret = siw_touch_gpio_init(dev,
				  reset_pin,
				  SIW_HAL_GPIO_RST);
	if (ret)
		return;

	siw_touch_gpio_direction_output(dev,
					reset_pin, GPIO_OUT_ONE);
	t_dev_dbg_gpio(dev, "set %s(%d) as output\n",
		       SIW_HAL_GPIO_RST, reset_pin);

	siw_touch_gpio_set_pull(dev,
				reset_pin, GPIO_PULL_UP);
	t_dev_dbg_gpio(dev, "set %s(%d) as pull-up(%d)\n",
		       SIW_HAL_GPIO_RST,
		       reset_pin, GPIO_NO_PULL);
}

static void siw_hal_trigger_gpio_reset(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);

	siw_hal_set_gpio_reset(dev, GPIO_OUT_ZERO);
	touch_msleep(1 + hal_dbg_delay(chip, HAL_DBG_DLY_HW_RST_0));
	siw_hal_set_gpio_reset(dev, GPIO_OUT_ONE);

	t_dev_info(dev, "trigger gpio reset\n");
}

static void siw_hal_free_gpio_reset(struct device *dev)
{
	struct siw_ts *ts = to_touch_core(dev);
	int reset_pin = touch_reset_pin(ts);

	if (__siw_hal_gpio_skip_reset(ts))
		return;

	siw_touch_gpio_free(dev, reset_pin);
}

static int __siw_hal_gpio_skip_irq(struct siw_ts *ts)
{
	struct device *dev = ts->dev;
	int irq_pin = touch_irq_pin(ts);

	if (!gpio_is_valid(irq_pin)) {
		t_dev_err(dev, "irq_pin inavlid, %d\n", irq_pin);
		return 1;
	}

	return 0;
}

static void siw_hal_init_gpio_irq(struct device *dev)
{
	struct siw_ts *ts = to_touch_core(dev);
	int irq_pin = touch_irq_pin(ts);
	int ret = 0;

	if (__siw_hal_gpio_skip_irq(ts))
		return;

	ret = siw_touch_gpio_init(dev,
				  irq_pin,
				  SIW_HAL_GPIO_IRQ);
	if (ret)
		return;

	siw_touch_gpio_direction_input(dev,
				       irq_pin);
	t_dev_dbg_gpio(dev, "set %s(%d) as input\n",
		       SIW_HAL_GPIO_IRQ,
		       irq_pin);

	siw_touch_gpio_set_pull(dev,
				irq_pin, GPIO_PULL_UP);
	t_dev_dbg_gpio(dev, "set %s(%d) as pull-up(%d)\n",
		       SIW_HAL_GPIO_IRQ,
		       irq_pin, GPIO_PULL_UP);
}

static void siw_hal_free_gpio_irq(struct device *dev)
{
	struct siw_ts *ts = to_touch_core(dev);
	int irq_pin = touch_irq_pin(ts);

	if (__siw_hal_gpio_skip_irq(ts))
		return;

	siw_touch_gpio_free(dev, irq_pin);
}

static void siw_hal_init_gpio_maker_id(struct device *dev)
{
}

static void siw_hal_free_gpio_maker_id(struct device *dev)
{
}

static void siw_hal_init_gpios(struct device *dev)
{
	siw_hal_init_gpio_reset(dev);

	siw_hal_init_gpio_irq(dev);

	siw_hal_init_gpio_maker_id(dev);

	siw_hal_trigger_gpio_reset(dev);
}

static void siw_hal_free_gpios(struct device *dev)
{
	siw_hal_free_gpio_reset(dev);

	siw_hal_free_gpio_irq(dev);

	siw_hal_free_gpio_maker_id(dev);
}

u32 t_bus_dbg_mask;

/* usage
 * (1) echo <value> > /sys/module/{Module Name}/parameters/bus_dbg_mask
 * (2) insmod {Siw Touch Module Name}.ko bus_dbg_mask=<value>
 */
module_param_named(bus_dbg_mask, t_bus_dbg_mask, uint,
		   S_IRUGO | S_IWUSR | S_IWGRP);

#define t_bus_info(_dev, fmt, args...)	__t_dev_info(_dev, "bus: " fmt, ##args)
#define t_bus_warn(_dev, fmt, args...)	__t_dev_warn(_dev, "bus: " fmt, ##args)

#define t_bus_err(_dev, fmt, args...)	__t_dev_err(_dev, "bus: " fmt, ##args)

#define t_bus_dbg(condition, _dev, fmt, args...)			\
		do {							\
			if (unlikely(t_bus_dbg_mask & (condition)))	\
				__t_dev_info(_dev, "bus: " fmt, ##args);\
		} while (0)

#define t_bus_dbg_base(_dev, fmt, args...)	\
		t_bus_dbg(DBG_BASE, _dev, fmt, ##args)

#define t_bus_dbg_trace(_dev, fmt, args...)	\
		t_bus_dbg(DBG_TRACE, _dev, fmt, ##args)

static inline void __siw_hal_bus_dbg(struct device *dev, u32 addr, u8 *buf,
				     int size,
				     int wr, int xfer)
{
	if (!wr && !xfer &&
	    (size == sizeof(struct siw_hal_touch_info))) {
		t_bus_dbg_trace(dev,
	"%s(%s) 0x%04X, 0x%04X: %02X %02X %02X %02X %02X %02X %02X %02X%s\n",
				(wr) ? "wr" : "rd",
				(xfer) ? "x" : "s",
				(u32)addr, (u32)size,
				buf[0], buf[1], buf[2], buf[3],
				buf[4], buf[5], buf[6], buf[7],
				(size > 8) ? " ..." : "");
		return;
	}

	if (size > 4) {
		t_bus_dbg_base(dev,
	"%s(%s) 0x%04X, 0x%04X: %02X %02X %02X %02X %02X %02X %02X %02X%s\n",
			       (wr) ? "wr" : "rd",
			       (xfer) ? "x" : "s",
			       (u32)addr, (u32)size,
			       buf[0], buf[1], buf[2], buf[3],
			       buf[4], buf[5], buf[6], buf[7],
			       (size > 8) ? " ..." : "");
	} else {
		t_bus_dbg_base(dev,
			"%s(%s) 0x%04X, 0x%04X: %02X %02X %02X %02X\n",
			       (wr) ? "wr" : "rd",
			       (xfer) ? "x" : "s",
			       (u32)addr, (u32)size,
			       buf[0], buf[1], buf[2], buf[3]);
	}
}

static void *__siw_hal_get_curr_buf(struct siw_ts *ts, dma_addr_t *dma,
					int tx)
{
	struct siw_touch_buf *t_buf;
	int *idx;
	void *buf = NULL;

	idx = (tx) ? &ts->tx_buf_idx : &ts->rx_buf_idx;
	t_buf = (tx) ? &ts->tx_buf[(*idx)] : &ts->rx_buf[(*idx)];

	buf = t_buf->buf;
	if (dma)
		*dma = t_buf->dma;

	(*idx)++;
	(*idx) %= SIW_TOUCH_MAX_BUF_IDX;

	return buf;
}

static int __used __siw_hal_do_reg_read(struct device *dev, u32 addr,
					void *data, int size)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int bus_tx_hdr_size = touch_tx_hdr_size(ts);
	int bus_rx_hdr_size = touch_rx_hdr_size(ts);
	int bus_rx_dummy_size = (touch_rx_dummy_size(ts) & 0xFFFF);
	int bus_rx_dummy_flag = (touch_rx_dummy_size(ts) >> 16);
	int bus_rd_hdr_flag = 0;
	struct touch_bus_msg _msg = {0, };
	struct touch_bus_msg *msg = &_msg;
	int tx_size = bus_tx_hdr_size;
	u8 *tx_buf;
	u8 *rx_buf;
	dma_addr_t tx_dma;
	dma_addr_t rx_dma;
	int ret = 0;

	if (!data) {
		t_dev_err(dev, "NULL data(0x%04X, 0x%04X)\n", addr, size);
		return -EFAULT;
	}

#if defined(__SIW_I2C_TYPE_1)
	if (touch_bus_type(ts) == BUS_IF_I2C) {
		struct siw_hal_reg *reg = chip->reg;

		/*
		 * If 0x200 and burst, change to 0x201
		 * If 0x201, change to 0x202
		 */
		if (addr == reg->tc_ic_status)
			addr += !!(size > 4);
		else
			addr += !!(addr == reg->tc_status);
	}
#endif

#if defined(__SIW_SPI_TYPE_1)
	/*
	 * 0x10 : 128-bit dummy
	 * size > 4 : burst
	 */
	if (bus_rx_dummy_flag & SPI_BUS_RX_DUMMY_FLAG_128BIT) {
		/* Burst restriction under 128-bit dummy */
		if (size > 4) {
			switch (addr & 0xF00) {
			case 0xC00:
			case 0xD00:
			case 0xE00:
				break;
			default:
				bus_rx_dummy_flag &=
					~SPI_BUS_RX_DUMMY_FLAG_128BIT;
				bus_rx_hdr_size = SPI_BUS_RX_HDR_SZ_32BIT;
				bus_rx_dummy_size = SPI_BUS_RX_DUMMY_SZ_32BIT;
				break;
			}
		}
	}
	if (bus_rx_dummy_flag & SPI_BUS_RX_DUMMY_FLAG_128BIT)
		bus_rd_hdr_flag |= SPI_BUS_RX_DUMMY_FLAG_128BIT;
#else
	bus_rx_dummy_flag = 0;
#endif

	tx_buf = __siw_hal_get_curr_buf(ts, &tx_dma, 1);
	rx_buf = __siw_hal_get_curr_buf(ts, &rx_dma, 0);

#if defined(__SIW_CONFIG_CLR_RX_BUFFER)
	if (touch_bus_type(ts) == BUS_IF_I2C)
		memset(&rx_buf[bus_rx_hdr_size], 0xFF, min(8, size));
#endif

#if defined(__SIW_BUS_ADDR_16BIT)
	tx_buf[0] = ((addr >> 8) & 0xff);
	tx_buf[1] = (addr & 0xff);
	tx_buf[2] = bus_rd_hdr_flag;	/* just prevent 'build warning' */
#else
	tx_buf[0] = bus_rd_hdr_flag | ((size > 4) ? 0x20 : 0x00);
	tx_buf[0] |= ((addr >> 8) & 0x0f);
	tx_buf[1] = (addr & 0xff);
#endif
	while (bus_rx_dummy_size--)
		tx_buf[tx_size++] = 0;

	msg->tx_buf = tx_buf;
	msg->tx_size = tx_size;
	msg->rx_buf = rx_buf;
	msg->rx_size = bus_rx_hdr_size + size;
	msg->tx_dma = tx_dma;
	msg->rx_dma = rx_dma;
	msg->bits_per_word = 8;
	msg->priv = 0;

	ret = siw_touch_bus_read(dev, msg);
	if (ret < 0) {
		t_dev_err(dev, "touch bus read error(0x%04X, 0x%04X), %d\n",
			  (u32)addr, (u32)size, ret);
		return ret;
	}

	memcpy(data, &rx_buf[bus_rx_hdr_size], size);

	__siw_hal_bus_dbg(dev, addr, (u8 *)data, size, 0, 0);

	return size;
}

static int __used __siw_hal_reg_read(struct device *dev, u32 addr,
	void *data, int size)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	int ret = 0;

	mutex_lock(&chip->bus_lock);
	ret = __siw_hal_do_reg_read(dev, addr, data, size);
	mutex_unlock(&chip->bus_lock);

	return ret;
}

static int __used __siw_hal_do_reg_write(struct device *dev, u32 addr,
		void *data, int size)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int bus_tx_hdr_size = touch_tx_hdr_size(ts);
	struct touch_bus_msg _msg = {0, };
	struct touch_bus_msg *msg = &_msg;
	u8 *tx_buf;
	dma_addr_t tx_dma;
	int ret = 0;

	if (!data) {
		t_dev_err(dev, "NULL data(0x%04X, 0x%04X)\n", addr, size);
		return -EFAULT;
	}

	tx_buf = __siw_hal_get_curr_buf(ts, &tx_dma, 1);

#if defined(__SIW_BUS_ADDR_16BIT)
	tx_buf[0] = ((addr >> 8) & 0xff);
	tx_buf[1] = (addr & 0xff);
#else
	tx_buf[0] = (touch_bus_type(ts) == BUS_IF_SPI) ? 0x60 :
		    ((size > 4) ? 0x60 : 0x40);
	tx_buf[0] |= ((addr >> 8) & 0x0f);
	tx_buf[1] = (addr & 0xff);
#endif

	msg->tx_buf = tx_buf;
	msg->tx_size = bus_tx_hdr_size + size;
	msg->rx_buf = NULL;
	msg->rx_size = 0;
	msg->tx_dma = tx_dma;
	msg->rx_dma = 0;
	msg->bits_per_word = 8;
	msg->priv = 0;

	memcpy(&tx_buf[bus_tx_hdr_size], data, size);

	ret = siw_touch_bus_write(dev, msg);
	if (ret < 0) {
		t_dev_err(dev, "touch bus write error(0x%04X, 0x%04X), %d\n",
			  (u32)addr, (u32)size, ret);
		return ret;
	}

	__siw_hal_bus_dbg(dev, addr, (u8 *)data, size, 1, 0);

	return size;
}

static int __used __siw_hal_reg_write(struct device *dev, u32 addr,
						void *data, int size)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	int ret = 0;

	mutex_lock(&chip->bus_lock);
	ret = __siw_hal_do_reg_write(dev, addr, data, size);
	mutex_unlock(&chip->bus_lock);

	return ret;
}

static void __used __siw_hal_do_xfer_dbg(struct device *dev,
		struct touch_xfer_msg *xfer)
{
	struct touch_xfer_data_t *tx = NULL;
	struct touch_xfer_data_t *rx = NULL;
	int i;

	for (i = 0; i < xfer->msg_count; i++) {
		tx = &xfer->data[i].tx;
		rx = &xfer->data[i].rx;

		t_dev_err(dev, "[%d] rx(0x%04X, 0x%04X) tx(0x%04X, 0x%04X)\n",
			  i,
			  (u32)rx->addr, (u32)rx->size,
			  (u32)tx->addr, (u32)tx->size);
	}
}

static int __used __siw_hal_do_xfer_to_single(struct device *dev,
		struct touch_xfer_msg *xfer)
{
	struct touch_xfer_data_t *tx = NULL;
	struct touch_xfer_data_t *rx = NULL;
	int i = 0;
	int ret = 0;

	for (i = 0; i < xfer->msg_count; i++) {
		tx = &xfer->data[i].tx;
		rx = &xfer->data[i].rx;

		if (rx->size) {
			ret = __siw_hal_do_reg_read(dev, rx->addr, rx->buf,
				rx->size);
			t_dev_dbg_trace(dev,
				"xfer single [%d/%d] - rd(%04Xh, %d), %d\n",
				i, xfer->msg_count, rx->addr, rx->size, ret);
			if (ret < 0)
				return ret;
		} else if (tx->size) {
			ret = __siw_hal_do_reg_write(dev, tx->addr, tx->buf,
				tx->size);
			t_dev_dbg_trace(dev,
				"xfer single [%d/%d] - wr(%04Xh, %d), %d\n",
				i, xfer->msg_count, rx->addr, rx->size, ret);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int __used __siw_hal_do_xfer_msg(struct device *dev,
					struct touch_xfer_msg *xfer)
{
	struct siw_ts *ts = to_touch_core(dev);
	struct touch_xfer_data_t *tx = NULL;
	struct touch_xfer_data_t *rx = NULL;
	int bus_tx_hdr_size = touch_tx_hdr_size(ts);
	int bus_rx_hdr_size = touch_rx_hdr_size(ts);
	int bus_rx_dummy_size = (touch_rx_dummy_size(ts) & 0xFFFF);
	int bus_rx_dummy_flag = (touch_rx_dummy_size(ts) >> 16);
	int bus_dummy;
	int buf_size = touch_get_act_buf_size(ts);
	int tx_size;
	int i = 0;
	int ret = 0;

	if (!touch_xfer_allowed(ts))
		return __siw_hal_do_xfer_to_single(dev, xfer);

	t_dev_dbg_base(dev, "xfer: start\n");

#if defined(__SIW_SPI_TYPE_1)
	/*
	 * 0x10 : 128-bit dummy
	 * size > 4 : burst
	 */
	if (bus_rx_dummy_flag & SPI_BUS_RX_DUMMY_FLAG_128BIT) {
		bus_rx_hdr_size = SPI_BUS_RX_HDR_SZ_32BIT;
		bus_rx_dummy_size = SPI_BUS_RX_DUMMY_SZ_32BIT;
	}
#else
	bus_rx_dummy_flag = 0;
#endif

	for (i = 0; i < xfer->msg_count; i++) {
		tx = &xfer->data[i].tx;
		rx = &xfer->data[i].rx;

		if (rx->size) {
			t_dev_dbg_base(dev, "xfer: rd set(%d)\n", i);
			tx_size = bus_tx_hdr_size;
			bus_dummy = bus_rx_dummy_size;

			tx->data[0] = (rx->size > 4) ? 0x20 : 0x00;
			tx->data[0] |= ((rx->addr >> 8) & 0x0f);
			tx->data[1] = (rx->addr & 0xff);
			while (bus_dummy--)
				tx->data[tx_size++] = 0;
			tx->size = tx_size;
			rx->size += bus_rx_hdr_size;
			continue;
		}

		t_dev_dbg_base(dev, "xfer: wr set(%d)\n", i);

		if (tx->size > (buf_size - bus_tx_hdr_size)) {
			t_dev_err(dev, "buffer overflow\n");
			return -EOVERFLOW;
		}

		tx->data[0] = 0x60;
		tx->data[0] |= ((tx->addr >> 8) & 0x0f);
		tx->data[1] = (tx->addr  & 0xff);
		memcpy(&tx->data[bus_tx_hdr_size], tx->buf, tx->size);
		tx->size += bus_tx_hdr_size;
	}

	t_dev_dbg_base(dev, "xfer: call bus xfer\n");

	ret = siw_touch_bus_xfer(dev, xfer);
	if (ret < 0) {
		t_dev_err(dev, "touch bus xfer error, %d\n", ret);
		__siw_hal_do_xfer_dbg(dev, xfer);
		return ret;
	}

	ret = 0;
	for (i = 0; i < xfer->msg_count; i++) {
		tx = &xfer->data[i].tx;
		rx = &xfer->data[i].rx;

		if (rx->size) {
			if (!rx->buf) {
				t_dev_err(dev, "NULL xfer->data[%d].rx.buf\n",
					i);
				return -EFAULT;
			}
			memcpy(rx->buf, rx->data + bus_rx_hdr_size,
			       (rx->size - bus_rx_hdr_size));
		}
		ret += rx->size;

		{
			struct touch_xfer_data_t *dbg =
				(rx->size) ? rx : tx;
			int dbg_hdr_size =
				(rx->size) ? bus_rx_hdr_size : bus_tx_hdr_size;

			__siw_hal_bus_dbg(dev, dbg->addr, dbg->buf,
				dbg->size - dbg_hdr_size, !(rx->size), 1);
		}
	}

	return ret;
}

int siw_hal_read_value(struct device *dev, u32 addr, u32 *value)
{
	int ret = __siw_hal_reg_read(dev, addr, value, sizeof(u32));

	if (ret < 0)
		t_hal_bus_err(dev, "read val err[%03Xh, 0x%X], %d",
			      addr, *value, ret);
	return ret;
}

int siw_hal_write_value(struct device *dev, u32 addr, u32 value)
{
	int ret = __siw_hal_reg_write(dev, addr, &value, sizeof(u32));

	if (ret < 0)
		t_hal_bus_err(dev, "write val err[%03Xh, 0x%X], %d",
			      addr, value, ret);
	return ret;
}

int siw_hal_reg_read(struct device *dev, u32 addr, void *data, int size)
{
	int ret = __siw_hal_reg_read(dev, addr, data, size);

	if (ret < 0)
		t_hal_bus_err(dev, "read reg err[%03Xh, 0x%X], %d",
			      addr, ((u32 *)data)[0], ret);
	return ret;
}

int siw_hal_reg_write(struct device *dev, u32 addr, void *data, int size)
{
	int ret = __siw_hal_reg_write(dev, addr, data, size);

	if (ret < 0)
		t_hal_bus_err(dev, "write reg err[%03Xh, 0x%X], %d",
			      addr, ((u32 *)data)[0], ret);
	return ret;
}

void siw_hal_xfer_init(struct device *dev, void *xfer_data)
{
	struct touch_xfer_msg *xfer = xfer_data;
	struct siw_touch_chip *chip = to_touch_chip(dev);

	mutex_lock(&chip->bus_lock);
	xfer->bits_per_word = 8;
	xfer->msg_count = 0;
}

int siw_hal_xfer_msg(struct device *dev, struct touch_xfer_msg *xfer)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	int ret = 0;

	ret = __siw_hal_do_xfer_msg(dev, xfer);
	mutex_unlock(&chip->bus_lock);

	return ret;
}

void siw_hal_xfer_add_rx(void *xfer_data, u32 reg, void *buf, u32 size)
{
	struct touch_xfer_msg *xfer = xfer_data;
	struct touch_xfer_data_t *rx = &xfer->data[xfer->msg_count].rx;
	struct touch_xfer_data_t *tx = &xfer->data[xfer->msg_count].tx;

	if (xfer->msg_count >= SIW_TOUCH_MAX_XFER_COUNT) {
		t_pr_err("touch xfer msg_count overflow (rx)\n");
		return;
	}

	rx->addr = reg;
	rx->buf = buf;
	rx->size = size;

	tx->addr = 0;
	tx->buf = NULL;
	tx->size = 0;

	xfer->msg_count++;
}

void siw_hal_xfer_add_tx(void *xfer_data, u32 reg, void *buf, u32 size)
{
	struct touch_xfer_msg *xfer = xfer_data;
	struct touch_xfer_data_t *rx = &xfer->data[xfer->msg_count].rx;
	struct touch_xfer_data_t *tx = &xfer->data[xfer->msg_count].tx;

	if (xfer->msg_count >= SIW_TOUCH_MAX_XFER_COUNT) {
		t_pr_err("touch xfer msg_count overflow (tx)\n");
		return;
	}

	rx->addr = 0;
	rx->buf = NULL;
	rx->size = 0;

	tx->addr = reg;
	tx->buf = buf;
	tx->size = size;

	xfer->msg_count++;
}

static int siw_hal_cmd_write(struct device *dev, u8 cmd)
{
	struct touch_bus_msg msg = {0, };
	u8 input[2] = {0, };
	int ret = 0;

	input[0] = cmd;
	input[1] = 0;

	msg.tx_buf = input;
	msg.tx_size = 2;

	msg.rx_buf = NULL;
	msg.rx_size = 0;
	msg.bits_per_word = 8;

	ret = siw_touch_bus_write(dev, &msg);
	if (ret < 0) {
		t_dev_err(dev, "touch cmd(0x%02X) write error, %d\n",
			  cmd, ret);
		return ret;
	}
	return 0;
}

static int siw_hal_condition_wait(struct device *dev,
				  u16 addr, u32 *value, u32 expect,
				  u32 mask, u32 delay, u32 retry)
{
	u32 data = 0;
	int ret = 0;

	do {
		touch_msleep(delay);

		ret = siw_hal_read_value(dev, addr, &data);
		switch (expect) {
		case FLASH_CODE_DNCHK_VALUE:
		case FLASH_CONF_DNCHK_VALUE:
			t_dev_dbg_base(dev,
	"wait read: addr[%04Xh] data[%08Xh], mask[%08Xh], expect[%08Xh], %d\n",
				addr, data, mask, expect, retry);
			break;
		}
		if ((ret >= 0) && ((data & mask) == expect)) {
			if (value)
				*value = data;

			return 0;
		}
	} while (--retry);

	if (value)
		*value = data;

	t_dev_err(dev,
	"wait fail: addr[%04Xh] data[%08Xh], mask[%08Xh], expect[%08Xh]\n",
		  addr, data, mask, expect);

	return -EPERM;
}

static void siw_hal_fb_notify_work_func(struct work_struct *fb_notify_work)
{
	struct siw_touch_chip *chip =
		container_of(to_delayed_work(fb_notify_work),
			     struct siw_touch_chip, fb_notify_work);
	int type = FB_RESUME;

	switch (chip->lcd_mode) {
	case LCD_MODE_U0:
	case LCD_MODE_U2:
		type = FB_SUSPEND;
		break;
	}

	siw_touch_notifier_call_chain(NOTIFY_FB, &type);
}

static void siw_hal_init_works(struct siw_touch_chip *chip)
{
	INIT_DELAYED_WORK(&chip->fb_notify_work, siw_hal_fb_notify_work_func);
}

static void siw_hal_free_works(struct siw_touch_chip *chip)
{
	cancel_delayed_work(&chip->fb_notify_work);
}

static void siw_hal_init_locks(struct siw_touch_chip *chip)
{
	mutex_init(&chip->bus_lock);
}

static void siw_hal_free_locks(struct siw_touch_chip *chip)
{
	mutex_destroy(&chip->bus_lock);
}


const struct tci_info siw_hal_tci_info_default[2] = {
	[TCI_1] = {
		.tap_count	= 2,
		.min_intertap	= 6,
		.max_intertap	= 70,
		.touch_slop	= 100,
		.tap_distance	= 10,
		.intr_delay	= 0,
	},
	[TCI_2] = {
		.tap_count	= 0,
		.min_intertap	= 6,
		.max_intertap	= 70,
		.touch_slop	= 100,
		.tap_distance	= 255,
		.intr_delay	= 20,
	},
};

#define siw_prt_tci_info(_dev, _idx, _info)	\
do {	\
	t_dev_info(_dev,	\
	"tci info[%s] tap_count %d, min_intertap %d, max_intertap %d\n",\
	_idx, _info->tap_count, _info->min_intertap, _info->max_intertap);\
	t_dev_info(_dev,	\
	"tci info[%s] touch_slop %d, tap_distance %d, intr_delay %d\n",\
	_idx, _info->touch_slop, _info->tap_distance, _info->intr_delay);\
} while (0)

#define siw_prt_area_info(_dev, _name, _area)	\
	t_dev_info(_dev, "%s %4d %4d %4d %4d\n",	\
		_name, _area->x1, _area->y1, _area->x2, _area->y2)

static void siw_hal_prt_tci_info(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct tci_ctrl *tci = &ts->tci;
	struct tci_info *info;
	struct active_area *area;
	struct reset_area *tci_qcover;

	info = &tci->info[TCI_1];
	siw_prt_tci_info(dev, "TCI_1", info);
	info = &tci->info[TCI_2];
	siw_prt_tci_info(dev, "TCI_2", info);

	area = &ts->tci.area;
	siw_prt_area_info(dev, "tci active area ", area);

	tci_qcover = &ts->tci.qcover_open;
	siw_prt_area_info(dev, "tci qcover_open ", tci_qcover);

	tci_qcover = &ts->tci.qcover_close;
	siw_prt_area_info(dev, "tci qcover_close", tci_qcover);
}

static void siw_hal_get_tci_info(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	void *tci_src;
	struct reset_area *tci_qcover;

	tci_src = pdata_tci_info(ts->pdata);
	if (tci_src == NULL)
		tci_src = (void *)siw_hal_tci_info_default;
	memcpy(ts->tci.info, tci_src, sizeof(siw_hal_tci_info_default));

	ts->tci.area.x1 = 0;
	ts->tci.area.y1 = 0;
	ts->tci.area.x2 = ts->caps.max_x;
	ts->tci.area.y2 = ts->caps.max_y;

	tci_qcover = pdata_tci_qcover_open(ts->pdata);
	if (tci_qcover != NULL)
		memcpy(&ts->tci.qcover_open, tci_qcover,
		sizeof(struct reset_area));

	tci_qcover = pdata_tci_qcover_close(ts->pdata);
	if (tci_qcover != NULL)
		memcpy(&ts->tci.qcover_close, tci_qcover,
		sizeof(struct reset_area));

	siw_hal_prt_tci_info(dev);
}

#if defined(__SIW_CONFIG_SWIPE)
const struct siw_hal_swipe_ctrl siw_hal_swipe_info_default = {
	.mode	= SWIPE_LEFT_BIT | SWIPE_RIGHT_BIT,
	.info = {
		[SWIPE_R] = {
			.distance		= 5,
			.ratio_thres		= 100,
			.ratio_distance		= 2,
			.ratio_period		= 5,
			.min_time		= 0,
			.max_time		= 150,
			.area.x1		= 401,
			.area.y1		= 0,
			.area.x2		= 1439,
			.area.y2		= 159,
		},
		[SWIPE_L] = {
			.distance		= 5,
			.ratio_thres	= 100,
			.ratio_distance	= 2,
			.ratio_period	= 5,
			.min_time		= 0,
			.max_time		= 150,
			.area.x1		= 401,	/* 0 */
			.area.y1		= 0,	/* 2060 */
			.area.x2		= 1439,
			.area.y2		= 159,	/* 2559 */
		},
	},
};

#define siw_prt_swipe_info(_dev, _idx, _info)	\
do {	\
	t_dev_info(_dev,	\
	"swipe info[%s] distance %d, ratio_thres %d, ratio_distance %d\n",\
	_idx, _info->distance, _info->ratio_thres, _info->ratio_distance);\
	t_dev_info(_dev,	\
	"swipe info[%s] ratio_period %d, min_time %d, max_time %d\n",\
	_idx, _info->ratio_period, _info->min_time, _info->max_time);\
	t_dev_info(_dev,	\
	"swipe info[%s] area_x1 %d, area_y1 %d, area_x2 %d, area_y2 %d\n",\
	_idx, _info->area.x1, _info->area.y1, _info->area.x2, _info->area.y2);\
} while (0)

static void siw_hal_prt_swipe_info(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_swipe_ctrl *swipe = &chip->swipe;
	struct siw_hal_swipe_info *info;

	t_dev_info(dev, "swipe mode %08Xh\n", swipe->mode);
	info = &swipe->info[SWIPE_R];
	siw_prt_swipe_info(dev, "SWIPE_R", info);
	info = &swipe->info[SWIPE_L];
	siw_prt_swipe_info(dev, "SWIPE_L", info);
}

static void siw_hal_get_swipe_info(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	void *swipe_src;

	swipe_src = pdata_swipe_ctrl(ts->pdata);
	if (!swipe_src)
		swipe_src = (void *)&siw_hal_swipe_info_default;

	memcpy(&chip->swipe, swipe_src, sizeof(struct siw_hal_swipe_ctrl));

	siw_hal_prt_swipe_info(dev);
}
#else	/* __SIW_CONFIG_SWIPE */
static void siw_hal_get_swipe_info(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);

	memset(&chip->swipe, 0, sizeof(struct siw_hal_swipe_ctrl));

	t_dev_info(dev, "swipe mode disabled\n");
}
#endif	/* __SIW_CONFIG_SWIPE */

static const char * const siw_hal_pwr_name[] = {
	[POWER_OFF]	= "Power off",
	[POWER_SLEEP]	= "Power sleep",
	[POWER_WAKE]	= "Power wake",
	[POWER_ON]	= "Power on",
};

static int siw_hal_power(struct device *dev, int ctrl)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	if ((ctrl < 0) || (ctrl > POWER_ON)) {
		t_dev_err(dev, "power ctrl: wrong ctrl value, %d\n", ctrl);
		return -EINVAL;
	}

	t_dev_dbg_pm(dev, "power ctrl: %s - %s\n",
		     touch_chip_name(ts), siw_hal_pwr_name[ctrl]);

	switch (ctrl) {
	case POWER_OFF:
		t_dev_dbg_pm(dev, "power ctrl: power off\n");
		atomic_set(&chip->init, IC_INIT_NEED);

		siw_hal_set_gpio_reset(dev, GPIO_OUT_ZERO);
		siw_hal_power_vio(dev, 0);
		siw_hal_power_vdd(dev, 0);
		touch_msleep(1 + hal_dbg_delay(chip, HAL_DBG_DLY_HW_RST_0));
		break;

	case POWER_ON:
		t_dev_dbg_pm(dev, "power ctrl: power on\n");
		siw_hal_power_vdd(dev, 1);
		siw_hal_power_vio(dev, 1);
		siw_hal_set_gpio_reset(dev, GPIO_OUT_ONE);
		break;

	case POWER_SLEEP:
		t_dev_dbg_pm(dev, "power ctrl: sleep\n");
		break;

	case POWER_WAKE:
		t_dev_dbg_pm(dev, "power ctrl: wake\n");
		break;

	case POWER_HW_RESET:
		t_dev_info(dev, "power ctrl: reset\n");
		siw_hal_reset_ctrl(dev, HW_RESET_ASYNC);
	}

	return 0;
}

enum {
	BOOT_CHK_SKIP = (1 << 16),
};

static int siw_hal_chk_boot_mode(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	u32 boot_failed = 0;
	u32 bootmode = 0;
	u32 boot_chk_offset_busy = 0;
	u32 boot_chk_offset_err = 0;
	u32 boot_chk_empty_mask = 0;
	int ret = 0;

	switch (chip->opt.t_boot_mode) {
	case 2:
		boot_chk_empty_mask = (1 << 6);
	/* fall through */
	case 1:
		boot_chk_offset_busy = 0;
		boot_chk_offset_err = 2;
		break;
	default:
		boot_chk_offset_busy = 1;
		boot_chk_offset_err = 3;
		break;
	}

	ret = siw_hal_read_value(dev,
				 reg->spr_boot_status,
				 &bootmode);
	if (ret < 0)
		return ret;

	/* maybe nReset is low state */
	if (!bootmode || (bootmode == ~0))
		return BOOT_CHK_SKIP;

	/* booting... need to wait */
	if ((bootmode >> boot_chk_offset_busy) & 0x1)
		return BOOT_CHK_SKIP;

	/* CRC error */
	boot_failed |= !!((bootmode >> boot_chk_offset_err) & 0x1);
	boot_failed |= (!!(bootmode & boot_chk_empty_mask)) << 1;

	if (boot_failed) {
		t_dev_err(dev, "boot fail: boot sts  = %08Xh(%02Xh)\n",
			  bootmode, boot_failed);
	}

	return boot_failed;
}

static int siw_hal_chk_boot_status(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	struct siw_hal_status_mask_bit *mask_bit = &chip->status_mask_bit;
	u32 boot_failed = 0;
	u32 tc_status = 0;
	u32 valid_cfg_crc_mask = 0;
	u32 valid_code_crc_mask = 0;
	int ret = 0;

	valid_cfg_crc_mask = (mask_bit->valid_cfg_crc) ?
		mask_bit->valid_cfg_crc : (1 << STS_POS_VALID_CFG_CRC);
	valid_code_crc_mask = (mask_bit->valid_code_crc) ?
		mask_bit->valid_code_crc : (1 << STS_POS_VALID_CODE_CRC);

	ret = siw_hal_read_value(dev,
				 reg->tc_status,
				 &tc_status);
	if (ret < 0)
		return ret;

	/* maybe nReset is low state */
	if (!tc_status || (tc_status == ~0))
		return BOOT_CHK_SKIP;

	if (!(tc_status & valid_cfg_crc_mask))
		boot_failed |= (1 << 5);
	if (!(tc_status & valid_code_crc_mask))
		boot_failed |= (1 << 4);
	if (boot_failed) {
		t_dev_err(dev, "boot fail: tc_status = %08Xh(%02Xh)\n",
			  tc_status, boot_failed);
	}

	return boot_failed;
}

enum {
	BOOT_CHK_MODE_RETRY	= 2,
	BOOT_CHK_STS_RETRY	= 2,
	/* */
	BOOT_CHK_MODE_DELAY	= 10,
	BOOT_CHK_STS_DELAY	= 10,
};

static int siw_hal_chk_boot(struct device *dev)
{
	u32 boot_failed = 0;
	int retry;
	int ret = 0;

	retry = BOOT_CHK_MODE_RETRY;
	while (retry--) {
		ret = siw_hal_chk_boot_mode(dev);
		if (ret < 0)
			return ret;
		if (ret == BOOT_CHK_SKIP)
			return 0;
		if (!ret)
			break;
		touch_msleep(BOOT_CHK_MODE_DELAY);
	}
	boot_failed |= ret;

	retry = BOOT_CHK_STS_RETRY;
	while (retry--) {
		ret = siw_hal_chk_boot_status(dev);
		if (ret < 0)
			return ret;
		if (ret == BOOT_CHK_SKIP)
			return boot_failed;
		if (!ret)
			break;
		touch_msleep(BOOT_CHK_STS_DELAY);
	}
	boot_failed |= ret;

	return boot_failed;
}

#if defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49408)
#define __SIW_SUPPORT_STATUS_ERROR_MEM
#endif

#if defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49106) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49408) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49409) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49501)
#define __SIW_SUPPORT_STATUS_ERROR_DISP
#endif

enum {
	IC_DEBUG_SIZE		= 16,

	IC_CHK_LOG_MAX		= (1 << 9),

	INT_IC_ABNORMAL_STATUS	= (1 << 0),

	INT_IC_ERROR_STATUS = ((1 << 5) | (1 << 3)),
};

static const struct siw_hal_status_filter status_filter_type_0[] = {
	_STS_FILTER(STS_ID_VALID_DEV_CTL, 1, STS_POS_VALID_DEV_CTL,
	0, "device ctl not set"),
	_STS_FILTER(STS_ID_VALID_CODE_CRC, 1, STS_POS_VALID_CODE_CRC_TYPE_0,
	0, "code crc invalid"),
	_STS_FILTER(STS_ID_ERROR_ABNORMAL, 1, STS_POS_ERROR_ABNORMAL,
	STS_FILTER_FLAG_TYPE_ERROR | STS_FILTER_FLAG_CHK_FAULT,
	"abnormal status detected"),
	_STS_FILTER(STS_ID_ERROR_SYSTEM, 1, STS_POS_ERROR_SYSTEM,
	STS_FILTER_FLAG_TYPE_ERROR | STS_FILTER_FLAG_ESD_SEND,
	"system error detected"),
	_STS_FILTER(STS_ID_ERROR_MISMTACH, 2, STS_POS_ERROR_MISMTACH,
	STS_FILTER_FLAG_TYPE_ERROR,
	"display mode mismatch"),
	_STS_FILTER(STS_ID_VALID_IRQ_PIN, 1, STS_POS_VALID_IRQ_PIN,
	0, "irq pin invalid"),
	_STS_FILTER(STS_ID_VALID_IRQ_EN, 1, STS_POS_VALID_IRQ_EN,
	0, "irq status invalid"),
	/* end mask */
	_STS_FILTER(STS_ID_NONE, 0, 0, 0, NULL),
};

static const struct siw_hal_status_filter status_filter_type_1[] = {
	_STS_FILTER(STS_ID_VALID_DEV_CTL, 1, STS_POS_VALID_DEV_CTL,
	0, "device ctl not set"),
	_STS_FILTER(STS_ID_VALID_CODE_CRC, 1, STS_POS_VALID_CODE_CRC,
	0, "code crc invalid"),
	_STS_FILTER(STS_ID_VALID_CFG_CRC, 1, STS_POS_VALID_CFG_CRC,
	0, "cfg crc invalid"),
	_STS_FILTER(STS_ID_ERROR_ABNORMAL, 1, STS_POS_ERROR_ABNORMAL,
	STS_FILTER_FLAG_TYPE_ERROR | STS_FILTER_FLAG_CHK_FAULT,
	"abnormal status detected"),
	_STS_FILTER(STS_ID_ERROR_SYSTEM, 1, STS_POS_ERROR_SYSTEM,
	STS_FILTER_FLAG_TYPE_ERROR | STS_FILTER_FLAG_ESD_SEND,
	"system error detected"),
	_STS_FILTER(STS_ID_ERROR_MISMTACH, 1, STS_POS_ERROR_MISMTACH,
	STS_FILTER_FLAG_TYPE_ERROR,
	"display mode mismatch"),
	_STS_FILTER(STS_ID_VALID_IRQ_PIN, 1, STS_POS_VALID_IRQ_PIN,
	0, "irq pin invalid"),
	_STS_FILTER(STS_ID_VALID_IRQ_EN, 1, STS_POS_VALID_IRQ_EN,
	0, "irq status invalid"),
	_STS_FILTER(STS_ID_VALID_TC_DRV, 1, STS_POS_VALID_TC_DRV,
	0, "driving invalid"),
#if defined(__SIW_SUPPORT_STATUS_ERROR_MEM)
	_STS_FILTER(STS_ID_ERROR_MEM, 1, STS_POS_ERROR_MEM,
	STS_FILTER_FLAG_TYPE_ERROR,
	"memory error detected"),
#endif
#if defined(__SIW_SUPPORT_STATUS_ERROR_DISP)
	_STS_FILTER(STS_ID_ERROR_DISP, 1, STS_POS_ERROR_DISP,
	STS_FILTER_FLAG_TYPE_ERROR | STS_FILTER_FLAG_ESD_SEND,
	"display error detected"),
#endif
	/* end mask */
	_STS_FILTER(STS_ID_NONE, 0, 0, 0, NULL),
};

static const struct siw_hal_status_filter status_filter_type_2[] = {
	_STS_FILTER(STS_ID_VALID_DEV_CTL, 1, STS_POS_VALID_DEV_CTL,
	0, "device ctl not set"),
	_STS_FILTER(STS_ID_ERROR_ABNORMAL, 1, STS_POS_ERROR_ABNORMAL,
	STS_FILTER_FLAG_TYPE_ERROR | STS_FILTER_FLAG_CHK_FAULT,
	"re-init required"),
	_STS_FILTER(STS_ID_VALID_IRQ_PIN, 1, STS_POS_VALID_IRQ_PIN,
	0, "irq pin invalid"),
	_STS_FILTER(STS_ID_VALID_IRQ_EN, 1, STS_POS_VALID_IRQ_EN,
	0, "irq status invalid"),
	_STS_FILTER(STS_ID_VALID_TC_DRV, 1, STS_POS_VALID_TC_DRV,
	0, "driving invalid"),
	/* end mask */
	_STS_FILTER(STS_ID_NONE, 0, 0, 0, NULL),
};

static u32 siw_hal_get_status_mask(struct device *dev, int id)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_status_filter *filter = chip->status_filter;
	u32 mask = 0;

	if (filter == NULL)
		goto out;

	while (1) {
		if (!filter->id || !filter->width)
			break;

		if (filter->id == id) {
			mask = ((1 << filter->width) - 1) << filter->pos;
			break;
		}
		filter++;
	}

out:
	return mask;
}

static void siw_hal_chk_report_type(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_fw_info *fw = &chip->fw;

	if (chip->report_type)
		return;

	if (!fw->product_id[0])
		return;

	switch (touch_chip_type(ts)) {
	default:
		chip->report_type = CHIP_REPORT_TYPE_0;
		break;
	}

	t_dev_info(dev, "report type  : %d\n", chip->report_type);
}

static int siw_hal_chk_status_type(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_fw_info *fw = &chip->fw;
	struct siw_hal_status_mask_bit *mask_bit = &chip->status_mask_bit;

	if (chip->status_type)
		return 0;

	if (!fw->product_id[0])
		return -EINVAL;

	siw_hal_chk_report_type(dev);

	switch (touch_chip_type(ts)) {
	case CHIP_SW42101:
		chip->status_type = CHIP_STATUS_TYPE_2;
		break;
	case CHIP_LG4894:
		if (!strncmp(fw->product_id, "L0W53K6P", 8)) {
			chip->status_type = CHIP_STATUS_TYPE_0;
			break;
		}
		chip->status_type = CHIP_STATUS_TYPE_1;
		break;
	default:
		chip->status_type = CHIP_STATUS_TYPE_1;
		break;
	}

	switch (chip->status_type) {
	case CHIP_STATUS_TYPE_2:
		chip->status_filter =
			(struct siw_hal_status_filter *)status_filter_type_2;
		break;
	case CHIP_STATUS_TYPE_0:
		chip->status_filter =
			(struct siw_hal_status_filter *)status_filter_type_0;
		break;
	default:
		chip->status_filter =
			(struct siw_hal_status_filter *)status_filter_type_1;
		break;
	}

	mask_bit->valid_dev_ctl =
		siw_hal_get_status_mask(dev, STS_ID_VALID_DEV_CTL);
	mask_bit->valid_code_crc =
		siw_hal_get_status_mask(dev, STS_ID_VALID_CODE_CRC);
	mask_bit->valid_cfg_crc =
		siw_hal_get_status_mask(dev, STS_ID_VALID_CFG_CRC);
	mask_bit->error_abnormal =
		siw_hal_get_status_mask(dev, STS_ID_ERROR_ABNORMAL);
	mask_bit->error_system =
		siw_hal_get_status_mask(dev, STS_ID_ERROR_SYSTEM);
	mask_bit->error_mismtach =
		siw_hal_get_status_mask(dev, STS_ID_ERROR_MISMTACH);
	mask_bit->valid_irq_pin =
		siw_hal_get_status_mask(dev, STS_ID_VALID_IRQ_PIN);
	mask_bit->valid_irq_en =
		siw_hal_get_status_mask(dev, STS_ID_VALID_IRQ_EN);
	mask_bit->error_mem =
		siw_hal_get_status_mask(dev, STS_ID_ERROR_MEM);
	mask_bit->valid_tv_drv =
		siw_hal_get_status_mask(dev, STS_ID_VALID_TC_DRV);
	mask_bit->error_disp =
		siw_hal_get_status_mask(dev, STS_ID_ERROR_DISP);

	t_dev_dbg_base(dev, "mask[v_dev]  : %08Xh\n", mask_bit->valid_dev_ctl);
	t_dev_dbg_base(dev, "mask[v_code] : %08Xh\n", mask_bit->valid_code_crc);
	t_dev_dbg_base(dev, "mask[v_cfg]  : %08Xh\n", mask_bit->valid_cfg_crc);
	t_dev_dbg_base(dev, "mask[e_abn]  : %08Xh\n", mask_bit->error_abnormal);
	t_dev_dbg_base(dev, "mask[e_sys]  : %08Xh\n", mask_bit->error_system);
	t_dev_dbg_base(dev, "mask[e_mis]  : %08Xh\n", mask_bit->error_mismtach);
	t_dev_dbg_base(dev, "mask[v_i_p]  : %08Xh\n", mask_bit->valid_irq_pin);
	t_dev_dbg_base(dev, "mask[v_i_e]  : %08Xh\n", mask_bit->valid_irq_en);
	t_dev_dbg_base(dev, "mask[e_mem]  : %08Xh\n", mask_bit->error_mem);
	t_dev_dbg_base(dev, "mask[v_tc]   : %08Xh\n", mask_bit->valid_tv_drv);
	t_dev_dbg_base(dev, "mask[e_disp] : %08Xh\n", mask_bit->error_disp);

	chip->status_mask_normal = mask_bit->valid_dev_ctl |
				   mask_bit->valid_code_crc |
				   mask_bit->valid_cfg_crc |
				   mask_bit->valid_irq_pin |
				   mask_bit->valid_irq_en |
				   mask_bit->valid_tv_drv |
				   0;

	chip->status_mask_logging = mask_bit->error_mismtach |
				    mask_bit->valid_irq_pin |
				    mask_bit->valid_irq_en |
				    mask_bit->valid_tv_drv |
				    0;

	chip->status_mask_reset = mask_bit->valid_dev_ctl |
				  mask_bit->valid_code_crc |
				  mask_bit->valid_cfg_crc |
				  mask_bit->error_abnormal |
				  mask_bit->error_system |
				  mask_bit->error_mem |
				  mask_bit->error_disp |
				  0;

	chip->status_mask = chip->status_mask_normal |
			    chip->status_mask_logging |
			    chip->status_mask_reset |
			    0;

	chip->status_mask_ic_abnormal = INT_IC_ABNORMAL_STATUS;
	chip->status_mask_ic_error = INT_IC_ERROR_STATUS;
	chip->status_mask_ic_disp_err = 0;

	switch (chip->opt.t_sts_mask) {
	case 1:
		chip->status_mask_ic_abnormal |= (0x3 << 6);
		chip->status_mask_ic_valid = 0xFFFF;
		break;
	case 2:
		chip->status_mask_ic_abnormal |= (0x3 << 1);
		chip->status_mask_ic_error = ((1 << 7) | (1 << 5));
		chip->status_mask_ic_valid = 0x7FFFF;
		chip->status_mask_ic_disp_err = (0x3 << 8);
		break;
	default:
		chip->status_mask_ic_valid = 0xFF;
		break;
	}

	t_dev_info(dev, "status type  : %d\n", chip->status_type);
	t_dev_info(dev, "status mask  : %08Xh\n", chip->status_mask);
	t_dev_info(dev, "normal      : %08Xh\n", chip->status_mask_normal);
	t_dev_info(dev, "logging     : %08Xh\n", chip->status_mask_logging);
	t_dev_info(dev, "reset       : %08Xh\n", chip->status_mask_reset);
	t_dev_info(dev, "ic abnormal : %08Xh\n", chip->status_mask_ic_abnormal);
	t_dev_info(dev, "ic error    : %08Xh\n", chip->status_mask_ic_error);
	t_dev_info(dev, "ic disp err : %08Xh\n", chip->status_mask_ic_disp_err);

	return 0;
}

struct siw_ic_info_chip_proto {
	int chip_type;
	int vchip;
	int vproto;
};

static const struct siw_ic_info_chip_proto siw_ic_info_chip_protos[] = {
	{ CHIP_LG4894, 4, 4 },
	{ CHIP_LG4895, 8, 4 },
	{ CHIP_LG4946, 7, 4 },
	{ CHIP_LG4951, 7, 4 },
	{ CHIP_SW1828, 9, 4 },
	{ CHIP_SW49105, 10, 4 },
	{ CHIP_SW49106, 11, 4 },
	{ CHIP_SW49406, 7, 4 },
	{ CHIP_SW49407, 9, 4 },
	{ CHIP_SW49408, 9, 4 },
	{ CHIP_SW49501, 14, 4 },
	{ CHIP_SW42101, 15, 4 },
	{ CHIP_NONE, 0, 0 },
};

static int siw_hal_ic_info_ver_check(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_fw_info *fw = &chip->fw;
	struct siw_ic_info_chip_proto *chip_proto;
	u32 vchip = fw->v.version.chip;
	u32 vproto = fw->v.version.protocol;

	chip_proto = (struct siw_ic_info_chip_proto *)siw_ic_info_chip_protos;
	while (1) {
		if (chip_proto->chip_type == CHIP_NONE)
			break;

		if (touch_chip_type(ts) == chip_proto->chip_type) {
			if ((chip_proto->vchip != vchip) ||
			    (chip_proto->vproto != vproto))
				break;

			t_dev_info(dev, "[%s] IC info is good: %d, %d\n",
				   touch_chip_name(ts), vchip, vproto);

			return 0;
		}

		chip_proto++;
	}

	t_dev_err(dev, "[%s] IC info is abnormal: %d, %d\n",
		  touch_chip_name(ts), vchip, vproto);

	return -EINVAL;
}

static int siw_hal_do_ic_info(struct device *dev, int prt_on)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_reg *reg = chip->reg;
	struct siw_hal_fw_info *fw = &chip->fw;
	struct touch_xfer_msg *xfer = ts->xfer;
	u32 product[2] = {0};
	u32 chip_id = 0;
	u32 version = 0;
	u32 version_ext = 0;
	u32 revision = 0;
	u32 bootmode = 0;
	u32 boot_chk_offset;
	int ret = 0;

	siw_hal_xfer_init(dev, xfer);

	siw_hal_xfer_add_rx(xfer,
			    reg->spr_chip_id,
			    (void *)&chip_id, sizeof(chip_id));
	siw_hal_xfer_add_rx(xfer,
			    reg->tc_version,
			    (void *)&version, sizeof(version));
	siw_hal_xfer_add_rx(xfer,
			    reg->info_chip_version,
			    (void *)&revision, sizeof(revision));
	siw_hal_xfer_add_rx(xfer,
			    reg->tc_product_id1,
			    (void *)&product[0], sizeof(product));
	if (chip->opt.f_ver_ext) {
		siw_hal_xfer_add_rx(xfer,
				    reg->tc_version_ext,
				    (void *)&version_ext, sizeof(version_ext));
	}
	siw_hal_xfer_add_rx(xfer,
			    reg->spr_boot_status,
			    (void *)&bootmode, sizeof(bootmode));

	ret = siw_hal_xfer_msg(dev, ts->xfer);
	if (ret < 0) {
		t_dev_err(dev, "ic_info(1): xfer failed, %d\n", ret);
		return ret;
	}

	if (chip->opt.f_info_more) {
		siw_hal_xfer_init(dev, xfer);

		siw_hal_xfer_add_rx(xfer,
				    reg->info_fpc_type,
				    (void *)&fw->fpc, sizeof(fw->fpc));
		siw_hal_xfer_add_rx(xfer,
				    reg->info_wfr_type,
				    (void *)&fw->wfr, sizeof(fw->wfr));
		siw_hal_xfer_add_rx(xfer,
				    reg->info_cg_type,
				    (void *)&fw->cg, sizeof(fw->cg));
		siw_hal_xfer_add_rx(xfer,
				    reg->info_lot_num,
				    (void *)&fw->lot, sizeof(fw->lot));
		siw_hal_xfer_add_rx(xfer,
				    reg->info_serial_num,
				    (void *)&fw->sn, sizeof(fw->sn));
		siw_hal_xfer_add_rx(xfer,
				    reg->info_date,
				    (void *)&fw->date, sizeof(fw->date));
		siw_hal_xfer_add_rx(xfer,
				    reg->info_time,
				    (void *)&fw->time, sizeof(fw->time));

		ret = siw_hal_xfer_msg(dev, ts->xfer);
		if (ret < 0) {
			t_dev_err(dev, "ic_info(2): xfer failed, %d\n", ret);
			return ret;
		}
	}

	siw_hal_fw_set_chip_id(fw, chip_id);
	siw_hal_fw_set_version(fw, version, version_ext);
	siw_hal_fw_set_revision(fw, revision);
	siw_hal_fw_set_prod_id(fw, (u8 *)product, sizeof(product));

	siw_hal_chk_status_type(dev);

	fw->wfr &= WAFER_TYPE_MASK;

	if (fw->version_ext) {
		int ferr;

		ferr = siw_hal_fw_chk_version_ext(fw->version_ext,
						  fw->v.version.ext);
		t_dev_info_sel(dev, prt_on,
			"[T] chip id %s, version %08X(%u.%02u) (0x%02X) %s\n",
			chip->fw.chip_id,
			fw->version_ext,
			fw->v.version.major, fw->v.version.minor,
			fw->revision,
			(ferr < 0) ? "(invalid)" : "");
	} else {
		t_dev_info_sel(dev, prt_on,
			"[T] chip id %s, version v%u.%02u (0x%08X, 0x%02X)\n",
			fw->chip_id,
			fw->v.version.major, fw->v.version.minor,
			version, fw->revision);
	}

	switch (chip->opt.t_boot_mode) {
	case 2:
	/* fall through */
	case 1:
		boot_chk_offset = 0;
		break;
	default:
		boot_chk_offset = 1;
		break;
	}
	t_dev_info_sel(dev, prt_on,
		"[T] product id %s, flash boot %s(%s), crc %s (0x%08X)\n",
		fw->product_id,
		((bootmode >> boot_chk_offset) & 0x1) ? "BUSY" : "idle",
		((bootmode >> (boot_chk_offset + 1)) & 0x1) ?
							"done" : "booting",
		((bootmode >> (boot_chk_offset + 2)) & 0x1) ? "ERROR" : "ok",
		bootmode);

	ret = siw_hal_chk_boot(dev);
	if (ret < 0)
		return ret;
	if (ret) {
		int boot_fail_cnt = chip->boot_fail_cnt;

		atomic_set(&chip->boot, IC_BOOT_FAIL);

		/* Limit to avoid infinite repetition */
		if (boot_fail_cnt >= BOOT_FAIL_RECOVERY_MAX) {
			t_dev_err(dev,
				"Boot fail can't be recovered(%d) - %02Xh\n",
				boot_fail_cnt, ret);
			return -EFAULT;
		}

		t_dev_err(dev, "Boot fail detected(%d) - %02Xh\n",
			  boot_fail_cnt, ret);

		chip->boot_fail_cnt++;

		/* return special flag to let the core layer know */
		return -ETDBOOTFAIL;
	}
	chip->boot_fail_cnt = 0;

	if (chip->opt.f_info_more) {
		t_dev_info_sel(dev, prt_on,
			       "[T] fpc %d, wfr %d, cg %d, lot %d\n",
			       fw->fpc, fw->wfr, fw->cg, fw->lot);
		t_dev_info_sel(dev, prt_on,
		"[T] sn %Xh, date %04d.%02d.%02d, time %02d:%02d:%02d site%d\n",
			       fw->sn,
			       fw->date & 0xFFFF,
			       ((fw->date >> 16) & 0xFF),
			       ((fw->date >> 24) & 0xFF),
			       fw->time & 0xFF,
			       ((fw->time >> 8) & 0xFF),
			       ((fw->time >> 16) & 0xFF),
			       ((fw->time >> 24) & 0xFF));
	}

	if (strcmp(fw->chip_id, touch_chip_id(ts))) {
		t_dev_err(dev, "Invalid chip id(%s), shall be %s\n",
			  fw->chip_id, touch_chip_id(ts));
		return -EINVAL;
	}

	ret = siw_hal_ic_info_ver_check(dev);

	return ret;
}

static int siw_hal_ic_info(struct device *dev)
{
	return siw_hal_do_ic_info(dev, 1);
}

#if defined(__SIW_CONFIG_FB) && !defined(__SIW_CONFIG_SYSTEM_PM)
static int siw_hal_fb_notifier_callback(struct notifier_block *self,
					unsigned long event, void *data)
{
	struct siw_ts *ts =
		container_of(self, struct siw_ts, fb_notif);
	struct fb_event *ev = (struct fb_event *)data;
	int *blank;

	if (!ev || !ev->data)
		return 0;

	blank = (int *)ev->data;

	if (event == FB_EARLY_EVENT_BLANK) {
		if (*blank == FB_BLANK_UNBLANK)
			t_dev_info(ts->dev, "FB_UNBLANK(early)\n");
		else if (*blank == FB_BLANK_POWERDOWN)
			t_dev_info(ts->dev, "FB_BLANK(early)\n");
	}

	if (event == FB_EVENT_BLANK) {
		if (*blank == FB_BLANK_UNBLANK)
			t_dev_info(ts->dev, "FB_UNBLANK\n");
		else if (*blank == FB_BLANK_POWERDOWN)
			t_dev_info(ts->dev, "FB_BLANK\n");
	}

	return 0;
}

/*
 * Change notifier to siw_hal_fb_notifier_callback
 * instead of siw_touch_fb_notifier_callback and
 * siw_touch_suspend/resume will be called via
 * siw_touch_fb_work_func
 * : siw_touch_notify -> siw_touch_qd_fb_work_now
 */
static int siw_hal_fb_notifier_init(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	t_dev_dbg_base(dev, "fb_notif change\n");

	fb_unregister_client(&ts->fb_notif);
	ts->fb_notif.notifier_call = siw_hal_fb_notifier_callback;
	fb_register_client(&ts->fb_notif);

	return 0;
}
#else	/* __SIW_CONFIG_FB */
static int siw_hal_fb_notifier_init(struct device *dev)
{
	return 0;
}
#endif	/* __SIW_CONFIG_FB */

static int siw_hal_init_reg_verify(struct device *dev,
				   u32 addr, u32 data, int retry,
				   const char *name)
{
	u32 rdata = ~0;
	int i;
	int ret = 0;

	for (i = 0; i < retry; i++) {
		ret = siw_hal_write_value(dev, addr, data);
		if (ret < 0) {
			t_dev_err(dev, "failed to write %s, %d\n",
				  name, ret);
			return ret;
		}

		ret = siw_hal_read_value(dev, addr, &rdata);
		if (ret < 0) {
			t_dev_err(dev, "failed to read %s, %d\n",
				  name, ret);
			return ret;
		}

		if (data == rdata) {
			t_dev_dbg_base(dev, "init reg done: %s(%08Xh)\n",
				name, rdata);
			return 0;
		}
	}

	t_dev_err(dev, "init reg failed: %08Xh, %08Xh\n", data, rdata);

	return -EFAULT;
}

static int siw_hal_init_reg_set_pre(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int ret = 0;

	if (chip->opt.f_attn_opt) {
		u32 addr = 0;

		if (touch_bus_type(ts) == BUS_IF_I2C)
			addr = 0xFE4;
		else
			addr = 0xFE5;
		if (addr) {
			ret = siw_hal_write_value(dev, addr, 0);
			if (ret < 0)
				goto out;
		}
		ret = siw_hal_init_reg_verify(dev,
					      0xFF3, ABNORMAL_IC_DETECTION, 3,
					      "spi_tattn_opt");
		if (ret < 0) {
			t_dev_err(dev,
				  "failed to set spi_tattn_opt, %d\n",
				  ret);
			goto out;
		}
	}

out:
	return ret;
}

static int siw_hal_init_reg_set_post(struct device *dev)
{
	return 0;
}

static int siw_hal_init_reg_set(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_reg *reg = chip->reg;
	u32 data = 1;
	int ret = 0;

	siw_hal_init_reg_set_pre(dev);

	ret = siw_hal_write_value(dev,
				  reg->tc_device_ctl,
				  1);
	if (ret < 0) {
		t_dev_err(dev, "failed to start chip, %d\n", ret);
		goto out;
	}

	ret = siw_hal_write_value(dev,
				  reg->tc_interrupt_ctl,
				  1);
	if (ret < 0) {
		t_dev_err(dev, "failed to start chip irq, %d\n", ret);
		goto out;
	}

	data = atomic_read(&ts->state.ime);

	ret = siw_hal_write_value(dev,
				  reg->ime_state,
				  data);
	if (ret < 0)
		goto out;

	siw_hal_init_reg_set_post(dev);

out:
	return ret;
}

#if defined(__SIW_SUPPORT_WATCH)
static int siw_hal_check_watch(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);

	siw_hal_watch_chk_font_status(chip->dev);

	if ((chip->lcd_mode == LCD_MODE_U2) &&
	    siw_hal_watch_is_disp_waton(dev) &&
	    siw_hal_watch_is_rtc_run(dev))
		siw_hal_watch_get_curr_time(dev, NULL, NULL);

	return 0;
}

static int siw_hal_check_mode_type_0(struct device *dev,
				     int lcd_mode, int chk_mode)
{
	int ret = 0;

	if (lcd_mode == LCD_MODE_U3)
		goto out;

	if (lcd_mode == LCD_MODE_U2) {
		if (chk_mode == LCD_MODE_U2_UNBLANK) {
			t_dev_info(dev, "U1 -> U2 : watch on\n");
			siw_hal_watch_init(dev);
			/* knockon mode change + swipe enable */
			ret = siw_hal_tc_driving(dev, LCD_MODE_U2);
			if (!ret)
				ret = 1;
		} else
			t_dev_info(dev, "U2 mode change\n");
		goto out;
	}

	if (lcd_mode == LCD_MODE_U2_UNBLANK) {
		switch (chk_mode) {
		case LCD_MODE_STOP:
			t_dev_info(dev,
				"Skip mode change : LCD_MODE_STOP -> U1\n");
			siw_hal_watch_display_off(dev);
			ret = 1;
			break;
		case LCD_MODE_U2:
			t_dev_info(dev, "U2 -> U1 : watch off\n");
			siw_hal_watch_display_off(dev);
			/* abs mode change + swipe disable */
			ret = siw_hal_tc_driving(dev, LCD_MODE_U2_UNBLANK);
			if (!ret)
				ret = 1;
			break;
		case LCD_MODE_U0:
			t_dev_info(dev, "U0 -> U1 mode change\n");
			break;
		default:
			t_dev_info(dev, "Not Defined Mode, %d\n", chk_mode);
			break;
		}
		goto out;
	}

	if (lcd_mode == LCD_MODE_U0) {
		t_dev_info(dev, "U0 mode change\n");
		goto out;
	}

	t_dev_info(dev, "Not defined mode, %d\n", lcd_mode);

out:
	return ret;
}

static int siw_hal_check_mode_type_1(struct device *dev,
				     int lcd_mode, int chk_mode)
{
	int ret = 0;

	if (lcd_mode == LCD_MODE_U3)
		goto out;

	if (lcd_mode == LCD_MODE_U2) {
		if (chk_mode == LCD_MODE_U2_UNBLANK) {
			t_dev_info(dev, "U1 -> U2 : watch on\n");
			siw_hal_watch_init(dev);
			ret = 1;
		} else {
			t_dev_info(dev, "U2 mode change\n");
			siw_hal_watch_init(dev);
		}
		goto out;
	}

	if (lcd_mode == LCD_MODE_U2_UNBLANK) {
		switch (chk_mode) {
		case LCD_MODE_U2:
			t_dev_info(dev, "U2 -> U1\n");
			break;
		case LCD_MODE_U0:
			t_dev_info(dev, "U0 -> U1 mode change\n");
			siw_hal_watch_init(dev);
			break;
		default:
			t_dev_info(dev, "Not Defined Mode, %d\n", chk_mode);
			break;
		}
		goto out;
	}

	if (lcd_mode == LCD_MODE_U0) {
		t_dev_info(dev, "U0 mode change\n");
		goto out;
	}

	t_dev_info(dev, "Not defined mode, %d\n", lcd_mode);

out:
	return ret;
}

static int siw_hal_check_mode(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	int ret = 0;

	switch (chip->opt.t_chk_mode) {
	case 1:
		ret = siw_hal_check_mode_type_1(dev,
			chip->lcd_mode, chip->prev_lcd_mode);
		break;
	default:
		ret = siw_hal_check_mode_type_0(dev,
			chip->lcd_mode, chip->driving_mode);
		break;
	}

	return ret;
}

static void siw_hal_lcd_event_read_reg(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_reg *reg = chip->reg;
	struct touch_xfer_msg *xfer = ts->xfer;
	u32 rdata[5] = {0, 0};
	int ret = 0;

	siw_hal_xfer_init(dev, xfer);

	siw_hal_xfer_add_rx(xfer,
			    reg->tc_ic_status,
			    (void *)&rdata[0], sizeof(rdata[0]));
	siw_hal_xfer_add_rx(xfer,
			    reg->tc_status,
			    (void *)&rdata[1], sizeof(rdata[1]));
	siw_hal_xfer_add_rx(xfer,
			    reg->spr_subdisp_status,
			    (void *)&rdata[2], sizeof(rdata[2]));
	siw_hal_xfer_add_rx(xfer,
			    reg->tc_version,
			    (void *)&rdata[3], sizeof(rdata[3]));
	siw_hal_xfer_add_rx(xfer,
			    reg->spr_chip_id,
			    (void *)&rdata[4], sizeof(rdata[4]));

	ret = siw_hal_xfer_msg(dev, xfer);
	if (ret < 0) {
		t_dev_err(dev, "xfer failed, %d\n", ret);
		return;
	}

	t_dev_info(dev,
"r[%04X] %08Xh, r[%04X] %08Xh, r[%04X] %08Xh, r[%04X] %08Xh, r[%04X] %08Xh\n",
		   reg->tc_ic_status, rdata[0],
		   reg->tc_status, rdata[1],
		   reg->spr_subdisp_status, rdata[2],
		   reg->tc_version, rdata[3],
		   reg->spr_chip_id, rdata[4]);
	t_dev_info(dev,
		   "v%d.%02d\n",
		   (rdata[3] >> 8) & 0xF, rdata[3] & 0xFF);
}
#else	/* __SIW_SUPPORT_WATCH */
static int siw_hal_check_watch(struct device *dev)
{
	return 0;
}
static int siw_hal_check_mode(struct device *dev)
{
	return 0;
}
static void siw_hal_lcd_event_read_reg(struct device *dev) { }
#endif	/* __SIW_SUPPORT_WATCH */

#define DIC_ERR_TYPE	0x10

static int siw_hal_send_esd_notifier(struct device *dev, int type)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int esd = type;
	int ret;

	if (touch_flags(ts) & TOUCH_SKIP_ESD_EVENT) {
		if (type < DIC_ERR_TYPE) {
			t_dev_info(dev, "skip sending ESD notifier\n");
			return 0;
		}
	}

	t_dev_info(dev, "trigger ESD notifier, %Xh\n", type);

	ret = siw_touch_atomic_notifier_call(LCD_EVENT_TOUCH_ESD_DETECTED,
					     (void *)&esd);
	if (ret)
		t_dev_err(dev, "check the value, %d\n", ret);

	/* Sending Event done */
	return 1;
}

static int siw_hal_init_quirk(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	u8 temp[10 + 4];
	int ret = 0;

	switch (touch_chip_type(ts)) {
	case CHIP_SW42101:
		memset(temp, 0, sizeof(temp));
		ret = siw_hal_reg_read(dev, 0x150, temp, 10);
		if (ret < 0) {
			t_dev_err(dev,
				"init quirk for SW42101 failed, %d\n", ret);
			return ret;
		}
		t_dev_info(dev, "init quirk for SW42101: %s\n", temp);
		break;
	}

	return 0;
}

static int siw_hal_init(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int init_retry = CHIP_INIT_RETRY_MAX;
	int i;
	int ret = 0;

	atomic_set(&chip->boot, IC_BOOT_DONE);

	if (atomic_read(&ts->state.core) == CORE_PROBE) {
		siw_hal_fb_notifier_init(dev);
		init_retry = CHIP_INIT_RETRY_PROBE;
	} else {
		ret = siw_hal_init_quirk(dev);
		if (ret < 0)
			goto out;
	}

	t_dev_dbg_base(dev, "charger_state = 0x%02X\n", chip->charger);

	if (atomic_read(&ts->state.debug_tool) == DEBUG_TOOL_ENABLE)
		siw_hal_abt_init(dev);

	for (i = 0; i < init_retry; i++) {
		ret = siw_hal_ic_info(dev);
		if (ret >= 0)
			break;
		/*
		 * When boot fail detected
		 *
		 * 1. At the fisrt detection,
		 *    it sends esd noti for LCD recovery(full reset procedure)
		 *    and skip fw_upgrade.
		 * 2. LCD driver is suppsed to send lcd mode notifier
		 *    back to touch side after its recovery.
		 * 3. The lcd mode notifier restarts init work again
		 *    via siw_touch_resume.
		 * 4. If boot fail detected again(counted by boot_fail_cnt)
		 *    it goes to fw_upgrade stage.
		 * (See siw_touch_init_work_func in siw_touch.c)
		 */
		if (ret == -ETDBOOTFAIL) {
			/* For the probe stage */
			if (atomic_read(&ts->state.core) == CORE_PROBE)
				break;

			/* Don't do recovery twice continuously */
			if (atomic_read(&chip->esd_noti_sent))
				break;

			/* At the first boot fail */
			if (chip->boot_fail_cnt > 1)
				break;

			if (siw_hal_send_esd_notifier(dev, 2)) {
				ret = -ETDSENTESD;
				break;
			}
		}

		t_dev_dbg_base(dev, "retry getting ic info (%d)\n", i);

		siw_touch_irq_control(dev, INTERRUPT_DISABLE);
		siw_hal_power(dev, POWER_OFF);
		siw_hal_power(dev, POWER_ON);
		touch_msleep(ts->caps.hw_reset_delay);
	}
	if (ret < 0)
		goto out;

	atomic_set(&chip->esd_noti_sent, 0);

	siw_hal_init_reg_set(dev);

	siw_hal_watch_rtc_on(dev);

	atomic_set(&chip->init, IC_INIT_DONE);
	atomic_set(&ts->state.sleep, IC_NORMAL);

	ret = siw_hal_lpwg_mode(dev);
	if (ret < 0) {
		t_dev_err(dev, "failed to lpwg_control, %d\n", ret);
		goto out;
	}

	ret = siw_hal_check_watch(dev);
	if (ret < 0) {
		t_dev_err(dev, "failed to check watch, %d\n", ret);
		goto out;
	}

out:
	if (ret) {
		t_dev_err(dev, "%s init failed, %d\n",
			  touch_chip_name(ts), ret);
	} else {
		t_dev_info(dev, "%s init done\n",
			   touch_chip_name(ts));
	}

	siwmon_submit_ops_step_chip_wh_name(dev, "%s init done",
					    touch_chip_name(ts), ret);

	return ret;
}

static int siw_hal_reinit(struct device *dev,
			  int pwr_con,
			  int delay,
			  int irq_enable,
			  int (*do_call)(struct device *dev))
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	int ret = 0;

	siw_touch_irq_control(dev, INTERRUPT_DISABLE);

	atomic_set(&chip->init, IC_INIT_NEED);

	if (pwr_con) {
		siw_hal_power(dev, POWER_OFF);
		siw_hal_power(dev, POWER_ON);
	} else {
		siw_hal_set_gpio_reset(dev, GPIO_OUT_ZERO);
		touch_msleep(1 + hal_dbg_delay(chip, HAL_DBG_DLY_HW_RST_0));
		siw_hal_set_gpio_reset(dev, GPIO_OUT_ONE);
	}

	touch_msleep(delay);

	if (do_call)
		ret = do_call(dev);

	if (irq_enable)
		siw_touch_irq_control(dev, INTERRUPT_ENABLE);

	return ret;
}

#define SIW_SW_RST_CTL_T2		0xFE0

static int siw_hal_sw_reset_type_2(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	t_dev_info(dev, "SW Reset(2)\n");

	siw_hal_write_value(dev, SIW_SW_RST_CTL_T2, 0);

	touch_msleep(1 + hal_dbg_delay(chip, HAL_DBG_DLY_SW_RST_0));

	siw_hal_write_value(dev, SIW_SW_RST_CTL_T2, 1);

	touch_msleep(ts->caps.sw_reset_delay +
		     hal_dbg_delay(chip, HAL_DBG_DLY_SW_RST_1));

	return 0;
}

static int siw_hal_sw_reset_type_1(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	t_dev_info(dev, "SW Reset(1)\n");

	siw_hal_cmd_write(dev, CMD_ENA);
	siw_hal_cmd_write(dev, CMD_RESET_LOW);

	touch_msleep(1 + hal_dbg_delay(chip, HAL_DBG_DLY_SW_RST_0));

	siw_hal_cmd_write(dev, CMD_RESET_HIGH);
	siw_hal_cmd_write(dev, CMD_DIS);

	touch_msleep(ts->caps.sw_reset_delay +
		     hal_dbg_delay(chip, HAL_DBG_DLY_SW_RST_1));

	return 0;
}

static int siw_hal_sw_reset_default(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	u32 chk_resp;
	u32 data;
	int ret = 0;

	/******************************************************
	* Siliconworks does not recommend to use SW reset    *
	* due to ist limitation in stability in LG4894.      *
	******************************************************/
	t_dev_info(dev, "SW Reset\n");
	ret = siw_hal_write_value(dev,
				  reg->spr_rst_ctl,
				  7);
	ret = siw_hal_write_value(dev,
				  reg->spr_rst_ctl,
				  0);

	/* Boot Start */
	ret = siw_hal_write_value(dev,
				  reg->spr_boot_ctl,
				  1);

	/* firmware boot done check */
	chk_resp = FLASH_BOOTCHK_VALUE;
	ret = siw_hal_condition_wait(dev, reg->tc_flash_dn_status, &data,
				chk_resp, ~0,
				10 + hal_dbg_delay(chip, HAL_DBG_DLY_SW_RST_0),
				200);
	if (ret < 0) {
		t_dev_err(dev, "failed : boot check(%Xh), %Xh\n",
			  chk_resp, data);
		goto out;
	}

out:
	return ret;
}

static int siw_hal_sw_reset(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int ret = 0;

	siw_touch_irq_control(dev, INTERRUPT_DISABLE);

	atomic_set(&chip->init, IC_INIT_NEED);

	switch (chip->opt.t_sw_rst) {
	case 2:
		ret = siw_hal_sw_reset_type_2(dev);
		break;

	case 1:
		ret = siw_hal_sw_reset_type_1(dev);
		break;

	default:
		ret = siw_hal_sw_reset_default(dev);
		break;
	}

	siw_touch_qd_init_work_sw(ts);

	return ret;
}

static int siw_hal_hw_reset(struct device *dev, int ctrl)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	t_dev_info(dev, "HW Reset(%s)\n",
		   (ctrl == HW_RESET_ASYNC) ? "Async" : "Sync");

	if (ctrl == HW_RESET_ASYNC) {
		siw_hal_reinit(dev, 0,
			hal_dbg_delay(chip, HAL_DBG_DLY_HW_RST_1), 0, NULL);
		siw_touch_qd_init_work_hw(ts);
		return 0;
	}

	siw_hal_reinit(dev, 0, ts->caps.hw_reset_delay, 1, siw_hal_init);

	return 0;
}

static int siw_hal_reset_ctrl(struct device *dev, int ctrl)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int ret = -EINVAL;

	mutex_lock(&ts->reset_lock);

	t_dev_info(dev, "%s reset control(%d)\n",
		   touch_chip_name(ts), ctrl);

	siw_hal_watch_set_rtc_clear(dev);

	switch (ctrl) {
	case SW_RESET:
		ret = siw_hal_sw_reset(dev);
		break;

	case HW_RESET_ASYNC:
	case HW_RESET_SYNC:
		ret = siw_hal_hw_reset(dev, ctrl);
		break;

	default:
		t_dev_err(dev, "unknown reset type, %d\n", ctrl);
		break;
	}

	mutex_unlock(&ts->reset_lock);

	return ret;
}

static int siw_hal_fw_rd_value(struct device *dev,
			       u32 addr, u32 *value)
{
	u32 data;
	int ret;

	ret = siw_hal_read_value(dev, addr, &data);
	if (ret < 0)
		return ret;

	t_dev_dbg_base(dev,
		"FW upgrade: reg rd: addr[%04Xh], value[%08Xh], %d\n",
		addr, data, ret);

	if (value)
		*value = data;

	return 0;
}

static int siw_hal_fw_wr_value(struct device *dev,
			       u32 addr, u32 value)
{
	int ret;

	ret = siw_hal_write_value(dev, addr, value);
	if (ret < 0)
		return ret;

	t_dev_dbg_base(dev,
		"FW upgrade: reg wr: addr[%04Xh], value[%08Xh], %d\n",
		addr, value, ret);

	return 0;
}

static int siw_hal_fw_wr_seq(struct device *dev,
			     u32 addr, u8 *data, int size)
{
	int ret;

	ret = siw_hal_reg_write(dev, addr, (void *)data, size);
	if (ret < 0)
		return ret;

	t_dev_dbg_base(dev,
		"FW upgrade: reg wr: addr[%04Xh], data[%02X ...], %d\n",
		addr, data[0], ret);

	return 0;
}

static int siw_hal_fw_sram_wr_enable(struct device *dev, int onoff)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	u32 data;
	int ret = 0;

	data = (onoff) ? 0x03 : 0x00;
	ret = siw_hal_fw_wr_value(dev, reg->spr_sram_ctl, data);
	if (ret < 0)
		goto out;

out:
	return ret;
}

#define FW_DN_LOG_UNIT	(8<<10)

static int siw_hal_fw_upgrade_fw_core(struct device *dev, u8 *dn_buf,
				      int dn_size)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_reg *reg = chip->reg;
	int is_i2c = !!(touch_bus_type(ts) == BUS_IF_I2C);
	u8 *fw_data;
	int fw_size;
	int fw_pos, curr_size;
	int fw_size_org = dn_size;
	int fw_dn_size = 0, fw_dn_percent;
	int ret = 0;

	fw_data = dn_buf;
	fw_size = dn_size;
	fw_pos = 0;
	while (fw_size) {
		t_dev_dbg_base(dev,
			"FW upgrade: fw_pos[%06Xh ...] = %02X %02X %02X %02X\n",
			fw_pos,
			fw_data[0], fw_data[1], fw_data[2], fw_data[3]);

		curr_size = min(fw_size, MAX_RW_SIZE);

		/* code sram base address write */
		ret = siw_hal_fw_wr_value(dev, reg->spr_code_offset,
					fw_pos >> 2);
		if (ret < 0)
			goto out;

		ret = siw_hal_fw_wr_seq(dev, reg->code_access_addr,
					(void *)fw_data, curr_size);
		if (ret < 0)
			goto out;

		fw_data += curr_size;
		fw_pos += curr_size;
		fw_size -= curr_size;

		/*
		 * Show progress log for slow I2C case
		 */
		if (!is_i2c)
			continue;

		fw_dn_size += curr_size;
		if (!fw_size || !(fw_dn_size & (FW_DN_LOG_UNIT - 1))) {
			fw_dn_percent = (fw_dn_size * 100);
			fw_dn_percent /= fw_size_org;

			t_dev_info(dev, "FW upgrade: downloading...(%d%c)\n",
				   fw_dn_percent, '%');
		}
	}

out:
	return ret;
}

static int siw_hal_fw_upgrade_conf_core(struct device *dev,
					u32 addr, u8 *dn_buf, int dn_size)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	int ret;

	/* conf sram base address write */
	ret = siw_hal_fw_wr_value(dev,
				  reg->serial_data_offset,
				  addr);
	if (ret < 0)
		goto out;

	/* Conf data download to conf sram */
	ret = siw_hal_fw_wr_seq(dev,
				reg->data_i2cbase_addr,
				(void *)dn_buf, dn_size);
	if (ret < 0)
		goto out;

out:
	return ret;
}

#if defined(__SIW_FW_TYPE_1)
/*
 * Common CONF + Specific CONF(s)
 */
enum {
	POW_C_CONF = 9,
	POW_S_CONF = 10,
};

enum {
	NUM_C_CONF = 1,
	MIN_S_CONF = 1,
	MAX_S_CONF = 31,
};

enum {
	MIN_S_CONF_IDX = 1,
	MAX_S_CONF_IDX = (MAX_S_CONF + 1),
};

#define FW_BOOT_LOADER_INIT		(0x74696E69)
#define FW_BOOT_LOADER_CODE		(0x544F4F42)

#define FW_BOOT_CODE_ADDR		(0x044)
#define FW_S_CONF_IDX_ADDR		(0x260)
#define FW_S_CONF_DN_ADDR		(0x267)

#define FW_TYPE_STR		"FW_TYPE_1"

#define FLASH_CONF_DNCHK_VALUE_TYPE_X	(FLASH_CONF_DNCHK_VALUE | 0x0C)
#define FLASH_CONF_SIZE_TYPE_X			(1<<9)

#define FW_POST_QUIRK_DELAY		20
#define FW_POST_QUIRK_COUNT		200

static int siw_hal_fw_upgrade_fw_post_quirk(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	u32 chk_resp, data;
	u32 boot_code_addr = chip->fw.boot_code_addr;
	int ret;

	ret = siw_hal_fw_wr_value(dev, boot_code_addr, FW_BOOT_LOADER_INIT);
	if (ret < 0)
		goto out;

	/* Set Serial Dump Done */
	ret = siw_hal_fw_wr_value(dev, reg->spr_boot_ctl, 1);
	if (ret < 0)
		goto out;

	/* Release CM3 core */
	ret = siw_hal_fw_wr_value(dev, reg->spr_rst_ctl, 0);
	if (ret < 0)
		goto out;

	/* firmware boot done check */
	chk_resp = FW_BOOT_LOADER_CODE;
	ret = siw_hal_condition_wait(dev, boot_code_addr, &data,
		chk_resp, ~0,
		FW_POST_QUIRK_DELAY + hal_dbg_delay(chip, HAL_DBG_DLY_FW_0),
		FW_POST_QUIRK_COUNT);
	if (ret < 0) {
		t_dev_err(dev, "FW upgrade: failed - boot check(%Xh), %08Xh\n",
			  chk_resp, data);
		goto out;
	}

	t_dev_info(dev, "FW upgrade: boot check done\n");

out:
	return ret;
}

static int siw_hal_fw_upgrade_conf_quirk(struct device *dev,
		u8 *fw_buf, int fw_size)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int fw_size_max;
	u32 conf_dn_addr;
	u32 data;
	int ret = 0;

	if (!chip->fw.conf_index)
		goto out;

	fw_size_max = touch_fw_size(ts);

	conf_dn_addr = chip->fw.conf_dn_addr;
	ret = siw_hal_fw_rd_value(dev, conf_dn_addr, &data);
	if (ret < 0)
		goto out;

	conf_dn_addr = (data & 0xFFFF);
	t_dev_dbg_base(dev, "FW upgrade: s_conf_dn_addr %04Xh (%08Xh)\n",
		       conf_dn_addr, data);

	data = fw_size_max +
	       (NUM_C_CONF << POW_C_CONF) +
	       ((chip->fw.conf_index - 1) << POW_S_CONF);
	ret = siw_hal_fw_upgrade_conf_core(dev, conf_dn_addr,
			(u8 *)&fw_buf[data], FLASH_CONF_SIZE);
	if (ret < 0)
		goto out;

out:
	return ret;
}

#define S_CFG_DBG_IDX		0

static void siw_hal_fw_var_init(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_fw_info *fw = &chip->fw;

	fw->conf_index = 0;
	fw->conf_idx_addr = FW_S_CONF_IDX_ADDR;
	fw->conf_dn_addr = FW_S_CONF_DN_ADDR;
	fw->boot_code_addr = FW_BOOT_CODE_ADDR;

	switch (touch_chip_type(ts)) {
	case CHIP_SW49501:
		fw->conf_idx_addr = 0x316;
		fw->conf_dn_addr = 0x31D;
		fw->boot_code_addr = 0x03F;
		break;
	}

	t_dev_info(dev, "FW upgrade: idx %Xh, dn %Xh, code %Xh\n",
		   fw->conf_idx_addr, fw->conf_dn_addr, fw->boot_code_addr);
}

static int siw_hal_fw_size_check(struct device *dev, int fw_size)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int fw_size_max = touch_fw_size(ts);
	int size_min = (fw_size_max + (NUM_C_CONF << POW_C_CONF) +
			(MIN_S_CONF << POW_S_CONF));
	int size_max = (fw_size_max + (NUM_C_CONF << POW_C_CONF) +
			(MAX_S_CONF << POW_S_CONF));

	siw_hal_fw_var_init(dev);

	if ((fw_size < size_min) || (fw_size > size_max)) {
		t_dev_err(dev, "FW upgrade: wrong file size - %Xh,\n",
			  fw_size);
		t_dev_err(dev, "shall be '%Xh <= x <= %Xh'\n",
			  size_min, size_max);
		return -EFAULT;
	}

	return 0;
}

static int siw_hal_fw_size_check_post(struct device *dev, int fw_size)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int fw_size_max = touch_fw_size(ts);
	int required_size;
	u32 index = 0;
	u32 conf_idx_addr = chip->fw.conf_idx_addr;
	int ret = 0;

#if (S_CFG_DBG_IDX != 0)
	index =  S_CFG_DBG_IDX;
	t_dev_warn(dev, "FW upgrade: conf_index fixed for debugging: %d\n",
		index);
#else
	ret = siw_hal_read_value(dev, conf_idx_addr, &index);
#endif
	if (ret < 0) {
		t_dev_err(dev,
			"FW upgrade: failed - conf_index(%04Xh) read, %d\n",
			conf_idx_addr, ret);
		return ret;
	}
	if ((index < MIN_S_CONF_IDX) || (index > MAX_S_CONF_IDX)) {
		t_dev_err(dev, "FW upgrade: failed - wrong cfg index, %d\n",
			index);
		return -EFAULT;
	}
	t_dev_info(dev, "FW upgrade: conf_index: %d\n", index);

	required_size = fw_size_max + (NUM_C_CONF << POW_C_CONF) +
			(index << POW_S_CONF);
	if (fw_size < required_size) {
		t_dev_err(dev, "FW upgrade: wrong file size - %Xh < %Xh,\n",
			  fw_size, required_size);
		return -EFAULT;
	}

	chip->fw.conf_index = index;

	return 0;
}
#else	/* __FW_TYPE_1 */
#define FW_TYPE_STR		"FW_TYPE_0"

#define FLASH_CONF_DNCHK_VALUE_TYPE_X	(FLASH_CONF_DNCHK_VALUE)
#define FLASH_CONF_SIZE_TYPE_X			FLASH_CONF_SIZE

#define FW_POST_QUIRK_DELAY		20
#define FW_POST_QUIRK_COUNT		200

static int siw_hal_fw_upgrade_fw_post_quirk(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	u32 chk_resp, data;
	int ret;

	/* Release CM3 core */
	ret = siw_hal_fw_wr_value(dev, reg->spr_rst_ctl, 0);
	if (ret < 0)
		goto out;

	/* Set Serial Dump Done */
	ret = siw_hal_fw_wr_value(dev, reg->spr_boot_ctl, 1);
	if (ret < 0)
		goto out;

	/* firmware boot done check */
	chk_resp = FLASH_BOOTCHK_VALUE;
	ret = siw_hal_condition_wait(dev, reg->tc_flash_dn_status, &data,
		chk_resp, ~0,
		FW_POST_QUIRK_DELAY + hal_dbg_delay(chip, HAL_DBG_DLY_FW_0),
		FW_POST_QUIRK_COUNT);
	if (ret < 0) {
		t_dev_err(dev, "FW upgrade: failed - boot check(%Xh), %08Xh\n",
			  chk_resp, data);
		goto out;
	}

	t_dev_info(dev, "FW upgrade: boot check done\n");

out:
	return ret;
}

static int siw_hal_fw_upgrade_conf_quirk(struct device *dev,
		u8 *fw_buf, int fw_size)
{
	return 0;
}

static int siw_hal_fw_size_check(struct device *dev, int fw_size)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int fw_size_max = touch_fw_size(ts);

	chip->fw.conf_index = 0;

	if ((fw_size != fw_size_max) &&
	    (fw_size != (fw_size_max + FLASH_CONF_SIZE))) {
		t_dev_err(dev, "FW upgrade: wrong file size - %Xh,\n",
			  fw_size);
		t_dev_err(dev, "            shall be '%Xh' or '%Xh + %Xh'\n",
			  fw_size_max, fw_size_max, FLASH_CONF_SIZE);
		return -EFAULT;
	}

	return 0;
}

static int siw_hal_fw_size_check_post(struct device *dev, int fw_size)
{
	return 0;
}
#endif	/* __FW_TYPE_1 */

enum {
	BIN_VER_OFFSET_POS = 0xE8,
	BIN_VER_EXT_OFFSET_POS = 0xDC,
	BIN_PID_OFFSET_POS = 0xF0,
};

static int siw_hal_fw_compare(struct device *dev, u8 *fw_buf)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_fw_info *fw = &chip->fw;
	struct siw_touch_fquirks *fquirks = touch_fquirks(ts);
	struct siw_hal_tc_version_bin *bin_ver;
	int fw_max_size = touch_fw_size(ts);
	u32 bin_ver_offset = 0;
	u32 bin_ver_ext_offset = 0;
	u32 bin_pid_offset = 0;
	u32 dev_major = 0;
	u32 dev_minor = 0;
	char pid[12] = {0, };
	u32 bin_major = 0;
	u32 bin_minor = 0;
	u32 bin_raw_ext = 0;
	int bin_diff = 0;
	int update = 0;

	if (fquirks->fwup_check) {
		update = fquirks->fwup_check(dev, fw_buf);
		if (update != -EAGAIN) {
			if (update < 0)
				return update;
			goto out;
		}
		update = 0;
	}

	if (fw->version_ext) {
		dev_major = fw->version_ext >> 8;
		dev_minor = fw->version_ext & 0xFF;
	} else {
		dev_major = fw->v.version.major;
		dev_minor = fw->v.version.minor;
	}

	if (atomic_read(&chip->boot) == IC_BOOT_FAIL) {
		update |= (1 << 7);
		goto out;
	}

	if (!dev_major && !dev_minor) {
		t_dev_err(dev,
			"fw can not be 0.0!! Check your panel connection!!\n");
		return 0;
	}

	bin_ver_offset = *((u32 *)&fw_buf[BIN_VER_OFFSET_POS]);
	if (!bin_ver_offset) {
		t_dev_err(dev, "FW compare: zero ver offset\n");
		return -EINVAL;
	}

	if (chip->opt.f_ver_ext)
		bin_ver_ext_offset = *((u32 *)&fw_buf[BIN_VER_EXT_OFFSET_POS]);
	else
		bin_ver_ext_offset = 0;

	if ((fw->version_ext && !bin_ver_ext_offset) ||
	    (!fw->version_ext && bin_ver_ext_offset)) {
		if (!ts->force_fwup) {
			t_dev_warn(dev,
		"FW compare: different version format, use force update %s",
				(fw->version_ext) ? "(ext)" : "");
			return -EINVAL;
		}
		bin_diff = 1;
	}

	bin_pid_offset = *((u32 *)&fw_buf[BIN_PID_OFFSET_POS]);
	if (!bin_pid_offset) {
		t_dev_err(dev, "FW compare: zero pid offset\n");
		return -EINVAL;
	}

	memcpy(pid, &fw_buf[bin_pid_offset], 8);
	t_dev_dbg_base(dev, "pid %s\n", pid);

	if ((bin_ver_offset > fw_max_size) ||
	    (bin_ver_ext_offset > fw_max_size) ||
	    (bin_pid_offset > fw_max_size)) {
		t_dev_err(dev,
"FW compare: invalid offset - ver %06Xh, ver_ext %06Xh pid %06Xh, max %06Xh\n",
bin_ver_offset, bin_ver_ext_offset, bin_pid_offset, fw_max_size);
		return -EINVAL;
	}

	t_dev_dbg_base(dev, "ver %06Xh, ver_ext %06Xh, pid %06Xh\n",
		       bin_ver_offset, bin_ver_ext_offset, bin_pid_offset);

	bin_ver = (struct siw_hal_tc_version_bin *)&fw_buf[bin_ver_offset];
	bin_major = bin_ver->major;
	bin_minor = bin_ver->minor;

	if (bin_ver_ext_offset) {
		if (!bin_ver->ext) {
			t_dev_err(dev, "FW compare: (no ext flag in binary)\n");
			return -EINVAL;
		}

		memcpy(&bin_raw_ext, &fw_buf[bin_ver_ext_offset],
			sizeof(bin_raw_ext));
		bin_major = bin_raw_ext >> 8;
		bin_minor = bin_raw_ext & 0xFF;

		t_dev_info(dev,
			   "FW compare: bin-ver: %08X (%s)(%d)\n",
			   bin_raw_ext, pid, bin_diff);

		if (siw_hal_fw_chk_version_ext(bin_raw_ext,
					       bin_ver->ext) < 0) {
			t_dev_err(dev,
				"FW compare: (invalid extension in binary)\n");
			return -EINVAL;
		}
	} else {
		t_dev_info(dev,
			   "FW compare: bin-ver: %d.%02d (%s)(%d)\n",
			   bin_major, bin_minor, pid, bin_diff);
	}

	if (fw->version_ext) {
		t_dev_info(dev, "FW compare: dev-ver: %08X (%s)\n",
			   fw->version_ext, fw->product_id);
	} else {
		t_dev_info(dev, "FW compare: dev-ver: %d.%02d (%s)\n",
			   dev_major, dev_minor, fw->product_id);
	}

	if (ts->force_fwup)
		update |= (1 << 0);
	else {
		if (bin_major > dev_major)
			update |= (1 << 1);
		else if (bin_major == dev_major) {
			if (bin_minor > dev_minor)
				update |= (1 << 2);
		}
	}

	if (memcmp(pid, fw->product_id, 8)) {
		t_dev_err(dev,
	"FW compare: bin-pid[%s] != dev-pid[%s], halted (up %02X, fup %02X)\n",
	pid, fw->product_id, update, ts->force_fwup);
		return -EINVAL;
	}

out:
	t_dev_info(dev,
		   "FW compare: up %02X, fup %02X\n",
		   update, ts->force_fwup);

	return update;
}

#if defined(__FW_VERIFY_TEST)
static int __siw_hal_fw_up_verify(struct device *dev, u8 *chk_buf,
				int chk_size)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	u8 *fw_rd_data, *fw_data;
	u8 *r_data, *w_data;
	int fw_size;
	int fw_pos, curr_size;
	int i;
	int ret = 0;

	fw_size = chk_size;

	fw_rd_data = kmalloc(fw_size, GFP_KERNEL);
	if (!fw_rd_data) {
		ret = -ENOMEM;
		goto out;
	}

	fw_data = fw_rd_data;
	fw_pos = 0;
	while (fw_size && fw_data) {
		curr_size = min(fw_size, MAX_RW_SIZE);

		/* code sram base address write */
		ret = siw_hal_write_value(dev, reg->spr_code_offset,
					fw_pos >> 2);
		if (ret < 0)
			goto out_free;

		ret = siw_hal_reg_read(dev, reg->code_access_addr,
				       (void *)fw_data, curr_size);
		if (ret < 0)
			goto out_free;

		fw_data += curr_size;
		fw_pos += curr_size;
		fw_size -= curr_size;
	}

	r_data = fw_rd_data;
	w_data = chk_buf;
	fw_size = chk_size;
	for (i = 0; i < fw_size; i++) {
		if ((*r_data) != (*w_data)) {
			t_dev_err(dev, "* Err [%06X] rd(%02X) != wr(%02X)\n",
				  i, (*r_data), (*w_data));
			ret = -EFAULT;
		}

		r_data++;
		w_data++;
	}

	if (ret >= 0)
		t_dev_info(dev, "FW dn verified\n");

out_free:
	kfree(fw_rd_data);

out:
	return ret;
}
#else	/* __FW_VERIFY_TEST */
static int __siw_hal_fw_up_verify(struct device *dev, u8 *chk_buf,
				int chk_size)
{
	return 0;
}
#endif	/* __FW_VERIFY_TEST */

static int siw_hal_fw_upgrade_fw_pre(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	int ret;

	/* Reset CM3 core */
	ret = siw_hal_fw_wr_value(dev, reg->spr_rst_ctl, 2);
	if (ret < 0)
		goto out;

	/* Disable SRAM write protection */
	ret = siw_hal_fw_sram_wr_enable(dev, 1);
	if (ret < 0)
		goto out;

out:
	return ret;
}

#define	FW_POST_DELAY		20
#define FW_POST_COUNT		200

static int siw_hal_fw_upgrade_fw_post(struct device *dev, int fw_size)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_reg *reg = chip->reg;
	u32 dn_cmd, chk_resp, data;
	int ret;

	/* Enable SRAM write protection */
	ret = siw_hal_fw_sram_wr_enable(dev, 0);
	if (ret < 0)
		goto out;

	ret = siw_hal_fw_upgrade_fw_post_quirk(dev);
	if (ret < 0)
		goto out;

	ret = siw_hal_fw_size_check_post(dev, fw_size);
	if (ret < 0)
		goto out;

	/* Firmware Download Start */
	dn_cmd = (FLASH_KEY_CODE_CMD << 16) | 1;
	ret = siw_hal_fw_wr_value(dev, reg->tc_flash_dn_ctl, dn_cmd);
	if (ret < 0)
		goto out;

	touch_msleep(ts->caps.hw_reset_delay);

	/* download check */
	chk_resp = FLASH_CODE_DNCHK_VALUE;
	ret = siw_hal_condition_wait(dev, reg->tc_flash_dn_status, &data,
		chk_resp, 0xFFFF,
		FW_POST_DELAY + hal_dbg_delay(chip, HAL_DBG_DLY_FW_1),
		FW_POST_COUNT);
	if (ret < 0) {
		t_dev_err(dev, "FW upgrade: failed - code check(%Xh), %08Xh\n",
			  chk_resp, data);
		goto out;
	}
	t_dev_info(dev, "FW upgrade: code check done\n");

out:
	return ret;
}

static int siw_hal_fw_upgrade_fw(struct device *dev,
				 u8 *fw_buf, int fw_size)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	u8 *fw_data;
	int fw_size_max;
	int ret = 0;

	/*
	 * Stage 1-1 : download code data
	 */
	fw_size_max = touch_fw_size(ts);

	ret = siw_hal_fw_upgrade_fw_pre(dev);
	if (ret < 0)
		goto out;

	/*
	 * [Caution]
	 * The size for F/W upgrade is fw_size_max, not fw->size
	 * because the fw file can have config area.
	 */
	fw_data = fw_buf;
	ret = siw_hal_fw_upgrade_fw_core(dev, fw_data, fw_size_max);
	if (ret < 0)
		goto out;

	ret = __siw_hal_fw_up_verify(dev, fw_data, fw_size_max);
	if (ret < 0)
		goto out;

	/*
	 * Stage 1-2: upgrade code data
	 */
	ret = siw_hal_fw_upgrade_fw_post(dev, fw_size);
	if (ret < 0)
		goto out;

out:
	return ret;
}

static int siw_hal_fw_upgrade_conf_pre(struct device *dev, u32 *value)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	int data = 0;
	int ret;

	ret = siw_hal_fw_rd_value(dev, reg->tc_confdn_base_addr, &data);
	if (ret < 0)
		goto out;

out:
	if (value)
		*value = data;

	return ret;
}

#define	CONF_POST_DELAY		20
#define CONF_POST_COUNT		200

static int siw_hal_fw_upgrade_conf_post(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	u32 dn_cmd, chk_resp, data;
	int ret;

	/* Conf Download Start */
	dn_cmd = (FLASH_KEY_CONF_CMD << 16) | 2;
	ret = siw_hal_fw_wr_value(dev, reg->tc_flash_dn_ctl, dn_cmd);
	if (ret < 0)
		goto out;

	/* Conf check */
	chk_resp = FLASH_CONF_DNCHK_VALUE_TYPE_X;
	ret = siw_hal_condition_wait(dev, reg->tc_flash_dn_status, &data,
		chk_resp, 0xFFFF,
		CONF_POST_DELAY + hal_dbg_delay(chip, HAL_DBG_DLY_FW_2),
		CONF_POST_COUNT);
	if (ret < 0) {
		t_dev_err(dev, "FW upgrade: failed - conf check(%Xh), %X\n",
			  chk_resp, data);
		ret = -EPERM;
		goto out;
	}

	t_dev_info(dev, "FW upgrade: conf check done\n");

	/* Release & Reset CM3 */
	ret = siw_hal_fw_wr_value(dev, reg->spr_rst_ctl, 1);
	if (ret < 0)
		goto out;

out:
	return ret;
}

static int siw_hal_fw_upgrade_conf(struct device *dev,
				   u8 *fw_buf, int fw_size)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int fw_size_max;
	u32 conf_dn_addr;
	u32 data;
	int ret;

	fw_size_max = touch_fw_size(ts);

	/*
	 * Stage 2-1: download config data
	 */
	ret = siw_hal_fw_upgrade_conf_pre(dev, &data);
	if (ret < 0)
		goto out;

	conf_dn_addr = ((data >> 16) & 0xFFFF);

	t_dev_dbg_base(dev, "FW upgrade: conf_dn_addr %04Xh (%08Xh)\n",
		       conf_dn_addr, data);

	/* C_CFG */
	ret = siw_hal_fw_upgrade_conf_core(dev, conf_dn_addr,
			(u8 *)&fw_buf[fw_size_max], FLASH_CONF_SIZE_TYPE_X);
	if (ret < 0)
		goto out;

	ret = siw_hal_fw_upgrade_conf_quirk(dev, fw_buf, fw_size);
	if (ret < 0)
		goto out;

	/*
	 * Stage 2-2: upgrade config data
	 */
	ret = siw_hal_fw_upgrade_conf_post(dev);
	if (ret < 0)
		goto out;

out:
	return ret;
}

static int siw_hal_fw_upgrade(struct device *dev,
			      u8 *fw_buf, int fw_size, int retry)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_touch_fquirks *fquirks = touch_fquirks(ts);
	int fw_size_max;
	u32 include_conf;
	int ret = 0;

	t_dev_info(dev, "===== FW upgrade: start (%d) =====\n", retry);

	if (fquirks->fwup_upgrade) {
		ret = fquirks->fwup_upgrade(dev, fw_buf, fw_size, retry);
		if (ret < 0)
			goto out;
		goto out_done;
	}

	fw_size_max = touch_fw_size(ts);

	ret = siw_hal_fw_size_check(dev, fw_size);
	if (ret < 0)
		goto out;

	include_conf = !!(fw_size > fw_size_max);
	t_dev_info(dev, "FW upgrade:%s include conf data\n",
		   (include_conf) ? "" : " not");

	t_dev_dbg_base(dev, "FW upgrade: fw size %08Xh, fw_size_max %08Xh\n",
		       fw_size, fw_size_max);

	ret = siw_hal_fw_upgrade_fw(dev, fw_buf, fw_size);
	if (ret < 0)
		goto out;

	if (include_conf) {
		ret = siw_hal_fw_upgrade_conf(dev, fw_buf, fw_size);
		if (ret < 0)
			goto out;
	}

out_done:
	t_dev_info(dev, "===== FW upgrade: done (%d) =====\n", retry);

out:
	return ret;
}

static int siw_hal_fw_do_get_file(const struct firmware **fw_p,
				  const char *name,
				  struct device *dev,
				  int abs_path)
{
	if (abs_path) {
		t_dev_err(dev, "get FW in abs path is not support\n");
		return -EINVAL;
	}

	return request_firmware(fw_p, name, dev);
}

static int siw_hal_fw_get_file(const struct firmware **fw_p,
			       char *fwpath,
			       struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	const struct firmware *fw = NULL;
	char *src_path;
	int src_len;
	int abs_path = 0;
	int ret = 0;

	if (atomic_read(&ts->state.fb) >= FB_SUSPEND) {
		t_dev_warn(dev, "state.fb is not FB_RESUME\n");
		ret = -EPERM;
		goto out;
	}

	if (ts->test_fwpath[0])
		src_path = (char *)ts->test_fwpath;
	else if (ts->def_fwcnt)
		src_path = (char *)ts->def_fwpath[0];
	else {
		t_dev_err(dev, "no target fw defined\n");
		ret = -ENOENT;
		goto out;
	}

	/*
	 * Absolute path option
	 * ex) echo {root}/.../target_fw_img > fw_upgrade
	 *          ++++++~~~~~~~~~~~~~~~~~~
	 *          flag  |
	 *                absolute path
	 */
	src_len = strlen(src_path);
	if (strncmp(src_path, "{root}", 6) == 0) {
		abs_path = 1;
		src_path += 6;
		src_len -= 6;
	}
	chip->fw_abs_path = abs_path;

	strlcpy(fwpath, src_path, src_len);
	fwpath[src_len] = 0;

	t_dev_info(dev, "target fw: %s (%s)\n",
		   fwpath,
		   (abs_path) ? "abs" : "rel");

	ret = siw_hal_fw_do_get_file(&fw,
				     (const char *)fwpath,
				     dev, abs_path);
	if (ret < 0) {
		if (ret == -ENOENT)
			t_dev_err(dev, "can't find fw: %s\n", fwpath);
		else {
			t_dev_err(dev, "can't %s fw: %s, %d\n",
				  (abs_path) ? "read" : "request",
				  fwpath, ret);
		}
		goto out;
	}

	if (fw_p)
		*fw_p = fw;

out:
	return ret;
}

static void siw_hal_fw_release_firm(struct device *dev,
				    const struct firmware *fw)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);

	if (chip->fw_abs_path) {
		chip->fw_abs_path = 0;
		kfree(fw->data);
		kfree(fw);
		return;
	}

	release_firmware(fw);
}

/*
 * FW upgrade option
 *
 * 1. If TOUCH_USE_FW_BINARY used
 * 1-1 Default upgrade (through version comparison)
 *     do upgarde using binary header link
 * 1-2 echo {bin} > fw_upgrade
 *     do force-upgrade using binary header link (same as 1-1)
 * 1-3 echo /.../fw_img > fw_upgrade
 *     do force-upgrade using request_firmware (relative path)
 * 1-4 echo {root}/.../fw_img > fw_upgrade
 *     do force-upgrade using normal file open control (absolute path)
 *
 * 2. Else
 * 2-1 Default upgrade (through version comparison)
 *     do upgarde using request_firmware (relative path)
 * 2-2 echo /.../fw_img > fw_upgrade
 *     do force-upgrade using request_firmware (relative path)
 * 2-3 echo {root}/.../fw_img > fw_upgrade
 *     do force-upgrade using normal file open control (absolute path)
 */
static int siw_hal_upgrade_not_allowed(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	if (atomic_read(&chip->init) == IC_INIT_NEED) {
		t_dev_warn(dev, "Not Ready, Need IC init (fw)\n");
		return 1;
	}

	if (atomic_read(&ts->state.fb) >= FB_SUSPEND) {
		t_dev_warn(dev, "state.fb is not FB_RESUME\n");
		return 1;
	}

	return 0;
}

static int siw_hal_upgrade(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_touch_fw_bin *fw_bin = NULL;
	const struct firmware *fw = NULL;
	char *fwpath = NULL;
	u8 *fw_buf = NULL;
	int fw_max_size = touch_fw_size(ts);
	int fw_size = 0;
	int fw_up_binary = 0;
	int i = 0;
	int ret_val = 0;
	int ret = 0;

	chip->fw_abs_path = 0;

	t_dev_info(dev, "fw type: %s\n", FW_TYPE_STR);

	if (siw_hal_upgrade_not_allowed(dev))
		return -EPERM;

	fwpath = touch_getname();
	if (fwpath == NULL) {
		t_dev_err(dev, "failed to allocate name buffer - fwpath\n");
		return -ENOMEM;
	}

	if (touch_flags(ts) & TOUCH_USE_FW_BINARY) {
		fw_up_binary = 1;

		if (ts->force_fwup & FORCE_FWUP_SYS_STORE) {
			switch (ts->test_fwpath[0]) {
			case 0:
			/* fall through */
			case ' ':	/* ignore space */
				break;

			default:
				/* if target string is not "{bin}" */
				if (strncmp(ts->test_fwpath, "{bin}", 5) != 0)
					fw_up_binary = 0;
				break;
			}
		}
	}

	if (fw_up_binary) {
		t_dev_info(dev, "getting fw from binary header data\n");
		fw_bin = touch_fw_bin(ts);
		if (fw_bin != NULL) {
			fw_buf = fw_bin->fw_data;
			fw_size = fw_bin->fw_size;
		} else
			t_dev_warn(dev, "empty fw info\n");
	} else {
		t_dev_info(dev, "getting fw from file\n");
		ret = siw_hal_fw_get_file(&fw, fwpath, dev);
		if (ret < 0)
			goto out;
		fw_buf = (u8 *)fw->data;
		fw_size = (int)fw->size;
	}

	ret = -EPERM;

	if ((fw_buf == NULL) || !fw_size) {
		t_dev_err(dev, "invalid fw info\n");
		goto out;
	}

	if (fw_size < fw_max_size) {
		t_dev_err(dev, "invalid fw size: %Xh < %Xh\n",
			  fw_size, fw_max_size);
		goto out;
	}

	t_dev_info(dev, "fw size: %d\n", fw_size);

	ret_val = siw_hal_fw_compare(dev, fw_buf);
	if (ret_val < 0)
		ret = ret_val;
	else if (ret_val) {
		touch_msleep(200);
		for (i = 0; i < 2 && ret; i++)
			ret = siw_hal_fw_upgrade(dev, fw_buf, fw_size, i);
	}

	if (fw)
		siw_hal_fw_release_firm(dev, fw);

out:
	if (ret) {
		siwmon_submit_ops_step_chip_wh_name(dev,
			"%s - FW upgrade halted", touch_chip_name(ts), ret);
	} else {
		siwmon_submit_ops_step_chip_wh_name(dev,
			"%s - FW upgrade done", touch_chip_name(ts), ret);
	}

	touch_putname(fwpath);

	return ret;
}

#if defined(__SIW_CONFIG_KNOCK)
static void siw_hal_set_tci_debug_type_1(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	u32 wdata = chip->tci_debug_type;
	int ret = 0;

	t_dev_info(dev, "TCI-Debug: %s\n", (wdata) ? "Enable" : "Disable");

	ret = siw_hal_write_value(dev, reg->tci_debug_fail_ctrl, wdata);
	if (ret < 0)
		t_dev_err(dev, "TCI-Debug: ctrl failed, %d\n", ret);
}

static void siw_hal_set_tci_debug(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);

	switch (chip->opt.t_chk_tci_debug) {
	case 1:
		siw_hal_set_tci_debug_type_1(dev);
		break;
	default:
		break;
	}
}

static int siw_hal_tci_knock(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_reg *reg = chip->reg;
	struct tci_info *info1 = &ts->tci.info[TCI_1];
	struct tci_info *info2 = &ts->tci.info[TCI_2];
	u32 lpwg_data[7];
	int ret = 0;

	siw_hal_set_tci_debug(dev);

	lpwg_data[0] = ts->tci.mode;
	lpwg_data[1] = info1->tap_count | (info2->tap_count << 16);
	lpwg_data[2] = info1->min_intertap | (info2->min_intertap << 16);
	lpwg_data[3] = info1->max_intertap | (info2->max_intertap << 16);
	lpwg_data[4] = info1->touch_slop | (info2->touch_slop << 16);
	lpwg_data[5] = info1->tap_distance | (info2->tap_distance << 16);
	lpwg_data[6] = info1->intr_delay | (info2->intr_delay << 16);

	t_dev_dbg_base(dev, "lpwg_data[0] : %08Xh\n", lpwg_data[0]);
	t_dev_dbg_base(dev, "lpwg_data[1] : %08Xh\n", lpwg_data[1]);
	t_dev_dbg_base(dev, "lpwg_data[2] : %08Xh\n", lpwg_data[2]);
	t_dev_dbg_base(dev, "lpwg_data[3] : %08Xh\n", lpwg_data[3]);
	t_dev_dbg_base(dev, "lpwg_data[4] : %08Xh\n", lpwg_data[4]);
	t_dev_dbg_base(dev, "lpwg_data[5] : %08Xh\n", lpwg_data[5]);
	t_dev_dbg_base(dev, "lpwg_data[6] : %08Xh\n", lpwg_data[6]);

	ret = siw_hal_reg_write(dev,
				reg->tci_enable_w,
				(void *)lpwg_data, sizeof(lpwg_data));

	return ret;
}

static int siw_hal_tci_password(struct device *dev)
{
	siw_hal_set_tci_debug(dev);

	return siw_hal_tci_knock(dev);
}

static int siw_hal_do_tci_active_area(struct device *dev,
				      u32 x1, u32 y1, u32 x2, u32 y2)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	int ret = 0;

	ret = siw_hal_write_value(dev,
				  reg->act_area_x1_w,
				  x1);
	if (ret < 0)
		goto out;
	ret = siw_hal_write_value(dev,
				  reg->act_area_y1_w,
				  y1);
	if (ret < 0)
		goto out;
	ret = siw_hal_write_value(dev,
				  reg->act_area_x2_w,
				  x2);
	if (ret < 0)
		goto out;
	ret = siw_hal_write_value(dev,
				  reg->act_area_y2_w,
				  y2);
	if (ret < 0)
		goto out;

out:
	return ret;
}

static int siw_hal_tci_active_area(struct device *dev,
				   u32 x1, u32 y1, u32 x2, u32 y2, int type)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int margin = touch_senseless_margin(ts);
	u32 area[4] = { 0, };
	int i;

	t_dev_info(dev,
		"tci_active_area[%d]: x1[%Xh], y1[%Xh], x2[%Xh], y2[%Xh]\n",
		type, x1, y1, x2, y2);

	area[0] = (x1 + margin) & 0xFFFF;
	area[1] = (y1 + margin) & 0xFFFF;
	area[2] = (x2 - margin) & 0xFFFF;
	area[3] = (y2 - margin) & 0xFFFF;

	for (i = 0; i < ARRAY_SIZE(area); i++)
		area[i] = (area[i]) | (area[i] << 16);

	return siw_hal_do_tci_active_area(dev,
		area[0], area[1], area[2], area[3]);
}

static int siw_hal_tci_area_set(struct device *dev, int cover_status)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct reset_area *qcover;
	const char *msg;
	int qcover_invalid = 0;

	if (touch_mode_not_allowed(ts, LCD_MODE_U3_QUICKCOVER))
		return 0;

	qcover = (cover_status == QUICKCOVER_CLOSE) ?
		 &ts->tci.qcover_close : &ts->tci.qcover_open;
	msg = (cover_status == QUICKCOVER_CLOSE) ?
	      "close" : "open";

	if (!qcover->x2 || !qcover->y2) {
		/* deactivated */
		return 0;
	}

	qcover_invalid |= (qcover->x1 >= qcover->x2);
	qcover_invalid |= (qcover->y1 >= qcover->y2);

	if (!qcover_invalid) {
		siw_hal_tci_active_area(dev,
					qcover->x1, qcover->y1,
					qcover->x2, qcover->y2,
					0);
	}

	t_dev_info(dev,
		   "lpwg active area - qcover %s %4d %4d %4d %4d%s\n",
		   msg, qcover->x1, qcover->y1, qcover->x2, qcover->y2,
		   (qcover_invalid) ? " (invalid)" : "");

	return 0;
}

static int siw_hal_tci_control(struct device *dev, int type)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_reg *reg = chip->reg;
	struct tci_ctrl *tci = &ts->tci;
	struct active_area *area = &tci->area;
	struct tci_info *info1 = &tci->info[TCI_1];
	struct tci_info *info2 = &tci->info[TCI_2];
	u32 reg_w = ~0;
	u32 data;
	int ret = 0;

	switch (type) {
	case ENABLE_CTRL:
		reg_w = reg->tci_enable_w;
		data = tci->mode;
		break;

	case TAP_COUNT_CTRL:
		reg_w = reg->tap_count_w;
		data = info1->tap_count | (info2->tap_count << 16);
		break;

	case MIN_INTERTAP_CTRL:
		reg_w = reg->min_intertap_w;
		data = info1->min_intertap | (info2->min_intertap << 16);
		break;

	case MAX_INTERTAP_CTRL:
		reg_w = reg->max_intertap_w;
		data = info1->max_intertap | (info2->max_intertap << 16);
		break;

	case TOUCH_SLOP_CTRL:
		reg_w = reg->touch_slop_w;
		data = info1->touch_slop | (info2->touch_slop << 16);
		break;

	case TAP_DISTANCE_CTRL:
		reg_w = reg->tap_distance_w;
		data = info1->tap_distance | (info2->tap_distance << 16);
		break;

	case INTERRUPT_DELAY_CTRL:
		reg_w = reg->int_delay_w;
		data = info1->intr_delay | (info2->intr_delay << 16);
		break;

	case ACTIVE_AREA_CTRL:
		ret = siw_hal_tci_active_area(dev,
					      area->x1, area->y1,
					      area->x2, area->y2,
					      type);
		break;

	default:
		break;
	}

	if (reg_w != ~0) {
		ret = siw_hal_write_value(dev,
					  reg_w,
					  data);
	}

	return ret;
}

static int siw_hal_lpwg_control(struct device *dev, int mode)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct tci_info *info1 = &ts->tci.info[TCI_1];
	int ret = 0;

	switch (mode) {
	case LPWG_DOUBLE_TAP:
		ts->tci.mode = 0x01;
		info1->intr_delay = 0;
		info1->tap_distance = 10;

		if (touch_senseless_margin(ts)) {
			ret = siw_hal_tci_control(dev, ACTIVE_AREA_CTRL);
			if (ret < 0)
				break;
		}

		ret = siw_hal_tci_knock(dev);
		break;

	case LPWG_PASSWORD:
		ts->tci.mode = 0x01 | (0x01 << 16);
		info1->intr_delay = ts->tci.double_tap_check ? 68 : 0;
		info1->tap_distance = 7;

		if (touch_senseless_margin(ts)) {
			ret = siw_hal_tci_control(dev, ACTIVE_AREA_CTRL);
			if (ret < 0)
				break;
		}

		ret = siw_hal_tci_password(dev);
		break;

	default:
		ts->tci.mode = 0;
		ret = siw_hal_tci_control(dev, ENABLE_CTRL);
		break;
	}

	t_dev_info(dev, "lpwg_control mode = %d\n", mode);

	return ret;
}
#else	/* __SIW_SUPPORT_KNOCK */
static int siw_hal_tci_area_set(struct device *dev, int cover_status)
{
	return 0;
}
static int siw_hal_lpwg_control(struct device *dev, int mode)
{
	return 0;
}
#endif	/* __SIW_SUPPORT_KNOCK */

#define SIW_OSC_CTL_T2		0xFE1
#define SIW_CLK_CTL_T2		0xFE2

struct siw_clock_setup {
	const int count;
	const u32 *data;
};

static int siw_hal_clock_osc_base(struct device *dev,
		const struct siw_clock_setup *setup, int onoff)
{
	u32 value;
	int i;

	if (setup == NULL) {
		t_dev_info(dev, "osc %s\n",
			   (onoff) ? "on" : "off");
		siw_hal_write_value(dev, SIW_OSC_CTL_T2, !!onoff);
		return 0;
	}

	for (i = 0; i < setup->count; i++) {
		value = setup->data[i];
		t_dev_info(dev, "osc %s : %Xh\n",
			   (onoff) ? "on" : "off", value);
		siw_hal_write_value(dev, SIW_OSC_CTL_T2, value);
		touch_msleep(1);
	}

	return 0;
}

static const u32 osc_setup_data_on_base[3] = {
	0x01, 0x03, 0x07,
};

static const u32 osc_setup_data_off_base[3] = {
	0x03, 0x01, 0x00,
};

static const struct siw_clock_setup osc_setup_spi_on_base = {
	.count = 3,
	.data = osc_setup_data_on_base,
};

static const struct siw_clock_setup osc_setup_spi_off_base = {
	.count = 3,
	.data = osc_setup_data_off_base,
};

static const struct siw_clock_setup osc_setup_i2c_on_base = {
	.count = 1,
	.data = &osc_setup_data_on_base[2],
};

static const struct siw_clock_setup osc_setup_i2c_off_base = {
	.count = 1,
	.data = &osc_setup_data_off_base[0],
};

static int siw_hal_clock_type_2_osc(struct device *dev, bool onoff)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int is_spi = !!(touch_bus_type(ts) == BUS_IF_SPI);
	const struct siw_clock_setup *osc_setup = NULL;

	switch (touch_chip_type(ts)) {
	case CHIP_SW49501:
		if (onoff) {
			osc_setup = (is_spi) ? &osc_setup_spi_on_base :
				    &osc_setup_i2c_on_base;
		} else {
			osc_setup = (is_spi) ? &osc_setup_spi_off_base :
				    &osc_setup_i2c_off_base;
		}
		break;
	}

	if (onoff) {
		/* I2C needs touch reset. */
		if (!is_spi)
			return 0;
	}

	siw_hal_clock_osc_base(dev, osc_setup, onoff);

	return 0;
}

static int siw_hal_clock_type_2_clk(struct device *dev, bool onoff)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int is_spi = !!(touch_bus_type(ts) == BUS_IF_SPI);

	if (!is_spi)
		return 0;

	siw_hal_write_value(dev, SIW_CLK_CTL_T2, !!onoff);

	return 0;
}

static int siw_hal_clock_type_2(struct device *dev, bool onoff)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	if (onoff) {
		/*
		 * [Notice]
		 * I2C needs touch reset.
		 */
		siw_hal_clock_type_2_osc(dev, onoff);
		siw_hal_clock_type_2_clk(dev, onoff);
		atomic_set(&ts->state.sleep, IC_NORMAL);
	} else {
		if (chip->lcd_mode == LCD_MODE_U0) {
			siw_hal_clock_type_2_clk(dev, onoff);
			siw_hal_clock_type_2_osc(dev, onoff);
			atomic_set(&ts->state.sleep, IC_DEEP_SLEEP);
		}
	}

	t_dev_info(dev, "siw_hal_clock(2) -> %s\n",
		   (onoff) ? "ON" : (!chip->lcd_mode) ? "OFF" : "SKIP");

	return 0;
}

static int siw_hal_clock_type_1(struct device *dev, bool onoff)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	siw_hal_cmd_write(dev, CMD_ENA);

	if (onoff) {
		siw_hal_cmd_write(dev, CMD_OSC_ON);
		siw_hal_cmd_write(dev, CMD_CLK_ON);
		atomic_set(&ts->state.sleep, IC_NORMAL);
	} else {
		if (chip->lcd_mode == LCD_MODE_U0) {
			siw_hal_cmd_write(dev, CMD_CLK_OFF);
			siw_hal_cmd_write(dev, CMD_OSC_OFF);
			atomic_set(&ts->state.sleep, IC_DEEP_SLEEP);
		}
	}

	siw_hal_cmd_write(dev, CMD_DIS);

	t_dev_info(dev, "siw_hal_clock(1) -> %s\n",
		   (onoff) ? "ON" : (!chip->lcd_mode) ? "OFF" : "SKIP");

	return 0;
}

static int siw_hal_clock(struct device *dev, bool onoff)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int state = atomic_read(&ts->state.sleep);
	int ret = 0;

	if (onoff) {
		if (state != IC_DEEP_SLEEP)
			return 0;
	} else {
		if (state == IC_DEEP_SLEEP) {
			t_dev_info(dev, "already sleep state\n");
			return 0;
		}
	}

	siw_touch_sys_osc(dev, onoff);

	switch (chip->opt.t_clock) {
	case 2:
		ret = siw_hal_clock_type_2(dev, onoff);
		break;
	case 1:
		ret = siw_hal_clock_type_1(dev, onoff);
		break;
	default:
		atomic_set(&ts->state.sleep,
			   (onoff) ? IC_NORMAL : IC_DEEP_SLEEP);
		t_dev_info(dev, "sleep state -> %s\n",
			   (onoff) ? "IC_NORMAL" : "IC_DEEP_SLEEP");
		break;
	}

	return ret;
}

#if defined(__SIW_CONFIG_SWIPE)
static int siw_hal_swipe_active_area(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	struct siw_hal_swipe_info *left = &chip->swipe.info[SWIPE_L];
	struct siw_hal_swipe_info *right = &chip->swipe.info[SWIPE_R];
	u32 active_area[4] = {0x0, };
	int ret = 0;

	active_area[0] = (right->area.x1) | (left->area.x1 << 16);
	active_area[1] = (right->area.y1) | (left->area.y1 << 16);
	active_area[2] = (right->area.x2) | (left->area.x2 << 16);
	active_area[3] = (right->area.y2) | (left->area.y2 << 16);

	ret = siw_hal_reg_write(dev,
				reg->swipe_act_area_x1_w,
				(void *)active_area, sizeof(active_area));

	return ret;
}

static int siw_hal_swipe_control(struct device *dev, int type)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	struct siw_hal_swipe_info *left = &chip->swipe.info[SWIPE_L];
	struct siw_hal_swipe_info *right = &chip->swipe.info[SWIPE_R];
	u32 reg_w = ~0;
	u32 data = 0;
	int ret = 0;

	switch (type) {
	case SWIPE_ENABLE_CTRL:
	case SWIPE_DISABLE_CTRL:
		reg_w = reg->swipe_enable_w;
		data = (type == SWIPE_ENABLE_CTRL) ?
		       chip->swipe.mode : 0;
		break;
	case SWIPE_DIST_CTRL:
		reg_w = reg->swipe_dist_w;
		data = (right->distance) | (left->distance << 16);
		break;
	case SWIPE_RATIO_THR_CTRL:
		reg_w = reg->swipe_ratio_thr_w;
		data = (right->ratio_thres) | (left->ratio_thres << 16);
		break;
	case SWIPE_RATIO_PERIOD_CTRL:
		reg_w = reg->swipe_ratio_period_w;
		data = (right->ratio_period) | (left->ratio_period << 16);
		break;
	case SWIPE_RATIO_DIST_CTRL:
		reg_w = reg->swipe_ratio_dist_w;
		data = (right->ratio_distance) |
		       (left->ratio_distance << 16);
		break;
	case SWIPE_TIME_MIN_CTRL:
		reg_w = reg->swipe_time_min_w;
		data = (right->min_time) | (left->min_time << 16);
		break;
	case SWIPE_TIME_MAX_CTRL:
		reg_w = reg->swipe_time_max_w;
		data = (right->max_time) | (left->max_time << 16);
		break;
	case SWIPE_AREA_CTRL:
		ret = siw_hal_swipe_active_area(dev);
		break;
	default:
		break;
	}

	if (reg_w != ~0)
		ret = siw_hal_write_value(dev, reg_w, data);

	return ret;
}

static int siw_hal_swipe_mode(struct device *dev, int mode)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	struct siw_hal_swipe_info *left = &chip->swipe.info[SWIPE_L];
	struct siw_hal_swipe_info *right = &chip->swipe.info[SWIPE_R];
	u32 swipe_data[11] = {0x0, };
	int ret = 0;

	if (!chip->swipe.mode || (mode != LCD_MODE_U2)) {
		ret = siw_hal_swipe_control(dev, SWIPE_DISABLE_CTRL);
		t_dev_dbg_base(dev, "swipe disabled\n");
		goto out;
	}

	swipe_data[0] = chip->swipe.mode;
	swipe_data[1] = (right->distance) | (left->distance << 16);
	swipe_data[2] = (right->ratio_thres) | (left->ratio_thres << 16);
	swipe_data[3] = (right->ratio_distance) | (left->ratio_distance << 16);
	swipe_data[4] = (right->ratio_period) | (left->ratio_period << 16);
	swipe_data[5] = (right->min_time) | (left->min_time << 16);
	swipe_data[6] = (right->max_time) | (left->max_time << 16);
	swipe_data[7] = (right->area.x1) | (left->area.x1 << 16);
	swipe_data[8] = (right->area.y1) | (left->area.y1 << 16);
	swipe_data[9] = (right->area.x2) | (left->area.x2 << 16);
	swipe_data[10] = (right->area.y2) | (left->area.y2 << 16);

	ret = siw_hal_reg_write(dev,
				reg->swipe_enable_w,
				(void *)swipe_data, sizeof(swipe_data));
	if (ret >= 0)
		t_dev_info(dev, "swipe enabled\n");

out:
	return ret;
}
#else	/* __SIW_CONFIG_SWIPE */
static int siw_hal_swipe_mode(struct device *dev, int mode)
{
	return 0;
}
#endif	/* __SIW_CONFIG_SWIPE */

static int siw_hal_tc_con_type_g(struct device *dev, u32 addr, int value,
				 char *name)
{
	int ret = 0;

	ret = siw_hal_write_value(dev, addr, value);
	if (ret < 0) {
		t_dev_err(dev, "failed to set %s[%04Xh], %d\n",
			  name, addr, ret);
		goto out;
	}

	t_dev_info(dev, "%s[%04Xh]: %s(%08Xh)\n",
		   name, addr,
		   (value & 0x1) ? "ON" : "OFF",
		   value);

out:
	return ret;
}

static int siw_hal_tc_con_glove(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_reg *reg = chip->reg;
	u32 addr = reg->glove_en;
	int value = atomic_read(&ts->state.glove);
	int ret = 0;

	if (chip->opt.f_glove_en)
		ret = siw_hal_tc_con_type_g(dev, addr, value, "glove_en");

	return ret;
}

static int siw_hal_tc_con_grab(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_reg *reg = chip->reg;
	u32 addr = reg->grab_en;
	int value = atomic_read(&ts->state.grab);
	int ret = 0;

	if (chip->opt.f_grab_en)
		ret = siw_hal_tc_con_type_g(dev, addr, value, "grab_en");

	return ret;
}

static int siw_hal_tc_con(struct device *dev, u32 code, void *param)
{
	int ret = 0;

	switch (code) {
	case TCON_GLOVE:
		ret = siw_hal_tc_con_glove(dev);
		break;
	case TCON_GRAB:
		ret = siw_hal_tc_con_grab(dev);
		break;
	}

	return ret;
}

static inline int __used siw_hal_tc_driving_u0(struct device *dev)
{
	return TC_DRIVE_CTL_START;
}

static inline int __used siw_hal_tc_driving_u2(struct device *dev)
{
	return (TC_DRIVE_CTL_DISP_U2 | TC_DRIVE_CTL_START);
}

static inline int __used siw_hal_tc_driving_u3(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int ctrl =
	(TC_DRIVE_CTL_DISP_U3 | TC_DRIVE_CTL_MODE_6LHB | TC_DRIVE_CTL_START);

	if (touch_flags(ts) & TOUCH_USE_VBLANK)
		ctrl &= ~TC_DRIVE_CTL_MODE_6LHB;

	if (atomic_read(&ts->state.debug_option_mask) & DEBUG_OPTION_1)
		ctrl &= ~TC_DRIVE_CTL_MODE_6LHB;

	return ctrl;
}

static inline int __used siw_hal_tc_driving_u3_partial(struct device *dev)
{
	return (TC_DRIVE_CTL_PARTIAL | siw_hal_tc_driving_u3(dev));
}

static inline int __used siw_hal_tc_driving_u3_qcover(struct device *dev)
{
	return (TC_DRIVE_CTL_QCOVER | siw_hal_tc_driving_u3(dev));
}

static inline int siw_hal_tc_driving_stop(struct device *dev)
{
	return TC_DRIVE_CTL_STOP;
}

static void siw_hal_chk_dbg_report(struct device *dev,
					u32 status, int irq)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	u32 debug_mask = (status >> 16) & 0x0F;
	u32 ic_debug[4];
	u32 debug_info = 0;
	u32 debug_len = 0;
	u32 debug_type = 0;
	int ret = 0;

	if (!chip->opt.f_dbg_report)
		return;

	/*
	 * 0x03 : Error report
	 * 0x04 : Debug report
	 */
	switch (debug_mask) {
	case 0x03:
	case 0x04:
		break;
	default:
		return;
	}

	ret = siw_hal_reg_read(dev, 0x23E, ic_debug, sizeof(ic_debug));
	if (ret < 0)
		return;

	debug_info = ic_debug[3];
	debug_len = ((debug_info >> 24) & 0xFF);
	debug_type = (debug_info & ((1 << 24) - 1));

	t_dev_info(dev,
		   "[%d] ic debug: s %08Xh / m %Xh, l %Xh, t %Xh (%08Xh)\n",
		   irq, status, debug_mask, debug_len, debug_type, debug_info);

	t_dev_info(dev,
		   "[%d] ic debug: log %08Xh %08Xh %08Xh\n",
		   irq, ic_debug[0], ic_debug[1], ic_debug[2]);
}

static int siw_hal_tc_driving_post(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_fw_info *fw = &chip->fw;
	u32 addr[4] = { 0, };
	u32 rdata;
	int ret = 0;

	switch (chip->opt.t_chk_mipi) {
	case 1:
		if (chip->driving_mode != LCD_MODE_U3)
			break;

		rdata = 0;
		switch (fw->v.version.major) {
		case 0:
		case 1:
			rdata = !!(fw->v.version.minor > 14);
			break;
		}

		addr[0] = (rdata) ? 0x284 : 0;
		break;
	default:
		goto out;
	}

	if (addr[0]) {
		rdata = 0;
		ret = siw_hal_read_value(dev, addr[0], &rdata);
		if (ret < 0)
			goto out;
		if (rdata) {
			t_dev_err(dev, "!! [Warning] !!\n");
			t_dev_err(dev, "Check MIPI script (%d)\n", rdata);
			t_dev_err(dev,
				"Maybe, MIPI(VIDEO) vs. FW(CMD) vice versa.\n");
		}
	}

out:
	return 0;
}

static int siw_hal_tc_driving(struct device *dev, int mode)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_reg *reg = chip->reg;
	u32 tc_status = 0;
	u32 running_status = 0;
	u32 ctrl = 0;
	u32 rdata;
	int re_init = 0;
	int ret = 0;

	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		t_dev_warn(dev,
			"can not control tc driving - deep sleep state\n");
		return 0;
	}

	if (touch_mode_not_allowed(ts, mode))
		return -EPERM;

	if (atomic_read(&ts->recur_chk)) {
		/* keep the last value for retry case */
		mode = chip->driving_mode;
		t_dev_info(dev, "keep the last mode(%d) for retry\n", mode);
	}

	chip->driving_mode = mode;

	switch (mode) {
	case LCD_MODE_U0:
		ctrl = siw_hal_tc_driving_u0(dev);
		break;

	case LCD_MODE_U2_UNBLANK:
	case LCD_MODE_U2:
		ctrl = siw_hal_tc_driving_u2(dev);
		break;

	case LCD_MODE_U3:
		ctrl = siw_hal_tc_driving_u3(dev);
		break;

	case LCD_MODE_U3_PARTIAL:
		ctrl = siw_hal_tc_driving_u3_partial(dev);
		break;

	case LCD_MODE_U3_QUICKCOVER:
		ctrl = siw_hal_tc_driving_u3_qcover(dev);
		break;

	case LCD_MODE_STOP:
		ctrl = siw_hal_tc_driving_stop(dev);
		break;

	default:
		t_dev_err(dev, "mode(%d) not supported\n", mode);
		return -ESRCH;
	}

	/* swipe set */
	ret = siw_hal_swipe_mode(dev, mode);
	if (ret < 0)
		t_dev_warn(dev, "swipe mode err, %d", ret);

	if ((mode == LCD_MODE_U0) ||
	    (mode == LCD_MODE_U2))
		touch_msleep(chip->drv_opt_delay);

	touch_msleep(hal_dbg_delay(chip, HAL_DBG_DLY_TC_DRIVING_0));

	t_dev_info(dev, "current driving mode is %s\n",
		   siw_lcd_driving_mode_str(mode));

	ret = siw_hal_read_value(dev,
				 reg->spr_subdisp_status,
				 &rdata);
	t_dev_info(dev, "DDI Display Mode[%04Xh] = 0x%08X\n",
		   reg->spr_subdisp_status, rdata);

	ret = siw_hal_write_value(dev,
				  reg->tc_drive_ctl,
				  ctrl);
	t_dev_info(dev, "TC Driving[%04Xh] wr 0x%08X\n",
		   reg->tc_drive_ctl, ctrl);

	rdata = chip->drv_delay + hal_dbg_delay(chip, HAL_DBG_DLY_TC_DRIVING_1);
	touch_msleep(rdata);
	t_dev_dbg_base(dev, "waiting %d msecs\n", rdata);

	if (siw_touch_boot_mode_tc_check(dev))
		goto out;

	if (mode == LCD_MODE_U3_PARTIAL)
		goto out;

	siw_hal_tc_driving_post(dev);

	ret = siw_hal_read_value(dev,
				 reg->tc_status,
				 &tc_status);
	if (ret < 0) {
		t_dev_err(dev, "failed to get tc_status\n");
		atomic_set(&ts->recur_chk, 0);
		return ret;
	}

	siw_hal_chk_dbg_report(dev, tc_status, 0);

	running_status = tc_status & 0x1F;

	re_init = 0;
	if (mode == LCD_MODE_STOP)
		re_init = !!running_status;
	else {
		if (!running_status ||
		    (running_status == 0x10) ||
		    (running_status == 0x0F))
			re_init = 1;
	}

	if (re_init) {
		ret = siw_hal_read_value(dev,
					 reg->spr_subdisp_status,
					 &rdata);

		if (atomic_read(&ts->recur_chk)) {
			t_dev_err(dev,
			"command failed: mode %d, tc_status %08Xh, DDI %08Xh\n",
				  mode, tc_status, rdata);
			atomic_set(&ts->recur_chk, 0);
			return -EFAULT;
		}

		t_dev_err(dev,
			"command missed: mode %d, tc_status %08Xh, DDI %08Xh\n",
			  mode, tc_status, rdata);

		atomic_set(&ts->recur_chk, 1);

		ret = siw_hal_reinit(dev, 1,
			100 + hal_dbg_delay(chip, HAL_DBG_DLY_HW_RST_2), 1,
			siw_hal_init);
		if (ret < 0)
			return ret;
	} else {
		t_dev_info(dev, "command done: mode %d, running_sts %02Xh\n",
			   mode, running_status);
	}

out:
	siw_hal_tc_con_glove(dev);
	siw_hal_tc_con_grab(dev);

	atomic_set(&ts->recur_chk, 0);

	return 0;
}

static void siw_hal_deep_sleep(struct device *dev)
{
	t_dev_info(dev, "deep sleep\n");

	siw_hal_tc_driving(dev, LCD_MODE_STOP);
	siw_hal_clock(dev, 0);
}

#if defined(__SIW_CONFIG_KNOCK)
static const char * const siw_hal_tci_debug_type_1_str[] = {
	"SUCCESS",
	"DISTANCE_INTER_TAP",
	"DISTANCE_TOUCH_SLOP",
	"MIN_TIMEOUT_INTER_TAP",
	"MAX_TIMEOUT_INTER_TAP",
	"LONG_PRESS_TIME_OUT",
	"MULTI_FINGER",
	"DELAY_TIME",
	"PALM_STATE",
	"OUT_OF_AREA",
};

#define TCI_FAIL_TYPE_1_NUM		ARRAY_SIZE(siw_hal_tci_debug_type_1_str)

static void siw_hal_debug_tci_type_1(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	char **str = (char **)siw_hal_tci_debug_type_1_str;
	u32 rdata = 0;
	u32 buffer = 0;
	u32 index = 0;
	int ret = 0;

	ret = siw_hal_read_value(dev,
				 reg->tci_debug_fail_status,
				 &rdata);
	if (ret < 0) {
		t_dev_err(dev,
			"failed to read tci debug fail status, %d\n", ret);
		return;
	}

	ret = siw_hal_read_value(dev,
				 reg->tci_debug_fail_buffer,
				 &buffer);
	if (ret < 0) {
		t_dev_err(dev,
			"failed to read tci debug fail buffer, %d\n", ret);
		return;
	}

	t_dev_info(dev,
		   "Status[%04Xh] = %08Xh, Buffer[%04Xh] = %08Xh\n",
		   reg->tci_debug_fail_status, rdata,
		   reg->tci_debug_fail_buffer, buffer);

	/* Knock On fail */
	if (rdata & 0x01) {
		index = (buffer & 0xFFFF);
		t_dev_info(dev, "TCI-Debug: on fail reason: %s(%08Xh)\n",
		(index < TCI_FAIL_TYPE_1_NUM) ? str[index] : "(unknown)",
		buffer);
	}

	/* Knock Code fail */
	if (rdata & 0x02) {
		index = ((buffer >> 16) & 0xFFFF);
		t_dev_info(dev, "TCI-Debug: code fail reason: %s(%08Xh)\n",
		(index < TCI_FAIL_TYPE_1_NUM) ? str[index] : "(unknown)",
		buffer);
	}
}

static void siw_hal_debug_tci(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);

	switch (chip->opt.t_chk_tci_debug) {
	case 1:
		siw_hal_debug_tci_type_1(dev);
		break;
	default:
		break;
	}
}
#else	/* __SIW_CONFIG_KNOCK */
static void siw_hal_debug_tci(struct device *dev)
{

}
#endif	/* __SIW_CONFIG_KNOCK */

#if defined(__SIW_CONFIG_SWIPE)
static void siw_hal_debug_swipe(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	u8 debug_reason_buf[SWIPE_MAX_NUM][SWIPE_DEBUG_MAX_NUM];
	u32 rdata[5] = {0, };
	u8 count[2] = {0, };
	u8 count_max = 0;
	u32 i, j = 0;
	u8 buf = 0;
	int ret = 0;

	if (!chip->swipe_debug_type)
		return;

	ret = siw_hal_reg_read(dev,
			       reg->swipe_debug_r,
			       (void *)&rdata, sizeof(rdata));

	count[SWIPE_R] = (rdata[0] & 0xFFFF);
	count[SWIPE_L] = ((rdata[0] >> 16) & 0xFFFF);
	count_max = (count[SWIPE_R] > count[SWIPE_L]) ?
		    count[SWIPE_R] : count[SWIPE_L];

	if (count_max == 0)
		return;

	if (count_max > SWIPE_DEBUG_MAX_NUM) {
		count_max = SWIPE_DEBUG_MAX_NUM;
		if (count[SWIPE_R] > SWIPE_DEBUG_MAX_NUM)
			count[SWIPE_R] = SWIPE_DEBUG_MAX_NUM;
		if (count[SWIPE_L] > SWIPE_DEBUG_MAX_NUM)
			count[SWIPE_L] = SWIPE_DEBUG_MAX_NUM;
	}

	for (i = 0; i < ((count_max - 1) >> 2) + 1; i++) {
		memcpy(&debug_reason_buf[SWIPE_R][i << 2],
			&rdata[i + 1], sizeof(u32));
		memcpy(&debug_reason_buf[SWIPE_L][i << 2],
			&rdata[i + 3], sizeof(u32));
	}

	for (i = 0; i < SWIPE_MAX_NUM; i++) {
		for (j = 0; j < count[i]; j++) {
			buf = debug_reason_buf[i][j];
			t_dev_info(dev, "SWIPE_%s - DBG[%d/%d]: %s\n",
				   i == SWIPE_R ? "Right" : "Left",
				   j + 1, count[i],
				   (buf > 0 && buf < SWIPE_FAIL_NUM) ?
				   siw_hal_swipe_debug_str[buf] :
				   siw_hal_swipe_debug_str[0]);
		}
	}
}
#else	/* __SIW_CONFIG_SWIPE */
static void siw_hal_debug_swipe(struct device *dev)
{

}
#endif	/* __SIW_CONFIG_SWIPE */

static void siw_hal_lpwg_ctrl_init(struct lpwg_mode_ctrl *ctrl)
{
	ctrl->clk = LPWG_SET_SKIP;
	ctrl->qcover = LPWG_SET_SKIP;
	ctrl->lpwg = LPWG_SET_SKIP;
	ctrl->lcd = LPWG_SET_SKIP;
}

static int siw_hal_lpwg_ctrl(struct device *dev,
			     struct lpwg_mode_ctrl *ctrl)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int ret = 0;

	if (ctrl->clk != LPWG_SET_SKIP) {
		if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP)
			siw_hal_clock(dev, ctrl->clk);
	}

	if (ctrl->qcover != LPWG_SET_SKIP) {
		ret = siw_hal_tci_area_set(dev, ctrl->qcover);
		if (ret < 0)
			goto out;
	}

	if (ctrl->lpwg != LPWG_SET_SKIP) {
		ret = siw_hal_lpwg_control(dev, ctrl->lpwg);
		if (ret < 0)
			goto out;
	}

	if (ctrl->lcd != LPWG_SET_SKIP)
		ret = siw_hal_tc_driving(dev, ctrl->lcd);

out:
	return ret;
}

static int siw_hal_lpwg_ctrl_skip(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	t_dev_info(dev, "skip lpwg_mode\n");

	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP)
		siw_hal_clock(dev, 1);

	siw_hal_debug_tci(dev);
	siw_hal_debug_swipe(dev);

	return 0;
}

static int siw_hal_lpwg_mode_suspend(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct lpwg_mode_ctrl ctrl;
	int changed = 0;
	int ret = 0;

	siw_hal_lpwg_ctrl_init(&ctrl);

	t_dev_info(dev, "lpwg suspend: mode %d, screen %d\n",
		   ts->lpwg.mode, ts->lpwg.screen);

	if (ts->role.mfts_lpwg) {
		t_dev_info(dev, "lpwg suspend: mfts_lpwg\n");

		ctrl.lpwg = LPWG_DOUBLE_TAP,
		     ctrl.lcd = chip->lcd_mode;
		goto out_con;
	}

	if (ts->lpwg.mode == LPWG_NONE) {
		if (ts->lpwg.screen)
			siw_hal_clock(dev, 1);
		else
			siw_hal_deep_sleep(dev);
		goto out;
	}

	if (ts->lpwg.screen) {
		siw_hal_lpwg_ctrl_skip(dev);
		goto out;
	}

#if defined(__SIW_CONFIG_PROX_ON_SUSPEND)
	if (ts->lpwg.sensor == PROX_NEAR) {
		t_dev_info(dev,
			"lpwg suspend: (ts->lpwg.sensor == PROX_NEAR)\n");
		siw_hal_deep_sleep(dev);
		goto out;
	}
#endif

	if (ts->lpwg.qcover == HOLE_NEAR) {
		/* knock on/code disable */
		ctrl.clk = 1;
		ctrl.qcover = QUICKCOVER_CLOSE;
		ctrl.lpwg = LPWG_NONE;
		ctrl.lcd = chip->lcd_mode;
		goto out_con;
	}

	/* knock on/code */
	ctrl.clk = 1;
	ctrl.qcover = QUICKCOVER_OPEN;
	ctrl.lpwg = ts->lpwg.mode;
	ctrl.lcd = chip->lcd_mode;

out_con:
	ret = siw_hal_lpwg_ctrl(dev, &ctrl);
	changed = 1;

out:
	t_dev_info(dev, "lpwg suspend(%d, %d): lcd_mode %d, driving_mode %d\n",
		   changed, ret,
		   chip->lcd_mode, chip->driving_mode);

	return ret;
}

static int siw_hal_lpwg_mode_resume(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct lpwg_mode_ctrl ctrl;
	int changed = 0;
	int ret = 0;

	siw_hal_lpwg_ctrl_init(&ctrl);

	t_dev_info(dev, "lpwg resume: mode %d, screen %d\n",
		   ts->lpwg.mode, ts->lpwg.screen);

	siw_touch_report_all_event(ts);

#if defined(__SIW_CONFIG_PROX_ON_RESUME)
	if (ts->lpwg.sensor == PROX_NEAR) {
		t_dev_info(dev,
			"lpwg resume: (ts->lpwg.sensor == PROX_NEAR)\n");

		ctrl.lcd = LCD_MODE_STOP;
		goto out_con;
	}
#endif

	if (ts->lpwg.screen) {
		int mode = chip->lcd_mode;

		/* normal */
		t_dev_info(dev, "lpwg resume: screen\n");

		if (touch_mode_allowed(ts, LCD_MODE_U3_QUICKCOVER)) {
			mode = (ts->lpwg.qcover == HOLE_NEAR) ?
			       LCD_MODE_U3_QUICKCOVER : mode;
		}

		ctrl.lpwg = LPWG_NONE;
		ctrl.lcd = mode;
		goto out_con;
	}

	if (touch_mode_not_allowed(ts, LCD_MODE_U3_PARTIAL))
		goto out;

	t_dev_info(dev, "lpwg resume: partial\n");

	if (touch_mode_allowed(ts, LCD_MODE_U3_QUICKCOVER)) {
		ctrl.qcover = (ts->lpwg.qcover == HOLE_NEAR) ?
			      QUICKCOVER_CLOSE : QUICKCOVER_OPEN;
		ctrl.lpwg = ts->lpwg.mode;
		ctrl.lcd = LCD_MODE_U3_PARTIAL;
		goto out_con;
	}

	ctrl.lpwg = (ts->lpwg.qcover == HOLE_NEAR) ?
		    LPWG_NONE : ts->lpwg.mode;
	ctrl.lcd = LCD_MODE_U3_PARTIAL;

out_con:
	ret = siw_hal_lpwg_ctrl(dev, &ctrl);
	changed = 1;

out:
	t_dev_info(dev, "lpwg resume(%d, %d): lcd_mode %d, driving_mode %d\n",
		   changed, ret,
		   chip->lcd_mode, chip->driving_mode);

	return ret;
}

static int siw_hal_lpwg_mode(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	if (atomic_read(&ts->state.fb) == FB_SUSPEND)
		return siw_hal_lpwg_mode_suspend(dev);

	return siw_hal_lpwg_mode_resume(dev);
}

#define TCI_INFO_CNT (sizeof(struct tci_info) / 2)

#define siw_prt_tci_control_info(_dev, _idx, _info)	\
do {	\
	t_dev_info(_dev,	\
	"tci info[%s] tap_count %d, min_intertap %d, max_intertap %d\n",\
	_idx, _info->tap_count, _info->min_intertap, _info->max_intertap);\
	t_dev_info(_dev,	\
	"tci info[%s] touch_slop %d, tap_distance %d, intr_delay %d\n",\
	_idx, _info->touch_slop, _info->tap_distance, _info->intr_delay);\
} while (0)

static int siw_hal_show_ic_tci_info(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	struct tci_info *info;
	u16 lpwg_data[TCI_INFO_CNT + 1][2];
	u16 lpwg_data_tci_1[TCI_INFO_CNT];
	u16 lpwg_data_tci_2[TCI_INFO_CNT];
	int i = 0;
	int ret = 0;

	t_dev_info(dev, "[ IC tci info ]\n");

	ret = siw_hal_reg_read(dev,
			       reg->tci_enable_w,
			       (void *)lpwg_data, sizeof(lpwg_data));
	if (ret < 0) {
		t_dev_err(dev, "tci info read fail\n");

		goto out;
	}

	for (i = 0; i < TCI_INFO_CNT; i++) {
		lpwg_data_tci_1[i] = lpwg_data[i + 1][0];
		lpwg_data_tci_2[i] = lpwg_data[i + 1][1];
	}

	info = (struct tci_info *)lpwg_data_tci_1;
	siw_prt_tci_control_info(dev, "TCI_1", info);

	info = (struct tci_info *)lpwg_data_tci_2;
	siw_prt_tci_control_info(dev, "TCI_2", info);

out:
	return ret;
}

static void siw_hal_show_driver_tci_info(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct tci_ctrl *tci = &ts->tci;
	struct tci_info *info;

	t_dev_info(dev, "[ driver tci info ]\n");

	info = &tci->info[TCI_1];
	siw_prt_tci_control_info(dev, "TCI_1", info);

	info = &tci->info[TCI_2];
	siw_prt_tci_control_info(dev, "TCI_2", info);
}

static int siw_hal_lpwg_ext_tci_info(struct device *dev, void *param)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_reg *reg = chip->reg;
	u16 *info_addr;
	int *data = (int *)param;
	int tci_type = 0;
	int index = 0;
	u16 value = 0;
	u16 write_en = 0;
	u16 value_2 = 0;
	u32 send_buf = 0;
	int ret = 0;

	tci_type = data[0];
	index = data[1];
	value = data[2];
	write_en = data[3];

	if ((tci_type > TCI_2) || (tci_type < 0)) {
		t_dev_err(dev, "invaild tci_type value [%d]\n", tci_type);
		goto out_invaild;
	}

	if ((index > (TCI_INFO_CNT - 1)) || (index < 0)) {
		t_dev_err(dev, "invaild index value [%d]\n", index);
		goto out_invaild;
	}

	info_addr = (u16 *)&ts->tci.info[!tci_type];
	value_2 = info_addr[index];

	send_buf = (value << (tci_type ? 16 : 0)) |
		(value_2 << (tci_type ? 0 : 16));

	if (write_en == 1) {
		t_dev_info(dev, "tci info write addr[%Xh] value[%Xh]\n",
			   reg->tap_count_w + index, send_buf);

		ret = siw_hal_write_value(dev,
					  reg->tap_count_w + index, send_buf);
		if (ret < 0) {
			t_dev_err(dev,
		"tci info write fail, so don't setting driver tci info\n");
			goto out_write_fail;
		}
	}

	info_addr = (u16 *)&ts->tci.info[tci_type];
	info_addr[index] = value;

	siw_hal_show_ic_tci_info(dev);

out_write_fail:
	siw_hal_show_driver_tci_info(dev);

out_invaild:
	return ret;
}

#define TCI_AREA_CNT (sizeof(struct active_area) / 2)

#define __siw_prt_tci_area(_dev, _fmt, _name, _area, _shift)	\
		t_dev_info(_dev,	\
			_fmt "x1:%4d, y1:%4d, x2:%4d, y2:%4d\n",\
			_name,	\
			((_area->x1 >> _shift) & 0xFFFF),	\
			((_area->y1 >> _shift) & 0xFFFF),	\
			((_area->x2 >> _shift) & 0xFFFF),	\
			((_area->y2 >> _shift) & 0xFFFF))

#define siw_prt_tci_area_drv(_dev, _name, _area, _shift)	\
	__siw_prt_tci_area(_dev, "tci %s area ", _name, _area, _shift)

static void siw_hal_show_driver_tci_area(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct active_area *area;
	struct reset_area *tci_qcover;

	t_dev_info(dev, "[ driver tci area ]\n");

	area = &ts->tci.area;
	siw_prt_tci_area_drv(dev, "[active      ]", area, 0);

	tci_qcover = &ts->tci.qcover_open;
	siw_prt_tci_area_drv(dev, "[qcover open ]", tci_qcover, 0);

	tci_qcover = &ts->tci.qcover_close;
	siw_prt_tci_area_drv(dev, "[qcover close]", tci_qcover, 0);
}

#define siw_prt_tci_area_ic(_dev, _name, _area, _shift)	\
	__siw_prt_tci_area(_dev, "tci area %s ", _name, _area, _shift)

static int siw_hal_show_ic_tci_area(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	struct reset_area *area;
	u32 area_data_buf[TCI_AREA_CNT] = {0, };
	int ret = 0;

	t_dev_info(dev, "[ IC tci area ]\n");

	ret = siw_hal_reg_read(dev,
			       reg->act_area_x1_w,
			       (void *)area_data_buf, sizeof(area_data_buf));
	if (ret < 0) {
		t_dev_err(dev, "tci area read fail\n");

		goto out;
	}

	area = (struct reset_area *)area_data_buf;
	siw_prt_tci_area_ic(dev, "[TCI_1]", area, 0);
	siw_prt_tci_area_ic(dev, "[TCI_2]", area, 16);

out:
	return ret;
}

static int siw_hal_lpwg(struct device *dev, u32 code, void *param)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct tci_ctrl *tci = &ts->tci;
	struct active_area *area = &tci->area;
	struct reset_area *area_qcover;
	struct lpwg_info *lpwg = &ts->lpwg;
	int *value = (int *)param;
	int changed = 0;
	int ret = 0;

	if (!ts->role.use_lpwg) {
		t_dev_warn(dev, "LPWG control not supported in %s\n",
			   touch_chip_name(ts));
		return 0;
	}

	if (atomic_read(&chip->init) == IC_INIT_NEED) {
		t_dev_warn(dev, "Not Ready, Need IC init (lpwg)\n");
		return 0;
	}

	touch_msleep(hal_dbg_delay(chip, HAL_DBG_DLY_LPWG));

	switch (code) {
	case LPWG_ACTIVE_AREA:
		area->x1 = value[0];
		area->x2 = value[1];
		area->y1 = value[2];
		area->y2 = value[3];
		t_dev_info(dev,
			"LPWG_ACTIVE_AREA: x1[%d], y1[%d], x2[%d], y2[%d]\n",
			area->x1, area->y1, area->x2, area->y2);
		break;

	case LPWG_TAP_COUNT:
		tci->info[TCI_2].tap_count = value[0];
		break;

	case LPWG_DOUBLE_TAP_CHECK:
		tci->double_tap_check = value[0];
		break;

	case LPWG_UPDATE_ALL:
		changed = (lpwg->mode != value[0]) |
			  ((lpwg->screen != value[1]) << 1) |
			  ((lpwg->sensor != value[2]) << 2) |
			  ((lpwg->qcover != value[3]) << 3);

		lpwg->mode = value[0];
		lpwg->screen = value[1];
		lpwg->sensor = value[2];
		lpwg->qcover = value[3];

		t_lpwg_mode = lpwg->mode;
		t_lpwg_screen = lpwg->screen;
		t_lpwg_sensor = lpwg->sensor;
		t_lpwg_qcover = lpwg->qcover;

		t_dev_info(dev,
"LPWG_UPDATE_ALL: mode[%d], screen[%s], sensor[%s], qcover[%s] (%02Xh)\n",
			   lpwg->mode,
			   lpwg->screen ? "ON" : "OFF",
			   lpwg->sensor ? "FAR" : "NEAR",
			   lpwg->qcover ? "CLOSE" : "OPEN",
			   changed);

#if defined(__SKIP_LPWG_UPDATE_ALL_FOR_SAME_INPUT)
		if (!changed) {
			t_dev_info(dev, "LPWG_UPDATE_ALL: not changed, skip\n");
			break;
		}
#endif

		ret = siw_hal_lpwg_mode(dev);
		break;

	case LPWG_REPLY:
		break;

	case LPWG_EXT_TCI_INFO_STORE:
		t_dev_info(dev,
"LPWG_EXT_TCI_INFO_STORE: tci[%s], index[%d], value[%d], write_en[%s]\n",
			   value[0] ? "TCI_2" : "TCI_1",
			   value[1],
			   value[2],
			   value[3] ? "YES" : "NO");

		ret = siw_hal_lpwg_ext_tci_info(dev, param);
		break;

	case LPWG_EXT_TCI_QOPEN_AREA_STORE:
		area_qcover = &tci->qcover_open;

		area_qcover->x1 = value[0];
		area_qcover->x2 = value[1];
		area_qcover->y1 = value[2];
		area_qcover->y2 = value[3];

		t_dev_info(dev,
	"LPWG_EXT_TCI_QOPEN_AREA_STORE: x1[%d], y1[%d], x2[%d], y2[%d]\n",
	area_qcover->x1, area_qcover->y1, area_qcover->x2, area_qcover->y2);
		break;

	case LPWG_EXT_TCI_QCLOSE_AREA_STORE:
		area_qcover = &tci->qcover_close;

		area_qcover->x1 = value[0];
		area_qcover->x2 = value[1];
		area_qcover->y1 = value[2];
		area_qcover->y2 = value[3];

		t_dev_info(dev,
	"LPWG_EXT_TCI_QCLOSE_AREA_STORE: x1[%d], y1[%d], x2[%d], y2[%d]\n",
	area_qcover->x1, area_qcover->y1, area_qcover->x2, area_qcover->y2);
		break;

	case LPWG_EXT_SWIPE_INFO_STORE:
		t_dev_info(dev, "LPWG_EXT_SWIPE_INFO_STORE\n");
		break;

	case LPWG_EXT_TCI_INFO_SHOW:
		t_dev_info(dev, "LPWG_EXT_TCI_INFO_SHOW\n");

		ret = siw_hal_show_ic_tci_info(dev);

		siw_hal_show_driver_tci_info(dev);
		break;

	case LPWG_EXT_TCI_AREA_SHOW:
		t_dev_info(dev, "LPWG_EXT_TCI_AREA_SHOW\n");

		ret = siw_hal_show_ic_tci_area(dev);

		siw_hal_show_driver_tci_area(dev);
		break;

	case LPWG_EXT_SWIPE_INFO_SHOW:
		break;
	}

	return ret;
}

#if defined(__SIW_SUPPORT_ASC)
static int siw_hal_asc(struct device *dev, u32 code, u32 value)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_reg *reg = chip->reg;
	struct siw_hal_asc_info *asc = &chip->asc;
	u32 rdata = 0;
	u32 wdata = 0;
	u32 asc_ret = 0;
	size_t mon_data[6] = { 0, };
	int mon_cnt = 0;
	int ret = 0;

	mon_data[mon_cnt++] = code;

	switch (code) {
	case ASC_READ_MAX_DELTA:
		ret = siw_hal_reg_read(dev,
				       reg->max_delta,
				       (void *)&rdata, sizeof(rdata));
		mon_data[mon_cnt++] = (size_t)reg->max_delta;
		mon_data[mon_cnt++] = (size_t)rdata;
		if (ret < 0)
			break;

		asc_ret = rdata;

		break;

	case ASC_GET_FW_SENSITIVITY:
	/* fall through */
	case ASC_WRITE_SENSITIVITY:
		ret = siw_hal_reg_read(dev,
				       reg->touch_max_r,
				       (void *)&rdata, sizeof(rdata));
		mon_data[mon_cnt++] = (size_t)reg->touch_max_r;
		mon_data[mon_cnt++] = (size_t)rdata;
		if (ret < 0)
			break;

		asc->normal_s = rdata;
		asc->acute_s = (rdata / 10) * 6;
		asc->obtuse_s = rdata;

		if (code == ASC_GET_FW_SENSITIVITY) {
			t_dev_info(dev,
			"max_r(%04Xh) = %d, n_s %d, a_s = %d, o_s = %d\n",
				   reg->touch_max_r,
				   rdata,
				   asc->normal_s,
				   asc->acute_s,
				   asc->obtuse_s);
			break;
		}

		switch (value) {
		case NORMAL_SENSITIVITY:
			wdata = asc->normal_s;
			break;
		case ACUTE_SENSITIVITY:
			wdata = asc->acute_s;
			break;
		case OBTUSE_SENSITIVITY:
			wdata = asc->obtuse_s;
			break;
		default:
			wdata = rdata;
			break;
		}

		ret = siw_hal_write_value(dev,
					  reg->touch_max_w,
					  wdata);
		mon_data[mon_cnt++] = (size_t)reg->touch_max_w;
		mon_data[mon_cnt++] = (size_t)wdata;
		if (ret < 0)
			break;

		t_dev_info(dev, "max_w(%04Xh) changed (%d -> %d)\n",
			   reg->touch_max_w,
			   rdata, wdata);
		break;
	default:
		break;
	}

	siwmon_submit_ops_wh_name(dev, "%s asc done",
				  touch_chip_name(ts),
				  mon_data, mon_cnt, asc_ret);

	return asc_ret;
}
#else	/* __SIW_SUPPORT_ASC */
static int siw_hal_asc(struct device *dev, u32 code, u32 value)
{
	return 0;
}
#endif	/* __SIW_SUPPORT_ASC */


#define siw_chk_sts_snprintf(_dev, _buf, _buf_max, _size, _fmt, _args...) \
({	\
	int _n_size = 0;	\
	_n_size = __siw_snprintf(_buf, _buf_max, _size, _fmt, ##_args);	\
	t_dev_dbg_trace(_dev, _fmt, ##_args);	\
	_n_size;	\
})

static int siw_hal_check_fault_type(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	u32 addr = 0;
	int fault_type = NON_FAULT_INT;
	int ret = 0;

	switch (chip->opt.t_chk_fault) {
	case 1:
		addr = 0x283;
		break;
	default:
		return NON_FAULT_INT;
	}

	ret = siw_hal_read_value(dev, addr, (u32 *)&fault_type);
	if (ret < 0)
		return NON_FAULT_INT;

	return fault_type;
}

static u32 siw_hal_check_sys_error_type(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	u32 addr = 0;
	u32 fault_type = NON_FAULT_U32;
	int ret = 0;

	switch (chip->opt.t_chk_sys_error) {
	case 1:
		addr = 0x020;
		break;
	case 2:
		addr = 0x021;
		break;
	case 3:
		addr = 0x01C;
		break;
	default:
		return NON_FAULT_U32;
	}

	ret = siw_hal_read_value(dev, addr, (u32 *)&fault_type);
	if (ret < 0)
		return NON_FAULT_U32;

	return fault_type;
}

static u32 siw_hal_check_sys_fault_type(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	u32 addr = 0;
	u32 fault_type = NON_FAULT_U32;
	int ret = 0;

	switch (chip->opt.t_chk_sys_fault) {
	case 1:
		addr = 0xFF4;
		break;
	default:
		return NON_FAULT_U32;
	}

	ret = siw_hal_read_value(dev, addr, (u32 *)&fault_type);
	if (ret < 0)
		return NON_FAULT_U32;

	return fault_type;
}

static int siw_hal_check_status_type_x(struct device *dev,
				       u32 status, u32 ic_status, int irq)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_status_filter *filter = chip->status_filter;
	u32 check_mask, detected;
	int type_error, type_esd, type_fault;
	u32 dbg_mask = 0;
	u32 log_flag = 0;
	u32 esd_send = 0;
	u32 tc_disp_err = 0;
	u32 ic_abnormal, ic_error, ic_disp_err;
	int log_max = IC_CHK_LOG_MAX;
	char log[IC_CHK_LOG_MAX] = {0, };
	int fault_val;
	int len = 0;
	int ret = 0;

	if (filter == NULL)
		return -EINVAL;

	if (ic_status & ~chip->status_mask_ic_valid) {
		t_dev_err(dev,
			"[%d] status %08Xh,ic_status %08Xh,ic_status invalid\n",
			  irq, status, ic_status);

		if (chip->lcd_mode != LCD_MODE_U0) {
			if (siw_hal_send_esd_notifier(dev, 8)) {
				atomic_set(&chip->esd_noti_sent, 1);
				return -ETDSENTESDIRQ;
			}
		}
		return -ERESTART;
	}

	while (1) {
		if (!filter->id || !filter->width)
			break;

		type_error = !!(filter->flag & STS_FILTER_FLAG_TYPE_ERROR);
		type_esd = !!(filter->flag & STS_FILTER_FLAG_ESD_SEND);
		type_fault = !!(filter->flag & STS_FILTER_FLAG_CHK_FAULT);

		check_mask = ((1 << filter->width) - 1) << filter->pos;

		detected = (type_error) ?
			(status & check_mask) : !(status & check_mask);

		if (check_mask && detected) {
			log_flag |= check_mask;
			esd_send |= (type_esd) ? check_mask : 0;

			fault_val = (type_fault) ?
				siw_hal_check_fault_type(dev) : -1;

			if (type_fault && (fault_val >= 0)) {
				len += siw_chk_sts_snprintf(dev, log,
					log_max, len, "[b%d] %s(%Xh) ",
					filter->pos, filter->str, fault_val);
			} else {
				len += siw_chk_sts_snprintf(dev, log,
					log_max, len, "[b%d] %s ",
					filter->pos, filter->str);
			}

			tc_disp_err |= !!(filter->id == STS_ID_ERROR_DISP);
		}

		filter++;
	}

	if (log_flag) {
		t_dev_err(dev,
			"[%d] status %08Xh, ic_status %08Xh, (%08Xh) %s\n",
			irq, status, ic_status, log_flag, log);
	}

	ic_abnormal = ic_status & chip->status_mask_ic_abnormal;
	ic_error = ic_status & chip->status_mask_ic_error;
	ic_disp_err = ic_status & chip->status_mask_ic_disp_err;

	if (ic_abnormal || ic_error || ic_disp_err) {
		u32 err_val[3] = { ic_abnormal, ic_error, ic_disp_err };
		char *err_str[3] = {
			"esd",
			"watchdog",
			"dic"
		};
		u32 sys_error, sys_fault;
		int log_add = !log_flag;
		int err_pre, i;

		sys_error = siw_hal_check_sys_error_type(dev);
		sys_fault = siw_hal_check_sys_fault_type(dev);

		log_add |= (!!((sys_error != NON_FAULT_U32) ||
			       (sys_fault != NON_FAULT_U32))) << 1;

		len = siw_chk_sts_snprintf(dev, log, log_max, 0,
					   "[%d] ", irq);

		err_pre = 0;
		for (i = 0; i < ARRAY_SIZE(err_val) ; i++) {
			if (!err_val[i])
				continue;

			if (err_pre)
				len += siw_chk_sts_snprintf(dev, log,
							log_max, len, " & ");
			len += siw_chk_sts_snprintf(dev, log,
					log_max, len, "%s", err_str[i]);
			err_pre |= err_val[i];
		}

		if (log_add) {
			len += siw_chk_sts_snprintf(dev, log, log_max, len,
						    " - ");

			if (log_add & 0x01) {
				len += siw_chk_sts_snprintf(dev, log,
					log_max, len,
					"status %08Xh, ic_status %08Xh%s",
					status, ic_status,
					(log_add & 0x02) ? ", " : " ");
			}

			if (log_add & 0x02) {
				len += siw_chk_sts_snprintf(dev, log,
					log_max, len,
					"sys_error %08Xh, sys_fault %08Xh",
					sys_error, sys_fault);
			}
		}

		t_dev_err(dev, "%s\n", log);

		esd_send |= (ic_abnormal | ic_disp_err);
		if (!esd_send)
			ret = -ERESTART;
	}

	if (esd_send) {
		ret = -ERESTART;
		if (chip->lcd_mode != LCD_MODE_U0) {
			int esd_type;

			esd_type =
				(ic_disp_err || tc_disp_err) ? DIC_ERR_TYPE : 0;

			if (siw_hal_send_esd_notifier(dev, esd_type | 1)) {
				atomic_set(&chip->esd_noti_sent, 1);
				return -ETDSENTESDIRQ;
			}
		}
	}

	if (ret == -ERESTART)
		return ret;

	dbg_mask = ((status >> 16) & 0xF);
	switch (dbg_mask) {
	case 0x2:
		t_dev_info(dev, "[%d] TC Driving OK\n", irq);
		ret = -ERANGE;
		break;
	case 0x3:
	/* fall through */
	case 0x4:
		t_dev_dbg_trace(dev, "[%d] dbg_mask %Xh\n",
				irq, dbg_mask);
		ret = -ERANGE;
		break;
	}

	return ret;
}

#if defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW1828) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW42101)
#define STS_RET_ERR		ERESTART
#else
#define STS_RET_ERR		ERANGE
#endif

static int siw_hal_do_check_status(struct device *dev,
				   u32 status, u32 ic_status, int irq)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	u32 reset_clr_bit = 0;
	u32 logging_clr_bit = 0;
	u32 int_norm_mask = 0;
	u32 status_mask = 0;
	int ret_pre = 0;
	int ret = 0;

	if (!status && !ic_status) {
		t_dev_err(dev, "all low detected\n");
		return -STS_RET_ERR;
	}
	if ((status == ~0) && (ic_status == ~0)) {
		t_dev_err(dev, "all high detected\n");
		return -STS_RET_ERR;
	}

	ret = siw_hal_chk_status_type(dev);
	if (ret < 0)
		return ret;

	reset_clr_bit = chip->status_mask_reset;
	logging_clr_bit = chip->status_mask_logging;
	int_norm_mask = chip->status_mask_normal;

	status_mask = status ^ int_norm_mask;

	if (!(irq & 0x80)) {
		t_dev_dbg_trace(dev, "[%d] h/w:%Xh, f/w:%Xh(%Xh)\n",
				irq, ic_status, status, status_mask);
	}
	irq &= 0x01;

	if (status_mask & reset_clr_bit) {
		t_dev_err(dev,
	"[%d] need reset: status %08Xh, ic_status %08Xh, chk %08Xh (%08Xh)\n",
	irq, status, ic_status, status_mask & reset_clr_bit, reset_clr_bit);
		ret_pre = -ERESTART;
	} else if (status_mask & logging_clr_bit) {
		t_dev_err(dev,
	"[%d] need logging: status %08Xh, ic_status %08Xh, chk %08Xh (%08Xh)\n",
	irq, status, ic_status, status_mask & logging_clr_bit, logging_clr_bit);
		ret_pre = -ERANGE;
	}

	switch (chip->status_type) {
	case CHIP_STATUS_TYPE_2:
	case CHIP_STATUS_TYPE_1:
	case CHIP_STATUS_TYPE_0:
		ret = siw_hal_check_status_type_x(dev, status, ic_status, irq);
		siw_hal_chk_dbg_report(dev, status, irq);
		break;
	default:
		t_dev_warn(dev, "unknown status type, %d\n", chip->status_type);
		break;
	}

	if (ret == -ETDSENTESDIRQ)
		return ret;

	if (ret_pre) {
		if (ret != -ERESTART)
			ret = ret_pre;
	}

	return ret;
}

static int siw_hal_check_status(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	u32 ic_status = chip->info.ic_status;
	u32 status = chip->info.device_status;

	return siw_hal_do_check_status(dev, status, ic_status, 1);
}

static int siw_hal_irq_abs_data_type_1(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_touch_data_type_1 *data;
	struct touch_data *tdata;
	u32 touch_count = chip->info.touch_cnt;
	int finger_index = 0;
	int i = 0;
	int ret = 0;

	data = (struct siw_hal_touch_data_type_1 *)chip->info.data;

	touch_count = chip->info.touch_cnt;
	ts->new_mask = 0;

	/* check if palm detected */
	if (data->track_id == PALM_ID) {
		if (data->event == TOUCHSTS_DOWN) {
			ts->is_palm = 1;
			t_dev_info(dev, "Palm Detected\n");
		} else if (data->event == TOUCHSTS_UP) {
			ts->is_palm = 0;
			t_dev_info(dev, "Palm Released\n");
		}
		ts->tcount = 0;
		ts->intr_status = TOUCH_IRQ_FINGER;
		return ret;
	}

	for (i = 0; i < touch_count; i++, data++) {
		if (data->track_id >= touch_max_finger(ts))
			continue;

		if ((data->event == TOUCHSTS_DOWN) ||
		    (data->event == TOUCHSTS_MOVE)) {
			ts->new_mask |= (1 << data->track_id);
			tdata = ts->tdata + data->track_id;

			tdata->id = data->track_id;
			tdata->type = data->tool_type;
			tdata->event = data->event;
			tdata->x = data->x;
			tdata->y = data->y;
			tdata->pressure = data->pressure;
			tdata->width_major = data->width_major;
			tdata->width_minor = data->width_minor;

			if (data->width_major == data->width_minor)
				tdata->orientation = 1;
			else
				tdata->orientation = (s8)data->angle;

			finger_index++;

			t_dev_dbg_abs(dev,
	"touch data [id %d, t %d, e %d, x %d, y %d, z %d - %d, %d, %d]\n",
				      tdata->id,
				      tdata->type,
				      tdata->event,
				      tdata->x,
				      tdata->y,
				      tdata->pressure,
				      tdata->width_major,
				      tdata->width_minor,
				      tdata->orientation);
		}
	}

	ts->tcount = finger_index;
	ts->intr_status = TOUCH_IRQ_FINGER;

	return ret;
}

static int siw_hal_irq_abs_data_type_0(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_touch_data *data;
	struct touch_data *tdata;
	u32 touch_count = chip->info.touch_cnt;
	int finger_index = 0;
	int i = 0;
	int ret = 0;

	data = (struct siw_hal_touch_data *)chip->info.data;

	touch_count = chip->info.touch_cnt;
	ts->new_mask = 0;

	/* check if palm detected */
	if (data->track_id == PALM_ID) {
		if (data->event == TOUCHSTS_DOWN) {
			ts->is_palm = 1;
			t_dev_info(dev, "Palm Detected\n");
		} else if (data->event == TOUCHSTS_UP) {
			ts->is_palm = 0;
			t_dev_info(dev, "Palm Released\n");
		}
		ts->tcount = 0;
		ts->intr_status = TOUCH_IRQ_FINGER;
		return ret;
	}

	for (i = 0; i < touch_count; i++, data++) {
		if (data->track_id >= touch_max_finger(ts))
			continue;

		if ((data->event == TOUCHSTS_DOWN) ||
		    (data->event == TOUCHSTS_MOVE)) {
			ts->new_mask |= (1 << data->track_id);
			tdata = ts->tdata + data->track_id;

			tdata->id = data->track_id;
			tdata->type = data->tool_type;
			tdata->event = data->event;
			tdata->x = data->x;
			tdata->y = data->y;
			tdata->pressure = data->pressure;
			tdata->width_major = data->width_major;
			tdata->width_minor = data->width_minor;

			if (data->width_major == data->width_minor)
				tdata->orientation = 1;
			else
				tdata->orientation = (s8)data->angle;

			finger_index++;

			t_dev_dbg_abs(dev,
	"touch data [id %d, t %d, e %d, x %d, y %d, z %d - %d, %d, %d]\n",
				      tdata->id,
				      tdata->type,
				      tdata->event,
				      tdata->x,
				      tdata->y,
				      tdata->pressure,
				      tdata->width_major,
				      tdata->width_minor,
				      tdata->orientation);
		}
	}

	ts->tcount = finger_index;
	ts->intr_status = TOUCH_IRQ_FINGER;

	return ret;
}

static int siw_hal_irq_abs_data(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	int ret = 0;

	switch (chip->report_type) {
	case CHIP_REPORT_TYPE_1:
		ret = siw_hal_irq_abs_data_type_1(dev);
		break;
	case CHIP_REPORT_TYPE_0:
		ret = siw_hal_irq_abs_data_type_0(dev);
		break;
	default:
		t_dev_warn(dev, "unknown report type, %d\n", chip->report_type);
		break;
	}

	return ret;
}

static int siw_hal_irq_abs(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	/* check if touch cnt is valid */
	if (chip->info.touch_cnt == 0 ||
		chip->info.touch_cnt > ts->caps.max_id) {
		struct siw_hal_touch_data *data = chip->info.data;

		t_dev_dbg_abs(dev, "Invalid touch count, %d(%d)\n",
			      chip->info.touch_cnt, ts->caps.max_id);

		/* debugging */
		t_dev_dbg_abs(dev,
			"t %d, ev %d, id %d, x %d, y %d, p %d, a %d, w %d %d\n",
			      data->tool_type, data->event, data->track_id,
			      data->x, data->y, data->pressure, data->angle,
			      data->width_major, data->width_minor);

		return -ERANGE;
	}

	return siw_hal_irq_abs_data(dev);
}

static int siw_hal_get_tci_data(struct device *dev, int count)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	u8 i = 0;
	u32 rdata[MAX_LPWG_CODE];

	if (!count)
		return 0;

	ts->lpwg.code_num = count;

	memcpy(&rdata, chip->info.data, sizeof(u32) * count);

	for (i = 0; i < count; i++) {
		ts->lpwg.code[i].x = rdata[i] & 0xffff;
		ts->lpwg.code[i].y = (rdata[i] >> 16) & 0xffff;

		if (ts->lpwg.mode == LPWG_PASSWORD)
			t_dev_info(dev, "LPWG data xxxx, xxxx\n");
		else
			t_dev_info(dev, "LPWG data %d, %d\n",
				   ts->lpwg.code[i].x, ts->lpwg.code[i].y);
	}
	ts->lpwg.code[count].x = -1;
	ts->lpwg.code[count].y = -1;

	return 0;
}

static int siw_hal_get_swipe_data(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	u32 rdata[3];
	int count = 1;

	/* swipe_info */
	/* start (X, Y), end (X, Y), time = 2bytes * 5 = 10 bytes */
	memcpy(&rdata, chip->info.data, sizeof(u32) * 3);

	t_dev_info(dev,
	"Swipe Gesture: start(%4d, %4d) end(%4d, %4d) swipe_time(%dms)\n",
		   rdata[0] & 0xffff, rdata[0] >> 16,
		   rdata[1] & 0xffff, rdata[1] >> 16,
		   rdata[2] & 0xffff);

	ts->lpwg.code_num = count;
	ts->lpwg.code[0].x = rdata[1] & 0xffff;
	ts->lpwg.code[0].x = rdata[1]  >> 16;

	ts->lpwg.code[count].x = -1;
	ts->lpwg.code[count].y = -1;

	return 0;
}

static int siw_hal_irq_lpwg_base(struct siw_ts *ts, int type)
{
	struct device *dev = ts->dev;
	int ret = 0;

	switch (type) {
	case KNOCK_1:
		if (ts->lpwg.mode == LPWG_NONE)
			break;
		t_dev_info(dev, "LPWG: TOUCH_IRQ_KNOCK\n");
		siw_hal_get_tci_data(dev,
				     ts->tci.info[TCI_1].tap_count);
		ts->intr_status = TOUCH_IRQ_KNOCK;
		break;
	case KNOCK_2:
		if (ts->lpwg.mode != LPWG_PASSWORD)
			break;
		t_dev_info(dev, "LPWG: TOUCH_IRQ_PASSWD\n");
		siw_hal_get_tci_data(dev,
				     ts->tci.info[TCI_2].tap_count);
		ts->intr_status = TOUCH_IRQ_PASSWD;
		break;
	case SWIPE_RIGHT:
		t_dev_info(dev, "LPWG: SWIPE_RIGHT\n");
		siw_hal_get_swipe_data(dev);
		ts->intr_status = TOUCH_IRQ_SWIPE_RIGHT;
		break;
	case SWIPE_LEFT:
		t_dev_info(dev, "LPWG: SWIPE_LEFT\n");
		siw_hal_get_swipe_data(dev);
		ts->intr_status = TOUCH_IRQ_SWIPE_LEFT;
		break;
	default:
		break;
	}

	return ret;
}

static int siw_hal_irq_lpwg_gesture(struct siw_ts *ts, int type)
{
	struct device *dev = ts->dev;
	int index = 0;
	int ret = 0;

	t_dev_info(dev, "lpwg gesture: %d\n", type);

	ts->intr_status = TOUCH_IRQ_GESTURE;
	ts->intr_gesture = TOUCH_UEVENT_GESTURE_C + index;

	return ret;
}

static int siw_hal_irq_lpwg_dir(struct siw_ts *ts, int type)
{
	struct device *dev = ts->dev;
	int index = 0;
	int ret = 0;

	t_dev_info(dev, "lpwg dir: %d\n", type);

	ts->intr_status = TOUCH_IRQ_GESTURE;
	ts->intr_gesture = TOUCH_UEVENT_GESTURE_DIR_RIGHT + index;

	return ret;
}

static int siw_hal_irq_lpwg_custom_debug(struct siw_ts *ts)
{
	struct device *dev = ts->dev;

	t_dev_info(dev, "LPWG: CUSTOM_DEBUG\n");
	siw_hal_debug_tci(dev);
	siw_hal_debug_swipe(dev);

	return 0;
}

static int siw_hal_irq_lpwg_knock_overtap(struct siw_ts *ts)
{
	struct device *dev = ts->dev;

	t_dev_info(dev, "LPWG: overtap\n");
	siw_hal_get_tci_data(dev, ts->tci.info[TCI_2].tap_count + 1);
	ts->intr_status = TOUCH_IRQ_PASSWD;

	return 0;
}

static int siw_hal_irq_lpwg(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	u32 type = chip->info.wakeup_type;
	int ret = 0;

	if (!type || (type > KNOCK_OVERTAP))
		goto out;

	if (type == CUSTOM_DEBUG)
		ret = siw_hal_irq_lpwg_custom_debug(ts);
	else if (type == KNOCK_OVERTAP)
		ret = siw_hal_irq_lpwg_knock_overtap(ts);
	else if (type <= SWIPE_LEFT)
		ret = siw_hal_irq_lpwg_base(ts, type);
	else if (type <= GESTURE_Z)
		ret = siw_hal_irq_lpwg_gesture(ts, type);
	else if ((type >= GESTURE_DIR_RIGHT) || (type <= GESTURE_DIR_UP))
		ret = siw_hal_irq_lpwg_dir(ts, type);
	else
		goto out;

	return ret;

out:
	t_dev_err(dev, "LPWG: unknown type, %d\n", type);

	return -EINVAL;
}

static int siw_hal_irq_handler(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_reg *reg = chip->reg;
	int size = 0;
	int ret = 0;

	if (atomic_read(&chip->init) == IC_INIT_NEED) {
		t_dev_warn(dev, "Not Ready, Need IC init (irq)\n");
		return 0;
	}

#if defined(__SIW_SUPPORT_PM_QOS)
	pm_qos_update_request(&chip->pm_qos_req, 10);
#endif
	size = 12 + (sizeof(struct siw_hal_touch_data) * touch_max_finger(ts));
	ret = siw_hal_reg_read(dev,
			       reg->tc_ic_status,
			       (void *)&chip->info, size);
#if defined(__SIW_SUPPORT_PM_QOS)
	pm_qos_update_request(&chip->pm_qos_req, PM_QOS_DEFAULT_VALUE);
#endif
	if (ret < 0)
		goto out;

	ret = siw_hal_check_status(dev);
	if (ret < 0)
		goto out;

	t_dev_dbg_irq(dev, "hal irq handler: wakeup_type %d\n",
		      chip->info.wakeup_type);

	if (chip->info.wakeup_type == ABS_MODE) {
		ret = siw_hal_irq_abs(dev);
		if (ret) {
			t_dev_err(dev, "siw_hal_irq_abs failed(%d), %d\n",
				  chip->info.touch_cnt, ret);
			goto out;
		}
	} else {
		ret = siw_hal_irq_lpwg(dev);
		if (ret) {
			t_dev_err(dev, "siw_hal_irq_lpwg failed, %d\n", ret);
			goto out;
		}
	}

out:
	return ret;
}

static void siw_hal_connect(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_reg *reg = chip->reg;
	int charger_state = atomic_read(&ts->state.connect);
	int wireless_state = atomic_read(&ts->state.wireless);

	if (wireless_state)
		chip->charger = CONNECT_WIRELESS;
	else {
		if ((charger_state < 0) ||
		    (charger_state > CONNECT_OTG)) {
			t_dev_err(dev,
				"invalid charger status, %d\n", charger_state);
			return;
		}
		chip->charger = charger_state;
	}

	t_dev_info(dev,
		   "charger_state = %Xh (%Xh, %Xh)\n",
		   chip->charger, charger_state, wireless_state);

	if (atomic_read(&ts->state.pm) > DEV_PM_RESUME) {
		t_dev_warn(dev, "DEV_PM_SUSPEND - Don't try SPI\n");
		return;
	}

	siw_hal_write_value(dev,
			    reg->spr_charger_status,
			    chip->charger);

	t_dev_info(dev, "charger_state set done\n");
}

static int siw_hal_lcd_mode(struct device *dev, u32 mode)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	if (touch_mode_not_allowed(ts, mode))
		return -EPERM;

	if ((chip->lcd_mode == LCD_MODE_U2) &&
	    siw_hal_watch_is_disp_waton(dev))
		siw_hal_watch_get_curr_time(dev, NULL, NULL);

	if (chip->opt.f_u2_blank_chg) {
		if (mode == LCD_MODE_U2_UNBLANK)
			mode = LCD_MODE_U2;
	}

	chip->prev_lcd_mode = chip->lcd_mode;
	chip->lcd_mode = mode;

	t_dev_info(dev, "lcd_mode: %d (prev: %d)\n",
		   mode, chip->prev_lcd_mode);

	return 0;
}

static int siw_hal_usb_status(struct device *dev, u32 mode)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	t_dev_info(dev, "TA Type: %d\n", atomic_read(&ts->state.connect));

	siw_hal_connect(dev);

	return 0;
}

static int siw_hal_wireless_status(struct device *dev, u32 onoff)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	t_dev_info(dev,
		"Wireless charger: 0x%02X\n", atomic_read(&ts->state.wireless));

	siw_hal_connect(dev);

	return 0;
}

static int siw_hal_earjack_status(struct device *dev, u32 onoff)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	t_dev_info(dev,
		"Earjack Type: 0x%02X\n", atomic_read(&ts->state.earjack));

	return 0;
}

#if defined(__SIW_SUPPORT_ABT)
static int siw_hal_debug_tool(struct device *dev, u32 value)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	if (value >= DEBUG_TOOL_MAX) {
		t_dev_err(dev,
			  "wrong index, debug tool select failed, %d\n",
			  value);
		return -EINVAL;
	}

	mutex_lock(&ts->lock);
	switch (value) {
	case DEBUG_TOOL_ENABLE:
		siw_hal_switch_to_abt_irq_handler(ts);
		break;
	default:
		siw_ops_restore_irq_handler(ts);
		t_dev_info(dev, "restore irq handler\n");
		break;
	}
	mutex_unlock(&ts->lock);

	return 0;
}
#else	/* __SIW_SUPPORT_ABT */
static int siw_hal_debug_tool(struct device *dev, u32 value)
{
	t_dev_info(dev, "Nop ...\n");
	return 0;
}
#endif	/* __SIW_SUPPORT_ABT */

static int siw_hal_notify(struct device *dev, ulong event, void *data)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_reg *reg = chip->reg;
	char *noti_str = "(unknown)";
	u32 value = 0;
	int ret = 0;

	if (data)
		value = *((u32 *)data);

	switch (event) {
	case LCD_EVENT_TOUCH_RESET_START:
	case LCD_EVENT_TOUCH_RESET_END:
		break;
	default:
		touch_msleep(hal_dbg_delay(chip, HAL_DBG_DLY_NOTIFY));
		break;
	}

	t_dev_dbg_noti(dev, "notify event(%d)\n", (u32)event);

	switch (event) {
	case NOTIFY_TOUCH_RESET:
		atomic_set(&chip->init, IC_INIT_NEED);
		atomic_set(&ts->state.hw_reset, event);

		t_dev_info(dev, "notify: reset\n");
		siw_hal_watch_set_rtc_clear(dev);
		noti_str = "TOUCH_RESET";
		break;
	case LCD_EVENT_TOUCH_RESET_START:
		atomic_set(&chip->init, IC_INIT_NEED);
		atomic_set(&ts->state.hw_reset, event);

		t_dev_info(dev, "notify: lcd_event: touch reset start\n");
		siw_touch_irq_control(ts->dev, INTERRUPT_DISABLE);
		siw_hal_set_gpio_reset(dev, GPIO_OUT_ZERO);
		touch_msleep(1 + hal_dbg_delay(chip, HAL_DBG_DLY_HW_RST_0));

		noti_str = "TOUCH_RESET_START";
		break;
	case LCD_EVENT_TOUCH_RESET_END:
		atomic_set(&ts->state.hw_reset, event);

		t_dev_info(dev, "notify: lcd_event: touch reset end\n");
		siw_hal_set_gpio_reset(dev, GPIO_OUT_ONE);

		siw_touch_qd_init_work_hw(ts);

		noti_str = "TOUCH_RESET_END";
		break;
	case LCD_EVENT_LCD_MODE:
		noti_str = "LCD_MODE";

		t_dev_info(dev, "notify: lcd_event: lcd mode\n");
		if (data == NULL) {
			t_dev_err(dev, "data is null, cancled\n");
			break;
		}

		ret = siw_hal_lcd_mode(dev, value);
		if (ret < 0)
			break;
		ret = siw_hal_check_mode(dev);
		if (!ret)
			queue_delayed_work(ts->wq, &chip->fb_notify_work, 0);
		ret = 0;
		break;

	case LCD_EVENT_READ_REG:
		t_dev_info(dev, "notify: lcd event: read reg\n");
		siw_hal_lcd_event_read_reg(dev);

		noti_str = "READ_REG";
		break;

	case NOTIFY_CONNECTION:
		t_dev_info(dev, "notify: connection\n");
		ret = siw_hal_usb_status(dev, value);

		noti_str = "CONNECTION";
		break;
	case NOTIFY_WIRELEES:
		t_dev_info(dev, "notify: wireless\n");
		ret = siw_hal_wireless_status(dev, value);

		noti_str = "WIRELESS";
		break;
	case NOTIFY_EARJACK:
		t_dev_info(dev, "notify: earjack\n");
		ret = siw_hal_earjack_status(dev, value);

		noti_str = "EARJACK";
		break;
	case NOTIFY_IME_STATE:
		t_dev_info(dev, "notify: do nothing for ime\n");

		noti_str = "IME_STATE";
		break;
	case NOTIFY_DEBUG_TOOL:
		ret = siw_hal_debug_tool(dev, value);
		t_dev_info(dev, "notify: debug tool\n");

		noti_str = "DEBUG_TOOL";
		break;
	case NOTIFY_CALL_STATE:
		t_dev_info(dev, "notify: call state\n");
		ret = siw_hal_write_value(dev,
					  reg->call_state,
					  value);

		noti_str = "CALL_STATE";
		break;
	case LCD_EVENT_TOUCH_DRIVER_REGISTERED:
	case LCD_EVENT_TOUCH_DRIVER_UNREGISTERED:
		if (0) {
			/* from siw_touch_probe */
			t_dev_info(dev, "notify: driver %s\n",
				(event == LCD_EVENT_TOUCH_DRIVER_REGISTERED) ?
				"registered" : "unregistered");
		}
		noti_str = "DRV";
		break;
	case LCD_EVENT_TOUCH_WATCH_LUT_UPDATE:
		t_dev_info(dev, "notify: WATCH_LUT_UPDATE(%lu)\n", event);
		noti_str = "WATCH_LUT";
		break;
	case LCD_EVENT_TOUCH_WATCH_POS_UPDATE:
		t_dev_info(dev, "notify: WATCH_POS_UPDATE(%lu)\n", event);
		noti_str = "WATCH_POS";
		break;
	case LCD_EVENT_TOUCH_PROXY_STATUS:
		t_dev_info(dev, "notify: PROXY_STATUS(%lu)\n", event);
		noti_str = "PROXY";
		break;
	case LCD_EVENT_TOUCH_ESD_DETECTED:
		t_dev_info(dev,
			"notify: ESD_DETECTED(%lu, %d)\n", event, value);
		noti_str = "ESD";
		break;
	default:
		t_dev_err(dev, "notify: %lu is not supported\n", event);
		break;
	}

	siwmon_submit_evt(dev, "NOTIFY", 0, noti_str, event, value, ret);

	return ret;
}

enum {
	SIW_GET_CHIP_NAME	= (1 << 0),
	SIW_GET_VERSION		= (1 << 1),
	SIW_GET_REVISION	= (1 << 2),
	SIW_GET_PRODUCT		= (1 << 3),
	/* */
	SIW_GET_OPT1		= (1 << 8),
	/* */
	SIW_GET_VER_SIMPLE	= (1 << 16),
	/* */
	SIW_GET_ALL			= 0xFFFF,
};

static int siw_hal_get_cmd_version(struct device *dev,
					char *buf, int flag)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	struct siw_hal_fw_info *fw = &chip->fw;
	int offset = 0;

	if (!flag)
		return 0;

	if (flag & SIW_GET_CHIP_NAME) {
		offset += siw_snprintf(buf, offset,
				       "chip : %s\n",
				       touch_chip_name(ts));
	}

	if (flag & (SIW_GET_VERSION | SIW_GET_VER_SIMPLE)) {
		char *ver_tag = (flag & SIW_GET_VER_SIMPLE) ?
			"" : "version : ";
		if (fw->version_ext) {
			offset += siw_snprintf(buf, offset,
				"%s%08X(%u.%02u)\n",
				ver_tag,
				fw->version_ext,
				fw->v.version.major, fw->v.version.minor);
		} else {
			offset += siw_snprintf(buf, offset,
				"%sv%u.%02u\n",
				ver_tag,
				fw->v.version.major, fw->v.version.minor);
		}
	}

	if (flag & SIW_GET_REVISION) {
		if (chip->fw.revision == 0xFF) {
			offset += siw_snprintf(buf, offset,
				"revision : Flash Erased(0xFF)\n");
		} else {
			offset += siw_snprintf(buf, offset,
				"revision : %d\n", fw->revision);
		}
	}

	if (flag & SIW_GET_PRODUCT) {
		offset += siw_snprintf(buf, offset,
				"product id : %s\n", fw->product_id);
	}

	if (flag & SIW_GET_OPT1) {
		if (chip->opt.f_info_more) {
			offset += siw_snprintf(buf, offset,
					       "fpc : %d\n", fw->fpc);
			offset += siw_snprintf(buf, offset,
					       "wfr : %d\n", fw->wfr);
			offset += siw_snprintf(buf, offset,
					       "cg  : %d\n", fw->cg);
			offset += siw_snprintf(buf, offset,
					       "lot : %d\n", fw->lot);
			offset += siw_snprintf(buf, offset,
					       "serial : 0x%X\n", fw->sn);
			offset += siw_snprintf(buf, offset,
				"date : %04d.%02d.%02d %02d:%02d:%02d Site%d\n",
				fw->date & 0xFFFF,
				((fw->date >> 16) & 0xFF),
				((fw->date >> 24) & 0xFF),
				fw->time & 0xFF,
				((fw->time >> 8) & 0xFF),
				((fw->time >> 16) & 0xFF),
				((fw->time >> 24) & 0xFF));
		}
	}

	return offset;
}

static int siw_hal_set(struct device *dev, u32 cmd, void *buf)
{
	return 0;
}

static int siw_hal_get(struct device *dev, u32 cmd, void *buf)
{
	int ret = 0;

	t_dev_dbg_base(dev, "cmd %d\n", cmd);

	switch (cmd) {
	case CMD_VERSION:
		ret = siw_hal_get_cmd_version(dev, (char *)buf, SIW_GET_ALL);
		break;

	case CMD_ATCMD_VERSION:
		ret = siw_hal_get_cmd_version(dev,
			(char *)buf, SIW_GET_VER_SIMPLE);
		break;

	default:
		break;
	}

	return ret;
}

u32 t_mon_dbg_mask;

/* usage
 * (1) echo <value> > /sys/module/{Module Name}/parameters/mon_dbg_mask
 * (2) insmod {Siw Touch Module Name}.ko mon_dbg_mask=<value>
 */
module_param_named(mon_dbg_mask, t_mon_dbg_mask, uint,
		   S_IRUGO | S_IWUSR | S_IWGRP);

#define t_mon_info(_dev, fmt, args...)	__t_dev_info(_dev, "mon: " fmt, ##args)
#define t_mon_warn(_dev, fmt, args...)	__t_dev_warn(_dev, "mon: " fmt, ##args)

#define t_mon_err(_dev, fmt, args...)	__t_dev_err(_dev, "mon: " fmt, ##args)

#define t_mon_dbg(condition, _dev, fmt, args...)			\
		do {							\
			if (unlikely(t_mon_dbg_mask & (condition)))	\
				__t_dev_info(_dev, "mon: " fmt, ##args);\
		} while (0)

#define t_mon_dbg_base(_dev, fmt, args...)	\
		t_mon_dbg(DBG_BASE, _dev, fmt, ##args)

#define t_mon_dbg_trace(_dev, fmt, args...)	\
		t_mon_dbg(DBG_TRACE, _dev, fmt, ##args)

static int siw_hal_mon_handler_chk_frame(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	u32 frame_addr = 0;
	u32 frame_s = 0;
	u32 frame_e = 0;
	u32 delay = 0;
	int cnt = 0;
	int i;
	int ret = 0;

	switch (chip->opt.t_chk_frame) {
	case 1:
		frame_addr = 0x24F;
		cnt = 20;
		delay = 1;
		break;
	default:
		return 0;
	}

	ret = siw_hal_read_value(dev,
				 frame_addr,
				 (void *)&frame_s);
	if (ret < 0)
		goto out;

	for (i = 0; i < cnt; i++) {
		touch_msleep(delay);

		ret = siw_hal_read_value(dev,
					 frame_addr,
					 (void *)&frame_e);
		if (ret < 0)
			goto out;

		if (frame_e != frame_s) {
			t_mon_dbg_trace(dev, "frame ok: %d(%d), %d x %d ms\n",
					frame_e, frame_s, i, delay);
			return 0;
		}
	}

	t_mon_err(dev, "frame not changed: %d, %d x %d ms\n",
		  frame_s, cnt, delay);

	ret = -ERESTART;

out:
	return ret;
}

static int siw_hal_mon_handler_chk_status(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	u32 dbg_mask = (t_mon_dbg_mask & DBG_TRACE) ? 0 : 0x80;
	u32 ic_status;
	u32 status;
	int ret = 0;

	ret = siw_hal_reg_read(dev,
			       reg->tc_ic_status,
			       (void *)&ic_status, sizeof(ic_status));
	if (ret < 0)
		goto out;

	ret = siw_hal_reg_read(dev,
			       reg->tc_status,
			       (void *)&status, sizeof(status));
	if (ret < 0)
		goto out;

	status |= 0x8000;
	ret = siw_hal_do_check_status(dev, status, ic_status, dbg_mask);
	if (ret < 0) {
		if (ret == -ERESTART)
			goto out;
		ret = 0;
	}

out:
	return ret;
}

static int siw_hal_mon_handler_chk_id(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	struct siw_hal_fw_info *fw = &chip->fw;
	u32 chip_id;
	int ret = 0;

	ret = siw_hal_read_value(dev,
				 reg->spr_chip_id,
				 &chip_id);
	if (ret < 0)
		goto out;

	if (fw->chip_id_raw != chip_id) {
		ret = -ERESTART;
		goto out;
	}

out:
	return ret;
}

struct siw_mon_hanlder_op {
	unsigned int step;
	unsigned int delay;
	unsigned int retry;
	char *name;
	int (*func)(struct device *dev);
};

#define SIW_MON_HANDLER_OP_SET(_step, _delay, _retry, _name, _func)	\
{	\
	.step = _step,	\
	.delay = _delay,	\
	.retry = _retry,	\
	.name = _name,	\
	.func = _func,	\
}

static const struct siw_mon_hanlder_op siw_mon_hanlder_ops[] = {
	[0] = SIW_MON_HANDLER_OP_SET(0, 10, 3, "id",
		siw_hal_mon_handler_chk_id),
	[1] = SIW_MON_HANDLER_OP_SET(1, 10, 3, "status",
		siw_hal_mon_handler_chk_status),
	[2] = SIW_MON_HANDLER_OP_SET(2, 10, 3, "frame",
		siw_hal_mon_handler_chk_frame),
};

static int siw_hal_mon_hanlder_do_op(struct device *dev,
			const struct siw_mon_hanlder_op *op, char *p_name)
{
	unsigned int delay = op->delay;
	unsigned int retry = op->retry;
	unsigned int i;
	int ret = 0;

	for (i = 0; i < retry; i++) {
		ret = op->func(dev);
		if (ret >= 0) {
			t_mon_dbg_trace(dev,
					"%s : [%d] %s check done\n",
					p_name, op->step, op->name);
			break;
		}

		t_mon_err(dev,
			  "%s : [%d] %s check failed(%d), %d (%d)\n",
			  p_name, op->step, op->name, i, ret, op->delay);

		touch_msleep(delay);
	}

	return ret;
}

static int siw_hal_mon_handler_skip(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	if (atomic_read(&ts->state.sleep) != IC_NORMAL)
		return 1;

	if (atomic_read(&ts->state.fb) != FB_RESUME)
		return 1;

	if (atomic_read(&ts->state.pm) != DEV_PM_RESUME)
		return 1;

	if (atomic_read(&ts->state.core) != CORE_NORMAL)
		return 1;

	if (atomic_read(&chip->init) != IC_INIT_DONE)
		return 1;

	if (chip->lcd_mode != LCD_MODE_U3)
		return 1;

	if (chip->driving_mode != LCD_MODE_U3)
		return 1;

	return 0;
}

static void siw_hal_mon_handler_self_reset(struct device *dev,
	char *title)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	const struct siw_mon_hanlder_op *ops = siw_mon_hanlder_ops;
	unsigned int ops_num = ARRAY_SIZE(siw_mon_hanlder_ops);
	char name[32];
	int step;
	int ret = 0;

	if (siw_hal_mon_handler_skip(dev))
		return;

	mutex_lock(&ts->lock);

	snprintf(name, sizeof(name), "%s self-reset", title);

	for (step = 0 ; step < ops_num ; step++, ops++) {
		if ((ops->step >= ops_num) ||
		    (ops->name == NULL) ||
		    (ops->func == NULL))
			break;

		ret = siw_hal_mon_hanlder_do_op(dev, ops, name);
		if (ret < 0)
			break;
	}

	if (ret < 0) {
		t_mon_err(dev,
			  "%s : recovery begins(hw reset)\n",
			  name);

		siw_hal_reset_ctrl(dev, HW_RESET_ASYNC);
	} else {
		t_mon_dbg_trace(dev,
				"%s : check ok\n",
				name);
	}

	mutex_unlock(&ts->lock);
}

static int siw_hal_mon_handler(struct device *dev, u32 opt)
{
	char *name;

	name = (opt & MON_FLAG_RST_ONLY) ? "reset cond" : "mon handler";

	t_mon_dbg_trace(dev, "%s begins\n", name);

	siw_hal_mon_handler_self_reset(dev, name);

	if (opt & MON_FLAG_RST_ONLY)
		goto out;

	/*
	 * For other controls
	 */

out:
	t_mon_dbg_trace(dev, "%s ends\n", name);

	return 0;
}

static int siw_hal_early_probe(struct device *dev)
{
	return 0;
}

enum {
	IC_TEST_ADDR_NOT_VALID = 0x8000,
};

int siw_hal_ic_test_unit(struct device *dev, u32 data)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	u32 data_rd;
	int ret;

	if (!reg->spr_chip_test) {
		t_dev_warn(dev, "ic test addr not valid, skip\n");
		return IC_TEST_ADDR_NOT_VALID;
	}

	ret = siw_hal_write_value(dev,
				  reg->spr_chip_test,
				  data);
	if (ret < 0) {
		t_dev_err(dev, "ic test wr err, %08Xh, %d\n", data, ret);
		goto out;
	}

	ret = siw_hal_read_value(dev,
				 reg->spr_chip_test,
				 &data_rd);
	if (ret < 0) {
		t_dev_err(dev, "ic test rd err: %08Xh, %d\n", data, ret);
		goto out;
	}

	if (data != data_rd) {
		t_dev_err(dev,
			"ic test cmp err, %08Xh, %08Xh\n", data, data_rd);
		ret = -EFAULT;
		goto out;
	}

out:
	return ret;
}

static int siw_hal_ic_test(struct device *dev)
{
	u32 data[] = {
		0x5A5A5A5A,
		0xA5A5A5A5,
		0xF0F0F0F0,
		0x0F0F0F0F,
		0xFF00FF00,
		0x00FF00FF,
		0xFFFF0000,
		0x0000FFFF,
		0xFFFFFFFF,
		0x00000000,
	};
	int i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(data); i++) {
		ret = siw_hal_ic_test_unit(dev, data[i]);
		if ((ret == IC_TEST_ADDR_NOT_VALID) || (ret < 0))
			break;
	}

	if (ret >= 0)
		t_dev_dbg_base(dev, "ic bus r/w test done\n");

	return ret;
}

static int siw_hal_get_product_id(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_hal_reg *reg = chip->reg;
	struct siw_hal_fw_info *fw = &chip->fw;
	u8 product_id[8 + 4] = { 0, };
	int ret;

	ret = siw_hal_reg_read(dev,
			       reg->tc_product_id1,
			       (void *)product_id, 8);
	if (ret < 0) {
		t_dev_err(dev,
			  "failed to read product id, %d\n",
			  ret);
		return ret;
	}

	siw_hal_fw_set_prod_id(fw, (u8 *)product_id, 8);

	t_dev_dbg_base(dev, "product id - %s\n", fw->product_id);

	return 0;
}

static int siw_hal_chipset_check(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int ret = 0;

	touch_msleep(ts->caps.hw_reset_delay);

	ret = siw_hal_init_quirk(dev);
	if (ret < 0)
		goto out;

	ret = siw_hal_ic_test(dev);
	if (ret < 0)
		goto out;

	ret = siw_hal_chk_boot(dev);
	if (ret < 0)
		goto out;
	if (ret) {
		t_dev_err(dev, "Boot fail detected\n");
		return 0;
	}

	/*
	 * Preceding process
	 * : get reference value to cope with product variation
	 */
	ret = siw_hal_get_product_id(dev);
	if (ret < 0)
		goto out;

out:
	return ret;
}

static int siw_hal_chipset_option(struct siw_touch_chip *chip)
{
	struct device *dev = chip->dev;
	struct siw_touch_chip_opt *opt = &chip->opt;
	struct siw_ts *ts = chip->ts;

	chip->drv_delay = 20;
	chip->drv_opt_delay = 0;

	switch (touch_chip_type(ts)) {
	case CHIP_LG4894:
		chip->drv_delay = 60;

		opt->t_chk_tci_debug = 1;
		break;

	case CHIP_LG4895:
		chip->drv_opt_delay = 200;

		opt->f_u2_blank_chg = 1;
		opt->t_clock = 1;
		opt->t_sw_rst = 1;
		break;

	case CHIP_LG4946:
		opt->f_info_more = 1;
		opt->f_glove_en = 1;
		opt->t_chk_mode = 1;
		opt->t_clock = 1;
		opt->t_sw_rst = 1;
		break;

	case CHIP_SW1828:
		opt->f_ver_ext = 1;
		opt->f_dbg_report = 1;
		opt->t_chk_frame = 1;
		break;

	case CHIP_SW49105:
		opt->f_attn_opt = 1;
		opt->t_sw_rst = 2;
		opt->t_chk_sys_error = 1;
		opt->t_chk_sys_fault = 1;
		break;

	case CHIP_SW49106:
		opt->f_attn_opt = 1;
		opt->t_boot_mode = 1;
		opt->t_sts_mask = 1;
		opt->t_sw_rst = 2;
		opt->t_chk_tci_debug = 1;
		break;

	case CHIP_SW49406:
		opt->f_attn_opt = 1;
		break;

	case CHIP_SW49407:
		opt->f_attn_opt = 1;
		opt->f_glove_en = 1;
		opt->f_grab_en = 1;
		opt->f_dbg_report = 1;
		opt->f_u2_blank_chg = 1;
		opt->t_sw_rst = 2;
		opt->t_clock = 2;
		opt->t_chk_mipi = 1;
		opt->t_chk_sys_error = 1;
		opt->t_chk_sys_fault = 1;
		opt->t_chk_fault = 1;
		break;

	case CHIP_SW49408:
		opt->f_attn_opt = 1;
		opt->t_chk_mode = 1;
		opt->t_sw_rst = 2;
		opt->t_clock = 2;
		opt->t_chk_sys_error = 2;
		opt->t_chk_sys_fault = 1;
		break;

	case CHIP_SW49409:
		opt->t_sw_rst = 2;
		opt->t_clock = 2;
		opt->t_chk_sys_error = 3;
		opt->t_chk_sys_fault = 1;
		break;

	case CHIP_SW49501:
		opt->f_glove_en = 1;
		opt->f_grab_en = 1;
		opt->f_dbg_report = 1;
		opt->t_boot_mode = 2;
		opt->t_sts_mask = 2;
		opt->t_sw_rst = 2;
		opt->t_clock = 2;
		opt->t_chk_tci_debug = 1;
		break;
	}

	t_dev_info(dev, "[opt summary]\n");
	t_dev_info(dev, " f_info_more     : %d\n", opt->f_info_more);
	t_dev_info(dev, " f_ver_ext       : %d\n", opt->f_ver_ext);
	t_dev_info(dev, " f_attn_opt      : %d\n", opt->f_attn_opt);
	t_dev_info(dev, " f_glove_en      : %d\n", opt->f_glove_en);
	t_dev_info(dev, " f_grab_en       : %d\n", opt->f_grab_en);
	t_dev_info(dev, " f_dbg_report    : %d\n", opt->f_dbg_report);
	t_dev_info(dev, " f_u2_blank_chg  : %d\n", opt->f_u2_blank_chg);
	t_dev_info(dev, " t_boot_mode     : %d\n", opt->t_boot_mode);
	t_dev_info(dev, " t_sts_mask      : %d\n", opt->t_sts_mask);
	t_dev_info(dev, " t_chk_mode      : %d\n", opt->t_chk_mode);
	t_dev_info(dev, " t_sw_rst        : %d\n", opt->t_sw_rst);
	t_dev_info(dev, " t_clock         : %d\n", opt->t_clock);
	t_dev_info(dev, " t_chk_mipi      : %d\n", opt->t_chk_mipi);
	t_dev_info(dev, " t_chk_frame     : %d\n", opt->t_chk_frame);
	t_dev_info(dev, " t_chk_tci_debug : %d\n", opt->t_chk_tci_debug);
	t_dev_info(dev, " t_chk_sys_error : %d\n", opt->t_chk_sys_error);
	t_dev_info(dev, " t_chk_sys_fault : %d\n", opt->t_chk_sys_fault);
	t_dev_info(dev, " t_chk_fault     : %d\n", opt->t_chk_fault);

	t_dev_info(dev, " drv_delay       : %d ms\n", chip->drv_delay);
	t_dev_info(dev, " drv_opt_delay   : %d ms\n", chip->drv_opt_delay);

	return 0;
}

static void __siw_hal_do_remove(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

#if defined(__SIW_SUPPORT_PM_QOS)
	pm_qos_remove_request(&chip->pm_qos_req);
#endif

	siw_hal_power_free(dev);
	siw_hal_free_gpios(dev);

	siw_hal_free_works(chip);
	siw_hal_free_locks(chip);

	touch_set_dev_data(ts, NULL);

	touch_kfree(dev, chip);
}

static int siw_hal_probe(struct device *dev)
{
	struct siw_ts *ts = to_touch_core(dev);
	struct siw_touch_chip *chip = NULL;
	int ret = 0;

	chip = touch_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		t_dev_err(dev, "failed to allocate %s data\n",
			  touch_chip_name(ts));
		return -ENOMEM;
	}

	chip->dev = dev;
	chip->reg = siw_ops_reg(ts);
	chip->ts = ts;

	touch_set_dev_data(ts, chip);

	siw_hal_chipset_option(chip);

	siw_hal_init_locks(chip);
	siw_hal_init_works(chip);

	siw_hal_init_gpios(dev);
	siw_hal_power_init(dev);

	if (siw_touch_get_boot_mode() == SIW_TOUCH_CHARGER_MODE) {
		if (touch_mode_allowed(ts, LCD_MODE_U3_PARTIAL)) {
			/* U3P driving and maintain 100ms before Deep sleep */
			ret = siw_hal_tc_driving(dev, LCD_MODE_U3_PARTIAL);
			if (ret < 0) {
				__siw_hal_do_remove(dev);
				siwmon_submit_ops_step_chip_wh_name(dev,
						"%s probe failed(charger mode)",
						touch_chip_name(ts), 0);
				return ret;
			}
			touch_msleep(80);
		}

		/* Deep Sleep */
		siw_hal_deep_sleep(dev);

		siwmon_submit_ops_step_chip_wh_name(dev,
						"%s probe done(charger mode)",
						touch_chip_name(ts), 0);
		return 0;
	}

	siw_hal_get_tci_info(dev);
	siw_hal_get_swipe_info(dev);

#if defined(__SIW_SUPPORT_PM_QOS)
	pm_qos_add_request(&chip->pm_qos_req,
			   PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_DEFAULT_VALUE);
#endif

	ret = siw_hal_chipset_check(dev);
	if (ret < 0) {
		__siw_hal_do_remove(dev);
		goto out;
	}

	chip->lcd_mode = LCD_MODE_U3;
	chip->tci_debug_type = 1;

	t_dev_dbg_base(dev, "%s probe done\n",
		       touch_chip_name(ts));

	siwmon_submit_ops_step_chip_wh_name(dev, "%s probe done",
					    touch_chip_name(ts), 0);

	return 0;

out:
	t_dev_dbg_base(dev, "%s probe failed\n",
		       touch_chip_name(ts));

	siwmon_submit_ops_step_chip_wh_name(dev, "%s probe failed",
					    touch_chip_name(ts), 0);

	return ret;
}

static int siw_hal_remove(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;

	__siw_hal_do_remove(dev);

	t_dev_dbg_base(dev, "%s remove done\n",
		       touch_chip_name(ts));

	return 0;
}

#if defined(__SIW_CONFIG_SYSTEM_PM)
static int siw_hal_do_suspend(struct device *dev)
{
	siw_touch_irq_control(dev, INTERRUPT_DISABLE);

	siw_hal_power(dev, POWER_OFF);

	return 0;
}

static int siw_hal_do_resume(struct device *dev)
{
	struct siw_ts *ts = to_touch_core(dev);

	siw_hal_power(dev, POWER_ON);

	/* Double check for reset */
	siw_hal_trigger_gpio_reset(dev);

	touch_msleep(ts->caps.hw_reset_delay);

	return 0;
}
#else	/* __SIW_CONFIG_SYSTEM_PM */
static int siw_hal_do_suspend(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int mfst_mode = 0;
	int ret = 0;

	if (siw_touch_get_boot_mode() == SIW_TOUCH_CHARGER_MODE)
		return -EPERM;

	mfst_mode = siw_touch_boot_mode_check(dev);
	if ((mfst_mode >= MINIOS_MFTS_FOLDER) && !ts->role.mfts_lpwg) {
		t_dev_info(dev, "touch_suspend - MFTS\n");
		siw_touch_irq_control(dev, INTERRUPT_DISABLE);
		siw_hal_power(dev, POWER_OFF);
		return -EPERM;
	}

	if ((chip->lcd_mode == LCD_MODE_U2) &&
	    siw_hal_watch_is_disp_waton(dev) &&
	    siw_hal_watch_is_rtc_run(dev))
		siw_hal_watch_get_curr_time(dev, NULL, NULL);

	if (atomic_read(&chip->init) == IC_INIT_DONE)
		siw_hal_lpwg_mode(dev);
	else /* need init */
		ret = 1;

	return ret;
}

static int siw_hal_do_resume(struct device *dev)
{
	struct siw_touch_chip *chip = to_touch_chip(dev);
	struct siw_ts *ts = chip->ts;
	int mfst_mode = 0;
	int ret = 0;

	mfst_mode = siw_touch_boot_mode_check(dev);
	if ((mfst_mode >= MINIOS_MFTS_FOLDER) && !ts->role.mfts_lpwg) {
		siw_hal_power(dev, POWER_ON);
		touch_msleep(ts->caps.hw_reset_delay);
		ret = siw_hal_ic_info(dev);
		if (ret < 0)
			t_dev_err(dev, "ic info err, %d\n", ret);
		if (siw_hal_upgrade(dev) == 0) {
			siw_hal_power(dev, POWER_OFF);
			siw_hal_power(dev, POWER_ON);
			touch_msleep(ts->caps.hw_reset_delay);
		}
	}

	if (siw_touch_get_boot_mode() == SIW_TOUCH_CHARGER_MODE) {
		if (touch_mode_allowed(ts, LCD_MODE_U3_PARTIAL)) {
			/* U3P driving and maintain 100ms at Resume */
			ret = siw_hal_tc_driving(dev, LCD_MODE_U3_PARTIAL);
			if (ret < 0)
				return ret;
			touch_msleep(80);
		}

		siw_hal_deep_sleep(dev);
		return -EPERM;
	}

	return 0;
}
#endif	/* __SIW_CONFIG_SYSTEM_PM */

static int siw_hal_suspend(struct device *dev)
{
	struct siw_ts *ts = to_touch_core(dev);
	int ret = 0;

	if (atomic_read(&ts->state.core) != CORE_NORMAL)
		return -EPERM;

	ret = siw_hal_do_suspend(dev);
	if (ret < 0)
		return ret;

	t_dev_dbg_pm(dev, "%s suspend done\n",
		     touch_chip_name(ts));

	return ret;
}

static int siw_hal_resume(struct device *dev)
{
	struct siw_ts *ts = to_touch_core(dev);
	int ret = 0;

	if (atomic_read(&ts->state.core) != CORE_NORMAL)
		return -EPERM;

	ret = siw_hal_do_resume(dev);
	if (ret < 0)
		return ret;

	t_dev_dbg_pm(dev, "%s resume done\n",
		     touch_chip_name(ts));

	return ret;
}

static const struct siw_hal_reg siw_touch_default_reg = {
	.spr_chip_test				= SPR_CHIP_TEST,
	.spr_chip_id				= SPR_CHIP_ID,
	.spr_rst_ctl				= SPR_RST_CTL,
	.spr_boot_ctl				= SPR_BOOT_CTL,
	.spr_sram_ctl				= SPR_SRAM_CTL,
	.spr_boot_status			= SPR_BOOT_STS,
	.spr_subdisp_status			= SPR_SUBDISP_STS,
	.spr_code_offset			= SPR_CODE_OFFSET,
	.tc_ic_status				= TC_IC_STATUS,
	.tc_status					= TC_STS,
	.tc_version					= TC_VERSION,
	.tc_product_id1				= TC_PRODUCT_ID1,
	.tc_product_id2				= TC_PRODUCT_ID2,
	.tc_version_ext				= TC_VERSION_EXT,
	.info_fpc_type				= INFO_FPC_TYPE,
	.info_wfr_type				= INFO_WFR_TYPE,
	.info_chip_version			= INFO_CHIP_VERSION,
	.info_cg_type				= INFO_CG_TYPE,
	.info_lot_num				= INFO_LOT_NUM,
	.info_serial_num			= INFO_SERIAL_NUM,
	.info_date					= INFO_DATE,
	.info_time					= INFO_TIME,
	.cmd_abt_loc_x_start_read	= CMD_ABT_LOC_X_START_READ,
	.cmd_abt_loc_x_end_read		= CMD_ABT_LOC_X_END_READ,
	.cmd_abt_loc_y_start_read	= CMD_ABT_LOC_Y_START_READ,
	.cmd_abt_loc_y_end_read		= CMD_ABT_LOC_Y_END_READ,
	.code_access_addr			= CODE_ACCESS_ADDR,
	.data_i2cbase_addr			= DATA_I2CBASE_ADDR,
	.prd_tcm_base_addr			= PRD_TCM_BASE_ADDR,
	.tc_device_ctl				= TC_DEVICE_CTL,
	.tc_interrupt_ctl			= TC_INTERRUPT_CTL,
	.tc_interrupt_status		= TC_INTERRUPT_STS,
	.tc_drive_ctl				= TC_DRIVE_CTL,
	.tci_enable_w				= TCI_ENABLE_W,
	.tap_count_w				= TAP_COUNT_W,
	.min_intertap_w				= MIN_INTERTAP_W,
	.max_intertap_w				= MAX_INTERTAP_W,
	.touch_slop_w				= TOUCH_SLOP_W,
	.tap_distance_w				= TAP_DISTANCE_W,
	.int_delay_w				= INT_DELAY_W,
	.act_area_x1_w				= ACT_AREA_X1_W,
	.act_area_y1_w				= ACT_AREA_Y1_W,
	.act_area_x2_w				= ACT_AREA_X2_W,
	.act_area_y2_w				= ACT_AREA_Y2_W,
	.tci_debug_fail_ctrl		= TCI_DEBUG_FAIL_CTRL,
	.tci_debug_fail_buffer		= TCI_DEBUG_FAIL_BUFFER,
	.tci_debug_fail_status		= TCI_DEBUG_FAIL_STATUS,
	.swipe_enable_w				= SWIPE_ENABLE_W,
	.swipe_dist_w				= SWIPE_DIST_W,
	.swipe_ratio_thr_w			= SWIPE_RATIO_THR_W,
	.swipe_ratio_period_w		= SWIPE_RATIO_PERIOD_W,
	.swipe_ratio_dist_w			= SWIPE_RATIO_DIST_W,
	.swipe_time_min_w			= SWIPE_TIME_MIN_W,
	.swipe_time_max_w			= SWIPE_TIME_MAX_W,
	.swipe_act_area_x1_w		= SWIPE_ACT_AREA_X1_W,
	.swipe_act_area_y1_w		= SWIPE_ACT_AREA_Y1_W,
	.swipe_act_area_x2_w		= SWIPE_ACT_AREA_X2_W,
	.swipe_act_area_y2_w		= SWIPE_ACT_AREA_Y2_W,
	.swipe_fail_debug_r			= SWIPE_FAIL_DEBUG_R,
	.swipe_debug_r				= SWIPE_DEBUG_R,
	.cmd_raw_data_report_mode_read	= CMD_RAW_DATA_REPORT_MODE_READ,
	.cmd_raw_data_compress_write	= CMD_RAW_DATA_COMPRESS_WRITE,
	.cmd_raw_data_report_mode_write	= CMD_RAW_DATA_REPORT_MODE_WRITE,
	.spr_charger_status				= SPR_CHARGER_STS,
	.ime_state					= IME_STATE,
	.max_delta					= MAX_DELTA,
	.touch_max_w				= TOUCH_MAX_W,
	.touch_max_r				= TOUCH_MAX_R,
	.call_state					= CALL_STATE,
	.tc_tsp_test_ctl			= TC_TSP_TEST_CTL,
	.tc_tsp_test_status			= TC_TSP_TEST_STS,
	.tc_tsp_test_pf_result		= TC_TSP_TEST_PF_RESULT,
	.tc_tsp_test_off_info		= TC_TSP_TEST_OFF_INFO,
	.tc_flash_dn_status			= TC_FLASH_DN_STS,
	.tc_confdn_base_addr		= TC_CONFDN_BASE_ADDR,
	.tc_flash_dn_ctl			= TC_FLASH_DN_CTL,
	.raw_data_ctl_read			= RAW_DATA_CTL_READ,
	.raw_data_ctl_write			= RAW_DATA_CTL_WRITE,
	.serial_data_offset			= SERIAL_DATA_OFFSET,
	/* __SIW_SUPPORT_WATCH */
	.ext_watch_font_offset		= EXT_WATCH_FONT_OFFSET,
	.ext_watch_font_addr		= EXT_WATCH_FONT_ADDR,
	.ext_watch_font_dn_addr_info = EXT_WATCH_FONT_DN_ADDR_INFO,
	.ext_watch_font_crc			= EXT_WATCH_FONT_CRC,
	.ext_watch_dcs_ctrl			= EXT_WATCH_DCS_CTRL,
	.ext_watch_mem_ctrl			= EXT_WATCH_MEM_CTRL,
	.ext_watch_ctrl				= EXT_WATCH_CTRL,
	.ext_watch_area_x			= EXT_WATCH_AREA_X,
	.ext_watch_area_y			= EXT_WATCH_AREA_Y,
	.ext_watch_blink_area		= EXT_WATCH_BLINK_AREA,
	.ext_watch_lut				= EXT_WATCH_LUT,
	.ext_watch_display_on		= EXT_WATCH_DISPLAY_ON,
	.ext_watch_display_status	= EXT_WATCH_DISPLAY_STATUS,
	.ext_watch_rtc_sct			= EXT_WATCH_RTC_SCT,
	.ext_watch_rtc_sctcnt		= EXT_WATCH_RTC_SCTCNT,
	.ext_watch_rtc_capture		= EXT_WATCH_RTC_CAPTURE,
	.ext_watch_rtc_ctst			= EXT_WATCH_RTC_CTST,
	.ext_watch_rtc_ecnt			= EXT_WATCH_RTC_ECNT,
	.ext_watch_hour_disp		= EXT_WATCH_HOUR_DISP,
	.ext_watch_blink_prd		= EXT_WATCH_BLINK_PRD,
	.ext_watch_rtc_run			= EXT_WATCH_RTC_RUN,
	.ext_watch_position			= EXT_WATCH_POSITION,
	.ext_watch_position_r		= EXT_WATCH_POSITION_R,
	.ext_watch_state			= EXT_WATCH_STATE,
	.sys_dispmode_status		= SYS_DISPMODE_STATUS,
	/* __SIW_SUPPORT_PRD */
	.prd_serial_tcm_offset		= PRD_SERIAL_TCM_OFFSET,
	.prd_tc_mem_sel				= PRD_TC_MEM_SEL,
	.prd_tc_test_mode_ctl		= PRD_TC_TEST_MODE_CTL,
	.prd_m1_m2_raw_offset		= PRD_M1_M2_RAW_OFFSET,
	.prd_tune_result_offset		= PRD_TUNE_RESULT_OFFSET,
	.prd_open3_short_offset		= PRD_OPEN3_SHORT_OFFSET,
	.prd_ic_ait_start_reg		= PRD_IC_AIT_START_REG,
	.prd_ic_ait_data_readystatus = PRD_IC_AIT_DATA_READYSTATUS,
	/* */
	.glove_en					= GLOVE_EN,
	.grab_en					= GRAB_EN,
};

enum {
	HAL_MON_INTERVAL_DEFAULT = 5,
};

static const struct siw_touch_operations siw_touch_default_ops = {
	/* Register Map */
	.reg				= (void *) &siw_touch_default_reg,
	/* Functions */
	.early_probe		= siw_hal_early_probe,
	.probe				= siw_hal_probe,
	.remove				= siw_hal_remove,
	.suspend			= siw_hal_suspend,
	.resume				= siw_hal_resume,
	.init				= siw_hal_init,
	.reset				= siw_hal_reset_ctrl,
	.ic_info			= siw_hal_ic_info,
	.tc_con				= siw_hal_tc_con,
	.tc_driving			= siw_hal_tc_driving,
	.chk_status			= siw_hal_check_status,
	.irq_handler		= siw_hal_irq_handler,
	.irq_abs			= siw_hal_irq_abs,
	.irq_lpwg			= siw_hal_irq_lpwg,
	.power				= siw_hal_power,
	.upgrade			= siw_hal_upgrade,
	.lpwg				= siw_hal_lpwg,
	.asc				= siw_hal_asc,
	.notify				= siw_hal_notify,
	.set				= siw_hal_set,
	.get				= siw_hal_get,
	/* */
	.sysfs				= siw_hal_sysfs,
	/* */
	.mon_handler		= siw_hal_mon_handler,
	.mon_interval		= HAL_MON_INTERVAL_DEFAULT,
	/* */
	.abt_sysfs			= siw_hal_abt_sysfs,
	.prd_sysfs			= siw_hal_prd_sysfs,
	.watch_sysfs		= siw_hal_watch_sysfs,
};

struct siw_touch_operations *siw_hal_get_default_ops(int opt)
{
	return (struct siw_touch_operations *)&siw_touch_default_ops;
}


