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

#include "fusb30x_global.h"	// Chip structure access
#include "../core/core.h"	// Core access
#include "hostcomm.h"
#include "platform_helpers.h"

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************        GPIO Interface         ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
const char *FUSB_DT_INTERRUPT_INTN = "fsc_interrupt_int_n";	// Name of the INT_N interrupt in the Device Tree
#define FUSB_DT_GPIO_INTN               "fairchild,int_n"	// Name of the Int_N GPIO pin in the Device Tree
#define FUSB_DT_GPIO_VBUS_5V            "fairchild,vbus5v"	// Name of the VBus 5V GPIO pin in the Device Tree
#define FUSB_DT_GPIO_VBUS_OTHER         "fairchild,vbusOther"	// Name of the VBus Other GPIO pin in the Device Tree

#ifdef DEBUG
#define FUSB_DT_GPIO_DEBUG_SM_TOGGLE    "fairchild,dbg_sm"	// Name of the debug State Machine toggle GPIO pin in the Device Tree
#endif // DEBUG

#ifdef FSC_INTERRUPT_TRIGGERED
/* Internal forward declarations */
static irqreturn_t _fusb_isr_intn(int irq, void *dev_id);
#endif // FSC_INTERRUPT_TRIGGERED

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
	pr_debug("%sabling, Audio Switch\n", enable ? "En" : "Dis");
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
	int ss_output_en_gpio, ss_sw_sel_gpio;
	int sw_val  = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (!chip) {
		pr_alert("FUSB  %s - Error: Chip structure is NULL!\n",
				 __func__);
		return -ENOMEM;
	}
	ss_output_en_gpio = chip->gpios[FUSB_SS_OE_EN_INDEX];
	ss_sw_sel_gpio = chip->gpios[FUSB_SS_SW_SEL_INDEX];
	if (disable_ss_switch)
		return 0;
	if (!(gpio_is_valid(ss_output_en_gpio) &&
		gpio_is_valid(ss_sw_sel_gpio)))
		return -ENODEV;
	/* Default switch position to 0 for all other values of CC */
	if (CC2 && !CC1)
		sw_val = 1;
	pr_debug("Setting SS_SW_SEL to %d\n", sw_val);
	gpio_set_value(ss_sw_sel_gpio, sw_val);
	pr_debug("Setting SS_OE_EN to enabled\n");
	gpio_set_value(ss_output_en_gpio, 0);
	return 0;
}
static int fusb_disableSuperspeedUSB(void)
{
	int ss_output_en_gpio;
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (!chip) {
		pr_alert("FUSB  %s - Error: Chip structure is NULL!\n",
			   __func__);
		return -ENOMEM;
	}
	ss_output_en_gpio = chip->gpios[FUSB_SS_OE_EN_INDEX];
	if (disable_ss_switch)
		return 0;
	if (gpio_is_valid(ss_output_en_gpio)) {
		pr_debug("Setting SS OE EN Switch to disabled\n");
		gpio_set_value(ss_output_en_gpio, 1);
		return 0;
	}
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
void platform_toggleAudioSwitch(bool enable)
{
	fusb_toggleAudioSwitch(enable);
}
int fusb_InitializeGPIO(void)
{
	int i, gpio_cnt, label_cnt;
	const char *label_prop = "fusb,gpio-labels";
	struct device_node *node;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return -ENOMEM;
	}
	/* Get our device tree node */
	node = chip->client->dev.of_node;
	gpio_cnt = of_gpio_count(node);
	label_cnt = of_property_count_strings(node, label_prop);
	if (gpio_cnt > ARRAY_SIZE(chip->gpios)) {
		dev_err(&chip->client->dev, "%s:%d gpio count is greater than %zu.\n",
			__func__, __LINE__, ARRAY_SIZE(chip->gpios));
		return -EINVAL;
	}
	if (label_cnt != gpio_cnt) {
		dev_err(&chip->client->dev, "%s:%d label count does not match gpio count.\n",
		__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < gpio_cnt; i++) {
		enum of_gpio_flags flags = 0;
		int gpio;
		const char *label = NULL;

		gpio = of_get_gpio_flags(node, i, &flags);
		if (gpio < 0) {
			dev_err(&chip->client->dev, "%s:%d of_get_gpio failed: %d\n",
				__func__, __LINE__, gpio);
			return gpio;
		}
		if (i < label_cnt)
			of_property_read_string_index(node, label_prop,
			i, &label);
		gpio_request_one(gpio, flags, label);
		gpio_export(gpio, true);
		gpio_export_link(&chip->client->dev, label, gpio);
		dev_dbg(&chip->client->dev, "%s: gpio=%d, flags=0x%x, label=%s\n",
		__func__, gpio, flags, label);
		chip->gpios[i] = gpio;
	}
	chip->gpio_IntN  = chip->gpios[FUSB_INT_INDEX];
	pr_debug("irq_gpio number is %d\n",
		chip->gpios[FUSB_INT_INDEX]);
#ifdef FPGA_BOARD
	/* Get our GPIO pins from the device tree, and then set their direction (input/output) */
	chip->gpio_IntN = of_get_named_gpio(node, FUSB_DT_GPIO_INTN, 0);
	if (!gpio_is_valid(chip->gpio_IntN)) {
		dev_err(&chip->client->dev,
			"%s - Error: Could not get GPIO for Int_N! Error code: %d\n",
			__func__, chip->gpio_IntN);
		return chip->gpio_IntN;
	}
	ret = gpio_direction_input(chip->gpio_IntN);
	if (ret != 0) {
		dev_err(&chip->client->dev,
			"%s - Error: Could not set GPIO direction to input for Int_N! Error code: %d\n",
			__func__, ret);
		return ret;
	}

	/* Export to sysfs */
	gpio_export(chip->gpio_IntN, false);
	gpio_export_link(&chip->client->dev, FUSB_DT_GPIO_INTN,
			 chip->gpio_IntN);
	// VBus 5V
	chip->gpio_VBus5V = of_get_named_gpio(node, FUSB_DT_GPIO_VBUS_5V, 0);
	if (!gpio_is_valid(chip->gpio_VBus5V)) {
		dev_err(&chip->client->dev,
			"%s - Error: Could not get GPIO for VBus5V! Error code: %d\n",
			__func__, chip->gpio_VBus5V);
		fusb_GPIO_Cleanup();
		return chip->gpio_VBus5V;
	}
	ret = gpio_direction_output(chip->gpio_VBus5V, chip->gpio_VBus5V_value);
	if (ret != 0) {
		dev_err(&chip->client->dev,
			"%s - Error: Could not set GPIO direction to output for VBus5V! Error code: %d\n",
			__func__, ret);
		fusb_GPIO_Cleanup();
		return ret;
	}
	// Export to sysfs
	gpio_export(chip->gpio_VBus5V, false);
	gpio_export_link(&chip->client->dev, FUSB_DT_GPIO_VBUS_5V,
			 chip->gpio_VBus5V);

	printk(KERN_DEBUG
	       "FUSB  %s - VBus 5V initialized as pin '%d' and is set to '%d'\n",
	       __func__, chip->gpio_VBus5V, chip->gpio_VBus5V_value ? 1 : 0);

	// VBus other (eg. 12V)
	// NOTE - This VBus is optional, so if it doesn't exist then fake it like it's on.
	chip->gpio_VBusOther =
	    of_get_named_gpio(node, FUSB_DT_GPIO_VBUS_OTHER, 0);
	if (!gpio_is_valid(chip->gpio_VBusOther)) {
		// Soft fail - provide a warning, but don't quit because we don't really need this VBus if only using VBus5v
		printk(KERN_WARNING
		       "%s - Error: Could not get GPIO for VBusOther! Error code: %d\n",
		       __func__, chip->gpio_VBusOther);
	} else {
		ret =
		    gpio_direction_output(chip->gpio_VBusOther,
					  chip->gpio_VBusOther_value);
		if (ret != 0) {
			dev_err(&chip->client->dev,
				"%s - Error: Could not set GPIO direction to output for VBus5V! Error code: %d\n",
				__func__, ret);
			return ret;
		}
	}
#endif
#ifdef DEBUG
	// State Machine Debug Notification
	// Optional GPIO - toggles each time the state machine is called
	chip->dbg_gpio_StateMachine =
	    of_get_named_gpio(node, FUSB_DT_GPIO_DEBUG_SM_TOGGLE, 0);
	if (!gpio_is_valid(chip->dbg_gpio_StateMachine)) {
		// Soft fail - provide a warning, but don't quit because we don't really need this VBus if only using VBus5v
		printk(KERN_WARNING
		       "%s - Error: Could not get GPIO for SM Debug Toggle! Error code: %d\n",
		       __func__, chip->dbg_gpio_StateMachine);
	} else {
		ret =
		    gpio_direction_output(chip->dbg_gpio_StateMachine,
					  chip->dbg_gpio_StateMachine_value);
		if (ret != 0) {
			dev_err(&chip->client->dev,
				"%s - Error: Could not set GPIO direction to output for SM Debug Toggle! Error code: %d\n",
				__func__, ret);
			return ret;
		}
		// Export to sysfs
		gpio_export(chip->dbg_gpio_StateMachine, false);
		gpio_export_link(&chip->client->dev,
				 FUSB_DT_GPIO_DEBUG_SM_TOGGLE,
				 chip->dbg_gpio_StateMachine);
	}
#endif // DEBUG

	return 0;		// Success!
}

void fusb_GPIO_Set_VBus5v(bool set)
{
#ifdef FPGA_BOARD
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
	}
	// GPIO must be valid by this point
	gpio_set_value(chip->gpio_VBus5V, set ? 1 : 0);
	chip->gpio_VBus5V_value = set;

	printk(KERN_DEBUG "FUSB  %s - VBus 5V set to: %d\n", __func__,
	       chip->gpio_VBus5V_value ? 1 : 0);
#endif
}

void fusb_GPIO_Set_VBusOther(bool set)
{
#ifdef FPGA_BOARD
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
	}
	// Only try to set if feature is enabled, otherwise just fake it
	if (gpio_is_valid(chip->gpio_VBusOther)) {
		gpio_set_value(chip->gpio_VBusOther, set ? 1 : 0);
	}
	chip->gpio_VBusOther_value = set;
#endif
}

bool fusb_GPIO_Get_VBus5v(void)
{
#ifdef FPGA_BOARD
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return false;
	}

	if (!gpio_is_valid(chip->gpio_VBus5V)) {
		printk(KERN_DEBUG
		       "FUSB  %s - Error: VBus 5V pin invalid! Pin value: %d\n",
		       __func__, chip->gpio_VBus5V);
	}

	return chip->gpio_VBus5V_value;
#endif
	return true;
}

bool fusb_GPIO_Get_VBusOther(void)
{
#ifdef FPGA_BOARD
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return false;
	}

	if (!gpio_is_valid(chip->gpio_VBusOther)) {
		printk(KERN_DEBUG
		       "FUSB  %s - Error: VBus Other pin invalid! Pin value: %d\n",
		       __func__, chip->gpio_VBusOther);
	}

	return chip->gpio_VBusOther_value;
#endif
	return true;
}

bool fusb_GPIO_Get_IntN(void)
{
	int ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return false;
	} else {
		ret = !gpio_get_value(chip->gpio_IntN);	// Int_N is active low
		if (ret < 0) {
			dev_err(&chip->client->dev,
				"%s - Error: Could not get GPIO value for gpio_IntN! Error code: %d\n",
				__func__, ret);
			return false;
		}
		return (ret > 0);
	}
}

#ifdef DEBUG
void dbg_fusb_GPIO_Set_SM_Toggle(bool set)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
	}

	if (gpio_is_valid(chip->dbg_gpio_StateMachine)) {
		gpio_set_value(chip->dbg_gpio_StateMachine, set ? 1 : 0);
		chip->dbg_gpio_StateMachine_value = set;
		printk(KERN_DEBUG
		       "FUSB  %s - State machine toggle GPIO set to: %d\n",
		       __func__, chip->dbg_gpio_StateMachine_value ? 1 : 0);
	} else {
		printk(KERN_DEBUG
		       "FUSB  %s - State machine toggle GPIO unavailable!\n",
		       __func__);
	}
}

bool dbg_fusb_GPIO_Get_SM_Toggle(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return false;
	}
	if (!gpio_is_valid(chip->dbg_gpio_StateMachine)) {
		printk(KERN_DEBUG
		       "FUSB  %s - Error: State machine toggle debug pin invalid! Pin number: %d\n",
		       __func__, chip->dbg_gpio_StateMachine);
	}
	return chip->dbg_gpio_StateMachine_value;
}
#endif // DEBUG

void fusb_GPIO_Cleanup(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return;
	}
#ifdef FSC_INTERRUPT_TRIGGERED
	if (gpio_is_valid(chip->gpio_IntN) && chip->gpio_IntN_irq != -1)	// -1 indicates that we don't have an IRQ to free
	{
		devm_free_irq(&chip->client->dev, chip->gpio_IntN_irq, chip);
	}
#endif

	if (gpio_is_valid(chip->gpio_IntN) >= 0) {
		gpio_unexport(chip->gpio_IntN);
		gpio_free(chip->gpio_IntN);
	}

	if (gpio_is_valid(chip->gpio_VBus5V) >= 0) {
		gpio_unexport(chip->gpio_VBus5V);
		gpio_free(chip->gpio_VBus5V);
	}

	if (gpio_is_valid(chip->gpio_VBusOther) >= 0) {
		gpio_free(chip->gpio_VBusOther);
	}
#ifdef DEBUG
	if (gpio_is_valid(chip->dbg_gpio_StateMachine) >= 0) {
		gpio_unexport(chip->dbg_gpio_StateMachine);
		gpio_free(chip->dbg_gpio_StateMachine);
	}
#endif // DEBUG
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************         I2C Interface         ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
bool fusb_I2C_WriteData(unsigned char address, unsigned char length,
			unsigned char *data)
{
	int i = 0;
	int ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (chip == NULL || chip->client == NULL || data == NULL)	// Sanity check
	{
		printk(KERN_ERR "%s - Error: %s is NULL!\n", __func__,
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
			if (ret == ERANGE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error writing byte data. Address: '0x%02x', Return: -ERANGE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == EINVAL) {
				dev_err(&chip->client->dev,
					"%s - I2C Error writing byte data. Address: '0x%02x', Return: -EINVAL.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else {
				dev_err(&chip->client->dev,
					"%s - Unexpected I2C error writing byte data. Address: '0x%02x', Return: '%04x'.  Attempt #%d / %d...\n",
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

bool fusb_I2C_ReadData(unsigned char address, unsigned char *data)
{
	int i = 0;
	int ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (chip == NULL || chip->client == NULL || data == NULL) {
		printk(KERN_ERR "%s - Error: %s is NULL!\n", __func__,
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
			if (ret == ERANGE) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -ERANGE.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else if (ret == EINVAL) {
				dev_err(&chip->client->dev,
					"%s - I2C Error reading byte data. Address: '0x%02x', Return: -EINVAL.  Attempt #%d / %d...\n",
					__func__, address, i,
					chip->numRetriesI2C);
			} else {
				dev_err(&chip->client->dev,
					"%s - Unexpected I2C error reading byte data. Address: '0x%02x', Return: '%04x'.  Attempt #%d / %d...\n",
					__func__, address, ret, i,
					chip->numRetriesI2C);
			}
		} else		// Successful i2c writes should always return 0
		{
			*data = (unsigned char)ret;
			break;
		}
	}
	mutex_unlock(&chip->lock);
	return (ret >= 0);
}

bool fusb_I2C_ReadBlockData(unsigned char address, unsigned char length,
			    unsigned char *data)
{
	int i = 0;
	int ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (chip == NULL || chip->client == NULL || data == NULL) {
		printk(KERN_ERR "%s - Error: %s is NULL!\n", __func__,
		       (chip ==
			NULL ? "Internal chip structure" : (chip->client ==
							    NULL ? "I2C Client"
							    :
							    "block read data buffer")));
		return false;
	}

	mutex_lock(&chip->lock);

	ret = i2c_smbus_read_i2c_block_data(chip->client, (u8) address, (u8) length, (u8 *) data);	// Read a byte of data from address
	if (ret < 0)		// Errors report as negative
	{
		if (ret == ERANGE) {
			dev_err(&chip->client->dev,
				"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -ERANGE.  Attempt #%d / %d...\n",
				__func__, address, i, chip->numRetriesI2C);
		} else if (ret == EINVAL) {
			dev_err(&chip->client->dev,
				"%s - I2C Error block reading byte data. Address: '0x%02x', Return: -EINVAL.  Attempt #%d / %d...\n",
				__func__, address, i, chip->numRetriesI2C);
		} else {
			dev_err(&chip->client->dev,
				"%s - Unexpected I2C error block reading byte data. Address: '0x%02x', Return: '%04x'.  Attempt #%d / %d...\n",
				__func__, address, ret, i, chip->numRetriesI2C);
		}
	} else if (ret != length)	// We didn't read everything we wanted
	{
		printk(KERN_ERR
		       "%s - Error: Block read request of %u bytes truncated to %u bytes.\n",
		       __func__, length, I2C_SMBUS_BLOCK_MAX);
	}

	mutex_unlock(&chip->lock);
	return (ret == length);
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************        Timer Interface        ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
static const unsigned long g_fusb_timer_tick_period_ns = 100000;	// Tick SM every 100us -> 100000ns  // TODO - optimize this number

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
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return HRTIMER_NORESTART;
	}

	if (!timer) {
		printk(KERN_ALERT
		       "FUSB  %s - Error: High-resolution timer is NULL!\n",
		       __func__);
		return HRTIMER_NORESTART;
	}

	core_tick_at_100us();

#ifdef DEBUG
	if (chip->dbgTimerTicks++ >= U8_MAX) {
		chip->dbgTimerRollovers++;
	}
#endif // DEBUG

	// Reset the timer expiration
	hrtimer_forward(timer, ktime_get(),
			ktime_set(0, g_fusb_timer_tick_period_ns));

	return HRTIMER_RESTART;	// Requeue the timer
}

void fusb_InitializeTimer(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return;
	}

	hrtimer_init(&chip->timer_state_machine, CLOCK_MONOTONIC, HRTIMER_MODE_REL);	// Init the timer structure
	chip->timer_state_machine.function = _fusb_TimerHandler;	// Assign the callback to call when time runs out

	printk(KERN_DEBUG "FUSB  %s - Timer initialized!\n", __func__);
}

void fusb_StartTimers(void)
{
	ktime_t ktime;
	struct fusb30x_chip *chip;

	chip = fusb30x_GetChip();
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return;
	}
	mutex_lock(&chip->lock);
	ktime = ktime_set(0, g_fusb_timer_tick_period_ns);	// Convert our timer period (in ns) to ktime
	hrtimer_start(&chip->timer_state_machine, ktime, HRTIMER_MODE_REL);	// Start the timer
	mutex_unlock(&chip->lock);
}

void fusb_StopTimers(void)
{
	int ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return;
	}
	mutex_lock(&chip->lock);
	if (hrtimer_active(&chip->timer_state_machine) != 0) {
		ret = hrtimer_cancel(&chip->timer_state_machine);
		printk(KERN_DEBUG
		       "%s - Active state machine hrtimer canceled: %d\n",
		       __func__, ret);
	}
	if (hrtimer_is_queued(&chip->timer_state_machine) != 0) {
		ret = hrtimer_cancel(&chip->timer_state_machine);
		printk(KERN_DEBUG
		       "%s - Queued state machine hrtimer canceled: %d\n",
		       __func__, ret);
	}
	mutex_unlock(&chip->lock);
	printk(KERN_DEBUG "FUSB  %s - Timer stopped!\n", __func__);
}

// Get the max value that we can delay in 10us increments at compile time
static const unsigned int MAX_DELAY_10US = (UINT_MAX / 10);
void fusb_Delay10us(u32 delay10us)
{
	unsigned int us = 0;
	if (delay10us > MAX_DELAY_10US) {
		printk(KERN_ALERT
		       "%s - Error: Delay of '%u' is too long! Must be less than '%u'.\n",
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
void fusb_timestamp_bytes_to_time(unsigned int *outSec,
				  unsigned int *outMS10ths, char *inBuf)
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
	int i = 0;
	int numLogs = 0;
	int numChars = 0;
	unsigned int TimeStampSeconds = 0;	// Timestamp value in seconds
	unsigned int TimeStampMS10ths = 0;	// Timestamp fraction in 10ths of milliseconds
	char tempBuf[FUSB_MAX_BUF_SIZE] = { 0 };
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (chip == NULL) {
		printk(KERN_ERR "%s - Chip structure is null!\n", __func__);
	} else if (buf == NULL || chip->HostCommBuf == NULL) {
		printk(KERN_ERR "%s - Buffer is null!\n", __func__);
	} else if (chip->HostCommBuf[0] == CMD_READ_PD_STATE_LOG)	// Parse out the PD state log
	{
		numLogs = chip->HostCommBuf[3];
		/* First byte echos the command, 4th byte is number of logs (2nd and 3rd bytes reserved as 0) */
		numChars += sprintf(tempBuf, "PD State Log has %u entries:\n", numLogs);	// Copy string + null terminator
		strcat(buf, tempBuf);

		/* Relevant data starts at 5th byte in this format: CMD 0 0 #Logs PDState time time time time */
		for (i = 4; (i + 4 < FSC_HOSTCOMM_BUFFER_SIZE) && (numChars < PAGE_SIZE) && (numLogs > 0); i += 5, numLogs--)	// Must be able to peek 4 bytes ahead, and don't overflow the output buffer (PAGE_SIZE)
		{
			// Parse out the timestamp
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

			case dbgGetRxPacket:	// Debug point for measuring Rx packet handling time in ProtocolGetRxPacket()
				{
					// Special parsing for this state - TimeStampSeconds is really the number of I2C reads performed, and TimeStampMS10ths is the time elapsed (in ms)
					numChars +=
					    sprintf(tempBuf,
						    "%02u|0.%04u\tdbgGetRxPacket\t\t\tNumber of I2C bytes read | Time elapsed\n",
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

			case DebugAccessory:
				{
					numChars +=
					    sprintf(tempBuf,
						    "[%u.%04u]\tDebugAccessory\n",
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
	int ret = 0;
	int i = 0;
	int j = 0;
	char tempByte = 0;
	int numBytes = 0;
	char temp[6] = { 0 };	// Temp buffer to parse out individual hex numbers, +1 for null terminator
	char temp_input[FSC_HOSTCOMM_BUFFER_SIZE] = { 0 };
	char output[FSC_HOSTCOMM_BUFFER_SIZE] = { 0 };
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (chip == NULL) {
		printk(KERN_ERR "%s - Chip structure is null!\n", __func__);
	} else if (input == NULL) {
		printk(KERN_ERR "%s - Error: Input buffer is NULL!\n",
		       __func__);
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
				printk(KERN_ERR
				       "FUSB  %s - Error: Hostcomm input is not a valid hex value! Return: '%d'\n",
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

// Define our device attributes to export them to sysfs
static DEVICE_ATTR(fusb30x_hostcomm, S_IRWXU | S_IRWXG | S_IROTH,
		   _fusb_Sysfs_Hostcomm_show, _fusb_Sysfs_Hostcomm_store);

void fusb_Sysfs_Init(void)
{
	int ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (chip == NULL) {
		printk(KERN_ERR "%s - Chip structure is null!\n", __func__);
		return;
	}
	ret =
	    device_create_file(&chip->client->dev, &dev_attr_fusb30x_hostcomm);
	if (ret != 0) {
		dev_err(&chip->client->dev,
			"FUSB  %s - Error: Unable to initialize sysfs device file 'fusb30x_io'! ret = %d\n",
			__func__, ret);
		return;
	}
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************        Driver Helpers         ******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
void fusb_InitializeCore(void)
{
	core_initialize();
	printk(KERN_DEBUG "FUSB  %s - Core is initialized!\n", __func__);
	fusb_StartTimers();
	printk(KERN_DEBUG "FUSB  %s - Timers are started!\n", __func__);
	core_enable_typec(TRUE);
	printk(KERN_DEBUG "FUSB  %s - Type-C State Machine is enabled!\n",
	       __func__);
}

bool fusb_IsDeviceValid(void)
{
	unsigned char val = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return FALSE;
	}
	// Test to see if we can do a successful I2C read
	if (!fusb_I2C_ReadData((unsigned char)0x01, &val)) {
		printk(KERN_ALERT
		       "FUSB  %s - Error: Could not communicate with device over I2C!\n",
		       __func__);
		return FALSE;
	}

	return TRUE;
}

void fusb_InitChipData(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (chip == NULL) {
		printk(KERN_ALERT "%s - Chip structure is null!\n", __func__);
		return;
	}
#ifdef DEBUG
	chip->dbgTimerTicks = 0;
	chip->dbgTimerRollovers = 0;
	chip->dbgSMTicks = 0;
	chip->dbgSMRollovers = 0;
	chip->dbg_gpio_StateMachine = false;
#endif // DEBUG

	/* GPIO Defaults */
	chip->gpio_VBus5V = -1;
	chip->gpio_VBus5V_value = false;
	chip->gpio_VBusOther = -1;
	chip->gpio_VBusOther_value = false;
	chip->gpio_IntN = -1;

#ifdef FSC_INTERRUPT_TRIGGERED
	chip->gpio_IntN_irq = -1;
#endif

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

int fusb_EnableInterrupts(void)
{
	int ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
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
	printk(KERN_DEBUG "%s - Success: Requested INT_N IRQ: '%d'\n", __func__,
	       chip->gpio_IntN_irq);

	/* Request threaded IRQ because we will likely sleep while handling the interrupt, trigger is active-low, don't handle concurrent interrupts */
	ret = devm_request_threaded_irq(&chip->client->dev, chip->gpio_IntN_irq, NULL, _fusb_isr_intn, IRQF_ONESHOT | IRQF_TRIGGER_LOW, FUSB_DT_INTERRUPT_INTN, chip);	// devm_* allocation/free handled by system
	if (ret) {
		dev_err(&chip->client->dev,
			"%s - Error: Unable to request threaded IRQ for INT_N GPIO! Error code: %d\n",
			__func__, ret);
		fusb_GPIO_Cleanup();
		return ret;
	}
	device_init_wakeup(&chip->client->dev, true);
	enable_irq_wake(chip->gpio_IntN_irq);
	enable_irq(chip->gpio_IntN_irq);
	return 0;
}

/*******************************************************************************
* Function:        _fusb_isr_intn
* Input:           irq - IRQ that was triggered
*                  dev_id - Ptr to driver data structure
* Return:          irqreturn_t - IRQ_HANDLED on success, IRQ_NONE on failure
* Description:     Activates the core
********************************************************************************/
static irqreturn_t _fusb_isr_intn(int irq, void *dev_id)
{
	struct fusb30x_chip *chip = dev_id;
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return IRQ_NONE;
	}
#ifdef DEBUG
	dbg_fusb_GPIO_Set_SM_Toggle(!chip->dbg_gpio_StateMachine_value);	// Optionally toggle debug GPIO when SM is called to measure thread tick rate

	if (chip->dbgSMTicks++ >= U8_MAX)	// Tick our state machine tick counter
	{
		chip->dbgSMRollovers++;	// Record a moderate amount of rollovers
	}
#endif // DEBUG
	pm_stay_awake(&chip->client->dev);
	core_state_machine();	// Run the state machine
	pm_relax(&chip->client->dev);
	return IRQ_HANDLED;
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
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
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
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return;
	}
#ifdef DEBUG
	dbg_fusb_GPIO_Set_SM_Toggle(!chip->dbg_gpio_StateMachine_value);	// Optionally toggle debug GPIO when SM is called to measure thread tick rate

	if (chip->dbgSMTicks++ >= U8_MAX)	// Tick our state machine tick counter
	{
		chip->dbgSMRollovers++;	// Record a moderate amount of rollovers
	}
#endif // DEBUG

	core_state_machine();	// Run the state machine
	schedule_work(&chip->worker);	// Reschedule ourselves to run again
}

void fusb_InitializeWorkers(void)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	printk(KERN_DEBUG "FUSB  %s - Initializing threads!\n", __func__);
	if (!chip) {
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
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
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
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
		printk(KERN_ALERT "FUSB  %s - Error: Chip structure is NULL!\n",
		       __func__);
		return;
	}
	schedule_delayed_work(&chip->init_worker,
			      msecs_to_jiffies(chip->InitDelayMS));
}

#endif // FSC_INTERRUPT_TRIGGERED, else
