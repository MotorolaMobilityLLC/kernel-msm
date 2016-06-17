/*
 * Definitions for akm09912 compass chip.
 */
#ifndef STML0XX_AKM_H
#define STML0XX_AKM_H

#include <linux/ioctl.h>

/* Device specific constant values */
#define AK09912_REG_WIA1			0x00
#define AK09912_REG_WIA2			0x01
#define AK09912_REG_INFO1			0x02
#define AK09912_REG_INFO2			0x03
#define AK09912_REG_ST1				0x10
#define AK09912_REG_HXL				0x11
#define AK09912_REG_HXH				0x12
#define AK09912_REG_HYL				0x13
#define AK09912_REG_HYH				0x14
#define AK09912_REG_HZL				0x15
#define AK09912_REG_HZH				0x16
#define AK09912_REG_TMPS			0x17
#define AK09912_REG_ST2				0x18
#define AK09912_REG_CNTL1			0x30
#define AK09912_REG_CNTL2			0x31
#define AK09912_REG_CNTL3			0x32
#define AK09912_REG_I2CDIS			0x36

#define AK09912_FUSE_ASAX			0x60
#define AK09912_FUSE_ASAY			0x61
#define AK09912_FUSE_ASAZ			0x62

#define AK09912_CNTL1_ENABLE_TEMP	0x80

#define AK09912_MODE_SNG_MEASURE	0x01
#define AK09912_MODE_SELF_TEST		0x10
#define AK09912_MODE_FUSE_ACCESS	0x1F
#define AK09912_MODE_POWERDOWN		0x00
#define AK09912_RESET_DATA			0x01

/* To avoid device dependency, convert to general name */
#define AKM_I2C_NAME			"akm09912"
#define AKM_MISCDEV_NAME		"stml0xx_akm"
#define AKM_SYSCLS_NAME			"compass"
#define AKM_SYSDEV_NAME			"akm09912"
#define AKM_REG_MODE			AK09912_REG_CNTL2
#define AKM_REG_RESET			AK09912_REG_CNTL3
#define AKM_REG_STATUS			AK09912_REG_ST1
#define AKM_WIA_VALE			0x48
#define AKM_MEASURE_TIME_US		10000
#define AKM_DRDY_IS_HIGH(x)		((x) & 0x01)
#define AKM_SENSOR_INFO_SIZE	2
#define AKM_SENSOR_CONF_SIZE	3
#define AKM_SENSOR_DATA_SIZE	9
#define AKM_SENSOR_TIME_SIZE    8   /* Size in bytes */

#define AKM_YPR_DATA_SIZE		19
#define AKM_RWBUF_SIZE			16

#define AKM_MODE_SNG_MEASURE		AK09912_MODE_SNG_MEASURE
#define AKM_MODE_SELF_TEST		AK09912_MODE_SELF_TEST
#define AKM_MODE_FUSE_ACCESS		AK09912_MODE_FUSE_ACCESS
#define AKM_MODE_POWERDOWN		AK09912_MODE_POWERDOWN
#define AKM_RESET_DATA			AK09912_RESET_DATA

#define ACC_DATA_FLAG		0
#define MAG_DATA_FLAG		1
#define FUSION_DATA_FLAG	2
#define AKM_NUM_SENSORS		3

#define ACC_DATA_READY		(1<<(ACC_DATA_FLAG))
#define MAG_DATA_READY		(1<<(MAG_DATA_FLAG))
#define FUSION_DATA_READY	(1<<(FUSION_DATA_FLAG))

#define AKMIO				0xA1

/* IOCTLs for AKM library */
#define ECS_IOCTL_READ			_IOWR(AKMIO, 0x01, char)
#define ECS_IOCTL_WRITE			_IOW(AKMIO, 0x02, char)
#define ECS_IOCTL_RESET			_IO(AKMIO, 0x03)
#define ECS_IOCTL_SET_MODE		_IOW(AKMIO, 0x10, char)
#define ECS_IOCTL_SET_YPR		_IOW(AKMIO, 0x11, \
					int[AKM_YPR_DATA_SIZE])
#define ECS_IOCTL_GET_INFO		_IOR(AKMIO, 0x20, \
					unsigned char[AKM_SENSOR_INFO_SIZE])
#define ECS_IOCTL_GET_CONF		_IOR(AKMIO, 0x21, \
					unsigned char[AKM_SENSOR_CONF_SIZE])
#define ECS_IOCTL_GET_DATA		_IOR(AKMIO, 0x22, \
					unsigned char[AKM_SENSOR_DATA_SIZE \
						+ AKM_SENSOR_TIME_SIZE])
#define ECS_IOCTL_GET_OPEN_STATUS	_IOR(AKMIO, 0x23, int)
#define ECS_IOCTL_GET_CLOSE_STATUS	_IOR(AKMIO, 0x24, int)
#define ECS_IOCTL_GET_DELAY		_IOR(AKMIO, 0x25, long long int)
#define ECS_IOCTL_GET_LAYOUT		_IOR(AKMIO, 0x26, char)
#define ECS_IOCTL_GET_ACCEL		_IOR(AKMIO, 0x30, short[3])
#define ECS_IOCTL_CHECK_READY           _IOR(AKMIO, 0x31, char)

/* AKM data queue */
#define AKM_DATA_MAX_QUEUE_SIZE 10
#define AKM_DATA_QUEUE_ENTRY_SIZE \
			(AKM_SENSOR_DATA_SIZE + AKM_SENSOR_TIME_SIZE)
struct akm_data_queue_t {
	int num_data;
	uint8_t head;
	uint8_t tail;
	uint8_t data[AKM_DATA_QUEUE_ENTRY_SIZE * AKM_DATA_MAX_QUEUE_SIZE];
};

#endif

