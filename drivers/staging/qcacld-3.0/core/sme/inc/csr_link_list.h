/*
 * Copyright (c) 2011-2012, 2014-2016, 2018-2019 The Linux Foundation. All rights reserved.
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
 * \file csr_link_list.h
 *
 * Exports and types for the Common link list interfaces.
 */

#ifndef CSR_LINK_LIST_H__
#define CSR_LINK_LIST_H__

#include "qdf_lock.h"
#include "qdf_mc_timer.h"
#include "cds_api.h"
#include "sir_types.h"

#define LL_ACCESS_LOCK          true
#define LL_ACCESS_NOLOCK        false

typedef struct tagListElem {
	struct tagListElem *last;
	struct tagListElem *next;
} tListElem;

typedef enum {
	LIST_FLAG_CLOSE = 0,
	LIST_FLAG_OPEN = 0xa1b2c4d7,
} tListFlag;

/* This is a circular double link list */
typedef struct tagDblLinkList {
	tListElem ListHead;
	qdf_mutex_t Lock;
	uint32_t Count;
	tListFlag Flag;
} tDblLinkList;

/*
 * To get the address of an object of (type) base on the (address)
 * of one of its (field)
 */
#define GET_BASE_ADDR(address, type, field) ((type *)( \
			(uint8_t *)(address) - \
			(uint8_t *)(&((type *)0)->field)))
/* To get the offset of (field) inside structure (type) */
#define GET_FIELD_OFFSET(type, field)  ((uintptr_t)(&(((type *)0)->field)))
#define GET_ROUND_UP(_Field, _Boundary) \
	(((_Field) + ((_Boundary) - 1))  & ~((_Boundary) - 1))
#define BITS_ON(_Field, _Bitmask)  ((_Field) |=  (_Bitmask))
#define BITS_OFF(_Field, _Bitmask) ((_Field) &= ~(_Bitmask))
#define csrIsListEmpty(pHead) ((pHead)->next == (pHead))

uint32_t csr_ll_count(tDblLinkList *pList);
QDF_STATUS csr_ll_open(tDblLinkList *pList);
void csr_ll_close(tDblLinkList *pList);
void csr_ll_lock(tDblLinkList *pList);
void csr_ll_unlock(tDblLinkList *pList);
bool csr_ll_is_list_empty(tDblLinkList *pList, bool fInterlocked);
void csr_ll_insert_head(tDblLinkList *pList, tListElem *pEntry,
		bool fInterlocked);
void csr_ll_insert_tail(tDblLinkList *pList, tListElem *pEntry,
		bool fInterlocked);
/* This function put pNewEntry before pEntry. Caller should have found pEntry */
void csr_ll_insert_entry(tDblLinkList *pList, tListElem *pEntry,
		tListElem *pNewEntry, bool fInterlocked);
tListElem *csr_ll_peek_head(tDblLinkList *pList, bool fInterlocked);
tListElem *csr_ll_peek_tail(tDblLinkList *pList, bool fInterlocked);
tListElem *csr_ll_remove_head(tDblLinkList *pList, bool fInterlocked);
tListElem *csr_ll_remove_tail(tDblLinkList *pList, bool fInterlocked);
bool csr_ll_remove_entry(tDblLinkList *pList, tListElem *pEntryToRemove,
		bool fInterlocked);
void csr_ll_purge(tDblLinkList *pList, bool fInterlocked);
/* csr_ll_next return NULL if reaching the end or list is empty */
tListElem *csr_ll_next(tDblLinkList *pList, tListElem *pEntry,
		bool fInterlocked);
tListElem *csr_ll_previous(tDblLinkList *pList, tListElem *pEntry,
		bool fInterlocked);
bool csr_ll_find_entry(tDblLinkList *pList, tListElem *pEntryToFind);
#endif
