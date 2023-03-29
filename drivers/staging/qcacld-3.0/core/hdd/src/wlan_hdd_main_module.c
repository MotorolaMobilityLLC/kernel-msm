/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * wlan_driver_resident: wlan_hdd_main_module.c
 *
 * wlan_driver_resident: WLAN Host Device Driver module interface implementation
 *
 */

#include <linux/module.h>
#include "qwlan_version.h"
#include "wlan_hdd_main.h"

static int __init hdd_module_init(void)
{
	return hdd_driver_load();
}

static void __exit hdd_module_exit(void)
{
	hdd_driver_unload();
}

module_init(hdd_module_init);
module_exit(hdd_module_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("WLAN HOST DEVICE DRIVER");

module_param_cb(con_mode, &con_mode_ops, &con_mode,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

module_param_cb(con_mode_ftm, &con_mode_ftm_ops, &con_mode_ftm,
		S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

module_param(country_code, charp, S_IRUSR | S_IRGRP | S_IROTH);
