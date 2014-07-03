#ifndef _HDMI_CMD_H_
#define _HDMI_CMD_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

extern int otm_cmdline_parse_option(char *);
extern int otm_cmdline_set_vic_option(int vic);
extern void otm_print_cmdline_option();

#endif

