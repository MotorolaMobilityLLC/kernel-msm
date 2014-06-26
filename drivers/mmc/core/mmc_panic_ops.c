/*
 * linux/drivers/mmc/core/mmc_panic_ops.c
 *
 * Copyright (C) 2011 Intel Corp
 * Author: dongxing.zhang@intel.com
 * Author: jun.zhang@intel.com
 * Author: chuansheng.liu@intel.com
 * Author: chuanxiao.dong@intel.com
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>

#include "core.h"
#include "bus.h"
#include "host.h"

#include "mmc_ops.h"


static struct mmc_panic_host *panic_host;

static int mmc_emergency_prepare(void)
{
	struct mmc_host *mmc = panic_host->mmc;

	if (mmc == NULL) {
		pr_err("%s: panic host was not setup\n", __func__);
		return -ENODEV;
	}

	/*
	 * once panic happened, we monopolize the host controller.
	 * so claim host without relase any more.
	 */
	mmc->claimed = 1;
	mmc->claimer = current;
	mmc->claim_cnt += 1;
#ifdef CONFIG_MMC_CLKGATE
	/*
	 * disable the clock gating
	 */
	mmc->clk_gated = false;
	mmc->clk_requests++;
	mmc->ios.clock = mmc->clk_old;
#endif
	return 0;
}

static void mmc_emergency_ready(void)
{
	panic_host->panic_ready = 1;
}

/*
 * Return the card size in sectors.
 *
 * return value:
 * the sector number
 */
static unsigned int mmc_get_capacity(struct mmc_card *card)
{
	if (!mmc_card_sd(card) && mmc_card_blockaddr(card))
		return card->ext_csd.sectors;
	else
		return card->csd.capacity << (card->csd.read_blkbits - 9);
}

static void mmc_emergency_send_req(struct mmc_request *mrq)
{
	struct mmc_panic_host *host = panic_host;

	mrq->cmd->error = 0;
	mrq->cmd->mrq = mrq;

	if (mrq->data) {
		BUG_ON(mrq->data->blksz > host->max_blk_size);
		BUG_ON(mrq->data->blocks > host->max_blk_count);
		BUG_ON(mrq->data->blocks * mrq->data->blksz >
				host->max_req_size);

		mrq->cmd->data = mrq->data;
		mrq->data->error = 0;
		mrq->data->mrq = mrq;
		if (mrq->stop) {
			mrq->data->stop = mrq->stop;
			mrq->stop->error = 0;
			mrq->stop->mrq = mrq;
		}
	}

	/*
	 * Send the request to host
	 *
	 * if request handling is successful, return.
	 * If request handling is failed and has rety, resend request.
	 * During retry, if request handling is still failed, core layer
	 * will keep on retry untill cmd->retries is 0.
	 *
	 * So in this way, makes retry blind to host driver. Totally
	 * controlled by core driver
	 */
	host->panic_ops->request(host, mrq);

	while ((mrq->cmd->error || (mrq->data && (mrq->data->error ||
			(mrq->data->stop && mrq->data->stop->error)))) &&
			mrq->cmd->retries > 0) {
		/* clear errors */
		mrq->cmd->error = 0;
		if (mrq->data) {
			mrq->data->error = 0;
			if (mrq->stop)
				mrq->stop->error = 0;
		}
		host->panic_ops->request(host, mrq);
		mrq->cmd->retries--;
	}
}

static int mmc_emergency_send_cmd(struct mmc_command *cmd, int retries)
{
	struct mmc_request mrq;

	memset(&mrq, 0, sizeof(struct mmc_request));

	memset(cmd->resp, 0, sizeof(cmd->resp));
	cmd->retries = retries;

	mrq.cmd = cmd;
	cmd->data = NULL;

	mmc_emergency_send_req(&mrq);

	return cmd->error;
}

static int __mmc_emergency_write(unsigned int blk_id)
{
	struct mmc_request mrq;
	struct mmc_command cmd;
	struct mmc_data data;

	memset(&mrq, 0, sizeof(struct mmc_request));
	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&data, 0, sizeof(struct mmc_data));

	mrq.cmd = &cmd;
	mrq.data = &data;
	cmd.opcode = MMC_WRITE_BLOCK;
	cmd.arg = blk_id;
	if (!panic_host->blkaddr)
		cmd.arg <<= 9;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
	/*
	 * Fix these values;
	 */
	data.blksz = 512;
	data.blocks = 1;
	data.dmabuf = panic_host->dmabuf;

	mmc_emergency_send_req(&mrq);

	return cmd.error;
}


static int mmc_emergency_go_idle(struct mmc_panic_host *host)
{
	int err;
	struct mmc_command cmd;

	/*
	 * Non-SPI hosts need to prevent chipselect going active during
	 * GO_IDLE; that would put chips into SPI mode.  Remind them of
	 * that in case of hardware that won't pull up DAT3/nCS otherwise.
	 *
	 * SPI hosts ignore ios.chip_select; it's managed according to
	 * rules that must accomodate non-MMC slaves which this layer
	 * won't even know about.
	 */
	if (!mmc_host_is_spi(host)) {
		host->ios.chip_select = MMC_CS_HIGH;
		host->panic_ops->set_ios(host);
		mdelay(1);
	}

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_GO_IDLE_STATE;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_NONE | MMC_CMD_BC;

	err = mmc_emergency_send_cmd(&cmd, 0);

	mdelay(1);

	if (!mmc_host_is_spi(host)) {
		host->ios.chip_select = MMC_CS_DONTCARE;
		host->panic_ops->set_ios(host);
		mdelay(1);
	}

	return err;
}
static int mmc_emergency_send_op_cond(struct mmc_panic_host *host,
		u32 ocr, u32 *rocr)
{
	struct mmc_command cmd;
	int i, err = 0;

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_SEND_OP_COND;
	cmd.arg = mmc_host_is_spi(host) ? 0 : ocr;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R3 | MMC_CMD_BCR;

	for (i = 100; i; i--) {
		err = mmc_emergency_send_cmd(&cmd, 0);
		if (err)
			break;

		/* if we're just probing, do a single pass */
		if (ocr == 0)
			break;

		/* otherwise wait until reset completes */
		if (mmc_host_is_spi(host)) {
			if (!(cmd.resp[0] & R1_SPI_IDLE))
				break;
		} else {
			if (cmd.resp[0] & MMC_CARD_BUSY)
				break;
		}

		err = -ETIMEDOUT;

		/*
		 * If command 1 is failed, wait 10ms and then
		 * have a retry. Card may need time to prepare
		 * for the next command 1
		 */
		mdelay(10);
	}

	if (rocr && !mmc_host_is_spi(host))
		*rocr = cmd.resp[0];

	return err;
}

static int mmc_emergency_all_send_cid(u32 *cid)
{
	int err;
	struct mmc_command cmd;

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_ALL_SEND_CID;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R2 | MMC_CMD_BCR;

	err = mmc_emergency_send_cmd(&cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	memcpy(cid, cmd.resp, sizeof(u32) * 4);

	return 0;
}

static int mmc_emergency_set_relative_addr(struct mmc_card *card)
{
	int err;
	struct mmc_command cmd;

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_SET_RELATIVE_ADDR;
	cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;

	err = mmc_emergency_send_cmd(&cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	return 0;
}

static int mmc_emergency_select_card(struct mmc_card *card)
{
	int err;
	struct mmc_command cmd;

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_SELECT_CARD;

	if (card) {
		cmd.arg = card->rca << 16;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
	} else {
		cmd.arg = 0;
		cmd.flags = MMC_RSP_NONE | MMC_CMD_AC;
	}

	err = mmc_emergency_send_cmd(&cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	return 0;
}

static int mmc_emergency_send_status(struct mmc_panic_host *host, u32 *status)
{
	struct mmc_card *card = host->card;
	int err;
	struct mmc_command cmd;

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_SEND_STATUS;
	if (!mmc_host_is_spi(host))
		cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;

	err = mmc_emergency_send_cmd(&cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	/* NOTE: callers are required to understand the difference
	 * between "native" and SPI format status words!
	 */
	if (status)
		*status = cmd.resp[0];

	return 0;
}
static int mmc_emergency_switch(struct mmc_panic_host *host,
		u8 set, u8 index, u8 value, u8 check_busy)
{
	struct mmc_card *card = host->card;
	int err;
	struct mmc_command cmd;
	u32 status;

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_SWITCH;
	cmd.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
		(index << 16) |
		(value << 8) |
		set;
	if (check_busy)
		cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
	else
		cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;

	err = mmc_emergency_send_cmd(&cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	/* Must check status to be sure of no errors */
	do {
		err = mmc_emergency_send_status(host, &status);
		if (err)
			return err;
		if (host->caps & MMC_CAP_WAIT_WHILE_BUSY)
			break;
		if (mmc_host_is_spi(host))
			break;
	} while (R1_CURRENT_STATE(status) == 7);

	if (mmc_host_is_spi(host)) {
		if (status & R1_SPI_ILLEGAL_COMMAND)
			return -EBADMSG;
	} else {
		if (status & 0xFDFFA000)
			pr_warn("%s: unexpected status %#x after switch",
				mmc_hostname(card->host), status);
		if (status & R1_SWITCH_ERROR)
			return -EBADMSG;
	}

	return 0;
}

static int mmc_emergency_cache_disable(struct mmc_panic_host *host)
{
	struct mmc_card *card = host->card;
	int err = 0;

	if (!(host->caps2 & MMC_CAP2_CACHE_CTRL))
		return err;

	if (card && mmc_card_mmc(card) && card->ext_csd.cache_ctrl &&
			(card->ext_csd.cache_size > 0)) {
		/*
		 * disable cache will cause flushing data to
		 * non-volatile storage, so we may need to
		 * check busy state here by polling card status
		 */
		err = mmc_emergency_switch(host,
				EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_CACHE_CTRL, 0,
				0);

		if (err)
			pr_err("%s: disable cache error %d in panic mode\n",
					mmc_hostname(card->host), err);
		else
			card->ext_csd.cache_ctrl = 0;
	}

	return err;
}

static int mmc_emergency_spi_set_crc(struct mmc_panic_host *host, int use)
{
	return -1;
}

static int mmc_emergency_send_cid(struct mmc_panic_host *host, u32 *cid)
{
	return -1;
}
/*
 * reinit card:
 * should also consider about the SPI host
 */
static int mmc_emergency_reinit_card(void)
{
	struct mmc_panic_host *host = panic_host;
	struct mmc_card *card = host->card;
	u32 ocr = host->ocr;
	int err, ddr = 0;
	u32 cid[4];
	unsigned int max_dtr;

	if (mmc_card_sd(card))
		return 0;

	/*
	 * before re-init card, flush cache first
	 * if there is.
	 * flush may be failed. So just ignore the failure
	 * here
	 */
	mmc_emergency_cache_disable(host);
	/*
	 * low the clock to be init clock
	 */
	if (mmc_host_is_spi(host)) {
		host->ios.chip_select = MMC_CS_HIGH;
		host->ios.bus_mode = MMC_BUSMODE_PUSHPULL;
	} else {
		host->ios.chip_select = MMC_CS_DONTCARE;
		host->ios.bus_mode = MMC_BUSMODE_OPENDRAIN;
	}
	host->ios.bus_width = MMC_BUS_WIDTH_1;
	host->ios.timing = MMC_TIMING_LEGACY;
	/*
	 * AS eMMC spec said, card init frequency cannot higher
	 * then 400Khz. But a good card should support for 400Khz
	 * frequence in initialize process.
	 */
	host->ios.clock = 400000;
	host->panic_ops->set_ios(host);

	/*
	 * Since we're changing the OCR value, we seem to
	 * need to tell some cards to go back to the idle
	 * state.  We wait 1ms to give cards time to
	 * respond.
	 */
	mmc_emergency_go_idle(host);

	/* The extra bit indicates that we support high capacity */
	err = mmc_emergency_send_op_cond(host, ocr | (1 << 30), NULL);
	if (err)
		goto err;

	/*
	 * For SPI, enable CRC as appropriate.
	 */
	if (mmc_host_is_spi(host)) {
		err = mmc_emergency_spi_set_crc(host, 1);
		if (err)
			goto err;
	}

	/*
	 * Fetch CID from card.
	 */
	if (mmc_host_is_spi(host))
		err = mmc_emergency_send_cid(host, cid);
	else
		err = mmc_emergency_all_send_cid(cid);
	if (err)
		goto err;

	if (memcmp(cid, card->raw_cid, sizeof(cid)) != 0) {
		err = -ENOENT;
		goto err;
	}

	/*
	 * For native busses:  set card RCA and quit open drain mode.
	 */
	if (!mmc_host_is_spi(host)) {
		err = mmc_emergency_set_relative_addr(card);
		if (err)
			goto err;

		host->ios.bus_mode = MMC_BUSMODE_PUSHPULL;
		host->panic_ops->set_ios(host);
	}
	/*
	 * Select card, as all following commands rely on that.
	 */
	if (!mmc_host_is_spi(host)) {
		err = mmc_emergency_select_card(card);
		if (err)
			goto err;
	}

	/*
	 * Activate high speed (if supported)
	 */
	if ((card->ext_csd.hs_max_dtr != 0) &&
			(host->caps & MMC_CAP_MMC_HIGHSPEED)) {
		err = mmc_emergency_switch(host, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_HS_TIMING, 1, true);
		if (err && err != -EBADMSG)
			goto err;

		if (err) {
			pr_warn("%s: switch to highspeed failed\n",
					__func__);
			err = 0;
		} else {
			mmc_card_set_highspeed(card);
			host->ios.timing = MMC_TIMING_MMC_HS;
			host->panic_ops->set_ios(host);
		}
	}

	/*
	 * Compute bus speed.
	 */
	max_dtr = (unsigned int)-1;

	if (mmc_card_highspeed(card)) {
		if (max_dtr > card->ext_csd.hs_max_dtr)
			max_dtr = card->ext_csd.hs_max_dtr;
		if (max_dtr > MMC_HIGH_52_MAX_DTR)
			max_dtr = MMC_HIGH_52_MAX_DTR;
	} else if (max_dtr > card->csd.max_dtr) {
		max_dtr = card->csd.max_dtr;
	}

	host->ios.clock = max_dtr;
	host->panic_ops->set_ios(host);

	/*
	 * Activate wide bus.
	 * By default use SDR mode for panic write
	 */
	if ((card->csd.mmca_vsn >= CSD_SPEC_VER_4) &&
		(host->caps & (MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA))) {
		unsigned ext_csd_bit, bus_width;

		if (host->caps & MMC_CAP_8_BIT_DATA) {
			ext_csd_bit = EXT_CSD_BUS_WIDTH_8;
			bus_width = MMC_BUS_WIDTH_8;
		} else {
			ext_csd_bit = EXT_CSD_BUS_WIDTH_4;
			bus_width = MMC_BUS_WIDTH_4;
		}

		err = mmc_emergency_switch(host, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_BUS_WIDTH, ext_csd_bit, true);

		if (err && err != -EBADMSG)
			goto err;

		if (err) {
			pr_warn("%s: switch to bus %dbit failed\n",
				__func__, 1 << bus_width);
			err = 0;
		} else {
			ddr = MMC_SDR_MODE;
			host->ios.bus_width = bus_width;
			host->panic_ops->set_ios(host);
		}
	}

	return 0;
err:
	return err;
}

/*
 * mmc_emergency_write - write 512Bytes to card in panic mode
 * @data: data pointer which should pointed to an area no more than
 * 512Bytes
 * @blk_id: the block id need to write this 512B data
 *
 * This function is supplied to ipanic driver to write 512B data
 * in panic mode. Please also make sure the data size should not be
 * larger than 512B, otherwise data lossing.
 */
int mmc_emergency_write(char *data, unsigned int blk_id)
{
	struct mmc_panic_host *host = panic_host;
	int ret;
	if (host == NULL) {
		pr_err("%s: no device for panic record\n", __func__);
		return -ENODEV;
	}

	if (!host->panic_ready) {
		pr_err("%s: device is not ready for panic record\n", __func__);
		return -EPERM;
	}

	if (!data) {
		pr_err("%s: invalided writing data\n", __func__);
		return -EINVAL;
	}

	if (blk_id > host->totalsecs || blk_id < 0) {
		pr_err("%s: invalided writing blk_id\n", __func__);
		return -EINVAL;
	}
	/*
	 * everything is OK. So, let's start panic record.
	 *
	 * Copy the message data to logbuf
	 */
	memcpy(host->logbuf, data, SECTOR_SIZE);

	/* hold Dekker mutex first */
	if (host->panic_ops->hold_mutex && host->panic_ops->release_mutex) {
		ret = host->panic_ops->hold_mutex(host);
		if (ret) {
			pr_err("%s: hold Dekker mutex failed\n", __func__);
			return ret;
		}
	}

	ret = __mmc_emergency_write(blk_id);

	/* release Dekker mutex */
	if (host->panic_ops->hold_mutex && host->panic_ops->release_mutex)
		host->panic_ops->release_mutex(host);

	return ret;
}
EXPORT_SYMBOL(mmc_emergency_write);

/*
 * mmc_emergency_init: init host controller and emmc card
 * when kernel panic occures
 *
 * return value:
 * 0 - init successfully
 * negative value - Failed during init
 * -ENODEV - emmc card was removed by driver
 */
int mmc_emergency_init(void)
{
	struct mmc_panic_host *host = panic_host;
	int ret;
	if (host == NULL || !host->mmc || !host->mmc->card) {
		pr_err("%s: no device for panic record\n", __func__);
		return -ENODEV;
	}

	ret = mmc_emergency_prepare();
	if (ret) {
		pr_err("%s: prepare host controller failed\n", __func__);
		return ret;
	}

	if (!host->panic_ops) {
		pr_err("%s: no panic_ops for panic host\n", __func__);
		return -EPERM;
	}

	/*
	 * prepare host controller
	 */
	if (host->panic_ops->prepare)
		host->panic_ops->prepare(host);

	/*
	 * during init eMMC card, don't want to be interrupted by SCU FW
	 */
	if (host->panic_ops->hold_mutex && host->panic_ops->release_mutex) {
		ret = host->panic_ops->hold_mutex(host);
		if (ret) {
			pr_err("%s: hold Dekker mutex failed\n", __func__);
			return ret;
		}
	} else if (host->panic_ops->power_on)
		/* don't have Dekker mutex, just power on host controller */
		host->panic_ops->power_on(host);

	/*
	 * reset card since we are not sure whether card is in a good status
	 *
	 * Since in panic mode, we init a old card, so all the command to be
	 * used has no data. So we can reuse the sdhci ops
	 */
	ret = mmc_emergency_reinit_card();
	if (ret) {
		pr_info("%s: reinit card failed\n", __func__);
		goto out;
	}

	/*
	 * OK. we are ready
	 */
	mmc_emergency_ready();
out:
	/* release Dekker mutex */
	if (host->panic_ops->hold_mutex && host->panic_ops->release_mutex)
		host->panic_ops->release_mutex(host);

	return ret;
}
EXPORT_SYMBOL(mmc_emergency_init);

/*
 * mmc_emergency_setup - init panic_host which is used for panic writing
 * @host: mmc host
 *
 * This function can sample some important value for panic_host use to init
 * host controller and card. It only works for the driver which has already
 * called mmc_alloc_panic_host in its probing process
 */
void mmc_emergency_setup(struct mmc_host *mmc)
{
	struct mmc_panic_host *host = panic_host;

	/*
	 * mmc host has no panic host
	 */
	if (!mmc->phost)
		return;

	/*
	 * before setup panic host, make sure panic host is
	 * allocated
	 */
	if (host == NULL)
		return;

	/*
	 * panic host has already been setup
	 */
	if (host->mmc)
		return;

	/*
	 * mmc host didn't init card
	 */
	if (!mmc->card)
		return;
	/*
	 * if is SDIO card or SD card, by pass
	 */
	if (mmc_card_sdio(mmc->card))
		return;

	kfree(host->card);

	host->card = kzalloc(sizeof(struct mmc_card), GFP_KERNEL);
	if (!host->card) {
		pr_err("%s: cannot alloc mmc_card for panic host\n",
				__func__);
		return;
	}

	memcpy(host->card, mmc->card, sizeof(struct mmc_card));
	host->caps = mmc->caps;
	host->caps2 = mmc->caps2;
	host->mmc = mmc;
	host->ocr = mmc->ocr;
	host->totalsecs = mmc_get_capacity(mmc->card);
	host->max_blk_size = mmc->max_blk_size;
	host->max_blk_count = mmc->max_blk_count;
	host->max_req_size = mmc->max_req_size;
	if (mmc_card_blockaddr(mmc->card))
		host->blkaddr = 1;
	/*
	 * sample ios values
	 */
	memset(&host->ios, 0, sizeof(struct mmc_ios));
	memcpy(&host->ios, &mmc->ios, sizeof(struct mmc_ios));
#ifdef CONFIG_MMC_CLKGATE
	if (mmc->ios.clock == 0)
		host->ios.clock = mmc->clk_old;
#endif
	if (host->panic_ops && host->panic_ops->setup)
		host->panic_ops->setup(host);

	return;
}
EXPORT_SYMBOL(mmc_emergency_setup);
/*
 * mmc_alloc_panic_host - used for host layer driver to alloc mmc_panic_host.
 * @host: mmc host
 * @ops: this is a pointer which points to mmc_host_panic_ops. This ops should
 * be defined in host layer driver
 *
 * This function need to know the mmc_host_panic_ops, host layer driver should
 * call this function during probing.
 *
 */
void mmc_alloc_panic_host(struct mmc_host *host,
		const struct mmc_host_panic_ops *ops)
{
	if (panic_host) {
		pr_info("%s: already allocate panic host\n", __func__);
		return;
	}

	panic_host = kzalloc(sizeof(struct mmc_panic_host), GFP_KERNEL);
	if (!panic_host) {
		pr_err("%s %s: panic structure allocate error\n",
				__func__, mmc_hostname(host));
		return;
	}
	/*
	 * allocate log buffer and DMA buffer
	 * log buffer size is 512
	 */
	panic_host->logbuf = kzalloc(SECTOR_SIZE, GFP_KERNEL);
	if (!panic_host->logbuf) {
		pr_err("%s %s: log buf allocate error\n",
				__func__, mmc_hostname(host));
		goto free_panic_host;
	}

	panic_host->dmabuf = dma_map_single(host->parent, panic_host->logbuf,
			SECTOR_SIZE, DMA_TO_DEVICE);
	if (!panic_host->dmabuf) {
		pr_err("%s %s: DMA buf allocate error\n",
				__func__, mmc_hostname(host));
		goto free_logbuf;
	}

	panic_host->panic_ops = ops;
	panic_host->mmc = NULL;
	host->phost = panic_host;

	return;

free_logbuf:
	kfree(panic_host->logbuf);
free_panic_host:
	kfree(panic_host);
}
EXPORT_SYMBOL(mmc_alloc_panic_host);
