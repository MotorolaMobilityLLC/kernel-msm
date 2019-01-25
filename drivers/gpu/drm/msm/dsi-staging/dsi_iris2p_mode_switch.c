
#include "dsi_iris2p_def.h"

u16 lp_memc_timing[2] = {IRIS_DTG_HRES_SETTING, IRIS_DTG_VRES_SETTING};
static uint16_t ratio[LEVEL_MAX] = { 8, 6, 5, 4, 4 };
static uint16_t ratio_720[LEVEL_MAX] = { 8, 6, 5, 4, 3};

static uint16_t ratio_16to10_level[LEVEL_MAX][2] = {
	{1212, 758},
	{1152, 720},
	{1024, 640},
	{640, 400},
	{480, 300}
};

static uint16_t ratio_16to9_level[LEVEL_MAX][2] = {
	{1280, 720},
	{1280, 720},
	{1024, 576},
	{640, 360},
	{480, 270}
};

static uint16_t ratio_4to3_level[LEVEL_MAX][2] = {
	{1200, 800},
	{1024, 768},
	{800, 600},
	{640, 480},
	{480, 360}
};

static enum iris_frc_prepare_state eiris_frc_prepare_state;
static enum iris_mode_rfb2frc_state eiris_mode_rfb2frc_state;
static enum iris_mode_frc2rfb_state eiris_mode_frc2rfb_state;
static enum iris_rfb_prepare_state eiris_rfb_prepare_state;
static enum iris_frc_cancel_state eiris_frc_cancel_state;
static enum iris_pt_prepare_state eiris_pt_prepare_state;
static enum iris_mode_pt2rfb_state eiris_mode_pt2rfb_state;
static enum iris_mode_rfb2pt_state eiris_mode_rfb2pt_state;


u32 iris_lp_memc_calc(u32 value)
{
	u32 hres, vres;
	u32 frc_timing = (u32)lp_memc_timing[1] << 16 | (u32)lp_memc_timing[0];
	static enum res_ratio ratio_type;

	hres = iris_info.work_mode.rx_ch ? (u32)iris_info.input_timing.hres * 2 : (u32)iris_info.input_timing.hres;
	vres = (u32)iris_info.input_timing.vres;

	if (value >= LEVEL_MAX)
	{
		value = LEVEL_MAX - 1;
		pr_err("#### %s:%d, Low Power MEMC level is out of range.\n", __func__, __LINE__);
	}

	pr_debug("#### %s:%d, Low Power MEMC level hres = %d, vres = %d, rx_ch = %d.\n", __func__, __LINE__,
						hres, vres, iris_info.work_mode.rx_ch);

	if(((hres / 4)	== (vres / 3)) || ((hres / 3)  == (vres / 4)))
	{
		ratio_type = ratio_4to3;
	}
	else if(((hres / 16)  == (vres / 10)) || ((hres / 10)  == (vres / 16)))
	{
		ratio_type = ratio_16to10;
	}
	else
	{
		ratio_type = ratio_16to9;
	}
	ratio_type = ratio_16to9;
	if ((hres * vres) >= ((u32) IMG1080P_HSIZE * (u32) IMG1080P_VSIZE))
	{
		switch (ratio_type) {
			case ratio_4to3:
				lp_memc_timing[0] = ratio_4to3_level[value][hres > vres ? 0 : 1];
				lp_memc_timing[1] = ratio_4to3_level[value][hres > vres ? 1 : 0];
				if (hres*10 / (u32)lp_memc_timing[0] > 40)
				{
					lp_memc_timing[0] = ratio_4to3_level[value-1][hres > vres ? 0 : 1];
					lp_memc_timing[1] = ratio_4to3_level[value-1][hres > vres ? 1 : 0];
				}
				break;
			case ratio_16to10:
				lp_memc_timing[0] = ratio_16to10_level[value][hres > vres ? 0 : 1];
				lp_memc_timing[1] = ratio_16to10_level[value][hres > vres ? 1 : 0];
				if (hres*10 / (u32)lp_memc_timing[0] > 40) {
					lp_memc_timing[0] = ratio_16to10_level[value-1][hres > vres ? 0 : 1];
					lp_memc_timing[1] = ratio_16to10_level[value-1][hres > vres ? 1 : 0];
				}
				break;
			case ratio_16to9:
				lp_memc_timing[0] = ratio_16to9_level[value][hres > vres ? 0 : 1];
				lp_memc_timing[1] = ratio_16to9_level[value][hres > vres ? 1 : 0];
				if (hres*10 / (u32)lp_memc_timing[0] > 40) {
					lp_memc_timing[0] = ratio_16to9_level[value-1][hres > vres ? 0 : 1];
					lp_memc_timing[1] = ratio_16to9_level[value-1][hres > vres ? 1 : 0];
				}
				break;
			default:
				break;
		};
	} else if ((hres * vres) > ((u32) IMG720P_HSIZE * (u32) IMG720P_VSIZE)) {
		lp_memc_timing[0] = hres * ratio_720[value] / RATIO_NUM;
		lp_memc_timing[1] = vres * ratio_720[value] / RATIO_NUM;
	} else {
		lp_memc_timing[0] = hres * ratio[value] / RATIO_NUM;
		lp_memc_timing[1] = vres * ratio[value] / RATIO_NUM;
	}

	frc_timing = (u32)lp_memc_timing[1] << 16 | (u32)lp_memc_timing[0];

	pr_debug("#### %s:%d,wHres: %d, wVres: %d, low power memc timing: 0x%x\n", __func__, __LINE__, lp_memc_timing[0], lp_memc_timing[1], frc_timing);

	return frc_timing;
}


static bool iris_pp_unit_enable_check(void)
{
	struct feature_setting *chip_setting = &iris_info.chip_setting;

	if ((0 == chip_setting->pq_setting.peaking)
		&& (0 == chip_setting->cm_setting.cm6axes)
		&& (0 == chip_setting->cm_setting.cm3d)
		&& (0 == chip_setting->cm_setting.color_temp_en)
		&& (0 == chip_setting->reading_mode.readingmode)
		&& (0 == chip_setting->hdr_setting.hdren))
		return false;
	else
		return true;
}

static bool iris_dbc_lce_enable_check(void)
{
	struct feature_setting *chip_setting = &iris_info.chip_setting;

	if ((0 == chip_setting->lce_setting.mode)
		&& (0 == chip_setting->dbc_setting.cabcmode)
		&& (0 == chip_setting->dbc_setting.dlv_sensitivity))
		return false;
	else
		return true;
}

static void iris_memc_path_commands_update(void)
{
	struct iris_mode_state *mode_state = &iris_info.mode_state;
	struct iris_fun_enable *enable = (struct iris_fun_enable *)&mode_state->frc_path;
	struct iris_dtg_setting *dtg_setting = &iris_info.dtg_setting;

	enable->true_cut_en = 0;
	enable->nrv_drc_en = 0;

	if (iris_pp_unit_enable_check())
		enable->pp_en = 1;
	else
		enable->pp_en = 0;
	enable->use_efifo_en = 0;
	enable->psr_post_sel = 0;
	enable->frc_data_format = 1;
	enable->capt_bitwidth = 0;
	enable->psr_bitwidth = 0;
	enable->dpp_en = 1;

	if (iris_dbc_lce_enable_check())
		enable->dbc_lce_en = 1;
	else
		enable->dbc_lce_en = 0;

	iris_cmd_reg_add(&meta_cmd, IRIS_DTG_ADDR + EVS_NEW_DLY, dtg_setting->evs_new_dly);
	iris_cmd_reg_add(&meta_cmd, IRIS_DTG_ADDR + DTG_REGSEL, 1);

	iris_cmd_reg_add(&meta_cmd, IRIS_DATA_PATH_ADDR, mode_state->frc_path);
	iris_cmd_reg_add(&meta_cmd, IRIS_MODE_ADDR, 0x800002);
}

static void iris_pt_path_commands_update(void)
{
	struct iris_mode_state *mode_state = &iris_info.mode_state;
	struct iris_fun_enable *enable = (struct iris_fun_enable *)&mode_state->pt_path;

	if (iris_pp_unit_enable_check())
		enable->pp_en = 1;
	else
		enable->pp_en = 0;

	enable->use_efifo_en = 1;
	enable->psr_post_sel = 0;
	enable->frc_data_format = 1;
	enable->capt_bitwidth = 0;
	enable->psr_bitwidth = 0;

	if (iris_dbc_lce_enable_check())
		enable->dbc_lce_en = 1;
	else
		enable->dbc_lce_en = 0;

	iris_cmd_reg_add(&meta_cmd, IRIS_DATA_PATH_ADDR, mode_state->pt_path);
	iris_cmd_reg_add(&meta_cmd, IRIS_MODE_ADDR, 0x800000);
}

static void iris_rfb_path_commands_update(void)
{
	struct iris_mode_state *mode_state = &iris_info.mode_state;
	struct iris_fun_enable *enable = (struct iris_fun_enable *)&mode_state->rfb_path;

	if (iris_pp_unit_enable_check())
		enable->pp_en = 1;
	else
		enable->pp_en = 0;
	enable->use_efifo_en = 0;
	enable->psr_post_sel = 0;
	enable->frc_data_format = 1;
	enable->capt_bitwidth = 0;
	enable->psr_bitwidth = 0;
	enable->dpp_en = 1;
	if (iris_dbc_lce_enable_check())
		enable->dbc_lce_en = 1;
	else
		enable->dbc_lce_en = 0;

	iris_cmd_reg_add(&meta_cmd, IRIS_DTG_ADDR + EVS_NEW_DLY, 1);
	iris_cmd_reg_add(&meta_cmd, IRIS_DTG_ADDR + DTG_REGSEL, 1);

	iris_cmd_reg_add(&meta_cmd, IRIS_DATA_PATH_ADDR, mode_state->rfb_path);
	iris_cmd_reg_add(&meta_cmd, IRIS_MODE_ADDR, 0x800001);
}

int iris_proc_frcc_setting(void)
{
	struct iris_frc_setting *frc_setting = &iris_info.frc_setting;
	// default val of reference register which need host to set.
	u32 val_frcc_reg5 = 0x3c000000;
	u32 val_frcc_reg8 = 0x10000000;
	u32 val_frcc_reg16 = 0x413120c8;
	u32 val_frcc_reg17 = 0x8000;
	u32 val_frcc_reg18 = 0;
	u32 val_frcc_cmd_th = 0x8000;
	u32 val_frcc_dtg_sync = 0;
	u32 video_ctrl2 = 0x92041b09;

	//formula variable
	u32 ThreeCoreEn, VD_CAP_DLY1_EN;
	u32 MaxFIFOFI, KeepTH, CarryTH, RepeatP1_TH, phase_map_en = 1;
	u32 RepeatCF_TH, TS_FRC_EN, INPUT_RECORD_THR, MERAREC_THR_VALID;
	u32 MetaGen_TH1, MetaGen_TH2, MetaRec_TH1, MetaRec_TH2;
	u32 FIRepeatCF_TH;

	//timing and feature variable
	u32 te_fps, display_vsync, Input_Vres, Scaler_EN = false, Capture_EN, Input_Vtotal;
	u32 DisplayVtotal, HsyncFreqIn, HsyncFreqOut, InVactive, StartLine, Vsize;
	int inputwidth = (iris_info.work_mode.rx_ch ? iris_info.input_timing.hres * 2 : iris_info.input_timing.hres);
	u32 Infps = frc_setting->in_fps;
	int adjustmemclevel = 3;
	int hlmd_func_enable = 0;
	int memclevel = iris_info.chip_setting.pq_setting.memclevel;

	//init variable
	te_fps = iris_info.output_timing.fps;
	display_vsync = iris_info.output_timing.fps;//iris to panel, TODO, or 120
	Input_Vres = iris_info.input_timing.vres;
	Capture_EN = 0;
	Input_Vtotal = iris_get_vtotal(&iris_info.input_timing);
	if (lp_memc_timing[0] != inputwidth)
		Scaler_EN = true;
	else
		Scaler_EN = false;
	DisplayVtotal = iris_get_vtotal(&iris_info.output_timing);
	HsyncFreqIn = te_fps * Input_Vtotal;
	HsyncFreqOut = display_vsync * DisplayVtotal;
	InVactive = 0;
	if (Capture_EN)
		StartLine = Input_Vres - InVactive;
	else if (Scaler_EN)
		StartLine = 5;
	else
		StartLine = 0;
	if (Capture_EN)
		Vsize = InVactive;
	else
		Vsize = Input_Vtotal;//DisplayVtotal;

	pr_debug("%s: get timing info, infps=%d, displayVtotal = %d, InVactive = %d, StartLine = %d, Vsize = %d\n",
		__func__, Infps, DisplayVtotal, InVactive, StartLine, Vsize);
	pr_debug("TE_fps = %d, display_vsync = %d, inputVres = %d, Scaler_EN = %d, capture_en = %d, InputVtotal = %d\n",
		te_fps, display_vsync, Input_Vres, Scaler_EN, Capture_EN, Input_Vtotal);

	switch (Infps) {
		case 24://24fps
			ThreeCoreEn = 0; VD_CAP_DLY1_EN = 0; MaxFIFOFI = 3; KeepTH = 252; CarryTH = 5;
			RepeatP1_TH = 5; RepeatCF_TH = 252; TS_FRC_EN = 0; MERAREC_THR_VALID = 1;
			MetaGen_TH1 = (Vsize / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			MetaGen_TH2 = (Vsize * 6 / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			MetaRec_TH1 = (Vsize * 5 / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			MetaRec_TH2 = (Vsize * 7 / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			INPUT_RECORD_THR = (Vsize  / 2 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			FIRepeatCF_TH = 252;
			break;
		case 30://30fps
			ThreeCoreEn = 0; VD_CAP_DLY1_EN = 0; MaxFIFOFI = 2; KeepTH = 252; CarryTH = 5;
			RepeatP1_TH = 5; RepeatCF_TH = 252; TS_FRC_EN = 0; MERAREC_THR_VALID = 1;
			if (frc_setting->low_delay) {
				video_ctrl2 = 0x12041209;
				phase_map_en = 0;
				MetaGen_TH1 = 200;
				MetaRec_TH1 = 300;
				INPUT_RECORD_THR = 100;
			} else {
				video_ctrl2 = 0x92041b09;
				phase_map_en = 1;
				MetaGen_TH1 = (Vsize / 2 + StartLine) * HsyncFreqOut / HsyncFreqIn;
				MetaRec_TH1 = (Vsize * 5 / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
				INPUT_RECORD_THR = (Vsize  / 2 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			}

			MetaGen_TH2 = (Vsize * 6 / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			MetaRec_TH2 = (Vsize * 7 / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			FIRepeatCF_TH = 252;
			break;
		case 25://25fps
			ThreeCoreEn = 1; VD_CAP_DLY1_EN = 0; MaxFIFOFI = 3; KeepTH = 253; CarryTH = 2;
			RepeatP1_TH = 2; RepeatCF_TH = 253; TS_FRC_EN = 0; MERAREC_THR_VALID = 1;
			MetaGen_TH1 = (Vsize / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			MetaGen_TH2 = (Vsize * 5 / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			MetaRec_TH1 = (Vsize * 3 / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			MetaRec_TH2 = (Vsize * 7 / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			INPUT_RECORD_THR = (Vsize  / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn - 10;
			FIRepeatCF_TH = 253;
			break;
		case 15://15fps
			hlmd_func_enable = 0;
			RepeatP1_TH = 2;
			CarryTH = 2;

			ThreeCoreEn = 0; VD_CAP_DLY1_EN = 0; MaxFIFOFI = 5; KeepTH = 253;
			RepeatCF_TH = 253; TS_FRC_EN = 1; MERAREC_THR_VALID = 1;
			MetaGen_TH1 = (Vsize / 2 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			MetaGen_TH2 = (Vsize * 6 / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			MetaRec_TH1 = (Vsize * 5 / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			MetaRec_TH2 = (Vsize * 7 / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			INPUT_RECORD_THR = (Vsize  / 2 + StartLine) * HsyncFreqOut / HsyncFreqIn - 10;
			FIRepeatCF_TH = 253;
		case 12://12fps
			ThreeCoreEn = 0; VD_CAP_DLY1_EN = 0; MaxFIFOFI = 5; KeepTH = 253; CarryTH = 2;
			RepeatP1_TH = 2; RepeatCF_TH = 253; TS_FRC_EN = 1; MERAREC_THR_VALID = 1;
			MetaGen_TH1 = (Vsize / 2 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			MetaGen_TH2 = (Vsize * 6 / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			MetaRec_TH1 = (Vsize * 5 / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			MetaRec_TH2 = (Vsize * 7 / 8 + StartLine) * HsyncFreqOut / HsyncFreqIn;
			INPUT_RECORD_THR = (Vsize  / 2 + StartLine) * HsyncFreqOut / HsyncFreqIn - 10;
			FIRepeatCF_TH = 253;
			break;
		case 60:// use as repeat mode
			val_frcc_cmd_th = frc_setting->frcc_cmd_th;
			val_frcc_reg8 = frc_setting->frcc_reg8;
			val_frcc_reg16 = frc_setting->frcc_reg16;
			val_frcc_cmd_th &= 0x1fffffff;
			val_frcc_cmd_th |= 0x20000000;
			val_frcc_reg8 |= 0xff00;
			val_frcc_reg16 &= 0xffff7fff;
			goto SET_REG_REPEAT;

		default:
			pr_err("%s, using default frcc parameters\n", __func__);
			goto SET_REG;
	}

	//todo
	if (memclevel == 3)
		adjustmemclevel = 3;
	else if (memclevel == 2)
		adjustmemclevel = 3;
	else if (memclevel == 1)
		adjustmemclevel = 2;
	else if (memclevel == 0)
		adjustmemclevel = 0;

	//val_frcc_reg5 = val_frcc_reg5 + ((pqlt_cur_setting->pq_setting.memclevel & 0x3) << 17) + (KeepTH * 2 << 7) + CarryTH;
	val_frcc_reg5 = val_frcc_reg5 + ((adjustmemclevel & 0x3) << 17) + (KeepTH << 8) + CarryTH + (phase_map_en << 16);
	val_frcc_reg8 = val_frcc_reg8 + (RepeatP1_TH << 8) + RepeatCF_TH;
	val_frcc_reg16 = val_frcc_reg16 + (TS_FRC_EN << 31) + (ThreeCoreEn << 15) + VD_CAP_DLY1_EN;
	val_frcc_reg17 = val_frcc_reg17 + (DisplayVtotal << 16) + INPUT_RECORD_THR;
	val_frcc_reg18 = val_frcc_reg18 + (MERAREC_THR_VALID << 31) + (MetaRec_TH2 << 16) + MetaRec_TH1;
	val_frcc_cmd_th = val_frcc_cmd_th + (MaxFIFOFI << 29) + (MetaGen_TH2 << 16) + MetaGen_TH1;
	val_frcc_dtg_sync |= FIRepeatCF_TH << FI_REPEATCF_TH;

SET_REG:
	pr_debug("%s: reg5=%x, reg8=%x, reg16=%x, reg17=%x, reg18=%x, cmd_th=%x\n", __func__,
		val_frcc_reg5, val_frcc_reg8, val_frcc_reg16, val_frcc_reg17, val_frcc_reg18, val_frcc_cmd_th);

	iris_cmd_reg_add(&meta_cmd, IRIS_PWIL_ADDR + VIDEO_CTRL2, video_ctrl2);
	iris_cmd_reg_add(&meta_cmd, FRCC_CTRL_REG5_ADDR, val_frcc_reg5);
	iris_cmd_reg_add(&meta_cmd, FRCC_CTRL_REG8_ADDR, val_frcc_reg8);
	iris_cmd_reg_add(&meta_cmd, FRCC_CTRL_REG16_ADDR, val_frcc_reg16);
	iris_cmd_reg_add(&meta_cmd, FRCC_CTRL_REG17_ADDR, val_frcc_reg17);
	iris_cmd_reg_add(&meta_cmd, FRCC_CTRL_REG18_ADDR, val_frcc_reg18);
	iris_cmd_reg_add(&meta_cmd, FRCC_DTG_SYNC, val_frcc_dtg_sync);
	iris_cmd_reg_add(&meta_cmd, FRCC_CMD_MOD_TH, val_frcc_cmd_th);
	iris_cmd_reg_add(&meta_cmd, FRCC_REG_SHOW, 0x2);
	iris_cmd_reg_add(&meta_cmd, IRIS_MVC_ADDR + 0x1ffe8, 0x00000000);

	frc_setting->frcc_cmd_th = val_frcc_cmd_th;
	frc_setting->frcc_reg8 = val_frcc_reg8;
	frc_setting->frcc_reg16 = val_frcc_reg16;
	return 0;
SET_REG_REPEAT:
	iris_cmd_reg_add(&meta_cmd, FRCC_CTRL_REG8_ADDR, val_frcc_reg8);
	iris_cmd_reg_add(&meta_cmd, FRCC_CTRL_REG16_ADDR, val_frcc_reg16);
	iris_cmd_reg_add(&meta_cmd, FRCC_CMD_MOD_TH, val_frcc_cmd_th);
	iris_cmd_reg_add(&meta_cmd, FRCC_REG_SHOW, 0x2);
	return 0;
}
#if 0
static int iris_memc_demo_win_set(struct fi_demo_win fi_demo_win_para)
{
	int endx, endy, startx, starty;
	static int hres, vres;
	int displaywidth = (iris_info.work_mode.tx_ch  ?  iris_info.output_timing.hres * 2 : iris_info.output_timing.hres);

	if ((lp_memc_timing[0] != hres) || (lp_memc_timing[1] != vres)) {
		// todo
		//iris_info.update.demo_win_fi = true;
		hres = lp_memc_timing[0];
		vres = lp_memc_timing[1];
	}

	// todo
	//if (iris_info.update.demo_win_fi && (iris_info.setting_info.quality_cur.pq_setting.memcdemo == 5)) {
	if (0) {
		//iris_info.update.demo_win_fi = false;
		startx = iris_info.fi_demo_win_info.startx * lp_memc_timing[0] / displaywidth;
		starty = iris_info.fi_demo_win_info.starty * lp_memc_timing[1] / iris_info.output_timing.vres;
		endx = iris_info.fi_demo_win_info.endx *  lp_memc_timing[0] / displaywidth;
		endy = iris_info.fi_demo_win_info.endy *  lp_memc_timing[1] / iris_info.output_timing.vres;

		if (endy + iris_info.fi_demo_win_info.borderwidth >= lp_memc_timing[1])
			endy = lp_memc_timing[1] - iris_info.fi_demo_win_info.borderwidth;

		pr_debug("iris: %s: startx = %d, starty = %d, endx = %d, endy = %d, lp_memc_timing[0] = %d, lp_memc_timing[1] = %d.\n",
				__func__, startx, starty, endx, endy, lp_memc_timing[0], lp_memc_timing[1]);
		fi_demo_win_para.colsize = (startx & 0xfff) | ((endx & 0xfff) << 16);
		fi_demo_win_para.rowsize = (starty & 0xfff) | ((endy & 0xfff) << 16);

		iris_cmd_reg_add(&meta_cmd, FI_DEMO_COL_SIZE, fi_demo_win_para.colsize);
		iris_cmd_reg_add(&meta_cmd, FI_DEMO_MODE_RING, fi_demo_win_para.color);
		iris_cmd_reg_add(&meta_cmd, FI_DEMO_ROW_SIZE, fi_demo_win_para.rowsize);
		iris_cmd_reg_add(&meta_cmd, FI_DEMO_MODE_CTRL, fi_demo_win_para.modectrl);
		iris_cmd_reg_add(&meta_cmd, FI_SHADOW_UPDATE, 1);
	}
	return 0;
}
#endif
static u32 iris_proc_scale(u32 dvts, u32 prev_dvts)
{
	u32 scale;
	if (abs(dvts-prev_dvts) <= ((dvts + prev_dvts) >> 5))
		scale = 64;
	else {
		scale = (dvts * 64 + prev_dvts / 2) / prev_dvts;
		scale = min((u32)255, scale);
		scale = max((u32)16, scale);
	}
	return scale;
}

static void iris_set_constant_ratio(void)
{
	unsigned int reg_in, reg_out, reg_scale, top_ctrl0;
	struct iris_frc_setting *frc_setting = &iris_info.frc_setting;

	reg_in = frc_setting->in_ratio << IRIS_PWIL_IN_FRAME_SHIFT | (1 << 15);
	reg_out = frc_setting->out_ratio << IRIS_PWIL_OUT_FRAME_SHIFT;
	reg_scale = 4096/frc_setting->scale << 24 | 64 << 16 | frc_setting->scale << 8 | frc_setting->scale;
	top_ctrl0 = (1 << ROTATION) | (1 << PHASE_SHIFT_EN);

	iris_cmd_reg_add(&meta_cmd, IRIS_PWIL_ADDR + 0x12FC, reg_in);
	iris_cmd_reg_add(&meta_cmd, IRIS_PWIL_ADDR + 0x0638, reg_out);

	iris_cmd_reg_add(&meta_cmd, IRIS_PWIL_ADDR + 0x10000, (1 << 8) | (1 << 6));
	//iris_cmd_reg_add(&meta_cmd, IRIS_MVC_ADDR + IRIS_MVC_TOP_CTRL0_OFF, top_ctrl0);
	iris_cmd_reg_add(&meta_cmd, IRIS_MVC_ADDR + 0x1D0, reg_scale);
	iris_cmd_reg_add(&meta_cmd, IRIS_MVC_ADDR + 0x1FF00, 1);
}

int iris_proc_constant_ratio(void)
{
	u32 dvts, in_t, out_t;
	uint32_t r;
	struct iris_frc_setting *frc_setting = &iris_info.frc_setting;
	struct iris_timing_info *output_timing = &iris_info.output_timing;

	dvts = 1000000 / frc_setting->in_fps;
	in_t = (dvts * output_timing->fps + 50000) / 100000;
	out_t = 10;

	r = gcd(in_t, out_t);
	pr_debug("in_t %u out_t %u r %u\n", in_t, out_t, r);
	frc_setting->in_ratio = out_t / r;
	frc_setting->out_ratio = in_t / r;
	frc_setting->scale = iris_proc_scale(dvts, dvts);

	pr_info("in/out %u:%u\n", frc_setting->in_ratio, frc_setting->out_ratio);
	// update register
	iris_set_constant_ratio();

	return 0;
}





int iris_mode_frc_prepare(void)
{
	struct iris_mode_state *mode_state = &iris_info.mode_state;
	u32 val = 0;
	int ret = false;

	switch (eiris_frc_prepare_state)
	{
		case IRIS_FRC_PATH_PROXY:
			iris_memc_path_commands_update();

			mode_state->kickoff_cnt = 1;
			eiris_frc_prepare_state = IRIS_FRC_WAIT_PREPARE_DONE;
			break;
		case IRIS_FRC_WAIT_PREPARE_DONE:
			if (mode_state->kickoff_cnt++ < 30) {
				#ifdef READ_CMD_ENABLE
					if (mode_state->kickoff_cnt > 5) {
					#ifdef ENABLE_IRIS_I2C_READ
						iris_i2c_reg_read(IRIS_MODE_ADDR, &val);
					#else
						iris_mipi_reg_read(IRIS_MODE_ADDR, &val);
					#endif
						if(val == 2)
							val = IRIS_FRC_PRE;
					}
				#else
					if (mode_state->kickoff_cnt == 25)
						val = IRIS_FRC_PRE;
				#endif

				if (val != IRIS_FRC_PRE)
					pr_info("iris: mode = %08x, cnt = %d\n", val, mode_state->kickoff_cnt);
				else
					eiris_frc_prepare_state = IRIS_FRC_PRE_DONE;
			} else {
				pr_debug("iris: memc prep time out\n");
				eiris_frc_prepare_state = IRIS_FRC_PRE_TIMEOUT;
			}
			break;
		case IRIS_FRC_PRE_DONE:
			iris_proc_frcc_setting();
			//iris_memc_demo_win_set(iris_info.fi_demo_win_info);
			iris_proc_constant_ratio();
			ret = true;
			break;
		case IRIS_FRC_PRE_TIMEOUT:
			break;
		default:
			break;
	}

	return ret;

}

void iris_update_frc_configs(void)
{
    iris_proc_frcc_setting();
    iris_proc_constant_ratio();
}

static int iris_mode_rfb2frc(struct dsi_panel *panel)
{
	int ret = false;

	switch (eiris_mode_rfb2frc_state)
	{
		case IRIS_RFB_FRC_SWITCH_COMMAND:
			iris_pwil_mode_set(panel, FRC_MODE, DSI_CMD_SET_STATE_HS);
			eiris_mode_rfb2frc_state = IRIS_RFB_FRC_SWITCH_DONE;
			break;
		case IRIS_RFB_FRC_SWITCH_DONE:
			ret = true;
			break;
		default:
			break;
	}

	return ret;
}

static int iris_mode_frc_cancel(void)
{
	int ret = false;

	switch (eiris_frc_cancel_state)
	{
		case IRIS_FRC_CANCEL_PATH_PROXY:
			iris_rfb_path_commands_update();

			eiris_frc_cancel_state = IRIS_FRC_CANCEL_DONE;
			break;
		case IRIS_FRC_CANCEL_DONE:
			ret = true;
			break;
		default:
			break;
	}

	return ret;
}

static int iris_mode_rfb_prepare(void)
{
	int ret = false;

	switch (eiris_rfb_prepare_state)
	{
		case IRIS_RFB_PATH_PROXY:
			eiris_rfb_prepare_state = IRIS_RFB_WAIT_PREPARE_DONE;
			break;
		case IRIS_RFB_WAIT_PREPARE_DONE:
			eiris_rfb_prepare_state = IRIS_RFB_PRE_DONE;
			break;
		case IRIS_RFB_PRE_DONE:
			ret = true;
			break;
		default:
			break;
	}

	return ret;
}

static int iris_mode_frc2rfb(struct dsi_panel *panel)
{
	struct iris_mode_state *mode_state = &iris_info.mode_state;
	int ret = false;

	switch (eiris_mode_frc2rfb_state)
	{
		case IRIS_FRC_RFB_SWITCH_COMMAND:
			iris_pwil_mode_set(panel, RFB_MODE, DSI_CMD_SET_STATE_HS);
			eiris_mode_frc2rfb_state = IRIS_FRC_RFB_DATA_PATH;
			mode_state->kickoff_cnt = 0;
			break;

		case IRIS_FRC_RFB_DATA_PATH:
			mode_state->kickoff_cnt++;
			if (mode_state->kickoff_cnt <= 2)
				break;
			iris_rfb_path_commands_update();

			eiris_mode_frc2rfb_state = IRIS_FRC_RFB_SWITCH_DONE;
			mode_state->kickoff_cnt = 0;
			break;

		case IRIS_FRC_RFB_SWITCH_DONE:
			mode_state->kickoff_cnt++;
			if (mode_state->kickoff_cnt <= 6)
				break;
			else
				ret = true;
			break;
		default:
			break;
	}

	return ret;

}



#if defined(IRIS_ABYPASS_LOWPOWER)

static bool pt_switch_ongoing = false;

static int iris_mode_pt2rfb(struct dsi_panel *panel)
{
	int ret = false;

	switch (eiris_mode_pt2rfb_state) {
	case IRIS_PT_RFB_SWITCH_COMMAND:
		iris_pwil_mode_set(panel, RFB_MODE, DSI_CMD_SET_STATE_HS);
		eiris_mode_pt2rfb_state = IRIS_PT_RFB_DATA_PATH;
		break;
	case IRIS_PT_RFB_DATA_PATH:
		iris_rfb_path_commands_update();
		eiris_mode_pt2rfb_state = IRIS_PT_RFB_SWITCH_DONE;
		break;
	case IRIS_PT_RFB_SWITCH_DONE:
		pt_switch_ongoing = false;
		ret = true;
		break;
	default:
		break;
	}

	return ret;
}


static int iris_mode_pt_prepare(struct dsi_panel *panel)
{
	int ret = false;

	switch (eiris_pt_prepare_state) {
	case IRIS_PT_PATH_PROXY:
		iris_pt_path_commands_update();
		eiris_pt_prepare_state = IRIS_PT_WAIT_PREPARE_DONE;
		break;
	case IRIS_PT_WAIT_PREPARE_DONE:
		eiris_pt_prepare_state = IRIS_PT_PRE_DONE;
		break;
	case IRIS_PT_PRE_DONE:
		ret = true;
		break;
	default:
		break;
	}

	return ret;
}

static int iris_mode_rfb2pt(struct dsi_panel *panel)
{
	int ret = false;

	switch (eiris_mode_rfb2pt_state) {
	case IRIS_RFB_PT_SWITCH_COMMAND:
		iris_pwil_mode_set(panel, PT_MODE, DSI_CMD_SET_STATE_HS);
		eiris_mode_rfb2pt_state = IRIS_RFB_PT_DATA_PATH;
		break;
	case IRIS_RFB_PT_DATA_PATH:
		eiris_mode_rfb2pt_state = IRIS_RFB_PT_SWITCH_DONE;
		break;
	case IRIS_RFB_PT_SWITCH_DONE:
		pt_switch_ongoing = false;
		ret = true;
		break;
	default:
		break;
	}

	return ret;
}
#endif

bool iris_mode_bypass_state_check(void)
{
	struct iris_mode_state *mode_state = &iris_info.mode_state;
	bool rc = false;

	rc = (mode_state->current_mode == IRIS_BYPASS_MODE)
		|| irisIsAbypassSwitchOngoing()
		|| irisIsInAbypassMode()
		|| irisIsInLowPowerAbypassMode();

	return rc;
}

void iris_mode_switch_state_reset(void)
{
	struct iris_mode_state *mode_state = &iris_info.mode_state;

	mode_state->sf_mode_switch_trigger = true;
	mode_state->sf_notify_mode = IRIS_MODE_RFB;
	mode_state->current_mode = (irisIsInAbypassMode()) ? IRIS_BYPASS_MODE : IRIS_RFB_MODE;
	mode_state->start_mode = -1;
	mode_state->next_mode = -1;
	mode_state->target_mode = -1;
}

static u32 switchTimeusCost = -1;
static ktime_t ktime_mdsw_start;
static ktime_t ktime_mdsw_end;

void irisLowPowerAbypassSwitch(struct dsi_panel *panel, int opt)
{
	struct iris_mode_state *mode_state = &iris_info.mode_state;

	if (mode_state->target_mode != mode_state->current_mode
		&& mode_state->target_mode != mode_state->start_mode) {
		//switch is ongoing
		pr_info("%s(%d): request ongoing from %d (next %d) into %d(current %d, sf %d)",
			__func__,
			__LINE__,
			mode_state->start_mode,
			mode_state->next_mode,
			mode_state->target_mode,
			mode_state->current_mode,
			mode_state->sf_notify_mode);
		return;
	}

	pr_info("%s(%d): request ongoing from %d (next %d) into %d(current %d, sf %d)",
			__func__,
			__LINE__,
			mode_state->start_mode,
			mode_state->next_mode,
			mode_state->target_mode,
			mode_state->current_mode,
			mode_state->sf_notify_mode);
	//general: from frb->lp abypass, or reverse from lp abypass ->rfb
	ktime_mdsw_start = ktime_get();

	if (opt == IRIS_IN_ONE_WIRED_ABYPASS) {
		mode_state->start_mode = mode_state->current_mode;//enum iris_mode from def.h
		mode_state->target_mode = IRIS_BYPASS_MODE;

		if (mode_state->start_mode == IRIS_RFB_MODE)
			mode_state->next_mode = IRIS_PT_PRE;
		else if (mode_state->start_mode == IRIS_PT_MODE)
			mode_state->next_mode = IRIS_BYPASS_MODE;

		if (mode_state->next_mode == IRIS_PT_PRE)
			mode_state->sf_notify_mode = IRIS_MODE_PT_PREPARE; //from device.h
		else if (mode_state->next_mode == IRIS_BYPASS_MODE)
			mode_state->sf_notify_mode = IRIS_MODE_PT2BYPASS;

		if (mode_state->target_mode != mode_state->current_mode)
			mode_state->sf_mode_switch_trigger = true;
	} else {
		mode_state->start_mode = mode_state->current_mode;//enum iris_mode from def.h
		mode_state->target_mode = IRIS_RFB_MODE;

		if (mode_state->start_mode == IRIS_BYPASS_MODE)
			mode_state->next_mode = IRIS_PT_MODE;
		else if (mode_state->start_mode == IRIS_PT_MODE)
			mode_state->next_mode = IRIS_RFB_PRE;

		if (mode_state->next_mode == IRIS_RFB_PRE)
			mode_state->sf_notify_mode = IRIS_MODE_RFB_PREPARE; //from device.h
		else if (mode_state->next_mode == IRIS_PT_MODE)
			mode_state->sf_notify_mode = IRIS_MODE_BYPASS2PT;

		if (mode_state->target_mode != mode_state->current_mode)
			mode_state->sf_mode_switch_trigger = true;
	}
}

void irisModeSwitchMonitor(struct dsi_panel *panel)
{
	struct iris_mode_state *mode_state = &iris_info.mode_state;

	if (mode_state->target_mode == mode_state->current_mode
		|| mode_state->target_mode == mode_state->start_mode) {
		//mode switdch was done
		return;
	}

	pr_info("%s(%d): current state: start %d next %d target %d(current %d, sf %d)",
			__func__,
			__LINE__,
			mode_state->start_mode,
			mode_state->next_mode,
			mode_state->target_mode,
			mode_state->current_mode,
			mode_state->sf_notify_mode);

	//ongoing
	if (mode_state->next_mode != mode_state->current_mode)
		return;

	if (iris_info.context_info.backlight_on == 0
		&& iris_info.context_info.iris_stay_in_abypass_en > 0) {
		pr_info("irisModeSwitchMonitor: wait for backlight on before switching into abypass mode.");
		return;
	}

	//general: from rfb->lp abypass or reverse from lp abypass ->rfb
	if (mode_state->target_mode == IRIS_BYPASS_MODE) {//enter bypass
		switch (mode_state->current_mode) {
		case IRIS_RFB_MODE:
			mode_state->next_mode = IRIS_PT_PRE;
			break;
		case IRIS_PT_PRE:
			mode_state->next_mode = IRIS_PT_MODE;
			break;
		case IRIS_PT_MODE:
			mode_state->next_mode = IRIS_BYPASS_MODE;
			break;
		default:
			break;
		}

		switch (mode_state->next_mode) {
		case IRIS_PT_PRE:
			mode_state->sf_notify_mode = IRIS_MODE_PT_PREPARE;
			break;
		case IRIS_PT_MODE:
			mode_state->sf_notify_mode = IRIS_MODE_RFB2PT;
			break;
		case IRIS_BYPASS_MODE:
			mode_state->sf_notify_mode = IRIS_MODE_PT2BYPASS;
			break;
		default:
			break;
		}

		if (mode_state->target_mode != mode_state->current_mode
			&& mode_state->next_mode != mode_state->current_mode)
			mode_state->sf_mode_switch_trigger = true;
	} else if (mode_state->target_mode == IRIS_RFB_MODE
			&& mode_state->start_mode == IRIS_BYPASS_MODE) {//exit abypass
		switch (mode_state->current_mode) {
		case IRIS_BYPASS_MODE:
			mode_state->next_mode = IRIS_PT_MODE;
			break;
		case IRIS_PT_MODE:
			mode_state->next_mode = IRIS_RFB_PRE;
			break;
		case IRIS_RFB_PRE:
			mode_state->next_mode = IRIS_RFB_MODE;
			break;
		default:
			break;
		}

		switch (mode_state->next_mode) {
		case IRIS_PT_MODE:
			mode_state->sf_notify_mode = IRIS_MODE_BYPASS2PT;
			break;
		case IRIS_RFB_PRE:
			mode_state->sf_notify_mode = IRIS_MODE_RFB_PREPARE;
			break;
		case IRIS_RFB_MODE:
			mode_state->sf_notify_mode = IRIS_MODE_PT2RFB;
			break;
		default:
			break;
		}

		if (mode_state->target_mode != mode_state->current_mode
			&& mode_state->next_mode != mode_state->current_mode)
			mode_state->sf_mode_switch_trigger = true;
	}

	if (mode_state->sf_mode_switch_trigger)
		pr_info("irisModeSwitchMonitor: mode switch triggered ...");
}

void iris_mode_switch_cmd(struct dsi_panel *panel)
{
	struct iris_mode_state *mode_state = &iris_info.mode_state;
	int pre_current_mode = mode_state->current_mode;

	if(mode_state->sf_mode_switch_trigger) {
		pr_info("%s(%d): request to switch mode from %d into %d",
			__func__, __LINE__,
			mode_state->current_mode,
			mode_state->sf_notify_mode);

		mode_state->sf_mode_switch_trigger = false;
		switch (mode_state->sf_notify_mode) {
			case IRIS_MODE_FRC_PREPARE:
				eiris_frc_prepare_state = IRIS_FRC_PATH_PROXY;
				break;
			case IRIS_MODE_RFB2FRC:
				eiris_mode_rfb2frc_state = IRIS_RFB_FRC_SWITCH_COMMAND;
				break;
			case IRIS_MODE_RFB_PREPARE:
				eiris_rfb_prepare_state = IRIS_RFB_PRE_DONE;
				break;
			case IRIS_MODE_FRC2RFB:
				eiris_mode_frc2rfb_state = IRIS_FRC_RFB_SWITCH_COMMAND;
				break;
			case IRIS_MODE_FRC_CANCEL:
				eiris_frc_cancel_state = IRIS_FRC_CANCEL_PATH_PROXY;
				break;
#if defined(IRIS_ABYPASS_LOWPOWER)
			case IRIS_MODE_PT_PREPARE:
			case IRIS_MODE_PTLOW_PREPARE:
				eiris_pt_prepare_state = IRIS_PT_PATH_PROXY;
				break;
			case IRIS_MODE_RFB2PT:
				eiris_mode_rfb2pt_state = IRIS_RFB_PT_SWITCH_COMMAND;
				break;
			case IRIS_MODE_PT2RFB:
				eiris_mode_pt2rfb_state = IRIS_PT_RFB_SWITCH_COMMAND;
				break;
			case IRIS_MODE_PT2BYPASS:
				iris_abypass_switch_state_init(IRIS_MODE_PT2BYPASS);
				if (!iris_debug_power_opt_disable) {
					/*delay one frame to wait clock on, for power optimization*/
					return;
				}
				break;
			case IRIS_MODE_BYPASS2PT:
				iris_abypass_switch_state_init(IRIS_MODE_BYPASS2PT);
				break;
#endif
			default:
				break;
		}
	}
	switch(mode_state->sf_notify_mode) {
		case IRIS_MODE_FRC_PREPARE:
			if(mode_state->current_mode != IRIS_RFB_MODE)
				break;
			if(true == iris_mode_frc_prepare())
			{
				mode_state->current_mode = IRIS_FRC_PRE;
				mode_state->sf_notify_mode = IRIS_MODE_FRC_PREPARE_DONE;
			}
			else
			{
				if(eiris_frc_prepare_state == IRIS_FRC_PRE_TIMEOUT)
				{
					mode_state->sf_notify_mode = IRIS_MODE_FRC_PREPARE_TIMEOUT;
				}
			}
			break;
		case IRIS_MODE_RFB2FRC:
			if(mode_state->current_mode != IRIS_FRC_PRE)
				break;
			if(true == iris_mode_rfb2frc(panel))
			{
				mode_state->current_mode = IRIS_FRC_MODE;
				mode_state->sf_notify_mode = IRIS_MODE_FRC;
			}
			break;
		case IRIS_MODE_FRC_CANCEL:
			if((mode_state->current_mode == IRIS_FRC_PRE) || (eiris_frc_prepare_state == IRIS_FRC_PRE_TIMEOUT))
			{
				if (true == iris_mode_frc_cancel()) {
					mode_state->current_mode = IRIS_RFB_MODE;
					eiris_frc_prepare_state = IRIS_FRC_PATH_PROXY;
					mode_state->sf_notify_mode = IRIS_MODE_RFB;
				}
			}
			break;
		case IRIS_MODE_RFB_PREPARE:
			if((mode_state->current_mode == IRIS_FRC_MODE) || (mode_state->current_mode == IRIS_PT_MODE))
			{
				if(true == iris_mode_rfb_prepare())
				{
					mode_state->current_mode = IRIS_RFB_PRE;
					mode_state->sf_notify_mode = IRIS_MODE_RFB_PREPARE_DONE;
				}
			}
			break;
		case IRIS_MODE_FRC2RFB:
			if(mode_state->current_mode != IRIS_RFB_PRE)
				 break;
			if(true == iris_mode_frc2rfb(panel))
			{
				mode_state->current_mode = IRIS_RFB_MODE;
				mode_state->sf_notify_mode = IRIS_MODE_RFB;
			}
			break;
#if defined(IRIS_ABYPASS_LOWPOWER)
		case IRIS_MODE_PT_PREPARE:
		case IRIS_MODE_PTLOW_PREPARE:
			if (mode_state->current_mode != IRIS_RFB_MODE)
				break;
			if (iris_mode_pt_prepare(panel)) {
				pt_switch_ongoing = true;
				mode_state->current_mode = IRIS_PT_PRE;
				mode_state->sf_notify_mode = IRIS_MODE_PT_PREPARE_DONE;
			}
			break;
		case IRIS_MODE_RFB2PT:
			if (mode_state->current_mode != IRIS_PT_PRE)
				break;
			if (iris_mode_rfb2pt(panel)) {
				mode_state->current_mode = IRIS_PT_MODE;
				mode_state->sf_notify_mode = IRIS_MODE_PT;
			}
			break;
		case IRIS_MODE_PT2RFB:
			if (mode_state->current_mode != IRIS_RFB_PRE)
				break;
			if (iris_mode_pt2rfb(panel)) {
				mode_state->current_mode = IRIS_RFB_MODE;
				mode_state->sf_notify_mode = IRIS_MODE_RFB;
			}
			break;
		case IRIS_MODE_PT2BYPASS:
			if (mode_state->current_mode != IRIS_PT_MODE)
				break;
			if (iris_pt_to_abypass_switch(panel)) {
				mode_state->current_mode = IRIS_BYPASS_MODE;
				mode_state->sf_notify_mode = IRIS_MODE_BYPASS;
				iris_resume_panel_brightness(panel);
			}
			break;
		case IRIS_MODE_BYPASS2PT:
			if (mode_state->current_mode != IRIS_BYPASS_MODE)
				break;
			if (iris_abypass_to_pt_switch(panel)) {
				mode_state->current_mode = IRIS_PT_MODE;
				mode_state->sf_notify_mode = IRIS_MODE_PT;
				iris_resume_panel_brightness(panel);
			}
			break;
#endif
		default:
			break;
	}

	if (pre_current_mode != mode_state->current_mode) {
		pr_info("%s, %d: mode from %d to %d\n", __func__, __LINE__, pre_current_mode, mode_state->current_mode);
		if (mode_state->current_mode == mode_state->target_mode) {
			ktime_mdsw_end = ktime_get();
			switchTimeusCost = (u32)ktime_to_us(ktime_mdsw_end) - (u32)ktime_to_us(ktime_mdsw_start);
			pr_info("%s(%d): completed (%d us)current state: start %d next %d target %d(current %d, sf %d)",
				__func__,
				__LINE__,
				(int)switchTimeusCost,
				mode_state->start_mode,
				mode_state->next_mode,
				mode_state->target_mode,
				mode_state->current_mode,
				mode_state->sf_notify_mode);

			//we arrived the target mode, reset
			mode_state->start_mode = mode_state->target_mode;
		}
	}
}
