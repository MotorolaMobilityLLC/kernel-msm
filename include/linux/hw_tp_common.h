/* < DTS2014010309198 sunlibin 20140104 begin */
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

/* < DTS2014012604495 sunlibin 20140126 begin */
/*IC type*/
/* < DTS2014033108554 songrongyuan 20140408 begin */
/*To delete the information of product ID */
#define IC_TYPE_3207 3207

/* < DTS2014031208199 songrongyuan 20140312 begin */
/* < DTS2014032805796 songrongyuan 20140328 begin */
/*To add to the product id information of the lensone*/
#define FW_OFILM_STR "000"
#define FW_EELY_STR "001"
#define FW_TRULY_STR "002"
#define FW_JUNDA_STR "005"
#define FW_LENSONE_STR "006"
/* DTS2014033108554 songrongyuan 20140408 end > */

#define MODULE_STR_LEN 3

enum f54_product_module_name {
	FW_OFILM = 0,
	FW_EELY = 1,
	FW_TRULY = 2,
	FW_JUNDA = 5,
	UNKNOW_PRODUCT_MODULE = 0xff,
};
/* DTS2014032805796 songrongyuan 20140328 end > */
/* DTS2014031208199 songrongyuan 20140312 end> */
/* DTS2014012604495 sunlibin 20140126 end> */
/* < DTS2014030305441 zhangmin 201400303 begin */
struct holster_mode{
	unsigned long holster_enable;
	int top_left_x0;
	int top_left_y0;
	int bottom_right_x1;
	int bottom_right_y1;
};
/* DTS2014030305441 zhangmin 20140303 end > */

struct kobject* tp_get_touch_screen_obj(void);
/* < DTS2014011515693 sunlibin 20140212 begin */
struct kobject* tp_get_glove_func_obj(void);
/* DTS2014011515693 sunlibin 20140212 end > */

#endif

/* DTS2014010309198 sunlibin 20140104 end > */
