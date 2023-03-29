/*
 * Copyright (c) 2015-2020 The Linux Foundation. All rights reserved.
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

/* Include files */

#include <wlan_hdd_includes.h>
#include <cds_api.h>
#include <cds_sched.h>
#include <wni_api.h>
#include <wlan_hdd_cfg.h>
#include "wlan_hdd_trace.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_hdd_conc_ut.h"
#include "qdf_types.h"
#include "qdf_trace.h"
#include "cds_utils.h"
#include "wma_types.h"
#include "wma.h"
#include "wma_api.h"
#include "wlan_policy_mgr_ucfg.h"

#define NUMBER_OF_SCENARIO 300
#define MAX_ALLOWED_CHAR_IN_REPORT 50

/**
 * struct report_t: Data structure to fill report
 *
 * @title: title of the concurrency case scenario
 * @first_persona: device type of first persona
 * @second_persona: device type of second persona
 * @third_persona: device type of third persona
 * @dbs_value: string to mention whether dbs enable or disable
 * @system_conf: string to mention what is system's configuration
 * @status: status field
 * @result_code: string to mention whether test case passed or failed
 * @reason: reason why test case failed
 * @pcl: preferred channel list
 *
 * This structure will be used by unit test framework to fill
 * report after running various concurrency scenarios.
 */
struct report_t {
	char title[2 * MAX_ALLOWED_CHAR_IN_REPORT];
	char first_persona[MAX_ALLOWED_CHAR_IN_REPORT];
	char second_persona[MAX_ALLOWED_CHAR_IN_REPORT];
	char third_persona[MAX_ALLOWED_CHAR_IN_REPORT];
	char dbs_value[MAX_ALLOWED_CHAR_IN_REPORT];
	char system_conf[MAX_ALLOWED_CHAR_IN_REPORT];
	bool status;
	char result_code[MAX_ALLOWED_CHAR_IN_REPORT];
	char reason[MAX_ALLOWED_CHAR_IN_REPORT];
	char pcl[2 * NUM_CHANNELS];
};

static struct report_t report[NUMBER_OF_SCENARIO];
static uint32_t report_idx;

static uint8_t wlan_hdd_valid_type_of_persona(uint32_t sub_type)
{
	switch (sub_type) {
	case PM_STA_MODE:
		return WMI_VDEV_TYPE_STA;
	case PM_SAP_MODE:
	case PM_P2P_CLIENT_MODE:
	case PM_P2P_GO_MODE:
		return WMI_VDEV_TYPE_AP;
	default:
		return WMI_VDEV_TYPE_STA;
	}
}

static const char *system_config_to_string(uint8_t idx)
{
	switch (idx) {
	CASE_RETURN_STRING(PM_THROUGHPUT);
	CASE_RETURN_STRING(PM_POWERSAVE);
	CASE_RETURN_STRING(PM_LATENCY);
	default:
		return "Unknown";
	}

}

static const char *device_mode_to_string(uint8_t idx)
{
	switch (idx) {
	CASE_RETURN_STRING(PM_STA_MODE);
	CASE_RETURN_STRING(PM_SAP_MODE);
	CASE_RETURN_STRING(PM_P2P_CLIENT_MODE);
	CASE_RETURN_STRING(PM_P2P_GO_MODE);
	default:
		return "none";
	}
}

static const char *pcl_type_to_string(uint8_t idx)
{
	switch (idx) {
	CASE_RETURN_STRING(PM_NONE);
	CASE_RETURN_STRING(PM_24G);
	CASE_RETURN_STRING(PM_5G);
	CASE_RETURN_STRING(PM_SCC_CH);
	CASE_RETURN_STRING(PM_MCC_CH);
	CASE_RETURN_STRING(PM_SCC_CH_24G);
	CASE_RETURN_STRING(PM_SCC_CH_5G);
	CASE_RETURN_STRING(PM_24G_SCC_CH);
	CASE_RETURN_STRING(PM_5G_SCC_CH);
	CASE_RETURN_STRING(PM_SCC_ON_5_SCC_ON_24_24G);
	CASE_RETURN_STRING(PM_SCC_ON_5_SCC_ON_24_5G);
	CASE_RETURN_STRING(PM_SCC_ON_24_SCC_ON_5_24G);
	CASE_RETURN_STRING(PM_SCC_ON_24_SCC_ON_5_5G);
	CASE_RETURN_STRING(PM_SCC_ON_5_SCC_ON_24);
	CASE_RETURN_STRING(PM_SCC_ON_24_SCC_ON_5);
	CASE_RETURN_STRING(PM_MCC_CH_24G);
	CASE_RETURN_STRING(PM_MCC_CH_5G);
	CASE_RETURN_STRING(PM_24G_MCC_CH);
	CASE_RETURN_STRING(PM_5G_MCC_CH);
	default:
		return "Unknown";
	}
}

void clean_report(struct hdd_context *hdd_ctx)
{
	uint32_t idx = 0;

	while (idx < NUMBER_OF_SCENARIO) {
		qdf_mem_zero(&report[idx], sizeof(struct report_t));
		idx++;
	}
	report_idx = 0;
}

void print_report(struct hdd_context *hdd_ctx)
{
	uint32_t idx = 0;

	pr_info("+----------Report start -----------+\n");
	while (idx < report_idx) {
		pr_info("Idx:[%d]\nTitle:%s\nResult:[%s]\n\t1st_person[%s]\n\t2nd_persona[%s]\n\t3rd_persona[%s]\n\tDBS[%s]\n\tsystem_config[%s]\n\treason[%s]\n\tpcl[%s]\n",
			idx,
			report[idx].title, report[idx].result_code,
			report[idx].first_persona, report[idx].second_persona,
			report[idx].third_persona, report[idx].dbs_value,
			report[idx].system_conf, report[idx].reason,
			report[idx].pcl);
		idx++;
	}
	pr_info("+----------Report end -----------+\n");
}

void fill_report(struct hdd_context *hdd_ctx, char *title,
	uint32_t first_persona, uint32_t second_persona, uint32_t third_persona,
	uint32_t chnl_1st_conn, uint32_t chnl_2nd_conn, uint32_t chnl_3rd_conn,
	bool status, enum policy_mgr_pcl_type pcl_type, char *reason,
	uint8_t *pcl)
{
	int i;
	char buf[4] = {0};
	uint8_t sys_pref = 0;

	if (report_idx >= NUMBER_OF_SCENARIO)
		return;

	ucfg_policy_mgr_get_sys_pref(hdd_ctx->psoc, &sys_pref);

	snprintf(report[report_idx].title,
		2 * MAX_ALLOWED_CHAR_IN_REPORT, "pcl for[%s] pcl_type[%s]",
		title, pcl_type_to_string(pcl_type));
	if (chnl_1st_conn == 0)
		snprintf(report[report_idx].first_persona,
			MAX_ALLOWED_CHAR_IN_REPORT, "%s",
			device_mode_to_string(first_persona));
	else
		snprintf(report[report_idx].first_persona,
			MAX_ALLOWED_CHAR_IN_REPORT,
			"%s-chnl{%d}",
			device_mode_to_string(first_persona), chnl_1st_conn);
	if (chnl_2nd_conn == 0)
		snprintf(report[report_idx].second_persona,
			MAX_ALLOWED_CHAR_IN_REPORT, "%s",
			device_mode_to_string(second_persona));
	else
		snprintf(report[report_idx].second_persona,
			MAX_ALLOWED_CHAR_IN_REPORT,
			"%s-chnl{%d}",
			device_mode_to_string(second_persona), chnl_2nd_conn);
	if (chnl_3rd_conn == 0)
		snprintf(report[report_idx].third_persona,
			MAX_ALLOWED_CHAR_IN_REPORT, "%s",
			device_mode_to_string(third_persona));
	else
		snprintf(report[report_idx].third_persona,
			MAX_ALLOWED_CHAR_IN_REPORT,
			"%s-chnl{%d}",
			device_mode_to_string(third_persona), chnl_3rd_conn);

	report[report_idx].status = status;
	snprintf(report[report_idx].dbs_value,
		MAX_ALLOWED_CHAR_IN_REPORT, "%s",
		policy_mgr_is_hw_dbs_capable(hdd_ctx->psoc)
		? "enable" : "disable");
	snprintf(report[report_idx].system_conf,
		MAX_ALLOWED_CHAR_IN_REPORT, "%s",
		system_config_to_string(sys_pref));
	snprintf(report[report_idx].result_code,
		MAX_ALLOWED_CHAR_IN_REPORT, "%s",
		status ? "PASS" : "FAIL");
	snprintf(report[report_idx].reason,
		MAX_ALLOWED_CHAR_IN_REPORT,
		reason);
	if (pcl) {
		qdf_mem_zero(report[report_idx].pcl,
				sizeof(report[report_idx].pcl));
		for (i = 0; i < NUM_CHANNELS; i++) {
			if (pcl[i] == 0)
				break;
			qdf_mem_zero(buf, sizeof(buf));
			snprintf(buf, sizeof(buf), "%d ", pcl[i]);
			strlcat(report[report_idx].pcl, buf,
				sizeof(report[report_idx].pcl));
			strlcat(report[report_idx].pcl, ", ",
				sizeof(report[report_idx].pcl));
		}
	}
	report_idx++;
}

static bool wlan_hdd_validate_pcl(struct hdd_context *hdd_ctx,
	enum policy_mgr_pcl_type pcl_type, uint8_t *pcl, uint32_t pcl_len,
	uint8_t first_connection_chnl, uint8_t second_connection_chnl,
	char *reason, uint32_t reason_length)
{
	bool status = true;
	uint32_t first_idx = 0;

	if ((pcl_type != PM_NONE) && (pcl_len == 0)) {
		snprintf(reason, reason_length, "no of channels = 0");
		return false;
	}

	switch (pcl_type) {
	case PM_NONE:
		if (pcl_len != 0) {
			snprintf(reason, reason_length, "no of channels>0");
			return false;
		}
		break;
	case PM_5G:
		for (first_idx = 0; first_idx < pcl_len; first_idx++) {
			if (!WLAN_REG_IS_5GHZ_CH(pcl[first_idx])) {
				snprintf(reason, reason_length,
					"2G channel found");
				return false;
			}
		}
		break;
	case PM_24G:
		for (first_idx = 0; first_idx < pcl_len; first_idx++) {
			if (!WLAN_REG_IS_24GHZ_CH(pcl[first_idx])) {
				snprintf(reason, reason_length,
					"5G channel found");
				return false;
			}
		}
		break;
	case PM_SCC_CH:
		if (second_connection_chnl > 0 &&
			(first_connection_chnl != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"invalid connections");
			return false;
		}
		if (pcl[0] != first_connection_chnl) {
			snprintf(reason, reason_length,
				"No SCC found");
			return false;
		}
		break;
	case PM_MCC_CH:
		if ((pcl[0] != first_connection_chnl) &&
				((second_connection_chnl > 0) &&
				 (pcl[0] != second_connection_chnl))) {
			snprintf(reason, reason_length,
				"MCC invalid");
			return false;
		}
		if ((second_connection_chnl > 0) &&
				(pcl[1] != first_connection_chnl &&
				 pcl[1] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"MCC invalid");
			return false;
		}
		break;
	case PM_SCC_CH_24G:
		if (second_connection_chnl > 0 &&
			(first_connection_chnl != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"invalid connections");
			return false;
		}
		if (pcl[0] != first_connection_chnl) {
			snprintf(reason, reason_length,
				"No SCC found");
			return false;
		}
		if (!WLAN_REG_IS_24GHZ_CH(pcl[pcl_len - 1])) {
			snprintf(reason, reason_length,
				"No 2.4Ghz chnl");
			return false;
		}
		break;
	case PM_SCC_CH_5G:
		if (second_connection_chnl > 0 &&
			(first_connection_chnl != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"invalid connections");
			return false;
		}
		if (pcl[0] != first_connection_chnl) {
			snprintf(reason, reason_length,
				"No SCC found");
			return false;
		}
		if (!WLAN_REG_IS_5GHZ_CH(pcl[pcl_len - 1])) {
			snprintf(reason, reason_length,
				"No 5Ghz chnl");
			return false;
		}
		break;
	case PM_24G_SCC_CH:
		if (!WLAN_REG_IS_24GHZ_CH(pcl[0])) {
			snprintf(reason, reason_length,
				"No 2.4Ghz chnl");
			return false;
		}
		if (second_connection_chnl > 0 &&
			(first_connection_chnl != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"invalid connections");
			return false;
		}
		if (pcl[pcl_len-1] != first_connection_chnl) {
			snprintf(reason, reason_length,
				"No SCC found");
			return false;
		}
		break;
	case PM_5G_SCC_CH:
		if (!WLAN_REG_IS_5GHZ_CH(pcl[0])) {
			snprintf(reason, reason_length,
				"No 5Ghz chnl");
			return false;
		}
		if (second_connection_chnl > 0 &&
			(first_connection_chnl != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"invalid connections");
			return false;
		}
		if (pcl[pcl_len-1] != first_connection_chnl) {
			snprintf(reason, reason_length,
				"No SCC found");
			return false;
		}
		break;
	case PM_MCC_CH_24G:
		if ((pcl[0] != first_connection_chnl) &&
			((second_connection_chnl > 0) &&
			 (pcl[0] != second_connection_chnl))) {
			snprintf(reason, reason_length,
				"MCC invalid");
			return false;
		}
		if ((second_connection_chnl > 0) &&
			(pcl[1] != first_connection_chnl &&
			 pcl[1] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"MCC invalid");
			return false;
		}
		if (!WLAN_REG_IS_24GHZ_CH(pcl[pcl_len - 1])) {
			snprintf(reason, reason_length,
				"No 24Ghz chnl");
			return false;
		}
		break;
	case PM_MCC_CH_5G:
		if ((pcl[0] != first_connection_chnl) &&
			((second_connection_chnl > 0) &&
			 (pcl[0] != second_connection_chnl))) {
			snprintf(reason, reason_length,
				"MCC invalid");
			return false;
		}
		if ((second_connection_chnl > 0) &&
			(pcl[1] != first_connection_chnl &&
			 pcl[1] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"MCC invalid");
			return false;
		}
		if (!WLAN_REG_IS_5GHZ_CH(pcl[pcl_len - 1])) {
			snprintf(reason, reason_length,
				"No 5Ghz chnl");
			return false;
		}
		break;
	case PM_24G_MCC_CH:
		if (!WLAN_REG_IS_24GHZ_CH(pcl[0])) {
			snprintf(reason, reason_length,
				"No 24Ghz chnl");
			return false;
		}
		if ((pcl[pcl_len-1] != first_connection_chnl) &&
			((second_connection_chnl > 0) &&
			 (pcl[pcl_len-1] != second_connection_chnl))) {
			snprintf(reason, reason_length,
				"MCC invalid");
			return false;
		}
		if ((second_connection_chnl > 0) &&
			(pcl[pcl_len-2] != first_connection_chnl &&
			 pcl[pcl_len-2] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"MCC invalid");
			return false;
		}
		break;
	case PM_5G_MCC_CH:
		if (!WLAN_REG_IS_5GHZ_CH(pcl[0])) {
			snprintf(reason, reason_length,
				"No 5Ghz chnl");
			return false;
		}
		if ((pcl[pcl_len-1] != first_connection_chnl) &&
			((second_connection_chnl > 0) &&
			 (pcl[pcl_len-1] != second_connection_chnl))) {
			snprintf(reason, reason_length,
				"MCC invalid");
			return false;
		}
		if ((second_connection_chnl > 0) &&
			(pcl[pcl_len-2] != first_connection_chnl &&
			 pcl[pcl_len-2] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"MCC invalid");
			return false;
		}
		break;
	case PM_SCC_ON_5_SCC_ON_24_24G:
		if (!WLAN_REG_IS_5GHZ_CH(pcl[0]) ||
			(pcl[0] != first_connection_chnl &&
			 pcl[0] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"No 5Ghz chnl/scc");
			return false;
		}
		if (!WLAN_REG_IS_24GHZ_CH(pcl[1]) ||
			(pcl[1] != first_connection_chnl &&
			 pcl[1] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"No 24Ghz chnl/scc");
			return false;
		}
		if (!WLAN_REG_IS_24GHZ_CH(pcl[pcl_len - 1])) {
			snprintf(reason, reason_length,
				"No 24Ghz chnls");
			return false;
		}
		break;
	case PM_SCC_ON_5_SCC_ON_24_5G:
		if (!WLAN_REG_IS_5GHZ_CH(pcl[0]) ||
			(pcl[0] != first_connection_chnl &&
			 pcl[0] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"No 5Ghz chnl/scc");
			return false;
		}
		if (!WLAN_REG_IS_24GHZ_CH(pcl[1]) ||
			(pcl[1] != first_connection_chnl &&
			 pcl[1] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"No 24Ghz chnl/scc");
			return false;
		}
		if (!WLAN_REG_IS_5GHZ_CH(pcl[pcl_len - 1])) {
			snprintf(reason, reason_length,
				"No 5Ghz chnls");
			return false;
		}
		break;
	case PM_SCC_ON_24_SCC_ON_5_24G:
		if (!WLAN_REG_IS_24GHZ_CH(pcl[0]) ||
			(pcl[0] != first_connection_chnl &&
			 pcl[0] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"No 24Ghz chnl/scc");
			return false;
		}
		if (!WLAN_REG_IS_5GHZ_CH(pcl[1]) ||
			(pcl[1] != first_connection_chnl &&
			 pcl[1] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"No 5Ghz chnl/scc");
			return false;
		}
		if (!WLAN_REG_IS_24GHZ_CH(pcl[pcl_len - 1])) {
			snprintf(reason, reason_length,
				"No 24Ghz chnls");
			return false;
		}
		break;
	case PM_SCC_ON_24_SCC_ON_5_5G:
		if (!WLAN_REG_IS_24GHZ_CH(pcl[0]) ||
			(pcl[0] != first_connection_chnl &&
			 pcl[0] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"No 24Ghz chnl/scc");
			return false;
		}
		if (!WLAN_REG_IS_5GHZ_CH(pcl[1]) ||
			(pcl[1] != first_connection_chnl &&
			 pcl[1] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"No 5Ghz chnl/scc");
			return false;
		}
		if (!WLAN_REG_IS_5GHZ_CH(pcl[pcl_len - 1])) {
			snprintf(reason, reason_length,
				"No 5Ghz chnls");
			return false;
		}
		break;
	case PM_SCC_ON_5_SCC_ON_24:
		if (!WLAN_REG_IS_5GHZ_CH(pcl[0]) ||
			(pcl[0] != first_connection_chnl &&
			 pcl[0] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"No 5Ghz chnl/scc");
			return false;
		}
		if (!WLAN_REG_IS_24GHZ_CH(pcl[1]) ||
			(pcl[1] != first_connection_chnl &&
			 pcl[1] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"No 24Ghz chnl/scc");
			return false;
		}
		if (pcl_len != 2) {
			snprintf(reason, reason_length,
				"more than 2 chnls");
			return false;
		}
		break;
	case PM_SCC_ON_24_SCC_ON_5:
		if (!WLAN_REG_IS_24GHZ_CH(pcl[0]) ||
			(pcl[0] != first_connection_chnl &&
			 pcl[0] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"No 24Ghz chnl/scc");
			return false;
		}
		if (!WLAN_REG_IS_5GHZ_CH(pcl[1]) ||
			(pcl[1] != first_connection_chnl &&
			 pcl[1] != second_connection_chnl)) {
			snprintf(reason, reason_length,
				"No 5Ghz chnl/scc");
			return false;
		}
		if (pcl_len != 2) {
			snprintf(reason, reason_length,
				"more than 2 chnls");
			return false;
		}
		break;
	default:
		snprintf(reason, reason_length,
			"Unknown option");
		status = false;
	}
	if (status == true) {
		snprintf(reason, reason_length,
			"success");
	}
	return status;
}

static void wlan_hdd_map_subtypes_hdd_wma(enum policy_mgr_con_mode *dst,
		enum policy_mgr_con_mode *src)
{
	/*
	 * wma defined sap subtype as 0
	 * Rest of the mappings are same
	 * In future, if mapping gets changed then re-map it here
	 */
	if (*src == PM_SAP_MODE)
		*dst = 0;
	else
		*dst = *src;
}

void wlan_hdd_one_connection_scenario(struct hdd_context *hdd_ctx)
{
	enum policy_mgr_con_mode sub_type;
	uint8_t pcl[NUM_CHANNELS] = {0},
		weight_list[NUM_CHANNELS] = {0};
	uint32_t pcl_len = 0, i, pcl_freqs[NUM_CHANNELS] = {0};
	bool status = false;
	enum policy_mgr_pcl_type pcl_type;
	char reason[20] = {0};
	QDF_STATUS ret;
	struct policy_mgr_sme_cbacks sme_cbacks;
	uint8_t system_pref = 0;

	ucfg_policy_mgr_get_sys_pref(hdd_ctx->psoc, &system_pref);

	sme_cbacks.sme_get_nss_for_vdev = sme_get_vdev_type_nss;
	/* flush the entire table first */
	ret = policy_mgr_psoc_enable(hdd_ctx->psoc);
	if (!QDF_IS_STATUS_SUCCESS(ret)) {
		hdd_err("Policy manager initialization failed");
		return;
	}

	for (sub_type = 0; sub_type < PM_MAX_NUM_OF_MODE; sub_type++) {
		/* validate one connection is created or no */
		if (policy_mgr_get_connection_count(hdd_ctx->psoc) != 0) {
			hdd_err("Test failed - No. of connection is not 0");
			return;
		}
		qdf_mem_zero(pcl, sizeof(pcl));
		pcl_len = 0;
		pcl_type = policy_mgr_get_pcl_from_first_conn_table(
			sub_type, system_pref);

		/* check PCL value for second connection is correct or no */
		policy_mgr_get_pcl(hdd_ctx->psoc, sub_type, pcl_freqs, &pcl_len,
				   weight_list, QDF_ARRAY_SIZE(weight_list));
		for (i = 0; i < pcl_len; i++)
			pcl[i] = wlan_freq_to_chan(pcl_freqs[i]);

		status = wlan_hdd_validate_pcl(hdd_ctx,
				pcl_type, pcl, pcl_len, 0, 0,
				reason, sizeof(reason));
		if ((pcl_type == PM_MAX_PCL_TYPE) && (pcl[0] == 0))
			continue;

		fill_report(hdd_ctx, "1 connection", sub_type,
				PM_MAX_NUM_OF_MODE,
				PM_MAX_NUM_OF_MODE,
				0, 0, 0,
				status, pcl_type, reason, pcl);
	}
}

void wlan_hdd_two_connections_scenario(struct hdd_context *hdd_ctx,
		uint8_t first_chnl, enum policy_mgr_chain_mode first_chain_mask)
{
	uint8_t vdevid = 0, tx_stream = 2, rx_stream = 2;
	uint8_t type = WMI_VDEV_TYPE_STA, channel_id = first_chnl, mac_id = 1;
	uint8_t pcl[NUM_CHANNELS] = {0},
			weight_list[NUM_CHANNELS] = {0};
	uint32_t pcl_len = 0, i, pcl_freqs[NUM_CHANNELS];
	enum policy_mgr_chain_mode chain_mask = first_chain_mask;
	enum policy_mgr_con_mode sub_type, next_sub_type, dummy_type;
	enum policy_mgr_pcl_type pcl_type;
	enum policy_mgr_one_connection_mode second_index;
	char reason[20] = {0};
	bool status = false;
	QDF_STATUS ret;
	struct policy_mgr_sme_cbacks sme_cbacks;
	uint8_t system_pref = 0;

	ucfg_policy_mgr_get_sys_pref(hdd_ctx->psoc, &system_pref);

	for (sub_type = PM_STA_MODE;
		sub_type < PM_MAX_NUM_OF_MODE; sub_type++) {
		type = wlan_hdd_valid_type_of_persona(sub_type);

		sme_cbacks.sme_get_nss_for_vdev = sme_get_vdev_type_nss;
		/* flush the entire table first */
		ret = policy_mgr_psoc_enable(hdd_ctx->psoc);
		if (!QDF_IS_STATUS_SUCCESS(ret)) {
			hdd_err("Policy manager initialization failed");
			return;
		}

		/* sub_type mapping between HDD and WMA are different */
		wlan_hdd_map_subtypes_hdd_wma(&dummy_type, &sub_type);
		/* add first connection as STA */
		policy_mgr_incr_connection_count_utfw(
			hdd_ctx->psoc, vdevid, tx_stream,
			rx_stream, chain_mask, type, dummy_type,
			wlan_chan_to_freq(channel_id), mac_id);
		/* validate one connection is created or no */
		if (policy_mgr_get_connection_count(hdd_ctx->psoc) != 1) {
			hdd_err("Test failed - No. of connection is not 1");
			return;
		}
		next_sub_type = PM_STA_MODE;
		while (next_sub_type < PM_MAX_NUM_OF_MODE) {
			/* get the PCL value & check the channels accordingly */
			second_index =
			policy_mgr_get_second_connection_pcl_table_index(
				hdd_ctx->psoc);
			if (PM_MAX_ONE_CONNECTION_MODE == second_index) {
				/* not valid combination*/
				hdd_err("couldn't find index for 2nd connection pcl table");
				next_sub_type++;
				continue;
			}
			qdf_mem_zero(pcl, sizeof(pcl));
			pcl_len = 0;
			pcl_type = policy_mgr_get_pcl_from_second_conn_table(
				second_index, next_sub_type, system_pref,
				policy_mgr_is_hw_dbs_capable(
					hdd_ctx->psoc));
			/* check PCL for second connection is correct or no */
			policy_mgr_get_pcl(hdd_ctx->psoc,
					   next_sub_type, pcl_freqs, &pcl_len,
					   weight_list,
					   QDF_ARRAY_SIZE(weight_list));
			for (i = 0; i < pcl_len; i++)
				pcl[i] = wlan_freq_to_chan(pcl_freqs[i]);
			status = wlan_hdd_validate_pcl(hdd_ctx,
					pcl_type, pcl, pcl_len, channel_id, 0,
					reason, sizeof(reason));
			if ((pcl_type == PM_MAX_PCL_TYPE) && (pcl[0] == 0)) {
				next_sub_type++;
				continue;
			}
			fill_report(hdd_ctx, "2 connections", sub_type,
					next_sub_type,
					PM_MAX_NUM_OF_MODE, first_chnl,
					0, 0, status, pcl_type, reason, pcl);
			next_sub_type++;
		}
	}
}

void wlan_hdd_three_connections_scenario(struct hdd_context *hdd_ctx,
		uint8_t first_chnl, uint8_t second_chnl,
		enum policy_mgr_chain_mode chain_mask, uint8_t use_same_mac)
{
	uint8_t vdevid_1 = 0, tx_stream_1 = 2, rx_stream_1 = 2;
	uint8_t vdevid_2 = 1, tx_stream_2 = 2, rx_stream_2 = 2;
	uint8_t channel_id_1 = first_chnl, channel_id_2 = second_chnl;
	uint8_t mac_id_1, mac_id_2;
	uint8_t type_1 = WMI_VDEV_TYPE_STA, type_2 = WMI_VDEV_TYPE_STA;
	uint8_t pcl[NUM_CHANNELS] = {0}, weight_list[NUM_CHANNELS] = {0};
	uint32_t pcl_len = 0, i, pcl_freqs[NUM_CHANNELS];
	enum policy_mgr_chain_mode chain_mask_1;
	enum policy_mgr_chain_mode chain_mask_2;
	enum policy_mgr_con_mode sub_type_1, sub_type_2, next_sub_type;
	enum policy_mgr_con_mode dummy_type_1, dummy_type_2;
	enum policy_mgr_pcl_type pcl_type;
	enum policy_mgr_two_connection_mode third_index;
	char reason[20] = {0};
	bool status = false;
	QDF_STATUS ret;
	struct policy_mgr_sme_cbacks sme_cbacks;
	uint8_t system_pref = 0;

	ucfg_policy_mgr_get_sys_pref(hdd_ctx->psoc, &system_pref);

	/* let's set the chain_mask, mac_ids*/
	if (chain_mask == POLICY_MGR_TWO_TWO) {
		mac_id_1 = 1;
		mac_id_2 = 1;
		chain_mask_1 = POLICY_MGR_TWO_TWO;
		chain_mask_2 = POLICY_MGR_TWO_TWO;
	} else if (use_same_mac == 1) {
		mac_id_1 = 1;
		mac_id_2 = 1;
		chain_mask_1 = POLICY_MGR_ONE_ONE;
		chain_mask_2 = POLICY_MGR_ONE_ONE;
	} else {
		mac_id_1 = 1;
		mac_id_2 = 2;
		chain_mask_1 = POLICY_MGR_ONE_ONE;
		chain_mask_2 = POLICY_MGR_ONE_ONE;
	}

	for (sub_type_1 = PM_STA_MODE;
		sub_type_1 < PM_MAX_NUM_OF_MODE; sub_type_1++) {

		type_1 = wlan_hdd_valid_type_of_persona(sub_type_1);

		sme_cbacks.sme_get_nss_for_vdev = sme_get_vdev_type_nss;
		/* flush the entire table first */
		ret = policy_mgr_psoc_enable(hdd_ctx->psoc);
		if (!QDF_IS_STATUS_SUCCESS(ret)) {
			hdd_err("Policy manager initialization failed");
			return;
		}

		/* sub_type mapping between HDD and WMA are different */
		wlan_hdd_map_subtypes_hdd_wma(&dummy_type_1, &sub_type_1);
		/* add first connection as STA */
		policy_mgr_incr_connection_count_utfw(
			hdd_ctx->psoc, vdevid_1, tx_stream_1, rx_stream_1,
			chain_mask_1, type_1, dummy_type_1,
			wlan_chan_to_freq(channel_id_1), mac_id_1);
		/* validate one connection is created or no */
		if (policy_mgr_get_connection_count(hdd_ctx->psoc) != 1) {
			hdd_err("Test fail - No. of connection not 1");
			return;
		}
		for (sub_type_2 = PM_STA_MODE;
			sub_type_2 < PM_MAX_NUM_OF_MODE; sub_type_2++) {

			type_2 = wlan_hdd_valid_type_of_persona(sub_type_2);
			/* sub_type mapping between HDD and WMA are different */
			wlan_hdd_map_subtypes_hdd_wma(&dummy_type_2,
					&sub_type_2);
			policy_mgr_incr_connection_count_utfw(
				hdd_ctx->psoc, vdevid_2, tx_stream_2,
				rx_stream_2, chain_mask_2, type_2,
				dummy_type_2,
				wlan_chan_to_freq(channel_id_2), mac_id_2);
			/* validate two connections are created or no */
			if (policy_mgr_get_connection_count(hdd_ctx->psoc)
				!= 2) {
				hdd_err("Test fail - No. connection not 2");
				return;
			}
			next_sub_type = PM_STA_MODE;
			while (next_sub_type < PM_MAX_NUM_OF_MODE) {
				third_index =
				policy_mgr_get_third_connection_pcl_table_index(
						hdd_ctx->psoc);
				if (PM_MAX_TWO_CONNECTION_MODE ==
						third_index) {
					/* not valid combination */
					next_sub_type++;
					continue;
				}
				qdf_mem_zero(pcl, sizeof(pcl));
				pcl_len = 0;
				pcl_type =
				policy_mgr_get_pcl_from_third_conn_table(
					third_index, next_sub_type,
					system_pref,
					policy_mgr_is_hw_dbs_capable(
					hdd_ctx->psoc));
				policy_mgr_get_pcl(
					hdd_ctx->psoc, next_sub_type,
					pcl_freqs, &pcl_len, weight_list,
					QDF_ARRAY_SIZE(weight_list));
				for (i = 0; i < pcl_len; i++)
					pcl[i] =
						wlan_freq_to_chan(pcl_freqs[i]);
				status = wlan_hdd_validate_pcl(hdd_ctx,
					pcl_type, pcl, pcl_len,
					channel_id_1, channel_id_2,
					reason, sizeof(reason));
				if ((pcl_type == PM_MAX_PCL_TYPE) &&
					(pcl[0] == 0)) {
					next_sub_type++;
					continue;
				}
				fill_report(hdd_ctx, "3 connections",
					sub_type_1, sub_type_2,
					next_sub_type, first_chnl,
					second_chnl, 0, status,
					pcl_type, reason, pcl);
				next_sub_type++;
			}
			/* remove entry to make a room for next iteration */
			policy_mgr_decr_connection_count(hdd_ctx->psoc,
				vdevid_2);
		}
		next_sub_type = PM_STA_MODE;
	}
}
