/* drivers/misc/ir2e71y/ir2e71y_io.h  (Display Driver)
 *
 * Copyright (C) 2016 SHARP CORPORATION
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

#ifndef IR2E71Y_IO_H
#define IR2E71Y_IO_H
/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* DEBUG MACROS                                                              */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */

#define IR2E71Y_BDIC_I2C_DEVNAME             ("sharp,bdic_i2c")
#define IR2E71Y_SENSOR_DEVNAME               ("sharp,sensor_i2c")

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */
#define IR2E71Y_GPIO_CTL_LOW                 (0)
#define IR2E71Y_GPIO_CTL_HIGH                (1)

#define IR2E71Y_IRQ_MAX_KIND                 (4)
enum {
    IR2E71Y_IRQ_DISABLE,
    IR2E71Y_IRQ_ENABLE,
    NUM_IR2E71Y_IRQ_CMD
};

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
void ir2e71y_IO_API_delay_us(unsigned long usec);
void ir2e71y_IO_API_msleep(unsigned int msec);
void ir2e71y_IO_API_usleep(unsigned int usec);
void ir2e71y_IO_API_Host_gpio_init(void);
void ir2e71y_IO_API_Host_gpio_exit(void);
int  ir2e71y_IO_API_Host_gpio_request(int num, char *label);
int  ir2e71y_IO_API_Host_gpio_free(int num);
int  ir2e71y_IO_API_set_Host_gpio(int num, int value);
int  ir2e71y_IO_API_get_Host_gpio(int num);

int  ir2e71y_IO_API_bdic_i2c_init(void);
int  ir2e71y_IO_API_bdic_i2c_exit(void);
int  ir2e71y_IO_API_bdic_i2c_write(unsigned char addr, unsigned char data);
int  ir2e71y_IO_API_bdic_i2c_mask_write(unsigned char addr, unsigned char data, unsigned char mask);
int  ir2e71y_IO_API_bdic_i2c_multi_write(unsigned char addr, unsigned char *wval, unsigned char size);
int  ir2e71y_IO_API_bdic_i2c_read(unsigned char addr, unsigned char *data);
int  ir2e71y_IO_API_bdic_i2c_multi_read(unsigned char addr, unsigned char *data, int size);

int  ir2e71y_IO_API_sensor_i2c_init(void);
int  ir2e71y_IO_API_sensor_i2c_exit(void);

int  ir2e71y_IO_API_Host_i2c_send(unsigned char slaveaddr, unsigned char *sendval, unsigned char size);
int  ir2e71y_IO_API_Host_i2c_recv(unsigned char slaveaddr, unsigned char *sendval, unsigned char sendsize,
                                   unsigned char *recvval, unsigned char recvsize);

void ir2e71y_IO_API_set_irq_port(int irq_port, struct platform_device *pdev);
int ir2e71y_IO_API_request_irq(irqreturn_t (*irq_handler)(int , void *));
void ir2e71y_IO_API_free_irq(void);
void ir2e71y_IO_API_set_irq_init(void);
int ir2e71y_IO_API_set_irq(int enable);
#endif /* IR2E71Y_IO_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
