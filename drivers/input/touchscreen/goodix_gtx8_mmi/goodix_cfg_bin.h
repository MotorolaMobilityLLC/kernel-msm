#ifndef _GOODIX_CFG_BIN_H_
#define _GOODIX_CFG_BIN_H_

#include "goodix_ts_core.h"

#define TS_DEFAULT_CFG_BIN "goodix_cfg_group2.bin"
#define TS_CFG_BIN_HEAD_LEN (sizeof(struct goodix_cfg_bin_head) + 6)
#define TS_PKG_CONST_INFO_LEN  (sizeof(struct goodix_cfg_pkg_const_info))
#define TS_PKG_REG_INFO_LEN (sizeof(struct goodix_cfg_pkg_reg_info))
#define TS_PKG_HEAD_LEN (TS_PKG_CONST_INFO_LEN + TS_PKG_REG_INFO_LEN)


#pragma pack(1)
struct goodix_cfg_pkg_reg {
	u16 addr;
	u8 reserved1;
	u8 reserved2;
};

struct goodix_cfg_pkg_const_info {
	u32 pkg_len;
	u8 ic_type[15];
	u8 cfg_type;
	u8 sensor_id;
	u8 hw_pid[8];
	u8 hw_vid[8];
	u8 fw_mask[9];
	u8 fw_patch[4];
	u16 x_res_offset;
	u16 y_res_offset;
	u16 trigger_offset;
};

struct goodix_cfg_pkg_reg_info {
	struct goodix_cfg_pkg_reg cfg_send_flag;
	struct goodix_cfg_pkg_reg version_base;
	struct goodix_cfg_pkg_reg pid;
	struct goodix_cfg_pkg_reg vid;
	struct goodix_cfg_pkg_reg sensor_id;
	struct goodix_cfg_pkg_reg fw_mask;
	struct goodix_cfg_pkg_reg fw_status;
	struct goodix_cfg_pkg_reg cfg_addr;
	struct goodix_cfg_pkg_reg esd;
	struct goodix_cfg_pkg_reg command;
	struct goodix_cfg_pkg_reg coor;
	struct goodix_cfg_pkg_reg gesture;
	struct goodix_cfg_pkg_reg fw_request;
	struct goodix_cfg_pkg_reg proximity;
	u8 reserved[9];
};

struct goodix_cfg_bin_head {
	u32 bin_len;
	u8 checksum;
	u8 bin_version[4];
	u8 pkg_num;
};

#pragma pack()

struct goodix_cfg_package {
	struct goodix_cfg_pkg_const_info cnst_info;
	struct goodix_cfg_pkg_reg_info reg_info;
	const u8 *cfg;
	u32 pkg_len;
};



struct goodix_cfg_bin {
	unsigned char *bin_data;
	unsigned int bin_data_len;
	struct goodix_cfg_bin_head head;
	struct goodix_cfg_package *cfg_pkgs;
};


int goodix_cfg_bin_proc(void *data);

int goodix_parse_cfg_bin(struct goodix_cfg_bin *cfg_bin);

int goodix_get_reg_and_cfg(struct goodix_ts_device *ts_dev,
	struct goodix_cfg_bin *cfg_bin);

int goodix_read_cfg_bin(struct goodix_ts_device *ts_dev, struct goodix_cfg_bin *cfg_bin);

int goodix_read_cfg_bin_from_dts(struct device_node *node,
	struct goodix_cfg_bin *cfg_bin);

void goodix_cfg_pkg_leToCpu(struct goodix_cfg_package *pkg);

int goodix_start_cfg_bin(struct goodix_ts_core *ts_core);

void goodix_release_fb_notifier(struct goodix_ts_core *core_data);

#endif
