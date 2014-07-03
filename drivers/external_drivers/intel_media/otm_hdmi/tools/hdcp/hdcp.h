#ifndef HDCP_MODULE_H
#define HDCP_MODULE_H
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

extern bool module_disable_hdcp;
extern bool module_force_ri_mismatch;
#endif/* HDCP_MODULE_H */
