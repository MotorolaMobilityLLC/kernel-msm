#include <linux/kernel.h>
#include <linux/stat.h>		// File permission masks
#include <linux/types.h>	// Kernel datatypes
#include <linux/i2c.h>		// I2C access, mutex
#include <linux/errno.h>	// Linux kernel error definitions
#include <linux/hrtimer.h>	// hrtimer
#include <linux/workqueue.h>	// work_struct, delayed_work
#include <linux/delay.h>	// udelay, usleep_range, msleep
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/power_supply.h>
#include <linux/debugfs.h>
#include "fusb30x_global.h"	// Chip structure access
#include "../core/core.h"	// Core access
#include "../core/fusb30X.h"
#include "platform_helpers.h"

#ifdef FSC_DEBUG
#include "hostcomm.h"
#include "../core/PD_Types.h"	// State Log states
#include "../core/TypeC_Types.h"	// State Log states
#endif // FSC_DEBUG
#include "../core/TypeC.h"
/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************        GPIO Interface         ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
const char *FUSB_DT_INTERRUPT_INTN = "fsc_interrupt_int_n";	// Name of the INT_N interrupt in the Device Tree
#define FUSB_DT_GPIO_INTN               "fairchild,int_n"	// Name of the Int_N GPIO pin in the Device Tree
#define FUSB_DT_GPIO_VBUS_5V            "fairchild,vbus5v"	// Name of the VBus 5V GPIO pin in the Device Tree
#define FUSB_DT_GPIO_VBUS_OTHER         "fairchild,vbusOther"	// Name of the VBus Other GPIO pin in the Device Tree

#ifdef FSC_DEBUG
#define FUSB_DT_GPIO_DEBUG_SM_TOGGLE    "fairchild,dbg_sm"	// Name of the debug State Machine toggle GPIO pin in the Device Tree
#endif // FSC_DEBUG

static bool disable_ss_switch;
bool debug_audio = false;
#ifdef FSC_INTERRUPT_TRIGGERED
/* Internal forward declarations */
static irqreturn_t _fusb_isr_intn(int irq, void *dev_id);
#endif // FSC_INTERRUPT_TRIGGERED
FSC_U16 gBcdDevice = 0;
static int FSA321_setSwitchState(FSASwitchState state)
{
	int aud_det_gpio, aud_sw_sel_gpio;
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (!chip) {
		pr_alert("FUSB  %s - Error: Chip structure is NULL!\n",
				 __func__);
		return -ENOMEM;
	}
	aud_det_gpio = chip->gpios[FUSB_AUD_DET_INDEX];
	aud_sw_sel_gpio = chip->gpios[FUSB_AUD_SW_SEL_INDEX];

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
		break;
	case fsa_usb_mode:
		gpio_set_value(aud_sw_sel_gpio, 0);
		gpio_set_value(aud_det_gpio, 1);
		break;
	default:
		break;
	}

	return 0;
}

static int fusb_toggleAudioSwitch(bool enable)
{
	int aud_det_gpio, aud_sw_sel_gpio;
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (!chip) {
		pr_alert("FUSB  %s - Error: Chip structure is NULL!\n",
				 __func__);
		return -ENOMEM;
	}
	aud_det_gpio = chip->gpios[FUSB_AUD_DET_INDEX];
	aud_sw_sel_gpio = chip->gpios[FUSB_AUD_SW_SEL_INDEX];
	if (!(gpio_is_valid(aud_det_gpio) &&
		gpio_is_valid(aud_sw_sel_gpio)))
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

static int fusb_enableSuperspeedUSB(int CC1, int CC2)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (!chip) {
		pr_alert("FUSB  %s - Error: Chip structure is NULL!\n",
				 __func__);
		return -ENOMEM;
	}
	if (disable_ss_switch)
		return 0;
	SwitchState = CC2 ? 2 : 1;
	power_supply_changed(&switch_psy);
	FUSB_LOG("Enabling SS lines for CC%d\n", SwitchState);
	return 0;
}
static int fusb_disableSuperspeedUSB(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (!chip) {
		pr_alert("FUSB  %s - Error: Chip structure is NULL!\n",
			   __func__);
		return -ENOMEM;
	}
	SwitchState = 0;
	power_supply_changed(&switch_psy);
	return -ENODEV;
}
void platform_disableSuperspeedUSB(void)
{
	fusb_disableSuperspeedUSB();
}
void platform_enableSuperspeedUSB(int CC1, int CC2)
{
	fusb_enableSuperspeedUSB(CC1, CC2);
}
void platform_toggleAudioSwitch(FSASwitchState state)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (!chip) {
		pr_alert("FUSB  %s - Error: Chip structure is NULL!\n",
			   __func__);
		return;
	}
	FUSB_LOG("SetAudio Mode %x\n", state);
	switch (state) {

	case fsa_lpm:
	if (chip->fsa321_switch)
		FSA321_setSwitchState(fsa_lpm);
	else
		fusb_toggleAudioSwitch(false);
	break;

	case fsa_audio_mode:
	if (chip->fsa321_switch)
		FSA321_setSwitchState(fsa_lpm);
	else
		fusb_toggleAudioSwitch(true);
	break;
		break;
	case fsa_usb_mode:
	if (chip->fsa321_switch)
		FSA321_setSwitchState(fsa_usb_mode);
	break;

	default:
		break;
	}
}
static bool fusb30x_mmi_factory(void)
{
	struct device_node *np = of_find_node_by_path("/chosen");
	bool factory = false;

	if (np)
		factory = of_property_read_bool(np, "mmi,factory-cable");

	of_node_put(np);

	return factory;
}
FSC_S32 fusb_InitializeGPIO(void)
{
	FSC_S32 ret = 0;
	int i, gpio_cnt, label_cnt;
	const char *label_prop = "fusb,gpio-labels";
	struct device_node *node;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_alert("FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return -ENOMEM;
	}
	/* Get our device tree node */
	node = chip->client->dev.of_node;
	gpio_cnt = of_gpio_count(node);
	label_cnt = of_property_count_strings(node, label_prop);
	for (i = 0; i < ARRAY_SIZE(chip->gpios); i++)
		chip->gpios[i] = -ENODEV;
	if (gpio_cnt > ARRAY_SIZE(chip->gpios)) {
		dev_err(&chip->client->dev,
			"%s:%d gpio count is greater than %zu.\n", __func__,
			__LINE__, ARRAY_SIZE(chip->gpios));
		return -EINVAL;
	}
	if (label_cnt != gpio_cnt) {
		dev_err(&chip->client->dev,
			"%s:%d label count does not match gpio count.\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < gpio_cnt; i++) {
		enum of_gpio_flags flags = 0;
		int gpio;
		const char *label = NULL;

		gpio = of_get_gpio_flags(node, i, &flags);
		if (gpio < 0) {
			dev_err(&chip->client->dev,
				"%s:%d of_get_gpio failed: %d\n", __func__,
				__LINE__, gpio);
			return gpio;
		}
		if (i < label_cnt)
			of_property_read_string_index(node, label_prop,
						      i, &label);
		gpio_request_one(gpio, flags, label);
		gpio_export(gpio, true);
		gpio_export_link(&chip->client->dev, label, gpio);
		dev_dbg(&chip->client->dev,
			"%s: gpio=%d, flags=0x%x, label=%s\n", __func__, gpio,
			flags, label);
		chip->gpios[i] = gpio;
	}
	chip->gpio_IntN = chip->gpios[FUSB_INT_INDEX];
	FUSB_LOG("irq_gpio number is %d\n", chip->gpios[FUSB_INT_INDEX]);
#ifdef FPGA_BOARD
	/* Get our GPIO pins from the device tree, allocate them, and then set their direction (input/output) */
	chip->gpio_IntN = of_get_named_gpio(node, FUSB_DT_GPIO_INTN, 0);
	if (!gpio_is_valid(chip->gpio_IntN)) {
		dev_err(&chip->client->dev,
			"%s - Error: Could not get named GPIO for Int_N! Error code: %d\n",
			__func__, chip->gpio_IntN);
		return chip->gpio_IntN;
	}
	// Request our GPIO to reserve it in the system - this should help ensure we have exclusive access (not guaranteed)
	ret = gpio_request(chip->gpio_IntN, FUSB_DT_GPIO_INTN);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"%s - Error: Could not request GPIO for Int_N! Error code: %d\n",
			__func__, ret);
		return ret;
	}

	ret = gpio_direction_input(chip->gpio_IntN);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"%s - Error: Could not set GPIO direction to input for Int_N! Error code: %d\n",
			__func__, ret);
		return ret;
	}
#endif
#ifdef FSC_DEBUG
	/* Export to sysfs */
	gpio_export(chip->gpio_IntN, false);
	gpio_export_link(&chip->client->dev, FUSB_DT_GPIO_INTN,
			 chip->gpio_IntN);
#endif // FSC_DEBUG
#ifdef FPGA_BOARD
	pr_info("FUSB  %s - INT_N GPIO initialized as pin '%d'\n", __func__,
		chip->gpio_IntN);

	// VBus 5V
	chip->gpio_VBus5V = of_get_named_gpio(node, FUSB_DT_GPIO_VBUS_5V, 0);
	if (!gpio_is_valid(chip->gpio_VBus5V)) {
		dev_err(&chip->client->dev,
			"%s - Error: Could not get named GPIO for VBus5V! Error code: %d\n",
			__func__, chip->gpio_VBus5V);
		fusb_GPIO_Cleanup();
		return chip->gpio_VBus5V;
	}
	// Request our GPIO to reserve it in the system - this should help ensure we have exclusive access (not guaranteed)
	ret = gpio_request(chip->gpio_VBus5V, FUSB_DT_GPIO_VBUS_5V);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"%s - Error: Could not request GPIO for VBus5V! Error code: %d\n",
			__func__, ret);
		return ret;
	}

	ret = gpio_direction_output(chip->gpio_VBus5V, chip->gpio_VBus5V_value);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"%s - Error: Could not set GPIO direction to output for VBus5V! Error code: %d\n",
			__func__, ret);
		fusb_GPIO_Cleanup();
		return ret;
	}
#ifdef FSC_DEBUG
	// Export to sysfs
	gpio_export(chip->gpio_VBus5V, false);
	gpio_export_link(&chip->client->dev, FUSB_DT_GPIO_VBUS_5V,
			 chip->gpio_VBus5V);
#endif // FSC_DEBUG

	pr_info
	    ("FUSB  %s - VBus 5V initialized as pin '%d' and is set to '%d'\n",
	     __func__, chip->gpio_VBus5V, chip->gpio_VBus5V_value ? 1 : 0);

	// VBus other (eg. 12V)
	// NOTE - This VBus is optional, so if it doesn't exist then fake it like it's on.
	chip->gpio_VBusOther =
	    of_get_named_gpio(node, FUSB_DT_GPIO_VBUS_OTHER, 0);
	if (!gpio_is_valid(chip->gpio_VBusOther)) {
		// Soft fail - provide a warning, but don't quit because we don't really need this VBus if only using VBus5v
		pr_warning
		    ("%s - Warning: Could not get GPIO for VBusOther! Error code: %d\n",
		     __func__, chip->gpio_VBusOther);
	} else {
		// Request our GPIO to reserve it in the system - this should help ensure we have exclusive access (not guaranteed)
		ret =
		    gpio_request(chip->gpio_VBusOther, FUSB_DT_GPIO_VBUS_OTHER);
		if (ret < 0) {
			dev_err(&chip->client->dev,
				"%s - Error: Could not request GPIO for VBusOther! Error code: %d\n",
				__func__, ret);
			return ret;
		}

		ret =
		    gpio_direction_output(chip->gpio_VBusOther,
					  chip->gpio_VBusOther_value);
		if (ret != 0) {
			dev_err(&chip->client->dev,
				"%s - Error: Could not set GPIO direction to output for VBusOther! Error code: %d\n",
				__func__, ret);
			return ret;
		} else {
			pr_info
			    ("FUSB  %s - VBusOther initialized as pin '%d' and is set to '%d'\n",
			     __func__, chip->gpio_VBusOther,
			     chip->gpio_VBusOther_value ? 1 : 0);

		}
	}
#endif
#ifdef FSC_DEBUG
	// State Machine Debug Notification
	// Optional GPIO - toggles each time the state machine is called
	chip->dbg_gpio_StateMachine =
	    of_get_named_gpio(node, FUSB_DT_GPIO_DEBUG_SM_TOGGLE, 0);
	if (!gpio_is_valid(chip->dbg_gpio_StateMachine)) {
		// Soft fail - provide a warning, but don't quit because we don't really need this VBus if only using VBus5v
		pr_warning
		    ("%s - Warning: Could not get GPIO for Debug GPIO! Error code: %d\n",
		     __func__, chip->dbg_gpio_StateMachine);
	} else {
		// Request our GPIO to reserve it in the system - this should help ensure we have exclusive access (not guaranteed)
		ret =
		    gpio_request(chip->dbg_gpio_StateMachine,
				 FUSB_DT_GPIO_DEBUG_SM_TOGGLE);
		if (ret < 0) {
			dev_err(&chip->client->dev,
				"%s - Error: Could not request GPIO for Debug GPIO! Error code: %d\n",
				__func__, ret);
			return ret;
		}

		ret =
		    gpio_direction_output(chip->dbg_gpio_StateMachine,
					  chip->dbg_gpio_StateMachine_value);
		if (ret != 0) {
			dev_err(&chip->client->dev,
				"%s - Error: Could not set GPIO direction to output for Debug GPIO! Error code: %d\n",
				__func__, ret);
			return ret;
		} else {
			pr_info
			    ("FUSB  %s - Debug GPIO initialized as pin '%d' and is set to '%d'\n",
			     __func__, chip->dbg_gpio_StateMachine,
			     chip->dbg_gpio_StateMachine_value ? 1 : 0);

		}

		// Export to sysfs
		gpio_export(chip->dbg_gpio_StateMachine, true);	// Allow direction to change to provide max debug flexibility
		gpio_export_link(&chip->client->dev,
				 FUSB_DT_GPIO_DEBUG_SM_TOGGLE,
				 chip->dbg_gpio_StateMachine);
	}
#endif // FSC_DEBUG
	chip->factory_mode = fusb30x_mmi_factory();
	if (!chip->factory_mode)
		chip->fsa321_switch = of_property_read_bool(node,
						  "fsa321-audio-switch");
	/* Set the FSA switch to lpm initially */
	if (chip->fsa321_switch)
		FSA321_setSwitchState(fsa_lpm);
	return 0;		// Success!
}

void fusb_GPIO_Set_VBus5v(FSC_BOOL set)
{
#ifdef FPGA_BOARD
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
	}
	// GPIO must be valid by this point
	if (gpio_cansleep(chip->gpio_VBus5V)) {
		/* 
		 * If your system routes GPIO calls through a queue of some kind, then
		 * it may need to be able to sleep. If so, this call must be used.
		 */
		gpio_set_value_cansleep(chip->gpio_VBus5V, set ? 1 : 0);
	} else {
		gpio_set_value(chip->gpio_VBus5V, set ? 1 : 0);
	}
	chip->gpio_VBus5V_value = set;

	FUSB_LOG("FUSB  %s - VBus 5V set to: %d\n", __func__,
		 chip->gpio_VBus5V_value ? 1 : 0);
#endif
}

void fusb_GPIO_Set_VBusOther(FSC_BOOL set)
{
#ifdef FPGA_BOARD
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
	}
	// Only try to set if feature is enabled, otherwise just fake it
	if (gpio_is_valid(chip->gpio_VBusOther)) {
		/*
		 * If your system routes GPIO calls through a queue of some kind, then
		 * it may need to be able to sleep. If so, this call must be used.
		 */
		if (gpio_cansleep(chip->gpio_VBusOther)) {
			gpio_set_value_cansleep(chip->gpio_VBusOther,
						set ? 1 : 0);
		} else {
			gpio_set_value(chip->gpio_VBusOther, set ? 1 : 0);
		}
	}
	chip->gpio_VBusOther_value = set;
#endif
}

FSC_BOOL fusb_GPIO_Get_VBus5v(void)
{
#ifdef FPGA_BOARD
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return false;
	}

	if (!gpio_is_valid(chip->gpio_VBus5V)) {
		pr_debug
		    ("FUSB  %s - Error: VBus 5V pin invalid! Pin value: %d\n",
		     __func__, chip->gpio_VBus5V);
	}

	return chip->gpio_VBus5V_value;
#endif
	return true;
}

FSC_BOOL fusb_GPIO_Get_VBusOther(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return false;
	}

	return chip->gpio_VBusOther_value;
}

FSC_BOOL fusb_GPIO_Get_IntN(void)
{
	FSC_S32 ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return false;
	} else {
		/*
		 * If your system routes GPIO calls through a queue of some kind, then
		 * it may need to be able to sleep. If so, this call must be used.
		 */
		if (gpio_cansleep(chip->gpio_IntN)) {
			ret = !gpio_get_value_cansleep(chip->gpio_IntN);
		} else {
			ret = !gpio_get_value(chip->gpio_IntN);	// Int_N is active low
		}
		return (ret != 0);
	}
}

#ifdef FSC_DEBUG
void dbg_fusb_GPIO_Set_SM_Toggle(FSC_BOOL set)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
	}

	if (gpio_is_valid(chip->dbg_gpio_StateMachine)) {
		/*
		 * If your system routes GPIO calls through a queue of some kind, then
		 * it may need to be able to sleep. If so, this call must be used.
		 */
		if (gpio_cansleep(chip->dbg_gpio_StateMachine)) {
			gpio_set_value_cansleep(chip->dbg_gpio_StateMachine,
						set ? 1 : 0);
		} else {
			gpio_set_value(chip->dbg_gpio_StateMachine,
				       set ? 1 : 0);
		}
		chip->dbg_gpio_StateMachine_value = set;
	}
}

FSC_BOOL dbg_fusb_GPIO_Get_SM_Toggle(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return false;
	}
	return chip->dbg_gpio_StateMachine_value;
}
#endif // FSC_DEBUG

void fusb_GPIO_Cleanup(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}
#ifdef FSC_INTERRUPT_TRIGGERED
	if (gpio_is_valid(chip->gpio_IntN) && chip->gpio_IntN_irq != -1)	// -1 indicates that we don't have an IRQ to free
	{
		devm_free_irq(&chip->client->dev, chip->gpio_IntN_irq, chip);
	}
#endif // FSC_INTERRUPT_TRIGGERED

	if (gpio_is_valid(chip->gpio_IntN) >= 0) {
#ifdef FSC_DEBUG
		gpio_unexport(chip->gpio_IntN);
#endif // FSC_DEBUG

		gpio_free(chip->gpio_IntN);
	}

	if (gpio_is_valid(chip->gpio_VBus5V) >= 0) {
#ifdef FSC_DEBUG
		gpio_unexport(chip->gpio_VBus5V);
#endif // FSC_DEBUG

		gpio_free(chip->gpio_VBus5V);
	}

	if (gpio_is_valid(chip->gpio_VBusOther) >= 0) {
		gpio_free(chip->gpio_VBusOther);
	}
#ifdef FSC_DEBUG
	if (gpio_is_valid(chip->dbg_gpio_StateMachine) >= 0) {
		gpio_unexport(chip->dbg_gpio_StateMachine);
		gpio_free(chip->dbg_gpio_StateMachine);
	}
#endif // FSC_DEBUG
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************         I2C Interface         ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
FSC_BOOL fusb_I2C_WriteData(FSC_U8 address, FSC_U8 length, FSC_U8 * data)
{
	FSC_S32 i = 0;
	FSC_S32 ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (chip == NULL || chip->client == NULL || data == NULL)	// Sanity check
	{
		pr_err("%s - Error: %s is NULL!\n", __func__,
		       (chip ==
			NULL ? "Internal chip structure" : (chip->client ==
							    NULL ? "I2C Client"
							    :
							    "Write data buffer")));
		return false;
	}
	mutex_lock(&chip->lock);
	// Retry on failure up to the retry limit
	for (i = 0; i <= chip->numRetriesI2C; i++) {
		ret = i2c_smbus_write_i2c_block_data(chip->client,	// Perform the actual I2C write on our client
						     address,	// Register address to write to
						     length,	// Number of bytes to write
						     data);	// Ptr to unsigned char data

		if (ret < 0)	// Errors report as negative
		{
			if (ret == -ERANGE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ERANGE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EINVAL) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EINVAL.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EAGAIN) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EAGAIN.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EALREADY) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EALREADY.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EBADE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EBADE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EBADMSG) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EBADMSG.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EBUSY) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EBUSY.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ECANCELED) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ECANCELED.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ECOMM) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ECOMM.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ECONNABORTED) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ECONNABORTED.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ECONNREFUSED) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ECONNREFUSED.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ECONNRESET) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ECONNRESET.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EDEADLK) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EDEADLK.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EDEADLOCK) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EDEADLOCK.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EDESTADDRREQ) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EDESTADDRREQ.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EFAULT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EFAULT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EHOSTDOWN) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EHOSTDOWN.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EHOSTUNREACH) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EHOSTUNREACH.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EILSEQ) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EILSEQ.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EINPROGRESS) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EINPROGRESS.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EINTR) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EINTR.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EIO) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EIO.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ELIBACC) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ELIBACC.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ELIBBAD) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ELIBBAD.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ELIBMAX) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ELIBMAX.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ELOOP) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ELOOP.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EMSGSIZE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EMSGSIZE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EMULTIHOP) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EMULTIHOP.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOBUFS) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOBUFS.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENODATA) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENODATA.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENODEV) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENODEV.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOLCK) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOLCK.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOMEM) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOMEM.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOMSG) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOMSG.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOPROTOOPT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOPROTOOPT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOSPC) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOSPC.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOSYS) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOSYS.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOTBLK) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOTBLK.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOTTY) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOTTY.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOTUNIQ) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENOTUNIQ.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENXIO) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ENXIO.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EOVERFLOW) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EOVERFLOW.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPERM) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EPERM.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPFNOSUPPORT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EPFNOSUPPORT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPIPE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EPIPE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPROTO) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EPROTO.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPROTONOSUPPORT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EPROTONOSUPPORT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ERANGE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ERANGE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EREMCHG) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EREMCHG.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EREMOTE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EREMOTE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EREMOTEIO) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EREMOTEIO.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ERESTART) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ERESTART.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ESRCH) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ESRCH.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ETIME) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ETIME.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ETIMEDOUT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ETIMEDOUT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ETXTBSY) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -ETXTBSY.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EUCLEAN) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EUCLEAN.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EUNATCH) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EUNATCH.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EUSERS) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EUSERS.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EWOULDBLOCK) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EWOULDBLOCK.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EXDEV) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EXDEV.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EXFULL) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block writing byte data. Address: '0x%02x', Return: -EXFULL.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EOPNOTSUPP) {
				dev_err(&chip->client->dev,
					"%s - I2C Error writing byte data. Address: '0x%02x', Return: -EOPNOTSUPP.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPROBE_DEFER) {
				dev_err(&chip->client->dev,
					"%s - I2C Error writing byte data. Address: '0x%02x', Return: -EPROBE_DEFER.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOENT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error writing byte data. Address: '0x%02x', Return: -ENOENT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else {
				dev_err(&chip->client->dev,
					"%s - Unexpected I2C error block writing byte data. Address: '0x%02x', Return: '%d'.  Attempt #%d / %d...\n",
					__func__, address, ret, i,
					chip->numRetriesI2C);
			}
		} else		// Successful i2c writes should always return 0
		{
			break;
		}
	}

	mutex_unlock(&chip->lock);
	return (ret >= 0);
}

FSC_BOOL fusb_I2C_ReadData(FSC_U8 address, FSC_U8 * data)
{
	FSC_S32 i = 0;
	FSC_S32 ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (chip == NULL || chip->client == NULL || data == NULL) {
		pr_err("%s - Error: %s is NULL!\n", __func__,
		       (chip ==
			NULL ? "Internal chip structure" : (chip->client ==
							    NULL ? "I2C Client"
							    :
							    "read data buffer")));
		return false;
	}
	mutex_lock(&chip->lock);
	// Retry on failure up to the retry limit
	for (i = 0; i <= chip->numRetriesI2C; i++) {
		ret = i2c_smbus_read_byte_data(chip->client, (u8) address);	// Read a byte of data from address
		if (ret < 0)	// Errors report as negative
		{
			if (ret == -ERANGE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ERANGE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EINVAL) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EINVAL.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EAGAIN) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EAGAIN.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EALREADY) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EALREADY.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EBADE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EBADE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EBADMSG) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EBADMSG.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EBUSY) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EBUSY.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ECANCELED) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ECANCELED.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ECOMM) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ECOMM.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ECONNABORTED) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ECONNABORTED.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ECONNREFUSED) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ECONNREFUSED.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ECONNRESET) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ECONNRESET.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EDEADLK) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EDEADLK.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EDEADLOCK) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EDEADLOCK.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EDESTADDRREQ) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EDESTADDRREQ.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EFAULT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EFAULT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EHOSTDOWN) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EHOSTDOWN.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EHOSTUNREACH) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EHOSTUNREACH.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EILSEQ) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EILSEQ.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EINPROGRESS) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EINPROGRESS.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EINTR) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EINTR.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EIO) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EIO.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ELIBACC) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ELIBACC.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ELIBBAD) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ELIBBAD.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ELIBMAX) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ELIBMAX.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ELOOP) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ELOOP.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EMSGSIZE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EMSGSIZE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EMULTIHOP) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EMULTIHOP.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOBUFS) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOBUFS.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENODATA) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENODATA.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENODEV) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENODEV.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOLCK) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOLCK.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOMEM) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOMEM.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOMSG) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOMSG.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOPROTOOPT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOPROTOOPT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOSPC) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOSPC.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOSYS) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOSYS.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOTBLK) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOTBLK.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOTTY) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOTTY.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOTUNIQ) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOTUNIQ.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENXIO) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENXIO.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EOVERFLOW) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EOVERFLOW.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPERM) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EPERM.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPFNOSUPPORT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EPFNOSUPPORT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPIPE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EPIPE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPROTO) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EPROTO.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPROTONOSUPPORT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EPROTONOSUPPORT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ERANGE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ERANGE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EREMCHG) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EREMCHG.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EREMOTE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EREMOTE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EREMOTEIO) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EREMOTEIO.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ERESTART) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ERESTART.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ESRCH) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ESRCH.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ETIME) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ETIME.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ETIMEDOUT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ETIMEDOUT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ETXTBSY) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ETXTBSY.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EUCLEAN) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EUCLEAN.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EUNATCH) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EUNATCH.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EUSERS) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EUSERS.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EWOULDBLOCK) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EWOULDBLOCK.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EXDEV) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EXDEV.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EXFULL) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EXFULL.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EOPNOTSUPP) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EOPNOTSUPP.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPROBE_DEFER) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EPROBE_DEFER.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOENT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOENT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else {
				dev_err(&chip->client->dev,
					"%s - Unexpected I2C error reading byte data. Address: '0x%02x', Return: '%d'.  Attempt #%d / %d...\n",
					__func__, address, ret, i,
					chip->numRetriesI2C);
			}
		} else		// Successful i2c writes should always return 0
		{
			*data = (FSC_U8) ret;
			break;
		}
	}

	mutex_unlock(&chip->lock);
	return (ret >= 0);
}

FSC_BOOL fusb_I2C_ReadBlockData(FSC_U8 address, FSC_U8 length, FSC_U8 * data)
{
	FSC_S32 i = 0;
	FSC_S32 ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (chip == NULL || chip->client == NULL || data == NULL) {
		pr_err("%s - Error: %s is NULL!\n", __func__,
		       (chip ==
			NULL ? "Internal chip structure" : (chip->client ==
							    NULL ? "I2C Client"
							    :
							    "block read data buffer")));
		return false;
	}

	mutex_lock(&chip->lock);

	// Retry on failure up to the retry limit
	for (i = 0; i <= chip->numRetriesI2C; i++) {
		ret = i2c_smbus_read_i2c_block_data(chip->client, (u8) address, (u8) length, (u8 *) data);	// Read a byte of data from address
		if (ret < 0)	// Errors report as negative
		{
			if (ret == -ERANGE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ERANGE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EINVAL) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EINVAL.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EAGAIN) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EAGAIN.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EALREADY) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EALREADY.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EBADE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EBADE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EBADMSG) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EBADMSG.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EBUSY) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EBUSY.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ECANCELED) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ECANCELED.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ECOMM) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ECOMM.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ECONNABORTED) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ECONNABORTED.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ECONNREFUSED) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ECONNREFUSED.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ECONNRESET) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ECONNRESET.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EDEADLK) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EDEADLK.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EDEADLOCK) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EDEADLOCK.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EDESTADDRREQ) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EDESTADDRREQ.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EFAULT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EFAULT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EHOSTDOWN) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EHOSTDOWN.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EHOSTUNREACH) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EHOSTUNREACH.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EILSEQ) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EILSEQ.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EINPROGRESS) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EINPROGRESS.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EINTR) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EINTR.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EIO) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EIO.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ELIBACC) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ELIBACC.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ELIBBAD) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ELIBBAD.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ELIBMAX) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ELIBMAX.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ELOOP) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ELOOP.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EMSGSIZE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EMSGSIZE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EMULTIHOP) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EMULTIHOP.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOBUFS) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOBUFS.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENODATA) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENODATA.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENODEV) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENODEV.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOLCK) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOLCK.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOMEM) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOMEM.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOMSG) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOMSG.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOPROTOOPT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOPROTOOPT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOSPC) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOSPC.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOSYS) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOSYS.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOTBLK) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOTBLK.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOTTY) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOTTY.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOTUNIQ) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENOTUNIQ.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENXIO) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ENXIO.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EOVERFLOW) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EOVERFLOW.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPERM) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EPERM.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPFNOSUPPORT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EPFNOSUPPORT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPIPE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EPIPE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPROTO) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EPROTO.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPROTONOSUPPORT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EPROTONOSUPPORT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ERANGE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ERANGE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EREMCHG) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EREMCHG.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EREMOTE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EREMOTE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EREMOTEIO) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EREMOTEIO.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ERESTART) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ERESTART.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ESRCH) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ESRCH.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ETIME) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ETIME.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ETIMEDOUT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ETIMEDOUT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ETXTBSY) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ETXTBSY.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EUCLEAN) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EUCLEAN.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EUNATCH) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EUNATCH.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EUSERS) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EUSERS.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EWOULDBLOCK) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EWOULDBLOCK.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EXDEV) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EXDEV.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EXFULL) {
				dev_err(&chip->client->dev,
					"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EXFULL.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EOPNOTSUPP) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EOPNOTSUPP.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -EPROBE_DEFER) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EPROBE_DEFER.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == -ENOENT) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ENOENT.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else {
				dev_err(&chip->client->dev,
					"%s - Unexpected I2C error block reading byte data. Address: '0x%02x', Return: '%d'.  Attempt #%d / %d...\n",
					__func__, address, ret, i,
					chip->numRetriesI2C);
			}
		} else if (ret != length)	// We didn't read everything we wanted
		{
			dev_err(&chip->client->dev,
				"%s - Error: Block read request of %u bytes truncated to %u bytes.\n",
				__func__, length, I2C_SMBUS_BLOCK_MAX);
		} else {
			break;	// Success, don't retry
		}
	}

	mutex_unlock(&chip->lock);
	return (ret == length);
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************        Timer Interface        ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
/* Tick SM every 1ms -> 1000000ns*/
static const unsigned long g_fusb_timer_tick_period_ns = 1000000;

/*******************************************************************************
* Function:        _fusb_TimerHandler
* Input:           timer: hrtimer struct to be handled
* Return:          HRTIMER_RESTART to restart the timer, or HRTIMER_NORESTART otherwise
* Description:     Ticks state machine timer counters and rearms itself
********************************************************************************/
enum hrtimer_restart _fusb_TimerHandler(struct hrtimer *timer)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return HRTIMER_NORESTART;
	}

	if (!timer) {
		pr_err("FUSB  %s - Error: High-resolution timer is NULL!\n",
		       __func__);
		return HRTIMER_NORESTART;
	}

	core_tick();

#ifdef FSC_DEBUG
	if (chip->dbgTimerTicks++ >= U8_MAX) {
		chip->dbgTimerRollovers++;
	}
#endif // FSC_DEBUG

	// Reset the timer expiration
	hrtimer_forward(timer, ktime_get(),
			ktime_set(0, g_fusb_timer_tick_period_ns));

	return HRTIMER_RESTART;	// Requeue the timer
}

void fusb_InitializeTimer(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}

	hrtimer_init(&chip->timer_state_machine, CLOCK_MONOTONIC, HRTIMER_MODE_REL);	// Init the timer structure
	chip->timer_state_machine.function = _fusb_TimerHandler;	// Assign the callback to call when time runs out

	FUSB_LOG("FUSB  %s - Timer initialized!\n", __func__);
}

void fusb_StartTimers(void)
{
	ktime_t ktime;
	struct fusb30x_chip *chip;

	chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}
#ifdef FSC_DEBUG
	/* Reset our debug timer counters */
	chip->dbgTimerTicks = 0;
	chip->dbgTimerRollovers = 0;
#endif // FSC_DEBUG


	ktime = ktime_set(0, g_fusb_timer_tick_period_ns);	// Convert our timer period (in ns) to ktime
	hrtimer_start(&chip->timer_state_machine, ktime, HRTIMER_MODE_REL);	// Start the timer
	FUSB_LOG("FUSB  %s - Timer started!\n", __func__);

}

void fusb_StopTimers(void)
{
	FSC_S32 ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}
	mutex_lock(&chip->lock);
	if (hrtimer_active(&chip->timer_state_machine) != 0) {
		ret = hrtimer_cancel(&chip->timer_state_machine);
		FUSB_LOG("%s - Active state machine hrtimer canceled: %d\n",
			 __func__, ret);
	}
	if (hrtimer_is_queued(&chip->timer_state_machine) != 0) {
		ret = hrtimer_cancel(&chip->timer_state_machine);
		FUSB_LOG("%s - Queued state machine hrtimer canceled: %d\n",
			 __func__, ret);
	}
	mutex_unlock(&chip->lock);
	FUSB_LOG("FUSB  %s - Timer stopped!\n", __func__);
}

// Get the max value that we can delay in 10us increments at compile time
static const FSC_U32 MAX_DELAY_10US = (UINT_MAX / 10);
void fusb_Delay10us(FSC_U32 delay10us)
{
	FSC_U32 us = 0;
	if (delay10us > MAX_DELAY_10US) {
		pr_err
		    ("%s - Error: Delay of '%u' is too long! Must be less than '%u'.\n",
		     __func__, delay10us, MAX_DELAY_10US);
		return;
	}

	us = delay10us * 10;	// Convert to microseconds (us)
	if (us <= 10)		// Best practice is to use udelay() for < ~10us times
	{
		udelay(us);	// BLOCKING delay for < 10us
	} else if (us < 20000)	// Best practice is to use usleep_range() for 10us-20ms
	{
		// TODO - optimize this range, probably per-platform
		usleep_range(us, us + (us / 10));	// Non-blocking sleep for at least the requested time, and up to the requested time + 10%
	} else			// Best practice is to use msleep() for > 20ms
	{
		msleep(us / 1000);	// Convert to ms. Non-blocking, low-precision sleep
	}
}

#ifdef FSC_DEBUG
/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************        SysFS Interface        ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*******************************************************************************
* Function:        fusb_timestamp_bytes_to_time
* Input:           outSec: Seconds part of output is stored here
*                  outMS10ths: 10ths of MS part of output is stored here
*                  inBuf: Ptr to first of 4 timestamp bytes, where the timestamp is in this format:
*                    [HI-10thsMS LO-10thsMS HI-Sec LO-Sec]
* Return:          None
* Description:     Parses the 4 bytes in inBuf into a 2-part timestamp: Seconds and 10ths of MS
********************************************************************************/
void fusb_timestamp_bytes_to_time(FSC_U32 * outSec, FSC_U32 * outMS10ths,
				  FSC_U8 * inBuf)
{
	if (outSec && outMS10ths && inBuf) {
		*outMS10ths = inBuf[0];
		*outMS10ths = *outMS10ths << 8;
		*outMS10ths |= inBuf[1];

		*outSec = inBuf[2];
		*outSec = *outSec << 8;
		*outSec |= inBuf[3];
	}
}

/*******************************************************************************
* Function:        fusb_get_pd_message_type
* Input:           header: PD message header. Bits 4..0 are the pd message type, bits 14..12 are num data objs
*                  out: Buffer to which the message type will be written, should be at least 32 bytes long
* Return:          int - Number of chars written to out, negative on error
* Description:     Parses both PD message header bytes for the message type as a null-terminated string.
********************************************************************************/
FSC_S32 fusb_get_pd_message_type(FSC_U16 header, FSC_U8 * out)
{
	FSC_S32 numChars = -1;	// Number of chars written, return value
	if ((!out) || !(out + 31))	// Check for our 32 byte buffer
	{
		pr_err("%s FUSB - Error: Invalid input buffer! header: 0x%x\n",
		       __func__, header);
		return -1;
	}
	// Bits 14..12 give num of data obj. This is a data message if there are data objects, otherwise it's a control message
	// See the PD spec, Table 6-1 "Message Header", for more details.
	if ((header & 0x7000) > 0) {
		switch (header & 0x0F) {
		case DMTSourceCapabilities:	// Source Capabilities
			{
				numChars = sprintf(out, "Source Capabilities");
				break;
			}
		case DMTRequest:	// Request
			{
				numChars = sprintf(out, "Request");
				break;
			}
		case DMTBIST:	// BIST
			{
				numChars = sprintf(out, "BIST");
				break;
			}
		case DMTSinkCapabilities:	// Sink Capabilities
			{
				numChars = sprintf(out, "Sink Capabilities");
				break;
			}
		case 0b00101:	// Battery Status
			{
				numChars = sprintf(out, "Battery Status");
				break;
			}
		case 0b00110:	// Source Alert
			{
				numChars = sprintf(out, "Source Alert");
				break;
			}
		case DMTVenderDefined:	// Vendor Defined
			{
				numChars = sprintf(out, "Vendor Defined");
				break;
			}
		default:	// Reserved/unused/unknown
			{
				numChars =
				    sprintf(out, "Reserved (Data) (0x%x)",
					    header);
				break;
			}
		}
	} else {
		switch (header & 0x0F) {
		case CMTGoodCRC:	// Good CRC
			{
				numChars = sprintf(out, "Good CRC");
				break;
			}
		case CMTGotoMin:	// Go to min
			{
				numChars = sprintf(out, "Go to Min");
				break;
			}
		case CMTAccept:	// Accept
			{
				numChars = sprintf(out, "Accept");
				break;
			}
		case CMTReject:	// Reject
			{
				numChars = sprintf(out, "Reject");
				break;
			}
		case CMTPing:	// Ping
			{
				numChars = sprintf(out, "Ping");
				break;
			}
		case CMTPS_RDY:	// PS_RDY
			{
				numChars = sprintf(out, "PS_RDY");
				break;
			}
		case CMTGetSourceCap:	// Get Source Cap
			{
				numChars =
				    sprintf(out, "Get Source Capabilities");
				break;
			}
		case CMTGetSinkCap:	// Get Sink Cap
			{
				numChars =
				    sprintf(out, "Get Sink Capabilities");
				break;
			}
		case CMTDR_Swap:	// Data Role Swap
			{
				numChars = sprintf(out, "Data Role Swap");
				break;
			}
		case CMTPR_Swap:	// Power Role Swap
			{
				numChars = sprintf(out, "Power Role Swap");
				break;
			}
		case CMTVCONN_Swap:	// VConn Swap
			{
				numChars = sprintf(out, "VConn Swap");
				break;
			}
		case CMTWait:	// Wait
			{
				numChars = sprintf(out, "Wait");
				break;
			}
		case CMTSoftReset:	// Soft Reset
			{
				numChars = sprintf(out, "Soft Reset");
				break;
			}
		case 0b01110:	// Not Supported
			{
				numChars = sprintf(out, "Not Supported");
				break;
			}
		case 0b01111:	// Get Source Cap Extended
			{
				numChars = sprintf(out, "Get Source Cap Ext");
				break;
			}
		case 0b10000:	// Get Source Status
			{
				numChars = sprintf(out, "Get Source Status");
				break;
			}
		case 0b10001:	// FR Swap
			{
				numChars = sprintf(out, "FR Swap");
				break;
			}
		default:	// Reserved/unused/unknown
			{
				numChars =
				    sprintf(out, "Reserved (CMD) (0x%x)",
					    header);
				break;
			}
		}
	}
	return numChars;
}

/*******************************************************************************
* Function:        fusb_Sysfs_Handle_Read
* Input:           output: Buffer to which the output will be written
* Return:          Number of chars written to output
* Description:     Reading this file will output the most recently saved hostcomm output buffer
********************************************************************************/
#define FUSB_MAX_BUF_SIZE 256	// Arbitrary temp buffer for parsing out driver data to sysfs
static ssize_t _fusb_Sysfs_Hostcomm_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	FSC_S32 i = 0;
	FSC_S32 numLogs = 0;
	FSC_S32 numChars = 0;
	FSC_U32 TimeStampSeconds = 0;	// Timestamp value in seconds
	FSC_U32 TimeStampMS10ths = 0;	// Timestamp fraction in 10ths of milliseconds
	FSC_S8 tempBuf[FUSB_MAX_BUF_SIZE] = { 0 };
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (chip == NULL) {
		pr_err("%s - Chip structure is null!\n", __func__);
	} else if (buf == NULL || chip->HostCommBuf == NULL) {
		pr_err("%s - Buffer is null!\n", __func__);
	} else if (chip->HostCommBuf[0] == CMD_READ_PD_STATE_LOG)	// Parse out the PD state log
	{
		numLogs = chip->HostCommBuf[3];
		/* First byte echos the command, 4th byte is number of logs (2nd and 3rd bytes reserved as 0) */
		numChars += sprintf(tempBuf, "PD State Log has %u entries:\n", numLogs);	// Copy string + null terminator
		strcat(buf, tempBuf);

		/* Relevant data starts at 5th byte in this format: CMD 0 0 #Logs PDState time time time time */
		for (i = 4; (i + 4 < FSC_HOSTCOMM_BUFFER_SIZE) && (numChars < PAGE_SIZE) && (numLogs > 0); i += 5, numLogs--)	// Must be able to peek 4 bytes ahead, and don't overflow the output buffer (PAGE_SIZE)
		{
			fusb_timestamp_bytes_to_time(&TimeStampSeconds,
						     &TimeStampMS10ths,
						     &chip->HostCommBuf[i + 1]);

			// sprintf should be safe here because we're controlling the strings being printed, just make sure the strings are less than FUSB_MAX_BUF_SIZE+1
			switch (chip->HostCommBuf[i]) {
			case peDisabled:	// Policy engine is disabled
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDisabled\t\tPolicy engine is disabled\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peErrorRecovery:	// Error recovery state
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeErrorRecovery\t\tError recovery state\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceHardReset:	// Received a hard reset
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceHardReset\t\tReceived a hard reset\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceSendHardReset:	// Source send a hard reset
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceSendHardReset\t\tSource send a hard reset\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceSoftReset:	// Received a soft reset
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceSoftReset\t\tReceived a soft reset\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceSendSoftReset:	// Send a soft reset
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceSendSoftReset\t\tSend a soft reset\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceStartup:	// Initial state
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceStartup\t\tInitial state\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceSendCaps:	// Send the source capabilities
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceSendCaps\t\tSend the source capabilities\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceDiscovery:	// Waiting to detect a USB PD sink
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceDiscovery\t\tWaiting to detect a USB PD sink\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceDisabled:	// Disabled state
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceDisabled\t\tDisabled state\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceTransitionDefault:	// Transition to default 5V state
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceTransitionDefault\t\tTransition to default 5V state\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceNegotiateCap:	// Negotiate capability and PD contract
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceNegotiateCap\t\tNegotiate capability and PD contract\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceCapabilityResponse:	// Respond to a request message with a reject/wait
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceCapabilityResponse\t\tRespond to a request message with a reject/wait\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceTransitionSupply:	// Transition the power supply to the new setting (accept request)
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceTransitionSupply\t\tTransition the power supply to the new setting (accept request)\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceReady:	// Contract is in place and output voltage is stable
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceReady\t\tContract is in place and output voltage is stable\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceGiveSourceCaps:	// State to resend source capabilities
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceGiveSourceCaps\t\tState to resend source capabilities\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceGetSinkCaps:	// State to request the sink capabilities
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceGetSinkCaps\t\tState to request the sink capabilities\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceSendPing:	// State to send a ping message
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceSendPing\t\tState to send a ping message\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceGotoMin:	// State to send the gotoMin and ready the power supply
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceGotoMin\t\tState to send the gotoMin and ready the power supply\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceGiveSinkCaps:	// State to send the sink capabilities if dual-role
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceGiveSinkCaps\t\tState to send the sink capabilities if dual-role\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceGetSourceCaps:	// State to request the source caps from the UFP
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceGetSourceCaps\t\tState to request the source caps from the UFP\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceSendDRSwap:	// State to send a DR_Swap message
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceSendDRSwap\t\tState to send a DR_Swap message\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceEvaluateDRSwap:	// Evaluate whether we are going to accept or reject the swap
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceEvaluateDRSwap\t\tEvaluate whether we are going to accept or reject the swap\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkHardReset:	// Received a hard reset
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkHardReset\t\tReceived a hard reset\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkSendHardReset:	// Sink send hard reset
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkSendHardReset\t\tSink send hard reset\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkSoftReset:	// Sink soft reset
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkSoftReset\t\tSink soft reset\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkSendSoftReset:	// Sink send soft reset
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkSendSoftReset\t\tSink send soft reset\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkTransitionDefault:	// Transition to the default state
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkTransitionDefault\t\tTransition to the default state\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkStartup:	// Initial sink state
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkStartup\t\tInitial sink state\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkDiscovery:	// Sink discovery state
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkDiscovery\t\tSink discovery state\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkWaitCaps:	// Sink wait for capabilities state
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkWaitCaps\t\tSink wait for capabilities state\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkEvaluateCaps:	// Sink state to evaluate the received source capabilities
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkEvaluateCaps\t\tSink state to evaluate the received source capabilities\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkSelectCapability:	// Sink state for selecting a capability
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkSelectCapability\t\tSink state for selecting a capability\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkTransitionSink:	// Sink state for transitioning the current power
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkTransitionSink\t\tSink state for transitioning the current power\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkReady:	// Sink ready state
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkReady\t\tSink ready state\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkGiveSinkCap:	// Sink send capabilities state
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkGiveSinkCap\t\tSink send capabilities state\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkGetSourceCap:	// Sink get source capabilities state
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkGetSourceCap\t\tSink get source capabilities state\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkGetSinkCap:	// Sink state to get the sink capabilities of the connected source
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkGetSinkCap\t\tSink state to get the sink capabilities of the connected source\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkGiveSourceCap:	// Sink state to send the source capabilities if dual-role
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkGiveSourceCap\t\tSink state to send the source capabilities if dual-role\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkSendDRSwap:	// State to send a DR_Swap message
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkSendDRSwap\t\tState to send a DR_Swap message\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkEvaluateDRSwap:	// Evaluate whether we are going to accept or reject the swap
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkEvaluateDRSwap\t\tEvaluate whether we are going to accept or reject the swap\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceSendVCONNSwap:	// Initiate a VCONN swap sequence
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceSendVCONNSwap\t\tInitiate a VCONN swap sequence\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkEvaluateVCONNSwap:	// Evaluate whether we are going to accept or reject the swap
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkEvaluateVCONNSwap\t\tEvaluate whether we are going to accept or reject the swap\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceSendPRSwap:	// Initiate a PR_Swap sequence
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceSendPRSwap\t\tInitiate a PR_Swap sequence\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceEvaluatePRSwap:	// Evaluate whether we are going to accept or reject the swap
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceEvaluatePRSwap\t\tEvaluate whether we are going to accept or reject the swap\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkSendPRSwap:	// Initiate a PR_Swap sequence
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkSendPRSwap\t\tInitiate a PR_Swap sequence\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSinkEvaluatePRSwap:	// Evaluate whether we are going to accept or reject the swap
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSinkEvaluatePRSwap\t\tEvaluate whether we are going to accept or reject the swap\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peGiveVdm:	// Send VDM data
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeGiveVdm\t\tSend VDM data\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peUfpVdmGetIdentity:	// Requesting Identity information from DPM
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeUfpVdmGetIdentity\t\tRequesting Identity information from DPM\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peUfpVdmSendIdentity:	// Sending Discover Identity ACK
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeUfpVdmSendIdentity\t\tSending Discover Identity ACK\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peUfpVdmGetSvids:	// Requesting SVID info from DPM
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeUfpVdmGetSvids\t\tRequesting SVID info from DPM\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peUfpVdmSendSvids:	// Sending Discover SVIDs ACK
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeUfpVdmSendSvids\t\tSending Discover SVIDs ACK\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peUfpVdmGetModes:	// Requesting Mode info from DPM
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeUfpVdmGetModes\t\tRequesting Mode info from DPM\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peUfpVdmSendModes:	// Sending Discover Modes ACK
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeUfpVdmSendModes\t\tSending Discover Modes ACK\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peUfpVdmEvaluateModeEntry:	// Requesting DPM to evaluate request to enter a mode, and enter if OK
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeUfpVdmEvaluateModeEntry\t\tRequesting DPM to evaluate request to enter a mode, and enter if OK\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peUfpVdmModeEntryNak:	// Sending Enter Mode NAK response
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeUfpVdmModeEntryNak\t\tSending Enter Mode NAK response\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peUfpVdmModeEntryAck:	// Sending Enter Mode ACK response
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeUfpVdmModeEntryAck\t\tSending Enter Mode ACK response\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peUfpVdmModeExit:	// Requesting DPM to evalute request to exit mode
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeUfpVdmModeExit\t\tRequesting DPM to evalute request to exit mode\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peUfpVdmModeExitNak:	// Sending Exit Mode NAK reponse
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeUfpVdmModeExitNak\t\tSending Exit Mode NAK reponse\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peUfpVdmModeExitAck:	// Sending Exit Mode ACK Response
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeUfpVdmModeExitAck\t\tSending Exit Mode ACK Response\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peUfpVdmAttentionRequest:	// Sending Attention Command
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeUfpVdmAttentionRequest\t\tSending Attention Command\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpUfpVdmIdentityRequest:	// Sending Identity Request
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpUfpVdmIdentityRequest\t\tSending Identity Request\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpUfpVdmIdentityAcked:	// Inform DPM of Identity
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpUfpVdmIdentityAcked\t\tInform DPM of Identity\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpUfpVdmIdentityNaked:	// Inform DPM of result
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpUfpVdmIdentityNaked\t\tInform DPM of result\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpCblVdmIdentityRequest:	// Sending Identity Request
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpCblVdmIdentityRequest\t\tSending Identity Request\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpCblVdmIdentityAcked:	// Inform DPM
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpCblVdmIdentityAcked\t\tInform DPM\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpCblVdmIdentityNaked:	// Inform DPM
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpCblVdmIdentityNaked\t\tInform DPM\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpVdmSvidsRequest:	// Sending Discover SVIDs request
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpVdmSvidsRequest\t\tSending Discover SVIDs request\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpVdmSvidsAcked:	// Inform DPM
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpVdmSvidsAcked\t\tInform DPM\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpVdmSvidsNaked:	// Inform DPM
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpVdmSvidsNaked\t\tInform DPM\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpVdmModesRequest:	// Sending Discover Modes request
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpVdmModesRequest\t\tSending Discover Modes request\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpVdmModesAcked:	// Inform DPM
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpVdmModesAcked\t\tInform DPM\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpVdmModesNaked:	// Inform DPM
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpVdmModesNaked\t\tInform DPM\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpVdmModeEntryRequest:	// Sending Mode Entry request
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpVdmModeEntryRequest\t\tSending Mode Entry request\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpVdmModeEntryAcked:	// Inform DPM
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpVdmModeEntryAcked\t\tInform DPM\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpVdmModeEntryNaked:	// Inform DPM
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpVdmModeEntryNaked\t\tInform DPM\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpVdmModeExitRequest:	// Sending Exit Mode request
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpVdmModeExitRequest\t\tSending Exit Mode request\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpVdmExitModeAcked:	// Inform DPM
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpVdmExitModeAcked\t\tInform DPM\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSrcVdmIdentityRequest:	// sending Discover Identity request
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSrcVdmIdentityRequest\t\tsending Discover Identity request\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSrcVdmIdentityAcked:	// inform DPM
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSrcVdmIdentityAcked\t\tinform DPM\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSrcVdmIdentityNaked:	// inform DPM
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSrcVdmIdentityNaked\t\tinform DPM\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDfpVdmAttentionRequest:	// Attention Request received
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDfpVdmAttentionRequest\t\tAttention Request received\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblReady:	// Cable power up state?
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblReady\t\tCable power up state?\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblGetIdentity:	// Discover Identity request received
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblGetIdentity\t\tDiscover Identity request received\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblGetIdentityNak:	// Respond with NAK
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblGetIdentityNak\t\tRespond with NAK\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblSendIdentity:	// Respond with Ack
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblSendIdentity\t\tRespond with Ack\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblGetSvids:	// Discover SVIDs request received
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblGetSvids\t\tDiscover SVIDs request received\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblGetSvidsNak:	// Respond with NAK
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblGetSvidsNak\t\tRespond with NAK\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblSendSvids:	// Respond with ACK
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblSendSvids\t\tRespond with ACK\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblGetModes:	// Discover Modes request received
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblGetModes\t\tDiscover Modes request received\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblGetModesNak:	// Respond with NAK
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblGetModesNak\t\tRespond with NAK\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblSendModes:	// Respond with ACK
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblSendModes\t\tRespond with ACK\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblEvaluateModeEntry:	// Enter Mode request received
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblEvaluateModeEntry\t\tEnter Mode request received\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblModeEntryAck:	// Respond with NAK
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblModeEntryAck\t\tRespond with NAK\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblModeEntryNak:	// Respond with ACK
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblModeEntryNak\t\tRespond with ACK\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblModeExit:	// Exit Mode request received
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblModeExit\t\tExit Mode request received\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblModeExitAck:	// Respond with NAK
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblModeExitAck\t\tRespond with NAK\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peCblModeExitNak:	// Respond with ACK
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeCblModeExitNak\t\tRespond with ACK\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peDpRequestStatus:	// Requesting PP Status
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeDpRequestStatus\t\tRequesting PP Status\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case PE_BIST_Receive_Mode:	// Bist Receive Mode
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tPE_BIST_Receive_Mode\t\tBist Receive Mode\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case PE_BIST_Frame_Received:	// Test Frame received by Protocol layer
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tPE_BIST_Frame_Received\t\tTest Frame received by Protocol layer\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case PE_BIST_Carrier_Mode_2:	// BIST Carrier Mode 2
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tPE_BIST_Carrier_Mode_2\t\tBIST Carrier Mode 2\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case peSourceWaitNewCapabilities:	// Wait for new Source Capabilities from Policy Manager
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tpeSourceWaitNewCapabilities\t\tWait for new Source Capabilities from Policy Manager\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case dbgGetRxPacket:	// Debug point for measuring Rx packet handling in ProtocolGetRxPacket()
				{
					// Special parsing for this state - TimeStampSeconds is really the number of I2C reads performed, and TimeStampMS10ths is the time elapsed (in ms)
					numChars +=
					    sprintf(tempBuf,
						    "%02u|0.%04u\tdbgGetRxPacket\t\t\tNumber of I2C bytes read | Time elapsed\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					//    numChars += sprintf(tempBuf, "%u | %u\tdbgGetRxPacket\t\t\tHeader[0] | Header[1]", chip->HostCommBuf[i + 1], chip->HostCommBuf[i + 3]);
					strcat(buf, tempBuf);
					break;
				}

			case dbgSendTxPacket:	// Debug point for measuring Tx packet handling in ProtocolTransmitMessage()
				{
					// Special parsing for this state - TimeStampSeconds is really the number of I2C reads performed, and TimeStampMS10ths is the time elapsed (in ms)
					numChars +=
					    sprintf(tempBuf,
						    "%02u|0.%04u\tdbgGetTxPacket\t\t\tNumber of I2C bytes sent | Time elapsed\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					//    numChars += sprintf(tempBuf, "%u | %u\tdbgGetRxPacket\t\t\tHeader[0] | Header[1]", chip->HostCommBuf[i + 1], chip->HostCommBuf[i + 3]);
					strcat(buf, tempBuf);
					break;
				}

			default:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tUKNOWN STATE: 0x%02x\n",
						    TimeStampSeconds,
						    TimeStampMS10ths,
						    chip->HostCommBuf[i]);
					strcat(buf, tempBuf);
					break;
				}
			}
		}
		strcat(buf, "\n");	// Append a newline for pretty++
		numChars++;	// Account for newline
	} else if (chip->HostCommBuf[0] == CMD_READ_STATE_LOG)	// Parse out the Type-C state log
	{
		numLogs = chip->HostCommBuf[3];
		/* First byte echos the command, 4th byte is number of logs (2nd and 3rd bytes reserved as 0) */
		numChars += sprintf(tempBuf, "Type-C State Log has %u entries:\n", numLogs);	// Copy string + null terminator
		strcat(buf, tempBuf);

		/* Relevant data starts at 5th byte in this format: CMD 0 0 #Logs State time time time time */
		for (i = 4; (i + 4 < FSC_HOSTCOMM_BUFFER_SIZE) && (numChars < PAGE_SIZE) && (numLogs > 0); i += 5, numLogs--)	// Must be able to peek 4 bytes ahead, and don't overflow the output buffer (PAGE_SIZE), only print logs we have
		{
			// Parse out the timestamp
			fusb_timestamp_bytes_to_time(&TimeStampSeconds,
						     &TimeStampMS10ths,
						     &chip->HostCommBuf[i + 1]);

			// sprintf should be safe here because we're controlling the strings being printed, just make sure the strings are less than FUSB_MAX_BUF_SIZE+1
			switch (chip->HostCommBuf[i]) {
			case Disabled:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tDisabled\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case ErrorRecovery:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tErrorRecovery\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case Unattached:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tUnattached\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case AttachWaitSink:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tAttachWaitSink\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case AttachedSink:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tAttachedSink\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case AttachWaitSource:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tAttachWaitSource\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case AttachedSource:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tAttachedSource\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case TrySource:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tTrySource\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case TryWaitSink:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tTryWaitSink\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case TrySink:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tTrySink\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case TryWaitSource:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tTryWaitSource\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case AudioAccessory:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tAudioAccessory\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case DebugAccessorySource:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tDebugAccessorySource\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case AttachWaitAccessory:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tAttachWaitAccessory\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case PoweredAccessory:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tPoweredAccessory\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case UnsupportedAccessory:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tUnsupportedAccessory\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case DelayUnattached:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tDelayUnattached\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}

			case UnattachedSource:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tUnattachedSource\n",
						    TimeStampSeconds,
						    TimeStampMS10ths);
					strcat(buf, tempBuf);
					break;
				}
			default:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tUKNOWN STATE: 0x%02x\n",
						    TimeStampSeconds,
						    TimeStampMS10ths,
						    chip->HostCommBuf[i]);
					strcat(buf, tempBuf);
					break;
				}
			}
		}
		strcat(buf, "\n");	// Append a newline for pretty++
		numChars++;	// Account for newline
	} else {
		for (i = 0; i < FSC_HOSTCOMM_BUFFER_SIZE; i++) {
			numChars += scnprintf(tempBuf, 6 * sizeof(char), "0x%02x ", chip->HostCommBuf[i]);	// Copy 1 byte + null term
			strcat(buf, tempBuf);	// Append each number to the output buffer
		}
		strcat(buf, "\n");	// Append a newline for pretty++
		numChars++;	// Account for newline
	}
	return numChars;
}

/*******************************************************************************
* Function:        fusb_Sysfs_Handle_Write
* Input:           input: Buffer passed in from OS (space-separated list of 8-bit hex values)
*                  size: Number of chars in input
*                  output: Buffer to which the output will be written
* Return:          Number of chars written to output
* Description:     Performs hostcomm duties, and stores output buffer in chip structure
********************************************************************************/
static ssize_t _fusb_Sysfs_Hostcomm_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *input, size_t size)
{
	FSC_S32 ret = 0;
	FSC_S32 i = 0;
	FSC_S32 j = 0;
	FSC_S8 tempByte = 0;
	FSC_S32 numBytes = 0;
	FSC_S8 temp[6] = { 0 };	// Temp buffer to parse out individual hex numbers, +1 for null terminator
	FSC_S8 temp_input[FSC_HOSTCOMM_BUFFER_SIZE] = { 0 };
	FSC_S8 output[FSC_HOSTCOMM_BUFFER_SIZE] = { 0 };
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (chip == NULL) {
		pr_err("%s - Chip structure is null!\n", __func__);
	} else if (input == NULL) {
		pr_err("%s - Error: Input buffer is NULL!\n", __func__);
	} else {
		// Convert the buffer to hex values
		for (i = 0; i < size; i = i + j) {
			// Parse out a hex number (at most 5 chars: "0x## ")
			for (j = 0; (j < 5) && (j + i < size); j++) {
				// End of the hex number (space-delimited)
				if (input[i + j] == ' ') {
					break;	// We found a space, stop copying this number and convert it
				}

				temp[j] = input[i + j];	// Copy the non-space byte into the temp buffer
			}

			temp[++j] = 0;	// Add a null terminator and move past the space

			// We have a hex digit (hopefully), now convert it
			ret = kstrtou8(temp, 16, &tempByte);
			if (ret != 0) {
				pr_err
				    ("FUSB  %s - Error: Hostcomm input is not a valid hex value! Return: '%d'\n",
				     __func__, ret);
				return 0;	// Quit on error
			} else {
				temp_input[numBytes++] = tempByte;
				if (numBytes >= FSC_HOSTCOMM_BUFFER_SIZE) {
					break;
				}
			}
		}

		fusb_ProcessMsg(temp_input, output);	// Handle the message
		memcpy(chip->HostCommBuf, output, FSC_HOSTCOMM_BUFFER_SIZE);	// Copy input into temp buffer
	}

	return size;
}

/* Fetch and display the PD state log */
static ssize_t _fusb_Sysfs_PDStateLog_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	FSC_S32 i = 0;
	FSC_S32 numChars = 0;
	FSC_S32 numLogs = 0;
	FSC_U16 PDMessageHeader = 0;	// PD Message header bytes
	FSC_U32 TimeStampSeconds = 0;	// Timestamp value in seconds
	FSC_U32 TimeStampMS10ths = 0;	// Timestamp fraction in 10ths of milliseconds
	FSC_U8 MessageType[32] = { 0 };	// Temp buffer to parse the PD message type from the PD message header
	FSC_U8 output[FSC_HOSTCOMM_BUFFER_SIZE] = { 0 };
	FSC_U8 tempBuf[FUSB_MAX_BUF_SIZE] = { 0 };
	tempBuf[0] = CMD_READ_PD_STATE_LOG;	// To request the PD statelog from Hostcomm

	/* Get the PD State Log */
	fusb_ProcessMsg(tempBuf, output);

	numLogs = output[3];
	/* First byte echos the command, 4th byte is number of logs (2nd and 3rd bytes reserved as 0) */
	numChars += sprintf(tempBuf, "PD State Log has %u entries:\n", numLogs);	// Copy string + null terminator
	strcat(buf, tempBuf);

	/* Relevant data starts at 5th byte in this format: CMD 0 0 #Logs PDState time time time time */
	for (i = 4; (i + 4 < FSC_HOSTCOMM_BUFFER_SIZE) && (numChars < PAGE_SIZE) && (numLogs > 0); i += 5, numLogs--)	// Must be able to peek 4 bytes ahead, and don't overflow the output buffer (PAGE_SIZE)
	{
		// Parse out the timestamp

		// Parse out the timestamp
		if (output[i] != dbgGetRxPacket) {
			fusb_timestamp_bytes_to_time(&TimeStampSeconds,
						     &TimeStampMS10ths,
						     &output[i + 1]);
		}
		// sprintf should be safe here because we're controlling the strings being printed, just make sure the strings are less than FUSB_MAX_BUF_SIZE+1
		switch (output[i]) {
		case peDisabled:	// Policy engine is disabled
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDisabled\t\tPolicy engine is disabled\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peErrorRecovery:	// Error recovery state
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeErrorRecovery\t\tError recovery state\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceHardReset:	// Received a hard reset
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceHardReset\t\tReceived a hard reset\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceSendHardReset:	// Source send a hard reset
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceSendHardReset\t\tSource send a hard reset\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceSoftReset:	// Received a soft reset
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceSoftReset\t\tReceived a soft reset\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceSendSoftReset:	// Send a soft reset
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceSendSoftReset\t\tSend a soft reset\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceStartup:	// Initial state
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceStartup\t\tInitial state\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceSendCaps:	// Send the source capabilities
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceSendCaps\t\tSend the source capabilities\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceDiscovery:	// Waiting to detect a USB PD sink
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceDiscovery\t\tWaiting to detect a USB PD sink\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceDisabled:	// Disabled state
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceDisabled\t\tDisabled state\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceTransitionDefault:	// Transition to default 5V state
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceTransitionDefault\t\tTransition to default 5V state\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceNegotiateCap:	// Negotiate capability and PD contract
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceNegotiateCap\t\tNegotiate capability and PD contract\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceCapabilityResponse:	// Respond to a request message with a reject/wait
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceCapabilityResponse\t\tRespond to a request message with a reject/wait\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceTransitionSupply:	// Transition the power supply to the new setting (accept request)
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceTransitionSupply\t\tTransition the power supply to the new setting (accept request)\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceReady:	// Contract is in place and output voltage is stable
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceReady\t\tContract is in place and output voltage is stable\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceGiveSourceCaps:	// State to resend source capabilities
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceGiveSourceCaps\t\tState to resend source capabilities\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceGetSinkCaps:	// State to request the sink capabilities
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceGetSinkCaps\t\tState to request the sink capabilities\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceSendPing:	// State to send a ping message
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceSendPing\t\tState to send a ping message\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceGotoMin:	// State to send the gotoMin and ready the power supply
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceGotoMin\t\tState to send the gotoMin and ready the power supply\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceGiveSinkCaps:	// State to send the sink capabilities if dual-role
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceGiveSinkCaps\t\tState to send the sink capabilities if dual-role\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceGetSourceCaps:	// State to request the source caps from the UFP
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceGetSourceCaps\t\tState to request the source caps from the UFP\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceSendDRSwap:	// State to send a DR_Swap message
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceSendDRSwap\t\tState to send a DR_Swap message\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceEvaluateDRSwap:	// Evaluate whether we are going to accept or reject the swap
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceEvaluateDRSwap\t\tEvaluate whether we are going to accept or reject the swap\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkHardReset:	// Received a hard reset
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkHardReset\t\tReceived a hard reset\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkSendHardReset:	// Sink send hard reset
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkSendHardReset\t\tSink send hard reset\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkSoftReset:	// Sink soft reset
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkSoftReset\t\tSink soft reset\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkSendSoftReset:	// Sink send soft reset
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkSendSoftReset\t\tSink send soft reset\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkTransitionDefault:	// Transition to the default state
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkTransitionDefault\t\tTransition to the default state\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkStartup:	// Initial sink state
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkStartup\t\tInitial sink state\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkDiscovery:	// Sink discovery state
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkDiscovery\t\tSink discovery state\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkWaitCaps:	// Sink wait for capabilities state
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkWaitCaps\t\tSink wait for capabilities state\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkEvaluateCaps:	// Sink state to evaluate the received source capabilities
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkEvaluateCaps\t\tSink state to evaluate the received source capabilities\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkSelectCapability:	// Sink state for selecting a capability
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkSelectCapability\t\tSink state for selecting a capability\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkTransitionSink:	// Sink state for transitioning the current power
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkTransitionSink\t\tSink state for transitioning the current power\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkReady:	// Sink ready state
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkReady\t\tSink ready state\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkGiveSinkCap:	// Sink send capabilities state
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkGiveSinkCap\t\tSink send capabilities state\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkGetSourceCap:	// Sink get source capabilities state
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkGetSourceCap\t\tSink get source capabilities state\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkGetSinkCap:	// Sink state to get the sink capabilities of the connected source
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkGetSinkCap\t\tSink state to get the sink capabilities of the connected source\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkGiveSourceCap:	// Sink state to send the source capabilities if dual-role
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkGiveSourceCap\t\tSink state to send the source capabilities if dual-role\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkSendDRSwap:	// State to send a DR_Swap message
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkSendDRSwap\t\tState to send a DR_Swap message\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkEvaluateDRSwap:	// Evaluate whether we are going to accept or reject the swap
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkEvaluateDRSwap\t\tEvaluate whether we are going to accept or reject the swap\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceSendVCONNSwap:	// Initiate a VCONN swap sequence
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceSendVCONNSwap\t\tInitiate a VCONN swap sequence\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkEvaluateVCONNSwap:	// Evaluate whether we are going to accept or reject the swap
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkEvaluateVCONNSwap\t\tEvaluate whether we are going to accept or reject the swap\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceSendPRSwap:	// Initiate a PR_Swap sequence
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceSendPRSwap\t\tInitiate a PR_Swap sequence\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceEvaluatePRSwap:	// Evaluate whether we are going to accept or reject the swap
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceEvaluatePRSwap\t\tEvaluate whether we are going to accept or reject the swap\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkSendPRSwap:	// Initiate a PR_Swap sequence
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkSendPRSwap\t\tInitiate a PR_Swap sequence\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSinkEvaluatePRSwap:	// Evaluate whether we are going to accept or reject the swap
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSinkEvaluatePRSwap\t\tEvaluate whether we are going to accept or reject the swap\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peGiveVdm:	// Send VDM data
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeGiveVdm\t\tSend VDM data\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peUfpVdmGetIdentity:	// Requesting Identity information from DPM
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeUfpVdmGetIdentity\t\tRequesting Identity information from DPM\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peUfpVdmSendIdentity:	// Sending Discover Identity ACK
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeUfpVdmSendIdentity\t\tSending Discover Identity ACK\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peUfpVdmGetSvids:	// Requesting SVID info from DPM
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeUfpVdmGetSvids\t\tRequesting SVID info from DPM\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peUfpVdmSendSvids:	// Sending Discover SVIDs ACK
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeUfpVdmSendSvids\t\tSending Discover SVIDs ACK\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peUfpVdmGetModes:	// Requesting Mode info from DPM
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeUfpVdmGetModes\t\tRequesting Mode info from DPM\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peUfpVdmSendModes:	// Sending Discover Modes ACK
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeUfpVdmSendModes\t\tSending Discover Modes ACK\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peUfpVdmEvaluateModeEntry:	// Requesting DPM to evaluate request to enter a mode, and enter if OK
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeUfpVdmEvaluateModeEntry\t\tRequesting DPM to evaluate request to enter a mode, and enter if OK\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peUfpVdmModeEntryNak:	// Sending Enter Mode NAK response
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeUfpVdmModeEntryNak\t\tSending Enter Mode NAK response\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peUfpVdmModeEntryAck:	// Sending Enter Mode ACK response
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeUfpVdmModeEntryAck\t\tSending Enter Mode ACK response\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peUfpVdmModeExit:	// Requesting DPM to evalute request to exit mode
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeUfpVdmModeExit\t\tRequesting DPM to evalute request to exit mode\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peUfpVdmModeExitNak:	// Sending Exit Mode NAK reponse
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeUfpVdmModeExitNak\t\tSending Exit Mode NAK reponse\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peUfpVdmModeExitAck:	// Sending Exit Mode ACK Response
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeUfpVdmModeExitAck\t\tSending Exit Mode ACK Response\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peUfpVdmAttentionRequest:	// Sending Attention Command
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeUfpVdmAttentionRequest\t\tSending Attention Command\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpUfpVdmIdentityRequest:	// Sending Identity Request
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpUfpVdmIdentityRequest\t\tSending Identity Request\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpUfpVdmIdentityAcked:	// Inform DPM of Identity
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpUfpVdmIdentityAcked\t\tInform DPM of Identity\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpUfpVdmIdentityNaked:	// Inform DPM of result
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpUfpVdmIdentityNaked\t\tInform DPM of result\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpCblVdmIdentityRequest:	// Sending Identity Request
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpCblVdmIdentityRequest\t\tSending Identity Request\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpCblVdmIdentityAcked:	// Inform DPM
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpCblVdmIdentityAcked\t\tInform DPM\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpCblVdmIdentityNaked:	// Inform DPM
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpCblVdmIdentityNaked\t\tInform DPM\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpVdmSvidsRequest:	// Sending Discover SVIDs request
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpVdmSvidsRequest\t\tSending Discover SVIDs request\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpVdmSvidsAcked:	// Inform DPM
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpVdmSvidsAcked\t\tInform DPM\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpVdmSvidsNaked:	// Inform DPM
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpVdmSvidsNaked\t\tInform DPM\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpVdmModesRequest:	// Sending Discover Modes request
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpVdmModesRequest\t\tSending Discover Modes request\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpVdmModesAcked:	// Inform DPM
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpVdmModesAcked\t\tInform DPM\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpVdmModesNaked:	// Inform DPM
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpVdmModesNaked\t\tInform DPM\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpVdmModeEntryRequest:	// Sending Mode Entry request
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpVdmModeEntryRequest\t\tSending Mode Entry request\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpVdmModeEntryAcked:	// Inform DPM
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpVdmModeEntryAcked\t\tInform DPM\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpVdmModeEntryNaked:	// Inform DPM
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpVdmModeEntryNaked\t\tInform DPM\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpVdmModeExitRequest:	// Sending Exit Mode request
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpVdmModeExitRequest\t\tSending Exit Mode request\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpVdmExitModeAcked:	// Inform DPM
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpVdmExitModeAcked\t\tInform DPM\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSrcVdmIdentityRequest:	// sending Discover Identity request
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSrcVdmIdentityRequest\t\tsending Discover Identity request\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSrcVdmIdentityAcked:	// inform DPM
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSrcVdmIdentityAcked\t\tinform DPM\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSrcVdmIdentityNaked:	// inform DPM
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSrcVdmIdentityNaked\t\tinform DPM\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDfpVdmAttentionRequest:	// Attention Request received
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDfpVdmAttentionRequest\t\tAttention Request received\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblReady:	// Cable power up state?
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblReady\t\tCable power up state?\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblGetIdentity:	// Discover Identity request received
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblGetIdentity\t\tDiscover Identity request received\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblGetIdentityNak:	// Respond with NAK
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblGetIdentityNak\t\tRespond with NAK\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblSendIdentity:	// Respond with Ack
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblSendIdentity\t\tRespond with Ack\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblGetSvids:	// Discover SVIDs request received
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblGetSvids\t\tDiscover SVIDs request received\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblGetSvidsNak:	// Respond with NAK
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblGetSvidsNak\t\tRespond with NAK\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblSendSvids:	// Respond with ACK
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblSendSvids\t\tRespond with ACK\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblGetModes:	// Discover Modes request received
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblGetModes\t\tDiscover Modes request received\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblGetModesNak:	// Respond with NAK
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblGetModesNak\t\tRespond with NAK\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblSendModes:	// Respond with ACK
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblSendModes\t\tRespond with ACK\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblEvaluateModeEntry:	// Enter Mode request received
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblEvaluateModeEntry\t\tEnter Mode request received\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblModeEntryAck:	// Respond with NAK
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblModeEntryAck\t\tRespond with NAK\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblModeEntryNak:	// Respond with ACK
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblModeEntryNak\t\tRespond with ACK\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblModeExit:	// Exit Mode request received
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblModeExit\t\tExit Mode request received\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblModeExitAck:	// Respond with NAK
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblModeExitAck\t\tRespond with NAK\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peCblModeExitNak:	// Respond with ACK
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeCblModeExitNak\t\tRespond with ACK\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peDpRequestStatus:	// Requesting PP Status
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeDpRequestStatus\t\tRequesting PP Status\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case PE_BIST_Receive_Mode:	// Bist Receive Mode
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tPE_BIST_Receive_Mode\t\tBist Receive Mode\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case PE_BIST_Frame_Received:	// Test Frame received by Protocol layer
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tPE_BIST_Frame_Received\t\tTest Frame received by Protocol layer\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case PE_BIST_Carrier_Mode_2:	// BIST Carrier Mode 2
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tPE_BIST_Carrier_Mode_2\t\tBIST Carrier Mode 2\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case peSourceWaitNewCapabilities:	// Wait for new Source Capabilities from Policy Manager
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tpeSourceWaitNewCapabilities\t\tWait for new Source Capabilities from Policy Manager\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case dbgSendTxPacket:	// Treat the same as Rx packet, just change "Rx" to "Tx"
		case dbgGetRxPacket:	// Debug point for measuring Rx packet handling time in ProtocolGetRxPacket()
			{
				// Special parsing for this state - TimeStampSeconds is really the number of I2C reads performed, and TimeStampMS10ths is the time elapsed (in ms)
				//   numChars += sprintf(tempBuf, "%02u|0.%04u\tdbgGetRxPacket\t\t\tNumber of I2C bytes read | Time elapsed\n", TimeStampSeconds, TimeStampMS10ths);

				// Recombine the 2 header bytes into a u16
				PDMessageHeader = output[i + 4];	// Get MSByte
				PDMessageHeader = PDMessageHeader << 8;	// Shift into MS position
				PDMessageHeader |= output[i + 2];	// Get LSByte

				// Parse out the message type to make the log easier to read
				if (fusb_get_pd_message_type
				    (PDMessageHeader, MessageType) > -1) {
					numChars +=
					    sprintf(tempBuf,
						    "0x%x\t\t%s\t\tMessage Type: %s\n",
						    PDMessageHeader,
						    (output[i] ==
						     dbgGetRxPacket) ?
						    "dbgGetRxPacket" :
						    "dbgSendTxPacket",
						    MessageType);
				} else {
					numChars +=
					    sprintf(tempBuf,
						    "0x%x\t\t%s\t\tMessage Type: UNKNOWN\n",
						    PDMessageHeader,
						    (output[i] ==
						     dbgGetRxPacket) ?
						    "dbgGetRxPacket" :
						    "dbgSendTxPacket");
				}
				strcat(buf, tempBuf);
				break;
			}

		default:
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tUKNOWN STATE: 0x%02x\n",
					    TimeStampSeconds, TimeStampMS10ths,
					    output[i]);
				strcat(buf, tempBuf);
				break;
			}
		}
	}
	strcat(buf, "\n");	// Append a newline for pretty++
	return ++numChars;	// Account for newline and return number of bytes to be shown
}

/* Fetch and display the Type-C state log */
static ssize_t _fusb_Sysfs_TypeCStateLog_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	FSC_S32 i = 0;
	FSC_S32 numChars = 0;
	FSC_S32 numLogs = 0;
	FSC_U32 TimeStampSeconds = 0;	// Timestamp value in seconds
	FSC_U32 TimeStampMS10ths = 0;	// Timestamp fraction in 10ths of milliseconds
	FSC_S8 output[FSC_HOSTCOMM_BUFFER_SIZE] = { 0 };
	FSC_S8 tempBuf[FUSB_MAX_BUF_SIZE] = { 0 };
	tempBuf[0] = CMD_READ_STATE_LOG;	// To request the Type-C statelog from Hostcomm

	/* Get the PD State Log */
	fusb_ProcessMsg(tempBuf, output);

	numLogs = output[3];
	/* First byte echos the command, 4th byte is number of logs (2nd and 3rd bytes reserved as 0) */
	numChars += sprintf(tempBuf, "Type-C State Log has %u entries:\n", numLogs);	// Copy string + null terminator
	strcat(buf, tempBuf);

	/* Relevant data starts at 5th byte in this format: CMD 0 0 #Logs State time time time time */
	for (i = 4; (i + 4 < FSC_HOSTCOMM_BUFFER_SIZE) && (numChars < PAGE_SIZE) && (numLogs > 0); i += 5, numLogs--)	// Must be able to peek 4 bytes ahead, and don't overflow the output buffer (PAGE_SIZE), only print logs we have
	{
		// Parse out the timestamp
		fusb_timestamp_bytes_to_time(&TimeStampSeconds,
					     &TimeStampMS10ths, &output[i + 1]);

		// sprintf should be safe here because we're controlling the strings being printed, just make sure the strings are less than FUSB_MAX_BUF_SIZE+1
		switch (output[i]) {
		case Disabled:
			{
				numChars +=
				    sprintf(tempBuf, "[%u.%04u]\tDisabled\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case ErrorRecovery:
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tErrorRecovery\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case Unattached:
			{
				numChars +=
				    sprintf(tempBuf, "[%u.%04u]\tUnattached\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case AttachWaitSink:
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tAttachWaitSink\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case AttachedSink:
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tAttachedSink\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case AttachWaitSource:
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tAttachWaitSource\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case AttachedSource:
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tAttachedSource\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case TrySource:
			{
				numChars +=
				    sprintf(tempBuf, "[%u.%04u]\tTrySource\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case TryWaitSink:
			{
				numChars +=
				    sprintf(tempBuf, "[%u.%04u]\tTryWaitSink\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case TrySink:
			{
				numChars +=
				    sprintf(tempBuf, "[%u.%04u]\tTrySink\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case TryWaitSource:
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tTryWaitSource\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case AudioAccessory:
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tAudioAccessory\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case DebugAccessorySource:
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tDebugAccessorySource\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case AttachWaitAccessory:
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tAttachWaitAccessory\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case PoweredAccessory:
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tPoweredAccessory\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case UnsupportedAccessory:
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tUnsupportedAccessory\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case DelayUnattached:
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tDelayUnattached\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}

		case UnattachedSource:
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tUnattachedSource\n",
					    TimeStampSeconds, TimeStampMS10ths);
				strcat(buf, tempBuf);
				break;
			}
		default:
			{
				numChars +=
				    sprintf(tempBuf,
					    "[%u.%04u]\tUKNOWN STATE: 0x%02x\n",
					    TimeStampSeconds, TimeStampMS10ths,
					    output[i]);
				strcat(buf, tempBuf);
				break;
			}
		}
	}
	strcat(buf, "\n");	// Append a newline for pretty++
	return ++numChars;	// Account for newline and return number of bytes to be shown
}
static ssize_t _fusb_Sysfs_bcd_device_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	return snprintf(buf, PAGE_SIZE - 2,
		"bcd device %x\n", gBcdDevice);
}

/* Reinitialize the FUSB302 */
static ssize_t _fusb_Sysfs_Reinitialize_fusb302(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (chip == NULL) {
		return sprintf(buf,
			       "FUSB302 Error: Internal chip structure pointer is NULL!\n");
	}

	/* Make sure that we are doing this in a thread-safe manner */
#ifdef FSC_INTERRUPT_TRIGGERED
	disable_irq(chip->gpio_IntN_irq);	// Waits for current IRQ handler to return, then disables it
#else
	fusb_StopThreads();	// Waits for current work to complete, then cancels scheduled work and flushed the work queue
#endif // FSC_INTERRUPT_TRIGGERED

	fusb_StopTimers();
	core_initialize();
	FUSB_LOG("FUSB  %s - Core is initialized!\n", __func__);
	fusb_StartTimers();
	core_enable_typec(TRUE);
	FUSB_LOG("FUSB  %s - Type-C State Machine is enabled!\n", __func__);

#ifdef FSC_INTERRUPT_TRIGGERED
	enable_irq(chip->gpio_IntN_irq);
#else
	// Schedule to kick off the main working thread
	schedule_work(&chip->worker);
#endif // FSC_INTERRUPT_TRIGGERED

	return sprintf(buf, "FUSB302 Reinitialized!\n");
}

// Define our device attributes to export them to sysfs
static DEVICE_ATTR(fusb30x_hostcomm, S_IRWXU | S_IRWXG | S_IROTH,
		   _fusb_Sysfs_Hostcomm_show, _fusb_Sysfs_Hostcomm_store);
static DEVICE_ATTR(pd_state_log, S_IRUSR | S_IRGRP | S_IROTH,
		   _fusb_Sysfs_PDStateLog_show, NULL);
static DEVICE_ATTR(typec_state_log, S_IRUSR | S_IRGRP | S_IROTH,
		   _fusb_Sysfs_TypeCStateLog_show, NULL);
static DEVICE_ATTR(reinitialize, S_IRUSR | S_IRGRP | S_IROTH,
		   _fusb_Sysfs_Reinitialize_fusb302, NULL);
static DEVICE_ATTR(bcd_device, S_IRUSR | S_IRGRP | S_IROTH,
		   _fusb_Sysfs_bcd_device_show, NULL);
static int fusb302_reg_dump(struct seq_file *m, void *data)
{
	int reg_addr[] = {
		regDeviceID, regSwitches0, regSwitches1,
		regMeasure, regSlice, regControl0, regControl1,
		regControl2, regControl3, regMask, regPower,
		regReset, regOCPreg, regMaska, regMaskb,
		regControl4, regStatus0a, regStatus1a,
		regInterrupta, regInterruptb,
		regStatus0, regStatus1, regInterrupt
	};
	static char * const reg_name[] = {
		  "Device ID", "Switches0", "Switches1", "Measure", "Slice",
		  "Control0", "Control1", "Control2", "Control3", "Mask1",
		  "Power", "Reset", "OCPReg", "Maska", "Maskb", "Control4",
		  "Status0a", "Status1a", "Interrupta", "Interruptb", "Status0",
		  "Status1", "Interrupt"
		};
	int i;
	u8 byte = 0;

	for (i = 0; i < sizeof(reg_addr) / sizeof(reg_addr[0]); i++) {
		DeviceRead(reg_addr[i], 1, &byte);
		seq_printf(m, "Register - %s: %02xH = %02x\n",
					reg_name[i], reg_addr[i], byte);
	}
	return 0;
}

static int fusbreg_debugfs_open(struct inode *inode, struct file *file)
{
	struct fusb30x_chip *chip = inode->i_private;

	return single_open(file, fusb302_reg_dump, chip);
}

static const struct file_operations fusbreg_debugfs_ops = {
	.owner          = THIS_MODULE,
	.open           = fusbreg_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};
static int get_reg(void *data, u64 *val)
{
	u8 temp = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (chip == NULL) {
		pr_err("%s - Chip structure is null!\n", __func__);
		return -ENODEV;
	}
	DeviceRead(chip->debug_address, 1, &temp);
	*val = temp;
	return 0;
}

static int set_reg(void *data, u64 val)
{
	struct fusb30x_chip *chip  = data;
	u8 temp;

	temp = (u8) val;
	DeviceWrite(chip->debug_address, 1, &temp);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fusbdata_reg_fops, get_reg, set_reg, "0x%02llx\n");

int fusb302_debug_init(void)
{
	struct dentry *ent;
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (chip == NULL) {
		pr_err("%s - Chip structure is null!\n", __func__);
		return -ENODEV;
	}
	chip->debug_root = debugfs_create_dir("fusb302", NULL);

	if (!chip->debug_root) {
		dev_err(&chip->client->dev, "Couldn't create debug dir\n");
		return -EINVAL;
	}

	ent = debugfs_create_file("register_dump",
			S_IFREG | S_IRUGO,
			chip->debug_root, chip,
			&fusbreg_debugfs_ops);


	if (!ent) {
		dev_err(&chip->client->dev, "Couldn't create reg dump\n");
		return -EINVAL;
	}

	ent = debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
				chip->debug_root, &(chip->debug_address));
	if (!ent) {
		dev_err(&chip->client->dev, "Error creating address entry\n");
		return -EINVAL;
	}

	ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
				chip->debug_root, chip, &fusbdata_reg_fops);
	if (!ent) {
		dev_err(&chip->client->dev, "Error creating data entry\n");
		return -EINVAL;
	}

	return 0;
}

int fusb302_debug_remove(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (chip == NULL) {
		pr_err("%s - Chip structure is null!\n", __func__);
		return -ENODEV;
	}
	debugfs_remove_recursive(chip->debug_root);
	return 0;
}
static ssize_t fusb302_state(struct device *dev, struct device_attribute *attr,
							 char *buf)
{
	char data[5];

	GetDeviceTypeCStatus(data);
	return snprintf(buf, PAGE_SIZE - 2,
			"SMC=%2x, connState=%2d, cc=%2x, current=%d\n",
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
	DeviceWrite(regSwitches0, 2, &Registers.Switches.byte[0]);
	return count;
}

static DEVICE_ATTR(enable_vconn, S_IWUSR | S_IWGRP, NULL, fusb302_enable_vconn);

static struct attribute *fusb302_sysfs_attrs[] = {
	&dev_attr_fusb30x_hostcomm.attr,
	&dev_attr_pd_state_log.attr,
	&dev_attr_typec_state_log.attr,
	&dev_attr_reinitialize.attr,
	&dev_attr_bcd_device.attr,
	&dev_attr_CC_state.attr,
	&dev_attr_enable_vconn.attr,
	&dev_attr_state.attr,
	NULL
};

static struct attribute_group fusb302_sysfs_attr_grp = {
	.attrs = fusb302_sysfs_attrs,
};

static int set_disable_ss_switch(const char *val, const struct kernel_param *kp)
{
	int rv;
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (chip == NULL) {
		pr_err("%s - Chip structure is null!\n", __func__);
		return -ENODEV;
	}
	rv = param_set_bool(val, kp);
	if (rv)
		return rv;

	if (disable_ss_switch && chip)
		fusb_disableSuperspeedUSB();

	return 0;
}

static struct kernel_param_ops disable_ss_param_ops = {
	.set = set_disable_ss_switch,
	.get = param_get_bool,
};

module_param_cb(disable_ss_switch, &disable_ss_param_ops,
				&disable_ss_switch, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_ss_switch, "Disable Super Speed Switch");

module_param(debug_audio, bool, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(debug_audio, "SSUSB enabled in audio accessory mode");
void fusb_Sysfs_Init(void)
{
	FSC_S32 ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (chip == NULL) {
		pr_err("%s - Chip structure is null!\n", __func__);
		return;
	}

	/* create attribute group for accessing the FUSB302 */
	ret =
	    sysfs_create_group(&chip->client->dev.kobj,
			       &fusb302_sysfs_attr_grp);
	if (ret) {
		pr_err("FUSB %s - Error creating sysfs attributes!\n",
		       __func__);
	}
}

#endif // FSC_DEBUG

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************        Driver Helpers         ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
void fusb_InitializeCore(void)
{
	core_initialize();
	FUSB_LOG("FUSB  %s - Core is initialized!\n", __func__);
	core_enable_typec(TRUE);
	FUSB_LOG("FUSB  %s - Type-C State Machine is enabled!\n", __func__);
}

FSC_BOOL fusb_IsDeviceValid(void)
{
	FSC_U8 val = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return FALSE;
	}
	// Test to see if we can do a successful I2C read
	if (!fusb_I2C_ReadData((FSC_U8) 0x01, &val)) {
		pr_err
		    ("FUSB  %s - Error: Could not communicate with device over I2C!\n",
		     __func__);
		return FALSE;
	}

	return TRUE;
}

void fusb_InitChipData(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (chip == NULL) {
		pr_err("%s - Chip structure is null!\n", __func__);
		return;
	}
#ifdef FSC_DEBUG
	chip->dbgTimerTicks = 0;
	chip->dbgTimerRollovers = 0;
	chip->dbgSMTicks = 0;
	chip->dbgSMRollovers = 0;
	chip->dbg_gpio_StateMachine = -1;
	chip->dbg_gpio_StateMachine_value = false;
#endif // FSC_DEBUG

	/* GPIO Defaults */
	chip->gpio_VBus5V = -1;
	chip->gpio_VBus5V_value = false;
	chip->gpio_VBusOther = -1;
	chip->gpio_VBusOther_value = false;
	chip->gpio_IntN = -1;

#ifdef FSC_INTERRUPT_TRIGGERED
	chip->gpio_IntN_irq = -1;
#endif // FSC_INTERRUPT_TRIGGERED

	/* I2C Configuration */
	chip->InitDelayMS = INIT_DELAY_MS;	// Time to wait before device init
	chip->numRetriesI2C = RETRIES_I2C;	// Number of times to retry I2C reads and writes
	chip->use_i2c_blocks = false;	// Assume failure
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/******************************************      IRQ/Threading Helpers       *****************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
#ifdef FSC_INTERRUPT_TRIGGERED
static int fusb_InterruptPinLow(void)
{
	FSC_S32 ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return -ENOMEM;
	}
	ret = !gpio_get_value(chip->gpio_IntN);
	return ret;
}

FSC_S32 fusb_EnableInterrupts(void)
{
	FSC_S32 ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return -ENOMEM;
	}

	/* Set up IRQ for INT_N GPIO */
	ret = gpio_to_irq(chip->gpio_IntN);	// Returns negative errno on error
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"%s - Error: Unable to request IRQ for INT_N GPIO! Error code: %d\n",
			__func__, ret);
		chip->gpio_IntN_irq = -1;	// Set to indicate error
		fusb_GPIO_Cleanup();
		return ret;
	}
	chip->gpio_IntN_irq = ret;
	FUSB_LOG("%s - Success: Requested INT_N IRQ: '%d'\n", __func__,
		 chip->gpio_IntN_irq);

	/* Request threaded IRQ because we will likely sleep while handling the interrupt, trigger is active-low, don't handle concurrent interrupts */
	ret = devm_request_irq(&chip->client->dev, chip->gpio_IntN_irq,
			_fusb_isr_intn, IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
			FUSB_DT_INTERRUPT_INTN, chip);
	if (ret) {
		dev_err(&chip->client->dev,
			"%s - Error: Unable to request threaded IRQ for INT_N GPIO! Error code: %d\n",
			__func__, ret);
		fusb_GPIO_Cleanup();
		return ret;
	}
	device_init_wakeup(&chip->client->dev, true);

	return 0;
}

/*******************************************************************************
* Function:        _fusb_isr_intn
* Input:           irq - IRQ that was triggered
*                  dev_id - Ptr to driver data structure
* Return:          irqreturn_t - IRQ_HANDLED on success, IRQ_NONE on failure
* Description:     Activates the core
********************************************************************************/
static irqreturn_t _fusb_isr_intn(FSC_S32 irq, void *dev_id)
{
	struct fusb30x_chip *chip = dev_id;
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return IRQ_NONE;
	}
	disable_irq_nosync(chip->gpio_IntN_irq);
	atomic_set(&chip->irq_disabled, 1);
	platform_run_wake_thread();
	return IRQ_HANDLED;
}

void _fusb_WakeWorker(struct work_struct *work)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}
	pm_stay_awake(&chip->client->dev);
	if (fusb_InterruptPinLow()) {
		core_state_machine();
	} else
		core_state_machine_imp();
	pm_relax(&chip->client->dev);
	if (atomic_read(&chip->irq_disabled) > 0) {
		atomic_set(&chip->irq_disabled, 0);
		enable_irq(chip->gpio_IntN_irq);
	}
}
void fusb_ScheduleWakeWork(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}
	if (chip->wake_worker_wq)
		queue_work(chip->wake_worker_wq, &chip->wake_worker);
	else
		schedule_work(&chip->wake_worker);
}
void fusb_InitializeWakeWorker(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();

	FUSB_LOG("FUSB  %s - Initializing Wakeup threads!\n", __func__);
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}
	chip->wake_worker_wq = alloc_workqueue(
				"fusb302_worker", WQ_HIGHPRI, 1);
	if (!chip->wake_worker_wq)
		pr_err("FUSB %s -Error: Failed to allocate workqueue\n",
				__func__);
	INIT_WORK(&chip->wake_worker, _fusb_WakeWorker);
}
void platform_run_wake_thread(void)
{
	fusb_ScheduleWakeWork();
}
#else

/*******************************************************************************
* Function:        _fusb_InitWorker
* Input:           delayed_work - passed in from OS
* Return:          none
* Description:     Callback for the init worker, kicks off the main worker
********************************************************************************/
void _fusb_InitWorker(struct work_struct *delayed_work)
{
	struct fusb30x_chip *chip =
	    container_of(delayed_work, struct fusb30x_chip, init_worker.work);
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}
	// Schedule to kick off the main working thread
	schedule_work(&chip->worker);
}

/*******************************************************************************
* Function:        _fusb_MainWorker
* Input:           delayed_work - passed in from OS
* Return:          none
* Description:     Activates the core
********************************************************************************/
void _fusb_MainWorker(struct work_struct *work)
{
	struct fusb30x_chip *chip =
	    container_of(work, struct fusb30x_chip, worker);
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}
#ifdef FSC_DEBUG
	dbg_fusb_GPIO_Set_SM_Toggle(!chip->dbg_gpio_StateMachine_value);	// Optionally toggle debug GPIO when SM is called to measure thread tick rate

	if (chip->dbgSMTicks++ >= U8_MAX)	// Tick our state machine tick counter
	{
		chip->dbgSMRollovers++;	// Record a moderate amount of rollovers
	}
#endif // FSC_DEBUG

	core_state_machine();	// Run the state machine
	schedule_work(&chip->worker);	// Reschedule ourselves to run again
}
void fusb_InitializeWorkers(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	FUSB_LOG("FUSB  %s - Initializing threads!\n", __func__);
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}
	// Initialize our delayed_work and work structs
	INIT_DELAYED_WORK(&chip->init_worker, _fusb_InitWorker);
	INIT_WORK(&chip->worker, _fusb_MainWorker);
}

void fusb_StopThreads(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}
	// Cancel the initial delayed work
	cancel_delayed_work_sync(&chip->init_worker);
	flush_delayed_work(&chip->init_worker);

	// Cancel the main worker
	flush_work(&chip->worker);
	cancel_work_sync(&chip->worker);
}

void fusb_ScheduleWork(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}
	schedule_delayed_work(&chip->init_worker,
			      msecs_to_jiffies(chip->InitDelayMS));
}
#endif // FSC_INTERRUPT_TRIGGERED, else
