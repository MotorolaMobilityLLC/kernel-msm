// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#include <linux/platform_device.h>
#include <linux/debugfs.h>

#include "hgsl.h"
#include "hgsl_debugfs.h"

#define HGSL_SHADOW_TS_INFO_LENGTH (64)
static int hgsl_stat_show(struct seq_file *s, void *unused)
{
	struct qcom_hgsl *hgsl = s->private;
	struct hgsl_hsync_timeline *tl;
	struct hgsl_isync_timeline *cur;
	struct hgsl_context *ctxt;
	struct doorbell_queue *dbq;
	struct doorbell_context_queue *dbcq;
	uint32_t ts = 0, idr;
	int i = 0, found = 0, ret, dev_hnd = GSL_HANDLE_DEV0;
	char shadow_ts_info[HGSL_SHADOW_TS_INFO_LENGTH];
	struct hgsl_active_wait *wait;

	seq_printf(s, "DEVICE INFO:\n"
		"{ default_iocoherency=%d, db_off=%d, total_mem_size=%lld; }\n",
		hgsl->default_iocoherency, hgsl->db_off,
		atomic64_read(&hgsl->total_mem_size));

	seq_printf(s, "\n%s\n%s\n", "ACTIVE ISYNCS:", "{");
	spin_lock(&hgsl->isync_timeline_lock);
	idr_for_each_entry(&hgsl->isync_timeline_idr, cur, idr) {
		seq_printf(s,
			"    %s: { t_context=0x%llx, signaled_ts=%u, flags=0x%x, 64bit=%u; }\n",
			cur->name, cur->context, cur->last_ts, cur->flags, cur->is64bits);
		found = 1;
	}
	spin_unlock(&hgsl->isync_timeline_lock);
	if (!found)
		seq_puts(s, "    null\n");
	seq_puts(s, "}\n");

	found = 0;
	seq_printf(s, "\n%s\n%s\n", "ACTIVE WAITING CONTEXTS:", "{");
	spin_lock(&hgsl->active_wait_lock);
	list_for_each_entry(wait, &hgsl->active_wait_list, head) {
		seq_printf(s, "    { context_id=%u, wait_timestamp=%u; }\n",
			wait->ctxt->context_id, wait->timestamp);
		found = 1;
	}
	spin_unlock(&hgsl->active_wait_lock);
	if (!found)
		seq_puts(s, "    null\n");
	seq_puts(s, "}\n");

	seq_printf(s, "\n%s\n", "ACTIVE CONTEXTS:");
	for (; dev_hnd < (HGSL_DEVICE_NUM + 1); dev_hnd++) {
		for (; i < HGSL_CONTEXT_NUM; i++) {
			ctxt = hgsl_get_context(hgsl, dev_hnd, i);
			if (!ctxt)
				continue;

			memset(shadow_ts_info, 0, sizeof(shadow_ts_info));
			if (ctxt->shadow_ts) {
				mutex_lock(&ctxt->lock);
				ts = get_context_retired_ts(ctxt);
				mutex_unlock(&ctxt->lock);
				snprintf(shadow_ts_info, sizeof(shadow_ts_info),
					"shadow ts enabled");
			} else {
				struct hgsl_ioctl_read_ts_params param;

				param.devhandle = dev_hnd;
				param.ctxthandle = i;
				param.type = GSL_TIMESTAMP_RETIRED;
				ret = hgsl_hyp_read_timestamp(&hgsl->global_hyp, &param);
				if (ret)
					ts = param.timestamp;
				snprintf(shadow_ts_info, sizeof(shadow_ts_info),
					"shadow ts disabled, read ts from BE %s",
					ret ? "failure" : "success");
			}
			seq_printf(s, "ID=%3u: {\n", ctxt->context_id);
			seq_printf(s,
				"    pid=%d, devhandle=%u, is_fe_shadow=%u, in_destroy=%u, ",
				ctxt->pid, ctxt->devhandle, ctxt->is_fe_shadow, ctxt->in_destroy);
			seq_printf(s,
				"dbq_info=0x%x, flags=0x%x, tcsr_idx=%d, db_signal=%u;\n",
				ctxt->dbq_info, ctxt->flags, ctxt->tcsr_idx, ctxt->db_signal);
			seq_printf(s,
				"    queued_ts=%u, last_ts=%u, retired_ts=%u;\n",
				ctxt->queued_ts, ctxt->last_ts, ts);
			seq_printf(s, "    %s;\n", shadow_ts_info);

			dbq = ctxt->dbq;
			if (dbq)
				seq_printf(s,
					"    dbq idx=%u: {\n"
					"        state=%u, tcsr_idx=%d, ibdesc_max_size=%u, seq_num=%d;\n"
					"    }\n",
					dbq->dbq_idx, dbq->state, dbq->tcsr_idx,
					dbq->ibdesc_max_size, atomic_read(&dbq->seq_num));

			dbcq = ctxt->dbcq;
			if (dbcq) {
				seq_printf(s,
					"    dbcq irq_bit_idx=%d: {\n", dbcq->irq_bit_idx);
				seq_printf(s,
					"        db_signal=%u, queue_size=0x%x, indirect_ib_ts=%u, ",
					dbcq->db_signal, dbcq->queue_size, dbcq->indirect_ib_ts);
				seq_printf(s, "seq_num=%u, dbcq_export_id=%u;\n",
					dbcq->seq_num, ctxt->dbcq_export_id);
				seq_puts(s, "    }\n");
			}

			tl = ctxt->timeline;
			if (tl)
				seq_printf(s,
					"    timeline=%s: {\n"
					"        fcontext=0x%llx, signaled_ts=%u;\n"
					"    }\n",
					tl->name, tl->fence_context, tl->last_ts);

			seq_puts(s, "}\n");
			hgsl_put_context(ctxt);
		}
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(hgsl_stat);

static int hgsl_client_mem_show(struct seq_file *s, void *unused)
{
	struct hgsl_priv *priv = s->private;
	struct hgsl_mem_node *tmp = NULL;
	struct rb_node *rb = NULL;

	seq_printf(s, "%16s %16s %10s %10s\n",
			"gpuaddr", "size", "flags", "type");

	mutex_lock(&priv->lock);
	for (rb = rb_first(&priv->mem_allocated); rb; rb = rb_next(rb)) {
		tmp = rb_entry(rb, struct hgsl_mem_node, mem_rb_node);
		seq_printf(s, "%p %16llx %10x %10d\n",
				tmp->memdesc.gpuaddr,
				tmp->memdesc.size,
				tmp->flags,
				tmp->memtype
				);
	}
	mutex_unlock(&priv->lock);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(hgsl_client_mem);

static int hgsl_client_memtype_show(struct seq_file *s, void *unused)
{
	struct hgsl_priv *priv = s->private;
	struct hgsl_mem_node *tmp = NULL;
	struct rb_node *rb = NULL;
	int i;
	int memtype;

	static struct {
		char *name;
		size_t size;
	} gpu_mem_types[] = {
		{"any", 0},
		{"framebuffer", 0},
		{"renderbbuffer", 0},
		{"arraybuffer", 0},
		{"elementarraybuffer", 0},
		{"vertexarraybuffer", 0},
		{"texture", 0},
		{"surface", 0},
		{"eglsurface", 0},
		{"gl", 0},
		{"cl", 0},
		{"cl_buffer_map", 0},
		{"cl_buffer_unmap", 0},
		{"cl_image_map", 0},
		{"cl_image_unmap", 0},
		{"cl_kernel_stack", 0},
		{"cmds", 0},
		{"2d", 0},
		{"egl_image", 0},
		{"egl_shadow", 0},
		{"multisample", 0},
		{"2d_ext", 0},
		{"3d_ext", 0}, /* 0x16 */
		{"unknown_type", 0},
		{"unknown_type", 0},
		{"unknown_type", 0},
		{"unknown_type", 0},
		{"unknown_type", 0},
		{"unknown_type", 0},
		{"unknown_type", 0},
		{"unknown_type", 0},
		{"unknown_type", 0},
		{"vk_any", 0}, /* 0x20 */
		{"vk_instance", 0},
		{"vk_physicaldevice", 0},
		{"vk_device", 0},
		{"vk_queue", 0},
		{"vk_cmdbuffer", 0},
		{"vk_devicememory", 0},
		{"vk_buffer", 0},
		{"vk_bufferview", 0},
		{"vk_image", 0},
		{"vk_imageview", 0},
		{"vk_shadermodule", 0},
		{"vk_pipeline", 0},
		{"vk_pipelinecache", 0},
		{"vk_pipelinelayout", 0},
		{"vk_sampler", 0},
		{"vk_samplerycbcrconversionkhr", 0}, /* 0x30 */
		{"vk_descriptorset", 0},
		{"vk_descriptorsetlayout", 0},
		{"vk_descriptorpool", 0},
		{"vk_fence", 0},
		{"vk_semaphore", 0},
		{"vk_event", 0},
		{"vk_querypool", 0},
		{"vk_framebuffer", 0},
		{"vk_renderpass", 0},
		{"vk_program", 0},
		{"vk_commandpool", 0},
		{"vk_surfacekhr", 0},
		{"vk_swapchainkhr", 0},
		{"vk_descriptorupdatetemplate", 0},
		{"vk_deferredoperationkhr", 0},
		{"vk_privatedataslotext", 0}, /* 0x40 */
		{"vk_debug_utils", 0},
		{"vk_tensor", 0},
		{"vk_tensorview", 0},
		{"vk_mlpipeline", 0},
		{"vk_acceleration_structure", 0},
	};

	for (i = 0; i < ARRAY_SIZE(gpu_mem_types); i++)
		gpu_mem_types[i].size = 0;

	mutex_lock(&priv->lock);
	for (rb = rb_first(&priv->mem_allocated); rb; rb = rb_next(rb)) {
		tmp = rb_entry(rb, struct hgsl_mem_node, mem_rb_node);
		memtype = GET_MEMTYPE(tmp->flags);
		if (memtype < ARRAY_SIZE(gpu_mem_types))
			gpu_mem_types[memtype].size += tmp->memdesc.size;
	}
	mutex_unlock(&priv->lock);

	seq_printf(s, "%16s %16s\n", "type", "size");
	for (i = 0; i < ARRAY_SIZE(gpu_mem_types); i++) {
		if (gpu_mem_types[i].size != 0)
			seq_printf(s, "%16s %16d\n",
					gpu_mem_types[i].name,
					gpu_mem_types[i].size);
	}


	return 0;
}
DEFINE_SHOW_ATTRIBUTE(hgsl_client_memtype);

int hgsl_debugfs_client_init(struct hgsl_priv *priv)
{
	struct qcom_hgsl *hgsl = priv->dev;
	unsigned char name[16];
	struct dentry *ret;

	snprintf(name, sizeof(name), "%d", priv->pid);
	ret = debugfs_create_dir(name,
				hgsl->clients_debugfs);
	if (IS_ERR(ret)) {
		pr_warn("Create debugfs proc node failed.\n");
		priv->debugfs_client = NULL;
		return PTR_ERR(ret);
	} else
		priv->debugfs_client = ret;

	priv->debugfs_mem = debugfs_create_file("mem", 0444,
			priv->debugfs_client,
			priv,
			&hgsl_client_mem_fops);

	priv->debugfs_memtype = debugfs_create_file("obj_types", 0444,
			priv->debugfs_client,
			priv,
			&hgsl_client_memtype_fops);

	return 0;
}

void hgsl_debugfs_client_release(struct hgsl_priv *priv)
{
	debugfs_remove_recursive(priv->debugfs_client);
}

void hgsl_debugfs_init(struct platform_device *pdev)
{
	struct qcom_hgsl *hgsl = platform_get_drvdata(pdev);
	struct dentry *root;

	root = debugfs_create_dir("hgsl", NULL);

	hgsl->debugfs = root;
	hgsl->clients_debugfs = debugfs_create_dir("clients", root);

	hgsl->debugfs_stat = debugfs_create_file("stat", 0444,
		root, hgsl, &hgsl_stat_fops);
}

void hgsl_debugfs_release(struct platform_device *pdev)
{
	struct qcom_hgsl *hgsl = platform_get_drvdata(pdev);

	debugfs_remove_recursive(hgsl->debugfs);
}
