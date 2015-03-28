/*Add for huawei TP*/
/*
 * Copyright (c) 2014 Huawei Device Company
 *
 * This file provide common requeirment for different touch IC.
 * 
 * 2014-01-04:Add "tp_get_touch_screen_obj" by sunlibin
 *
 */
#ifndef __HW_TP_COMMON__
#define __HW_TP_COMMON__

/*IC type*/
/*To delete the information of product ID */
#define IC_TYPE_3207 3207

/*To add to the product id information of the lensone*/
#define FW_OFILM_STR "000"
#define FW_EELY_STR "001"
#define FW_TRULY_STR "002"
#define FW_JUNDA_STR "005"
#define FW_LENSONE_STR "006"

#define MODULE_STR_LEN 3

enum f54_product_module_name {
	FW_OFILM = 0,
	FW_EELY = 1,
	FW_TRULY = 2,
	FW_JUNDA = 5,
	UNKNOW_PRODUCT_MODULE = 0xff,
};
struct holster_mode {
	unsigned long holster_enable;
	int top_left_x0;
	int top_left_y0;
	int bottom_right_x1;
	int bottom_right_y1;
};

struct kobject *tp_get_touch_screen_obj(void);
struct kobject *tp_get_glove_func_obj(void);

#endif

