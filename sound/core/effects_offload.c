/*
 *  effect_offload.c - effects offload core
 *
 *  Copyright (C) 2013 Intel Corporation
 *  Authors:	Lakshmi N Vinnakota <lakshmi.n.vinnakota@intel.com>
 *		Vinod Koul <vinod.koul@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
#define FORMAT(fmt) "%s: %d: " fmt, __func__, __LINE__
#define pr_fmt(fmt) KBUILD_MODNAME ": " FORMAT(fmt)

#include <linux/module.h>
#include <linux/uio.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/effect_offload.h>
#include <sound/effect_driver.h>

static DEFINE_MUTEX(effect_mutex);

int snd_ctl_effect_create(struct snd_card *card, void *arg)
{
	int retval = 0;
	struct snd_effect *effect;

	effect = kmalloc(sizeof(*effect), GFP_KERNEL);
	if (!effect)
		return -ENOMEM;
	if (copy_from_user(effect, (void __user *)arg, sizeof(*effect))) {
		retval = -EFAULT;
		goto out;
	}
	pr_debug("effect_offload: device %u, pos %u, mode%u\n",
			effect->device, effect->pos, effect->mode);

	mutex_lock(&card->effect_lock);
	retval = card->effect_ops->create(card, effect);
	mutex_unlock(&card->effect_lock);
out:
	kfree(effect);
	return retval;
}
EXPORT_SYMBOL_GPL(snd_ctl_effect_create);

int snd_ctl_effect_destroy(struct snd_card *card, void *arg)
{
	int retval = 0;
	struct snd_effect *effect;

	effect = kmalloc(sizeof(*effect), GFP_KERNEL);
	if (!effect)
		return -ENOMEM;
	if (copy_from_user(effect, (void __user *)arg, sizeof(*effect))) {
		retval = -EFAULT;
		goto out;
	}
	mutex_lock(&card->effect_lock);
	retval = card->effect_ops->destroy(card, effect);
	mutex_unlock(&card->effect_lock);
out:
	kfree(effect);
	return retval;
}
EXPORT_SYMBOL_GPL(snd_ctl_effect_destroy);

int snd_ctl_effect_set_params(struct snd_card *card, void *arg)
{
	int retval = 0;
	struct snd_effect_params *params;
	char *params_ptr;
	char __user *argp = (char __user *)arg;

	params = kmalloc(sizeof(*params), GFP_KERNEL);
	if (!params)
		return -ENOMEM;

	if (copy_from_user(params, argp, sizeof(*params))) {
		retval = -EFAULT;
		goto out;
	}
	params_ptr = kmalloc(params->size, GFP_KERNEL);
	if (!params_ptr) {
		retval = -ENOMEM;
		goto out;
	}

	if (copy_from_user((void *)params_ptr, (void __user *)params->buffer_ptr,
				params->size)) {
		retval = -EFAULT;
		goto free_buf;
	}

	params->buffer_ptr = (unsigned long)params_ptr;

	mutex_lock(&card->effect_lock);
	retval = card->effect_ops->set_params(card, params);
	mutex_unlock(&card->effect_lock);
free_buf:
	kfree(params_ptr);
out:
	kfree(params);
	return retval;
}
EXPORT_SYMBOL_GPL(snd_ctl_effect_set_params);

int snd_ctl_effect_get_params(struct snd_card *card, void *arg)
{
	int retval = 0;
	struct snd_effect_params inparams;
	struct snd_effect_params *outparams;
	unsigned int offset;
	char *params_ptr;
	char __user *argp = (char __user *)arg;

	if (copy_from_user((void *)&inparams, argp, sizeof(inparams)))
		retval = -EFAULT;

	outparams = kmalloc(sizeof(*outparams), GFP_KERNEL);
	if (!outparams)
		return -ENOMEM;

	memcpy(outparams, &inparams, sizeof(inparams));
	params_ptr = kmalloc(inparams.size, GFP_KERNEL);
	if (!params_ptr) {
		retval = -ENOMEM;
		goto free_out;
	}

	if (copy_from_user((void *)params_ptr, (void *)inparams.buffer_ptr,
							inparams.size)) {
		retval = -EFAULT;
		goto free_buf;
	}

	outparams->buffer_ptr = (unsigned long)params_ptr;

	mutex_lock(&card->effect_lock);
	retval = card->effect_ops->get_params(card, outparams);
	mutex_unlock(&card->effect_lock);

	if (retval)
		goto free_buf;

	if (!outparams->size)
		goto free_buf;

	if (outparams->size > inparams.size) {
		pr_err("mem insufficient to copy\n");
		retval = -EMSGSIZE;
		goto free_buf;
	} else {
		offset = offsetof(struct snd_effect_params, size);
		if (copy_to_user((argp + offset), (void *)&outparams->size,
								sizeof(u32)))
			retval = -EFAULT;

		if (copy_to_user((void *)inparams.buffer_ptr,
				(void *) outparams->buffer_ptr, outparams->size))
			retval = -EFAULT;
	}
free_buf:
	kfree(params_ptr);
free_out:
	kfree(outparams);
	return retval;
}
EXPORT_SYMBOL_GPL(snd_ctl_effect_get_params);

int snd_ctl_effect_query_num_effects(struct snd_card *card, void *arg)
{
	int retval = 0;
	int __user *ip = arg;

	mutex_lock(&card->effect_lock);
	retval = card->effect_ops->query_num_effects(card);
	mutex_unlock(&card->effect_lock);

	if (retval < 0)
		goto out;
	retval = put_user(retval, ip) ? -EFAULT : 0;
out:
	return retval;
}
EXPORT_SYMBOL_GPL(snd_ctl_effect_query_num_effects);

int snd_ctl_effect_query_effect_caps(struct snd_card *card, void *arg)
{
	int retval = 0;
	struct snd_effect_caps *caps;
	unsigned int offset, insize;
	char *caps_ptr;
	char __user *argp = (char __user *)arg;
	char __user *bufp;

	caps = kzalloc(sizeof(*caps), GFP_KERNEL);
	if (!caps)
		return -ENOMEM;

	if (copy_from_user(caps, argp, sizeof(*caps))) {
		retval = -EFAULT;
		goto out;
	}

	bufp = (void __user *)caps->buffer_ptr;
	insize = caps->size;
	caps_ptr = kmalloc(caps->size, GFP_KERNEL);
	if (!caps_ptr) {
		retval = -ENOMEM;
		goto out;
	}

	caps->buffer_ptr = (unsigned long)caps_ptr;

	mutex_lock(&card->effect_lock);
	retval = card->effect_ops->query_effect_caps(card, caps);
	mutex_unlock(&card->effect_lock);

	if (retval)
		goto free_buf;

	if (insize < caps->size) {
		pr_err("mem insufficient to copy\n");
		retval = -EMSGSIZE;
		goto free_buf;
	}

	offset = offsetof(struct snd_effect_caps, size);
	if (copy_to_user((argp + offset), (void *)&caps->size, sizeof(u32))) {
		retval = -EFAULT;
		goto free_buf;
	}

	if (copy_to_user(bufp, (void *)caps->buffer_ptr, caps->size))
		retval = -EFAULT;

free_buf:
	kfree(caps_ptr);
out:
	kfree(caps);
	return retval;
}
EXPORT_SYMBOL_GPL(snd_ctl_effect_query_effect_caps);

/**
 * snd_effect_register - register compressed device
 *
 * @card : snd card to which the effect is registered
 * @ops : effect_ops to register
 */
int snd_effect_register(struct snd_card *card, struct snd_effect_ops *ops)
{

	if (card == NULL || ops == NULL)
		return -EINVAL;

	if (snd_BUG_ON(!ops->create))
		return -EINVAL;
	if (snd_BUG_ON(!ops->destroy))
		return -EINVAL;
	if (snd_BUG_ON(!ops->set_params))
		return -EINVAL;

	mutex_init(&card->effect_lock);

	pr_debug("Registering Effects to card %s\n", card->shortname);
	/* register the effect ops with the card */
	mutex_lock(&effect_mutex);
	card->effect_ops = ops;
	mutex_unlock(&effect_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(snd_effect_register);

int snd_effect_deregister(struct snd_card *card)
{
	pr_debug("Removing effects for card %s\n", card->shortname);
	mutex_lock(&effect_mutex);
	card->effect_ops = NULL;
	mutex_unlock(&effect_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(snd_effect_deregister);

static int __init snd_effect_init(void)
{
	return 0;
}

static void __exit snd_effect_exit(void)
{
}

module_init(snd_effect_init);
module_exit(snd_effect_exit);

MODULE_DESCRIPTION("ALSA Effect offload framework");
MODULE_AUTHOR("Lakshmi N Vinnakota <lakshmi.n.vinnakota@intel.com>");
MODULE_AUTHOR("Vinod Koul <vinod.koul@intel.com>");
MODULE_LICENSE("GPL v2");
