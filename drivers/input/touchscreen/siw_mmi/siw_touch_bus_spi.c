/*
 * siw_touch_bus_spi.c - SiW touch bus spi driver
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
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <asm/memory.h>

#include <linux/platform_device.h>
#include <linux/spi/spi.h>

#include "siw_touch.h"
#include "siw_touch_bus.h"
#include "siw_touch_irq.h"

#if defined(CONFIG_SPI_MASTER)

#define siwmon_submit_bus_spi_read(_spi, _data, _ret)	\
		siwmon_submit_bus(&_spi->dev, "SPI_R", _data, _ret)

#define siwmon_submit_bus_spi_write(_spi, _data, _ret)	\
		siwmon_submit_bus(&_spi->dev, "SPI_W", _data, _ret)

#define siwmon_submit_bus_spi_xfer(_spi, _data, _ret)	\
		siwmon_submit_bus(&_spi->dev, "SPI_X", _data, _ret)

static void siw_touch_spi_message_init(struct spi_device *spi,
						struct spi_message *m)
{
	spi_message_init(m);
}

static void siw_touch_spi_message_add_tail(struct spi_device *spi,
						struct spi_transfer *x,
						struct spi_message *m)
{
	spi_message_add_tail(x, m);
}

static int siw_touch_spi_sync(struct spi_device *spi,
				struct spi_message *m)
{
	return spi_sync(spi, m);
}

static void siw_touch_spi_err_dump(struct spi_device *spi,
				struct spi_transfer *xs, int num, int _read)
{
	struct spi_transfer *x = xs;
	int i;

	t_dev_err(&spi->dev, "spi transfer err :\n");
	for (i = 0; i < num; i++) {
		t_dev_err(&spi->dev,
				" x[%d] - len %d, cs %d, bpw %d\n",
				i,
				x->len, x->cs_change, spi->bits_per_word);
		siw_touch_bus_err_dump_data(&spi->dev,
					(char *)x->tx_buf, x->len, i, "t");
		if (_read) {
			siw_touch_bus_err_dump_data(&spi->dev,
					(char *)x->rx_buf, x->len, i, "r");
		}
		x++;
	}
}

static int siw_touch_spi_init(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	int ret;

	ret = spi_setup(spi);
	if (ret < 0) {
		t_dev_err(dev, "failed to perform SPI setup\n");
		return ret;
	}

	t_dev_info(dev, "spi init: %d.%d Mhz, mode %d, bpw %d, cs %d (%s)\n",
			freq_to_mhz_unit(spi->max_speed_hz),
			freq_to_khz_top(spi->max_speed_hz),
			spi->mode,
			spi->bits_per_word,
			spi->chip_select,
			dev_name(&spi->master->dev));

	return 0;
}

static int siw_touch_spi_do_read(struct spi_device *spi,
					struct touch_bus_msg *msg)
{
	struct spi_transfer x = {
		.cs_change = 0,
		.bits_per_word = spi->bits_per_word,
		.delay_usecs = 0,
		.speed_hz = spi->max_speed_hz,
	};
	struct spi_message m;
	int ret = 0;

	/*
	 * Bus control can need to be modifyed up to main chipset spec.
	 */

	if ((msg->rx_size > SIW_TOUCH_MAX_BUF_SIZE) ||
		(msg->tx_size > SIW_TOUCH_MAX_BUF_SIZE)) {
		t_dev_err(&spi->dev, "spi rd: buffer overflow - rx %Xh, tx %Xh\n",
			msg->rx_size, msg->tx_size);
		return -EOVERFLOW;
	}

	siw_touch_spi_message_init(spi, &m);

	x.tx_buf = msg->tx_buf;
	x.rx_buf = msg->rx_buf;
	x.tx_dma = msg->tx_dma;
	x.rx_dma = msg->rx_dma;
	x.len = msg->rx_size;

	siw_touch_spi_message_add_tail(spi, &x, &m);

	ret = siw_touch_spi_sync(spi, &m);
	siwmon_submit_bus_spi_read(spi, msg, ret);
	if (ret < 0)
		siw_touch_spi_err_dump(spi, &x, 1, 1);

	return ret;
}

static int siw_touch_spi_read(struct device *dev, void *msg)
{
	return siw_touch_spi_do_read(to_spi_device(dev),
		(struct touch_bus_msg *)msg);
}

int siw_touch_spi_do_write(struct spi_device *spi,
						struct touch_bus_msg *msg)
{
	struct spi_transfer x = {
		.cs_change = 0,
		.bits_per_word = spi->bits_per_word,
		.delay_usecs = 0,
		.speed_hz = spi->max_speed_hz,
	};
	struct spi_message m;
	int ret = 0;

	/*
	 * Bus control can need to be modifyed up to main chipset spec.
	 */

	if (msg->tx_size > SIW_TOUCH_MAX_BUF_SIZE) {
		t_dev_err(&spi->dev, "spi wr: buffer overflow - tx %Xh\n",
			msg->tx_size);
		return -EOVERFLOW;
	}

	siw_touch_spi_message_init(spi, &m);

	x.tx_buf = msg->tx_buf;
	x.rx_buf = msg->rx_buf;
	x.tx_dma = msg->tx_dma;
	x.rx_dma = msg->rx_dma;
	x.len = msg->tx_size;

	siw_touch_spi_message_add_tail(spi, &x, &m);

	ret = siw_touch_spi_sync(spi, &m);
	siwmon_submit_bus_spi_write(spi, msg, ret);
	if (ret < 0)
		siw_touch_spi_err_dump(spi, &x, 1, 0);

	return ret;
}

static int siw_touch_spi_write(struct device *dev, void *msg)
{
	return siw_touch_spi_do_write(to_spi_device(dev),
		(struct touch_bus_msg *)msg);
}

#if defined(CONFIG_TOUCHSCREEN_SIW_MMI_MON) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_MON_MODULE)
static void __siw_touch_spi_xfer_mon(struct spi_device *spi,
				struct touch_xfer_msg *xfer,
				int ret)
{
	struct touch_xfer_data_t *tx = NULL;
	struct touch_xfer_data_t *rx = NULL;
	struct touch_bus_msg *msg;
	struct touch_bus_msg msg_buf[SIW_TOUCH_MAX_XFER_COUNT];
	int idx = 0;
	int cnt = xfer->msg_count;
	int i;

	for (i = 0; i < cnt; i++) {
		tx = &xfer->data[i].tx;
		rx = &xfer->data[i].rx;

		msg = &msg_buf[idx++];
		msg->tx_buf = tx->data;
		msg->tx_size = tx->size;
		msg->rx_buf = rx->data;
		msg->rx_size = rx->size;
		msg->bits_per_word = spi->bits_per_word;
		msg->priv = (i<<8) | cnt;

		siwmon_submit_bus_spi_xfer(spi, msg, ret);
	}
}
#else	/* CONFIG_TOUCHSCREEN_SIW_MMI_MON */
static inline void __siw_touch_spi_xfer_mon(struct spi_device *spi,
				struct touch_xfer_msg *xfer,
				int ret){ }
#endif	/* CONFIG_TOUCHSCREEN_SIW_MMI_MON */

static int siw_touch_spi_do_xfer(struct spi_device *spi,
					struct touch_xfer_msg *xfer)
{
	struct device *dev = &spi->dev;
	struct touch_xfer_data_t *tx = NULL;
	struct touch_xfer_data_t *rx = NULL;
	struct spi_transfer _x[SIW_TOUCH_MAX_XFER_COUNT];
	struct spi_transfer *x;
	struct spi_message m;
	int cnt = xfer->msg_count;
	int i = 0;
	int ret = 0;

	if (!xfer) {
		t_dev_err(dev, "NULL xfer\n");
		return -EINVAL;
	}

	/*
	 * Bus control can need to be modifyed up to main chipset spec.
	 */

	if (cnt > SIW_TOUCH_MAX_XFER_COUNT) {
		t_dev_err(dev, "cout exceed, %d\n", cnt);
		return -EINVAL;
	}

	siw_touch_spi_message_init(spi, &m);

	memset(_x, 0, sizeof(_x));

	x = _x;
	for (i = 0; i < cnt; i++) {
		tx = &xfer->data[i].tx;
		rx = &xfer->data[i].rx;

		x->cs_change = !!(i < (xfer->msg_count - 1));
		x->bits_per_word = spi->bits_per_word;
		x->delay_usecs = 0;
		x->speed_hz = spi->max_speed_hz;

		if (rx->size) {
			x->tx_buf = tx->data;
			x->rx_buf = rx->data;
			x->len = rx->size;
		} else {
			x->tx_buf = tx->data;
			x->rx_buf = NULL;
			x->len = tx->size;
		}

		if (x->len > SIW_TOUCH_MAX_BUF_SIZE) {
			t_dev_err(&spi->dev,
				"spi xfer[%d, %d]: buffer overflow - %s %Xh\n",
				i, cnt,
				(rx->size) ? "rd" : "wr", x->len);
			break;
		}

		siw_touch_spi_message_add_tail(spi, x, &m);

		x++;
	}

	ret = siw_touch_spi_sync(spi, &m);
	__siw_touch_spi_xfer_mon(spi, xfer, ret);
	if (ret < 0)
		siw_touch_spi_err_dump(spi, _x, cnt, 0);

	return ret;
}

static int siw_touch_spi_xfer(struct device *dev, void *xfer)
{
	return siw_touch_spi_do_xfer(to_spi_device(dev),
		(struct touch_xfer_msg *)xfer);
}

static int siw_touch_spi_cfg(struct spi_device *spi,
					struct siw_touch_pdata *pdata)
{
	struct device *dev = &spi->dev;
	u32 tmp;

	spi->chip_select = 0;

	tmp = pdata_bits_per_word(pdata);
	if (tmp == ~0) {
		t_dev_err(dev,
			"spi alloc: wrong spi setup: bits_per_word, %d\n", tmp);
		return -EFAULT;
	}
	spi->bits_per_word = (u8)tmp;

	tmp = pdata_spi_mode(pdata);
	if (tmp == ~0) {
		t_dev_err(dev,
			"spi alloc: wrong spi setup: spi_mode, %d\n", tmp);
		return -EFAULT;
	}
	spi->mode = tmp;

	tmp = pdata_max_freq(pdata);
	if (spi_freq_out_of_range(tmp)) {
		t_dev_err(dev,
			"spi alloc: wrong spi setup: max_freq, %d\n", tmp);
		return -EFAULT;
	}
	spi->max_speed_hz = tmp;

	t_dev_info(dev, "spi alloc: %d.%d Mhz, mode %d, bpw %d\n",
			freq_to_mhz_unit(tmp),
			freq_to_khz_top(tmp),
			spi->mode, spi->bits_per_word);

	return 0;
}

static struct siw_ts *siw_touch_spi_alloc(
			struct spi_device *spi,
			struct siw_touch_bus_drv *bus_drv)
{
	struct device *dev = &spi->dev;
	struct siw_ts *ts = NULL;
	struct siw_touch_pdata *pdata = NULL;
	int ret = 0;

	ts = touch_kzalloc(dev, sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		t_dev_err(dev,
				"spi alloc: failed to allocate memory for touch data\n");
		goto out;
	}

	ts->bus_dev = spi;
	ts->addr = (size_t)spi;
	ts->dev = dev;
	ts->irq = spi->irq;
	pdata = bus_drv->pdata;
	if (!pdata) {
		t_dev_err(dev, "spi alloc: NULL pdata\n");
		goto out_pdata;
	}

	ts->pdata = pdata;

	ret = siw_setup_params(ts, pdata);
	if (ret < 0)
		goto out_name;

	siw_setup_operations(ts, bus_drv->pdata->ops);

	ts->bus_init = siw_touch_spi_init;
	ts->bus_read = siw_touch_spi_read;
	ts->bus_write = siw_touch_spi_write;
	ts->bus_xfer = siw_touch_spi_xfer;

	ret = siw_touch_bus_tr_data_init(ts);
	if (ret < 0)
		goto out_tr;

	ret = siw_touch_spi_cfg(spi, pdata);
	if (ret < 0)
		goto out_spi;

	spi_set_drvdata(spi, ts);

	return ts;

out_spi:
	siw_touch_bus_tr_data_free(ts);

out_tr:

out_name:

out_pdata:
	touch_kfree(dev, ts);

out:
	return NULL;
}

static void siw_touch_spi_free(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct siw_ts *ts = to_touch_core(dev);

	spi_set_drvdata(spi, NULL);

	siw_touch_bus_tr_data_free(ts);

	touch_kfree(dev, ts);
}

static int siw_touch_spi_probe(struct spi_device *spi)
{
	struct siw_touch_bus_drv *bus_drv = NULL;
	struct siw_ts *ts = NULL;
	struct device *dev = &spi->dev;
	int ret = 0;

	t_dev_info_bus_parent(dev);

	bus_drv = container_of(to_spi_driver(dev->driver),
					struct siw_touch_bus_drv, bus.spi_drv);
	if (bus_drv == NULL) {
		t_dev_err(dev, "NULL bus_drv\n");
		return -EINVAL;
	}

	ts = siw_touch_spi_alloc(spi, bus_drv);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	ret = siw_touch_probe(ts);
	if (ret)
		goto out_plat;

	return 0;

out_plat:

out:
	return ret;
}

static int siw_touch_spi_remove(struct spi_device *spi)
{
	struct siw_ts *ts = to_touch_core(&spi->dev);

	siw_touch_remove(ts);

	siw_touch_spi_free(spi);

	return 0;
}

#if defined(CONFIG_PM_SLEEP)
static int siw_touch_spi_pm_suspend(struct device *dev)
{
	int ret = 0;

	ret = siw_touch_bus_pm_suspend(dev, 0);

	return ret;
}

static int siw_touch_spi_pm_resume(struct device *dev)
{
	int ret = 0;

	ret = siw_touch_bus_pm_resume(dev, 0);

	return ret;
}

#if defined(__SIW_CONFIG_FASTBOOT)
static int siw_touch_spi_pm_freeze(struct device *dev)
{
	int ret = 0;

	ret = siw_touch_bus_pm_suspend(dev, 1);

	return ret;
}

static int siw_touch_spi_pm_thaw(struct device *dev)
{
	int ret = 0;

	ret = siw_touch_bus_pm_resume(dev, 1);

	return ret;
}
#endif

static const struct dev_pm_ops siw_touch_spi_pm_ops = {
	.suspend	= siw_touch_spi_pm_suspend,
	.resume		= siw_touch_spi_pm_resume,
#if defined(__SIW_CONFIG_FASTBOOT)
	.freeze		= siw_touch_spi_pm_freeze,
	.thaw		= siw_touch_spi_pm_thaw,
	.poweroff	= siw_touch_spi_pm_freeze,
	.restore	= siw_touch_spi_pm_thaw,
#endif
};
#define DEV_PM_OPS	(&siw_touch_spi_pm_ops)
#else	/* CONFIG_PM_SLEEP */
#define DEV_PM_OPS	NULL
#endif	/* CONFIG_PM_SLEEP */

static struct spi_device_id siw_touch_spi_id[] = {
	{ SIW_TOUCH_NAME, 0 },
	{ }
};

int siw_touch_spi_add_driver(void *data)
{
	struct siw_touch_chip_data *chip_data = data;
	struct siw_touch_bus_drv *bus_drv = NULL;
	struct siw_touch_pdata *pdata = NULL;
	struct spi_driver *spi_drv = NULL;
	char *drv_name = NULL;
	int bus_type;
	int ret = 0;

	if (chip_data == NULL) {
		t_pr_err("NULL chip data\n");
		return -EINVAL;
	}

	if (chip_data->pdata == NULL) {
		t_pr_err("NULL chip pdata\n");
		return -EINVAL;
	}

	bus_type = pdata_bus_type((struct siw_touch_pdata *)chip_data->pdata);

	bus_drv = siw_touch_bus_create_bus_drv(bus_type);
	if (!bus_drv) {
		ret = -ENOMEM;
		goto out_drv;
	}

	pdata = siw_touch_bus_create_bus_pdata(bus_type);
	if (!pdata) {
		ret = -ENOMEM;
		goto out_pdata;
	}

	memcpy(pdata, chip_data->pdata, sizeof(*pdata));

	drv_name = pdata_drv_name(pdata);

	bus_drv->pdata = pdata;

	spi_drv = &bus_drv->bus.spi_drv;
	spi_drv->driver.name = drv_name;
	spi_drv->driver.owner = pdata->owner;
#if defined(__SIW_CONFIG_OF)
	spi_drv->driver.of_match_table = pdata->of_match_table;
#endif
	spi_drv->driver.pm = DEV_PM_OPS;

	spi_drv->probe = siw_touch_spi_probe;
	spi_drv->remove = siw_touch_spi_remove;
	spi_drv->id_table = siw_touch_spi_id;
	if (drv_name) {
		memset((void *)siw_touch_spi_id[0].name, 0, SPI_NAME_SIZE);
		snprintf((char *)siw_touch_spi_id[0].name,
				SPI_NAME_SIZE,
				"%s",
				drv_name);
	}

	ret = spi_register_driver(spi_drv);
	if (ret) {
		t_pr_err("spi_register_driver[%s] failed, %d\n",
				drv_name, ret);
		goto out_client;
	}

	chip_data->bus_drv = bus_drv;

	return 0;

out_client:
	siw_touch_bus_free_bus_pdata(pdata);

out_pdata:
	siw_touch_bus_free_bus_drv(bus_drv);

out_drv:

	return ret;
}

int siw_touch_spi_del_driver(void *data)
{
	struct siw_touch_chip_data *chip_data = data;
	struct siw_touch_bus_drv *bus_drv = NULL;

	if (chip_data == NULL) {
		t_pr_err("NULL touch chip data\n");
		return -ENODEV;
	}

	bus_drv = chip_data->bus_drv;
	if (bus_drv) {
		spi_unregister_driver(&bus_drv->bus.spi_drv);

		if (bus_drv->pdata)
			siw_touch_bus_free_bus_pdata(bus_drv->pdata);

		siw_touch_bus_free_bus_drv(bus_drv);
		chip_data->bus_drv = NULL;
	}

	return 0;
}

#else	/* CONFIG_SPI_MASTER */
int siw_touch_spi_add_driver(void *data)
{
	t_pr_err("SPI : not supported in this system\n");
	return -ENODEV;
}

int siw_touch_spi_del_driver(void *data)
{
	t_pr_err("SPI : not supported in this system\n");
	return -ENODEV;
}

#endif	/* CONFIG_SPI_MASTER */

