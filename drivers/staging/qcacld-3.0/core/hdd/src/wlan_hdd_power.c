/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

/**
 * DOC: wlan_hdd_power.c
 *
 * WLAN power management functions
 *
 */

/* Include files */

#include <linux/pm.h>
#include <linux/wait.h>
#include <linux/cpu.h>
#include "osif_sync.h"
#include <wlan_hdd_includes.h>
#if defined(WLAN_OPEN_SOURCE) && defined(CONFIG_HAS_WAKELOCK)
#include <linux/wakelock.h>
#endif
#include "qdf_types.h"
#include "sme_api.h"
#include <cds_api.h>
#include <cds_sched.h>
#include <mac_init_api.h>
#include <wlan_qct_sys.h>
#include <wlan_hdd_main.h>
#include <wlan_hdd_assoc.h>
#include <wlan_nlink_srv.h>
#include <wlan_hdd_misc.h>
#include <wlan_hdd_power.h>
#include <wlan_hdd_host_offload.h>
#include <dbglog_host.h>
#include <wlan_hdd_trace.h>
#include <wlan_hdd_p2p.h>

#include <linux/semaphore.h>
#include <wlan_hdd_hostapd.h>

#include <linux/inetdevice.h>
#include <wlan_hdd_cfg.h>
#include <wlan_hdd_scan.h>
#include <wlan_hdd_stats.h>
#include <wlan_hdd_cfg80211.h>
#include <net/addrconf.h>
#include <wlan_hdd_lpass.h>

#include <wma_types.h>
#include <ol_txrx_osif_api.h>
#include <ol_defines.h>
#include "hif.h"
#include "hif_unit_test_suspend.h"
#include "sme_power_save_api.h"
#include "wlan_policy_mgr_api.h"
#include "cdp_txrx_flow_ctrl_v2.h"
#include "pld_common.h"
#include "wlan_hdd_driver_ops.h"
#include <wlan_logging_sock_svc.h>
#include "scheduler_api.h"
#include "cds_utils.h"
#include "wlan_hdd_packet_filter_api.h"
#include "wlan_cfg80211_scan.h"
#include <dp_txrx.h>
#include "wlan_ipa_ucfg_api.h"
#include <wlan_cfg80211_mc_cp_stats.h>
#include "wlan_p2p_ucfg_api.h"
#include "wlan_mlme_ucfg_api.h"
#include "wlan_osif_request_manager.h"
#include <wlan_hdd_sar_limits.h>
#include "wlan_pkt_capture_ucfg_api.h"
#include "wlan_hdd_thermal.h"
#include "wlan_hdd_object_manager.h"
#include <linux/igmp.h>
#include "qdf_types.h"
/* Preprocessor definitions and constants */
#ifdef QCA_WIFI_NAPIER_EMULATION
#define HDD_SSR_BRING_UP_TIME 3000000
#else
#define HDD_SSR_BRING_UP_TIME 30000
#endif

/* Type declarations */

#ifdef FEATURE_WLAN_DIAG_SUPPORT
void hdd_wlan_suspend_resume_event(uint8_t state)
{
	WLAN_HOST_DIAG_EVENT_DEF(suspend_state, struct host_event_suspend);
	qdf_mem_zero(&suspend_state, sizeof(suspend_state));

	suspend_state.state = state;
	WLAN_HOST_DIAG_EVENT_REPORT(&suspend_state, EVENT_WLAN_SUSPEND_RESUME);
}

/**
 * hdd_wlan_offload_event()- send offloads event
 * @type: offload type
 * @state: enabled or disabled
 *
 * This Function send offloads enable/disable diag event
 *
 * Return: void.
 */

void hdd_wlan_offload_event(uint8_t type, uint8_t state)
{
	WLAN_HOST_DIAG_EVENT_DEF(host_offload, struct host_event_offload_req);
	qdf_mem_zero(&host_offload, sizeof(host_offload));

	host_offload.offload_type = type;
	host_offload.state = state;

	WLAN_HOST_DIAG_EVENT_REPORT(&host_offload, EVENT_WLAN_OFFLOAD_REQ);
}
#endif

#ifdef QCA_CONFIG_SMP

/* timeout in msec to wait for RX_THREAD to suspend */
#define HDD_RXTHREAD_SUSPEND_TIMEOUT 200

void wlan_hdd_rx_thread_resume(struct hdd_context *hdd_ctx)
{
	if (hdd_ctx->is_ol_rx_thread_suspended) {
		cds_resume_rx_thread();
		hdd_ctx->is_ol_rx_thread_suspended = false;
	}
}

int wlan_hdd_rx_thread_suspend(struct hdd_context *hdd_ctx)
{
	p_cds_sched_context cds_sched_context = get_cds_sched_ctxt();
	int rc;

	if (!cds_sched_context)
		return 0;

	/* Suspend tlshim rx thread */
	set_bit(RX_SUSPEND_EVENT, &cds_sched_context->ol_rx_event_flag);
	wake_up_interruptible(&cds_sched_context->ol_rx_wait_queue);
	rc = wait_for_completion_timeout(&cds_sched_context->
					 ol_suspend_rx_event,
					 msecs_to_jiffies
					 (HDD_RXTHREAD_SUSPEND_TIMEOUT)
					);
	if (!rc) {
		clear_bit(RX_SUSPEND_EVENT,
			  &cds_sched_context->ol_rx_event_flag);
		hdd_err("Failed to stop tl_shim rx thread");
		return -EINVAL;
	}
	hdd_ctx->is_ol_rx_thread_suspended = true;

	return 0;
}
#endif /* QCA_CONFIG_SMP */

/**
 * hdd_enable_gtk_offload() - enable GTK offload
 * @adapter: pointer to the adapter
 *
 * Central function to enable GTK offload.
 *
 * Return: nothing
 */
static void hdd_enable_gtk_offload(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		return;
	}

	status = ucfg_pmo_enable_gtk_offload_in_fwr(vdev);
	if (status != QDF_STATUS_SUCCESS)
		hdd_info("Failed to enable gtk offload");
	hdd_objmgr_put_vdev(vdev);
}

#ifdef WLAN_FEATURE_IGMP_OFFLOAD
/**
 * hdd_send_igmp_offload_params() - enable igmp offload
 * @adapter: pointer to the adapter
 * @enable: enable/disable
 *
 * Return: nothing
 */
static QDF_STATUS
hdd_send_igmp_offload_params(struct hdd_adapter *adapter,
			     bool enable)
{
	struct wlan_objmgr_vdev *vdev;
	struct in_device *in_dev = adapter->dev->ip_ptr;
	struct ip_mc_list *ip_list;
	struct pmo_igmp_offload_req *igmp_req = NULL;
	int count = 0;
	QDF_STATUS status;

	if (!in_dev) {
		hdd_err("in_dev is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	ip_list = in_dev->mc_list;
	if (!ip_list) {
		hdd_debug("ip list empty");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	igmp_req = qdf_mem_malloc(sizeof(*igmp_req));
	if (!igmp_req) {
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	while (ip_list && ip_list->multiaddr && enable &&
	       count < MAX_MC_IP_ADDR) {
		if (IGMP_QUERY_ADDRESS !=  ip_list->multiaddr) {
			igmp_req->grp_ip_address[count] = ip_list->multiaddr;
			count++;
		}

		ip_list = ip_list->next;
	}
	igmp_req->enable = enable;
	igmp_req->num_grp_ip_address = count;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		qdf_mem_free(igmp_req);
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	status = ucfg_pmo_enable_igmp_offload(vdev, igmp_req);
	if (status != QDF_STATUS_SUCCESS)
		hdd_info("Failed to enable igmp offload");

	hdd_objmgr_put_vdev(vdev);
	qdf_mem_free(igmp_req);
out:
	return status;
}

/**
 * hdd_enable_igmp_offload() - enable GTK offload
 * @adapter: pointer to the adapter
 *
 * Enable IGMP offload in suspended case to save power
 *
 * Return: nothing
 */
static void hdd_enable_igmp_offload(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		return;
	}
	status = hdd_send_igmp_offload_params(adapter, true);
	if (status != QDF_STATUS_SUCCESS)
		hdd_info("Failed to enable igmp offload");
	hdd_objmgr_put_vdev(vdev);
}

/**
 * hdd_disable_igmp_offload() - disable IGMP offload
 * @adapter: pointer to the adapter
 *
 * Enable IGMP offload in suspended case to save power
 *
 * Return: nothing
 */
static void hdd_disable_igmp_offload(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		return;
	}
	status = hdd_send_igmp_offload_params(adapter, false);
	if (status != QDF_STATUS_SUCCESS)
		hdd_info("Failed to enable igmp offload");
	hdd_objmgr_put_vdev(vdev);
}
#else
static inline void
hdd_enable_igmp_offload(struct hdd_adapter *adapter)
{}

static inline QDF_STATUS
hdd_send_igmp_offload_params(struct hdd_adapter *adapter,
			     bool enable)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_hdd_send_igmp_offload_params(struct hdd_adapter *adapter, bool enable,
				  uint32_t *mc_address, uint32_t count)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
hdd_disable_igmp_offload(struct hdd_adapter *adapter)
{}
#endif

/**
 * hdd_disable_gtk_offload() - disable GTK offload
 * @adapter:   pointer to the adapter
 *
 * Central function to disable GTK offload.
 *
 * Return: nothing
 */
static void hdd_disable_gtk_offload(struct hdd_adapter *adapter)
{
	struct pmo_gtk_rsp_req gtk_rsp_request;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	/* ensure to get gtk rsp first before disable it*/
	gtk_rsp_request.callback = wlan_hdd_cfg80211_update_replay_counter_cb;

	/* Passing as void* as PMO does not know legacy HDD adapter type */
	gtk_rsp_request.callback_context = (void *)adapter;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		return;
	}

	status = ucfg_pmo_get_gtk_rsp(vdev, &gtk_rsp_request);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Failed to send get gtk rsp status:%d", status);
		goto put_vdev;
	}

	hdd_debug("send get_gtk_rsp successful");
	status = ucfg_pmo_disable_gtk_offload_in_fwr(vdev);
	if (status != QDF_STATUS_SUCCESS)
		hdd_info("Failed to disable gtk offload");
put_vdev:
	hdd_objmgr_put_vdev(vdev);
}

#ifdef WLAN_NS_OFFLOAD
/**
 * __wlan_hdd_ipv6_changed() - IPv6 notifier callback function
 * @net_dev: net_device whose IP address changed
 * @event: event from kernel, NETDEV_UP or NETDEV_DOWN
 *
 * This is a callback function that is registered with the kernel via
 * register_inet6addr_notifier() which allows the driver to be
 * notified when there is an IPv6 address change.
 *
 * Return: None
 */
static void __wlan_hdd_ipv6_changed(struct net_device *net_dev,
				    unsigned long event)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(net_dev);
	struct hdd_context *hdd_ctx;
	int errno;

	hdd_enter_dev(net_dev);

	errno = hdd_validate_adapter(adapter);
	if (errno || adapter->dev != net_dev)
		goto exit;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		goto exit;

	/* Only need to be notified for ipv6_add_addr
	 * No need for ipv6_del_addr or addrconf_ifdown
	 */
	if (event == NETDEV_UP &&
	    (adapter->device_mode == QDF_STA_MODE ||
	     adapter->device_mode == QDF_P2P_CLIENT_MODE)) {
		hdd_debug("invoking sme_dhcp_done_ind");
		sme_dhcp_done_ind(hdd_ctx->mac_handle, adapter->vdev_id);
		schedule_work(&adapter->ipv6_notifier_work);
	}

exit:
	hdd_exit();
}

int wlan_hdd_ipv6_changed(struct notifier_block *nb,
			  unsigned long data, void *context)
{
	struct inet6_ifaddr *ifa = context;
	struct net_device *net_dev = ifa->idev->dev;
	struct osif_vdev_sync *vdev_sync;

	if (osif_vdev_sync_op_start(net_dev, &vdev_sync))
		return NOTIFY_DONE;

	__wlan_hdd_ipv6_changed(net_dev, data);

	osif_vdev_sync_op_stop(vdev_sync);

	return NOTIFY_DONE;
}

/**
 * hdd_fill_ipv6_uc_addr() - fill IPv6 unicast addresses
 * @idev: pointer to net device
 * @ipv6addr: destination array to fill IPv6 addresses
 * @ipv6addr_type: IPv6 Address type
 * @scope_array: scope of ipv6 addr
 * @count: number of IPv6 addresses
 *
 * This is the IPv6 utility function to populate unicast addresses.
 *
 * Return: 0 on success, error number otherwise.
 */
static int hdd_fill_ipv6_uc_addr(struct inet6_dev *idev,
				uint8_t ipv6_uc_addr[][QDF_IPV6_ADDR_SIZE],
				uint8_t *ipv6addr_type,
				enum pmo_ns_addr_scope *scope_array,
				uint32_t *count)
{
	struct inet6_ifaddr *ifa;
	struct list_head *p;
	uint32_t scope;

	read_lock_bh(&idev->lock);
	list_for_each(p, &idev->addr_list) {
		if (*count >= PMO_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA) {
			read_unlock_bh(&idev->lock);
			return -EINVAL;
		}
		ifa = list_entry(p, struct inet6_ifaddr, if_list);
		if (ifa->flags & IFA_F_DADFAILED)
			continue;
		scope = ipv6_addr_src_scope(&ifa->addr);
		switch (scope) {
		case IPV6_ADDR_SCOPE_GLOBAL:
		case IPV6_ADDR_SCOPE_LINKLOCAL:
			qdf_mem_copy(ipv6_uc_addr[*count], &ifa->addr.s6_addr,
				sizeof(ifa->addr.s6_addr));
			ipv6addr_type[*count] = PMO_IPV6_ADDR_UC_TYPE;
			scope_array[*count] = ucfg_pmo_ns_addr_scope(scope);
			hdd_debug("Index %d scope = %s UC-Address: %pI6",
				*count, (scope == IPV6_ADDR_SCOPE_LINKLOCAL) ?
				"LINK LOCAL" : "GLOBAL", ipv6_uc_addr[*count]);
			*count += 1;
			break;
		default:
			hdd_warn("The Scope %d is not supported", scope);
		}
	}

	read_unlock_bh(&idev->lock);
	return 0;
}

/**
 * hdd_fill_ipv6_ac_addr() - fill IPv6 anycast addresses
 * @idev: pointer to net device
 * @ipv6addr: destination array to fill IPv6 addresses
 * @ipv6addr_type: IPv6 Address type
 * @scope_array: scope of ipv6 addr
 * @count: number of IPv6 addresses
 *
 * This is the IPv6 utility function to populate anycast addresses.
 *
 * Return: 0 on success, error number otherwise.
 */
static int hdd_fill_ipv6_ac_addr(struct inet6_dev *idev,
				uint8_t ipv6_ac_addr[][QDF_IPV6_ADDR_SIZE],
				uint8_t *ipv6addr_type,
				enum pmo_ns_addr_scope *scope_array,
				uint32_t *count)
{
	struct ifacaddr6 *ifaca;
	uint32_t scope;

	read_lock_bh(&idev->lock);
	for (ifaca = idev->ac_list; ifaca; ifaca = ifaca->aca_next) {
		if (*count >= PMO_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA) {
			read_unlock_bh(&idev->lock);
			return -EINVAL;
		}
		/* For anycast addr no DAD */
		scope = ipv6_addr_src_scope(&ifaca->aca_addr);
		switch (scope) {
		case IPV6_ADDR_SCOPE_GLOBAL:
		case IPV6_ADDR_SCOPE_LINKLOCAL:
			qdf_mem_copy(ipv6_ac_addr[*count], &ifaca->aca_addr,
				sizeof(ifaca->aca_addr));
			ipv6addr_type[*count] = PMO_IPV6_ADDR_AC_TYPE;
			scope_array[*count] = ucfg_pmo_ns_addr_scope(scope);
			hdd_debug("Index %d scope = %s AC-Address: %pI6",
				*count, (scope == IPV6_ADDR_SCOPE_LINKLOCAL) ?
				"LINK LOCAL" : "GLOBAL", ipv6_ac_addr[*count]);
			*count += 1;
			break;
		default:
			hdd_warn("The Scope %d is not supported", scope);
		}
	}

	read_unlock_bh(&idev->lock);
	return 0;
}

void hdd_enable_ns_offload(struct hdd_adapter *adapter,
			   enum pmo_offload_trigger trigger)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct wlan_objmgr_psoc *psoc = hdd_ctx->psoc;
	struct wlan_objmgr_vdev *vdev;
	struct inet6_dev *in6_dev;
	struct pmo_ns_req *ns_req;
	QDF_STATUS status;
	int errno;

	hdd_enter();

	if (!psoc) {
		hdd_err("psoc is NULL");
		goto out;
	}

	in6_dev = __in6_dev_get(adapter->dev);
	if (!in6_dev) {
		hdd_err("IPv6 dev does not exist. Failed to request NSOffload");
		goto out;
	}

	ns_req = qdf_mem_malloc(sizeof(*ns_req));
	if (!ns_req)
		goto out;

	ns_req->psoc = psoc;
	ns_req->vdev_id = adapter->vdev_id;
	ns_req->trigger = trigger;
	ns_req->count = 0;

	/* check if offload cache and send is required or not */
	status = ucfg_pmo_ns_offload_check(psoc, trigger, adapter->vdev_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_debug("NS offload is not required");
		goto free_req;
	}

	/* Unicast Addresses */
	errno = hdd_fill_ipv6_uc_addr(in6_dev, ns_req->ipv6_addr,
				      ns_req->ipv6_addr_type, ns_req->scope,
				      &ns_req->count);
	if (errno) {
		hdd_disable_ns_offload(adapter, trigger);
		hdd_debug("Max supported addresses: disabling NS offload");
		goto free_req;
	}

	/* Anycast Addresses */
	errno = hdd_fill_ipv6_ac_addr(in6_dev, ns_req->ipv6_addr,
				      ns_req->ipv6_addr_type, ns_req->scope,
				      &ns_req->count);
	if (errno) {
		hdd_disable_ns_offload(adapter, trigger);
		hdd_debug("Max supported addresses: disabling NS offload");
		goto free_req;
	}

	/* cache ns request */
	status = ucfg_pmo_cache_ns_offload_req(ns_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to cache ns request; status:%d", status);
		goto free_req;
	}

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		goto free_req;
	}
	/* enable ns request */
	status = ucfg_pmo_enable_ns_offload_in_fwr(vdev, trigger);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to enable ns offload; status:%d", status);
		goto put_vdev;
	}

	hdd_wlan_offload_event(SIR_IPV6_NS_OFFLOAD, SIR_OFFLOAD_ENABLE);
put_vdev:
	hdd_objmgr_put_vdev(vdev);
free_req:
	qdf_mem_free(ns_req);

out:
	hdd_exit();
}

void hdd_disable_ns_offload(struct hdd_adapter *adapter,
		enum pmo_offload_trigger trigger)
{
	QDF_STATUS status;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	status = ucfg_pmo_ns_offload_check(hdd_ctx->psoc, trigger,
					   adapter->vdev_id);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_debug("Flushing of NS offload not required");
		goto out;
	}

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		goto out;
	}

	status = ucfg_pmo_flush_ns_offload_req(vdev);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Failed to flush NS Offload");
		goto put_vdev;
	}

	status = ucfg_pmo_disable_ns_offload_in_fwr(vdev, trigger);
	if (status != QDF_STATUS_SUCCESS)
		hdd_err("Failed to disable NS Offload");
	else
		hdd_wlan_offload_event(SIR_IPV6_NS_OFFLOAD,
			SIR_OFFLOAD_DISABLE);
put_vdev:
	hdd_objmgr_put_vdev(vdev);
out:
	hdd_exit();

}

/**
 * hdd_send_ps_config_to_fw() - Check user pwr save config set/reset PS
 * @adapter: pointer to hdd adapter
 *
 * This function checks the power save configuration saved in MAC context
 * and sends power save config to FW.
 *
 * Return: None
 */
static void hdd_send_ps_config_to_fw(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx;
	bool usr_ps_cfg;

	if (hdd_validate_adapter(adapter))
		return;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	usr_ps_cfg = ucfg_mlme_get_user_ps(hdd_ctx->psoc, adapter->vdev_id);
	if (usr_ps_cfg)
		sme_ps_enable_disable(hdd_ctx->mac_handle, adapter->vdev_id,
				      SME_PS_ENABLE);
	else
		sme_ps_enable_disable(hdd_ctx->mac_handle, adapter->vdev_id,
				      SME_PS_DISABLE);
}

/**
 * __hdd_ipv6_notifier_work_queue() - IPv6 notification work function
 * @adapter: adapter whose IP address changed
 *
 * This function performs the work initially trigged by a callback
 * from the IPv6 netdev notifier.  Since this means there has been a
 * change in IPv6 state for the interface, the NS offload is
 * reconfigured.
 *
 * Return: None
 */
static void __hdd_ipv6_notifier_work_queue(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx;
	int errno;

	hdd_enter();

	errno = hdd_validate_adapter(adapter);
	if (errno)
		goto exit;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		goto exit;

	hdd_enable_ns_offload(adapter, pmo_ipv6_change_notify);

	hdd_send_ps_config_to_fw(adapter);
exit:
	hdd_exit();
}

void hdd_ipv6_notifier_work_queue(struct work_struct *work)
{
	struct hdd_adapter *adapter = container_of(work, struct hdd_adapter,
						   ipv6_notifier_work);
	struct osif_vdev_sync *vdev_sync;

	if (osif_vdev_sync_op_start(adapter->dev, &vdev_sync))
		return;

	__hdd_ipv6_notifier_work_queue(adapter);

	osif_vdev_sync_op_stop(vdev_sync);
}
#endif /* WLAN_NS_OFFLOAD */

static void hdd_enable_hw_filter(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		return;
	}

	status = ucfg_pmo_enable_hw_filter_in_fwr(vdev);
	if (status != QDF_STATUS_SUCCESS)
		hdd_info("Failed to enable hardware filter");

	hdd_objmgr_put_vdev(vdev);
	hdd_exit();
}

static void hdd_disable_hw_filter(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		return;
	}

	status = ucfg_pmo_disable_hw_filter_in_fwr(vdev);
	if (status != QDF_STATUS_SUCCESS)
		hdd_info("Failed to disable hardware filter");

	hdd_objmgr_put_vdev(vdev);

	hdd_exit();
}

static void hdd_enable_action_frame_patterns(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;
	hdd_enter();

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		return;
	}

	status = ucfg_pmo_enable_action_frame_patterns(vdev,
						       QDF_SYSTEM_SUSPEND);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_info("Failed to enable action frame patterns");

	hdd_objmgr_put_vdev(vdev);

	hdd_exit();
}

static void hdd_disable_action_frame_patterns(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		return;
	}

	status = ucfg_pmo_disable_action_frame_patterns(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_info("Failed to disable action frame patterns");

	hdd_objmgr_put_vdev(vdev);

	hdd_exit();
}

void hdd_enable_host_offloads(struct hdd_adapter *adapter,
	enum pmo_offload_trigger trigger)
{
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		goto out;
	}

	if (!ucfg_pmo_is_vdev_supports_offload(vdev)) {
		hdd_debug("offload is not supported on vdev opmode %d",
			  adapter->device_mode);
		goto put_vdev;
	}

	if (!ucfg_pmo_is_vdev_connected(vdev)) {
		hdd_debug("offload is not supported on disconnected vdevs");
		goto put_vdev;
	}

	hdd_debug("enable offloads");
	hdd_enable_gtk_offload(adapter);
	hdd_enable_arp_offload(adapter, trigger);
	hdd_enable_ns_offload(adapter, trigger);
	hdd_enable_mc_addr_filtering(adapter, trigger);
	if (adapter->device_mode == QDF_STA_MODE)
		hdd_enable_igmp_offload(adapter);

	if (adapter->device_mode != QDF_NDI_MODE)
		hdd_enable_hw_filter(adapter);
	hdd_enable_action_frame_patterns(adapter);
put_vdev:
	hdd_objmgr_put_vdev(vdev);
out:
	hdd_exit();

}

void hdd_disable_host_offloads(struct hdd_adapter *adapter,
	enum pmo_offload_trigger trigger)
{
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		goto out;
	}

	if (!ucfg_pmo_is_vdev_supports_offload(vdev)) {
		hdd_info("offload is not supported on this vdev opmode: %d",
				adapter->device_mode);
			goto put_vdev;
	}

	if (!ucfg_pmo_is_vdev_connected(vdev)) {
		hdd_info("vdev is not connected");
		goto put_vdev;
	}

	hdd_debug("disable offloads");
	hdd_disable_gtk_offload(adapter);
	hdd_disable_arp_offload(adapter, trigger);
	hdd_disable_ns_offload(adapter, trigger);
	hdd_disable_mc_addr_filtering(adapter, trigger);
	if (adapter->device_mode == QDF_STA_MODE)
		hdd_disable_igmp_offload(adapter);
	if (adapter->device_mode != QDF_NDI_MODE)
		hdd_disable_hw_filter(adapter);
	hdd_disable_action_frame_patterns(adapter);

put_vdev:
	hdd_objmgr_put_vdev(vdev);
out:
	hdd_exit();

}

/**
 * hdd_lookup_ifaddr() - Lookup interface address data by name
 * @adapter: the adapter whose name should be searched for
 *
 * return in_ifaddr pointer on success, NULL for failure
 */
static struct in_ifaddr *hdd_lookup_ifaddr(struct hdd_adapter *adapter)
{
	struct in_ifaddr *ifa;
	struct in_device *in_dev;

	if (!adapter) {
		hdd_err("adapter is null");
		return NULL;
	}

	in_dev = __in_dev_get_rtnl(adapter->dev);
	if (!in_dev) {
		hdd_err("Failed to get in_device");
		return NULL;
	}

	/* lookup address data by interface name */
	for (ifa = in_dev->ifa_list; ifa; ifa = ifa->ifa_next) {
		if (!strcmp(adapter->dev->name, ifa->ifa_label))
			return ifa;
	}

	return NULL;
}

/**
 * hdd_populate_ipv4_addr() - Populates the adapter's IPv4 address
 * @adapter: the adapter whose IPv4 address is desired
 * @ipv4_addr: the address of the array to copy the IPv4 address into
 *
 * return: zero for success; non-zero for failure
 */
static int hdd_populate_ipv4_addr(struct hdd_adapter *adapter,
				  uint8_t *ipv4_addr)
{
	struct in_ifaddr *ifa;
	int i;

	if (!adapter) {
		hdd_err("adapter is null");
		return -EINVAL;
	}

	if (!ipv4_addr) {
		hdd_err("ipv4_addr is null");
		return -EINVAL;
	}

	ifa = hdd_lookup_ifaddr(adapter);
	if (!ifa || !ifa->ifa_local) {
		hdd_err("ipv4 address not found");
		return -EINVAL;
	}

	/* convert u32 to byte array */
	for (i = 0; i < 4; i++)
		ipv4_addr[i] = (ifa->ifa_local >> i * 8) & 0xff;

	return 0;
}

/**
 * hdd_set_grat_arp_keepalive() - Enable grat APR keepalive
 * @adapter: the HDD adapter to configure
 *
 * This configures gratuitous APR keepalive based on the adapter's current
 * connection information, specifically IPv4 address and BSSID
 *
 * return: zero for success, non-zero for failure
 */
static int hdd_set_grat_arp_keepalive(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	int exit_code;
	struct hdd_context *hdd_ctx;
	struct hdd_station_ctx *sta_ctx;
	struct keep_alive_req req = {
		.packetType = SIR_KEEP_ALIVE_UNSOLICIT_ARP_RSP,
		.dest_macaddr = QDF_MAC_ADDR_BCAST_INIT,
	};

	if (!adapter) {
		hdd_err("adapter is null");
		return -EINVAL;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return -EINVAL;
	}

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (!sta_ctx) {
		hdd_err("sta_ctx is null");
		return -EINVAL;
	}

	exit_code = hdd_populate_ipv4_addr(adapter, req.hostIpv4Addr);
	if (exit_code) {
		hdd_err("Failed to populate ipv4 address");
		return exit_code;
	}

	/* according to RFC5227, sender/target ip address should be the same */
	qdf_mem_copy(&req.destIpv4Addr, &req.hostIpv4Addr,
		     sizeof(req.destIpv4Addr));

	qdf_copy_macaddr(&req.bssid, &sta_ctx->conn_info.bssid);
	ucfg_mlme_get_sta_keep_alive_period(hdd_ctx->psoc, &req.timePeriod);
	req.sessionId = adapter->vdev_id;

	hdd_debug("Setting gratuitous ARP keepalive; ipv4_addr:%u.%u.%u.%u",
		 req.hostIpv4Addr[0], req.hostIpv4Addr[1],
		 req.hostIpv4Addr[2], req.hostIpv4Addr[3]);

	status = sme_set_keep_alive(hdd_ctx->mac_handle, req.sessionId, &req);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to set keepalive");
		return qdf_status_to_os_return(status);
	}

	return 0;
}

/**
 * __hdd_ipv4_notifier_work_queue() - IPv4 notification work function
 * @adapter: adapter whose IP address changed
 *
 * This function performs the work initially trigged by a callback
 * from the IPv4 netdev notifier.  Since this means there has been a
 * change in IPv4 state for the interface, the ARP offload is
 * reconfigured. Also, Updates the HLP IE info with IP address info
 * to fw if LFR3 is enabled
 *
 * Return: None
 */
static void __hdd_ipv4_notifier_work_queue(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx;
	int errno;
	struct csr_roam_profile *roam_profile;
	struct in_ifaddr *ifa;
	enum station_keepalive_method val;
	QDF_STATUS status;

	hdd_enter();

	errno = hdd_validate_adapter(adapter);
	if (errno)
		goto exit;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		goto exit;

	hdd_enable_arp_offload(adapter, pmo_ipv4_change_notify);

	status = ucfg_mlme_get_sta_keepalive_method(hdd_ctx->psoc, &val);
	if (QDF_IS_STATUS_ERROR(status))
		goto exit;

	if (val == MLME_STA_KEEPALIVE_GRAT_ARP)
		hdd_set_grat_arp_keepalive(adapter);

	hdd_debug("FILS Roaming support: %d",
		  hdd_ctx->is_fils_roaming_supported);
	roam_profile = hdd_roam_profile(adapter);

	ifa = hdd_lookup_ifaddr(adapter);
	if (ifa && hdd_ctx->is_fils_roaming_supported)
		sme_send_hlp_ie_info(hdd_ctx->mac_handle, adapter->vdev_id,
				     roam_profile, ifa->ifa_local);
	hdd_send_ps_config_to_fw(adapter);
exit:
	hdd_exit();
}

void hdd_ipv4_notifier_work_queue(struct work_struct *work)
{
	struct hdd_adapter *adapter = container_of(work, struct hdd_adapter,
						   ipv4_notifier_work);
	struct osif_vdev_sync *vdev_sync;

	if (osif_vdev_sync_op_start(adapter->dev, &vdev_sync))
		return;

	__hdd_ipv4_notifier_work_queue(adapter);

	osif_vdev_sync_op_stop(vdev_sync);
}

/**
 * __wlan_hdd_ipv4_changed() - IPv4 notifier callback function
 * @net_dev: the net_device whose IP address changed
 *
 * This is a callback function that is registered with the kernel via
 * register_inetaddr_notifier() which allows the driver to be
 * notified when there is an IPv4 address change.
 *
 * Return: None
 */
static void __wlan_hdd_ipv4_changed(struct net_device *net_dev)
{
	struct in_ifaddr *ifa;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(net_dev);
	struct hdd_context *hdd_ctx;
	int errno;

	hdd_enter_dev(net_dev);

	errno = hdd_validate_adapter(adapter);
	if (errno || adapter->dev != net_dev)
		goto exit;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		goto exit;

	if (adapter->device_mode == QDF_STA_MODE ||
	    adapter->device_mode == QDF_P2P_CLIENT_MODE) {
		hdd_debug("invoking sme_dhcp_done_ind");
		sme_dhcp_done_ind(hdd_ctx->mac_handle, adapter->vdev_id);

		if (!ucfg_pmo_is_arp_offload_enabled(hdd_ctx->psoc)) {
			hdd_debug("Offload not enabled");
			goto exit;
		}

		ifa = hdd_lookup_ifaddr(adapter);
		if (ifa && ifa->ifa_local)
			schedule_work(&adapter->ipv4_notifier_work);
	}

exit:
	hdd_exit();
}

int wlan_hdd_ipv4_changed(struct notifier_block *nb,
			  unsigned long data, void *context)
{
	struct in_ifaddr *ifa = context;
	struct net_device *net_dev = ifa->ifa_dev->dev;
	struct osif_vdev_sync *vdev_sync;

	if (osif_vdev_sync_op_start(net_dev, &vdev_sync))
		return NOTIFY_DONE;

	__wlan_hdd_ipv4_changed(net_dev);

	osif_vdev_sync_op_stop(vdev_sync);

	return NOTIFY_DONE;
}

#ifdef FEATURE_RUNTIME_PM
int wlan_hdd_pm_qos_notify(struct notifier_block *nb, unsigned long curr_val,
			   void *context)
{
	struct hdd_context *hdd_ctx = container_of(nb, struct hdd_context,
						   pm_qos_notifier);
	void *hif_ctx;
	bool is_any_sta_connected = false;

	if (hdd_ctx->driver_status != DRIVER_MODULES_ENABLED) {
		hdd_debug_rl("Driver Module closed; skipping pm qos notify");
		return 0;
	}

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_ctx) {
		hdd_err("Hif context is Null");
		return -EINVAL;
	}

	is_any_sta_connected = hdd_is_any_sta_connected(hdd_ctx);

	hdd_debug("PM QOS update: runtime_pm_prevented %d Current value: %ld, is_any_sta_connected %d",
		  hdd_ctx->runtime_pm_prevented, curr_val,
		  is_any_sta_connected);
	qdf_spin_lock_irqsave(&hdd_ctx->pm_qos_lock);

	if (!hdd_ctx->runtime_pm_prevented &&
	    is_any_sta_connected &&
	    curr_val != wlan_hdd_get_pm_qos_cpu_latency()) {
		hif_pm_runtime_get_noresume(hif_ctx, RTPM_ID_QOS_NOTIFY);
		hdd_ctx->runtime_pm_prevented = true;
	} else if (hdd_ctx->runtime_pm_prevented &&
		   curr_val == wlan_hdd_get_pm_qos_cpu_latency()) {
		hif_pm_runtime_put(hif_ctx, RTPM_ID_QOS_NOTIFY);
		hdd_ctx->runtime_pm_prevented = false;
	}

	qdf_spin_unlock_irqrestore(&hdd_ctx->pm_qos_lock);

	return NOTIFY_DONE;
}
#endif

/**
 * hdd_get_ipv4_local_interface() - get ipv4 local interafce from iface list
 * @adapter: Adapter context for which ARP offload is to be configured
 *
 * Return:
 *	ifa - on successful operation,
 *	NULL - on failure of operation
 */
static struct in_ifaddr *hdd_get_ipv4_local_interface(
				struct hdd_adapter *adapter)
{
	struct in_ifaddr **ifap = NULL;
	struct in_ifaddr *ifa = NULL;
	struct in_device *in_dev;

	in_dev = __in_dev_get_rtnl(adapter->dev);
	if (in_dev) {
		for (ifap = &in_dev->ifa_list; (ifa = *ifap) != NULL;
			     ifap = &ifa->ifa_next) {
			if (!strcmp(adapter->dev->name, ifa->ifa_label)) {
				/* if match break */
				return ifa;
			}
		}
	}
	ifa = NULL;

	return ifa;
}

void hdd_enable_arp_offload(struct hdd_adapter *adapter,
			    enum pmo_offload_trigger trigger)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct wlan_objmgr_psoc *psoc = hdd_ctx->psoc;
	QDF_STATUS status;
	struct pmo_arp_req *arp_req;
	struct in_ifaddr *ifa;
	struct wlan_objmgr_vdev *vdev;

	arp_req = qdf_mem_malloc(sizeof(*arp_req));
	if (!arp_req)
		return;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		goto free_req;
	}

	arp_req->psoc = psoc;
	arp_req->vdev_id = adapter->vdev_id;
	arp_req->trigger = trigger;

	status = ucfg_pmo_check_arp_offload(psoc, trigger, adapter->vdev_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_debug("ARP offload not required");
		goto put_vdev;
	}

	ifa = hdd_get_ipv4_local_interface(adapter);
	if (!ifa || !ifa->ifa_local) {
		hdd_info("IP Address is not assigned");
		status = QDF_STATUS_NOT_INITIALIZED;
		goto put_vdev;
	}

	arp_req->ipv4_addr = (uint32_t)ifa->ifa_local;

	status = ucfg_pmo_cache_arp_offload_req(arp_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("failed to cache arp offload req; status:%d", status);
		goto put_vdev;
	}

	status = ucfg_pmo_enable_arp_offload_in_fwr(vdev, trigger);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("failed arp offload config in fw; status:%d", status);
		goto put_vdev;
	}

	hdd_wlan_offload_event(PMO_IPV4_ARP_REPLY_OFFLOAD, PMO_OFFLOAD_ENABLE);

put_vdev:
	hdd_objmgr_put_vdev(vdev);
free_req:
	qdf_mem_free(arp_req);
}

void hdd_disable_arp_offload(struct hdd_adapter *adapter,
		enum pmo_offload_trigger trigger)
{
	QDF_STATUS status;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct wlan_objmgr_vdev *vdev;

	status = ucfg_pmo_check_arp_offload(hdd_ctx->psoc, trigger,
					    adapter->vdev_id);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_debug("Flushing of ARP offload not required");
		return;
	}

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		return;
	}

	status = ucfg_pmo_flush_arp_offload_req(vdev);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_objmgr_put_vdev(vdev);
		hdd_err("Failed to flush arp Offload");
		return;
	}

	status = ucfg_pmo_disable_arp_offload_in_fwr(vdev, trigger);
	if (status == QDF_STATUS_SUCCESS)
		hdd_wlan_offload_event(PMO_IPV4_ARP_REPLY_OFFLOAD,
			PMO_OFFLOAD_DISABLE);
	else
		hdd_info("fail to disable arp offload");
	hdd_objmgr_put_vdev(vdev);
}

void hdd_enable_mc_addr_filtering(struct hdd_adapter *adapter,
				  enum pmo_offload_trigger trigger)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	QDF_STATUS status;

	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	if (!hdd_adapter_is_connected_sta(adapter))
		return;

	status = ucfg_pmo_enable_mc_addr_filtering_in_fwr(hdd_ctx->psoc,
							  adapter->vdev_id,
							  trigger);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_debug("failed to enable mc list; status:%d", status);

}

void hdd_disable_mc_addr_filtering(struct hdd_adapter *adapter,
				   enum pmo_offload_trigger trigger)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	QDF_STATUS status;

	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	if (!hdd_adapter_is_connected_sta(adapter))
		return;

	status = ucfg_pmo_disable_mc_addr_filtering_in_fwr(hdd_ctx->psoc,
							   adapter->vdev_id,
							   trigger);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("failed to disable mc list; status:%d", status);
}

int hdd_cache_mc_addr_list(struct pmo_mc_addr_list_params *mc_list_config)
{
	QDF_STATUS status;

	status = ucfg_pmo_cache_mc_addr_list(mc_list_config);

	return qdf_status_to_os_return(status);
}

void hdd_disable_and_flush_mc_addr_list(struct hdd_adapter *adapter,
					enum pmo_offload_trigger trigger)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	QDF_STATUS status;

	if (!hdd_adapter_is_connected_sta(adapter))
		goto flush_mc_list;

	/* disable mc list first because the mc list is cached in PMO */
	status = ucfg_pmo_disable_mc_addr_filtering_in_fwr(hdd_ctx->psoc,
							   adapter->vdev_id,
							   trigger);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_debug("failed to disable mc list; status:%d", status);

flush_mc_list:
	status = ucfg_pmo_flush_mc_addr_list(hdd_ctx->psoc,
					     adapter->vdev_id);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_debug("failed to flush mc list; status:%d", status);

}

/**
 * hdd_update_conn_state_mask(): record info needed by wma_suspend_req
 * @adapter: adapter to get info from
 * @conn_state_mask: mask of connection info
 *
 * currently only need to send connection info.
 */
static void hdd_update_conn_state_mask(struct hdd_adapter *adapter,
				       uint32_t *conn_state_mask)
{

	eConnectionState conn_state;
	struct hdd_station_ctx *sta_ctx;

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	conn_state = sta_ctx->conn_info.conn_state;

	if (conn_state == eConnectionState_Associated)
		*conn_state_mask |= (1 << adapter->vdev_id);
}

/**
 * hdd_suspend_wlan() - Driver suspend function
 * @callback: Callback function to invoke when driver is ready to suspend
 * @callbackContext: Context to pass back to @callback function
 *
 * Return: 0 on success else error code.
 */
static int
hdd_suspend_wlan(void)
{
	struct hdd_context *hdd_ctx;
	QDF_STATUS status;
	struct hdd_adapter *adapter = NULL, *next_adapter = NULL;
	uint32_t conn_state_mask = 0;

	hdd_info("WLAN being suspended by OS");

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("HDD context is Null");
		return -EINVAL;
	}

	if (cds_is_driver_recovering() || cds_is_driver_in_bad_state()) {
		hdd_info("Recovery in Progress. State: 0x%x Ignore suspend!!!",
			 cds_get_driver_state());
		return -EINVAL;
	}

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   NET_DEV_HOLD_SUSPEND_WLAN) {
		if (wlan_hdd_validate_vdev_id(adapter->vdev_id)) {
			hdd_adapter_dev_put_debug(adapter,
						  NET_DEV_HOLD_SUSPEND_WLAN);
			continue;
		}

		/* stop all TX queues before suspend */
		hdd_debug("Disabling queues for dev mode %s",
			  qdf_opmode_str(adapter->device_mode));
		wlan_hdd_netif_queue_control(adapter,
					     WLAN_STOP_ALL_NETIF_QUEUE,
					     WLAN_CONTROL_PATH);

		if (adapter->device_mode == QDF_STA_MODE)
			status = hdd_enable_default_pkt_filters(adapter);

		/* Configure supported OffLoads */
		hdd_enable_host_offloads(adapter, pmo_apps_suspend);
		hdd_update_conn_state_mask(adapter, &conn_state_mask);
		hdd_adapter_dev_put_debug(adapter, NET_DEV_HOLD_SUSPEND_WLAN);
	}

	status = ucfg_pmo_psoc_user_space_suspend_req(hdd_ctx->psoc,
						      QDF_SYSTEM_SUSPEND);
	if (status != QDF_STATUS_SUCCESS)
		return -EAGAIN;

	hdd_ctx->hdd_wlan_suspended = true;

	hdd_configure_sar_sleep_index(hdd_ctx);

	hdd_wlan_suspend_resume_event(HDD_WLAN_EARLY_SUSPEND);

	return 0;
}

/**
 * hdd_resume_wlan() - Driver resume function
 *
 * Return: 0 on success else error code.
 */
static int hdd_resume_wlan(void)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter, *next_adapter = NULL;
	QDF_STATUS status;

	hdd_info("WLAN being resumed by OS");

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("HDD context is Null");
		return -EINVAL;
	}

	if (cds_is_driver_recovering() || cds_is_driver_in_bad_state()) {
		hdd_info("Recovery in Progress. State: 0x%x Ignore resume!!!",
			 cds_get_driver_state());
		return -EINVAL;
	}

	hdd_ctx->hdd_wlan_suspended = false;
	hdd_wlan_suspend_resume_event(HDD_WLAN_EARLY_RESUME);

	/*loop through all adapters. Concurrency */
	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   NET_DEV_HOLD_RESUME_WLAN) {
		if (wlan_hdd_validate_vdev_id(adapter->vdev_id)) {
			hdd_adapter_dev_put_debug(adapter,
						  NET_DEV_HOLD_RESUME_WLAN);
			continue;
		}

		/* Disable supported OffLoads */
		hdd_disable_host_offloads(adapter, pmo_apps_resume);

		/* wake the tx queues */
		hdd_debug("Enabling queues for dev mode %s",
			  qdf_opmode_str(adapter->device_mode));
		wlan_hdd_netif_queue_control(adapter,
					WLAN_WAKE_ALL_NETIF_QUEUE,
					WLAN_CONTROL_PATH);

		if (adapter->device_mode == QDF_STA_MODE)
			status = hdd_disable_default_pkt_filters(adapter);

		hdd_adapter_dev_put_debug(adapter, NET_DEV_HOLD_RESUME_WLAN);
	}

	ucfg_ipa_resume(hdd_ctx->pdev);
	status = ucfg_pmo_psoc_user_space_resume_req(hdd_ctx->psoc,
						     QDF_SYSTEM_SUSPEND);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	hdd_configure_sar_resume_index(hdd_ctx);

	return 0;
}

void hdd_svc_fw_shutdown_ind(struct device *dev)
{
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	hdd_ctx ? wlan_hdd_send_svc_nlink_msg(hdd_ctx->radio_index,
					      WLAN_SVC_FW_SHUTDOWN_IND,
					      NULL, 0) : 0;
}

/**
 * hdd_ssr_restart_sap() - restart sap on SSR
 * @hdd_ctx:   hdd context
 *
 * Return:     nothing
 */
static void hdd_ssr_restart_sap(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;

	hdd_enter();

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   NET_DEV_HOLD_SSR_RESTART_SAP) {
		if (adapter->device_mode == QDF_SAP_MODE) {
			if (test_bit(SOFTAP_INIT_DONE, &adapter->event_flags)) {
				hdd_debug("Restart prev SAP session");
				wlan_hdd_start_sap(adapter, true);
			}
		}
		hdd_adapter_dev_put_debug(adapter,
					  NET_DEV_HOLD_SSR_RESTART_SAP);
	}

	hdd_exit();
}

QDF_STATUS hdd_wlan_shutdown(void)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;
	struct wlan_objmgr_vdev *vdev;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	hdd_info("WLAN driver shutting down!");

	/* Get the HDD context. */
	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("HDD context is Null");
		return QDF_STATUS_E_FAILURE;
	}

	hdd_set_connection_in_progress(false);
	policy_mgr_clear_concurrent_session_count(hdd_ctx->psoc);

	hdd_debug("Invoking packetdump deregistration API");
	wlan_deregister_txrx_packetdump(OL_TXRX_PDEV_ID);

	/* resume wlan threads before adapter reset which does vdev destroy */
	if (hdd_ctx->is_scheduler_suspended) {
		scheduler_resume();
		hdd_ctx->is_scheduler_suspended = false;
		hdd_ctx->is_wiphy_suspended = false;
		hdd_ctx->hdd_wlan_suspended = false;
		ucfg_pmo_resume_all_components(hdd_ctx->psoc,
					       QDF_SYSTEM_SUSPEND);
	}

	wlan_hdd_rx_thread_resume(hdd_ctx);

	dp_txrx_resume(cds_get_context(QDF_MODULE_ID_SOC));

	if (ucfg_pkt_capture_get_mode(hdd_ctx->psoc) !=
						PACKET_CAPTURE_MODE_DISABLE) {
		adapter = hdd_get_adapter(hdd_ctx, QDF_MONITOR_MODE);
		if (adapter) {
			vdev = hdd_objmgr_get_vdev(adapter);
			if (vdev) {
				ucfg_pkt_capture_resume_mon_thread(vdev);
				hdd_objmgr_put_vdev(vdev);
			} else {
				hdd_err("vdev is NULL");
			}
		}
	}

	/*
	 * After SSR, FW clear its txrx stats. In host,
	 * as adapter is intact so those counts are still
	 * available. Now if agains Set stats command comes,
	 * then host will increment its counts start from its
	 * last saved value, i.e., count before SSR, and FW will
	 * increment its count from 0. This will finally sends a
	 * mismatch of packet counts b/w host and FW to framework
	 * that will create ambiquity. Therfore, Resetting the host
	 * counts here so that after SSR both FW and host start
	 * increment their counts from 0.
	 */
	hdd_reset_all_adapters_connectivity_stats(hdd_ctx);

	hdd_reset_all_adapters(hdd_ctx);

	ucfg_ipa_uc_ssr_cleanup(hdd_ctx->pdev);

	/* Flush cached rx frame queue */
	if (soc)
		cdp_flush_cache_rx_queue(soc);

	/* De-register the HDD callbacks */
	hdd_deregister_cb(hdd_ctx);

	hdd_wlan_stop_modules(hdd_ctx, false);

	hdd_lpass_notify_stop(hdd_ctx);

	qdf_set_smmu_fault_state(false);
	hdd_info("WLAN driver shutdown complete");

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT
/**
 * hdd_wlan_ssr_reinit_event()- send ssr reinit state
 *
 * This Function send send ssr reinit state diag event
 *
 * Return: void.
 */
static void hdd_wlan_ssr_reinit_event(void)
{
	WLAN_HOST_DIAG_EVENT_DEF(ssr_reinit, struct host_event_wlan_ssr_reinit);
	qdf_mem_zero(&ssr_reinit, sizeof(ssr_reinit));
	ssr_reinit.status = SSR_SUB_SYSTEM_REINIT;
	WLAN_HOST_DIAG_EVENT_REPORT(&ssr_reinit,
					EVENT_WLAN_SSR_REINIT_SUBSYSTEM);
}
#else
static inline void hdd_wlan_ssr_reinit_event(void)
{

}
#endif

/**
 * hdd_send_default_scan_ies - send default scan ies to fw
 *
 * This function is used to send default scan ies to fw
 * in case of wlan re-init
 *
 * Return: void
 */
static void hdd_send_default_scan_ies(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   NET_DEV_HOLD_SEND_DEFAULT_SCAN_IES) {
		if (hdd_is_interface_up(adapter) &&
		    (adapter->device_mode == QDF_STA_MODE ||
		    adapter->device_mode == QDF_P2P_DEVICE_MODE) &&
		    adapter->scan_info.default_scan_ies) {
			sme_set_default_scan_ie(hdd_ctx->mac_handle,
				      adapter->vdev_id,
				      adapter->scan_info.default_scan_ies,
				      adapter->scan_info.default_scan_ies_len);
		}
		hdd_adapter_dev_put_debug(adapter,
					  NET_DEV_HOLD_SEND_DEFAULT_SCAN_IES);
	}
}

/**
 * hdd_restore_sar_config - Restore the saved SAR config after SSR
 * @hdd_ctx: HDD context
 *
 * Restore the SAR config that was lost during SSR.
 *
 * Return: None
 */
static void hdd_restore_sar_config(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;

	if (!hdd_ctx->sar_cmd_params)
		return;

	status = sme_set_sar_power_limits(hdd_ctx->mac_handle,
					  hdd_ctx->sar_cmd_params);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Unable to configured SAR after SSR");
}

void hdd_handle_cached_commands(void)
{
	struct net_device *net_dev;
	struct hdd_adapter *adapter = NULL;
	struct hdd_context *hdd_ctx;
	struct osif_vdev_sync *vdev_sync_arr = osif_get_vdev_sync_arr();
	struct osif_vdev_sync *vdev_sync;
	int i;
	uint8_t cmd_id;

	/* Get the HDD context */
	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	for (i = 0; i < WLAN_MAX_VDEVS; i++) {
		vdev_sync = vdev_sync_arr + i;
		if (!vdev_sync || !vdev_sync->in_use)
			continue;

		cmd_id = osif_vdev_get_cached_cmd(vdev_sync);
		net_dev = vdev_sync->net_dev;
		if (net_dev) {
			adapter = WLAN_HDD_GET_PRIV_PTR(
					(struct net_device *)net_dev);
			if (!adapter)
				continue;
		} else {
			continue;
		}

		switch (cmd_id) {
		case NO_COMMAND:
			break;
		case INTERFACE_DOWN:
			hdd_debug("Handling cached interface down command for %s",
				  adapter->dev->name);

			if (adapter->device_mode == QDF_SAP_MODE ||
			    adapter->device_mode == QDF_P2P_GO_MODE)
				hdd_hostapd_stop_no_trans(net_dev);
			else
				hdd_stop_no_trans(net_dev);

			osif_vdev_cache_command(vdev_sync, NO_COMMAND);
			break;
		default:
			break;
		}
	}
}

QDF_STATUS hdd_wlan_re_init(void)
{
	struct hdd_context *hdd_ctx = NULL;
	struct hdd_adapter *adapter;
	int ret;
	bool bug_on_reinit_failure = CFG_BUG_ON_REINIT_FAILURE_DEFAULT;
	bool value;

	hdd_prevent_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_REINIT);

	/* Get the HDD context */
	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("HDD context is Null");
		goto err_ctx_null;
	}
	bug_on_reinit_failure = hdd_ctx->config->bug_on_reinit_failure;

	adapter = hdd_get_first_valid_adapter(hdd_ctx);
	if (!adapter)
		hdd_err("Failed to get adapter");

	hdd_dp_trace_init(hdd_ctx->config);

	ret = hdd_wlan_start_modules(hdd_ctx, true);
	if (ret) {
		hdd_err("Failed to start wlan after error");
		goto err_re_init;
	}

	hdd_update_hw_sw_info(hdd_ctx);

	wlan_hdd_send_svc_nlink_msg(hdd_ctx->radio_index,
				WLAN_SVC_FW_CRASHED_IND, NULL, 0);

	/* Restart all adapters */
	hdd_start_all_adapters(hdd_ctx);

	hdd_init_scan_reject_params(hdd_ctx);

	complete(&adapter->roaming_comp_var);
	hdd_ctx->bt_coex_mode_set = false;

	/* Allow the phone to go to sleep */
	hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_REINIT);
	/* set chip power save failure detected callback */
	sme_set_chip_pwr_save_fail_cb(hdd_ctx->mac_handle,
				      hdd_chip_pwr_save_fail_detected_cb);

	hdd_restore_thermal_mitigation_config(hdd_ctx);
	hdd_restore_sar_config(hdd_ctx);

	hdd_send_default_scan_ies(hdd_ctx);
	hdd_info("WLAN host driver reinitiation completed!");

	ucfg_mlme_get_sap_internal_restart(hdd_ctx->psoc, &value);
	if (value)
		hdd_ssr_restart_sap(hdd_ctx);
	hdd_wlan_ssr_reinit_event();

	if (hdd_ctx->is_wiphy_suspended)
		hdd_ctx->is_wiphy_suspended = false;

	return QDF_STATUS_SUCCESS;

err_re_init:
	qdf_dp_trace_deinit();

err_ctx_null:
	/* Allow the phone to go to sleep */
	hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_REINIT);
	if (bug_on_reinit_failure)
		QDF_BUG(0);
	return -EPERM;
}

int wlan_hdd_set_powersave(struct hdd_adapter *adapter,
	bool allow_power_save, uint32_t timeout)
{
	mac_handle_t mac_handle;
	struct hdd_context *hdd_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool is_bmps_enabled;
	struct hdd_station_ctx *hdd_sta_ctx = NULL;

	if (!adapter) {
		hdd_err("Adapter NULL");
		return -ENODEV;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("hdd context is NULL");
		return -EINVAL;
	}

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (!hdd_sta_ctx) {
		hdd_err("hdd_sta_context is NULL");
		return -EINVAL;
	}

	hdd_debug("Allow power save: %d", allow_power_save);
	mac_handle = hdd_ctx->mac_handle;

	/*
	 * This is a workaround for defective AP's that send a disassoc
	 * immediately after WPS connection completes. Defer powersave by a
	 * small amount if the affected AP is detected.
	 */
	if (allow_power_save &&
	    adapter->device_mode == QDF_STA_MODE &&
	    !adapter->session.station.ap_supports_immediate_power_save) {
		timeout = AUTO_PS_DEFER_TIMEOUT_MS;
		hdd_debug("Defer power-save due to AP spec non-conformance");
	}

	if (allow_power_save) {
		if (QDF_STA_MODE == adapter->device_mode ||
		    QDF_P2P_CLIENT_MODE == adapter->device_mode) {
			hdd_debug("Disabling Auto Power save timer");
			status = sme_ps_disable_auto_ps_timer(mac_handle,
						adapter->vdev_id);
			if (status != QDF_STATUS_SUCCESS)
				goto end;
		}

		ucfg_mlme_set_user_ps(hdd_ctx->psoc, adapter->vdev_id, true);
		ucfg_mlme_is_bmps_enabled(hdd_ctx->psoc, &is_bmps_enabled);
		if (is_bmps_enabled) {
			hdd_debug("Wlan driver Entering Power save");

			/*
			 * Enter Power Save command received from GUI
			 * this means DHCP is completed
			 */
			if (timeout) {
				status = sme_ps_enable_auto_ps_timer(mac_handle,
							    adapter->vdev_id,
							    timeout);
				if (status != QDF_STATUS_SUCCESS)
					goto end;
			} else {
				status = sme_ps_enable_disable(mac_handle,
						adapter->vdev_id,
						SME_PS_ENABLE);
				if (status != QDF_STATUS_SUCCESS)
					goto end;
			}
		} else {
			hdd_debug("Power Save is not enabled in the cfg");
		}
	} else {
		hdd_debug("Wlan driver Entering Full Power");

		ucfg_mlme_set_user_ps(hdd_ctx->psoc, adapter->vdev_id, false);
		/*
		 * Enter Full power command received from GUI
		 * this means we are disconnected
		 */
		status = sme_ps_disable_auto_ps_timer(mac_handle,
					adapter->vdev_id);

		if (status != QDF_STATUS_SUCCESS)
			goto end;

		ucfg_mlme_is_bmps_enabled(hdd_ctx->psoc, &is_bmps_enabled);
		if (is_bmps_enabled) {
			status = sme_ps_enable_disable(mac_handle,
						       adapter->vdev_id,
						       SME_PS_DISABLE);
			if (status != QDF_STATUS_SUCCESS)
				goto end;
		}

		if (adapter->device_mode == QDF_STA_MODE) {
			hdd_twt_del_dialog_in_ps_disable(hdd_ctx,
						&hdd_sta_ctx->conn_info.bssid,
						adapter->vdev_id);
		}
	}

end:
	return qdf_status_to_os_return(status);
}

static void wlan_hdd_print_suspend_fail_stats(struct hdd_context *hdd_ctx)
{
	struct suspend_resume_stats *stats = &hdd_ctx->suspend_resume_stats;

	hdd_err("ipa:%d, radar:%d, roam:%d, scan:%d, initial_wakeup:%d",
		stats->suspend_fail[SUSPEND_FAIL_IPA],
		stats->suspend_fail[SUSPEND_FAIL_RADAR],
		stats->suspend_fail[SUSPEND_FAIL_ROAM],
		stats->suspend_fail[SUSPEND_FAIL_SCAN],
		stats->suspend_fail[SUSPEND_FAIL_INITIAL_WAKEUP]);
}

void wlan_hdd_inc_suspend_stats(struct hdd_context *hdd_ctx,
				enum suspend_fail_reason reason)
{
	wlan_hdd_print_suspend_fail_stats(hdd_ctx);
	hdd_ctx->suspend_resume_stats.suspend_fail[reason]++;
	wlan_hdd_print_suspend_fail_stats(hdd_ctx);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)
static inline void
hdd_sched_scan_results(struct wiphy *wiphy, uint64_t reqid)
{
	cfg80211_sched_scan_results(wiphy);
}
#else
static inline void
hdd_sched_scan_results(struct wiphy *wiphy, uint64_t reqid)
{
	cfg80211_sched_scan_results(wiphy, reqid);
}
#endif

/**
 * __wlan_hdd_cfg80211_resume_wlan() - cfg80211 resume callback
 * @wiphy: Pointer to wiphy
 *
 * This API is called when cfg80211 driver resumes driver updates
 * latest sched_scan scan result(if any) to cfg80211 database
 *
 * Return: integer status
 */
static int __wlan_hdd_cfg80211_resume_wlan(struct wiphy *wiphy)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct hdd_adapter *adapter;
	struct wlan_objmgr_vdev *vdev;
	int exit_code;

	hdd_enter();

	if (cds_is_driver_recovering()) {
		hdd_debug("Driver is recovering; Skipping resume");
		exit_code = 0;
		goto exit_with_code;
	}

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam() ||
	    QDF_GLOBAL_MONITOR_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in mode %d",
			hdd_get_conparam());
		exit_code = -EINVAL;
		goto exit_with_code;
	}

	if (hdd_ctx->config->is_wow_disabled) {
		hdd_err("wow is disabled");
		return -EINVAL;
	}

	if (hdd_ctx->driver_status != DRIVER_MODULES_ENABLED) {
		hdd_debug("Driver is not enabled; Skipping resume");
		exit_code = 0;
		goto exit_with_code;
	}

	status = hdd_resume_wlan();
	if (status != QDF_STATUS_SUCCESS) {
		exit_code = 0;
		goto exit_with_code;
	}
	/* Resume control path scheduler */
	if (hdd_ctx->is_scheduler_suspended) {
		scheduler_resume();
		hdd_ctx->is_scheduler_suspended = false;
	}
	/* Resume all components registered to pmo */
	status = ucfg_pmo_resume_all_components(hdd_ctx->psoc,
						QDF_SYSTEM_SUSPEND);
	if (status != QDF_STATUS_SUCCESS) {
		exit_code = 0;
		goto exit_with_code;
	}
	/* Resume tlshim Rx thread */
	if (hdd_ctx->enable_rxthread)
		wlan_hdd_rx_thread_resume(hdd_ctx);

	if (hdd_ctx->enable_dp_rx_threads)
		dp_txrx_resume(cds_get_context(QDF_MODULE_ID_SOC));

	if (ucfg_pkt_capture_get_mode(hdd_ctx->psoc) !=
						PACKET_CAPTURE_MODE_DISABLE) {
		adapter = hdd_get_adapter(hdd_ctx, QDF_MONITOR_MODE);
		if (adapter) {
			vdev = hdd_objmgr_get_vdev(adapter);
			if (vdev) {
				ucfg_pkt_capture_resume_mon_thread(vdev);
				hdd_objmgr_put_vdev(vdev);
			} else {
				hdd_err("vdev is NULL");
			}
		}
	}

	ucfg_pmo_notify_system_resume(hdd_ctx->psoc);

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_CFG80211_RESUME_WLAN,
		   NO_SESSION, hdd_ctx->is_wiphy_suspended);

	hdd_ctx->is_wiphy_suspended = false;

	hdd_ctx->suspend_resume_stats.resumes++;
	exit_code = 0;

exit_with_code:
	hdd_exit();
	return exit_code;
}

static int _wlan_hdd_cfg80211_resume_wlan(struct wiphy *wiphy)
{
	void *hif_ctx;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	int errno;

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (hif_ctx)
		hif_pm_runtime_put(hif_ctx, RTPM_ID_SUSPEND_RESUME);

	if (!hdd_ctx) {
		hdd_err_rl("hdd context is null");
		return -ENODEV;
	}

	/* If Wifi is off, return success for system resume */
	if (hdd_ctx->driver_status != DRIVER_MODULES_ENABLED) {
		hdd_debug("Driver Modules not Enabled ");
		return 0;
	}

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;


	errno = __wlan_hdd_cfg80211_resume_wlan(wiphy);

	/* It may happen during cfg80211 suspend this timer is stopped.
	 * This means that if
	 * 1) work was queued in the workqueue, it was removed from the
	 *    workqueue and suspend proceeded.
	 * 2) The work was scheduled and cfg80211 suspend waited for this
	 *    work to complete and then suspend proceeded.
	 * So here in cfg80211 resume, check if no interface is up and
	 * the module state is enabled then trigger idle timer start.
	 */
	if (!hdd_is_any_interface_open(hdd_ctx) &&
	    hdd_ctx->driver_status == DRIVER_MODULES_ENABLED)
		hdd_psoc_idle_timer_start(hdd_ctx);

	return errno;
}

int wlan_hdd_cfg80211_resume_wlan(struct wiphy *wiphy)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = _wlan_hdd_cfg80211_resume_wlan(wiphy);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

static void hdd_suspend_cb(void)
{
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("HDD context is NULL");
		return;
	}

	complete(&hdd_ctx->mc_sus_event_var);
}

/**
 * __wlan_hdd_cfg80211_suspend_wlan() - cfg80211 suspend callback
 * @wiphy: Pointer to wiphy
 * @wow: Pointer to wow
 *
 * This API is called when cfg80211 driver suspends
 *
 * Return: integer status
 */
static int __wlan_hdd_cfg80211_suspend_wlan(struct wiphy *wiphy,
				     struct cfg80211_wowlan *wow)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct hdd_adapter *adapter, *next_adapter = NULL;
	mac_handle_t mac_handle;
	struct wlan_objmgr_vdev *vdev;
	int rc;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_CFG80211_SUSPEND_WLAN;

	hdd_enter();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam() ||
	    QDF_GLOBAL_MONITOR_MODE == hdd_get_conparam()) {
		hdd_err_rl("Command not allowed in mode %d",
			   hdd_get_conparam());
		return -EINVAL;
	}

	rc = wlan_hdd_validate_context(hdd_ctx);
	if (0 != rc)
		return rc;

	if (hdd_ctx->config->is_wow_disabled) {
		hdd_info_rl("wow is disabled");
		return -EINVAL;
	}

	if (hdd_ctx->driver_status != DRIVER_MODULES_ENABLED) {
		hdd_debug("Driver Modules not Enabled ");
		return 0;
	}

	mac_handle = hdd_ctx->mac_handle;

	/* If RADAR detection is in progress (HDD), prevent suspend. The flag
	 * "dfs_cac_block_tx" is set to true when RADAR is found and stay true
	 * until CAC is done for a SoftAP which is in started state.
	 */
	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (wlan_hdd_validate_vdev_id(adapter->vdev_id)) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			continue;
		}

		if (QDF_SAP_MODE == adapter->device_mode) {
			if (BSS_START ==
			    WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter)->bss_state &&
			    true ==
			    WLAN_HDD_GET_AP_CTX_PTR(adapter)->
			    dfs_cac_block_tx) {
				hdd_err("RADAR detection in progress, do not allow suspend");
				wlan_hdd_inc_suspend_stats(hdd_ctx,
							   SUSPEND_FAIL_RADAR);
				hdd_adapter_dev_put_debug(adapter, dbgid);
				if (next_adapter)
					hdd_adapter_dev_put_debug(next_adapter,
								  dbgid);
				return -EAGAIN;
			} else if (!ucfg_pmo_get_enable_sap_suspend(
				   hdd_ctx->psoc)) {
				/* return -EOPNOTSUPP if SAP does not support
				 * suspend
				 */
				hdd_err("SAP does not support suspend!!");
				hdd_adapter_dev_put_debug(adapter, dbgid);
				if (next_adapter)
					hdd_adapter_dev_put_debug(next_adapter,
								  dbgid);
				return -EOPNOTSUPP;
			}
		} else if (QDF_P2P_GO_MODE == adapter->device_mode) {
			if (!ucfg_pmo_get_enable_sap_suspend(
				   hdd_ctx->psoc)) {
				/* return -EOPNOTSUPP if GO does not support
				 * suspend
				 */
				hdd_err("GO does not support suspend!!");
				hdd_adapter_dev_put_debug(adapter, dbgid);
				if (next_adapter)
					hdd_adapter_dev_put_debug(next_adapter,
								  dbgid);
				return -EOPNOTSUPP;
			}
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}
	/* p2p cleanup task based on scheduler */
	ucfg_p2p_cleanup_tx_by_psoc(hdd_ctx->psoc);
	ucfg_p2p_cleanup_roc_by_psoc(hdd_ctx->psoc);

	if (hdd_is_connection_in_progress(NULL, NULL)) {
		hdd_err_rl("Connection is in progress, rejecting suspend");
		return -EINVAL;
	}

	/* flush any pending powersave timers */
	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (wlan_hdd_validate_vdev_id(adapter->vdev_id)) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			continue;
		}

		sme_ps_timer_flush_sync(mac_handle, adapter->vdev_id);
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	/*
	 * Suspend all components registered to pmo, abort ongoing scan and
	 * don't allow new scan any more before scheduler thread suspended.
	 */
	if (ucfg_pmo_suspend_all_components(hdd_ctx->psoc,
					    QDF_SYSTEM_SUSPEND)) {
		hdd_err("Some components not ready to suspend!");
		return -EAGAIN;
	}

	/*
	 * Suspend IPA early before proceeding to suspend other entities like
	 * firmware to avoid any race conditions.
	 */
	if (ucfg_ipa_suspend(hdd_ctx->pdev)) {
		hdd_err("IPA not ready to suspend!");
		wlan_hdd_inc_suspend_stats(hdd_ctx, SUSPEND_FAIL_IPA);
		return -EAGAIN;
	}

	/* Suspend control path scheduler */
	scheduler_register_hdd_suspend_callback(hdd_suspend_cb);
	scheduler_set_event_mask(MC_SUSPEND_EVENT);
	scheduler_wake_up_controller_thread();

	/* Wait for suspend confirmation from scheduler */
	rc = wait_for_completion_timeout(&hdd_ctx->mc_sus_event_var,
		msecs_to_jiffies(WLAN_WAIT_TIME_MCTHREAD_SUSPEND));
	if (!rc) {
		scheduler_clear_event_mask(MC_SUSPEND_EVENT);
		hdd_err("Failed to stop mc thread");
		goto resume_tx;
	}
	hdd_ctx->is_scheduler_suspended = true;

	if (hdd_ctx->enable_rxthread) {
		if (wlan_hdd_rx_thread_suspend(hdd_ctx))
			goto resume_ol_rx;
	}

	if (hdd_ctx->enable_dp_rx_threads) {
		if (dp_txrx_suspend(cds_get_context(QDF_MODULE_ID_SOC)))
			goto resume_ol_rx;
	}

	if (ucfg_pkt_capture_get_mode(hdd_ctx->psoc) !=
						PACKET_CAPTURE_MODE_DISABLE) {
		adapter = hdd_get_adapter(hdd_ctx, QDF_MONITOR_MODE);
		if (adapter) {
			vdev = hdd_objmgr_get_vdev(adapter);
			if (!vdev) {
				hdd_err("vdev is NULL");
				goto resume_dp_thread;
			}
			if (ucfg_pkt_capture_suspend_mon_thread(vdev)) {
				hdd_objmgr_put_vdev(vdev);
				goto resume_dp_thread;
			}
			hdd_objmgr_put_vdev(vdev);
		}
	}

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_CFG80211_SUSPEND_WLAN,
		   NO_SESSION, hdd_ctx->is_wiphy_suspended);

	if (hdd_suspend_wlan() < 0) {
		hdd_err("Failed to suspend WLAN");
		goto resume_dp_thread;
	}

	hdd_ctx->is_wiphy_suspended = true;

	hdd_exit();
	return 0;

resume_dp_thread:
	if (hdd_ctx->enable_dp_rx_threads)
		dp_txrx_resume(cds_get_context(QDF_MODULE_ID_SOC));

	/* Resume packet capture MON thread */
	if (ucfg_pkt_capture_get_mode(hdd_ctx->psoc) !=
						PACKET_CAPTURE_MODE_DISABLE) {
		adapter = hdd_get_adapter(hdd_ctx, QDF_MONITOR_MODE);
		if (adapter) {
			vdev = hdd_objmgr_get_vdev(adapter);
			if (vdev) {
				ucfg_pkt_capture_resume_mon_thread(vdev);
				hdd_objmgr_put_vdev(vdev);
			} else {
				hdd_err("vdev is NULL");
			}
		}
	}

resume_ol_rx:
	/* Resume tlshim Rx thread */
	wlan_hdd_rx_thread_resume(hdd_ctx);
	scheduler_resume();
	hdd_ctx->is_scheduler_suspended = false;
resume_tx:
	hdd_resume_wlan();
	return -ETIME;

}

static int _wlan_hdd_cfg80211_suspend_wlan(struct wiphy *wiphy,
					   struct cfg80211_wowlan *wow)
{
	void *hif_ctx;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	int errno;

	if (!hdd_ctx) {
		hdd_err_rl("hdd context is null");
		return -ENODEV;
	}

	/* If Wifi is off, return success for system suspend */
	if (hdd_ctx->driver_status != DRIVER_MODULES_ENABLED) {
		hdd_debug("Driver Modules not Enabled ");
		return 0;
	}

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_ctx)
		return -EINVAL;

	errno = hif_pm_runtime_get_sync(hif_ctx, RTPM_ID_SUSPEND_RESUME);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_suspend_wlan(wiphy, wow);
	if (errno) {
		hif_pm_runtime_put(hif_ctx, RTPM_ID_SUSPEND_RESUME);
		return errno;
	}

	return errno;
}

int wlan_hdd_cfg80211_suspend_wlan(struct wiphy *wiphy,
				   struct cfg80211_wowlan *wow)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (0 != errno)
		return errno;

	/*
	 * Flush the idle shutdown before ops start.This is done here to avoid
	 * the deadlock as idle shutdown waits for the dsc ops
	 * to complete.
	 */
	hdd_psoc_idle_timer_stop(hdd_ctx);

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = _wlan_hdd_cfg80211_suspend_wlan(wiphy, wow);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

/**
 * hdd_stop_dhcp_ind() - API to stop DHCP sequence
 * @adapter: Adapter on which DHCP needs to be stopped
 *
 * Release the wakelock held for DHCP process and allow
 * the runtime pm to continue
 *
 * Return: None
 */
static void hdd_stop_dhcp_ind(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	hdd_debug("DHCP stop indicated through power save");
	sme_dhcp_stop_ind(hdd_ctx->mac_handle, adapter->device_mode,
			  adapter->mac_addr.bytes,
			  adapter->vdev_id);
	hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_DHCP);
	qdf_runtime_pm_allow_suspend(&hdd_ctx->runtime_context.connect);
}

/**
 * hdd_start_dhcp_ind() - API to start DHCP sequence
 * @adapter: Adapter on which DHCP needs to be stopped
 *
 * Prevent APPS suspend and the runtime suspend during
 * DHCP sequence
 *
 * Return: None
 */
static void hdd_start_dhcp_ind(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	hdd_debug("DHCP start indicated through power save");
	qdf_runtime_pm_prevent_suspend(&hdd_ctx->runtime_context.connect);
	hdd_prevent_suspend_timeout(HDD_WAKELOCK_TIMEOUT_CONNECT,
				    WIFI_POWER_EVENT_WAKELOCK_DHCP);
	sme_dhcp_start_ind(hdd_ctx->mac_handle, adapter->device_mode,
			   adapter->mac_addr.bytes,
			   adapter->vdev_id);
}

/**
 * __wlan_hdd_cfg80211_set_power_mgmt() - set cfg80211 power management config
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @allow_power_save: is wlan allowed to go into power save mode
 * @timeout: Timeout value in ms
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_set_power_mgmt(struct wiphy *wiphy,
					      struct net_device *dev,
					      bool allow_power_save,
					      int timeout)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx;
	int status;

	hdd_enter();

	if (timeout < 0) {
		hdd_debug("User space timeout: %d; Enter full power or power save",
			  timeout);
		timeout = 0;
	}

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	if (wlan_hdd_validate_vdev_id(adapter->vdev_id))
		return -EINVAL;

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_CFG80211_SET_POWER_MGMT,
		   adapter->vdev_id, timeout);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	status = wlan_hdd_validate_context(hdd_ctx);

	if (0 != status)
		return status;

	if (hdd_ctx->driver_status != DRIVER_MODULES_ENABLED) {
		hdd_debug("Driver Module not enabled return success");
		return 0;
	}

	status = wlan_hdd_set_powersave(adapter, allow_power_save, timeout);

	if (hdd_adapter_is_connected_sta(adapter)) {
		hdd_debug("vdev mode %d enable dhcp protection",
			  adapter->device_mode);
		allow_power_save ? hdd_stop_dhcp_ind(adapter) :
			hdd_start_dhcp_ind(adapter);
	} else {
		hdd_debug("vdev mod %d disconnected ignore dhcp protection",
			  adapter->device_mode);
	}

	hdd_exit();
	return status;
}

int wlan_hdd_cfg80211_set_power_mgmt(struct wiphy *wiphy,
				     struct net_device *dev,
				     bool allow_power_save,
				     int timeout)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_set_power_mgmt(wiphy, dev, allow_power_save,
						   timeout);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * __wlan_hdd_cfg80211_set_txpower() - set TX power
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to network device
 * @type: TX power setting type
 * @mbm: TX power in mBm
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_set_txpower(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   enum nl80211_tx_power_setting type,
					   int mbm)
{
	struct hdd_context *hdd_ctx = (struct hdd_context *) wiphy_priv(wiphy);
	mac_handle_t mac_handle;
	struct hdd_adapter *adapter;
	struct qdf_mac_addr bssid = QDF_MAC_ADDR_BCAST_INIT;
	struct qdf_mac_addr selfmac;
	QDF_STATUS status;
	int errno;
	int dbm;

	hdd_enter();

	if (!wdev) {
		hdd_err("wdev is null, set tx power failed");
		return -EIO;
	}

	adapter = WLAN_HDD_GET_PRIV_PTR(wdev->netdev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_CFG80211_SET_TXPOWER,
		   NO_SESSION, type);

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	if (wlan_hdd_validate_vdev_id(adapter->vdev_id))
		return -EINVAL;

	if (adapter->device_mode == QDF_SAP_MODE ||
	    adapter->device_mode == QDF_P2P_GO_MODE) {
		qdf_copy_macaddr(&bssid, &adapter->mac_addr);
	} else {
		struct hdd_station_ctx *sta_ctx =
			WLAN_HDD_GET_STATION_CTX_PTR(adapter);

		if (eConnectionState_Associated ==
		    sta_ctx->conn_info.conn_state)
			qdf_copy_macaddr(&bssid, &sta_ctx->conn_info.bssid);
	}

	qdf_copy_macaddr(&selfmac, &adapter->mac_addr);

	mac_handle = hdd_ctx->mac_handle;

	dbm = MBM_TO_DBM(mbm);

	/*
	 * the original implementation of this function expected power
	 * values in dBm instead of mBm. If the conversion from mBm to
	 * dBm is zero, then assume dBm was passed.
	 */
	if (!dbm)
		dbm = mbm;

	status = ucfg_mlme_set_current_tx_power_level(hdd_ctx->psoc, dbm);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("sme_cfg_set_int failed for tx power %hu, %d",
			dbm, status);
		return -EIO;
	}

	hdd_debug("Set tx power level %d dbm", dbm);

	switch (type) {
	/* Automatically determine transmit power */
	case NL80211_TX_POWER_AUTOMATIC:
	/* Fall through */
	case NL80211_TX_POWER_LIMITED:
	/* Limit TX power by the mBm parameter */
		status = sme_set_max_tx_power(mac_handle, bssid, selfmac, dbm);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Setting maximum tx power failed, %d", status);
			return -EIO;
		}
		break;

	case NL80211_TX_POWER_FIXED:    /* Fix TX power to the mBm parameter */
		status = sme_set_tx_power(mac_handle, adapter->vdev_id,
					  bssid, adapter->device_mode, dbm);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Setting tx power failed, %d", status);
			return -EIO;
		}
		break;
	default:
		hdd_err("Invalid power setting type %d", type);
		return -EIO;
	}

	hdd_exit();
	return 0;
}

int wlan_hdd_cfg80211_set_txpower(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  enum nl80211_tx_power_setting type,
				  int mbm)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_set_txpower(wiphy, wdev, type, mbm);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

static void wlan_hdd_get_tx_power(struct hdd_adapter *adapter, int *dbm)

{
	struct wlan_objmgr_vdev *vdev;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_info("vdev is NULL");
		return;
	}

	wlan_cfg80211_mc_cp_stats_get_tx_power(vdev, dbm);
	hdd_objmgr_put_vdev(vdev);
}

#ifdef FEATURE_ANI_LEVEL_REQUEST
static void hdd_get_ani_level_cb(struct wmi_host_ani_level_event *ani,
				 uint8_t num, void *context)
{
	struct osif_request *request;
	struct ani_priv *priv;
	uint8_t min_recv_freqs = QDF_MIN(num, MAX_NUM_FREQS_FOR_ANI_LEVEL);

	request = osif_request_get(context);
	if (!request) {
		hdd_err("Obsolete request");
		return;
	}

	/* propagate response back to requesting thread */
	priv = osif_request_priv(request);
	priv->ani = qdf_mem_malloc(min_recv_freqs *
				   sizeof(struct wmi_host_ani_level_event));
	if (!priv->ani)
		goto complete;

	priv->num_freq = min_recv_freqs;
	qdf_mem_copy(priv->ani, ani,
		     min_recv_freqs * sizeof(struct wmi_host_ani_level_event));

complete:
	osif_request_complete(request);
	osif_request_put(request);
}

/**
 * wlan_hdd_get_ani_level_dealloc() - Dealloc mem allocated in priv data
 * @priv: the priv data
 *
 * Return: None
 */
static void wlan_hdd_get_ani_level_dealloc(void *priv)
{
	struct ani_priv *ani = priv;

	if (ani->ani)
		qdf_mem_free(ani->ani);
}

QDF_STATUS wlan_hdd_get_ani_level(struct hdd_adapter *adapter,
				  struct wmi_host_ani_level_event *ani,
				  uint32_t *parsed_freqs,
				  uint8_t num_freqs)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	int ret;
	QDF_STATUS status;
	void *cookie;
	struct osif_request *request;
	struct ani_priv *priv;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = 1000,
		.dealloc = wlan_hdd_get_ani_level_dealloc,
	};

	if (!hdd_ctx) {
		hdd_err("Invalid HDD context");
		return QDF_STATUS_E_INVAL;
	}

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return QDF_STATUS_E_NOMEM;
	}
	cookie = osif_request_cookie(request);

	status = sme_get_ani_level(hdd_ctx->mac_handle, parsed_freqs,
				   num_freqs, hdd_get_ani_level_cb, cookie);

	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Unable to retrieve ani level");
		goto complete;
	} else {
		/* request was sent -- wait for the response */
		ret = osif_request_wait_for_response(request);
		if (ret) {
			hdd_err("SME timed out while retrieving ANI level");
			status = QDF_STATUS_E_TIMEOUT;
			goto complete;
		}
	}

	priv = osif_request_priv(request);

	qdf_mem_copy(ani, priv->ani, sizeof(struct wmi_host_ani_level_event) *
		     priv->num_freq);

complete:
	/*
	 * either we never sent a request, we sent a request and
	 * received a response or we sent a request and timed out.
	 * regardless we are done with the request.
	 */
	osif_request_put(request);

	hdd_exit();
	return status;
}
#endif

/**
 * __wlan_hdd_cfg80211_get_txpower() - get TX power
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to network device
 * @dbm: Pointer to TX power in dbm
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_get_txpower(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  int *dbm)
{

	struct hdd_context *hdd_ctx = (struct hdd_context *) wiphy_priv(wiphy);
	struct net_device *ndev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(ndev);
	int status;
	struct hdd_station_ctx *sta_ctx;
	static bool is_rate_limited;

	hdd_enter_dev(ndev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	*dbm = 0;

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return status;

	/* Validate adapter sessionId */
	status = wlan_hdd_validate_vdev_id(adapter->vdev_id);
	if (status)
		return status;
	switch (adapter->device_mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
		sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
		if (sta_ctx->hdd_reassoc_scenario) {
			hdd_debug("Roaming is in progress, rej this req");
			return -EINVAL;
		}
		if (sta_ctx->conn_info.conn_state !=
		    eConnectionState_Associated) {
			hdd_debug("Not associated");
			return 0;
		}
		break;
	case QDF_SAP_MODE:
	case QDF_P2P_GO_MODE:
		if (!test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
			hdd_debug("SAP is not started yet");
			return 0;
		}
		break;
	default:
		hdd_debug_rl("Current interface is not supported for get tx_power");
		return 0;
	}

	HDD_IS_RATE_LIMIT_REQ(is_rate_limited,
			      hdd_ctx->config->nb_commands_interval);
	if (hdd_ctx->driver_status != DRIVER_MODULES_ENABLED ||
	    is_rate_limited) {
		hdd_debug("Modules not enabled/rate limited, use cached stats");
		/* Send cached data to upperlayer*/
		*dbm = adapter->hdd_stats.class_a_stat.max_pwr;
		return 0;
	}

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_CFG80211_GET_TXPOWER,
		   adapter->vdev_id, adapter->device_mode);

	wlan_hdd_get_tx_power(adapter, dbm);
	hdd_debug("power: %d", *dbm);

	return 0;
}

int wlan_hdd_cfg80211_get_txpower(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 int *dbm)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_get_txpower(wiphy, wdev, dbm);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

int hdd_set_power_config(struct hdd_context *hddctx,
			 struct hdd_adapter *adapter,
			 uint8_t power)
{
	QDF_STATUS status;

	if (adapter->device_mode != QDF_STA_MODE &&
	    adapter->device_mode != QDF_P2P_CLIENT_MODE) {
		hdd_info("Advanced power save only allowed in STA/P2P-Client modes:%d",
			 adapter->device_mode);
		return -EINVAL;
	}

	if (power > PMO_PS_ADVANCED_POWER_SAVE_ENABLE ||
	    power < PMO_PS_ADVANCED_POWER_SAVE_DISABLE) {
		hdd_err("invalid power value: %d", power);
		return -EINVAL;
	}

	if (ucfg_pmo_get_max_ps_poll(hddctx->psoc)) {
		hdd_info("Disable advanced power save since max ps poll is enabled");
		power = PMO_PS_ADVANCED_POWER_SAVE_DISABLE;
	}

	status = wma_set_power_config(adapter->vdev_id, power);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("failed to configure power: %d", status);
		return -EINVAL;
	}

	/* cache latest userspace power save config to reapply after SSR */
	ucfg_pmo_set_power_save_mode(hddctx->psoc, power);

	return 0;
}


#ifdef WLAN_SUSPEND_RESUME_TEST
static struct net_device *g_dev;
static struct wiphy *g_wiphy;
static enum wow_resume_trigger g_resume_trigger;

#define HDD_FA_SUSPENDED_BIT (0)
static unsigned long fake_apps_state;

/**
 * __hdd_wlan_fake_apps_resume() - The core logic for
 *	hdd_wlan_fake_apps_resume() skipping the call to hif_fake_apps_resume(),
 *	which is only need for non-irq resume
 * @wiphy: the kernel wiphy struct for the device being resumed
 * @dev: the kernel net_device struct for the device being resumed
 *
 * Return: none, calls QDF_BUG() on failure
 */
static void __hdd_wlan_fake_apps_resume(struct wiphy *wiphy,
					struct net_device *dev)
{
	struct hif_opaque_softc *hif_ctx;
	qdf_device_t qdf_dev;

	hdd_info("Unit-test resume WLAN");

	qdf_dev = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	if (!qdf_dev) {
		hdd_err("Failed to get QDF device context");
		QDF_BUG(0);
		return;
	}

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_ctx) {
		hdd_err("Failed to get HIF context");
		return;
	}

	if (!test_and_clear_bit(HDD_FA_SUSPENDED_BIT, &fake_apps_state)) {
		hdd_alert("Not unit-test suspended; Nothing to do");
		return;
	}

	/* simulate kernel disable irqs */
	QDF_BUG(!hif_apps_wake_irq_disable(hif_ctx));

	QDF_BUG(!wlan_hdd_bus_resume_noirq());

	/* simulate kernel enable irqs */
	QDF_BUG(!hif_apps_irqs_enable(hif_ctx));

	QDF_BUG(!wlan_hdd_bus_resume());

	QDF_BUG(!wlan_hdd_cfg80211_resume_wlan(wiphy));

	if (g_resume_trigger == WOW_RESUME_TRIGGER_HTC_WAKEUP)
		hif_vote_link_down(hif_ctx);

	dev->watchdog_timeo = HDD_TX_TIMEOUT;

	hdd_alert("Unit-test resume succeeded");
}

/**
 * hdd_wlan_fake_apps_resume_irq_callback() - Irq callback function for resuming
 *	from unit-test initiated suspend from irq wakeup signal
 *
 * Resume wlan after getting very 1st CE interrupt from target
 *
 * Return: none
 */
static void hdd_wlan_fake_apps_resume_irq_callback(void)
{
	hdd_info("Trigger unit-test resume WLAN");

	QDF_BUG(g_wiphy);
	QDF_BUG(g_dev);
	__hdd_wlan_fake_apps_resume(g_wiphy, g_dev);
	g_wiphy = NULL;
	g_dev = NULL;
}

int hdd_wlan_fake_apps_suspend(struct wiphy *wiphy, struct net_device *dev,
			       enum wow_interface_pause pause_setting,
			       enum wow_resume_trigger resume_setting)
{
	int errno;
	qdf_device_t qdf_dev;
	struct hif_opaque_softc *hif_ctx;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	struct wow_enable_params wow_params = {
		.is_unit_test = true,
		.interface_pause = pause_setting,
		.resume_trigger = resume_setting
	};

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	if (!hdd_ctx->config->is_unit_test_framework_enabled) {
		hdd_warn_rl("UT framework is disabled");
		return -EINVAL;
	}

	hdd_info("Unit-test suspend WLAN");

	if (pause_setting < WOW_INTERFACE_PAUSE_DEFAULT ||
	    pause_setting >= WOW_INTERFACE_PAUSE_COUNT) {
		hdd_err("Invalid interface pause %d (expected range [0, 2])",
			pause_setting);
		return -EINVAL;
	}

	if (resume_setting < WOW_RESUME_TRIGGER_DEFAULT ||
	    resume_setting >= WOW_RESUME_TRIGGER_COUNT) {
		hdd_err("Invalid resume trigger %d (expected range [0, 2])",
			resume_setting);
		return -EINVAL;
	}

	qdf_dev = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	if (!qdf_dev) {
		hdd_err("Failed to get QDF device context");
		return -EINVAL;
	}

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_ctx) {
		hdd_err("Failed to get HIF context");
		return -EINVAL;
	}

	if (test_and_set_bit(HDD_FA_SUSPENDED_BIT, &fake_apps_state)) {
		hdd_alert("Already unit-test suspended; Nothing to do");
		return 0;
	}

	/* pci link is needed to wakeup from HTC wakeup trigger */
	if (resume_setting == WOW_RESUME_TRIGGER_HTC_WAKEUP)
		hif_vote_link_up(hif_ctx);

	errno = wlan_hdd_cfg80211_suspend_wlan(wiphy, NULL);
	if (errno)
		goto link_down;

	errno = wlan_hdd_unit_test_bus_suspend(wow_params);
	if (errno)
		goto cfg80211_resume;

	/* simulate kernel disabling irqs */
	errno = hif_apps_irqs_disable(hif_ctx);
	if (errno)
		goto bus_resume;

	errno = wlan_hdd_bus_suspend_noirq();
	if (errno)
		goto enable_irqs;

	/* pass wiphy/dev to callback via global variables */
	g_wiphy = wiphy;
	g_dev = dev;
	g_resume_trigger = resume_setting;
	hif_ut_apps_suspend(hif_ctx, hdd_wlan_fake_apps_resume_irq_callback);

	/* re-enable wake irq */
	errno = hif_apps_wake_irq_enable(hif_ctx);
	if (errno)
		goto fake_apps_resume;

	/*
	 * Tell the kernel not to worry if TX queues aren't moving. This is
	 * expected since we are suspending the wifi hardware, but not APPS
	 */
	dev->watchdog_timeo = INT_MAX;

	hdd_alert("Unit-test suspend succeeded");

	return 0;

fake_apps_resume:
	hif_ut_apps_resume(hif_ctx);

enable_irqs:
	QDF_BUG(!hif_apps_irqs_enable(hif_ctx));

bus_resume:
	QDF_BUG(!wlan_hdd_bus_resume());

cfg80211_resume:
	QDF_BUG(!wlan_hdd_cfg80211_resume_wlan(wiphy));

link_down:
	hif_vote_link_down(hif_ctx);

	clear_bit(HDD_FA_SUSPENDED_BIT, &fake_apps_state);
	hdd_err("Unit-test suspend failed: %d", errno);

	return errno;
}

int hdd_wlan_fake_apps_resume(struct wiphy *wiphy, struct net_device *dev)
{
	struct hif_opaque_softc *hif_ctx;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	if (!hdd_ctx->config->is_unit_test_framework_enabled) {
		hdd_warn_rl("UT framework is disabled");
		return -EINVAL;
	}

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_ctx) {
		hdd_err("Failed to get HIF context");
		return -EINVAL;
	}

	hif_ut_apps_resume(hif_ctx);
	__hdd_wlan_fake_apps_resume(wiphy, dev);

	return 0;
}
#endif
