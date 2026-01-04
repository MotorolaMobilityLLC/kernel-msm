// SPDX-License-Identifier: GPL-2.0
/*
 * fs/f2fs/checkpoint.c
 *
 * Copyright (c) 2026 Google
 */
#include <linux/f2fs_fs.h>
#include <linux/delayacct.h>
#include <linux/ioprio.h>
#include <linux/math64.h>

#include "f2fs.h"
#include "segment.h"
#include <trace/events/f2fs.h>

static inline void get_lock_elapsed_time(struct f2fs_time_stat *ts)
{
	ts->total_time = ktime_get();
#ifdef CONFIG_64BIT
	ts->running_time = current->se.sum_exec_runtime;
#endif
#if defined(CONFIG_SCHED_INFO) && defined(CONFIG_SCHEDSTATS)
	ts->runnable_time = current->sched_info.run_delay;
#endif
#ifdef CONFIG_TASK_DELAY_ACCT
	if (current->delays)
		ts->io_sleep_time = current->delays->blkio_delay;
#endif
}

static inline void trace_lock_elapsed_time_start(struct f2fs_rwsem *sem,
						struct f2fs_lock_context *lc)
{
	lc->lock_trace = trace_f2fs_lock_elapsed_time_enabled();
	if (!lc->lock_trace)
		return;

	get_lock_elapsed_time(&lc->ts);
}

static inline void trace_lock_elapsed_time_end(struct f2fs_rwsem *sem,
				struct f2fs_lock_context *lc, bool is_write)
{
	struct f2fs_time_stat tts;
	unsigned long long total_time;
	unsigned long long running_time = 0;
	unsigned long long runnable_time = 0;
	unsigned long long io_sleep_time = 0;
	unsigned long long other_time = 0;
	unsigned npm = NSEC_PER_MSEC;

	if (!lc->lock_trace)
		return;

	get_lock_elapsed_time(&tts);

	total_time = div_u64(tts.total_time - lc->ts.total_time, npm);
	if (total_time <= MAX_LOCK_ELAPSED_TIME)
		return;

#ifdef CONFIG_64BIT
	running_time = div_u64(tts.running_time - lc->ts.running_time, npm);
#endif
#if defined(CONFIG_SCHED_INFO) && defined(CONFIG_SCHEDSTATS)
	runnable_time = div_u64(tts.runnable_time - lc->ts.runnable_time, npm);
#endif
#ifdef CONFIG_TASK_DELAY_ACCT
	io_sleep_time = div_u64(tts.io_sleep_time - lc->ts.io_sleep_time, npm);
#endif
	if (total_time > running_time + io_sleep_time + runnable_time)
		other_time = total_time - running_time -
				io_sleep_time - runnable_time;

	trace_f2fs_lock_elapsed_time(sem->sbi, sem->name, is_write, current,
			get_current_ioprio(), total_time, running_time,
			runnable_time, io_sleep_time, other_time);
}

void f2fs_down_read_trace(struct f2fs_rwsem *sem, struct f2fs_lock_context *lc)
{
	f2fs_down_read(sem);
	trace_lock_elapsed_time_start(sem, lc);
}

int f2fs_down_read_trylock_trace(struct f2fs_rwsem *sem, struct f2fs_lock_context *lc)
{
	if (!f2fs_down_read_trylock(sem))
		return 0;
	trace_lock_elapsed_time_start(sem, lc);
	return 1;
}

void f2fs_up_read_trace(struct f2fs_rwsem *sem, struct f2fs_lock_context *lc)
{
	f2fs_up_read(sem);
	trace_lock_elapsed_time_end(sem, lc, false);
}

void f2fs_down_write_trace(struct f2fs_rwsem *sem, struct f2fs_lock_context *lc)
{
	f2fs_down_write(sem);
	trace_lock_elapsed_time_start(sem, lc);
}

int f2fs_down_write_trylock_trace(struct f2fs_rwsem *sem, struct f2fs_lock_context *lc)
{
	if (!f2fs_down_write_trylock(sem))
		return 0;
	trace_lock_elapsed_time_start(sem, lc);
	return 1;
}

void f2fs_up_write_trace(struct f2fs_rwsem *sem, struct f2fs_lock_context *lc)
{
	f2fs_up_write(sem);
	trace_lock_elapsed_time_end(sem, lc, true);
}
