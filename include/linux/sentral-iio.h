#ifndef _SENTRAL_IIO_H_
#define _SENTRAL_IIO_H_

#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/notifier.h>
#include <linux/irq_work.h>
#include <linux/iio/iio.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ENDIS(b) (b ? "enable" : "disable")
#define TFSTR(b) (b ? "true" : "false")

#define SENTRAL_NAME "sentral"
#define SENTRAL_HUB_CLASS_NAME "sensorhub"
#define SENTRAL_HUB_DEVICE_NAME "sentral"
#define SENTRAL_WORKQUEUE_NAME "sentral_mcu"

#define I2C_BLOCK_SIZE_MAX (I2C_SMBUS_BLOCK_MAX)

#define DATA_BUFFER_SIZE 16384
#define SENTRAL_FIFO_BLOCK_SIZE 50
#define SENTRAL_SENSOR_TIMESTAMP_SCALE_NS 31250

#define FW_IMAGE_SIGNATURE 0x652A
#define FW_CDS_SIGNATURE 0xC88B
#define RAM_BUF_LEN 64

#define PARAM_READ_SIZE_MAX 16
#define PARAM_WRITE_SIZE_MAX 8
#define PARAM_MAX_RETRY 10
#define PARAM_SENSORS_ACTUAL_OFFSET 31

#define SENTRAL_WATCHDOG_WORK_MSECS 500

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
	SR_PARAM_PAGE =   0x54,
	SR_HOST_CONTROL = 0x55,
	SR_PARAM_LOAD =   0x5C,
	SR_PARAM_REQ =    0x64,
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
	SST_MAX,
	SST_FIRST = SST_ACCELEROMETER,
	SST_TIMESTAMP_LSW =             0xFC,
	SST_TIMESTAMP_MSW =             0xFD,
	SST_META_EVENT =                0xFE,
	SST_ALL =                       0xFF,
};

static const char *sentral_sensor_type_strings[] = {
	"NOP",
	"ACCELEROMETER",
	"GEOMAGNETIC_FIELD",
	"ORIENTATION",
	"GYROSCOPE",
	"LIGHT",
	"PRESSURE",
	"TEMPERATURE",
	"PROXIMITY",
	"GRAVITY",
	"LINEAR_ACCELERATION",
	"ROTATION_VECTOR",
	"RELATIVE_HUMIDITY",
	"AMBIENT_TEMPERATURE",
	"MAGNETIC_FIELD_UNCALIBRATED",
	"GAME_ROTATION_VECTOR",
	"GYROSCOPE_UNCALIBRATED",
	"SIGNIFICANT_MOTION",
	"STEP_DETECTOR",
	"STEP_COUNTER",
	"GEOMAGNETIC_ROTATION_VECTOR",
	"HEART_RATE",
	"TILT_DETECTOR",
};

enum sentral_param_page {
	SPP_SYS =             0x01,
	SPP_ALGO_WARM_START = 0x02,
	SPP_SENSORS =         0x03,
	SPP_ALGO_KNOBS =      0x13,
};

enum sentral_param_system {
	SP_SYS_META_EVENT_CONTROL = 0x01,
	SP_SYS_FIFO_CONTROL = 0x02,
	SP_SYS_SENSOR_STATUS_B0 = 0x03,
	SP_SYS_SENSOR_STATUS_B1 = 0x04,
	SP_SYS_HOST_IRQ_TS = 0x30,
	SP_SYS_PHYS_SENSOR_STATUS = 0x31,
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
struct sentral_param_timestamp_page {
	union {
		u64 bytes;
		struct {
			u32 host_irq_ts;
			u32 current_ts;
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
	union {
		u64 bytes;
		struct {
			struct sentral_param_sensor_status accel;
			struct sentral_param_sensor_status gyro;
			struct sentral_param_sensor_status mag;
		} status;
	};
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
	int irq;
	struct class *hub_class;
	struct device *hub_device;
	struct iio_dev *indio_dev;
	struct workqueue_struct *sentral_wq;
	struct work_struct work_reset;
	struct work_struct work_fifo_read;
	struct delayed_work work_watchdog;
	struct mutex lock;
	struct wake_lock w_lock;
	struct notifier_block nb;
	struct sentral_chip_control chip_control;
	u8 *data_buffer;
	bool init_complete;
	u64 ts_irq_utime;
	u32 ts_irq_stime;
	u64 ts_sensor_utime;
	u32 ts_sensor_stime;
};

#endif /* _SENTRAL_IIO_H_ */
