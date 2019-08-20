/*
 * Copyright (C) 2019 Motorola Mobility Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pid.h>

#include <linux/signal.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <trace/events/signal.h>

#define CREATE_TRACE_POINTS
#include <trace/events/mmi_sigprint.h>


#define STORE_SIGINFO(_errno, _code, info)			\
	do {							\
		if (info == SEND_SIG_NOINFO ||			\
		    info == SEND_SIG_FORCED) {			\
			_errno	= 0;				\
			_code	= SI_USER;			\
		} else if (info == SEND_SIG_PRIV) {		\
			_errno	= 0;				\
			_code	= SI_KERNEL;			\
		} else {					\
			_errno	= info->si_errno;		\
			_code	= info->si_code;		\
		}						\
	} while (0)

static const char * const signal_deliver_results[] = {
	"delivered",
	"ignored",
	"already_pending",
	"overflow_fail",
	"lost_info",
};

#ifndef TASK_STATE_TO_CHAR_STR
#define TASK_STATE_TO_CHAR_STR "RSDTtXZxKWPNn"
#endif

static const char stat_nam[] = TASK_STATE_TO_CHAR_STR;

/*
 * This portion of the code can be used as template if user want to add specific
 * signals to debug msg.
 */

/*
static void probe_signal_generate(void *ignore, int sig, struct siginfo *info,
		struct task_struct *task, int group, int result)
{
	unsigned state = task->state ? __ffs(task->state) + 1 : 0;
	int errno, code;

	// only log delivered signals
	STORE_SIGINFO(errno, code, info);
	pr_info("[signal][%d:%s]generate sig %d to [%d:%s:%c] errno=%d code=%d grp=%d res=%s\n",
		 current->pid, current->comm, sig,
		task->pid, task->comm,
		state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?',
		errno, code, group, signal_deliver_results[result]);
}

static void probe_signal_deliver(void *ignore, int sig, struct siginfo *info,
		struct k_sigaction *ka)
{
	int errno, code;

	STORE_SIGINFO(errno, code, info);
	pr_info("[signal]sig %d delivered to [%d:%s] errno=%d code=%d sa_handler=%lx sa_flags=%lx\n",
			sig, current->pid, current->comm, errno, code,
			(unsigned long)ka->sa.sa_handler, ka->sa.sa_flags);
}
*/

static void probe_death_signal(void *ignore, int sig, struct siginfo *info,
		struct task_struct *task, int _group, int result)
{
	struct signal_struct *signal = task->signal;
	unsigned int state;
	int group;

	/*
	 * all action will cause process coredump or terminate
	 * kernel log reduction: only print delivered signals
	 */
	if (sig_fatal(task, sig) && result == TRACE_SIGNAL_DELIVERED) {
		signal = task->signal;
		group = _group ||
			(signal->flags & (SIGNAL_GROUP_EXIT | SIGNAL_GROUP_COREDUMP));

		/*
		 * kernel log reduction
		 * skip SIGRTMIN because it's used as timer signal
		 * skip if the target thread is already dead
		 */
		if (sig == SIGRTMIN ||
		    (task->state & (TASK_DEAD | EXIT_DEAD | EXIT_ZOMBIE)))
			return;
		/*
		 * Global init gets no signals it doesn't want.
		 * Container-init gets no signals it doesn't want from same
		 * container.
		 *
		 * Note that if global/container-init sees a sig_kernel_only()
		 * signal here, the signal must have been generated internally
		 * or must have come from an ancestor namespace. In either
		 * case, the signal cannot be dropped.
		 */
		if (unlikely(signal->flags & SIGNAL_UNKILLABLE) &&
				!sig_kernel_only(sig))
			return;

		/*
		 * kernel log reduction
		 * only print process instead of all threads
		 */
		if (group && (task != task->group_leader))
			return;

		state = task->state ? __ffs(task->state) + 1 : 0;
		pr_info("[signal][%d:%s]send death sig %d to[%d:%s:%c]\n",
			 current->pid, current->comm,
			 sig, task->pid, task->comm,
			 state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?');
	} else if ((sig_kernel_stop(sig) && result == TRACE_SIGNAL_DELIVERED) ||
		   sig == SIGCONT) {

		/*
		 * kernel log reduction
		 * only print process instead of all threads
		 */
		if (_group && (task != task->group_leader))
			return;

		state = task->state ? __ffs(task->state) + 1 : 0;
		pr_info("[signal][%d:%s]send %s sig %d to[%d:%s:%c]\n",
			 current->pid, current->comm,
			 (sig == SIGCONT) ? "continue" : "stop",
			 sig, task->pid, task->comm,
			 state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?');
	}
}

static int __init init_signal_log(void)
{

	register_trace_signal_generate(probe_death_signal, NULL);

	// Code example if extra debug msg needed to add.
	/*
	register_trace_signal_deliver(probe_signal_deliver, NULL);
	register_trace_signal_generate(probe_signal_generate, NULL);
	*/
	return 0;
}

static int mmi_sigprint_init(void)
{

	return init_signal_log();
}


device_initcall(mmi_sigprint_init);
