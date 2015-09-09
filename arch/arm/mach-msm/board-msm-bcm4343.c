/*
 * Copyright (C) 2014 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <linux/mmc/host.h>
#include <linux/if.h>

#include <mach/gpio.h>
#include <mach/board.h>
#include <linux/of_gpio.h>

#include <linux/fcntl.h>
#include <linux/fs.h>

#define BCM_DBG pr_debug

#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define WLAN_SCAN_BUF_SIZE		(64 * 1024)
#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_HDRSIZE			336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SKB_BUF_NUM	17

static int gpio_wl_reg_on = 28;

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wlan_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static struct wlan_mem_prealloc wlan_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

static void *wlan_static_scan_buf0;
static void *wlan_static_scan_buf1;

static void *brcm_wlan_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;

	if (section == WLAN_STATIC_SCAN_BUF0)
		return wlan_static_scan_buf0;

	if (section == WLAN_STATIC_SCAN_BUF1)
		return wlan_static_scan_buf1;

	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wlan_mem_array[section].size < size)
		return NULL;

	return wlan_mem_array[section].mem_ptr;
}

static int __init brcm_init_wlan_mem(void)
{
	int i;
	int j;
	for (i = 0; i < WLAN_SKB_BUF_NUM; i++)
		wlan_static_skb[i] = NULL;

	for (i = 0; i < 8; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (; i < 16; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;

	for (i = 0; i < PREALLOC_WLAN_SEC_NUM; i++) {
		wlan_mem_array[i].mem_ptr =
				kmalloc(wlan_mem_array[i].size, GFP_KERNEL);

		if (!wlan_mem_array[i].mem_ptr)
			goto err_mem_alloc;
	}

	wlan_static_scan_buf0 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf0)
		goto err_mem_alloc;

	wlan_static_scan_buf1 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf1)
		goto err_mem_alloc;


	BCM_DBG("%s: WIFI MEM Allocated\n", __func__);
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0; j < i; j++)
		kfree(wlan_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0; j < i; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}


static int __init brcm_wifi_init_gpio(struct device_node *np)
{
	if (np) {
		int wl_reg_on;
		wl_reg_on = of_get_named_gpio(np, "wl_reg_on", 0);
		if (wl_reg_on >= 0) {
			gpio_wl_reg_on = wl_reg_on;
			BCM_DBG("%s: wl_reg_on GPIO %d\n", __func__, wl_reg_on);
		}
	}

	if (gpio_request(gpio_wl_reg_on, "WL_REG_ON"))
		pr_err("%s: Failed to request gpio %d for WL_REG_ON\n",
			__func__, gpio_wl_reg_on);
	else
		pr_err("%s: gpio_request WL_REG_ON done\n", __func__);

	if (gpio_export(gpio_wl_reg_on, false))
		pr_err("%s: failed to export gpio %d\n", __func__, gpio_wl_reg_on);

	if (gpio_direction_output(gpio_wl_reg_on, 1))
		pr_err("%s: WL_REG_ON failed to pull up\n", __func__);
	else
		BCM_DBG("%s: WL_REG_ON is pulled up\n", __func__);

	return 0;
}

static int brcm_wlan_power(int on)
{
	pr_info("%s Enter: power %s\n", __func__, on ? "on" : "off");

	if (on) {
		if (gpio_direction_output(gpio_wl_reg_on, 1)) {
			pr_err("%s: WL_REG_ON didn't output high\n", __func__);
			return -EIO;
		}
	} else {
		if (gpio_direction_output(gpio_wl_reg_on, 0)) {
			pr_err("%s: WL_REG_ON didn't output low\n", __func__);
			return -EIO;
		}
	}
	return 0;
}

static int brcm_wlan_reset(int onoff)
{
	return 0;
}

static int brcm_wlan_set_carddetect(int val)
{
	return 0;
}

/* Customized Locale table : OPTIONAL feature */
#define WLC_CNTRY_BUF_SZ        4
struct cntry_locales_custom {
	char iso_abbrev[WLC_CNTRY_BUF_SZ];
	char custom_locale[WLC_CNTRY_BUF_SZ];
	int  custom_locale_rev;
};

static struct cntry_locales_custom brcm_wlan_translate_custom_table[] = {
	/* Table should be filled out based on custom platform regulatory requirement */
	{"",   "XT", 0},  /* Universal if Country code is unknown or empty */
	{"US", "US", 0},
	{"AE", "AE", 0},
	{"AR", "AR", 0},
	{"AT", "AT", 0},
	{"AU", "AU", 0},
	{"BE", "BE", 0},
	{"BG", "BG", 0},
	{"BN", "BN", 0},
	{"BR", "BR", 0},
	{"CA", "US", 0},   /* Previously was CA/31 */
	{"CH", "CH", 0},
	{"CY", "CY", 0},
	{"CZ", "CZ", 0},
	{"DE", "DE", 0},
	{"DK", "DK", 0},
	{"EE", "EE", 0},
	{"ES", "ES", 0},
	{"FI", "FI", 0},
	{"FR", "FR", 0},
	{"GB", "GB", 0},
	{"GR", "GR", 0},
	{"HK", "HK", 0},
	{"HR", "HR", 0},
	{"HU", "HU", 0},
	{"IE", "IE", 0},
	{"IN", "IN", 0},
	{"IS", "IS", 0},
	{"IT", "IT", 0},
	{"ID", "ID", 0},
	{"JP", "JP", 72},
	{"KR", "KR", 0},
	{"KW", "KW", 0},
	{"LI", "LI", 0},
	{"LT", "LT", 0},
	{"LU", "LU", 0},
	{"LV", "LV", 0},
	{"MA", "MA", 0},
	{"MT", "MT", 0},
	{"MX", "MX", 0},
	{"MY", "MY", 0},
	{"NL", "NL", 0},
	{"NO", "NO", 0},
	{"NZ", "NZ", 0},
	{"PL", "PL", 0},
	{"PT", "PT", 0},
	{"PY", "PY", 0},
	{"RO", "RO", 0},
	{"RU", "RU", 0},
	{"SE", "SE", 0},
	{"SG", "SG", 0},
	{"SI", "SI", 0},
	{"SK", "SK", 0},
	{"TH", "TH", 0},
	{"TR", "TR", 0},
	{"TW", "TW", 0},
	{"VN", "VN", 0},
};

static void *brcm_wlan_get_country_code(char *ccode)
{
	struct cntry_locales_custom *locales;
	int size;
	int i;

	if (!ccode)
		return NULL;

	locales = brcm_wlan_translate_custom_table;
	size = ARRAY_SIZE(brcm_wlan_translate_custom_table);

	for (i = 0; i < size; i++)
		if (strcmp(ccode, locales[i].iso_abbrev) == 0)
			return &locales[i];
	return &locales[0];
}

static unsigned char brcm_mac_addr[IFHWADDRLEN] = { 0, 0x90, 0x4c, 0, 0, 0 };

static int __init brcm_mac_addr_setup(char *str)
{
	char macstr[IFHWADDRLEN*3];
	char *macptr = macstr;
	char *token;
	int i = 0;

	if (!str)
		return 0;
	BCM_DBG("wlan MAC = %s\n", str);
	if (strlen(str) >= sizeof(macstr))
		return 0;
	strlcpy(macstr, str, sizeof(macstr));

	while ((token = strsep(&macptr, ":")) != NULL) {
		unsigned long val;
		int res;

		if (i >= IFHWADDRLEN)
			break;
		res = kstrtoul(token, 0x10, &val);
		if (res < 0)
			break;
		brcm_mac_addr[i++] = (u8)val;
	}

	if (i < IFHWADDRLEN && strlen(macstr)==IFHWADDRLEN*2) {
		/* try again with wrong format (sans colons) */
		u64 mac;
		if (kstrtoull(macstr, 0x10, &mac) < 0)
			return 0;
		for (i=0; i<IFHWADDRLEN; i++)
			brcm_mac_addr[IFHWADDRLEN-1-i] = (u8)((0xFF)&(mac>>(i*8)));
	}

	return i==IFHWADDRLEN ? 1:0;
}

__setup("androidboot.wifimacaddr=", brcm_mac_addr_setup);

static int brcm_wifi_get_mac_addr(unsigned char *buf)
{
	uint rand_mac;

	if (!buf)
		return -EFAULT;

	if ((brcm_mac_addr[4] == 0) && (brcm_mac_addr[5] == 0)) {
		prandom_seed((uint)jiffies);
		rand_mac = prandom_u32();
		brcm_mac_addr[3] = (unsigned char)rand_mac;
		brcm_mac_addr[4] = (unsigned char)(rand_mac >> 8);
		brcm_mac_addr[5] = (unsigned char)(rand_mac >> 16);
	}
	memcpy(buf, brcm_mac_addr, IFHWADDRLEN);
	return 0;
}

static struct resource brcm_wlan_resources[] = {
	[0] = {
		.name	= "bcmdhd_wlan_irq",
		.start	= 0, /* Dummy */
		.end	= 0, /* Dummy */
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE
			| IORESOURCE_IRQ_HIGHLEVEL,
	},
};

static struct wifi_platform_data brcm_wlan_control = {
	.set_power	= brcm_wlan_power,
	.set_reset	= brcm_wlan_reset,
	.set_carddetect	= brcm_wlan_set_carddetect,
	.get_mac_addr = brcm_wifi_get_mac_addr,
	.mem_prealloc	= brcm_wlan_mem_prealloc,
	.get_country_code = brcm_wlan_get_country_code,
};

static struct platform_device brcm_device_wlan = {
	.name		= "bcmdhd_wlan",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(brcm_wlan_resources),
	.resource	= brcm_wlan_resources,
	.dev		= {
		.platform_data = &brcm_wlan_control,
	},
};

static int __init brcm_wlan_init(void)
{
	int rc = 0;
	struct device_node *np;
	int gpio_wlan_host_wake;

	BCM_DBG("%s: START\n", __func__);

	np = of_find_compatible_node(NULL, NULL, "bcm,bcm4343");
	if (np && of_device_is_available(np)) {
		gpio_wlan_host_wake = of_get_named_gpio(np, "wl_host_wake", 0);
		brcm_wlan_resources[0].start = gpio_to_irq(gpio_wlan_host_wake);

		brcm_init_wlan_mem();
		brcm_wifi_init_gpio(np);

		rc = platform_device_register(&brcm_device_wlan);
		brcm_device_wlan.dev.of_node = np;
	}

	return rc;
}
subsys_initcall(brcm_wlan_init);
