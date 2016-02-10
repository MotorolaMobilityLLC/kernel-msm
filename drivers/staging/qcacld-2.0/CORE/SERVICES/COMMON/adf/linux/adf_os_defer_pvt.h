/*
 * Copyright (c) 2013-2015 The Linux Foundation. All rights reserved.
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


#ifndef _ADF_CMN_OS_DEFER_PVT_H
#define _ADF_CMN_OS_DEFER_PVT_H

#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include "vos_cnss.h"
#include <adf_os_types.h>

typedef struct tasklet_struct  __adf_os_bh_t;
typedef struct workqueue_struct __adf_os_workqueue_t;

/**
 * wrapper around the real task func
 */
typedef struct {
    struct work_struct   work;
    adf_os_defer_fn_t    fn;
    void                 *arg;
}__adf_os_work_t;

typedef struct {
    struct delayed_work  dwork;
    adf_os_defer_fn_t    fn;
    void                 *arg;
}__adf_os_delayed_work_t;

extern void __adf_os_defer_func(struct work_struct *work);
extern void __adf_os_defer_delayed_func(struct work_struct *work);

typedef void (*__adf_os_bh_fn_t)(unsigned long arg);

static inline a_status_t
__adf_os_init_work(adf_os_handle_t    hdl,
                   __adf_os_work_t      *work,
                   adf_os_defer_fn_t    func,
                   void                 *arg)
{
    /*Initilize func and argument in work struct */
    work->fn = func;
    work->arg = arg;
    vos_init_work(&work->work, __adf_os_defer_func);
    return A_STATUS_OK;
}

static inline a_status_t
__adf_os_init_delayed_work(adf_os_handle_t    hdl,
                   __adf_os_delayed_work_t      *work,
                   adf_os_defer_fn_t    func,
                   void                 *arg)
{
    /*Initilize func and argument in work struct */
    work->fn = func;
    work->arg = arg;
    vos_init_delayed_work(&work->dwork, __adf_os_defer_delayed_func);
    return A_STATUS_OK;
}

static inline __adf_os_workqueue_t* __adf_os_create_workqueue(char *name)
{
    return create_workqueue(name);
}

static inline void __adf_os_queue_work(adf_os_handle_t hdl, __adf_os_workqueue_t *wqueue, __adf_os_work_t* work)
{
    queue_work(wqueue, &work->work);
}

static inline void __adf_os_queue_delayed_work(adf_os_handle_t hdl, __adf_os_workqueue_t *wqueue, __adf_os_delayed_work_t* work, a_uint32_t delay)
{
    queue_delayed_work(wqueue, &work->dwork, delay);
}

static inline void __adf_os_flush_workqueue(adf_os_handle_t hdl, __adf_os_workqueue_t *wqueue)
{
    flush_workqueue(wqueue);
}

static inline void __adf_os_destroy_workqueue(adf_os_handle_t hdl, __adf_os_workqueue_t *wqueue)
{
    destroy_workqueue(wqueue);
}

static inline  a_status_t __adf_os_init_bh(adf_os_handle_t  hdl,
                                     struct tasklet_struct *bh,
                                     adf_os_defer_fn_t  func,
                                     void               *arg)
{
     tasklet_init(bh, (__adf_os_bh_fn_t)func, (unsigned long)arg);

     return A_STATUS_OK;
}

static inline a_status_t
__adf_os_sched_work(adf_os_handle_t hdl, __adf_os_work_t  * work)
{
    schedule_work(&work->work);
    return A_STATUS_OK;
}

static inline a_status_t  __adf_os_sched_bh(adf_os_handle_t hdl,
                                     struct tasklet_struct * bh)
{
    tasklet_schedule(bh);

    return A_STATUS_OK;
}

static inline a_status_t
__adf_os_disable_work(adf_os_handle_t hdl, __adf_os_work_t  * work)
{
   /**
    * XXX:???
    */
    return A_STATUS_OK;
}
static inline a_status_t
__adf_os_disable_bh(adf_os_handle_t hdl, struct tasklet_struct *bh)
{
    tasklet_kill(bh);

    return A_STATUS_OK;
}
#endif /*_ADF_CMN_OS_DEFER_PVT_H*/
