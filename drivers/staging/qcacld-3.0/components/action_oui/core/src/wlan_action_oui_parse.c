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

/**
 * DOC: Implement parsing logic for action_oui strings, extract
 * extensions and store them using linked list. Functions defined in
 * this file can be accessed internally in action_oui component only.
 */

#include "wlan_action_oui_main.h"
#include "wlan_action_oui_public_struct.h"
#include "wlan_action_oui_tgt_api.h"
#include "target_if_action_oui.h"
#include <qdf_str.h>
#include <wlan_utility.h>

/**
 * action_oui_string_to_hex() - convert string to uint8_t hex array
 * @token - string to be converted
 * @hex - output string to hold converted string
 * @no_of_lengths - count of possible lengths for input string
 * @possible_lengths - array holding possible lengths
 *
 * This function converts the continuous input string of even length and
 * containing hexa decimal characters into hexa decimal array of uint8_t type.
 * Input string needs to be NULL terminated and the length should match with
 * one of entries in @possible_lengths
 *
 * Return: If conversion is successful return true else false
 */
static bool action_oui_string_to_hex(uint8_t *token, uint8_t *hex,
			      uint32_t no_of_lengths,
			      uint32_t *possible_lengths)
{
	uint32_t token_len = qdf_str_len(token);
	uint32_t hex_str_len;
	uint32_t i;
	int ret;

	if (!token_len || (token_len & 0x01)) {
		action_oui_err("Token len is not multiple of 2");
		return false;
	}

	for (i = 0; i < no_of_lengths; i++)
		if (token_len == possible_lengths[i])
			break;

	if (i == no_of_lengths) {
		action_oui_err("Token len doesn't match with expected len");
		return false;
	}

	hex_str_len = token_len / 2;

	ret = qdf_hex_str_to_binary(hex, token, hex_str_len);
	if (ret) {
		action_oui_err("Token doesn't contain hex digits");
		return false;
	}

	return true;
}

/**
 * action_oui_token_string() - converts enum value to string
 * token_id: enum value to be converted to string
 *
 * This function converts the enum value of type action_oui_token_type
 * to string
 *
 * Return: converted string
 */
static
uint8_t *action_oui_token_string(enum action_oui_token_type token_id)
{
	switch (token_id) {
		CASE_RETURN_STRING(ACTION_OUI_TOKEN);
		CASE_RETURN_STRING(ACTION_OUI_DATA_LENGTH_TOKEN);
		CASE_RETURN_STRING(ACTION_OUI_DATA_TOKEN);
		CASE_RETURN_STRING(ACTION_OUI_DATA_MASK_TOKEN);
		CASE_RETURN_STRING(ACTION_OUI_INFO_MASK_TOKEN);
		CASE_RETURN_STRING(ACTION_OUI_MAC_ADDR_TOKEN);
		CASE_RETURN_STRING(ACTION_OUI_MAC_MASK_TOKEN);
		CASE_RETURN_STRING(ACTION_OUI_CAPABILITY_TOKEN);
		CASE_RETURN_STRING(ACTION_OUI_END_TOKEN);
	}

	return (uint8_t *) "UNKNOWN";
}

/**
 * validate_and_convert_oui() - validate and convert OUI str to hex array
 * @token: OUI string
 * @ext: pointer to container which holds converted hex array
 * @action_token: next action to be parsed
 *
 * This is an internal function invoked from action_oui_parse to validate
 * the OUI string for action OUI inis, convert them to hex array and store it
 * in action_oui extension. After successful parsing update the
 * @action_token to hold the next expected string.
 *
 * Return: If conversion is successful return true else false
 */
static
bool validate_and_convert_oui(uint8_t *token,
			struct action_oui_extension *ext,
			enum action_oui_token_type *action_token)
{
	bool valid;
	uint32_t expected_token_len[2] = {6, 10};

	valid = action_oui_string_to_hex(token, ext->oui, 2,
					 expected_token_len);
	if (!valid)
		return false;

	ext->oui_length = qdf_str_len(token) / 2;

	*action_token = ACTION_OUI_DATA_LENGTH_TOKEN;

	return valid;
}

/**
 * validate_and_convert_data_length() - validate data len str
 * @token: data length string
 * @ext: pointer to container which holds hex value formed from input str
 * @action_token: next action to be parsed
 *
 * This is an internal function invoked from action_oui_parse to validate
 * the data length string for action OUI inis, convert it to hex value and
 * store it in action_oui extension. After successful parsing update the
 * @action_token to hold the next expected string.
 *
 * Return: If conversion is successful return true else false
 */
static bool
validate_and_convert_data_length(uint8_t *token,
				struct action_oui_extension *ext,
				enum action_oui_token_type *action_token)
{
	uint32_t token_len = qdf_str_len(token);
	int ret;
	uint8_t len = 0;

	if (token_len != 1 && token_len != 2) {
		action_oui_err("Invalid str token len for action OUI data len");
		return false;
	}

	ret = kstrtou8(token, 16, &len);
	if (ret) {
		action_oui_err("Invalid char in action OUI data len str token");
		return false;
	}

	if ((uint32_t)len > ACTION_OUI_MAX_DATA_LENGTH) {
		action_oui_err("action OUI data len is more than %u",
			ACTION_OUI_MAX_DATA_LENGTH);
		return false;
	}

	ext->data_length = len;

	if (!ext->data_length)
		*action_token = ACTION_OUI_INFO_MASK_TOKEN;
	else
		*action_token = ACTION_OUI_DATA_TOKEN;

	return true;
}

/**
 * validate_and_convert_data() - validate and convert data str to hex array
 * @token: data string
 * @ext: pointer to container which holds converted hex array
 * @action_token: next action to be parsed
 *
 * This is an internal function invoked from action_oui_parse to validate
 * the data string for action OUI inis, convert it to hex array and store in
 * action_oui extension. After successful parsing update the @action_token
 * to hold the next expected string.
 *
 * Return: If conversion is successful return true else false
 */
static bool
validate_and_convert_data(uint8_t *token,
			      struct action_oui_extension *ext,
			      enum action_oui_token_type *action_token)
{
	bool valid;
	uint32_t expected_token_len[1] = {2 * ext->data_length};

	valid = action_oui_string_to_hex(token, ext->data, 1,
					 expected_token_len);
	if (!valid)
		return false;

	*action_token = ACTION_OUI_DATA_MASK_TOKEN;

	return true;
}

/**
 * validate_and_convert_data_mask() - validate and convert data mask str
 * @token: data mask string
 * @ext: pointer to container which holds converted hex array
 * @action_token: next action to be parsed
 *
 * This is an internal function invoked from action_oui_parse to validate
 * the data mask string for action OUI inis, convert it to hex array and store
 * in action_oui extension. After successful parsing update the
 * @action_token to hold the next expected string.
 *
 * Return: If conversion is successful return true else false
 */
static bool
validate_and_convert_data_mask(uint8_t *token,
				   struct action_oui_extension *ext,
				   enum action_oui_token_type *action_token)
{
	bool valid;
	uint32_t expected_token_len[1];
	uint32_t data_mask_length;
	uint32_t data_length = ext->data_length;

	if (data_length % 8 == 0)
		data_mask_length = data_length / 8;
	else
		data_mask_length = ((data_length / 8) + 1);

	if (data_mask_length > ACTION_OUI_MAX_DATA_MASK_LENGTH)
		return false;

	expected_token_len[0] = 2 * data_mask_length;

	valid = action_oui_string_to_hex(token, ext->data_mask, 1,
				  expected_token_len);
	if (!valid)
		return false;

	ext->data_mask_length = data_mask_length;

	*action_token = ACTION_OUI_INFO_MASK_TOKEN;

	return valid;
}

/**
 * validate_and_convert_info_mask() - validate and convert info mask str
 * @token: info mask string
 * @ext: pointer to container which holds converted hex array
 * @action_token: next action to be parsed
 *
 * This is an internal function invoked from action_oui_parse to validate
 * the info mask string for action OUI inis, convert it to hex array and store
 * in action_oui extension. After successful parsing update the
 * @action_token to hold the next expected string.
 *
 * Return: If conversion is successful return true else false
 */
static bool
validate_and_convert_info_mask(uint8_t *token,
				   struct action_oui_extension *ext,
				   enum action_oui_token_type *action_token)
{
	uint32_t token_len = qdf_str_len(token);
	uint8_t hex_value = 0;
	uint32_t info_mask;
	int ret;

	if (token_len != 2) {
		action_oui_err("action OUI info mask str token len is not of 2 chars");
		return false;
	}

	ret = kstrtou8(token, 16, &hex_value);
	if (ret) {
		action_oui_err("Invalid char in action OUI info mask str token");
		return false;
	}

	info_mask = hex_value;

	ext->info_mask = info_mask;

	if (!info_mask || !(info_mask & ~ACTION_OUI_INFO_OUI)) {
		*action_token = ACTION_OUI_END_TOKEN;
		return true;
	}

	if (info_mask & ~ACTION_OUI_INFO_MASK) {
		action_oui_err("Invalid bits are set in action OUI info mask");
		return false;
	}

	/*
	 * If OUI bit is not set in the info presence, we need to ignore the
	 * OUI and OUI Data. Set OUI and OUI data length to 0 here.
	 */
	if (!(info_mask & ACTION_OUI_INFO_OUI)) {
		ext->oui_length = 0;
		ext->data_length = 0;
		ext->data_mask_length = 0;
	}

	if (info_mask & ACTION_OUI_INFO_MAC_ADDRESS) {
		*action_token = ACTION_OUI_MAC_ADDR_TOKEN;
		return true;
	}

	*action_token = ACTION_OUI_CAPABILITY_TOKEN;
	return true;
}

/**
 * validate_and_convert_mac_addr() - validate and convert mac addr str
 * @token: mac address string
 * @ext: pointer to container which holds converted hex array
 * @action_token: next action to be parsed
 *
 * This is an internal function invoked from action_oui_parse to validate
 * the mac address string for action OUI inis, convert it to hex array and store
 * in action_oui extension. After successful parsing update the
 * @action_token to hold the next expected string.
 *
 * Return: If conversion is successful return true else false
 */
static bool
validate_and_convert_mac_addr(uint8_t *token,
				  struct action_oui_extension *ext,
				  enum action_oui_token_type *action_token)
{
	uint32_t expected_token_len[1] = {2 * QDF_MAC_ADDR_SIZE};
	bool valid;

	valid = action_oui_string_to_hex(token, ext->mac_addr, 1,
				  expected_token_len);
	if (!valid)
		return false;

	ext->mac_addr_length = QDF_MAC_ADDR_SIZE;

	*action_token = ACTION_OUI_MAC_MASK_TOKEN;

	return true;
}

/**
 * validate_and_convert_mac_mask() - validate and convert mac mask
 * @token: mac mask string
 * @ext: pointer to container which holds converted hex value
 * @action_token: next action to be parsed
 *
 * This is an internal function invoked from action_oui_parse to validate
 * the mac mask string for action OUI inis, convert it to hex value and store
 * in action_oui extension. After successful parsing update the
 * @action_token to hold the next expected string.
 *
 * Return: If conversion is successful return true else false
 */
static bool
validate_and_convert_mac_mask(uint8_t *token,
				  struct action_oui_extension *ext,
				  enum action_oui_token_type *action_token)
{
	uint32_t expected_token_len[1] = {2};
	uint32_t info_mask = ext->info_mask;
	bool valid;
	uint32_t mac_mask_length;

	valid = action_oui_string_to_hex(token, ext->mac_mask, 1,
				  expected_token_len);
	if (!valid)
		return false;

	mac_mask_length = qdf_str_len(token) / 2;
	if (mac_mask_length > ACTION_OUI_MAC_MASK_LENGTH) {
		action_oui_err("action OUI mac mask str token len is more than %u chars",
			expected_token_len[0]);
		return false;
	}

	ext->mac_mask_length = mac_mask_length;

	if ((info_mask & ACTION_OUI_INFO_AP_CAPABILITY_NSS) ||
	    (info_mask & ACTION_OUI_INFO_AP_CAPABILITY_HT) ||
	    (info_mask & ACTION_OUI_INFO_AP_CAPABILITY_VHT) ||
	    (info_mask & ACTION_OUI_INFO_AP_CAPABILITY_BAND)) {
		*action_token = ACTION_OUI_CAPABILITY_TOKEN;
		return true;
	}

	*action_token = ACTION_OUI_END_TOKEN;
	return true;
}

/**
 * validate_and_convert_capability() - validate and convert capability str
 * @token: capability string
 * @ext: pointer to container which holds converted hex value
 * @action_token: next action to be parsed
 *
 * This is an internal function invoked from action_oui_parse to validate
 * the capability string for action OUI inis, convert it to hex value and store
 * in action_oui extension. After successful parsing update the
 * @action_token to hold the next expected string.
 *
 * Return: If conversion is successful return true else false
 */
static bool
validate_and_convert_capability(uint8_t *token,
				struct action_oui_extension *ext,
				enum action_oui_token_type *action_token)
{
	uint32_t expected_token_len[1] = {2};
	uint32_t info_mask = ext->info_mask;
	uint32_t capability_length;
	uint8_t caps_0;
	bool valid;

	valid = action_oui_string_to_hex(token, ext->capability, 1,
				  expected_token_len);
	if (!valid)
		return false;

	capability_length = qdf_str_len(token) / 2;
	if (capability_length > ACTION_OUI_MAX_CAPABILITY_LENGTH) {
		action_oui_err("action OUI capability str token len is more than %u chars",
			expected_token_len[0]);
		return false;
	}

	caps_0 = ext->capability[0];

	if ((info_mask & ACTION_OUI_INFO_AP_CAPABILITY_NSS) &&
	    (!(caps_0 & ACTION_OUI_CAPABILITY_NSS_MASK))) {
		action_oui_err("Info presence for NSS is set but respective bits in capability are not set");
		return false;
	}

	if ((info_mask & ACTION_OUI_INFO_AP_CAPABILITY_BAND) &&
	    (!(caps_0 & ACTION_OUI_CAPABILITY_BAND_MASK))) {
		action_oui_err("Info presence for BAND is set but respective bits in capability are not set");
		return false;
	}

	ext->capability_length = capability_length;

	*action_token = ACTION_OUI_END_TOKEN;

	return true;
}

/**
 * action_oui_extension_store() - store action oui extension
 * @priv_obj: pointer to action_oui priv obj
 * @oui_priv: type of the action
 * @ext: oui extension to store in sme
 *
 * This function stores the parsed oui extension
 *
 * Return: QDF_STATUS
 *
 */
static QDF_STATUS
action_oui_extension_store(struct action_oui_psoc_priv *psoc_priv,
			   struct action_oui_priv *oui_priv,
			   struct action_oui_extension ext)
{
	struct action_oui_extension_priv *ext_priv;

	qdf_mutex_acquire(&oui_priv->extension_lock);
	if (qdf_list_size(&oui_priv->extension_list) ==
			  ACTION_OUI_MAX_EXTENSIONS) {
		qdf_mutex_release(&oui_priv->extension_lock);
		action_oui_err("Reached maximum OUI extensions");
		return QDF_STATUS_E_FAILURE;
	}

	ext_priv = qdf_mem_malloc(sizeof(*ext_priv));
	if (!ext_priv) {
		qdf_mutex_release(&oui_priv->extension_lock);
		return QDF_STATUS_E_NOMEM;
	}

	ext_priv->extension = ext;
	qdf_list_insert_back(&oui_priv->extension_list, &ext_priv->item);
	psoc_priv->total_extensions++;
	qdf_mutex_release(&oui_priv->extension_lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
action_oui_parse(struct action_oui_psoc_priv *psoc_priv,
		 uint8_t *oui_string, enum action_oui_id action_id)
{
	struct action_oui_extension ext = {0};
	enum action_oui_token_type action_token = ACTION_OUI_TOKEN;
	char *str1;
	char *str2;
	char *token;
	bool valid = true;
	bool oui_count_exceed = false;
	uint32_t oui_index = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct action_oui_priv *oui_priv;

	if (!oui_string) {
		action_oui_err("Invalid string for action oui: %u", action_id);
		return QDF_STATUS_E_INVAL;
	}

	oui_priv = psoc_priv->oui_priv[action_id];
	if (!oui_priv) {
		action_oui_err("action oui priv not allocated");
		return QDF_STATUS_E_INVAL;
	}

	str1 = qdf_str_trim((char *)oui_string);

	while (str1) {
		str2 = (char *)qdf_str_left_trim(str1);
		if (str2[0] == '\0') {
			action_oui_err("Invalid spaces in action oui: %u at extension: %u for token: %s",
				action_id,
				oui_index + 1,
				action_oui_token_string(action_token));
			valid = false;
			break;
		}

		token = strsep(&str2, " ");
		if (!token) {
			action_oui_err("Invalid string for token: %s at extension: %u in action oui: %u",
				action_oui_token_string(action_token),
				oui_index + 1, action_id);
			valid = false;
			break;
		}

		str1 = str2;

		switch (action_token) {

		case ACTION_OUI_TOKEN:
			valid = validate_and_convert_oui(token, &ext,
							 &action_token);
			break;

		case ACTION_OUI_DATA_LENGTH_TOKEN:
			valid = validate_and_convert_data_length(token, &ext,
								&action_token);
			break;

		case ACTION_OUI_DATA_TOKEN:
			valid = validate_and_convert_data(token, &ext,
							&action_token);
			break;

		case ACTION_OUI_DATA_MASK_TOKEN:
			valid = validate_and_convert_data_mask(token, &ext,
							&action_token);
			break;

		case ACTION_OUI_INFO_MASK_TOKEN:
			valid = validate_and_convert_info_mask(token, &ext,
							&action_token);
			break;

		case ACTION_OUI_MAC_ADDR_TOKEN:
			valid = validate_and_convert_mac_addr(token, &ext,
							&action_token);
			break;

		case ACTION_OUI_MAC_MASK_TOKEN:
			valid = validate_and_convert_mac_mask(token, &ext,
							&action_token);
			break;

		case ACTION_OUI_CAPABILITY_TOKEN:
			valid = validate_and_convert_capability(token, &ext,
							&action_token);
			break;

		default:
			valid = false;
			break;
		}

		if (!valid) {
			action_oui_err("Invalid string for token: %s at extension: %u in action oui: %u",
				action_oui_token_string(action_token),
				oui_index + 1,
				action_id);
			break;
		}

		if (action_token != ACTION_OUI_END_TOKEN)
			continue;

		status = action_oui_extension_store(psoc_priv, oui_priv, ext);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			valid = false;
			action_oui_err("sme set of extension: %u for action oui: %u failed",
				oui_index + 1, action_id);
			break;
		}

		oui_index++;
		if (oui_index == ACTION_OUI_MAX_EXTENSIONS) {
			if (str1)
				oui_count_exceed = true;
			break;
		}

		/* reset the params for next action OUI parse */
		action_token = ACTION_OUI_TOKEN;
		qdf_mem_zero(&ext, sizeof(ext));
	}

	if (oui_count_exceed) {
		action_oui_err("Reached Maximum extensions: %u in action_oui: %u, ignoring the rest",
			ACTION_OUI_MAX_EXTENSIONS, action_id);
		return QDF_STATUS_SUCCESS;
	}

	if (action_token != ACTION_OUI_TOKEN &&
	    action_token != ACTION_OUI_END_TOKEN &&
	    valid && !str1) {
		action_oui_err("No string for token: %s at extension: %u in action oui: %u",
			action_oui_token_string(action_token),
			oui_index + 1,
			action_id);
		valid = false;
	}

	if (!oui_index) {
		action_oui_err("Not able to parse any extension in action oui: %u",
			action_id);
		return QDF_STATUS_E_INVAL;
	}

	if (valid)
		action_oui_debug("All extensions: %u parsed successfully in action oui: %u",
			  oui_index, action_id);
	else
		action_oui_err("First %u extensions parsed successfully in action oui: %u",
			oui_index, action_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS action_oui_send(struct action_oui_psoc_priv *psoc_priv,
			enum action_oui_id action_id)
{
	QDF_STATUS status;
	struct action_oui_request *req;
	struct action_oui_priv *oui_priv;
	struct action_oui_extension *extension;
	struct action_oui_extension_priv *ext_priv;
	qdf_list_node_t *node = NULL;
	qdf_list_node_t *next_node = NULL;
	qdf_list_t *extension_list;
	uint32_t len;
	uint32_t no_oui_extensions;

	oui_priv = psoc_priv->oui_priv[action_id];
	if (!oui_priv)
		return QDF_STATUS_SUCCESS;

	extension_list = &oui_priv->extension_list;
	qdf_mutex_acquire(&oui_priv->extension_lock);
	if (qdf_list_empty(extension_list)) {
		qdf_mutex_release(&oui_priv->extension_lock);
		return QDF_STATUS_SUCCESS;
	}

	no_oui_extensions = qdf_list_size(extension_list);
	len = sizeof(*req) + no_oui_extensions * sizeof(*extension);
	req = qdf_mem_malloc(len);
	if (!req) {
		qdf_mutex_release(&oui_priv->extension_lock);
		return QDF_STATUS_E_NOMEM;
	}

	req->action_id = oui_priv->id;
	req->no_oui_extensions = no_oui_extensions;
	req->total_no_oui_extensions = psoc_priv->total_extensions;

	extension = req->extension;
	qdf_list_peek_front(extension_list, &node);
	while (node) {
		ext_priv = qdf_container_of(node,
					   struct action_oui_extension_priv,
					   item);
		*extension = ext_priv->extension;
		status = qdf_list_peek_next(extension_list, node,
						&next_node);
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;
		node = next_node;
		next_node = NULL;
		extension++;
	}

	qdf_mutex_release(&oui_priv->extension_lock);

	status = tgt_action_oui_send(psoc_priv->psoc, req);
	qdf_mem_free(req);

	return status;
}

/**
 * check_for_vendor_oui_data() - compares for vendor OUI data from IE
 * and returns true if OUI data matches with the ini
 * @extension: pointer to action oui extension data
 * @oui_ptr: pointer to Vendor IE in the beacon
 *
 * Return: true or false
 */
static bool
check_for_vendor_oui_data(struct action_oui_extension *extension,
			  const uint8_t *oui_ptr)
{
	const uint8_t *data;
	uint8_t i, j, elem_len, data_len;
	uint8_t data_mask = 0x80;

	if (!oui_ptr)
		return false;

	elem_len = oui_ptr[1];
	if (elem_len < extension->oui_length)
		return false;

	data_len = elem_len - extension->oui_length;
	if (data_len < extension->data_length)
		return false;

	data = &oui_ptr[2 + extension->oui_length];
	for (i = 0, j = 0;
	     (i < data_len && j < extension->data_mask_length);
	     i++) {
		if ((extension->data_mask[j] & data_mask) &&
		    !(extension->data[i] == data[i]))
			return false;
		data_mask = data_mask >> 1;
		if (!data_mask) {
			data_mask = 0x80;
			j++;
		}
	}

	return true;
}

/**
 * check_for_vendor_ap_mac() - compares for vendor AP MAC in the ini with
 * bssid from the session and returns true if matches
 * @extension: pointer to action oui extension data
 * @attr: pointer to structure containing mac_addr (bssid) of AP
 *
 * Return: true or false
 */
static bool
check_for_vendor_ap_mac(struct action_oui_extension *extension,
			struct action_oui_search_attr *attr)
{
	uint8_t i;
	uint8_t mac_mask = 0x80;
	uint8_t *mac_addr = attr->mac_addr;

	for (i = 0; i < QDF_MAC_ADDR_SIZE; i++) {
		if ((*extension->mac_mask & mac_mask) &&
		    !(extension->mac_addr[i] == mac_addr[i]))
			return false;
		mac_mask = mac_mask >> 1;
	}

	return true;
}

/**
 * check_for_vendor_ap_capabilities() - Compares various Vendor AP
 * capabilities like NSS, HT, VHT, Band from the ini with the AP's capability
 * from the beacon and returns true if all the capability matches
 * @extension: pointer to oui extension data
 * @attr: pointer to structure containing type of action, ap capabilities
 *
 * Return: true or false
 */
static bool
check_for_vendor_ap_capabilities(struct action_oui_extension *extension,
				 struct action_oui_search_attr *attr)
{
	uint8_t nss_mask;

	if (extension->info_mask & ACTION_OUI_INFO_AP_CAPABILITY_NSS) {
		nss_mask = 1 << (attr->nss - 1);
		if (!((*extension->capability &
		    ACTION_OUI_CAPABILITY_NSS_MASK) &
		    nss_mask))
		return false;
	}

	if (extension->info_mask & ACTION_OUI_INFO_AP_CAPABILITY_HT) {
		if (*extension->capability &
		    ACTION_OUI_CAPABILITY_HT_ENABLE_MASK) {
			if (!attr->ht_cap)
				return false;
		} else {
			if (attr->ht_cap)
				return false;
		}
	}

	if (extension->info_mask & ACTION_OUI_INFO_AP_CAPABILITY_VHT) {
		if (*extension->capability &
		    ACTION_OUI_CAPABILITY_VHT_ENABLE_MASK) {
			if (!attr->vht_cap)
				return false;
		} else {
			if (attr->vht_cap)
				return false;
		}
	}

	if (extension->info_mask & ACTION_OUI_INFO_AP_CAPABILITY_BAND &&
	    ((attr->enable_5g &&
	    !(*extension->capability & ACTION_CAPABILITY_5G_BAND_MASK)) ||
	    (attr->enable_2g &&
	    !(*extension->capability & ACTION_OUI_CAPABILITY_2G_BAND_MASK))))
		return false;

	return true;
}

static const uint8_t *
action_oui_get_oui_ptr(struct action_oui_extension *extension,
		       struct action_oui_search_attr *attr)
{
	if (!attr->ie_data || !attr->ie_length)
		return NULL;

	return wlan_get_vendor_ie_ptr_from_oui(extension->oui,
					       extension->oui_length,
					       attr->ie_data,
					       attr->ie_length);
}

bool
action_oui_search(struct action_oui_psoc_priv *psoc_priv,
		  struct action_oui_search_attr *attr,
		  enum action_oui_id action_id)
{
	struct action_oui_priv *oui_priv;
	struct action_oui_extension_priv *priv_ext;
	struct action_oui_extension *extension;
	qdf_list_node_t *node = NULL;
	qdf_list_node_t *next_node = NULL;
	qdf_list_t *extension_list;
	QDF_STATUS qdf_status;
	const uint8_t *oui_ptr;
	bool wildcard_oui = false;

	oui_priv = psoc_priv->oui_priv[action_id];
	if (!oui_priv) {
		action_oui_debug("action oui for id %d is empty",
				 action_id);
		return false;
	}

	extension_list = &oui_priv->extension_list;
	qdf_mutex_acquire(&oui_priv->extension_lock);
	if (qdf_list_empty(extension_list)) {
		qdf_mutex_release(&oui_priv->extension_lock);
		return false;
	}

	qdf_list_peek_front(extension_list, &node);
	while (node) {
		priv_ext = qdf_container_of(node,
					   struct action_oui_extension_priv,
					   item);
		extension = &priv_ext->extension;

		/*
		 * If a wildcard OUI bit is not set in the info_mask, proceed
		 * to other checks skipping the OUI and vendor data checks
		 */

		if (!(extension->info_mask & ACTION_OUI_INFO_OUI))
			wildcard_oui = true;

		oui_ptr = action_oui_get_oui_ptr(extension, attr);

		if (!oui_ptr  && !wildcard_oui)
			goto next;

		if (extension->data_length && !wildcard_oui &&
		    !check_for_vendor_oui_data(extension, oui_ptr))
			goto next;


		if ((extension->info_mask & ACTION_OUI_INFO_MAC_ADDRESS) &&
		    !check_for_vendor_ap_mac(extension, attr))
			goto next;

		if (!check_for_vendor_ap_capabilities(extension, attr))
			goto next;

		action_oui_debug("Vendor AP/STA found for OUI");
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
				   extension->oui, extension->oui_length);
		qdf_mutex_release(&oui_priv->extension_lock);
		return true;
next:
		qdf_status = qdf_list_peek_next(extension_list,
						node, &next_node);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			break;

		node = next_node;
		next_node = NULL;
		wildcard_oui = false;
	}

	qdf_mutex_release(&oui_priv->extension_lock);
	return false;
}
