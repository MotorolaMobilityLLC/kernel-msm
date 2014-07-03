/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 * Jackie Li<yaodong.li@intel.com>
 */

#ifndef __MDFLD_DSI_DBI_DSR_H__
#define __MDFLD_DSI_DBI_DSR_H__

#include "mdfld_dsi_output.h"

enum {
	DSR_INIT = 0,
	DSR_EXITED,
	DSR_ENTERED_LEVEL0,
	DSR_ENTERED_LEVEL1,
};

/*protected by context_lock in dsi config*/
struct mdfld_dsi_dsr {
	/*dsr reference count*/
	int ref_count;

	int free_count;

	/*dsr enabled*/
	int dsr_enabled;
	/*dsr state*/
	int dsr_state;
	/*power off work*/
	struct work_struct power_off_work;
	/*power on work*/
	struct work_struct power_on_work;

	/*dsi config*/
	void *dsi_config;
};

int mdfld_dsi_dsr_update_panel_fb(struct mdfld_dsi_config *dsi_config);
int mdfld_dsi_dsr_report_te(struct mdfld_dsi_config *dsi_config);

int mdfld_dsi_dsr_forbid(struct mdfld_dsi_config *dsi_config);
int mdfld_dsi_dsr_allow(struct mdfld_dsi_config *dsi_config);
int mdfld_dsi_dsr_forbid_locked(struct mdfld_dsi_config *dsi_config);
int mdfld_dsi_dsr_allow_locked(struct mdfld_dsi_config *dsi_config);

int mdfld_dsi_dsr_init(struct mdfld_dsi_config *dsi_config);
void mdfld_dsi_dsr_destroy(struct mdfld_dsi_config *dsi_config);

void mdfld_dsi_dsr_enable(struct mdfld_dsi_config *dsi_config);

int mdfld_dsi_dsr_in_dsr_locked(struct mdfld_dsi_config *dsi_config);

/*FIXME: remove it later*/
extern void ospm_suspend_display(struct drm_device *dev);
extern void ospm_suspend_pci(struct pci_dev *pdev);

#endif /*__MDFLD_DSI_DBI_DSR_H__*/
