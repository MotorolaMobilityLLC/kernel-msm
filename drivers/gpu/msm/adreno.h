/* Copyright (c) 2008-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __ADRENO_H
#define __ADRENO_H

#include "kgsl_device.h"
#include "adreno_drawctxt.h"
#include "adreno_ringbuffer.h"
#include "kgsl_iommu.h"
#include <mach/ocmem.h>

#define DEVICE_3D_NAME "kgsl-3d"
#define DEVICE_3D0_NAME "kgsl-3d0"

#define ADRENO_DEVICE(device) \
		KGSL_CONTAINER_OF(device, struct adreno_device, dev)

#define ADRENO_CONTEXT(device) \
		KGSL_CONTAINER_OF(device, struct adreno_context, base)

#define ADRENO_CHIPID_CORE(_id) (((_id) >> 24) & 0xFF)
#define ADRENO_CHIPID_MAJOR(_id) (((_id) >> 16) & 0xFF)
#define ADRENO_CHIPID_MINOR(_id) (((_id) >> 8) & 0xFF)
#define ADRENO_CHIPID_PATCH(_id) ((_id) & 0xFF)

/* Flags to control command packet settings */
#define KGSL_CMD_FLAGS_NONE             0
#define KGSL_CMD_FLAGS_PMODE		BIT(0)
#define KGSL_CMD_FLAGS_INTERNAL_ISSUE   BIT(1)
#define KGSL_CMD_FLAGS_GET_INT		BIT(2)
#define KGSL_CMD_FLAGS_WFI              BIT(3)

/* Command identifiers */
#define KGSL_CONTEXT_TO_MEM_IDENTIFIER	0x2EADBEEF
#define KGSL_CMD_IDENTIFIER		0x2EEDFACE
#define KGSL_CMD_INTERNAL_IDENTIFIER	0x2EEDD00D
#define KGSL_START_OF_IB_IDENTIFIER	0x2EADEABE
#define KGSL_END_OF_IB_IDENTIFIER	0x2ABEDEAD
#define KGSL_END_OF_FRAME_IDENTIFIER	0x2E0F2E0F
#define KGSL_NOP_IB_IDENTIFIER	        0x20F20F20

#ifdef CONFIG_MSM_SCM
#define ADRENO_DEFAULT_PWRSCALE_POLICY  (&kgsl_pwrscale_policy_tz)
#elif defined CONFIG_MSM_SLEEP_STATS_DEVICE
#define ADRENO_DEFAULT_PWRSCALE_POLICY  (&kgsl_pwrscale_policy_idlestats)
#else
#define ADRENO_DEFAULT_PWRSCALE_POLICY  NULL
#endif

void adreno_debugfs_init(struct kgsl_device *device);

#define ADRENO_ISTORE_START 0x5000 /* Istore offset */

#define ADRENO_NUM_CTX_SWITCH_ALLOWED_BEFORE_DRAW	50

/* One cannot wait forever for the core to idle, so set an upper limit to the
 * amount of time to wait for the core to go idle
 */

#define ADRENO_IDLE_TIMEOUT (20 * 1000)

enum adreno_gpurev {
	ADRENO_REV_UNKNOWN = 0,
	ADRENO_REV_A200 = 200,
	ADRENO_REV_A203 = 203,
	ADRENO_REV_A205 = 205,
	ADRENO_REV_A220 = 220,
	ADRENO_REV_A225 = 225,
	ADRENO_REV_A305 = 305,
	ADRENO_REV_A320 = 320,
	ADRENO_REV_A330 = 330,
};

/*
 * Maximum size of the dispatcher ringbuffer - the actual inflight size will be
 * smaller then this but this size will allow for a larger range of inflight
 * sizes that can be chosen at runtime
 */

#define ADRENO_DISPATCH_CMDQUEUE_SIZE 128

/**
 * struct adreno_dispatcher - container for the adreno GPU dispatcher
 * @mutex: Mutex to protect the structure
 * @state: Current state of the dispatcher (active or paused)
 * @timer: Timer to monitor the progress of the command batches
 * @inflight: Number of command batch operations pending in the ringbuffer
 * @fault: True if a HW fault was detected
 * @pending: Priority list of contexts waiting to submit command batches
 * @plist_lock: Spin lock to protect the pending queue
 * @cmdqueue: Queue of command batches currently flight
 * @head: pointer to the head of of the cmdqueue.  This is the oldest pending
 * operation
 * @tail: pointer to the tail of the cmdqueue.  This is the most recently
 * submitted operation
 * @work: work_struct to put the dispatcher in a work queue
 * @kobj: kobject for the dispatcher directory in the device sysfs node
 */
struct adreno_dispatcher {
	struct mutex mutex;
	unsigned int state;
	struct timer_list timer;
	struct timer_list fault_timer;
	unsigned int inflight;
	int fault;
	struct plist_head pending;
	spinlock_t plist_lock;
	struct kgsl_cmdbatch *cmdqueue[ADRENO_DISPATCH_CMDQUEUE_SIZE];
	unsigned int head;
	unsigned int tail;
	struct work_struct work;
	struct kobject kobj;
};

struct adreno_gpudev;

struct adreno_device {
	struct kgsl_device dev;    /* Must be first field in this struct */
	unsigned int chip_id;
	enum adreno_gpurev gpurev;
	unsigned long gmem_base;
	unsigned int gmem_size;
	struct adreno_context *drawctxt_active;
	const char *pfp_fwfile;
	unsigned int *pfp_fw;
	size_t pfp_fw_size;
	unsigned int pfp_fw_version;
	const char *pm4_fwfile;
	unsigned int *pm4_fw;
	size_t pm4_fw_size;
	unsigned int pm4_fw_version;
	struct adreno_ringbuffer ringbuffer;
	unsigned int mharb;
	struct adreno_gpudev *gpudev;
	unsigned int wait_timeout;
	unsigned int istore_size;
	unsigned int pix_shader_start;
	unsigned int instruction_size;
	unsigned int ib_check_level;
	unsigned int fast_hang_detect;
	unsigned int ft_policy;
	unsigned int long_ib_detect;
	unsigned int long_ib;
	unsigned int long_ib_ts;
	unsigned int ft_pf_policy;
	unsigned int gpulist_index;
	struct ocmem_buf *ocmem_hdl;
	unsigned int ocmem_base;
	unsigned int gpu_cycles;
	struct adreno_dispatcher dispatcher;
};

#define PERFCOUNTER_FLAG_NONE 0x0
#define PERFCOUNTER_FLAG_KERNEL 0x1

/* Structs to maintain the list of active performance counters */

/**
 * struct adreno_perfcount_register: register state
 * @countable: countable the register holds
 * @refcount: number of users of the register
 * @offset: register hardware offset
 */
struct adreno_perfcount_register {
	unsigned int countable;
	unsigned int refcount;
	unsigned int offset;
	unsigned int flags;
};

/**
 * struct adreno_perfcount_group: registers for a hardware group
 * @regs: available registers for this group
 * @reg_count: total registers for this group
 */
struct adreno_perfcount_group {
	struct adreno_perfcount_register *regs;
	unsigned int reg_count;
};

/**
 * adreno_perfcounts: all available perfcounter groups
 * @groups: available groups for this device
 * @group_count: total groups for this device
 */
struct adreno_perfcounters {
	struct adreno_perfcount_group *groups;
	unsigned int group_count;
};

struct adreno_gpudev {
	/*
	 * These registers are in a different location on A3XX,  so define
	 * them in the structure and use them as variables.
	 */
	unsigned int reg_rbbm_status;
	unsigned int reg_cp_pfp_ucode_data;
	unsigned int reg_cp_pfp_ucode_addr;
	/* keeps track of when we need to execute the draw workaround code */
	int ctx_switches_since_last_draw;

	struct adreno_perfcounters *perfcounters;

	/* GPU specific function hooks */
	int (*ctxt_create)(struct adreno_device *, struct adreno_context *);
	int (*ctxt_save)(struct adreno_device *, struct adreno_context *);
	int (*ctxt_restore)(struct adreno_device *, struct adreno_context *);
	int (*ctxt_draw_workaround)(struct adreno_device *,
					struct adreno_context *);
	irqreturn_t (*irq_handler)(struct adreno_device *);
	void (*irq_control)(struct adreno_device *, int);
	unsigned int (*irq_pending)(struct adreno_device *);
	void * (*snapshot)(struct adreno_device *, void *, int *, int);
	int (*rb_init)(struct adreno_device *, struct adreno_ringbuffer *);
	void (*perfcounter_init)(struct adreno_device *);
	void (*start)(struct adreno_device *);
	unsigned int (*busy_cycles)(struct adreno_device *);
	void (*perfcounter_enable)(struct adreno_device *, unsigned int group,
		unsigned int counter, unsigned int countable);
	uint64_t (*perfcounter_read)(struct adreno_device *adreno_dev,
		unsigned int group, unsigned int counter,
		unsigned int offset);
};

#define FT_DETECT_REGS_COUNT 12

/* Fault Tolerance policy flags */
#define  KGSL_FT_OFF                      BIT(0)
#define  KGSL_FT_REPLAY                   BIT(1)
#define  KGSL_FT_SKIPIB                   BIT(2)
#define  KGSL_FT_SKIPFRAME                BIT(3)
#define  KGSL_FT_DISABLE                  BIT(4)
#define  KGSL_FT_TEMP_DISABLE             BIT(5)
#define  KGSL_FT_DEFAULT_POLICY           (KGSL_FT_REPLAY + KGSL_FT_SKIPIB)

/* This internal bit is used to skip the PM dump on replayed command batches */
#define  KGSL_FT_SKIP_PMDUMP              BIT(31)

/* Pagefault policy flags */
#define KGSL_FT_PAGEFAULT_INT_ENABLE         BIT(0)
#define KGSL_FT_PAGEFAULT_GPUHALT_ENABLE     BIT(1)
#define KGSL_FT_PAGEFAULT_LOG_ONE_PER_PAGE   BIT(2)
#define KGSL_FT_PAGEFAULT_LOG_ONE_PER_INT    BIT(3)
#define KGSL_FT_PAGEFAULT_DEFAULT_POLICY     (KGSL_FT_PAGEFAULT_INT_ENABLE + \
					KGSL_FT_PAGEFAULT_GPUHALT_ENABLE)

extern struct adreno_gpudev adreno_a2xx_gpudev;
extern struct adreno_gpudev adreno_a3xx_gpudev;

/* A2XX register sets defined in adreno_a2xx.c */
extern const unsigned int a200_registers[];
extern const unsigned int a220_registers[];
extern const unsigned int a225_registers[];
extern const unsigned int a200_registers_count;
extern const unsigned int a220_registers_count;
extern const unsigned int a225_registers_count;

/* A3XX register set defined in adreno_a3xx.c */
extern const unsigned int a3xx_registers[];
extern const unsigned int a3xx_registers_count;

extern const unsigned int a3xx_hlsq_registers[];
extern const unsigned int a3xx_hlsq_registers_count;

extern const unsigned int a330_registers[];
extern const unsigned int a330_registers_count;

extern unsigned int ft_detect_regs[];


int adreno_idle(struct kgsl_device *device);
void adreno_regread(struct kgsl_device *device, unsigned int offsetwords,
				unsigned int *value);
void adreno_regwrite(struct kgsl_device *device, unsigned int offsetwords,
				unsigned int value);

int adreno_dump(struct kgsl_device *device, int manual);
unsigned int adreno_a3xx_rbbm_clock_ctl_default(struct adreno_device
							*adreno_dev);

struct kgsl_memdesc *adreno_find_region(struct kgsl_device *device,
						unsigned int pt_base,
						unsigned int gpuaddr,
						unsigned int size);

uint8_t *adreno_convertaddr(struct kgsl_device *device,
	unsigned int pt_base, unsigned int gpuaddr, unsigned int size);

struct kgsl_memdesc *adreno_find_ctxtmem(struct kgsl_device *device,
	unsigned int pt_base, unsigned int gpuaddr, unsigned int size);

void *adreno_snapshot(struct kgsl_device *device, void *snapshot, int *remain,
		int hang);

void adreno_dispatcher_start(struct adreno_device *adreno_dev);
int adreno_dispatcher_init(struct adreno_device *adreno_dev);
void adreno_dispatcher_close(struct adreno_device *adreno_dev);
int adreno_dispatcher_idle(struct adreno_device *adreno_dev,
		unsigned int timeout);
void adreno_dispatcher_irq_fault(struct kgsl_device *device);
void adreno_dispatcher_stop(struct adreno_device *adreno_dev);

int adreno_context_queue_cmd(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt, struct kgsl_cmdbatch *cmdbatch,
		uint32_t *timestamp);

void adreno_dispatcher_schedule(struct kgsl_device *device);
void adreno_dispatcher_pause(struct adreno_device *adreno_dev);
void adreno_dispatcher_queue_context(struct kgsl_device *device,
	struct adreno_context *drawctxt);
int adreno_reset(struct kgsl_device *device);

int adreno_perfcounter_get(struct adreno_device *adreno_dev,
	unsigned int groupid, unsigned int countable, unsigned int *offset,
	unsigned int flags);

int adreno_perfcounter_put(struct adreno_device *adreno_dev,
	unsigned int groupid, unsigned int countable);

static inline int adreno_is_a200(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A200);
}

static inline int adreno_is_a203(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A203);
}

static inline int adreno_is_a205(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A205);
}

static inline int adreno_is_a20x(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev <= 209);
}

static inline int adreno_is_a220(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A220);
}

static inline int adreno_is_a225(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A225);
}

static inline int adreno_is_a22x(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev  == ADRENO_REV_A220 ||
		adreno_dev->gpurev == ADRENO_REV_A225);
}

static inline int adreno_is_a2xx(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev <= 299);
}

static inline int adreno_is_a3xx(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev >= 300);
}

static inline int adreno_is_a305(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A305);
}

static inline int adreno_is_a320(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A320);
}

static inline int adreno_is_a330(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A330);
}

static inline int adreno_is_a330v2(struct adreno_device *adreno_dev)
{
	return ((adreno_dev->gpurev == ADRENO_REV_A330) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chip_id) > 0));
}

static inline int adreno_rb_ctxtswitch(unsigned int *cmd)
{
	return (cmd[0] == cp_nop_packet(1) &&
		cmd[1] == KGSL_CONTEXT_TO_MEM_IDENTIFIER);
}

/**
 * adreno_context_timestamp() - Return the last queued timestamp for the context
 * @k_ctxt: Pointer to the KGSL context to query
 * @rb: Pointer to the ringbuffer structure for the GPU
 *
 * Return the last queued context for the given context. This is used to verify
 * that incoming requests are not using an invalid (unsubmitted) timestamp
 */
static inline int adreno_context_timestamp(struct kgsl_context *k_ctxt,
		struct adreno_ringbuffer *rb)
{
	if (k_ctxt) {
		struct adreno_context *a_ctxt = ADRENO_CONTEXT(k_ctxt);
		return a_ctxt->timestamp;
	}
	return rb->global_ts;
}

/**
 * adreno_encode_istore_size - encode istore size in CP format
 * @adreno_dev - The 3D device.
 *
 * Encode the istore size into the format expected that the
 * CP_SET_SHADER_BASES and CP_ME_INIT commands:
 * bits 31:29 - istore size as encoded by this function
 * bits 27:16 - vertex shader start offset in instructions
 * bits 11:0 - pixel shader start offset in instructions.
 */
static inline int adreno_encode_istore_size(struct adreno_device *adreno_dev)
{
	unsigned int size;
	/* in a225 the CP microcode multiplies the encoded
	 * value by 3 while decoding.
	 */
	if (adreno_is_a225(adreno_dev))
		size = adreno_dev->istore_size/3;
	else
		size = adreno_dev->istore_size;

	return (ilog2(size) - 5) << 29;
}

static inline int __adreno_add_idle_indirect_cmds(unsigned int *cmds,
						unsigned int nop_gpuaddr)
{
	/* Adding an indirect buffer ensures that the prefetch stalls until
	 * the commands in indirect buffer have completed. We need to stall
	 * prefetch with a nop indirect buffer when updating pagetables
	 * because it provides stabler synchronization */
	*cmds++ = CP_HDR_INDIRECT_BUFFER_PFD;
	*cmds++ = nop_gpuaddr;
	*cmds++ = 2;
	*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmds++ = 0x00000000;
	return 5;
}

static inline int adreno_add_change_mh_phys_limit_cmds(unsigned int *cmds,
						unsigned int new_phys_limit,
						unsigned int nop_gpuaddr)
{
	unsigned int *start = cmds;

	*cmds++ = cp_type0_packet(MH_MMU_MPU_END, 1);
	*cmds++ = new_phys_limit;
	cmds += __adreno_add_idle_indirect_cmds(cmds, nop_gpuaddr);
	return cmds - start;
}

static inline int adreno_add_bank_change_cmds(unsigned int *cmds,
					int cur_ctx_bank,
					unsigned int nop_gpuaddr)
{
	unsigned int *start = cmds;

	*cmds++ = cp_type0_packet(REG_CP_STATE_DEBUG_INDEX, 1);
	*cmds++ = (cur_ctx_bank ? 0 : 0x20);
	cmds += __adreno_add_idle_indirect_cmds(cmds, nop_gpuaddr);
	return cmds - start;
}

/*
 * adreno_read_cmds - Add pm4 packets to perform read
 * @device - Pointer to device structure
 * @cmds - Pointer to memory where read commands need to be added
 * @addr - gpu address of the read
 * @val - The GPU will wait until the data at address addr becomes
 * equal to value
 */
static inline int adreno_add_read_cmds(struct kgsl_device *device,
				unsigned int *cmds, unsigned int addr,
				unsigned int val, unsigned int nop_gpuaddr)
{
	unsigned int *start = cmds;

	*cmds++ = cp_type3_packet(CP_WAIT_REG_MEM, 5);
	/* MEM SPACE = memory, FUNCTION = equals */
	*cmds++ = 0x13;
	*cmds++ = addr;
	*cmds++ = val;
	*cmds++ = 0xFFFFFFFF;
	*cmds++ = 0xFFFFFFFF;
	cmds += __adreno_add_idle_indirect_cmds(cmds, nop_gpuaddr);
	return cmds - start;
}

/*
 * adreno_idle_cmds - Add pm4 packets for GPU idle
 * @adreno_dev - Pointer to device structure
 * @cmds - Pointer to memory where idle commands need to be added
 */
static inline int adreno_add_idle_cmds(struct adreno_device *adreno_dev,
							unsigned int *cmds)
{
	unsigned int *start = cmds;

	*cmds++ = cp_type3_packet(CP_WAIT_FOR_IDLE, 1);
	*cmds++ = 0x00000000;

	if ((adreno_dev->gpurev == ADRENO_REV_A305) ||
		(adreno_dev->gpurev == ADRENO_REV_A320)) {
		*cmds++ = cp_type3_packet(CP_WAIT_FOR_ME, 1);
		*cmds++ = 0x00000000;
	}

	return cmds - start;
}

#endif /*__ADRENO_H */
