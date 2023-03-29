/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_hdd_unit_test.c
 *
 * config wlan_hdd_unit_test which will be used by wext and
 * debugfs unit_test_host
 */
#include "wlan_hdd_main.h"
#include "qdf_delayed_work_test.h"
#include "qdf_hashtable_test.h"
#include "qdf_periodic_work_test.h"
#include "qdf_ptr_hash_test.h"
#include "qdf_slist_test.h"
#include "qdf_talloc_test.h"
#include "qdf_str.h"
#include "qdf_trace.h"
#include "qdf_tracker_test.h"
#include "qdf_types_test.h"
#include "wlan_dsc_test.h"
#include "wlan_hdd_unit_test.h"

typedef uint32_t (*hdd_ut_callback)(void);

struct hdd_ut_entry {
	const hdd_ut_callback callback;
	const char *name;
};

struct hdd_ut_entry hdd_ut_entries[] = {
	{ .name = "dsc", .callback = dsc_unit_test },
	{ .name = "qdf_delayed_work", .callback = qdf_delayed_work_unit_test },
	{ .name = "qdf_ht", .callback = qdf_ht_unit_test },
	{ .name = "qdf_periodic_work",
	  .callback = qdf_periodic_work_unit_test },
	{ .name = "qdf_ptr_hash", .callback = qdf_ptr_hash_unit_test },
	{ .name = "qdf_slist", .callback = qdf_slist_unit_test },
	{ .name = "qdf_talloc", .callback = qdf_talloc_unit_test },
	{ .name = "qdf_tracker", .callback = qdf_tracker_unit_test },
	{ .name = "qdf_types", .callback = qdf_types_unit_test },
};

#define hdd_for_each_ut_entry(cursor) \
	for (cursor = hdd_ut_entries; \
	     cursor < hdd_ut_entries + ARRAY_SIZE(hdd_ut_entries); \
	     cursor++)

static struct hdd_ut_entry *hdd_ut_lookup(const char *name)
{
	struct hdd_ut_entry *entry;

	hdd_for_each_ut_entry(entry) {
		if (qdf_str_eq(entry->name, name))
			return entry;
	}

	return NULL;
}

static uint32_t hdd_ut_single(const struct hdd_ut_entry *entry)
{
	uint32_t errors;

	hdd_nofl_info("START: '%s'", entry->name);

	errors = entry->callback();
	if (errors)
		hdd_nofl_err("FAIL: '%s' with %u errors", entry->name, errors);
	else
		hdd_nofl_info("PASS: '%s'", entry->name);

	return errors;
}

int wlan_hdd_unit_test(struct hdd_context *hdd_ctx, const char *name)
{
	struct hdd_ut_entry *entry;
	uint32_t errors = 0;

	hdd_nofl_info("Unit tests begin");

	if (!name || !name[0] || qdf_str_eq(name, "all")) {
		hdd_for_each_ut_entry(entry)
			errors += hdd_ut_single(entry);
	} else {
		entry = hdd_ut_lookup(name);
		if (entry) {
			errors += hdd_ut_single(entry);
		} else {
			hdd_nofl_err("Unit test '%s' not found", name);
			return -EINVAL;
		}
	}

	hdd_nofl_info("Unit tests complete");

	return errors ? -EPERM : 0;
}
