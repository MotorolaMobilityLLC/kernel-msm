/* Copyright (c) 2013, Motorola Mobility LLC
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

#ifndef MDSS_TIMEOUT_H
#define MDSS_TIMEOUT_H

#include "mdss.h"
#include "mdss_mdp.h"

#ifdef CONFIG_FB_MSM_MDSS_TIMEOUT_DUMP

extern char *mdss_timeout_data;
extern u32 mdss_timeout_data_pos;
extern void mdss_timeout_init(void (*dump_func)(void *data), void *data);
extern void mdss_timeout_dump(const char *timeout_type);

#define MDSS_DUMP_SIZE (2*PAGE_SIZE)
#define MDSS_TIMEOUT_DUMP(fmt, args...) \
	do { \
		if (mdss_timeout_data != NULL) { \
			mdss_timeout_data_pos += scnprintf( \
				&mdss_timeout_data[mdss_timeout_data_pos], \
				MDSS_DUMP_SIZE-mdss_timeout_data_pos-1, \
				fmt, ##args); \
			if (mdss_timeout_data_pos > 0 && \
				mdss_timeout_data[mdss_timeout_data_pos-1] != '\n') \
				mdss_timeout_data_pos += scnprintf( \
					&mdss_timeout_data[mdss_timeout_data_pos], \
					MDSS_DUMP_SIZE-mdss_timeout_data_pos-1, \
					"\n"); \
		} \
	} while (0)

#define MDSS_TIMEOUT_LOG(fmt, args...) \
	do { \
		pr_err(fmt, ##args); \
		MDSS_TIMEOUT_DUMP(fmt, ##args); \
	} while (0)

#else

#define MDSS_TIMEOUT_DUMP(fmt, args...)
#define MDSS_TIMEOUT_LOG(fmt, args...) pr_err(fmt, ##args)

static inline void mdss_timeout_init(void (*dump_func)(void *data), void *data)
	{};
static inline void mdss_timeout_dump(const char *timeout_type)
	{};

#endif

#endif /* MDSS_TIMEOUT_H */
