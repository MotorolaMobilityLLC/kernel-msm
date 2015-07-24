/*
 * vl6180x_port_i2c.c
 *
 *  Created on: Oct 22, 2014
 *      Author:  Teresa Tao
 */

#include "vl6180x_i2c.h"
#include <linux/i2c.h>
#include <linux/module.h>
#include "stmvl6180-i2c.h"
#include "stmvl6180-cci.h"

#define I2C_M_WR			0x00
#ifdef CAMERA_CCI
static struct msm_camera_i2c_client *pclient;
#else
static struct i2c_client *pclient;
#endif

void i2c_setclient(void *client)
{
#ifdef CAMERA_CCI
	pclient = (struct msm_camera_i2c_client *)client;
#else
	pclient = (struct i2c_client *)client;
#endif

}

void *i2c_getclient(void)
{
	return (void *)pclient;
}

/** int  VL6180x_I2CWrite(VL6180xDev_t dev, void *buff, uint8_t len);
 * @brief       Write data buffer to VL6180x device via i2c
 * @param dev   The device to write to
 * @param buff  The data buffer
 * @param len   The length of the transaction in byte
 * @return      0 on success
 */
int VL6180x_I2CWrite(VL6180xDev_t dev, uint8_t *buff, uint8_t len)
{


	int err = 0;
#ifdef CAMERA_CCI
	uint16_t index;

	index = ((uint16_t)buff[0] << 8) | (uint16_t)buff[1];
	/*pr_err("%s: index: %d len: %d\n", __func__, index, len); */

	if (len == 3) {
		uint8_t data;
		data = buff[2];
		/* for byte access */
		err = pclient->i2c_func_tbl->i2c_write(pclient, index, data, MSM_CAMERA_I2C_BYTE_DATA);
		if (err < 0) {
			pr_err("%s:%d failed status=%d\n", __func__, __LINE__, err);
			return err;
		}
	} else if (len == 4) {
		uint16_t data;
		data = ((uint16_t)buff[2] << 8) | (uint16_t)buff[3];
		err = pclient->i2c_func_tbl->i2c_write(pclient, index, data, MSM_CAMERA_I2C_WORD_DATA);
		if (err < 0) {
			pr_err("%s:%d failed status=%d\n", __func__, __LINE__, err);
			return err;
		}
	} else if (len == 6) {
		err = pclient->i2c_func_tbl->i2c_write_seq(pclient, index, &buff[2], 4);
		if (err < 0) {
			pr_err("%s:%d failed status=%d\n", __func__, __LINE__, err);
			return err;
		}

	}
	#else
	struct i2c_msg msg[1];

	msg[0].addr = pclient->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].buf = buff;
	msg[0].len = len;

	err = i2c_transfer(pclient->adapter, msg, 1); /* return the actual messages transfer */
	if (err != 1) {
		pr_err("%s: i2c_transfer err:%d, addr:0x%x, reg:0x%x\n", __func__, err, pclient->addr, (buff[0] << 8 | buff[1]));
		return -1;
	}
	#endif

	return 0;
}


/** int VL6180x_I2CRead(VL6180xDev_t dev, void *buff, uint8_t len);
 * @brief       Read data buffer from VL6180x device via i2c
 * @param dev   The device to read from
 * @param buff  The data buffer to fill
 * @param len   The length of the transaction in byte
 * @return      transaction status
 */
int VL6180x_I2CRead(VL6180xDev_t dev, uint8_t *buff, uint8_t len)
{

	int err = 0;
#ifdef CAMERA_CCI
	uint16_t index;
	index = ((uint16_t)buff[0] << 8) | (uint16_t)buff[1];
	/* pr_err("%s: index: %d\n", __func__, index); */
	err = pclient->i2c_func_tbl->i2c_read_seq(pclient, index, buff, len);
	if (err < 0) {
		pr_err("%s:%d failed status=%d\n", __func__, __LINE__, err);
		return err;
	}
#else
	struct i2c_msg msg[1];

	msg[0].addr = pclient->addr;
	msg[0].flags = I2C_M_RD|pclient->flags;
	msg[0].buf = buff;
	msg[0].len = len;

	err = i2c_transfer(pclient->adapter, &msg[0], 1); /* return the actual mesage transfer */
	if (err != 1) {
		pr_err("%s: Read i2c_transfer err:%d, addr:0x%x\n", __func__, err, pclient->addr);
		return -1;
	}
#endif

	return 0;
}
