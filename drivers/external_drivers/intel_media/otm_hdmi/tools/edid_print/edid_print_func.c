
#include <linux/kernel.h>
#include <linux/types.h>
#include <otm_hdmi_defs.h>
#include <otm_hdmi.h>

#include "edid_print.h"

void set_log_level(int log_level) {

	if (log_level >= 0 && log_level <= 4) {
		struct hdmi_context_t *hdmictx =
			(struct hdmi_context_t*) otm_hdmi_get_context();
		void *log_val = (void *)&log_level;
		otm_hdmi_set_attribute(hdmictx, OTM_HDMI_ATTR_ID_DEBUG,
					log_val, false);
	} else {
		printk("enter log_level >= 0 and <=4");
	}
}
