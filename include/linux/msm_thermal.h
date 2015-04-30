/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_THERMAL_H
#define __MSM_THERMAL_H

#define TSENS_NAME_MAX 20
#define HOTPLUG_DEVICE "hotplug"
#define CPU0_DEVICE     "cpu0"
#define CPU1_DEVICE     "cpu1"
#define CPU2_DEVICE     "cpu2"
#define CPU3_DEVICE     "cpu3"
#define CPU4_DEVICE     "cpu4"
#define CPU5_DEVICE     "cpu5"
#define CPU6_DEVICE     "cpu6"
#define CPU7_DEVICE     "cpu7"
#define CPUFREQ_MAX_NO_MITIGATION     UINT_MAX
#define CPUFREQ_MIN_NO_MITIGATION     0
#define HOTPLUG_NO_MITIGATION(_mask)  cpumask_clear(_mask)

struct msm_thermal_data {
	struct platform_device *pdev;
	uint32_t sensor_id;
	uint32_t poll_ms;
	int32_t limit_temp_degC;
	int32_t temp_hysteresis_degC;
	uint32_t bootup_freq_step;
	uint32_t bootup_freq_control_mask;
	int32_t core_limit_temp_degC;
	int32_t core_temp_hysteresis_degC;
	int32_t hotplug_temp_degC;
	int32_t hotplug_temp_hysteresis_degC;
	uint32_t core_control_mask;
	uint32_t freq_mitig_temp_degc;
	uint32_t freq_mitig_temp_hysteresis_degc;
	uint32_t freq_mitig_control_mask;
	uint32_t freq_limit;
	int32_t vdd_rstr_temp_degC;
	int32_t vdd_rstr_temp_hyst_degC;
	int32_t vdd_mx_min;
	int32_t psm_temp_degC;
	int32_t psm_temp_hyst_degC;
	int32_t ocr_temp_degC;
	int32_t ocr_temp_hyst_degC;
	uint32_t ocr_sensor_id;
	int32_t phase_rpm_resource_type;
	int32_t phase_rpm_resource_id;
	int32_t gfx_phase_warm_temp_degC;
	int32_t gfx_phase_warm_temp_hyst_degC;
	int32_t gfx_phase_hot_temp_degC;
	int32_t gfx_phase_hot_temp_hyst_degC;
	int32_t gfx_sensor;
	int32_t gfx_phase_request_key;
	int32_t cx_phase_hot_temp_degC;
	int32_t cx_phase_hot_temp_hyst_degC;
	int32_t cx_phase_request_key;
	int32_t vdd_mx_temp_degC;
	int32_t vdd_mx_temp_hyst_degC;
	int32_t therm_reset_temp_degC;
};

enum device_req_type {
	DEVICE_REQ_NONE = -1,
	HOTPLUG_MITIGATION_REQ,
	CPUFREQ_MITIGATION_REQ,
	DEVICE_REQ_MAX,
};

/**
 * For frequency mitigation request, if client is interested
 * only in one, either max_freq or min_freq, update default
 * value for other one also for mitigation request.
 * Default value for request structure variables:
 *   max_freq = UINT_MAX;
 *   min_freq = 0;
 *   offline_mask =  CPU_MASK_NONE;
 */
struct cpufreq_request {
	uint32_t                     max_freq;
	uint32_t                     min_freq;
};

union device_request {
	struct cpufreq_request       freq;
	cpumask_t                    offline_mask;
};

struct device_clnt_data;
struct device_manager_data {
	char                         device_name[TSENS_NAME_MAX];
	union device_request         active_req;
	struct list_head             client_list;
	struct list_head             dev_ptr;
	struct mutex                 clnt_lock;
	int (*request_validate)(struct device_clnt_data *,
			union device_request *,
			enum device_req_type);
	int (*update)(struct device_manager_data *);
	void                         *data;
};

struct device_clnt_data {
	struct device_manager_data   *dev_mgr;
	bool                         req_active;
	union device_request         request;
	struct list_head             clnt_ptr;
	void (*callback)(struct device_clnt_data *,
			union device_request *req, void *);
	void                         *usr_data;
};

#ifdef CONFIG_THERMAL_MONITOR
extern int msm_thermal_init(struct msm_thermal_data *pdata);
extern int msm_thermal_device_init(void);
extern int msm_thermal_set_frequency(uint32_t cpu, uint32_t freq,
	bool is_max);
extern int msm_thermal_set_cluster_freq(uint32_t cluster, uint32_t freq,
	bool is_max);
extern int msm_thermal_get_freq_plan_size(uint32_t cluster,
	unsigned int *table_len);
extern int msm_thermal_get_cluster_freq_plan(uint32_t cluster,
	unsigned int *table_ptr);
/**
 * devmgr_register_mitigation_client - Register for a device and
 *                                     gets a handle for mitigation.
 * @dev: Client device structure.
 * @device_name: Mitgation device name which the client is interested
 *               to mitigate.
 * @callback: Optional callback pointer for device change notification,
 *            otherwise pass NULL.
 *
 * Returns client handle structure for that device on success, or NULL
 * with IS_ERR() condition containing error number.
 */
extern struct device_clnt_data *devmgr_register_mitigation_client(
				struct device *dev,
				const char *device_name,
				void (*callback)(struct device_clnt_data *,
				union device_request *, void *));
/**
 * devmgr_client_request_mitigation -  Set a valid mitigation for
 *                                     registered device.
 * @clnt: Client handle for device.
 * @type: Type of device request populated above.
 * @req:  Valid mitigation request.
 *
 * Returns zero on successful mitigation update, or negative error number.
 */
extern int devmgr_client_request_mitigation(struct device_clnt_data *clnt,
						enum device_req_type type,
						union device_request *req);
/**
 * devmgr_unregister_mitigation_client - Unregister mitigation device
 * @dev: Client device structure.
 * @clnt: Client handle for device.
 */
extern void devmgr_unregister_mitigation_client(
					struct device *dev,
					struct device_clnt_data *clnt);
#else
static inline int msm_thermal_init(struct msm_thermal_data *pdata)
{
	return -ENOSYS;
}
static inline int msm_thermal_device_init(void)
{
	return -ENOSYS;
}
static inline int msm_thermal_set_frequency(uint32_t cpu, uint32_t freq,
	bool is_max)
{
	return -ENOSYS;
}
static inline int msm_thermal_set_cluster_freq(uint32_t cluster, uint32_t freq,
	bool is_max);
{
	return -ENOSYS;
}
static inline int msm_thermal_get_freq_plan_size(uint32_t cluster,
	unsigned int *table_len);
{
	return -ENOSYS;
}
static inline int msm_thermal_get_cluster_freq_plan(uint32_t cluster,
	unsigned int *table_ptr);
{
	return -ENOSYS;
}
static inline struct device_clnt_data *devmgr_register_mitigation_client(
				struct device *dev,
				const char *device_name,
				void (*callback)(struct device_clnt_data *,
				union device_request *, void *))
{
	return NULL;
}
static inline int devmgr_client_request_mitigation(
					struct device_clnt_data *clnt,
					enum device_req_type type,
					union device_request *req)
{
	return -ENOSYS;
}
static inline void devmgr_unregister_mitigation_client(
					struct device *dev,
					struct device_clnt_data *clnt)
{
}
#endif

#endif /*__MSM_THERMAL_H*/
