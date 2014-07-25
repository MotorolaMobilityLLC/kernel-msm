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
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/export.h>
#include <linux/rtc.h>
#include "rtmutex_common.h"

#include <linux/delay.h>
#include <linux/slab.h>

//ASUS_BSP +++ Josh_Hsu "Enable last kmsg feature for Google"
#define ASUS_LAST_KMSG	1

#if ASUS_LAST_KMSG
#include <linux/init.h>     //For getting bootreason

unsigned int LAST_KMSG_BUFFER = 0x11F00000;
static int last_kmsg_length = 0;
static int is_lk_rebased = 0;
static char bootreason[30];

static void initKernelEnv(void);
static void deinitKernelEnv(void);
#endif
//ASUS_BSP --- Josh_Hsu "Enable last kmsg feature for Google"

/* Must be the same with the config of qcom,asusdebug in *.dtsi */
/* 
 * For Google ASIT requirment, we need to implement last_kmsg 
 * we put last_kmsg buffer to 0x11F00000, length is 512 kbytes
 * and move PRINTK_BUFFER to 0x11F80000
 */
unsigned int PRINTK_BUFFER = 0x11F80000;

extern struct timezone sys_tz;
static struct workqueue_struct *ASUSEvtlog_workQueue;

//ASUS_BSP +++ Josh_Hsu "Enable last kmsg feature for Google"
#if ASUS_LAST_KMSG

/* Setup bootreason */
static int set_bootreason(char *str)
{
    strcpy(bootreason, str);
	return 0;
}
__setup("bootreason=", set_bootreason);
#endif
//ASUS_BSP --- Josh_Hsu "Enable last kmsg feature for Google"

/* Enable and disable UART console */
extern void suspend_console(void);
extern void resume_console(void);
static int console_state = 1;

/*
 *   All thread information (Refined)
 */

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

        if (g_iPtr < PHONE_HANG_LOG_SIZE) 
        {
            g_iPtr += len;
            return 0;
        }
    }
    g_iPtr = PHONE_HANG_LOG_SIZE;
    return -1;
}

/*
 *   Printk and Last kmsg
 */
extern void printk_buffer_rebase(void);

//ASUS_BSP +++ Josh_Hsu "Enable last kmsg feature for Google"
#if ASUS_LAST_KMSG

static unsigned int g_cat_amount_left;
static unsigned int g_cat_read_pos;
static int g_lk_fetched;
static char* g_last_kmsg_buffer;
static int g_cat_finished;

/* Get last kmsg from file */
int fill_lk_content(char* buf, int buflen){

    if(g_lk_fetched)
        return 0;

    if(buflen > PRINTK_PARSE_SIZE)
        return -2;

    /* Allocate buffer */
    g_last_kmsg_buffer = kmalloc(buflen, GFP_KERNEL);
    if (!g_last_kmsg_buffer)
    {
        printk("[adbg] g_last_kmsg_buffer malloc failure\n");
        return -1;
    }

    /* Copy content */
    memcpy(g_last_kmsg_buffer, buf, buflen);

    g_lk_fetched = 1;

    return 0;
}

/* Calculate the size of lk and skip the extra 0's in buffer */
int fetch_last_kmsg_size(char* buf, int buflen, int option){

    int offset = 0;
    int zero_count = 0;
    int size = 0;
    int zero_tolerent = 30;
    int ret;

    /* 
     * Following situaion is not allowed 
     * 1. buf and buflen is not legal
     * 2. option is set to 1, which means debug mode
     * 3. we already fetech lk, skip here
     */
    if(!buf || !buflen || option == 1 || g_lk_fetched)
        return 0;

    while(buflen){
        if( !*(buf + offset ) ){
            zero_count++;
            if(zero_count > zero_tolerent){
                break;
            }
        }
        size++;
        offset++;
        buflen--;
    }

    last_kmsg_length = size - zero_count; //Leave the end of string byte
    
    // Adjust read position
	g_cat_amount_left = last_kmsg_length;
	g_cat_read_pos = 0;

    // Fill the lk buffer for user to cat
    ret = fill_lk_content(buf, last_kmsg_length);
    if(ret)
        printk("[adbg] Fill LK content failed %d\n", ret);

    printk("[adbg] Fetch last kmsg finished with size %d\n", last_kmsg_length);

    return size;
}

static ssize_t asuslastkmsg_read(struct file *file, char __user *buf,
             size_t length, loff_t *offset)
{	
   	int bytes_read = 0;
	char* msg_Ptr = g_last_kmsg_buffer + g_cat_read_pos;
    char reason[100];
    int reason_size = 0;

    if(!msg_Ptr){
        printk("[adbg] Read LK: msg_Prt is null.\n");
        return 0;
    }

    if(g_cat_finished){
        g_cat_finished = 0;
        return 0;
    }

   	if (g_cat_amount_left == 0){
        /* Start print bootreason and reset read head */
        printk("[adbg] Read LK: Start print bootreason, len left %d\n", length);
        
        reason_size = snprintf(reason, 100, 
            "\n\nNo errors detected\nBoot info:\nLast boot reason: %s\n", bootreason);
        if(reason_size < length){
            while (reason_size && length)  {
                put_user(*(reason + bytes_read), buf++);
                reason_size--;
                bytes_read++;
                length--;
   	        }
        }

        printk("%s", reason);
        
		g_cat_read_pos = 0;
		g_cat_amount_left = last_kmsg_length;
        g_cat_finished = 1;
        
		return bytes_read;
   	}

   	while (length && g_cat_amount_left)  {
         put_user(*(msg_Ptr++), buf++);

         length--;
		 g_cat_amount_left--;
         bytes_read++;
   	}

	g_cat_read_pos += bytes_read;

   	return bytes_read;
}

static ssize_t asuslastkmsg_write(struct file *file, 
    const char __user *buf, size_t count, loff_t *ppos)
{
	char command[256];
	
	if (count > 256)
		count = 256;
	if (copy_from_user(command, buf, count))
		return -EFAULT;
	
    return count;
}

static void save_last_kmsg_buffer(char* filename){

    char *last_kmsg_buffer;
    char lk_filename[256];
	int lk_file_handle;

    /* If LK is rebased, we skip here */
    if(is_lk_rebased)
        return;

	// Address setting
    last_kmsg_buffer = (char*)LAST_KMSG_BUFFER;
    if(filename)
        sprintf(lk_filename, filename);
    else
	    sprintf(lk_filename, "/asdf/last_kmsg.txt");

    initKernelEnv();

	// Save last kmsg
	lk_file_handle = sys_open(lk_filename, O_CREAT|O_RDWR|O_SYNC, 0);

	if(!IS_ERR((const void *)lk_file_handle))
    {
        sys_write(lk_file_handle, (unsigned char*)last_kmsg_buffer, PRINTK_PARSE_SIZE);
        sys_close(lk_file_handle);
    } else {
        printk("[adbg] last kmsg save failed: [%d]\n", lk_file_handle);
    }

    fetch_last_kmsg_size(last_kmsg_buffer, PRINTK_PARSE_SIZE, 0);

    deinitKernelEnv();
}

#endif
//ASUS_BSP --- Josh_Hsu "Enable last kmsg feature for Google"

static int asusdebug_open(struct inode * inode, struct file * file)
{
    return 0;
}

static int asusdebug_release(struct inode * inode, struct file * file)
{
    return 0;
}

static ssize_t asusdebug_read(struct file *file, char __user *buf,
             size_t count, loff_t *ppos)
{
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

void print_log_to_console(unsigned char* buf, int len){
    int count = len;
    char* buffer = buf;

    const int printk_max_size = 512;
    char* record;
    int record_offset = 0;

    while(count > 0){
        char message[printk_max_size];

        record = strchr(buffer, '\n');

        if (record == NULL )
            break;

        record_offset = record - buffer;

        memcpy(message, buffer, record_offset);
        message[record_offset] = '\0';

        printk("%s\n", message);

        count = count - record_offset - 1;
        buffer = buffer + record_offset + 1;
    }
}

void save_last_shutdown_log(char* filename)
{
	char *last_shutdown_log_unparsed;
	
	int file_handle_unparsed;
    unsigned long long t;
    unsigned long nanosec_rem;

    t = cpu_clock(0);
    nanosec_rem = do_div(t, 1000000000);

	pr_info("[adbg] %s()++\n", __func__);

	// Address setting
    last_shutdown_log_unparsed = (char*)PRINTK_BUFFER;

    // File name setting
	sprintf(messages_unparsed, "/asdf/LastShutdown_%lu.%06lu_unparsed.txt", (unsigned long) t, nanosec_rem / 1000);
	printk("[adbg] %s(), messages_unparsed: %s\n", __func__, messages_unparsed);

    sprintf(messages, "/asdf/LastShutdown_%lu.%06lu.txt", (unsigned long) t, nanosec_rem / 1000);

#if ASUS_LAST_KMSG
	save_last_kmsg_buffer(messages);
#endif

    initKernelEnv();

	// Save unparsed log first, in case parser cannnot work
	file_handle_unparsed = sys_open(messages_unparsed, O_CREAT|O_RDWR|O_SYNC, 0);

	if(!IS_ERR((const void *)file_handle_unparsed))
    {
        sys_write(file_handle_unparsed, (unsigned char*)last_shutdown_log_unparsed, PRINTK_BUFFER_SLOT_SIZE);
        sys_close(file_handle_unparsed);
    } else {
        printk("[adbg] [ASDF] save_last_shutdown_error: [%d]\n", file_handle_unparsed);
    }
    
    deinitKernelEnv();          

    pr_info("[adbg] %s()--\n", __func__);
}

void get_last_shutdown_log(void)
{
    unsigned int *last_shutdown_log_addr;

    last_shutdown_log_addr = (unsigned int *)((unsigned int)PRINTK_BUFFER +
        (unsigned int)PRINTK_BUFFER_SLOT_SIZE);

    printk("[adbg] get_last_shutdown_log: last_shutdown_log_addr=0x%08x, value=0x%08x\n",
        (unsigned int)last_shutdown_log_addr, *last_shutdown_log_addr);

//ASUS_BSP +++ Josh_Hsu "Enable last kmsg feature for Google"
#if ASUS_LAST_KMSG
	save_last_kmsg_buffer(NULL);
#endif
//ASUS_BSP --- Josh_Hsu "Enable last kmsg feature for Google"

    if( *last_shutdown_log_addr == (unsigned int)PRINTK_BUFFER_MAGIC)
    {
        save_last_shutdown_log("LastShutdown");
    }

    /* Printk buffer rebase will also rebase last_kmsg if needed */
    printk_buffer_rebase();
    is_lk_rebased = 1;
    
    *last_shutdown_log_addr = PRINTK_BUFFER_MAGIC;  //ASUS_BSP ++
}
EXPORT_SYMBOL(get_last_shutdown_log);

int first = 0;
int asus_asdf_set = 0;

static ssize_t asusdebug_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned int *last_shutdown_log_addr;

	if (count > 256)
		count = 256;
	if (copy_from_user(messages, buf, count))
		return -EFAULT;

    if(strncmp(messages, "load", 4) == 0)
    {
        first = 1;
        return count;
    }
    else if(strncmp(messages, "slowlog", 7) == 0)
	{
		printk("[adbg] slowlog is currently disabled.\n");
		return count;
	}
	else if(strncmp(messages, "panic", 5) == 0)
	{
		printk("[adbg] Please use echo c > /proc/sysrq-trigger instead.\n");
		panic("panic test");
	}
	else if(strncmp(messages, "get_asdf_log", strlen("get_asdf_log")) == 0)
	{
		printk(KERN_WARNING "[ASDF] Now dumping ASDF last shutdown log\n");
		last_shutdown_log_addr = (unsigned int *)((unsigned int)PRINTK_BUFFER + 
		    (unsigned int)PRINTK_BUFFER_SLOT_SIZE);
		printk(KERN_WARNING "[ASDF] last_shutdown_log_addr=0x%08x, value=0x%08x\n", 
		    (unsigned int)last_shutdown_log_addr, *last_shutdown_log_addr);

		if(!asus_asdf_set)
		{
			asus_asdf_set = 1;
			get_last_shutdown_log();
			printk(KERN_WARNING "[ASDF] get_last_shutdown_log: last_shutdown_log_addr=0x%08x, value=0x%08x\n", 
			    (unsigned int)last_shutdown_log_addr, *last_shutdown_log_addr);

			(*last_shutdown_log_addr)=(unsigned int)PRINTK_BUFFER_MAGIC;
		}
	}
    else if(strncmp(messages, "audbg_on", 8) == 0)
	{
		printk("[adbg] Enabling audio debug\n");
        if(!console_state){
            resume_console();
            console_state = 1;
        }
		return count;
	}
    else if(strncmp(messages, "audbg_off", 9) == 0)
	{
		printk("[adbg] Disabling audio debug\n");
        if(console_state){
            suspend_console();
            console_state = 0;
        }
		return count;
	}
    else
    {
		printk("[adbg] %s option in asusdebug is no longer supported.\n", messages);
		return count;
	}

	return count;
}

/*
 *  Eventlog mask mechanism
 */

extern int suspend_in_progress;
static int g_hfileEvtlog = -MAX_ERRNO;
static int g_bEventlogEnable = 1;
static char g_Asus_Eventlog[ASUS_EVTLOG_MAX_ITEM][ASUS_EVTLOG_STR_MAXLEN];
static int g_Asus_Eventlog_read = 0;
static int g_Asus_Eventlog_write = 0;

static void do_write_event_worker(struct work_struct *work);
static DECLARE_WORK(eventLog_Work, do_write_event_worker);

static struct mutex mA;
#define AID_SDCARD_RW 1015

static void do_write_event_worker(struct work_struct *work)
{
    char buffer[256];
    
    while(first == 0 || suspend_in_progress)
    {
        printk("[adbg] waiting first\n");
        msleep(1000);
    }   
    
    if(IS_ERR((const void*)g_hfileEvtlog))
    {
        long size;
        {
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
            mutex_lock(&mA);

            str_len = strlen(g_Asus_Eventlog[g_Asus_Eventlog_read]);
            pchar = g_Asus_Eventlog[g_Asus_Eventlog_read];
            g_Asus_Eventlog_read ++;
            g_Asus_Eventlog_read %= ASUS_EVTLOG_MAX_ITEM;   
            mutex_unlock(&mA);
            
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
        }
        sys_close(g_hfileEvtlog);
    }
}
         
void ASUSEvtlog(const char *fmt, ...)
{
    va_list args;
    char* buffer;
    
    if(g_bEventlogEnable == 0)
        return;

    if (!in_interrupt() && !in_atomic() && !irqs_disabled())
        mutex_lock(&mA);//spin_lock(&spinlock_eventlog);
    
    buffer = g_Asus_Eventlog[g_Asus_Eventlog_write];

    g_Asus_Eventlog_write ++;
    g_Asus_Eventlog_write %= ASUS_EVTLOG_MAX_ITEM;
        
    if (!in_interrupt() && !in_atomic() && !irqs_disabled())
        mutex_unlock(&mA);//spin_unlock(&spinlock_eventlog);

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

/*
 *  Asusdebug module initial function
 */

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

//ASUS_BSP +++ Josh_Hsu "Enable last kmsg feature for Google"
#ifdef ASUS_LAST_KMSG
static const struct file_operations proc_asuslastkmsg_operations = {
    .read       = asuslastkmsg_read,
	.write      = asuslastkmsg_write,
};
#endif
//ASUS_BSP --- Josh_Hsu "Enable last kmsg feature for Google"

static int __init proc_asusdebug_init(void)
{
    proc_create("asusdebug", S_IALLUGO, NULL, &proc_asusdebug_operations);
    proc_create("asusevtlog", S_IRWXUGO, NULL, &proc_asusevtlog_operations);
    proc_create("asusevtlog-switch", S_IRWXUGO, NULL, &proc_evtlogswitch_operations);
//ASUS_BSP +++ Josh_Hsu "Enable last kmsg feature for Google"
#if ASUS_LAST_KMSG
	proc_create("last_kmsg", S_IALLUGO, NULL, &proc_asuslastkmsg_operations);

    LAST_KMSG_BUFFER = (unsigned int)ioremap(LAST_KMSG_BUFFER, PRINTK_PARSE_SIZE);
#endif
//ASUS_BSP --- Josh_Hsu "Enable last kmsg feature for Google"
    PRINTK_BUFFER = (unsigned int)ioremap(PRINTK_BUFFER, PRINTK_BUFFER_SIZE);
    mutex_init(&mA);
    ASUSEvtlog_workQueue  = create_singlethread_workqueue("ASUSEVTLOG_WORKQUEUE");

    return 0;
}
module_init(proc_asusdebug_init);

EXPORT_COMPAT("qcom,asusdebug");
