/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MDSS_HDMI_SLIMPORT_H__
#define __MDSS_HDMI_SLIMPORT_H__

#include <linux/platform_device.h>

struct msm_hdmi_slimport_ops {
	int (*set_upstream_hpd)(struct platform_device *pdev, uint8_t on);

};

int msm_hdmi_register_slimport(struct platform_device *pdev,
			  struct msm_hdmi_slimport_ops *ops, void *data);

/* TODO: hack for P0 HDMI HPD detection */
void hdmi_hpd_hack(int new_hpd_state);

#endif /* __MDSS_HDMI_SLIMPORT_H__ */
