/*
 * arch/arm/mach-msm/lge/board-wifi-bcm.c
 *
 * Copyright (C) 2013,2014 LGE, Inc
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/if.h>
#include <linux/random.h>
#include <linux/netdevice.h>
#include <linux/partialresume.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/wlan_plat.h>
#include <linux/fs.h>
#include <linux/regulator/consumer.h>
#include <asm/io.h>

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

#ifdef CONFIG_BCMDHD_GPIO_WL_RESET
#define WLAN_POWER CONFIG_BCMDHD_GPIO_WL_RESET
#else
#define WLAN_POWER          46
#endif

#ifdef CONFIG_BCMDHD_GPIO_WL_HOSTWAKEUP
#define WLAN_HOSTWAKE CONFIG_BCMDHD_GPIO_WL_HOSTWAKEUP
#else
#define WLAN_HOSTWAKE       37
#endif

#define WLAN_SCAN_BUF_SIZE  65536

static int gpio_power = WLAN_POWER;
static int gpio_hostwake = WLAN_HOSTWAKE;
static int power_enabled = 0;

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
	int i, num;

	for (i = 0; i < WLAN_SKB_BUF_NUM; i++)
		wlan_static_skb[i] = NULL;

	num = (WLAN_SKB_BUF_NUM - 1) >> 1;
	for (i = 0; i < num; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	num = WLAN_SKB_BUF_NUM -1;
	for (; i < num; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM; i++) {
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
		goto err_static_scan_buf;

	pr_info("%s: WIFI: MEM is pre-allocated\n", __func__);
	return 0;

err_static_scan_buf:
	pr_err("%s: failed to allocate scan_buf0\n", __func__);
	kfree(wlan_static_scan_buf0);
	wlan_static_scan_buf0 = NULL;

err_mem_alloc:
	pr_err("%s: failed to allocate mem_alloc\n", __func__);
	for (i -= 1; i >= 0; i--) {
		kfree(wlan_mem_array[i].mem_ptr);
		wlan_mem_array[i].mem_ptr = NULL;
	}

	i = WLAN_SKB_BUF_NUM;
err_skb_alloc:
	pr_err("%s: failed to allocate skb_alloc\n", __func__);
	for (i -= 1; i >= 0 ; i--) {
		dev_kfree_skb(wlan_static_skb[i]);
		wlan_static_skb[i] = NULL;
	}

	return -ENOMEM;
}

static unsigned int g_wifi_detect;
static void *sdc_dev;
void (*sdc_status_cb)(int card_present, void *dev);

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

static void bcm_set_regulator(struct regulator *reg, int enable)
{
	if (!reg) {
		pr_debug("%s: regulator is null\n", __func__);
		return;
	}

	if (enable) {
		if (regulator_enable(reg))
			 pr_warn("%s: failed to enable regulator\n",
					 __func__);
	} else {
		if(regulator_disable(reg))
			pr_warn("%s: failed to disable regulator\n",
					__func__);
	}
}

int bcm_wifi_set_power(int enable)
{
	static struct regulator *reg_batfet = NULL;
	int ret = 0;

	if (!reg_batfet) {
		reg_batfet = regulator_get(NULL, "batfet");
		if (IS_ERR(reg_batfet)) {
			pr_warn("%s: failed to get regualtor(batfet)\n",
					__func__);
			reg_batfet = NULL;
		}
		if (power_enabled)
			bcm_set_regulator(reg_batfet, 1);
	}

	enable = !!enable;
	if (!(power_enabled ^ enable))
		return 0;

	if (enable) {
		ret = gpio_direction_output(gpio_power, 1);
		if (ret) {
			pr_err("%s: WL_REG_ON: failed to pull up (%d)\n",
					__func__, ret);
			return ret;
		}
		power_enabled = 1;
		bcm_set_regulator(reg_batfet, 1);

		/* WLAN chip to reset */
		mdelay(150);
		pr_info("%s: WIFI ON\n", __func__);
	} else {
		ret = gpio_direction_output(gpio_power, 0);
		if (ret) {
			pr_err("%s:  WL_REG_ON: failed to pull down (%d)\n",
					__func__, ret);
			return ret;
		}
		power_enabled = 0;
		bcm_set_regulator(reg_batfet, 0);

		/* WLAN chip down */
		mdelay(100);
		pr_info("%s: WiFi OFF\n", __func__);
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

	rc = gpio_request_one(gpio_power, GPIOF_OUT_INIT_HIGH, "WL_REG_ON");
	if (rc < 0) {
		pr_err("%s: Failed to request gpio %d for WL_REG_ON\n",
				__func__, gpio_power);
		return rc;
	}
	power_enabled = 1;

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

	pr_info("%s: mapped gpio %d as power control\n", __func__,
			gpio_power);
	pr_info("%s: mapped gpio %d as hostwake control\n", __func__,
			gpio_hostwake);

	if (bcm_init_wlan_mem() < 0)
		goto err_alloc_wifi_mem_array;

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

static inline int xdigit(char c)
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
ether_aton_r(const char *asc, struct ether_addr * addr)
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

struct ether_addr * ether_aton(const char *asc)
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
				__func__, FILE_WIFI_MACADDR, ret);
		goto random_mac;
	}
	set_fs(oldfs);

	fp = filp_open(FILE_WIFI_MACADDR, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: Failed to read error %s\n",
				__func__, FILE_WIFI_MACADDR);
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
					__func__, macread );
			goto random_mac;
		}

		macbin = convmac->ether_addr_octet;
#else
		macbin = (unsigned char*)macread;
#endif
		pr_info("%s: READ MAC ADDRESS %02X:%02X:%02X:%02X:%02X:%02X\n",
				__func__,
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

	if (memcmp(mymac, nullmac, ETHER_ADDR_LEN) != 0) {
		/* Mac displayed from UI is never updated..
		   So, mac obtained on initial time is used */
		memcpy(buf, mymac, ETHER_ADDR_LEN);
		return 0;
	}

	prandom_seed((uint)jiffies);
	rand_mac = prandom_u32();
	buf[0] = 0x00;
	buf[1] = 0x90;
	buf[2] = 0x4c;
	buf[3] = (unsigned char)rand_mac;
	buf[4] = (unsigned char)(rand_mac >> 8);
	buf[5] = (unsigned char)(rand_mac >> 16);

	memcpy(mymac, buf, 6);

	pr_info("[%s] Exiting. MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
			__func__,
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
	{"",   "XZ", 11},	/* Universal if Country code is unknown or empty */
	{"CK", "XZ", 11},	/* Universal if Country code is Cook Island (13.4.27)*/
	{"CU", "XZ", 11},	/* Universal if Country code is Cuba (13.4.27)*/
	{"FO", "XZ", 11},	/* Universal if Country code is Faroe Island (13.4.27)*/
	{"GI", "XZ", 11},	/* Universal if Country code is Gibraltar (13.4.27)*/
	{"IM", "XZ", 11},	/* Universal if Country code is Isle of Man (13.4.27)*/
	{"IR", "XZ", 11},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"JE", "XZ", 11},	/* Universal if Country code is Jersey (13.4.27)*/
	{"KP", "XZ", 11},	/* Universal if Country code is North Korea (13.4.27)*/
	{"MH", "XZ", 11},	/* Universal if Country code is MARSHALL ISLANDS */
	{"NF", "XZ", 11},	/* Universal if Country code is Norfolk Island (13.4.27)*/
	{"NU", "XZ", 11},	/* Universal if Country code is Niue (13.4.27)*/
	{"PM", "XZ", 11},	/* Universal if Country code is Saint Pierre and Miquelon (13.4.27)*/
	{"PN", "XZ", 11},	/* Universal if Country code is Pitcairn Islands (13.4.27)*/
	{"PS", "XZ", 11},	/* Universal if Country code is PALESTINIAN TERRITORY, OCCUPIED */
	{"SD", "XZ", 11},	/* Universal if Country code is SUDAN */
	{"SS", "XZ", 11},	/* Universal if Country code is South_Sudan (13.4.27)*/
	{"SY", "XZ", 11},	/* Universal if Country code is SYRIAN ARAB REPUBLIC */
	{"TL", "XZ", 11},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"AD", "AD", 0},
	{"AE", "AE", 6},
	{"AF", "AF", 0},
	{"AG", "AG", 2},
	{"AI", "AI", 1},
	{"AL", "AL", 2},
	{"AM", "AM", 0},
	{"AN", "AN", 3},
	{"AO", "AO", 0},
	{"AR", "AU", 6},
	{"AS", "AS", 12},  /* changed 2 -> 12*/
	{"AT", "AT", 4},
	{"AU", "AU", 6},
	{"AW", "AW", 2},
	{"AZ", "AZ", 2},
	{"BA", "BA", 2},
	{"BB", "BB", 0},
	{"BD", "BD", 2},
	{"BE", "BE", 4},
	{"BF", "BF", 0},
	{"BG", "BG", 4},
	{"BH", "BH", 4},
	{"BI", "BI", 0},
	{"BJ", "BJ", 0},
	{"BM", "BM", 12},
	{"BN", "BN", 4},
	{"BO", "BO", 0},
	{"BR", "BR", 15},
	{"BS", "BS", 2},
	{"BT", "BT", 0},
	{"BW", "BW", 0},
	{"BY", "BY", 3},
	{"BZ", "BZ", 0},
	{"CA", "US", 118},
	{"CD", "CD", 0},
	{"CF", "CF", 0},
	{"CG", "CG", 0},
	{"CH", "CH", 4},
	{"CI", "CI", 0},
	{"CL", "CL", 0},
	{"CM", "CM", 0},
	{"CN", "CN", 38},
	{"CO", "CO", 17},
	{"CR", "CR", 17},
	{"CV", "CV", 0},
	{"CX", "CX", 0},
	{"CY", "CY", 4},
	{"CZ", "CZ", 4},
	{"DE", "DE", 7},
	{"DJ", "DJ", 0},
	{"DK", "DK", 4},
	{"DM", "DM", 0},
	{"DO", "DO", 0},
	{"DZ", "DZ", 1},
	{"EC", "EC", 21},
	{"EE", "EE", 4},
	{"EG", "EG", 0},
	{"ER", "ER", 0},
	{"ES", "ES", 4},
	{"ET", "ET", 2},
	{"FI", "FI", 4},
	{"FJ", "FJ", 0},
	{"FK", "FK", 0},
	{"FM", "FM", 0},
	{"FR", "FR", 5},
	{"GA", "GA", 0},
	{"GB", "GB", 6},
	{"GD", "GD", 2},
	{"GE", "GE", 0},
	{"GF", "GF", 2},
	{"GH", "GH", 0},
	{"GM", "GM", 0},
	{"GN", "GN", 0},
	{"GP", "GP", 2},
	{"GQ", "GQ", 0},
	{"GR", "GR", 4},
	{"GT", "GT", 1},
	{"GU", "GU", 12},
	{"GW", "GW", 0},
	{"GY", "GY", 0},
	{"HK", "HK", 2},
	{"HN", "HN", 0},
	{"HR", "HR", 4},
	{"HT", "HT", 0},
	{"HU", "HU", 4},
	{"ID", "ID", 1},
	{"IE", "IE", 5},
	{"IL", "IL", 7},
	{"IN", "IN", 3},
	{"IQ", "IQ", 0},
	{"IS", "IS", 4},
	{"IT", "IT", 4},
	{"JM", "JM", 0},
	{"JO", "JO", 3},
	{"JP", "JP", 58},
	{"KE", "SA", 0},
	{"KG", "KG", 0},
	{"KH", "KH", 2},
	{"KI", "KI", 0},
	{"KM", "KM", 0},
	{"KN", "KN", 0},
	{"KR", "KR", 57},
	{"KW", "KW", 5},
	{"KY", "KY", 3},
	{"KZ", "KZ", 0},
	{"LA", "LA", 2},
	{"LB", "LB", 5},
	{"LC", "LC", 0},
	{"LI", "LI", 4},
	{"LK", "LK", 1},
	{"LR", "LR", 0},
	{"LS", "LS", 2},
	{"LT", "LT", 4},
	{"LU", "LU", 3},
	{"LV", "LV", 4},
	{"LY", "LY", 0},
	{"MA", "MA", 2},
	{"MC", "MC", 1},
	{"MD", "MD", 2},
	{"ME", "ME", 2},
	{"MF", "MF", 0},
	{"MG", "MG", 0},
	{"MK", "MK", 2},
	{"ML", "ML", 0},
	{"MM", "MM", 0},
	{"MN", "MN", 1},
	{"MO", "MO", 2},
	{"MP", "MP", 0},
	{"MQ", "MQ", 2},
	{"MR", "MR", 2},
	{"MS", "MS", 0},
	{"MT", "MT", 4},
	{"MU", "MU", 2},
	{"MV", "MV", 3},
	{"MW", "MW", 1},
	{"MX", "AU", 6},
	{"MY", "MY", 3},
	{"MZ", "MZ", 0},
	{"NA", "NA", 0},
	{"NC", "NC", 0},
	{"NE", "NE", 0},
	{"NG", "NG", 0},
	{"NI", "NI", 2},
	{"NL", "NL", 4},
	{"NO", "NO", 4},
	{"NP", "ID", 5},
	{"NR", "NR", 0},
	{"NZ", "NZ", 4},
	{"OM", "OM", 4},
	{"PA", "PA", 17},
	{"PE", "PE", 20},
	{"PF", "PF", 0},
	{"PG", "AU", 6},
	{"PH", "PH", 5},
	{"PK", "PK", 0},
	{"PL", "PL", 4},
	{"PR", "US", 118},
	{"PT", "PT", 4},
	{"PW", "PW", 0},
	{"PY", "PY", 2},
	{"QA", "QA", 0},
	{"RE", "RE", 2},
	{"RKS", "KG", 0},
	{"RO", "RO", 4},
	{"RS", "RS", 2},
	{"RU", "RU", 13},
	{"RW", "RW", 0},
	{"SA", "SA", 0},
	{"SB", "SB", 0},
	{"SC", "SC", 0},
	{"SE", "SE", 4},
	{"SG", "SG", 0},
	{"SI", "SI", 4},
	{"SK", "SK", 4},
	{"SL", "SL", 0},
	{"SM", "SM", 0},
	{"SN", "SN", 2},
	{"SO", "SO", 0},
	{"SR", "SR", 0},
	{"ST", "ST", 0},
	{"SV", "SV", 25},
	{"SZ", "SZ", 0},
	{"TC", "TC", 0},
	{"TD", "TD", 0},
	{"TF", "TF", 0},
	{"TG", "TG", 0},
	{"TH", "TH", 5},
	{"TJ", "TJ", 0},
	{"TM", "TM", 0},
	{"TN", "TN", 0},
	{"TO", "TO", 0},
	{"TR", "TR", 7},
	{"TT", "TT", 3},
	{"TV", "TV", 0},
	{"TW", "TW", 1},
	{"TZ", "TZ", 0},
	{"UA", "UA", 8},
	{"UG", "UG", 2},
	{"US", "US", 118},
	{"UY", "UY", 1},
	{"UZ", "MA", 2},
	{"VA", "VA", 2},
	{"VC", "VC", 0},
	{"VE", "VE", 3},
	{"VG", "VG", 2},
	{"VI", "US", 118},
	{"VN", "VN", 4},
	{"VU", "VU", 0},
	{"WS", "WS", 0},
	{"YE", "YE", 0},
	{"YT", "YT", 2},
	{"ZA", "ZA", 6},
	{"ZM", "LA", 2},
	{"ZW", "ZW", 0},
	{"00", "XZ", 999},
};

static void *bcm_wifi_get_country_code(char *ccode)
{
	int size, i;
	static struct cntry_locales_custom country_code;

	size = ARRAY_SIZE(wifi_translate_custom_table);

	if (!size || !ccode)
		return NULL;

	for (i = 0; i < size; i++) {
		if (!strcmp(ccode, wifi_translate_custom_table[i].iso_abbrev))
			return &wifi_translate_custom_table[i];
	}

	memset(&country_code, 0, sizeof(struct cntry_locales_custom));
	strlcpy(country_code.custom_locale, ccode, COUNTRY_BUF_SZ);

	return &country_code;
}

#ifdef CONFIG_PARTIALRESUME
static int dummy_partial_resume(struct partial_resume *pr)
{
	return 0;
}

#define PR_INIT_STATE		0
#define PR_IN_RESUME_STATE	1
#define PR_RESUME_OK_STATE	2
#define PR_SUSPEND_OK_STATE	3

static DECLARE_COMPLETION(bcm_pk_comp);
static DECLARE_COMPLETION(bcm_wd_comp);
static int bcm_suspend = PR_INIT_STATE;
static spinlock_t bcm_lock;

/*
 * Partial Resume State Machine:
    _______
   / else [INIT]________________________
   \______/   | notify_resume           \
           [IN_RESUME]              wait_for_ready
           /       \ vote_for_suspend   /
   vote_for_resume [SUSPEND_OK]________/
           \       / vote_for_resume  /
          [RESUME_OK]                /
                   \________________/
 */

static bool bcm_wifi_process_partial_resume(int action)
{
	bool suspend = false;
	int timeout = 0;

	if ((action != WIFI_PR_NOTIFY_RESUME) && (bcm_suspend == PR_INIT_STATE))
		return suspend;

	if (action == WIFI_PR_WAIT_FOR_READY)
		timeout = wait_for_completion_timeout(&bcm_pk_comp,
						      msecs_to_jiffies(50));

	spin_lock(&bcm_lock);
	switch (action) {
	case WIFI_PR_WAIT_FOR_READY:
		suspend = (bcm_suspend == PR_SUSPEND_OK_STATE) && (timeout != 0);
		if (suspend) {
			spin_unlock(&bcm_lock);
			timeout = wait_for_completion_timeout(&bcm_wd_comp,
							msecs_to_jiffies(100));
			spin_lock(&bcm_lock);
			suspend = (timeout != 0);
		}
		bcm_suspend = PR_INIT_STATE;
		break;
	case WIFI_PR_VOTE_FOR_RESUME:
		bcm_suspend = PR_RESUME_OK_STATE;
		complete(&bcm_pk_comp);
		break;
	case WIFI_PR_VOTE_FOR_SUSPEND:
		if (bcm_suspend == PR_IN_RESUME_STATE)
			bcm_suspend = PR_SUSPEND_OK_STATE;
		complete(&bcm_pk_comp);
		break;
	case WIFI_PR_NOTIFY_RESUME:
		INIT_COMPLETION(bcm_pk_comp);
		bcm_suspend = PR_IN_RESUME_STATE;
		break;
	case WIFI_PR_INIT:
		bcm_suspend = PR_INIT_STATE;
		break;
	case WIFI_PR_WD_INIT:
		INIT_COMPLETION(bcm_wd_comp);
		break;
	case WIFI_PR_WD_COMPLETE:
		complete(&bcm_wd_comp);
		break;
	}
	spin_unlock(&bcm_lock);
	return suspend;
}

bool wlan_vote_for_suspend(void)
{
	return bcm_wifi_process_partial_resume(WIFI_PR_VOTE_FOR_SUSPEND);
}
EXPORT_SYMBOL(wlan_vote_for_suspend);

static int bcm_wifi_partial_resume(struct partial_resume *pr)
{
	bool suspend;

	suspend = bcm_wifi_process_partial_resume(WIFI_PR_WAIT_FOR_READY);
	pr_info("%s: vote %d\n", __func__, suspend);
	return suspend ? 1 : -1;
}
#endif

static struct wifi_platform_data bcm_wifi_control = {
	.mem_prealloc	= bcm_wifi_mem_prealloc,
	.set_power	= bcm_wifi_set_power,
	.set_reset      = bcm_wifi_reset,
	.set_carddetect = bcm_wifi_carddetect,
	.get_mac_addr   = bcm_wifi_get_mac_addr,
	.get_country_code = bcm_wifi_get_country_code,
#ifdef CONFIG_PARTIALRESUME
	.partial_resume = bcm_wifi_process_partial_resume,
#endif
};

static struct resource wifi_resource[] = {
	[0] = {
		.name = "bcmdhd_wlan_irq",
		.start = 0,  /* assigned later */
		.end   = 0,  /* assigned later */
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE, /* for HW_OOB */
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

static int __init bcm_wifi_init(void)
{
	bcm_wifi_init_gpio_mem(&bcm_wifi_device);
	platform_device_register(&bcm_wifi_device);

	return 0;
}

subsys_initcall(bcm_wifi_init);

#ifdef CONFIG_PARTIALRESUME
static struct partial_resume smd_pr = {
	.irq = 200,
	.partial_resume = dummy_partial_resume,
};

static struct partial_resume mpm_pr = {
	.irq = 203,
	.partial_resume = dummy_partial_resume,
};

static struct partial_resume vsync_pr = {
	.irq = 459,
	.partial_resume = dummy_partial_resume,
};

static struct partial_resume wlan_pr = {
	.partial_resume = bcm_wifi_partial_resume,
};

int __init wlan_partial_resume_init(void)
{
	int rc;

	spin_lock_init(&bcm_lock); /* Setup partial resume */
	complete(&bcm_wd_comp);    /* Prepare for case when WD is not set */
	wlan_pr.irq = bcm_wifi_device.resource->start;
	rc = register_partial_resume(&wlan_pr);
	pr_debug("%s: after registering %pF: %d\n", __func__,
		 wlan_pr.partial_resume, rc);
	rc = register_partial_resume(&smd_pr);
	rc = register_partial_resume(&mpm_pr);
	rc = register_partial_resume(&vsync_pr);
	pr_debug("%s: after registering %pF: %d\n", __func__,
		 smd_pr.partial_resume, rc);
	return rc;
}

late_initcall(wlan_partial_resume_init);
#endif

MODULE_DESCRIPTION("WiFi control for BCMDHD");
MODULE_LICENSE("GPL");
