/*
 * Copyright (c) 2011-2012, 2014-2018 The Linux Foundation. All rights reserved.
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
 * DOC: csr_link_list.c
 *
 * Implementation for the Common link list interfaces.
 */
#include "csr_link_list.h"
#include "qdf_lock.h"
#include "qdf_mem.h"
#include "qdf_trace.h"
#include "qdf_mc_timer.h"

static inline void csr_list_init(tListElem *pList)
{
	pList->last = pList->next = pList;
}

static inline void csr_list_remove_entry(tListElem *pEntry)
{
	tListElem *pLast;
	tListElem *pNext;

	pLast = pEntry->last;
	pNext = pEntry->next;
	pLast->next = pNext;
	pNext->last = pLast;
}

static inline tListElem *csr_list_remove_head(tListElem *pHead)
{
	tListElem *pEntry;
	tListElem *pNext;

	pEntry = pHead->next;
	pNext = pEntry->next;
	pHead->next = pNext;
	pNext->last = pHead;

	return pEntry;
}

static inline tListElem *csr_list_remove_tail(tListElem *pHead)
{
	tListElem *pEntry;
	tListElem *pLast;

	pEntry = pHead->last;
	pLast = pEntry->last;
	pHead->last = pLast;
	pLast->next = pHead;

	return pEntry;
}

static inline void csr_list_insert_tail(tListElem *pHead, tListElem *pEntry)
{
	tListElem *pLast;

	pLast = pHead->last;
	pEntry->last = pLast;
	pEntry->next = pHead;
	pLast->next = pEntry;
	pHead->last = pEntry;
}

static inline void csr_list_insert_head(tListElem *pHead, tListElem *pEntry)
{
	tListElem *pNext;

	pNext = pHead->next;
	pEntry->next = pNext;
	pEntry->last = pHead;
	pNext->last = pEntry;
	pHead->next = pEntry;
}

/* Insert pNewEntry before pEntry */
static void csr_list_insert_entry(tListElem *pEntry, tListElem *pNewEntry)
{
	tListElem *pLast;

	if (!pEntry) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pEntry is Null", __func__);
		return;
	}

	pLast = pEntry->last;
	pLast->next = pNewEntry;
	pEntry->last = pNewEntry;
	pNewEntry->next = pEntry;
	pNewEntry->last = pLast;
}

uint32_t csr_ll_count(tDblLinkList *pList)
{
	uint32_t c = 0;

	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return c;
	}

	if (pList && (LIST_FLAG_OPEN == pList->Flag))
		c = pList->Count;

	return c;
}

void csr_ll_lock(tDblLinkList *pList)
{

	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return;
	}

	if (LIST_FLAG_OPEN == pList->Flag)
		qdf_mutex_acquire(&pList->Lock);
}

void csr_ll_unlock(tDblLinkList *pList)
{

	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return;
	}

	if (LIST_FLAG_OPEN == pList->Flag)
		qdf_mutex_release(&pList->Lock);
}

bool csr_ll_is_list_empty(tDblLinkList *pList, bool fInterlocked)
{
	bool fEmpty = true;

	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return fEmpty;
	}

	if (LIST_FLAG_OPEN == pList->Flag) {
		if (fInterlocked)
			csr_ll_lock(pList);

		fEmpty = csrIsListEmpty(&pList->ListHead);

		if (fInterlocked)
			csr_ll_unlock(pList);
	}
	return fEmpty;
}

bool csr_ll_find_entry(tDblLinkList *pList, tListElem *pEntryToFind)
{
	bool fFound = false;
	tListElem *pEntry;

	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return fFound;
	}

	if (LIST_FLAG_OPEN == pList->Flag) {
		pEntry = csr_ll_peek_head(pList, LL_ACCESS_NOLOCK);

		/* Have to make sure we don't loop back to the head of the list,
		 * which will happen if the entry is NOT on the list.
		 */

		while (pEntry && (pEntry != &pList->ListHead)) {
			if (pEntry == pEntryToFind) {
				fFound = true;
				break;
			}
			pEntry = pEntry->next;
		}

	}
	return fFound;
}

QDF_STATUS csr_ll_open(tDblLinkList *pList)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	QDF_STATUS qdf_status;

	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (LIST_FLAG_OPEN != pList->Flag) {
		pList->Count = 0;
		qdf_status = qdf_mutex_create(&pList->Lock);

		if (QDF_IS_STATUS_SUCCESS(qdf_status)) {
			csr_list_init(&pList->ListHead);
			pList->Flag = LIST_FLAG_OPEN;
		} else
			status = QDF_STATUS_E_FAILURE;
	}
	return status;
}

void csr_ll_close(tDblLinkList *pList)
{
	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return;
	}

	if (LIST_FLAG_OPEN == pList->Flag) {
		/* Make sure the list is empty... */
		csr_ll_purge(pList, LL_ACCESS_LOCK);
		qdf_mutex_destroy(&pList->Lock);
		pList->Flag = LIST_FLAG_CLOSE;
	}
}

void csr_ll_insert_tail(tDblLinkList *pList, tListElem *pEntry,
			bool fInterlocked)
{
	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return;
	}

	if (LIST_FLAG_OPEN == pList->Flag) {
		if (fInterlocked)
			csr_ll_lock(pList);

		csr_list_insert_tail(&pList->ListHead, pEntry);
		pList->Count++;
		if (fInterlocked)
			csr_ll_unlock(pList);
	}
}

void csr_ll_insert_head(tDblLinkList *pList, tListElem *pEntry,
			bool fInterlocked)
{

	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return;
	}

	if (LIST_FLAG_OPEN == pList->Flag) {
		if (fInterlocked)
			csr_ll_lock(pList);

		csr_list_insert_head(&pList->ListHead, pEntry);
		pList->Count++;
		if (fInterlocked)
			csr_ll_unlock(pList);
	}
}

void csr_ll_insert_entry(tDblLinkList *pList, tListElem *pEntry,
			 tListElem *pNewEntry, bool fInterlocked)
{
	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return;
	}

	if (LIST_FLAG_OPEN == pList->Flag) {
		if (fInterlocked)
			csr_ll_lock(pList);

		csr_list_insert_entry(pEntry, pNewEntry);
		pList->Count++;
		if (fInterlocked)
			csr_ll_unlock(pList);
	}
}

tListElem *csr_ll_remove_tail(tDblLinkList *pList, bool fInterlocked)
{
	tListElem *pEntry = NULL;

	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return pEntry;
	}

	if (LIST_FLAG_OPEN == pList->Flag) {
		if (fInterlocked)
			csr_ll_lock(pList);

		if (!csrIsListEmpty(&pList->ListHead)) {
			pEntry = csr_list_remove_tail(&pList->ListHead);
			pList->Count--;
		}
		if (fInterlocked)
			csr_ll_unlock(pList);
	}

	return pEntry;
}

tListElem *csr_ll_peek_tail(tDblLinkList *pList, bool fInterlocked)
{
	tListElem *pEntry = NULL;

	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return pEntry;
	}

	if (LIST_FLAG_OPEN == pList->Flag) {
		if (fInterlocked)
			csr_ll_lock(pList);

		if (!csrIsListEmpty(&pList->ListHead))
			pEntry = pList->ListHead.last;

		if (fInterlocked)
			csr_ll_unlock(pList);
	}

	return pEntry;
}

tListElem *csr_ll_remove_head(tDblLinkList *pList, bool fInterlocked)
{
	tListElem *pEntry = NULL;

	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return pEntry;
	}

	if (LIST_FLAG_OPEN == pList->Flag) {
		if (fInterlocked)
			csr_ll_lock(pList);

		if (!csrIsListEmpty(&pList->ListHead)) {
			pEntry = csr_list_remove_head(&pList->ListHead);
			pList->Count--;
		}

		if (fInterlocked)
			csr_ll_unlock(pList);
	}

	return pEntry;
}

tListElem *csr_ll_peek_head(tDblLinkList *pList, bool fInterlocked)
{
	tListElem *pEntry = NULL;

	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return pEntry;
	}

	if (LIST_FLAG_OPEN == pList->Flag) {
		if (fInterlocked)
			csr_ll_lock(pList);

		if (!csrIsListEmpty(&pList->ListHead))
			pEntry = pList->ListHead.next;

		if (fInterlocked)
			csr_ll_unlock(pList);
	}

	return pEntry;
}

void csr_ll_purge(tDblLinkList *pList, bool fInterlocked)
{
	tListElem *pEntry;

	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return;
	}

	if (LIST_FLAG_OPEN == pList->Flag) {
		if (fInterlocked)
			csr_ll_lock(pList);

		/* Remove everything from the list */
		while ((pEntry = csr_ll_remove_head(pList, LL_ACCESS_NOLOCK)))
			;

		if (fInterlocked)
			csr_ll_unlock(pList);
	}
}

bool csr_ll_remove_entry(tDblLinkList *pList, tListElem *pEntryToRemove,
			 bool fInterlocked)
{
	bool fFound = false;
	tListElem *pEntry;

	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return fFound;
	}

	if (LIST_FLAG_OPEN == pList->Flag) {
		if (fInterlocked)
			csr_ll_lock(pList);

		pEntry = csr_ll_peek_head(pList, LL_ACCESS_NOLOCK);

		/* Have to make sure we don't loop back to the head of the
		 * list, which will happen if the entry is NOT on the list.
		 */
		while (pEntry && (pEntry != &pList->ListHead)) {
			if (pEntry == pEntryToRemove) {
				csr_list_remove_entry(pEntry);
				pList->Count--;

				fFound = true;
				break;
			}

			pEntry = pEntry->next;
		}
		if (fInterlocked)
			csr_ll_unlock(pList);
	}

	return fFound;
}

tListElem *csr_ll_next(tDblLinkList *pList, tListElem *pEntry,
		       bool fInterlocked)
{
	tListElem *pNextEntry = NULL;

	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return pNextEntry;
	}

	if (LIST_FLAG_OPEN == pList->Flag) {
		if (fInterlocked)
			csr_ll_lock(pList);

		if (!csrIsListEmpty(&pList->ListHead)
		    && csr_ll_find_entry(pList, pEntry)) {
			pNextEntry = pEntry->next;
			/* Make sure we don't walk past the head */
			if (pNextEntry == &pList->ListHead)
				pNextEntry = NULL;
		}

		if (fInterlocked)
			csr_ll_unlock(pList);
	}

	return pNextEntry;
}

tListElem *csr_ll_previous(tDblLinkList *pList, tListElem *pEntry,
			   bool fInterlocked)
{
	tListElem *pNextEntry = NULL;

	if (!pList) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Error!! pList is Null", __func__);
		return pNextEntry;
	}

	if (LIST_FLAG_OPEN == pList->Flag) {
		if (fInterlocked)
			csr_ll_lock(pList);

		if (!csrIsListEmpty(&pList->ListHead)
		    && csr_ll_find_entry(pList, pEntry)) {
			pNextEntry = pEntry->last;
			/* Make sure we don't walk past the head */
			if (pNextEntry == &pList->ListHead)
				pNextEntry = NULL;
		}

		if (fInterlocked)
			csr_ll_unlock(pList);
	}

	return pNextEntry;
}
