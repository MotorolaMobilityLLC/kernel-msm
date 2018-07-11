/* Copyright (c) 2015 Cirrus Logic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef MSM_CIRRUS_PLAYBACK_H
#define MSM_CIRRUS_PLAYBACK_H

#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/apr_audio-v2.h>
#include <sound/q6afe-v2.h>
#include <sound/q6audio-v2.h>
#include <uapi/sound/msm-cirrus-playback.h>

struct afe_custom_crus_set_config_t {
	struct apr_hdr hdr;
	struct afe_port_cmd_set_param_v2 param;
	struct afe_port_param_data_v2 data;
} __packed;

struct afe_custom_crus_get_config_t {
	struct apr_hdr hdr;
	struct afe_port_cmd_get_param_v2 param;
	struct afe_port_param_data_v2 data;
} __packed;

/* Payload struct for getting or setting one integer value from/to the DSP
 * module
 */
struct crus_single_data_t {
	int32_t	value;
};

/* Payload struct for getting or setting two integer values from/to the DSP
 * module
 */
struct crus_dual_data_t {
	int32_t data1;
	int32_t data2;
};

/* Payload struct for getting or setting three integer values from/to the DSP
 * module
 */
struct crus_triple_data_t {
	int32_t data1;
	int32_t data2;
	int32_t data3;
};

/* Payload struct for setting the RX and TX use cases */
struct crus_rx_run_case_ctrl_t {
	int32_t value;
	int32_t status_l;
	int32_t checksum_l;
	int32_t z_l;
	int32_t status_r;
	int32_t checksum_r;
	int32_t z_r;
	int32_t atemp;
};

/* Payload struct for getting calibration result from DSP module */
struct cirrus_cal_result_t {
	int32_t status_l;
	int32_t checksum_l;
	int32_t z_l;
	int32_t status_r;
	int32_t checksum_r;
	int32_t z_r;
};

#define APR_CHUNK_SIZE		256
#define CONFIG_FILE_SIZE	128
#define PAYLOAD_FOLLOWS_CONFIG	4
#define MAX_TUNING_CONFIGS	4
#define MIN_CHAN_SWAP_SAMPLES	48
#define MAX_CHAN_SWAP_SAMPLES	9600

/* Payload struct for sending an external configuration string to the DSP
 * module
 */
struct crus_external_config_t {
	uint32_t total_size;
	uint32_t chunk_size;
	int32_t done;
	int32_t reserved;
	int32_t config;
	char data[APR_CHUNK_SIZE];
};

/* Payload struct for sending an external tuning transition string to the DSP
 * module
 */
struct crus_delta_config_t {
	uint32_t total_size;
	uint32_t chunk_size;
	int32_t done;
	int32_t index;
	int32_t reserved;
	int32_t config;
	char data[APR_CHUNK_SIZE];
};

extern int afe_apr_send_pkt_crus(void *data, int index, int set);
extern int crus_afe_callback(void *payload, int size);
void msm_crus_pb_add_controls(struct snd_soc_platform *platform);

#endif /* _MSM_CIRRUS_PLAYBACK_H */

