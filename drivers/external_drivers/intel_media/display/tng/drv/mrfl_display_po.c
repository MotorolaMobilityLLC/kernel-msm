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
 */

/*
 * NOTE: this file is only for merrifield HDMI & JDI panel power on
 * TODO: remove me later.
 */
#include "mdfld_dsi_output.h"
#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_dpi.h"
#include "mdfld_output.h"
#include <asm/intel_scu_ipc.h>
#include "mdfld_dsi_pkg_sender.h"
#include "psb_drv.h"

static void __iomem *io_base;
void setiobase(uint8_t *value)
{
	io_base = value;
}
/*common functions*/
/*----------------------------------------------------------------------------*/
static void gunit_sb_write(u32 arg0, u32 arg1, u32 arg2, u32 arg3)
{
	u32 ret;
	int retry=0;
	u32 sb_pkt = (arg1 << 16) | (arg0 << 8) | 0xf0;

	/* write the register to side band register address */
	iowrite32(arg2, io_base + 0x2108);
	iowrite32(arg3, io_base + 0x2104);
	iowrite32(sb_pkt, io_base + 0x2100);

	ret = ioread32(io_base + 0x210c);
	while ((retry++ < 0x1000) && (ret != 0x2)) {
		msleep(1);
		ret = ioread32(io_base + 0x210c);
	}

	if (ret != 2)
		DRM_ERROR("%s:Failed to received SB interrupt\n", __func__);
}

static u32 gunit_sb_read(u32 arg0, u32 arg1, u32 arg2)
{
	u32 ret;
	int retry=0;
	u32 sb_pkt = arg1 << 16 | arg0 << 8 | 0xf0;

	/* write the register to side band register address */
	iowrite32(arg2, io_base + 0x2108);
	iowrite32(sb_pkt, io_base + 0x2100);

	ret = ioread32(io_base + 0x210c);
	while ((retry < 0x1000) && (ret != 2)) {
		msleep(1);
		ret = ioread32(io_base + 0x210c);
	}

	if (ret != 2)
		DRM_ERROR("%s: Failed to received SB interrupt\n", __func__);
	else
		ret = ioread32(io_base + 0x2104);

	return ret;
}

void power_off_pipe(u32 msg_port, u32 msg_reg, u32 val, u32 val_comp)
{
	u32 ret;
	int retry=0;

	ret = intel_mid_msgbus_read32(msg_port, msg_reg);

	intel_mid_msgbus_write32(msg_port, msg_reg, ret | val);

	ret = intel_mid_msgbus_read32(msg_port, msg_reg);
	if (((ret & val_comp) != val_comp) && (retry < 1000)) {
		retry++;
		ret = intel_mid_msgbus_read32(msg_port, msg_reg);
	}
}

void power_on_pipe(u32 msg_port, u32 msg_reg,
							u32 val_comp, u32 val_write)
{
	u32 ret;
	int retry=0;

	ret = intel_mid_msgbus_read32(msg_port, msg_reg);

	if ((ret & val_comp) == 0) {
		DRM_ERROR("%s: pipe is already powered on\n", __func__);
		return;
	} else {
		intel_mid_msgbus_write32(msg_port, msg_reg, ret & val_write);
		ret = intel_mid_msgbus_read32(msg_port, msg_reg);
		while ((retry < 1000) && ((ret & val_comp) != 0)) {
			msleep(1);
			ret = intel_mid_msgbus_read32(msg_port, msg_reg);
			retry++;
		}
		if ((ret & val_comp) != 0)
			DRM_ERROR("%s: powering on pipe failed\n", __func__);
		if (msg_port == 0x4 && msg_reg == 0x3b) {
			DRM_ERROR("%s: skip powering up MIO AFE\n", __func__);
		}
	}
}

static void pipe_timing(int pipe, u32 arg0, u32 arg1, u32 arg2,
		u32 arg3, u32 arg4, u32 arg5, u32 arg6, u32 arg7)
{
	/* Pipe A Horizontal Total Register */
	iowrite32(arg0, io_base + 0x60000 + pipe);

	/* Pipe A Horizontal Blank Register */
	iowrite32(arg1, io_base + 0x60004 + pipe);

	/* Pipe A Horizontal Sync Register */
	iowrite32(arg2, io_base + 0x60008 + pipe);

	/* Pipe A Vertical Total Register */
	iowrite32(arg3, io_base + 0x6000c + pipe);

	/* Pipe A Vertical Blank Register */
	iowrite32(arg4, io_base + 0x60010 + pipe);

	/* Pipe A Vertical Sync Register */
	iowrite32(arg5, io_base + 0x60014 + pipe);

	/* Pipe A Source image size Register */
	iowrite32(arg6, io_base + 0x6001c + pipe);

	/* Pipe A Vertical Shift Sync Register */
	iowrite32(arg7, io_base + 0x60028 + pipe);
}

/*HDMI power on sequence*/
/*----------------------------------------------------------------------------*/

#define VIDEO_DIP_CTL   0x61170
#define VIDEO_DIP_DATA  0x61178
#define VIDEO_DIP_FREQ_EVERY_FRAME 0x00010000

/* HDMI resolutions */
#define HD_640x480              1
#define HD_720x480              2
#define HD_1920x1080            3
#define HD_1920x1200            4

#define RESO6x4             (0)
#define RRESO13x7_mipi      (0x1)
#define RRESO10x7           (0x2)
#define RRESO10x7_mipi      (0x3)
#define RRESO19x10          (0x4)
#define RRESO19x10_mipi     (0x5)
#define RRESO19x12_mipi     (0x6)
#define RRESO16x12          (0x7)
#define RRESO16x12_mipi     (0x8)
#define RRESO8x4_mipi       (0x9)
#define RRESO8x6            (0xa)
#define RRESO8x6_new        (0xb)
#define RRESO8x6_2          (0xc)
#define RRESO12x10          (0xd)
#define RRESO12x7_mipi      (0xe)
#define RRESO12x10_75       (0xf)
#define RRESO25x16_mipi     (0x10)
#define RRESO7x12_mipi      (0x11)
#define RRESO7x4            (0x12)
#define RRESO8x12_mipi      (0x13)

static void power_off_all_pipes(void)
{
	power_off_pipe(0x4, 0x3C, 0x3, 0x3000000);
	power_off_pipe(0x4, 0x36, 0xC, 0xC000000);
	power_off_pipe(0x4, 0x36, 0x30, 0x3000000);
	//power_off_pipe(0x4, 0x3b, 0x3, 0x3000000);
	//power_off_pipe(0x4, 0x36, 0x3, 0x3000000);
}

static void power_on_all_pipes(void)
{
	//power_on_pipe(0x4, 0x36, 0x3000000, 0xFFFFFFFC); /* pipe A */
	//power_on_pipe(0x4, 0x3b, 0x3000000, 0xFFFFFFFC); /* MIO */
	power_on_pipe(0x4, 0x36, 0xc000000, 0xfffffff3); /* pipe B */
	power_on_pipe(0x4, 0x3c, 0x3000000, 0xfffffffc); /* HDMI */
}

void hdmi_dll_program(void)
{
	u32 ret, status;
	u32 arg3 = (0x11 << 24) | (0x1 << 11) | (2 << 8) |
		(116) | (3 << 21) | (2 << 16) | (1 << 12);
	int retry=0;

	/* Common reset */
        iowrite32(0x70006800, io_base + 0xF018);

	gunit_sb_write(0x13,0x1,0x800c, arg3);
	gunit_sb_write(0x13, 0x1,0x8048, 0x009F0051);
	gunit_sb_write(0x13, 0x1,0x8014, 0x0D714300);

	/* enable pll */
	iowrite32(0xf0006800, io_base + 0xf018);
	ret = ioread32(io_base + 0xf018);
	ret &= 0x8000;
	while ((retry++ < 1000) && (ret != 0x8000)) {
		msleep(1);
		ret = ioread32(io_base + 0xf018);
		ret &= 0x8000;
	}

	if (ret != 0x8000) {
		DRM_ERROR("%s: DPLL failed to lock, exit...\n", __func__);
		return;
	}

	/* Enabling firewall for modphy */
    gunit_sb_write(0x13,0x1,0x801c,0x01000000);
    status = gunit_sb_read(0x13,0x0,0x801c);

	/* Disabling global Rcomp */
	gunit_sb_write(0x13,0x1,0x80E0,0x8000);

	/* Stagger Programming */
	gunit_sb_write(0x13,0x1,0x0230,0x401F00);
	gunit_sb_write(0x13,0x1,0x0430,0x541F00);
}

static void hdmi_pipe_set_reso(int pipe, int reso)
{
	int i;

	if (reso == RRESO19x10) {
		pipe_timing(pipe, 0x0A0F077F, 0x0A0F077F, 0x08C707F7, 0x045D0437,
				0x045D0437, 0x043B0438, 0x077F0437, 0x0 );
	} else if (reso == RESO6x4) {
		pipe_timing(pipe, 0x31f027f, 0x31f027f, 0x2ef028f, 0x20c01df,
				0x20c01df, 0x1eb01e9, 0x27f01df, 0x0 );
	} else {
		DRM_ERROR("%s: only support 1920x1080 or 640x480, exit...\n", __func__);
	}

	i = 0;
	while (i <= 0x1c) {
		i += 4;
	}
}

static void hdmi_sprite_enable(struct pci_dev *pdev, int sprite, int size_h,
		int size_v, int pos_x, int pos_y, u32 base_addr)
{
	u32 bgsm, gtt_base_addr, surface_base_addr;
	u32 controlval, positionval, resolutionval;
	void __iomem *bgsm_virt;
	int pipe_src_w, pipe_src_h;
	int stride = size_h * 4;
	//int stride = size_h * 2; /* 16 bit */

	if (sprite != 0x1000) {
		DRM_ERROR("%s, unsupported sprite: 0x%x\n", __func__, sprite);
		return;
	}

	/*set center*/
	pos_x = 600;
	pos_y = 0;
	pipe_src_w = 1920;
	pipe_src_h = 1080;

	size_h = (size_h > 1920) ? 1920 : size_h;
	size_v = (size_v > 1080) ? 1080 : size_v;

	controlval = 0x98000000; /* BGRX888 */
	//controlval = 0x94000000; /* BGRX5650 */

	pci_read_config_dword(pdev, 0x70, &bgsm);

	bgsm_virt = ioremap_nocache(bgsm, 16);

	gtt_base_addr = ioread32(bgsm_virt);

	surface_base_addr = base_addr - (gtt_base_addr & 0xFFFFF000);

	positionval = (pos_y << 16 ) | pos_x;
        resolutionval = ((size_v -1)<< 16 )| (size_h-1);

	/* Disable VGA */
        iowrite32(0x80000000, io_base + 0x71400);

        /* Disable PND deadline calc and enable HDMI lanes ready as workaround */
        iowrite32(0x80800000, io_base + 0x70400);

        /* Disable clock gating */
	/* Disabling clock gating is not needed. Not sure why it
	 * was put here. If this code executes, it causes display
	 * controller not doing clock gating and preventing S0i1-Display
	 * from working properly
	 */
        //iowrite32(0xffffffff, io_base + 0x70500);
        //iowrite32(0xffffffff, io_base + 0x70504);

	/* Sprite Control Register (Sprite B) */

	/*    Sprite Format = BGRX */
	iowrite32(controlval, io_base + 0x70180 + sprite);

	/* Sprite Linear Offet (panning) */
	iowrite32(0x0, io_base + 0x70184 + sprite);

	/* Sprite Stride */
	iowrite32(stride, io_base + 0x70188 + sprite);

	/* Sprite Position */
	iowrite32(positionval, io_base + 0x7018c + sprite);

	/* Sprite HxW */
	iowrite32(resolutionval, io_base + 0x70190 + sprite);

	/*override pipe src*/
	iowrite32(((pipe_src_w - 1) << 16) | (pipe_src_h - 1),
				io_base + 0x6001c + sprite);

	/* Sprite Base Address Register */
	iowrite32(0, io_base + 0x7019c + sprite);

       printk(KERN_ERR "SPBCNTR[0x%p] = 0x%x \n", io_base +  0x70180 + sprite,
               ioread32(io_base +  0x70180 + sprite));
       printk(KERN_ERR "SPBSTRIDE[0x%p] = 0x%x \n", io_base + 0x70184 + sprite,
               ioread32(io_base + 0x70184 + sprite));
       printk("SPBLINOFF [0x%p] = 0x%x \n", io_base  + 0x70188 + sprite,
               ioread32(io_base  + 0x70188 + sprite));
        printk("SPBPOS [0x%p] = 0x%x \n", io_base + 0x7018c + sprite,
               ioread32(io_base +  0x7018c + sprite));
        printk("SPBSIZE [0x%p] = 0x%x \n", io_base  + 0x70190 + sprite,
               ioread32(io_base + 0x70190 + sprite));
        printk("SPBSURF [0x%p] = 0x%x \n", io_base  + 0x7019c + sprite,
               ioread32(io_base  + 0x7019c + sprite));

}

static void hdmi_configure(int reso)
{
	u32 temp_val;

	/* HDMI PHY SET */
	iowrite32(0xaa1b8700, io_base + 0x61134); /* HDMIPHYMISCCTL */

	/* set port */
	temp_val = 0xc0000818;
	temp_val |= 0x00000200; /* MODE_HDMI */

	iowrite32(temp_val, io_base + 0X61140);

	/* set video dip data */
	//	hdmi_set_avi_dip();

	/* DIP control reg */
	temp_val = ioread32(io_base + VIDEO_DIP_CTL);
	temp_val |= 0xa0000000; /* enable DIP */

	/* AVI DIP */
	temp_val = temp_val & 0xFFC7FFFF;
	temp_val = temp_val | 0x00000000;
	iowrite32(temp_val, io_base + VIDEO_DIP_CTL);

	/* Pb0,Length, Version, Type code */
	iowrite32(0x000d0282, io_base + VIDEO_DIP_DATA);
	/*    #PB4, PB3, PB2, PB1 */

	/* temp_val = hdmi_get_h_v_aspect_ratio(); */
	/* 0x100000 works for all resolutions */
	temp_val = 0x100000;

	iowrite32(temp_val, io_base + VIDEO_DIP_DATA);

	/* temp_val = hdmi_get_video_index(); */
	/* 640x480 */
	if (reso == HD_640x480)
		temp_val = 0;
	/* 720x480 */
	else if (reso == HD_720x480)
		temp_val = 1;
	else
		temp_val = 1;


	iowrite32(temp_val, io_base + VIDEO_DIP_DATA);
	iowrite32(0, io_base + VIDEO_DIP_DATA);

	iowrite32(0, io_base + VIDEO_DIP_DATA);
	iowrite32(0, io_base + VIDEO_DIP_DATA);
	iowrite32(0, io_base + VIDEO_DIP_DATA);
	iowrite32(0, io_base + VIDEO_DIP_DATA);

	iowrite32(0, io_base + VIDEO_DIP_DATA);
	iowrite32(0, io_base + VIDEO_DIP_DATA);
	iowrite32(0, io_base + VIDEO_DIP_DATA);
	iowrite32(0, io_base + VIDEO_DIP_DATA);

	iowrite32(0, io_base + VIDEO_DIP_DATA);
	iowrite32(0, io_base + VIDEO_DIP_DATA);
	iowrite32(0, io_base + VIDEO_DIP_DATA);
	iowrite32(0, io_base + VIDEO_DIP_DATA);


 	/* enable the bit 21 */
	temp_val = temp_val|0x00200000;
	/* reset the frequencey */
	temp_val = temp_val & 0xFFFCFFFF;
	temp_val = temp_val | VIDEO_DIP_FREQ_EVERY_FRAME;
    iowrite32(temp_val, io_base + VIDEO_DIP_CTL);
}

static void hdmi_wait_for_vblank(int pipe)
{
	int count = 1000;
	u32 dwcrcerr = ioread32(io_base + 0x70024 + pipe);
	dwcrcerr |= 0xb3060000;
	iowrite32(dwcrcerr, io_base + 0x70024 + pipe);
	dwcrcerr = ioread32(io_base + 0x70024 + pipe);
	dwcrcerr &= 0x4;
	while (count-- && dwcrcerr != 0x4) {
		dwcrcerr = ioread32(io_base + 0x70024 + pipe);
		dwcrcerr &= 0x4;
		msleep(1);
	}
	if (dwcrcerr != 0x4) {
		DRM_ERROR("Vblank interrupt failed to generate.Test exiting\n");
		return;
	}

	/* Clearing Vblank interrupt */
	dwcrcerr = ioread32(io_base + 0x70024 + pipe);
	dwcrcerr &= 0x4;
	iowrite32(dwcrcerr, io_base + 0x70024 + pipe);
}

void hdmi_power_on(struct drm_device *dev)
{

	struct pci_dev *pdev = dev->pdev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	u32 ret;

	DRM_INFO("=================== hdmi begin ==========================");

	io_base = dev_priv->vdc_reg;

	power_off_all_pipes();

	power_on_all_pipes();

	pci_read_config_dword(pdev, 0x4, &ret);

	pci_read_config_dword(pdev, 0x70, &ret);

	pci_read_config_dword(pdev, 0x10, &ret);

	hdmi_dll_program();

	/* PIPE_B, RESO19x10 */
	hdmi_pipe_set_reso(0x1000, RRESO19x10);

	 /* SPRITE_B:1920x1080 */
	hdmi_sprite_enable(pdev, 0x1000,720,1280,0,0,0x2900000);

	hdmi_configure(HD_1920x1200);

	/* pipe_enable(0x1000, 0); PIPE_B, VIDEO_MODE */
	iowrite32(0x80000000, io_base + 0x70008 + 0x1000);

	/* wait for vblank */
	hdmi_wait_for_vblank(0x1000);
	DRM_INFO("=================== hdmi end ==========================");
}

void mrfl_power_on_displays(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	io_base = dev_priv->vdc_reg;

	/*power on hdmi*/
	//hdmi_power_on(dev);
}
