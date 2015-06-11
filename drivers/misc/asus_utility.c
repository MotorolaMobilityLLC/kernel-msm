/*
 * asus.utilityc - driver for 
 *
 * Copyright (C) 2014 Cliff Yu <cliff_yu@asus.com>
 *
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/asus_utility.h>
#include <linux/asusdebug.h>

#define MODE_PROC_MAX_BUFF_SIZE  256
#define WAIT_FOR_AMBIENT_DRAW_MS                (HZ)
#define USER_ROOT_DIR "asus_utility"  
#define USER_ENTRY   "interactive"
/*
 * notification part
 * each driver have to use register_hs_notifier() to register a callback-function
 * if the driver want to be notified while hall sensor status is changing
 *
 * register_mode_notifier(): drivers can use the func to register a callback
 * unregister_mode_notifier(): drivers can use the func to unregister a callback

 */
uint32_t mode_notifier_base;

static struct proc_dir_entry *mode_root;  

static int interactive_status = 0;
static RAW_NOTIFIER_HEAD(mode_chain_head);

static struct workqueue_struct *notify_workQueue;
static void do_notify_on_worker(struct work_struct *work);
static void do_notify_off_worker(struct work_struct *work);
static DECLARE_WORK(notify_on_Work, do_notify_on_worker);
static DECLARE_WORK(notify_off_Work, do_notify_off_worker);
static struct wake_lock ambient_drawing_wakelock;
int modeSendNotify(unsigned long val)
{
       int ret = 0;
       printk("%s++ , val =%lu\r\n",__FUNCTION__,val);

       ASUSEvtlog("[MODE] Interactive mode : %lu\n", val);
       ret = (raw_notifier_call_chain(&mode_chain_head, val, NULL) == NOTIFY_BAD) ? -EINVAL : 0;
       
       printk("%s-- , val =%lu\r\n",__FUNCTION__,val);
       if(-EINVAL==ret)
            printk("notify callback %lu failed and terminated\r\n", val);
       
	return ret;
}
EXPORT_SYMBOL_GPL(modeSendNotify);


int register_mode_notifier(struct notifier_block *nb)
{	
	return raw_notifier_chain_register(&mode_chain_head, nb);
}
EXPORT_SYMBOL_GPL(register_mode_notifier);

int unregister_mode_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&mode_chain_head, nb);
}
EXPORT_SYMBOL_GPL(unregister_mode_notifier);


/**
 * =======================================================================
 */

static void do_notify_on_worker(struct work_struct *work){
	modeSendNotify(1);
	//printk ("%s", msg);
	interactive_status = 1;
}

static void do_notify_off_worker(struct work_struct *work){
	modeSendNotify(0);
	//printk ("%s", msg);
	interactive_status = 0;
}

static int mode_write_proc_interactive (struct file *filp, const char __user *buff, size_t len, loff_t *pos)
{
	char msg[MODE_PROC_MAX_BUFF_SIZE];
	if (len > MODE_PROC_MAX_BUFF_SIZE)
		len = MODE_PROC_MAX_BUFF_SIZE;
	
	printk(KERN_DEBUG "[MODE] %s():\n", __func__);

	if (copy_from_user(msg, buff, len))
		return -EFAULT;

	if (!strncmp(msg,"FB_BLANK_ENTER_INTERACTIVE",len-1)){
		if (interactive_status == 0)
			queue_work(notify_workQueue, &notify_on_Work);
	}
	else if (!strncmp(msg,"FB_BLANK_ENTER_NON_INTERACTIVE",len-1)){

		wake_lock_timeout(&ambient_drawing_wakelock, WAIT_FOR_AMBIENT_DRAW_MS);
		printk("wake_lock_timeout(Hz) for ambient mode UI...\n");

		if (interactive_status == 1)
			queue_work(notify_workQueue, &notify_off_Work);
	}
	else {
		//printk ("%s", msg);
		pr_err("Invalid Status\r\n");
		interactive_status = -1;		
	}

	return len;
}


static int mode_proc_show_interactive(struct seq_file *seq, void *v)
{
	printk(KERN_DEBUG "[MODE] %s():\n", __func__);
	
	seq_printf(seq, "%d\n", interactive_status);
	
	return 0;
}

static void *mode_proc_start(struct seq_file *seq, loff_t *pos)
{
	static unsigned long counter = 0;
	
	if(*pos == 0){
		return &counter;
	}
	else{
		*pos = 0;
		return NULL;
	}
}

static void *mode_proc_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return NULL;
}

static void mode_proc_stop(struct seq_file *seq, void *v)
{

}

static const struct seq_operations mode_proc_interactive_seq = {
	.start		= mode_proc_start,
	.show		= mode_proc_show_interactive,
	.next		= mode_proc_next,
	.stop		= mode_proc_stop,
};

static int mode_open_proc_interactive(struct inode *inode, struct file *file)
{
	return seq_open(file, &mode_proc_interactive_seq);
}

static const struct file_operations mode_proc_interactive_fops = {
	.owner		= THIS_MODULE,
	.open		= mode_open_proc_interactive,
	.read		= seq_read,
	.write		= mode_write_proc_interactive,
};

static int init_debug_port(void)
{
	struct proc_dir_entry *mode; 
	
	wake_lock_init(&ambient_drawing_wakelock, WAKE_LOCK_SUSPEND, "ambient_drawing_wakelock");

    mode_root = proc_mkdir(USER_ROOT_DIR, NULL); 
    if (NULL== mode_root) 
    { 
		printk(KERN_ALERT"Create dir /proc/%s error!\n", USER_ROOT_DIR); 
		return -1; 
    } 
    printk(KERN_INFO"Create dir /proc/%s\n", USER_ROOT_DIR); 

    
    mode = proc_create(USER_ENTRY, 0666, mode_root, &mode_proc_interactive_fops); 
    if (NULL == mode) 
    { 
        printk(KERN_ALERT"Create entry %s under /proc/%s error!\n", USER_ENTRY,USER_ROOT_DIR); 
        goto err_out; 
    }
  
    printk(KERN_INFO"Create /proc/%s/%s\n", USER_ROOT_DIR,USER_ENTRY);
 
    return 0; 
    
err_out: 
    remove_proc_entry(USER_ROOT_DIR,mode_root); 
    return -1; 
}

static void remove_debug_port(void)
{
    remove_proc_entry(USER_ENTRY,mode_root); 
    remove_proc_entry(USER_ROOT_DIR,NULL);
    wake_lock_destroy(&ambient_drawing_wakelock);
}

/**
 * =======================================================================
 */

static int mode_notifier_probe(struct platform_device *pdev)
{
	printk( "[MODE] %s() +++\n", __FUNCTION__);
	
	printk( "[MODE] %s() ---\n", __FUNCTION__);
	return 0;
}


static struct platform_driver mode_notifier_driver = {
	.driver	= {
		.name	= "mode_notifier",
		.owner	= THIS_MODULE,
	},
	.probe	= mode_notifier_probe,
	//.remove	= mode_notifier_remove,
};

static int __init mode_notifier_init(void)
{	
	printk( "[MODE] %s() +++\n", __FUNCTION__);

	init_debug_port();
	notify_workQueue  = create_singlethread_workqueue("NOTIFY_WORKQUEUE");
	
	printk( "[MODE] %s() ---\n", __FUNCTION__);
	
	return 1;
}

static void __exit mode_notifier_exit(void)
{

	remove_debug_port();
	
	platform_driver_unregister(&mode_notifier_driver);
}

module_init(mode_notifier_init);
module_exit(mode_notifier_exit);

MODULE_DESCRIPTION("Mode Notifier Driver");
MODULE_LICENSE("GPL");
