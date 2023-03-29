/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/slab.h>
#include "osif_psoc_sync.h"

#include "pld_pcie_fw_sim.h"

#if defined(CONFIG_PLD_PCIE_FW_SIM) || defined(CONFIG_PLD_IPCIE_FW_SIM)

#ifdef QCA_WIFI_3_0_ADRASTEA
#define CE_COUNT_MAX 12
#else
#define CE_COUNT_MAX 8
#endif

#endif

#ifdef CONFIG_PLD_IPCIE_FW_SIM
/**
 * pld_pcie_fw_sim_probe() - Probe function for PCIE platform driver
 * @pdev: PCIE device
 * @id: PCIE device ID table
 *
 * The probe function will be called when PCIE device provided
 * in the ID table is detected.
 *
 * Return: int
 */
static int pld_pcie_fw_sim_probe(struct pci_dev *pdev,
				 const struct pci_device_id *id)
{
	struct pld_context *pld_context;
	int ret = 0;

	pld_context = pld_get_global_context();
	if (!pld_context) {
		ret = -ENODEV;
		goto out;
	}

	ret = pld_add_dev(pld_context, &pdev->dev, NULL,
			  PLD_BUS_TYPE_IPCI_FW_SIM);
	if (ret)
		goto out;

	return pld_context->ops->probe(&pdev->dev,
		       PLD_BUS_TYPE_IPCI_FW_SIM, NULL, NULL);

out:
	return ret;
}

/**
 * pld_pcie_fw_sim_remove() - Remove function for PCIE device
 * @pdev: PCIE device
 *
 * The remove function will be called when PCIE device is disconnected
 *
 * Return: void
 */
static void pld_pcie_fw_sim_remove(struct pci_dev *pdev)
{
	struct pld_context *pld_context;
	int errno;
	struct osif_psoc_sync *psoc_sync;

	errno = osif_psoc_sync_trans_start_wait(&pdev->dev, &psoc_sync);
	if (errno)
		return;

	osif_psoc_sync_unregister(&pdev->dev);
	osif_psoc_sync_wait_for_ops(psoc_sync);

	pld_context = pld_get_global_context();

	if (!pld_context)
		goto out;

	pld_context->ops->remove(&pdev->dev, PLD_BUS_TYPE_IPCI_FW_SIM);

	pld_del_dev(pld_context, &pdev->dev);

out:
	osif_psoc_sync_trans_stop(psoc_sync);
	osif_psoc_sync_destroy(psoc_sync);
}

/**
 * pld_pcie_fw_sim_idle_restart_cb() - Perform idle restart
 * @pdev: PCIE device
 * @id: PCIE device ID
 *
 * This function will be called if there is an idle restart request
 *
 * Return: int
 */
static int pld_pcie_fw_sim_idle_restart_cb(struct pci_dev *pdev,
					   const struct pci_device_id *id)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->idle_restart)
		return pld_context->ops->idle_restart(&pdev->dev,
						      PLD_BUS_TYPE_IPCI_FW_SIM);

	return -ENODEV;
}

/**
 * pld_pcie_fw_sim_idle_shutdown_cb() - Perform idle shutdown
 * @pdev: PCIE device
 * @id: PCIE device ID
 *
 * This function will be called if there is an idle shutdown request
 *
 * Return: int
 */
static int pld_pcie_fw_sim_idle_shutdown_cb(struct pci_dev *pdev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->shutdown)
		return pld_context->ops->idle_shutdown(&pdev->dev,
						PLD_BUS_TYPE_IPCI_FW_SIM);

	return -ENODEV;
}

/**
 * pld_pcie_fw_sim_reinit() - SSR re-initialize function for PCIE device
 * @pdev: PCIE device
 * @id: PCIE device ID
 *
 * During subsystem restart(SSR), this function will be called to
 * re-initialize pcie device.
 *
 * Return: int
 */
static int pld_pcie_fw_sim_reinit(struct pci_dev *pdev,
				  const struct pci_device_id *id)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->reinit)
		return pld_context->ops->reinit(&pdev->dev,
				PLD_BUS_TYPE_IPCI_FW_SIM, NULL, NULL);

	return -ENODEV;
}

/**
 * pld_pcie_fw_sim_shutdown() - SSR shutdown function for PCIE device
 * @pdev: PCIE device
 *
 * During SSR, this function will be called to shutdown PCIE device.
 *
 * Return: void
 */
static void pld_pcie_fw_sim_shutdown(struct pci_dev *pdev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->shutdown)
		pld_context->ops->shutdown(&pdev->dev,
						PLD_BUS_TYPE_IPCI_FW_SIM);
}

/**
 * pld_pcie_fw_sim_crash_shutdown() - Crash shutdown function for PCIE device
 * @pdev: PCIE device
 *
 * This function will be called when a crash is detected, it will shutdown
 * the PCIE device.
 *
 * Return: void
 */
static void pld_pcie_fw_sim_crash_shutdown(struct pci_dev *pdev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->crash_shutdown)
		pld_context->ops->crash_shutdown(&pdev->dev,
						PLD_BUS_TYPE_IPCI_FW_SIM);
}

/**
 * pld_pcie_fw_sim_notify_handler() - Modem state notification callback function
 * @pdev: PCIE device
 * @state: modem power state
 *
 * This function will be called when there's a modem power state change.
 *
 * Return: void
 */
static void pld_pcie_fw_sim_notify_handler(struct pci_dev *pdev, int state)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->modem_status)
		pld_context->ops->modem_status(&pdev->dev,
					       PLD_BUS_TYPE_IPCI_FW_SIM, state);
}

/**
 * pld_pcie_fw_sim_uevent() - update wlan driver status callback function
 * @pdev: PCIE device
 * @status driver uevent status
 *
 * This function will be called when platform driver wants to update wlan
 * driver's status.
 *
 * Return: void
 */
static void pld_pcie_fw_sim_uevent(struct pci_dev *pdev, uint32_t status)
{
	struct pld_context *pld_context;
	struct pld_uevent_data data = {0};

	pld_context = pld_get_global_context();
	if (!pld_context)
		return;

	switch (status) {
	case CNSS_RECOVERY:
		data.uevent = PLD_FW_RECOVERY_START;
		break;
	case CNSS_FW_DOWN:
		data.uevent = PLD_FW_DOWN;
		break;
	default:
		goto out;
	}

	if (pld_context->ops->uevent)
		pld_context->ops->uevent(&pdev->dev, &data);

out:
	return;
}

/**
 * pld_pcie_fw_sim_set_thermal_state: Set thermal state for thermal mitigation
 * @dev: device
 * @thermal_state: Thermal state set by thermal subsystem
 * @mon_id: Thermal cooling device ID
 *
 * This function will be called when thermal subsystem notifies platform
 * driver about change in thermal state.
 *
 * Return: 0 for success
 * Non zero failure code for errors
 */
static int pld_pcie_fw_sim_set_thermal_state(struct device *dev,
					     unsigned long thermal_state,
					     int mon_id)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops->set_curr_therm_cdev_state)
		return pld_context->ops->set_curr_therm_cdev_state(dev,
							      thermal_state,
							      mon_id);

	return -ENOTSUPP;
}

static struct pci_device_id pld_pcie_fw_sim_id_table[] = {
	{ 0x168c, 0x003c, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0x003e, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0x0041, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0xabcd, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0x7021, PCI_ANY_ID, PCI_ANY_ID },
	{ 0 }
};

#ifdef MULTI_IF_NAME
#define PLD_PCIE_FW_SIM_OPS_NAME "pld_ipcie_fw_sim_" MULTI_IF_NAME
#else
#define PLD_PCIE_FW_SIM_OPS_NAME "pld_ipcie_fw_sim"
#endif

#endif

#ifdef CONFIG_PLD_PCIE_FW_SIM
/**
 * pld_pcie_fw_sim_probe() - Probe function for PCIE platform driver
 * @pdev: PCIE device
 * @id: PCIE device ID table
 *
 * The probe function will be called when PCIE device provided
 * in the ID table is detected.
 *
 * Return: int
 */
static int pld_pcie_fw_sim_probe(struct pci_dev *pdev,
				 const struct pci_device_id *id)
{
	struct pld_context *pld_context;
	int ret = 0;

	pld_context = pld_get_global_context();
	if (!pld_context) {
		ret = -ENODEV;
		goto out;
	}

	ret = pld_add_dev(pld_context, &pdev->dev, NULL,
			  PLD_BUS_TYPE_PCIE_FW_SIM);
	if (ret)
		goto out;

	return pld_context->ops->probe(&pdev->dev,
		       PLD_BUS_TYPE_PCIE_FW_SIM, pdev, (void *)id);

out:
	return ret;
}

/**
 * pld_pcie_fw_sim_remove() - Remove function for PCIE device
 * @pdev: PCIE device
 *
 * The remove function will be called when PCIE device is disconnected
 *
 * Return: void
 */
static void pld_pcie_fw_sim_remove(struct pci_dev *pdev)
{
	struct pld_context *pld_context;
	int errno;
	struct osif_psoc_sync *psoc_sync;

	errno = osif_psoc_sync_trans_start_wait(&pdev->dev, &psoc_sync);
	if (errno)
		return;

	osif_psoc_sync_unregister(&pdev->dev);
	osif_psoc_sync_wait_for_ops(psoc_sync);

	pld_context = pld_get_global_context();

	if (!pld_context)
		goto out;

	pld_context->ops->remove(&pdev->dev, PLD_BUS_TYPE_PCIE_FW_SIM);

	pld_del_dev(pld_context, &pdev->dev);

out:
	osif_psoc_sync_trans_stop(psoc_sync);
	osif_psoc_sync_destroy(psoc_sync);
}

/**
 * pld_pcie_fw_sim_idle_restart_cb() - Perform idle restart
 * @pdev: PCIE device
 * @id: PCIE device ID
 *
 * This function will be called if there is an idle restart request
 *
 * Return: int
 */
static int pld_pcie_fw_sim_idle_restart_cb(struct pci_dev *pdev,
					   const struct pci_device_id *id)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->idle_restart)
		return pld_context->ops->idle_restart(&pdev->dev,
						      PLD_BUS_TYPE_PCIE_FW_SIM);

	return -ENODEV;
}

/**
 * pld_pcie_fw_sim_idle_shutdown_cb() - Perform idle shutdown
 * @pdev: PCIE device
 * @id: PCIE device ID
 *
 * This function will be called if there is an idle shutdown request
 *
 * Return: int
 */
static int pld_pcie_fw_sim_idle_shutdown_cb(struct pci_dev *pdev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->shutdown)
		return pld_context->ops->idle_shutdown(&pdev->dev,
						PLD_BUS_TYPE_PCIE_FW_SIM);

	return -ENODEV;
}

/**
 * pld_pcie_fw_sim_reinit() - SSR re-initialize function for PCIE device
 * @pdev: PCIE device
 * @id: PCIE device ID
 *
 * During subsystem restart(SSR), this function will be called to
 * re-initialize pcie device.
 *
 * Return: int
 */
static int pld_pcie_fw_sim_reinit(struct pci_dev *pdev,
				  const struct pci_device_id *id)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->reinit)
		return pld_context->ops->reinit(&pdev->dev,
				PLD_BUS_TYPE_PCIE_FW_SIM, pdev, (void *)id);

	return -ENODEV;
}

/**
 * pld_pcie_fw_sim_shutdown() - SSR shutdown function for PCIE device
 * @pdev: PCIE device
 *
 * During SSR, this function will be called to shutdown PCIE device.
 *
 * Return: void
 */
static void pld_pcie_fw_sim_shutdown(struct pci_dev *pdev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->shutdown)
		pld_context->ops->shutdown(&pdev->dev,
						PLD_BUS_TYPE_PCIE_FW_SIM);
}

/**
 * pld_pcie_fw_sim_crash_shutdown() - Crash shutdown function for PCIE device
 * @pdev: PCIE device
 *
 * This function will be called when a crash is detected, it will shutdown
 * the PCIE device.
 *
 * Return: void
 */
static void pld_pcie_fw_sim_crash_shutdown(struct pci_dev *pdev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->crash_shutdown)
		pld_context->ops->crash_shutdown(&pdev->dev,
						PLD_BUS_TYPE_PCIE_FW_SIM);
}

/**
 * pld_pcie_fw_sim_notify_handler() - Modem state notification callback function
 * @pdev: PCIE device
 * @state: modem power state
 *
 * This function will be called when there's a modem power state change.
 *
 * Return: void
 */
static void pld_pcie_fw_sim_notify_handler(struct pci_dev *pdev, int state)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->modem_status)
		pld_context->ops->modem_status(&pdev->dev,
					       PLD_BUS_TYPE_PCIE_FW_SIM, state);
}

/**
 * pld_pcie_fw_sim_uevent() - update wlan driver status callback function
 * @pdev: PCIE device
 * @status driver uevent status
 *
 * This function will be called when platform driver wants to update wlan
 * driver's status.
 *
 * Return: void
 */
static void pld_pcie_fw_sim_uevent(struct pci_dev *pdev, uint32_t status)
{
	struct pld_context *pld_context;
	struct pld_uevent_data data = {0};

	pld_context = pld_get_global_context();
	if (!pld_context)
		return;

	switch (status) {
	case CNSS_RECOVERY:
		data.uevent = PLD_FW_RECOVERY_START;
		break;
	case CNSS_FW_DOWN:
		data.uevent = PLD_FW_DOWN;
		break;
	default:
		goto out;
	}

	if (pld_context->ops->uevent)
		pld_context->ops->uevent(&pdev->dev, &data);

out:
	return;
}

/**
 * pld_pcie_fw_sim_set_thermal_state: Set thermal state for thermal mitigation
 * @dev: device
 * @thermal_state: Thermal state set by thermal subsystem
 * @mon_id: Thermal cooling device ID
 *
 * This function will be called when thermal subsystem notifies platform
 * driver about change in thermal state.
 *
 * Return: 0 for success
 * Non zero failure code for errors
 */
static int pld_pcie_fw_sim_set_thermal_state(struct device *dev,
					     unsigned long thermal_state,
					     int mon_id)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops->set_curr_therm_cdev_state)
		return pld_context->ops->set_curr_therm_cdev_state(dev,
							      thermal_state,
							      mon_id);

	return -ENOTSUPP;
}

static struct pci_device_id pld_pcie_fw_sim_id_table[] = {
	{ 0x168c, 0x003c, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0x003e, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0x0041, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0xabcd, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0x7021, PCI_ANY_ID, PCI_ANY_ID },
	{ 0 }
};

#ifdef MULTI_IF_NAME
#define PLD_PCIE_FW_SIM_OPS_NAME "pld_pcie_fw_sim_" MULTI_IF_NAME
#else
#define PLD_PCIE_FW_SIM_OPS_NAME "pld_pcie_fw_sim"
#endif

#endif

#if defined(CONFIG_PLD_PCIE_FW_SIM) || defined(CONFIG_PLD_IPCIE_FW_SIM)
struct cnss_wlan_driver pld_pcie_fw_sim_ops = {
	.name       = PLD_PCIE_FW_SIM_OPS_NAME,
	.id_table   = pld_pcie_fw_sim_id_table,
	.probe      = pld_pcie_fw_sim_probe,
	.remove     = pld_pcie_fw_sim_remove,
	.idle_restart  = pld_pcie_fw_sim_idle_restart_cb,
	.idle_shutdown = pld_pcie_fw_sim_idle_shutdown_cb,
	.reinit     = pld_pcie_fw_sim_reinit,
	.shutdown   = pld_pcie_fw_sim_shutdown,
	.crash_shutdown = pld_pcie_fw_sim_crash_shutdown,
	.modem_status   = pld_pcie_fw_sim_notify_handler,
	.update_status  = pld_pcie_fw_sim_uevent,
	.set_therm_cdev_state = pld_pcie_fw_sim_set_thermal_state,
};

/**
 * pld_pcie_fw_sim_register_driver() - Register PCIE device callback functions
 *
 * Return: int
 */
int pld_pcie_fw_sim_register_driver(void)
{
	return cnss_fw_sim_wlan_register_driver(&pld_pcie_fw_sim_ops);
}

/**
 * pld_pcie_fw_sim_unregister_driver() - Unregister PCIE device callback
 *					 functions
 *
 * Return: void
 */
void pld_pcie_fw_sim_unregister_driver(void)
{
	cnss_fw_sim_wlan_unregister_driver(&pld_pcie_fw_sim_ops);
}

/**
 * pld_pcie_fw_sim_wlan_enable() - Enable WLAN
 * @dev: device
 * @config: WLAN configuration data
 * @mode: WLAN mode
 * @host_version: host software version
 *
 * This function enables WLAN FW. It passed WLAN configuration data,
 * WLAN mode and host software version to FW.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pcie_fw_sim_wlan_enable(struct device *dev,
				struct pld_wlan_enable_cfg *config,
				enum pld_driver_mode mode,
				const char *host_version)
{
	struct cnss_wlan_enable_cfg cfg;
	enum cnss_driver_mode cnss_mode;

	cfg.num_ce_tgt_cfg = config->num_ce_tgt_cfg;
	cfg.ce_tgt_cfg = (struct cnss_ce_tgt_pipe_cfg *)
		config->ce_tgt_cfg;
	cfg.num_ce_svc_pipe_cfg = config->num_ce_svc_pipe_cfg;
	cfg.ce_svc_cfg = (struct cnss_ce_svc_pipe_cfg *)
		config->ce_svc_cfg;
	cfg.num_shadow_reg_cfg = config->num_shadow_reg_cfg;
	cfg.shadow_reg_cfg = (struct cnss_shadow_reg_cfg *)
		config->shadow_reg_cfg;
	cfg.num_shadow_reg_v2_cfg = config->num_shadow_reg_v2_cfg;
	cfg.shadow_reg_v2_cfg = (struct cnss_shadow_reg_v2_cfg *)
		config->shadow_reg_v2_cfg;
	cfg.rri_over_ddr_cfg_valid = config->rri_over_ddr_cfg_valid;
	if (config->rri_over_ddr_cfg_valid) {
		cfg.rri_over_ddr_cfg.base_addr_low =
			 config->rri_over_ddr_cfg.base_addr_low;
		cfg.rri_over_ddr_cfg.base_addr_high =
			 config->rri_over_ddr_cfg.base_addr_high;
	}

	switch (mode) {
	case PLD_FTM:
		cnss_mode = CNSS_FTM;
		break;
	case PLD_EPPING:
		cnss_mode = CNSS_EPPING;
		break;
	default:
		cnss_mode = CNSS_MISSION;
		break;
	}
	return cnss_fw_sim_wlan_enable(dev, &cfg, cnss_mode, host_version);
}

/**
 * pld_pcie_fw_sim_wlan_disable() - Disable WLAN
 * @dev: device
 * @mode: WLAN mode
 *
 * This function disables WLAN FW. It passes WLAN mode to FW.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pcie_fw_sim_wlan_disable(struct device *dev, enum pld_driver_mode mode)
{
	return cnss_fw_sim_wlan_disable(dev, CNSS_OFF);
}

/**
 * pld_pcie_fw_sim_get_soc_info() - Get SOC information
 * @dev: device
 * @info: buffer to SOC information
 *
 * Return SOC info to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pcie_fw_sim_get_soc_info(struct device *dev, struct pld_soc_info *info)
{
	int ret = 0;
	struct cnss_soc_info cnss_info = {0};

	if (!info)
		return -ENODEV;

	ret = cnss_fw_sim_get_soc_info(dev, &cnss_info);
	if (ret)
		return ret;

	info->v_addr = cnss_info.va;
	info->p_addr = cnss_info.pa;
	info->chip_id = cnss_info.chip_id;
	info->chip_family = cnss_info.chip_family;
	info->board_id = cnss_info.board_id;
	info->soc_id = cnss_info.soc_id;
	info->fw_version = cnss_info.fw_version;
	strlcpy(info->fw_build_timestamp, cnss_info.fw_build_timestamp,
		sizeof(info->fw_build_timestamp));
	info->device_version.family_number =
		cnss_info.device_version.family_number;
	info->device_version.device_number =
		cnss_info.device_version.device_number;
	info->device_version.major_version =
		cnss_info.device_version.major_version;
	info->device_version.minor_version =
		cnss_info.device_version.minor_version;

	return 0;
}

/**
 * pld_pcie_fw_sim_get_platform_cap() - Get platform capabilities
 * @dev: device
 * @cap: buffer to the capabilities
 *
 * Return capabilities to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pcie_fw_sim_get_platform_cap(struct device *dev,
				     struct pld_platform_cap *cap)
{
	int ret = 0;
	struct cnss_platform_cap cnss_cap;

	if (!cap)
		return -ENODEV;

	ret = cnss_fw_sim_get_platform_cap(dev, &cnss_cap);
	if (ret)
		return ret;

	memcpy(cap, &cnss_cap, sizeof(*cap));
	return 0;
}

#endif
