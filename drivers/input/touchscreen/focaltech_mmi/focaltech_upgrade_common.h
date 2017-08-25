#ifndef __LINUX_FOCALTECH_UPGRADE_COMMON_H__
#define __LINUX_FOCALTECH_UPGRADE_COMMON_H__

#include <linux/i2c.h>
#include <linux/types.h>
#include "focaltech_flash.h"

#define FTS_MAX_TRIES				5
#define FTS_RETRY_DLY				20
#define FTS_MAX_WR_BUF				10
#define FTS_MAX_RD_BUF				2
#define FTS_FW_PKT_META_LEN			6
#define FTS_FW_PKT_DLY_MS			20
#define FTS_FW_LAST_PKT				0x6ffa
#define FTS_EARSE_DLY_MS			100
#define FTS_55_AA_DLY_NS			5000
#define FTS_CAL_START				0x04
#define FTS_CAL_FIN				0x00
#define FTS_CAL_STORE				0x05
#define FTS_CAL_RETRY				100
#define FTS_REG_CAL				0x00
#define FTS_CAL_MASK				0x70
#define FTS_BLOADER_SIZE_OFF			12
#define FTS_BLOADER_NEW_SIZE			30
#define FTS_DATA_LEN_OFF_OLD_FW			8
#define FTS_DATA_LEN_OFF_NEW_FW			14
#define FTS_FINISHING_PKT_LEN_OLD_FW		6
#define FTS_FINISHING_PKT_LEN_NEW_FW		12
#define FTS_MAGIC_BLOADER_Z7			0x7bfa
#define FTS_MAGIC_BLOADER_LZ4			0x6ffa
#define FTS_MAGIC_BLOADER_GZF_30		0x7ff4
#define FTS_MAGIC_BLOADER_GZF			0x7bf4
#define FTS_REG_ECC				0xCC
#define FTS_RST_CMD_REG2			0xBC
#define FTS_READ_ID_REG				0x90
#define FTS_ERASE_APP_REG			0x61
#define FTS_ERASE_PARAMS_CMD			0x63
#define FTS_FW_WRITE_CMD			0xBF
#define FTS_REG_RESET_FW			0x07
#define FTS_RST_CMD_REG1			0xFC
#define LEN_FLASH_ECC_MAX			0xFFFE

#define BL_VERSION_LZ4				0
#define BL_VERSION_Z7				1
#define BL_VERSION_GZF				2

#define FTS_PACKET_LENGTH			128
#define FTS_SETTING_BUF_LEN			128

#define FTS_UPGRADE_LOOP			30
#define FTS_MAX_POINTS_2			2
#define FTS_MAX_POINTS_5			5
#define FTS_MAX_POINTS_10			10
#define AUTO_CLB_NEED				1
#define AUTO_CLB_NONEED				0
#define FTS_UPGRADE_AA				0xAA
#define FTS_UPGRADE_55				0x55

/* Register Address */
#define FTS_REG_INT_CNT				0x8F
#define FTS_REG_FLOW_WORK_CNT			0x91
#define FTS_REG_WORKMODE			0x00
#define FTS_REG_WORKMODE_FACTORY_VALUE		0x40
#define FTS_REG_WORKMODE_WORK_VALUE		0x00
#define FTS_REG_CHIP_ID				0xA3
#define FTS_REG_CHIP_ID2			0x9F
#define FTS_REG_POWER_MODE			0xA5
#define FTS_REG_POWER_MODE_SLEEP_VALUE		0x03
#define FTS_REG_FW_VER				0xA6
#define FTS_REG_VENDOR_ID			0xA8
#define FTS_REG_LCD_BUSY_NUM			0xAB
#define FTS_REG_FACE_DEC_MODE_EN		0xB0
#define FTS_REG_GLOVE_MODE_EN			0xC0
#define FTS_REG_COVER_MODE_EN			0xC1
#define FTS_REG_CHARGER_MODE_EN			0x8B
#define FTS_REG_GESTURE_EN			0xD0
#define FTS_REG_GESTURE_OUTPUT_ADDRESS		0xD3
#define FTS_REG_ESD_SATURATE			0xED

#define FLAGBIT(x)		(0x00000001 << (x))
#define FLAGBITS(x, y)		((0xFFFFFFFF >> (32 - (y) - 1)) << (x))

#define FLAG_ICSERIALS_LEN	5
#define FLAG_IDC_BIT		11

#define IC_SERIALS(x)		((x) & FLAGBITS(0, FLAG_ICSERIALS_LEN - 1))
#define FTS_CHIP_IDC(x)		(((x) & FLAGBIT(FLAG_IDC_BIT)) == \
					FLAGBIT(FLAG_IDC_BIT))

#define FTS_DEBUG		pr_debug
#define FTS_ERROR		pr_err
#define FTS_INFO		pr_info
#define FTS_FUNC_ENTER()	pr_debug("[FTS]%s: Enter\n", __func__)
#define FTS_FUNC_EXIT()		pr_debug("[FTS]%s: Exit(%d)\n", __func__, \
								__LINE__)

enum ENUM_APP_INFO {
	APP_LEN		= 0x00,
	APP_LEN_NE	= 0x02,
	APP_P1_ECC	= 0x04,
	APP_P1_ECC_NE	= 0x06,
	APP_P2_ECC	= 0x08,
	APP_P2_ECC_NE	= 0x0A,
	APP_LEN_H	= 0x12,
	APP_LEN_H_NE	= 0x14,
	APP_BLR_ID	= 0x1C,
	APP_BLR_ID_NE	= 0x1D,
	PBOOT_ID_H	= 0x1E,
	PBOOT_ID_L	= 0x1F
};

enum FW_STATUS {
	FTS_RUN_IN_ERROR,
	FTS_RUN_IN_APP,
	FTS_RUN_IN_ROM,
	FTS_RUN_IN_PRAM,
	FTS_RUN_IN_BOOTLOADER
};

struct ft_chip_t {
	unsigned long type;
	unsigned char chip_idh;
	unsigned char chip_idl;
	unsigned char rom_idh;
	unsigned char rom_idl;
	unsigned char pramboot_idh;
	unsigned char pramboot_idl;
	unsigned char bootloader_idh;
	unsigned char bootloader_idl;
	unsigned long chip_type;
};

struct fts_upgrade_fun {
	int (*get_app_bin_file_ver)(const char *);
	int (*upgrade_with_app_bin_file)(struct i2c_client *, const char *);
	int (*upgrade_with_lcd_cfg_bin_file)(struct i2c_client *, const char *);
};

extern int fts_i2c_read(struct i2c_client *client,
				char *writebuf, int writelen,
				char *readbuf, int readlen);
extern int fts_i2c_write(struct i2c_client *client,
				char *writebuf, int writelen);
extern int fts_i2c_write_reg(struct i2c_client *client,
				u8 addr, const u8 val);
extern int fts_i2c_read_reg(struct i2c_client *client, u8 addr, u8 *val);
extern enum FW_STATUS fts_ctpm_get_pram_or_rom_id
						(struct i2c_client *client);

extern bool fts_ecc_check(const u8 *pbt_buf,
			u32 star_addr, u32 len, u16 ecc_addr);

static inline u16 fts_data_word(const u8 *pbt_buf, u32 addr)
{
	return ((u16)pbt_buf[addr] << 8) + pbt_buf[addr + 1];
}


extern int fts_ctpm_upgrade_idc_init(struct i2c_client *client,
					u32 dw_length);
extern void fts_ctpm_start_pramboot(struct i2c_client *client);
extern int fts_ctpm_start_fw_upgrade(struct i2c_client *client);
extern bool fts_ctpm_check_run_state(struct i2c_client *client,
					int state);
extern int fts_ctpm_upgrade_ecc(struct i2c_client *client,
					u32 startaddr, u32 length);
extern int fts_ctpm_pramboot_ecc(struct i2c_client *client);
extern int fts_ctpm_erase_flash(struct i2c_client *client);
extern int fts_ctpm_write_pramboot_for_idc(struct i2c_client *client,
					u32 length, const u8 *readbuf);
extern int fts_ctpm_write_app_for_idc(struct i2c_client *client,
					u32 length, const u8 *readbuf);
#ifdef CONFIG_TOUCHSCREEN_FOCALTECH_UPGRADE_8716_MMI
extern void ft8716_set_upgrade_function(struct fts_upgrade_fun **curr);
extern void ft8716_set_chip_id(struct ft_chip_t **curr);
#endif
#ifdef CONFIG_TOUCHSCREEN_FOCALTECH_UPGRADE_5X46_MMI
extern void ft5x46_set_upgrade_function(struct fts_upgrade_fun **curr);
extern void ft5x46_set_chip_id(struct ft_chip_t **curr);
#endif

#endif
