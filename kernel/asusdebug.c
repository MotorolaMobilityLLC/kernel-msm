/* //20100930 jack_wong for asus debug mechanisms +++++
 *  asusdebug.c
 * //20100930 jack_wong for asus debug mechanisms -----
 *
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/rtc.h>
#include <linux/list.h>
//#include <linux/asus_ver.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/export.h>
extern int g_user_dbg_mode;

#include <linux/rtc.h>
#include "rtmutex_common.h"

#include <linux/delay.h>
#include <linux/slab.h>

/* BSP++ */
#include <linux/syslog.h>

/* Must be the same with the config of qcom,asusdebug in *.dtsi */
unsigned int PRINTK_BUFFER = 0x11F00000;
unsigned int RTB_BUFFER = 0x11F00000 + SZ_1M;

extern struct timezone sys_tz;
#define RT_MUTEX_HAS_WAITERS	1UL
#define RT_MUTEX_OWNER_MASKALL	1UL

struct mutex fake_mutex;
struct completion fake_completion;
struct rt_mutex fake_rtmutex;
static struct workqueue_struct *ASUSEvtlog_workQueue;

int asus_rtc_read_time(struct rtc_time *tm)
{
    struct timespec ts; 
    
    getnstimeofday(&ts);
    ts.tv_sec -= sys_tz.tz_minuteswest * 60;
    rtc_time_to_tm(ts.tv_sec, tm);
    printk("[adbg] now %04d%02d%02d-%02d%02d%02d, tz=%d\r\n", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, sys_tz.tz_minuteswest);
    return 0; 
}
EXPORT_SYMBOL(asus_rtc_read_time);
#if 1   
//--------------------   debug message logger   ------------------------------------
struct workqueue_struct *ASUSDebugMsg_workQueue;
EXPORT_SYMBOL(ASUSDebugMsg_workQueue);
//--------------  phone hang log part  --------------------------------------------------
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
//                             all thread information
///////////////////////////////////////////////////////////////////////////////////////////////////


#if 0

#define save_log    printk

#else

static char* g_phonehang_log;
static int g_iPtr = 0;
int save_log(const char *f, ...)
{
    va_list args;
    int len;

    if (g_iPtr < PHONE_HANG_LOG_SIZE) 
    {
        va_start(args, f);
        len = vsnprintf(g_phonehang_log + g_iPtr, PHONE_HANG_LOG_SIZE - g_iPtr, f, args);
        va_end(args);
        //printk("%s", g_phonehang_log + g_iPtr);
        if (g_iPtr < PHONE_HANG_LOG_SIZE) 
        {
            g_iPtr += len;
            return 0;
        }
    }
    g_iPtr = PHONE_HANG_LOG_SIZE;
    return -1;
}
#endif
#if 0
#define TASK_RUNNING        0
#define TASK_INTERRUPTIBLE  1
#define TASK_UNINTERRUPTIBLE    2
#define __TASK_STOPPED      4
#define __TASK_TRACED       8
/* in tsk->exit_state */
#define EXIT_ZOMBIE     16
#define EXIT_DEAD       32
/* in tsk->state again */
#define TASK_DEAD       64
#define TASK_WAKEKILL       128
#define TASK_WAKING     256
#endif

static char *task_state_array[] = {
    "RUNNING",      /*  0 */
    "INTERRUPTIBLE",     /*  1 */
    "UNINTERRUPTIB",   /*  2 */
    "STOPPED",      /*  4 */
    "TRACED", /*  8 */
    "EXIT ZOMBIE",       /* 16 */
    "EXIT DEAD",      /* 32 */
    "DEAD",      /* 64 */
    "WAKEKILL",      /* 128 */
    "WAKING"      /* 256 */
};
struct thread_info_save;
struct thread_info_save
{
    struct task_struct *pts;
    pid_t pid;
    u64 sum_exec_runtime;
    u64 vruntime;
    struct thread_info_save* pnext;
};
static char * print_state(long state)
{   
    int i;
    if(state == 0)
        return task_state_array[0];
    for(i = 1; i <= 16; i++)
    {
        if(1<<(i-1) & state)
            return task_state_array[i];
    }
    return "NOTFOUND";
    
}

/*
 * Ease the printing of nsec fields:
 */
static long long nsec_high(unsigned long long nsec)
{
    if ((long long)nsec < 0) {
        nsec = -nsec;
        do_div(nsec, 1000000);
        return -nsec;
    }
    do_div(nsec, 1000000);

    return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
    unsigned long long nsec1;
    if ((long long)nsec < 0)
        nsec = -nsec;

    nsec1 =  do_div(nsec, 1000000);
    return do_div(nsec1, 1000000);
}
#define MAX_STACK_TRACE_DEPTH   64
struct stack_trace {
    unsigned int nr_entries, max_entries;
    unsigned long *entries;
    int skip;   /* input argument: How many entries to skip */
};

struct stack_trace_data {
    struct stack_trace *trace;
    unsigned int no_sched_functions;
    unsigned int skip;
};

struct stackframe {
    unsigned long fp;
    unsigned long sp;
    unsigned long lr;
    unsigned long pc;
};
int unwind_frame(struct stackframe *frame);
void notrace walk_stackframe(struct stackframe *frame,
             int (*fn)(struct stackframe *, void *), void *data);

void save_stack_trace_asus(struct task_struct *tsk, struct stack_trace *trace);
void show_stack1(struct task_struct *p1, void *p2)
{
    struct stack_trace trace;
    unsigned long *entries;
    int i;

    entries = kmalloc(MAX_STACK_TRACE_DEPTH * sizeof(*entries), GFP_KERNEL);
    if (!entries)
    {
        printk("[adbg] entries malloc failure\n");
        return;
    }
    trace.nr_entries    = 0;
    trace.max_entries   = MAX_STACK_TRACE_DEPTH;
    trace.entries       = entries;
    trace.skip      = 0;
    save_stack_trace_asus(p1, &trace);

    for (i = 0; i < trace.nr_entries; i++) 
    {
        save_log("[<%p>] %pS\n", (void *)entries[i], (void *)entries[i]);
    }
    kfree(entries);
}


#define SPLIT_NS(x) nsec_high(x), nsec_low(x)
void print_all_thread_info(void)
{
    struct task_struct *pts;
    struct thread_info *pti;
    struct rtc_time tm;
    asus_rtc_read_time(&tm);    
    
    #if 1
    g_phonehang_log = (char*)PHONE_HANG_LOG_BUFFER;//phys_to_virt(PHONE_HANG_LOG_BUFFER);
    g_iPtr = 0;
    memset(g_phonehang_log, 0, PHONE_HANG_LOG_SIZE);
    #endif
    
    save_log("PhoneHang-%04d%02d%02d-%02d%02d%02d.txt  ---  ASUS_SW_VER : %s----------------------------------------------\r\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ASUS_SW_VER);
    save_log(" pID----ppID----NAME----------------SumTime---vruntime--SPri-NPri-State----------PmpCnt-Binder----Waiting\r\n");

    for_each_process(pts)
    {
        pti = (struct thread_info *)((int)(pts->stack)  & ~(THREAD_SIZE - 1));
        save_log("-----------------------------------------------------\r\n");
        save_log(" %-7d", pts->pid);
        
        if(pts->parent)
            save_log("%-8d", pts->parent->pid);
        else
            save_log("%-8d", 0);
        
        save_log("%-20s", pts->comm);
        save_log("%lld.%06ld", SPLIT_NS(pts->se.sum_exec_runtime));
        save_log("     %lld.%06ld     ", SPLIT_NS(pts->se.vruntime));
        save_log("%-5d", pts->static_prio);
        save_log("%-5d", pts->normal_prio);
        save_log("%-15s", print_state((pts->state & TASK_REPORT) | pts->exit_state));
        
#ifndef ASUS_SHIP_BUILD        
//        save_log("%-6d", pts->binder_call_to_proc_pid);    
#endif
        save_log("%-6d", pti->preempt_count);    
        
        
        if(pti->pWaitingMutex != &fake_mutex && pti->pWaitingMutex != NULL)
        {
			if (pti->pWaitingMutex->name)
			{
				save_log("    Mutex:%s,", pti->pWaitingMutex->name + 1);
				printk("    Mutex:%s,", pti->pWaitingMutex->name + 1);
			}
			else
				printk("pti->pWaitingMutex->name == NULL\r\n");

//ASUS_BSP ++				
			if (pti->pWaitingMutex->mutex_owner_asusdebug)
			{
				save_log(" Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
				printk(" Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
			}
			else
				printk("pti->pWaitingMutex->mutex_owner_asusdebug == NULL\r\n");
				
			if (pti->pWaitingMutex->mutex_owner_asusdebug->comm)
			{
				save_log(" %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
				printk(" %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
			}
			else
				printk("pti->pWaitingMutex->mutex_owner_asusdebug->comm == NULL\r\n");
//ASUS_BSP --
		}

		if(pti->pWaitingCompletion != &fake_completion && pti->pWaitingCompletion!=NULL)
		{
			if (pti->pWaitingCompletion->name)
	            save_log("    Completion:wait_for_completion %s", pti->pWaitingCompletion->name );
	        else
				printk("pti->pWaitingCompletion->name == NULL\r\n");
		}

        if(pti->pWaitingRTMutex != &fake_rtmutex && pti->pWaitingRTMutex != NULL)
        {
			struct task_struct *temp = rt_mutex_owner(pti->pWaitingRTMutex);
			if (temp)
				save_log("    RTMutex: Owned by pID(%d)", temp->pid);
            else
				printk("pti->pWaitingRTMutex->temp == NULL\r\n");
			if (temp->comm)
				save_log(" %s", temp->pid, temp->comm);
			else
				printk("pti->pWaitingRTMutex->temp->comm == NULL\r\n");
		}

	save_log("\r\n");
        show_stack1(pts, NULL);
        
        save_log("\r\n");
        
        if(!thread_group_empty(pts))
        {
            struct task_struct *p1 = next_thread(pts);
            do
            {
                pti = (struct thread_info *)((int)(p1->stack)  & ~(THREAD_SIZE - 1));
                save_log(" %-7d", p1->pid);
                
                if(pts->parent)
                    save_log("%-8d", p1->parent->pid);
                else
                    save_log("%-8d", 0);
                
                save_log("%-20s", p1->comm);
                save_log("%lld.%06ld", SPLIT_NS(p1->se.sum_exec_runtime));
                save_log("     %lld.%06ld     ", SPLIT_NS(p1->se.vruntime));
                save_log("%-5d", p1->static_prio);
                save_log("%-5d", p1->normal_prio);
                save_log("%-15s", print_state((p1->state & TASK_REPORT) | p1->exit_state));
#ifndef ASUS_SHIP_BUILD                
//                save_log("%-6d", pts->binder_call_to_proc_pid);    
#endif                
                save_log("%-6d", pti->preempt_count);    
                
        if(pti->pWaitingMutex != &fake_mutex && pti->pWaitingMutex != NULL)
        {
			if (pti->pWaitingMutex->name)
			{
				save_log("    Mutex:%s,", pti->pWaitingMutex->name + 1);
				printk("    Mutex:%s,", pti->pWaitingMutex->name + 1);
			}
			else
				printk("pti->pWaitingMutex->name == NULL\r\n");

//ASUS_BSP ++
			if (pti->pWaitingMutex->mutex_owner_asusdebug)
			{
				save_log(" Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
				printk(" Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
			}
			else
				printk("pti->pWaitingMutex->mutex_owner_asusdebug == NULL\r\n");
				
			if (pti->pWaitingMutex->mutex_owner_asusdebug->comm)
			{
				save_log(" %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
				printk(" %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
			}
			else
				printk("pti->pWaitingMutex->mutex_owner_asusdebug->comm == NULL\r\n");
//ASUS_BSP --
		}

		if(pti->pWaitingCompletion != &fake_completion && pti->pWaitingCompletion!=NULL)
		{
			if (pti->pWaitingCompletion->name)
	            save_log("    Completion:wait_for_completion %s", pti->pWaitingCompletion->name );
	        else
				printk("pti->pWaitingCompletion->name == NULL\r\n");
		}
   
        if(pti->pWaitingRTMutex != &fake_rtmutex && pti->pWaitingRTMutex != NULL)
        {
			struct task_struct *temp = rt_mutex_owner(pti->pWaitingRTMutex);
			if (temp)
				save_log("    RTMutex: Owned by pID(%d)", temp->pid);
            else
				printk("pti->pWaitingRTMutex->temp == NULL\r\n");
			if (temp->comm)
				save_log(" %s", temp->pid, temp->comm);
			else
				printk("pti->pWaitingRTMutex->temp->comm == NULL\r\n");
		}
                save_log("\r\n");
                show_stack1(p1, NULL);
                
                save_log("\r\n");
                p1 = next_thread(p1);
            }while(p1 != pts);
        }
        save_log("-----------------------------------------------------\r\n\r\n\r\n");
        
    }
    save_log("\r\n\r\n\r\n\r\n");
    
    
    #if 1
    //iounmap(g_phonehang_log);
    #endif
}

struct thread_info_save *ptis_head = NULL;
int find_thread_info(struct task_struct *pts, int force)
{
    struct thread_info *pti;
    struct thread_info_save *ptis, *ptis_ptr;
    u64 vruntime = 0, sum_exec_runtime;
    
    if(ptis_head != NULL)
    {
        ptis = ptis_head->pnext;
        ptis_ptr = NULL;
        //printk("initial ptis %x,\n\r", ptis);
        while(ptis)
        {
            //printk("initial ptis->pts %x, pts=%x\n\r", ptis->pts, pts);
            if(ptis->pid == pts->pid && ptis->pts == pts)
            {
                //printk("found pts=%x\n\r", pts);
                ptis_ptr = ptis; 
                break; 
            }
            ptis = ptis->pnext;
        }
        //printk("ptis_ptr=%x\n\r", ptis_ptr);
        if(ptis_ptr)
        {
            //printk("found ptis->pid=%d, sum_exec_runtime  new:%lld.%06ld  old:%lld.%06ld \n\r", ptis->pid, SPLIT_NS(pts->se.sum_exec_runtime), SPLIT_NS(ptis->sum_exec_runtime));
            sum_exec_runtime = pts->se.sum_exec_runtime - ptis->sum_exec_runtime;
            //printk("difference=%lld.%06ld  \n\r", SPLIT_NS(sum_exec_runtime));
        }
        else
        {
            sum_exec_runtime = pts->se.sum_exec_runtime;
        }
        //printk("utime=%d, stime=%d\n\r", utime, stime);
        if(sum_exec_runtime > 0 || force)
        {
            pti = (struct thread_info *)((int)(pts->stack)  & ~(THREAD_SIZE - 1));
            save_log(" %-7d", pts->pid);
            
            if(pts->parent)
                save_log("%-8d", pts->parent->pid);
            else
                save_log("%-8d", 0);
            
            save_log("%-20s", pts->comm);
            save_log("%lld.%06ld", SPLIT_NS(sum_exec_runtime));
            if(nsec_high(sum_exec_runtime) > 1000)
                save_log(" ******");
            save_log("     %lld.%06ld     ", SPLIT_NS(vruntime));            
            save_log("%-5d", pts->static_prio);
            save_log("%-5d", pts->normal_prio);
            save_log("%-15s", print_state(pts->state));
            save_log("%-6d", pti->preempt_count);    
            
        if(pti->pWaitingMutex != &fake_mutex && pti->pWaitingMutex != NULL)
        {
			if (pti->pWaitingMutex->name)
			{
				save_log("    Mutex:%s,", pti->pWaitingMutex->name + 1);
				printk("    Mutex:%s,", pti->pWaitingMutex->name + 1);
			}
			else
				printk("pti->pWaitingMutex->name == NULL\r\n");

//ASUS_BSP ++				
			if (pti->pWaitingMutex->mutex_owner_asusdebug)
			{
				save_log(" Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
				printk(" Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
			}
			else
				printk("pti->pWaitingMutex->mutex_owner_asusdebug == NULL\r\n");
				
			if (pti->pWaitingMutex->mutex_owner_asusdebug->comm)
			{
				save_log(" %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
				printk(" %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
			}
			else
				printk("pti->pWaitingMutex->mutex_owner_asusdebug->comm == NULL\r\n");
//ASUS_BSP --
		}

            if(pti->pWaitingCompletion != &fake_completion && pti->pWaitingCompletion!=NULL)
			{
				if (pti->pWaitingCompletion->name)
					save_log("    Completion:wait_for_completion %s", pti->pWaitingCompletion->name );
				else
					printk("pti->pWaitingCompletion->name == NULL\r\n");
			}
    
        if(pti->pWaitingRTMutex != &fake_rtmutex && pti->pWaitingRTMutex != NULL)
        {
			struct task_struct *temp = rt_mutex_owner(pti->pWaitingRTMutex);
			if (temp)
				save_log("    RTMutex: Owned by pID(%d)", temp->pid);
            else
				printk("pti->pWaitingRTMutex->temp == NULL\r\n");
			if (temp->comm)
				save_log(" %s", temp->pid, temp->comm);
			else
				printk("pti->pWaitingRTMutex->temp->comm == NULL\r\n");
		}
           
            save_log("\r\n");
            
            show_stack1(pts, NULL);
            save_log("\r\n");
        }
        else 
            return 0;
    }
    return 1;    

}

struct worker {
	/* on idle list while idle, on busy hash table while busy */
	union {
		struct list_head	entry;	/* L: while idle */
		struct hlist_node	hentry;	/* L: while busy */
	};

	struct work_struct	*current_work;	/* L: work being processed */
	int *current_cwq; /* L: current_work's cwq */
	struct list_head	scheduled;	/* L: scheduled works */
	struct task_struct	*task;		/* I: worker task */	
};

void save_all_thread_info(void)
{
    struct task_struct *pts;
    struct thread_info *pti;
    struct thread_info_save *ptis = NULL, *ptis_ptr = NULL;
#ifndef ASUS_SHIP_BUILD
//    struct worker *pworker ;
#endif
    
    struct rtc_time tm;

    asus_rtc_read_time(&tm);    
    #if 1
    g_phonehang_log = (char*)PHONE_HANG_LOG_BUFFER;//phys_to_virt(PHONE_HANG_LOG_BUFFER);
    g_iPtr = 0;
    memset(g_phonehang_log, 0, PHONE_HANG_LOG_SIZE);
    #endif
    
    save_log("ASUSSlowg-%04d%02d%02d-%02d%02d%02d.txt  ---  ASUS_SW_VER : %s----------------------------------------------\r\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ASUS_SW_VER);
    save_log(" pID----ppID----NAME----------------SumTime---vruntime--SPri-NPri-State----------PmpCnt-binder----Waiting\r\n");
    

    if(ptis_head != NULL)
    {
        struct thread_info_save *ptis_next = ptis_head->pnext;
        struct thread_info_save *ptis_next_next;
        while(ptis_next)
        {
            ptis_next_next = ptis_next->pnext;
            kfree(ptis_next);
            ptis_next = ptis_next_next;
        }
        kfree(ptis_head);
        ptis_head = NULL;
    }
    
    if(ptis_head == NULL)
    {
        ptis_ptr = ptis_head = kmalloc(sizeof( struct thread_info_save), GFP_KERNEL);
        if(!ptis_head)
        {
            printk("[adbg] kmalloc ptis_head failure\n"); 
            return;
        }
        memset(ptis_head, 0, sizeof( struct thread_info_save));
    }    
    
    for_each_process(pts)
    {
        pti = (struct thread_info *)((int)(pts->stack)  & ~(THREAD_SIZE - 1));
        //printk("for pts %x, ptis_ptr=%x\n\r", pts, ptis_ptr);
        ptis = kmalloc(sizeof( struct thread_info_save), GFP_KERNEL);
        if(!ptis)
        {
            printk("[adbg] kmalloc ptis failure\n"); 
            return;        
        }
        memset(ptis, 0, sizeof( struct thread_info_save)); 
        
        save_log("-----------------------------------------------------\r\n");
        save_log(" %-7d", pts->pid);
        if(pts->parent)
            save_log("%-8d", pts->parent->pid);
        else
            save_log("%-8d", 0);
        
        save_log("%-20s", pts->comm);
        save_log("%lld.%06ld", SPLIT_NS(pts->se.sum_exec_runtime));
        save_log("     %lld.%06ld     ", SPLIT_NS(pts->se.vruntime));
        save_log("%-5d", pts->static_prio);
        save_log("%-5d", pts->normal_prio);
        save_log("%-15s", print_state((pts->state & TASK_REPORT) | pts->exit_state));
#ifndef ASUS_SHIP_BUILD        
//        save_log("call_proc:%-6d  ", pts->binder_call_to_proc_pid);    
 //       save_log("call_thread:%-6d  ", pts->binder_call_to_thread_pid); 
 //       save_log("call_code:%-6d  ", pts->binder_call_code); 
#endif
        save_log("%-6d", pti->preempt_count);    
        
        if(pti->pWaitingMutex != &fake_mutex && pti->pWaitingMutex != NULL)
        {
			if (pti->pWaitingMutex->name)
			{
				save_log("    Mutex:%s,", pti->pWaitingMutex->name + 1);
				printk("    Mutex:%s,", pti->pWaitingMutex->name + 1);
			}
			else
				printk("pti->pWaitingMutex->name == NULL\r\n");

//ASUS_BSP ++				
			if (pti->pWaitingMutex->mutex_owner_asusdebug)
			{
				save_log(" Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
				printk(" Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
			}
			else
				printk("pti->pWaitingMutex->mutex_owner_asusdebug == NULL\r\n");
				
			if (pti->pWaitingMutex->mutex_owner_asusdebug->comm)
			{
				save_log(" %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
				printk(" %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
			}
			else
				printk("pti->pWaitingMutex->mutex_owner_asusdebug->comm == NULL\r\n");
//ASUS_BSP --
		}
	if(pti->pWaitingCompletion != &fake_completion && pti->pWaitingCompletion!=NULL)
	{
		if (pti->pWaitingCompletion->name)
			save_log("    Completion:wait_for_completion %s", pti->pWaitingCompletion->name );
		else
			printk("pti->pWaitingCompletion->name == NULL\r\n");
	}

        if(pti->pWaitingRTMutex != &fake_rtmutex && pti->pWaitingRTMutex != NULL)
        {
			struct task_struct *temp = rt_mutex_owner(pti->pWaitingRTMutex);
			if (temp)
				save_log("    RTMutex: Owned by pID(%d)", temp->pid);
            else
				printk("pti->pWaitingRTMutex->temp == NULL\r\n");
			if (temp->comm)
				save_log(" %s", temp->pid, temp->comm);
			else
				printk("pti->pWaitingRTMutex->temp->comm == NULL\r\n");
		}
		
#if 0//ndef ASUS_SHIP_BUILD
	if ( strncmp(pts->comm, "kworker", strlen("kworker")) ==0 )
	{
		 pworker = (struct worker *) pts->pworker;

		 printk("pworker=%p\r\n", pworker);

		if ( pworker->current_work != NULL )
		{
			char fname[KSYM_SYMBOL_LEN] = {0};
			sprint_symbol(fname, (unsigned long)pworker->current_work->func);
			save_log("%s", fname);
			printk("pworker name = %s\r\n", fname);
		}
	}
#endif

        save_log("\r\n");
        show_stack1(pts, NULL);
        
        save_log("\r\n");
         

        ptis->pid = pts->pid;
        ptis->pts = pts;
        ptis->sum_exec_runtime = pts->se.sum_exec_runtime;
        //printk("saving %d, sum_exec_runtime  %lld.%06ld  \n\r", ptis->pid, SPLIT_NS(ptis->sum_exec_runtime));
        ptis->vruntime = pts->se.vruntime;  
        //printk("newing ptis %x utime=%d, stime=%d\n\r", ptis, pts->utime, pts->stime);          
          
        ptis_ptr->pnext = ptis;
        ptis_ptr = ptis;
        
        if(!thread_group_empty(pts))
        {
            struct task_struct *p1 = next_thread(pts);
            do
            {
                pti = (struct thread_info *)((int)(p1->stack)  & ~(THREAD_SIZE - 1));
                //printk("for pts %x, ptis_ptr=%x\n\r", pts, ptis_ptr);
                ptis = kmalloc(sizeof( struct thread_info_save), GFP_KERNEL);
                if(!ptis)
                {
                    printk("[adbg] kmalloc ptis 2 failure\n"); 
                    return;        
                }
                memset(ptis, 0, sizeof( struct thread_info_save)); 
        
                ptis->pid = p1->pid;
                ptis->pts = p1;
                ptis->sum_exec_runtime = p1->se.sum_exec_runtime;
                //printk("saving %d, sum_exec_runtime  %lld.%06ld  \n\r", ptis->pid, SPLIT_NS(ptis->sum_exec_runtime));
                ptis->vruntime = p1->se.vruntime;  
                //printk("newing ptis %x utime=%d, stime=%d\n\r", ptis, pts->utime, pts->stime);          
                
                ptis_ptr->pnext = ptis;
                ptis_ptr = ptis;
                save_log(" %-7d", p1->pid);
                
                if(pts->parent)
                    save_log("%-8d", p1->parent->pid);
                else
                    save_log("%-8d", 0);
                
                save_log("%-20s", p1->comm);
                save_log("%lld.%06ld", SPLIT_NS(p1->se.sum_exec_runtime));
                save_log("     %lld.%06ld     ", SPLIT_NS(p1->se.vruntime));
                save_log("%-5d", p1->static_prio);
                save_log("%-5d", p1->normal_prio);
                save_log("%-15s", print_state((p1->state & TASK_REPORT) | p1->exit_state));
#ifndef ASUS_SHIP_BUILD        
//                save_log("call_proc:%-6d  ", pts->binder_call_to_proc_pid);    
 //               save_log("call_thread:%-6d  ", pts->binder_call_to_thread_pid); 
 //               save_log("call_code:%-6d  ", pts->binder_call_code); 
#endif
                save_log("%-6d", pti->preempt_count);    
                
        if(pti->pWaitingMutex != &fake_mutex && pti->pWaitingMutex != NULL)
        {
			if (pti->pWaitingMutex->name)
			{
				save_log("    Mutex:%s,", pti->pWaitingMutex->name + 1);
				printk("    Mutex:%s,", pti->pWaitingMutex->name + 1);
			}
			else
				printk("pti->pWaitingMutex->name == NULL\r\n");
				
//ASUS_BSP ++
			if (pti->pWaitingMutex->mutex_owner_asusdebug)
			{
				save_log(" Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
				printk(" Owned by pID(%d)", pti->pWaitingMutex->mutex_owner_asusdebug->pid);
			}
			else
				printk("pti->pWaitingMutex->mutex_owner_asusdebug == NULL\r\n");
				
			if (pti->pWaitingMutex->mutex_owner_asusdebug->comm)
			{
				save_log(" %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
				printk(" %s",pti->pWaitingMutex->mutex_owner_asusdebug->comm);
			}
			else
				printk("pti->pWaitingMutex->mutex_owner_asusdebug->comm == NULL\r\n");
//ASUS_BSP --
		}

		if(pti->pWaitingCompletion != &fake_completion && pti->pWaitingCompletion!=NULL)
		{
			if (pti->pWaitingCompletion->name)
	            save_log("    Completion:wait_for_completion %s", pti->pWaitingCompletion->name );
	        else
				printk("pti->pWaitingCompletion->name == NULL\r\n");
		}
  
        if(pti->pWaitingRTMutex != &fake_rtmutex && pti->pWaitingRTMutex != NULL)
        {
			struct task_struct *temp = rt_mutex_owner(pti->pWaitingRTMutex);
			if (temp)
				save_log("    RTMutex: Owned by pID(%d)", temp->pid);
            else
				printk("pti->pWaitingRTMutex->temp == NULL\r\n");
			if (temp->comm)
				save_log(" %s", temp->pid, temp->comm);
			else
				printk("pti->pWaitingRTMutex->temp->comm == NULL\r\n");
		}
                save_log("\r\n");
                show_stack1(p1, NULL);
                
                save_log("\r\n");
                
                p1 = next_thread(p1);
            }while(p1 != pts);
        }        
        
        
    }

}

EXPORT_SYMBOL(save_all_thread_info);

void delta_all_thread_info(void)
{
    struct task_struct *pts;
    int ret = 0, ret2 = 0;

    pr_info("%s()++\n", __func__);

    save_log("\r\nDELTA INFO----------------------------------------------------------------------------------------------\r\n");
    save_log(" pID----ppID----NAME----------------SumTime---vruntime--SPri-NPri-State----------PmpCnt----Waiting\r\n");
    for_each_process(pts)
    {
        //printk("for pts %x,\n\r", pts);
        ret = find_thread_info(pts, 0);
        if(!thread_group_empty(pts))
        {
            struct task_struct *p1 = next_thread(pts);
            ret2 = 0;
            do
            {
                ret2 += find_thread_info(p1, 0);
                p1 = next_thread(p1);
            }while(p1 != pts);
            if(ret2 && !ret)
                find_thread_info(pts, 1);        
        }    
        if(ret || ret2)          
            save_log("-----------------------------------------------------\r\n\r\n-----------------------------------------------------\r\n");
    }
    save_log("\r\n\r\n\r\n\r\n");

    pr_info("%s()--\n", __func__);
}
EXPORT_SYMBOL(delta_all_thread_info);
///////////////////////////////////////////////////////////////////////////////////////////////////
extern void printk_buffer_rebase(void);

extern unsigned char debug_mask_setting[ASUS_MSK_GROUP] ;

static int g_read_pos;

static int asusdebug_open(struct inode * inode, struct file * file)
{
    //printk("asusdebug_open\n");
    g_read_pos = 0;
    return 0;
}

static int asusdebug_release(struct inode * inode, struct file * file)
{
    //printk("asusdebug_release\n");
    return 0;
}

static ssize_t asusdebug_read(struct file *file, char __user *buf,
             size_t count, loff_t *ppos)
{
    char print_buf[32];
    int ret, iret;
    //printk("asusdebug_read %d\n", g_read_pos);
    if(g_read_pos < ASUS_MSK_GROUP)
    {
        sprintf(print_buf, "group %d: %02x\n", g_read_pos, debug_mask_setting[g_read_pos]);
        /* Transfering data to user space */ 
        ret = strlen(print_buf);
        iret = copy_to_user(buf, print_buf, ret);
        g_read_pos ++;
        //printk(print_buf);
        return ret;
    }
    else
        return 0;
}
static mm_segment_t oldfs;
static void initKernelEnv(void)
{
    oldfs = get_fs();
    set_fs(KERNEL_DS);
}

static void deinitKernelEnv(void)
{
    set_fs(oldfs);
}
char messages[256];
char messages_unparsed[256];

void save_phone_hang_log(void)
{
    int file_handle;
    int ret;

    pr_info("%s()++\n", __func__);

    //---------------saving phone hang log if any -------------------------------
    g_phonehang_log = (char*)PHONE_HANG_LOG_BUFFER;// phys_to_virt(PHONE_HANG_LOG_BUFFER);
    printk("[adbg] save_phone_hang_log PRINTK_BUFFER=%x, PRINTK_BUFFER=PHONE_HANG_LOG_BUFFE=%x \n", PRINTK_BUFFER, PHONE_HANG_LOG_BUFFER);
    //printk("save_phone_hang_log %c%c%c%c%c%c%c%c%c, strncmp(g_phonehang_log, ASUSSlowg, 9)=%d\n",g_phonehang_log[0],g_phonehang_log[1],g_phonehang_log[2],g_phonehang_log[3],g_phonehang_log[4],g_phonehang_log[5],g_phonehang_log[6],g_phonehang_log[7],g_phonehang_log[8], strncmp(g_phonehang_log, "ASUSSlowg", 9));
    if(g_phonehang_log && ((strncmp(g_phonehang_log, "PhoneHang", 9) == 0) || (strncmp(g_phonehang_log, "ASUSSlowg", 9) == 0)) )
    {
        printk("[adbg] save_phone_hang_log-1\n");
        initKernelEnv();
        memset(messages, 0, sizeof(messages));
        strcpy(messages, "/asdf/");
        strncat(messages, g_phonehang_log, 29);
        file_handle = sys_open(messages, O_CREAT|O_WRONLY|O_SYNC, 0);
        printk("[adbg] save_phone_hang_log-2 file_handle %d, name=%s\n", file_handle, messages);
        if(!IS_ERR((const void *)file_handle))
        {
            ret = sys_write(file_handle, (unsigned char*)g_phonehang_log, strlen(g_phonehang_log));
            sys_close(file_handle);
        }
        deinitKernelEnv();
        
    }
    if(g_phonehang_log)
    {
        g_phonehang_log[0] = 0;   
        //iounmap(g_phonehang_log);
    }

    pr_info("%s()--\n", __func__);
}
EXPORT_SYMBOL(save_phone_hang_log);

/* ASUS_BSP ++++ Josh_Hsu: Add for support parse asdf log */
/*
 * parse_last_shutdown_log will convert struct log to readable text
 * @char* buf: a memory space used to store readable text
 */
int parse_last_shutdown_log(char* buf){
		return do_syslog(SYSLOG_ACTION_READ_KERNEL, buf, PRINTK_BUFFER_SLOT_SIZE, SYSLOG_FROM_PROC);
}
/* ASUS_BSP ---- Josh_Hsu: Add for support parse asdf log */

void save_last_shutdown_log(char* filename)
{
    char *last_shutdown_log;
	char *last_shutdown_log_unparsed;	// ASUS_BSP ++++ Josh_Hsu: Unparsed log pointer
	
    int parse_length;
    int file_handle;
	int file_handle_unparsed;
    unsigned long long t;
    unsigned long nanosec_rem;

    t = cpu_clock(0);
    nanosec_rem = do_div(t, 1000000000);

	pr_info("[adbg] %s()++\n", __func__);

	// Address setting
    last_shutdown_log_unparsed = (char*)PRINTK_BUFFER;
    sprintf(messages, "/asdf/LastShutdown_%lu.%06lu.txt", (unsigned long) t, nanosec_rem / 1000);
    printk("[adbg] %s(), messages: %s\n", __func__, messages);

	sprintf(messages_unparsed, "/asdf/LastShutdown_%lu.%06lu_unparsed.txt", (unsigned long) t, nanosec_rem / 1000);
	printk("[adbg] %s(), messages_unparsed: %s\n", __func__, messages_unparsed);

    initKernelEnv();

	// Save unparsed log first, in case parser cannnot work
    file_handle = sys_open(messages, O_CREAT|O_RDWR|O_SYNC, 0);
	file_handle_unparsed = sys_open(messages_unparsed, O_CREAT|O_RDWR|O_SYNC, 0);

	if(!IS_ERR((const void *)file_handle_unparsed))
    {
        sys_write(file_handle_unparsed, (unsigned char*)last_shutdown_log_unparsed, PRINTK_BUFFER_SLOT_SIZE);
        sys_close(file_handle_unparsed);
    } else {
        printk("[adbg] [ASDF] save_last_shutdown_error: [%d]\n", file_handle_unparsed);
    }

	// ASUS_BSP ++++ Josh_Hsu: Add for support parse asdf log
	// Save parsed log, using parser parse_last_shutdown_log
	last_shutdown_log = kmalloc(PRINTK_BUFFER_SLOT_SIZE, GFP_KERNEL);
	parse_length = parse_last_shutdown_log(last_shutdown_log);
	// ASUS_BSP ---- Josh_Hsu

    if(!IS_ERR((const void *)file_handle))
    {
        sys_write(file_handle, (unsigned char*)last_shutdown_log, parse_length);
        sys_close(file_handle);
    } else {
        printk("[adbg] [ASDF] save_last_shutdown_error: [%d]\n", file_handle);
    }

    deinitKernelEnv();          

    pr_info("[adbg] %s()--\n", __func__);

	kfree(last_shutdown_log); // ASUS_BSP ++++ Josh_Hsu: Release memory

}

//ASUS_BSP ++
#if 0
#if defined(CONFIG_MSM_RTB)
extern struct msm_rtb_state msm_rtb;

int g_saving_rtb_log = 1;
	
void save_rtb_log(void)
{
    char *rtb_log;
    char rtb_log_path[256] = {0};
    //~ struct rtc_time tm;
    int file_handle;
    unsigned long long t;
    unsigned long nanosec_rem;

    pr_info("%s()++\n", __func__);

    //~ asus_rtc_read_time(&tm);    
    rtb_log = (char*)msm_rtb.rtb;
    t = cpu_clock(0);
    nanosec_rem = do_div(t, 1000000000);
    snprintf(rtb_log_path, sizeof(rtb_log_path)-1, "/asdf/rtb_%lu.%06lu.bin",
        (unsigned long) t,      
        nanosec_rem / 1000);

    initKernelEnv();
    file_handle = sys_open(rtb_log_path, O_CREAT|O_RDWR|O_SYNC, 0);
    if(!IS_ERR((const void *)file_handle))
    {
        sys_write(file_handle, (unsigned char*)rtb_log, msm_rtb.size);
        sys_close(file_handle);
    } else {
		printk("[ASDF] save_rtb_log_error: [%d]\n", file_handle);
    }
    deinitKernelEnv();

    pr_info("%s()++\n", __func__);
}
#endif
#endif
//ASUS_BSP ++

typedef struct tzbsp_dump_cpu_ctx_s
{
    unsigned int mon_lr;
    unsigned int mon_spsr;
    unsigned int usr_r0;
    unsigned int usr_r1;
    unsigned int usr_r2;
    unsigned int usr_r3;
    unsigned int usr_r4;
    unsigned int usr_r5;
    unsigned int usr_r6;
    unsigned int usr_r7;
    unsigned int usr_r8;
    unsigned int usr_r9;
    unsigned int usr_r10;
    unsigned int usr_r11;
    unsigned int usr_r12;
    unsigned int usr_r13;
    unsigned int usr_r14;
    unsigned int irq_spsr;
    unsigned int irq_r13;
    unsigned int irq_r14;
    unsigned int svc_spsr;
    unsigned int svc_r13;
    unsigned int svc_r14;
    unsigned int abt_spsr;
    unsigned int abt_r13;
    unsigned int abt_r14;
    unsigned int und_spsr;
    unsigned int und_r13;
    unsigned int und_r14;
    unsigned int fiq_spsr;
    unsigned int fiq_r8;
    unsigned int fiq_r9;
    unsigned int fiq_r10;
    unsigned int fiq_r11;
    unsigned int fiq_r12;
    unsigned int fiq_r13;
    unsigned int fiq_r14;
    
} tzbsp_dump_cpu_ctx_t;

typedef struct tzbsp_dump_buf_s
{
    unsigned int sc_status[2];
    tzbsp_dump_cpu_ctx_t sc_ns[2];
    tzbsp_dump_cpu_ctx_t sec;
} tzbsp_dump_buf_t;

void save_last_watchdog_reg(void)
{
    tzbsp_dump_buf_t *last_watchdog_reg;
    struct rtc_time tm;
    int file_handle;
    
    asus_rtc_read_time(&tm);    
    last_watchdog_reg = (tzbsp_dump_buf_t*)PHONE_HANG_LOG_BUFFER - PRINTK_BUFFER_SLOT_SIZE / 2;//phys_to_virt((PHONE_HANG_LOG_BUFFER - PRINTK_BUFFER_SLOT_SIZE / 2));
    if(*((int*)last_watchdog_reg) != PRINTK_BUFFER_MAGIC)
    {
        sprintf(messages, "/asdf/%s_%04d%02d%02d-%02d%02d%02d.txt", "WatchdogReg", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    
        initKernelEnv();
        file_handle = sys_open(messages, O_CREAT|O_RDWR|O_SYNC, 0);
        if(!IS_ERR((const void *)file_handle))
        {
            sys_write(file_handle, (unsigned char*)last_watchdog_reg, sizeof(tzbsp_dump_buf_t));
            sys_close(file_handle);
        }
        deinitKernelEnv();          
    }
    memset(last_watchdog_reg, 0, sizeof(tzbsp_dump_buf_t));
    *((int*)last_watchdog_reg) = PRINTK_BUFFER_MAGIC;
}

void get_last_shutdown_log(void)
{
    unsigned int *last_shutdown_log_addr;

    last_shutdown_log_addr = (unsigned int *)((unsigned int)PRINTK_BUFFER + (unsigned int)PRINTK_BUFFER_SLOT_SIZE);

    printk("[adbg] get_last_shutdown_log: last_shutdown_log_addr=0x%08x, value=0x%08x\n",
        (unsigned int)last_shutdown_log_addr, *last_shutdown_log_addr);

    if( *last_shutdown_log_addr == (unsigned int)PRINTK_BUFFER_MAGIC)
    {
        save_last_shutdown_log("LastShutdown");
            //save_last_watchdog_reg();
    }

    printk_buffer_rebase();
    *last_shutdown_log_addr = PRINTK_BUFFER_MAGIC;  //ASUS_BSP ++
}
EXPORT_SYMBOL(get_last_shutdown_log);
int first = 0;
int watchdog_test = 0;
int asus_asdf_set = 0;

//extern int save_tz_log(void);  //adbg++

static ssize_t asusdebug_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    
    int group, value, ret;
    int file_handle;

	unsigned int *last_shutdown_log_addr;

// +++ ASUS_BSP : add for user build
#ifdef ASUS_SHIP_BUILD
	if (count > 256)
		count = 256;
	if (copy_from_user(messages, buf, count))
		return -EFAULT;
    if(strncmp(messages, "load", 4) == 0)
    {
        //printk("load!! filter opening\n");
        initKernelEnv();
        file_handle = sys_open(ASUS_DBG_FILTER_PATH, O_RDONLY, 0);
        
        if(!IS_ERR((const void *)file_handle))
        {
            //printk("filter opened %x\n", file_handle);
            ret = sys_read(file_handle, (unsigned char*)debug_mask_setting, 64);
            //printk("filter read, ret=%d\n", ret);
            sys_close(file_handle);
        }
        else
            printk("[adbg] !!! no filter found !!!\n");
        deinitKernelEnv();
        first = 1;
        return count;
        
    }
//ASUS_BSP ++
    else if(strncmp(messages, "dbg", 3) == 0)
    {
        g_user_dbg_mode = 1;
        printk("[adbg] Kernel dbg mode = %d\n", g_user_dbg_mode);
    }
    else if(strncmp(messages, "ndbg", 4) == 0)
    {
        g_user_dbg_mode = 0;
        printk("[adbg] Kernel dbg mode = %d\n", g_user_dbg_mode);
    }
    else if(strncmp(messages, "panic", 5) == 0)
    {
        panic("panic test");
    }
//ASUS_BSP --
	else if(strncmp(messages, "get_asdf_log", strlen("get_asdf_log")) == 0)
	{
		printk(KERN_WARNING "[ASDF] Now dumping ASDF last shutdown log\n");
#if 0
#ifdef CONFIG_MSM_RTB
			extern int g_saving_rtb_log;
#endif
#endif
            last_shutdown_log_addr = (unsigned int *)((unsigned int)PRINTK_BUFFER + (unsigned int)PRINTK_BUFFER_SLOT_SIZE);
            printk(KERN_WARNING "[ASDF] last_shutdown_log_addr=0x%08x, value=0x%08x\n", (unsigned int)last_shutdown_log_addr, *last_shutdown_log_addr);

            if(!asus_asdf_set)
            {
                asus_asdf_set = 1;
				save_phone_hang_log();
                get_last_shutdown_log();
                printk(KERN_WARNING "[ASDF] get_last_shutdown_log: last_shutdown_log_addr=0x%08x, value=0x%08x\n", (unsigned int)last_shutdown_log_addr, *last_shutdown_log_addr);
#ifdef CONFIG_MSM_RTB
                if ( (*last_shutdown_log_addr)==(unsigned int)PRINTK_BUFFER_MAGIC )
					//save_rtb_log();
#endif

				(*last_shutdown_log_addr)=(unsigned int)PRINTK_BUFFER_MAGIC;
            }
#if 0
#ifdef CONFIG_MSM_RTB
            g_saving_rtb_log = 0;
#endif
#endif
	}
    else if(strncmp(messages, "slowlog", 7) == 0)
    {
		printk("[adbg] start to gi chk, line:%d\n", __LINE__);
		save_all_thread_info();
		
		msleep(5 * 1000);
		
		printk("[adbg] start to gi delta, line:%d\n", __LINE__);
		delta_all_thread_info();
		save_phone_hang_log();
        return count;
    }	
	return count;
#endif  /* ASUS_SHIP_BUILD */
// --- ASUS_BSP : add for user build
   
    if (count > 256)
        count = 256;
    if (copy_from_user(messages, buf, count))
        return -EFAULT;

    if(strncmp(messages, "load", 4) == 0)
    {
        //printk("load!! filter opening\n");
        initKernelEnv();
        file_handle = sys_open(ASUS_DBG_FILTER_PATH, O_RDONLY, 0);
        
        if(!IS_ERR((const void *)file_handle))
        {
            //printk("filter opened %x\n", file_handle);
            ret = sys_read(file_handle, (unsigned char*)debug_mask_setting, 64);
            //printk("filter read, ret=%d\n", ret);
            sys_close(file_handle);
        }
        else
            printk("[adbg] !!! no filter found !!!\n");
        deinitKernelEnv();
        first = 1;
        return count;
        
    }
    else if(strncmp(messages, "dbg", 3) == 0)
    {
		g_user_dbg_mode = 1;
		printk("[adbg] Kernel dbg mode = %d\n", g_user_dbg_mode);
	}
	else if(strncmp(messages, "ndbg", 4) == 0)
    {
		g_user_dbg_mode = 0;
		printk("[adbg] Kernel dbg mode = %d\n", g_user_dbg_mode);
	}
    else if(strncmp(messages, "slowlog", 7) == 0)
    {
		printk("[adbg] start to gi chk, line:%d\n", __LINE__);
		save_all_thread_info();
		
		msleep(5 * 1000);
		
		printk("[adbg] start to gi delta, line:%d\n", __LINE__);
		delta_all_thread_info();
		save_phone_hang_log();
        return count;
    }
    else if(strncmp(messages, "watchdog_test", 13) == 0)
    {
		printk("[adbg] start watchdog test...\r\n");
		watchdog_test = 1;
	}    
	else if(strncmp(messages, "tzlog", 5) == 0)
    {
//        save_tz_log();  //adbg++
        return count;
    }
	else if(strncmp(messages, "gichk", 5) == 0)
    {
        save_all_thread_info();
        return count;
    }
    else if(strncmp(messages, "gidelta", 7) == 0)
    {
        delta_all_thread_info();
        //printk("gidelta\n");
        save_phone_hang_log();
        return count;
    }
    else if(strncmp(messages, "gi", 2) == 0)
    {
        print_all_thread_info();
        return count;
    }    
	else if(strncmp(messages, "get_lastshutdown_log", 20) == 0)
	{
        save_phone_hang_log();
	}
	else if(strncmp(messages, "get_asdf_log", strlen("get_asdf_log")) == 0)
	{
#if 0
#ifdef CONFIG_MSM_RTB
			extern int g_saving_rtb_log;
#endif
#endif
            last_shutdown_log_addr = (unsigned int *)((unsigned int)PRINTK_BUFFER + (unsigned int)PRINTK_BUFFER_SLOT_SIZE);
            printk("[ASDF] get_asdf_log last_shutdown_log_addr=0x%08x, value=0x%08x\n", (unsigned int)last_shutdown_log_addr, *last_shutdown_log_addr);

            if(!asus_asdf_set)
            {
                asus_asdf_set = 1;
				save_phone_hang_log();
                get_last_shutdown_log();
                printk("[ASDF] get_asdf_log get_last_shutdown_log: last_shutdown_log_addr=0x%08x, value=0x%08x\n", (unsigned int)last_shutdown_log_addr, *last_shutdown_log_addr);
#if 0
#ifdef CONFIG_MSM_RTB
                if ( (*last_shutdown_log_addr)==(unsigned int)PRINTK_BUFFER_MAGIC )
					//save_rtb_log();
#endif
#endif

				(*last_shutdown_log_addr)=(unsigned int)PRINTK_BUFFER_MAGIC;
            }
#if 0
#ifdef CONFIG_MSM_RTB
            g_saving_rtb_log = 0;
#endif
#endif
	}
	else if(strncmp(messages, "get_phonehang_log", 17) == 0)
	{
		//initKernelEnv();
		save_phone_hang_log();
		//deinitKernelEnv();		
	}	
	else if(strncmp(messages, "gichk", 5) == 0)
    {
        save_all_thread_info();
        return count;
    }
    else if(strncmp(messages, "gidelta", 7) == 0)
    {
        delta_all_thread_info();
        //printk("gidelta\n");
        save_phone_hang_log(); 
        return count;
    }
    else if(strncmp(messages, "gi", 2) == 0)
    {
        print_all_thread_info();
        return count;
    }    
    else if(strncmp(messages, "printk", 6) == 0)
    {
//        printk_lcd("TestAAA");
        die("test123", NULL, 0);
    } 
    else if(strncmp(messages, "panic", 5) == 0)
    {
//        printk_lcd("panic");
        panic("panic test");
    } 
    else if(strncmp(messages, "die", 3) == 0)
    {
//        printk_lcd("die");
        die("die test", NULL, 0);
    }         
    else if(strncmp(messages, "modem", 5) == 0)
    {
        #include <mach/subsystem_restart.h>
//        printk_lcd("TestAAA");
        subsystem_restart("external_modem");
    } 
 
    else
    {	
        sscanf(messages, "%d %02x", &group, &value);
        //printk("setting group %d mask %02x\n", group, value);

        if(group < 0 || group > ASUS_MSK_GROUP)
            return 0;
        if(value > 0xff)
            return 0;
        debug_mask_setting[group] = value;
        initKernelEnv();
        file_handle = sys_open(ASUS_DBG_FILTER_PATH, O_CREAT|O_RDWR|O_SYNC, 0);
        if(!IS_ERR((const void*)file_handle))
        {
            //printk("create filter successfully file_handle=%x\n", file_handle);
            ret = sys_write(file_handle, (unsigned char*)debug_mask_setting, 64);
            //printk("write done, ret=%d,\n", ret);
            
            sys_close(file_handle);
        }
        else
            printk("[adbg] ERROR! cannot create filter file");
        
        deinitKernelEnv();
        return count;
    }
	
	return count;
}


/////////////////////////////////////////////////////////////////////////////////////////////
////                  Eventlog mask mechanism
/////////////////////////////////////////////////////////////////////////////////////////////

                 
//struct asus_evtlog_work_data{
    //struct work_struct eventLog_Work;         
//    void *data;
//};                 

          
extern int suspend_in_progress;

static int g_hfileEvtlog = -MAX_ERRNO;
static int g_bEventlogEnable = 1;
static char g_Asus_Eventlog[ASUS_EVTLOG_MAX_ITEM][ASUS_EVTLOG_STR_MAXLEN];
static int g_Asus_Eventlog_read = 0;
static int g_Asus_Eventlog_write = 0;
//static spinlock_t spinlock_eventlog;

static void do_write_event_worker(struct work_struct *work);
static DECLARE_WORK(eventLog_Work, do_write_event_worker);

static struct mutex mA;
#define AID_SDCARD_RW 1015
static void do_write_event_worker(struct work_struct *work)
{
    char buffer[256];
    
    //struct asus_evtlog_work_data* my_work_data = container_of(work, struct asus_evtlog_work_data, eventLog_Work);
    
    while(first == 0 || suspend_in_progress)
    {
        printk("[adbg] waiting first\n");
        msleep(1000);
    }   
    
    if(IS_ERR((const void*)g_hfileEvtlog))
    {
        long size;
#if 0//def ASUS_SHIP_BUILD        
        g_hfileEvtlog = sys_open(ASUS_EVTLOG_PATH"_enable", O_RDONLY, 0);
        if(IS_ERR((const void*)g_hfileEvtlog))
            g_bEventlogEnable = 0;
        else
#endif            
        {
#if 0//def ASUS_SHIP_BUILD                    
            sys_close(g_hfileEvtlog);
#endif            
            
            g_hfileEvtlog = sys_open(ASUS_EVTLOG_PATH".txt", O_CREAT|O_RDWR|O_SYNC, 0666);
            if (g_hfileEvtlog < 0)
                printk("[adbg] 1. open %s failed, err:%d\n", ASUS_EVTLOG_PATH".txt", g_hfileEvtlog);

            sys_chown(ASUS_EVTLOG_PATH".txt", AID_SDCARD_RW, AID_SDCARD_RW);
            
            size = sys_lseek(g_hfileEvtlog, 0, SEEK_END);
            if(size >= SZ_2M)
            {        
                sys_close(g_hfileEvtlog); 
                sys_rmdir(ASUS_EVTLOG_PATH"_old.txt");
                sys_rename(ASUS_EVTLOG_PATH".txt", ASUS_EVTLOG_PATH"_old.txt");
                g_hfileEvtlog = sys_open(ASUS_EVTLOG_PATH".txt", O_CREAT|O_RDWR|O_SYNC, 0666);
                if (g_hfileEvtlog < 0)
                    printk("[adbg] 1. open %s failed during renaming old one, err:%d\n", ASUS_EVTLOG_PATH".txt", g_hfileEvtlog);
            }    
            sprintf(buffer, "\n\n---------------System Boot----%s---------\n", ASUS_SW_VER);
            printk("\n[adbg]------------System Boot----%s---------\n", ASUS_SW_VER);

            sys_write(g_hfileEvtlog, buffer, strlen(buffer));
            sys_close(g_hfileEvtlog);
        }
    }
    if(!IS_ERR((const void*)g_hfileEvtlog))
    {
        int str_len;
        char* pchar;
        long size;

        g_hfileEvtlog = sys_open(ASUS_EVTLOG_PATH".txt", O_CREAT|O_RDWR|O_SYNC, 0666);
        if (g_hfileEvtlog < 0)
            printk("[adbg] 2. open %s failed, err:%d\n", ASUS_EVTLOG_PATH".txt", g_hfileEvtlog);
        sys_chown(ASUS_EVTLOG_PATH".txt", AID_SDCARD_RW, AID_SDCARD_RW);

        size = sys_lseek(g_hfileEvtlog, 0, SEEK_END);
        if(size >= SZ_2M)
        {		 
            sys_close(g_hfileEvtlog); 
            sys_rmdir(ASUS_EVTLOG_PATH"_old.txt");
            sys_rename(ASUS_EVTLOG_PATH".txt", ASUS_EVTLOG_PATH"_old.txt");
            g_hfileEvtlog = sys_open(ASUS_EVTLOG_PATH".txt", O_CREAT|O_RDWR|O_SYNC, 0666);
            if (g_hfileEvtlog < 0)
                printk("[adbg] 2. open %s failed during renaming old one, err:%d\n", ASUS_EVTLOG_PATH".txt", g_hfileEvtlog);
        }

        while(g_Asus_Eventlog_read != g_Asus_Eventlog_write)
        {
            //spin_lock(&spinlock_eventlog);
            mutex_lock(&mA);
            //printk("------------------------------------g_Asus_Eventlog_read = %d\n", g_Asus_Eventlog_read);
            str_len = strlen(g_Asus_Eventlog[g_Asus_Eventlog_read]);
            pchar = g_Asus_Eventlog[g_Asus_Eventlog_read];
            g_Asus_Eventlog_read ++;
            g_Asus_Eventlog_read %= ASUS_EVTLOG_MAX_ITEM;   
            mutex_unlock(&mA);
            //printk("do_write_event_worker  str_len=%d, %s\n", str_len, pchar);
            if(pchar[str_len - 1] != '\n')
            {
                pchar[str_len] = '\n';
                pchar[str_len + 1] = '\0';
            }
            while(suspend_in_progress)
            {
                printk("[adbg] waiting for resume\n");
                msleep(1000);
            }    
            sys_write(g_hfileEvtlog, pchar, strlen(pchar));
            sys_fsync(g_hfileEvtlog);
            printk(pchar);
            
            //spin_unlock(&spinlock_eventlog);
        }
        sys_close(g_hfileEvtlog);
    }
    //printk("do_write_event_worker  freeing data\n");
    //kfree(my_work_data->data);
    //printk("do_write_event_worker  freeing my_work_data\n");
    //kfree(my_work_data);
    //printk("do_write_event_worker  freed \n");

    
}
         
extern struct timezone sys_tz;

void ASUSEvtlog(const char *fmt, ...)
{

    va_list args;
    char* buffer;
    
    if(g_bEventlogEnable == 0)
        return;
    //printk("-------------------------------g_Asus_Eventlog_write/asdf/ASUSEvtlog = %d\n", g_Asus_Eventlog_write);
    //mutex_lock(&mA);
    if (!in_interrupt() && !in_atomic() && !irqs_disabled())
        mutex_lock(&mA);//spin_lock(&spinlock_eventlog);
    
    buffer = g_Asus_Eventlog[g_Asus_Eventlog_write];
    //printk("g_Asus_Eventlog_write = %d\n", g_Asus_Eventlog_write);
    g_Asus_Eventlog_write ++;
    g_Asus_Eventlog_write %= ASUS_EVTLOG_MAX_ITEM;
        
    if (!in_interrupt() && !in_atomic() && !irqs_disabled())
        mutex_unlock(&mA);//spin_unlock(&spinlock_eventlog);
    //mutex_unlock(&mA);

    memset(buffer, 0, ASUS_EVTLOG_STR_MAXLEN);
    if(buffer)
    {
        struct rtc_time tm;
        struct timespec ts;
        
        getnstimeofday(&ts);
        ts.tv_sec -= sys_tz.tz_minuteswest * 60; // to get correct timezone information
        rtc_time_to_tm(ts.tv_sec, &tm);
        getrawmonotonic(&ts);
        sprintf(buffer, "(%ld)%04d-%02d-%02d %02d:%02d:%02d :",ts.tv_sec,tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    
        va_start(args, fmt);
        vscnprintf(buffer + strlen(buffer), ASUS_EVTLOG_STR_MAXLEN - strlen(buffer), fmt, args);
        va_end(args);
        //printk(buffer);
        queue_work(ASUSEvtlog_workQueue, &eventLog_Work);
        
        
        
    }
    else
        printk("[adbg] ASUSEvtlog buffer cannot be allocated");


}
EXPORT_SYMBOL(ASUSEvtlog);
static ssize_t evtlogswitch_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    if(strncmp(buf, "0", 1) == 0) {
        ASUSEvtlog("ASUSEvtlog disable !!");
        printk("[adbg] ASUSEvtlog disable !!");
        flush_work(&eventLog_Work);
        g_bEventlogEnable = 0;
    }
    if(strncmp(buf, "1", 1) == 0) {
        g_bEventlogEnable = 1;
        ASUSEvtlog("ASUSEvtlog enable !!");
        printk("[adbg] ASUSEvtlog enable !!");
    }

    return count;
}
static ssize_t asusevtlog_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    if (count > 256)
        count = 256;

    memset(messages, 0, sizeof(messages));
    if (copy_from_user(messages, buf, count))
        return -EFAULT;
    ASUSEvtlog(messages);

    return count;
}
//20101202_Bruno: added to get debug mask value
bool isASUS_MSK_set(const char *fmt)
{
    unsigned char *p = (unsigned char*) fmt;

    if(p[0] == ASUS_MSK_MAGIC)
    {
        if(!(debug_mask_setting[p[1]] & p[2]))
        {
            //printk("debug_mask_setting NOT set, p[1] is %d ,p[2] is %d\n",debug_mask_setting[p[1]],p[2]);
            return 0;
        }	
        else
        {
            //printk("debug_mask_setting set\n");
            return 1;
        }
    }
	else
		return 0;
}
EXPORT_SYMBOL(isASUS_MSK_set);
//20101202_Bruno: added to get debug mask value

/////////////////////////////////////////////////////////////////////////////////////////////
////                  Asusdebug module initial function
/////////////////////////////////////////////////////////////////////////////////////////////
static const struct file_operations proc_evtlogswitch_operations = {
    .write      = evtlogswitch_write,
};
static const struct file_operations proc_asusevtlog_operations = {
    .write      = asusevtlog_write,
};
static const struct file_operations proc_asusdebug_operations = {
    .read       = asusdebug_read,
    .write      = asusdebug_write,
    .open       = asusdebug_open,
    .release    = asusdebug_release,
};
//--

unsigned int asusdebug_enable = 0;
unsigned int readflag = 0;
static ssize_t turnon_asusdebug_proc_read(struct file *filp, char __user *buff, size_t len, loff_t *off)
{
    char print_buf[32];
    unsigned int ret = 0,iret = 0;
    sprintf(print_buf, "asusdebug: %s\n", asusdebug_enable? "off":"on");
    ret = strlen(print_buf);
    iret = copy_to_user(buff, print_buf, ret);
    if (!readflag){
               readflag = 1;
               return ret;
       }
       else{
               readflag = 0;
               return 0;
       }
}
static ssize_t turnon_asusdebug_proc_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
       char messages[256];
    memset(messages, 0, sizeof(messages));
       if (len > 256)
               len = 256;
       if (copy_from_user(messages, buff, len))
               return -EFAULT;
    if(strncmp(messages, "off", 3) == 0)
    {
               asusdebug_enable = 0x11223344;
       }
       else if(strncmp(messages, "on", 2) == 0)
       {
               asusdebug_enable = 0;
       }
       return len;
}
static struct file_operations turnon_asusdebug_proc_ops = {
    .read = turnon_asusdebug_proc_read,
    .write = turnon_asusdebug_proc_write,
};

static int __init proc_asusdebug_init(void)
{
    proc_create("asusdebug", S_IALLUGO, NULL, &proc_asusdebug_operations);
    proc_create("asusevtlog", S_IRWXUGO, NULL, &proc_asusevtlog_operations);
    proc_create("asusevtlog-switch", S_IRWXUGO, NULL, &proc_evtlogswitch_operations);
    proc_create("asusdebug-switch", S_IRWXUGO, NULL, &turnon_asusdebug_proc_ops);
    PRINTK_BUFFER = (unsigned int)ioremap(PRINTK_BUFFER, PRINTK_BUFFER_SIZE);
    mutex_init(&mA);
    fake_mutex.owner = current;
    fake_mutex.mutex_owner_asusdebug = current;
    fake_mutex.name = " fake_mutex";
	
    strcpy(fake_completion.name," fake_completion");
	
    fake_rtmutex.owner = current;
    //spin_lock_init(&spinlock_eventlog);
    ASUSEvtlog_workQueue  = create_singlethread_workqueue("ASUSEVTLOG_WORKQUEUE");

    return 0;
}
module_init(proc_asusdebug_init);

EXPORT_COMPAT("qcom,asusdebug");
