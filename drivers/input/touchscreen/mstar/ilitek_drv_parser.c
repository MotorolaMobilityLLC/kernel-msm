/*
 * Copyright (C) 2006-2017 ILITEK TECHNOLOGY CORP.
 *
 * Description: ILITEK I2C touchscreen driver for linux platform.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Author: Dicky Chiang
 * Maintain: Luca Hsu, Tigers Huang
 */

#include "ilitek_drv_common.h"
#include <linux/firmware.h>

typedef struct _ST_INI_FILE_DATA {
	char pSectionName[PARSER_MAX_KEY_NAME_LEN];
	char pKeyName[PARSER_MAX_KEY_NAME_LEN];
	char pKeyValue[PARSER_MAX_KEY_VALUE_LEN];
	int iSectionNameLen;
	int iKeyNameLen;
	int iKeyValueLen;
} ST_INI_FILE_DATA;

ST_INI_FILE_DATA ms_ini_file_data[PARSER_MAX_KEY_NUM];

int ini_items = 0;
char M_CFG_SSL = '[';
char M_CFG_SSR = ']';
char M_CFG_NIS = ':';
char M_CFG_NTS = '#';
char M_CFG_EQS = '=';

static int isdigit_t(int x)
{
	if (x <= '9' && x >= '0')
		return 1;
	else
		return 0;
}

static int isspace_t(int x)
{
	if (x == ' ' || x == '\t' || x == '\n'
			|| x == '\f' || x == '\b' || x == '\r')
		return 1;
	else
		return 0;
}

static long atol_t(char *nptr)
{
	int c; /* current char */
	long total; /* current total */
	int sign; /* if ''-'', then negative, otherwise positive */

	/* skip whitespace */
	while (isspace_t((int)(unsigned char)*nptr))
		++nptr;
	c = (int)(unsigned char)*nptr++;
	sign = c; /* save sign indication */
	if (c == '-' || c == '+')
		c = (int)(unsigned char)*nptr++; /* skip sign */
	total = 0;
	while (isdigit_t(c)) {
		total = 10 * total + (c - '0'); /* accumulate digit */
		c = (int)(unsigned char)*nptr++; /* get next char */
	}
	if (sign == '-')
		return -total;
	else
		return total; /* return result, negated if necessary */
}

int ms_atoi(char *nptr)
{
	return (int)atol_t(nptr);
}

int ms_ini_get_key(char *section, char *key, char *value)
{
	int i = 0;
	int ret = -1;
	int len = 0;

	len = strlen(key);

	for (i = 0; i < ini_items; i++) {
			if (strncmp(section, ms_ini_file_data[i].pSectionName,
				 ms_ini_file_data[i].iSectionNameLen) != 0)
				 continue;

			if (strncmp(key, ms_ini_file_data[i].pKeyName, len) == 0) {
				memcpy(value, ms_ini_file_data[i].pKeyValue, ms_ini_file_data[i].iKeyValueLen);
				ret = 0;
				break;
			}
	}

	return ret;
}

int ms_get_ini_data(char *section, char *ItemName, char *returnValue)
{
	char *value = NULL;
	int len = 0;

	if (returnValue == NULL) {
		DBG(&g_i2c_client->dev, "returnValue as NULL in function\n");
		return 0;
	}

	value = kcalloc(PARSER_MAX_KEY_VALUE_LEN, sizeof(char), GFP_KERNEL);
	if (value == NULL) {
		DBG(&g_i2c_client->dev, "Failed to allocate value's mem\n");
		return 0;
	}

	if (ms_ini_get_key(section, ItemName, value) < 0)
		goto out;

	len = snprintf(returnValue, PARSER_MAX_KEY_VALUE_LEN, "%s", value);
out:
	kfree(value);
	return len;
}

int ms_ini_file_get_line(char *filedata, char *buffer, int maxlen)
{
	int i = 0;
	int j = 0;
	int iRetNum = -1;
	char ch1 = '\0';

	for (i = 0, j = 0; i < maxlen; j++) {
		ch1 = filedata[j];
		iRetNum = j + 1;
		if (ch1 == '\n' || ch1 == '\r') {
			ch1 = filedata[j + 1];
			if (ch1 == '\n' || ch1 == '\r')
				iRetNum++;
			break;
		} else if (ch1 == 0x00) {
			iRetNum = -1;
			break; /*file end*/
		} else {
			buffer[i++] = ch1;
		}
	}

	buffer[i] = '\0';
	return iRetNum;
}

static char *ms_ini_str_trim_r(char *buf)
{
	int len, i;
	char *tmp = NULL;

	len = strlen(buf);

	tmp = kcalloc(len, sizeof(char), GFP_KERNEL);

	for (i = 0; i < len; i++) {
		if (buf[i] != ' ')
			break;
	}

	if (i < len)
		strlcpy(tmp, (buf+i), len + 1);

	strlcpy(buf, tmp, len + 1);
	kfree(tmp);
	return buf;
}

static char *ms_ini_str_trim_l(char *buf)
{
	int len, i;
	char *tmp = NULL;

	len = strlen(buf);

	tmp = kcalloc(len, sizeof(char), GFP_KERNEL);

	for (i = 0; i < len; i++) {
		if (buf[len - i - 1] != ' ')
			break;
	}

	if (i < len)
		strlcpy(tmp, buf, len + 1);

	strlcpy(buf, tmp, len + 1);
	kfree(tmp);
	return buf;
}

static void ms_init_key_data(void)
{
	int i = 0;

	DBG(&g_i2c_client->dev, " %s\n", __func__);

	ini_items = 0;

	for (i = 0; i < PARSER_MAX_KEY_NUM; i++) {
		memset(ms_ini_file_data[i].pSectionName, 0, PARSER_MAX_KEY_NAME_LEN);
		memset(ms_ini_file_data[i].pKeyName, 0, PARSER_MAX_KEY_NAME_LEN);
		memset(ms_ini_file_data[i].pKeyValue, 0, PARSER_MAX_KEY_VALUE_LEN);
		ms_ini_file_data[i].iSectionNameLen = 0;
		ms_ini_file_data[i].iKeyNameLen = 0;
		ms_ini_file_data[i].iKeyValueLen = 0;
	}
}

static int ms_ini_get_key_data(char *filedata)
{
	int i, res = 0, n = 0, dataoff = 0, iEqualSign = 0;
	char *ini_buf = NULL, *tmpSectionName = NULL;

	if (filedata == NULL) {
		DBG(&g_i2c_client->dev, "INI filedata is NULL\n");
		res = -EINVAL;
		goto out;
	}

	ini_buf = kcalloc(PARSER_MAX_CFG_BUF + 1, sizeof(char), GFP_KERNEL);
	if (IS_ERR(ini_buf) || ini_buf == NULL) {
		DBG(&g_i2c_client->dev, "Failed to allocate ini_buf memory, %ld\n", PTR_ERR(ini_buf));
		res = -ENOMEM;
		goto out;
	}

	tmpSectionName = kcalloc(512, sizeof(char), GFP_KERNEL);
	if (IS_ERR(tmpSectionName) || tmpSectionName == NULL) {
		DBG(&g_i2c_client->dev, "Failed to allocate tmpSectionName memory, %ld\n", PTR_ERR(tmpSectionName));
		res = -ENOMEM;
		goto out;
	}

	while (1) {
		if (ini_items > PARSER_MAX_KEY_NUM) {
			DBG(&g_i2c_client->dev, "MAX_KEY_NUM: Out Of Length\n");
			res = -1;
			goto out;
		}

		n = ms_ini_file_get_line(filedata + dataoff, ini_buf, PARSER_MAX_CFG_BUF);

		if (n < 0) {
			DBG(&g_i2c_client->dev, "End of Line\n");
			goto out;
		}

		dataoff += n;

		if (isspace_t((int)(unsigned char)*ini_buf) || ini_buf[0] == 0)
			continue;
		n = strlen(ms_ini_str_trim_l(ms_ini_str_trim_r(ini_buf)));
		if (n == 0 || ini_buf[0] == M_CFG_NTS)
			continue;

		/* Get section names */
		if (n > 2 && ((ini_buf[0] == M_CFG_SSL && ini_buf[n - 1] != M_CFG_SSR))) {
			DBG(&g_i2c_client->dev, "Bad Section:%s\n\n", ini_buf);
			res = -EINVAL;
			goto out;
		}

		if (ini_buf[0] == M_CFG_SSL) {
			ms_ini_file_data[ini_items].iSectionNameLen = n-2;
			if (PARSER_MAX_KEY_NAME_LEN < ms_ini_file_data[ini_items].iSectionNameLen) {
				DBG(&g_i2c_client->dev, "MAX_KEY_NAME_LEN: Out Of Length\n");
				res = -1;
				goto out;
			}

			ini_buf[n-1] = 0x00;
			strlcpy((char *)tmpSectionName, ini_buf + 1, 512 + 1);
			DBG(&g_i2c_client->dev, "Section Name:%s, Len:%d\n", tmpSectionName, n-2);
			continue;
		}

		/* copy section's name without square brackets to its real buffer */
		strlcpy(ms_ini_file_data[ini_items].pSectionName, tmpSectionName, 512 + 1);
		ms_ini_file_data[ini_items].iSectionNameLen = strlen(tmpSectionName);

		iEqualSign = 0;
		for (i = 0; i < n; i++) {
			if (ini_buf[i] == M_CFG_EQS) {
				iEqualSign = i;
				break;
			}
		}

		if (0 == iEqualSign)
			continue;

		/* Get key names */
		ms_ini_file_data[ini_items].iKeyNameLen = iEqualSign;
		if (PARSER_MAX_KEY_NAME_LEN < ms_ini_file_data[ini_items].iKeyNameLen) {
				DBG(&g_i2c_client->dev, "MAX_KEY_NAME_LEN: Out Of Length\n\n");
				res = -1;
				goto out;
		}

		memcpy(ms_ini_file_data[ini_items].pKeyName,
			ini_buf, ms_ini_file_data[ini_items].iKeyNameLen);

		/* Get a value with its key */
		ms_ini_file_data[ini_items].iKeyValueLen = n - iEqualSign - 1;
		if (PARSER_MAX_KEY_VALUE_LEN < ms_ini_file_data[ini_items].iKeyValueLen) {
				DBG(&g_i2c_client->dev, "MAX_KEY_VALUE_LEN: Out Of Length\n\n");
				res = -1;
				goto out;
		}

		memcpy(ms_ini_file_data[ini_items].pKeyValue,
			ini_buf + iEqualSign + 1, ms_ini_file_data[ini_items].iKeyValueLen);

		DBG(&g_i2c_client->dev, "%s = %s\n", ms_ini_file_data[ini_items].pKeyName,
			ms_ini_file_data[ini_items].pKeyValue);

		ini_items++;
	}

out:
	if (ini_buf != NULL) {
		kfree(ini_buf);
		ini_buf = NULL;
	}
	if (tmpSectionName != NULL) {
		kfree(tmpSectionName);
		tmpSectionName = NULL;
	}
	return res;
}

int ms_ini_split_u16_array(char *key, u16 *pBuf)
{
	char *s = key;
	char *pToken;
	int nCount = 0;
	int res;
	long s_to_long = 0;

	if (isspace_t((int)(unsigned char)*s) == 0) {
		while ((pToken = strsep(&s, ",")) != NULL) {
			res = kstrtol(pToken, 0, &s_to_long);
			if (res == 0)
				pBuf[nCount] = s_to_long;
			else
				DBG(&g_i2c_client->dev, "convert string too long, res = %d\n", res);
			nCount++;
		}
	}
	return nCount;
}

int ms_ini_split_golden(int *pBuf, int line)
{
	char *pToken = NULL;
	int nCount = 0;
	int res;
	int num_count = 0;
	long s_to_long = 0;
	char szSection[100] = {0};
	char str[100] = {0};
	char *s = NULL;

	while (num_count < line) {
		snprintf(szSection, sizeof(szSection), "Golden_CH_%d", num_count);
		DBG(&g_i2c_client->dev, "szSection = %s \n", szSection);
		ms_get_ini_data("RULES", szSection, str);
		s = str;
		while ((pToken = strsep(&s, ",")) != NULL) {
			res = kstrtol(pToken, 0, &s_to_long);
			if (res == 0)
				pBuf[nCount] = s_to_long;
			else
				DBG(&g_i2c_client->dev, "convert string too long, res = %d\n", res);
			nCount++;
		}
		num_count++;
	}
	return nCount;
}

u8 *ms_ini_split_u8_array(char *key)
{
	char *s = key, *pToken;
	int i, nCount = 0;
	u8 *tmp = NULL, *data = NULL;
	long s_to_long = 0;

	tmp = kcalloc(PARSER_MAX_KEY_VALUE_LEN, sizeof(u8), GFP_KERNEL);
	if (tmp == NULL) {
		pr_err("Failed to allocate tmp's mem\n");
		return NULL;
	}

	if (isspace_t((int)(unsigned char)*s) == 0) {
		while ((pToken = strsep(&s, ",")) != NULL) {
			if (kstrtol(pToken, 0, &s_to_long) == 0)
				tmp[nCount] = s_to_long;
			else
				DBG(&g_i2c_client->dev, "convert string too long\n");
			nCount++;
		}
	}

	data = kcalloc(nCount, sizeof(u8), GFP_KERNEL);

	for (i = 0; i < nCount; i++) {
		data[i] = tmp[i];
	}

	kfree(tmp);
	return data;
}
unsigned int *ms_ini_split_int_array(char *key)
{
	char *s = key, *pToken;
	int i, nCount = 0;
	unsigned int *tmp = NULL, *data = NULL;
	long s_to_long = 0;

	tmp = kcalloc(PARSER_MAX_KEY_VALUE_LEN, sizeof(unsigned int), GFP_KERNEL);
	if (tmp == NULL) {
		pr_err("Failed to allocate tmp's mem\n");
		return NULL;
	}

	if (isspace_t((int)(unsigned char)*s) == 0) {
		while ((pToken = strsep(&s, ",")) != NULL) {
			if (kstrtol(pToken, 0, &s_to_long) == 0)
				tmp[nCount] = s_to_long;
			else
				DBG(&g_i2c_client->dev, "convert string too long\n");

			nCount++;
		}
	}

	data = kcalloc(nCount, sizeof(unsigned int), GFP_KERNEL);

	for (i = 0; i < nCount; i++) {
		data[i] = tmp[i];
	}

	kfree(tmp);
	return data;
}

int mp_parser(char *name)
{
	int res = 0, fsize = 0;
	char *tmp = NULL;
	const struct firmware *fw_entry;

	/*
	 * The default path locates under /lib/firmware/.
	 * Refer to the driver/base/firmware_base.c if wanna to change its path.
	 */
	if (request_firmware(&fw_entry, name, &g_i2c_client->dev) < 0) {
		pr_err("Failed to load ini %s\n", name);
		res = -ENOENT;
		goto out;
	}

	fsize = fw_entry->size;
	DBG(&g_i2c_client->dev, "ini size = %d\n", fsize);

	tmp = kzalloc(fsize + 1, GFP_KERNEL);
	if (IS_ERR(tmp) || tmp == NULL) {
		DBG(&g_i2c_client->dev, "Failed to allocate ini data \n");
		res = -ENOMEM;
		goto out;
	}

	memcpy(tmp, fw_entry->data, fsize);
	tmp[fsize] = '\0';

	ms_init_key_data();

	res = ms_ini_get_key_data(tmp);
	if (res < 0) {
		DBG(&g_i2c_client->dev, "Failed to get physical ini data, res = %d\n", res);
		goto out;
	}

	DBG(&g_i2c_client->dev, "Parsing INI file doen\n");

out:
	release_firmware(fw_entry);
	if (tmp != NULL) {
		kfree(tmp);
		tmp = NULL;
	}
	return res;
}
