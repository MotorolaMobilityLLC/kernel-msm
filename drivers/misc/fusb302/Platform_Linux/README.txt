README.TXT

Fairchild Semiconductor FUSB302 Linux Platform Driver Integration Notes
_______________________________________________________________________________
Device Tree
	Currently, the driver requires the use of Device Tree in order to 
	function with the I2C bus and the required GPIO pins. Modify the 
	following Device Tree snippet to specify resources specific to your 
	system and include them in your kernel's Device Tree. The FUSB302 
	requires a minimum of 2 GPIO pins, a valid I2C bus, and I2C slave 
	address 0x22.
	
	/*********************************************************************/
	i2c@######## {								// replace ######## with the address of your I2C bus
		fusb30x@22 {							// I2C Slave Address - Always @22
				compatible = "fairchild,fusb302";		// String must match driver's .compatible string exactly
				reg = <0x22>;					// I2C Slave address
				status = "okay";				// The device is enabled, comment out or delete to disable
				//status = "disabled";				// Uncomment to disable the device from being loaded at boot
				fairchild,vbus5v	= <&msm_gpio 39 0>;	// VBus 5V GPIO pin - <&gpio_bus pin# 0>. Do not change "fairchild,vbus5v"
				fairchild,vbusOther	= <&msm_gpio 40 0>;	// VBus Other GPIO pin - optional, but if used, name "fairchild,vbusOther" must not change.
				fairchild,int_n		= <&pm8994_gpios 4 0>;	// INT_N GPIO pin - <&gpio_bus pin# 0>. Do not change "fairchild,int_n"
			};
	};
	/*********************************************************************/

_______________________________________________________________________________
Compilation/Makefile
	You must define the preprocessor macro "PLATFORM_LINUX" in order to 
	pull in the correct typedefs. You may define the preprocessor macro 
	"FSC_INTERRUPT_TRIGGERED" to configure the driver to run in interrupt 
	mode. Otherwise, the driver will run in polling mode. 
	
	The following example snippet is from a Makefile expecting the 
	following directory structure:
	
	path/to/MakefileDir/
		|---- Makefile
		|---- Platform_Linux/
		|---- core/
			|---- vdm/
				|---- DisplayPort/
	
	Makefile
	/*********************************************************************/
	# Required flag to configure the core to operate with the Linux kernel
	ccflags-y += -DPLATFORM_LINUX
	# Optional flag to enable interrupt-driven operation
	ccflags-y += -DFSC_INTERRUPT_TRIGGERED
	obj-y   += fusb30x_whole.o
	fusb30x_whole-objs := Platform_Linux/fusb30x_driver.o \
			      Platform_Linux/fusb30x_global.o \
			      Platform_Linux/platform.o \
			      Platform_Linux/platform_helpers.o \
			      Platform_Linux/hostcomm.o \
			      core/AlternateModes.o \
			      core/core.o \
			      core/fusb30X.o \
			      core/Log.o \
			      core/PDPolicy.o \
			      core/PDProtocol.o \
			      core/TypeC.o \
			      core/vdm/bitfield_translators.o \
			      core/vdm/vdm.o \
			      core/vdm/vdm_callbacks.o \
			      core/vdm/vdm_config.o \
			      core/vdm/DisplayPort/configure.o \
			      core/vdm/DisplayPort/dp.o \
			      core/vdm/DisplayPort/dp_system_stubs.o
	/*********************************************************************/