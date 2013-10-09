/*
 * arch/arm/mach-msm/lge/board-wifi-bcm.c
 *
 * Copyright (C) 2013 LGE, Inc
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
#include <linux/irq.h>
#include <linux/if.h>
#include <linux/random.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <mach/board_lge.h>

#define WLAN_STATIC_SCAN_BUF0           5
#define WLAN_STATIC_SCAN_BUF1           6
#define PREALLOC_WLAN_SEC_NUM           4
#define PREALLOC_WLAN_BUF_NUM           160
#define PREALLOC_WLAN_SECTION_HEADER    24

#define WLAN_SECTION_SIZE_0     (PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1     (PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2     (PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3     (PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_HDRSIZE                 336
#define DHD_SKB_1PAGE_BUFSIZE   ((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE   ((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE   ((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SKB_BUF_NUM        17

#define WLAN_POWER          26
#define WLAN_HOSTWAKE_REV_F 74 /* for Lunchbox */
#define WLAN_HOSTWAKE       44

static int gpio_power = WLAN_POWER;
static int gpio_hostwake = WLAN_HOSTWAKE;

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

static void *wlan_static_scan_buf0 = NULL;
static void *wlan_static_scan_buf1 = NULL;

static void *bcm_wifi_mem_prealloc(int section, unsigned long size)
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

static int bcm_init_wlan_mem(void)
{
	int i;

	for (i = 0; i < WLAN_SKB_BUF_NUM; i++) {
		wlan_static_skb[i] = NULL;
	}

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

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
		wlan_mem_array[i].mem_ptr =
			kmalloc(wlan_mem_array[i].size, GFP_KERNEL);
		if (!wlan_mem_array[i].mem_ptr)
			goto err_mem_alloc;
	}

	wlan_static_scan_buf0 = kmalloc(65536, GFP_KERNEL);
	if (!wlan_static_scan_buf0)
		goto err_mem_alloc;

	wlan_static_scan_buf1 = kmalloc(65536, GFP_KERNEL);
	if (!wlan_static_scan_buf1)
		goto err_static_scan_buf;

	pr_info("wifi: %s: WIFI MEM Allocated\n", __func__);
	return 0;

err_static_scan_buf:
	pr_err("%s: failed to allocate scan_buf0\n", __func__);
	kfree(wlan_static_scan_buf0);
	wlan_static_scan_buf0 = NULL;

err_mem_alloc:
	pr_err("%s: failed to allocate mem_alloc\n", __func__);
	for (; i >= 0 ; i--) {
		kfree(wlan_mem_array[i].mem_ptr);
		wlan_mem_array[i].mem_ptr = NULL;
	}

	i = WLAN_SKB_BUF_NUM;
err_skb_alloc:
	pr_err("%s: failed to allocate skb_alloc\n", __func__);
	for (; i >= 0 ; i--) {
		dev_kfree_skb(wlan_static_skb[i]);
		wlan_static_skb[i] = NULL;
	}

	return -ENOMEM;
}

static unsigned int g_wifi_detect;
static void *sdc_dev;
void (*sdc_status_cb)(int card_present, void *dev);

#ifdef CONFIG_BCM4335BT
void __init bcm_bt_devs_init(void);
int bcm_bt_lock(int cookie);
void bcm_bt_unlock(int cookie);
#endif /* CONFIG_BCM4335BT */

int wcf_status_register(void (*cb)(int card_present, void *dev), void *dev)
{
	pr_info("%s\n", __func__);

	if (sdc_status_cb)
		return -EINVAL;

	sdc_status_cb = cb;
	sdc_dev = dev;

	return 0;
}

unsigned int wcf_status(struct device *dev)
{
	pr_info("%s: wifi_detect = %d\n", __func__, g_wifi_detect);
	return g_wifi_detect;
}

int bcm_wifi_set_power(int enable)
{
	int ret = 0;

	if (enable) {
#ifdef CONFIG_BCM4335BT
		int lock_cookie_wifi = 'W' | 'i'<<8 | 'F'<<16 | 'i'<<24;	/* cookie is "WiFi" */

		printk("WiFi: trying to acquire BT lock\n");
		if (bcm_bt_lock(lock_cookie_wifi) != 0)
			printk("** WiFi: timeout in acquiring bt lock**\n");
		pr_err("%s: btlock acquired\n",__FUNCTION__);
#endif /* CONFIG_BCM4335BT */
		ret = gpio_direction_output(gpio_power, 1);

#ifdef CONFIG_BCM4335BT
		bcm_bt_unlock(lock_cookie_wifi);
#endif /* CONFIG_BCM4335BT */
		if (ret) {
			pr_err("%s: WL_REG_ON  failed to pull up (%d)\n",
					__func__, ret);
			return ret;
		}

		/* WLAN chip to reset */
		mdelay(150);
		pr_info("%s: wifi power successed to pull up\n", __func__);
	} else {
#ifdef CONFIG_BCM4335BT
		int lock_cookie_wifi = 'W' | 'i'<<8 | 'F'<<16 | 'i'<<24;	/* cookie is "WiFi" */

		printk("WiFi: trying to acquire BT lock\n");
		if (bcm_bt_lock(lock_cookie_wifi) != 0)
			printk("** WiFi: timeout in acquiring bt lock**\n");
		pr_err("%s: btlock acquired\n",__FUNCTION__);
#endif /* CONFIG_BCM4335BT */
		ret = gpio_direction_output(gpio_power, 0);
#ifdef CONFIG_BCM4335BT
		bcm_bt_unlock(lock_cookie_wifi);
#endif /* CONFIG_BCM4335BT */
		if (ret) {
			pr_err("%s:  WL_REG_ON  failed to pull down (%d)\n",
					__func__, ret);
			return ret;
		}

		/* WLAN chip down */
		mdelay(100);
		pr_info("%s: wifi power successed to pull down\n", __func__);
	}

	return ret;
}

static int __init bcm_wifi_init_gpio_mem(struct platform_device *pdev)
{
	int rc = 0;
	unsigned gpio_config_power;
	unsigned gpio_config_hostwake;

	gpio_config_power = GPIO_CFG(gpio_power, 0, GPIO_CFG_OUTPUT,
			GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);
	gpio_config_hostwake = GPIO_CFG(gpio_hostwake, 0, GPIO_CFG_INPUT,
			GPIO_CFG_NO_PULL, GPIO_CFG_2MA);


	/* WLAN_POWER */
	rc = gpio_tlmm_config(gpio_config_power, GPIO_CFG_ENABLE);
	if (rc < 0) {
		pr_err("%s: Failed to configure WLAN_POWER\n", __func__);
		return rc;
	}

	rc = gpio_request_one(gpio_power, GPIOF_OUT_INIT_LOW, "WL_REG_ON");
	if (rc < 0) {
		pr_err("%s: Failed to request gpio %d for WL_REG_ON\n",
				__func__, gpio_power);
		return rc;
	}

	/* HOST_WAKEUP */
	rc = gpio_tlmm_config(gpio_config_hostwake, GPIO_CFG_ENABLE);
	if (rc < 0) {
		pr_err("%s: Failed to configure wlan_wakes_msm\n", __func__);
		goto err_gpio_tlmm_wakes_msm;
	}

	rc = gpio_request_one(gpio_hostwake, GPIOF_IN, "wlan_wakes_msm");
	if (rc < 0) {
		pr_err("%s: Failed to request gpio %d for wlan_wakes_msm\n",
				__func__, gpio_hostwake);
		goto err_gpio_tlmm_wakes_msm;
	}

	if (pdev) {
		struct resource *resource = pdev->resource;
		if (resource) {
			resource->start = gpio_to_irq(gpio_hostwake);
			resource->end = gpio_to_irq(gpio_hostwake);
		}
	}

	if (bcm_init_wlan_mem() < 0)
		goto err_alloc_wifi_mem_array;

	pr_info("%s: wifi gpio and mem initialized\n", __func__);
	return 0;

err_alloc_wifi_mem_array:
	gpio_free(gpio_hostwake);
err_gpio_tlmm_wakes_msm:
	gpio_free(gpio_power);
	return rc;
}

static int bcm_wifi_reset(int on)
{
	return 0;
}

static int bcm_wifi_carddetect(int val)
{
	g_wifi_detect = val;

	if (sdc_status_cb)
		sdc_status_cb(val, sdc_dev);
	else
		pr_warn("%s: There is no callback for notify\n", __func__);
	return 0;
}

#define ETHER_ADDR_LEN    6
#define FILE_WIFI_MACADDR "/persist/wifi/.macaddr"

static inline int xdigit (char c)
{
	unsigned d;

	d = (unsigned)(c-'0');
	if (d < 10)
		return (int)d;
	d = (unsigned)(c-'a');
	if (d < 6)
		return (int)(10+d);
	d = (unsigned)(c-'A');
	if (d < 6)
		return (int)(10+d);
	return -1;
}

struct ether_addr {
	unsigned char ether_addr_octet[ETHER_ADDR_LEN];
} __attribute__((__packed__));

struct ether_addr *
ether_aton_r (const char *asc, struct ether_addr * addr)
{
	int i, val0, val1;

	for (i = 0; i < ETHER_ADDR_LEN; ++i) {
		val0 = xdigit(*asc);
		asc++;
		if (val0 < 0)
			return NULL;

		val1 = xdigit(*asc);
		asc++;
		if (val1 < 0)
			return NULL;

		addr->ether_addr_octet[i] = (unsigned char)((val0 << 4) + val1);

		if (i < ETHER_ADDR_LEN - 1) {
			if (*asc != ':')
				return NULL;
			asc++;
		}
	}

	if (*asc != '\0')
		return NULL;

	return addr;
}

struct ether_addr * ether_aton (const char *asc)
{
	static struct ether_addr addr;
	return ether_aton_r(asc, &addr);
}

static int bcm_wifi_get_mac_addr(unsigned char *buf)
{
	int ret = 0;

	mm_segment_t oldfs;
	struct kstat stat;
	struct file* fp;
	int readlen = 0;
	char macread[128] = {0,};
	uint rand_mac;
	static unsigned char mymac[ETHER_ADDR_LEN] = {0,};
	const unsigned char nullmac[ETHER_ADDR_LEN] = {0,};
	const unsigned char bcastmac[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

	if (buf == NULL)
		return -EAGAIN;

	memset(buf, 0x00, ETHER_ADDR_LEN);

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_stat(FILE_WIFI_MACADDR, &stat);
	if (ret) {
		set_fs(oldfs);
		pr_err("%s: Failed to get information from file %s (%d)\n",
				__FUNCTION__, FILE_WIFI_MACADDR, ret);
		goto random_mac;
	}
	set_fs(oldfs);

	fp = filp_open(FILE_WIFI_MACADDR, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: Failed to read error %s\n",
				__FUNCTION__, FILE_WIFI_MACADDR);
		goto random_mac;
	}

#ifdef WIFI_MAC_FORMAT_ASCII
	readlen = kernel_read(fp, fp->f_pos, macread, 17); // 17 = 12 + 5
#else
	readlen = kernel_read(fp, fp->f_pos, macread, 6);
#endif
	if (readlen > 0) {
		unsigned char* macbin;
#ifdef WIFI_MAC_FORMAT_ASCII
		struct ether_addr* convmac = ether_aton( macread );

		if (convmac == NULL) {
			pr_err("%s: Invalid Mac Address Format %s\n",
					__FUNCTION__, macread );
			goto random_mac;
		}

		macbin = convmac->ether_addr_octet;
#else
		macbin = (unsigned char*)macread;
#endif
		pr_info("%s: READ MAC ADDRESS %02X:%02X:%02X:%02X:%02X:%02X\n",
				__FUNCTION__,
				macbin[0], macbin[1], macbin[2],
				macbin[3], macbin[4], macbin[5]);

		if (memcmp(macbin, nullmac, ETHER_ADDR_LEN) == 0 ||
				memcmp(macbin, bcastmac, ETHER_ADDR_LEN) == 0) {
			filp_close(fp, NULL);
			goto random_mac;
		}
		memcpy(buf, macbin, ETHER_ADDR_LEN);
	} else {
		filp_close(fp, NULL);
		goto random_mac;
	}

	filp_close(fp, NULL);
	return ret;

random_mac:

	pr_debug("%s: %p\n", __func__, buf);

	if (memcmp( mymac, nullmac, ETHER_ADDR_LEN) != 0) {
		/* Mac displayed from UI is never updated..
		   So, mac obtained on initial time is used */
		memcpy(buf, mymac, ETHER_ADDR_LEN);
		return 0;
	}

	srandom32((uint)jiffies);
	rand_mac = random32();
	buf[0] = 0x00;
	buf[1] = 0x90;
	buf[2] = 0x4c;
	buf[3] = (unsigned char)rand_mac;
	buf[4] = (unsigned char)(rand_mac >> 8);
	buf[5] = (unsigned char)(rand_mac >> 16);

	memcpy(mymac, buf, 6);

	pr_info("[%s] Exiting. MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
			__FUNCTION__,
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5] );

	return 0;
}

#define COUNTRY_BUF_SZ	4
struct cntry_locales_custom {
	char iso_abbrev[COUNTRY_BUF_SZ];
	char custom_locale[COUNTRY_BUF_SZ];
	int custom_locale_rev;
};

/* Customized Locale table */
static struct cntry_locales_custom wifi_translate_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement */
	{"",   "XV", 17},	/* Universal if Country code is unknown or empty */
	{"IR", "XV", 17},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"SD", "XV", 17},	/* Universal if Country code is SUDAN */
	{"SY", "XV", 17},	/* Universal if Country code is SYRIAN ARAB REPUBLIC */
	{"GL", "XV", 17},	/* Universal if Country code is GREENLAND */
	{"PS", "XV", 17},	/* Universal if Country code is PALESTINE */
	{"TL", "XV", 17},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"MH", "XV", 17},	/* Universal if Country code is MARSHALL ISLANDS */
	{"PK", "XV", 17},	/* Universal if Country code is PAKISTAN */
	{"CK", "XV", 17},	/* Universal if Country code is Cook Island (13.4.27)*/
	{"CU", "XV", 17},	/* Universal if Country code is Cuba (13.4.27)*/
	{"FK", "XV", 17},	/* Universal if Country code is Falkland Island (13.4.27)*/
	{"FO", "XV", 17},	/* Universal if Country code is Faroe Island (13.4.27)*/
	{"GI", "XV", 17},	/* Universal if Country code is Gibraltar (13.4.27)*/
	{"IM", "XV", 17},	/* Universal if Country code is Isle of Man (13.4.27)*/
	{"CI", "XV", 17},	/* Universal if Country code is Ivory Coast (13.4.27)*/
	{"JE", "XV", 17},	/* Universal if Country code is Jersey (13.4.27)*/
	{"KP", "XV", 17},	/* Universal if Country code is North Korea (13.4.27)*/
	{"FM", "XV", 17},	/* Universal if Country code is Micronesia (13.4.27)*/
	{"MM", "XV", 17},	/* Universal if Country code is Myanmar (13.4.27)*/
	{"NU", "XV", 17},	/* Universal if Country code is Niue (13.4.27)*/
	{"NF", "XV", 17},	/* Universal if Country code is Norfolk Island (13.4.27)*/
	{"PN", "XV", 17},	/* Universal if Country code is Pitcairn Islands (13.4.27)*/
	{"PM", "XV", 17},	/* Universal if Country code is Saint Pierre and Miquelon (13.4.27)*/
	{"SS", "XV", 17},	/* Universal if Country code is South_Sudan (13.4.27)*/
	{"AL", "AL", 2},
	{"DZ", "DZ", 1},
	{"AS", "AS", 12},	/* changed 2 -> 12*/
	{"AI", "AI", 1},
	{"AG", "AG", 2},
	{"AR", "AR", 21},
	{"AW", "AW", 2},
	{"AU", "AU", 6},
	{"AT", "AT", 4},
	{"AZ", "AZ", 2},
	{"BS", "BS", 2},
	{"BH", "BH", 4},	/* changed 24 -> 4*/
	{"BD", "BD", 2},
	{"BY", "BY", 3},
	{"BE", "BE", 4},
	{"BM", "BM", 12},
	{"BA", "BA", 2},
	{"BR", "BR", 4},
	{"VG", "VG", 2},
	{"BN", "BN", 4},
	{"BG", "BG", 4},
	{"KH", "KH", 2},
	{"CA", "CA", 31},
	{"KY", "KY", 3},
	{"CN", "CN", 24},
	{"CO", "CO", 17},
	{"CR", "CR", 17},
	{"HR", "HR", 4},
	{"CY", "CY", 4},
	{"CZ", "CZ", 4},
	{"DK", "DK", 4},
	{"EE", "EE", 4},
	{"ET", "ET", 2},
	{"FI", "FI", 4},
	{"FR", "FR", 5},
	{"GF", "GF", 2},
	{"DE", "DE", 7},
	{"GR", "GR", 4},
	{"GD", "GD", 2},
	{"GP", "GP", 2},
	{"GU", "GU", 12},
	{"HK", "HK", 2},
	{"HU", "HU", 4},
	{"IS", "IS", 4},
	{"IN", "IN", 3},
	{"ID", "KR", 25},	/* ID/1 -> KR/24 */
	{"IE", "IE", 5},
	{"IL", "BO", 0},	/* IL/7 -> BO/0 */
	{"IT", "IT", 4},
	{"JP", "JP", 58},
	{"JO", "JO", 3},
	{"KW", "KW", 5},
	{"LA", "LA", 2},
	{"LV", "LV", 4},
	{"LB", "LB", 5},
	{"LS", "LS", 2},
	{"LI", "LI", 4},
	{"LT", "LT", 4},
	{"LU", "LU", 3},
	{"MO", "MO", 2},
	{"MK", "MK", 2},
	{"MW", "MW", 1},
	{"MY", "MY", 3},
	{"MV", "MV", 3},
	{"MT", "MT", 4},
	{"MQ", "MQ", 2},
	{"MR", "MR", 2},
	{"MU", "MU", 2},
	{"YT", "YT", 2},
	{"MX", "MX", 20},
	{"MD", "MD", 2},
	{"MC", "MC", 1},
	{"ME", "ME", 2},
	{"MA", "MA", 2},
	{"NP", "NP", 3},
	{"NL", "NL", 4},
	{"AN", "AN", 2},
	{"NZ", "NZ", 4},
	{"NO", "NO", 4},
	{"OM", "OM", 4},
	{"PA", "PA", 17},
	{"PG", "PG", 2},
	{"PY", "PY", 2},
	{"PE", "PE", 20},
	{"PH", "PH", 5},
	{"PL", "PL", 4},
	{"PT", "PT", 4},
	{"PR", "PR", 20},
	{"RE", "RE", 2},
	{"RO", "RO", 4},
	{"SN", "SN", 2},
	{"RS", "RS", 2},
	{"SG", "SG", 4},
	{"SK", "SK", 4},
	{"SI", "SI", 4},
	{"ES", "ES", 4},
	{"LK", "LK", 1},
	{"SE", "SE", 4},
	{"CH", "CH", 4},
	{"TW", "TW", 1},
	{"TH", "TH", 5},
	{"TT", "TT", 3},
	{"TR", "TR", 7},
	{"AE", "AE", 6},
	{"UG", "UG", 2},
	{"GB", "GB", 6},
	{"UY", "UY", 1},
	{"VI", "VI", 13},
	{"VA", "VA", 12},	/* changed 2 -> 12 */
	{"VE", "VE", 3},
	{"VN", "VN", 4},
	{"MA", "MA", 1},
	{"ZM", "ZM", 2},
	{"EC", "EC", 21},
	{"SV", "SV", 19},
	{"KR", "KR", 57},
	{"RU", "RU", 13},
	{"UA", "UA", 8},
	{"GT", "GT", 1},
	{"MN", "MN", 1},
	{"NI", "NI", 2},
	{"US", "Q2", 57},
};

static void *bcm_wifi_get_country_code(char *ccode)
{
	int size, i;
	static struct cntry_locales_custom country_code;

	size = ARRAY_SIZE(wifi_translate_custom_table);

	if ((size == 0) || (ccode == NULL))
		return NULL;

	for (i = 0; i < size; i++) {
		if (!strcmp(ccode, wifi_translate_custom_table[i].iso_abbrev))
			return &wifi_translate_custom_table[i];
	}

	memset(&country_code, 0, sizeof(struct cntry_locales_custom));
	strlcpy(country_code.custom_locale, ccode, COUNTRY_BUF_SZ);

	return &country_code;
}

static struct wifi_platform_data bcm_wifi_control = {
	.mem_prealloc	= bcm_wifi_mem_prealloc,
	.set_power	= bcm_wifi_set_power,
	.set_reset      = bcm_wifi_reset,
	.set_carddetect = bcm_wifi_carddetect,
	.get_mac_addr   = bcm_wifi_get_mac_addr, 
	.get_country_code = bcm_wifi_get_country_code,
};

static struct resource wifi_resource[] = {
	[0] = {
		.name = "bcmdhd_wlan_irq",
		.start = 0,  //assigned later
		.end   = 0,  //assigned later
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE, // for HW_OOB
	},
};

static struct platform_device bcm_wifi_device = {
	.name           = "bcmdhd_wlan",
	.id             = 1,
	.num_resources  = ARRAY_SIZE(wifi_resource),
	.resource       = wifi_resource,
	.dev            = {
		.platform_data = &bcm_wifi_control,
	},
};

void __init init_bcm_wifi(void)
{
	if (HW_REV_F == lge_get_board_revno())
		gpio_hostwake = WLAN_HOSTWAKE_REV_F;

	bcm_wifi_init_gpio_mem(&bcm_wifi_device);
	platform_device_register(&bcm_wifi_device);
}
