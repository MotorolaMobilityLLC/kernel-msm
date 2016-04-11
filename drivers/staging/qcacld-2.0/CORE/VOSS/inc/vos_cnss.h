/*
 * Copyright (c) 2015-2016 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#ifndef _VOS_CNSS_H
#define _VOS_CNSS_H

#include "vos_status.h"
#ifdef CONFIG_CNSS
#include <net/cnss.h>
#endif

#if defined(WLAN_OPEN_SOURCE) && !defined(CONFIG_CNSS)
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/sched.h>

enum cnss_bus_width_type {
	CNSS_BUS_WIDTH_NONE,
	CNSS_BUS_WIDTH_LOW,
	CNSS_BUS_WIDTH_MEDIUM,
	CNSS_BUS_WIDTH_HIGH
};

static inline void vos_wlan_pci_link_down(void){ return; }
static inline int vos_pcie_shadow_control(struct pci_dev *dev, bool enable)
{
	return -ENODEV;
}

static inline u8 *vos_get_cnss_wlan_mac_buff(struct device *dev, uint32_t *num)
{
	*num = 0;
	return NULL;
}

static inline void vos_init_work(struct work_struct *work, work_func_t func)
{
	INIT_WORK(work, func);
}
static inline void vos_flush_work(void *work)
{
	cancel_work_sync(work);
}

static inline void vos_init_delayed_work(struct delayed_work *work,
					work_func_t func)
{
	INIT_DELAYED_WORK(work, func);
}

static inline void vos_flush_delayed_work(void *dwork)
{
	cancel_delayed_work_sync(dwork);
}

static inline void vos_pm_wake_lock_init(struct wakeup_source *ws,
					const char *name)
{
	wakeup_source_init(ws, name);
}

static inline void vos_pm_wake_lock(struct wakeup_source *ws)
{
	__pm_stay_awake(ws);
}

static inline void vos_pm_wake_lock_timeout(struct wakeup_source *ws,
					ulong msec)
{
	 __pm_wakeup_event(ws, msec);
}

static inline void vos_pm_wake_lock_release(struct wakeup_source *ws)
{
	__pm_relax(ws);
}

static inline void vos_pm_wake_lock_destroy(struct wakeup_source *ws)
{
	wakeup_source_trash(ws);
}

static inline int vos_wlan_pm_control(bool vote)
{
	return 0;
}
static inline void vos_lock_pm_sem(void) { return; }
static inline void vos_release_pm_sem(void) { return; }

static inline void vos_get_monotonic_bootime_ts(struct timespec *ts)
{
	get_monotonic_boottime(ts);
}

static inline void vos_get_boottime_ts(struct timespec *ts)
{
	ktime_get_ts(ts);
}

static inline void *vos_get_virt_ramdump_mem(struct device *dev,
						unsigned long *size)
{
	return NULL;
}

static inline void vos_device_crashed(struct device *dev) { return; }

#ifdef QCA_CONFIG_SMP
static inline int vos_set_cpus_allowed_ptr(struct task_struct *task, ulong cpu)
{
	return set_cpus_allowed_ptr(task, cpumask_of(cpu));
}
#else
static inline int vos_set_cpus_allowed_ptr(struct task_struct *task, ulong cpu)
{
	return 0;
}
#endif

static inline void vos_device_self_recovery(struct device *dev) { return; }
static inline void vos_request_pm_qos_type(int latency_type, u32 qos_val)
{
	return;
}
static inline void vos_remove_pm_qos(void) { return; }
static inline int vos_request_bus_bandwidth(struct device *dev, int bandwidth)
{
	return 0;
}
static inline int vos_get_platform_cap(void *cap) { return 1; }
static inline void vos_set_driver_status(int status) { return; }
static inline int vos_get_bmi_setup(void) { return 0; }
static inline int vos_get_sha_hash(const u8 *data, u32 data_len,
				u8 *hash_idx, u8 *out)
{
	return 1;
}
static inline void *vos_get_fw_ptr(void) { return NULL; }
static inline int vos_auto_suspend(void) { return 0; }
static inline int vos_auto_resume(void) { return 0; }
static inline void vos_runtime_init(struct device *dev, int auto_delay)
{
	return;
}
static inline void vos_runtime_exit(struct device *dev) { return; }
static inline int vos_set_wlan_unsafe_channel(u16 *unsafe_ch_list,
					u16 ch_count)
{
	return -EINVAL;
}

static inline int vos_get_wlan_unsafe_channel(u16 *unsafe_ch_list,
					u16 *ch_count, u16 buf_len)
{
	return -EINVAL;
}

static inline int vos_wlan_set_dfs_nol(const void *info, u16 info_len)
{
	return -EINVAL;
}

static inline int vos_wlan_get_dfs_nol(void *info, u16 info_len)
{
	return -EINVAL;
}

static inline void vos_get_monotonic_boottime_ts(struct timespec *ts)
{
	get_monotonic_boottime(ts);
}

static inline void vos_schedule_recovery_work(struct device *dev) { return; }

static inline bool vos_is_ssr_fw_dump_required(void)
{
	return true;
}

static inline int vos_update_boarddata(unsigned char *buf, unsigned int len)
{
	return 0;
}

static inline int vos_cache_boarddata(unsigned int offset,
	unsigned int len, unsigned char *buf)
{
	return 0;
}

typedef void (*oob_irq_handler_t) (void *dev_para);
static inline bool vos_oob_enabled(void)
{
	return false;
}

static inline int vos_register_oob_irq_handler(oob_irq_handler_t handler,
		void *pm_oob)
{
	return -ENOSYS;
}

static inline int vos_unregister_oob_irq_handler(void *pm_oob)
{
	return -ENOSYS;
}

static inline void vos_dump_stack (struct task_struct *task)
{
}
#else /* END WLAN_OPEN_SOURCE and !CONFIG_CNSS */
static inline void vos_dump_stack (struct task_struct *task)
{
	cnss_dump_stack(task);
}

static inline u8 *vos_get_cnss_wlan_mac_buff(struct device *dev, uint32_t *num)
{
	return cnss_common_get_wlan_mac_address(dev, num);
}

static inline void vos_init_work(struct work_struct *work, work_func_t func)
{
	cnss_init_work(work, func);
}

static inline void vos_flush_work(void *work)
{
	cnss_flush_work(work);
}

static inline void vos_flush_delayed_work(void *dwork)
{
	cnss_flush_delayed_work(dwork);
}

static inline void vos_pm_wake_lock_init(struct wakeup_source *ws,
					const char *name)
{
	cnss_pm_wake_lock_init(ws, name);
}

static inline void vos_pm_wake_lock(struct wakeup_source *ws)
{
	cnss_pm_wake_lock(ws);
}

static inline void vos_pm_wake_lock_timeout(struct wakeup_source *ws,
					ulong msec)
{
	cnss_pm_wake_lock_timeout(ws, msec);
}

static inline void vos_pm_wake_lock_release(struct wakeup_source *ws)
{
	cnss_pm_wake_lock_release(ws);
}

static inline void vos_pm_wake_lock_destroy(struct wakeup_source *ws)
{
	cnss_pm_wake_lock_destroy(ws);
}

static inline void vos_get_monotonic_boottime_ts(struct timespec *ts)
{
        cnss_get_monotonic_boottime(ts);
}

#if defined(CONFIG_CNSS) && defined(HIF_PCI)
static inline void vos_set_driver_status(int status)
{
	cnss_set_driver_status(status ? CNSS_LOAD_UNLOAD : CNSS_INITIALIZED);
}
#else
static inline void vos_set_driver_status(int status) {}
#endif

static inline int vos_wlan_set_dfs_nol(const void *info, u16 info_len)
{
	return cnss_wlan_set_dfs_nol(info, info_len);
}

static inline void vos_init_delayed_work(struct delayed_work *work,
					work_func_t func)
{
       cnss_init_delayed_work(work, func);
}

static inline void *vos_get_virt_ramdump_mem(struct device *dev,
						unsigned long *size)
{
	return cnss_common_get_virt_ramdump_mem(dev, size);
}

static inline int vos_wlan_get_dfs_nol(void *info, u16 info_len)
{
	return cnss_wlan_get_dfs_nol(info, info_len);
}

static inline void vos_get_boottime_ts(struct timespec *ts)
{
        cnss_get_boottime(ts);
}

#ifdef HIF_SDIO
static inline void vos_request_pm_qos_type(int latency_type, u32 qos_val)
{
	cnss_sdio_request_pm_qos_type(latency_type, qos_val);
}
#elif defined(HIF_PCI)
static inline void vos_request_pm_qos_type(int latency_type, u32 qos_val)
{
	cnss_pci_request_pm_qos_type(latency_type, qos_val);
}
#endif

#ifdef HIF_SDIO
static inline void vos_remove_pm_qos(void)
{
	cnss_sdio_remove_pm_qos();
}
#elif defined(HIF_PCI)
static inline void vos_remove_pm_qos(void)
{
	cnss_pci_remove_pm_qos();
}
#endif

static inline int vos_vendor_cmd_reply(struct sk_buff *skb)
{
        return cnss_vendor_cmd_reply(skb);
}

static inline int vos_set_wlan_unsafe_channel(u16 *unsafe_ch_list,
					u16 ch_count)
{
	return cnss_set_wlan_unsafe_channel(unsafe_ch_list, ch_count);
}

static inline int vos_get_wlan_unsafe_channel(u16 *unsafe_ch_list,
					u16 *ch_count, u16 buf_len)
{
	return cnss_get_wlan_unsafe_channel(unsafe_ch_list, ch_count, buf_len);
}

#ifdef CONFIG_CNSS
static inline void vos_schedule_recovery_work(struct device *dev)
{
	cnss_common_schedule_recovery_work(dev);
}

static inline void vos_device_crashed(struct device *dev)
{
	cnss_common_device_crashed(dev);
}

static inline void vos_device_self_recovery(struct device *dev)
{
	cnss_common_device_self_recovery(dev);
}

#else
static inline void vos_schedule_recovery_work(struct device *dev) {};

static inline void vos_device_crashed(struct device *dev) {};

static inline void vos_device_self_recovery(struct device *dev) {};

#endif

#if  defined(CONFIG_CNSS) && defined(HIF_SDIO)
static inline bool vos_is_ssr_fw_dump_required(void)
{
	return (cnss_get_restart_level() != CNSS_RESET_SUBSYS_COUPLED);
}
#else
static inline bool vos_is_ssr_fw_dump_required(void)
{
	return true;
}
#endif

#if defined(CONFIG_CNSS) && defined(HIF_PCI)
static inline void vos_lock_pm_sem(void)
{
	cnss_lock_pm_sem();
}
#else
static inline void vos_lock_pm_sem(void) {}
#endif

#if defined(CONFIG_CNSS) && defined(HIF_PCI)
static inline void vos_release_pm_sem(void)
{
	cnss_release_pm_sem();
}
#else
static inline void vos_release_pm_sem(void) {}
#endif

#ifdef FEATURE_BUS_BANDWIDTH
static inline int vos_request_bus_bandwidth(struct device *dev, int bandwidth)
{
	return cnss_common_request_bus_bandwidth(dev, bandwidth);
}
#endif

#if defined(CONFIG_CNSS) && defined(HIF_PCI) && defined(CONFIG_CNSS_SECURE_FW)
static inline int vos_get_sha_hash(const u8 *data, u32 data_len,
				u8 *hash_idx, u8 *out)
{
	return cnss_get_sha_hash(data, data_len, hash_idx, out);
}
#else
static inline int vos_get_sha_hash(const u8 *data, u32 data_len,
				u8 *hash_idx, u8 *out)
{
	return -EINVAL;
}
#endif

static inline int vos_set_cpus_allowed_ptr(struct task_struct *task, ulong cpu)
{
        return cnss_set_cpus_allowed_ptr(task, cpu);
}

#if defined(CONFIG_CNSS) && defined(HIF_PCI)
#ifdef CONFIG_PCI_MSM
static inline int vos_wlan_pm_control(bool vote)
{
	return cnss_wlan_pm_control(vote);
}
#else
static inline int vos_wlan_pm_control(bool vote)
{
	return 0;
}
#endif

static inline int vos_get_platform_cap(void *cap)
{
	return cnss_get_platform_cap(cap);
}

static inline int vos_get_bmi_setup(void)
{
	return cnss_get_bmi_setup();
}

#ifdef CONFIG_CNSS_SECURE_FW
static inline void *vos_get_fw_ptr(void)
{
	return cnss_get_fw_ptr();
}
#else
static inline void *vos_get_fw_ptr(void)
{
	return NULL;
}
#endif

static inline int vos_auto_suspend(void)
{
	return cnss_auto_suspend();
}

static inline int vos_auto_resume(void)
{
	return cnss_auto_resume();
}

static inline int vos_pm_runtime_request(struct device *dev,
				enum cnss_runtime_request request)
{
	return cnss_pm_runtime_request(dev, request);
}

static inline void vos_runtime_init(struct device *dev, int auto_delay)
{
	cnss_runtime_init(dev, auto_delay);
}

static inline void vos_runtime_exit(struct device *dev)
{
	cnss_runtime_exit(dev);
}
#endif

#if defined(CONFIG_CNSS) && defined(HIF_PCI)
static inline void vos_wlan_pci_link_down(void)
{
	cnss_wlan_pci_link_down();
}

static inline int vos_pcie_shadow_control(struct pci_dev *dev, bool enable)
{
	return cnss_pcie_shadow_control(dev, enable);
}
#else
static inline void vos_wlan_pci_link_down(void){ return; }
static inline int vos_pcie_shadow_control(struct pci_dev *dev, bool enable)
{
	return -ENODEV;
}
#endif

#if defined(CONFIG_CNSS) && defined(HIF_SDIO) && defined(WLAN_SCPC_FEATURE)
static inline int vos_update_boarddata(unsigned char *buf, unsigned int len)
{
	return cnss_update_boarddata(buf, len);
}

static inline int vos_cache_boarddata(unsigned int offset,
	unsigned int len, unsigned char *buf)
{
	return cnss_cache_boarddata(buf, len, offset);
}
#else
static inline int vos_update_boarddata(unsigned char *buf, unsigned int len)
{
	return 0;
}

static inline int vos_cache_boarddata(unsigned int offset,
	unsigned int len, unsigned char *buf)
{
	return 0;
}
#endif

#if defined(CONFIG_CNSS) && defined(HIF_SDIO)
static inline bool vos_oob_enabled(void)
{
	bool enabled = true;

	if (-ENOSYS == cnss_wlan_query_oob_status())
		enabled = false;

	return enabled;
}

static inline int vos_register_oob_irq_handler(oob_irq_handler_t handler,
		void *pm_oob)
{
	return cnss_wlan_register_oob_irq_handler(handler, pm_oob);
}

static inline int vos_unregister_oob_irq_handler(void *pm_oob)
{
	return cnss_wlan_unregister_oob_irq_handler(pm_oob);
}
#else
typedef void (*oob_pci_irq_handler_t) (void *dev_para);
static inline bool vos_oob_enabled(void)
{
	return false;
}

static inline int vos_register_oob_irq_handler(oob_pci_irq_handler_t handler,
		void *pm_oob)
{
	return -ENOSYS;
}

static inline int vos_unregister_oob_irq_handler(void *pm_oob)
{
	return -ENOSYS;
}
#endif /*END CONFIG_CNSS_SDIO */
#endif /* CONFIG_CNSS */

#endif/* _VOS_CNSS_H */
