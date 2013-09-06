/* Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/android_pmem.h>
#include <linux/msm_rotator.h>
#include <linux/io.h>
#include <mach/msm_rotator_imem.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/file.h>
#include <linux/major.h>
#include <linux/regulator/consumer.h>
#include <linux/msm_ion.h>
#include <linux/sync.h>
#include <linux/sw_sync.h>

#ifdef CONFIG_MSM_BUS_SCALING
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#endif
#include <mach/msm_subsystem_map.h>
#include <mach/iommu_domains.h>

#define DRIVER_NAME "msm_rotator"

#define MSM_ROTATOR_BASE (msm_rotator_dev->io_base)
#define MSM_ROTATOR_INTR_ENABLE			(MSM_ROTATOR_BASE+0x0020)
#define MSM_ROTATOR_INTR_STATUS			(MSM_ROTATOR_BASE+0x0024)
#define MSM_ROTATOR_INTR_CLEAR			(MSM_ROTATOR_BASE+0x0028)
#define MSM_ROTATOR_START			(MSM_ROTATOR_BASE+0x0030)
#define MSM_ROTATOR_MAX_BURST_SIZE		(MSM_ROTATOR_BASE+0x0050)
#define MSM_ROTATOR_HW_VERSION			(MSM_ROTATOR_BASE+0x0070)
#define MSM_ROTATOR_SW_RESET			(MSM_ROTATOR_BASE+0x0074)
#define MSM_ROTATOR_SRC_SIZE			(MSM_ROTATOR_BASE+0x1108)
#define MSM_ROTATOR_SRCP0_ADDR			(MSM_ROTATOR_BASE+0x110c)
#define MSM_ROTATOR_SRCP1_ADDR			(MSM_ROTATOR_BASE+0x1110)
#define MSM_ROTATOR_SRCP2_ADDR			(MSM_ROTATOR_BASE+0x1114)
#define MSM_ROTATOR_SRC_YSTRIDE1		(MSM_ROTATOR_BASE+0x111c)
#define MSM_ROTATOR_SRC_YSTRIDE2		(MSM_ROTATOR_BASE+0x1120)
#define MSM_ROTATOR_SRC_FORMAT			(MSM_ROTATOR_BASE+0x1124)
#define MSM_ROTATOR_SRC_UNPACK_PATTERN1		(MSM_ROTATOR_BASE+0x1128)
#define MSM_ROTATOR_SUB_BLOCK_CFG		(MSM_ROTATOR_BASE+0x1138)
#define MSM_ROTATOR_OUT_PACK_PATTERN1		(MSM_ROTATOR_BASE+0x1154)
#define MSM_ROTATOR_OUTP0_ADDR			(MSM_ROTATOR_BASE+0x1168)
#define MSM_ROTATOR_OUTP1_ADDR			(MSM_ROTATOR_BASE+0x116c)
#define MSM_ROTATOR_OUTP2_ADDR			(MSM_ROTATOR_BASE+0x1170)
#define MSM_ROTATOR_OUT_YSTRIDE1		(MSM_ROTATOR_BASE+0x1178)
#define MSM_ROTATOR_OUT_YSTRIDE2		(MSM_ROTATOR_BASE+0x117c)
#define MSM_ROTATOR_SRC_XY			(MSM_ROTATOR_BASE+0x1200)
#define MSM_ROTATOR_SRC_IMAGE_SIZE		(MSM_ROTATOR_BASE+0x1208)

#define MSM_ROTATOR_MAX_ROT	0x07
#define MSM_ROTATOR_MAX_H	0x1fff
#define MSM_ROTATOR_MAX_W	0x1fff

/* from lsb to msb */
#define GET_PACK_PATTERN(a, x, y, z, bit) \
			(((a)<<((bit)*3))|((x)<<((bit)*2))|((y)<<(bit))|(z))
#define CLR_G 0x0
#define CLR_B 0x1
#define CLR_R 0x2
#define CLR_ALPHA 0x3

#define CLR_Y  CLR_G
#define CLR_CB CLR_B
#define CLR_CR CLR_R

#define ROTATIONS_TO_BITMASK(r) ((((r) & MDP_ROT_90) ? 1 : 0)  | \
				 (((r) & MDP_FLIP_LR) ? 2 : 0) | \
				 (((r) & MDP_FLIP_UD) ? 4 : 0))

#define IMEM_NO_OWNER -1;

#define MAX_SESSIONS 16
#define INVALID_SESSION -1
#define VERSION_KEY_MASK 0xFFFFFF00
#define MAX_DOWNSCALE_RATIO 3
#define MAX_COMMIT_QUEUE 4
#define WAIT_ROT_TIMEOUT 1000

#define MAX_TIMELINE_NAME_LEN 16
#define WAIT_FENCE_FIRST_TIMEOUT MSEC_PER_SEC
#define WAIT_FENCE_FINAL_TIMEOUT (10 * MSEC_PER_SEC)

#define ROTATOR_REVISION_V0		0
#define ROTATOR_REVISION_V1		1
#define ROTATOR_REVISION_V2		2
#define ROTATOR_REVISION_NONE	0xffffffff
#define	BASE_ADDR(height, y_stride) ((height % 64) * y_stride)
#define	HW_BASE_ADDR(height, y_stride) (((dstp0_ystride >> 5) << 11) - \
					((dst_height & 0x3f) * dstp0_ystride))

uint32_t rotator_hw_revision;
static char rot_iommu_split_domain;

/*
 * rotator_hw_revision:
 * 0 == 7x30
 * 1 == 8x60
 * 2 == 8960
 *
 */
struct tile_parm {
	unsigned int width;  /* tile's width */
	unsigned int height; /* tile's height */
	unsigned int row_tile_w; /* tiles per row's width */
	unsigned int row_tile_h; /* tiles per row's height */
};

struct msm_rotator_mem_planes {
	unsigned int num_planes;
	unsigned int plane_size[4];
	unsigned int total_size;
};

#define checkoffset(offset, size, max_size) \
	((size) > (max_size) || (offset) > ((max_size) - (size)))

struct msm_rotator_fd_info {
	int pid;
	int ref_cnt;
	struct list_head list;
};

struct rot_sync_info {
	u32 initialized;
	struct sync_fence *acq_fen;
	struct sync_fence *rel_fen;
	int rel_fen_fd;
	struct sw_sync_timeline *timeline;
	int timeline_value;
	struct mutex sync_mutex;
	atomic_t queue_buf_cnt;
};

struct msm_rotator_session {
	struct msm_rotator_img_info img_info;
	struct msm_rotator_fd_info fd_info;
	int fast_yuv_enable;
	int enable_2pass;
	u32 mem_hid;
};

struct msm_rotator_commit_info {
	struct msm_rotator_data_info data_info;
	struct msm_rotator_img_info img_info;
	unsigned int format;
	unsigned int in_paddr;
	unsigned int out_paddr;
	unsigned int in_chroma_paddr;
	unsigned int out_chroma_paddr;
	unsigned int in_chroma2_paddr;
	unsigned int out_chroma2_paddr;
	struct file *srcp0_file;
	struct file *srcp1_file;
	struct file *dstp0_file;
	struct file *dstp1_file;
	struct ion_handle *srcp0_ihdl;
	struct ion_handle *srcp1_ihdl;
	struct ion_handle *dstp0_ihdl;
	struct ion_handle *dstp1_ihdl;
	int ps0_need;
	int session_index;
	struct sync_fence *acq_fen;
	int fast_yuv_en;
	int enable_2pass;
};

struct msm_rotator_dev {
	void __iomem *io_base;
	int irq;
	struct clk *core_clk;
	struct msm_rotator_session *rot_session[MAX_SESSIONS];
	struct list_head fd_list;
	struct clk *pclk;
	int rot_clk_state;
	struct regulator *regulator;
	struct delayed_work rot_clk_work;
	struct clk *imem_clk;
	int imem_clk_state;
	struct delayed_work imem_clk_work;
	struct platform_device *pdev;
	struct cdev cdev;
	struct device *device;
	struct class *class;
	dev_t dev_num;
	int processing;
	int last_session_idx;
	struct mutex rotator_lock;
	struct mutex imem_lock;
	int imem_owner;
	wait_queue_head_t wq;
	struct ion_client *client;
	#ifdef CONFIG_MSM_BUS_SCALING
	uint32_t bus_client_handle;
	#endif
	u32 sec_mapped;
	u32 mmu_clk_on;
	struct rot_sync_info sync_info[MAX_SESSIONS];
	/* non blocking */
	struct mutex commit_mutex;
	struct mutex commit_wq_mutex;
	struct completion commit_comp;
	u32 commit_running;
	struct work_struct commit_work;
	struct msm_rotator_commit_info commit_info[MAX_COMMIT_QUEUE];
	atomic_t commit_q_r;
	atomic_t commit_q_w;
	atomic_t commit_q_cnt;
	struct rot_buf_type *y_rot_buf;
	struct rot_buf_type *chroma_rot_buf;
	struct rot_buf_type *chroma2_rot_buf;
};

#define COMPONENT_5BITS 1
#define COMPONENT_6BITS 2
#define COMPONENT_8BITS 3

static struct msm_rotator_dev *msm_rotator_dev;
#define mrd msm_rotator_dev
static void rot_wait_for_commit_queue(u32 is_all);

enum {
	CLK_EN,
	CLK_DIS,
	CLK_SUSPEND,
};
struct res_mmu_clk {
	char *mmu_clk_name;
	struct clk *mmu_clk;
};

static struct res_mmu_clk rot_mmu_clks[] = {
	{"mdp_iommu_clk"}, {"rot_iommu_clk"},
	{"vcodec_iommu0_clk"}, {"vcodec_iommu1_clk"},
	{"smmu_iface_clk"}
};

u32 rotator_allocate_2pass_buf(struct rot_buf_type *rot_buf, int s_ndx)
{
	ion_phys_addr_t	addr, read_addr = 0;
	size_t buffer_size;
	unsigned long len;

	if (!rot_buf) {
		pr_err("Rot_buf NULL pointer %s %i", __func__, __LINE__);
		return 0;
	}

	if (rot_buf->write_addr || !IS_ERR_OR_NULL(rot_buf->ihdl))
		return 0;

	buffer_size = roundup(1920 * 1088, SZ_4K);

	if (!IS_ERR_OR_NULL(mrd->client)) {
		pr_info("%s:%d ion based allocation\n",
			__func__, __LINE__);
		rot_buf->ihdl = ion_alloc(mrd->client, buffer_size, SZ_4K,
			mrd->rot_session[s_ndx]->mem_hid,
			mrd->rot_session[s_ndx]->mem_hid & ION_SECURE);
		if (!IS_ERR_OR_NULL(rot_buf->ihdl)) {
			if (rot_iommu_split_domain) {
				if (ion_map_iommu(mrd->client, rot_buf->ihdl,
					ROTATOR_SRC_DOMAIN, GEN_POOL, SZ_4K,
					0, &read_addr, &len, 0, 0)) {
					pr_err("ion_map_iommu() read failed\n");
					return -ENOMEM;
				}
				if (mrd->rot_session[s_ndx]->mem_hid &
								ION_SECURE) {
					if (ion_phys(mrd->client, rot_buf->ihdl,
						&addr, (size_t *)&len)) {
						pr_err(
						"%s:%d: ion_phys map failed\n",
							 __func__, __LINE__);
						return -ENOMEM;
					}
				} else {
					if (ion_map_iommu(mrd->client,
					    rot_buf->ihdl, ROTATOR_DST_DOMAIN,
					    GEN_POOL, SZ_4K, 0, &addr, &len,
					    0, 0)) {
						pr_err("ion_map_iommu() failed\n");
						return -ENOMEM;
					}
				}
			} else {
				if (ion_map_iommu(mrd->client, rot_buf->ihdl,
					ROTATOR_SRC_DOMAIN, GEN_POOL, SZ_4K,
					0, &addr, &len, 0, 0)) {
					pr_err("ion_map_iommu() write failed\n");
					return -ENOMEM;
				}
			}
		} else {
			pr_err("%s:%d: ion_alloc failed\n", __func__,
				__LINE__);
			return -ENOMEM;
		}
	} else {
		addr = allocate_contiguous_memory_nomap(buffer_size,
			mrd->rot_session[s_ndx]->mem_hid, 4);
	}
	if (addr) {
		pr_info("allocating %d bytes at write=%x, read=%x for 2-pass\n",
			buffer_size, (u32) addr, (u32) read_addr);
		rot_buf->write_addr = addr;

		if (read_addr)
			rot_buf->read_addr = read_addr;
		else
			rot_buf->read_addr = rot_buf->write_addr;

		return 0;
	} else {
		pr_err("%s cannot allocate memory for rotator 2-pass!\n",
			 __func__);
		return -ENOMEM;
	}
}

void rotator_free_2pass_buf(struct rot_buf_type *rot_buf, int s_ndx)
{

	if (!rot_buf) {
		pr_err("Rot_buf NULL pointer %s %i", __func__, __LINE__);
		return;
	}

	if (!rot_buf->write_addr)
		return;

	if (!IS_ERR_OR_NULL(mrd->client)) {
		if (!IS_ERR_OR_NULL(rot_buf->ihdl)) {
			if (rot_iommu_split_domain) {
				if (!(mrd->rot_session[s_ndx]->mem_hid &
								ION_SECURE))
					ion_unmap_iommu(mrd->client,
					rot_buf->ihdl, ROTATOR_DST_DOMAIN,
								GEN_POOL);
				ion_unmap_iommu(mrd->client, rot_buf->ihdl,
					ROTATOR_SRC_DOMAIN, GEN_POOL);
			} else {
				ion_unmap_iommu(mrd->client, rot_buf->ihdl,
					ROTATOR_SRC_DOMAIN, GEN_POOL);
			}
			ion_free(mrd->client, rot_buf->ihdl);
			rot_buf->ihdl = NULL;
			pr_info("%s:%d Free rotator 2pass memory",
					__func__, __LINE__);
		}
	} else {
		if (rot_buf->write_addr) {
			free_contiguous_memory_by_paddr(rot_buf->write_addr);
			pr_debug("%s:%d Free rotator 2pass pmem\n", __func__,
				__LINE__);
		}
	}
	rot_buf->write_addr = 0;
	rot_buf->read_addr = 0;
}

int msm_rotator_iommu_map_buf(int mem_id, int domain,
	unsigned long *start, unsigned long *len,
	struct ion_handle **pihdl, unsigned int secure)
{
	if (!msm_rotator_dev->client)
		return -EINVAL;

	*pihdl = ion_import_dma_buf(msm_rotator_dev->client, mem_id);
	if (IS_ERR_OR_NULL(*pihdl)) {
		pr_err("ion_import_dma_buf() failed\n");
		return PTR_ERR(*pihdl);
	}
	pr_debug("%s(): ion_hdl %p, ion_fd %d\n", __func__, *pihdl, mem_id);

	if (rot_iommu_split_domain) {
		if (secure) {
			if (ion_phys(msm_rotator_dev->client,
				*pihdl, start, (unsigned *)len)) {
				pr_err("%s:%d: ion_phys map failed\n",
					 __func__, __LINE__);
				return -ENOMEM;
			}
		} else {
			if (ion_map_iommu(msm_rotator_dev->client,
				*pihdl,	domain, GEN_POOL,
				SZ_4K, 0, start, len, 0,
				ION_IOMMU_UNMAP_DELAYED)) {
				pr_err("ion_map_iommu() failed\n");
				return -EINVAL;
			}
		}
	} else {
		if (ion_map_iommu(msm_rotator_dev->client,
			*pihdl,	ROTATOR_SRC_DOMAIN, GEN_POOL,
			SZ_4K, 0, start, len, 0, ION_IOMMU_UNMAP_DELAYED)) {
			pr_err("ion_map_iommu() failed\n");
			return -EINVAL;
		}
	}

	pr_debug("%s(): mem_id %d, start 0x%lx, len 0x%lx\n",
		__func__, mem_id, *start, *len);
	return 0;
}

int msm_rotator_imem_allocate(int requestor)
{
	int rc = 0;

#ifdef CONFIG_MSM_ROTATOR_USE_IMEM
	switch (requestor) {
	case ROTATOR_REQUEST:
		if (mutex_trylock(&msm_rotator_dev->imem_lock)) {
			msm_rotator_dev->imem_owner = ROTATOR_REQUEST;
			rc = 1;
		} else
			rc = 0;
		break;
	case JPEG_REQUEST:
		mutex_lock(&msm_rotator_dev->imem_lock);
		msm_rotator_dev->imem_owner = JPEG_REQUEST;
		rc = 1;
		break;
	default:
		rc = 0;
	}
#else
	if (requestor == JPEG_REQUEST)
		rc = 1;
#endif
	if (rc == 1) {
		cancel_delayed_work(&msm_rotator_dev->imem_clk_work);
		if (msm_rotator_dev->imem_clk_state != CLK_EN
			&& msm_rotator_dev->imem_clk) {
			clk_prepare_enable(msm_rotator_dev->imem_clk);
			msm_rotator_dev->imem_clk_state = CLK_EN;
		}
	}

	return rc;
}
EXPORT_SYMBOL(msm_rotator_imem_allocate);

void msm_rotator_imem_free(int requestor)
{
#ifdef CONFIG_MSM_ROTATOR_USE_IMEM
	if (msm_rotator_dev->imem_owner == requestor) {
		schedule_delayed_work(&msm_rotator_dev->imem_clk_work, HZ);
		mutex_unlock(&msm_rotator_dev->imem_lock);
	}
#else
	if (requestor == JPEG_REQUEST)
		schedule_delayed_work(&msm_rotator_dev->imem_clk_work, HZ);
#endif
}
EXPORT_SYMBOL(msm_rotator_imem_free);

static void msm_rotator_imem_clk_work_f(struct work_struct *work)
{
#ifdef CONFIG_MSM_ROTATOR_USE_IMEM
	if (mutex_trylock(&msm_rotator_dev->imem_lock)) {
		if (msm_rotator_dev->imem_clk_state == CLK_EN
		     && msm_rotator_dev->imem_clk) {
			clk_disable_unprepare(msm_rotator_dev->imem_clk);
			msm_rotator_dev->imem_clk_state = CLK_DIS;
		} else if (msm_rotator_dev->imem_clk_state == CLK_SUSPEND)
			msm_rotator_dev->imem_clk_state = CLK_DIS;
		mutex_unlock(&msm_rotator_dev->imem_lock);
	}
#endif
}

/* enable clocks needed by rotator block */
static void enable_rot_clks(void)
{
	if (msm_rotator_dev->regulator)
		regulator_enable(msm_rotator_dev->regulator);
	if (msm_rotator_dev->core_clk != NULL)
		clk_prepare_enable(msm_rotator_dev->core_clk);
	if (msm_rotator_dev->pclk != NULL)
		clk_prepare_enable(msm_rotator_dev->pclk);
}

/* disable clocks needed by rotator block */
static void disable_rot_clks(void)
{
	if (msm_rotator_dev->core_clk != NULL)
		clk_disable_unprepare(msm_rotator_dev->core_clk);
	if (msm_rotator_dev->pclk != NULL)
		clk_disable_unprepare(msm_rotator_dev->pclk);
	if (msm_rotator_dev->regulator)
		regulator_disable(msm_rotator_dev->regulator);
}

static void msm_rotator_rot_clk_work_f(struct work_struct *work)
{
	if (mutex_trylock(&msm_rotator_dev->rotator_lock)) {
		if (msm_rotator_dev->rot_clk_state == CLK_EN) {
			disable_rot_clks();
			msm_rotator_dev->rot_clk_state = CLK_DIS;
		} else if (msm_rotator_dev->rot_clk_state == CLK_SUSPEND)
			msm_rotator_dev->rot_clk_state = CLK_DIS;
		mutex_unlock(&msm_rotator_dev->rotator_lock);
	}
}

static irqreturn_t msm_rotator_isr(int irq, void *dev_id)
{
	if (msm_rotator_dev->processing) {
		msm_rotator_dev->processing = 0;
		wake_up(&msm_rotator_dev->wq);
	} else
		printk(KERN_WARNING "%s: unexpected interrupt\n", DRIVER_NAME);

	return IRQ_HANDLED;
}

static void msm_rotator_signal_timeline(u32 session_index)
{
	struct rot_sync_info *sync_info;
	sync_info = &msm_rotator_dev->sync_info[session_index];

	if ((!sync_info->timeline) || (!sync_info->initialized))
		return;

	mutex_lock(&sync_info->sync_mutex);
	sw_sync_timeline_inc(sync_info->timeline, 1);
	sync_info->timeline_value++;
	mutex_unlock(&sync_info->sync_mutex);
}

static void msm_rotator_signal_timeline_done(u32 session_index)
{
	struct rot_sync_info *sync_info;
	sync_info = &msm_rotator_dev->sync_info[session_index];

	if ((sync_info->timeline == NULL) ||
		(sync_info->initialized == false))
			return;
	mutex_lock(&sync_info->sync_mutex);
	sw_sync_timeline_inc(sync_info->timeline, 1);
	sync_info->timeline_value++;
	if (atomic_read(&sync_info->queue_buf_cnt) <= 0)
		pr_err("%s queue_buf_cnt=%d", __func__,
			atomic_read(&sync_info->queue_buf_cnt));
	else
		atomic_dec(&sync_info->queue_buf_cnt);
	mutex_unlock(&sync_info->sync_mutex);
}

static void msm_rotator_release_acq_fence(u32 session_index)
{
	struct rot_sync_info *sync_info;
	sync_info = &msm_rotator_dev->sync_info[session_index];

	if ((!sync_info->timeline) || (!sync_info->initialized))
		return;
	mutex_lock(&sync_info->sync_mutex);
	sync_info->acq_fen = NULL;
	mutex_unlock(&sync_info->sync_mutex);
}

static void msm_rotator_release_all_timeline(void)
{
	int i;
	struct rot_sync_info *sync_info;
	for (i = 0; i < MAX_SESSIONS; i++) {
		sync_info = &msm_rotator_dev->sync_info[i];
		if (sync_info->initialized) {
			msm_rotator_signal_timeline(i);
			msm_rotator_release_acq_fence(i);
		}
	}
}

static void msm_rotator_wait_for_fence(struct sync_fence *acq_fen)
{
	int ret;
	if (acq_fen) {
		ret = sync_fence_wait(acq_fen,
				WAIT_FENCE_FIRST_TIMEOUT);
		if (ret == -ETIME) {
			pr_warn("%s: timeout, wait %ld more ms\n",
				__func__, WAIT_FENCE_FINAL_TIMEOUT);
			ret = sync_fence_wait(acq_fen,
				WAIT_FENCE_FINAL_TIMEOUT);
		}
		if (ret < 0) {
			pr_err("%s: sync_fence_wait failed! ret = %x\n",
				__func__, ret);
		}
		sync_fence_put(acq_fen);
	}
}

static int  msm_rotator_buf_sync(unsigned long arg)
{
	struct msm_rotator_buf_sync buf_sync;
	int ret = 0;
	struct sync_fence *fence = NULL;
	struct rot_sync_info *sync_info;
	struct sync_pt *rel_sync_pt;
	struct sync_fence *rel_fence;
	int rel_fen_fd;
	u32 s;

	if (copy_from_user(&buf_sync, (void __user *)arg, sizeof(buf_sync)))
		return -EFAULT;

	rot_wait_for_commit_queue(false);
	for (s = 0; s < MAX_SESSIONS; s++)
		if ((msm_rotator_dev->rot_session[s] != NULL) &&
			(buf_sync.session_id ==
			(unsigned int)msm_rotator_dev->rot_session[s]
			))
			break;

	if (s == MAX_SESSIONS)  {
		pr_err("%s invalid session id %d", __func__,
			buf_sync.session_id);
		return -EINVAL;
	}

	sync_info = &msm_rotator_dev->sync_info[s];

	if (sync_info->acq_fen)
		pr_err("%s previous acq_fen will be overwritten", __func__);

	if ((sync_info->timeline == NULL) ||
		(sync_info->initialized == false))
		return -EINVAL;

	mutex_lock(&sync_info->sync_mutex);
	if (buf_sync.acq_fen_fd >= 0)
		fence = sync_fence_fdget(buf_sync.acq_fen_fd);

	sync_info->acq_fen = fence;

	if (sync_info->acq_fen &&
		(buf_sync.flags & MDP_BUF_SYNC_FLAG_WAIT)) {
		msm_rotator_wait_for_fence(sync_info->acq_fen);
		sync_info->acq_fen = NULL;
	}

	rel_sync_pt = sw_sync_pt_create(sync_info->timeline,
			sync_info->timeline_value +
			atomic_read(&sync_info->queue_buf_cnt) + 1);
	if (rel_sync_pt == NULL) {
		pr_err("%s: cannot create sync point", __func__);
		ret = -ENOMEM;
		goto buf_sync_err_1;
	}
	/* create fence */
	rel_fence = sync_fence_create("msm_rotator-fence",
			rel_sync_pt);
	if (rel_fence == NULL) {
		sync_pt_free(rel_sync_pt);
		pr_err("%s: cannot create fence", __func__);
		ret = -ENOMEM;
		goto buf_sync_err_1;
	}
	/* create fd */
	rel_fen_fd = get_unused_fd_flags(0);
	if (rel_fen_fd < 0) {
		pr_err("%s: get_unused_fd_flags failed", __func__);
		ret  = -EIO;
		goto buf_sync_err_2;
	}
	sync_fence_install(rel_fence, rel_fen_fd);
	buf_sync.rel_fen_fd = rel_fen_fd;
	sync_info->rel_fen = rel_fence;
	sync_info->rel_fen_fd = rel_fen_fd;

	ret = copy_to_user((void __user *)arg, &buf_sync, sizeof(buf_sync));
	mutex_unlock(&sync_info->sync_mutex);
	return ret;
buf_sync_err_2:
	sync_fence_put(rel_fence);
buf_sync_err_1:
	if (sync_info->acq_fen)
		sync_fence_put(sync_info->acq_fen);
	sync_info->acq_fen = NULL;
	mutex_unlock(&sync_info->sync_mutex);
	return ret;
}

static unsigned int tile_size(unsigned int src_width,
		unsigned int src_height,
		const struct tile_parm *tp)
{
	unsigned int tile_w, tile_h;
	unsigned int row_num_w, row_num_h;
	tile_w = tp->width * tp->row_tile_w;
	tile_h = tp->height * tp->row_tile_h;
	row_num_w = (src_width + tile_w - 1) / tile_w;
	row_num_h = (src_height + tile_h - 1) / tile_h;
	return ((row_num_w * row_num_h * tile_w * tile_h) + 8191) & ~8191;
}

static int get_bpp(int format)
{
	switch (format) {
	case MDP_RGB_565:
	case MDP_BGR_565:
		return 2;

	case MDP_XRGB_8888:
	case MDP_ARGB_8888:
	case MDP_RGBA_8888:
	case MDP_BGRA_8888:
	case MDP_RGBX_8888:
		return 4;

	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CRCB_H2V2:
	case MDP_Y_CB_CR_H2V2:
	case MDP_Y_CR_CB_H2V2:
	case MDP_Y_CR_CB_GH2V2:
	case MDP_Y_CRCB_H2V2_TILE:
	case MDP_Y_CBCR_H2V2_TILE:
		return 1;

	case MDP_RGB_888:
	case MDP_YCBCR_H1V1:
	case MDP_YCRCB_H1V1:
		return 3;

	case MDP_YCBYCR_H2V1:
	case MDP_YCRYCB_H2V1:
		return 2;/* YCrYCb interleave */

	case MDP_Y_CRCB_H2V1:
	case MDP_Y_CBCR_H2V1:
		return 1;

	default:
		return -1;
	}

}

static int msm_rotator_get_plane_sizes(uint32_t format,	uint32_t w, uint32_t h,
				       struct msm_rotator_mem_planes *p)
{
	/*
	 * each row of samsung tile consists of two tiles in height
	 * and two tiles in width which means width should align to
	 * 64 x 2 bytes and height should align to 32 x 2 bytes.
	 * video decoder generate two tiles in width and one tile
	 * in height which ends up height align to 32 X 1 bytes.
	 */
	const struct tile_parm tile = {64, 32, 2, 1};
	int i;

	if (p == NULL)
		return -EINVAL;

	if ((w > MSM_ROTATOR_MAX_W) || (h > MSM_ROTATOR_MAX_H))
		return -ERANGE;

	memset(p, 0, sizeof(*p));

	switch (format) {
	case MDP_XRGB_8888:
	case MDP_ARGB_8888:
	case MDP_RGBA_8888:
	case MDP_BGRA_8888:
	case MDP_RGBX_8888:
	case MDP_RGB_888:
	case MDP_RGB_565:
	case MDP_BGR_565:
	case MDP_YCBYCR_H2V1:
	case MDP_YCRYCB_H2V1:
	case MDP_YCBCR_H1V1:
	case MDP_YCRCB_H1V1:
		p->num_planes = 1;
		p->plane_size[0] = w * h * get_bpp(format);
		break;
	case MDP_Y_CRCB_H2V1:
	case MDP_Y_CBCR_H2V1:
	case MDP_Y_CRCB_H1V2:
	case MDP_Y_CBCR_H1V2:
		p->num_planes = 2;
		p->plane_size[0] = w * h;
		p->plane_size[1] = w * h;
		break;
	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CRCB_H2V2:
		p->num_planes = 2;
		p->plane_size[0] = w * h;
		p->plane_size[1] = w * h / 2;
		break;
	case MDP_Y_CRCB_H2V2_TILE:
	case MDP_Y_CBCR_H2V2_TILE:
		p->num_planes = 2;
		p->plane_size[0] = tile_size(w, h, &tile);
		p->plane_size[1] = tile_size(w, h/2, &tile);
		break;
	case MDP_Y_CB_CR_H2V2:
	case MDP_Y_CR_CB_H2V2:
		p->num_planes = 3;
		p->plane_size[0] = w * h;
		p->plane_size[1] = (w / 2) * (h / 2);
		p->plane_size[2] = (w / 2) * (h / 2);
		break;
	case MDP_Y_CR_CB_GH2V2:
		p->num_planes = 3;
		p->plane_size[0] = ALIGN(w, 16) * h;
		p->plane_size[1] = ALIGN(w / 2, 16) * (h / 2);
		p->plane_size[2] = ALIGN(w / 2, 16) * (h / 2);
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < p->num_planes; i++)
		p->total_size += p->plane_size[i];

	return 0;
}

/* Checking invalid destination image size on FAST YUV for YUV420PP(NV12) with
 * HW issue  for rotation 90 + U/D filp + with/without flip operation
 * (rotation 90 + U/D + L/R flip is rotation 270 degree option) and pix_rot
 * block issue with tile line size is 4.
 *
 * Rotator structure is:
 * if Fetch input image: W x H,
 * Downscale: W` x H` = W/ScaleHor(2, 4 or 8) x H/ScaleVert(2, 4 or 8)
 * Rotated output : W`` x H`` = (W` x H`) or (H` x W`) depends on "Rotation 90
 * degree option"
 *
 * Pack: W`` x H``
 *
 * Rotator source ROI image width restriction is applied to W x H (case a,
 * image resolution before downscaling)
 *
 * Packer source Image width/ height restriction are applied to  W`` x H``
 * (case c, image resolution after rotation)
 *
 * Supertile (64 x 8) and YUV (2 x 2)  alignment restriction should be
 * applied to the W x H (case a). Input image should be at least (2 x 2).
 *
 * "Support If packer source image height <= 256, multiple of 8", this
 * restriction should be applied to the rotated image (W`` x H``)
 */

uint32_t fast_yuv_invalid_size_checker(unsigned char rot_mode,
						uint32_t src_width,
						uint32_t dst_width,
						uint32_t src_height,
						uint32_t dst_height,
						uint32_t dstp0_ystride,
						uint32_t is_planar420)
{
	uint32_t hw_limit;

	hw_limit  = is_planar420 ? 512 : 256;

	/* checking image constaints for missing EOT event from pix_rot block */
	if ((src_width > hw_limit) && ((src_width % (hw_limit / 2)) == 8))
		return -EINVAL;

	if (rot_mode & MDP_ROT_90) {

		if ((src_height % 128) == 8)
			return -EINVAL;

		/* if rotation 90 degree on fast yuv
		 * rotator image input width has to be multiple of 8
		 * rotator image input height has to be multiple of 8
		 */
		if (((dst_width % 8) != 0) || ((dst_height % 8) != 0))
			return -EINVAL;

		if ((rot_mode & MDP_FLIP_UD) ||
			(rot_mode & (MDP_FLIP_UD | MDP_FLIP_LR))) {

			/* image constraint checking for wrong address
			 * generation HW issue for Y plane checking
			 */
			if (((dst_height % 64) != 0) &&
				((dst_height / 64) >= 4)) {

				/* compare golden logic for second
				 * tile base address generation in row
				 * with actual HW implementation
				*/
				if (BASE_ADDR(dst_height, dstp0_ystride) !=
					HW_BASE_ADDR(dst_height, dstp0_ystride))
						return -EINVAL;
			}

			if (is_planar420) {
				dst_width = dst_width / 2;
				dstp0_ystride = dstp0_ystride / 2;
			}

			dst_height = dst_height / 2;

			/* image constraint checking for wrong
			 * address generation HW issue. for
			 * U/V (P) or UV (PP) plane checking
			 */
			if (((dst_height % 64) != 0) && ((dst_height / 64) >=
				(hw_limit / 128))) {

				/* compare golden logic for
				 * second tile base address
				 * generation in row with
				 * actual HW implementation
				*/
				if (BASE_ADDR(dst_height, dstp0_ystride) !=
					HW_BASE_ADDR(dst_height, dstp0_ystride))
						return -EINVAL;
			}
		}
	} else {
		/* if NOT applying rotation 90 degree on fast yuv,
		 * rotator image input width has to be multiple of 8
		 * rotator image input height has to be multiple of 8
		*/
		if (((dst_width % 8) != 0) || ((dst_height % 8) != 0))
			return -EINVAL;
	}

	return 0;
}

static int msm_rotator_ycxcx_h2v1(struct msm_rotator_img_info *info,
				  unsigned int in_paddr,
				  unsigned int out_paddr,
				  unsigned int use_imem,
				  int new_session,
				  unsigned int in_chroma_paddr,
				  unsigned int out_chroma_paddr)
{
	int bpp;
	uint32_t dst_format;
	switch (info->src.format) {
	case MDP_Y_CRCB_H2V1:
		if (info->rotations & MDP_ROT_90)
			dst_format = MDP_Y_CRCB_H1V2;
		else
			dst_format = info->src.format;
		break;
	case MDP_Y_CBCR_H2V1:
		if (info->rotations & MDP_ROT_90)
			dst_format = MDP_Y_CBCR_H1V2;
		else
			dst_format = info->src.format;
		break;
	default:
		return -EINVAL;
	}
	if (info->dst.format != dst_format)
		return -EINVAL;

	bpp = get_bpp(info->src.format);
	if (bpp < 0)
		return -ENOTTY;

	iowrite32(in_paddr, MSM_ROTATOR_SRCP0_ADDR);
	iowrite32(in_chroma_paddr, MSM_ROTATOR_SRCP1_ADDR);
	iowrite32(out_paddr +
			((info->dst_y * info->dst.width) + info->dst_x),
		  MSM_ROTATOR_OUTP0_ADDR);
	iowrite32(out_chroma_paddr +
			((info->dst_y * info->dst.width) + info->dst_x),
		  MSM_ROTATOR_OUTP1_ADDR);

	if (new_session) {
		iowrite32(info->src.width |
			  info->src.width << 16,
			  MSM_ROTATOR_SRC_YSTRIDE1);
		if (info->rotations & MDP_ROT_90)
			iowrite32(info->dst.width |
				  info->dst.width*2 << 16,
				  MSM_ROTATOR_OUT_YSTRIDE1);
		else
			iowrite32(info->dst.width |
				  info->dst.width << 16,
				  MSM_ROTATOR_OUT_YSTRIDE1);
		if (info->src.format == MDP_Y_CBCR_H2V1) {
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CB, CLR_CR, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CB, CLR_CR, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
		} else {
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CR, CLR_CB, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CR, CLR_CB, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
		}
		iowrite32((1  << 18) | 		/* chroma sampling 1=H2V1 */
			  (ROTATIONS_TO_BITMASK(info->rotations) << 9) |
			  1 << 8 |			/* ROT_EN */
			  info->downscale_ratio << 2 |	/* downscale v ratio */
			  info->downscale_ratio,	/* downscale h ratio */
			  MSM_ROTATOR_SUB_BLOCK_CFG);
		iowrite32(0 << 29 | 		/* frame format 0 = linear */
			  (use_imem ? 0 : 1) << 22 | /* tile size */
			  2 << 19 | 		/* fetch planes 2 = pseudo */
			  0 << 18 | 		/* unpack align */
			  1 << 17 | 		/* unpack tight */
			  1 << 13 | 		/* unpack count 0=1 component */
			  (bpp-1) << 9 |	/* src Bpp 0=1 byte ... */
			  0 << 8  | 		/* has alpha */
			  0 << 6  | 		/* alpha bits 3=8bits */
			  3 << 4  | 		/* R/Cr bits 1=5 2=6 3=8 */
			  3 << 2  | 		/* B/Cb bits 1=5 2=6 3=8 */
			  3 << 0,   		/* G/Y  bits 1=5 2=6 3=8 */
			  MSM_ROTATOR_SRC_FORMAT);
	}

	return 0;
}

static int msm_rotator_ycxcx_h2v2(struct msm_rotator_img_info *info,
				  unsigned int in_paddr,
				  unsigned int out_paddr,
				  unsigned int use_imem,
				  int new_session,
				  unsigned int in_chroma_paddr,
				  unsigned int out_chroma_paddr,
				  unsigned int in_chroma2_paddr,
				  unsigned int out_chroma2_paddr,
				  int fast_yuv_en)
{
	uint32_t dst_format;
	int is_tile = 0;

	switch (info->src.format) {
	case MDP_Y_CRCB_H2V2_TILE:
		is_tile = 1;
		dst_format = MDP_Y_CRCB_H2V2;
		break;
	case MDP_Y_CR_CB_H2V2:
	case MDP_Y_CR_CB_GH2V2:
		if (fast_yuv_en) {
			dst_format = info->src.format;
			break;
		}
	case MDP_Y_CRCB_H2V2:
		dst_format = MDP_Y_CRCB_H2V2;
		break;
	case MDP_Y_CB_CR_H2V2:
		if (fast_yuv_en) {
			dst_format = info->src.format;
			break;
		}
		dst_format = MDP_Y_CBCR_H2V2;
		break;
	case MDP_Y_CBCR_H2V2_TILE:
		is_tile = 1;
	case MDP_Y_CBCR_H2V2:
		dst_format = MDP_Y_CBCR_H2V2;
		break;
	default:
		return -EINVAL;
	}
	if (info->dst.format  != dst_format)
		return -EINVAL;

	/* rotator expects YCbCr for planar input format */
	if ((info->src.format == MDP_Y_CR_CB_H2V2 ||
	    info->src.format == MDP_Y_CR_CB_GH2V2) &&
	    rotator_hw_revision < ROTATOR_REVISION_V2)
		swap(in_chroma_paddr, in_chroma2_paddr);

	iowrite32(in_paddr, MSM_ROTATOR_SRCP0_ADDR);
	iowrite32(in_chroma_paddr, MSM_ROTATOR_SRCP1_ADDR);
	iowrite32(in_chroma2_paddr, MSM_ROTATOR_SRCP2_ADDR);

	iowrite32(out_paddr +
			((info->dst_y * info->dst.width) + info->dst_x),
		  MSM_ROTATOR_OUTP0_ADDR);
	iowrite32(out_chroma_paddr +
			(((info->dst_y * info->dst.width)/2) + info->dst_x),
		  MSM_ROTATOR_OUTP1_ADDR);
	if (out_chroma2_paddr)
		iowrite32(out_chroma2_paddr +
			(((info->dst_y * info->dst.width)/2) + info->dst_x),
			  MSM_ROTATOR_OUTP2_ADDR);

	if (new_session) {
		if (in_chroma2_paddr) {
			if (info->src.format == MDP_Y_CR_CB_GH2V2) {
				iowrite32(ALIGN(info->src.width, 16) |
					ALIGN((info->src.width / 2), 16) << 16,
					MSM_ROTATOR_SRC_YSTRIDE1);
				iowrite32(ALIGN((info->src.width / 2), 16),
					MSM_ROTATOR_SRC_YSTRIDE2);
			} else {
				iowrite32(info->src.width |
					(info->src.width / 2) << 16,
					MSM_ROTATOR_SRC_YSTRIDE1);
				iowrite32((info->src.width / 2),
					MSM_ROTATOR_SRC_YSTRIDE2);
			}
		} else {
			iowrite32(info->src.width |
					info->src.width << 16,
					MSM_ROTATOR_SRC_YSTRIDE1);
		}
		if (out_chroma2_paddr) {
			if (info->dst.format == MDP_Y_CR_CB_GH2V2) {
				iowrite32(ALIGN(info->dst.width, 16) |
					ALIGN((info->dst.width / 2), 16) << 16,
					MSM_ROTATOR_OUT_YSTRIDE1);
				iowrite32(ALIGN((info->dst.width / 2), 16),
					MSM_ROTATOR_OUT_YSTRIDE2);
			} else {
				iowrite32(info->dst.width |
						info->dst.width/2 << 16,
						MSM_ROTATOR_OUT_YSTRIDE1);
				iowrite32(info->dst.width/2,
						MSM_ROTATOR_OUT_YSTRIDE2);
			}
		} else {
			iowrite32(info->dst.width |
					info->dst.width << 16,
					MSM_ROTATOR_OUT_YSTRIDE1);
		}

		if (dst_format == MDP_Y_CBCR_H2V2 ||
			dst_format == MDP_Y_CB_CR_H2V2) {
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CB, CLR_CR, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CB, CLR_CR, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
		} else {
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CR, CLR_CB, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CR, CLR_CB, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
		}

		iowrite32((3  << 18) |		/* chroma sampling 3=4:2:0 */
			  (ROTATIONS_TO_BITMASK(info->rotations) << 9) |
			  1 << 8 |			/* ROT_EN */
			  fast_yuv_en << 4 |		/*fast YUV*/
			  info->downscale_ratio << 2 |	/* downscale v ratio */
			  info->downscale_ratio,	/* downscale h ratio */
			  MSM_ROTATOR_SUB_BLOCK_CFG);

		iowrite32((is_tile ? 2 : 0) << 29 |  /* frame format */
			  (use_imem ? 0 : 1) << 22 | /* tile size */
			  (in_chroma2_paddr ? 1 : 2) << 19 | /* fetch planes */
			  0 << 18 |		/* unpack align */
			  1 << 17 |		/* unpack tight */
			  1 << 13 |		/* unpack count 0=1 component */
			  0 << 9  |		/* src Bpp 0=1 byte ... */
			  0 << 8  |		/* has alpha */
			  0 << 6  |		/* alpha bits 3=8bits */
			  3 << 4  |		/* R/Cr bits 1=5 2=6 3=8 */
			  3 << 2  |		/* B/Cb bits 1=5 2=6 3=8 */
			  3 << 0,		/* G/Y  bits 1=5 2=6 3=8 */
			  MSM_ROTATOR_SRC_FORMAT);
	}
	return 0;
}

static int msm_rotator_ycxcx_h2v2_2pass(struct msm_rotator_img_info *info,
				  unsigned int in_paddr,
				  unsigned int out_paddr,
				  unsigned int use_imem,
				  int new_session,
				  unsigned int in_chroma_paddr,
				  unsigned int out_chroma_paddr,
				  unsigned int in_chroma2_paddr,
				  unsigned int out_chroma2_paddr,
				  int fast_yuv_en,
				  int enable_2pass,
				  int session_index)
{
	uint32_t pass2_src_format, pass1_dst_format, dst_format;
	int is_tile = 0, post_pass1_buf_is_planar = 0;
	unsigned int status;
	int post_pass1_ystride = info->src_rect.w >> info->downscale_ratio;
	int post_pass1_height = info->src_rect.h >> info->downscale_ratio;

	/* DST format = SRC format for non-tiled SRC formats
	 * when fast YUV is enabled. For TILED formats,
	 * DST format of MDP_Y_CRCB_H2V2_TILE = MDP_Y_CRCB_H2V2
	 * DST format of MDP_Y_CBCR_H2V2_TILE = MDP_Y_CBCR_H2V2
	 */
	switch (info->src.format) {
	case MDP_Y_CRCB_H2V2_TILE:
		is_tile = 1;
		dst_format = MDP_Y_CRCB_H2V2;
		pass1_dst_format = MDP_Y_CRCB_H2V2;
		pass2_src_format = pass1_dst_format;
		break;
	case MDP_Y_CR_CB_H2V2:
	case MDP_Y_CR_CB_GH2V2:
	case MDP_Y_CB_CR_H2V2:
		post_pass1_buf_is_planar = 1;
	case MDP_Y_CRCB_H2V2:
	case MDP_Y_CBCR_H2V2:
		dst_format = info->src.format;
		pass1_dst_format = info->src.format;
		pass2_src_format = pass1_dst_format;
		break;
	case MDP_Y_CBCR_H2V2_TILE:
		is_tile = 1;
		dst_format = MDP_Y_CBCR_H2V2;
		pass1_dst_format = info->src.format;
		pass2_src_format = pass1_dst_format;
		break;
	default:
		return -EINVAL;
	}
	if (info->dst.format  != dst_format)
		return -EINVAL;

	/* Beginning of Pass-1 */
	/* rotator expects YCbCr for planar input format */
	if ((info->src.format == MDP_Y_CR_CB_H2V2 ||
	    info->src.format == MDP_Y_CR_CB_GH2V2) &&
	    rotator_hw_revision < ROTATOR_REVISION_V2)
		swap(in_chroma_paddr, in_chroma2_paddr);

	iowrite32(in_paddr, MSM_ROTATOR_SRCP0_ADDR);
	iowrite32(in_chroma_paddr, MSM_ROTATOR_SRCP1_ADDR);
	iowrite32(in_chroma2_paddr, MSM_ROTATOR_SRCP2_ADDR);

	if (new_session) {
		if (in_chroma2_paddr) {
			if (info->src.format == MDP_Y_CR_CB_GH2V2) {
				iowrite32(ALIGN(info->src.width, 16) |
					ALIGN((info->src.width / 2), 16) << 16,
					MSM_ROTATOR_SRC_YSTRIDE1);
				iowrite32(ALIGN((info->src.width / 2), 16),
					MSM_ROTATOR_SRC_YSTRIDE2);
			} else {
				iowrite32(info->src.width |
					(info->src.width / 2) << 16,
					MSM_ROTATOR_SRC_YSTRIDE1);
				iowrite32((info->src.width / 2),
					MSM_ROTATOR_SRC_YSTRIDE2);
			}
		} else {
			iowrite32(info->src.width |
					info->src.width << 16,
					MSM_ROTATOR_SRC_YSTRIDE1);
		}
	}

	pr_debug("src_rect.w=%i src_rect.h=%i src_rect.x=%i src_rect.y=%i",
		info->src_rect.w, info->src_rect.h, info->src_rect.x,
		info->src_rect.y);
	pr_debug("src.width=%i src.height=%i src_format=%i",
		info->src.width, info->src.height, info->src.format);
	pr_debug("dst_width=%i dst_height=%i dst.x=%i dst.y=%i",
		info->dst.width, info->dst.height, info->dst_x, info->dst_y);
	pr_debug("post_pass1_ystride=%i post_pass1_height=%i downscale=%i",
		post_pass1_ystride, post_pass1_height, info->downscale_ratio);

	rotator_allocate_2pass_buf(mrd->y_rot_buf, session_index);
	rotator_allocate_2pass_buf(mrd->chroma_rot_buf, session_index);
	if (post_pass1_buf_is_planar)
		rotator_allocate_2pass_buf(mrd->chroma2_rot_buf, session_index);

	iowrite32(mrd->y_rot_buf->write_addr, MSM_ROTATOR_OUTP0_ADDR);
	iowrite32(mrd->chroma_rot_buf->write_addr, MSM_ROTATOR_OUTP1_ADDR);
	if (post_pass1_buf_is_planar)
		iowrite32(mrd->chroma2_rot_buf->write_addr,
				MSM_ROTATOR_OUTP2_ADDR);

	if (post_pass1_buf_is_planar) {
		if (pass1_dst_format == MDP_Y_CR_CB_GH2V2) {
			iowrite32(ALIGN(post_pass1_ystride, 16) |
				ALIGN((post_pass1_ystride / 2), 16) << 16,
				MSM_ROTATOR_OUT_YSTRIDE1);
			iowrite32(ALIGN((post_pass1_ystride / 2), 16),
				MSM_ROTATOR_OUT_YSTRIDE2);
		} else {
			iowrite32(post_pass1_ystride |
					post_pass1_ystride / 2 << 16,
					MSM_ROTATOR_OUT_YSTRIDE1);
			iowrite32(post_pass1_ystride / 2,
					MSM_ROTATOR_OUT_YSTRIDE2);
		}
	} else {
		iowrite32(post_pass1_ystride |
				post_pass1_ystride << 16,
				MSM_ROTATOR_OUT_YSTRIDE1);
	}

	if (pass1_dst_format == MDP_Y_CBCR_H2V2 ||
		pass1_dst_format == MDP_Y_CB_CR_H2V2) {
		iowrite32(GET_PACK_PATTERN(0, 0, CLR_CB, CLR_CR, 8),
			  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
		iowrite32(GET_PACK_PATTERN(0, 0, CLR_CB, CLR_CR, 8),
			  MSM_ROTATOR_OUT_PACK_PATTERN1);
	} else {
		iowrite32(GET_PACK_PATTERN(0, 0, CLR_CR, CLR_CB, 8),
			  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
		iowrite32(GET_PACK_PATTERN(0, 0, CLR_CR, CLR_CB, 8),
			  MSM_ROTATOR_OUT_PACK_PATTERN1);
	}

	iowrite32((3  << 18) |		/* chroma sampling 3=4:2:0 */
		   0 << 9 |		/* Pass-1 No Rotation */
		   1 << 8 |			/* ROT_EN */
		   fast_yuv_en << 4 |		/*fast YUV*/
		   info->downscale_ratio << 2 |	/* downscale v ratio */
		   info->downscale_ratio,	/* downscale h ratio */
		   MSM_ROTATOR_SUB_BLOCK_CFG);

	iowrite32((is_tile ? 2 : 0) << 29 |  /* frame format */
		  (use_imem ? 0 : 1) << 22 | /* tile size */
		  (in_chroma2_paddr ? 1 : 2) << 19 | /* fetch planes */
		  0 << 18 |		/* unpack align */
		  1 << 17 |		/* unpack tight */
		  1 << 13 |		/* unpack count 0=1 component */
		  0 << 9  |		/* src Bpp 0=1 byte ... */
		  0 << 8  |		/* has alpha */
		  0 << 6  |		/* alpha bits 3=8bits */
		  3 << 4  |		/* R/Cr bits 1=5 2=6 3=8 */
		  3 << 2  |		/* B/Cb bits 1=5 2=6 3=8 */
		  3 << 0,		/* G/Y  bits 1=5 2=6 3=8 */
		  MSM_ROTATOR_SRC_FORMAT);

	iowrite32(3, MSM_ROTATOR_INTR_ENABLE);

	msm_rotator_dev->processing = 1;
	iowrite32(0x1, MSM_ROTATOR_START);
	mutex_unlock(&msm_rotator_dev->rotator_lock);
	/* End of Pass-1 */
	wait_event(msm_rotator_dev->wq,
		   (msm_rotator_dev->processing == 0));
	/* Beginning of Pass-2 */
	mutex_lock(&msm_rotator_dev->rotator_lock);
	status = (unsigned char)ioread32(MSM_ROTATOR_INTR_STATUS);
	if ((status & 0x03) != 0x01) {
		pr_err("%s(): AXI Bus Error, issuing SW_RESET\n",
							__func__);
		iowrite32(0x1, MSM_ROTATOR_SW_RESET);
	}
	iowrite32(0, MSM_ROTATOR_INTR_ENABLE);
	iowrite32(3, MSM_ROTATOR_INTR_CLEAR);

	if (use_imem)
		iowrite32(0x42, MSM_ROTATOR_MAX_BURST_SIZE);

	iowrite32(((post_pass1_height & 0x1fff)
				<< 16) |
		  (post_pass1_ystride & 0x1fff),
		  MSM_ROTATOR_SRC_SIZE);
	iowrite32(0 << 16 | 0,
		  MSM_ROTATOR_SRC_XY);
	iowrite32(((post_pass1_height & 0x1fff)
				<< 16) |
		  (post_pass1_ystride & 0x1fff),
		  MSM_ROTATOR_SRC_IMAGE_SIZE);

	/* rotator expects YCbCr for planar input format */
	if ((pass2_src_format == MDP_Y_CR_CB_H2V2 ||
	    pass2_src_format == MDP_Y_CR_CB_GH2V2) &&
	    rotator_hw_revision < ROTATOR_REVISION_V2)
		swap(mrd->chroma_rot_buf->read_addr,
				mrd->chroma2_rot_buf->read_addr);

	iowrite32(mrd->y_rot_buf->read_addr, MSM_ROTATOR_SRCP0_ADDR);
	iowrite32(mrd->chroma_rot_buf->read_addr,
						MSM_ROTATOR_SRCP1_ADDR);
	if (mrd->chroma2_rot_buf->read_addr)
		iowrite32(mrd->chroma2_rot_buf->read_addr,
						MSM_ROTATOR_SRCP2_ADDR);

	if (post_pass1_buf_is_planar) {
		if (pass2_src_format == MDP_Y_CR_CB_GH2V2) {
			iowrite32(ALIGN(post_pass1_ystride, 16) |
				ALIGN((post_pass1_ystride / 2), 16) << 16,
				MSM_ROTATOR_SRC_YSTRIDE1);
			iowrite32(ALIGN((post_pass1_ystride / 2), 16),
				MSM_ROTATOR_SRC_YSTRIDE2);
		} else {
			iowrite32(post_pass1_ystride |
				(post_pass1_ystride / 2) << 16,
				MSM_ROTATOR_SRC_YSTRIDE1);
			iowrite32((post_pass1_ystride / 2),
				MSM_ROTATOR_SRC_YSTRIDE2);
		}
	} else {
		iowrite32(post_pass1_ystride |
				post_pass1_ystride << 16,
				MSM_ROTATOR_SRC_YSTRIDE1);
	}

	iowrite32(out_paddr +
			((info->dst_y * info->dst.width) + info->dst_x),
		  MSM_ROTATOR_OUTP0_ADDR);
	iowrite32(out_chroma_paddr +
			(((info->dst_y * info->dst.width)/2) + info->dst_x),
		  MSM_ROTATOR_OUTP1_ADDR);
	if (out_chroma2_paddr)
		iowrite32(out_chroma2_paddr +
			(((info->dst_y * info->dst.width)/2) + info->dst_x),
			  MSM_ROTATOR_OUTP2_ADDR);


	if (new_session) {
		if (out_chroma2_paddr) {
			if (info->dst.format == MDP_Y_CR_CB_GH2V2) {
				iowrite32(ALIGN(info->dst.width, 16) |
					ALIGN((info->dst.width / 2), 16) << 16,
					MSM_ROTATOR_OUT_YSTRIDE1);
				iowrite32(ALIGN((info->dst.width / 2), 16),
					MSM_ROTATOR_OUT_YSTRIDE2);
			} else {
				iowrite32(info->dst.width |
						info->dst.width/2 << 16,
						MSM_ROTATOR_OUT_YSTRIDE1);
				iowrite32(info->dst.width/2,
						MSM_ROTATOR_OUT_YSTRIDE2);
			}
		} else {
			iowrite32(info->dst.width |
					info->dst.width << 16,
					MSM_ROTATOR_OUT_YSTRIDE1);
		}

		if (dst_format == MDP_Y_CBCR_H2V2 ||
			dst_format == MDP_Y_CB_CR_H2V2) {
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CB, CLR_CR, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CB, CLR_CR, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
		} else {
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CR, CLR_CB, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CR, CLR_CB, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
		}

		iowrite32((3  << 18) |	/* chroma sampling 3=4:2:0 */
			  (ROTATIONS_TO_BITMASK(info->rotations) << 9) |
			  1 << 8 |			/* ROT_EN */
			  fast_yuv_en << 4 |		/*fast YUV*/
			  0 << 2 |	/* No downscale in Pass-2 */
			  0,	/* No downscale in Pass-2 */
			  MSM_ROTATOR_SUB_BLOCK_CFG);

		iowrite32(0 << 29 |
				/* Output of Pass-1 will always be non-tiled */
			  (use_imem ? 0 : 1) << 22 | /* tile size */
			  (in_chroma2_paddr ? 1 : 2) << 19 | /* fetch planes */
			  0 << 18 | 		/* unpack align */
			  1 << 17 | 		/* unpack tight */
			  1 << 13 | 		/* unpack count 0=1 component */
			  0 << 9  |		/* src Bpp 0=1 byte ... */
			  0 << 8  | 		/* has alpha */
			  0 << 6  | 		/* alpha bits 3=8bits */
			  3 << 4  | 		/* R/Cr bits 1=5 2=6 3=8 */
			  3 << 2  | 		/* B/Cb bits 1=5 2=6 3=8 */
			  3 << 0,   		/* G/Y  bits 1=5 2=6 3=8 */
			  MSM_ROTATOR_SRC_FORMAT);
	}
	return 0;
}

static int msm_rotator_ycxycx(struct msm_rotator_img_info *info,
			      unsigned int in_paddr,
			      unsigned int out_paddr,
			      unsigned int use_imem,
			      int new_session,
			      unsigned int out_chroma_paddr)
{
	int bpp;
	uint32_t dst_format;

	switch (info->src.format) {
	case MDP_YCBYCR_H2V1:
		if (info->rotations & MDP_ROT_90)
			dst_format = MDP_Y_CBCR_H1V2;
		else
			dst_format = MDP_Y_CBCR_H2V1;
		break;
	case MDP_YCRYCB_H2V1:
		if (info->rotations & MDP_ROT_90)
			dst_format = MDP_Y_CRCB_H1V2;
		else
			dst_format = MDP_Y_CRCB_H2V1;
		break;
	default:
		return -EINVAL;
	}

	if (info->dst.format != dst_format)
		return -EINVAL;

	bpp = get_bpp(info->src.format);
	if (bpp < 0)
		return -ENOTTY;

	iowrite32(in_paddr, MSM_ROTATOR_SRCP0_ADDR);
	iowrite32(out_paddr +
			((info->dst_y * info->dst.width) + info->dst_x),
		  MSM_ROTATOR_OUTP0_ADDR);
	iowrite32(out_chroma_paddr +
			((info->dst_y * info->dst.width)/2 + info->dst_x),
		  MSM_ROTATOR_OUTP1_ADDR);

	if (new_session) {
		iowrite32(info->src.width * bpp,
			  MSM_ROTATOR_SRC_YSTRIDE1);
		if (info->rotations & MDP_ROT_90)
			iowrite32(info->dst.width |
				  (info->dst.width*2) << 16,
				  MSM_ROTATOR_OUT_YSTRIDE1);
		else
			iowrite32(info->dst.width |
				  (info->dst.width) << 16,
				  MSM_ROTATOR_OUT_YSTRIDE1);

		if (dst_format == MDP_Y_CBCR_H1V2 ||
			dst_format == MDP_Y_CBCR_H2V1) {
			iowrite32(GET_PACK_PATTERN(0, CLR_CB, 0, CLR_CR, 8),
					MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CB, CLR_CR, 8),
					MSM_ROTATOR_OUT_PACK_PATTERN1);
		} else {
			iowrite32(GET_PACK_PATTERN(0, CLR_CR, 0, CLR_CB, 8),
					MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, 0, CLR_CR, CLR_CB, 8),
					MSM_ROTATOR_OUT_PACK_PATTERN1);
		}
		iowrite32((1  << 18) | 		/* chroma sampling 1=H2V1 */
			  (ROTATIONS_TO_BITMASK(info->rotations) << 9) |
			  1 << 8 |			/* ROT_EN */
			  info->downscale_ratio << 2 |	/* downscale v ratio */
			  info->downscale_ratio,	/* downscale h ratio */
			  MSM_ROTATOR_SUB_BLOCK_CFG);
		iowrite32(0 << 29 | 		/* frame format 0 = linear */
			  (use_imem ? 0 : 1) << 22 | /* tile size */
			  0 << 19 | 		/* fetch planes 0=interleaved */
			  0 << 18 | 		/* unpack align */
			  1 << 17 | 		/* unpack tight */
			  3 << 13 | 		/* unpack count 0=1 component */
			  (bpp-1) << 9 |	/* src Bpp 0=1 byte ... */
			  0 << 8  | 		/* has alpha */
			  0 << 6  | 		/* alpha bits 3=8bits */
			  3 << 4  | 		/* R/Cr bits 1=5 2=6 3=8 */
			  3 << 2  | 		/* B/Cb bits 1=5 2=6 3=8 */
			  3 << 0,   		/* G/Y  bits 1=5 2=6 3=8 */
			  MSM_ROTATOR_SRC_FORMAT);
	}

	return 0;
}

static int msm_rotator_rgb_types(struct msm_rotator_img_info *info,
				 unsigned int in_paddr,
				 unsigned int out_paddr,
				 unsigned int use_imem,
				 int new_session)
{
	int bpp, abits, rbits, gbits, bbits;

	if (info->src.format != info->dst.format)
		return -EINVAL;

	bpp = get_bpp(info->src.format);
	if (bpp < 0)
		return -ENOTTY;

	iowrite32(in_paddr, MSM_ROTATOR_SRCP0_ADDR);
	iowrite32(out_paddr +
			((info->dst_y * info->dst.width) + info->dst_x) * bpp,
		  MSM_ROTATOR_OUTP0_ADDR);

	if (new_session) {
		iowrite32(info->src.width * bpp, MSM_ROTATOR_SRC_YSTRIDE1);
		iowrite32(info->dst.width * bpp, MSM_ROTATOR_OUT_YSTRIDE1);
		iowrite32((0  << 18) | 		/* chroma sampling 0=rgb */
			  (ROTATIONS_TO_BITMASK(info->rotations) << 9) |
			  1 << 8 |			/* ROT_EN */
			  info->downscale_ratio << 2 |	/* downscale v ratio */
			  info->downscale_ratio,	/* downscale h ratio */
			  MSM_ROTATOR_SUB_BLOCK_CFG);
		switch (info->src.format) {
		case MDP_RGB_565:
			iowrite32(GET_PACK_PATTERN(0, CLR_R, CLR_G, CLR_B, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, CLR_R, CLR_G, CLR_B, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
			abits = 0;
			rbits = COMPONENT_5BITS;
			gbits = COMPONENT_6BITS;
			bbits = COMPONENT_5BITS;
			break;

		case MDP_BGR_565:
			iowrite32(GET_PACK_PATTERN(0, CLR_B, CLR_G, CLR_R, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, CLR_B, CLR_G, CLR_R, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
			abits = 0;
			rbits = COMPONENT_5BITS;
			gbits = COMPONENT_6BITS;
			bbits = COMPONENT_5BITS;
			break;

		case MDP_RGB_888:
		case MDP_YCBCR_H1V1:
		case MDP_YCRCB_H1V1:
			iowrite32(GET_PACK_PATTERN(0, CLR_R, CLR_G, CLR_B, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(0, CLR_R, CLR_G, CLR_B, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
			abits = 0;
			rbits = COMPONENT_8BITS;
			gbits = COMPONENT_8BITS;
			bbits = COMPONENT_8BITS;
			break;

		case MDP_ARGB_8888:
		case MDP_RGBA_8888:
		case MDP_XRGB_8888:
		case MDP_RGBX_8888:
			iowrite32(GET_PACK_PATTERN(CLR_ALPHA, CLR_R, CLR_G,
						   CLR_B, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(CLR_ALPHA, CLR_R, CLR_G,
						   CLR_B, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
			abits = COMPONENT_8BITS;
			rbits = COMPONENT_8BITS;
			gbits = COMPONENT_8BITS;
			bbits = COMPONENT_8BITS;
			break;

		case MDP_BGRA_8888:
			iowrite32(GET_PACK_PATTERN(CLR_ALPHA, CLR_B, CLR_G,
						   CLR_R, 8),
				  MSM_ROTATOR_SRC_UNPACK_PATTERN1);
			iowrite32(GET_PACK_PATTERN(CLR_ALPHA, CLR_B, CLR_G,
						   CLR_R, 8),
				  MSM_ROTATOR_OUT_PACK_PATTERN1);
			abits = COMPONENT_8BITS;
			rbits = COMPONENT_8BITS;
			gbits = COMPONENT_8BITS;
			bbits = COMPONENT_8BITS;
			break;

		default:
			return -EINVAL;
		}
		iowrite32(0 << 29 | 		/* frame format 0 = linear */
			  (use_imem ? 0 : 1) << 22 | /* tile size */
			  0 << 19 | 		/* fetch planes 0=interleaved */
			  0 << 18 | 		/* unpack align */
			  1 << 17 | 		/* unpack tight */
			  (abits ? 3 : 2) << 13 | /* unpack count 0=1 comp */
			  (bpp-1) << 9 | 	/* src Bpp 0=1 byte ... */
			  (abits ? 1 : 0) << 8  | /* has alpha */
			  abits << 6  | 	/* alpha bits 3=8bits */
			  rbits << 4  | 	/* R/Cr bits 1=5 2=6 3=8 */
			  bbits << 2  | 	/* B/Cb bits 1=5 2=6 3=8 */
			  gbits << 0,   	/* G/Y  bits 1=5 2=6 3=8 */
			  MSM_ROTATOR_SRC_FORMAT);
	}

	return 0;
}

static int get_img(struct msmfb_data *fbd, int domain,
	unsigned long *start, unsigned long *len, struct file **p_file,
	int *p_need, struct ion_handle **p_ihdl, unsigned int secure)
{
	int ret = 0;
#ifdef CONFIG_FB
	struct file *file = NULL;
	int put_needed, fb_num;
#endif
#ifdef CONFIG_ANDROID_PMEM
	unsigned long vstart;
#endif

	*p_need = 0;

#ifdef CONFIG_FB
	if (fbd->flags & MDP_MEMORY_ID_TYPE_FB) {
		file = fget_light(fbd->memory_id, &put_needed);
		if (file == NULL) {
			pr_err("fget_light returned NULL\n");
			return -EINVAL;
		}

		if (MAJOR(file->f_dentry->d_inode->i_rdev) == FB_MAJOR) {
			fb_num = MINOR(file->f_dentry->d_inode->i_rdev);
			if (get_fb_phys_info(start, len, fb_num,
				ROTATOR_SUBSYSTEM_ID)) {
				pr_err("get_fb_phys_info() failed\n");
				ret = -1;
			} else {
				*p_file = file;
				*p_need = put_needed;
			}
		} else {
			pr_err("invalid FB_MAJOR failed\n");
			ret = -1;
		}
		if (ret)
			fput_light(file, put_needed);
		return ret;
	}
#endif

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	return msm_rotator_iommu_map_buf(fbd->memory_id, domain, start,
		len, p_ihdl, secure);
#endif
#ifdef CONFIG_ANDROID_PMEM
	if (!get_pmem_file(fbd->memory_id, start, &vstart, len, p_file))
		return 0;
	else
		return -ENOMEM;
#endif

}

static void put_img(struct file *p_file, struct ion_handle *p_ihdl,
	int domain, unsigned int secure)
{
#ifdef CONFIG_ANDROID_PMEM
	if (p_file != NULL)
		put_pmem_file(p_file);
#endif

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	if (!IS_ERR_OR_NULL(p_ihdl)) {
		pr_debug("%s(): p_ihdl %p\n", __func__, p_ihdl);
		if (rot_iommu_split_domain) {
			if (!secure)
				ion_unmap_iommu(msm_rotator_dev->client,
					p_ihdl, domain, GEN_POOL);
		} else {
			ion_unmap_iommu(msm_rotator_dev->client,
				p_ihdl, ROTATOR_SRC_DOMAIN, GEN_POOL);
		}

		ion_free(msm_rotator_dev->client, p_ihdl);
	}
#endif
}

static int msm_rotator_rotate_prepare(
	struct msm_rotator_data_info *data_info,
	struct msm_rotator_commit_info *commit_info)
{
	unsigned int format;
	struct msm_rotator_data_info info;
	unsigned int in_paddr, out_paddr;
	unsigned long src_len, dst_len;
	int rc = 0, s;
	struct file *srcp0_file = NULL, *dstp0_file = NULL;
	struct file *srcp1_file = NULL, *dstp1_file = NULL;
	struct ion_handle *srcp0_ihdl = NULL, *dstp0_ihdl = NULL;
	struct ion_handle *srcp1_ihdl = NULL, *dstp1_ihdl = NULL;
	int ps0_need, p_need;
	unsigned int in_chroma_paddr = 0, out_chroma_paddr = 0;
	unsigned int in_chroma2_paddr = 0, out_chroma2_paddr = 0;
	struct msm_rotator_img_info *img_info;
	struct msm_rotator_mem_planes src_planes, dst_planes;

	mutex_lock(&msm_rotator_dev->rotator_lock);
	info = *data_info;

	for (s = 0; s < MAX_SESSIONS; s++)
		if ((msm_rotator_dev->rot_session[s] != NULL) &&
			(info.session_id ==
			(unsigned int)msm_rotator_dev->rot_session[s]
			))
			break;

	if (s == MAX_SESSIONS) {
		pr_err("%s() : Attempt to use invalid session_id %d\n",
			__func__, s);
		rc = -EINVAL;
		mutex_unlock(&msm_rotator_dev->rotator_lock);
		return rc;
	}

	img_info = &(msm_rotator_dev->rot_session[s]->img_info);
	if (img_info->enable == 0) {
		dev_dbg(msm_rotator_dev->device,
			"%s() : Session_id %d not enabled\n", __func__, s);
		rc = -EINVAL;
		mutex_unlock(&msm_rotator_dev->rotator_lock);
		return rc;
	}

	if (msm_rotator_get_plane_sizes(img_info->src.format,
					img_info->src.width,
					img_info->src.height,
					&src_planes)) {
		pr_err("%s: invalid src format\n", __func__);
		rc = -EINVAL;
		mutex_unlock(&msm_rotator_dev->rotator_lock);
		return rc;
	}
	if (msm_rotator_get_plane_sizes(img_info->dst.format,
					img_info->dst.width,
					img_info->dst.height,
					&dst_planes)) {
		pr_err("%s: invalid dst format\n", __func__);
		rc = -EINVAL;
		mutex_unlock(&msm_rotator_dev->rotator_lock);
		return rc;
	}

	rc = get_img(&info.src, ROTATOR_SRC_DOMAIN, (unsigned long *)&in_paddr,
			(unsigned long *)&src_len, &srcp0_file, &ps0_need,
			&srcp0_ihdl, 0);
	if (rc) {
		pr_err("%s: in get_img() failed id=0x%08x\n",
			DRIVER_NAME, info.src.memory_id);
		goto rotate_prepare_error;
	}

	rc = get_img(&info.dst, ROTATOR_DST_DOMAIN, (unsigned long *)&out_paddr,
			(unsigned long *)&dst_len, &dstp0_file, &p_need,
			&dstp0_ihdl, img_info->secure);
	if (rc) {
		pr_err("%s: out get_img() failed id=0x%08x\n",
		       DRIVER_NAME, info.dst.memory_id);
		goto rotate_prepare_error;
	}

	format = img_info->src.format;
	if (((info.version_key & VERSION_KEY_MASK) == 0xA5B4C300) &&
			((info.version_key & ~VERSION_KEY_MASK) > 0) &&
			(src_planes.num_planes == 2)) {
		if (checkoffset(info.src.offset,
				src_planes.plane_size[0],
				src_len)) {
			pr_err("%s: invalid src buffer (len=%lu offset=%x)\n",
			       __func__, src_len, info.src.offset);
			rc = -ERANGE;
			goto rotate_prepare_error;
		}
		if (checkoffset(info.dst.offset,
				dst_planes.plane_size[0],
				dst_len)) {
			pr_err("%s: invalid dst buffer (len=%lu offset=%x)\n",
			       __func__, dst_len, info.dst.offset);
			rc = -ERANGE;
			goto rotate_prepare_error;
		}

		rc = get_img(&info.src_chroma, ROTATOR_SRC_DOMAIN,
				(unsigned long *)&in_chroma_paddr,
				(unsigned long *)&src_len, &srcp1_file, &p_need,
				&srcp1_ihdl, 0);
		if (rc) {
			pr_err("%s: in chroma get_img() failed id=0x%08x\n",
				DRIVER_NAME, info.src_chroma.memory_id);
			goto rotate_prepare_error;
		}

		rc = get_img(&info.dst_chroma, ROTATOR_DST_DOMAIN,
				(unsigned long *)&out_chroma_paddr,
				(unsigned long *)&dst_len, &dstp1_file, &p_need,
				&dstp1_ihdl, img_info->secure);
		if (rc) {
			pr_err("%s: out chroma get_img() failed id=0x%08x\n",
				DRIVER_NAME, info.dst_chroma.memory_id);
			goto rotate_prepare_error;
		}

		if (checkoffset(info.src_chroma.offset,
				src_planes.plane_size[1],
				src_len)) {
			pr_err("%s: invalid chr src buf len=%lu offset=%x\n",
			       __func__, src_len, info.src_chroma.offset);
			rc = -ERANGE;
			goto rotate_prepare_error;
		}

		if (checkoffset(info.dst_chroma.offset,
				src_planes.plane_size[1],
				dst_len)) {
			pr_err("%s: invalid chr dst buf len=%lu offset=%x\n",
			       __func__, dst_len, info.dst_chroma.offset);
			rc = -ERANGE;
			goto rotate_prepare_error;
		}

		in_chroma_paddr += info.src_chroma.offset;
		out_chroma_paddr += info.dst_chroma.offset;
	} else {
		if (checkoffset(info.src.offset,
				src_planes.total_size,
				src_len)) {
			pr_err("%s: invalid src buffer (len=%lu offset=%x)\n",
			       __func__, src_len, info.src.offset);
			rc = -ERANGE;
			goto rotate_prepare_error;
		}
		if (checkoffset(info.dst.offset,
				dst_planes.total_size,
				dst_len)) {
			pr_err("%s: invalid dst buffer (len=%lu offset=%x)\n",
			       __func__, dst_len, info.dst.offset);
			rc = -ERANGE;
			goto rotate_prepare_error;
		}
	}

	in_paddr += info.src.offset;
	out_paddr += info.dst.offset;

	if (!in_chroma_paddr && src_planes.num_planes >= 2)
		in_chroma_paddr = in_paddr + src_planes.plane_size[0];
	if (!out_chroma_paddr && dst_planes.num_planes >= 2)
		out_chroma_paddr = out_paddr + dst_planes.plane_size[0];
	if (src_planes.num_planes >= 3)
		in_chroma2_paddr = in_chroma_paddr + src_planes.plane_size[1];
	if (dst_planes.num_planes >= 3)
		out_chroma2_paddr = out_chroma_paddr + dst_planes.plane_size[1];

	commit_info->data_info = info;
	commit_info->img_info = *img_info;
	commit_info->format = format;
	commit_info->in_paddr = in_paddr;
	commit_info->out_paddr = out_paddr;
	commit_info->in_chroma_paddr = in_chroma_paddr;
	commit_info->out_chroma_paddr = out_chroma_paddr;
	commit_info->in_chroma2_paddr = in_chroma2_paddr;
	commit_info->out_chroma2_paddr = out_chroma2_paddr;
	commit_info->srcp0_file = srcp0_file;
	commit_info->srcp1_file = srcp1_file;
	commit_info->srcp0_ihdl = srcp0_ihdl;
	commit_info->srcp1_ihdl = srcp1_ihdl;
	commit_info->dstp0_file = dstp0_file;
	commit_info->dstp0_ihdl = dstp0_ihdl;
	commit_info->dstp1_file = dstp1_file;
	commit_info->dstp1_ihdl = dstp1_ihdl;
	commit_info->ps0_need = ps0_need;
	commit_info->session_index = s;
	commit_info->acq_fen = msm_rotator_dev->sync_info[s].acq_fen;
	commit_info->fast_yuv_en = mrd->rot_session[s]->fast_yuv_enable;
	commit_info->enable_2pass = mrd->rot_session[s]->enable_2pass;
	mutex_unlock(&msm_rotator_dev->rotator_lock);
	return 0;

rotate_prepare_error:
	put_img(dstp1_file, dstp1_ihdl, ROTATOR_DST_DOMAIN,
		msm_rotator_dev->rot_session[s]->img_info.secure);
	put_img(srcp1_file, srcp1_ihdl, ROTATOR_SRC_DOMAIN, 0);
	put_img(dstp0_file, dstp0_ihdl, ROTATOR_DST_DOMAIN,
		msm_rotator_dev->rot_session[s]->img_info.secure);

	/* only source may use frame buffer */
	if (info.src.flags & MDP_MEMORY_ID_TYPE_FB)
		fput_light(srcp0_file, ps0_need);
	else
		put_img(srcp0_file, srcp0_ihdl, ROTATOR_SRC_DOMAIN, 0);
	dev_dbg(msm_rotator_dev->device, "%s() returning rc = %d\n",
		__func__, rc);
	mutex_unlock(&msm_rotator_dev->rotator_lock);
	return rc;
}

static int msm_rotator_do_rotate_sub(
	struct msm_rotator_commit_info *commit_info)
{
	unsigned int status, format;
	struct msm_rotator_data_info info;
	unsigned int in_paddr, out_paddr;
	int use_imem = 0, rc = 0;
	struct file *srcp0_file, *dstp0_file;
	struct file *srcp1_file, *dstp1_file;
	struct ion_handle *srcp0_ihdl, *dstp0_ihdl;
	struct ion_handle *srcp1_ihdl, *dstp1_ihdl;
	int s, ps0_need;
	unsigned int in_chroma_paddr, out_chroma_paddr;
	unsigned int in_chroma2_paddr, out_chroma2_paddr;
	struct msm_rotator_img_info *img_info;

	mutex_lock(&msm_rotator_dev->rotator_lock);

	info = commit_info->data_info;
	img_info = &commit_info->img_info;
	format = commit_info->format;
	in_paddr = commit_info->in_paddr;
	out_paddr = commit_info->out_paddr;
	in_chroma_paddr = commit_info->in_chroma_paddr;
	out_chroma_paddr = commit_info->out_chroma_paddr;
	in_chroma2_paddr = commit_info->in_chroma2_paddr;
	out_chroma2_paddr = commit_info->out_chroma2_paddr;
	srcp0_file = commit_info->srcp0_file;
	srcp1_file = commit_info->srcp1_file;
	srcp0_ihdl = commit_info->srcp0_ihdl;
	srcp1_ihdl = commit_info->srcp1_ihdl;
	dstp0_file = commit_info->dstp0_file;
	dstp0_ihdl = commit_info->dstp0_ihdl;
	dstp1_file = commit_info->dstp1_file;
	dstp1_ihdl = commit_info->dstp1_ihdl;
	ps0_need = commit_info->ps0_need;
	s = commit_info->session_index;

	msm_rotator_wait_for_fence(commit_info->acq_fen);
	commit_info->acq_fen = NULL;

	cancel_delayed_work(&msm_rotator_dev->rot_clk_work);
	if (msm_rotator_dev->rot_clk_state != CLK_EN) {
		enable_rot_clks();
		msm_rotator_dev->rot_clk_state = CLK_EN;
	}
	enable_irq(msm_rotator_dev->irq);

#ifdef CONFIG_MSM_ROTATOR_USE_IMEM
	use_imem = msm_rotator_imem_allocate(ROTATOR_REQUEST);
#else
	use_imem = 0;
#endif
	/*
	 * workaround for a hardware bug. rotator hardware hangs when we
	 * use write burst beat size 16 on 128X128 tile fetch mode. As a
	 * temporary fix use 0x42 for BURST_SIZE when imem used.
	 */
	if (use_imem)
		iowrite32(0x42, MSM_ROTATOR_MAX_BURST_SIZE);

	iowrite32(((img_info->src_rect.h & 0x1fff)
				<< 16) |
		  (img_info->src_rect.w & 0x1fff),
		  MSM_ROTATOR_SRC_SIZE);
	iowrite32(((img_info->src_rect.y & 0x1fff)
				<< 16) |
		  (img_info->src_rect.x & 0x1fff),
		  MSM_ROTATOR_SRC_XY);
	iowrite32(((img_info->src.height & 0x1fff)
				<< 16) |
		  (img_info->src.width & 0x1fff),
		  MSM_ROTATOR_SRC_IMAGE_SIZE);

	switch (format) {
	case MDP_RGB_565:
	case MDP_BGR_565:
	case MDP_RGB_888:
	case MDP_ARGB_8888:
	case MDP_RGBA_8888:
	case MDP_XRGB_8888:
	case MDP_BGRA_8888:
	case MDP_RGBX_8888:
	case MDP_YCBCR_H1V1:
	case MDP_YCRCB_H1V1:
		rc = msm_rotator_rgb_types(img_info,
					   in_paddr, out_paddr,
					   use_imem,
					   msm_rotator_dev->last_session_idx
								!= s);
		break;
	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CRCB_H2V2:
	case MDP_Y_CB_CR_H2V2:
	case MDP_Y_CR_CB_H2V2:
	case MDP_Y_CR_CB_GH2V2:
	case MDP_Y_CRCB_H2V2_TILE:
	case MDP_Y_CBCR_H2V2_TILE:
		if (!commit_info->enable_2pass)
			rc = msm_rotator_ycxcx_h2v2(img_info,
					    in_paddr, out_paddr, use_imem,
					    msm_rotator_dev->last_session_idx
								!= s,
					    in_chroma_paddr,
					    out_chroma_paddr,
					    in_chroma2_paddr,
					    out_chroma2_paddr,
					    commit_info->fast_yuv_en);
		else
			rc = msm_rotator_ycxcx_h2v2_2pass(img_info,
					    in_paddr, out_paddr, use_imem,
					    msm_rotator_dev->last_session_idx
								!= s,
					    in_chroma_paddr,
					    out_chroma_paddr,
					    in_chroma2_paddr,
					    out_chroma2_paddr,
					    commit_info->fast_yuv_en,
					    commit_info->enable_2pass,
					    s);
		break;
	case MDP_Y_CBCR_H2V1:
	case MDP_Y_CRCB_H2V1:
		rc = msm_rotator_ycxcx_h2v1(img_info,
					    in_paddr, out_paddr, use_imem,
					    msm_rotator_dev->last_session_idx
								!= s,
					    in_chroma_paddr,
					    out_chroma_paddr);
		break;
	case MDP_YCBYCR_H2V1:
	case MDP_YCRYCB_H2V1:
		rc = msm_rotator_ycxycx(img_info,
				in_paddr, out_paddr, use_imem,
				msm_rotator_dev->last_session_idx != s,
				out_chroma_paddr);
		break;
	default:
		rc = -EINVAL;
		pr_err("%s(): Unsupported format %u\n", __func__, format);
		goto do_rotate_exit;
	}

	if (rc != 0) {
		msm_rotator_dev->last_session_idx = INVALID_SESSION;
		pr_err("%s(): Invalid session error\n", __func__);
		goto do_rotate_exit;
	}

	iowrite32(3, MSM_ROTATOR_INTR_ENABLE);

	msm_rotator_dev->processing = 1;
	iowrite32(0x1, MSM_ROTATOR_START);
	mutex_unlock(&msm_rotator_dev->rotator_lock);
	wait_event(msm_rotator_dev->wq,
		   (msm_rotator_dev->processing == 0));
	mutex_lock(&msm_rotator_dev->rotator_lock);
	status = (unsigned char)ioread32(MSM_ROTATOR_INTR_STATUS);
	if ((status & 0x03) != 0x01) {
		pr_err("%s(): AXI Bus Error, issuing SW_RESET\n", __func__);
		iowrite32(0x1, MSM_ROTATOR_SW_RESET);
		rc = -EFAULT;
	}
	iowrite32(0, MSM_ROTATOR_INTR_ENABLE);
	iowrite32(3, MSM_ROTATOR_INTR_CLEAR);

do_rotate_exit:
	disable_irq(msm_rotator_dev->irq);
#ifdef CONFIG_MSM_ROTATOR_USE_IMEM
	msm_rotator_imem_free(ROTATOR_REQUEST);
#endif
	schedule_delayed_work(&msm_rotator_dev->rot_clk_work, HZ);
	put_img(dstp1_file, dstp1_ihdl, ROTATOR_DST_DOMAIN,
		img_info->secure);
	put_img(srcp1_file, srcp1_ihdl, ROTATOR_SRC_DOMAIN, 0);
	put_img(dstp0_file, dstp0_ihdl, ROTATOR_DST_DOMAIN,
		img_info->secure);

	/* only source may use frame buffer */
	if (info.src.flags & MDP_MEMORY_ID_TYPE_FB)
		fput_light(srcp0_file, ps0_need);
	else
		put_img(srcp0_file, srcp0_ihdl, ROTATOR_SRC_DOMAIN, 0);
	msm_rotator_signal_timeline_done(s);
	mutex_unlock(&msm_rotator_dev->rotator_lock);
	dev_dbg(msm_rotator_dev->device, "%s() returning rc = %d\n",
		__func__, rc);

	return rc;
}

static void rot_wait_for_commit_queue(u32 is_all)
{
	int ret = 0;
	u32 loop_cnt = 0;

	while (1) {
		mutex_lock(&mrd->commit_mutex);
		if (is_all && (atomic_read(&mrd->commit_q_cnt) == 0))
			break;
		if ((!is_all) &&
			(atomic_read(&mrd->commit_q_cnt) < MAX_COMMIT_QUEUE))
			break;
		INIT_COMPLETION(mrd->commit_comp);
		mutex_unlock(&mrd->commit_mutex);
		ret = wait_for_completion_interruptible_timeout(
				&mrd->commit_comp,
			msecs_to_jiffies(WAIT_ROT_TIMEOUT));
		if ((ret <= 0) ||
			(atomic_read(&mrd->commit_q_cnt) >= MAX_COMMIT_QUEUE) ||
				(loop_cnt > MAX_COMMIT_QUEUE)) {
			pr_err("%s wait for commit queue failed ret=%d pointers:%d %d",
				__func__, ret, atomic_read(&mrd->commit_q_r),
				atomic_read(&mrd->commit_q_w));
			mutex_lock(&mrd->commit_mutex);
			ret = -ETIME;
			break;
		} else {
			ret = 0;
		}
		loop_cnt++;
	};
	if (is_all || ret) {
		atomic_set(&mrd->commit_q_r, 0);
		atomic_set(&mrd->commit_q_cnt, 0);
		atomic_set(&mrd->commit_q_w, 0);
	}
	mutex_unlock(&mrd->commit_mutex);
}

static int msm_rotator_do_rotate(unsigned long arg)
{
	struct msm_rotator_data_info info;
	struct rot_sync_info *sync_info;
	int session_index, ret;
	int commit_q_w;

	if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
		return -EFAULT;

	rot_wait_for_commit_queue(false);
	mutex_lock(&mrd->commit_mutex);
	commit_q_w = atomic_read(&mrd->commit_q_w);
	ret = msm_rotator_rotate_prepare(&info,
			&mrd->commit_info[commit_q_w]);
	if (ret) {
		mutex_unlock(&mrd->commit_mutex);
		return ret;
	}

	session_index = mrd->commit_info[commit_q_w].session_index;
	sync_info = &msm_rotator_dev->sync_info[session_index];
	mutex_lock(&sync_info->sync_mutex);
	atomic_inc(&sync_info->queue_buf_cnt);
	sync_info->acq_fen = NULL;
	mutex_unlock(&sync_info->sync_mutex);

	if (atomic_inc_return(&mrd->commit_q_w) >= MAX_COMMIT_QUEUE)
		atomic_set(&mrd->commit_q_w, 0);
	atomic_inc(&mrd->commit_q_cnt);

	schedule_work(&mrd->commit_work);
	mutex_unlock(&mrd->commit_mutex);

	if (info.wait_for_finish)
		rot_wait_for_commit_queue(true);

	return 0;
}

static void rot_commit_wq_handler(struct work_struct *work)
{
	mutex_lock(&mrd->commit_wq_mutex);
	mutex_lock(&mrd->commit_mutex);
	while (atomic_read(&mrd->commit_q_cnt) > 0) {
		mrd->commit_running = true;
		mutex_unlock(&mrd->commit_mutex);
		msm_rotator_do_rotate_sub(
			&mrd->commit_info[atomic_read(&mrd->commit_q_r)]);
		mutex_lock(&mrd->commit_mutex);
		if (atomic_read(&mrd->commit_q_cnt) > 0) {
			atomic_dec(&mrd->commit_q_cnt);
			if (atomic_inc_return(&mrd->commit_q_r) >=
					MAX_COMMIT_QUEUE)
				atomic_set(&mrd->commit_q_r, 0);
		}
		complete_all(&mrd->commit_comp);
	}
	mrd->commit_running = false;
	if (atomic_read(&mrd->commit_q_r) != atomic_read(&mrd->commit_q_w))
		pr_err("%s invalid state: r=%d w=%d cnt=%d", __func__,
			atomic_read(&mrd->commit_q_r),
			atomic_read(&mrd->commit_q_w),
			atomic_read(&mrd->commit_q_cnt));
	mutex_unlock(&mrd->commit_mutex);
	mutex_unlock(&mrd->commit_wq_mutex);
}

static void msm_rotator_set_perf_level(u32 wh, u32 is_rgb)
{
	u32 perf_level;

	if (is_rgb)
		perf_level = 1;
	else if (wh <= (640 * 480))
		perf_level = 2;
	else if (wh <= (736 * 1280))
		perf_level = 3;
	else
		perf_level = 4;

#ifdef CONFIG_MSM_BUS_SCALING
	msm_bus_scale_client_update_request(msm_rotator_dev->bus_client_handle,
		perf_level);
#endif

}

static int rot_enable_iommu_clocks(struct msm_rotator_dev *rot_dev)
{
	int ret = 0, i;
	if (rot_dev->mmu_clk_on)
		return 0;
	for (i = 0; i < ARRAY_SIZE(rot_mmu_clks); i++) {
		rot_mmu_clks[i].mmu_clk = clk_get(&msm_rotator_dev->pdev->dev,
			rot_mmu_clks[i].mmu_clk_name);
		if (IS_ERR(rot_mmu_clks[i].mmu_clk)) {
			pr_err(" %s: Get failed for clk %s", __func__,
				   rot_mmu_clks[i].mmu_clk_name);
			ret = PTR_ERR(rot_mmu_clks[i].mmu_clk);
			break;
		}
		ret = clk_prepare_enable(rot_mmu_clks[i].mmu_clk);
		if (ret) {
			clk_put(rot_mmu_clks[i].mmu_clk);
			rot_mmu_clks[i].mmu_clk = NULL;
		}
	}
	if (ret) {
		for (i--; i >= 0; i--) {
			clk_disable_unprepare(rot_mmu_clks[i].mmu_clk);
			clk_put(rot_mmu_clks[i].mmu_clk);
			rot_mmu_clks[i].mmu_clk = NULL;
		}
	} else {
		rot_dev->mmu_clk_on = 1;
	}
	return ret;
}

static int rot_disable_iommu_clocks(struct msm_rotator_dev *rot_dev)
{
	int i;
	if (!rot_dev->mmu_clk_on)
		return 0;
	for (i = 0; i < ARRAY_SIZE(rot_mmu_clks); i++) {
		clk_disable_unprepare(rot_mmu_clks[i].mmu_clk);
		clk_put(rot_mmu_clks[i].mmu_clk);
		rot_mmu_clks[i].mmu_clk = NULL;
	}
	rot_dev->mmu_clk_on = 0;
	return 0;
}

static int map_sec_resource(struct msm_rotator_dev *rot_dev)
{
	int ret = 0;
	if (rot_dev->sec_mapped)
		return 0;

	ret = rot_enable_iommu_clocks(rot_dev);
	if (ret) {
		pr_err("IOMMU clock enabled failed while open");
		return ret;
	}
	ret = msm_ion_secure_heap(ION_HEAP(ION_CP_MM_HEAP_ID));
	if (ret)
		pr_err("ION heap secure failed heap id %d ret %d\n",
			   ION_CP_MM_HEAP_ID, ret);
	else
		rot_dev->sec_mapped = 1;
	rot_disable_iommu_clocks(rot_dev);
	return ret;
}

static int unmap_sec_resource(struct msm_rotator_dev *rot_dev)
{
	int ret = 0;
	ret = rot_enable_iommu_clocks(rot_dev);
	if (ret) {
		pr_err("IOMMU clock enabled failed while close\n");
		return ret;
	}
	msm_ion_unsecure_heap(ION_HEAP(ION_CP_MM_HEAP_ID));
	rot_dev->sec_mapped = 0;
	rot_disable_iommu_clocks(rot_dev);
	return ret;
}

static int msm_rotator_start(unsigned long arg,
			     struct msm_rotator_fd_info *fd_info)
{
	struct msm_rotator_img_info info;
	struct msm_rotator_session *rot_session = NULL;
	int rc = 0;
	int s, is_rgb = 0;
	int first_free_idx = INVALID_SESSION;
	unsigned int dst_w, dst_h;
	unsigned int is_planar420 = 0;
	int fast_yuv_en = 0, enable_2pass = 0;
	struct rot_sync_info *sync_info;

	if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
		return -EFAULT;

	if ((info.rotations > MSM_ROTATOR_MAX_ROT) ||
	    (info.src.height > MSM_ROTATOR_MAX_H) ||
	    (info.src.width > MSM_ROTATOR_MAX_W) ||
	    (info.dst.height > MSM_ROTATOR_MAX_H) ||
	    (info.dst.width > MSM_ROTATOR_MAX_W) ||
	    (info.downscale_ratio > MAX_DOWNSCALE_RATIO)) {
		pr_err("%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (info.rotations & MDP_ROT_90) {
		dst_w = info.src_rect.h >> info.downscale_ratio;
		dst_h = info.src_rect.w >> info.downscale_ratio;
	} else {
		dst_w = info.src_rect.w >> info.downscale_ratio;
		dst_h = info.src_rect.h >> info.downscale_ratio;
	}

	if (checkoffset(info.src_rect.x, info.src_rect.w, info.src.width) ||
	    checkoffset(info.src_rect.y, info.src_rect.h, info.src.height) ||
	    checkoffset(info.dst_x, dst_w, info.dst.width) ||
	    checkoffset(info.dst_y, dst_h, info.dst.height)) {
		pr_err("%s: Invalid src or dst rect\n", __func__);
		return -ERANGE;
	}

	switch (info.src.format) {
	case MDP_Y_CB_CR_H2V2:
	case MDP_Y_CR_CB_H2V2:
	case MDP_Y_CR_CB_GH2V2:
		is_planar420 = 1;
	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CRCB_H2V2:
	case MDP_Y_CRCB_H2V2_TILE:
	case MDP_Y_CBCR_H2V2_TILE:
		if (rotator_hw_revision >= ROTATOR_REVISION_V2) {
			if (info.downscale_ratio &&
					(info.rotations & MDP_ROT_90)) {
				fast_yuv_en = !fast_yuv_invalid_size_checker(
						0,
						info.src.width,
						info.src_rect.w >>
							info.downscale_ratio,
						info.src.height,
						info.src_rect.h >>
							info.downscale_ratio,
						info.src_rect.w >>
							info.downscale_ratio,
						is_planar420);

				fast_yuv_en = fast_yuv_en &&
						!fast_yuv_invalid_size_checker(
						info.rotations,
						info.src_rect.w >>
							info.downscale_ratio,
						dst_w,
						info.src_rect.h >>
							info.downscale_ratio,
						dst_h,
						dst_w,
						is_planar420);
			} else {
				fast_yuv_en = !fast_yuv_invalid_size_checker(
						info.rotations,
						info.src.width,
						dst_w,
						info.src.height,
						dst_h,
						dst_w,
						is_planar420);
			}

			if (fast_yuv_en && info.downscale_ratio &&
				(info.rotations & MDP_ROT_90))
					enable_2pass = 1;
		}
	break;
	default:
		fast_yuv_en = 0;
	}

	switch (info.src.format) {
	case MDP_RGB_565:
	case MDP_BGR_565:
	case MDP_RGB_888:
	case MDP_ARGB_8888:
	case MDP_RGBA_8888:
	case MDP_XRGB_8888:
	case MDP_RGBX_8888:
	case MDP_BGRA_8888:
		is_rgb = 1;
		info.dst.format = info.src.format;
		break;
	case MDP_Y_CBCR_H2V1:
	if (info.rotations & MDP_ROT_90) {
		info.dst.format = MDP_Y_CBCR_H1V2;
		break;
	}
	case MDP_Y_CRCB_H2V1:
	if (info.rotations & MDP_ROT_90) {
		info.dst.format = MDP_Y_CRCB_H1V2;
		break;
	}
	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CRCB_H2V2:
	case MDP_YCBCR_H1V1:
	case MDP_YCRCB_H1V1:
		info.dst.format = info.src.format;
		break;
	case MDP_YCBYCR_H2V1:
		if (info.rotations & MDP_ROT_90)
			info.dst.format = MDP_Y_CBCR_H1V2;
		else
			info.dst.format = MDP_Y_CBCR_H2V1;
		break;
	case MDP_YCRYCB_H2V1:
		if (info.rotations & MDP_ROT_90)
			info.dst.format = MDP_Y_CRCB_H1V2;
		else
			info.dst.format = MDP_Y_CRCB_H2V1;
		break;
	case MDP_Y_CB_CR_H2V2:
		if (fast_yuv_en) {
			info.dst.format = info.src.format;
			break;
		}
	case MDP_Y_CBCR_H2V2_TILE:
		info.dst.format = MDP_Y_CBCR_H2V2;
		break;
	case MDP_Y_CR_CB_H2V2:
	case MDP_Y_CR_CB_GH2V2:
		if (fast_yuv_en) {
			info.dst.format = info.src.format;
			break;
		}
	case MDP_Y_CRCB_H2V2_TILE:
		info.dst.format = MDP_Y_CRCB_H2V2;
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&msm_rotator_dev->rotator_lock);

	msm_rotator_set_perf_level((info.src.width*info.src.height), is_rgb);

	for (s = 0; s < MAX_SESSIONS; s++) {
		if ((msm_rotator_dev->rot_session[s] != NULL) &&
			(info.session_id ==
			(unsigned int)msm_rotator_dev->rot_session[s]
			)) {
			rot_session = msm_rotator_dev->rot_session[s];
			rot_session->img_info =	info;
			rot_session->fd_info =	*fd_info;
			rot_session->fast_yuv_enable = fast_yuv_en;
			rot_session->enable_2pass = enable_2pass;

			if (msm_rotator_dev->last_session_idx == s)
				msm_rotator_dev->last_session_idx =
				INVALID_SESSION;
			break;
		}

		if ((msm_rotator_dev->rot_session[s] == NULL) &&
			(first_free_idx == INVALID_SESSION))
				first_free_idx = s;
	}

	if ((s == MAX_SESSIONS) && (first_free_idx != INVALID_SESSION)) {
		/* allocate a session id */
		msm_rotator_dev->rot_session[first_free_idx] =
			kzalloc(sizeof(struct msm_rotator_session),
					GFP_KERNEL);
		if (!msm_rotator_dev->rot_session[first_free_idx]) {
			printk(KERN_ERR "%s : unable to alloc mem\n",
					__func__);
			rc = -ENOMEM;
			goto rotator_start_exit;
		}
		info.session_id = (unsigned int)
			msm_rotator_dev->rot_session[first_free_idx];
		rot_session = msm_rotator_dev->rot_session[first_free_idx];

		rot_session->img_info =	info;
		rot_session->fd_info =	*fd_info;
		rot_session->fast_yuv_enable = fast_yuv_en;
		rot_session->enable_2pass = enable_2pass;

		if (!IS_ERR_OR_NULL(mrd->client)) {
			if (rot_session->img_info.secure) {
				rot_session->mem_hid &= ~BIT(ION_IOMMU_HEAP_ID);
				rot_session->mem_hid |= BIT(ION_CP_MM_HEAP_ID);
				rot_session->mem_hid |= ION_SECURE;
			} else {
				rot_session->mem_hid &= ~BIT(ION_CP_MM_HEAP_ID);
				rot_session->mem_hid &= ~ION_SECURE;
				rot_session->mem_hid |= BIT(ION_IOMMU_HEAP_ID);
			}
		}

		s = first_free_idx;
	} else if (s == MAX_SESSIONS) {
		dev_dbg(msm_rotator_dev->device, "%s: all sessions in use\n",
			__func__);
		rc = -EBUSY;
	}

	if (rc == 0 && copy_to_user((void __user *)arg, &info, sizeof(info)))
		rc = -EFAULT;
	if ((rc == 0) && (info.secure))
		map_sec_resource(msm_rotator_dev);

	sync_info = &msm_rotator_dev->sync_info[s];
	if ((rc == 0) && (sync_info->initialized == false)) {
		char timeline_name[MAX_TIMELINE_NAME_LEN];
		if (sync_info->timeline == NULL) {
			snprintf(timeline_name, sizeof(timeline_name),
				"msm_rot_%d", first_free_idx);
			sync_info->timeline =
				sw_sync_timeline_create(timeline_name);
			if (sync_info->timeline == NULL)
				pr_err("%s: cannot create %s time line",
					__func__, timeline_name);
			sync_info->timeline_value = 0;
		}
		mutex_init(&sync_info->sync_mutex);
		sync_info->initialized = true;
	}
	sync_info->acq_fen = NULL;
	atomic_set(&sync_info->queue_buf_cnt, 0);
rotator_start_exit:
	mutex_unlock(&msm_rotator_dev->rotator_lock);

	return rc;
}

static int msm_rotator_finish(unsigned long arg)
{
	int rc = 0;
	int s;
	unsigned int session_id;

	if (copy_from_user(&session_id, (void __user *)arg, sizeof(s)))
		return -EFAULT;

	rot_wait_for_commit_queue(true);
	mutex_lock(&msm_rotator_dev->rotator_lock);
	for (s = 0; s < MAX_SESSIONS; s++) {
		if ((msm_rotator_dev->rot_session[s] != NULL) &&
			(session_id ==
			(unsigned int)msm_rotator_dev->rot_session[s])) {
			if (msm_rotator_dev->last_session_idx == s)
				msm_rotator_dev->last_session_idx =
					INVALID_SESSION;
			msm_rotator_signal_timeline(s);
			msm_rotator_release_acq_fence(s);
			if (msm_rotator_dev->rot_session[s]->enable_2pass) {
				rotator_free_2pass_buf(mrd->y_rot_buf, s);
				rotator_free_2pass_buf(mrd->chroma_rot_buf, s);
				rotator_free_2pass_buf(mrd->chroma2_rot_buf, s);
			}
			kfree(msm_rotator_dev->rot_session[s]);
			msm_rotator_dev->rot_session[s] = NULL;
			break;
		}
	}

	if (s == MAX_SESSIONS)
		rc = -EINVAL;
#ifdef CONFIG_MSM_BUS_SCALING
	msm_bus_scale_client_update_request(msm_rotator_dev->bus_client_handle,
		0);
#endif
	if (msm_rotator_dev->sec_mapped)
		unmap_sec_resource(msm_rotator_dev);
	mutex_unlock(&msm_rotator_dev->rotator_lock);
	return rc;
}

static int
msm_rotator_open(struct inode *inode, struct file *filp)
{
	struct msm_rotator_fd_info *tmp, *fd_info = NULL;
	int i;

	if (filp->private_data)
		return -EBUSY;

	mutex_lock(&msm_rotator_dev->rotator_lock);
	for (i = 0; i < MAX_SESSIONS; i++) {
		if (msm_rotator_dev->rot_session[i] == NULL)
			break;
	}

	if (i == MAX_SESSIONS) {
		mutex_unlock(&msm_rotator_dev->rotator_lock);
		return -EBUSY;
	}

	list_for_each_entry(tmp, &msm_rotator_dev->fd_list, list) {
		if (tmp->pid == current->pid) {
			fd_info = tmp;
			break;
		}
	}

	if (!fd_info) {
		fd_info = kzalloc(sizeof(*fd_info), GFP_KERNEL);
		if (!fd_info) {
			mutex_unlock(&msm_rotator_dev->rotator_lock);
			pr_err("%s: insufficient memory to alloc resources\n",
			       __func__);
			return -ENOMEM;
		}
		list_add(&fd_info->list, &msm_rotator_dev->fd_list);
		fd_info->pid = current->pid;
	}
	fd_info->ref_cnt++;
	mutex_unlock(&msm_rotator_dev->rotator_lock);

	filp->private_data = fd_info;

	return 0;
}

static int
msm_rotator_close(struct inode *inode, struct file *filp)
{
	struct msm_rotator_fd_info *fd_info;
	int s;

	fd_info = (struct msm_rotator_fd_info *)filp->private_data;

	mutex_lock(&msm_rotator_dev->rotator_lock);
	if (--fd_info->ref_cnt > 0) {
		mutex_unlock(&msm_rotator_dev->rotator_lock);
		return 0;
	}

	for (s = 0; s < MAX_SESSIONS; s++) {
		if (msm_rotator_dev->rot_session[s] != NULL &&
		&(msm_rotator_dev->rot_session[s]->fd_info) == fd_info) {
			pr_debug("%s: freeing rotator session %p (pid %d)\n",
				 __func__, msm_rotator_dev->rot_session[s],
				 fd_info->pid);
			rot_wait_for_commit_queue(true);
			msm_rotator_signal_timeline(s);
			kfree(msm_rotator_dev->rot_session[s]);
			msm_rotator_dev->rot_session[s] = NULL;
			if (msm_rotator_dev->last_session_idx == s)
				msm_rotator_dev->last_session_idx =
					INVALID_SESSION;
		}
	}
	list_del(&fd_info->list);
	kfree(fd_info);
	mutex_unlock(&msm_rotator_dev->rotator_lock);

	return 0;
}

static long msm_rotator_ioctl(struct file *file, unsigned cmd,
						 unsigned long arg)
{
	struct msm_rotator_fd_info *fd_info;

	if (_IOC_TYPE(cmd) != MSM_ROTATOR_IOCTL_MAGIC)
		return -ENOTTY;

	fd_info = (struct msm_rotator_fd_info *)file->private_data;

	switch (cmd) {
	case MSM_ROTATOR_IOCTL_START:
		return msm_rotator_start(arg, fd_info);
	case MSM_ROTATOR_IOCTL_ROTATE:
		return msm_rotator_do_rotate(arg);
	case MSM_ROTATOR_IOCTL_FINISH:
		return msm_rotator_finish(arg);
	case MSM_ROTATOR_IOCTL_BUFFER_SYNC:
		return msm_rotator_buf_sync(arg);

	default:
		dev_dbg(msm_rotator_dev->device,
			"unexpected IOCTL %d\n", cmd);
		return -ENOTTY;
	}
}

static const struct file_operations msm_rotator_fops = {
	.owner = THIS_MODULE,
	.open = msm_rotator_open,
	.release = msm_rotator_close,
	.unlocked_ioctl = msm_rotator_ioctl,
};

static int __devinit msm_rotator_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct resource *res;
	struct msm_rotator_platform_data *pdata = NULL;
	int i, number_of_clks;
	uint32_t ver;

	msm_rotator_dev = kzalloc(sizeof(struct msm_rotator_dev), GFP_KERNEL);
	if (!msm_rotator_dev) {
		printk(KERN_ERR "%s Unable to allocate memory for struct\n",
		       __func__);
		return -ENOMEM;
	}
	for (i = 0; i < MAX_SESSIONS; i++)
		msm_rotator_dev->rot_session[i] = NULL;
	msm_rotator_dev->last_session_idx = INVALID_SESSION;

	pdata = pdev->dev.platform_data;
	number_of_clks = pdata->number_of_clocks;
	rot_iommu_split_domain = pdata->rot_iommu_split_domain;

	msm_rotator_dev->imem_owner = IMEM_NO_OWNER;
	mutex_init(&msm_rotator_dev->imem_lock);
	INIT_LIST_HEAD(&msm_rotator_dev->fd_list);
	msm_rotator_dev->imem_clk_state = CLK_DIS;
	INIT_DELAYED_WORK(&msm_rotator_dev->imem_clk_work,
			  msm_rotator_imem_clk_work_f);
	msm_rotator_dev->imem_clk = NULL;
	msm_rotator_dev->pdev = pdev;

	msm_rotator_dev->core_clk = NULL;
	msm_rotator_dev->pclk = NULL;

	mrd->y_rot_buf = kmalloc(sizeof(struct rot_buf_type), GFP_KERNEL);
	mrd->chroma_rot_buf = kmalloc(sizeof(struct rot_buf_type), GFP_KERNEL);
	mrd->chroma2_rot_buf = kmalloc(sizeof(struct rot_buf_type), GFP_KERNEL);

	memset((void *)mrd->y_rot_buf, 0, sizeof(struct rot_buf_type));
	memset((void *)mrd->chroma_rot_buf, 0, sizeof(struct rot_buf_type));
	memset((void *)mrd->chroma2_rot_buf, 0, sizeof(struct rot_buf_type));

#ifdef CONFIG_MSM_BUS_SCALING
	if (!msm_rotator_dev->bus_client_handle && pdata &&
		pdata->bus_scale_table) {
		msm_rotator_dev->bus_client_handle =
			msm_bus_scale_register_client(
				pdata->bus_scale_table);
		if (!msm_rotator_dev->bus_client_handle) {
			pr_err("%s not able to get bus scale handle\n",
				__func__);
		}
	}
#endif

	for (i = 0; i < number_of_clks; i++) {
		if (pdata->rotator_clks[i].clk_type == ROTATOR_IMEM_CLK) {
			msm_rotator_dev->imem_clk =
			clk_get(&msm_rotator_dev->pdev->dev,
				pdata->rotator_clks[i].clk_name);
			if (IS_ERR(msm_rotator_dev->imem_clk)) {
				rc = PTR_ERR(msm_rotator_dev->imem_clk);
				msm_rotator_dev->imem_clk = NULL;
				printk(KERN_ERR "%s: cannot get imem_clk "
					"rc=%d\n", DRIVER_NAME, rc);
				goto error_imem_clk;
			}
			if (pdata->rotator_clks[i].clk_rate)
				clk_set_rate(msm_rotator_dev->imem_clk,
					pdata->rotator_clks[i].clk_rate);
		}
		if (pdata->rotator_clks[i].clk_type == ROTATOR_PCLK) {
			msm_rotator_dev->pclk =
			clk_get(&msm_rotator_dev->pdev->dev,
				pdata->rotator_clks[i].clk_name);
			if (IS_ERR(msm_rotator_dev->pclk)) {
				rc = PTR_ERR(msm_rotator_dev->pclk);
				msm_rotator_dev->pclk = NULL;
				printk(KERN_ERR "%s: cannot get pclk rc=%d\n",
					DRIVER_NAME, rc);
				goto error_pclk;
			}

			if (pdata->rotator_clks[i].clk_rate)
				clk_set_rate(msm_rotator_dev->pclk,
					pdata->rotator_clks[i].clk_rate);
		}

		if (pdata->rotator_clks[i].clk_type == ROTATOR_CORE_CLK) {
			msm_rotator_dev->core_clk =
			clk_get(&msm_rotator_dev->pdev->dev,
				pdata->rotator_clks[i].clk_name);
			if (IS_ERR(msm_rotator_dev->core_clk)) {
				rc = PTR_ERR(msm_rotator_dev->core_clk);
				msm_rotator_dev->core_clk = NULL;
				printk(KERN_ERR "%s: cannot get core clk "
					"rc=%d\n", DRIVER_NAME, rc);
			goto error_core_clk;
			}

			if (pdata->rotator_clks[i].clk_rate)
				clk_set_rate(msm_rotator_dev->core_clk,
					pdata->rotator_clks[i].clk_rate);
		}
	}

	msm_rotator_dev->regulator = regulator_get(&msm_rotator_dev->pdev->dev,
						   "vdd");
	if (IS_ERR(msm_rotator_dev->regulator))
		msm_rotator_dev->regulator = NULL;

	msm_rotator_dev->rot_clk_state = CLK_DIS;
	INIT_DELAYED_WORK(&msm_rotator_dev->rot_clk_work,
			  msm_rotator_rot_clk_work_f);

	mutex_init(&msm_rotator_dev->rotator_lock);
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	msm_rotator_dev->client = msm_ion_client_create(-1, pdev->name);
#endif
	platform_set_drvdata(pdev, msm_rotator_dev);


	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		printk(KERN_ALERT
		       "%s: could not get IORESOURCE_MEM\n", DRIVER_NAME);
		rc = -ENODEV;
		goto error_get_resource;
	}
	msm_rotator_dev->io_base = ioremap(res->start,
					   resource_size(res));

#ifdef CONFIG_MSM_ROTATOR_USE_IMEM
	if (msm_rotator_dev->imem_clk)
		clk_prepare_enable(msm_rotator_dev->imem_clk);
#endif
	enable_rot_clks();
	ver = ioread32(MSM_ROTATOR_HW_VERSION);
	disable_rot_clks();

#ifdef CONFIG_MSM_ROTATOR_USE_IMEM
	if (msm_rotator_dev->imem_clk)
		clk_disable_unprepare(msm_rotator_dev->imem_clk);
#endif
	if (ver != pdata->hardware_version_number)
		pr_debug("%s: invalid HW version ver 0x%x\n",
			DRIVER_NAME, ver);

	rotator_hw_revision = ver;
	rotator_hw_revision >>= 16;     /* bit 31:16 */
	rotator_hw_revision &= 0xff;

	pr_info("%s: rotator_hw_revision=%x\n",
		__func__, rotator_hw_revision);

	msm_rotator_dev->irq = platform_get_irq(pdev, 0);
	if (msm_rotator_dev->irq < 0) {
		printk(KERN_ALERT "%s: could not get IORESOURCE_IRQ\n",
		       DRIVER_NAME);
		rc = -ENODEV;
		goto error_get_irq;
	}
	rc = request_irq(msm_rotator_dev->irq, msm_rotator_isr,
			 IRQF_TRIGGER_RISING, DRIVER_NAME, NULL);
	if (rc) {
		printk(KERN_ERR "%s: request_irq() failed\n", DRIVER_NAME);
		goto error_get_irq;
	}
	/* we enable the IRQ when we need it in the ioctl */
	disable_irq(msm_rotator_dev->irq);

	rc = alloc_chrdev_region(&msm_rotator_dev->dev_num, 0, 1, DRIVER_NAME);
	if (rc < 0) {
		printk(KERN_ERR "%s: alloc_chrdev_region Failed rc = %d\n",
		       __func__, rc);
		goto error_get_irq;
	}

	msm_rotator_dev->class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(msm_rotator_dev->class)) {
		rc = PTR_ERR(msm_rotator_dev->class);
		printk(KERN_ERR "%s: couldn't create class rc = %d\n",
		       DRIVER_NAME, rc);
		goto error_class_create;
	}

	msm_rotator_dev->device = device_create(msm_rotator_dev->class, NULL,
						msm_rotator_dev->dev_num, NULL,
						DRIVER_NAME);
	if (IS_ERR(msm_rotator_dev->device)) {
		rc = PTR_ERR(msm_rotator_dev->device);
		printk(KERN_ERR "%s: device_create failed %d\n",
		       DRIVER_NAME, rc);
		goto error_class_device_create;
	}

	cdev_init(&msm_rotator_dev->cdev, &msm_rotator_fops);
	rc = cdev_add(&msm_rotator_dev->cdev,
		      MKDEV(MAJOR(msm_rotator_dev->dev_num), 0),
		      1);
	if (rc < 0) {
		printk(KERN_ERR "%s: cdev_add failed %d\n", __func__, rc);
		goto error_cdev_add;
	}

	init_waitqueue_head(&msm_rotator_dev->wq);
	INIT_WORK(&msm_rotator_dev->commit_work, rot_commit_wq_handler);
	init_completion(&msm_rotator_dev->commit_comp);
	mutex_init(&msm_rotator_dev->commit_mutex);
	mutex_init(&msm_rotator_dev->commit_wq_mutex);
	atomic_set(&msm_rotator_dev->commit_q_w, 0);
	atomic_set(&msm_rotator_dev->commit_q_r, 0);
	atomic_set(&msm_rotator_dev->commit_q_cnt, 0);

	dev_dbg(msm_rotator_dev->device, "probe successful\n");
	return rc;

error_cdev_add:
	device_destroy(msm_rotator_dev->class, msm_rotator_dev->dev_num);
error_class_device_create:
	class_destroy(msm_rotator_dev->class);
error_class_create:
	unregister_chrdev_region(msm_rotator_dev->dev_num, 1);
error_get_irq:
	iounmap(msm_rotator_dev->io_base);
error_get_resource:
	mutex_destroy(&msm_rotator_dev->rotator_lock);
	if (msm_rotator_dev->regulator)
		regulator_put(msm_rotator_dev->regulator);
	clk_put(msm_rotator_dev->core_clk);
error_core_clk:
	clk_put(msm_rotator_dev->pclk);
error_pclk:
	if (msm_rotator_dev->imem_clk)
		clk_put(msm_rotator_dev->imem_clk);
error_imem_clk:
	mutex_destroy(&msm_rotator_dev->imem_lock);
	kfree(msm_rotator_dev);
	return rc;
}

static int __devexit msm_rotator_remove(struct platform_device *plat_dev)
{
	int i;

	rot_wait_for_commit_queue(true);
#ifdef CONFIG_MSM_BUS_SCALING
	if (msm_rotator_dev->bus_client_handle) {
		msm_bus_scale_unregister_client
			(msm_rotator_dev->bus_client_handle);
		msm_rotator_dev->bus_client_handle = 0;
	}
#endif
	free_irq(msm_rotator_dev->irq, NULL);
	mutex_destroy(&msm_rotator_dev->rotator_lock);
	cdev_del(&msm_rotator_dev->cdev);
	device_destroy(msm_rotator_dev->class, msm_rotator_dev->dev_num);
	class_destroy(msm_rotator_dev->class);
	unregister_chrdev_region(msm_rotator_dev->dev_num, 1);
	iounmap(msm_rotator_dev->io_base);
	if (msm_rotator_dev->imem_clk) {
		if (msm_rotator_dev->imem_clk_state == CLK_EN)
			clk_disable_unprepare(msm_rotator_dev->imem_clk);
		clk_put(msm_rotator_dev->imem_clk);
		msm_rotator_dev->imem_clk = NULL;
	}
	if (msm_rotator_dev->rot_clk_state == CLK_EN)
		disable_rot_clks();
	clk_put(msm_rotator_dev->core_clk);
	clk_put(msm_rotator_dev->pclk);
	if (msm_rotator_dev->regulator)
		regulator_put(msm_rotator_dev->regulator);
	msm_rotator_dev->core_clk = NULL;
	msm_rotator_dev->pclk = NULL;
	mutex_destroy(&msm_rotator_dev->imem_lock);
	for (i = 0; i < MAX_SESSIONS; i++)
		if (msm_rotator_dev->rot_session[i] != NULL)
			kfree(msm_rotator_dev->rot_session[i]);
	kfree(msm_rotator_dev);
	return 0;
}

#ifdef CONFIG_PM
static int msm_rotator_suspend(struct platform_device *dev, pm_message_t state)
{
	rot_wait_for_commit_queue(true);
	mutex_lock(&msm_rotator_dev->imem_lock);
	if (msm_rotator_dev->imem_clk_state == CLK_EN
		&& msm_rotator_dev->imem_clk) {
		clk_disable_unprepare(msm_rotator_dev->imem_clk);
		msm_rotator_dev->imem_clk_state = CLK_SUSPEND;
	}
	mutex_unlock(&msm_rotator_dev->imem_lock);
	mutex_lock(&msm_rotator_dev->rotator_lock);
	if (msm_rotator_dev->rot_clk_state == CLK_EN) {
		disable_rot_clks();
		msm_rotator_dev->rot_clk_state = CLK_SUSPEND;
	}
	msm_rotator_release_all_timeline();
	mutex_unlock(&msm_rotator_dev->rotator_lock);
	return 0;
}

static int msm_rotator_resume(struct platform_device *dev)
{
	mutex_lock(&msm_rotator_dev->imem_lock);
	if (msm_rotator_dev->imem_clk_state == CLK_SUSPEND
		&& msm_rotator_dev->imem_clk) {
		clk_prepare_enable(msm_rotator_dev->imem_clk);
		msm_rotator_dev->imem_clk_state = CLK_EN;
	}
	mutex_unlock(&msm_rotator_dev->imem_lock);
	mutex_lock(&msm_rotator_dev->rotator_lock);
	if (msm_rotator_dev->rot_clk_state == CLK_SUSPEND) {
		enable_rot_clks();
		msm_rotator_dev->rot_clk_state = CLK_EN;
	}
	mutex_unlock(&msm_rotator_dev->rotator_lock);
	return 0;
}
#endif

static struct platform_driver msm_rotator_platform_driver = {
	.probe = msm_rotator_probe,
	.remove = __devexit_p(msm_rotator_remove),
#ifdef CONFIG_PM
	.suspend = msm_rotator_suspend,
	.resume = msm_rotator_resume,
#endif
	.driver = {
		.owner = THIS_MODULE,
		.name = DRIVER_NAME
	}
};

static int __init msm_rotator_init(void)
{
	return platform_driver_register(&msm_rotator_platform_driver);
}

static void __exit msm_rotator_exit(void)
{
	return platform_driver_unregister(&msm_rotator_platform_driver);
}

module_init(msm_rotator_init);
module_exit(msm_rotator_exit);

MODULE_DESCRIPTION("MSM Offline Image Rotator driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
