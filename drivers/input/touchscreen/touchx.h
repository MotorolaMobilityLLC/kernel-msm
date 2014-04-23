/*
 * Copyright (C) 2014 Motorola Mobility LLC.
 */

#ifndef _TOUCHXM_
#define _TOUCHXM_

struct touchxs {
	void (*touchx)(int *x, int *y, unsigned char fn, unsigned char nf);
	struct input_dev *touch_magic_dev;
	struct mutex virtual_touch_mutex;
	int finger_down;
};

extern struct touchxs touchxp;

#endif
