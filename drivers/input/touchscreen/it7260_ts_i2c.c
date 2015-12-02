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
#define SCREEN_X_RESOLUTION		320
#define SCREEN_Y_RESOLUTION		320

#define IOC_MAGIC			'd'
#define IOCTL_SET			_IOW(IOC_MAGIC, 1, struct ioctl_cmd168)
#define IOCTL_GET			_IOR(IOC_MAGIC, 2, struct ioctl_cmd168)

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
#define VER_FIRMWARE			0x00
#define VER_CONFIG			0x06
#define VERSION_LENGTH			10
#define CMD_PWR_CTL			0x04 /* subcommand is zero, next byte is power mode */
#define PWR_CTL_LOW_POWER_MODE		0x01
#define PWR_CTL_SLEEP_MODE		0x02
#define CMD_UNKNOWN_7			0x07 /* command is not documented in the datasheet v1.0.0.7 */
#define CMD_FIRMWARE_REINIT_C		0x0C
#define CMD_CALIBRATE			0x13 /* needs to be followed by 4 bytes of zeroes */
#define CMD_FIRMWARE_UPGRADE		0x60
#define FIRMWARE_MODE_ENTER		0x00
#define FIRMWARE_MODE_EXIT		0x80
#define CMD_SET_START_OFFSET		0x61 /* address for FW read/write */
#define CMD_FW_WRITE			0x62 /* subcommand is number of bytes to write */
#define CMD_FW_READ			0x63 /* subcommand is number of bytes to read */
#define CMD_FIRMWARE_REINIT_6F		0x6F
#define CMD_CHECKSUM			0x1A

#define FW_WRITE_CHUNK_SIZE		128
#define FW_WRITE_RETRY_COUNT		4
#define CHIP_FLASH_SIZE			0x8000
#define SYSFS_FW_UPLOAD_MODE_MANUAL	2
#define SYSFS_RESULT_FAIL		(-1)
#define SYSFS_RESULT_NOT_DONE		0
#define SYSFS_RESULT_SUCCESS		1
#define DEVICE_READY_MAX_WAIT		50

/* result of reading with BUF_QUERY bits */
#define CMD_STATUS_BITS			0x07
#define CMD_STATUS_DONE			0x00
#define CMD_STATUS_BUSY			0x01
#define CMD_STATUS_ERROR		0x02
#define PT_INFO_BITS			0xF8
#define BT_INFO_NONE			0x00
#define PT_INFO_YES			0x80
#define BT_INFO_NONE_BUT_DOWN		0x08 /* no new data but finder(s) still down */

#define ENDLESS true
#define SLOW true

/* use this to include integers in commands */
#define CMD_UINT16(v)	((uint8_t)(v)) , ((uint8_t)((v) >> 8))

struct FingerData {
	uint8_t xLo;
	uint8_t hi;
	uint8_t yLo;
	uint8_t pressure;
};

struct PointData {
	uint8_t flags;
	uint8_t palm;
	struct FingerData fd[3];
};

#define PD_FLAGS_DATA_TYPE_BITS		0xF0
#define PD_FLAGS_DATA_TYPE_TOUCH	0x00 /* other types (like chip-detected gestures) exist but we do not care */
#define PD_FLAGS_NOT_PEN		0x08 /* set if pen touched, clear if finger(s) */
#define PD_FLAGS_HAVE_FINGERS		0x07 /* a bit for each finger data that is valid (from lsb to msb) */
#define PD_PALM_FLAG_BIT		0x01
#define FD_PRESSURE_BITS		0x0F
#define FD_PRESSURE_NONE		0x00
#define FD_PRESSURE_HOVER		0x01
#define FD_PRESSURE_LIGHT		0x02
#define FD_PRESSURE_NORMAL		0x04
#define FD_PRESSURE_HIGH		0x08
#define FD_PRESSURE_HEAVY		0x0F

struct ioctl_cmd168 {
	uint16_t bufferIndex;
	uint16_t length;
	uint16_t buffer[MAX_BUFFER_SIZE];
};

struct IT7260_ts_data {
	struct i2c_client *client;
	struct input_dev *touch_dev;
	struct input_dev *palm_dev;
	struct delayed_work afterpalm_work;
	struct delayed_work touchidle_on_work;
	struct delayed_work exit_idle_work;
	uint8_t reset_gpio;
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
	TOUCH_UP = 1,
	TOUCH_DOWN
};

static int8_t fwUploadResult = SYSFS_RESULT_NOT_DONE;
static int8_t calibrationWasSuccessful = SYSFS_RESULT_NOT_DONE;

static DEFINE_MUTEX(sleepModeMutex);
static bool chipAwake = true;;
static bool hadFingerDown;
static bool hadPalmDown;
static bool isTouchLocked;
static bool isDriverAvailable = true;;
static bool chipInLowPower;
static bool driverInLowPower;
static bool touchMissed;
static int suspend_touch_down;
static int suspend_touch_up;
static struct IT7260_ts_data *gl_ts;
static struct wake_lock touch_lock;
static struct wake_lock touch_time_lock;
static int lastTouch = TOUCH_UP;
static unsigned long last_time_exit_low;
static char fwVersion[30];

#define I2C_RETRY_DELAY		15		/* Waiting for signals [ms] */
#define I2C_RETRIES		2		/* Number of retries */
#define WAKELOCK_HOLD_MS	(HZ/2)

#define LOGE(...)	pr_err(DEVICE_NAME ": " __VA_ARGS__)
#define LOGI(...)	pr_info(DEVICE_NAME ": " __VA_ARGS__)

/* workqueue for delay_work */
static struct workqueue_struct *IT7260_wq;

/* internal use func - does not make sure chip is ready before read */
static uint8_t i2c_read_data(uint8_t bufferIndex, uint8_t *dataBuffer, uint16_t dataLength)
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
		dev_err(&gl_ts->client->dev, "read transfer error\n");
		err = -EIO;
		return err;
	}

	return 0;
}

static uint8_t i2c_write_data(uint8_t bufferIndex, const uint8_t *dataBuffer, uint16_t dataLength)
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
		dev_err(&gl_ts->client->dev, "write transfer error\n");
		err = -EIO;
		return err;
	}

	return 0;
}

/*
 * Device is apparently always ready for i2c but not for actual register reads/writes.
 * This function ascertains it is ready for that too. the results of this call often
 * were ignored.
 */
static bool wait_device_ready(bool endless, bool slow)
{
	uint8_t ucQuery;
	uint32_t count = DEVICE_READY_MAX_WAIT;

	do {
		if (i2c_read_data(BUF_QUERY, &ucQuery, sizeof(ucQuery)) < 0)
			ucQuery = CMD_STATUS_BUSY;

		if (slow)
			mdelay(10);
		if (!endless)
			count--;

	} while ((ucQuery & CMD_STATUS_BUSY) && count);

	if (ucQuery == CMD_STATUS_DONE)
		return true;
	else
		return false;
}

static uint8_t i2c_read_with_check(uint8_t bufferIndex, uint8_t *dataBuffer, uint16_t dataLength)
{
	wait_device_ready(false, false);
	if (i2c_read_data(bufferIndex, dataBuffer, dataLength) == 0)
		return true;
	else
		return false;
}

static uint8_t i2c_write_with_check(uint8_t bufferIndex, const uint8_t *dataBuffer, uint16_t dataLength)
{
	wait_device_ready(false, false);
	if (i2c_write_data(bufferIndex, dataBuffer, dataLength) == 0)
		return true;
	else
		return false;
}

static bool chipFirmwareReinitialize(uint8_t cmdOfChoice)
{
	uint8_t cmd[] = {cmdOfChoice};
	uint8_t rsp[2];

	if (!i2c_write_with_check(BUF_COMMAND, cmd, sizeof(cmd)))
		return false;

	if (!i2c_read_with_check(BUF_RESPONSE, rsp, sizeof(rsp)))
		return false;

	/* a reply of two zero bytes signifies success */
	return !rsp[0] && !rsp[1];
}

static bool chipFirmwareUpgradeModeEnterExit(bool enter)
{
	uint8_t cmd[] = {CMD_FIRMWARE_UPGRADE, 0, 'I', 'T', '7', '2', '6', '0', 0x55, 0xAA};
	uint8_t resp[2];

	cmd[1] = enter ? FIRMWARE_MODE_ENTER : FIRMWARE_MODE_EXIT;
	if (!i2c_write_with_check(BUF_COMMAND, cmd, sizeof(cmd)))
		return false;

	if (!i2c_read_with_check(BUF_RESPONSE, resp, sizeof(resp)))
		return false;

	/* a reply of two zero bytes signifies success */
	return !resp[0] && !resp[1];
}

static bool chipSetStartOffset(uint16_t offset)
{
	uint8_t cmd[] = {CMD_SET_START_OFFSET, 0, CMD_UINT16(offset)};
	uint8_t resp[2];

	if (!i2c_write_with_check(BUF_COMMAND, cmd, 4))
		return false;


	if (!i2c_read_with_check(BUF_RESPONSE, resp, sizeof(resp)))
		return false;


	/* a reply of two zero bytes signifies success */
	return !resp[0] && !resp[1];
}


/* write fwLength bytes from fwData at chip offset writeStartOffset */
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
			i2c_write_with_check(BUF_COMMAND, cmdWrite, 2 + curWriteSz);

			/* set offset and read the data back */
			msleep(10);
			chipSetStartOffset(writeStartOffset + curDataOfst);
			i2c_write_with_check(BUF_COMMAND, cmdRead, sizeof(cmdRead));
			i2c_read_with_check(BUF_RESPONSE, bufRead, curWriteSz);

			/* verify. If success break out of retry loop */
			for (i = 0; i < curWriteSz && bufRead[i] == cmdWrite[i + 2]; i++)
				;
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

static bool chipFirmwareUpload(uint32_t fwLen, const uint8_t *fwData, uint32_t cfgLen, const uint8_t *cfgData)
{
	bool ret = false;

	/* enter fw upload mode */
	if (!chipFirmwareUpgradeModeEnterExit(true))
		return false;

	/* flash the firmware if requested */
	if (fwLen && fwData && !chipFlashWriteAndVerify(fwLen, fwData, 0)) {
		LOGE("failed to upload touch firmware\n");
		goto out;
	}

	/* flash config data if requested */
	if (fwLen && fwData && !chipFlashWriteAndVerify(cfgLen, cfgData, CHIP_FLASH_SIZE - cfgLen)) {
		LOGE("failed to upload touch cfg data\n");
		goto out;
	}

out:
	ret = chipFirmwareUpgradeModeEnterExit(false) &&
		chipFirmwareReinitialize(CMD_FIRMWARE_REINIT_6F);
	return ret;
}

static bool ChangePalmThreshold(uint8_t threshold_H, uint8_t threshold_L)
{
	uint8_t puc_cmd_buf[4], resp[2];

	puc_cmd_buf[0] = 0x21;
	puc_cmd_buf[1] = 0x00;
	puc_cmd_buf[2] = threshold_L;
	puc_cmd_buf[3] = threshold_H;
	resp[0] = 0xFF;
	resp[1] = 0xFF;

	if (i2c_write_with_check(BUF_COMMAND, puc_cmd_buf, sizeof(puc_cmd_buf)) != true) {
		LOGE("[%d] %s i2c write fail.\n", __LINE__, __func__);
		return false;
	}

	if (i2c_read_with_check(BUF_RESPONSE, resp, sizeof(resp)) != true) {
		LOGE("[%d] %s i2c read fail.\n", __LINE__, __func__);
		return false;
	}

	LOGI("Palm threshold = 0x%02X%02X,  resp= 0x%02X%02X.\n", threshold_H, threshold_L, resp[1], resp[0]);
	return !resp[1] && !resp[0];
}

/* both buffers should be VERSION_LENGTH in size,
 * but only a part of them is significant */
static bool chipGetVersions(uint8_t *verFw, uint8_t *verCfg, bool logIt)
{
	/* this code to get versions is reproduced as was written, but it does not make sense. Something here *PROBABLY IS* wrong */
	static const uint8_t cmdReadFwVer[] = {CMD_READ_VERSIONS, VER_FIRMWARE};
	static const uint8_t cmdReadCfgVer[] = {CMD_READ_VERSIONS, VER_CONFIG};
	bool ret = false;

	disable_irq(gl_ts->client->irq);
	/* this structure is so that we definitely do all the calls, but still return a status in case anyone cares */
	ret = i2c_write_with_check(BUF_COMMAND, cmdReadFwVer, sizeof(cmdReadFwVer)) || ret;
	ret = i2c_read_with_check(BUF_RESPONSE, verFw, VERSION_LENGTH) || ret;
	ret = i2c_write_with_check(BUF_COMMAND, cmdReadCfgVer, sizeof(cmdReadCfgVer)) || ret;
	ret = i2c_read_with_check(BUF_RESPONSE, verCfg, VERSION_LENGTH) || ret;
	enable_irq(gl_ts->client->irq);

	if (logIt)
		LOGI("current versions: fw@{%X,%X,%X,%X}, cfg@{%X,%X,%X,%X}\n",
			verFw[5], verFw[6], verFw[7], verFw[8],
			verCfg[1], verCfg[2], verCfg[3], verCfg[4]);

	if (ret)
		return false;
	else
		return true;
}

#ifdef CONFIG_ASUS_UTILITY
static void chipLowPowerMode(bool low)
{

	LOGI("%s: low power %s\n", __func__, low ? "enter" : "exit");

	if (low) {
		input_mt_slot(gl_ts->touch_dev, 0);
		input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
		input_mt_slot(gl_ts->touch_dev, 1);
		input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
		input_sync(gl_ts->touch_dev);
		if (!(driverInLowPower)) {
			smp_wmb();
			enable_irq_wake(gl_ts->client->irq);
		}
		driverInLowPower = true;
		isTouchLocked = true;
		wake_unlock(&touch_lock);
		queue_delayed_work(IT7260_wq, &gl_ts->touchidle_on_work, 500);
	} else {
		cancel_delayed_work(&gl_ts->touchidle_on_work);

		/* Touch Reset */
		gpio_direction_output(gl_ts->reset_gpio, 1);
		msleep(50);
		gpio_direction_output(gl_ts->reset_gpio, 0);
		msleep(50);
		chipInLowPower = false;

		if (driverInLowPower) {
			smp_wmb();
			disable_irq_wake(gl_ts->client->irq);
		}
		driverInLowPower = false;
		isTouchLocked = false;
		hadPalmDown = false;
		last_time_exit_low = jiffies;
		wake_unlock(&touch_lock);
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
	bool autoUpgrade = false;

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
	autoUpgrade = (verFw[5] != fw->data[8] || verFw[6] != fw->data[9] || verFw[7] != fw->data[10] || verFw[8] != fw->data[11]) ||
			(verCfg[1] != cfg->data[cfgLen - 8] || verCfg[2] != cfg->data[cfgLen - 7] || verCfg[3] != cfg->data[cfgLen - 6] || verCfg[4] != cfg->data[cfgLen - 5]);

	/* fix touch firmware/config update failed issue */
	/* this code to check versions is reproduced as was written, but it does not quite make sense. Something here *IS* wrong */
	fwUploadResult = SYSFS_RESULT_NOT_DONE;
	if (fwLen && cfgLen) {
		if (manualUpgrade || autoUpgrade) {
			LOGI("firmware/config will be upgraded\n");
			disable_irq(gl_ts->client->irq);
			success = chipFirmwareUpload(fwLen, fw->data, cfgLen, cfg->data);
			enable_irq(gl_ts->client->irq);

			fwUploadResult = success ? SYSFS_RESULT_SUCCESS : SYSFS_RESULT_FAIL;
			LOGI("upload %s\n", success ? "success" : "failed");
		} else {
			LOGI("firmware/config upgrade not needed\n");
		}
	}

	chipGetVersions(verFw, verCfg, true);
	snprintf(fwVersion, sizeof(fwVersion), "%d,%d,%d,%d # %d,%d,%d,%d", verFw[5], verFw[6],
		verFw[7], verFw[8], verCfg[1], verCfg[2], verCfg[3], verCfg[4]);

	if (fwLen)
		release_firmware(fw);

	if (cfgLen)
		release_firmware(cfg);

	return count;
}

static ssize_t sysfsUpgradeShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, sizeof(fwUploadResult), "%d", fwUploadResult);
}

static ssize_t sysfsCalibrationShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, sizeof(calibrationWasSuccessful), "%d", calibrationWasSuccessful);
}

static bool chipSendCalibrationCmd(bool autoTuneOn)
{
	uint8_t cmdCalibrate[] = {CMD_CALIBRATE, 0, autoTuneOn ? 1 : 0, 0, 0};
	return i2c_write_with_check(BUF_COMMAND, cmdCalibrate, sizeof(cmdCalibrate));
}

static ssize_t sysfsCalibrationStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	uint8_t resp;

	if (!chipSendCalibrationCmd(false))
		LOGE("failed to send calibration command\n");
	else {
		calibrationWasSuccessful = i2c_read_with_check(BUF_RESPONSE, &resp, sizeof(resp)) ? SYSFS_RESULT_SUCCESS : SYSFS_RESULT_FAIL;

		/* previous logic that was here never called chipFirmwareReinitialize() due to checking a guaranteed-not-null value against null. We now call it. Hopefully this is OK */
		if (!resp)
			LOGI("chipFirmwareReinitialize -> %s\n", chipFirmwareReinitialize(CMD_FIRMWARE_REINIT_6F) ? "success" : "fail");
	}

	return count;
}

static ssize_t sysfsPointShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t pointData[sizeof(struct PointData)];
	uint8_t readSuccess;
	ssize_t ret;

	readSuccess = i2c_read_data(BUF_POINT_INFO, pointData, sizeof(pointData));
	ret = snprintf(buf, 50, "point_show read ret[%d]--point[%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x]=\n",
			readSuccess, pointData[0], pointData[1], pointData[2], pointData[3],
			pointData[4], pointData[5], pointData[6], pointData[7], pointData[8],
			pointData[9], pointData[10], pointData[11], pointData[12], pointData[13]);


	LOGI("%s", buf);

	return ret;
}

static ssize_t sysfsStatusShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t ret;
	ret = wait_device_ready(false, false);
	return snprintf(buf, 3, "%d\n", ret ? 1 : 0);
}

static ssize_t sysfsVersionShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, sizeof(fwVersion), "%s\n", fwVersion);
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
	} else
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
			i2c_read_data(BUF_QUERY, &dummy, sizeof(dummy));
			enable_irq(gl_ts->client->irq);
			LOGI("touch is going to wake!\n\n");
		} else {
			disable_irq(gl_ts->client->irq);
			i2c_write_data(BUF_COMMAND, cmdGoSleep, sizeof(cmdGoSleep));
			LOGI("touch is going to sleep...\n\n");
		}
		chipAwake = goToWake;
		mutex_unlock(&sleepModeMutex);
		return count;
	} else
		return -EINTR;
}

static ssize_t sysfsChecksumShow(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint8_t cmd[] = {CMD_CHECKSUM, 0x09, 0x03};
	uint8_t rsp[2];
	uint8_t ret = 0;

	if (i2c_write_data(BUF_COMMAND, cmd, sizeof(cmd)) < 0)
		return snprintf(buf, 3, "%d\n", ret);

	wait_device_ready(ENDLESS, false);
	if (i2c_read_data(BUF_RESPONSE, rsp, sizeof(rsp)) < 0)
		return snprintf(buf, 3, "%d\n", ret);

	if (rsp[0] || rsp[1])
		ret = 0;
	else
		ret = 1;
	LOGI("%s: rsp:%d, %d\n", __func__, rsp[0], rsp[1]);
	return snprintf(buf, 3, "%d\n", ret ? 1 : 0);
}

static int atoi(const char *in)
{
	int ret = 0;
	if ('0' <= *in && *in <= '9')
		ret = *in - '0';
	if ('a' <= *in && *in <= 'f')
		ret = *in - 'a' + 10;
	if ('A' <= *in && *in <= 'F')
		ret = *in - 'A' + 10;

	return ret;
}

static ssize_t sysfsPalmStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	uint8_t threshold_H, threshold_L;
	char str[4];

	strlcpy(str, buf, sizeof(str));
	threshold_H = atoi(&str[0])*16 + atoi(&str[1]);
	threshold_L = atoi(&str[2])*16 + atoi(&str[3]);
	LOGI("%s: get threshold %s", __func__, str);
	LOGI("%s: get threshold %d %d", __func__, threshold_H, threshold_L);
	ChangePalmThreshold(threshold_H, threshold_L);
	return count;
}

static DEVICE_ATTR(status, S_IRUGO|S_IWUSR|S_IRGRP, sysfsStatusShow, NULL);
static DEVICE_ATTR(version, S_IRUGO|S_IWUSR|S_IRGRP, sysfsVersionShow, NULL);
static DEVICE_ATTR(sleep, S_IRUGO|S_IWUSR|S_IRGRP, sysfsSleepShow, sysfsSleepStore);
static DEVICE_ATTR(checksum, S_IRUGO|S_IWUSR|S_IRGRP, sysfsChecksumShow, NULL);
static DEVICE_ATTR(palm, S_IWGRP|S_IWUSR, NULL, sysfsPalmStore);

static struct attribute *it7260_attrstatus[] = {
	&dev_attr_status.attr,
	&dev_attr_version.attr,
	&dev_attr_sleep.attr,
	&dev_attr_checksum.attr,
	&dev_attr_palm.attr,
	NULL
};

static const struct attribute_group it7260_attrstatus_group = {
	.attrs = it7260_attrstatus,
};

static DEVICE_ATTR(calibration, S_IRUGO|S_IWUSR|S_IRGRP, sysfsCalibrationShow, sysfsCalibrationStore);
static DEVICE_ATTR(upgrade, S_IRUGO|S_IWUSR|S_IRGRP, sysfsUpgradeShow, sysfsUpgradeStore);
static DEVICE_ATTR(point, S_IRUGO|S_IWUSR|S_IRGRP, sysfsPointShow, NULL);


static struct attribute *it7260_attributes[] = {
	&dev_attr_calibration.attr,
	&dev_attr_upgrade.attr,
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

static int mode_notify_sys(struct notifier_block *notif, unsigned long code, void *data)
{
	LOGI("[PF]%s +\n", __func__);
	switch (code) {
	case 0: /* FB_BLANK_ENTER_NON_INTERACTIVE */
		chipLowPowerMode(1);
		break;
	case 1: /* FB_BLANK_ENTER_INTERACTIVE */
		chipLowPowerMode(0);
		break;
	default:
		/* huh?? */
		break;
	}
	LOGI("[PF]%s -\n", __func__);
	return 0;
}

static struct notifier_block display_mode_notifier = {
	.notifier_call =    mode_notify_sys,
};

/* parsing row data to x, y, pressure */
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

static void waitNotifyEvt(struct work_struct *work)
{
	if (!driverInLowPower) {
		isTouchLocked = false;
	}
}

static void touchIdleOnEvt(struct work_struct *work)
{
	static const uint8_t cmdLowPower[] = { CMD_PWR_CTL, 0x00, PWR_CTL_LOW_POWER_MODE};
	chipInLowPower = true;
	if (!ChangePalmThreshold(0x70, 0x00))
		LOGE("%s: change palm threshold fail.\n", __func__);
	if (i2c_write_data(BUF_COMMAND, cmdLowPower, sizeof(cmdLowPower)) < 0)
		LOGE("%s: fail to enter low power mode\n", __func__);
}

static void exitIdleEvt(struct work_struct *work)
{
	LOGI("IT7260: Special IRQ trigger touch event\n");
	isTouchLocked = true;
	input_mt_slot(gl_ts->touch_dev, 0);
	input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, true);
	input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_X, 160);
	input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_Y, 160);
	input_sync(gl_ts->touch_dev);
	input_mt_slot(gl_ts->touch_dev, 0);
	input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
	input_sync(gl_ts->touch_dev);
	wake_lock_timeout(&touch_time_lock, WAKELOCK_HOLD_MS);
	wake_unlock(&touch_lock);
	last_time_exit_low = jiffies;
	if (!ChangePalmThreshold(0xA0, 0x00))
		LOGE("%s: change palm threshold fail.\n", __func__);
}

static void sendPalmEvt(void)
{
	input_report_key(gl_ts->touch_dev, KEY_SLEEP, 1);
	input_sync(gl_ts->touch_dev);
	input_report_key(gl_ts->touch_dev, KEY_SLEEP, 0);
	input_sync(gl_ts->touch_dev);

	if (gl_ts->palm_en) {
		input_report_abs(gl_ts->palm_dev, ABS_DISTANCE, 1);
		input_sync(gl_ts->palm_dev);
		input_report_abs(gl_ts->palm_dev, ABS_DISTANCE, 0);
		input_sync(gl_ts->palm_dev);
	}
}

/* contrary to the name this code does not just read data - lots of processing happens */
static void readTouchDataPoint(void)
{
	struct PointData pointData;
	uint8_t devStatus;
	uint8_t pressure1 = FD_PRESSURE_NONE;
	uint8_t pressure2 = FD_PRESSURE_NONE;
	uint16_t x1, y1, x2, y2;

	/* verify there is point data to read & it is readable and valid */
	i2c_read_data(BUF_QUERY, &devStatus, sizeof(devStatus));
	if (!((devStatus & PT_INFO_BITS) & PT_INFO_YES)) {
		/* LOGE("readTouchDataPoint() called when no data available (0x%02X)\n", devStatus); */
		return;
	}
	/* read point data from i2c */
	if (i2c_read_data(BUF_POINT_INFO, (void *)&pointData, sizeof(pointData)) < 0) {
		LOGE("%s: failed to read point data buffer\n", __func__);
		return;
	}
	/* check point data is position data type */
	if ((pointData.flags & PD_FLAGS_DATA_TYPE_BITS) != PD_FLAGS_DATA_TYPE_TOUCH) {
		LOGE("%s: dropping non-point data of type 0x%02X\n", __func__, pointData.flags);
		if (!ChangePalmThreshold(0xA0, 0x00))
			LOGE("%s: change palm threshold fail.\n", __func__);
		return;
	}
	/* palm detection */
	if ((pointData.palm & PD_PALM_FLAG_BIT) && !isTouchLocked && !hadPalmDown) {
		LOGI("%s: handle palm interrupt\n", __func__);
		if (jiffies - last_time_exit_low > HZ/4) {
			isTouchLocked = true;
			hadPalmDown = true;
			sendPalmEvt();
			queue_delayed_work(IT7260_wq, &gl_ts->afterpalm_work, 30);
		}
		return;
	}

	/* for mouti-touch just check point 0 (0x01) and point 1 (0x02) */
	if ((pointData.flags & PD_FLAGS_HAVE_FINGERS) & 0x03)
		readFingerData(&x1, &y1, &pressure1, &x2, &y2, &pressure2, pointData.fd);
	pr_debug(DEVICE_NAME "point data: P0:(%d, %d, %d) P2:(%d, %d, %d)\n",
		x1, y1, pressure1, x2, y2, pressure2);
	if (pressure1 >= FD_PRESSURE_LIGHT || pressure2 >= FD_PRESSURE_LIGHT) {

		if (!hadFingerDown)
			hadFingerDown = true;

		/* filter points when palming or touching screen edge */
		if (!isTouchLocked && pointData.flags & 0x01) {
			input_mt_slot(gl_ts->touch_dev, 0);
			input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, true);
			input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_X, x1);
			input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_Y, y1);
		} else {
			input_mt_slot(gl_ts->touch_dev, 0);
			input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
		}

		if (!isTouchLocked && pointData.flags & 0x02) {
			input_mt_slot(gl_ts->touch_dev, 1);
			input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, true);
			input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_X, x2);
			input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_Y, y2);
		} else {
			input_mt_slot(gl_ts->touch_dev, 1);
			input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
		}

		input_sync(gl_ts->touch_dev);

	} else {
		hadFingerDown = false;
		hadPalmDown = false;
		input_mt_slot(gl_ts->touch_dev, 0);
		input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
		input_mt_slot(gl_ts->touch_dev, 1);
		input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
		input_sync(gl_ts->touch_dev);
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

	if (!isTouchLocked) {
		i2c_read_data(BUF_QUERY, &devStatus, sizeof(devStatus));
		if (!((devStatus & PT_INFO_BITS) & PT_INFO_YES)) {
			/* LOGE("readTouchDataPoint_Ambient() called when no data available (0x%02X)\n", devStatus);*/
			isTouchLocked = true;
			wake_unlock(&touch_lock);
			return;
		}
		if (i2c_read_data(BUF_POINT_INFO, (void *)&pointData, sizeof(pointData)) < 0) {
			LOGE("readTouchDataPoint_Ambient() failed to read point data buffer\n");
			isTouchLocked = true;
			wake_unlock(&touch_lock);
			return;
		}
		if ((pointData.flags & PD_FLAGS_DATA_TYPE_BITS) != PD_FLAGS_DATA_TYPE_TOUCH) {
			if (pointData.flags == 16) {
				LOGE("readTouchDataPoint_Ambient() send touch event by type 0x%02X\n", pointData.flags);
			} else {
				LOGE("readTouchDataPoint_Ambient() dropping non-point data of type 0x%02X\n", pointData.flags);
				isTouchLocked = true;
				wake_unlock(&touch_lock);
				return;
			}
		}

		if ((pointData.flags & PD_FLAGS_HAVE_FINGERS) & 0x03)
			readFingerData(&x1, &y1, &pressure1, &x2, &y2, &pressure2, pointData.fd);

		if ((pointData.palm & PD_PALM_FLAG_BIT)) {
			if (hadFingerDown)
				cancel_delayed_work(&gl_ts->exit_idle_work);
			hadFingerDown = false;
		}

		if (pressure1 >= FD_PRESSURE_LIGHT || pressure2 >= FD_PRESSURE_LIGHT) {
			if (hadFingerDown)
				cancel_delayed_work(&gl_ts->exit_idle_work);
			else
				hadFingerDown = true;
		} else if (hadFingerDown && (!(pointData.palm & PD_PALM_FLAG_BIT))) {
			hadFingerDown = false;
			suspend_touch_up = getMsTime();

			cancel_delayed_work(&gl_ts->exit_idle_work);
			if (lastTouch == TOUCH_UP)
				touchMissed = true;
			else
				lastTouch = TOUCH_UP;

			if (touchMissed || suspend_touch_up - suspend_touch_down < 700) {
				if (touchMissed) {
					LOGI("%s: touch down missed, send touch up\n",
					     __func__);
					touchMissed = false;
				}
				input_mt_slot(gl_ts->touch_dev, 0);
				input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, true);
				input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_X, 160);
				input_report_abs(gl_ts->touch_dev, ABS_MT_POSITION_Y, 160);
				input_sync(gl_ts->touch_dev);
				input_mt_slot(gl_ts->touch_dev, 0);
				input_mt_report_slot_state(gl_ts->touch_dev, MT_TOOL_FINGER, false);
				input_sync(gl_ts->touch_dev);
				wake_lock_timeout(&touch_time_lock, WAKELOCK_HOLD_MS);
				last_time_exit_low = jiffies;
			}
			isTouchLocked = true;
			wake_unlock(&touch_lock);
		} else if (pointData.flags == 16) {
			hadFingerDown = true;
			queue_delayed_work(IT7260_wq, &gl_ts->exit_idle_work, 5);
		} else if (pressure1 == 0 && pressure2 == 0 && (!(pointData.palm & PD_PALM_FLAG_BIT))) {
			isTouchLocked = true;
			wake_unlock(&touch_lock);
		}

	} else if (isTouchLocked) {
		msleep(10);
		if (isDriverAvailable) {
			wake_lock(&touch_lock);
			isTouchLocked = false;
			chipInLowPower = false;
			suspend_touch_down = getMsTime();
			if (lastTouch == TOUCH_UP)
				lastTouch = TOUCH_DOWN;
			else
				touchMissed = true;
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

static bool chipIdentifyIT7260(void)
{
	static const uint8_t cmdIdent[] = {CMD_IDENT_CHIP};
	static const uint8_t expectedID[] = {0x0A, 'I', 'T', 'E', '7', '2', '6', '0'};
	uint8_t chipID[10] = {0};

	if (i2c_write_data(BUF_COMMAND, cmdIdent, sizeof(cmdIdent)) < 0) {
		LOGE("i2c_write_with_check() failed\n");
		return false;
	}

	if (i2c_read_data(BUF_RESPONSE, chipID, sizeof(chipID)) < 0) {
		LOGE("i2c_read_with_check() failed\n");
		return false;
	}

	LOGI("chipIdentifyIT7260 read id: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
		chipID[0], chipID[1], chipID[2], chipID[3], chipID[4],
		chipID[5], chipID[6], chipID[7], chipID[8], chipID[9]);

	if (memcmp(chipID, expectedID, sizeof(expectedID)))
		return false;

	if (chipID[8] == '5' && chipID[9] == '6')
		LOGI("rev BX3 found\n");
	else if (chipID[8] == '6' && chipID[9] == '6')
		LOGI("rev BX4 found\n");
	else	/* unknown, but let's hope it is still a version we can talk to */
		LOGI("rev (0x%02X 0x%02X) found\n", chipID[8], chipID[9]);

	return true;
}

int parse_reset_gpio(struct device *dev)
{
	u32 value;
	struct device_node *node = dev->of_node;

	if (of_property_read_u32(node, "reset", &value)) {
		dev_err(dev, "Missing Touch Reset GPIO Value\n");
		return -EINVAL;
	}

	return value;
}

static int IT7260_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct IT7260_i2c_platform_data *pdata;
	int ret = -1;
	int err;

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
		dev_err(&client->dev, "failed to register sysfs #1\n");
		goto err_sysfs_grp_create_1;
	}

	if (!chipIdentifyIT7260()) {
		LOGE("chipIdentifyIT7260 FAIL");
		goto err_ident_fail_or_input_alloc;
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
	set_bit(INPUT_PROP_DIRECT, gl_ts->touch_dev->propbit);
	set_bit(KEY_SLEEP, gl_ts->touch_dev->keybit);
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
		LOGE("failed to allocate input device\n");
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
		LOGE("failed to register input device\n");
		goto err_input_register;
	}

	gl_ts->palm_en = 0;
	gl_ts->cdev = palm_cdev;
	gl_ts->cdev.enabled = 0;
	gl_ts->cdev.sensors_enable = palm_enable_set;
	if (sensors_classdev_register(&client->dev, &gl_ts->cdev)) {
		LOGE("[IT7260] %s: class device create failed\n", __func__);
		goto err_out;
	}

	touchMissed = false;

	IT7260_wq = create_workqueue("IT7260_wq");
	if (!IT7260_wq)
		goto err_check_functionality_failed;

	INIT_DELAYED_WORK(&gl_ts->afterpalm_work, waitNotifyEvt);
	INIT_DELAYED_WORK(&gl_ts->touchidle_on_work, touchIdleOnEvt);
	INIT_DELAYED_WORK(&gl_ts->exit_idle_work, exitIdleEvt);

	if (request_threaded_irq(client->irq, NULL, IT7260_ts_threaded_handler, IRQF_TRIGGER_LOW | IRQF_ONESHOT, client->name, gl_ts)) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_irq_reg;
	}

	if (IS_ENABLED(CONFIG_ASUS_UTILITY)) {
		register_mode_notifier(&display_mode_notifier);
	}

	if (sysfs_create_group(&(client->dev.kobj), &it7260_attr_group)) {
		dev_err(&client->dev, "failed to register sysfs #2\n");
		goto err_sysfs_grp_create_2;
	}
	wake_lock_init(&touch_lock, WAKE_LOCK_SUSPEND, "touch-lock");
	wake_lock_init(&touch_time_lock, WAKE_LOCK_SUSPEND, "touch-time-lock");

	gl_ts->reset_gpio = parse_reset_gpio(&client->dev);
	LOGI("IT7260: gpio_request %d\n", gl_ts->reset_gpio);
	err = gpio_request(gl_ts->reset_gpio, "CTP_RST_N");
	if (err < 0) {
		LOGI("IT7260: gpio_request %d error: %d\n", gl_ts->reset_gpio, err);
	}
	return 0;

err_sysfs_grp_create_2:
	if (IS_ENABLED(CONFIG_ASUS_UTILITY)) {
		unregister_mode_notifier(&display_mode_notifier);
	}
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
	return ret;
}

static int IT7260_ts_remove(struct i2c_client *client)
{
	destroy_workqueue(IT7260_wq);
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
		i2c_write_data(data.bufferIndex, buffer, data.length);
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
			i2c_read_data(data.bufferIndex, buffer, data.length);
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
	isDriverAvailable = true;
	return 0;
}

static int IT7260_ts_suspend(struct i2c_client *i2cdev, pm_message_t pmesg)
{
	static const uint8_t cmdLowPower[] = { CMD_PWR_CTL, 0x00, PWR_CTL_LOW_POWER_MODE};
	isTouchLocked = true;
	driverInLowPower = true;
	isDriverAvailable = false;

	cancel_delayed_work(&gl_ts->touchidle_on_work);
	enable_irq_wake(gl_ts->client->irq);

	if (!chipInLowPower) {
		i2c_write_data(BUF_COMMAND, cmdLowPower, sizeof(cmdLowPower));
		chipInLowPower = true;
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
	gpio_free(gl_ts->reset_gpio);
	if (IS_ENABLED(CONFIG_ASUS_UTILITY))
		unregister_mode_notifier(&display_mode_notifier);
	wake_lock_destroy(&touch_lock);
	wake_lock_destroy(&touch_time_lock);
	if (IT7260_wq)
		destroy_workqueue(IT7260_wq);
}

module_init(IT7260_ts_init);
module_exit(IT7260_ts_exit);

MODULE_DESCRIPTION("IT7260 Touchscreen Driver");
MODULE_LICENSE("GPL");
