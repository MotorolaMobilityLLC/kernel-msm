#include "dsi_iris2p_api.h"

#define IRIS_VDD_GPIO_CONN
#define IRIS_RESET_GPIO_CONN
#define IRIS_WAKEUP_GPIO_CONN
#define USE_EXTERNAL_CLOCK

#if defined(CONFIG_IRIS2P_FULL_SUPPORT)
#if defined(USE_EXTERNAL_CLOCK)
static bool iris_clk3_prepared = false;
#endif

static int dsi_panel_tx_cmd_set(struct dsi_panel *panel,
				enum dsi_cmd_set_type type);

int dsi_iris_power_on(struct dsi_panel *panel)
{
        int rc = 0;

        pr_info("dsi_iris_power_on\n");
#if defined(USE_EXTERNAL_CLOCK)
        if (panel->iris_hw_cfg.div_clk3 != NULL){
                rc = clk_prepare_enable(panel->iris_hw_cfg.div_clk3);
                if (rc) {
                        pr_err("Unable to enable div_clk3\n");
                } else {
                        iris_clk3_prepared = true;
                        usleep_range(10*1000, 10*1000);/*wait stable clock*/
                        pr_info("dsi_iris_power_on: clk enabled external div_clk3\n");
                }
        }
#else
        pr_info("dsi_iris_power_on: clk xtal\n");
#endif

        if (panel->iris_hw_cfg.gpio_valid <= 0)
                return rc;

#ifdef IRIS_VDD_GPIO_CONN
        if (gpio_is_valid(panel->iris_hw_cfg.iris_1v1)) {
                rc = gpio_direction_output(panel->iris_hw_cfg.iris_1v1, 1);
                if (rc) {
                        pr_err("unable to set dir for disp gpio rc=%d\n", rc);
                        return rc;
                }
		pr_info("dsi_iris_power_on: 1.1v power up\n");
        } else {
                pr_err("dsi_iris_power_on: [fatal] invalid 1.1v\n");
        }
#endif
#ifdef IRIS_RESET_GPIO_CONN
       if (gpio_is_valid(panel->iris_hw_cfg.iris_reset_gpio)) {
                rc = gpio_direction_output(panel->iris_hw_cfg.iris_reset_gpio, 0);
                if (rc) {
                        pr_err("unable to set iris_reset_gpio rc=%d\n", rc);
                        return rc;
                }
                usleep_range(10*1000,  10*1000);
                rc = gpio_direction_output(panel->iris_hw_cfg.iris_reset_gpio, 1);
                if (rc) {
                        pr_err("unable to set iris_reset_gpio rc=%d\n", rc);
                        return rc;
                }
                usleep_range(10*1000,  10*1000);
		pr_info("dsi_iris_power_on: reset by gpio %d\n", panel->iris_hw_cfg.iris_reset_gpio);
        } else {
                pr_err("dsi_iris_power_on: [fatal] invalid reset gpio\n");
        }
#endif
        return rc;
}

int dsi_iris_power_off(struct dsi_panel *panel)
{
        int rc = 0;

        pr_info("dsi_iris_power_off: iris active gpio count %d\n", panel->iris_hw_cfg.gpio_valid);

        if (panel->iris_hw_cfg.gpio_valid <= 0)
                return rc;

#ifdef IRIS_VDD_GPIO_CONN
        if (gpio_is_valid(panel->iris_hw_cfg.iris_1v1)) {
                rc = gpio_direction_output(panel->iris_hw_cfg.iris_1v1, 0);
                if (rc) {
                        pr_err("unable to set dir for disp gpio rc=%d\n", rc);
                        return rc;
                }
		pr_info("dsi_iris_power_off: iris VDD_EN set low %d\n", panel->iris_hw_cfg.iris_1v1);
        }
#endif

#if defined(USE_EXTERNAL_CLOCK)
        if (panel->iris_hw_cfg.div_clk3 != NULL && iris_clk3_prepared){
                clk_disable_unprepare(panel->iris_hw_cfg.div_clk3);
		iris_clk3_prepared = false;
		pr_info("dsi_iris_power_off: iris clk3 disable unprepare.\n");
        }
#endif
        return rc;
}

static int pt_power_enable = 0;
int dsi_iris_pt_power(struct dsi_panel *panel, bool enable)
{
        int rc = 0, i = 0;
        struct dsi_vreg *vreg;
        int num_of_v = 0;

        for (i = 0; i < panel->power_info.count; i++) {
                vreg = &panel->power_info.vregs[i];
                if (!strcmp(vreg->vreg_name, "vddio")) {
                        break;
                }
        }
        if (i == panel->power_info.count) {
                pr_err("dsp no find vddio\n");
                rc = -1;
                goto exit;
        }

        if (enable) {
                if (1 == pt_power_enable) {
                        pr_info("vddio has already power on");
                        goto exit;
                }
                /* vddio power-up */
                pr_info("vddio enable");
                rc = regulator_set_load(vreg->vreg,
                                                   vreg->enable_load);
                if (rc) {
                   pr_err("regulator_set_load fail\n");
                }
                num_of_v = regulator_count_voltages(vreg->vreg);
                if (num_of_v > 0) {
                   rc = regulator_set_voltage(vreg->vreg,
                                                  vreg->min_voltage,
                                                  vreg->max_voltage);
                   if (rc) {
                           pr_err("Set voltage(%s) fail, rc=%d\n",
                                        vreg->vreg_name, rc);
                   }
                }
                rc = regulator_enable(vreg->vreg);
                if (rc) {
                   pr_err("regulator_enable fail\n");
                }

#ifdef IRIS_VDD_GPIO_CONN
                if (gpio_is_valid(panel->iris_hw_cfg.iris_1v1)) {
                   rc = gpio_direction_output(panel->iris_hw_cfg.iris_1v1, 1);
                   if (rc) {
                           pr_err("unable to set dir for disp gpio rc=%d\n", rc);
                           goto exit;
                   }
                }
#endif
                pt_power_enable = 1;
        } else {
                /*  power-off */
                if (0 == pt_power_enable) {
                        pr_info("vddio has already vddio disable");
                        goto exit;
                }
                pr_info("vddio disable");
                regulator_set_load(vreg->vreg,
                                        vreg->disable_load);
                regulator_disable(vreg->vreg);

#ifdef IRIS_VDD_GPIO_CONN
                if (gpio_is_valid(panel->iris_hw_cfg.iris_1v1)) {
                        rc = gpio_direction_output(panel->iris_hw_cfg.iris_1v1, 0);
                        if (rc) {
                                pr_err("unable to set dir for disp gpio rc=%d\n", rc);
                                goto exit;
                        }
                }
#endif
                pt_power_enable = 0;
        }

exit:
        return rc;
}

int dsi_panel_parse_iris_gpios(struct dsi_panel *panel, struct device_node *of_node)
{
        int rc = 0;

	panel->iris_hw_cfg.gpio_valid = 0;
        memset(&panel->iris_hw_cfg, 0, sizeof(struct iris_hw_config));
	panel->iris_hw_cfg.iris_reset_gpio = -1;
	panel->iris_hw_cfg.iris_1v1 = -1;
	panel->iris_hw_cfg.iris_wakeup = -1;
#ifdef IRIS_RESET_GPIO_CONN
        panel->iris_hw_cfg.iris_reset_gpio = of_get_named_gpio(of_node,
                                              "qcom,dsp-reset-gpio",
                                              0);
        if (!gpio_is_valid(panel->iris_hw_cfg.iris_reset_gpio)) {
                pr_err("[%s] failed get dsp reset gpio, rc=%d\n", panel->name, rc);
                rc = -EINVAL;
                return rc;
        }
	panel->iris_hw_cfg.gpio_valid += 1;
#endif
#ifdef IRIS_VDD_GPIO_CONN
        panel->iris_hw_cfg.iris_1v1 = of_get_named_gpio(of_node,
                                                  "qcom,dsp-iris_1v1",
                                                  0);
        if (!gpio_is_valid(panel->iris_hw_cfg.iris_1v1)) {
                pr_err("[%s] failed get dsp 1v1 power, rc=%d\n", panel->name, rc);
                rc = -EINVAL;
                return rc;
        }
	panel->iris_hw_cfg.gpio_valid += 1;
#endif
#ifdef IRIS_WAKEUP_GPIO_CONN
	panel->iris_hw_cfg.iris_wakeup = of_get_named_gpio(of_node,
                                                  "qcom,dsp-wakeup-gpio",
                                                  0);
        if (!gpio_is_valid(panel->iris_hw_cfg.iris_wakeup)) {
                pr_err("[%s] failed get dsp wakeup gpio, rc=%d\n", panel->name, rc);
                rc = -EINVAL;
                return rc;
        }
        panel->iris_hw_cfg.gpio_valid += 1;
#endif

#if 0
        panel->iris_hw_cfg.iris_gpio_test = of_get_named_gpio(of_node,
                                                  "qcom,dsp-test-gpio",
                                                  0);
        if (!gpio_is_valid(panel->iris_hw_cfg.iris_gpio_test)) {
                pr_err("[%s] failed get dsp test gpio, rc=%d\n", panel->name, rc);
                rc = -EINVAL;
                return rc;
        }
#endif
        return rc;
}

void iris_enable_ref_clk(struct dsi_panel *panel, struct device *parent)
{
#if defined(CONFIG_IRIS2P_FULL_SUPPORT)
	int rc = 0;
	panel->iris_hw_cfg.div_clk3 = devm_clk_get(parent, "div_clk3");
	if (IS_ERR(panel->iris_hw_cfg.div_clk3)) {
		pr_err("%s:Unable to get div_clk3\n", __func__);
		panel->iris_hw_cfg.div_clk3 = NULL;
	} else {
		pr_info("%s: iris get div_clk3 ok", __func__);
                rc = clk_prepare_enable(panel->iris_hw_cfg.div_clk3);
                if (rc) {
                        pr_err("dsi_panel_get: Unable to enable div_clk3 for iris2p\n");
                } else {
                        iris_clk3_prepared = true;
                        //usleep_range(10*1000, 10*1000);/*wait stable clock*/
                        pr_info("dsi_panel_get: continued-splash: clk enabled external div_clk3 for iris2p OK\n");
                }
	}
#endif
}

int iris_dsi_panel_enable(struct dsi_panel *panel)
{
	int rc = 0;
#define PANEL_RECOVERY_RETRY_MAX         (2)
#define PANEL_POWERMODE_RESET_CODE       (0x08)
#define PANEL_POWERMODE_DISPLAYON_CODE   (0x0c)
        int recover_count_on_failure = PANEL_RECOVERY_RETRY_MAX;
        u8 powermode = 0;

        iris_pre_lightup(panel);

        do {
                //powermode = iris_mipi_power_mode_read(panel, DSI_CMD_SET_STATE_LP);
		if (0 != iris_check_powermode(panel, PANEL_POWERMODE_RESET_CODE, 3)) {
			powermode = iris_mipi_power_mode_read(panel, DSI_CMD_SET_STATE_LP);
                        pr_err("iris: Bef DSI_CMD_SET_ON powermode: %d(0x%x)", powermode, powermode);
                        iris_reset_failure_recover(panel);
                        iris_pre_lightup(panel);
                        powermode = iris_mipi_power_mode_read(panel, DSI_CMD_SET_STATE_LP);
                        pr_err("iris: Bef DSI_CMD_SET_ON powermode: %d(0x%x)", powermode, powermode);
                }

                rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_SET_ON);

                if (rc) {
                        pr_err("[%s] failed to send DSI_CMD_SET_ON cmds, rc=%d\n",
                                panel->name, rc);
                }

                powermode = iris_mipi_power_mode_read(panel, DSI_CMD_SET_STATE_LP);
		if (powermode == 0xff)
			powermode = 0;
                if ((powermode&(u8)PANEL_POWERMODE_DISPLAYON_CODE) != PANEL_POWERMODE_DISPLAYON_CODE
			|| irisIsSimulatedPanelInitFailureRecover()) {
                        recover_count_on_failure --;
                        pr_err("iris: Aft DSI_CMD_SET_ON powermode: [0x%x]  %d", powermode, recover_count_on_failure);
                        if (recover_count_on_failure > 0) {
                                dsi_panel_reset(panel);
                                usleep_range(20000, 20010);
                                continue;
                        }
                }
                break;
        } while (recover_count_on_failure > 0);

        iris_lightup(panel);

	return rc;
}

int iris_dsi_gpio_free(struct dsi_panel *panel)
{
	int rc = 0;

#if defined(CONFIG_IRIS2P_FULL_SUPPORT)
#ifdef IRIS_VDD_GPIO_CONN
	if (gpio_is_valid(panel->iris_hw_cfg.iris_1v1))
		gpio_free(panel->iris_hw_cfg.iris_1v1);
#endif
#ifdef IRIS_RESET_GPIO_CONN
	if (gpio_is_valid(panel->iris_hw_cfg.iris_reset_gpio))
		gpio_free(panel->iris_hw_cfg.iris_reset_gpio);
#endif
#ifdef IRIS_WAKEUP_GPIO_CONN
	if (gpio_is_valid(panel->iris_hw_cfg.iris_wakeup))
		gpio_free(panel->iris_hw_cfg.iris_wakeup);
#endif
#endif

	return rc;
}
#endif
