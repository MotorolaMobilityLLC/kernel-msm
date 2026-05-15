/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/smci_object.h>

struct keymgr_key_info {
	uint32_t key_size;
	uint32_t reserved;
};

#define KEYMANAGER_UID (0x13B)

#define KEY_MGR_HIBERNATE 1
#define KEY_MGR_HIBERNATE_WITH_ENCRYPTION 2

#define KEY_MGR_ERROR_INVALID_EVENT 10
#define KEY_MGR_ERROR_INVALID_OPERATION 11
#define KEY_MGR_ERROR_INVALID_KEYSIZE 12
#define KEY_MGR_ERROR_KEY_GENERATION 13
#define KEY_MGR_ERROR_RPMB_OPERATION 14

#define KEY_MGR_OP_GETKEY 0
#define KEY_MGR_OP_PREPARE 1
#define KEY_MGR_OP_RESERVED 2

static inline int32_t
key_manager_release(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RELEASE, 0, 0);
}

static inline int32_t
key_manager_retain(struct smci_object self)
{
	return smci_object_invoke(self, SMCI_OBJECT_OP_RETAIN, 0, 0);
}

static inline int32_t
key_manager_getkey(struct smci_object self, uint32_t event_val,
			   void *keyout_ptr, size_t keyout_len,
			   size_t *keyout_lenout)
{
	int32_t result = 0;
	union smci_object_arg a[2] = {{{0, 0}}};

	a[0].b = (struct smci_object_buf) { &event_val, sizeof(uint32_t) };
	a[1].b = (struct smci_object_buf) { keyout_ptr, keyout_len * 1 };

	result = smci_object_invoke(self, KEY_MGR_OP_GETKEY, a,
				    SMCI_OBJECT_COUNTS_PACK(1, 1, 0, 0));

	*keyout_lenout = a[1].b.size / 1;

	return result;
}

static inline int32_t
key_manager_prepare(struct smci_object self, uint32_t event_val,
			    const struct keymgr_key_info *keyinfo_ptr)
{
	union smci_object_arg a[1] = {{{0, 0}}};
	struct {
		uint32_t m_event;
		struct keymgr_key_info m_keyinfo;
	} i = {0};

	a[0].b = (struct smci_object_buf) { &i, 12 };
	i.m_event = event_val;
	i.m_keyinfo = *keyinfo_ptr;

	return smci_object_invoke(self, KEY_MGR_OP_PREPARE, a,
				  SMCI_OBJECT_COUNTS_PACK(1, 0, 0, 0));
}
