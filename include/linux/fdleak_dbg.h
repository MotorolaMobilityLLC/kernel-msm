/*
 * Copyright (C) 2013 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef __LINUX_FDLEAK_DBG_H

#ifndef CONFIG_ANDROID_DEBUG_FD_LEAK
#define CONFIG_ANDROID_DEBUG_FD_LEAK	0
#endif

#if CONFIG_ANDROID_DEBUG_FD_LEAK
#include <linux/sched.h>
/*
 * Log the backtrace of current task_struct, if it is injecting a big fd
 * into dst_tsk. One fd is likely leaked if it is big.
 */
void warn_if_big_fd(unsigned int fd, struct task_struct *dst_tsk);
#else
#define warn_if_big_fd(fd, dst_tsk) do {} while (0)
#endif

#endif /* __LINUX_FDLEAK_DBG_H */
