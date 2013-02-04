#ifndef _DRIVERS_TTY_DIAG_H_
#define _DRIVERS_TTY_DIAG_H_

#ifdef CONFIG_DIAG_OVER_USB
#include <mach/usbdiag.h>
#endif

struct usb_diag_ch *tty_diag_channel_open(const char *name, void *priv,
		void (*notify)(void *, unsigned, struct diag_request *));
void tty_diag_channel_close(struct usb_diag_ch *diag_ch);
int tty_diag_channel_read(struct usb_diag_ch *diag_ch,
				struct diag_request *d_req);
int tty_diag_channel_write(struct usb_diag_ch *diag_ch,
				struct diag_request *d_req);
void tty_diag_channel_abandon_request(void);
int tty_diag_get_dbg_ftm_flag_value(void);
int tty_diag_set_dbg_ftm_flag_value(int val);

#endif /* _DRIVERS_TTY_DIAG_H_ */
