/*
 * Copyright (c) 2013-2020 The Linux Foundation. All rights reserved.
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
 * @file wlan_hdd_wowl.c
 *
 * @brief wake up on WLAN API file
 */

/* Include Files */

#include "qdf_str.h"
#include <wlan_hdd_includes.h>
#include <wlan_hdd_wowl.h>
#include <wlan_pmo_wow_public_struct.h>
#include "wlan_hdd_object_manager.h"

/* Preprocessor Definitions and Constants */
#define WOWL_INTER_PTRN_TOKENIZER   ';'
#define WOWL_INTRA_PTRN_TOKENIZER   ':'

/* Type Declarations */

static char *g_hdd_wowl_ptrns[WOWL_MAX_PTRNS_ALLOWED];
static bool g_hdd_wowl_ptrns_debugfs[WOWL_MAX_PTRNS_ALLOWED] = { 0 };

static uint8_t g_hdd_wowl_ptrns_count;

static inline int find_ptrn_len(const char *ptrn)
{
	int len = 0;

	while (*ptrn != '\0' && *ptrn != WOWL_INTER_PTRN_TOKENIZER) {
		len++;
		ptrn++;
	}
	return len;
}

/**
 * dump_hdd_wowl_ptrn() - log wow patterns
 * @ptrn: pointer to wow pattern
 *
 * Return: none
 */
static void dump_hdd_wowl_ptrn(struct pmo_wow_add_pattern *ptrn)
{
	hdd_debug("Dumping WOW pattern");
	hdd_nofl_debug("Pattern Id = 0x%x", ptrn->pattern_id);
	hdd_nofl_debug("Pattern Byte Offset = 0x%x", ptrn->pattern_byte_offset);
	hdd_nofl_debug("Pattern_size = 0x%x", ptrn->pattern_size);
	hdd_nofl_debug("Pattern_mask_size = 0x%x", ptrn->pattern_mask_size);
	hdd_nofl_debug("Pattern: ");
	qdf_trace_hex_dump(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
			   ptrn->pattern, ptrn->pattern_size);
	hdd_nofl_debug("pattern_mask: ");
	qdf_trace_hex_dump(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
			   ptrn->pattern_mask, ptrn->pattern_mask_size);
}

static QDF_STATUS
hdd_get_num_wow_filters(struct hdd_context *hdd_ctx, uint8_t *num_filters)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc = hdd_ctx->psoc;

	status = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_HDD_ID_OBJ_MGR);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	*num_filters = ucfg_pmo_get_num_wow_filters(hdd_ctx->psoc);

	wlan_objmgr_psoc_release_ref(psoc, WLAN_HDD_ID_OBJ_MGR);

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_add_wowl_ptrn() - Function which will add the WoWL pattern to be
 *			 used when PBM filtering is enabled
 * @adapter: pointer to the adapter
 * @ptrn: pointer to the pattern string to be added
 *
 * Return: false if any errors encountered, true otherwise
 */
bool hdd_add_wowl_ptrn(struct hdd_adapter *adapter, const char *ptrn)
{
	struct pmo_wow_add_pattern wow_pattern;
	int i, empty_slot, len, offset;
	QDF_STATUS status;
	const char *temp;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	uint8_t num_filters;
	bool invalid_ptrn = false;
	struct wlan_objmgr_vdev *vdev;

	status = hdd_get_num_wow_filters(hdd_ctx, &num_filters);
	if (QDF_IS_STATUS_ERROR(status))
		return false;

	/* There has to have at least 1 byte for each field (pattern
	 * size, mask size, pattern, mask) e.g. PP:QQ:RR:SS ==> 11
	 * chars
	 */
	len = find_ptrn_len(ptrn);
	while (len >= 11) {
		empty_slot = -1;

		/* check if pattern is already configured */
		for (i = num_filters - 1; i >= 0; i--) {
			if (!g_hdd_wowl_ptrns[i]) {
				empty_slot = i;
				continue;
			}

			if (strlen(g_hdd_wowl_ptrns[i]) == len) {
				if (!memcmp(ptrn, g_hdd_wowl_ptrns[i], len)) {
					hdd_err("WoWL pattern '%s' already configured",
						g_hdd_wowl_ptrns[i]);
					ptrn += len;
					goto next_ptrn;
				}
			}
		}

		/* Maximum number of patterns have been configured already */
		if (empty_slot == -1) {
			hdd_err("Max WoW patterns (%u) reached", num_filters);
			return false;
		}

		/* Validate the pattern */
		if (ptrn[2] != WOWL_INTRA_PTRN_TOKENIZER ||
		    ptrn[5] != WOWL_INTRA_PTRN_TOKENIZER) {
			hdd_err("Malformed pattern string. Skip!");
			invalid_ptrn = true;
			ptrn += len;
			goto next_ptrn;
		}

		/* Extract the pattern size */
		wow_pattern.pattern_size =
			(hex_to_bin(ptrn[0]) * 0x10) + hex_to_bin(ptrn[1]);

		/* Extract the pattern mask size */
		wow_pattern.pattern_mask_size =
			(hex_to_bin(ptrn[3]) * 0x10) + hex_to_bin(ptrn[4]);

		if (wow_pattern.pattern_size > PMO_WOWL_BCAST_PATTERN_MAX_SIZE
		    || wow_pattern.pattern_mask_size >
		    WOWL_PTRN_MASK_MAX_SIZE) {
			hdd_err("Invalid length specified. Skip!");
			invalid_ptrn = true;
			ptrn += len;
			goto next_ptrn;
		}

		/* compute the offset of tokenizer after the pattern */
		offset = 5 + 2 * wow_pattern.pattern_size + 1;
		if ((offset >= len) ||
		    (ptrn[offset] != WOWL_INTRA_PTRN_TOKENIZER)) {
			hdd_err("Malformed pattern string..skip!");
			invalid_ptrn = true;
			ptrn += len;
			goto next_ptrn;
		}

		/* compute the end of pattern sring */
		offset = offset + 2 * wow_pattern.pattern_mask_size;
		if (offset + 1 != len) {
			/* offset begins with 0 */
			hdd_err("Malformed pattern string...skip!");
			invalid_ptrn = true;
			ptrn += len;
			goto next_ptrn;
		}

		temp = ptrn;

		/* Now advance to where pattern begins */
		ptrn += 6;

		/* Extract the pattern */
		for (i = 0; i < wow_pattern.pattern_size; i++) {
			wow_pattern.pattern[i] =
				(hex_to_bin(ptrn[0]) * 0x10) +
				hex_to_bin(ptrn[1]);
			ptrn += 2;      /* skip to next byte */
		}

		/* Skip over the ':' separator after the pattern */
		ptrn++;

		/* Extract the pattern Mask */
		for (i = 0; i < wow_pattern.pattern_mask_size; i++) {
			wow_pattern.pattern_mask[i] =
				(hex_to_bin(ptrn[0]) * 0x10) +
				hex_to_bin(ptrn[1]);
			ptrn += 2;      /* skip to next byte */
		}

		/* All is good. Store the pattern locally */
		g_hdd_wowl_ptrns[empty_slot] = qdf_mem_malloc(len + 1);
		if (!g_hdd_wowl_ptrns[empty_slot])
			return false;

		memcpy(g_hdd_wowl_ptrns[empty_slot], temp, len);
		g_hdd_wowl_ptrns[empty_slot][len] = '\0';
		wow_pattern.pattern_id = empty_slot;
		wow_pattern.pattern_byte_offset = 0;

		vdev = hdd_objmgr_get_vdev(adapter);
		if (!vdev) {
			hdd_err("vdev is null");
			qdf_mem_free(g_hdd_wowl_ptrns[empty_slot]);
			g_hdd_wowl_ptrns[empty_slot] = NULL;
			return false;
		}
		/* Register the pattern downstream */
		status = ucfg_pmo_add_wow_user_pattern(vdev, &wow_pattern);
		if (QDF_IS_STATUS_ERROR(status)) {
			/* Add failed, so invalidate the local storage */
			hdd_err("sme_wowl_add_bcast_pattern failed with error code (%d)",
				status);
			qdf_mem_free(g_hdd_wowl_ptrns[empty_slot]);
			g_hdd_wowl_ptrns[empty_slot] = NULL;
		}
		hdd_objmgr_put_vdev(vdev);
		dump_hdd_wowl_ptrn(&wow_pattern);

next_ptrn:
		if (*ptrn == WOWL_INTER_PTRN_TOKENIZER) {
			/* move past the tokenizer */
			ptrn += 1;
			len = find_ptrn_len(ptrn);
			continue;
		} else {
			break;
		}
	}

	if (invalid_ptrn)
		return false;

	return true;
}

/**
 * hdd_del_wowl_ptrn() - Function which will remove a WoWL pattern
 * @adapter: pointer to the adapter
 * @ptrn: pointer to the pattern string to be removed
 *
 * Return: false if any errors encountered, true otherwise
 */
bool hdd_del_wowl_ptrn(struct hdd_adapter *adapter, const char *ptrn)
{
	uint8_t id;
	bool patternFound = false;
	QDF_STATUS status;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	uint8_t num_filters;
	struct wlan_objmgr_vdev *vdev;

	status = hdd_get_num_wow_filters(hdd_ctx, &num_filters);
	if (QDF_IS_STATUS_ERROR(status))
		return false;

	/* lookup pattern */
	for (id = 0; id < num_filters; id++) {
		if (!g_hdd_wowl_ptrns[id])
			continue;

		if (qdf_str_eq(ptrn, g_hdd_wowl_ptrns[id])) {
			patternFound = true;
			break;
		}
	}

	/* If pattern present, remove it from downstream */
	if (!patternFound)
		return false;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return false;

	status = ucfg_pmo_del_wow_user_pattern(vdev, id);
	hdd_objmgr_put_vdev(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		return false;

	/* Remove from local storage as well */
	hdd_err("Deleted pattern with id %d [%s]", id, g_hdd_wowl_ptrns[id]);

	qdf_mem_free(g_hdd_wowl_ptrns[id]);
	g_hdd_wowl_ptrns[id] = NULL;

	return true;
}

/**
 * hdd_add_wowl_ptrn_debugfs() - Function which will add a WoW pattern
 *				 sent from debugfs interface
 * @adapter: pointer to the adapter
 * @pattern_idx: index of the pattern to be added
 * @pattern_offset: offset of the pattern in the frame payload
 * @pattern_buf: pointer to the pattern hex string to be added
 * @pattern_mask: pointer to the pattern mask hex string
 *
 * Return: false if any errors encountered, true otherwise
 */
bool hdd_add_wowl_ptrn_debugfs(struct hdd_adapter *adapter, uint8_t pattern_idx,
			       uint8_t pattern_offset, char *pattern_buf,
			       char *pattern_mask)
{
	struct pmo_wow_add_pattern wow_pattern;
	QDF_STATUS qdf_ret_status;
	uint16_t pattern_len, mask_len, i;
	struct wlan_objmgr_vdev *vdev;

	if (pattern_idx > (WOWL_MAX_PTRNS_ALLOWED - 1)) {
		hdd_err("WoW pattern index %d is out of range (0 ~ %d)",
			pattern_idx, WOWL_MAX_PTRNS_ALLOWED - 1);

		return false;
	}

	pattern_len = strlen(pattern_buf);

	/* Since the pattern is a hex string, 2 characters represent 1 byte. */
	if (pattern_len % 2) {
		hdd_err("Malformed WoW pattern!");

		return false;
	}

	pattern_len >>= 1;
	if (!pattern_len || pattern_len > WOWL_PTRN_MAX_SIZE) {
		hdd_err("WoW pattern length %d is out of range (1 ~ %d).",
			pattern_len, WOWL_PTRN_MAX_SIZE);

		return false;
	}

	wow_pattern.pattern_id = pattern_idx;
	wow_pattern.pattern_byte_offset = pattern_offset;
	wow_pattern.pattern_size = pattern_len;

	if (wow_pattern.pattern_size > PMO_WOWL_BCAST_PATTERN_MAX_SIZE) {
		hdd_err("WoW pattern size (%d) greater than max (%d)",
			wow_pattern.pattern_size,
			PMO_WOWL_BCAST_PATTERN_MAX_SIZE);
		return false;
	}
	/* Extract the pattern */
	for (i = 0; i < wow_pattern.pattern_size; i++) {
		wow_pattern.pattern[i] =
			(hex_to_bin(pattern_buf[0]) << 4) +
			hex_to_bin(pattern_buf[1]);

		/* Skip to next byte */
		pattern_buf += 2;
	}

	/* Get pattern mask size by pattern length */
	wow_pattern.pattern_mask_size = pattern_len >> 3;
	if (pattern_len % 8)
		wow_pattern.pattern_mask_size += 1;

	mask_len = strlen(pattern_mask);
	if ((mask_len % 2)
	    || (wow_pattern.pattern_mask_size != (mask_len >> 1))) {
		hdd_err("Malformed WoW pattern mask!");

		return false;
	}
	if (wow_pattern.pattern_mask_size > WOWL_PTRN_MASK_MAX_SIZE) {
		hdd_err("WoW pattern mask size (%d) greater than max (%d)",
			wow_pattern.pattern_mask_size,
			WOWL_PTRN_MASK_MAX_SIZE);
		return false;
	}
	/* Extract the pattern mask */
	for (i = 0; i < wow_pattern.pattern_mask_size; i++) {
		wow_pattern.pattern_mask[i] =
			(hex_to_bin(pattern_mask[0]) << 4) +
			hex_to_bin(pattern_mask[1]);

		/* Skip to next byte */
		pattern_mask += 2;
	}

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return false;

	/* Register the pattern downstream */
	qdf_ret_status = ucfg_pmo_add_wow_user_pattern(vdev, &wow_pattern);
	hdd_objmgr_put_vdev(vdev);
	if (!QDF_IS_STATUS_SUCCESS(qdf_ret_status)) {
		hdd_err("pmo_wow_user_pattern failed with error code (%d).",
			  qdf_ret_status);

		return false;
	}

	/* All is good. */
	if (!g_hdd_wowl_ptrns_debugfs[pattern_idx]) {
		g_hdd_wowl_ptrns_debugfs[pattern_idx] = 1;
		g_hdd_wowl_ptrns_count++;
	}

	dump_hdd_wowl_ptrn(&wow_pattern);

	return true;
}

/**
 * hdd_del_wowl_ptrn_debugfs() - Function which will remove a WoW pattern
 *				 sent from debugfs interface
 * @adapter: pointer to the adapter
 * @pattern_idx: index of the pattern to be removed
 *
 * Return: false if any errors encountered, true otherwise
 */
bool hdd_del_wowl_ptrn_debugfs(struct hdd_adapter *adapter,
			       uint8_t pattern_idx)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS qdf_ret_status;

	if (pattern_idx > (WOWL_MAX_PTRNS_ALLOWED - 1)) {
		hdd_err("WoW pattern index %d is not in the range (0 ~ %d).",
			pattern_idx, WOWL_MAX_PTRNS_ALLOWED - 1);

		return false;
	}

	if (!g_hdd_wowl_ptrns_debugfs[pattern_idx]) {
		hdd_err("WoW pattern %d is not in the table.",
			pattern_idx);

		return false;
	}

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return false;

	qdf_ret_status = ucfg_pmo_del_wow_user_pattern(vdev, pattern_idx);
	hdd_objmgr_put_vdev(vdev);
	if (!QDF_IS_STATUS_SUCCESS(qdf_ret_status)) {
		hdd_err("sme_wowl_del_bcast_pattern failed with error code (%d).",
			 qdf_ret_status);

		return false;
	}

	g_hdd_wowl_ptrns_debugfs[pattern_idx] = 0;
	g_hdd_wowl_ptrns_count--;

	return true;
}

void hdd_free_user_wowl_ptrns(void)
{
	int i;

	for (i = 0; i < WOWL_MAX_PTRNS_ALLOWED; ++i) {
		if (g_hdd_wowl_ptrns[i]) {
			qdf_mem_free(g_hdd_wowl_ptrns[i]);
			g_hdd_wowl_ptrns[i] = NULL;
		}
	}
}
