#ifndef MODE_NOTIFY_H
#define MODE_NOTIFY_H


#include <linux/notifier.h>
/**
 *   Notification Msg definition
 */

int register_mode_notifier(struct notifier_block *nb);
int unregister_mode_notifier(struct notifier_block *nb);

enum mode_msg{
		FB_BLANK_ENTER_NON_INTERACTIVE,
		FB_BLANK_ENTER_INTERACTIVE,
		FB_BLANK_AMBIENT_ON,
		FB_BLANK_AMBIENT_OFF,
};

#endif
