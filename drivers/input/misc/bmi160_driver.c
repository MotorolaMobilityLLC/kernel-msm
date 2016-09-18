/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmi160_driver.c
 * @date     2015/08/17 14:40
 * @id       "09afbe6"
 * @version  1.4
 *
 * @brief
 * The core code of BMI160 device driver
 *
 * @detail
 * This file implements the core code of BMI160 device driver,
 * which includes hardware related functions, input device register,
 * device attribute files, etc.
*/

#include "bmi160_driver.h"
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#define I2C_BURST_READ_MAX_LEN      (256)
#define BMI160_STORE_COUNT  (6000)
#define LMADA     (1)
uint64_t g_current_apts_us;
static unsigned char g_fifo_data_arr[2048];/*1024 + 12*4*/

/*BMI power supply VDD 1.71V-3.6V VIO 1.2-3.6V */
#define BMI160_VDD_MIN_UV       1750000
#define BMI160_VDD_MAX_UV       3600000
#define BMI160_VIO_MIN_UV       1200000
#define BMI160_VIO_MAX_UV       3600000

static struct sensors_classdev accel_cdev = {
	.name = "bmi160-accel",
	.vendor = "Bosch Corporation",
	.version = 1,
	.handle = SENSORS_ACCELERATION_HANDLE,
	.type = SENSOR_TYPE_ACCELEROMETER,
	.max_range = "156.8",
	.resolution = "0.00781",
	.sensor_power = "0.13",
	.min_delay = 1000,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 200,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
	.sensors_calibrate = NULL,
	.sensors_write_cal_params = NULL,
	.params = NULL,
};

static struct sensors_classdev gyro_cdev = {
	.name = "bmi160-gyro",
	.vendor = "Bosch Corporation",
	.version = 1,
	.handle = SENSORS_GYROSCOPE_HANDLE,
	.type = SENSOR_TYPE_GYROSCOPE,
	.max_range = "35",
	.resolution = "0.06",
	.sensor_power = "0.13",
	.min_delay = 2000,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 200,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
	.sensors_calibrate = NULL,
	.sensors_write_cal_params = NULL,
	.params = NULL,
};

enum BMI_SENSOR_INT_T {
	/* Interrupt enable0*/
	BMI_ANYMO_X_INT = 0,
	BMI_ANYMO_Y_INT,
	BMI_ANYMO_Z_INT,
	BMI_D_TAP_INT,
	BMI_S_TAP_INT,
	BMI_ORIENT_INT,
	BMI_FLAT_INT,
	/* Interrupt enable1*/
	BMI_HIGH_X_INT,
	BMI_HIGH_Y_INT,
	BMI_HIGH_Z_INT,
	BMI_LOW_INT,
	BMI_DRDY_INT,
	BMI_FFULL_INT,
	BMI_FWM_INT,
	/* Interrupt enable2 */
	BMI_NOMOTION_X_INT,
	BMI_NOMOTION_Y_INT,
	BMI_NOMOTION_Z_INT,
	BMI_STEP_DETECTOR_INT,
	INT_TYPE_MAX
};

/*bmi fifo sensor type combination*/
enum BMI_SENSOR_FIFO_COMBINATION {
	BMI_FIFO_A = 0,
	BMI_FIFO_G,
	BMI_FIFO_M,
	BMI_FIFO_G_A,
	BMI_FIFO_M_A,
	BMI_FIFO_M_G,
	BMI_FIFO_M_G_A,
	BMI_FIFO_COM_MAX
};

/*bmi fifo analyse return err status*/
enum BMI_FIFO_ANALYSE_RETURN_T {
	FIFO_OVER_READ_RETURN = -10,
	FIFO_SENSORTIME_RETURN = -9,
	FIFO_SKIP_OVER_LEN = -8,
	FIFO_M_G_A_OVER_LEN = -7,
	FIFO_M_G_OVER_LEN = -6,
	FIFO_M_A_OVER_LEN = -5,
	FIFO_G_A_OVER_LEN = -4,
	FIFO_M_OVER_LEN = -3,
	FIFO_G_OVER_LEN = -2,
	FIFO_A_OVER_LEN = -1
};

/*!bmi sensor generic power mode enum */
enum BMI_DEV_OP_MODE {
	SENSOR_PM_NORMAL = 0,
	SENSOR_PM_LP1,
	SENSOR_PM_SUSPEND,
	SENSOR_PM_LP2
};

/*! bmi acc sensor power mode enum */
enum BMI_ACC_PM_TYPE {
	BMI_ACC_PM_NORMAL = 0,
	BMI_ACC_PM_LP1,
	BMI_ACC_PM_SUSPEND,
	BMI_ACC_PM_LP2,
	BMI_ACC_PM_MAX
};

/*! bmi gyro sensor power mode enum */
enum BMI_GYRO_PM_TYPE {
	BMI_GYRO_PM_NORMAL = 0,
	BMI_GYRO_PM_FAST_START,
	BMI_GYRO_PM_SUSPEND,
	BMI_GYRO_PM_MAX
};

/*! bmi mag sensor power mode enum */
enum BMI_MAG_PM_TYPE {
	BMI_MAG_PM_NORMAL = 0,
	BMI_MAG_PM_LP1,
	BMI_MAG_PM_SUSPEND,
	BMI_MAG_PM_LP2,
	BMI_MAG_PM_MAX
};


/*! bmi sensor support type*/
enum BMI_SENSOR_TYPE {
	BMI_ACC_SENSOR,
	BMI_GYRO_SENSOR,
	BMI_MAG_SENSOR,
	BMI_SENSOR_TYPE_MAX
};

/*!bmi sensor generic power mode enum */
enum BMI_AXIS_TYPE {
	X_AXIS = 0,
	Y_AXIS,
	Z_AXIS,
	AXIS_MAX
};

/*!bmi sensor generic intterrupt enum */
enum BMI_INT_TYPE {
	BMI160_INT0 = 0,
	BMI160_INT1,
	BMI160_INT_MAX
};

/*! bmi sensor time resolution definition*/
enum BMI_SENSOR_TIME_RS_TYPE {
	TS_0_78_HZ = 1,/*0.78HZ*/
	TS_1_56_HZ,/*1.56HZ*/
	TS_3_125_HZ,/*3.125HZ*/
	TS_6_25_HZ,/*6.25HZ*/
	TS_12_5_HZ,/*12.5HZ*/
	TS_25_HZ,/*25HZ, odr=6*/
	TS_50_HZ,/*50HZ*/
	TS_100_HZ,/*100HZ*/
	TS_200_HZ,/*200HZ*/
	TS_400_HZ,/*400HZ*/
	TS_800_HZ,/*800HZ*/
	TS_1600_HZ,/*1600HZ*/
	TS_MAX_HZ
};

/*! bmi sensor interface mode */
enum BMI_SENSOR_IF_MODE_TYPE {
	/*primary interface:autoconfig/secondary interface off*/
	P_AUTO_S_OFF = 0,
	/*primary interface:I2C/secondary interface:OIS*/
	P_I2C_S_OIS,
	/*primary interface:autoconfig/secondary interface:Magnetometer*/
	P_AUTO_S_MAG,
	/*interface mode reseved*/
	IF_MODE_RESEVED

};

/*! bmi160 acc/gyro calibration status in H/W layer */
enum BMI_CALIBRATION_STATUS_TYPE {
	/*BMI FAST Calibration ready x/y/z status*/
	BMI_ACC_X_FAST_CALI_RDY = 0,
	BMI_ACC_Y_FAST_CALI_RDY,
	BMI_ACC_Z_FAST_CALI_RDY
};

unsigned int reg_op_addr;

static const int bmi_pmu_cmd_acc_arr[BMI_ACC_PM_MAX] = {
	/*!bmi pmu for acc normal, low power1,
	 * suspend, low power2 mode command */
	CMD_PMU_ACC_NORMAL,
	CMD_PMU_ACC_LP1,
	CMD_PMU_ACC_SUSPEND,
	CMD_PMU_ACC_LP2
};

static const int bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_MAX] = {
	/*!bmi pmu for gyro normal, fast startup,
	 * suspend mode command */
	CMD_PMU_GYRO_NORMAL,
	CMD_PMU_GYRO_FASTSTART,
	CMD_PMU_GYRO_SUSPEND
};

static const int bmi_pmu_cmd_mag_arr[BMI_MAG_PM_MAX] = {
	/*!bmi pmu for mag normal, low power1,
	 * suspend, low power2 mode command */
	CMD_PMU_MAG_NORMAL,
	CMD_PMU_MAG_LP1,
	CMD_PMU_MAG_SUSPEND,
	CMD_PMU_MAG_LP2
};

static const char *bmi_axis_name[AXIS_MAX] = {"x", "y", "z"};

static const int bmi_interrupt_type[] = {
	/*!bmi interrupt type */
	/* Interrupt enable0 , index=0~6*/
	BMI160_ANY_MOTION_X_ENABLE,
	BMI160_ANY_MOTION_Y_ENABLE,
	BMI160_ANY_MOTION_Z_ENABLE,
	BMI160_DOUBLE_TAP_ENABLE,
	BMI160_SINGLE_TAP_ENABLE,
	BMI160_ORIENT_ENABLE,
	BMI160_FLAT_ENABLE,
	/* Interrupt enable1, index=7~13*/
	BMI160_HIGH_G_X_ENABLE,
	BMI160_HIGH_G_Y_ENABLE,
	BMI160_HIGH_G_Z_ENABLE,
	BMI160_LOW_G_ENABLE,
	BMI160_DATA_RDY_ENABLE,
	BMI160_FIFO_FULL_ENABLE,
	BMI160_FIFO_WM_ENABLE,
	/* Interrupt enable2, index = 14~17*/
	BMI160_NOMOTION_X_ENABLE,
	BMI160_NOMOTION_Y_ENABLE,
	BMI160_NOMOTION_Z_ENABLE,
	BMI160_STEP_DETECTOR_EN
};

/*! bmi sensor time depend on ODR*/
struct bmi_sensor_time_odr_tbl {
	u32 ts_duration_lsb;
	u32 ts_duration_us;
	u32 ts_delat;/*sub current delat fifo_time*/
};

struct bmi160_axis_data_t {
	s16 x;
	s16 y;
	s16 z;
};
struct bmi160_value_t {
	struct bmi160_axis_data_t acc;
	struct bmi160_axis_data_t gyro;
#ifdef BMI160_MAG_INTERFACE_SUPPORT
	struct bmi160_axis_data_t mag;
#endif
	int64_t ts_intvl;
};
struct bmi160_type_mapping_type {

	/*! bmi16x sensor chip id */
	uint16_t chip_id;

	/*! bmi16x chip revision code */
	uint16_t revision_id;

	/*! bmi160 sensor name */
	const char *sensor_name;
};

struct bmi160_store_info_t {
	uint8_t current_frm_cnt;
	uint64_t current_apts_us[2];
	uint8_t fifo_ts_total_frmcnt;
	uint64_t fifo_time;
};
static struct workqueue_struct *reportdata_wq;
#define FIFO_READ_LENGTH_RECOMMENDED    (67)
#define FIFO_SENSORTIME_OVERFLOW_MASK   (0x1000000)
#define FIFO_SENSORTIME_RESOLUTION              (390625)

static uint8_t s_fifo_data_buf[FIFO_DATA_BUFSIZE * 2] = {0};
static uint64_t sensor_time_old = 0;
static uint64_t sensor_time_new = 0;
static uint64_t host_time_old = 0;
static uint64_t host_time_new = 0;

#define BMI_RING_BUF_SIZE 100

static struct bmi160_value_t bmi_ring_buf[BMI_RING_BUF_SIZE];
static int s_ring_buf_head = 0;
static int s_ring_buf_tail = 0;


uint64_t get_current_timestamp(void)
{
	uint64_t ts;
	struct timeval tv;

	do_gettimeofday(&tv);
	ts = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;

	return ts;
}


/*! sensor support type map */
static const struct bmi160_type_mapping_type sensor_type_map[] = {

	{SENSOR_CHIP_ID_BMI, SENSOR_CHIP_REV_ID_BMI, "BMI160/162AB"},
	{SENSOR_CHIP_ID_BMI_C2, SENSOR_CHIP_REV_ID_BMI, "BMI160C2"},
	{SENSOR_CHIP_ID_BMI_C3, SENSOR_CHIP_REV_ID_BMI, "BMI160C3"},

};

/*!bmi160 sensor time depends on ODR */
static const struct bmi_sensor_time_odr_tbl
		sensortime_duration_tbl[TS_MAX_HZ] = {
	{0x010000, 2560000, 0x00ffff},/*2560ms, 0.39hz, odr=resver*/
	{0x008000, 1280000, 0x007fff},/*1280ms, 0.78hz, odr_acc=1*/
	{0x004000, 640000, 0x003fff},/*640ms, 1.56hz, odr_acc=2*/
	{0x002000, 320000, 0x001fff},/*320ms, 3.125hz, odr_acc=3*/
	{0x001000, 160000, 0x000fff},/*160ms, 6.25hz, odr_acc=4*/
	{0x000800, 80000,  0x0007ff},/*80ms, 12.5hz*/
	{0x000400, 40000, 0x0003ff},/*40ms, 25hz, odr_acc = odr_gyro =6*/
	{0x000200, 20000, 0x0001ff},/*20ms, 50hz, odr = 7*/
	{0x000100, 10000, 0x0000ff},/*10ms, 100hz, odr=8*/
	{0x000080, 5000, 0x00007f},/*5ms, 200hz, odr=9*/
	{0x000040, 2500, 0x00003f},/*2.5ms, 400hz, odr=10*/
	{0x000020, 1250, 0x00001f},/*1.25ms, 800hz, odr=11*/
	{0x000010, 625, 0x00000f},/*0.625ms, 1600hz, odr=12*/

};

static void bmi_dump_reg(struct bmi_client_data *client_data)
{
	#define REG_MAX0 0x24
	#define REG_MAX1 0x56
	int i;
	u8 dbg_buf0[REG_MAX0];
	u8 dbg_buf1[REG_MAX1];
	u8 dbg_buf_str0[REG_MAX0 * 3 + 1] = "";
	u8 dbg_buf_str1[REG_MAX1 * 3 + 1] = "";

	dev_notice(client_data->dev, "\nFrom 0x00:\n");

	client_data->device.bus_read(client_data->device.dev_addr,
			BMI_REG_NAME(USER_CHIP_ID), dbg_buf0, REG_MAX0);
	for (i = 0; i < REG_MAX0; i++) {
		sprintf(dbg_buf_str0 + i * 3, "%02x%c", dbg_buf0[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	dev_notice(client_data->dev, "%s\n", dbg_buf_str0);

	client_data->device.bus_read(client_data->device.dev_addr,
			BMI160_USER_ACCEL_CONFIG_ADDR, dbg_buf1, REG_MAX1);
	dev_notice(client_data->dev, "\nFrom 0x40:\n");
	for (i = 0; i < REG_MAX1; i++) {
		sprintf(dbg_buf_str1 + i * 3, "%02x%c", dbg_buf1[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	dev_notice(client_data->dev, "\n%s\n", dbg_buf_str1);

}

/*!
* BMI160 sensor remapping function
* need to give some parameter in BSP files first.
*/
static const struct bosch_sensor_axis_remap
	bst_axis_remap_tab_dft[MAX_AXIS_REMAP_TAB_SZ] = {
	/* src_x src_y src_z  sign_x  sign_y  sign_z */
	{  0,	 1,    2,	  1,	  1,	  1 }, /* P0 */
	{  1,	 0,    2,	  1,	 -1,	  1 }, /* P1 */
	{  0,	 1,    2,	 -1,	 -1,	  1 }, /* P2 */
	{  1,	 0,    2,	 -1,	  1,	  1 }, /* P3 */

	{  0,	 1,    2,	 -1,	  1,	 -1 }, /* P4 */
	{  1,	 0,    2,	 -1,	 -1,	 -1 }, /* P5 */
	{  0,	 1,    2,	  1,	 -1,	 -1 }, /* P6 */
	{  1,	 0,    2,	  1,	  1,	 -1 }, /* P7 */
};

static int bmi160_power_ctl(struct bmi_client_data *data, bool enable)
{
	int ret = 0;
	int err = 0;

	if (!enable && data->power_enabled) {
		ret = regulator_disable(data->vdd);
		if (ret) {
			dev_err(&data->i2c->dev,
				"Regulator vdd disable failed ret=%d\n", ret);
			return ret;
		}

		ret = regulator_disable(data->vio);
		if (ret) {
			dev_err(&data->i2c->dev,
				"Regulator vio disable failed ret=%d\n", ret);
			err = regulator_enable(data->vdd);
			return ret;
		}
		data->power_enabled = enable;
	} else if (enable && !data->power_enabled) {
		ret = regulator_enable(data->vdd);
		if (ret) {
			dev_err(&data->i2c->dev,
				"Regulator vdd enable failed ret=%d\n", ret);
			return ret;
		}

		ret = regulator_enable(data->vio);
		if (ret) {
			dev_err(&data->i2c->dev,
				"Regulator vio enable failed ret=%d\n", ret);
			err = regulator_disable(data->vdd);
			return ret;
		}
		data->power_enabled = enable;
	} else {
		dev_info(&data->i2c->dev,
				"Power on=%d. enabled=%d\n",
				enable, data->power_enabled);
	}

	return ret;
}

static int bmi160_power_init(struct bmi_client_data *data)
{
	int ret;

	data->vdd = regulator_get(&data->i2c->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		ret = PTR_ERR(data->vdd);
		dev_err(&data->i2c->dev,
			"Regulator get failed vdd ret=%d\n", ret);
		return ret;
	}

	if (regulator_count_voltages(data->vdd) > 0) {
		ret = regulator_set_voltage(data->vdd,
				BMI160_VDD_MIN_UV,
				BMI160_VDD_MAX_UV);
		if (ret) {
			dev_err(&data->i2c->dev,
				"Regulator set failed vdd ret=%d\n",
				ret);
			goto reg_vdd_put;
		}
	}

	data->vio = regulator_get(&data->i2c->dev, "vio");
	if (IS_ERR(data->vio)) {
		ret = PTR_ERR(data->vio);
		dev_err(&data->i2c->dev,
			"Regulator get failed vio ret=%d\n", ret);
		goto reg_vdd_set;
	}

	if (regulator_count_voltages(data->vio) > 0) {
			ret = regulator_set_voltage(data->vio,
				BMI160_VIO_MIN_UV,
				BMI160_VIO_MAX_UV);
		if (ret) {
			dev_err(&data->i2c->dev,
			"Regulator set failed vio ret=%d\n", ret);
			goto reg_vio_put;
		}
	}

	return 0;

reg_vio_put:
	regulator_put(data->vio);
reg_vdd_set:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, BMI160_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return ret;
}

static int bmi160_power_deinit(struct bmi_client_data *data)
{
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd,
				0, BMI160_VDD_MAX_UV);

	regulator_put(data->vdd);

	if (regulator_count_voltages(data->vio) > 0)
		regulator_set_voltage(data->vio,
				0, BMI160_VIO_MAX_UV);

	regulator_put(data->vio);

	return 0;
}

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

static void bmi_remap_sensor_data(struct bmi160_axis_data_t *val,
		struct bmi_client_data *client_data)
{
	struct bosch_sensor_data bsd;

	if ((NULL == client_data->bst_pd) ||
			(BOSCH_SENSOR_PLACE_UNKNOWN
			 == client_data->bst_pd->place))
		return;

	bsd.x = val->x;
	bsd.y = val->y;
	bsd.z = val->z;

	bst_remap_sensor_data_dft_tab(&bsd,
			client_data->bst_pd->place);

	val->x = bsd.x;
	val->y = bsd.y;
	val->z = bsd.z;

}

static void bmi_remap_data_acc(struct bmi_client_data *client_data,
				struct bmi160_accel_t *acc_frame)
{
	struct bosch_sensor_data bsd;

	if ((NULL == client_data->bst_pd) ||
			(BOSCH_SENSOR_PLACE_UNKNOWN
			 == client_data->bst_pd->place))
		return;

	bsd.x = acc_frame->x;
	bsd.y = acc_frame->y;
	bsd.z = acc_frame->z;

	bst_remap_sensor_data_dft_tab(&bsd,
			client_data->bst_pd->place);

	acc_frame->x = bsd.x;
	acc_frame->y = bsd.y;
	acc_frame->z = bsd.z;


}

static void bmi_remap_data_gyro(struct bmi_client_data *client_data,
					struct bmi160_gyro_t *gyro_frame)
{
	struct bosch_sensor_data bsd;

	if ((NULL == client_data->bst_pd) ||
			(BOSCH_SENSOR_PLACE_UNKNOWN
			 == client_data->bst_pd->place))
		return;

	bsd.x = gyro_frame->x;
	bsd.y = gyro_frame->y;
	bsd.z = gyro_frame->z;

	bst_remap_sensor_data_dft_tab(&bsd,
			client_data->bst_pd->place);

	gyro_frame->x = bsd.x;
	gyro_frame->y = bsd.y;
	gyro_frame->z = bsd.z;


}

static void bmi_fifo_frame_bytes_extend_calc(
	struct bmi_client_data *client_data,
	unsigned int *fifo_frmbytes_extend)
{

	switch (client_data->fifo_data_sel) {
	case BMI_FIFO_A_SEL:
	case BMI_FIFO_G_SEL:
		*fifo_frmbytes_extend = 7;
		break;
	case BMI_FIFO_G_A_SEL:
		*fifo_frmbytes_extend = 13;
		break;
	case BMI_FIFO_M_SEL:
		*fifo_frmbytes_extend = 9;
		break;
	case BMI_FIFO_M_A_SEL:
	case BMI_FIFO_M_G_SEL:
		/*8(mag) + 6(gyro or acc) +1(head) = 15*/
		*fifo_frmbytes_extend = 15;
		break;
	case BMI_FIFO_M_G_A_SEL:
		/*8(mag) + 6(gyro or acc) + 6 + 1 = 21*/
		*fifo_frmbytes_extend = 21;
		break;
	default:
		*fifo_frmbytes_extend = 0;
		break;

	};

}

static int bmi_input_init(struct bmi_client_data *client_data)
{
	struct input_dev *dev;
	int err = 0;

	dev = devm_input_allocate_device(&client_data->i2c->dev);
	if (NULL == dev)
		return -ENOMEM;

	dev->name = BMI160_ACCEL_INPUT_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_abs_params(dev, ABS_X, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Y, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Z, ABSMIN, ABSMAX, 0, 0);

	input_set_drvdata(dev, client_data);
	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		dev_notice(client_data->dev, "bmi160 accel input free!\n");
		return err;
	}
	client_data->input_accel = dev;
	dev_notice(client_data->dev,
		"bmi160 accel input register successfully, %s!\n",
		client_data->input_accel->name);

	dev = devm_input_allocate_device(&client_data->i2c->dev);
	if (NULL == dev)
		return -ENOMEM;

	dev->name = BMI160_GYRO_INPUT_NAME;
	dev->id.bustype = BUS_I2C;


	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_abs_params(dev, ABS_RX, GYRO_MAX_VALUE, GYRO_MIN_VALUE, 0, 0);
	input_set_abs_params(dev, ABS_RY, GYRO_MAX_VALUE, GYRO_MIN_VALUE, 0, 0);
	input_set_abs_params(dev, ABS_RZ, GYRO_MAX_VALUE, GYRO_MIN_VALUE, 0, 0);
	input_set_drvdata(dev, client_data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		dev_notice(client_data->dev, "bmi160 accel input free!\n");
		return err;
	}
	client_data->input_gyro = dev;
	dev_notice(client_data->dev,
		"bmi160 gyro input register successfully, %s!\n",
		client_data->input_gyro->name);

	return err;
}


static void bmi_input_destroy(struct bmi_client_data *client_data)
{
	struct input_dev *dev = client_data->input_accel;

	input_unregister_device(dev);
	input_free_device(dev);

	dev = client_data->input_gyro;
	input_unregister_device(dev);
	input_free_device(dev);
}

static int bmi_check_chip_id(struct bmi_client_data *client_data)
{
	int8_t err = 0;
	int8_t i = 0;
	uint8_t chip_id = 0;
	uint8_t read_count = 0;
	u8 bmi_sensor_cnt = sizeof(sensor_type_map)
				/ sizeof(struct bmi160_type_mapping_type);
	/* read and check chip id */
	while (read_count++ < CHECK_CHIP_ID_TIME_MAX) {
		if (client_data->device.bus_read(client_data->device.dev_addr,
				BMI_REG_NAME(USER_CHIP_ID), &chip_id, 1) < 0) {

			dev_err(client_data->dev,
					"Bosch Sensortec Device not found"
						"read chip_id:%d\n", chip_id);
			continue;
		} else {
			for (i = 0; i < bmi_sensor_cnt; i++) {
				if (sensor_type_map[i].chip_id == chip_id) {
					client_data->chip_id = chip_id;
					dev_notice(client_data->dev,
					"Bosch Sensortec Device detected, "
			"HW IC name: %s\n", sensor_type_map[i].sensor_name);
					break;
				}
			}
			if (i < bmi_sensor_cnt)
				break;
			else {
				if (read_count == CHECK_CHIP_ID_TIME_MAX) {
					dev_err(client_data->dev,
				"Failed!Bosch Sensortec Device not found"
					" mismatch chip_id:%d\n", chip_id);
					err = -ENODEV;
					return err;
				}
			}
			mdelay(1);
		}
	}
	return err;

}

static int bmi_pmu_set_suspend(struct bmi_client_data *client_data)
{
	int err = 0;
	if (client_data == NULL)
		return -EINVAL;
	else {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[SENSOR_PM_SUSPEND]);
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[SENSOR_PM_SUSPEND]);
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_mag_arr[SENSOR_PM_SUSPEND]);
		client_data->pw.acc_pm = BMI_ACC_PM_SUSPEND;
		client_data->pw.gyro_pm = BMI_GYRO_PM_SUSPEND;
		client_data->pw.mag_pm = BMI_MAG_PM_SUSPEND;
	}

	return err;
}

static int bmi_get_err_status(struct bmi_client_data *client_data)
{
	int err = 0;

	err = BMI_CALL_API(get_error_status)(&client_data->err_st.fatal_err,
		&client_data->err_st.err_code, &client_data->err_st.i2c_fail,
	&client_data->err_st.drop_cmd, &client_data->err_st.mag_drdy_err);
	return err;
}


static enum hrtimer_restart reportdata_timer_fun(
	struct hrtimer *hrtimer)
{
	struct bmi_client_data *client_data =
		container_of(hrtimer, struct bmi_client_data, timer);
	int32_t delay = 0;
	delay = atomic_read(&client_data->delay);
	queue_work(reportdata_wq, &(client_data->report_data_work));
	client_data->work_delay_kt = ns_to_ktime(delay*1000000);
	hrtimer_forward(hrtimer, ktime_get(), client_data->work_delay_kt);

	return HRTIMER_RESTART;
}

static ssize_t bmi_show_enable_timer(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	return snprintf(buf, 16, "%d\n", client_data->is_timer_running);
}

static ssize_t bmi_store_enable_timer(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;
	if (data) {
		if (0 == client_data->is_timer_running) {
			hrtimer_start(&client_data->timer,
							ns_to_ktime(10000000),
							HRTIMER_MODE_REL);
		client_data->is_timer_running = 1;
	}
	} else {
		if (1 == client_data->is_timer_running) {
			hrtimer_cancel(&client_data->timer);
			client_data->is_timer_running = 0;
		}
	}
	return count;
}

static void bmi_work_func(struct work_struct *work)
{
	struct bmi_client_data *client_data =
		container_of((struct delayed_work *)work,
			struct bmi_client_data, work);
	unsigned long delay =
		msecs_to_jiffies(atomic_read(&client_data->delay));
	struct bmi160_accel_t data;
	struct bmi160_axis_data_t bmi160_udata;
	int err;

	err = BMI_CALL_API(read_accel_xyz)(&data);
	if (err < 0)
		return;

	bmi160_udata.x = data.x;
	bmi160_udata.y = data.y;
	bmi160_udata.z = data.z;

	bmi_remap_sensor_data(&bmi160_udata, client_data);
	/*report current frame via input event*/
	input_event(client_data->input_accel, EV_ABS, ABS_X, bmi160_udata.x);
	input_event(client_data->input_accel, EV_ABS, ABS_Y, bmi160_udata.y);
	input_event(client_data->input_accel, EV_ABS, ABS_Z, bmi160_udata.z);
	input_sync(client_data->input_accel);

	schedule_delayed_work(&client_data->work, delay);
}

static void bmi_gyro_work_func(struct work_struct *work)
{
	struct bmi_client_data *client_data = container_of(
		(struct delayed_work *)work,
		struct bmi_client_data, gyro_work);
	unsigned long delay =
		 msecs_to_jiffies(atomic_read(&client_data->delay));
	struct bmi160_gyro_t data;
	struct bmi160_axis_data_t bmi160_udata;
	int err;

	err = BMI_CALL_API(read_gyro_xyz)(&data);
	if (err < 0)
		return;

	bmi160_udata.x = data.x;
	bmi160_udata.y = data.y;
	bmi160_udata.z = data.z;

	bmi_remap_sensor_data(&bmi160_udata, client_data);
	/*report current frame via input event*/
	input_event(client_data->input_gyro, EV_ABS, ABS_RX, bmi160_udata.x);
	input_event(client_data->input_gyro, EV_ABS, ABS_RY, bmi160_udata.y);
	input_event(client_data->input_gyro, EV_ABS, ABS_RZ, bmi160_udata.z);
	input_sync(client_data->input_gyro);

	schedule_delayed_work(&client_data->gyro_work, delay);
}

static uint8_t dbg_buf_str[2048] = "";
static void bmi_hrtimer_work_func(struct work_struct *work)
{
	struct bmi_client_data *client_data =
		container_of((struct delayed_work *)work,
	struct bmi_client_data, work);

	unsigned int fifo_len0 = 0;
	unsigned int fifo_frmbytes_ext = 0;
	unsigned int fifo_read_len = 0;
	int i, err;

	bmi_fifo_frame_bytes_extend_calc(client_data, &fifo_frmbytes_ext);

	err = BMI_CALL_API(fifo_length)(&fifo_len0);
	client_data->fifo_bytecount = fifo_len0;

	if (client_data->fifo_bytecount == 0 || err) {
		return ;
	}

	fifo_read_len = client_data->fifo_bytecount + fifo_frmbytes_ext;
	if (fifo_read_len > FIFO_DATA_BUFSIZE) {
		fifo_read_len = FIFO_DATA_BUFSIZE;
	}

	if (!err) {
		err = bmi_burst_read_wrapper(client_data->device.dev_addr,
			BMI160_USER_FIFO_DATA__REG, s_fifo_data_buf,
			fifo_read_len);
	}

	for (i = 0; i < fifo_read_len; i++) {
		sprintf(dbg_buf_str + i * 3, "%02x%c", s_fifo_data_buf[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	pr_info("%s\n", dbg_buf_str);

}

static ssize_t bmi160_chip_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	return sprintf(buf, "0x%x\n", client_data->chip_id);
}

static ssize_t bmi160_err_st_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	int err = 0;
	err = bmi_get_err_status(client_data);
	if (err)
		return err;
	else {
		return sprintf(buf, "fatal_err:0x%x, err_code:%d,\n\n"
			"i2c_fail_err:%d, drop_cmd_err:%d, mag_drdy_err:%d\n",
			client_data->err_st.fatal_err,
			client_data->err_st.err_code,
			client_data->err_st.i2c_fail,
			client_data->err_st.drop_cmd,
			client_data->err_st.mag_drdy_err);
	}
}

static ssize_t bmi160_sensor_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	u32 sensor_time;
	err = BMI_CALL_API(get_sensor_time)(&sensor_time);
	if (err)
		return err;
	else
		return sprintf(buf, "0x%x\n", (unsigned int)sensor_time);
}

static ssize_t bmi160_fifo_flush_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long enable;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;
	if (enable)
		err = BMI_CALL_API(set_command_register)(CMD_CLR_FIFO_DATA);

	if (err)
		dev_err(client_data->dev, "fifo flush failed!\n");

	return count;

}


static ssize_t bmi160_fifo_bytecount_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned int fifo_bytecount = 0;

	BMI_CALL_API(fifo_length)(&fifo_bytecount);
	err = sprintf(buf, "%u\n", fifo_bytecount);
	return err;
}

static ssize_t bmi160_fifo_bytecount_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	int err;
	unsigned long data;
	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	client_data->fifo_bytecount = (unsigned int) data;

	return count;
}

int bmi160_fifo_data_sel_get(struct bmi_client_data *client_data)
{
	int err = 0;
	unsigned char fifo_acc_en, fifo_gyro_en, fifo_mag_en;
	unsigned char fifo_datasel;

	err += BMI_CALL_API(get_fifo_accel_enable)(&fifo_acc_en);
	err += BMI_CALL_API(get_fifo_gyro_enable)(&fifo_gyro_en);
	err += BMI_CALL_API(get_fifo_mag_enable)(&fifo_mag_en);

	if (err)
		return err;

	fifo_datasel = (fifo_acc_en << BMI_ACC_SENSOR) |
			(fifo_gyro_en << BMI_GYRO_SENSOR) |
				(fifo_mag_en << BMI_MAG_SENSOR);

	client_data->fifo_data_sel = fifo_datasel;

	return err;


}

static ssize_t bmi160_fifo_data_sel_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	err = bmi160_fifo_data_sel_get(client_data);
	if (err)
		return -EINVAL;
	return sprintf(buf, "%d\n", client_data->fifo_data_sel);
}

/* write any value to clear all the fifo data. */
static ssize_t bmi160_fifo_data_sel_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	int err;
	unsigned long data;
	unsigned char fifo_datasel;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* data format: aimed 0b0000 0x(m)x(g)x(a), x:1 enable, 0:disable*/
	if (data > 7)
		return -EINVAL;


	fifo_datasel = (unsigned char)data;


	err += BMI_CALL_API(set_fifo_accel_enable)
			((fifo_datasel & (1 << BMI_ACC_SENSOR)) ? 1 :  0);
	err += BMI_CALL_API(set_fifo_gyro_enable)
			(fifo_datasel & (1 << BMI_GYRO_SENSOR) ? 1 : 0);
	err += BMI_CALL_API(set_fifo_mag_enable)
			((fifo_datasel & (1 << BMI_MAG_SENSOR)) ? 1 : 0);

	/*err += BMI_CALL_API(set_command_register)(CMD_CLR_FIFO_DATA);*/
	if (err)
		return -EIO;
	else {
		dev_notice(client_data->dev, "FIFO A_en:%d, G_en:%d, M_en:%d\n",
			(fifo_datasel & (1 << BMI_ACC_SENSOR)) ? 1 :  0,
			(fifo_datasel & (1 << BMI_GYRO_SENSOR) ? 1 : 0),
			((fifo_datasel & (1 << BMI_MAG_SENSOR)) ? 1 : 0));
		client_data->fifo_data_sel = fifo_datasel;
	}

	return count;
}

static int bmi_fifo_analysis_handle(struct bmi_client_data *client_data,
				u8 *fifo_data, u16 fifo_length, char *buf)
{
	u8 frame_head = 0;/* every frame head*/
	int len = 0;

	/*u8 skip_frame_cnt = 0;*/
	u8 acc_frm_cnt = 0;/*0~146*/
	u8 gyro_frm_cnt = 0;
	u8 mag_frm_cnt = 0;
	u8 tmp_frm_cnt = 0;
	/*u8 tmp_odr = 0;*/
	/*uint64_t current_apts_us = 0;*/
	/*fifo data last frame start_index A G M*/
	u64 fifo_time = 0;
	static uint32_t current_frm_ts;
	u16 fifo_index = 0;/* fifo data buff index*/
	u16 fifo_index_tmp = 0;
	u16 i = 0;
	s8 last_return_st = 0;
	int err = 0;
	unsigned int frame_bytes = 0;
	struct bmi160_mag_xyzr_t mag;
	struct bmi160_mag_xyz_s32_t mag_comp_xyz;
	struct bmi160_accel_t acc_frame_arr[FIFO_FRAME_CNT];
	struct bmi160_gyro_t gyro_frame_arr[FIFO_FRAME_CNT];
	struct bmi160_mag_xyzr_t mag_frame_arr[FIFO_FRAME_CNT];

	struct odr_t odr;

	memset(&odr, 0, sizeof(odr));
	memset(&mag, 0, sizeof(mag));
	memset(&mag_comp_xyz, 0, sizeof(mag_comp_xyz));
	for (i = 0; i < FIFO_FRAME_CNT; i++) {
		memset(&mag_frame_arr[i], 0, sizeof(struct bmi160_mag_xyzr_t));
		memset(&acc_frame_arr[i], 0, sizeof(struct bmi160_accel_t));
		memset(&gyro_frame_arr[i], 0, sizeof(struct bmi160_gyro_t));
	}
	/*current_apts_us = get_current_timestamp();*/
	/* no fifo select for bmi sensor*/
	if (!client_data->fifo_data_sel) {
		dev_err(client_data->dev,
				"No select any sensor FIFO for BMI16x\n");
		return -EINVAL;
	}
	/*driver need read acc_odr/gyro_odr/mag_odr*/
	if ((client_data->fifo_data_sel) & (1 << BMI_ACC_SENSOR))
		odr.acc_odr = client_data->odr.acc_odr;
	if ((client_data->fifo_data_sel) & (1 << BMI_GYRO_SENSOR))
		odr.gyro_odr = client_data->odr.gyro_odr;
	if ((client_data->fifo_data_sel) & (1 << BMI_MAG_SENSOR))
		odr.mag_odr = client_data->odr.mag_odr;
	bmi_fifo_frame_bytes_extend_calc(client_data, &frame_bytes);
/* search sensor time sub function firstly */
	for (fifo_index = 0; fifo_index < fifo_length;) {
		/* conside limited HW i2c burst reading issue,
		need to re-calc index 256 512 768 1024...*/
		if ((fifo_index_tmp >> 8) != (fifo_index >> 8)) {
			if (fifo_data[fifo_index_tmp] ==
				fifo_data[(fifo_index >> 8)<<8]) {
				fifo_index = (fifo_index >> 8) << 8;
				fifo_length +=
					(fifo_index - fifo_index_tmp + 1);
			}
		}
		fifo_index_tmp = fifo_index;
		/* compare index with 256/512/ before doing parsing*/
		if (((fifo_index + frame_bytes) >> 8) != (fifo_index >> 8)) {
			fifo_index = ((fifo_index + frame_bytes) >> 8) << 8;
			continue;
		}

		frame_head = fifo_data[fifo_index];

		switch (frame_head) {
			/*skip frame 0x40 22 0x84*/
		case FIFO_HEAD_SKIP_FRAME:
		/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + 1 > fifo_length) {
				last_return_st = FIFO_SKIP_OVER_LEN;
				break;
			}
			/*skip_frame_cnt = fifo_data[fifo_index];*/
			fifo_index = fifo_index + 1;
		break;

			/*M & G & A*/
		case FIFO_HEAD_M_G_A:
		{/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + MGA_BYTES_FRM > fifo_length) {
				last_return_st = FIFO_M_G_A_OVER_LEN;
				break;
			}

			/* mag frm index = gyro */
			mag_frm_cnt = gyro_frm_cnt;
			mag_frame_arr[mag_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			mag_frame_arr[mag_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			mag_frame_arr[mag_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];
			mag_frame_arr[mag_frm_cnt].r =
				fifo_data[fifo_index + 7] << 8 |
					fifo_data[fifo_index + 6];

			gyro_frame_arr[gyro_frm_cnt].x =
				fifo_data[fifo_index + 9] << 8 |
					fifo_data[fifo_index + 8];
			gyro_frame_arr[gyro_frm_cnt].y =
				fifo_data[fifo_index + 11] << 8 |
					fifo_data[fifo_index + 10];
			gyro_frame_arr[gyro_frm_cnt].z =
				fifo_data[fifo_index + 13] << 8 |
					fifo_data[fifo_index + 12];

			acc_frame_arr[acc_frm_cnt].x =
				fifo_data[fifo_index + 15] << 8 |
					fifo_data[fifo_index + 14];
			acc_frame_arr[acc_frm_cnt].y =
				fifo_data[fifo_index + 17] << 8 |
					fifo_data[fifo_index + 16];
			acc_frame_arr[acc_frm_cnt].z =
				fifo_data[fifo_index + 19] << 8 |
					fifo_data[fifo_index + 18];

			mag_frm_cnt++;/* M fram_cnt++ */
			gyro_frm_cnt++;/* G fram_cnt++ */
			acc_frm_cnt++;/* A fram_cnt++ */

			fifo_index = fifo_index + MGA_BYTES_FRM;
			break;
		}

		case FIFO_HEAD_M_A:
		{/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + MA_BYTES_FRM > fifo_length) {
				last_return_st = FIFO_M_A_OVER_LEN;
				break;
			}

			mag_frm_cnt = acc_frm_cnt;

			mag_frame_arr[mag_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			mag_frame_arr[mag_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			mag_frame_arr[mag_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];
			mag_frame_arr[mag_frm_cnt].r =
				fifo_data[fifo_index + 7] << 8 |
					fifo_data[fifo_index + 6];

			acc_frame_arr[acc_frm_cnt].x =
				fifo_data[fifo_index + 9] << 8 |
					fifo_data[fifo_index + 8];
			acc_frame_arr[acc_frm_cnt].y =
				fifo_data[fifo_index + 11] << 8 |
					fifo_data[fifo_index + 10];
			acc_frame_arr[acc_frm_cnt].z =
				fifo_data[fifo_index + 13] << 8 |
					fifo_data[fifo_index + 12];

			mag_frm_cnt++;/* M fram_cnt++ */
			acc_frm_cnt++;/* A fram_cnt++ */

			fifo_index = fifo_index + MA_BYTES_FRM;
			break;
		}

		case FIFO_HEAD_M_G:
		{/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + MG_BYTES_FRM > fifo_length) {
				last_return_st = FIFO_M_G_OVER_LEN;
				break;
			}

			mag_frm_cnt = gyro_frm_cnt;
			mag_frame_arr[mag_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			mag_frame_arr[mag_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			mag_frame_arr[mag_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];
			mag_frame_arr[mag_frm_cnt].r =
				fifo_data[fifo_index + 7] << 8 |
					fifo_data[fifo_index + 6];

			gyro_frame_arr[gyro_frm_cnt].x =
				fifo_data[fifo_index + 9] << 8 |
					fifo_data[fifo_index + 8];
			gyro_frame_arr[gyro_frm_cnt].y =
				fifo_data[fifo_index + 11] << 8 |
					fifo_data[fifo_index + 10];
			gyro_frame_arr[gyro_frm_cnt].z =
				fifo_data[fifo_index + 13] << 8 |
					fifo_data[fifo_index + 12];

			mag_frm_cnt++;/* M fram_cnt++ */
			gyro_frm_cnt++;/* G fram_cnt++ */
			fifo_index = fifo_index + MG_BYTES_FRM;
		break;
		}

		case FIFO_HEAD_G_A:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + GA_BYTES_FRM > fifo_length) {
				last_return_st = FIFO_G_A_OVER_LEN;
				break;
			}
			gyro_frame_arr[gyro_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			gyro_frame_arr[gyro_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			gyro_frame_arr[gyro_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];

			acc_frame_arr[acc_frm_cnt].x =
				fifo_data[fifo_index + 7] << 8 |
					fifo_data[fifo_index + 6];
			acc_frame_arr[acc_frm_cnt].y =
				fifo_data[fifo_index + 9] << 8 |
					fifo_data[fifo_index + 8];
			acc_frame_arr[acc_frm_cnt].z =
				fifo_data[fifo_index + 11] << 8 |
					fifo_data[fifo_index + 10];

			bmi_remap_data_gyro(client_data,
				&gyro_frame_arr[gyro_frm_cnt]);
			bmi_remap_data_acc(client_data,
				&acc_frame_arr[acc_frm_cnt]);

			gyro_frm_cnt++;
			acc_frm_cnt++;
			fifo_index = fifo_index + GA_BYTES_FRM;

			break;
		}
		case FIFO_HEAD_A:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + A_BYTES_FRM > fifo_length) {
				last_return_st = FIFO_A_OVER_LEN;
				break;
			}

			acc_frame_arr[acc_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			acc_frame_arr[acc_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			acc_frame_arr[acc_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];

			bmi_remap_data_acc(client_data,
				&acc_frame_arr[acc_frm_cnt]);

			acc_frm_cnt++;/*acc_frm_cnt*/
			fifo_index = fifo_index + A_BYTES_FRM;
			break;
		}
		case FIFO_HEAD_G:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + G_BYTES_FRM > fifo_length) {
				last_return_st = FIFO_G_OVER_LEN;
				break;
			}

			gyro_frame_arr[gyro_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			gyro_frame_arr[gyro_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			gyro_frame_arr[gyro_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];

			bmi_remap_data_gyro(client_data,
				&gyro_frame_arr[gyro_frm_cnt]);

			gyro_frm_cnt++;/*gyro_frm_cnt*/

			fifo_index = fifo_index + G_BYTES_FRM;
			break;
		}
		case FIFO_HEAD_M:
		{	/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;
			if (fifo_index + A_BYTES_FRM > fifo_length) {
				last_return_st = FIFO_M_OVER_LEN;
				break;
			}

			mag_frame_arr[mag_frm_cnt].x =
				fifo_data[fifo_index + 1] << 8 |
					fifo_data[fifo_index + 0];
			mag_frame_arr[mag_frm_cnt].y =
				fifo_data[fifo_index + 3] << 8 |
					fifo_data[fifo_index + 2];
			mag_frame_arr[mag_frm_cnt].z =
				fifo_data[fifo_index + 5] << 8 |
					fifo_data[fifo_index + 4];
			mag_frame_arr[mag_frm_cnt].r =
				fifo_data[fifo_index + 7] << 8 |
					fifo_data[fifo_index + 6];

			mag_frm_cnt++;/* M fram_cnt++ */

			fifo_index = fifo_index + M_BYTES_FRM;
			break;
		}

		/* sensor time frame*/
		case FIFO_HEAD_SENSOR_TIME:
		{
			/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;

			if (fifo_index + 3 > fifo_length) {
				last_return_st = FIFO_SENSORTIME_RETURN;
				break;
			}
			fifo_time =
				fifo_data[fifo_index + 2] << 16 |
				fifo_data[fifo_index + 1] << 8 |
				fifo_data[fifo_index + 0];

			client_data->fifo_time = fifo_time;
			/*fifo sensor time frame index + 3*/
			fifo_index = fifo_index + 3;
			break;
		}
		case FIFO_HEAD_OVER_READ_LSB:
			/*fifo data frame index + 1*/
			fifo_index = fifo_index + 1;

			if (fifo_index + 1 > fifo_length) {
				last_return_st = FIFO_OVER_READ_RETURN;
				break;
			}
			if (fifo_data[fifo_index] ==
					FIFO_HEAD_OVER_READ_MSB) {
				/*fifo over read frame index + 1*/
				fifo_index = fifo_index + 1;
				break;
			} else {
				last_return_st = FIFO_OVER_READ_RETURN;
				break;
			}

		default:
			last_return_st = 1;
			break;

			}
			if (last_return_st)
				break;
		}
	fifo_time = 0;
/*current_frm_ts = current_apts_us -
((fifo_time & (sensortime_duration_tbl[odr.acc_odr].ts_delat)) +
(sensortime_duration_tbl[odr.acc_odr].ts_duration_lsb
*(acc_frm_cnt - i - 1)))*625/16;*/
/*Acc Only*/
	if (client_data->fifo_data_sel == BMI_FIFO_A_SEL) {
		for (i = 0; i < acc_frm_cnt; i++) {
			/*current_frm_ts += 256;*/
			current_frm_ts +=
		sensortime_duration_tbl[odr.acc_odr].ts_duration_us*LMADA;

			len = sprintf(buf, "%s %d %d %d %u ",
					ACC_FIFO_HEAD,
					acc_frame_arr[i].x,
					acc_frame_arr[i].y,
					acc_frame_arr[i].z,
					current_frm_ts);
			buf += len;
			err += len;
		}
	}


	/*only for G*/
	if (client_data->fifo_data_sel == BMI_FIFO_G_SEL) {
		for (i = 0; i < gyro_frm_cnt; i++) {
			/*current_frm_ts += 256;*/
			current_frm_ts +=
		sensortime_duration_tbl[odr.gyro_odr].ts_duration_us*LMADA;

			len = sprintf(buf, "%s %d %d %d %u ",
						GYRO_FIFO_HEAD,
						gyro_frame_arr[i].x,
						gyro_frame_arr[i].y,
						gyro_frame_arr[i].z,
						current_frm_ts
					);
			buf += len;
			err += len;
		}
	}

	/*only for M*/
	if (client_data->fifo_data_sel == BMI_FIFO_M_SEL) {
		for (i = 0; i < mag_frm_cnt; i++) {
			/*current_frm_ts += 256;*/
			current_frm_ts +=
		sensortime_duration_tbl[odr.mag_odr].ts_duration_us*LMADA;
#ifdef BMI160_AKM09912_SUPPORT
			mag_comp_xyz.x = mag_frame_arr[i].x;
			mag_comp_xyz.y = mag_frame_arr[i].y;
			mag_comp_xyz.z = mag_frame_arr[i].z;
			bmi160_bst_akm09912_compensate_xyz_raw(
			&mag_comp_xyz);
#else
			mag.x = mag_frame_arr[i].x >> 3;
			mag.y = mag_frame_arr[i].y >> 3;
			mag.z = mag_frame_arr[i].z >> 1;
			mag.r = mag_frame_arr[i].r >> 2;
			bmi160_bmm150_mag_compensate_xyz_raw(
			&mag_comp_xyz, mag);
#endif
			len = sprintf(buf, "%s %d %d %d %u ",
						MAG_FIFO_HEAD,
						mag_comp_xyz.x,
						mag_comp_xyz.y,
						mag_comp_xyz.z,
						current_frm_ts
					);

			buf += len;
			err += len;
		}

	}

/*only for A M G*/
if (client_data->fifo_data_sel == BMI_FIFO_M_G_A_SEL) {

	for (i = 0; i < gyro_frm_cnt; i++) {
		/*sensor timeLSB*/
		/*dia(sensor_time) = fifo_time & (0xff), uint:LSB, 39.0625us*/
		/*AP tinmestamp 390625/10000 = 625 /16 */
		current_frm_ts +=
		sensortime_duration_tbl[odr.gyro_odr].ts_duration_us*LMADA;

		if (mag_frame_arr[i].x) {
#ifdef BMI160_AKM09912_SUPPORT
			mag_comp_xyz.x = mag_frame_arr[i].x;
			mag_comp_xyz.y = mag_frame_arr[i].y;
			mag_comp_xyz.z = mag_frame_arr[i].z;
			bmi160_bst_akm09912_compensate_xyz_raw(
				&mag_comp_xyz);
#else
			mag.x = mag_frame_arr[i].x >> 3;
			mag.y = mag_frame_arr[i].y >> 3;
			mag.z = mag_frame_arr[i].z >> 1;
			mag.r = mag_frame_arr[i].r >> 2;
			bmi160_bmm150_mag_compensate_xyz_raw(
				&mag_comp_xyz, mag);
#endif
				len = sprintf(buf,
			"%s %d %d %d %u %s %d %d %d %u %s %d %d %d %u ",
					GYRO_FIFO_HEAD,
					gyro_frame_arr[i].x,
					gyro_frame_arr[i].y,
					gyro_frame_arr[i].z,
					current_frm_ts,
					ACC_FIFO_HEAD,
					acc_frame_arr[i].x,
					acc_frame_arr[i].y,
					acc_frame_arr[i].z,
					current_frm_ts,
					MAG_FIFO_HEAD,
					mag_comp_xyz.x,
					mag_comp_xyz.y,
					mag_comp_xyz.z,
					current_frm_ts);
				buf += len;
				err += len;
		} else {
				len = sprintf(buf,
			"%s %d %d %d %u %s %d %d %d %u ",
					GYRO_FIFO_HEAD,
					gyro_frame_arr[i].x,
					gyro_frame_arr[i].y,
					gyro_frame_arr[i].z,
					current_frm_ts,
					ACC_FIFO_HEAD,
					acc_frame_arr[i].x,
					acc_frame_arr[i].y,
					acc_frame_arr[i].z,
					current_frm_ts
					);

				buf += len;
				err += len;
		}
	}

}
/*only for A G*/
if (client_data->fifo_data_sel == BMI_FIFO_G_A_SEL) {

	for (i = 0; i < gyro_frm_cnt; i++) {
		/*sensor timeLSB*/
		/*dia(sensor_time) = fifo_time & (0xff), uint:LSB, 39.0625us*/
		/*AP tinmestamp 390625/10000 = 625 /16 */
		current_frm_ts +=
		sensortime_duration_tbl[odr.gyro_odr].ts_duration_us*LMADA;
		len = sprintf(buf,
	"%s %d %d %d %u %s %d %d %d %u ",
			GYRO_FIFO_HEAD,
			gyro_frame_arr[i].x,
			gyro_frame_arr[i].y,
			gyro_frame_arr[i].z,
			current_frm_ts,
			ACC_FIFO_HEAD,
			acc_frame_arr[i].x,
			acc_frame_arr[i].y,
			acc_frame_arr[i].z,
			current_frm_ts
			);
		buf += len;
		err += len;
		}
	}

/*only for A M */
if (client_data->fifo_data_sel == BMI_FIFO_M_A_SEL) {
	for (i = 0; i < acc_frm_cnt; i++) {
		/*sensor timeLSB*/
		/*dia(sensor_time) = fifo_time & (0xff), uint:LSB, 39.0625us*/
		/*AP tinmestamp 390625/10000 = 625 /16 */
		/*current_frm_ts += 256;*/
		current_frm_ts +=
		sensortime_duration_tbl[odr.acc_odr].ts_duration_us*LMADA;

		if (mag_frame_arr[i].x) {
#ifdef BMI160_AKM09912_SUPPORT
			mag_comp_xyz.x = mag_frame_arr[i].x;
			mag_comp_xyz.y = mag_frame_arr[i].y;
			mag_comp_xyz.z = mag_frame_arr[i].z;
			bmi160_bst_akm09912_compensate_xyz_raw(
			&mag_comp_xyz);
#else
			mag.x = mag_frame_arr[i].x >> 3;
			mag.y = mag_frame_arr[i].y >> 3;
			mag.z = mag_frame_arr[i].z >> 1;
			mag.r = mag_frame_arr[i].r >> 2;
			bmi160_bmm150_mag_compensate_xyz_raw(
			&mag_comp_xyz, mag);
#endif
			len = sprintf(buf,
			"%s %d %d %d %u %s %d %d %d %u ",
					ACC_FIFO_HEAD,
					acc_frame_arr[i].x,
					acc_frame_arr[i].y,
					acc_frame_arr[i].z,
					current_frm_ts,
					MAG_FIFO_HEAD,
					mag_comp_xyz.x,
					mag_comp_xyz.y,
					mag_comp_xyz.z,
					current_frm_ts);
				buf += len;
				err += len;
		} else {
			len = sprintf(buf, "%s %d %d %d %u ",
					ACC_FIFO_HEAD,
					acc_frame_arr[i].x,
					acc_frame_arr[i].y,
					acc_frame_arr[i].z,
					current_frm_ts
					);

				buf += len;
				err += len;
		}
	}
}

/*only forG M*/
if (client_data->fifo_data_sel == BMI_FIFO_M_G_SEL) {
	if (gyro_frm_cnt) {
		tmp_frm_cnt = gyro_frm_cnt;
		/*tmp_odr = odr.gyro_odr;*/
	}

	for (i = 0; i < tmp_frm_cnt; i++) {
		current_frm_ts +=
		sensortime_duration_tbl[odr.gyro_odr].ts_duration_us*LMADA;
		if (mag_frame_arr[i].x) {
#ifdef BMI160_AKM09912_SUPPORT
			mag_comp_xyz.x = mag_frame_arr[i].x;
			mag_comp_xyz.y = mag_frame_arr[i].y;
			mag_comp_xyz.z = mag_frame_arr[i].z;
			bmi160_bst_akm09912_compensate_xyz_raw(
			&mag_comp_xyz);
#else
			mag.x = mag_frame_arr[i].x >> 3;
			mag.y = mag_frame_arr[i].y >> 3;
			mag.z = mag_frame_arr[i].z >> 1;
			mag.r = mag_frame_arr[i].r >> 2;
			bmi160_bmm150_mag_compensate_xyz_raw(
			&mag_comp_xyz, mag);
#endif
			len = sprintf(buf,
		"%s %d %d %d %u %s %d %d %d %u ",
				GYRO_FIFO_HEAD,
				gyro_frame_arr[i].x,
				gyro_frame_arr[i].y,
				gyro_frame_arr[i].z,
				current_frm_ts,
				MAG_FIFO_HEAD,
				mag_comp_xyz.x,
				mag_comp_xyz.y,
				mag_comp_xyz.z,
				current_frm_ts);
			buf += len;
			err += len;
		} else {
			len = sprintf(buf, "%s %d %d %d %u ",
				GYRO_FIFO_HEAD,
				gyro_frame_arr[i].x,
				gyro_frame_arr[i].y,
				gyro_frame_arr[i].z,
				current_frm_ts
				);

				buf += len;
				err += len;
		}
	}
}


	return err;


}


static ssize_t bmi160_fifo_data_out_frame_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	int err = 0;
	unsigned int fifo_bytecount_tmp;

	if (client_data->selftest > 0) {
		dev_err(client_data->dev,
			"in selftest no data available in fifo_data_frame\n");
		return -ENOMEM;
	}

	if (NULL == g_fifo_data_arr) {
			dev_err(client_data->dev,
				"no memory available in fifo_data_frame\n");
			return -ENOMEM;
	}

	if (client_data->pw.acc_pm == 2 && client_data->pw.gyro_pm == 2
					&& client_data->pw.mag_pm == 2) {
		dev_err(client_data->dev, "pw_acc: %d, pw_gyro: %d, pw_mag:%d\n",
			client_data->pw.acc_pm, client_data->pw.gyro_pm,
				client_data->pw.mag_pm);
		return -EINVAL;
	}
	if (!client_data->fifo_data_sel)
		return sprintf(buf,
			"no selsect sensor fifo, fifo_data_sel:%d\n",
						client_data->fifo_data_sel);

	if (client_data->fifo_bytecount == 0)
		return -EINVAL;

	g_current_apts_us = get_current_timestamp();

	BMI_CALL_API(fifo_length)(&fifo_bytecount_tmp);
	if (fifo_bytecount_tmp > client_data->fifo_bytecount)
		client_data->fifo_bytecount = fifo_bytecount_tmp;
	if (client_data->fifo_bytecount > 210) {
		/*err += BMI_CALL_API(set_command_register)(
		CMD_CLR_FIFO_DATA);*/
		client_data->fifo_bytecount = 210;
	}
	if (!err) {
		memset(g_fifo_data_arr, 0, 2048);
		err = bmi_burst_read_wrapper(client_data->device.dev_addr,
			BMI160_USER_FIFO_DATA__REG, g_fifo_data_arr,
						client_data->fifo_bytecount);
	} else
		dev_err(client_data->dev, "read fifo leght err");
	if (err) {
		dev_err(client_data->dev, "brust read fifo err\n");
		return err;
	}
		err = bmi_fifo_analysis_handle(client_data, g_fifo_data_arr,
			client_data->fifo_bytecount, buf);


	return err;
}

static ssize_t bmi160_fifo_watermark_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data = 0xff;

	err = BMI_CALL_API(get_fifo_wm)(&data);

	if (err)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_fifo_watermark_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long data;
	unsigned char fifo_watermark;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	fifo_watermark = (unsigned char)data;
	err = BMI_CALL_API(set_fifo_wm)(fifo_watermark);
	if (err)
		return -EIO;

	pr_info("fifo_watermark count %d", count);

	if (BMI_CALL_API(set_intr_enable_1)
		(BMI160_FIFO_WM_ENABLE, 1) < 0) {
		pr_info("set fifo wm enable failed");
		return -EIO;
	}
	mutex_lock(&client_data->mutex_ring_buf);
	s_ring_buf_head = s_ring_buf_tail = 0;
	mutex_unlock(&client_data->mutex_ring_buf);

	return count;
}


static ssize_t bmi160_fifo_header_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data = 0xff;

	err = BMI_CALL_API(get_fifo_header_enable)(&data);

	if (err)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_fifo_header_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	int err;
	unsigned long data;
	unsigned char fifo_header_en;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	if (data > 1)
		return -ENOENT;

	fifo_header_en = (unsigned char)data;
	err = BMI_CALL_API(set_fifo_header_enable)(fifo_header_en);
	if (err)
		return -EIO;

	client_data->fifo_head_en = fifo_header_en;

	return count;
}

static ssize_t bmi160_fifo_time_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data = 0;

	err = BMI_CALL_API(get_fifo_time_enable)(&data);

	if (!err)
		err = sprintf(buf, "%d\n", data);

	return err;
}

static ssize_t bmi160_fifo_time_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long data;
	unsigned char fifo_ts_en;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	fifo_ts_en = (unsigned char)data;

	err = BMI_CALL_API(set_fifo_time_enable)(fifo_ts_en);
	if (err)
		return -EIO;

	return count;
}

static ssize_t bmi160_fifo_int_tag_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	unsigned char fifo_tag_int1 = 0;
	unsigned char fifo_tag_int2 = 0;
	unsigned char fifo_tag_int;

	err += BMI_CALL_API(get_fifo_tag_intr1_enable)(&fifo_tag_int1);
	err += BMI_CALL_API(get_fifo_tag_intr2_enable)(&fifo_tag_int2);

	fifo_tag_int = (fifo_tag_int1 << BMI160_INT0) |
			(fifo_tag_int2 << BMI160_INT1);

	if (!err)
		err = sprintf(buf, "%d\n", fifo_tag_int);

	return err;
}

static ssize_t bmi160_fifo_int_tag_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	int err;
	unsigned long data;
	unsigned char fifo_tag_int_en;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	if (data > 3)
		return -EINVAL;

	fifo_tag_int_en = (unsigned char)data;

	err += BMI_CALL_API(set_fifo_tag_intr1_enable)
			((fifo_tag_int_en & (1 << BMI160_INT0)) ? 1 :  0);
	err += BMI_CALL_API(set_fifo_tag_intr2_enable)
			((fifo_tag_int_en & (1 << BMI160_INT1)) ? 1 :  0);

	if (err) {
		dev_err(client_data->dev, "fifo int tag en err:%d\n", err);
		return -EIO;
	}
	client_data->fifo_int_tag_en = fifo_tag_int_en;

	return count;
}

static int bmi160_set_acc_op_mode(struct bmi_client_data *client_data,
							unsigned long op_mode)
{
	int err = 0;
	unsigned char stc_enable;
	unsigned char std_enable;
	mutex_lock(&client_data->mutex_op_mode);

	if (op_mode < BMI_ACC_PM_MAX) {
		switch (op_mode) {
		case BMI_ACC_PM_NORMAL:
			err = BMI_CALL_API(set_command_register)
			(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_NORMAL]);
			client_data->pw.acc_pm = BMI_ACC_PM_NORMAL;
			mdelay(10);
			break;
		case BMI_ACC_PM_LP1:
			err = BMI_CALL_API(set_command_register)
			(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_LP1]);
			client_data->pw.acc_pm = BMI_ACC_PM_LP1;
			mdelay(3);
			break;
		case BMI_ACC_PM_SUSPEND:
			BMI_CALL_API(get_step_counter_enable)(&stc_enable);
			BMI_CALL_API(get_step_detector_enable)(&std_enable);
			if ((stc_enable == 0) && (std_enable == 0) &&
				(client_data->sig_flag == 0)) {
				err = BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_SUSPEND]);
				client_data->pw.acc_pm = BMI_ACC_PM_SUSPEND;
				mdelay(10);
			}
			break;
		case BMI_ACC_PM_LP2:
			err = BMI_CALL_API(set_command_register)
			(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_LP2]);
			client_data->pw.acc_pm = BMI_ACC_PM_LP2;
			mdelay(3);
			break;
		default:
			mutex_unlock(&client_data->mutex_op_mode);
			return -EINVAL;
		}
	} else {
		mutex_unlock(&client_data->mutex_op_mode);
		return -EINVAL;
	}

	mutex_unlock(&client_data->mutex_op_mode);

	return err;


}

static int bmi160_set_gyro_op_mode(struct bmi_client_data *client_data,
					unsigned long op_mode)
{
	int err = 0;

	mutex_lock(&client_data->mutex_op_mode);

	if (op_mode < BMI_GYRO_PM_MAX) {
		switch (op_mode) {
		case BMI_GYRO_PM_NORMAL:
			err = BMI_CALL_API(set_command_register)
			(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_NORMAL]);
			client_data->pw.gyro_pm = BMI_GYRO_PM_NORMAL;
			msleep(60);
			break;
		case BMI_GYRO_PM_FAST_START:
			err = BMI_CALL_API(set_command_register)
			(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_FAST_START]);
			client_data->pw.gyro_pm = BMI_GYRO_PM_FAST_START;
			msleep(60);
			break;
		case BMI_GYRO_PM_SUSPEND:
			err = BMI_CALL_API(set_command_register)
			(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_SUSPEND]);
			client_data->pw.gyro_pm = BMI_GYRO_PM_SUSPEND;
			msleep(60);
			break;
		default:
			mutex_unlock(&client_data->mutex_op_mode);
			return -EINVAL;
		}
	} else {
		mutex_unlock(&client_data->mutex_op_mode);
		return -EINVAL;
	}

	mutex_unlock(&client_data->mutex_op_mode);
	return err;

}

static ssize_t bmi160_temperature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	s16 temp = 0xff;

	err = BMI_CALL_API(get_temp)(&temp);

	if (!err)
		err = sprintf(buf, "0x%x\n", temp);

	return err;
}

static ssize_t bmi160_place_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	int place = BOSCH_SENSOR_PLACE_UNKNOWN;

	if (NULL != client_data->bst_pd)
		place = client_data->bst_pd->place;

	return sprintf(buf, "%d\n", place);
}

static ssize_t bmi160_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", atomic_read(&client_data->delay));

}

static ssize_t bmi160_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	int err;
	unsigned long data;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	if (data == 0) {
		err = -EINVAL;
		return err;
	}

	if (data < BMI_DELAY_MIN)
		data = BMI_DELAY_MIN;

	atomic_set(&client_data->delay, (unsigned int)data);

	return count;
}

static ssize_t bmi160_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", atomic_read(&client_data->wkqueue_en));

}

static ssize_t bmi160_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	int err;
	unsigned long enable;
	int pre_enable = atomic_read(&client_data->wkqueue_en);

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;

	enable = enable ? 1 : 0;
	mutex_lock(&client_data->mutex_enable);
	if (enable) {
		if (pre_enable == 0) {
			if (bmi160_power_ctl(client_data, true)) {
				dev_err(dev, "power up sensor failed.\n");
				goto mutex_exit;
			}
			bmi160_set_acc_op_mode(client_data,
							BMI_ACC_PM_NORMAL);
			schedule_delayed_work(&client_data->work,
			msecs_to_jiffies(atomic_read(&client_data->delay)));
			atomic_set(&client_data->wkqueue_en, 1);
		}

	} else {
		if (pre_enable == 1) {
			bmi160_set_acc_op_mode(client_data,
							BMI_ACC_PM_SUSPEND);

			cancel_delayed_work_sync(&client_data->work);
			atomic_set(&client_data->wkqueue_en, 0);
			if (bmi160_power_ctl(client_data, false)) {
				dev_err(dev, "power up sensor failed.\n");
				goto mutex_exit;
			}
		}
	}

mutex_exit:
	mutex_unlock(&client_data->mutex_enable);

        mutex_lock(&client_data->mutex_ring_buf);
        s_ring_buf_head = s_ring_buf_tail = 0;
        mutex_unlock(&client_data->mutex_ring_buf);

	return count;
}

#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
/* accel sensor part */
static ssize_t bmi160_anymot_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char data;

	err = BMI_CALL_API(get_intr_any_motion_durn)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_anymot_duration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_intr_any_motion_durn)((unsigned char)data);
	if (err < 0)
		return -EIO;

	return count;
}

static ssize_t bmi160_anymot_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_intr_any_motion_thres)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_anymot_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_intr_any_motion_thres)((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_step_detector_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 data = 0;
	u8 step_det;
	int err;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	err = BMI_CALL_API(get_step_detector_enable)(&step_det);
	/*bmi160_get_status0_step_int*/
	if (err < 0)
		return err;
/*client_data->std will be updated in bmi_stepdetector_interrupt_handle */
	if ((step_det == 1) && (client_data->std == 1)) {
		data = 1;
		client_data->std = 0;
		}
	else {
		data = 0;
		}
	return snprintf(buf, 16, "%d\n", data);
}

static ssize_t bmi160_step_detector_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_step_detector_enable)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_step_detector_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_step_detector_enable)((unsigned char)data);
	if (err < 0)
		return -EIO;
	if (data == 0)
		client_data->pedo_data.wkar_step_detector_status = 0;
	return count;
}

static ssize_t bmi160_signification_motion_enable_store(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/*0x62 (bit 1) INT_MOTION_3 int_sig_mot_sel*/
	err = BMI_CALL_API(set_intr_significant_motion_select)(
		(unsigned char)data);
	if (err < 0)
		return -EIO;
	if (data == 1) {
		err = BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_X_ENABLE, 1);
		err += BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_Y_ENABLE, 1);
		err += BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_Z_ENABLE, 1);
		if (err < 0)
			return -EIO;
		client_data->sig_flag = 1;
	} else {
		err = BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_X_ENABLE, 0);
		err += BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_Y_ENABLE, 0);
		err += BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_Z_ENABLE, 0);
		if (err < 0)
			return -EIO;
		client_data->sig_flag = 0;
	}
	return count;
}

static ssize_t bmi160_signification_motion_enable_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;
	/*0x62 (bit 1) INT_MOTION_3 int_sig_mot_sel*/
	err = BMI_CALL_API(get_intr_significant_motion_select)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static int sigmotion_init_interrupts(u8 sig_map_int_pin)
{
	int ret = 0;
/*0x60  */
	ret += bmi160_set_intr_any_motion_thres(0x1e);
/* 0x62(bit 3~2)	0=1.5s */
	ret += bmi160_set_intr_significant_motion_skip(0);
/*0x62(bit 5~4)	1=0.5s*/
	ret += bmi160_set_intr_significant_motion_proof(1);
/*0x50 (bit 0, 1, 2)  INT_EN_0 anymo x y z*/
	ret += bmi160_map_significant_motion_intr(sig_map_int_pin);
/*0x62 (bit 1) INT_MOTION_3	int_sig_mot_sel
close the signification_motion*/
	ret += bmi160_set_intr_significant_motion_select(0);
/*close the anymotion interrupt*/
	ret += BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_X_ENABLE, 0);
	ret += BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_Y_ENABLE, 0);
	ret += BMI_CALL_API(set_intr_enable_0)
					(BMI160_ANY_MOTION_Z_ENABLE, 0);
	if (ret)
		pr_info("bmi160 sig motion failed setting,%d!\n", ret);
	return ret;

}
#endif

static ssize_t bmi160_acc_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char range;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = BMI_CALL_API(get_accel_range)(&range);
	if (err)
		return err;

	client_data->range.acc_range = range;
	return sprintf(buf, "%d\n", range);
}

static ssize_t bmi160_acc_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long range;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &range);
	if (err)
		return err;

	err = BMI_CALL_API(set_accel_range)(range);
	if (err)
		return -EIO;

	client_data->range.acc_range = range;
	return count;
}

static ssize_t bmi160_acc_odr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char acc_odr;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = BMI_CALL_API(get_accel_output_data_rate)(&acc_odr);
	if (err)
		return err;

	client_data->odr.acc_odr = acc_odr;
	return sprintf(buf, "%d\n", acc_odr);
}

static ssize_t bmi160_acc_odr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long acc_odr;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &acc_odr);
	if (err)
		return err;

	if (acc_odr < 1 || acc_odr > 12)
		return -EIO;

	if (acc_odr < 5)
		err = BMI_CALL_API(set_accel_under_sampling_parameter)(1);
	else
		err = BMI_CALL_API(set_accel_under_sampling_parameter)(0);

	if (err)
		return err;

	err = BMI_CALL_API(set_accel_output_data_rate)(acc_odr);
	if (err)
		return -EIO;
	client_data->odr.acc_odr = acc_odr;
	return count;
}

static ssize_t bmi160_acc_op_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	int err = 0;
	u8 accel_pmu_status = 0;
	err = BMI_CALL_API(get_accel_power_mode_stat)(
		&accel_pmu_status);

	if (err)
		return err;
	else
		return sprintf(buf, "reg:%d, val:%d\n", accel_pmu_status,
			client_data->pw.acc_pm);
}

static ssize_t bmi160_acc_op_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	int err;
	unsigned long op_mode;
	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;

	err = bmi160_set_acc_op_mode(client_data, op_mode);
	if (err)
		return err;
	else
		return count;

}

static ssize_t bmi160_acc_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	struct bmi160_accel_t data;
	struct bmi160_axis_data_t bmi160_udata;
	int err;

	err = BMI_CALL_API(read_accel_xyz)(&data);
	if (err < 0)
		return err;

	bmi160_udata.x = data.x;
	bmi160_udata.y = data.y;
	bmi160_udata.z = data.z;

	bmi_remap_sensor_data(&bmi160_udata, client_data);
	return sprintf(buf, "%hd %hd %hd\n",
			bmi160_udata.x, bmi160_udata.y, bmi160_udata.z);
}

static ssize_t bmi160_acc_fast_calibration_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_foc_accel_x)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_acc_fast_calibration_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	s8 accel_offset_x = 0;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* 0: disable, 1: +1g, 2: -1g, 3: 0g */
	if (data > 3)
		return -EINVAL;

	err = BMI_CALL_API(set_accel_foc_trigger)(X_AXIS,
					data, &accel_offset_x);
	if (err)
		return -EIO;
	else
		client_data->calib_status |=
			BMI_FAST_CALI_TRUE << BMI_ACC_X_FAST_CALI_RDY;
	return count;
}

static ssize_t bmi160_acc_fast_calibration_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_foc_accel_y)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_acc_fast_calibration_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	s8 accel_offset_y = 0;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* 0: disable, 1: +1g, 2: -1g, 3: 0g */
	if (data > 3)
		return -EINVAL;

	err = BMI_CALL_API(set_accel_foc_trigger)(Y_AXIS,
				data, &accel_offset_y);
	if (err)
		return -EIO;
	else
		client_data->calib_status |=
			BMI_FAST_CALI_TRUE << BMI_ACC_Y_FAST_CALI_RDY;
	return count;
}

static ssize_t bmi160_acc_fast_calibration_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_foc_accel_z)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_acc_fast_calibration_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	s8 accel_offset_z = 0;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* 0: disable, 1: +1g, 2: -1g, 3: 0g */
	if (data > 3)
		return -EINVAL;

	err = BMI_CALL_API(set_accel_foc_trigger)(Z_AXIS,
			data, &accel_offset_z);
	if (err)
		return -EIO;
	else
		client_data->calib_status |=
			BMI_FAST_CALI_TRUE << BMI_ACC_Z_FAST_CALI_RDY;

	if (client_data->calib_status == BMI_FAST_CALI_ALL_RDY) {
		input_event(client_data->input_accel, EV_MSC,
		INPUT_EVENT_FAST_ACC_CALIB_DONE, 1);
		input_sync(client_data->input_accel);
		client_data->calib_status = 0;
	}

	return count;
}

static ssize_t bmi160_acc_offset_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_accel_offset_compensation_xaxis)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}


static ssize_t bmi160_acc_offset_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_accel_offset_compensation_xaxis)
						((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_acc_offset_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_accel_offset_compensation_yaxis)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_acc_offset_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_accel_offset_compensation_yaxis)
						((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_acc_offset_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_accel_offset_compensation_zaxis)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_acc_offset_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_accel_offset_compensation_zaxis)
						((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	u8 raw_data[15] = {0};
	unsigned int sensor_time = 0;

	int err;
	memset(raw_data, 0, sizeof(raw_data));

	err = client_data->device.bus_read(client_data->device.dev_addr,
			BMI160_USER_DATA_8_GYRO_X_LSB__REG, raw_data, 15);
	if (err)
		return err;

	udelay(10);
	sensor_time = (u32)(raw_data[14] << 16 | raw_data[13] << 8
						| raw_data[12]);

	return sprintf(buf, "%d %d %d %d %d %d %u",
					(s16)(raw_data[1] << 8 | raw_data[0]),
				(s16)(raw_data[3] << 8 | raw_data[2]),
				(s16)(raw_data[5] << 8 | raw_data[4]),
				(s16)(raw_data[7] << 8 | raw_data[6]),
				(s16)(raw_data[9] << 8 | raw_data[8]),
				(s16)(raw_data[11] << 8 | raw_data[10]),
				sensor_time);

}

static ssize_t bmi160_step_counter_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = BMI_CALL_API(get_step_counter_enable)(&data);

	client_data->stc_enable = data;

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_step_counter_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_step_counter_enable)((unsigned char)data);

	client_data->stc_enable = data;

	if (err < 0)
		return -EIO;
	return count;
}


static ssize_t bmi160_step_counter_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_step_mode)((unsigned char)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_step_counter_clc_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = bmi160_clear_step_counter();

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_step_counter_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s16 data;
	int err;
	static u16 last_stc_value;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = BMI_CALL_API(read_step_count)(&data);

	if (err < 0)
		return err;
	if (data >= last_stc_value) {
		client_data->pedo_data.last_step_counter_value += (
			data - last_stc_value);
		last_stc_value = data;
	} else
		last_stc_value = data;
	return sprintf(buf, "%d\n",
		client_data->pedo_data.last_step_counter_value);
}

static ssize_t bmi160_bmi_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	u8 raw_data[12] = {0};

	int err;
	memset(raw_data, 0, sizeof(raw_data));

	err = client_data->device.bus_read(client_data->device.dev_addr,
			BMI160_USER_DATA_8_GYRO_X_LSB__REG, raw_data, 12);
	if (err)
		return err;
	/*output:gyro x y z acc x y z*/
	return sprintf(buf, "%hd %d %hd %hd %hd %hd\n",
					(s16)(raw_data[1] << 8 | raw_data[0]),
				(s16)(raw_data[3] << 8 | raw_data[2]),
				(s16)(raw_data[5] << 8 | raw_data[4]),
				(s16)(raw_data[7] << 8 | raw_data[6]),
				(s16)(raw_data[9] << 8 | raw_data[8]),
				(s16)(raw_data[11] << 8 | raw_data[10]));

}

static int bmi_ring_buf_get(struct bmi160_value_t *pData);
static ssize_t bmi160_ring_buf_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int err = 0;
	int len;
	struct bmi160_value_t data;
	char  *tmp = buf;

	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	mutex_lock(&client_data->mutex_ring_buf);
	while (bmi_ring_buf_get(&data) != 0)
	{
#ifdef BMI160_MAG_INTERFACE_SUPPORT
		len = snprintf(tmp, PAGE_SIZE,
			 "%d %d %d %d %d %d %d %d %d %lld",
			data.acc.x, data.acc.y, data.acc.z,
			data.gyro.x, data.gyro.y, data.gyro.z,
			data.mag.x, data.mag.y, data.mag.z,
			data.ts_intvl);
		pr_info("%d %d %d %lld\n",
			data.mag.x,
			data.mag.y,
			data.mag.z,
			data.ts_intvl);
#else
		len = snprintf(tmp, PAGE_SIZE,
			 "%d %d %d %d %d %d %lld",
			data.acc.x, data.acc.y, data.acc.z,
			data.gyro.x, data.gyro.y, data.gyro.z,
			data.ts_intvl);
#endif
		tmp += len;
		err += len;
		pr_info("%d %d %d %d %d %d %lld\n",
			data.acc.x, data.acc.y, data.acc.z,
			data.gyro.x, data.gyro.y, data.gyro.z,
			data.ts_intvl);

	}
	mutex_unlock(&client_data->mutex_ring_buf);
	return err;

}

static ssize_t bmi160_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	return sprintf(buf, "0x%x\n",
				atomic_read(&client_data->selftest_result));
}

static int bmi_restore_hw_cfg(struct bmi_client_data *client);
static void bmi_delay(u32 msec);
/*!
 * @brief store selftest result which make up of acc and gyro
 * format: 0b 0000 xxxx  x:1 failed, 0 success
 * bit3:     gyro_self
 * bit2..0: acc_self z y x
 */
static ssize_t bmi160_selftest_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	int err = 0;
	int i = 0;

	u8 acc_selftest = 0;
	u8 gyro_selftest = 0;
	u8 bmi_selftest = 0;
	s16 axis_p_value, axis_n_value;
	u16 diff_axis[3] = {0xff, 0xff, 0xff};
	u8 acc_odr, range, acc_selftest_amp, acc_selftest_sign;

#ifdef BMI160_MAG_INTERFACE_SUPPORT
        struct bmi160_mag_xyz_s32_t pos_data;
        struct bmi160_mag_xyz_s32_t neg_data;
#endif

	dev_notice(client_data->dev, "Selftest for BMI16x starting.\n");

	client_data->selftest = 1;

#ifdef BMI160_MAG_INTERFACE_SUPPORT
	/* Power mode value 0x06 */
	err += bmi160_set_mag_write_data(BMI160_BMM_POWER_MODE_REG);
	bmi_delay(BMI160_GEN_READ_WRITE_DELAY);
	/*write 0x4C register to write set power mode to normal*/
	err += bmi160_set_mag_write_addr(BMI160_BMM150_POWE_MODE_REG);
	bmi_delay(BMI160_GEN_READ_WRITE_DELAY);

	/* 0x18 Sets the PMU mode for the Magnetometer to suspend */
	err += bmi160_set_mag_write_data(MAG_MODE_SUSPEND);
	bmi_delay(BMI160_GEN_READ_WRITE_DELAY);
	/* write 0x18 to register 0x4E */
	err += bmi160_set_mag_write_addr(BMI160_USER_MAG_IF_3_ADDR);
	/* write the Z repetitions*/
	/* The v_data_u8 have to write for the register
	It write the value in the register 0x4F*/
	err += bmi160_set_mag_write_data(BMI160_MAG_REGULAR_REPZ);
	bmi_delay(BMI160_GEN_READ_WRITE_DELAY);
	err += bmi160_set_mag_write_addr(BMI160_BMM150_Z_REP);
	bmi_delay(BMI160_GEN_READ_WRITE_DELAY);

	/* 0xC2 */
	err += bmi160_set_mag_write_data(0xC2);
	bmi_delay(BMI160_GEN_READ_WRITE_DELAY);
	/* write register 0x4C */
	err += bmi160_set_mag_write_addr(BMI160_BMM150_POWE_MODE_REG);
	bmi_delay(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);

	err += bmi160_bmm150_mag_compensate_xyz(&pos_data);
	bmi_delay(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	/* 0X82 */
	err += bmi160_set_mag_write_data(0x82);
	bmi_delay(BMI160_GEN_READ_WRITE_DELAY);
	/* write register 0x4C */
	err += bmi160_set_mag_write_addr(BMI160_BMM150_POWE_MODE_REG);

	bmi_delay(BMI160_SEC_INTERFACE_GEN_READ_WRITE_DELAY);
	err += bmi160_bmm150_mag_compensate_xyz(&neg_data);

	diff_axis[2] = pos_data.z - neg_data.z;
	dev_info(client_data->dev,
	"pos_data.x:%d, pos_data.y:%d, pos_data.z:%d,\nneg_data.x:%d, neg_data.y:%d, neg_data.z:%d\n",
	pos_data.x, pos_data.y, pos_data.z, neg_data.x, neg_data.y, neg_data.z);
#endif

	/*soft reset*/
	err = BMI_CALL_API(set_command_register)(CMD_RESET_USER_REG);
	msleep(70);
	err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_NORMAL]);
	err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_NORMAL]);
	err += BMI_CALL_API(set_accel_under_sampling_parameter)(0);
	err += BMI_CALL_API(set_accel_output_data_rate)(
	BMI160_ACCEL_OUTPUT_DATA_RATE_1600HZ);

	/* set to 8G range*/
	err += BMI_CALL_API(set_accel_range)(BMI160_ACCEL_RANGE_8G);
	/* set to self amp high */
	err += BMI_CALL_API(set_accel_selftest_amp)(BMI_SELFTEST_AMP_HIGH);


	err += BMI_CALL_API(get_accel_output_data_rate)(&acc_odr);
	err += BMI_CALL_API(get_accel_range)(&range);
	err += BMI_CALL_API(get_accel_selftest_amp)(&acc_selftest_amp);
	err += BMI_CALL_API(read_accel_x)(&axis_n_value);

	dev_info(client_data->dev,
			"acc_odr:%d, acc_range:%d, acc_selftest_amp:%d, acc_x:%d\n",
				acc_odr, range, acc_selftest_amp, axis_n_value);

	for (i = X_AXIS; i < AXIS_MAX; i++) {
		axis_n_value = 0;
		axis_p_value = 0;
		/* set every selftest axis */
		/*set_acc_selftest_axis(param),param x:1, y:2, z:3
		* but X_AXIS:0, Y_AXIS:1, Z_AXIS:2
		* so we need to +1*/
		err += BMI_CALL_API(set_accel_selftest_axis)(i + 1);
		msleep(50);
		switch (i) {
		case X_AXIS:
			/* set negative sign */
			err += BMI_CALL_API(set_accel_selftest_sign)(0);
			err += BMI_CALL_API(get_accel_selftest_sign)(
				&acc_selftest_sign);

			msleep(60);
			err += BMI_CALL_API(read_accel_x)(&axis_n_value);
			dev_info(client_data->dev,
			"acc_x_selftest_sign:%d, axis_n_value:%d\n",
			acc_selftest_sign, axis_n_value);

			/* set postive sign */
			err += BMI_CALL_API(set_accel_selftest_sign)(1);
			err += BMI_CALL_API(get_accel_selftest_sign)(
				&acc_selftest_sign);

			msleep(60);
			err += BMI_CALL_API(read_accel_x)(&axis_p_value);
			dev_info(client_data->dev,
			"acc_x_selftest_sign:%d, axis_p_value:%d\n",
			acc_selftest_sign, axis_p_value);
			diff_axis[i] = abs(axis_p_value - axis_n_value);
			break;

		case Y_AXIS:
			/* set negative sign */
			err += BMI_CALL_API(set_accel_selftest_sign)(0);
			msleep(60);
			err += BMI_CALL_API(read_accel_y)(&axis_n_value);
			/* set postive sign */
			err += BMI_CALL_API(set_accel_selftest_sign)(1);
			msleep(60);
			err += BMI_CALL_API(read_accel_y)(&axis_p_value);
			diff_axis[i] = abs(axis_p_value - axis_n_value);
			break;

		case Z_AXIS:
			/* set negative sign */
			err += BMI_CALL_API(set_accel_selftest_sign)(0);
			msleep(60);
			err += BMI_CALL_API(read_accel_z)(&axis_n_value);
			/* set postive sign */
			err += BMI_CALL_API(set_accel_selftest_sign)(1);
			msleep(60);
			err += BMI_CALL_API(read_accel_z)(&axis_p_value);
			/* also start gyro self test */
			err += BMI_CALL_API(set_gyro_selftest_start)(1);
			msleep(60);
			err += BMI_CALL_API(get_gyro_selftest)(&gyro_selftest);

			diff_axis[i] = abs(axis_p_value - axis_n_value);
			break;
		default:
			err += -EINVAL;
			break;
		}
		if (err) {
			dev_err(client_data->dev,
				"Failed selftest axis:%s, p_val=%d, n_val=%d\n",
				bmi_axis_name[i], axis_p_value, axis_n_value);
			client_data->selftest = 0;
			return -EINVAL;
		}

		/*400mg for acc z axis*/
		if (Z_AXIS == i) {
			if (diff_axis[i] < 1639) {
				acc_selftest |= 1 << i;
				dev_err(client_data->dev,
					"Over selftest minimum for "
					"axis:%s,diff=%d,p_val=%d, n_val=%d\n",
					bmi_axis_name[i], diff_axis[i],
						axis_p_value, axis_n_value);
			}
		} else {
			/*800mg for x or y axis*/
			if (diff_axis[i] < 3277) {
				acc_selftest |= 1 << i;

				if (bmi_get_err_status(client_data) < 0)
					return err;
				dev_err(client_data->dev,
					"Over selftest minimum for "
					"axis:%s,diff=%d, p_val=%d, n_val=%d\n",
					bmi_axis_name[i], diff_axis[i],
						axis_p_value, axis_n_value);
				dev_err(client_data->dev, "err_st:0x%x\n",
						client_data->err_st.err_st_all);

			}
		}

	}
	/* gyro_selftest==1,gyro selftest successfully,
	* but bmi_result bit4 0 is successful, 1 is failed*/
	bmi_selftest = (acc_selftest & 0x0f) | ((!gyro_selftest) << AXIS_MAX);
	atomic_set(&client_data->selftest_result, bmi_selftest);
	/*soft reset*/
	err = BMI_CALL_API(set_command_register)(CMD_RESET_USER_REG);
	if (err) {
		client_data->selftest = 0;
		return err;
	}
	msleep(50);

	bmi_restore_hw_cfg(client_data);

	client_data->selftest = 0;
	dev_notice(client_data->dev, "Selftest for BMI16x finished\n");

	return count;
}

/* gyro sensor part */
static ssize_t bmi160_gyro_op_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	int err = 0;
	u8 gyro_pmu_status = 0;

	err = BMI_CALL_API(get_gyro_power_mode_stat)(
		&gyro_pmu_status);

	if (err)
		return err;
	else
		return sprintf(buf, "reg:%d, val:%d\n", gyro_pmu_status,
				client_data->pw.gyro_pm);
}

static ssize_t bmi160_gyro_op_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	unsigned long op_mode;
	int err;

	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;

	mutex_lock(&client_data->mutex_op_mode);

	if (op_mode < BMI_GYRO_PM_MAX) {
		switch (op_mode) {
		case BMI_GYRO_PM_NORMAL:
			err = BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_NORMAL]);
			client_data->pw.gyro_pm = BMI_GYRO_PM_NORMAL;
			mdelay(60);
			break;
		case BMI_GYRO_PM_FAST_START:
			err = BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_FAST_START]);
			client_data->pw.gyro_pm = BMI_GYRO_PM_FAST_START;
			mdelay(60);
			break;
		case BMI_GYRO_PM_SUSPEND:
			err = BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_SUSPEND]);
			client_data->pw.gyro_pm = BMI_GYRO_PM_SUSPEND;
			mdelay(60);
			break;
		default:
			mutex_unlock(&client_data->mutex_op_mode);
			return -EINVAL;
		}
	} else {
		mutex_unlock(&client_data->mutex_op_mode);
		return -EINVAL;
	}

	mutex_unlock(&client_data->mutex_op_mode);

	if (err)
		return err;
	else
		return count;

}

static ssize_t bmi160_gyro_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	struct bmi160_gyro_t data;
	struct bmi160_axis_data_t bmi160_udata;
	int err;

	err = BMI_CALL_API(read_gyro_xyz)(&data);
	if (err < 0)
		return err;

	bmi160_udata.x = data.x;
	bmi160_udata.y = data.y;
	bmi160_udata.z = data.z;

	bmi_remap_sensor_data(&bmi160_udata, client_data);

	return sprintf(buf, "%hd %hd %hd\n", bmi160_udata.x,
				bmi160_udata.y, bmi160_udata.z);
}

static ssize_t bmi160_gyro_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char range;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = BMI_CALL_API(get_gyro_range)(&range);
	if (err)
		return err;

	client_data->range.gyro_range = range;
	return sprintf(buf, "%d\n", range);
}

static ssize_t bmi160_gyro_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long range;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &range);
	if (err)
		return err;

	err = BMI_CALL_API(set_gyro_range)(range);
	if (err)
		return -EIO;

	client_data->range.gyro_range = range;
	return count;
}

static ssize_t bmi160_gyro_odr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	unsigned char gyro_odr;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = BMI_CALL_API(get_gyro_output_data_rate)(&gyro_odr);
	if (err)
		return err;

	client_data->odr.gyro_odr = gyro_odr;
	return sprintf(buf, "%d\n", gyro_odr);
}

static ssize_t bmi160_gyro_odr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long gyro_odr;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &gyro_odr);
	if (err)
		return err;

	if (gyro_odr < 6 || gyro_odr > 13)
		return -EIO;

	err = BMI_CALL_API(set_gyro_output_data_rate)(gyro_odr);
	if (err)
		return -EIO;

	client_data->odr.gyro_odr = gyro_odr;
	return count;
}

static ssize_t bmi160_gyro_fast_calibration_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err;

	err = BMI_CALL_API(get_foc_gyro_enable)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_gyro_fast_calibration_en_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long enable;
	s8 err;
	s16 gyr_off_x;
	s16 gyr_off_y;
	s16 gyr_off_z;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;

	err = BMI_CALL_API(set_foc_gyro_enable)((u8)enable,
				&gyr_off_x, &gyr_off_y, &gyr_off_z);

	if (err < 0)
		return -EIO;
	else {
		input_event(client_data->input_gyro, EV_MSC,
			INPUT_EVENT_FAST_GYRO_CALIB_DONE, 1);
		input_sync(client_data->input_gyro);
	}
	return count;
}

static ssize_t bmi160_gyro_offset_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s16 data = 0;
	s8 err = 0;

	err = BMI_CALL_API(get_gyro_offset_compensation_xaxis)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_gyro_offset_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	s8 err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_gyro_offset_compensation_xaxis)((s16)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_gyro_offset_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s16 data = 0;
	s8 err = 0;

	err = BMI_CALL_API(get_gyro_offset_compensation_yaxis)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_gyro_offset_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	s8 err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_gyro_offset_compensation_yaxis)((s16)data);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_gyro_offset_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s16 data = 0;
	int err = 0;

	err = BMI_CALL_API(get_gyro_offset_compensation_zaxis)(&data);

	if (err < 0)
		return err;
	return sprintf(buf, "%d\n", data);
}

static ssize_t bmi160_gyro_offset_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err = BMI_CALL_API(set_gyro_offset_compensation_zaxis)((s16)data);

	if (err < 0)
		return -EIO;
	return count;
}


/* mag sensor part */
#ifdef BMI160_MAG_INTERFACE_SUPPORT
static ssize_t bmi160_mag_op_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	u8 mag_op_mode;
	s8 err;
	err = bmi160_get_mag_power_mode_stat(&mag_op_mode);
	if (err) {
		dev_err(client_data->dev,
			"Failed to get BMI160 mag power mode:%d\n", err);
		return err;
	} else
		return sprintf(buf, "%d, reg:%d\n",
					client_data->pw.mag_pm, mag_op_mode);
}

static ssize_t bmi160_mag_op_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	unsigned long op_mode;
	int err;

	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;

	if (op_mode == client_data->pw.mag_pm)
		return count;

	mutex_lock(&client_data->mutex_op_mode);


	if (op_mode < BMI_MAG_PM_MAX) {
		switch (op_mode) {
		case BMI_MAG_PM_NORMAL:
			/* need to modify as mag sensor connected,
			 * set write address to 0x4c and triggers
			 * write operation
			 * 0x4c(op mode control reg)
			 * enables normal mode in magnetometer */
#ifdef BMI160_AKM09912_SUPPORT
			err = bmi160_set_bst_akm_and_secondary_if_powermode
					(BMI160_MAG_FORCE_MODE);
#else
			err = bmi160_set_bmm150_mag_and_secondary_if_power_mode
					(BMI160_MAG_FORCE_MODE);
#endif
			client_data->pw.mag_pm = BMI_MAG_PM_NORMAL;
			mdelay(5);
			break;
		case BMI_MAG_PM_LP1:
			/* need to modify as mag sensor connected,
			 * set write address to 0x4 band triggers
			 * write operation
			 * 0x4b(bmm150, power control reg, bit0)
			 * enables power in magnetometer*/
#ifdef BMI160_AKM09912_SUPPORT
			err = bmi160_set_bst_akm_and_secondary_if_powermode
					(BMI160_MAG_FORCE_MODE);
#else
			err = bmi160_set_bmm150_mag_and_secondary_if_power_mode
					(BMI160_MAG_FORCE_MODE);
#endif
			client_data->pw.mag_pm = BMI_MAG_PM_LP1;
			mdelay(5);
			break;
		case BMI_MAG_PM_SUSPEND:
		case BMI_MAG_PM_LP2:
#ifdef BMI160_AKM09912_SUPPORT
		err = bmi160_set_bst_akm_and_secondary_if_powermode
					(BMI160_MAG_SUSPEND_MODE);
#else
		err = bmi160_set_bmm150_mag_and_secondary_if_power_mode
					(BMI160_MAG_SUSPEND_MODE);
#endif
			client_data->pw.mag_pm = op_mode;
			mdelay(5);
			break;
		default:
			mutex_unlock(&client_data->mutex_op_mode);
			return -EINVAL;
		}
	} else {
		mutex_unlock(&client_data->mutex_op_mode);
		return -EINVAL;
	}

	mutex_unlock(&client_data->mutex_op_mode);

	if (err) {
		dev_err(client_data->dev,
			"Failed to switch BMI160 mag power mode:%d\n",
			client_data->pw.mag_pm);
		return err;
	} else
		return count;

}

static ssize_t bmi160_mag_odr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	unsigned char mag_odr = 0;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = BMI_CALL_API(get_mag_output_data_rate)(&mag_odr);
	if (err)
		return err;

	client_data->odr.mag_odr = mag_odr;
	return sprintf(buf, "%d\n", mag_odr);
}

static ssize_t bmi160_mag_odr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long mag_odr;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &mag_odr);
	if (err)
		return err;
	/*1~25/32hz,..6(25hz),7(50hz),... */
	err = BMI_CALL_API(set_mag_output_data_rate)(mag_odr);
	if (err)
		return -EIO;

	client_data->odr.mag_odr = mag_odr;
	return count;
}

static ssize_t bmi160_mag_i2c_address_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 data;
	s8 err;

	err = BMI_CALL_API(set_mag_manual_enable)(1);
	err += BMI_CALL_API(get_i2c_device_addr)(&data);
	err += BMI_CALL_API(set_mag_manual_enable)(0);

	if (err < 0)
		return err;
	return sprintf(buf, "0x%x\n", data);
}

static ssize_t bmi160_mag_i2c_address_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err += BMI_CALL_API(set_mag_manual_enable)(1);
	if (!err)
		err += BMI_CALL_API(set_i2c_device_addr)((unsigned char)data);
	err += BMI_CALL_API(set_mag_manual_enable)(0);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_mag_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	struct bmi160_mag_xyz_s32_t data;

	struct bmi160_axis_data_t bmi160_udata;
	int err;
	/* raw data with compensation */
#ifdef BMI160_AKM09912_SUPPORT
	err = bmi160_bst_akm09912_compensate_xyz(&data);
#else
	err = bmi160_bmm150_mag_compensate_xyz(&data);
#endif

	if (err < 0) {
		memset(&data, 0, sizeof(data));
		dev_err(client_data->dev, "mag not ready!\n");
	}
	bmi160_udata.x = data.x;
	bmi160_udata.y = data.y;
	bmi160_udata.z = data.z;

	bmi_remap_sensor_data(&bmi160_udata, client_data);
	return sprintf(buf, "%hd %hd %hd\n", bmi160_udata.x,
					bmi160_udata.y, bmi160_udata.z);

}

static ssize_t bmi160_mag_offset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	unsigned char mag_offset;
	err = BMI_CALL_API(get_mag_offset)(&mag_offset);
	if (err)
		return err;

	return sprintf(buf, "%d\n", mag_offset);

}

static ssize_t bmi160_mag_offset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	err += BMI_CALL_API(set_mag_manual_enable)(1);
	if (err == 0)
		err += BMI_CALL_API(set_mag_offset)((unsigned char)data);
	err += BMI_CALL_API(set_mag_manual_enable)(0);

	if (err < 0)
		return -EIO;
	return count;
}

static ssize_t bmi160_mag_chip_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s8 err = 0;
	u8 mag_chipid;

	err = bmi160_set_mag_manual_enable(0x01);
	/* read mag chip_id value */
#ifdef BMI160_AKM09912_SUPPORT
	err += bmi160_set_mag_read_addr(AKM09912_CHIP_ID_REG);
		/* 0x04 is mag_x lsb register */
	err += bmi160_read_reg(BMI160_USER_DATA_0_MAG_X_LSB__REG,
							&mag_chipid, 1);

	/* Must add this commands to re-set data register addr of mag sensor */
	err += bmi160_set_mag_read_addr(AKM_DATA_REGISTER);
#else
	err += bmi160_set_mag_read_addr(BMI160_BMM150_CHIP_ID);
	/* 0x04 is mag_x lsb register */
	err += bmi160_read_reg(BMI160_USER_DATA_0_MAG_X_LSB__REG,
							&mag_chipid, 1);

	/* Must add this commands to re-set data register addr of mag sensor */
	/* 0x42 is  bmm150 data register address */
	err += bmi160_set_mag_read_addr(BMI160_BMM150_DATA_REG);
#endif

	err += bmi160_set_mag_manual_enable(0x00);

	if (err)
		return err;

	return sprintf(buf, "chip_id:0x%x\n", mag_chipid);

}

#endif

#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
static ssize_t bmi_enable_int_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int interrupt_type, value;

	sscanf(buf, "%3d %3d", &interrupt_type, &value);

	if (interrupt_type < 0 || interrupt_type > 16)
		return -EINVAL;

	if (interrupt_type <= BMI_FLAT_INT) {
		if (BMI_CALL_API(set_intr_enable_0)
				(bmi_interrupt_type[interrupt_type], value) < 0)
			return -EINVAL;
	} else if (interrupt_type <= BMI_FWM_INT) {
		if (BMI_CALL_API(set_intr_enable_1)
			(bmi_interrupt_type[interrupt_type], value) < 0)
			return -EINVAL;
	} else {
		if (BMI_CALL_API(set_intr_enable_2)
			(bmi_interrupt_type[interrupt_type], value) < 0)
			return -EINVAL;
	}

	return count;
}

#endif

static ssize_t bmi_register_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	bmi_dump_reg(client_data);

	return sprintf(buf, "Dump OK\n");
}

static ssize_t bmi_register_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	int reg_addr = 0;
	int data;
	u8 write_reg_add = 0;
	u8 write_data = 0;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	err = sscanf(buf, "%3d %3d", &reg_addr, &data);
	if (err < 2)
		return err;

	if (data > 0xff)
		return -EINVAL;

	write_reg_add = (u8)reg_addr;
	write_data = (u8)data;
	err += BMI_CALL_API(write_reg)(write_reg_add, &write_data, 1);

	if (!err) {
		dev_info(client_data->dev, "write reg 0x%2x, value= 0x%2x\n",
			reg_addr, data);
	} else {
		dev_err(client_data->dev, "write reg fail\n");
		return err;
	}
	return count;
}

static DEVICE_ATTR(chip_id, S_IRUGO,
		bmi160_chip_id_show, NULL);
static DEVICE_ATTR(err_st, S_IRUGO,
		bmi160_err_st_show, NULL);
static DEVICE_ATTR(sensor_time, S_IRUGO,
		bmi160_sensor_time_show, NULL);

static DEVICE_ATTR(selftest, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_selftest_show, bmi160_selftest_store);
static DEVICE_ATTR(fifo_flush, S_IWUSR|S_IWGRP|S_IWOTH,
		NULL, bmi160_fifo_flush_store);
static DEVICE_ATTR(fifo_bytecount, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_fifo_bytecount_show, bmi160_fifo_bytecount_store);
static DEVICE_ATTR(fifo_data_sel, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_fifo_data_sel_show, bmi160_fifo_data_sel_store);
static DEVICE_ATTR(fifo_data_frame, S_IRUGO,
		bmi160_fifo_data_out_frame_show, NULL);

static DEVICE_ATTR(fifo_watermark, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_fifo_watermark_show, bmi160_fifo_watermark_store);

static DEVICE_ATTR(fifo_header_en, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_fifo_header_en_show, bmi160_fifo_header_en_store);
static DEVICE_ATTR(fifo_time_en, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_fifo_time_en_show, bmi160_fifo_time_en_store);
static DEVICE_ATTR(fifo_int_tag_en, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_fifo_int_tag_en_show, bmi160_fifo_int_tag_en_store);

static DEVICE_ATTR(temperature, S_IRUGO,
		bmi160_temperature_show, NULL);
static DEVICE_ATTR(place, S_IRUGO,
		bmi160_place_show, NULL);
static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_delay_show, bmi160_delay_store);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_enable_show, bmi160_enable_store);
static DEVICE_ATTR(acc_range, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_range_show, bmi160_acc_range_store);
static DEVICE_ATTR(acc_odr, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_odr_show, bmi160_acc_odr_store);
static DEVICE_ATTR(acc_op_mode, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_op_mode_show, bmi160_acc_op_mode_store);
static DEVICE_ATTR(acc_value, S_IRUGO,
		bmi160_acc_value_show, NULL);
static DEVICE_ATTR(acc_fast_calibration_x, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_fast_calibration_x_show,
		bmi160_acc_fast_calibration_x_store);
static DEVICE_ATTR(acc_fast_calibration_y, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_fast_calibration_y_show,
		bmi160_acc_fast_calibration_y_store);
static DEVICE_ATTR(acc_fast_calibration_z, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_fast_calibration_z_show,
		bmi160_acc_fast_calibration_z_store);
static DEVICE_ATTR(acc_offset_x, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_offset_x_show,
		bmi160_acc_offset_x_store);
static DEVICE_ATTR(acc_offset_y, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_offset_y_show,
		bmi160_acc_offset_y_store);
static DEVICE_ATTR(acc_offset_z, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_acc_offset_z_show,
		bmi160_acc_offset_z_store);
static DEVICE_ATTR(test, S_IRUGO,
		bmi160_test_show, NULL);
static DEVICE_ATTR(stc_enable, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_step_counter_enable_show,
		bmi160_step_counter_enable_store);
static DEVICE_ATTR(stc_mode, S_IWUSR|S_IWGRP|S_IWOTH,
		NULL, bmi160_step_counter_mode_store);
static DEVICE_ATTR(stc_clc, S_IWUSR|S_IWGRP|S_IWOTH,
		NULL, bmi160_step_counter_clc_store);
static DEVICE_ATTR(stc_value, S_IRUGO,
		bmi160_step_counter_value_show, NULL);


/* gyro part */
static DEVICE_ATTR(gyro_op_mode, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_gyro_op_mode_show, bmi160_gyro_op_mode_store);
static DEVICE_ATTR(gyro_value, S_IRUGO,
		bmi160_gyro_value_show, NULL);
static DEVICE_ATTR(gyro_range, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_gyro_range_show, bmi160_gyro_range_store);
static DEVICE_ATTR(gyro_odr, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_gyro_odr_show, bmi160_gyro_odr_store);
static DEVICE_ATTR(gyro_fast_calibration_en, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
bmi160_gyro_fast_calibration_en_show, bmi160_gyro_fast_calibration_en_store);
static DEVICE_ATTR(gyro_offset_x, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
bmi160_gyro_offset_x_show, bmi160_gyro_offset_x_store);
static DEVICE_ATTR(gyro_offset_y, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
bmi160_gyro_offset_y_show, bmi160_gyro_offset_y_store);
static DEVICE_ATTR(gyro_offset_z, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
bmi160_gyro_offset_z_show, bmi160_gyro_offset_z_store);

#ifdef BMI160_MAG_INTERFACE_SUPPORT
static DEVICE_ATTR(mag_op_mode, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_mag_op_mode_show, bmi160_mag_op_mode_store);
static DEVICE_ATTR(mag_odr, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_mag_odr_show, bmi160_mag_odr_store);
static DEVICE_ATTR(mag_i2c_addr, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_mag_i2c_address_show, bmi160_mag_i2c_address_store);
static DEVICE_ATTR(mag_value, S_IRUGO,
		bmi160_mag_value_show, NULL);
static DEVICE_ATTR(mag_offset, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_mag_offset_show, bmi160_mag_offset_store);

static DEVICE_ATTR(mag_chip_id, S_IRUGO,
		bmi160_mag_chip_id_show, NULL);

#endif


#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
static DEVICE_ATTR(enable_int, S_IWUSR|S_IWGRP|S_IWOTH,
		NULL, bmi_enable_int_store);
static DEVICE_ATTR(anymot_duration, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_anymot_duration_show, bmi160_anymot_duration_store);
static DEVICE_ATTR(anymot_threshold, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_anymot_threshold_show, bmi160_anymot_threshold_store);
static DEVICE_ATTR(std_stu, S_IRUGO,
		bmi160_step_detector_status_show, NULL);
static DEVICE_ATTR(std_en, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_step_detector_enable_show,
		bmi160_step_detector_enable_store);
static DEVICE_ATTR(sig_en, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi160_signification_motion_enable_show,
		bmi160_signification_motion_enable_store);

#endif
static DEVICE_ATTR(enable_timer, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi_show_enable_timer, bmi_store_enable_timer);

static DEVICE_ATTR(register, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		bmi_register_show, bmi_register_store);

static DEVICE_ATTR(bmi_value, S_IRUGO,
		bmi160_bmi_value_show, NULL);

static DEVICE_ATTR(bmi_ring_buf, S_IRUGO,
		bmi160_ring_buf_show, NULL);


static struct attribute *bmi160_attributes[] = {
	&dev_attr_chip_id.attr,
	&dev_attr_err_st.attr,
	&dev_attr_sensor_time.attr,
	&dev_attr_selftest.attr,

	&dev_attr_test.attr,
	&dev_attr_fifo_flush.attr,
	&dev_attr_fifo_header_en.attr,
	&dev_attr_fifo_time_en.attr,
	&dev_attr_fifo_int_tag_en.attr,
	&dev_attr_fifo_bytecount.attr,
	&dev_attr_fifo_data_sel.attr,
	&dev_attr_fifo_data_frame.attr,

	&dev_attr_fifo_watermark.attr,

	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_temperature.attr,
	&dev_attr_place.attr,

	&dev_attr_acc_range.attr,
	&dev_attr_acc_odr.attr,
	&dev_attr_acc_op_mode.attr,
	&dev_attr_acc_value.attr,

	&dev_attr_acc_fast_calibration_x.attr,
	&dev_attr_acc_fast_calibration_y.attr,
	&dev_attr_acc_fast_calibration_z.attr,
	&dev_attr_acc_offset_x.attr,
	&dev_attr_acc_offset_y.attr,
	&dev_attr_acc_offset_z.attr,

	&dev_attr_stc_enable.attr,
	&dev_attr_stc_mode.attr,
	&dev_attr_stc_clc.attr,
	&dev_attr_stc_value.attr,

	&dev_attr_gyro_op_mode.attr,
	&dev_attr_gyro_value.attr,
	&dev_attr_gyro_range.attr,
	&dev_attr_gyro_odr.attr,
	&dev_attr_gyro_fast_calibration_en.attr,
	&dev_attr_gyro_offset_x.attr,
	&dev_attr_gyro_offset_y.attr,
	&dev_attr_gyro_offset_z.attr,

#ifdef BMI160_MAG_INTERFACE_SUPPORT
	&dev_attr_mag_chip_id.attr,
	&dev_attr_mag_op_mode.attr,
	&dev_attr_mag_odr.attr,
	&dev_attr_mag_i2c_addr.attr,
	&dev_attr_mag_value.attr,
	&dev_attr_mag_offset.attr,

#endif

#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
	&dev_attr_enable_int.attr,

	&dev_attr_anymot_duration.attr,
	&dev_attr_anymot_threshold.attr,
	&dev_attr_std_stu.attr,
	&dev_attr_std_en.attr,
	&dev_attr_sig_en.attr,

#endif
	&dev_attr_enable_timer.attr,
	&dev_attr_register.attr,
	&dev_attr_bmi_value.attr,
	&dev_attr_bmi_ring_buf.attr,
	NULL
};

static struct attribute_group bmi160_attribute_group = {
	.attrs = bmi160_attributes
};

static void bmi_delay(u32 msec)
{
	mdelay(msec);
}

#if defined(BMI160_ENABLE_INT1) || defined(BMI160_ENABLE_INT2)
static void bmi_slope_interrupt_handle(struct bmi_client_data *client_data)
{
	/* anym_first[0..2]: x, y, z */
	u8 anym_first[3] = {0};
	u8 status2;
	u8 anym_sign;
	u8 i = 0;

	client_data->device.bus_read(client_data->device.dev_addr,
				BMI160_USER_INTR_STAT_2_ADDR, &status2, 1);
	anym_first[0] = BMI160_GET_BITSLICE(status2,
				BMI160_USER_INTR_STAT_2_ANY_MOTION_FIRST_X);
	anym_first[1] = BMI160_GET_BITSLICE(status2,
				BMI160_USER_INTR_STAT_2_ANY_MOTION_FIRST_Y);
	anym_first[2] = BMI160_GET_BITSLICE(status2,
				BMI160_USER_INTR_STAT_2_ANY_MOTION_FIRST_Z);
	anym_sign = BMI160_GET_BITSLICE(status2,
				BMI160_USER_INTR_STAT_2_ANY_MOTION_SIGN);

	for (i = 0; i < 3; i++) {
		if (anym_first[i]) {
			/*1: negative*/
			if (anym_sign)
				dev_notice(client_data->dev,
				"Anymotion interrupt happend!"
				"%s axis, negative sign\n", bmi_axis_name[i]);
			else
				dev_notice(client_data->dev,
				"Anymotion interrupt happend!"
				"%s axis, postive sign\n", bmi_axis_name[i]);
		}
	}


}

static uint64_t bmi_get_alarm_timestamp_ns(void)
{
	uint64_t ts_ap;
	struct timespec tmp_time;
	get_monotonic_boottime(&tmp_time);
	ts_ap = (uint64_t)tmp_time.tv_sec * 1000000000L + tmp_time.tv_nsec;
	return ts_ap;
}


static int bmi_ring_buf_empty(void)
{
	return s_ring_buf_head == s_ring_buf_tail;
}

static int bmi_ring_buf_full(void)
{
	return (s_ring_buf_tail + 1) % BMI_RING_BUF_SIZE == s_ring_buf_head;
}

static int bmi_ring_buf_put(struct bmi160_value_t data)
{
	if (bmi_ring_buf_full())
		return 0;

	bmi_ring_buf[s_ring_buf_tail].acc.x = data.acc.x; 
	bmi_ring_buf[s_ring_buf_tail].acc.y = data.acc.y;
	bmi_ring_buf[s_ring_buf_tail].acc.z = data.acc.z;
	bmi_ring_buf[s_ring_buf_tail].gyro.x = data.gyro.x; 
	bmi_ring_buf[s_ring_buf_tail].gyro.y = data.gyro.y;
	bmi_ring_buf[s_ring_buf_tail].gyro.z = data.gyro.z;
#ifdef BMI160_MAG_INTERFACE_SUPPORT
	bmi_ring_buf[s_ring_buf_tail].mag.x = data.mag.x; 
	bmi_ring_buf[s_ring_buf_tail].mag.y = data.mag.y;
	bmi_ring_buf[s_ring_buf_tail].mag.z = data.mag.z;
#endif
	bmi_ring_buf[s_ring_buf_tail].ts_intvl= data.ts_intvl;
	s_ring_buf_tail = (s_ring_buf_tail + 1) % BMI_RING_BUF_SIZE;

	return 1;
}

static int bmi_ring_buf_get(struct bmi160_value_t *pData)
{
	if (bmi_ring_buf_empty())
		return 0;

	pData->acc.x = bmi_ring_buf[s_ring_buf_head].acc.x;
	pData->acc.y = bmi_ring_buf[s_ring_buf_head].acc.y;
	pData->acc.z = bmi_ring_buf[s_ring_buf_head].acc.z;
	pData->gyro.x = bmi_ring_buf[s_ring_buf_head].gyro.x;
	pData->gyro.y = bmi_ring_buf[s_ring_buf_head].gyro.y;
	pData->gyro.z = bmi_ring_buf[s_ring_buf_head].gyro.z;
#ifdef BMI160_MAG_INTERFACE_SUPPORT
	pData->mag.x = bmi_ring_buf[s_ring_buf_head].mag.x;
	pData->mag.y = bmi_ring_buf[s_ring_buf_head].mag.y;
	pData->mag.z = bmi_ring_buf[s_ring_buf_head].mag.z;
#endif
	pData->ts_intvl = bmi_ring_buf[s_ring_buf_head].ts_intvl;

	s_ring_buf_head = (s_ring_buf_head + 1) % BMI_RING_BUF_SIZE;

	return 1;
}


static int bmi_fifo_get_decode_data(struct bmi160_axis_data_t *acc,
				    struct bmi160_axis_data_t *gyro,
				    struct bmi160_axis_data_t *mag,
				    u8 *fifo_data,
				    u16 fifo_index)
{
	/* array index to get elements from fifo data*/
	u16 array_index = 0;

	/* get xyzr axis mag data */
	if (mag != NULL)
	{
		struct bmi160_mag_xyzr_t mag_valid;
		struct bmi160_mag_xyzr_t mag_data;
		struct bmi160_mag_xyz_s32_t mag_comp_xyz;
		mag_data.x = fifo_data[fifo_index + 0]
					| fifo_data[fifo_index + 1] << 8;
		mag_data.y = fifo_data[fifo_index + 2]
					| fifo_data[fifo_index + 3] << 8;
		mag_data.z = fifo_data[fifo_index + 4]
					| fifo_data[fifo_index + 5] << 8;
		mag_data.r = fifo_data[fifo_index + 6]
					| fifo_data[fifo_index + 7] << 8;
		fifo_index += 8;
		mag_valid.x = mag_data.x >> 3;
		mag_valid.y = mag_data.y >> 3;
		mag_valid.z = mag_data.z >> 1;
		mag_valid.r = mag_data.r >> 2;
		bmi160_bmm150_mag_compensate_xyz_raw(&mag_comp_xyz, mag_valid);
		mag->x = mag_comp_xyz.x;
		mag->y = mag_comp_xyz.y;
		mag->z = mag_comp_xyz.z;
	}

	/* get xyz axis gyro data */
	if (gyro != NULL)
	{
		gyro->x = fifo_data[fifo_index + 0]
				    	 | fifo_data[fifo_index + 1] << 8;
		gyro->y = fifo_data[fifo_index + 2]
					 | fifo_data[fifo_index + 3] << 8;
		gyro->z = fifo_data[fifo_index + 4]
					 | fifo_data[fifo_index + 5] << 8;
		fifo_index += 6;
	}

	/* get xyz axis accel data */
	if (acc != NULL)
	{
		acc->x = fifo_data[fifo_index + 0]
					| fifo_data[fifo_index + 1] << 8;
		acc->y = fifo_data[fifo_index + 2]
					| fifo_data[fifo_index + 3] << 8;
		acc->z = fifo_data[fifo_index + 4]
					| fifo_data[fifo_index + 5] << 8;
		fifo_index += 6;
	}

	return array_index;

}

static int bmi_pow(int x, int y)
{
	int result = 1;

	if (y == 1)
		return x;


	if (y > 0) {
		while (--y) {
			result *= x;
		}
	} else {
		result = 0;
	}

	return result;
}

/*
* Decoding of FIFO frames (only exemplary subset). Calculates accurate timestamps based on the
* drift information. Returns value of the SENSORTIME frame.
*
* fifo_data_buf - pointer to fetched FIFO buffer
* fifo_length - number of valid bytes in the FIFO buffer
* drift - clock drift value, needed to calculate accurate timestamps
* timestamp - initial timestamp for the first sample of the FIFO chunk
* acc_odr - current value of the ACC_CONF.acc_odr register field
*/
static uint64_t bmi_fifo_data_decode(uint8_t *fifo_data_buf, uint16_t fifo_length,
					int drift, uint64_t timestamp, int odr_pow)
{
	int idx = 0;
	uint8_t header;
	uint64_t fifo_time = 0;
	uint64_t timestamp_next = timestamp;
	struct bmi160_value_t bmi160_value;

	while (idx < fifo_length) {
		header = fifo_data_buf[idx];
		memset(&bmi160_value, 0, sizeof (bmi160_value));
		switch (header) {
			case FIFO_HEAD_SENSOR_TIME:
				/* sensortime frame, length = 4 bytes*/
				if (idx + 3 < fifo_length) {
					fifo_time = fifo_data_buf[idx + 1] |
							fifo_data_buf[idx + 2] << 8 |
							fifo_data_buf[idx + 3] << 16;
					return fifo_time;
				}
				idx += 4;
				break;
			case FIFO_HEAD_A:
				/* accel frame, length = 7 bytes */
				idx += 1;
				if (idx + 6 < fifo_length) {
					bmi_fifo_get_decode_data(
						&bmi160_value.acc,
						 NULL, NULL,
						 fifo_data_buf, idx);
					bmi160_value.ts_intvl =
					 (int64_t)(625 * odr_pow * (drift > 0 ? drift : 1000));
					if (bmi_ring_buf_put(bmi160_value) == 0)
						pr_info("bmi ring buf full head %d, tail %d",
							s_ring_buf_head, s_ring_buf_tail);
					if (drift > 0)
						timestamp_next += (uint64_t)(625 * odr_pow * drift);
					else
						timestamp_next += (uint64_t)(625 * odr_pow * 1000);
				}
				idx += 6;
				break;
			case FIFO_HEAD_G:
				/* gyro frame, length = 7 bytes */
				idx += 1;
				if (idx + 6 < fifo_length) {
					bmi_fifo_get_decode_data(NULL,
						 &bmi160_value.gyro,
						 NULL, fifo_data_buf, idx);
					bmi160_value.ts_intvl =
					 (int64_t)(625 * odr_pow * (drift > 0 ? drift : 1000));
					if (bmi_ring_buf_put(bmi160_value) == 0)
						pr_info("bmi ring buf full head %d, tail %d",
							s_ring_buf_head, s_ring_buf_tail);
				}

				idx += 6;
				break;
			case FIFO_HEAD_G_A:
				idx += 1;
				if (idx + 12 < fifo_length) {
					bmi_fifo_get_decode_data(&bmi160_value.acc,
						 &bmi160_value.gyro,
						 NULL, fifo_data_buf, idx);
					bmi160_value.ts_intvl =
					 (int64_t)(625 * odr_pow * (drift > 0 ? drift : 1000));
					if (bmi_ring_buf_put(bmi160_value) == 0)
						pr_info("bmi ring buf full head %d, tail %d",
							s_ring_buf_head, s_ring_buf_tail);
				}

				idx += 12;
				break;
#ifdef BMI160_MAG_INTERFACE_SUPPORT
			case FIFO_HEAD_M:
				idx +=1;
				if (idx + 8 < fifo_length) {
					bmi_fifo_get_decode_data(NULL, NULL,
						 &bmi160_value.mag,
						 fifo_data_buf, idx);
					bmi160_value.ts_intvl =
					 (int64_t)(625 * odr_pow * (drift > 0 ? drift : 1000));
					if (bmi_ring_buf_put(bmi160_value) == 0)
						pr_info("bmi ring buf full head %d, tail %d",
							s_ring_buf_head, s_ring_buf_tail);
				}

				idx += 8;
				break;
			case FIFO_HEAD_M_A:
				idx +=1;
				if (idx + 14 < fifo_length) {
					bmi_fifo_get_decode_data(&bmi160_value.acc, NULL,
						 &bmi160_value.mag, fifo_data_buf, idx);
					bmi160_value.ts_intvl =
					 (int64_t)(625 * odr_pow * (drift > 0 ? drift : 1000));
					if (bmi_ring_buf_put(bmi160_value) == 0)
						pr_info("bmi ring buf full head %d, tail %d",
							s_ring_buf_head, s_ring_buf_tail);
				}

				idx += 14;
				break;

			case FIFO_HEAD_M_G:
				idx +=1;
				if (idx + 14 < fifo_length) {
					bmi_fifo_get_decode_data(NULL, &bmi160_value.gyro,
						 &bmi160_value.mag, fifo_data_buf, idx);
					bmi160_value.ts_intvl =
					 (int64_t)(625 * odr_pow * (drift > 0 ? drift : 1000));
					if (bmi_ring_buf_put(bmi160_value) == 0)
						pr_info("bmi ring buf full head %d, tail %d",
							s_ring_buf_head, s_ring_buf_tail);
				}

				idx += 14;
				break;
			case FIFO_HEAD_M_G_A:
				idx +=1;
				if (idx + 20 < fifo_length) {
					bmi_fifo_get_decode_data(&bmi160_value.acc,
							 &bmi160_value.gyro,
							 &bmi160_value.mag, fifo_data_buf, idx);
					bmi160_value.ts_intvl =
					 (int64_t)(625 * odr_pow * (drift > 0 ? drift : 1000));
					if (bmi_ring_buf_put(bmi160_value) == 0)
						pr_info("bmi ring buf full head %d, tail %d",
							s_ring_buf_head, s_ring_buf_tail);
				}

				idx += 20;
				pr_info("second interface mag");
				break;
#endif
			case FIFO_HEAD_OVER_READ_LSB:
				/* end of fifo chunk */
				idx = fifo_length;
				break;
			default:
				pr_info("ERROR parsing FIFO!! header 0x%x", header);
				idx = fifo_length;
				break;
		}
	}

	return -1;
}

static uint64_t bmi_fifo_next_timestamp_calc(uint64_t host_time_new,
						uint64_t sensor_time_new,
						int drift, int odr_pow,
						uint8_t bmi_odr)
{
	uint64_t time_age = ((sensor_time_new & (0xFFFF >> bmi_odr))
				 * (drift > 0 ? drift : 1000)) * 390625;
	return ((host_time_new - div64_u64(time_age, 10000000)
		 + ((drift > 0 ? drift : 1000) * (625 * odr_pow))));
}

static int bmi_fifo_time_drift_calc(uint64_t host_time_new,
				 uint64_t sensor_time_new,
				uint64_t host_time_old,
				 uint64_t sensor_time_old)
{
	uint64_t delta_st, delta_ht;
	int drift = 0;

	if (host_time_old > 0) {
		delta_st = sensor_time_new >= sensor_time_old ?
			(sensor_time_new - sensor_time_old) :
			(sensor_time_new + FIFO_SENSORTIME_OVERFLOW_MASK - sensor_time_old);
		delta_ht = host_time_new - host_time_old;
		if (delta_st != 0)
			drift = (int)div64_u64(delta_ht * 10000,
			 (delta_st * FIFO_SENSORTIME_RESOLUTION));
		else
			drift = 0;

	} else {
		drift = 0;
	}

	return drift;
}

static void bmi_fifo_watermark_interrupt_handle
				(struct bmi_client_data *client_data)
{
	int err = 0;
	unsigned int fifo_len0 = 0;
	unsigned int  fifo_frmbytes_ext = 0;
	static int time_drift = 0;
	static uint64_t timestamp_next = 0;
	uint8_t bmi_odr = client_data->odr.acc_odr;
	int odr_pow = bmi_pow(2, 12 - bmi_odr + 1);
	/*TO DO*/

	if (client_data->fifo_data_sel == 4)
	{
		bmi_odr = client_data->odr.mag_odr;
		odr_pow = bmi_pow(2, 12 - bmi_odr + 1);
	}

	bmi_fifo_frame_bytes_extend_calc(client_data, &fifo_frmbytes_ext);

	if (client_data->pw.acc_pm == 2 && client_data->pw.gyro_pm == 2
					&& client_data->pw.mag_pm == 2)
		pr_info("pw_acc: %d, pw_gyro: %d\n",
			client_data->pw.acc_pm, client_data->pw.gyro_pm);
	if (!client_data->fifo_data_sel)
		pr_info("no selsect sensor fifo, fifo_data_sel:%d\n",
						client_data->fifo_data_sel);

	err = BMI_CALL_API(fifo_length)(&fifo_len0);
	client_data->fifo_bytecount = fifo_len0;

	if (client_data->fifo_bytecount == 0 || err)
	{
		pr_info("fifo_bytecount is 0!!");
		return ;
	}
	if (client_data->fifo_bytecount + fifo_frmbytes_ext > FIFO_DATA_BUFSIZE)
		client_data->fifo_bytecount = FIFO_DATA_BUFSIZE;
	/* need give attention for the time of burst read*/
	memset (s_fifo_data_buf, 0, sizeof (s_fifo_data_buf));
	if (!err) {
		err = bmi_burst_read_wrapper(client_data->device.dev_addr,
			BMI160_USER_FIFO_DATA__REG, s_fifo_data_buf,
			client_data->fifo_bytecount + fifo_frmbytes_ext);
		/* store host timestamp after reading fifo */
		host_time_new = bmi_get_alarm_timestamp_ns();
		if (timestamp_next == 0) {
			sensor_time_new = bmi_fifo_data_decode(s_fifo_data_buf,
				client_data->fifo_bytecount + fifo_frmbytes_ext,
				time_drift, timestamp_next, odr_pow);
		} else {
			sensor_time_new = bmi_fifo_data_decode(s_fifo_data_buf,
				client_data->fifo_bytecount + fifo_frmbytes_ext,
				time_drift, host_time_new, odr_pow);
		}
		if (sensor_time_new >= 0) {
			time_drift = bmi_fifo_time_drift_calc(host_time_new,
				sensor_time_new, host_time_old, sensor_time_old);
			timestamp_next = bmi_fifo_next_timestamp_calc(host_time_new,
				sensor_time_new, time_drift, odr_pow, bmi_odr);
			/* store current timestamps as the old ones
			for next delta calculation */
			host_time_old = host_time_new;
			sensor_time_old = sensor_time_new;
		}
	} else {
		dev_err(client_data->dev, "read fifo leght err");
	}

	if (err)
		dev_err(client_data->dev, "brust read fifo err\n");

}

static void bmi_signification_motion_interrupt_handle(
		struct bmi_client_data *client_data)
{
	pr_info("bmi_signification_motion_interrupt_handle\n");
	input_event(client_data->input_accel, EV_MSC, INPUT_EVENT_SGM, 1);
	input_sync(client_data->input_accel);
	input_event(client_data->input_gyro, EV_MSC, INPUT_EVENT_SGM, 1);
	input_sync(client_data->input_gyro);
	bmi160_set_command_register(CMD_RESET_INT_ENGINE);

}
static void bmi_stepdetector_interrupt_handle(
	struct bmi_client_data *client_data)
{
	u8 current_step_dector_st = 0;
	client_data->pedo_data.wkar_step_detector_status++;
	current_step_dector_st =
		client_data->pedo_data.wkar_step_detector_status;
	client_data->std = ((current_step_dector_st == 1) ? 0 : 1);
}

static void bmi_irq_work_func(struct work_struct *work)
{
	struct bmi_client_data *client_data =
		container_of((struct work_struct *)work,
			struct bmi_client_data, irq_work);

	unsigned char int_status[4] = {0, 0, 0, 0};

	client_data->device.bus_read(client_data->device.dev_addr,
				BMI160_USER_INTR_STAT_0_ADDR, int_status, 4);

	if (BMI160_GET_BITSLICE(int_status[0],
					BMI160_USER_INTR_STAT_0_ANY_MOTION))
		bmi_slope_interrupt_handle(client_data);

	if (BMI160_GET_BITSLICE(int_status[0],
			BMI160_USER_INTR_STAT_0_STEP_INTR))
		bmi_stepdetector_interrupt_handle(client_data);
	if (BMI160_GET_BITSLICE(int_status[1],
			BMI160_USER_INTR_STAT_1_FIFO_WM_INTR))
		bmi_fifo_watermark_interrupt_handle(client_data);

	/* Clear ALL inputerrupt status after handler sig mition*/
	/* Put this commads intot the last one*/
	if (BMI160_GET_BITSLICE(int_status[0],
		BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR))
		bmi_signification_motion_interrupt_handle(client_data);

}

static irqreturn_t bmi_irq_handler(int irq, void *handle)
{
	struct bmi_client_data *client_data = handle;

	if (client_data == NULL)
		return IRQ_HANDLED;
	if (client_data->dev == NULL)
		return IRQ_HANDLED;
	schedule_work(&client_data->irq_work);

	return IRQ_HANDLED;
}
#endif /* defined(BMI_ENABLE_INT1)||defined(BMI_ENABLE_INT2) */

static int bmi_restore_hw_cfg(struct bmi_client_data *client)
{
	int err = 0;

	if ((client->fifo_data_sel) & (1 << BMI_ACC_SENSOR)) {
		err += BMI_CALL_API(set_accel_range)(client->range.acc_range);
		err += BMI_CALL_API(set_accel_output_data_rate)
				(client->odr.acc_odr);
		err += BMI_CALL_API(set_fifo_accel_enable)(1);
	}
	if ((client->fifo_data_sel) & (1 << BMI_GYRO_SENSOR)) {
		err += BMI_CALL_API(set_gyro_range)(client->range.gyro_range);
		err += BMI_CALL_API(set_gyro_output_data_rate)
				(client->odr.gyro_odr);
		err += BMI_CALL_API(set_fifo_gyro_enable)(1);
	}
	if ((client->fifo_data_sel) & (1 << BMI_MAG_SENSOR)) {
		err += BMI_CALL_API(set_mag_output_data_rate)
				(client->odr.mag_odr);
		err += BMI_CALL_API(set_fifo_mag_enable)(1);
	}
	err += BMI_CALL_API(set_command_register)(CMD_CLR_FIFO_DATA);

	mutex_lock(&client->mutex_op_mode);
	if (client->pw.acc_pm != BMI_ACC_PM_SUSPEND) {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_NORMAL]);
		mdelay(3);
	}
	mutex_unlock(&client->mutex_op_mode);

	mutex_lock(&client->mutex_op_mode);
	if (client->pw.gyro_pm != BMI_GYRO_PM_SUSPEND) {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_NORMAL]);
		mdelay(3);
	}
	mutex_unlock(&client->mutex_op_mode);

	mutex_lock(&client->mutex_op_mode);

	if (client->pw.mag_pm != BMI_MAG_PM_SUSPEND) {
#ifdef BMI160_AKM09912_SUPPORT
		err += bmi160_set_bst_akm_and_secondary_if_powermode
					(BMI160_MAG_FORCE_MODE);
#else
		err += bmi160_set_bmm150_mag_and_secondary_if_power_mode
					(BMI160_MAG_FORCE_MODE);
#endif
		mdelay(3);
	}
	mutex_unlock(&client->mutex_op_mode);

	return err;
}

static void bmi160_set_acc_enable(struct device *dev, unsigned int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bmi_client_data *client_data = i2c_get_clientdata(client);
	int acc_pre_enable = atomic_read(&client_data->wkqueue_en);
	int gyro_current_enable;

	mutex_lock(&client_data->mutex_enable);
	if (enable) {
		if (acc_pre_enable == 0) {
			if (!client_data->power_enabled) {
				if (bmi160_power_ctl(client_data, true)) {
					dev_err(dev, "power up sensor failed.\n");
					goto mutex_exit;
				}
			}

			bmi160_set_acc_op_mode(client_data, BMI_ACC_PM_NORMAL);
			schedule_delayed_work(&client_data->work,
			msecs_to_jiffies(atomic_read(&client_data->delay)));
			atomic_set(&client_data->wkqueue_en, 1);
		}
	} else {
		if ((acc_pre_enable == 1) && client_data->power_enabled) {
			bmi160_set_acc_op_mode(client_data, BMI_ACC_PM_SUSPEND);
			cancel_delayed_work_sync(&client_data->work);
			atomic_set(&client_data->wkqueue_en, 0);
			gyro_current_enable = atomic_read(&client_data->gyro_en);
			if (!gyro_current_enable) {
				if (bmi160_power_ctl(client_data, false)) {
					dev_err(dev, "power up sensor failed.\n");
					goto mutex_exit;
				}
			}
		}
	}

mutex_exit:
	mutex_unlock(&client_data->mutex_enable);
	dev_notice(dev,
		"acc_enable en_state=%d\n",
		 atomic_read(&client_data->wkqueue_en));
}


static void bmi160_set_gyro_enable(struct device *dev, unsigned int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bmi_client_data *client_data = i2c_get_clientdata(client);
	int gyro_pre_enable = atomic_read(&client_data->gyro_en);
	int acc_current_enable;

	mutex_lock(&client_data->mutex_enable);
	if (enable) {
		if (gyro_pre_enable == 0) {
			if (!client_data->power_enabled) {
				if (bmi160_power_ctl(client_data, true)) {
					dev_err(dev, "power up sensor failed.\n");
					goto mutex_exit;
				}
			}
			bmi160_set_gyro_op_mode(client_data, BMI_GYRO_PM_NORMAL);
			schedule_delayed_work(&client_data->gyro_work,
			 msecs_to_jiffies(atomic_read(&client_data->delay)));
			atomic_set(&client_data->gyro_en, 1);
		}
	} else {
		if ((gyro_pre_enable == 1) && client_data->power_enabled) {
			bmi160_set_gyro_op_mode(client_data, BMI_GYRO_PM_SUSPEND);
			cancel_delayed_work_sync(&client_data->gyro_work);
			atomic_set(&client_data->gyro_en, 0);
			acc_current_enable = atomic_read(&client_data->wkqueue_en);
			if (!acc_current_enable) {
				if (bmi160_power_ctl(client_data, false)) {
					dev_err(dev, "power up sensor failed.\n");
					goto mutex_exit;
				}
			}
		}
	}

mutex_exit:
	mutex_unlock(&client_data->mutex_enable);
	dev_notice(&client->dev, "gyro_enable en_state=%d\n",
			 atomic_read(&client_data->gyro_en));
}


static int bmi160_self_calibration_xyz(struct sensors_classdev *sensors_cdev,
			int axis, int apply_now)
{
	int err = 0;
	s8 accel_offset_x = 0;
	s8 accel_offset_y = 0;
	s8 accel_offset_z = 0;

	struct bmi_client_data *client_data = container_of(sensors_cdev,
				struct bmi_client_data, accel_cdev);

	err = BMI_CALL_API(set_accel_foc_trigger)(X_AXIS,
				 1, &accel_offset_x);
	if (!err)
		client_data->calib_status |=
			BMI_FAST_CALI_TRUE << BMI_ACC_X_FAST_CALI_RDY;
	else
		return -EIO;

	err = BMI_CALL_API(set_accel_foc_trigger)(Y_AXIS,
				 1, &accel_offset_y);
	if (!err)
		client_data->calib_status |=
			BMI_FAST_CALI_TRUE << BMI_ACC_Y_FAST_CALI_RDY;
	else
		return -EIO;

	err = BMI_CALL_API(set_accel_foc_trigger)(Z_AXIS,
					 1, &accel_offset_z);
	if (!err)
		client_data->calib_status |=
			BMI_FAST_CALI_TRUE << BMI_ACC_Z_FAST_CALI_RDY;
	else
		return -EIO;

	return 0;
}

static int bmi160_accel_poll_delay(struct sensors_classdev *sensors_cdev,
				unsigned int delay_ms)
{
	struct bmi_client_data *data = container_of(sensors_cdev,
					struct bmi_client_data, accel_cdev);

	if (delay_ms < 10)
		delay_ms = 10;
	if (delay_ms > 10)
		delay_ms = 10;
	atomic_set(&data->delay, (unsigned int) delay_ms);
	return 0;
}

static int bmi160_cdev_enable_accel(struct sensors_classdev *sensors_cdev,
				 unsigned int enable)
{
	struct bmi_client_data *client_data = container_of(sensors_cdev,
					struct bmi_client_data, accel_cdev);
	bmi160_set_acc_enable(&client_data->i2c->dev, enable);
	return 0;
}

static int bmi160_gyro_poll_delay(struct sensors_classdev *sensors_cdev,
				unsigned int delay_ms)
{
	struct bmi_client_data *data = container_of(sensors_cdev,
					struct bmi_client_data, gyro_cdev);

	if (delay_ms < 10)
		delay_ms = 10;
	if (delay_ms > 10)
		delay_ms = 10;
	atomic_set(&data->delay, (unsigned int) delay_ms);
	return 0;
}


static int bmi160_cdev_enable_gyro(struct sensors_classdev *sensors_cdev,
				 unsigned int enable)
{
	struct bmi_client_data *client_data = container_of(sensors_cdev,
					struct bmi_client_data, gyro_cdev);
	bmi160_set_gyro_enable(&client_data->i2c->dev, enable);
	return 0;
}


int bmi_probe(struct bmi_client_data *client_data, struct device *dev)
{
	int err = 0;
#ifdef BMI160_MAG_INTERFACE_SUPPORT
	u8 mag_dev_addr;
	u8 mag_urst_len;
	u8 mag_op_mode;
#endif
	err = bmi160_power_init(client_data);
	if (err) {
		dev_err(&client_data->i2c->dev,
			 "Failed to get sensor regulators\n");
		err = -EINVAL;
		goto exit_err_clean;
	}
	err = bmi160_power_ctl(client_data, true);
	if (err) {
		dev_err(&client_data->i2c->dev,
			 "Failed to enable sensor power\n");
		err = -EINVAL;
		goto deinit_power_exit;
	}

	/* check chip id */
	err = bmi_check_chip_id(client_data);
	if (err)
		goto disable_power_exit;

	dev_set_drvdata(dev, client_data);
	client_data->dev = dev;

	mutex_init(&client_data->mutex_enable);
	mutex_init(&client_data->mutex_op_mode);
	mutex_init(&client_data->mutex_ring_buf);

	/* input device init */
	err = bmi_input_init(client_data);
	if (err < 0)
		goto disable_power_exit;

	/* sysfs node creation */
	err = sysfs_create_group(&client_data->i2c->dev.kobj,
			&bmi160_attribute_group);

	if (err < 0)
		goto exit_err_sysfs;

	/*to do*/
	client_data->accel_cdev = accel_cdev;
	client_data->accel_cdev.sensors_enable = bmi160_cdev_enable_accel;
	client_data->accel_cdev.sensors_poll_delay = bmi160_accel_poll_delay;
	client_data->accel_cdev.sensors_calibrate = bmi160_self_calibration_xyz;
	err = sensors_classdev_register(&client_data->input_accel->dev,
			 &client_data->accel_cdev);
	if (err) {
		dev_err(&client_data->i2c->dev, "sensors class register failed.\n");
		return err;
	}

	client_data->gyro_cdev = gyro_cdev;
	client_data->gyro_cdev.sensors_enable = bmi160_cdev_enable_gyro;
	client_data->gyro_cdev.sensors_poll_delay = bmi160_gyro_poll_delay;
	err = sensors_classdev_register(&client_data->input_gyro->dev,
			&client_data->gyro_cdev);
	if (err) {
		dev_err(&client_data->i2c->dev, "sensors class register failed.\n");
		return err;
	}

	if (NULL != dev->platform_data) {
		client_data->bst_pd = kzalloc(sizeof(*client_data->bst_pd),
				GFP_KERNEL);

		if (NULL != client_data->bst_pd) {
			memcpy(client_data->bst_pd, dev->platform_data,
					sizeof(*client_data->bst_pd));
			dev_notice(dev, "%s sensor driver set place: p%d\n",
					client_data->bst_pd->name,
					client_data->bst_pd->place);
		}
	}

	if (NULL != client_data->bst_pd) {
		memcpy(client_data->bst_pd, dev->platform_data,
				sizeof(*client_data->bst_pd));
		dev_notice(dev, "%s sensor driver set place: p%d\n",
				client_data->bst_pd->name,
				client_data->bst_pd->place);
	}

	/* workqueue init */
	INIT_DELAYED_WORK(&client_data->work, bmi_work_func);
	INIT_DELAYED_WORK(&client_data->gyro_work, bmi_gyro_work_func);
	atomic_set(&client_data->delay, BMI_DELAY_DEFAULT);
	atomic_set(&client_data->wkqueue_en, 0);
	atomic_set(&client_data->gyro_en, 0);

	/* h/w init */
	client_data->device.delay_msec = bmi_delay;
	err = BMI_CALL_API(init)(&client_data->device);

	/*workqueue init*/
	INIT_WORK(&client_data->report_data_work, bmi_hrtimer_work_func);
	reportdata_wq = create_singlethread_workqueue("bmi160_wq");
	if (NULL == reportdata_wq) {
		pr_info("fail to create the reportdta_wq %d", -ENOMEM);
	}
	hrtimer_init(&client_data->timer, CLOCK_MONOTONIC,
		HRTIMER_MODE_REL);
	client_data->timer.function = reportdata_timer_fun;
	client_data->work_delay_kt = ns_to_ktime(10000000);
	client_data->is_timer_running = 0;
	client_data->time_odr = 500000; /*200Hz*/

	bmi_dump_reg(client_data);

	/*power on detected*/
	/*or softrest(cmd 0xB6) */
	/*fatal err check*/
	/*soft reset*/
	err += BMI_CALL_API(set_command_register)(CMD_RESET_USER_REG);
	mdelay(3);
	if (err)
		dev_err(dev, "Failed soft reset, er=%d", err);
	/*usr data config page*/
	err += BMI_CALL_API(set_target_page)(USER_DAT_CFG_PAGE);
	if (err)
		dev_err(dev, "Failed cffg page, er=%d", err);
	err += bmi_get_err_status(client_data);
	if (err) {
		dev_err(dev, "Failed to bmi16x init!err_st=0x%x\n",
				client_data->err_st.err_st_all);
		goto exit_err_sysfs;
	}

#ifdef BMI160_MAG_INTERFACE_SUPPORT
	err += bmi160_set_command_register(MAG_MODE_NORMAL);
	mdelay(2);
	err += bmi160_get_mag_power_mode_stat(&mag_op_mode);
	mdelay(2);
	err += BMI_CALL_API(get_i2c_device_addr)(&mag_dev_addr);
	mdelay(2);
#ifdef BMI160_AKM09912_SUPPORT
	err += BMI_CALL_API(set_i2c_device_addr)(BMI160_AKM09912_I2C_ADDRESS);
	/*do not need to check the return value for mag 2nd interface*/
	bmi160_bst_akm_mag_interface_init(BMI160_AKM09912_I2C_ADDRESS);
#else
	err += BMI_CALL_API(set_i2c_device_addr)(BMI160_AUX_BMM150_I2C_ADDRESS);
	/*do not need to check the return value for mag 2nd interface*/
	bmi160_bmm150_mag_interface_init();
#endif
	err += bmi160_set_mag_burst(3);
	err += bmi160_get_mag_burst(&mag_urst_len);
	dev_info(client_data->dev,
		"BMI160 mag_urst_len:%d, mag_add:0x%x, mag_op_mode:%d\n",
		mag_urst_len, mag_dev_addr, mag_op_mode);
#endif
	if (err < 0)
		goto exit_err_sysfs;

#ifdef BMI160_ENABLE_INT1
	client_data->gpio_pin = of_get_named_gpio_flags(dev->of_node,
				"bosch,gpio-int1", 0, NULL);
	dev_info(client_data->dev, "BMI160 qpio number:%d\n",
				client_data->gpio_pin);
	err += gpio_request_one(client_data->gpio_pin,
				GPIOF_IN, "bmi160_int");
	err += gpio_direction_input(client_data->gpio_pin);
	client_data->IRQ = gpio_to_irq(client_data->gpio_pin);
	if (err) {
		dev_err(client_data->dev,
			"can not request gpio to irq number\n");
		client_data->gpio_pin = 0;
	}
	/* maps interrupt to INT1/InT2 pin */
	BMI_CALL_API(set_intr_any_motion)(BMI_INT0, ENABLE);
	BMI_CALL_API(set_intr_fifo_wm)(BMI_INT0, ENABLE);
	/*BMI_CALL_API(set_int_drdy)(BMI_INT0, ENABLE);*/

	/*Set interrupt trige level way */
	BMI_CALL_API(set_intr_edge_ctrl)(BMI_INT0, BMI_INT_LEVEL);
	bmi160_set_intr_level(BMI_INT0, 1);
	/*set interrupt latch temporary, 5 ms*/
	/*bmi160_set_latch_int(5);*/

	BMI_CALL_API(set_output_enable)(
	BMI160_INTR1_OUTPUT_ENABLE, ENABLE);
	sigmotion_init_interrupts(BMI160_MAP_INTR1);
	BMI_CALL_API(map_step_detector_intr)(BMI160_MAP_INTR1);
	/*close step_detector in init function*/
	BMI_CALL_API(set_step_detector_enable)(0);
#endif

#ifdef BMI160_ENABLE_INT2
	client_data->gpio_pin = of_get_named_gpio_flags(dev->of_node,
				"bosch,gpio-int2", 0, NULL);
	dev_info(client_data->dev, "BMI160 qpio number:%d\n",
				client_data->gpio_pin);
	err += gpio_request_one(client_data->gpio_pin,
					GPIOF_IN, "bmi160_int");
	err += gpio_direction_input(client_data->gpio_pin);
	client_data->IRQ = gpio_to_irq(client_data->gpio_pin);
	if (err) {
		dev_err(client_data->dev,
			"can not request gpio to irq number\n");
		client_data->gpio_pin = 0;
	}
	/* maps interrupt to INT1/InT2 pin */
	BMI_CALL_API(set_intr_any_motion)(BMI_INT1, ENABLE);
	BMI_CALL_API(set_intr_fifo_wm)(BMI_INT1, ENABLE);
	BMI_CALL_API(set_int_drdy)(BMI_INT1, ENABLE);

	/*Set interrupt trige level way */
	BMI_CALL_API(set_intr_edge_ctrl)(BMI_INT1, BMI_INT_LEVEL);
	bmi160_set_intr_level(BMI_INT1, 1);
	/*set interrupt latch temporary, 5 ms*/
	/*bmi160_set_latch_int(5);*/

	BMI_CALL_API(set_output_enable)(
	BMI160_INTR2_OUTPUT_ENABLE, ENABLE);
	sigmotion_init_interrupts(BMI160_MAP_INTR2);
	BMI_CALL_API(map_step_detector_intr)(BMI160_MAP_INTR2);
	/*close step_detector in init function*/
	BMI_CALL_API(set_step_detector_enable)(0);
#endif
	err = request_irq(client_data->IRQ, bmi_irq_handler,
			IRQF_TRIGGER_RISING, "bmi160", client_data);
	if (err)
		dev_err(client_data->dev, "could not request irq\n");
	INIT_WORK(&client_data->irq_work, bmi_irq_work_func);

	client_data->selftest = 0;

	client_data->fifo_data_sel = 0;
	BMI_CALL_API(get_accel_output_data_rate)(&client_data->odr.acc_odr);
	BMI_CALL_API(get_gyro_output_data_rate)(&client_data->odr.gyro_odr);
	BMI_CALL_API(get_mag_output_data_rate)(&client_data->odr.mag_odr);
	BMI_CALL_API(set_fifo_time_enable)(1);
	BMI_CALL_API(get_accel_range)(&client_data->range.acc_range);
	BMI_CALL_API(get_gyro_range)(&client_data->range.gyro_range);
	/* now it's power on which is considered as resuming from suspend */

	/* set sensor PMU into suspend power mode for all */
	if (bmi_pmu_set_suspend(client_data) < 0) {
		dev_err(dev, "Failed to set BMI160 to suspend power mode\n");
		goto exit_err_sysfs;
	}

	bmi160_power_ctl(client_data, false);

	dev_notice(dev, "sensor_time:%d, %d",
		sensortime_duration_tbl[0].ts_delat,
		sensortime_duration_tbl[0].ts_duration_lsb);
	dev_notice(dev, "sensor %s probed successfully", SENSOR_NAME);

	return 0;

exit_err_sysfs:
	if (err)
		bmi_input_destroy(client_data);
disable_power_exit:
	bmi160_power_ctl(client_data, false);
deinit_power_exit:
	bmi160_power_deinit(client_data);
exit_err_clean:
	if (err) {
		if (client_data != NULL) {
			if (NULL != client_data->bst_pd) {
				kfree(client_data->bst_pd);
				client_data->bst_pd = NULL;
			}
		}
	}

	return err;
}
EXPORT_SYMBOL(bmi_probe);

/*!
 * @brief remove bmi client
 *
 * @param dev the pointer of device
 *
 * @return zero
 * @retval zero
*/
int bmi_remove(struct device *dev)
{
	int err = 0;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);

	if (NULL != client_data) {
#ifdef CONFIG_HAS_EARLYSUSPEND
		unregister_early_suspend(&client_data->early_suspend_handler);
#endif
		mutex_lock(&client_data->mutex_enable);
		if (BMI_ACC_PM_NORMAL == client_data->pw.acc_pm ||
			BMI_GYRO_PM_NORMAL == client_data->pw.gyro_pm ||
				BMI_MAG_PM_NORMAL == client_data->pw.mag_pm) {
			cancel_delayed_work_sync(&client_data->work);
		}
		mutex_unlock(&client_data->mutex_enable);

		err = bmi_pmu_set_suspend(client_data);

		mdelay(5);

		sysfs_remove_group(&client_data->i2c->dev.kobj,
				&bmi160_attribute_group);
		bmi_input_destroy(client_data);

		if (NULL != client_data->bst_pd) {
			kfree(client_data->bst_pd);
			client_data->bst_pd = NULL;
		}
	kfree(client_data);
	}
	return err;
}
EXPORT_SYMBOL(bmi_remove);

static int bmi_post_resume(struct bmi_client_data *client_data)
{
	int err = 0;

	mutex_lock(&client_data->mutex_enable);

	if (atomic_read(&client_data->wkqueue_en) == 1) {
		bmi160_set_acc_op_mode(client_data, BMI_ACC_PM_NORMAL);
		schedule_delayed_work(&client_data->work,
				msecs_to_jiffies(
					atomic_read(&client_data->delay)));
	}
	mutex_unlock(&client_data->mutex_enable);
	if (client_data->is_timer_running) {
		hrtimer_start(&client_data->timer,
					ns_to_ktime(client_data->time_odr),
			HRTIMER_MODE_REL);
		client_data->base_time = 0;
		client_data->timestamp = 0;
		client_data->is_timer_running = 1;
	}

	return err;
}


int bmi_suspend(struct device *dev)
{
	int err = 0;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	unsigned char stc_enable;
	unsigned char std_enable;
	dev_err(client_data->dev, "bmi suspend function entrance");

	if (client_data->is_timer_running) {
		hrtimer_cancel(&client_data->timer);
		client_data->base_time = 0;
		client_data->timestamp = 0;
		client_data->fifo_time = 0;
	}

	if (atomic_read(&client_data->wkqueue_en) == 1) {
		bmi160_set_acc_op_mode(client_data, BMI_ACC_PM_SUSPEND);
		cancel_delayed_work_sync(&client_data->work);
	}
	BMI_CALL_API(get_step_counter_enable)(&stc_enable);
	BMI_CALL_API(get_step_detector_enable)(&std_enable);
	if (client_data->pw.acc_pm != BMI_ACC_PM_SUSPEND &&
		(stc_enable != 1) && (std_enable != 1) &&
		(client_data->sig_flag != 1)) {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_SUSPEND]);
		/*client_data->pw.acc_pm = BMI_ACC_PM_SUSPEND;*/
		mdelay(3);
	}
	if (client_data->pw.gyro_pm != BMI_GYRO_PM_SUSPEND) {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_SUSPEND]);
		/*client_data->pw.gyro_pm = BMI_GYRO_PM_SUSPEND;*/
		mdelay(3);
	}

	if (client_data->pw.mag_pm != BMI_MAG_PM_SUSPEND) {
#ifdef BMI160_AKM09912_SUPPORT
		err += bmi160_set_bst_akm_and_secondary_if_powermode
					(BMI160_MAG_SUSPEND_MODE);
#else
		err += bmi160_set_bmm150_mag_and_secondary_if_power_mode
					(BMI160_MAG_SUSPEND_MODE);
#endif
		/*client_data->pw.gyro_pm = BMI160_MAG_SUSPEND_MODE;*/
		mdelay(3);
	}

	return err;
}
EXPORT_SYMBOL(bmi_suspend);

int bmi_resume(struct device *dev)
{
	int err = 0;
	struct bmi_client_data *client_data = dev_get_drvdata(dev);
	if (client_data->pw.acc_pm != BMI_ACC_PM_SUSPEND) {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_acc_arr[BMI_ACC_PM_NORMAL]);
		mdelay(3);
	}
	if (client_data->pw.gyro_pm != BMI_GYRO_PM_SUSPEND) {
		err += BMI_CALL_API(set_command_register)
				(bmi_pmu_cmd_gyro_arr[BMI_GYRO_PM_NORMAL]);
		mdelay(3);
	}

	if (client_data->pw.mag_pm != BMI_MAG_PM_SUSPEND) {
#ifdef BMI160_AKM09912_SUPPORT
		err += bmi160_set_bst_akm_and_secondary_if_powermode
					(BMI160_MAG_FORCE_MODE);
#else
		err += bmi160_set_bmm150_mag_and_secondary_if_power_mode
					(BMI160_MAG_FORCE_MODE);
#endif
		mdelay(3);
	}
	/* post resume operation */
	err += bmi_post_resume(client_data);

	return err;
}
EXPORT_SYMBOL(bmi_resume);

