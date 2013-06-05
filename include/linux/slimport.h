/*
 * Copyright(c) 2012, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _SLIMPORT_H
#define _SLIMPORT_H

#define DEBUG

#ifdef DEBUG
#define SP_DEV_DBG(args...) pr_info(args)
#else
#define SP_DEV_DBG(args...) (void)0
#endif

#define SP_DEV_NOTICE(args...) pr_notice(args)
#define SP_DEV_ERR(args...) pr_err(args)

#define SSC_EN
#define HDCP_EN

#if 0
#define SSC_1
#define EYE_TEST
#define EDID_DEBUG_PRINT
#endif

#define AUX_ERR 1
#define AUX_OK 0

extern bool sp_tx_hw_lt_done;
extern bool sp_tx_hw_lt_enable;
extern bool sp_tx_link_config_done ;
extern enum SP_TX_System_State sp_tx_system_state;
extern enum RX_CBL_TYPE sp_tx_rx_type;
extern enum RX_CBL_TYPE sp_tx_rx_type_backup;
extern unchar sp_tx_pd_mode;

extern unchar bedid_break;
extern unchar sp_tx_hw_hdcp_en;
extern struct i2c_client *anx7808_client;

int sp_read_reg(uint8_t slave_addr, uint8_t offset, uint8_t *buf);
int sp_write_reg(uint8_t slave_addr, uint8_t offset, uint8_t value);
void sp_tx_hardware_poweron(void);
void sp_tx_hardware_powerdown(void);
int slimport_read_edid_block(int block, uint8_t *edid_buf);
unchar slimport_get_link_bw(void);
unchar sp_get_ds_cable_type(void);

#ifdef CONFIG_SLIMPORT_ANX7808
bool slimport_is_connected(void);
#else
static inline bool slimport_is_connected(void)
{
	return false;
}
#endif

#endif
