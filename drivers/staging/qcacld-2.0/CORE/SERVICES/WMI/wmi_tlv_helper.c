/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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

/* wmi_tlv_platform.c file will be different for different components like Pronto firmware, Pronto windows host driver,
     Pronto LA host driver because their memory management functions are different */
#include "wmi_tlv_platform.c"
#include "wmi_tlv_defs.h"
#include "wmi_version.h"

#define WMITLV_GET_ATTRIB_NUM_TLVS  0xFFFFFFFF

#define WMITLV_GET_CMDID(val) (val & 0x00FFFFFF)
#define WMITLV_GET_NUM_TLVS(val) ((val >> 24) & 0xFF)

#define WMITLV_GET_TAGID(val) (val & 0x00000FFF)
#define WMITLV_GET_TAG_STRUCT_SIZE(val) ((val >> 12) & 0x000001FF)
#define WMITLV_GET_TAG_ARRAY_SIZE(val) ((val >> 21) & 0x000001FF)
#define WMITLV_GET_TAG_VARIED(val) ((val >> 30) & 0x00000001)

#define WMITLV_SET_ATTRB0(id) ((WMITLV_GET_TAG_NUM_TLV_ATTRIB(id) << 24) | (id & 0x00FFFFFF))
#define WMITLV_SET_ATTRB1(tagID, tagStructSize, tagArraySize, tagVaried) (((tagVaried&0x1)<<30) | ((tagArraySize&0x1FF)<<21) | ((tagStructSize&0x1FF)<<12) | (tagID&0xFFF))

#define WMITLV_OP_SET_TLV_ATTRIB_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)  \
        WMITLV_SET_ATTRB1(elem_tlv_tag, sizeof(elem_struc_type), arr_size, var_len),

#define WMITLV_GET_CMD_EVT_ATTRB_LIST(id) \
                    WMITLV_SET_ATTRB0(id), \
                    WMITLV_TABLE(id,SET_TLV_ATTRIB,NULL,0)

A_UINT32 cmdAttrList[] =
    {
        WMITLV_ALL_CMD_LIST(WMITLV_GET_CMD_EVT_ATTRB_LIST)
    };

A_UINT32 evtAttrList[] =
    {
        WMITLV_ALL_EVT_LIST(WMITLV_GET_CMD_EVT_ATTRB_LIST)
    };


#ifdef NO_DYNAMIC_MEM_ALLOC
static wmitlv_cmd_param_info *g_WmiStaticCmdParamInfoBuf = NULL;
A_UINT32                      g_WmiStaticMaxCmdParamTlvs=0;
#endif

/* TLV helper routines */

/*
 * WMI TLV Helper function to set the static cmd_param_tlv structure and number of TLVs that can be
 * accomodated in the structure. This function should be used when dynamic memory allocation is not
 * supported.
 *
 * When dynamic memory allocation is not supported by any component then NO_DYNAMIC_MEMALLOC
 * macro has to be defined in respective tlv_platform.c file. And respective component has to allocate
 * cmd_param_tlv structure buffer to accomodate whatever number of TLV's. Both the buffer address
 * and number of TLV's that can be accomodated in the buffer should be sent as arguments to this function.
 *
 * Return None
 */
void
wmitlv_set_static_param_tlv_buf(void *param_tlv_buf, A_UINT32 max_tlvs_accomodated)
{
#ifdef NO_DYNAMIC_MEM_ALLOC
    g_WmiStaticCmdParamInfoBuf = param_tlv_buf;
    g_WmiStaticMaxCmdParamTlvs = max_tlvs_accomodated;
#endif
}

/*
 * WMI TLV Helper functions to find the attributes of the Command/Event TLVs.
 * Return 0 if success. Return >=1 if failure.
 */
A_UINT32 wmitlv_get_attributes(A_UINT32 is_cmd_id, A_UINT32 cmd_event_id, A_UINT32 curr_tlv_order, wmitlv_attributes_struc* tlv_attr_ptr)
{
    A_UINT32 i, base_index, num_tlvs, num_entries;
    A_UINT32 *pAttrArrayList;

    if (is_cmd_id)
    {
        pAttrArrayList = &cmdAttrList[0];
        num_entries = (sizeof(cmdAttrList)/sizeof(A_UINT32));
    }
    else
    {
        pAttrArrayList = &evtAttrList[0];
        num_entries = (sizeof(evtAttrList)/sizeof(A_UINT32));
    }

    for (i = 0; i < num_entries; i++)
    {
        num_tlvs = WMITLV_GET_NUM_TLVS(pAttrArrayList[i]);
        if (WMITLV_GET_CMDID(cmd_event_id) == WMITLV_GET_CMDID(pAttrArrayList[i]))
        {
            tlv_attr_ptr->cmd_num_tlv = num_tlvs;
            /* Return success from here when only number of TLVS for this command/event is required */
            if (curr_tlv_order == WMITLV_GET_ATTRIB_NUM_TLVS)
            {
                wmi_tlv_print_verbose("%s: WMI TLV attribute definitions for %s:0x%x found; num_of_tlvs:%d\n",
                               __func__, (is_cmd_id ? "Cmd" : "Evt"), cmd_event_id, num_tlvs);
                return 0;
            }

            /* Return failure if tlv_order is more than the expected number of TLVs */
            if (curr_tlv_order >= num_tlvs)
            {
                wmi_tlv_print_error("%s: ERROR: TLV order %d greater than num_of_tlvs:%d for %s:0x%x\n",
                               __func__, curr_tlv_order, num_tlvs, (is_cmd_id ? "Cmd" : "Evt"), cmd_event_id);
                return 1;
            }

            base_index = i + 1; // index to first TLV attributes
            wmi_tlv_print_verbose("%s: WMI TLV attributes for %s:0x%x tlv[%d]:0x%x\n",
                           __func__, (is_cmd_id ? "Cmd" : "Evt"), cmd_event_id, curr_tlv_order, pAttrArrayList[(base_index+curr_tlv_order)]);
            tlv_attr_ptr->tag_order = curr_tlv_order;
            tlv_attr_ptr->tag_id = WMITLV_GET_TAGID(pAttrArrayList[(base_index+curr_tlv_order)]);
            tlv_attr_ptr->tag_struct_size = WMITLV_GET_TAG_STRUCT_SIZE(pAttrArrayList[(base_index+curr_tlv_order)]);
            tlv_attr_ptr->tag_varied_size = WMITLV_GET_TAG_VARIED(pAttrArrayList[(base_index+curr_tlv_order)]);
            tlv_attr_ptr->tag_array_size = WMITLV_GET_TAG_ARRAY_SIZE(pAttrArrayList[(base_index+curr_tlv_order)]);
            return 0;
        }
        i += num_tlvs;
    }

    wmi_tlv_print_error("%s: ERROR: Didn't found WMI TLV attribute definitions for %s:0x%x\n",
                   __func__, (is_cmd_id ? "Cmd" : "Evt"), cmd_event_id);
    return 1;
}

/*
 * Helper Function to vaidate the prepared TLV's for an WMI event/command to be sent
 * Return 0 if success.
 *           <0 if failure.
 */
static int
wmitlv_check_tlv_params(
    void *os_handle, void *param_struc_ptr, A_UINT32 param_buf_len, A_UINT32 is_cmd_id, A_UINT32 wmi_cmd_event_id)
{
    wmitlv_attributes_struc attr_struct_ptr;
    A_UINT32 buf_idx = 0;
    A_UINT32 tlv_index = 0;
    A_UINT8 *buf_ptr = (unsigned char *)param_struc_ptr;
    A_UINT32  expected_num_tlvs, expected_tlv_len;

    /* Get the number of TLVs for this command/event */
    if (wmitlv_get_attributes(is_cmd_id, wmi_cmd_event_id, WMITLV_GET_ATTRIB_NUM_TLVS, &attr_struct_ptr) != 0)
    {
        wmi_tlv_print_error("%s: ERROR: Couldn't get expected number of TLVs for Cmd=%d\n",
                           __func__, wmi_cmd_event_id);
        goto Error_wmitlv_check_tlv_params;
    }

    /* NOTE: the returned number of TLVs is in "attr_struct_ptr.cmd_num_tlv" */

    expected_num_tlvs = attr_struct_ptr.cmd_num_tlv;

    while ((buf_idx + WMI_TLV_HDR_SIZE) <= param_buf_len)
    {
        A_UINT32 curr_tlv_tag = WMITLV_GET_TLVTAG(WMITLV_GET_HDR(buf_ptr));
        A_UINT32 curr_tlv_len = WMITLV_GET_TLVLEN(WMITLV_GET_HDR(buf_ptr));

        if ((buf_idx + WMI_TLV_HDR_SIZE + curr_tlv_len) > param_buf_len)
        {
            wmi_tlv_print_error("%s: ERROR: Invalid TLV length for Cmd=%d Tag_order=%d buf_idx=%d Tag:%d Len:%d TotalLen:%d\n",
                               __func__, wmi_cmd_event_id, tlv_index, buf_idx, curr_tlv_tag, curr_tlv_len, param_buf_len);
            goto Error_wmitlv_check_tlv_params;
        }


        /* Get the attributes of the TLV with the given order in "tlv_index" */
        wmi_tlv_OS_MEMZERO(&attr_struct_ptr,sizeof(wmitlv_attributes_struc));
        if (wmitlv_get_attributes(is_cmd_id, wmi_cmd_event_id, tlv_index, &attr_struct_ptr) != 0)
        {
            wmi_tlv_print_error("%s: ERROR: No TLV attributes found for Cmd=%d Tag_order=%d\n",
                               __func__, wmi_cmd_event_id, tlv_index);
            goto Error_wmitlv_check_tlv_params;
        }

        /* Found the TLV that we wanted */
        wmi_tlv_print_verbose("%s: [tlv %d]: tag=%d, len=%d\n", __func__, tlv_index, curr_tlv_tag, curr_tlv_len);

        /* Validating Tag ID order */
        if (curr_tlv_tag != attr_struct_ptr.tag_id) {
            wmi_tlv_print_error("%s: ERROR: TLV has wrong tag in order for Cmd=0x%x. Given=%d, Expected=%d.\n",
                   __func__, wmi_cmd_event_id,curr_tlv_tag, attr_struct_ptr.tag_id);
            goto Error_wmitlv_check_tlv_params;
        }

        /* Validate Tag length */
        /* Array TLVs length checking needs special handling */
        if ((curr_tlv_tag >= WMITLV_TAG_FIRST_ARRAY_ENUM) && (curr_tlv_tag <= WMITLV_TAG_LAST_ARRAY_ENUM))
        {
            if (attr_struct_ptr.tag_varied_size == WMITLV_SIZE_FIX)
            {
                /* Array size can't be invalid for fixed size Array TLV */
                if (WMITLV_ARR_SIZE_INVALID == attr_struct_ptr.tag_array_size){
                    wmi_tlv_print_error("%s: ERROR: array_size can't be invalid for Array TLV Cmd=0x%x Tag=%d\n",
                           __func__, wmi_cmd_event_id, curr_tlv_tag);
                    goto Error_wmitlv_check_tlv_params;
                }

                expected_tlv_len = attr_struct_ptr.tag_array_size * attr_struct_ptr.tag_struct_size;
                /* Paddding is only required for Byte array Tlvs all other array tlv's should be aligned to 4 bytes during their definition */
                if (WMITLV_TAG_ARRAY_BYTE == attr_struct_ptr.tag_id)
                {
                    expected_tlv_len = roundup(expected_tlv_len, sizeof(A_UINT32));
                }

                if (curr_tlv_len != expected_tlv_len){
                    wmi_tlv_print_error("%s: ERROR: TLV has wrong length for Cmd=0x%x. Tag_order=%d  Tag=%d, Given_Len:%d Expected_Len=%d.\n",
                   __func__, wmi_cmd_event_id, tlv_index, curr_tlv_tag, curr_tlv_len, expected_tlv_len);
                    goto Error_wmitlv_check_tlv_params;
                }
            }
            else
            {
                /* Array size should be invalid for variable size Array TLV */
                if (WMITLV_ARR_SIZE_INVALID != attr_struct_ptr.tag_array_size){
                    wmi_tlv_print_error("%s: ERROR: array_size should be invalid for Array TLV Cmd=0x%x Tag=%d\n",
                           __func__, wmi_cmd_event_id, curr_tlv_tag);
                    goto Error_wmitlv_check_tlv_params;
                }

                /* Incase of variable length TLV's, there is no expectation on the length field so do whatever checking
                                you can depending on the TLV tag if TLV length is non-zero */
                if (curr_tlv_len != 0)
                {
                    /* Verify TLV length is aligned to the size of structure */
                    if ((curr_tlv_len%attr_struct_ptr.tag_struct_size)!=0)
                    {
                        wmi_tlv_print_error("%s: ERROR: TLV length %d for Cmd=0x%x is not aligned to size of structure(%d bytes)\n",
                               __func__, curr_tlv_len, wmi_cmd_event_id, attr_struct_ptr.tag_struct_size);
                        goto Error_wmitlv_check_tlv_params;
                    }

                    if (curr_tlv_tag == WMITLV_TAG_ARRAY_STRUC)
                    {
                        A_UINT8     *tlv_buf_ptr = NULL;
                        A_UINT32    in_tlv_len;
                        A_UINT32    idx;
                        A_UINT32    num_of_elems;

                        /* Verify length of inner TLVs */

                        num_of_elems = curr_tlv_len/attr_struct_ptr.tag_struct_size;
                        /* Set tlv_buf_ptr to the first inner TLV address */
                        tlv_buf_ptr = buf_ptr + WMI_TLV_HDR_SIZE;
                        for(idx=0; idx<num_of_elems; idx++)
                        {
                            in_tlv_len = WMITLV_GET_TLVLEN(WMITLV_GET_HDR(tlv_buf_ptr));
                            if ((in_tlv_len + WMI_TLV_HDR_SIZE)!=attr_struct_ptr.tag_struct_size){
                                wmi_tlv_print_error("%s: ERROR: TLV has wrong length for Cmd=0x%x. Tag_order=%d  Tag=%d, Given_Len:%zu Expected_Len=%d.\n",
                               __func__, wmi_cmd_event_id, tlv_index, curr_tlv_tag, (in_tlv_len + WMI_TLV_HDR_SIZE), attr_struct_ptr.tag_struct_size);
                                goto Error_wmitlv_check_tlv_params;
                            }
                            tlv_buf_ptr += in_tlv_len + WMI_TLV_HDR_SIZE;
                        }
                    }
                    else if ((curr_tlv_tag == WMITLV_TAG_ARRAY_UINT32) ||
                             (curr_tlv_tag == WMITLV_TAG_ARRAY_BYTE) ||
                             (curr_tlv_tag == WMITLV_TAG_ARRAY_FIXED_STRUC))
                    {
                        /* Nothing to verify here */
                    }
                    else
                    {
                        wmi_tlv_print_error("%s ERROR Need to handle the Array tlv %d for variable length for Cmd=0x%x\n",
                                   __func__, attr_struct_ptr.tag_id, wmi_cmd_event_id);
                        goto Error_wmitlv_check_tlv_params;
                    }
                }
            }
        }
        else
        {
            /* Non-array TLV. */

            if ((curr_tlv_len + WMI_TLV_HDR_SIZE) != attr_struct_ptr.tag_struct_size)
            {
                wmi_tlv_print_error("%s: ERROR: TLV has wrong length for Cmd=0x%x. Given=%zu, Expected=%d.\n",
                       __func__, wmi_cmd_event_id,(curr_tlv_len + WMI_TLV_HDR_SIZE), attr_struct_ptr.tag_struct_size);
                goto Error_wmitlv_check_tlv_params;
            }
        }

        /* Check TLV length is aligned to 4 bytes or not */
        if ((curr_tlv_len%sizeof(A_UINT32))!=0)
        {
            wmi_tlv_print_error("%s: ERROR: TLV length %d for Cmd=0x%x is not aligned to %zu bytes\n",
                   __func__, curr_tlv_len, wmi_cmd_event_id, sizeof(A_UINT32));
            goto Error_wmitlv_check_tlv_params;
        }

        tlv_index++;
        buf_ptr += curr_tlv_len + WMI_TLV_HDR_SIZE;
        buf_idx += curr_tlv_len + WMI_TLV_HDR_SIZE;
    }

    if (tlv_index!=expected_num_tlvs)
    {
        wmi_tlv_print_verbose("%s: INFO: Less number of TLVs filled for Cmd=0x%x Filled %d Expected=%d\n",
               __func__, wmi_cmd_event_id, tlv_index, expected_num_tlvs);
    }

    return(0);
Error_wmitlv_check_tlv_params:
    return(-1);
}

/*
 * Helper Function to vaidate the prepared TLV's for an WMI event to be sent
 * Return 0 if success.
 *           <0 if failure.
 */
int
wmitlv_check_event_tlv_params(
    void *os_handle, void *param_struc_ptr, A_UINT32 param_buf_len, A_UINT32 wmi_cmd_event_id)
{
    A_UINT32 is_cmd_id = 0;
    return(wmitlv_check_tlv_params(os_handle,param_struc_ptr,param_buf_len,is_cmd_id,wmi_cmd_event_id));
}

/*
 * Helper Function to vaidate the prepared TLV's for an WMI command to be sent
 * Return 0 if success.
 *           <0 if failure.
 */
int
wmitlv_check_command_tlv_params(
    void *os_handle, void *param_struc_ptr, A_UINT32 param_buf_len, A_UINT32 wmi_cmd_event_id)
{
    A_UINT32 is_cmd_id = 1;
    return(wmitlv_check_tlv_params(os_handle,param_struc_ptr,param_buf_len,is_cmd_id,wmi_cmd_event_id));
}


/*
 * Helper Function to vaidate the TLV's coming for an event/command and also pads data to TLV's if necessary
 * Return 0 if success.
              <0 if failure.
 */
static int
wmitlv_check_and_pad_tlvs(
    void *os_handle, void *param_struc_ptr, A_UINT32 param_buf_len, A_UINT32 is_cmd_id, A_UINT32 wmi_cmd_event_id, void **wmi_cmd_struct_ptr)
{
    wmitlv_attributes_struc attr_struct_ptr;
    A_UINT32 buf_idx = 0;
    A_UINT32 tlv_index = 0;
    A_UINT32 num_of_elems = 0;
    int tlv_size_diff=0;
    A_UINT8 *buf_ptr = (unsigned char *)param_struc_ptr;
    wmitlv_cmd_param_info *cmd_param_tlvs_ptr = NULL;
    A_UINT32  remaining_expected_tlvs=0xFFFFFFFF;
    A_UINT32 len_wmi_cmd_struct_buf;

    /* Get the number of TLVs for this command/event */
    if (wmitlv_get_attributes(is_cmd_id, wmi_cmd_event_id, WMITLV_GET_ATTRIB_NUM_TLVS, &attr_struct_ptr) != 0)
    {
        wmi_tlv_print_error("%s: ERROR: Couldn't get expected number of TLVs for Cmd=%d\n",
                           __func__, wmi_cmd_event_id);
        return -1;
    }
    /* NOTE: the returned number of TLVs is in "attr_struct_ptr.cmd_num_tlv" */

    /* Create base structure of format wmi_cmd_event_id##_param_tlvs */
    len_wmi_cmd_struct_buf = attr_struct_ptr.cmd_num_tlv * sizeof(wmitlv_cmd_param_info);
#ifndef NO_DYNAMIC_MEM_ALLOC
    /* Dynamic memory allocation supported */
    wmi_tlv_os_mem_alloc(os_handle, *wmi_cmd_struct_ptr, len_wmi_cmd_struct_buf);
#else
    /* Dynamic memory allocation is not supported. Use the buffer g_WmiStaticCmdParamInfoBuf, which should be set using wmi_tlv_set_static_param_tlv_buf(),
            for base structure of format wmi_cmd_event_id##_param_tlvs */
    *wmi_cmd_struct_ptr = g_WmiStaticCmdParamInfoBuf;
    if (attr_struct_ptr.cmd_num_tlv > g_WmiStaticMaxCmdParamTlvs)
    {
        /* Error: Expecting more TLVs that accomodated for static structure  */
        wmi_tlv_print_error("%s: Error: Expecting more TLVs that accomodated for static structure. Expected:%d Accomodated:%d\n",
               __func__, attr_struct_ptr.cmd_num_tlv, g_WmiStaticMaxCmdParamTlvs);
        return -1;
    }
#endif
    if (*wmi_cmd_struct_ptr == NULL) {
        /* Error: unable to alloc memory */
        wmi_tlv_print_error("%s: Error: unable to alloc memory (size=%d) for TLV\n",
               __func__, len_wmi_cmd_struct_buf);
        return -1;
    }


    cmd_param_tlvs_ptr = (wmitlv_cmd_param_info *)*wmi_cmd_struct_ptr;
    wmi_tlv_OS_MEMZERO(cmd_param_tlvs_ptr, len_wmi_cmd_struct_buf);
    remaining_expected_tlvs = attr_struct_ptr.cmd_num_tlv;

    while (((buf_idx + WMI_TLV_HDR_SIZE) <= param_buf_len)&&(remaining_expected_tlvs))
    {
        A_UINT32 curr_tlv_tag = WMITLV_GET_TLVTAG(WMITLV_GET_HDR(buf_ptr));
        A_UINT32 curr_tlv_len = WMITLV_GET_TLVLEN(WMITLV_GET_HDR(buf_ptr));
        int      num_padding_bytes = 0;

        /* Get the attributes of the TLV with the given order in "tlv_index" */
        wmi_tlv_OS_MEMZERO(&attr_struct_ptr,sizeof(wmitlv_attributes_struc));
        if (wmitlv_get_attributes(is_cmd_id, wmi_cmd_event_id, tlv_index, &attr_struct_ptr) != 0)
        {
            wmi_tlv_print_error("%s: ERROR: No TLV attributes found for Cmd=%d Tag_order=%d\n",
                               __func__, wmi_cmd_event_id, tlv_index);
            goto Error_wmitlv_check_and_pad_tlvs;
        }

        /* Found the TLV that we wanted */
        wmi_tlv_print_verbose("%s: [tlv %d]: tag=%d, len=%d\n", __func__, tlv_index, curr_tlv_tag, curr_tlv_len);

        /* Validating Tag order */
        if (curr_tlv_tag != attr_struct_ptr.tag_id) {
            wmi_tlv_print_error("%s: ERROR: TLV has wrong tag in order for Cmd=0x%x. Given=%d, Expected=%d.\n",
                   __func__, wmi_cmd_event_id,curr_tlv_tag, attr_struct_ptr.tag_id);
            goto Error_wmitlv_check_and_pad_tlvs;
        }

        if ((curr_tlv_tag >= WMITLV_TAG_FIRST_ARRAY_ENUM) && (curr_tlv_tag <= WMITLV_TAG_LAST_ARRAY_ENUM))
        {
            /* Current Tag is an array of some kind. */
            /* Skip the TLV header of this array */
            buf_ptr += WMI_TLV_HDR_SIZE;
            buf_idx += WMI_TLV_HDR_SIZE;
        }
        else
        {
            /* Non-array TLV. */
            curr_tlv_len += WMI_TLV_HDR_SIZE;
        }

        if (attr_struct_ptr.tag_varied_size == WMITLV_SIZE_FIX)
        {
            /* This TLV is fixed length */
            if (WMITLV_ARR_SIZE_INVALID == attr_struct_ptr.tag_array_size)
            {
                tlv_size_diff = curr_tlv_len - attr_struct_ptr.tag_struct_size;
                num_of_elems = (curr_tlv_len>WMI_TLV_HDR_SIZE)?1:0;
            }
            else
            {
                tlv_size_diff = curr_tlv_len - (attr_struct_ptr.tag_struct_size*attr_struct_ptr.tag_array_size);
                num_of_elems = attr_struct_ptr.tag_array_size;
            }
        }
        else
        {
            /* This TLV has a variable number of elements */
            if (WMITLV_TAG_ARRAY_STRUC  == attr_struct_ptr.tag_id)
            {
                A_UINT32 in_tlv_len = 0;

                if (curr_tlv_len != 0)
                {
                    in_tlv_len = WMITLV_GET_TLVLEN(WMITLV_GET_HDR(buf_ptr));
                    in_tlv_len += WMI_TLV_HDR_SIZE;
                    tlv_size_diff = in_tlv_len - attr_struct_ptr.tag_struct_size;
                    num_of_elems = curr_tlv_len/in_tlv_len;
                    wmi_tlv_print_verbose("%s: WARN: TLV array of structures in_tlv_len=%d struct_size:%d diff:%d num_of_elems=%d \n",
                           __func__, in_tlv_len, attr_struct_ptr.tag_struct_size, tlv_size_diff, num_of_elems);
                }
                else
                {
                    tlv_size_diff = 0;
                    num_of_elems   = 0;
                }
            }
            else if ((WMITLV_TAG_ARRAY_UINT32 == attr_struct_ptr.tag_id) ||
                     (WMITLV_TAG_ARRAY_BYTE == attr_struct_ptr.tag_id) ||
                     (WMITLV_TAG_ARRAY_FIXED_STRUC  == attr_struct_ptr.tag_id))
            {
                tlv_size_diff = 0;
                num_of_elems = curr_tlv_len/attr_struct_ptr.tag_struct_size;
            }
            else
            {
                wmi_tlv_print_error("%s ERROR Need to handle this tag ID for variable length %d\n",__func__,attr_struct_ptr.tag_id);
                goto Error_wmitlv_check_and_pad_tlvs;
            }
        }

        if ((WMITLV_TAG_ARRAY_STRUC  == attr_struct_ptr.tag_id) &&
            (tlv_size_diff!=0))
        {
            void        *new_tlv_buf = NULL;
            A_UINT8     *tlv_buf_ptr = NULL;
            A_UINT32    in_tlv_len;
            A_UINT32    i;

            if (attr_struct_ptr.tag_varied_size == WMITLV_SIZE_FIX)
            {
                /* This is not allowed. The tag WMITLV_TAG_ARRAY_STRUC can only be used with variable-length structure array
                                 should not have a fixed number of elements (contradicting). Use WMITLV_TAG_ARRAY_FIXED_STRUC tag for
                                 fixed size structure array(where structure never change without breaking compatibility) */
                wmi_tlv_print_error("%s: ERROR: TLV (tag=%d) should be variable-length and not fixed length\n",
                       __func__, curr_tlv_tag);
                goto Error_wmitlv_check_and_pad_tlvs;
            }

            /* Warning: Needs to allocate a larger structure and pad with zeros */
            wmi_tlv_print_error("%s: WARN: TLV array of structures needs padding. tlv_size_diff=%d\n",
                   __func__, tlv_size_diff);

            /* incoming structure length */
            in_tlv_len = WMITLV_GET_TLVLEN(WMITLV_GET_HDR(buf_ptr)) + WMI_TLV_HDR_SIZE;
#ifndef NO_DYNAMIC_MEM_ALLOC
            wmi_tlv_os_mem_alloc(os_handle, new_tlv_buf, (num_of_elems * attr_struct_ptr.tag_struct_size));
            if (new_tlv_buf == NULL) {
                /* Error: unable to alloc memory */
                wmi_tlv_print_error("%s: Error: unable to alloc memory (size=%d) for padding the TLV array %d\n",
                       __func__, (num_of_elems * attr_struct_ptr.tag_struct_size), curr_tlv_tag);
                goto Error_wmitlv_check_and_pad_tlvs;
            }

            wmi_tlv_OS_MEMZERO(new_tlv_buf, (num_of_elems * attr_struct_ptr.tag_struct_size));
            tlv_buf_ptr = (A_UINT8 *)new_tlv_buf;
            for(i=0; i<num_of_elems; i++)
            {
                if (tlv_size_diff>0)
                {
                    /* Incoming structure size is greater than expected structure size.
                                        so copy the number of bytes equal to expected structure size */
                    wmi_tlv_OS_MEMCPY(tlv_buf_ptr, (void*)(buf_ptr+i*in_tlv_len), attr_struct_ptr.tag_struct_size);
                }
                else
                {
                    /* Incoming structure size is smaller than expected structure size.
                                        so copy the number of bytes equal to incoming structure size
                                        (other bytes would be zeroes) */
                    wmi_tlv_OS_MEMCPY(tlv_buf_ptr, (void*)(buf_ptr+i*in_tlv_len), in_tlv_len);
                }
                tlv_buf_ptr += attr_struct_ptr.tag_struct_size;
            }
#else
            {
                A_UINT8     *src_addr;
                A_UINT8     *dst_addr;
                A_UINT32    buf_mov_len;

                if (tlv_size_diff < 0)
                {
                    /* Incoming structure size is smaller than expected size then this needs padding for each element in the array */

                    /* Find amount of bytes to be padded for one element */
                    num_padding_bytes = tlv_size_diff * -1;

                    /* Move subsequent TLVs by number of bytes to be padded for all elements */
                    if (param_buf_len > (buf_idx + curr_tlv_len))
                    {
                        src_addr = buf_ptr + curr_tlv_len;
                        dst_addr = buf_ptr + curr_tlv_len + (num_padding_bytes * num_of_elems);
                        buf_mov_len  = param_buf_len - (buf_idx + curr_tlv_len);

                        wmi_tlv_OS_MEMMOVE(dst_addr, src_addr, buf_mov_len);
                    }

                    /* Move subsequent elements of array down by number of bytes to be padded for one element and alse set padding bytes to zero */
                    tlv_buf_ptr = buf_ptr;
                    for(i=0; i<num_of_elems; i++)
                    {
                        src_addr = tlv_buf_ptr + in_tlv_len;
                        if (i != (num_of_elems-1))
                        {
                            /* Need not move anything for last element in the array */
                            dst_addr = tlv_buf_ptr + in_tlv_len + num_padding_bytes;
                            buf_mov_len  = curr_tlv_len - ((i+1) * in_tlv_len);

                            wmi_tlv_OS_MEMMOVE(dst_addr, src_addr, buf_mov_len);
                        }

                        /* Set the padding bytes to zeroes */
                        wmi_tlv_OS_MEMZERO(src_addr, num_padding_bytes);

                        tlv_buf_ptr += attr_struct_ptr.tag_struct_size;
                    }

                    /* Update the number of padding bytes to total number of bytes padded for all elements in the array */
                    num_padding_bytes = num_padding_bytes * num_of_elems;

                    new_tlv_buf = buf_ptr;
                }
                else
                {
                    /* Incoming structure size is greater than expected size then this needs shrinking for each element in the array */

                    /* Find amount of bytes to be shrinked for one element */
                    num_padding_bytes = tlv_size_diff * -1;

                    /* Move subsequent elements of array up by number of bytes to be shrinked for one element */
                    tlv_buf_ptr = buf_ptr;
                    for(i=0; i<(num_of_elems-1); i++)
                    {
                        src_addr = tlv_buf_ptr + in_tlv_len;
                        dst_addr = tlv_buf_ptr + in_tlv_len + num_padding_bytes;
                        buf_mov_len  = curr_tlv_len - ((i+1) * in_tlv_len);

                        wmi_tlv_OS_MEMMOVE(dst_addr, src_addr, buf_mov_len);

                        tlv_buf_ptr += attr_struct_ptr.tag_struct_size;
                    }

                    /* Move subsequent TLVs by number of bytes to be shrinked for all elements */
                    if (param_buf_len > (buf_idx + curr_tlv_len))
                    {
                        src_addr = buf_ptr + curr_tlv_len;
                        dst_addr = buf_ptr + curr_tlv_len + (num_padding_bytes * num_of_elems);
                        buf_mov_len  = param_buf_len - (buf_idx + curr_tlv_len);

                        wmi_tlv_OS_MEMMOVE(dst_addr, src_addr, buf_mov_len);
                    }

                    /* Update the number of padding bytes to total number of bytes shrinked for all elements in the array */
                    num_padding_bytes = num_padding_bytes * num_of_elems;

                    new_tlv_buf = buf_ptr;
                }
            }
#endif
            cmd_param_tlvs_ptr[tlv_index].tlv_ptr = new_tlv_buf;
            cmd_param_tlvs_ptr[tlv_index].num_elements = num_of_elems;
            cmd_param_tlvs_ptr[tlv_index].buf_is_allocated = 1; // Indicates that buffer is allocated

        }
        else if (tlv_size_diff>=0)
        {
            /* Warning: some parameter truncation */
            if (tlv_size_diff > 0)
            {
                wmi_tlv_print_verbose("%s: WARN: TLV truncated. tlv_size_diff=%d, curr_tlv_len=%d\n",
                       __func__, tlv_size_diff, curr_tlv_len);
            }
            /* TODO: this next line needs more comments and explanation */
            cmd_param_tlvs_ptr[tlv_index].tlv_ptr = (attr_struct_ptr.tag_varied_size && !curr_tlv_len)?NULL:(void *)buf_ptr;
            cmd_param_tlvs_ptr[tlv_index].num_elements = num_of_elems;
            cmd_param_tlvs_ptr[tlv_index].buf_is_allocated = 0; // Indicates that buffer is not allocated
        }
        else
        {
            void *new_tlv_buf = NULL;

            /* Warning: Needs to allocate a larger structure and pad with zeros */
            wmi_tlv_print_verbose("%s: WARN: TLV needs padding. tlv_size_diff=%d\n",
                   __func__, tlv_size_diff);
#ifndef NO_DYNAMIC_MEM_ALLOC
            /* Dynamic memory allocation is supported */
            wmi_tlv_os_mem_alloc(os_handle, new_tlv_buf, (curr_tlv_len-tlv_size_diff));
            if (new_tlv_buf == NULL) {
                /* Error: unable to alloc memory */
                wmi_tlv_print_error("%s: Error: unable to alloc memory (size=%d) for padding the TLV %d\n",
                       __func__, (curr_tlv_len-tlv_size_diff), curr_tlv_tag);
                goto Error_wmitlv_check_and_pad_tlvs;
            }

            wmi_tlv_OS_MEMZERO(new_tlv_buf, (curr_tlv_len-tlv_size_diff));
            wmi_tlv_OS_MEMCPY(new_tlv_buf, (void*)buf_ptr, curr_tlv_len);
#else
            /* Dynamic memory allocation is not supported. Padding has to be done with in the existing buffer assuming we have enough space
                         to grow */
            {
                /* Note: tlv_size_diff is a value less than zero */
                /* Move the Subsequent TLVs by amount of bytes needs to be padded */
                A_UINT8     *src_addr;
                A_UINT8     *dst_addr;
                A_UINT32    src_len;

                num_padding_bytes = (tlv_size_diff * -1);

                src_addr = buf_ptr + curr_tlv_len;
                dst_addr = buf_ptr + curr_tlv_len + num_padding_bytes;
                src_len  = param_buf_len - (buf_idx + curr_tlv_len);

                wmi_tlv_OS_MEMMOVE(dst_addr, src_addr, src_len);

                /* Set the padding bytes to zeroes */
                wmi_tlv_OS_MEMZERO(src_addr, num_padding_bytes);

                new_tlv_buf = buf_ptr;
            }
#endif
            cmd_param_tlvs_ptr[tlv_index].tlv_ptr = new_tlv_buf;
            cmd_param_tlvs_ptr[tlv_index].num_elements = num_of_elems;
            cmd_param_tlvs_ptr[tlv_index].buf_is_allocated = 1; // Indicates that buffer is allocated
        }

        tlv_index++;
        remaining_expected_tlvs--;
        buf_ptr += curr_tlv_len + num_padding_bytes;
        buf_idx += curr_tlv_len + num_padding_bytes;
    }

    return(0);
Error_wmitlv_check_and_pad_tlvs:
    if (is_cmd_id) {
        wmitlv_free_allocated_command_tlvs(wmi_cmd_event_id, wmi_cmd_struct_ptr);
    }
    else {
        wmitlv_free_allocated_event_tlvs(wmi_cmd_event_id, wmi_cmd_struct_ptr);
    }
    *wmi_cmd_struct_ptr = NULL;
    return(-1);
}

/*
 * Helper Function to validate and pad(if necessary) for incoming WMI Event TLVs
 * Return 0 if success.
              <0 if failure.
 */
int
wmitlv_check_and_pad_event_tlvs(
    void *os_handle, void *param_struc_ptr, A_UINT32 param_buf_len, A_UINT32 wmi_cmd_event_id, void **wmi_cmd_struct_ptr)
{
    A_UINT32 is_cmd_id = 0;
    return(wmitlv_check_and_pad_tlvs(os_handle,param_struc_ptr,param_buf_len,is_cmd_id,wmi_cmd_event_id,wmi_cmd_struct_ptr));
}

/*
 * Helper Function to validate and pad(if necessary) for incoming WMI Command TLVs
 * Return 0 if success.
              <0 if failure.
 */
int
wmitlv_check_and_pad_command_tlvs(
    void *os_handle, void *param_struc_ptr, A_UINT32 param_buf_len, A_UINT32 wmi_cmd_event_id, void **wmi_cmd_struct_ptr)
{
    A_UINT32 is_cmd_id = 1;
    return(wmitlv_check_and_pad_tlvs(os_handle,param_struc_ptr,param_buf_len,is_cmd_id,wmi_cmd_event_id,wmi_cmd_struct_ptr));
}

/*
 * Helper Function to free any allocated buffers for WMI Event/Command TLV processing
 * Return None
 */
static void wmitlv_free_allocated_tlvs(A_UINT32 is_cmd_id, A_UINT32 cmd_event_id, void **wmi_cmd_struct_ptr)
{
    void *ptr = *wmi_cmd_struct_ptr;

    if(!ptr)
    {
        wmi_tlv_print_error("%s: Nothing to free for CMD/Event 0x%x\n",__func__,cmd_event_id);
        return;
    }

#ifndef NO_DYNAMIC_MEM_ALLOC

/* macro to free that previously allocated memory for this TLV. When (op==FREE_TLV_ELEM). */
#define WMITLV_OP_FREE_TLV_ELEM_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)  \
           if ((((WMITLV_TYPEDEF_STRUCT_PARAMS_TLVS(wmi_cmd_event_id)*)ptr)->WMITLV_FIELD_BUF_IS_ALLOCATED(elem_name)) && \
                (((WMITLV_TYPEDEF_STRUCT_PARAMS_TLVS(wmi_cmd_event_id)*)ptr)->elem_name != NULL)) \
           { \
               wmi_tlv_os_mem_free(((WMITLV_TYPEDEF_STRUCT_PARAMS_TLVS(wmi_cmd_event_id)*)ptr)->elem_name);\
           }


#define WMITLV_FREE_TLV_ELEMS(id)            \
        case id:\
        {\
            WMITLV_TABLE(id, FREE_TLV_ELEM, NULL, 0)     \
        } \
        break;

    if (is_cmd_id) {
        switch(cmd_event_id)
        {
            WMITLV_ALL_CMD_LIST(WMITLV_FREE_TLV_ELEMS);
            default:
                wmi_tlv_print_error("%s: ERROR: Cannot find the TLVs attributes for Cmd=0x%x, %d\n",
                              __func__, cmd_event_id, cmd_event_id);
        }
    }
    else {
        switch(cmd_event_id)
        {
            WMITLV_ALL_EVT_LIST(WMITLV_FREE_TLV_ELEMS);
            default:
                wmi_tlv_print_error("%s: ERROR: Cannot find the TLVs attributes for Cmd=0x%x, %d\n",
                             __func__, cmd_event_id, cmd_event_id);
        }
    }

    wmi_tlv_os_mem_free(*wmi_cmd_struct_ptr);
    *wmi_cmd_struct_ptr = NULL;
#endif

    return;
}

/*
 * Helper Function to free any allocated buffers for WMI Command TLV processing
 * Return None
 */
void wmitlv_free_allocated_command_tlvs(A_UINT32 cmd_event_id, void **wmi_cmd_struct_ptr)
{
    wmitlv_free_allocated_tlvs(1, cmd_event_id, wmi_cmd_struct_ptr);
}

/*
 * Helper Function to free any allocated buffers for WMI Event TLV processing
 * Return None
 */
void wmitlv_free_allocated_event_tlvs(A_UINT32 cmd_event_id, void **wmi_cmd_struct_ptr)
{
    wmitlv_free_allocated_tlvs(0, cmd_event_id, wmi_cmd_struct_ptr);
}

/*
 * Returns 1 if the two given versions are compatible.
 * Else return 0 if Incompatible.
 */
int
wmi_versions_are_compatible(wmi_abi_version *vers1, wmi_abi_version *vers2)
{
    if ((vers1->abi_version_ns_0 != vers2->abi_version_ns_0) ||
        (vers1->abi_version_ns_1 != vers2->abi_version_ns_1) ||
        (vers1->abi_version_ns_2 != vers2->abi_version_ns_2) ||
        (vers1->abi_version_ns_3 != vers2->abi_version_ns_3))
    {
        /* The namespaces are different. Incompatible. */
       return 0;
    }

    if (vers1->abi_version_0 != vers2->abi_version_0) {
        /* The major or minor versions are different. Incompatible */
        return 0;
    }
    /* We ignore the build version */
    return 1;
}

/*
 * Returns 1 if the two given versions are compatible.
 * Else return 0 if Incompatible.
 */
int
wmi_versions_can_downgrade(int num_whitelist, wmi_whitelist_version_info *version_whitelist_table,
                           wmi_abi_version *my_vers, wmi_abi_version *opp_vers, wmi_abi_version *out_vers)
{
    A_UINT8  can_try_to_downgrade;
    A_UINT32 my_major_vers = WMI_VER_GET_MAJOR(my_vers->abi_version_0);
    A_UINT32 my_minor_vers = WMI_VER_GET_MINOR(my_vers->abi_version_0);
    A_UINT32 opp_major_vers = WMI_VER_GET_MAJOR(opp_vers->abi_version_0);
    A_UINT32 opp_minor_vers = WMI_VER_GET_MINOR(opp_vers->abi_version_0);
    A_UINT32 downgraded_minor_vers;

    if ((my_vers->abi_version_ns_0 != opp_vers->abi_version_ns_0) ||
        (my_vers->abi_version_ns_1 != opp_vers->abi_version_ns_1) ||
        (my_vers->abi_version_ns_2 != opp_vers->abi_version_ns_2) ||
        (my_vers->abi_version_ns_3 != opp_vers->abi_version_ns_3))
    {
        /* The namespaces are different. Incompatible. */
        can_try_to_downgrade = FALSE;
    }
    else if (my_major_vers != opp_major_vers) {
        /* Major version is different. Incompatible and cannot downgrade. */
        can_try_to_downgrade = FALSE;
    }
    else {
        /* Same major version. */

        if (my_minor_vers < opp_minor_vers) {
            /* Opposite party is newer. Incompatible and cannot downgrade. */
            can_try_to_downgrade = FALSE;
        }
        else if (my_minor_vers > opp_minor_vers) {
            /* Opposite party is older. Check whitelist if we can downgrade */
            can_try_to_downgrade = TRUE;
        }
        else {
            /* Same version */
            wmi_tlv_OS_MEMCPY(out_vers, my_vers, sizeof(wmi_abi_version));
            return 1;
        }
    }

    if (!can_try_to_downgrade) {
        wmi_tlv_print_error("%s: Warning: incompatible WMI version.\n", __func__);
        wmi_tlv_OS_MEMCPY(out_vers, my_vers, sizeof(wmi_abi_version));
        return 0;
    }
    /* Try to see we can downgrade the supported version */
    downgraded_minor_vers = my_minor_vers;
    while (downgraded_minor_vers > opp_minor_vers)
    {
        A_UINT8 downgraded = FALSE;
        int i;

        for (i=0; i<num_whitelist; i++)
        {
            if (version_whitelist_table[i].major != my_major_vers) {
                continue;   /* skip */
            }
            if ((version_whitelist_table[i].namespace_0 != my_vers->abi_version_ns_0) ||
                (version_whitelist_table[i].namespace_1 != my_vers->abi_version_ns_1) ||
                (version_whitelist_table[i].namespace_2 != my_vers->abi_version_ns_2) ||
                (version_whitelist_table[i].namespace_3 != my_vers->abi_version_ns_3))
            {
                continue;   /* skip */
            }
            if (version_whitelist_table[i].minor == downgraded_minor_vers) {
                /* Found the next version that I can downgrade */
                wmi_tlv_print_error("%s: Note: found a whitelist entry to downgrade. wh. list ver: %d,%d,0x%x 0x%x 0x%x 0x%x\n",
                       __func__, version_whitelist_table[i].major, version_whitelist_table[i].minor,
                       version_whitelist_table[i].namespace_0, version_whitelist_table[i].namespace_1,
                       version_whitelist_table[i].namespace_2, version_whitelist_table[i].namespace_3);
                downgraded_minor_vers--;
                downgraded = TRUE;
                break;
            }
        }
        if (!downgraded) {
           break; /* Done since we did not find any whitelist to downgrade version */
        }
    }
    wmi_tlv_OS_MEMCPY(out_vers, my_vers, sizeof(wmi_abi_version));
    out_vers->abi_version_0 = WMI_VER_GET_VERSION_0(my_major_vers, downgraded_minor_vers);
    if (downgraded_minor_vers != opp_minor_vers) {
        wmi_tlv_print_error("%s: Warning: incompatible WMI version and cannot downgrade.\n", __func__);
        return 0;   /* Incompatible */
    }
    else {
        return 1;   /* Compatible */
    }
}

/*
 * This routine will compare and set the WMI ABI version.
 * First, compare my version with the opposite side's version.
 * If incompatible, then check the whitelist to see if our side can downgrade.
 * Finally, fill in the final ABI version into the output, out_vers.
 * Return 0 if the output version is compatible                              .
 * Else return 1 if the output version is incompatible.                                                                          .
 */
int
wmi_cmp_and_set_abi_version(int num_whitelist, wmi_whitelist_version_info *version_whitelist_table,
                            struct _wmi_abi_version *my_vers,
                            struct _wmi_abi_version *opp_vers,
                            struct _wmi_abi_version *out_vers)
{
    wmi_tlv_print_verbose("%s: Our WMI Version: Mj=%d, Mn=%d, bd=%d, ns0=0x%x ns1:0x%x ns2:0x%x ns3:0x%x\n", __func__,
           WMI_VER_GET_MAJOR(my_vers->abi_version_0), WMI_VER_GET_MINOR(my_vers->abi_version_0), my_vers->abi_version_1,
           my_vers->abi_version_ns_0, my_vers->abi_version_ns_1, my_vers->abi_version_ns_2, my_vers->abi_version_ns_3);

    wmi_tlv_print_verbose("%s: Opposite side WMI Version: Mj=%d, Mn=%d, bd=%d, ns0=0x%x ns1:0x%x ns2:0x%x ns3:0x%x\n", __func__,
           WMI_VER_GET_MAJOR(opp_vers->abi_version_0), WMI_VER_GET_MINOR(opp_vers->abi_version_0), opp_vers->abi_version_1,
           opp_vers->abi_version_ns_0, opp_vers->abi_version_ns_1, opp_vers->abi_version_ns_2, opp_vers->abi_version_ns_3);

    /* By default, the output version is our version. */
    wmi_tlv_OS_MEMCPY(out_vers, my_vers, sizeof(wmi_abi_version));
    if (!wmi_versions_are_compatible(my_vers, opp_vers))
    {
        /* Our host version and the given firmware version are incompatible. */
        if (wmi_versions_can_downgrade(num_whitelist, version_whitelist_table, my_vers, opp_vers, out_vers))
        {
            /* We can downgrade our host versions to match firmware. */
            wmi_tlv_print_error("%s: Host downgraded WMI Versions to match fw. Ret version: Mj=%d, Mn=%d, bd=%d, ns0=0x%x ns1:0x%x ns2:0x%x ns3:0x%x\n", __func__,
                   WMI_VER_GET_MAJOR(out_vers->abi_version_0), WMI_VER_GET_MINOR(out_vers->abi_version_0), out_vers->abi_version_1,
                   out_vers->abi_version_ns_0, out_vers->abi_version_ns_1, out_vers->abi_version_ns_2, out_vers->abi_version_ns_3);
            return 0; /* Compatible */
        }
        else {
            /* Warn: We cannot downgrade our host versions to match firmware. */
            wmi_tlv_print_error("%s: WARN: Host WMI Versions mismatch with fw. Ret version: Mj=%d, Mn=%d, bd=%d, ns0=0x%x ns1:0x%x ns2:0x%x ns3:0x%x\n", __func__,
                   WMI_VER_GET_MAJOR(out_vers->abi_version_0), WMI_VER_GET_MINOR(out_vers->abi_version_0), out_vers->abi_version_1,
                   out_vers->abi_version_ns_0, out_vers->abi_version_ns_1, out_vers->abi_version_ns_2, out_vers->abi_version_ns_3);

            return 1; /* Incompatible */
        }
    }
    else {
        /* We are compatible. Our host version is the output version */
        wmi_tlv_print_verbose("%s: Host and FW Compatible WMI Versions. Ret version: Mj=%d, Mn=%d, bd=%d, ns0=0x%x ns1:0x%x ns2:0x%x ns3:0x%x\n", __func__,
               WMI_VER_GET_MAJOR(out_vers->abi_version_0), WMI_VER_GET_MINOR(out_vers->abi_version_0), out_vers->abi_version_1,
                   out_vers->abi_version_ns_0, out_vers->abi_version_ns_1, out_vers->abi_version_ns_2, out_vers->abi_version_ns_3);
        return 0; /* Compatible */
    }
}
