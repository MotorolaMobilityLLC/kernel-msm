/*
 * These are the public elements of the RF detect module.
 */

#ifndef __SAR_H__
#define __SAR_H__

struct rf_platform_data {
	unsigned int gpio_main;
	unsigned int gpio_aux;
	int debounce_interval;
};

#endif /* __SAR_H__ */
