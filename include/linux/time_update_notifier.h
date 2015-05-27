#ifndef _TIME_UPDATE_NOTIFIER_H
#define _TIME_UPDATE_NOTIFIER_H

#include <linux/notifier.h>

int time_update_register_notifier(struct notifier_block *nb);
int time_update_unregister_notifier(struct notifier_block *nb);

#endif /* _TIME_UPDATE_NOTIFIER_H */
