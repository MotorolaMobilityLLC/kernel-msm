/*
 * Copyright (c) 2018 Motorola Mobility, LLC.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <linux/string.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/pinctrl/consumer.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/firmware.h>

#define CHIP_ID_REG		0x0000
#define HW_VER_REG		0x0002
#define CUST_ID_REG		0x0003
#define OTP_FW_VER_REG		0x0004
#define E2PROM_FW_VER_REG	0x001c
#define DEV_STATUS_REG		0x0034
#define IRQ_STATUS_REG		0x0036
#define IRQ_ENABLE_REG		0x0038
#define CHG_ST_MSG_REG		0x003a
#define EPT_MSG_REG		0x003b
#define VOUT_ADC_REG		0x003c
#define VOUT_SET_REG		0x003e
#define VRECT_ADJ_REG		0x003f
#define VRECT_ADC_REG		0x0040
#define IOUT_REG		0x0044
#define DIE_TEMP_REG		0x0046
#define OPT_FREQ_REG		0x0048
#define ILIMIT_SET_REG		0x004a
#define CMD_1_REG		0x004c
#define SYS_OPT_MODE_REG	0x004d
#define CMD_2_REG		0x004e
#define PPP_HEADER_REG		0x0050
#define PPP_DATA_1_REG		0x0051
#define PPP_DATA_2_REG		0x0052
#define PPP_DATA_3_REG		0x0053
#define PPP_DATA_4_REG		0x0054
#define PPP_DATA_5_REG		0x0055
#define IRQ_CLEAR_REG		0x0056
#define BC_HEADER_REG		0x0058
#define BC_DATA_1_REG		0x0059
#define BC_DATA_2_REG		0x005a
#define BC_DATA_3_REG		0x005b
#define BC_DATA_4_REG		0x005c
#define BC_DATA_5_REG		0x005d
#define OVP_CFG_REG		0x0062
#define FOD_CFG_REG		0x0068
#define FASTCHG_V_REG		0x0078

#define ST_ID_AUTH_SUCCESS	BIT(13)
#define ST_ID_AUTH_FAILED	BIT(12)
#define ST_SEND_PKT_SUCCESS	BIT(11)
#define ST_SEND_PKT_TIMEOUT	BIT(10)
#define ST_DEV_AUTH_SUCCESS	BIT(9)
#define ST_DEV_AUTH_FAILED	BIT(8)
#define ST_LDO_OFF		BIT(7)
#define ST_LDO_ON		BIT(6)
#define ST_MODE_CHANGE		BIT(5)
#define ST_TX_DATA_RCVD		BIT(4)
#define ST_RX_READY		BIT(3)
#define ST_OVER_TEMP		BIT(2)
#define ST_OVER_VOLT		BIT(1)
#define ST_OVER_CURR		BIT(0)

#define CMD_2_FAST_CHG		BIT(7)
#define CMD_2_SWITCH_RAM	BIT(6)
#define CMD_2_CLR_IRQ		BIT(5)
#define CMD_2_SEND_CSP		BIT(4)
#define CMD_2_SEND_EPT		BIT(3)
#define CMD_2_SEND_AUTH		BIT(2)
#define CMD_2_TOGGLE_LDO	BIT(1)
#define CMD_2_SEND_RX_DATA	BIT(0)

#define PPP_PKG_HDR18		0x18
#define PPP_PKG_HDR28		0x28
#define PPP_PKG_HDR38		0x38
#define PPP_PKG_HDR48		0x48
#define PPP_PKG_HDR58		0x58

#define BC_CMD_NONE		0x00
#define BC_CMD_SET_FREQ		0x03
#define BC_CMD_GET_FREQ		0x04
#define BC_CMD_READ_FW_VER	0x05
#define BC_CMD_READ_IIN		0x06
#define BC_CMD_READ_VIN		0x07
#define BC_CMD_SET_VIN		0x0a
#define BC_CMD_CHGR_TYPE	0x0b
#define BC_CMD_RESET		0x0c
#define BC_CMD_READ_I2C		0x0d
#define BC_CMD_WRITE_I2C	0x0e
#define BC_CMD_VI2C_INIT	0x10

#define WAIT_FOR_AUTH_MS 1000
#define WAIT_FOR_RCVD_TIMEOUT_MS 1000
#define HEARTBEAT_INTERVAL_MS 60000

#define MAX_VOUT_MV_DEFAULT 5000
#define MAX_IOUT_MA_DEFAULT 500
#define MAX_VOUT_MV_NORM 5000
#define MAX_IOUT_MA_NORM 1000
#define MAX_VOUT_MV_FAST 9000
#define MAX_IOUT_MA_FAST 1100

#define WLS_SHOW_MAX_SIZE 32
#define CHIP_ID_ADDR0 0x5870
#define CHIP_ID_ADDR1 0x5874
#define FOD_ARRAY_LEN 12


#define p922x_err(chip, fmt, ...)		\
	pr_err("%s: %s: " fmt, chip->name,	\
		__func__, ##__VA_ARGS__)	\

#define p922x_dbg(chip, reason, fmt, ...)			\
	do {							\
		if (*chip->debug_mask & (reason))		\
			pr_info("%s: %s: " fmt, chip->name,	\
				__func__, ##__VA_ARGS__);	\
		else						\
			pr_debug("%s: %s: " fmt, chip->name,	\
				__func__, ##__VA_ARGS__);	\
	} while (0)

static char otp_bootloader[] = {
	0x00, 0x04, 0x00, 0x20, 0x35, 0x01, 0x00, 0x00,
	0x41, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFE, 0xE7, 0x00, 0x00, 0x80, 0x00, 0x00, 0xE0,
	0x00, 0xBF, 0x40, 0x1E, 0xFC, 0xD2, 0x70, 0x47,
	0x00, 0xB5, 0x60, 0x4A, 0x60, 0x4B, 0x01, 0x70,
	0x01, 0x20, 0xFF, 0xF7, 0xF3, 0xFF, 0x52, 0x1E,
	0x02, 0xD0, 0x18, 0x8B, 0x00, 0x06, 0xF7, 0xD4,
	0x00, 0xBD, 0xF7, 0xB5, 0x05, 0x46, 0x5B, 0x48,
	0x81, 0xB0, 0x00, 0x21, 0x94, 0x46, 0x81, 0x81,
	0x57, 0x48, 0x31, 0x21, 0x01, 0x80, 0x04, 0x21,
	0x81, 0x80, 0x06, 0x21, 0x01, 0x82, 0x28, 0x20,
	0xFF, 0xF7, 0xDC, 0xFF, 0x00, 0x24, 0x0D, 0xE0,
	0x02, 0x99, 0x28, 0x5D, 0x09, 0x5D, 0x02, 0x46,
	0x8A, 0x43, 0x01, 0xD0, 0x10, 0x20, 0x3F, 0xE0,
	0x81, 0x43, 0x02, 0xD0, 0x28, 0x19, 0xFF, 0xF7,
	0xD3, 0xFF, 0x64, 0x1C, 0x64, 0x45, 0xEF, 0xD3,
	0x49, 0x48, 0x36, 0x21, 0x01, 0x82, 0x00, 0x24,
	0x2F, 0xE0, 0x02, 0x98, 0x00, 0x27, 0x06, 0x5D,
	0x28, 0x19, 0x00, 0x90, 0x44, 0x4A, 0x08, 0x20,
	0x90, 0x80, 0x02, 0x20, 0xFF, 0xF7, 0xBA, 0xFF,
	0x28, 0x5D, 0x33, 0x46, 0x83, 0x43, 0x16, 0xD0,
	0x3F, 0x49, 0x04, 0x20, 0x88, 0x80, 0x02, 0x20,
	0xFF, 0xF7, 0xB0, 0xFF, 0x19, 0x46, 0x00, 0x98,
	0xFF, 0xF7, 0xB2, 0xFF, 0x3A, 0x49, 0x0F, 0x20,
	0x88, 0x80, 0x02, 0x20, 0xFF, 0xF7, 0xA6, 0xFF,
	0x28, 0x5D, 0xB0, 0x42, 0x03, 0xD0, 0x7F, 0x1C,
	0x0A, 0x2F, 0xDF, 0xD3, 0x01, 0xE0, 0x0A, 0x2F,
	0x06, 0xD3, 0x35, 0x48, 0x29, 0x19, 0x41, 0x80,
	0x29, 0x5D, 0xC1, 0x80, 0x04, 0x20, 0x03, 0xE0,
	0x64, 0x1C, 0x64, 0x45, 0xCD, 0xD3, 0x02, 0x20,
	0x2D, 0x49, 0x11, 0x22, 0x0A, 0x80, 0x04, 0x22,
	0x8A, 0x80, 0x2C, 0x49, 0xFF, 0x22, 0x8A, 0x81,
	0x04, 0xB0, 0xF0, 0xBD, 0x2C, 0x49, 0x2B, 0x48,
	0x08, 0x60, 0x2C, 0x48, 0x00, 0x23, 0x83, 0x81,
	0x2A, 0x49, 0x20, 0x39, 0x8B, 0x83, 0x03, 0x80,
	0x24, 0x48, 0x5A, 0x21, 0x40, 0x38, 0x01, 0x80,
	0x81, 0x15, 0x81, 0x80, 0x0B, 0x21, 0x01, 0x81,
	0x25, 0x49, 0x81, 0x81, 0x14, 0x20, 0xFF, 0xF7,
	0x71, 0xFF, 0x24, 0x4A, 0x01, 0x20, 0x10, 0x80,
	0x02, 0x20, 0xFF, 0xF7, 0x6B, 0xFF, 0x8D, 0x20,
	0x10, 0x80, 0x93, 0x80, 0xFF, 0x20, 0x90, 0x82,
	0x03, 0x20, 0x00, 0x02, 0x10, 0x82, 0xFC, 0x20,
	0x90, 0x83, 0x1C, 0x49, 0x95, 0x20, 0x20, 0x31,
	0x08, 0x80, 0x15, 0x4D, 0x2B, 0x80, 0x28, 0x88,
	0x2C, 0x46, 0x01, 0x28, 0xFB, 0xD1, 0x61, 0x88,
	0x80, 0x03, 0xA2, 0x88, 0x08, 0x18, 0x51, 0x18,
	0x8B, 0xB2, 0x00, 0x21, 0x04, 0xE0, 0x0E, 0x19,
	0x36, 0x7A, 0xF3, 0x18, 0x9B, 0xB2, 0x49, 0x1C,
	0x8A, 0x42, 0xF8, 0xD8, 0xE1, 0x88, 0x99, 0x42,
	0x01, 0xD0, 0x08, 0x20, 0x08, 0xE0, 0x00, 0x2A,
	0x05, 0xD0, 0x07, 0x49, 0x08, 0x31, 0xFF, 0xF7,
	0x50, 0xFF, 0x20, 0x80, 0xDF, 0xE7, 0x02, 0x20,
	0x28, 0x80, 0xDC, 0xE7, 0x10, 0x27, 0x00, 0x00,
	0x00, 0x5C, 0x00, 0x40, 0x40, 0x30, 0x00, 0x40,
	0x00, 0x04, 0x00, 0x20, 0xFF, 0x0F, 0x00, 0x00,
	0x80, 0xE1, 0x00, 0xE0, 0x20, 0x6C, 0x00, 0x40,
	0x04, 0x1D, 0x00, 0x00, 0x00, 0x64, 0x00, 0x40,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

enum {
	PROGRAM_FW_NONE,
	PROGRAM_FW_PENDING,
	PROGRAM_FW_SUCCESS,
	PROGRAM_FW_FAIL,
};

enum print_reason {
	PR_INTERRUPT    = BIT(0),
	PR_MISC         = BIT(2),
	PR_MOTO         = BIT(7),
};

static int __debug_mask = PR_MOTO | PR_INTERRUPT;
module_param_named(
	debug_mask, __debug_mask, int, S_IRUSR | S_IWUSR
);

enum {
	TX_CHGR_TYPE_UNKNOWN	= 0x00,
	TX_CHGR_TYPE_SDP	= 0x01,
	TX_CHGR_TYPE_CDP	= 0x02,
	TX_CHGR_TYPE_DCP	= 0x03,
	TX_CHGR_TYPE_QC20	= 0x05,
	TX_CHGR_TYPE_QC30	= 0x06,
	TX_CHGR_TYPE_PD		= 0x07,
	TX_CHGR_TYPE_MAX	= TX_CHGR_TYPE_PD,
};

enum {
	TX_AUTH_STAT_NONE,
	TX_AUTH_STAT_ID_FAILED,
	TX_AUTH_STAT_ID_PASSED,
	TX_AUTH_STAT_DEV_FAILED,
	TX_AUTH_STAT_DEV_PASSED,
};

struct battery_st {
	int status;
	int capacity;
	int voltage_now;
	int voltage_max;
	int current_now;
	int current_max;
	int charge_type;
	int temp;
};

struct p922x_pkg_t {
	u8 header;
	u8 cmd;
	u8 data[5];
};

struct p922x_charger {
	const char		*name;
	int			*debug_mask;
	struct i2c_client	*client;
	struct device		*dev;
	struct regmap		*regmap;

	bool			rx_chip_det;
	bool			tx_auth_enabled;
	u32			chg_efficiency;
	u32			chg_limit_soc;
	u32			chg_idle_cl;
	struct regulator	*vdd_i2c_vreg;
	struct gpio		irq_gpio;
	struct pinctrl		*pinctrl_irq;
	const char		*pinctrl_name;

	u16			irq_en;
	u16			irq_stat;
	u16			stat;
	u16			rx_vout_max;
	u16			rx_iout_max;
	u16			wls_allowed_vmax;
	u16			wls_allowed_imax;
	bool			ldo_enabled;
	bool			tx_connected;
	bool			wired_connected;
	bool			charging_disabled;
	bool			rx_ready;
	int			tx_auth_st;
	int			tx_chg_type;
	int			temp_level;
	int			thermal_levels;
	struct battery_st	batt_st;

	struct mutex            send_pkg_lock;
	struct completion	rcvd_completion;

	struct delayed_work	tx_auth_done_work;
	struct work_struct	chg_det_work;
	struct delayed_work	heartbeat_work;

	u32			peek_poke_address;
	struct dentry		*debug_root;

	struct power_supply	*consumer_psy;
	struct power_supply	*batt_psy;
	struct power_supply	*usb_psy;
	struct power_supply	*main_psy;
	struct power_supply     *wls_psy;
	struct power_supply_desc	wls_psy_d;

	const char		*fw_name;
	bool			program_fw_enabled;
	int			program_fw_stat;
	u8			fod_array_5v[FOD_ARRAY_LEN];
	u8			fod_array_9v[FOD_ARRAY_LEN];
	u32			fod_array_5v_len;
	u32			fod_array_9v_len;
};

static const char * const type_text[] = {
	"UNKNOWN",
	"SDP",
	"CDP",
	"DCP",
	"QC2.0",
	"QC3.0",
	"PD",
};

static char *pm_wls_supplied_to[] = {
	"wireless",
};

static char *pm_wls_supplied_from[] = {
	"battery",
	"usb",
	"dc",
};

static int p922x_read_reg(struct p922x_charger *chip, u16 reg, u8 *val)
{
	int rc;
	unsigned int temp;

	rc = regmap_read(chip->regmap, reg, &temp);
	if (rc < 0)
		p922x_err(chip, "Failed to read reg=0x%x, rc=%d\n", reg, rc);
	else
		*val = (u8)temp;

	return rc;
}

static int p922x_write_reg(struct p922x_charger *chip, u16 reg, u8 val)
{
	int rc;

	rc = regmap_write(chip->regmap, reg, val);
	if (rc < 0)
		p922x_err(chip, "Failed to write reg=0x%x, rc=%d\n", reg, rc);

	return rc;
}

static int p922x_read_buffer(struct p922x_charger *chip, u16 reg,
		u8 *buf, u32 size)
{
	int rc;

	rc = regmap_bulk_read(chip->regmap, reg, buf, size);
	if (rc < 0)
		p922x_err(chip, "Failed to read buffer on reg=0x%x, rc=%d\n",
					reg, rc);

	return rc;
}

static int p922x_write_buffer(struct p922x_charger *chip, u16 reg,
		u8 *buf, u32 size)
{
	int rc = 0;

	while (size--) {
		rc = regmap_write(chip->regmap, reg++, *buf++);
		if (rc < 0)
			break;
	}

	if (rc < 0)
		p922x_err(chip, "Failed to write buffer on reg=0x%x, rc=%d\n",
					reg, rc);

	return rc;
}

static int p922x_get_package_size(u8 hdr)
{
	if (hdr < 0x20)
		return 1;
	else if (hdr < 0x80)
		return (2 + ((hdr - 0x20) >> 4));
	else if (hdr < 0xe0)
		return (8 + ((hdr - 0x80) >> 3));
	else
		return (20 + ((hdr - 0xe0) >> 2));
}

static int p922x_send_package(struct p922x_charger *chip,
		struct p922x_pkg_t *pkg)
{
	int rc;
	int length = p922x_get_package_size(pkg->header) + 1;

	if (chip->tx_auth_st != TX_AUTH_STAT_DEV_PASSED) {
		p922x_err(chip, "Prevent sending ppp packet in auth stat %d\n",
					chip->tx_auth_st);
		return -EOPNOTSUPP;
	}

	/* write data into proprietary packet buffer */
	rc = p922x_write_buffer(chip, PPP_HEADER_REG, (u8 *)pkg, length);
	if (rc < 0)
		return rc;

	/* send proprietary packet */
	rc = p922x_write_reg(chip, CMD_2_REG,  CMD_2_SEND_RX_DATA);

	return rc;
}

static int p922x_receive_package(struct p922x_charger *chip, u8 *data)
{
	int rc;
	long t;
	u8 header;
	u8 length;

	t = wait_for_completion_interruptible_timeout(&chip->rcvd_completion,
				msecs_to_jiffies(WAIT_FOR_RCVD_TIMEOUT_MS));
	if (t > 0) {
		rc = p922x_read_reg(chip, BC_HEADER_REG, &header);
		if (rc < 0) {
			p922x_err(chip, "Failed to get header, rc = %d\n", rc);
			return rc;
		}

		length = p922x_get_package_size(header);
		rc = p922x_read_buffer(chip, BC_DATA_1_REG, data, length);
		if (rc < 0) {
			p922x_err(chip, "Failed to get data, rc = %d\n", rc);
			return rc;
		}
	} else if (t == 0) {
		rc = -ETIMEDOUT;
		p922x_err(chip, "Wait for rvcd timeout\n");
	} else
		rc = t;

	return rc;
}

static int p922x_get_tx_chgr_type(struct p922x_charger *chip)
{
	int rc;
	struct p922x_pkg_t pkg;
	u8 data[16] = {0};
	int type = TX_CHGR_TYPE_UNKNOWN;

	pkg.header = PPP_PKG_HDR18;
	pkg.cmd = BC_CMD_CHGR_TYPE;
	mutex_lock(&chip->send_pkg_lock);
	rc = p922x_send_package(chip, &pkg);
	if (rc < 0) {
		p922x_err(chip, "Failed to send package, rc = %d\n", rc);
		goto unlock;
	}

	rc = p922x_receive_package(chip, data);
	if (rc < 0) {
		p922x_err(chip, "Failed to receive package, rc = %d\n", rc);
		goto unlock;
	}
	if (data[1] > TX_CHGR_TYPE_MAX) {
		p922x_err(chip, "Receive invalid charger type = %d\n", data[1]);
		goto unlock;
	} else {
		type = (int)data[1];
		if (type > TX_CHGR_TYPE_MAX)
			type = TX_CHGR_TYPE_UNKNOWN;
	}

unlock:
	mutex_unlock(&chip->send_pkg_lock);

	return type;
}

static int p922x_set_fastchg_voltage(struct p922x_charger *chip, u16 mv)
{
	int rc;

	if (chip->tx_auth_st != TX_AUTH_STAT_DEV_PASSED) {
		p922x_err(chip, "Prevent fastchg in auth stat %d\n",
					chip->tx_auth_st);
		return -EOPNOTSUPP;
	}

	rc = p922x_write_buffer(chip, FASTCHG_V_REG, (u8 *)&mv, 2);
	if (rc < 0) {
		p922x_err(chip, "Failed to set fastchg voltage, rc = %d\n", rc);
		return rc;
	}

	rc = p922x_write_reg(chip, CMD_2_REG, CMD_2_FAST_CHG);
	if (rc < 0) {
		p922x_err(chip, "Failed to set cmd reg, rc = %d\n", rc);
		return rc;
	}
	chip->rx_vout_max = mv;
	p922x_dbg(chip, PR_MISC, "Set fast charging voltage: %dmV\n", mv);

	if (chip->fod_array_9v_len == FOD_ARRAY_LEN &&
		mv == MAX_VOUT_MV_FAST) {
		p922x_write_buffer(chip, FOD_CFG_REG,
				chip->fod_array_9v, FOD_ARRAY_LEN);
	}

	return rc;
}

static int p922x_get_rx_vout(struct p922x_charger *chip)
{
	int rc;
	u16 volt;

	if (!chip->ldo_enabled)
		return 0;

	rc = p922x_read_buffer(chip, VOUT_ADC_REG, (u8 *)&volt, 2);
	if (rc < 0) {
		p922x_err(chip, "Failed to read rx voltage, rc = %d\n", rc);
		return rc;
	}

	return (volt & 0x0FFF) * 6 * 2100 / 4095;
}

static int p922x_set_rx_vout(struct p922x_charger *chip, u16 mv)
{
	int rc;
	u16 rx_vout_max = mv;

	if (mv < 3500)
		mv = 3500;
	else if (mv > 12500)
		mv = 12500;

	mv = (mv - 3500) / 100;
	rc = p922x_write_reg(chip, VOUT_SET_REG, (u8)mv);
	if (rc < 0)
		p922x_err(chip, "Failed to set rx voltage, rc = %d\n", rc);
	else
		chip->rx_vout_max = rx_vout_max;

	return rc;
}

static int p922x_get_rx_iout(struct p922x_charger *chip)
{
	int rc;
	u16 ma;

	if (!chip->ldo_enabled)
		return 0;

	rc = p922x_read_buffer(chip, IOUT_REG, (u8 *)&ma, 2);
	if (rc < 0) {
		p922x_err(chip, "Failed to read rx current, rc = %d\n", rc);
		return rc;
	}

	return ma;
}

static int p922x_get_rx_ocl(struct p922x_charger *chip)
{
	int rc;
	u16 ma;

	rc = p922x_read_buffer(chip, ILIMIT_SET_REG, (u8 *)&ma, 2);
	if (rc < 0) {
		p922x_err(chip, "Failed to read rx current, rc = %d\n", rc);
		return rc;
	}

	return (ma & 0xf) * 100 + 100;
}

static int p922x_get_rx_chip_id(struct p922x_charger *chip, u16 *id)
{
	int rc;
	u8 data;

	rc = p922x_read_reg(chip, CHIP_ID_ADDR0, &data);
	if (rc < 0) {
		p922x_err(chip, "Read CHIP_ID_ADDR0 failed, rc=%d\n", rc);
		return rc;
	}
	*id = data << 8;

	rc = p922x_read_reg(chip, CHIP_ID_ADDR1, &data);
	if (rc < 0) {
		p922x_err(chip, "Read CHIP_ID_ADDR1 failed, rc=%d\n", rc);
		return rc;
	}
	*id = *id | data;

	return rc;
}

static int p922x_get_tx_vin(struct p922x_charger *chip)
{
	int rc;
	struct p922x_pkg_t pkg;
	u8 data[16] = {0};

	pkg.header = PPP_PKG_HDR18;
	pkg.cmd = BC_CMD_READ_VIN;
	mutex_lock(&chip->send_pkg_lock);
	rc = p922x_send_package(chip, &pkg);
	if (rc < 0) {
		p922x_err(chip, "Failed to send package, rc = %d\n", rc);
		goto unlock;
	}

	rc = p922x_receive_package(chip, data);
	if (rc < 0) {
		p922x_err(chip, "Failed to receive package, rc = %d\n", rc);
		goto unlock;
	}

	rc = data[1] | (data[2] << 8);

unlock:
	mutex_unlock(&chip->send_pkg_lock);
	return rc;
}

static int p922x_set_tx_vin(struct p922x_charger *chip, u16 mv)
{
	int rc;
	struct p922x_pkg_t pkg;

	pkg.header = PPP_PKG_HDR38;
	pkg.cmd = BC_CMD_SET_VIN;
	pkg.data[0] = mv & 0xff;
	pkg.data[1] = (mv >> 8) & 0xff;
	mutex_lock(&chip->send_pkg_lock);
	rc = p922x_send_package(chip, &pkg);
	if (rc < 0)
		p922x_err(chip, "Failed to send package, rc = %d\n", rc);

	mutex_unlock(&chip->send_pkg_lock);

	return rc;
}

static int p922x_get_tx_icl(struct p922x_charger *chip)
{
	int rc;
	struct p922x_pkg_t pkg;
	u8 data[16] = {0};

	pkg.header = PPP_PKG_HDR18;
	pkg.cmd = BC_CMD_READ_IIN;
	mutex_lock(&chip->send_pkg_lock);
	rc = p922x_send_package(chip, &pkg);
	if (rc < 0) {
		p922x_err(chip, "Failed to send package, rc = %d\n", rc);
		goto unlock;
	}

	rc = p922x_receive_package(chip, data);
	if (rc < 0) {
		p922x_err(chip, "Failed to receive package, rc = %d\n", rc);
		goto unlock;
	}

	rc = data[1] | (data[2] << 8);

unlock:
	mutex_unlock(&chip->send_pkg_lock);
	return rc;
}

static int p922x_enable_charging(struct p922x_charger *chip, bool on)
{
	int rc = 0;

	if (chip->ldo_enabled != on) {
		rc = p922x_write_reg(chip, CMD_2_REG,  CMD_2_TOGGLE_LDO);
		if (rc < 0)
			p922x_err(chip, "Failed to %s RX ldo, rc = %d\n",
				on ? "enable":"disable", rc);
		else {
			chip->ldo_enabled = on;
			p922x_dbg(chip, PR_MOTO, "RX ldo is %s\n",
				on ? "enabled":"disabled");
		}
	}

	return rc;
}

static void p922x_get_tx_capability(struct p922x_charger *chip,
		u16 *vmax, u16 *imax)
{
	switch (chip->tx_chg_type) {
	case TX_CHGR_TYPE_UNKNOWN:
		if (!chip->tx_auth_enabled) {
			*vmax = MAX_VOUT_MV_FAST;
			*imax = MAX_IOUT_MA_FAST;
		} else if (chip->tx_auth_st != TX_AUTH_STAT_NONE) {
			*vmax = MAX_VOUT_MV_NORM;
			*imax = MAX_IOUT_MA_NORM;
		} else {
			*vmax = MAX_VOUT_MV_DEFAULT;
			*imax = MAX_IOUT_MA_DEFAULT;
		}
		break;
	case TX_CHGR_TYPE_QC20:
	case TX_CHGR_TYPE_QC30:
		*vmax = MAX_VOUT_MV_FAST;
		*imax = MAX_IOUT_MA_FAST;
		break;
	case TX_CHGR_TYPE_CDP:
	case TX_CHGR_TYPE_DCP:
		*vmax = MAX_VOUT_MV_NORM;
		*imax = MAX_IOUT_MA_NORM;
		break;
	case TX_CHGR_TYPE_SDP:
	default:
		*vmax = MAX_VOUT_MV_DEFAULT;
		*imax = MAX_IOUT_MA_DEFAULT;
		break;
	}
}

static int p922x_request_rx_power(struct p922x_charger *chip,
		int vout, int iout)
{
	int rc = 0;
	u16 vmax, imax;

	if (vout < 0 && iout < 0) {
		p922x_err(chip, "Invalid request power parameters\n");
		return -EINVAL;
	}

	p922x_get_tx_capability(chip, &vmax, &imax);
	if (vout >= 0) {
		if (vmax < vout)
			return -EINVAL;

		rc = p922x_set_fastchg_voltage(chip, vout);
		if (rc == -EOPNOTSUPP && vout < MAX_VOUT_MV_FAST)
			rc = p922x_set_rx_vout(chip, vout);
		if (rc < 0) {
			p922x_err(chip, "Failed to req vout=%d, rc=%d\n",
						vout, rc);
			return rc;
		}
	}

	if (chip->rx_vout_max <= MAX_VOUT_MV_NORM &&
	    (vmax > MAX_VOUT_MV_NORM ||
	    imax > MAX_IOUT_MA_NORM))
		imax = MAX_IOUT_MA_NORM;

	if (chip->rx_iout_max > imax)
		chip->rx_iout_max = imax;

	if (iout >= 0) {
		if (imax < iout)
			return -EINVAL;

		chip->rx_iout_max = iout;
	}

	p922x_dbg(chip, PR_MOTO, "Request RX power, V=%dmV, I=%dmA\n",
				chip->rx_vout_max, chip->rx_iout_max);
	return rc;
}

static int p922x_update_supplies_status(struct p922x_charger *chip)
{
	int rc;
	union power_supply_propval prop = {0,};

	if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		pr_debug("USB psy not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &prop);
	if (rc) {
		p922x_err(chip, "Couldn't read USB present prop, rc=%d\n", rc);
		return rc;
	} else if (!prop.intval) {
		rc = power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_TYPEC_MODE, &prop);
		if (rc) {
			p922x_err(chip,
				"Couldn't read USB mode prop, rc=%d\n", rc);
			return rc;
		}
	}
	chip->wired_connected = !!prop.intval;

	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");
	if (!chip->batt_psy) {
		pr_debug("Battery psy not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_STATUS, &prop);
	if (rc) {
		p922x_err(chip, "Couldn't read batt status prop, rc=%d\n", rc);
		return rc;
	}
	chip->batt_st.status = prop.intval;

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CAPACITY, &prop);
	if (rc) {
		p922x_err(chip, "Couldn't read batt capacity prop, rc=%d\n",
					rc);
		return rc;
	}
	chip->batt_st.capacity = prop.intval;

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_TEMP, &prop);
	if (rc) {
		p922x_err(chip, "Couldn't read batt temp prop, rc=%d\n",
					rc);
		return rc;
	}
	chip->batt_st.temp = prop.intval;

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
	if (rc) {
		p922x_err(chip, "Couldn't read batt voltage now prop, rc=%d\n",
					rc);
		return rc;
	}
	chip->batt_st.voltage_now = prop.intval / 1000;

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CURRENT_NOW, &prop);
	if (rc) {
		p922x_err(chip, "Couldn't read batt current now prop, rc=%d\n",
					rc);
		return rc;
	}
	chip->batt_st.current_now = prop.intval / 1000;

	rc = power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE, &prop);
	if (rc) {
		p922x_err(chip, "Couldn't read batt charge type prop, rc=%d\n",
					rc);
		return rc;
	}
	chip->batt_st.charge_type = prop.intval;

	if (!chip->main_psy)
		chip->main_psy = power_supply_get_by_name("main");
	if (!chip->main_psy) {
		pr_debug("Main psy not found\n");
		return -EINVAL;
	}
	rc = power_supply_get_property(chip->main_psy,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &prop);
	if (rc) {
		p922x_err(chip, "Couldn't read batt voltage max prop, rc=%d\n",
					rc);
		return rc;
	}
	chip->batt_st.voltage_max = prop.intval / 1000;

	rc = power_supply_get_property(chip->main_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &prop);
	if (rc) {
		p922x_err(chip, "Couldn't read batt current max prop, rc=%d\n",
					rc);
		return rc;
	}
	chip->batt_st.current_max = prop.intval / 1000;

	return 0;
}

static void p922x_handle_wls_insertion(struct p922x_charger *chip)
{
	chip->tx_connected = true;
	chip->rx_vout_max = MAX_VOUT_MV_DEFAULT;
	chip->rx_iout_max = 0;
	chip->ldo_enabled = true;

	power_supply_changed(chip->wls_psy);
	cancel_delayed_work(&chip->heartbeat_work);
	schedule_delayed_work(&chip->heartbeat_work,
			msecs_to_jiffies(0));
	cancel_delayed_work(&chip->tx_auth_done_work);
	schedule_delayed_work(&chip->tx_auth_done_work,
			msecs_to_jiffies(WAIT_FOR_AUTH_MS * 2));
}

static void p922x_handle_wls_removal(struct p922x_charger *chip)
{
	chip->irq_en = 0;
	chip->irq_stat = 0;
	chip->stat = 0;
	chip->rx_vout_max = 0;
	chip->rx_iout_max = 0;
	chip->rx_ready = false;
	chip->ldo_enabled = false;
	chip->tx_connected = false;
	chip->tx_auth_st = TX_AUTH_STAT_NONE;
	chip->tx_chg_type = TX_CHGR_TYPE_UNKNOWN;
	chip->temp_level = 0;
	chip->thermal_levels = 0;
	memset(&chip->batt_st, 0, sizeof(struct battery_st));

	power_supply_changed(chip->wls_psy);
	cancel_delayed_work(&chip->heartbeat_work);
	cancel_delayed_work(&chip->tx_auth_done_work);
}

static void p922x_clear_irq(struct p922x_charger *chip, u16 mask)
{
	p922x_write_buffer(chip, IRQ_CLEAR_REG, (u8 *)&mask, 2);
	p922x_write_reg(chip, CMD_2_REG, CMD_2_CLR_IRQ);
}

static void p922x_tx_auth_done_work(struct work_struct *work)
{
	struct p922x_charger *chip = container_of(work,
				struct p922x_charger, tx_auth_done_work.work);

	if (chip->tx_auth_enabled) {
		/* if no authen irq is received within WAIT_FOR_AUTH_MS * 2
		 * duration time after wireless insertion, means rx has already
		 * completed auth, and consider it as dev authen passed.
		 */
		switch (chip->tx_auth_st) {
		case TX_AUTH_STAT_NONE:
			p922x_dbg(chip, PR_MOTO, "No authen is received\n");
			chip->tx_auth_st = TX_AUTH_STAT_DEV_PASSED;
			break;
		case TX_AUTH_STAT_ID_PASSED:
			p922x_write_reg(chip, CMD_2_REG, CMD_2_SEND_AUTH);
			break;
		case TX_AUTH_STAT_DEV_PASSED:
			p922x_dbg(chip, PR_MOTO, "Auth has been done\n");
			break;
		default:
			break;
		}
	} else {
		p922x_dbg(chip, PR_MOTO, "Auth is not required\n");
		chip->tx_auth_st = TX_AUTH_STAT_DEV_PASSED;
	}

	if (chip->tx_auth_st == TX_AUTH_STAT_DEV_PASSED) {
		chip->tx_chg_type = p922x_get_tx_chgr_type(chip);
		p922x_dbg(chip, PR_MOTO, "TX charger type is %s\n",
					type_text[chip->tx_chg_type]);
	}

	cancel_delayed_work(&chip->heartbeat_work);
	schedule_delayed_work(&chip->heartbeat_work,
			msecs_to_jiffies(0));
}

static void p922x_chg_det_work(struct work_struct *work)
{
	int rc;
	u16 chip_id;
	struct p922x_charger *chip = container_of(work,
				struct p922x_charger, chg_det_work);

	rc = p922x_read_buffer(chip, CHIP_ID_REG, (u8 *)&chip_id, 2);
	if (chip->tx_connected && rc == -ENOTCONN) {
		p922x_handle_wls_removal(chip);
		p922x_dbg(chip, PR_MOTO, "Wireless charger is removed\n");
	} else if (!chip->tx_connected && !rc) {
		p922x_handle_wls_insertion(chip);
		p922x_dbg(chip, PR_MOTO, "CHIP_ID = 0x%04x\n", chip_id);
		p922x_dbg(chip, PR_MOTO, "Wireless charger is inserted\n");
	}
}

static void p922x_heartbeat_work(struct work_struct *work)
{
	struct p922x_charger *chip = container_of(work,
				struct p922x_charger, heartbeat_work.work);

	if (chip->tx_connected) {
		int rc;
		u16 vmax, imax;
		u16 target_vout, target_iout;

		rc = p922x_update_supplies_status(chip);
		if (rc < 0) {
			p922x_err(chip,	"Update supplies status, rc=%d\n", rc);
			schedule_delayed_work(&chip->heartbeat_work,
				msecs_to_jiffies(HEARTBEAT_INTERVAL_MS));
			return;
		}

		p922x_get_tx_capability(chip, &vmax, &imax);
		if (chip->wired_connected || chip->charging_disabled) {
			target_vout = MAX_VOUT_MV_DEFAULT;
			target_iout = 0;
		} else if (chip->batt_st.capacity > chip->chg_limit_soc) {
			target_vout = MAX_VOUT_MV_DEFAULT;
			if (chip->batt_st.status == POWER_SUPPLY_STATUS_FULL)
				target_iout = chip->chg_idle_cl;
			else if (chip->batt_st.capacity == 100)
				target_iout = MAX_IOUT_MA_DEFAULT / 2;
			else
				target_iout = MAX_IOUT_MA_DEFAULT;
		} else {
			int batt_pwr_max;
			int chg_pwr_max;
			int type = chip->batt_st.charge_type;

			if (type == POWER_SUPPLY_CHARGE_TYPE_TAPER &&
			    chip->batt_st.current_now < 0) {
				batt_pwr_max = chip->batt_st.voltage_max *
						(-chip->batt_st.current_now);
				chg_pwr_max = batt_pwr_max * 100 /
						chip->chg_efficiency;
			} else if (type == POWER_SUPPLY_CHARGE_TYPE_FAST ||
			    type == POWER_SUPPLY_CHARGE_TYPE_TRICKLE) {
				batt_pwr_max = chip->batt_st.voltage_max *
						chip->batt_st.current_max;
				chg_pwr_max = batt_pwr_max * 100 /
						chip->chg_efficiency;
			} else
				chg_pwr_max = chip->rx_vout_max *
						chip->rx_iout_max;

			chg_pwr_max = min(chg_pwr_max, (vmax * imax));
			target_vout = vmax;
			target_iout = chg_pwr_max / target_vout;
			if (target_iout < chip->chg_idle_cl)
				target_iout = chip->chg_idle_cl;
		}

		target_vout = min(target_vout, chip->wls_allowed_vmax);
		target_iout = min(target_iout, chip->wls_allowed_imax);
		if (target_vout != chip->rx_vout_max ||
		    target_iout != chip->rx_iout_max) {
			rc = p922x_request_rx_power(chip,
						target_vout,
						target_iout);
			if (rc < 0)
				p922x_err(chip, "Request power, rc=%d\n", rc);
			else
				power_supply_changed(chip->wls_psy);
		} else if (!chip->consumer_psy) {
			chip->consumer_psy =
				power_supply_get_by_name(pm_wls_supplied_to[0]);
			power_supply_changed(chip->wls_psy);
		}

		schedule_delayed_work(&chip->heartbeat_work,
			msecs_to_jiffies(HEARTBEAT_INTERVAL_MS));
	}
}

static int p922x_hw_init(struct p922x_charger *chip)
{
	int rc;

	if (regulator_count_voltages(chip->vdd_i2c_vreg) > 0) {
		rc = regulator_set_voltage(chip->vdd_i2c_vreg,
					1800000, 1800000);
		if (rc) {
			p922x_err(chip, "Failed to set vreg voltage, rc=%d\n",
						rc);
			return rc;
		}
	}
	rc = regulator_enable(chip->vdd_i2c_vreg);
	if (rc) {
		p922x_err(chip, "Failed to enable vdd vreg, rc=%d\n", rc);
		return rc;
	}

	if (chip->pinctrl_name) {
		chip->pinctrl_irq = pinctrl_get_select(chip->dev,
						chip->pinctrl_name);
		if (IS_ERR(chip->pinctrl_irq)) {
			p922x_err(chip,
				"Couldn't get/set %s pinctrl state rc=%ld\n",
				chip->pinctrl_name,
				PTR_ERR(chip->pinctrl_irq));
			rc = PTR_ERR(chip->pinctrl_irq);
			goto disable_vreg;
		}
	}

	rc = gpio_request_one(chip->irq_gpio.gpio, chip->irq_gpio.flags,
						chip->irq_gpio.label);
	if (rc) {
		p922x_err(chip, "Failed to request wls_int GPIO rc=%d\n", rc);
		goto disable_vreg;
	}

	if (chip->rx_chip_det) {
		u16 chip_id;
		bool rx_present = gpio_get_value(chip->irq_gpio.gpio);

		if (!rx_present) {
			rc = p922x_get_rx_chip_id(chip, &chip_id);
			rx_present = rc < 0 ? false : true;
		}
		p922x_dbg(chip, PR_MOTO, "RX chip is %s\n",
			rx_present ? "present" : "absent");

		if (!rx_present) {
			rc = -ENODEV;
			gpio_free(chip->irq_gpio.gpio);
			goto disable_vreg;
		}
		return 0;
	}

disable_vreg:
	regulator_disable(chip->vdd_i2c_vreg);

	return rc;
}

#define WLS_LOG_BUF_SIZE 128
static int p922x_program_otp_bootloader(struct p922x_charger *chip)
{
	int i;
	int rc = 0;
	u32 offset;
	char log_buf[WLS_LOG_BUF_SIZE] = {0};

	/*
	 * Transfer 9220 boot loader code "OTPBootloader" to 9220 SRAM
	 * - Setup 9220 registers before transferring the boot loader code
	 * - Transfer the boot loader code to 9220 SRAM
	 * - Reset 9220 => 9220 M0 runs the boot loader
	 */
	rc = p922x_write_reg(chip, 0x3000, 0x5a);
	if (rc < 0) {
		p922x_err(chip, "Failed to write 0x3000(5a), rc=%d\n", rc);
		return rc;
	}

	rc = p922x_write_reg(chip, 0x3040, 0x10);
	if (rc < 0) {
		p922x_err(chip, "Failed to write 0x3040(10), rc=%d\n", rc);
		return rc;
	}

	/* Write OTP bootloader data */
	for (i = 0; i < sizeof(otp_bootloader); i++) {
		rc = p922x_write_reg(chip, 0x1c00 + i, otp_bootloader[i]);
		if (rc < 0) {
			p922x_err(chip, "Failed to program OTP bootloader\n");
			return rc;
		}

		if (i % 16) {
			offset += scnprintf(log_buf + offset,
				WLS_LOG_BUF_SIZE,
				"0x%02x, ", otp_bootloader[i]);
			if (i == (sizeof(otp_bootloader) - 1))
				p922x_dbg(chip, PR_MOTO, "%s\n", log_buf);
		} else {
			p922x_dbg(chip, PR_MOTO, "%s\n", log_buf);
			memset(log_buf, '\0', WLS_LOG_BUF_SIZE);
			offset = scnprintf(log_buf,
				WLS_LOG_BUF_SIZE,
				"BL@0x%04x: 0x%02x, ", i, otp_bootloader[i]);
		}
	}

	rc = p922x_write_reg(chip, 0x3048, 0x80);
	if (rc < 0) {
		p922x_err(chip, "Failed to write 0x3048(80), rc=%d\n", rc);
		return rc;
	}

	/* ignoreNAK */
	rc = p922x_write_reg(chip, 0x3040, 0x80);
	if (rc < 0) {
		p922x_err(chip, "Failed to write 0x3040(80), rc=%d\n", rc);
		return rc;
	}
	msleep(100);

	return rc;
}

#define FW_OTP_CHECK_ST_RETRY_CNT 5
#define FW_OTP_PACK_HEADER_LEN 8
#define FW_OTP_PACK_DATA_LEN 128
#define FW_OTP_PACK_SIZE (FW_OTP_PACK_HEADER_LEN + FW_OTP_PACK_DATA_LEN)
static int p922x_program_otp_package(struct p922x_charger *chip,
			const u8 *src, u16 addr, u32 len)
{
	int i;
	int rc;
	u32 offset;
	u16 check_sum = addr;
	int retry_cnt = 0;
	char buf[FW_OTP_PACK_SIZE];
	char log_buf[WLS_LOG_BUF_SIZE] = {0};

	memset(buf, 0, FW_OTP_PACK_SIZE);

	/*(1) Copy the 128 bytes of the OTP image data to the packet data buffer
	 *    Array.Copy(srcData, i + srcOffs, sBuf, 8, 128);
	 *    Copy 128 bytes from srcData (starting at i+srcOffs)
	 *    to sBuf (starting at 8)
	 *	srcData     --- source array
	 *	i + srcOffs     --- start index in source array
	 *	sBuf         --- destination array
	 *	8         --- start index in destination array
	 *	128         --- elements to copy
	 */
	memcpy(buf + FW_OTP_PACK_HEADER_LEN, src, len);

	for (i = len - 1; i >= 0; i--) {
		if (buf[i + FW_OTP_PACK_HEADER_LEN] != 0)
			break;
		len--;
	}
	/* skip programming if nothing to program*/
	if (len == 0)
		return 0;

	/*
	 *(2) Calculate the packet checksum of the 128-byte data,
	 *	StartAddr, and CodeLength
	 */
	for (; i >= 0; i--)
		check_sum += buf[i + FW_OTP_PACK_HEADER_LEN];
	check_sum += len;

	/*(3) Fill up StartAddr, CodeLength, CheckSum of the current packet.*/
	memcpy(buf + 2, &addr, 2);
	memcpy(buf + 4, &len, 2);
	memcpy(buf + 6, &check_sum, 2);

	/* Send the current packet to 9220 SRAM via I2C
	 * read status is guaranteed to be != 1 at this point
	 */
	for (i = 0; i < (len + FW_OTP_PACK_HEADER_LEN); i++) {
		rc = p922x_write_reg(chip, 0x400 + i, buf[i]);
		if (rc < 0) {
			p922x_err(chip, "ERROR: on writing to OTP buffer\n");
			return rc;
		}

		if (i < FW_OTP_PACK_HEADER_LEN)
			continue;

		if ((i - FW_OTP_PACK_HEADER_LEN) % 16) {
			offset += scnprintf(log_buf + offset,
				WLS_LOG_BUF_SIZE, "0x%02x, ", buf[i]);
			if (i == (len + FW_OTP_PACK_HEADER_LEN - 1))
				p922x_dbg(chip, PR_MOTO, "%s\n", log_buf);
		} else {
			p922x_dbg(chip, PR_MOTO, "%s\n", log_buf);
			memset(log_buf, '\0', WLS_LOG_BUF_SIZE);
			offset = scnprintf(log_buf, WLS_LOG_BUF_SIZE,
				"OTP@0x%04x(CS-%04x): 0x%02x, ",
				addr + (i - FW_OTP_PACK_HEADER_LEN),
				check_sum, buf[i]);
		}
	}

	/* Write 1 to the Status in the SRAM. This informs the 9220 to
	 * start programming the new packet
	 * from SRAM to OTP memory
	 */
	rc = p922x_write_reg(chip, 0x400, 1);
	if (rc < 0) {
		p922x_err(chip, "ERROR: on OTP buffer validation\n");
		return rc;
	}

	/* Wait for 9220 bootloader to complete programming the current
	 * packet image data from SRAM to the OTP.
	 * The boot loader will update the Status in the SRAM as follows:
	 * Status:
	 * "0" - reset value (from AP)
	 * "1" - buffer validated / busy (from AP)
	 * "2" - finish "OK" (from the boot loader)
	 * "4" - programming error (from the boot loader)
	 * "8" - wrong check sum (from the boot loader)
	 * "16"- programming not possible (try to write "0" to bit location
	 * already programmed to "1")
	 *       (from the boot loader)
	 * DateTime startT = DateTime.Now;
	 */
	do {
		msleep(100);
		p922x_read_reg(chip, 0x400, buf);
		if (retry_cnt++ > FW_OTP_CHECK_ST_RETRY_CNT)
			break;
	} while (buf[0] == 1);

	if (buf[0] != 2) {
		p922x_err(chip,
			"ERROR: Programming OTP buffer status:%02x at:%d\n",
			buf[0], addr);
		return -EAGAIN;
	}

	return 0;
}

static int p922x_program_otp(struct p922x_charger *chip,
		u16 destAddr, const u8 *src, u32 size)
{
	int i;
	int rc;

	/* Program OTP image data to 9220 OTP memory */
	for (i = destAddr; i < destAddr + size; i += FW_OTP_PACK_DATA_LEN) {
		int len = FW_OTP_PACK_DATA_LEN;

		if ((destAddr + size) < (i + FW_OTP_PACK_DATA_LEN))
			len = destAddr + size - i;
		rc = p922x_program_otp_package(chip, src, i, len);
		if (rc < 0) {
			p922x_err(chip, "Program otp failed at 0x%04x\n", i);
			return rc;
		}
		src += FW_OTP_PACK_DATA_LEN;
	}

	/*
	 * Need to reset or power cycle 9220 to run the OTP code
	*/
	rc = p922x_write_reg(chip, 0x3000, 0x5a);
	if (rc < 0) {
		p922x_err(chip, "Failed to write 0x3000(5a), rc=%d\n", rc);
		return rc;
	}

	rc = p922x_write_reg(chip, 0x3048, 0x00);
	if (rc < 0) {
		p922x_err(chip, "Failed to write 0x3048(00), rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int p922x_program_fw(struct p922x_charger *chip)
{
	int rc;
	u16 chip_id;
	const struct firmware *fw = NULL;

	if (chip->program_fw_stat == PROGRAM_FW_PENDING) {
		p922x_err(chip, "Programming FW is ongoing\n");
		return -EBUSY;
	}
	chip->program_fw_stat = PROGRAM_FW_PENDING;

	rc = request_firmware(&fw, chip->fw_name, NULL);
	if (rc < 0) {
		p922x_err(chip, "Request firmware (%s) failed, rc=%d\n",
			chip->fw_name, rc);
		chip->program_fw_stat = PROGRAM_FW_FAIL;
		return rc;
	}

	rc = p922x_get_rx_chip_id(chip, &chip_id);
	if (rc < 0) {
		p922x_err(chip, "Failed to get RX CHIP ID, rc=%d\n", rc);
		chip->program_fw_stat = PROGRAM_FW_FAIL;
		goto release_fw;
	}
	p922x_dbg(chip, PR_MOTO, "RX chip ID is 0x%04x\n", chip_id);

	rc = p922x_program_otp_bootloader(chip);
	if (rc < 0) {
		p922x_err(chip, "Programming OTP bootloader failed, rc=%d\n",
			rc);
		chip->program_fw_stat = PROGRAM_FW_FAIL;
		goto release_fw;
	}

	rc = p922x_program_otp(chip, 0x0000, fw->data, fw->size);
	if (rc < 0) {
		p922x_err(chip, "Programming OTP failed, rc=%d\n", rc);
		chip->program_fw_stat = PROGRAM_FW_FAIL;
		goto release_fw;
	}

	chip->program_fw_stat = PROGRAM_FW_SUCCESS;
	p922x_dbg(chip, PR_MOTO, "Programming FW success\n");

release_fw:
	release_firmware(fw);

	return rc;
}

static int p922x_prepare_rx(struct p922x_charger *chip)
{
	int rc;

	chip->irq_en = 0x3FFF;
	rc = p922x_write_buffer(chip, IRQ_ENABLE_REG, (u8 *)&chip->irq_en, 2);
	if (rc < 0) {
		p922x_err(chip, "Failed to write irq enable, rc=%d\n", rc);
		return rc;
	}

	if (chip->fod_array_5v_len == FOD_ARRAY_LEN) {
		p922x_write_buffer(chip, FOD_CFG_REG,
				chip->fod_array_5v, FOD_ARRAY_LEN);
	}

	return 0;
}

static int p922x_get_status(struct p922x_charger *chip)
{
	int rc;

	chip->irq_en = 0;
	rc = p922x_read_buffer(chip, IRQ_ENABLE_REG, (u8 *)&chip->irq_en, 2);
	if (rc < 0) {
		p922x_err(chip, "Failed to get irq enable, rc=%d\n", rc);
		return rc;
	}

	chip->irq_stat = 0;
	rc = p922x_read_buffer(chip, IRQ_STATUS_REG, (u8 *)&chip->irq_stat, 2);
	if (rc < 0) {
		p922x_err(chip, "Failed to get irq status, rc=%d\n", rc);
		return rc;
	}

	chip->stat = 0;
	rc = p922x_read_buffer(chip, DEV_STATUS_REG, (u8 *)&chip->stat, 2);
	if (rc < 0) {
		p922x_err(chip, "Failed to get status, rc=%d\n", rc);
		return rc;
	}

	p922x_dbg(chip, PR_MOTO, "IRQ_ENABLE=0x%x, IRQ_ST=0x%x, STATUS=0x%x\n",
			chip->irq_en, chip->irq_stat, chip->stat);

	return 0;
}

static void p922x_check_status(struct p922x_charger *chip)
{
	u16 status = chip->irq_stat & chip->stat;

	if (status & ST_ID_AUTH_SUCCESS) {
		chip->tx_auth_st = TX_AUTH_STAT_ID_PASSED;
		cancel_delayed_work(&chip->tx_auth_done_work);
		schedule_delayed_work(&chip->tx_auth_done_work,
				msecs_to_jiffies(WAIT_FOR_AUTH_MS));
		p922x_dbg(chip, PR_INTERRUPT, "IRQ: ST_ID_AUTH_SUCCESS\n");
	}
	if (status & ST_ID_AUTH_FAILED) {
		chip->tx_auth_st = TX_AUTH_STAT_ID_FAILED;
		cancel_delayed_work(&chip->tx_auth_done_work);
		schedule_delayed_work(&chip->tx_auth_done_work,
				msecs_to_jiffies(0));
		p922x_dbg(chip, PR_INTERRUPT, "IRQ: ST_ID_AUTH_FAILED\n");
	}

	if (status & ST_DEV_AUTH_SUCCESS) {
		chip->tx_auth_st = TX_AUTH_STAT_DEV_PASSED;
		cancel_delayed_work(&chip->tx_auth_done_work);
		schedule_delayed_work(&chip->tx_auth_done_work,
				msecs_to_jiffies(WAIT_FOR_AUTH_MS));
		p922x_dbg(chip, PR_INTERRUPT, "IRQ: ST_DEV_AUTH_SUCCESS\n");
	}
	if (status & ST_DEV_AUTH_FAILED) {
		chip->tx_auth_st = TX_AUTH_STAT_DEV_FAILED;
		cancel_delayed_work(&chip->tx_auth_done_work);
		schedule_delayed_work(&chip->tx_auth_done_work,
				msecs_to_jiffies(0));
		p922x_dbg(chip, PR_INTERRUPT, "IRQ: ST_DEV_AUTH_FAILED\n");
	}

	if (status & ST_SEND_PKT_SUCCESS)
		p922x_dbg(chip, PR_INTERRUPT, "IRQ: ST_SEND_PKT_SUCCESS\n");
	if (status & ST_SEND_PKT_TIMEOUT)
		p922x_dbg(chip, PR_INTERRUPT, "IRQ: ST_SEND_PKT_TIMEOUT\n");

	if (status & ST_LDO_OFF) {
		chip->ldo_enabled = false;
		p922x_dbg(chip, PR_INTERRUPT, "IRQ: ST_LDO_OFF\n");
	}
	if (status & ST_LDO_ON) {
		chip->ldo_enabled = true;
		p922x_dbg(chip, PR_INTERRUPT, "IRQ: ST_LDO_ON\n");
	}

	if (status & ST_MODE_CHANGE)
		p922x_dbg(chip, PR_INTERRUPT, "IRQ: ST_MODE_CHANGE\n");

	if (status & ST_TX_DATA_RCVD) {
		complete(&chip->rcvd_completion);
		p922x_dbg(chip, PR_INTERRUPT, "IRQ: ST_TX_DATA_RCVD\n");
	}

	if (status & ST_RX_READY)
		p922x_dbg(chip, PR_INTERRUPT, "IRQ: ST_RX_READY\n");

	if (status & ST_OVER_TEMP)
		p922x_dbg(chip, PR_INTERRUPT, "IRQ: ST_OVER_TEMP\n");
	if (status & ST_OVER_VOLT)
		p922x_dbg(chip, PR_INTERRUPT, "IRQ: ST_OVER_VOLT\n");
	if (status & ST_OVER_CURR)
		p922x_dbg(chip, PR_INTERRUPT, "IRQ: ST_OVER_CURR\n");
}

static irqreturn_t p922x_irq_handler(int irq, void *dev_id)
{
	int rc = 0;
	bool chg_changed = false;
	struct p922x_charger *chip = dev_id;

	if (!chip) {
		p922x_err(chip, "Invalid chip\n");
		return IRQ_HANDLED;
	}

	if (chip->program_fw_stat == PROGRAM_FW_PENDING) {
		p922x_err(chip, "Skip irq for FW programming\n");
		return IRQ_HANDLED;
	}

	if (!chip->rx_ready) {
		rc = p922x_prepare_rx(chip);
		if (!rc)
			chip->rx_ready = true;
		chg_changed = true;
	}

	if (chip->rx_ready) {
		rc = p922x_get_status(chip);
		if (!rc) {
			p922x_clear_irq(chip, chip->irq_stat);
			p922x_check_status(chip);
		} else
			chg_changed = true;
	}

	if (chg_changed)
		schedule_work(&chip->chg_det_work);

	return IRQ_HANDLED;
}

static ssize_t fw_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;
	u8 ver[4];
	struct p922x_charger *chip = dev_get_drvdata(dev);

	rc = p922x_read_buffer(chip, E2PROM_FW_VER_REG, ver, 4);
	if (rc < 0)
		return rc;

	return scnprintf(buf, WLS_SHOW_MAX_SIZE, "%02d.%02d.%02d.%02d\n",
					ver[3], ver[2], ver[1], ver[0]);
}
static DEVICE_ATTR(fw_ver, S_IRUGO, fw_ver_show, NULL);

static ssize_t program_fw_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct p922x_charger *chip = dev_get_drvdata(dev);

	return scnprintf(buf, WLS_SHOW_MAX_SIZE, "%d\n", chip->program_fw_stat);
}
static DEVICE_ATTR(program_fw_stat, S_IRUGO, program_fw_stat_show, NULL);

static ssize_t program_fw_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int rc;
	unsigned long r;
	unsigned long flash;
	struct p922x_charger *chip = dev_get_drvdata(dev);

	r = kstrtoul(buf, 0, &flash);
	if (r) {
		p922x_err(chip, "Invalid flash value = %ld\n", flash);
		return -EINVAL;
	}

	if (flash) {
		rc = p922x_program_fw(chip);
		if (rc < 0)
			return rc;
	}
	return r ? r : count;
}
static DEVICE_ATTR(program_fw, S_IWUSR, NULL, program_fw_store);

static ssize_t chip_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;
	u16 chip_id;
	struct p922x_charger *chip = dev_get_drvdata(dev);

	rc = p922x_get_rx_chip_id(chip, &chip_id);
	if (rc < 0)
		return rc;

	return scnprintf(buf, WLS_SHOW_MAX_SIZE, "0x%04x\n", chip_id);
}
static DEVICE_ATTR(chip_id, S_IRUGO, chip_id_show, NULL);

static ssize_t fastchg_volt_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int rc;
	unsigned long r;
	unsigned long volt;
	struct p922x_charger *chip = dev_get_drvdata(dev);

	r = kstrtoul(buf, 0, &volt);
	if (r) {
		p922x_err(chip, "Invalid fastchg volt value = %lu\n", volt);
		return -EINVAL;
	}

	rc = p922x_set_fastchg_voltage(chip, (u16)volt);
	if (rc < 0)
		return rc;

	power_supply_changed(chip->wls_psy);
	return r ? r : count;
}
static DEVICE_ATTR(fastchg_volt, S_IWUSR, NULL, fastchg_volt_store);

static ssize_t fod_5v_configure_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int i = 0, ret = 0, sum = 0;
	char *buffer;
	unsigned int temp;
	u8 fod_array_temp[FOD_ARRAY_LEN];
	struct p922x_charger *chip = dev_get_drvdata(dev);

	buffer = (char *)buf;

	for (i = 0; i < FOD_ARRAY_LEN; i++) {
		ret = sscanf((const char *)buffer, "0x%x,%s", &temp, buffer);
		fod_array_temp[i] = temp;
		sum++;
		if (ret != 2)
			break;
	}

	if (sum == FOD_ARRAY_LEN) {
		memcpy(chip->fod_array_5v, fod_array_temp,
						sizeof(fod_array_temp));
		chip->fod_array_5v_len = FOD_ARRAY_LEN;
		p922x_write_buffer(chip, FOD_CFG_REG,
				chip->fod_array_5v, FOD_ARRAY_LEN);
	}

	return count;
}

static ssize_t fod_5v_configure_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int count = 0, i = 0;
	struct p922x_charger *chip = dev_get_drvdata(dev);

	for (i = 0; i < FOD_ARRAY_LEN; i++) {
		count += scnprintf(buf+count, WLS_SHOW_MAX_SIZE,
				"0x%02x ", chip->fod_array_5v[i]);
		if (i == FOD_ARRAY_LEN - 1)
			count += scnprintf(buf+count, WLS_SHOW_MAX_SIZE, "\n");
	}

	return count;
}

static DEVICE_ATTR(fod_5v_configure, 0644,
			fod_5v_configure_show, fod_5v_configure_store);

static ssize_t fod_9v_configure_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int i = 0, ret = 0, sum = 0;
	char *buffer;
	unsigned int temp;
	u8 fod_array_temp[FOD_ARRAY_LEN];
	struct p922x_charger *chip = dev_get_drvdata(dev);

	buffer = (char *)buf;

	for (i = 0; i < FOD_ARRAY_LEN; i++) {
		ret = sscanf((const char *)buffer, "0x%x,%s", &temp, buffer);
		fod_array_temp[i] = temp;
		sum++;
		if (ret != 2)
			break;
	}

	if (sum == FOD_ARRAY_LEN) {
		memcpy(chip->fod_array_9v, fod_array_temp,
					sizeof(fod_array_temp));
		chip->fod_array_9v_len = FOD_ARRAY_LEN;
		p922x_write_buffer(chip, FOD_CFG_REG,
				chip->fod_array_9v, FOD_ARRAY_LEN);
	}

	return count;
}

static ssize_t fod_9v_configure_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int count = 0, i = 0;
	struct p922x_charger *chip = dev_get_drvdata(dev);

	for (i = 0; i < FOD_ARRAY_LEN; i++) {
		count += scnprintf(buf+count, WLS_SHOW_MAX_SIZE,
				"0x%02x ", chip->fod_array_9v[i]);
		if (i == FOD_ARRAY_LEN - 1)
			count += scnprintf(buf+count, WLS_SHOW_MAX_SIZE, "\n");
	}

	return count;
}

static DEVICE_ATTR(fod_9v_configure, 0644,
			fod_9v_configure_show, fod_9v_configure_store);

static ssize_t rx_vout_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int rc;
	unsigned long r;
	unsigned long vout;
	struct p922x_charger *chip = dev_get_drvdata(dev);

	r = kstrtoul(buf, 0, &vout);
	if (r) {
		p922x_err(chip, "Invalid vout value = %lu\n", vout);
		return -EINVAL;
	}

	rc = p922x_set_rx_vout(chip, (u16)vout);
	if (rc < 0)
		return rc;

	power_supply_changed(chip->wls_psy);
	return r ? r : count;
}

static ssize_t rx_vout_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int vout;
	struct p922x_charger *chip = dev_get_drvdata(dev);

	vout = p922x_get_rx_vout(chip);
	if (vout < 0)
		return vout;

	return scnprintf(buf, WLS_SHOW_MAX_SIZE, "%d\n", vout);
}
static DEVICE_ATTR(rx_vout, 0644, rx_vout_show, rx_vout_store);

static ssize_t rx_iout_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int iout;
	struct p922x_charger *chip = dev_get_drvdata(dev);

	iout = p922x_get_rx_iout(chip);
	if (iout < 0)
		return iout;

	return scnprintf(buf, WLS_SHOW_MAX_SIZE, "%d\n", iout);
}
static DEVICE_ATTR(rx_iout, S_IRUGO, rx_iout_show, NULL);

static ssize_t rx_ocl_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ocl;
	struct p922x_charger *chip = dev_get_drvdata(dev);

	ocl = p922x_get_rx_ocl(chip);
	if (ocl < 0)
		return ocl;

	return scnprintf(buf, WLS_SHOW_MAX_SIZE, "%d\n", ocl);
}
static DEVICE_ATTR(rx_ocl, S_IRUGO, rx_ocl_show, NULL);

static ssize_t tx_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct p922x_charger *chip = dev_get_drvdata(dev);
	int type;

	if (chip->tx_auth_st == TX_AUTH_STAT_DEV_PASSED)
		chip->tx_chg_type = p922x_get_tx_chgr_type(chip);
	type = chip->tx_chg_type;

	return scnprintf(buf, WLS_SHOW_MAX_SIZE, "%s\n", type_text[type]);
}
static DEVICE_ATTR(tx_type, S_IRUGO, tx_type_show, NULL);

static ssize_t tx_vin_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int rc;
	unsigned long r;
	unsigned long vin;
	struct p922x_charger *chip = dev_get_drvdata(dev);

	r = kstrtoul(buf, 0, &vin);
	if (r) {
		p922x_err(chip, "Invalid tx vin value = %lu\n", vin);
		return -EINVAL;
	}

	rc = p922x_set_tx_vin(chip, (u16)vin);
	if (rc < 0)
		return rc;

	return r ? r : count;
}

static ssize_t tx_vin_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int vin;
	struct p922x_charger *chip = dev_get_drvdata(dev);

	vin = p922x_get_tx_vin(chip);
	if (vin < 0)
		return vin;

	return scnprintf(buf, WLS_SHOW_MAX_SIZE, "%d\n", vin);
}
static DEVICE_ATTR(tx_vin, 0644, tx_vin_show, tx_vin_store);

static ssize_t tx_icl_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int icl;
	struct p922x_charger *chip = dev_get_drvdata(dev);

	icl = p922x_get_tx_icl(chip);
	if (icl < 0)
		return icl;

	return scnprintf(buf, WLS_SHOW_MAX_SIZE, "%d\n", icl);
}
static DEVICE_ATTR(tx_icl, S_IRUGO, tx_icl_show, NULL);

static int show_dump_regs(struct seq_file *m, void *data)
{
	struct p922x_charger *chip = m->private;
	u8 buf[4];

	p922x_read_buffer(chip, CHIP_ID_REG, buf, 2);
	seq_printf(m, "CHIP_ID: 0x%02x%02x\n", buf[1], buf[0]);
	p922x_read_reg(chip, HW_VER_REG, &buf[0]);
	seq_printf(m, "HW_VER: 0x%02x\n", buf[0]);
	p922x_read_reg(chip, CUST_ID_REG, &buf[0]);
	seq_printf(m, "CUST_ID: 0x%02x\n", buf[0]);
	p922x_read_buffer(chip, OTP_FW_VER_REG, buf, 4);
	seq_printf(m, "OTP_FW_VER: 0x%02x%02x:0x%02x%02x\n",
			buf[0], buf[1], buf[2], buf[3]);
	p922x_read_buffer(chip, E2PROM_FW_VER_REG, buf, 4);
	seq_printf(m, "E2PROM_FW_VER: 0x%02x%02x:0x%02x%02x\n",
			buf[0], buf[1], buf[2], buf[3]);
	p922x_read_buffer(chip, DEV_STATUS_REG, buf, 2);
	seq_printf(m, "DEV STATUS: 0x%02x%02x\n", buf[1], buf[0]);
	p922x_read_buffer(chip, IRQ_STATUS_REG, buf, 2);
	seq_printf(m, "IRQ STATUS: 0x%02x%02x\n", buf[1], buf[0]);
	p922x_read_buffer(chip, IRQ_ENABLE_REG, buf, 2);
	seq_printf(m, "IRQ ENABLE: 0x%02x%02x\n", buf[1], buf[0]);
	p922x_read_buffer(chip, VOUT_ADC_REG, buf, 2);
	seq_printf(m, "VOUT ADC: %dmV\n",
		(((buf[1]&0x0F) << 8)|buf[0]) * 6 * 2100 / 4095);
	p922x_read_reg(chip, VOUT_SET_REG, &buf[0]);
	seq_printf(m, "VOUT SET: %dmV\n", buf[0] * 100 + 3500);
	p922x_read_buffer(chip, VRECT_ADC_REG, buf, 2);
	seq_printf(m, "VRECT ADC: %dmV\n",
		(((buf[1]&0x0F) << 8)|buf[0]) * 10 * 2100 / 4095);
	p922x_read_buffer(chip, IOUT_REG, buf, 2);
	seq_printf(m, "IOUT: %dmA\n", (buf[1] << 8) | buf[0]);
	p922x_read_buffer(chip, DIE_TEMP_REG, buf, 2);
	seq_printf(m, "DIE TEMP: %d C\n",
		((((buf[1]&0x0F) << 8)|buf[0]) - 1350) * 83 / 444 - 273);
	p922x_read_buffer(chip, OPT_FREQ_REG, buf, 2);
	seq_printf(m, "OPT FREQ: %dkHz\n",
		64 * 6000 * 256 / ((buf[1] << 8) | buf[0]));
	p922x_read_buffer(chip, ILIMIT_SET_REG, buf, 2);
	seq_printf(m, "ILIMIT_SET: %dmA\n", buf[0] * 100 + 100);
	p922x_read_reg(chip, SYS_OPT_MODE_REG, &buf[0]);
	seq_printf(m, "SYS_OPT_MODE: %d\n", buf[0]);

	return 0;
}

static int dump_regs_debugfs_open(struct inode *inode, struct file *file)
{
	struct p922x_charger *chip = inode->i_private;

	return single_open(file, show_dump_regs, chip);
}

static const struct file_operations dump_regs_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= dump_regs_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int get_reg(void *data, u64 *val)
{
	struct p922x_charger *chip = data;
	int rc;
	u8 temp;

	rc = p922x_read_reg(chip, chip->peek_poke_address, &temp);
	if (rc) {
		p922x_err(chip, "Couldn't read reg %x rc = %d\n",
			chip->peek_poke_address, rc);
		return -EAGAIN;
	}
	*val = temp;
	return 0;
}

static int set_reg(void *data, u64 val)
{
	struct p922x_charger *chip = data;
	int rc;
	u8 temp;

	temp = (u8) val;
	rc = p922x_write_reg(chip, chip->peek_poke_address, temp);
	if (rc) {
		p922x_err(chip, "Couldn't write 0x%02x to 0x%02x rc= %d\n",
			temp, chip->peek_poke_address, rc);
		return -EAGAIN;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(poke_poke_debug_ops, get_reg, set_reg, "0x%02llx\n");

static void create_debugfs_entries(struct p922x_charger *chip)
{
	struct dentry *ent;

	chip->debug_root = debugfs_create_dir("p922x", NULL);
	if (!chip->debug_root) {
		p922x_err(chip, "Couldn't create debug dir\n");
		return;
	}

	ent = debugfs_create_file("dump_regs", S_IFREG | S_IRUGO,
				  chip->debug_root, chip,
				  &dump_regs_debugfs_ops);
	if (!ent)
		p922x_err(chip, "Couldn't create dump_regs debug file\n");

	ent = debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
				  chip->debug_root,
				  &(chip->peek_poke_address));
	if (!ent)
		p922x_err(chip, "Couldn't create address debug file\n");

	ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
				  chip->debug_root, chip,
				  &poke_poke_debug_ops);
	if (!ent)
		p922x_err(chip, "Couldn't create data debug file\n");
}

static void create_sysfs_entries(struct p922x_charger *chip)
{
	int rc;

	rc = device_create_file(chip->dev, &dev_attr_fod_5v_configure);
	if (rc)
		p922x_err(chip, "Couldn't create fod_5v_configure\n");

	rc = device_create_file(chip->dev, &dev_attr_fod_9v_configure);
	if (rc)
		p922x_err(chip, "Couldn't create fod_9v_configure\n");

	rc = device_create_file(chip->dev, &dev_attr_chip_id);
	if (rc)
		p922x_err(chip, "Couldn't create chip_id\n");

	rc = device_create_file(chip->dev, &dev_attr_fastchg_volt);
	if (rc)
		p922x_err(chip, "Couldn't create fastchg_volt\n");

	rc = device_create_file(chip->dev, &dev_attr_rx_vout);
	if (rc)
		p922x_err(chip, "Couldn't create rx_vout\n");

	rc = device_create_file(chip->dev, &dev_attr_rx_iout);
	if (rc)
		p922x_err(chip, "Couldn't create rx_iout\n");

	rc = device_create_file(chip->dev, &dev_attr_rx_ocl);
	if (rc)
		p922x_err(chip, "Couldn't create rx_ocl\n");

	rc = device_create_file(chip->dev, &dev_attr_tx_type);
	if (rc)
		p922x_err(chip, "Couldn't create tx_type\n");

	rc = device_create_file(chip->dev, &dev_attr_tx_vin);
	if (rc)
		p922x_err(chip, "Couldn't create tx_vin\n");

	rc = device_create_file(chip->dev, &dev_attr_tx_icl);
	if (rc)
		p922x_err(chip, "Couldn't create tx_icl\n");

	if (chip->program_fw_enabled) {
		rc = device_create_file(chip->dev, &dev_attr_fw_ver);
		if (rc)
			p922x_err(chip, "Couldn't create fw_ver\n");

		rc = device_create_file(chip->dev, &dev_attr_program_fw_stat);
		if (rc)
			p922x_err(chip, "Couldn't create program_fw_stat\n");

		rc = device_create_file(chip->dev, &dev_attr_program_fw);
		if (rc)
			p922x_err(chip, "Couldn't create program_fw\n");
	}
}

static void remove_sysfs_entries(struct p922x_charger *chip)
{
	device_remove_file(chip->dev, &dev_attr_chip_id);
	device_remove_file(chip->dev, &dev_attr_fastchg_volt);
	device_remove_file(chip->dev, &dev_attr_rx_vout);
	device_remove_file(chip->dev, &dev_attr_rx_iout);
	device_remove_file(chip->dev, &dev_attr_rx_ocl);
	device_remove_file(chip->dev, &dev_attr_tx_type);
	device_remove_file(chip->dev, &dev_attr_tx_vin);
	device_remove_file(chip->dev, &dev_attr_tx_icl);
	device_remove_file(chip->dev, &dev_attr_fod_5v_configure);
	device_remove_file(chip->dev, &dev_attr_fod_9v_configure);
	if (chip->program_fw_enabled) {
		device_remove_file(chip->dev, &dev_attr_fw_ver);
		device_remove_file(chip->dev, &dev_attr_program_fw_stat);
		device_remove_file(chip->dev, &dev_attr_program_fw);
	}
}

static int p922x_parse_dt(struct p922x_charger *chip)
{
	int rc = 0;
	struct device_node *node = chip->dev->of_node;

	if (!node) {
		p922x_err(chip, "device tree info. missing\n");
		return -EINVAL;
	}

	chip->tx_auth_enabled = of_property_read_bool(node,
					"idt,tx-auth-enabled");

	rc = of_property_read_u32(node, "idt,chg-efficiency",
					&chip->chg_efficiency);
	if (rc)
		chip->chg_efficiency = 85;

	rc = of_property_read_u32(node, "idt,chg-limit-soc",
					&chip->chg_limit_soc);
	if (rc)
		chip->chg_limit_soc = 95;

	rc = of_property_read_u32(node, "idt,chg-idle-cl",
					&chip->chg_idle_cl);
	if (rc)
		chip->chg_idle_cl = 50;

	if (of_find_property(node, "vdd-i2c-supply", NULL)) {
		chip->vdd_i2c_vreg = devm_regulator_get(chip->dev, "vdd-i2c");
		if (IS_ERR(chip->vdd_i2c_vreg))
			return PTR_ERR(chip->vdd_i2c_vreg);
	}

	if (of_gpio_count(node) < 0) {
		p922x_err(chip, "No GPIOS defined.\n");
		return -EINVAL;
	}
	chip->irq_gpio.gpio = of_get_gpio_flags(node, 0,
				(enum of_gpio_flags *)&chip->irq_gpio.flags);
	of_property_read_string_index(node, "gpio-names", 0,
					&chip->irq_gpio.label);
	if (!gpio_is_valid(chip->irq_gpio.gpio)) {
		p922x_err(chip, "Invalid gpio wls_int=%d\n",
					chip->irq_gpio.gpio);
		return -EINVAL;
	}

	chip->pinctrl_name = of_get_property(chip->dev->of_node,
						"pinctrl-names", NULL);

	chip->rx_chip_det = of_property_read_bool(node,
					"idt,rx-chip-det");

	chip->program_fw_enabled = of_property_read_bool(node,
					"idt,program-fw-enabled");

	chip->fw_name = of_get_property(chip->dev->of_node,
						"fw-name", NULL);
	if (!chip->fw_name)
		chip->fw_name = "p922x_otp.fw";

	chip->fod_array_5v_len =
		of_property_count_u8_elems(node,
					"fod-array-5v-val");

	chip->fod_array_9v_len =
		of_property_count_u8_elems(node,
					"fod-array-9v-val");

	of_property_read_u8_array(chip->dev->of_node, "fod-array-5v-val",
					chip->fod_array_5v,
					chip->fod_array_5v_len);
	of_property_read_u8_array(chip->dev->of_node, "fod-array-9v-val",
					chip->fod_array_9v,
					chip->fod_array_9v_len);
	return 0;
}

static enum power_supply_property p922x_wls_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_REAL_TYPE,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL,
	POWER_SUPPLY_PROP_NUM_SYSTEM_TEMP_LEVELS,
};

static int p922x_wls_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct p922x_charger *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->tx_connected;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->ldo_enabled;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = p922x_get_rx_iout(chip);
		if (rc >= 0)
			val->intval = rc * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = chip->rx_iout_max * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = p922x_get_rx_vout(chip);
		if (rc >= 0)
			val->intval = rc * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = chip->rx_vout_max * 1000;
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		val->intval = POWER_SUPPLY_TYPE_WIRELESS;
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = !chip->charging_disabled;
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		val->intval = chip->temp_level;
		break;
	case POWER_SUPPLY_PROP_NUM_SYSTEM_TEMP_LEVELS:
		val->intval = chip->thermal_levels;
		break;
	default:
		return -EINVAL;
	}

	if (rc < 0) {
		p922x_dbg(chip, PR_MISC, "Couldn't get prop %d rc = %d\n",
					psp, rc);
		return rc;
	}

	return 0;
}

static int p922x_wls_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct p922x_charger *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if ((val->intval / 1000) > MAX_IOUT_MA_FAST)
			return -EINVAL;
		chip->wls_allowed_imax = val->intval / 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if ((val->intval / 1000) > MAX_VOUT_MV_FAST)
			return -EINVAL;
		chip->wls_allowed_vmax = val->intval / 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		chip->charging_disabled = !val->intval;
		if (chip->tx_connected)
			rc = p922x_enable_charging(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		if (chip->thermal_levels <= 0)
			break;
		if (val->intval > chip->thermal_levels)
			chip->temp_level = chip->thermal_levels - 1;
		else
			chip->temp_level = val->intval;
		break;
	default:
		return -EINVAL;
	}

	power_supply_changed(chip->wls_psy);
	return rc;
}

static int p922x_wls_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int rc;

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}

static void p922x_external_power_changed(struct power_supply *psy)
{
	struct p922x_charger *chip = power_supply_get_drvdata(psy);

	if (chip->tx_connected) {
		schedule_work(&chip->chg_det_work);
		cancel_delayed_work(&chip->heartbeat_work);
		schedule_delayed_work(&chip->heartbeat_work,
				msecs_to_jiffies(0));
	}
}

static const struct regmap_config p922x_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0xFFFF,
};

static int p922x_charger_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int rc;
	struct p922x_charger *chip;
	struct power_supply_config wls_psy_cfg = {};

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->dev = &client->dev;
	chip->name = "WLS";
	chip->debug_mask = &__debug_mask;
	INIT_DELAYED_WORK(&chip->tx_auth_done_work, p922x_tx_auth_done_work);
	INIT_WORK(&chip->chg_det_work, p922x_chg_det_work);
	INIT_DELAYED_WORK(&chip->heartbeat_work, p922x_heartbeat_work);
	device_init_wakeup(chip->dev, true);

	chip->regmap = regmap_init_i2c(client, &p922x_regmap_config);
	if (IS_ERR(chip->regmap)) {
		pr_err("Couldn't initialize register regmap rc = %ld\n",
				PTR_ERR(chip->regmap));
		rc = PTR_ERR(chip->regmap);
		goto free_mem;
	}

	rc = p922x_parse_dt(chip);
	if (rc) {
		p922x_err(chip, "Couldn't parse DT nodes rc=%d\n", rc);
		goto free_mem;
	}

	i2c_set_clientdata(client, chip);
	dev_set_drvdata(chip->dev, chip);
	mutex_init(&chip->send_pkg_lock);
	init_completion(&chip->rcvd_completion);

	chip->wls_psy_d.name		= "idt_wireless";
	chip->wls_psy_d.type		= POWER_SUPPLY_TYPE_WIRELESS;
	chip->wls_psy_d.get_property	= p922x_wls_get_prop;
	chip->wls_psy_d.set_property	= p922x_wls_set_prop;
	chip->wls_psy_d.property_is_writeable =
					p922x_wls_prop_is_writeable;
	chip->wls_psy_d.properties	= p922x_wls_props;
	chip->wls_psy_d.num_properties	=
				ARRAY_SIZE(p922x_wls_props);
	chip->wls_psy_d.external_power_changed =
					p922x_external_power_changed;

	wls_psy_cfg.drv_data = chip;
	wls_psy_cfg.supplied_to = pm_wls_supplied_to;
	wls_psy_cfg.num_supplicants = ARRAY_SIZE(pm_wls_supplied_to);
	chip->wls_psy = power_supply_register(chip->dev,
			&chip->wls_psy_d,
			&wls_psy_cfg);
	if (IS_ERR(chip->wls_psy)) {
		p922x_err(chip, "Couldn't register wls psy rc=%ld\n",
				PTR_ERR(chip->wls_psy));
		rc = PTR_ERR(chip->wls_psy);
		goto free_mem;
	}
	chip->wls_psy->supplied_from = pm_wls_supplied_from;
	chip->wls_psy->num_supplies = ARRAY_SIZE(pm_wls_supplied_from);

	chip->irq_en = 0;
	chip->irq_stat = 0;
	chip->stat = 0;
	chip->rx_vout_max = 0;
	chip->rx_iout_max = 0;
	chip->wls_allowed_vmax = MAX_VOUT_MV_FAST;
	chip->wls_allowed_imax = MAX_IOUT_MA_FAST;
	chip->rx_ready = false;
	chip->ldo_enabled = false;
	chip->tx_connected = false;
	chip->tx_auth_st = TX_AUTH_STAT_NONE;
	chip->tx_chg_type = TX_CHGR_TYPE_UNKNOWN;
	chip->temp_level = 0;
	chip->thermal_levels = 0;
	memset(&chip->batt_st, 0, sizeof(struct battery_st));

	rc = p922x_hw_init(chip);
	if (rc < 0) {
		p922x_err(chip, "Failed to init hw, rc=%d\n", rc);
		goto free_psy;
	}

	rc = devm_request_threaded_irq(&client->dev, client->irq, NULL,
			p922x_irq_handler,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			client->dev.driver->name, chip);
	if (rc) {
		p922x_err(chip, "Failed irq=%d request rc = %d\n",
				client->irq, rc);
		goto free_psy;
	}
	enable_irq_wake(client->irq);

	p922x_irq_handler(client->irq, chip);

	create_debugfs_entries(chip);

	create_sysfs_entries(chip);

	pr_info("p922x wireless receiver initialized successfully\n");

	return 0;

free_psy:
	power_supply_unregister(chip->wls_psy);
free_mem:
	devm_kfree(chip->dev, chip);
	return rc;
}

static int p922x_charger_remove(struct i2c_client *client)
{
	struct p922x_charger *chip = i2c_get_clientdata(client);

	remove_sysfs_entries(chip);

	cancel_delayed_work_sync(&chip->heartbeat_work);

	mutex_destroy(&chip->send_pkg_lock);
	debugfs_remove_recursive(chip->debug_root);
	return 0;
}

static void p922x_shutdown(struct i2c_client *client)
{
	struct p922x_charger *chip = i2c_get_clientdata(client);

	p922x_dbg(chip, PR_MOTO, "Wireless charger shutdown\n");
}

static int p922x_suspend(struct device *dev)
{
	struct p922x_charger *chip = dev_get_drvdata(dev);

	if (regulator_is_enabled(chip->vdd_i2c_vreg))
		return regulator_disable(chip->vdd_i2c_vreg);

	return 0;
}

static int p922x_resume(struct device *dev)
{
	struct p922x_charger *chip = dev_get_drvdata(dev);

	if (!regulator_is_enabled(chip->vdd_i2c_vreg))
		return regulator_enable(chip->vdd_i2c_vreg);

	return 0;
}

static const struct dev_pm_ops p922x_pm_ops = {
	.suspend	= p922x_suspend,
	.resume		= p922x_resume,
};

static const struct of_device_id p922x_match_table[] = {
	{ .compatible = "idt,p922x-charger",},
	{ },
};

static const struct i2c_device_id p922x_charger_id[] = {
	{"p922x-charger", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, p922x_charger_id);

static struct i2c_driver p922x_charger_driver = {
	.driver		= {
		.name		= "p922x-charger",
		.owner		= THIS_MODULE,
		.of_match_table	= p922x_match_table,
		.pm		= &p922x_pm_ops,
	},
	.probe		= p922x_charger_probe,
	.remove		= p922x_charger_remove,
	.id_table	= p922x_charger_id,
	.shutdown	= p922x_shutdown,
};

module_i2c_driver(p922x_charger_driver);

MODULE_DESCRIPTION("p922x charger");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:p922x-charger");
