// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/cpumask.h>

#define NO_TIMEOUT -1U
#define MIDPOINT_DEFAULT_QOS_TIMEOUT_MS 100000U
static int midpoint_freq = 0;
static int qos_timeout_ms = MIDPOINT_DEFAULT_QOS_TIMEOUT_MS;

module_param(midpoint_freq, int, 0400);
module_param(qos_timeout_ms, int, 0400);

static DEFINE_PER_CPU(struct freq_qos_request, qos_max_req);
static DEFINE_PER_CPU(struct freq_qos_request, qos_min_req);
static struct delayed_work qos_remove_work;
static void request_freq_qos(struct work_struct *w);
static DECLARE_WORK(request_qos_work, request_freq_qos);

static void midpoint_qos_remove(void) {
	struct freq_qos_request *req;
	int cpu;

	for_each_possible_cpu(cpu) {
		req = &per_cpu(qos_min_req, cpu);
		freq_qos_remove_request(req);
		req = &per_cpu(qos_max_req, cpu);
		freq_qos_remove_request(req);
	}

	pr_info("Removed midpoint min qos.\n");
}

static void midpoint_qos_remove_work(struct work_struct *work)
{
	midpoint_qos_remove();
}

static void request_freq_qos(struct work_struct *w)
{
	struct cpufreq_policy *policy;
	struct freq_qos_request *req;
	int cpu, ret;
	int wait_count = 3;

	pr_info("Start init point\n");
	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);

		/*Add waiting time for initial setting of cluster (1.policy is not null 2.governor is not null)*/
		while ((!policy || !policy->governor
				|| !policy->governor->name[0]) && wait_count > 0) {
			wait_count --;
			msleep(50);
			if (unlikely(!policy)) {
				policy = cpufreq_cpu_get(cpu);
			}
		}

		if (!policy || !policy->governor || !policy->governor->name[0]) {
			pr_err("%s: cpufreq policy not found for cpu%d\n", __func__, cpu);
			return;
		}

		/*Only allow setting if governor is powersave */
		if (strcmp(policy->governor->name, "powersave")) {
			pr_err("%s: exiting as governor is %s\n", __func__,
					!policy->governor ? "NULL" : policy->governor->name);
			return;
		}

		req = &per_cpu(qos_max_req, cpu);
		ret = freq_qos_add_request(&policy->constraints, req,
						FREQ_QOS_MAX, midpoint_freq);

		if (ret < 0) {
			cpufreq_cpu_put(policy);
			goto out;
		}

		req = &per_cpu(qos_min_req, cpu);
		ret = freq_qos_add_request(&policy->constraints, req,
						FREQ_QOS_MIN, midpoint_freq);
		cpufreq_cpu_put(policy);
		if (ret < 0)
			goto out;
	}

	if (qos_timeout_ms != NO_TIMEOUT) {
		INIT_DELAYED_WORK(&qos_remove_work, midpoint_qos_remove_work);
		schedule_delayed_work(&qos_remove_work, msecs_to_jiffies(qos_timeout_ms));
	}

	pr_info("Added midpoint qos with freq=%d qos_timeout_ms=%d.\n", midpoint_freq, qos_timeout_ms);
	return;

out:
	midpoint_qos_remove();

	if (ret < 0) {
		pr_err("%s: Failed to add freq constraint (%d) on cpu=%d\n",
						__func__, ret, cpu);
		return;
	}
}

void midpoint_init(void)
{
	if (!midpoint_freq) {
		pr_info("No boot frequency\n");
		return;
	}
	schedule_work(&request_qos_work);
}
