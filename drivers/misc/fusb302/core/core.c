#include "core.h"
#include "TypeC.h"
#include "PDProtocol.h"
#include "PDPolicy.h"

#ifdef FSC_DEBUG
#include "version.h"
#endif // FSC_DEBUG

/*
 * Call this function to initialize the core.
 */
void core_initialize(void)
{
	InitializeTypeCVariables();	// Initialize the TypeC variables for the state machine
	InitializePDProtocolVariables();	// Initialize the USB PD variables
	InitializePDPolicyVariables();	// Initialize the USB PD variables
	InitializeTypeC();
}

/*
 * Call this function to enable or disable the core Type-C State Machine.
 * TRUE  -> enable the core state machine
 * FALSE -> disable the core state machine
 */
void core_enable_typec(FSC_BOOL enable)
{
	if (enable == TRUE)
		EnableTypeCStateMachine();
	else
		DisableTypeCStateMachine();
}

/*
 * Call this function to run the state machines.
 */
void core_state_machine(void)
{
	StateMachineTypeC();
}

/*
 * Call this function every 100us for the core's timers.
 */
void core_tick_at_100us(void)
{
	TypeCTickAt100us();
	ProtocolTickAt100us();
	PolicyTickAt100us();

#ifdef FSC_DEBUG
	LogTickAt100us();
#endif // FSC_DEBUG
}

#ifdef FSC_DEBUG
/*
 * Call this function to get the lower 8-bits of the core revision number.
 */
FSC_U8 core_get_rev_lower(void)
{
	return FSC_TYPEC_CORE_FW_REV_LOWER;
}

/*
 * Call this function to get the middle 8-bits of the core revision number.
 */
FSC_U8 core_get_rev_middle(void)
{
	return FSC_TYPEC_CORE_FW_REV_MIDDLE;
}

/*
 * Call this function to get the upper 8-bits of the core revision number.
 */
FSC_U8 core_get_rev_upper(void)
{
	return FSC_TYPEC_CORE_FW_REV_UPPER;
}
#endif // FSC_DEBUG

void core_set_vbus_transition_time(FSC_U32 time_ms)
{
	SetVbusTransitionTime(time_ms);
}

#ifdef FSC_DEBUG
void core_configure_port_type(FSC_U8 config)
{
	ConfigurePortType(config);
}

void core_enable_pd(FSC_BOOL enable)
{
	if (enable == TRUE)
		EnableUSBPD();
	else
		DisableUSBPD();
}

#ifdef FSC_HAVE_SRC
void core_set_source_caps(FSC_U8 * buf)
{
	WriteSourceCapabilities(buf);
}

void core_get_source_caps(FSC_U8 * buf)
{
	ReadSourceCapabilities(buf);
}
#endif // FSC_HAVE_SRC

#ifdef FSC_HAVE_SNK
void core_set_sink_caps(FSC_U8 * buf)
{
	WriteSinkCapabilities(buf);
}

void core_get_sink_caps(FSC_U8 * buf)
{
	ReadSinkCapabilities(buf);
}

void core_set_sink_req(FSC_U8 * buf)
{
	WriteSinkRequestSettings(buf);
}

void core_get_sink_req(FSC_U8 * buf)
{
	ReadSinkRequestSettings(buf);
}
#endif // FSC_HAVE_SNK

void core_send_hard_reset(void)
{
	SendUSBPDHardReset();
}

void core_process_pd_buffer_read(FSC_U8 * InBuffer, FSC_U8 * OutBuffer)
{
	ProcessPDBufferRead(InBuffer, OutBuffer);
}

void core_process_typec_pd_status(FSC_U8 * InBuffer, FSC_U8 * OutBuffer)
{
	ProcessTypeCPDStatus(InBuffer, OutBuffer);
}

void core_process_typec_pd_control(FSC_U8 * InBuffer, FSC_U8 * OutBuffer)
{
	ProcessTypeCPDControl(InBuffer, OutBuffer);
}

void core_process_local_register_request(FSC_U8 * InBuffer, FSC_U8 * OutBuffer)
{
	ProcessLocalRegisterRequest(InBuffer, OutBuffer);
}

void core_process_set_typec_state(FSC_U8 * InBuffer, FSC_U8 * OutBuffer)
{
	ProcessSetTypeCState(InBuffer, OutBuffer);
}

void core_process_read_typec_state_log(FSC_U8 * InBuffer, FSC_U8 * OutBuffer)
{
	ProcessReadTypeCStateLog(InBuffer, OutBuffer);
}

void core_process_read_pd_state_log(FSC_U8 * InBuffer, FSC_U8 * OutBuffer)
{
	ProcessReadPDStateLog(InBuffer, OutBuffer);
}

void core_set_alternate_modes(FSC_U8 * InBuffer, FSC_U8 * OutBuffer)
{
	setAlternateModes(InBuffer[3]);
}

void core_set_manual_retries(FSC_U8 * InBuffer, FSC_U8 * OutBuffer)
{
	setManualRetries(InBuffer[4]);
}

FSC_U8 core_get_alternate_modes(void)
{
	return getAlternateModes();
}

FSC_U8 core_get_manual_retries(void)
{
	return getManualRetries();
}

void core_set_state_unattached(void)
{
	SetStateUnattached();
}

#endif // FSC_DEBUG
