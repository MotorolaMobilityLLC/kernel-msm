// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/qcom_scm.h>
#include <linux/qtee_shmbridge.h>
#include <linux/slab.h>
#include <linux/smci_object.h>
#include <linux/smci_clientenv.h>
#include "smci_mem_lat.h"
#include "trace-bus-prof.h"

#define SAMPLE_MS	10
#define MAGIC (0x01CB)

#ifndef UINT32_C
#define UINT32_C(x) ((uint32_t)(x))
#endif

enum cmd {
	MEM_LAT_START_PROFILING = 1,
	MEM_LAT_GET_DATA = 2,
	MEM_LAT_STOP_PROFILING = 3,
	MEM_LAT_LAST_ID = 0x7FFFFFFF
};

#define CPU_BIT_SHIFT 0
#define GPU_BIT_SHIFT 1
#define NSP_BIT_SHIFT 2

#define CPU_PROFILING_ENABLED	BIT(CPU_BIT_SHIFT)
#define GPU_PROFILING_ENABLED	BIT(GPU_BIT_SHIFT)
#define NSP_PROFILING_ENABLED	BIT(NPU_BIT_SHIFT)
#define MEM_LATENCY_FEATURE_ID 2106

enum error {
	E_SUCCESS = 0, /* Operation successful */
	E_FAILURE = 1, /* Operation failed due to unknown err */
	E_NULL_PARAM = 2, /* Null Parameter */
	E_INVALID_ARG = 3, /* Arg is not recognized */
	E_BAD_ADDRESS = 4, /* Ptr arg is bad address */
	E_INVALID_ARG_LEN = 5, /* Arg length is wrong */
	E_NOT_SUPPORTED =  6, /* Operation not supported */
	E_UNINITIALIZED = 7, /* Operation not permitted on platform */
	E_PARTIAL_DUMP = 8, /* Operation not permitted right now */
	E_RESERVED = 0x7FFFFFFF
};

enum bus_lat_masters {
	CPU = 0,
	GPU,
	NSP,
	MAX_MASTER,
};

struct mem_lat_data {
	u64	qtime;
	enum	error	err;
	u16	magic;
	u32	histbin[MAX_MASTER][8];
} __packed;


struct mem_lat_start_req {
	u32	cmd_id;
	u32	active_masters;
} __packed;

struct mem_lat_get_req {
	u32	cmd_id;
	u8	*buf_ptr;
	u32	buf_size;
	u32	type; /*Stop : 0, Reset : 1*/
} __packed;


struct mem_lat_stop_req {
	u32	cmd_id;
} __packed;

struct mem_lat_rsp {
	u32	cmd_id;
	enum error	status;
} __packed;

union mem_lat_req {
	struct mem_lat_start_req start_req;
	struct mem_lat_get_req get_req;
	struct mem_lat_stop_req stop_req;
} __packed;

struct mem_cmd_buf {
	union		mem_lat_req lat_req;
	struct		mem_lat_rsp lat_resp;
	u32		req_size;
} __packed;

struct lat_sample {
	u64	ts;
	u32	histbin[8];
} __packed;

struct master_data {
	u16			curr_idx;
	u16			unread_samples;
	struct lat_sample	*lat_data;
	char			buf[PAGE_SIZE];
};

struct bus_lat_dev_data {
	struct work_struct	work;
	struct workqueue_struct	*wq;
	struct hrtimer		hrtimer;
	u16			max_samples;
	u16			size_of_line;
	u32			active_masters;
	u32			available_masters;
	struct mutex		lock;
	struct mem_lat_data	*data;
	struct master_data	mdata[MAX_MASTER];
};

static struct dentry *bus_lat_dir;
static char *master_names[MAX_MASTER] = {"CPU", "GPU", "NSP"};
static struct bus_lat_dev_data *bus_lat;

static ssize_t get_last_samples(struct file *file, char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	int index, ret = 0, m_idx, enable, i, size = 0;
	char *master_name;

	mutex_lock(&bus_lat->lock);
	if (!bus_lat->active_masters) {
		pr_err("No master is enabled for mem latency\n");
		goto unlock;
	}

	master_name = file->private_data;
	for (m_idx = 0; m_idx < MAX_MASTER; m_idx++) {
		if (!strcasecmp(master_names[m_idx], master_name))
			break;
	}

	enable = (bus_lat->active_masters & BIT(m_idx));
	if (!enable) {
		pr_err("%s memory latency is not enabled\n", master_names[m_idx]);
		ret = -EINVAL;
		goto unlock;
	}

	index = (bus_lat->mdata[m_idx].curr_idx - bus_lat->mdata[m_idx].unread_samples +
				bus_lat->max_samples) % bus_lat->max_samples;
	for (i = 0; i < bus_lat->mdata[m_idx].unread_samples; i++) {
		size += scnprintf(bus_lat->mdata[m_idx].buf + size, PAGE_SIZE - size,
			"%llx\t%x\t%x\t%x\t%x\t%x\t%x\t%x\t%x\n",
			bus_lat->mdata[m_idx].lat_data[index].ts,
			bus_lat->mdata[m_idx].lat_data[index].histbin[0],
			bus_lat->mdata[m_idx].lat_data[index].histbin[1],
			bus_lat->mdata[m_idx].lat_data[index].histbin[2],
			bus_lat->mdata[m_idx].lat_data[index].histbin[3],
			bus_lat->mdata[m_idx].lat_data[index].histbin[4],
			bus_lat->mdata[m_idx].lat_data[index].histbin[5],
			bus_lat->mdata[m_idx].lat_data[index].histbin[6],
			bus_lat->mdata[m_idx].lat_data[index].histbin[7]);
			index = (index + 1) % bus_lat->max_samples;
	}

	bus_lat->mdata[m_idx].unread_samples = 0;
	ret = simple_read_from_buffer(user_buf, count, ppos, bus_lat->mdata[m_idx].buf, size);
unlock:
	mutex_unlock(&bus_lat->lock);

	return ret;
}

static int memory_lat_profiling_command(const void *req)
{
	int ret = 0;
	u32 qseos_cmd_id = 0;
	struct mem_lat_rsp *rsp = NULL;
	size_t req_size = 0, rsp_size = 0;
	struct qtee_shm shm = {0};

	if (!req)
		return -EINVAL;
	rsp = &((struct mem_cmd_buf *)req)->lat_resp;
	rsp_size = sizeof(struct mem_lat_rsp);
	req_size = ((struct mem_cmd_buf *)req)->req_size;
	qseos_cmd_id = *(u32 *)req;
	ret = qtee_shmbridge_allocate_shm(PAGE_ALIGN(req_size + rsp_size), &shm);

	if (ret) {
		ret = -ENOMEM;
		pr_err("qtee_shmbridge_allocate_shm failed, ret :%d\n", ret);
		goto out;
	}

	memcpy(shm.vaddr, req, req_size);
	qtee_shmbridge_flush_shm_buf(&shm);
	switch (qseos_cmd_id) {
	case	MEM_LAT_START_PROFILING:
	case	MEM_LAT_GET_DATA:
	case	MEM_LAT_STOP_PROFILING:
		/* Send the command to TZ */
		ret = qcom_scm_memory_lat_profiler(shm.paddr, req_size,
						shm.paddr + req_size, rsp_size);
		break;
	default:
		pr_err("cmd_id %d is not supported.\n", qseos_cmd_id);
		ret = -EINVAL;
	}

	qtee_shmbridge_inv_shm_buf(&shm);
	memcpy(rsp, (char *)shm.vaddr + req_size, rsp_size);
out:
	qtee_shmbridge_free_shm(&shm);
	/* Verify cmd id and Check that request succeeded. */
	if ((rsp->status != 0) ||
		(qseos_cmd_id != rsp->cmd_id)) {
		ret = -1;
		pr_err("Status: %d,Cmd: %d qseos_cmd_id=%d\n",
			rsp->status, rsp->cmd_id, qseos_cmd_id);
	}

	return ret;
}

static int start_memory_lat_stats(void)
{
	int ret = 0;
	struct mem_cmd_buf *mem_cmd_buf = NULL;

	mem_cmd_buf = kzalloc(sizeof(*mem_cmd_buf), GFP_KERNEL);

	if (!mem_cmd_buf)
		return -ENOMEM;

	mem_cmd_buf->lat_req.start_req.cmd_id = MEM_LAT_START_PROFILING;
	mem_cmd_buf->lat_req.start_req.active_masters = bus_lat->active_masters;
	mem_cmd_buf->req_size = sizeof(struct mem_lat_start_req);
	ret = memory_lat_profiling_command(mem_cmd_buf);

	if (ret) {
		pr_err("Error starting memory latency stats, ret = %d\n", ret);
		goto out;
	}
	if (!hrtimer_active(&bus_lat->hrtimer))
		hrtimer_start(&bus_lat->hrtimer,
				ms_to_ktime(SAMPLE_MS), HRTIMER_MODE_REL_PINNED);
out:
	kfree(mem_cmd_buf);

	return ret;
}

static int stop_memory_lat_stats(void)
{
	int ret;
	struct mem_cmd_buf *mem_cmd_buf = NULL;

	hrtimer_cancel(&bus_lat->hrtimer);
	cancel_work_sync(&bus_lat->work);
	mem_cmd_buf = kzalloc(sizeof(*mem_cmd_buf), GFP_KERNEL);

	if (!mem_cmd_buf)
		return -ENOMEM;

	mem_cmd_buf->lat_req.stop_req.cmd_id = MEM_LAT_STOP_PROFILING;
	mem_cmd_buf->req_size = sizeof(struct mem_lat_stop_req);
	ret = memory_lat_profiling_command(mem_cmd_buf);

	if (ret)
		pr_err("Error stopping memory latency stats, ret = %d\n", ret);

	kfree(mem_cmd_buf);

	return 0;
}

static int set_mon_enabled(void *data, u64 val)
{
	u32 count, enable = val ? 1 : 0;
	char *master_name = data;
	int i, ret = 0;
	struct smci_object mem_lat_env = {NULL, NULL};
	struct smci_object mem_lat_profiler = {NULL, NULL};

	ret = smci_get_client_env_object(&mem_lat_env);
	if (ret) {
		mem_lat_env.invoke = NULL;
		mem_lat_env.context = NULL;
		pr_err("mem_lat_profiler: get client env object failed\n");
		ret =  -EIO;
		goto end;
	}

	ret = smci_clientenv_open(mem_lat_env, SMCI_MEM_LAT_PROFILER_SERVICE_UID,
			&mem_lat_profiler);
	if (ret) {
		mem_lat_profiler.invoke = NULL;
		mem_lat_profiler.context = NULL;
		pr_err("mem_lat_profiler: smci client env open failed\n");
		ret = -EIO;
		goto end;
	}

	ret = smci_mem_lat_profiler_check_license_status(mem_lat_profiler,
			MEM_LATENCY_FEATURE_ID, NULL, 0);
	if (ret) {
		pr_err("mem_lat_profiler: smci_mem_lat_profiler_check_license_status failed\n");
		ret = -EIO;
		goto end;
	}

	mutex_lock(&bus_lat->lock);

	for (i = 0; i < MAX_MASTER; i++) {
		if (!strcasecmp(master_names[i], master_name))
			break;
	}

	if (enable == (bus_lat->active_masters & BIT(i)))
		goto unlock;

	count = hweight32(bus_lat->active_masters);

	if (count >= MAX_MASTER && enable) {
		pr_err("Max masters already enabled\n");
		ret = -EINVAL;
		goto unlock;
	}

	mutex_unlock(&bus_lat->lock);

	if (count)
		stop_memory_lat_stats();

	mutex_lock(&bus_lat->lock);
	bus_lat->active_masters = (bus_lat->active_masters ^ BIT(i));

	if (bus_lat->active_masters)
		start_memory_lat_stats();
	ret = 0;

unlock:
	mutex_unlock(&bus_lat->lock);
	return ret;
end:
	SMCI_OBJECT_ASSIGN_NULL(mem_lat_profiler);
	SMCI_OBJECT_ASSIGN_NULL(mem_lat_env);
	return ret;
}

static int get_mon_enabled(void *data, u64 *val)
{
	char *master_name  = data;
	int i;

	mutex_lock(&bus_lat->lock);

	for (i = 0; i < MAX_MASTER; i++) {
		if (!strcasecmp(master_names[i], master_name))
			break;
	}

	if (bus_lat->active_masters & BIT(i))
		*val = 1;
	else
		*val = 0;

	mutex_unlock(&bus_lat->lock);

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(set_mon_enabled_ops, get_mon_enabled, set_mon_enabled, "%llu\n");

static const struct file_operations show_last_samples_ops = {
	.read = get_last_samples,
	.open = simple_open,
	.llseek = default_llseek,
};

static void bus_lat_update_work(struct work_struct *work)
{
	const int bufsize = sizeof(struct mem_lat_data);
	struct mem_cmd_buf *mem_cmd_buf;
	struct qtee_shm buf_shm = {0};
	int ret, i, j;
	u16 magic;

	mem_cmd_buf = kzalloc(sizeof(*mem_cmd_buf), GFP_KERNEL);
	if (!mem_cmd_buf)
		return;

	ret = qtee_shmbridge_allocate_shm(PAGE_ALIGN(bufsize), &buf_shm);
	if (ret) {
		pr_err("shmbridge alloc buf failed\n");
		return;
	}

	mem_cmd_buf->lat_req.get_req.cmd_id = MEM_LAT_GET_DATA;
	mem_cmd_buf->lat_req.get_req.buf_ptr = (u8 *)buf_shm.paddr;
	mem_cmd_buf->lat_req.get_req.buf_size = bufsize;
	mem_cmd_buf->lat_req.get_req.type = 1;
	mem_cmd_buf->req_size = sizeof(struct mem_lat_get_req);
	qtee_shmbridge_flush_shm_buf(&buf_shm);
	ret = memory_lat_profiling_command(mem_cmd_buf);
	if (ret) {
		pr_err("memory_lat_profiling_command failed\n");
		goto err;
	}

	qtee_shmbridge_inv_shm_buf(&buf_shm);
	memcpy(bus_lat->data, (char *)buf_shm.vaddr, sizeof(*bus_lat->data));
	magic = bus_lat->data->magic;
	if (magic != MAGIC) {
		pr_err("Expected magic value is %x but got %x\n", MAGIC, magic);
		goto err;
	}

	mutex_lock(&bus_lat->lock);

	/*
	 * For all the max masters update the unread samples and populate in
	 * the memory latency profiling data structure.
	 */
	for (i = 0; i < MAX_MASTER; i++) {
		bus_lat->mdata[i].lat_data[bus_lat->mdata[i].curr_idx].ts = bus_lat->data->qtime;
		for (j = 0; j < 8; j++)
			bus_lat->mdata[i].lat_data[bus_lat->mdata[i].curr_idx].histbin[j]
							= bus_lat->data->histbin[i][j];
		bus_lat->mdata[i].unread_samples =
				min(++bus_lat->mdata[i].unread_samples, bus_lat->max_samples);
		bus_lat->mdata[i].curr_idx =
					(bus_lat->mdata[i].curr_idx + 1) % bus_lat->max_samples;
	}

	for (i = 0; i < MAX_MASTER; i++) {
		if (!(bus_lat->active_masters & BIT(i)))
			continue;
		trace_memory_lat_last_sample(bus_lat->data->qtime, i,
			bus_lat->data->histbin[i]);
	}

	mutex_unlock(&bus_lat->lock);

err:
	qtee_shmbridge_free_shm(&buf_shm);
	kfree(mem_cmd_buf);
}

static enum hrtimer_restart hrtimer_handler(struct hrtimer *timer)
{
	ktime_t now = ktime_get();

	queue_work(bus_lat->wq, &bus_lat->work);
	hrtimer_forward(timer, now, ms_to_ktime(SAMPLE_MS));

	return HRTIMER_RESTART;
}

static int bus_lat_create_fs_entries(void)
{
	int i;
	struct dentry *ret = NULL, *master_dir;

	bus_lat_dir = debugfs_create_dir("mem_lat", 0);
	if (IS_ERR(bus_lat_dir)) {
		pr_err("Debugfs directory creation failed for mem_lat\n");
		return PTR_ERR(bus_lat_dir);
	}

	for (i = 0; i < MAX_MASTER; i++) {
		master_dir = debugfs_create_dir(master_names[i], bus_lat_dir);
		if (IS_ERR(master_dir)) {
			pr_err("Debugfs directory creation failed for %s\n", master_names[i]);
			goto cleanup;
		}

		ret = debugfs_create_file("show_last_samples", 0400, master_dir,
						master_names[i], &show_last_samples_ops);
		if (IS_ERR(ret)) {
			pr_err("Debugfs file creation failed for show_last_samples\n");
			goto cleanup;
		}

		ret = debugfs_create_file("enable", 0644, master_dir,
						master_names[i], &set_mon_enabled_ops);
		if (IS_ERR(ret)) {
			pr_err("Debugfs file creation failed for enable\n");
			goto cleanup;
		}
	}

	return 0;

cleanup:
	for (; i >= 0; i--)
		debugfs_remove_recursive(debugfs_lookup(master_names[i], bus_lat_dir));
	debugfs_remove_recursive(bus_lat_dir);

	return -ENOENT;
}
static int __init qcom_bus_lat_init(void)
{
	int i, j, ret = 0;

	bus_lat =  kzalloc(sizeof(*bus_lat), GFP_KERNEL);

	if (!bus_lat)
		return -ENOMEM;

	bus_lat->data = kzalloc(sizeof(struct mem_lat_data), GFP_KERNEL);
	if (!bus_lat->data) {
		kfree(bus_lat);
		return -ENOMEM;
	}

	for (i = 0; i < MAX_MASTER; i++)
		bus_lat->available_masters |= BIT(i);

	ret =  bus_lat_create_fs_entries();
	if (ret < 0)
		goto err;

	/*
	 * to get no of hex char in a line multiplying size of struct lat_sample by 2
	 * and adding 8 for tabs and 1 for new line.
	 */
	bus_lat->size_of_line = sizeof(struct lat_sample) * 2 + 9;
	bus_lat->max_samples = PAGE_SIZE / bus_lat->size_of_line;

	for (i = 0; i < MAX_MASTER ; i++) {
		bus_lat->mdata[i].lat_data = kcalloc(bus_lat->max_samples,
				sizeof(struct lat_sample), GFP_KERNEL);
		if (!bus_lat->mdata[i].lat_data) {
			ret = -ENOMEM;
			goto debugfs_file_err;
		}
	}

	mutex_init(&bus_lat->lock);
	hrtimer_init(&bus_lat->hrtimer, CLOCK_MONOTONIC,
				HRTIMER_MODE_REL);
	bus_lat->hrtimer.function = hrtimer_handler;
	bus_lat->wq = create_freezable_workqueue("bus_lat_wq");
	if (!bus_lat->wq) {
		pr_err("Couldn't create bus_lat workqueue.\n");
		ret = -ENOMEM;
		goto debugfs_file_err;
	}

	INIT_WORK(&bus_lat->work, &bus_lat_update_work);

	return ret;

debugfs_file_err:
	for (j = 0; j < i; j++)
		kfree(bus_lat->mdata[j].lat_data);
	debugfs_remove_recursive(bus_lat_dir);
err:
	kfree(bus_lat->data);
	kfree(bus_lat);
	return ret;
}

module_init(qcom_bus_lat_init);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. BUS_LAT driver");
MODULE_LICENSE("GPL");
