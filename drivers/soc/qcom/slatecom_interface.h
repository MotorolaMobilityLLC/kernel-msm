/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */
#ifndef SLATECOM_INTERFACE_H
#define SLATECOM_INTERFACE_H

/*
 * slate_soft_reset() - soft reset Blackghost
 * Return 0 on success or -Ve on error
 */
int slate_soft_reset(void);

/*
 * is_twm_exit()
 * Return true if device is booting up on TWM exit.
 * value is auto cleared once read.
 */
bool is_twm_exit(void);

/*
 * is_slate_running()
 * Return true if slate is running.
 * value is auto cleared once read.
 */
bool is_slate_running(void);

/*
 * set_slate_dsp_state()
 * Set slate dsp state
 */
void set_slate_dsp_state(bool status);

/*
 * set_slate_bt_state()
 * Set slate bt state
 */
void set_slate_bt_state(bool status);
void slatecom_intf_notify_glink_channel_state(bool state);
void slatecom_rx_msg(void *data, int len);

/*
 * WM header type - generic header structure
 */
struct wear_header {
	uint32_t opcode;
	uint32_t payload_size;
};

/**
 * Opcodes to be received on slate-control channel.
 */
enum WMSlateCtrlChnlOpcode {
	/*
	 * Command to slate to enter TWM mode
	 */
	GMI_WEAR_MGR_ENTER_TWM = 1,

	/*
	 * Notification to slate about Modem Processor Sub System
	 * is down due to a subsystem reset.
	 */
	GMI_WEAR_MGR_SSR_MPSS_DOWN_NOTIFICATION = 2,

	/*
	 * Notification to slate about Modem Processor Sub System
	 * being brought up after a subsystem reset.
	 */
	GMI_WEAR_MGR_SSR_MPSS_UP_NOTIFICATION = 3,

	/*
	 * Notification to slate about ADSP Sub System
	 * is down due to a subsystem reset.
	 */
	GMI_WEAR_MGR_SSR_ADSP_DOWN_INDICATION = 8,

	/*
	 * Notification to slate about Modem Processor
	 * Sub System being brought up after a subsystem reset.
	 */
	GMI_WEAR_MGR_SSR_ADSP_UP_INDICATION = 9,

	/*
	 * Notification to MSM for generic wakeup in tracker mode
	 */
	GMI_WEAR_MGR_WAKE_UP_NO_REASON = 10,

	/*
	 * Notification to Slate About Entry to Tracker-DS
	 */
	GMI_WEAR_MGR_ENTER_TRACKER_DS = 11,

	/*
	 * Notification to Slate About Entry to Tracker-DS
	 */
	GMI_WEAR_MGR_EXIT_TRACKER_DS = 12,

	/*
	 * Notification to Slate About Time-sync update
	 */
	GMI_WEAR_MGR_TIME_SYNC_UPDATE = 13,	/* payload struct: wear_time_sync_t*/

	/*
	 * Notification to Slate About Timeval UTC
	 */
	GMI_WEAR_MGR_TIMEVAL_UTC = 14,		/* payload struct: wear_timeval_utc_t*/

	/*
	 * Notification to Slate About Daylight saving time
	 */
	GMI_WEAR_MGR_DST = 15,			/* payload struct: wear_dst_t*/

};
#endif /* SLATECOM_INTERFACE_H */

