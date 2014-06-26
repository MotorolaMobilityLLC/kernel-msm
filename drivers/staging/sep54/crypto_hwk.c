/*
 *  Copyright(c) 2012-2013 Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program; if not, write to the Free Software Foundation, Inc., 59
 *  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#define pr_fmt(fmt) "sep_hwk: " fmt

#include <linux/module.h>
#include <linux/crypto.h>
#include <crypto/algapi.h>
#include <linux/highmem.h>

#include "dx_driver.h"
#include "sep_sysfs.h"
#include "sep_power.h"
#include "dx_sepapp_kapi.h"

#define HWK_APP_UUID "INTEL HWK 000001"
#define HWK_CMD_CRYPTO 8

struct hwk_context {
	struct sep_client_ctx *sctx;
	u32 sess_id, key_id;
};

static inline void hwk_pm_runtime_get(void)
{
#ifdef SEP_RUNTIME_PM
	dx_sep_pm_runtime_get();
#endif
}

static inline void hwk_pm_runtime_put(void)
{
#ifdef SEP_RUNTIME_PM
	dx_sep_pm_runtime_put();
#endif
}

static int hwk_ctx_init(struct crypto_tfm *tfm)
{
	struct hwk_context *hctx = crypto_tfm_ctx(tfm);
	u8 uuid[16] = HWK_APP_UUID;
	enum dxdi_sep_module ret;
	int rc;

	hwk_pm_runtime_get();

	hctx->sctx = dx_sepapp_context_alloc();
	if (!hctx->sctx) {
		rc = -ENOMEM;
		goto out;
	}

	pr_debug("%s: opening session\n", __func__);
	rc = dx_sepapp_session_open(hctx->sctx, uuid, 0, NULL, NULL,
				    &hctx->sess_id, &ret);
	pr_debug("%s: %d: %p %d\n", __func__, rc, hctx->sctx, hctx->sess_id);
	if (rc != 0)
		dx_sepapp_context_free(hctx->sctx);

out:
	hwk_pm_runtime_put();

	return rc;
}

static void hwk_ctx_cleanup(struct crypto_tfm *tfm)
{
	struct hwk_context *hctx = crypto_tfm_ctx(tfm);

	pr_debug("%s: %p %d\n", __func__, hctx->sctx, hctx->sess_id);
	if (dx_sepapp_session_close(hctx->sctx, hctx->sess_id))
		BUG();
	dx_sepapp_context_free(hctx->sctx);
	pr_debug("%s: session closed\n", __func__);
}

static int hwk_set_key(struct crypto_ablkcipher *tfm,
			     const u8 *key, unsigned int keylen)
{
	struct hwk_context *hctx = crypto_tfm_ctx(crypto_ablkcipher_tfm(tfm));

	hctx->key_id = *((u32 *)key);
	pr_debug("%s: key_id=%d\n", __func__, hctx->key_id);
	return 0;
}

#if defined(HWK_ST_DUMP_BUF) || defined(HWK_DUMP_BUF)
static void hwk_dump_buf(u8 *buf, const char *buf_name, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (i % 64 == 0)
			printk("\n%s: ", buf_name);
		printk("%02x", buf[i]);
	}
	printk("\n");
}
#endif

#ifdef HWK_DUMP_BUF
static void hwk_dump_sg(struct scatterlist *sg, const char *buf_name)
{
	u8 *buf = kmap(sg_page(sg));
	hwk_dump_buf(buf + sg->offset, buf_name, sg->length);
	kunmap(sg_page(sg));
}
#endif

static int hwk_process(struct ablkcipher_request *req, bool encrypt)
{
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	struct hwk_context *hctx = crypto_tfm_ctx(crypto_ablkcipher_tfm(tfm));
	struct dxdi_sepapp_kparams p;
	enum dxdi_sep_module ret_origin;
	struct scatterlist iv_sg;
	struct page *iv_page;
	int rc;

	iv_page = virt_to_page(req->info);
	sg_init_table(&iv_sg, 1);
	sg_set_page(&iv_sg, iv_page, SEP_AES_IV_SIZE,
		    (unsigned long)req->info % PAGE_SIZE);

#ifdef HWK_DUMP_BUF
	hwk_dump_buf(req->info, "iv", SEP_AES_IV_SIZE);
	hwk_dump_sg(&iv_sg, "iv");
	hwk_dump_sg(req->src, "src");
#endif

	memset(&p, 0, sizeof(p));

	p.params_types[0] = DXDI_SEPAPP_PARAM_VAL;
	p.params[0].val.data[0] = hctx->key_id;
	p.params[0].val.data[1] = req->nbytes | (encrypt << 16);
	p.params[0].val.copy_dir = DXDI_DATA_TO_DEVICE;

	p.params_types[1] = DXDI_SEPAPP_PARAM_MEMREF;
	p.params[1].kmemref.dma_direction = DXDI_DATA_TO_DEVICE;
	p.params[1].kmemref.sgl = &iv_sg;
	p.params[1].kmemref.nbytes = SEP_AES_IV_SIZE;

	p.params_types[2] = DXDI_SEPAPP_PARAM_MEMREF;
	p.params[2].kmemref.dma_direction = DXDI_DATA_TO_DEVICE;
	p.params[2].kmemref.sgl = req->src;
	p.params[2].kmemref.nbytes = req->nbytes;

	p.params_types[3] = DXDI_SEPAPP_PARAM_MEMREF;
	p.params[3].kmemref.dma_direction = DXDI_DATA_FROM_DEVICE;
	p.params[3].kmemref.sgl = req->dst;
	p.params[3].kmemref.nbytes = req->nbytes;

	pr_debug("%s: size=%d dir=%d\n", __func__, req->nbytes, encrypt);
	rc = dx_sepapp_command_invoke(hctx->sctx, hctx->sess_id,
				      HWK_CMD_CRYPTO, &p, &ret_origin);
	pr_debug("%s: done: %d\n", __func__, rc);

	if (rc != 0) {
		pr_err("%s: error invoking command %d: %x (ret_origin= %x)\n",
			__func__, HWK_CMD_CRYPTO, rc, ret_origin);
		return -EINVAL;
	}

#ifdef HWK_DUMP_BUF
	hwk_dump_sg(req->dst, "dst");
#endif

	return rc;
}

static int hwk_encrypt(struct ablkcipher_request *req)
{
	return hwk_process(req, true);
}

static int hwk_decrypt(struct ablkcipher_request *req)
{
	return hwk_process(req, false);
}

static struct crypto_alg hwk_alg = {
	.cra_priority = 0,
	.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
	.cra_ctxsize = sizeof(struct hwk_context),
	.cra_alignmask = 0, /* Cannot use this due to bug in kernel */
	.cra_type = &crypto_ablkcipher_type,
	.cra_module = THIS_MODULE,
	.cra_name = "cbchk(aes)",
	.cra_driver_name = MODULE_NAME "-aes-cbchk",
	.cra_blocksize = SEP_AES_BLOCK_SIZE,
	.cra_u = {
		.ablkcipher = {
			.min_keysize = SEP_AES_256_BIT_KEY_SIZE,
			.max_keysize = SEP_AES_256_BIT_KEY_SIZE,
			.ivsize = SEP_AES_IV_SIZE,
			.setkey = hwk_set_key,
			.encrypt = hwk_encrypt,
			.decrypt = hwk_decrypt,
		},
	},
	.cra_init = hwk_ctx_init,
	.cra_exit = hwk_ctx_cleanup
};

int hwk_init(void)
{
	int rc = crypto_register_alg(&hwk_alg);
	if (rc != 0)
		pr_err("failed to register %s\n", hwk_alg.cra_name);
	return rc;
}

void hwk_fini(void)
{
	crypto_unregister_alg(&hwk_alg);
}

#ifdef SEP_HWK_UNIT_TEST
enum hwk_self_test {
	HWK_ST_NOT_STARTED = 0,
	HWK_ST_RUNNING,
	HWK_ST_SUCCESS,
	HWK_ST_ERROR
};

static enum hwk_self_test hwk_st_status = HWK_ST_NOT_STARTED;
static const char * const hwk_st_strings[] = {
	"not started",
	"running",
	"success",
	"error"
};

ssize_t sys_hwk_st_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{

	return sprintf(buf, "%s\n", hwk_st_strings[hwk_st_status]);
}

struct hwk_st_op_result {
	struct completion completion;
	int rc;
};

static void hwk_st_op_complete(struct crypto_async_request *req, int rc)
{
	struct hwk_st_op_result *hr = req->data;

	if (rc == -EINPROGRESS)
		return;

	hr->rc = rc;
	complete(&hr->completion);
}

static int hwk_st_do_op(struct ablkcipher_request *req, struct page *src,
		struct page *dst, bool enc)
{
	struct scatterlist src_sg, dst_sg;
	struct hwk_st_op_result hr = { .rc = 0 };
	char iv[SEP_AES_IV_SIZE] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
				     12, 13, 14, 15 };
	int ret;

	init_completion(&hr.completion);
	ablkcipher_request_set_callback(req,
			CRYPTO_TFM_REQ_MAY_BACKLOG | CRYPTO_TFM_REQ_MAY_SLEEP,
			hwk_st_op_complete, &hr);

	sg_init_table(&src_sg, 1);
	sg_set_page(&src_sg, src, PAGE_SIZE, 0);
	sg_init_table(&dst_sg, 1);
	sg_set_page(&dst_sg, dst, PAGE_SIZE, 0);
	ablkcipher_request_set_crypt(req, &src_sg, &dst_sg, PAGE_SIZE, iv);

	pr_info("%s: submiting %s op..\n", __func__, enc ? "enc" : "dec");
	if (enc)
		ret = crypto_ablkcipher_encrypt(req);
	else
		ret = crypto_ablkcipher_decrypt(req);
	pr_info("%s: op submitted\n", __func__);
	if (ret == -EINPROGRESS || ret == -EBUSY) {
		wait_for_completion(&hr.completion);
		ret = hr.rc;
	}
	pr_info("%s: op completed\n", __func__);

	return ret;
}

ssize_t sys_hwk_st_start(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	struct ablkcipher_request *req;
	struct page *src, *enc, *dec;
	struct crypto_ablkcipher *acipher;
	char *tmp, *tmp2, *tmp3;
	int ret = -EINVAL, i;
	u32 hwk_id;

	if (hwk_st_status == HWK_ST_RUNNING)
		return count;

	hwk_st_status = HWK_ST_RUNNING;

	ret = kstrtouint(buf, 10, &hwk_id);
	if (ret) {
		pr_err("bad hardware key id: %d\n", ret);
		goto out;
	}

	ret = -ENOMEM;
	src = alloc_page(GFP_KERNEL);
	if (!src) {
		pr_err("failed to allocate src page\n");
		goto out;
	}
	enc = alloc_page(GFP_KERNEL);
	if (!enc) {
		pr_err("failed to allocate enc page\n");
		goto out_free_src;
	}
	dec = alloc_page(GFP_KERNEL);
	if (!dec) {
		pr_err("failed to allocate dec page\n");
		goto out_free_enc;
	}

	acipher = crypto_alloc_ablkcipher("cbchk(aes)", 0, 0);
	if (IS_ERR(acipher)) {
		pr_err("error allocating cipher: %ld\n", PTR_ERR(acipher));
		ret = -EINVAL;
		goto out_free_dec;
	}

	tmp = kmap(src);
	for (i = 0; i < PAGE_SIZE; i++)
		tmp[i] = i;
	kunmap(src);

	crypto_ablkcipher_set_flags(acipher, CRYPTO_TFM_REQ_WEAK_KEY);

	pr_debug("setting hardware key %d\n", hwk_id);
	ret = crypto_ablkcipher_setkey(acipher, (u8 *)&hwk_id, sizeof(hwk_id));
	if (ret) {
		pr_err("error setting hardware key: %d\n", ret);
		goto out_free_cipher;
	}

	req = ablkcipher_request_alloc(acipher, GFP_NOFS);
	if (!req) {
		ret = -EINVAL;
		pr_err("failed to allocate cipher request\n");
		goto out_free_cipher;
	}


	ret = hwk_st_do_op(req, src, enc, true);
	if (ret) {
		pr_err("encryption failed: %d\n", ret);
		goto out_free_req;
	}

	ret = hwk_st_do_op(req, enc, dec, false);
	if (ret) {
		pr_err("decryption failed: %d\n", ret);
		goto out_free_req;
	}

	tmp = kmap(src); tmp2 = kmap(enc); tmp3 = kmap(dec);
#ifdef HWK_ST_DUMP_BUF
	hwk_dump_buf(tmp, "src", PAGE_SIZE);
	hwk_dump_buf(tmp2, "enc", PAGE_SIZE);
	hwk_dump_buf(tmp3, "dec", PAGE_SIZE);
#endif
	for (i = 0; i < PAGE_SIZE; i++) {
		if (tmp[i] != tmp3[i]) {
			ret = -EINVAL;
			break;
		}
	}
	kunmap(src); kunmap(enc); kunmap(dec);

	if (ret)
		pr_err("dec != src\n");

out_free_req:
	ablkcipher_request_free(req);
out_free_cipher:
	crypto_free_ablkcipher(acipher);
out_free_dec:
	__free_pages(dec, 0);
out_free_enc:
	__free_pages(enc, 0);
out_free_src:
	__free_pages(src, 0);
out:
	if (ret)
		hwk_st_status = HWK_ST_ERROR;
	else
		hwk_st_status = HWK_ST_SUCCESS;
	return count;
}
#endif
