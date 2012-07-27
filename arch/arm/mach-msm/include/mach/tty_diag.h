#ifndef _DRIVERS_TTY_DIAG_H_
#define _DRIVERS_TTY_DIAG_H_

#include <linux/diagchar.h>

#define DIAG_LEGACY		"diag"
#define DIAG_MDM		"diag_mdm"

struct legacy_diag_ch *tty_diag_channel_open(const char *name, void *priv,
		void (*notify)(void *, unsigned, struct diag_request *));
void tty_diag_channel_close(struct legacy_diag_ch *diag_ch);
int tty_diag_channel_read(struct legacy_diag_ch *diag_ch,
				struct diag_request *d_req);
int tty_diag_channel_write(struct legacy_diag_ch *diag_ch,
				struct diag_request *d_req);
void tty_diag_channel_abandon_request(void);
int tty_diag_get_dbg_ftm_flag_value(void);
int tty_diag_set_dbg_ftm_flag_value(int val);

#endif /* _DRIVERS_TTY_DIAG_H_ */
