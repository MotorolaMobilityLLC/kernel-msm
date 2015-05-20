/*
 * Copyright (C) 2012-2014 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/rtc.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/m4sensorhub/MemMapLog.h>
#include <linux/m4sensorhub.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/m4sensorhub_notify.h>

#define M4SENSORHUB_NUM_GPIOS       6

/* --------------- Global Declarations -------------- */
char m4sensorhub_debug;
EXPORT_SYMBOL_GPL(m4sensorhub_debug);

/* ------------ Local Function Prototypes ----------- */

/* -------------- Local Data Structures ------------- */
static struct miscdevice m4sensorhub_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = M4SENSORHUB_DRIVER_NAME,
};

struct init_call {
	int(*initcb)(struct init_calldata *);
	void *pdata;
	struct init_call *next;
};

/* --------------- Local Declarations -------------- */
static struct m4sensorhub_data m4sensorhub_misc_data;
static struct init_call *inithead;
static struct init_call *preflash_head;
static char tcmd_exec_status;

unsigned short force_upgrade;
module_param(force_upgrade, short, 0644);
MODULE_PARM_DESC(force_upgrade, "Force FW download ignoring version check");

unsigned short debug_level;
module_param(debug_level, short, 0644);
MODULE_PARM_DESC(debug_level,
	"Set debug level 1 (CRITICAL) to 7 (VERBOSE_DEBUG)");

/* -------------- Global Functions ----------------- */
struct m4sensorhub_data *m4sensorhub_client_get_drvdata(void)
{
	return &m4sensorhub_misc_data;
}
EXPORT_SYMBOL_GPL(m4sensorhub_client_get_drvdata);


/* -------------- Local Functions ----------------- */

static ssize_t m4sensorhub_get_dbg(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", m4sensorhub_debug);
}

/* BEGIN BOARD FILE */
/* TODO: replace with request array */

int m4sensorhub_set_bootmode(struct m4sensorhub_data *m4sensorhub,
			 enum m4sensorhub_bootmode bootmode)
{
	if (m4sensorhub == NULL) {
		pr_err("%s: M4 data is NULL\n", __func__);
		return -EINVAL;
	}

	switch (bootmode) {
	case BOOTMODE0_HIGH:
		gpio_set_value(m4sensorhub->hwconfig.boot0_gpio, 1);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(m4sensorhub_set_bootmode);

void m4sensorhub_hw_reset(struct m4sensorhub_data *m4sensorhub)
{
	int err = 0;

	if (m4sensorhub == NULL) {
		pr_err("%s: M4 data is NULL\n", __func__);
		err = -ENODATA;
		goto m4sensorhub_hw_reset_fail;
	} else if (m4sensorhub->i2c_client == NULL) {
		pr_err("%s: I2C client is missing\n", __func__);
		err = -ENODATA;
		goto m4sensorhub_hw_reset_fail;
	}

	err = m4sensorhub_set_bootmode(m4sensorhub, BOOTMODE0_HIGH);
	if (err < 0) {
		pr_err("%s: Failed to enter bootmode 01\n", __func__);
		goto m4sensorhub_hw_reset_fail;
	}
	usleep_range(5000, 10000);
	/* if we haven't acquired the gpio.. its not low
	 to drive it low, acquire it */
	if (m4sensorhub->hwconfig.reset_acquired == false) {
		err = gpio_request(m4sensorhub->hwconfig.reset_gpio,
							"m4sensorhub-reset");
		if (err) {
			pr_err("Failed acquiring M4 Sensor Hub Reset GPIO-%d (%d)\n",
			       m4sensorhub->hwconfig.reset_gpio, err);
			goto m4sensorhub_hw_reset_fail;
		}
		/* hold M4 reset till M4 load firmware procduce starts
		* this is needed for snowflake touch determination
		*/
		gpio_direction_output(m4sensorhub->hwconfig.reset_gpio, 0);
		m4sensorhub->hwconfig.reset_acquired = true;
	}

	gpio_set_value(m4sensorhub->hwconfig.reset_gpio, 0);
	usleep_range(10000, 12000);
	/* release GPIO so that padconf setting takes over*/
	gpio_free(m4sensorhub->hwconfig.reset_gpio);
	m4sensorhub->hwconfig.reset_acquired = false;
	msleep(600);

m4sensorhub_hw_reset_fail:
	if (err < 0)
		pr_err("%s: Failed with error code %d", __func__, err);
}
EXPORT_SYMBOL_GPL(m4sensorhub_hw_reset);


/* callback from driver to initialize hardware on probe */
static int m4sensorhub_hw_init(struct m4sensorhub_data *m4sensorhub,
		struct device_node *node)
{
	int gpio;
	int err = -EINVAL;
	const char *fp = NULL;

	if (m4sensorhub == NULL) {
		pr_err("%s: M4 data is NULL\n", __func__);
		err = -ENODATA;
		goto error;
	} else if (node == NULL) {
		pr_err("%s: Device node is missing\n", __func__);
		err = -ENODATA;
		goto error;
	}

	of_property_read_string(node, "mot,fw-filename", &fp);
	if (fp == NULL) {
		pr_err("%s: Missing M4 sensorhub firmware filename\n",
			__func__);
		err = -EINVAL;
		goto error;
	}
	m4sensorhub->filename = (char *)fp;

	gpio = of_get_named_gpio_flags(node, "mot,wakeirq-gpio", 0, NULL);
	err = (gpio < 0) ? -ENODEV : gpio_request(gpio, "m4sensorhub-wakeintr");
	if (err) {
		pr_err("Failed acquiring M4 Sensor Hub wake IRQ GPIO-%d (%d)\n",
			gpio, err);
		goto error;
	}
	gpio_direction_input(gpio);
	m4sensorhub->hwconfig.wakeirq_gpio = gpio;


        gpio = of_get_named_gpio_flags(node, "mot,nowakeirq-gpio", 0, NULL);
        err = (gpio < 0) ? -ENODEV : gpio_request(gpio, "m4sensorhub-nowakeintr");
        if (err) {
                pr_err("Failed acquiring M4 Sensor Hub nowake IRQ GPIO-%d (%d)\n",
                        gpio, err);
                goto error_nowakeirq;
        }
        gpio_direction_input(gpio);
        m4sensorhub->hwconfig.nowakeirq_gpio = gpio;


	gpio = of_get_named_gpio_flags(node, "mot,reset-gpio", 0, NULL);
	err = (gpio < 0) ? -ENODEV : gpio_request(gpio, "m4sensorhub-reset");
	if (err) {
		pr_err("Failed acquiring M4 Sensor Hub Reset GPIO-%d (%d)\n",
			gpio, err);
		goto error_reset;
	}
	/* hold M4 reset till M4 load firmware procduce starts
	 * this is needed for snowflake touch determination
	 */
	gpio_direction_output(gpio, 0);
	m4sensorhub->hwconfig.reset_gpio = gpio;
	m4sensorhub->hwconfig.reset_acquired = true;

	gpio = of_get_named_gpio_flags(node, "mot,boot0-gpio", 0, NULL);
	err = (gpio < 0) ? -ENODEV : gpio_request(gpio, "m4sensorhub-boot0");
	if (err) {
		pr_err("Failed acquiring M4 Sensor Hub Boot0 GPIO-%d (%d)\n",
			gpio, err);
		goto error_boot0;
	}
	gpio_direction_output(gpio, 0);
	m4sensorhub->hwconfig.boot0_gpio = gpio;

	return 0;
error_boot0:
	gpio_free(m4sensorhub->hwconfig.reset_gpio);
	m4sensorhub->hwconfig.reset_acquired = false;
	m4sensorhub->hwconfig.reset_gpio = -1;
error_reset:
	gpio_free(m4sensorhub->hwconfig.nowakeirq_gpio);
	m4sensorhub->hwconfig.nowakeirq_gpio = -1;
error_nowakeirq:
        gpio_free(m4sensorhub->hwconfig.wakeirq_gpio);
        m4sensorhub->hwconfig.wakeirq_gpio = -1;
error:
	m4sensorhub->filename = NULL;
	return err;
}

/* callback from driver to free hardware on shutdown */
static void m4sensorhub_hw_free(struct m4sensorhub_data *m4sensorhub)
{

	if (m4sensorhub == NULL) {
		pr_err("%s: M4 data is NULL\n", __func__);
		return;
	}

	if (m4sensorhub->hwconfig.nowakeirq_gpio >= 0) {
		gpio_free(m4sensorhub->hwconfig.nowakeirq_gpio);
		m4sensorhub->hwconfig.nowakeirq_gpio = -1;
	}

        if (m4sensorhub->hwconfig.wakeirq_gpio >= 0) {
                gpio_free(m4sensorhub->hwconfig.wakeirq_gpio);
                m4sensorhub->hwconfig.wakeirq_gpio = -1;
        }

	if (m4sensorhub->hwconfig.reset_gpio >= 0) {
		gpio_free(m4sensorhub->hwconfig.reset_gpio);
		m4sensorhub->hwconfig.reset_gpio = -1;
	}

	if (m4sensorhub->hwconfig.boot0_gpio >= 0) {
		gpio_free(m4sensorhub->hwconfig.boot0_gpio);
		m4sensorhub->hwconfig.boot0_gpio = -1;
	}

	m4sensorhub->filename = NULL;
}

int m4sensorhub_register_initcall(int(*initfunc)(struct init_calldata *),
								void *pdata)
{
	struct init_call *inc = NULL;

	inc = kzalloc(sizeof(struct init_call), GFP_KERNEL);
	if (inc == NULL) {
		KDEBUG(M4SH_ERROR, "%s: Unable to allocate init call mem\n",
			__func__);
		return -ENOMEM;
	}

	inc->initcb = initfunc;
	inc->pdata = pdata;
	/* add it to the list */
	if (inithead == NULL)
		inc->next = NULL;
	else
		inc->next = inithead;

	inithead = inc;
	return 0;
}
EXPORT_SYMBOL_GPL(m4sensorhub_register_initcall);

void m4sensorhub_unregister_initcall(int(*initfunc)(struct init_calldata *))
{
	struct init_call *node = inithead;
	struct init_call *prev;

	for (node = inithead, prev = NULL;
		node != NULL;
		prev = node, node = node->next) {
		if (node->initcb == initfunc) {
			/* remove this node */
			if (node == inithead)
				inithead = node->next;
			else
				prev->next = node->next;
			kfree(node);
		}
	}
}
EXPORT_SYMBOL_GPL(m4sensorhub_unregister_initcall);

int m4sensorhub_register_preflash_callback(
		int(*initfunc)(struct init_calldata *), void *pdata)
{
	struct init_call *inc = NULL;

	inc = kzalloc(sizeof(struct init_call), GFP_KERNEL);
	if (inc == NULL) {
		KDEBUG(M4SH_ERROR, "%s: Unable to allocate init call mem\n",
			__func__);
		return -ENOMEM;
	}

	inc->initcb = initfunc;
	inc->pdata = pdata;
	/* add it to the list */
	if (preflash_head == NULL)
		inc->next = NULL;
	else
		inc->next = preflash_head;

	preflash_head = inc;
	return 0;
}
EXPORT_SYMBOL_GPL(m4sensorhub_register_preflash_callback);

void m4sensorhub_unregister_preflash_callback(
		int(*initfunc)(struct init_calldata *))
{
	struct init_call *node = preflash_head;
	struct init_call *prev;

	for (node = preflash_head, prev = NULL;
		node != NULL;
		prev = node, node = node->next) {
		if (node->initcb == initfunc) {
			/* remove this node */
			if (node == preflash_head)
				preflash_head = node->next;
			else
				prev->next = node->next;
			kfree(node);
		}
	}
}
EXPORT_SYMBOL_GPL(m4sensorhub_unregister_preflash_callback);

void m4sensorhub_call_preflash_callbacks(void)
{
	struct init_calldata arg;
	struct init_call *inc = NULL;
	struct init_call *prev = NULL;
	int err = 0;

	/* Call any registered preflash callbacks */
	inc = preflash_head;
	arg.p_m4sensorhub_data = &m4sensorhub_misc_data;
	prev = NULL;
	while (inc) {
		arg.p_data = inc->pdata;
		err = inc->initcb(&arg);
		if (err < 0) {
			KDEBUG(M4SH_ERROR,
				"%s: Callback failed with error code %d\n",
				__func__, err);
		}
		prev = inc;
		inc = inc->next;
		kfree(prev);
	}

	return;
}
EXPORT_SYMBOL_GPL(m4sensorhub_call_preflash_callbacks);

bool m4sensorhub_preflash_callbacks_exist(void)
{
	if (preflash_head != NULL)
		return true;
	else
		return false;
}
EXPORT_SYMBOL_GPL(m4sensorhub_preflash_callbacks_exist);
/* END BOARD FILE FUNCTIONS */

/* Downloads m4 firmware and also initializes all m4 drivers */
static void m4sensorhub_initialize(const struct firmware *firmware,
					void *context)
{
	int err = 0;
	struct init_call *inc, *prev;
	struct init_calldata arg;

	if (firmware == NULL) {
		KDEBUG(M4SH_ERROR, "%s: No firmware data recieved\n",
			__func__);
		return;
	}

	/* Initiate M4 firmware download */
	KDEBUG(M4SH_CRITICAL, "%s: Starting M4 download %s = %d\n",
		__func__, "with force_upgrade", force_upgrade);
	err = m4sensorhub_l4_load_firmware(&m4sensorhub_misc_data,
			force_upgrade, firmware);

	if (err < 0) {
		KDEBUG(M4SH_ERROR, "%s: %s = %d\n",
			__func__, "Failed to load M4 firmware", err);
		/* Since download failed, return M4 to boot mode */
		m4sensorhub_hw_reset(&m4sensorhub_misc_data);
		panic("%s: forcing panic...\n", __func__);
		return;
	}

	err = m4sensorhub_test_m4_reboot(&m4sensorhub_misc_data, false);
	if (err < 0) {
		/* Getting here is unlikely because failures normally panic */
		KDEBUG(M4SH_ERROR, "%s: Testing M4 reboot failed.\n", __func__);
		return;
	}

	err = m4sensorhub_irq_init(&m4sensorhub_misc_data);
	if (err < 0) {
		KDEBUG(M4SH_ERROR, "%s: m4sensorhub irq init failed (err=%d)\n",
			__func__, err);
		return;
	}

	/* Initialize all the m4 drivers */
	inc = inithead;
	arg.p_m4sensorhub_data = &m4sensorhub_misc_data;
	prev = NULL;
	while (inc) {
		arg.p_data = inc->pdata;
		err = inc->initcb(&arg);
		if (err < 0) {
			KDEBUG(M4SH_ERROR,
				"%s: Callback failed with error code %d\n",
				__func__, err);
		}
		prev = inc;
		inc = inc->next;
		kfree(prev);
	}

	/* Now that all drivers are kicked off, flag this
	as our normal mode of operation */
	m4sensorhub_misc_data.mode = NORMALMODE;
}

static ssize_t m4sensorhub_set_dbg(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long debug;

	if ((kstrtol(buf, 10, &debug) < 0) ||
	    (debug < M4SH_NODEBUG) || (debug > M4SH_VERBOSE_DEBUG))
		return -EINVAL;

	m4sensorhub_debug = debug;
	KDEBUG(M4SH_CRITICAL, "%s: M4 Sensor Hub debug level = %d\n",
		__func__, m4sensorhub_debug);

	return count;
}

static DEVICE_ATTR(debug_level, S_IRUSR|S_IWUSR, m4sensorhub_get_dbg,
		m4sensorhub_set_dbg);

static ssize_t m4sensorhub_get_loglevel(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned long long loglevel;

	m4sensorhub_reg_read(&m4sensorhub_misc_data,
		M4SH_REG_LOG_LOGENABLE, (char *)&loglevel);
	KDEBUG(M4SH_INFO, "M4 loglevel = %llx", loglevel);
	return sprintf(buf, "%llu\n", loglevel);
}
int m4sensorhub_update_loglevels(char *tag, char *level,
			uint32_t *log_levels)
{
	int32_t levelindex = -1, tagindex = -1, i;
	uint32_t mask, array_index;
	
	for (i = 0; i < LOG_LEVELS_MAX; i++) {
		if (strcmp(acLogLevels[i], level) == 0) {
			levelindex = i;
			break;
		}
	}

	for (i = 0; i < LOG_MAX; i++) {
		if (strcmp(acLogTags[i], tag) == 0) {
			tagindex = i;
			break;
		}
	}

	if ((tagindex == -1) || (levelindex == -1))
		return 0;

	array_index = (tagindex / LOG_TAGS_PER_ENABLE);

	/*Clear the revelant bits*/
	mask = LOG_TAG_MASK;
	*(log_levels + array_index) &= ~(mask << ((tagindex % LOG_TAGS_PER_ENABLE) * LOG_NO_OF_BITS_PER_TAG));
	/*set debug level for the relevant bits*/
	*(log_levels + array_index) |= (levelindex << ((tagindex % LOG_TAGS_PER_ENABLE) * 2));
	KDEBUG(M4SH_INFO, "New M4 log levels = 0x%x 0x%x\n", *(log_levels), *(log_levels+1));

	return 1;
}

/* Usage: adb shell into the directory of sysinterface log_level and
   echo LOG_ACCEL=LOG_DEGUB > log_level */
static ssize_t m4sensorhub_set_loglevel(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	uint32_t cur_loglevels[LOG_EN_SIZE] = {0};
	char *tag, *level;
	char **logbuf = (char **) &buf;
	int exitcount =  20;

	m4sensorhub_reg_read(&m4sensorhub_misc_data,
		M4SH_REG_LOG_LOGENABLE, (char *)cur_loglevels);

	while (1) {
		tag = strsep(logbuf, "=,\n ");
		if (tag == NULL)
			break;
		level = strsep(logbuf, "=,\n ");
		if (level == NULL)
			break;

	
		if (m4sensorhub_update_loglevels(tag, level, cur_loglevels) == 1) {
			break;
		}
		exitcount--;
		if (exitcount == 0)
			break;
	}

	m4sensorhub_reg_write(&m4sensorhub_misc_data,
		M4SH_REG_LOG_LOGENABLE, (unsigned char *)cur_loglevels,
		m4sh_no_mask);

	return count;
}

static DEVICE_ATTR(log_level, S_IRUSR|S_IWUSR, m4sensorhub_get_loglevel,
		m4sensorhub_set_loglevel);

static ssize_t m4sensorhub_get_tcmd_response(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (tcmd_exec_status)
		return sprintf(buf, "TCMD execution passed\n");
	else
		return sprintf(buf, "TCMD execution failed\n");
}

static ssize_t m4sensorhub_execute_tcmd(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int opcode, subopcode;
	int ret, tcmd_resp_len, i;
	char tcmd_buf[20];
	tcmd_exec_status = 0;

	sscanf(buf, "0x%x 0x%x 0x%x", &opcode, &subopcode, &tcmd_resp_len);
	tcmd_buf[0] = M4SH_TYPE_TCMD;
	tcmd_buf[1] = (opcode & 0xFF);
	tcmd_buf[2] = (subopcode & 0xFF);
	ret = m4sensorhub_i2c_write_read(&m4sensorhub_misc_data,
			tcmd_buf, 3, tcmd_resp_len);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "m4sensorhub tcmd i2c failed\n");
		return ret;
	}
	if (ret != tcmd_resp_len) {
		KDEBUG(M4SH_ERROR, "m4sensorhub tcmd wrong num bytes read\n");
		return -EBADE;
	}
	for (i = 0; i < tcmd_resp_len; i++)
		KDEBUG(M4SH_INFO, "%#x ", (unsigned char)tcmd_buf[i]);
	KDEBUG(M4SH_INFO, "\n");

	if (tcmd_buf[0] == 0x00)
		tcmd_exec_status = 1;

	return count;
}
static DEVICE_ATTR(tcmd, S_IRUSR|S_IWUSR, m4sensorhub_get_tcmd_response,
		m4sensorhub_execute_tcmd);

static ssize_t m4sensorhub_get_download_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n",
		m4sensorhub_misc_data.mode == NORMALMODE ? "1" : "0");
}

static DEVICE_ATTR(download_status, S_IRUGO,
				m4sensorhub_get_download_status, NULL);

static ssize_t m4sensorhub_get_firmware_version(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%#hx\n",
		m4sensorhub_misc_data.mode == NORMALMODE ?
				m4sensorhub_misc_data.fw_version : 0xFFFF);
}

static DEVICE_ATTR(firmware_version, S_IRUGO,
				m4sensorhub_get_firmware_version, NULL);

static ssize_t m4sensorhub_disable_interrupts(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;

	ret = m4sensorhub_irq_disable_all(&m4sensorhub_misc_data);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "%s: Unable to disable all m4 interrupts\n",
			__func__);
		return ret;
	}
	return count;
}
static DEVICE_ATTR(disable_interrupts, S_IWUSR, NULL,
			m4sensorhub_disable_interrupts);

/*
This sysfs is to be used only to disable power management. It
cannot be used to enable power management
*/
static ssize_t m4sensorhub_disable_powermanagement(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char enable = 0;

	return m4sensorhub_reg_write_1byte(&m4sensorhub_misc_data,
		M4SH_REG_POWER_ENABLE, enable, 0xFF);
}
static DEVICE_ATTR(disable_powermanagement, S_IWUSR, NULL,
		m4sensorhub_disable_powermanagement);

/*
This sysfs can be used to read/write m4 registers
This is provided to help in debugging m4 issues
Example to read an m4 register interrupt enable1( bank = 0x02, regaddr = 0x0B)
echo R 0x02 0x0B 0x01 > raw_i2c
R -- indicates read transaction
0x02 -- reg bank address
0x0B -- register address
0x01 -- number of bytes to be read
The result of the read will be printed out in kernel logs

To write to a m4 regiser interrupt enable1( bank = 0x02, regaddr = 0x0B)
echo W 0x02 0x0B 0xFF > raw_i2c
W -- indicates write transaction
0x02 -- reg bank address
0x0b -- register addr
0xFF -- data to be written to this address
Note:
Write trasactions have a limitation to be able to write only 1 byte
*/
static ssize_t m4sensorhub_raw_i2c(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned char buffer[20];
	unsigned int bank, addr, len;
	unsigned char operation;
	int ret, i;

	sscanf(buf, "%c 0x%x 0x%x 0x%x", &operation, &bank, &addr, &len);
	if (operation == 'R') {
		KDEBUG(M4SH_INFO, "Reading %d bytes from bank 0x%x add 0x%x\n",
		       len, bank, addr);
		buffer[0] = (bank & 0xFF);
		buffer[1] = (addr & 0xFF);
		ret = m4sensorhub_i2c_write_read(&m4sensorhub_misc_data,
						 buffer, 2, len);
		if (ret != len) {
			KDEBUG(M4SH_ERROR,
				"Failed to read from bank 0x%x addr 0x%x",
				bank, addr);
		} else {
			for (i = 0; i < len; i++)
				KDEBUG(M4SH_INFO, "0x%x ",
				       (unsigned char)buffer[i]);
			KDEBUG(M4SH_INFO, "\n");
		}
	} else if (operation == 'W') {
		KDEBUG(M4SH_INFO, "Writing 0x%x to bank 0x%x addr 0x%x\n",
		       len, bank, addr);
		buffer[0] = (bank & 0xFF);
		buffer[1] = (addr & 0xFF);
		buffer[2] = (len & 0xFF);
		ret = m4sensorhub_i2c_write_read(&m4sensorhub_misc_data,
						 buffer, 3, 0);
		if (ret != 1) {
			KDEBUG(M4SH_ERROR,
				"Failed to write 0x%x from bank 0x%x addr 0x%x",
				len, bank, addr);
		}
	} else {
		KDEBUG(M4SH_ERROR, "Unknown operation = %c", operation);
	}

	return count;
}
static DEVICE_ATTR(raw_i2c, S_IWUSR, NULL, m4sensorhub_raw_i2c);

static struct attribute *m4sensorhub_control_attributes[] = {
	&dev_attr_tcmd.attr,
	&dev_attr_log_level.attr,
	&dev_attr_debug_level.attr,
	&dev_attr_firmware_version.attr,
	&dev_attr_download_status.attr,
	&dev_attr_disable_interrupts.attr,
	&dev_attr_disable_powermanagement.attr,
	&dev_attr_raw_i2c.attr,
	NULL
};

static const struct attribute_group m4sensorhub_control_group = {
	.attrs = m4sensorhub_control_attributes,
};

static int m4sensorhub_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct m4sensorhub_data *m4sensorhub = &m4sensorhub_misc_data;
	struct device_node *node = client->dev.of_node;
	int err = -EINVAL;

	/* Set debug based on module argument if set, otherwise use
	   default logging rate based on build type */
	if (debug_level)
		m4sensorhub_debug = debug_level;
	else {
#ifdef CONFIG_DEBUG_FS
		/* engineering build */
		m4sensorhub_debug = M4SH_INFO;
#else
		/* user/userdebug builds */
		m4sensorhub_debug = M4SH_ERROR;
#endif
	}

	/* Enabling detailed level M4 logs for all builds*/
	m4sensorhub_debug = M4SH_INFO;
	KDEBUG(M4SH_ERROR, "%s: Initializing M4 Sensor Hub debug=%d\n",
			__func__, m4sensorhub_debug);

	m4sensorhub->mode = UNINITIALIZED;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		KDEBUG(M4SH_ERROR, "%s: client not i2c capable\n", __func__);
		err = -ENODEV;
		goto err_unload;
	}

	/* link m4sensorhub to i2c_client, hw_init uses i2c_client */
	m4sensorhub->i2c_client = client;

	err = m4sensorhub_hw_init(m4sensorhub, node);
	if (err < 0) {
		KDEBUG(M4SH_ERROR, "%s: hw_init failed!", __func__);
		goto done;
	}

	/* link i2c_client to m4sensorhub */
	i2c_set_clientdata(client, m4sensorhub);

	err = misc_register(&m4sensorhub_misc_device);
	if (err < 0) {
		KDEBUG(M4SH_ERROR, "%s: misc_register failed: %d\n",
			__func__, err);
		goto err_hw_free;
	}

	err = sysfs_create_group(&client->dev.kobj, &m4sensorhub_control_group);
	if (err < 0) {
		KDEBUG(M4SH_ERROR, "%s: Failed to create sysfs group\n",
			__func__);
		goto err_deregister;
	}

	if (m4sensorhub->hwconfig.wakeirq_gpio >= 0) {
		client->irq = gpio_to_irq(m4sensorhub->hwconfig.wakeirq_gpio);
		m4sensorhub->hwconfig.wakeirq = client->irq;
	} else {
		KDEBUG(M4SH_ERROR, "%s: No IRQ configured\n", __func__);
		err = -ENODEV;
		goto err_unregister_control_group;
	}

        if (m4sensorhub->hwconfig.nowakeirq_gpio >= 0)
                 m4sensorhub->hwconfig.nowakeirq =
			gpio_to_irq(m4sensorhub->hwconfig.nowakeirq_gpio);
        else {
                KDEBUG(M4SH_ERROR, "%s: No IRQ configured\n", __func__);
                err = -ENODEV;
                goto err_unregister_control_group;
        }

	err = m4sensorhub_panic_init(m4sensorhub);
	if (err < 0) {
		KDEBUG(M4SH_ERROR, "%s: Panic init failed\n", __func__);
		goto err_reg_shutdown;
	}

	KDEBUG(M4SH_INFO, "%s: disabling peripherals\n", __func__);
	m4sensorhub_notify_subscriber(DISABLE_PERIPHERAL);
	err = request_firmware_nowait(THIS_MODULE,
			FW_ACTION_HOTPLUG, m4sensorhub->filename,
			&(m4sensorhub->i2c_client->dev),
			GFP_KERNEL, m4sensorhub,
			m4sensorhub_initialize);
	if (err < 0) {
		KDEBUG(M4SH_ERROR, "%s: request_firmware_nowait failed: %d\n",
			__func__, err);
		goto err_panic_shutdown;
	}
	KDEBUG(M4SH_NOTICE, "Registered M4 Sensor Hub\n");

	goto done;

err_panic_shutdown:
	m4sensorhub_panic_shutdown(m4sensorhub);
err_reg_shutdown:
	m4sensorhub_reg_shutdown(m4sensorhub);
err_unregister_control_group:
	sysfs_remove_group(&client->dev.kobj, &m4sensorhub_control_group);
err_deregister:
	misc_deregister(&m4sensorhub_misc_device);
err_hw_free:
	m4sensorhub->i2c_client = NULL;
	i2c_set_clientdata(client, NULL);
	m4sensorhub_hw_free(m4sensorhub);
	m4sensorhub = NULL;
err_unload:
done:
	if (err < 0) {
		KDEBUG(M4SH_ERROR, "%s: Probe failed with error code %d\n",
			__func__, err);
	}

	return err;
}

static int __exit m4sensorhub_remove(struct i2c_client *client)
{
	struct m4sensorhub_data *m4sensorhub = i2c_get_clientdata(client);
	KDEBUG(M4SH_INFO, "Removing M4 Sensor Hub Driver\n");

	if (m4sensorhub == NULL)
		return 0;

	m4sensorhub_irq_shutdown(m4sensorhub);
	m4sensorhub_panic_shutdown(m4sensorhub);
	m4sensorhub_reg_shutdown(m4sensorhub);
	sysfs_remove_group(&client->dev.kobj, &m4sensorhub_control_group);
	m4sensorhub_hw_reset(m4sensorhub);
	misc_deregister(&m4sensorhub_misc_device);
	m4sensorhub->i2c_client = NULL;
	i2c_set_clientdata(client, NULL);
	m4sensorhub_hw_free(m4sensorhub);
	m4sensorhub = NULL;

	return 0;
}

#ifdef CONFIG_PM
static int m4sensorhub_suspend(struct i2c_client *client, pm_message_t mesg)
{
	KDEBUG(M4SH_DEBUG, "%s\n", __func__);
	m4sensorhub_misc_data.irq_dbg.suspend = 1;
	return 0;
}

static int m4sensorhub_resume(struct i2c_client *client)
{
	KDEBUG(M4SH_DEBUG, "%s\n", __func__);
	m4sensorhub_misc_data.irq_dbg.suspend = 0;
	return 0;
}
#endif /* CONFIG_PM */

static const struct of_device_id of_m4sensorhub_match[] = {
	{ .compatible = "mot,m4sensorhub", },
	{},
};

static const struct i2c_device_id m4sensorhub_id[] = {
	{M4SENSORHUB_DRIVER_NAME, 0},
	{ },
};

MODULE_DEVICE_TABLE(i2c, m4sensorhub_id);

static struct i2c_driver m4sensorhub_driver = {
	.driver = {
		.name = M4SENSORHUB_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_m4sensorhub_match),
	},
	.probe = m4sensorhub_probe,
	.remove = __exit_p(m4sensorhub_remove),
#ifdef CONFIG_PM
	.suspend = m4sensorhub_suspend,
	.resume = m4sensorhub_resume,
#endif /* CONFIG_PM */
	.id_table = m4sensorhub_id,
};

static int __init m4sensorhub_init(void)
{
	return i2c_add_driver(&m4sensorhub_driver);
}

static void __exit m4sensorhub_exit(void)
{
	i2c_del_driver(&m4sensorhub_driver);
	return;
}

module_init(m4sensorhub_init);
module_exit(m4sensorhub_exit);

MODULE_ALIAS("platform:m4sensorhub");
MODULE_DESCRIPTION("M4 Sensor Hub driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
