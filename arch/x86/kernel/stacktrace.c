/*
 * Stack trace management functions
 *
 *  Copyright (C) 2006-2009 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 */
#include <linux/sched.h>
#include <linux/stacktrace.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/stacktrace.h>
#include <linux/mm.h>

static int save_stack_stack(void *data, char *name)
{
	return 0;
}

static void
__save_stack_address(void *data, unsigned long addr, bool reliable, bool nosched)
{
	struct stack_trace *trace = data;
#ifdef CONFIG_FRAME_POINTER
	if (!reliable)
		return;
#endif
	if (nosched && in_sched_functions(addr))
		return;
	if (trace->skip > 0) {
		trace->skip--;
		return;
	}
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = addr;
}

static void save_stack_address(void *data, unsigned long addr, int reliable)
{
	return __save_stack_address(data, addr, reliable, false);
}

static void
save_stack_address_nosched(void *data, unsigned long addr, int reliable)
{
	return __save_stack_address(data, addr, reliable, true);
}

static const struct stacktrace_ops save_stack_ops = {
	.stack		= save_stack_stack,
	.address	= save_stack_address,
	.walk_stack	= print_context_stack,
};

static const struct stacktrace_ops save_stack_ops_nosched = {
	.stack		= save_stack_stack,
	.address	= save_stack_address_nosched,
	.walk_stack	= print_context_stack,
};

/*
 * Save stack-backtrace addresses into a stack_trace buffer.
 */
void save_stack_trace(struct stack_trace *trace)
{
	dump_trace(current, NULL, NULL, 0, &save_stack_ops, trace);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}
EXPORT_SYMBOL_GPL(save_stack_trace);

void save_stack_trace_regs(struct pt_regs *regs, struct stack_trace *trace)
{
	dump_trace(current, regs, NULL, 0, &save_stack_ops, trace);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}

void save_stack_trace_tsk(struct task_struct *tsk, struct stack_trace *trace)
{
	dump_trace(tsk, NULL, NULL, 0, &save_stack_ops_nosched, trace);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}
EXPORT_SYMBOL_GPL(save_stack_trace_tsk);

/* Userspace stacktrace - based on kernel/trace/trace_sysprof.c */

struct stack_frame_user {
	const void __user	*next_fp;
	unsigned long		ret_addr;
};

static int
__copy_stack_frame(const void __user *fp, void *frame, unsigned long framesize)
{
	int ret;

	if (!access_ok(VERIFY_READ, fp, framesize))
		return 0;

	ret = 1;
	pagefault_disable();
	if (__copy_from_user_inatomic(frame, fp, framesize))
		ret = 0;
	pagefault_enable();

	return ret;
}

#ifdef CONFIG_COMPAT

#include <asm/compat.h>

struct compat_stack_frame_user {
	compat_uptr_t next_fp;
	compat_ulong_t ret_addr;
};

static inline int copy_stack_frame(const void __user *fp,
				   struct stack_frame_user *frame)
{
	struct compat_stack_frame_user frame32 = { 0 };

	if (!test_thread_flag(TIF_IA32))
		return __copy_stack_frame(fp, frame, sizeof(*frame));

	if (!__copy_stack_frame(fp, &frame32, sizeof(frame32)))
		return 0;

	frame->next_fp = compat_ptr(frame32.next_fp);
	frame->ret_addr = (unsigned long)frame32.ret_addr;
	return 1;
}

static inline int access_frame(struct task_struct *task, unsigned long addr,
			       struct stack_frame_user *frame)
{
	struct compat_stack_frame_user frame32 = { 0 };

	if (!test_tsk_thread_flag(task, TIF_IA32))
		return access_process_vm(task, addr,
					 (void *)frame, sizeof(*frame), 0);

	if (!access_process_vm(task, addr, (void *)&frame32,
			       sizeof(frame32), 0))
		return 0;

	frame->next_fp = compat_ptr(frame32.next_fp);
	frame->ret_addr = (unsigned long)frame32.ret_addr;
	return 1;
}
#else
static inline int copy_stack_frame(const void __user *fp,
				   struct stack_frame_user *frame)
{
	return __copy_stack_frame(fp, frame, sizeof(*frame));
}

static inline int access_frame(struct task_struct *task, unsigned long addr,
			       struct stack_frame_user *frame)
{
	return access_process_vm(task, addr, (void *)frame, sizeof(*frame), 0);
}
#endif

static inline void __save_stack_trace_user(struct stack_trace *trace)
{
	const struct pt_regs *regs = task_pt_regs(current);
	const void __user *fp = (const void __user *)regs->bp;

	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = regs->ip;

	while (trace->nr_entries < trace->max_entries) {
		struct stack_frame_user frame;

		frame.next_fp = NULL;
		frame.ret_addr = 0;
		if (!copy_stack_frame(fp, &frame))
			break;
		if ((unsigned long)fp < regs->sp)
			break;
		if (frame.ret_addr) {
			trace->entries[trace->nr_entries++] =
				frame.ret_addr;
		}
		if (fp == frame.next_fp)
			break;
		fp = frame.next_fp;
	}
}

void save_stack_trace_user(struct stack_trace *trace)
{
	/*
	 * Trace user stack if we are not a kernel thread
	 */
	if (current->mm) {
		__save_stack_trace_user(trace);
	}
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}

static inline void __save_stack_trace_user_task(struct task_struct *task,
		struct stack_trace *trace)
{
	const struct pt_regs *regs = task_pt_regs(task);
	const void __user *fp;
	unsigned long addr;
#ifdef CONFIG_SMP
	if (task != current && task->state == TASK_RUNNING && task->on_cpu) {
		/* To trap into kernel at least once */
		smp_send_reschedule(task_cpu(task));
	}
#endif
	fp = (const void __user *)regs->bp;
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = regs->ip;

	while (trace->nr_entries < trace->max_entries) {
		struct stack_frame_user frame;

		frame.next_fp = NULL;
		frame.ret_addr = 0;

		addr = (unsigned long)fp;
		if (!access_frame(task, addr, &frame))
			break;
		if ((unsigned long)fp < regs->sp)
			break;
		if (frame.ret_addr) {
			trace->entries[trace->nr_entries++] =
				frame.ret_addr;
		}
		if (fp == frame.next_fp)
			break;
		fp = frame.next_fp;
	}
}

void save_stack_trace_user_task(struct task_struct *task,
		struct stack_trace *trace)
{
	if (task == current || !task) {
		save_stack_trace_user(trace);
		return;
	}

	if (task->mm)
		__save_stack_trace_user_task(task, trace);

	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}
EXPORT_SYMBOL_GPL(save_stack_trace_user_task);

