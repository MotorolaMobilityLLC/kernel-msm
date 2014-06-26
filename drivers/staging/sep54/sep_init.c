/*******************************************************************
* (c) Copyright 2011-2012 Discretix Technologies Ltd.              *
* This software is protected by copyright, international           *
* treaties and patents, and distributed under multiple licenses.   *
* Any use of this Software as part of the Discretix CryptoCell or  *
* Packet Engine products requires a commercial license.            *
* Copies of this Software that are distributed with the Discretix  *
* CryptoCell or Packet Engine product drivers, may be used in      *
* accordance with a commercial license, or at the user's option,   *
* used and redistributed under the terms and conditions of the GNU *
* General Public License ("GPL") version 2, as published by the    *
* Free Software Foundation.                                        *
* This program is distributed in the hope that it will be useful,  *
* but WITHOUT ANY LIABILITY AND WARRANTY; without even the implied *
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. *
* See the GNU General Public License version 2 for more details.   *
* You should have received a copy of the GNU General Public        *
* License version 2 along with this Software; if not, please write *
* to the Free Software Foundation, Inc., 59 Temple Place - Suite   *
* 330, Boston, MA 02111-1307, USA.                                 *
* Any copy or reproduction of this Software, as permitted under    *
* the GNU General Public License version 2, must include this      *
* Copyright Notice as well as any other notices provided under     *
* the said license.                                                *
********************************************************************/

/* \file
   This file implements the SeP FW initialization sequence.
   This is part of the Discretix CC initalization specifications              */

#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_SEP_INIT

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/firmware.h>
/* Registers definitions from shared/hw/include */
#include "dx_reg_base_host.h"
#include "dx_host.h"
#define DX_CC_HOST_VIRT	/* must be defined before including dx_cc_regs.h */
#include "dx_cc_regs.h"
#include "dx_bitops.h"
#include "sep_log.h"
#include "dx_driver.h"
#include "dx_init_cc_abi.h"
#include "dx_init_cc_defs.h"
#include "sep_sram_map.h"
#include "sep_init.h"
#include "sep_request_mgr.h"
#include "sep_power.h"

#ifdef DEBUG
#define FW_INIT_TIMEOUT_SEC     10
#define COLD_BOOT_TIMEOUT_SEC	10
#else
#define FW_INIT_TIMEOUT_SEC     3
#define COLD_BOOT_TIMEOUT_SEC	3
#endif
#define FW_INIT_TIMEOUT_MSEC	(FW_INIT_TIMEOUT_SEC * 1000)
#define COLD_BOOT_TIMEOUT_MSEC  (COLD_BOOT_TIMEOUT_SEC * 1000)

#define FW_INIT_PARAMS_BUF_LEN		1024

/*** CC_INIT handlers ***/

/**
 * struct cc_init_ctx - CC init. context
 * @drvdata:		Associated device driver
 * @resident_p:		Pointer to resident image buffer
 * @resident_dma_addr:	DMA address of resident image buffer
 * @resident_size:	Size in bytes of the resident image
 * @cache_p:	Pointer to (i)cache image buffer
 * @cache_dma_addr:	DMA address of the (i)cache image
 * @cache_size:		Size in bytes if the (i)cache image
 * @vrl_p:		Pointer to VRL image buffer
 * @vrl_dma_addr:	DMA address of the VRL
 * @vrl_size:		Size in bytes of the VRL
 * @msg_buf:		A buffer for building the CC-Init message
 */
struct cc_init_ctx {
	struct sep_drvdata *drvdata;
	void *resident_p;
	dma_addr_t resident_dma_addr;
	size_t resident_size;
	void *cache_p;
	dma_addr_t cache_dma_addr;
	size_t cache_size;
	void *vrl_p;
	dma_addr_t vrl_dma_addr;
	size_t vrl_size;
	u32 msg_buf[DX_CC_INIT_MSG_LENGTH];
};

static void destroy_cc_init_ctx(struct cc_init_ctx *ctx)
{
	struct device *mydev = ctx->drvdata->dev;

	if (ctx->vrl_p != NULL)
		dma_free_coherent(mydev, ctx->vrl_size,
				  ctx->vrl_p, ctx->vrl_dma_addr);
	if (ctx->cache_p != NULL)
		dma_free_coherent(mydev, ctx->cache_size,
				  ctx->cache_p, ctx->cache_dma_addr);
	if (ctx->resident_p != NULL)
		dma_free_coherent(mydev, ctx->resident_size,
				  ctx->resident_p, ctx->resident_dma_addr);
	kfree(ctx);
}

/**
 * fetch_image() - Fetch CC image using request_firmware mechanism and
 *	locate it in a DMA coherent buffer.
 *
 * @mydev:
 * @image_name:		Image file name (from /lib/firmware/)
 * @image_pp:		Allocated image buffer
 * @image_dma_addr_p:	Allocated image DMA addr
 * @image_size_p:	Loaded image size
 */
static int fetch_image(struct device *mydev, const char *image_name,
		       void **image_pp, dma_addr_t *image_dma_addr_p,
		       size_t *image_size_p)
{
	const struct firmware *image;
	int rc;

	rc = request_firmware(&image, image_name, mydev);
	if (unlikely(rc != 0)) {
		pr_err("Failed loading image %s (%d)\n", image_name, rc);
		return -ENODEV;
	}
	*image_pp = dma_alloc_coherent(mydev,
				       image->size, image_dma_addr_p,
				       GFP_KERNEL);
	if (unlikely(*image_pp == NULL)) {
		pr_err("Failed allocating DMA mem. for resident image\n");
		rc = -ENOMEM;
	} else {
		memcpy(*image_pp, image->data, image->size);
		*image_size_p = image->size;
	}
	/* Image copied into the DMA coherent buffer. No need for "firmware" */
	release_firmware(image);
	if (likely(rc == 0))
		pr_debug("%s: %zu Bytes\n", image_name, *image_size_p);
	return rc;
}

#ifdef CACHE_IMAGE_NAME
static enum dx_cc_init_msg_icache_size icache_size_to_enum(u8
							   icache_size_log2)
{
	int i;
	const int icache_sizes_enum2log[] = DX_CC_ICACHE_SIZE_ENUM2LOG;

	for (i = 0; i < sizeof(icache_sizes_enum2log) / sizeof(int); i++)
		if ((icache_size_log2 == icache_sizes_enum2log[i]) &&
		    (icache_sizes_enum2log[i] >= 0))
			return (enum dx_cc_init_msg_icache_size)i;
	pr_err("Requested Icache size (%uKB) is invalid\n",
		    1 << (icache_size_log2 - 10));
	return DX_CC_INIT_MSG_ICACHE_SCR_INVALID_SIZE;
}
#endif

/**
 * get_cc_init_checksum() - Calculate CC_INIT message checksum
 *
 * @msg_p:	Pointer to the message buffer
 * @length:	Size of message in _bytes_
 */
static u32 get_cc_init_checksum(u32 *msg_p)
{
	int bytes_remain;
	u32 sum = 0;
	u16 *tdata = (u16 *)msg_p;

	for (bytes_remain = DX_CC_INIT_MSG_LENGTH * sizeof(u32);
	     bytes_remain > 1; bytes_remain -= 2)
		sum += *tdata++;
	/*  Add left-over byte, if any */
	if (bytes_remain > 0)
		sum += *(u8 *)tdata;
	/*  Fold 32-bit sum to 16 bits */
	while ((sum >> 16) != 0)
		sum = (sum & 0xFFFF) + (sum >> 16);
	return ~sum & 0xFFFF;
}

static void build_cc_init_msg(struct cc_init_ctx *init_ctx)
{
	u32 *const msg_p = init_ctx->msg_buf;
	struct sep_drvdata *drvdata = init_ctx->drvdata;
#ifndef VRL_KEY_INDEX
	/* Verify VRL key against this truncated hash value */
	u32 const vrl_key_hash[] = VRL_KEY_HASH;
#endif
#ifdef CACHE_IMAGE_NAME
	enum dx_cc_init_msg_icache_size icache_size_code;
#endif

	memset(msg_p, 0, DX_CC_INIT_MSG_LENGTH * sizeof(u32));
	msg_p[DX_CC_INIT_MSG_TOKEN_OFFSET] = DX_CC_INIT_HEAD_MSG_TOKEN;
	msg_p[DX_CC_INIT_MSG_LENGTH_OFFSET] = DX_CC_INIT_MSG_LENGTH;
	msg_p[DX_CC_INIT_MSG_OP_CODE_OFFSET] = DX_HOST_REQ_CC_INIT;
	msg_p[DX_CC_INIT_MSG_FLAGS_OFFSET] =
	    DX_CC_INIT_FLAGS_RESIDENT_ADDR_FLAG;
	msg_p[DX_CC_INIT_MSG_RESIDENT_IMAGE_OFFSET] =
	    init_ctx->resident_dma_addr;

#ifdef CACHE_IMAGE_NAME
	icache_size_code = icache_size_to_enum(drvdata->icache_size_log2);
	msg_p[DX_CC_INIT_MSG_FLAGS_OFFSET] |=
	    DX_CC_INIT_FLAGS_I_CACHE_ADDR_FLAG |
	    DX_CC_INIT_FLAGS_D_CACHE_EXIST_FLAG |
	    DX_CC_INIT_FLAGS_CACHE_ENC_FLAG | DX_CC_INIT_FLAGS_CACHE_COPY_FLAG;
#ifdef DX_CC_INIT_FLAGS_CACHE_SCRAMBLE_FLAG
	/* Enable scrambling if available */
	msg_p[DX_CC_INIT_MSG_FLAGS_OFFSET] |=
	    DX_CC_INIT_FLAGS_CACHE_SCRAMBLE_FLAG;
#endif
	msg_p[DX_CC_INIT_MSG_I_CACHE_IMAGE_OFFSET] = init_ctx->cache_dma_addr;
	msg_p[DX_CC_INIT_MSG_I_CACHE_DEST_OFFSET] =
	    page_to_phys(drvdata->icache_pages);
	msg_p[DX_CC_INIT_MSG_I_CACHE_SIZE_OFFSET] = icache_size_code;
	msg_p[DX_CC_INIT_MSG_D_CACHE_ADDR_OFFSET] =
	    page_to_phys(drvdata->dcache_pages);
	msg_p[DX_CC_INIT_MSG_D_CACHE_SIZE_OFFSET] =
	    1 << drvdata->dcache_size_log2;
#elif defined(SEP_BACKUP_BUF_SIZE)
	/* Declare SEP backup buffer resources */
	msg_p[DX_CC_INIT_MSG_HOST_BUFF_ADDR_OFFSET] =
	    virt_to_phys(drvdata->sep_backup_buf);
	msg_p[DX_CC_INIT_MSG_HOST_BUFF_SIZE_OFFSET] =
	    drvdata->sep_backup_buf_size;
#endif				/*CACHE_IMAGE_NAME */

	msg_p[DX_CC_INIT_MSG_VRL_ADDR_OFFSET] = init_ctx->vrl_dma_addr;
	/* Handle VRL key hash */
#ifdef VRL_KEY_INDEX
	msg_p[DX_CC_INIT_MSG_KEY_INDEX_OFFSET] = VRL_KEY_INDEX;
#else	/* Key should be validated against VRL_KEY_HASH */
	msg_p[DX_CC_INIT_MSG_KEY_INDEX_OFFSET] =
	    DX_CC_INIT_MSG_VRL_KEY_INDEX_INVALID;
	msg_p[DX_CC_INIT_MSG_KEY_HASH_0_OFFSET] = vrl_key_hash[0];
	msg_p[DX_CC_INIT_MSG_KEY_HASH_1_OFFSET] = vrl_key_hash[1];
	msg_p[DX_CC_INIT_MSG_KEY_HASH_2_OFFSET] = vrl_key_hash[2];
	msg_p[DX_CC_INIT_MSG_KEY_HASH_3_OFFSET] = vrl_key_hash[3];
#endif

	msg_p[DX_CC_INIT_MSG_CHECK_SUM_OFFSET] = get_cc_init_checksum(msg_p);

	dump_word_array("CC_INIT", msg_p, DX_CC_INIT_MSG_LENGTH);
}

/**
 * create_cc_init_ctx() - Create CC-INIT message and allocate associated
 *	resources (load FW images, etc.)
 *
 * @drvdata:		Device context
 *
 * Returns the allocated message context or NULL on failure.
 */
struct cc_init_ctx *create_cc_init_ctx(struct sep_drvdata *drvdata)
{
	struct cc_init_ctx *init_ctx;
	struct device *const mydev = drvdata->dev;
	int rc;

	init_ctx = kzalloc(sizeof(struct cc_init_ctx), GFP_KERNEL);
	if (unlikely(init_ctx == NULL)) {
		pr_err("Failed allocating CC-Init. context\n");
		rc = -ENOMEM;
		goto create_err;
	}
	init_ctx->drvdata = drvdata;
	rc = fetch_image(mydev, RESIDENT_IMAGE_NAME, &init_ctx->resident_p,
			 &init_ctx->resident_dma_addr,
			 &init_ctx->resident_size);
	if (unlikely(rc != 0))
		goto create_err;
#ifdef CACHE_IMAGE_NAME
	rc = fetch_image(mydev, CACHE_IMAGE_NAME, &init_ctx->cache_p,
			 &init_ctx->cache_dma_addr, &init_ctx->cache_size);
	if (unlikely(rc != 0))
		goto create_err;
#endif				/*CACHE_IMAGE_NAME */
	rc = fetch_image(mydev, VRL_IMAGE_NAME, &init_ctx->vrl_p,
			 &init_ctx->vrl_dma_addr, &init_ctx->vrl_size);
	if (unlikely(rc != 0))
		goto create_err;
	build_cc_init_msg(init_ctx);
	return init_ctx;

 create_err:

	if (init_ctx != NULL)
		destroy_cc_init_ctx(init_ctx);
	return NULL;
}

/**
 * sepinit_wait_for_cold_boot_finish() - Wait for SeP to reach cold-boot-finish
 *					state (i.e., ready for driver-init)
 * @drvdata:
 *
 * Returns int 0 for success, !0 on timeout while waiting for cold-boot-finish
 */
static int sepinit_wait_for_cold_boot_finish(struct sep_drvdata *drvdata)
{
	enum dx_sep_state cur_state;
	u32 cur_status;
	int rc = 0;

	cur_state =
	    dx_sep_wait_for_state(DX_SEP_STATE_DONE_COLD_BOOT |
				  DX_SEP_STATE_FATAL_ERROR,
				  COLD_BOOT_TIMEOUT_MSEC);
	if (cur_state != DX_SEP_STATE_DONE_COLD_BOOT) {
		rc = -EIO;
		cur_status =
		    READ_REGISTER(drvdata->cc_base + SEP_STATUS_GPR_OFFSET);
		pr_err(
			    "Failed waiting for DONE_COLD_BOOT from SeP (state=0x%08X status=0x%08X)\n",
			    cur_state, cur_status);
	}

	return rc;
}

/**
 * dispatch_cc_init_msg() - Push given CC_INIT message into SRAM and signal
 *	SeP to start cold boot sequence
 *
 * @drvdata:
 * @init_cc_msg_p:	A pointer to the message context
 */
static int dispatch_cc_init_msg(struct sep_drvdata *drvdata,
				struct cc_init_ctx *cc_init_ctx_p)
{
	int i;
	u32 is_data_ready;
	/*
	 * get the base address of the SRAM and add the offset
	 * for the CC_Init message
	 */
	const u32 msg_target_addr =
	    READ_REGISTER(DX_CC_REG_ADDR(drvdata->cc_base,
					 HOST, HOST_SEP_SRAM_THRESHOLD)) +
	    DX_CC_INIT_MSG_OFFSET_IN_SRAM;

	/* Initialize SRAM access address register for message location */
	WRITE_REGISTER(DX_CC_REG_ADDR(drvdata->cc_base, SRAM, SRAM_ADDR),
		       msg_target_addr);
	/* Write the message word by word to the SEP intenral offset */
	for (i = 0; i < sizeof(cc_init_ctx_p->msg_buf) / sizeof(u32);
		i++) {
		/* write data to SRAM */
		WRITE_REGISTER(DX_CC_REG_ADDR(drvdata->cc_base,
					      SRAM, SRAM_DATA),
			       cc_init_ctx_p->msg_buf[i]);
		/* wait for write complete */
		do {
			is_data_ready = 1 &
			    READ_REGISTER(DX_CC_REG_ADDR(drvdata->cc_base,
							 SRAM,
							 SRAM_DATA_READY));
		} while (!is_data_ready);
		/* TODO: Timeout in case something gets broken */
	}
	/* Signal SeP: Request CC_INIT */
	WRITE_REGISTER(drvdata->cc_base +
		       HOST_SEP_GPR_REG_OFFSET(DX_HOST_REQ_GPR_IDX),
		       DX_HOST_REQ_CC_INIT);
	return 0;
}

/**
 * sepinit_do_cc_init() - Initiate SeP cold boot sequence and wait for
 *	its completion.
 *
 * @drvdata:
 *
 * This function loads the CC firmware and dispatches a CC_INIT request message
 * Returns int 0 for success
 */
int sepinit_do_cc_init(struct sep_drvdata *drvdata)
{
	u32 cur_state;
	struct cc_init_ctx *cc_init_ctx_p;
	int rc;

	cur_state = dx_sep_wait_for_state(DX_SEP_STATE_START_SECURE_BOOT,
					  COLD_BOOT_TIMEOUT_MSEC);
	if (cur_state != DX_SEP_STATE_START_SECURE_BOOT) {
		pr_err("Bad SeP state = 0x%08X\n", cur_state);
		return -EIO;
	}
#ifdef __BIG_ENDIAN
	/* Enable byte swapping in DMA operations */
	WRITE_REGISTER(DX_CC_REG_ADDR(drvdata->cc_base, HOST, HOST_HOST_ENDIAN),
		       0xCCUL);
	/* TODO: Define value in device specific header files? */
#endif
	cc_init_ctx_p = create_cc_init_ctx(drvdata);
	if (likely(cc_init_ctx_p != NULL))
		rc = dispatch_cc_init_msg(drvdata, cc_init_ctx_p);
	else
		rc = -ENOMEM;
	if (likely(rc == 0))
		rc = sepinit_wait_for_cold_boot_finish(drvdata);
	if (cc_init_ctx_p != NULL)
		destroy_cc_init_ctx(cc_init_ctx_p);
	return rc;
}

/*** FW_INIT handlers ***/

#ifdef DEBUG
#define ENUM_CASE_RETURN_STR(enum_name)	(case enum_name: return #enum_name)

static const char *param2str(enum dx_fw_init_tlv_params param_type)
{
	switch (param_type) {
		ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_NULL);
		ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_FIRST);
		ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_LAST);
		ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_DISABLE_MODULES);
		ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_HOST_AXI_CONFIG);
		ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_HOST_DEF_APPLET_CONFIG);
		ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_NUM_OF_DESC_QS);
		ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_DESC_QS_ADDR);
		ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_DESC_QS_SIZE);
		ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_CTX_CACHE_PART);
		ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_SEP_REQUEST_PARAMS);
		ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_SEP_FREQ);
	default:
		return "(unknown param.)";
	}
}

static void dump_fwinit_params(struct sep_drvdata *drvdata,
			       u32 *fw_init_params_buf_p)
{
#define LINE_BUF_LEN 90		/* increased to hold values for 2 queues */
	const u32 last_tl_word = DX_TL_WORD(DX_FW_INIT_PARAM_LAST, 1);
	u32 tl_word;
	u16 type, len;
	u32 *cur_buf_p;
	unsigned int i = 0;
	char line_buf[LINE_BUF_LEN];
	unsigned int line_offset;

	pr_debug("Dx SeP fw_init params dump:\n");
	cur_buf_p = fw_init_params_buf_p;
	do {
		tl_word = le32_to_cpu(*cur_buf_p);
		cur_buf_p++;
		type = DX_TL_GET_TYPE(tl_word);
		len = DX_TL_GET_LENGTH(tl_word);

		if ((cur_buf_p + len - fw_init_params_buf_p) >
		    (FW_INIT_PARAMS_BUF_LEN / sizeof(u32))) {
			pr_err
			    ("LAST parameter not found up to buffer end\n");
			break;
		}

		line_offset = snprintf(line_buf, LINE_BUF_LEN,
				       "Type=0x%04X (%s), Length=%u , Val={",
				       type, param2str(type), len);
		for (i = 0; i < len; i++) {
			/*
			 * 11 is length of printed value
			 * (formatted with 0x%08X in the
			 * next call to snprintf
			 */
			if (line_offset + 11 >= LINE_BUF_LEN) {
				pr_debug("%s\n", line_buf);
				line_offset = 0;
			}
			line_offset += snprintf(line_buf + line_offset,
						LINE_BUF_LEN - line_offset,
						"0x%08X,",
						le32_to_cpu(*cur_buf_p));
			cur_buf_p++;
		}
		pr_debug("%s}\n", line_buf);
	} while (tl_word != last_tl_word);
}
#else
#define dump_fwinit_params(drvdata, fw_init_params_buf_p) do {} while (0)
#endif /*DEBUG*/
/**
 * add_fwinit_param() - Add TLV parameter for FW-init.
 * @tlv_buf:	 The base of the TLV parameters buffers
 * @idx_p:	 (in/out): Current tlv_buf word index
 * @checksum_p:	 (in/out): 32bit checksum for TLV array
 * @type:	 Parameter type
 * @length:	 Parameter length (size in words of "values")
 * @values:	 Values array ("length" values)
 *
 * Returns void
 */
static void add_fwinit_param(u32 *tlv_buf, u32 *idx_p,
			     u32 *checksum_p,
			     enum dx_fw_init_tlv_params type, u16 length,
			     const u32 *values)
{
	const u32 tl_word = DX_TL_WORD(type, length);
	int i;

#ifdef DEBUG
	/* Verify that we have enough space for LAST param. after this param. */
	if ((*idx_p + 1 + length + 2) > (FW_INIT_PARAMS_BUF_LEN / 4)) {
		pr_err("tlv_buf size limit reached!\n");
		SEP_DRIVER_BUG();
	}
#endif

	/* Add type-length word */
	tlv_buf[(*idx_p)++] = cpu_to_le32(tl_word);
	*checksum_p += tl_word;

	/* Add values */
	for (i = 0; i < length; i++) {
		/* Add value words if any. TL-word is counted as first... */
		tlv_buf[(*idx_p)++] = cpu_to_le32(values[i]);
		*checksum_p += values[i];
	}
}

/**
 * init_fwinit_param_list() - Initialize TLV parameters list
 * @tlv_buf:	The pointer to the TLV list array buffer
 * @idx_p:	The pointer to the variable that would maintain current
 *		position in the tlv_array
 * @checksum_p:	The pointer to the variable that would accumulate the
 *		TLV array checksum
 *
 * Returns void
 */
static void init_fwinit_param_list(u32 *tlv_buf, u32 *idx_p,
				   u32 *checksum_p)
{
	const u32 magic = DX_FW_INIT_PARAM_FIRST_MAGIC;
	/* Initialize index and checksum variables */
	*idx_p = 0;
	*checksum_p = 0;
	/* Start with FIRST_MAGIC parameter */
	add_fwinit_param(tlv_buf, idx_p, checksum_p,
			 DX_FW_INIT_PARAM_FIRST, 1, &magic);
}

/**
 * terminate_fwinit_param_list() - Terminate the TLV parameters list with
 *				LAST/checksum parameter
 * @tlv_buf:	The pointer to the TLV list array buffer
 * @idx_p:	The pointer to the variable that would maintain current
 *		position in the tlv_array
 * @checksum_p:	The pointer to the variable that would accumulate the
 *		TLV array checksum
 *
 * Returns void
 */
static void terminate_fwinit_param_list(u32 *tlv_buf, u32 *idx_p,
					u32 *checksum_p)
{
	const u32 tl_word = DX_TL_WORD(DX_FW_INIT_PARAM_LAST, 1);

	tlv_buf[(*idx_p)++] = cpu_to_le32(tl_word);
	*checksum_p += tl_word;	/* Last TL word is included in checksum */
	tlv_buf[(*idx_p)++] = cpu_to_le32(~(*checksum_p));
}

static int create_fwinit_command(struct sep_drvdata *drvdata,
				 u32 **fw_init_params_buf_pp,
				 dma_addr_t *fw_init_params_dma_p)
{
	u32 idx;
	u32 checksum = 0;
#ifdef SEP_FREQ_MHZ
	u32 sep_freq = SEP_FREQ_MHZ;
#endif
	dma_addr_t q_base_dma;
	unsigned long q_size;
	/* arrays for queue parameters values */
	u32 qs_base_dma[SEP_MAX_NUM_OF_DESC_Q];
	u32 qs_size[SEP_MAX_NUM_OF_DESC_Q];
	u32 qs_ctx_size[SEP_MAX_NUM_OF_DESC_Q];
	u32 qs_ctx_size_total;
	u32 qs_num = drvdata->num_of_desc_queues;
	u32 sep_request_params[DX_SEP_REQUEST_PARAM_MSG_LEN];
	int i;
	int rc;

	/* For klocwork, add extra check */
	if (qs_num > SEP_MAX_NUM_OF_DESC_Q) {
		pr_err("Max number of desc queues (%d) exceeded (%d)\n");
		return -EINVAL;
	}

	/* allocate coherent working buffer */
	*fw_init_params_buf_pp = dma_alloc_coherent(drvdata->dev,
						    FW_INIT_PARAMS_BUF_LEN,
						    fw_init_params_dma_p,
						    GFP_KERNEL);
	pr_debug("fw_init_params_dma=0x%08lX fw_init_params_va=0x%p\n",
		      (unsigned long)*fw_init_params_dma_p,
		      *fw_init_params_buf_pp);
	if (*fw_init_params_buf_pp == NULL) {
		pr_err("Unable to allocate coherent workspace buffer\n");
		return -ENOMEM;
	}

	init_fwinit_param_list(*fw_init_params_buf_pp, &idx, &checksum);

#ifdef SEP_FREQ_MHZ
	add_fwinit_param(*fw_init_params_buf_pp, &idx, &checksum,
			 DX_FW_INIT_PARAM_SEP_FREQ, 1, &sep_freq);
#endif

	/* No need to validate number of queues - already validated in
	 * sep_setup() */

	add_fwinit_param(*fw_init_params_buf_pp, &idx, &checksum,
			 DX_FW_INIT_PARAM_NUM_OF_DESC_QS, 1, &qs_num);

	/* Fetch per-queue information */
	qs_ctx_size_total = 0;
	for (i = 0; i < qs_num; i++) {
		desc_q_get_info4sep(drvdata->queue[i].desc_queue,
				    &q_base_dma, &q_size);
		/* Data is first fetched into q_base_dma and q_size because
		 * return value is of different type than u32 */
		qs_base_dma[i] = q_base_dma;
		qs_size[i] = q_size;
		qs_ctx_size[i] =
		    ctxmgr_sep_cache_get_size(drvdata->queue[i].sep_cache);
		if ((qs_base_dma[i] == 0) || (qs_size[i] == 0) ||
		    (qs_ctx_size[i] == 0)) {
			pr_err(
				    "Invalid queue %d resources: base=0x%08X size=%u ctx_cache_size=%u\n",
				    i, qs_base_dma[i], qs_size[i],
				    qs_ctx_size[i]);
			rc = -EINVAL;
			goto fwinit_error;
		}
		qs_ctx_size_total += qs_ctx_size[i];
	}

	if (qs_ctx_size_total > drvdata->num_of_sep_cache_entries) {
		pr_err("Too many context cache entries allocated(%u>%u)\n",
			    qs_ctx_size_total,
			    drvdata->num_of_sep_cache_entries);
		rc = -EINVAL;
		goto fwinit_error;
	}

	add_fwinit_param(*fw_init_params_buf_pp, &idx, &checksum,
			 DX_FW_INIT_PARAM_DESC_QS_ADDR, qs_num, qs_base_dma);
	add_fwinit_param(*fw_init_params_buf_pp, &idx, &checksum,
			 DX_FW_INIT_PARAM_DESC_QS_SIZE, qs_num, qs_size);
	add_fwinit_param(*fw_init_params_buf_pp, &idx, &checksum,
			 DX_FW_INIT_PARAM_CTX_CACHE_PART, qs_num, qs_ctx_size);

	/* Prepare sep request params */
	dx_sep_req_get_sep_init_params(sep_request_params);
	add_fwinit_param(*fw_init_params_buf_pp, &idx, &checksum,
			 DX_FW_INIT_PARAM_SEP_REQUEST_PARAMS,
			 DX_SEP_REQUEST_PARAM_MSG_LEN, sep_request_params);

	terminate_fwinit_param_list(*fw_init_params_buf_pp, &idx, &checksum);

	return 0;

 fwinit_error:
	dma_free_coherent(drvdata->dev, FW_INIT_PARAMS_BUF_LEN,
			  *fw_init_params_buf_pp, *fw_init_params_dma_p);
	return rc;
}

static void destroy_fwinit_command(struct sep_drvdata *drvdata,
				   u32 *fw_init_params_buf_p,
				   dma_addr_t fw_init_params_dma)
{
	/* release TLV parameters buffer */
	dma_free_coherent(drvdata->dev, FW_INIT_PARAMS_BUF_LEN,
			  fw_init_params_buf_p, fw_init_params_dma);
}

/**
 * sepinit_get_fw_props() - Get the FW properties (version, cache size, etc.)
 *				as given in the
 * @drvdata:	 Context where to fill retrieved data
 *
 * Get the FW properties (version, cache size, etc.) as given in the
 * respective GPRs.
 * This function should be called only after sepinit_wait_for_cold_boot_finish
 */
void sepinit_get_fw_props(struct sep_drvdata *drvdata)
{

	u32 init_fw_props;
	/* SeP ROM version */
	drvdata->rom_ver = READ_REGISTER(drvdata->cc_base +
					 SEP_HOST_GPR_REG_OFFSET
					 (DX_SEP_INIT_ROM_VER_GPR_IDX));
	drvdata->fw_ver =
	    READ_REGISTER(drvdata->cc_base +
			  SEP_HOST_GPR_REG_OFFSET(DX_SEP_INIT_FW_VER_GPR_IDX));
	mdelay(100);		/* TODO for kernel hang bug */
	init_fw_props = READ_REGISTER(drvdata->cc_base +
				      SEP_HOST_GPR_REG_OFFSET
				      (DX_SEP_INIT_FW_PROPS_GPR_IDX));

	drvdata->num_of_desc_queues =
	    BITFIELD_GET(init_fw_props,
			 DX_SEP_INIT_NUM_OF_QUEUES_BIT_OFFSET,
			 DX_SEP_INIT_NUM_OF_QUEUES_BIT_SIZE);
	drvdata->num_of_sep_cache_entries =
	    BITFIELD_GET(init_fw_props,
			 DX_SEP_INIT_CACHE_CTX_SIZE_BIT_OFFSET,
			 DX_SEP_INIT_CACHE_CTX_SIZE_BIT_SIZE);
	drvdata->mlli_table_size =
	    BITFIELD_GET(init_fw_props,
			 DX_SEP_INIT_MLLI_TBL_SIZE_BIT_OFFSET,
			 DX_SEP_INIT_MLLI_TBL_SIZE_BIT_SIZE);

	pr_info("ROM Ver.=0x%08X , FW Ver.=0x%08X\n"
		     "SEP queues=%u, Ctx.Cache#ent.=%u , MLLIsize=%lu B\n",
		     drvdata->rom_ver, drvdata->fw_ver,
		     drvdata->num_of_desc_queues,
		     drvdata->num_of_sep_cache_entries,
		     drvdata->mlli_table_size);
}

/**
 * sepinit_wait_for_fw_init_done() - Wait for FW initialization to complete
 * @drvdata:
 *
 * Wait for FW initialization to complete
 * This function should be invoked after sepinit_set_fw_init_params
 * Returns int
 */
static int sepinit_wait_for_fw_init_done(struct sep_drvdata *drvdata)
{
	enum dx_sep_state cur_state;
	u32 cur_status;
	int rc = 0;

	cur_state =
	    dx_sep_wait_for_state(DX_SEP_STATE_DONE_FW_INIT |
				  DX_SEP_STATE_FATAL_ERROR,
				  FW_INIT_TIMEOUT_MSEC);
	if (cur_state != DX_SEP_STATE_DONE_FW_INIT) {
		rc = -EIO;
		cur_status =
		    READ_REGISTER(drvdata->cc_base + SEP_STATUS_GPR_OFFSET);
		pr_err(
			    "Failed waiting for DONE_FW_INIT from SeP (state=0x%08X status=0x%08X)\n",
			    cur_state, cur_status);
	} else {
		pr_info("DONE_FW_INIT\n");
	}

	return rc;
}

/**
 * sepinit_do_fw_init() - Initialize SeP FW
 * @drvdata:
 *
 * Provide SeP FW with initialization parameters and wait for DONE_FW_INIT.
 *
 * Returns int 0 on success
 */
int sepinit_do_fw_init(struct sep_drvdata *drvdata)
{
	int rc;
	u32 *fw_init_params_buf_p;
	dma_addr_t fw_init_params_dma;

	rc = create_fwinit_command(drvdata,
				   &fw_init_params_buf_p, &fw_init_params_dma);
	if (rc != 0)
		return rc;
	dump_fwinit_params(drvdata, fw_init_params_buf_p);
	/* Write the physical address of the FW init parameters buffer */
	WRITE_REGISTER(drvdata->cc_base +
		       HOST_SEP_GPR_REG_OFFSET(DX_HOST_REQ_PARAM_GPR_IDX),
		       fw_init_params_dma);
	/* Initiate FW-init */
	WRITE_REGISTER(drvdata->cc_base +
		       HOST_SEP_GPR_REG_OFFSET(DX_HOST_REQ_GPR_IDX),
		       DX_HOST_REQ_FW_INIT);
	rc = sepinit_wait_for_fw_init_done(drvdata);
	destroy_fwinit_command(drvdata,
			       fw_init_params_buf_p, fw_init_params_dma);
	return rc;
}
