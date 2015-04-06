/*
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
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

#ifndef _WMI_TLV_HELPER_H_
#define _WMI_TLV_HELPER_H_

/*
 * Every command or event parameter structure will need a TLV definition.
 * The macro WMITLV_TABLE is used to help build this TLV definition. Inside this macro define, the
 * individual TLV's are specified. The parameters for WMITLV_ELEM are:
 * (1) the list of parameters that are passed unchanged from the WMITLV_TABLE. Currently, they are id,op,buf,len
 * (2) The TLV Tag. You should create a new tag for each cmd/event in WMITLV_TAG_ID. The name of the
 *     tag is <WMI_TLVTAG_STRUC_><CMD or Event ID name>. There are special tags,
 *     e.g. WMI_TLVTAG_ARRAY_UINT32 and WMI_TLVTAG_ARRAY_STRUC. WMI_TLVTAG_ARRAY_UINT32 is for a
 *     variable size array of UINT32 elements. WMI_TLVTAG_ARRAY_STRUC is for a varialbe size array
 *     of structures.
 * (3) type of the TLV. For WMI_TLVTAG_ARRAY_* tag, then it is the type of each element.
 * (4) Name of this TLV. It must be unique in this TLV TABLE.
 * (5) Either WMITLV_SIZE_FIX or WMITLV_SIZE_VAR to indicate if this TLV is variable size.
 *
 * Note: It is important that the last TLV_ELEM does not have the "\" character.
*/

/* Size of the TLV Header which is the Tag and Length fields */
#define WMI_TLV_HDR_SIZE   (1 * sizeof(A_UINT32))

/** TLV Helper macro to get the TLV Header given the pointer
 *  to the TLV buffer. */
#define WMITLV_GET_HDR(tlv_buf)        (((A_UINT32 *)(tlv_buf))[0])

/** TLV Helper macro to set the TLV Header given the pointer
 *  to the TLV buffer. */
#define WMITLV_SET_HDR(tlv_buf, tag, len) (((A_UINT32 *)(tlv_buf))[0]) = ((tag << 16) | (len & 0x0000FFFF))

/** TLV Helper macro to get the TLV Tag given the TLV header. */
#define WMITLV_GET_TLVTAG(tlv_header)  ((A_UINT32)((tlv_header)>>16))

/** TLV Helper macro to get the TLV Buffer Length (minus TLV
 *  header size) given the TLV header. */
#define WMITLV_GET_TLVLEN(tlv_header)  ((A_UINT32)((tlv_header) & 0x0000FFFF))

/** TLV Helper macro to get the TLV length from TLV structure size by removing TLV header size */
#define WMITLV_GET_STRUCT_TLVLEN(tlv_struct)    ((A_UINT32)(sizeof(tlv_struct)-WMI_TLV_HDR_SIZE))

/* Indicates whether the TLV is fixed size or variable length */
#define WMITLV_SIZE_FIX     0
#define WMITLV_SIZE_VAR     1

typedef struct {
	A_UINT32 tag_order;
	A_UINT32 tag_id;
	A_UINT32 tag_struct_size;
	A_UINT32 tag_varied_size;
	A_UINT32 tag_array_size;
    A_UINT32 cmd_num_tlv;
} wmitlv_attributes_struc;


/* Template structure definition for a variable size array of UINT32 */
typedef struct {
    A_UINT32    tlv_header;     /* TLV tag and len; tag equals WMI_TLVTAG_ARRAY_UINT32 */
    A_UINT32    uint32_array[1]; /* variable length Array of UINT32 */
} wmitlv_array_uint32;

/* Template structure definition for a variable size array of unknown structure */
typedef struct {
    A_UINT32    tlv_header;     /* TLV tag and len; tag equals WMI_TLVTAG_ARRAY_STRUC */
    A_UINT32    struc_array[1]; /* variable length Array of structures */
} wmitlv_array_struc;

/*
 * Used to fill in the "arr_size" parameter when it is not specified and hence, invalid. Can be used to
 * indicate if the original TLV definition specify this fixed array size.
 */
#define WMITLV_ARR_SIZE_INVALID  0x1FE

#define WMITLV_GET_TAG_NUM_TLV_ATTRIB(wmi_cmd_event_id)      \
       WMI_TLV_HLPR_NUM_TLVS_FOR_##wmi_cmd_event_id


void
wmitlv_set_static_param_tlv_buf(void *param_tlv_buf, A_UINT32 max_tlvs_accomodated);

void
wmitlv_free_allocated_command_tlvs(
    A_UINT32 cmd_id,
    void **wmi_cmd_struct_ptr);

void
wmitlv_free_allocated_event_tlvs(
    A_UINT32 event_id,
    void **wmi_cmd_struct_ptr);

int
wmitlv_check_command_tlv_params(
    void *os_ctx, void *param_struc_ptr, A_UINT32 param_buf_len, A_UINT32 wmi_cmd_event_id);

int
wmitlv_check_event_tlv_params(
    void *os_ctx, void *param_struc_ptr, A_UINT32 param_buf_len, A_UINT32 wmi_cmd_event_id);

int
wmitlv_check_and_pad_command_tlvs(
    void *os_ctx, void *param_struc_ptr, A_UINT32 param_buf_len, A_UINT32 wmi_cmd_event_id, void **wmi_cmd_struct_ptr);

int
wmitlv_check_and_pad_event_tlvs(
    void *os_ctx, void *param_struc_ptr, A_UINT32 param_buf_len, A_UINT32 wmi_cmd_event_id, void **wmi_cmd_struct_ptr);

/** This structure is the element for the Version WhiteList
 *  table. */
typedef struct {
    A_UINT32      major;
    A_UINT32      minor;
    A_UINT32      namespace_0;
    A_UINT32      namespace_1;
    A_UINT32      namespace_2;
    A_UINT32      namespace_3;
} wmi_whitelist_version_info;

struct _wmi_abi_version;   /* Forward declaration to make the ARM compiler happy */

int
wmi_cmp_and_set_abi_version(int num_whitelist, wmi_whitelist_version_info *version_whitelist_table,
                            struct _wmi_abi_version *my_vers,
                            struct _wmi_abi_version *opp_vers,
                            struct _wmi_abi_version *out_vers);

int
wmi_versions_are_compatible(struct _wmi_abi_version *vers1, struct _wmi_abi_version *vers2);

#endif /*_WMI_TLV_HELPER_H_*/
