/*******************************************************************************
Copyright ?2014, STMicroelectronics International N.V.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of STMicroelectronics nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
NON-INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS ARE DISCLAIMED.
IN NO EVENT SHALL STMICROELECTRONICS INTERNATIONAL N.V. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************************/
/*
 * vl6180x_port_i2c.c
 *
 *  Created on: Oct 22, 2014
 *      Author:  Teresa Tao
 */

#include "vl6180x_i2c.h"
#include <linux/i2c.h>

#define I2C_M_WR			0x00
static struct i2c_client *pclient=NULL;

void i2c_setclient(struct i2c_client *client)
{
	pclient = client;

}
struct i2c_client* i2c_getclient(void)
{
	return pclient;
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
	struct i2c_msg msg[1];
	int err=0;

	msg[0].addr = pclient->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].buf= buff;
	msg[0].len=len;

	err = i2c_transfer(pclient->adapter,msg,1); //return the actual messages transfer
	if(err != 1)
	{
		pr_err("%s: i2c_transfer err:%d, addr:0x%x, reg:0x%x\n", __func__, err, pclient->addr,
																				(buff[0]<<8|buff[1]));
		return -1;
	}
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
	struct i2c_msg msg[1];
	int err=0;

	msg[0].addr = pclient->addr;
	msg[0].flags = I2C_M_RD|pclient->flags;
	msg[0].buf= buff;
	msg[0].len=len;

	err = i2c_transfer(pclient->adapter,&msg[0],1); //return the actual mesage transfer
	if(err != 1)
	{
		pr_err("%s: Read i2c_transfer err:%d, addr:0x%x\n", __func__, err, pclient->addr);
		return -1;
	}
    return 0;
}
