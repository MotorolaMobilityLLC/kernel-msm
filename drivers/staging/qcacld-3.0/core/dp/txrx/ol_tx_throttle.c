/*
 * Copyright (c) 2012-2019 The Linux Foundation. All rights reserved.
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

#include <qdf_nbuf.h>           /* qdf_nbuf_t, etc. */
#include <qdf_atomic.h>         /* qdf_atomic_read, etc. */
#include <ol_cfg.h>             /* ol_cfg_addba_retry */
#include <htt.h>                /* HTT_TX_EXT_TID_MGMT */
#include <ol_htt_tx_api.h>      /* htt_tx_desc_tid */
#include <ol_txrx_api.h>        /* ol_txrx_vdev_handle */
#include <ol_txrx_ctrl_api.h>   /* ol_txrx_sync, ol_tx_addba_conf */
#include <cdp_txrx_tx_throttle.h>
#include <ol_ctrl_txrx_api.h>   /* ol_ctrl_addba_req */
#include <ol_txrx_internal.h>   /* TXRX_ASSERT1, etc. */
#include <ol_tx_desc.h>         /* ol_tx_desc, ol_tx_desc_frame_list_free */
#include <ol_tx.h>              /* ol_tx_vdev_ll_pause_queue_send */
#include <ol_tx_sched.h>	/* ol_tx_sched_notify, etc. */
#include <ol_tx_queue.h>
#include <ol_txrx.h>          /* ol_tx_desc_pool_size_hl */
#include <ol_txrx_dbg.h>        /* ENABLE_TX_QUEUE_LOG */
#include <qdf_types.h>          /* bool */
#include "cdp_txrx_flow_ctrl_legacy.h"
#include <ol_txrx_peer_find.h>
#include <cdp_txrx_handle.h>

#ifdef QCA_LL_TX_FLOW_CONTROL_V2
/**
 * ol_txrx_thermal_pause() - pause due to thermal mitigation
 * @pdev: pdev handle
 *
 * Return: none
 */
static inline
void ol_txrx_thermal_pause(struct ol_txrx_pdev_t *pdev)
{
	ol_txrx_pdev_pause(pdev, OL_TXQ_PAUSE_REASON_THERMAL_MITIGATION);
}

/**
 * ol_txrx_thermal_unpause() - unpause due to thermal mitigation
 * @pdev: pdev handle
 *
 * Return: none
 */
static inline
void ol_txrx_thermal_unpause(struct ol_txrx_pdev_t *pdev)
{
	ol_txrx_pdev_unpause(pdev, OL_TXQ_PAUSE_REASON_THERMAL_MITIGATION);
}
#else
/**
 * ol_txrx_thermal_pause() - pause due to thermal mitigation
 * @pdev: pdev handle
 *
 * Return: none
 */
static inline
void ol_txrx_thermal_pause(struct ol_txrx_pdev_t *pdev)
{
}

/**
 * ol_txrx_thermal_unpause() - unpause due to thermal mitigation
 * @pdev: pdev handle
 *
 * Return: none
 */
static inline
void ol_txrx_thermal_unpause(struct ol_txrx_pdev_t *pdev)
{
	ol_tx_pdev_ll_pause_queue_send_all(pdev);
}
#endif

static void ol_tx_pdev_throttle_phase_timer(void *context)
{
	struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)context;
	int ms;
	enum throttle_level cur_level;
	enum throttle_phase cur_phase;

	/* update the phase */
	pdev->tx_throttle.current_throttle_phase++;

	if (pdev->tx_throttle.current_throttle_phase == THROTTLE_PHASE_MAX)
		pdev->tx_throttle.current_throttle_phase = THROTTLE_PHASE_OFF;

	if (pdev->tx_throttle.current_throttle_phase == THROTTLE_PHASE_OFF) {
		/* Traffic is stopped */
		ol_txrx_dbg(
				   "throttle phase --> OFF\n");
		ol_txrx_throttle_pause(pdev);
		ol_txrx_thermal_pause(pdev);
		cur_level = pdev->tx_throttle.current_throttle_level;
		cur_phase = pdev->tx_throttle.current_throttle_phase;
		ms = pdev->tx_throttle.throttle_time_ms[cur_level][cur_phase];
		if (pdev->tx_throttle.current_throttle_level !=
				THROTTLE_LEVEL_0) {
			ol_txrx_dbg(
					   "start timer %d ms\n", ms);
			qdf_timer_start(&pdev->tx_throttle.
							phase_timer, ms);
		}
	} else {
		/* Traffic can go */
		ol_txrx_dbg(
					"throttle phase --> ON\n");
		ol_txrx_throttle_unpause(pdev);
		ol_txrx_thermal_unpause(pdev);
		cur_level = pdev->tx_throttle.current_throttle_level;
		cur_phase = pdev->tx_throttle.current_throttle_phase;
		ms = pdev->tx_throttle.throttle_time_ms[cur_level][cur_phase];
		if (pdev->tx_throttle.current_throttle_level !=
		    THROTTLE_LEVEL_0) {
			ol_txrx_dbg("start timer %d ms\n", ms);
			qdf_timer_start(&pdev->tx_throttle.phase_timer,	ms);
		}
	}
}

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
static void ol_tx_pdev_throttle_tx_timer(void *context)
{
	struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)context;

	ol_tx_pdev_ll_pause_queue_send_all(pdev);
}
#endif

#ifdef CONFIG_HL_SUPPORT

/**
 * ol_tx_set_throttle_phase_time() - Set the thermal mitgation throttle phase
 *				     and time
 * @pdev: the peer device object
 * @level: throttle phase level
 * @ms: throttle time
 *
 * Return: None
 */
static void
ol_tx_set_throttle_phase_time(struct ol_txrx_pdev_t *pdev, int level, int *ms)
{
	qdf_timer_stop(&pdev->tx_throttle.phase_timer);

	/* Set the phase */
	if (level != THROTTLE_LEVEL_0) {
		pdev->tx_throttle.current_throttle_phase = THROTTLE_PHASE_OFF;
		*ms = pdev->tx_throttle.throttle_time_ms[level]
						[THROTTLE_PHASE_OFF];

		/* pause all */
		ol_txrx_throttle_pause(pdev);
	} else {
		pdev->tx_throttle.current_throttle_phase = THROTTLE_PHASE_ON;
		*ms = pdev->tx_throttle.throttle_time_ms[level]
						[THROTTLE_PHASE_ON];

		/* unpause all */
		ol_txrx_throttle_unpause(pdev);
	}
}
#else

static void
ol_tx_set_throttle_phase_time(struct ol_txrx_pdev_t *pdev, int level, int *ms)
{
	/* Reset the phase */
	pdev->tx_throttle.current_throttle_phase = THROTTLE_PHASE_OFF;

	/* Start with the new time */
	*ms = pdev->tx_throttle.
		throttle_time_ms[level][THROTTLE_PHASE_OFF];

	qdf_timer_stop(&pdev->tx_throttle.phase_timer);
}
#endif

void ol_tx_throttle_set_level(struct cdp_soc_t *soc_hdl,
			      uint8_t pdev_id, int level)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle pdev = ol_txrx_get_pdev_from_pdev_id(soc, pdev_id);
	int ms = 0;

	if (level >= THROTTLE_LEVEL_MAX) {
		ol_txrx_dbg("invalid throttle level set %d, ignoring", level);
		return;
	}

	if (qdf_unlikely(!pdev)) {
		ol_txrx_err("pdev is NULL");
		return;
	}

	ol_txrx_info("Setting throttle level %d\n", level);

	/* Set the current throttle level */
	pdev->tx_throttle.current_throttle_level = (enum throttle_level)level;

	ol_tx_set_throttle_phase_time(pdev, level, &ms);

	if (level != THROTTLE_LEVEL_0)
		qdf_timer_start(&pdev->tx_throttle.phase_timer, ms);
}

void ol_tx_throttle_init_period(struct cdp_soc_t *soc_hdl,
				uint8_t pdev_id, int period,
				uint8_t *dutycycle_level)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle pdev;
	int i;

	if (qdf_unlikely(!soc)) {
		ol_txrx_err("soc is NULL");
		return;
	}

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, pdev_id);
	if (qdf_unlikely(!pdev)) {
		ol_txrx_err("pdev is NULL");
		return;
	}

	/* Set the current throttle level */
	pdev->tx_throttle.throttle_period_ms = period;

	ol_txrx_dbg("level  OFF  ON\n");
	for (i = 0; i < THROTTLE_LEVEL_MAX; i++) {
		pdev->tx_throttle.throttle_time_ms[i][THROTTLE_PHASE_ON] =
			pdev->tx_throttle.throttle_period_ms -
				((dutycycle_level[i] *
				  pdev->tx_throttle.throttle_period_ms) / 100);
		pdev->tx_throttle.throttle_time_ms[i][THROTTLE_PHASE_OFF] =
			pdev->tx_throttle.throttle_period_ms -
			pdev->tx_throttle.throttle_time_ms[
				i][THROTTLE_PHASE_ON];
		ol_txrx_dbg("%d      %d    %d\n", i,
			    pdev->tx_throttle.
			    throttle_time_ms[i][THROTTLE_PHASE_OFF],
			    pdev->tx_throttle.
			    throttle_time_ms[i][THROTTLE_PHASE_ON]);
	}
}

void ol_tx_throttle_init(struct ol_txrx_pdev_t *pdev)
{
	uint32_t throttle_period;
	uint8_t dutycycle_level[THROTTLE_LEVEL_MAX];
	int i;

	pdev->tx_throttle.current_throttle_level = THROTTLE_LEVEL_0;
	pdev->tx_throttle.current_throttle_phase = THROTTLE_PHASE_OFF;
	qdf_spinlock_create(&pdev->tx_throttle.mutex);

	throttle_period = ol_cfg_throttle_period_ms(pdev->ctrl_pdev);

	for (i = 0; i < THROTTLE_LEVEL_MAX; i++)
		dutycycle_level[i] =
			ol_cfg_throttle_duty_cycle_level(pdev->ctrl_pdev, i);

	ol_tx_throttle_init_period(cds_get_context(QDF_MODULE_ID_SOC), pdev->id,
				   throttle_period, &dutycycle_level[0]);

	qdf_timer_init(pdev->osdev, &pdev->tx_throttle.phase_timer,
		       ol_tx_pdev_throttle_phase_timer, pdev,
		       QDF_TIMER_TYPE_SW);

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
	qdf_timer_init(pdev->osdev, &pdev->tx_throttle.tx_timer,
		       ol_tx_pdev_throttle_tx_timer, pdev, QDF_TIMER_TYPE_SW);
#endif

	pdev->tx_throttle.tx_threshold = THROTTLE_TX_THRESHOLD;
}

void
ol_txrx_throttle_pause(ol_txrx_pdev_handle pdev)
{
	qdf_spin_lock_bh(&pdev->tx_throttle.mutex);

	if (pdev->tx_throttle.is_paused) {
		qdf_spin_unlock_bh(&pdev->tx_throttle.mutex);
		return;
	}

	pdev->tx_throttle.is_paused = true;
	qdf_spin_unlock_bh(&pdev->tx_throttle.mutex);
	ol_txrx_pdev_pause(pdev, 0);
}

void
ol_txrx_throttle_unpause(ol_txrx_pdev_handle pdev)
{
	qdf_spin_lock_bh(&pdev->tx_throttle.mutex);

	if (!pdev->tx_throttle.is_paused) {
		qdf_spin_unlock_bh(&pdev->tx_throttle.mutex);
		return;
	}

	pdev->tx_throttle.is_paused = false;
	qdf_spin_unlock_bh(&pdev->tx_throttle.mutex);
	ol_txrx_pdev_unpause(pdev, 0);
}
