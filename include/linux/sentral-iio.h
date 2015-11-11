/*
 * COPYRIGHT (C) 2015 PNI SENSOR CORPORATION
 *
 * LICENSED UNDER THE APACHE LICENSE, VERSION 2.0 (THE "LICENSE");
 * YOU MAY NOT USE THIS FILE EXCEPT IN COMPLIANCE WITH THE LICENSE.
 * YOU MAY OBTAIN A COPY OF THE LICENSE AT
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY PNI SENSOR CORPORATION "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PNI SENSOR CORPORATION BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SENTRAL_IIO_H_
#define _SENTRAL_IIO_H_

#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/notifier.h>
#include <linux/irq_work.h>
#include <linux/iio/iio.h>
#include <linux/wait.h>

#define SEN_DRV_PROJECT_ID "1"
#define SEN_DRV_SUBPROJECT_ID "2"
#define SEN_DRV_VERSION "1.2.7"
#define SEN_DRV_BUILD "7"
#define SEN_DRV_DATE "Thu Nov  5 14:18:36 PDT 2015"

// comment out the following to use printk logging instead of dyndbg
#define SENTRAL_LOG_DYNDBG 1

// some build env may not have all levels available, so we define the level per
// log type here
#define SENTRAL_LOG_PRINTK_D KERN_DEBUG
#define SENTRAL_LOG_PRINTK_E KERN_ERR
#define SENTRAL_LOG_PRINTK_I KERN_INFO

#ifdef SENTRAL_LOG_DYNDBG
#define LOGD(dev, fmt, ...) dev_dbg(dev, fmt, ##__VA_ARGS__)
#define LOGE(dev, fmt, ...) dev_err(dev, fmt, ##__VA_ARGS__)
#define LOGI(dev, fmt, ...) dev_info(dev, fmt, ##__VA_ARGS__)
#else /* dyndbg not available, use printk */
#define LOGD(dev, fmt, ...) printk(SENTRAL_LOG_PRINTK_D "%s(): " fmt, __func__, ##__VA_ARGS__)
#define LOGE(dev, fmt, ...) printk(SENTRAL_LOG_PRINTK_E "%s(): " fmt, __func__, ##__VA_ARGS__)
#define LOGI(dev, fmt, ...) printk(SENTRAL_LOG_PRINTK_I "%s(): " fmt, __func__, ##__VA_ARGS__)
#endif /* SENTRAL_LOG_DYNDBG */

#define I(fmt, ...) printk(KERN_INFO "[SNS_HUB] " fmt, ##__VA_ARGS__)

#define SEN_DBG_PIO 1

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ENSTR(b) (b ? "enable" : "disable")
#define TFSTR(b) (b ? "true" : "false")

#define SENTRAL_NAME "sentral"
#define SENTRAL_HUB_CLASS_NAME "sensorhub"
#define SENTRAL_HUB_DEVICE_NAME "sentral"
#define SENTRAL_WORKQUEUE_NAME "sentral_mcu"

#define I2C_BLOCK_SIZE_MAX (I2C_SMBUS_BLOCK_MAX)

#define DATA_BUFFER_SIZE 16384
#define SENTRAL_FIFO_BLOCK_SIZE 50
#define SENTRAL_SENSOR_TIMESTAMP_SCALE_NS 31250
#define SENTRAL_FIFO_WATERMARK_BUFFER 500

#define FW_IMAGE_SIGNATURE 0x652A
#define FW_CDS_SIGNATURE 0xC88B
#define RAM_BUF_LEN 64

#define PARAM_READ_SIZE_MAX 16
#define PARAM_WRITE_SIZE_MAX 8
#define PARAM_MAX_RETRY 20
#define PARAM_SENSORS_ACTUAL_OFFSET 31

#define SENTRAL_WATCHDOG_WORK_MSECS 2000
#define SENTRAL_TS_REF_RESET_WORK_SECS 40
#define SENTRAL_WQ_FIFO_TIMEOUT_MSECS 5000
#define SENTRAL_WQ_FLUSH_TIMEOUT_MSECS 5000

#define SENTRAL_INACTIVITY_RATE_HZ 1
#define SENTRAL_COACH_RATE_HZ 30

#define SENTRAL_GESTURE_WRIST_TILT_WRAP 4

#define SENTRAL_CAL_TS_COUNT 50
#define SENTRAL_CAL_TS_SAMPLE_DELAY_MS 20

#define SENTRAL_FIFO_OVERFLOW_THRD 5

#define SENTRAL_STEP_COUNTER_TIMER 60*HZ

enum sentral_registers {
	SR_FIFO_START =   0x00,
	SR_FIFO_FLUSH =   0x32,
	SR_CHIP_CONTROL = 0x34,
	SR_HOST_STATUS =  0x35,
	SR_INT_STATUS =   0x36,
	SR_CHIP_STATUS =  0x37,
	SR_FIFO_BYTES =   0x38,
	SR_PARAM_ACK =    0x3A,
	SR_PARAM_SAVE =   0x3B,
	SR_WAKE_SRC =     0x4D,
	SR_STEP_REPORT =  0x4E,
	SR_ERROR =        0x50,
	SR_PARAM_PAGE =   0x54,
	SR_HOST_CONTROL = 0x55,
	SR_PARAM_LOAD =   0x5C,
	SR_PARAM_REQ =    0x64,
	SR_FITNESS_ID =   0x65,
	SR_VIBRATOR_EN =  0x66,
	SR_ROM_VERSION =  0x70,
	SR_PRODUCT_ID =   0x90,
	SR_REV_ID =       0x91,
	SR_UPLOAD_ADDR =  0x94,
	SR_UPLOAD_DATA =  0x96,
	SR_CRC_HOST =     0x97,
	SR_RESET_REQ =    0x9B,
	SR_PT_READY =     0x9E,
	SR_PT_CONFIG =    0xA0,
	SR_FIRST = SR_CHIP_CONTROL,
	SR_MAX = SR_PT_CONFIG,
};

enum sentral_error_value {
	SEN_ERR_SUCCESS =       0x00,
	SEN_ERR_INIT_PASS =     0x01,
	SEN_ERR_TEST_PASS =     0x02,
	SEN_ERR_MAG_RESET =     0x11,
	SEN_ERR_ACCEL_RESET =   0x12,
	SEN_ERR_GYRO_RESET =    0x14,
	SEN_ERR_RSVD_RESET =    0x18,
	SEN_ERR_INIT_FAILED =   0x20,
	SEN_ERR_UNEXP_DEV =     0x21,
	SEN_ERR_NO_DEV =        0x22,
	SEN_ERR_UNKNOWN =       0x23,
	SEN_ERR_DATA_UNAVAIL =  0x24,
	SEN_ERR_SLOW_RATE =     0x25,
	SEN_ERR_DATA_OVERFLOW = 0x26,
	SEN_ERR_STACK_OVERFLOW =0x27,
	SEN_ERR_MATH =          0x30,
	SEN_ERR_MEM =           0x40,
	SEN_ERR_SWI3 =          0x41,
	SEN_ERR_SWI4 =          0x42,
	SEN_ERR_INST =          0x43,
	SEN_ERR_ALGO =          0x50,
	SEN_ERR_SELF_TEST =     0x60,
	SEN_ERR_SELF_TEST_X =   0x61,
	SEN_ERR_SELF_TEST_Y =   0x62,
	SEN_ERR_SELF_TEST_Z =   0x63,
	SEN_ERR_FIFO_DISCARD =  0x70,
	SEN_ERR_FIFO_RETRIEVE = 0x71,
	SEN_ERR_FIFO_INIT =     0x72,
	SEN_ERR_RAM_ALLOC =     0x73,
	SEN_ERR_RAM_ADJ =       0x74,
	SEN_ERR_HOST_INT =      0x75,
	SEN_ERR_RATE_INVALID =  0x80,
	SEN_ERR_I2C_OVERFLOW =  0x90,
};

enum sentral_host_control_flags {
	SEN_HOST_CTRL_ALGO_STANDBY =  1 << 0,
	SEN_HOST_CTRL_ABORT_XFER =    1 << 1,
	SEN_HOST_CTRL_UPDATE_XFER =   1 << 2,
	SEN_HOST_CTRL_INT_DISABLE =   1 << 3,
	SEN_HOST_CTRL_NED_MODE =      1 << 4,
	SEN_HOST_CTRL_AP_SUSPENDED =  1 << 5,
	SEN_HOST_CTRL_REQ_SELF_TEST = 1 << 6,
	SEN_HOST_CTRL_RESERVED =      1 << 7,
};

enum sentral_chip_control_flags {
	SEN_CHIP_CTRL_CPU_RUN =       1 << 0,
	SEN_CHIP_CTRL_UPLOAD_ENABLE = 1 << 1,
};

enum sentral_sensor_type {
	SST_NOP =                          0,
	SST_ACCELEROMETER =                1,
	SST_GEOMAGNETIC_FIELD =            2,
	SST_ORIENTATION =                  3,
	SST_GYROSCOPE =                    4,
	SST_LIGHT =                        5,
	SST_PRESSURE =                     6,
	SST_TEMPERATURE =                  7,
	SST_PROXIMITY =                    8,
	SST_GRAVITY =                      9,
	SST_LINEAR_ACCELERATION =         10,
	SST_ROTATION_VECTOR =             11,
	SST_RELATIVE_HUMIDITY =           12,
	SST_AMBIENT_TEMPERATURE =         13,
	SST_MAGNETIC_FIELD_UNCALIBRATED = 14,
	SST_GAME_ROTATION_VECTOR =        15,
	SST_GYROSCOPE_UNCALIBRATED =      16,
	SST_SIGNIFICANT_MOTION =          17,
	SST_STEP_DETECTOR =               18,
	SST_STEP_COUNTER =                19,
	SST_GEOMAGNETIC_ROTATION_VECTOR = 20,
	SST_HEART_RATE =                  21,
	SST_TILT_DETECTOR =               22,
	SST_WAKE_GESTURE =                23,
	SST_GLANCE_GESTURE =              24,
	SST_PICK_UP_GESTURE =             25,
	SST_WRIST_TILT_GESTURE =          26,
	SST_SLEEP =                       28,
	SST_COACH =                       29,
	SST_INACTIVITY_ALARM =            30,
	SST_ACTIVITY =                    31,
	SST_MAX,
	SST_FIRST = SST_ACCELEROMETER,
	SST_DEBUG =                     0xF5,
	SST_TIMESTAMP_LSW =             0xFC,
	SST_TIMESTAMP_MSW =             0xFD,
	SST_META_EVENT =                0xFE,
	SST_ALL =                       0xFF,
};

static const char *sentral_sensor_type_strings[SST_MAX] = {
	[SST_NOP] = "NOP",
	[SST_ACCELEROMETER] = "ACCELEROMETER",
	[SST_GEOMAGNETIC_FIELD] = "MAGNETIC_FIELD",
	[SST_ORIENTATION] = "ORIENTATION",
	[SST_GYROSCOPE] = "GYROSCOPE",
	[SST_LIGHT] = "LIGHT",
	[SST_PRESSURE] = "PRESSURE",
	[SST_TEMPERATURE] = "TEMPERATURE",
	[SST_PROXIMITY] = "PROXIMITY",
	[SST_GRAVITY] = "GRAVITY",
	[SST_LINEAR_ACCELERATION] = "LINEAR_ACCELERATION",
	[SST_ROTATION_VECTOR] = "ROTATION_VECTOR",
	[SST_RELATIVE_HUMIDITY] = "RELATIVE_HUMIDITY",
	[SST_AMBIENT_TEMPERATURE] = "AMBIENT_TEMPERATURE",
	[SST_MAGNETIC_FIELD_UNCALIBRATED] = "MAGNETIC_FIELD_UNCALIBRATED",
	[SST_GAME_ROTATION_VECTOR] = "GAME_ROTATION_VECTOR",
	[SST_GYROSCOPE_UNCALIBRATED] = "GYROSCOPE_UNCALIBRATED",
	[SST_SIGNIFICANT_MOTION] = "SIGNIFICANT_MOTION",
	[SST_STEP_DETECTOR] = "STEP_DETECTOR",
	[SST_STEP_COUNTER] = "STEP_COUNTER",
	[SST_GEOMAGNETIC_ROTATION_VECTOR] = "GEOMAGNETIC_ROTATION_VECTOR",
	[SST_HEART_RATE] = "HEART_RATE",
	[SST_TILT_DETECTOR] = "TILT_DETECTOR",
	[SST_WAKE_GESTURE] = "WAKE_GESTURE",
	[SST_GLANCE_GESTURE] = "GLANCE_GESTURE",
	[SST_PICK_UP_GESTURE] = "PICK_UP_GESTURE",
	[SST_WRIST_TILT_GESTURE] = "WRIST_TILT_GESTURE",
	[SST_SLEEP] = "SLEEP",
	[SST_COACH] = "COACH",
	[SST_INACTIVITY_ALARM] = "INACTIVITY_ALARM",
	[SST_ACTIVITY] = "ACTIVITY",
};

static const u16 sentral_fitness_id_rates[] = {
	0, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000,
};

enum sentral_param_page {
	SPP_SYS =             1,
	SPP_ALGO_WARM_START = 2,
	SPP_SENSORS =         3,
	SPP_ALGO_KNOBS =     13,
	SPP_ASUS =           14,
};

enum sentral_param_system {
	SP_SYS_META_EVENT_CONTROL =  1,
	SP_SYS_FIFO_CONTROL =        2,
	SP_SYS_SENSOR_STATUS_B0 =    3,
	SP_SYS_SENSOR_STATUS_B1 =    4,
	SP_SYS_HOST_IRQ_TS =        30,
	SP_SYS_PHYS_SENSOR_STATUS = 31,
};

enum sentral_param_asus {
	SP_ASUS_INACTIVITY_TIMEOUT = 1,
	SP_ASUS_STEP_COUNT_INIT = 2,
};

enum sentral_sensor_power_mode {
	SEN_SENSOR_POWER_MODE_NOT_PRESENT = 0x00,
	SEN_SENSOR_POWER_MODE_POWER_DOWN,
	SEN_SENSOR_POWER_MODE_SUSPEND,
	SEN_SENSOR_POWER_MODE_SELF_TEST,
	SEN_SENSOR_POWER_MODE_INT_MOTION,
	SEN_SENSOR_POWER_MODE_ONE_SHOT,
	SEN_SENSOR_POWER_MODE_LOW_POWER,
	SEN_SENSOR_POWER_MODE_ACTIVE,
};

enum sentral_iio_channel_scan_id {
	SEN_SCAN_U32_1,
	SEN_SCAN_U32_2,
	SEN_SCAN_U32_3,
	SEN_SCAN_U32_4,
	SEN_SCAN_TS,
};

enum sentral_meta_event {
	SEN_META_NA = 0,
	SEN_META_FLUSH_COMPLETE,
	SEN_META_SAMPLE_RATE_CHANGED,
	SEN_META_POWER_MODE_CHANGED,
	SEN_META_ERROR,
	SEN_META_MAG_TRANSIENT,
	SEN_META_CAL_STATUS_CHANGED,
	SEN_META_STILLNESS_CHANGED,
	SEN_META_ALGO_SLOWDOWN,
	SEN_META_CAL_STABLE,
	SEN_META_RESERVED,
	SEN_META_SENSOR_ERROR,
	SEN_META_FIFO_OVERFLOW,
	SEN_META_DYN_RANGE_CHANGED,
	SEN_META_FIFO_WATERMARK,
	SEN_META_SELF_TEST_RESULTS,
	SEN_META_INITIALIZED,
	SEN_META_MAX,
	SEN_META_FIRST = SEN_META_FLUSH_COMPLETE,
};

struct sentral_data_meta {
	u8 event_id;
	u8 byte_1;
	u8 byte_2;
};

static const char *sentral_meta_event_strings[SEN_META_MAX] = {
	[SEN_META_FLUSH_COMPLETE] = "Flush Complete",
	[SEN_META_SAMPLE_RATE_CHANGED] = "Sample Rate Changed",
	[SEN_META_POWER_MODE_CHANGED] = "Power Mode Changed",
	[SEN_META_ERROR] = "Error",
	[SEN_META_MAG_TRANSIENT] = "Magnetic Transient",
	[SEN_META_CAL_STATUS_CHANGED] = "Cal Status Changed",
	[SEN_META_STILLNESS_CHANGED] = "Stillness Changed",
	[SEN_META_ALGO_SLOWDOWN] = "Algorithm Slowdown",
	[SEN_META_CAL_STABLE] = "Calibration Stable",
	[SEN_META_RESERVED] = "Reserved",
	[SEN_META_SENSOR_ERROR] = "Sensor Error",
	[SEN_META_FIFO_OVERFLOW] = "FIFO Overflow",
	[SEN_META_DYN_RANGE_CHANGED] = "Dynamic Range Changed",
	[SEN_META_FIFO_WATERMARK] = "FIFO Watermark",
	[SEN_META_SELF_TEST_RESULTS] = "Self-Test Results",
	[SEN_META_INITIALIZED] = "Initialized",
};

enum sentral_cal_status {
	SEN_CAL_STATUS_UNCAL = 0,
	SEN_CAL_STATUS_RUNNING,
	SEN_CAL_STATUS_FAILED,
	SEN_CAL_STATUS_CALIBRATED,
};

enum sentral_debug_type {
	SEN_DEBUG_STRING = 0,
	SEN_DEBUG_BINARY,
	SEN_DEBUG_RESERVED1,
	SEN_DEBUG_RESERVED2,
};

// PARAMETERS

// SYSTEM

// system param #1
struct sentral_param_meta_event_control {
	union {
		u64 bytes;
		struct {
			u8 event:1;
			u8 interrupt:1;
		} events[32];
	};
} __attribute__((__packed__));

// system param #2
struct sentral_param_fifo_control {
	union {
		u64 bytes;
		struct {
			u16 watermark;
			u16 size;
		} fifo;
	};
} __attribute__((__packed__));

// system param #3 (1-16), #4 (17-32)
struct sentral_param_sensor_status {
	union {
		u8 byte;
		struct {
			u8 data_available:1;
			u8 i2c_nack:1;
			u8 device_id_error:1;
			u8 transient_error:1;
			u8 data_lost:1;
			u8 power_mode:3;
		} bits;
	};
};

struct sentral_param_sensor_status_bank_page {
	union {
		u64 bytes;
		struct sentral_param_sensor_status sensor[16];
	};
};

// system param #30
struct sentral_param_timestamp {
	union {
		u64 bytes;
		struct {
			u32 int_stime;
			u32 current_stime;
		} ts;
	};
};

// system param #31

struct sentral_param_phys_sensor_status {
	u16 sample_rate;
	u16 dynamic_range;
	union {
		u8 byte;
		struct {
			u8 int_enable:1;
			u8 reserved:4;
			u8 power_mode:3;
		} bits;
	} flags;
} __attribute__((__packed__));

struct sentral_param_phys_sensor_status_page {
	struct sentral_param_phys_sensor_status accel;
	struct sentral_param_phys_sensor_status gyro;
	struct sentral_param_phys_sensor_status mag;
} __attribute__((__packed__));

// PARAM PAGE 3: SENSORS

struct sentral_param_sensor_info {
	u8 sensor_type;
	u8 driver_id;
	u8 driver_version;
	u8 power;
	u16 max_range;
	u16 resolution;
	u16 max_rate;
	u16 fifo_reserved;
	u16 fifo_max;
	u8 event_size;
	u8 reserved;
} __attribute__((__packed__));

struct sentral_param_sensor_config {
	u16 sample_rate;
	u16 max_report_latency;
	u16 change_sensitivity;
	u16 dynamic_range;
} __attribute__((__packed__));

// STATUS

struct sentral_chip_control {
	union {
		u8 byte;
		struct {
			u8 cpu_run:1;
			u8 upload_enable:1;
		} bits;
	};
};

struct sentral_host_status {
	union {
		u8 byte;
		struct {
			u8 cpu_reset:1;
			u8 algo_standby:1;
			u8 host_iface_id:3;
			u8 algo_id:3;
		} bits;
	};
};

struct sentral_chip_status {
	union {
		u8 byte;
		struct {
			u8 eeprom_detected:1;
			u8 ee_upload_done:1;
			u8 ee_upload_error:1;
			u8 idle:1;
			u8 no_eeprom:1;
		} bits;
	};
};

struct sentral_host_control {
	union {
		u8 byte;
		struct {
			u8 algo_standby:1;
			u8 abort_xfer:1;
			u8 update_xfer_count:1;
			u8 host_int_disable:1;
			u8 ned_coords:1;
			u8 ap_suspended:1;
			u8 sensor_self_test:1;
			u8 reserved:1;
		} bits;
	};
};

struct sentral_pt_config {
	union {
		u8 byte;
		struct {
			u8 enable:1;
			u8 clock_stretch:1;
		} bits;
	};
};

struct sentral_fw_flags {
	u16 eeprom_no_exec:1;
	u16 reserved1:7;
	u16 i2c_clock_speed:3;
	u16 rom_ver_exp:4;
	u16 reserved2:1;
};

struct sentral_fw_header {
	u16 signature;
	struct sentral_fw_flags flags;
	u32 text_crc;
	u32 reserved1;
	u16 text_length;
	u16 reserved2;
} __attribute__((__packed__));

struct sentral_fw_cds {
	u16 signature;
	u8 reserved[6];
	u16 ram_version;
	u8 cds_version;
	u8 boot_protocol;
	u8 pin_selection[8];
	u8 pull_selection[8];
	u8 device_name[16];
} __attribute__((__packed__));

struct sentral_wake_src_count {
	union {
		u8 byte;
		struct {
			u8 wrist_tilt:2;
			u8 sig_motion:2;
			u8 reserved:4;
		} bits;
	};
};

struct sentral_data_debug {
	struct {
		u8 length:6;
		u8 type:2;
	} attr;
	char value[12];
};

struct sentral_sensor_ref_time {
	u64 system_ns;
	u32 hub_stime;
};

struct sentral_step_count {
	u16 curr;
	u16 prev;
	u64 base;
	u64 total;
};

struct sentral_platform_data {
	unsigned int gpio_irq;
	const char *firmware;
};

struct sentral_hw_id {
	u8 product_id;
	u8 revision_id;
} __attribute__((__packed__));

struct sentral_device {
	struct i2c_client *client;
	const struct i2c_device_id *device_id;
	struct sentral_platform_data platform_data;
	unsigned int irq;
	struct class *hub_class;
	struct device *hub_device;
	struct iio_dev *indio_dev;
	struct workqueue_struct *sentral_wq;
	struct work_struct work_reset;
	struct work_struct work_fifo_read;
	struct delayed_work work_watchdog;
	struct delayed_work work_ts_ref_reset;
	struct mutex lock;
	struct mutex lock_special;
	struct mutex lock_flush;
	struct mutex lock_reset;
	struct mutex lock_ts;
	struct wakeup_source wlock_irq;
	struct wake_lock w_lock_reset;
	struct notifier_block nb;
	wait_queue_head_t wq_flush;
	bool flush_pending;
	bool flush_complete;
	u8 *data_buffer;
	bool init_complete;
	u32 ts_sensor_stime;
	u16 ts_msw_last;
	u16 ts_lsw_last;
	u16 ts_msw_meta_rate_change[SST_MAX];
	u16 ts_msw_offset[SST_MAX];
	u8 ts_msw_offset_reset[SST_MAX];
	struct sentral_sensor_ref_time ts_sensor_ref[SST_MAX];
	unsigned int stime_scale;
	unsigned long enabled_mask;
	unsigned long ts_ref_reset_mask;
	struct sentral_param_sensor_config sensor_config[SST_MAX];
	struct sentral_wake_src_count wake_src_prev;
	unsigned long sensor_warmup_mask;
	u32 fw_crc;
	bool warm_reset;
	struct sentral_step_count step_count;
	unsigned int crash_count;
	unsigned int overflow_count;
	u16 fifo_watermark;
	struct timer_list sc_timer;
	u16 step_counter_rate;
	bool step_counter_reset_rate_en;
	wait_queue_head_t wq_fifo;
	atomic_t fifo_pending;
	u8 latest_accel_buffer[24];
};

#endif /* _SENTRAL_IIO_H_ */
