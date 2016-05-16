/* drivers/sharp/shlog/shrlog.c
 *
 * Copyright (C) 2010- Sharp Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 *
 *
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <soc/qcom/sharp/shrlog.h>
#include <soc/qcom/sharp/shrlog_restart.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <asm-generic/sections.h>
#include <asm/memory.h>

#if defined(CONFIG_SHARP_PANIC_ON_SUBSYS)
#include <soc/qcom/restart.h>
#include <soc/qcom/subsystem_restart.h>
#endif /* CONFIG_SHARP_PANIC_ON_SUBSYS */

#if defined(CONFIG_SHARP_HANDLE_PANIC)
#include <asm/cacheflush.h>
#include <linux/qpnp/power-on.h>
#endif /* CONFIG_SHARP_HANDLE_PANIC */

/*
 *
 *
 *
 */
static shrlog_fixed_apps_info *shrlog_info = NULL;
extern unsigned long get_xtime_sec_addr(void);

int shrlog_is_enabled(void)
{
	return shrlog_info ? 1 : 0;
}

/*
 *
 *
 *
 */
#if defined(CONFIG_SHARP_PANIC_ON_SUBSYS)

int shrlog_subsystem_device_panic_hook(struct subsys_desc *desc)
{
	const char *name = NULL;
	if (!shrlog_info)
		return 0;
	if (!desc)
		return -EINVAL;
	name = desc->name;
	if (!name)
		return -EINVAL;
	if (!strcmp(name, "modem")) {
		msm_set_restart_mode(RESTART_MODEM_CRASH);
	}
	return 0;
}

int shrlog_subsystem_restart_hook(struct subsys_desc *desc)
{
	if (!shrlog_info)
		return 0;
	shrlog_subsystem_device_panic_hook(desc);
	msleep(500);
	panic("subsys-restart: Resetting the SoC - subsys crashed.\n");
	return 0;
}

#endif /* CONFIG_SHARP_PANIC_ON_SUBSYS */

/*
 *
 *
 *
 */
#if defined(CONFIG_SHARP_HANDLE_PANIC)

static int sharp_panic_cache_handler(struct notifier_block *this,
				     unsigned long event, void *ptr)
{
	if (!shrlog_info)
		return NOTIFY_DONE;
	/* Push out any further dirty data, and ensure cache is empty */
	flush_cache_all();
	/* outer_disable is not supported by 64bit kernel */
#ifndef CONFIG_ARM64
	/* Push out the dirty data from external caches */
	outer_disable();
#endif /* CONFIG_ARM64 */
	return NOTIFY_DONE;
}

static struct notifier_block panic_nb = {
	.notifier_call  = sharp_panic_cache_handler,
};

#endif /* CONFIG_SHARP_HANDLE_PANIC */

/*
 *
 *
 *
 */
#if defined(CONFIG_SHARP_HANDLE_PANIC)

static void *qcom_restart_reason = NULL;

static void shrlog_set_restart_reason(unsigned int reason_value)
{
	if (!shrlog_info)
		return;
	if (!qcom_restart_reason)
		return;
	__raw_writel(reason_value, qcom_restart_reason);
}

void sharp_panic_init_restart_reason_addr(void *addr)
{
	qcom_restart_reason = addr;
}

bool sharp_panic_needs_warm_reset(const char *cmd, bool dload_mode,
				  int restart_mode, int in_panic)
{
	bool need_warm_reset = false;
	if (!shrlog_info)
		return false;
	if (in_panic)
		need_warm_reset = true;
	return need_warm_reset;
}

void sharp_panic_clear_restart_reason(void)
{
	if (!shrlog_info)
		return;
	qpnp_pon_set_restart_reason(PON_RESTART_REASON_UNKNOWN);
	shrlog_set_restart_reason(0x00000000);
}
int sharp_panic_preset_restart_reason(int download_mode)
{
	if (!shrlog_info)
		return download_mode;
	qpnp_pon_set_restart_reason(PON_RESTART_REASON_WHILE_LINUX_RUNNING);
	shrlog_set_restart_reason(0x77665577);
	return 0;
}
void sharp_panic_set_restart_reason(const char *cmd, bool dload_mode,
				    int restart_mode, int in_panic)
{
	if (!shrlog_info)
		return;
	if (in_panic) {
		if (!dload_mode) {
			qpnp_pon_set_restart_reason(
				PON_RESTART_REASON_EMERGENCY_NODLOAD);
		}
		if (restart_mode == RESTART_MODEM_CRASH)
			shrlog_set_restart_reason(0x77665595);
		else
			shrlog_set_restart_reason(0x77665590);
	}
}

#endif /* CONFIG_SHARP_HANDLE_PANIC */


/*
 *
 *
 *
 */

static int shrlog_set_apps_info(shrlog_fixed_apps_info *apps_info)
{
#if defined(CONFIG_SHARP_SHLOG_RAMOOPS)
	int ret = 0;
	struct device_node *node;
#endif /* CONFIG_SHARP_SHLOG_RAMOOPS */
	if (!apps_info)
		return -ENOMEM;

	/*
	 *
	 */
	apps_info->adr.st.xtime_sec_addr = get_xtime_sec_addr();
	apps_info->adr.st._text_addr = (shrlog_kernel_t)(_text);
	apps_info->adr.st._stext_addr = (shrlog_kernel_t)(_stext);
	apps_info->adr.st._etext_addr = (shrlog_kernel_t)(_etext);
	apps_info->size_of_task_struct = sizeof(struct task_struct);
	apps_info->size_of_thread_struct = sizeof(struct thread_struct);
	apps_info->size_of_mm_struct = sizeof(struct mm_struct);
	apps_info->stack_offset = offsetof(struct task_struct, stack);
	apps_info->tasks_offset = offsetof(struct task_struct, tasks);
	apps_info->pid_offset = offsetof(struct task_struct, pid);
	apps_info->thread_group_offset = offsetof(struct task_struct,
						  thread_group);
	apps_info->comm_offset = offsetof(struct task_struct, comm);
	apps_info->mm_offset = offsetof(struct task_struct, mm);
	apps_info->pgd_offset = offsetof(struct mm_struct, pgd);
#ifdef CONFIG_ARM64
	apps_info->size_of_cpu_context = sizeof(struct cpu_context);
	apps_info->cpu_context_fp_offset =
		offsetof(struct task_struct, thread.cpu_context.fp);
	apps_info->cpu_context_sp_offset =
		offsetof(struct task_struct, thread.cpu_context.sp);
	apps_info->cpu_context_pc_offset =
		offsetof(struct task_struct, thread.cpu_context.pc);
#endif /* CONFIG_ARM64 */
#ifdef CONFIG_ARM
	apps_info->size_of_thread_info = sizeof(struct thread_info);
	apps_info->cpu_context_in_thread_info = offsetof(struct thread_info,
							 cpu_context);
	apps_info->size_of_cpu_context_save = sizeof(struct cpu_context_save);
	apps_info->cpu_context_save_fp_offset =
		offsetof(struct cpu_context_save, fp);
	apps_info->cpu_context_save_sp_offset =
		offsetof(struct cpu_context_save, sp);
	apps_info->cpu_context_save_pc_offset =
		offsetof(struct cpu_context_save, pc);
#endif /* CONFIG_ARM */

	/*
	 *
	 */
#if defined(CONFIG_SHARP_SHLOG_RAMOOPS)
	apps_info->oops_addr = 0;
	apps_info->oops_size = 0;
	node = of_find_compatible_node(NULL, NULL, "ramoops");
	if (node) {
		u32 oops_addr = 0;
		u32 oops_size = 0;
		ret = of_property_read_u32(node,
					   "android,ramoops-buffer-start",
					   &oops_addr);
		if (ret) {
			oops_size = 0;
		} else {
			ret = of_property_read_u32(node,
						   "android,ramoops-buffer-size",
						   &oops_size);
			if (ret)
				oops_size = 0;
		}
		apps_info->oops_addr = oops_addr;
		apps_info->oops_size = oops_size;

		if (oops_size) {
			ret = of_property_read_u32(node,
						   "android,ramoops-console-size",
						   &oops_size);
			apps_info->oops_console_size = ( ret ? 0 : oops_size );
			ret = of_property_read_u32(node,
						   "android,ramoops-record-size",
						   &oops_size);
			apps_info->oops_record_size = ( ret ? 0 : oops_size );
			ret = of_property_read_u32(node,
						   "android,ramoops-pmsg-size",
						   &oops_size);
			apps_info->oops_pmsg_size = ( ret ? 0 : oops_size );
		}
	}
#endif /* CONFIG_SHARP_SHLOG_RAMOOPS */

	/*
	 *
	 */
	apps_info->info_size = sizeof(*apps_info);
	apps_info->info_magic = SHRLOG_FIXED_LINUX_MAGIC_NUM;

	return 0;
}

static int shrlog_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct shrlog_ram_fixed_t *shrlog_ram_fixed = NULL;
	uint32_t offset_in_region = 0;
	struct device_node* pnode = NULL;
	void *ramfixed_ptr = NULL;
#if defined(CONFIG_SHARP_SHLOG_MODEM)
	u32 modem_addr = 0;
	u64 modem_size = 0;
	u32 modem_range[2];
#endif /* CONFIG_SHARP_SHLOG_MODEM */

	if (!pdev->dev.of_node) {
		pr_err("unable to find DT imem node\n");
		return -ENODEV;
	}

	shrlog_info = kzalloc(sizeof(*shrlog_info), GFP_KERNEL);

	if (!shrlog_info)
		return -ENOMEM;

	ret = of_property_read_u32(pdev->dev.of_node,
				   "sharp,shrlog-offset",
				   &offset_in_region);
	if (ret)
		offset_in_region = 0;
	pnode = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (pnode){
		ramfixed_ptr = of_iomap(pnode, 0);
		if (!ramfixed_ptr) {
			of_node_put(pnode);
		}
	} else {
		pnode = of_parse_phandle(pdev->dev.of_node,
					 "linux,contiguous-region", 0);
		if (pnode) {
			const u32 *addr;
			u64 mem_size;
			addr = of_get_address(pnode, 0, &mem_size, NULL);
			if (IS_ERR_OR_NULL(addr)) {
				ramfixed_ptr = NULL;
			} else {
				u32 mem_addr;
				mem_addr = (u32)of_read_ulong(addr, 2);
				ramfixed_ptr = ioremap(mem_addr, mem_size);
			}
			of_node_put(pnode);
		}
	}
	if (ramfixed_ptr)
		shrlog_ram_fixed =
			(struct shrlog_ram_fixed_t*)(((char*)ramfixed_ptr)+offset_in_region);
	else
		pr_debug("region is not reserved for %s\n", pdev->name);

	if (!shrlog_ram_fixed)
		return -ENOMEM;

	ret = shrlog_set_apps_info(shrlog_info);
	if (ret)
		return ret;

	shrlog_ram_fixed->shrlog_ram_fixed_addr =
		(shrlog_uint64)((shrlog_kernel_t)shrlog_info);
	shrlog_ram_fixed->linux_phys_offset = PHYS_OFFSET;

#if defined(CONFIG_SHARP_SHLOG_MODEM)
	ret = of_property_read_u32_array(pdev->dev.of_node,
					 "modem-range", modem_range,
					 ARRAY_SIZE(modem_range));
	if (!ret) {
		modem_addr = modem_range[0];
		modem_size = modem_range[1];
	} else {
		pnode = of_parse_phandle(pdev->dev.of_node, "modem-region", 0);
		if (pnode) {
			const u32 *addr;
			addr = of_get_address(pnode, 0, &modem_size, NULL);
			if (IS_ERR_OR_NULL(addr)) {
				modem_size = 0;
				modem_addr = 0;
			} else {
				modem_addr = (u32)of_read_ulong(addr, 2);
			}
			of_node_put(pnode);
		} else {
			modem_size = 0;
			modem_addr = 0;
		}
	}
	shrlog_ram_fixed->modem_region_start = modem_addr;
	shrlog_ram_fixed->modem_region_size = modem_size;
#endif /* CONFIG_SHARP_SHLOG_MODEM */

	shrlog_ram_fixed->magic_num1 = (shrlog_uint64)SHRLOG_FIXED_MAGIC_NUM;
	shrlog_ram_fixed->magic_num2 = (shrlog_uint64)SHRLOG_FIXED_MAGIC_NUM;

#if defined(CONFIG_SHARP_HANDLE_PANIC)
	if (!qcom_restart_reason) {
		struct device_node *node;
		node = of_find_compatible_node(NULL, NULL,
					       "qcom,msm-imem-restart_reason");
		if (!node) {
			pr_err("unable to find DT imem restart reason node\n");
		} else {
			qcom_restart_reason = of_iomap(node, 0);
			if (!qcom_restart_reason) {
				pr_err("unable to map imem restart reason offset\n");
				return -ENOMEM;
			}
		}
	}
	atomic_notifier_chain_register(&panic_notifier_list, &panic_nb);
#endif /* CONFIG_SHARP_HANDLE_PANIC */

	return 0;
}

static int shrlog_remove(struct platform_device *pdev)
{
	/* not supported */
	return 0;
}

/*
 *
 *
 *
 */
#define SHRLOG_COMPAT_STR       "sharp,shrlog"

static struct of_device_id shrlog_match_table[] = {
	{
		.compatible = SHRLOG_COMPAT_STR
	},
	{},
};

static struct platform_driver shrlog_driver = {
	.probe = shrlog_probe,
	.remove = shrlog_remove,
	.driver = {
		.name = "shrlog",
		.of_match_table = shrlog_match_table
	},
};

static int __init shrlog_init(void)
{
	return platform_driver_register(&shrlog_driver);
}
static void __exit shrlog_exit(void)
{
	platform_driver_unregister(&shrlog_driver);
}

subsys_initcall(shrlog_init);
module_exit(shrlog_exit);

