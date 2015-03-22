/* < DTS2013050605374 shenjinming 20130508 begin */
/* < DTS2013062605264 sunlibin 20130702 begin */
/* add cypress new driver ttda-02.03.01.476713 */

/*
 * cyttsp4_spi.c
 * Cypress TrueTouch(TM) Standard Product V4 SPI Driver module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * Author: Aleksej Makarov <aleksej.makarov@sonyericsson.com>
 * Modified by: Cypress Semiconductor for test with device
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/hrtimer.h>
#include <linux/cyttsp4_bus.h>
#include <linux/cyttsp4_core.h>
#include "cyttsp4_spi.h"

#include "cyttsp4_devtree.h"

#define CY_SPI_WR_OP		0x00 /* r/~w */
#define CY_SPI_RD_OP		0x01
#define CY_SPI_A8_BIT		0x02
#define CY_SPI_WR_HEADER_BYTES	2
#define CY_SPI_RD_HEADER_BYTES	1
#define CY_SPI_SYNC_BYTE	0
#define CY_SPI_SYNC_ACK		0x62 /* from TRM *A protocol */
#define CY_SPI_DATA_SIZE	(3 * 256)
#define CY_SPI_BITS_PER_WORD	8
#define CY_SPI_NUM_RETRY	3

#define CY_SPI_MAX_HEADER_BYTES	\
		max(CY_SPI_WR_HEADER_BYTES, CY_SPI_RD_HEADER_BYTES)

struct cyttsp4_spi {
	struct spi_device *client;
	char const *id;
	struct mutex lock;
};

static void _cyttsp4_spi_pr_buf(struct cyttsp4_spi *ts_spi, u8 *buf,
			int size, char const *info)
{
#ifdef VERBOSE_DEBUG
	static char b[CY_SPI_DATA_SIZE * 3 + 1];
	unsigned i, k;

	for (i = k = 0; i < size; i++, k += 3)
		snprintf(b + k, sizeof(b) - k, "%02x ", buf[i]);
	dev_dbg(&ts_spi->client->dev, "%s: %s\n", info, b);
#endif
}

static int cyttsp4_spi_xfer(u8 op, struct cyttsp4_spi *ts,
			u16 reg, u8 *buf, int length)
{
	struct device *dev = &ts->client->dev;
	struct spi_message msg;
	struct spi_transfer xfer[2];
	u8 wr_hdr_buf[CY_SPI_MAX_HEADER_BYTES];
	u8 rd_hdr_buf[CY_SPI_MAX_HEADER_BYTES];
	int rc;

	memset(wr_hdr_buf, 0, CY_SPI_MAX_HEADER_BYTES);
	memset(rd_hdr_buf, 0, CY_SPI_MAX_HEADER_BYTES);
	memset(xfer, 0, sizeof(xfer));

	spi_message_init(&msg);

	/* Header buffer */
	xfer[0].tx_buf = wr_hdr_buf;
	xfer[0].rx_buf = rd_hdr_buf;

	switch (op) {
	case CY_SPI_WR_OP:
		if (length + CY_SPI_WR_HEADER_BYTES > CY_SPI_DATA_SIZE) {
			dev_vdbg(dev,
				"%s: length+%d=%d is greater than SPI max=%d\n",
				__func__, CY_SPI_WR_HEADER_BYTES,
				length + CY_SPI_WR_HEADER_BYTES,
				CY_SPI_DATA_SIZE);
			rc = -EINVAL;
			goto cyttsp4_spi_xfer_exit;
		}

		/* Header byte 0 */
		if (reg > 255)
			wr_hdr_buf[0] = CY_SPI_WR_OP + CY_SPI_A8_BIT;
		else
			wr_hdr_buf[0] = CY_SPI_WR_OP;

		/* Header byte 1 */
		wr_hdr_buf[1] = reg % 256;

		xfer[0].len = CY_SPI_WR_HEADER_BYTES;

		spi_message_add_tail(&xfer[0], &msg);

		/* Data buffer */
		if (buf) {
			xfer[1].tx_buf = buf;
			xfer[1].len = length;

			spi_message_add_tail(&xfer[1], &msg);
		}
		break;

	case CY_SPI_RD_OP:
		if (!buf) {
			dev_err(dev, "%s: No read buffer\n", __func__);
			rc = -EINVAL;
			goto cyttsp4_spi_xfer_exit;
		}

		if ((length + CY_SPI_RD_HEADER_BYTES) > CY_SPI_DATA_SIZE) {
			dev_vdbg(dev,
				"%s: length+%d=%d is greater than SPI max=%d\n",
				__func__, CY_SPI_RD_HEADER_BYTES,
				length + CY_SPI_RD_HEADER_BYTES,
				CY_SPI_DATA_SIZE);
			rc = -EINVAL;
			goto cyttsp4_spi_xfer_exit;
		}

		/* Header byte 0 */
		wr_hdr_buf[0] = CY_SPI_RD_OP;

		xfer[0].len = CY_SPI_RD_HEADER_BYTES;

		spi_message_add_tail(&xfer[0], &msg);

		/* Data buffer */
		xfer[1].rx_buf = buf;
		xfer[1].len = length;

		spi_message_add_tail(&xfer[1], &msg);
		break;

	default:
		dev_dbg(dev, "%s: bad op code=%d\n", __func__, op);
		rc = -EINVAL;
		goto cyttsp4_spi_xfer_exit;
	}

	rc = spi_sync(ts->client, &msg);
	if (rc < 0) {
		dev_vdbg(dev, "%s: spi_sync() error %d, len=%d, op=%d\n",
			__func__, rc, xfer[0].len, op);
		/*
		 * do not return here since probably a bad ACK sequence
		 * let the following ACK check handle any errors and
		 * allow silent retries
		 */
	}

	if (rd_hdr_buf[CY_SPI_SYNC_BYTE] != CY_SPI_SYNC_ACK) {
		/* signal ACK error so silent retry */
		rc = 1;

		switch (op) {
		case CY_SPI_WR_OP:
			_cyttsp4_spi_pr_buf(ts, wr_hdr_buf,
				CY_SPI_WR_HEADER_BYTES,
				"spi_wr_buf HEAD");
			if (buf)
				_cyttsp4_spi_pr_buf(ts, buf,
					length, "spi_wr_buf DATA");
			break;

		case CY_SPI_RD_OP:
			_cyttsp4_spi_pr_buf(ts, rd_hdr_buf,
				CY_SPI_RD_HEADER_BYTES, "spi_rd_buf HEAD");
			_cyttsp4_spi_pr_buf(ts, buf, length,
				"spi_rd_buf DATA");
			break;

		default:
			/*
			 * should not get here due to error check
			 * in first switch
			 */
			break;
		}
	}

cyttsp4_spi_xfer_exit:
	return rc;
}

static s32 cyttsp4_spi_read_block_data(struct cyttsp4_spi *ts, u16 addr,
				int length, void *data, int max_xfer)
{
	int rc = -EINVAL;
	int retry = 0;
	int trans_len;
	struct device *dev = &ts->client->dev;

	dev_vdbg(dev, "%s: Enter\n", __func__);

	while (length > 0) {
		trans_len = min(length, max_xfer);

		/* Write address */
		rc = cyttsp4_spi_xfer(CY_SPI_WR_OP, ts, addr, NULL, 0);
		if (rc < 0) {
			dev_err(dev, "%s: Fail write address r=%d\n",
				__func__, rc);
			return rc;
		}

		/* Read data */
		rc = cyttsp4_spi_xfer(CY_SPI_RD_OP, ts, addr, data, trans_len);
		if (rc < 0) {
			dev_err(dev, "%s: Fail read r=%d\n", __func__, rc);
			goto exit;
		} else if (rc > 0) {
			/* Perform retry or fail */
			if (retry++ < CY_SPI_NUM_RETRY) {
				dev_dbg(dev, "%s: ACK error, retry %d\n",
					__func__, retry);
				continue;
			} else {
				dev_err(dev, "%s: ACK error\n", __func__);
				rc = -EIO;
				goto exit;
			}
		}

		length -= trans_len;
		data += trans_len;
		addr += trans_len;
	}
exit:
	return rc;
}

static s32 cyttsp4_spi_write_block_data(struct cyttsp4_spi *ts, u16 addr,
				int length, const void *data, int max_xfer)
{
	int rc = -EINVAL;
	int retry = 0;
	int trans_len;
	struct device *dev = &ts->client->dev;

	dev_vdbg(dev, "%s: Enter\n", __func__);

	while (length > 0) {
		trans_len = min(length, max_xfer);

		rc = cyttsp4_spi_xfer(CY_SPI_WR_OP, ts, addr, (void *)data,
				trans_len);
		if (rc < 0) {
			dev_err(dev, "%s: Fail write r=%d\n", __func__, rc);
			goto exit;
		} else if (rc > 0) {
			/* Perform retry or fail */
			if (retry++ < CY_SPI_NUM_RETRY) {
				dev_dbg(dev, "%s: ACK error, retry %d\n",
					__func__, retry);
				continue;
			} else {
				dev_err(dev, "%s: ACK error\n", __func__);
				rc = -EIO;
				goto exit;
			}
		}

		length -= trans_len;
		data += trans_len;
		addr += trans_len;
	}
exit:
	return rc;
}

static int cyttsp4_spi_write(struct cyttsp4_adapter *adap, u16 addr,
		const void *buf, int size, int max_xfer)
{
	struct cyttsp4_spi *ts = dev_get_drvdata(adap->dev);
	int rc;

	pm_runtime_get_noresume(adap->dev);
	mutex_lock(&ts->lock);
	rc = cyttsp4_spi_write_block_data(ts, addr, size, buf, max_xfer);
	mutex_unlock(&ts->lock);
	pm_runtime_put_noidle(adap->dev);

	return rc;
}

static int cyttsp4_spi_read(struct cyttsp4_adapter *adap, u16 addr,
		void *buf, int size, int max_xfer)
{
	struct cyttsp4_spi *ts = dev_get_drvdata(adap->dev);
	int rc;

	pm_runtime_get_noresume(adap->dev);
	mutex_lock(&ts->lock);
	rc = cyttsp4_spi_read_block_data(ts, addr, size, buf, max_xfer);
	mutex_unlock(&ts->lock);
	pm_runtime_put_noidle(adap->dev);

	return rc;
}

static struct cyttsp4_ops ops = {
	.write = cyttsp4_spi_write,
	.read = cyttsp4_spi_read,
};

static struct of_device_id cyttsp4_spi_of_match[] = {
	{ .compatible = "cy,cyttsp4_spi_adapter", }, { }
};
MODULE_DEVICE_TABLE(of, cyttsp4_spi_of_match);

static int __devinit cyttsp4_spi_probe(struct spi_device *spi)
{
	struct cyttsp4_spi *ts_spi;
	int rc = 0;
	struct device *dev = &spi->dev;
	const struct of_device_id *match;
	char const *adap_id;

	dev_dbg(dev, "%s: Probing ...\n", __func__);

	spi->bits_per_word = CY_SPI_BITS_PER_WORD;
	spi->mode = SPI_MODE_0;

	rc = spi_setup(spi);
	if (rc < 0) {
		dev_err(dev, "%s: SPI setup error %d\n", __func__, rc);
		return rc;
	}

	ts_spi = kzalloc(sizeof(*ts_spi), GFP_KERNEL);
	if (ts_spi == NULL) {
		dev_err(dev, "%s: Error, kzalloc\n", __func__);
		rc = -ENOMEM;
		goto error_alloc_data_failed;
	}

	match = of_match_device(of_match_ptr(cyttsp4_spi_of_match), dev);
	if (match) {
		rc = of_property_read_string(dev->of_node, "cy,adapter_id",
				&adap_id);
		if (rc) {
			dev_err(dev, "%s: OF error rc=%d\n", __func__, rc);
			goto error_free_data;
		}
		cyttsp4_devtree_register_devices(dev);
	} else {
		adap_id = dev_get_platdata(dev);
	}

	mutex_init(&ts_spi->lock);
	ts_spi->client = spi;
	ts_spi->id = (adap_id) ? adap_id : CYTTSP4_SPI_NAME;
	dev_set_drvdata(&spi->dev, ts_spi);

	dev_dbg(dev, "%s: add adap='%s' (CYTTSP4_SPI_NAME=%s)\n", __func__,
		ts_spi->id, CYTTSP4_SPI_NAME);

	pm_runtime_enable(&spi->dev);

	rc = cyttsp4_add_adapter(ts_spi->id, &ops, dev);
	if (rc) {
		dev_err(dev, "%s: Error on probe %s\n", __func__,
			CYTTSP4_SPI_NAME);
		goto add_adapter_err;
	}

	dev_info(dev, "%s: Successful prob %s\n", __func__, CYTTSP4_SPI_NAME);

	return 0;

add_adapter_err:
	pm_runtime_disable(&spi->dev);
	dev_set_drvdata(&spi->dev, NULL);
error_free_data:
	kfree(ts_spi);
error_alloc_data_failed:
	return rc;
}

static int __devexit cyttsp4_spi_remove(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct cyttsp4_spi *ts_spi = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);
	cyttsp4_del_adapter(ts_spi->id);
	pm_runtime_disable(&spi->dev);
	dev_set_drvdata(&spi->dev, NULL);
	kfree(ts_spi);
	return 0;
}

static const struct spi_device_id cyttsp4_spi_id[] = {
	{ CYTTSP4_SPI_NAME, 0 },  { }
};
MODULE_DEVICE_TABLE(spi, cyttsp4_spi_id);

static struct spi_driver cyttsp4_spi_driver = {
	.driver = {
		.name = CYTTSP4_SPI_NAME,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = cyttsp4_spi_of_match,
	},
	.probe = cyttsp4_spi_probe,
	.remove = __devexit_p(cyttsp4_spi_remove),
	.id_table = cyttsp4_spi_id,
};

static int __init cyttsp4_spi_init(void)
{
	int err;

	err = spi_register_driver(&cyttsp4_spi_driver);
	pr_info("%s: Cypress TTSP SPI Touchscreen Driver (Built %s) rc=%d\n",
		 __func__, CY_DRIVER_DATE, err);

	return err;
}
module_init(cyttsp4_spi_init);

static void __exit cyttsp4_spi_exit(void)
{
	spi_unregister_driver(&cyttsp4_spi_driver);
	pr_info("%s: module exit\n", __func__);
}
module_exit(cyttsp4_spi_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product SPI driver");
MODULE_AUTHOR("Aleksej Makarov <aleksej.makarov@sonyericsson.com>");
/* DTS2013062605264 sunlibin 20130702 end > */
/* DTS2013050605374 shenjinming 20130508 end > */
