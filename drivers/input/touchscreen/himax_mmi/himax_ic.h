/* Himax Android Driver Sample Code for HX83102 chipset
*
* Copyright (C) 2017 Himax Corporation.
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

#include "himax_platform.h"
#include "himax_common.h"
#include <linux/slab.h>

#define HIMAX_REG_RETRY_TIMES 5

enum fw_image_type {
	fw_image_32k	= 0x01,
	fw_image_48k,
	fw_image_60k,
	fw_image_64k,
	fw_image_124k,
	fw_image_128k,
};

int himax_hand_shaking(struct i2c_client *client);
void himax_set_SMWP_enable(struct i2c_client *client,uint8_t SMWP_enable, bool suspended);
void himax_set_HSEN_enable(struct i2c_client *client,uint8_t HSEN_enable, bool suspended);
void himax_usb_detect_set(struct i2c_client *client,uint8_t *cable_config);
int himax_determin_diag_rawdata(int diag_command);
int himax_determin_diag_storage(int diag_command);
void himax_diag_register_set(struct i2c_client *client, uint8_t diag_command);
void himax_flash_dump_func(struct i2c_client *client, uint8_t local_flash_command, int Flash_Size, uint8_t *flash_buffer);
int himax_chip_self_test(struct i2c_client *client);
void himax_burst_enable(struct i2c_client *client, uint8_t auto_add_4_byte); ////himax_83110_BURST_INC0_EN
void himax_register_read(struct i2c_client *client, uint8_t *read_addr, int read_length, uint8_t *read_data, bool cfg_flag); ////RegisterRead83110
void himax_flash_read(struct i2c_client *client, uint8_t *reg_byte, uint8_t *read_data); ////himax_83110_Flash_Read
void himax_flash_write_burst(struct i2c_client *client, uint8_t * reg_byte, uint8_t * write_data); ////himax_83110_Flash_Write_Burst
void himax_flash_write_burst_lenth(struct i2c_client *client, uint8_t *reg_byte, uint8_t *write_data, int length); ////himax_83110_Flash_Write_Burst_lenth
void himax_register_write(struct i2c_client *client, uint8_t *write_addr, int write_length, uint8_t *write_data, bool cfg_flag); ////RegisterWrite83110
bool himax_sense_off(struct i2c_client *client); ////himax_83110_SenseOff
void himax_interface_on(struct i2c_client *client); ////himax_83110_Interface_on
bool wait_wip(struct i2c_client *client, int Timing);
void himax_sense_on(struct i2c_client *client, uint8_t FlashMode); ////himax_83110_SenseOn
void himax_chip_erase(struct i2c_client *client); ////himax_83110_Chip_Erase
bool himax_block_erase(struct i2c_client *client); ////himax_83110_Block_Erase
bool himax_sector_erase(struct i2c_client *client, int start_addr); ////himax_83110_Sector_Erase
void himax_sram_write(struct i2c_client *client, uint8_t *FW_content); ////himax_83110_Sram_Write
bool himax_sram_verify(struct i2c_client *client, uint8_t *FW_File, int FW_Size); ////himax_83110_Sram_Verify
void himax_flash_programming(struct i2c_client *client, uint8_t *FW_content, int FW_Size); ////himax_83110_Flash_Programming
bool himax_check_chip_version(struct i2c_client *client); ////himax_83110_CheckChipVersion
int himax_check_CRC(struct i2c_client *client, int mode); ////himax_83110_Check_CRC
bool Calculate_CRC_with_AP(unsigned char *FW_content , int CRC_from_FW, int mode);
int fts_ctpm_fw_upgrade_with_sys_fs_32k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref);
int fts_ctpm_fw_upgrade_with_sys_fs_60k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref);
int fts_ctpm_fw_upgrade_with_sys_fs_64k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref);
int fts_ctpm_fw_upgrade_with_sys_fs_124k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref);
int fts_ctpm_fw_upgrade_with_sys_fs_128k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref);
void himax_touch_information(struct i2c_client *client);
int  himax_read_i2c_status(struct i2c_client *client);
int  himax_read_ic_trigger_type(struct i2c_client *client);
void himax_read_FW_ver(struct i2c_client *client);
bool himax_ic_package_check(struct i2c_client *client);
void himax_power_on_init(struct i2c_client *client);
bool himax_read_event_stack(struct i2c_client *client, uint8_t *buf, uint8_t length);
void himax_get_DSRAM_data(struct i2c_client *client, uint8_t *info_data);
bool himax_calculateChecksum(struct i2c_client *client, bool change_iref);
bool himax_program_reload(struct i2c_client *client);//5442 Program_Reload
void himax_set_reload_cmd(uint8_t *write_data, int idx, uint32_t cmd_from, uint32_t cmd_to, uint32_t cmd_beat);//5442 SetReloadCmd
void himax_flash_page_write(struct i2c_client *client, uint8_t *write_addr, uint8_t *write_data);//5442 Flash_Page_Write
uint32_t himax_XX42_check_CRC(struct i2c_client *client, uint8_t *start_addr, int reload_length);//5442 XX42_Check_CRC
uint8_t himax_read_DD_status(uint8_t *cmd_set, uint8_t *tmp_data);
int himax_read_FW_status(uint8_t *state_addr, uint8_t *tmp_addr);
void himax_resume_ic_action(struct i2c_client *client);
void himax_suspend_ic_action(struct i2c_client *client);

//ts_work
int cal_data_len(int raw_cnt_rmd, int HX_MAX_PT, int raw_cnt_max);
bool diag_check_sum(struct himax_report_data *hx_touch_data); //return checksum value
void diag_parse_raw_data(struct himax_report_data *hx_touch_data,int mul_num, int self_num,uint8_t diag_cmd, int16_t *mutual_data, int16_t *self_data);