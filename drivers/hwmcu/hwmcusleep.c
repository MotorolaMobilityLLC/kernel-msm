/* Copyright (c) 2015, Huawei Technologies Co., Ltd. All rights reserved.
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


#define pr_fmt(fmt) "Mcusleep: %s: " fmt, __func__

#include <linux/module.h>	/* kernel module definitions */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>

#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/ioport.h>
#include <linux/param.h>
#include <linux/bitops.h>
#include <linux/termios.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>



/* Defines*/

#define VERSION "1.1"
#define PROC_DIR "mcusleep/sleep"

/*irq_polarity*/
#define POLARITY_LOW 0
#define POLARITY_HIGH 1


/*signal*/
#define MCU_SIG_WAKEUP_AP (42)

/*buff len*/
#define MCU_BUFF_LEN (255)

/*kill signal to app*/
extern int send_sig(int sig, struct task_struct *p, int priv);
#define KILL_PROC(nr, sig) \
{ \
struct task_struct *tsk; \
struct pid *pid;    \
pid = find_get_pid((pid_t)nr);    \
tsk = pid_task(pid, PIDTYPE_PID);    \
if (tsk) send_sig(sig, tsk, 1); \
}

/*mcu sleep info struct*/
struct mcusleep_info 
{
    /*mcu wake up ap, gpio input*/
    unsigned int mcu_host_wake;
    /*ap wake up mcu , gpio output*/
    unsigned int ext_wake_mcu;
    unsigned int host_wake_irq;
    unsigned int ap_status_gpio;
    unsigned int mcu_status_gpio;
    struct wake_lock wake_lock;
    int irq_polarity;
    int has_ext_wake;
    int mcu_irq_wake_status;
};

/* MCU wake ap timeout */
#define MCU_TIMER_INTERVAL (2 * HZ)

/* state variable names and bit positions */
#define MCU_ALLOW_SLEEP 0x01

static bool has_lpm_enabled = false;
static struct mcusleep_info *mcuinfo;
static int app_pid = 0;

/*
* Local function prototypes
*/
static int mcusleep_wake_allow(void);
static void mcusleep_wake_deny(void);

/*
* Global variables
*/

/** Global state flags */
static unsigned long flags;

/** Tasklet to respond to change in hostwake line */
static struct tasklet_struct mcu_hostwake_task;


/*proc f_ops*/
struct proc_dir_entry *mcusleep_dir;


/**
* @return 1 if the mcu has awake, 0 asleep.
*/
int mcusleep_in_sleep(void)
{
    /* check if mcu_status_gpio is asleep */
    return (gpio_get_value(mcuinfo->mcu_status_gpio));
}

/**
* A tasklet function that runs in tasklet context and set ap_status_gpio.
* @param data Not used.
*/
static void mcusleep_hostwake_task(unsigned long data)
{
    gpio_direction_output(mcuinfo->ap_status_gpio, 1);
    pr_info("mcusleep_hostwake_task isr ap status high\n");
    /* Start the wake lock */
    wake_lock_timeout(&mcuinfo->wake_lock, MCU_TIMER_INTERVAL);

}



/**
* Schedules a tasklet to run when receiving an interrupt on the
* <code>HOST_WAKE</code> GPIO pin.
* @param irq Not used.
* @param dev_id Not used.
*/
static irqreturn_t mcusleep_hostwake_isr(int irq, void *dev_id)
{
    /* schedule a tasklet to handle the change in the host wake line */
    pr_info("mcusleep_hostwake_isr\n");
    tasklet_schedule(&mcu_hostwake_task);
    return IRQ_HANDLED;
}

/**
* Starts the Sleep-Mode Protocol on the Host.
* @return On success, 0. On error, -1, and <code>errno</code> is set
* appropriately.
*/
static int mcusleep_wake_allow(void)
{
    int retval;

    retval = enable_irq_wake(mcuinfo->host_wake_irq);
    if (retval < 0) 
    {
        pr_err("Couldn't enable mcu_host_wake as wakeup interrupt\n");
        goto fail;
    }

    mcuinfo->mcu_irq_wake_status = true;
    pr_info("enable_irq_wake host_wake_irq\n");

    set_bit(MCU_ALLOW_SLEEP, &flags);
    return 0;

fail:

    return retval;
}

/**
* Stops the Sleep-Mode Protocol on the Host.
*/
static void mcusleep_wake_deny(void)
{
    /*pull high ap status gpio*/
    gpio_direction_output(mcuinfo->ap_status_gpio, 1);
    pr_info("gpio_direction_output ap_status_gpio high\n");
    wake_lock_timeout(&mcuinfo->wake_lock, HZ / 2);
}

/**
* read mcu sleep status
*/

static ssize_t mcusleep_read_proc_lpm
                (struct file *file, char __user *userbuf,
                size_t bytes, loff_t *off)
{
    int asleep = 0;
    
    asleep = gpio_get_value(mcuinfo->mcu_host_wake);
    pr_info("mcu host wake status: %d\n", asleep);
    return 0;
}

/**
* write mcu sleep proc
*/
static int mcusleep_write_proc_lpm(struct file *file,
    const char __user * buffer, size_t count,
    loff_t * ppos)
{
    char b;

    if (count < 1)
    return -EINVAL;

    if (copy_from_user(&b, buffer, 1))
    return -EFAULT;

    if (b == '0')
    {
        /* mcusleep_wake_deny */
        mcusleep_wake_deny();
        has_lpm_enabled = false;
    } 
    else 
    {
        /* mcusleep_wake_allow */
        if (!has_lpm_enabled) 
        {
            has_lpm_enabled = true;
            /* if arm sleep started, start mcusleep enable irq */
            mcusleep_wake_allow();
        }
    }

    return count;
}

/**
* write mcu sleep proc
*/
static int mcusleep_write_proc_pid(struct file *file,
    const char __user * buffer, size_t count,
    loff_t * ppos)
{
    char b[MCU_BUFF_LEN] = {0};

    if (count < 1)
    return -EINVAL;

    if (copy_from_user(&b, buffer, strlen(buffer)))
    return -EFAULT;

    app_pid = simple_strtol(b, NULL, 10);
	if( app_pid <=0 )
	{
		/*pid must > 0, not support init process use this function*/
		return -EFAULT;
	}

    pr_info("mcusleep_write_proc_pid:%d\n",
        app_pid);
    return count;
}


/**
* get gpio resource id, use of_get_named_gpio
*/
static int mcusleep_populate_dt_pinfo(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;
    int tmp;

    tmp = of_get_named_gpio(np, "mcu-wake-ap-gpio", 0);
    if (tmp < 0) 
    {
        pr_err("couldn't find mcu-wake-ap-gpio\n");
        return -ENODEV;
    }
    mcuinfo->mcu_host_wake = tmp;

    tmp = of_get_named_gpio(np, "mcu-status-gpio", 0);
    if (tmp < 0) 
    {
        pr_err("couldn't find mcu-status-gpio\n");
        return -ENODEV;
    }
    mcuinfo->mcu_status_gpio = tmp;
    
    tmp = of_get_named_gpio(np, "ext-wake-mcu-gpio", 0);
    if (tmp < 0)
    {
        mcuinfo->has_ext_wake = 0;
    }
    else
    {
        mcuinfo->has_ext_wake = 1;
    }
    
    if (mcuinfo->has_ext_wake)
    {
        mcuinfo->ext_wake_mcu = tmp;
    }
    tmp = of_get_named_gpio(np, "mcu-ap-status-gpio", 0);
    if (tmp < 0) 
    {
        pr_err("couldn't find mcu-ap-status-gpio\n");
        return -ENODEV;
    }
    mcuinfo->ap_status_gpio = tmp;

    pr_info("mcu_host_wake %d, ext_wake_mcu %d ap_status_gpio:%d\n",
        mcuinfo->mcu_host_wake, mcuinfo->ext_wake_mcu, mcuinfo->ap_status_gpio);
    return 0;
}

/**
* get gpio resource id, use platform_get_resource_byname
*/

static int mcusleep_populate_pinfo(struct platform_device *pdev)
{
    struct resource *res;

    res = platform_get_resource_byname(pdev, IORESOURCE_IO,
        "mcu-wake-ap-gpio");
    if (!res) 
    {
        pr_err("couldn't find mcu_host_wake gpio\n");
        return -ENODEV;
    }
    mcuinfo->mcu_host_wake = res->start;

    res = platform_get_resource_byname(pdev, IORESOURCE_IO,
        "mcu-ap-status-gpio");
    if (!res) 
    {
        pr_err("couldn't find ap_status_gpio gpio\n");
        return -ENODEV;
    }
    mcuinfo->ap_status_gpio = res->start;

    res = platform_get_resource_byname(pdev, IORESOURCE_IO,
        "mcu-status-gpio");
    if (!res) 
    {
        pr_err("couldn't find mcu_status_gpio gpio\n");
        return -ENODEV;
    }
    mcuinfo->mcu_status_gpio = res->start;
    
    res = platform_get_resource_byname(pdev, IORESOURCE_IO,
        "ext-wake-mcu-gpio");
    if (!res)
    {
        mcuinfo->has_ext_wake = 0;
    }
    else
    {
        mcuinfo->has_ext_wake = 1;
    }
    
    if (mcuinfo->has_ext_wake)
    {
        mcuinfo->ext_wake_mcu = res->start;
    }

    return 0;
}

/**
* mcu sleep module  drv probe
*/
static int mcusleep_probe(struct platform_device *pdev)
{
    struct resource *res;
    int ret;

    mcuinfo = kzalloc(sizeof(struct mcusleep_info), GFP_KERNEL);
    if (!mcuinfo)
    {
        return -ENOMEM;
    }
    
    if (pdev->dev.of_node) 
    {
        ret = mcusleep_populate_dt_pinfo(pdev);
        if (ret < 0) 
        {
            pr_err("couldn't populate info from dt\n");
            goto free_msi;
        }
    } 
    else 
    {
        ret = mcusleep_populate_pinfo(pdev);
        if (ret < 0) 
        {
            pr_err("couldn't populate info\n");
            goto free_msi;
        }
    }

    /* configure mcu_host_wake as input */
    ret = gpio_request_one(mcuinfo->mcu_host_wake, GPIOF_IN, "mcu-wake-ap-gpio");
    if (ret < 0) 
    {
        pr_err("failed to configure input"
                " direction for GPIO %d, error %d\n", mcuinfo->mcu_host_wake, ret);
        goto free_msi;
    }

    if (mcuinfo->has_ext_wake) 
    {
        /* configure ext_wake_mcu as output mode */
        ret = gpio_request_one(mcuinfo->ext_wake_mcu, GPIOF_OUT_INIT_LOW,
                                "ext-wake-mcu-gpio");
        if (ret < 0) 
        {
            pr_err("failed to configure output"
                    " direction for GPIO %d, error %d\n",
            mcuinfo->ext_wake_mcu, ret);
            goto free_mcu_host_wake;
        }
    }

    ret = gpio_request_one(mcuinfo->ap_status_gpio, GPIOF_OUT_INIT_HIGH,
                            "mcu-ap-status-gpio");
    if (ret < 0) 
    {
        pr_err("failed to configure output"
                " direction for GPIO %d, error %d\n",
        mcuinfo->ap_status_gpio, ret);
        goto free_ap_status_gpio;
    }

    ret = gpio_request_one(mcuinfo->mcu_status_gpio, GPIOF_IN,
                            "mcu-status-gpio");
    if (ret < 0) 
    {
        pr_err("failed to configure output"
                " direction for GPIO %d, error %d\n",
        mcuinfo->mcu_status_gpio, ret);
        goto free_mcu_status_gpio;
    }

    res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "mcu_host_wake");
    if (!res) 
    {
        pr_err("couldn't find mcu_host_wake irq\n");
        ret = -ENODEV;
        goto free_mcu_host_wake;
    }
    mcuinfo->host_wake_irq = res->start;
    if (mcuinfo->host_wake_irq < 0) 
    {
        pr_err("couldn't find mcu_host_wake irq\n");
        ret = -ENODEV;
        goto free_mcu_ext_wake;
    }

    /*low edge (falling edge) */
    mcuinfo->irq_polarity = POLARITY_LOW; 

    wake_lock_init(&mcuinfo->wake_lock, WAKE_LOCK_SUSPEND, "mcusleep");

    pr_info("host_wake_irq %d, polarity %d\n",
            mcuinfo->host_wake_irq, mcuinfo->irq_polarity);

    ret = request_irq(mcuinfo->host_wake_irq, mcusleep_hostwake_isr,
                    IRQF_DISABLED |  IRQF_TRIGGER_RISING,
                    "mcusleep hostwake", NULL);
    if (ret < 0) 
    {
        pr_err("Couldn't acquire MCU HOST WAKE UP IRQ\n");
        goto free_mcu_ext_wake;
    }

    gpio_direction_output(mcuinfo->ap_status_gpio, 1);

    mcuinfo->mcu_irq_wake_status = false;
    pr_info("mcu sleep probe ok init ap status high\n");
    return 0;

free_mcu_ext_wake:
    gpio_free(mcuinfo->ext_wake_mcu);
free_ap_status_gpio:
    gpio_free(mcuinfo->ap_status_gpio);
free_mcu_status_gpio:
    gpio_free(mcuinfo->mcu_status_gpio);
free_mcu_host_wake:
    gpio_free(mcuinfo->mcu_host_wake);
free_msi:
    kfree(mcuinfo);

    return ret;
}

/**
* mcu sleep module  drv remove
*/
static int mcusleep_remove(struct platform_device *pdev)
{
    free_irq(mcuinfo->host_wake_irq, NULL);
    gpio_free(mcuinfo->mcu_host_wake);
    gpio_free(mcuinfo->ap_status_gpio);
    gpio_free(mcuinfo->ext_wake_mcu);
    wake_lock_destroy(&mcuinfo->wake_lock);
    kfree(mcuinfo);
    return 0;
}

/**
* mcu sleep module  drv resume, do nothing
*/
static int mcusleep_resume(struct platform_device *pdev)
{

    return 0;
}

/**
* mcu sleep module  drv suspend, do nothing
*/
static int mcusleep_suspend(struct platform_device *pdev, pm_message_t state)
{
    return 0;
}

/**
* mcu mcusleep_match_table
*/
static struct of_device_id mcusleep_match_table[] = 
{
    {.compatible = "qcom,mcusleep"},
    {}
};

/**
* mcu platform_driver struct
*/
static struct platform_driver mcusleep_driver = 
{
    .probe = mcusleep_probe,
    .remove = mcusleep_remove,
    .suspend = mcusleep_suspend,  
    .resume = mcusleep_resume,
    .driver = {
        .name = "mcusleep",
        .owner = THIS_MODULE,
        .of_match_table = mcusleep_match_table,
    },
};

/**
* mcu file_operations
*/
static const struct file_operations lpm_fops = 
{
    .owner = THIS_MODULE,
    .read = mcusleep_read_proc_lpm,
    .write = mcusleep_write_proc_lpm,
};

/**
* mcu pid file
*/
static const struct file_operations pid_fops = 
{
    .owner = THIS_MODULE,
    .write = mcusleep_write_proc_pid,
};

/**
* make proc sys node
*/
static struct proc_dir_entry *mcusleep_proc_create(const char *name,
                                                umode_t mode,
                                                struct proc_dir_entry
                                                *parent,
                                                const struct file_operations
                                                *fops)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
    return proc_create(name, mode, parent, fops);
#else
    struct proc_dir_entry *entry;
    entry = create_proc_entry(name, mode, parent);
    if (entry)
    {
        entry->proc_fops = fops;
    }
    return entry;
#endif
}


/**
* Initializes the module.
* @return On success, 0. On error, -1, and <code>errno</code> is set
* appropriately.
*/
static int __init mcusleep_init(void)
{
    int retval;
    struct proc_dir_entry *ent;

    pr_info("Mcusleep Mode Driver Ver %s\n", VERSION);

    retval = platform_driver_register(&mcusleep_driver);
    if (retval)
    {
        return retval;
    }
    if (mcuinfo == NULL)
    {
        return 0;
    }
    mcusleep_dir = proc_mkdir("mcusleep", NULL);
    if (mcusleep_dir == NULL) 
    {
        pr_err("Unable to create /proc/mcusleep directory\n");
        return -ENOMEM;
    }

    /* Creating write only "lpm" entry */
    ent = mcusleep_proc_create("lpm", S_IRUGO | S_IWUSR | S_IWGRP, mcusleep_dir,
                                &lpm_fops);
    if (ent == NULL) 
    {
        pr_err("Unable to create /proc/%s/lpm entry\n", PROC_DIR);
        retval = -ENOMEM;
        goto fail;
    }

    /* Creating write only "pid" entry */
    ent = mcusleep_proc_create("pid", S_IRUGO | S_IWUSR | S_IWGRP, mcusleep_dir,
                                &pid_fops);
    if (ent == NULL) 
    {
        pr_err("Unable to create /proc/%s/lpm entry\n", PROC_DIR);
        retval = -ENOMEM;
        goto fail;
    }

    /* clear all status bits */
    flags = 0;

    /* initialize host wake tasklet */
    tasklet_init(&mcu_hostwake_task, mcusleep_hostwake_task, 0);

    return 0;

fail:
    remove_proc_entry("lpm", mcusleep_dir);
    remove_proc_entry("pid", mcusleep_dir);
    remove_proc_entry("mcusleep", 0);
    return retval;
}

/**
* Cleans up the module.
*/
static void __exit mcusleep_exit(void)
{
    if (mcuinfo == NULL)
    {
        return;
    }
    /* assert bt wake */
    if (mcuinfo->has_ext_wake == 1)
    {
        gpio_direction_output(mcuinfo->ext_wake_mcu, 0);
    }

    if (test_bit(MCU_ALLOW_SLEEP, &flags)) 
    {
        if (disable_irq_wake(mcuinfo->host_wake_irq))
        {
            pr_err("Couldn't disable hostwake IRQ wakeup mode\n");
        }
        free_irq(mcuinfo->host_wake_irq, NULL);
    }

    platform_driver_unregister(&mcusleep_driver);

    remove_proc_entry("lpm", mcusleep_dir);
    remove_proc_entry("pid", mcusleep_dir);
    remove_proc_entry("mcusleep", 0);
}

module_init(mcusleep_init);
module_exit(mcusleep_exit);

MODULE_DESCRIPTION("Mcu Sleep Mode Driver ver %s " VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

