/*
 * This file should be included by the platform. It includes all of the 
 * functions that the core provides.
 */

#ifndef _FSC_CORE_H
#define	_FSC_CORE_H

#include "version.h"
#include "fusb30X.h"
#include "TypeC.h"
#include "PDPolicy.h"
#include "PDProtocol.h"
#include "vdm/DisplayPort/interface_dp.h"
#include "platform.h"

VOID core_initialize(VOID);
VOID core_state_machine(VOID);
VOID core_tick_at_100us(VOID);
VOID core_enable_typec(BOOL enable);

VOID core_set_vbus_transition_time(UINT32 time_ms);
UINT8 core_get_rev_lower(VOID);
UINT8 core_get_rev_upper(VOID);
VOID core_configure_port_type(UINT8 config);
VOID core_enable_pd(BOOL enable);
VOID core_set_source_caps(UINT8 * buf);
VOID core_get_source_caps(UINT8 * buf);
VOID core_set_sink_caps(UINT8 * buf);
VOID core_get_sink_caps(UINT8 * buf);
VOID core_set_sink_req(UINT8 * buf);
VOID core_get_sink_req(UINT8 * buf);
VOID core_send_hard_reset(VOID);

VOID core_process_pd_buffer_read(UINT8 * InBuffer, UINT8 * OutBuffer);
VOID core_process_typec_pd_status(UINT8 * InBuffer, UINT8 * OutBuffer);
VOID core_process_typec_pd_control(UINT8 * InBuffer, UINT8 * OutBuffer);
VOID core_process_local_register_request(UINT8 * InBuffer, UINT8 * OutBuffer);
VOID core_process_set_typec_state(UINT8 * InBuffer, UINT8 * OutBuffer);
VOID core_process_read_typec_state_log(UINT8 * InBuffer, UINT8 * OutBuffer);
VOID core_process_read_pd_state_log(UINT8 * InBuffer, UINT8 * OutBuffer);

VOID core_set_alternate_modes(UINT8 * InBuffer, UINT8 * OutBuffer);
VOID core_set_manual_retries(UINT8 * InBuffer, UINT8 * OutBuffer);
UINT8 core_get_alternate_modes(VOID);
UINT8 core_get_manual_retries(VOID);

VOID core_set_state_unattached(VOID);	// Set state machine to unattached so modes update

#endif /* _FSC_CORE_H */
