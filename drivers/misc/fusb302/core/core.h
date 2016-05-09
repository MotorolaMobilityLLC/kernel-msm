/*
 * This file should be included by the platform. It includes all of the 
 * functions that the core provides.
 */

#ifndef _FSC_CORE_H
#define	_FSC_CORE_H

#include "platform.h"

void core_initialize(void);
void core_state_machine(void);
void core_tick_at_100us(void);
void core_enable_typec(FSC_BOOL enable);
void core_state_machine_imp(void);
void core_set_vbus_transition_time(FSC_U32 time_ms);

#ifdef FSC_DEBUG
FSC_U8 core_get_rev_lower(void);
FSC_U8 core_get_rev_middle(void);
FSC_U8 core_get_rev_upper(void);
void core_configure_port_type(FSC_U8 config);
void core_enable_pd(FSC_BOOL enable);
void core_set_source_caps(FSC_U8 * buf);
void core_get_source_caps(FSC_U8 * buf);
void core_set_sink_caps(FSC_U8 * buf);
void core_get_sink_caps(FSC_U8 * buf);
void core_set_sink_req(FSC_U8 * buf);
void core_get_sink_req(FSC_U8 * buf);
void core_send_hard_reset(void);
void core_send_sink_request(void);
void core_send_sink_request_voltage(void);
void core_process_pd_buffer_read(FSC_U8 * InBuffer, FSC_U8 * OutBuffer);
void core_process_typec_pd_status(FSC_U8 * InBuffer, FSC_U8 * OutBuffer);
void core_process_typec_pd_control(FSC_U8 * InBuffer, FSC_U8 * OutBuffer);
void core_process_local_register_request(FSC_U8 * InBuffer, FSC_U8 * OutBuffer);
void core_process_set_typec_state(FSC_U8 * InBuffer, FSC_U8 * OutBuffer);
void core_process_read_typec_state_log(FSC_U8 * InBuffer, FSC_U8 * OutBuffer);
void core_process_read_pd_state_log(FSC_U8 * InBuffer, FSC_U8 * OutBuffer);
void core_process_send_dr_swap(void);
void core_process_send_vdm(void);
void core_set_alternate_modes(FSC_U8 * InBuffer, FSC_U8 * OutBuffer);
void core_set_manual_retries(FSC_U8 * InBuffer, FSC_U8 * OutBuffer);
FSC_U8 core_get_alternate_modes(void);
FSC_U8 core_get_manual_retries(void);

void core_set_state_unattached(void);	// Set state machine to unattached so modes update

#endif // FSC_DEBUG

#endif /* _FSC_CORE_H */
