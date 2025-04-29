// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt) "smci_kernel: %s: " fmt, __func__

#include <linux/smci_clientenv.h>
#include <linux/firmware/qcom/si_object.h>

static int32_t smci_do_invoke(void *context, uint32_t op,
	union smci_object_arg *args, uint32_t counts);

/* Marshaling APIs. */

static void smci_marshal_in(struct si_arg si_args[], union smci_object_arg args[], uint32_t counts)
{
	size_t i = 0;

	SMCI_FOR_ARGS(i, counts, BI) {
		si_args[i].b.addr = args[i].b.ptr;
		si_args[i].b.size = args[i].b.size;

		si_args[i].type = SI_AT_IB;
	}

	SMCI_FOR_ARGS(i, counts, BO) {
		si_args[i].b.addr = args[i].b.ptr;
		si_args[i].b.size = args[i].b.size;

		si_args[i].type = SI_AT_OB;
	}

	SMCI_FOR_ARGS(i, counts, OI) {
		si_args[i].o = args[i].o.context;

		/* No need to temporarily get objects; we trust the kernel clients. */
		si_args[i].type = SI_AT_IO;
	}

	SMCI_FOR_ARGS(i, counts, OO)
		si_args[i].type = SI_AT_OO;
}

static void smci_marshal_out(union smci_object_arg args[], struct si_arg si_args[], uint32_t counts)
{
	size_t i = 0;

	SMCI_FOR_ARGS(i, counts, BO) {
		args[i].b.size = si_args[i].b.size;
	}

	SMCI_FOR_ARGS(i, counts, OO) {
		args[i].o.context = si_args[i].o;
		args[i].o.invoke = smci_do_invoke;
	}
}

/* Invocation function. */
static int32_t smci_do_invoke(void *context, uint32_t op,
	union smci_object_arg *args, uint32_t counts)
{
	int ret, result;
	struct si_object *object = context;
	struct si_arg *si_args;
	struct si_object_invoke_ctx *oic;

	if (SMCI_OBJECT_OP_IS_LOCAL(op)) {
		switch (SMCI_OBJECT_OP_METHODID(op)) {
		case SMCI_OBJECT_OP_RETAIN:
			get_si_object(object);
			return SMCI_OBJECT_OK;
		case SMCI_OBJECT_OP_RELEASE:
			put_si_object(object);
			return SMCI_OBJECT_OK;
		default:
			return SMCI_OBJECT_ERROR_REMOTE;
		}
	}

	/* Allocate resources. */
	oic = kzalloc(sizeof(*oic), GFP_KERNEL);
	if (!oic)
		return SMCI_OBJECT_ERROR_KMEM;

	si_args = kcalloc(SMCI_OBJECT_COUNTS_TOTAL(counts) + 1, sizeof(struct si_arg), GFP_KERNEL);
	if (!si_args) {
		ret = SMCI_OBJECT_ERROR_KMEM;
		goto out_failed;
	}

	pr_debug("%s object invocation with %lu arguments (%04x) and op %d.\n",
		si_object_name(object), SMCI_OBJECT_COUNTS_TOTAL(counts), counts, op);

	/* + INITIATE an invocation. */
	smci_marshal_in(si_args, args, counts);

	ret = si_object_do_invoke(oic, object, op, si_args, &result);
	if (ret) {
		pr_err("si_object_do_invoke failed with %d.\n", ret);
		ret = SMCI_OBJECT_ERROR_UNAVAIL;
		goto out_failed;
	}

	if (!result)
		smci_marshal_out(args, si_args, counts);

	ret = result;

out_failed:

	pr_debug("%s object invocation returned with %d.\n", si_object_name(object), ret);

	kfree(si_args);
	kfree(oic);

	return ret;
}

static int smci_get_root_obj(struct smci_object *root_obj)
{
	root_obj->context = ROOT_SI_OBJECT;
	root_obj->invoke = smci_do_invoke;

	return 0;
}

/**
 * smci_get_client_env_object() - Get a client environment object.
 * @client_env_obj: The client environment object.
 *
 * Returns a client environment object to a client. The object is used to
 * initiate object based IPC with QTEE to request secure services.
 *
 * Return: On success, returns 0; on failure, returns SMCI_OBJECT_ERROR*.
 */
int32_t smci_get_client_env_object(struct smci_object *client_env_obj)
{
	int ret;
	struct smci_object root_obj;

	ret = smci_get_root_obj(&root_obj);
	if (ret) {
		pr_err("Failed to get root_obj (ret = %d).\n", ret);
		return ret;
	}

	ret = smci_clientenv_registerwithcredentials(root_obj, SMCI_OBJECT_NULL, client_env_obj);
	if (ret)
		pr_err("Failed to get client_env_object (ret = %d).\n", ret);

	return ret;
}
EXPORT_SYMBOL_GPL(smci_get_client_env_object);
