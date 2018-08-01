/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 * Based on TDD v7.0 implemented by Mstar & ILITEK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef __MP_TEST_H
#define __MP_TEST_H

#define MP_PASS      1
#define MP_FAIL     -1
#define BENCHMARK 1
#define NODETYPE	 	1
#define RAWDATA_NO_BK_DATA_SHIFT_9881H 8192
#define RAWDATA_NO_BK_DATA_SHIFT_9881F 4096
#define TYPE_BENCHMARK 0
#define TYPE_NO_JUGE 1
#define TYPE_JUGE 2

#define EXEC_READ  0
#define EXEC_WRITE 1

#define INT_CHECK 0
#define POLL_CHECK 1
#define DELAY_CHECK 2
#define ISR_CHECK 3
#define RETRY_COUNT 0

#define NORMAL_CSV_PASS_NAME		"mp_pass"
#define NORMAL_CSV_FAIL_NAME		"mp_fail"
#define OPPO_CSV_PASS_NAME		"oppo_mp_pass"
#define OPPO_CSV_FAIL_NAME		"oppo_mp_fail"
#define OPPO_CSV_LCM_PASS_NAME		"oppo_mp_lcm_pass"
#define OPPO_CSV_LCM_FAIL_NAME		"oppo_mp_lcm_fail"

struct mp_test_P540_open {
	int32_t *cbk_700;
	int32_t *cbk_250;
	int32_t *cbk_200;
	int32_t *charg_rate;
	int32_t *full_Open;
	int32_t *dac;
	int32_t *cdc;
};

struct mp_test_items {
	char *name;
	/* The description must be the same as ini's section name */
	char *desp;
	char *result;
	int catalog;
	uint8_t cmd;
	uint8_t spec_option;
	uint8_t type_option;
	bool run;
	int max;
	int max_res;
	int item_result;
	int test_count;
	int min;
	int min_res;
	int frame_count;
	int trimmed_mean;
	int lowest_percentage;
	int highest_percentage;
	int32_t *result_buf;
	int32_t *buf;
	int32_t *max_buf;
	int32_t *min_buf;
	int32_t *bench_mark_max;
	int32_t *bench_mark_min;
	int32_t *node_type;
	int (*do_test)(int index);
};

struct mp_nodp_calc {
	bool is60HZ;
	bool isLongV;

	/* Input */
	uint16_t tshd;
	uint8_t multi_term_num_120;
	uint8_t multi_term_num_60;
	uint16_t tsvd_to_tshd;
	uint16_t qsh_tdf;

	/* Settings */
	uint8_t auto_trim;
	uint16_t tp_tshd_wait_120;
	uint16_t ddi_width_120;
	uint16_t tp_tshd_wait_60;
	uint16_t ddi_width_60;
	uint16_t dp_to_tp;
	uint16_t tx_wait_const;
	uint16_t tx_wait_const_multi;
	uint16_t tp_to_dp;
	uint8_t phase_adc;
	uint8_t r2d_pw;
	uint8_t rst_pw;
	uint8_t rst_pw_back;
	uint8_t dac_td;
	uint8_t qsh_pw;
	uint8_t qsh_td;
	uint8_t drop_nodp;

	/* Output */
	uint32_t first_tp_width;
	uint32_t tp_width;
	uint32_t txpw;
	uint32_t long_tsdh_wait;
	uint32_t nodp;
};

struct core_mp_test_data {
	struct mp_nodp_calc nodp;
	/* A flag shows a test run in particular */
	bool run;
	bool oppo_run;
	bool oppo_lcm;
	bool m_signal;
	bool m_dac;
	bool s_signal;
	bool s_dac;
	bool key_dac;
	bool st_dac;
	bool p_no_bk;
	bool p_has_bk;
	bool open_integ;
	bool open_cap;

	int xch_len;
	int ych_len;
	int stx_len;
	int srx_len;
	int key_len;
	int st_len;
	int frame_len;
	int mp_items;
	int final_result;

	/* Tx/Rx threshold & buffer */
	int TxDeltaMax;
	int TxDeltaMin;
	int RxDeltaMax;
	int RxDeltaMin;
	int32_t *tx_delta_buf;
	int32_t *rx_delta_buf;
	int32_t *tx_max_buf;
	int32_t *tx_min_buf;
	int32_t *rx_max_buf;
	int32_t *rx_min_buf;

	int tdf;
	bool retry;
	int busy_cdc;
	bool ctrl_lcm;
};

extern struct core_mp_test_data *core_mp;
extern struct mp_test_items tItems[];

extern void dump_data(void *data, int type, int len, int row_len, const char *name);
extern int core_mp_calc_timing_nodp(void);
extern int core_mp_ctrl_lcm_status(bool on);
extern void core_mp_test_free(void);
extern void core_mp_show_result(void);
extern void core_mp_run_test(char *item, bool ini);
extern int core_mp_move_code(void);
extern int core_mp_init(void);
#endif
