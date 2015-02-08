#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <mach/gpio.h>
#include <linux/gpio.h>

#define GPIO_WL_HOST_WAKE 66
#define GPIO_WL_REG_ON 110


#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM

#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define WLAN_STATIC_DHD_INFO_BUF	7
#define WLAN_SCAN_BUF_SIZE		(64 * 1024)
#define WLAN_DHD_INFO_BUF_SIZE	(16 * 1024)
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

void *wlan_static_scan_buf0;
void *wlan_static_scan_buf1;
void *wlan_static_dhd_info_buf;

static void *brcm_wlan_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;

	if (section == WLAN_STATIC_SCAN_BUF0)
		return wlan_static_scan_buf0;

	if (section == WLAN_STATIC_SCAN_BUF1)
		return wlan_static_scan_buf1;

	if (section == WLAN_STATIC_DHD_INFO_BUF) {
		if (size > WLAN_DHD_INFO_BUF_SIZE) {
			pr_err("request DHD_INFO size(%lu) is bigger than static size(%d).\n", size, WLAN_DHD_INFO_BUF_SIZE);
			return NULL;
		}
		return wlan_static_dhd_info_buf;
	}

	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wlan_mem_array[section].size < size)
		return NULL;

	return wlan_mem_array[section].mem_ptr;
}

static int brcm_init_wlan_mem(void)
{
	int i;
	int j;

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

	wlan_static_scan_buf0 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf0)
		goto err_mem_alloc;

	wlan_static_scan_buf1 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf1)
		goto err_mem_alloc;

	wlan_static_dhd_info_buf = kmalloc(WLAN_DHD_INFO_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_info_buf)
		goto err_mem_alloc;

	printk(KERN_INFO"%s: WIFI MEM Allocated\n", __func__);
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wlan_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */


static unsigned config_gpio_wl_reg_on[] = {
	GPIO_CFG(GPIO_WL_REG_ON, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA) };

static unsigned config_gpio_wl_host_wake[] = {
	GPIO_CFG(GPIO_WL_HOST_WAKE, 0, GPIO_CFG_INPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA) };


int __init brcm_wifi_init_gpio(void)
{
	if (gpio_tlmm_config(config_gpio_wl_reg_on[0], GPIO_CFG_ENABLE))
		printk(KERN_ERR "%s: Failed to configure GPIO"
			" - WL_REG_ON\n", __func__);

	if (gpio_request(GPIO_WL_REG_ON, "WL_REG_ON"))
		printk(KERN_ERR "Failed to request gpio %d for WL_REG_ON\n",
			GPIO_WL_REG_ON);

	if (gpio_direction_output(GPIO_WL_REG_ON, 0))
		printk(KERN_ERR "%s: WL_REG_ON  "
			"failed to pull down\n", __func__);


	if (gpio_tlmm_config(config_gpio_wl_host_wake[0], GPIO_CFG_ENABLE))
		printk(KERN_ERR "%s: Failed to configure GPIO"
			" - WL_HOST_WAKE\n", __func__);

	if (gpio_request(GPIO_WL_HOST_WAKE, "WL_HOST_WAKE"))
		printk(KERN_ERR "Failed to request gpio for WL_HOST_WAKE\n");

	if (gpio_direction_input(GPIO_WL_HOST_WAKE))
		printk(KERN_ERR "%s: WL_HOST_WAKE  "
			"failed to pull down\n", __func__);


	return 0;
}

static int brcm_wlan_power(int onoff)
{
	printk(KERN_INFO"------------------------------------------------");
	printk(KERN_INFO"------------------------------------------------\n");
	printk(KERN_INFO"%s Enter: power %s\n", __func__, onoff ? "on" : "off");
	if (onoff) {
		/*
		if (gpio_request(GPIO_WL_REG_ON, "WL_REG_ON"))
		{
			printk("Failed to request for WL_REG_ON\n");
		}*/
		if (gpio_direction_output(GPIO_WL_REG_ON, 1)) {
			printk(KERN_ERR "%s: WL_REG_ON  failed to pull up\n",
				__func__);
			return -EIO;
		}
	} else {
		/*
		if (gpio_request(GPIO_WL_REG_ON, "WL_REG_ON"))
		{
			printk("Failed to request for WL_REG_ON\n");
		}
		*/
		if (gpio_direction_output(GPIO_WL_REG_ON, 0)) {
			printk(KERN_ERR "%s: WL_REG_ON  failed to pull down\n",
				__func__);
			return -EIO;
		}
	}
	return 0;
}

static int brcm_wlan_reset(int onoff)
{
  /*
	gpio_set_value(GPIO_WLAN_ENABLE,
			onoff ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
  */
	return 0;
}


static int brcm_wifi_cd; /* WIFI virtual 'card detect' status */
static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;

int brcm_wifi_status_register(
		void (*callback)(int card_present, void *dev_id),
		void *dev_id)
{
   int * p = (int *)dev_id;
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	printk(KERN_ERR "check print\n");
	printk(KERN_ERR "%s: callback is %p, devid is %d\n",
		__func__, wifi_status_cb, *p);
	return 0;
}

#if 1
unsigned int brcm_wifi_status(struct device *dev)
{
	printk("%s:%d status %d\n",__func__,__LINE__,brcm_wifi_cd);
	return brcm_wifi_cd;
}
#endif


static int brcm_wlan_set_carddetect(int val)
{
	pr_debug("%s: wifi_status_cb : %p, devid : %p, val : %d\n",
		__func__, wifi_status_cb, wifi_status_cb_devid, val);
	brcm_wifi_cd = val;
	if (wifi_status_cb)
	{
		printk(KERN_ERR "%s: callback is %p, devid is %p\n",
		__func__, wifi_status_cb, wifi_status_cb_devid);
		wifi_status_cb(val, wifi_status_cb_devid);
	}
	else
		pr_warning("%s: Nobody to notify\n", __func__);

	/* msleep(200); wait for carddetect */
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
	/* Table should be filled out based
	on custom platform regulatory requirement */
	{"",   "XZ", 11},  /* Universal if Country code is unknown or empty */
	{"AE", "AE", 1},
	{"AR", "AR", 1},
	{"AT", "AT", 1},
	{"AU", "AU", 2},
	{"BE", "BE", 1},
	{"BG", "BG", 1},
	{"BN", "BN", 1},
	{"CA", "CA", 2},
	{"CH", "CH", 1},
	{"CY", "CY", 1},
	{"CZ", "CZ", 1},
	{"DE", "DE", 3},
	{"DK", "DK", 1},
	{"EE", "EE", 1},
	{"ES", "ES", 1},
	{"FI", "FI", 1},
	{"FR", "FR", 1},
	{"GB", "GB", 1},
	{"GR", "GR", 1},
	{"HR", "HR", 1},
	{"HU", "HU", 1},
	{"IE", "IE", 1},
	{"IS", "IS", 1},
	{"IT", "IT", 1},
	{"JP", "JP", 5},
	{"KR", "KR", 24},
	{"KW", "KW", 1},
	{"LI", "LI", 1},
	{"LT", "LT", 1},
	{"LU", "LU", 1},
	{"LV", "LV", 1},
	{"MA", "MA", 1},
	{"MT", "MT", 1},
	{"MX", "MX", 1},
	{"NL", "NL", 1},
	{"NO", "NO", 1},
	{"PL", "PL", 1},
	{"PT", "PT", 1},
	{"PY", "PY", 1},
	{"RO", "RO", 1},
	{"RU", "RU", 5},
	{"SE", "SE", 1},
	{"SG", "SG", 4},
	{"SI", "SI", 1},
	{"SK", "SK", 1},
	{"TR", "TR", 7},
	{"TW", "TW", 2},
	{"US", "US", 46}
};


static void *brcm_wlan_get_country_code(char *ccode)
{
	int size = ARRAY_SIZE(brcm_wlan_translate_custom_table);
	int i;

	if (!ccode)
		return NULL;

	for (i = 0; i < size; i++)
		if (strcmp(ccode,
		brcm_wlan_translate_custom_table[i].iso_abbrev) == 0)
			return &brcm_wlan_translate_custom_table[i];
	return &brcm_wlan_translate_custom_table[0];
}


static struct resource brcm_wlan_resources[] = {
	[0] = {
		.name	= "bcmdhd_wlan_irq",
		//.start	= MSM_GPIO_TO_INT(GPIO_WL_HOST_WAKE),
		//.end	= MSM_GPIO_TO_INT(GPIO_WL_HOST_WAKE),
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE
			| IORESOURCE_IRQ_HIGHLEVEL,
	},
};

static struct wifi_platform_data brcm_wlan_control = {
	.set_power	= brcm_wlan_power,
	.set_reset	= brcm_wlan_reset,
	.set_carddetect	= brcm_wlan_set_carddetect,
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc	= brcm_wlan_mem_prealloc,
#endif
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

int __init brcm_wlan_init(void)
{
	printk(KERN_INFO"%s: start\n", __func__);

	brcm_wifi_init_gpio();
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	brcm_init_wlan_mem();
#endif

    /* set wifi gpio*/
    brcm_wlan_resources[0].start = gpio_to_irq(GPIO_WL_HOST_WAKE);
    brcm_wlan_resources[0].end = gpio_to_irq(GPIO_WL_HOST_WAKE);
    /* set wifi to power off */
    brcm_wlan_power(0);


	return platform_device_register(&brcm_device_wlan);
}
