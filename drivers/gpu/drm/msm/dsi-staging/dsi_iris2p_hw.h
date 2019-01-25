#ifndef _DSI_IRIS2P_HARDWARE_H_
#define _DSI_IRIS2P_HARDWARE_H_

struct iris_hw_config{
	int gpio_valid;
	int iris_reset_gpio;
	int iris_1v1;
	int iris_wakeup;
	int iris_gpio_test;
	struct clk *div_clk3;
};

#endif//_DSI_IRIS2P_HARDWARE_H_
