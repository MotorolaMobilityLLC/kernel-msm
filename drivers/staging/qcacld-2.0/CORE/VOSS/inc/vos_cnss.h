/*
 * Copyright (c) 2015 The Linux Foundation. All rights reserved.
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
#include <net/cnss.h>

#if defined(WLAN_OPEN_SOURCE) && !defined(CONFIG_CNSS)
#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/sched.h>

enum cnss_bus_width_type {
	CNSS_BUS_WIDTH_NONE,
	CNSS_BUS_WIDTH_LOW,
	CNSS_BUS_WIDTH_MEDIUM,
	CNSS_BUS_WIDTH_HIGH
};

static inline void
vos_init_work(struct work_struct *work, work_func_t func)
{
	INIT_WORK(work, func);
}
static inline void vos_flush_work(void *work)
{
	cancel_work_sync(work);
}

static inline void
vos_init_delayed_work(struct delayed_work *work, work_func_t func)
{
	INIT_DELAYED_WORK(work, func);
}

static inline void vos_flush_delayed_work(void *dwork)
{
	cancel_delayed_work_sync(dwork);
}

#ifdef CONFIG_HAS_WAKELOCK
static inline void
vos_pm_wake_lock_init(struct wake_lock *wl, const char *name)
{
	wake_lock_init(wl, WAKE_LOCK_SUSPEND, name);
}

static inline void vos_pm_wake_lock(struct wake_lock *wl)
{
	wake_lock(wl);
}

static inline void
vos_pm_wake_lock_timeout(struct wake_lock *wl, ulong msec)
{
	wake_lock_timeout(wl, msecs_to_jiffies(msec));
}

static inline void vos_pm_wake_lock_release(struct wake_lock *wl)
{
	wake_unlock(wl);
}

static inline void vos_pm_wake_lock_destroy(struct wake_lock *wl)
{
	wake_lock_destroy(wl);
}
#else
static inline void vos_pm_wake_lock_init(int *wl, const char *name) { }
static inline void vos_pm_wake_lock(int *wl) { }
static inline void vos_pm_wake_lock_timeout(int *wl, ulong msec) { }
static inline void vos_pm_wake_lock_release(int *wl) { }
static inline void vos_pm_wake_lock_destroy(int *wl) { }
#endif

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

static inline int
vos_get_ramdump_mem(unsigned long *address, unsigned long *size)
{
	return 0;
}

static inline void *vos_get_virt_ramdump_mem(unsigned long *size)
{
	return NULL;
}

static inline void vos_device_crashed(void) { return; }
static inline int vos_set_cpus_allowed_ptr(struct task_struct *task, ulong cpu)
{
	return set_cpus_allowed_ptr(task, cpumask_of(cpu));
}

static inline void vos_device_self_recovery(void) { return; }
static inline void vos_request_pm_qos(u32 qos_val) { return; }
static inline void vos_remove_pm_qos(void) { return; }
static inline int vos_request_bus_bandwidth(int bandwidth) { return 0; }
static inline int vos_get_platform_cap(void *cap) { return 1; }
static inline void vos_set_driver_status(int status) { return; }
static inline int vos_get_bmi_setup(void) { return 0; }
static inline int
vos_get_sha_hash(const u8 *data, u32 data_len, u8 *hash_idx, u8 *out)
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
static inline int
vos_set_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 ch_count)
{
	return -EINVAL;
}

static inline int
vos_get_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 *ch_count, u16 buf_len)
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

static inline void vos_schedule_recovery_work(void) { return; }
#else
static inline void
vos_init_work(struct work_struct *work, work_func_t func)
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

static inline void
vos_pm_wake_lock_init(struct wakeup_source *ws, const char *name)
{
	cnss_pm_wake_lock_init(ws, name);
}

static inline void vos_pm_wake_lock(struct wakeup_source *ws)
{
	cnss_pm_wake_lock(ws);
}

static inline void
vos_pm_wake_lock_timeout(struct wakeup_source *ws, ulong msec)
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

#ifdef CONFIG_CNSS_PCI
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

static inline void
vos_init_delayed_work(struct delayed_work *work, work_func_t func)
{
       cnss_init_delayed_work(work, func);
}

#ifdef CONFIG_CNSS_PCI
static inline void *vos_get_virt_ramdump_mem(unsigned long *size)
{
	return cnss_get_virt_ramdump_mem(size);
}
#else
static inline void *vos_get_virt_ramdump_mem(unsigned long *size)
{
	return NULL;
}
#endif

static inline int vos_wlan_get_dfs_nol(void *info, u16 info_len)
{
	return cnss_wlan_get_dfs_nol(info, info_len);
}

static inline void vos_get_boottime_ts(struct timespec *ts)
{
        cnss_get_boottime(ts);
}

#ifdef CONFIG_CNSS_PCI
static inline void vos_request_pm_qos(u32 qos_val)
{
	cnss_request_pm_qos(qos_val);
}
#else
static inline void vos_request_pm_qos(u32 qos_val) {}
#endif

#ifdef CONFIG_CNSS_PCI
static inline void vos_remove_pm_qos(void)
{
	cnss_remove_pm_qos();
}
#else
static inline void vos_remove_pm_qos(void) {}
#endif

static inline int vos_vendor_cmd_reply(struct sk_buff *skb)
{
        return cnss_vendor_cmd_reply(skb);
}

static inline int
vos_set_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 ch_count)
{
	return cnss_set_wlan_unsafe_channel(unsafe_ch_list, ch_count);
}

static inline int
vos_get_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 *ch_count, u16 buf_len)
{
	return cnss_get_wlan_unsafe_channel(unsafe_ch_list, ch_count, buf_len);
}

#if defined(CONFIG_CNSS_PCI) || defined(CONFIG_CNSS_SDIO)
static inline void vos_schedule_recovery_work(void)
{
	cnss_schedule_recovery_work();
}

static inline void vos_device_crashed(void)
{
	cnss_device_crashed();
}

static inline void vos_device_self_recovery(void)
{
	cnss_device_self_recovery();
}

#else
static inline void vos_schedule_recovery_work(void) {};

static inline void vos_device_crashed(void) {};

static inline void vos_device_self_recovery(void) {};

#endif

#ifdef CONFIG_CNSS_PCI
static inline void vos_lock_pm_sem(void)
{
	cnss_lock_pm_sem();
}
#else
static inline void vos_lock_pm_sem(void) {}
#endif

#ifdef CONFIG_CNSS_PCI
static inline void vos_release_pm_sem(void)
{
	cnss_release_pm_sem();
}
#else
static inline void vos_release_pm_sem(void) {}
#endif

#ifdef FEATURE_BUS_BANDWIDTH
static inline int vos_request_bus_bandwidth(int bandwidth)
{
	return cnss_request_bus_bandwidth(bandwidth);
}
#else
static inline int vos_request_bus_bandwidth(int bandwidth)
{
	return 0;
}
#endif

#ifdef CONFIG_CNSS_PCI
static inline int
vos_get_sha_hash(const u8 *data, u32 data_len, u8 *hash_idx, u8 *out)
{
	return cnss_get_sha_hash(data, data_len, hash_idx, out);
}
#else
static inline int
vos_get_sha_hash(const u8 *data, u32 data_len, u8 *hash_idx, u8 *out)
{
	return -EINVAL;
}
#endif

static inline int vos_set_cpus_allowed_ptr(struct task_struct *task, ulong cpu)
{
        return cnss_set_cpus_allowed_ptr(task, cpu);
}

#ifdef CONFIG_CNSS_PCI
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

static inline int
vos_get_ramdump_mem(unsigned long *address, unsigned long *size)
{
	return cnss_get_ramdump_mem(address, size);
}

static inline int vos_get_platform_cap(void *cap)
{
	return cnss_get_platform_cap(cap);
}

static inline int vos_get_bmi_setup(void)
{
	return cnss_get_bmi_setup();
}

static inline void *vos_get_fw_ptr(void)
{
	return cnss_get_fw_ptr();
}

static inline int vos_auto_suspend(void)
{
	return cnss_auto_suspend();
}

static inline int vos_auto_resume(void)
{
	return cnss_auto_resume();
}

static inline int
vos_pm_runtime_request(struct device *dev, enum cnss_runtime_request request)
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

static inline void vos_wlan_pci_link_down(void)
{
	cnss_wlan_pci_link_down();
}

static inline int vos_pcie_shadow_control(struct pci_dev *dev, bool enable)
{
	return cnss_pcie_shadow_control(dev, enable);
}
#endif
#endif
#endif/* _VOS_CNSS_H */
