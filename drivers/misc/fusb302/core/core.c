#include "core.h"
#include "TypeC.h"
#include "PDProtocol.h"
#include "PDPolicy.h"
#include "version.h"

/*
 * Call this function to initialize the core.
 */
VOID core_initialize(VOID)
{
	InitializeTypeCVariables();	// Initialize the TypeC variables for the state machine
	InitializePDProtocolVariables();	// Initialize the USB PD variables
	InitializePDPolicyVariables();	// Initialize the USB PD variables
	InitializeTypeC();
}

VOID core_initialize_config(VOID)
{
	InitializeTypeCVariables();	// Initialize the TypeC variables for the state machine
	InitializePDProtocolVariables();	// Initialize the USB PD variables
	InitializePDPolicyVariables();	// Initialize the USB PD variables
	InitializeTypeC();

	/* TODO, config */
}

/*
 * Call this function to enable or disable the core Type-C State Machine.
 * TRUE  -> enable the core state machine
 * FALSE -> disable the core state machine
 */
VOID core_enable_typec(BOOL enable)
{
	if (enable == TRUE)
		EnableTypeCStateMachine();
	else
		DisableTypeCStateMachine();
}

/*
 * Call this function to run the state machines.
 */
VOID core_state_machine(VOID)
{
	StateMachineTypeC();
}

/*
 * Call this function every 100us for the core's timers.
 */
VOID core_tick_at_100us(VOID)
{
	TypeCTickAt100us();
	ProtocolTickAt100us();
	PolicyTickAt100us();
	LogTickAt100us();
}

/*
 * Call this function to get the lower 8-bits of the core revision number.
 */
UINT8 core_get_rev_lower(VOID)
{
	return FSC_TYPEC_CORE_FW_REV_LOWER;
}

/*
 * Call this function to get the middle 8-bits of the core revision number.
 */
UINT8 core_get_rev_middle(VOID)
{
	return FSC_TYPEC_CORE_FW_REV_MIDDLE;
}

/*
 * Call this function to get the upper 8-bits of the core revision number.
 */
UINT8 core_get_rev_upper(VOID)
{
	return FSC_TYPEC_CORE_FW_REV_UPPER;
}

VOID core_set_vbus_transition_time(UINT32 time_ms)
{
	SetVbusTransitionTime(time_ms);
}

VOID core_configure_port_type(UINT8 config)
{
	ConfigurePortType(config);
}

VOID core_enable_pd(BOOL enable)
{
	if (enable == TRUE)
		EnableUSBPD();
	else
		DisableUSBPD();
}

VOID core_set_source_caps(UINT8 * buf)
{
	WriteSourceCapabilities(buf);
}

VOID core_get_source_caps(UINT8 * buf)
{
	ReadSourceCapabilities(buf);
}

VOID core_set_sink_caps(UINT8 * buf)
{
	WriteSinkCapabilities(buf);
}

VOID core_get_sink_caps(UINT8 * buf)
{
	ReadSinkCapabilities(buf);
}

VOID core_set_sink_req(UINT8 * buf)
{
	WriteSinkRequestSettings(buf);
}

VOID core_get_sink_req(UINT8 * buf)
{
	ReadSinkRequestSettings(buf);
}

VOID core_send_hard_reset(VOID)
{
	SendUSBPDHardReset();
}

VOID core_process_pd_buffer_read(UINT8 * InBuffer, UINT8 * OutBuffer)
{
	ProcessPDBufferRead(InBuffer, OutBuffer);
}

VOID core_process_typec_pd_status(UINT8 * InBuffer, UINT8 * OutBuffer)
{
	ProcessTypeCPDStatus(InBuffer, OutBuffer);
}

VOID core_process_typec_pd_control(UINT8 * InBuffer, UINT8 * OutBuffer)
{
	ProcessTypeCPDControl(InBuffer, OutBuffer);
}

VOID core_process_local_register_request(UINT8 * InBuffer, UINT8 * OutBuffer)
{
	ProcessLocalRegisterRequest(InBuffer, OutBuffer);
}

VOID core_process_set_typec_state(UINT8 * InBuffer, UINT8 * OutBuffer)
{
	ProcessSetTypeCState(InBuffer, OutBuffer);
}

VOID core_process_read_typec_state_log(UINT8 * InBuffer, UINT8 * OutBuffer)
{
	ProcessReadTypeCStateLog(InBuffer, OutBuffer);
}

VOID core_process_read_pd_state_log(UINT8 * InBuffer, UINT8 * OutBuffer)
{
	ProcessReadPDStateLog(InBuffer, OutBuffer);
}

VOID core_set_alternate_modes(UINT8 * InBuffer, UINT8 * OutBuffer)
{
	setAlternateModes(InBuffer[3]);
}

VOID core_set_manual_retries(UINT8 * InBuffer, UINT8 * OutBuffer)
{
	setManualRetries(InBuffer[4]);
}

UINT8 core_get_alternate_modes(VOID)
{
	return getAlternateModes();
}

UINT8 core_get_manual_retries(VOID)
{
	return getManualRetries();
}

VOID core_set_state_unattached(VOID)
{
	SetStateUnattached();
}
