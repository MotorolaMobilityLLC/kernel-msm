/* 
 * File:   fusb30x_driver.c
 * Author: Tim Bremm <tim.bremm@fairchildsemi.com>
 * Company: Fairchild Semiconductor
 *
 * Created on September 2, 2015, 10:22 AM
 */

/* Standard Linux includes */
#include <linux/init.h>		// __init, __initdata, etc
#include <linux/module.h>	// Needed to be a module
#include <linux/kernel.h>	// Needed to be a kernel module
#include <linux/i2c.h>		// I2C functionality
#include <linux/slab.h>		// devm_kzalloc
#include <linux/types.h>	// Kernel datatypes
#include <linux/errno.h>	// EINVAL, ERANGE, etc
#include <linux/of_device.h>	// Device tree functionality
#include <linux/interrupt.h>
/* Driver-specific includes */
#include "fusb30x_global.h"	// Driver-specific structures/types
#include "platform_helpers.h"	// I2C R/W, GPIO, misc, etc

#ifdef FSC_DEBUG
#include "../core/core.h"	// GetDeviceTypeCStatus
#endif // FSC_DEBUG
#include "../core/TypeC_Types.h"
#include "../core/fusb30X.h"
#include "fusb30x_driver.h"

#define FUSB_LOG_PAGES 50
void *fusb302_ipc_log = NULL;
CC_ORIENTATION fusb302_cc = NONE;

void fusb302_shutdown(struct i2c_client *i2c)
{
	u8 data = 0x0;
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (!chip) {
		pr_alert("FUSB  %s - Error: Chip structure is NULL!\n",
				 __func__);
		return;
	}

	DeviceWrite(regPower, 1, &data);	/* Disable Power */
	DeviceWrite(regControl2, 1, &data);	/* Disable Toggle */

	disable_irq(chip->gpio_IntN_irq);
}

enum power_supply_property fusb_power_supply_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_DISABLE_USB,
	POWER_SUPPLY_PROP_WAKEUP,
	POWER_SUPPLY_PROP_MASK_INT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_SWITCH_STATE,
};

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
	int ret = 0;
	char data[5];
	ConnectionState ConnState;

	GetDeviceTypeCStatus(data);
	ConnState = (ConnectionState)data[1] & 0xFF;
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
		const unsigned int *val)
{
	/* Return nothing for now */
	return 0;
}

/******************************************************************************
* Driver functions
******************************************************************************/
static int __init fusb30x_init(void)
{
	FUSB_LOG("FUSB  %s - Start driver initialization...\n", __func__);

	return i2c_add_driver(&fusb30x_driver);
}

static void __exit fusb30x_exit(void)
{
	i2c_del_driver(&fusb30x_driver);
	FUSB_LOG("FUSB  %s - Driver deleted...\n", __func__);
}

static int fusb30x_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int ret = 0;
	struct fusb30x_chip *chip;
	struct i2c_adapter *adapter;
	struct dual_role_phy_desc *desc;
	struct dual_role_phy_instance *dual_role;
	USBTypeCPort PortType;

	if (!client) {
		pr_err("FUSB  %s - Error: Client structure is NULL!\n",
		       __func__);
		return -EINVAL;
	}
	dev_info(&client->dev, "%s\n", __func__);
	fusb302_ipc_log = ipc_log_context_create(FUSB_LOG_PAGES, "fusb302", 0);
	/* Make sure probe was called on a compatible device */
	if (!of_match_device(fusb30x_dt_match, &client->dev)) {
		dev_err(&client->dev,
			"FUSB  %s - Error: Device tree mismatch!\n", __func__);
		return -EINVAL;
	}
	FUSB_LOG("FUSB  %s - Device tree matched!\n", __func__);

	/* Allocate space for our chip structure (devm_* is managed by the device) */
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev,
			"FUSB  %s - Error: Unable to allocate memory for g_chip!\n",
			__func__);
		return -ENOMEM;
	}
	chip->client = client;	// Assign our client handle to our chip
	fusb30x_SetChip(chip);	// Set our global chip's address to the newly allocated memory
	FUSB_LOG("FUSB  %s - Chip structure is set! Chip: %p ... g_chip: %p\n",
		 __func__, chip, fusb30x_GetChip());

	/* Initialize the chip lock */
	mutex_init(&chip->lock);

	/* Initialize the chip's data members */
	fusb_InitChipData();
	FUSB_LOG("FUSB  %s - Chip struct data initialized!\n", __func__);

	/* Verify that the system has our required I2C/SMBUS functionality (see <linux/i2c.h> for definitions) */
	adapter = to_i2c_adapter(client->dev.parent);
	if (i2c_check_functionality
	    (adapter, FUSB30X_I2C_SMBUS_BLOCK_REQUIRED_FUNC)) {
		chip->use_i2c_blocks = true;
	} else {
		// If the platform doesn't support block reads, try with block writes and single reads (works with eg. RPi)
		// NOTE: It is likely that this may result in non-standard behavior, but will often be 'close enough' to work for most things
		dev_warn(&client->dev,
			 "FUSB  %s - Warning: I2C/SMBus block read/write functionality not supported, checking single-read mode...\n",
			 __func__);
		if (!i2c_check_functionality
		    (adapter, FUSB30X_I2C_SMBUS_REQUIRED_FUNC)) {
			dev_err(&client->dev,
				"FUSB  %s - Error: Required I2C/SMBus functionality not supported!\n",
				__func__);
			dev_err(&client->dev,
				"FUSB  %s - I2C Supported Functionality Mask: 0x%x\n",
				__func__, i2c_get_functionality(adapter));
			return -EIO;
		}
	}
	FUSB_LOG("FUSB  %s - I2C Functionality check passed! Block reads: %s\n",
		 __func__, chip->use_i2c_blocks ? "YES" : "NO");

	/* Assign our struct as the client's driverdata */
	i2c_set_clientdata(client, chip);
	FUSB_LOG("FUSB  %s - I2C client data set!\n", __func__);

	/* Verify that our device exists and that it's what we expect */
	if (!fusb_IsDeviceValid()) {
		dev_err(&client->dev,
			"FUSB  %s - Error: Unable to communicate with device!\n",
			__func__);
		return -EIO;
	}
	FUSB_LOG("FUSB  %s - Device check passed!\n", __func__);
	usbc_psy.name	= "usbc";
	usbc_psy.type		= POWER_SUPPLY_TYPE_USBC;
	usbc_psy.get_property	= fusb_power_supply_get_property;
	usbc_psy.set_property	= fusb_power_supply_set_property;
	usbc_psy.properties	= fusb_power_supply_props;
	usbc_psy.num_properties	= ARRAY_SIZE(fusb_power_supply_props);
	usbc_psy.property_is_writeable = fusb_power_supply_is_writeable;
	ret = power_supply_register(&client->dev, &usbc_psy);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		return ret;
	}

	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
		desc = devm_kzalloc(&client->dev,
			sizeof(struct dual_role_phy_desc),
			GFP_KERNEL);
		if (!desc) {
			dev_err(&client->dev, "unable to allocate dual role descriptor\n");
			goto unregister_usbcpsy;
		}

		desc->name = "otg_default";
		PortType = (USBTypeCPort)GetTypeCSMControl() & 0x03;
		switch (PortType) {
		case USBTypeC_Sink:
			desc->supported_modes =
				DUAL_ROLE_SUPPORTED_MODES_UFP;
			break;
		case USBTypeC_Source:
			desc->supported_modes =
				DUAL_ROLE_SUPPORTED_MODES_DFP;
			break;
		case USBTypeC_DRP:
		default:
			desc->supported_modes =
				DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
			break;
		}

		desc->get_property = dual_role_get_local_prop;
		desc->set_property = dual_role_set_prop;
		desc->properties = fusb_drp_properties;
		desc->num_properties = ARRAY_SIZE(fusb_drp_properties);
		desc->property_is_writeable = dual_role_is_writeable;
		dual_role = devm_dual_role_instance_register(
					   &client->dev, desc);
		dual_role->drv_data = client;
		chip->dual_role = dual_role;
		chip->desc = desc;
	}

	/* Initialize the platform's GPIO pins and IRQ */
	ret = fusb_InitializeGPIO();
	if (ret) {
		dev_err(&client->dev,
			"FUSB  %s - Error: Unable to initialize GPIO!\n",
			__func__);
		goto unregister_usbcpsy;
	}
	FUSB_LOG("FUSB  %s - GPIO initialized!\n", __func__);

	/* Initialize our timer */
	fusb_InitializeTimer();
	FUSB_LOG("FUSB  %s - Timers initialized!\n", __func__);

	/* Initialize the core and enable the state machine (NOTE: timer and GPIO must be initialized by now) */
	fusb_InitializeCore();
	FUSB_LOG("FUSB  %s - Core is initialized!\n", __func__);

#ifdef FSC_DEBUG
	/* Initialize debug sysfs file accessors */
	fusb_Sysfs_Init();
	FUSB_LOG("FUSB  %s - Sysfs device file created!\n", __func__);
#endif // FSC_DEBUG

#ifdef FSC_INTERRUPT_TRIGGERED
	fusb_InitializeWakeWorker();
	/* Enable interrupts after successful core/GPIO initialization */
	ret = fusb_EnableInterrupts();
	if (ret) {
		dev_err(&client->dev,
			"FUSB  %s - Error: Unable to enable interrupts! Error code: %d\n",
			__func__, ret);
		goto unregister_usbcpsy;
	}
#else
	/* Init our workers, but don't start them yet */
	fusb_InitializeWorkers();
	/* Start worker threads after successful initialization */
	fusb_ScheduleWork();
	FUSB_LOG("FUSB  %s - Workers initialized and scheduled!\n", __func__);
#endif // ifdef FSC_POLLING elif FSC_INTERRUPT_TRIGGERED

	fusb302_debug_init();
	dev_info(&client->dev,
		 "FUSB  %s - FUSB30X Driver loaded successfully!\n", __func__);
	return ret;
unregister_usbcpsy:
	power_supply_unregister(&usbc_psy);
	return ret;
}

static int fusb30x_remove(struct i2c_client *client)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (!chip) {
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return -EINVAL;
	}
	FUSB_LOG("FUSB  %s - Removing fusb30x device!\n", __func__);

#ifndef FSC_INTERRUPT_TRIGGERED	// Polling mode by default
	fusb_StopThreads();
#endif // !FSC_INTERRUPT_TRIGGERED

	fusb_StopTimers();
	fusb_GPIO_Cleanup();
	fusb302_debug_remove();
	power_supply_unregister(&usbc_psy);
	if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
		devm_dual_role_instance_unregister(
			&client->dev, chip->dual_role);
		devm_kfree(&client->dev, chip->desc);
	}
	FUSB_LOG("FUSB  %s - FUSB30x device removed from driver...\n",
		 __func__);
	if (fusb302_ipc_log)
		ipc_log_context_destroy(fusb302_ipc_log);
	fusb302_ipc_log = NULL;
	return 0;
}

/*******************************************************************************
 * Driver macros
 ******************************************************************************/
module_init(fusb30x_init);	// Defines the module's entrance function
module_exit(fusb30x_exit);	// Defines the module's exit function

MODULE_LICENSE("GPL");		// Exposed on call to modinfo
MODULE_DESCRIPTION("Fairchild FUSB30x Driver");	// Exposed on call to modinfo
MODULE_AUTHOR("Tim Bremm<tim.bremm@fairchildsemi.com>");	// Exposed on call to modinfo
