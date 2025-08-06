// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/qtee_shmbridge.h>
#include <linux/workqueue.h>
#include <linux/firmware/qcom/si_object.h>

int si_core_get_client_env(struct si_object_invoke_ctx *oic, struct si_object **client_env)
{
	int ret, result;
	struct si_arg args[3] = { 0 };

	args[0].o = NULL_SI_OBJECT;
	args[0].type = SI_AT_IO;
	args[1].type = SI_AT_OO;
	args[2].type = SI_AT_END;

	/* IClientEnv_OP_registerWithCredentials  is 5. */
	ret = si_object_do_invoke(oic, ROOT_SI_OBJECT, 5, args, &result);
	if (ret || result) {
		pr_err("%s failed with result %d(ret = %d).\n", __func__,
		       result, ret);
		return -EINVAL;
	}

	*client_env = args[1].o;

	return 0;
}
EXPORT_SYMBOL_GPL(si_core_get_client_env);

int si_core_client_env_open(struct si_object_invoke_ctx *oic, struct si_object *client_env,
			    u32 uid_val, struct si_object **service)
{
	int ret, result;
	struct si_arg args[3] = { 0 };

	args[0].b = (struct si_buffer) { {&uid_val}, sizeof(uid_val) };
	args[0].type = SI_AT_IB;
	args[1].type = SI_AT_OO;
	args[2].type = SI_AT_END;

	/* IClientEnv_OP_open is 0. */
	ret = si_object_do_invoke(oic, client_env, 0, args, &result);
	if (ret || result) {
		pr_err("%s failed with result %d(ret = %d).\n", __func__,
		       result, ret);
		return -EINVAL;
	}

	*service = args[1].o;

	return 0;
}
EXPORT_SYMBOL_GPL(si_core_client_env_open);

