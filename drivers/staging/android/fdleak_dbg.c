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

#include <linux/fdleak_dbg.h>
#include <linux/preempt.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/kmod.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fdtable.h>

/*
 * A small fdleak_dbg_bigfd value can lead to obvious performance issues
 * in Android. Use MIN_BIG_FD as the bottom line.
 */
#define MIN_BIG_FD			128
#ifndef CONFIG_ANDROID_BIG_FD
#define CONFIG_ANDROID_BIG_FD		(__FD_SETSIZE / 2)
#endif

#define SYSMON_EXE			"/system/bin/sysmon"

/*
 * Value of "/sys/kernel/debug/fdleak_dbg_bigfd":
 * 0 ~ (MIN_BIG_FD - 1)			==> See MIN_BIG_FD
 * MIN_BIG_FD ~ (__FD_SETSIZE - 1)	==> Enable FD leak debugging
 * __FD_SETSIZE ~ UINT_MAX		==> Disable FD leak debugging
 * -1					==> Disable FD leak debugging
 *
 * Assuming one slightly-outdated fdleak_dbg_bigfd value adds no big noise,
 * we now have no memory barriers for fdleak_dbg_bigfd.
 */
static uint32_t fdleak_dbg_bigfd = UINT_MAX;

static int notify_sysmond_of_big_fd(unsigned int fd, pid_t tid, pid_t pid)
{
	char pid_str[12];
	char tid_str[12];
	char fd_str[12];
	char *argv[] = { SYSMON_EXE, "start", "log_bigfd", "-q",
			 "-p", pid_str, " -t", tid_str, fd_str, NULL };
	char *envp[] = { NULL };
	snprintf(pid_str, sizeof(pid_str), "%d", pid);
	snprintf(tid_str, sizeof(tid_str), "%d", tid);
	snprintf(fd_str, sizeof(fd_str), "%d", fd);
	return call_usermodehelper(argv[0], argv, envp, UMH_WAIT_EXEC);
}

void warn_if_big_fd(unsigned int fd, struct task_struct *dst_tsk)
{
	pid_t cur_tid;
	pid_t dst_pid;

	if ((fd < fdleak_dbg_bigfd) || (fd >= __FD_SETSIZE)) {
		/*
		 * Ignore if fd is too small or too big
		 */
		return;
	}

	cur_tid = task_pid_nr(current);
	dst_pid = task_tgid_nr(dst_tsk);

	if (!notify_sysmond_of_big_fd(fd, cur_tid, dst_pid)) {
		/*
		 * Big fd added to dst_pid by cur_tid.
		 * Wait some mill-seconds to grab current thread's stack
		 */
		msleep_interruptible(100);
	}
}

static int get_fdleak_dbg_bigfd(void *data, u64 *val)
{
	*val = fdleak_dbg_bigfd;
	return 0;
}

static int set_fdleak_dbg_bigfd(void *data, u64 val)
{
	uint32_t bigfd = (uint32_t)val;
	if (bigfd < MIN_BIG_FD)
		bigfd = MIN_BIG_FD;
	fdleak_dbg_bigfd = bigfd;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fdleak_dbg_bigfd_fops, get_fdleak_dbg_bigfd,
			set_fdleak_dbg_bigfd, "%llu\n");

static int __init fdleak_dbg_init(void)
{
	fdleak_dbg_bigfd = CONFIG_ANDROID_BIG_FD;
	debugfs_create_file("fdleak_dbg_bigfd", 0644, NULL, NULL,
			    &fdleak_dbg_bigfd_fops);
	return 0;
}

static void __exit fdleak_dbg_exit(void)
{
}

module_init(fdleak_dbg_init);
module_exit(fdleak_dbg_exit);
MODULE_LICENSE("GPL");
