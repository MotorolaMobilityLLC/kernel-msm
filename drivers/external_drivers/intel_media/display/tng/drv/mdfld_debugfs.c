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

#include <linux/seq_file.h>
#include <linux/debugfs.h>

#include <drm/drmP.h>
#include "psb_drv.h"

#if defined(CONFIG_DEBUG_FS)

static int mdfld_dc_dpll_regs_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	int i;

	seq_printf(m, "DISPLAY DPLL\n");

	seq_printf(m, "\tDPLL:\n");
	for (i=0xf000; i<0xffff; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	return 0;
}

static int mdfld_dc_pipeline_a_regs_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	int i;

	seq_printf(m, "DISPLAY PIPELINE A\n");

	seq_printf(m, "\tPALETTE A/B/C:\n");
	seq_printf(m, "\t\t reg(0xa000) = 0x%x\n", REG_READ(0xa000));

	seq_printf(m, "\tMIPI A:\n");
	for (i=0xb000; i<0xb0ff; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	seq_printf(m, "\tDSI ADAPTER:\n");
	for (i=0xb104; i<=0xb138; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	seq_printf(m, "\tDPLL:\n");
	for (i=0xf000; i<0xffff; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	seq_printf(m, "\tPIPELINE A:\n");
	for (i=0x60000; i<0x600ff; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	seq_printf(m, "\tPORT CONTROL:\n");
	for (i=0x61190; i<0x61194; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	seq_printf(m, "\tPIPELINE A CONTROL:\n");
	for (i=0x70000; i<0x700ff; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	return 0;
}

static int mdfld_dc_pipeline_b_regs_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	int i;

	seq_printf(m, "DISPLAY PIPELINE B\n");

	seq_printf(m, "\tPALETTE B:\n");
	seq_printf(m, "\t\t reg(0xa800) = 0x%x\n", REG_READ(0xa800));

	seq_printf(m, "\tPIPELINE B:\n");
	for (i=0x61000; i<0x610ff; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	seq_printf(m, "\tHDMI PORT CONTROL/HDCP/TV:\n");
	for (i=0x61110; i<=0x61178; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	seq_printf(m, "\tPANEL FITTING:\n");
	for (i=0x61200; i<0x612ff; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	seq_printf(m, "\tPIPELINE B CONTROL:\n");
	for (i=0x71000; i<0x710ff; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	return 0;
}

static int mdfld_dc_pipeline_c_regs_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	int i;

	seq_printf(m, "DISPLAY PIPELINE C\n");
	seq_printf(m, "\tPALETTE C:\n");
	seq_printf(m, "\t\t reg(0xac00) = 0x%x\n", REG_READ(0xac00));

	seq_printf(m, "\tMIPI C:\n");
	for (i=0xb800; i<0xb8ff; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	seq_printf(m, "\tDSI ADAPTER:\n");
	for (i=0xb904; i<=0xb938; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	seq_printf(m, "\tPIPELINE C:\n");
	for (i=0x62000; i<0x620ff; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	seq_printf(m, "\tPORT CONTROL:\n");
	for (i=0x62190; i<0x62194; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	seq_printf(m, "\tPIPELINE C CONTROL:\n");
	for (i=0x72000; i<0x720ff; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	return 0;
}

static int mdfld_dc_overlay_a_regs_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	int i;

	seq_printf(m, "DISPLAY OVERLAY A\n");
	seq_printf(m, "\tOVERLAY A:\n");
	for (i=0x30000; i<0x34023; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	return 0;
}

static int mdfld_dc_overlay_c_regs_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	int i;

	seq_printf(m, "DISPLAY OVERLAY C\n");
	seq_printf(m, "\tOVERLAY C:\n");
	for (i=0x38000; i<0x3c023; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	return 0;
}

static int dc_sprite_regs_info(struct seq_file *m, void *data, int index)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	char c;
	u32 reg_offset;
	int i;

	switch (index) {
	case 0:
		c = 'A';
		reg_offset = 0;
		break;
	case 1:
		c = 'B';
		reg_offset = 0x1000;
		break;
	case 2:
		c = 'C';
		reg_offset = 0x2000;
		break;
	case 3:
		c = 'D';
		reg_offset = 0x3000;
		break;
	case 4:
		c = 'E';
		reg_offset = 0x4000;
		break;
	case 5:
		c = 'F';
		reg_offset = 0x5000;
		break;
	default:
		return -EINVAL;
	}

	seq_printf(m, "DISPLAY SPRITE %c\n", c);
	for (i=0x70180 + reg_offset; i<0x701d4+reg_offset; i+=4)
		seq_printf(m, "\t\t reg(0x%x) = 0x%x\n", i, REG_READ(i));

	return 0;
}

static int mdfld_dc_sprite_a_regs_info(struct seq_file *m, void *data)
{
	return dc_sprite_regs_info(m, data, 0);
}

static int mdfld_dc_sprite_b_regs_info(struct seq_file *m, void *data)
{
	return dc_sprite_regs_info(m, data, 1);
}

static int mdfld_dc_sprite_c_regs_info(struct seq_file *m, void *data)
{
	return dc_sprite_regs_info(m, data, 2);
}

static int mdfld_dc_sprite_d_regs_info(struct seq_file *m, void *data)
{
	return dc_sprite_regs_info(m, data, 3);
}

static int mdfld_dc_sprite_e_regs_info(struct seq_file *m, void *data)
{
	return dc_sprite_regs_info(m, data, 4);
}

static int mdfld_dc_sprite_f_regs_info(struct seq_file *m, void *data)
{
	return dc_sprite_regs_info(m, data, 5);
}

static struct drm_info_list mdfld_debugfs_list[] = {
	{"mdfld_dc_dpll_regs", mdfld_dc_dpll_regs_info, 0},
	{"mdfld_dc_pipeline_a_regs", mdfld_dc_pipeline_a_regs_info, 0},
	{"mdfld_dc_pipeline_b_regs", mdfld_dc_pipeline_b_regs_info, 0},
	{"mdfld_dc_pipeline_c_regs", mdfld_dc_pipeline_c_regs_info, 0},
	{"mdfld_dc_overlay_a_regs", mdfld_dc_overlay_a_regs_info, 0},
	{"mdfld_dc_overlay_c_regs", mdfld_dc_overlay_c_regs_info, 0},
	{"dc_sprite_a_regs", mdfld_dc_sprite_a_regs_info, 0},
	{"dc_sprite_b_regs", mdfld_dc_sprite_b_regs_info, 0},
	{"dc_sprite_c_regs", mdfld_dc_sprite_c_regs_info, 0},
	{"dc_sprite_d_regs", mdfld_dc_sprite_d_regs_info, 0},
	{"dc_sprite_e_regs", mdfld_dc_sprite_e_regs_info, 0},
	{"dc_sprite_f_regs", mdfld_dc_sprite_f_regs_info, 0},
};
#define MDFLD_DEBUGFS_ENTRIES ARRAY_SIZE(mdfld_debugfs_list)

int mdfld_debugfs_init(struct drm_minor *minor)
{
	return drm_debugfs_create_files(mdfld_debugfs_list,
				MDFLD_DEBUGFS_ENTRIES,
				minor->debugfs_root, minor);

}

void mdfld_debugfs_cleanup(struct drm_minor *minor)
{
	drm_debugfs_remove_files(mdfld_debugfs_list,
			MDFLD_DEBUGFS_ENTRIES,
			minor);
}

#endif /*CONFIG_DEBUG_FS*/
