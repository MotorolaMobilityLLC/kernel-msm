#ifndef __CAM_SENSOR_VSYNC_H__
#define __CAM_SENSOR_VSYNC_H__
#include <linux/ioctl.h>
#include <linux/videodev2.h>

#define GET_VSYNC_VALID(sync_setting) ((sync_setting) & 0x80 ? 1 : 0)
#define GET_VSYNC1_SETING(sync_setting) ((sync_setting) & 0x01)
#define GET_VSYNC2_SETING(sync_setting) ((sync_setting) & 0x02)

#define CAM_SENSOR_VSYNC_READ \
	_IOWR('X', BASE_VIDIOC_PRIVATE + 50, int)
#define CAM_SENSOR_VSYNC_READ32 \
	_IOWR('X', BASE_VIDIOC_PRIVATE + 50, int)

#define CAM_SENSOR_VSYNC_WRITE \
	_IOWR('X', BASE_VIDIOC_PRIVATE + 51, int)
#define CAM_SENSOR_VSYNC_WRITE32 \
	_IOWR('X', BASE_VIDIOC_PRIVATE + 51, int)

#define CAM_SENSOR_VSYNC_INIT \
	_IOWR('X', BASE_VIDIOC_PRIVATE + 52, int)
#define CAM_SENSOR_VSYNC_INIT32 \
	_IOWR('X', BASE_VIDIOC_PRIVATE + 52, int)

#define CAM_SENSOR_VSYNC_RELEASE \
	_IOWR('X', BASE_VIDIOC_PRIVATE + 53, int)
#define CAM_SENSOR_VSYNC_RELEASE32 \
	_IOWR('X', BASE_VIDIOC_PRIVATE + 53, int)
#endif
