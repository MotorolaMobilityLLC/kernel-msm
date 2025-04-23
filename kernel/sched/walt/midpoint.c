// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpufreq.h>

static int midpoint_freq = 1700000;

static void cpufreq_gov_midpoint_limits(struct cpufreq_policy *policy)
{
	__cpufreq_driver_target(policy, midpoint_freq, CPUFREQ_RELATION_L);
}

static struct cpufreq_governor cpufreq_gov_midpoint = {
	.name		= "midpoint",
	.owner		= THIS_MODULE,
	.flags		= CPUFREQ_GOV_STRICT_TARGET,
	.limits		= cpufreq_gov_midpoint_limits,
};

cpufreq_governor_init(cpufreq_gov_midpoint);
cpufreq_governor_exit(cpufreq_gov_midpoint);
MODULE_LICENSE("GPL");
