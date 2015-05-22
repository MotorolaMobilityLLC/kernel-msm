/*  Copyright (c) 2015, HUAWEI TECHNOLOGIES CO., LTD. All rights reserved.
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

#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/kthread.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/power_supply.h>

#define  OPTIMAL_CPU_FREQ_VALUE 787200

static int __init thermal_protect_init(void)
{
    int ret = 0;

    /* b/20896947 In order to limit the potential thermal overheat during reboot process
       when ambient temperature is high, CPU0 freq during kernel boot process is set to
       OPTIMAL_CPU_FREQ_VALUE. The scaling governer may later on be adjusted again when
       Android starts booting */
    ret = cpufreq_driver_target(cpufreq_cpu_get(0),OPTIMAL_CPU_FREQ_VALUE,
                                CPUFREQ_RELATION_H);
    if (ret < 0)
    {
        printk(KERN_ERR "Thermal_protect: Unable to adjust CPU0 freq \n");
        return -ENODEV;
    }

    return 0;
}


static void __exit thermal_protect_exit(void)
{
}

fs_initcall(thermal_protect_init);

module_exit(thermal_protect_exit);