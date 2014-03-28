/* drivers/video/msm/logo.c
 *
 * Show Logo in RLE 565 format
 *
 * Copyright (C) 2008 Google Incorporated
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/vt_kern.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>

#include <linux/irq.h>
#include <asm/system.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_dsi.h"

#define fb_width(fb)	((fb)->var.xres)
#define fb_height(fb)	((fb)->var.yres)
#define fb_size(fb)	((fb)->var.xres * (fb)->var.yres * 2)

#ifdef CONFIG_SAMSUNG_LPM_MODE
extern int poweroff_charging;
#endif

/*#define ENABLE_BOOTLOGO*/
/*#define ENABLE_SCREEN_COPY*/
#if defined(ENABLE_BOOTLOGO)
static struct workqueue_struct  *wq_bootlogo;
static struct delayed_work w_bootlogo;
#endif

int get_lcd_attached(void) {
	return 1;
};

static void memset16(void *_ptr, unsigned short val, unsigned count)
{
	unsigned short *ptr = _ptr;
	count >>= 1;
	while (count--)
		*ptr++ = val;
}


/* convert RGB565 to RBG8888 */
static int total_pixel = 1;
static int memset16_rgb8888(void *_ptr, unsigned short val, unsigned count,
				struct fb_info *fb)
{
	unsigned short *ptr = _ptr;
	unsigned short red;
	unsigned short green;
	unsigned short blue;
	int need_align = (fb->fix.line_length >> 2) - fb->var.xres;
	int align_amount = need_align << 1;
	int pad = 0;

	red = (val & 0xF800) >> 8;
	green = (val & 0x7E0) >> 3;
	blue = (val & 0x1F) << 3;

	count >>= 1;
	while (count--) {
		*ptr++ = (green << 8) | red;
		*ptr++ = blue;

		if (need_align) {
			if (!(total_pixel % fb->var.xres)) {
				ptr += align_amount;
				pad++;
			}
		}

		total_pixel++;
	}

	return pad * align_amount;
}

/* 565RLE image format: [count(2 bytes), rle(2 bytes)] */
int load_565rle_image(char *filename)
{
	struct fb_info *info;
	int fd, count, err = 0;
	unsigned max;
	unsigned short *data, *bits, *ptr;
	int pad;

	info = registered_fb[0];
	if (!info) {
		printk(KERN_WARNING "%s: Can not access framebuffer\n",
			__func__);
		return -ENODEV;
	}

	fd = sys_open(filename, O_RDONLY, 0);
	if (fd < 0) {
		printk(KERN_WARNING "%s: Can not open %s\n",
			__func__, filename);
		return -ENOENT;
	}
	count = sys_lseek(fd, (off_t)0, 2);
	if (count <= 0) {
		err = -EIO;
		goto err_logo_close_file;
	}
	sys_lseek(fd, (off_t)0, 0);
	data = kmalloc(count, GFP_KERNEL);
	if (!data) {
		printk(KERN_WARNING "%s: Can not alloc data\n", __func__);
		err = -ENOMEM;
		goto err_logo_close_file;
	}
	if (sys_read(fd, (char *)data, count) != count) {
		err = -EIO;
		goto err_logo_free_data;
	}

	max = fb_width(info) * fb_height(info);
	ptr = data;
	if ((info->node == 1 || info->node == 2)) {
		err = -EPERM;
		pr_err("%s:%d no info->creen_base on fb%d!\n",
		       __func__, __LINE__, info->node);
		goto err_logo_free_data;
	}
	bits = (unsigned short *)(info->screen_base);
	if(!bits)
	{
		printk(KERN_WARNING "%s: screen_base is not valid\n", __func__);
		goto err_logo_free_data;
	}
	while (count > 3) {
		unsigned n = ptr[0];
		if (n > max)
			break;
		if (info->var.bits_per_pixel >= 24) {
			pad = memset16_rgb8888(bits, ptr[1], n << 1, info);
			bits += n << 1;
			bits += pad;
		} else {
			memset16(bits, ptr[1], n << 1);
			bits += n;
		}
		max -= n;
		ptr += 2;
		count -= 4;
	}

err_logo_free_data:
	kfree(data);
err_logo_close_file:
	sys_close(fd);
	return err;
}
EXPORT_SYMBOL(load_565rle_image);

#ifdef ENABLE_SCREEN_COPY
static int samsung_copy_bootloader_screen(void *virt)
{

	struct fb_info *info = registered_fb[0];

	unsigned long bl_fb_addr = 0;
	unsigned long *bl_fb_addr_va;
	unsigned long  pipe_addr, pipe_src_size;
	u32 height, width, rgb_size, bpp;char *bit_src,*bit_dst;
	size_t size;int i,j;
	pr_info("%s:+\n",__func__);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	pipe_addr = MDSS_MDP_REG_SSPP_OFFSET(3) +
		MDSS_MDP_REG_SSPP_SRC0_ADDR;

	pipe_src_size =
		MDSS_MDP_REG_SSPP_OFFSET(3) + MDSS_MDP_REG_SSPP_SRC_SIZE;

	bpp        = 3;
	
	rgb_size   = MDSS_MDP_REG_READ(pipe_src_size);
	bl_fb_addr = MDSS_MDP_REG_READ(pipe_addr);
	
	height = (rgb_size >> 16) & 0xffff;
	width  = rgb_size & 0xffff;
	size = PAGE_ALIGN(height * width * bpp);

	if( bl_fb_addr == 0x0)
		return -1;

#if defined(CONFIG_FB_MSM_EDP_SAMSUNG)
		bl_fb_addr_va = (unsigned long *)ioremap(bl_fb_addr, size*2);
#else
		bl_fb_addr_va = (unsigned long *)ioremap(bl_fb_addr, size);
#endif

	pr_info("%s:%d addr:%p->%p, splash_height=%d splash_width=%d Buffer size=%d\n",
			__func__, __LINE__, (void *)bl_fb_addr,(void *)bl_fb_addr_va,
			 height, width, size);



	bit_src = (char *) bl_fb_addr_va;
	bit_dst = (char *)info->screen_base;

	for( i = 0,j=0; j < size*2; i+=4,j+=3) {
		bit_dst[i] = bit_src[j];
		bit_dst[i+1] = bit_src[j+1];
		bit_dst[i+2] = bit_src[j+2];
	}
	
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	return 0;
}
#endif

static int  samsung_mdss_allocate_framebuffer(struct fb_info *info){


	void *virt = NULL;size_t size;
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	static struct ion_handle *ihdl;
	struct ion_client *iclient = mdss_get_ionclient();
	static ion_phys_addr_t phys;

	ihdl = ion_alloc(iclient, 0x1000000, SZ_1M,
			ION_HEAP(ION_QSECOM_HEAP_ID), 0);
	if (IS_ERR_OR_NULL(ihdl)) {
		pr_err("unable to alloc fbmem from ion (%p)\n", ihdl);
		return -ENOMEM;
	}

	virt = ion_map_kernel(iclient, ihdl);
	ion_phys(iclient, ihdl, &phys, &size);
	info->screen_base = virt;
	info->fix.smem_start = phys;
	info->fix.smem_len = size;

	msm_iommu_map_contig_buffer(phys, mfd->mdp.fb_mem_get_iommu_domain(), 0, size, SZ_4K, 0,
					    &mfd->iova);

#ifdef ENABLE_SCREEN_COPY
	/* Copy  screen */
	if(contsplash_lkstat == 1)
		ret =  samsung_copy_bootloader_screen(virt);
#endif

	return 1;
}

int load_samsung_boot_logo(void)
{
	struct fb_info *info = registered_fb[0];
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)info->par;
	struct mdss_panel_data *pdata = dev_get_platdata(&mfd->pdev->dev);
	int ret;

	if (!info) {
		printk(KERN_WARNING "%s: Can not access framebuffer\n",
			__func__);
		return -ENODEV;
	}

#ifdef CONFIG_SAMSUNG_LPM_MODE
	/* LPM mode : no boot logo */
	if(poweroff_charging)
		return 0;
#endif

	if (get_lcd_attached() == 0)
	{
		pr_err("%s: get_lcd_attached(0)!\n",__func__);
		return -ENODEV;
	}

	pr_info("%s:+\n",__func__);
	ret = samsung_mdss_allocate_framebuffer(info);
	
	info->fbops->fb_open(registered_fb[0], 0);
	
	if (ret && load_565rle_image("initlogo.rle")) {
		char *bits = info->screen_base;
		int i = 0;
		unsigned int max = fb_height(info) * fb_width(info);
		unsigned int val = 0xff;
		if(!bits)
		{
			printk(KERN_WARNING "%s: screen_base is not valid\n", __func__);
			return 0;
		}
		for (i = 0; i < 3; i++) {
			int count = max/3;
			while (count--) {
				unsigned int *addr = \
					(unsigned int *) bits;
				*addr = val;
				bits += 4;
			}
			val <<= 8;
		}
	} else
		pr_info("%s : load_565rle_image loading fail\n", __func__);
	
	fb_pan_display(info, &info->var);
	pdata->set_backlight(pdata, 114);
	
	pr_info("%s:-\n",__func__);
	return 0;
}
EXPORT_SYMBOL(load_samsung_boot_logo);

#if defined(ENABLE_BOOTLOGO)
static void bootlogo_work(struct work_struct *work)
{
	struct msm_fb_data_type *mfd = NULL ;
	static int bootlogo_displayed = 0;

	if (get_lcd_attached() == 0)
	{
		pr_err("%s: get_lcd_attached(0)!\n",__func__);
		return ;
	}

	if(!registered_fb[0]) {
			queue_delayed_work(wq_bootlogo, &w_bootlogo, msecs_to_jiffies(200));
			return;
	}	
	mfd = (struct msm_fb_data_type *)registered_fb[0]->par;
	if(bootlogo_displayed) {
		/* Need to release framebuffer once someone from userspace opens fb. */
		pr_info("%s: mfd->ref_cnt: %d\n",__func__, mfd->ref_cnt);
		if(mfd->ref_cnt >1) {
			registered_fb[0]->fbops->fb_release(registered_fb[0], 0);
			pr_info("Boot logo releasing fb0\n");
			memset(registered_fb[0]->screen_base,0x0,registered_fb[0]->fix.smem_len); 
		}else {
			queue_delayed_work(wq_bootlogo, &w_bootlogo, msecs_to_jiffies(1000));
		}
		return;
	}   
	load_samsung_boot_logo();
	bootlogo_displayed = 1;
	queue_delayed_work(wq_bootlogo, &w_bootlogo, msecs_to_jiffies(5000));
}

static int __init boot_logo_init(void) {

	
#ifdef CONFIG_SAMSUNG_LPM_MODE
	/* LPM mode : no boot logo */
	if(poweroff_charging)
		return 0;
#endif

	if (get_lcd_attached() == 0)
	{
		pr_err("%s: get_lcd_attached(0)!\n",__func__);
		return -ENODEV;
	}

	pr_info("%s:+\n",__func__);
	wq_bootlogo =	create_singlethread_workqueue("bootlogo");
	INIT_DELAYED_WORK(&w_bootlogo, bootlogo_work);
				
	queue_delayed_work(wq_bootlogo,
					&w_bootlogo, msecs_to_jiffies(4000));
	return 0;
}
module_init(boot_logo_init)
#endif
