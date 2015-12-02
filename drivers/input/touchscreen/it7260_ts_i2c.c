/* drivers/input/touchscreen/IT7260_ts_i2c.c
 *
 * Copyright (C) 2014 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * this is a mostly complete rewrite of the original driver. some functionality like
 * "goldensample" was removed due to not being used and being very poorly documented
 * (thus I was unable to rewrite it since I had no idea what it was supposed to do).
 * XXX:
 * Much like the original driver, this one does not support multitouch. The chip can
 * but the driver does not. At some point [soon] we may want to address this.
 * Also there is no consitency in this driver as to when it waits for chip ready and
 * when it does not. Additionalyl errors from i2c are almost always ignored.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <asm/uaccess.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/firmware.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/sync.h>
#include <linux/proc_fs.h>
#include <linux/wakelock.h>
#include <linux/sensors.h>
#ifdef CONFIG_ASUS_UTILITY
#include <linux/notifier.h>
#include <linux/asus_utility.h>
#include <linux/input/mt.h>
#endif

#define MAX_BUFFER_SIZE			144
#define DEVICE_NAME			"IT7260"
#ifdef CONFIG_ASUS_WREN
#define SCREEN_X_RESOLUTION		280
#define SCREEN_Y_RESOLUTION		280
#else
#define SCREEN_X_RESOLUTION		320
#define SCREEN_Y_RESOLUTION		320
#endif

#define IOC_MAGIC			'd'
#define IOCTL_SET				_IOW(IOC_MAGIC, 1, struct ioctl_cmd168)
#define IOCTL_GET				_IOR(IOC_MAGIC, 2, struct ioctl_cmd168)

#define MAGIC_KEY_NONE			0
#define MAGIC_KEY_PALM			1

#define BUF_COMMAND			0x20 /* all commands writes go to this idx */
#define BUF_SYS_COMMAND			0x40
#define BUF_QUERY			0x80 /* "revice ready?" and "wake up please" and "read touch data" reads go to this idx */
#define BUF_RESPONSE			0xA0 /* most command responce reads go to this idx */
#define BUF_SYS_RESPONSE		0xC0
#define BUF_POINT_INFO			0xE0 /* reads of "point" go through here and produce 14 bytes of data */

/* commands and their subcommands. when no subcommands exist, a zero is send as the second byte */
#define CMD_IDENT_CHIP			0x00
#define CMD_READ_VERSIONS		0x01 /* VERSION_LENGTH bytes of data in response */
# define VER_FIRMWARE			0x00
# define VER_CONFIG			0x06
# define VERSION_LENGTH			10
#define CMD_PWR_CTL			0x04 /* subcommand is zero, next byte is power mode */
# define PWR_CTL_LOW_POWER_MODE	0x01
# define PWR_CTL_SLEEP_MODE		0x02
#define CMD_UNKNOWN_7			0x07 /* command is not documented in the datasheet v1.0.0.7 */
#define CMD_FIRMWARE_REINIT_C	0x0C
#define CMD_CALIBRATE			0x13 /* needs to be followed by 4 bytes of zeroes */
#define CMD_FIRMWARE_UPGRADE	0x60
# define FIRMWARE_MODE_ENTER	0x00
# define FIRMWARE_MODE_EXIT		0x80
#define CMD_SET_START_OFFSET	0x61 /* address for FW read/write */
#define CMD_FW_WRITE			0x62 /* subcommand is number of bytes to write */
#define CMD_FW_READ			0x63 /* subcommand is number of bytes to read */
#define CMD_FIRMWARE_REINIT_6F	0x6F

#define FW_WRITE_CHUNK_SIZE			128
#define FW_WRITE_RETRY_COUNT		4
#define CHIP_FLASH_SIZE			0x8000
#define SYSFS_FW_UPLOAD_MODE_MANUAL	2
#define SYSFS_FW_UPDATE_SCRIPT	1
#define SYSFS_POINT_LOG_OPEN	1
#define SYSFS_RESULT_FAIL		(-1)
#define SYSFS_RESULT_NOT_DONE		0
#define SYSFS_RESULT_SUCCESS		1
#define DEVICE_READY_MAX_WAIT		500
#define DEVICE_READY_MAX_PROBE_WAIT		50
#define DEVICE_READY_MAX_UPDATE_WAIT		50

//result of reading with BUF_QUERY bits
#define CMD_STATUS_BITS			0x07
#define CMD_STATUS_DONE			0x00
#define CMD_STATUS_BUSY			0x01
#define CMD_STATUS_ERROR		0x02
#define PT_INFO_BITS			0xF8
#define BT_INFO_NONE			0x00
#define PT_INFO_YES			0x80
#define BT_INFO_NONE_BUT_DOWN	0x08 /* no new data but finder(s) still down */

#define FW_CUSTOMER_ID1			5
#define FW_CUSTOMER_ID2			13
#ifdef CONFIG_ASUS_WREN
#define CFG_CUSTOMER_ID1			7
#define CFG_CUSTOMER_ID2			2
#else
#define CFG_CUSTOMER_ID1			7
#define CFG_CUSTOMER_ID2			1
#endif

static int fw_upgrade_flag = 0;
static int config_upgrade_flag = 0;
static int enter_lowpower_flag = 0;
static bool probe_flag = false;
static bool update_flag = false;
static bool point_log = false;

//show touch point message flag
static int TOUCH_P1_DOWN_FLAG = 0;
static int TOUCH_P2_DOWN_FLAG = 0;
static int AMB_TOUCH_HAD_SEND_FLAG = 0;
static uint16_t AMB_x1, AMB_y1;

/* use this to include integers in commands */
#define CMD_UINT16(v)		((uint8_t)(v)) , ((uint8_t)((v) >> 8))

struct FingerData {
	uint8_t xLo;
	uint8_t hi;
	uint8_t yLo;
	uint8_t pressure;
} __attribute__((packed));

struct PointData {
	uint8_t flags;
	uint8_t palm;
	struct FingerData fd[3];
} __attribute__((packed));

#define PD_FLAGS_DATA_TYPE_BITS		0xF0
#define PD_FLAGS_DATA_TYPE_TOUCH	0x00 /* other types (like chip-detected gestures) exist but we do not care */
#define PD_FLAGS_NOT_PEN		0x08 /* set if pen touched, clear if finger(s) */
#define PD_FLAGS_HAVE_FINGERS		0x07 /* a bit for each finger data that is valid (from lsb to msb) */
#define PD_PALM_FLAG_BIT		0x01
#define FD_PRESSURE_BITS			0x0F
#define FD_PRESSURE_NONE			0x00
#define FD_PRESSURE_HOVER			0x01
#define FD_PRESSURE_LIGHT			0x02
#define FD_PRESSURE_NORMAL			0x04
#define FD_PRESSURE_HIGH			0x08
#define FD_PRESSURE_HEAVY			0x0F

struct ioctl_cmd168 {
	uint16_t bufferIndex;
	uint16_t length;
	uint16_t buffer[MAX_BUFFER_SIZE];
};

/* add delay_work for palm */
struct IT7260_ts_data {
	struct i2c_client *client;
	struct input_dev *touch_dev;
	struct delayed_work afterpalm_work;
	struct delayed_work touchidle_on_work;
	struct delayed_work exit_idle_work;
	struct input_dev *palm_dev;
	struct sensors_classdev cdev;
	bool palm_en;
};

struct ite7260_perfile_data {
	rwlock_t lock;
	uint16_t bufferIndex;
	uint16_t length;
	uint16_t buffer[MAX_BUFFER_SIZE];
};

enum {
	TOUCH_UP = 0,
	TOUCH_DOWN = 1
};

static int8_t fwUploadResult = SYSFS_RESULT_NOT_DONE;
static int8_t calibrationWasSuccessful = SYSFS_RESULT_NOT_DONE;
static bool devicePresent = false;
static DEFINE_MUTEX(sleepModeMutex);
static bool chipAwake = true;
static bool hadFingerDown = false;
static bool hadPalmDown = false;
static bool isTouchLocked = false;
static bool isDriverAvailable = true;
static bool chipInLowPower = false;
static bool driverInLowPower = false;
static bool TP_DLMODE = false;
static bool first_leavelowpower = true;
static bool touchMissed;
static uint64_t suspend_touch_down = 0;
static uint64_t suspend_touch_up = 0;
static uint64_t last_time_send_palm = 0;
static uint64_t exit_idle_event_time = 0;
static struct IT7260_ts_data *gl_ts;
static struct wake_lock touch_lock;
static struct wake_lock touch_time_lock;
static int lastTouch = TOUCH_UP;
static unsigned long last_time_exit_low = 0;
static char fwVersion[20];
static int RESET_GPIO = 0;
static int TP_DLMODE_GPIO_VALUE = 0;
static int ret = -1;

#define TP_DLMODE_GPIO			30
#define I2C_RETRY_DELAY			15		/* Waiting for signals [ms] */
#define I2C_RETRIES				2		/* Number of retries */
#define WAKELOCK_HOLD_MS		(HZ/2)

#define LOGE(...)	pr_err(DEVICE_NAME " TOUCH_ERR: " __VA_ARGS__)
#define LOGI(...)	printk(DEVICE_NAME ": " __VA_ARGS__)

/* add workqueue for delay_work */
static struct workqueue_struct *IT7260_wq;

/* internal use func - does not make sure chip is ready before read */
static int i2cReadNoReadyCheck(uint8_t bufferIndex, uint8_t *dataBuffer, uint16_t dataLength)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[2] = {
		{
			.addr = gl_ts->client->addr,
			.flags = I2C_M_NOSTART,
			.len = 1,
			.buf = &bufferIndex
		},
		{
			.addr = gl_ts->client->addr,
			.flags = I2C_M_RD,
			.len = dataLength,
			.buf = dataBuffer
		}
	};

	memset(dataBuffer, 0xFF, dataLength);

	do {
		err = i2c_transfer(gl_ts->client->adapter, msgs, 2);
		if (err != 2)
		    msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		err = -EIO;
	}

	return err;
}

static int i2cWriteNoReadyCheck(uint8_t bufferIndex, const uint8_t *dataBuffer, uint16_t dataLength)
{
	int err;
	int tries = 0;

	uint8_t txbuf[257];
	struct i2c_msg msg = {
		.addr = gl_ts->client->addr,
		.flags = 0,
		.len = dataLength + 1,
		.buf = txbuf
	};

	/* just to be careful */
	BUG_ON(dataLength > sizeof(txbuf) - 1);

	txbuf[0] = bufferIndex;
	memcpy(txbuf + 1, dataBuffer, dataLength);

	do {
		err = i2c_transfer(gl_ts->client->adapter, &msg, 1);
		if (err != 1)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		err = -EIO;
	}

	return err;
}

/*
 * Device is apparently always ready for i2c but not for actual register reads/writes.
 * This function ascertains it is ready for that too. the results of this call often
 * were ignored.
 */
static int waitDeviceReady(bool forever, bool slowly, bool probe)
{
	uint8_t ucQuery;
	uint32_t count;

	if (probe) {
		count = DEVICE_READY_MAX_PROBE_WAIT;
	} else if (update_flag) {
		count = DEVICE_READY_MAX_UPDATE_WAIT;
	} else {
		count = DEVICE_READY_MAX_WAIT;
	}

	do {
		if (i2cReadNoReadyCheck(BUF_QUERY, &ucQuery, sizeof(ucQuery)) != 2)
			ucQuery = CMD_STATUS_BUSY;

		if (slowly) {
			mdelay(1000);
		} else if (probe_flag) {
			mdelay(10);
		}

		if (update_flag && (ucQuery & CMD_STATUS_BUSY)) {
			mdelay(10);
		}

		if (!forever)
			count--;

	}while((ucQuery & CMD_STATUS_BUSY) && count && !enter_lowpower_flag);

	return !ucQuery;
}

static int i2cRead(uint8_t bufferIndex, uint8_t *dataBuffer, uint16_t dataLength)
{
	waitDeviceReady(false, false, false);
	return i2cReadNoReadyCheck(bufferIndex, dataBuffer, dataLength);
}

static int i2cWrite(uint8_t bufferIndex, const uint8_t *dataBuffer, uint16_t dataLength)
{
	waitDeviceReady(false, false, false);
	return i2cWriteNoReadyCheck(bufferIndex, dataBuffer, dataLength);
}

static bool chipFirmwareReinitialize(uint8_t cmdOfChoice)
{
	uint8_t cmd[] = {cmdOfChoice};
	uint8_t rsp[2];

	if (i2cWrite(BUF_COMMAND, cmd, sizeof(cmd)) != 1)
		return false;

	if (i2cRead(BUF_RESPONSE, rsp, sizeof(rsp)) != 2)
		return false;

	/* a reply of two zero bytes signifies success */
	return !rsp[0] && !rsp[1];
}

static bool chipFirmwareUpgradeModeEnterExit(bool enter)
{
	uint8_t cmd[] = {CMD_FIRMWARE_UPGRADE, 0, 'I', 'T', '7', '2', '6', '0', 0x55, 0xAA};
	uint8_t resp[2];

	cmd[1] = enter ? FIRMWARE_MODE_ENTER : FIRMWARE_MODE_EXIT;
	if (i2cWrite(BUF_COMMAND, cmd, sizeof(cmd)) != 1)
		return false;

	if (i2cRead(BUF_RESPONSE, resp, sizeof(resp)) != 2)
		return false;

	/* a reply of two zero bytes signifies success */
	return !resp[0] && !resp[1];
}

static bool chipSetStartOffset(uint16_t offset)
{
	uint8_t cmd[] = {CMD_SET_START_OFFSET, 0, CMD_UINT16(offset)};
	uint8_t resp[2];

	if (i2cWrite(BUF_COMMAND, cmd, 4) != 1)
		return false;


	if (i2cRead(BUF_RESPONSE, resp, sizeof(resp)) != 2)
		return false;


	/* a reply of two zero bytes signifies success */
	return !resp[0] && !resp[1];
}


//write fwLength bytes from fwData at chip offset writeStartOffset
static bool chipFlashWriteAndVerify(unsigned int fwLength, const uint8_t *fwData, uint16_t writeStartOffset)
{
	uint32_t curDataOfst;

	for (curDataOfst = 0; curDataOfst < fwLength; curDataOfst += FW_WRITE_CHUNK_SIZE) {

		uint8_t cmdWrite[2 + FW_WRITE_CHUNK_SIZE] = {CMD_FW_WRITE};
		uint8_t bufRead[FW_WRITE_CHUNK_SIZE];
		uint8_t cmdRead[2] = {CMD_FW_READ};
		unsigned i, nRetries;
		uint32_t curWriteSz;

		/* figure out how much to write */
		curWriteSz = fwLength - curDataOfst;
		if (curWriteSz > FW_WRITE_CHUNK_SIZE)
			curWriteSz = FW_WRITE_CHUNK_SIZE;

		/* prepare the write command */
		cmdWrite[1] = curWriteSz;
		for (i = 0; i < curWriteSz; i++)
			cmdWrite[i + 2] = fwData[curDataOfst + i];

		/* prepare the read command */
		cmdRead[1] = curWriteSz;

		for (nRetries = 0; nRetries < FW_WRITE_RETRY_COUNT; nRetries++) {

			/* set write offset and write the data*/
			chipSetStartOffset(writeStartOffset + curDataOfst);
			if (i2cWrite(BUF_COMMAND, cmdWrite, 2 + curWriteSz) != 1) {
				LOGE("[%d] %s i2c write fail.\n", __LINE__, __func__);
			}

			/* set offset and read the data back */
			chipSetStartOffset(writeStartOffset + curDataOfst);
			if (i2cWrite(BUF_COMMAND, cmdRead, sizeof(cmdRead)) != 1) {
				LOGE("[%d] %s i2c write fail.\n", __LINE__, __func__);
			}
			if (i2cRead(BUF_RESPONSE, bufRead, curWriteSz) != 2) {
				LOGE("[%d] %s i2c read fail.\n", __LINE__, __func__);
			}

			/* verify. If success break out of retry loop */
			for (i = 0; i < curWriteSz && bufRead[i] == cmdWrite[i + 2]; i++);
			if (i == curWriteSz)
				break;
			LOGE("write of data offset %u failed on try %u at byte %u/%u\n", curDataOfst, nRetries, i, curWriteSz);
		}
		/* if we've failed after all the retries, tell the caller */
		if (nRetries == FW_WRITE_RETRY_COUNT)
			return false;
	}

	return true;
}

static bool CheckSourceFile(uint32_t fwLen, const uint8_t *fwData, uint32_t cfgLen, const uint8_t *cfgData, uint16_t FWReg, uint16_t CFGReg)
{
	uint16_t i, j;
	uint16_t CurFW, CurCFG;
	uint16_t fwFileCRC, cfgFileCRC;
	uint8_t FWReg_check, CFGReg_check;

	fwFileCRC = fwData[14];
	for (i = 0; i < fwLen; i++) {

		if (i == 14 || i == 15) {
			CurFW = 0;
		} else {
			CurFW = ((fwData[i])<< 8);
		}

		for (j = 0; j < 8; j++) {
			if((short)(FWReg ^ CurFW) < 0) {
				FWReg = (FWReg << 1) ^ 0x1021;
			} else {
				FWReg <<= 1;
			}
			CurFW <<= 1;
		}
	}

	cfgFileCRC = cfgData[cfgLen-2];
	for (i = 0; i < (cfgLen-2); i++) {

		CurCFG = ((cfgData[i])<< 8);

		for (j = 0; j < 8; j++) {
			if((short)(CFGReg ^ CurCFG) < 0) {
				CFGReg = (CFGReg << 1) ^ 0x1021;
			} else {
				CFGReg <<= 1;
			}
			CurCFG <<= 1;
		}
	}

	FWReg_check = (uint8_t) FWReg;
	CFGReg_check = (uint8_t) CFGReg;
	LOGI("fwFileCRC::0x%x, CalculateFWCRC::0x%x, cfgFileCRC::0x%x, CalculateCFGCRC::0x%x\n", fwFileCRC, FWReg_check, cfgFileCRC, CFGReg_check);

	if((fwFileCRC == FWReg_check)&&(cfgFileCRC == CFGReg_check)) {
		LOGI("firmware/config source file are correct !!\n");
		return true;
	} else {
		return false;
	}
}

static bool VerifyChecksum(struct device *dev, uint32_t fwLen, const uint8_t *fwData, uint32_t cfgLen, const uint8_t *cfgData)
{
	uint8_t pucCommandBuffer[3], resp[2];

	pucCommandBuffer[0] = 0x1A;
	pucCommandBuffer[1] = 0x09;
	pucCommandBuffer[2] = 0x03;
	resp[0] = 0xFF;
	resp[1] = 0xFF;

	waitDeviceReady(false, true, true);

	if (i2cWriteNoReadyCheck(BUF_COMMAND, pucCommandBuffer, sizeof(pucCommandBuffer)) != 1) {
		LOGE("[%d] %s i2c write fail.\n", __LINE__, __func__);
		return false;
	}

	waitDeviceReady(false, true, true);

	if (i2cReadNoReadyCheck(BUF_RESPONSE, resp, sizeof(resp)) != 2) {
		LOGE("[%d] %s i2c read fail.\n", __LINE__, __func__);
		return false;
	}

	LOGI("CRC resp[1] = %02X, resp[0] = %02X.\n", resp[1], resp[0]);

	return !resp[1] && !resp[0];
}

static bool ChangePalmThreshold(uint8_t threshold_H, uint8_t threshold_L) {
	uint8_t pucCommandBuffer[4], resp[2];

	pucCommandBuffer[0] = 0x21;
	pucCommandBuffer[1] = 0x00;
	pucCommandBuffer[2] = threshold_L;
	pucCommandBuffer[3] = threshold_H;
	resp[0] = 0xFF;
	resp[1] = 0xFF;

	if (i2cWrite(BUF_COMMAND, pucCommandBuffer, sizeof(pucCommandBuffer)) != 1) {
		LOGE("[%d] %s i2c write fail.\n", __LINE__, __func__);
		return false;
	}

	if (i2cRead(BUF_RESPONSE, resp, sizeof(resp)) != 2) {
		LOGE("[%d] %s i2c read fail.\n", __LINE__, __func__);
		return false;
	}

	LOGI("Palm threshold = 0x%02X%02X,  resp= 0x%02X%02X.\n", threshold_H, threshold_L, resp[1], resp[0]);

	return !resp[1] && !resp[0];
}

static bool chipFirmwareUpload(struct device *dev, uint32_t fwLen, const uint8_t *fwData, uint32_t cfgLen, const uint8_t *cfgData)
{
	bool success = false;

	/* enter fw upload mode */
	if (!chipFirmwareUpgradeModeEnterExit(true)) {
		LOGE("Enter fw upload mode fail.\n");
		return false;
	}

	/* flash the firmware if requested */
	if (fw_upgrade_flag) {
		LOGI("start to flash firmware...\n");
		if (fwLen && fwData && !chipFlashWriteAndVerify(fwLen, fwData, 0)) {
			LOGE("failed to upload touch firmware\n");
			goto out;
		}
	}

	/* flash config data if requested */
	if (config_upgrade_flag) {
		LOGI("start to flash config...\n");
		if (cfgLen && cfgData && !chipFlashWriteAndVerify(cfgLen, cfgData, CHIP_FLASH_SIZE - cfgLen)) {
			LOGE("failed to upload touch cfg data\n");
			goto out;
		}
	}

	success = true;

out:
	return chipFirmwareUpgradeModeEnterExit(false) && chipFirmwareReinitialize(CMD_FIRMWARE_REINIT_6F) && success;
}

//both buffers should be VERSION_LENGTH in size, but only a part of them is significant */
static bool chipGetVersions(uint8_t *verFw, uint8_t *verCfg, bool logIt)
{
	/* this code to get versions is reproduced as was written, but it does not make sense. Something here *PROBABLY IS* wrong */
	static const uint8_t cmdReadFwVer[] = {CMD_READ_VERSIONS, VER_FIRMWARE};
	static const uint8_t cmdReadCfgVer[] = {CMD_READ_VERSIONS, VER_CONFIG};
	bool ret = true;

	disable_irq(gl_ts->client->irq);

	/* this structure is so that we definitely do all the calls, but still return a status in case anyone cares */
	if (i2cWrite(BUF_COMMAND, cmdReadFwVer, sizeof(cmdReadFwVer)) != 1) {
		LOGE("[%d] %s i2c write fail.\n", __LINE__, __func__);
		ret = false;
	} else if (i2cRead(BUF_RESPONSE, verFw, VERSION_LENGTH) != 2) {
		LOGE("[%d] %s i2c read fail.\n", __LINE__, __func__);
		ret = false;
	} else if (i2cWrite(BUF_COMMAND, cmdReadCfgVer, sizeof(cmdReadCfgVer)) != 1) {
		LOGE("[%d] %s i2c write fail.\n", __LINE__, __func__);
		ret = false;
	} else if (i2cRead(BUF_RESPONSE, verCfg, VERSION_LENGTH) != 2) {
		LOGE("[%d] %s i2c read fail.\n", __LINE__, __func__);
		ret = false;
	}

	enable_irq(gl_ts->client->irq);

	if (logIt)
		LOGI("current versions: fw@{%X,%X,%X,%X}, cfg@{%X,%X,%X,%X}\n",
			verFw[5], verFw[6], verFw[7], verFw[8],
			verCfg[1], verCfg[2], verCfg[3], verCfg[4]);

	sprintf(fwVersion, "%x,%x,%x,%x # %x,%x,%x,%x",verFw[5], verFw[6], verFw[7], verFw[8],
			 verCfg[1], verCfg[2], verCfg[3], verCfg[4]);

	return ret;
}

#ifdef CONFIG_ASUS_UTILITY
/* fix touch will not wake up system in suspend mode */
static void chipLowPowerMode(bool low)
{
	int allow_irq_wake = !(driverInLowPower);
	enter_lowpower_flag = low;
	ret = -1;

	if (devicePresent) {
		LOGI("low power %s\n", low ? "enter" : "exit");

		if (low) {
			driverInLowPower = true;
			input_mt_slot(gl_ts->touch_dev, 0);
			input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
			input_mt_slot(gl_ts->touch_dev, 1);
			input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
			input_sync(gl_ts->touch_dev);
			if (allow_irq_wake) {
				smp_wmb();
				ret = enable_irq_wake(gl_ts->client->irq);
				if (ret != 0) {
					LOGI("[%d] %s enable IRQ wake fail. ret = %d.\n", __LINE__, __func__, ret);
				}
			}
			isTouchLocked = true;
			LOGI("[%d] %s set isTouchLocked = %d.\n", __LINE__, __func__, isTouchLocked);
			wake_unlock(&touch_lock);
			queue_delayed_work(IT7260_wq, &gl_ts->touchidle_on_work, 500);
		} else {
			driverInLowPower = false;
			cancel_delayed_work(&gl_ts->touchidle_on_work);
			cancel_delayed_work(&gl_ts->exit_idle_work);
			LOGI("[%d] %s TP_DLMODE = %d, update_flag = %d.\n", __LINE__, __func__, TP_DLMODE, update_flag);
			if (first_leavelowpower && !TP_DLMODE && !update_flag) {
				//Touch Reset
				ret = gpio_request(RESET_GPIO, "CTP_RST_N");
				if (ret < 0) {
					LOGE("[%d] %s gpio_request %d error: %d\n", __LINE__, __func__, RESET_GPIO, ret);
					gpio_free(RESET_GPIO);
					msleep(50);
				} else {
					LOGI("[%d] %s touch reset\n", __LINE__, __func__);
					gpio_direction_output(RESET_GPIO,0);
					mdelay(60);
					gpio_free(RESET_GPIO);
					msleep(50);
					chipInLowPower = false;
					first_leavelowpower = false;
				}
			}
			if (!allow_irq_wake) {
				smp_wmb();
				ret = disable_irq_wake(gl_ts->client->irq);
				if (ret != 0) {
					LOGI("[%d] %s disable IRQ wake fail. ret = %d.\n", __LINE__, __func__, ret);
				}
			}
			isTouchLocked = false;
			LOGI("[%d] %s set isTouchLocked = %d. \n", __LINE__, __func__, isTouchLocked);
			hadPalmDown = false;
			last_time_exit_low = jiffies;
			wake_unlock(&touch_lock);
		}
	}
}
#endif

static ssize_t sysfsUpgradeStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	const struct firmware *fw, *cfg;
	uint8_t verFw[10], verCfg[10];
	unsigned fwLen = 0, cfgLen = 0;
	bool manualUpgrade, success;
	int mode = 0;
	ret = -1;

	if (request_firmware(&fw, "it7260.fw", dev))
		LOGE("failed to get firmware for it7260\n");
	else
		fwLen = fw->size;

	if (request_firmware(&cfg, "it7260.cfg", dev))
		LOGE("failed to get config data for it7260\n");
	else
		cfgLen = cfg->size;

	sscanf(buf, "%d", &mode);
	manualUpgrade = mode == SYSFS_FW_UPLOAD_MODE_MANUAL;
	LOGI("firmware found %ub of fw and %ub of config in %s mode\n",
		fwLen, cfgLen, manualUpgrade ? "manual" : "normal");

	chipGetVersions(verFw, verCfg, true);

	if (fwLen && cfgLen) {
		LOGI("upgrading versions: fw@{%X,%X,%X,%X}, cfg@{%X,%X,%X,%X}\n",
		fw->data[8], fw->data[9], fw->data[10], fw->data[11],
		cfg->data[cfgLen - 8], cfg->data[cfgLen - 7], cfg->data[cfgLen - 6], cfg->data[cfgLen - 5]);
	}

	/* fix touch firmware/config update failed issue */
	/* this code to check versions is reproduced as was written, but it does not quite make sense. Something here *IS* wrong */
	fwUploadResult = SYSFS_RESULT_NOT_DONE;
	fw_upgrade_flag = 0;
	config_upgrade_flag = 0;
	if (fwLen && cfgLen) {
		if (!manualUpgrade) {
			LOGI("manualUpgrade = %d, compare version.\n", manualUpgrade);
			if (fw->data[8] == FW_CUSTOMER_ID1 && fw->data[9] == FW_CUSTOMER_ID2 &&
				cfg->data[cfgLen - 8] == CFG_CUSTOMER_ID1 && cfg->data[cfgLen - 7] == CFG_CUSTOMER_ID2) {
				if (verFw[5] != FW_CUSTOMER_ID1 || verFw[6] != FW_CUSTOMER_ID2 ||
					verCfg[1] != CFG_CUSTOMER_ID1 || verCfg[2] != CFG_CUSTOMER_ID2) {
					LOGI("ic's fw/cfg is wrong, need to upgrade.\n");
					fw_upgrade_flag = 1;
					config_upgrade_flag = 1;
				} else {
					if (verFw[7] == 0xFF || verFw[8] == 0xFF || verFw[8] < fw->data[11] ||
						(verFw[8] == fw->data[11] && verFw[7] < fw->data[10])) {
						LOGI("ic's fw is old, force to upgrade.\n");
						fw_upgrade_flag = 1;
					}
					if (verCfg[3] == 0xFF || verCfg[4] == 0xFF || verCfg[3] < cfg->data[cfgLen - 6] ||
						(verCfg[3] == cfg->data[cfgLen - 6] && verCfg[4] < cfg->data[cfgLen - 5])) {
						LOGI("ic's cfg is old, force to upgrade.\n");
						config_upgrade_flag = 1;
					}
					if (verFw[7] == fw->data[10] && verFw[8] == fw->data[11] && verCfg[3] == cfg->data[cfgLen - 6] &&
						verCfg[4] == cfg->data[cfgLen - 5]) {
						LOGI("same version, check CRC.\n");
					}
				}
			} else {
				LOGI("fw/cfg file is wrong. Do not need to upgrade.\n");
			}
		} else {
			fw_upgrade_flag = 1;
			config_upgrade_flag = 1;
		}

		/*check CRC*/
		if (!fw_upgrade_flag || !config_upgrade_flag) {
			disable_irq(gl_ts->client->irq);
			if(!VerifyChecksum(dev, fwLen, fw->data, cfgLen, cfg->data)) {
				LOGI("checksum fail, force to upgrade.\n");
				fw_upgrade_flag = 1;
				config_upgrade_flag = 1;
			} else {
				LOGI("checksum pass.\n");
			}
			enable_irq(gl_ts->client->irq);
		}

		if (fw_upgrade_flag || config_upgrade_flag) {
			if(CheckSourceFile(fwLen, fw->data, cfgLen, cfg->data, 0xFFFF, 0xFFFF))	{
				LOGI("firmware/config will be upgraded\n");
				disable_irq(gl_ts->client->irq);
				success = chipFirmwareUpload(dev, fwLen, fw->data, cfgLen, cfg->data);
				LOGI("upload %s\n", success ? "success" : "failed");
				if(!success && !TP_DLMODE) {
					LOGI("enter tp download mode to force updating fw and config.\n");
					ret = gpio_request(RESET_GPIO, "CTP_RST_N");
					if (ret < 0) {
						LOGE("[%d] %s gpio_request %d error: %d\n", __LINE__, __func__, RESET_GPIO, ret);
						gpio_free(RESET_GPIO);
					} else {
						ret = gpio_request(TP_DLMODE_GPIO, "CTP_DLM_N");
						if (ret < 0) {
							LOGE("[%d] %s gpio_request %d error: %d\n", __LINE__, __func__, TP_DLMODE_GPIO, ret);
							gpio_free(TP_DLMODE_GPIO);
						} else {
							gpio_direction_output(TP_DLMODE_GPIO, 1);
							mdelay(10);
							gpio_direction_output(RESET_GPIO,0);
							mdelay(60);
							gpio_free(RESET_GPIO);
							mdelay(100);
							gpio_set_value(TP_DLMODE_GPIO, 0);
							mdelay(200);
							TP_DLMODE = true;
						}
					}
					fw_upgrade_flag = 1;
					config_upgrade_flag = 1;
					success = chipFirmwareUpload(dev, fwLen, fw->data, cfgLen, cfg->data);
					LOGI("upload %s\n", success ? "success" : "failed");
				}
				enable_irq(gl_ts->client->irq);

				fwUploadResult = success ? SYSFS_RESULT_SUCCESS : SYSFS_RESULT_FAIL;
				if (success && TP_DLMODE) {
					TP_DLMODE = false;
					LOGI("leave tp download mode.\n");
				}
			} else {
				LOGI("firmware/config source file error!!!!!!!\n");
			}
		} else {
			LOGI("firmware/config upgrade not needed\n");
		}
	}

	chipGetVersions(verFw, verCfg, true);

	if (fwLen)
		release_firmware(fw);

	if (cfgLen)
		release_firmware(cfg);

	return count;
}

static ssize_t sysfsUpgradeShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", fwUploadResult);
}

static ssize_t sysfsCalibrationShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", calibrationWasSuccessful);
}

static int chipSendCalibrationCmd(bool autoTuneOn)
{
	uint8_t cmdCalibrate[] = {CMD_CALIBRATE, 0, autoTuneOn ? 1 : 0, 0, 0};
	return i2cWrite(BUF_COMMAND, cmdCalibrate, sizeof(cmdCalibrate));
}

static ssize_t sysfsCalibrationStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	uint8_t resp;

	if (chipSendCalibrationCmd(false) != 1)
		LOGE("failed to send calibration command\n");
	else {
		calibrationWasSuccessful = (i2cRead(BUF_RESPONSE, &resp, sizeof(resp)) == 2) ? SYSFS_RESULT_SUCCESS : SYSFS_RESULT_FAIL;

		/* previous logic that was here never called chipFirmwareReinitialize() due to checking a guaranteed-not-null value against null. We now call it. Hopefully this is OK */
		if (!resp)
			LOGI("chipFirmwareReinitialize -> %s\n", chipFirmwareReinitialize(CMD_FIRMWARE_REINIT_6F) ? "success" : "fail");
	}

	return count;
}

static ssize_t sysfsPointShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t pointData[sizeof(struct PointData)];
	bool readSuccess;
	ssize_t ret;

	readSuccess = i2cReadNoReadyCheck(BUF_POINT_INFO, pointData, sizeof(pointData));
	ret = sprintf(buf, "point_show read ret[%d]--point[%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x]=\n",
		readSuccess, pointData[0],pointData[1],pointData[2],pointData[3],
		pointData[4],pointData[5],pointData[6],pointData[7],pointData[8],
		pointData[9],pointData[10],pointData[11],pointData[12],pointData[13]);


	LOGI("%s", buf);

	return ret;
}

static ssize_t sysfsPointStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/* it makes no sense to allow this, but for some reason asus chose to. Leaving it here to avoid breaking userspace tools we don't know about */
	return count;
}

static ssize_t sysfsStatusShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t verFw[10], verCfg[10];
	ssize_t ret;

	devicePresent = chipGetVersions(verFw, verCfg, true);
	LOGI("[%d] %s touch status = %d\n", __LINE__, __func__, devicePresent ? 1 : 0);
	ret = sprintf(buf, "%d\n", devicePresent ? 1 : 0);

	return ret;
}

static ssize_t sysfsStatusStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/*
	 * A per previous code that was here, any write causes the version info to be printed to the kernel log.
	 * Technically the write was passed through scanf, looking for an integer, but the value was discarded.
	 * Keeping this functionality in case something relied on it.
	 */
	return count;
}

static ssize_t sysfsChecksumShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t pucCommandBuffer[3], resp[2];
	ssize_t ret;

	disable_irq(gl_ts->client->irq);
	pucCommandBuffer[0] = 0x1A;
	pucCommandBuffer[1] = 0x09;
	pucCommandBuffer[2] = 0x03;
	resp[0] = 0xFF;
	resp[1] = 0xFF;

	waitDeviceReady(false, true, true);

	if (i2cWriteNoReadyCheck(BUF_COMMAND, pucCommandBuffer, sizeof(pucCommandBuffer)) != 1) {
		LOGE("[%d] %s i2c write fail.\n", __LINE__, __func__);
		enable_irq(gl_ts->client->irq);
		return sprintf(buf, "%s\n", "i2c write failed.");
	}

	waitDeviceReady(false, true, true);

	if (i2cReadNoReadyCheck(BUF_RESPONSE, resp, sizeof(resp)) != 2) {
		LOGE("[%d] %s i2c read fail.\n", __LINE__, __func__);
		enable_irq(gl_ts->client->irq);
		return sprintf(buf, "%s\n", "i2c read failed.");
	}

	LOGI("[%d] %s CRC resp[1] = %02X, resp[0] = %02X.\n", __LINE__, __func__, resp[1], resp[0]);
	ret = sprintf(buf, "%02X%02X\n", resp[1], resp[0]);
	enable_irq(gl_ts->client->irq);

	return ret;
}

static ssize_t sysfsChecksumStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t sysfsResetShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;

	ret = gpio_request(RESET_GPIO, "CTP_RST_N");
	if (ret < 0) {
		LOGE("[%d] %s gpio_request %d error: %d\n", __LINE__, __func__, RESET_GPIO, ret);
		gpio_free(RESET_GPIO);
		msleep(50);
	} else {
		LOGI("[%d] %s touch reset\n", __LINE__, __func__);
		gpio_direction_output(RESET_GPIO,0);
		mdelay(60);
		gpio_free(RESET_GPIO);
		msleep(50);
		chipInLowPower = false;
	}

	return ret;
}

static ssize_t sysfsResetStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t sysfsUpdateFlagShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", update_flag);
}

static ssize_t sysfsUpdateFlagStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int mode = 0;

	sscanf(buf, "%d", &mode);
	update_flag = mode == SYSFS_FW_UPDATE_SCRIPT;
	LOGI("[%d] %s update_flag = %d.\n", __LINE__, __func__, update_flag);
	return count;
}

static ssize_t sysfsPointLogShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", point_log);
}

static ssize_t sysfsPointLogStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int mode = 0;

	sscanf(buf, "%d", &mode);
	point_log = mode == SYSFS_POINT_LOG_OPEN;
	LOGI("[%d] %s point_log = %d.\n", __LINE__, __func__, point_log);
	return count;
}

static ssize_t sysfsVersionShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t verFw[10] = {0};
	uint8_t verCfg[10] = {0};
	char local_fwVersion[20] = {0};
	uint8_t cmdReadFwVer[] = {CMD_READ_VERSIONS, VER_FIRMWARE};
	uint8_t cmdReadCfgVer[] = {CMD_READ_VERSIONS, VER_CONFIG};
	ssize_t ret;

	disable_irq(gl_ts->client->irq);

	if (i2cWrite(BUF_COMMAND, cmdReadFwVer, sizeof(cmdReadFwVer)) != 1) {
		LOGE("[%d] %s i2c write fail.\n", __LINE__, __func__);
		enable_irq(gl_ts->client->irq);
		return sprintf(buf, "%s\n", "i2c failed.");
	} else if (i2cRead(BUF_RESPONSE, verFw, VERSION_LENGTH) != 2) {
		LOGE("[%d] %s i2c read fail.\n", __LINE__, __func__);
		enable_irq(gl_ts->client->irq);
		return sprintf(buf, "%s\n", "i2c failed.");
	} else if (i2cWrite(BUF_COMMAND, cmdReadCfgVer, sizeof(cmdReadCfgVer)) != 1) {
		LOGE("[%d] %s i2c write fail.\n", __LINE__, __func__);
		enable_irq(gl_ts->client->irq);
		return sprintf(buf, "%s\n", "i2c failed.");
	} else if (i2cRead(BUF_RESPONSE, verCfg, VERSION_LENGTH) != 2) {
		LOGE("[%d] %s i2c read fail.\n", __LINE__, __func__);
		enable_irq(gl_ts->client->irq);
		return sprintf(buf, "%s\n", "i2c failed.");
	}

	enable_irq(gl_ts->client->irq);

	LOGI("[%d] %s current versions: fw@{%X,%X,%X,%X}, cfg@{%X,%X,%X,%X}\n", __LINE__, __func__,
		verFw[5], verFw[6], verFw[7], verFw[8],verCfg[1], verCfg[2], verCfg[3], verCfg[4]);

	sprintf(local_fwVersion, "%x-%x-%x-%x#%x-%x-%x-%x",verFw[5], verFw[6], verFw[7], verFw[8],
			 verCfg[1], verCfg[2], verCfg[3], verCfg[4]);

	ret = sprintf(buf, "%s\n", local_fwVersion);

	return ret;
}

static ssize_t sysfsVersionStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	/* it makes no sense to allow this, but for some reason asus chose to. Leaving it here to avoid breaking userspace tools we don't know about */
	return count;
}

static ssize_t sysfsSleepShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	/*
	 * The usefulness of this was questionable at best - we were at least leaking a byte of
	 * kernel data (by claiming to return a byte but not writing to buf. To fix this now
	 * we actually return the sleep status
	 */
	if (!mutex_lock_interruptible(&sleepModeMutex)) {
		*buf = chipAwake ? '1' : '0';
		mutex_unlock(&sleepModeMutex);
		return 1;
	}
	else
		return -EINTR;
}

static ssize_t sysfsSleepStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	static const uint8_t cmdGoSleep[] = {CMD_PWR_CTL, 0x00, PWR_CTL_SLEEP_MODE};
	int goToSleepVal;
	bool goToWake;
	uint8_t dummy;

	sscanf(buf, "%d", &goToSleepVal);
	goToWake = !goToSleepVal;	/* convert to bool of proper polarity */

	if (!mutex_lock_interruptible(&sleepModeMutex)) {
		if ((chipAwake && goToWake) || (!chipAwake && !goToWake))
			LOGE("duplicate request to %s chip\n", goToWake ? "wake" : "sleep");
		else if (goToWake) {
			i2cReadNoReadyCheck(BUF_QUERY, &dummy, sizeof(dummy));
			enable_irq(gl_ts->client->irq);
			LOGI("touch is going to wake!\n");
		} else {
			disable_irq(gl_ts->client->irq);
			i2cWriteNoReadyCheck(BUF_COMMAND, cmdGoSleep, sizeof(cmdGoSleep));
			LOGI("touch is going to sleep...\n");
		}
		chipAwake = goToWake;
		mutex_unlock(&sleepModeMutex);
		return count;
	} else
		return -EINTR;
}

static DEVICE_ATTR(status, S_IRUGO|S_IWUSR, sysfsStatusShow, sysfsStatusStore);
static DEVICE_ATTR(version, S_IRUGO|S_IWUSR, sysfsVersionShow, sysfsVersionStore);
static DEVICE_ATTR(sleep, S_IRUGO|S_IWUSR, sysfsSleepShow, sysfsSleepStore);
static DEVICE_ATTR(reset, S_IRUGO|S_IWUSR, sysfsResetShow, sysfsResetStore);
static DEVICE_ATTR(checksum, S_IRUGO|S_IWUSR, sysfsChecksumShow, sysfsChecksumStore);
static DEVICE_ATTR(pointlog, S_IRUGO|S_IWUSR, sysfsPointLogShow, sysfsPointLogStore);

static struct attribute *it7260_attrstatus[] = {
	&dev_attr_status.attr,
	&dev_attr_version.attr,
	&dev_attr_sleep.attr,
	&dev_attr_reset.attr,
	&dev_attr_checksum.attr,
	&dev_attr_pointlog.attr,
	NULL
};

static const struct attribute_group it7260_attrstatus_group = {
	.attrs = it7260_attrstatus,
};

static DEVICE_ATTR(calibration, S_IRUGO|S_IWUSR, sysfsCalibrationShow, sysfsCalibrationStore);
static DEVICE_ATTR(upgrade, S_IRUGO|S_IWUSR, sysfsUpgradeShow, sysfsUpgradeStore);
static DEVICE_ATTR(point, S_IRUGO|S_IWUSR, sysfsPointShow, sysfsPointStore);
static DEVICE_ATTR(updateflag, S_IRUGO|S_IWUSR, sysfsUpdateFlagShow, sysfsUpdateFlagStore);

static struct attribute *it7260_attributes[] = {
	&dev_attr_calibration.attr,
	&dev_attr_upgrade.attr,
	&dev_attr_updateflag.attr,
	&dev_attr_point.attr,
	NULL
};

static const struct attribute_group it7260_attr_group = {
	.attrs = it7260_attributes,
};

static int palm_enable_set(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
	gl_ts->palm_en = enable;

	return 0;
}

static struct sensors_classdev palm_cdev = {
	.name = "palm",
	.vendor = "ITE",
	.version = 1,
	.enabled = 0,
	.sensors_enable = NULL,
};

static void chipExternalCalibration(bool autoTuneEnabled)
{
	uint8_t resp[2];

	LOGI("sent calibration command -> %d\n", chipSendCalibrationCmd(autoTuneEnabled));
	waitDeviceReady(true, true, false);
	if (i2cReadNoReadyCheck(BUF_RESPONSE, resp, sizeof(resp)) != 2) {
		LOGE("[%d] %s i2c read fail.\n", __LINE__, __func__);
	}
	chipFirmwareReinitialize(CMD_FIRMWARE_REINIT_C);
}

void sendCalibrationCmd(void)
{
	chipExternalCalibration(false);
}
EXPORT_SYMBOL(sendCalibrationCmd);

void enableAutoTune(void)
{
	chipExternalCalibration(true);
}
EXPORT_SYMBOL(enableAutoTune);

#ifdef CONFIG_ASUS_UTILITY
static int mode_notify_sys(struct notifier_block *notif, unsigned long code, void *data)
{
	LOGI("[PF]%s code = %ld +\n", __func__, code);
	switch (code) {
	case 0: //FB_BLANK_ENTER_NON_INTERACTIVE
		chipLowPowerMode(1);
		break;
	case 1: //FB_BLANK_ENTER_INTERACTIVE
		chipLowPowerMode(0);
		break;
	default:
		LOGI("[PF]%s default +\n", __func__);
		break;
	}
	LOGI("[PF]%s code = %ld -\n", __func__, code);
	return 0;
}

static struct notifier_block display_mode_notifier = {
	.notifier_call = mode_notify_sys,
};
#endif


static void readFingerData(uint16_t *xP1, uint16_t *yP1, uint8_t *pressureP1, uint16_t *xP2, uint16_t *yP2, uint8_t *pressureP2, const struct FingerData *fd)
{
	uint16_t x1 = fd[0].xLo;
	uint16_t y1 = fd[0].yLo;
	uint16_t x2 = fd[1].xLo;
	uint16_t y2 = fd[1].yLo;

	x1 += ((uint16_t)(fd[0].hi & 0x0F)) << 8;
	y1 += ((uint16_t)(fd[0].hi & 0xF0)) << 4;

	x2 += ((uint16_t)(fd[1].hi & 0x0F)) << 8;
	y2 += ((uint16_t)(fd[1].hi & 0xF0)) << 4;

	if (xP1)
		*xP1 = x1;
	if (yP1)
		*yP1 = y1;
	if (pressureP1)
		*pressureP1 = fd[0].pressure & FD_PRESSURE_BITS;

	if (xP2)
		*xP2 = x2;
	if (yP2)
		*yP2 = y2;
	if (pressureP2)
		*pressureP2 = fd[1].pressure & FD_PRESSURE_BITS;
}

static uint64_t getMsTime(void)
{
	struct timespec ts;
	uint64_t ret;

	getnstimeofday(&ts);

	/* convert nsec to msec */
	ret = ts.tv_nsec;
	do_div(ret, 1000000UL);

	/* add in sec */
	return ret + ts.tv_sec * 1000ULL;
}

static void waitNotifyEvt(struct work_struct *work) {
	if (!driverInLowPower) {
		isTouchLocked = false;
		LOGI("[%d] %s set isTouchLocked = %d. \n", __LINE__, __func__, isTouchLocked);
	}
}

static void touchIdleOnEvt(struct work_struct *work) {
	static const uint8_t cmdLowPower[] = { CMD_PWR_CTL, 0x00, PWR_CTL_LOW_POWER_MODE};
	LOGI("[%d] %s chipInLowPower = %d, TP_DLMODE = %d. \n", __LINE__, __func__, chipInLowPower, TP_DLMODE);
	if (!chipInLowPower && !TP_DLMODE) {
		if (!ChangePalmThreshold(0x70,0x00)) {
			LOGI("[%d] %s change palm threshold fail.\n", __LINE__, __func__);
		}
		if (i2cWriteNoReadyCheck(BUF_COMMAND, cmdLowPower, sizeof(cmdLowPower)) != 1) {
			LOGE("[%d] %s i2c write fail.\n", __LINE__, __func__);
			chipInLowPower = false;
		} else {
			chipInLowPower = true;
			LOGI("[%d] %s set chipInLowPower = %d. \n", __LINE__, __func__, chipInLowPower);
		}
	}
}

static void exitIdleEvt(struct work_struct *work) {
	if (driverInLowPower) {
		AMB_TOUCH_HAD_SEND_FLAG = 1;
		exit_idle_event_time = getMsTime();
		if (exit_idle_event_time - last_time_send_palm > 800) {
			isTouchLocked = true;
			LOGI("[%d] %s Touch DOWN x = 1, y = 1.\n", __LINE__, __func__);
			input_mt_slot(gl_ts->touch_dev,0);
			input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, true);
			input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_X, 1);
			input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_Y, 1);
			input_sync(gl_ts->touch_dev);
			input_mt_slot(gl_ts->touch_dev,0);
			input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
			input_sync(gl_ts->touch_dev);
			wake_lock_timeout(&touch_time_lock, WAKELOCK_HOLD_MS);
			wake_unlock(&touch_lock);
			last_time_exit_low = jiffies;
			if (!ChangePalmThreshold(0xA0,0x00)) {
				LOGI("[%d] %s change palm threshold fail.\n", __LINE__, __func__);
			}
		} else {
				LOGI("[%d] %s Wait palm to ambient, PALM_DELTA_TIME = %llu.\n", __LINE__, __func__, exit_idle_event_time - last_time_send_palm);
		}
		AMB_TOUCH_HAD_SEND_FLAG = 0;
	}
}

static void sendPalmEvt(void)
{
	last_time_send_palm = getMsTime();
	LOGI("[%d] %s PALM event, time = %llu.\n", __LINE__, __func__, last_time_send_palm);
	input_report_key(gl_ts->touch_dev, KEY_SLEEP, 1);
	input_sync(gl_ts->touch_dev);
	if (gl_ts->palm_en) {
		input_report_abs(gl_ts->palm_dev, ABS_DISTANCE, 1);
		input_sync(gl_ts->palm_dev);
	}
	input_report_key(gl_ts->touch_dev, KEY_SLEEP, 0);
	input_sync(gl_ts->touch_dev);
	if (gl_ts->palm_en) {
		input_report_abs(gl_ts->palm_dev, ABS_DISTANCE, 0);
		input_sync(gl_ts->palm_dev);
	}
	input_mt_slot(gl_ts->touch_dev, 0);
	input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
	input_mt_slot(gl_ts->touch_dev, 1);
	input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
	input_sync(gl_ts->touch_dev);
}

/* contrary to the name this code does not just read data - lots of processing happens */
static void readTouchDataPoint(void)
{
	struct PointData pointData;
	uint8_t devStatus;
	uint8_t pressure1 = FD_PRESSURE_NONE;
	uint8_t pressure2 = FD_PRESSURE_NONE;
	uint16_t x1, y1, x2, y2;

	if (chipInLowPower) {
		chipInLowPower = false;
	}

	/* verify there is point data to read & it is readable and valid */
	if (i2cReadNoReadyCheck(BUF_QUERY, &devStatus, sizeof(devStatus)) == 2) {
		if (!((devStatus & PT_INFO_BITS) & PT_INFO_YES)) {
			if (!TP_DLMODE) {
				LOGE(" %s called when no data available (0x%02X)\n", __func__, devStatus);
			}
			return;
		}
	}
	if (i2cReadNoReadyCheck(BUF_POINT_INFO, (void*)&pointData, sizeof(pointData)) != 2) {
		LOGE(" %s failed to read point data buffer\n", __func__);
		return;
	}
	if ((pointData.flags & PD_FLAGS_DATA_TYPE_BITS) != PD_FLAGS_DATA_TYPE_TOUCH) {
		LOGE(" %s dropping non-point data of type 0x%02X\n", __func__, pointData.flags);
		if (pointData.flags == 0x10) {
			if (!ChangePalmThreshold(0xA0,0x00)) {
				LOGI("[%d] %s change palm threshold fail.\n", __LINE__, __func__);
			}
		}
		return;
	}

	if ((pointData.palm & PD_PALM_FLAG_BIT) && !isTouchLocked && !hadPalmDown) {
		if (jiffies - last_time_exit_low > HZ/4) {
			isTouchLocked = true;
			hadPalmDown = true;
			sendPalmEvt();
			queue_delayed_work(IT7260_wq, &gl_ts->afterpalm_work, 30);
		}
	}

	/* this check may look stupid, but it is here for when MT arrives to this driver. for now just check finger 0 */
	if ((pointData.flags & PD_FLAGS_HAVE_FINGERS) & 0x03)
		readFingerData(&x1, &y1, &pressure1, &x2, &y2, &pressure2, pointData.fd);

	if (pressure1 >= FD_PRESSURE_LIGHT || pressure2 >= FD_PRESSURE_LIGHT) {

		if (!hadFingerDown)
			hadFingerDown = true;

		if (isTouchLocked)
			LOGI("Touch is locked. \n");

		if (!isTouchLocked && pointData.flags & 0x01) {
			input_mt_slot(gl_ts->touch_dev,0);
			input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, true);
			input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_X, x1);
			input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_Y, y1);
			if (point_log) {
				LOGI("TOUCH P1 DOWN, x = %d, y = %d.\n", x1, y1);
			} else {
				if (TOUCH_P1_DOWN_FLAG == 0) {
					LOGI("TOUCH P1 DOWN, x = %d, y = %d.\n", x1, y1);
					TOUCH_P1_DOWN_FLAG = 1;
				}
			}
		} else {
			input_mt_slot(gl_ts->touch_dev, 0);
			input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
			if (!point_log && TOUCH_P1_DOWN_FLAG == 1) {
				LOGI("TOUCH P1 UP.\n");
				TOUCH_P1_DOWN_FLAG = 0;
			}
		}

		if (!isTouchLocked && pointData.flags & 0x02) {
			input_mt_slot(gl_ts->touch_dev,1);
			input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, true);
			input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_X, x2);
			input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_Y, y2);
			if (point_log) {
				LOGI("TOUCH P2 DOWN, x = %d, y = %d.\n", x2, y2);
			} else {
				if (TOUCH_P2_DOWN_FLAG == 0) {
					LOGI("TOUCH P2 DOWN, x = %d, y = %d.\n", x2, y2);
					TOUCH_P2_DOWN_FLAG = 1;
				}
			}
		} else {
			input_mt_slot(gl_ts->touch_dev, 1);
			input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
			if (!point_log && TOUCH_P2_DOWN_FLAG == 1) {
				LOGI("TOUCH P2 UP.\n");
				TOUCH_P2_DOWN_FLAG = 0;
			}
		}

		input_sync(gl_ts->touch_dev);

	} else if (!(pointData.palm & PD_PALM_FLAG_BIT)) {
		hadFingerDown = false;
		hadPalmDown = false;

		input_mt_slot(gl_ts->touch_dev, 0);
		input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
		input_mt_slot(gl_ts->touch_dev, 1);
		input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
		input_sync(gl_ts->touch_dev);

		if (!point_log && TOUCH_P1_DOWN_FLAG == 1) {
			LOGI("TOUCH P1 UP.\n");
			TOUCH_P1_DOWN_FLAG = 0;
		}
		if (!point_log && TOUCH_P2_DOWN_FLAG == 1) {
			LOGI("TOUCH P2 UP.\n");
			TOUCH_P2_DOWN_FLAG = 0;
		}
	}
}

static void readTouchDataPoint_Ambient(void)
{
	struct PointData pointData;
	uint8_t devStatus;
	uint8_t pressure1 = FD_PRESSURE_NONE;
	uint16_t x1, y1;
	uint8_t pressure2 = FD_PRESSURE_NONE;
	uint16_t x2, y2;

	if (chipInLowPower) {
		chipInLowPower = false;
	}

	if (isTouchLocked && !TP_DLMODE)
		LOGI("[%d] %s isTouchLocked = %d, isDriverAvailable = %d. \n",
			__LINE__, __func__, isTouchLocked, isDriverAvailable);

	if (!isTouchLocked) {
		if (i2cReadNoReadyCheck(BUF_QUERY, &devStatus, sizeof(devStatus)) == 2) {
			if (!((devStatus & PT_INFO_BITS) & PT_INFO_YES)) {
				isTouchLocked = true;
				if (!TP_DLMODE) {
					LOGE("[%d] %s called when no data available (0x%02X), set isTouchLocked = %d.\n",
						__LINE__, __func__, devStatus, isTouchLocked);
				}
				wake_unlock(&touch_lock);
				return;
			}
		}
		if (i2cReadNoReadyCheck(BUF_POINT_INFO, (void*)&pointData, sizeof(pointData)) != 2) {
			isTouchLocked = true;
			LOGE("[%d] %s failed to read point data buffer, set isTouchLocked = %d.\n",
				__LINE__, __func__, isTouchLocked);
			wake_unlock(&touch_lock);
			return;
		}
		if ((pointData.flags & PD_FLAGS_DATA_TYPE_BITS) != PD_FLAGS_DATA_TYPE_TOUCH) {
			if (pointData.flags == 16) {
				LOGE("[%d] %s send touch event by type 0x%02X\n", __LINE__, __func__, pointData.flags);
			} else {
				LOGE("[%d] %s dropping non-point data of type 0x%02X\n", __LINE__, __func__, pointData.flags);
				isTouchLocked = true;
				wake_unlock(&touch_lock);
				return;
			}
		}

		if ((pointData.flags & PD_FLAGS_HAVE_FINGERS) & 0x03) {
			readFingerData(&x1, &y1, &pressure1, &x2, &y2, &pressure2, pointData.fd);
			AMB_x1 = x1;
			AMB_y1 = y1;
		}

		if ((pointData.palm & PD_PALM_FLAG_BIT)) {
			if (hadFingerDown) {
				cancel_delayed_work(&gl_ts->exit_idle_work);
			}
			hadFingerDown = false;
		}

		if (pressure1 >= FD_PRESSURE_LIGHT || pressure2 >= FD_PRESSURE_LIGHT) {
			if (hadFingerDown) {
				cancel_delayed_work(&gl_ts->exit_idle_work);
			} else {
				hadFingerDown = true;
			}
			readFingerData(&x1, &y1, &pressure1, &x2, &y2, &pressure2, pointData.fd);
			AMB_x1 = x1;
			AMB_y1 = y1;
		} else if (hadFingerDown && (!(pointData.palm & PD_PALM_FLAG_BIT))) {
			hadFingerDown = false;
			suspend_touch_up = getMsTime();

			cancel_delayed_work(&gl_ts->exit_idle_work);
			if (lastTouch == TOUCH_UP) {
				touchMissed = true;
			} else {
				lastTouch = TOUCH_UP;
				touchMissed = false;
			}
			if ((touchMissed || (suspend_touch_up - suspend_touch_down < 140)) && (suspend_touch_up - last_time_send_palm > 800)) {
				if (touchMissed)
					LOGI("[%d] %s touch down missed.\n", __LINE__, __func__);

				if (!AMB_TOUCH_HAD_SEND_FLAG) {
					if (AMB_x1 <= 0 || AMB_y1 <= 0) {
						AMB_x1 = 1;
						AMB_y1 = 1;
					}
					LOGI("[%d] %s Touch DOWN x = %d, y = %d.\n", __LINE__, __func__, AMB_x1, AMB_y1);
					input_mt_slot(gl_ts->touch_dev,0);
					input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, true);
					input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_X, AMB_x1);
					input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_Y, AMB_y1);
					input_sync(gl_ts->touch_dev);
					input_mt_slot(gl_ts->touch_dev,0);
					input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
					input_sync(gl_ts->touch_dev);
					wake_lock_timeout(&touch_time_lock, WAKELOCK_HOLD_MS);
					last_time_exit_low = jiffies;
					if (!ChangePalmThreshold(0xA0,0x00)) {
						LOGI("[%d] %s change palm threshold fail.\n", __LINE__, __func__);
					}
				} else {
					AMB_TOUCH_HAD_SEND_FLAG = 0;
				}
			} else if (suspend_touch_up - last_time_send_palm <= 800) {
				LOGI("[%d] %s Wait palm to ambient, PALM_DELTA_TIME = %llu.\n", __LINE__, __func__, suspend_touch_up - last_time_send_palm);
			} else if (suspend_touch_up - suspend_touch_down >= 140) {
				LOGI("[%d] %s Move action do not wake up, DOWN_DELTA_TIME = %llu.\n", __LINE__, __func__, suspend_touch_up - suspend_touch_down);
			}
			if (driverInLowPower) {
				isTouchLocked = true;
				LOGI("[%d] %s set isTouchLocked = %d. \n", __LINE__, __func__, isTouchLocked);
			}
			wake_unlock(&touch_lock);
		} else if (pointData.flags == 16) {
			hadFingerDown = true;
			AMB_x1 = 0;
			AMB_y1 = 0;
			queue_delayed_work(IT7260_wq, &gl_ts->exit_idle_work, 5);
		} else if (pressure1 == 0 && pressure2 == 0 && (!(pointData.palm & PD_PALM_FLAG_BIT))){
			if (driverInLowPower) {
				isTouchLocked = true;
				LOGI("[%d] %s set isTouchLocked = %d. \n", __LINE__, __func__, isTouchLocked);
			}
			wake_unlock(&touch_lock);
		}

	} else if (isTouchLocked) {
		if (!isDriverAvailable) {
			msleep(10);
			if (!TP_DLMODE) {
				LOGI("[%d] %s after sleep.\n", __LINE__, __func__);
			}
		}
		if (isDriverAvailable) {
			wake_lock(&touch_lock);
			isTouchLocked = false;
			if (!TP_DLMODE) {
				LOGI("[%d] %s set isTouchLocked = %d. \n", __LINE__, __func__, isTouchLocked);
			}
			suspend_touch_down = getMsTime();
			lastTouch = TOUCH_DOWN;
			touchMissed = false;
			readTouchDataPoint_Ambient();
		}
	}
}

static irqreturn_t IT7260_ts_threaded_handler(int irq, void *devid)
{
	smp_rmb();
	if (driverInLowPower) {
		readTouchDataPoint_Ambient();
		smp_wmb();
		/* XXX: call readTouchDataPoint() here maybe ? */
	} else {
		readTouchDataPoint();
	}

	return IRQ_HANDLED;
}

static bool chipIdentifyIT7260(bool probe)
{
	static const uint8_t cmdIdent[] = {CMD_IDENT_CHIP};
	static const uint8_t expectedID[] = {0x0A, 'I', 'T', 'E', '7', '2', '6', '0'};
	uint8_t chipID[10] = {0,};

	disable_irq(gl_ts->client->irq);

	LOGI("start to chipIdentifyIT7260...\n");

	waitDeviceReady(false, false, probe);

	if (i2cWriteNoReadyCheck(BUF_COMMAND, cmdIdent, sizeof(cmdIdent)) != 1) {
		LOGE("[%d] %s i2c write fail.\n", __LINE__, __func__);
		return false;
	}

	waitDeviceReady(false, false, probe);

	if (i2cReadNoReadyCheck(BUF_RESPONSE, chipID, sizeof(chipID)) != 2) {
		LOGE("[%d] %s i2c read fail.\n", __LINE__, __func__);
		return false;
	}

	enable_irq(gl_ts->client->irq);

	LOGI("chipIdentifyIT7260 read id: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
		chipID[0], chipID[1], chipID[2], chipID[3], chipID[4],
		chipID[5], chipID[6], chipID[7], chipID[8], chipID[9]);

	/* here asus never verified the chip ID data was correct. I chose to do it just to be safe */
	if (memcmp(chipID, expectedID, sizeof(expectedID)))
		return false;

	if (chipID[8] == '5' && chipID[9] == '6')
		LOGI("rev BX3 found\n");
	else if (chipID[8] == '6' && chipID[9] == '6')
		LOGI("rev BX4 found\n");
	else	/* unknown, but let's hope it is still a version we can talk to */
		LOGI("rev (0x%02X 0x%02X) found\n", chipID[8], chipID[9]);

	LOGI("end to chipIdentifyIT7260...\n");

	return true;
}

int parse_reset_gpio(struct device *dev)
{
	u32 value;
	struct device_node *node = dev->of_node;

	if (of_property_read_u32(node, "reset", &value)) {
		LOGE("Missing Touch Reset GPIO Value\n");
		return -EINVAL;
	}

	return value;
}

static int IT7260_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct IT7260_i2c_platform_data *pdata;
	int ret = -1;

	LOGI("start to probe...\n");
	probe_flag = true;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		LOGE("need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_out;
	}

	if (!client->irq) {
		LOGE("need IRQ\n");
		ret = -ENODEV;
		goto err_out;
	}

	gl_ts = kzalloc(sizeof(*gl_ts), GFP_KERNEL);
	if (!gl_ts) {
		ret = -ENOMEM;
		goto err_out;
	}

	gl_ts->client = client;

	i2c_set_clientdata(client, gl_ts);
	pdata = client->dev.platform_data;

	if (sysfs_create_group(&(client->dev.kobj), &it7260_attrstatus_group)) {
		LOGE("failed to register sysfs #1\n");
		goto err_sysfs_grp_create_1;
	}

	RESET_GPIO = parse_reset_gpio(&client->dev);
	LOGI("parse reset gpio RESET_GPIO = %d.\n", RESET_GPIO);

	if (!chipIdentifyIT7260(true)) {
		LOGE("chipIdentifyIT7260 FAIL\n");
		ret = gpio_request(RESET_GPIO, "CTP_RST_N");
		if (ret < 0) {
			LOGE("[%d] %s gpio_request %d error: %d\n", __LINE__, __func__, RESET_GPIO, ret);
			gpio_free(RESET_GPIO);
		} else {
			ret = gpio_request(TP_DLMODE_GPIO, "CTP_DLM_N");
			if (ret < 0) {
				LOGE("[%d] %s gpio_request %d error: %d\n", __LINE__, __func__, TP_DLMODE_GPIO, ret);
				gpio_free(TP_DLMODE_GPIO);
			} else {
				gpio_direction_output(TP_DLMODE_GPIO, 1);
				mdelay(10);
				gpio_direction_output(RESET_GPIO,0);
				mdelay(60);
				gpio_free(RESET_GPIO);
				mdelay(100);
				gpio_set_value(TP_DLMODE_GPIO, 0);
				mdelay(200);
				TP_DLMODE = true;
				LOGI("chipIdentifyIT7260 FAIL in tp download mode.\n");
			}
		}
	}

	gl_ts->touch_dev = input_allocate_device();
	if (!gl_ts->touch_dev) {
		LOGE("failed to allocate input device\n");
		ret = -ENOMEM;
		goto err_ident_fail_or_input_alloc;
	}

	gl_ts->touch_dev->name = DEVICE_NAME;
	gl_ts->touch_dev->phys = "I2C";
	gl_ts->touch_dev->id.bustype = BUS_I2C;
	gl_ts->touch_dev->id.vendor = 0x0001;
	gl_ts->touch_dev->id.product = 0x7260;
	set_bit(EV_SYN, gl_ts->touch_dev->evbit);
	set_bit(EV_KEY, gl_ts->touch_dev->evbit);
	set_bit(EV_ABS, gl_ts->touch_dev->evbit);
	set_bit(INPUT_PROP_DIRECT,gl_ts->touch_dev->propbit);
	set_bit(KEY_SLEEP,gl_ts->touch_dev->keybit);
	input_mt_init_slots(gl_ts->touch_dev, 2, 0);
	input_set_abs_params(gl_ts->touch_dev, ABS_X, 0, SCREEN_X_RESOLUTION, 0, 0);
	input_set_abs_params(gl_ts->touch_dev, ABS_Y, 0, SCREEN_Y_RESOLUTION, 0, 0);
	input_set_abs_params(gl_ts->touch_dev, ABS_MT_POSITION_X, 0, SCREEN_X_RESOLUTION, 0, 0);
	input_set_abs_params(gl_ts->touch_dev, ABS_MT_POSITION_Y, 0, SCREEN_Y_RESOLUTION, 0, 0);

	if (input_register_device(gl_ts->touch_dev)) {
		LOGE("failed to register input device\n");
		goto err_input_register;
	}

	gl_ts->palm_dev = input_allocate_device();
	if (!gl_ts->palm_dev) {
		LOGE("failed to allocate palm device\n");
		ret = -ENOMEM;
		goto err_ident_fail_or_input_alloc;
	}

	gl_ts->palm_dev->name = "ASUS PALM";
	gl_ts->palm_dev->id.bustype = BUS_I2C;
	input_set_capability(gl_ts->palm_dev, EV_ABS, ABS_DISTANCE);
	__set_bit(EV_SYN, gl_ts->palm_dev->evbit);
	__set_bit(ABS_DISTANCE, gl_ts->palm_dev->absbit);
	input_set_abs_params(gl_ts->palm_dev, ABS_DISTANCE, 0, 1, 0, 0);

	if (input_register_device(gl_ts->palm_dev)) {
		LOGE("failed to register palm device\n");
		goto err_input_register;
	}

	gl_ts->palm_en = 0;
	gl_ts->cdev = palm_cdev;
	gl_ts->cdev.enabled = 0;
	gl_ts->cdev.sensors_enable = palm_enable_set;

	if (sensors_classdev_register(&client->dev, &gl_ts->cdev)) {
		LOGE(" %s: class device create failed\n", __func__);
		goto err_out;
	}

	touchMissed = false;

	IT7260_wq = create_workqueue("IT7260_wq");
	if (!IT7260_wq)
		goto err_check_functionality_failed;

	INIT_DELAYED_WORK(&gl_ts->afterpalm_work, waitNotifyEvt);
	INIT_DELAYED_WORK(&gl_ts->touchidle_on_work, touchIdleOnEvt);
	INIT_DELAYED_WORK(&gl_ts->exit_idle_work, exitIdleEvt);

	if (request_threaded_irq(client->irq, NULL, IT7260_ts_threaded_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, client->name, gl_ts)) {
		LOGE("request_irq failed\n");
		goto err_irq_reg;
	}

	#ifdef CONFIG_ASUS_UTILITY
	register_mode_notifier(&display_mode_notifier);
	#endif

	if (sysfs_create_group(&(client->dev.kobj), &it7260_attr_group)) {
		LOGE("failed to register sysfs #2\n");
		goto err_sysfs_grp_create_2;
	}
	wake_lock_init(&touch_lock, WAKE_LOCK_SUSPEND, "touch-lock");
	wake_lock_init(&touch_time_lock, WAKE_LOCK_SUSPEND, "touch-time-lock");

	if (!TP_DLMODE) {
		TP_DLMODE_GPIO_VALUE = gpio_get_value(TP_DLMODE_GPIO);
		if (TP_DLMODE_GPIO_VALUE == 1) {
			TP_DLMODE = true;
			LOGI("in tp download mode.\n");
		}
	}

	devicePresent = true;
	LOGI("[%d] %s set devicePresent = %d.\n", __LINE__, __func__, devicePresent);

	probe_flag = false;

	return 0;

err_sysfs_grp_create_2:
	#ifdef CONFIG_ASUS_UTILITY
	unregister_mode_notifier(&display_mode_notifier);
	#endif
	free_irq(client->irq, gl_ts);

err_irq_reg:
	input_unregister_device(gl_ts->touch_dev);
	gl_ts->touch_dev = NULL;
	input_unregister_device(gl_ts->palm_dev);
	gl_ts->palm_dev = NULL;

err_input_register:
	if (gl_ts->touch_dev)
		input_free_device(gl_ts->touch_dev);
	if (gl_ts->palm_dev)
		input_free_device(gl_ts->palm_dev);

err_ident_fail_or_input_alloc:
	sysfs_remove_group(&(client->dev.kobj), &it7260_attrstatus_group);

err_sysfs_grp_create_1:
	kfree(gl_ts);

err_check_functionality_failed:
	if (IT7260_wq)
		destroy_workqueue(IT7260_wq);

err_out:
	probe_flag = false;
	return ret;
}

static int IT7260_ts_remove(struct i2c_client *client)
{
	destroy_workqueue(IT7260_wq);
	devicePresent = false;
	return 0;
}

/*
 * ioctl on this file allows one to directly send commands to the chip and read replies from it.
 * This is bad for a whole variety of reasons, like not knowing the chip state at any given
 * point in time. The data is sent to the kernel in a rather peculiar format too - it is unpacked
 * into an uint16_t array where each element is a byte to send (and thus has the top 8 bits set
 * to 0). I have no idea why asus chose to do it this way, but preserving in case they have tools
 * that need this. To make matters weirder, there are two commands that can directly control the
 * state of the IRQ. Stranger yet, they are *ALSO* sent to the chip, which will surely be confused
 * as they are in the *RESERVED* buffer address space. Also preserving this in case asus guys know
 * something I do not about that.
 */
static long ite7260_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	static const uint8_t fakeCmdIrqOff[] = {0x00, 0x49, 0x54, 0x37, 0x32};
	static const uint8_t fakeCmdIrqOn[] = {0x80, 0x49, 0x54, 0x37, 0x32};
	uint8_t buffer[MAX_BUFFER_SIZE] = {0,};
	struct ioctl_cmd168 data = {0, };
	unsigned i;

	switch (cmd) {
	case IOCTL_SET:
		LOGE("direct command TX from userspace is undoubtedly bad!\n");
		if (copy_from_user(&data, (void __user *)arg, sizeof(struct ioctl_cmd168)))
			return -EFAULT;
		if (data.length >= sizeof(buffer))
			return -EINVAL; /* stop trying to overflow kernel stack! */

		for (i = 0; i < data.length; i++)
			buffer[i] = data.buffer[i];

		if (data.bufferIndex == 0x60) {
			if (!memcmp(buffer, fakeCmdIrqOff, sizeof(fakeCmdIrqOff))) {
				LOGI("Disabling IRQ.\n");
				disable_irq(gl_ts->client->irq);
			} else if (!memcmp(buffer, fakeCmdIrqOn, sizeof(fakeCmdIrqOff))) {
				LOGI("Enabling IRQ.\n");
				enable_irq(gl_ts->client->irq);
			}
			LOGE("reserved command being sent to chip, this is probably bad!\n");
		}
		if (i2cWriteNoReadyCheck(data.bufferIndex, buffer, data.length) != 1) {
			LOGE("[%d] %s i2c write fail.\n", __LINE__, __func__);
		}
		return 0;

	case IOCTL_GET:
		LOGE("direct command RX from userspace is undoubtedly bad!\n");
		if (copy_from_user(&data, (void __user *)arg, sizeof(struct ioctl_cmd168)))
			return -EFAULT;
		if (data.length >= sizeof(buffer))
			return -EINVAL; /* stop trying to overflow kernel stack! */

		if (!data.length)
			data.buffer[0] = 128;
		else {
			if (i2cReadNoReadyCheck(data.bufferIndex, buffer, data.length) != 2) {
				LOGE("[%d] %s i2c read fail.\n", __LINE__, __func__);
			}
			for (i = 0; i < data.length; i++)
				data.buffer[i] = buffer[i];
		}

		if (copy_to_user((int __user *)arg, &data, sizeof(struct ioctl_cmd168)))
			return -EFAULT;
		return 0;

	default:
		return -ENOTTY;
	}
}

static int ite7260_open(struct inode *inode, struct file *filp)
{
	struct ite7260_perfile_data *dev;
	int i;


	dev = kmalloc(sizeof(struct ite7260_perfile_data), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	rwlock_init(&dev->lock);
	for (i = 0; i < MAX_BUFFER_SIZE; i++)
		dev->buffer[i] = 0xFF;

	filp->private_data = dev;

	return 0;
}

static int ite7260_close(struct inode *inode, struct file *filp)
{
	struct ite7260_perfile_data *dev = filp->private_data;

	if (dev)
		kfree(dev);

	return 0;
}

static const struct file_operations ite7260_fops = {
	.owner = THIS_MODULE,
	.open = ite7260_open,
	.release = ite7260_close,
	.unlocked_ioctl = ite7260_ioctl,
};

static const struct i2c_device_id IT7260_ts_id[] = {
	{ DEVICE_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, IT7260_ts_id);

static const struct of_device_id IT7260_match_table[] = {
	{ .compatible = "ITE,IT7260_ts",},
	{},
};
MODULE_DEVICE_TABLE(of, IT7260_match_table);

static int IT7260_ts_resume(struct i2c_client *i2cdev)
{
	LOGI("Enter ts resume!\n");
	isDriverAvailable = true;
	return 0;
}

static int IT7260_ts_suspend(struct i2c_client *i2cdev, pm_message_t pmesg)
{
	static const uint8_t cmdLowPower[] = { CMD_PWR_CTL, 0x00, PWR_CTL_LOW_POWER_MODE};
	LOGI("Enter ts suspend!\n");
	ret = -1;
	isTouchLocked = true;
	driverInLowPower = true;
	isDriverAvailable = false;

	cancel_delayed_work(&gl_ts->touchidle_on_work);

	ret = enable_irq_wake(gl_ts->client->irq);
	if (ret != 0) {
		LOGI("[%d] %s enable IRQ wake fail. ret = %d.\n", __LINE__, __func__, ret);
	}

	LOGI("[%d] %s chipInLowPower = %d, TP_DLMODE = %d. \n", __LINE__, __func__, chipInLowPower, TP_DLMODE);
	if (!chipInLowPower && !TP_DLMODE) {
		if (!ChangePalmThreshold(0x70,0x00)) {
			LOGI("[%d] %s change palm threshold fail.\n", __LINE__, __func__);
		}
		if (i2cWriteNoReadyCheck(BUF_COMMAND, cmdLowPower, sizeof(cmdLowPower)) != 1) {
			LOGE("[%d] %s i2c write fail.\n", __LINE__, __func__);
			chipInLowPower = false;
		} else {
			chipInLowPower = true;
			LOGI("[%d] %s set chipInLowPower = %d. \n", __LINE__, __func__, chipInLowPower);
		}
	}
	return 0;
}

static struct i2c_driver IT7260_ts_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = DEVICE_NAME,
		.of_match_table = of_match_ptr(IT7260_match_table),
	},
	.probe = IT7260_ts_probe,
	.remove = IT7260_ts_remove,
	.id_table = IT7260_ts_id,
	.resume   = IT7260_ts_resume,
	.suspend = IT7260_ts_suspend,
};

static int __init IT7260_ts_init(void)
{
	return i2c_add_driver(&IT7260_ts_driver);
}

static void __exit IT7260_ts_exit(void)
{
	i2c_del_driver(&IT7260_ts_driver);
	gpio_free(16);
#ifdef CONFIG_ASUS_UTILITY
	unregister_mode_notifier(&display_mode_notifier);
#endif
	wake_lock_destroy(&touch_lock);
	wake_lock_destroy(&touch_time_lock);
	if (IT7260_wq)
		destroy_workqueue(IT7260_wq);
}

module_init(IT7260_ts_init);
module_exit(IT7260_ts_exit);

MODULE_DESCRIPTION("IT7260 Touchscreen Driver");
MODULE_LICENSE("GPL");
