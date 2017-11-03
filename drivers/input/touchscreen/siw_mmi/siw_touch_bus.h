/*
 * SiW touch bus core driver
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#ifndef __SIW_TOUCH_BUS_H
#define __SIW_TOUCH_BUS_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>


enum {
	I2C_BUS_TX_HDR_SZ = 2,
	I2C_BUS_RX_HDR_SZ = 0,
	I2C_BUS_TX_DUMMY_SZ = 0,
	I2C_BUS_RX_DUMMY_SZ = 0,
};

enum {
	SPI_BUS_TX_HDR_SZ = 2,
	SPI_BUS_RX_HDR_SZ = 4,
	SPI_BUS_TX_DUMMY_SZ = 0,
	SPI_BUS_RX_DUMMY_SZ = 2,
};

/* __SIW_SPI_TYPE_1 */
enum {
	SPI_BUS_RX_HDR_SZ_32BIT = (4+2),
	SPI_BUS_RX_DUMMY_SZ_32BIT = 4,
	/* */
	SPI_BUS_RX_HDR_SZ_128BIT = (16+2),
	SPI_BUS_RX_DUMMY_SZ_128BIT = 16,
};

enum {
	SPI_BUS_RX_DUMMY_FLAG_128BIT = 0x10,
};

#define __CLOCK_1KHZ		1000
#define __CLOCK_100KHZ		(100 * __CLOCK_1KHZ)
#define __CLOCK_1MHZ		(__CLOCK_1KHZ * __CLOCK_1KHZ)

#define SPI_MAX_FREQ		(20 * __CLOCK_1MHZ)
#define SPI_MIN_FREQ		(1 * __CLOCK_1MHZ)

static inline int spi_freq_out_of_range(u32 freq)
{
	return ((freq > SPI_MAX_FREQ) || (freq < SPI_MIN_FREQ));
}

static inline u32 freq_to_mhz_unit(u32 freq)
{
	return (freq / __CLOCK_1MHZ);
}

static inline u32 freq_to_khz_unit(u32 freq)
{
	return (freq / __CLOCK_1KHZ);
}

static inline u32 freq_to_khz_top(u32 freq)
{
	return ((freq % __CLOCK_1MHZ) / __CLOCK_100KHZ);
}

/* __SIW_SPI_TYPE_1 */

struct touch_bus_msg {
	u8 *tx_buf;
	int tx_size;
	u8 *rx_buf;
	int rx_size;
	dma_addr_t tx_dma;
	dma_addr_t rx_dma;
	u8 bits_per_word;
	int priv;
};

struct touch_xfer_data_t {
	u16 addr;
	u16 size;
	u8 *buf;
	u8 data[SIW_TOUCH_MAX_BUF_SIZE];
};

struct touch_xfer_data {
	struct touch_xfer_data_t tx;
	struct touch_xfer_data_t rx;
};

struct touch_xfer_msg {
	struct touch_xfer_data data[SIW_TOUCH_MAX_XFER_COUNT];
	u8 bits_per_word;
	u8 msg_count;
};

struct siw_touch_bus_drv {
	union {
		struct i2c_driver i2c_drv;
		struct spi_driver spi_drv;
	} bus;
	struct siw_touch_pdata *pdata;
};


#define t_dev_info_bus_parent(_dev)	\
	t_dev_info(_dev, "dev bus probe : %s/%s/%s\n",	\
			dev_name(_dev->parent->parent),		\
			dev_name(_dev->parent),		\
			dev_name(_dev))


extern int siw_touch_bus_tr_data_init(struct siw_ts *ts);
extern void siw_touch_bus_tr_data_free(struct siw_ts *ts);

extern int siw_touch_bus_pin_get(struct siw_ts *ts);
extern int siw_touch_bus_pin_put(struct siw_ts *ts);

extern void *siw_touch_bus_create_bus_drv(int bus_type);
extern void siw_touch_bus_free_bus_drv(void *bus_drv);

extern void *siw_touch_bus_create_bus_pdata(int bus_type);
extern void siw_touch_bus_free_bus_pdata(void *pdata);

extern int siw_touch_bus_alloc_buffer(struct siw_ts *ts);
extern int siw_touch_bus_free_buffer(struct siw_ts *ts);

extern int siw_touch_bus_init(struct device *dev);
extern int siw_touch_bus_read(struct device *dev, struct touch_bus_msg *msg);
extern int siw_touch_bus_write(struct device *dev, struct touch_bus_msg *msg);
extern int siw_touch_bus_xfer(struct device *dev, struct touch_xfer_msg *xfer);

extern void siw_touch_bus_err_dump_data(struct device *dev,
							u8 *buf, int len,
							int idx, char *name);

extern int siw_touch_bus_add_driver(struct siw_touch_chip_data *chip_data);
extern int siw_touch_bus_del_driver(struct siw_touch_chip_data *chip_data);

extern int siw_touch_bus_pm_suspend(struct device *dev, int freeze);
extern int siw_touch_bus_pm_resume(struct device *dev, int thaw);


#endif	/* __SIW_TOUCH_BUS_H */

