/*
 * R69001 Touchscreen Controller Driver
 * Source file
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/input-polldev.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>
#include <linux/suspend.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>

#define MODE_COUNT                  4
#define MODE_STRING_MAX_LEN         30
#define MODE_FILE_NAME              "mode"
#endif

#define CONFIG_R69001_POLLING_TIME 10
#include <linux/r69001-ts.h>

#define R69001_TS_NAME              "r69001-ts-i2c"

/* Coordinates data register address */
#define REG_COORDINATES_DATA        0x00    /* High */
#define REG_INFO1                   0x00    /* Low */
#define REG_INFO2                   0x01
#define REG_DATA0                   0x02
#define REG_DATA1                   0x0b
#define REG_DATA2                   0x14
#define REG_DATA3                   0x1d
#define REG_DATA4                   0x26

/* One set coordinates data size */
#define ONE_SET_COORD_DATA_SIZE     9

/* Boot Mode */
#define BOOT_MODE_BOOT_ROM          0x80

/* Commands */
#define COMMAND_BOOT                0x10
#define COMMAND_FIRMWARE_UPDATE     0x20

/* Control register address */
#define REG_CONTROL                 0x1c    /* High */
#define REG_SCAN_MODE               0x00    /* Low */
#define REG_SCAN_CYCLE              0x01
#define REG_INT_POLL_CTRL           0x02
#define REG_INT_SIGNAL_OUTPUT_CTRL  0x03
#define REG_WRITE_DATA_CTRL         0x04
#define REG_READY_DATA              0x05
#define REG_SCAN_COUNTER            0x06
#define REG_FUNC_CTRL               0x0b
#define REG_LOW_POWER               0x17

/* Ready data */
#define READY_COORDINATES           0x01
#define READY_RAW                   0x02
#define READY_BASELINE              0x04
#define READY_DIFF                  0x08
#define READY_LABElMAP              0x10
#define READY_CALIBRATION           0x20
#define READY_GESTURE               0x40

/* Scan Mode */
#define SCAN_MODE_STOP              R69001_SCAN_MODE_STOP
#define SCAN_MODE_LOW_POWER         R69001_SCAN_MODE_LOW_POWER
#define SCAN_MODE_FULL_SCAN         R69001_SCAN_MODE_FULL_SCAN
#define SCAN_MODE_CALIBRATION       R69001_SCAN_MODE_CALIBRATION

/* Interrupt/Polling mode */
#define INTERRUPT_MODE              R69001_TS_INTERRUPT_MODE
#define POLLING_MODE                R69001_TS_POLLING_MODE
#define POLLING_LOW_EDGE_MODE       R69001_TS_POLLING_LOW_EDGE_MODE
#define CALIBRATION_INTERRUPT_MODE  R69001_TS_CALIBRATION_INTERRUPT_MODE
#define UNKNOW_MODE                 255

#define DEFAULT_INTERRUPT_MASK      0x0e
#define CALIBRATION_INTERRUPT_MASK  0x08
#define TOUCH_ID_MIN                1
#define TOUCH_ID_INVALID            0xff

struct r69001_ts_finger {
	u16 x;
	u16 y;
	u8 z;
	u8 t;
};

struct r69001_ts_before_regs {
	u8 int_signal_output_ctrl;
	u8 scan_cycle;
};

struct r69001_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct r69001_ts_finger finger[MAX_FINGERS];
	struct r69001_io_data data;
	struct r69001_ts_before_regs regs;
	struct r69001_platform_data *pdata;
	unsigned int finger_mask;
	u8 mode;
	u8 t_num;
};

#ifdef CONFIG_DEBUG_FS
static struct dentry *r69001_ts_dbgfs_root;
static const char * const r69001_ts_modes[] = { "interrupt", "polling",
		"polling low edge", "calibration interrupt" };
#endif

static void r69001_set_mode(struct r69001_ts_data *ts, u8 mode, u16 poll_time);

static int r69001_ts_read_data(struct r69001_ts_data *ts,
				u8 addr_h, u8 addr_l, u16 size, u8 *data)
{
	struct i2c_client *client = ts->client;
	struct i2c_msg msg[2];
	int error;
	u8 buf[2];

	buf[0] = addr_h;
	buf[1] = addr_l;

	/* Set data point */
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = buf;

	/* Byte read */
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = data;

	error = i2c_transfer(client->adapter, msg, 1);
	if (error > 0)
		error = i2c_transfer(client->adapter, msg + 1, 1);
	if (error < 0)
		dev_err(&client->dev,
			"I2C read error high: 0x%02x low:0x%02x size:%d ret:%d\n",
			addr_h, addr_l, size, error);

	return error;
}

static int
r69001_ts_write_data(struct r69001_ts_data *ts, u8 addr_h, u8 addr_l, u8 data)
{
	struct i2c_client *client = ts->client;
	struct i2c_msg msg;
	int error;
	u8 buf[3];

	buf[0] = addr_h;
	buf[1] = addr_l;
	buf[2] = data;

	/* Byte write */
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = sizeof(buf);
	msg.buf = buf;

	error = i2c_transfer(client->adapter, &msg, 1);
	if (error < 0)
		dev_err(&client->dev,
			"I2C write error high: 0x%02x low:0x%02x data:0x%02x ret:%d\n",
			addr_h, addr_l, data, error);
	return error;
}

static void
r69001_ts_report_coordinates_data(struct r69001_ts_data *ts, int filter)
{
	struct r69001_ts_finger *finger = ts->finger;
	struct input_dev *input_dev = ts->input_dev;
	unsigned int mask = 0;
	u8 i;

	for (i = 0; i < ts->t_num; i++) {
		if (finger[i].t < TOUCH_ID_MIN || finger[i].t == filter)
			continue;
		finger[i].t -= TOUCH_ID_MIN;
		input_mt_slot(input_dev, finger[i].t);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
		input_report_abs(input_dev, ABS_MT_POSITION_X, finger[i].x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, finger[i].y);
		input_report_abs(input_dev, ABS_MT_PRESSURE, finger[i].z);
		mask |= (1 << finger[i].t);
	}

	/* Get the removed fingers */
	ts->finger_mask &= ~mask;

	/* Release the removed fingers */
	for (i = 0; ts->finger_mask != 0; i++) {
		if (ts->finger_mask & 0x01) {
			input_mt_slot(input_dev, i);
			input_mt_report_slot_state(input_dev,
					MT_TOOL_FINGER, false);
		}
		ts->finger_mask >>= 1;
	}

	/* SYN_REPORT */
	input_sync(input_dev);

	ts->finger_mask = mask;
	ts->t_num = 0;
}

static int r69001_ts_read_coordinates_data(struct r69001_ts_data *ts)
{
	struct r69001_ts_finger *finger = ts->finger;
	u8 i;
	u8 numt = 0;
	u8 data[ONE_SET_COORD_DATA_SIZE] = { 0 };
	u8 lowreg[5] = {REG_DATA0, REG_DATA1, REG_DATA2, REG_DATA3, REG_DATA4};
	int error;
	bool inval_id = false;

	error = r69001_ts_read_data(ts,
			REG_COORDINATES_DATA, REG_INFO1, 1, &numt);
	if (error < 0)
		return error;

	numt &= 0x0f;
	if (numt > MAX_FINGERS)
		numt = MAX_FINGERS;

	for (i = 0; i < numt; i++) {
		if (i % 2) {
			finger[i].x =
				((u16)(data[7] & 0x0f) << 8) | (u16)(data[5]);
			finger[i].y =
				((u16)(data[7] & 0xf0) << 4) | (u16)(data[6]);
			finger[i].z = data[8];
			finger[i].t = (data[0] & 0xf0) >> 4;
			if (finger[i].t < TOUCH_ID_MIN)
				inval_id = true;

		} else {
			error = r69001_ts_read_data(ts,
					REG_COORDINATES_DATA, lowreg[i / 2],
					ONE_SET_COORD_DATA_SIZE, data);
			if (error < 0)
				return error;
			finger[i].x =
				((u16)(data[3] & 0x0f) << 8) | (u16)(data[1]);
			finger[i].y =
				((u16)(data[3] & 0xf0) << 4) | (u16)(data[2]);
			finger[i].z = data[4];
			finger[i].t = data[0] & 0x0f;
			if (finger[i].t < TOUCH_ID_MIN)
				inval_id = true;
		}
	}

	/* Only update the number when there is no error happened */
	ts->t_num = numt;
	return inval_id ? TOUCH_ID_INVALID : 0;
}

static irqreturn_t r69001_ts_irq_handler(int irq, void *dev_id)
{
	struct r69001_ts_data *ts = dev_id;
	struct i2c_client *client = ts->client;
	u8 mode = 0;
	int err = 0;
	int filter = 0;

	r69001_ts_read_data(ts, REG_CONTROL, REG_SCAN_MODE, 1, &mode);

	if (ts->data.mode.mode == INTERRUPT_MODE) {
		if (mode == SCAN_MODE_STOP) {
			/* if receive a touchscreen interrupt, but the scan
			 * mode is stop and we are in interrupt mode, that means
			 * touch panel just power on, so re-init it
			 */
			ts->data.mode.mode = UNKNOW_MODE;
			r69001_ts_write_data(ts, REG_CONTROL,
						REG_SCAN_CYCLE, SCAN_TIME);
			r69001_set_mode(ts, ts->mode, POLL_INTERVAL);
		}

		err = r69001_ts_read_coordinates_data(ts);
		if (err < 0) {
			dev_err(&client->dev,
					"%s: Read coordinate data failed\n",
					__func__);

			return IRQ_HANDLED;
		}
		if (err == TOUCH_ID_INVALID)
			filter = 1;
		r69001_ts_report_coordinates_data(ts, filter);
	}

	return IRQ_HANDLED;
}

/*
 * Set Int Ctl
 * mode : 0 = INT Mode, 1 = POLL Mode, 2 = POLL + INT Mode
 * poll_time : Polling interval (msec, 1 - 1000)
 *
 * The msleep(100) in this driver comes directly from Vendor's
 * driver, and can't find any explanation in the datasheet, so
 * just keep it now.
 */
static void r69001_set_mode(struct r69001_ts_data *ts, u8 mode, u16 poll_time)
{
	struct i2c_client *client = ts->client;

	if (ts->data.mode.mode == mode)
		return;

	switch (mode) {
	case INTERRUPT_MODE:
		r69001_ts_write_data(ts, REG_CONTROL,
				REG_INT_POLL_CTRL, INTERRUPT_MODE);

		r69001_ts_write_data(ts, REG_CONTROL,
				REG_SCAN_MODE, SCAN_MODE_STOP);
		r69001_ts_write_data(ts, REG_CONTROL,
				REG_INT_SIGNAL_OUTPUT_CTRL,
				DEFAULT_INTERRUPT_MASK);
		msleep(100);
		r69001_ts_write_data(ts, REG_CONTROL,
				REG_SCAN_MODE, SCAN_MODE_FULL_SCAN);
		ts->data.mode.mode = mode;
		break;
	case POLLING_MODE:
	case POLLING_LOW_EDGE_MODE:
		r69001_ts_write_data(ts, REG_CONTROL,
				REG_INT_POLL_CTRL, POLLING_MODE);
		if (mode == POLLING_LOW_EDGE_MODE)
			r69001_ts_write_data(ts, REG_CONTROL,
				REG_INT_SIGNAL_OUTPUT_CTRL, 0x01);
		else
			r69001_ts_write_data(ts, REG_CONTROL,
				REG_INT_SIGNAL_OUTPUT_CTRL, 0x00);
		r69001_ts_write_data(ts, REG_CONTROL,
				REG_SCAN_MODE, SCAN_MODE_STOP);
		msleep(100);
		r69001_ts_write_data(ts, REG_CONTROL,
				REG_SCAN_MODE, SCAN_MODE_FULL_SCAN);
		r69001_ts_write_data(ts, REG_CONTROL,
				REG_WRITE_DATA_CTRL, 0x01);
		if (poll_time && poll_time <= POLL_INTERVAL_MAX)
			ts->data.mode.poll_time = poll_time;
		else
			ts->data.mode.poll_time = POLL_INTERVAL;
		ts->data.mode.mode = mode;
		break;
	case CALIBRATION_INTERRUPT_MODE:
		r69001_ts_write_data(ts, REG_CONTROL,
				REG_SCAN_MODE, SCAN_MODE_STOP);
		r69001_ts_write_data(ts, REG_CONTROL,
				REG_INT_SIGNAL_OUTPUT_CTRL,
				CALIBRATION_INTERRUPT_MASK);
		msleep(100);
		r69001_ts_write_data(ts, REG_CONTROL,
				REG_SCAN_MODE, SCAN_MODE_CALIBRATION);
		ts->data.mode.mode = mode;
		break;
	default:
		dev_err(&client->dev, "Set Int Ctl bad parameter = %d\n", mode);
		break;
	}
}

#ifdef CONFIG_DEBUG_FS
static int r69001_ts_dbgfs_show(struct seq_file *seq, void *unused)
{
	struct r69001_ts_data *ts;

	ts = (struct r69001_ts_data *)seq->private;
	if (!ts)
		return -EFAULT;

	if (ts->data.mode.mode >= MODE_COUNT)
		return -EFAULT;

	seq_printf(seq, "%s\n", r69001_ts_modes[ts->data.mode.mode]);

	return 0;
}

static int r69001_ts_dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, r69001_ts_dbgfs_show, inode->i_private);
}

static ssize_t r69001_ts_dbgfs_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int i;
	struct seq_file *seq;
	struct r69001_ts_data *ts;
	char buf[MODE_STRING_MAX_LEN] = {0};

	if (count > sizeof(buf) - 1)
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count - 1))
		return -EFAULT;

	seq = (struct seq_file *)file->private_data;
	if (!seq) {
		pr_warn("r69001-touchscreen: Failed to get seq_file\n");
		return -EFAULT;
	}

	ts = (struct r69001_ts_data *)seq->private;
	if (!ts) {
		pr_warn("r69001-touchscreen: Failed to get private data\n");
		return -EFAULT;
	}

	for (i = 0; i < MODE_COUNT; i++) {
		if (!strncmp(buf, r69001_ts_modes[i], MODE_STRING_MAX_LEN)) {
			r69001_set_mode(ts, i, POLL_INTERVAL);
			break;
		}
	}

	if (i == MODE_COUNT) {
		pr_warn("r69001-touchscreen: Invalid mode: %s\n", buf);
		return -EINVAL;
	}

	return count;
}

static const struct file_operations r69001_ts_dbgfs_fops = {
	.owner		= THIS_MODULE,
	.open		= r69001_ts_dbgfs_open,
	.read		= seq_read,
	.write		= r69001_ts_dbgfs_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int r69001_ts_create_dbgfs(struct r69001_ts_data *ts)
{
	struct dentry *entry;

	r69001_ts_dbgfs_root = debugfs_create_dir(R69001_TS_NAME, NULL);
	if (!r69001_ts_dbgfs_root) {
		dev_warn(&ts->client->dev, "debugfs_create_dir failed\n");
		return -ENOMEM;
	}

	entry = debugfs_create_file(MODE_FILE_NAME, S_IRUSR,
			r69001_ts_dbgfs_root,
			(void *)ts, &r69001_ts_dbgfs_fops);
	if (!entry) {
		debugfs_remove_recursive(r69001_ts_dbgfs_root);
		r69001_ts_dbgfs_root = NULL;
		dev_warn(&ts->client->dev, "%s debugfs entry creation failed\n",
				MODE_FILE_NAME);
		return -ENOMEM;
	}

	return 0;
}

static void r69001_ts_remove_dbgfs(void)
{
	debugfs_remove_recursive(r69001_ts_dbgfs_root);
}
#endif

static int
r69001_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct r69001_ts_data *ts;
	struct input_dev *input_dev;
	struct r69001_platform_data *pdata = client->dev.platform_data;
	int error;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "Not I2C_FUNC_I2C\n");
		return -EIO;
	}

	ts = kzalloc(sizeof(struct r69001_ts_data), GFP_KERNEL);
	if (!ts) {
		dev_err(&client->dev, "Out of memory\n");
		return -ENOMEM;
	}
	if (!pdata) {
		dev_err(&client->dev, "No touch platform data\n");
		error = -EINVAL;
		goto err1;
	}
	ts->client = client;
	ts->pdata = pdata;

	if (!client->irq) /* not fast irq but a gpio one */
		client->irq = gpio_to_irq(pdata->gpio);

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "Unable to allocated input device\n");
		error =  -ENOMEM;
		goto err2;
	}

	ts->input_dev = input_dev;

	input_dev->name = "r69001-touchscreen";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,6,0))
	input_mt_init_slots(input_dev, MAX_FINGERS, 0);
#else
	input_mt_init_slots(input_dev, MAX_FINGERS);
#endif
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, MIN_X, MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, MIN_Y, MAX_Y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, MIN_Z, MAX_Z, 0, 0);

	error = input_register_device(ts->input_dev);
	if (error) {
		dev_err(&client->dev, "Failed to register %s input device\n",
							input_dev->name);
		goto err3;
	}

	i2c_set_clientdata(client, ts);

	ts->mode = INTERRUPT_MODE;

	error = request_threaded_irq(client->irq, NULL, r69001_ts_irq_handler,
			pdata->irq_type, client->name, ts);
	if (error) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		goto err4;
	}

#ifdef CONFIG_DEBUG_FS
	error = r69001_ts_create_dbgfs(ts);
	if (error)
		dev_warn(&client->dev, "Failed to create debugfs\n");
#endif

	return 0;

err4:
	input_unregister_device(ts->input_dev);
err3:
	input_free_device(ts->input_dev);
err2:
err1:
	kfree(ts);
	return error;
}

static int r69001_ts_remove(struct i2c_client *client)
{
	struct r69001_ts_data *ts = i2c_get_clientdata(client);

#ifdef CONFIG_DEBUG_FS
	r69001_ts_remove_dbgfs();
#endif

	input_unregister_device(ts->input_dev);
	if (client->irq)
		free_irq(client->irq, ts);
	kfree(ts);
	return 0;
}

static const struct i2c_device_id r69001_ts_id[] = {
	{ R69001_TS_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, r69001_ts_id);

static struct i2c_driver r69001_ts_driver = {
	.probe = r69001_ts_probe,
	.remove = r69001_ts_remove,
	.id_table = r69001_ts_id,
	.driver = {
		.name = R69001_TS_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init r69001_ts_init(void)
{
	return i2c_add_driver(&r69001_ts_driver);
}

static void __exit r69001_ts_exit(void)
{
	i2c_del_driver(&r69001_ts_driver);
}

module_init(r69001_ts_init);
module_exit(r69001_ts_exit);

MODULE_DESCRIPTION("Renesas SP Driver R69001 Touchscreen Controller Driver");
MODULE_LICENSE("GPL");
