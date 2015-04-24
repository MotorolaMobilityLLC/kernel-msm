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
#include <asm/io.h>
#include <linux/fs.h>

#define PREALLOC_WLAN_NUMBER_OF_SECTIONS    4
#define PREALLOC_WLAN_NUMBER_OF_BUFFERS     160
#define PREALLOC_WLAN_SECTION_HEADER        24

#define WLAN_SECTION_SIZE_0 (PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_1 (PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_2 (PREALLOC_WLAN_NUMBER_OF_BUFFERS * 512)
#define WLAN_SECTION_SIZE_3 (PREALLOC_WLAN_NUMBER_OF_BUFFERS * 1024)

#define WLAN_SKB_BUF_NUM    16

#define WLAN_POWER    35
#define WLAN_HOSTWAKE 46

static unsigned wlan_wakes_msm[] = {
	    GPIO_CFG(WLAN_HOSTWAKE, 0, GPIO_CFG_INPUT,
		     GPIO_CFG_NO_PULL, GPIO_CFG_2MA) };

/* for wifi power supply */
static unsigned wifi_config_power_on[] = {
	GPIO_CFG(WLAN_POWER, 0, GPIO_CFG_OUTPUT,
		 GPIO_CFG_PULL_UP, GPIO_CFG_2MA)
};

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

typedef struct wifi_mem_prealloc_struct {
	    void *mem_ptr;
	    unsigned long size;
} wifi_mem_prealloc_t;

static wifi_mem_prealloc_t wifi_mem_array[PREALLOC_WLAN_NUMBER_OF_SECTIONS] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

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
	printk("[wlan]: %s: +++\n", __func__);

	if (sdc_status_cb)
		return -EINVAL;

	sdc_status_cb = cb;
	if(!sdc_status_cb)
		printk("[wlan]: %s: sdc_status_cb=null\n", __func__);
	sdc_dev = dev;
	if(!sdc_dev)
		printk("[wlan]: %s: sdc_dev=null\n", __func__);

	return 0;
}

unsigned int wcf_status(struct device *dev)
{
	printk("[wlan]: %s: wifi_detect = %d\n", __func__, g_wifi_detect);
	return g_wifi_detect;
}

static void *bcm_wifi_mem_prealloc(int section, unsigned long size)
{
	printk("[wlan]: %s: +++\n", __func__);
	
	if (section == PREALLOC_WLAN_NUMBER_OF_SECTIONS)
		return wlan_static_skb;
	if ((section < 0) || (section > PREALLOC_WLAN_NUMBER_OF_SECTIONS))
		return NULL;
	if (wifi_mem_array[section].size < size)
		return NULL;
	return wifi_mem_array[section].mem_ptr;
}

int bcm_wifi_set_power(int enable)
{
	int ret = 0;

	printk("[wlan]: %s: +++\n", __func__);

	if (enable) {
#ifdef CONFIG_BCM4335BT
		int lock_cookie_wifi = 'W' | 'i'<<8 | 'F'<<16 | 'i'<<24;	/* cookie is "WiFi" */

		printk("WiFi: trying to acquire BT lock\n");
		if (bcm_bt_lock(lock_cookie_wifi) != 0)
			printk("** WiFi: timeout in acquiring bt lock**\n");
		pr_err("%s: btlock acquired\n",__FUNCTION__);
#endif /* CONFIG_BCM4335BT */
		ret = gpio_direction_output(WLAN_POWER, 1); 

#ifdef CONFIG_BCM4335BT
		bcm_bt_unlock(lock_cookie_wifi);
#endif /* CONFIG_BCM4335BT */
		if (ret) {
			printk("[wlan]: %s: WL_REG_EN  failed to pull up (%d)\n", __func__, ret);
			return ret;
		}

		/* WLAN chip to reset */
		mdelay(150);
		printk("[wlan]: %s: wifi power successed to pull up\n", __func__);
	} else {
#ifdef CONFIG_BCM4335BT
		int lock_cookie_wifi = 'W' | 'i'<<8 | 'F'<<16 | 'i'<<24;	/* cookie is "WiFi" */

		printk("WiFi: trying to acquire BT lock\n");
		if (bcm_bt_lock(lock_cookie_wifi) != 0)
			printk("** WiFi: timeout in acquiring bt lock**\n");
		pr_err("%s: btlock acquired\n",__FUNCTION__);
#endif /* CONFIG_BCM4335BT */
		printk("[wlan]: %s: before gpio_set_value\n", __func__);
		ret = gpio_direction_output(WLAN_POWER, 0);
		//gpio_set_value(WLAN_POWER, 0);
		printk("[wlan]: %s: after gpio_set_value\n", __func__);
		
#ifdef CONFIG_BCM4335BT
		bcm_bt_unlock(lock_cookie_wifi);
#endif /* CONFIG_BCM4335BT */
		if (ret) {
			printk("[wlan]: %s:  WL_REG_EN  failed to pull down (%d)\n", __func__, ret);
			return ret;
		}

		/* WLAN chip down */
		mdelay(100);
		printk("[wlan]: %s: wifi power successed to pull down\n", __func__);
	}

	return ret;
}

int __init bcm_wifi_init_gpio_mem(struct platform_device *pdev)
{
	int i;
	int rc = 0;

	printk("[wlan]: %s: +++\n", __func__);

	/* WLAN_POWER */
	rc = gpio_tlmm_config(wifi_config_power_on[0], GPIO_CFG_ENABLE);
	if (rc < 0) {
		printk("[wlan]: %s: Failed to configure WLAN_POWER\n", __func__);
		return rc;
	}

	//rc = gpio_request_one(WLAN_POWER, GPIOF_OUT_INIT_LOW, "WL_REG_ON");
	rc = gpio_request_one(WLAN_POWER, GPIOF_OUT_INIT_LOW, "WL_REG_EN");
	if (rc < 0) {
		printk("[wlan]: %s: Failed to request gpio %d for WL_REG_EN\n",
				__func__, WLAN_POWER);
		return rc;
	}

	/* HOST_WAKEUP */
	rc = gpio_tlmm_config(wlan_wakes_msm[0], GPIO_CFG_ENABLE);
	if (rc < 0) {
		printk("[wlan]: %s: Failed to configure wlan_wakes_msm\n", __func__);
		goto err_gpio_tlmm_wakes_msm;
	}

	rc = gpio_request_one(WLAN_HOSTWAKE, GPIOF_IN, "wlan_wakes_msm");
	if (rc < 0) {
		printk("[wlan]: %s: Failed to request gpio %d for wlan_wakes_msm\n",
				__func__, WLAN_HOSTWAKE);
		goto err_gpio_tlmm_wakes_msm;
	}

	if (pdev) {
		struct resource *resource = pdev->resource;
		if (resource) {
			resource->start = gpio_to_irq(WLAN_HOSTWAKE);
			resource->end = gpio_to_irq(WLAN_HOSTWAKE);
		}
	}

	for (i = 0; i < WLAN_SKB_BUF_NUM; i++) {
		struct sk_buff *skb;
		if (i < (WLAN_SKB_BUF_NUM / 2))
			skb = dev_alloc_skb(4096);
		else
			skb = dev_alloc_skb(8192);
		if (!skb) {
			printk("[wlan]: %s: Failed to alloc skb buffer\n", __func__);
			rc = -ENOMEM;
			goto err_alloc_skb;
		}
		wlan_static_skb[i] = skb;
	}

	for (i = 0; i < PREALLOC_WLAN_NUMBER_OF_SECTIONS; i++) {
		wifi_mem_array[i].mem_ptr = kmalloc(wifi_mem_array[i].size,
				GFP_KERNEL);
		if (!wifi_mem_array[i].mem_ptr) {
			printk("[wlan]: %s: Failed to alloc memory\n", __func__);
			rc = -ENOMEM;
			goto err_alloc_wifi_mem_array;
		}
	}

	printk("[wlan]: %s: bcm_wifi_init_gpio_mem successfully\n", __func__);
	return 0;

err_alloc_wifi_mem_array:
	for (i = 0; i < PREALLOC_WLAN_NUMBER_OF_SECTIONS; i++) {
		if (wifi_mem_array[i].mem_ptr) {
			kfree(wifi_mem_array[i].mem_ptr);
			wifi_mem_array[i].mem_ptr = NULL;
		}
	}
err_alloc_skb:
	for (i = 0; i < WLAN_SKB_BUF_NUM; i++) {
		if (wlan_static_skb[i]) {
			dev_kfree_skb_any(wlan_static_skb[i]);
			wlan_static_skb[i] = NULL;
		}
	}
	gpio_free(WLAN_HOSTWAKE);
err_gpio_tlmm_wakes_msm:
	gpio_free(WLAN_POWER);
	return rc;
}
static int bcm_wifi_reset(int on)
{
	printk("[wlan]: %s: +++\n", __func__);
	return 0;
}

static int bcm_wifi_carddetect(int val)
{
	g_wifi_detect = val;

	printk("[wlan]: %s: +++\n", __func__);

	if (sdc_status_cb)
		sdc_status_cb(val, sdc_dev);
	else
		printk("[wlan]: %s: There is no callback for notify\n", __func__);
	return 0;
}

#define ETHER_ADDR_LEN    6
#define FILE_WIFI_MACADDR "/factory/mac.txt"

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

		/*if (i < ETHER_ADDR_LEN - 1) {
			if (*asc != ':')
				return NULL;
			asc++;
		}*/
	}

	/*if (*asc != '\0')
		return NULL;*/

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
	char macasc[128] = {0,};
	uint rand_mac;
	static unsigned char mymac[ETHER_ADDR_LEN] = {0,};
	const unsigned char nullmac[ETHER_ADDR_LEN] = {0,};

	printk("[wlan]: %s: +++\n", __func__);

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

	fp->f_pos += strlen("MacAddress0=");

	readlen = kernel_read(fp, fp->f_pos, macasc, 17); // 17 = 12 + 5
	if (readlen > 0) {
		unsigned char* macbin;
		struct ether_addr* convmac = ether_aton( macasc );

		if (convmac == NULL) {
			pr_err("%s: Invalid Mac Address Format %s\n",
					__FUNCTION__, macasc );
			goto random_mac;
		}

		macbin = convmac->ether_addr_octet;

		pr_info("%s: READ MAC ADDRESS %02X:%02X:%02X:%02X:%02X:%02X\n",
				__FUNCTION__,
				macbin[0], macbin[1], macbin[2],
				macbin[3], macbin[4], macbin[5]);

		memcpy(buf, macbin, ETHER_ADDR_LEN);
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

	//srandom32((uint)jiffies);
	//rand_mac = random32();
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
			__FUNCTION__,
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5] );

	return 0;
}

static struct wifi_platform_data bcm_wifi_control = {
	.mem_prealloc	= bcm_wifi_mem_prealloc,
	.set_power		= bcm_wifi_set_power,
	.set_reset      = bcm_wifi_reset,
	.set_carddetect = bcm_wifi_carddetect,
	.get_mac_addr   = bcm_wifi_get_mac_addr, 
};

static struct resource wifi_resource[] = {
	[0] = {
		.name = "bcmdhd_wlan_irq",
		.start = 0,  //assigned later
		.end   = 0,  //assigned later
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE, // for HW_OOB
		//.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_LOWEDGE, // for HW_OOB
		//.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE, // for HW_OOB
		//.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE | IORESOURCE_IRQ_LOWEDGE | IORESOURCE_IRQ_SHAREABLE, // for SW_OOB
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
	int i=-1;
	
	printk("[wlan]: %s: +++\n", __func__);
	bcm_wifi_init_gpio_mem(&bcm_wifi_device);
	i = platform_device_register(&bcm_wifi_device);
	printk("[wlan]: platform_device_register return %d\n", i);
	
	//bcm_wifi_set_power(1);
}
