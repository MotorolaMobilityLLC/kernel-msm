#include "dsi_iris2p_def.h"
#include "dsi_iris2p_api.h"

#include "sde_encoder.h"
#include "dsi_ctrl_hw.h"
#include "sde_encoder_phys.h"
#include "dsi_drm.h"

struct dsi_iris_info iris_info;
char grcp_header[GRCP_HEADER] = {
	PWIL_TAG('P', 'W', 'I', 'L'),
	PWIL_TAG('G', 'R', 'C', 'P'),
	PWIL_U32(0x3),
	0x00,
	0x00,
	PWIL_U16(0x2),
};
struct iris_grcp_cmd init_cmd[INIT_CMD_NUM];
struct iris_grcp_cmd grcp_cmd;
struct iris_grcp_cmd meta_cmd;
struct iris_grcp_cmd gamma_cmd;

char iris_read_cmd_buf[16];
int iris_debug_dtg_v12 = 1;
u8 *app_fw_buf = NULL;
u8 *lut_fw_buf = NULL;
u8 *gamma_fw_buf = NULL;
u8 *iris_cmlut_buf = NULL;
struct dsi_display *iris_display;
struct drm_device *iris_drm;
struct dsi_panel *iris_panel;

static int iris_panel_te_tweak_enabled = 1;/*code running policy*/

static int iris_bp_mode = 1;//IRIS_NO_ABYPASS;/*code running policy*/

static int iris_dump_dsi_packets_enabled = 0;/*code running policy*/

static int panel_working_c0 = 0;
static int panel_expected_c0 = 0;
static int panelTeCalibrationRequired = 0;

#define KINGDISPLAY_PANEL_TE_INIT_OTP_VALUE (0x88ec)
#define KINGDISPLAY_PANEL_TE_BA_MIN (0x8800)
#define KINGDISPLAY_PANEL_TE_BA_MAX (0x88ff)
#define KINGDISPLAY_PANEL_TE_ONE_STEP_US (17)

//for T3 samsung panel
#define SAMSUNG_PANEL_TE_D1_MIN (0x13)
#define SAMSUNG_PANEL_TE_D1_MAX (0x1f)
#define SAMSUNG_PANEL_TE_INIT_OTP_VALUE (0x1b)
#define SAMSUNG_TE_ONE_STEP_US (56)

#define IRIS_BASIC_TRACE       0x01
#define IRIS_ONOFF_TRACE       0x02
#define IRIS_CONTINUED_TRACE   0x04
#define IRIS_PQ_TRACE          0x08
#define IRIS_LIGHTUP_TRACE     0x10
#define IRIS_DSI_STATS_TRACE   0x20
#define IRIS_MEMC_TRACE        0x40
#define IRIS_LPBYPASS_TRACE    0x80

static int iris_trace_printout_opt = 0;/*code running policy*/

int iris_full_functioning = -1;
int iris_in_cont_splash_displaying = -1;

static int connected_lcd_module = 0; /*added multiple panel support to distinguish lcd modules*/

static int os_bootup_completed = 0;


static bool iris_hw_status_check_allowed = false;/*code running policy*/
static bool iris_hw_status_check_enabled = false;
static bool iris_hw_status_check_started = false;

//#define SAMSUNG_PANEL_TE_ADJUST_ALTERNATIVE

#ifdef SAMSUNG_PANEL_TE_ADJUST_ALTERNATIVE
static int set_te_on_next_wakeup = 0;
#endif

bool record_first_backlight = true;
bool record_first_kickoff = true;
ktime_t ktime_first_backlight;
ktime_t ktime_first_kickoff;

#define RECOVER_RESET_FAILURE_SIMULATED 0x10
#define RECOVER_ENABLE_FAILURE_SIMULATED 0x20
#define RECOVER_INIT_FAILURE_SIMULATED 0x40
#define RECOVER_HOST_DSI_CTRL_FAILURE_SIMULATED 0x80
#define RECOVER_PANEL_INIT_FAILURE_SIMULATED 0x1
#define RECOVER_ACTION_NOW_SIMULATED 0x1000

static int iris_lightup_failure_recovered_simulated = 0;

static int stay_in_lowpower_abypass = 0;

void validatePanelExpectedC0(void);

bool irisStayInLowPowerAbypass(void)
{
	return (stay_in_lowpower_abypass == IRIS_IN_ONE_WIRED_ABYPASS);
}

void irisUserSetLowPowerAbypass(int value)
{
	stay_in_lowpower_abypass = value;
}

bool irisIsFullyFunctioning(void)
{
	return (iris_full_functioning > 0);
}

void irisSetTeTweakOpt(int opt)
{
	iris_panel_te_tweak_enabled = opt;
}

void iris_reset_failure_recover(struct dsi_panel *panel);
int iris_init_failure_recover(struct dsi_panel *panel);
int iris_enable_failure_recover(struct dsi_panel *panel);

void irisSetRecoverOption(int opt)
{
	if (opt&RECOVER_ACTION_NOW_SIMULATED) {
		if (opt&RECOVER_RESET_FAILURE_SIMULATED)
			iris_reset_failure_recover(iris_panel);
		else if (opt&RECOVER_INIT_FAILURE_SIMULATED)
			iris_init_failure_recover(iris_panel);
		else if (opt&RECOVER_ENABLE_FAILURE_SIMULATED)
			iris_enable_failure_recover(iris_panel);
	}
	iris_lightup_failure_recovered_simulated = opt;
}

bool irisIsSimulatedResetFailureRecover(void)
{
	return (iris_lightup_failure_recovered_simulated&RECOVER_RESET_FAILURE_SIMULATED);
}

bool irisIsSimulatedEnableFailureRecover(void)
{
	return (iris_lightup_failure_recovered_simulated&RECOVER_ENABLE_FAILURE_SIMULATED);
}

bool irisIsSimulatedInitFailureRecover(void)
{
	return (iris_lightup_failure_recovered_simulated&RECOVER_INIT_FAILURE_SIMULATED);
}

bool irisIsSimulatedHostDsiCtrlFailureRecover(void)
{
	return (iris_lightup_failure_recovered_simulated&RECOVER_HOST_DSI_CTRL_FAILURE_SIMULATED);
}

bool irisIsSimulatedPanelInitFailureRecover(void)
{
	return (iris_lightup_failure_recovered_simulated&RECOVER_PANEL_INIT_FAILURE_SIMULATED);
}

void irisSetDumpOpt(int opt)
{
	iris_dump_dsi_packets_enabled = opt;
}

void irisSetTracePrintOpt(int opt)
{
	iris_trace_printout_opt = opt;
}

bool irisIsAllowedBasicTrace(void)
{
	return (iris_trace_printout_opt&IRIS_BASIC_TRACE);
}

bool irisIsAllowedPanelOnOffTrace(void)
{
	return (iris_trace_printout_opt&IRIS_ONOFF_TRACE);
}

bool irisIsAllowedContinuedSplashTrace(void)
{
	return (iris_trace_printout_opt&IRIS_CONTINUED_TRACE);
}

bool irisIsAllowedPqTrace(void)
{
        return (iris_trace_printout_opt&IRIS_PQ_TRACE);
}

bool irisIsAllowedMemcTrace(void)
{
	return (iris_trace_printout_opt&IRIS_MEMC_TRACE);
}

bool irisIsAllowedLpBypassTrace(void)
{
	return (iris_trace_printout_opt&IRIS_LPBYPASS_TRACE);
}

bool irisIsAllowedDsiTrace(void)
{
	return (iris_trace_printout_opt&IRIS_DSI_STATS_TRACE);
}

bool isAllowedLightupTrace(void)
{
	return (iris_trace_printout_opt&IRIS_LIGHTUP_TRACE);
}

bool irisIsInAbypassMode(void)
{
	return (iris_bp_mode != IRIS_NO_ABYPASS);
}

bool irisIsInLowPowerAbypassMode(void)
{
	return (iris_info.abypass_ctrl.abypass_status == ANALOG_BYPASS_MODE);
}

bool irisIsInAbypassHbmEn(void)
{
	return (iris_bp_mode == IRIS_IN_ABYPASS_HBM_ENABLED);
}

void irisSetAbypassMode(int mode)
{
	iris_bp_mode = mode;
}

#define DUMP_DATA_FOR_BOOTLOADER
#ifdef DUMP_DATA_FOR_BOOTLOADER
#define DSI_CMDBUF_LIMIT (256)
#define BLOB_MAX  (DSI_CMDBUF_LIMIT*3)
void iris_dump_packet(u8 *data, int size)
{
	char strbuf[BLOB_MAX];
	ssize_t len = 0;
	int blobCount = 0;
	int indexBlob = 0;
	int i = 0;

	if (iris_dump_dsi_packets_enabled <= 0)
		return;

	pr_err("size = %d\n", size);

	blobCount = size/DSI_CMDBUF_LIMIT;
	if (size%DSI_CMDBUF_LIMIT > 0) {
		blobCount += 1;
	}

	pr_err("blobCount = %d\n", blobCount);

	for (indexBlob = 0; indexBlob < blobCount; indexBlob++) {
		len = 0;
		memset(strbuf, 0, sizeof(strbuf));
		for (i = indexBlob*DSI_CMDBUF_LIMIT; i < size; i++) {
			snprintf(strbuf + len, 4, " %02x", data[i]);
			len = strlen(strbuf);
		}
		pr_err("[%d]: string length: %d\n", indexBlob, (int)len);
		pr_err("%s\n", strbuf);
		pr_err("\n");
	}

	pr_err("\n");
}
#define DUMP_PACKET   iris_dump_packet
#else
#define DUMP_PACKET(...)
#endif

void iris_drm_device_handle_get(struct dsi_display *display, struct drm_device *drm_dev)
{
	iris_display = display;
	iris_drm = drm_dev;
}

void iris_workmode_init(struct dsi_iris_info *iris_info, struct dsi_panel *panel)
{
	struct iris_work_mode *pwork_mode = &(iris_info->work_mode);

	/*iris mipirx mode*/
	if (DSI_OP_CMD_MODE == panel->panel_mode) {
		pwork_mode->rx_mode = DSI_OP_CMD_MODE;
	} else {
		pr_info("iris abypass mode\n");
		pwork_mode->rx_mode = DSI_OP_VIDEO_MODE;
	}

	pwork_mode->rx_ch = 0;
	pwork_mode->rx_dsc = 0;
	pwork_mode->rx_pxl_mode = 0;
	/*iris mipitx mode*/
	pwork_mode->tx_mode = DSI_OP_CMD_MODE;
	pwork_mode->tx_ch = 0;
	pwork_mode->tx_dsc = 0;
	pwork_mode->tx_pxl_mode = 1;
}

void iris_timing_init(struct dsi_iris_info *iris_info)
{
	struct iris_timing_info *pinput_timing = &(iris_info->input_timing);
	struct iris_timing_info *poutput_timing = &(iris_info->output_timing);

	pinput_timing->hres = 1080;
	pinput_timing->hfp = 80;
	pinput_timing->hbp = 60;
	pinput_timing->hsw = 48;
	pinput_timing->vres = 2340;
	pinput_timing->vbp = 16;
	pinput_timing->vfp = 20;
	pinput_timing->vsw = 16;
	pinput_timing->fps = 60;

	poutput_timing->hres = 1080;
	poutput_timing->hfp = 80;
	poutput_timing->hbp = 60;
	poutput_timing->hsw = 48;
	poutput_timing->vres = 2340;
	poutput_timing->vbp = 16;
	poutput_timing->vfp = 115;
	poutput_timing->vsw = 16;
	poutput_timing->fps = 60;

	pr_info("iris timing for MOTO panels: %dx%d\n", pinput_timing->hres, pinput_timing->vres);
}

void iris_clock_tree_init(struct dsi_iris_info *iris_info)
{
	struct iris_pll_setting *pll = &(iris_info->hw_setting.pll_setting);
	struct iris_clock_source *clock = &(iris_info->hw_setting.clock_setting.dclk);
	u8 clk_default[6] = {0x01, 0x01, 0x03, 0x01, 0x03, 0x00};
	u8 indx;

	pr_info("iris clock tree for MOTO panels\n");
	pll->ppll_ctrl0 = 0x2;
	pll->ppll_ctrl1 = 0x271101;
	pll->ppll_ctrl2 = 0xF76850;

	pll->dpll_ctrl0 = 0x2002;
	pll->dpll_ctrl1 = 0x271101;
	pll->dpll_ctrl2 = 0x6B404F;

	pll->mpll_ctrl0 = 0x2;
	pll->mpll_ctrl1 = 0x3e0901;
	pll->mpll_ctrl2 = 0x800000;

	pll->txpll_div = 0x0;
	pll->txpll_sel = 0x2;
	pll->reserved = 0x0;

	for (indx = 0; indx < 6; indx++) {
		clock->sel = clk_default[indx] & 0xf;
		clock->div = (clk_default[indx] >> 4) & 0xf;
		clock->div_en = !!clock->div;
		clock++;
	}

}

void iris_mipi_info_init(struct dsi_iris_info *iris_info)
{
	struct iris_mipirx_setting *rx_setting = &(iris_info->hw_setting.rx_setting);
	struct iris_mipitx_setting *tx_setting = &(iris_info->hw_setting.tx_setting);

	pr_info("iris MOTO panels mipi info\n");
	rx_setting->mipirx_dsi_functional_program = 0x64;
	rx_setting->mipirx_eot_ecc_crc_disable = 7;
	rx_setting->mipirx_data_lane_timing_param = 0xff09;

	tx_setting->mipitx_dsi_tx_ctrl = 0xA00C139;
	tx_setting->mipitx_hs_tx_timer = 0x927C00;
	tx_setting->mipitx_bta_lp_timer = 0xFFFF37;
	tx_setting->mipitx_initialization_reset_timer = 0xA8C0780;
	tx_setting->mipitx_dphy_timing_margin = 0x00040401;
	tx_setting->mipitx_lp_timing_para = 0x10010008;
	tx_setting->mipitx_data_lane_timing_param = 0x160D1307;
	tx_setting->mipitx_clk_lane_timing_param = 0x160C3109;
	tx_setting->mipitx_dphy_pll_para = 0x780;
	tx_setting->mipitx_dphy_trim_1 = 0xEDB5380F;
}

void iris_feature_setting_init(struct feature_setting *setting)
{
	setting->pq_setting.peaking = 0;
	setting->pq_setting.sharpness = 0;
	setting->pq_setting.memcdemo = 0;
	setting->pq_setting.gamma = 0;
	setting->pq_setting.memclevel = 3;
	setting->pq_setting.contrast = 0x32;
	setting->pq_setting.cinema_en = 0;
	setting->pq_setting.peakingdemo = 0;
	setting->pq_setting.update = 1;

	setting->dbc_setting.brightness = 0;
	setting->dbc_setting.ext_pwm = 0;
	setting->dbc_setting.cabcmode = 0;
	setting->dbc_setting.dlv_sensitivity = 0;
	setting->dbc_setting.update = 1;

	setting->lp_memc_setting.level = 0;
	setting->lp_memc_setting.value = iris_lp_memc_calc(0);
	setting->lp_memc_setting.update = 1;

	setting->lce_setting.mode = 0;
	setting->lce_setting.mode1level = 1;
	setting->lce_setting.mode2level = 1;
	setting->lce_setting.demomode = 0;
	setting->lce_setting.graphics_detection = 0;
	setting->lce_setting.update = 1;

	setting->cm_setting.cm6axes = 0;
	setting->cm_setting.cm3d = 0;
	setting->cm_setting.demomode = 0;
	setting->cm_setting.ftc_en = 0;
	setting->cm_setting.color_temp_en = 0;
	setting->cm_setting.color_temp = 65;
	setting->cm_setting.color_temp_adjust = 32;
	setting->cm_setting.sensor_auto_en = 0;
	setting->cm_setting.update = 1;

	setting->hdr_setting.hdrlevel = 0;
	setting->hdr_setting.hdren = 0;
	setting->hdr_setting.update = 1;

	setting->lux_value.luxvalue = 0;
	setting->lux_value.update = 1;

	setting->cct_value.cctvalue = 6500;
	setting->cct_value.update = 1;

	setting->reading_mode.readingmode = 0;
	setting->reading_mode.update = 1;
	setting->sdr_setting.sdr2hdr = 0;

	setting->color_adjust.saturation = 128;
	setting->color_adjust.hue = 90;
	setting->color_adjust.Contrast = 50;
	setting->color_adjust.update = 1;

	setting->oled_brightness.brightness = 0;
	setting->oled_brightness.update = 1;

	setting->gamma_enable = 1;
}

void iris_feature_setting_update_check(void)
{
	struct feature_setting *chip_setting = &iris_info.chip_setting;
	struct feature_setting *user_setting = &iris_info.user_setting;
	struct iris_setting_update *settint_update = &iris_info.settint_update;

	iris_feature_setting_init(chip_setting);

	if ((user_setting->pq_setting.peaking != chip_setting->pq_setting.peaking)
		|| (user_setting->pq_setting.sharpness != chip_setting->pq_setting.sharpness)
		|| (user_setting->pq_setting.memcdemo != chip_setting->pq_setting.memcdemo)
		|| (user_setting->pq_setting.gamma != chip_setting->pq_setting.gamma)
		|| (user_setting->pq_setting.memclevel != chip_setting->pq_setting.memclevel)
		|| (user_setting->pq_setting.contrast != chip_setting->pq_setting.contrast)
		|| (user_setting->pq_setting.cinema_en != chip_setting->pq_setting.cinema_en)
		|| (user_setting->pq_setting.peakingdemo != chip_setting->pq_setting.peakingdemo))
		settint_update->pq_setting = true;

	if ((user_setting->dbc_setting.brightness != chip_setting->dbc_setting.brightness)
		|| (user_setting->dbc_setting.ext_pwm != chip_setting->dbc_setting.ext_pwm)
		|| (user_setting->dbc_setting.cabcmode != chip_setting->dbc_setting.cabcmode)
		|| (user_setting->dbc_setting.dlv_sensitivity != chip_setting->dbc_setting.dlv_sensitivity))
		settint_update->dbc_setting = true;

	if ((user_setting->lp_memc_setting.level != chip_setting->lp_memc_setting.level)
		|| (user_setting->lp_memc_setting.value != chip_setting->lp_memc_setting.value))
		settint_update->lp_memc_setting = true;

	if ((user_setting->lce_setting.mode != chip_setting->lce_setting.mode)
		|| (user_setting->lce_setting.mode1level != chip_setting->lce_setting.mode1level)
		|| (user_setting->lce_setting.mode2level != chip_setting->lce_setting.mode2level)
		|| (user_setting->lce_setting.demomode != chip_setting->lce_setting.demomode)
		|| (user_setting->lce_setting.graphics_detection != chip_setting->lce_setting.graphics_detection))
		settint_update->lce_setting = true;

	if ((user_setting->cm_setting.cm6axes != chip_setting->cm_setting.cm6axes)
		|| (user_setting->cm_setting.cm3d != chip_setting->cm_setting.cm3d)
		|| (user_setting->cm_setting.demomode != chip_setting->cm_setting.demomode)
		|| (user_setting->cm_setting.ftc_en != chip_setting->cm_setting.ftc_en)
		|| (user_setting->cm_setting.color_temp_en != chip_setting->cm_setting.color_temp_en)
		|| (user_setting->cm_setting.color_temp != chip_setting->cm_setting.color_temp)
		|| (user_setting->cm_setting.color_temp_adjust != chip_setting->cm_setting.color_temp_adjust)
		|| (user_setting->cm_setting.sensor_auto_en != chip_setting->cm_setting.sensor_auto_en)) {
		if (iris_info.lut_info.lut_fw_state && iris_info.gamma_info.gamma_fw_state)
			settint_update->cm_setting = true;
		else
			settint_update->cm_setting = false;
	}

	if ((user_setting->hdr_setting.hdrlevel != chip_setting->hdr_setting.hdrlevel)
		|| (user_setting->hdr_setting.hdren != chip_setting->hdr_setting.hdren)) {
		user_setting->hdr_setting.hdrlevel = chip_setting->hdr_setting.hdrlevel;
		user_setting->hdr_setting.hdren = chip_setting->hdr_setting.hdren;
	}

	if (user_setting->lux_value.luxvalue != chip_setting->lux_value.luxvalue)
		settint_update->lux_value = true;

	if (user_setting->cct_value.cctvalue != chip_setting->cct_value.cctvalue)
		settint_update->cct_value = true;

	if (user_setting->reading_mode.readingmode != chip_setting->reading_mode.readingmode)
		settint_update->reading_mode = true;

	if (user_setting->sdr_setting.sdr2hdr != chip_setting->sdr_setting.sdr2hdr)
		user_setting->sdr_setting.sdr2hdr = chip_setting->sdr_setting.sdr2hdr;

	if ((user_setting->color_adjust.saturation != chip_setting->color_adjust.saturation)
		|| (user_setting->color_adjust.hue != chip_setting->color_adjust.hue)
		|| (user_setting->color_adjust.Contrast != chip_setting->color_adjust.Contrast))
		settint_update->color_adjust = true;

	if (user_setting->oled_brightness.brightness != chip_setting->oled_brightness.brightness)
		settint_update->oled_brightness = true;

	if (user_setting->gamma_enable != chip_setting->gamma_enable)
		settint_update->gamma_table = true;

	/*todo: update FRC setting*/
	settint_update->pq_setting = true;
	settint_update->lp_memc_setting = true;
}

void iris_cmd_reg_add(struct iris_grcp_cmd *pcmd, u32 addr, u32 val)
{
	*(u32 *)(pcmd->cmd + pcmd->cmd_len) = cpu_to_le32(addr);
	*(u32 *)(pcmd->cmd + pcmd->cmd_len + 4) = cpu_to_le32(val);
	pcmd->cmd_len += 8;
}

void iris_mipitx_reg_config(struct dsi_iris_info *iris_info, struct iris_grcp_cmd *pcmd)
{
	struct iris_work_mode *pwork_mode = &(iris_info->work_mode);
	struct iris_mipitx_setting *tx_setting = &(iris_info->hw_setting.tx_setting);
	struct iris_timing_info *poutput_timing = &(iris_info->output_timing);
	u32 tx_ch, mipitx_addr = IRIS_MIPI_TX_ADDR, dual_ch_ctrl, dsi_tx_ctrl = 0;
	u32 ddrclk_div, ddrclk_div_en;

	ddrclk_div = iris_info->hw_setting.pll_setting.txpll_div;
	ddrclk_div_en = !!ddrclk_div;

	for (tx_ch = 0; tx_ch < (pwork_mode->tx_ch + 1); tx_ch++) {
#ifdef MIPI_SWAP
		mipitx_addr -= tx_ch * IRIS_MIPI_ADDR_OFFSET;
#else
		mipitx_addr += tx_ch * IRIS_MIPI_ADDR_OFFSET;
#endif

		if (pwork_mode->tx_mode)
			dsi_tx_ctrl = tx_setting->mipitx_dsi_tx_ctrl | (0x1 << 8);
		else
			dsi_tx_ctrl = tx_setting->mipitx_dsi_tx_ctrl & (~(0x1 << 8));
		iris_cmd_reg_add(pcmd, mipitx_addr + DSI_TX_CTRL, dsi_tx_ctrl & 0xfffffffe);

		iris_cmd_reg_add(pcmd, mipitx_addr + DPHY_TIMING_MARGIN, tx_setting->mipitx_dphy_timing_margin);
		iris_cmd_reg_add(pcmd, mipitx_addr + DPHY_LP_TIMING_PARA, tx_setting->mipitx_lp_timing_para);
		iris_cmd_reg_add(pcmd, mipitx_addr + DPHY_DATA_LANE_TIMING_PARA, tx_setting->mipitx_data_lane_timing_param);
		iris_cmd_reg_add(pcmd, mipitx_addr + DPHY_CLOCK_LANE_TIMING_PARA, tx_setting->mipitx_clk_lane_timing_param);
		iris_cmd_reg_add(pcmd, mipitx_addr + DPHY_TRIM_1, tx_setting->mipitx_dphy_trim_1);
		iris_cmd_reg_add(pcmd, mipitx_addr + DPHY_CTRL, 1 | (ddrclk_div << 26) | (ddrclk_div_en << 28));

		if (pwork_mode->tx_ch) {
			dual_ch_ctrl = pwork_mode->tx_ch + ((poutput_timing->hres * 2) << 16);
			if (0 == tx_ch)
				dual_ch_ctrl += 1 << 1;
			else
				dual_ch_ctrl += 2 << 1;
			iris_cmd_reg_add(pcmd, mipitx_addr + DUAL_CH_CTRL, dual_ch_ctrl);
		}
		iris_cmd_reg_add(pcmd, mipitx_addr + HS_TX_TIMER, tx_setting->mipitx_hs_tx_timer);
		iris_cmd_reg_add(pcmd, mipitx_addr + BTA_LP_TIMER, tx_setting->mipitx_bta_lp_timer);
		iris_cmd_reg_add(pcmd, mipitx_addr + INITIALIZATION_RESET_TIMER, tx_setting->mipitx_initialization_reset_timer);

		iris_cmd_reg_add(pcmd, mipitx_addr + TX_RESERVED_0, 4);

		iris_cmd_reg_add(pcmd, mipitx_addr + DSI_TX_CTRL, dsi_tx_ctrl);
	}

}

void iris_mipirx_reg_config(struct dsi_iris_info *iris_info, struct iris_grcp_cmd *pcmd)
{
	struct iris_work_mode *pwork_mode = &(iris_info->work_mode);
	struct iris_timing_info *pinput_timing = &(iris_info->input_timing);
	struct iris_mipirx_setting *rx_setting = &(iris_info->hw_setting.rx_setting);
	u32 dbi_handler_ctrl = 0, frame_col_addr = 0;
	u32 rx_ch, mipirx_addr = IRIS_MIPI_RX_ADDR;

	for (rx_ch = 0; rx_ch < (pwork_mode->rx_ch + 1); rx_ch++) {
#ifdef MIPI_SWAP
		mipirx_addr -= rx_ch * IRIS_MIPI_ADDR_OFFSET;
#else
		mipirx_addr += rx_ch * IRIS_MIPI_ADDR_OFFSET;
#endif
		iris_cmd_reg_add(pcmd, mipirx_addr + DEVICE_READY, 0x00000000);
		/*reset for DFE*/
		iris_cmd_reg_add(pcmd, mipirx_addr + RESET_ENABLE_DFE, 0x00000000);
		iris_cmd_reg_add(pcmd, mipirx_addr + RESET_ENABLE_DFE, 0x00000001);

		dbi_handler_ctrl = 0xf0000 + (pwork_mode->rx_ch << 23);
		/* left side enable */
		if (pwork_mode->rx_ch && (0 == rx_ch))
		dbi_handler_ctrl += (1 << 24);
		/*ext_mipi_rx_ctrl*/
		if (1 == rx_ch)
			dbi_handler_ctrl += (1 << 22);
		iris_cmd_reg_add(pcmd, mipirx_addr + DBI_HANDLER_CTRL, dbi_handler_ctrl);

		if (pwork_mode->rx_mode) {
			frame_col_addr = (pwork_mode->rx_ch) ? (pinput_timing->hres * 2 - 1) : (pinput_timing->hres - 1);
			iris_cmd_reg_add(pcmd, mipirx_addr + FRAME_COLUMN_ADDR, frame_col_addr << 16);
			iris_cmd_reg_add(pcmd, mipirx_addr + ABNORMAL_COUNT_THRES, 0xffffffff);
		}
		iris_cmd_reg_add(pcmd, mipirx_addr + INTEN, 0x3);
		iris_cmd_reg_add(pcmd, mipirx_addr + INTERRUPT_ENABLE, 0x0);
		iris_cmd_reg_add(pcmd, mipirx_addr + DSI_FUNCTIONAL_PROGRAMMING, rx_setting->mipirx_dsi_functional_program);
		iris_cmd_reg_add(pcmd, mipirx_addr + EOT_ECC_CRC_DISABLE, rx_setting->mipirx_eot_ecc_crc_disable);
		iris_cmd_reg_add(pcmd, mipirx_addr + DATA_LANE_TIMING_PARAMETER, rx_setting->mipirx_data_lane_timing_param);
		iris_cmd_reg_add(pcmd, mipirx_addr + DPI_SYNC_COUNT, pinput_timing->hsw + (pinput_timing->vsw << 16));
		iris_cmd_reg_add(pcmd, mipirx_addr + DEVICE_READY, 0x00000001);
	}
}

void iris_sys_reg_config(struct dsi_iris_info *iris_info, struct iris_grcp_cmd *pcmd)
{
	u32 clkmux_ctrl = 0x42180100, clkdiv_ctrl = 0x08;
	struct iris_pll_setting *pll = &iris_info->hw_setting.pll_setting;
	struct iris_clock_setting *clock = &iris_info->hw_setting.clock_setting;

	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + CLKMUX_CTRL, clkmux_ctrl | (clock->escclk.sel << 11) | pll->txpll_sel);
	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + CLKDIV_CTRL, clkdiv_ctrl | (clock->escclk.div << 3) | (clock->escclk.div_en << 7));

	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + DCLK_SRC_SEL, clock->dclk.sel | (clock->dclk.div << 8) | (clock->dclk.div_en << 10));
	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + INCLK_SRC_SEL, clock->inclk.sel | (clock->inclk.div << 8) | (clock->inclk.div_en << 10));
	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + MCUCLK_SRC_SEL, clock->mcuclk.sel | (clock->mcuclk.div << 8) | (clock->mcuclk.div_en << 12));
	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + PCLK_SRC_SEL, clock->pclk.sel | (clock->pclk.div << 8) | (clock->pclk.div_en << 10));
	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + MCLK_SRC_SEL, clock->mclk.sel | (clock->mclk.div << 8) | (clock->mclk.div_en << 10));

	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + PPLL_B_CTRL0, pll->ppll_ctrl0);
	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + PPLL_B_CTRL1, pll->ppll_ctrl1);
	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + PPLL_B_CTRL2, pll->ppll_ctrl2);

	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + DPLL_B_CTRL0, pll->dpll_ctrl0);
	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + DPLL_B_CTRL1, pll->dpll_ctrl1);
	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + DPLL_B_CTRL2, pll->dpll_ctrl2);

	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + MPLL_B_CTRL0, pll->mpll_ctrl0);
	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + MPLL_B_CTRL1, pll->mpll_ctrl1);
	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + MPLL_B_CTRL2, pll->mpll_ctrl2);

	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + PLL_CTRL, 0x1800e);
	iris_cmd_reg_add(pcmd, IRIS_SYS_ADDR + PLL_CTRL, 0x18000);
	if (DSI_OP_VIDEO_MODE == iris_info->work_mode.rx_mode) {
		iris_cmd_reg_add(pcmd, 0xf000002c, 2);
		iris_cmd_reg_add(pcmd, 0xf000002c, 0);
		iris_cmd_reg_add(pcmd, 0xf0010008, 0x80f5);
	}
}

void iris_init_cmd_build(struct dsi_iris_info *iris_info)
{
	u32 indx = 0, grcp_len = 0;

	memset(init_cmd, 0, sizeof(init_cmd));
	for (indx = 0; indx < INIT_CMD_NUM; indx++) {
		memcpy(init_cmd[indx].cmd, grcp_header, GRCP_HEADER);
		init_cmd[indx].cmd_len = GRCP_HEADER;
	}

	iris_mipitx_reg_config(iris_info, &init_cmd[0]);
	iris_mipirx_reg_config(iris_info, &init_cmd[1]);
	iris_sys_reg_config(iris_info, &init_cmd[2]);

	for (indx = 0; indx < INIT_CMD_NUM; indx++) {
		grcp_len = (init_cmd[indx].cmd_len - GRCP_HEADER) / 4;
		*(u32 *)(init_cmd[indx].cmd + 8) = cpu_to_le32(grcp_len + 1);
		*(u16 *)(init_cmd[indx].cmd + 14) = cpu_to_le16(grcp_len);
		pr_err("%s: iris init cmd #%d: [%d] length: %d\n", __func__, indx,
				(int)init_cmd[indx].cmd_len,
				grcp_len);
	}
}


u32 iris_get_vtotal(struct iris_timing_info *info)
{
	u32 vtotal;

	vtotal = info->vfp + info->vsw + info->vbp + info->vres;
	return vtotal;
}

u32 iris_get_htotal(struct iris_timing_info *info)
{
	u32 htotal;

	htotal = info->hfp + info->hsw + info->hbp + info->hres;
	return htotal;
}

void iris_dtg_setting_calc(struct dsi_iris_info *iris_info)
{
#define DELTA_PERIOD_MAX 95
	u32 vtotal = iris_get_vtotal(&iris_info->output_timing);
	u32 htotal = iris_get_htotal(&iris_info->output_timing);
	u32 vfp = iris_info->output_timing.vfp;
	u32 ovs_lock_te_en =0, psr_mask = 0, evs_sel = 0;
	u32 te_en = 0, te_interval = 0, te_sel = 0, sw_te_en = 0, sw_fix_te_en = 0, te_auto_adj_en = 0, te_ext_en = 0, te_ext_filter_en = 0, te_ext_filter_thr = 0;
	u32 cmd_mode_en = 0, cm_hw_rfb_mode = 0, cm_hw_frc_mode = 0;
	u32 dtg_en = 1, ivsa_sel = 1, vfp_adj_en = 1, dframe_ratio = 1, vframe_ratio = 1, lock_sel = 1;
	u32 sw_dvs_period = (vtotal << 8);
	u32 sw_te_scanline = 0, sw_te_scanline_frc = 0, te_ext_dly = 0, te_out_sel = 0, te_out_filter_thr = 0, te_out_filter_en = 0, te_ext_dly_frc = 0;
	u32 te2ovs_dly = 0, te2ovs_dly_frc = 0;
	u32 evs_dly = 6, evs_new_dly = 1;
	u32 vfp_max = 0;
	u32 vfp_extra = DELTA_PERIOD_MAX;
	u32 lock_mode = 0;
	u32 vres_mem = 0, psr_rd = 2, frc_rd = 4, scale_down = 2, dsc = 2, margin = 2, scale_up = 2;
	u32 peaking = 2;
	u32 i2o_dly = 0;
	struct iris_dtg_setting *dtg_setting = &iris_info->dtg_setting;
	struct iris_work_mode *work_mode = &iris_info->work_mode;

	//dtg 1.1 mode, video in
	if (DSI_OP_VIDEO_MODE == work_mode->rx_mode)
	{
		evs_sel = 1;
		psr_mask = 1;
		dtg_setting->dtg_delay = 3;
		vfp_max = vfp + vfp_extra;
	}
	else if (DSI_OP_VIDEO_MODE == work_mode->tx_mode)
	{
		if (!iris_debug_dtg_v12) {
			//dtg 1.3 mode, command in and video out
			ovs_lock_te_en = 1;
			cmd_mode_en = 1;
			te_en = 1;
			sw_fix_te_en = 1;
			te_auto_adj_en = 1;
			te2ovs_dly = vfp - 1;
			te2ovs_dly_frc = (vtotal*3)/4;
			dtg_setting->dtg_delay = 2;
			vfp_max = (te2ovs_dly_frc > vfp) ? te2ovs_dly_frc : vfp;
		} else {
			//dtg 1.2 mode, command in and video out
			evs_sel = 1;
			evs_dly = 2;
			evs_new_dly = 1;
			cmd_mode_en = 1;
			cm_hw_frc_mode = 3;
			cm_hw_rfb_mode = 3;
			te_en = 1;
			te_sel = 1;
			sw_te_en = 1;
			te_auto_adj_en = 1;
			vres_mem = iris_lp_memc_calc(LEVEL_MAX - 1) >> 16;
			i2o_dly = ((psr_rd > frc_rd? psr_rd: frc_rd) + scale_down + dsc + margin) * iris_info->input_timing.vres +
					scale_up * iris_info->output_timing.vres +
					peaking * vres_mem;
			sw_te_scanline = vtotal * vres_mem - (i2o_dly - iris_info->output_timing.vbp * vres_mem - iris_info->output_timing.vsw * vres_mem);
			sw_te_scanline /= vres_mem;
			sw_te_scanline_frc = (vtotal)/2;
			te_out_filter_thr = (vtotal)/2;
			te_out_filter_en = 1;
			dtg_setting->dtg_delay = 2;
			vfp_max = vfp + vfp_extra;
		}
	}
	//dtg 1.4 mode, command in and command out
	else if (DSI_OP_CMD_MODE == work_mode->tx_mode)
	{
		vfp_max = vfp + vfp_extra;
		evs_dly = 2;
		evs_sel = 1;
		ovs_lock_te_en = 1;
		cmd_mode_en = 1;
		te_en = 1;
		te_auto_adj_en = 1;
		te_ext_en = 1;

		te_ext_filter_thr = (((u32)iris_info->output_timing.hres * (u32)iris_info->output_timing.vres * 100)/vtotal/htotal)*vtotal/100;
		te2ovs_dly = 2;
		te2ovs_dly_frc = 2;
		te_ext_dly = 1;
		te_out_sel = 1;
		te_out_filter_thr = (vtotal)/2;
		te_out_filter_en = 1;
		te_ext_dly_frc = (vtotal)/4;
		dtg_setting->dtg_delay = 1;
		te_ext_filter_en = 1;
		lock_mode = 2;

		te_sel = 1;
		sw_te_en = 1;
		vres_mem = iris_lp_memc_calc(0) >> 16;
		i2o_dly = ((psr_rd > frc_rd? psr_rd: frc_rd) + scale_down + dsc + margin) * iris_info->input_timing.vres +
				scale_up * iris_info->output_timing.vres +
				peaking * vres_mem;
		sw_te_scanline = vtotal * vres_mem - (i2o_dly - iris_info->output_timing.vbp * vres_mem - iris_info->output_timing.vsw * vres_mem);
		sw_te_scanline /= vres_mem;
		sw_te_scanline_frc = (vtotal)/4;
		evs_new_dly = (scale_down + dsc + margin) * iris_info->input_timing.vres / vres_mem - te2ovs_dly;
	}

	dtg_setting->dtg_ctrl = dtg_en + (ivsa_sel << 3) + (dframe_ratio << 4) + (vframe_ratio << 9) + (vfp_adj_en << 17) +
								(ovs_lock_te_en << 18) + (lock_sel << 26) + (evs_sel << 28) + (psr_mask << 30);
	dtg_setting->dtg_ctrl_1 = (cmd_mode_en) + (lock_mode << 5) + (cm_hw_rfb_mode << 10) + (cm_hw_frc_mode << 12);
	dtg_setting->evs_dly = evs_dly;
	dtg_setting->evs_new_dly = evs_new_dly;
	dtg_setting->te_ctrl = (te_en) + (te_interval << 1) + (te_sel << 2) + (sw_te_en << 3) + (sw_fix_te_en << 5) +
						(te_auto_adj_en << 6) + (te_ext_en << 7) + (te_ext_filter_en << 8) + (te_ext_filter_thr << 9);
	dtg_setting->dvs_ctrl = (DSI_OP_CMD_MODE == work_mode->tx_mode) ? sw_dvs_period : (sw_dvs_period + (2 << 30));/* for cin-vout, should + (2 << 30);*/
	dtg_setting->te_ctrl_1 = sw_te_scanline;
	dtg_setting->te_ctrl_2 = sw_te_scanline_frc;
	dtg_setting->te_ctrl_3 = te_ext_dly + (te_out_sel << 24);
	dtg_setting->te_ctrl_4 = te_out_filter_thr + (te_out_filter_en << 24);
	dtg_setting->te_ctrl_5 = te_ext_dly_frc;
	dtg_setting->te_dly = te2ovs_dly;
	dtg_setting->te_dly_1 = te2ovs_dly_frc;
	dtg_setting->vfp_ctrl_0 = vfp + (1<<24);
	dtg_setting->vfp_ctrl_1 = vfp_max;

}

void iris_buffer_alloc(void)
{
	if(!app_fw_buf) {
		app_fw_buf = kzalloc(DSI_DMA_TX_BUF_SIZE, GFP_KERNEL);
		if (!app_fw_buf) {
			pr_err("%s: failed to alloc app fw mem, size = %d\n", __func__, DSI_DMA_TX_BUF_SIZE);
		}
	}

	if (!lut_fw_buf) {
		lut_fw_buf = kzalloc(IRIS_CM_LUT_LENGTH * 4 * CMI_TOTAL, GFP_KERNEL);
		if (!lut_fw_buf) {
			pr_err("%s: failed to alloc lut fw mem, size = %d\n", __func__, IRIS_CM_LUT_LENGTH * 4 * CMI_TOTAL);
		}
	}

	if (!gamma_fw_buf) {
		gamma_fw_buf = kzalloc(8 * 1024, GFP_KERNEL);
		if (!gamma_fw_buf) {
			pr_err("%s: failed to alloc gamma fw mem, size = %d\n", __func__, 8 * 1024);
		}
	}

	if (!iris_cmlut_buf) {
		iris_cmlut_buf = kzalloc(32 * 1024, GFP_KERNEL);
		if (!iris_cmlut_buf) {
			pr_err("%s: failed to alloc cm lut mem, size = %d\n", __func__, 32 * 1024);
		}
	}
}

void iris_analog_bypass_info_init(struct dsi_iris_info *iris_info)
{
	struct iris_abypass_ctrl *pabypass_ctrl = &(iris_info->abypass_ctrl);

	pabypass_ctrl->analog_bypass_enable = true;
	pabypass_ctrl->abypass_status = PASS_THROUGH_MODE;
	pabypass_ctrl->abypass_debug = false;
	pabypass_ctrl->abypass_to_pt_enable = 0;
	pabypass_ctrl->pt_to_abypass_enable = 0;
	pabypass_ctrl->abypass_switch_state = MCU_STOP_ENTER_STATE;
	pabypass_ctrl->base_time = 0;
	pabypass_ctrl->frame_delay = 0;
}

int iris_abyp_gpio_init(struct dsi_panel *panel);

void iris_info_init(struct dsi_panel *panel)
{
	iris_panel = panel;
	iris_full_functioning = 0;
	iris_in_cont_splash_displaying = 0;
	iris_hw_status_check_enabled = false;
	/*added multiple panel support to distinguish lcd modules*/
	if (!strcmp(iris_panel->name, "KINGDISPLAY-RM69299-1080-2248-6P2Inch")) {
		connected_lcd_module = LCM_KINGDISPLAY;
	} else {
		connected_lcd_module = LCM_SAMSUNG;
	}

	memset(&iris_info, 0, sizeof(iris_info));

	iris_workmode_init(&iris_info, panel);
	iris_timing_init(&iris_info);
	iris_clock_tree_init(&iris_info);
	iris_mipi_info_init(&iris_info);
	iris_analog_bypass_info_init(&iris_info);
	iris_feature_setting_init(&iris_info.user_setting);
	iris_feature_setting_init(&iris_info.chip_setting);

	iris_init_cmd_build(&iris_info);
	iris_dtg_setting_calc(&iris_info);
	iris_buffer_alloc();
	iris_abyp_gpio_init(panel);
	mutex_init(&iris_info.config_mutex);
	spin_lock_init(&iris_info.iris_lock);
	mutex_init(&iris_info.dsi_send_mutex);
	pr_err("%s: for %s completed!\n", __func__, panel->name);
}

int iris_dsi_cmds_send(struct dsi_panel *panel,
				struct dsi_cmd_desc *cmds,
				u32 count,
				enum dsi_cmd_set_state state)
{
	int rc = 0, i = 0;
	ssize_t len;
	const struct mipi_dsi_host_ops *ops = panel->host->ops;

	if (!panel || !panel->cur_mode || cmds == NULL)
		return -EINVAL;

	if (count == 0) {
		pr_debug("[%s] No commands to be sent for state\n",
			 panel->name);
		goto error;
	}

	for (i = 0; i < count; i++) {
		/* TODO:  handle last command */
		if (state == DSI_CMD_SET_STATE_LP)
			cmds->msg.flags |= MIPI_DSI_MSG_USE_LPM;
		if (cmds->last_command)
			cmds->msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;

		if (panel->host == NULL || cmds == NULL) {
			pr_err("iris_dsi_cmds_send: invalid panel host or cmds");
			break;
		}

		len = ops->transfer(panel->host, &cmds->msg);
		if (len < 0) {
			rc = len;
			pr_err("iris_dsi_cmds_send failed to set cmds(%d), rc=%d\n", cmds->msg.type, rc);
			goto error;
		}
		if (cmds->post_wait_ms)
			usleep_range(cmds->post_wait_ms*1000,
					((cmds->post_wait_ms*1000)+10));

		cmds++;
	}
error:
	return rc;
}

u8 iris_mipi_power_mode_read(struct dsi_panel *panel, enum dsi_cmd_set_state state)
{
	const char get_power_mode[1] = {0x0a};
	const struct mipi_dsi_host_ops *ops = panel->host->ops;
	struct dsi_cmd_desc cmds = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_DCS_READ, 0, 0, 0, sizeof(get_power_mode), get_power_mode, 1, iris_read_cmd_buf}, 1, 0};
#else
		{0, MIPI_DSI_DCS_READ, 0, 0, sizeof(get_power_mode), get_power_mode, 1, iris_read_cmd_buf}, 1, 0};
#endif
	int rc = 0;
	u8 data = 0xff;

	memset(iris_read_cmd_buf, 0, sizeof(iris_read_cmd_buf));
	if (state == DSI_CMD_SET_STATE_LP)
		cmds.msg.flags |= MIPI_DSI_MSG_USE_LPM;
	if (cmds.last_command)
		cmds.msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;
	cmds.msg.flags |= BIT(6);

	rc = ops->transfer(panel->host, &cmds.msg);
	if (rc < 0) {
		pr_err("iris_mipi_power_mode_read failed to set cmds(%d), rc=%d, read=%d\n",
			cmds.msg.type,
			rc,
			iris_read_cmd_buf[0]);
		return data;
	}
	data = iris_read_cmd_buf[0];
	if (isAllowedLightupTrace())
		pr_err("power_mode: %d\n", data);

	return data;
}

int iris_mipi_ddb_para_read(struct dsi_panel *panel,
enum dsi_cmd_set_state state, char* buf, int size)
{
	char *readbuf = buf;
	char get_ddb_para[1] = {0xa1};
	const struct mipi_dsi_host_ops *ops = panel->host->ops;
	struct dsi_cmd_desc cmds = {
		{0, MIPI_DSI_DCS_READ, 0, 0, 0, sizeof(get_ddb_para), get_ddb_para, size, readbuf}, 1, 0};
	int rc = 0;

	if (state == DSI_CMD_SET_STATE_LP)
		cmds.msg.flags |= MIPI_DSI_MSG_USE_LPM;
	if (cmds.last_command)
		cmds.msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;
	cmds.msg.flags |= BIT(6);

	rc = ops->transfer(panel->host, &cmds.msg);
	if (rc < 0) {
		pr_err("failed to set cmds(%d), rc=%d, read=%d\n", cmds.msg.type, rc, readbuf[0]);
		return rc;
	}

	return rc;
}

int iris_i2c_ddb_para_read(char *buf, int size)
{
	u32 read_cmd_done = 0, read_data;
	u8 cnt = 0, indx = 1;
	u8 data[20];

	mutex_lock(&iris_info.dsi_send_mutex);
	iris_i2c_reg_write(IRIS_MIPI_TX_ADDR + TX_INTCLR, 0x80000000);
	iris_i2c_reg_write(IRIS_MIPI_TX_ADDR + WR_PACKET_HEADER_OFFS, 0x03001237);
	iris_i2c_reg_write(IRIS_MIPI_TX_ADDR + WR_PACKET_HEADER_OFFS, 0x0200a106);

	do {
		usleep_range(1000 * indx, 1010 * indx);
		iris_i2c_reg_read(IRIS_MIPI_TX_ADDR + TX_INTSTAT_RAW, &read_cmd_done);
		read_cmd_done &= (1 << 31);
		if (read_cmd_done) {
			iris_i2c_reg_read(IRIS_MIPI_TX_ADDR + RD_PACKET_DATA_OFFS, &read_data);
			for (cnt = 0; cnt < 20; cnt += 4) {
				iris_i2c_reg_read(IRIS_MIPI_TX_ADDR + RD_PACKET_DATA_OFFS, &read_data);
				memcpy(data + cnt, &read_data, 4);
			}
			memcpy(buf, data, size);
			mutex_unlock(&iris_info.dsi_send_mutex);
			return 0;
		}

		indx++;
	} while (indx < 8);
	pr_err("read ddb para failed: %d\n", indx);

	mutex_unlock(&iris_info.dsi_send_mutex);
	return -1;
}

int iris_mipitx_read_fifo_empty_check(void)
{
	u8 indx = 0;
	u32 tx_status = 0, read_data = 0;

	do {
		iris_i2c_reg_read(IRIS_MIPI_TX_ADDR + DSI_TX_STATUS, &tx_status);
		if (tx_status & (1 << 15)) {
			/* read fifo is empty */
			return 0;
		} else {
			iris_i2c_reg_read(IRIS_MIPI_TX_ADDR + RD_PACKET_DATA_OFFS, &read_data);
		}
		indx++;
	} while (indx < 10);

	pr_err("iris check mipitx read fifo empty failed: %d\n", indx);

	return -1;
}

int iris_i2c_panel_state_read(int reg, u8 *buf)
{
	u32 read_cmd_done = 0, read_data = 0;
	u8 indx = 1;

	mutex_lock(&iris_info.dsi_send_mutex);
	iris_mipitx_read_fifo_empty_check();

	iris_i2c_reg_write(IRIS_MIPI_TX_ADDR + TX_INTCLR, 0x80000000);
	iris_i2c_reg_write(IRIS_MIPI_TX_ADDR + WR_PACKET_HEADER_OFFS, 0x01000137);
	iris_i2c_reg_write(IRIS_MIPI_TX_ADDR + WR_PACKET_HEADER_OFFS, reg);

	do {
		usleep_range(1000 * indx, 1010 * indx);
		iris_i2c_reg_read(IRIS_MIPI_TX_ADDR + TX_INTSTAT_RAW, &read_cmd_done);
		read_cmd_done &= (1 << 31);
		if (read_cmd_done) {
			iris_i2c_reg_read(IRIS_MIPI_TX_ADDR + RD_PACKET_DATA_OFFS, &read_data);
			*buf = (read_data >> 8) & 0xfc;
			mutex_unlock(&iris_info.dsi_send_mutex);
			return 0;
		}

		indx++;
	} while (indx < 8);
	pr_err("iris read panel state failed: %x\n", read_data);
	mutex_unlock(&iris_info.dsi_send_mutex);

	return -1;
}

u8 iris_i2c_power_mode_read(void)
{
	u32 reg_value = 0;

	iris_i2c_reg_read(IRIS_MIPI_RX_ADDR + DCS_CMD_PARA_1, &reg_value);

	return (u8)(reg_value & 0xff);
}

int iris_Romcode_state_check(struct dsi_panel *panel, u8 data, u8 retry_cnt, bool viaIIC)
{
#ifdef READ_CMD_ENABLE
	u8 cnt = 0, powermode = 0;

	do {
		#ifdef ENABLE_IRIS_I2C_READ
		if (viaIIC)
			powermode = iris_i2c_power_mode_read();
		else
			powermode = iris_mipi_power_mode_read(panel, DSI_CMD_SET_STATE_LP);
		#else
			powermode = iris_mipi_power_mode_read(panel, DSI_CMD_SET_STATE_LP);
		#endif
		if (isAllowedLightupTrace())
			pr_err("read back iris powermode: %d(0x%x), cnt: %d\n", powermode, powermode, cnt);

		if (powermode == 0xff)
			powermode = 0;

		if ((powermode&data) == data)
			break;
		usleep_range(500, 510);
		cnt++;
	} while (((powermode&data) != data) && cnt < retry_cnt);

	/* read failed */
	if (cnt == retry_cnt) {
#ifdef ENABLE_IRIS_I2C_READ
		pr_err("iris_Romcode_state_check: viaIIC %d, powermode %x failed in 0x%x\n", viaIIC, data, powermode);
#else
		pr_err("iris_Romcode_state_check: powermode %x failed in 0x%x\n", data, powermode);
#endif
		return -EINVAL;
	}
#endif
	return 0;
}

int iris_check_powermode(struct dsi_panel *panel, u8 data, u8 retry_cnt)
{
	return iris_Romcode_state_check(panel, data, retry_cnt, false);
}

void iris_mipirx_mode_set(struct dsi_panel *panel, int mode, enum dsi_cmd_set_state state)
{
	char mipirx_mode[1] = {0x3f};
	struct dsi_cmd_desc iris_mipirx_mode_cmds = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM, 0, 0, 0, sizeof(mipirx_mode), mipirx_mode, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM, 0, 0, sizeof(mipirx_mode), mipirx_mode, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif
	switch (mode) {
	case MCU_VIDEO:
		mipirx_mode[0] = 0x3f;
		break;
	case MCU_CMD:
		mipirx_mode[0] = 0x1f;
		break;
	case PWIL_VIDEO:
		mipirx_mode[0] = 0xbf;
		break;
	case PWIL_CMD:
		mipirx_mode[0] = 0x7f;
		break;
	case BYPASS_VIDEO:
		mipirx_mode[0] = 0xff;
		break;
	case BYPASS_CMD:
		mipirx_mode[0] = 0xdf;
		break;
	default:
		break;
	}

	iris_dsi_cmds_send(panel, &iris_mipirx_mode_cmds, 1, state);
}

void iris_init_cmd_send(struct dsi_panel *panel, enum dsi_cmd_set_state state)
{
	struct dsi_cmd_desc iris_init_info_cmds[] = {
#ifdef NEW_MIPI_DSI_MSG
		{{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PROC,     CMD_PKT_SIZE, init_cmd[0].cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC},
		{{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, MCU_PROC * 2, CMD_PKT_SIZE, init_cmd[1].cmd, 1, iris_read_cmd_buf}, 1, MCU_PROC * 2},
		{{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, MCU_PROC * 2, CMD_PKT_SIZE, init_cmd[2].cmd, 1, iris_read_cmd_buf}, 1, MCU_PROC * 2},
#else
		{{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PKT_SIZE, init_cmd[0].cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC},
		{{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PKT_SIZE, init_cmd[1].cmd, 1, iris_read_cmd_buf}, 1, MCU_PROC * 2},
		{{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PKT_SIZE, init_cmd[2].cmd, 1, iris_read_cmd_buf}, 1, MCU_PROC * 2},
#endif
	};

	iris_init_info_cmds[0].msg.tx_len = init_cmd[0].cmd_len;
	iris_init_info_cmds[1].msg.tx_len = init_cmd[1].cmd_len;
	iris_init_info_cmds[2].msg.tx_len = init_cmd[2].cmd_len;

	if (isAllowedLightupTrace())
		pr_err("iris: send init cmd: %d %d %d\n",
				(int)iris_init_info_cmds[0].msg.tx_len,
				(int)iris_init_info_cmds[1].msg.tx_len,
				(int)iris_init_info_cmds[2].msg.tx_len);

	iris_dsi_cmds_send(panel, iris_init_info_cmds, ARRAY_SIZE(iris_init_info_cmds), state);

	if (isAllowedLightupTrace())
		pr_err("iris: send init cmd done.\n");

	DUMP_PACKET(init_cmd[0].cmd, init_cmd[0].cmd_len);
	DUMP_PACKET(init_cmd[1].cmd, init_cmd[1].cmd_len);
	DUMP_PACKET(init_cmd[2].cmd, init_cmd[2].cmd_len);
}

void iris_timing_info_send(struct dsi_panel *panel, enum dsi_cmd_set_state state)
{
	char iris_workmode[] = {
		0x80, 0x87, 0x0, 0x3,
		PWIL_U32(0x0),
	};
	char iris_timing[] = {
		0x80, 0x87, 0x0, 0x0,
		PWIL_U32(0x01e00010),
		PWIL_U32(0x00160010),
		PWIL_U32(0x0320000a),
		PWIL_U32(0x00080008),
		PWIL_U32(0x3c1f),
		PWIL_U32(0x01e00014),
		PWIL_U32(0x00160010),
		PWIL_U32(0x0320000a),
		PWIL_U32(0x00080008),
		PWIL_U32(0x3c1f),
		PWIL_U32(0x00100008),
		PWIL_U32(0x80),
		PWIL_U32(0x00100008),
		PWIL_U32(0x80)
	};
	struct dsi_cmd_desc iris_timing_info_cmd[] = {
#ifdef NEW_MIPI_DSI_MSG
		{{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, MCU_PROC, sizeof(iris_workmode), iris_workmode, 1, iris_read_cmd_buf}, 1, MCU_PROC},
		{{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, MCU_PROC, sizeof(iris_timing), iris_timing, 1, iris_read_cmd_buf}, 1, MCU_PROC},
#else
		{{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(iris_workmode), iris_workmode, 1, iris_read_cmd_buf}, 1, MCU_PROC},
		{{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(iris_timing), iris_timing, 1, iris_read_cmd_buf}, 1, MCU_PROC},
#endif
	};
	struct iris_timing_info *pinput_timing = &(iris_info.input_timing);
	struct iris_timing_info *poutput_timing = &(iris_info.output_timing);

	memcpy(iris_workmode + 4, &(iris_info.work_mode), 4);

	*(u32 *)(iris_timing + 4) = cpu_to_le32((pinput_timing->hres << 16) + pinput_timing->hfp);
	*(u32 *)(iris_timing + 8) = cpu_to_le32((pinput_timing->hsw << 16) + pinput_timing->hbp);
	*(u32 *)(iris_timing + 12) = cpu_to_le32((pinput_timing->vres << 16) + pinput_timing->vfp);
	*(u32 *)(iris_timing + 16) = cpu_to_le32((pinput_timing->vsw << 16) + pinput_timing->vbp);
	*(u32 *)(iris_timing + 20) = cpu_to_le32((pinput_timing->fps << 8) + 0x1f);

	*(u32 *)(iris_timing + 24) = cpu_to_le32((poutput_timing->hres << 16) + poutput_timing->hfp);
	*(u32 *)(iris_timing + 28) = cpu_to_le32((poutput_timing->hsw << 16) + poutput_timing->hbp);
	*(u32 *)(iris_timing + 32) = cpu_to_le32((poutput_timing->vres << 16) + poutput_timing->vfp);
	*(u32 *)(iris_timing + 36) = cpu_to_le32((poutput_timing->vsw << 16) + poutput_timing->vbp);
	*(u32 *)(iris_timing + 40) = cpu_to_le32((poutput_timing->fps << 8) + 0x1f);

	if (isAllowedLightupTrace())
		pr_info("iris: send timing info\n");

	iris_dsi_cmds_send(panel, iris_timing_info_cmd, ARRAY_SIZE(iris_timing_info_cmd), state);

	DUMP_PACKET(iris_workmode, sizeof(iris_workmode));
	DUMP_PACKET(iris_timing, sizeof(iris_timing));
}

void iris_ctrl_cmd_send(struct dsi_panel *panel, u8 cmd, enum dsi_cmd_set_state state)
{
	char romcode_ctrl[] = {
		PWIL_TAG('P', 'W', 'I', 'L'),
		PWIL_TAG('G', 'R', 'C', 'P'),
		PWIL_U32(0x00000005),
		0x00,
		0x00,
		PWIL_U16(0x04),
		PWIL_U32(IRIS_MODE_ADDR),  /*proxy_MB1*/
		PWIL_U32(0x00000000),
		PWIL_U32(IRIS_PROXY_MB7_ADDR),  /*proxy_MB7*/
		PWIL_U32(0x00040000),
	};
	struct dsi_cmd_desc iris_romcode_ctrl_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PROC, sizeof(romcode_ctrl), romcode_ctrl, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(romcode_ctrl), romcode_ctrl, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif

	if ((cmd | CONFIG_DATAPATH) || (cmd | ENABLE_DPORT) || (cmd | REMAP))
		iris_romcode_ctrl_cmd.post_wait_ms = INIT_WAIT;

	romcode_ctrl[20] = cmd;
	*(u32 *)(romcode_ctrl + 28) = cpu_to_le32(iris_info.firmware_info.app_fw_size);

	iris_dsi_cmds_send(panel, &iris_romcode_ctrl_cmd, 1, state);
	DUMP_PACKET(romcode_ctrl, sizeof(romcode_ctrl));
}

void iris_grcp_buffer_init(struct dsi_panel *panel, enum dsi_cmd_set_state state)
{
	static char grcp_buf_init[] = {
		PWIL_TAG('P', 'W', 'I', 'L'),
		PWIL_TAG('G', 'R', 'C', 'P'),
		PWIL_U32(0x0000000b),
		0x03,
		0x00,
		PWIL_U16(0x09),
		PWIL_U32(IRIS_GRCP_CTRL_ADDR + 0X12fdc),
		PWIL_U32(0x0),
		PWIL_U32(0x0),
		PWIL_U32(0x0),
		PWIL_U32(0x0),
		PWIL_U32(0x0),
		PWIL_U32(0x0),
		PWIL_U32(0x0),
		PWIL_U32(0x0),
		PWIL_U32(0x0),
	};

	struct dsi_cmd_desc iris_grcp_buf_init_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, 0, sizeof(grcp_buf_init), grcp_buf_init, 1, iris_read_cmd_buf}, 1, 0};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(grcp_buf_init), grcp_buf_init, 1, iris_read_cmd_buf}, 1, 0};
#endif

	if (isAllowedLightupTrace())
		pr_info("iris: init GRCP buffer\n");
	iris_dsi_cmds_send(panel, &iris_grcp_buf_init_cmd, 1, state);
	DUMP_PACKET(grcp_buf_init, sizeof(grcp_buf_init));
}

void iris_bp_cmd_send(struct dsi_panel *panel, enum dsi_cmd_set_state state)
{
	u32 grcp_len = 0;
	struct dsi_cmd_desc iris_bp_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, 0};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, 0};
#endif
	mutex_lock(&iris_info.dsi_send_mutex);
	memset(&grcp_cmd, 0, sizeof(grcp_cmd));
	memcpy(grcp_cmd.cmd, grcp_header, GRCP_HEADER);
	grcp_cmd.cmd_len = GRCP_HEADER;

	iris_cmd_reg_add(&grcp_cmd, IRIS_SYS_ADDR + 0x2c, 0x2);
	iris_cmd_reg_add(&grcp_cmd, IRIS_SYS_ADDR + 0x2c, 0x3);
	grcp_len = (grcp_cmd.cmd_len - GRCP_HEADER)/4;
	*(u32*)(grcp_cmd.cmd + 8) = cpu_to_le32(grcp_len + 1);
	*(u16*)(grcp_cmd.cmd + 14) = cpu_to_le16(grcp_len);
	iris_bp_cmd.msg.tx_len = grcp_cmd.cmd_len;
	iris_dsi_cmds_send(panel, &iris_bp_cmd, 1, state);
	mutex_unlock(&iris_info.dsi_send_mutex);
	DUMP_PACKET(grcp_cmd.cmd, grcp_cmd.cmd_len);
	pr_info("iris: bp cmd sent\n");
}

static int iris_handover_panel_dcs_cmd(struct dsi_panel *panel,
					struct dsi_cmd_desc *panel_dcs_cmd,
					enum dsi_cmd_set_state state)
{
	int ret = 0;
	u32 grcp_len = 0;
	u32 cnt = 0;
	u8 *databuf = 0;
	union iris_mipi_tx_cmd_header header;
	union iris_mipi_tx_cmd_payload payload;
	struct dsi_cmd_desc iris_handover_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif

	if (!panel->panel_initialized || panel_dcs_cmd == NULL) {
		ret = -1;
		return ret;
	}
	mutex_lock(&iris_info.dsi_send_mutex);
	memset(&grcp_cmd, 0, sizeof(grcp_cmd));
	memcpy(grcp_cmd.cmd, grcp_header, GRCP_HEADER);
	grcp_cmd.cmd_len = GRCP_HEADER;

	memset(&header, 0, sizeof(header));
	memset(&payload, 0, sizeof(payload));
	header.stHdr.dtype = panel_dcs_cmd->msg.type;
	databuf = (u8*)(panel_dcs_cmd->msg.tx_buf);

	switch (panel_dcs_cmd->msg.type) {
	case MIPI_DSI_DCS_SHORT_WRITE://0x5
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM://0x15
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM://0x13
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM://0x23
	case MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE://0x37
		//short write
		header.stHdr.ecc = 0x1;
		header.stHdr.len[0] = databuf[0];
		if (panel_dcs_cmd->msg.tx_len == 2) {
			header.stHdr.len[1] = databuf[1];
		}
		if (irisIsAllowedDsiTrace())
			pr_err("iris: dcs handover header(shortwrite): 0x%x\n", header.hdr32);
		iris_cmd_reg_add(&grcp_cmd, IRIS_MIPI_TX_ADDR + WR_PACKET_HEADER_OFFS, header.hdr32);
		break;
	case MIPI_DSI_DCS_READ://0x06
		//short read
		header.stHdr.ecc = 0x0;
		header.stHdr.len[0] = databuf[0];
		if (panel_dcs_cmd->msg.tx_len == 2) {
			header.stHdr.len[1] = databuf[1];
		}
		if (irisIsAllowedDsiTrace())
			pr_err("iris: dcs handover header(shortread): 0x%x\n", header.hdr32);
		iris_cmd_reg_add(&grcp_cmd, IRIS_MIPI_TX_ADDR + WR_PACKET_HEADER_OFFS, header.hdr32);
		break;
	case MIPI_DSI_DCS_LONG_WRITE://0x39
	case MIPI_DSI_GENERIC_LONG_WRITE://0x29
		//long write
		header.stHdr.ecc = 0x5;
		header.stHdr.len[0] = panel_dcs_cmd->msg.tx_len&0xff;
		header.stHdr.len[1] = (panel_dcs_cmd->msg.tx_len>>8)&0xff;

		if (irisIsAllowedDsiTrace())
			pr_err("iris: dcs handover header(longwrite): 0x%x\n", header.hdr32);
		iris_cmd_reg_add(&grcp_cmd, IRIS_MIPI_TX_ADDR + WR_PACKET_HEADER_OFFS, header.hdr32);
		for (cnt = 0; cnt < panel_dcs_cmd->msg.tx_len; cnt = cnt + 4) {
			if ((panel_dcs_cmd->msg.tx_len - cnt) >= 4) {
				memcpy(payload.p, ((u8 *)panel_dcs_cmd->msg.tx_buf) + cnt, 4);
			} else {
				memcpy(payload.p, ((u8 *)panel_dcs_cmd->msg.tx_buf) + cnt, panel_dcs_cmd->msg.tx_len - cnt);
			}
			if (irisIsAllowedDsiTrace())
				pr_err("iris: dcs handover header(longwrite): 0x%x\n", payload.pld32);
			iris_cmd_reg_add(&grcp_cmd, IRIS_MIPI_TX_ADDR + WR_PACKET_PAYLOAD_OFFS, payload.pld32);
		}
		break;
	default:
		ret = -1;
		break;
	}

	grcp_len = (grcp_cmd.cmd_len - GRCP_HEADER)/4;
	*(u32*)(grcp_cmd.cmd + 8) = cpu_to_le32(grcp_len + 1);
	*(u16*)(grcp_cmd.cmd + 14) = cpu_to_le16(grcp_len);
	iris_handover_cmd.msg.tx_len = grcp_cmd.cmd_len;
	iris_dsi_cmds_send(panel, &iris_handover_cmd, 1, state);
	mutex_unlock(&iris_info.dsi_send_mutex);
	DUMP_PACKET(grcp_cmd.cmd, grcp_cmd.cmd_len);
	if (irisIsAllowedDsiTrace())
		pr_info("iris: handover panel dcs cmd sent\n");
	return ret;
}

static u16 lastBrightness = 0x0;
static u16 savedBrightness = 0x0;
static bool panel_hbm_on = false;
static bool need_resume_backlight = false;

#ifdef CONFIG_LCD_DCSBL_CABC_GRADIENT
int iris_handover_dcs_panel_switch_dimming(struct dsi_panel *panel, u16 brightness, enum dsi_cmd_set_state state)
{
	int ret = -1;
	static u8 displayControlPayload[2] = {MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x20};
	struct dsi_cmd_desc panel_dcs_control_display_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, 0, sizeof(displayControlPayload), displayControlPayload, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(displayControlPayload), displayControlPayload, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif

	if (displayControlPayload[1] == 0x28) {
		if (irisIsInAbypassMode() || irisIsInLowPowerAbypassMode()) {
			ret = iris_dsi_cmds_send(panel, &panel_dcs_control_display_cmd, 1, state);
		} else {
			ret = iris_handover_panel_dcs_cmd(panel, &panel_dcs_control_display_cmd, 1, state);
		}
		if (isAllowedLightupTrace())
			pr_info("iris: setting the 53-register for display dimming returned %d", ret);
		displayControlPayload[1] = 0x20;
	}

	if (lastBrightness == 0 && brightness != 0) {
		displayControlPayload[1] = 0x28;
	}

	return ret;
}
#endif

void iris_resume_panel_brightness(struct dsi_panel *panel)
{
	if (savedBrightness != lastBrightness
		&& savedBrightness > 0
		&& need_resume_backlight) {
		pr_info("iris: resuming backlight brightness set: %d->%d", lastBrightness, savedBrightness);

		iris_handover_dcs_set_display_brightness(panel, savedBrightness, DSI_CMD_SET_STATE_LP);
	}
}

bool irisIsAbypassSwitchOngoing(void)
{
	struct iris_mode_state *mode_state = &iris_info.mode_state;

	return (mode_state->sf_notify_mode == IRIS_MODE_PT2BYPASS
			|| mode_state->sf_notify_mode == IRIS_MODE_BYPASS2PT);
}

int iris_handover_dcs_enable_panel_hbm(struct dsi_panel *panel, bool setHbm, enum dsi_cmd_set_state state);

#define BACKLIGHT_ON_AFTER_XMS_FIRST_KICKOFF  (20)
int iris_handover_dcs_set_display_brightness(struct dsi_panel *panel, u16 brightness, enum dsi_cmd_set_state state)
{
	int ret = -1;
	u8 brightnessPayload[3] = {MIPI_DCS_SET_DISPLAY_BRIGHTNESS, brightness>>8, brightness&0xff};
	struct dsi_cmd_desc panel_dcs_brightness_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(brightnessPayload), brightnessPayload, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, sizeof(brightnessPayload), brightnessPayload, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif
	/*added 8bit brightness support for lcd bl*/
	u8 brightnessPayload_8bit[2] = {MIPI_DCS_SET_DISPLAY_BRIGHTNESS, brightness&0xff};
	struct dsi_cmd_desc panel_dcs_brightness_cmd_8bit = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(brightnessPayload_8bit), brightnessPayload_8bit, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, sizeof(brightnessPayload_8bit), brightnessPayload_8bit, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif

	if (!panel->panel_initialized || os_bootup_completed == 0) {
		panel_hbm_on = false;
		pr_info("iris_handover_dcs_set_display_brightness: ignored, os bootup is still ongoing[%d, %d, %d]\n",
			(int)panel->panel_initialized,
			(int)iris_in_cont_splash_displaying,
			(int)iris_full_functioning);
			return ret;
	}
	if (!record_first_kickoff) {
		/*if the first kickkoff is not triggered yet, ignore the backlight change*/
		lastBrightness = brightness;
		pr_info("%s: backlight change too early, ignored!", __func__);
		return ret;
	}
	if (irisIsAbypassSwitchOngoing()) {
		/*abypass mode switching is still ongoing, ignore backlight changes*/
		savedBrightness = brightness; /*lastBrightness != savedBrightness*/
		need_resume_backlight = true;
		pr_info("%s: backlight change during abypass, ignored!", __func__);
		return ret;
	}
	need_resume_backlight = false;
	if (!record_first_backlight) {
		u32 timeus1 = 0;
		u32 timeus01 = 0;

		record_first_backlight = true;
		ktime_first_backlight = ktime_get();
		timeus01 = (u32)ktime_to_us(ktime_first_backlight) - (u32)ktime_to_us(ktime_first_kickoff);
		if (timeus01 < BACKLIGHT_ON_AFTER_XMS_FIRST_KICKOFF*1000) {
			u32 delayUs = BACKLIGHT_ON_AFTER_XMS_FIRST_KICKOFF*1000 - timeus01;

			usleep_range(delayUs, delayUs + 10);
			timeus1 = (u32)ktime_to_us(ktime_first_backlight);
			if (timeus01 < BACKLIGHT_ON_AFTER_XMS_FIRST_KICKOFF*1000/2)
				pr_info("%s: lpabypass=%d, brightness=%d, hbm=%d, first backlight on at %d, after kickoff for %d",
					__func__,
					iris_info.abypass_ctrl.abypass_status,
					brightness,
					panel_hbm_on,
					timeus1,
					timeus01);
		}
		iris_info.context_info.backlight_on = 1;
	}
	/* added support to Display Dimming*/
#ifdef CONFIG_LCD_DCSBL_CABC_GRADIENT
	iris_handover_dcs_panel_switch_dimming(panel, brightness, state);
#endif
	lastBrightness = brightness;

	if (isAllowedLightupTrace())
		pr_info("%s: bpstatus=%d %d, brightness=%d,hbm=%d,max_level=%d\n", __func__,
			irisIsInAbypassMode(), irisIsInLowPowerAbypassMode(),
			brightness,
			(int)panel_hbm_on,
			(int)panel->bl_config.bl_max_level);

	if (irisIsInAbypassMode() || irisIsInLowPowerAbypassMode()) {
		if (panel_hbm_on)
			return ret;
		if (panel->bl_config.bl_max_level > 255)
			return iris_dsi_cmds_send(panel, &panel_dcs_brightness_cmd, 1, state);
		else
			return iris_dsi_cmds_send(panel, &panel_dcs_brightness_cmd_8bit, 1, state);
	}
	if (panel_hbm_on) {
		if (isAllowedLightupTrace())
			pr_info("iris_handover_dcs_set_display_brightness: brightness=%d, bl_max_level=%d",
					(int)brightness, (int)panel->bl_config.bl_max_level);
		if (brightness < panel->bl_config.bl_max_level) {
			panel_hbm_on = false;
			iris_handover_dcs_enable_panel_hbm(panel, false, DSI_CMD_SET_STATE_LP);
		}
	}
	if (panel->bl_config.bl_max_level > 255)
		ret = iris_handover_panel_dcs_cmd(panel, &panel_dcs_brightness_cmd, state);
	else
		ret = iris_handover_panel_dcs_cmd(panel, &panel_dcs_brightness_cmd_8bit, state);
	if (!panel_hbm_on
		&& (brightness == panel->bl_config.bl_max_level)
		&& iris_hdr_enable_get()) {
		panel_hbm_on = true;
		iris_handover_dcs_enable_panel_hbm(panel, true, DSI_CMD_SET_STATE_LP);
	}

	return ret;
}

int iris_handover_dcs_enable_panel_hbm_kingdisplay(struct dsi_panel *panel, bool setHbm, enum dsi_cmd_set_state state)
{
	u8 cmd0[2] = {0xfe, 0x40};
	u8 cmd1[2] = {0x64, 0x02};
	u8 cmd2[2] = {0xfe, 0x00};
	u8 hbmpayload[2] = {MIPI_DCS_WRITE_CONTROL_DISPLAY, 0xe0};
	int ret = -1;
	struct dsi_cmd_desc panel_dcs_hbm_enter_cmd[] = {
#ifdef NEW_MIPI_DSI_MSG
			{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, 0, sizeof(cmd0), cmd0, 1, iris_read_cmd_buf}, 0, 0},
			{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, 0, sizeof(cmd1), cmd1, 1, iris_read_cmd_buf}, 0, 0},
			{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, 0, sizeof(cmd2), cmd2, 1, iris_read_cmd_buf}, 0, 0},
			{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, 0, sizeof(hbmpayload), hbmpayload, 1, iris_read_cmd_buf}, 0, 0},
	};
#else
			{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(cmd0), cmd0, 1, iris_read_cmd_buf}, 0, 0},
			{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(cmd1), cmd1, 1, iris_read_cmd_buf}, 0, 0},
			{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(cmd2), cmd2, 1, iris_read_cmd_buf}, 0, 0},
			{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(hbmpayload), hbmpayload, 1, iris_read_cmd_buf}, 0, 0},
	};
#endif
	struct dsi_cmd_desc panel_dcs_hbm_exit_cmd[] = {
#ifdef NEW_MIPI_DSI_MSG
			{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, 0, sizeof(cmd2), cmd2, 1, iris_read_cmd_buf}, 0, 0},
			{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, 0, sizeof(hbmpayload), hbmpayload, 1, iris_read_cmd_buf}, 0, 0},
	};
#else
			{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(cmd2), cmd2, 1, iris_read_cmd_buf}, 0, 0},
			{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(hbmpayload), hbmpayload, 1, iris_read_cmd_buf}, 0, 0},
	};
#endif
	if (!panel->panel_initialized)
		return ret;
	if (setHbm)
		hbmpayload[1] = 0xe0;
	else
		hbmpayload[1] = 0x20;
	if (isAllowedLightupTrace())
		pr_info("iris: set kingdisplay HBM: %s", setHbm ? "on" : "off");
	if (irisIsInAbypassMode() || irisIsInLowPowerAbypassMode()) {
		if (setHbm)
			ret = iris_dsi_cmds_send(panel,
						panel_dcs_hbm_enter_cmd,
						ARRAY_SIZE(panel_dcs_hbm_enter_cmd),
						state);
		else
			ret = iris_dsi_cmds_send(panel,
						panel_dcs_hbm_exit_cmd,
						ARRAY_SIZE(panel_dcs_hbm_exit_cmd),
						state);
	} else {
		if (setHbm) {
			ret = iris_handover_panel_dcs_cmd(panel, &panel_dcs_hbm_enter_cmd[0], state);
			ret = iris_handover_panel_dcs_cmd(panel, &panel_dcs_hbm_enter_cmd[1], state);
			ret = iris_handover_panel_dcs_cmd(panel, &panel_dcs_hbm_enter_cmd[2], state);
			ret = iris_handover_panel_dcs_cmd(panel, &panel_dcs_hbm_enter_cmd[3], state);
		} else {
			ret = iris_handover_panel_dcs_cmd(panel, &panel_dcs_hbm_exit_cmd[0], state);
			ret = iris_handover_panel_dcs_cmd(panel, &panel_dcs_hbm_exit_cmd[1], state);
		}
	}
	return ret;
}

int iris_handover_dcs_enable_panel_hbm_samsung(struct dsi_panel *panel, bool setHbm, enum dsi_cmd_set_state state)
{
	/* format hbm enable dcs */
	int ret = -1;
	u8 hbmpayload[2] = {MIPI_DCS_WRITE_CONTROL_DISPLAY, 0xe0};
	struct dsi_cmd_desc panel_dcs_hbm_cmd = {
#ifdef NEW_MIPI_DSI_MSG
			{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, 0, sizeof(hbmpayload), hbmpayload, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
			{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(hbmpayload), hbmpayload, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif
	if (!panel->panel_initialized)
		return ret;
	if (setHbm)
		hbmpayload[1] = 0xe0;
	else
		hbmpayload[1] = 0x28;
	if (isAllowedLightupTrace())
		pr_info("iris: set samsung HBM: %s", setHbm ? "on" : "off");
	if (irisIsInAbypassMode() || irisIsInLowPowerAbypassMode()) {
		return iris_dsi_cmds_send(panel, &panel_dcs_hbm_cmd, 1, state);
	}

	return iris_handover_panel_dcs_cmd(panel, &panel_dcs_hbm_cmd, state);
}

int iris_handover_dcs_enable_panel_hbm(struct dsi_panel *panel, bool setHbm, enum dsi_cmd_set_state state)
{
	int ret = -1;

#ifdef _TEST_PANEL_READ
	irisReadPanelTeConfig(panel, DSI_CMD_SET_STATE_LP);
#endif

	if (connected_lcd_module == LCM_KINGDISPLAY)
		ret = iris_handover_dcs_enable_panel_hbm_kingdisplay(panel, setHbm, state);
	else
		ret = iris_handover_dcs_enable_panel_hbm_samsung(panel, setHbm, state);

	return ret;
}

void iris_prepare_for_hdr_switch(struct dsi_panel *panel, int hdr_en)
{
	if (hdr_en) {
		savedBrightness = lastBrightness;
		if (isAllowedLightupTrace())
			pr_info("iris_prepare_for_hdr_switch: brightness=%d, bl_max_level=%d",
					(int)lastBrightness, (int)panel->bl_config.bl_max_level);
		if (panel->bl_config.bl_max_level == lastBrightness) {
			iris_handover_dcs_set_display_brightness(panel, panel->bl_config.bl_max_level, DSI_CMD_SET_STATE_LP);
			panel_hbm_on = true;
			iris_handover_dcs_enable_panel_hbm(panel, true, DSI_CMD_SET_STATE_LP);
		}
	} else {
		if (irisIsInAbypassHbmEn()) {
			return;
		}
		if (lastBrightness < panel->bl_config.bl_max_level) {
			savedBrightness = lastBrightness;
		}
		if (panel_hbm_on) {
			if (isAllowedLightupTrace())
				pr_info("iris_prepare_for_hdr_switch: saved brightness=%d, last brightness=%d\n",
						(int)savedBrightness, (int)lastBrightness);
			panel_hbm_on = false;
			iris_handover_dcs_enable_panel_hbm(panel, false, DSI_CMD_SET_STATE_LP);
		}
	}
	return;
}

void iris_hbm_direct_switch(struct dsi_panel *panel, int hbm_en)
{
	if (hbm_en) {
		savedBrightness = lastBrightness;
		if (isAllowedLightupTrace())
			pr_info("iris_hbm_direct_switch: brightness=%d, bl_max_level=%d",
					(int)lastBrightness, (int)panel->bl_config.bl_max_level);
		if (panel->bl_config.bl_max_level == lastBrightness) {
			iris_handover_dcs_set_display_brightness(panel,
													panel->bl_config.bl_max_level,
													DSI_CMD_SET_STATE_LP);
			panel_hbm_on = true;
			iris_handover_dcs_enable_panel_hbm(panel, true, DSI_CMD_SET_STATE_LP);
		}
	} else {
		if (irisIsInAbypassHbmEn())
			return;

		if (lastBrightness < panel->bl_config.bl_max_level)
			savedBrightness = lastBrightness;

		if (panel_hbm_on) {
			if (isAllowedLightupTrace())
				pr_info("iris_hbm_direct_switch: saved brightness=%d, last brightness=%d\n",
						(int)savedBrightness, (int)lastBrightness);
			panel_hbm_on = false;
			iris_handover_dcs_enable_panel_hbm(panel, false, DSI_CMD_SET_STATE_LP);
			iris_handover_dcs_set_display_brightness(panel, savedBrightness, DSI_CMD_SET_STATE_LP);
		}
	}
}

void iris_feature_init_cmd_send(struct dsi_panel *panel, enum dsi_cmd_set_state state)
{
	struct feature_setting *chip_setting = &iris_info.chip_setting;
	struct feature_setting *user_setting = &iris_info.user_setting;
	struct iris_setting_update *settint_update = &iris_info.settint_update;
	u32 grcp_len = 0;
	struct dsi_cmd_desc iris_pq_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif

	if (iris_debug_fw_download_disable)
		return;
	if (!iris_info.lut_info.lut_fw_state || !iris_info.gamma_info.gamma_fw_state) {
		/* if ccf files were not loaded yet, try loading them once again. */
		trigger_download_ccf_fw();
		/*if (!iris_info.lut_info.lut_fw_state || !iris_info.gamma_info.gamma_fw_state)
			return;*/
	}

	mutex_lock(&iris_info.dsi_send_mutex);
	memset(&grcp_cmd, 0, sizeof(grcp_cmd));
	memcpy(grcp_cmd.cmd, grcp_header, GRCP_HEADER);
	grcp_cmd.cmd_len = GRCP_HEADER;

	iris_feature_setting_update_check();

	//if (irisIsAllowedPqTrace())
		pr_info("%s, %d: updating: pq %d,dbc %d,memc %d,coloradj %d,lce %d,cmupdate %d(0x%x),lux %d, cct %d,read %d,hdr %d, gamma %d,sdr %d.\n",
			__func__, __LINE__,
			settint_update->pq_setting,
			settint_update->dbc_setting,
			settint_update->lp_memc_setting,
			settint_update->color_adjust,
			settint_update->lce_setting,
			settint_update->cm_setting,
			*((u32 *)&chip_setting->cm_setting),
			settint_update->lux_value,
			settint_update->cct_value,
			settint_update->reading_mode,
			settint_update->hdr_setting,
			settint_update->gamma_table,
			settint_update->sdr_setting);

	if (settint_update->pq_setting) {
		chip_setting->pq_setting = user_setting->pq_setting;
		iris_cmd_reg_add(&grcp_cmd, IRIS_PQ_SETTING_ADDR, *((u32 *)&chip_setting->pq_setting));
		settint_update->pq_setting = false;
	}

	if (settint_update->dbc_setting) {
		chip_setting->dbc_setting = user_setting->dbc_setting;
		iris_cmd_reg_add(&grcp_cmd, IRIS_DBC_SETTING_ADDR, *((u32 *)&chip_setting->dbc_setting));
		settint_update->dbc_setting = false;
	}

	if (settint_update->lp_memc_setting) {
		chip_setting->lp_memc_setting = user_setting->lp_memc_setting;
		iris_cmd_reg_add(&grcp_cmd, IRIS_LPMEMC_SETTING_ADDR, chip_setting->lp_memc_setting.value | 0x80000000);
		settint_update->lp_memc_setting = false;
	}

	if (settint_update->lce_setting) {
		chip_setting->lce_setting = user_setting->lce_setting;
		iris_cmd_reg_add(&grcp_cmd, IRIS_LCE_SETTING_ADDR, *((u32 *)&chip_setting->lce_setting));
		settint_update->lce_setting = false;
	} else {
		iris_cmd_reg_add(&grcp_cmd, IRIS_LCE_SETTING_ADDR, 0x0);
	}

	if (settint_update->cm_setting) {
		chip_setting->cm_setting = user_setting->cm_setting;
		iris_cmd_reg_add(&grcp_cmd, IRIS_CM_SETTING_ADDR, *((u32 *)&chip_setting->cm_setting));
		settint_update->cm_setting = false;
	} else {
		iris_cmd_reg_add(&grcp_cmd, IRIS_CM_SETTING_ADDR, 0x0);
	}

	if (settint_update->reading_mode) {
		chip_setting->reading_mode = user_setting->reading_mode;
		iris_cmd_reg_add(&grcp_cmd, IRIS_READING_MODE_ADDR, *((u32 *)&chip_setting->reading_mode));
		settint_update->reading_mode= false;
	} else {
		iris_cmd_reg_add(&grcp_cmd, IRIS_READING_MODE_ADDR, 0x0);
	}

	if (settint_update->color_adjust) {
		chip_setting->color_adjust = user_setting->color_adjust;
		iris_cmd_reg_add(&grcp_cmd, IRIS_COLOR_ADJUST_ADDR, *((u32 *)&chip_setting->color_adjust));
		settint_update->color_adjust = false;
	} else {
		iris_cmd_reg_add(&grcp_cmd, IRIS_COLOR_ADJUST_ADDR, 0x325a80);
	}

	if (settint_update->oled_brightness) {
		chip_setting->oled_brightness = user_setting->oled_brightness;
		iris_cmd_reg_add(&grcp_cmd, IRIS_OLED_BRIGHT_ADDR, *((u32 *)&chip_setting->oled_brightness));
		settint_update->oled_brightness = false;
	} else {
		iris_cmd_reg_add(&grcp_cmd, IRIS_OLED_BRIGHT_ADDR, 0x0);
	}

	iris_cmd_reg_add(&grcp_cmd, IRIS_HDR_SETTING_ADDR, 0x0);
	iris_cmd_reg_add(&grcp_cmd, IRIS_LUX_VALUE_ADDR, 0x0);
	iris_cmd_reg_add(&grcp_cmd, IRIS_CCT_VALUE_ADDR, 0x0);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DRC_INFO_ADDR, 0x0);
	iris_cmd_reg_add(&grcp_cmd, IRIS_SDR_SETTING_ADDR, 0x0);

	grcp_len = (grcp_cmd.cmd_len - GRCP_HEADER) / 4;
	*(u32 *)(grcp_cmd.cmd + 8) = cpu_to_le32(grcp_len + 1);
	*(u16 *)(grcp_cmd.cmd + 14) = cpu_to_le16(grcp_len);

	iris_pq_cmd.msg.tx_len = grcp_cmd.cmd_len;
	iris_dsi_cmds_send(panel, &iris_pq_cmd, 1, state);

	iris_cmlut_table_load(panel);

	mutex_unlock(&iris_info.dsi_send_mutex);
}


void iris_dtg_setting_cmd_send(struct dsi_panel *panel, enum dsi_cmd_set_state state)
{
	u32 grcp_len = 0;
	struct iris_dtg_setting *dtg_setting = &iris_info.dtg_setting;
	struct dsi_cmd_desc iris_dtg_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif

	mutex_lock(&iris_info.dsi_send_mutex);
	memset(&grcp_cmd, 0, sizeof(grcp_cmd));
	memcpy(grcp_cmd.cmd, grcp_header, GRCP_HEADER);
	grcp_cmd.cmd_len = GRCP_HEADER;

	/*0x29 cmd*/
	//iris_cmd_reg_add(&grcp_cmd, IRIS_MIPI_TX_ADDR + WR_PACKET_HEADER_OFFS, 0x03002905);

	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + DTG_DELAY, dtg_setting->dtg_delay);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + TE_CTRL, dtg_setting->te_ctrl);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + TE_CTRL_1, dtg_setting->te_ctrl_1);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + TE_CTRL_2, dtg_setting->te_ctrl_2);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + TE_CTRL_3, dtg_setting->te_ctrl_3);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + TE_CTRL_4, dtg_setting->te_ctrl_4);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + TE_CTRL_5, dtg_setting->te_ctrl_5);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + DTG_CTRL_1,dtg_setting->dtg_ctrl_1);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + DTG_CTRL, dtg_setting->dtg_ctrl);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + EVS_DLY, dtg_setting->evs_dly);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + EVS_NEW_DLY, 1);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + DVS_CTRL, dtg_setting->dvs_ctrl);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + TE_DLY, dtg_setting->te_dly);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + TE_DLY_1, dtg_setting->te_dly_1);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + VFP_CTRL_0, dtg_setting->vfp_ctrl_0);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + VFP_CTRL_1, dtg_setting->vfp_ctrl_1);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + DTG_RESERVE, 0x1000000b);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DTG_ADDR + DTG_REGSEL, 1);
#ifdef EFUSE_WRITE
	iris_cmd_reg_add(&grcp_cmd, IRIS_SYS_ADDR + DFT_EFUSE_CTRL, 0x80000126);
	iris_cmd_reg_add(&grcp_cmd, IRIS_SYS_ADDR + DFT_EFUSE_CTRL_1, 0x00004005);
	iris_cmd_reg_add(&grcp_cmd, 0xF1620138, 0xFFFF2045);
	iris_cmd_reg_add(&grcp_cmd, 0xF16A0084, 0xFFFF2045);
#endif
	/* set PWM output to 25 Khz */
	iris_cmd_reg_add(&grcp_cmd, 0xf10c0000, 0x000004a4);
	iris_cmd_reg_add(&grcp_cmd, 0xf10c0004, 0x0000ffff);
	iris_cmd_reg_add(&grcp_cmd, 0xf10c0010, 0x00000003);
	iris_cmd_reg_add(&grcp_cmd, 0xf10c0014, 0x00011008);

	iris_cmd_reg_add(&grcp_cmd, IRIS_DPORT_ADDR + DPORT_DBIB, 0x0);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DPORT_ADDR + DPORT_CTRL1, 0x21000803);
	iris_cmd_reg_add(&grcp_cmd, IRIS_DPORT_ADDR + DPORT_REGSEL, 1);

	grcp_len = (grcp_cmd.cmd_len - GRCP_HEADER) / 4;
	*(u32 *)(grcp_cmd.cmd + 8) = cpu_to_le32(grcp_len + 1);
	*(u16 *)(grcp_cmd.cmd + 14) = cpu_to_le16(grcp_len);

	iris_dtg_cmd.msg.tx_len = grcp_cmd.cmd_len;
	iris_dsi_cmds_send(panel, &iris_dtg_cmd, 1, state);
	mutex_unlock(&iris_info.dsi_send_mutex);

	DUMP_PACKET(grcp_cmd.cmd, grcp_cmd.cmd_len);

	pr_err("iris: dtg set: %d\n", (int)grcp_cmd.cmd_len);
}

void iris_pwil_mode_set(struct dsi_panel *panel, u8 mode, enum dsi_cmd_set_state state)
{
	char pwil_mode[2] = {0x00, 0x00};
	struct dsi_cmd_desc iris_pwil_mode_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0, 0, 0, sizeof(pwil_mode), pwil_mode, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM, 0, 0, sizeof(pwil_mode), pwil_mode, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif

	if (PT_MODE == mode) {
		pwil_mode[0] = 0x0;
		pwil_mode[1] = 0x1;
	} else if (RFB_MODE == mode) {
		pwil_mode[0] = 0xc;
		pwil_mode[1] = 0x1;
	} else if (BIN_MODE == mode) {
		pwil_mode[0] = 0xc;
		pwil_mode[1] = 0x20;
	} else if (FRC_MODE == mode) {
		pwil_mode[0] = 0x4;
		pwil_mode[1] = 0x2;
	}

	iris_dsi_cmds_send(panel, &iris_pwil_mode_cmd, 1, state);
	DUMP_PACKET(pwil_mode, sizeof(pwil_mode));
}

void iris_mipi_mem_addr_cmd_send(struct dsi_panel *panel, u16 column, u16 page, enum dsi_cmd_set_state state)
{
	char mem_addr[2] = {0x36, 0x0};
	char pixel_format[2] = {0x3a, 0x77};
	char col_addr[5] = {0x2a, 0x00, 0x00, 0x03, 0xff};
	char page_addr[5] = {0x2b, 0x00, 0x00, 0x03, 0xff};
	struct dsi_cmd_desc iris_mem_addr_cmd[] = {
#ifdef NEW_MIPI_DSI_MSG
		{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, 0, sizeof(mem_addr), mem_addr, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, 0, sizeof(pixel_format), pixel_format, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(col_addr), col_addr, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(page_addr), page_addr, 1, iris_read_cmd_buf}, 1, 0},
#else
		{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(mem_addr), mem_addr, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(pixel_format), pixel_format, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, sizeof(col_addr), col_addr, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, sizeof(page_addr), page_addr, 1, iris_read_cmd_buf}, 1, 0},
#endif
	};

	col_addr[3] = (column >> 8) & 0xff;
	col_addr[4] = column & 0xff;
	page_addr[3] = (page >> 8) & 0xff;
	page_addr[4] = page & 0xff;

	if (isAllowedLightupTrace())
		pr_err("iris: set mipi mem addr: %x, %x\n", column, page);
	iris_dsi_cmds_send(panel, iris_mem_addr_cmd, ARRAY_SIZE(iris_mem_addr_cmd), state);
	DUMP_PACKET(mem_addr, sizeof(mem_addr));
	DUMP_PACKET(pixel_format, sizeof(pixel_format));
	DUMP_PACKET(col_addr, sizeof(col_addr));
	DUMP_PACKET(page_addr, sizeof(page_addr));
}

int iris_firmware_data_read(const u8 *fw_data, size_t fw_size)
{
	u32 pkt_size = FW_COL_CNT * 3, cmd_len = 0, cmd_cnt = 0, buf_indx = 0;


	if(!app_fw_buf) {
		app_fw_buf = kzalloc(DSI_DMA_TX_BUF_SIZE, GFP_KERNEL);
		if (!app_fw_buf) {
			pr_err("%s: failed to alloc app fw mem, size = %d\n", __func__, DSI_DMA_TX_BUF_SIZE);
			return false;
		}
	}
	memset(app_fw_buf, 0, DSI_DMA_TX_BUF_SIZE);

	while (fw_size) {
		if (fw_size >= pkt_size)
			cmd_len = pkt_size;
		else
			cmd_len = fw_size;

		if (0 == cmd_cnt)
			app_fw_buf[0] = DCS_WRITE_MEM_START;
		else
			app_fw_buf[buf_indx] = DCS_WRITE_MEM_CONTINUE;

		memcpy(app_fw_buf + buf_indx + 1, fw_data, cmd_len);

		fw_size -= cmd_len;
		fw_data += cmd_len;
		cmd_cnt++;
		buf_indx += cmd_len + 1;
	}
	return true;
}

int iris_firmware_download_init(struct dsi_panel *panel, const char *name)
{
	const struct firmware *fw = NULL;
	int ret = 0, result = true;

	//iris_info.firmware_info.app_fw_size = 0;
	if (name) {
		/* Firmware file must be in /system/etc/firmware/ */
		ret = request_firmware(&fw, name, iris_drm->dev);
		if (ret) {
			pr_err("%s: failed to request firmware: %s, ret = %d\n",
					__func__, name, ret);
			result = false;
		} else {
			pr_err("%s: request firmware: name = %s, size = %zu bytes\n",
						__func__, name, fw->size);
			if (iris_firmware_data_read(fw->data, fw->size))
				iris_info.firmware_info.app_fw_size = fw->size;
			release_firmware(fw);
		}
	} else {
		pr_err("%s: firmware is null\n", __func__);
		result = false;
	}

	return result;
}

void iris_firmware_download_prepare(struct dsi_panel *panel, size_t size)
{
#define TIME_INTERVAL 20  /*ms*/
	u32 bin_capt_size = 0x04380780;

	char fw_download_config[] = {
		PWIL_TAG('P', 'W', 'I', 'L'),
		PWIL_TAG('G', 'R', 'C', 'P'),
		PWIL_U32(0x000000013),
		0x00,
		0x00,
		PWIL_U16(0x0012),
		PWIL_U32(IRIS_PWIL_ADDR + 0x0004),  /*PWIL ctrl1 confirm transfer mode and cmd mode, single channel.*/
		PWIL_U32(0x00004144),
		PWIL_U32(IRIS_PWIL_ADDR + 0x0218),  /*CAPEN*/
		PWIL_U32(0xc0000003),
		PWIL_U32(IRIS_PWIL_ADDR + 0x1140),  /*channel order*/
		PWIL_U32(0xc6120010),
		PWIL_U32(IRIS_PWIL_ADDR + 0x1144),  /*pixelformat*/
		PWIL_U32(0x888),
		PWIL_U32(IRIS_PWIL_ADDR + 0x114c),  /*capt size*/
		PWIL_U32(0x08c80438),
		PWIL_U32(IRIS_PWIL_ADDR + 0x1158),	/*mem addr*/
		PWIL_U32(0x00000000),
		PWIL_U32(IRIS_PWIL_ADDR + 0x10000), /*update setting. using SW update mode*/
		PWIL_U32(0x00000100),
		PWIL_U32(IRIS_PWIL_ADDR + 0x1fff0), /*clear down load int*/
		PWIL_U32(0x00008000),
		PWIL_U32(IRIS_MIPI_RX_ADDR + 0xc),	/*mipi_rx setting DBI_bus*/
		PWIL_U32(0x000f0000),
		PWIL_U32(IRIS_MIPI_RX_ADDR + 0x001c), /*mipi_rx time out threshold*/
		PWIL_U32(0xffffffff)
	};
	u32 threshold = 0, fw_hres, fw_vres;
	struct dsi_cmd_desc fw_download_config_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, 0, sizeof(fw_download_config), fw_download_config, 1, iris_read_cmd_buf}, 1, 0};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(fw_download_config), fw_download_config, 1, iris_read_cmd_buf}, 1, 0};
#endif

	bin_capt_size = (iris_info.input_timing.vres <<16) + iris_info.input_timing.hres;
	*(u32 *)(fw_download_config + 52) = cpu_to_le32(bin_capt_size);

	threshold = 200000; //ctrl->pclk_rate / 1000;
	threshold *= TIME_INTERVAL;
	*(u32 *)(fw_download_config + 92) = cpu_to_le32(threshold);

	/*firmware download need mipirx work on single cmd mode, pwil work on binarary mode.*/
	iris_dsi_cmds_send(panel, &fw_download_config_cmd, 1, DSI_CMD_SET_STATE_HS);


	iris_pwil_mode_set(panel, BIN_MODE, DSI_CMD_SET_STATE_HS);

	fw_hres = FW_COL_CNT - 1;
	fw_vres = (size + FW_COL_CNT * 3 - 1) / (FW_COL_CNT * 3) - 1;
	iris_mipi_mem_addr_cmd_send(panel, fw_hres, fw_vres, DSI_CMD_SET_STATE_HS);

}

int iris_firmware_data_send(struct dsi_panel *panel, u8 *buf, size_t fw_size)
{
	u32 pkt_size = FW_COL_CNT * 3, cmd_len = 0, cmd_cnt = 0, buf_indx = 0, cmd_indx = 0;
	static struct dsi_cmd_desc fw_send_cmd[FW_DW_CMD_CNT];

	memset(fw_send_cmd, 0, sizeof(fw_send_cmd));

	while (fw_size) {
		if (fw_size >= pkt_size)
			cmd_len = pkt_size;
		else
			cmd_len = fw_size;

		cmd_indx = cmd_cnt % FW_DW_CMD_CNT;
		fw_send_cmd[cmd_indx].last_command = 0;
		fw_send_cmd[cmd_indx].msg.type = 0x39;
		fw_send_cmd[cmd_indx].msg.tx_len = pkt_size + 1;
		fw_send_cmd[cmd_indx].msg.tx_buf = buf + buf_indx;
		fw_send_cmd[cmd_indx].msg.rx_len = 1;
		fw_send_cmd[cmd_indx].msg.rx_buf = iris_read_cmd_buf;

		fw_size -= cmd_len;
		cmd_cnt++;
		buf_indx += cmd_len + 1;

		if (((FW_DW_CMD_CNT - 1) == cmd_indx) || (0 == fw_size)) {
			fw_send_cmd[cmd_indx].last_command = 1;
			fw_send_cmd[cmd_indx].post_wait_ms = 0;
			iris_dsi_cmds_send(panel, fw_send_cmd, cmd_indx + 1, DSI_CMD_SET_STATE_HS);
		}
	}
	return true;
}

u16 iris_mipi_fw_download_result_read(struct dsi_panel *panel)
{
	char mipirx_status[1] = {0xaf};
	const struct mipi_dsi_host_ops *ops = panel->host->ops;
	struct dsi_cmd_desc cmds = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_DCS_READ, 0, 0, 0, sizeof(mipirx_status), mipirx_status, 2, iris_read_cmd_buf}, 1, 0};
#else
		{0, MIPI_DSI_DCS_READ, 0, 0, sizeof(mipirx_status), mipirx_status, 2, iris_read_cmd_buf}, 1, 0};
#endif
	int rc = 0;
	u16 data = 0xffff;

	memset(iris_read_cmd_buf, 0, sizeof(iris_read_cmd_buf));
	cmds.msg.flags |= MIPI_DSI_MSG_USE_LPM | MIPI_DSI_MSG_LASTCOMMAND;
	cmds.msg.flags |= BIT(6);

	rc = ops->transfer(panel->host, &cmds.msg);
	if (rc < 0) {
		pr_err("iris_mipi_fw_download_result_read failed to set cmds(%d), rc=%d\n", cmds.msg.type, rc);
		return 0xff;
	}
	data = iris_read_cmd_buf[0] | ((iris_read_cmd_buf[1] & 0x0f) << 8);
	pr_err("iris download_reslut: %x\n", data);

	return (data & 0x0f00);
}

u16 iris_i2c_fw_download_result_read(void)
{
	u32 reg_value = 0;

	iris_i2c_reg_read(IRIS_MIPI_RX_ADDR + INTSTAT_RAW, &reg_value);
	reg_value &= (1 << 10);

	return (u16)(reg_value >> 2);
}

int iris_fw_download_result_check(struct dsi_panel *panel)
{
#define FW_CHECK_COUNT 20
	u16 cnt = 0, result = 0;
//	u32 reg_value;

	do {
		#ifdef ENABLE_IRIS_I2C_READ
			result = iris_i2c_fw_download_result_read();
		#else
			result = iris_mipi_fw_download_result_read(panel);
		#endif
		if (0x0100 == result)
			break;

		usleep_range(1000, 1010);
		cnt++;
	} while ((result != 0x0100) && cnt < FW_CHECK_COUNT);
/*
#ifdef I2C_ENABLE
	iris_i2c_reg_read(IRIS_MIPI_RX_ADDR + INTSTAT_RAW, &reg_value);
#else
	iris_mipi_reg_read(IRIS_MIPI_RX_ADDR + INTSTAT_RAW, &reg_value);
#endif
	if (reg_value & (0x3 << 7)) {
		pr_err("abnormal error: %x\n", reg_value);
		iris_i2c_reg_write(IRIS_MIPI_RX_ADDR + INTCLR, 0x780);
		return false;
	}
*/
	/*read failed*/
	if (FW_CHECK_COUNT == cnt) {
		pr_err("iris firmware download failed\n");
		return false;
	} else
		pr_info("iris firmware download success\n");

	return true;
}

void iris_firmware_download_restore(struct dsi_panel *panel, bool cont_splash)
{
	char fw_download_restore[] = {
		PWIL_TAG('P', 'W', 'I', 'L'),
		PWIL_TAG('G', 'R', 'C', 'P'),
		PWIL_U32(0x00000007),
		0x00,
		0x00,
		PWIL_U16(0x06),
		PWIL_U32(IRIS_PWIL_ADDR + 0x0004),
		PWIL_U32(0x00004140),
		PWIL_U32(IRIS_MIPI_RX_ADDR + 0xc),
		PWIL_U32(0x000f0000),
		PWIL_U32(IRIS_MIPI_RX_ADDR + 0x001c),
		PWIL_U32(0xffffffff)
	};
	u32 col_addr = 0, page_addr = 0;
	struct dsi_cmd_desc fw_download_restore_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, 0, sizeof(fw_download_restore), fw_download_restore, 1, iris_read_cmd_buf}, 1, 0};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(fw_download_restore), fw_download_restore, 1, iris_read_cmd_buf}, 1, 0};
#endif

	if (DSI_OP_CMD_MODE == iris_info.work_mode.rx_mode)
		fw_download_restore[20] += (2 << 1);

	if (1 == iris_info.work_mode.rx_ch) {
		fw_download_restore[20] += 1;
		fw_download_restore[30] = 0x8f;
	}

	if (1 == iris_info.work_mode.rx_ch)
		col_addr = iris_info.input_timing.hres * 2 - 1;
	else
		col_addr = iris_info.input_timing.hres - 1;

	page_addr = iris_info.input_timing.vres - 1;

	iris_dsi_cmds_send(panel, &fw_download_restore_cmd, 1, DSI_CMD_SET_STATE_HS);

	if (cont_splash) {
		iris_pwil_mode_set(panel, RFB_MODE, DSI_CMD_SET_STATE_HS);

		if (DSI_OP_CMD_MODE == iris_info.work_mode.rx_mode)
			iris_mipi_mem_addr_cmd_send(panel, col_addr, page_addr, DSI_CMD_SET_STATE_HS);
	}
}

int iris_firmware_download(struct dsi_panel *panel,
							const char *name,
							bool cont_splash)
{
#define FW_DOWNLOAD_RETRYCNT_MAX 3
	int ret = true, result = false, cnt = 0;
	int fw_size = 0, cm3d = iris_info.user_setting.cm_setting.cm3d;

	if (!iris_info.firmware_info.app_fw_size)
		ret = iris_firmware_download_init(panel, name);

	if (true == ret) {
		fw_size = iris_info.firmware_info.app_fw_size;

		//if (!cm3d)
		iris_firmware_cmlut_update(app_fw_buf, cm3d);
		iris_firmware_gamma_table_update(app_fw_buf, cm3d);

		iris_firmware_download_prepare(panel, fw_size);
		for (cnt = 0; cnt < FW_DOWNLOAD_RETRYCNT_MAX; cnt++) {
			iris_firmware_data_send(panel, app_fw_buf, fw_size);
#ifdef READ_CMD_ENABLE
			result = iris_fw_download_result_check(panel);
#else
			result = true;
			usleep_range(10000, 10010);
#endif
			if (result == true)
				break;
			pr_info("iris firmware download again\n");
		}
		iris_firmware_download_restore(panel, cont_splash);
	}
	return result;
}

int iris_lut_firmware_init(const char *name)
{
	const struct firmware *fw = NULL;
	struct iris_lut_info *lut_info = &iris_info.lut_info;
	int indx, ret = 0;

	if (lut_info->lut_fw_state)
		return true;

	if (!lut_fw_buf) {
		lut_fw_buf = kzalloc(IRIS_CM_LUT_LENGTH * 4 * CMI_TOTAL, GFP_KERNEL);
		if (!lut_fw_buf) {
			pr_err("%s: failed to alloc  lut fw mem, size = %d\n", __func__, IRIS_CM_LUT_LENGTH * 4 * CMI_TOTAL);
			return false;
		}
	}

	for (indx = 0; indx < CMI_TOTAL; indx++)
		lut_info->cmlut[indx] = (u32 *)(lut_fw_buf + indx * IRIS_CM_LUT_LENGTH * 4);

	if (name) {
		/* Firmware file must be in /system/etc/firmware/ */
		ret = request_firmware(&fw, name, iris_drm->dev);
		if (ret) {
			pr_err("%s: failed to request firmware: %s, ret = %d\n",
					__func__, name, ret);
			return false;
		} else {
			pr_info("%s: request firmware: name = %s, size = %zu bytes\n",
						__func__, name, fw->size);
			memcpy(lut_fw_buf, fw->data, fw->size);
			lut_info->lut_fw_state = true;
			release_firmware(fw);
		}
	} else {
		pr_err("%s: firmware is null\n", __func__);
		return false;
	}

	return true;
}

int iris_gamma_firmware_init(const char *name)
{
	const struct firmware *fw = NULL;
	struct iris_gamma_info *gamma_info = &iris_info.gamma_info;
	int ret = 0;

	if (gamma_info->gamma_fw_state)
		return true;

	if (!gamma_fw_buf) {
		gamma_fw_buf = kzalloc(8 * 1024, GFP_KERNEL);
		if (!gamma_fw_buf) {
			pr_err("%s: failed to alloc gamma fw mem, size = %d\n", __func__, 8 * 1024);
			return false;
		}
	}

	if (name) {
		/* Firmware file must be in /system/etc/firmware/ */
		ret = request_firmware(&fw, name, iris_drm->dev);
		if (ret) {
			pr_err("%s: failed to request firmware: %s, ret = %d\n",
					__func__, name, ret);
			return false;
		} else {
			pr_info("%s: request firmware: name = %s, size = %zu bytes\n",
						__func__, name, fw->size);
			memcpy(gamma_fw_buf, fw->data, fw->size);
			gamma_info->gamma_fw_size = fw->size;
			gamma_info->gamma_fw_state = true;
			release_firmware(fw);
		}
	} else {
		pr_err("%s: firmware is null\n", __func__);
		return false;
	}

	return true;
}

void iris_firmware_cmlut_table_copy(u8 *fw_buf, u32 lut_offset, u8 *lut_buf, u32 lut_size)
{
	u32 pkt_size = FW_COL_CNT * 3, buf_offset, pkt_offset;
	u32 len;

	buf_offset = (lut_offset / pkt_size) *(pkt_size + 1);
	pkt_offset = lut_offset % pkt_size;
	fw_buf += buf_offset + pkt_offset + 1;

	while (lut_size) {
		if (pkt_offset) {
			len = pkt_size - pkt_offset;
			pkt_offset = 0;
		} else if (lut_size > pkt_size)
			len = pkt_size;
		else
			len = lut_size;

		memcpy(fw_buf, lut_buf, len);
		fw_buf += len + 1;
		lut_buf += len;
		lut_size -= len;
	}
}

void iris_firmware_cmlut_update(u8 *buf, u32 cm3d)
{
	struct iris_lut_info *lut_info = &iris_info.lut_info;

	if (!lut_info->lut_fw_state) {
		pr_err("iris_firmware_cmlut_update: cm lut not available.\n");
		return;
	}

	if (isAllowedLightupTrace())
		pr_info("iris_firmware_cmlut_update: copy cm3d(%d).\n", cm3d);

	switch (cm3d) {
		case CM_NTSC:		//table for NTSC
		iris_firmware_cmlut_table_copy(buf, IRIS_CMLUT_2800K_ADDR, (u8 *)(lut_info->cmlut[CMI_NTSC_2800]), IRIS_CM_LUT_LENGTH * 4);
		iris_firmware_cmlut_table_copy(buf, IRIS_CMLUT_6500K_ADDR, (u8 *)(lut_info->cmlut[CMI_NTSC_6500_C51_ENDIAN]), IRIS_CM_LUT_LENGTH * 4);
		iris_firmware_cmlut_table_copy(buf, IRIS_CMLUT_8000K_ADDR, (u8 *)(lut_info->cmlut[CMI_NTSC_8000]), IRIS_CM_LUT_LENGTH * 4);
		break;
		case CM_sRGB:		//table for SRGB
		iris_firmware_cmlut_table_copy(buf, IRIS_CMLUT_2800K_ADDR, (u8 *)(lut_info->cmlut[CMI_SRGB_2800]), IRIS_CM_LUT_LENGTH * 4);
		iris_firmware_cmlut_table_copy(buf, IRIS_CMLUT_6500K_ADDR, (u8 *)(lut_info->cmlut[CMI_SRGB_6500_C51_ENDIAN]), IRIS_CM_LUT_LENGTH * 4);
		iris_firmware_cmlut_table_copy(buf, IRIS_CMLUT_8000K_ADDR, (u8 *)(lut_info->cmlut[CMI_SRGB_8000]), IRIS_CM_LUT_LENGTH * 4);
		break;
		case CM_DCI_P3:		//table for DCI-P3
		iris_firmware_cmlut_table_copy(buf, IRIS_CMLUT_2800K_ADDR, (u8 *)(lut_info->cmlut[CMI_P3_2800]), IRIS_CM_LUT_LENGTH * 4);
		iris_firmware_cmlut_table_copy(buf, IRIS_CMLUT_6500K_ADDR, (u8 *)(lut_info->cmlut[CMI_P3_6500_C51_ENDIAN]), IRIS_CM_LUT_LENGTH * 4);
		iris_firmware_cmlut_table_copy(buf, IRIS_CMLUT_8000K_ADDR, (u8 *)(lut_info->cmlut[CMI_P3_8000]), IRIS_CM_LUT_LENGTH * 4);
		break;
		case CM_HDR:		//table for hdr
		iris_firmware_cmlut_table_copy(buf, IRIS_CMLUT_2800K_ADDR, (u8 *)(lut_info->cmlut[CMI_HDR_2800]), IRIS_CM_LUT_LENGTH * 4);
		iris_firmware_cmlut_table_copy(buf, IRIS_CMLUT_6500K_ADDR, (u8 *)(lut_info->cmlut[CMI_HDR_6500_C51_ENDIAN]), IRIS_CM_LUT_LENGTH * 4);
		iris_firmware_cmlut_table_copy(buf, IRIS_CMLUT_8000K_ADDR, (u8 *)(lut_info->cmlut[CMI_HDR_6500_C51_ENDIAN]), IRIS_CM_LUT_LENGTH * 4);
		break;
	}
}
#ifdef EFUSE_WRITE
void iris_efuse_write(struct dsi_panel *panel)
{
	u32 grcp_len = 0;
	struct dsi_cmd_desc iris_efusewrite_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif

	mutex_lock(&iris_info.dsi_send_mutex);
	memset(&grcp_cmd, 0, sizeof(grcp_cmd));
	memcpy(grcp_cmd.cmd, grcp_header, GRCP_HEADER);
	grcp_cmd.cmd_len = GRCP_HEADER;

	iris_cmd_reg_add(&grcp_cmd, IRIS_SYS_ADDR + DFT_EFUSE_CTRL, 0x80000126);
	iris_cmd_reg_add(&grcp_cmd, IRIS_SYS_ADDR + DFT_EFUSE_CTRL_1, 0x00004005);
	iris_cmd_reg_add(&grcp_cmd, IRIS_SYS_ADDR + 0x28, 0x1);
	iris_cmd_reg_add(&grcp_cmd, IRIS_SYS_ADDR + 0x28, 0x3);
	iris_cmd_reg_add(&grcp_cmd, IRIS_SYS_ADDR + 0x28, 0x2);

	grcp_len = (grcp_cmd.cmd_len - GRCP_HEADER) / 4;
	*(u32 *)(grcp_cmd.cmd + 8) = cpu_to_le32(grcp_len + 1);
	*(u16 *)(grcp_cmd.cmd + 14) = cpu_to_le16(grcp_len);

	iris_efusewrite_cmd.msg.tx_len = grcp_cmd.cmd_len;
	iris_dsi_cmds_send(panel, &iris_efusewrite_cmd, 1, DSI_CMD_SET_STATE_LP);
	mutex_unlock(&iris_info.dsi_send_mutex);

	//DUMP_PACKET(grcp_cmd.cmd, grcp_cmd.cmd_len);
}
#endif

void iris_bp_cmd_send(struct dsi_panel *panel, enum dsi_cmd_set_state state);

static void iris_sys_bp_lightup(struct dsi_panel *panel)
{
	pr_err("iris_sys_bp_lightup: via sys mipi cmd.\n");
	iris_mipirx_mode_set(panel, PWIL_CMD, DSI_CMD_SET_STATE_LP);
	iris_bp_cmd_send(panel, DSI_CMD_SET_STATE_LP);
}

void iris_bp_lightup(struct dsi_panel *panel)
{
	pr_err("iris_bp_lightup: start.\n");
	iris_sys_bp_lightup(panel);
	pr_err("iris_bp_lightup: end.\n");
	usleep_range(50*1000, 50*1000);
}

void trigger_download_ccf_fw(void)
{
	pr_err("iris triggering ccf fw downloading ...\n");
	iris_lut_firmware_init(LUT_FIRMWARE_NAME);
	iris_gamma_firmware_init(GAMMA_FIRMWARE_NAME);
}

void iris_gamma_table_endian_convert(u8 *dst_buf, u8 *src_buf)
{
	u32 cnt = IRIS_GAMMA_TABLE_LENGTH * 3;

	for (cnt = 0; cnt < IRIS_GAMMA_TABLE_LENGTH * 3; cnt++) {
		*dst_buf = *(src_buf + 3);
		*(dst_buf + 1) = *(src_buf + 2);
		*(dst_buf + 2) = *(src_buf + 1);
		*(dst_buf + 3) = *src_buf;

		dst_buf += 4;
		src_buf += 4;
	}
}

void iris_firmware_gamma_table_update(u8 *buf, u32 cm3d)
{
	u8 *table_buf = gamma_fw_buf;
	u8 *tmp_buf = gamma_fw_buf + 6 * 1024;

	if (!iris_info.gamma_info.gamma_fw_state || !gamma_fw_buf) {
		pr_err("iris_firmware_gamma_table_update: ccf not ready yet(cm3d=%d).\n", cm3d);
		return;
	}

	if (isAllowedLightupTrace())
		pr_err("iris_firmware_gamma_table_update: gamma_fw_state=%d, cm3d=%d.\n",
				(int)iris_info.gamma_info.gamma_fw_state,
				(int)cm3d);

	table_buf += 16;
	switch (cm3d) {
	case CM_NTSC:
		iris_gamma_table_endian_convert(tmp_buf, table_buf);
		iris_firmware_cmlut_table_copy(buf, IRIS_GAMMA_TABLE_ADDR, tmp_buf, IRIS_GAMMA_TABLE_LENGTH * 3 * 4);
		break;
	case CM_sRGB:
		table_buf += IRIS_GAMMA_TABLE_LENGTH * 3 * 4;
		iris_gamma_table_endian_convert(tmp_buf, table_buf);
		iris_firmware_cmlut_table_copy(buf, IRIS_GAMMA_TABLE_ADDR, tmp_buf, IRIS_GAMMA_TABLE_LENGTH * 3 * 4);
		break;
	case CM_DCI_P3:
		table_buf += 2 * IRIS_GAMMA_TABLE_LENGTH * 3 * 4;
		iris_gamma_table_endian_convert(tmp_buf, table_buf);
		iris_firmware_cmlut_table_copy(buf, IRIS_GAMMA_TABLE_ADDR, tmp_buf, IRIS_GAMMA_TABLE_LENGTH * 3 * 4);
		break;
	default:
		pr_err("iris_firmware_gamma_table_update: did nothing.\n");
		break;
	}
}

void iris_reset_failure_recover(struct dsi_panel *panel)
{
	u8 cnt = 0;
	int ret = 0;

	do {
		/* reset iris */
		if (gpio_is_valid(panel->iris_hw_cfg.iris_reset_gpio)) {
			gpio_direction_output(panel->iris_hw_cfg.iris_reset_gpio, 0);
			usleep_range(5000, 5010);
			gpio_direction_output(panel->iris_hw_cfg.iris_reset_gpio, 1);
			usleep_range(5000, 5010);
		}
		ret = iris_Romcode_state_check(panel, 0x01, 5, true);
		cnt++;
		pr_err("iris_reset_failure_recover: %d\n", cnt);
	} while((cnt < 3) && (ret < 0));
}

void iris_pre_lightup(struct dsi_panel *panel);

int iris_init_failure_recover(struct dsi_panel *panel)
{
	u8 cnt = 0;
	int ret = 0;

	do {
		/* reset iris */
		if (gpio_is_valid(panel->iris_hw_cfg.iris_reset_gpio)) {
			gpio_direction_output(panel->iris_hw_cfg.iris_reset_gpio, 0);
			usleep_range(5000, 5010);
			gpio_direction_output(panel->iris_hw_cfg.iris_reset_gpio, 1);
			usleep_range(5000, 5010);
		}
		pr_err("iris_init_failure_recovering: %d\n", cnt);
		iris_pre_lightup(panel);
		iris_mipirx_mode_set(panel, PWIL_CMD, DSI_CMD_SET_STATE_HS);
		/* wait Romcode ready */
		ret = iris_Romcode_state_check(panel, 0x03, 5, true);
		cnt++;
		pr_err("iris_init_failure_recover: %d\n", cnt);
	} while ((cnt < 3) && (ret < 0));

	return ret;
}

int iris_enable_failure_recover(struct dsi_panel *panel)
{
	u8 cnt = 0;
	int ret = 0;
	u32 column, page;

	do {
		ret = iris_init_failure_recover(panel);
		if (ret != 0)
			break;

		iris_dtg_setting_cmd_send(panel, DSI_CMD_SET_STATE_HS);
		iris_ctrl_cmd_send(panel, ENABLE_DPORT, DSI_CMD_SET_STATE_HS);
		iris_pwil_mode_set(panel, RFB_MODE, DSI_CMD_SET_STATE_HS);
		/* update rx column/page addr */
		column = iris_info.work_mode.rx_ch ?
				(iris_info.input_timing.hres * 2 - 1) : (iris_info.input_timing.hres - 1);
		page = iris_info.input_timing.vres - 1;
		iris_mipi_mem_addr_cmd_send(panel, column, page, DSI_CMD_SET_STATE_HS);
		/* wait Romcode ready */
		ret = iris_Romcode_state_check(panel, 0x07, 5, true);
		cnt++;
		pr_err("iris_enable_failure_recover: %d\n", cnt);
	} while((cnt < 3) && (ret < 0));

	return ret;
}

void iris_firmware_download_cont_splash(struct dsi_panel *panel)
{
	u32 cmd = 0;
	static bool first_boot = true;
	int ret = 0;

	if (!first_boot)
		return;

	pr_info("iris_firmware_download_cont_splash\n");
	first_boot = false;
	iris_in_cont_splash_displaying = 1;
	panel_expected_c0 = 0x0;
	panel_hbm_on = false;
	iris_info.context_info.backlight_on = 1;
	iris_info.context_info.iris_stay_in_abypass_en = 0;
	if (irisIsInAbypassMode()) {
		iris_mode_switch_state_reset();
		iris_info.power_status.low_power_state = LP_STATE_POWER_UP;
		os_bootup_completed = 1;
		pr_info("%s: irisIsInAbypassMode!", __func__);
		return;
	}

	if (!strcmp(panel->name, "SAM_S6E3FA3_SAM_1080x2248_6P2Inch_video_nolcd")) {
		iris_debug_fw_download_disable = 1;
		pr_info("%s: no lcd, ignored firmware download!", __func__);
	}

	if (iris_debug_fw_download_disable) {
		iris_info.firmware_info.app_fw_state = false;
		iris_info.firmware_info.app_cnt = 0;
	} else {
#if 0
		/*should not request ccf here, because pcs may be ongoing.*/
		iris_lut_firmware_init(LUT_FIRMWARE_NAME);
		iris_gamma_firmware_init(GAMMA_FIRMWARE_NAME);
#endif
		/*wait Romcode ready*/
		ret = iris_Romcode_state_check(panel, 0x03, 5, true);
		pr_info("%s: romcode powermode check, ret=%d\n", __func__, ret);
		if (ret < 0 || irisIsSimulatedEnableFailureRecover())
			iris_enable_failure_recover(panel);

		/* firmware download */
		iris_info.firmware_info.app_fw_state =
				iris_firmware_download(panel, APP_FIRMWARE_NAME, true);
		if (true == iris_info.firmware_info.app_fw_state) {
			cmd = REMAP + ITCM_COPY;
			iris_ctrl_cmd_send(panel, cmd, DSI_CMD_SET_STATE_HS);
			iris_info.firmware_info.app_cnt = 0;
			iris_full_functioning = 1;
			iris_hw_status_check_enabled = true;
			iris_hw_status_check_started = true;
		} else {
			/*firmware NOT loaded*/
			//iris_debug_fw_download_disable = 1;
		}
	}
	iris_feature_setting_update_check();
	iris_mode_switch_state_reset();
	iris_info.power_status.low_power_state = LP_STATE_POWER_UP;
	os_bootup_completed = 1;
}

void iris_dport_enable_cmd_send(struct dsi_panel *panel, bool enable)
{
	char dport_enable[] = {
		PWIL_TAG('P', 'W', 'I', 'L'),
		PWIL_TAG('G', 'R', 'C', 'P'),
		PWIL_U32(0x00000005),
		0x00,
		0x00,
		PWIL_U16(0x04),
		PWIL_U32(IRIS_DPORT_ADDR + DPORT_CTRL1),
		PWIL_U32(0x21000803),
		PWIL_U32(IRIS_DPORT_ADDR + DPORT_REGSEL),
		PWIL_U32(0x000f0001)
	};
	struct dsi_cmd_desc dport_enable_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, 0, sizeof(dport_enable), dport_enable, 1, iris_read_cmd_buf}, 1, 16};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(dport_enable), dport_enable, 1, iris_read_cmd_buf}, 1, 16};
#endif

	if (enable)
		dport_enable[20] = 0x03;
	else
		dport_enable[20] = 0x00;

	iris_dsi_cmds_send(panel, &dport_enable_cmd, 1, DSI_CMD_SET_STATE_HS);
}


void iris_power_down(void)
{
#ifdef I2C_ENABLE
	iris_i2c_reg_write(0xf104000c, 0xffffffff);

	iris_i2c_reg_write(0xf0000210, 0x0000070b);
	iris_i2c_reg_write(0xf0000214, 0x0000070b);
	iris_i2c_reg_write(0xf0000218, 0x0003ff03);
	iris_i2c_reg_write(0xf000021c, 0x0000070b);
	iris_i2c_reg_write(0xf0000228, 0x0000070b);
	iris_i2c_reg_write(0xf0000014, 0x00000001);
	iris_i2c_reg_write(0xf0000014, 0x00000000);

	iris_i2c_reg_write(0xf0000140, 0x00000003);
	iris_i2c_reg_write(0xf0000150, 0x00002003);
	iris_i2c_reg_write(0xf0000160, 0x00000002);

	iris_i2c_reg_write(0xf0180000, 0x0a00c138);
	iris_i2c_reg_write(0xf0180004, 0x00000000);

	iris_i2c_reg_write(0xf0000000, 0x3fc87fff);
	iris_i2c_reg_write(0xf0000004, 0x00007f8e);
#endif
}

u32 timeus0, timeus1, timeus2;
void iris_pre_lightup(struct dsi_panel *panel)
{
	ktime_t ktime0, ktime1;
	int ret = 0;

	record_first_backlight = false;
	record_first_kickoff = false;
	iris_hw_status_check_enabled = false;
	panel_hbm_on = false;
	iris_full_functioning = 0;/*reset flags*/
	iris_in_cont_splash_displaying = 0;
	iris_info.abypass_ctrl.abypass_status = PASS_THROUGH_MODE;
	iris_info.abypass_ctrl.abypass_to_pt_enable = 0;
	iris_info.abypass_ctrl.pt_to_abypass_enable = 0;

	pr_err("iris_pre_lightup start\n");
	ktime0 = ktime_get();
	if (irisStayInLowPowerAbypass()) {
		iris_feature_setting_init(&iris_info.user_setting);
		iris_feature_setting_init(&iris_info.chip_setting);
	}
#ifdef EFUSE_WRITE
	pr_info("iris_pre_lightup: efuse writing\n");
	iris_mipirx_mode_set(panel, PWIL_CMD, DSI_CMD_SET_STATE_LP);
	iris_efuse_write(panel);
	usleep_range(10000, 10010);
	pr_info("iris_pre_lightup: efuse written\n");
#endif
	ret = iris_Romcode_state_check(panel, 0x01, 3, true);
	if (ret < 0 || irisIsSimulatedResetFailureRecover())
		iris_reset_failure_recover(panel);

	if (irisIsInAbypassMode()) {
		return iris_bp_lightup(panel);
	}

	iris_mipirx_mode_set(panel, PWIL_CMD, DSI_CMD_SET_STATE_LP);
	/* init sys/mipirx/mipitx */
	iris_init_cmd_send(panel, DSI_CMD_SET_STATE_LP);
	if (isAllowedLightupTrace())
		pr_err("iris_pre_lightup: iris_init_cmd_sent\n");
	if (DSI_OP_VIDEO_MODE == panel->panel_mode) {
		msleep(100);
		iris_power_down();
		return;
	}
	/* send work mode and timing info */
	iris_mipirx_mode_set(panel, MCU_CMD, DSI_CMD_SET_STATE_LP);
	iris_timing_info_send(panel, DSI_CMD_SET_STATE_LP);
	if (isAllowedLightupTrace())
		pr_err("iris_pre_lightup: iris_timing_info sent\n");

	/*configure data path*/
	iris_mipirx_mode_set(panel, PWIL_CMD, DSI_CMD_SET_STATE_LP);
	iris_ctrl_cmd_send(panel, CONFIG_DATAPATH, DSI_CMD_SET_STATE_LP);
	if (isAllowedLightupTrace())
		pr_err("iris_pre_lightup: iris_ctrl_cmd sent\n");
	/*init grcp buffer*/
	iris_grcp_buffer_init(panel, DSI_CMD_SET_STATE_HS);
	if (isAllowedLightupTrace())
		pr_err("iris_pre_lightup: iris_grcp_buffer_init sent\n");
	/*init feature*/
	iris_feature_init_cmd_send(panel, DSI_CMD_SET_STATE_HS);
	if (isAllowedLightupTrace())
		pr_err("iris_pre_lightup: iris_feature_init_cmd sent\n");

	/* bypass panel on command */
	iris_mipirx_mode_set(panel, BYPASS_CMD, DSI_CMD_SET_STATE_LP);
	if (isAllowedLightupTrace())
		pr_err("iris_pre_lightup: iris BYPASS_CMD sent\n");

	if (iris_info.work_mode.tx_mode == DSI_OP_CMD_MODE) {
		u32 column, page;

		column = iris_info.work_mode.rx_ch ?
				(iris_info.input_timing.hres * 2 - 1) : (iris_info.input_timing.hres - 1);
		page = iris_info.input_timing.vres - 1;
		iris_mipi_mem_addr_cmd_send(panel, column, page, DSI_CMD_SET_STATE_LP);
	}

	/* wait panel reset ready */
	ktime1 = ktime_get();
	timeus0 = (u32) ktime_to_us(ktime1) - (u32)ktime_to_us(ktime0);
	if (timeus0 < 10000)
		usleep_range((10000 - timeus0), (10010 - timeus0));

	usleep_range(10000, 10010);

	pr_err("iris_pre_lightup end\n");
}

void iris_misc_cmd_send(struct dsi_panel *panel, enum dsi_cmd_set_state state, int capt_en)
{
	u32 grcp_len = 0;
	struct dsi_cmd_desc iris_misc_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif

	mutex_lock(&iris_info.dsi_send_mutex);
	memset(&grcp_cmd, 0, sizeof(grcp_cmd));
	memcpy(grcp_cmd.cmd, grcp_header, GRCP_HEADER);
	grcp_cmd.cmd_len = GRCP_HEADER;

	if (capt_en > 0) {
		iris_cmd_reg_add(&grcp_cmd, 0xf1240218, 0xc0000003);
		iris_cmd_reg_add(&grcp_cmd, 0xf1250000, 0x100);
	} else {
		iris_cmd_reg_add(&grcp_cmd, 0xf1240218, 0xc0000001);
		iris_cmd_reg_add(&grcp_cmd, 0xf1250000, 0x100);
	}

	grcp_len = (grcp_cmd.cmd_len - GRCP_HEADER) / 4;
	*(u32 *)(grcp_cmd.cmd + 8) = cpu_to_le32(grcp_len + 1);
	*(u16 *)(grcp_cmd.cmd + 14) = cpu_to_le16(grcp_len);

	iris_misc_cmd.msg.tx_len = grcp_cmd.cmd_len;
	iris_dsi_cmds_send(panel, &iris_misc_cmd, 1, state);
	mutex_unlock(&iris_info.dsi_send_mutex);
	pr_err("iris: misc cmd sent\n");
}

void iris_lightup(struct dsi_panel *panel)
{
	u32 column, page;
	ktime_t ktime0, ktime1, ktime2, ktime3;
	int ret = 0;

	if (DSI_OP_VIDEO_MODE == panel->panel_mode) {
		return;
	}
	pr_err("iris_lightup started\n");

	iris_info.context_info.backlight_on = 0;
	iris_info.context_info.iris_stay_in_abypass_en = irisStayInLowPowerAbypass() ? 1 : 0;

	if (irisIsInAbypassMode()) {
		if (irisIsInAbypassHbmEn()) {
			int enableHbm = 1;

			iris_hbm_direct_switch(panel, enableHbm);
		}
		iris_mode_switch_state_reset();
		iris_info.power_status.low_power_state = LP_STATE_POWER_UP;

		os_bootup_completed = 1;
		return;
	}
#ifdef SAMSUNG_PANEL_TE_ADJUST_ALTERNATIVE
	//to be removed
#else
	panel_working_c0 = 0;
	panel_expected_c0 = 0;
	irisResetTeVariables();
#endif
	ktime0 = ktime_get();
	iris_mipirx_mode_set(panel, PWIL_CMD, DSI_CMD_SET_STATE_HS);

	if (iris_debug_fw_download_disable) {
		iris_info.firmware_info.app_fw_state = false;
		iris_info.firmware_info.app_cnt = 0;
	} else {
		iris_lut_firmware_init(LUT_FIRMWARE_NAME);
		iris_gamma_firmware_init(GAMMA_FIRMWARE_NAME);
		/* wait Romcode ready */
		ret = iris_Romcode_state_check(panel, 0x03, 5, true);
		if (ret < 0 || irisIsSimulatedInitFailureRecover())
			iris_init_failure_recover(panel);
		/* firmware download */
		iris_info.firmware_info.app_fw_state =
				iris_firmware_download(panel, APP_FIRMWARE_NAME, false);
		if (true == iris_info.firmware_info.app_fw_state) {
			iris_ctrl_cmd_send(panel, (REMAP + ITCM_COPY), DSI_CMD_SET_STATE_HS);
			iris_info.firmware_info.app_cnt = 0;
		} else {
			/*firmware NOT loaded*/
			//iris_debug_fw_download_disable = 1;
		}
	}

	/* wait panel 0x11 cmd ready */
	ktime1 = ktime_get();
	timeus1 = (u32) ktime_to_us(ktime1) - (u32)ktime_to_us(ktime0);
	if (timeus1 < 120000)
		usleep_range((120000 - timeus1), (120010 - timeus1));


	ktime2 = ktime_get();
	if (DSI_OP_VIDEO_MODE == iris_info.work_mode.rx_mode) {
		iris_mipirx_mode_set(panel, PWIL_VIDEO, DSI_CMD_SET_STATE_HS);
		iris_dtg_setting_cmd_send(panel, DSI_CMD_SET_STATE_HS);
		iris_pwil_mode_set(panel, RFB_MODE, DSI_CMD_SET_STATE_HS);
	} else {
		iris_dtg_setting_cmd_send(panel, DSI_CMD_SET_STATE_HS);
		iris_pwil_mode_set(panel, RFB_MODE, DSI_CMD_SET_STATE_HS);
		/* update rx column/page addr */
		column = iris_info.work_mode.rx_ch ? (iris_info.input_timing.hres * 2 - 1) : (iris_info.input_timing.hres - 1);
		page = iris_info.input_timing.vres - 1;
		iris_mipi_mem_addr_cmd_send(panel, column, page, DSI_CMD_SET_STATE_HS);
	}
	/*wait appcode init done*/
	if (!iris_debug_fw_download_disable
		&& iris_info.firmware_info.app_fw_state) {
		iris_Romcode_state_check(panel, 0x9b, 200, true);
		iris_full_functioning = 1;
		iris_hw_status_check_enabled = true;
		iris_hw_status_check_started = true;
	}

	iris_mode_switch_state_reset();
	iris_info.power_status.low_power_state = LP_STATE_POWER_UP;

	os_bootup_completed = 1;

	/* wait panel 0x29 cmd ready */
	ktime3 = ktime_get();
	timeus2 = (u32) ktime_to_us(ktime3) - (u32)ktime_to_us(ktime2);
	if (timeus2 < 20000)
		usleep_range((20000 - timeus2), (20010 - timeus2));

	pr_err("iris light up time: %d %d %d\n", timeus0, timeus1, timeus2);

	if (irisStayInLowPowerAbypass()) {
		pr_info("iris lightup into low power abypass\n");
		iris_request_kickoff_count(300);
		irisLowPowerAbypassSwitch(panel, IRIS_IN_ONE_WIRED_ABYPASS);
	}
}

void iris_lightdown(struct dsi_panel *panel)
{
	pr_info("iris lightdown reset status\n");

	iris_hw_status_check_enabled = false;
	iris_info.context_info.backlight_on = 0;
	iris_info.abypass_ctrl.abypass_status = PASS_THROUGH_MODE;
	iris_info.abypass_ctrl.abypass_to_pt_enable = 0;
	iris_info.abypass_ctrl.pt_to_abypass_enable = 0;
	iris_full_functioning = 0;
	panel_hbm_on = false;
#ifdef FIXME_IRIS
	iris_dport_enable_cmd_send(panel, false);
	iris_mipirx_mode_set(panel, BYPASS_CMD, DSI_CMD_SET_STATE_HS);
#endif
}

u8 irisReadPanelTeConfig_Samsung(struct dsi_panel *panel, enum dsi_cmd_set_state state)
{
	static char iris_response_c0[16];
	char start_cmd[3] = {0xf0, 0x5a, 0x5a};
	char end_cmd[3] = {0xf0, 0xa5, 0xa5};
	char get_c0_value[1] = {0xc0};
	struct dsi_cmd_desc cmds[] = {
#ifdef NEW_MIPI_DSI_MSG
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(start_cmd), start_cmd, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_READ, BIT(6), 0, 0, sizeof(get_c0_value), get_c0_value, 1, iris_response_c0}, 1, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(end_cmd), end_cmd, 1, iris_read_cmd_buf}, 1, 0},
#else
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, sizeof(start_cmd), start_cmd, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_READ, BIT(6), 0, sizeof(get_c0_value), get_c0_value, 1, iris_response_c0}, 1, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, sizeof(end_cmd), end_cmd, 1, iris_read_cmd_buf}, 1, 0},
#endif
	};
	int rc = 0;
	u8 data = 0xff;

	if (irisIsInAbypassMode() || irisIsInLowPowerAbypassMode()) {
		mutex_lock(&iris_info.dsi_send_mutex);
		memset(iris_read_cmd_buf, 0, sizeof(iris_read_cmd_buf));
		memset(iris_response_c0, 0, sizeof(iris_response_c0));
		rc = iris_dsi_cmds_send(panel, cmds, ARRAY_SIZE(cmds), state);
		mutex_unlock(&iris_info.dsi_send_mutex);
		data = iris_response_c0[0];
	} else {
		u32 reg_value = 0;

		rc = iris_handover_panel_dcs_cmd(panel, &cmds[0], state);
		rc = iris_handover_panel_dcs_cmd(panel, &cmds[1], state);
		iris_mipi_reg_read(IRIS_MIPI_TX_ADDR + RD_PACKET_DATA_OFFS, &reg_value);
		rc = iris_handover_panel_dcs_cmd(panel, &cmds[2], state);
		pr_info("iris: reading samsung panel te registers c0: 0x%x, 0x%x\n", reg_value, (reg_value>>8)&0xff);
		data = (reg_value>>8)&0xff;
	}
	pr_info("iris: samsung panel c0 value: 0x%x\n", data);

	return data;
}

u8 mipi_panel_reg_dcs_read(struct dsi_panel *panel, enum dsi_cmd_set_state state, char addr)
{
	char get_data[1] = {0x0a};
	const struct mipi_dsi_host_ops *ops = panel->host->ops;
	struct dsi_cmd_desc cmds = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_DCS_READ, 0, 0, 0, 1, (const void *)get_data, 1, (void *)iris_read_cmd_buf}, 1, 0};
#else
		{0, MIPI_DSI_DCS_READ, 0, 0, 1, (const void *)get_data, 1, (void *)iris_read_cmd_buf}, 1, 0};
#endif
	int rc = 0;
	u8 data = 0xff;

	get_data[0] = addr;
	memset(iris_read_cmd_buf, 0, sizeof(iris_read_cmd_buf));
	if (state == DSI_CMD_SET_STATE_LP)
		cmds.msg.flags |= MIPI_DSI_MSG_USE_LPM;
	if (cmds.last_command)
		cmds.msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;
	cmds.msg.flags |= BIT(6);

	rc = ops->transfer(panel->host, &cmds.msg);
	if (rc < 0) {
		pr_err("mipi_panel_reg_dcs_read failed to set cmds(%d), rc=%d, read=%d\n",
			cmds.msg.type,
			rc,
			iris_read_cmd_buf[0]);
		return data;
	}
	data = iris_read_cmd_buf[0];
	if (isAllowedLightupTrace())
		pr_info("mipi_panel_reg_dcs_read: addr 0x%x, value 0x%x\n", addr, data);

	return data;
}

u8 irisReadPanelTeConfig_Kingdisplay(struct dsi_panel *panel, enum dsi_cmd_set_state state)
{
	static char iris_response_a[16];
	static char iris_response_b[16];
	char start_cmd[2] = {0xfe, 0x40};
	int size = 1;
	u8 set_size[2] = {(u8)(size&0xff), (u8)(size>>8)};
	char get_0a_cmd[1] = {0x0a};
	char get_0b_cmd[1] = {0x0b};
	char exit_cmd[2] = {0xfe, 0x00};
	struct dsi_cmd_desc kd_te_default_cmd[] = {
#ifdef NEW_MIPI_DSI_MSG
		{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, 0, sizeof(start_cmd), start_cmd, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, 0, 0, 0, sizeof(set_size), set_size, 1, iris_read_cmd_buf}, 1, 0},
		{{0, MIPI_DSI_DCS_READ, BIT(6), 0, 0, sizeof(get_0a_cmd), get_0a_cmd, 1, iris_response_a}, 1, 0},
		{{0, MIPI_DSI_DCS_READ, BIT(6), 0, 0, sizeof(get_0b_cmd), get_0b_cmd, 1, iris_response_b}, 1, 0},
		{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, 0, sizeof(exit_cmd), exit_cmd, 1, iris_read_cmd_buf}, 0, 0},
#else
                {{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(start_cmd), start_cmd, 1, iris_read_cmd_buf}, 0, 0},
                {{0, MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, 0, 0, sizeof(set_size), set_size, 1, iris_read_cmd_buf}, 1, 0},
                {{0, MIPI_DSI_DCS_READ, BIT(6), 0, sizeof(get_0a_cmd), get_0a_cmd, 1, iris_response_a}, 1, 0},
                {{0, MIPI_DSI_DCS_READ, BIT(6), 0, sizeof(get_0b_cmd), get_0b_cmd, 1, iris_response_b}, 1, 0},
                {{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(exit_cmd), exit_cmd, 1, iris_read_cmd_buf}, 0, 0},
#endif
	};
	int rc = 0;
	u8 data_0a = 0xff;
	u8 data_0b = 0xff;

	if (irisIsInAbypassMode() || irisIsInLowPowerAbypassMode()) {
		mutex_lock(&iris_info.dsi_send_mutex);
		memset(iris_read_cmd_buf, 0, sizeof(iris_read_cmd_buf));
		memset(iris_response_a, 0, sizeof(iris_response_a));
		memset(iris_response_b, 0, sizeof(iris_response_b));

		rc = iris_dsi_cmds_send(panel, kd_te_default_cmd, ARRAY_SIZE(kd_te_default_cmd), state);
		mutex_unlock(&iris_info.dsi_send_mutex);

		data_0a = iris_response_a[0];
		data_0b = iris_response_b[0];
		pr_info("iris: reading kingdisplay panel 0a: 0x%x and 0b: 0x%x, rc=%d\n",
				data_0a, data_0b, rc);
		data_0a = iris_mipi_power_mode_read(panel, DSI_CMD_SET_STATE_LP);
		pr_info("iris: reading kingdisplay panel power mode 0a: 0x%x\n", data_0a);

	} else {
		u32 reg_value = 0;

		rc = iris_handover_panel_dcs_cmd(panel, &kd_te_default_cmd[0], state);
		rc = iris_handover_panel_dcs_cmd(panel, &kd_te_default_cmd[1], state);
		rc = iris_handover_panel_dcs_cmd(panel, &kd_te_default_cmd[2], state);
		iris_mipi_reg_read(IRIS_MIPI_TX_ADDR + RD_PACKET_DATA_OFFS, &reg_value);
		pr_info("iris: reading kingdisplay panel te registers 0A: 0x%x, 0x%x\n", reg_value, (reg_value>>8)&0xff);
		data_0a = (reg_value>>8)&0xff;
		rc = iris_handover_panel_dcs_cmd(panel, &kd_te_default_cmd[3], state);
		iris_mipi_reg_read(IRIS_MIPI_TX_ADDR + RD_PACKET_DATA_OFFS, &reg_value);
		pr_info("iris: reading kingdisplay panel te registers 0B: 0x%x, 0x%x\n", reg_value, (reg_value>>8)&0xff);
		data_0b = (reg_value>>8)&0xff;
		rc = iris_handover_panel_dcs_cmd(panel, &kd_te_default_cmd[4], state);
	}
	return 0;
}

u8 irisReadPanelTeConfig(struct dsi_panel *panel, enum dsi_cmd_set_state state)
{
	if (connected_lcd_module == LCM_KINGDISPLAY) {
		return irisReadPanelTeConfig_Kingdisplay(panel, state);
	} else {
		return irisReadPanelTeConfig_Samsung(panel, state);
	}
}

void irisAdjustPanelTE_Samsung(struct dsi_panel *panel, int bypasscmd, u32 value, enum dsi_cmd_set_state state)
{
	char start_cmd[3] = {0xf0, 0x5a, 0x5a};
	char start_cmd_2[3] = {0xfc, 0x5a, 0x5a};
	char b0_cmd[2] = {0xb0, 0x14};
	char d1_cmd[2] = {0xd1, 0x1b};
	char end_cmd_2[3] = {0xf0, 0xa5, 0xa5};
	char end_cmd[3] = {0xfc, 0xa5, 0xa5};
	struct dsi_cmd_desc samsung_te_tweak_cmd[] = {
#ifdef NEW_MIPI_DSI_MSG
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(start_cmd), start_cmd, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(start_cmd_2), start_cmd_2, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, 0, sizeof(b0_cmd), b0_cmd, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, 0, sizeof(d1_cmd), d1_cmd, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(end_cmd_2), end_cmd_2, 1, iris_read_cmd_buf}, 1, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(end_cmd), end_cmd, 1, iris_read_cmd_buf}, 1, 0},
#else
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, sizeof(start_cmd), start_cmd, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, sizeof(start_cmd_2), start_cmd_2, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(b0_cmd), b0_cmd, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(d1_cmd), d1_cmd, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, sizeof(end_cmd_2), end_cmd_2, 1, iris_read_cmd_buf}, 1, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, sizeof(end_cmd), end_cmd, 1, iris_read_cmd_buf}, 1, 0},
#endif
	};
	d1_cmd[1] = value&0xff;

	pr_info("iris: tweak samsung panel(T3) te: 0x%x, d1: 0x%x\n", value, d1_cmd[1]);
	if (value < SAMSUNG_PANEL_TE_D1_MIN) {
		pr_err("iris: tweak samsung panel(T3) te: invalid d1 value\n");
		return;
	}

	if ((value&0xff00) > 0) {
		pr_err("iris: reading samsung panel te configuration ...\n");
		irisReadPanelTeConfig(panel, state);
		return;
	}

	panel_working_c0 = d1_cmd[1];
	panel_expected_c0 = d1_cmd[1];

	if (irisIsInAbypassMode() || bypasscmd > 0) {
		mutex_lock(&iris_info.dsi_send_mutex);
		iris_dsi_cmds_send(panel, samsung_te_tweak_cmd, ARRAY_SIZE(samsung_te_tweak_cmd), state);
		mutex_unlock(&iris_info.dsi_send_mutex);
	} else {
		iris_handover_panel_dcs_cmd(panel, &samsung_te_tweak_cmd[0], state);
		iris_handover_panel_dcs_cmd(panel, &samsung_te_tweak_cmd[1], state);
		iris_handover_panel_dcs_cmd(panel, &samsung_te_tweak_cmd[2], state);
		iris_handover_panel_dcs_cmd(panel, &samsung_te_tweak_cmd[3], state);
		iris_handover_panel_dcs_cmd(panel, &samsung_te_tweak_cmd[4], state);
		iris_handover_panel_dcs_cmd(panel, &samsung_te_tweak_cmd[5], state);
	}
}

void irisAdjustPanelTE_KingDisplay(struct dsi_panel *panel, int bypasscmd, u32 value, enum dsi_cmd_set_state state)
{
	char start_cmd[2] = {0xfe, 0x40};
	char a0_cmd[2] = {0x0a, 0xf0};
	char b0_cmd[2] = {0x0b, 0x8c};
	char exit_cmd[2] = {0xfe, 0x00};
	struct dsi_cmd_desc te_tweak_cmd[] = {
#ifdef NEW_MIPI_DSI_MSG
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(start_cmd), start_cmd, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(a0_cmd), a0_cmd, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(b0_cmd), b0_cmd, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, 0, sizeof(exit_cmd), exit_cmd, 1, iris_read_cmd_buf}, 0, 0},
#else
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, sizeof(start_cmd), start_cmd, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, sizeof(a0_cmd), a0_cmd, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, sizeof(b0_cmd), b0_cmd, 1, iris_read_cmd_buf}, 0, 0},
		{{0, MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(exit_cmd), exit_cmd, 1, iris_read_cmd_buf}, 0, 0},
#endif
	};
	a0_cmd[1] = value&0xff;
	b0_cmd[1] = (value>>8)&0xff;

	pr_info("iris: tweak kingdisplay panel te: 0x%x, a0: 0x%x, b0: 0x%x\n",
			value, a0_cmd[1], b0_cmd[1]);
	if (value < 2) {
		pr_err("iris: tweak kingdisplay panel te: invalid a0 value\n");
		return;
	}

	panel_working_c0 = value;
	panel_expected_c0 = value;

	if (irisIsInAbypassMode() || bypasscmd > 0) {
		mutex_lock(&iris_info.dsi_send_mutex);
		iris_dsi_cmds_send(panel, te_tweak_cmd, ARRAY_SIZE(te_tweak_cmd), state);
		mutex_unlock(&iris_info.dsi_send_mutex);
	} else {
		iris_handover_panel_dcs_cmd(panel, &te_tweak_cmd[0], state);
		iris_handover_panel_dcs_cmd(panel, &te_tweak_cmd[1], state);
		iris_handover_panel_dcs_cmd(panel, &te_tweak_cmd[2], state);
		iris_handover_panel_dcs_cmd(panel, &te_tweak_cmd[3], state);
	}
}

void irisResetTeRanking(void);

void irisAdjustPanelTE(struct dsi_panel *panel, int bypasscmd, u32 value, enum dsi_cmd_set_state state)
{
	irisResetTeRanking();
	if (connected_lcd_module == LCM_KINGDISPLAY)
		irisAdjustPanelTE_KingDisplay(panel, bypasscmd, value, state);
	else
		irisAdjustPanelTE_Samsung(panel, bypasscmd, value, state);
}

void irisResetPanelTeRegs(void)
{
	panel_expected_c0 = 0;
	panel_working_c0 = 0;
}

#define TE_DELTA_N    (120)
static int currentTeNum = 0;
static int TeN0 = 0;
static int TeN1 = 0;
static int IDEAL_TE_PERIOD_US = 16667; /*us*/
static int realAveragePeriodUs = 0;
static int instantPeriodUs = 0;
static int deltaTimeErr = 0;
static u32 deltaTimeUs = 0;
static ktime_t te_t0, te_t1;
static u32 frameCount = 0;
static int ALLOWED_MAX_DEVIATION = 167; /*us 1%*/
static int ALLOWED_NICE_DEVIATION = 51; /*us 0.3%*/
static int ALLOWED_PERFECT_DEVIATION = 17; /*us 0.1%*/
static int rank_bad = 0;
static int rank_max = 0;
static int rank_nice = 0;
static int rank_perfect = 0;
static int timeErr_avg_rankbad = 0;
static int timeErr_avg_rankmax = 0;
static int timeErr_avg_ranknice = 0;
static int timeErr_avg_rankperfect = 0;
static int timeErr_total_rankbad = 0;
static int timeErr_total_rankmax = 0;
static int timeErr_total_ranknice = 0;
static int timeErr_total_rankperfect = 0;

#define TE_BAD             (0)
#define TE_WITHIN_SPEC_MAX (1)
#define TE_NICE            (2)
#define TE_PERFECT         (4)

static int te_quality = 0;

#define TE_PERIOD_TOO_SHORT     (0x10)
#define TE_PERIOD_NORMAL        (0x20)
#define TE_PERIOD_TOO_LONG      (0x40)

static int te_period_status = 0;
static int rank_highest = 0;
static int calibrationExpired = 0;
#define CALIBRATION_EXPIRED_MAX   (5)

static int rank_te_period_tooshort = 0;
static int rank_te_period_toolong = 0;
static int rank_te_period_normal = 0;

static int actionRecoverTE = 0;
static int panel_trained_c0 = 0;
static int te_training_started = 0;

void irisResetTeRanking(void)
{
	rank_bad = 0;
	rank_max = 0;
	rank_nice = 0;
	rank_perfect = 0;
	rank_highest = 0;
	te_quality = -1;
	te_period_status = -1;
	rank_te_period_tooshort = 0;
	rank_te_period_toolong = 0;
	rank_te_period_normal = 0;
	calibrationExpired = 0;
	timeErr_avg_rankbad = 0;
	timeErr_avg_rankmax = 0;
	timeErr_avg_ranknice = 0;
	timeErr_avg_rankperfect = 0;
	timeErr_total_rankbad = 0;
	timeErr_total_rankmax = 0;
	timeErr_total_ranknice = 0;
	timeErr_total_rankperfect = 0;
}

void irisResetTeVariables(void)
{
	TeN0 = 0;
	TeN1 = 0;
	realAveragePeriodUs = 0;
	instantPeriodUs = 0;
	irisResetTeRanking();
	te_training_started = 0;
}

void irisReportTeCount(int vsync_count)
{
	if (frameCount >= 0xFFFF)
		frameCount = 0;
	frameCount++;

	currentTeNum = vsync_count;
	if (TeN0 == 0) {
		TeN0 = vsync_count;
		te_t0 = ktime_get();
	} else if ((currentTeNum - TeN0) > TE_DELTA_N) {
		TeN1 = vsync_count;
		te_t1 = ktime_get();
		deltaTimeUs = (u32)ktime_to_us(te_t1) - (u32)ktime_to_us(te_t0);
		realAveragePeriodUs = (int)(deltaTimeUs/(TeN1 - TeN0));
		deltaTimeErr = IDEAL_TE_PERIOD_US - realAveragePeriodUs;
		if (deltaTimeErr < -ALLOWED_MAX_DEVIATION) {
			rank_bad++;
			rank_te_period_toolong++;
			timeErr_total_rankbad += deltaTimeErr;
			timeErr_avg_rankbad = timeErr_total_rankbad/rank_bad;
		} else if (deltaTimeErr < -ALLOWED_NICE_DEVIATION) {
			rank_max++;
			rank_te_period_toolong++;
			timeErr_total_rankmax += deltaTimeErr;
			timeErr_avg_rankmax = timeErr_total_rankmax/rank_max;
		} else if (deltaTimeErr < -ALLOWED_PERFECT_DEVIATION) {
			rank_nice++;
			rank_te_period_normal++;
			timeErr_total_ranknice += deltaTimeErr;
			timeErr_avg_ranknice = timeErr_total_ranknice/rank_nice;
		} else if (deltaTimeErr < ALLOWED_PERFECT_DEVIATION) {
			rank_perfect++;
			rank_te_period_normal++;
			timeErr_total_rankperfect += deltaTimeErr;
			timeErr_avg_rankperfect = timeErr_total_rankperfect/rank_perfect;
		} else if (deltaTimeErr < ALLOWED_NICE_DEVIATION) {
			rank_nice++;
			rank_te_period_normal++;
			timeErr_total_ranknice += deltaTimeErr;
			timeErr_avg_ranknice = timeErr_total_ranknice/rank_nice;
		} else if (deltaTimeErr < ALLOWED_MAX_DEVIATION) {
			rank_max++;
			rank_te_period_tooshort++;
			timeErr_total_rankmax += deltaTimeErr;
			timeErr_avg_rankmax = timeErr_total_rankmax/rank_max;
		} else {
			rank_bad++;
			rank_te_period_tooshort++;
			timeErr_total_rankbad += deltaTimeErr;
			timeErr_avg_rankbad = timeErr_total_rankbad/rank_bad;
		}

		if (rank_highest < rank_bad)
			rank_highest = rank_bad;
		if (rank_highest < rank_max)
			rank_highest = rank_max;
		if (rank_highest < rank_nice)
			rank_highest = rank_nice;
		if (rank_highest < rank_perfect)
			rank_highest = rank_perfect;

		if (rank_highest == rank_bad)
			te_quality = TE_BAD;
		if (rank_highest == rank_max)
			te_quality = TE_WITHIN_SPEC_MAX;
		if (rank_highest == rank_nice)
			te_quality = TE_NICE;
		if (rank_highest == rank_perfect)
			te_quality = TE_PERFECT;

		te_period_status = TE_PERIOD_NORMAL;
		if (rank_te_period_normal > rank_te_period_tooshort
			&& rank_te_period_normal > rank_te_period_toolong) {
			te_period_status = TE_PERIOD_NORMAL;
		} else if (rank_te_period_tooshort > rank_te_period_toolong) {
			te_period_status = TE_PERIOD_TOO_SHORT;
		} else {
			te_period_status = TE_PERIOD_TOO_LONG;
		}

		if (deltaTimeErr == IDEAL_TE_PERIOD_US)
			actionRecoverTE ++;
		else
			actionRecoverTE = 0;

		TeN0 = 0;/*calculate again*/
		if (irisIsAllowedMemcTrace())
			pr_err("iris: TE avg: %dus, err:%dus, [r%d %d %d %d(%d)][totalerr:%d %d %d %d][avgerr:%d %d %d %d][p%d %d %d (%d)]\n",
					realAveragePeriodUs,
					deltaTimeErr,
					rank_bad, rank_max, rank_nice, rank_perfect,
					te_quality,
					timeErr_total_rankbad, timeErr_total_rankmax, timeErr_total_ranknice, timeErr_total_rankperfect,
					timeErr_avg_rankbad, timeErr_avg_rankmax, timeErr_avg_ranknice, timeErr_avg_rankperfect,
					rank_te_period_tooshort, rank_te_period_normal, rank_te_period_toolong,
					te_period_status);
	} else if (panelTeCalibrationRequired > 0) {
		TeN1 = vsync_count;
		te_t1 = ktime_get();
		deltaTimeUs = (u32)ktime_to_us(te_t1) - (u32)ktime_to_us(te_t0);
		instantPeriodUs = (int)(deltaTimeUs/(TeN1 - TeN0));
		deltaTimeErr = IDEAL_TE_PERIOD_US - instantPeriodUs;
		if (deltaTimeErr == IDEAL_TE_PERIOD_US)
			actionRecoverTE++;
		else
			actionRecoverTE = 0;

		calibrationExpired++;
		TeN0 = 0;/*calculate again*/
		if (irisIsAllowedMemcTrace())
			pr_err("iris: TE rt: %dus, err:%dus\n", instantPeriodUs, deltaTimeErr);
		if (deltaTimeErr >= 0) {
			if (deltaTimeErr < ALLOWED_MAX_DEVIATION)
				panelTeCalibrationRequired = 0;
		} else {
			if (deltaTimeErr > -ALLOWED_MAX_DEVIATION)
				panelTeCalibrationRequired = 0;
		}
		if (calibrationExpired >= CALIBRATION_EXPIRED_MAX) {
			panelTeCalibrationRequired = 0;
			pr_err("iris: TE rt: calibration expired: %dus\n", deltaTimeErr);
		}
	}

	if (irisIsAllowedBasicTrace())
		pr_err("iris: TE %d(fc=%d)\n", currentTeNum, (int)frameCount);
}

void validatePanelExpectedC0(void)
{
	if (connected_lcd_module == LCM_KINGDISPLAY) {
		/*0x8800~0x88ff*/
		if (panel_expected_c0 < KINGDISPLAY_PANEL_TE_BA_MIN
			|| panel_expected_c0 >KINGDISPLAY_PANEL_TE_BA_MAX)
			panel_expected_c0 = KINGDISPLAY_PANEL_TE_INIT_OTP_VALUE;
	} else {
		/*d1: 0x11~0x17*/
		if (panel_expected_c0 < SAMSUNG_PANEL_TE_D1_MIN
			|| panel_expected_c0 > SAMSUNG_PANEL_TE_D1_MAX)
			panel_expected_c0 = SAMSUNG_PANEL_TE_INIT_OTP_VALUE;
	}
}

u32 timeusCost = -1;
ktime_t ktime_te_start;
ktime_t ktime_te_end;

#define RANK_VALID_THRESHOLD (5)

void irisMonitorPanelTE(struct dsi_panel *panel)
{
	struct iris_mode_state *mode_state = &iris_info.mode_state;
	bool teIsGoodEnough = false;

	if (iris_panel_te_tweak_enabled <= 0
		|| !irisIsFullyFunctioning()
		|| os_bootup_completed <= 0) {
		//pr_info("iris: TE system not ready for TE tweaking.\n");
		return;
	}

	if (irisIsInAbypassMode()
		|| irisIsInLowPowerAbypassMode()) {
		//pr_info("iris: TE system not ready for TE tweaking.\n");
		return;
	}

	if ((rank_bad + rank_max + rank_nice + rank_perfect) < RANK_VALID_THRESHOLD
		|| (rank_max == rank_nice && te_quality == TE_NICE)) {
		//pr_info("iris: TE system for more info.\n");
		return;
	}
	if (connected_lcd_module == LCM_SAMSUNG) {
		if (te_quality >= TE_NICE)
			teIsGoodEnough = true;
	} else {
		//LCM_KINGDISPLAY
		if (te_quality >= TE_PERFECT)
			teIsGoodEnough = true;
	}
	if (teIsGoodEnough) {
		if (panel_expected_c0 != 0)
			panel_trained_c0 = panel_expected_c0;
		if (te_training_started > 0) {
			ktime_te_end = ktime_get();
			timeusCost = (u32)ktime_to_us(ktime_te_end) - (u32)ktime_to_us(ktime_te_start);
			if (irisIsAllowedMemcTrace())
				pr_info("iris: TE tweak cost %dus (%dms)", timeusCost, timeusCost/1000);
		}
		te_training_started = 0;/*completed*/
		if (irisIsAllowedMemcTrace())
			pr_info("iris: TE system TE quality nice(0x%x),period status %d,time-err: %d, no need TE tweak.\n",
					panel_expected_c0, te_period_status, deltaTimeErr);
		return;
	}

	if (irisIsAllowedMemcTrace())
		pr_info("iris: TE quality level %d,period status %d,time-err: %d, need tweak for memc.\n",
					te_quality, te_period_status, deltaTimeErr);
#ifdef SAMSUNG_PANEL_TE_ADJUST_ALTERNATIVE
//ignored
#endif
	if (actionRecoverTE > 3) {
		/*must adjust TE to low to recover the system*/
		if (irisIsAllowedMemcTrace())
			pr_info("iris: TE system Vsync counter stop working!");
	}
	if (panelTeCalibrationRequired > 0) {
		if (irisIsAllowedMemcTrace())
			pr_info("iris: TE calibration is ongoing!");
		return;
	}
	if ((te_training_started == 0)
		&& mode_state->sf_notify_mode != IRIS_MODE_FRC) {
		if (irisIsAllowedMemcTrace())
			pr_info("iris: TE system not in FRC mode: %d!\n", mode_state->sf_notify_mode);
		return;
	}
	if (irisIsAllowedMemcTrace())
		pr_info("iris: TE system in FRC: %d or training is ongoing!\n", mode_state->sf_notify_mode);
#ifdef USE_TRAINED_C0
	if (panel_trained_c0 > 0 && panel_expected_c0 == 0) {
		panel_expected_c0 = panel_trained_c0;
		pr_info("iris:TE set expected c0 to the trained c0: 0x%x\n", panel_trained_c0);
	}
#endif
	if (panel_working_c0 == panel_expected_c0) {
		if (connected_lcd_module == LCM_KINGDISPLAY) {
			//ONE STEP IS ABOUT 0.06~0.08HZ, OR 17US/STEP
			int adjustTimeErr = 0;
			int step = 0;

			if (te_quality == TE_BAD)
				adjustTimeErr = timeErr_avg_rankbad;
			else if (te_quality == TE_WITHIN_SPEC_MAX)
				adjustTimeErr = timeErr_avg_rankmax;
			else if (te_quality == TE_NICE)
				adjustTimeErr = timeErr_avg_ranknice;

			step = (adjustTimeErr >= 0) ? adjustTimeErr : -adjustTimeErr;
			if (step > ALLOWED_PERFECT_DEVIATION)
				step = step/KINGDISPLAY_PANEL_TE_ONE_STEP_US;
			else
				step = 0;

			if (irisIsAllowedMemcTrace())
				pr_info("iris: TE kingdisplay: working:0x%x, deltaTimeErr=%d,adjustTimeErr=%d,step=%d.\n",
						panel_working_c0, deltaTimeErr, adjustTimeErr, step);
			if (panel_expected_c0 == 0)
				panel_expected_c0 = KINGDISPLAY_PANEL_TE_INIT_OTP_VALUE;
			if (adjustTimeErr < 0)
				panel_expected_c0 -= step;
			else
				panel_expected_c0 += step;
			/*make sure panel_expected_c0 is still valid*/
			/*per Kingdisplay's email OTP fe 40; 0a ec; 0b 88*/
			validatePanelExpectedC0();
		} else {
			/*for samsung panel*/
			/*one step is about 0.2Hz, or 56us/step*/
			int step = (deltaTimeErr >= 0) ? deltaTimeErr : -deltaTimeErr;

			if (step > ALLOWED_NICE_DEVIATION)
				step = (step - ALLOWED_NICE_DEVIATION)/SAMSUNG_TE_ONE_STEP_US;
			else if (step > ALLOWED_PERFECT_DEVIATION)
				step = (step - ALLOWED_PERFECT_DEVIATION)/SAMSUNG_TE_ONE_STEP_US;
			else
				step = 1;
			if (panel_expected_c0 == 0)
				panel_expected_c0 = SAMSUNG_PANEL_TE_INIT_OTP_VALUE;
			if (te_period_status == TE_PERIOD_TOO_LONG)
				panel_expected_c0 -= step;
			else
				panel_expected_c0 += step;

			validatePanelExpectedC0();
		}
	}

	if (panel_working_c0 != panel_expected_c0) {
		pr_info("iris: panel te: working c0:0x%x, expected c0:0x%x\n", panel_working_c0, panel_expected_c0);

		panelTeCalibrationRequired = 1;
		validatePanelExpectedC0();
		ktime_te_start = ktime_get();
		irisAdjustPanelTE(panel, -1, panel_expected_c0, DSI_CMD_SET_STATE_HS);
		irisResetTeRanking();
		te_training_started = 1;
	}
}

#if defined(IRIS_ABYPASS_LOWPOWER)
int iris_abypass_switch_write(unsigned long val)
{
	//unsigned long val;
	static int cnt = 0;

	if (!iris_info.abypass_ctrl.analog_bypass_enable) {
		pr_info("analog bypass is NOT enabled.\n");
		return 0;
	}
	iris_info.abypass_ctrl.abypass_debug = true;
	cnt++;
	if (val) {
		iris_info.abypass_ctrl.pt_to_abypass_enable = 1;
		iris_info.abypass_ctrl.abypass_to_pt_enable = 0;
		iris_info.abypass_ctrl.abypass_switch_state = MCU_STOP_ENTER_STATE;
		iris_info.abypass_ctrl.frame_delay = 0;
		pr_err("pt->analog bypass, %d\n", cnt);
	} else {
		iris_info.abypass_ctrl.pt_to_abypass_enable = 0;
		iris_info.abypass_ctrl.abypass_to_pt_enable = 1;
		iris_info.abypass_ctrl.abypass_switch_state = LOW_POWER_EXIT_STATE;
		iris_info.abypass_ctrl.frame_delay = 0;
		pr_err("analog bypass->pt, %d\n", cnt);
	}
	return 0;
}

void iris_abypass_switch_state_init(int mode)
{
	pr_info("iris_abypass_switch_state_init: %d(analog_bypass_enable=%d)\n",
			mode, iris_info.abypass_ctrl.analog_bypass_enable);

	if (!iris_info.abypass_ctrl.analog_bypass_enable)
		return;

	if (iris_abyp_gpio_init(iris_panel) != 0) {
		pr_err("%s: iris_abyp_gpio_init err!\n", __func__);
		return;
	}
	if (mode == IRIS_MODE_PT2BYPASS) {
		iris_info.abypass_ctrl.pt_to_abypass_enable = 1;
		iris_info.abypass_ctrl.abypass_to_pt_enable = 0;
		iris_info.abypass_ctrl.abypass_switch_state = MCU_STOP_ENTER_STATE;
		iris_info.abypass_ctrl.frame_delay = 0;
		pr_err("%s: pt->analog bypass\n", __func__);
	} else if (mode == IRIS_MODE_BYPASS2PT) {
		iris_info.abypass_ctrl.pt_to_abypass_enable = 0;
		iris_info.abypass_ctrl.abypass_to_pt_enable = 1;
		iris_info.abypass_ctrl.abypass_switch_state = LOW_POWER_EXIT_STATE;
		iris_info.abypass_ctrl.frame_delay = 0;
		pr_err("%s: analog bypass->pt\n", __func__);
	}
}

void iris_low_power_stop_mcu(struct dsi_panel *panel, bool stop, int state)
{
	u32 grcp_len = 0;
	struct dsi_cmd_desc iris_dsi_format_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif

	mutex_lock(&iris_info.dsi_send_mutex);
	memset(&grcp_cmd, 0, sizeof(grcp_cmd));
	memcpy(grcp_cmd.cmd, grcp_header, GRCP_HEADER);
	grcp_cmd.cmd_len = GRCP_HEADER;

	if (stop) {
		iris_cmd_reg_add(&grcp_cmd, IRIS_MIPI_RX_ADDR + DCS_CMD_PARA_2, LP_CMD_MCU_STOP);
	} else {
		/*wake up MCU*/
		iris_cmd_reg_add(&grcp_cmd, 0xf0060008, 0x00000001);
		iris_cmd_reg_add(&grcp_cmd, 0xf006000c, 0x00000001);
	}

	grcp_len = (grcp_cmd.cmd_len - GRCP_HEADER) / 4;
	*(u32 *)(grcp_cmd.cmd + 8) = cpu_to_le32(grcp_len + 1);
	*(u16 *)(grcp_cmd.cmd + 14) = cpu_to_le16(grcp_len);

	iris_dsi_format_cmd.msg.tx_len = grcp_cmd.cmd_len;
	iris_dsi_cmds_send(panel, &iris_dsi_format_cmd, 1, state);
	mutex_unlock(&iris_info.dsi_send_mutex);
	pr_debug("iris: low power mcu stop: %d(packet len %d)\n", stop, (int)iris_dsi_format_cmd.msg.tx_len);
}

u8 iris_mipi_signal_mode_read(struct dsi_panel *panel, enum dsi_cmd_set_state state)
{
	char get_signal_mode[1] = {0x0e};
	const struct mipi_dsi_host_ops *ops = panel->host->ops;
	struct dsi_cmd_desc cmds = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_DCS_READ, 0, 0, 0, sizeof(get_signal_mode), (const void *)get_signal_mode, 1, (void *)iris_read_cmd_buf}, 1, 0};
#else
		{0, MIPI_DSI_DCS_READ, 0, 0, sizeof(get_signal_mode), (const void *)get_signal_mode, 1, (void *)iris_read_cmd_buf}, 1, 0};
#endif
	int rc = 0;
	u8 data = 0xff;

	memset(iris_read_cmd_buf, 0, sizeof(iris_read_cmd_buf));
	if (state == DSI_CMD_SET_STATE_LP)
		cmds.msg.flags |= MIPI_DSI_MSG_USE_LPM;
	if (cmds.last_command)
		cmds.msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;
	cmds.msg.flags |= BIT(6);

	rc = ops->transfer(panel->host, &cmds.msg);
	if (rc < 0) {
		pr_err("iris_mipi_signal_mode_read failed to set cmds(%d), rc=%d, read=%d\n",
			cmds.msg.type,
			rc,
			iris_read_cmd_buf[0]);
		return data;
	}
	data = iris_read_cmd_buf[0];
	if (isAllowedLightupTrace())
		pr_info("iris signal mode: %d\n", data);

	return data;
}
#endif

int iris_abyp_gpio_init(struct dsi_panel *panel)
{
	int ret = -1;

	if (!gpio_is_valid(panel->iris_hw_cfg.iris_wakeup) || panel->iris_hw_cfg.gpio_valid <= 0) {
		pr_err("%s:%d, iris abyp gpio not configured\n", __func__, __LINE__);
		return ret;
	}
	if (gpio_direction_input(panel->iris_hw_cfg.iris_wakeup) != 0) {
		pr_err("%s:%d, iris abyp gpio not input\n", __func__, __LINE__);
		return ret;
	}
	ret = 0;

	return ret;
}

void irisTest(int round, int delay)
{
#define POR_CLOCK2  180 /*0.1MHz*/
#if 0
	int cnt = 0;
#endif
	u32 start_end_delay = 0, pulse_delay = 0;
	unsigned long flags;
	ktime_t ktime0, ktime1, ktime2, ktime3, ktime4;
	u32 deltaUs = 0;
	u32 high_width1 = 0;
	u32 high_width2 = 0;
	u32 low_width1 = 0;
	u32 low_width2 = 0;

	if (!gpio_is_valid(iris_panel->iris_hw_cfg.iris_gpio_test)) {
		pr_err("%s:%d, iris test gpio line not configured\n", __func__, __LINE__);
		return;
	}
	pr_err("%s:%d: pulse count = %d\n", __func__, __LINE__, round);

	start_end_delay = 16*16*16*10/POR_CLOCK2;/*us*/
	pulse_delay = 16*16*2*10/POR_CLOCK2;/*us*/
	if (delay > 0 && delay < 999999)
		pulse_delay = delay;
	if (round == 1) {
		gpio_direction_output(iris_panel->iris_hw_cfg.iris_gpio_test, 0);
		/*added this api into drivers/pinctrl/qcom/pinctrl-msm.c*/
		/*oem_gpio_control(iris_panel->iris_hw_cfg.iris_gpio_test);*/
		pr_err("gpio:%d\n", iris_panel->iris_hw_cfg.iris_gpio_test);
	} else {
		spin_lock_irqsave(&iris_info.iris_lock, flags);
		gpio_direction_output(iris_panel->iris_hw_cfg.iris_gpio_test, 0);
		gpio_set_value(iris_panel->iris_hw_cfg.iris_gpio_test, 0);
		ktime0 = ktime_get();
		gpio_set_value(iris_panel->iris_hw_cfg.iris_gpio_test, 1);
		ktime1 = ktime_get();
		gpio_set_value(iris_panel->iris_hw_cfg.iris_gpio_test, 0);
		ktime2 = ktime_get();
		gpio_set_value(iris_panel->iris_hw_cfg.iris_gpio_test, 1);
		ktime3 = ktime_get();
		gpio_set_value(iris_panel->iris_hw_cfg.iris_gpio_test, 0);
		ktime4 = ktime_get();
		spin_unlock_irqrestore(&iris_info.iris_lock, flags);
		/*end*/
		udelay(start_end_delay);
		deltaUs = (u32)ktime_to_us(ktime4) - (u32)ktime_to_us(ktime0);
		high_width1 = (u32)ktime_to_us(ktime1) - (u32)ktime_to_us(ktime0);
		low_width1 = (u32)ktime_to_us(ktime2) - (u32)ktime_to_us(ktime1);
		high_width2 = (u32)ktime_to_us(ktime3) - (u32)ktime_to_us(ktime2);
		low_width2 = (u32)ktime_to_us(ktime4) - (u32)ktime_to_us(ktime3);
		pr_err("%s(%d): 88whole%d,(H%d,L%d,H%d,L%d)\n",
				__func__, __LINE__, (int)deltaUs,
				(int)high_width1, (int)low_width1,
				(int)high_width2, (int)low_width2);
	}
}

#if defined(IRIS_ABYPASS_LOWPOWER)
void iris_send_delay_packets(struct dsi_panel *panel, enum dsi_cmd_set_state state)
{
	/*add delay for core domain & PLL power up*/
	u32 grcp_len = 0;
	struct dsi_cmd_desc iris_dsi_format_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif
	int i;

	mutex_lock(&iris_info.dsi_send_mutex);
	memset(&grcp_cmd, 0, sizeof(grcp_cmd));
	memcpy(grcp_cmd.cmd, grcp_header, GRCP_HEADER);
	grcp_cmd.cmd_len = GRCP_HEADER;

	for (i = 0; i < 15; i++)
		iris_cmd_reg_add(&grcp_cmd, 0xf0000280, 0x10);

	grcp_len = (grcp_cmd.cmd_len - GRCP_HEADER) / 4;
	*(u32 *)(grcp_cmd.cmd + 8) = cpu_to_le32(grcp_len + 1);
	*(u16 *)(grcp_cmd.cmd + 14) = cpu_to_le16(grcp_len);

	iris_dsi_format_cmd.msg.tx_len = grcp_cmd.cmd_len;
	pr_info("iris_send_delay_packets:(packet len %d)\n", (int)iris_dsi_format_cmd.msg.tx_len);
	iris_dsi_cmds_send(panel, &iris_dsi_format_cmd, 1, state);
	mutex_unlock(&iris_info.dsi_send_mutex);
}

void iris_low_power_mode_set(struct dsi_panel *panel, bool enable, enum dsi_cmd_set_state state)
{
	u32 grcp_len = 0;
	struct dsi_cmd_desc iris_dsi_format_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif
	struct iris_work_mode *pwork_mode = &(iris_info.work_mode);
	struct iris_clock_setting *clock = &iris_info.hw_setting.clock_setting;
	struct iris_pll_setting *pll = &iris_info.hw_setting.pll_setting;

	mutex_lock(&iris_info.dsi_send_mutex);
	memset(&grcp_cmd, 0, sizeof(grcp_cmd));
	memcpy(grcp_cmd.cmd, grcp_header, GRCP_HEADER);
	grcp_cmd.cmd_len = GRCP_HEADER;

	if (enable) {
		/*power down edram1~8*/
		iris_cmd_reg_add(&grcp_cmd, 0xf1040000, 0x1e7fde51);
		iris_cmd_reg_add(&grcp_cmd, 0xf104000c, 0xfffffffe);

		/*disable DPHY*/
		if (pwork_mode->rx_ch) {
			iris_cmd_reg_add(&grcp_cmd, 0xf0160000, 0x00000000);
			iris_cmd_reg_add(&grcp_cmd, 0xf0160030, 0x00000000);
		}
		iris_cmd_reg_add(&grcp_cmd, 0xf0180000, 0x0a00c138);
		iris_cmd_reg_add(&grcp_cmd, 0xf0180004, 0x00000000);
		iris_cmd_reg_add(&grcp_cmd, 0xf0180018, 0x00000101);
		if (pwork_mode->tx_ch) {
			iris_cmd_reg_add(&grcp_cmd, 0xf01c0000, 0x0a00c138);
			iris_cmd_reg_add(&grcp_cmd, 0xf01c0004, 0x00000000);
			iris_cmd_reg_add(&grcp_cmd, 0xf01c0018, 0x00000101);
		}
		/*set clock source to XCLK*/
		iris_cmd_reg_add(&grcp_cmd, 0xf0000210, 0x0000070b);
		iris_cmd_reg_add(&grcp_cmd, 0xf0000214, 0x0000070b);
		iris_cmd_reg_add(&grcp_cmd, 0xf0000218, 0x00000f0b);
		iris_cmd_reg_add(&grcp_cmd, 0xf000021c, 0x0000070b);
		iris_cmd_reg_add(&grcp_cmd, 0xf0000228, 0x0000070b);
		/*power down PLL*/
		iris_cmd_reg_add(&grcp_cmd, 0xf0000140, 0x00000003);
		iris_cmd_reg_add(&grcp_cmd, 0xf0000150, 0x00002003);
		iris_cmd_reg_add(&grcp_cmd, 0xf0000160, 0x00000003);
		/*gate off clock*/
		iris_cmd_reg_add(&grcp_cmd, 0xf0000000, 0x3fc87fff);
		iris_cmd_reg_add(&grcp_cmd, 0xf0000004, 0x00007f8e);
		/*power down MIPI_RX1/FRC/CORE domain*/
		iris_cmd_reg_add(&grcp_cmd, 0xf0000038, 0x1007001f);
		iris_cmd_reg_add(&grcp_cmd, 0xf0000040, 0x0207000f);
		iris_cmd_reg_add(&grcp_cmd, 0xf000003c, 0x1007000f);
	} else {
		/*power up MIPI_RX1/FRC/CORE domain*/
		iris_cmd_reg_add(&grcp_cmd, 0xf0000040, 0x02070000);
		if (pwork_mode->rx_ch)
			iris_cmd_reg_add(&grcp_cmd, 0xf0000038, 0x10070000);
		iris_cmd_reg_add(&grcp_cmd, 0xf000003c, 0x10070000);
		/*power up PLL*/
		iris_cmd_reg_add(&grcp_cmd, 0xf0000140, pll->ppll_ctrl0);
		iris_cmd_reg_add(&grcp_cmd, 0xf0000150, pll->dpll_ctrl0);
		iris_cmd_reg_add(&grcp_cmd, 0xf0000160, pll->mpll_ctrl0);

		grcp_len = (grcp_cmd.cmd_len - GRCP_HEADER) / 4;
		*(u32 *)(grcp_cmd.cmd + 8) = cpu_to_le32(grcp_len + 1);
		*(u16 *)(grcp_cmd.cmd + 14) = cpu_to_le16(grcp_len);
		iris_dsi_format_cmd.msg.tx_len = grcp_cmd.cmd_len;
		pr_info("iris low power mode:enable=%d(packet len %d) line 3185\n",
				enable, (int)iris_dsi_format_cmd.msg.tx_len);
		iris_dsi_cmds_send(panel, &iris_dsi_format_cmd, 1, state);
		mutex_unlock(&iris_info.dsi_send_mutex);
		/*add delay for core domain & PLL power up
		for (i = 0; i < 30; i++)
			iris_cmd_reg_add(&grcp_cmd, 0xf0000280, 0x10);*/
		iris_send_delay_packets(panel, state);
		iris_send_delay_packets(panel, state);

		mutex_lock(&iris_info.dsi_send_mutex);
		memset(&grcp_cmd, 0, sizeof(grcp_cmd));
		memcpy(grcp_cmd.cmd, grcp_header, GRCP_HEADER);
		grcp_cmd.cmd_len = GRCP_HEADER;

		/*gate on clock*/
		if (pwork_mode->rx_ch) {
			iris_cmd_reg_add(&grcp_cmd, 0xf0000000, 0x20000000);
			iris_cmd_reg_add(&grcp_cmd, 0xf0000004, 0x00000000);
		} else {
			iris_cmd_reg_add(&grcp_cmd, 0xf0000000, 0x2cc00044);
			iris_cmd_reg_add(&grcp_cmd, 0xf0000004, 0x00000280);
		}
		iris_cmd_reg_add(&grcp_cmd, 0xf0000248, 0x02008000);
		/*restore clock source*/
		iris_cmd_reg_add(&grcp_cmd,
						IRIS_SYS_ADDR + DCLK_SRC_SEL,
						clock->dclk.sel|(clock->dclk.div<<8)|(clock->dclk.div_en<<10));
		iris_cmd_reg_add(&grcp_cmd,
						IRIS_SYS_ADDR + INCLK_SRC_SEL,
						clock->inclk.sel|(clock->inclk.div<<8)|(clock->inclk.div_en<<10));
		iris_cmd_reg_add(&grcp_cmd,
						IRIS_SYS_ADDR + MCUCLK_SRC_SEL,
						clock->mcuclk.sel|(clock->mcuclk.div<<8)|(clock->mcuclk.div_en<<10));
		iris_cmd_reg_add(&grcp_cmd,
						IRIS_SYS_ADDR + PCLK_SRC_SEL,
						clock->pclk.sel|(clock->pclk.div<<8)|(clock->pclk.div_en<<10));
		iris_cmd_reg_add(&grcp_cmd,
						IRIS_SYS_ADDR + MCLK_SRC_SEL,
						clock->mclk.sel|(clock->mclk.div<<8)|(clock->mclk.div_en<<10));
		/*set TXA_LP_RX_EN/TXB_LP_RX_EN = 1*/
		iris_cmd_reg_add(&grcp_cmd, IRIS_SYS_ADDR + 0x84, 0xf);

		/*enable DPHY*/
		if (pwork_mode->rx_ch) {
			iris_cmd_reg_add(&grcp_cmd, 0xf0160030, 0x00000001);
			iris_cmd_reg_add(&grcp_cmd, 0xf0160000, 0x00000001);
		}
		iris_cmd_reg_add(&grcp_cmd, 0xf0180004, 0x00000001);
		iris_cmd_reg_add(&grcp_cmd, 0xf0180000, 0x0a00c139);
		if (pwork_mode->tx_ch) {
			iris_cmd_reg_add(&grcp_cmd, 0xf01c0004, 0x00000001);
			iris_cmd_reg_add(&grcp_cmd, 0xf01c0000, 0x0a00c139);
		}
		/*power up edram1~8*/
		iris_cmd_reg_add(&grcp_cmd, 0xf104000c, 0x00000000);
		iris_cmd_reg_add(&grcp_cmd, 0xf1040000, 0x1e7fde52);
	}

	grcp_len = (grcp_cmd.cmd_len - GRCP_HEADER) / 4;
	*(u32 *)(grcp_cmd.cmd + 8) = cpu_to_le32(grcp_len + 1);
	*(u16 *)(grcp_cmd.cmd + 14) = cpu_to_le16(grcp_len);

	iris_dsi_format_cmd.msg.tx_len = grcp_cmd.cmd_len;
	pr_info("iris low power mode:enable=%d(packet len %d)line 3254\n",
			enable, (int)iris_dsi_format_cmd.msg.tx_len);
	iris_dsi_cmds_send(panel, &iris_dsi_format_cmd, 1, state);
	mutex_unlock(&iris_info.dsi_send_mutex);
}

void iris_mipitx_te_source_change(struct dsi_panel *panel, enum dsi_cmd_set_state state)
{
	u32 grcp_len = 0;
	struct dsi_cmd_desc iris_dsi_format_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif
	struct iris_work_mode *pwork_mode = &(iris_info.work_mode);
	u32 tx_ch, mipitx_addr = IRIS_MIPI_TX_ADDR;

	mutex_lock(&iris_info.dsi_send_mutex);
	memset(&grcp_cmd, 0, sizeof(grcp_cmd));
	memcpy(grcp_cmd.cmd, grcp_header, GRCP_HEADER);
	grcp_cmd.cmd_len = GRCP_HEADER;

	for (tx_ch = 0; tx_ch < (pwork_mode->tx_ch + 1); tx_ch++) {
#ifdef MIPI_SWAP
		mipitx_addr -= tx_ch*IRIS_MIPI_ADDR_OFFSET;
#else
		mipitx_addr += tx_ch*IRIS_MIPI_ADDR_OFFSET;
#endif
		iris_cmd_reg_add(&grcp_cmd, mipitx_addr + TE_FLOW_CTRL, 0x00000100);
	}
	grcp_len = (grcp_cmd.cmd_len - GRCP_HEADER) / 4;
	*(u32 *)(grcp_cmd.cmd + 8) = cpu_to_le32(grcp_len + 1);
	*(u16 *)(grcp_cmd.cmd + 14) = cpu_to_le16(grcp_len);

	iris_dsi_format_cmd.msg.tx_len = grcp_cmd.cmd_len;
	iris_dsi_cmds_send(panel, &iris_dsi_format_cmd, 1, state);
	mutex_unlock(&iris_info.dsi_send_mutex);
}

void iris_sys_ext_te_sel(struct dsi_panel *panel, enum dsi_cmd_set_state state)
{
	u32 grcp_len = 0;
	struct dsi_cmd_desc iris_dsi_format_cmd = {
#ifdef NEW_MIPI_DSI_MSG
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#else
		{0, MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, CMD_PKT_SIZE, grcp_cmd.cmd, 1, iris_read_cmd_buf}, 1, CMD_PROC};
#endif

	mutex_lock(&iris_info.dsi_send_mutex);
	memset(&grcp_cmd, 0, sizeof(grcp_cmd));
	memcpy(grcp_cmd.cmd, grcp_header, GRCP_HEADER);
	grcp_cmd.cmd_len = GRCP_HEADER;

	iris_cmd_reg_add(&grcp_cmd, IRIS_SYS_ADDR + ALT_CTRL0, 0x00008000);

	grcp_len = (grcp_cmd.cmd_len - GRCP_HEADER) / 4;
	*(u32 *)(grcp_cmd.cmd + 8) = cpu_to_le32(grcp_len + 1);
	*(u16 *)(grcp_cmd.cmd + 14) = cpu_to_le16(grcp_len);

	iris_dsi_format_cmd.msg.tx_len = grcp_cmd.cmd_len;
	iris_dsi_cmds_send(panel, &iris_dsi_format_cmd, 1, state);
	mutex_unlock(&iris_info.dsi_send_mutex);
}

int iris_pt_to_abypass_switch(struct dsi_panel *panel)
{
	int *pswitch_state = &iris_info.abypass_ctrl.abypass_switch_state;
	int *pframe_delay = &iris_info.abypass_ctrl.frame_delay;
	int bypass_mode = BYPASS_CMD, pwil_mode = PWIL_CMD, ret = 0;
	u8 gpio_value;
	static ktime_t kt0, kt1;

	if (!iris_info.abypass_ctrl.pt_to_abypass_enable)
		return ret;
	if (iris_info.work_mode.rx_mode) {
		bypass_mode = BYPASS_CMD;
		pwil_mode = PWIL_CMD;
	} else {
		bypass_mode = BYPASS_VIDEO;
		pwil_mode = PWIL_VIDEO;
	}
	/*if state switch need delay several video frames*/
	if (*pframe_delay > 0)
		*pframe_delay -= 1;
	if (*pframe_delay > 0)
		return ret;

	switch (*pswitch_state) {
	case MCU_STOP_ENTER_STATE:
		kt0 = ktime_get();
		/*MCU stop*/
		iris_low_power_stop_mcu(panel, true, DSI_CMD_SET_STATE_HS);
		*pswitch_state = LOW_POWER_ENTER_STATE;
		*pframe_delay = 3;
		break;
	case LOW_POWER_ENTER_STATE:
		gpio_value = gpio_get_value(panel->iris_hw_cfg.iris_wakeup);
		if (gpio_value == 1) {
			/*set Rx pwil mode*/
			iris_mipirx_mode_set(panel, pwil_mode, DSI_CMD_SET_STATE_LP);
			/*enter low power mode*/
			iris_low_power_mode_set(panel, true, DSI_CMD_SET_STATE_LP);
			*pswitch_state = ANALOG_BYPASS_STATE;
		}
		pr_info("%s: Enter ABYP. GPIO wakeup value:%d\n", __func__, gpio_value);
		*pframe_delay = 1;
		break;
	case ANALOG_BYPASS_STATE:
		*pframe_delay = 0;
		iris_info.abypass_ctrl.abypass_status = ANALOG_BYPASS_MODE;
		iris_info.abypass_ctrl.pt_to_abypass_enable = 0;
		ret = 1;
		kt1 = ktime_get();
		pr_info("%s: pt->abyp: total_time: %d us\n", __func__, (u32)ktime_to_us(kt1) - (u32)ktime_to_us(kt0));
		iris_feature_setting_init(&iris_info.user_setting);
		iris_feature_setting_init(&iris_info.chip_setting);
		stay_in_lowpower_abypass = IRIS_IN_ONE_WIRED_ABYPASS;
		break;
	default:
		break;
	}

	pr_info("iris_pt_to_abypass_switch state: %d, delay: %d\n", *pswitch_state, *pframe_delay);

	return ret;
}

int iris_abypass_to_pt_switch(struct dsi_panel *panel)
{
	int *pswitch_state = &iris_info.abypass_ctrl.abypass_switch_state;
	int *pframe_delay = &iris_info.abypass_ctrl.frame_delay;
	int bypass_mode = BYPASS_CMD, pwil_mode = PWIL_CMD, ret = 0;
	u8 gpio_value = 0;
	static ktime_t kt0, kt1;

	if (!iris_info.abypass_ctrl.abypass_to_pt_enable)
		return ret;

	if (iris_info.work_mode.rx_mode) {
		bypass_mode = BYPASS_CMD;
		pwil_mode = PWIL_CMD;
	} else {
		bypass_mode = BYPASS_VIDEO;
		pwil_mode = PWIL_VIDEO;
	}
	/*if state switch need delay several video frames*/
	if (*pframe_delay > 0)
		*pframe_delay -= 1;
	if (*pframe_delay > 0)
		return ret;

	switch (*pswitch_state) {
	case LOW_POWER_EXIT_STATE:
		kt0 = ktime_get();
		/*exit low power mode*/
		iris_low_power_mode_set(panel, false, DSI_CMD_SET_STATE_LP);
		*pswitch_state = MCU_STOP_EXIT_STATE;
		*pframe_delay = 1;
		break;
	case MCU_STOP_EXIT_STATE:
		/*resume MCU*/
		iris_low_power_stop_mcu(panel, false, DSI_CMD_SET_STATE_LP);
		*pswitch_state = ANALOG_BYPASS_EXIT_STATE;
		/*AP should wait Firmware ready*/
		*pframe_delay = 10;
		break;
	case ANALOG_BYPASS_EXIT_STATE:
		/*analog bypass*/
		gpio_value = gpio_get_value(panel->iris_hw_cfg.iris_wakeup);
		if (gpio_value == 0) {
			*pswitch_state = PASS_THROUGH_STATE;
		}
		pr_info("%s: Exit ABYP. GPIO wakeup value: %d\n", __func__, gpio_value);
		*pframe_delay = 1;
		break;
	case PASS_THROUGH_STATE:
		iris_mipirx_mode_set(panel, pwil_mode, DSI_CMD_SET_STATE_LP);
		iris_pwil_mode_set(panel, PT_MODE, DSI_CMD_SET_STATE_HS);
		/*change TX's TE source*/
		if (iris_info.work_mode.tx_mode)
			iris_mipitx_te_source_change(panel, DSI_CMD_SET_STATE_LP);
		*pswitch_state = CHANGE_EXT_TE_STATE;
		*pframe_delay = 5;
		break;
	case CHANGE_EXT_TE_STATE:
		/*change SYS's EXT_TE*/
		if (iris_info.work_mode.tx_mode)
			iris_sys_ext_te_sel(panel, DSI_CMD_SET_STATE_LP);
		*pframe_delay = 0;
		iris_info.abypass_ctrl.abypass_status = PASS_THROUGH_MODE;
		iris_info.abypass_ctrl.abypass_to_pt_enable = 0;
		ret = 1;
		stay_in_lowpower_abypass = 0;
		kt1 = ktime_get();
		pr_info("%s: abyp->pt: total_time: %d us\n", __func__, (u32)ktime_to_us(kt1) - (u32)ktime_to_us(kt0));
		break;
	default:
		break;
	}

	pr_info("iris_abypass_to_pt_switch state: %d, delay: %d\n", *pswitch_state, *pframe_delay);

	return ret;
}
#endif /*defined(IRIS_ABYPASS_LOWPOWER)*/

static struct dsi_ctrl *iris_get_dsi_ctrl(void)
{
	struct dsi_display *display = iris_display;
	struct dsi_ctrl *ctrl = NULL;

	if (display->ctrl_count > 1) {
		pr_err("%s: ctrl_count is not right %d\n", __func__, display->ctrl_count);
		return NULL;
	}
	ctrl = display->ctrl[display->clk_master_idx].ctrl;
	return ctrl;
}

static struct drm_encoder *iris_get_drm_encoder(void)
{
	struct dsi_display *display = iris_display;
	struct dsi_bridge *bridge = NULL;
	struct drm_encoder *drm_enc = NULL;

	bridge = display->bridge;
	drm_enc = bridge->base.encoder;

	return drm_enc;
}

static int iris_wait_lanes_idle(void)
{
	int rc = 0;
	u32 lanes = 0;
	struct dsi_ctrl *dsi_ctrl = NULL;

	dsi_ctrl = iris_get_dsi_ctrl();
	if (dsi_ctrl->host_config.panel_mode == DSI_OP_CMD_MODE)
		lanes = dsi_ctrl->host_config.common_config.data_lanes;
	rc = dsi_ctrl->hw.ops.wait_for_lane_idle(&dsi_ctrl->hw, lanes);
	if (rc)
		pr_err("lanes not entering idle\n");
	return rc;
}

static void iris_sde_encoder_idle(void)
{
	struct drm_encoder *drm_enc = NULL;

	drm_enc = iris_get_drm_encoder();
	sde_encoder_wait_for_idle(drm_enc);
}

bool iris_sde_encoder_rc_lock(void)
{
	struct dsi_ctrl *ctrl = NULL;
	struct drm_encoder *drm_enc = NULL;

	ctrl = iris_get_dsi_ctrl();
	drm_enc = iris_get_drm_encoder();
	/*ctrl_lock will be needed by transfter_msg*/
	mutex_lock(&ctrl->ctrl_lock);
	sde_encoder_rc_lock(drm_enc);
	iris_sde_encoder_idle();
	iris_wait_lanes_idle();

	return true;
}

bool iris_sde_encoder_rc_unlock(void)
{
	struct dsi_ctrl *ctrl = NULL;
	struct drm_encoder *drm_enc = NULL;

	ctrl = iris_get_dsi_ctrl();
	drm_enc = iris_get_drm_encoder();

	sde_encoder_rc_unlock(drm_enc);
	mutex_unlock(&ctrl->ctrl_lock);

	return true;
}

void iris_enable_fullfunction(struct dsi_panel *panel)
{
	u32 cmd = 0;

	/* firmware download */
	iris_info.firmware_info.app_fw_state =
		iris_firmware_download(panel, APP_FIRMWARE_NAME, true);
	if (true == iris_info.firmware_info.app_fw_state) {
		cmd = REMAP + ITCM_COPY;
		iris_ctrl_cmd_send(panel, cmd, DSI_CMD_SET_STATE_HS);
		iris_info.firmware_info.app_cnt = 0;
		iris_full_functioning = 1;
		iris_hw_status_check_enabled = true;
		iris_hw_status_check_started = true;
	}

	iris_feature_setting_update_check();
	iris_mode_switch_state_reset();
	iris_info.power_status.low_power_state = LP_STATE_POWER_UP;
}

#define IRIS_STATUS_CHECK_INTERVAL_US        5000000 /*every 5 seconds*/
#define SYS_INSTAT_RAW                        0x1ffe4

void iris_check_hw_status(void)
{
	static ktime_t ktime_last_check;
	ktime_t ktime_now;
	u32 timeus_gone = 0;

	if (!iris_hw_status_check_allowed)
		return;

	if (irisIsInAbypassMode()
		|| irisIsInLowPowerAbypassMode()
		|| irisStayInLowPowerAbypass())
		return;

	if (!iris_hw_status_check_enabled || !iris_hw_status_check_allowed)
		return;

	if (iris_hw_status_check_started) {
		iris_hw_status_check_started = false;
		ktime_last_check = ktime_get();
		return;
	}

	ktime_now = ktime_get();
	timeus_gone = (u32)ktime_to_us(ktime_now) - (u32)ktime_to_us(ktime_last_check);
	if (timeus_gone >= IRIS_STATUS_CHECK_INTERVAL_US) {
		u32 reg_value = 0;
#ifdef ENABLE_IRIS_I2C_READ
		iris_i2c_reg_read(IRIS_SYS_ADDR + SYS_INSTAT_RAW, &reg_value);
#else
		iris_mipi_reg_read(IRIS_SYS_ADDR + SYS_INSTAT_RAW, &reg_value);
#endif
		if (reg_value>>20) {
			pr_err("SYS_INSTAT_RAW: 0x%x", reg_value);

			iris_hw_status_check_enabled = false;
			iris_enable_failure_recover(iris_panel);
			iris_enable_fullfunction(iris_panel);
		} else {
			ktime_last_check = ktime_get();
			if (isAllowedLightupTrace())
				pr_err("SYS_INSTAT_RAW: iris stat OK\n");
		}
	}
}
