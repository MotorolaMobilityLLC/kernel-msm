/* drivers/misc/ir2e71y/ir2e71y_main.h  (Display Driver)
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

#ifndef IR2E71Y_IR2E71Y_MAIN_H
#define IR2E71Y_IR2E71Y_MAIN_H

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define IR2E71Y_BDIC_CHIPVER_0                       (0)
#define IR2E71Y_BDIC_CHIPVER_1                       (1)

#define IR2E71Y_BDIC_VERSION71                       (0x08)
#define IR2E71Y_BDIC_GET_CHIPVER(version)            ((version >> 4) & 0x0F)
#define IR2E71Y_ALS_SENSOR_ADJUST_STATUS_COMPLETED   (0x90)
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)                               (sizeof(x) / sizeof((x)[0]))
#endif /* ARRAY_SIZE */
#define IR2E71Y_BDIC_REGSET(x)                       (ir2e71y_bdic_API_seq_regset(x, ARRAY_SIZE(x)))

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */
enum {
    IR2E71Y_BDIC_STR,
    IR2E71Y_BDIC_SET,
    IR2E71Y_BDIC_CLR,
    IR2E71Y_BDIC_RMW,
    IR2E71Y_BDIC_STRM,
    IR2E71Y_BDIC_BANK,
    IR2E71Y_BDIC_WAIT,
    IR2E71Y_ALS_STR,
    IR2E71Y_ALS_RMW,
    IR2E71Y_ALS_STRM,
    IR2E71Y_ALS_STRMS
};

typedef struct {
    unsigned char   addr;
    unsigned char   flg;
    unsigned char   data;
    unsigned char   mask;
    unsigned long   wait;
} ir2e71y_bdicRegSetting_t;

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
int  ir2e71y_bdic_API_boot_init(int *chipver);
int  ir2e71y_bdic_API_boot_init_2nd(void);
int ir2e71y_bdic_API_kerl_init(struct platform_device *pdev);
int  ir2e71y_bdic_API_shutdown(void);
int  ir2e71y_bdic_API_set_active(void);
int  ir2e71y_bdic_API_set_standby(void);
void ir2e71y_bdic_API_als_sensor_adjust(struct ir2e71y_photo_sensor_adj *adj);
void ir2e71y_bdic_API_check_sensor_param(struct ir2e71y_photo_sensor_adj *adj_in, \
                                        struct ir2e71y_photo_sensor_adj *adj_out);
int  ir2e71y_bdic_API_seq_regset(const ir2e71y_bdicRegSetting_t *regtable, int size);
int  ir2e71y_bdic_API_IO_write_reg(unsigned char reg, unsigned char val);
int  ir2e71y_bdic_API_IO_multi_write_reg(unsigned char reg, unsigned char *wval, unsigned char size);
int  ir2e71y_bdic_API_IO_read_reg(unsigned char reg, unsigned char *val);
int  ir2e71y_bdic_API_IO_multi_read_reg(unsigned char reg, unsigned char *val, int size);
int  ir2e71y_bdic_API_IO_set_bit_reg(unsigned char reg, unsigned char val);
int  ir2e71y_bdic_API_IO_clr_bit_reg(unsigned char reg, unsigned char val);
int  ir2e71y_bdic_API_IO_bank_set(unsigned char val);
int  ir2e71y_bdic_API_IO_msk_bit_reg(unsigned char reg, unsigned char val, unsigned char msk);
#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
int  ir2e71y_bdic_API_IO_i2c_transfer(struct ir2e71y_bdic_i2c_msg *msg);
int  ir2e71y_bdic_API_IO_psals_write_reg(unsigned char reg, unsigned char val);
int  ir2e71y_bdic_API_IO_psals_read_reg(unsigned char reg, unsigned char *val);
int  ir2e71y_bdic_API_IO_psals_msk_bit_reg(unsigned char reg, unsigned char val, unsigned char mask);
int  ir2e71y_bdic_API_IO_psals_burst_write_reg(unsigned char *wval, unsigned char dataNum);
int  ir2e71y_bdic_API_IO_psals_burst_read_reg(unsigned char reg, unsigned char *rval, unsigned char dataNum);
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */

#endif  /* IR2E71Y_IR2E71Y_MAIN_H */
/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
