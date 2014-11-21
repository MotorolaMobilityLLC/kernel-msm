struct touchxs {
	void (*touchx)(int *x, int *y, unsigned char fn, unsigned char nf);
	struct input_dev *touch_magic_dev;
	struct mutex virtual_touch_mutex;
	int finger_down;
};

extern struct touchxs touchxp;
