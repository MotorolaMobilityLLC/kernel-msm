# IPC Devices
obj-y += platform_sst_audio.o
obj-y += platform_mrfl_regulator.o
obj-y += platform_soc_thermal.o
obj-$(subst m,y,$(CONFIG_SND_MOOR_MACHINE)) += platform_mrfld_audio.o
obj-y += platform_ipc.o
obj-y += platform_i2c_gpio.o
obj-y += platform_msic.o
obj-y += platform_msic_audio.o
obj-y += platform_msic_gpio.o
obj-y += platform_msic_ocd.o
obj-y += platform_tc35876x.o
obj-y += pci/
obj-$(subst m,y,$(CONFIG_INTEL_MID_POWER_BUTTON)) += platform_msic_power_btn.o
obj-$(subst m,y,$(CONFIG_GPIO_INTEL_PMIC)) += platform_pmic_gpio.o
obj-$(subst m,y,$(CONFIG_SENSORS_MRFL_OCD)) += platform_mrfl_ocd.o
obj-$(subst m,y,$(CONFIG_PMIC_CCSM)) += platform_mrfl_pmic.o
obj-$(subst m,y,$(CONFIG_I2C_PMIC)) += platform_mrfl_pmic_i2c.o
obj-$(subst m,y,$(CONFIG_INTEL_MOOR_THERMAL)) += platform_moor_thermal.o
obj-$(subst m,y,$(CONFIG_INTEL_SCU_FLIS)) += platform_scu_flis.o

# I2C Devices
obj-$(subst m,y,$(CONFIG_I2C_DESIGNWARE_CORE_FORK)) += platform_dw_i2c.o
obj-$(subst m,y,$(CONFIG_GPIO_PCA953X)) += platform_max7315.o
obj-$(subst m,y,$(CONFIG_GPIO_PCA953X)) += platform_tca6416.o
obj-$(subst m,y,$(CONFIG_CHARGER_BQ24192)) += platform_bq24192.o
obj-$(subst m,y,$(CONFIG_BQ24261_CHARGER)) += platform_bq24261.o
obj-$(subst m,y,$(CONFIG_SND_SOC_WM8994)) += platform_wm8994.o
obj-$(subst m,y,$(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4_FORK)) += platform_rmi4.o
obj-$(subst m,y,$(CONFIG_TOUCHSCREEN_R69001_I2C)) += platform_r69001.o
obj-$(subst m,y,$(CONFIG_NFC_PN544_PLATFORM_DATA)) += platform_pn544.o
obj-$(subst m,y,$(CONFIG_SENSORS_LSM303_MAG)) += platform_lsm303.o
obj-$(subst m,y,$(CONFIG_GPIO_PCA953X)) += platform_pca9574.o

# SPI Devices
obj-$(subst m,y,$(CONFIG_SERIAL_MRST_MAX3110)) += platform_max3111.o

# MISC Devices
obj-$(subst m,y,$(CONFIG_KEYBOARD_GPIO)) += platform_gpio_keys.o
# ADC
obj-$(subst m,y,$(CONFIG_MSIC_GPADC))	+= platform_msic_adc.o
obj-$(subst m,y,$(CONFIG_IIO_BASINCOVE_GPADC))	+= platform_bcove_adc.o
# UART Devices
obj-$(subst m,y,$(CONFIG_SERIAL_MFD_HSU_EXT)) += platform_hsu.o
#I2C Devices

# Panel Control Device
obj-$(subst m,y,$(CONFIG_DRM_MRFLD)) += platform_panel.o
# WIFI devices
obj-$(subst m,y,$(CONFIG_WIFI_PLATFORM_DATA)) += platform_wifi.o
# BT devices
obj-$(subst m,y,$(CONFIG_BCM_BT_LPM)) += platform_btlpm.o
# SCU log
obj-$(subst m,y,$(CONFIG_SCU_LOGGING)) += platform_scu_log.o
# Display Control Device
obj-y += platform_display.o
