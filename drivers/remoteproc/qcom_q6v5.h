/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __QCOM_Q6V5_H__
#define __QCOM_Q6V5_H__

#ifndef SUPPORT_MOT_FEATURES
/*add macro definitions for reference in the qcom_q6v5.c file*/
#define SUPPORT_MOT_FEATURES
#endif

#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/soc/qcom/qcom_aoss.h>

struct icc_path;
struct rproc;
struct qcom_smem_state;
struct qcom_sysmon;

#define PING_TIMEOUT 500 /* in milliseconds */
#define PING_TEST_WAIT 500 /* in milliseconds */

struct string_node {
	struct list_head list;
	char str[];
};

struct qcom_q6v5 {
	struct device *dev;
	struct rproc *rproc;

	struct qcom_smem_state *state;
	struct qcom_smem_state *ping_state;
	struct qmp *qmp;

	struct icc_path *path;
	struct icc_path *crypto_path;

	unsigned int stop_bit;
	unsigned int ping_bit;

	int wdog_irq;
	int fatal_irq;
	int ready_irq;
	int handover_irq;
	int stop_irq;
	int pong_irq;

	struct rproc_subdev *ssr_subdev;
	struct rproc_subdev *glink_subdev;

	struct work_struct crash_handler;

	bool handover_issued;

	struct completion start_done;
	struct completion stop_done;
	struct completion subsys_booted;
	struct completion ping_done;

	int crash_reason;
	int crash_stack;
	unsigned int smem_host_id;

	bool running;

	const char *load_state;
	void (*handover)(struct qcom_q6v5 *q6v5);
	unsigned long long seq;
	unsigned long long crash_seq;

	bool early_boot;

	bool always_ssr;
	struct string_node *always_ssr_reasons;
	struct wakeup_source *ws;
};

int ping_subsystem(struct qcom_q6v5 *q6v5);
int ping_subsystem_init(struct qcom_q6v5 *q6v5, struct platform_device *pdev);

static inline void qcom_q6v5_pas_set_bw(struct qcom_q6v5 *q6v5, u32 avg_bw, u32 peak_bw)
{
	int ret;

	if (!q6v5->crypto_path)
		return;

	ret = icc_set_bw(q6v5->crypto_path, avg_bw, peak_bw);
	if (ret < 0) {
		dev_err(q6v5->dev, "failed to set crypto_path bandwidth request\n");
		icc_set_bw(q6v5->crypto_path, 0, 0);
	}
}

int qcom_q6v5_init(struct qcom_q6v5 *q6v5, struct platform_device *pdev,
		   struct rproc *rproc, int crash_reason, int crash_stack,
		   unsigned int smem_host_id, const char *load_state, bool early_boot,
		   void (*handover)(struct qcom_q6v5 *q6v5));
void qcom_q6v5_deinit(struct qcom_q6v5 *q6v5);

void qcom_q6v5_register_ssr_subdev(struct qcom_q6v5 *q6v5, struct rproc_subdev *ssr_subdev);
void qcom_q6v5_register_glink_subdev(struct qcom_q6v5 *q6v5, struct rproc_subdev *glink_subdev);
int qcom_q6v5_prepare(struct qcom_q6v5 *q6v5);
int qcom_q6v5_unprepare(struct qcom_q6v5 *q6v5);
int qcom_q6v5_request_stop(struct qcom_q6v5 *q6v5, struct qcom_sysmon *sysmon);
int qcom_q6v5_wait_for_start(struct qcom_q6v5 *q6v5, int timeout);
unsigned long qcom_q6v5_panic(struct qcom_q6v5 *q6v5);

#endif
