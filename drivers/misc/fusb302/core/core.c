#include <linux/printk.h>
#include <linux/jiffies.h>
#include "core.h"
#include "TypeC.h"
#include "PDProtocol.h"
#include "PDPolicy.h"
#include "TypeC_Types.h"
#include "PD_Types.h"

#include "vdm/vdm.h"
#ifdef FSC_DEBUG
#include "version.h"
#endif // FSC_DEBUG
extern FSC_BOOL PolicyHasContract;
extern doDataObject_t USBPDContract;
extern SourceOrSink sourceOrSink;
extern USBTypeCCurrent SinkCurrent;

static void core_wakeup_statemachine(void)
{
	Registers.Mask.M_COMP_CHNG = 0;
	Registers.Mask.M_ACTIVITY = 0;
	Registers.Mask.M_COLLISION = 0;
	DeviceWrite(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.M_RETRYFAIL = 0;
	Registers.MaskAdv.M_TXSENT = 0;
	Registers.MaskAdv.M_SOFTRST = 0;
	Registers.MaskAdv.M_HARDRST = 0;
	DeviceWrite(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.MaskAdv.M_GCRCSENT = 0;
	DeviceWrite(regMaskb, 1, &Registers.MaskAdv.byte[1]);
}
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
void core_state_machine_imp(void)
{
	WakeStateMachineTypeC();
}
/*
 * Call this function every 100us for the core's timers.
 */
void core_tick(void)
{
	TypeCTick();
	PolicyTick();

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
	core_wakeup_statemachine();
	PolicyState = peSinkGetSourceCap;
	PolicySubIndex = 0;
	PDTxStatus = txIdle;
	PolicySinkGetSourceCap();
	ProtocolIdle();
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
	core_wakeup_statemachine();
	PolicyState = peSinkSendHardReset;
	PolicySubIndex = 0;
	PDTxStatus = txIdle;
	PolicySinkSendHardReset();
	ProtocolIdle();
	platform_run_wake_thread();
}
/*Re-evaluate the source capability based on
* new voltage max from charging driver
*/
void core_send_sink_request_voltage(void)
{
	static int timeout_count;

	atomic_set(&coreReqCtx.pending, 1);
	core_wakeup_statemachine();
	PolicySubIndex = 0;
	PDTxStatus = txIdle;
	PolicyStateTimer = 1000;
	PolicyState = peSinkReady;
	PolicySinkEvaluateCaps();
	PolicySinkSelectCapability();
	ProtocolIdle();
	if (!wait_for_completion_timeout(&coreReqCtx.complete,
			msecs_to_jiffies(1000))) {
		pr_err("core_send_sink_request_voltage timeout!\n");
		timeout_count++;
		if (timeout_count > 3) {
			timeout_count = 0;
			core_send_hard_reset();
			platform_delay_10us(SLEEP_DELAY*500);
		}
	} else
		timeout_count = 0;
	atomic_set(&coreReqCtx.pending, 0);
}
void core_send_sink_request(void)
{
	static int timeout_count;

	atomic_set(&coreReqCtx.pending, 1);
	core_wakeup_statemachine();
	PolicySubIndex = 0;
	PDTxStatus = txIdle;
	PolicyStateTimer = 1000;
	PolicyState = peSinkReady;
	requestCurLimit(gRequestOpCurrent);
	USBPDPolicyEngine();
	ProtocolIdle();
	if (!wait_for_completion_timeout(&coreReqCtx.complete,
			msecs_to_jiffies(1000))) {
		pr_err("core_send_sink_request timeout!\n");
		timeout_count++;
		if (timeout_count > 3) {
			timeout_count = 0;
			core_send_hard_reset();
			platform_delay_10us(SLEEP_DELAY*500);
		}
	} else
		timeout_count = 0;
	atomic_set(&coreReqCtx.pending, 0);
	/*Wait 100 ms for charger to do its job*/
	platform_delay_10us(SLEEP_DELAY*100);
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
void core_process_send_dr_swap(void)
{
	core_wakeup_statemachine();
	PolicySubIndex = 0;
	PDTxStatus = txIdle;
	PolicyState = peSinkSendDRSwap;
	PolicySinkSendDRSwap();
	USBPDPolicyEngine();
	ProtocolIdle();
}
void core_process_send_vdm(void)
{
	core_wakeup_statemachine();
	PolicySubIndex = 1;
	PDTxStatus = txIdle;
	PolicyState = peSinkReady;
	requestDiscoverIdentity(SOP_TYPE_SOP);
	USBPDPolicyEngine();
	ProtocolIdle();
}
void core_set_alternate_modes(FSC_U8 * InBuffer, FSC_U8 * OutBuffer)
{
	// Deprecated
}

void core_set_manual_retries(FSC_U8 * InBuffer, FSC_U8 * OutBuffer)
{
	setManualRetries(InBuffer[4]);
}
FSC_U8 core_get_alternate_modes(void)
{
	return 0;
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
