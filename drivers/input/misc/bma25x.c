/*!
 * @section LICENSE
 * (C) Copyright 2013 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bma25x.c
 * @date    2014/02/13 15:50
 * @id       "564eaab"
 * @version  2.0
 *
 * @brief
 * This file contains all function implementations for the BMA25X in linux
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/sensors.h>
#include <linux/kthread.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/string.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#endif

#include "bstclass.h"

#define ACC_NAME  "ACC"
/*#define BMA25X_ENABLE_INT1*/
#ifdef ENABLE_ISR_DEBUG_MSG
#define ISR_INFO(dev, fmt, arg...) dev_info(dev, fmt, ##arg)
#else
#define ISR_INFO(dev, fmt, arg...)
#endif

/*#define BMA25X_SENSOR_IDENTIFICATION_ENABLE*/

#define SENSOR_NAME                 "bma25x-accel"
#define ABSMIN                      -512
#define ABSMAX                      512
#define SLOPE_THRESHOLD_VALUE       32
#define SLOPE_DURATION_VALUE        1
#define INTERRUPT_LATCH_MODE        13
#define INTERRUPT_ENABLE            1
#define INTERRUPT_DISABLE           0
#define MAP_SLOPE_INTERRUPT         2
#define SLOPE_X_INDEX               5
#define SLOPE_Y_INDEX               6
#define SLOPE_Z_INDEX               7
#define BMA25X_RANGE_SET            3 /* +/- 2G */
#define BMA25X_RANGE_SHIFT          4 /* shift 4 bits for 2G */
#define BMA25X_BW_SET               12 /* 125HZ  */

#define I2C_RETRY_DELAY()           usleep_range(1000, 2000)
/* wait 2ms for calibration ready */
#define WAIT_CAL_READY()            usleep_range(2000, 2500)
/* >3ms wait device ready */
#define WAIT_DEVICE_READY()         usleep_range(3000, 5000)
/* >5ms for device reset */
#define RESET_DELAY()               usleep_range(5000, 10000)
/* wait 10ms for self test  done */
#define SELF_TEST_DELAY()           usleep_range(10000, 15000)

#define LOW_G_INTERRUPT             REL_Z
#define HIGH_G_INTERRUPT            REL_HWHEEL
#define SLOP_INTERRUPT              REL_DIAL
#define DOUBLE_TAP_INTERRUPT        REL_WHEEL
#define SINGLE_TAP_INTERRUPT        REL_MISC
#define ORIENT_INTERRUPT            ABS_PRESSURE
#define FLAT_INTERRUPT              ABS_DISTANCE
#define SLOW_NO_MOTION_INTERRUPT    REL_Y

#define HIGH_G_INTERRUPT_X_HAPPENED                 1
#define HIGH_G_INTERRUPT_Y_HAPPENED                 2
#define HIGH_G_INTERRUPT_Z_HAPPENED                 3
#define HIGH_G_INTERRUPT_X_NEGATIVE_HAPPENED        4
#define HIGH_G_INTERRUPT_Y_NEGATIVE_HAPPENED        5
#define HIGH_G_INTERRUPT_Z_NEGATIVE_HAPPENED        6
#define SLOPE_INTERRUPT_X_HAPPENED                  7
#define SLOPE_INTERRUPT_Y_HAPPENED                  8
#define SLOPE_INTERRUPT_Z_HAPPENED                  9
#define SLOPE_INTERRUPT_X_NEGATIVE_HAPPENED         10
#define SLOPE_INTERRUPT_Y_NEGATIVE_HAPPENED         11
#define SLOPE_INTERRUPT_Z_NEGATIVE_HAPPENED         12
#define DOUBLE_TAP_INTERRUPT_HAPPENED               13
#define SINGLE_TAP_INTERRUPT_HAPPENED               14
#define UPWARD_PORTRAIT_UP_INTERRUPT_HAPPENED       15
#define UPWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED     16
#define UPWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED    17
#define UPWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED   18
#define DOWNWARD_PORTRAIT_UP_INTERRUPT_HAPPENED     19
#define DOWNWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED   20
#define DOWNWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED  21
#define DOWNWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED 22
#define FLAT_INTERRUPT_TURE_HAPPENED                23
#define FLAT_INTERRUPT_FALSE_HAPPENED               24
#define LOW_G_INTERRUPT_HAPPENED                    25
#define SLOW_NO_MOTION_INTERRUPT_HAPPENED           26

#define PAD_LOWG                    0
#define PAD_HIGHG                   1
#define PAD_SLOP                    2
#define PAD_DOUBLE_TAP              3
#define PAD_SINGLE_TAP              4
#define PAD_ORIENT                  5
#define PAD_FLAT                    6
#define PAD_SLOW_NO_MOTION          7

#define BMA25X_EEP_OFFSET                       0x16
#define BMA25X_IMAGE_BASE                       0x38
#define BMA25X_IMAGE_LEN                        22

#define BMA25X_CHIP_ID_REG                      0x00
#define BMA25X_VERSION_REG                      0x01
#define BMA25X_X_AXIS_LSB_REG                   0x02
#define BMA25X_X_AXIS_MSB_REG                   0x03
#define BMA25X_Y_AXIS_LSB_REG                   0x04
#define BMA25X_Y_AXIS_MSB_REG                   0x05
#define BMA25X_Z_AXIS_LSB_REG                   0x06
#define BMA25X_Z_AXIS_MSB_REG                   0x07
#define BMA25X_TEMPERATURE_REG                  0x08
#define BMA25X_STATUS1_REG                      0x09
#define BMA25X_STATUS2_REG                      0x0A
#define BMA25X_STATUS_TAP_SLOPE_REG             0x0B
#define BMA25X_STATUS_ORIENT_HIGH_REG           0x0C
#define BMA25X_STATUS_FIFO_REG                  0x0E
#define BMA25X_RANGE_SEL_REG                    0x0F
#define BMA25X_BW_SEL_REG                       0x10
#define BMA25X_MODE_CTRL_REG                    0x11
#define BMA25X_LOW_NOISE_CTRL_REG               0x12
#define BMA25X_DATA_CTRL_REG                    0x13
#define BMA25X_RESET_REG                        0x14
#define BMA25X_INT_ENABLE1_REG                  0x16
#define BMA25X_INT_ENABLE2_REG                  0x17
#define BMA25X_INT_SLO_NO_MOT_REG               0x18
#define BMA25X_INT1_PAD_SEL_REG                 0x19
#define BMA25X_INT_DATA_SEL_REG                 0x1A
#define BMA25X_INT2_PAD_SEL_REG                 0x1B
#define BMA25X_INT_SRC_REG                      0x1E
#define BMA25X_INT_SET_REG                      0x20
#define BMA25X_INT_CTRL_REG                     0x21
#define BMA25X_LOW_DURN_REG                     0x22
#define BMA25X_LOW_THRES_REG                    0x23
#define BMA25X_LOW_HIGH_HYST_REG                0x24
#define BMA25X_HIGH_DURN_REG                    0x25
#define BMA25X_HIGH_THRES_REG                   0x26
#define BMA25X_SLOPE_DURN_REG                   0x27
#define BMA25X_SLOPE_THRES_REG                  0x28
#define BMA25X_SLO_NO_MOT_THRES_REG             0x29
#define BMA25X_TAP_PARAM_REG                    0x2A
#define BMA25X_TAP_THRES_REG                    0x2B
#define BMA25X_ORIENT_PARAM_REG                 0x2C
#define BMA25X_THETA_BLOCK_REG                  0x2D
#define BMA25X_THETA_FLAT_REG                   0x2E
#define BMA25X_FLAT_HOLD_TIME_REG               0x2F
#define BMA25X_FIFO_WML_TRIG                    0x30
#define BMA25X_SELF_TEST_REG                    0x32
#define BMA25X_EEPROM_CTRL_REG                  0x33
#define BMA25X_SERIAL_CTRL_REG                  0x34
#define BMA25X_EXTMODE_CTRL_REG                 0x35
#define BMA25X_OFFSET_CTRL_REG                  0x36
#define BMA25X_OFFSET_PARAMS_REG                0x37
#define BMA25X_OFFSET_X_AXIS_REG                0x38
#define BMA25X_OFFSET_Y_AXIS_REG                0x39
#define BMA25X_OFFSET_Z_AXIS_REG                0x3A
#define BMA25X_GP0_REG                          0x3B
#define BMA25X_GP1_REG                          0x3C
#define BMA25X_FIFO_MODE_REG                    0x3E
#define BMA25X_FIFO_DATA_OUTPUT_REG             0x3F

#define BMA25X_CHIP_ID__POS             0
#define BMA25X_CHIP_ID__MSK             0xFF
#define BMA25X_CHIP_ID__LEN             8
#define BMA25X_CHIP_ID__REG             BMA25X_CHIP_ID_REG

#define BMA25X_VERSION__POS          0
#define BMA25X_VERSION__LEN          8
#define BMA25X_VERSION__MSK          0xFF
#define BMA25X_VERSION__REG          BMA25X_VERSION_REG

#define BMA2x2_SLO_NO_MOT_DUR__POS   2
#define BMA2x2_SLO_NO_MOT_DUR__LEN   6
#define BMA2x2_SLO_NO_MOT_DUR__MSK   0xFC
#define BMA2x2_SLO_NO_MOT_DUR__REG   BMA25X_SLOPE_DURN_REG

#define BMA25X_NEW_DATA_X__POS          0
#define BMA25X_NEW_DATA_X__LEN          1
#define BMA25X_NEW_DATA_X__MSK          0x01
#define BMA25X_NEW_DATA_X__REG          BMA25X_X_AXIS_LSB_REG

#define BMA25X_ACC_X14_LSB__POS           2
#define BMA25X_ACC_X14_LSB__LEN           6
#define BMA25X_ACC_X14_LSB__MSK           0xFC
#define BMA25X_ACC_X14_LSB__REG           BMA25X_X_AXIS_LSB_REG

#define BMA25X_ACC_X12_LSB__POS           4
#define BMA25X_ACC_X12_LSB__LEN           4
#define BMA25X_ACC_X12_LSB__MSK           0xF0
#define BMA25X_ACC_X12_LSB__REG           BMA25X_X_AXIS_LSB_REG

#define BMA25X_ACC_X10_LSB__POS           6
#define BMA25X_ACC_X10_LSB__LEN           2
#define BMA25X_ACC_X10_LSB__MSK           0xC0
#define BMA25X_ACC_X10_LSB__REG           BMA25X_X_AXIS_LSB_REG

#define BMA25X_ACC_X8_LSB__POS           0
#define BMA25X_ACC_X8_LSB__LEN           0
#define BMA25X_ACC_X8_LSB__MSK           0x00
#define BMA25X_ACC_X8_LSB__REG           BMA25X_X_AXIS_LSB_REG

#define BMA25X_ACC_X_MSB__POS           0
#define BMA25X_ACC_X_MSB__LEN           8
#define BMA25X_ACC_X_MSB__MSK           0xFF
#define BMA25X_ACC_X_MSB__REG           BMA25X_X_AXIS_MSB_REG

#define BMA25X_NEW_DATA_Y__POS          0
#define BMA25X_NEW_DATA_Y__LEN          1
#define BMA25X_NEW_DATA_Y__MSK          0x01
#define BMA25X_NEW_DATA_Y__REG          BMA25X_Y_AXIS_LSB_REG

#define BMA25X_ACC_Y14_LSB__POS           2
#define BMA25X_ACC_Y14_LSB__LEN           6
#define BMA25X_ACC_Y14_LSB__MSK           0xFC
#define BMA25X_ACC_Y14_LSB__REG           BMA25X_Y_AXIS_LSB_REG

#define BMA25X_ACC_Y12_LSB__POS           4
#define BMA25X_ACC_Y12_LSB__LEN           4
#define BMA25X_ACC_Y12_LSB__MSK           0xF0
#define BMA25X_ACC_Y12_LSB__REG           BMA25X_Y_AXIS_LSB_REG

#define BMA25X_ACC_Y10_LSB__POS           6
#define BMA25X_ACC_Y10_LSB__LEN           2
#define BMA25X_ACC_Y10_LSB__MSK           0xC0
#define BMA25X_ACC_Y10_LSB__REG           BMA25X_Y_AXIS_LSB_REG

#define BMA25X_ACC_Y8_LSB__POS           0
#define BMA25X_ACC_Y8_LSB__LEN           0
#define BMA25X_ACC_Y8_LSB__MSK           0x00
#define BMA25X_ACC_Y8_LSB__REG           BMA25X_Y_AXIS_LSB_REG

#define BMA25X_ACC_Y_MSB__POS           0
#define BMA25X_ACC_Y_MSB__LEN           8
#define BMA25X_ACC_Y_MSB__MSK           0xFF
#define BMA25X_ACC_Y_MSB__REG           BMA25X_Y_AXIS_MSB_REG

#define BMA25X_NEW_DATA_Z__POS          0
#define BMA25X_NEW_DATA_Z__LEN          1
#define BMA25X_NEW_DATA_Z__MSK          0x01
#define BMA25X_NEW_DATA_Z__REG          BMA25X_Z_AXIS_LSB_REG

#define BMA25X_ACC_Z14_LSB__POS           2
#define BMA25X_ACC_Z14_LSB__LEN           6
#define BMA25X_ACC_Z14_LSB__MSK           0xFC
#define BMA25X_ACC_Z14_LSB__REG           BMA25X_Z_AXIS_LSB_REG

#define BMA25X_ACC_Z12_LSB__POS           4
#define BMA25X_ACC_Z12_LSB__LEN           4
#define BMA25X_ACC_Z12_LSB__MSK           0xF0
#define BMA25X_ACC_Z12_LSB__REG           BMA25X_Z_AXIS_LSB_REG

#define BMA25X_ACC_Z10_LSB__POS           6
#define BMA25X_ACC_Z10_LSB__LEN           2
#define BMA25X_ACC_Z10_LSB__MSK           0xC0
#define BMA25X_ACC_Z10_LSB__REG           BMA25X_Z_AXIS_LSB_REG

#define BMA25X_ACC_Z8_LSB__POS           0
#define BMA25X_ACC_Z8_LSB__LEN           0
#define BMA25X_ACC_Z8_LSB__MSK           0x00
#define BMA25X_ACC_Z8_LSB__REG           BMA25X_Z_AXIS_LSB_REG

#define BMA25X_ACC_Z_MSB__POS           0
#define BMA25X_ACC_Z_MSB__LEN           8
#define BMA25X_ACC_Z_MSB__MSK           0xFF
#define BMA25X_ACC_Z_MSB__REG           BMA25X_Z_AXIS_MSB_REG

#define BMA25X_TEMPERATURE__POS         0
#define BMA25X_TEMPERATURE__LEN         8
#define BMA25X_TEMPERATURE__MSK         0xFF
#define BMA25X_TEMPERATURE__REG         BMA25X_TEMP_RD_REG

#define BMA25X_LOWG_INT_S__POS          0
#define BMA25X_LOWG_INT_S__LEN          1
#define BMA25X_LOWG_INT_S__MSK          0x01
#define BMA25X_LOWG_INT_S__REG          BMA25X_STATUS1_REG

#define BMA25X_HIGHG_INT_S__POS          1
#define BMA25X_HIGHG_INT_S__LEN          1
#define BMA25X_HIGHG_INT_S__MSK          0x02
#define BMA25X_HIGHG_INT_S__REG          BMA25X_STATUS1_REG

#define BMA25X_SLOPE_INT_S__POS          2
#define BMA25X_SLOPE_INT_S__LEN          1
#define BMA25X_SLOPE_INT_S__MSK          0x04
#define BMA25X_SLOPE_INT_S__REG          BMA25X_STATUS1_REG


#define BMA25X_SLO_NO_MOT_INT_S__POS          3
#define BMA25X_SLO_NO_MOT_INT_S__LEN          1
#define BMA25X_SLO_NO_MOT_INT_S__MSK          0x08
#define BMA25X_SLO_NO_MOT_INT_S__REG          BMA25X_STATUS1_REG

#define BMA25X_DOUBLE_TAP_INT_S__POS     4
#define BMA25X_DOUBLE_TAP_INT_S__LEN     1
#define BMA25X_DOUBLE_TAP_INT_S__MSK     0x10
#define BMA25X_DOUBLE_TAP_INT_S__REG     BMA25X_STATUS1_REG

#define BMA25X_SINGLE_TAP_INT_S__POS     5
#define BMA25X_SINGLE_TAP_INT_S__LEN     1
#define BMA25X_SINGLE_TAP_INT_S__MSK     0x20
#define BMA25X_SINGLE_TAP_INT_S__REG     BMA25X_STATUS1_REG

#define BMA25X_ORIENT_INT_S__POS         6
#define BMA25X_ORIENT_INT_S__LEN         1
#define BMA25X_ORIENT_INT_S__MSK         0x40
#define BMA25X_ORIENT_INT_S__REG         BMA25X_STATUS1_REG

#define BMA25X_FLAT_INT_S__POS           7
#define BMA25X_FLAT_INT_S__LEN           1
#define BMA25X_FLAT_INT_S__MSK           0x80
#define BMA25X_FLAT_INT_S__REG           BMA25X_STATUS1_REG

#define BMA25X_FIFO_FULL_INT_S__POS           5
#define BMA25X_FIFO_FULL_INT_S__LEN           1
#define BMA25X_FIFO_FULL_INT_S__MSK           0x20
#define BMA25X_FIFO_FULL_INT_S__REG           BMA25X_STATUS2_REG

#define BMA25X_FIFO_WM_INT_S__POS           6
#define BMA25X_FIFO_WM_INT_S__LEN           1
#define BMA25X_FIFO_WM_INT_S__MSK           0x40
#define BMA25X_FIFO_WM_INT_S__REG           BMA25X_STATUS2_REG

#define BMA25X_DATA_INT_S__POS           7
#define BMA25X_DATA_INT_S__LEN           1
#define BMA25X_DATA_INT_S__MSK           0x80
#define BMA25X_DATA_INT_S__REG           BMA25X_STATUS2_REG

#define BMA25X_SLOPE_FIRST_X__POS        0
#define BMA25X_SLOPE_FIRST_X__LEN        1
#define BMA25X_SLOPE_FIRST_X__MSK        0x01
#define BMA25X_SLOPE_FIRST_X__REG        BMA25X_STATUS_TAP_SLOPE_REG

#define BMA25X_SLOPE_FIRST_Y__POS        1
#define BMA25X_SLOPE_FIRST_Y__LEN        1
#define BMA25X_SLOPE_FIRST_Y__MSK        0x02
#define BMA25X_SLOPE_FIRST_Y__REG        BMA25X_STATUS_TAP_SLOPE_REG

#define BMA25X_SLOPE_FIRST_Z__POS        2
#define BMA25X_SLOPE_FIRST_Z__LEN        1
#define BMA25X_SLOPE_FIRST_Z__MSK        0x04
#define BMA25X_SLOPE_FIRST_Z__REG        BMA25X_STATUS_TAP_SLOPE_REG

#define BMA25X_SLOPE_SIGN_S__POS         3
#define BMA25X_SLOPE_SIGN_S__LEN         1
#define BMA25X_SLOPE_SIGN_S__MSK         0x08
#define BMA25X_SLOPE_SIGN_S__REG         BMA25X_STATUS_TAP_SLOPE_REG

#define BMA25X_TAP_FIRST_X__POS        4
#define BMA25X_TAP_FIRST_X__LEN        1
#define BMA25X_TAP_FIRST_X__MSK        0x10
#define BMA25X_TAP_FIRST_X__REG        BMA25X_STATUS_TAP_SLOPE_REG

#define BMA25X_TAP_FIRST_Y__POS        5
#define BMA25X_TAP_FIRST_Y__LEN        1
#define BMA25X_TAP_FIRST_Y__MSK        0x20
#define BMA25X_TAP_FIRST_Y__REG        BMA25X_STATUS_TAP_SLOPE_REG

#define BMA25X_TAP_FIRST_Z__POS        6
#define BMA25X_TAP_FIRST_Z__LEN        1
#define BMA25X_TAP_FIRST_Z__MSK        0x40
#define BMA25X_TAP_FIRST_Z__REG        BMA25X_STATUS_TAP_SLOPE_REG

#define BMA25X_TAP_SIGN_S__POS         7
#define BMA25X_TAP_SIGN_S__LEN         1
#define BMA25X_TAP_SIGN_S__MSK         0x80
#define BMA25X_TAP_SIGN_S__REG         BMA25X_STATUS_TAP_SLOPE_REG

#define BMA25X_HIGHG_FIRST_X__POS        0
#define BMA25X_HIGHG_FIRST_X__LEN        1
#define BMA25X_HIGHG_FIRST_X__MSK        0x01
#define BMA25X_HIGHG_FIRST_X__REG        BMA25X_STATUS_ORIENT_HIGH_REG

#define BMA25X_HIGHG_FIRST_Y__POS        1
#define BMA25X_HIGHG_FIRST_Y__LEN        1
#define BMA25X_HIGHG_FIRST_Y__MSK        0x02
#define BMA25X_HIGHG_FIRST_Y__REG        BMA25X_STATUS_ORIENT_HIGH_REG

#define BMA25X_HIGHG_FIRST_Z__POS        2
#define BMA25X_HIGHG_FIRST_Z__LEN        1
#define BMA25X_HIGHG_FIRST_Z__MSK        0x04
#define BMA25X_HIGHG_FIRST_Z__REG        BMA25X_STATUS_ORIENT_HIGH_REG

#define BMA25X_HIGHG_SIGN_S__POS         3
#define BMA25X_HIGHG_SIGN_S__LEN         1
#define BMA25X_HIGHG_SIGN_S__MSK         0x08
#define BMA25X_HIGHG_SIGN_S__REG         BMA25X_STATUS_ORIENT_HIGH_REG

#define BMA25X_ORIENT_S__POS             4
#define BMA25X_ORIENT_S__LEN             3
#define BMA25X_ORIENT_S__MSK             0x70
#define BMA25X_ORIENT_S__REG             BMA25X_STATUS_ORIENT_HIGH_REG

#define BMA25X_FLAT_S__POS               7
#define BMA25X_FLAT_S__LEN               1
#define BMA25X_FLAT_S__MSK               0x80
#define BMA25X_FLAT_S__REG               BMA25X_STATUS_ORIENT_HIGH_REG

#define BMA25X_FIFO_FRAME_COUNTER_S__POS             0
#define BMA25X_FIFO_FRAME_COUNTER_S__LEN             7
#define BMA25X_FIFO_FRAME_COUNTER_S__MSK             0x7F
#define BMA25X_FIFO_FRAME_COUNTER_S__REG             BMA25X_STATUS_FIFO_REG

#define BMA25X_FIFO_OVERRUN_S__POS             7
#define BMA25X_FIFO_OVERRUN_S__LEN             1
#define BMA25X_FIFO_OVERRUN_S__MSK             0x80
#define BMA25X_FIFO_OVERRUN_S__REG             BMA25X_STATUS_FIFO_REG

#define BMA25X_RANGE_SEL__POS             0
#define BMA25X_RANGE_SEL__LEN             4
#define BMA25X_RANGE_SEL__MSK             0x0F
#define BMA25X_RANGE_SEL__REG             BMA25X_RANGE_SEL_REG

#define BMA25X_BANDWIDTH__POS             0
#define BMA25X_BANDWIDTH__LEN             5
#define BMA25X_BANDWIDTH__MSK             0x1F
#define BMA25X_BANDWIDTH__REG             BMA25X_BW_SEL_REG

#define BMA25X_SLEEP_DUR__POS             1
#define BMA25X_SLEEP_DUR__LEN             4
#define BMA25X_SLEEP_DUR__MSK             0x1E
#define BMA25X_SLEEP_DUR__REG             BMA25X_MODE_CTRL_REG

#define BMA25X_MODE_CTRL__POS             5
#define BMA25X_MODE_CTRL__LEN             3
#define BMA25X_MODE_CTRL__MSK             0xE0
#define BMA25X_MODE_CTRL__REG             BMA25X_MODE_CTRL_REG

#define BMA25X_DEEP_SUSPEND__POS          5
#define BMA25X_DEEP_SUSPEND__LEN          1
#define BMA25X_DEEP_SUSPEND__MSK          0x20
#define BMA25X_DEEP_SUSPEND__REG          BMA25X_MODE_CTRL_REG

#define BMA25X_EN_LOW_POWER__POS          6
#define BMA25X_EN_LOW_POWER__LEN          1
#define BMA25X_EN_LOW_POWER__MSK          0x40
#define BMA25X_EN_LOW_POWER__REG          BMA25X_MODE_CTRL_REG

#define BMA25X_EN_SUSPEND__POS            7
#define BMA25X_EN_SUSPEND__LEN            1
#define BMA25X_EN_SUSPEND__MSK            0x80
#define BMA25X_EN_SUSPEND__REG            BMA25X_MODE_CTRL_REG

#define BMA25X_SLEEP_TIMER__POS          5
#define BMA25X_SLEEP_TIMER__LEN          1
#define BMA25X_SLEEP_TIMER__MSK          0x20
#define BMA25X_SLEEP_TIMER__REG          BMA25X_LOW_NOISE_CTRL_REG

#define BMA25X_LOW_POWER_MODE__POS          6
#define BMA25X_LOW_POWER_MODE__LEN          1
#define BMA25X_LOW_POWER_MODE__MSK          0x40
#define BMA25X_LOW_POWER_MODE__REG          BMA25X_LOW_NOISE_CTRL_REG

#define BMA25X_EN_LOW_NOISE__POS          7
#define BMA25X_EN_LOW_NOISE__LEN          1
#define BMA25X_EN_LOW_NOISE__MSK          0x80
#define BMA25X_EN_LOW_NOISE__REG          BMA25X_LOW_NOISE_CTRL_REG

#define BMA25X_DIS_SHADOW_PROC__POS       6
#define BMA25X_DIS_SHADOW_PROC__LEN       1
#define BMA25X_DIS_SHADOW_PROC__MSK       0x40
#define BMA25X_DIS_SHADOW_PROC__REG       BMA25X_DATA_CTRL_REG

#define BMA25X_EN_DATA_HIGH_BW__POS         7
#define BMA25X_EN_DATA_HIGH_BW__LEN         1
#define BMA25X_EN_DATA_HIGH_BW__MSK         0x80
#define BMA25X_EN_DATA_HIGH_BW__REG         BMA25X_DATA_CTRL_REG

#define BMA25X_EN_SOFT_RESET__POS         0
#define BMA25X_EN_SOFT_RESET__LEN         8
#define BMA25X_EN_SOFT_RESET__MSK         0xFF
#define BMA25X_EN_SOFT_RESET__REG         BMA25X_RESET_REG

#define BMA25X_EN_SOFT_RESET_VALUE        0xB6

#define BMA25X_EN_SLOPE_X_INT__POS         0
#define BMA25X_EN_SLOPE_X_INT__LEN         1
#define BMA25X_EN_SLOPE_X_INT__MSK         0x01
#define BMA25X_EN_SLOPE_X_INT__REG         BMA25X_INT_ENABLE1_REG

#define BMA25X_EN_SLOPE_Y_INT__POS         1
#define BMA25X_EN_SLOPE_Y_INT__LEN         1
#define BMA25X_EN_SLOPE_Y_INT__MSK         0x02
#define BMA25X_EN_SLOPE_Y_INT__REG         BMA25X_INT_ENABLE1_REG

#define BMA25X_EN_SLOPE_Z_INT__POS         2
#define BMA25X_EN_SLOPE_Z_INT__LEN         1
#define BMA25X_EN_SLOPE_Z_INT__MSK         0x04
#define BMA25X_EN_SLOPE_Z_INT__REG         BMA25X_INT_ENABLE1_REG

#define BMA25X_EN_DOUBLE_TAP_INT__POS      4
#define BMA25X_EN_DOUBLE_TAP_INT__LEN      1
#define BMA25X_EN_DOUBLE_TAP_INT__MSK      0x10
#define BMA25X_EN_DOUBLE_TAP_INT__REG      BMA25X_INT_ENABLE1_REG

#define BMA25X_EN_SINGLE_TAP_INT__POS      5
#define BMA25X_EN_SINGLE_TAP_INT__LEN      1
#define BMA25X_EN_SINGLE_TAP_INT__MSK      0x20
#define BMA25X_EN_SINGLE_TAP_INT__REG      BMA25X_INT_ENABLE1_REG

#define BMA25X_EN_ORIENT_INT__POS          6
#define BMA25X_EN_ORIENT_INT__LEN          1
#define BMA25X_EN_ORIENT_INT__MSK          0x40
#define BMA25X_EN_ORIENT_INT__REG          BMA25X_INT_ENABLE1_REG

#define BMA25X_EN_FLAT_INT__POS            7
#define BMA25X_EN_FLAT_INT__LEN            1
#define BMA25X_EN_FLAT_INT__MSK            0x80
#define BMA25X_EN_FLAT_INT__REG            BMA25X_INT_ENABLE1_REG

#define BMA25X_EN_HIGHG_X_INT__POS         0
#define BMA25X_EN_HIGHG_X_INT__LEN         1
#define BMA25X_EN_HIGHG_X_INT__MSK         0x01
#define BMA25X_EN_HIGHG_X_INT__REG         BMA25X_INT_ENABLE2_REG

#define BMA25X_EN_HIGHG_Y_INT__POS         1
#define BMA25X_EN_HIGHG_Y_INT__LEN         1
#define BMA25X_EN_HIGHG_Y_INT__MSK         0x02
#define BMA25X_EN_HIGHG_Y_INT__REG         BMA25X_INT_ENABLE2_REG

#define BMA25X_EN_HIGHG_Z_INT__POS         2
#define BMA25X_EN_HIGHG_Z_INT__LEN         1
#define BMA25X_EN_HIGHG_Z_INT__MSK         0x04
#define BMA25X_EN_HIGHG_Z_INT__REG         BMA25X_INT_ENABLE2_REG

#define BMA25X_EN_LOWG_INT__POS            3
#define BMA25X_EN_LOWG_INT__LEN            1
#define BMA25X_EN_LOWG_INT__MSK            0x08
#define BMA25X_EN_LOWG_INT__REG            BMA25X_INT_ENABLE2_REG

#define BMA25X_EN_NEW_DATA_INT__POS        4
#define BMA25X_EN_NEW_DATA_INT__LEN        1
#define BMA25X_EN_NEW_DATA_INT__MSK        0x10
#define BMA25X_EN_NEW_DATA_INT__REG        BMA25X_INT_ENABLE2_REG

#define BMA25X_INT_FFULL_EN_INT__POS        5
#define BMA25X_INT_FFULL_EN_INT__LEN        1
#define BMA25X_INT_FFULL_EN_INT__MSK        0x20
#define BMA25X_INT_FFULL_EN_INT__REG        BMA25X_INT_ENABLE2_REG

#define BMA25X_INT_FWM_EN_INT__POS        6
#define BMA25X_INT_FWM_EN_INT__LEN        1
#define BMA25X_INT_FWM_EN_INT__MSK        0x40
#define BMA25X_INT_FWM_EN_INT__REG        BMA25X_INT_ENABLE2_REG

#define BMA25X_INT_SLO_NO_MOT_EN_X_INT__POS        0
#define BMA25X_INT_SLO_NO_MOT_EN_X_INT__LEN        1
#define BMA25X_INT_SLO_NO_MOT_EN_X_INT__MSK        0x01
#define BMA25X_INT_SLO_NO_MOT_EN_X_INT__REG        BMA25X_INT_SLO_NO_MOT_REG

#define BMA25X_INT_SLO_NO_MOT_EN_Y_INT__POS        1
#define BMA25X_INT_SLO_NO_MOT_EN_Y_INT__LEN        1
#define BMA25X_INT_SLO_NO_MOT_EN_Y_INT__MSK        0x02
#define BMA25X_INT_SLO_NO_MOT_EN_Y_INT__REG        BMA25X_INT_SLO_NO_MOT_REG

#define BMA25X_INT_SLO_NO_MOT_EN_Z_INT__POS        2
#define BMA25X_INT_SLO_NO_MOT_EN_Z_INT__LEN        1
#define BMA25X_INT_SLO_NO_MOT_EN_Z_INT__MSK        0x04
#define BMA25X_INT_SLO_NO_MOT_EN_Z_INT__REG        BMA25X_INT_SLO_NO_MOT_REG

#define BMA25X_INT_SLO_NO_MOT_EN_SEL_INT__POS        3
#define BMA25X_INT_SLO_NO_MOT_EN_SEL_INT__LEN        1
#define BMA25X_INT_SLO_NO_MOT_EN_SEL_INT__MSK        0x08
#define BMA25X_INT_SLO_NO_MOT_EN_SEL_INT__REG        BMA25X_INT_SLO_NO_MOT_REG

#define BMA25X_EN_INT1_PAD_LOWG__POS        0
#define BMA25X_EN_INT1_PAD_LOWG__LEN        1
#define BMA25X_EN_INT1_PAD_LOWG__MSK        0x01
#define BMA25X_EN_INT1_PAD_LOWG__REG        BMA25X_INT1_PAD_SEL_REG

#define BMA25X_EN_INT1_PAD_HIGHG__POS       1
#define BMA25X_EN_INT1_PAD_HIGHG__LEN       1
#define BMA25X_EN_INT1_PAD_HIGHG__MSK       0x02
#define BMA25X_EN_INT1_PAD_HIGHG__REG       BMA25X_INT1_PAD_SEL_REG

#define BMA25X_EN_INT1_PAD_SLOPE__POS       2
#define BMA25X_EN_INT1_PAD_SLOPE__LEN       1
#define BMA25X_EN_INT1_PAD_SLOPE__MSK       0x04
#define BMA25X_EN_INT1_PAD_SLOPE__REG       BMA25X_INT1_PAD_SEL_REG

#define BMA25X_EN_INT1_PAD_SLO_NO_MOT__POS        3
#define BMA25X_EN_INT1_PAD_SLO_NO_MOT__LEN        1
#define BMA25X_EN_INT1_PAD_SLO_NO_MOT__MSK        0x08
#define BMA25X_EN_INT1_PAD_SLO_NO_MOT__REG        BMA25X_INT1_PAD_SEL_REG

#define BMA25X_EN_INT1_PAD_DB_TAP__POS      4
#define BMA25X_EN_INT1_PAD_DB_TAP__LEN      1
#define BMA25X_EN_INT1_PAD_DB_TAP__MSK      0x10
#define BMA25X_EN_INT1_PAD_DB_TAP__REG      BMA25X_INT1_PAD_SEL_REG

#define BMA25X_EN_INT1_PAD_SNG_TAP__POS     5
#define BMA25X_EN_INT1_PAD_SNG_TAP__LEN     1
#define BMA25X_EN_INT1_PAD_SNG_TAP__MSK     0x20
#define BMA25X_EN_INT1_PAD_SNG_TAP__REG     BMA25X_INT1_PAD_SEL_REG

#define BMA25X_EN_INT1_PAD_ORIENT__POS      6
#define BMA25X_EN_INT1_PAD_ORIENT__LEN      1
#define BMA25X_EN_INT1_PAD_ORIENT__MSK      0x40
#define BMA25X_EN_INT1_PAD_ORIENT__REG      BMA25X_INT1_PAD_SEL_REG

#define BMA25X_EN_INT1_PAD_FLAT__POS        7
#define BMA25X_EN_INT1_PAD_FLAT__LEN        1
#define BMA25X_EN_INT1_PAD_FLAT__MSK        0x80
#define BMA25X_EN_INT1_PAD_FLAT__REG        BMA25X_INT1_PAD_SEL_REG

#define BMA25X_EN_INT2_PAD_LOWG__POS        0
#define BMA25X_EN_INT2_PAD_LOWG__LEN        1
#define BMA25X_EN_INT2_PAD_LOWG__MSK        0x01
#define BMA25X_EN_INT2_PAD_LOWG__REG        BMA25X_INT2_PAD_SEL_REG

#define BMA25X_EN_INT2_PAD_HIGHG__POS       1
#define BMA25X_EN_INT2_PAD_HIGHG__LEN       1
#define BMA25X_EN_INT2_PAD_HIGHG__MSK       0x02
#define BMA25X_EN_INT2_PAD_HIGHG__REG       BMA25X_INT2_PAD_SEL_REG

#define BMA25X_EN_INT2_PAD_SLOPE__POS       2
#define BMA25X_EN_INT2_PAD_SLOPE__LEN       1
#define BMA25X_EN_INT2_PAD_SLOPE__MSK       0x04
#define BMA25X_EN_INT2_PAD_SLOPE__REG       BMA25X_INT2_PAD_SEL_REG

#define BMA25X_EN_INT2_PAD_SLO_NO_MOT__POS        3
#define BMA25X_EN_INT2_PAD_SLO_NO_MOT__LEN        1
#define BMA25X_EN_INT2_PAD_SLO_NO_MOT__MSK        0x08
#define BMA25X_EN_INT2_PAD_SLO_NO_MOT__REG        BMA25X_INT2_PAD_SEL_REG

#define BMA25X_EN_INT2_PAD_DB_TAP__POS      4
#define BMA25X_EN_INT2_PAD_DB_TAP__LEN      1
#define BMA25X_EN_INT2_PAD_DB_TAP__MSK      0x10
#define BMA25X_EN_INT2_PAD_DB_TAP__REG      BMA25X_INT2_PAD_SEL_REG

#define BMA25X_EN_INT2_PAD_SNG_TAP__POS     5
#define BMA25X_EN_INT2_PAD_SNG_TAP__LEN     1
#define BMA25X_EN_INT2_PAD_SNG_TAP__MSK     0x20
#define BMA25X_EN_INT2_PAD_SNG_TAP__REG     BMA25X_INT2_PAD_SEL_REG

#define BMA25X_EN_INT2_PAD_ORIENT__POS      6
#define BMA25X_EN_INT2_PAD_ORIENT__LEN      1
#define BMA25X_EN_INT2_PAD_ORIENT__MSK      0x40
#define BMA25X_EN_INT2_PAD_ORIENT__REG      BMA25X_INT2_PAD_SEL_REG

#define BMA25X_EN_INT2_PAD_FLAT__POS        7
#define BMA25X_EN_INT2_PAD_FLAT__LEN        1
#define BMA25X_EN_INT2_PAD_FLAT__MSK        0x80
#define BMA25X_EN_INT2_PAD_FLAT__REG        BMA25X_INT2_PAD_SEL_REG

#define BMA25X_EN_INT1_PAD_NEWDATA__POS     0
#define BMA25X_EN_INT1_PAD_NEWDATA__LEN     1
#define BMA25X_EN_INT1_PAD_NEWDATA__MSK     0x01
#define BMA25X_EN_INT1_PAD_NEWDATA__REG     BMA25X_INT_DATA_SEL_REG

#define BMA25X_EN_INT1_PAD_FWM__POS     1
#define BMA25X_EN_INT1_PAD_FWM__LEN     1
#define BMA25X_EN_INT1_PAD_FWM__MSK     0x02
#define BMA25X_EN_INT1_PAD_FWM__REG     BMA25X_INT_DATA_SEL_REG

#define BMA25X_EN_INT1_PAD_FFULL__POS     2
#define BMA25X_EN_INT1_PAD_FFULL__LEN     1
#define BMA25X_EN_INT1_PAD_FFULL__MSK     0x04
#define BMA25X_EN_INT1_PAD_FFULL__REG     BMA25X_INT_DATA_SEL_REG

#define BMA25X_EN_INT2_PAD_FFULL__POS     5
#define BMA25X_EN_INT2_PAD_FFULL__LEN     1
#define BMA25X_EN_INT2_PAD_FFULL__MSK     0x20
#define BMA25X_EN_INT2_PAD_FFULL__REG     BMA25X_INT_DATA_SEL_REG

#define BMA25X_EN_INT2_PAD_FWM__POS     6
#define BMA25X_EN_INT2_PAD_FWM__LEN     1
#define BMA25X_EN_INT2_PAD_FWM__MSK     0x40
#define BMA25X_EN_INT2_PAD_FWM__REG     BMA25X_INT_DATA_SEL_REG

#define BMA25X_EN_INT2_PAD_NEWDATA__POS     7
#define BMA25X_EN_INT2_PAD_NEWDATA__LEN     1
#define BMA25X_EN_INT2_PAD_NEWDATA__MSK     0x80
#define BMA25X_EN_INT2_PAD_NEWDATA__REG     BMA25X_INT_DATA_SEL_REG

#define BMA25X_UNFILT_INT_SRC_LOWG__POS        0
#define BMA25X_UNFILT_INT_SRC_LOWG__LEN        1
#define BMA25X_UNFILT_INT_SRC_LOWG__MSK        0x01
#define BMA25X_UNFILT_INT_SRC_LOWG__REG        BMA25X_INT_SRC_REG

#define BMA25X_UNFILT_INT_SRC_HIGHG__POS       1
#define BMA25X_UNFILT_INT_SRC_HIGHG__LEN       1
#define BMA25X_UNFILT_INT_SRC_HIGHG__MSK       0x02
#define BMA25X_UNFILT_INT_SRC_HIGHG__REG       BMA25X_INT_SRC_REG

#define BMA25X_UNFILT_INT_SRC_SLOPE__POS       2
#define BMA25X_UNFILT_INT_SRC_SLOPE__LEN       1
#define BMA25X_UNFILT_INT_SRC_SLOPE__MSK       0x04
#define BMA25X_UNFILT_INT_SRC_SLOPE__REG       BMA25X_INT_SRC_REG

#define BMA25X_UNFILT_INT_SRC_SLO_NO_MOT__POS        3
#define BMA25X_UNFILT_INT_SRC_SLO_NO_MOT__LEN        1
#define BMA25X_UNFILT_INT_SRC_SLO_NO_MOT__MSK        0x08
#define BMA25X_UNFILT_INT_SRC_SLO_NO_MOT__REG        BMA25X_INT_SRC_REG

#define BMA25X_UNFILT_INT_SRC_TAP__POS         4
#define BMA25X_UNFILT_INT_SRC_TAP__LEN         1
#define BMA25X_UNFILT_INT_SRC_TAP__MSK         0x10
#define BMA25X_UNFILT_INT_SRC_TAP__REG         BMA25X_INT_SRC_REG

#define BMA25X_UNFILT_INT_SRC_DATA__POS        5
#define BMA25X_UNFILT_INT_SRC_DATA__LEN        1
#define BMA25X_UNFILT_INT_SRC_DATA__MSK        0x20
#define BMA25X_UNFILT_INT_SRC_DATA__REG        BMA25X_INT_SRC_REG

#define BMA25X_INT1_PAD_ACTIVE_LEVEL__POS       0
#define BMA25X_INT1_PAD_ACTIVE_LEVEL__LEN       1
#define BMA25X_INT1_PAD_ACTIVE_LEVEL__MSK       0x01
#define BMA25X_INT1_PAD_ACTIVE_LEVEL__REG       BMA25X_INT_SET_REG

#define BMA25X_INT2_PAD_ACTIVE_LEVEL__POS       2
#define BMA25X_INT2_PAD_ACTIVE_LEVEL__LEN       1
#define BMA25X_INT2_PAD_ACTIVE_LEVEL__MSK       0x04
#define BMA25X_INT2_PAD_ACTIVE_LEVEL__REG       BMA25X_INT_SET_REG

#define BMA25X_INT1_PAD_OUTPUT_TYPE__POS        1
#define BMA25X_INT1_PAD_OUTPUT_TYPE__LEN        1
#define BMA25X_INT1_PAD_OUTPUT_TYPE__MSK        0x02
#define BMA25X_INT1_PAD_OUTPUT_TYPE__REG        BMA25X_INT_SET_REG

#define BMA25X_INT2_PAD_OUTPUT_TYPE__POS        3
#define BMA25X_INT2_PAD_OUTPUT_TYPE__LEN        1
#define BMA25X_INT2_PAD_OUTPUT_TYPE__MSK        0x08
#define BMA25X_INT2_PAD_OUTPUT_TYPE__REG        BMA25X_INT_SET_REG

#define BMA25X_INT_MODE_SEL__POS                0
#define BMA25X_INT_MODE_SEL__LEN                4
#define BMA25X_INT_MODE_SEL__MSK                0x0F
#define BMA25X_INT_MODE_SEL__REG                BMA25X_INT_CTRL_REG

#define BMA25X_RESET_INT__POS           7
#define BMA25X_RESET_INT__LEN           1
#define BMA25X_RESET_INT__MSK           0x80
#define BMA25X_RESET_INT__REG           BMA25X_INT_CTRL_REG

#define BMA25X_LOWG_DUR__POS                    0
#define BMA25X_LOWG_DUR__LEN                    8
#define BMA25X_LOWG_DUR__MSK                    0xFF
#define BMA25X_LOWG_DUR__REG                    BMA25X_LOW_DURN_REG

#define BMA25X_LOWG_THRES__POS                  0
#define BMA25X_LOWG_THRES__LEN                  8
#define BMA25X_LOWG_THRES__MSK                  0xFF
#define BMA25X_LOWG_THRES__REG                  BMA25X_LOW_THRES_REG

#define BMA25X_LOWG_HYST__POS                   0
#define BMA25X_LOWG_HYST__LEN                   2
#define BMA25X_LOWG_HYST__MSK                   0x03
#define BMA25X_LOWG_HYST__REG                   BMA25X_LOW_HIGH_HYST_REG

#define BMA25X_LOWG_INT_MODE__POS               2
#define BMA25X_LOWG_INT_MODE__LEN               1
#define BMA25X_LOWG_INT_MODE__MSK               0x04
#define BMA25X_LOWG_INT_MODE__REG               BMA25X_LOW_HIGH_HYST_REG

#define BMA25X_HIGHG_DUR__POS                    0
#define BMA25X_HIGHG_DUR__LEN                    8
#define BMA25X_HIGHG_DUR__MSK                    0xFF
#define BMA25X_HIGHG_DUR__REG                    BMA25X_HIGH_DURN_REG

#define BMA25X_HIGHG_THRES__POS                  0
#define BMA25X_HIGHG_THRES__LEN                  8
#define BMA25X_HIGHG_THRES__MSK                  0xFF
#define BMA25X_HIGHG_THRES__REG                  BMA25X_HIGH_THRES_REG

#define BMA25X_HIGHG_HYST__POS                  6
#define BMA25X_HIGHG_HYST__LEN                  2
#define BMA25X_HIGHG_HYST__MSK                  0xC0
#define BMA25X_HIGHG_HYST__REG                  BMA25X_LOW_HIGH_HYST_REG

#define BMA25X_SLOPE_DUR__POS                    0
#define BMA25X_SLOPE_DUR__LEN                    2
#define BMA25X_SLOPE_DUR__MSK                    0x03
#define BMA25X_SLOPE_DUR__REG                    BMA25X_SLOPE_DURN_REG

#define BMA25X_SLO_NO_MOT_DUR__POS                    2
#define BMA25X_SLO_NO_MOT_DUR__LEN                    6
#define BMA25X_SLO_NO_MOT_DUR__MSK                    0xFC
#define BMA25X_SLO_NO_MOT_DUR__REG                    BMA25X_SLOPE_DURN_REG

#define BMA25X_SLOPE_THRES__POS                  0
#define BMA25X_SLOPE_THRES__LEN                  8
#define BMA25X_SLOPE_THRES__MSK                  0xFF
#define BMA25X_SLOPE_THRES__REG                  BMA25X_SLOPE_THRES_REG

#define BMA25X_SLO_NO_MOT_THRES__POS                  0
#define BMA25X_SLO_NO_MOT_THRES__LEN                  8
#define BMA25X_SLO_NO_MOT_THRES__MSK                  0xFF
#define BMA25X_SLO_NO_MOT_THRES__REG           BMA25X_SLO_NO_MOT_THRES_REG

#define BMA25X_TAP_DUR__POS                    0
#define BMA25X_TAP_DUR__LEN                    3
#define BMA25X_TAP_DUR__MSK                    0x07
#define BMA25X_TAP_DUR__REG                    BMA25X_TAP_PARAM_REG

#define BMA25X_TAP_SHOCK_DURN__POS             6
#define BMA25X_TAP_SHOCK_DURN__LEN             1
#define BMA25X_TAP_SHOCK_DURN__MSK             0x40
#define BMA25X_TAP_SHOCK_DURN__REG             BMA25X_TAP_PARAM_REG

#define BMA25X_ADV_TAP_INT__POS                5
#define BMA25X_ADV_TAP_INT__LEN                1
#define BMA25X_ADV_TAP_INT__MSK                0x20
#define BMA25X_ADV_TAP_INT__REG                BMA25X_TAP_PARAM_REG

#define BMA25X_TAP_QUIET_DURN__POS             7
#define BMA25X_TAP_QUIET_DURN__LEN             1
#define BMA25X_TAP_QUIET_DURN__MSK             0x80
#define BMA25X_TAP_QUIET_DURN__REG             BMA25X_TAP_PARAM_REG

#define BMA25X_TAP_THRES__POS                  0
#define BMA25X_TAP_THRES__LEN                  5
#define BMA25X_TAP_THRES__MSK                  0x1F
#define BMA25X_TAP_THRES__REG                  BMA25X_TAP_THRES_REG

#define BMA25X_TAP_SAMPLES__POS                6
#define BMA25X_TAP_SAMPLES__LEN                2
#define BMA25X_TAP_SAMPLES__MSK                0xC0
#define BMA25X_TAP_SAMPLES__REG                BMA25X_TAP_THRES_REG

#define BMA25X_ORIENT_MODE__POS                  0
#define BMA25X_ORIENT_MODE__LEN                  2
#define BMA25X_ORIENT_MODE__MSK                  0x03
#define BMA25X_ORIENT_MODE__REG                  BMA25X_ORIENT_PARAM_REG

#define BMA25X_ORIENT_BLOCK__POS                 2
#define BMA25X_ORIENT_BLOCK__LEN                 2
#define BMA25X_ORIENT_BLOCK__MSK                 0x0C
#define BMA25X_ORIENT_BLOCK__REG                 BMA25X_ORIENT_PARAM_REG

#define BMA25X_ORIENT_HYST__POS                  4
#define BMA25X_ORIENT_HYST__LEN                  3
#define BMA25X_ORIENT_HYST__MSK                  0x70
#define BMA25X_ORIENT_HYST__REG                  BMA25X_ORIENT_PARAM_REG

#define BMA25X_ORIENT_AXIS__POS                  7
#define BMA25X_ORIENT_AXIS__LEN                  1
#define BMA25X_ORIENT_AXIS__MSK                  0x80
#define BMA25X_ORIENT_AXIS__REG                  BMA25X_THETA_BLOCK_REG

#define BMA25X_ORIENT_UD_EN__POS                  6
#define BMA25X_ORIENT_UD_EN__LEN                  1
#define BMA25X_ORIENT_UD_EN__MSK                  0x40
#define BMA25X_ORIENT_UD_EN__REG                  BMA25X_THETA_BLOCK_REG

#define BMA25X_THETA_BLOCK__POS                  0
#define BMA25X_THETA_BLOCK__LEN                  6
#define BMA25X_THETA_BLOCK__MSK                  0x3F
#define BMA25X_THETA_BLOCK__REG                  BMA25X_THETA_BLOCK_REG

#define BMA25X_THETA_FLAT__POS                  0
#define BMA25X_THETA_FLAT__LEN                  6
#define BMA25X_THETA_FLAT__MSK                  0x3F
#define BMA25X_THETA_FLAT__REG                  BMA25X_THETA_FLAT_REG

#define BMA25X_FLAT_HOLD_TIME__POS              4
#define BMA25X_FLAT_HOLD_TIME__LEN              2
#define BMA25X_FLAT_HOLD_TIME__MSK              0x30
#define BMA25X_FLAT_HOLD_TIME__REG              BMA25X_FLAT_HOLD_TIME_REG

#define BMA25X_FLAT_HYS__POS                   0
#define BMA25X_FLAT_HYS__LEN                   3
#define BMA25X_FLAT_HYS__MSK                   0x07
#define BMA25X_FLAT_HYS__REG                   BMA25X_FLAT_HOLD_TIME_REG

#define BMA25X_FIFO_WML_TRIG_RETAIN__POS                   0
#define BMA25X_FIFO_WML_TRIG_RETAIN__LEN                   6
#define BMA25X_FIFO_WML_TRIG_RETAIN__MSK                   0x3F
#define BMA25X_FIFO_WML_TRIG_RETAIN__REG                   BMA25X_FIFO_WML_TRIG

#define BMA25X_EN_SELF_TEST__POS                0
#define BMA25X_EN_SELF_TEST__LEN                2
#define BMA25X_EN_SELF_TEST__MSK                0x03
#define BMA25X_EN_SELF_TEST__REG                BMA25X_SELF_TEST_REG

#define BMA25X_NEG_SELF_TEST__POS               2
#define BMA25X_NEG_SELF_TEST__LEN               1
#define BMA25X_NEG_SELF_TEST__MSK               0x04
#define BMA25X_NEG_SELF_TEST__REG               BMA25X_SELF_TEST_REG

#define BMA25X_SELF_TEST_AMP__POS               4
#define BMA25X_SELF_TEST_AMP__LEN               1
#define BMA25X_SELF_TEST_AMP__MSK               0x10
#define BMA25X_SELF_TEST_AMP__REG               BMA25X_SELF_TEST_REG


#define BMA25X_UNLOCK_EE_PROG_MODE__POS     0
#define BMA25X_UNLOCK_EE_PROG_MODE__LEN     1
#define BMA25X_UNLOCK_EE_PROG_MODE__MSK     0x01
#define BMA25X_UNLOCK_EE_PROG_MODE__REG     BMA25X_EEPROM_CTRL_REG

#define BMA25X_START_EE_PROG_TRIG__POS      1
#define BMA25X_START_EE_PROG_TRIG__LEN      1
#define BMA25X_START_EE_PROG_TRIG__MSK      0x02
#define BMA25X_START_EE_PROG_TRIG__REG      BMA25X_EEPROM_CTRL_REG

#define BMA25X_EE_PROG_READY__POS          2
#define BMA25X_EE_PROG_READY__LEN          1
#define BMA25X_EE_PROG_READY__MSK          0x04
#define BMA25X_EE_PROG_READY__REG          BMA25X_EEPROM_CTRL_REG

#define BMA25X_UPDATE_IMAGE__POS                3
#define BMA25X_UPDATE_IMAGE__LEN                1
#define BMA25X_UPDATE_IMAGE__MSK                0x08
#define BMA25X_UPDATE_IMAGE__REG                BMA25X_EEPROM_CTRL_REG

#define BMA25X_EE_REMAIN__POS                4
#define BMA25X_EE_REMAIN__LEN                4
#define BMA25X_EE_REMAIN__MSK                0xF0
#define BMA25X_EE_REMAIN__REG                BMA25X_EEPROM_CTRL_REG

#define BMA25X_EN_SPI_MODE_3__POS              0
#define BMA25X_EN_SPI_MODE_3__LEN              1
#define BMA25X_EN_SPI_MODE_3__MSK              0x01
#define BMA25X_EN_SPI_MODE_3__REG              BMA25X_SERIAL_CTRL_REG

#define BMA25X_I2C_WATCHDOG_PERIOD__POS        1
#define BMA25X_I2C_WATCHDOG_PERIOD__LEN        1
#define BMA25X_I2C_WATCHDOG_PERIOD__MSK        0x02
#define BMA25X_I2C_WATCHDOG_PERIOD__REG        BMA25X_SERIAL_CTRL_REG

#define BMA25X_EN_I2C_WATCHDOG__POS            2
#define BMA25X_EN_I2C_WATCHDOG__LEN            1
#define BMA25X_EN_I2C_WATCHDOG__MSK            0x04
#define BMA25X_EN_I2C_WATCHDOG__REG            BMA25X_SERIAL_CTRL_REG

#define BMA25X_EXT_MODE__POS              7
#define BMA25X_EXT_MODE__LEN              1
#define BMA25X_EXT_MODE__MSK              0x80
#define BMA25X_EXT_MODE__REG              BMA25X_EXTMODE_CTRL_REG

#define BMA25X_ALLOW_UPPER__POS        6
#define BMA25X_ALLOW_UPPER__LEN        1
#define BMA25X_ALLOW_UPPER__MSK        0x40
#define BMA25X_ALLOW_UPPER__REG        BMA25X_EXTMODE_CTRL_REG

#define BMA25X_MAP_2_LOWER__POS            5
#define BMA25X_MAP_2_LOWER__LEN            1
#define BMA25X_MAP_2_LOWER__MSK            0x20
#define BMA25X_MAP_2_LOWER__REG            BMA25X_EXTMODE_CTRL_REG

#define BMA25X_MAGIC_NUMBER__POS            0
#define BMA25X_MAGIC_NUMBER__LEN            5
#define BMA25X_MAGIC_NUMBER__MSK            0x1F
#define BMA25X_MAGIC_NUMBER__REG            BMA25X_EXTMODE_CTRL_REG

#define BMA25X_UNLOCK_EE_WRITE_TRIM__POS        4
#define BMA25X_UNLOCK_EE_WRITE_TRIM__LEN        4
#define BMA25X_UNLOCK_EE_WRITE_TRIM__MSK        0xF0
#define BMA25X_UNLOCK_EE_WRITE_TRIM__REG        BMA25X_CTRL_UNLOCK_REG

#define BMA25X_EN_SLOW_COMP_X__POS              0
#define BMA25X_EN_SLOW_COMP_X__LEN              1
#define BMA25X_EN_SLOW_COMP_X__MSK              0x01
#define BMA25X_EN_SLOW_COMP_X__REG              BMA25X_OFFSET_CTRL_REG

#define BMA25X_EN_SLOW_COMP_Y__POS              1
#define BMA25X_EN_SLOW_COMP_Y__LEN              1
#define BMA25X_EN_SLOW_COMP_Y__MSK              0x02
#define BMA25X_EN_SLOW_COMP_Y__REG              BMA25X_OFFSET_CTRL_REG

#define BMA25X_EN_SLOW_COMP_Z__POS              2
#define BMA25X_EN_SLOW_COMP_Z__LEN              1
#define BMA25X_EN_SLOW_COMP_Z__MSK              0x04
#define BMA25X_EN_SLOW_COMP_Z__REG              BMA25X_OFFSET_CTRL_REG

#define BMA25X_FAST_CAL_RDY_S__POS             4
#define BMA25X_FAST_CAL_RDY_S__LEN             1
#define BMA25X_FAST_CAL_RDY_S__MSK             0x10
#define BMA25X_FAST_CAL_RDY_S__REG             BMA25X_OFFSET_CTRL_REG

#define BMA25X_CAL_TRIGGER__POS                5
#define BMA25X_CAL_TRIGGER__LEN                2
#define BMA25X_CAL_TRIGGER__MSK                0x60
#define BMA25X_CAL_TRIGGER__REG                BMA25X_OFFSET_CTRL_REG

#define BMA25X_RESET_OFFSET_REGS__POS           7
#define BMA25X_RESET_OFFSET_REGS__LEN           1
#define BMA25X_RESET_OFFSET_REGS__MSK           0x80
#define BMA25X_RESET_OFFSET_REGS__REG           BMA25X_OFFSET_CTRL_REG

#define BMA25X_COMP_CUTOFF__POS                 0
#define BMA25X_COMP_CUTOFF__LEN                 1
#define BMA25X_COMP_CUTOFF__MSK                 0x01
#define BMA25X_COMP_CUTOFF__REG                 BMA25X_OFFSET_PARAMS_REG

#define BMA25X_COMP_TARGET_OFFSET_X__POS        1
#define BMA25X_COMP_TARGET_OFFSET_X__LEN        2
#define BMA25X_COMP_TARGET_OFFSET_X__MSK        0x06
#define BMA25X_COMP_TARGET_OFFSET_X__REG        BMA25X_OFFSET_PARAMS_REG

#define BMA25X_COMP_TARGET_OFFSET_Y__POS        3
#define BMA25X_COMP_TARGET_OFFSET_Y__LEN        2
#define BMA25X_COMP_TARGET_OFFSET_Y__MSK        0x18
#define BMA25X_COMP_TARGET_OFFSET_Y__REG        BMA25X_OFFSET_PARAMS_REG

#define BMA25X_COMP_TARGET_OFFSET_Z__POS        5
#define BMA25X_COMP_TARGET_OFFSET_Z__LEN        2
#define BMA25X_COMP_TARGET_OFFSET_Z__MSK        0x60
#define BMA25X_COMP_TARGET_OFFSET_Z__REG        BMA25X_OFFSET_PARAMS_REG

#define BMA25X_FIFO_DATA_SELECT__POS                 0
#define BMA25X_FIFO_DATA_SELECT__LEN                 2
#define BMA25X_FIFO_DATA_SELECT__MSK                 0x03
#define BMA25X_FIFO_DATA_SELECT__REG                 BMA25X_FIFO_MODE_REG

#define BMA25X_FIFO_TRIGGER_SOURCE__POS                 2
#define BMA25X_FIFO_TRIGGER_SOURCE__LEN                 2
#define BMA25X_FIFO_TRIGGER_SOURCE__MSK                 0x0C
#define BMA25X_FIFO_TRIGGER_SOURCE__REG                 BMA25X_FIFO_MODE_REG

#define BMA25X_FIFO_TRIGGER_ACTION__POS                 4
#define BMA25X_FIFO_TRIGGER_ACTION__LEN                 2
#define BMA25X_FIFO_TRIGGER_ACTION__MSK                 0x30
#define BMA25X_FIFO_TRIGGER_ACTION__REG                 BMA25X_FIFO_MODE_REG

#define BMA25X_FIFO_MODE__POS                 6
#define BMA25X_FIFO_MODE__LEN                 2
#define BMA25X_FIFO_MODE__MSK                 0xC0
#define BMA25X_FIFO_MODE__REG                 BMA25X_FIFO_MODE_REG


#define BMA25X_STATUS1                             0
#define BMA25X_STATUS2                             1
#define BMA25X_STATUS3                             2
#define BMA25X_STATUS4                             3
#define BMA25X_STATUS5                             4


#define BMA25X_RANGE_2G                 3
#define BMA25X_RANGE_4G                 5
#define BMA25X_RANGE_8G                 8
#define BMA25X_RANGE_16G                12


#define BMA25X_BW_7_81HZ        0x08
#define BMA25X_BW_15_63HZ       0x09
#define BMA25X_BW_31_25HZ       0x0A
#define BMA25X_BW_62_50HZ       0x0B
#define BMA25X_BW_125HZ         0x0C
#define BMA25X_BW_250HZ         0x0D
#define BMA25X_BW_500HZ         0x0E
#define BMA25X_BW_1000HZ        0x0F

#define BMA25X_SLEEP_DUR_0_5MS        0x05
#define BMA25X_SLEEP_DUR_1MS          0x06
#define BMA25X_SLEEP_DUR_2MS          0x07
#define BMA25X_SLEEP_DUR_4MS          0x08
#define BMA25X_SLEEP_DUR_6MS          0x09
#define BMA25X_SLEEP_DUR_10MS         0x0A
#define BMA25X_SLEEP_DUR_25MS         0x0B
#define BMA25X_SLEEP_DUR_50MS         0x0C
#define BMA25X_SLEEP_DUR_100MS        0x0D
#define BMA25X_SLEEP_DUR_500MS        0x0E
#define BMA25X_SLEEP_DUR_1S           0x0F

#define BMA25X_LATCH_DUR_NON_LATCH    0x00
#define BMA25X_LATCH_DUR_250MS        0x01
#define BMA25X_LATCH_DUR_500MS        0x02
#define BMA25X_LATCH_DUR_1S           0x03
#define BMA25X_LATCH_DUR_2S           0x04
#define BMA25X_LATCH_DUR_4S           0x05
#define BMA25X_LATCH_DUR_8S           0x06
#define BMA25X_LATCH_DUR_LATCH        0x07
#define BMA25X_LATCH_DUR_NON_LATCH1   0x08
#define BMA25X_LATCH_DUR_250US        0x09
#define BMA25X_LATCH_DUR_500US        0x0A
#define BMA25X_LATCH_DUR_1MS          0x0B
#define BMA25X_LATCH_DUR_12_5MS       0x0C
#define BMA25X_LATCH_DUR_25MS         0x0D
#define BMA25X_LATCH_DUR_50MS         0x0E
#define BMA25X_LATCH_DUR_LATCH1       0x0F

#define BMA25X_MODE_NORMAL             0
#define BMA25X_MODE_LOWPOWER1          1
#define BMA25X_MODE_SUSPEND            2
#define BMA25X_MODE_DEEP_SUSPEND       3
#define BMA25X_MODE_LOWPOWER2          4
#define BMA25X_MODE_STANDBY            5

#define BMA25X_X_AXIS           0
#define BMA25X_Y_AXIS           1
#define BMA25X_Z_AXIS           2

#define BMA25X_Low_G_Interrupt       0
#define BMA25X_High_G_X_Interrupt    1
#define BMA25X_High_G_Y_Interrupt    2
#define BMA25X_High_G_Z_Interrupt    3
#define BMA25X_DATA_EN               4
#define BMA25X_Slope_X_Interrupt     5
#define BMA25X_Slope_Y_Interrupt     6
#define BMA25X_Slope_Z_Interrupt     7
#define BMA25X_Single_Tap_Interrupt  8
#define BMA25X_Double_Tap_Interrupt  9
#define BMA25X_Orient_Interrupt      10
#define BMA25X_Flat_Interrupt        11
#define BMA25X_FFULL_INTERRUPT       12
#define BMA25X_FWM_INTERRUPT         13

#define BMA25X_INT1_LOWG         0
#define BMA25X_INT2_LOWG         1
#define BMA25X_INT1_HIGHG        0
#define BMA25X_INT2_HIGHG        1
#define BMA25X_INT1_SLOPE        0
#define BMA25X_INT2_SLOPE        1
#define BMA25X_INT1_SLO_NO_MOT   0
#define BMA25X_INT2_SLO_NO_MOT   1
#define BMA25X_INT1_DTAP         0
#define BMA25X_INT2_DTAP         1
#define BMA25X_INT1_STAP         0
#define BMA25X_INT2_STAP         1
#define BMA25X_INT1_ORIENT       0
#define BMA25X_INT2_ORIENT       1
#define BMA25X_INT1_FLAT         0
#define BMA25X_INT2_FLAT         1
#define BMA25X_INT1_NDATA        0
#define BMA25X_INT2_NDATA        1
#define BMA25X_INT1_FWM          0
#define BMA25X_INT2_FWM          1
#define BMA25X_INT1_FFULL        0
#define BMA25X_INT2_FFULL        1

#define BMA25X_SRC_LOWG         0
#define BMA25X_SRC_HIGHG        1
#define BMA25X_SRC_SLOPE        2
#define BMA25X_SRC_SLO_NO_MOT   3
#define BMA25X_SRC_TAP          4
#define BMA25X_SRC_DATA         5

#define BMA25X_INT1_OUTPUT      0
#define BMA25X_INT2_OUTPUT      1
#define BMA25X_INT1_LEVEL       0
#define BMA25X_INT2_LEVEL       1

#define BMA25X_LOW_DURATION            0
#define BMA25X_HIGH_DURATION           1
#define BMA25X_SLOPE_DURATION          2
#define BMA25X_SLO_NO_MOT_DURATION     3

#define BMA25X_LOW_THRESHOLD            0
#define BMA25X_HIGH_THRESHOLD           1
#define BMA25X_SLOPE_THRESHOLD          2
#define BMA25X_SLO_NO_MOT_THRESHOLD     3


#define BMA25X_LOWG_HYST                0
#define BMA25X_HIGHG_HYST               1

#define BMA25X_ORIENT_THETA             0
#define BMA25X_FLAT_THETA               1

#define BMA25X_I2C_SELECT               0
#define BMA25X_I2C_EN                   1

#define BMA25X_SLOW_COMP_X              0
#define BMA25X_SLOW_COMP_Y              1
#define BMA25X_SLOW_COMP_Z              2

#define BMA25X_CUT_OFF                  0
#define BMA25X_OFFSET_TRIGGER_X         1
#define BMA25X_OFFSET_TRIGGER_Y         2
#define BMA25X_OFFSET_TRIGGER_Z         3

#define BMA25X_GP0                      0
#define BMA25X_GP1                      1

#define BMA25X_SLO_NO_MOT_EN_X          0
#define BMA25X_SLO_NO_MOT_EN_Y          1
#define BMA25X_SLO_NO_MOT_EN_Z          2
#define BMA25X_SLO_NO_MOT_EN_SEL        3

#define BMA25X_WAKE_UP_DUR_20MS         0
#define BMA25X_WAKE_UP_DUR_80MS         1
#define BMA25X_WAKE_UP_DUR_320MS                2
#define BMA25X_WAKE_UP_DUR_2560MS               3

#define BMA25X_SELF_TEST0_ON            1
#define BMA25X_SELF_TEST1_ON            2

#define BMA25X_EE_W_OFF                 0
#define BMA25X_EE_W_ON                  1

#define BMA25X_LOW_TH_IN_G(gthres, range)           ((256 * gthres) / range)


#define BMA25X_HIGH_TH_IN_G(gthres, range)          ((256 * gthres) / range)


#define BMA25X_LOW_HY_IN_G(ghyst, range)            ((32 * ghyst) / range)


#define BMA25X_HIGH_HY_IN_G(ghyst, range)           ((32 * ghyst) / range)


#define BMA25X_SLOPE_TH_IN_G(gthres, range)    ((128 * gthres) / range)


#define BMA25X_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)


#define BMA25X_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

#ifdef CONFIG_BMA_ENABLE_NEWDATA_INT
#define BMA2x2_IS_NEWDATA_INT_ENABLED()        (true)
#else
#define BMA2x2_IS_NEWDATA_INT_ENABLED()        (false)
#endif

#ifdef BMA25X_ENABLE_INT1
#define BMA2x2_IS_INT1_ENABLED()        (true)
#else
#define BMA2x2_IS_INT1_ENABLED()        (false)
#endif

#ifdef BMA25X_ENABLE_INT2
#define BMA2x2_IS_INT2_ENABLED()        (true)
#else
#define BMA2x2_IS_INT2_ENABLED()        (false)
#endif

#define CHECK_CHIP_ID_TIME_MAX 5
#define BMA255_CHIP_ID 0XFA
#define BMA250E_CHIP_ID 0XF9
#define BMA222E_CHIP_ID 0XF8
#define BMA280_CHIP_ID 0XFB
#define BMA355_CHIP_ID 0XEA

#define BMA255_TYPE 0
#define BMA250E_TYPE 1
#define BMA222E_TYPE 2
#define BMA280_TYPE 3

#define MAX_FIFO_F_LEVEL 32
#define MAX_FIFO_F_BYTES 6
#define BMA_MAX_RETRY_I2C_XFER (100)

/*! Bosch sensor unknown place*/
#define BOSCH_SENSOR_PLACE_UNKNOWN (-1)
/*! Bosch sensor remapping table size P0~P7*/
#define MAX_AXIS_REMAP_TAB_SZ 8
#define BOSCH_SENSOR_PLANE        0
#define BOSCH_SENSOR_UP        1
#define BOSCH_SENSOR_DOWN        2
#define RETRY_TIME        50
/*!
 * @brief:BMI058 feature
 *  macro definition
 */

#define BMA25X_FIFO_DAT_SEL_X                     1
#define BMA25X_FIFO_DAT_SEL_Y                     2
#define BMA25X_FIFO_DAT_SEL_Z                     3

/*! high g or slope interrupt type definition*/
/*! High G interrupt of x, y, z axis happened */
#define HIGH_G_INTERRUPT_X            HIGH_G_INTERRUPT_X_HAPPENED
#define HIGH_G_INTERRUPT_Y            HIGH_G_INTERRUPT_Y_HAPPENED
#define HIGH_G_INTERRUPT_Z            HIGH_G_INTERRUPT_Z_HAPPENED
/*! High G interrupt of x, y, z negative axis happened */
#define HIGH_G_INTERRUPT_X_N          HIGH_G_INTERRUPT_X_NEGATIVE_HAPPENED
#define HIGH_G_INTERRUPT_Y_N          HIGH_G_INTERRUPT_Y_NEGATIVE_HAPPENED
#define HIGH_G_INTERRUPT_Z_N          HIGH_G_INTERRUPT_Z_NEGATIVE_HAPPENED
/*! Slope interrupt of x, y, z axis happened */
#define SLOPE_INTERRUPT_X             SLOPE_INTERRUPT_X_HAPPENED
#define SLOPE_INTERRUPT_Y             SLOPE_INTERRUPT_Y_HAPPENED
#define SLOPE_INTERRUPT_Z             SLOPE_INTERRUPT_Z_HAPPENED
/*! Slope interrupt of x, y, z negative axis happened */
#define SLOPE_INTERRUPT_X_N           SLOPE_INTERRUPT_X_NEGATIVE_HAPPENED
#define SLOPE_INTERRUPT_Y_N           SLOPE_INTERRUPT_Y_NEGATIVE_HAPPENED
#define SLOPE_INTERRUPT_Z_N           SLOPE_INTERRUPT_Z_NEGATIVE_HAPPENED

/*BMA power supply VDD 1.62V-3.6V VIO 1.2-3.6V */
#define BMA2x2_VDD_MIN_UV       2000000
#define BMA2x2_VDD_MAX_UV       3400000
#define BMA2x2_VIO_MIN_UV       1500000
#define BMA2x2_VIO_MAX_UV       3400000

/* Polling delay in msecs */
#define POLL_INTERVAL_MIN_MS        10
#define POLL_INTERVAL_MAX_MS        4000
#define POLL_DEFAULT_INTERVAL_MS 200

#define POLL_MS_100HZ 10

/* Interrupt delay in msecs */
#define BMA_INT_MAX_DELAY        64

#define MAX_RANGE_MAP        4

#define BMA_CAL_BUF_SIZE        99

struct bma25x_type_map_t {

	/*! bma25x sensor chip id */
	uint16_t chip_id;

	/*! bma25x sensor type */
	uint16_t sensor_type;

	/*! bma25x sensor name */
	const char *sensor_name;

	/*! bma25x sensor resolution */
	const char *resolution;
};

static const struct bma25x_type_map_t sensor_type_map[] = {
	{BMA255_CHIP_ID, BMA255_TYPE, "BMA255/254", "0.00957031"},
	{BMA355_CHIP_ID, BMA255_TYPE, "BMA355", "0.00957031"},
	{BMA250E_CHIP_ID, BMA250E_TYPE, "BMA250E", "0.03828125"},
	{BMA222E_CHIP_ID, BMA222E_TYPE, "BMA222E", "0.153125"},
	{BMA280_CHIP_ID, BMA280_TYPE, "BMA280", "0.00239258"},
};

/*!
 * we use a typedef to hide the detail,
 * because this type might be changed
 */
struct bosch_sensor_axis_remap {
	/* src means which source will be mapped to target x, y, z axis */
	/* if an target OS axis is remapped from (-)x,
	 * src is 0, sign_* is (-)1 */
	/* if an target OS axis is remapped from (-)y,
	 * src is 1, sign_* is (-)1 */
	/* if an target OS axis is remapped from (-)z,
	 * src is 2, sign_* is (-)1 */
	int src_x:3;
	int src_y:3;
	int src_z:3;

	int sign_x:2;
	int sign_y:2;
	int sign_z:2;
};

struct bosch_sensor_data {
	union {
		int16_t v[3];
		struct {
			int16_t x;
			int16_t y;
			int16_t z;
		};
	};
};

struct bma25xacc {
	s16 x;
	s16 y;
	s16 z;
};

struct bma25x_platform_data {
	int poll_interval;
	int gpio_int1;
	int gpio_int2;
	unsigned int int1_flag;
	unsigned int int2_flag;
	s8 place;
	bool int_en;
	bool use_int2; /* Use interrupt pin2 */
	bool use_hrtimer;
};

struct bma25x_suspend_state {
	bool powerEn;
};

struct bma25x_pinctrl_data {
	struct pinctrl          *pctrl;
	struct pinctrl_state    *pins_default;
	struct pinctrl_state    *pins_sleep;
};

struct bma25x_data {
	struct i2c_client *bma25x_client;
	struct sensors_classdev cdev;
	atomic_t delay;
	atomic_t enable;
	atomic_t selftest_result;
	atomic_t cal_status;
	char calibrate_buf[BMA_CAL_BUF_SIZE];
	unsigned int chip_id;
	unsigned int chip_type;
	unsigned int fifo_count;
	unsigned char fifo_datasel;
	unsigned char mode;
	signed char sensor_type;
	struct input_dev *input;
	struct bst_dev *bst_acc;
	struct bma25xacc value;
	struct mutex value_mutex;
	struct mutex enable_mutex;
	struct mutex mode_mutex;
	struct mutex op_lock;
	struct workqueue_struct *data_wq;
	struct delayed_work work;
	struct work_struct irq_work;
	struct hrtimer accel_timer;
	int accel_wkp_flag;
	struct task_struct *accel_task;
	bool accel_delay_change;
	wait_queue_head_t accel_wq;
	struct regulator *vdd;
	struct regulator *vio;
	bool power_enabled;
	unsigned char bandwidth;
	unsigned char range;
	unsigned int int_flag;
	int sensitivity;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	int IRQ;
	struct bma25x_platform_data *pdata;
	struct bma25x_suspend_state suspend_state;
	struct bma25x_pinctrl_data *pctrl_data;

	int ref_count;
	struct input_dev *dev_interrupt;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bma25x_early_suspend(struct early_suspend *h);
static void bma25x_late_resume(struct early_suspend *h);
#endif

static int bma25x_open_init(struct i2c_client *client,
		struct bma25x_data *data);
static int bma25x_set_mode(struct i2c_client *client, u8 mode);
static int bma25x_get_mode(struct i2c_client *client, u8 *mode);
static int bma25x_normal_to_suspend(struct bma25x_data *bma25x,
		unsigned char data1, unsigned char data2);
static int bma25x_power_ctl(struct bma25x_data *data, bool on);
static int bma25x_get_sensitivity(struct bma25x_data *bma25x, int range);
static void bma25x_pinctrl_state(struct bma25x_data *data, bool active);

static struct sensors_classdev sensors_cdev = {
	.name = "bma25x-accel",
	.vendor = "bosch",
	.version = 1,
	.handle = SENSORS_ACCELERATION_HANDLE,
	.type = SENSOR_TYPE_ACCELEROMETER,
	.max_range = "156.8",        /* 16g */
	.resolution = "0.153125",        /* 15.6mg */
	.sensor_power = "0.13",        /* typical value */
	.min_delay = POLL_INTERVAL_MIN_MS * 1000, /* in microseconds */
	.max_delay = POLL_INTERVAL_MAX_MS,
	.max_latency = POLL_INTERVAL_MAX_MS,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = POLL_DEFAULT_INTERVAL_MS, /* in millisecond */
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
	.sensors_self_test = NULL,
};

/*Remapping for BMA25X*/

static const struct bosch_sensor_axis_remap
bst_axis_remap_tab_dft[MAX_AXIS_REMAP_TAB_SZ] = {
	/* src_x src_y src_z  sign_x  sign_y  sign_z */
	{  0,    1,    2,     1,      1,      1 }, /* P0 */
	{  1,    0,    2,     1,     -1,      1 }, /* P1 */
	{  0,    1,    2,    -1,     -1,      1 }, /* P2 */
	{  1,    0,    2,    -1,      1,      1 }, /* P3 */

	{  0,    1,    2,    -1,      1,     -1 }, /* P4 */
	{  1,    0,    2,    -1,     -1,     -1 }, /* P5 */
	{  0,    1,    2,     1,     -1,     -1 }, /* P6 */
	{  1,    0,    2,     1,      1,     -1 }, /* P7 */
};

static const int bosch_sensor_range_map[MAX_RANGE_MAP] = {
	0, /*2G range*/
	1, /*4G range*/
	2, /*8G range*/
	3  /*16G range*/
};

static void bst_remap_sensor_data(struct bosch_sensor_data *data,
		const struct bosch_sensor_axis_remap *remap)
{
	struct bosch_sensor_data tmp;

	tmp.x = data->v[remap->src_x] * remap->sign_x;
	tmp.y = data->v[remap->src_y] * remap->sign_y;
	tmp.z = data->v[remap->src_z] * remap->sign_z;

	memcpy(data, &tmp, sizeof(*data));
}


static void bst_remap_sensor_data_dft_tab(struct bosch_sensor_data *data,
		int place)
{
	/* sensor with place 0 needs not to be remapped */
	if ((place <= 0) || (place >= MAX_AXIS_REMAP_TAB_SZ))
		return;

	bst_remap_sensor_data(data, &bst_axis_remap_tab_dft[place]);
}

static void bma25x_remap_sensor_data(struct bma25xacc *val,
		struct bma25x_data *client_data)
{
	struct bosch_sensor_data bsd;
	bsd.x = val->x;
	bsd.y = val->y;
	bsd.z = val->z;

	bst_remap_sensor_data_dft_tab(&bsd, client_data->pdata->place);

	val->x = bsd.x;
	val->y = bsd.y;
	val->z = bsd.z;

}


static int bma25x_smbus_read_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;
	dummy = i2c_smbus_read_byte_data(client, reg_addr);
	if (dummy < 0)
		return -EIO;
	*data = dummy & 0x000000ff;

	return 0;
}

static int bma25x_smbus_write_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;

	dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
	if (dummy < 0)
		return -EIO;
	udelay(2);
	return 0;
}

static int bma25x_smbus_read_byte_block(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	s32 dummy = 0;
	dummy = i2c_smbus_read_i2c_block_data(client, reg_addr, len, data);
	if (dummy < 0)
		return -EIO;
	return 0;
}

static int bma25x_check_chip_id(struct i2c_client *client,
		struct bma25x_data *data)
{
	int i = 0;
	int err = 0;
	unsigned char chip_id = 0;
	unsigned char read_count = 0;
	unsigned char bma25x_sensor_type_count = 0;

	bma25x_sensor_type_count =
		sizeof(sensor_type_map) / sizeof(struct bma25x_type_map_t);

	while (read_count++ < CHECK_CHIP_ID_TIME_MAX) {
		err = bma25x_smbus_read_byte(client, BMA25X_CHIP_ID_REG,
				&chip_id);
		if (err < 0) {
			dev_err(&client->dev,
				"Bosch Sensortec Device not found i2c bus read error,read chip_id:%d\n",
				chip_id);
			err = -ENODEV;
			return err;
		}
		for (i = 0; i < bma25x_sensor_type_count; i++) {
			if (sensor_type_map[i].chip_id == chip_id) {
				data->sensor_type =
					sensor_type_map[i].sensor_type;
				data->chip_id = chip_id;
				dev_dbg(&client->dev,
						"Bosch Sensortec Device detected, HW IC name: %s\n",
						sensor_type_map[i].sensor_name);
				data->chip_type = i;
				return err;
			}
		}
		if (i < bma25x_sensor_type_count) {
			return err;
		} else if (read_count == CHECK_CHIP_ID_TIME_MAX) {
			dev_err(&client->dev,
				"Failed!Bosch Sensortec Device not found, mismatch chip_id:%d\n",
				chip_id);
			err = -ENODEV;
			return err;
		}
		I2C_RETRY_DELAY();
	}
	return err;
}

#if defined(BMA25X_ENABLE_INT1) || defined(BMA25X_ENABLE_INT2)
static int bma25x_set_newdata(struct i2c_client *client,
		unsigned char channel, unsigned char int_newdata)
{

	unsigned char data = 0;
	int comres = 0;

	switch (channel) {
	case BMA25X_INT1_NDATA:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT1_PAD_NEWDATA__REG, &data);
		data = BMA25X_SET_BITSLICE(data,
				BMA25X_EN_INT1_PAD_NEWDATA, int_newdata);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT1_PAD_NEWDATA__REG, &data);
		break;
	case BMA25X_INT2_NDATA:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT2_PAD_NEWDATA__REG, &data);
		data = BMA25X_SET_BITSLICE(data,
				BMA25X_EN_INT2_PAD_NEWDATA, int_newdata);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT2_PAD_NEWDATA__REG, &data);
		break;
	default:
		comres = -1;
		break;
	}

	return comres;

}
#endif

#ifdef BMA25X_ENABLE_INT1
static int bma25x_set_int1_pad_sel(struct i2c_client *client,
		unsigned char int1sel)
{
	int comres = 0;
	unsigned char data = 0;
	unsigned char state = 0;
	state = 0x01;


	switch (int1sel) {
	case 0:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT1_PAD_LOWG__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT1_PAD_LOWG,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT1_PAD_LOWG__REG, &data);
		break;
	case 1:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT1_PAD_HIGHG__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT1_PAD_HIGHG,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT1_PAD_HIGHG__REG, &data);
		break;
	case 2:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT1_PAD_SLOPE__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT1_PAD_SLOPE,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT1_PAD_SLOPE__REG, &data);
		break;
	case 3:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT1_PAD_DB_TAP__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT1_PAD_DB_TAP,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT1_PAD_DB_TAP__REG, &data);
		break;
	case 4:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT1_PAD_SNG_TAP__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT1_PAD_SNG_TAP,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT1_PAD_SNG_TAP__REG, &data);
		break;
	case 5:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT1_PAD_ORIENT__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT1_PAD_ORIENT,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT1_PAD_ORIENT__REG, &data);
		break;
	case 6:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT1_PAD_FLAT__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT1_PAD_FLAT,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT1_PAD_FLAT__REG, &data);
		break;
	case 7:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT1_PAD_SLO_NO_MOT__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT1_PAD_SLO_NO_MOT,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT1_PAD_SLO_NO_MOT__REG, &data);
		break;
	default:
		break;
	}

	return comres;
}
#endif /* BMA25X_ENABLE_INT1 */

#ifdef BMA25X_ENABLE_INT2
static int bma25x_set_int2_pad_sel(struct i2c_client *client,
		unsigned char int2sel)
{
	int comres = 0;
	unsigned char data = 0;
	unsigned char state = 0;
	state = 0x01;


	switch (int2sel) {
	case 0:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT2_PAD_LOWG__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT2_PAD_LOWG,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT2_PAD_LOWG__REG, &data);
		break;
	case 1:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT2_PAD_HIGHG__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT2_PAD_HIGHG,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT2_PAD_HIGHG__REG, &data);
		break;
	case 2:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT2_PAD_SLOPE__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT2_PAD_SLOPE,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT2_PAD_SLOPE__REG, &data);
		break;
	case 3:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT2_PAD_DB_TAP__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT2_PAD_DB_TAP,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT2_PAD_DB_TAP__REG, &data);
		break;
	case 4:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT2_PAD_SNG_TAP__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT2_PAD_SNG_TAP,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT2_PAD_SNG_TAP__REG, &data);
		break;
	case 5:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT2_PAD_ORIENT__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT2_PAD_ORIENT,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT2_PAD_ORIENT__REG, &data);
		break;
	case 6:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT2_PAD_FLAT__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT2_PAD_FLAT,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT2_PAD_FLAT__REG, &data);
		break;
	case 7:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_EN_INT2_PAD_SLO_NO_MOT__REG, &data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_EN_INT2_PAD_SLO_NO_MOT,
				state);
		comres = bma25x_smbus_write_byte(client,
				BMA25X_EN_INT2_PAD_SLO_NO_MOT__REG, &data);
		break;
	default:
		break;
	}

	return comres;
}
#endif /* BMA25X_ENABLE_INT2 */

#if defined(BMA25X_ENABLE_INT1) || defined(BMA25X_ENABLE_INT2)
static int bma25x_get_interruptstatus1(struct i2c_client *client,
		unsigned char *intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma25x_smbus_read_byte(client, BMA25X_STATUS1_REG, &data);
	*intstatus = data;

	return comres;
}

#ifdef CONFIG_BMA_ENABLE_NEWDATA_INT
static int bma25x_get_interruptstatus2(struct i2c_client *client,
		unsigned char *intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma25x_smbus_read_byte(client, BMA25X_STATUS2_REG, &data);
	*intstatus = data;

	return comres;
}
#endif

static int bma25x_get_HIGH_first(struct i2c_client *client,
		unsigned char param, unsigned char *intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	switch (param) {
	case 0:
		comres = bma25x_smbus_read_byte(client,
				BMA25X_STATUS_ORIENT_HIGH_REG, &data);
		data = BMA25X_GET_BITSLICE(data, BMA25X_HIGHG_FIRST_X);
		*intstatus = data;
		break;
	case 1:
		comres = bma25x_smbus_read_byte(client,
		BMA25X_STATUS_ORIENT_HIGH_REG, &data);
		data = BMA25X_GET_BITSLICE(data, BMA25X_HIGHG_FIRST_Y);
		*intstatus = data;
		break;
	case 2:
		comres = bma25x_smbus_read_byte(client,
		 BMA25X_STATUS_ORIENT_HIGH_REG, &data);
		data = BMA25X_GET_BITSLICE(data, BMA25X_HIGHG_FIRST_Z);
		*intstatus = data;
		break;
	default:
		break;
	}

	return comres;
}

static int bma25x_get_HIGH_sign(struct i2c_client *client,
		unsigned char *intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma25x_smbus_read_byte(client,
			BMA25X_STATUS_ORIENT_HIGH_REG, &data);
	data = BMA25X_GET_BITSLICE(data, BMA25X_HIGHG_SIGN_S);
	*intstatus = data;

	return comres;
}

#ifndef CONFIG_SIG_MOTION
static int bma25x_get_slope_first(struct i2c_client *client,
		unsigned char param, unsigned char *intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	switch (param) {
	case 0:
		comres = bma25x_smbus_read_byte(client,
		 BMA25X_STATUS_TAP_SLOPE_REG, &data);
		data = BMA25X_GET_BITSLICE(data, BMA25X_SLOPE_FIRST_X);
		*intstatus = data;
		break;
	case 1:
		comres = bma25x_smbus_read_byte(client,
		 BMA25X_STATUS_TAP_SLOPE_REG, &data);
		data = BMA25X_GET_BITSLICE(data, BMA25X_SLOPE_FIRST_Y);
		*intstatus = data;
		break;
	case 2:
		comres = bma25x_smbus_read_byte(client,
		 BMA25X_STATUS_TAP_SLOPE_REG, &data);
		data = BMA25X_GET_BITSLICE(data, BMA25X_SLOPE_FIRST_Z);
		*intstatus = data;
		break;
	default:
		break;
	}

	return comres;
}

static int bma25x_get_slope_sign(struct i2c_client *client,
		unsigned char *intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma25x_smbus_read_byte(client,
			BMA25X_STATUS_TAP_SLOPE_REG, &data);
	data = BMA25X_GET_BITSLICE(data, BMA25X_SLOPE_SIGN_S);
	*intstatus = data;

	return comres;
}
#endif

static int bma25x_get_orient_status(struct i2c_client *client,
		unsigned char *intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma25x_smbus_read_byte(client,
			BMA25X_STATUS_ORIENT_HIGH_REG, &data);
	data = BMA25X_GET_BITSLICE(data, BMA25X_ORIENT_S);
	*intstatus = data;

	return comres;
}

static int bma25x_get_orient_flat_status(struct i2c_client *client,
		unsigned char *intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma25x_smbus_read_byte(client,
			BMA25X_STATUS_ORIENT_HIGH_REG, &data);
	data = BMA25X_GET_BITSLICE(data, BMA25X_FLAT_S);
	*intstatus = data;

	return comres;
}
#endif /* defined(BMA25X_ENABLE_INT1)||defined(BMA25X_ENABLE_INT2) */

static int bma25x_get_fifo_mode(struct i2c_client *client,
	unsigned char *fifo_mode)
{
	int comres;
	unsigned char data = 0;

	comres = bma25x_smbus_read_byte(client, BMA25X_FIFO_MODE__REG, &data);
	*fifo_mode = BMA25X_GET_BITSLICE(data, BMA25X_FIFO_MODE);

	return comres;
}

/*!
 * brief: bma25x switch from normal to suspend mode
 * @param[i] bma25x
 * @param[i] data1, write to PMU_LPW
 * @param[i] data2, write to PMU_LOW_NOSIE
 *
 * @return zero success, none-zero failed
 */
static int bma25x_normal_to_suspend(struct bma25x_data *bma25x,
		unsigned char data1, unsigned char data2)
{
	unsigned char current_fifo_mode = 0;
	unsigned char current_op_mode = 0;
	if (bma25x == NULL)
		return -EINVAL;
	/* get current op mode from mode register */
	if (bma25x_get_mode(bma25x->bma25x_client, &current_op_mode) < 0)
		return -EIO;
	/* only aimed at operatiom mode chang from normal/lpw1 mode
	 * to suspend state.
	 */
	if (current_op_mode == BMA25X_MODE_NORMAL ||
			current_op_mode == BMA25X_MODE_LOWPOWER1) {
		/* get current fifo mode from fifo config register */
		if (bma25x_get_fifo_mode(bma25x->bma25x_client,
					&current_fifo_mode) < 0) {
			return -EIO;
		} else {
			bma25x_smbus_write_byte(bma25x->bma25x_client,
				BMA25X_LOW_NOISE_CTRL_REG, &data2);
			bma25x_smbus_write_byte(bma25x->bma25x_client,
				BMA25X_MODE_CTRL_REG, &data1);
			bma25x_smbus_write_byte(bma25x->bma25x_client,
				BMA25X_FIFO_MODE__REG, &current_fifo_mode);
			usleep(3000);
			return 0;
		}
	} else {
		bma25x_smbus_write_byte(bma25x->bma25x_client,
				BMA25X_LOW_NOISE_CTRL_REG, &data2);
		bma25x_smbus_write_byte(bma25x->bma25x_client,
				BMA25X_MODE_CTRL_REG, &data1);
		usleep(3000);
		return 0;
	}

}

static int bma25x_set_mode(struct i2c_client *client, unsigned char mode)
{
	int comres = 0;
	unsigned char data1 = 0;
	unsigned char data2 = 0;
	int ret = 0;
	struct bma25x_data *bma25x = i2c_get_clientdata(client);
	dev_err(&client->dev, "Ebma25x_set_mode entry!!!\n");
	mutex_lock(&bma25x->mode_mutex);
	if (BMA25X_MODE_SUSPEND == mode) {
		if (bma25x->ref_count > 0) {
			bma25x->ref_count--;
			if (0 < bma25x->ref_count) {
				mutex_unlock(&bma25x->mode_mutex);
				return 0;
			}
		}
	} else {
		bma25x->ref_count++;
		if (1 < bma25x->ref_count) {
			mutex_unlock(&bma25x->mode_mutex);
			return 0;
		}
	}
	mutex_unlock(&bma25x->mode_mutex);
	if (mode < 6) {
		comres = bma25x_smbus_read_byte(client,
				BMA25X_MODE_CTRL_REG, &data1);
		comres = bma25x_smbus_read_byte(client,
				BMA25X_LOW_NOISE_CTRL_REG, &data2);
		switch (mode) {
		case BMA25X_MODE_NORMAL:
			data1  = BMA25X_SET_BITSLICE(data1,
			 BMA25X_MODE_CTRL, 0);
			data2  = BMA25X_SET_BITSLICE(data2,
			 BMA25X_LOW_POWER_MODE, 0);
			bma25x_smbus_write_byte(client,
			 BMA25X_MODE_CTRL_REG, &data1);
			usleep(3000);
			bma25x_smbus_write_byte(client,
			 BMA25X_LOW_NOISE_CTRL_REG, &data2);
			break;
		case BMA25X_MODE_LOWPOWER1:
			data1  = BMA25X_SET_BITSLICE(data1,
			 BMA25X_MODE_CTRL, 2);
			data2  = BMA25X_SET_BITSLICE(data2,
			 BMA25X_LOW_POWER_MODE, 0);
			bma25x_smbus_write_byte(client,
			 BMA25X_MODE_CTRL_REG, &data1);
			usleep(3000);
			bma25x_smbus_write_byte(client,
			 BMA25X_LOW_NOISE_CTRL_REG, &data2);
			break;
		case BMA25X_MODE_SUSPEND:
			data1  = BMA25X_SET_BITSLICE(data1,
			 BMA25X_MODE_CTRL, 4);
			data2  = BMA25X_SET_BITSLICE(data2,
			 BMA25X_LOW_POWER_MODE, 0);
			/*aimed at anomaly resolution when switch to suspend*/
			ret = bma25x_normal_to_suspend(bma25x, data1, data2);
			if (ret < 0)
				dev_err(&client->dev, "Error switching to suspend");
			break;
		case BMA25X_MODE_DEEP_SUSPEND:
			data1  = BMA25X_SET_BITSLICE(data1,
			 BMA25X_MODE_CTRL, 1);
			data2  = BMA25X_SET_BITSLICE(data2,
			 BMA25X_LOW_POWER_MODE, 1);
			bma25x_smbus_write_byte(client,
			 BMA25X_MODE_CTRL_REG, &data1);
			usleep(3000);
			bma25x_smbus_write_byte(client,
			 BMA25X_LOW_NOISE_CTRL_REG, &data2);
			break;
		case BMA25X_MODE_LOWPOWER2:
			data1  = BMA25X_SET_BITSLICE(data1,
			 BMA25X_MODE_CTRL, 2);
			data2  = BMA25X_SET_BITSLICE(data2,
			 BMA25X_LOW_POWER_MODE, 1);
			bma25x_smbus_write_byte(client,
			 BMA25X_MODE_CTRL_REG, &data1);
			usleep(3000);
			bma25x_smbus_write_byte(client,
			 BMA25X_LOW_NOISE_CTRL_REG, &data2);
			break;
		case BMA25X_MODE_STANDBY:
			data1  = BMA25X_SET_BITSLICE(data1,
			 BMA25X_MODE_CTRL, 4);
			data2  = BMA25X_SET_BITSLICE(data2,
			 BMA25X_LOW_POWER_MODE, 1);
			bma25x_smbus_write_byte(client,
			 BMA25X_LOW_NOISE_CTRL_REG, &data2);
			usleep(3000);
			bma25x_smbus_write_byte(client,
			 BMA25X_MODE_CTRL_REG, &data1);
			break;
		}
	} else {
		comres = -1;
	}
	return comres;
}


static int bma25x_get_mode(struct i2c_client *client, unsigned char *mode)
{
	int comres = 0;
	unsigned char data1  = 0;
	unsigned char data2  = 0;

	comres = bma25x_smbus_read_byte(client,
		BMA25X_MODE_CTRL_REG, &data1);
	comres = bma25x_smbus_read_byte(client,
		BMA25X_LOW_NOISE_CTRL_REG, &data2);

	data1  = (data1 & 0xE0) >> 5;
	data2  = (data2 & 0x40) >> 6;

	if (data2 == 0x00) {
		switch (data1) {
		case 0:
			*mode  = BMA25X_MODE_NORMAL;
			break;
		case 1:
			*mode  = BMA25X_MODE_DEEP_SUSPEND;
			break;
		case 2:
			*mode  = BMA25X_MODE_LOWPOWER1;
			break;
		case 4:
		case 6:
			*mode  = BMA25X_MODE_SUSPEND;
			break;
		default:
			comres = -ENODEV;
			break;
		}
	} else if (data2 == 0x01) {
		switch (data1) {
		case 0:
		case 1:
		case 6:
			*mode  = BMA25X_MODE_DEEP_SUSPEND;
			break;
		case 2:
			*mode  = BMA25X_MODE_LOWPOWER2;
			break;
		case 4:
			*mode  = BMA25X_MODE_STANDBY;
			break;
		default:
			comres = -ENODEV;
			break;
		}
	} else {
		comres = -ENODEV;
	}
	return comres;
}

static int bma25x_set_range(struct i2c_client *client, unsigned char Range)
{
	int comres = 0;
	unsigned char data1 = 0;
	struct bma25x_data *bma25x = i2c_get_clientdata(client);

	if ((Range == 3) || (Range == 5) || (Range == 8) || (Range == 12)) {
		comres = bma25x_smbus_read_byte(client,
				BMA25X_RANGE_SEL_REG, &data1);
		switch (Range) {
		case BMA25X_RANGE_2G:
			data1  = BMA25X_SET_BITSLICE(data1,
			 BMA25X_RANGE_SEL, 3);
			break;
		case BMA25X_RANGE_4G:
			data1  = BMA25X_SET_BITSLICE(data1,
			 BMA25X_RANGE_SEL, 5);
			break;
		case BMA25X_RANGE_8G:
			data1  = BMA25X_SET_BITSLICE(data1,
			 BMA25X_RANGE_SEL, 8);
			break;
		case BMA25X_RANGE_16G:
			data1  = BMA25X_SET_BITSLICE(data1,
			 BMA25X_RANGE_SEL, 12);
			break;
		default:
			break;
		}
		comres += bma25x_smbus_write_byte(client,
				BMA25X_RANGE_SEL_REG, &data1);
		bma25x_get_sensitivity(bma25x, Range);
	} else {
		comres = -1;
	}
	return comres;
}

static int bma25x_get_range(struct i2c_client *client, unsigned char *Range)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma25x_smbus_read_byte(client,
			BMA25X_RANGE_SEL__REG, &data);
	data = BMA25X_GET_BITSLICE(data, BMA25X_RANGE_SEL);
	*Range = data;

	return comres;
}


static int bma25x_set_bandwidth(struct i2c_client *client, unsigned char BW)
{
	int comres = 0;
	unsigned char data = 0;
	int Bandwidth = 0;

	if (BW > 7 && BW < 16) {
		switch (BW) {
		case BMA25X_BW_7_81HZ:
			Bandwidth = BMA25X_BW_7_81HZ;
			/*  7.81 Hz      64000 uS   */
			break;
		case BMA25X_BW_15_63HZ:
			Bandwidth = BMA25X_BW_15_63HZ;
			/*  15.63 Hz     32000 uS   */
			break;
		case BMA25X_BW_31_25HZ:
			Bandwidth = BMA25X_BW_31_25HZ;
			/*  31.25 Hz     16000 uS   */
			break;
		case BMA25X_BW_62_50HZ:
			Bandwidth = BMA25X_BW_62_50HZ;
			/*  62.50 Hz     8000 uS   */
			break;
		case BMA25X_BW_125HZ:
			Bandwidth = BMA25X_BW_125HZ;
			/*  125 Hz       4000 uS   */
			break;
		case BMA25X_BW_250HZ:
			Bandwidth = BMA25X_BW_250HZ;
			/*  250 Hz       2000 uS   */
			break;
		case BMA25X_BW_500HZ:
			Bandwidth = BMA25X_BW_500HZ;
			/*  500 Hz       1000 uS   */
			break;
		case BMA25X_BW_1000HZ:
			Bandwidth = BMA25X_BW_1000HZ;
			/*  1000 Hz      500 uS   */
			break;
		default:
			break;
		}
		comres = bma25x_smbus_read_byte(client, BMA25X_BANDWIDTH__REG,
				&data);
		data = BMA25X_SET_BITSLICE(data, BMA25X_BANDWIDTH, Bandwidth);
		comres += bma25x_smbus_write_byte(client, BMA25X_BANDWIDTH__REG,
				&data);
	} else {
		comres = -1;
	}

	return comres;
}

static int bma25x_get_bandwidth(struct i2c_client *client, unsigned char *BW)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma25x_smbus_read_byte(client, BMA25X_BANDWIDTH__REG, &data);
	data = BMA25X_GET_BITSLICE(data, BMA25X_BANDWIDTH);
	*BW = data;

	return comres;
}

static int bma25x_soft_reset(struct i2c_client *client)
{
	int comres = 0;
	unsigned char data = BMA25X_EN_SOFT_RESET_VALUE;

	comres = bma25x_smbus_write_byte(client,
			BMA25X_EN_SOFT_RESET__REG, &data);
	return comres;
}

const int bma25x_sensor_bitwidth[] = {
	12,  10,  8, 14
};

static int bma25x_get_sensitivity(struct bma25x_data *bma25x, int range)
{

	switch (range) {
	case BMA25X_RANGE_2G:
		bma25x->sensitivity = bosch_sensor_range_map[0];
		break;
	case BMA25X_RANGE_4G:
		bma25x->sensitivity = bosch_sensor_range_map[1];
		break;
	case BMA25X_RANGE_8G:
		bma25x->sensitivity = bosch_sensor_range_map[2];
		break;
	case BMA25X_RANGE_16G:
		bma25x->sensitivity = bosch_sensor_range_map[3];
		break;
	default:
		bma25x->sensitivity = bosch_sensor_range_map[0];
		break;
	}
	return 0;
}

static int bma25x_read_accel_xyz(struct i2c_client *client,
		signed char sensor_type, struct bma25xacc *acc)
{
	int comres = 0;
	unsigned char data[6];
	struct bma25x_data *client_data = i2c_get_clientdata(client);
#ifndef BMA25X_SENSOR_IDENTIFICATION_ENABLE
	int bitwidth;
#endif
	comres = bma25x_smbus_read_byte_block(client,
			BMA25X_ACC_X12_LSB__REG, data, 6);
	if (sensor_type >= 4)
		return -EINVAL;

	acc->x = (data[1]<<8)|data[0];
	acc->y = (data[3]<<8)|data[2];
	acc->z = (data[5]<<8)|data[4];

#ifndef BMA25X_SENSOR_IDENTIFICATION_ENABLE
	bitwidth = bma25x_sensor_bitwidth[sensor_type];
	acc->x = (acc->x >> (16 - bitwidth));
	acc->y = (acc->y >> (16 - bitwidth));
	acc->z = (acc->z >> (16 - bitwidth));
#endif
	bma25x_remap_sensor_data(acc, client_data);
	return comres;
}

static void bma25x_report_axis_data(struct bma25x_data *bma25x,
		struct bma25xacc *value)
{
	ktime_t ts;
	int err;

	ts = ktime_get_boottime();
	err = bma25x_read_accel_xyz(bma25x->bma25x_client,
			bma25x->sensor_type, value);
	if (err < 0) {
		dev_err(&bma25x->bma25x_client->dev,
				"read accel data failed! err = %d\n", err);
		return;
	}
	input_report_abs(bma25x->input,
			ABS_X, (int)value->x << bma25x->sensitivity);
	input_report_abs(bma25x->input,
			ABS_Y, (int)value->y << bma25x->sensitivity);
	input_report_abs(bma25x->input,
			ABS_Z, (int)value->z << bma25x->sensitivity);
	input_event(bma25x->input,
			EV_SYN, SYN_TIME_SEC, ktime_to_timespec(ts).tv_sec);
	input_event(bma25x->input,
			EV_SYN, SYN_TIME_NSEC, ktime_to_timespec(ts).tv_nsec);
	input_sync(bma25x->input);
}

static void bma25x_work_func(struct work_struct *work)
{
	struct bma25x_data *bma25x =
		container_of((struct delayed_work *)work,
		struct bma25x_data, work);
	struct bma25xacc value;
	unsigned long delay = msecs_to_jiffies(atomic_read(&bma25x->delay));

	bma25x_report_axis_data(bma25x, &value);
	mutex_lock(&bma25x->value_mutex);
	bma25x->value = value;
	mutex_unlock(&bma25x->value_mutex);
	queue_delayed_work(bma25x->data_wq, &bma25x->work, delay);
}

static enum hrtimer_restart accel_timer_handle(struct hrtimer *hrtimer)
{
	struct bma25x_data *bma25x;
	ktime_t ktime;

	bma25x = container_of(hrtimer, struct bma25x_data, accel_timer);
	ktime = ktime_set(0, atomic_read(&bma25x->delay) * NSEC_PER_MSEC);
	hrtimer_forward_now(&bma25x->accel_timer, ktime);
	bma25x->accel_wkp_flag = 1;
	wake_up_interruptible(&bma25x->accel_wq);
	return HRTIMER_RESTART;
}

static int accel_poll_thread(void *data)
{
	struct bma25x_data *bma25x = data;
	struct bma25xacc value;

	while (1) {
		wait_event_interruptible(bma25x->accel_wq,
			((bma25x->accel_wkp_flag != 0) ||
			kthread_should_stop()));
		bma25x->accel_wkp_flag = 0;
		if (kthread_should_stop())
			break;

		mutex_lock(&bma25x->op_lock);
		if (bma25x->accel_delay_change) {
			if (atomic_read(&bma25x->delay) <= POLL_MS_100HZ)
				set_wake_up_idle(true);
			else
				set_wake_up_idle(false);
			bma25x->accel_delay_change = false;
		}
		mutex_unlock(&bma25x->op_lock);

		bma25x_report_axis_data(bma25x, &value);
		mutex_lock(&bma25x->value_mutex);
		bma25x->value = value;
		mutex_unlock(&bma25x->value_mutex);
	}

	return 0;
}

static void bma25x_set_enable(struct device *dev, int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);
	int pre_enable = atomic_read(&bma25x->enable);
	mutex_lock(&bma25x->enable_mutex);
	dev_err(dev, "bma25x_set_enable entry ebable:%d pre_enable:%d!!!\n",
		enable, pre_enable);
	if (enable) {
		if (pre_enable == 0) {
			if (bma25x_power_ctl(bma25x, true))
				dev_err(dev, "power failed\n");

			if (bma25x_open_init(client, bma25x) < 0)
				dev_err(dev, "set init failed\n");

			bma25x_set_mode(bma25x->bma25x_client,
				BMA25X_MODE_NORMAL);

#ifndef CONFIG_BMA_ENABLE_NEWDATA_INT
			schedule_delayed_work(&bma25x->work,
				msecs_to_jiffies(atomic_read(&bma25x->delay)));
#endif
			atomic_set(&bma25x->enable, 1);
		}
	} else {
		if (pre_enable == 1) {
			bma25x_set_mode(bma25x->bma25x_client,
				BMA25X_MODE_SUSPEND);

#ifndef CONFIG_BMA_ENABLE_NEWDATA_INT
			cancel_delayed_work_sync(&bma25x->work);
#endif
			atomic_set(&bma25x->enable, 0);
			if (bma25x_power_ctl(bma25x, false))
				dev_err(dev, "power failed\n");
		}
	}
	mutex_unlock(&bma25x->enable_mutex);
}

static int bma25x_cdev_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct bma25x_data *data = container_of(sensors_cdev,
			struct bma25x_data, cdev);

	bma25x_set_enable(&data->bma25x_client->dev, enable);
	return 0;
}

static int bma25x_cdev_poll_delay(struct sensors_classdev *sensors_cdev,
		unsigned int delay_ms)
{
	struct bma25x_data *data =
		container_of(sensors_cdev, struct bma25x_data, cdev);

	if (delay_ms < POLL_INTERVAL_MIN_MS)
		delay_ms = POLL_INTERVAL_MIN_MS;
	if (delay_ms > POLL_INTERVAL_MAX_MS)
		delay_ms = POLL_INTERVAL_MAX_MS;
	atomic_set(&data->delay, (unsigned int) delay_ms);

	return 0;
}

static ssize_t bma25x_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);

	if (bma25x_get_range(bma25x->bma25x_client, &data) < 0)
		return snprintf(buf, PAGE_SIZE, "Read error\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", data);
}

static ssize_t bma25x_range_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (bma25x_set_range(bma25x->bma25x_client, (unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma25x_bandwidth_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);

	if (bma25x_get_bandwidth(bma25x->bma25x_client, &data) < 0)
		return snprintf(buf, PAGE_SIZE, "Read error\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", data);
}

static ssize_t bma25x_bandwidth_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma25x->sensor_type == BMA280_TYPE)
		if ((unsigned char) data > 14)
			return -EINVAL;

	if (bma25x_set_bandwidth(bma25x->bma25x_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma25x_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&bma25x->delay));
}

static ssize_t bma25x_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (data < POLL_INTERVAL_MIN_MS)
		data = POLL_INTERVAL_MIN_MS;
	if (data > POLL_INTERVAL_MAX_MS)
		data = POLL_INTERVAL_MAX_MS;
	atomic_set(&bma25x->delay, (unsigned int) data);

	return count;
}

static ssize_t bma25x_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE,
			"%d\n", atomic_read(&bma25x->enable));
}

static ssize_t bma25x_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;

	if (kstrtoul(buf, 10, &data))
		return -EINVAL;
	dev_err(dev, "bma25x_enable_store entry\n");
	bma25x_set_enable(dev, data);
	dev_err(dev, "bma25x_enable_store finish\n");
	return count;
}

static ssize_t bma25x_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma25x_data *bma25x = input_get_drvdata(input);
	struct bma25xacc acc_value;

	bma25x_read_accel_xyz(bma25x->bma25x_client,
			bma25x->sensor_type, &acc_value);

	return snprintf(buf, PAGE_SIZE, "%d %d %d\n",
			acc_value.x, acc_value.y, acc_value.z);
}

static DEVICE_ATTR(range,
		S_IRUSR|S_IRGRP|S_IWUSR, bma25x_range_show, bma25x_range_store);
static DEVICE_ATTR(bandwidth,
		S_IRUSR|S_IRGRP|S_IWUSR,
		bma25x_bandwidth_show, bma25x_bandwidth_store);
static DEVICE_ATTR(delay,
		S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP,
		bma25x_delay_show, bma25x_delay_store);
static DEVICE_ATTR(enable,
		S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP,
		bma25x_enable_show, bma25x_enable_store);
static DEVICE_ATTR(value, S_IRUGO, bma25x_value_show, NULL);
static struct attribute *bma25x_attributes[] = {
	&dev_attr_range.attr,
	&dev_attr_bandwidth.attr,
	&dev_attr_delay.attr,
	&dev_attr_enable.attr,
	&dev_attr_value.attr,
	NULL
};

static struct attribute_group bma25x_attribute_group = {
	.attrs = bma25x_attributes
};

#if defined(BMA25X_ENABLE_INT1) || defined(BMA25X_ENABLE_INT2)
unsigned char *orient[] = {
	"upward looking portrait upright",
	"upward looking portrait upside-down",
	"upward looking landscape left",
	"upward looking landscape right",
	"downward looking portrait upright",
	"downward looking portrait upside-down",
	"downward looking landscape left",
	"downward looking landscape right"
};

static void bma25x_high_g_interrupt_handle(struct bma25x_data *bma25x)
{
	unsigned char first_value = 0;
	unsigned char sign_value = 0;
	int i;

	for (i = 0; i < 3; i++) {
		bma25x_get_HIGH_first(bma25x->bma25x_client, i, &first_value);
		if (first_value == 1) {
			bma25x_get_HIGH_sign(bma25x->bma25x_client,
					&sign_value);
			if (sign_value == 1) {
				if (i == 0)
					input_report_rel(bma25x->dev_interrupt,
							HIGH_G_INTERRUPT,
							HIGH_G_INTERRUPT_X_N);
				if (i == 1)
					input_report_rel(bma25x->dev_interrupt,
							HIGH_G_INTERRUPT,
							HIGH_G_INTERRUPT_Y_N);
				if (i == 2)
					input_report_rel(bma25x->dev_interrupt,
							HIGH_G_INTERRUPT,
							HIGH_G_INTERRUPT_Z_N);
			} else {
				if (i == 0)
					input_report_rel(bma25x->dev_interrupt,
							HIGH_G_INTERRUPT,
							HIGH_G_INTERRUPT_X);
				if (i == 1)
					input_report_rel(bma25x->dev_interrupt,
							HIGH_G_INTERRUPT,
							HIGH_G_INTERRUPT_Y);
				if (i == 2)
					input_report_rel(bma25x->dev_interrupt,
							HIGH_G_INTERRUPT,
							HIGH_G_INTERRUPT_Z);
			}
		}

		ISR_INFO(&bma25x->bma25x_client->dev,
			"High G interrupt happened,exis is %d,first is %d,sign is %d\n",
			i, first_value, sign_value);
	}
}

#ifndef CONFIG_SIG_MOTION
static void bma25x_slope_interrupt_handle(struct bma25x_data *bma25x)
{
	unsigned char first_value = 0;
	unsigned char sign_value = 0;
	int i;
	for (i = 0; i < 3; i++) {
		bma25x_get_slope_first(bma25x->bma25x_client, i, &first_value);
		if (first_value == 1) {
			bma25x_get_slope_sign(bma25x->bma25x_client,
					&sign_value);
			if (sign_value == 1) {
				if (i == 0)
					input_report_rel(bma25x->dev_interrupt,
							SLOP_INTERRUPT,
							SLOPE_INTERRUPT_X_N);
				if (i == 1)
					input_report_rel(bma25x->dev_interrupt,
							SLOP_INTERRUPT,
							SLOPE_INTERRUPT_Y_N);
				if (i == 2)
					input_report_rel(bma25x->dev_interrupt,
							SLOP_INTERRUPT,
							SLOPE_INTERRUPT_Z_N);
			} else {
				if (i == 0)
					input_report_rel(bma25x->dev_interrupt,
							SLOP_INTERRUPT,
							SLOPE_INTERRUPT_X);
				if (i == 1)
					input_report_rel(bma25x->dev_interrupt,
							SLOP_INTERRUPT,
							SLOPE_INTERRUPT_Y);
				if (i == 2)
					input_report_rel(bma25x->dev_interrupt,
							SLOP_INTERRUPT,
							SLOPE_INTERRUPT_Z);

			}
		}

		ISR_INFO(&bma25x->bma25x_client->dev,
			"Slop interrupt happened,exis is %d,first is %d,sign is %d\n",
			i, first_value, sign_value);
	}
}
#endif

#ifdef CONFIG_BMA_ENABLE_NEWDATA_INT
static void bma25x_read_new_data(struct bma25x_data *bma25x)
{
	struct bma25xacc value;

	bma25x_report_axis_data(bma25x, &value);
	mutex_lock(&bma25x->value_mutex);
	bma25x->value = value;
	mutex_unlock(&bma25x->value_mutex);
	return;
}

static int bma25x_data_ready_handle(struct bma25x_data *bma25x)
{
	int ret;
	unsigned char status = 0;

	ret = bma25x_get_interruptstatus2(bma25x->bma25x_client, &status);
	if (ret) {
		dev_err(&bma25x->bma25x_client->dev,
				"read interrupt status2 err, err=%d\n", ret);
		return -EIO;
	}

	if ((status & 0x80) == 0x80) {
		bma25x_read_new_data(bma25x);
		return 0;
	}

	if (status != 0) {
		dev_dbg(&bma25x->bma25x_client->dev,
				"Interrupt flag is detected, state2 =0x%x\n",
				status);
		return -EAGAIN;
	}

	/* Check if any other interrupt is triggered. */
	ret = bma25x_get_interruptstatus1(bma25x->bma25x_client,
			&status);
	if (ret) {
		dev_err(&bma25x->bma25x_client->dev,
				"read interrupt status1 err, err=%d\n", ret);
		return -EIO;
	}

	/*
	 * Read new data if no other interrupt is triggered.
	 * BMA2x2 data ready flag will be cleared if new data acquisition
	 * is started, sometimes we cannot get that flag.
	 */
	if (status == 0) {
		bma25x_read_new_data(bma25x);
		return 0;
	}

	dev_dbg(&bma25x->bma25x_client->dev,
		"Data ready int is not detected, state1 =0x%x\n", status);
	return -EAGAIN;
}
#else
static int bma25x_data_ready_handle(struct bma25x_data *bma25x)
{
	return -EAGAIN;
}
#endif

static void bma25x_irq_work_func(struct work_struct *work)
{
	struct bma25x_data *bma25x =
		container_of((struct work_struct *)work,
				struct bma25x_data, irq_work);

	unsigned char status = 0;
	unsigned char first_value = 0;
	unsigned char sign_value = 0;

	if (bma25x_data_ready_handle(bma25x) != -EAGAIN)
		return;

	bma25x_get_interruptstatus1(bma25x->bma25x_client, &status);
	ISR_INFO(&bma25x->bma25x_client->dev,
		"bma25x_irq_work_func, status = 0x%x, ret=%d\n", status);

	switch (status) {
	case 0x01:
		ISR_INFO(&bma25x->bma25x_client->dev,
				"Low G interrupt happened\n");
		input_report_rel(bma25x->dev_interrupt, LOW_G_INTERRUPT,
				LOW_G_INTERRUPT_HAPPENED);
		break;
	case 0x02:
		bma25x_high_g_interrupt_handle(bma25x);
		break;
	case 0x04:
		bma25x_slope_interrupt_handle(bma25x);
		break;
	case 0x08:
		ISR_INFO(&bma25x->bma25x_client->dev,
				"slow/ no motion interrupt happened\n");
		input_report_rel(bma25x->dev_interrupt,
				SLOW_NO_MOTION_INTERRUPT,
				SLOW_NO_MOTION_INTERRUPT_HAPPENED);
		break;
#ifndef CONFIG_DOUBLE_TAP
	case 0x10:
		ISR_INFO(&bma25x->bma25x_client->dev,
				"double tap interrupt happened\n");
		input_report_rel(bma25x->dev_interrupt,
				DOUBLE_TAP_INTERRUPT,
				DOUBLE_TAP_INTERRUPT_HAPPENED);
		break;
	case 0x20:
		ISR_INFO(&bma25x->bma25x_client->dev,
				"single tap interrupt happened\n");
		input_report_rel(bma25x->dev_interrupt,
				SINGLE_TAP_INTERRUPT,
				SINGLE_TAP_INTERRUPT_HAPPENED);
		break;
#endif
	case 0x40:
		bma25x_get_orient_status(bma25x->bma25x_client,
				&first_value);
		ISR_INFO(&bma25x->bma25x_client->dev,
				"orient interrupt happened,%s\n",
				orient[first_value]);
		if (first_value == 0)
			input_report_abs(bma25x->dev_interrupt,
			ORIENT_INTERRUPT,
			UPWARD_PORTRAIT_UP_INTERRUPT_HAPPENED);
		else if (first_value == 1)
			input_report_abs(bma25x->dev_interrupt,
			ORIENT_INTERRUPT,
			UPWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED);
		else if (first_value == 2)
			input_report_abs(bma25x->dev_interrupt,
			ORIENT_INTERRUPT,
			UPWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED);
		else if (first_value == 3)
			input_report_abs(bma25x->dev_interrupt,
			ORIENT_INTERRUPT,
			UPWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED);
		else if (first_value == 4)
			input_report_abs(bma25x->dev_interrupt,
			ORIENT_INTERRUPT,
			DOWNWARD_PORTRAIT_UP_INTERRUPT_HAPPENED);
		else if (first_value == 5)
			input_report_abs(bma25x->dev_interrupt,
			ORIENT_INTERRUPT,
			DOWNWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED);
		else if (first_value == 6)
			input_report_abs(bma25x->dev_interrupt,
			ORIENT_INTERRUPT,
			DOWNWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED);
		else if (first_value == 7)
			input_report_abs(bma25x->dev_interrupt,
			ORIENT_INTERRUPT,
			DOWNWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED);
		break;
	case 0x80:
		bma25x_get_orient_flat_status(bma25x->bma25x_client,
				&sign_value);
		ISR_INFO(&bma25x->bma25x_client->dev,
				"flat interrupt happened,flat status is %d\n",
				sign_value);
		if (sign_value == 1) {
			input_report_abs(bma25x->dev_interrupt,
					FLAT_INTERRUPT,
					FLAT_INTERRUPT_TURE_HAPPENED);
		} else {
			input_report_abs(bma25x->dev_interrupt,
					FLAT_INTERRUPT,
					FLAT_INTERRUPT_FALSE_HAPPENED);
		}
		break;
	default:
		break;
	}
}

static irqreturn_t bma25x_irq_handler(int irq, void *handle)
{
	struct bma25x_data *data = handle;

	if (data == NULL)
		return IRQ_HANDLED;
	if (data->bma25x_client == NULL)
		return IRQ_HANDLED;

	queue_work(data->data_wq, &data->irq_work);

	return IRQ_HANDLED;
}
#else
static void bma25x_irq_work_func(struct work_struct *work)
{
	struct bma25x_data *bma25x = container_of((struct work_struct *)work,
			struct bma25x_data, irq_work);

	dev_dbg(&bma25x->bma25x_client->dev,
			"Interrupt feature is not enabled!\n");
	return;
}

static irqreturn_t bma25x_irq_handler(int irq, void *handle)
{
	struct bma25x_data *bma25x = handle;

	dev_dbg(&bma25x->bma25x_client->dev,
			"Interrupt feature is not enabled!\n");
	return IRQ_HANDLED;
}
#endif /* defined(BMA25X_ENABLE_INT1)||defined(BMA25X_ENABLE_INT2) */

static int bma25x_power_ctl(struct bma25x_data *data, bool on)
{
	int ret = 0;
	int err = 0;
	if (!on && data->power_enabled) {
		ret = regulator_disable(data->vdd);
		if (ret) {
			dev_err(&data->bma25x_client->dev,
				"Regulator vdd disable failed ret=%d\n", ret);
			return ret;
		}
		dev_err(&data->bma25x_client->dev,
			"bma25x_power_ctl entry222!!\n");

		ret = regulator_disable(data->vio);
		if (ret) {
			dev_err(&data->bma25x_client->dev,
				"Regulator vio disable failed ret=%d\n", ret);
			err = regulator_enable(data->vdd);
			return ret;
		}
		data->power_enabled = on;
	} else if (on && !data->power_enabled) {
		ret = regulator_enable(data->vdd);
		if (ret) {
			dev_err(&data->bma25x_client->dev,
				"Regulator vdd enable failed ret=%d\n", ret);
			return ret;
		}
		ret = regulator_enable(data->vio);
		if (ret) {
			dev_err(&data->bma25x_client->dev,
				"Regulator vio enable failed ret=%d\n", ret);
			err = regulator_disable(data->vdd);
			return ret;
		}
		data->power_enabled = on;
	} else {
		dev_info(&data->bma25x_client->dev,
			"Power on=%d. enabled=%d\n", on, data->power_enabled);
	}
	return ret;
}

static int bma25x_power_init(struct bma25x_data *data)
{
	int ret;

	data->vdd = regulator_get(&data->bma25x_client->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		ret = PTR_ERR(data->vdd);
		dev_err(&data->bma25x_client->dev,
				"Regulator get failed vdd ret=%d\n", ret);
		return ret;
	}

	if (regulator_count_voltages(data->vdd) > 0) {
		ret = regulator_set_voltage(data->vdd,
			BMA2x2_VDD_MIN_UV, BMA2x2_VDD_MAX_UV);
		if (ret) {
			dev_err(&data->bma25x_client->dev,
				"Regulator set failed vdd ret=%d\n", ret);
			goto reg_vdd_put;
		}
	}

	data->vio = regulator_get(&data->bma25x_client->dev, "vio");
	if (IS_ERR(data->vio)) {
		ret = PTR_ERR(data->vio);
		dev_err(&data->bma25x_client->dev,
				"Regulator get failed vio ret=%d\n", ret);
		goto reg_vdd_set;
	}

	if (regulator_count_voltages(data->vio) > 0) {
		ret = regulator_set_voltage(data->vio,
				BMA2x2_VIO_MIN_UV, BMA2x2_VIO_MAX_UV);
		if (ret) {
			dev_err(&data->bma25x_client->dev,
				"Regulator set failed vio ret=%d\n", ret);
			goto reg_vio_put;
		}
	}
	return 0;

reg_vio_put:
	regulator_put(data->vio);
reg_vdd_set:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, BMA2x2_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return ret;
}

static int bma25x_power_deinit(struct bma25x_data *data)
{
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, BMA2x2_VDD_MAX_UV);

	regulator_put(data->vdd);

	if (regulator_count_voltages(data->vio) > 0)
		regulator_set_voltage(data->vio, 0, BMA2x2_VIO_MAX_UV);

	regulator_put(data->vio);

	return 0;
}

#ifdef CONFIG_OF
static int bma25x_parse_dt(struct device *dev,
	struct bma25x_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	u32 temp_val;
	int rc;

	rc = of_property_read_u32(np, "bosch,init-interval", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read init-interval\n");
		return rc;
	} else {
		pdata->poll_interval = temp_val;
	}

	rc = of_property_read_u32(np, "bosch,place", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read sensor place paramater\n");
		return rc;
	}
	if (temp_val > 7 || temp_val < 0) {
		dev_err(dev, "Invalid place parameter, use default value 0\n");
		pdata->place = 0;
	} else {
		pdata->place = temp_val;
	}

	pdata->int_en = of_property_read_bool(np, "bosch,use-interrupt");
	pdata->use_int2 = of_property_read_bool(np, "bosch,use-int2");
	pdata->use_hrtimer = of_property_read_bool(np, "bosch,use-hrtimer");
	pdata->gpio_int1 = of_get_named_gpio_flags(dev->of_node,
			"bosch,gpio-int1", 0, &pdata->int1_flag);
	/* pdata->gpio_int2 = of_get_named_gpio_flags(dev->of_node,
	   "bosch,gpio-int2", 0, &pdata->int2_flag);*/
	return 0;
}
#else
static int bma25x_parse_dt(struct device *dev,
		struct bma25x_platform_data *pdata)
{
	return -EINVAL;
}
#endif

static void bma25x_double_tap_disable(struct bma25x_data *data)
{
	return;
}
static int bma25x_open_init(struct i2c_client *client, struct bma25x_data *data)
{
	int err;

	err = bma25x_set_bandwidth(client, data->bandwidth);
	if (err < 0) {
		dev_err(&client->dev, "init bandwidth error\n");
		return err;
	}
	err = bma25x_set_range(client, data->range);
	if (err < 0) {
		dev_err(&client->dev, "init bandwidth error\n");
		return err;
	}
	return 0;
}

static int bma25x_get_interrupt_gpio(const struct bma25x_data *data,
	const unsigned int gpio)
{
	struct i2c_client *client = data->bma25x_client;
	int err;

	if (!gpio_is_valid(gpio)) {
		dev_err(&client->dev, "gpio(%d) is invalid,\n", gpio);
		return -EINVAL;
	}

	err = gpio_request(gpio, "bma25x_gpio_int");
	if (err) {
		dev_err(&client->dev,
			"Unable to request gpio %d, err=%d\n", gpio, err);
		return err;
	}

	err = gpio_direction_input(gpio);
	if (err) {
		dev_err(&client->dev,
			"Unable to set gpio direction %d, err=%d\n", gpio, err);
		gpio_free(gpio);
		return err;
	}

	client->irq = gpio_to_irq(gpio);
	dev_dbg(&client->dev,
			"Interrupt gpio=%d, irq=%d\n", gpio, client->irq);

	return 0;
}

static int bma25x_pinctrl_init(struct bma25x_data *data)
{
	struct i2c_client *client = data->bma25x_client;
	struct bma25x_pinctrl_data *pctrl_data;
	struct pinctrl *pctrl;
	int ret = 0;

	pctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(pctrl)) {
		ret = PTR_ERR(pctrl);
		dev_err(&client->dev,
				"Failed to get pin pinctrl, err:%d\n", ret);
		goto exit;
	}
	pctrl_data = devm_kzalloc(&client->dev,
			sizeof(*pctrl_data), GFP_KERNEL);
	if (!pctrl_data) {
		dev_err(&client->dev, "No memory for pinctrl data\n");
		ret = -ENOMEM;
		goto exit;
	}
	pctrl_data->pctrl = pctrl;

	pctrl_data->pins_default = pinctrl_lookup_state(pctrl, "default");
	if (IS_ERR_OR_NULL(pctrl_data->pins_default)) {
		ret = PTR_ERR(pctrl_data->pins_default);
		dev_err(&client->dev,
		"Could not get default pinstate, err:%d\n", ret);
		goto exit;
	}
	/* "sleep" state is optional to compatible with old config  */
	pctrl_data->pins_sleep = pinctrl_lookup_state(pctrl, "sleep");
	if (IS_ERR_OR_NULL(pctrl_data->pins_sleep)) {
		dev_info(&client->dev,
				"Could not get sleep pinstate, err:%ld\n",
				PTR_ERR(pctrl_data->pins_sleep));
		pctrl_data->pins_sleep = NULL;
	}
	data->pctrl_data = pctrl_data;

exit:
	return ret;
}

static void bma25x_pinctrl_state(struct bma25x_data *data, bool active)
{
	struct device dev = data->bma25x_client->dev;
	int ret;

	if (!data->pctrl_data)
		return;

	if (active) {
		ret = pinctrl_select_state(data->pctrl_data->pctrl,
				data->pctrl_data->pins_default);
		if (ret)
			dev_info(&dev, "Select default pinstate err:%d\n", ret);
	} else {
		if (!data->pctrl_data->pins_sleep) {
			dev_dbg(&dev, "Pinstate 'sleep' is not defined\n");
		} else {
			ret = pinctrl_select_state(data->pctrl_data->pctrl,
					data->pctrl_data->pins_sleep);
			if (ret)
				dev_info(&dev,
				"Select sleep pinstate err:%d\n", ret);
		}
	}
	dev_dbg(&dev, "Select pinctrl state=%d\n", active);
}

static int bma25x_eeprom_prog(struct i2c_client *client)
{
	int res = 0, timeout = 0;
	unsigned char databuf = 0;
	res = bma25x_smbus_read_byte(client,
			BMA25X_EEPROM_CTRL_REG, &databuf);
	if (res < 0) {
		dev_err(&client->dev, "read eeprom control reg error1\n");
		return res;
	}
	databuf |= 0x01;
	res = bma25x_smbus_write_byte(client,
			BMA25X_EEPROM_CTRL_REG, &databuf);
	if (res < 0) {
		dev_err(&client->dev, "write eeprom control reg error1\n");
		return res;
	}

	res = bma25x_smbus_read_byte(client,
			BMA25X_EEPROM_CTRL_REG, &databuf);
	if (res < 0) {
		dev_err(&client->dev, "read eeprom control reg error2\n");
		return res;
	}
	databuf |= 0x02;
	res = bma25x_smbus_write_byte(client,
			BMA25X_EEPROM_CTRL_REG, &databuf);
	if (res < 0) {
		dev_err(&client->dev, "write eeprom control reg error2\n");
		return res;
	}
	do {
		WAIT_CAL_READY();
		res = bma25x_smbus_read_byte(client,
				BMA25X_EEPROM_CTRL_REG, &databuf);
		if (res < 0) {
			dev_err(&client->dev, "read nvm_rdy error\n");
			return res;
		}
		databuf = (databuf >> 2) & 0x01;
		if (++timeout == 50) {
			dev_err(&client->dev, "check nvm_rdy time out\n");
			break;
		}
	} while (databuf == 0);

	res = bma25x_smbus_read_byte(client,
			BMA25X_EEPROM_CTRL_REG, &databuf);
	if (res < 0) {
		dev_err(&client->dev, "read eeprom control reg error3\n");
		return res;
	}
	databuf &= 0xFE;
	res = bma25x_smbus_write_byte(client,
			BMA25X_EEPROM_CTRL_REG, &databuf);
	if (res < 0) {
		dev_err(&client->dev, "write eeprom control reg error3\n");
		return res;
	}
	return res;
}

static int bma25x_self_calibration_xyz(struct sensors_classdev *
		sensors_cdev, int axis, int apply_now)
{
	int error;
	bool pre_enable;
	struct bma25x_data *data = container_of(sensors_cdev,
			struct bma25x_data, cdev);
	struct i2c_client *client = data->bma25x_client;

	pre_enable = atomic_read(&data->enable);
	if (pre_enable)
		bma25x_set_enable(&client->dev, 0);

	if (atomic_cmpxchg(&data->cal_status, 0, 1)) {
		dev_err(&client->dev, "do calibration error\n");
		return -EBUSY;
	}

	error = bma25x_power_ctl(data, true);
	if (error) {
		dev_err(&client->dev, "Failed to enable sensor power\n");
		error = -EINVAL;
		goto exit;
	}

	dev_dbg(&client->dev, "xyz axis fast calibration finished\n");
	error = bma25x_eeprom_prog(client);
	if (error < 0) {
		dev_err(&client->dev, "wirte calibration to eeprom failed\n");
		goto exit;
	}
	snprintf(data->calibrate_buf,
		sizeof(data->calibrate_buf), "%d,%d,%d", 0, 0, 0);
	sensors_cdev->params = data->calibrate_buf;

	error = bma25x_power_ctl(data, false);
	if (error) {
		dev_err(&client->dev, "Failed to disable sensor power\n");
		goto exit;
	}

exit:
	atomic_set(&data->cal_status, 0);
	if (pre_enable)
		bma25x_set_enable(&client->dev, 1);

	return error;
}

static int bma25x_write_cal_params(struct sensors_classdev *sensors_cdev,
		struct cal_result_t *cal_result)
{
	struct bma25x_data *data =
		container_of(sensors_cdev, struct bma25x_data, cdev);

	snprintf(data->calibrate_buf,
			sizeof(data->calibrate_buf), "%d,%d,%d", 0, 0, 0);
	sensors_cdev->params = data->calibrate_buf;
	return 0;
}

static int bma25x_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0;
	struct bma25x_data *data;
	struct input_dev *dev;
	struct bst_dev  *dev_acc;
	struct bma25x_platform_data *pdata;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c_check_functionality error\n");
		err = -EPERM;
		goto exit;
	}
	data = kzalloc(sizeof(struct bma25x_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allcated memory\n");
			err = -ENOMEM;
			goto kfree_exit;
		}
		err = bma25x_parse_dt(&client->dev, pdata);
		if (err) {
			dev_err(&client->dev, "Failed to parse device tree\n");
			err = -EINVAL;
			goto pdata_free_exit;
		}
	} else {
		pdata = client->dev.platform_data;
		dev_err(&client->dev, "Use platform data\n");
	}

	if (!pdata) {
		dev_err(&client->dev, "Cannot get device platform data\n");
		err = -EINVAL;
		goto kfree_exit;
	}
	data->pdata = pdata;
	i2c_set_clientdata(client, data);
	data->bma25x_client = client;

	err = bma25x_power_init(data);
	if (err) {
		dev_err(&client->dev, "Failed to get sensor regulators\n");
		err = -EINVAL;
		goto free_i2c_clientdata_exit;
	}
	err = bma25x_power_ctl(data, true);
	if (err) {
		dev_err(&client->dev, "Failed to enable sensor power\n");
		err = -EINVAL;
		goto deinit_power_exit;
	}

	usleep(5000);
	if (bma25x_soft_reset(client) < 0) {
		dev_err(&client->dev,
				"i2c bus write error, pls check HW connection\n");
		err = -EINVAL;
		goto disable_power_exit;
	}
	usleep(5000);
	/* read and check chip id */
	if (bma25x_check_chip_id(client, data) < 0) {
		err = -EINVAL;
		goto disable_power_exit;
	}
	mutex_init(&data->value_mutex);
	mutex_init(&data->mode_mutex);
	mutex_init(&data->enable_mutex);
	mutex_init(&data->op_lock);
	data->bandwidth = BMA25X_BW_SET;
	data->range = BMA25X_RANGE_SET;
	data->sensitivity = bosch_sensor_range_map[0];
	atomic_set(&data->cal_status, 0);
	err = bma25x_open_init(client, data);
	if (err < 0) {
		err = -EINVAL;
		goto disable_power_exit;
	}

	if (pdata->int_en) {
		/* check interrupt feature enable state */
		err = bma25x_pinctrl_init(data);
		if (err) {
			dev_err(&client->dev,
				"Failed to init pinctrl err=%d\n", err);
			err = -EINVAL;
			goto disable_power_exit;
		}
		/* check interrupt feature enable state */
		if ((pdata->use_int2 && (!BMA2x2_IS_INT2_ENABLED())) ||
			(!pdata->use_int2 && (!BMA2x2_IS_INT1_ENABLED()))) {
			dev_err(&client->dev,
			"Interrupt support is not enabled, int1=%d, int2=%d use_int2=%d\n",
			BMA2x2_IS_INT1_ENABLED(),
			BMA2x2_IS_INT2_ENABLED(), pdata->use_int2);
			err = -EINVAL;
			goto disable_power_exit;
		}

		if (pdata->use_int2) {
			data->int_flag = pdata->int2_flag;
			err = bma25x_get_interrupt_gpio(data, pdata->gpio_int2);
		} else {
			data->int_flag = pdata->int1_flag;
			err = bma25x_get_interrupt_gpio(data, pdata->gpio_int1);
		}
		if (err) {
			dev_err(&client->dev,
				"Failed to get interrupt gpio, err=%d\n", err);
			err = -EINVAL;
			goto set_pinctrl_sleep;
		}

		data->IRQ = client->irq;
		if (!data->int_flag)
			data->int_flag = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		dev_dbg(&client->dev, "IRQ=%d, use_int2=%d, int_flag=0x%x\n",
				data->IRQ, pdata->use_int2, data->int_flag);

		err = request_irq(data->IRQ, bma25x_irq_handler,
			data->int_flag, "bma25x", data);
		if (err) {
			dev_err(&client->dev,  "Could not request irq\n");
			goto free_interrupt_gpio;
		}
		disable_irq(data->IRQ);
		INIT_WORK(&data->irq_work, bma25x_irq_work_func);
	} else {

		if (!pdata->use_hrtimer) {
			INIT_DELAYED_WORK(&data->work, bma25x_work_func);
		} else {
			hrtimer_init(&data->accel_timer, CLOCK_BOOTTIME,
					HRTIMER_MODE_REL);
			data->accel_timer.function = accel_timer_handle;

			init_waitqueue_head(&data->accel_wq);
			data->accel_wkp_flag = 0;
			data->accel_task = kthread_run(accel_poll_thread, data,
					"bma_accel");
		}
	}

	data->data_wq = create_freezable_workqueue("bma25x_data_work");
	if (!data->data_wq) {
		dev_err(&client->dev, "Cannot get create workqueue!\n");
		goto free_irq_exit;
	}

	atomic_set(&data->delay, POLL_DEFAULT_INTERVAL_MS);
	atomic_set(&data->enable, 0);

	dev = devm_input_allocate_device(&client->dev);
	if (!dev) {
		dev_err(&client->dev, "Cannot allocate input device\n");
		err = -ENOMEM;
		goto destroy_workqueue_exit;
	}
	/* only value events reported */
	dev->name = SENSOR_NAME;
	dev->id.bustype = BUS_I2C;
	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_abs_params(dev, ABS_X, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Y, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Z, ABSMIN, ABSMAX, 0, 0);

	input_set_drvdata(dev, data);
	err = input_register_device(dev);
	if (err < 0) {
		dev_err(&client->dev, "Cannot register input device\n");
		goto free_irq_exit;
	}
	data->input = dev;
	err = sysfs_create_group(&data->input->dev.kobj,
			&bma25x_attribute_group);
	if (err < 0) {
		dev_err(&client->dev, "Cannot create sysfs for bma25x\n");
		goto remove_dtap_sysfs_exit;
	}

	dev_acc = bst_allocate_device();
	if (!dev_acc) {
		dev_err(&client->dev, "Cannot allocate bst device\n");
		err = -ENOMEM;
		goto remove_bma25x_sysfs_exit;
	}
	dev_acc->name = ACC_NAME;

	bst_set_drvdata(dev_acc, data);

	err = bst_register_device(dev_acc);
	if (err < 0) {
		dev_err(&client->dev, "Cannot register bst device\n");
		goto bst_free_acc_exit;
	}

	data->bst_acc = dev_acc;
	err = sysfs_create_group(&data->bst_acc->dev.kobj,
			&bma25x_attribute_group);

	if (err < 0) {
		dev_err(&client->dev, "Cannot create sysfs for bst_acc.\n");
		goto bst_free_exit;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = bma25x_early_suspend;
	data->early_suspend.resume = bma25x_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	data->ref_count = 0;
	data->fifo_datasel = 0;
	data->fifo_count = 0;
	data->cdev = sensors_cdev;
	data->cdev.min_delay = POLL_INTERVAL_MIN_MS * 1000;
	data->cdev.delay_msec = pdata->poll_interval;
	data->cdev.sensors_enable = bma25x_cdev_enable;
	data->cdev.sensors_poll_delay = bma25x_cdev_poll_delay;
	data->cdev.sensors_calibrate = bma25x_self_calibration_xyz;
	data->cdev.sensors_write_cal_params = bma25x_write_cal_params;
	data->cdev.resolution =
		sensor_type_map[data->chip_type].resolution;
	if (pdata->int_en)
		data->cdev.max_delay = BMA_INT_MAX_DELAY;
	err = sensors_classdev_register(&data->input->dev, &data->cdev);
	if (err) {
		dev_err(&client->dev, "create class device file failed!\n");
		err = -EINVAL;
		goto remove_bst_acc_sysfs_exit;
	}

	dev_notice(&client->dev, "BMA2x2 driver probe successfully");
	if (pdata->int_en)
		bma25x_pinctrl_state(data, false);
	bma25x_power_ctl(data, false);
	return 0;

remove_bst_acc_sysfs_exit:
	sysfs_remove_group(&data->bst_acc->dev.kobj,
			&bma25x_attribute_group);
bst_free_exit:
	bst_unregister_device(dev_acc);
bst_free_acc_exit:
	bst_free_device(dev_acc);
remove_bma25x_sysfs_exit:
	sysfs_remove_group(&data->input->dev.kobj,
			&bma25x_attribute_group);
remove_dtap_sysfs_exit:

destroy_workqueue_exit:
	if (!pdata->use_hrtimer) {
		destroy_workqueue(data->data_wq);
	} else {
		hrtimer_cancel(&data->accel_timer);
		kthread_stop(data->accel_task);
	}
free_irq_exit:
free_interrupt_gpio:
	if (pdata->int_en) {
		if (pdata->use_int2)
			gpio_free(pdata->gpio_int2);
		else
			gpio_free(pdata->gpio_int1);
	}
set_pinctrl_sleep:
	if (pdata->int_en)
		bma25x_pinctrl_state(data, false);
disable_power_exit:
	bma25x_power_ctl(data, false);
deinit_power_exit:
	bma25x_power_deinit(data);
free_i2c_clientdata_exit:
	i2c_set_clientdata(client, NULL);
pdata_free_exit:
	if (pdata && (client->dev.of_node))
		devm_kfree(&client->dev, pdata);
	data->pdata = NULL;
kfree_exit:
	kfree(data);
exit:
	return err;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bma25x_early_suspend(struct early_suspend *h)
{
	struct bma25x_data *data =
		container_of(h, struct bma25x_data, early_suspend);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		bma25x_set_mode(data->bma25x_client, BMA25X_MODE_SUSPEND);
#ifndef CONFIG_BMA_ENABLE_NEWDATA_INT
		cancel_delayed_work_sync(&data->work);
#endif
	}
	mutex_unlock(&data->enable_mutex);
}

static void bma25x_late_resume(struct early_suspend *h)
{
	struct bma25x_data *data =
		container_of(h, struct bma25x_data, early_suspend);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		bma25x_set_mode(data->bma25x_client, BMA25X_MODE_NORMAL);
#ifndef CONFIG_BMA_ENABLE_NEWDATA_INT
		schedule_delayed_work(&data->work,
				msecs_to_jiffies(atomic_read(&data->delay)));
#endif
	}
	mutex_unlock(&data->enable_mutex);
}
#endif

static int bma25x_remove(struct i2c_client *client)
{
	struct bma25x_data *data = i2c_get_clientdata(client);

	sensors_classdev_unregister(&data->cdev);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif

	if (data->bst_acc) {
		bst_unregister_device(data->bst_acc);
		bst_free_device(data->bst_acc);
	}

	bma25x_double_tap_disable(data);
	if (data->input)
		sysfs_remove_group(&data->input->dev.kobj,
				&bma25x_attribute_group);

	bma25x_set_enable(&client->dev, 0);
	if (!data->pdata->use_hrtimer) {
		destroy_workqueue(data->data_wq);
	} else {
		hrtimer_cancel(&data->accel_timer);
		kthread_stop(data->accel_task);
	}
	bma25x_power_deinit(data);
	i2c_set_clientdata(client, NULL);
	if (data->pdata && (client->dev.of_node))
		devm_kfree(&client->dev, data->pdata);
	data->pdata = NULL;
	kfree(data);
	return 0;
}

void bma25x_shutdown(struct i2c_client *client)
{
	struct bma25x_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->enable_mutex);
	bma25x_set_mode(data->bma25x_client, BMA25X_MODE_DEEP_SUSPEND);
	mutex_unlock(&data->enable_mutex);
}

#ifdef CONFIG_PM
static int bma25x_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct bma25x_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		bma25x_set_mode(data->bma25x_client, BMA25X_MODE_SUSPEND);
#ifndef CONFIG_BMA_ENABLE_NEWDATA_INT
		cancel_delayed_work_sync(&data->work);
#endif
	}
	mutex_unlock(&data->enable_mutex);
	return 0;
}

static int bma25x_resume(struct i2c_client *client)
{
	struct bma25x_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		bma25x_set_mode(data->bma25x_client, BMA25X_MODE_NORMAL);
#ifndef CONFIG_BMA_ENABLE_NEWDATA_INT
		schedule_delayed_work(&data->work,
				msecs_to_jiffies(atomic_read(&data->delay)));
#endif
	}
	mutex_unlock(&data->enable_mutex);
	return 0;
}
#else
#define bma25x_suspend      NULL
#define bma25x_resume       NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id bma25x_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, bma25x_id);

static const struct of_device_id bma25x_of_match[] = {
	{ .compatible = "bosch,bma253", },
	{ },
};

static struct i2c_driver bma25x_driver = {
	.driver = {
		.owner  = THIS_MODULE,
		.name   = SENSOR_NAME,
		.of_match_table = bma25x_of_match,
	},
	.suspend    = bma25x_suspend,
	.resume     = bma25x_resume,
	.id_table   = bma25x_id,
	.probe      = bma25x_probe,
	.remove     = bma25x_remove,
	.shutdown   = bma25x_shutdown,
};

static int __init BMA25X_init(void)
{
	return i2c_add_driver(&bma25x_driver);
}

static void __exit BMA25X_exit(void)
{
	i2c_del_driver(&bma25x_driver);
}

MODULE_AUTHOR("contact@bosch-sensortec.com");
MODULE_DESCRIPTION("BMA25X ACCELEROMETER SENSOR DRIVER");
MODULE_LICENSE("GPL v2");

module_init(BMA25X_init);
module_exit(BMA25X_exit);
