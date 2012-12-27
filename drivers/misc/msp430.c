/*
 * Copyright (C) 2010-2012 Motorola, Inc.
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

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/msp430.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/time.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>




#define NAME			     "msp430"

#define I2C_RETRY_DELAY			5
#define I2C_RETRIES			5
#define I2C_RESPONSE_LENGTH		8
#define MSP430_MAXDATA_LENGTH		250
#define MSP430_DELAY_USEC		10
#define MSP430_RESPONSE_MSG		0x3b
#define MSP430_RESPONSE_MSG_SUCCESS	0x00
#define MSP430_CRC_SEED			0xffff
#define MSP430_COMMAND_HEADER		0x80
#define MSP430_CRC_LENGTH		2
#define MSP430_OPCODE_LENGTH		1
#define MSP430_ADDRESS_LENGTH		3
#define MSP430_CORECOMMAND_LENGTH	(MSP430_OPCODE_LENGTH +\
					MSP430_MAXDATA_LENGTH +\
					MSP430_ADDRESS_LENGTH)
#define MSP430_HEADER_LENGTH		1
#define MSP430_CMDLENGTH_BYTES		2
#define G_MAX				0x7FFF
#define MSP430_CLIENT_MASK		0xF0

/* MSP430 memory map */
#define ID				0x00
#define REV_ID				0x01
#define ERROR_STATUS			0x02

#define AP_POSIX_TIME			0x10

#define ACCEL_UPDATE_RATE		0x16
#define MAG_UPDATE_RATE			0x17
#define PRESSURE_UPDATE_RATE		0x18
#define GYRO_UPDATE_RATE		0x19

#define NONWAKESENSOR_CONFIG		0x1A
#define WAKESENSOR_CONFIG		0x1B

#define MOTION_DUR			0x20

#define ZRMOTION_DUR			0x22

#define BYPASS_MODE			0x24
#define SLAVE_ADDRESS			0x25

#define ALGO_CONFIG			0x26
#define ALGO_INT_STATUS			0x27

#define RADIAL_DUR			0x2A

#define RADIAL_DATA			0x2C
#define MOTION_DATA			0x2D

#define LUX_TABLE_VALUES                0x34
#define BRIGHTNESS_TABLE_VALUES         0x35

#define INTERRUPT_MASK			0x37
#define WAKESENSOR_STATUS		0x39
#define INTERRUPT_STATUS		0x3A

#define ACCEL_X				0x3B

#define DOCK_DATA			0x3F

#define TEMPERATURE_DATA		0x41

#define GYRO_X				0x43

#define MAG_HX				0x49

#define DISP_ROTATE_DATA		0x4A
#define FLAT_DATA			0x4B

#define CURRENT_PRESSURE		0x66

#define ALS_LUX				0x6A

#define DISPLAY_BRIGHTNESS		0x6B

#define PROXIMITY			0x6C

#define STOWED				0x6D

#define MOTION_DUR_1			0x71
#define ZRMOTION_DUR_1			0x72
#define MOTION_DUR_2			0x73
#define ZRMOTION_DUR_2			0x74
#define MOTION_DUR_3			0x75
#define ZRMOTION_DUR_3			0x76
#define MOTION_THR_1			0x77
#define ZRMOTION_THR_1			0x78
#define MOTION_THR_2			0x79
#define ZRMOTION_THR_2			0x7A
#define MOTION_THR_3			0x7B
#define ZRMOTION_THR_3			0x7C

#define RESET				0x7F

#define MSP430_AS_DATA_QUEUE_SIZE	0x20
#define MSP430_AS_DATA_QUEUE_MASK	0x1F
#define MSP430_ES_DATA_QUEUE_SIZE	0x08
#define MSP430_ES_DATA_QUEUE_MASK	0x07

#define KDEBUG(format, s...)	if (g_debug) pr_info(format, ##s)
static char g_debug;

static unsigned int msp430_irq_disable;
module_param_named(irq_disable, msp430_irq_disable, uint, 0644);

/* to hold sensor state so that it can be restored on resume */
static unsigned short g_nonwake_sensor_state;
static unsigned short g_wake_sensor_state;

static unsigned char msp_cmdbuff[MSP430_HEADER_LENGTH + MSP430_CMDLENGTH_BYTES +
			MSP430_CORECOMMAND_LENGTH + MSP430_CRC_LENGTH];

enum msp_mode {
	UNINITIALIZED,
	BOOTMODE,
	NORMALMODE,
	FACTORYMODE
};

struct msp430_data {
	struct i2c_client *client;
	struct msp430_platform_data *pdata;
	/* to avoid two i2c communications at the same time */
	struct mutex lock;
	struct work_struct irq_work;
	struct work_struct irq_wake_work;
	struct workqueue_struct *irq_work_queue;
	struct wake_lock wakelock;

	int hw_initialized;

	atomic_t enabled;
	int irq;
	int irq_wake;
	unsigned int current_addr;
	enum msp_mode mode;
	unsigned char intp_mask;
	struct early_suspend early_suspend;

	dev_t msp430_dev_num;
	struct class *msp430_class;
	struct cdev as_cdev;
	struct cdev ms_cdev;

	struct switch_dev dsdev; /* Standard Dock switch */
	struct switch_dev edsdev; /* Motorola Dock switch */

	struct msp430_android_sensor_data
		msp430_as_data_buffer[MSP430_AS_DATA_QUEUE_SIZE];
	int msp430_as_data_buffer_head;
	int msp430_as_data_buffer_tail;
	wait_queue_head_t msp430_as_data_wq;

	struct msp430_moto_sensor_data
		msp430_ms_data_buffer[MSP430_ES_DATA_QUEUE_SIZE];
	int msp430_ms_data_buffer_head;
	int msp430_ms_data_buffer_tail;
	wait_queue_head_t msp430_ms_data_wq;
};

enum msp_commands {
	PASSWORD_RESET,
	MASS_ERASE,
	PROGRAM_CODE,
	END_FIRMWARE,
	PASSWORD_RESET_DEFAULT
};

enum msp_opcode {
	PASSWORD_OPCODE = 0x11,
	MASSERASE_OPCODE = 0x15,
	RXDATA_OPCODE = 0x10,
};


struct msp_response {

	/* 0x0080 */
	unsigned short header;
	unsigned char len_lsb;
	unsigned char len_msb;
	unsigned char cmd;
	unsigned char data;
	unsigned char crc_lsb;
	unsigned char crc_msb;
};

static const unsigned char msp_motion_dur_a[] = {
	MOTION_DUR,
	MOTION_DUR_1,
	MOTION_DUR_2,
	MOTION_DUR_3
};

static const unsigned char msp_zrmotion_dur_a[] = {
	ZRMOTION_DUR,
	ZRMOTION_DUR_1,
	ZRMOTION_DUR_2,
	ZRMOTION_DUR_3
};

static const unsigned short crc_table[256] = {
0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5,
0x60c6, 0x70e7, 0x8108, 0x9129, 0xa14a, 0xb16b,
0xc18c, 0xd1ad, 0xe1ce, 0xf1ef, 0x1231, 0x0210,
0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c,
0xf3ff, 0xe3de, 0x2462, 0x3443, 0x0420, 0x1401,
0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a, 0xb54b,
0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6,
0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719, 0x8738,
0xf7df, 0xe7fe, 0xd79d, 0xc7bc, 0x48c4, 0x58e5,
0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969,
0xa90a, 0xb92b, 0x5af5, 0x4ad4, 0x7ab7, 0x6a96,
0x1a71, 0x0a50, 0x3a33, 0x2a12, 0xdbfd, 0xcbdc,
0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03,
0x0c60, 0x1c41, 0xedae, 0xfd8f, 0xcdec, 0xddcd,
0xad2a, 0xbd0b, 0x8d68, 0x9d49, 0x7e97, 0x6eb6,
0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a,
0x9f59, 0x8f78, 0x9188, 0x81a9, 0xb1ca, 0xa1eb,
0xd10c, 0xc12d, 0xf14e, 0xe16f, 0x1080, 0x00a1,
0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c,
0xe37f, 0xf35e, 0x02b1, 0x1290, 0x22f3, 0x32d2,
0x4235, 0x5214, 0x6277, 0x7256, 0xb5ea, 0xa5cb,
0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447,
0x5424, 0x4405, 0xa7db, 0xb7fa, 0x8799, 0x97b8,
0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3, 0x36f2,
0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9,
0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806, 0x6827,
0x18c0, 0x08e1, 0x3882, 0x28a3, 0xcb7d, 0xdb5c,
0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0,
0x2ab3, 0x3a92, 0xfd2e, 0xed0f, 0xdd6c, 0xcd4d,
0xbdaa, 0xad8b, 0x9de8, 0x8dc9, 0x7c26, 0x6c07,
0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba,
0x8fd9, 0x9ff8, 0x6e17, 0x7e36, 0x4e55, 0x5e74,
0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

struct msp430_data *msp430_misc_data;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void msp430_early_suspend(struct early_suspend *handler);
static void msp430_late_resume(struct early_suspend *handler);
#endif

static int msp430_i2c_write_read(struct msp430_data *ps_msp430, u8 *buf,
			int writelen, int readlen)
{
	int tries, err = 0;
	struct msp_response *response;
	struct i2c_msg msgs[] = {
		{
			.addr = ps_msp430->client->addr,
			.flags = ps_msp430->client->flags,
			.len = writelen,
			.buf = buf,
		},
		{
			.addr = ps_msp430->client->addr,
			.flags = ps_msp430->client->flags | I2C_M_RD,
			.len = readlen,
			.buf = buf,
		},
	};
	if (ps_msp430->mode == FACTORYMODE)
		return err;
	if (buf == NULL || writelen == 0 || readlen == 0)
		return -EFAULT;

	if (ps_msp430->mode == BOOTMODE) {
		KDEBUG("MSP430 In msp430_i2c_write_read\n");
		KDEBUG("MSP430 sending: ");
		for (tries = 0; tries < writelen; tries++)
			KDEBUG("MSP430 %02x", buf[tries]);
	}
	tries = 0;
	do {
		err = i2c_transfer(ps_msp430->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < I2C_RETRIES));
	if (err != 2) {
		dev_err(&ps_msp430->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
		KDEBUG("MSP430 Read from MSP: ");
		for (tries = 0; tries < readlen; tries++)
			KDEBUG("MSP430 %02x", buf[tries]);

		if (ps_msp430->mode == BOOTMODE) {
			response = (struct msp_response *) buf;
			if ((response->cmd == MSP430_RESPONSE_MSG &&
				response->data != MSP430_RESPONSE_MSG_SUCCESS)
				|| (response->cmd != MSP430_RESPONSE_MSG)) {
					pr_err("i2c cmd returned failure\n");
					err = -EIO;
			}
		}
	}
	return err;
}


static int msp430_i2c_read(struct msp430_data *ps_msp430, u8 *buf, int len)
{
	int tries, err = 0;

	if (ps_msp430->mode == FACTORYMODE)
		return err;
	if (buf == NULL || len == 0)
		return -EFAULT;
	tries = 0;
	do {
		err = i2c_master_recv(ps_msp430->client, buf, len);
		if (err < 0)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err < 0) && (++tries < I2C_RETRIES));
	if (err < 0) {
		dev_err(&ps_msp430->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		KDEBUG("Read was successsful:\n");
		for (tries = 0; tries < err ; tries++)
			KDEBUG("MSP430 %02x", buf[tries]);
	}
	return err;
}

static int msp430_i2c_write(struct msp430_data *ps_msp430, u8 *buf, int len)
{
	int err = 0;
	int tries = 0;

	if (ps_msp430->mode == FACTORYMODE)
		return err;

	tries = 0;
	do {
		err = i2c_master_send(ps_msp430->client, buf, len);
		if (err < 0)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err < 0) && (++tries < I2C_RETRIES));

	if (err < 0) {
		dev_err(&ps_msp430->client->dev, "msp430: write error\n");
		err = -EIO;
	} else {
		KDEBUG("MSP430 msp430 i2c write successful\n");
		err = 0;
	}
	return err;
}

static int msp430_hw_init(struct msp430_data *ps_msp430)
{
	int err = 0;
	KDEBUG("MSP430 in msp430_hw_init\n");
	ps_msp430->hw_initialized = 1;
	return err;
}

static void msp430_device_power_off(struct msp430_data *ps_msp430)
{
	KDEBUG("MSP430 in msp430_device_power_off\n");
	if (ps_msp430->hw_initialized == 1) {
		if (ps_msp430->pdata->power_off)
			ps_msp430->pdata->power_off();
		ps_msp430->hw_initialized = 0;
	}
}

static int msp430_device_power_on(struct msp430_data *ps_msp430)
{
	int err = 0;
	KDEBUG("In msp430_device_power_on\n");
	if (ps_msp430->pdata->power_on) {
		err = ps_msp430->pdata->power_on();
		if (err < 0) {
			dev_err(&ps_msp430->client->dev,
				"power_on failed: %d\n", err);
			return err;
		}
	}
	if (!ps_msp430->hw_initialized) {
		err = msp430_hw_init(ps_msp430);
		if (err < 0) {
			msp430_device_power_off(ps_msp430);
			return err;
		}
	}
	return err;
}

static ssize_t dock_print_name(struct switch_dev *switch_dev, char *buf)
{
	switch (switch_get_state(switch_dev)) {
	case NO_DOCK:
		return sprintf(buf, "None\n");
	case DESK_DOCK:
		return sprintf(buf, "DESK\n");
	case CAR_DOCK:
		return sprintf(buf, "CAR\n");
	}

	return -EINVAL;
}

static int msp430_as_data_buffer_write(struct msp430_data *ps_msp430,
	unsigned char type, signed short data1, signed short data2,
	signed short data3, unsigned char status)
{
	int new_head;
	struct timespec ts;

	new_head = (ps_msp430->msp430_as_data_buffer_head + 1)
		& MSP430_AS_DATA_QUEUE_MASK;
	if (new_head == ps_msp430->msp430_as_data_buffer_tail) {
		pr_err("MSP430 %s: data buffer full\n", __func__);
		wake_up(&ps_msp430->msp430_as_data_wq);
		return 0;
	}

	ps_msp430->msp430_as_data_buffer[new_head].type = type;
	ps_msp430->msp430_as_data_buffer[new_head].data1 = data1;
	ps_msp430->msp430_as_data_buffer[new_head].data2 = data2;
	ps_msp430->msp430_as_data_buffer[new_head].data3 = data3;
	ps_msp430->msp430_as_data_buffer[new_head].status = status;

	ktime_get_ts(&ts);
	ps_msp430->msp430_as_data_buffer[new_head].timestamp
		= ts.tv_sec*1000000000LL + ts.tv_nsec;

	ps_msp430->msp430_as_data_buffer_head = new_head;
	wake_up(&ps_msp430->msp430_as_data_wq);

	return 1;
}

static int msp430_as_data_buffer_read(struct msp430_data *ps_msp430,
	struct msp430_android_sensor_data *buff)
{
	int new_tail;

	if (ps_msp430->msp430_as_data_buffer_tail
		== ps_msp430->msp430_as_data_buffer_head)
		return 0;

	new_tail = (ps_msp430->msp430_as_data_buffer_tail + 1)
		& MSP430_AS_DATA_QUEUE_MASK;

	*buff = ps_msp430->msp430_as_data_buffer[new_tail];

	ps_msp430->msp430_as_data_buffer_tail = new_tail;

	return 1;
}

static int msp430_ms_data_buffer_write(struct msp430_data *ps_msp430,
	unsigned char type, signed short data1, signed short data2)
{
	int new_head;
	struct timespec ts;

	new_head = (ps_msp430->msp430_ms_data_buffer_head + 1)
		& MSP430_ES_DATA_QUEUE_MASK;
	if (new_head == ps_msp430->msp430_ms_data_buffer_tail) {
		pr_err("MSP430 %s: data buffer full\n", __func__);
		wake_up(&ps_msp430->msp430_ms_data_wq);
		return 0;
	}

	ps_msp430->msp430_ms_data_buffer[new_head].type = type;
	ps_msp430->msp430_ms_data_buffer[new_head].data1 = data1;
	ps_msp430->msp430_ms_data_buffer[new_head].data2 = data2;

	ktime_get_ts(&ts);
	ps_msp430->msp430_ms_data_buffer[new_head].timestamp
		= ts.tv_sec*1000000000LL + ts.tv_nsec;

	ps_msp430->msp430_ms_data_buffer_head = new_head;
	wake_up(&ps_msp430->msp430_ms_data_wq);

	return 1;
}

static int msp430_ms_data_buffer_read(struct msp430_data *ps_msp430,
	struct msp430_moto_sensor_data *buff)
{
	int new_tail;

	if (ps_msp430->msp430_ms_data_buffer_tail
		== ps_msp430->msp430_ms_data_buffer_head)
		return 0;

	new_tail = (ps_msp430->msp430_ms_data_buffer_tail + 1)
		& MSP430_ES_DATA_QUEUE_MASK;

	*buff = ps_msp430->msp430_ms_data_buffer[new_tail];

	ps_msp430->msp430_ms_data_buffer_tail = new_tail;

	return 1;
}

static irqreturn_t msp430_isr(int irq, void *dev)
{
	struct msp430_data *ps_msp430 = dev;

	if (msp430_irq_disable) {
		disable_irq_wake(ps_msp430->irq);
		return IRQ_HANDLED;
	}

	queue_work(ps_msp430->irq_work_queue, &ps_msp430->irq_work);
	if (ps_msp430->irq_wake == -1)
		queue_work(ps_msp430->irq_work_queue,
			&ps_msp430->irq_wake_work);
	return IRQ_HANDLED;
}

static irqreturn_t msp430_wake_isr(int irq, void *dev)
{
	struct msp430_data *ps_msp430 = dev;

	if (msp430_irq_disable) {
		disable_irq_wake(ps_msp430->irq);
		return IRQ_HANDLED;
	}

	queue_work(ps_msp430->irq_work_queue, &ps_msp430->irq_wake_work);
	return IRQ_HANDLED;
}

static void msp430_irq_work_func(struct work_struct *work)
{
	int err;
	unsigned short irq_status;
	signed short x, y, z;
	struct msp430_data *ps_msp430 = container_of(work,
			struct msp430_data, irq_work);

	if (ps_msp430->mode == BOOTMODE)
		return;

	KDEBUG("MSP430 In msp430_irq_work_func\n");
	mutex_lock(&ps_msp430->lock);

	/* read interrupt mask register */
	msp_cmdbuff[0] = INTERRUPT_STATUS;
	err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 2);
	if (err < 0) {
		pr_err("MSP430 Reading from msp failed\n");
		goto EXIT;
	}
	irq_status = (msp_cmdbuff[1] << 8) | msp_cmdbuff[0];

	if (irq_status & M_ACCEL) {
		/* read accelerometer values from MSP */
		msp_cmdbuff[0] = ACCEL_X;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 6);
		if (err < 0) {
			pr_err("MSP430 Reading Accel from msp failed\n");
			goto EXIT;
		}

		x = (msp_cmdbuff[0] << 8) | msp_cmdbuff[1];
		y = (msp_cmdbuff[2] << 8) | msp_cmdbuff[3];
		z = (msp_cmdbuff[4] << 8) | msp_cmdbuff[5];
		msp430_as_data_buffer_write(ps_msp430, DT_ACCEL, x, y, z, 0);

		KDEBUG("MSP430 Sending acc(x,y,z)values:x=%d,y=%d,z=%d\n",
			x, y, z);
	}
	if (irq_status & M_ECOMPASS) {
		/*Read orientation values */
		msp_cmdbuff[0] = MAG_HX;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 13);
		if (err < 0) {
			pr_err("MSP430 Reading Ecompass failed\n");
			goto EXIT;
		}

		x = (msp_cmdbuff[0] << 8) | msp_cmdbuff[1];
		y = (msp_cmdbuff[2] << 8) | msp_cmdbuff[3];
		z = (msp_cmdbuff[4] << 8) | msp_cmdbuff[5];
		msp430_as_data_buffer_write(ps_msp430, DT_MAG, x, y, z, 0);

		KDEBUG("MSP430 Sending mag(x,y,z)values:x=%d,y=%d,z=%d\n",
			x, y, z);

		x = (msp_cmdbuff[6] << 8) | msp_cmdbuff[7];
		y = (msp_cmdbuff[8] << 8) | msp_cmdbuff[9];
		z = (msp_cmdbuff[10] << 8) | msp_cmdbuff[11];
		msp430_as_data_buffer_write(ps_msp430, DT_ORIENT, x, y, z,
			msp_cmdbuff[12]);

		KDEBUG("MSP430 Sending orient(x,y,z)values:x=%d,y=%d,z=%d\n",
		       x, y, z);
	}
	if (irq_status & M_GYRO) {
		msp_cmdbuff[0] = GYRO_X;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 6);
		if (err < 0) {
			pr_err("MSP430 Reading Gyroscope failed\n");
			goto EXIT;
		}
		x = (msp_cmdbuff[0] << 8) | msp_cmdbuff[1];
		y = (msp_cmdbuff[2] << 8) | msp_cmdbuff[3];
		z = (msp_cmdbuff[4] << 8) | msp_cmdbuff[5];
		msp430_as_data_buffer_write(ps_msp430, DT_GYRO, x, y, z, 0);

		KDEBUG("MSP430 Sending gyro(x,y,z)values:x=%d,y=%d,z=%d\n",
			x, y, z);
	}
	if (irq_status & M_ALS) {
		msp_cmdbuff[0] = ALS_LUX;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 2);
		if (err < 0) {
			pr_err("MSP430 Reading ALS from msp failed\n");
			goto EXIT;
		}
		x = (msp_cmdbuff[0] << 8) | msp_cmdbuff[1];
		msp430_as_data_buffer_write(ps_msp430, DT_ALS, x, 0, 0, 0);

		KDEBUG("Sending ALS %d\n", x);
	}
	if (irq_status & M_TEMPERATURE) {
		/*Read temperature value */
		msp_cmdbuff[0] = TEMPERATURE_DATA;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 2);
		if (err < 0) {
			pr_err("MSP430 Reading Temperature failed\n");
			goto EXIT;
		}
		x = (msp_cmdbuff[0] << 8) | msp_cmdbuff[1];
		msp430_as_data_buffer_write(ps_msp430, DT_TEMP, x, 0, 0, 0);

		KDEBUG("MSP430 Sending temp(x)value: %d\n", x);
	}
	if (irq_status & M_PRESSURE) {
		/*Read pressure value */
		msp_cmdbuff[0] = CURRENT_PRESSURE;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 4);
		if (err < 0) {
			pr_err("MSP430 Reading Pressure failed\n");
			goto EXIT;
		}
		x = (msp_cmdbuff[0] << 8) | msp_cmdbuff[1];
		y = (msp_cmdbuff[2] << 8) | msp_cmdbuff[3];
		msp430_as_data_buffer_write(ps_msp430, DT_PRESSURE, x, y, 0, 0);

		KDEBUG("MSP430 Sending pressure %d\n", (x << 16) | (y & 0xFFFF));
	}
	if (irq_status & M_DISP_ROTATE) {
		/*Read Display Rotate value */
		msp_cmdbuff[0] = DISP_ROTATE_DATA;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 1);
		if (err < 0) {
			pr_err("MSP430 Reading disp_rotate failed\n");
			goto EXIT;
		}
		x = msp_cmdbuff[0];
		msp430_as_data_buffer_write(ps_msp430,
			DT_DISP_ROTATE, x, 0, 0, 0);

		KDEBUG("MSP430 Sending disp_rotate(x)value: %d\n", x);
	}
	if (irq_status & M_DISP_BRIGHTNESS) {
		msp_cmdbuff[0] = DISPLAY_BRIGHTNESS;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 1);
		if (err < 0) {
			pr_err("MSP430 Reading Display Brightness failed\n");
			goto EXIT;
		}
		x = msp_cmdbuff[0];
		msp430_as_data_buffer_write(ps_msp430, DT_DISP_BRIGHT,
			x, 0, 0, 0);

		KDEBUG("MSP430 Sending Display Brightness %d\n", x);
	}

EXIT:
	/* For now HAE needs events even if the activity is still */
	mutex_unlock(&ps_msp430->lock);
}

static void msp430_irq_wake_work_func(struct work_struct *work)
{
	int err;
	unsigned short irq_status, irq2_status;
	signed short x, y;
	struct msp430_data *ps_msp430 = container_of(work,
			struct msp430_data, irq_wake_work);

	if (ps_msp430->mode == BOOTMODE)
		return;

	KDEBUG("MSP430 In msp430_irq_wake_work_func\n");
	mutex_lock(&ps_msp430->lock);

	/* read interrupt mask register */
	msp_cmdbuff[0] = WAKESENSOR_STATUS;
	err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 2);
	if (err < 0) {
		pr_err("MSP430 Reading from msp failed\n");
		goto EXIT;
	}
	irq_status = (msp_cmdbuff[1] << 8) | msp_cmdbuff[0];

	/* read algorithm interrupt status register */
	msp_cmdbuff[0] = ALGO_INT_STATUS;
	err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 2);
	if (err < 0) {
		pr_err("MSP430 Reading from msp failed\n");
		goto EXIT;
	}
	irq2_status = (msp_cmdbuff[1] << 8) | msp_cmdbuff[0];

	if (irq_status & M_DOCK) {
		msp_cmdbuff[0] = DOCK_DATA;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 1);
		if (err < 0) {
			pr_err("MSP430 Reading Dock state failed\n");
			goto EXIT;
		}
		x = msp_cmdbuff[0];
		msp430_as_data_buffer_write(ps_msp430, DT_DOCK, x, 0, 0, 0);
		if (ps_msp430->dsdev.dev != NULL)
			switch_set_state(&ps_msp430->dsdev, x);
		if (ps_msp430->edsdev.dev != NULL)
			switch_set_state(&ps_msp430->edsdev, x);

		KDEBUG("MSP430 Dock status:%d\n", x);
	}
	if (irq_status & M_PROXIMITY) {
		msp_cmdbuff[0] = PROXIMITY;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 1);
		if (err < 0) {
			pr_err("MSP430 Reading prox from msp failed\n");
			goto EXIT;
		}
		x = msp_cmdbuff[0];
		msp430_as_data_buffer_write(ps_msp430, DT_PROX, x, 0, 0, 0);

		KDEBUG("Sending Proximity distance %d\n", x);
	}
	if (irq_status & M_FLATUP) {
		msp_cmdbuff[0] = FLAT_DATA;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 1);
		if (err < 0) {
			pr_err("MSP430 Reading flat data from msp failed\n");
			goto EXIT;
		}
		x = msp_cmdbuff[0];
		msp430_as_data_buffer_write(ps_msp430, DT_FLAT_UP, x, 0, 0, 0);

		KDEBUG("MSP430 Sending Flat up %d\n", x);
	}
	if (irq_status & M_FLATDOWN) {
		msp_cmdbuff[0] = FLAT_DATA;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 1);
		if (err < 0) {
			pr_err("MSP430 Reading flat data from msp failed\n");
			goto EXIT;
		}
		x = msp_cmdbuff[0];
		msp430_as_data_buffer_write(ps_msp430,
			DT_FLAT_DOWN, x, 0, 0, 0);

		KDEBUG("MSP430 Sending Flat down %d\n", x);
	}
	if (irq_status & M_STOWED) {
		msp_cmdbuff[0] = STOWED;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 1);
		if (err < 0) {
			pr_err("MSP430 Reading stowed from msp failed\n");
			goto EXIT;
		}
		x = msp_cmdbuff[0];
		msp430_as_data_buffer_write(ps_msp430, DT_STOWED, x, 0, 0, 0);

		KDEBUG("Sending Stowed status %d\n", x);
	}
	if (irq2_status & M_MMOVEME) {
		/* Client recieving action will be upper 2 MSB of status */
		x = (irq2_status & MSP430_CLIENT_MASK) | M_MMOVEME;
		msp430_ms_data_buffer_write(ps_msp430, DT_MMMOVE, x, 0);

		KDEBUG("MSP430 Sending meaningful movement event\n");
	}
	if (irq2_status & M_NOMMOVE) {
		/* Client recieving action will be upper 2 MSB of status */
		x = (irq2_status & MSP430_CLIENT_MASK) | M_NOMMOVE;
		msp430_ms_data_buffer_write(ps_msp430, DT_NOMOVE, x, 0);

		KDEBUG("MSP430 Sending no meaningful movement event\n");
	}
	if (irq2_status & M_RADIAL) {
		msp_cmdbuff[0] = RADIAL_DATA;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 4);
		if (err < 0) {
			pr_err("MSP430 Reading Radial movement failed\n");
			goto EXIT;
		}
		x = (msp_cmdbuff[0] << 8) | msp_cmdbuff[1];
		y = (msp_cmdbuff[2] << 8) | msp_cmdbuff[3];
		msp430_ms_data_buffer_write(ps_msp430, DT_RADIAL, x, y);

		KDEBUG("MSP430 Radial north:%d,east:%d", x, y);
	}
EXIT:
	mutex_unlock(&ps_msp430->lock);
}

static int msp430_enable(struct msp430_data *ps_msp430)
{
	int err = 0;

	KDEBUG("MSP430 msp430_enable\n");
	if (!atomic_cmpxchg(&ps_msp430->enabled, 0, 1)) {
		err = msp430_device_power_on(ps_msp430);
		if (err < 0) {
			atomic_set(&ps_msp430->enabled, 0);
			pr_err("msp430_enable returned with %d\n", err);
			return err;
		}
	}

	return err;
}

static int msp430_misc_open(struct inode *inode, struct file *file)
{
	int err = 0;
	KDEBUG("MSP430 msp430_misc_open\n");

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;
	file->private_data = msp430_misc_data;

	err = msp430_enable(msp430_misc_data);

	return err;
}

unsigned short msp430_calculate_crc(unsigned char *data, size_t length)
{
	size_t count;
	unsigned int crc = MSP430_CRC_SEED;
	unsigned int temp;
	for (count = 0; count < length; ++count) {
		temp = (*data++ ^ (crc >> 8)) & 0xff;
		crc = crc_table[temp] ^ (crc << 8);
	}
	return (unsigned short)(crc);
}

/* the caller function is resposible to free mem allocated in this function. */
void msp430_build_command(enum msp_commands cmd,
		const char *inbuff, unsigned int *length)
{
	unsigned int index = 0, i, len = *length;
	unsigned short corecmdlen;
	unsigned short crc;
	struct msp430_data *ps_msp = msp430_misc_data;

	/*allocate for the msp command */
	msp_cmdbuff[index++] = MSP430_COMMAND_HEADER; /* header */
	switch (cmd) {
	case PASSWORD_RESET:
		msp_cmdbuff[index++] = 0x21; /* len LSB */
		msp_cmdbuff[index++] = 0x00; /* len MSB */
		msp_cmdbuff[index++] = PASSWORD_OPCODE; /* opcode */
		for (i = 4; i < 36; i++) /* followed by FFs */
			msp_cmdbuff[index++] = 0xff;
		msp_cmdbuff[index++] = 0x9E; /* CRC LSB */
		msp_cmdbuff[index++] = 0xE6; /* CRC MSB */
		break;
	case MASS_ERASE:
		msp_cmdbuff[index++] = 0x01; /* len LSB */
		msp_cmdbuff[index++] = 0x00; /* len MSB */
		msp_cmdbuff[index++] = MASSERASE_OPCODE; /* opcode */
		msp_cmdbuff[index++] = 0x64; /* crc LSB */
		msp_cmdbuff[index++] = 0xa3; /* crc MSB */
		break;
	case PROGRAM_CODE:
		/*code length */
		KDEBUG("MSP430 No of bytes got from user = %d", len);
		corecmdlen = len + MSP430_OPCODE_LENGTH + MSP430_ADDRESS_LENGTH;
		msp_cmdbuff[index++] =
			(unsigned char)(corecmdlen & 0xff); /* LSB len */
		msp_cmdbuff[index++] =
			(unsigned char)((corecmdlen >> 8) & 0xff);/*MSB len*/
		msp_cmdbuff[index++] = RXDATA_OPCODE; /* opcode */
		/* LSB of write addr on MSP */
		msp_cmdbuff[index++] =
			(unsigned char)(ps_msp->current_addr & 0xff);
		/* middle byte of write addr */
		msp_cmdbuff[index++] =
			(unsigned char)((ps_msp->current_addr >> 8) & 0xff);
		/* MSB of write addr on MSP */
		msp_cmdbuff[index++] =
			(unsigned char)((ps_msp->current_addr >> 16) & 0xff);
		/* copy data from user to kernel space */
		if (copy_from_user(msp_cmdbuff+index, inbuff, len)) {
			pr_err("MSP430 copy from user returned error\n");
			index = 0;
		} else {
			index += len; /*increment index with data len*/
			crc = msp430_calculate_crc(msp_cmdbuff+3, len+1+3);
			/* crc LSB */
			msp_cmdbuff[index++] = (unsigned char)(crc & 0xff);
			/* crc MSB */
			msp_cmdbuff[index++] = (unsigned char)((crc >> 8)&0xff);
		}
		break;
	case END_FIRMWARE:
		msp_cmdbuff[index++] = 0x06; /* len LSB */
		msp_cmdbuff[index++] = 0x00; /* len MSB */
		msp_cmdbuff[index++] = RXDATA_OPCODE; /* opcode */
		msp_cmdbuff[index++] = 0xfe;
		msp_cmdbuff[index++] = 0xff;
		msp_cmdbuff[index++] = 0x00;
		msp_cmdbuff[index++] = 0x00;
		msp_cmdbuff[index++] = 0x44;
		msp_cmdbuff[index++] = 0x89; /* crc LSB */
		msp_cmdbuff[index++] = 0xa7; /* crc MSB */
		break;
	case PASSWORD_RESET_DEFAULT:
		msp_cmdbuff[index++] = 0x21; /* len LSB */
		msp_cmdbuff[index++] = 0x00; /* len MSB */
		msp_cmdbuff[index++] = PASSWORD_OPCODE; /* opcode */
		for (i = 0; i < 32; i++) /* followed by 30 FFs */
			msp_cmdbuff[index++] = 0xff;
		msp_cmdbuff[index++] = 0x9e; /* CRC LSB */
		msp_cmdbuff[index++] = 0xE6; /* CRC MSB */
		break;
	default:
		pr_info("Invalid msp430 cmd\n");
		index = 0;
		break;
	}
	/*command length */
	*length = index;
}

static ssize_t msp430_misc_write(struct file *file, const char __user *buff,
				 size_t count, loff_t *ppos)
{
	int err = 0;
	struct msp430_data *ps_msp430;
	unsigned int len = (unsigned int)count;

	KDEBUG("MSP430 msp430_misc_write\n");
	ps_msp430 = msp430_misc_data;
	if (len > MSP430_MAXDATA_LENGTH || len == 0) {
		pr_err("packet size >MSP430_MAXDATA_LENGTH or 0\n");
		err = -EINVAL;
		return err;
	}
	KDEBUG("MSP430 Leng = %d", len); /* debug */

	mutex_lock(&ps_msp430->lock);

	if (ps_msp430->mode == BOOTMODE) {
		KDEBUG("MSP430  msp430_misc_write: boot mode\n");
		/* build the msp430 command to program code */
		msp430_build_command(PROGRAM_CODE, buff, &len);
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff,
				 len, I2C_RESPONSE_LENGTH);
		/* increment the current MSP write addr by count */
		if (err == 0) {
			msp430_misc_data->current_addr += count;
			/* return the number of bytes successfully written */
			err = len;
		}
	} else {
		KDEBUG("MSP430 msp430_misc_write: normal mode\n");
		if (copy_from_user(msp_cmdbuff, buff, count)) {
			pr_err("MSP430 copy from user returned error\n");
			err = -EINVAL;
		}
		if (err == 0)
			err = msp430_i2c_write(ps_msp430, msp_cmdbuff, count);
		if (err == 0)
			err = len;
	}
	mutex_unlock(&ps_msp430->lock);
	return err;
}

/* gpio toggling to switch modes(boot mode,normal mode)on MSP430 */
int switch_msp430_mode(enum msp_mode mode)
{
	unsigned int bslen_pin_active_value =
		msp430_misc_data->pdata->bslen_pin_active_value;

	/* bootloader mode */
	if (mode == BOOTMODE) {
		gpio_set_value(msp430_misc_data->pdata->gpio_bslen,
				(bslen_pin_active_value));
		KDEBUG("MSP430 toggling to switch to boot mode\n");
	} else {
		/*normal mode */
		gpio_set_value(msp430_misc_data->pdata->gpio_bslen,
				!(bslen_pin_active_value));
		KDEBUG("MSP430 toggling to normal or factory mode\n");
	}
	msleep_interruptible(I2C_RETRY_DELAY);
	gpio_set_value(msp430_misc_data->pdata->gpio_reset, 0);
	msleep_interruptible(I2C_RETRY_DELAY);
	gpio_set_value(msp430_misc_data->pdata->gpio_reset, 1);
	msleep_interruptible(I2C_RETRY_DELAY);
	msp430_misc_data->mode = mode;
	return 0;
}

static int msp430_get_version(struct msp430_data *ps_msp430)
{
	int err = 0;
	if (ps_msp430->mode == BOOTMODE) {
		KDEBUG("MSP430 Switch to normal to get version\n");
		switch_msp430_mode(NORMALMODE);
		msleep_interruptible(I2C_RETRY_DELAY);
	}
	KDEBUG("MSP430 MSP software version: ");
	msp_cmdbuff[0] = REV_ID;
	err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 1);
	if (err >= 0) {
		err = (int)msp_cmdbuff[0];
		pr_err("MSP430 version %02x", msp_cmdbuff[0]);
	}
	return err;
}

static int msp430_bootloadermode(struct msp430_data *ps_msp430)
{
	int err = 0;
	unsigned int cmdlen = 0;
	/* switch MSP to bootloader mode */
	KDEBUG("MSP430 Switching to bootloader mode\n");
	err = switch_msp430_mode(BOOTMODE);
	/* send password reset command to unlock MSP	 */
	KDEBUG("MSP430 Password reset for reset vector\n");
	msp430_build_command(PASSWORD_RESET, NULL, &cmdlen);
	err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff,
				cmdlen, I2C_RESPONSE_LENGTH);
	/* password reset for reset vector failed */
	if (err < 0) {
		/* password for blank reset vector */
		msp430_build_command(PASSWORD_RESET_DEFAULT, NULL, &cmdlen);
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff,
			cmdlen, I2C_RESPONSE_LENGTH);
	}
	return err;
}

static int msp430_test_write_read(struct msp430_data *ps_msp430,
				void __user *argp)
{
	int err = 0;
	unsigned short readwritebyte;
	unsigned char reg;
	unsigned int bytecount;
	int i;
	if (copy_from_user(&readwritebyte, argp, sizeof(unsigned short))) {
		pr_err("copy from user returned error\n");
		return -EFAULT;
	}
	bytecount = (readwritebyte >> 8) & 0xff;
	reg = readwritebyte & 0xff;
	msp_cmdbuff[0] = reg;
	err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, bytecount);
	if (err < -1)
		pr_err("MSP430 Failed to write read\n");
	if (copy_to_user(argp, &bytecount, sizeof(unsigned short)))
		return -EFAULT;
	for (i = 0; i < bytecount; i++)
		KDEBUG("%02x ", msp_cmdbuff[i]);
	return err;
}

static long msp430_misc_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	static int brightness_table_loaded;
	int err = -ENOTTY;
	unsigned int addr = 0;
	struct msp430_data *ps_msp430 = file->private_data;
	unsigned int cmdlen = 0;
	unsigned char byte;
	unsigned char bytes[2];
	unsigned short delay;
	int index;

	mutex_lock(&ps_msp430->lock);

	KDEBUG("MSP430 msp430_misc_ioctl = %d\n", cmd);
	switch (cmd) {
	case MSP430_IOCTL_BOOTLOADERMODE:
		err = msp430_bootloadermode(ps_msp430);
		break;
	case MSP430_IOCTL_NORMALMODE:
		err = switch_msp430_mode(NORMALMODE);
		break;
	case MSP430_IOCTL_MASSERASE:
		msp430_build_command(MASS_ERASE, NULL, &cmdlen);
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff,
					cmdlen, I2C_RESPONSE_LENGTH);
		break;
	case MSP430_IOCTL_SETSTARTADDR:
		if (copy_from_user(&addr, argp, sizeof(addr))) {
			pr_err("copy start address returned error\n");
			err = -EFAULT;
			break;
		}
		/* store the start addr */
		msp430_misc_data->current_addr = addr;
		err = 0;
		break;
	case MSP430_IOCTL_SET_FACTORY_MODE:
		KDEBUG("MSP430 set factory mode\n");
		err = switch_msp430_mode(FACTORYMODE);
		break;
	case MSP430_IOCTL_TEST_BOOTMODE:
		/* switch MSP to bootloader mode */
		err = switch_msp430_mode(BOOTMODE);
		break;

	case MSP430_IOCTL_SET_DEBUG:
		/* enable or disble msp driver debug messages */
		if (copy_from_user(&g_debug, argp, sizeof(g_debug))) {
			KDEBUG("copy set debug state returned error\n");
			err = -EFAULT;
			break;
		}
		err = 0;
		break;
	default:
		/* The IOCLTS below in next switch should not be called
		   when device is in boot mode */
		if (ps_msp430->mode == BOOTMODE) {
			pr_err("MSP430 Attempted normal mode ioctl in boot\n");
			mutex_unlock(&ps_msp430->lock);
			return -EFAULT;
		}
	}


	switch (cmd) {
	case MSP430_IOCTL_GET_VERSION:
		err = msp430_get_version(ps_msp430);
		break;
	case MSP430_IOCTL_SET_ACC_DELAY:
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			KDEBUG("copy acc delay returned error\n");
			err = -EFAULT;
			break;
		}
		msp_cmdbuff[0] = ACCEL_UPDATE_RATE;
		msp_cmdbuff[1] = delay;
		err = msp430_i2c_write(ps_msp430, msp_cmdbuff, 2);
		break;

	case MSP430_IOCTL_SET_MAG_DELAY:
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			KDEBUG("copy mag delay returned error\n");
			err = -EFAULT;
		} else {
			msp_cmdbuff[0] = MAG_UPDATE_RATE;
			msp_cmdbuff[1] = delay;
			err = msp430_i2c_write(ps_msp430, msp_cmdbuff, 2);
		}
		break;
	case MSP430_IOCTL_SET_GYRO_DELAY:
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			KDEBUG("copy gyro delay returned error\n");
			err = -EFAULT;
			break;
		}
		msp_cmdbuff[0] = GYRO_UPDATE_RATE;
		msp_cmdbuff[1] = delay;
		err = msp430_i2c_write(ps_msp430, msp_cmdbuff, 2);
		break;
	case MSP430_IOCTL_SET_PRES_DELAY:
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			KDEBUG("copy pres delay returned error\n");
			err = -EFAULT;
			break;
		}
		msp_cmdbuff[0] = PRESSURE_UPDATE_RATE;
		msp_cmdbuff[1] = delay;
		err = msp430_i2c_write(ps_msp430, msp_cmdbuff, 2);
		break;
	case MSP430_IOCTL_SET_SENSORS:
		if (copy_from_user(bytes, argp, 2 * sizeof(unsigned char))) {
			KDEBUG("copy set sensors returned error\n");
			err = -EFAULT;
			break;
		}

		if ((brightness_table_loaded == 0)
				&& (bytes[1] & (M_DISP_BRIGHTNESS >> 8))) {
			msp_cmdbuff[0] = LUX_TABLE_VALUES;
			for (index = 0; index < LIGHTING_TABLE_SIZE; index++) {
				msp_cmdbuff[(2 * index) + 1]
					= ps_msp430->pdata->lux_table[index]
					>> 8;
				msp_cmdbuff[(2 * index) + 2]
					= ps_msp430->pdata->lux_table[index]
					& 0xFF;
			}
			err = msp430_i2c_write(ps_msp430, msp_cmdbuff,
				(2 * LIGHTING_TABLE_SIZE) + 1);
			if (err)
				break;
			msp_cmdbuff[0] = BRIGHTNESS_TABLE_VALUES;
			for (index = 0; index < LIGHTING_TABLE_SIZE; index++) {
				msp_cmdbuff[index + 1]
				= ps_msp430->pdata->brightness_table[index];
			}
			err = msp430_i2c_write(ps_msp430, msp_cmdbuff,
				LIGHTING_TABLE_SIZE + 1);
			if (err)
				break;
			KDEBUG("MSP430 brightness tables loaded\n");
			brightness_table_loaded = 1;
		}

		msp_cmdbuff[0] = NONWAKESENSOR_CONFIG;
		msp_cmdbuff[1] = bytes[0];
		msp_cmdbuff[2] = bytes[1];
		err = msp430_i2c_write(ps_msp430, msp_cmdbuff, 3);
		/* save sensor state any time this changes */
		g_nonwake_sensor_state = (msp_cmdbuff[2] << 8) | msp_cmdbuff[1];
		break;
	case MSP430_IOCTL_GET_SENSORS:
		msp_cmdbuff[0] = NONWAKESENSOR_CONFIG;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 2);
		if (err < 0) {
			pr_err("MSP430 Reading get sensors failed\n");
			break;
		}
		bytes[0] = msp_cmdbuff[0];
		bytes[1] = msp_cmdbuff[1];
		if (copy_to_user(argp, bytes, 2 * sizeof(unsigned char)))
			err = -EFAULT;
		break;
	case MSP430_IOCTL_SET_WAKESENSORS:
		if (copy_from_user(bytes, argp, 2 * sizeof(unsigned char))) {
			KDEBUG("copy set sensors returned error\n");
			return -EFAULT;
		}
		msp_cmdbuff[0] = WAKESENSOR_CONFIG;
		msp_cmdbuff[1] = bytes[0];
		msp_cmdbuff[2] = bytes[1];
		err = msp430_i2c_write(ps_msp430, msp_cmdbuff, 3);
		/* save sensor state any time this changes */
		g_wake_sensor_state =  (msp_cmdbuff[2] << 8) | msp_cmdbuff[1];
		break;
	case MSP430_IOCTL_GET_WAKESENSORS:
		msp_cmdbuff[0] = WAKESENSOR_CONFIG;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 2);
		if (err < 0) {
			pr_err("MSP430 Reading get sensors failed\n");
			break;
		}
		bytes[0] = msp_cmdbuff[0];
		bytes[1] = msp_cmdbuff[1];
		if (copy_to_user(argp, bytes, 2 * sizeof(unsigned char)))
			return -EFAULT;
		break;
	case MSP430_IOCTL_SET_ALGOS:
		byte = 0;
		if (copy_from_user(&byte, argp, sizeof(byte))) {
			KDEBUG("copy set algos returned error\n");
			err = -EFAULT;
			break;
		}
		msp_cmdbuff[0] = ALGO_CONFIG;
		msp_cmdbuff[1] = byte;
		err = msp430_i2c_write(ps_msp430, msp_cmdbuff, 2);
		break;
	case MSP430_IOCTL_GET_ALGOS:
		msp_cmdbuff[0] = ALGO_CONFIG;
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 1);
		if (err < 0) {
			pr_err("MSP430 Reading get algos failed\n");
			break;
		}
		byte = msp_cmdbuff[0];
		if (copy_to_user(argp, &byte, sizeof(byte)))
			err = -EFAULT;
		break;
	case MSP430_IOCTL_SET_RADIAL_DUR:
		byte = 0;
		if (copy_from_user(&addr, argp, sizeof(addr))) {
			KDEBUG("copy set radial dur returned error\n");
			err = -EFAULT;
			break;
		}
		msp_cmdbuff[0] = RADIAL_DUR;
		msp_cmdbuff[1] = addr >> 8;
		msp_cmdbuff[2] = addr & 0xFF;
		err = msp430_i2c_write(ps_msp430, msp_cmdbuff, 3);
		break;
	case MSP430_IOCTL_SET_MOTION_DUR:
		if (copy_from_user(&addr, argp, sizeof(addr))) {
			KDEBUG("copy set motion dur returned error\n");
			err = -EFAULT;
			break;
		}
		byte = addr >> 8;
		if (byte < sizeof(msp_motion_dur_a))
			msp_cmdbuff[0] = msp_motion_dur_a[byte];
		else {
			KDEBUG("invalid set motion dur arg passed in\n");
			err = -EFAULT;
			break;
		}
		msp_cmdbuff[1] = addr & 0xFF;
		err = msp430_i2c_write(ps_msp430, msp_cmdbuff, 2);
		break;
	case MSP430_IOCTL_SET_ZRMOTION_DUR:
		if (copy_from_user(&addr, argp, sizeof(addr))) {
			KDEBUG("copy zmotion dur returned error\n");
			err = -EFAULT;
			break;
		}
		byte = addr >> 8;
		if (byte < sizeof(msp_zrmotion_dur_a))
			msp_cmdbuff[0] = msp_zrmotion_dur_a[byte];
		else {
			KDEBUG("invalid zmotion dur arg passed in\n");
			err = -EFAULT;
			break;
		}
		msp_cmdbuff[1] = addr & 0xFF;
		err = msp430_i2c_write(ps_msp430, msp_cmdbuff, 2);
		break;

	case MSP430_IOCTL_GET_DOCK_STATUS:
		err = msp430_i2c_write_read(ps_msp430, msp_cmdbuff, 1, 1);
		byte = msp_cmdbuff[0];
		if (copy_to_user(argp, &byte, sizeof(byte)))
			err = -EFAULT;
		break;
	case MSP430_IOCTL_TEST_READ:
		err = msp430_i2c_read(ps_msp430, &byte, 1);
		/* msp430 will return num of bytes read or error */
		if (err > 0)
			err = byte;
		break;
	case MSP430_IOCTL_TEST_WRITE:
		if (copy_from_user(&byte, argp, sizeof(unsigned char))) {
			pr_err("copy test write returned error\n");
			err = -EFAULT;
			break;
		}
		err = msp430_i2c_write(ps_msp430, &byte, 1);
		break;
	case MSP430_IOCTL_TEST_WRITE_READ:
		err = msp430_test_write_read(ps_msp430, argp);
		break;
	case MSP430_IOCTL_GET_VERNAME:
		if (copy_to_user(argp, &(ps_msp430->pdata->fw_version),
				FW_VERSION_SIZE))
			err = -EFAULT;
		else
			err = 0;
		break;
	/* No default here since previous switch could have
	   handled the command and cannot over write that */
	}

	mutex_unlock(&ps_msp430->lock);
	return err;
}

static int msp430_as_open(struct inode *inode, struct file *file)
{
	int err = 0;

	KDEBUG("MSP430 msp430_as_open\n");

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;
	file->private_data = msp430_misc_data;

	return err;
}

static ssize_t msp430_as_read(struct file *file, char __user *buffer,
	size_t size, loff_t *ppos)
{
	int ret;
	struct msp430_android_sensor_data tmp_buff;
	struct msp430_data *ps_msp430 = file->private_data;

	ret = msp430_as_data_buffer_read(ps_msp430, &tmp_buff);
	if (ret == 0)
		return 0;
	ret = copy_to_user(buffer, &tmp_buff,
		sizeof(struct msp430_android_sensor_data));
	if (ret != 0) {
		pr_err("%s: copy error\n", __func__);
		return 0;
	}

	return sizeof(struct msp430_android_sensor_data);
}

static unsigned int msp430_as_poll(struct file *file,
	struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct msp430_data *ps_msp430 = file->private_data;

	poll_wait(file, &ps_msp430->msp430_as_data_wq, wait);
	if (ps_msp430->msp430_as_data_buffer_head
		!= ps_msp430->msp430_as_data_buffer_tail)
		mask = POLLIN | POLLRDNORM;

	return mask;
}

static const struct file_operations msp430_as_fops = {
	.owner = THIS_MODULE,
	.open = msp430_as_open,
	.read = msp430_as_read,
	.poll = msp430_as_poll,
};

static int msp430_ms_open(struct inode *inode, struct file *file)
{
	int err = 0;

	KDEBUG("MSP430 msp430_ms_open\n");

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;
	file->private_data = msp430_misc_data;

	return err;
}

static ssize_t msp430_ms_read(struct file *file, char __user *buffer,
	size_t size, loff_t *ppos)
{
	int ret;
	struct msp430_moto_sensor_data tmp_buff;
	struct msp430_data *ps_msp430 = file->private_data;

	ret = msp430_ms_data_buffer_read(ps_msp430, &tmp_buff);
	if (copy_to_user(buffer, &tmp_buff,
		sizeof(struct msp430_moto_sensor_data))
		!= 0) {
		pr_err("%s: copy error\n", __func__);
		ret = 0;
	}

	return ret;
}

static unsigned int msp430_ms_poll(struct file *file,
	struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct msp430_data *ps_msp430 = file->private_data;

	poll_wait(file, &ps_msp430->msp430_ms_data_wq, wait);
	if (ps_msp430->msp430_ms_data_buffer_head
		!= ps_msp430->msp430_ms_data_buffer_tail)
		mask = POLLIN | POLLRDNORM;

	return mask;
}

static const struct file_operations msp430_ms_fops = {
	.owner = THIS_MODULE,
	.open = msp430_ms_open,
	.read = msp430_ms_read,
	.poll = msp430_ms_poll,
};

static const struct file_operations msp430_misc_fops = {
	.owner = THIS_MODULE,
	.open = msp430_misc_open,
	.unlocked_ioctl = msp430_misc_ioctl,
	.write = msp430_misc_write,
};

static struct miscdevice msp430_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = NAME,
	.fops = &msp430_misc_fops,
};


static int msp430_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct msp430_data *ps_msp430;
	int err = -1;
	dev_info(&client->dev, "msp430 probe begun\n");

	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL, exiting\n");
		err = -ENODEV;
		goto err0;
	}
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto err0;
	}
	ps_msp430 = kzalloc(sizeof(*ps_msp430), GFP_KERNEL);
	if (ps_msp430 == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
			"failed to allocate memory for module data: %d\n", err);
		goto err0;
	}

	mutex_init(&ps_msp430->lock);
	mutex_lock(&ps_msp430->lock);
	wake_lock_init(&ps_msp430->wakelock, WAKE_LOCK_SUSPEND, "msp430");

	ps_msp430->client = client;
	ps_msp430->mode = UNINITIALIZED;


	/* Set to passive mode by default */
	g_debug = 0;
	g_nonwake_sensor_state = 0;
	g_wake_sensor_state = 0;
	/* clear the interrupt mask */
	ps_msp430->intp_mask = 0x00;

	INIT_WORK(&ps_msp430->irq_work, msp430_irq_work_func);
	INIT_WORK(&ps_msp430->irq_wake_work, msp430_irq_wake_work_func);

	ps_msp430->irq_work_queue = create_singlethread_workqueue("msp430_wq");
	if (!ps_msp430->irq_work_queue) {
		err = -ENOMEM;
		dev_err(&client->dev, "cannot create work queue: %d\n", err);
		goto err1;
	}
	ps_msp430->pdata = kmalloc(sizeof(*ps_msp430->pdata), GFP_KERNEL);
	if (ps_msp430->pdata == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
			"failed to allocate memory for pdata: %d\n", err);
		goto err2;
	}
	memcpy(ps_msp430->pdata, client->dev.platform_data,
		sizeof(*ps_msp430->pdata));
	i2c_set_clientdata(client, ps_msp430);
	ps_msp430->client->flags &= 0x00;

	if (ps_msp430->pdata->init) {
		err = ps_msp430->pdata->init();
		if (err < 0) {
			dev_err(&client->dev, "init failed: %d\n", err);
			goto err3;
		}
	}

	/*configure interrupt*/
	ps_msp430->irq = gpio_to_irq(ps_msp430->pdata->gpio_int);
	if (ps_msp430->pdata->gpio_wakeirq != -1)
		ps_msp430->irq_wake
			= gpio_to_irq(ps_msp430->pdata->gpio_wakeirq);
	else
		ps_msp430->irq_wake = -1;

	err = msp430_device_power_on(ps_msp430);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err4;
	}

	if (ps_msp430->irq_wake != -1)
		enable_irq_wake(ps_msp430->irq_wake);
	atomic_set(&ps_msp430->enabled, 1);

	msp430_misc_data = ps_msp430;
	err = misc_register(&msp430_misc_device);
	if (err < 0) {
		dev_err(&client->dev, "misc register failed: %d\n", err);
		goto err6;
	}

	if (alloc_chrdev_region(&ps_msp430->msp430_dev_num, 0, 2, "msp430")
		< 0)
		pr_err("%s: alloc_chrdev_region failed\n", __func__);
	ps_msp430->msp430_class = class_create(THIS_MODULE, "msp430");

	cdev_init(&ps_msp430->as_cdev, &msp430_as_fops);
	ps_msp430->as_cdev.owner = THIS_MODULE;
	err = cdev_add(&ps_msp430->as_cdev, ps_msp430->msp430_dev_num, 1);
	if (err)
		pr_err("%s: cdev_add as failed: %d\n", __func__, err);

	device_create(ps_msp430->msp430_class, NULL,
		MKDEV(MAJOR(ps_msp430->msp430_dev_num), 0),
		ps_msp430, "msp430_as");

	cdev_init(&ps_msp430->ms_cdev, &msp430_ms_fops);
	ps_msp430->ms_cdev.owner = THIS_MODULE;
	err = cdev_add(&ps_msp430->ms_cdev, ps_msp430->msp430_dev_num + 1, 1);
	if (err)
		pr_err("%s: cdev_add ms failed: %d\n", __func__, err);

	device_create(ps_msp430->msp430_class, NULL,
		MKDEV(MAJOR(ps_msp430->msp430_dev_num), 1),
		ps_msp430, "msp430_ms");

	msp430_device_power_off(ps_msp430);

	atomic_set(&ps_msp430->enabled, 0);

	err = request_irq(ps_msp430->irq, msp430_isr,
				(IRQF_TRIGGER_RISING | IRQF_TRIGGER_HIGH),
				NAME, ps_msp430);
	if (err < 0) {
		dev_err(&client->dev, "request irq failed: %d\n", err);
		goto err7;
	}

	if (ps_msp430->irq_wake != -1) {
		err = request_irq(ps_msp430->irq_wake, msp430_wake_isr,
				(IRQF_TRIGGER_RISING | IRQF_TRIGGER_HIGH),
				NAME, ps_msp430);
		if (err < 0) {
			dev_err(&client->dev, "request wake irq failed: %d\n",
				err);
			goto err8;
		}
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ps_msp430->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ps_msp430->early_suspend.suspend = msp430_early_suspend;
	ps_msp430->early_suspend.resume = msp430_late_resume;
	register_early_suspend(&ps_msp430->early_suspend);
#endif

	init_waitqueue_head(&ps_msp430->msp430_as_data_wq);
	init_waitqueue_head(&ps_msp430->msp430_ms_data_wq);

	ps_msp430->dsdev.name = "dock";
	ps_msp430->dsdev.print_name = dock_print_name;
	err = switch_dev_register(&ps_msp430->dsdev);
	if (err) {
		pr_err("couldn't register switch (%s) rc=%d\n",
					ps_msp430->dsdev.name, err);
		ps_msp430->dsdev.dev = NULL;
	}

	ps_msp430->edsdev.name = "extdock";
	ps_msp430->edsdev.print_name = dock_print_name;
	err = switch_dev_register(&ps_msp430->edsdev);
	if (err) {
		pr_err("couldn't register switch (%s) rc=%d\n",
					ps_msp430->edsdev.name, err);
		ps_msp430->edsdev.dev = NULL;
	}

	mutex_unlock(&ps_msp430->lock);

	dev_info(&client->dev, "msp430 probed\n");

	return 0;

err8:
	free_irq(ps_msp430->irq, ps_msp430);
err7:
	misc_deregister(&msp430_misc_device);
err6:
	msp430_device_power_off(ps_msp430);
err4:
	if (ps_msp430->pdata->exit)
		ps_msp430->pdata->exit();
err3:
	kfree(ps_msp430->pdata);
err2:
	destroy_workqueue(ps_msp430->irq_work_queue);
err1:
	mutex_unlock(&ps_msp430->lock);
	mutex_destroy(&ps_msp430->lock);
	wake_lock_destroy(&ps_msp430->wakelock);
	kfree(ps_msp430);
err0:
	return err;
}

static int msp430_remove(struct i2c_client *client)
{
	struct msp430_data *ps_msp430 = i2c_get_clientdata(client);
	pr_err("MSP430 msp430_remove\n");
	switch_dev_unregister(&ps_msp430->dsdev);
	switch_dev_unregister(&ps_msp430->edsdev);
	if (ps_msp430->irq_wake != -1)
		free_irq(ps_msp430->irq_wake, ps_msp430);
	free_irq(ps_msp430->irq, ps_msp430);
	misc_deregister(&msp430_misc_device);
	msp430_device_power_off(ps_msp430);
	if (ps_msp430->pdata->exit)
		ps_msp430->pdata->exit();
	kfree(ps_msp430->pdata);
	destroy_workqueue(ps_msp430->irq_work_queue);
	mutex_destroy(&ps_msp430->lock);
	wake_lock_destroy(&ps_msp430->wakelock);
	disable_irq_wake(ps_msp430->irq);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ps_msp430->early_suspend);
#endif
	kfree(ps_msp430);

	return 0;
}

static int msp430_resume(struct i2c_client *client)
{
	struct msp430_data *ps_msp430 = i2c_get_clientdata(client);
	KDEBUG("MSP430 msp430_resume\n");
	mutex_lock(&ps_msp430->lock);

	if ((ps_msp430->mode == NORMALMODE) && (g_nonwake_sensor_state != 0)) {
		/* restore traditional sensor data */
		msp_cmdbuff[0] = NONWAKESENSOR_CONFIG;
		msp_cmdbuff[1] = g_nonwake_sensor_state & 0xFF;
		msp_cmdbuff[2] = g_nonwake_sensor_state >> 8;
		msp430_i2c_write(ps_msp430, msp_cmdbuff, 3);
	}

	mutex_unlock(&ps_msp430->lock);
	return 0;
}

static int msp430_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct msp430_data *ps_msp430 = i2c_get_clientdata(client);
	KDEBUG("MSP430 msp430_suspend\n");
	mutex_lock(&ps_msp430->lock);

	if ((ps_msp430->mode == NORMALMODE) && (g_nonwake_sensor_state != 0)) {
		/* turn off traditional sensor data */
		msp_cmdbuff[0] = NONWAKESENSOR_CONFIG;
		msp_cmdbuff[1] = 0;
		msp_cmdbuff[2] = 0;
		msp430_i2c_write(ps_msp430, msp_cmdbuff, 3);
	}

	mutex_unlock(&ps_msp430->lock);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void msp430_early_suspend(struct early_suspend *handler)
{
	struct msp430_data *ps_msp430;

	ps_msp430 = container_of(handler, struct msp430_data, early_suspend);
	msp430_suspend(ps_msp430->client, PMSG_SUSPEND);
}

static void msp430_late_resume(struct early_suspend *handler)
{
	struct msp430_data *ps_msp430;

	ps_msp430 = container_of(handler, struct msp430_data, early_suspend);
	msp430_resume(ps_msp430->client);
}
#endif

static const struct i2c_device_id msp430_id[] = {
	{NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, msp430_id);

static struct i2c_driver msp430_driver = {
	.driver = {
		   .name = NAME,
		   .owner = THIS_MODULE,
		   },
	.probe = msp430_probe,
	.remove = msp430_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.resume = msp430_resume,
	.suspend = msp430_suspend,
#endif
	.id_table = msp430_id,
};

static int __init msp430_init(void)
{
	pr_info(KERN_ERR "MSP430 msp430 sensor processor\n");
	return i2c_add_driver(&msp430_driver);
}

static void __exit msp430_exit(void)
{
	pr_err("MSP430 msp430_exit\n");
	i2c_del_driver(&msp430_driver);
	return;
}

module_init(msp430_init);
module_exit(msp430_exit);

MODULE_DESCRIPTION("MSP430 sensor processor");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
