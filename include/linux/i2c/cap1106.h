
#ifndef _LINUX_I2C_CAP1106_H
#define _LINUX_I2C_CAP1106_H

#define CAP1106_I2C_NAME "cap1106"
#define CAP1106_SAR_GPIO 1 /* APP2MDM_SAR */
#define CAP1106_SAR_GPIO_NAME "APP2MDM_SAR"
#define CAP1106_DET_GPIO 52 /* SAR_DET_3G */
#define CAP1106_DET_GPIO_NAME "SAR_DET_3G"
#define CAP1106_INIT_TABLE_SIZE 26

struct cap1106_i2c_platform_data {
	int app2mdm_enable;
	int sar_gpio;
	char *sar_gpio_name;
	int det_gpio;
	char *det_gpio_name;
	const unsigned char init_table[CAP1106_INIT_TABLE_SIZE];
};

#endif /* _LINUX_I2C_CAP1106_H */
