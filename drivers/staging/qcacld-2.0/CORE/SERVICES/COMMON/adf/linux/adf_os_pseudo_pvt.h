/*
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */


#ifndef __ADF_HST_OS_PSEUDO_PVT_H
#define __ADF_HST_OS_PSEUDO_PVT_H

#include <linux/module.h>

#define NUM_PSEUDO_DEVS     2

#define __adf_os_pseudo_module_init(_fn)   \
    static inline int __adf_os_pseudo_mod_init(void) \
    {   \
        a_status_t st;  \
        int i = 0; \
        st = (_fn)();   \
        for(i = 0; i < NUM_PSEUDO_DEVS; i++)    \
            __adf_net_pseudo_attach(mod_name); \
        return st;      \
    }   \
    module_init(__adf_os_pseudo_mod_init);

#define __adf_os_pseudo_module_exit(_fn)   \
    static inline void __adf_os_pseudo_mod_exit(void) \
    {   \
        int i = 0; \
        for(i = 0; i < NUM_PSEUDO_DEVS; i++)    \
            __adf_net_pseudo_detach(mod_name);  \
        (_fn)();    \
    } \
    module_exit(__adf_os_pseudo_mod_exit);


/**
 * initiallize the PCI driver structure
 * Instance name will be <name>_pci_info
 */
#define __adf_os_pseudo_set_drv_info(_name, _ifname, _pseudo_ids, _attach, _detach, \
    _suspend, _resume)     \
{   \
   .drv_attach  = (_attach),  \
   .drv_detach  = (_detach),  \
   .drv_suspend = (_suspend),    \
   .drv_resume  = (_resume),  \
   .pci_id      = (_pseudo_ids),   \
   .mod_name    = #_name,    \
   .ifname      = #_ifname,  \
}; \
const char *mod_name = #_name; \
extern int __adf_net_pseudo_attach(const char *mod_name); \
extern int __adf_net_pseudo_detach(const char *mod_name);

#endif
