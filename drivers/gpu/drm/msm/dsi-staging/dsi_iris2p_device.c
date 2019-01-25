/*Pixelworks iris2p Driver
 *
 * Copyright (C) 2016-2018 Pixelworks Inc.
 * License terms: GU General ublic License (GL) version 2
 *
 * Copyright (c) 2014 - 2018, The Linux Foundation. All rights reserved.
*/

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/ctype.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/ioctl.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <asm/uaccess.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>

#include "dsi_iris2p_def.h"

#ifdef IRIS_CHECK_REG
#define IRIS_CHECK_TIMES 2000
#endif

enum {
	NO_MCF,
	ACTIVE_MCF,
	DEFAULT_MCF,
};

struct iris2p_data{
	struct mutex irislock;
	int pt_factory_poweron;
#ifdef IRIS_CHECK_REG
	struct delayed_work iris_check_work;
#endif
	int mcf_value;
	int esd_cnt;
	int esd_check_value;
	int esd_enable;
};

static struct iris2p_data *iris_p;

enum {
	AUTO_ARG_TYPE_STR,
	AUTO_ARG_TYPE_INT32,
};

struct auto_arg {
	int type;
	union {
		int32_t m_int32;
		const char *m_str;
	};
};

struct auto_args {
	int argc;
	struct auto_arg *argv;
};

int parse_arg(const char *s, struct auto_arg *arg)
{
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		long v;
		v = simple_strtol(s, NULL, 16);
		arg->type = AUTO_ARG_TYPE_INT32;
		arg->m_int32 = v;
	} else if (isdigit(s[0])) {
		long v;
		v = simple_strtol(s, NULL, 10);
		arg->type = AUTO_ARG_TYPE_INT32;
		arg->m_int32 = v;
	} else {
		arg->type = AUTO_ARG_TYPE_STR;
		arg->m_str = s;
	}

	return 0;
}

int parse_auto_args(char *s, struct auto_args *args)
{
	int i = 0;
	char c = 0;
	int last_is_arg_flag = 0;
	const char *last_arg;

	args->argc = 0;

	i = -1;
	do {
		c = s[++i];
		if (c == ' ' || c == ',' || c == '\n' || c == '\r' || c == 0) {
			if (last_is_arg_flag) {
				args->argc++;
			}
			last_is_arg_flag = 0;
		} else {
			last_is_arg_flag = 1;
		}
	} while (c != 0 && c != '\n' && c != '\r');

	args->argv =
	    (struct auto_arg *)kmalloc(args->argc * sizeof(struct auto_arg),
				       GFP_KERNEL);

	i = -1;
	last_is_arg_flag = 0;
	last_arg = s;
	args->argc = 0;
	do {
		c = s[++i];
		if (c == ' ' || c == ',' || c == '\n' || c == '\r' || c == 0) {
			if (last_is_arg_flag) {
				parse_arg(last_arg, args->argv + args->argc++);
				s[i] = 0;
			}
			last_is_arg_flag = 0;
		} else {
			if (last_is_arg_flag == 0) {
				last_arg = s + i;
			}
			last_is_arg_flag = 1;
		}
	} while (c != 0 && c != '\n' && c != '\r');

	return c == 0 ? i : i + 1;
}

void free_auto_args(struct auto_args *args)
{
	kfree(args->argv);
	args->argc = 0;
}

/*
 * factory pt test
 * enable iris power: adb echo pt 1 > /dev/iris2p
 * disable iris power: adb echo pt 0 > /dev/iris2p
 *
 */
int do_cmd_pt_power(struct iris2p_data *pdata,const struct auto_args *args)
{
	int value = 0;
	int rc = 0;

	pr_err("enter:%s\n",__func__);

	if (args->argc != 2 && args->argv[1].type != AUTO_ARG_TYPE_INT32) {
	        pr_err( "Miss or unknown args!");
	        rc = -1;
	        return rc;
	}
	if (dsi_panel_initialized(iris_panel))
		return -EINVAL;

	mutex_lock(&iris_p->irislock);
	value = args->argv[1].m_int32;
	pr_err("%s:enable = %d\n", __func__, value);
	rc = dsi_iris_pt_power(iris_panel, value);
	if (rc) {
		pr_err("fail dsi_iris_pt_power %d\n", value);
		goto exit;
	}
	if (value) {
		iris_p->pt_factory_poweron = 1;
	} else {
		iris_p->pt_factory_poweron = 0;
	}

exit:
	mutex_unlock(&iris_p->irislock);

	return rc;
}
int do_cmd_test_opt(struct iris2p_data *pdata,const struct auto_args *args)
{
	int value = 0;
	int delay = 0;
	int rc = 0;

	pr_err("enter:%s\n",__func__);

	if (args->argc != 2 && args->argv[1].type != AUTO_ARG_TYPE_INT32) {
	        pr_err( "Miss or unknown args!");
	        rc = -1;
	        return rc;
	} //if (dsi_panel_initialized(iris_panel))return -EINVAL;

	mutex_lock(&iris_p->irislock);
	value = args->argv[1].m_int32;
	delay = args->argv[2].m_int32;
	pr_err("%s:round = %d, delay = %d\n", __func__, value, delay);

	irisTest(value, delay);

	mutex_unlock(&iris_p->irislock);

	return rc;
}

/*
 * factory bp test
 * enable iris bp mode: adb shell "echo bp 1 >/dev/iris2p"
 * enable iris bp mode with hbm enabled: adb shell "echo bp 2 >/dev/iris2p"
 * disable iris bp mode: adb shell "echo bp 0 >/dev/iris2p"
 *
 */

int do_cmd_bp_opt(struct iris2p_data *pdata,const struct auto_args *args)
{
	int value = 0;
	int rc = 0;

	pr_err("enter:%s\n",__func__);

	if (args->argc != 2 && args->argv[1].type != AUTO_ARG_TYPE_INT32) {
	        pr_err( "Miss or unknown args!");
	        rc = -1;
	        return rc;
	}

	mutex_lock(&iris_p->irislock);
	value = args->argv[1].m_int32;
	pr_err("%s:enable = %d\n", __func__, value);

	if (value == 1)
		value = IRIS_IN_ABYPASS;
	else if (value == 2)
		value = IRIS_IN_ABYPASS_HBM_ENABLED;
	else if (value == 4)
		value = IRIS_IN_ONE_WIRED_ABYPASS;
	else
		value = IRIS_NO_ABYPASS;

	irisSetAbypassMode(value);

	if (value) {
		pr_err("%s:enabling iris abypass mode\n", __func__);
	} else {
		pr_err("%s:disabling iris abypass mode\n", __func__);
	}

	mutex_unlock(&iris_p->irislock);

	return rc;
}

/*
* enable hbm mode: adb shell "echo hbm 1 >/dev/iris2p"
* disable hbm mode: adb shell "echo hbm 0 >/dev/iris2p"
*
*/

int do_cmd_hbm_opt(struct iris2p_data *pdata,const struct auto_args *args)
{
	int value = 0;
	int rc = 0;
	int enableHbm = 0;

	pr_err("enter:%s\n",__func__);

	if (args->argc != 2 && args->argv[1].type != AUTO_ARG_TYPE_INT32) {
	        pr_err( "Miss or unknown args!");
	        rc = -1;
	        return rc;
	}

	mutex_lock(&iris_p->irislock);
	value = args->argv[1].m_int32;
	pr_err("%s:enable = %d\n", __func__, value);

	if (value > 0)
		enableHbm = 1;
	else
		enableHbm = 0;

	iris_hbm_direct_switch(iris_panel, enableHbm);

	mutex_unlock(&iris_p->irislock);

	return rc;
}

/*
* enable iris trace printout: adb shell "echo trace 16/32 >/dev/iris2p"
* disable iris trace printout: adb shell "echo trace 0 >/dev/iris2p"
*
*/

int do_cmd_trace_opt(struct iris2p_data *pdata,const struct auto_args *args)
{
	int value = 0;
	int rc = 0;

	pr_err("enter:%s\n",__func__);

	if (args->argc != 2 && args->argv[1].type != AUTO_ARG_TYPE_INT32) {
	        pr_err( "Miss or unknown args!");
	        rc = -1;
	        return rc;
	}

	mutex_lock(&iris_p->irislock);
	value = args->argv[1].m_int32;
	pr_err("%s:enable = %d\n", __func__, value);

	if (value < 0)
		value = 0;

	irisSetTracePrintOpt(value);

	mutex_unlock(&iris_p->irislock);

	return rc;
}

int do_cmd_recover_opt(struct iris2p_data *pdata,const struct auto_args *args)
{
	int value = 0;
	int rc = 0;

	pr_err("enter:%s\n",__func__);

	if (args->argc != 2 && args->argv[1].type != AUTO_ARG_TYPE_INT32) {
	        pr_err( "Miss or unknown args!");
	        rc = -1;
	        return rc;
	}

	mutex_lock(&iris_p->irislock);
	value = args->argv[1].m_int32;
	pr_err("%s:recover option = %d\n", __func__, value);

	if (value < 0)
		value = 0;

	irisSetRecoverOption(value);

	mutex_unlock(&iris_p->irislock);

	return rc;
}

int do_cmd_te_opt(struct iris2p_data *pdata,const struct auto_args *args)
{
	int value = 0;
	int rc = 0;

	pr_err("enter:%s\n",__func__);

	if (args->argc != 2 && args->argv[1].type != AUTO_ARG_TYPE_INT32) {
	        pr_err( "Miss or unknown args!");
	        rc = -1;
	        return rc;
	}

	mutex_lock(&iris_p->irislock);
	value = args->argv[1].m_int32;
	pr_err("%s:TE tweak enable = %d\n", __func__, value);

	if (value > 0)
		value = 1;
	else
		value = 0;

	irisSetTeTweakOpt(value);

	mutex_unlock(&iris_p->irislock);

	return rc;
}

int do_cmd_dump_opt(struct iris2p_data *pdata,const struct auto_args *args)
{
	int value = 0;
	int rc = 0;

	pr_err("enter:%s\n",__func__);

	if (args->argc != 2 && args->argv[1].type != AUTO_ARG_TYPE_INT32) {
	        pr_err( "Miss or unknown args!");
	        rc = -1;
	        return rc;
	}

	mutex_lock(&iris_p->irislock);
	value = args->argv[1].m_int32;
	pr_err("%s:dsi packets dump enable = %d\n", __func__, value);

	if (value > 0)
		value = 1;
	else
		value = 0;

	irisSetDumpOpt(value);

	mutex_unlock(&iris_p->irislock);

	return rc;
}

int do_cmd_memc(struct iris2p_data *pdata,const struct auto_args *args)
{
	int data = 0;
	u32 mode;
	int rc = -1;
	struct iris_mode_state *mode_state = &iris_info.mode_state;

	pr_err("enter:%s\n", __func__);

	if (args->argc != 2 && args->argv[1].type != AUTO_ARG_TYPE_INT32) {
		pr_err( "Miss or unknown args!");
		return rc;
	}
	data = args->argv[1].m_int32;
	pr_err("enter:%s data = %d\n", __func__, data);
	mode = data & 0xffff;
	mode_state->sf_mode_switch_trigger = true;
	mode_state->sf_notify_mode = mode;
	if (mode == IRIS_MODE_FRC_PREPARE)
		iris_video_fps_get(data >> 16);

	return 0;
}

int do_cmd_modeswitch_opt(struct iris2p_data *pdata,const struct auto_args *args)
{
	int data = 0;
	u32 mode;
	int rc = -1;
	struct iris_mode_state *mode_state = &iris_info.mode_state;

	pr_err("enter:%s\n", __func__);

	if (args->argc != 2 && args->argv[1].type != AUTO_ARG_TYPE_INT32) {
		pr_err( "Miss or unknown args!");
		return rc;
	}
	data = args->argv[1].m_int32;
	pr_err("enter:%s data = %d\n", __func__, data);
	mode = data;
	mode_state->sf_mode_switch_trigger = true;
	mode_state->sf_notify_mode = mode;

	return 0;
}

int do_cmd_ptbypassswitch_opt(struct iris2p_data *pdata,const struct auto_args *args)
{
	int data = 0;
	int rc = -1;

	pr_err("enter:%s\n", __func__);

	if (args->argc != 2 && args->argv[1].type != AUTO_ARG_TYPE_INT32) {
		pr_err( "Miss or unknown args!");
		return rc;
	}
	data = args->argv[1].m_int32;
	pr_err("enter:%s data = %d\n", __func__, data);
	rc = iris_abypass_switch_write(data);

	return 0;
}

int do_cmd_power_opt(struct iris2p_data *pdata,const struct auto_args *args)
{
	int data = 0;
	int ret = -1;

	//pr_err("enter:%s\n",__func__);

	if (args->argc != 2 && args->argv[1].type != AUTO_ARG_TYPE_INT32) {
		pr_err( "Mis or unknown args!");
		return ret;
	}
	data = args->argv[1].m_int32;
	pr_err("enter:%s data = %d\n",__func__, data);
	iris_debug_power_opt_disable = data;

	return 0;
}


int do_cmd_ui(struct iris2p_data *pdata,const struct auto_args *args)
{
	int type = 0, value = 0, rc = -1;

	pr_err("enter:%s\n",__func__);

	if (args->argc != 2 && args->argv[1].type != AUTO_ARG_TYPE_INT32) {
		pr_err( "Miss or unknown args!");
		return rc;
	}
	type = args->argv[1].m_int32;
	value = args->argv[2].m_int32;
	pr_err("enter:%s enable = %d %d\n",__func__, type, value);

	iris_configure(type, value);

	return 0;
}

int do_cmd(struct iris2p_data *pdata, const struct auto_args *args)
{
	const char *s;
	int ret = 0;
	if (args->argv->type != AUTO_ARG_TYPE_STR)
		return 0;

	s = args->argv->m_str;
	pr_err("%s:%s  v1 = 0x%x   v2 =0x%x\n",__func__,s,args->argv[1].m_int32,args->argv[2].m_int32);
	//echo memc > /dev/iris2p
	if (!strcmp(s, "memc"))
		return do_cmd_memc(pdata, args);
	if (!strcmp(s, "ui"))
		return do_cmd_ui(pdata, args);
	if (!strcmp(s, "pt"))
		return do_cmd_pt_power(pdata, args);
	if (!strcmp(s, "power"))
		return do_cmd_power_opt(pdata, args);
	if (!strcmp(s, "bp"))
		return do_cmd_bp_opt(pdata, args);
	if (!strcmp(s, "hbm"))
		return do_cmd_hbm_opt(pdata, args);
	if (!strcmp(s, "trace"))
		return do_cmd_trace_opt(pdata, args);
	if (!strcmp(s, "te"))
		return do_cmd_te_opt(pdata, args);
	if (!strcmp(s, "dump"))
		return do_cmd_dump_opt(pdata, args);
	if (!strcmp(s, "ms"))
		return do_cmd_modeswitch_opt(pdata, args);
	if (!strcmp(s, "ptbypass"))
		return do_cmd_ptbypassswitch_opt(pdata, args);
	if (!strcmp(s, "test"))
		return do_cmd_test_opt(pdata, args);
	if (!strcmp(s, "recover"))
		return do_cmd_recover_opt(pdata, args);

	return ret;
}

static long iris2p_ioctl(struct file *file,
					   unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *ubuf = (void __user *)arg;
//	pr_info("%s,  cmd=%d, ubuf = %p\n", __func__, _IOC_NR(cmd), ubuf);

	switch(cmd) {
		case MSMFB_IRIS_OPERATE_CONF:
			ret = iris_ioctl_operate_conf(ubuf);
			break;
		case MSMFB_IRIS_OPERATE_MODE:
			ret = iris_ioctl_operate_mode(ubuf);
			break;
		case MSMFB_IRIS_OPERATE_TOOL:
			//ret = msmfb_iris_operate_tool(mfd, argp);
			break;
		default:
			pr_info("invalide cmd %d\n", cmd);
			break;
	}
	return ret;
}

static int iris2p_open(struct inode *inode, struct file *file)
{
	file->private_data = (void *)iris_p;

	return 0;
}

static int iris2p_release(struct inode *ip, struct file *fp)
{
	return 0;
}

static ssize_t iris2p_write(struct file *file,
							const char __user * user_buf,
							size_t count,
							loff_t * ppos)
{
	char *buf;
	struct auto_args args;
	int i;
	struct iris2p_data *iris2p = file->private_data;

	buf = (char *)kmalloc(count + 1, GFP_KERNEL);
	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;
	buf[count] = 0;

	i = 0;
	while (buf[i] != 0) {
		i += parse_auto_args(buf + i, &args);
		if (args.argc == 0)
			continue;
		do_cmd(iris2p, &args);
		free_auto_args(&args);
	}
	kfree(buf);

	return count;
}
 char rel_buf[2048];
static ssize_t iris2p_func_status_show(struct device *dev, struct device_attribute *attr,
											char *buf)
{
	ssize_t len = 0;
	struct feature_setting *chip_setting = &iris_info.chip_setting;
	struct iris_pq_setting *pq_setting = &chip_setting->pq_setting;
	struct iris_dbc_setting *dbc_setting = &chip_setting->dbc_setting;
	struct iris_memc_setting *lp_memc_setting = &chip_setting->lp_memc_setting;
	struct iris_lce_setting *lce_setting = &chip_setting->lce_setting;
	struct iris_cm_setting *cm_setting = &chip_setting->cm_setting;
	struct iris_hdr_setting *hdr_setting = &chip_setting->hdr_setting;
	struct iris_lux_value *lux_value = &chip_setting->lux_value;
	struct iris_cct_value *cct_value = &chip_setting->cct_value;
	struct iris_reading_mode *reading_mode = &chip_setting->reading_mode;
	struct iris_sdr_setting *sdr_setting = &chip_setting->sdr_setting;
	struct iris_color_adjust *color_adjust = &chip_setting->color_adjust;
	struct iris_oled_brightness *oled_brightness = &chip_setting->oled_brightness;

	memset(rel_buf, 0, sizeof(rel_buf));
	sprintf(rel_buf,
			"iris2p function parameter info\n"
			"***pq_setting***\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n",
			"peaking", pq_setting->peaking,
			"sharpness", pq_setting->sharpness,
			"memcdemo", pq_setting->memcdemo,
			"gamma", pq_setting->gamma,
			"memclevel", pq_setting->memclevel,
			"contrast", pq_setting->contrast,
			"cinema_en", pq_setting->cinema_en,
			"peakingdemo", pq_setting->peakingdemo);
	len = strlen(rel_buf);
	sprintf(rel_buf+len,
			"***dbc_setting***\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n",
			"brightness", dbc_setting->brightness,
			"ext_pwm", dbc_setting->ext_pwm,
			"cabcmode", dbc_setting->cabcmode,
			"dlv_sensitivity", dbc_setting->dlv_sensitivity);
	len = strlen(rel_buf);
	sprintf(rel_buf+len,
			"***lp_memc_setting***\n"
			"%-20s:\t%d\n"
			"%-20s:\t0x%x\n",
			"level", lp_memc_setting->level,
			"value", lp_memc_setting->value);
	len = strlen(rel_buf);
	sprintf(rel_buf+len,
			"***lce_setting***\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n",
			"mode", lce_setting->mode,
			"mode1level", lce_setting->mode1level,
			"mode2level", lce_setting->mode2level,
			"demomode", lce_setting->demomode,
			"graphics_detection", lce_setting->graphics_detection);
	len = strlen(rel_buf);
	sprintf(rel_buf+len,
			"***cm_setting***\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n",
			"cm6axes", cm_setting->cm6axes,
			"cm3d", cm_setting->cm3d,
			"demomode", cm_setting->demomode,
			"ftc_en", cm_setting->ftc_en,
			"color_temp_en", cm_setting->color_temp_en,
			"color_temp", cm_setting->color_temp,
			"color_temp_adjust", cm_setting->color_temp_adjust,
			"sensor_auto_en", cm_setting->sensor_auto_en);
	len = strlen(rel_buf);
	sprintf(rel_buf+len,
			"***hdr_setting***\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n",
			"hdrlevel", hdr_setting->hdrlevel,
			"hdren", hdr_setting->hdren);
	len = strlen(rel_buf);
	sprintf(rel_buf+len,
			"***lux_value***\n"
			"%-20s:\t%d\n",
			"luxvalue", lux_value->luxvalue);
	len = strlen(rel_buf);
	sprintf(rel_buf+len,
			"***cct_value***\n"
			"%-20s:\t%d\n",
			"cctvalue", cct_value->cctvalue);
	len = strlen(rel_buf);
	sprintf(rel_buf+len,
			"***reading_mode***\n"
			"%-20s:\t%d\n",
			"readingmode",reading_mode->readingmode);
	if (1 == sdr_setting->sdr2hdr) {
		len = strlen(rel_buf);
		sprintf(rel_buf+len,
				"***sdr2hdr_mode***\n"
				"%-20s:\tGamehdr\n",
				"sdr2hdrmode");
	} else if (2 == sdr_setting->sdr2hdr) {
		len = strlen(rel_buf);
		sprintf(rel_buf+len,
				"***sdr2hdr_mode***\n"
				"%-20s:\tVideohdr\n",
				"sdr2hdrmode");
	} else {
		len = strlen(rel_buf);
		sprintf(rel_buf+len,
				"***sdr2hdr_mode***\n"
				"%-20s:\toff\n",
				"sdr2hdrmode");
	}

	len = strlen(rel_buf);
	sprintf(rel_buf+len,
			"***color_adjust***\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n"
			"%-20s:\t%d\n",
			"saturation", color_adjust->saturation,
			"hue", color_adjust->hue,
			"Contrast", color_adjust->Contrast);

	len = strlen(rel_buf);
	sprintf(rel_buf+len,
			"***oled_brightness***\n"
			"%-20s:\t%d\n",
			"brightness",oled_brightness->brightness);

	return sprintf(buf, "%s\n", rel_buf);
}
static ssize_t memc_status_show(struct device *dev, struct device_attribute *attr,
									char *buf)
{
	char name[20];
	struct iris_mode_state *mode_state = &iris_info.mode_state;
	int current_mode = mode_state->current_mode;

	if (NULL == mode_state) {
		pr_err("mode_state is NULL");
		return -1;
	}
	memset(name, 0, sizeof(name));
	switch (current_mode)
	{
		case IRIS_PT_MODE:
			strcpy(name, "PT");
			break;
		case IRIS_RFB_MODE:
			strcpy(name, "RFB");
			break;
		case IRIS_FRC_MODE:
			strcpy(name, "on");
			break;
		case IRIS_BYPASS_MODE:
			strcpy(name, "BYPASS");
			break;
		case IRIS_PT_PRE:
			strcpy(name, "PT_PRE");
			break;
		case IRIS_RFB_PRE:
			strcpy(name, "RFB_PRE");
			break;
		case IRIS_FRC_PRE:
			strcpy(name, "FRC_PRE");
			break;
		default:
			strcpy(name, "off");
			break;
	}

	return sprintf(buf, "%s\n", name);
}

static ssize_t iris2p_pt_power_show(struct device *dev, struct device_attribute *attr,
										char *buf)
{
	char name[2];

	memset(name, 0, sizeof(name));
	if (iris_p->pt_factory_poweron) {
		strcpy(name, "1");
	} else {
		strcpy(name, "0");
	}

	return sprintf(buf, "%s\n", name);
}

static ssize_t iris2p_pt_power_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t n)
{
	int ret = 0;
	int32_t enable = 0;

	mutex_lock(&iris_p->irislock);
	ret = sscanf(buf, "%d", &enable);
	if (ret < 1) {
		pr_err("invalid value %d\n",enable);
		goto exit;
	}
	if (dsi_panel_initialized(iris_panel))
		goto exit;

	ret = dsi_iris_pt_power(iris_panel, enable);
	if (ret) {
		pr_err("fail dsi_iris_pt_power %d\n", enable);
		goto exit;
	}
	if (enable) {
		iris_p->pt_factory_poweron = 1;
	} else {
		iris_p->pt_factory_poweron = 0;
	}

exit:
	mutex_unlock(&iris_p->irislock);

	return n;
}

static ssize_t iris2p_force_func_disable_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	char name[2];

	memset(name, 0, sizeof(name));
	if (iris_info.firmware_info.app_fw_state) {
		strlcpy(name, "0", sizeof(name));
	} else {
		strlcpy(name, "1", sizeof(name));
	}

	return snprintf(buf, 2, "%s\n", name);
}

static ssize_t iris2p_force_func_disable_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t n)
{
	int ret = 0;
	int32_t enable = 0;

	mutex_lock(&iris_p->irislock);
	ret = sscanf(buf, "%d", &enable);
	if (ret < 1) {
		pr_err("invalid value %d\n", enable);
		goto exit;
	}

	if (enable) {
		iris_debug_fw_download_disable = 1;
	} else {
		iris_debug_fw_download_disable = 0;
	}

exit:
	mutex_unlock(&iris_p->irislock);

	return n;
}

static ssize_t iris2p_firmware_show(struct device *dev, struct device_attribute *attr,
										char *buf)
{
	char name[20];

	memset(name, 0, sizeof(name));
	if (iris_info.lut_info.lut_fw_state && iris_info.gamma_info.gamma_fw_state) {
		if (iris_p->mcf_value == DEFAULT_MCF) {
			strlcpy(name, "DEFAULT_MCF", sizeof(name));
		} else if (iris_p->mcf_value == ACTIVE_MCF) {
			strlcpy(name, "ACTIVE_MCF", sizeof(name));
		}
	} else {
		strlcpy(name, "NO_MCF", sizeof(name));
	}

	return snprintf(buf, 20, "%s\n", name);
}

static ssize_t iris2p_firmware_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t n)
{
	int ret = 0;
	int32_t enable = 0;

	ret = sscanf(buf, "%d", &enable);
	if (ret < 1) {
		pr_err("invalid value %d\n",enable);
		goto exit;
	}
	/* enable fw */
	if (enable) {
		if (2 == enable) {
			iris_p->mcf_value = DEFAULT_MCF;
		} else {
			iris_p->mcf_value = ACTIVE_MCF;
		}
		trigger_download_ccf_fw();
	}

exit:

	return n;
}

static ssize_t iris2p_esd_cnt_show(struct device *dev, struct device_attribute *attr,
										char *buf)
{
	char name[10];

	memset(name, 0, sizeof(name));
	if (iris_p->esd_enable) {
		snprintf(name, 10, "%d\n", iris_p->esd_cnt);
	} else {
		strlcpy(name, "no used\n", sizeof(name));
	}

	return snprintf(buf, 10, "%s\n", name);
}

static ssize_t iris2p_esd_cnt_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t n)
{
	int ret = 0;
	int32_t value = -1;

	ret = sscanf(buf, "%d", &value);
	if (ret < 1) {
		pr_err("invalid value %d\n",value);
		goto exit;
	}
	if (0 == value) {
		iris_p->esd_enable = 0;
	} else if (1 == value) {
		iris_p->esd_enable = 1;
	}

exit:
	return n;
}

static DEVICE_ATTR(iris2p_force_func_disable, 0644,
	iris2p_force_func_disable_show, iris2p_force_func_disable_store);
static DEVICE_ATTR(iris2p_pt_power, 0664, iris2p_pt_power_show, iris2p_pt_power_store);
static DEVICE_ATTR(memc_status, 0664, memc_status_show, NULL);
static DEVICE_ATTR(iris2p_func_status, 0664, iris2p_func_status_show, NULL);
static DEVICE_ATTR(iris2p_firmware, 0664, iris2p_firmware_show, iris2p_firmware_store);
static DEVICE_ATTR(iris2p_esd_cnt, 0664, iris2p_esd_cnt_show, iris2p_esd_cnt_store);


static struct attribute *iris2p_attrs[] = {
	&dev_attr_memc_status.attr,
	&dev_attr_iris2p_func_status.attr,
	&dev_attr_iris2p_pt_power.attr,
	&dev_attr_iris2p_force_func_disable.attr,
	&dev_attr_iris2p_firmware.attr,
	&dev_attr_iris2p_esd_cnt.attr,
	NULL
};

static const struct attribute_group iris2p_attr_group = {
	.attrs = iris2p_attrs,
};

static const struct file_operations iris2p_fops = {
	.owner = THIS_MODULE,
	.open = iris2p_open,
	.unlocked_ioctl = iris2p_ioctl,
	.compat_ioctl = iris2p_ioctl,
	.release = iris2p_release,
	.write = iris2p_write,
};

struct miscdevice iris2p_misc;

int iris_work_enable(bool enable)
{
#ifdef IRIS_CHECK_REG
	if (enable) {
		schedule_delayed_work(&iris_p->iris_check_work, msecs_to_jiffies(IRIS_CHECK_TIMES));
	} else {
		cancel_delayed_work_sync(&iris_p->iris_check_work);
	}
#endif
	return 0;
}

#ifdef IRIS_CHECK_REG
static void iris_reg_check_func(struct work_struct *work)

{
#ifdef I2C_ENABLE
	int32_t val = 0;
	int32_t ret = 0;

	if (!dsi_panel_initialized(iris_panel)) {
		pr_info("panel no work,no to check iris\n");
		return;
	}

	ret = iris_i2c_reg_read(INT_STATUS, &val);
	if (val) {
		pr_info("iris INT_STATUS 0x%x\n", val);
	}
	schedule_delayed_work(&iris_p->iris_check_work, msecs_to_jiffies(IRIS_CHECK_TIMES));
#endif

}
#endif

u32 iris_register_list[] = {
	0xf0000000,
	0xf0000004,
	0xf0000008,
	0xf000000c,
	0xf0000218,
	0xf0000250,
	0xf0040000,
	0xf0040008,
	0xf0040010,
	0xf0040018,
	0xf0040020,
	0xf0040028,
	0xf0040030,
	0xf0040038,
	0xf0100000,
	0xf0100004,
	0xf0120004,
	0xf0212fd4,
	0xf0212fd8,
	0xf0212fdc,
	0xf0212fe0,
	0xf0212fe4,
	0xf0212fe8,
	0xf0212fec,
	0xf0212ff0,
	0xf0212ff4,
	0xf0212ff8,
	0xf0212ffc,
	0xf019ffe4,
	0xf123ffe4,
};

void iris_esd_register_dump(void)
{
	u32 value = 0;
	u32 index = 0;

	pr_err("iris esd register dump: \n");
	for (index = 0; index < sizeof(iris_register_list)/ 4; index++) {
		iris_i2c_reg_read(iris_register_list[index], &value);
		pr_err("%08x : %08x\n", iris_register_list[index], value);
	}
}


int notify_iris_esd_exit(void)
{

	iris_p->esd_check_value = 0x00;

	return 0;
}

int notify_iris_esd_cancel(void *display)
{
       notify_iris_esd_exit();

       return 0;
}

/*
* IRIS_STATUS reg:0xf0100000
* bit[4~7]:iris internal detect count
* run normal:alway increase;
* abnormal:no increase;
*/

int get_iris_status(void)
{
	int32_t ret = 1;
#ifdef I2C_ENABLE
	unsigned int data = 0;
	unsigned int run_status = 0x00;
	u8 lcdstatus = 0;
	u8 lcdsi = 0;
	int32_t val = 0;

	if (!iris_info.firmware_info.app_fw_state) {
		pr_info("fw no ready return esd");
		return ret;
	}

	if (iris_mode_bypass_state_check())
		return ret;

	ret = iris_i2c_reg_read(IRIS_RUN_STATUS, &data);
	if (ret < 1) {
		pr_err("fail to get iris status %d\n", ret);
		return 1;
	}

	iris_i2c_reg_read(IRIS_INT_STATUS, &val);
	if (0x800 != val) {
		pr_info("iris INT_STATUS 0x%x\n", val);
	}
	/* lcd 0a register */
	ret = iris_i2c_panel_state_read(0x00000a06, &lcdstatus);
	if (ret) {
		pr_err("fail to read lcd status\n");
	}
	/* lcd 05 register */
	ret = iris_i2c_panel_state_read(0x00000506, &lcdsi);
	if (ret) {
		pr_err("fail to read lcd status\n");
	}

	run_status = (data>>8)&0xff;
	pr_info("iris IRIS_RUN_STATUS 0x%x lcdstatus 0x%x lcdsi 0x%x\n",
		run_status, lcdstatus, lcdsi);
	if (lcdstatus == 0xff)
		lcdstatus = 0;
	if (ret || ((lcdstatus&0x0c) != 0x0c)
		|| ((run_status&0xf0) == iris_p->esd_check_value) || lcdsi) {
		pr_err("iris status err 0x%x, lcdstatus 0x%x", run_status, lcdstatus);
		iris_p->esd_check_value = 0x00;
		if (iris_p->esd_cnt < 10000) {
			iris_p->esd_cnt++;
		} else {
			iris_p->esd_cnt = 0;
		}
		if (iris_p->esd_enable) {
			pr_err("esd recovery\n");
			iris_esd_register_dump();
			ret = -1;
		}
	} else {
		iris_p->esd_check_value = run_status&0xf0;
		ret = 1;
	}
#endif

	return ret;
}

void iris2p_register_fs(void)
{
	int32_t rc;
	iris2p_misc.minor = MISC_DYNAMIC_MINOR;
	iris2p_misc.name = "iris2p";
	iris2p_misc.fops = &iris2p_fops;

	rc = misc_register(&iris2p_misc);
	if (rc < 0) {
		pr_err("Error: misc_register returned %d", rc);
	}
	rc = sysfs_create_group(&iris2p_misc.this_device->kobj,
				&iris2p_attr_group);
	if (rc) {
		pr_err("failed to register memc sysfs %d\n", rc);
	}
	iris_p = (struct iris2p_data *)kmalloc(sizeof(struct iris2p_data), GFP_KERNEL);
	memset(iris_p, 0, sizeof(struct iris2p_data));
	mutex_init(&iris_p->irislock);
#ifdef IRIS_CHECK_REG
	INIT_DELAYED_WORK(&iris_p->iris_check_work, iris_reg_check_func);
#endif
	iris_p->esd_enable = 1;
}

void iris2p_unregister_fs(void)
{
	if (NULL != iris_p)
		kfree(iris_p);
	iris_p = NULL;
}
