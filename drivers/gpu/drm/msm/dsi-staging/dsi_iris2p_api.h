#ifndef DSI_IRIS2P_API_H
#define DSI_IRIS2P_API_H
#include "dsi_iris2p_hw.h"
#include "dsi_panel.h"
#include "dsi_display.h"

extern void iris_info_init(struct dsi_panel *panel);
extern void iris_pre_lightup(struct dsi_panel *panel);
extern void iris_lightup(struct dsi_panel *panel);
extern void iris_bp_lightup(struct dsi_panel *panel);
extern int iris_handover_dcs_set_display_brightness(struct dsi_panel *panel, u16 brightness, enum dsi_cmd_set_state state);

extern void iris2p_register_fs(void);
extern void iris2p_unregister_fs(void);
extern void iris_cmd_kickoff_proc(void);
extern void iris_drm_device_handle_get(struct dsi_display *display, struct drm_device *drm_dev);
extern u32 iris_hdr_enable_get(void);
extern void iris_firmware_download_cont_splash(struct dsi_panel *panel);
extern int get_iris_status(void);
extern int notify_iris_esd_exit(void);
extern void iris_lightdown(struct dsi_panel *panel);
extern bool irisIsAllowedDsiTrace(void);
extern bool irisIsAllowedBasicTrace(void);
extern bool irisIsAllowedPanelOnOffTrace(void);
extern bool irisIsAllowedContinuedSplashTrace(void);
extern void irisReportTeCount(int vsync_count);
extern bool irisIsSimulatedHostDsiCtrlFailureRecover(void);
extern bool irisIsSimulatedPanelInitFailureRecover(void);
extern u8   iris_mipi_power_mode_read(struct dsi_panel *panel, enum dsi_cmd_set_state state);
extern void iris_reset_failure_recover(struct dsi_panel *panel);
extern int  iris_check_powermode(struct dsi_panel *panel, u8 data, u8 retry_cnt);

extern int dsi_iris_power_on(struct dsi_panel *panel);
extern int dsi_iris_power_off(struct dsi_panel *panel);
extern int dsi_iris_pt_power(struct dsi_panel *panel, bool enable);
extern int dsi_panel_parse_iris_gpios(struct dsi_panel *panel, struct device_node *of_node);
extern void iris_enable_ref_clk(struct dsi_panel *panel, struct device *parent);
extern int iris_dsi_panel_enable(struct dsi_panel *panel);
extern int iris_dsi_gpio_free(struct dsi_panel *panel);
extern int notify_iris_esd_cancel(void *display);
extern int iris_work_enable(bool enable);
#endif
