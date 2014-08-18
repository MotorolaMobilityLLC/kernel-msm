/*
 * Copyright (C) 2012-2013 Motorola Mobility LLC
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
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/wakelock.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/fsa8500.h>
#include <sound/soc.h>
#include <sound/jack.h>

#include "fsa8500-core.h"


#define NAME "fsa8500"

#define I2C_RETRY_DELAY		5 /* ms */
#define I2C_RETRIES		5

#define FSA8500_JACK_MASK (SND_JACK_HEADSET | SND_JACK_HEADPHONE| \
				SND_JACK_LINEOUT | SND_JACK_UNSUPPORTED)
#define FSA8500_JACK_BUTTON_MASK (SND_JACK_BTN_0 | SND_JACK_BTN_1 | \
				SND_JACK_BTN_2 | SND_JACK_BTN_3 | \
				SND_JACK_BTN_4 | SND_JACK_BTN_5 | \
				SND_JACK_BTN_6 | SND_JACK_BTN_7)

#define VDD_UA_ON_LOAD	10000
#define	SND_JACK_BTN_SHIFT	20

static struct snd_soc_jack hs_jack;
static struct snd_soc_jack button_jack;

static struct i2c_client *fsa8500_client;

struct fsa8500_data {
	struct regulator *vdd;
	struct regulator *micvdd;
	int gpio_irq;
	int gpio;
	struct snd_soc_jack *hs_jack;
	struct snd_soc_jack *button_jack;
	u8 irq_status[5];
	int hs_acc_type;
	int inserted;
	int button_pressed;
	int amp_state;
	int mic_state;
	int alwayson_micb;
	int button_detect_state;
	struct mutex lock;
	struct wake_lock wake_lock;
	struct work_struct work_det;
	struct workqueue_struct *wq;
	struct fsa8500_platform_data *pdata;
};

/* I2C Read/Write Functions */
static int fsa8500_i2c_read(struct fsa8500_data *fsa8500,
					u8 reg, u8 *value, int len)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = fsa8500_client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg,
		 },
		{
		 .addr = fsa8500_client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = value,
		 },
	};

	do {
		err = i2c_transfer(fsa8500_client->adapter, msgs,
				ARRAY_SIZE(msgs));
		if (err != ARRAY_SIZE(msgs))
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

	if (err != ARRAY_SIZE(msgs)) {
		dev_err(&fsa8500_client->dev, "read transfer error %d\n", err);
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}
static int fsa8500_i2c_write(struct fsa8500_data *fsa8500, u8 reg, u8 value)
{
	int err;
	int tries = 0;
	u8 buf[2] = {0, 0};

	struct i2c_msg msgs[] = {
		{
		 .addr = fsa8500_client->addr,
		 .flags = fsa8500_client->flags & I2C_M_TEN,
		 .len = 2,
		 .buf = buf,
		 },
	};

	buf[0] = reg;
	buf[1] = value;

	do {
		err = i2c_transfer(fsa8500_client->adapter, msgs,
					ARRAY_SIZE(msgs));
		if (err != ARRAY_SIZE(msgs))
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

	if (err != ARRAY_SIZE(msgs)) {
		dev_err(&fsa8500_client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}


static int fsa8500_reg_read(struct fsa8500_data *fsa8500, u8 reg, u8 *value)
{
	return fsa8500_i2c_read(fsa8500, reg , value, 1);
}

static int fsa8500_reg_write(struct fsa8500_data *fsa8500,
			u8 reg,
			u8 value,
			u8 mask)
{
	int retval;
	u8 old_value;

	value &= mask;

	retval = fsa8500_reg_read(fsa8500, reg , &old_value);

	if (retval != 0)
		goto error;

	old_value &= ~mask;
	value |= old_value;

	retval = fsa8500_i2c_write(fsa8500, reg, value);

error:
	return retval;
}

static void fsa8500_initialize(struct fsa8500_platform_data *pdata,
				struct fsa8500_data *fsa8500)
{
	int i;
	int retval;

	/* Reset */
	fsa8500_reg_write(fsa8500, FSA8500_RESET_CONTROL, FSA8500_RESET, 0xff);

	/* Initialize device registers */
	for (i = 0; i < pdata->init_regs_num; i++) {
		retval = fsa8500_reg_write(fsa8500, pdata->init_regs[i].reg,
					pdata->init_regs[i].value, 0xff);
		if (retval != 0)
			goto error;
	}

	pr_info("fsa8500_initialize success\n");
error:
	return;
}

#ifdef CONFIG_DEBUG_FS

static struct dentry *fsa8500_dir;
static struct dentry *fsa8500_reg_file;
static struct dentry *fsa8500_set_reg_file;

static int fsa8500_registers_print(struct seq_file *s, void *p)
{
	struct fsa8500_data *fsa8500 = s->private;
	u8 reg;
	u8 value;
	pr_info("%s: print registers", __func__);
	seq_puts(s, "fsa8500 registers:\n");
	for (reg = 1; reg < FSA8500_MAX_REGISTER_VAL; reg++) {
		fsa8500_reg_read(fsa8500, reg, &value);
		seq_printf(s, "[0x%x]:  0x%x\n", reg, value);
	}

	return 0;
}

static int fsa8500_registers_open(struct inode *inode, struct file *file)
{
	return single_open(file, fsa8500_registers_print, inode->i_private);
}


static int fsa8500_set_reg_open_file(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t fsa8500_set_reg(struct file *file,
				  const char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	struct fsa8500_data *fsa8500 = file->private_data;
	char buf[32];
	ssize_t buf_size;
	unsigned int user_reg;
	unsigned int user_value;
	u8 reg_read_back;
	/* Get userspace string and assure termination */
	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	if (sscanf(buf, "0x%02x  0x%02x", &user_reg, &user_value) != 2)
		return -EFAULT;

	if (user_reg > FSA8500_MAX_REGISTER_VAL) {
		pr_info("%s: val check failed", __func__);
		return -EINVAL;
	}

	pr_info("%s: set register 0x%02x value 0x%02x", __func__,
		user_reg, user_value);

	fsa8500_reg_write(fsa8500, user_reg & 0xFF, user_value & 0XFF, 0xFF);
	fsa8500_reg_read(fsa8500, user_reg, &reg_read_back);

	pr_info("%s: debug write reg[0x%02x] with 0x%02x after readback: 0x%02x\n",
				__func__, user_reg, user_value, reg_read_back);

	return buf_size;
}


static const struct file_operations fsa8500_registers_fops = {
	.open = fsa8500_registers_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations fsa8500_get_set_reg_fops = {
	.open = fsa8500_set_reg_open_file,
	.write = fsa8500_set_reg,
	.owner = THIS_MODULE,
};


static void fsa8500_setup_debugfs(struct fsa8500_data *fsa8500)
{
	if (!fsa8500)
		goto exit_no_debugfs;

	fsa8500_dir = debugfs_create_dir("fsa8500", NULL);
	if (!fsa8500_dir)
		goto exit_no_debugfs;

	fsa8500_reg_file = debugfs_create_file("registers",
				S_IRUGO, fsa8500_dir, fsa8500,
				&fsa8500_registers_fops);
	if (!fsa8500_reg_file)
		goto exit_destroy_dir;

	fsa8500_set_reg_file = debugfs_create_file("set_reg",
				S_IWUSR, fsa8500_dir, fsa8500,
				&fsa8500_get_set_reg_fops);
	if (!fsa8500_set_reg_file)
		goto exit_destroy_set_reg;

	return;

exit_destroy_set_reg:
	debugfs_remove(fsa8500_reg_file);
exit_destroy_dir:
	debugfs_remove(fsa8500_dir);
exit_no_debugfs:

	return;
}
static inline void fsa8500_remove_debugfs(void)
{
	debugfs_remove(fsa8500_set_reg_file);
	debugfs_remove(fsa8500_reg_file);
	debugfs_remove(fsa8500_dir);
}
#else
static inline void fsa8500_setup_debugfs(struct fsa8500_data *fsa8500)
{
}
static inline void fsa8500_remove_debugfs(void)
{
}
#endif

void fsa8500_hp_event(int event)
{
	struct fsa8500_data *fsa8500;

	pr_debug("%s: hp_event %d\n", __func__, event);

	if (fsa8500_client == NULL)
		return;
	else {
		fsa8500 = i2c_get_clientdata(fsa8500_client);
		if (fsa8500 == NULL)
			return;
	}

	mutex_lock(&fsa8500->lock);
	if (event) {
		if (fsa8500->inserted)
			fsa8500->amp_state = FSA8500_AMP_ENABLED;
	} else
		fsa8500->amp_state = FSA8500_AMP_DISABLED;

	mutex_unlock(&fsa8500->lock);
	return;
}
EXPORT_SYMBOL_GPL(fsa8500_hp_event);

void fsa8500_mic_event(int event)
{
	struct fsa8500_data *fsa8500;

	pr_debug("%s: mic_event %d\n", __func__, event);

	if (fsa8500_client == NULL)
		return;
	else {
		fsa8500 = i2c_get_clientdata(fsa8500_client);
		if (fsa8500 == NULL)
			return;
	}

	mutex_lock(&fsa8500->lock);
	if (fsa8500->alwayson_micb) {
		mutex_unlock(&fsa8500->lock);
		/* mic bias always on in this case nothing to do */
		return;
	}

	if (event) {
		if (fsa8500->inserted)
			fsa8500->mic_state = FSA8500_MIC_ENABLED;
	} else
		fsa8500->mic_state = FSA8500_MIC_DISABLED;

	mutex_unlock(&fsa8500->lock);
	return;
}
EXPORT_SYMBOL_GPL(fsa8500_mic_event);

static int fsa8500_update_device_status(struct fsa8500_data *fsa8500)
{

	fsa8500_i2c_read(fsa8500, FSA8500_INT_REG1,
			fsa8500->irq_status, sizeof(fsa8500->irq_status));
	pr_debug("%s:  regs 0x%x,0x%x,0x%x,0x%x,0x%x\n", __func__,
		fsa8500->irq_status[0], fsa8500->irq_status[1],
		fsa8500->irq_status[2], fsa8500->irq_status[3],
		fsa8500->irq_status[4]);
	if (fsa8500->irq_status[0])
		pr_debug("fsa8500:Detection event %s%s%s%s%s%s%s%s\n",
			(fsa8500->irq_status[0] & 0x1) ? "-3pole-" : "",
			(fsa8500->irq_status[0] & 0x2) ? "-4pole-OMTP-" : "",
			(fsa8500->irq_status[0] & 0x4) ? "-4pole-CTIA-" : "",
			(fsa8500->irq_status[0] & 0x8) ? "-UART-" : "",
			(fsa8500->irq_status[0] & 0x10) ? "-Disconnect-" : "",
			(fsa8500->irq_status[0] & 0x20) ? "-Lint-" : "",
			(fsa8500->irq_status[0] & 0x40) ? "-Moisture-" : "",
			(fsa8500->irq_status[0] & 0x80) ? "-Unknown-" : "");
	if (fsa8500->irq_status[1])
		pr_debug("%s: Key Short press & release: 0x%x\n", __func__,
			fsa8500->irq_status[1]);
	if (fsa8500->irq_status[2])
		pr_debug("%s: Key Long press: 0x%x\n", __func__,
			fsa8500->irq_status[2]);
	if (fsa8500->irq_status[3])
		pr_debug("%s: Key Long release: 0x%x\n", __func__,
			fsa8500->irq_status[3]);

	return 0;
}

static int fsa8500_get_hs_acc_type(struct fsa8500_data *fsa8500)
{
	int acc_type;
	if (fsa8500->irq_status[0] & 0x6)
		acc_type = SND_JACK_HEADSET;
	else
		if (fsa8500->irq_status[4] & FSA8500_ACCESSORY_3)
			acc_type = SND_JACK_LINEOUT;
		else
			acc_type = SND_JACK_HEADPHONE;

	pr_debug("%s: Accessory type %d\n", __func__,
			fsa8500->irq_status[4] & 0x7);
	return acc_type;
}

static int fsa8500_report_hs(struct fsa8500_data *fsa8500)
{
	unsigned int status;
	if ((fsa8500->button_jack == NULL) || (fsa8500->hs_jack == NULL)) {
		pr_err("fsa8500: Something plugged in, report it later\n");
		return -EINVAL;
	}

	/* 3 or 4 pole HS */
	if (fsa8500->irq_status[0] & 0x7) {
		fsa8500->inserted = 1;
		fsa8500->hs_acc_type = fsa8500_get_hs_acc_type(fsa8500);
		pr_debug("%s:report HS insert,type %d\n", __func__,
					fsa8500->hs_acc_type);
		snd_soc_jack_report_no_dapm(fsa8500->hs_jack,
					fsa8500->hs_acc_type,
					fsa8500->hs_jack->jack->type);
	}

	/* Disconnect or UART */
	if ((fsa8500->irq_status[0] & 0x18) && fsa8500->inserted) {
		pr_debug("%s:report HS removal,type %d\n", __func__,
					fsa8500->hs_acc_type);
		snd_soc_jack_report_no_dapm(fsa8500->hs_jack, 0,
					fsa8500->hs_jack->jack->type);
		if (fsa8500->button_pressed) {
			pr_debug("%s:report button release\n", __func__);
			snd_soc_jack_report_no_dapm(fsa8500->button_jack,
				0, fsa8500->button_jack->jack->type);
		fsa8500->button_pressed = 0;
		}
		fsa8500->inserted = 0;
	}

	/* UART */
	if (fsa8500->irq_status[0] & 0x8)
		pr_info("%s:UART port detected\n", __func__);

	/* Key Short press */
	if (fsa8500->irq_status[1] & 0x7F) {
		status = fsa8500->irq_status[1] & 0x7F;
		pr_debug("%s:report key 0x%x short press & release\n",
					__func__, status);
		snd_soc_jack_report_no_dapm(fsa8500->button_jack,
					status<<SND_JACK_BTN_SHIFT,
					status<<SND_JACK_BTN_SHIFT);

		snd_soc_jack_report_no_dapm(fsa8500->button_jack,
					0, status<<SND_JACK_BTN_SHIFT);
	}

	/* Key Long press */
	if (fsa8500->irq_status[2] & 0x7F) {
		status = fsa8500->irq_status[2] & 0x7F;
		pr_debug("%s:report key 0x%x long press\n", __func__, status);
		snd_soc_jack_report_no_dapm(fsa8500->button_jack,
					status<<SND_JACK_BTN_SHIFT,
					status<<SND_JACK_BTN_SHIFT);

		fsa8500->button_pressed |= status;
	}

	/* Key Long release */
	if (fsa8500->irq_status[3] & 0x7F) {
		status = fsa8500->irq_status[3] & 0x7F;
		pr_debug("%s:report key %d release\n", __func__, status);
		snd_soc_jack_report_no_dapm(fsa8500->button_jack,
					0, status<<SND_JACK_BTN_SHIFT);

		fsa8500->button_pressed &= ~status;
	}

	return 0;
}

int fsa8500_hs_detect(struct snd_soc_codec *codec)
{
	int ret = -EINVAL, i;
	struct fsa8500_platform_data *pdata;
	struct fsa8500_data *fsa8500;

	if (fsa8500_client == NULL)
		return ret;

	fsa8500 = i2c_get_clientdata(fsa8500_client);

	if (fsa8500 == NULL)
		return ret;

	pdata = fsa8500->pdata;

	if (fsa8500->hs_jack == NULL) {
		ret = snd_soc_jack_new(codec, "Headset Jack",
					FSA8500_JACK_MASK, &hs_jack);
		if (ret) {
			pr_err("%s: Failed to create new jack\n", __func__);
			return ret;
		}
		fsa8500->hs_jack = &hs_jack;
	}

	if (fsa8500->button_jack == NULL) {
		ret = snd_soc_jack_new(codec, "Button Jack",
				FSA8500_JACK_BUTTON_MASK, &button_jack);
		if (ret) {
			pr_err("Failed to create new jack\n");
			return ret;
		}
		fsa8500->button_jack = &button_jack;
	}

	if (pdata->num_keys && pdata->keymap) {
		for (i = 0; i < pdata->num_keys; i++) {
			ret = snd_jack_set_key(fsa8500->button_jack->jack,
					pdata->keymap[i].soc_btn,
					pdata->keymap[i].keycode);
			if (ret) {
				pr_err("%s: Failed to set code for 0x%x -- 0x%x\n",
					__func__, pdata->keymap[i].soc_btn,
					pdata->keymap[i].keycode);
				return ret;
			}
		}
	} else {
		ret = snd_jack_set_key(fsa8500->button_jack->jack,
					SND_JACK_BTN_0, KEY_MEDIA);
		if (ret) {
			pr_err("%s: Failed to set code for btn-0\n", __func__);
			return ret;
		}
	}

	mutex_lock(&fsa8500->lock);

	if (!fsa8500->inserted) {
		ret = regulator_set_optimum_mode(fsa8500->vdd,
							VDD_UA_ON_LOAD);
		if (ret < 0)
			pr_err("%s: failed to set optimum mode\n", __func__);
		}

	/* if something is plugged in, the irq has been triggered,
	   the status regs were readed and cleared.
	   Just report stored value */

	ret = fsa8500_report_hs(fsa8500);
	if (!fsa8500->inserted)
		regulator_set_optimum_mode(fsa8500->vdd, 0);

	mutex_unlock(&fsa8500->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(fsa8500_hs_detect);

static irqreturn_t fsa8500_irq_handler(int irq, void *data)
{
	struct fsa8500_data *irq_data = data;

	wake_lock_timeout(&irq_data->wake_lock, HZ);
	queue_work(irq_data->wq, &irq_data->work_det);
	return IRQ_HANDLED;
}

static void fsa8500_det_thread(struct work_struct *work)
{
	struct fsa8500_data *irq_data =
				i2c_get_clientdata(fsa8500_client);

	mutex_lock(&irq_data->lock);
	wake_lock(&irq_data->wake_lock);

	fsa8500_update_device_status(irq_data);
	fsa8500_report_hs(irq_data);

	wake_unlock(&irq_data->wake_lock);
	mutex_unlock(&irq_data->lock);
}

#ifdef CONFIG_OF
static int fsa8500_parse_dt_regs_array(const u32 *arr,
	struct fsa8500_regs *regs, int count)
{
	u32 len = 0, reg, val;
	int i;

	for (i = 0; i < count*2; i += 2) {
		reg = be32_to_cpu(arr[i]);
		val = be32_to_cpu(arr[i + 1]);
		if ((reg < FSA8500_MAX_REGISTER_VAL) && (val < 0xff)) {
			regs->reg = (char) reg;
			regs->value = (char) val;
			len++;
			pr_debug("%s: regs: 0x%x=0x%x\n", __func__, reg, val);
			regs++;
		}
	}
	return len;
}

static int fsa8500_parse_dt_keymap(const u32 *arr,
	struct fsa8500_keymap *keymap, int count)
{
	u32 len = 0, btn, code;
	int i;

	if (!arr)
		return 0;

	for (i = 0; i < count*2; i += 2) {
		btn = be32_to_cpu(arr[i]);
		code = be32_to_cpu(arr[i + 1]);
		if (btn & 0xff00000) {
			keymap->soc_btn = btn;
			keymap->keycode = code;
			len++;
			pr_debug("%s: key: 0x%x=0x%x\n", __func__, btn, code);
			keymap++;
		}
	}
	return len;
}

static struct fsa8500_platform_data *
fsa8500_of_init(struct i2c_client *client)
{
	struct fsa8500_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	const u32 *regs_arr, *keymap;
	int regs_len, keymap_len;
	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "pdata allocation failure\n");
		return NULL;
	}

	of_property_read_u32(np, "fsa8500-always-on-micbias",
				&pdata->alwayson_micbias);
	pdata->irq_gpio = of_get_gpio(np, 0);

	regs_arr = of_get_property(np, "fsa8500-init-regs",
			&regs_len);
	if (!regs_arr || (regs_len & 1)) {
		pr_warn("fsa8500: No init registers setting\n");
		regs_len = 0;
	} else {
		regs_len /= 2 * sizeof(u32);
		if (regs_len > FSA8500_MAX_REGISTER_VAL)
			regs_len = FSA8500_MAX_REGISTER_VAL;

		pdata->init_regs = devm_kzalloc(&client->dev,
					sizeof(struct fsa8500_regs) * regs_len,
					GFP_KERNEL);

		if (!pdata->init_regs) {
			pr_warn("fsa8500: init_regs allocation failure\n");
			return pdata;
		}
		pdata->init_regs_num = fsa8500_parse_dt_regs_array(regs_arr,
						pdata->init_regs, regs_len);
	}

	keymap = of_get_property(np, "fsa8500-keymap",
			&keymap_len);
	if (!keymap || (keymap_len & 1)) {
		pr_warn("fsa8500: No keymap setting\n");
		keymap_len = 0;
	} else {
		keymap_len /= 2 * sizeof(u32);
		if (keymap_len > FSA8500_NUM_KEYS)
			keymap_len = FSA8500_NUM_KEYS;

		pdata->keymap = devm_kzalloc(&client->dev,
				sizeof(struct fsa8500_keymap) * keymap_len,
				GFP_KERNEL);
		if (!pdata->keymap) {
			pr_warn("fsa8500: keymap allocation failure\n");
			return pdata;
		}
		pdata->num_keys = fsa8500_parse_dt_keymap(keymap,
						pdata->keymap, keymap_len);
	}

	return pdata;
}
#else
static inline struct fsa8500_platform_data *
fsa8500_of_init(struct i2c_client *client)
{
	return NULL;
}
#endif

static int fsa8500_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct fsa8500_data *fsa8500;
	struct fsa8500_platform_data *fsa8500_pdata;
	int err;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "check_functionality failed\n");
		return -EIO;
	}

	if (client->dev.of_node)
			fsa8500_pdata = fsa8500_of_init(client);
	else
			fsa8500_pdata = client->dev.platform_data;

	/* Check platform data */
	if (fsa8500_pdata == NULL) {
		dev_err(&client->dev,
			"platform data is NULL\n");
		return -EINVAL;
	}

	fsa8500 = kzalloc(sizeof(*fsa8500), GFP_KERNEL);
	if (fsa8500 == NULL) {
		dev_err(&client->dev,
			"Failed to allocate memory for module data\n");
		return -ENOMEM;
	}

	err = gpio_request(fsa8500_pdata->irq_gpio, "hs irq");
	if (err) {
		dev_err(&client->dev, "fsa8500 irq gpio init failed\n");
		goto gpio_init_fail;
	}

	/* set global variables */
	fsa8500_client = client;
	i2c_set_clientdata(client, fsa8500);
	mutex_init(&fsa8500->lock);
	fsa8500->gpio = fsa8500_pdata->irq_gpio;
	fsa8500->inserted = 0;
	fsa8500->button_pressed = 0;
	fsa8500->button_jack = NULL;
	fsa8500->hs_jack = NULL;
	fsa8500->pdata = fsa8500_pdata;
	/* This flag is used to indicate that mic bias should be always
	 * on when headset is inserted, needed for always on voice
	 * operations and noise estimate.
	 */
	fsa8500->alwayson_micb = fsa8500_pdata->alwayson_micbias;

	/* enable regulators */
	fsa8500->vdd = regulator_get(&client->dev, "hs_det_vdd");
	if (IS_ERR(fsa8500->vdd)) {
		pr_err("%s: Error getting vdd regulator.\n", __func__);
		err = PTR_ERR(fsa8500->vdd);
		goto reg_get_vdd;
	}

	regulator_set_voltage(fsa8500->vdd, 1800000, 1800000);
	err = regulator_enable(fsa8500->vdd);
	if (err < 0) {
		pr_err("%s: Error enabling vdd regulator.\n", __func__);
		goto reg_enable_vdd_fail;
	}

	fsa8500->micvdd = regulator_get(&client->dev, "hs_det_micvdd");
	if (IS_ERR(fsa8500->micvdd)) {
		pr_err("%s: Error getting micvdd regulator.\n", __func__);
		err = PTR_ERR(fsa8500->micvdd);
		goto reg_get_micvdd;
	}

	regulator_set_voltage(fsa8500->micvdd, 2850000, 2850000);
	err = regulator_enable(fsa8500->micvdd);
	if (err < 0) {
		pr_err("%s: Error enabling micvdd regulator.\n", __func__);
		goto reg_enable_micvdd_fail;
	}

	wake_lock_init(&fsa8500->wake_lock, WAKE_LOCK_SUSPEND, "hs_det");

	/* Initialize device registers */

	fsa8500_initialize(fsa8500_pdata, fsa8500);

	fsa8500->wq = create_singlethread_workqueue("fsa8500");
	if (fsa8500->wq == NULL) {
		err = -ENOMEM;
		goto wq_fail;
	}

	INIT_WORK(&fsa8500->work_det, fsa8500_det_thread);

	fsa8500->gpio_irq = gpio_to_irq(fsa8500->gpio);
	/* active low interrupt */
	err = request_irq(fsa8500->gpio_irq, fsa8500_irq_handler,
					IRQF_TRIGGER_FALLING,
			"hs_det", fsa8500);
	if (err) {
		pr_err("%s: IRQ request failed: %d\n", __func__, err);
		goto irq_fail;
	}
	enable_irq_wake(fsa8500->gpio_irq);

	/* setup debug fs interfaces */
	fsa8500_setup_debugfs(fsa8500);

	pr_info("fsa8500 probed successfully\n");

	return 0;

irq_fail:
	wake_lock_destroy(&fsa8500->wake_lock);
	destroy_workqueue(fsa8500->wq);
wq_fail:
reg_enable_micvdd_fail:
	regulator_put(fsa8500->micvdd);
reg_get_micvdd:
	regulator_disable(fsa8500->vdd);
reg_enable_vdd_fail:
	regulator_put(fsa8500->vdd);
reg_get_vdd:
	gpio_free(fsa8500->gpio);
gpio_init_fail:
	kfree(fsa8500);
	fsa8500_client = NULL;
	return err;
}

static int fsa8500_remove(struct i2c_client *client)
{
	struct fsa8500_data *fsa8500 = i2c_get_clientdata(client);
	regulator_set_optimum_mode(fsa8500->vdd, 0);
	regulator_disable(fsa8500->vdd);
	regulator_put(fsa8500->vdd);
	regulator_disable(fsa8500->micvdd);
	regulator_put(fsa8500->micvdd);
	gpio_free(fsa8500->gpio);
	wake_lock_destroy(&fsa8500->wake_lock);
	destroy_workqueue(fsa8500->wq);
	fsa8500_remove_debugfs();
	kfree(fsa8500);
	fsa8500_client = NULL;
	return 0;
}

static const struct i2c_device_id fsa8500_id[] = {
	{NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, fsa8500_id);

#ifdef CONFIG_OF
static struct of_device_id fsa8500_match_tbl[] = {
	{ .compatible = "fairchild,fsa8500" },
	{ },
};
MODULE_DEVICE_TABLE(of, fsa8500_match_tbl);
#endif

static struct i2c_driver fsa8500_driver = {
	.driver = {
			.name = NAME,
			.owner = THIS_MODULE,
			.of_match_table = of_match_ptr(fsa8500_match_tbl),
	},
	.probe = fsa8500_probe,
	.remove = fsa8500_remove,
	.id_table = fsa8500_id,
};

static int fsa8500_init(void)
{
	return i2c_add_driver(&fsa8500_driver);
}

static void fsa8500_exit(void)
{
	i2c_del_driver(&fsa8500_driver);
	return;
}

module_init(fsa8500_init);
module_exit(fsa8500_exit);

MODULE_DESCRIPTION("Fairchild FSA8500 driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola Mobility");
