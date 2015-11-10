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
#ifdef CONFIG_PARTIALRESUME
#include <linux/partialresume.h>
#include <linux/spinlock.h>
#endif

#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/msm_pcie.h>
#include <linux/of_gpio.h>

#include <linux/fcntl.h>
#include <linux/fs.h>
#include "pcie.h"

#define BCM_DBG pr_debug

static int gpio_wl_reg_on = 82;
static int brcm_wake_irq = -1;

#ifdef CONFIG_PARTIALRESUME
static bool bcm_wifi_process_partial_resume(int action);
#endif

#ifdef CONFIG_DHD_USE_STATIC_BUF

#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define WLAN_STATIC_DHD_INFO_BUF	7
#define WLAN_STATIC_DHD_IF_FLOW_LKUP    9
#define WLAN_SCAN_BUF_SIZE		(64 * 1024)
#define WLAN_DHD_INFO_BUF_SIZE		(24 * 1024)

#define WLAN_DHD_IF_FLOW_LKUP_SIZE      (20 * 1024)

#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#ifdef CONFIG_BCMDHD_PCIE
#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	0
#define WLAN_SECTION_SIZE_2	0
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)
#else
#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)
#endif /* CONFIG_BCMDHD_PCIE */

#define DHD_SKB_HDRSIZE			336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define DHD_SKB_1PAGE_BUF_NUM	8
#define DHD_SKB_2PAGE_BUF_NUM	8
#define DHD_SKB_4PAGE_BUF_NUM	1

#define WLAN_SKB_1_2PAGE_BUF_NUM	((DHD_SKB_1PAGE_BUF_NUM) + \
		(DHD_SKB_2PAGE_BUF_NUM))
#define WLAN_SKB_BUF_NUM	((WLAN_SKB_1_2PAGE_BUF_NUM) + \
		(DHD_SKB_4PAGE_BUF_NUM))



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

void *wlan_static_scan_buf0 = NULL;
void *wlan_static_scan_buf1 = NULL;
void *wlan_static_dhd_info_buf = NULL;
void *wlan_static_if_flow_lkup = NULL;


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
			pr_err("request DHD_INFO size(%lu) is bigger than static size(%d).\n",
				size, WLAN_DHD_INFO_BUF_SIZE);
			return NULL;
		}
		return wlan_static_dhd_info_buf;
	}
	if (section == WLAN_STATIC_DHD_IF_FLOW_LKUP)  {
		if (size > WLAN_DHD_IF_FLOW_LKUP_SIZE) {
			pr_err("request DHD_IF_FLOW_LKUP size(%lu) is bigger than static size(%d).\n",
				size, WLAN_DHD_IF_FLOW_LKUP_SIZE);
			return NULL;
		}

		return wlan_static_if_flow_lkup;
	}
	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wlan_mem_array[section].size < size)
		return NULL;

	return wlan_mem_array[section].mem_ptr;
}

static int brcm_init_wlan_mem(void)
{
	int i, j;

	for (i = 0; i < DHD_SKB_1PAGE_BUF_NUM; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (i = DHD_SKB_1PAGE_BUF_NUM; i < WLAN_SKB_1_2PAGE_BUF_NUM; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;

	for (i = 0; i < PREALLOC_WLAN_SEC_NUM; i++) {
		if (wlan_mem_array[i].size > 0) {
			wlan_mem_array[i].mem_ptr =
				kmalloc(wlan_mem_array[i].size, GFP_KERNEL);
			if (!wlan_mem_array[i].mem_ptr)
				goto err_mem_alloc;
		}
	}

	wlan_static_scan_buf0 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf0) {
		pr_err("Failed to alloc wlan_static_scan_buf0\n");
		goto err_mem_alloc;
	}

	wlan_static_scan_buf1 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf1) {
		pr_err("Failed to alloc wlan_static_scan_buf1\n");
		goto err_mem_alloc;
	}

	wlan_static_dhd_info_buf = kmalloc(WLAN_DHD_INFO_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_info_buf) {
		pr_err("Failed to alloc wlan_static_dhd_info_buf\n");
		goto err_mem_alloc;
	}
#ifdef CONFIG_BCMDHD_PCIE
	wlan_static_if_flow_lkup = kmalloc(WLAN_DHD_IF_FLOW_LKUP_SIZE, GFP_KERNEL);
	if (!wlan_static_if_flow_lkup) {
		pr_err("Failed to alloc wlan_static_if_flow_lkup\n");
		goto err_mem_alloc;
	}
#endif /* CONFIG_BCMDHD_PCIE */

	return 0;

err_mem_alloc:
	if (wlan_static_dhd_info_buf)
		kfree(wlan_static_dhd_info_buf);

	if (wlan_static_scan_buf1)
		kfree(wlan_static_scan_buf1);

	if (wlan_static_scan_buf0)
		kfree(wlan_static_scan_buf0);

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
#endif /* CONFIG_DHD_USE_STATIC_BUF */


int __init brcm_wifi_init_gpio(struct device_node *np)
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
		pr_err("%s: Faiiled to request gpio %d for WL_REG_ON\n",
			__func__, gpio_wl_reg_on);
	else
		pr_err("%s: gpio_request WL_REG_ON done\n", __func__);

	if (gpio_direction_output(gpio_wl_reg_on, 1))
		pr_err("%s: WL_REG_ON failed to pull up\n", __func__);
	else
		BCM_DBG("%s: WL_REG_ON is pulled up\n", __func__);

	if (gpio_get_value(gpio_wl_reg_on))
		BCM_DBG("%s: Initial WL_REG_ON: [%d]\n",
			__func__, gpio_get_value(gpio_wl_reg_on));

	return 0;
}

int brcm_wlan_power(int on)
{
	BCM_DBG("------------------------------------------------");
	BCM_DBG("------------------------------------------------\n");
	pr_info("%s Enter: power %s\n", __func__, on ? "on" : "off");

	if (on) {
		if (gpio_direction_output(gpio_wl_reg_on, 1)) {
			pr_err("%s: WL_REG_ON didn't output high\n", __func__);
			return -EIO;
		}
		if (!gpio_get_value(gpio_wl_reg_on))
			pr_err("[%s] gpio didn't set high.\n", __func__);
	} else {
		if (gpio_direction_output(gpio_wl_reg_on, 0)) {
			pr_err("%s: WL_REG_ON didn't output low\n", __func__);
			return -EIO;
		}
	}
	return 0;
}
EXPORT_SYMBOL(brcm_wlan_power);

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
	{"",   "XT", 49},  /* Universal if Country code is unknown or empty */
	{"US", "US", 176},
	{"AE", "AE", 1},
	{"AR", "AR", 21},
	{"AT", "AT", 4},
	{"AU", "AU", 40},
	{"BE", "BE", 4},
	{"BG", "BG", 4},
	{"BN", "BN", 4},
	{"BR", "BR", 4},
	{"CA", "US", 176},   /* Previousely was CA/31 */
	{"CH", "CH", 4},
	{"CY", "CY", 4},
	{"CZ", "CZ", 4},
	{"DE", "DE", 7},
	{"DK", "DK", 4},
	{"EE", "EE", 4},
	{"ES", "ES", 4},
	{"FI", "FI", 4},
	{"FR", "FR", 5},
	{"GB", "GB", 6},
	{"GR", "GR", 4},
	{"HK", "HK", 2},
	{"HR", "HR", 4},
	{"HU", "HU", 4},
	{"IE", "IE", 5},
	{"IN", "IN", 28},
	{"IS", "IS", 4},
	{"IT", "IT", 4},
	{"ID", "ID", 5},
	{"JP", "JP", 86},
	{"KR", "KR", 57},
	{"KW", "KW", 5},
	{"LI", "LI", 4},
	{"LT", "LT", 4},
	{"LU", "LU", 3},
	{"LV", "LV", 4},
	{"MA", "MA", 2},
	{"MT", "MT", 4},
	{"MX", "MX", 20},
	{"MY", "MY", 16},
	{"NL", "NL", 4},
	{"NO", "NO", 4},
	{"NZ", "NZ", 4},
	{"PL", "PL", 4},
	{"PT", "PT", 4},
	{"PY", "PY", 2},
	{"RO", "RO", 4},
	{"RU", "RU", 13},
	{"SE", "SE", 4},
	{"SG", "SG", 19},
	{"SI", "SI", 4},
	{"SK", "SK", 4},
	{"TH", "TH", 5},
	{"TR", "TR", 7},
	{"TW", "TW", 1},
	{"VN", "VN", 4},
};

struct cntry_locales_custom brcm_wlan_translate_nodfs_table[] = {
	{"",   "XT", 50},  /* Universal if Country code is unknown or empty */
	{"US", "US", 177},
	{"AU", "AU", 41},
	{"BR", "BR", 18},
	{"CA", "US", 177},
	{"CH", "E0", 33},
	{"CY", "E0", 33},
	{"CZ", "E0", 33},
	{"DE", "E0", 33},
	{"DK", "E0", 33},
	{"EE", "E0", 33},
	{"ES", "E0", 33},
	{"EU", "E0", 33},
	{"FI", "E0", 33},
	{"FR", "E0", 33},
	{"GB", "E0", 33},
	{"GR", "E0", 33},
	{"HK", "SG", 20},
	{"HR", "E0", 33},
	{"HU", "E0", 33},
	{"IE", "E0", 33},
	{"IN", "IN", 29},
	{"ID", "ID", 5},
	{"IS", "E0", 33},
	{"IT", "E0", 33},
	{"JP", "JP", 87},
	{"KR", "KR", 79},
	{"KW", "KW", 5},
	{"LI", "E0", 33},
	{"LT", "E0", 33},
	{"LU", "E0", 33},
	{"LV", "LV", 4},
	{"MA", "MA", 2},
	{"MT", "E0", 33},
	{"MY", "MY", 17},
	{"MX", "US", 177},
	{"NL", "E0", 33},
	{"NO", "E0", 33},
	{"PL", "E0", 33},
	{"PT", "E0", 33},
	{"RO", "E0", 33},
	{"RU", "RU", 50},
	{"SE", "E0", 33},
	{"SG", "SG", 20},
	{"SI", "E0", 33},
	{"SK", "E0", 33},
	{"SZ", "E0", 33},
	{"TH", "TH", 9},
	{"TW", "TW", 60},
};

struct cntry_locales_custom brcm_wlan_translate_ap_table[] = {
	{"JP", "JP", 991},
};

static void *brcm_wlan_get_country_code(char *ccode, u32 flags)
{
	struct cntry_locales_custom *locales;
	int size;
	int i;

	if (!ccode)
		return NULL;

	if (flags & WLAN_PLAT_AP_FLAG) {
		locales = brcm_wlan_translate_ap_table;
		size = ARRAY_SIZE(brcm_wlan_translate_ap_table);
		for (i = 0; i < size; i++)
			if (strcmp(ccode, locales[i].iso_abbrev) == 0)
				return &locales[i];
	}

	if (flags & WLAN_PLAT_NODFS_FLAG) {
		locales = brcm_wlan_translate_nodfs_table;
		size = ARRAY_SIZE(brcm_wlan_translate_nodfs_table);
	} else {
		locales = brcm_wlan_translate_custom_table;
		size = ARRAY_SIZE(brcm_wlan_translate_custom_table);
	}

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

static int brcm_wlan_get_wake_irq(void)
{
	return brcm_wake_irq;
}

static struct resource brcm_wlan_resources[] = {
	[0] = {
		.name	= "bcmdhd_wlan_irq",
		.start	= 0, /* Dummy */
		.end	= 0, /* Dummy */
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE
			| IORESOURCE_IRQ_HIGHLEVEL, /* Dummy */
	},
};

static struct wifi_platform_data brcm_wlan_control = {
	.set_power	= brcm_wlan_power,
	.set_reset	= brcm_wlan_reset,
	.set_carddetect	= brcm_wlan_set_carddetect,
	.get_mac_addr = brcm_wifi_get_mac_addr,
#ifdef CONFIG_DHD_USE_STATIC_BUF
	.mem_prealloc	= brcm_wlan_mem_prealloc,
#endif
	.get_wake_irq	= brcm_wlan_get_wake_irq,
	.get_country_code = brcm_wlan_get_country_code,
#ifdef CONFIG_PARTIALRESUME
	.partial_resume = bcm_wifi_process_partial_resume,
#endif
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
	int rc;
	struct device_node *np;

	BCM_DBG("%s: START\n", __func__);

#ifdef CONFIG_DHD_USE_STATIC_BUF
	brcm_init_wlan_mem();
#endif
	rc = platform_device_register(&brcm_device_wlan);

	np = of_find_compatible_node(NULL, NULL, "bcm,bcm4356");
	brcm_device_wlan.dev.of_node = np;
	brcm_wake_irq = gpio_to_irq(of_get_named_gpio(np, "wl_host_wake", 0));

	brcm_wifi_init_gpio(np);
	return rc;
}
subsys_initcall(brcm_wlan_init);

#ifdef CONFIG_PARTIALRESUME
static int smd_partial_resume(struct partial_resume *pr)
{
	return 0;
}

#define PR_INIT_STATE          0
#define PR_IN_RESUME_STATE     1
#define PR_RESUME_OK_STATE     2
#define PR_SUSPEND_OK_STATE    3

static DECLARE_COMPLETION(bcm_comp);
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
		timeout = wait_for_completion_timeout(&bcm_comp,
						      msecs_to_jiffies(50));

	spin_lock(&bcm_lock);
	switch (action) {
	case WIFI_PR_WAIT_FOR_READY:
		suspend = (bcm_suspend == PR_SUSPEND_OK_STATE) && (timeout != 0);
		bcm_suspend = PR_INIT_STATE;
		break;
	case WIFI_PR_VOTE_FOR_RESUME:
		bcm_suspend = PR_RESUME_OK_STATE;
		complete(&bcm_comp);
		break;
	case WIFI_PR_VOTE_FOR_SUSPEND:
		if (bcm_suspend == PR_IN_RESUME_STATE)
			bcm_suspend = PR_SUSPEND_OK_STATE;
		complete(&bcm_comp);
		break;
	case WIFI_PR_NOTIFY_RESUME:
		INIT_COMPLETION(bcm_comp);
		bcm_suspend = PR_IN_RESUME_STATE;
		break;
	case WIFI_PR_INIT:
		bcm_suspend = PR_INIT_STATE;
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

static struct partial_resume smd_pr = {
	.irq = 200,
	.partial_resume = smd_partial_resume,
};

static struct partial_resume wlan_pr = {
	.partial_resume = bcm_wifi_partial_resume,
};

int __init wlan_partial_resume_init(void)
{
	int rc;

	/* Setup partial resume */
	spin_lock_init(&bcm_lock);
	wlan_pr.irq = brcm_wake_irq;
	printk("%s: Wlan wake IRQ = %d\n", __func__, wlan_pr.irq);
	rc = register_partial_resume(&wlan_pr);
	rc = register_partial_resume(&smd_pr);
	return rc;
}

late_initcall(wlan_partial_resume_init);
#endif
