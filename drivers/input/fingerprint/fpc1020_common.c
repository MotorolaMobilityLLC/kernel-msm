/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013,2014 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#include "fpc1020_common.h"
#include "fpc1020_regs.h"
#include "fpc1020_capture.h"

#include <linux/sched.h>
struct chip_struct {
	fpc1020_chip_t type;
	u16 hwid;
	u8 revision;
	u8 pixel_rows;
	u8 pixel_columns;
	u8 adc_group_size;
};

#define SPI_MAX_TRY_TIMES    10
/* -------------------------------------------------------------------- */
/* fpc1020 driver constants                     */
/* -------------------------------------------------------------------- */
#define FPC1140_ROWS        192u
#define FPC1140_COLUMNS     56u

#define FPC1150_ROWS        208u
#define FPC1150_COLUMNS     80u

#define FPC1021_ROWS        160u
#define FPC1021_COLUMNS     160u

#define FPC1020_ROWS        192u
#define FPC1020_COLUMNS     192u
#define FPC102X_ADC_GROUP_SIZE  8u

#define FPC1020_EXT_HWID_CHECK_ID1020A_ROWS 5u
static const char *chip_text[] = {
	"N/A",			/* FPC1020_CHIP_NONE */
	"fpc1020a",		/* FPC1020_CHIP_1020A */
	"fpc1021a",		/* FPC1020_CHIP_1021A */
	"fpc1021b",		/* FPC1020_CHIP_1021B */
	"fpc1150a",		/* FPC1020_CHIP_1150A */
	"fpc1140b",		/* FPC1020_CHIP_1140B */
};

static const struct chip_struct chip_data[] = {
	{FPC1020_CHIP_1020A, 0x020a, 0, FPC1020_ROWS, FPC1020_COLUMNS,
	 FPC102X_ADC_GROUP_SIZE},
	{FPC1020_CHIP_1021A, 0x021a, 2, FPC1021_ROWS, FPC1021_COLUMNS,
	 FPC102X_ADC_GROUP_SIZE},
	{FPC1020_CHIP_1021B, 0x021b, 1, FPC1021_ROWS, FPC1021_COLUMNS,
	 FPC102X_ADC_GROUP_SIZE},
	{FPC1020_CHIP_1150A, 0x150a, 1, FPC1150_ROWS, FPC1150_COLUMNS,
	 FPC102X_ADC_GROUP_SIZE},
	{FPC1020_CHIP_1140B, 0x140b, 1, FPC1140_ROWS, FPC1140_COLUMNS,
	 FPC102X_ADC_GROUP_SIZE},
	{FPC1020_CHIP_NONE, 0, 0, 0, 0, 0}
};

const fpc1020_setup_t fpc1020_setup_default_CT_a1a2 = {
	.adc_gain = {1, 0, 1},	/*Dry, Wet, Normal */
	.adc_shift = {1, 5, 0},
	.pxl_ctrl = {0x0f0b, 0x0f1b, 0x0f1b},
	.capture_settings_mux = 0,
	.capture_count = 3,
	.capture_mode = FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start = 18,
	.capture_row_count = 173,
	.capture_col_start = 1,
	.capture_col_groups = 20,
	.capture_finger_up_threshold = 0,
	.capture_finger_down_threshold = 6,
	.finger_detect_threshold = 0x50,
	.wakeup_detect_rows = {92, 92},
	.wakeup_detect_cols = {8, 15},
	.finger_auto_threshold = true,
};

const fpc1020_setup_t fpc1020_setup_default_CT_a3a4 = {
	.adc_gain = {10, 2, 1},	/* Dry , Wet, Normal */
	.adc_shift = {3, 8, 13},
	.pxl_ctrl = {0x0f0a, 0x0f1b, 0x0f1b},
	.capture_settings_mux = 0,
	.capture_count = 3,
	.capture_mode = FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start = 18,
	.capture_row_count = 173,
	.capture_col_start = 1,
	.capture_col_groups = 20,
	.capture_finger_up_threshold = 0,
	.capture_finger_down_threshold = 6,
	.finger_detect_threshold = 0x50,
	.wakeup_detect_rows = {92, 92},
	.wakeup_detect_cols = {8, 15},
	.finger_auto_threshold = true,
};

const fpc1020_setup_t fpc1020_setup_default_LO_a1a2 = {
	.adc_gain = {0, 1, 2},	/*Dry, Wet, Normal */
	.adc_shift = {5, 1, 4},
	.pxl_ctrl = {0x0f0b, 0x0f1f, 0x0f0b},
	.capture_settings_mux = 0,
	.capture_count = 3,
	.capture_mode = FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start = 19,
	.capture_row_count = 171,
	.capture_col_start = 2,
	.capture_col_groups = 21,
	.capture_finger_up_threshold = 0,
	.capture_finger_down_threshold = 6,
	.finger_detect_threshold = 0x50,
	.wakeup_detect_rows = {92, 92},
	.wakeup_detect_cols = {8, 15},
	.finger_auto_threshold = true,
};

const fpc1020_setup_t fpc1020_setup_default_LO_a3a4 = {
	.adc_gain = {8, 0, 1},	/* Dry, Wet, Normal */
	.adc_shift = {4, 6, 11},
	.pxl_ctrl = {0x0f0a, 0x0f1f, 0x0f0B},
	.capture_settings_mux = 0,
	.capture_count = 3,
	.capture_mode = FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start = 19,
	.capture_row_count = 171,
	.capture_col_start = 2,
	.capture_col_groups = 21,
	.capture_finger_up_threshold = 0,
	.capture_finger_down_threshold = 6,
	.finger_detect_threshold = 0x50,
	.wakeup_detect_rows = {92, 92},
	.wakeup_detect_cols = {8, 15},
	.finger_auto_threshold = true,
};

const fpc1020_setup_t fpc1020_setup_default_1021_a2b1 = {
	.adc_gain = {10, 10, 10},	//{0, 0, 4}
	.adc_shift = {8, 8, 8},	//{14, 7, 6}
	.pxl_ctrl = {0x0f1b, 0x0f1f, 0x0f1b},
	.capture_settings_mux = 0,
	.capture_count = 1,
	.capture_mode = FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start = 0,
	.capture_row_count = FPC1021_ROWS,
	.capture_col_start = 0,
	.capture_col_groups = FPC1021_COLUMNS / FPC102X_ADC_GROUP_SIZE,
	.capture_finger_up_threshold = 0,
	.capture_finger_down_threshold = 6,
	.finger_detect_threshold = 0x50,
	.wakeup_detect_rows = {76, 76},
	.wakeup_detect_cols = {7, 11},
	.finger_auto_threshold = true,
};

const fpc1020_setup_t fpc1020_setup_default_1150_a1 = {
	.adc_gain = {0, 0, 4},
	.adc_shift = {14, 7, 6},
	.pxl_ctrl = {0x0f1b, 0x0f1f, 0x0f1b},
	.capture_settings_mux = 0,
	.capture_count = 1,
	.capture_mode = FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start = 0,
	.capture_row_count = FPC1150_ROWS,
	.capture_col_start = 0,
	.capture_col_groups = FPC1150_COLUMNS / FPC102X_ADC_GROUP_SIZE,
	.capture_finger_up_threshold = 0,
	.capture_finger_down_threshold = 7,
	.finger_detect_threshold = 0x50,
	.wakeup_detect_rows = {72, 128},
	.wakeup_detect_cols = {4, 4},
	.finger_auto_threshold = true,
};

const fpc1020_setup_t fpc1020_setup_default_1140 = {
	.adc_gain = {2, 2, 2},
	.adc_shift = {10, 10, 10},
	.pxl_ctrl = {0x1e, 0x0e, 0x0a},
	.capture_settings_mux = 0,
	.capture_count = 1,
	.capture_mode = FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start = 0,
	.capture_row_count = FPC1140_ROWS,
	.capture_col_start = 0,
	.capture_col_groups = FPC1140_COLUMNS / FPC102X_ADC_GROUP_SIZE,
	.capture_finger_up_threshold = 0,
	.capture_finger_down_threshold = 7,
	.finger_detect_threshold = 0x50,
	.wakeup_detect_rows = {66, 118},
	.wakeup_detect_cols = {24, 24},
	.finger_auto_threshold = true,
};

const fpc1020_diag_t fpc1020_diag_default = {
	.selftest = 0,
	.spi_register = 0,
	.spi_regsize = 0,
	.spi_data = 0,
	.fingerdetect = 0,
	.navigation_enable = 0,
	.wakeup_enable = 0,
	.result = 0,
};

static int fpc1020_check_hw_id_extended(fpc1020_data_t * fpc1020);

static int fpc1020_hwid_1020a(fpc1020_data_t * fpc1020);

static int fpc1020_write_id_1020a_setup(fpc1020_data_t * fpc1020);

static int fpc1020_write_sensor_1020a_setup(fpc1020_data_t * fpc1020);
static int fpc1020_write_sensor_1020a_a1a2_setup(fpc1020_data_t * fpc1020);
static int fpc1020_write_sensor_1020a_a3a4_setup(fpc1020_data_t * fpc1020);

static int fpc1020_write_sensor_1021_setup(fpc1020_data_t * fpc1020);

static int fpc1020_write_sensor_1150a_setup(fpc1020_data_t * fpc1020);

static int fpc1020_write_sensor_1140_setup(fpc1020_data_t * fpc1020);
/* -------------------------------------------------------------------- */
/* function definitions                         */
/* -------------------------------------------------------------------- */
u32 fpc1020_calc_huge_buffer_minsize(fpc1020_data_t * fpc1020)
{
	const u32 buff_min = FPC1020_EXT_HWID_CHECK_ID1020A_ROWS *
	    FPC1020_COLUMNS;
	u32 buff_req;

	buff_req = (fpc1020->chip.type == FPC1020_CHIP_NONE) ? buff_min :
	    (fpc1020->chip.pixel_columns *
	     fpc1020->chip.pixel_rows * FPC1020_BUFFER_MAX_IMAGES);

	return (buff_req > buff_min) ? buff_req : buff_min;
}

#ifdef CONFIG_HUAWEI_DSM
void send_msg_to_dsm(struct dsm_client *dsm_client, int error, int dsm)
{
	int err_dsm = 0;
	if (!dsm_client_ocuppy(dsm_client)) {
		if (dsm == DSM_FINGERPRINT_SPISYNC_ERROR_NO) {
			dsm_client_record(dsm_client, "spi sync error=%d\n",
					  error);
			switch (error) {
			case -ESHUTDOWN:
				err_dsm =
				    DSM_FINGERPRINT_SPI_ESHUTDOWN_ERROR_NO;
				break;
			case -EBUSY:
				err_dsm = DSM_FINGERPRINT_SPI_EBUSY_ERROR_NO;
				break;
			case -EINVAL:
				err_dsm = DSM_FINGERPRINT_SPI_EINVAL_ERROR_NO;
				break;
			case -EIO:
				err_dsm = DSM_FINGERPRINT_SPI_EIO_ERROR_NO;
				break;
			case -EINPROGRESS:
				err_dsm =
				    DSM_FINGERPRINT_SPI_EINPROGRESS_ERROR_NO;
				break;
			default:
				err_dsm = DSM_FINGERPRINT_SPISYNC_ERROR_NO;
				break;
			}
		} else if (dsm == DSM_FINGERPRINT_TEST_DEADPIXELS_ERROR_NO) {
			dsm_client_record(dsm_client,
					  "test dead pixels error=%d\n", error);
			switch (error) {
			case FPC_1020_MIN_CHECKER_DIFF_ERROR:
				err_dsm =
				    DSM_FINGERPRINT_DIFF_DEADPIXELS_ERROR_NO;
				break;
			case FPC_1020_TOO_MANY_DEADPIXEL_ERROR:
				err_dsm =
				    DSM_FINGERPRINT_MANY_DEADPIXELS_ERROR_NO;
				break;
			default:
				err_dsm =
				    DSM_FINGERPRINT_TEST_DEADPIXELS_ERROR_NO;
				break;
			}
		} else
			return;
		dsm_client_notify(dsm_client, err_dsm);
	}

}
#endif

/* -------------------------------------------------------------------- */
int fpc1020_manage_huge_buffer(fpc1020_data_t * fpc1020, u32 new_size)
{
	int error = 0;
	int buffer_order_new, buffer_order_curr;

	buffer_order_curr = get_order(fpc1020->huge_buffer_size);
	buffer_order_new = get_order(new_size);

	if (new_size == 0) {
		if (fpc1020->huge_buffer) {
			free_pages((unsigned long)fpc1020->huge_buffer,
				   buffer_order_curr);

			fpc1020->huge_buffer = NULL;
		}
		fpc1020->huge_buffer_size = 0;
		error = 0;

	} else {
		if (fpc1020->huge_buffer &&
		    (buffer_order_curr != buffer_order_new)) {

			free_pages((unsigned long)fpc1020->huge_buffer,
				   buffer_order_curr);

			fpc1020->huge_buffer = NULL;
		}

		if (fpc1020->huge_buffer == NULL) {
			fpc1020->huge_buffer =
			    (u8 *) __get_free_pages(GFP_KERNEL,
						    buffer_order_new);

			fpc1020->huge_buffer_size = (fpc1020->huge_buffer) ?
			    (u32) PAGE_SIZE << buffer_order_new : 0;

			error = (fpc1020->huge_buffer_size == 0) ? -ENOMEM : 0;
		}
	}

	if (error) {
		dev_err(&fpc1020->spi->dev, "%s, failed %d\n", __func__, error);
	} else {
		dev_info(&fpc1020->spi->dev, "%s, size=%d bytes\n",
			 __func__, fpc1020->huge_buffer_size);
	}

	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_setup_defaults(fpc1020_data_t * fpc1020)
{
	int error;
	const fpc1020_setup_t *ptr;

	memcpy((void *)&fpc1020->diag,
	       (void *)&fpc1020_diag_default, sizeof(fpc1020_diag_t));

	error = (fpc1020->chip.type == FPC1020_CHIP_1020A) ||
	    (fpc1020->chip.type == FPC1020_CHIP_1021A) ||
	    (fpc1020->chip.type == FPC1020_CHIP_1021B) ||
	    (fpc1020->chip.type == FPC1020_CHIP_1150A) ||
	    (fpc1020->chip.type == FPC1020_CHIP_1140B) ? 0 : -EINVAL;

	if (error)
		goto out_err;
	if (fpc1020->chip.type == FPC1020_CHIP_1020A) {
		if (gpio_is_valid(fpc1020->moduleID_gpio)) {
			printk
			    ("fingerprint module ID = %d  chip_revision =%d \n",
			     gpio_get_value(fpc1020->moduleID_gpio),
			     fpc1020->chip.revision);
			if (gpio_get_value(fpc1020->moduleID_gpio)) {
				printk("fingerprint module CT high \n");
				ptr =
				    (fpc1020->chip.revision ==
				     1) ? &fpc1020_setup_default_CT_a1a2
				    : (fpc1020->chip.revision ==
				       2) ? &fpc1020_setup_default_CT_a1a2
				    : (fpc1020->chip.revision ==
				       3) ? &fpc1020_setup_default_CT_a3a4
				    : (fpc1020->chip.revision ==
				       4) ? &fpc1020_setup_default_CT_a3a4 :
				    NULL;
			} else {
				printk("fingerprint module LO low \n");
				ptr =
				    (fpc1020->chip.revision ==
				     1) ? &fpc1020_setup_default_LO_a1a2
				    : (fpc1020->chip.revision ==
				       2) ? &fpc1020_setup_default_LO_a1a2
				    : (fpc1020->chip.revision ==
				       3) ? &fpc1020_setup_default_LO_a3a4
				    : (fpc1020->chip.revision ==
				       4) ? &fpc1020_setup_default_LO_a3a4 :
				    NULL;
			}

		} else
			ptr =
			    (fpc1020->chip.revision ==
			     1) ? &fpc1020_setup_default_LO_a1a2
			    : (fpc1020->chip.revision ==
			       2) ? &fpc1020_setup_default_LO_a1a2 : (fpc1020->
								      chip.revision
								      ==
								      3) ?
			    &fpc1020_setup_default_LO_a3a4 : (fpc1020->
							      chip.revision ==
							      4) ?
			    &fpc1020_setup_default_LO_a3a4 : NULL;
	} else if (fpc1020->chip.type == FPC1020_CHIP_1021A) {
		ptr =
		    (fpc1020->chip.revision ==
		     2) ? &fpc1020_setup_default_1021_a2b1 : NULL;
	} else if (fpc1020->chip.type == FPC1020_CHIP_1021B) {
		ptr =
		    (fpc1020->chip.revision ==
		     1) ? &fpc1020_setup_default_1021_a2b1 : NULL;
	} else if (fpc1020->chip.type == FPC1020_CHIP_1150A) {
		ptr =
		    (fpc1020->chip.revision ==
		     1) ? &fpc1020_setup_default_1150_a1 : NULL;
	} else if (fpc1020->chip.type == FPC1020_CHIP_1140B) {
		ptr =
		    (fpc1020->chip.revision ==
		     1) ? &fpc1020_setup_default_1140 : NULL;
	} else {
		ptr = NULL;
	}

	error = (ptr == NULL) ? -EINVAL : 0;
	if (error)
		goto out_err;

	memcpy((void *)&fpc1020->setup, ptr, sizeof(fpc1020_setup_t));
	fpc1020->fp_thredhold = fpc1020->setup.finger_detect_threshold;
	printk("fingerprint setup_defaults fp_thredhold=%d \n",
	       fpc1020->fp_thredhold);
	dev_dbg(&fpc1020->spi->dev, "%s OK\n", __func__);

	return 0;

out_err:
	memset((void *)&fpc1020->setup, 0, sizeof(fpc1020_setup_t));
	dev_err(&fpc1020->spi->dev, "%s FAILED %d\n", __func__, error);

	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_gpio_reset(fpc1020_data_t * fpc1020)
{
	int error = 0;
	int counter = FPC1020_RESET_RETRIES;

	while (counter) {
		counter--;

		gpio_set_value(fpc1020->reset_gpio, 1);
		udelay(FPC1020_RESET_HIGH1_US);

		gpio_set_value(fpc1020->reset_gpio, 0);
		udelay(FPC1020_RESET_LOW_US);

		gpio_set_value(fpc1020->reset_gpio, 1);
		udelay(FPC1020_RESET_HIGH2_US);

		error = gpio_get_value(fpc1020->irq_gpio) ? 0 : -EIO;

		if (!error) {
			printk(KERN_INFO "%s OK !\n", __func__);
			counter = 0;
		} else {
			printk(KERN_INFO "%s timed out,retrying ...\n",
			       __func__);

			udelay(1250);
		}
	}
	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_spi_reset(fpc1020_data_t * fpc1020)
{
	int error = 0;
	int counter = FPC1020_RESET_RETRIES;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	while (counter) {
		counter--;

		error = fpc1020_cmd(fpc1020, FPC1020_CMD_SOFT_RESET, false);

		if (error >= 0) {
			error = fpc1020_wait_for_irq(fpc1020,
						     FPC1020_DEFAULT_IRQ_TIMEOUT_MS);
		}

		if (error >= 0) {
			error = gpio_get_value(fpc1020->irq_gpio) ? 0 : -EIO;

			if (!error) {
				dev_dbg(&fpc1020->spi->dev,
					"%s OK !\n", __func__);

				counter = 0;

			} else {
				dev_dbg(&fpc1020->spi->dev,
					"%s timed out,retrying ...\n",
					__func__);
			}
		}
	}
	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_reset(fpc1020_data_t * fpc1020)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	error = (fpc1020->soft_reset_enabled) ?
	    fpc1020_spi_reset(fpc1020) : fpc1020_gpio_reset(fpc1020);

	if (error < 0) {
#ifdef CONFIG_HUAWEI_DSM
		if (!dsm_client_ocuppy(fpc1020->dsm_fingerprint_client)) {
			dsm_client_record(fpc1020->dsm_fingerprint_client,
					  "GPIO Reset error=%d\n", error);
			dsm_client_notify(fpc1020->dsm_fingerprint_client,
					  DSM_FINGERPRINT_RESET_ERROR_NO);
		}
#endif
		goto out;
	}
	disable_irq(fpc1020->irq);
	fpc1020->interrupt_done = false;
	enable_irq(fpc1020->irq);

	error = fpc1020_read_irq_for_reset(fpc1020, true);

	if (error < 0)
		goto out;

	if (error != FPC_1020_IRQ_REG_BITS_REBOOT) {
		dev_err(&fpc1020->spi->dev,
			"IRQ register, expected 0x%x, got 0x%x.\n",
			FPC_1020_IRQ_REG_BITS_REBOOT, (u8) error);

		error = -EIO;
		goto out;
	}

	error = (gpio_get_value(fpc1020->irq_gpio) != 0) ? -EIO : 0;

	if (error)
		dev_err(&fpc1020->spi->dev, "IRQ pin, not low after clear.\n");

	error = fpc1020_read_irq(fpc1020, true);

	if (error != 0) {
		dev_err(&fpc1020->spi->dev,
			"IRQ register, expected 0x%x, got 0x%x.\n",
			0, (u8) error);

		error = -EIO;
		goto out;
	}

	fpc1020->capture.available_bytes = 0;
	fpc1020->capture.read_offset = 0;
	fpc1020->capture.read_pending_eof = false;
out:
	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_check_hw_id(fpc1020_data_t * fpc1020)
{
	int error = 0;
	u16 hardware_id;

	fpc1020_reg_access_t reg;
	int counter = 0;
	bool match = false;

	FPC1020_MK_REG_READ(reg, FPC102X_REG_HWID, &hardware_id);
	error = fpc1020_reg_access(fpc1020, &reg);

	if (error)
		return error;

	while (!match && chip_data[counter].type != FPC1020_CHIP_NONE) {
		if (chip_data[counter].hwid == hardware_id)
			match = true;
		else
			counter++;
	}

	if (match) {
		fpc1020->chip.type = chip_data[counter].type;
		fpc1020->chip.revision = chip_data[counter].revision;

		fpc1020->chip.pixel_rows = chip_data[counter].pixel_rows;
		fpc1020->chip.pixel_columns = chip_data[counter].pixel_columns;
		fpc1020->chip.adc_group_size =
		    chip_data[counter].adc_group_size;

		if (fpc1020->chip.revision == 0) {
			error = fpc1020_check_hw_id_extended(fpc1020);

			if (error > 0) {
				fpc1020->chip.revision = error;
				error = 0;
			}
		}

		dev_info(&fpc1020->spi->dev,
			 "Hardware id: 0x%x (%s, rev.%d) \n",
			 hardware_id,
			 chip_text[fpc1020->chip.type], fpc1020->chip.revision);
	} else {
		dev_err(&fpc1020->spi->dev,
			"Hardware id mismatch: got 0x%x\n", hardware_id);

		fpc1020->chip.type = FPC1020_CHIP_NONE;
		fpc1020->chip.revision = 0;

		return -EIO;
	}

	return error;
}

/* -------------------------------------------------------------------- */
const char *fpc1020_hw_id_text(fpc1020_data_t * fpc1020)
{
	return chip_text[fpc1020->chip.type];
}

/* -------------------------------------------------------------------- */
static int fpc1020_check_hw_id_extended(fpc1020_data_t * fpc1020)
{
	int error = 0;

	if (fpc1020->chip.revision != 0) {
		return fpc1020->chip.revision;
	}

	error = (fpc1020->chip.type == FPC1020_CHIP_1020A) ?
	    fpc1020_hwid_1020a(fpc1020) : -EINVAL;

	if (error < 0) {
		dev_err(&fpc1020->spi->dev,
			"%s, Unable to check chip revision %d\n",
			__func__, error);
	}

	return (error < 0) ? error : fpc1020->chip.revision;
}

/* -------------------------------------------------------------------- */
static int fpc1020_hwid_1020a(fpc1020_data_t * fpc1020)
{
	int error = 0;
	int xpos, ypos, m1, m2, count;

	const int num_rows = FPC1020_EXT_HWID_CHECK_ID1020A_ROWS;
	const int num_pixels = 32;
	const u32 image_size = num_rows * fpc1020->chip.pixel_columns;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	if (fpc1020->chip.type != FPC1020_CHIP_1020A)
		return -EINVAL;

	error = fpc1020_write_id_1020a_setup(fpc1020);
	if (error)
		goto out_err;

	error = fpc1020_capture_set_crop(fpc1020,
					 0,
					 fpc1020->chip.pixel_columns /
					 fpc1020->chip.adc_group_size, 0,
					 num_rows);
	if (error)
		goto out_err;

	error = fpc1020_capture_buffer(fpc1020,
				       fpc1020->huge_buffer, 0, image_size);
	if (error)
		goto out_err;

	m1 = m2 = count = 0;

	for (ypos = 1; ypos < num_rows; ypos++) {
		for (xpos = 0; xpos < num_pixels; xpos++) {
			m1 += fpc1020->huge_buffer
			    [(ypos * fpc1020->chip.pixel_columns) + xpos];

			m2 += fpc1020->huge_buffer
			    [(ypos * fpc1020->chip.pixel_columns) +
			     (fpc1020->chip.pixel_columns - 1 - xpos)];
			count++;
		}
	}

	m1 /= count;
	m2 /= count;

	if (fpc1020_check_in_range_u64(m1, 181, 219) &&
	    fpc1020_check_in_range_u64(m2, 101, 179)) {
		fpc1020->chip.revision = 1;
	} else if (fpc1020_check_in_range_u64(m1, 181, 219) &&
		   fpc1020_check_in_range_u64(m2, 181, 219)) {
		fpc1020->chip.revision = 2;
	} else if (fpc1020_check_in_range_u64(m1, 101, 179) &&
		   fpc1020_check_in_range_u64(m2, 151, 179)) {
		fpc1020->chip.revision = 3;
	} else if (fpc1020_check_in_range_u64(m1, 0, 99) &&
		   fpc1020_check_in_range_u64(m2, 0, 99)) {
		fpc1020->chip.revision = 4;
	} else {
		fpc1020->chip.revision = 0;
	}

	dev_dbg(&fpc1020->spi->dev, "%s m1,m2 = %d,%d %s rev=%d \n", __func__,
		m1, m2,
		(fpc1020->chip.revision == 0) ? "UNKNOWN!" : "detected",
		fpc1020->chip.revision);

out_err:
	return error;
}

/* -------------------------------------------------------------------- */
static int fpc1020_write_id_1020a_setup(fpc1020_data_t * fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	fpc1020_reg_access_t reg;

	temp_u16 = 15 << 8;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = 0xffff;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_TST_COL_PATTERN_EN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x04;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_write_sensor_setup(fpc1020_data_t * fpc1020)
{
	switch (fpc1020->chip.type) {
	case FPC1020_CHIP_1020A:
		return fpc1020_write_sensor_1020a_setup(fpc1020);

	case FPC1020_CHIP_1021A:
	case FPC1020_CHIP_1021B:
		return fpc1020_write_sensor_1021_setup(fpc1020);

	case FPC1020_CHIP_1150A:
		return fpc1020_write_sensor_1150a_setup(fpc1020);

	case FPC1020_CHIP_1140B:
		return fpc1020_write_sensor_1140_setup(fpc1020);

	default:
		break;
	}

	return -EINVAL;
}

/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1020a_setup(fpc1020_data_t * fpc1020)
{
	switch (fpc1020->chip.revision) {
	case 1:
	case 2:
		return fpc1020_write_sensor_1020a_a1a2_setup(fpc1020);

	case 3:
	case 4:
		return fpc1020_write_sensor_1020a_a3a4_setup(fpc1020);

	default:
		return fpc1020_write_sensor_1020a_a3a4_setup(fpc1020);
		break;
	}

	return -EINVAL;
}

/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1020a_a1a2_setup(fpc1020_data_t * fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u32 temp_u32;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	const int mux = 0;
	const int rev = fpc1020->chip.revision;

//    dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	if (rev == 0)
		return -EINVAL;

	temp_u64 = (rev == 1) ? 0x363636363f3f3f3f : 0x141414141e1e1e1e;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = (rev == 1) ? 0x33 : 0x0f;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_RST_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = (rev == 1) ? 0x37 : 0x15;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	if (rev < 3) {
		temp_u64 = 0x5540003f24;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SETUP, &temp_u64);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;

		temp_u32 = 0x00080000;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ANA_TEST_MUX, &temp_u32);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;

		temp_u64 = 0x5540003f34;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SETUP, &temp_u64);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;
	}

	temp_u8 = 0x02;		//32内部3.3V  02是外部供电VDDTX
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.adc_shift[mux];
	temp_u16 <<= 8;
	temp_u16 |= fpc1020->setup.adc_gain[mux];

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.pxl_ctrl[mux];
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = fpc1020->fp_thredhold;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_THRES, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = 0x0005;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_CNTR, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}

/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1020a_a3a4_setup(fpc1020_data_t * fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	const int mux = 0;
	const int rev = fpc1020->chip.revision;

//    dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	if (rev == 4) {
		temp_u8 = 0x09;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY,
				     &temp_u8);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;

		temp_u64 = 0x0808080814141414;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;
	} else if (rev == 3) {
		temp_u64 = 0x1717171723232323;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;

		temp_u8 = 0x0f;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_RST_DLY, &temp_u8);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;

		temp_u8 = 0x18;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY,
				     &temp_u8);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;
	} else {
		error = -EINVAL;
		goto out;
	}

	temp_u8 = 0x02;		/* internal supply */
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.adc_shift[mux];
	temp_u16 <<= 8;
	temp_u16 |= fpc1020->setup.adc_gain[mux];

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.pxl_ctrl[mux];
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = fpc1020->fp_thredhold;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_THRES, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = 0x0005;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_CNTR, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}

/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1021_setup(fpc1020_data_t * fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	const int mux = 0;
	const int rev = fpc1020->chip.revision;

	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	if (rev == 0)
		return -EINVAL;

	temp_u8 = 0x09;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x0808080814141414;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x02;		/* internal supply */
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.adc_shift[mux];
	temp_u16 <<= 8;
	temp_u16 |= fpc1020->setup.adc_gain[mux];

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.pxl_ctrl[mux];
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->fp_thredhold;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_THRES, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = 0x0005;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_CNTR, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}

/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1150a_setup(fpc1020_data_t * fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u32 temp_u32;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	const int mux = 0;
	const int rev = fpc1020->chip.revision;

	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	if (rev == 0)
		return -EINVAL;

	temp_u8 = 0x09;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x0808080814141414;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x02;		/* internal supply */
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.adc_shift[mux];
	temp_u16 <<= 8;
	temp_u16 |= fpc1020->setup.adc_gain[mux];

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.pxl_ctrl[mux];
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u32 = fpc1020->fp_thredhold;
	FPC1020_MK_REG_WRITE(reg, FPC1150_REG_FNGR_DET_THRES, &temp_u32);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u32 = 0x190100ff;
	FPC1020_MK_REG_WRITE(reg, FPC1150_REG_FNGR_DET_CNTR, &temp_u32);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}

/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1140_setup(fpc1020_data_t * fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u32 temp_u32;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	const int mux = FPC1020_BUFFER_MAX_IMAGES - 1;
	const int rev = fpc1020->chip.revision;

	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	if (rev == 0)
		return -EINVAL;

	temp_u8 = 0x09;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x0808080814141414;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

//    temp_u8 = (fpc1020->vddtx_mv > 0) ? 0x02 :  /* external supply */
//        (fpc1020->txout_boost) ? 0x22 : 0x12;   /* internal supply */
	temp_u8 = 0x02;		/* internal supply */
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.adc_shift[mux];
	temp_u16 <<= 8;
	temp_u16 |= fpc1020->setup.adc_gain[mux];

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.pxl_ctrl[mux];
	temp_u16 |= FPC1020_PXL_BIAS_CTRL;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u32 = 0x0001;	/* fngrUpSteps */
	temp_u32 <<= 8;
	temp_u32 |= fpc1020->setup.finger_detect_threshold;	/* fngrLstThr */
	temp_u32 <<= 8;
	temp_u32 |= fpc1020->setup.finger_detect_threshold;	/* fngrDetThr */
	FPC1020_MK_REG_WRITE(reg, FPC1150_REG_FNGR_DET_THRES, &temp_u32);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	//temp_u32 = 0x190100ff;
	temp_u32 = 0x00000005;
	FPC1020_MK_REG_WRITE(reg, FPC1150_REG_FNGR_DET_CNTR, &temp_u32);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_write_cb_test_setup_1140(fpc1020_data_t * fpc1020, bool invert)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u64 temp_u64;
	fpc1020_reg_access_t reg;

	temp_u16 = (invert) ? 0x55aa : 0xaa55;
	dev_dbg(&fpc1020->spi->dev, "%s, pattern 0x%x\n", __func__, temp_u16);

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_TST_COL_PATTERN_EN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x04;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = (invert) ? 0x0f1b : 0x0f0f;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = (invert) ? 0x0800 : 0x00;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x14;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_RST_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x20;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x1e1e1e1e2d2d2d2d;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_write_sensor_checkerboard_setup_1020(fpc1020_data_t * fpc1020,
						 u16 pattern)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u32 temp_u32;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	int mux = 0;
	int rev = fpc1020->chip.revision;

	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);
	printk("%s: chip.revision = %d\n", __func__, fpc1020->chip.revision);

	if (rev == 0)
		return -EINVAL;

	temp_u64 = (rev == 1
		    || rev == 2) ? 0x363636363f3f3f3f : 0x141414141e1e1e1e;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = (rev == 1 || rev == 2) ? 0x33 : 0x0f;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_RST_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = (rev == 1 || rev == 2) ? 0x37 : 0x15;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	if (rev < 3) {
		temp_u64 = 0x5540003f24;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SETUP, &temp_u64);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;

		temp_u32 = 0x00080000;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ANA_TEST_MUX, &temp_u32);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;

		temp_u64 = 0x5540003f34;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SETUP, &temp_u64);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;
	}

	temp_u8 = 0x02;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = (rev == 1 || rev == 2) ? 0x0001 : 0x1100;

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = 0x0f1b;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x50;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_THRES, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = 0x0005;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_CNTR, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = pattern;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_TST_COL_PATTERN_EN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_write_sensor_checkerboard_setup_1021(fpc1020_data_t * fpc1020,
						 u16 pattern)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u64 temp_u64;
	fpc1020_reg_access_t reg;

	temp_u16 = pattern;	//(invert) ? 0x55aa : 0xaa55;
	dev_dbg(&fpc1020->spi->dev, "%s, pattern 0x%x\n", __func__, temp_u16);

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_TST_COL_PATTERN_EN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x14;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = (pattern == 0x55aa) ? 0x0f1a : 0x0f0e;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = (pattern == 0x55aa) ? 0x0800 : 0x0000;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x2f;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_RST_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x38;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x373737373f3f3f3f;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x5540002d24;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SETUP, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_wait_for_irq(fpc1020_data_t * fpc1020, int timeout)
{
	int result = 0;
	/* check input parameter */
	if (!timeout) {
		result = wait_event_interruptible(fpc1020->wq_irq_return,
						  fpc1020->interrupt_done);
	} else {
		result =
		    wait_event_interruptible_timeout(fpc1020->wq_irq_return,
						     fpc1020->interrupt_done,
						     timeout);
	}

	if (result < 0) {
		dev_err(&fpc1020->spi->dev,
			"wait_event_interruptible interrupted by signal.\n");

		return result;
	}

	if (result || !timeout) {
		fpc1020->interrupt_done = false;
		return 0;
	}

	return -ETIMEDOUT;
}

/* -------------------------------------------------------------------- */
int fpc1020_read_irq(fpc1020_data_t * fpc1020, bool clear_irq)
{
	int error = 0;
	u8 irq_status;
	/* const */ fpc1020_reg_access_t reg_read = {
		.reg = FPC102X_REG_READ_INTERRUPT,
		.write = false,
		.reg_size = FPC1020_REG_SIZE(FPC102X_REG_READ_INTERRUPT),
		.dataptr = &irq_status
	};

	/* const */ fpc1020_reg_access_t reg_clear = {
		.reg = FPC102X_REG_READ_INTERRUPT_WITH_CLEAR,
		.write = false,
		.reg_size =
		    FPC1020_REG_SIZE(FPC102X_REG_READ_INTERRUPT_WITH_CLEAR),
		.dataptr = &irq_status
	};

	error = fpc1020_reg_access(fpc1020,
				   (clear_irq) ? &reg_clear : &reg_read);

	if (error >= 0 && irq_status == FPC_1020_IRQ_REG_BITS_REBOOT) {
		dev_err(&fpc1020->spi->dev, "%s irq_status = 0x%x\n", __func__,
			irq_status);
		error = -EIO;
	}
	return (error < 0) ? error : irq_status;
}

/* -------------------------------------------------------------------- */
int fpc1020_read_irq_for_reset(fpc1020_data_t * fpc1020, bool clear_irq)
{
	int error = 0;
	u8 irq_status;
	/* const */ fpc1020_reg_access_t reg_read = {
		.reg = FPC102X_REG_READ_INTERRUPT,
		.write = false,
		.reg_size = FPC1020_REG_SIZE(FPC102X_REG_READ_INTERRUPT),
		.dataptr = &irq_status
	};

	/* const */ fpc1020_reg_access_t reg_clear = {
		.reg = FPC102X_REG_READ_INTERRUPT_WITH_CLEAR,
		.write = false,
		.reg_size =
		    FPC1020_REG_SIZE(FPC102X_REG_READ_INTERRUPT_WITH_CLEAR),
		.dataptr = &irq_status
	};

	error = fpc1020_reg_access(fpc1020,
				   (clear_irq) ? &reg_clear : &reg_read);

	return (error < 0) ? error : irq_status;
}

/* -------------------------------------------------------------------- */
int fpc1020_read_status_reg(fpc1020_data_t * fpc1020)
{
	int error = 0;
	u8 status;
	/* const */ fpc1020_reg_access_t reg_read = {
		.reg = FPC102X_REG_FPC_STATUS,
		.write = false,
		.reg_size = FPC1020_REG_SIZE(FPC102X_REG_FPC_STATUS),
		.dataptr = &status
	};

	error = fpc1020_reg_access(fpc1020, &reg_read);

	return (error < 0) ? error : status;
}

/* -------------------------------------------------------------------- */
int fpc1020_reg_access(fpc1020_data_t * fpc1020,
		       fpc1020_reg_access_t * reg_data)
{
	int error = 0;
	struct spi_message msg;
	unsigned int spi_failed_times = 0;

	struct spi_transfer cmd = {
		.cs_change = 0,
		.delay_usecs = 0,
		.speed_hz = FPC1020_SPI_CLOCK_SPEED,
		.tx_buf = &(reg_data->reg),
		.rx_buf = NULL,
		.len = 1 + FPC1020_REG_ACCESS_DUMMY_BYTES(reg_data->reg),
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 8,
	};

	struct spi_transfer data = {
		.cs_change = 1,
		.delay_usecs = 0,
		.speed_hz = FPC1020_SPI_CLOCK_SPEED,
		.tx_buf = (reg_data->write) ? fpc1020->huge_buffer : NULL,
		.rx_buf = (!reg_data->write) ? fpc1020->huge_buffer : NULL,
		.len = reg_data->reg_size,
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 8,
	};
#if CS_CONTROL
	gpio_set_value(fpc1020->cs_gpio, 0);
#endif
	if (reg_data->write) {
#if (target_little_endian)
		int src = 0;
		int dst = reg_data->reg_size - 1;

		while (src < reg_data->reg_size) {
			fpc1020->huge_buffer[dst] = reg_data->dataptr[src];
			src++;
			dst--;
		}
#else
		memcpy(fpc1020->huge_buffer,
		       reg_data->dataptr, reg_data->reg_size);
#endif
	}

	spi_message_init(&msg);
	spi_message_add_tail(&cmd, &msg);
	spi_message_add_tail(&data, &msg);

	do {
		error = spi_sync(fpc1020->spi, &msg);

		if (0 == error) {
			break;
		} else {
			dev_err(&fpc1020->spi->dev,
				"spi_sync failed error=%d.\n", error);
			msleep(100);
			spi_failed_times++;
		}
	} while (spi_failed_times < SPI_MAX_TRY_TIMES);
	if (error) {
		dev_err(&fpc1020->spi->dev,
			"%s %d: sync error reached MAX FAILed times\n",
			__func__, __LINE__);
#ifdef CONFIG_HUAWEI_DSM
		send_msg_to_dsm(fpc1020->dsm_fingerprint_client, error,
				DSM_FINGERPRINT_SPISYNC_ERROR_NO);
#endif
		error = -ECOMM;
		goto out;
	}

	if (!reg_data->write) {
#if (target_little_endian)
		int src = reg_data->reg_size - 1;
		int dst = 0;

		while (dst < reg_data->reg_size) {
			reg_data->dataptr[dst] = fpc1020->huge_buffer[src];
			src--;
			dst++;
		}
#else
		memcpy(reg_data->dataptr,
		       fpc1020->huge_buffer, reg_data->reg_size);
#endif
	}
#if CS_CONTROL
	gpio_set_value(fpc1020->cs_gpio, 1);
#endif
/*
    dev_dbg(&fpc1020->spi->dev,
        "%s %s 0x%x/%dd (%d bytes) %x %x %x %x : %x %x %x %x\n",
         __func__,
        (reg_data->write) ? "WRITE" : "READ",
        reg_data->reg,
        reg_data->reg,
        reg_data->reg_size,
        (reg_data->reg_size > 0) ? fpc1020->huge_buffer[0] : 0,
        (reg_data->reg_size > 1) ? fpc1020->huge_buffer[1] : 0,
        (reg_data->reg_size > 2) ? fpc1020->huge_buffer[2] : 0,
        (reg_data->reg_size > 3) ? fpc1020->huge_buffer[3] : 0,
        (reg_data->reg_size > 4) ? fpc1020->huge_buffer[4] : 0,
        (reg_data->reg_size > 5) ? fpc1020->huge_buffer[5] : 0,
        (reg_data->reg_size > 6) ? fpc1020->huge_buffer[6] : 0,
        (reg_data->reg_size > 7) ? fpc1020->huge_buffer[7] : 0);
*/
out:
	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_cmd(fpc1020_data_t * fpc1020, fpc1020_cmd_t cmd, u8 wait_irq_mask)
{
	int error = 0;
	struct spi_message msg;
	unsigned int spi_failed_times = 0;

	struct spi_transfer t = {
		.cs_change = 1,
		.delay_usecs = 0,
		.speed_hz = FPC1020_SPI_CLOCK_SPEED,
		.tx_buf = &cmd,
		.rx_buf = NULL,
		.len = 1,
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 0,
	};
#if CS_CONTROL
	gpio_set_value(fpc1020->cs_gpio, 0);
#endif
	spi_message_init(&msg);
	spi_message_add_tail(&t, &msg);

	do {
		error = spi_sync(fpc1020->spi, &msg);

		if (0 == error) {
			break;
		} else {
			dev_err(&fpc1020->spi->dev,
				"spi_sync failed error=%d.\n", error);
			msleep(100);
			spi_failed_times++;
		}
	} while (spi_failed_times < SPI_MAX_TRY_TIMES);

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"%s %d: sync error reached MAX FAILed times\n",
			__func__, __LINE__);
#ifdef CONFIG_HUAWEI_DSM
		send_msg_to_dsm(fpc1020->dsm_fingerprint_client, error,
				DSM_FINGERPRINT_SPISYNC_ERROR_NO);
#endif
		error = -ECOMM;
		goto out;
	}
	if ((error >= 0) && wait_irq_mask) {
		error = fpc1020_wait_for_irq(fpc1020,
					     FPC1020_IRQ_TIMEOUT_AFTER_CMD_MS);

		if (error >= 0)
			error = fpc1020_read_irq(fpc1020, true);
		else {
#ifdef CONFIG_HUAWEI_DSM
			if (!dsm_client_ocuppy(fpc1020->dsm_fingerprint_client)) {
				dsm_client_record
				    (fpc1020->dsm_fingerprint_client,
				     "wait for irq after cmd error=%d\n",
				     error);
				dsm_client_notify
				    (fpc1020->dsm_fingerprint_client,
				     DSM_FINGERPRINT_IRQ_AFTER_CMD_ERROR_NO);
			}
#endif
		}
	}
#if CS_CONTROL
	gpio_set_value(fpc1020->cs_gpio, 1);
#endif
	// dev_dbg(&fpc1020->spi->dev, "%s 0x%x/%dd\n", __func__, cmd, cmd);
out:
	return error;
}

int fpc1020_dummy_capture(fpc1020_data_t * fpc1020)
{
	int error = 0;

	error = fpc1020_read_irq(fpc1020, true);
	if (error < 0)
		return error;
	error = fpc1020_cmd(fpc1020,
			    FPC1020_CMD_FINGER_PRESENT_QUERY,
			    FPC_1020_IRQ_REG_BIT_COMMAND_DONE);
	if (error < 0)
		return error;
	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_wait_finger_present(fpc1020_data_t * fpc1020)
{
	int error = 0;
	int wait_idle_ms = 20;
	int try_time = 2000 / wait_idle_ms;
	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	error = fpc1020_dummy_capture(fpc1020);
	if (error < 0)
		return error;

	if (fpc1020_sleep(fpc1020, false) < 0) {
		fpc1020_wake_up(fpc1020);
		fpc1020_write_sensor_setup(fpc1020);
		error = fpc1020_dummy_capture(fpc1020);

		if (error < 0)
			return error;
		error = fpc1020_sleep(fpc1020, false);

		if (error < 0)
			return error;
	}

	while (1) {
		error = fpc1020_wait_for_irq(fpc1020,
					     FPC1020_DEFAULT_IRQ_TIMEOUT_MS);

		if (fpc1020->worker.stop_request) {
			dev_dbg(&fpc1020->spi->dev, "%s stop Request \n",
				__func__);
			return -EINTR;
		}

		if (error < 0) {
			if (error == -ETIMEDOUT) {
				continue;
			}
			dev_err(&fpc1020->spi->dev,
				"%s fpc1020_wait_for_irq error =%d \n",
				__func__, error);
			return error;
		}

		if (error >= 0) {
			wake_lock(&fpc1020->fp_wake_lock);
			error = -1;
			while (error < 0 && try_time >= 0) {
				msleep(wait_idle_ms);
				if (fp_SPI_RESUME ==
				    atomic_read(&fpc1020->spistate)) {
					msleep(wait_idle_ms);
					error = fpc1020_wake_up(fpc1020);
				}
				if (fpc1020->worker.stop_request) {
					dev_dbg(&fpc1020->spi->dev,
						"%s stop Request \n", __func__);

					error = -EINTR;
					goto err_wake;
				}
				try_time--;
				if (try_time < 0) {
					error = -ETIME;
					goto err_wake;
				}
			}

			fpc1020_write_sensor_setup(fpc1020);
err_wake:
			wake_unlock(&fpc1020->fp_wake_lock);
			wake_lock_timeout(&fpc1020->fp_wake_lock,
					  FPC1020_DEFAULT_WAKELOCK_TIME_S);
			return error;
		}
	}
}

/* -------------------------------------------------------------------- */
int fpc1020_check_finger_present_raw(fpc1020_data_t * fpc1020)
{
	fpc1020_reg_access_t reg;
	u16 temp_u16;
	int error = 0;

	error = fpc1020_read_irq(fpc1020, true);

	if (error < 0)
		return error;

	error = fpc1020_cmd(fpc1020,
			    FPC1020_CMD_FINGER_PRESENT_QUERY,
			    FPC_1020_IRQ_REG_BIT_COMMAND_DONE);

	if (error < 0)
		return error;

	FPC1020_MK_REG_READ(reg, FPC102X_REG_FINGER_PRESENT_STATUS, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);

	if (error) {
		dev_dbg(&fpc1020->spi->dev, "%s error = 0x%x\n", __func__,
			error);
		return error;
	}
//  dev_dbg(&fpc1020->spi->dev, "%s zonedata = 0x%x\n", __func__, temp_u16);

	return temp_u16;
}

/* -------------------------------------------------------------------- */
int fpc1020_check_finger_present_sum(fpc1020_data_t * fpc1020)
{
	int zones = 0;
	u16 mask = FPC1020_FINGER_DETECT_ZONE_MASK;
	u8 count = 0;

	zones = fpc1020_check_finger_present_raw(fpc1020);

	if (zones < 0)
		return zones;
	else {
		zones &= mask;
		while (zones && mask) {
			count += (zones & 1) ? 1 : 0;
			zones >>= 1;
			mask >>= 1;
		}
		// dev_dbg(&fpc1020->spi->dev, "%s %d zones\n", __func__, count);
		return (int)count;
	}
}

/* -------------------------------------------------------------------- */
int fpc1020_wake_up(fpc1020_data_t * fpc1020)
{
	const fpc1020_status_reg_t status_mask = FPC1020_STATUS_REG_MODE_MASK;

	int power = 0;
	int reset = 0;
	int status = 0;

	reset = fpc1020_reset(fpc1020);
	status = fpc1020_read_status_reg(fpc1020);
	if (power == 0 && reset == 0 && status >= 0 &&
	    (fpc1020_status_reg_t) (status & status_mask) ==
	    FPC1020_STATUS_REG_IN_IDLE_MODE) {

		dev_dbg(&fpc1020->spi->dev, "%s OK\n", __func__);

		return 0;
	} else {

		dev_err(&fpc1020->spi->dev, "%s FAILED -%d\n", __func__, EIO);

		return -EIO;
	}
}

int fpc1020_sleep(fpc1020_data_t * fpc1020, bool deep_sleep)
{
	const char *str_deep = "deep";
	const char *str_regular = "regular";
	bool sleep_ok;
	int retries = FPC1020_SLEEP_RETRIES;
	const fpc1020_status_reg_t status_mask = FPC1020_STATUS_REG_MODE_MASK;

	int error = 0;

	error = fpc1020_cmd(fpc1020,
			    (deep_sleep) ? FPC1020_CMD_ACTIVATE_DEEP_SLEEP_MODE
			    : FPC1020_CMD_ACTIVATE_SLEEP_MODE, 0);

	if (error) {
		dev_dbg(&fpc1020->spi->dev,
			"%s %s command failed %d\n", __func__,
			(deep_sleep) ? str_deep : str_regular, error);

		return error;
	}

	error = 0;
	sleep_ok = false;

	while (!sleep_ok && retries && (error >= 0)) {

		error = fpc1020_read_status_reg(fpc1020);

		if (error < 0) {
			dev_dbg(&fpc1020->spi->dev,
				"%s %s read status failed %d\n", __func__,
				(deep_sleep) ? str_deep : str_regular, error);
		} else {
			error &= status_mask;
			sleep_ok = (deep_sleep) ?
			    error == FPC1020_STATUS_REG_IN_DEEP_SLEEP_MODE :
			    error == FPC1020_STATUS_REG_IN_SLEEP_MODE;
		}
		if (!sleep_ok) {
			udelay(FPC1020_SLEEP_RETRY_TIME_US);
			retries--;
		}
	}

#if CS_CONTROL
	if (deep_sleep && sleep_ok && gpio_is_valid(fpc1020->cs_gpio))
		gpio_set_value(fpc1020->cs_gpio, 0);
#endif
	/* Optional: Also disable power supplies in sleep */

	if (sleep_ok) {
		dev_dbg(&fpc1020->spi->dev,
			"%s %s OK\n", __func__,
			(deep_sleep) ? str_deep : str_regular);
		return 0;
	} else {
		dev_err(&fpc1020->spi->dev,
			"%s %s FAILED\n", __func__,
			(deep_sleep) ? str_deep : str_regular);
		return (deep_sleep) ? -EIO : -EAGAIN;
	}
}

/* -------------------------------------------------------------------- */
int fpc1020_fetch_image(fpc1020_data_t * fpc1020,
			u8 * buffer,
			int offset, u32 image_size_bytes, u32 buff_size)
{
	int error = 0;
	struct spi_message msg;
	const u8 tx_data[2] = { FPC1020_CMD_READ_IMAGE, 0 };
	unsigned int spi_failed_times = 0;

	dev_dbg(&fpc1020->spi->dev,
		"%s (+%d, buff_size=%d , image_byte_size =%d memory)\n",
		__func__, offset, buff_size, image_size_bytes);

	if ((offset + (int)image_size_bytes) > buff_size) {
		dev_err(&fpc1020->spi->dev,
			"Image buffer too small for offset +%d (max %d bytes)",
			offset, buff_size);

		error = -ENOBUFS;
	}

	if (!error) {
		struct spi_transfer cmd = {
			.cs_change = 0,
			.delay_usecs = 0,
			.speed_hz = FPC1020_SPI_CLOCK_SPEED,
			.tx_buf = tx_data,
			.rx_buf = NULL,
			.len = 2,
			.tx_dma = 0,
			.rx_dma = 0,
			.bits_per_word = 0,
		};

		struct spi_transfer data = {
			.cs_change = 1,
			.delay_usecs = 0,
			.speed_hz = FPC1020_SPI_CLOCK_SPEED,
			.tx_buf = NULL,
			.rx_buf = fpc1020->single_buff,
			.len = (int)image_size_bytes,
			.tx_dma = 0,
			.rx_dma = 0,
			.bits_per_word = 0,
		};
#if CS_CONTROL
		gpio_set_value(fpc1020->cs_gpio, 0);
#endif
		spi_message_init(&msg);
		spi_message_add_tail(&cmd, &msg);
		spi_message_add_tail(&data, &msg);

		do {
			error = spi_sync(fpc1020->spi, &msg);

			if (0 == error) {
				break;
			} else {
				dev_err(&fpc1020->spi->dev,
					"spi_sync failed error=%d.\n", error);
				msleep(100);
				spi_failed_times++;
			}
		} while (spi_failed_times < SPI_MAX_TRY_TIMES);
		if (error) {
			dev_err(&fpc1020->spi->dev,
				"%s %d: sync error reached MAX FAILed times\n",
				__func__, __LINE__);
#ifdef CONFIG_HUAWEI_DSM
			send_msg_to_dsm(fpc1020->dsm_fingerprint_client, error,
					DSM_FINGERPRINT_SPISYNC_ERROR_NO);
#endif
			error = -ECOMM;
			goto out;
		}

		memmove(buffer + offset, fpc1020->single_buff,
			image_size_bytes);

#if CS_CONTROL
		gpio_set_value(fpc1020->cs_gpio, 1);
#endif
	}

	error = fpc1020_read_irq(fpc1020, true);

	if (error > 0)
		error = (error & FPC_1020_IRQ_REG_BIT_ERROR) ? -EIO : 0;

out:
	return error;
}

/* -------------------------------------------------------------------- */
bool fpc1020_check_in_range_u64(u64 val, u64 min, u64 max)
{
	return (val >= min) && (val <= max);
}

/* -------------------------------------------------------------------- */
u32 fpc1020_calc_image_size(fpc1020_data_t * fpc1020)
{
	unsigned int image_byte_size = fpc1020->setup.capture_row_count *
	    fpc1020->setup.capture_col_groups * FPC1020_ADC_GROUP;

	dev_dbg(&fpc1020->spi->dev, "%s Rows %d->%d,Cols %d->%d (%d bytes)\n",
		__func__,
		fpc1020->setup.capture_row_start,
		fpc1020->setup.capture_row_start
		+ fpc1020->setup.capture_row_count - 1,
		fpc1020->setup.capture_col_start
		* FPC1020_ADC_GROUP,
		(fpc1020->setup.capture_col_start * FPC1020_ADC_GROUP)
		+ (fpc1020->setup.capture_col_groups *
		   FPC1020_ADC_GROUP) - 1, image_byte_size);

	return image_byte_size;
}

/* -------------------------------------------------------------------- */

size_t fpc1020_calc_image_size_selftest(fpc1020_data_t * fpc1020)
{

	unsigned int image_byte_size;

	switch (fpc1020->chip.type) {
	case FPC1020_CHIP_1020A:
		image_byte_size = (192 * 192);
		break;
	case FPC1020_CHIP_1021A:
	case FPC1020_CHIP_1021B:
		image_byte_size = (160 * 160);
		break;
	case FPC1020_CHIP_1150A:
		image_byte_size = (80 * 208);
		break;
	case FPC1020_CHIP_1140B:
		image_byte_size = (56 * 192);
		break;
	default:
		image_byte_size = (192 * 192);
		break;
	}
	return image_byte_size;
}

   /* -------------------------------------------------------------------- */
