/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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

/*
 *
 * This file dph_hash_table.cc implements the member functions of
 * DPH hash table class.
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "cds_api.h"
#include "sch_api.h"
#include "dph_global.h"
#include "lim_api.h"
#include "wma_if.h"
#include "wlan_mlme_api.h"

void dph_hash_table_init(struct mac_context *mac,
			 struct dph_hash_table *hash_table)
{
	uint16_t i;

	for (i = 0; i < hash_table->size; i++) {
		hash_table->pHashTable[i] = 0;
	}

	for (i = 0; i < hash_table->size; i++) {
		hash_table->pDphNodeArray[i].valid = 0;
		hash_table->pDphNodeArray[i].added = 0;
		hash_table->pDphNodeArray[i].assocId = i;
	}

}

/* --------------------------------------------------------------------- */
/**
 * hash_function
 *
 * FUNCTION:
 * Hashing function
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param staAddr MAC address of the station
 * @return None
 */

static uint16_t hash_function(struct mac_context *mac, uint8_t staAddr[],
			      uint16_t numSta)
{
	int i;
	uint16_t sum = 0;

	for (i = 0; i < 6; i++)
		sum += staAddr[i];

	return sum % numSta;
}

/* --------------------------------------------------------------------- */
/**
 * dph_lookup_hash_entry
 *
 * FUNCTION:
 * Look up an entry in hash table
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param staAddr MAC address of the station
 * @param pStaId pointer to the Station ID assigned to the station
 * @return pointer to STA hash entry if lookup was a success \n
 *         NULL if lookup was a failure
 */

tpDphHashNode dph_lookup_hash_entry(struct mac_context *mac, uint8_t staAddr[],
				    uint16_t *pAssocId,
				    struct dph_hash_table *hash_table)
{
	tpDphHashNode ptr = NULL;
	uint16_t index = hash_function(mac, staAddr, hash_table->size);

	if (!hash_table->pHashTable) {
		pe_err("pHashTable is NULL");
		return ptr;
	}

	for (ptr = hash_table->pHashTable[index]; ptr; ptr = ptr->next) {
		if (dph_compare_mac_addr(staAddr, ptr->staAddr)) {
			*pAssocId = ptr->assocId;
			break;
		}
	}
	return ptr;
}

/* --------------------------------------------------------------------- */
/**
 * dph_get_hash_entry
 *
 * FUNCTION:
 * Get a pointer to the hash node
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param staId Station ID
 * @return pointer to STA hash entry if lookup was a success \n
 *         NULL if lookup was a failure
 */

tpDphHashNode dph_get_hash_entry(struct mac_context *mac, uint16_t peerIdx,
				 struct dph_hash_table *hash_table)
{
	if (peerIdx < hash_table->size) {
		if (hash_table->pDphNodeArray[peerIdx].added)
			return &hash_table->pDphNodeArray[peerIdx];
		else
			return NULL;
	} else
		return NULL;

}

static inline tpDphHashNode get_node(struct mac_context *mac, uint8_t assocId,
				     struct dph_hash_table *hash_table)
{
	return &hash_table->pDphNodeArray[assocId];
}

/** -------------------------------------------------------------
   \fn dph_init_sta_state
   \brief Initialize STA state. this function saves the staId from the current entry in the DPH table with given assocId
 \ if validStaIdx flag is set. Otherwise it sets the staId to invalid.
   \param  struct mac_context *   mac
   \param  tSirMacAddr staAddr
   \param  uint16_t assocId
   \param  uint8_t validStaIdx -   true ==> the staId in the DPH entry with given assocId is valid and restore it back.
 \                                              false ==> set the staId to invalid.
   \return tpDphHashNode - DPH hash node if found.
   -------------------------------------------------------------*/

tpDphHashNode dph_init_sta_state(struct mac_context *mac, tSirMacAddr staAddr,
				 uint16_t assocId,
				 struct dph_hash_table *hash_table)
{
	tpDphHashNode sta, pnext;

	if (assocId >= hash_table->size) {
		pe_err("Invalid Assoc Id %d", assocId);
		return NULL;
	}

	sta = get_node(mac, (uint8_t) assocId, hash_table);
	pnext = sta->next;

	/* Clear the STA node except for the next pointer */
	qdf_mem_zero((uint8_t *)sta, sizeof(tDphHashNode));
	sta->next = pnext;

	/* Initialize the assocId */
	sta->assocId = assocId;

	/* Initialize STA mac address */
	qdf_mem_copy(sta->staAddr, staAddr, sizeof(tSirMacAddr));

	sta->added = 1;
	sta->is_disassoc_deauth_in_progress = 0;
	sta->sta_deletion_in_progress = false;
	sta->valid = 1;
	return sta;
}

/* --------------------------------------------------------------------- */
/**
 * dph_add_hash_entry
 *
 * FUNCTION:
 * Add entry to hash table
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param staAddr MAC address of the station
 * @param staId Station ID assigned to the station
 * @return Pointer to STA hash entry
 */

tpDphHashNode dph_add_hash_entry(struct mac_context *mac, tSirMacAddr staAddr,
				 uint16_t assocId,
				 struct dph_hash_table *hash_table)
{
	tpDphHashNode ptr, node;
	uint16_t index = hash_function(mac, staAddr, hash_table->size);

	pe_debug("assocId: %d index: %d STA addr: "QDF_MAC_ADDR_FMT,
		 assocId, index, QDF_MAC_ADDR_REF(staAddr));

	if (assocId >= hash_table->size) {
		pe_err("invalid STA id %d", assocId);
		return NULL;
	}

	if (hash_table->pDphNodeArray[assocId].added) {
		pe_err("already added STA %d", assocId);
		return NULL;
	}

	for (ptr = hash_table->pHashTable[index]; ptr; ptr = ptr->next) {
		if (ptr == ptr->next) {
			pe_err("Infinite Loop");
			return NULL;
		}

		if (dph_compare_mac_addr(staAddr, ptr->staAddr)
		    || ptr->assocId == assocId)
			break;
	}

	if (ptr) {
		/* Duplicate entry */
		pe_err("assocId %d hashIndex %d entry exists",
			assocId, index);
		return NULL;
	} else {
		if (dph_init_sta_state
			    (mac, staAddr, assocId, hash_table) == NULL) {
			pe_err("could not Init STA id: %d", assocId);
			return NULL;
		}
		/* Add the node to the link list */
		hash_table->pDphNodeArray[assocId].next =
			hash_table->pHashTable[index];
		hash_table->pHashTable[index] =
			&hash_table->pDphNodeArray[assocId];

		node = hash_table->pHashTable[index];
		return node;
	}
}

/* --------------------------------------------------------------------- */
/**
 * dph_delete_hash_entry
 *
 * FUNCTION:
 * Delete entry from hash table
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param staAddr MAC address of the station
 * @param staId Station ID assigned to the station
 * @return QDF_STATUS_SUCCESS if successful,
 *         QDF_STATUS_E_FAILURE otherwise
 */

QDF_STATUS dph_delete_hash_entry(struct mac_context *mac, tSirMacAddr staAddr,
				 uint16_t assocId,
				 struct dph_hash_table *hash_table)
{
	tpDphHashNode ptr, prev;
	uint16_t index = hash_function(mac, staAddr, hash_table->size);

	pe_debug("assocId: %d index: %d STA addr: "QDF_MAC_ADDR_FMT,
		 assocId, index, QDF_MAC_ADDR_REF(staAddr));

	if (assocId >= hash_table->size) {
		pe_err("invalid STA id %d", assocId);
		return QDF_STATUS_E_FAILURE;
	}

	if (hash_table->pDphNodeArray[assocId].added == 0) {
		pe_err("STA %d never added", assocId);
		return QDF_STATUS_E_FAILURE;
	}

	for (prev = 0, ptr = hash_table->pHashTable[index];
	     ptr; prev = ptr, ptr = ptr->next) {
		if (dph_compare_mac_addr(staAddr, ptr->staAddr))
			break;
		if (prev == ptr) {
			pe_err("Infinite Loop");
			return QDF_STATUS_E_FAILURE;
		}
	}

	if (ptr) {
		/* / Delete the entry after invalidating it */
		ptr->valid = 0;
		memset(ptr->staAddr, 0, sizeof(ptr->staAddr));
		if (prev == 0)
			hash_table->pHashTable[index] = ptr->next;
		else
			prev->next = ptr->next;
		ptr->added = 0;
		ptr->is_disassoc_deauth_in_progress = 0;
		ptr->sta_deletion_in_progress = false;
		ptr->next = 0;
	} else {
		pe_err("Entry not present STA addr: "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(staAddr));
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}


