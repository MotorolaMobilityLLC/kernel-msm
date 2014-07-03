/*
 * Merrifield Display Controller Driver
 *
 * Copyright (C) 2011 Intel Corporation
 * Author: Mark F. Brown <mark.f.brown@intel.com>
 * Author: Joel Rosenzweig <joel.b.rosenzweig@intel.com>
 * This code is based from goldfish_fb.c from Google Android
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include "mrfl_display_controller.h"

#define DRV_NAME "Merrifield Display Controller"

struct merrifield_fb {
	void __iomem *io_virt;
	uint32_t reg_base;
	int rotation;
	struct fb_info fb;
	u32 cmap[16];
};

static struct fb_fix_screeninfo merrifield_fb_fix = {
	.id = "mrfl_fb",
	.type = FB_TYPE_PACKED_PIXELS,
	.ypanstep = 1,
	.visual = FB_VISUAL_PSEUDOCOLOR,
	.accel = FB_ACCEL_NONE,
};

void __iomem *lcd_io_virt_global;

static inline u32 convert_bitfield(int val, struct fb_bitfield *bf)
{
	unsigned int mask = (1 << bf->length) - 1;

	return (val >> (16 - bf->length) & mask) << bf->offset;
}

static int merrifield_fb_setcolreg(unsigned int regno, unsigned int red,
				   unsigned int green, unsigned int blue,
				   unsigned int transp, struct fb_info *info)
{
	struct merrifield_fb *fb = container_of(info,
						struct merrifield_fb, fb);

	if (regno < 16) {
		fb->cmap[regno] = convert_bitfield(transp, &fb->fb.var.transp) |
		    convert_bitfield(blue, &fb->fb.var.blue) |
		    convert_bitfield(green, &fb->fb.var.green) |
		    convert_bitfield(red, &fb->fb.var.red);
		return 0;
	} else
		return 1;

}

static int merrifield_fb_check_var(struct fb_var_screeninfo *var,
				   struct fb_info *info)
{
	if ((var->rotate & 1) != (info->var.rotate & 1)) {
		if ((var->xres != info->var.yres) ||
		    (var->yres != info->var.xres) ||
		    (var->xres_virtual != info->var.yres) ||
		    (var->yres_virtual > info->var.xres * 2) ||
		    (var->yres_virtual < info->var.xres)) {
			return -EINVAL;
		}
	} else {
		if ((var->xres != info->var.xres) ||
		    (var->yres != info->var.yres) ||
		    (var->xres_virtual != info->var.xres) ||
		    (var->yres_virtual > info->var.yres * 2) ||
		    (var->yres_virtual < info->var.yres)) {
			return -EINVAL;
		}
	}

	if ((var->xoffset != info->var.xoffset) ||
	    (var->bits_per_pixel != info->var.bits_per_pixel) ||
	    (var->grayscale != info->var.grayscale)) {
		return -EINVAL;
	}
	return 0;
}

static int merrifield_fb_set_par(struct fb_info *info)
{
	struct merrifield_fb *fb = container_of(info,
						struct merrifield_fb, fb);

	if (fb->rotation != fb->fb.var.rotate) {
		info->fix.line_length = info->var.xres * 2;
		fb->rotation = fb->fb.var.rotate;
	}

	return 0;
}

static int merrifield_fb_pan_display(struct fb_var_screeninfo *var,
				     struct fb_info *info)
{
	/* TODO convert magic numbers to macro definitions */
	if (var->yoffset == 0)
		/* surface address register */
		iowrite32(0x0, lcd_io_virt_global + 0x7019C);
	else
		/* surface address register */
		iowrite32(var->xres * (var->bits_per_pixel / 8) * var->yres,
			  lcd_io_virt_global + 0x7019C);

	return 0;
}

static struct fb_ops merrifield_fb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = merrifield_fb_check_var,
	.fb_set_par = merrifield_fb_set_par,
	.fb_setcolreg = merrifield_fb_setcolreg,
	.fb_pan_display = merrifield_fb_pan_display,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
};

static void init_display_controller_registers(void __iomem *io_virt)
{

	int width = H_ACTIVE;
	int height = V_ACTIVE;
	int viewport_width = H_ACTIVE;
	int viewport_height = V_ACTIVE;

	/* TODO convert magic numbers to macro definitions */
	/* Programming LNC CRC Registers */
	iowrite32(0x80135937, io_virt + 0x60050);
	iowrite32(0x0048efae, io_virt + 0x60054);
	iowrite32(0x004ad4cb, io_virt + 0x60058);
	iowrite32(0x0, io_virt + 0x6005c);
	/* Program clocks and check for clock lock */
	/* Program DSI PLL */
	iowrite32(0x0, io_virt + 0x0F014);
	iowrite32(0x000000c1, io_virt + 0xF040);
	iowrite32(0x00800000, io_virt + 0xF014);
	iowrite32(0x80800000, io_virt + 0xF014);
	iowrite32(0x0, io_virt + 0x62190);
	/* Enable MIPI port */
	iowrite32(0x80810000, io_virt + 0x61190);
	iowrite32(0x270f04, io_virt + 0x61210);
	/* MIPI DPHY PARAM REG X */
	iowrite32(0xb14540c, io_virt + 0xB080);
	/* Data lanes - 2 RGB 888 X */
	iowrite32((RGB888 << 7) | 0x12, io_virt + 0xB00c);
	/* Video mode - Burst mode X */
	iowrite32(0x5, io_virt + 0xB058);
	/* MIPI control register X */
	iowrite32(0x18, io_virt + 0xB104);
	/* Interrupt enable X */
	iowrite32(0xffffffff, io_virt + 0xB008);
	/* MIPI HS-TX timeout reg X */
	iowrite32(0x3fffff, io_virt + 0xB010);
	/* LP reception timeout reg X */
	iowrite32(0xffff, io_virt + 0xB014);
	/* Turnaround timeout X */
	iowrite32(0x1f, io_virt + 0xB018);
	/* Reset timeout X */
	iowrite32(0xff, io_virt + 0xB01c);
	/* HS to LP timeout reg X */
	iowrite32(0x46, io_virt + 0xB044);
	/* MIPI Clock lane switching time count */
	iowrite32(0xa0014, io_virt + 0xB088);
	/* DBI bandwidth control register */
	iowrite32(0x400, io_virt + 0xB084);
	/* Master_init_timer X */
	iowrite32(0x7d0, io_virt + 0xB050);
	/* Disable clock stopping X */
	iowrite32(0x0, io_virt + 0xB05C);
	/* LP Byte clock X */
	iowrite32(0x4, io_virt + 0xB060);
	/* DPI resolution X */
	iowrite32((height << 16) | width, io_virt + 0xB020);
	/* Horizontal sync padding X */
	iowrite32(0x4, io_virt + 0xB028);
	/* Horizontal back porch X */
	iowrite32(0xe, io_virt + 0xB02c);
	/* Horizontal Front porch X */
	iowrite32(0x8, io_virt + 0xB030);
	/* Horizontal active area count X */
	iowrite32(0x2d0, io_virt + 0xB034);
	/* Vertical Sync padding X */
	iowrite32(0x4, io_virt + 0xB038);
	/* Vertical back porch X */
	iowrite32(0x8, io_virt + 0xB03c);
	/* Vertical front porch X */
	iowrite32(0x7, io_virt + 0xB040);
	/* Turn on DPI */
	iowrite32(0x1, io_virt + 0xB000);
	/* Turn on DPI Control register */
	iowrite32(0x2, io_virt + 0xB048);
	/* Programming Pipe A */
	iowrite32((((H_ACTIVE + 80 - 1) << 16) | (H_ACTIVE - 1)),
		  io_virt + 0x60000);
	iowrite32((((H_ACTIVE + 80 - 1) << 16) | (H_ACTIVE - 1)),
		  io_virt + 0x60004);
	iowrite32(((H_ACTIVE + 48 - 1) << 16) | (H_ACTIVE + 8 - 1),
		  io_virt + 0x60008);
	iowrite32((((V_ACTIVE + 5) << 16) | (V_ACTIVE - 1)), io_virt + 0x6000C);
	iowrite32((((V_ACTIVE + 5) << 16) | (V_ACTIVE - 1)), io_virt + 0x60010);
	iowrite32((((V_ACTIVE + 2) << 16) | (V_ACTIVE + 1)), io_virt + 0x60014);
	iowrite32((((H_ACTIVE - 1) << 16) | (V_ACTIVE - 1)), io_virt + 0x6001C);
	iowrite32(0x7b1dffff, io_virt + 0x70500);
	iowrite32(0x6c000000, io_virt + 0x70504);
	iowrite32(0x0, io_virt + 0x701D0);
	iowrite32(0x0, io_virt + 0x701D4);
	/* 0x5 == RGB565 */
	/* 0xa == RGB888 24-bit RGB no Alpha */
	/* 0xf == RGBA8888 32-bit RGB */
	/* Enable Display Sprite A */
	iowrite32(0x80000000 | (0x5 << 26), io_virt + 0x70180);
	/* Stride */
	iowrite32(0x00000780, io_virt + 0x70188);
	/* Linear offset register */
	iowrite32(0x0, io_virt + 0x70184);
	/* Position */
	iowrite32(0x0, io_virt + 0x7018C);
	/* Width and height X */
	iowrite32(((viewport_height - 1) << 16) | (viewport_width - 1),
		  io_virt + 0x70190);
	/* Surface address register */
	iowrite32(0x0, io_virt + 0x7019C);
	/* Disable VGA plane */
	iowrite32(0x80000000, io_virt + 0x71400);
	/* Pipe A Enable */
	iowrite32(0x80000000, io_virt + 0x70008);
	/* Pipe A Status register */
	iowrite32(0xb000ffff, io_virt + 0x70024);

}

static int merrifield_fb_probe(struct pci_dev *pdev,
			       const struct pci_device_id *id)
{
	int err;
	void __iomem *io_virt;
	struct fb_info *info;
	int fb_base = 0;
	int width = H_ACTIVE;
	int height = V_ACTIVE;

	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "failed to enable device!\n");
		goto failure;
	}

	info = framebuffer_alloc(sizeof *info, &pdev->dev);
	if (!info) {
		dev_err(&pdev->dev, "framebuffer allocation failure.\n");
		goto failure;
	}

	merrifield_fb_fix.mmio_start = pci_resource_start(pdev, 0);
	merrifield_fb_fix.mmio_len = pci_resource_len(pdev, 0);

	if (!request_mem_region(merrifield_fb_fix.mmio_start,
				merrifield_fb_fix.mmio_len, DRV_NAME)) {
		dev_err(&pdev->dev, "mmio request_mem_region failure!\n");
		goto failure;
	}

	info->par = kzalloc(sizeof *info->par, GFP_KERNEL);
	if (info->par == NULL) {
		dev_err(&pdev->dev, "failed to allocate info->par\n");
		goto failure;
	}
	lcd_io_virt_global = io_virt =
	    ioremap_nocache(merrifield_fb_fix.mmio_start,
			    merrifield_fb_fix.mmio_len);

	pci_read_config_dword(pdev, 0x5C, &fb_base);

	/* Allocate enough for up 2 x 16-bit frame buffers at
	 * our given resolution which is used for double buffering */
	merrifield_fb_fix.smem_start = fb_base;
	merrifield_fb_fix.smem_len = H_ACTIVE * V_ACTIVE * 4;

	info->screen_base = ioremap_nocache(merrifield_fb_fix.smem_start,
					    merrifield_fb_fix.smem_len);
	info->fix = merrifield_fb_fix;
	info->fbops = &merrifield_fb_ops;
	info->flags =
	    FBINFO_DEFAULT | FBINFO_HWACCEL_DISABLED | FBINFO_FLAG_DEFAULT;
	strcat(info->fix.id, DRV_NAME);
	info->var.activate = FB_ACTIVATE_NOW;
	info->device = &pdev->dev;

	err = fb_alloc_cmap(&info->cmap, 256, 0);
	if (err < 0) {
		dev_err(&pdev->dev, "cmap allocation failure.\n");
		goto failure;
	}

	/* RGB 5:6:5 */
	info->pseudo_palette = &info->cmap;
	info->cmap.len = 16;
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = FB_VISUAL_TRUECOLOR;
	info->fix.line_length = width * 2;
	info->fix.accel = FB_ACCEL_NONE;
	info->fix.ypanstep = 1;
	info->fix.smem_start = merrifield_fb_fix.smem_start;
	info->fix.smem_len = merrifield_fb_fix.smem_len;
	info->var.xres = width;
	info->var.yres = height;
	info->var.xres_virtual = width;
	info->var.yres_virtual = height * 2;
	info->var.bits_per_pixel = 16;
	info->var.height = height;
	info->var.width = width;
	info->var.red.offset = 11;
	info->var.red.length = 5;
	info->var.green.offset = 5;
	info->var.green.length = 6;
	info->var.blue.offset = 0;
	info->var.blue.length = 5;

	err = fb_set_var(info, &info->var);
	if (err) {
		dev_err(&pdev->dev, "error setting var info\n");
		goto failure;
	}

	info->pixmap.addr = kmalloc(4096, GFP_KERNEL);
	if (!info->pixmap.addr) {
		dev_err(&pdev->dev, "pixmap allocation failure\n");
		goto failure;
	}

	info->pixmap.size = 4096;
	info->pixmap.buf_align = 4;
	info->pixmap.scan_align = 1;
	info->pixmap.access_align = 32;
	info->pixmap.flags = FB_PIXMAP_SYSTEM;

	pci_set_drvdata(pdev, info);
	platform_set_drvdata(pdev, info);
	if (register_framebuffer(info) < 0) {
		dev_err(&pdev->dev, "could not register framebuffer\n");
		goto failure;
	}

	init_display_controller_registers(io_virt);

	return 0;

 failure:
	/* TODO clean-up routine */
	BUG();
}

static void merrifield_fb_remove(struct pci_dev *pdev)
{
	/* TODO add teardown routine */
	BUG();
}

static DEFINE_PCI_DEVICE_TABLE(merrifield_fb_devices) = {
	{
	PCI_VENDOR_ID_INTEL, 0x1180, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0}, {
	0,}
};

MODULE_DEVICE_TABLE(pci, merrifield_fb_devices);

static struct pci_driver merrifield_fb_driver = {
	.name = DRV_NAME,
	.id_table = merrifield_fb_devices,
	.probe = merrifield_fb_probe,
	.remove = merrifield_fb_remove,
};

static int __init merrifield_fb_init(void)
{
	return pci_register_driver(&merrifield_fb_driver);
}

static void __exit merrifield_fb_exit(void)
{
	pci_unregister_driver(&merrifield_fb_driver);
}

module_init(merrifield_fb_init);
module_exit(merrifield_fb_exit);
