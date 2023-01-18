/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __QSEECOM_KERNEL_H_
#define __QSEECOM_KERNEL_H_

#include <linux/types.h>


#define QSEECOM_ALIGN_SIZE	0x40
#define QSEECOM_ALIGN_MASK	(QSEECOM_ALIGN_SIZE - 1)
#define QSEECOM_ALIGN(x)	\
	((x + QSEECOM_ALIGN_MASK) & (~QSEECOM_ALIGN_MASK))

/*
 * struct qseecom_handle -
 *      Handle to the qseecom device for kernel clients
 * @dev  - qseecom_dev_handle
 * @sbuf - shared buffer pointer
 * @sbbuf_len - shared buffer size
 */
struct qseecom_handle {
	void *dev; /* in/out */
	unsigned char *sbuf; /* in/out */
	uint32_t sbuf_len; /* in/out */
};

#if IS_ENABLED(CONFIG_QTI_CRYPTO_FDE)
enum QSEECom_key_management_usage_type {
		QSEECOM_KM_USAGE_DISK_ENCRYPTION = 0x01,
		QSEECOM_KM_USAGE_FILE_ENCRYPTION = 0x02,
		QSEECOM_KM_USAGE_UFS_ICE_DISK_ENCRYPTION = 0x03,
		QSEECOM_KM_USAGE_SDCC_ICE_DISK_ENCRYPTION = 0x04,
		QSEECOM_KM_USAGE_MAX
};
#endif

int qseecom_start_app(struct qseecom_handle **handle,
			char *app_name,
			uint32_t size);
int qseecom_shutdown_app(struct qseecom_handle **handle);
int qseecom_send_command(struct qseecom_handle *handle,
			void *send_buf, uint32_t sbuf_len,
			void *resp_buf, uint32_t rbuf_len);
#if IS_ENABLED(CONFIG_QTI_CRYPTO_FDE)
int qseecom_create_key_in_slot(uint8_t usage_code, uint8_t key_slot,
			       const uint8_t *key_id, const uint8_t *inhash32);
#endif

#if IS_ENABLED(CONFIG_QSEECOM_PROXY)
struct qseecom_drv_ops {
	int (*qseecom_send_command)(struct qseecom_handle *handle, void *send_buf,
				uint32_t sbuf_len, void *resp_buf, uint32_t rbuf_len);
	int (*qseecom_start_app)(struct qseecom_handle **handle,
				char *app_name, uint32_t size);
	int (*qseecom_shutdown_app)(struct qseecom_handle **handle);
#if IS_ENABLED(CONFIG_QTI_CRYPTO_FDE)
	int (*qseecom_create_key_in_slot)(uint8_t usage_code, uint8_t key_slot,
					  const uint8_t *key_id, const uint8_t *inhash32);
#endif
};

int provide_qseecom_kernel_fun_ops(const struct qseecom_drv_ops *ops);
#endif /* CONFIG_QSEECOM_PROXY */

#endif /* __QSEECOM_KERNEL_H_ */
