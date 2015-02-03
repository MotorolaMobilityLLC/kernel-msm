#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <linux/mmc/host.h>
#include <mach/gpio.h>
#include <mach/board.h>
#if defined(CONFIG_SPARSE_IRQ)
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#endif /* defined(CONFIG_SPARSE_IRQ) */
//#include <linux/barcode_emul.h>

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

#ifdef CONFIG_SEC_KS01_PROJECT
enum {
    FPGA_VSIL_A_1P2_EN = 0,
    FPGA_GPIO_01,
    FPGA_GPIO_02,
    FPGA_GPIO_WLAN_EN,
    FPGA_GPIO_BT_EN,
    FPGA_GPIO_BT_WAKE,
    FPGA_GPIO_TDMB_RST,
    FPGA_GPIO_TDMB_EN,
    FPGA_GPIO_MHL_RST,
    FPGA_GPIO_VPS_SOUND_EN,
};
#endif /* defined(CONFIG_SEC_KS01_PROJECT) */

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
	if (wlan_static_scan_buf0)
		kfree(wlan_static_scan_buf0);
	if (wlan_static_scan_buf1)
		kfree(wlan_static_scan_buf1);
	if (wlan_static_dhd_info_buf)
		kfree(wlan_static_dhd_info_buf);

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

/* MSM8974 WLAN_EN GPIO Number */
#if defined(CONFIG_SEC_K_PROJECT) || defined(CONFIG_SEC_KACTIVE_PROJECT) || defined(CONFIG_SEC_KSPORTS_PROJECT)
#define GPIO_WL_REG_ON 308
#elif defined(CONFIG_SEC_H_PROJECT) || defined(CONFIG_SEC_VIENNA_PROJECT) || defined(CONFIG_SEC_LT03_PROJECT) ||\
      defined(CONFIG_SEC_PICASSO_PROJECT) || defined(CONFIG_SEC_V2_PROJECT) || defined(CONFIG_SEC_JS_PROJECT) ||\
      defined(CONFIG_SEC_F_PROJECT) || defined(CONFIG_SEC_MONTBLANC_PROJECT) || defined(CONFIG_SEC_KACTIVE_PROJECT) ||\
      defined(CONFIG_SEC_FRESCO_PROJECT)
#define GPIO_WL_REG_ON 53
#elif defined(CONFIG_MACH_MELIUSCASKT) || defined(CONFIG_MACH_MELIUSCAKTT) || defined(CONFIG_MACH_MELIUSCALGT)
#define GPIO_WL_REG_ON 100
#endif /* defined CONFIG_SEC_K_PROJECT and CONFIG_SEC_KACTIVE_PROJECT */

#define GPIO_WL_REG_ON 32

/* MSM8974 WLAN_HOST_WAKE GPIO Number */
#if defined(CONFIG_SEC_K_PROJECT) || defined(CONFIG_SEC_KACTIVE_PROJECT)
#if defined(CONFIG_MACH_KLTE_JPN_WLAN_OBSOLETE)
#define GPIO_WL_HOST_WAKE 73
#else
#define GPIO_WL_HOST_WAKE 92
#endif
#else
#define GPIO_WL_HOST_WAKE 31
#endif


#ifdef CONFIG_SEC_KS01_PROJECT
extern int ice_gpiox_get(int num);
extern int ice_gpiox_set(int num, int val);
#endif

#if !defined(CONFIG_SEC_K_PROJECT) && !defined(CONFIG_SEC_KS01_PROJECT) && !defined(CONFIG_SEC_KACTIVE_PROJECT)
static unsigned config_gpio_wl_reg_on[] = {
	GPIO_CFG(GPIO_WL_REG_ON, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA) };
#endif /* not defined (CONFIG_SEC_K_PROJECT && CONFIG_SEC_KS01_PROJECT) && CONFIG_SEC_KACTIVE_PROJECT*/

static int brcm_wifi_cd; /* WIFI virtual 'card detect' status */
static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;
static void *wifi_mmc_host;
// jade.son
extern void sdio_ctrl_power(struct mmc_host *card, bool onoff);

static unsigned get_gpio_wl_host_wake(void)
{
	unsigned gpio_wl_host_wake;

	gpio_wl_host_wake = GPIO_WL_HOST_WAKE;

	return gpio_wl_host_wake;
}

#if defined(CONFIG_SEC_K_PROJECT) || defined(CONFIG_SEC_KACTIVE_PROJECT)
extern unsigned int system_rev;
#endif

int __init brcm_wifi_init_gpio(void)
{
	unsigned gpio_cfg = GPIO_CFG(get_gpio_wl_host_wake(), 0, GPIO_CFG_INPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA);

#ifndef CONFIG_SEC_KS01_PROJECT
#if !defined(CONFIG_SEC_K_PROJECT) && !defined(CONFIG_SEC_KACTIVE_PROJECT)
	if (gpio_tlmm_config(config_gpio_wl_reg_on[0], GPIO_CFG_ENABLE))
		printk(KERN_ERR "%s: Failed to configure GPIO"
			" - WL_REG_ON\n", __func__);
#endif /* not defined CONFIG_SEC_K_PROJECT and CONFIG_SEC_KACTIVE_PROJECT*/

	if (gpio_request(GPIO_WL_REG_ON, "WL_REG_ON"))
		printk(KERN_ERR "Failed to request gpio %d for WL_REG_ON\n",
			GPIO_WL_REG_ON);

	if (gpio_direction_output(GPIO_WL_REG_ON, 0))
		printk(KERN_ERR "%s: WL_REG_ON  "
			"failed to pull down\n", __func__);
#endif /* not defined CONFIG_SEC_KS01_PROJECT */

#if defined(CONFIG_SEC_K_PROJECT)
	if (system_rev == 4) {
		printk("WLAN: Skip wlan irq setting..\n");
	}
	else
	{
		if (gpio_tlmm_config(gpio_cfg, GPIO_CFG_ENABLE))
			printk(KERN_ERR "%s: Failed to configure GPIO"
				" - WL_HOST_WAKE\n", __func__);

		if (gpio_request(get_gpio_wl_host_wake(), "WL_HOST_WAKE"))
			printk(KERN_ERR "Failed to request gpio for WL_HOST_WAKE\n");

		if (gpio_direction_input(get_gpio_wl_host_wake()))
			printk(KERN_ERR "%s: WL_HOST_WAKE  "
				"failed to pull down\n", __func__);
	}
#else
	if (gpio_tlmm_config(gpio_cfg, GPIO_CFG_ENABLE))
		printk(KERN_ERR "%s: Failed to configure GPIO"
			" - WL_HOST_WAKE\n", __func__);

	if (gpio_request(get_gpio_wl_host_wake(), "WL_HOST_WAKE"))
		printk(KERN_ERR "Failed to request gpio for WL_HOST_WAKE\n");

	if (gpio_direction_input(get_gpio_wl_host_wake()))
		printk(KERN_ERR "%s: WL_HOST_WAKE  "
			"failed to pull down\n", __func__);
#endif
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
		}
		*/

#ifdef CONFIG_SEC_KS01_PROJECT
		if (ice_gpiox_set(FPGA_GPIO_WLAN_EN, 1)) {
			printk(KERN_ERR "%s: WL_REG_ON  failed to pull up\n", __func__);
				return -EIO;
		}
#else
		printk(KERN_INFO"GPIO_REG_ON : %d\n" ,GPIO_WL_REG_ON); //jade.son
		printk(KERN_INFO"WL_REG_ON on-step : [%d]\n" , gpio_get_value(GPIO_WL_REG_ON));
		if (gpio_direction_output(GPIO_WL_REG_ON, 1)) {
			printk(KERN_ERR "%s: check WL_REG_ON pin for H\n", __func__);
			printk(KERN_ERR "%s: WL_REG_ON  failed to pull up\n", __func__);
				return -EIO;
		}


		if(gpio_get_value(GPIO_WL_REG_ON)){
			printk(KERN_INFO"WL_REG_ON on-step-2 : [%d]\n" , gpio_get_value(GPIO_WL_REG_ON));
		}
		else
		{
			printk("[%s] gpio value is 0. We need reinit.\n",__func__);
#if !defined(CONFIG_SEC_K_PROJECT) && !defined(CONFIG_SEC_KACTIVE_PROJECT)
			if (gpio_tlmm_config(config_gpio_wl_reg_on[0], GPIO_CFG_ENABLE))
				printk(KERN_ERR "%s: Failed to configure GPIO"
						" - WL_REG_ON\n", __func__);
#endif /* not defined CONFIG_SEC_K_PROJECT and CONFIG_SEC_KACTIVE_PROJECT*/

			if (gpio_direction_output(GPIO_WL_REG_ON, 1))
				printk(KERN_ERR "%s: WL_REG_ON  "
						"failed to pull down\n", __func__);
		}
#endif /* defined CONFIG_SEC_KS01_PROJECT */

	} else {
/*
		if (gpio_request(GPIO_WL_REG_ON, "WL_REG_ON"))
		{
			printk("Failed to request for WL_REG_ON\n");
		}
*/

#ifdef CONFIG_SEC_KS01_PROJECT
		if (ice_gpiox_set(FPGA_GPIO_WLAN_EN, 0)) {
			printk(KERN_ERR "%s: WL_REG_ON  failed to pull down\n", __func__);
				return -EIO;
		}
#else
		printk(KERN_INFO"WL_REG_ON off-step : [%d]\n" , gpio_get_value(GPIO_WL_REG_ON));
#if 1
		if (gpio_direction_output(GPIO_WL_REG_ON, 0)) {
			printk(KERN_ERR "%s: WL_REG_ON  failed to pull down\n", __func__);
				return -EIO;
		}
#endif
		printk(KERN_INFO"WL_REG_ON off-step-2 : [%d]\n" , gpio_get_value(GPIO_WL_REG_ON));
#endif
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

int brcm_wifi_status_register(
	void (*callback)(int card_present, void *dev_id),
	void *dev_id, void *mmc_host)
{
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	wifi_mmc_host = mmc_host;
	printk(KERN_INFO "%s: callback is %p, devid is %p\n",
		__func__, wifi_status_cb, dev_id);
	return 0;
}

unsigned int brcm_wifi_status(struct device *dev)
{
	printk("%s:%d status %d\n",__func__,__LINE__,brcm_wifi_cd);
	return brcm_wifi_cd;
}

static int brcm_wlan_set_carddetect(int val)
{
	printk("%s: wifi_status_cb : %p, devid : %p, val : %d\n",
		__func__, wifi_status_cb, wifi_status_cb_devid, val);
	brcm_wifi_cd = val;
	if (wifi_status_cb && brcm_wifi_cd )
		wifi_status_cb(val, wifi_status_cb_devid);
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
	{"",   "XY", 4},  /* universal */
	{"US", "US", 69}, /* input ISO "US" to : US regrev 69 */
	{"CA", "US", 69}, /* input ISO "CA" to : US regrev 69 */
	{"EU", "EU", 5},  /* European union countries */
	{"AT", "EU", 5},
	{"BE", "EU", 5},
	{"BG", "EU", 5},
	{"CY", "EU", 5},
	{"CZ", "EU", 5},
	{"DK", "EU", 5},
	{"EE", "EU", 5},
	{"FI", "EU", 5},
	{"FR", "EU", 5},
	{"DE", "EU", 5},
	{"GR", "EU", 5},
	{"HU", "EU", 5},
	{"IE", "EU", 5},
	{"IT", "EU", 5},
	{"LV", "EU", 5},
	{"LI", "EU", 5},
	{"LT", "EU", 5},
	{"LU", "EU", 5},
	{"MT", "EU", 5},
	{"NL", "EU", 5},
	{"PL", "EU", 5},
	{"PT", "EU", 5},
	{"RO", "EU", 5},
	{"SK", "EU", 5},
	{"SI", "EU", 5},
	{"ES", "EU", 5},
	{"SE", "EU", 5},
	{"GB", "EU", 5},  /* input ISO "GB" to : EU regrev 05 */
	{"IL", "IL", 0},
	{"CH", "CH", 0},
	{"TR", "TR", 0},
	{"NO", "NO", 0},
	{"KR", "XY", 3},
	{"AU", "XY", 3},
	{"CN", "XY", 3},  /* input ISO "CN" to : XY regrev 03 */
	{"TW", "XY", 3},
	{"AR", "XY", 3},
	{"MX", "XY", 3}
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
#if !defined(CONFIG_SPARSE_IRQ)
		.start	= MSM_GPIO_TO_INT(GPIO_WL_HOST_WAKE),
		.end	= MSM_GPIO_TO_INT(GPIO_WL_HOST_WAKE),
#endif /* !defined(CONFIG_SPARSE_IRQ) */
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
	printk(KERN_INFO"%s: gpio_to_irq IRQ=%d \n",
			__func__, (int)gpio_to_irq(GPIO_WL_HOST_WAKE) );

#if defined(CONFIG_SPARSE_IRQ)
	brcm_wlan_resources[0].start = gpio_to_irq(GPIO_WL_HOST_WAKE);
	brcm_wlan_resources[0].end = gpio_to_irq(GPIO_WL_HOST_WAKE);
#endif /* defined(CONFIG_SPARSE_IRQ) */
	brcm_wifi_init_gpio();
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	brcm_init_wlan_mem();
#endif

#if defined(CONFIG_SEC_K_PROJECT)
	if (system_rev >= 5)
		return platform_device_register(&brcm_device_wlan);
	else
		return -1;
#else
	return platform_device_register(&brcm_device_wlan);
#endif
}
device_initcall_sync(brcm_wlan_init);
