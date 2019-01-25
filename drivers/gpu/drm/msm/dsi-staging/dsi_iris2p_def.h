
#ifndef DSI_IRIS2P_DEF_H
#define DSI_IRIS2P_DEF_H


#include "dsi_panel.h"
#include "dsi_display.h"
#include "dsi_iris2p_reg.h"
#include "dsi_iris2p_device.h"
#include <drm/drm_mipi_dsi.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/firmware.h>
#include <linux/gcd.h>

#include <video/mipi_display.h>

#define IRIS_ABYPASS_LOWPOWER

#define READ_CMD_ENABLE
#define ENABLE_IRIS_I2C_READ
//#define I2C_ENABLE
//#define EFUSE_WRITE       (1)
#define NEW_MIPI_DSI_MSG  (1) //kernel/msm-4.9/include/drm/drm_mipi_dsi.h struct mipi_dsi_msg

#define ALWAYS_AP_UPDATE_CMLUT

#define WAKEUP_TIME 50
#define CMD_PROC 0
#define MCU_PROC 1
#define INIT_WAIT 0


#define CMD_PKT_SIZE 512
#define GRCP_HEADER 16
#define INIT_CMD_NUM 3

/* MEMC */
#define LEVEL_MAX   (5)
#define RATIO_NUM  (8)
#define FI_REPEATCF_TH		16

#define IRIS_DTG_HRES_SETTING 768
#define IRIS_DTG_VRES_SETTING 2048
#define IMG1080P_HSIZE   (1920)
#define IMG1080P_VSIZE   (1080)
#define IMG720P_HSIZE    (1280)
#define IMG720P_VSIZE    (720)

/* firmware download */
#define FW_COL_CNT  81
#define FW_DW_CMD_CNT  2
#define DSI_DMA_TX_BUF_SIZE	SZ_512K
#define DCS_WRITE_MEM_START 0x2C
#define DCS_WRITE_MEM_CONTINUE 0x3C
#define APP_FIRMWARE_NAME	"iris2p.fw"
#define LUT_FIRMWARE_NAME	"iris2p_ccf1.fw"
#define GAMMA_FIRMWARE_NAME	"iris2p_ccf2.fw"

#define IRIS_CM_LUT_LENGTH		5741
#define IRIS_CMLUT_2800K_ADDR	0x1e000
#define IRIS_CMLUT_6500K_ADDR	0x26000
#define IRIS_CMLUT_8000K_ADDR	0x2e000
#define IRIS_CCT_MIN_VALUE		2800
#define IRIS_CCT_MAX_VALUE		9300
#define IRIS_X_6500K			3128
#define IRIS_X_8000K			2831
#define IRIS_X_2800K			4481
#define IRIS_CMLUT_IP_ADDR		0xf1568000
#define IRIS_CMLUT_GRCP_PKT_SIZE  220
#define IRIS_CMLUT_GRCP_PKT_NUM	1
#define IRIS_GAMMA_TABLE_ADDR  0x16000
#define IRIS_GAMMA_TABLE_LENGTH 65

#define IRIS_RUN_STATUS 0xf0100000
#define IRIS_INT_STATUS 0xf0120004

#define IRIS_NO_ABYPASS                         0x00
#define IRIS_IN_ABYPASS                         0xab00
#define IRIS_IN_ABYPASS_HBM_ENABLED             0xabcb
#define IRIS_IN_ONE_WIRED_ABYPASS               0xabcd

#define IRIS_HDR_LEVEL_MIN    0x14
#define IRIS_HDR_LEVEL_MAX   0x50


#define PWIL_TAG(a, b, c, d) d, c, b, a
#define PWIL_U32(x) \
	(__u8)(((x)	) & 0xff), \
	(__u8)(((x) >>  8) & 0xff), \
	(__u8)(((x) >> 16) & 0xff), \
	(__u8)(((x) >> 24) & 0xff)

#define PWIL_U16(x) \
	(__u8)(((x)	) & 0xff), \
	(__u8)(((x) >> 8 ) & 0xff)


enum mipi_rx_mode {
	MCU_VIDEO = 0,
	MCU_CMD = 1,
	PWIL_VIDEO = 2,
	PWIL_CMD = 3,
	BYPASS_VIDEO = 4,
	BYPASS_CMD = 5,
};

enum romcode_ctrl {
	CONFIG_DATAPATH = 1,
	ENABLE_DPORT = 2,
	ITCM_COPY = 4,
	REMAP = 8,
};

enum res_ratio{
	ratio_4to3 = 1,
	ratio_16to10,
	ratio_16to9,
};

enum pwil_mode {
	PT_MODE,
	RFB_MODE,
	BIN_MODE,
	FRC_MODE,
};


enum iris_mode {
	IRIS_PT_MODE = 0,
	IRIS_RFB_MODE,
	IRIS_FRC_MODE,
	IRIS_BYPASS_MODE,
	IRIS_PT_PRE,
	IRIS_RFB_PRE,
	IRIS_FRC_PRE,
};

enum iris_frc_prepare_state {
	IRIS_FRC_PATH_PROXY = 0x0,
	IRIS_FRC_WAIT_PREPARE_DONE,
	IRIS_FRC_PRE_DONE,
	IRIS_FRC_PRE_TIMEOUT,
};

enum iris_mode_rfb2frc_state {
	IRIS_RFB_FRC_SWITCH_COMMAND = 0x00,
	IRIS_RFB_FRC_SWITCH_DONE,
};

enum iris_mode_frc2rfb_state {
	IRIS_FRC_RFB_SWITCH_COMMAND = 0x00,
	IRIS_FRC_RFB_DATA_PATH,
	IRIS_FRC_RFB_SWITCH_DONE,
};

enum iris_rfb_prepare_state {
	IRIS_RFB_PATH_PROXY = 0x0,
	IRIS_RFB_WAIT_PREPARE_DONE,
	IRIS_RFB_PRE_DONE,
};

enum iris_frc_cancel_state {
	IRIS_FRC_CANCEL_PATH_PROXY = 0x0,
	IRIS_FRC_CANCEL_DONE,
};

enum iris_pt_prepare_state {
	IRIS_PT_PATH_PROXY = 0x0,
	IRIS_PT_WAIT_PREPARE_DONE = 0x01,
	IRIS_PT_PRE_DONE = 0x02,
};

enum iris_mode_pt2rfb_state {
	IRIS_PT_RFB_SWITCH_COMMAND = 0x0,
	IRIS_PT_RFB_DATA_PATH = 0x01,
	IRIS_PT_RFB_SWITCH_DONE = 0x02,
};

enum iris_mode_rfb2pt_state {
	IRIS_RFB_PT_SWITCH_COMMAND = 0x0,
	IRIS_RFB_PT_DATA_PATH = 0x01,
	IRIS_RFB_PT_SWITCH_DONE = 0x02,
};

enum cmlut_buffer_index {
    CMI_NTSC_2800 = 0,
    CMI_NTSC_6500,
    CMI_NTSC_6500_C51_ENDIAN,
    CMI_NTSC_8000,
    CMI_SRGB_2800,
    CMI_SRGB_6500,
    CMI_SRGB_6500_C51_ENDIAN,
    CMI_SRGB_8000,
    CMI_P3_2800,
    CMI_P3_6500,
    CMI_P3_6500_C51_ENDIAN,
    CMI_P3_8000,
    CMI_HDR_2800,
    CMI_HDR_6500_C51_ENDIAN,
    CMI_SDR_GAME_2800,
    CMI_SDR_GAME_6500,
    CMI_SDR_GAME_6500_C51_ENDIAN,
    CMI_SDR_GAME_8000,
    CMI_SDR_VIDEO_2800,
    CMI_SDR_VIDEO_6500,
    CMI_SDR_VIDEO_6500_C51_ENDIAN,
    CMI_SDR_VIDEO_8000,
    CMI_CALCULATION,
    CMI_TOTAL, //24
};

enum cm3d_index {
	CM_NTSC = 0,
	CM_sRGB,
	CM_DCI_P3,
	CM_HDR,
	CM_SDR_GAME,
	CM_SDR_VIDEO,
};

struct iris_mipi_tx_cmd_hdr {
	u8 dtype;
	u8 len[2];
	u8 ecc;
};

union iris_mipi_tx_cmd_header {
	struct iris_mipi_tx_cmd_hdr	stHdr;
	u32 hdr32;
};

union iris_mipi_tx_cmd_payload {
	u8 p[4];
	u32 pld32;
};

enum iris_onewired_cmd {
	RX0_POWER_UP = 1,
	BYPASS_MODE_CHANGE = 2,
	RX0_POWER_DOWN = 3,
	RX0_RESET = 4,
	FORCE_WORK_MODE_SWITCH = 5,
	REVERSED = 6,//RESERVED?
};

enum iris_abypass_status {
	ANALOG_BYPASS_MODE = 0,
	PASS_THROUGH_MODE,
};

enum iris_abypass_switch_state {
	PASS_THROUGH_STATE = 0,
	MCU_STOP_ENTER_STATE,
	TTL_CMD_BYPASS_STATE,
	ANALOG_BYPASS_ENTER_STATE,
	RX0_POWER_DOWN_STATE,
	ANALOG_BYPASS_STATE,
	RX0_POWER_UP_STATE,
	RFB_STATE,
	MCU_STOP_EXIT_STATE,
	ANALOG_BYPASS_EXIT_STATE,
	LOW_POWER_ENTER_STATE,
	LOW_POWER_EXIT_STATE,
	CHANGE_EXT_TE_STATE,
};

enum iris_lp_state {
	LP_STATE_POWER_UP = 0,
	LP_STATE_POWER_UP_PREPARE,
	LP_STATE_POWER_DOWN,
	LP_STATE_POWER_DOWN_PREPARE,
};

enum iris_lp_cmd {
	LP_CMD_LP_ENTER = 1,
	LP_CMD_LP_EXIT,
	LP_CMD_MCU_STOP,
};

// MB2
struct iris_fun_enable {
	u32 reserved0:11;
	u32 true_cut_en:1;		//bit11, Only use in FRC mode
	u32 reserved1:3;
	u32 nrv_drc_en:1;		//bit15, Only use in FRC mode
	u32 frc_buf_num:1;		//no used
	u32 phase_en:1;		//no used
	u32 pp_en:1;		//bit18
	u32 reserved2:1;
	u32 use_efifo_en:1;	//bit20
	u32 psr_post_sel:1;	//bit21
	u32 reserved3:1;
	u32 frc_data_format:1;	//bit23, 0: YUV444 1: YUV422
	u32 capt_bitwidth:1;	//bit24, 0: 8bit; 1: 10bit
	u32 psr_bitwidth:1;	//bit25, 0: 8bit; 1: 10bit
	u32 dbc_lce_en:1;		//bit26
	u32 dpp_en:1;		//bit27
	u32 reserved4:4;
};


struct iris_grcp_cmd {
	char cmd[CMD_PKT_SIZE];
	int cmd_len;
};

struct iris_work_mode {
	u32 rx_mode:1;			/* 0-video/1-cmd */
	u32 rx_ch:1;			/* 0-single/1-dual */
	u32 rx_dsc:1;			/* 0-non DSC/1-DSC */
	u32 rx_pxl_mode:1;		/*interleave/left-right */
	u32 reversed0:12;

	u32 tx_mode:1;			/* 0-video/1-cmd */
	u32 tx_ch:1;			/* 0-single/1-dual */
	u32 tx_dsc:1;			/* 0-non DSC/1-DSC */
	u32 tx_pxl_mode:1;		/* interleave/left-right */
	u32 reversed1:12;
};

struct iris_timing_info {
	u16 hfp;
	u16 hres;
	u16 hbp;
	u16 hsw;
	u16 vfp;
	u16 vres;
	u16 vbp;
	u16 vsw;
	u16 fps;
};

struct iris_pll_setting {
	u32 ppll_ctrl0;
	u32 ppll_ctrl1;
	u32 ppll_ctrl2;

	u32 dpll_ctrl0;
	u32 dpll_ctrl1;
	u32 dpll_ctrl2;

	u32 mpll_ctrl0;
	u32 mpll_ctrl1;
	u32 mpll_ctrl2;

	u32 txpll_div;
	u32 txpll_sel;
	u32 reserved;
};

struct iris_clock_source {
	u8 sel;
	u8 div;
	u8 div_en;
};

struct iris_clock_setting {
	struct iris_clock_source dclk;
	struct iris_clock_source inclk;
	struct iris_clock_source mcuclk;
	struct iris_clock_source pclk;
	struct iris_clock_source mclk;
	struct iris_clock_source escclk;
};

struct iris_mipirx_setting {
	u32 mipirx_dsi_functional_program;
	u32 mipirx_eot_ecc_crc_disable;
	u32 mipirx_data_lane_timing_param;
};

struct iris_mipitx_setting {
	u32 mipitx_dsi_tx_ctrl;
	u32 mipitx_hs_tx_timer;
	u32 mipitx_bta_lp_timer;
	u32 mipitx_initialization_reset_timer;
	u32 mipitx_dphy_timing_margin;
	u32 mipitx_lp_timing_para;
	u32 mipitx_data_lane_timing_param;
	u32 mipitx_clk_lane_timing_param;
	u32 mipitx_dphy_pll_para;
	u32 mipitx_dphy_trim_1;
};

struct iris_hw_setting {
	struct iris_pll_setting pll_setting;
	struct iris_clock_setting clock_setting;
	struct iris_mipirx_setting rx_setting;
	struct iris_mipitx_setting tx_setting;
};

struct iris_dtg_setting {
	u32 dtg_ctrl;
	u32 dtg_ctrl_1;
	u32 evs_dly;
	u32 evs_new_dly;
	u32 dtg_delay;
	u32 te_ctrl;
	u32 te_ctrl_1;
	u32 te_ctrl_2;
	u32 te_ctrl_3;
	u32 te_ctrl_4;
	u32 te_ctrl_5;
	u32 te_dly;
	u32 te_dly_1;
	u32 dvs_ctrl;
	u32 vfp_ctrl_0;
	u32 vfp_ctrl_1;
};

struct iris_fw_info {
	u32 app_fw_size;
	u32 app_fw_state;
	u32 app_cnt;
};

struct iris_lut_info {
	u32 *cmlut[CMI_TOTAL];
	u32 lut_fw_state;
};

struct iris_gamma_info {
	u32 gamma_fw_size;
	u32 gamma_fw_state;
};

struct iris_mode_state {
	int current_mode;
	int sf_notify_mode;
	int sf_mode_switch_trigger;
	int start_mode;
	int next_mode;
	int target_mode;
	int kickoff_cnt;
	u32 frc_path;
	u32 rfb_path;
	u32 pt_path;
};

struct iris_frc_setting {
	int in_fps;
	u32 frcc_cmd_th;
	u32 frcc_reg8;
	u32 frcc_reg16;
	u32 in_ratio;
	u32 out_ratio;
	u32 scale;
	u32 low_delay;
};

struct demo_win_info {
	int   startx;    //12bits width
	int   starty;    //12bits width
	int   endx;      //12bits width
	int   endy;     //12bits width
	int   color;      //Y U V 8bits width,      Y[7:0], U[15:8], V[23:16]
	int   borderwidth;    ///3bits width
	int   fi_demo_en;          //bool
	int   sharpness_en;   //bool
	int   cm_demo_en;   //bool
};

struct fi_demo_win {
	int   startx;    //12bits width panel position
	int   starty;    //12bits width panel position
	int   endx;      //12bits width panel position
	int   endy;     //12bits width panel position
	int   borderwidth;    ///3bits width
	int   colsize;
	int   color;
	int   rowsize;
	int   modectrl;
};

struct cm_demo_win {
	int   startx;    //12bits width panel position
	int   starty;    //12bits width panel position
	int   endx;      //12bits width panel position
	int   endy;     //12bits width panel position
	int   cm_demo_en;   //bool
};

struct peaking_demo_win {
	int   startx;    //12bits width panel position
	int   starty;    //12bits width panel position
	int   endx;      //12bits width panel position
	int   endy;     //12bits width panel position
	int   sharpness_en;   //bool
};

// MB3
struct iris_pq_setting {
	u32 peaking:4;
	u32 sharpness:4;
	u32 memcdemo:4;
	u32 gamma:2;
	u32 memclevel:2;
	u32 contrast:8;
	u32 cinema_en:1;
	u32 peakingdemo:4;
	u32 reserved:2;
	u32 update:1;
};

// MB5
struct iris_dbc_setting {
	u32 update:1;
	u32 reserved:7;
	u32 brightness:7;
	u32 ext_pwm:1;
	u32 cabcmode:4;
	u32 dlv_sensitivity:12;
};

struct iris_memc_setting {
	int update;
	u8	level;
	u32 value;
};

struct iris_lce_setting {
	u32 mode:4;
	u32 mode1level:4;
	u32 mode2level:4;
	u32 demomode:4;
	u32 graphics_detection:1;
	u32 reserved:14;
	u32 update:1;
};

struct iris_cm_setting {
	u32 cm6axes:3;
	u32 cm3d:5;
	u32 demomode:3;
	u32 ftc_en:1;
	u32 color_temp_en:1;
	u32 color_temp:9;
	u32 color_temp_adjust:6;
	u32 sensor_auto_en:1;
	u32 reserved:2;
	u32 update:1;
};

struct iris_hdr_setting {
	u32 hdrlevel:8;
	u32 hdren:1;
	u32 reserved:22;
	u32 update:1;
};

struct iris_lux_value {
	u32 luxvalue:16;
	u32 reserved:15;
	u32 update:1;
};

struct iris_cct_value {
	u32 cctvalue:16;
	u32 reserved:15;
	u32 update:1;
};

struct iris_reading_mode {
	u32 readingmode:8;
	u32 reserved:23;
	u32 update:1;
};

struct iris_sdr_setting {
	u32 sdr2hdr;
};

struct iris_color_adjust {
	u32 saturation:8;
	u32 hue:8;
	u32 Contrast:8;
	u32 reserved:7;
	u32 update:1;
};

struct iris_oled_brightness {
	u32 brightness:31;
	u32 update:1;
};

struct feature_setting {
	struct iris_pq_setting pq_setting;
	struct iris_dbc_setting dbc_setting;
	struct iris_memc_setting lp_memc_setting;
	struct iris_lce_setting lce_setting;
	struct iris_cm_setting cm_setting;
	struct iris_hdr_setting hdr_setting;
	struct iris_lux_value lux_value;
	struct iris_cct_value cct_value;
	struct iris_reading_mode reading_mode;
	struct iris_sdr_setting sdr_setting;
	struct iris_color_adjust color_adjust;
	struct iris_oled_brightness oled_brightness;
	u8 gamma_enable; // todo
};

struct iris_setting_update {
	uint32_t demo_win_fi:1;
	uint32_t pq_setting:1;
	uint32_t dbc_setting:1;
	uint32_t lp_memc_setting:1;
	uint32_t color_adjust:1;
	uint32_t lce_setting:1;
	uint32_t cm_setting:1;
	uint32_t cmd_setting:1;
	uint32_t lux_value:1;
	uint32_t cct_value:1;
	uint32_t reading_mode:1;
	uint32_t hdr_setting:1;
	uint32_t demo_win_peaking:1;
	uint32_t demo_win_cm:1;
	uint32_t gamma_table:1;
	uint32_t sdr_setting:1;
	uint32_t oled_brightness:1;
	uint32_t reserved:15;
};

struct iris_power_status {
	int low_power_state;
};

struct iris_abypass_ctrl {
	bool analog_bypass_enable;
	bool abypass_status;
	bool abypass_debug;
	bool abypass_to_pt_enable;
	bool pt_to_abypass_enable;
	int abypass_switch_state;
	u32 base_time;
	int frame_delay;
};

struct system_context_info {
	int backlight_on;
	int iris_stay_in_abypass_en;
};

struct dsi_iris_info {
	struct iris_work_mode work_mode;
	struct iris_timing_info input_timing;
	struct iris_timing_info output_timing;
	struct iris_hw_setting hw_setting;
	struct iris_dtg_setting dtg_setting;
	struct iris_fw_info firmware_info;
	struct iris_lut_info lut_info;
	struct iris_gamma_info gamma_info;
	struct iris_mode_state mode_state;
	struct iris_frc_setting frc_setting;
	struct fi_demo_win fi_demo_win_info;
	struct peaking_demo_win peaking_demo_win_info;
	struct cm_demo_win cm_demo_win_info;
	struct feature_setting chip_setting;
	struct feature_setting user_setting;
	struct iris_setting_update settint_update;
	struct iris_power_status power_status;
	struct iris_abypass_ctrl  abypass_ctrl;
	struct mutex config_mutex;
	spinlock_t iris_lock;
	struct mutex dsi_send_mutex;
	struct system_context_info context_info;
};

/*added multiple panel support to distinguish lcd module*/
enum mul_lcd_module {
	LCM_SAMSUNG,
	LCM_KINGDISPLAY,
};

extern bool record_first_backlight;
extern bool record_first_kickoff;
extern ktime_t ktime_first_backlight;
extern ktime_t ktime_first_kickoff;

extern int iris_full_functioning;
extern int iris_in_cont_splash_displaying;

extern struct dsi_iris_info iris_info;
extern char iris_read_cmd_buf[16];
extern struct iris_grcp_cmd grcp_cmd;
extern struct iris_grcp_cmd meta_cmd;
extern struct iris_grcp_cmd gamma_cmd;
extern u16 lp_memc_timing[2];
extern struct dsi_panel *iris_panel;
extern char grcp_header[GRCP_HEADER];
extern u8 *iris_cmlut_buf;
extern u8 *gamma_fw_buf;
extern int  iris_debug_power_opt_disable;
extern int  iris_debug_fw_download_disable;
extern u32  iris_lp_memc_calc(u32 value);
extern u32  iris_get_vtotal(struct iris_timing_info *info);
extern u32  iris_get_htotal(struct iris_timing_info *info);
extern void iris_cmd_reg_add(struct iris_grcp_cmd *pcmd, u32 addr, u32 val);
extern int  iris_dsi_cmds_send(struct dsi_panel *panel,
				struct dsi_cmd_desc *cmds,
				u32 count,
				enum dsi_cmd_set_state state);
extern void iris_pwil_mode_set(struct dsi_panel *panel, u8 mode, enum dsi_cmd_set_state state);
extern int  iris_ioctl_operate_mode(void __user *argp);
extern int  iris_ioctl_operate_conf(void __user *argp);
extern void iris_mode_switch_cmd(struct dsi_panel *panel);
extern void iris_firmware_cmlut_update(u8 *buf, u32 cm3d);
extern void iris_firmware_gamma_table_update(u8 *buf, u32 cm3d);
extern int  iris_configure(u32 type, u32 value);
extern void iris_video_fps_get(u32 frame_rate);
extern void iris_mode_switch_state_reset(void);
extern int  iris_i2c_reg_write(unsigned int addr, unsigned int data);
extern int  iris_i2c_reg_read(unsigned int addr, unsigned int *data);
extern void iris_mipi_reg_write(u32 addr, u32 value, enum dsi_cmd_set_state state);
extern int  iris_mipi_reg_read(u32 addr, u32 *value);
extern void iris_update_frc_configs(void);
extern int  iris_lut_firmware_init(const char *name);
extern int  iris_gamma_firmware_init(const char *name);
extern int  get_lcd_calibrate_data(uint8_t **data, int *size);
extern void trigger_download_ccf_fw(void);
extern void iris_cmlut_table_load(struct dsi_panel *panel);
extern void iris_prepare_for_hdr_switch(struct dsi_panel *panel, int hdr_en);
extern bool irisIsInAbypassMode(void);
extern bool irisIsInAbypassHbmEn(void);
extern void irisSetAbypassMode(int mode);
extern void irisSetTracePrintOpt(int opt);
extern void irisAdjustPanelTE(struct dsi_panel *panel, int bypasscmd, u32 value, enum dsi_cmd_set_state state);
extern u8   irisReadPanelTeConfig(struct dsi_panel *panel, enum dsi_cmd_set_state state);
extern void irisSetTeTweakOpt(int opt);
extern void irisSetDumpOpt(int opt);
extern void irisMonitorPanelTE(struct dsi_panel *panel);
extern void irisResetTeVariables(void);
extern bool irisIsFullyFunctioning(void);
extern void iris_abypass_switch_state_init(int mode);
extern int  iris_pt_to_abypass_switch(struct dsi_panel *panel);
extern int  iris_abypass_to_pt_switch(struct dsi_panel *panel);
extern int  iris_abypass_gpio_init(struct dsi_panel *panel);
extern void iris_one_wired_cmd_send(struct dsi_panel *panel, int cmd);
extern void iris_mipirx_mode_set(struct dsi_panel *panel, int mode, enum dsi_cmd_set_state state);
extern int  iris_abypass_switch_write(unsigned long val);
extern void irisTest(int round, int delay);
extern void irisLowPowerAbypassSwitch(struct dsi_panel *panel, int opt);
extern void irisModeSwitchMonitor(struct dsi_panel *panel);
extern bool irisIsAllowedLpBypassTrace(void);
extern bool iris_sde_encoder_rc_lock(void);
extern bool iris_sde_encoder_rc_unlock(void);
extern u8   iris_mipi_signal_mode_read(struct dsi_panel *panel, enum dsi_cmd_set_state state);
extern void irisSetRecoverOption(int opt);
extern void irisResetPanelTeRegs(void);
extern void iris_feature_setting_init(struct feature_setting *setting);
extern bool irisStayInLowPowerAbypass(void);
extern void irisUserSetLowPowerAbypass(int value);
extern bool irisIsInLowPowerAbypassMode(void);
extern void iris_request_kickoff_count(int count);
extern void iris_hbm_direct_switch(struct dsi_panel *panel, int hbm_en);
extern void iris_resume_panel_brightness(struct dsi_panel *panel);
extern void iris_check_hw_status(void);
extern bool irisIsAllowedPqTrace(void);
extern bool irisIsAbypassSwitchOngoing(void);
extern bool iris_mode_bypass_state_check(void);
extern int iris_mipi_ddb_para_read(struct dsi_panel *panel, enum dsi_cmd_set_state state, char* buf, int size);
extern int iris_i2c_ddb_para_read(char *buf, int size);
extern int iris_i2c_panel_state_read(int reg, u8 *buf);
extern void iris_esd_register_dump(void);
extern bool iris_mode_bypass_state_check(void);
extern u8   iris_mipi_power_mode_read(struct dsi_panel *panel, enum dsi_cmd_set_state state);
extern extern int dsi_iris_pt_power(struct dsi_panel *panel, bool enable);
#endif
