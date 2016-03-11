#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/usb/class-dual-role.h>
#include "fusb302.h"

#ifdef CONFIG_FSUSB42_MUX
#include <linux/fsusb42.h>
#endif

#define PD_SUPPORT

#ifdef PD_SUPPORT
#include "usbpd.h"
#endif

#define FUSB302_I2C_NAME "fusb302"
//#define CUST_EINT_CC_DECODER_NUM 95

int VBUS_5V_EN;
int VBUS_12V_EN;

#define PORT_UFP                0
#define PORT_DFP                1
#define PORT_DRP                2

#define HOST_CUR_NO     0
#define HOST_CUR_USB        1
#define HOST_CUR_1500       2
#define HOST_CUR_3000       3

#define CC_UNKNOW       4
#define CC_RD_3000      3
#define CC_RD_1500      2
#define CC_RD_DEFAULT       1
#define CC_RA           0

/* Configure Definition */
#define FUSB302_I2C_NUM     7
#define FUSB302_PORT_TYPE   USBTypeC_Sink
#define FUSB302_HOST_CUR    HOST_CUR_USB
//#define FUSB302_PORT_TYPE USBTypeC_DRP
//#define FUDB302_PORT_TYPE USBTypeC_Sink

#define FUSB_MS_TO_NS(x) (x * 1000 * 1000)
#define Delay10us(x) udelay(x*10);
#define FUSB_NUM_GPIOS (3)
#define FUSB_INT_INDEX 0
#define FUSB_AUD_SW_SEL_INDEX 1
#define FUSB_AUD_DET_INDEX 2

#define FUSB302_DEBUG

#ifdef FUSB302_DEBUG
#define FUSB_LOG(fmt, args...)  pr_debug("[fusb302]" fmt, ##args)
#else
#define FUSB_LOG(fmt, args...)
#endif

#define ROLE_SWITCH_TIMEOUT  1500


struct fusb302_i2c_data {
	struct i2c_client *client;
	struct work_struct eint_work;
	struct task_struct *thread;
	int gpios[FUSB_NUM_GPIOS];
	int irq;
	spinlock_t lock;
	struct power_supply usbc_psy;
	struct power_supply switch_psy;
	bool fsa321_switch;
	bool factory_mode;
	struct dual_role_phy_instance *dual_role;
	struct dual_role_phy_desc *desc;
	struct dentry *debug_root;
	u32 debug_address;
};

struct fusb302_i2c_data *fusb_i2c_data;

static DECLARE_WAIT_QUEUE_HEAD(fusb_thread_wq);
bool state_changed;
bool suspended;

static struct hrtimer state_hrtimer;
static struct hrtimer debounce_hrtimer1;
static struct hrtimer debounce_hrtimer2;
static struct hrtimer toggle_hrtimer;

extern PolicyState_t PolicyState;
extern ProtocolState_t ProtocolState;
extern PDTxStatus_t PDTxStatus;
//
/////////////////////////////////////////////////////////////////////////////
//      Variables accessible outside of the FUSB300 state machine
/////////////////////////////////////////////////////////////////////////////
FUSB300reg_t Registers;		// Variable holding the current status of the FUSB300 registers
bool USBPDActive;		// Variable to indicate whether the USB PD state machine is active or not
bool USBPDEnabled;		// Variable to indicate whether USB PD is enabled (by the host)
u32 PRSwapTimer;		// Timer used to bail out of a PR_Swap from the Type-C side if necessary

USBTypeCPort PortType;		// Variable indicating which type of port we are implementing
bool blnCCPinIsCC1;		// Flag to indicate if the CC1 pin has been detected as the CC pin
bool blnCCPinIsCC2;		// Flag to indicate if the CC2 pin has been detected as the CC pin
bool blnSMEnabled;		// Flag to indicate whether the FUSB300 state machine is enabled
ConnectionState ConnState;	// Variable indicating the current connection state

/////////////////////////////////////////////////////////////////////////////
//      Variables accessible only inside FUSB300 state machine
/////////////////////////////////////////////////////////////////////////////
static bool blnSrcPreferred;	// Flag to indicate whether we prefer the Src role when in DRP
static bool blnAccSupport;	// Flag to indicate whether the port supports accessories
static bool blnINTActive;	// Flag to indicate that an interrupt occurred that needs to be handled
static bool usbDataDisabled;    /* Flag to indicate Data lines on USB are disabled */
static u16 StateTimer;		// Timer used to validate proceeding to next state
static u16 DebounceTimer1;	// Timer used for first level debouncing
static u16 DebounceTimer2;	// Timer used for second level debouncing
static u16 ToggleTimer;		// Timer used for CC swapping in the FUSB302
static CCTermType CC1TermAct;	// Active CC1 termination value
static CCTermType CC2TermAct;	// Active CC2 termination value
static CCTermType CC1TermDeb;	// Debounced CC1 termination value
static CCTermType CC2TermDeb;	// Debounced CC2 termination value
static USBTypeCCurrent SinkCurrent;	// Variable to indicate the current capability we have received
static USBTypeCCurrent SourceCurrent;	// Variable to indicate the current capability we are broadcasting
static u16 SwitchState; /* Variable to indicate switch state for SS lines */

/////////////////////////////////////////////////////////////////////////////
//                        FUSB300 I2C Routines
/////////////////////////////////////////////////////////////////////////////
int FUSB300Write(unsigned char regAddr, unsigned char length,
		 unsigned char *data)
{
	return i2c_smbus_write_i2c_block_data(fusb_i2c_data->client, regAddr,
					      length, data);
}

int FUSB300Read(unsigned char regAddr, unsigned char length,
		unsigned char *data)
{
	return i2c_smbus_read_i2c_block_data(fusb_i2c_data->client, regAddr,
					     length, data);
}

/////////////////////////////////////////////////////////////////////////////
//                     Internal Routines
////////////////////////////////////////////////////////////////////////////
void SetStateDelayUnattached(void);
void StateMachineUnattached(void);
void StateMachineAttachWaitSnk(void);
void StateMachineAttachedSink(void);
void StateMachineAttachWaitSrc(void);
void StateMachineAttachedSource(void);
void StateMachineTrySrc(void);
void StateMachineTryWaitSnk(void);
void StateMachineAttachWaitAcc(void);
void StateMachineDelayUnattached(void);

void SetStateUnattached(void);
void SetStateAttachWaitSnk(void);
void SetStateAttachWaitAcc(void);
void SetStateAttachWaitSrc(void);
void SetStateTrySrc(void);
void SetStateAttachedSink(void);
void SetStateAttachedSrc(void);
void UpdateSinkCurrent(CCTermType Termination);
void SetStateTryWaitSnk(void);
void UpdateSourcePowerMode(void);

static int FUSB300Int_PIN_LVL(void)
{
	int ret = gpio_get_value(fusb_i2c_data->gpios[FUSB_INT_INDEX]);
	FUSB_LOG("gpio irq_gpio value = %d\n", ret);
	return ret;
}

static int FSA321_setSwitchState(FSASwitchState state)
{
	int aud_det_gpio = fusb_i2c_data->gpios[FUSB_AUD_DET_INDEX];
	int aud_sw_sel_gpio = fusb_i2c_data->gpios[FUSB_AUD_SW_SEL_INDEX];

	if (!(gpio_is_valid(aud_det_gpio) && gpio_is_valid(aud_sw_sel_gpio)))
		return -ENODEV;

	FUSB_LOG("Set Audio Switch to state %d\n", state);

	switch (state) {
	case fsa_lpm:
		gpio_set_value(aud_sw_sel_gpio, 1);
		gpio_set_value(aud_det_gpio, 1);
		break;
	case fsa_audio_mode:
		gpio_set_value(aud_sw_sel_gpio, 0);
		gpio_set_value(aud_det_gpio, 0);
	case fsa_usb_mode:
		gpio_set_value(aud_sw_sel_gpio, 0);
		gpio_set_value(aud_det_gpio, 1);
	default:
		break;
	}

	return 0;
}

static int FUSB302_toggleAudioSwitch(bool enable)
{
	int aud_det_gpio = fusb_i2c_data->gpios[FUSB_AUD_DET_INDEX];
	int aud_sw_sel_gpio = fusb_i2c_data->gpios[FUSB_AUD_SW_SEL_INDEX];

	if (!(gpio_is_valid(aud_det_gpio) && gpio_is_valid(aud_sw_sel_gpio)))
		return -ENODEV;

	FUSB_LOG("%sabling, Audio Switch\n", enable ? "En" : "Dis");

	if (enable) {
		gpio_set_value(aud_sw_sel_gpio, 1);
		gpio_set_value(aud_det_gpio, 0);
	} else {
		gpio_set_value(aud_sw_sel_gpio, 0);
		gpio_set_value(aud_det_gpio, 1);
	}

	return 0;
}

static bool disable_ss_switch;
static int FUSB302_enableSuperspeedUSB(int CC1, int CC2)
{
	if (disable_ss_switch)
		return 0;

	SwitchState = CC1 ? 1 : 2;

	FUSB_LOG("Enabling SS lines for CC%d\n", SwitchState);
	power_supply_changed(&fusb_i2c_data->switch_psy);

	return 0;
}

static int FUSB302_disableSuperspeedUSB(void)
{
	SwitchState = 0;
	power_supply_changed(&fusb_i2c_data->switch_psy);
	return 0;
}

static int set_disable_ss_switch(const char *val, const struct kernel_param *kp)
{
	int rv = param_set_bool(val, kp);
	if (rv)
		return rv;

	if (disable_ss_switch && fusb_i2c_data)
		FUSB302_disableSuperspeedUSB();

	return 0;
}

static struct kernel_param_ops disable_ss_param_ops = {
	.set = set_disable_ss_switch,
	.get = param_get_bool,
};

module_param_cb(disable_ss_switch, &disable_ss_param_ops,
		&disable_ss_switch, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_ss_switch, "Disable Super Speed Switch");
/*
static void SourceOutput(int vol, int current)
{
    return;
}
*/
void FUSB302_start_timer(struct hrtimer *timer, int ms)
{
	ktime_t ktime;
	ktime = ktime_set(0, FUSB_MS_TO_NS(ms));
	hrtimer_start(timer, ktime, HRTIMER_MODE_REL);
}

enum hrtimer_restart func_hrtimer(struct hrtimer *timer)
{
	if (timer == &state_hrtimer)
		StateTimer = 0;
	else if (timer == &debounce_hrtimer1)
		DebounceTimer1 = 0;
	else if (timer == &debounce_hrtimer2)
		DebounceTimer2 = 0;
	else if (timer == &toggle_hrtimer)
		ToggleTimer = 0;

	state_changed = true;
	wake_up(&fusb_thread_wq);

	return HRTIMER_NORESTART;
}

void wake_up_statemachine(void)
{
	//FUSB_LOG("wake_up_statemachine.\n");
	state_changed = true;
	wake_up_interruptible(&fusb_thread_wq);
}

/*******************************************************************************
 * Function:        InitializeFUSB300Variables
 * Input:           None
 * Return:          None
 * Description:     Initializes the FUSB300 state machine variables
 ******************************************************************************/
void InitializeFUSB300Variables(void)
{
	blnSMEnabled = true;	// Disable the FUSB300 state machine by default
	blnAccSupport = false;	// Disable accessory support by default
	blnSrcPreferred = false;	// Clear the source preferred flag by default
	PortType = USBTypeC_DRP;	// Initialize to a dual-role port by default
	//  PortType = USBTypeC_Sink;            // Initialize to a Sink port by default
	ConnState = Disabled;	// Initialize to the disabled state?
	blnINTActive = false;	// Clear the handle interrupt flag
	blnCCPinIsCC1 = false;	// Clear the flag to indicate CC1 is CC
	blnCCPinIsCC2 = false;	// Clear the flag to indicate CC2 is CC
	SwitchState = 0;	/* Set the Switch to OFF State */

	StateTimer = USHRT_MAX;	// Disable the state timer
	DebounceTimer1 = USHRT_MAX;	// Disable the 1st debounce timer
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd debounce timer
	ToggleTimer = USHRT_MAX;	// Disable the toggle timer
	CC1TermDeb = CCTypeNone;	// Set the CC1 termination type to none initially
	CC2TermDeb = CCTypeNone;	// Set the CC2 termination type to none initially
	CC1TermAct = CC1TermDeb;	// Initialize the active CC1 value
	CC2TermAct = CC2TermDeb;	// Initialize the active CC2 value
	SinkCurrent = utccNone;	// Clear the current advertisement initially
	SourceCurrent = utccDefault;	// Set the current advertisement to the default
	Registers.DeviceID.byte = 0x00;	// Clear
	Registers.Switches.byte[0] = 0x03;	// Only enable the device pull-downs by default
	Registers.Switches.byte[1] = 0x00;	// Disable the BMC transmit drivers
	Registers.Measure.byte = 0x00;	// Clear
	Registers.Slice.byte = SDAC_DEFAULT;	// Set the SDAC threshold to ~0.544V by default (from FUSB302)
	Registers.Control.byte[0] = 0x20;	// Set to mask all interrupts by default (from FUSB302)
	Registers.Control.byte[1] = 0x00;	// Clear
	Registers.Control.byte[2] = 0x02;	//
	Registers.Control.byte[3] = 0x06;	//
	Registers.Mask.byte = 0x00;	// Clear
	Registers.Power.byte = 0x07;	// Initialize to everything enabled except oscillator
	Registers.Status.Status = 0;	// Clear status bytes
	Registers.Status.StatusAdv = 0;	// Clear the advanced status bytes
	Registers.Status.Interrupt1 = 0;	// Clear the interrupt1 byte
	Registers.Status.InterruptAdv = 0;	// Clear the advanced interrupt bytes
	USBPDActive = false;	// Clear the USB PD active flag
	USBPDEnabled = true;	// Clear the USB PD enabled flag until enabled by the host
	PRSwapTimer = 0;	// Clear the PR Swap timer

	//
	suspended = false;
	state_changed = false;
	hrtimer_init(&state_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	state_hrtimer.function = func_hrtimer;

	hrtimer_init(&debounce_hrtimer1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	debounce_hrtimer1.function = func_hrtimer;

	hrtimer_init(&debounce_hrtimer2, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	debounce_hrtimer2.function = func_hrtimer;

	hrtimer_init(&toggle_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	toggle_hrtimer.function = func_hrtimer;
}

void InitializeFUSB300(void)
{
	FUSB_LOG("enter:%s", __func__);
	FUSB300Read(regDeviceID, 2, &Registers.DeviceID.byte);	// Read the device ID
	FUSB300Read(regSlice, 1, &Registers.Slice.byte);	// Read the slice
	Registers.Mask.byte = 0x44;	// Mask I_ACTIVITY and I_WAKE to reduce system load
	FUSB300Write(regMask, 1, &Registers.Mask.byte);	// Clear all interrupt masks (we want to do something with them)
	// Update the control and power values since they will be written in either the Unattached.UFP or Unattached.DFP states
	Registers.Control.dword = 0x06220004;	// Reset all back to default, but clear the INT_MASK bit
	switch (PortType) {
	case USBTypeC_Sink:
		Registers.Control.MODE = 0b10;
		break;
	case USBTypeC_Source:
		Registers.Control.MODE = 0b11;
		break;
	default:
		Registers.Control.MODE = 0b01;
		break;
	}
	FUSB300Write(regControl2, 2, &Registers.Control.byte[2]);	// Update the control registers for toggling
	Registers.Control4.TOG_USRC_EXIT = 1;	// Stop toggling with Ra/Ra
	FUSB300Write(regControl4, 1, &Registers.Control4.byte);	// Commit to the device
	Registers.Power.byte = 0x01;	// Initialize such that only the bandgap and wake circuit are enabled by default
	FUSB300Read(regStatus0a, 2, &Registers.Status.byte[0]);	// Read the advanced status registers to make sure we are in sync with the device
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the standard status registers to make sure we are in sync with the device
	// Do not read any of the interrupt registers, let the state machine handle those
	SetStateDelayUnattached();
}

void DisableFUSB300StateMachine(void)
{
	blnSMEnabled = false;
	SetStateDisabled();
	//InitializeFUSB300Timer(false);
}

void EnableFUSB300StateMachine(void)
{
	InitializeFUSB300();
//    InitializeFUSB300Timer(true);
	blnSMEnabled = true;
}

/*******************************************************************************
 * Function:        StateMachineFUSB300
 * Input:           None
 * Return:          None
 * Description:     This is the state machine for the entire USB PD
 *                  This handles all the communication between the master and
 *                  slave.  This function is called by the Timer3 ISR on a
 *                  sub-interval of the 1/4 UI in order to meet timing
 *                  requirements.
 ******************************************************************************/
void StateMachineFUSB300(void)
{
	FUSB_LOG
	    ("connstate=%d, policyState=%d, protocolState=%d, PDTxStatus=%d\n",
	     ConnState, PolicyState, ProtocolState, PDTxStatus);
	//if (!blnSMEnabled)
	//    return;
	if (!FUSB300Int_PIN_LVL()) {
		// Read and store regStatus0a, regStatus1a, regInterrupta,
		//regInterruptb, regStatus0, regStatus1, regInterrupt
		int reg_addr[] =
		    { regDeviceID, regSwitches0, regSwitches1, regMeasure,
			regSlice,
			regControl0, regControl1, regControl2, regControl3,
			regMask, regPower, regReset,
			regOCPreg, regMaska, regMaskb, regControl4
		};

		char buf[1024];
		int i, len = 0;
		u8 byte = 0;
		for (i = 0; i < sizeof(reg_addr) / sizeof(reg_addr[0]); i++) {
			FUSB300Read(reg_addr[i], 1, &byte);
			len +=
			    sprintf(buf + len, "%02xH:%02x ", reg_addr[i],
				    byte);
			if (((i + 1) % 6) == 0)
				len += sprintf(buf + len, "\n");
		}
		FUSB_LOG("%s\n", buf);
		FUSB300Read(regStatus0a, 1, &Registers.Status.byte[0]);
		FUSB300Read(regStatus1a, 1, &Registers.Status.byte[1]);
		FUSB300Read(regInterrupta, 1, &Registers.Status.byte[2]);
		FUSB300Read(regInterruptb, 1, &Registers.Status.byte[3]);
		FUSB300Read(regStatus0, 1, &Registers.Status.byte[4]);
		FUSB300Read(regStatus1, 1, &Registers.Status.byte[5]);
		FUSB300Read(regInterrupt, 1, &Registers.Status.byte[6]);
	}
#ifdef PD_SUPPORT
	if (USBPDActive)	// Only call the USB PD routines if we have enabled the block
	{
		USBPDProtocol();	// Call the protocol state machine to handle any timing critical operations
		USBPDPolicyEngine();	// Once we have handled any Type-C and protocol events, call the USB PD Policy Engine
	}
#endif

	switch (ConnState) {
	case Disabled:
		StateMachineDisabled();
		break;
	case ErrorRecovery:
		StateMachineErrorRecovery();
		break;
	case Unattached:
		StateMachineUnattached();
		break;
	case AttachWaitSink:
		StateMachineAttachWaitSnk();
		break;
	case AttachedSink:
		StateMachineAttachedSink();
		break;
	case AttachWaitSource:
		StateMachineAttachWaitSrc();
		break;
	case AttachedSource:
		StateMachineAttachedSource();
		break;
	case TrySource:
		StateMachineTrySrc();
		break;
	case TryWaitSink:
		StateMachineTryWaitSnk();
		break;
	case AudioAccessory:
		StateMachineAudioAccessory();
		break;
	case DebugAccessory:
		StateMachineDebugAccessory();
		break;
	case AttachWaitAccessory:
		StateMachineAttachWaitAcc();
		break;
	case PoweredAccessory:
		StateMachinePoweredAccessory();
		break;
	case UnsupportedAccessory:
		StateMachineUnsupportedAccessory();
		break;
	case DelayUnattached:
		StateMachineDelayUnattached();
		break;
	default:
		SetStateDelayUnattached();	// We shouldn't get here, so go to the unattached state just in case
		break;
	}
	Registers.Status.Interrupt1 = 0;	// Clear the interrupt register once we've gone through the state machines
	Registers.Status.InterruptAdv = 0;	// Clear the advanced interrupt registers once we've gone through the state machines
}

void StateMachineDisabled(void)
{
	// Do nothing until directed to go to some other state...
}

void StateMachineErrorRecovery(void)
{
	if (StateTimer == 0) {
		SetStateDelayUnattached();
	}
}

void StateMachineDelayUnattached(void)
{
	if (StateTimer == 0) {
		SetStateUnattached();
	}
}

void StateMachineUnattached(void)
{
	if (Registers.Status.I_TOGDONE) {
		switch (Registers.Status.TOGSS) {
		case 0b101:	// Rp detected on CC1
			blnCCPinIsCC1 = true;
			blnCCPinIsCC2 = false;
			SetStateAttachWaitSnk();	// Go to the AttachWaitSnk state
			break;
		case 0b110:	// Rp detected on CC2
			blnCCPinIsCC1 = false;
			blnCCPinIsCC2 = true;
			SetStateAttachWaitSnk();	// Go to the AttachWaitSnk state
			break;
		case 0b001:	// Rd detected on CC1
			blnCCPinIsCC1 = true;
			blnCCPinIsCC2 = false;
			if ((PortType == USBTypeC_Sink) && (blnAccSupport))	// If we are configured as a sink and support accessories...
				SetStateAttachWaitAcc();	// Go to the AttachWaitAcc state
			else	// Otherwise we must be configured as a source or DRP
				SetStateAttachWaitSrc();	// So go to the AttachWaitSnk state
			break;
		case 0b010:	// Rd detected on CC2
			blnCCPinIsCC1 = false;
			blnCCPinIsCC2 = true;
			if ((PortType == USBTypeC_Sink) && (blnAccSupport))	// If we are configured as a sink and support accessories...
				SetStateAttachWaitAcc();	// Go to the AttachWaitAcc state
			else	// Otherwise we must be configured as a source or DRP
				SetStateAttachWaitSrc();	// So go to the AttachWaitSnk state
			break;
		case 0b111:	// Ra detected on both CC1 and CC2
			blnCCPinIsCC1 = false;
			blnCCPinIsCC2 = false;
			if ((PortType == USBTypeC_Sink) && (blnAccSupport))	// If we are configured as a sink and support accessories...
				SetStateAttachWaitAcc();	// Go to the AttachWaitAcc state
			else	// Otherwise we must be configured as a source or DRP
				SetStateAttachWaitSrc();	// So go to the AttachWaitSnk state
			break;
		default:	// Shouldn't get here, but just in case reset everything...
			Registers.Control.TOGGLE = 0;	// Disable the toggle in order to clear...
			FUSB300Write(regControl2, 1, &Registers.Control.byte[2]);	// Commit the control state
			Delay10us(1);

			Registers.Control.TOGGLE = 1;	// Re-enable the toggle state machine... (allows us to get another I_TOGDONE interrupt)
			FUSB300Write(regControl2, 1, &Registers.Control.byte[2]);	// Commit the control state
			break;
		}
	}
	//  rand();
}

void StateMachineAttachWaitSnk(void)
{
	// If the both CC lines has been open for tPDDebounce, go to the unattached state
	// If VBUS and the we've been Rd on exactly one pin for 100ms... go to the attachsnk state
	CCTermType CCValue = DecodeCCTermination();	// Grab the latest CC termination value
	if (Registers.Switches.MEAS_CC1)	// If we are looking at CC1
	{
		if (CC1TermAct != CCValue)	// Check to see if the value has changed...
		{
			CC1TermAct = CCValue;	// If it has, update the value
			DebounceTimer1 = tPDDebounceMin;	// Restart the debounce timer with tPDDebounce (wait 10ms before detach)
			FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
		}
	} else			// Otherwise we are looking at CC2
	{
		if (CC2TermAct != CCValue)	// Check to see if the value has changed...
		{
			CC2TermAct = CCValue;	// If it has, update the value
			DebounceTimer1 = tPDDebounceMin;	// Restart the debounce timer with tPDDebounce (wait 10ms before detach)
			FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
		}
	}
	if (DebounceTimer1 == 0)	// Check to see if our debounce timer has expired...
	{
		DebounceTimer1 = USHRT_MAX;	// If it has, disable it so we don't come back in here until we have debounced a change in state
		if ((CC1TermDeb != CC1TermAct) || (CC2TermDeb != CC2TermAct)) {
			DebounceTimer2 = tCCDebounceMin - tPDDebounceMin;	// Once the CC state is known, start the tCCDebounce timer to validate
			FUSB302_start_timer(&debounce_hrtimer2, DebounceTimer2);
		}
		CC1TermDeb = CC1TermAct;	// Update the CC1 debounced value
		CC2TermDeb = CC2TermAct;	// Update the CC2 debounced value
	}
	if (ToggleTimer == 0)	// If are toggle timer has expired, it's time to swap detection
	{
		if (Registers.Switches.MEAS_CC1)	// If we are currently on the CC1 pin...
			ToggleMeasureCC2();	// Toggle over to look at CC2
		else		// Otherwise assume we are using the CC2...
			ToggleMeasureCC1();	// So toggle over to look at CC1
		ToggleTimer = tFUSB302Toggle;	// Reset the toggle timer to our default toggling (<tPDDebounce to avoid disconnecting the other side when we remove pull-ups)
		FUSB302_start_timer(&toggle_hrtimer, ToggleTimer);
	}
	if ((CC1TermDeb == CCTypeRa) && (CC2TermDeb == CCTypeRa))	// If we have detected SNK.Open for atleast tPDDebounce on both pins...
		SetStateDelayUnattached();	// Go to the unattached state
	else if (Registers.Status.VBUSOK && (DebounceTimer2 == 0))	// If we have detected VBUS and we have detected an Rp for >tCCDebounce...
	{
		if ((CC1TermDeb > CCTypeRa) && (CC2TermDeb == CCTypeRa))	// If Rp is detected on CC1
		{
			if ((PortType == USBTypeC_DRP) && blnSrcPreferred)	// If we are configured as a DRP and prefer the source role...
				SetStateTrySrc();	// Go to the Try.Src state
			else	// Otherwise we are free to attach as a sink
			{
				blnCCPinIsCC1 = true;	// Set the CC pin to CC1
				blnCCPinIsCC2 = false;	//
				SetStateAttachedSink();	// Go to the Attached.Snk state
			}
		} else if ((CC1TermDeb == CCTypeRa) && (CC2TermDeb > CCTypeRa))	// If Rp is detected on CC2
		{
			if ((PortType == USBTypeC_DRP) && blnSrcPreferred)	// If we are configured as a DRP and prefer the source role...
				SetStateTrySrc();	// Go to the Try.Src state
			else	// Otherwise we are free to attach as a sink
			{
				blnCCPinIsCC1 = false;	//
				blnCCPinIsCC2 = true;	// Set the CC pin to CC2
				SetStateAttachedSink();	// Go to the Attached.Snk State
			}
		}
	}
}

void StateMachineAttachWaitSrc(void)
{
	CCTermType CCValue = DecodeCCTermination();	// Grab the latest CC termination value
	if (Registers.Switches.MEAS_CC1)	// If we are looking at CC1
	{
		if (CC1TermAct != CCValue)	// Check to see if the value has changed...
		{
			CC1TermAct = CCValue;	// If it has, update the value
			DebounceTimer1 = tPDDebounceMin;	// Restart the debounce timer (tPDDebounce)
			FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
		}
	} else			// Otherwise we are looking at CC2
	{
		if (CC2TermAct != CCValue)	// Check to see if the value has changed...
		{
			CC2TermAct = CCValue;	// If it has, update the value
			DebounceTimer1 = tPDDebounceMin;	// Restart the debounce timer (tPDDebounce)
			FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
		}
	}
	if (DebounceTimer1 == 0)	// Check to see if our debounce timer has expired...
	{
		DebounceTimer1 = USHRT_MAX;	// If it has, disable it so we don't come back in here until we have debounced a change in state
		if ((CC1TermDeb != CC1TermAct) || (CC2TermDeb != CC2TermAct)) {
			DebounceTimer2 = tCCDebounceMin;	// Once the CC state is known, start the tCCDebounce timer to validate
			FUSB302_start_timer(&debounce_hrtimer2, DebounceTimer2);
		}
		CC1TermDeb = CC1TermAct;	// Update the CC1 debounced value
		CC2TermDeb = CC2TermAct;	// Update the CC2 debounced value
	}
	if (ToggleTimer == 0)	// If are toggle timer has expired, it's time to swap detection
	{
		if (Registers.Switches.MEAS_CC1)	// If we are currently on the CC1 pin...
			ToggleMeasureCC2();	// Toggle over to look at CC2
		else		// Otherwise assume we are using the CC2...
			ToggleMeasureCC1();	// So toggle over to look at CC1
		ToggleTimer = tFUSB302Toggle;	// Reset the toggle timer to our default toggling (<tPDDebounce to avoid disconnecting the other side when we remove pull-ups)
		FUSB302_start_timer(&toggle_hrtimer, ToggleTimer);
	}
	if ((CC1TermDeb == CCTypeNone) && (CC2TermDeb == CCTypeNone))	// If our debounced signals are both open, go to the unattached state
	{
		SetStateDelayUnattached();
	} else if (DebounceTimer2 == 0)	// Otherwise, we are checking to see if we have had a solid state for tCCDebounce
	{
		if ((CC1TermDeb == CCTypeRa) && (CC2TermDeb == CCTypeRa))	// If both pins are Ra, it's an audio accessory
			SetStateAudioAccessory();
		else if ((CC1TermDeb > CCTypeRa) && (CC2TermDeb > CCTypeRa))	// If both pins are Rd, it's a debug accessory
			SetStateDebugAccessory();
		else if ((CC1TermDeb == CCTypeNone)
				&& (CC2TermDeb == CCTypeRa)) {
			/*
			 * If exactly one pin is open and the other is Ra,
			 * go to the unattached state.
			 */
			SetStateDelayUnattached();
		} else if ((CC1TermDeb == CCTypeRa)
				&& (CC2TermDeb == CCTypeNone)) {
			/*
			 * If exactly one pin is open and the other is Ra,
			 * go to the unattached state.
			 */
			SetStateDelayUnattached();
		} else if (CC1TermDeb > CCTypeRa)
		{
			/* If CC1 is Rd and CC2 is not... */
			blnCCPinIsCC1 = true;	// Set the CC pin to CC1
			blnCCPinIsCC2 = false;
			SetStateAttachedSrc();	// Go to the Attached.Src state
		} else if (CC2TermDeb > CCTypeRa)	// If CC2 is Rd and CC1 is not...
		{
			blnCCPinIsCC1 = false;
			blnCCPinIsCC2 = true;	// Set the CC pin to CC2
			SetStateAttachedSrc();	// Go to the Attached.Src state
		}
	}
}

void StateMachineAttachWaitAcc(void)
{
	CCTermType CCValue = DecodeCCTermination();	// Grab the latest CC termination value
	if (Registers.Switches.MEAS_CC1)	// If we are looking at CC1
	{
		if (CC1TermAct != CCValue)	// Check to see if the value has changed...
		{
			CC1TermAct = CCValue;	// If it has, update the value
			DebounceTimer1 = tCCDebounceNom;	// Restart the debounce timer (tCCDebounce)
			FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
		}
	} else			// Otherwise we are looking at CC2
	{
		if (CC2TermAct != CCValue)	// Check to see if the value has changed...
		{
			CC2TermAct = CCValue;	// If it has, update the value
			DebounceTimer1 = tCCDebounceNom;	// Restart the debounce timer (tCCDebounce)
			FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
		}
	}
	if (ToggleTimer == 0)	// If are toggle timer has expired, it's time to swap detection
	{
		if (Registers.Switches.MEAS_CC1)	// If we are currently on the CC1 pin...
			ToggleMeasureCC2();	// Toggle over to look at CC2
		else		// Otherwise assume we are using the CC2...
			ToggleMeasureCC1();	// So toggle over to look at CC1
		ToggleTimer = tFUSB302Toggle;	// Reset the toggle timer to our default toggling (<tPDDebounce to avoid disconnecting the other side when we remove pull-ups)
		FUSB302_start_timer(&toggle_hrtimer, ToggleTimer);
	}
	if (DebounceTimer1 == 0)	// Check to see if the signals have been stable for tCCDebounce
	{
		if ((CC1TermDeb == CCTypeRa) && (CC2TermDeb == CCTypeRa))	// If they are both Ra, it's an audio accessory
			SetStateAudioAccessory();
		else if ((CC1TermDeb > CCTypeRa) && (CC2TermDeb > CCTypeRa))	// If they are both Rd, it's a debug accessory
			SetStateDebugAccessory();
		else if ((CC1TermDeb == CCTypeNone) || (CC2TermDeb == CCTypeNone))	// If either pin is open, it's considered a detach
			SetStateDelayUnattached();
		else if ((CC1TermDeb > CCTypeRa) && (CC2TermDeb == CCTypeRa))	// If CC1 is Rd and CC2 is Ra, it's a powered accessory (CC1 is CC)
		{
			blnCCPinIsCC1 = true;
			blnCCPinIsCC2 = false;
			SetStatePoweredAccessory();
		} else if ((CC1TermDeb == CCTypeRa) && (CC2TermDeb > CCTypeRa))	// If CC1 is Ra and CC2 is Rd, it's a powered accessory (CC2 is CC)
		{
			blnCCPinIsCC1 = true;
			blnCCPinIsCC2 = false;
			SetStatePoweredAccessory();
		}
	}
}

void StateMachineAttachedSink(void)
{
	CCTermType CCValue = DecodeCCTermination();	// Grab the latest CC termination value
	FUSB_LOG
	    ("vbus=%d, prswaptimer=%d, CCValue=%d, MEAS_CC1 =%d, MEAS_CC2 =%d\n",
	     Registers.Status.VBUSOK, PRSwapTimer, CCValue,
	     Registers.Switches.MEAS_CC1, Registers.Switches.MEAS_CC2);
	if ((Registers.Status.VBUSOK == false) && (!PRSwapTimer))	// If VBUS is removed and we are not in the middle of a power role swap...
		SetStateDelayUnattached();	// Go to the unattached state
	else {
		if (Registers.Switches.MEAS_CC1)	// If we are looking at CC1
		{
			if (CCValue != CC1TermAct)	// If the CC voltage has changed...
			{
				CC1TermAct = CCValue;	// Store the updated value
				DebounceTimer1 = tPDDebounceMin;	// Reset the debounce timer to the minimum tPDdebounce
				FUSB302_start_timer(&debounce_hrtimer1,
						    DebounceTimer1);
			} else if (DebounceTimer1 == 0)	// If the signal has been debounced
			{
				DebounceTimer1 = USHRT_MAX;	// Disable the debounce timer until we get a change
				CC1TermDeb = CC1TermAct;	// Store the debounced termination for CC1
				UpdateSinkCurrent(CC1TermDeb);	// Update the advertised current
			}
		} else {
			if (CCValue != CC2TermAct)	// If the CC voltage has changed...
			{
				CC2TermAct = CCValue;	// Store the updated value
				DebounceTimer1 = tPDDebounceMin;	// Reset the debounce timer to the minimum tPDdebounce
				FUSB302_start_timer(&debounce_hrtimer1,
						    DebounceTimer1);
			} else if (DebounceTimer1 == 0)	// If the signal has been debounced
			{
				DebounceTimer1 = USHRT_MAX;	// Disable the debounce timer until we get a change
				CC2TermDeb = CC2TermAct;	// Store the debounced termination for CC2
				UpdateSinkCurrent(CC2TermDeb);	// Update the advertised current
			}
		}
	}
}

void StateMachineAttachedSource(void)
{
	CCTermType CCValue = DecodeCCTermination();	// Grab the latest CC termination value
	struct power_supply *usb_psy = power_supply_get_by_name("usb");
	if (Registers.Switches.MEAS_CC1)	// Did we detect CC1 as the CC pin?
	{
		if (CC1TermAct != CCValue)	// If the CC voltage has changed...
		{
			CC1TermAct = CCValue;	// Store the updated value
			DebounceTimer1 = tPDDebounceMin;	// Reset the debounce timer to the minimum tPDdebounce
			FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
		} else if (DebounceTimer1 == 0)	// If the signal has been debounced
		{
			DebounceTimer1 = USHRT_MAX;	// Disable the debounce timer until we get a change
			CC1TermDeb = CC1TermAct;	// Store the debounced termination for CC1
		}
		if ((CC1TermDeb == CCTypeNone) && (!PRSwapTimer))	// If the debounced CC pin is detected as open and we aren't in the middle of a PR_Swap
		{
			/* Notify USB driver to exit host mode */
			if (usb_psy)
				power_supply_set_usb_otg(usb_psy, 0);

			if ((PortType == USBTypeC_DRP) && blnSrcPreferred)	// Check to see if we need to go to the TryWait.SNK state...
				SetStateTryWaitSnk();
			else	// Otherwise we are going to the unattached state
				SetStateDelayUnattached();
		}
	} else			// We must have detected CC2 as the CC pin
	{
		if (CC2TermAct != CCValue)	// If the CC voltage has changed...
		{
			CC2TermAct = CCValue;	// Store the updated value
			DebounceTimer1 = tPDDebounceMin;	// Reset the debounce timer to the minimum tPDdebounce
			FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
		} else if (DebounceTimer1 == 0)	// If the signal has been debounced
		{
			DebounceTimer1 = USHRT_MAX;	// Disable the debounce timer until we get a change
			CC2TermDeb = CC2TermAct;	// Store the debounced termination for CC1
		}
		if ((CC2TermDeb == CCTypeNone) && (!PRSwapTimer))	// If the debounced CC pin is detected as open and we aren't in the middle of a PR_Swap
		{
			/* Notify USB driver to exit host mode */
			if (usb_psy)
				power_supply_set_usb_otg(usb_psy, 0);
			if ((PortType == USBTypeC_DRP) && blnSrcPreferred)	// Check to see if we need to go to the TryWait.SNK state...
				SetStateTryWaitSnk();
			else	// Otherwise we are going to the unattached state
				SetStateDelayUnattached();
		}
	}
}

void StateMachineTryWaitSnk(void)
{
	CCTermType CCValue = DecodeCCTermination();	// Grab the latest CC termination value
	if (Registers.Switches.MEAS_CC1)	// If we are looking at CC1
	{
		if (CC1TermAct != CCValue)	// Check to see if the value has changed...
		{
			CC1TermAct = CCValue;	// If it has, update the value
			DebounceTimer1 = tPDDebounceMin;	// Restart the debounce timer with tPDDebounce (wait 10ms before detach)
			FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
		}
	} else			// Otherwise we are looking at CC2
	{
		if (CC2TermAct != CCValue)	// Check to see if the value has changed...
		{
			CC2TermAct = CCValue;	// If it has, update the value
			DebounceTimer1 = tPDDebounceMin;	// Restart the debounce timer with tPDDebounce (wait 10ms before detach)
			FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
		}
	}
	if (DebounceTimer1 == 0)	// Check to see if our debounce timer has expired...
	{
		DebounceTimer1 = USHRT_MAX;	// If it has, disable it so we don't come back in here until we have debounced a change in state
		if ((CC1TermDeb != CC1TermAct) || (CC2TermDeb != CC2TermAct)) {
			DebounceTimer2 = tCCDebounceMin - tPDDebounceMin;	// Once the CC state is known, start the tCCDebounce timer to validate
			//FIXME  handle timer restart.
			FUSB302_start_timer(&debounce_hrtimer2, DebounceTimer2);
		}
		CC1TermDeb = CC1TermAct;	// Update the CC1 debounced value
		CC2TermDeb = CC2TermAct;	// Update the CC2 debounced value
	}
	if (ToggleTimer == 0)	// If are toggle timer has expired, it's time to swap detection
	{
		if (Registers.Switches.MEAS_CC1)	// If we are currently on the CC1 pin...
			ToggleMeasureCC2();	// Toggle over to look at CC2
		else		// Otherwise assume we are using the CC2...
			ToggleMeasureCC1();	// So toggle over to look at CC1
		ToggleTimer = tFUSB302Toggle;	// Reset the toggle timer to our default toggling (<tPDDebounce to avoid disconnecting the other side when we remove pull-ups)
		FUSB302_start_timer(&toggle_hrtimer, ToggleTimer);
	}
	if ((StateTimer == 0) && (CC1TermDeb == CCTypeRa) && (CC2TermDeb == CCTypeRa))	// If tDRPTryWait has expired and we detected open on both pins...
		SetStateDelayUnattached();	// Go to the unattached state
	else if (Registers.Status.VBUSOK && (DebounceTimer2 == 0))	// If we have detected VBUS and we have detected an Rp for >tCCDebounce...
	{
		if ((CC1TermDeb > CCTypeRa) && (CC2TermDeb == CCTypeRa))	// If Rp is detected on CC1
		{
			blnCCPinIsCC1 = true;	// Set the CC pin to CC1
			blnCCPinIsCC2 = false;	//
			SetStateAttachedSink();	// Go to the Attached.Snk state
		} else if ((CC1TermDeb == CCTypeRa) && (CC2TermDeb > CCTypeRa))	// If Rp is detected on CC2
		{
			blnCCPinIsCC1 = false;	//
			blnCCPinIsCC2 = true;	// Set the CC pin to CC2
			SetStateAttachedSink();	// Go to the Attached.Snk State
		}
	}
}

void StateMachineTrySrc(void)
{
	CCTermType CCValue = DecodeCCTermination();	// Grab the latest CC termination value
	if (Registers.Switches.MEAS_CC1)	// If we are looking at CC1
	{
		if (CC1TermAct != CCValue)	// Check to see if the value has changed...
		{
			CC1TermAct = CCValue;	// If it has, update the value
			DebounceTimer1 = tPDDebounceMin;	// Restart the debounce timer (tPDDebounce)
			FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
		}
	} else			// Otherwise we are looking at CC2
	{
		if (CC2TermAct != CCValue)	// Check to see if the value has changed...
		{
			CC2TermAct = CCValue;	// If it has, update the value
			DebounceTimer1 = tPDDebounceMin;	// Restart the debounce timer (tPDDebounce)
			FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
		}
	}
	if (DebounceTimer1 == 0)	// Check to see if our debounce timer has expired...
	{
		DebounceTimer1 = USHRT_MAX;	// If it has, disable it so we don't come back in here until a new debounce value is ready
		CC1TermDeb = CC1TermAct;	// Update the CC1 debounced value
		CC2TermDeb = CC2TermAct;	// Update the CC2 debounced value
	}
	if (ToggleTimer == 0)	// If are toggle timer has expired, it's time to swap detection
	{
		if (Registers.Switches.MEAS_CC1)	// If we are currently on the CC1 pin...
			ToggleMeasureCC2();	// Toggle over to look at CC2
		else		// Otherwise assume we are using the CC2...
			ToggleMeasureCC1();	// So toggle over to look at CC1
		ToggleTimer = tPDDebounceMax;	// Reset the toggle timer to the max tPDDebounce to ensure the other side sees the pull-up for the min tPDDebounce
		FUSB302_start_timer(&toggle_hrtimer, ToggleTimer);
	}
	if ((CC1TermDeb > CCTypeRa) && ((CC2TermDeb == CCTypeNone) || (CC2TermDeb == CCTypeRa)))	// If the CC1 pin is Rd for atleast tPDDebounce...
	{
		blnCCPinIsCC1 = true;	// The CC pin is CC1
		blnCCPinIsCC2 = false;
		SetStateAttachedSrc();	// Go to the Attached.Src state
	} else if ((CC2TermDeb > CCTypeRa) && ((CC1TermDeb == CCTypeNone) || (CC1TermDeb == CCTypeRa)))	// If the CC2 pin is Rd for atleast tPDDebounce...
	{
		blnCCPinIsCC1 = false;	// The CC pin is CC2
		blnCCPinIsCC2 = true;
		SetStateAttachedSrc();	// Go to the Attached.Src state
	} else if (StateTimer == 0)	// If we haven't detected Rd on exactly one of the pins and we have waited for tDRPTry...
		SetStateTryWaitSnk();	// Move onto the TryWait.Snk state to not get stuck in here
}

void StateMachineDebugAccessory(void)
{
	CCTermType CCValue = DecodeCCTermination();	// Grab the latest CC termination value
	if (CC1TermAct != CCValue)	// If the CC voltage has changed...
	{
		CC1TermAct = CCValue;	// Store the updated value
		DebounceTimer1 = tCCDebounceMin;	// Reset the debounce timer to the minimum tCCDebounce
		FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	} else if (DebounceTimer1 == 0)	// If the signal has been debounced
	{
		DebounceTimer1 = USHRT_MAX;	// Disable the debounce timer until we get a change
		CC1TermDeb = CC1TermAct;	// Store the debounced termination for CC1
	}
	if (CC1TermDeb == CCTypeNone)	// If we have detected an open for > tCCDebounce
		SetStateDelayUnattached();	// Go to the unattached state
	else if (CC1TermDeb == CCTypeRa)
		SetStateUnattached();
}

void StateMachineAudioAccessory(void)
{
	CCTermType CCValue = DecodeCCTermination();	// Grab the latest CC termination value
	if (CC1TermAct != CCValue)	// If the CC voltage has changed...
	{
		CC1TermAct = CCValue;	// Store the updated value
		DebounceTimer1 = tCCDebounceMin;	// Reset the debounce timer to the minimum tCCDebounce
		FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	} else if (DebounceTimer1 == 0)	// If the signal has been debounced
	{
		DebounceTimer1 = USHRT_MAX;	// Disable the debounce timer until we get a change
		CC1TermDeb = CC1TermAct;	// Store the debounced termination for CC1
	}
	if (CC1TermDeb == CCTypeNone)	// If we have detected an open for > tCCDebounce
		SetStateDelayUnattached();	// Go to the unattached state
	else if (CC1TermDeb > CCTypeRa) {
		SetStateUnattached();
	}
}

void StateMachinePoweredAccessory(void)
{
	CCTermType CCValue = DecodeCCTermination();	// Grab the latest CC termination value
	if (CC1TermAct != CCValue)	// If the CC voltage has changed...
	{
		CC1TermAct = CCValue;	// Store the updated value
		DebounceTimer1 = tPDDebounceMin;	// Reset the debounce timer to the minimum tPDdebounce
		FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	} else if (DebounceTimer1 == 0)	// If the signal has been debounced
	{
		DebounceTimer1 = USHRT_MAX;	// Disable the debounce timer until we get a change
		CC1TermDeb = CC1TermAct;	// Store the debounced termination for CC1
	}
	if (CC1TermDeb == CCTypeNone)	// If we have detected an open for > tCCDebounce
		SetStateDelayUnattached();	// Go to the unattached state
	else if (StateTimer == 0)	// If we have timed out (tAMETimeout) and haven't entered an alternate mode...
		SetStateUnsupportedAccessory();	// Go to the Unsupported.Accessory state
}

void StateMachineUnsupportedAccessory(void)
{
	CCTermType CCValue = DecodeCCTermination();	// Grab the latest CC termination value
	if (CC1TermAct != CCValue)	// If the CC voltage has changed...
	{
		CC1TermAct = CCValue;	// Store the updated value
		DebounceTimer1 = tPDDebounceMin;	// Reset the debounce timer to the minimum tPDDebounce
		FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	} else if (DebounceTimer1 == 0)	// If the signal has been debounced
	{
		DebounceTimer1 = USHRT_MAX;	// Disable the debounce timer until we get a change
		CC1TermDeb = CC1TermAct;	// Store the debounced termination for CC1
	}
	if (CC1TermDeb == CCTypeNone)	// If we have detected an open for > tCCDebounce
		SetStateDelayUnattached();	// Go to the unattached state
}

/////////////////////////////////////////////////////////////////////////////
//                      State Machine Configuration
/////////////////////////////////////////////////////////////////////////////

void SetStateDisabled(void)
{
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 0;		// Disable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	Registers.Power.PWR = 0x01;	// Enter low power state
	Registers.Control.TOGGLE = 0;	// Disable the toggle state machine
	Registers.Control.HOST_CUR = 0x00;	// Disable the currents for the pull-ups (not used for UFP)
	Registers.Switches.word = 0x0000;	// Disable all pull-ups and pull-downs on the CC pins and disable the BMC transmitters
	FUSB300Write(regPower, 1, &Registers.Power.byte);	// Commit the power state
	FUSB300Write(regControl0, 3, &Registers.Control.byte[0]);	// Commit the control state
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	// Commit the switch state
#ifdef PD_SUPPORT
	USBPDDisable(false);	// Disable the USB PD state machine (no need to write FUSB300 again since we are doing it here)
#endif
	CC1TermDeb = CCTypeNone;	// Clear the debounced CC1 state
	CC2TermDeb = CCTypeNone;	// Clear the debounced CC2 state
	CC1TermAct = CC1TermDeb;	// Clear the active CC1 state
	CC2TermAct = CC2TermDeb;	// Clear the active CC2 state
	blnCCPinIsCC1 = false;	// Clear the CC1 pin flag
	blnCCPinIsCC2 = false;	// Clear the CC2 pin flag
	ConnState = Disabled;	// Set the state machine variable to Disabled
	StateTimer = USHRT_MAX;	// Disable the state timer (not used in this state)
	DebounceTimer1 = USHRT_MAX;	// Disable the 1st level debounce timer
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debounce timer
	ToggleTimer = USHRT_MAX;	// Disable the toggle timer
	//wake_up_statemachine();
}

void SetStateErrorRecovery(void)
{
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 0;		// Disable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	Registers.Power.PWR = 0x01;	// Enter low power state
	Registers.Control.TOGGLE = 0;	// Disable the toggle state machine
	Registers.Control.HOST_CUR = 0x00;	// Disable the currents for the pull-ups (not used for UFP)
	Registers.Switches.word = 0x0000;	// Disable all pull-ups and pull-downs on the CC pins and disable the BMC transmitters
	FUSB300Write(regPower, 1, &Registers.Power.byte);	// Commit the power state
	FUSB300Write(regControl0, 3, &Registers.Control.byte[0]);	// Commit the control state
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	// Commit the switch state
#ifdef PD_SUPPORT
	USBPDDisable(false);	// Disable the USB PD state machine (no need to write FUSB300 again since we are doing it here)
#endif
	CC1TermDeb = CCTypeNone;	// Clear the debounced CC1 state
	CC2TermDeb = CCTypeNone;	// Clear the debounced CC2 state
	CC1TermAct = CC1TermDeb;	// Clear the active CC1 state
	CC2TermAct = CC2TermDeb;	// Clear the active CC2 state
	blnCCPinIsCC1 = false;	// Clear the CC1 pin flag
	blnCCPinIsCC2 = false;	// Clear the CC2 pin flag
	ConnState = ErrorRecovery;	// Set the state machine variable to ErrorRecovery
	StateTimer = tErrorRecovery;	// Load the tErrorRecovery duration into the state transition timer
	FUSB302_start_timer(&state_hrtimer, StateTimer);
	DebounceTimer1 = USHRT_MAX;	// Disable the 1st level debounce timer
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debounce timer
	ToggleTimer = USHRT_MAX;	// Disable the toggle timer
	wake_up_statemachine();
}

void SetStateDelayUnattached(void)
{
	// This state is only here because of the precision timing source we have with the FPGA
	// We are trying to avoid having the toggle state machines in sync with each other
	// Causing the tDRPAdvert period to overlap causing the devices to not attach for a period of time
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 0;		// Disable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	Registers.Power.PWR = 0x01;	// Enter low power state
	Registers.Control.TOGGLE = 0;	// Disable the toggle state machine
	Registers.Control.HOST_CUR = 0x00;	// Disable the currents for the pull-ups (not used for UFP)
	Registers.Switches.word = 0x0000;	// Disable all pull-ups and pull-downs on the CC pins and disable the BMC transmitters
	FUSB300Write(regPower, 1, &Registers.Power.byte);	// Commit the power state
	FUSB300Write(regControl0, 3, &Registers.Control.byte[0]);	// Commit the control state
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	// Commit the switch state
#ifdef PD_SUPPORT
	USBPDDisable(false);	// Disable the USB PD state machine (no need to write FUSB300 again since we are doing it here)
#endif
	/* Default Switches to OFF state */
	FUSB302_disableSuperspeedUSB();
	if (fusb_i2c_data->fsa321_switch)
		FSA321_setSwitchState(fsa_lpm);
	else
		FUSB302_toggleAudioSwitch(false);
#ifdef CONFIG_FSUSB42_MUX
	fsusb42_set_state(FSUSB_OFF);
#endif
	CC1TermDeb = CCTypeNone;	// Clear the debounced CC1 state
	CC2TermDeb = CCTypeNone;	// Clear the debounced CC2 state
	CC1TermAct = CC1TermDeb;	// Clear the active CC1 state
	CC2TermAct = CC2TermDeb;	// Clear the active CC2 state
	blnCCPinIsCC1 = false;	// Clear the CC1 pin flag
	blnCCPinIsCC2 = false;	// Clear the CC2 pin flag
	ConnState = DelayUnattached;	// Set the state machine variable to delayed unattached
	StateTimer = 10;	// Set the state timer to a random value to not synchronize the toggle start (use a multiple of RAND_MAX+1 as the modulus operator)
	FUSB302_start_timer(&state_hrtimer, StateTimer);
	DebounceTimer1 = USHRT_MAX;	// Disable the 1st level debounce timer
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debounce timer
	ToggleTimer = USHRT_MAX;	// Disable the toggle timer
	wake_up_statemachine();
}

void SetStateUnattached(void)
{
	// This function configures the Toggle state machine in the FUSB302 to handle all of the unattached states.
	// This allows for the MCU to be placed in a low power mode until the FUSB302 wakes it up upon detecting something
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 0;		// Disable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	Registers.Mask.byte = 0xFF;	// Disable all Mask Interrupts
	FUSB300Write(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = ~0x40;	// Enable only Toggle Interrupt
	FUSB300Write(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.Control.HOST_CUR = 0x01;	// Enable the defauult host current for the pull-ups (regardless of mode)
	Registers.Control.TOGGLE = 1;	// Enable the toggle
	if ((PortType == USBTypeC_DRP) || (blnAccSupport))	// If we are a DRP or supporting accessories
		Registers.Control.MODE = 0b01;	// We need to enable the toggling functionality for Rp/Rd
	else if (PortType == USBTypeC_Source)	// If we are strictly a Source
		Registers.Control.MODE = 0b11;	// We just need to look for Rd
	else			// Otherwise we are a UFP
		Registers.Control.MODE = 0b10;	// So we need to only look for Rp
	Registers.Switches.word = 0x0003;	// Enable the pull-downs on the CC pins, toggle overrides anyway
	Registers.Power.PWR = 0x07;	// Enable everything except internal oscillator
	Registers.Measure.MDAC = MDAC_2P05V;	// Set up DAC threshold to 2.05V
	FUSB300Write(regPower, 1, &Registers.Power.byte);	// Commit the power state
	FUSB300Write(regControl0, 3, &Registers.Control.byte[0]);	// Commit the control state
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	// Commit the switch state
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	// Commit the DAC threshold
#ifdef PD_SUPPORT
	USBPDDisable(false);	// Disable the USB PD state machine (no need to write FUSB300 again since we are doing it here)
#endif
	ConnState = Unattached;	// Set the state machine variable to unattached
	fusb_i2c_data->usbc_psy.type = POWER_SUPPLY_TYPE_USBC;
	power_supply_changed(&fusb_i2c_data->usbc_psy);
	SinkCurrent = utccNone;
	CC1TermDeb = CCTypeNone;	// Clear the termination for this state
	CC2TermDeb = CCTypeNone;	// Clear the termination for this state
	CC1TermAct = CC1TermDeb;
	CC2TermAct = CC2TermDeb;
	blnCCPinIsCC1 = false;	// Clear the CC1 pin flag
	blnCCPinIsCC2 = false;	// Clear the CC2 pin flag
	StateTimer = USHRT_MAX;	// Disable the state timer, not used in this state
	DebounceTimer1 = USHRT_MAX;	// Disable the 1st level debounce timer, not used in this state
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debounce timer, not used in this state
	ToggleTimer = USHRT_MAX;	// Disable the toggle timer, not used in this state
	if (fusb_i2c_data->fsa321_switch)
		FSA321_setSwitchState(fsa_lpm);
	else
		FUSB302_toggleAudioSwitch(false);
	wake_up_statemachine();
}

void SetStateAttachWaitSnk(void)
{
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 0;		// Disable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	Registers.Mask.byte = 0x44;	// Disable Activity and Wake Interrupts
	FUSB300Write(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0x00;	// Enable all Interrupts
	FUSB300Write(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.Power.PWR = 0x07;	// Enable everything except internal oscillator
	Registers.Switches.word = 0x0003;	// Enable the pull-downs on the CC pins
	if (blnCCPinIsCC1)
		Registers.Switches.MEAS_CC1 = 1;
	else
		Registers.Switches.MEAS_CC2 = 1;
	Registers.Measure.MDAC = MDAC_2P05V;	// Set up DAC threshold to 2.05V
	Registers.Control.HOST_CUR = 0x00;	// Disable the host current
	Registers.Control.TOGGLE = 0;	// Disable the toggle
	FUSB300Write(regPower, 1, &Registers.Power.byte);	// Commit the power state
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	// Commit the switch state
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	// Commit the DAC threshold
	FUSB300Write(regControl0, 3, &Registers.Control.byte[0]);	// Commit the host current
	Delay10us(25);		// Delay the reading of the COMP and BC_LVL to allow time for settling
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the current state of the BC_LVL and COMP
	ConnState = AttachWaitSink;	// Set the state machine variable to AttachWait.Snk
	fusb_i2c_data->usbc_psy.type = POWER_SUPPLY_TYPE_USBC;
	SinkCurrent = utccNone;	// Set the current advertisment variable to none until we determine what the current is
	if (Registers.Switches.MEAS_CC1)	// If CC1 is what initially got us into the wait state...
	{
		CC1TermAct = DecodeCCTermination();	// Determine what is initially on CC1
		CC2TermAct = CCTypeNone;	// Put something that we shouldn't see on the CC2 to force a debouncing
	} else {
		CC1TermAct = CCTypeNone;	// Put something that we shouldn't see on the CC1 to force a debouncing
		CC2TermAct = DecodeCCTermination();	// Determine what is initially on CC2
	}
	CC1TermDeb = CCTypeNone;	// Initially set to invalid
	CC2TermDeb = CCTypeNone;	// Initially set to invalid
	StateTimer = USHRT_MAX;	// Disable the state timer, not used in this state
	DebounceTimer1 = tPDDebounceMax;	// Set the tPDDebounce for validating signals to transition to
	FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debouncing until the first level has been debounced
	ToggleTimer = tFUSB302Toggle;	// Set the toggle timer to look at each pin for tFUSB302Toggle duration
	FUSB302_start_timer(&toggle_hrtimer, ToggleTimer);
	wake_up_statemachine();
}

void SetStateAttachWaitSrc(void)
{
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 0;		// Disable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	Registers.Mask.byte = 0x44;	// Disable Activity and Wake Interrupts
	FUSB300Write(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0x00;	// Enable all Interrupts
	FUSB300Write(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.Power.PWR = 0x07;	// Enable everything except internal oscillator
	Registers.Switches.word = 0x0000;	// Clear the register for the case below
	if (blnCCPinIsCC1)	// If we detected CC1 as an Rd
		Registers.Switches.word = 0x0044;	// Enable CC1 pull-up and measure
	else
		Registers.Switches.word = 0x0088;	// Enable CC2 pull-up and measure
	SourceCurrent = utccDefault;	// Set the default current level
	UpdateSourcePowerMode();	// Update the settings for the FUSB302
	Registers.Control.TOGGLE = 0;	// Disable the toggle
	FUSB300Write(regPower, 1, &Registers.Power.byte);	// Commit the power state
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	// Commit the switch state
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	// Commit the DAC threshold
	FUSB300Write(regControl2, 1, &Registers.Control.byte[2]);	// Commit the toggle
	Delay10us(25);		// Delay the reading of the COMP and BC_LVL to allow time for settling
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the current state of the BC_LVL and COMP
	ConnState = AttachWaitSource;	// Set the state machine variable to AttachWait.Src
	fusb_i2c_data->usbc_psy.type = POWER_SUPPLY_TYPE_USBC;
	SinkCurrent = utccNone;	// Not used in Src
	if (Registers.Switches.MEAS_CC1)	// If CC1 is what initially got us into the wait state...
	{
		CC1TermAct = DecodeCCTermination();	// Determine what is initially on CC1
		CC2TermAct = CCTypeNone;	// Assume that the initial value on CC2 is open
	} else {
		CC1TermAct = CCTypeNone;	// Assume that the initial value on CC1 is open
		CC2TermAct = DecodeCCTermination();	// Determine what is initially on CC2
	}
	CC1TermDeb = CCTypeRa;	// Initially set both the debounced values to Ra to force the 2nd level debouncing
	CC2TermDeb = CCTypeRa;	// Initially set both the debounced values to Ra to force the 2nd level debouncing
	StateTimer = USHRT_MAX;	// Disable the state timer, not used in this state
	DebounceTimer1 = tPDDebounceMin;	// Only debounce the lines for tPDDebounce so that we can debounce a detach condition
	FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debouncing initially to force completion of a 1st level debouncing
	ToggleTimer = tDRP;	// Set the initial toggle time to tDRP to ensure the other end sees the Rp
	FUSB302_start_timer(&toggle_hrtimer, ToggleTimer);
	wake_up_statemachine();
}

void SetStateAttachWaitAcc(void)
{
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 0;		// Disable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	Registers.Mask.byte = 0x44;	// Disable Activity and Wake Interrupts
	FUSB300Write(regMask, 1, &Registers.Mask.byte);
	Registers.MaskAdv.byte[0] = 0x00;	// Enable all Interrupts
	FUSB300Write(regMaska, 1, &Registers.MaskAdv.byte[0]);
	Registers.Power.PWR = 0x07;	// Enable everything except internal oscillator
	Registers.Switches.word = 0x0044;	// Enable CC1 pull-up and measure
	UpdateSourcePowerMode();
	Registers.Control.TOGGLE = 0;	// Disable the toggle
	FUSB300Write(regPower, 1, &Registers.Power.byte);	// Commit the power state
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	// Commit the switch state
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	// Commit the DAC threshold
	FUSB300Write(regControl2, 1, &Registers.Control.byte[2]);	// Commit the toggle
	Delay10us(25);		// Delay the reading of the COMP and BC_LVL to allow time for settling
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the current state of the BC_LVL and COMP
	ConnState = AttachWaitAccessory;	// Set the state machine variable to AttachWait.Accessory
	fusb_i2c_data->usbc_psy.type = POWER_SUPPLY_TYPE_USBC;
	SinkCurrent = utccNone;	// Not used in accessories
	CC1TermAct = DecodeCCTermination();	// Determine what is initially on CC1
	CC2TermAct = CCTypeNone;	// Assume that the initial value on CC2 is open
	CC1TermDeb = CCTypeNone;	// Initialize to open
	CC2TermDeb = CCTypeNone;	// Initialize to open
	StateTimer = USHRT_MAX;	// Disable the state timer, not used in this state
	DebounceTimer1 = tCCDebounceNom;	// Once in this state, we are waiting for the lines to be stable for tCCDebounce before changing states
	FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debouncing initially to force completion of a 1st level debouncing
	ToggleTimer = tFUSB302Toggle;	// We're looking for the status of both lines of an accessory, no need to keep the line pull-ups on for tPDDebounce
	FUSB302_start_timer(&toggle_hrtimer, ToggleTimer);
	wake_up_statemachine();
}

void SetStateAttachedSrc(void)
{
	struct power_supply *usb_psy = power_supply_get_by_name("usb");
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 1;		// Enable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	SourceCurrent = utccDefault;	// Reset the current to the default advertisement
	UpdateSourcePowerMode();	// Update the source power mode
	if (blnCCPinIsCC1 == true)	// If CC1 is detected as the CC pin...
		Registers.Switches.word = 0x0064;	// Configure VCONN on CC2, pull-up on CC1, measure CC1
	else			// Otherwise we are assuming CC2 is CC
		Registers.Switches.word = 0x0098;	// Configure VCONN on CC1, pull-up on CC2, measure CC2
	Registers.Power.PWR = 0x07;	// Enable everything except internal oscillator
#ifdef PD_SUPPORT
	USBPDEnable(false, true);	// Enable the USB PD state machine if applicable (no need to write to FUSB300 again), set as DFP
#endif
	FUSB300Write(regPower, 1, &Registers.Power.byte);	// Commit the power state
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	// Commit the switch state
	Delay10us(25);		// Delay the reading of the COMP and BC_LVL to allow time for settling
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the current state of the BC_LVL and COMP
	// Maintain the existing CC term values from the wait state
	ConnState = AttachedSource;	// Set the state machine variable to Attached.Src
	fusb_i2c_data->usbc_psy.type = POWER_SUPPLY_TYPE_USBC_SRC;
	power_supply_changed(&fusb_i2c_data->usbc_psy);
	/* Enable SSUSB Switch and set orientation */
	FUSB302_enableSuperspeedUSB(blnCCPinIsCC1, blnCCPinIsCC2);
	if (fusb_i2c_data->fsa321_switch)
		FSA321_setSwitchState(fsa_usb_mode);

#ifdef CONFIG_FSUSB42_MUX
	fsusb42_set_state(FSUSB_STATE_USB);
#endif
	/* Notify USB driver to switch to host mode */
	if (usb_psy)
		power_supply_set_usb_otg(usb_psy, 1);
	SinkCurrent = utccNone;	// Set the Sink current to none (not used in source)
	StateTimer = USHRT_MAX;	// Disable the state timer, not used in this state
	DebounceTimer1 = tPDDebounceMin;	// Set the debounce timer to tPDDebounceMin for detecting a detach
	FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debouncing, not needed in this state
	ToggleTimer = USHRT_MAX;	// Disable the toggle timer, not used in this state
	wake_up_statemachine();
}

void SetStateAttachedSink(void)
{
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 0;		// Disable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	Registers.Power.PWR = 0x07;	// Enable everything except internal oscillator
	Registers.Control.HOST_CUR = 0x00;	// Disable the host current
	Registers.Measure.MDAC = MDAC_2P05V;	// Set up DAC threshold to 2.05V
//    Registers.Switches.word = 0x0007;                               // Enable the pull-downs on the CC pins, measure CC1 and disable the BMC transmitters
	Registers.Switches.word = 0x0003;	// Enable the pull-downs on the CC pins
	if (blnCCPinIsCC1)
		Registers.Switches.MEAS_CC1 = 1;
	else
		Registers.Switches.MEAS_CC2 = 1;
#ifdef PD_SUPPORT
	USBPDEnable(false, false);	// Enable the USB PD state machine (no need to write FUSB300 again since we are doing it here)
#endif
	FUSB300Write(regPower, 1, &Registers.Power.byte);	// Commit the power state
	FUSB300Write(regControl0, 1, &Registers.Control.byte[0]);	// Commit the host current
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	// Commit the DAC threshold
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	// Commit the switch state
	Delay10us(25);		// Delay the reading of the COMP and BC_LVL to allow time for settling
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the current state of the BC_LVL and COMP
	ConnState = AttachedSink;	// Set the state machine variable to Attached.Sink
	fusb_i2c_data->usbc_psy.type = POWER_SUPPLY_TYPE_USBC_SINK;
	power_supply_changed(&fusb_i2c_data->usbc_psy);
	SinkCurrent = utccDefault;	// Set the current advertisment variable to the default until we detect something different
	/* Enable SSUSB Switch and set orientation */
	FUSB302_enableSuperspeedUSB(blnCCPinIsCC1, blnCCPinIsCC2);
	if (fusb_i2c_data->fsa321_switch)
		FSA321_setSwitchState(fsa_usb_mode);
#ifdef CONFIG_FSUSB42_MUX
	fsusb42_set_state(FSUSB_STATE_USB);
#endif
	// Maintain the existing CC term values from the wait state
	StateTimer = USHRT_MAX;	// Disable the state timer, not used in this state
	DebounceTimer1 = tPDDebounceMin;	// Set the debounce timer to tPDDebounceMin for detecting changes in advertised current
	FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debounce timer, not used in this state
	ToggleTimer = USHRT_MAX;	// Disable the toggle timer, not used in this state
	wake_up_statemachine();
}

void RoleSwapToAttachedSink(void)
{
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 0;		// Disable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	Registers.Control.HOST_CUR = 0x00;	// Disable the host current
	Registers.Measure.MDAC = MDAC_2P05V;	// Set up DAC threshold to 2.05V
	if (blnCCPinIsCC1)	// If the CC pin is CC1...
	{
		Registers.Switches.PU_EN1 = 0;	// Disable the pull-up on CC1
		Registers.Switches.PDWN1 = 1;	// Enable the pull-down on CC1
		// No change for CC2, it may be used as VCONN
		CC1TermAct = CCTypeRa;	// Initialize the CC term as open
		CC1TermDeb = CCTypeRa;	// Initialize the CC term as open
	} else {
		Registers.Switches.PU_EN2 = 0;	// Disable the pull-up on CC2
		Registers.Switches.PDWN2 = 1;	// Enable the pull-down on CC2
		// No change for CC1, it may be used as VCONN
		CC2TermAct = CCTypeRa;	// Initialize the CC term as open
		CC2TermDeb = CCTypeRa;	// Initialize the CC term as open
	}
	FUSB300Write(regControl0, 1, &Registers.Control.byte[0]);	// Commit the host current
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	// Commit the DAC threshold
	FUSB300Write(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the switch state
	Delay10us(25);		// Delay the reading of the COMP and BC_LVL to allow time for settling
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the current state of the BC_LVL and COMP
	ConnState = AttachedSink;	// Set the state machine variable to Attached.Sink
	SinkCurrent = utccNone;	// Set the current advertisment variable to none until we determine what the current is
	StateTimer = USHRT_MAX;	// Disable the state timer, not used in this state
	DebounceTimer1 = tPDDebounceMin;	// Set the debounce timer to tPDDebounceMin for detecting changes in advertised current
	FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debounce timer, not used in this state
	ToggleTimer = USHRT_MAX;	// Disable the toggle timer, not used in this state
	wake_up_statemachine();
}

void RoleSwapToAttachedSource(void)
{
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 1;		// Enable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	UpdateSourcePowerMode();	// Update the pull-up currents and measure block
	if (blnCCPinIsCC1)	// If the CC pin is CC1...
	{
		Registers.Switches.PU_EN1 = 1;	// Enable the pull-up on CC1
		Registers.Switches.PDWN1 = 0;	// Disable the pull-down on CC1
		// No change for CC2, it may be used as VCONN
		CC1TermAct = CCTypeNone;	// Initialize the CC term as open
		CC1TermDeb = CCTypeNone;	// Initialize the CC term as open
	} else {
		Registers.Switches.PU_EN2 = 1;	// Enable the pull-up on CC2
		Registers.Switches.PDWN2 = 0;	// Disable the pull-down on CC2
		// No change for CC1, it may be used as VCONN
		CC2TermAct = CCTypeNone;	// Initialize the CC term as open
		CC2TermDeb = CCTypeNone;	// Initialize the CC term as open
	}
	FUSB300Write(regSwitches0, 1, &Registers.Switches.byte[0]);	// Commit the switch state
	Delay10us(25);		// Delay the reading of the COMP and BC_LVL to allow time for settling
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the current state of the BC_LVL and COMP
	ConnState = AttachedSource;	// Set the state machine variable to Attached.Src
	SinkCurrent = utccNone;	// Set the Sink current to none (not used in Src)
	StateTimer = USHRT_MAX;	// Disable the state timer, not used in this state
	DebounceTimer1 = tPDDebounceMin;	// Set the debounce timer to tPDDebounceMin for detecting a detach
	FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debouncing, not needed in this state
	ToggleTimer = USHRT_MAX;	// Disable the toggle timer, not used in this state
	wake_up_statemachine();
}

void SetStateTryWaitSnk(void)
{
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 0;		// Disable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	Registers.Switches.word = 0x0007;	// Enable the pull-downs on the CC pins and measure on CC1
	Registers.Power.PWR = 0x07;	// Enable everything except internal oscillator
	Registers.Measure.MDAC = MDAC_2P05V;	// Set up DAC threshold to 2.05V
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	// Commit the switch state
	FUSB300Write(regPower, 1, &Registers.Power.byte);	// Commit the power state
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	// Commit the DAC threshold
	Delay10us(25);		// Delay the reading of the COMP and BC_LVL to allow time for settling
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the current state of the BC_LVL and COMP
	ConnState = TryWaitSink;	// Set the state machine variable to TryWait.Snk
	fusb_i2c_data->usbc_psy.type = POWER_SUPPLY_TYPE_USBC;
	SinkCurrent = utccNone;	// Set the current advertisment variable to none until we determine what the current is
	if (Registers.Switches.MEAS_CC1) {
		CC1TermAct = DecodeCCTermination();	// Determine what is initially on CC1
		CC2TermAct = CCTypeNone;	// Assume that the initial value on CC2 is open
	} else {
		CC1TermAct = CCTypeNone;	// Assume that the initial value on CC1 is open
		CC2TermAct = DecodeCCTermination();	// Determine what is initially on CC2
	}
	CC1TermDeb = CCTypeNone;	// Initially set the debounced value to none so we don't immediately detach
	CC2TermDeb = CCTypeNone;	// Initially set the debounced value to none so we don't immediately detach
	StateTimer = tDRPTryWait;	// Set the state timer to tDRPTryWait to timeout if Rp isn't detected
	FUSB302_start_timer(&state_hrtimer, StateTimer);
	DebounceTimer1 = tPDDebounceMin;	// The 1st level debouncing is based upon tPDDebounce
	FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debouncing initially until we validate the 1st level
	ToggleTimer = tFUSB302Toggle;	// Toggle the measure quickly (tFUSB302Toggle) to see if we detect an Rp on either
	FUSB302_start_timer(&toggle_hrtimer, ToggleTimer);
	wake_up_statemachine();
}

void SetStateTrySrc(void)
{
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 0;		// Disable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	SourceCurrent = utccDefault;	// Reset the current to the default advertisement
	Registers.Power.PWR = 0x07;	// Enable everything except internal oscillator
	Registers.Switches.word = 0x0000;	// Disable everything (toggle overrides anyway)
	if (blnCCPinIsCC1)	// If we detected CC1 as an Rd
	{
		Registers.Switches.PU_EN1 = 1;	// Enable the pull-up on CC1
		Registers.Switches.MEAS_CC1 = 1;	// Measure on CC1
	} else {
		Registers.Switches.PU_EN2 = 1;	// Enable the pull-up on CC1\2
		Registers.Switches.MEAS_CC2 = 1;	// Measure on CC2
	}
	UpdateSourcePowerMode();
	FUSB300Write(regPower, 1, &Registers.Power.byte);	// Commit the power state
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	// Commit the switch state
	Delay10us(25);		// Delay the reading of the COMP and BC_LVL to allow time for settling
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the current state of the BC_LVL and COMP
	ConnState = TrySource;	// Set the state machine variable to Try.Src
	fusb_i2c_data->usbc_psy.type = POWER_SUPPLY_TYPE_USBC;
	SinkCurrent = utccNone;	// Not used in Try.Src
	blnCCPinIsCC1 = false;	// Clear the CC1 is CC flag (don't know)
	blnCCPinIsCC2 = false;	// Clear the CC2 is CC flag (don't know)
	if (Registers.Switches.MEAS_CC1) {
		CC1TermAct = DecodeCCTermination();	// Determine what is initially on CC1
		CC2TermAct = CCTypeNone;	// Assume that the initial value on CC2 is open
	} else {
		CC1TermAct = CCTypeNone;	// Assume that the initial value on CC1 is open
		CC2TermAct = DecodeCCTermination();	// Determine what is initially on CC2
	}
	CC1TermDeb = CCTypeNone;	// Initially set the debounced value as open until we actually debounce the signal
	CC2TermDeb = CCTypeNone;	// Initially set both the active and debounce the same
	StateTimer = tDRPTry;	// Set the state timer to tDRPTry to timeout if Rd isn't detected
	FUSB302_start_timer(&state_hrtimer, StateTimer);
	DebounceTimer1 = tPDDebounceMin;	// Debouncing is based soley off of tPDDebounce
	FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level since it's not needed
	ToggleTimer = tPDDebounceMax;	// Keep the pull-ups on for the max tPDDebounce to ensure that the other side acknowledges the pull-up
	FUSB302_start_timer(&toggle_hrtimer, ToggleTimer);
	wake_up_statemachine();
}

void SetStateDebugAccessory(void)
{
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 0;		// Disable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	Registers.Power.PWR = 0x07;	// Enable everything except internal oscillator
	Registers.Switches.word = 0x0044;	// Enable CC1 pull-up and measure
	UpdateSourcePowerMode();
	FUSB300Write(regPower, 1, &Registers.Power.byte);	// Commit the power state
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	// Commit the switch state
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	// Commit the DAC threshold
	Delay10us(25);		// Delay the reading of the COMP and BC_LVL to allow time for settling
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the current state of the BC_LVL and COMP
	ConnState = DebugAccessory;	// Set the state machine variable to Debug.Accessory
	fusb_i2c_data->usbc_psy.type = POWER_SUPPLY_TYPE_USBC_DBG;
	power_supply_changed(&fusb_i2c_data->usbc_psy);
	SinkCurrent = utccNone;	// Not used in accessories
	// Maintain the existing CC term values from the wait state
	StateTimer = USHRT_MAX;	// Disable the state timer, not used in this state
	DebounceTimer1 = tCCDebounceNom;	// Once in this state, we are waiting for the lines to be stable for tCCDebounce before changing states
	FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debouncing initially to force completion of a 1st level debouncing
	ToggleTimer = USHRT_MAX;	// Once we are in the debug.accessory state, we are going to stop toggling and only monitor CC1
	if (fusb_i2c_data->fsa321_switch)
		FSA321_setSwitchState(fsa_usb_mode);
	wake_up_statemachine();
}

void SetStateAudioAccessory(void)
{
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 0;		// Disable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	Registers.Power.PWR = 0x07;	// Enable everything except internal oscillator
	Registers.Switches.word = 0x0044;	// Enable CC1 pull-up and measure
	UpdateSourcePowerMode();
	FUSB300Write(regPower, 1, &Registers.Power.byte);	// Commit the power state
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	// Commit the switch state
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	// Commit the DAC threshold
	Delay10us(25);		// Delay the reading of the COMP and BC_LVL to allow time for settling
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the current state of the BC_LVL and COMP
	ConnState = AudioAccessory;	// Set the state machine variable to Audio.Accessory
	fusb_i2c_data->usbc_psy.type = POWER_SUPPLY_TYPE_USBC_AUDIO;
	power_supply_changed(&fusb_i2c_data->usbc_psy);
	SinkCurrent = utccNone;	// Not used in accessories
	// Maintain the existing CC term values from the wait state
	StateTimer = USHRT_MAX;	// Disable the state timer, not used in this state
	DebounceTimer1 = tCCDebounceNom;	// Once in this state, we are waiting for the lines to be stable for tCCDebounce before changing states
	FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debouncing initially to force completion of a 1st level debouncing
	ToggleTimer = USHRT_MAX;	// Once we are in the audio.accessory state, we are going to stop toggling and only monitor CC1
	/* Turn on Audio Switch and notify headset detection */
	if (fusb_i2c_data->fsa321_switch)
		FSA321_setSwitchState(fsa_audio_mode);
	else
		FUSB302_toggleAudioSwitch(true);
	wake_up_statemachine();
}

void SetStatePoweredAccessory(void)
{
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 0;		// Disable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	SourceCurrent = utcc1p5A;	// Have the option of 1.5A/3.0A for powered accessories, choosing 1.5A advert
	UpdateSourcePowerMode();	// Update the Source Power mode
	if (blnCCPinIsCC1 == true)	// If CC1 is detected as the CC pin...
		Registers.Switches.word = 0x0064;	// Configure VCONN on CC2, pull-up on CC1, measure CC1
	else			// Otherwise we are assuming CC2 is CC
		Registers.Switches.word = 0x0098;	// Configure VCONN on CC1, pull-up on CC2, measure CC2
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	// Commit the switch state
	Delay10us(25);		// Delay the reading of the COMP and BC_LVL to allow time for settling
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the current state of the BC_LVL and COMP
	// Maintain the existing CC term values from the wait state
	// TODO: The line below will be uncommented once we have full support for VDM's and can enter an alternate mode as needed for Powered.Accessories
	// USBPDEnable(true, true);
	ConnState = PoweredAccessory;	// Set the state machine variable to powered.accessory
	fusb_i2c_data->usbc_psy.type = POWER_SUPPLY_TYPE_USBC_PWR;
	power_supply_changed(&fusb_i2c_data->usbc_psy);
	SinkCurrent = utccNone;	// Set the Sink current to none (not used in source)
	StateTimer = tAMETimeout;	// Set the state timer to tAMETimeout (need to enter alternate mode by this time)
	FUSB302_start_timer(&state_hrtimer, StateTimer);
	DebounceTimer1 = tPDDebounceMin;	// Set the debounce timer to the minimum tPDDebounce to check for detaches
	FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debounce timer, not used in this state
	ToggleTimer = USHRT_MAX;	// Disable the toggle timer, only looking at the actual CC line
	wake_up_statemachine();
}

void SetStateUnsupportedAccessory(void)
{
	FUSB_LOG("enter:%s\n", __func__);
	VBUS_5V_EN = 0;		// Disable the 5V output...
	VBUS_12V_EN = 0;	// Disable the 12V output
	SourceCurrent = utccDefault;	// Reset the current to the default advertisement for this state
	UpdateSourcePowerMode();	// Update the Source Power mode
	Registers.Switches.VCONN_CC1 = 0;	// Make sure VCONN is turned off
	Registers.Switches.VCONN_CC2 = 0;	// Make sure VCONN is turned off
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);	// Commit the switch state
	Delay10us(25);		// Delay the reading of the COMP and BC_LVL to allow time for settling
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read the current state of the BC_LVL and COMP
	ConnState = UnsupportedAccessory;	// Set the state machine variable to unsupported.accessory
	fusb_i2c_data->usbc_psy.type = POWER_SUPPLY_TYPE_USBC_UNSUPP;
	power_supply_changed(&fusb_i2c_data->usbc_psy);
	SinkCurrent = utccNone;	// Set the Sink current to none (not used in source)
	StateTimer = USHRT_MAX;	// Disable the state timer, not used in this state
	DebounceTimer1 = tPDDebounceMin;	// Set the debounce timer to the minimum tPDDebounce to check for detaches
	FUSB302_start_timer(&debounce_hrtimer1, DebounceTimer1);
	DebounceTimer2 = USHRT_MAX;	// Disable the 2nd level debounce timer, not used in this state
	ToggleTimer = USHRT_MAX;	// Disable the toggle timer, only looking at the actual CC line
	wake_up_statemachine();
}

void UpdateSourcePowerMode(void)
{
	switch (SourceCurrent) {
	case utccDefault:
		Registers.Measure.MDAC = MDAC_1P6V;	// Set up DAC threshold to 1.6V (default USB current advertisement)
		Registers.Control.HOST_CUR = 0x01;	// Set the host current to reflect the default USB power
		break;
	case utcc1p5A:
		Registers.Measure.MDAC = MDAC_1P6V;	// Set up DAC threshold to 1.6V
		Registers.Control.HOST_CUR = 0x02;	// Set the host current to reflect 1.5A
		break;
	case utcc3p0A:
		Registers.Measure.MDAC = MDAC_2P6V;	// Set up DAC threshold to 2.6V
		Registers.Control.HOST_CUR = 0x03;	// Set the host current to reflect 3.0A
		break;
	default:		// This assumes that there is no current being advertised
		Registers.Measure.MDAC = MDAC_1P6V;	// Set up DAC threshold to 1.6V (default USB current advertisement)
		Registers.Control.HOST_CUR = 0x00;	// Set the host current to disabled
		break;
	}
	FUSB300Write(regMeasure, 1, &Registers.Measure.byte);	// Commit the DAC threshold
	FUSB300Write(regControl0, 1, &Registers.Control.byte[0]);	// Commit the host current
}

/////////////////////////////////////////////////////////////////////////////
//                        Type C Support Routines
/////////////////////////////////////////////////////////////////////////////

void ToggleMeasureCC1(void)
{
	FUSB_LOG("%s\n", __func__);
	Registers.Switches.PU_EN1 = Registers.Switches.PU_EN2;	// If the pull-up was enabled on CC2, enable it for CC1
	Registers.Switches.PU_EN2 = 0;	// Disable the pull-up on CC2 regardless, since we aren't measuring CC2 (prevent short)
	Registers.Switches.MEAS_CC1 = 1;	// Set CC1 to measure
	Registers.Switches.MEAS_CC2 = 0;	// Clear CC2 from measuring
	FUSB300Write(regSwitches0, 1, &Registers.Switches.byte[0]);	// Set the switch to measure
	Delay10us(25);		// Delay the reading of the COMP and BC_LVL to allow time for settling
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read back the status to get the current COMP and BC_LVL
}

void ToggleMeasureCC2(void)
{
	FUSB_LOG("%s\n", __func__);
	Registers.Switches.PU_EN2 = Registers.Switches.PU_EN1;	// If the pull-up was enabled on CC1, enable it for CC2
	Registers.Switches.PU_EN1 = 0;	// Disable the pull-up on CC1 regardless, since we aren't measuring CC1 (prevent short)
	Registers.Switches.MEAS_CC1 = 0;	// Clear CC1 from measuring
	Registers.Switches.MEAS_CC2 = 1;	// Set CC2 to measure
	FUSB300Write(regSwitches0, 1, &Registers.Switches.byte[0]);	// Set the switch to measure
	Delay10us(25);		// Delay the reading of the COMP and BC_LVL to allow time for settling
	FUSB300Read(regStatus0, 2, &Registers.Status.byte[4]);	// Read back the status to get the current COMP and BC_LVL
}

CCTermType DecodeCCTermination(void)
{
	CCTermType Termination = CCTypeNone;	// By default set it to nothing
	if (Registers.Status.COMP == 0)	// If COMP is high, the BC_LVL's don't matter
	{
		switch (Registers.Status.BC_LVL)	// Determine which level
		{
		case 0b00:	// If BC_LVL is lowest... it's an vRa
			Termination = CCTypeRa;
			break;
		case 0b01:	// If BC_LVL is 1, it's default
			Termination = CCTypeRdUSB;
			break;
		case 0b10:	// If BC_LVL is 2, it's vRd1p5
			Termination = CCTypeRd1p5;
			break;
		default:	// Otherwise it's vRd3p0
			Termination = CCTypeRd3p0;
			break;
		}
	}
	return Termination;	// Return the termination type
}

void UpdateSinkCurrent(CCTermType Termination)
{
	switch (Termination) {
	case CCTypeRdUSB:	// If we detect the default...
	case CCTypeRa:		// Or detect an accessory (vRa)
		SinkCurrent = utccDefault;
		break;
	case CCTypeRd1p5:	// If we detect 1.5A
		SinkCurrent = utcc1p5A;
		break;
	case CCTypeRd3p0:	// If we detect 3.0A
		SinkCurrent = utcc3p0A;
		break;
	default:
		SinkCurrent = utccNone;
		break;
	}
	power_supply_changed(&fusb_i2c_data->usbc_psy);
}

/////////////////////////////////////////////////////////////////////////////
//                     Externally Accessible Routines
/////////////////////////////////////////////////////////////////////////////

void ConfigurePortType(unsigned char Control)
{
	unsigned char value;
	DisableFUSB300StateMachine();
	value = Control & 0x03;
	switch (value) {
	case 1:
		PortType = USBTypeC_Source;
		break;
	case 2:
		PortType = USBTypeC_DRP;
		break;
	default:
		PortType = USBTypeC_Sink;
		break;
	}
	if (Control & 0x04)
		blnAccSupport = true;
	else
		blnAccSupport = false;
	if (Control & 0x08)
		blnSrcPreferred = true;
	else
		blnSrcPreferred = false;
	value = (Control & 0x30) >> 4;
	switch (value) {
	case 1:
		SourceCurrent = utccDefault;
		break;
	case 2:
		SourceCurrent = utcc1p5A;
		break;
	case 3:
		SourceCurrent = utcc3p0A;
		break;
	default:
		SourceCurrent = utccNone;
		break;
	}
	if (Control & 0x80)
		EnableFUSB300StateMachine();
}

void UpdateCurrentAdvert(unsigned char Current)
{
	switch (Current) {
	case 1:
		SourceCurrent = utccDefault;
		break;
	case 2:
		SourceCurrent = utcc1p5A;
		break;
	case 3:
		SourceCurrent = utcc3p0A;
		break;
	default:
		SourceCurrent = utccNone;
		break;
	}
	if (ConnState == AttachedSource)
		UpdateSourcePowerMode();
}

void GetFUSB300TypeCStatus(unsigned char abytData[])
{
	int intIndex = 0;
	abytData[intIndex++] = GetTypeCSMControl();	// Grab a snapshot of the top level control
	abytData[intIndex++] = ConnState & 0xFF;	// Get the current state
	abytData[intIndex++] = GetCCTermination();	// Get the current CC termination
	abytData[intIndex++] = SinkCurrent;	// Set the sink current capability detected
}

unsigned char GetTypeCSMControl(void)
{
	unsigned char status = 0;
	status |= (PortType & 0x03);	// Set the type of port that we are configured as
	switch (PortType)	// Set the port type that we are configured as
	{
	case USBTypeC_Source:
		status |= 0x01;	// Set Source type
		break;
	case USBTypeC_DRP:
		status |= 0x02;	// Set DRP type
		break;
	default:		// If we are not DRP or Source, we are Sink which is a value of zero as initialized
		break;
	}
	if (blnAccSupport)	// Set the flag if we support accessories
		status |= 0x04;
	if (blnSrcPreferred)	// Set the flag if we prefer Source mode (as a DRP)
		status |= 0x08;
	status |= (SourceCurrent << 4);
	if (blnSMEnabled)	// Set the flag if the state machine is enabled
		status |= 0x80;
	return status;
}

unsigned char GetCCTermination(void)
{
	unsigned char status = 0;
	status |= (CC1TermDeb & 0x07);	// Set the current CC1 termination
//    if (blnCC1Debounced)                    // Set the flag if the CC1 pin has been debounced
//        status |= 0x08;
	status |= ((CC2TermDeb & 0x07) << 4);	// Set the current CC2 termination
//    if (blnCC2Debounced)                    // Set the flag if the CC2 pin has been debounced
//        status |= 0x80;
	return status;
}

static irqreturn_t cc_eint_interrupt_handler(int irq, void *data)
{
	FUSB_LOG("%s\n", __func__);
	wake_up_statemachine();
	return IRQ_HANDLED;
}

static void fusb_eint_work(struct work_struct *work)
{
	FUSB_LOG("%s\n", __func__);
	wake_up_statemachine();
}

int fusb302_state_kthread(void *x)
{
	struct sched_param param = {.sched_priority = 98 };
	FUSB_LOG
	    ("***********enter fusb302 state thread!!1 ********************\n");
	sched_setscheduler(current, SCHED_RR, &param);

	while (1) {
		if (FUSB300Int_PIN_LVL() || suspended) {
			set_current_state(TASK_INTERRUPTIBLE);
			wait_event_interruptible(fusb_thread_wq,
				state_changed == true && suspended == false);
			state_changed = false;
			set_current_state(TASK_RUNNING);
		}
		StateMachineFUSB300();
	}
	return 0;
}

static void fusb302_device_check(void)
{
	FUSB300Read(regDeviceID, 2, &Registers.DeviceID.byte);
	FUSB_LOG("device id:%2x\n", Registers.DeviceID.byte);
}

static int fusb302_reg_dump(struct seq_file *m, void *data)
{
	int reg_addr[] =
	    { regDeviceID, regSwitches0, regSwitches1, regMeasure, regSlice,
		regControl0, regControl1, regControl2, regControl3, regMask,
		regPower, regReset,
		regOCPreg, regMaska, regMaskb, regControl4, regStatus0a,
		regStatus1a, regInterrupta,
		regInterruptb, regStatus0, regStatus1, regInterrupt
	};

	static char *reg_name[] = {
		  "Device ID", "Switches0", "Switches1", "Measure", "Slice",
		  "Control0", "Control1", "Control2", "Control3", "Mask1",
		  "Power", "Reset", "OCPReg", "Maska", "Maskb", "Control4",
		  "Status0a", "Status1a", "Interrupta", "Interruptb", "Status0",
		  "Status1", "Interrupt"
		};


	int i;
	u8 byte = 0;
	for (i = 0; i < sizeof(reg_addr) / sizeof(reg_addr[0]); i++) {
		FUSB300Read(reg_addr[i], 1, &byte);
		seq_printf(m, "Register - %s: %02xH = %02x\n",
					reg_name[i], reg_addr[i], byte);
	}
	return 0;
}

static int fusbreg_debugfs_open(struct inode *inode, struct file *file)
{
	struct fusb_i2c_data *fusb = inode->i_private;

	return single_open(file, fusb302_reg_dump, fusb);
}

static const struct file_operations fusbreg_debugfs_ops = {
	.owner          = THIS_MODULE,
	.open           = fusbreg_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};


static ssize_t fusb302_state(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	char data[5];
	GetFUSB300TypeCStatus(data);
	return sprintf(buf, "SMC=%2x, connState=%2d, cc=%2x, current=%d\n",
		       data[0], data[1], data[2], data[3]);
}

static DEVICE_ATTR(state, 0440, fusb302_state, NULL);

static ssize_t fusb302_CC_state(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char data[5] = "";

	if (blnCCPinIsCC1)
		strlcpy(data, "CC1", sizeof(data));
	else if (blnCCPinIsCC2)
		strlcpy(data, "CC2", sizeof(data));
	return snprintf(buf, PAGE_SIZE - 2, "%s\n", data);
}

static DEVICE_ATTR(CC_state, S_IRUGO, fusb302_CC_state, NULL);

static ssize_t fusb302_enable_vconn(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	bool enable = false;

	if (!blnCCPinIsCC1 && !blnCCPinIsCC2) {
		pr_err("Trying to enable VCONN when CC is indeterminate\n");
		return -EINVAL;
	}

	if (!strnicmp(buf, "1", 1))
		enable = true;

	if (blnCCPinIsCC1 && !blnCCPinIsCC2) {
		Registers.Switches.VCONN_CC1 = 0;
		Registers.Switches.VCONN_CC2 = enable;
	} else if (!blnCCPinIsCC1 && blnCCPinIsCC2) {
		Registers.Switches.VCONN_CC1 = enable;
		Registers.Switches.VCONN_CC2 = 0;
	} else {
		pr_err("Invalid CC\n");
		return -EINVAL;
	}

	pr_err("Trying to %sable, VCONN on %s\n",
	       enable ? "en" : "dis", blnCCPinIsCC1 ? "CC2" : "CC1");
	/* Commit the Switch State */
	FUSB300Write(regSwitches0, 2, &Registers.Switches.byte[0]);
	return count;
}

static DEVICE_ATTR(enable_vconn, S_IWUSR | S_IWGRP, NULL, fusb302_enable_vconn);

static bool fusb302_mmi_factory(void)
{
	struct device_node *np = of_find_node_by_path("/chosen");
	bool factory = false;

	if (np)
		factory = of_property_read_bool(np, "mmi,factory-cable");

	of_node_put(np);

	return factory;
}

#define IRQ_GPIO_NAME "fusb_irq"
#ifdef CONFIG_OF
static int fusb302_parse_dt(struct device *dev, struct fusb302_i2c_data *fusb)
{
	int i;
	int gpio_cnt = of_gpio_count(dev->of_node);
	const char *label_prop = "fusb,gpio-labels";
	int label_cnt = of_property_count_strings(dev->of_node, label_prop);

	for (i = 0; i < ARRAY_SIZE(fusb->gpios); i++)
		fusb->gpios[i] = -ENODEV;

	if (gpio_cnt > ARRAY_SIZE(fusb->gpios)) {
		dev_err(dev, "%s:%d gpio count is greater than %zu.\n",
			__func__, __LINE__, ARRAY_SIZE(fusb->gpios));
		return -EINVAL;
	}

	if (label_cnt != gpio_cnt) {
		dev_err(dev, "%s:%d label count does not match gpio count.\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < gpio_cnt; i++) {
		enum of_gpio_flags flags = 0;
		int gpio;
		const char *label = NULL;

		gpio = of_get_gpio_flags(dev->of_node, i, &flags);
		if (gpio < 0) {
			dev_err(dev, "%s:%d of_get_gpio failed: %d\n",
				__func__, __LINE__, gpio);
			return gpio;
		}

		if (i < label_cnt)
			of_property_read_string_index(dev->of_node, label_prop,
						      i, &label);

		gpio_request_one(gpio, flags, label);
		gpio_export(gpio, true);
		gpio_export_link(dev, label, gpio);

		dev_dbg(dev, "%s: gpio=%d, flags=0x%x, label=%s\n",
			__func__, gpio, flags, label);

		fusb->gpios[i] = gpio;
	}

	fusb->irq = gpio_to_irq(fusb->gpios[FUSB_INT_INDEX]);
	FUSB_LOG("irq_gpio number is %d, irq = %d\n",
		 fusb->gpios[FUSB_INT_INDEX], fusb->irq);

	fusb->factory_mode = fusb302_mmi_factory();
	if (!fusb->factory_mode)
		fusb->fsa321_switch = of_property_read_bool(dev->of_node,
							"fsa321-audio-switch");
	return 0;
}
#else
static inline int fusb302_parse_dt(struct device *dev,
				   struct fusb302_i2c_data *fusb)
{
	return 0;
}
#endif

static enum power_supply_property switch_power_supply_props[] = {
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_SWITCH_STATE,
};

static int switch_power_supply_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = psy->type;
		break;
	case POWER_SUPPLY_PROP_SWITCH_STATE:
		val->intval = SwitchState;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}


static enum power_supply_property fusb_power_supply_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_DISABLE_USB,
};

static int fusb_power_supply_set_property(struct power_supply *psy,
				 enum power_supply_property prop,
				 const union power_supply_propval *val)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		break;
	case POWER_SUPPLY_PROP_DISABLE_USB:
		usbDataDisabled = !!val->intval;
		break;
	default:
		return -EINVAL;
	}
	power_supply_changed(&fusb_i2c_data->usbc_psy);
	return 0;
}

static int fusb_power_supply_is_writeable(struct power_supply *psy,
				 enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_DISABLE_USB:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static int fusb_power_supply_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		if ((ConnState > Unattached) && (ConnState < DelayUnattached))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if ((ConnState == PoweredAccessory) ||
		    (ConnState == UnsupportedAccessory) ||
		    (ConnState == DebugAccessory) ||
		    (ConnState == AudioAccessory) ||
		    (ConnState == AttachedSink) ||
		    (ConnState == AttachedSource))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (ConnState != AttachedSink)
			val->intval = 0;
		else if (SinkCurrent == utcc1p5A)
			val->intval = 1500000;
		else if (SinkCurrent == utcc3p0A)
			val->intval = 3000000;
		else if (SinkCurrent == utccDefault)
			val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		val->intval = 0; /* PD Not supported yet */
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = psy->type;
		break;
	case POWER_SUPPLY_PROP_DISABLE_USB:
		val->intval = usbDataDisabled;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum dual_role_property fusb_drp_properties[] = {
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
};

 /* Callback for "cat /sys/class/dual_role_usb/otg_default/<property>" */
static int dual_role_get_local_prop(struct dual_role_phy_instance *dual_role,
	enum dual_role_property prop,
	unsigned int *val)
{
	struct fusb302_i2c_data *fusb;
	struct i2c_client *client = dual_role_get_drvdata(dual_role);
	int ret = 0;

	if (!client)
		return -EINVAL;

	fusb = i2c_get_clientdata(client);

	if (ConnState == AttachedSource) {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_DFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = DUAL_ROLE_PROP_PR_SRC;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_HOST;
		else
			ret = -EINVAL;
	} else if (ConnState == AttachedSink) {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_UFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = DUAL_ROLE_PROP_PR_SNK;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_DEVICE;
		else
			ret = -EINVAL;
	} else {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_NONE;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = DUAL_ROLE_PROP_PR_NONE;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_NONE;
		else
			ret = -EINVAL;
	}

	return ret;
}

/* Decides whether userspace can change a specific property */
static int dual_role_is_writeable(struct dual_role_phy_instance *drp,
				enum dual_role_property prop) {
	/* Return 0 for now */
	return 0;
}

/* Callback for "echo <value> >
 *                      /sys/class/dual_role_usb/<name>/<property>"
 * Block until the entire final state is reached.
 * Blocking is one of the better ways to signal when the operation
 * is done.
 * This function tries to switched to Attached.SRC or Attached.SNK
 * by forcing the mode into SRC or SNK.
 * On failure, we fall back to Try.SNK state machine.
 */
static int dual_role_set_prop(struct dual_role_phy_instance *dual_role,
				enum dual_role_property prop,
				const unsigned int *val) {
	/* Return nothing for now */
	return 0;
}

static int get_reg(void *data, u64 *val)
{
	struct fusb302_i2c_data *fusb = data;
	u8 temp = 0;

	FUSB300Read(fusb->debug_address, 1, &temp);
	*val = temp;
	return 0;
}

static int set_reg(void *data, u64 val)
{
	struct fusb302_i2c_data *fusb = data;
	u8 temp;

	temp = (u8) val;
	FUSB300Write(fusb->debug_address, 1, &temp);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fusbdata_reg_fops, get_reg, set_reg, "0x%02llx\n");

int fusb302_debug_init(struct fusb302_i2c_data *fusb)
{
	struct dentry *ent;

	fusb->debug_root = debugfs_create_dir("fusb302", NULL);

	if (!fusb->debug_root) {
		dev_err(&fusb->client->dev, "Couldn't create debug dir\n");
		return -EINVAL;
	}

	ent = debugfs_create_file("register_dump",
			S_IFREG | S_IRUGO,
			fusb->debug_root, fusb,
			&fusbreg_debugfs_ops);


	if (!ent) {
		dev_err(&fusb->client->dev, "Couldn't create reg dump\n");
		return -EINVAL;
	}

	ent = debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
				fusb->debug_root, &(fusb->debug_address));
	if (!ent) {
		dev_err(&fusb->client->dev, "Error creating address entry\n");
		return -EINVAL;
	}

	ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
				fusb->debug_root, fusb, &fusbdata_reg_fops);
	if (!ent) {
		dev_err(&fusb->client->dev, "Error creating data entry\n");
		return -EINVAL;
	}

	return 0;
}

int fusb302_debug_remove(struct fusb302_i2c_data *fusb)
{
	debugfs_remove_recursive(fusb->debug_root);
	return 0;
}

static int fusb302_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct fusb302_i2c_data *fusb;
	int ret_device_file = 0;
	unsigned char data;
	int retval = 0;
	struct dual_role_phy_desc *desc;
	struct dual_role_phy_instance *dual_role;
	FUSB_LOG("enter probe\n");
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&i2c->dev,
			"%s: SMBus byte data commands not supported by host\n",
			__func__);
		return -EIO;
	}
	if (i2c->dev.of_node) {
		fusb = devm_kzalloc(&i2c->dev,
				    sizeof(struct fusb302_i2c_data),
				    GFP_KERNEL);
		if (!fusb) {
			dev_err(&i2c->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		retval = fusb302_parse_dt(&i2c->dev, fusb);
		if (retval)
			return retval;
	} else {
		fusb = i2c->dev.platform_data;
	}

	/*
	   fusb = kzalloc(sizeof(struct fusb302_i2c_data), GFP_KERNEL);
	   if (!fusb) {
	   dev_err(&i2c->dev, "private data alloc fail\n");
	   goto exit;
	   } */

	InitializeFUSB300Variables();
#ifdef PD_SUPPORT
	InitializeUSBPDVariables();
#endif

	fusb_i2c_data = fusb;
	i2c_set_clientdata(i2c, fusb);
	fusb->client = i2c;


	/* Set the FSA switch to lpm initially */
	if (fusb_i2c_data->fsa321_switch)
		FSA321_setSwitchState(fsa_lpm);

	ret_device_file = device_create_file(&(i2c->dev), &dev_attr_state);
	ret_device_file = device_create_file(&(i2c->dev), &dev_attr_CC_state);
	ret_device_file = device_create_file(&(i2c->dev),
					     &dev_attr_enable_vconn);


	fusb->usbc_psy.name		= "usbc";
	fusb->usbc_psy.type		= POWER_SUPPLY_TYPE_USBC;
	fusb->usbc_psy.get_property	= fusb_power_supply_get_property;
	fusb->usbc_psy.set_property	= fusb_power_supply_set_property;
	fusb->usbc_psy.properties	= fusb_power_supply_props;
	fusb->usbc_psy.num_properties	= ARRAY_SIZE(fusb_power_supply_props);
	fusb->usbc_psy.property_is_writeable = fusb_power_supply_is_writeable;
	retval = power_supply_register(&i2c->dev, &fusb->usbc_psy);
	if (retval) {
		dev_err(&i2c->dev, "failed to register usbc_psy\n");
		return retval;
	}

	fusb->switch_psy.name		= "usbc_switch";
	fusb->switch_psy.type		= POWER_SUPPLY_TYPE_SWITCH;
	fusb->switch_psy.get_property	= switch_power_supply_get_property;
	fusb->switch_psy.properties	= switch_power_supply_props;
	fusb->switch_psy.num_properties	= ARRAY_SIZE(switch_power_supply_props);
	retval = power_supply_register(&i2c->dev, &fusb->switch_psy);
	if (retval) {
		dev_err(&i2c->dev, "failed to register switch_psy\n");
		goto unregister_usbcpsy;
	}

	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
		desc = devm_kzalloc(&i2c->dev,
					sizeof(struct dual_role_phy_desc),
					GFP_KERNEL);
		if (!desc) {
			dev_err(&i2c->dev, "unable to allocate dual role descriptor\n");
			goto unregister_switchpsy;
		}

		desc->name = "otg_default";

		switch (PortType) {
		case USBTypeC_Sink:
			desc->supported_modes = DUAL_ROLE_SUPPORTED_MODES_UFP;
			break;
		case USBTypeC_Source:
			desc->supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP;
			break;
		case USBTypeC_DRP:
		default:
			desc->supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
			break;
		}

		desc->get_property = dual_role_get_local_prop;
		desc->set_property = dual_role_set_prop;
		desc->properties = fusb_drp_properties;
		desc->num_properties = ARRAY_SIZE(fusb_drp_properties);
		desc->property_is_writeable = dual_role_is_writeable;
		dual_role = devm_dual_role_instance_register(&i2c->dev, desc);
		dual_role->drv_data = i2c;
		fusb->dual_role = dual_role;
		fusb->desc = desc;
	}


	fusb302_device_check();
	//Initialize FUSB302
	data = 0x01;
	FUSB300Write(regReset, 1, &data);
	InitializeFUSB300();

	//create kernel run
	fusb->thread =
	    kthread_run(fusb302_state_kthread, NULL, "fusb302_state_kthread");

	INIT_WORK(&fusb_i2c_data->eint_work, fusb_eint_work);
	retval = request_threaded_irq(fusb->irq, NULL,
				      cc_eint_interrupt_handler,
				      IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				      FUSB302_I2C_NAME, fusb);
	if (retval < 0) {
		dev_err(&i2c->dev,
			"%s: Failed to create irq thread\n", __func__);
		goto unregister_dr;
	}

	fusb302_debug_init(fusb);

	/* Initialize the Switch */
	power_supply_changed(&fusb->switch_psy);
	FUSB_LOG("probe successfully!\n");
	return 0;

unregister_dr:
	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
		devm_dual_role_instance_unregister(&i2c->dev, fusb->dual_role);
		devm_kfree(&i2c->dev, fusb->desc);
	}
unregister_switchpsy:
	power_supply_unregister(&fusb->switch_psy);
unregister_usbcpsy:
	power_supply_unregister(&fusb->usbc_psy);
	devm_kfree(&i2c->dev, fusb);
	return retval;
}

static int fusb302_remove(struct i2c_client *i2c)
{
	struct fusb302_i2c_data *fusb = i2c_get_clientdata(i2c);

	fusb302_debug_remove(fusb);
	device_remove_file(&(i2c->dev), &dev_attr_state);
	device_remove_file(&(i2c->dev), &dev_attr_CC_state);
	device_remove_file(&(i2c->dev), &dev_attr_enable_vconn);
	if (fusb->irq)
		free_irq(fusb->irq, fusb);
	power_supply_unregister(&fusb->switch_psy);
	power_supply_unregister(&fusb->usbc_psy);
	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
		devm_dual_role_instance_unregister(&i2c->dev, fusb->dual_role);
		devm_kfree(&i2c->dev, fusb->desc);
	}
	devm_kfree(&i2c->dev, fusb);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int fusb302_suspend(struct device *dev)
{
	struct fusb302_i2c_data *fusb = dev_get_drvdata(dev);

	suspended = true;
	/*
	 * disable the irq and enable irq_wake
	 * capability to the interrupt line.
	 */
	if (fusb->irq) {
		disable_irq(fusb->irq);
		enable_irq_wake(fusb->irq);
	}

	return 0;
}

static int fusb302_resume(struct device *dev)
{
	struct fusb302_i2c_data *fusb = dev_get_drvdata(dev);

	if (fusb->irq) {
		disable_irq_wake(fusb->irq);
		enable_irq(fusb->irq);
	}

	suspended = false;
	if (state_changed == true)
		wake_up_interruptible(&fusb_thread_wq);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(fusb302_pm_ops, fusb302_suspend,
			fusb302_resume);

static const struct i2c_device_id fusb302_id[] = {
	{FUSB302_I2C_NAME, 0},
	{}

};

/*
static struct i2c_board_info __initdata fusb302_i2c_boardinfo[] = {
        {
                I2C_BOARD_INFO(FUSB302_I2C_NAME, (0x22)),
        },
};*/

#ifdef CONFIG_OF
static struct of_device_id fusb302_match_table[] = {
	{.compatible = "fairchild,fusb302",},
	{},
};
#else
#define fusb302_match_table NULL
#endif
static struct i2c_driver fusb302_i2c_driver = {
	.driver = {
		   .name = FUSB302_I2C_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = fusb302_match_table,
		   .pm = &fusb302_pm_ops,
		   },
	.probe = fusb302_probe,
	.remove = fusb302_remove,
	.id_table = fusb302_id,
};

static int __init fusb302_i2c_init(void)
{
	printk("FUSB302:%s\n", __func__);
	/*i2c_register_board_info(FUSB302_I2C_NUM, fusb302_i2c_boardinfo,
	   ARRAY_SIZE(fusb302_i2c_boardinfo)); */

	return i2c_add_driver(&fusb302_i2c_driver);
}

static void __exit fusb302_i2c_exit(void)
{
	i2c_del_driver(&fusb302_i2c_driver);
}

module_init(fusb302_i2c_init);
module_exit(fusb302_i2c_exit);
