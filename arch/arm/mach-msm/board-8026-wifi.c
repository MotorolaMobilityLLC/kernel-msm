#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <mach/gpio.h>
#include <linux/gpio.h>
#include <linux/random.h>
#include <linux/ctype.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>

#define GPIO_WL_HOST_WAKE 66
#define GPIO_WL_REG_ON 110

/* for wifi mac address custom */
#define TEMP_BUFFER_LEN 3
#define ETHER_ADDR_LEN 6
#define ETHER_STR_LEN 12
#define ETHER_BUFFER_LEN 16

static int power_enabled = 0;
char g_wlan_ether_addr[] = {0x00,0x00,0x00,0x00,0x00,0x00};

typedef enum
{
	WLAN_BASE10		=	10,
	WLAN_BASE16		=	16,
}WLAN_BASE_TYPE;


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
	static struct regulator *reg_batfet = NULL;
	int ret = 0;

	printk(KERN_INFO"%s Enter: power %s\n", __func__, onoff ? "on" : "off");

	if (!reg_batfet) {
		reg_batfet = regulator_get(NULL, "batfet");
		if (IS_ERR(reg_batfet)) {
			pr_warn("%s: failed to get regualtor(batfet)\n",
						__func__);
			reg_batfet = NULL;
		}
	}

	onoff = !!onoff;
	if (!(power_enabled ^ onoff)) {
		printk(KERN_INFO"%s: power already %s\n", __func__, onoff ? "on" : "off");
		return 0;
	}

	if (onoff) {
		if (gpio_direction_output(GPIO_WL_REG_ON, 1)) {
			printk(KERN_ERR "%s: WL_REG_ON  failed to pull up\n",
				__func__);
			return -EIO;
		}

		power_enabled = 1;

		if (reg_batfet) {
			ret = regulator_enable(reg_batfet);
			if (ret)
			pr_warn("%s: failed to enable regulator(batfet)\n",
						__func__);
		}
	} else {
		power_enabled = 0;

		if (reg_batfet) {
			ret = regulator_disable(reg_batfet);
			if (ret)
				pr_warn("%s: failed to disable regulator(batfet)\n",
						__func__);
			}

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

int wlan_strtoi(const char *nptr, const char **endptr, WLAN_BASE_TYPE base)
{
	int ret = 0;
	if (NULL == nptr)
	{
		return ret;
	}

	while (isspace(*nptr))
	{
		nptr++;
	}

	while ((isdigit(*nptr) && (WLAN_BASE10 == base || WLAN_BASE16 == base))
		|| (isalpha(*nptr) && (WLAN_BASE16 == base)))
	{
		if (isdigit(*nptr))
		{
			ret = (ret * base) + (*nptr - '0');
		}
		else
		{
			if (isupper(*nptr))
			{
				ret = (ret * base) + (*nptr - 'A');
			}
			else
			{
				ret = (ret * base) + (*nptr - 'a');
			}
			ret += WLAN_BASE10;
		}
		nptr++;
	}

	if (NULL != endptr)
	{
		*endptr = nptr;
	}
	return ret;
}

static ssize_t wifi_mac_read(struct file *file, char __user *userbuf,
							size_t bytes, loff_t *off)
{
	return 0;
}

static ssize_t wifi_mac_write(struct file *file, const char __user *buffer,
							size_t count, loff_t *pos)
{
		int i = 0;
		char temp[TEMP_BUFFER_LEN];
		char temp_buf[ETHER_BUFFER_LEN] = {0};

		if (count < ETHER_STR_LEN)
		{
			return -EINVAL;
		}

		if (copy_from_user(temp_buf, buffer, ETHER_STR_LEN))
		{
			return -EFAULT;
		}

		for (i = 0; i < ETHER_STR_LEN; i++)
		{
			if (((temp_buf[i] < '0')|| (temp_buf[i] > '9')) &&
				((temp_buf[i] < 'a') || (temp_buf[i] > 'f')) &&
				((temp_buf[i] < 'A') || (temp_buf[i] > 'F')))
			{
				pr_err("wlan:invalid mac address\n");
				return -EINVAL;
			}
		}

		for (i = 0; i < ETHER_ADDR_LEN; i++)
		{
			memset(temp, 0, TEMP_BUFFER_LEN);
			memcpy(temp, temp_buf + 2*i, TEMP_BUFFER_LEN-1);
			g_wlan_ether_addr[i] = (char)wlan_strtoi(temp, NULL, WLAN_BASE16);
		}

	return count;
}


static const struct file_operations proc_fops_wifi_mac = {
	.owner = THIS_MODULE,
	.read = wifi_mac_read,
	.write = wifi_mac_write,
};

int __init brcm_wlan_proc_create(void)
{
	int retval = 0;
	struct proc_dir_entry *ent;
	struct proc_dir_entry *wifi_dir;

	wifi_dir = proc_mkdir("wifi", NULL);
	if (wifi_dir == NULL)
	{
		pr_err("Unable to create /proc/wifi directory.\n");
		return -ENOMEM;
	}

	/* read/write wifi mac entries */
	ent = proc_create("mac", 0660, wifi_dir, &proc_fops_wifi_mac);
	if (ent == NULL)
	{
		pr_err("Unable to create /proc/wifi/mac entry.\n");
		retval = -ENOMEM;
		goto fail;
	}
	return retval;

fail:
	remove_proc_entry("wifi", 0);
	return retval;

}

int brcm_wlan_get_mac_addr(unsigned char *buf)
{
	const char null_addr[] = {0x00,0x00,0x00,0x00,0x00,0x00};

	if (NULL == buf)
	{
		return -1;
	}

	if (0 == memcmp(g_wlan_ether_addr, null_addr, ETHER_ADDR_LEN))
	{
		get_random_bytes(buf, ETHER_ADDR_LEN);
		buf[0] = 0x00;
		return 0;
	}

	memcpy(buf, g_wlan_ether_addr, ETHER_ADDR_LEN);

	printk(KERN_INFO"%s:MAC:%02X:%02X:%02X:%02X:%02X:%02X\n", __func__,
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

	return 0;
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
	.get_mac_addr = brcm_wlan_get_mac_addr,
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
    brcm_wlan_proc_create();


	return platform_device_register(&brcm_device_wlan);
}
