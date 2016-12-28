#ifndef __MXG2320_H__
#define __MXG2320_H__

#include <linux/ioctl.h>
#include <linux/mutex.h>
/* add by James for using hrtimer */
#include <linux/hrtimer.h>

#define MXG_DEV_NAME            "mxg3300"
#define MXG_INPUT_DEV_NAME      "mxg3300_compass"
#define MXG_MISC_DEV_NAME       "mxg3300_dev"

#define MXG_CMP1_ID             0x13
#define MXG_CMP2_ID             0x30

/* MXG3300 Operation Mode */
#define MXG_MODE_SNG_MEASURE    0xC1
#define MXG_MODE_CONT_10HZ      0xC2
#define MXG_MODE_CONT_20HZ      0xC4
#define MXG_MODE_CONT_50HZ      0xC6
#define MXG_MODE_CONT_100HZ     0xC8
#define MXG_MODE_CONT_200HZ     0xCC
#define MXG_MODE_SELF_TEST      0x90 /* Selftest with 16bit data enable */
#define MXG_MODE_FUSE_ACCESS    0x1F
#define MXG_MODE_POWER_DOWN     0xC0
#define MXG_SOFT_RESET          0x01

/* Register map */
#define MXG_REG_DID             0x50
#define MXG_REG_INFO            0x52
#define MXG_REG_ST1             0x00
#define MXG_REG_XDH             0x01
#define MXG_REG_XDL             0x02
#define MXG_REG_YDH             0x03
#define MXG_REG_YDL             0x04
#define MXG_REG_ZDH             0x05
#define MXG_REG_ZDL             0x06
#define MXG_REG_ST2             0x08
#define MXG_REG_OPMODE          0x10
#define MXG_REG_SRST            0x11
#define MXG_REG_CHOP            0x12
#define MXG_REG_XASA            0x53
#define MXG_REG_YASA            0x54
#define MXG_REG_ZASA            0x55

/* Event Type */
#define EVENT_RAW_X             ABS_X
#define EVENT_RAW_Y             ABS_Y
#define EVENT_RAW_Z             ABS_Z

/* IOCTL Magic code */
#define MXG_TYPE                'M'

/* IOCTL Command List */
#define MXG_IOCTL_SET_ENABLE    _IOW(MXG_TYPE, 0x10, int)
#define MXG_IOCTL_GET_ENABLE    _IOR(MXG_TYPE, 0x20, int)
#define MXG_IOCTL_SET_DELAY     _IOW(MXG_TYPE, 0x30, unsigned int)
#define MXG_IOCTL_GET_DELAY     _IOR(MXG_TYPE, 0x40, unsigned int)
#define MXG_IOCTL_GET_INFO      _IOR(MXG_TYPE, 0x50, unsigned char[2])
#define MXG_IOCTL_GET_MCOEF     _IOR(MXG_TYPE, 0x60, unsigned char[3])

struct mxg_platform_data {
	/*
	 * position_array[0-2] value is 3-axis coodination.
	 * "0" value is [x-axis]
	 * "1" value is [y-axis]
	 * "2" value is [z-axis]
	 * position_array[3-5] value is axis sign.
	 * "0" value is Positive
	 * "1" value is Negative
	 */
	u32 position_array[6];
	int	use_hrtimer; /* reserved: add by James for using hrtimer. */
};

struct mxg_sensor_data {
	struct mxg_platform_data *plat_data;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct miscdevice *misc_dev;
	struct device *dev;
	struct sensors_classdev sensor_cdev;

	struct workqueue_struct *work_queue;
	struct delayed_work dwork;
	struct delayed_work selftest_work;
	wait_queue_head_t selftest_wq;
	atomic_t selftest_done;

	struct regulator *vdd_ana;
	struct regulator *vdd_io;
	bool power_on;

	/* start by James for using hrtimer. */
	struct timespec ts;
	struct  hrtimer poll_timer;
	int use_hrtimer;
	/* end by James for using hrtimer. */

	struct mutex lock;
	atomic_t en;
	u8 mode; /* Mode 0x1 : Single, 0x10 : Self-test */
	u32 odr;
	u8 mcoef[3];    /* axis sensitivity adjustment */
	s16 self[3];
	u8 info[3];
	s32 raw_data[3];
	s32 lastData[3];

	/* dummy value to avoid sensor event get eaten */
	int	rep_cnt;
	u32 flush_count;
};

#endif /* __MXG2320_H__ */
