/*
 * Copyright (c) 2011-2015, 2017-2019 The Linux Foundation. All rights reserved.
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
 * This file dph_hash_table.h contains the definition of the scheduler class.
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#ifndef __DPH_HASH_TABLE_H__
#define __DPH_HASH_TABLE_H__

#include "ani_global.h"
/* Compare MAC addresses, return true if same */
static inline uint8_t dph_compare_mac_addr(uint8_t addr1[], uint8_t addr2[])
{
	return (addr1[0] == addr2[0]) &&
		(addr1[1] == addr2[1]) &&
		(addr1[2] == addr2[2]) &&
		(addr1[3] == addr2[3]) &&
		(addr1[4] == addr2[4]) && (addr1[5] == addr2[5]);
}

/**
 * struct dph_hash_table - DPH hash table
 * @pHashTable: The actual hash table
 * @pDphNodeArray: The state array
 * @size: The size of the hash table
 */
struct dph_hash_table {
	tpDphHashNode *pHashTable;
	tDphHashNode *pDphNodeArray;
	uint16_t size;
};

tpDphHashNode dph_lookup_hash_entry(struct mac_context *mac, uint8_t staAddr[],
				    uint16_t *pStaId,
				    struct dph_hash_table *hash_table);

/* Get a pointer to the hash node */
tpDphHashNode dph_get_hash_entry(struct mac_context *mac, uint16_t staId,
				 struct dph_hash_table *hash_table);

/* Add an entry to the hash table */
tpDphHashNode dph_add_hash_entry(struct mac_context *mac,
				 tSirMacAddr staAddr,
				 uint16_t staId,
				 struct dph_hash_table *hash_table);

/* Delete an entry from the hash table */
QDF_STATUS dph_delete_hash_entry(struct mac_context *mac,
				 tSirMacAddr staAddr, uint16_t staId,
				 struct dph_hash_table *hash_table);

/**
 * dph_hash_table_init - Initialize a DPH Hash Table
 * @mac: Global MAC Context
 * @hash_table: Pointer to the Hash Table to initialize
 */
void dph_hash_table_init(struct mac_context *mac,
			 struct dph_hash_table *hash_table);

/* Initialize STA state */
tpDphHashNode dph_init_sta_state(struct mac_context *mac,
				 tSirMacAddr staAddr,
				 uint16_t staId,
				 struct dph_hash_table *hash_table);

#endif
