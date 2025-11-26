/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 **/
#ifndef __SMCI_BWPROF_H
#define __SMCI_BWPROF_H

#include <linux/smci_object.h>

#define SMCI_BWPROF_SERVICE_UID UINT32_C(426)
#define SMCI_BWPROF_SERVICE_ERROR_NO_MEM INT32_C(10)
#define SMCI_BWPROF_SERVICE_ERROR_INVALID_ARGS INT32_C(11)
#define SMCI_BWPROF_SERVICE_ERROR_LICENSE_CHECK_FAILED INT32_C(12)
#define SMCI_BWPROF_SERVICE_ERROR_NO_VALID_LICENSE INT32_C(13)
#define SMCI_BWPROF_SERVICE_OP_CHECK_LICENSE_STATUS 0

static inline int32_t smci_bwprof_release(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RELEASE, 0, 0);
}

static inline int32_t smci_bwprof_retain(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RETAIN, 0, 0);
}
/*
 **
 ** The function does license check for bwprof feature.
 **
 ** @param[in]  feature_id_val   Feature id of sub feature whose license check
 is needed.
 ** @param[in]  licensee_ptr    Buffer containing hash of ISVs.
 **
 ** @return
 ** 0 (SMCI_OBJECT_OK) indicates success
 */
static inline int32_t smci_bwprof_license_check(struct smci_object self,
	uint32_t feature_id_val, const void *licensee_ptr, size_t licensee_len)
{
	int32_t result;
	union smci_object_arg a[2];

	a[0].b = (struct smci_object_buf) { &feature_id_val, sizeof(uint32_t) };
	a[1].bi = (struct smci_object_buf_in) { licensee_ptr, licensee_len *
		sizeof(uint8_t) };
	result = smci_object_invoke(self, SMCI_BWPROF_SERVICE_OP_CHECK_LICENSE_STATUS,
			a, SMCI_OBJECT_COUNTS_PACK(2, 0, 0, 0));
	return result;
}
#endif /* __SMCI_BWPROF_H */

