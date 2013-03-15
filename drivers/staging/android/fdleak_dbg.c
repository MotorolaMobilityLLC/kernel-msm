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
#include <linux/slab.h>

/*
 * A small fdleak_dbg_bigfd value can lead to obvious performance issues
 * in Android. Use MIN_BIG_FD as the bottom line.
 */
#define MIN_BIG_FD			(0x80)
#ifndef CONFIG_ANDROID_BIG_FD
#define CONFIG_ANDROID_BIG_FD		(__FD_SETSIZE / 2)
#endif

#define ANDROID_EXE_PATH		"/system/bin/app_process"
#define HELSMON_EXE			"/system/bin/helsmon"

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

static int notify_helsmond_of_big_fd(unsigned int fd, pid_t tid, pid_t pid)
{
	char pid_str[12];
	char tid_str[12];
	char fd_str[12];
	char *argv[] = { HELSMON_EXE, "start", "log_bigfd", "-q",
			 "-p", pid_str, " -t", tid_str, fd_str, NULL };
	char *envp[] = { NULL };
	snprintf(pid_str, sizeof(pid_str), "%d", pid);
	snprintf(tid_str, sizeof(tid_str), "%d", tid);
	snprintf(fd_str, sizeof(fd_str), "0x%06x", fd);
	return call_usermodehelper(argv[0], argv, envp, UMH_WAIT_EXEC);
}

static int is_android_exe(struct task_struct *tsk)
{
	struct file *exe_file;
	char *pathbuf, *path;
	int ret = 0;

	exe_file = get_mm_exe_file(tsk->mm);
	if (!exe_file)
		return ret;

	pathbuf = kmalloc(PATH_MAX, GFP_TEMPORARY);
	if (!pathbuf)
		goto put_exe_file;

	path = d_path(&exe_file->f_path, pathbuf, PATH_MAX);
	if (!IS_ERR(path))
		ret = !strcmp(path, ANDROID_EXE_PATH);

	kfree(pathbuf);

put_exe_file:
	fput(exe_file);
	return ret;
}

void warn_if_big_fd(unsigned int fd, struct task_struct *dst_tsk)
{
	int sleep_ms;
	pid_t cur_tid;
	pid_t dst_pid;

	if ((fd < fdleak_dbg_bigfd) || (current->flags & PF_KTHREAD)) {
		/* Do nothing if fd is small or not in user-space process */
		return;
	}

	sleep_ms = 0;
	cur_tid = task_pid_nr(current);
	dst_pid = task_tgid_nr(dst_tsk);

	if (is_android_exe(current)) {
		struct siginfo info;
		info.si_signo = SIGUSR1;
		/*
		 * The following 4 fields are severely hacked
		 */
		info.si_errno = fd;
		info.si_code = SI_KERNEL;
		info.si_uid = (uid_t)dst_pid;
		info.si_pid = cur_tid;
		/*
		 * Assume there's at least one thread waiting for
		 * SIGUSR1 with SI_KERNEL si_code, that may dump
		 * the backtrace of "current" task and then send
		 * SIGCONT back as soon as possible.
		 */
		if (!group_send_sig_info(SIGUSR1, &info, current)) {
			/* Wait at most 100 milli-seconds */
			sleep_ms = 100;
		}
	} else if (!notify_helsmond_of_big_fd(fd, cur_tid, dst_pid)) {
		/* Slower because of added IPC. Use a longer timeout */
		sleep_ms = 200;
	}

	if (sleep_ms > 0) {
		/* Wait for dumping current thread's stack */
		msleep_interruptible(sleep_ms);
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
