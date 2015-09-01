/* Copyright (c) 2014, Motorola Mobility, LLC.
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
#ifndef _FB_QUICKDRAW_H_
#define _FB_QUICKDRAW_H_

#define FB_QUICKDRAW_IOCTL_MAGIC 'q'
#define FB_QUICKDRAW_INIT		_IO(FB_QUICKDRAW_IOCTL_MAGIC, 1)
#define FB_QUICKDRAW_ADD_BUFFER		_IOW(FB_QUICKDRAW_IOCTL_MAGIC, 2, \
					struct fb_quickdraw_buffer_data)
#define FB_QUICKDRAW_REMOVE_BUFFER	_IOW(FB_QUICKDRAW_IOCTL_MAGIC, 3, int)
#define FB_QUICKDRAW_LOCK_BUFFER	_IOW(FB_QUICKDRAW_IOCTL_MAGIC, 4, int)
#define FB_QUICKDRAW_UNLOCK_BUFFER	_IOW(FB_QUICKDRAW_IOCTL_MAGIC, 5, int)

struct fb_quickdraw_rect {
	int x;
	int y;
	int w;
	int h;
};

struct fb_quickdraw_buffer_data {
	int buffer_id;
	int user_fd;
	int format;
	struct fb_quickdraw_rect rect;
};

#ifdef __KERNEL__

#define FB_QUICKDRAW_NAME	"fb_quickdraw"

#define COORD_NO_OVERRIDE          -1
#define QUICKDRAW_ESD_RECOVERED -1337

#ifdef CONFIG_FB_QUICKDRAW

int fb_quickdraw_prepare(unsigned char panel_state);
int fb_quickdraw_execute(int buffer_id, int commit, int x, int y);
int fb_quickdraw_erase(int commit, int x1, int y1, int x2, int y2);
int fb_quickdraw_cleanup(int commit);

#else

static inline int fb_quickdraw_prepare(unsigned char panel_state)
	{ return 0; };
static inline int fb_quickdraw_execute(int buffer_id, int commit, int x, int y)
	{ return 0; };
static inline int fb_quickdraw_erase(int commit, int x1, int y1, int x2, int y2)
	{ return 0; };
static inline int fb_quickdraw_cleanup(int commit)
	{ return 0; };
#endif /* CONFIG_FB_QUICKDRAW */

#endif /* __KERNEL__ */

#endif /* _FB_QUICKDRAW_H_ */
