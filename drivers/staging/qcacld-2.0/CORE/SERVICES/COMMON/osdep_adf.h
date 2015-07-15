/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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


#ifndef _ATH_LINUX_OSDEP_ADF_H
#define _ATH_LINUX_OSDEP_ADF_H

#include "wlan_opts.h"
#include <adf_os_mem.h>
#include <adf_os_lock.h>
#include <adf_os_time.h>
#include <adf_os_timer.h>
#include <adf_os_io.h>
#include <adf_os_module.h>
#include <adf_os_defer.h>

 #include <net/sch_generic.h>
 #include <net/inet_ecn.h>
 #include <linux/netdevice.h>
#if defined(HIF_USB)
#include <linux/usb.h>
#else
 #include <linux/pci.h>
#endif
 #include <linux/stddef.h>
 #include <linux/err.h>
 #include <asm/checksum.h>
 #include <net/ip6_checksum.h>
 #include <linux/crc32.h>


 #include <linux/ip.h>
 #include <linux/tcp.h>
 #include <linux/in.h>


#include <adf_nbuf.h>
#include <sys/queue.h>

#include <linux/if_vlan.h>

#include "if_upperproto.h"
#include "ah_osdep.h"

#ifdef AR9100
#include <ar9100.h>
#endif /* AR9100 */

#define INLINE    __adf_os_inline

/* UNREFERENCED_PARAMETER - provide a dummy reference */
#define UNREFERENCED_PARAMETER(an) ((void) (an))

#if ATH_DEBUG
#ifndef ASSERT
#define ASSERT(expr)  adf_os_assert(expr)
#endif
#else
#define ASSERT(expr)
#endif /* ATH_DEBUG */

#define OS_LOG_DBGPRINT(_xfmt, ...)

#ifdef AR9100
/*
 * Howl needs DDR FIFO flush before any desc/dma data can be read.
 */
#define ATH_FLUSH_FIFO    ar9100_flush_wmac
#else
#define ATH_FLUSH_FIFO()
#endif

#define KASSERT(exp, msg) do {          \
    if (unlikely(!(exp))) {         \
        printk msg;         \
        BUG();              \
    }                   \
} while (0)

/*
 * Map Linux spin locks to OS independent names
 */
#ifdef ANDROID
static inline void spin_lock_dpc(spinlock_t *lock)
{
    adf_os_spin_lock_bh((adf_os_spinlock_t *)lock);
}
static inline void spin_unlock_dpc(spinlock_t *lock)
{
    adf_os_spin_unlock_bh((adf_os_spinlock_t *)lock);
}
#else
#define spin_lock_dpc(a)    adf_os_spin_lock_bh(a)
#define spin_unlock_dpc(a)  adf_os_spin_unlock_bh(a)
#endif

#define spin_lock_destroy(a)

#define os_tasklet_lock(a, b)        adf_os_spin_lock_irq(a, b)
#define os_tasklet_unlock(a, b)      adf_os_spin_unlock_irq(a, b)

/*
** Need to define byte order based on the CPU configuration.
*/
#ifndef _LITTLE_ENDIAN
#define _LITTLE_ENDIAN  1234
#endif
#ifndef _BIG_ENDIAN
#define _BIG_ENDIAN 4321
#endif
#ifdef __BIG_ENDIAN
    #define _BYTE_ORDER    _BIG_ENDIAN
#else
    #define _BYTE_ORDER    _LITTLE_ENDIAN
#endif

/*
 * Work Queue related macros
 */

#define ATH_CREATE_WQUEUE(name)  adf_os_create_workqueue(name)
#define ATH_CREATE_WORK(a,b,c)   adf_os_create_work(0, a, b, c)
#define ATH_CREATE_DELAYED_WORK(a,b,c)   adf_os_create_delayed_work(0, a, b, c)
#define ATH_QUEUE_WORK(a,b)      adf_os_queue_work(0, a, b)
#define ATH_QUEUE_DELAYED_WORK(a,b,c)   adf_os_queue_delayed_work(0, a, b, c)
#define ATH_FLUSH_WQUEUE(a)   adf_os_flush_workqueue(0, a)
#define ATH_DESTROY_WQUEUE(a)   adf_os_destroy_workqueue(0, a)

/*
 * Deduce if tasklets are available.  If not then
 * fall back to using the immediate work queue.
 */
#define tq_struct adf_os_bh_t
#define ATH_INIT_TQUEUE(a,b,c)      adf_os_create_bh(0, a, b, c)
#define ATH_SCHEDULE_TQUEUE(a,b)    adf_os_sched_bh(0, a)
typedef unsigned long TQUEUE_ARG;
#define mark_bh(a)

#define ATH_SYSCTL_DECL(f, ctl, write, filp, buffer, lenp, ppos) \
    f(ctl_table *ctl, int write, void *buffer,                   \
        size_t *lenp, loff_t *ppos)
#define ATH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer, lenp, ppos) \
    proc_dointvec(ctl, write, buffer, lenp, ppos)
#define ATH_SYSCTL_PROC_DOSTRING(ctl, write, filp, buffer, lenp, ppos) \
    proc_dostring(ctl, write, filp, buffer, lenp, ppos)

/*
 * Byte Order stuff
 */
#define    le16toh(_x)    le16_to_cpu(_x)
#define    htole16(_x)    cpu_to_le16(_x)
#define    htobe16(_x)    cpu_to_be16(_x)
#define    le32toh(_x)    le32_to_cpu(_x)
#define    htole32(_x)    cpu_to_le32(_x)
#define    be16toh(_x)    be16_to_cpu(_x)
#define    be32toh(_x)    be32_to_cpu(_x)
#define    htobe32(_x)    cpu_to_be32(_x)

#define EOK    (0)

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE  1
#endif

#define IP_PRI_SHIFT              5

#ifndef IPV6_VERSION_MASK
#define IPV6_VERSION_MASK           0xF0000000
#endif
#ifndef IPV6_PRIORITY_MASK
#define IPV6_PRIORITY_MASK          0x0FF00000
#endif
#ifndef IPV6_FLOWLABEL_MASK
#define IPV6_FLOWLABEL_MASK         0x000FFFFF
#endif
#ifndef IPV6_VERSION_SHIFT
#define IPV6_VERSION_SHIFT          28
#endif
#ifndef IPV6_PRIORITY_SHIFT
#define IPV6_PRIORITY_SHIFT         20
#endif
#ifndef IPV6_FLOWLABEL_SHIFT
#define IPV6_FLOWLABEL_SHIFT        0
#endif

#ifndef ARPHRD_IEEE80211
#define ARPHRD_IEEE80211 801		/* IEEE 802.11.  */
#endif

#define MAX_TX_RX_PACKET_SIZE     2500

static INLINE const char *
ether_sprintf(const uint8_t mac[6])
{
        static char buf[32];

        __adf_os_snprint(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return buf;
}

typedef unsigned int rwlock_state_t __attribute__((unused));

#define OS_ATOMIC_CMPXCHG(_Counter, _cmp, _xchg)   cmpxchg((int32_t*)(_Counter), _cmp, _xchg)

/*
 * Normal Delay functions. Time specified in microseconds.
 */
#define OS_DELAY(_us)            adf_os_udelay(_us)

#define OS_SLEEP(_us)            schedule_timeout_uninterruptible(usecs_to_jiffies(_us))

#define OS_MEMCPY(_dst, _src, _len)       adf_os_mem_copy(_dst, _src, _len)
#define OS_MEMMOVE(_dst, _src, _len)      adf_os_mem_move(_dst, _src, _len)
#define OS_MEMZERO(_buf, _len)            adf_os_mem_zero(_buf, _len)
#define OS_MEMSET(_buf, _ch, _len)        adf_os_mem_set(_buf, _ch, _len)
#define OS_MEMCMP(_mem1, _mem2, _len)     adf_os_mem_cmp(_mem1, _mem2, _len)

#define OS_SET_WDTIMEOUT(__sc, __timeo) \
{\
    ((__sc)->sc_osdev->netdev)->watchdog_timeo = (__timeo);\
}

#define OS_CLR_NETDEV_FLAG(__sc, __flag)\
{\
    ((__sc)->sc_osdev->netdev)->flags &= ~(__flag);\
}
#define OS_NETIF_WAKE_QUEUE(__sc)\
{\
    netif_wake_queue(((__sc)->sc_osdev->netdev));\
}
#define OS_NETIF_STOP_QUEUE(__sc)\
{\
    netif_stop_queue(((__sc)->sc_osdev->netdev));\
}
#define OS_NETDEV_UPDATE_TRANS(__sc)\
{\
    ((__sc)->sc_osdev->netdev)->trans_start = jiffies;\
}

/*
 * Locking interface for node
 */
#ifdef ATH_USB
typedef rwlock_t usb_readwrite_lock_t;
#endif

#define OS_RWLOCK_INIT(_rwl)        rwlock_init(_rwl)
#define OS_RWLOCK_DESTROY(_nt)

#ifdef CONFIG_SMP
/* Undo the one provided by the kernel to debug spin locks */
#undef spin_lock
#undef spin_unlock
#undef spin_trylock

#define spin_lock(x) \
do { \
 spin_lock_bh(x);\
} while (0)

#define spin_unlock(x) \
do { \
if (!spin_is_locked(x)) { \
WARN_ON(1); \
printk(KERN_EMERG " %s:%d unlock addr=%p, %s \n", __func__, __LINE__, x, \
!spin_is_locked(x)? "Not locked":""); \
} \
spin_unlock_bh(x);\
} while (0)

#define spin_trylock(x) spin_trylock_bh(x)

#define OS_SUPPORT_ASYNC_Q 1 /* support for handling asyn function calls */

#endif // ifdef CONFIG_SMP

#define OS_RWLOCK_READ_LOCK(rwl, lock_state)  do {\
    if (irqs_disabled() || in_irq()) {\
        read_lock(rwl);\
    } else {\
        read_lock_bh(rwl);\
    }\
} while (0)

#define OS_RWLOCK_WRITE_LOCK(rwl, lock_state)  do { \
    if (irqs_disabled() || in_irq()) {\
        write_lock(rwl);\
    } else {\
        write_lock_bh(rwl);\
    }\
} while(0)

#define OS_RWLOCK_READ_UNLOCK(rwl, lock_state)  do {\
    if (irqs_disabled() || in_irq()) {\
        read_unlock(rwl);\
    } else {\
        read_unlock_bh(rwl);\
    }\
} while (0)

#define OS_RWLOCK_WRITE_UNLOCK(rwl, lock_state) do {\
    if (irqs_disabled() || in_irq()) {\
        write_unlock(rwl);\
    } else {\
        write_unlock_bh(rwl);\
    }\
} while (0)

#define OS_RWLOCK_READ_LOCK_BH(rwl, lock_state) do { \
    read_lock_bh(rwl); \
} while (0)

#define OS_RWLOCK_WRITE_LOCK_BH(rwl, lock_state) do { \
    write_lock_bh(rwl); \
} while (0)

#define OS_RWLOCK_READ_UNLOCK_BH(rwl, lock_state) do { \
    read_unlock_bh(rwl); \
} while (0)

#define OS_RWLOCK_WRITE_UNLOCK_BH(rwl, lock_state) do { \
    write_unlock_bh(rwl); \
} while (0)

/* Irqsave/restore version */
#define OS_RWLOCK_READ_LOCK_IRQSAVE(rwl, lock_state, flags)  do { \
  read_lock_irqsave(rwl, flags); \
} while (0)

#define OS_RWLOCK_WRITE_LOCK_IRQSAVE(rwl, lock_state, flags) do { \
   write_lock_irqsave(rwl, flags); \
} while (0)

#define OS_RWLOCK_READ_UNLOCK_IRQRESTORE(rwl, lock_state, flags) do { \
   read_unlock_irqrestore(rwl, flags); \
} while (0)

#define OS_RWLOCK_WRITE_UNLOCK_IRQRESTORE(rwl, lock_state, flags) do { \
   write_unlock_irqrestore(rwl, flags); \
} while (0)


#ifndef OS_SUPPORT_ASYNC_Q
#define OS_SUPPORT_ASYNC_Q 0
#endif

/* Calculate the modulo with the following restrictions:
    lowest 10 bits of div is zero.
    div is at most 20 bits.

Theory for the math.
    Suppose x = A + B
    Then x % r = ((A % r) + B) % r

    For our case, we let A be the top 32-bits of x which is non-zero.
    Then x = (2^22 * a) + b.
*/
static INLINE u_int32_t OS_MOD64_TBTT_OFFSET(u_int64_t num64, u_int32_t div)
{

    u_int32_t remainder_last10bits = (u_int32_t)num64 & 0x003FF;

    /* Remove the low 10 bits. */
    div = div >> 10;
    num64 = num64 >> 10;

    {
        /* Get the mod of the top 32 bits */
        u_int32_t num_hi = (u_int32_t)(num64 >> 22);
#define BITS_22_MASK    0x003FFFFF
#define BITS_10_MASK    0x000003FF
        u_int32_t num_lo = (u_int32_t)(num64) & BITS_22_MASK;

        u_int32_t remainder_hi = num_hi % div;
        u_int32_t remainder_lo = num_lo;

        u_int32_t remainder32 = (remainder_hi << 22) + remainder_lo;

        remainder32 = remainder32 % div;
        /* Put back the last 10 bits */
        remainder32 = (remainder32 << 10) + remainder_last10bits;

        return(remainder32);
    }
}

/*
 * System time interface
 */

typedef adf_os_time_t systime_t;
typedef adf_os_time_t systick_t;

static INLINE adf_os_time_t
OS_GET_TIMESTAMP(void)
{
    return adf_os_ticks(); /* Fix double conversion from jiffies to ms */
}

static INLINE adf_os_time_t
OS_GET_TICKS(void)
{
    return adf_os_ticks();
}

#define CONVERT_SYSTEM_TIME_TO_MS(_t)        adf_os_ticks_to_msecs(_t)
#define CONVERT_SYSTEM_TIME_TO_SEC(_t)       (adf_os_ticks_to_msecs(_t) / 1000)
#define CONVERT_SEC_TO_SYSTEM_TIME(_t)       ((_t) * HZ)
#define CONVERT_MS_TO_SYSTEM_TIME(_t)        ((_t) * HZ/1000)

struct _NIC_DEV;

typedef struct _NIC_DEV * osdev_t;


typedef struct timer_list        os_timer_t;
typedef void (*timer_func)(void *);

typedef struct _os_mesg_t {
    STAILQ_ENTRY(_os_mesg_t)  mesg_next;
    u_int16_t                 mesg_type;
    u_int16_t                 mesg_len;
    /* followed by mesg_len bytes */
} os_mesg_t;

#define OS_ASYNC_Q_MAX_MESGS 4

#define OS_SCHEDULE_ROUTING_MESG_TYPE 1

typedef void* os_task_handle_t;
typedef void (*os_tasklet_routine_t)(
    void                *context,
    os_task_handle_t    task_handle
    );

typedef struct {
    os_tasklet_routine_t    routine;
    atomic_t                queued;
    void                    *data;
    spinlock_t              lock;
} os_task_t;

typedef struct _os_schedule_routing_mesg {
   os_tasklet_routine_t routine;
   void* context;
} os_schedule_routing_mesg ;

typedef union _os_async_q_mesg {
   os_schedule_routing_mesg s_mesg;
} os_async_q_mesg;


typedef void (*os_mesg_handler_t)(
    void                *ctx,
    u_int16_t           mesg_type,
    u_int16_t           mesg_len,
    void                *mesg
    );

typedef struct {
    osdev_t                     dev_handle;
    int32_t                     num_queued;
    int32_t                     mesg_len;
    u_int8_t                    *mesg_queue_buf;
    STAILQ_HEAD(, _os_mesg_t)    mesg_head;        /* queued mesg buffers */
    STAILQ_HEAD(, _os_mesg_t)    mesg_free_head;   /* free mesg buffers  */
    spinlock_t                  lock;
    spinlock_t                  ev_handler_lock;
#ifdef USE_SOFTINTR
    void                        *_task;
#else
    os_timer_t                  _timer;
#endif
    os_mesg_handler_t           handler;
    void                        *ctx;
    u_int8_t                    is_synchronous:1;
} os_mesg_queue_t;

struct ath_freebuf_bin {
    TAILQ_ENTRY(ath_freebuf_bin)    buf_list;
    unsigned char *head;
    unsigned char *end;
    struct sk_buff *skb;
    int lifetime;
};

typedef TAILQ_HEAD(ath_freebufhead_s, ath_freebuf_bin) ath_freebufhead;

struct rxbuf_recycle_ops {
    void    (*osdev_wbuf_collect)(void *, void *);
    void *  (*osdev_wbuf_recycle)(void *);
};
/*
 * Definition of OS-dependent device structure.
 * It'll be opaque to the actual ATH layer.
 */
struct _NIC_DEV {
    void                *bdev;      /* bus device handle */
    struct net_device   *netdev;    /* net device handle (wifi%d) */
    adf_os_bh_t         intr_tq;    /* tasklet */
    struct net_device_stats devstats;  /* net device statisitics */
    HAL_BUS_CONTEXT		bc;
#ifdef ATH_PERF_PWR_OFFLOAD
    struct device       *device;    /* generic device */
    wait_queue_head_t   event_queue;
#endif /* PERF_PWR_OFFLOAD */
#if OS_SUPPORT_ASYNC_Q
    os_mesg_queue_t     async_q;  /* mesgq to handle async calls */
#endif
#ifdef ATH_BUS_PM
    u_int8_t		    isDeviceAsleep;
#endif /* ATH_BUS_PM */
};

static INLINE unsigned char *
OS_MALLOC(osdev_t pNicDev, unsigned long ulSizeInBytes, int gfp)
{
    return adf_os_mem_alloc(NULL, ulSizeInBytes);
}
#define OS_FREE(_p)                     adf_os_mem_free(_p)

#define OS_MALLOC_WITH_TAG(_ppMem, _size, _tag)    do {   \
    *(_ppMem) = adf_os_mem_alloc(NULL, _size); \
} while(0)

#define OS_FREE_WITH_TAG(_pMem, _size)    adf_os_mem_free(_pMem)

static INLINE void *
OS_ALLOC_VAP(osdev_t osdev, u_int32_t len)
{
    void *netif;

    /*
     * In NDIS, we just allocate memory for interface object here.
     * For Linux, it may need to allocate and setup a net_device.
     */
    netif = adf_os_mem_alloc(NULL, len);
    if (netif != NULL)
    {
        adf_os_mem_zero(netif, len);
    }

    return netif;
}

static INLINE void
OS_FREE_VAP(void *netif)
{
    /* Just free the structure in NDIS.
     * In Linux, it should call unregister_netdevice
     * to free the memory */
    adf_os_mem_free(netif);
}

#define bus_type_error	0

#define BUS_DMA_FROMDEVICE     0
#define BUS_DMA_TODEVICE       1

#if bus_type_error
#error "No bus type is specified"
#endif

typedef dma_addr_t * dma_context_t;

#define OS_DMA_MEM_CONTEXT(context)         \
    dma_addr_t   context;

#define OS_GET_DMA_MEM_CONTEXT(var, field)  \
    &(var->field)

#define OS_COPY_DMA_MEM_CONTEXT(dst, src)   \
    *dst = *src

#define OS_ZERO_DMA_MEM_CONTEXT(context)   \
    *context = 0

#ifdef ATH_SUPPORT_SHARED_IRQ
#define		ATH_LOCK_IRQ(_osdev)		disable_irq(_osdev->netdev->irq)
#define		ATH_UNLOCK_IRQ(_osdev)		enable_irq(_osdev->netdev->irq)
#else
#define		ATH_LOCK_IRQ(_osdev)
#define		ATH_UNLOCK_IRQ(_osdev)
#endif

#ifdef ATH_SUPPORT_HTC
#define OS_EXEC_INTSAFE(_osdev, _fn, _arg) do {    \
    _fn(_arg);                                    \
} while (0)
#else /* #ifdef ATH_SUPPORT_HTC */
#define OS_EXEC_INTSAFE(_osdev, _fn, _arg) do {    \
    unsigned long flags;                        \
    ATH_LOCK_IRQ(_osdev);    \
    local_irq_save(flags);                        \
    _fn(_arg);                                    \
    local_irq_restore(flags);                    \
    ATH_UNLOCK_IRQ(_osdev);   \
} while (0)
#endif /* #ifdef ATH_SUPPORT_HTC */

/*
 * Timer Interfaces. Use these macros to declare timer
 * and retrieve timer argument. This is mainly for resolving
 * different argument types for timer function in different OS.
 */

#define OS_DECLARE_TIMER(_fn)                  void _fn(void *)

#define OS_TIMER_FUNC(_fn)                     \
    void _fn(void *timer_arg)

#define OS_GET_TIMER_ARG(_arg, _type)          \
    (_arg) = (_type)(timer_arg)


#define OS_INIT_TIMER(_osdev, _timer, _fn, _ctx, type)  adf_os_timer_init(_osdev, _timer, _fn, _ctx, type)

#define OS_SET_TIMER(_timer, _ms)      adf_os_timer_mod(_timer, _ms)

#define OS_CANCEL_TIMER(_timer)        adf_os_timer_cancel(_timer)

#define OS_FREE_TIMER(_timer)          adf_os_timer_cancel(_timer)

static INLINE void
OS_GET_RANDOM_BYTES(void *p, u_int16_t n)
{
    get_random_bytes(p, n);
}

typedef enum _mesgq_priority_t {
    MESGQ_PRIORITY_LOW,
    MESGQ_PRIORITY_NORMAL,
    MESGQ_PRIORITY_HIGH
} mesgq_priority_t;

typedef enum _mesgq_event_delivery_type {
    MESGQ_ASYNCHRONOUS_EVENT_DELIVERY,
    MESGQ_SYNCHRONOUS_EVENT_DELIVERY,
} mesgq_event_delivery_type;




/*
 * OS_MESGQ_* API to deliver messages(events) asynchronosly.
 * messages are queued up into a queue and are delivered in the context of
 * timer thread. this will avoid reentrency issues across different
 * module boundaries.
 */

static INLINE void
os_mesgq_handler(void *timer_arg)
{
    os_mesg_queue_t    *queue = (os_mesg_queue_t*)timer_arg;
    os_mesg_t          *mesg = NULL;
    void               *msg;

    /*
     * Request access to message queue to retrieve message for processing
     */
    spin_lock(&(queue->lock));

    mesg = STAILQ_FIRST(&queue->mesg_head);
    while(mesg) {
        STAILQ_REMOVE_HEAD(&queue->mesg_head, mesg_next);
        if (mesg->mesg_len) {
            msg =  (void *) (mesg+1);
        } else {
            msg = NULL;
        }
        /*
         * Release access to message queue before processing message
         */
        spin_unlock(&(queue->lock));

        /*
         * Ensure just one message can be processes at a time.
         */
        spin_lock(&(queue->ev_handler_lock));
        queue->handler(queue->ctx,mesg->mesg_type,mesg->mesg_len, msg);
        spin_unlock(&(queue->ev_handler_lock));

        /*
         * Request access to message queue to retrieve next message
         */
        spin_lock(&(queue->lock));
        queue->num_queued--;
        STAILQ_INSERT_TAIL(&queue->mesg_free_head,mesg, mesg_next);
        mesg = STAILQ_FIRST(&queue->mesg_head);
    }

    /*
     * Release message queue
     */
    spin_unlock(&(queue->lock));
}

/*
 * initialize message queue.
 * devhandle   : os dev handle.
 * queue       : message queue.
 * mesg_len    : maximum length of message.
 * max_queued  : maximum number of messages that can be queued at any time.
 * msg_handler : handler function which will be called
 *                asynchronously to deliver each message.
 */
static INLINE int OS_MESGQ_INIT(osdev_t devhandle, os_mesg_queue_t *queue,
                                u_int32_t mesg_len, u_int32_t max_queued,
                                os_mesg_handler_t msg_handler, void *context,
                                mesgq_priority_t priority,
                                mesgq_event_delivery_type mq_type)
{
    int i,len;
    os_mesg_t *mesg;

    len = (mesg_len + sizeof(struct _os_mesg_t));
    queue->mesg_queue_buf = adf_os_mem_alloc(NULL, (adf_os_size_t) len*max_queued);
    if (!queue->mesg_queue_buf)
        return -ENOMEM;
    queue->dev_handle = devhandle;
    STAILQ_INIT(&queue->mesg_head);
    STAILQ_INIT(&queue->mesg_free_head);
    spin_lock_init(&(queue->lock));
    spin_lock_init(&(queue->ev_handler_lock));
    mesg = (os_mesg_t *)queue->mesg_queue_buf;
    for (i=0;i<max_queued;++i) {
        STAILQ_INSERT_TAIL(&queue->mesg_free_head,mesg,mesg_next);
        mesg = (os_mesg_t *) ((u_int8_t *) mesg + len);
    }
    queue->mesg_len = mesg_len;
    queue->ctx = context;
    queue->handler = msg_handler;
    queue->num_queued = 0;
    if (mq_type == MESGQ_ASYNCHRONOUS_EVENT_DELIVERY) {
        queue->is_synchronous=0;
    } else {
        queue->is_synchronous=1;
    }
#ifdef USE_SOFTINTR
	queue->_task = softintr_establish(IPL_SOFTNET,os_mesgq_handler,(void *)queue);
#else
    OS_INIT_TIMER(devhandle,&queue->_timer, os_mesgq_handler, queue,
       ADF_DEFERRABLE_TIMER);
#endif

    return 0;
}

/*
 * send a message.
 * queue : message queue.
 * msg   : message (opaque) . the size of the message
 *         is equal to the mesg_length passed to the OS_MESG_INIT
 *
 */
static INLINE int OS_MESGQ_SEND(os_mesg_queue_t *queue,u_int16_t type, u_int16_t len,  void *msg)
{
    os_mesg_t *mesg;

    spin_lock(&(queue->lock));
    if (queue->is_synchronous ) {
        queue->handler(queue->ctx,type,len, msg);
    } else {
        mesg = STAILQ_FIRST(&queue->mesg_free_head);
        KASSERT(len <= queue->mesg_len, ("len <= queue->mesg_len"));
        if (mesg) {
            STAILQ_REMOVE_HEAD(&queue->mesg_free_head, mesg_next);
            mesg->mesg_type = type;
            mesg->mesg_len = len;
            if (len) {
                OS_MEMCPY((u_int8_t *)(mesg+1),msg,len);
            }
            STAILQ_INSERT_TAIL(&queue->mesg_head, mesg, mesg_next);
            queue->num_queued++;
        } else {
            spin_unlock(&(queue->lock));
            printk("No more message queue buffers !!! \n");
            return -ENOMEM;
        }
        if (queue->num_queued == 1) {
            /* schedule a task (timer) to handle the messages */
#ifdef USE_SOFTINTR
            softintr_schedule(queue->_task);
#else
            OS_SET_TIMER(&queue->_timer,0);
#endif
        }
    }
    spin_unlock(&(queue->lock));
    return 0;
}

/*
 * this is only for single threaded operating systems.
 * assert for now.
 */
static INLINE int OS_MESGQ_SEND_SYNC(os_mesg_queue_t *queue,u_int16_t type, u_int16_t len,  void *msg, bool flush)
{
    KASSERT(0,(" mesg queue sync send is not supported by linux"));
    return 0;
}

static INLINE int OS_MESGQ_CAN_SEND_SYNC(void)
{
    return TRUE;
}

/*
 * drain all the messages.
 * queue : message queue.
 */
static INLINE void OS_MESGQ_DRAIN(os_mesg_queue_t *queue, os_mesg_handler_t msg_handler)
{
    os_mesg_t *mesg = NULL;
    void *msg;

    spin_lock(&(queue->lock));
#ifndef USE_SOFTINTR
    OS_CANCEL_TIMER(&queue->_timer);
#endif
    mesg = STAILQ_FIRST(&queue->mesg_head);
    while(mesg) {
        STAILQ_REMOVE_HEAD(&queue->mesg_head, mesg_next);
        queue->num_queued--;
        if (msg_handler != NULL) {
            if (mesg->mesg_len) {
                msg = (void *) (mesg+1);
            } else {
                msg = NULL;
            }
            msg_handler(queue->ctx, mesg->mesg_type, mesg->mesg_len, msg);
        }
        STAILQ_INSERT_TAIL(&queue->mesg_free_head,mesg, mesg_next);
        mesg = STAILQ_FIRST(&queue->mesg_head);
    };
    STAILQ_INIT(&queue->mesg_head);
    spin_unlock(&(queue->lock));
}


/*
 * destroy the message queue.
 * queue : message queue.
 * reclaim all the resorces.
 */

static INLINE void OS_MESGQ_DESTROY(os_mesg_queue_t *queue)
{
    spin_lock(&(queue->lock));
#ifdef USE_SOFTINTR
    softintr_disestablish(queue->_task);
#else
    OS_CANCEL_TIMER(&queue->_timer);
#endif
    queue->num_queued = 0;
    STAILQ_INIT(&queue->mesg_head);
    STAILQ_INIT(&queue->mesg_free_head);
    adf_os_mem_free(queue->mesg_queue_buf);
#ifndef USE_SOFTINTR
    OS_FREE_TIMER(&queue->_timer);
#endif
    spin_unlock(&(queue->lock));
    spin_lock_destroy(&(queue->lock));
    spin_lock_destroy(&(queue->ev_handler_lock));
}



/*
 * temp WAR for windows hang (dead lock). It can be removed when VAP SM is re-written (bug 65137).
 */
static INLINE int
OS_SCHEDULE_ROUTING(osdev_t pNicDev,
                     os_tasklet_routine_t routine,
                     void* context)
{
#if OS_SUPPORT_ASYNC_Q
   os_schedule_routing_mesg  s_mesg ;
   s_mesg.routine = routine;
   s_mesg.context = context;
   OS_MESGQ_SEND(&pNicDev->async_q,OS_SCHEDULE_ROUTING_MESG_TYPE, sizeof(os_schedule_routing_mesg), &s_mesg);

#else
    routine(context, NULL);
#endif
    return 0;
}

static INLINE void
OS_FREE_ROUTING(void* workItemHandle)
{
}

/*
** These are required for network manager support
*/

#ifndef SET_NETDEV_DEV
#define    SET_NETDEV_DEV(ndev, pdev)
#endif

#ifdef to_net_dev
#define ATH_GET_NETDEV_DEV(ndev)    ((ndev)->dev.parent)
#else
#define ATH_GET_NETDEV_DEV(ndev)    ((ndev)->class_dev.dev)
#endif



/*
 * Opaque S/G List Entry
 */
typedef struct scatterlist            sg_t;

#include "hwdef.h"

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a)         (sizeof(a) / sizeof((a)[0]))
#endif

#define MIN(a, b)                adf_os_min(a, b)
#define MAX(a, b)                adf_os_max(a, b)

/*
 * PCI configuration space access
 */
#ifdef ATH_PCI

static INLINE u_int32_t
OS_PCI_READ_CONFIG(osdev_t osdev, u_int32_t offset, void *p, u_int32_t bytes)
{
    struct pci_dev *pdev = (struct pci_dev *)osdev->bdev;

    switch (bytes) {
    case 1:
        pci_read_config_byte(pdev, offset, p);
        break;
    case 2:
        pci_read_config_word(pdev, offset, p);
        break;
    case 4:
        pci_read_config_dword(pdev, offset, p);
        break;
    }
    return bytes;
}

static INLINE void
OS_PCI_WRITE_CONFIG(osdev_t osdev, u_int32_t offset, void *p, u_int32_t bytes)
{
    struct pci_dev *pdev = (struct pci_dev *)osdev->bdev;

    switch (bytes) {
    case 1:
        pci_write_config_byte(pdev, offset, *(u_int8_t *)p);
        break;
    case 2:
        pci_write_config_word(pdev, offset, *(u_int16_t *)p);
        break;
    case 4:
        pci_write_config_dword(pdev, offset, *(u_int32_t *)p);
        break;
    }
}

#else

static INLINE u_int32_t
OS_PCI_READ_CONFIG(osdev_t osdev, u_int32_t offset, void *p, u_int32_t bytes)
{
    OS_MEMSET(p, 0xff, bytes);
    return 0;
}

#define OS_PCI_WRITE_CONFIG(_osdev, _offset, _p, _bytes)

#endif

void *OS_ALLOC_VAP(osdev_t dev, u_int32_t len);
void OS_FREE_VAP(void *netif);

// ALLOC_DMA_MAP_CONTEXT_AREA is a NULL macro and is implemented only for BSD.
#define ALLOC_DMA_MAP_CONTEXT_AREA(os_handle, p_memctx)
#define FREE_DMA_CONTEXT_POOL(os_handle, name)
#define ALLOC_DMA_CONTEXT_POOL(os_handle, name, numdesc)

#define ATH_QOSNULL_TXDESC    64
#define ATH_FRAG_PER_MSDU   1
#ifndef ATH_TXBUF
#define ATH_TXBUF   512/ATH_FRAG_PER_MSDU
#endif

/*
 * minimum h/w qdepth to be sustained to maximize aggregation
 */
#define ATH_AGGR_MIN_QDEPTH 2
#define OS_MAX_RXBUF_SIZE(_statuslen)   (IEEE80211_MAX_MPDU_LEN + _statuslen)

#define ATH_GET_RX_CONTEXT_BUF(_wbuf)  \
                          (ATH_RX_CONTEXT(_wbuf)->ctx_rxbuf)
#define ATH_SET_RX_CONTEXT_BUF(_wbuf, _bf)  \
                          (ATH_GET_RX_CONTEXT_BUF(_wbuf) = _bf)

// This macro is used to avoid another wrapper around ath_rxbuf_alloc.
// For Mac OS, we need to OR in ATH_RXBUF_ALLOC_DONTWAIT with length.
// Not needed for other OS'.
#define ATH_ALLOCATE_RXBUFFER(_sc, _len)   ath_rxbuf_alloc(_sc, _len)

#ifndef OS_EXPORT_SYMBOL
#define OS_EXPORT_SYMBOL(_sym) EXPORT_SYMBOL(_sym)
#endif

struct ieee80211_node;
struct ieee80211_cb {
    u_int8_t        vlan[8];    /* reserve for vlan tag info */
    struct ieee80211_node   *ni;
    u_int32_t       flags;
#define N_LINK0     0x01            /* frame needs WEP encryption */
#define N_FF        0x02            /* fast frame */
#define N_PWR_SAV   0x04            /* bypass power save handling */
#define N_UAPSD     0x08            /* frame flagged for u-apsd handling */
#define N_EAPOL     0x10            /* frame flagged for EAPOL handling */
#define N_AMSDU     0x20            /* frame flagged for AMSDU handling */
#define N_NULL_PWR_SAV  0x40            /* null data with power save bit on */
#define N_PROBING   0x80            /* frame flagged as a probing one */
#define N_ERROR         0x100                   /* frame flagged as a error one */
#define N_MOREDATA      0x200                   /* more data flag */
#define N_SMPSACTM      0x400                   /* This frame is SM power save Action Mgmt frame */
#define N_QOS           0x800                   /* This is a QOS frame*/
#define N_ENCAP_DONE    0x1000              /* This frame is marked as fast-path pkts, some encapsulation work has been done by h/w */
#define N_CLONED        0x2000      /* frame is cloned in rx path */
#define N_ANT_TRAIN     0x8000      /* frame is smart antenna training packet */
#define N_ANT_TRAIN_LAST        0x20000		/* Last smart antenna training packet */
#ifdef ATH_SUPPORT_WAPI
#define N_WAI                   0x40000                  /* frame flagged for WAPI handling */
#endif
#define N_FMSS                   0x100000     /* frame needs FMS handling */

    u_int8_t        u_tid;      /* user priority from vlan/ip tos   */
        u_int8_t                exemptiontype;  /* exemption type of this frame (0,1,2)*/
        u_int8_t                type;           /* type of this frame */

        union {
            void *context;  /* pointer to context area */
        }_u;

#if defined(ATH_SUPPORT_P2P) || defined(ATH_SUPPORT_TDLS)
        void            *complete_handler;     /* complete handler */
        void            *complete_handler_arg; /* complete handler arg */
#endif
};
//#endif /* CONVERGED_SW */

#ifndef ATH_SUPPORT_HTC
/*
 * For packet capture, define the same physical layer packet header
 * structure as used in the wlan-ng driver
 */
typedef struct {
    u_int32_t did;
    u_int16_t status;
    u_int16_t len;
    u_int32_t data;
} p80211item_uint32_t;

typedef struct {
    u_int32_t msgcode;
    u_int32_t msglen;
#define WLAN_DEVNAMELEN_MAX 16
    u_int8_t devname[WLAN_DEVNAMELEN_MAX];
    p80211item_uint32_t hosttime;
    p80211item_uint32_t mactime;
    p80211item_uint32_t channel;
    p80211item_uint32_t rssi;
    p80211item_uint32_t sq;
    p80211item_uint32_t signal;
    p80211item_uint32_t noise;
    p80211item_uint32_t rate;
    p80211item_uint32_t istx;
    p80211item_uint32_t frmlen;
} wlan_ng_prism2_header;

#endif

#define N_FLAG_SET(_nbf, _flag) \
        (((struct ieee80211_cb *)(_nbf)->cb)->flags |= (_flag))
#define N_FLAG_CLR(_nbf, _flag) \
        (((struct ieee80211_cb *)(_nbf)->cb)->flags &= ~(_flag))
#define N_FLAG_GET(_nbf, _flag) \
        (((struct ieee80211_cb *)(_nbf)->cb)->flags & (_flag))
#define N_FLAG_IS(_nbf, _flag) \
        ((((struct ieee80211_cb *)(_nbf)->cb)->flags & (_flag)) == (_flag))
#define N_FLAG_KEEP_ONLY(_nbf, _flag) \
        (((struct ieee80211_cb *)(_nbf)->cb)->flags &= (_flag))

#define N_PWR_SAV_SET(nbf) N_FLAG_SET((nbf), N_PWR_SAV)
#define N_PWR_SAV_CLR(nbf) N_FLAG_CLR((nbf), N_PWR_SAV)
#define N_PWR_SAV_GET(nbf) N_FLAG_GET((nbf), N_PWR_SAV)
#define N_PWR_SAV_IS(nbf)  N_FLAG_IS((nbf), N_PWR_SAV)

#define N_NULL_PWR_SAV_SET(nbf) N_FLAG_SET((nbf), N_NULL_PWR_SAV)
#define N_NULL_PWR_SAV_CLR(nbf) N_FLAG_CLR((nbf), N_NULL_PWR_SAV)
#define N_NULL_PWR_SAV_GET(nbf) N_FLAG_GET((nbf), N_NULL_PWR_SAV)
#define N_NULL_PWR_SAV_IS(nbf)  N_FLAG_IS((nbf), N_NULL_PWR_SAV)

#define N_PROBING_SET(nbf) N_FLAG_SET((nbf), N_PROBING)
#define N_PROBING_CLR(nbf) N_FLAG_CLR((nbf), N_PROBING)
#define N_PROBING_GET(nbf) N_FLAG_GET((nbf), N_PROBING)
#define N_PROBING_IS(nbf)  N_FLAG_IS((nbf), N_PROBING)

#define N_CLONED_SET(nbf) N_FLAG_SET((nbf), N_CLONED)
#define N_CLONED_CLR(nbf) N_FLAG_CLR((nbf), N_CLONED)
#define N_CLONED_GET(nbf) N_FLAG_GET((nbf), N_CLONED)
#define N_CLONED_IS(nbf)  N_FLAG_IS((nbf), N_CLONED)

#define N_MOREDATA_SET(nbf) N_FLAG_SET((nbf), N_MOREDATA)
#define N_MOREDATA_CLR(nbf) N_FLAG_CLR((nbf), N_MOREDATA)
#define N_MOREDATA_GET(nbf) N_FLAG_GET((nbf), N_MOREDATA)
#define N_MOREDATA_IS(nbf)  N_FLAG_IS((nbf), N_MOREDATA)

#define N_SMPSACTM_SET(nbf) N_FLAG_SET((nbf), N_SMPSACTM)
#define N_SMPSACTM_CLR(nbf) N_FLAG_CLR((nbf), N_SMPSACTM)
#define N_SMPSACTM_GET(nbf) N_FLAG_GET((nbf), N_SMPSACTM)
#define N_SMPSACTM_IS(nbf)  N_FLAG_IS((nbf), N_SMPSACTM)

#define N_QOS_SET(nbf)      N_FLAG_SET((nbf), N_QOS)
#define N_QOS_CLR(nbf)      N_FLAG_CLR((nbf), N_QOS)
#define N_QOS_GET(nbf)      N_FLAG_GET((nbf), N_QOS)
#define N_QOS_IS(nbf)       N_FLAG_IS((nbf), N_QOS)

#define N_EAPOL_SET(nbf)    N_FLAG_SET((nbf), N_EAPOL)
#define N_EAPOL_IS(nbf)     N_FLAG_IS((nbf), N_EAPOL)

#define N_AMSDU_SET(nbf)    N_FLAG_SET((nbf), N_AMSDU)
#define N_AMSDU_IS(nbf)     N_FLAG_IS((nbf), N_AMSDU)

#define N_FF_SET(nbf)       N_FLAG_SET((nbf), N_FF)
#define N_FF_IS(nbf)        N_FLAG_IS((nbf), N_FF)

#define N_UAPSD_SET(nbf)    N_FLAG_SET((nbf), N_UAPSD)
#define N_UAPSD_CLR(nbf)    N_FLAG_CLR((nbf), N_UAPSD)
#define N_UAPSD_IS(nbf)     N_FLAG_IS((nbf), N_UAPSD)

#define N_ENCAP_DONE_IS(nbf)    N_FLAG_IS((nbf), N_ENCAP_DONE)
#define N_ENCAP_DONE_SET(nbf)   N_FLAG_SET((nbf), N_ENCAP_DONE)
#define N_ENCAP_DONE_CLR(nbf)   N_FLAG_CLR((nbf), N_ENCAP_DONE)

#define N_STATUS_SET(nbf, _status)

#define N_STATUS_GET(nbf)              \
        if(N_FLAG_IS((nbf), N_ERROR))  \
            return WB_STATUS_TX_ERROR; \
        else                           \
            return WB_STATUS_OK;

#define N_CONTEXT_SET(_nbf, _context)   \
        (((struct ieee80211_cb *)(_nbf)->cb)->_u.context = (_context))
#define N_CONTEXT_GET(_nbf)             \
        (((struct ieee80211_cb *)(_nbf)->cb)->_u.context)

#define N_TYPE_SET(_nbf, _type) \
        (((struct ieee80211_cb *)(_nbf)->cb)->type = (_type))
#define N_TYPE_GET(_nbf) \
        (((struct ieee80211_cb *)(_nbf)->cb)->type)

#define N_NODE_SET(_nbf, _ni) \
        (((struct ieee80211_cb *)(_nbf)->cb)->ni = (_ni))
#define N_NODE_GET(_nbf) \
        (((struct ieee80211_cb *)(_nbf)->cb)->ni)

#define N_COMPLETE_HANDLER_SET(_nbf, _handler) \
        (((struct ieee80211_cb *)(_nbf)->cb)->complete_handler = (_handler))
#define N_COMPLETE_HANDLER_ARG_SET(_nbf, _arg) \
        (((struct ieee80211_cb *)(_nbf)->cb)->complete_handler_arg = (_arg))

#define N_COMPLETE_HANDLER_GET(_nbf)    \
        (((struct ieee80211_cb *)(_nbf)->cb)->complete_handler)
#define N_COMPLETE_HANDLER_ARG_GET(_nbf)    \
        (((struct ieee80211_cb *)(_nbf)->cb)->complete_handler_arg)

#define N_EXMTYPE_SET(_nbf, _type) \
        (((struct ieee80211_cb *)(_nbf)->cb)->exemptiontype = (_type))
#define N_EXMTYPE_GET(_nbf) \
        (((struct ieee80211_cb *)(_nbf)->cb)->exemptiontype)

#define N_TID_SET(_nbf, _tid) \
        (((struct ieee80211_cb *)(_nbf)->cb)->u_tid = (_tid))
#define N_TID_GET(_nbf) \
        (((struct ieee80211_cb *)(_nbf)->cb)->u_tid)

#define N_ANT_GET(_nbf) \
        (((struct ieee80211_cb *)(_nbf)->cb)->smart_antenna)
#define N_RATE_INDEX_GET(_nbf) \
                (((struct ieee80211_cb *)(_nbf)->cb)->rateIndex)
#define N_TRAIN_PKT_IS(nbf)  N_FLAG_IS((nbf), N_ANT_TRAIN)
#define N_TRAIN_LASTPKT_IS(nbf)  N_FLAG_IS((nbf), N_ANT_TRAIN_LAST)

#define N_ANT_SET(_nbf, _antenna) \
        (((struct ieee80211_cb *)(_nbf)->cb)->smart_antenna = (_antenna))
#define N_RATE_INDEX_SET(_nbf, _idx) \
                (((struct ieee80211_cb *)(_nbf)->cb)->rateIndex = (_idx))
#define N_TRAIN_PKT_SET(nbf)  N_FLAG_SET((nbf), N_ANT_TRAIN)
#define N_TRAIN_PKT_UNSET(nbf)  N_FLAG_CLR((nbf), N_ANT_TRAIN)
#define N_TRAIN_LASTPKT_SET(nbf)  N_FLAG_SET((nbf), N_ANT_TRAIN_LAST)

/*
 * nbufs on the power save queue are tagged with an age and
 * timed out.  We reuse the hardware checksum field in the
 * nbuf packet header to store this data.
 * XXX use private cb area
 */
#define N_AGE_SET(skb, v)   (skb)->csum = (v)
#define N_AGE_GET(skb)      (skb)->csum
#define N_AGE_SUB(skb, adj) (skb)->csum -= (adj)


#endif /* end of _ATH_LINUX_OSDEP_ADF_H */
