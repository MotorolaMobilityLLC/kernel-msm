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
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/sensors.h>

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
#include <linux/wakelock.h>
#include <linux/jiffies.h>

#include "bstclass.h"

#define ACC_NAME  "ACC"
#ifdef ENABLE_ISR_DEBUG_MSG
#define ISR_INFO(dev, fmt, arg...) dev_info(dev, fmt, ##arg)
#else
#define ISR_INFO(dev, fmt, arg...)
#endif

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

#define LOW_G_INTERRUPT            ABS_DISTANCE
#define HIGH_G_INTERRUPT            REL_HWHEEL
#define SLOP_INTERRUPT              REL_DIAL
#define DOUBLE_TAP_INTERRUPT        REL_WHEEL
#define SINGLE_TAP_INTERRUPT        REL_MISC
#define ORIENT_INTERRUPT            ABS_PRESSURE
#define FLAT_INTERRUPT               REL_Z
#define SLOW_NO_MOTION_INTERRUPT    REL_Y
#define REL_TIME_SEC		REL_RX
#define REL_TIME_NSEC	REL_RY
#define REL_FLUSH	REL_RZ
#define REL_INT_FLUSH	REL_X

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

#define FLATUP_GESTURE 0xF2
#define FLATDOWN_GESTURE 0xF4
#define EXIT_FLATUP_GESTURE 0xE2
#define EXIT_FLATDOWN_GESTURE 0xE4
#define GLANCE_EXIT_FLATUP_GESTURE 0x02
#define GLANCE_EXIT_FLATDOWN_GESTURE 0x04
#define GLANCE_MOVEMENT_GESTURE 0x10

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

#define BMA25X_SLO_NO_MOT_DUR__POS   2
#define BMA25X_SLO_NO_MOT_DUR__LEN   6
#define BMA25X_SLO_NO_MOT_DUR__MSK   0xFC
#define BMA25X_SLO_NO_MOT_DUR__REG   BMA25X_SLOPE_DURN_REG

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
#define BMA25X_VDD_MIN_UV       2000000
#define BMA25X_VDD_MAX_UV       3400000
#define BMA25X_VIO_MIN_UV       1500000
#define BMA25X_VIO_MAX_UV       3400000

/* Polling delay in msecs */
#define POLL_INTERVAL_MIN_MS        10
#define POLL_INTERVAL_MAX_MS        4000
#define POLL_DEFAULT_INTERVAL_MS 200

#define POLL_MS_100HZ 10

/* Interrupt delay in msecs */
#define BMA_INT_MAX_DELAY        64

#define MAX_RANGE_MAP        4

#define BMA_CAL_BUF_SIZE        99

#define TEST_BIT(pos, number) (number & (1 << pos))

#define BMA25X_POWER_ALWAYS_ON

#define BMA25X_MAP_FLAT_TO_INT2

enum {
	BMA25X_AOD,
	BMA25X_ACC,
};

enum {
	FlatUp = 0,
	FlatDown,
	Motion,
	numSensors   /* This needs to be at the end of the list */
};

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
	char calibrate_buf[BMA_CAL_BUF_SIZE];
	unsigned int chip_id;
	unsigned int chip_type;
	unsigned int fifo_count;
	unsigned char fifo_datasel;
	unsigned char mode;
	signed char sensor_type;
	struct input_dev *input;
	struct input_dev *dev_interrupt;
	struct bst_dev *bst_acc;
	struct bma25xacc value;
	struct mutex enable_mutex;
	struct mutex mode_mutex;
	struct mutex int_mode_mutex;
	struct workqueue_struct *data_wq;
	struct work_struct work;
	struct hrtimer timer;
	ktime_t work_delay_kt;
	struct work_struct int1_irq_work;
	struct work_struct int2_irq_work;
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
	int IRQ1;
	int IRQ2;
	struct bma25x_platform_data *pdata;
	struct bma25x_suspend_state suspend_state;
	struct bma25x_pinctrl_data *pctrl_data;
#ifdef BMA25X_ENABLE_INT1
	atomic_t flat_flag;
	int mEnabled;
	int flat_threshold;
	int aod_flag;
	int flat_up_value;
	int flat_down_value;
	struct delayed_work flat_work;
	struct wake_lock aod_wakelock;
#endif

	int read_flag;
	u8 read_reg;

	int ref_count;
};

static int bma25x_power_ctl(struct bma25x_data *data, bool on);
static int bma25x_get_sensitivity(struct bma25x_data *bma25x, int range);
#if defined(BMA25X_ENABLE_INT1) || defined(BMA25X_ENABLE_INT2)
static int bma25x_set_Int_Enable(struct i2c_client *client, unsigned char
		InterruptType , unsigned char value);
#endif
#ifdef BMA25X_ENABLE_INT1
static int bma25x_set_int1_pad_sel(struct i2c_client *client,
		unsigned char int1sel);
#ifdef BMA25X_MAP_FLAT_TO_INT2
static int bma25x_set_int2_pad_sel(struct i2c_client *client,
		unsigned char int2sel);
#endif
#endif

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
	int bitwidth;
	comres = bma25x_smbus_read_byte_block(client,
			BMA25X_ACC_X12_LSB__REG, data, 6);
	if (sensor_type >= 4)
		return -EINVAL;

	acc->x = (data[1]<<8)|data[0];
	acc->y = (data[3]<<8)|data[2];
	acc->z = (data[5]<<8)|data[4];

	bitwidth = bma25x_sensor_bitwidth[sensor_type];
	acc->x = (acc->x >> (16 - bitwidth));
	acc->y = (acc->y >> (16 - bitwidth));
	acc->z = (acc->z >> (16 - bitwidth));
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
	input_report_rel(bma25x->input,
			REL_TIME_SEC, ktime_to_timespec(ts).tv_sec);
	input_report_rel(bma25x->input,
			REL_TIME_NSEC, ktime_to_timespec(ts).tv_nsec);
	input_sync(bma25x->input);
}
#ifndef BMA25X_ENABLE_INT2
static void bma25x_work_func(struct work_struct *work)
{
	struct bma25x_data *data = container_of((struct work_struct *)work,
		struct bma25x_data, work);
	struct bma25xacc value;

	bma25x_report_axis_data(data, &value);
	data->value = value;
}

enum hrtimer_restart poll_function(struct hrtimer *timer)
{
	struct bma25x_data *data = container_of((struct hrtimer *)timer,
		struct bma25x_data, timer);
	int32_t delay = 0;

	delay = atomic_read(&data->delay);
	queue_work(data->data_wq, &data->work);
	data->work_delay_kt = ns_to_ktime(delay*1000000);
	hrtimer_forward_now(&data->timer, data->work_delay_kt);

	return HRTIMER_RESTART;
}
#endif

#ifdef BMA25X_ENABLE_INT1
static void bma25x_set_powermode(struct bma25x_data *bma25x, int enable, int who)
{
	unsigned char databuf = 0, mode = 0;

	mutex_lock(&bma25x->mode_mutex);
	mode = bma25x->mode;
	if (enable)
		mode |= (1<<who);
	else
		mode &= (~(1<<who));
	if (mode && !bma25x->mode) {
		dev_info(&bma25x->bma25x_client->dev,
			"enter normal mode\n");
		if (bma25x_power_ctl(bma25x, true))
			dev_err(&bma25x->bma25x_client->dev, "power failed\n");

		databuf = 0x00;
		bma25x_smbus_write_byte(bma25x->bma25x_client,
			BMA25X_MODE_CTRL_REG, &databuf);
		usleep_range(3000, 3000);
	} else if (!mode && bma25x->mode) {
		dev_info(&bma25x->bma25x_client->dev,
			"enter suspend mode\n");
		databuf = 0x80;
		bma25x_smbus_write_byte(bma25x->bma25x_client,
			BMA25X_MODE_CTRL_REG, &databuf);
	}
	bma25x->mode = mode;
	mutex_unlock(&bma25x->mode_mutex);
}
#endif

static void bma25x_set_enable(struct device *dev, int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);
#ifndef BMA25X_ENABLE_INT1
	unsigned char databuf = 0;
#endif
	int pre_enable = atomic_read(&bma25x->enable);
	mutex_lock(&bma25x->enable_mutex);
	dev_info(dev, "bma25x_set_enable entry ebable:%d pre_enable:%d!!!\n",
		enable, pre_enable);
	if (enable) {
		if (pre_enable == 0) {
			if (bma25x_power_ctl(bma25x, true))
				dev_err(dev, "power failed\n");
#ifdef BMA25X_ENABLE_INT1
			bma25x_set_powermode(bma25x, 1, BMA25X_ACC);
#else
			databuf = 0x00;
			bma25x_smbus_write_byte(bma25x->bma25x_client,
				BMA25X_MODE_CTRL_REG, &databuf);
			usleep_range(3000, 3000);
#endif
#ifndef BMA25X_ENABLE_INT2
		hrtimer_start(&bma25x->timer,
			bma25x->work_delay_kt, HRTIMER_MODE_REL);
#endif
			atomic_set(&bma25x->enable, 1);
		}
	} else {
		if (pre_enable == 1) {
#ifdef BMA25X_ENABLE_INT1
			bma25x_set_powermode(bma25x, 0, BMA25X_ACC);
#else
			databuf = 0x80;
			bma25x_smbus_write_byte(bma25x->bma25x_client,
				BMA25X_MODE_CTRL_REG, &databuf);
#endif
#ifndef BMA25X_ENABLE_INT2
			hrtimer_cancel(&bma25x->timer);
#endif
			atomic_set(&bma25x->enable, 0);
			if (bma25x_power_ctl(bma25x, false))
				dev_err(dev, "power failed\n");
		}
	}
	mutex_unlock(&bma25x->enable_mutex);
	dev_info(dev, "bma25x_set_enable finished!!!\n");
}
#ifdef BMA25X_ENABLE_INT1
static int bma25x_set_theta_flat(struct i2c_client *client, unsigned char
		thetaflat)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma25x_smbus_read_byte(client, BMA25X_THETA_FLAT__REG, &data);
	data = BMA25X_SET_BITSLICE(data, BMA25X_THETA_FLAT, thetaflat);
	comres = bma25x_smbus_write_byte(client, BMA25X_THETA_FLAT__REG, &data);

	return comres;
}

static int bma25x_set_flat_hold_time(struct i2c_client *client, unsigned char
		holdtime)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma25x_smbus_read_byte(client, BMA25X_FLAT_HOLD_TIME__REG,
			&data);
	data = BMA25X_SET_BITSLICE(data, BMA25X_FLAT_HOLD_TIME, holdtime);
	comres = bma25x_smbus_write_byte(client, BMA25X_FLAT_HOLD_TIME__REG,
			&data);

	return comres;
}

static int bma25x_set_slope_no_mot_duration(struct i2c_client *client,
			unsigned char duration)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma25x_smbus_read_byte(client,
			BMA25X_SLO_NO_MOT_DUR__REG, &data);
	data = BMA25X_SET_BITSLICE(data, BMA25X_SLO_NO_MOT_DUR, duration);
	comres = bma25x_smbus_write_byte(client,
			BMA25X_SLO_NO_MOT_DUR__REG, &data);

	return comres;
}

static int bma25x_set_slope_no_mot_threshold(struct i2c_client *client,
		unsigned char threshold)
{
	int comres = 0;
	unsigned char data = 0;

	data = threshold;
	comres = bma25x_smbus_write_byte(client,
			BMA25X_SLO_NO_MOT_THRES_REG, &data);

	return comres;
}

static int bma25x_set_en_no_motion_int(struct bma25x_data *bma25x,
		int en)
{
	int err = 0;
	struct i2c_client *client = bma25x->bma25x_client;

	if (en) {
		/*dur: 192 samples ~= 3s, threshold: 32.25mg, no motion select*/
		err = bma25x_set_slope_no_mot_duration(client, 0x02);
		err += bma25x_set_slope_no_mot_threshold(client, 0x09);
		/*Enable the interrupts*/
		err += bma25x_set_Int_Enable(client, 12, 1);/*slow/no motion X*/
		err += bma25x_set_Int_Enable(client, 13, 1);/*slow/no motion Y*/
		err += bma25x_set_Int_Enable(client, 15, 1);
	} else {
		err = bma25x_set_Int_Enable(client, 12, 0);/*slow/no motion X*/
		err += bma25x_set_Int_Enable(client, 13, 0);/*slow/no motion Y*/
	}
	return err;
}

static int bma25x_flat_update(struct bma25x_data *bma25x)
{
	static struct bma25xacc acc;

	bma25x_read_accel_xyz(bma25x->bma25x_client,
			bma25x->sensor_type, &acc);
	ISR_INFO(&bma25x->bma25x_client->dev,
		"bma25x_flat_updatez value = %d\n", acc.z);
	if (acc.z > bma25x->flat_threshold)
		bma25x->flat_up_value = FLATUP_GESTURE;
	else
		bma25x->flat_up_value = EXIT_FLATUP_GESTURE;
	if (acc.z < (-1 * bma25x->flat_threshold))
		bma25x->flat_down_value = FLATDOWN_GESTURE;
	else
		bma25x->flat_down_value = EXIT_FLATDOWN_GESTURE;

	if (TEST_BIT(FlatUp, bma25x->mEnabled)) {
		dev_info(&bma25x->bma25x_client->dev,
			"update FlatUp value =%d\n", bma25x->flat_up_value);
		input_report_rel(bma25x->dev_interrupt,
				FLAT_INTERRUPT, bma25x->flat_up_value);
		input_sync(bma25x->dev_interrupt);
	}
	if (TEST_BIT(FlatDown, bma25x->mEnabled)) {
		dev_info(&bma25x->bma25x_client->dev,
			"update FlatDown value =%d\n", bma25x->flat_down_value);
		input_report_rel(bma25x->dev_interrupt,
				FLAT_INTERRUPT, bma25x->flat_down_value);
		input_sync(bma25x->dev_interrupt);
	}

	return 0;
}

static int bma25x_set_en_sig_int_mode(struct bma25x_data *bma25x,
		int en)
{
	int err = 0;
	int newstatus = en;
	ISR_INFO(&bma25x->bma25x_client->dev,
			"int_mode entry value = %x  %x\n",
			bma25x->mEnabled, newstatus);
	mutex_lock(&bma25x->int_mode_mutex);

	/* handle flat up/down */
	if (!bma25x->mEnabled && newstatus) {
		bma25x_set_powermode(bma25x, 1, BMA25X_AOD);
		bma25x_set_bandwidth(
			bma25x->bma25x_client, BMA25X_BW_500HZ);
		bma25x_flat_update(bma25x);
		bma25x_set_theta_flat(bma25x->bma25x_client, 0x08);
		if ((bma25x->flat_up_value == FLATUP_GESTURE) ||
			(bma25x->flat_down_value == FLATDOWN_GESTURE))
			bma25x_set_flat_hold_time(bma25x->bma25x_client, 0x00);
		else
			bma25x_set_flat_hold_time(bma25x->bma25x_client, 0x03);
		bma25x_set_Int_Enable(bma25x->bma25x_client, 11, 1);
	} else if (bma25x->mEnabled && !newstatus) {
		bma25x_set_Int_Enable(bma25x->bma25x_client, 11, 0);
		cancel_delayed_work_sync(&bma25x->flat_work);
	}
	if (TEST_BIT(FlatUp, newstatus) &&
			!TEST_BIT(FlatUp, bma25x->mEnabled)) {
		ISR_INFO(&bma25x->bma25x_client->dev,
		"int_mode FlatUp value =%d\n", bma25x->flat_up_value);
		input_report_rel(bma25x->dev_interrupt,
		FLAT_INTERRUPT, bma25x->flat_up_value);
		input_sync(bma25x->dev_interrupt);
	}
	if (TEST_BIT(FlatDown, newstatus) &&
			!TEST_BIT(FlatDown, bma25x->mEnabled)) {
		ISR_INFO(&bma25x->bma25x_client->dev,
		"int_mode FlatDown value =%d\n", bma25x->flat_down_value);
		input_report_rel(bma25x->dev_interrupt,
		FLAT_INTERRUPT, bma25x->flat_down_value);
		input_sync(bma25x->dev_interrupt);
	}

	/* handle motion */
	if (TEST_BIT(Motion, newstatus) &&
			!TEST_BIT(Motion, bma25x->mEnabled))
		err = bma25x_set_en_no_motion_int(bma25x, 1);
	else if (!TEST_BIT(Motion, newstatus) &&
			TEST_BIT(Motion, bma25x->mEnabled))
		err = bma25x_set_en_no_motion_int(bma25x, 0);

	if (!bma25x->mEnabled && newstatus) {
		enable_irq_wake(bma25x->IRQ1);
		enable_irq_wake(bma25x->IRQ2);
	}
	else if (bma25x->mEnabled && !newstatus) {
		disable_irq_wake(bma25x->IRQ1);
		disable_irq_wake(bma25x->IRQ2);
		bma25x_set_bandwidth(
			bma25x->bma25x_client,	bma25x->bandwidth);
		bma25x_set_powermode(bma25x, 0, BMA25X_AOD);
	}

	bma25x->mEnabled = newstatus;
	mutex_unlock(&bma25x->int_mode_mutex);
	ISR_INFO(&bma25x->bma25x_client->dev, "int_mode finished!!!\n");
	return err;
}
#endif
static int bma25x_cdev_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct bma25x_data *data = container_of(sensors_cdev,
			struct bma25x_data, cdev);
#ifdef BMA25X_ENABLE_INT2
	if ((1 == enable) || (0 == enable)) {
		bma25x_set_Int_Enable(data->bma25x_client, 4, enable);
		bma25x_set_enable(&data->bma25x_client->dev, enable);
	}
#else
	if ((1 == enable) || (0 == enable))
		bma25x_set_enable(&data->bma25x_client->dev, enable);
#endif
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

#ifdef BMA25X_ENABLE_INT2
	if (delay_ms <= 10)
		data->bandwidth = BMA25X_BW_62_50HZ;
	else if (delay_ms <= 20)
		data->bandwidth = BMA25X_BW_31_25HZ;
	else if (delay_ms <= 80)
		data->bandwidth = BMA25X_BW_15_63HZ;
	else if (delay_ms <= 200)
		data->bandwidth = BMA25X_BW_7_81HZ;
	else {
		dev_err(&data->bma25x_client->dev,
				"bma25x_cdev_poll_delay error value!!\n");
	}
	bma25x_set_bandwidth(data->bma25x_client, data->bandwidth);
#endif

	atomic_set(&data->delay, (unsigned int) delay_ms);

	return 0;
}

static ssize_t bma25x_flush_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (1 == data) {
		input_report_rel(bma25x->input, REL_FLUSH, -1);
		input_sync(bma25x->input);
	} else if (2 == data) {
		input_report_rel(bma25x->dev_interrupt, REL_INT_FLUSH, -1);
		input_sync(bma25x->dev_interrupt);
	}

	return count;
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

#ifdef BMA25X_ENABLE_INT2
	if (data <= 10)
		bma25x->bandwidth = BMA25X_BW_62_50HZ;
	else if (data <= 20)
		bma25x->bandwidth = BMA25X_BW_31_25HZ;
	else if (data <= 80)
		bma25x->bandwidth = BMA25X_BW_15_63HZ;
	else if (data <= 200)
		bma25x->bandwidth = BMA25X_BW_7_81HZ;
	else {
		dev_err(&bma25x->bma25x_client->dev,
				"bma25x_delay_store error value!!\n");
	}
	bma25x_set_bandwidth(bma25x->bma25x_client, bma25x->bandwidth);
#endif
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
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long data = 0;
	if (kstrtoul(buf, 10, &data))
		return -EINVAL;

	dev_err(dev, "bma25x_enable_store entry\n");
#ifdef BMA25X_ENABLE_INT2
	if ((1 == data) || (0 == data)) {
		bma25x_set_Int_Enable(client, 4, (unsigned char)data);
		bma25x_set_enable(&client->dev, (int)data);
	}
#else
	if ((1 == data) || (0 == data))
		bma25x_set_enable(&client->dev, data);
#endif
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
static ssize_t bma25x_flatdown_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int databuf = 0;
#ifdef BMA25X_ENABLE_INT1
	struct input_dev *input = to_input_dev(dev);
	struct bma25x_data *bma25x = input_get_drvdata(input);
	if (FLATDOWN_GESTURE == bma25x->flat_down_value)
		databuf = 1;
	else if (EXIT_FLATDOWN_GESTURE == bma25x->flat_down_value)
		databuf = 0;
#endif
	return snprintf(buf, PAGE_SIZE, "%d\n", databuf);
}

static ssize_t bma25x_debug_reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)

{
	ssize_t len = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);
	int i = 0;
	unsigned char data = 0;

	if (!bma25x) {
		dev_err(dev, "bma25x_data is null!!\n");
		return 0;
	}
	for (i = 0; i < 0x3F; ++i) {
		bma25x_smbus_read_byte(bma25x->bma25x_client, i, &data);
		len += snprintf(buf+len, PAGE_SIZE-len,
			"reg=0x%x value = 0x%x\n", i, data);
	}
	return len;
}

static int bma25x_set_reg_vaule(struct bma25x_data *bma25x,
		unsigned char reg_ddr, unsigned char databuf)
{
	int err = 0;
	err = bma25x_smbus_write_byte(bma25x->bma25x_client,
		reg_ddr, &databuf);
	return err;
}

static ssize_t bma25x_debug_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);
	int  reg = 0;
	int data = 0;
	int ret = 0;
	ret = sscanf(buf, "%x,%x", &reg, &data);
	dev_err(dev, "reg=0x%x, data=0x%x ret=%d", reg, data, ret);
	bma25x_set_reg_vaule(bma25x, (unsigned char)reg, (unsigned char)data);
	return count;
}
#ifdef BMA25X_ENABLE_INT1
static ssize_t bma25x_flat_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%d\n", bma25x->flat_threshold);
}

static ssize_t bma25x_flat_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	bma25x->flat_threshold = data;

	return count;
}

static ssize_t bma25x_int_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%d\n", bma25x->mEnabled);
}

static ssize_t bma25x_int_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data = 0;
	int error = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	bma25x_set_en_sig_int_mode(bma25x, data);

	return count;
}
#endif

static ssize_t bma25x_reg_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);
	u8 data[20];
	char *p = buf;

	if (bma25x->read_flag) {
		bma25x->read_flag = 0;
		bma25x_smbus_read_byte(bma25x->bma25x_client, bma25x->read_reg, data);
		p += snprintf(p, PAGE_SIZE, "%02x\n", data[0]);
		return (p-buf);
	}

	bma25x_smbus_read_byte_block(bma25x->bma25x_client, 0x09, data, 4);
	p += snprintf(p, PAGE_SIZE, "INT DATA(09~0c)=%02x,%02x,%02x,%02x\n",
			data[0], data[1], data[2], data[3]);

	bma25x_smbus_read_byte_block(bma25x->bma25x_client, 0x16, data, 3);
	p += snprintf(p, PAGE_SIZE, "INT EN(16~18)=%02x,%02x,%02x\n",
			data[0], data[1], data[2]);

	bma25x_smbus_read_byte_block(bma25x->bma25x_client, 0x19, data, 3);
	p += snprintf(p, PAGE_SIZE, "INT MAP(19~1b)=%02x,%02x,%02x\n",
			data[0], data[1], data[2]);

	bma25x_smbus_read_byte_block(bma25x->bma25x_client, 0x1e, data, 1);
	p += snprintf(p, PAGE_SIZE, "INT SRC(1e)=%02x\n",
			data[0]);

	bma25x_smbus_read_byte_block(bma25x->bma25x_client, 0x20, data, 1);
	p += snprintf(p, PAGE_SIZE, "INT OUT CTRL(20)=%02x\n",
			data[0]);

	bma25x_smbus_read_byte_block(bma25x->bma25x_client, 0x21, data, 1);
	p += snprintf(p, PAGE_SIZE, "INT LATCH(21)=%02x\n",
			data[0]);

	bma25x_smbus_read_byte_block(bma25x->bma25x_client, 0x27, data, 3);
	p += snprintf(p, PAGE_SIZE, "SLO NOMOT SET(27~29)=%02x,%02x,%02x\n",
			data[0], data[1], data[2]);

	bma25x_smbus_read_byte_block(bma25x->bma25x_client, 0x2e, data, 2);
	p += snprintf(p, PAGE_SIZE, "FLAT SET(2E~2F)=%02x,%02x\n",
			data[0], data[1]);

	return (p-buf);
}

static ssize_t bma25x_reg_dump_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma25x_data *bma25x = i2c_get_clientdata(client);
	unsigned int val, reg, opt;

	if (sscanf(buf, "%x,%x,%x", &reg, &val, &opt) == 3) {
		bma25x->read_reg = *((u8 *)&reg);
		bma25x->read_flag = 1;
	} else if (sscanf(buf, "%x,%x", &reg, &val) == 2) {
		dev_err(dev, "%s,reg = 0x%02x, val = 0x%02x\n",
			__func__, *(u8 *)&reg, *(u8 *)&val);
		bma25x_smbus_write_byte(bma25x->bma25x_client,
					*(u8 *)&reg, (u8 *)&val);
	}

	return count;
}

static DEVICE_ATTR(flush, S_IWUSR|S_IWGRP|S_IRUGO,
		NULL, bma25x_flush_store);
static DEVICE_ATTR(range, S_IWUSR|S_IWGRP|S_IRUGO,
		bma25x_range_show, bma25x_range_store);
static DEVICE_ATTR(bandwidth, S_IWUSR|S_IWGRP|S_IRUGO,
		bma25x_bandwidth_show, bma25x_bandwidth_store);
static DEVICE_ATTR(delay, S_IWUSR|S_IWGRP|S_IRUGO,
		bma25x_delay_show, bma25x_delay_store);
static DEVICE_ATTR(enable, S_IWUSR|S_IWGRP|S_IRUGO,
		bma25x_enable_show, bma25x_enable_store);
static DEVICE_ATTR(value, S_IRUGO,  bma25x_value_show, NULL);
static DEVICE_ATTR(flatdown, S_IWUSR|S_IRUGO,
		bma25x_flatdown_show, NULL);
static DEVICE_ATTR(debug_reg, S_IWUSR|S_IWGRP|S_IRUGO,
		bma25x_debug_reg_show, bma25x_debug_reg_store);
#ifdef BMA25X_ENABLE_INT1
static DEVICE_ATTR(flat_threshold, S_IWUSR|S_IWGRP|S_IRUGO,
		bma25x_flat_threshold_show, bma25x_flat_threshold_store);
static DEVICE_ATTR(int_mode, S_IWUSR|S_IWGRP|S_IRUGO,
		bma25x_int_mode_show, bma25x_int_mode_store);
#endif
static DEVICE_ATTR(reg, S_IRUGO | S_IWUSR, bma25x_reg_dump_show,
		bma25x_reg_dump_store);
static struct attribute *bma25x_attributes[] = {
	&dev_attr_flush.attr,
	&dev_attr_range.attr,
	&dev_attr_bandwidth.attr,
	&dev_attr_delay.attr,
	&dev_attr_enable.attr,
	&dev_attr_value.attr,
	&dev_attr_flatdown.attr,
	&dev_attr_debug_reg.attr,
#ifdef BMA25X_ENABLE_INT1
	&dev_attr_flat_threshold.attr,
	&dev_attr_int_mode.attr,
#endif
	&dev_attr_reg.attr,
	NULL
};

static struct attribute_group bma25x_attribute_group = {
	.attrs = bma25x_attributes
};

static int bma25x_power_ctl(struct bma25x_data *data, bool on)
{
#ifdef BMA25X_POWER_ALWAYS_ON
	return 0;
#else
	int ret = 0;
	int err = 0;
	if (!on && data->power_enabled) {
		ret = regulator_disable(data->vdd);
		if (ret) {
			dev_err(&data->bma25x_client->dev,
				"Regulator vdd disable failed ret=%d\n", ret);
			return ret;
		}
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
#endif
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
			BMA25X_VDD_MIN_UV, BMA25X_VDD_MAX_UV);
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
				BMA25X_VIO_MIN_UV, BMA25X_VIO_MAX_UV);
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
		regulator_set_voltage(data->vdd, 0, BMA25X_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return ret;
}

static int bma25x_power_deinit(struct bma25x_data *data)
{
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, BMA25X_VDD_MAX_UV);

	regulator_put(data->vdd);

	if (regulator_count_voltages(data->vio) > 0)
		regulator_set_voltage(data->vio, 0, BMA25X_VIO_MAX_UV);

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
	pdata->gpio_int1 = of_get_named_gpio_flags(dev->of_node,
			"bosch,gpio-int1", 0, &pdata->int1_flag);
	 pdata->gpio_int2 = of_get_named_gpio_flags(dev->of_node,
			"bosch,gpio-int2", 0, &pdata->int2_flag);
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
#if defined(BMA25X_ENABLE_INT1) || defined(BMA25X_ENABLE_INT2)
#ifdef BMA25X_ENABLE_INT1
static int bma25x_get_orient_flat_status(struct i2c_client *client, unsigned
		char *intstatus)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma25x_smbus_read_byte(client, BMA25X_STATUS_ORIENT_HIGH_REG,
			&data);
	data = BMA25X_GET_BITSLICE(data, BMA25X_FLAT_S);
	*intstatus = data;

	return comres;
}

static void bma25x_flat_work_func(struct work_struct *work)
{
	struct bma25x_data *data = container_of((struct delayed_work *)work,
		struct bma25x_data, flat_work);
	static struct bma25xacc acc;
	unsigned char sign_value = 0, sign_value2 = 0;
	int flat_up_value = 0;
	int flat_down_value = 0;
	int loop = 5;

	ISR_INFO(&data->bma25x_client->dev, "bma25x_flat_work_func entry: %d\n", atomic_read(&data->flat_flag));

	flat_up_value = data->flat_up_value;
	flat_down_value = data->flat_down_value;

	do {
		bma25x_get_orient_flat_status(data->bma25x_client,
				&sign_value);
		ISR_INFO(&data->bma25x_client->dev,
		"flat interrupt sign_value=%d\n", sign_value);
		bma25x_read_accel_xyz(data->bma25x_client,
				data->sensor_type, &acc);
		ISR_INFO(&data->bma25x_client->dev,
		"flat interrupt acc x,y,z=%d %d %d\n", acc.x, acc.y, acc.z);
		bma25x_set_Int_Enable(data->bma25x_client, 11, 0);
		if (1 == sign_value) {
			if (acc.z > 0)
				data->flat_up_value = FLATUP_GESTURE;
			else
				data->flat_up_value = EXIT_FLATUP_GESTURE;
			if (acc.z < 0)
				data->flat_down_value = FLATDOWN_GESTURE;
			else
				data->flat_down_value = EXIT_FLATDOWN_GESTURE;
			if (data->flat_down_value == FLATDOWN_GESTURE)
				bma25x_set_flat_hold_time(data->bma25x_client, 0x01);
			else
				bma25x_set_flat_hold_time(data->bma25x_client, 0x00);
		} else {
			data->flat_up_value = EXIT_FLATUP_GESTURE;
			data->flat_down_value = EXIT_FLATDOWN_GESTURE;
			if (atomic_read(&data->flat_flag))
				bma25x_set_flat_hold_time(data->bma25x_client, 0x00);
			else
				bma25x_set_flat_hold_time(data->bma25x_client, 0x03);
		}
		bma25x_set_Int_Enable(data->bma25x_client, 11, 1);
		bma25x_get_orient_flat_status(data->bma25x_client,
				&sign_value2);
		ISR_INFO(&data->bma25x_client->dev,
		"flat interrupt sign_value2=%d\n", sign_value2);
	} while (sign_value2 != sign_value && loop--);

	if (sign_value2 != sign_value)
		dev_info(&data->bma25x_client->dev,
			"flat not stable\n");

	atomic_set(&data->flat_flag, 0);

	if (TEST_BIT(FlatUp, data->mEnabled) &&
		(data->flat_up_value != flat_up_value)) {
		input_report_rel(data->dev_interrupt,
				FLAT_INTERRUPT, data->flat_up_value);
		input_sync(data->dev_interrupt);
	}
	if (TEST_BIT(FlatDown, data->mEnabled) &&
		(data->flat_down_value != flat_down_value)) {
		input_report_rel(data->dev_interrupt,
				FLAT_INTERRUPT, data->flat_down_value);
		input_sync(data->dev_interrupt);
	}
	if (TEST_BIT(Motion, data->mEnabled)) {
		if ((data->flat_up_value != flat_up_value) &&
			(flat_up_value == FLATUP_GESTURE)) {
			dev_info(&data->bma25x_client->dev,
				"glance exit flat up interrupt happened\n");
			input_report_rel(data->dev_interrupt,
					FLAT_INTERRUPT,
					GLANCE_EXIT_FLATUP_GESTURE);
			input_sync(data->dev_interrupt);
			wake_lock_timeout(&data->aod_wakelock, msecs_to_jiffies(100));
		}
		if ((data->flat_down_value != flat_down_value) &&
			(flat_down_value == FLATDOWN_GESTURE)) {
			dev_info(&data->bma25x_client->dev,
				"glance exit flat down interrupt happened\n");
			input_report_rel(data->dev_interrupt,
					FLAT_INTERRUPT,
					GLANCE_EXIT_FLATDOWN_GESTURE);
			input_sync(data->dev_interrupt);
			wake_lock_timeout(&data->aod_wakelock, msecs_to_jiffies(100));
		}
	}
	ISR_INFO(&data->bma25x_client->dev, "bma25x_flat_work_func finished\n");
}

static void bma25x_int1_irq_work_func(struct work_struct *work)
{
	struct bma25x_data *data = container_of((struct work_struct *)work,
		struct bma25x_data, int1_irq_work);
	unsigned char status = 0;
	unsigned char slow_data = 0;
	bma25x_smbus_read_byte(data->bma25x_client,
		BMA25X_STATUS1_REG, &status);
	ISR_INFO(&data->bma25x_client->dev,
		"bma25x_int1_irq_work_func entry status=0x%x\n", status);
	if (0 == data->mEnabled) {
		dev_info(&data->bma25x_client->dev,
		"flat interrupt mEnabled=%d\n", data->mEnabled);
		goto exit;
	}
	switch (status) {
#ifndef BMA25X_MAP_FLAT_TO_INT2
	case 0x80:
		queue_delayed_work(data->data_wq,
		&data->flat_work, msecs_to_jiffies(50));
		break;
	case 0x88:
#endif
	case 0x08:
		bma25x_smbus_read_byte(
		data->bma25x_client, 0x18, &slow_data);
		if (TEST_BIT(3, slow_data)) {
			dev_info(&data->bma25x_client->dev,
				"no motion interrupt happened\n");
			bma25x_set_Int_Enable(
				data->bma25x_client, 12, 0);
			bma25x_set_Int_Enable(
				data->bma25x_client, 13, 0);
			bma25x_set_slope_no_mot_duration(
				data->bma25x_client, 0x00);
			bma25x_set_slope_no_mot_threshold(
				data->bma25x_client, 0x32);
			bma25x_set_Int_Enable(
				data->bma25x_client, 12, 1);
			bma25x_set_Int_Enable(
				data->bma25x_client, 13, 1);
			bma25x_set_Int_Enable(
				data->bma25x_client, 15, 0);
			cancel_delayed_work_sync(&data->flat_work);
			atomic_set(&data->flat_flag, 1);
			queue_delayed_work(data->data_wq, &data->flat_work, 0);
		} else {
			dev_info(&data->bma25x_client->dev,
				"glance slow motion interrupt happened\n");
			if (data->flat_down_value != FLATDOWN_GESTURE &&
				data->flat_up_value != FLATUP_GESTURE) {
				input_report_rel(data->dev_interrupt,
					SLOW_NO_MOTION_INTERRUPT,
					GLANCE_MOVEMENT_GESTURE);
				input_sync(data->dev_interrupt);
				wake_lock_timeout(&data->aod_wakelock, msecs_to_jiffies(100));
			}
			bma25x_set_Int_Enable(
				data->bma25x_client, 12, 0);
			bma25x_set_Int_Enable(
				data->bma25x_client, 13, 0);
			bma25x_set_slope_no_mot_duration(
				data->bma25x_client, 0x01);
			if (data->aod_flag) {
				cancel_delayed_work_sync(&data->flat_work);
				data->aod_flag = 0;
			}
			bma25x_set_slope_no_mot_threshold(
				data->bma25x_client, 0x09);
			bma25x_set_Int_Enable(
				data->bma25x_client, 12, 1);
			bma25x_set_Int_Enable(
				data->bma25x_client, 13, 1);
			bma25x_set_Int_Enable(
				data->bma25x_client, 15, 1);
		}
		break;
	default:
		break;
	}
exit:
	ISR_INFO(&data->bma25x_client->dev,
		"bma25x_int1_irq_work_func finished!!\n");
}

static irqreturn_t bma25x_int1_irq_handler(int irq, void *handle)
{
	struct bma25x_data *data = handle;

	if (data == NULL)
		return IRQ_HANDLED;
	if (data->bma25x_client == NULL)
		return IRQ_HANDLED;

	schedule_work(&data->int1_irq_work);

	return IRQ_HANDLED;
}
#endif

#ifdef BMA25X_ENABLE_INT2
static void bma25x_int2_irq_work_func(struct work_struct *work)
{
	struct bma25x_data *data = container_of((struct work_struct *)work,
		struct bma25x_data, int2_irq_work);
	unsigned char status = 0;
	static struct bma25xacc acc;
	bma25x_smbus_read_byte(
		data->bma25x_client, BMA25X_STATUS2_REG, &status);
	ISR_INFO(&data->bma25x_client->dev,
		"bma25x_int2_irq_work_func entry!!\n");
	if ((status&0x80) == 0x80) {
		bma25x_report_axis_data(data, &acc);
		input_sync(data->input);
		data->value = acc;
		return;
	}
}

static irqreturn_t bma25x_int2_irq_handler(int irq, void *handle)
	{
	struct bma25x_data *data = handle;

	if (data == NULL)
		return IRQ_HANDLED;
	if (data->bma25x_client == NULL)
		return IRQ_HANDLED;

	schedule_work(&data->int2_irq_work);

	return IRQ_HANDLED;
}
#elif defined (BMA25X_MAP_FLAT_TO_INT2)
static irqreturn_t bma25x_int2_irq_handler(int irq, void *handle)
{
	struct bma25x_data *data = handle;

	if (data == NULL)
		return IRQ_HANDLED;
	if (data->bma25x_client == NULL)
		return IRQ_HANDLED;

	queue_delayed_work(data->data_wq,
		&data->flat_work, msecs_to_jiffies(50));
	wake_lock_timeout(&data->aod_wakelock, msecs_to_jiffies(60));

	return IRQ_HANDLED;
}
#endif
#if defined(BMA25X_ENABLE_INT1) || defined(BMA25X_ENABLE_INT2)
static int bma25x_set_Int_Enable(struct i2c_client *client, unsigned char
		InterruptType , unsigned char value)
{
	int comres = 0;
	unsigned char data1 = 0;
	unsigned char data2 = 0;

	if ((11 < InterruptType) && (InterruptType < 16)) {
		switch (InterruptType) {
		case 12:
			/* slow/no motion X Interrupt  */
			comres = bma25x_smbus_read_byte(client,
				BMA25X_INT_SLO_NO_MOT_EN_X_INT__REG, &data1);
			data1 = BMA25X_SET_BITSLICE(data1,
				BMA25X_INT_SLO_NO_MOT_EN_X_INT, value);
			comres = bma25x_smbus_write_byte(client,
				BMA25X_INT_SLO_NO_MOT_EN_X_INT__REG, &data1);
			break;
		case 13:
			/* slow/no motion Y Interrupt  */
			comres = bma25x_smbus_read_byte(client,
				BMA25X_INT_SLO_NO_MOT_EN_Y_INT__REG, &data1);
			data1 = BMA25X_SET_BITSLICE(data1,
				BMA25X_INT_SLO_NO_MOT_EN_Y_INT, value);
			comres = bma25x_smbus_write_byte(client,
				BMA25X_INT_SLO_NO_MOT_EN_Y_INT__REG, &data1);
			break;
		case 14:
			/* slow/no motion Z Interrupt  */
			comres = bma25x_smbus_read_byte(client,
				BMA25X_INT_SLO_NO_MOT_EN_Z_INT__REG, &data1);
			data1 = BMA25X_SET_BITSLICE(data1,
				BMA25X_INT_SLO_NO_MOT_EN_Z_INT, value);
			comres = bma25x_smbus_write_byte(client,
				BMA25X_INT_SLO_NO_MOT_EN_Z_INT__REG, &data1);
			break;
		case 15:
			/* slow / no motion Interrupt select */
			comres = bma25x_smbus_read_byte(client,
				BMA25X_INT_SLO_NO_MOT_EN_SEL_INT__REG, &data1);
			data1 = BMA25X_SET_BITSLICE(data1,
				BMA25X_INT_SLO_NO_MOT_EN_SEL_INT, value);
			comres = bma25x_smbus_write_byte(client,
				BMA25X_INT_SLO_NO_MOT_EN_SEL_INT__REG, &data1);
		}

	return comres;
	}

	comres = bma25x_smbus_read_byte(client, BMA25X_INT_ENABLE1_REG, &data1);
	comres = bma25x_smbus_read_byte(client, BMA25X_INT_ENABLE2_REG, &data2);

	value = value & 1;
	switch (InterruptType) {
	case 0:
		/* Low G Interrupt  */
		data2 = BMA25X_SET_BITSLICE(data2, BMA25X_EN_LOWG_INT,
				value);
		break;
	case 1:
		/* High G X Interrupt */
		data2 = BMA25X_SET_BITSLICE(data2, BMA25X_EN_HIGHG_X_INT,
				value);
		break;
	case 2:
		/* High G Y Interrupt */
		data2 = BMA25X_SET_BITSLICE(data2, BMA25X_EN_HIGHG_Y_INT,
				value);
		break;
	case 3:
		/* High G Z Interrupt */
		data2 = BMA25X_SET_BITSLICE(data2, BMA25X_EN_HIGHG_Z_INT,
				value);
		break;
	case 4:
		/* New Data Interrupt  */
		data2 = BMA25X_SET_BITSLICE(data2, BMA25X_EN_NEW_DATA_INT,
				value);
		break;
	case 5:
		/* Slope X Interrupt */
		data1 = BMA25X_SET_BITSLICE(data1, BMA25X_EN_SLOPE_X_INT,
				value);
		break;
	case 6:
		/* Slope Y Interrupt */
		data1 = BMA25X_SET_BITSLICE(data1, BMA25X_EN_SLOPE_Y_INT,
				value);
		break;

	case 7:
		/* Slope Z Interrupt */
		data1 = BMA25X_SET_BITSLICE(data1, BMA25X_EN_SLOPE_Z_INT,
				value);
		break;
	case 8:
		/* Single Tap Interrupt */
		data1 = BMA25X_SET_BITSLICE(data1, BMA25X_EN_SINGLE_TAP_INT,
				value);
		break;
	case 9:
		/* Double Tap Interrupt */
		data1 = BMA25X_SET_BITSLICE(data1, BMA25X_EN_DOUBLE_TAP_INT,
				value);
		break;
	case 10:
		/* Orient Interrupt  */
		data1 = BMA25X_SET_BITSLICE(data1, BMA25X_EN_ORIENT_INT, value);
		break;

	case 11:
		/* Flat Interrupt */
		data1 = BMA25X_SET_BITSLICE(data1, BMA25X_EN_FLAT_INT, value);
		break;
	default:
		break;
	}
	comres = bma25x_smbus_write_byte(client, BMA25X_INT_ENABLE1_REG,
			&data1);
	comres = bma25x_smbus_write_byte(client, BMA25X_INT_ENABLE2_REG,
			&data2);

	return comres;
}
#endif
static int bma25x_set_Int_Mode(struct i2c_client *client, unsigned char Mode)
{
	int comres = 0;
	unsigned char data = 0;

	comres = bma25x_smbus_read_byte(client,
			BMA25X_INT_MODE_SEL__REG, &data);
	data = BMA25X_SET_BITSLICE(data, BMA25X_INT_MODE_SEL, Mode);
	comres = bma25x_smbus_write_byte(client,
			BMA25X_INT_MODE_SEL__REG, &data);

	return comres;
}
#ifdef BMA25X_ENABLE_INT2
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
#ifdef BMA25X_MAP_FLAT_TO_INT2
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
#endif
#endif
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
#endif
static int bma25x_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0;
	struct bma25x_data *data;
	struct input_dev *dev;
	struct input_dev *dev_interrupt;
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
	usleep_range(5000, 5000);
	if (bma25x_soft_reset(client) < 0) {
		dev_err(&client->dev,
				"i2c bus write error, pls check HW connection\n");
		err = -EINVAL;
		goto deinit_power_exit;
	}
	usleep_range(5000, 5000);
	/* read and check chip id */
	if (bma25x_check_chip_id(client, data) < 0) {
		err = -EINVAL;
		goto deinit_power_exit;
	}
	mutex_init(&data->mode_mutex);
	mutex_init(&data->enable_mutex);
	mutex_init(&data->int_mode_mutex);
	data->bandwidth = BMA25X_BW_SET;
	data->range = BMA25X_RANGE_SET;
	data->sensitivity = bosch_sensor_range_map[0];
	err = bma25x_set_bandwidth(client, data->bandwidth);
	if (err < 0) {
		dev_err(&client->dev, "init bandwidth error\n");
		err = -EINVAL;
		goto deinit_power_exit;
	}
	err = bma25x_set_range(client, data->range);
	if (err < 0) {
		dev_err(&client->dev, "init bandwidth error\n");
		err = -EINVAL;
		goto deinit_power_exit;
	}
#if defined(BMA25X_ENABLE_INT1) || defined(BMA25X_ENABLE_INT2)
#ifdef BMA25X_ENABLE_INT1
	/*set interrupt1 for  special function*/
	err = bma25x_get_interrupt_gpio(data, pdata->gpio_int1);
	if (err) {
		dev_err(&data->bma25x_client->dev,
			"Failed to get interrupt gpio, err=%d\n", err);
		return -EINVAL;
		goto deinit_power_exit;
	}
	data->IRQ1 = client->irq;
#endif
#if defined(BMA25X_ENABLE_INT2) || defined(BMA25X_MAP_FLAT_TO_INT2)
	/*set interrupt2 for accelerometer data*/
	err = bma25x_get_interrupt_gpio(data, pdata->gpio_int2);
	if (err) {
		dev_err(&data->bma25x_client->dev,
			"Failed to get interrupt gpio, err=%d\n", err);
		return -EINVAL;
		goto deinit_power_exit;
	}
	data->IRQ2 = client->irq;
#endif
	err = bma25x_pinctrl_init(data);
	if (err) {
		dev_err(&data->bma25x_client->dev,
			"Failed to init pinctrl err=%d\n", err);
		return -EINVAL;
		goto deinit_power_exit;
	}
#ifdef BMA25X_ENABLE_INT1
#ifdef BMA25X_MAP_FLAT_TO_INT2
	bma25x_set_int2_pad_sel(client, PAD_FLAT);
#else
	bma25x_set_int1_pad_sel(client, PAD_FLAT);
#endif
	bma25x_set_int1_pad_sel(client, PAD_SLOP);
	bma25x_set_int1_pad_sel(client, PAD_SLOW_NO_MOTION);
#endif
#ifdef BMA25X_ENABLE_INT2
	bma25x_set_newdata(client, BMA25X_INT1_NDATA, 1);
	bma25x_set_newdata(client, BMA25X_INT2_NDATA, 0);
#endif
	bma25x_set_Int_Mode(client, 0x01);/*latch interrupt 250ms*/
#ifdef BMA25X_ENABLE_INT1
	err = request_irq(data->IRQ1, bma25x_int1_irq_handler,
			IRQF_TRIGGER_RISING,	"bma25x_int1", data);
	if (err)
		dev_err(&client->dev,  "could not request irq\n");
	INIT_WORK(&data->int1_irq_work, bma25x_int1_irq_work_func);
	INIT_DELAYED_WORK(&data->flat_work, bma25x_flat_work_func);
#endif
#if defined(BMA25X_ENABLE_INT2) || defined(BMA25X_MAP_FLAT_TO_INT2)
	err = request_irq(data->IRQ2, bma25x_int2_irq_handler,
			IRQF_TRIGGER_RISING,	"bma25x_int2", data);
	if (err)
		dev_err(&client->dev,  "could not request irq\n");
#ifdef BMA25X_ENABLE_INT2
	INIT_WORK(&data->int2_irq_work, bma25x_int2_irq_work_func);
#endif
#endif
#endif
#ifndef BMA25X_ENABLE_INT2
	data->data_wq = create_freezable_workqueue("bma25x_data_work");
	if (!data->data_wq) {
		dev_err(&client->dev, "Cannot get create workqueue!\n");
		goto deinit_power_exit;
	}
	hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->timer.function = &poll_function;
	data->work_delay_kt = ns_to_ktime(10000000);
	INIT_WORK(&data->work, bma25x_work_func);
#endif
	atomic_set(&data->delay, POLL_DEFAULT_INTERVAL_MS);
	atomic_set(&data->enable, 0);

	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	dev_interrupt = input_allocate_device();
	if (!dev_interrupt) {
		kfree(data);
		input_free_device(dev); /*free the successful dev and return*/
		return -ENOMEM;
	}
	/* only value events reported */
	dev->name = SENSOR_NAME;
	dev->id.bustype = BUS_I2C;
	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_abs_params(dev, ABS_X, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Y, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Z, ABSMIN, ABSMAX, 0, 0);
	input_set_capability(dev, EV_REL, REL_TIME_SEC);
	input_set_capability(dev, EV_REL, REL_TIME_NSEC);
	input_set_capability(dev, EV_REL, REL_FLUSH);
	input_set_drvdata(dev, data);
	err = input_register_device(dev);
	if (err < 0) {
		dev_err(&client->dev, "Cannot register input device\n");
		goto err_register_input_device;
	}

	/* all interrupt generated events are moved to interrupt input devices*/
	dev_interrupt->name = "bma25x-interrupt";
	dev_interrupt->id.bustype = BUS_I2C;
	input_set_abs_params(dev_interrupt, ABS_X, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev_interrupt, ABS_Y, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev_interrupt, ABS_Z, ABSMIN, ABSMAX, 0, 0);
	input_set_capability(dev_interrupt, EV_REL,
		SLOW_NO_MOTION_INTERRUPT);
	input_set_capability(dev_interrupt, EV_ABS,
		ORIENT_INTERRUPT);
	input_set_capability(dev_interrupt, EV_REL,
		FLAT_INTERRUPT);
	input_set_capability(dev_interrupt, EV_REL,
		REL_INT_FLUSH);
	input_set_drvdata(dev_interrupt, data);
	err = input_register_device(dev_interrupt);
	if (err < 0)
		goto err_register_input_device_interrupt;

	data->input = dev;
	data->dev_interrupt = dev_interrupt;

	dev_acc = bst_allocate_device();
	if (!dev_acc) {
		dev_err(&client->dev, "Cannot allocate bst device\n");
		err = -ENOMEM;
		goto error_sysfs;
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
	data->ref_count = 0;
	data->fifo_datasel = 0;
	data->fifo_count = 0;
#ifdef BMA25X_ENABLE_INT1
	data->flat_threshold = 962;
	data->aod_flag = 0;
	data->flat_up_value = 0;
	data->flat_down_value = 0;
	data->mEnabled = 0;
	atomic_set(&data->flat_flag, 0);
	wake_lock_init(&data->aod_wakelock, WAKE_LOCK_SUSPEND, "aod wakelock");
#endif
	data->cdev = sensors_cdev;
	data->cdev.min_delay = POLL_INTERVAL_MIN_MS * 1000;
	data->cdev.delay_msec = pdata->poll_interval;
	data->cdev.sensors_enable = bma25x_cdev_enable;
	data->cdev.sensors_poll_delay = bma25x_cdev_poll_delay;
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
	dev_notice(&client->dev, "bma25x driver probe successfully");
	bma25x_power_ctl(data, false);
	return 0;

remove_bst_acc_sysfs_exit:
	sysfs_remove_group(&data->bst_acc->dev.kobj,
			&bma25x_attribute_group);
bst_free_exit:
	bst_unregister_device(dev_acc);
bst_free_acc_exit:
	bst_free_device(dev_acc);
error_sysfs:
	input_unregister_device(data->input);
err_register_input_device_interrupt:
	input_free_device(dev_interrupt);
	input_unregister_device(data->input);
err_register_input_device:
	input_free_device(dev);
	destroy_workqueue(data->data_wq);
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

static int bma25x_remove(struct i2c_client *client)
{
	struct bma25x_data *data = i2c_get_clientdata(client);

	sensors_classdev_unregister(&data->cdev);
	if (data->bst_acc) {
		bst_unregister_device(data->bst_acc);
		bst_free_device(data->bst_acc);
	}

	bma25x_double_tap_disable(data);
	if (data->input)
		sysfs_remove_group(&data->input->dev.kobj,
				&bma25x_attribute_group);

	bma25x_set_enable(&client->dev, 0);
	destroy_workqueue(data->data_wq);
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
	unsigned char databuf = 0;

	mutex_lock(&data->enable_mutex);
	databuf = 0x80;
	bma25x_smbus_write_byte(data->bma25x_client,
			BMA25X_MODE_CTRL_REG, &databuf);
	mutex_unlock(&data->enable_mutex);
}

#ifdef CONFIG_PM
static int bma25x_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct bma25x_data *data = i2c_get_clientdata(client);
#ifndef BMA25X_ENABLE_INT1
	unsigned char databuf = 0;
#endif
	mutex_lock(&data->enable_mutex);

#ifndef BMA25X_ENABLE_INT1
	databuf = 0x80;
	bma25x_smbus_write_byte(data->bma25x_client,
		BMA25X_MODE_CTRL_REG, &databuf);
#endif

#ifndef BMA25X_ENABLE_INT2
	if (atomic_read(&data->enable) == 1) {
		hrtimer_cancel(&data->timer);
	}
#endif
	mutex_unlock(&data->enable_mutex);
	return 0;
}

static int bma25x_resume(struct i2c_client *client)
{
	struct bma25x_data *data = i2c_get_clientdata(client);
#ifndef BMA25X_ENABLE_INT1
	unsigned char databuf = 0;
#endif
	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
#ifndef BMA25X_ENABLE_INT1
		/* set suspend mode first then set normal mode
		     it is a workaound to prevent entering normal mode fail */
		databuf = 0x80;
		bma25x_smbus_write_byte(data->bma25x_client,
			BMA25X_MODE_CTRL_REG, &databuf);
		databuf = 0x00;
		bma25x_smbus_write_byte(data->bma25x_client,
			BMA25X_FIFO_MODE_REG, &databuf);
		databuf = 0x00;
		bma25x_smbus_write_byte(data->bma25x_client,
			BMA25X_MODE_CTRL_REG, &databuf);
		usleep_range(3000, 3000);
#endif
#ifndef BMA25X_ENABLE_INT2
		hrtimer_start(&data->timer,
			data->work_delay_kt, HRTIMER_MODE_REL);
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
