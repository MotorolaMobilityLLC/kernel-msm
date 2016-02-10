/*
 * Copyright (c) 2014-2015 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/**=============================================================================
  wlan_hdd_dp_utils.c

  \brief      Utility functions for data path module

  Description...

  ==============================================================================**/
/* $HEADER$ */

/**-----------------------------------------------------------------------------
  Include files
  ----------------------------------------------------------------------------*/
#include <wlan_hdd_dp_utils.h>
#include <wlan_hdd_main.h>
#include <sirDebug.h>

/**-----------------------------------------------------------------------------
  Preprocessor definitions and constants
 ----------------------------------------------------------------------------*/

/**-----------------------------------------------------------------------------
  Type declarations
 ----------------------------------------------------------------------------*/

/**-----------------------------------------------------------------------------
  Function declarations and documentation
 ----------------------------------------------------------------------------*/


VOS_STATUS hdd_list_insert_front( hdd_list_t *pList, hdd_list_node_t *pNode )
{
   list_add( pNode, &pList->anchor );
   pList->count++;
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_list_insert_back( hdd_list_t *pList, hdd_list_node_t *pNode )
{
   list_add_tail( pNode, &pList->anchor );
   pList->count++;
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_list_insert_back_size( hdd_list_t *pList, hdd_list_node_t *pNode, v_SIZE_t *pSize )
{
   list_add_tail( pNode, &pList->anchor );
   pList->count++;
   *pSize = pList->count;
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_list_remove_front( hdd_list_t *pList, hdd_list_node_t **ppNode )
{
   struct list_head * listptr;

   if ( list_empty( &pList->anchor ) )
   {
      return VOS_STATUS_E_EMPTY;
   }

   listptr = pList->anchor.next;
   *ppNode = listptr;
   list_del(pList->anchor.next);
   pList->count--;

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_list_remove_back( hdd_list_t *pList, hdd_list_node_t **ppNode )
{
   struct list_head * listptr;

   if ( list_empty( &pList->anchor ) )
   {
      return VOS_STATUS_E_EMPTY;
   }

   listptr = pList->anchor.prev;
   *ppNode = listptr;
   list_del(pList->anchor.prev);
   pList->count--;

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_list_remove_node( hdd_list_t *pList,
                                 hdd_list_node_t *pNodeToRemove )
{
   hdd_list_node_t *tmp;
   int found = 0;

   if ( list_empty( &pList->anchor ) )
   {
      return VOS_STATUS_E_EMPTY;
   }

    // verify that pNodeToRemove is indeed part of list pList
   list_for_each(tmp, &pList->anchor)
   {
     if (tmp == pNodeToRemove)
     {
        found = 1;
        break;
     }
   }
   if (found == 0)
       return VOS_STATUS_E_INVAL;

   list_del(pNodeToRemove);
   pList->count--;

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_list_peek_front( hdd_list_t *pList,
                                hdd_list_node_t **ppNode )
{
   struct list_head * listptr;
   if ( list_empty( &pList->anchor ) )
   {
      return VOS_STATUS_E_EMPTY;
   }

   listptr = pList->anchor.next;
   *ppNode = listptr;
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_list_peek_next( hdd_list_t *pList, hdd_list_node_t *pNode,
                               hdd_list_node_t **ppNode )
{
   struct list_head * listptr;
   int found = 0;
   hdd_list_node_t *tmp;

   if ( ( pList == NULL) || ( pNode == NULL) || (ppNode == NULL))
   {
      return VOS_STATUS_E_FAULT;
   }

   if ( list_empty(&pList->anchor) )
   {
       return VOS_STATUS_E_EMPTY;
   }

   // verify that pNode is indeed part of list pList
   list_for_each(tmp, &pList->anchor)
   {
     if (tmp == pNode)
     {
        found = 1;
        break;
     }
   }

   if (found == 0)
   {
      return VOS_STATUS_E_INVAL;
   }

   listptr = pNode->next;
   if (listptr == &pList->anchor)
   {
       return VOS_STATUS_E_EMPTY;
   }

   *ppNode =  listptr;

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_string_to_hex( char *pSrcMac, int length, char *pDescMac )
{
   int i;
   int k;
   char temp[3] = {0};
   int rv;

   //18 is MAC Address length plus the colons
   if ( !pSrcMac && (length > 18 || length < 18) )
   {
      return VOS_STATUS_E_FAILURE;
   }
   i = k = 0;
   while ( i < length )
   {
       memcpy(temp, pSrcMac+i, 2);
       rv = kstrtou8(temp, 16, &pDescMac[k++]);
       if (rv < 0)
           return VOS_STATUS_E_FAILURE;
       i += 3;
   }

   return VOS_STATUS_SUCCESS;
}

#ifdef QCA_FEATURE_RPS
/**
 * hdd_dp_util_send_rps_ind() - send rps indication to daemon
 * @hdd_ctxt: hdd context pointer
 *
 * If RPS feature enabled by INI, send RPS enable indication to daemon
 * Indication contents is the name of interface to find correct sysfs node
 * Should send all available interfaces
 *
 * Return: none
 */
void hdd_dp_util_send_rps_ind(hdd_context_t  *hdd_ctxt)
{
	int i = 0;
	uint8_t cpu_map_list_len = 0;
	hdd_adapter_t *adapter;
	hdd_adapter_list_node_t *adapter_node, *next;
	VOS_STATUS status = VOS_STATUS_SUCCESS;
	struct wlan_rps_data rps_data;

	rps_data.num_queues = NUM_TX_QUEUES;

	hddLog(LOG1, FL("cpu_map_list '%s'"), hdd_ctxt->cfg_ini->cpu_map_list);

	/* in case no cpu map list is provided, simply return */
	if (!strlen(hdd_ctxt->cfg_ini->cpu_map_list)) {
		hddLog(VOS_TRACE_LEVEL_ERROR, FL("no cpu map list found"));
		return;
	}

	if (VOS_STATUS_SUCCESS !=
		hdd_hex_string_to_u16_array(hdd_ctxt->cfg_ini->cpu_map_list,
				rps_data.cpu_map_list,
				&cpu_map_list_len,
				WLAN_SVC_IFACE_NUM_QUEUES)) {
		return;
	}

	rps_data.num_queues =
		(cpu_map_list_len < rps_data.num_queues) ?
				cpu_map_list_len : rps_data.num_queues;

	for (i = 0; i < rps_data.num_queues; i++) {
		hddLog(LOG1, FL("cpu_map_list[%d] = 0x%x"),
			i, rps_data.cpu_map_list[i]);
	}

	status = hdd_get_front_adapter (hdd_ctxt, &adapter_node);
	while (NULL != adapter_node && VOS_STATUS_SUCCESS == status) {
		adapter = adapter_node->pAdapter;
		if (NULL != adapter) {
			strlcpy(rps_data.ifname, adapter->dev->name,
				sizeof(rps_data.ifname));
			wlan_hdd_send_svc_nlink_msg(WLAN_SVC_RPS_ENABLE_IND,
				&rps_data, sizeof(rps_data));
		}
		status = hdd_get_next_adapter (hdd_ctxt, adapter_node, &next);
		adapter_node = next;
	}
}
#endif /* QCA_FEATURE_RPS */

