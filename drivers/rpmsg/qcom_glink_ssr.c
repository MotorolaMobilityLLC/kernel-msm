// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) 2017, Linaro Ltd.
 */
#include <linux/of.h>
#include <linux/completion.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/rpmsg.h>
#include <linux/rpmsg/qcom_glink.h>
#include <linux/remoteproc/qcom_rproc.h>
#include <linux/ipc_logging.h>

/**
 * struct do_cleanup_msg - The data structure for an SSR do_cleanup message
 * @version:	The G-Link SSR protocol version
 * @command:	The G-Link SSR command - do_cleanup
 * @seq_num:	Sequence number
 * @name_len:	Length of the name of the subsystem being restarted
 * @name:	G-Link edge name of the subsystem being restarted
 */
struct do_cleanup_msg {
	__le32 version;
	__le32 command;
	__le32 seq_num;
	__le32 name_len;
	char name[32];
};

/**
 * struct cleanup_done_msg - The data structure for an SSR cleanup_done message
 * @version:	The G-Link SSR protocol version
 * @response:	The G-Link SSR response to a do_cleanup command, cleanup_done
 * @seq_num:	Sequence number
 */
struct cleanup_done_msg {
	__le32 version;
	__le32 response;
	__le32 seq_num;
};

/**
 * G-Link SSR protocol commands
 */
#define GLINK_SSR_DO_CLEANUP	0
#define GLINK_SSR_CLEANUP_DONE	1

struct glink_ssr {
	struct device *dev;
	struct rpmsg_endpoint *ept;

	struct notifier_block nb;

	u32 seq_num;
	struct completion completion;
};

#define MSM_SSR_LOG_PAGE_CNT 4
static void *ssr_ilc;

#define MSM_SSR_INFO(x, ...) ipc_log_string(ssr_ilc, x, ##__VA_ARGS__)

/* Notifier list for all registered glink_ssr instances */
static BLOCKING_NOTIFIER_HEAD(ssr_notifiers);

static struct rpmsg_endpoint *rpm_ept;
static u32 sequence_num;

/**
 * qcom_glink_ssr_notify() - notify GLINK SSR about stopped remoteproc
 * @ssr_name:	name of the remoteproc that has been stopped
 */
void qcom_glink_ssr_notify(const char *ssr_name)
{
	blocking_notifier_call_chain(&ssr_notifiers, 0, (void *)ssr_name);
}
EXPORT_SYMBOL_GPL(qcom_glink_ssr_notify);

static int qcom_glink_ssr_callback(struct rpmsg_device *rpdev,
				   void *data, int len, void *priv, u32 addr)
{
	struct cleanup_done_msg *msg = data;
	struct glink_ssr *ssr = dev_get_drvdata(&rpdev->dev);

	if (len < sizeof(*msg)) {
		dev_err(ssr->dev, "message too short\n");
		return -EINVAL;
	}

	if (le32_to_cpu(msg->version) != 0)
		return -EINVAL;

	if (le32_to_cpu(msg->response) != GLINK_SSR_CLEANUP_DONE)
		return 0;

	if (le32_to_cpu(msg->seq_num) != ssr->seq_num) {
		dev_err(ssr->dev, "invalid sequence number of response\n");
		return -EINVAL;
	}

	complete(&ssr->completion);

	return 0;
}

#if IS_ENABLED(CONFIG_DEEPSLEEP) && IS_ENABLED(CONFIG_RPMSG_QCOM_GLINK_RPM)
void glink_ssr_notify_rpm(void)
{
	struct do_cleanup_msg msg;
	int ret;

	sequence_num = sequence_num + 1;
	memset(&msg, 0, sizeof(msg));
	msg.command = cpu_to_le32(GLINK_SSR_DO_CLEANUP);
	msg.seq_num = cpu_to_le32(sequence_num);
	msg.name_len = cpu_to_le32(strlen("apss"));
	strscpy(msg.name, "apss", sizeof(msg.name));

	MSM_SSR_INFO("%s: notify of %s Deep Sleep seq_num:%d\n",
				"rpm", "apss", sequence_num);

	ret = rpmsg_send(rpm_ept, &msg, sizeof(msg));
	if (ret)
		pr_err("fail to send do cleanup to rpm for APSS SSR %d\n",
					ret);
}
EXPORT_SYMBOL(glink_ssr_notify_rpm);
#endif

static int qcom_glink_ssr_notifier_call(struct notifier_block *nb,
					unsigned long event,
					void *data)
{
	struct glink_ssr *ssr = container_of(nb, struct glink_ssr, nb);
	struct do_cleanup_msg msg;
	char *ssr_name = data;
	int ret;
	struct device *dev;
	const char *name;

	dev = ssr->dev;
	if (!dev || !ssr->ept)
		return NOTIFY_DONE;

	name = dev->parent->of_node->name;
	if (!strcmp(name, "rpm-glink")) {
		if (ssr->seq_num < sequence_num)
			ssr->seq_num = sequence_num;
		sequence_num = sequence_num + 1;
	}

	ssr->seq_num++;
	reinit_completion(&ssr->completion);

	memset(&msg, 0, sizeof(msg));
	msg.command = cpu_to_le32(GLINK_SSR_DO_CLEANUP);
	msg.seq_num = cpu_to_le32(ssr->seq_num);
	msg.name_len = cpu_to_le32(strlen(ssr_name));
	strlcpy(msg.name, ssr_name, sizeof(msg.name));

	ret = rpmsg_send(ssr->ept, &msg, sizeof(msg));
	if (ret < 0)
		dev_err(ssr->dev, "failed to send cleanup message\n");

	ret = wait_for_completion_timeout(&ssr->completion, HZ);
	if (!ret)
		dev_err(ssr->dev, "timeout waiting for cleanup done message\n");

	return NOTIFY_DONE;
}

static int qcom_glink_ssr_probe(struct rpmsg_device *rpdev)
{
	struct glink_ssr *ssr;
	struct device_node *node;
	struct device *dev;

	ssr = devm_kzalloc(&rpdev->dev, sizeof(*ssr), GFP_KERNEL);
	if (!ssr)
		return -ENOMEM;

	init_completion(&ssr->completion);

	ssr->dev = &rpdev->dev;
	ssr->ept = rpdev->ept;
	ssr->nb.notifier_call = qcom_glink_ssr_notifier_call;

	dev = rpdev->dev.parent;
	node = dev->of_node;
	if (!strcmp(node->name, "rpm-glink")) {
		sequence_num = 0;
		rpm_ept = rpdev->ept;
	}

	ssr_ilc = ipc_log_context_create(MSM_SSR_LOG_PAGE_CNT,
				"glink_ssr", 0);

	dev_set_drvdata(&rpdev->dev, ssr);

	return blocking_notifier_chain_register(&ssr_notifiers, &ssr->nb);
}

static void qcom_glink_ssr_remove(struct rpmsg_device *rpdev)
{
	struct glink_ssr *ssr = dev_get_drvdata(&rpdev->dev);

	ssr->dev = NULL;
	ssr->ept = NULL;

	dev_set_drvdata(&rpdev->dev, NULL);

	blocking_notifier_chain_unregister(&ssr_notifiers, &ssr->nb);
}

static const struct rpmsg_device_id qcom_glink_ssr_match[] = {
	{ "glink_ssr" },
	{}
};

static struct rpmsg_driver qcom_glink_ssr_driver = {
	.probe = qcom_glink_ssr_probe,
	.remove = qcom_glink_ssr_remove,
	.callback = qcom_glink_ssr_callback,
	.id_table = qcom_glink_ssr_match,
	.drv = {
		.name = "qcom_glink_ssr",
	},
};
module_rpmsg_driver(qcom_glink_ssr_driver);
