/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2014 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */  
    
#include "fpc1020_debug.h"
    
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>
static int storeIndex = 0;
 static struct file *file_open(const char *path, int flags, int rights) 
{
	struct file *filp = NULL;
	mm_segment_t oldfs;
	int err = 0;
	 oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);
	set_fs(oldfs);
	if (IS_ERR(filp))
		 {
		err = PTR_ERR(filp);
		return NULL;
		}
	return filp;
}

 static void file_close(struct file *file) 
{
	filp_close(file, NULL);
}  static int file_write(struct file *file, unsigned long long offset,
			    unsigned char *data, unsigned int size) 
{
	mm_segment_t oldfs;
	int ret;
	 oldfs = get_fs();
	set_fs(get_ds());
	 ret = vfs_write(file, data, size, &offset);
	 set_fs(oldfs);
	return ret;
}

 static int file_sync(struct file *file) 
{
	vfs_fsync(file, 0);
	return 0;
}

 void fpc1020_debug_store_image(u8 * p_image, int width, int height) 
{
	int image_size = width * height * sizeof(u8);
	char a_filename[100];
	char a_header[100];
	struct file *p_file;
	if (p_image == NULL)
		 {
		pr_info("[FPC] fpc1020_debug_store_image NULL\n");
		return;
		}
	sprintf(a_filename, "/sdcard/fpc_debug/nav_image_%04d.pgm",
		 storeIndex);
	sprintf(a_header, "P5\n# CREATOR: FPC Driver Debug\n%d %d\n255\n",
		 width, height);
	 p_file = file_open(a_filename, O_WRONLY | O_CREAT, 0644);
	 file_write(p_file, 0, a_header, strlen(a_header));
	file_write(p_file, strlen(a_header), p_image, image_size);
	file_sync(p_file);
	file_close(p_file);
	 storeIndex++;
}


