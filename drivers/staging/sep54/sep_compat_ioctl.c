/*
 * Copyright (C) 2013  Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/kernel.h>
#include <linux/compat.h>
#include <linux/compiler.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include "sep_compat_ioctl.h"
#include "dx_driver_abi.h"
#include "dx_dev_defs.h"
#include "sepapp.h"

typedef int sep_ioctl_compat_t(struct file *filp, unsigned int cmd,
			       unsigned long arg);

static int compat_sep_ioctl_get_ver_major(struct file *filp, unsigned int cmd,
					  unsigned long arg)
{
	return sep_ioctl(filp, cmd, arg);
}

static int compat_sep_ioctl_get_ver_minor(struct file *filp, unsigned int cmd,
					  unsigned long arg)
{
	return sep_ioctl(filp, cmd, arg);
}

static int compat_sep_ioctl_get_sym_cipher_ctx_size(struct file *filp,
						    unsigned int cmd,
						    unsigned long arg)
{
	return sep_ioctl(filp, cmd, arg);
}

static int compat_sep_ioctl_get_auth_enc_ctx_size(struct file *filp,
						  unsigned int cmd,
						  unsigned long arg)
{
	return sep_ioctl(filp, cmd, arg);
}

static int compat_sep_ioctl_get_mac_ctx_size(struct file *filp,
					     unsigned int cmd,
					     unsigned long arg)
{
	return sep_ioctl(filp, cmd, arg);
}
static int compat_sep_ioctl_get_hash_ctx_size(struct file *filp,
					      unsigned int cmd,
					      unsigned long arg)
{
	return sep_ioctl(filp, cmd, arg);
}

#pragma pack(push)
#pragma pack(4)
struct sym_cipher_init_params_32 {
	u32 context_buf;	/*[in] */
	struct dxdi_sym_cipher_props props;	/*[in] */
	u32 error_info;	/*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_sym_cipher_init(struct file *filp, unsigned int cmd,
					    unsigned long arg)
{
	struct sym_cipher_init_params_32 init_32;
	struct dxdi_sym_cipher_init_params __user *init_params;
	int ret;

	if (copy_from_user(&init_32, (void __user *)arg, sizeof(init_32)))
		return -EFAULT;

	init_params = compat_alloc_user_space(sizeof(*init_params));
	if (!access_ok(VERIFY_WRITE, init_params, sizeof(*init_params)))
		return -EFAULT;

	if (__put_user((void __user *)(unsigned long)init_32.context_buf,
		       &init_params->context_buf)
	    || copy_to_user(&init_params->props, &init_32.props,
			    sizeof(init_32.props)))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)init_params);

	if (__get_user(init_32.error_info, &init_params->error_info))
		return -EFAULT;

	if (put_user(init_32.error_info,
		       &((struct sym_cipher_init_params_32 *)arg)->error_info))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct auth_enc_init_params_32 {
	u32 context_buf; /*[in] */
	struct dxdi_auth_enc_props props; /*[in] */
	u32 error_info; /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_auth_enc_init(struct file *filp, unsigned int cmd,
					  unsigned long arg)
{
	struct auth_enc_init_params_32 up32;
	struct dxdi_auth_enc_init_params __user *init_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	init_params = compat_alloc_user_space(sizeof(*init_params));
	if (!access_ok(VERIFY_WRITE, init_params, sizeof(*init_params)))
		return -EFAULT;

	if (__put_user((void __user *)(unsigned long)up32.context_buf,
		       &init_params->context_buf)
	    || copy_to_user(&init_params->props, &up32.props,
			    sizeof(up32.props)))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)init_params);

	if (__get_user(up32.error_info, &init_params->error_info))
		return -EFAULT;

	if (put_user(up32.error_info,
		       &((struct auth_enc_init_params_32 *)arg)->error_info))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct mac_init_params_32 {
	u32 context_buf; /*[in] */
	struct dxdi_mac_props props;  /*[in] */
	u32 error_info; /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_mac_init(struct file *filp, unsigned int cmd,
				     unsigned long arg)
{
	struct mac_init_params_32 up32;
	struct dxdi_mac_init_params __user *init_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	init_params = compat_alloc_user_space(sizeof(*init_params));
	if (!access_ok(VERIFY_WRITE, init_params, sizeof(*init_params)))
		return -EFAULT;

	if (__put_user((void __user *)(unsigned long)up32.context_buf,
		       &init_params->context_buf)
	    || copy_to_user(&init_params->props, &up32.props,
			    sizeof(up32.props)))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)init_params);

	if (__get_user(up32.error_info, &init_params->error_info))
		return -EFAULT;

	if (put_user(up32.error_info,
		       &((struct mac_init_params_32 *)arg)->error_info))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct hash_init_params_32 {
	u32 context_buf; /*[in] */
	enum dxdi_hash_type hash_type;  /*[in] */
	u32 error_info; /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_hash_init(struct file *filp, unsigned int cmd,
				      unsigned long arg)
{
	struct hash_init_params_32 up32;
	struct dxdi_hash_init_params __user *init_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	init_params = compat_alloc_user_space(sizeof(*init_params));
	if (!access_ok(VERIFY_WRITE, init_params, sizeof(*init_params)))
		return -EFAULT;

	if (__put_user((void __user *)(unsigned long)up32.context_buf,
		       &init_params->context_buf)
	    || __put_user(up32.hash_type, &init_params->hash_type))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)init_params);

	if (__get_user(up32.error_info, &init_params->error_info))
		return -EFAULT;

	if (put_user(up32.error_info,
		       &((struct hash_init_params_32 *)arg)->error_info))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct process_dblk_params_32 {
	u32 context_buf; /*[in] */
	u32 data_in;  /*[in] */
	u32 data_out; /*[in] */
	enum dxdi_data_block_type data_block_type;  /*[in] */
	u32 data_in_size; /*[in] */
	u32 error_info; /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_proc_dblk(struct file *filp, unsigned int cmd,
				      unsigned long arg)
{
	struct process_dblk_params_32 up32;
	struct dxdi_process_dblk_params __user *dblk_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	dblk_params = compat_alloc_user_space(sizeof(*dblk_params));
	if (!access_ok(VERIFY_WRITE, dblk_params, sizeof(*dblk_params)))
		return -EFAULT;

	if (__put_user((void __user *)(unsigned long)up32.context_buf,
		       &dblk_params->context_buf)
	    || __put_user((void __user *)(unsigned long)up32.data_in,
			  &dblk_params->data_in)
	    || __put_user((void __user *)(unsigned long)up32.data_out,
			  &dblk_params->data_out)
	    || __put_user(up32.data_block_type, &dblk_params->data_block_type)
	    || __put_user(up32.data_in_size, &dblk_params->data_in_size))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)dblk_params);

	if (__get_user(up32.error_info, &dblk_params->error_info))
		return -EFAULT;

	if (put_user(up32.error_info,
		       &((struct process_dblk_params_32 *)arg)->error_info))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct fin_process_params_32 {
	u32 context_buf; /*[in] */
	u32 data_in;  /*[in] */
	u32 data_out; /*[in] */
	u32 data_in_size; /*[in] (octets) */
	u8 digest_or_mac[DXDI_DIGEST_SIZE_MAX]; /*[out] */
	u8 digest_or_mac_size;  /*[out] (octets) */
	u32 error_info; /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_fin_proc(struct file *filp, unsigned int cmd,
				     unsigned long arg)
{
	struct fin_process_params_32 up32;
	struct dxdi_fin_process_params __user *fin_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	fin_params = compat_alloc_user_space(sizeof(*fin_params));
	if (!access_ok(VERIFY_WRITE, fin_params, sizeof(*fin_params)))
		return -EFAULT;

	if (__put_user((void __user *)(unsigned long)up32.context_buf,
		       &fin_params->context_buf)
	    || __put_user((void __user *)(unsigned long)up32.data_in,
			  &fin_params->data_in)
	    || __put_user((void __user *)(unsigned long)up32.data_out,
			  &fin_params->data_out)
	    || __put_user(up32.data_in_size, &fin_params->data_in_size))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)fin_params);

	if (copy_from_user(up32.digest_or_mac, fin_params->digest_or_mac,
			   DXDI_DIGEST_SIZE_MAX)
	    || __get_user(up32.digest_or_mac_size,
			  &fin_params->digest_or_mac_size)
	    || __get_user(up32.error_info, &fin_params->error_info))
		return -EFAULT;

	if (copy_to_user((void __user *)arg, &up32, sizeof(up32)))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct combined_init_params_32 {
	struct dxdi_combined_props props; /*[in] */
	u32 error_info; /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_combined_init(struct file *filp, unsigned int cmd,
					  unsigned long arg)
{
	struct combined_init_params_32 up32;
	struct dxdi_combined_init_params __user *init_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	init_params = compat_alloc_user_space(sizeof(*init_params));
	if (!access_ok(VERIFY_WRITE, init_params, sizeof(*init_params)))
		return -EFAULT;

	if (copy_to_user(&init_params->props, &up32.props, sizeof(up32.props)))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)init_params);

	if (__get_user(up32.error_info, &init_params->error_info))
		return -EFAULT;

	if (put_user(up32.error_info,
		       &((struct combined_init_params_32 *)arg)->error_info))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct combined_proc_dblk_params_32 {
	struct dxdi_combined_props props; /*[in] */
	u32 data_in;  /*[in] */
	u32 data_out; /*[out] */
	u32 data_in_size; /*[in] (octets) */
	u32 error_info; /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_combined_proc_dblk(struct file *filp,
					       unsigned int cmd,
					       unsigned long arg)
{
	struct combined_proc_dblk_params_32 up32;
	struct dxdi_combined_proc_dblk_params __user *blk_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	blk_params = compat_alloc_user_space(sizeof(*blk_params));
	if (!access_ok(VERIFY_WRITE, blk_params, sizeof(*blk_params)))
		return -EFAULT;

	if (copy_to_user(&blk_params->props, &up32.props,
			 sizeof(up32.props))
	    || __put_user((void __user *)(unsigned long)up32.data_in,
			  &blk_params->data_in)
	    || __put_user((void __user *)(unsigned long)up32.data_out,
			  &blk_params->data_out)
	    || __put_user(up32.data_in_size, &blk_params->data_in_size))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)blk_params);

	if (__get_user(up32.error_info, &blk_params->error_info))
		return -EFAULT;

	if (put_user(up32.error_info,
		       &((struct combined_proc_dblk_params_32 *)arg)->error_info))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct combined_proc_params_32 {
	struct dxdi_combined_props props; /*[in] */
	u32 data_in;  /*[in] */
	u32 data_out; /*[out] */
	u32 data_in_size; /*[in] (octets) */
	u8 auth_data[DXDI_DIGEST_SIZE_MAX]; /*[out] */
	u8 auth_data_size;  /*[out] (octets) */
	u32 error_info; /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_combined_fin_proc(struct file *filp,
					      unsigned int cmd,
					      unsigned long arg)
{
	struct combined_proc_params_32 up32;
	struct dxdi_combined_proc_params __user *user_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	user_params = compat_alloc_user_space(sizeof(*user_params));
	if (!access_ok(VERIFY_WRITE, user_params, sizeof(*user_params)))
		return -EFAULT;

	if (copy_to_user(&user_params->props, &up32.props,
			 sizeof(up32.props))
	    || __put_user((void __user *)(unsigned long)up32.data_in,
			  &user_params->data_in)
	    || __put_user((void __user *)(unsigned long)up32.data_out,
			  &user_params->data_out)
	    || __put_user(up32.data_in_size, &user_params->data_in_size))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)user_params);

	if (copy_from_user(up32.auth_data, user_params->auth_data,
			   DXDI_DIGEST_SIZE_MAX)
	    || __get_user(up32.auth_data_size,
			  &user_params->auth_data_size)
	    || __get_user(up32.error_info, &user_params->error_info))
		return -EFAULT;

	if (copy_to_user((void __user *)arg, &up32, sizeof(up32)))
		return -EFAULT;

	return ret;
}

static int compat_sep_ioctl_combined_proc(struct file *filp,
					  unsigned int cmd, unsigned long arg)
{
	return compat_sep_ioctl_combined_fin_proc(filp, cmd, arg);
}

#pragma pack(push)
#pragma pack(4)
struct sym_cipher_proc_params_32 {
	u32 context_buf; /*[in] */
	struct dxdi_sym_cipher_props props; /*[in] */
	u32 data_in;  /*[in] */
	u32 data_out; /*[in] */
	u32 data_in_size; /*[in] (octets) */
	u32 error_info; /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_sym_cipher_proc(struct file *filp, unsigned int cmd,
					    unsigned long arg)
{
	struct sym_cipher_proc_params_32 up32;
	struct dxdi_sym_cipher_proc_params __user *user_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	user_params = compat_alloc_user_space(sizeof(*user_params));
	if (!access_ok(VERIFY_WRITE, user_params, sizeof(*user_params)))
		return -EFAULT;

	if (__put_user((void __user *)(unsigned long)up32.context_buf,
		       &user_params->context_buf)
	    || copy_to_user(&user_params->props, &up32.props,
			    sizeof(up32.props))
	    || __put_user((void __user *)(unsigned long)up32.data_in,
			  &user_params->data_in)
	    || __put_user((void __user *)(unsigned long)up32.data_out,
			  &user_params->data_out)
	    || __put_user(up32.data_in_size, &user_params->data_in_size))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)user_params);

	if (__get_user(up32.error_info, &user_params->error_info))
		return -EFAULT;

	if (put_user(up32.error_info,
		       &((struct sym_cipher_proc_params_32 *)arg)->error_info))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct auth_enc_proc_params_32 {
	u32 context_buf; /*[in] */
	struct dxdi_auth_enc_props props; /*[in] */
	u32 adata;    /*[in] */
	u32 text_data;  /*[in] */
	u32 data_out; /*[in] */
	u8 tag[DXDI_DIGEST_SIZE_MAX]; /*[out] */
	u32 error_info; /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_auth_enc_proc(struct file *filp, unsigned int cmd,
					  unsigned long arg)
{
	struct auth_enc_proc_params_32 up32;
	struct dxdi_auth_enc_proc_params __user *user_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	user_params = compat_alloc_user_space(sizeof(*user_params));
	if (!access_ok(VERIFY_WRITE, user_params, sizeof(*user_params)))
		return -EFAULT;

	if (__put_user((void __user *)(unsigned long)up32.context_buf,
		       &user_params->context_buf)
	    || copy_to_user(&user_params->props, &up32.props,
			    sizeof(up32.props))
	    || __put_user((void __user *)(unsigned long)up32.adata,
			  &user_params->adata)
	    || __put_user((void __user *)(unsigned long)up32.text_data,
			  &user_params->text_data)
	    || __put_user((void __user *)(unsigned long)up32.data_out,
			  &user_params->data_out))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)user_params);

	if (copy_from_user(up32.tag, user_params->tag,
			   DXDI_DIGEST_SIZE_MAX)
	    || __get_user(up32.error_info, &user_params->error_info))
		return -EFAULT;

	if (copy_to_user((void __user *)arg, &up32, sizeof(up32)))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct mac_proc_params_32 {
	u32 context_buf; /*[in] */
	struct dxdi_mac_props props;  /*[in] */
	u32 data_in;  /*[in] */
	u32 data_in_size; /*[in] (octets) */
	u8 mac[DXDI_DIGEST_SIZE_MAX]; /*[out] */
	u8 mac_size;  /*[out] (octets) */
	u32 error_info; /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_mac_proc(struct file *filp, unsigned int cmd,
				     unsigned long arg)
{
	struct mac_proc_params_32 up32;
	struct dxdi_mac_proc_params __user *user_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	user_params = compat_alloc_user_space(sizeof(*user_params));
	if (!access_ok(VERIFY_WRITE, user_params, sizeof(*user_params)))
		return -EFAULT;

	if (__put_user((void __user *)(unsigned long)up32.context_buf,
		       &user_params->context_buf)
	    || copy_to_user(&user_params->props, &up32.props,
			 sizeof(up32.props))
	    || __put_user((void __user *)(unsigned long)up32.data_in,
			  &user_params->data_in)
	    || __put_user(up32.data_in_size, &user_params->data_in_size))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)user_params);

	if (copy_from_user(up32.mac, user_params->mac, DXDI_DIGEST_SIZE_MAX)
	    || __get_user(up32.mac_size, &user_params->mac_size)
	    || __get_user(up32.error_info, &user_params->error_info))
		return -EFAULT;

	if (copy_to_user((void __user *)arg, &up32, sizeof(up32)))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct hash_proc_params_32 {
	u32 context_buf; /*[in] */
	enum dxdi_hash_type hash_type;  /*[in] */
	u32 data_in;  /*[in] */
	u32 data_in_size; /*[in] (octets) */
	u8 digest[DXDI_DIGEST_SIZE_MAX];  /*[out] */
	u8 digest_size; /*[out] (octets) */
	u32 error_info; /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_hash_proc(struct file *filp, unsigned int cmd,
				      unsigned long arg)
{
	struct hash_proc_params_32 up32;
	struct dxdi_hash_proc_params __user *user_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	user_params = compat_alloc_user_space(sizeof(*user_params));
	if (!access_ok(VERIFY_WRITE, user_params, sizeof(*user_params)))
		return -EFAULT;

	if (__put_user((void __user *)(unsigned long)up32.context_buf,
		       &user_params->context_buf)
	    || __put_user(up32.hash_type, &user_params->hash_type)
	    || __put_user((void __user *)(unsigned long)up32.data_in,
			  &user_params->data_in)
	    || __put_user(up32.data_in_size, &user_params->data_in_size))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)user_params);

	if (copy_from_user(up32.digest, user_params->digest,
			   DXDI_DIGEST_SIZE_MAX)
	    || __get_user(up32.digest_size, &user_params->digest_size)
	    || __get_user(up32.error_info, &user_params->error_info))
		return -EFAULT;

	if (copy_to_user((void __user *)arg, &up32, sizeof(up32)))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct sep_rpc_params_32 {
	u16 agent_id; /*[in] */
	u16 func_id;  /*[in] */
	struct dxdi_memref mem_refs[SEP_RPC_MAX_MEMREF_PER_FUNC]; /*[in] */
	u32 rpc_params_size;  /*[in] */
	u32 rpc_params; /*[in] */
	/* rpc_params to be copied into kernel DMA buffer */
	enum seprpc_retcode error_info; /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_sep_rpc(struct file *filp, unsigned int cmd,
				    unsigned long arg)
{
	struct sep_rpc_params_32 up32;
	struct dxdi_sep_rpc_params __user *user_params;

	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	user_params = compat_alloc_user_space(sizeof(*user_params));
	if (!access_ok(VERIFY_WRITE, user_params, sizeof(*user_params)))
		return -EFAULT;

	if (__put_user(up32.agent_id, &user_params->agent_id)
	    || __put_user(up32.func_id, &user_params->func_id)
	    || copy_to_user(user_params->mem_refs, up32.mem_refs,
			    sizeof(struct dxdi_memref)
			    * SEP_RPC_MAX_MEMREF_PER_FUNC)
	    || __put_user(up32.rpc_params_size, &user_params->rpc_params_size)
	    || __put_user((void __user *)(unsigned long)up32.rpc_params,
			 &user_params->rpc_params))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)user_params);

	if (__get_user(up32.error_info, &user_params->error_info))
		return -EFAULT;

	if (put_user(up32.error_info,
		       &((struct sep_rpc_params_32 *)arg)->error_info))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct register_mem4dma_params_32 {
	struct dxdi_memref memref;  /*[in] */
	int memref_id;  /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_register_mem4dma(struct file *filp,
					     unsigned int cmd,
					     unsigned long arg)
{
	struct register_mem4dma_params_32 up32;
	struct dxdi_register_mem4dma_params __user *user_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	user_params = compat_alloc_user_space(sizeof(*user_params));
	if (!access_ok(VERIFY_WRITE, user_params, sizeof(*user_params)))
		return -EFAULT;

	if (copy_to_user(&user_params->memref, &up32.memref,
		sizeof(struct dxdi_memref)))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)user_params);
	if (ret)
		return ret;

	if (__get_user(up32.memref_id, &user_params->memref_id))
		return -EFAULT;

	if (copy_to_user((void __user *)arg, &up32, sizeof(up32)))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct alloc_mem4dma_params_32 {
	u32 size; /*[in] */
	int memref_id;  /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_alloc_mem4dma(struct file *filp, unsigned int cmd,
					  unsigned long arg)
{
	struct alloc_mem4dma_params_32 up32;
	struct dxdi_alloc_mem4dma_params __user *user_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	user_params = compat_alloc_user_space(sizeof(*user_params));
	if (!access_ok(VERIFY_WRITE, user_params, sizeof(*user_params)))
		return -EFAULT;

	if (__put_user(up32.size, &user_params->size))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)user_params);
	if (ret)
		return ret;

	if (__get_user(up32.memref_id, &user_params->memref_id))
		return -EFAULT;

	if (copy_to_user((void __user *)arg, &up32, sizeof(up32)))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct free_mem4dma_params_32 {
	int memref_id;  /*[in] */
};
#pragma pack(pop)

static int compat_sep_ioctl_free_mem4dma(struct file *filp, unsigned int cmd,
					 unsigned long arg)
{
	return sep_ioctl(filp, cmd, arg);
}

#pragma pack(push)
#pragma pack(4)
struct aes_iv_params_32 {
	u32 context_buf; /*[in] */
	u8 iv_ptr[DXDI_AES_IV_SIZE];  /*[in]/[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_set_iv(struct file *filp, unsigned int cmd,
				   unsigned long arg)
{
	struct aes_iv_params_32 up32;
	struct dxdi_aes_iv_params *user_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	user_params = compat_alloc_user_space(sizeof(*user_params));
	if (!access_ok(VERIFY_WRITE, user_params, sizeof(*user_params)))
		return -EFAULT;

	if (__put_user((void __user *)(unsigned long)up32.context_buf,
		      &user_params->context_buf)
	    || __copy_to_user(user_params->iv_ptr, up32.iv_ptr,
			      DXDI_AES_IV_SIZE))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)user_params);

	return ret;
}

static int compat_sep_ioctl_get_iv(struct file *filp, unsigned int cmd,
				   unsigned long arg)
{
	struct aes_iv_params_32 up32;
	struct dxdi_aes_iv_params *user_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	user_params = compat_alloc_user_space(sizeof(*user_params));
	if (!access_ok(VERIFY_WRITE, user_params, sizeof(*user_params)))
		return -EFAULT;

	if (__put_user((void __user *)(unsigned long)up32.context_buf,
		       &user_params->context_buf)
	    || __copy_to_user(&user_params->iv_ptr, &up32.iv_ptr,
			      DXDI_AES_IV_SIZE))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)user_params);

	if (ret)
		return ret;

	if (__copy_from_user(up32.iv_ptr, user_params->iv_ptr,
			     DXDI_AES_IV_SIZE))
		return -EFAULT;

	if (copy_to_user((void __user *)arg, &up32, sizeof(up32)))
		return -EFAULT;

	return ret;
}

#pragma pack(push)
#pragma pack(4)
struct sepapp_session_open_params_32 {
	u8 app_uuid[DXDI_SEPAPP_UUID_SIZE]; /*[in] */
	u32 auth_method;  /*[in] */
	u32 auth_data[3]; /*[in] */
	struct dxdi_sepapp_params app_auth_data;  /*[in/out] */
	int session_id; /*[out] */
	enum dxdi_sep_module sep_ret_origin;  /*[out] */
	u32 error_info; /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_sepapp_session_open(struct file *filp,
						unsigned int cmd,
						unsigned long arg)
{
	struct sepapp_session_open_params_32 up32;
	struct dxdi_sepapp_session_open_params __user *user_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	user_params = compat_alloc_user_space(sizeof(*user_params));
	if (!access_ok(VERIFY_WRITE, user_params, sizeof(*user_params)))
		return -EFAULT;

	if (copy_to_user(&user_params->app_uuid[0], &up32.app_uuid[0],
			 DXDI_SEPAPP_UUID_SIZE)
	    || __put_user(up32.auth_method, &user_params->auth_method)
	    || copy_to_user(user_params->auth_data, &up32.auth_data,
			    3 * sizeof(u32))
	    || copy_to_user(&user_params->app_auth_data, &up32.app_auth_data,
			    sizeof(struct dxdi_sepapp_params)))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)user_params);
	if (ret)
		return ret;

	if (__copy_from_user(&up32.app_auth_data, &user_params->app_auth_data,
			     sizeof(struct dxdi_sepapp_params))
	    || __get_user(up32.session_id, &user_params->session_id)
	    || __get_user(up32.sep_ret_origin, &user_params->sep_ret_origin)
	    || __get_user(up32.error_info, &user_params->error_info))
		return -EFAULT;

	if (copy_to_user((void __user *)arg, &up32, sizeof(up32)))
		return -EFAULT;

	return 0;
}

#pragma pack(push)
#pragma pack(4)
struct sepapp_session_close_params_32 {
	int session_id; /*[in] */
};
#pragma pack(pop)

static int compat_sep_ioctl_sepapp_session_close(struct file *filp,
						 unsigned int cmd,
						 unsigned long arg)
{
	return sep_ioctl(filp, cmd, arg);
}

#pragma pack(push)
#pragma pack(4)
struct sepapp_command_invoke_params_32 {
	u8 app_uuid[DXDI_SEPAPP_UUID_SIZE]; /*[in] */
	int session_id; /*[in] */
	u32 command_id; /*[in] */
	struct dxdi_sepapp_params command_params; /*[in/out] */
	enum dxdi_sep_module sep_ret_origin;  /*[out] */
	u32 error_info; /*[out] */
};
#pragma pack(pop)

static int compat_sep_ioctl_sepapp_command_invoke(struct file *filp,
						  unsigned int cmd,
						  unsigned long arg)
{
	struct sepapp_command_invoke_params_32 up32;
	struct dxdi_sepapp_command_invoke_params __user *user_params;
	int ret;

	if (copy_from_user(&up32, (void __user *)arg, sizeof(up32)))
		return -EFAULT;

	user_params = compat_alloc_user_space(sizeof(*user_params));
	if (!access_ok(VERIFY_WRITE, user_params, sizeof(*user_params)))
		return -EFAULT;

	if (copy_to_user(&user_params->app_uuid,
			 &up32.app_uuid,
			 DXDI_SEPAPP_UUID_SIZE)
	    || __put_user(up32.session_id, &user_params->session_id)
	    || __put_user(up32.command_id, &user_params->command_id)
	    || copy_to_user(&user_params->command_params,
			    &up32.command_params,
			    sizeof(struct dxdi_sepapp_params)))
		return -EFAULT;

	ret = sep_ioctl(filp, cmd, (unsigned long)user_params);
	if (ret)
		return ret;


	if (__get_user(up32.sep_ret_origin, &user_params->sep_ret_origin)
	    || __copy_from_user(&up32.command_params,
				&user_params->command_params,
				sizeof(struct dxdi_sepapp_params))
	    || __get_user(up32.error_info, &user_params->error_info))
		return -EFAULT;

	if (copy_to_user((void __user *)arg, &up32, sizeof(up32)))
		return -EFAULT;

	return ret;
}

static sep_ioctl_compat_t *sep_compat_ioctls[] = {
	/* Version info. commands */
	[DXDI_IOC_NR_GET_VER_MAJOR] = compat_sep_ioctl_get_ver_major,
	[DXDI_IOC_NR_GET_VER_MINOR] = compat_sep_ioctl_get_ver_minor,
	/* Context size queries */
	[DXDI_IOC_NR_GET_SYMCIPHER_CTX_SIZE] = compat_sep_ioctl_get_sym_cipher_ctx_size,
	[DXDI_IOC_NR_GET_AUTH_ENC_CTX_SIZE] = compat_sep_ioctl_get_auth_enc_ctx_size,
	[DXDI_IOC_NR_GET_MAC_CTX_SIZE] = compat_sep_ioctl_get_mac_ctx_size,
	[DXDI_IOC_NR_GET_HASH_CTX_SIZE] = compat_sep_ioctl_get_hash_ctx_size,
	/* Init context commands */
	[DXDI_IOC_NR_SYMCIPHER_INIT] = compat_sep_ioctl_sym_cipher_init,
	[DXDI_IOC_NR_AUTH_ENC_INIT] = compat_sep_ioctl_auth_enc_init,
	[DXDI_IOC_NR_MAC_INIT] = compat_sep_ioctl_mac_init,
	[DXDI_IOC_NR_HASH_INIT] = compat_sep_ioctl_hash_init,
	/* Processing commands */
	[DXDI_IOC_NR_PROC_DBLK] = compat_sep_ioctl_proc_dblk,
	[DXDI_IOC_NR_FIN_PROC] = compat_sep_ioctl_fin_proc,
	/* "Integrated" processing operations */
	[DXDI_IOC_NR_SYMCIPHER_PROC] = compat_sep_ioctl_sym_cipher_proc,
	[DXDI_IOC_NR_AUTH_ENC_PROC] = compat_sep_ioctl_auth_enc_proc,
	[DXDI_IOC_NR_MAC_PROC] = compat_sep_ioctl_mac_proc,
	[DXDI_IOC_NR_HASH_PROC] = compat_sep_ioctl_hash_proc,
	/* SeP RPC */
	[DXDI_IOC_NR_SEP_RPC] = compat_sep_ioctl_sep_rpc,
	/* Memory registration */
	[DXDI_IOC_NR_REGISTER_MEM4DMA] = compat_sep_ioctl_register_mem4dma,
	[DXDI_IOC_NR_ALLOC_MEM4DMA] = compat_sep_ioctl_alloc_mem4dma,
	[DXDI_IOC_NR_FREE_MEM4DMA] = compat_sep_ioctl_free_mem4dma,
	/* SeP Applets API */
	[DXDI_IOC_NR_SEPAPP_SESSION_OPEN] = compat_sep_ioctl_sepapp_session_open,
	[DXDI_IOC_NR_SEPAPP_SESSION_CLOSE] = compat_sep_ioctl_sepapp_session_close,
	[DXDI_IOC_NR_SEPAPP_COMMAND_INVOKE] = compat_sep_ioctl_sepapp_command_invoke,
	/* Combined mode */
	[DXDI_IOC_NR_COMBINED_INIT] = compat_sep_ioctl_combined_init,
	[DXDI_IOC_NR_COMBINED_PROC_DBLK] = compat_sep_ioctl_combined_proc_dblk,
	[DXDI_IOC_NR_COMBINED_PROC_FIN] = compat_sep_ioctl_combined_fin_proc,
	[DXDI_IOC_NR_COMBINED_PROC] = compat_sep_ioctl_combined_proc,

	/* AES IV set/get API */
	[DXDI_IOC_NR_SET_IV] = compat_sep_ioctl_set_iv,
	[DXDI_IOC_NR_GET_IV] = compat_sep_ioctl_get_iv,
};

long sep_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned int nr = _IOC_NR(cmd);
	sep_ioctl_compat_t *callback = NULL;
	int ret;

	pr_debug("Calling the IOCTL compat %d\n", nr);

	if (nr < ARRAY_SIZE(sep_compat_ioctls))
		callback = sep_compat_ioctls[nr];

	if (callback != NULL)
		ret = (*callback) (filp, cmd, arg);
	else
		ret = sep_ioctl(filp, cmd, arg);

	return ret;
}
