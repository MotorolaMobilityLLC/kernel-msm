/*
 *  linux/drivers/mmc/core/mmc_ops.h
 *
 *  Copyright 2006-2007 Pierre Ossman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#include <linux/slab.h>
#include <linux/export.h>
#include <linux/types.h>
#include <linux/scatterlist.h>

#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>

#include "core.h"
#include "mmc_ops.h"

#define MMC_OPS_TIMEOUT_MS	(10 * 60 * 1000) /* 10 minute timeout */

static int _mmc_select_card(struct mmc_host *host, struct mmc_card *card)
{
	int err;
	struct mmc_command cmd = {0};

	BUG_ON(!host);

	cmd.opcode = MMC_SELECT_CARD;

	if (card) {
		cmd.arg = card->rca << 16;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
	} else {
		cmd.arg = 0;
		cmd.flags = MMC_RSP_NONE | MMC_CMD_AC;
	}

	err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	return 0;
}

int mmc_select_card(struct mmc_card *card)
{
	BUG_ON(!card);

	return _mmc_select_card(card->host, card);
}

int mmc_deselect_cards(struct mmc_host *host)
{
	return _mmc_select_card(host, NULL);
}

int mmc_card_sleepawake(struct mmc_host *host, int sleep)
{
	struct mmc_command cmd = {0};
	struct mmc_card *card = host->card;
	int err;

	if (sleep)
		mmc_deselect_cards(host);

	cmd.opcode = MMC_SLEEP_AWAKE;
	cmd.arg = card->rca << 16;
	if (sleep)
		cmd.arg |= 1 << 15;

	cmd.flags = MMC_RSP_R1B | MMC_CMD_AC;
	err = mmc_wait_for_cmd(host, &cmd, 0);
	if (err)
		return err;

	/*
	 * If the host does not wait while the card signals busy, then we will
	 * will have to wait the sleep/awake timeout.  Note, we cannot use the
	 * SEND_STATUS command to poll the status because that command (and most
	 * others) is invalid while the card sleeps.
	 */
	if (!(host->caps & MMC_CAP_WAIT_WHILE_BUSY))
		mmc_delay(DIV_ROUND_UP(card->ext_csd.sa_timeout, 10000));

	if (!sleep)
		err = mmc_select_card(card);

	return err;
}

int mmc_go_idle(struct mmc_host *host)
{
	int err;
	struct mmc_command cmd = {0};

	/*
	 * Non-SPI hosts need to prevent chipselect going active during
	 * GO_IDLE; that would put chips into SPI mode.  Remind them of
	 * that in case of hardware that won't pull up DAT3/nCS otherwise.
	 *
	 * SPI hosts ignore ios.chip_select; it's managed according to
	 * rules that must accommodate non-MMC slaves which this layer
	 * won't even know about.
	 */
	if (!mmc_host_is_spi(host)) {
		mmc_set_chip_select(host, MMC_CS_HIGH);
		mmc_delay(1);
	}

	cmd.opcode = MMC_GO_IDLE_STATE;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_NONE | MMC_CMD_BC;

	err = mmc_wait_for_cmd(host, &cmd, 0);

	mmc_delay(1);

	if (!mmc_host_is_spi(host)) {
		mmc_set_chip_select(host, MMC_CS_DONTCARE);
		mmc_delay(1);
	}

	host->use_spi_crc = 0;

	return err;
}

int mmc_send_op_cond(struct mmc_host *host, u32 ocr, u32 *rocr)
{
	struct mmc_command cmd = {0};
	int i, err = 0;

	BUG_ON(!host);

	cmd.opcode = MMC_SEND_OP_COND;
	cmd.arg = mmc_host_is_spi(host) ? 0 : ocr;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R3 | MMC_CMD_BCR;

	for (i = 200; i; i--) {
		err = mmc_wait_for_cmd(host, &cmd, 0);
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

		usleep_range(5000, 5500);
	}

	if (rocr && !mmc_host_is_spi(host))
		*rocr = cmd.resp[0];

	return err;
}

int mmc_all_send_cid(struct mmc_host *host, u32 *cid)
{
	int err;
	struct mmc_command cmd = {0};

	BUG_ON(!host);
	BUG_ON(!cid);

	cmd.opcode = MMC_ALL_SEND_CID;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R2 | MMC_CMD_BCR;

	err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	memcpy(cid, cmd.resp, sizeof(u32) * 4);

	return 0;
}

int mmc_set_relative_addr(struct mmc_card *card)
{
	int err;
	struct mmc_command cmd = {0};

	BUG_ON(!card);
	BUG_ON(!card->host);

	cmd.opcode = MMC_SET_RELATIVE_ADDR;
	cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;

	err = mmc_wait_for_cmd(card->host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	return 0;
}

static int
mmc_send_cxd_native(struct mmc_host *host, u32 arg, u32 *cxd, int opcode)
{
	int err;
	struct mmc_command cmd = {0};

	BUG_ON(!host);
	BUG_ON(!cxd);

	cmd.opcode = opcode;
	cmd.arg = arg;
	cmd.flags = MMC_RSP_R2 | MMC_CMD_AC;

	err = mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	memcpy(cxd, cmd.resp, sizeof(u32) * 4);

	return 0;
}

/*
 * NOTE: void *buf, caller for the buf is required to use DMA-capable
 * buffer or on-stack buffer (with some overhead in callee).
 */
static int
mmc_send_cxd_data(struct mmc_card *card, struct mmc_host *host,
		u32 opcode, void *buf, unsigned len)
{
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;
	void *data_buf;
	int is_on_stack;

	is_on_stack = object_is_on_stack(buf);
	if (is_on_stack) {
		/*
		 * dma onto stack is unsafe/nonportable, but callers to this
		 * routine normally provide temporary on-stack buffers ...
		 */
		data_buf = kmalloc(len, GFP_KERNEL);
		if (!data_buf)
			return -ENOMEM;
	} else
		data_buf = buf;

	mrq.cmd = &cmd;
	mrq.data = &data;

	cmd.opcode = opcode;
	cmd.arg = 0;

	/* NOTE HACK:  the MMC_RSP_SPI_R1 is always correct here, but we
	 * rely on callers to never use this with "native" calls for reading
	 * CSD or CID.  Native versions of those commands use the R2 type,
	 * not R1 plus a data block.
	 */
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = len;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	sg_init_one(&sg, data_buf, len);

	if (opcode == MMC_SEND_CSD || opcode == MMC_SEND_CID) {
		/*
		 * The spec states that CSR and CID accesses have a timeout
		 * of 64 clock cycles.
		 */
		data.timeout_ns = 0;
		data.timeout_clks = 64;
	} else
		mmc_set_data_timeout(&data, card);

	mmc_wait_for_req(host, &mrq);

	if (is_on_stack) {
		memcpy(buf, data_buf, len);
		kfree(data_buf);
	}

	if (cmd.error)
		return cmd.error;
	if (data.error)
		return data.error;

	return 0;
}

int mmc_send_csd(struct mmc_card *card, u32 *csd)
{
	int ret, i;
	u32 *csd_tmp;

	if (!mmc_host_is_spi(card->host))
		return mmc_send_cxd_native(card->host, card->rca << 16,
				csd, MMC_SEND_CSD);

	csd_tmp = kmalloc(16, GFP_KERNEL);
	if (!csd_tmp)
		return -ENOMEM;

	ret = mmc_send_cxd_data(card, card->host, MMC_SEND_CSD, csd_tmp, 16);
	if (ret)
		goto err;

	for (i = 0;i < 4;i++)
		csd[i] = be32_to_cpu(csd_tmp[i]);

err:
	kfree(csd_tmp);
	return ret;
}

int mmc_send_cid(struct mmc_host *host, u32 *cid)
{
	int ret, i;
	u32 *cid_tmp;

	if (!mmc_host_is_spi(host)) {
		if (!host->card)
			return -EINVAL;
		return mmc_send_cxd_native(host, host->card->rca << 16,
				cid, MMC_SEND_CID);
	}

	cid_tmp = kmalloc(16, GFP_KERNEL);
	if (!cid_tmp)
		return -ENOMEM;

	ret = mmc_send_cxd_data(NULL, host, MMC_SEND_CID, cid_tmp, 16);
	if (ret)
		goto err;

	for (i = 0;i < 4;i++)
		cid[i] = be32_to_cpu(cid_tmp[i]);

err:
	kfree(cid_tmp);
	return ret;
}

int mmc_send_ext_csd(struct mmc_card *card, u8 *ext_csd)
{
	return mmc_send_cxd_data(card, card->host, MMC_SEND_EXT_CSD,
			ext_csd, 512);
}
EXPORT_SYMBOL_GPL(mmc_send_ext_csd);

int mmc_spi_read_ocr(struct mmc_host *host, int highcap, u32 *ocrp)
{
	struct mmc_command cmd = {0};
	int err;

	cmd.opcode = MMC_SPI_READ_OCR;
	cmd.arg = highcap ? (1 << 30) : 0;
	cmd.flags = MMC_RSP_SPI_R3;

	err = mmc_wait_for_cmd(host, &cmd, 0);

	*ocrp = cmd.resp[1];
	return err;
}

int mmc_spi_set_crc(struct mmc_host *host, int use_crc)
{
	struct mmc_command cmd = {0};
	int err;

	cmd.opcode = MMC_SPI_CRC_ON_OFF;
	cmd.flags = MMC_RSP_SPI_R1;
	cmd.arg = use_crc;

	err = mmc_wait_for_cmd(host, &cmd, 0);
	if (!err)
		host->use_spi_crc = use_crc;
	return err;
}

/**
 *	__mmc_switch - modify EXT_CSD register
 *	@card: the MMC card associated with the data transfer
 *	@set: cmd set values
 *	@index: EXT_CSD register index
 *	@value: value to program into EXT_CSD register
 *	@timeout_ms: timeout (ms) for operation performed by register write,
 *                   timeout of zero implies maximum possible timeout
 *	@use_busy_signal: use the busy signal as response type
 *
 *	Modifies the EXT_CSD register for selected card.
 */
int __mmc_switch(struct mmc_card *card, u8 set, u8 index, u8 value,
	       unsigned int timeout_ms, bool use_busy_signal)
{
	int err;
	struct mmc_command cmd = {0};
	unsigned long timeout;
	u32 status;

	BUG_ON(!card);
	BUG_ON(!card->host);

	cmd.opcode = MMC_SWITCH;
	cmd.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
		  (index << 16) |
		  (value << 8) |
		  set;
	cmd.flags = MMC_CMD_AC;
	if (use_busy_signal)
		cmd.flags |= MMC_RSP_SPI_R1B | MMC_RSP_R1B;
	else
		cmd.flags |= MMC_RSP_SPI_R1 | MMC_RSP_R1;


	cmd.cmd_timeout_ms = timeout_ms;

	err = mmc_wait_for_cmd(card->host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	/* No need to check card status in case of unblocking command */
	if (!use_busy_signal)
		return 0;

	/* Must check status to be sure of no errors */
	timeout = jiffies + msecs_to_jiffies(MMC_OPS_TIMEOUT_MS);
	do {
		err = mmc_send_status(card, &status);
		if (err) {
			if (err == -EILSEQ && index == EXT_CSD_HS_TIMING) {
				pr_warn("%s: CMD13 error after switching timing\n"
					"%s: this error can be ignored...\n",
					mmc_hostname(card->host),
					mmc_hostname(card->host));
				return 0;
			} else
				return err;
		}
		if (card->host->caps & MMC_CAP_WAIT_WHILE_BUSY)
			break;
		if (mmc_host_is_spi(card->host))
			break;

		/* Timeout if the device never leaves the program state. */
		if (time_after(jiffies, timeout)) {
			pr_err("%s: Card stuck in programming state! %s\n",
				mmc_hostname(card->host), __func__);
			return -ETIMEDOUT;
		}
	} while (R1_CURRENT_STATE(status) == R1_STATE_PRG);

	if (mmc_host_is_spi(card->host)) {
		if (status & R1_SPI_ILLEGAL_COMMAND)
			return -EBADMSG;
	} else {
		if (status & 0xFDFFA000)
			pr_warning("%s: unexpected status %#x after "
			       "switch", mmc_hostname(card->host), status);
		if (status & R1_SWITCH_ERROR)
			return -EBADMSG;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(__mmc_switch);

int mmc_switch(struct mmc_card *card, u8 set, u8 index, u8 value,
		unsigned int timeout_ms)
{
	return __mmc_switch(card, set, index, value, timeout_ms, true);
}
EXPORT_SYMBOL_GPL(mmc_switch);

int mmc_send_status(struct mmc_card *card, u32 *status)
{
	int err;
	struct mmc_command cmd = {0};

	BUG_ON(!card);
	BUG_ON(!card->host);

	cmd.opcode = MMC_SEND_STATUS;
	if (!mmc_host_is_spi(card->host))
		cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;

	err = mmc_wait_for_cmd(card->host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	/* NOTE: callers are required to understand the difference
	 * between "native" and SPI format status words!
	 */
	if (status)
		*status = cmd.resp[0];

	return 0;
}

static int
mmc_send_bus_test(struct mmc_card *card, struct mmc_host *host, u8 opcode,
		  u8 len)
{
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;
	u8 *data_buf;
	u8 *test_buf;
	int i, err;
	static u8 testdata_8bit[8] = { 0x55, 0xaa, 0, 0, 0, 0, 0, 0 };
	static u8 testdata_4bit[4] = { 0x5a, 0, 0, 0 };

	/* dma onto stack is unsafe/nonportable, but callers to this
	 * routine normally provide temporary on-stack buffers ...
	 */
	data_buf = kmalloc(len, GFP_KERNEL);
	if (!data_buf)
		return -ENOMEM;

	if (len == 8)
		test_buf = testdata_8bit;
	else if (len == 4)
		test_buf = testdata_4bit;
	else {
		pr_err("%s: Invalid bus_width %d\n",
		       mmc_hostname(host), len);
		kfree(data_buf);
		return -EINVAL;
	}

	if (opcode == MMC_BUS_TEST_W)
		memcpy(data_buf, test_buf, len);

	mrq.cmd = &cmd;
	mrq.data = &data;
	cmd.opcode = opcode;
	cmd.arg = 0;

	/* NOTE HACK:  the MMC_RSP_SPI_R1 is always correct here, but we
	 * rely on callers to never use this with "native" calls for reading
	 * CSD or CID.  Native versions of those commands use the R2 type,
	 * not R1 plus a data block.
	 */
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = len;
	data.blocks = 1;
	if (opcode == MMC_BUS_TEST_R)
		data.flags = MMC_DATA_READ;
	else
		data.flags = MMC_DATA_WRITE;

	data.sg = &sg;
	data.sg_len = 1;
	sg_init_one(&sg, data_buf, len);
	mmc_wait_for_req(host, &mrq);
	err = 0;
	if (opcode == MMC_BUS_TEST_R) {
		for (i = 0; i < len / 4; i++)
			if ((test_buf[i] ^ data_buf[i]) != 0xff) {
				err = -EIO;
				break;
			}
	}
	kfree(data_buf);

	if (cmd.error)
		return cmd.error;
	if (data.error)
		return data.error;

	return err;
}

int mmc_bus_test(struct mmc_card *card, u8 bus_width)
{
	int err, width;

	if (bus_width == MMC_BUS_WIDTH_8)
		width = 8;
	else if (bus_width == MMC_BUS_WIDTH_4)
		width = 4;
	else if (bus_width == MMC_BUS_WIDTH_1)
		return 0; /* no need for test */
	else
		return -EINVAL;

	/*
	 * Ignore errors from BUS_TEST_W.  BUS_TEST_R will fail if there
	 * is a problem.  This improves chances that the test will work.
	 */
	mmc_send_bus_test(card, card->host, MMC_BUS_TEST_W, width);
	err = mmc_send_bus_test(card, card->host, MMC_BUS_TEST_R, width);
	return err;
}

int mmc_send_hpi_cmd(struct mmc_card *card, u32 *status)
{
	struct mmc_command cmd = {0};
	unsigned int opcode;
	int err;

	if (!card->ext_csd.hpi) {
		pr_warning("%s: Card didn't support HPI command\n",
			   mmc_hostname(card->host));
		return -EINVAL;
	}

	opcode = card->ext_csd.hpi_cmd;
	if (opcode == MMC_STOP_TRANSMISSION)
		cmd.flags = MMC_RSP_R1B | MMC_CMD_AC;
	else if (opcode == MMC_SEND_STATUS)
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
	else
		return -EINVAL;

	cmd.opcode = opcode;
	cmd.arg = card->rca << 16 | 1;

	err = mmc_wait_for_cmd(card->host, &cmd, 0);
	if (err) {
		pr_warn("%s: error %d interrupting operation. "
			"HPI command response %#x\n", mmc_hostname(card->host),
			err, cmd.resp[0]);
		return err;
	}
	if (status)
		*status = cmd.resp[0];

	return 0;
}

static int mmc_rpmb_send_command(struct mmc_card *card, u8 *buf, __u16 blks,
		__u16 type, u8 req_type)
{
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_command sbc = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;
	u8 *transfer_buf = NULL;

	mrq.sbc = &sbc;
	mrq.cmd = &cmd;
	mrq.data = &data;
	mrq.stop = NULL;
	transfer_buf = kzalloc(512 * blks, GFP_KERNEL);
	if (!transfer_buf)
		return -ENOMEM;

	/*
	 * set CMD23
	 */
	sbc.opcode = MMC_SET_BLOCK_COUNT;
	sbc.arg = blks;
	if ((req_type == RPMB_REQ) && type == RPMB_WRITE_DATA)
		sbc.arg |= 1 << 31;
	sbc.flags = MMC_RSP_R1 | MMC_CMD_AC;

	/*
	 * set CMD25/18
	 */
	sg_init_one(&sg, transfer_buf, 512 * blks);
	if (req_type == RPMB_REQ) {
		cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
		sg_copy_from_buffer(&sg, 1, buf, 512 * blks);
		data.flags |= MMC_DATA_WRITE;
	} else {
		cmd.opcode = MMC_READ_MULTIPLE_BLOCK;
		data.flags |= MMC_DATA_READ;
	}
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
	data.blksz = 512;
	data.blocks = blks;
	data.sg = &sg;
	data.sg_len = 1;

	mmc_set_data_timeout(&data, card);

	mmc_wait_for_req(card->host, &mrq);

	if (req_type != RPMB_REQ)
		sg_copy_to_buffer(&sg, 1, buf, 512 * blks);

	kfree(transfer_buf);

	if (cmd.error)
		return cmd.error;
	if (data.error)
		return data.error;
	return 0;
}

void mmc_rpmb_post_frame(struct mmc_core_rpmb_req *rpmb_req)
{
	int i;
	struct mmc_ioc_rpmb_req *p_req;
	__u8 *buf_frame;

	if (!rpmb_req || !rpmb_req->ready)
		return;

	p_req = rpmb_req->req;
	buf_frame = rpmb_req->frame;

	if (!p_req || !buf_frame)
		return;
	/*
	 * Regarding to the check rules, here is the post
	 * rules
	 * All will return result.
	 * GET_WRITE_COUNTER:
	 *              must: write counter, nonce
	 *              optional: MAC
	 * WRITE_DATA:
	 *              must: MAC, write counter
	 * READ_DATA:
	 *              must: nonce, data
	 *              optional: MAC
	 * PROGRAM_KEY:
	 *              must: Nothing
	 *
	 * Except READ_DATA, all of these operations only need to parse
	 * one frame. READ_DATA needs blks frames to get DATA
	 */

	memcpy(p_req->result, buf_frame + RPMB_RES_BEG, 2);
	*p_req->result = be16_to_cpup(p_req->result);

	if (p_req->type == RPMB_PROGRAM_KEY)
		goto out;

	if (p_req->type == RPMB_GET_WRITE_COUNTER ||
			p_req->type == RPMB_WRITE_DATA) {
		memcpy(p_req->wc, buf_frame + RPMB_WCOUNTER_BEG, 4);
		*p_req->wc = be32_to_cpup(p_req->wc);
	}

	if (p_req->type == RPMB_GET_WRITE_COUNTER ||
			p_req->type == RPMB_READ_DATA) {
		/* nonce copy */
		memcpy(p_req->nonce, buf_frame + RPMB_NONCE_BEG, 16);
	}
	/*
	 * Take MAC within the last package
	 */
	if (p_req->type == RPMB_READ_DATA) {
		__u8 *data = p_req->data;
		for (i = 0; i < p_req->blk_cnt; i++) {
			memcpy(data, buf_frame + i * 512 + RPMB_DATA_BEG, 256);
			data += 256;
		}
		/*
		 * MAC stored in the last package
		 */
		if (p_req->mac) {
			i--;
			memcpy(p_req->mac, buf_frame + i * 512 + RPMB_MAC_BEG,
					32);
		}
	} else if (p_req->mac)
		memcpy(p_req->mac, buf_frame + RPMB_MAC_BEG, 32);
out:
	kfree(buf_frame);
	rpmb_req->frame = NULL;
	return;
}
EXPORT_SYMBOL_GPL(mmc_rpmb_post_frame);

static int mmc_rpmb_request_check(struct mmc_card *card,
		struct mmc_ioc_rpmb_req *p_req)
{
	/*
	 * Some paramter is a must for the operation. Different
	 * operation expect different paramters. Below code is
	 * used for checking this.
	 *
	 * All operations will need result.
	 * GET_WRITE_COUNTER:
	 *              must: write counter, nonce
	 *              optional: MAC
	 * WRITE_DATA:
	 *              must: MAC, data, write counter
	 * READ_DATA:
	 *              must: nonce, data
	 *              optional: MAC
	 * PROGRAM_KEY:
	 *              must: MAC
	 *
	 * So here, we only check the 'must' paramters
	 */
	if (!p_req->result) {
		pr_err("%s: Type %d has NULL pointer for result\n",
				mmc_hostname(card->host), p_req->type);
		return -EINVAL;
	}

	if (p_req->type == RPMB_GET_WRITE_COUNTER) {
		if (!p_req->nonce || !p_req->wc) {
			pr_err("%s: Type %d has NULL pointer for nonce/wc\n",
					mmc_hostname(card->host), p_req->type);
			return -EINVAL;
		}
		/*
		 * used to allocate frame
		 */
		p_req->blk_cnt = 1;
	} else if (p_req->type == RPMB_WRITE_DATA ||
			p_req->type == RPMB_READ_DATA) {
		if ((__u32)(p_req->addr + p_req->blk_cnt) >
				card->ext_csd.rpmb_size) {
			pr_err("%s Type %d: beyond the RPMB partition rang addr %d, blk_cnt %d, rpmb_size %d\n",
					mmc_hostname(card->host),
					p_req->type,
					p_req->addr,
					p_req->blk_cnt,
					card->ext_csd.rpmb_size);
			/*
			 * Not return error here since we want device to handle
			 * such errors
			 */
		}
		if (p_req->blk_cnt == 0) {
			pr_err("%s: Type %d has zero block count\n",
					mmc_hostname(card->host),
					p_req->blk_cnt);
			return -EINVAL;
		} else if (p_req->blk_cnt > card->rpmb_max_req) {
			pr_err("%s: Type %d has invalid block count, cannot large than %d\n",
					mmc_hostname(card->host),
					p_req->blk_cnt,
					card->rpmb_max_req);
			return -EINVAL;
		}
		if (!p_req->data) {
			pr_err("%s: Type %d has NULL pointer for data\n",
					mmc_hostname(card->host), p_req->type);
			return -EINVAL;
		}
		if (p_req->type == RPMB_WRITE_DATA) {
			if (!p_req->wc || !p_req->mac) {
				pr_err("%s: Type %d has NULL pointer for write counter/MAC\n",
						mmc_hostname(card->host),
						p_req->type);
				return -EINVAL;
			}
		} else {
			if (!p_req->nonce) {
				pr_err("%s: Type %d has NULL pointer for nonce\n",
						mmc_hostname(card->host),
						p_req->type);
				return -EINVAL;
			}
		}
	} else
		return -EOPNOTSUPP;

	return 0;
}

/*
 * prepare the request of RPMB frame
 * RPMB frame is MSB first
 * convert needed bytes
 * return how many frames will be prepared
 */
int mmc_rpmb_pre_frame(struct mmc_core_rpmb_req *rpmb_req,
		struct mmc_card *card)
{
	int i, ret;
	struct mmc_ioc_rpmb_req *p_req;
	__u8 *buf_frame;
	__u16 blk_cnt, addr, type;
	__u32 w_counter;

	if (!rpmb_req || !card)
		return -EINVAL;

	p_req = rpmb_req->req;
	if (!p_req) {
		pr_err("%s: mmc_ioc_rpmb_req is NULL. Wrong parameter\n",
				mmc_hostname(card->host));
		return -EINVAL;
	}

	/*
	 * make sure these two items are clear
	 */
	rpmb_req->ready = 0;
	rpmb_req->frame = NULL;

	ret = mmc_rpmb_request_check(card, p_req);
	if (ret)
		return ret;

	buf_frame = kzalloc(512 * p_req->blk_cnt, GFP_KERNEL);
	if (!buf_frame) {
		pr_err("%s: cannot allocate frame for type %d\n",
				mmc_hostname(card->host), p_req->type);
		return -ENOMEM;
	}

	type = cpu_to_be16p(&p_req->type);
	if (p_req->type == RPMB_GET_WRITE_COUNTER ||
			p_req->type == RPMB_READ_DATA) {
		/*
		 * One package prepared
		 * This request needs Nonce and type
		 * If is data read, then also need addr
		 */
		memcpy(buf_frame + RPMB_TYPE_BEG, &type, 2);
		if (p_req->type == RPMB_READ_DATA) {
			addr = cpu_to_be16p(&p_req->addr);
			memcpy(buf_frame + RPMB_ADDR_BEG, &addr, 2);
		}
		/* convert Nonce code */
		memcpy(buf_frame + RPMB_NONCE_BEG, p_req->nonce, 16);
	} else if (p_req->type == RPMB_WRITE_DATA) {
		__u8 *data = p_req->data;
		/*
		 * multiple package prepared
		 * This request nees blk_cnt, addr, write_counter,
		 * data and mac
		 */
		blk_cnt = cpu_to_be16p(&p_req->blk_cnt);
		addr = cpu_to_be16p(&p_req->addr);
		w_counter = cpu_to_be32p(p_req->wc);
		for (i = 0; i < p_req->blk_cnt; i++) {
			memcpy(buf_frame + i * 512 + RPMB_TYPE_BEG,
					&type, 2);
			memcpy(buf_frame + i * 512 + RPMB_BLKS_BEG,
					&blk_cnt, 2);
			memcpy(buf_frame + i * 512 + RPMB_ADDR_BEG,
					&addr, 2);
			memcpy(buf_frame + i * 512 + RPMB_WCOUNTER_BEG,
					&w_counter, 4);
			memcpy(buf_frame + i * 512 + RPMB_DATA_BEG,
					data, 256);
			data += 256;
		}
		/* convert MAC code */
		memcpy(buf_frame + 512 * (i - 1) + RPMB_MAC_BEG,
				p_req->mac, 32);
	} else {
		pr_err("%s: We shouldn't be here\n", mmc_hostname(card->host));
		kfree(buf_frame);
		return -EINVAL;
	}
	rpmb_req->ready = 1;
	rpmb_req->frame = buf_frame;
	return 0;
}
EXPORT_SYMBOL_GPL(mmc_rpmb_pre_frame);

int mmc_rpmb_partition_ops(struct mmc_core_rpmb_req *rpmb_req,
		struct mmc_card *card)
{
	int err = 0;
	struct mmc_ioc_rpmb_req *p_req;
	__u16 type, blks;
	__u8 *buf_frame;

	if (!rpmb_req || !card)
		return -EINVAL;

	p_req = rpmb_req->req;
	buf_frame = rpmb_req->frame;

	if (!p_req || !rpmb_req->ready || !buf_frame) {
		pr_err("%s: mmc_ioc_rpmb_req is not prepared\n",
				mmc_hostname(card->host));
		return -EINVAL;
	}

	type = p_req->type;
	blks = p_req->blk_cnt;

	/*
	 * STEP 1: send request to RPMB partition
	 */
	if (type == RPMB_WRITE_DATA)
		err = mmc_rpmb_send_command(card, buf_frame, blks,
				type, RPMB_REQ);
	else
		err = mmc_rpmb_send_command(card, buf_frame, 1, type, RPMB_REQ);

	if (err) {
		pr_err("%s: request write counter failed (%d)\n",
				mmc_hostname(card->host), err);
		goto out;
	}

	memset(buf_frame, 0, 512 * blks);
	/*
	 * STEP 2: check write result
	 * Only for WRITE_DATA or Program key
	 */
	if (type == RPMB_WRITE_DATA) {
		buf_frame[RPMB_TYPE_BEG + 1] = RPMB_RESULT_READ;
		err = mmc_rpmb_send_command(card, buf_frame, 1,
				RPMB_RESULT_READ, RPMB_REQ);
		if (err) {
			pr_err("%s: request write counter failed (%d)\n",
					mmc_hostname(card->host), err);
			goto out;
		}
	}

	/*
	 * STEP 3: get response from RPMB partition
	 */

	if (type == RPMB_READ_DATA)
		err = mmc_rpmb_send_command(card, buf_frame,
				blks, type, RPMB_RESP);
	else
		err = mmc_rpmb_send_command(card, buf_frame,
				1, type, RPMB_RESP);
	if (err) {
		pr_err("%s: response write counter failed (%d)\n",
				mmc_hostname(card->host), err);
	}
out:
	return err;
}
EXPORT_SYMBOL_GPL(mmc_rpmb_partition_ops);

static int mmc_switch_part(struct mmc_card *card, u8 part)
{
	int ret;

	ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_PART_CONFIG, part,
			card->ext_csd.part_time);
	if (ret)
		pr_err("%s: switch failed with %d, part %d\n",
				__func__, ret, part);

	return ret;
}
/*
 * @part: GPP partition part number
 * @addr: GPP write group
 */
int mmc_wp_status(struct mmc_card *card, unsigned int part,
		unsigned int addr, u8 *wp_status)
{
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};
	struct mmc_request mrq = {0};
	struct scatterlist sg;
	u32 status = 0;
	int err = 0;
	u8 *rbuf = NULL;

	if (!card)
		return -ENODEV;

	if (!card->ext_csd.gpp_sz[part - EXT_CSD_PART_CONFIG_ACC_GP0]) {
		pr_err("%s: doesn't have GPP%d\n", __func__,
				part - 3);
		return -ENODEV;
	}

	rbuf = kzalloc(8, GFP_KERNEL);
	if (rbuf == NULL) {
		pr_err("%s: no memory\n", __func__);
		return -ENOMEM;
	}

	cmd.opcode = MMC_SEND_WRITE_PROT_TYPE;
	cmd.arg = addr * card->ext_csd.wpg_sz;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	data.sg = &sg;
	data.sg_len = 1;
	data.blksz = 8;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	sg_init_one(data.sg, rbuf, 8);
	mrq.data = &data;
	mrq.cmd = &cmd;

	mmc_claim_host(card->host);

	mmc_set_data_timeout(&data, card);

	err = mmc_switch_part(card, part);
	if (err) {
		mmc_release_host(card->host);
		dev_err(mmc_dev(card->host), "%s: swith error %d\n",
				__func__, err);
		goto out;
	}

	mmc_wait_for_req(card->host, &mrq);
	if (cmd.error) {
		dev_err(mmc_dev(card->host), "%s: cmd error %d\n",
				__func__, cmd.error);
	}
	if (data.error) {
		dev_err(mmc_dev(card->host), "%s: data error %d\n",
				__func__, data.error);
	}

	/* Must check status to be sure of no errors */
	do {
		err = mmc_send_status(card, &status);
		if (err) {
			pr_err("%s: get card status err %d, status 0x%x\n",
					__func__, err, status);
			goto out;
		}
		if (card->host->caps & MMC_CAP_WAIT_WHILE_BUSY)
			break;
		if (mmc_host_is_spi(card->host))
			break;
	} while (R1_CURRENT_STATE(status) == R1_STATE_PRG);

	if (mmc_host_is_spi(card->host)) {
		if (status & R1_SPI_ILLEGAL_COMMAND) {
			pr_err("%s: error card status 0x%x\n",
					__func__, status);
			goto out;
		}
	} else {
		if (status & 0xFDFFA000)
			pr_warn("%s: unexpected status %#x after switch",
					__func__, status);
		if (status & R1_SWITCH_ERROR) {
			pr_err("%s: card switch error, status 0x%x\n",
					__func__, status);
		}
		if (status & R1_OUT_OF_RANGE) {
			pr_err("%s: addr out of range, status 0x%x\n",
					__func__, status);
			goto out;
		}
	}

	mmc_switch_part(card, EXT_CSD_PART_CONFIG_ACC_USER);

	mmc_release_host(card->host);

	sg_copy_from_buffer(data.sg, 1, rbuf, 8);

	/*
	 * the first write protect group type is in the last two
	 * bits in the last byte read from the device.
	 */
	*wp_status = rbuf[7] & 0x3;

	kfree(rbuf);

	return 0;
out:
	kfree(rbuf);

	return -EPERM;
}
EXPORT_SYMBOL_GPL(mmc_wp_status);

/**
 *     mmc_switch_bits - modify EXT_CSD register
 *     @card: the MMC card associated with the data transfer
 *     @set: cmd set values
 *     @index: EXT_CSD register index
 *     @value: value to program into EXT_CSD register
 *     @timeout_ms: timeout (ms) for operation performed by register write,
 *                   timeout of zero implies maximum possible timeout
 *     @check_busy: Set the 'R1B' flag or not. Some operations, such as
 *                   Sanitize, may need long time to finish. And some
 *                   host controller, such as the SDHCI host controller,
 *                   only allows limited max timeout value. So, introduce
 *                   this to skip the busy check for those operations.
 *     @set: true when want to set value; false when want to clear value
 *
 *     Modifies the EXT_CSD register for selected card.
 */
static int mmc_switch_bits(struct mmc_card *card, u8 cmdset, u8 index, u8 value,
		unsigned int timeout_ms, int check_busy, bool set)
{
	int err;
	struct mmc_command cmd = {0};
	u32 status;
	u8 access = set ? MMC_SWITCH_MODE_SET_BITS :
		MMC_SWITCH_MODE_CLEAR_BITS;

	BUG_ON(!card);
	BUG_ON(!card->host);

	cmd.opcode = MMC_SWITCH;
	cmd.arg = (access << 24) |
		(index << 16) |
		(value << 8) |
		cmdset;
	if (check_busy)
		cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
	else
		cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;

	cmd.cmd_timeout_ms = timeout_ms;

	err = mmc_wait_for_cmd(card->host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	/* Must check status to be sure of no errors */
	do {
		err = mmc_send_status(card, &status);
		if (err)
			return err;
		if (card->host->caps & MMC_CAP_WAIT_WHILE_BUSY)
			break;
		if (mmc_host_is_spi(card->host))
			break;
	} while (R1_CURRENT_STATE(status) == R1_STATE_PRG);

	if (mmc_host_is_spi(card->host)) {
		if (status & R1_SPI_ILLEGAL_COMMAND)
			return -EBADMSG;
	} else {
		if (status & 0xFDFFA000)
			pr_warn("%s: unexpected status %#x\n",
					mmc_hostname(card->host), status);
		if (status & R1_SWITCH_ERROR)
			return -EBADMSG;
	}

	return 0;
}
/*
 * This needs to be called with host claimed
 * @part: GPP partition part ID, should be 1/2/3/4.
 * @addr: GPP write group unit
 */
int mmc_set_user_wp(struct mmc_card *card, unsigned int part,
		unsigned int wpg)
{
	struct mmc_command cmd = {0};
	int err = 0;
	u32 status = 0;

	if (!card)
		return -ENODEV;

	mmc_claim_host(card->host);

	/*
	 * enable WP to partitions
	 * set bit2 of ext_csd[171], permanent write protect
	 */
	err = mmc_switch_bits(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_USER_WP,
			EXT_CSD_PERMANENT_WP, card->ext_csd.generic_cmd6_time,
			true, true);
	if (err) {
		pr_err("%s: enable permanent write protect err %d!\n",
				__func__, err);
		mmc_release_host(card->host);
		return err;
	}

	err = mmc_switch_part(card, part);
	if (err)
		goto switchback;

	cmd.opcode = MMC_SET_WRITE_PROT;
	cmd.arg = wpg * card->ext_csd.wpg_sz;
	cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
	err = mmc_wait_for_cmd(card->host, &cmd, MMC_CMD_RETRIES);
	if (err) {
		pr_err("%s: failed to set addr 0x%x write protected, err %d\n",
				__func__, cmd.arg, err);
		goto out;
	}

	/* Must check status to be sure of no errors */
	do {
		err = mmc_send_status(card, &status);
		if (err) {
			pr_err("%s: card status get err %d, status 0x%x\n",
					__func__, err, status);
			goto out;
		}
		if (card->host->caps & MMC_CAP_WAIT_WHILE_BUSY)
			break;
		if (mmc_host_is_spi(card->host))
			break;
	} while (R1_CURRENT_STATE(status) == R1_STATE_PRG);

	if (mmc_host_is_spi(card->host)) {
		if (status & R1_SPI_ILLEGAL_COMMAND) {
			pr_err("%s: error card status 0x%x\n",
					__func__, status);
			err = -EILSEQ;
			goto out;
		}
	} else {
		if (status & 0xFDFFA000)
			pr_warn("%s: unexpected status %#x after switch",
					__func__, status);
		if (status & R1_SWITCH_ERROR) {
			pr_err("%s: card switch error, status 0x%x\n",
					__func__, status);
			err = -EIO;
			goto out;
		}
		if (status & R1_OUT_OF_RANGE) {
			pr_err("%s: addr out of range, status 0x%x\n",
					__func__, status);
			err = -EINVAL;
		}
	}

out:
	err = mmc_switch_part(card, EXT_CSD_PART_CONFIG_ACC_USER);
	if (err) {
		pr_warn("%s: switch to USER partition failed!\n", __func__);
		WARN_ON(err);
	}

switchback:
	/*
	 * clear bit2 of ext_csd[171], permanent write protect
	 */
	err = mmc_switch_bits(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_USER_WP,
			EXT_CSD_PERMANENT_WP, card->ext_csd.generic_cmd6_time,
			true, false);
	if (err) {
		pr_err("%s: clear write protect err %d!\n",
				__func__, err);
	}

	mmc_release_host(card->host);

	return err;
}
EXPORT_SYMBOL_GPL(mmc_set_user_wp);
