/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#ifndef __Q6_ADM_V2_H__
#define __Q6_ADM_V2_H__


#define ADM_PATH_PLAYBACK 0x1
#define ADM_PATH_LIVE_REC 0x2
#define ADM_PATH_NONLIVE_REC 0x3
#define ADM_PATH_COMPRESSED_RX 0x5
#include <linux/qdsp6v2/rtac.h>
#include <sound/q6afe-v2.h>
#include <sound/q6audio-v2.h>

#define MAX_MODULES_IN_TOPO 16
#define ADM_GET_TOPO_MODULE_LIST_LENGTH\
		((MAX_MODULES_IN_TOPO + 1) * sizeof(uint32_t))
#define AUD_PROC_BLOCK_SIZE	4096
#define AUD_VOL_BLOCK_SIZE	4096
#define AUDIO_RX_CALIBRATION_SIZE	(AUD_PROC_BLOCK_SIZE + \
						AUD_VOL_BLOCK_SIZE)
enum {
	ADM_RX_AUDPROC_CAL,
	ADM_TX_AUDPROC_CAL,
	ADM_RX_AUDVOL_CAL,
	ADM_TX_AUDVOL_CAL,
	ADM_CUSTOM_TOP_CAL,
	ADM_RTAC,
	ADM_MAX_CAL_TYPES,
};

#define ADM_MAX_CHANNELS 8
/* multiple copp per stream. */
struct route_payload {
	unsigned int copp_ids[AFE_MAX_PORTS];
	unsigned short num_copps;
	unsigned int session_id;
};

int srs_trumedia_open(int port_id, int srs_tech_id, void *srs_params);

int adm_open(int port, int path, int rate, int mode, int topology,
				int perf_mode, uint16_t bits_per_sample);

int adm_get_params(int port_id, uint32_t module_id, uint32_t param_id,
			uint32_t params_length, char *params);

int adm_dolby_dap_send_params(int port_id, char *params,
				 uint32_t params_length);

int adm_multi_ch_copp_open(int port, int path, int rate, int mode,
			int topology, int perf_mode, uint16_t bits_per_sample);

int adm_unmap_cal_blocks(void);

int adm_map_rtac_block(struct rtac_cal_block_data *cal_block);

int adm_unmap_rtac_block(uint32_t *mem_map_handle);

int adm_memory_map_regions(int port_id, phys_addr_t *buf_add,
	uint32_t mempool_id, uint32_t *bufsz, uint32_t bufcnt);

int adm_memory_unmap_regions(int port_id);

int adm_close(int port, int perf_mode);

int adm_matrix_map(int session_id, int path, int num_copps,
		unsigned int *port_id, int copp_id, int perf_mode);

int adm_connect_afe_port(int mode, int session_id, int port_id);

void adm_ec_ref_rx_id(int  port_id);

int adm_get_copp_id(int port_id);

int adm_get_lowlatency_copp_id(int port_id);

void adm_set_multi_ch_map(char *channel_map);

void adm_get_multi_ch_map(char *channel_map);

int adm_set_stereo_to_custom_stereo(int port_id, unsigned int session_id,
				    char *params, uint32_t params_length);

int adm_get_pp_topo_module_list(int port_id, int32_t param_length,
				char *params);

int adm_set_volume(int port_id, int volume);

int adm_set_softvolume(int port_id,
		       struct audproc_softvolume_params *softvol_param);

int adm_param_enable(int port_id, int module_id,  int enable);

int adm_send_calibration(int port_id, int path, int perf_mode, int cal_type,
			 char *params, int size);

int adm_set_wait_parameters(int port_id);

int adm_reset_wait_parameters(int port_id);

int adm_wait_timeout(int port_id, int wait_time);

int adm_store_cal_data(int port_id, int path, int perf_mode, int cal_type,
		       char *params, int *size);

int adm_get_topology_id(int port_id);

int adm_send_compressed_device_mute(int port_id, bool mute_on);

int adm_send_compressed_device_latency(int port_id, int latency);
#endif /* __Q6_ADM_V2_H__ */
