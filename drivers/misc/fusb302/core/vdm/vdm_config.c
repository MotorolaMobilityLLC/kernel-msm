#include "vdm_config.h"

void ConfigureVdmResponses(UINT8 * bytes)
{
	UINT16 _svid = 0;
	UINT32 _mode = 0;

	if (*(bytes + 2)) {
		svid_enable = TRUE;
	} else {
		svid_enable = FALSE;
	}

	_svid |= *(bytes + 2 + 4 + 1);
	_svid <<= 8;
	_svid |= *(bytes + 2 + 4 + 0);
	my_svid = _svid;

	if (*(bytes + 2 + 8)) {
		mode_enable = TRUE;
	} else {
		mode_enable = FALSE;
	}

	_mode = _mode | *(bytes + 2 + 12 + 3);
	_mode = _mode << 8;
	_mode = _mode | *(bytes + 2 + 12 + 2);
	_mode = _mode << 8;
	_mode = _mode | *(bytes + 2 + 12 + 1);
	_mode = _mode << 8;
	_mode = _mode | *(bytes + 2 + 12 + 0);
	my_mode = _mode;

	mode_entered = FALSE;
}

void ReadVdmConfiguration(UINT8 * data)
{
	if (svid_enable) {
		*(data + 0 + 0) = 1;
	} else {
		*(data + 0 + 1) = 0;
	}
	*(data + 0 + 1) = 0;
	*(data + 0 + 2) = 0;
	*(data + 0 + 3) = 0;

	*(data + 4 + 0) = (my_svid & 0xFF);
	*(data + 4 + 1) = ((my_svid >> 8) & 0xFF);
	*(data + 4 + 2) = 0;
	*(data + 4 + 3) = 0;

	if (mode_enable) {
		*(data + 8 + 0) = 1;
	} else {
		*(data + 8 + 0) = 0;
	}
	*(data + 8 + 1) = 0;
	*(data + 8 + 2) = 0;
	*(data + 8 + 3) = 0;

	*(data + 12 + 0) = (my_mode & 0xFF);
	*(data + 12 + 1) = ((my_mode >> 8) & 0xFF);
	*(data + 12 + 2) = ((my_mode >> 16) & 0xFF);
	*(data + 12 + 3) = ((my_mode >> 24) & 0xFF);

	// add in mode entry status
	if (mode_entered) {
		*(data + 16 + 0) = 1;
	} else {
		*(data + 16 + 0) = 0;
	}
	*(data + 16 + 1) = 0;
	*(data + 16 + 2) = 0;
	*(data + 16 + 3) = 0;
}

void configDp(BOOL enabled, UINT32 status)
{
	DpEnabled = enabled;
	DpStatus.word = status;
}

void configAutoDpModeEntry(BOOL enabled, UINT32 mask, UINT32 value)
{
	DpAutoModeEntryEnabled = enabled;
	DpModeEntryMask.word = mask;
	DpModeEntryValue.word = value;
}

void WriteDpControls(UINT8 * data)
{
	BOOL en;
	BOOL ame_en;
	UINT32 m, v, stat;

	m = 0;
	v = 0;
	en = FALSE;
	ame_en = FALSE;

	en = *data++ ? TRUE : FALSE;
	stat = (UINT32) * data++;
	stat |= ((UINT32) (*data++) << 8);
	stat |= ((UINT32) (*data++) << 16);
	stat |= ((UINT32) (*data++) << 24);

	ame_en = *data++ ? TRUE : FALSE;
	m = (UINT32) * data++;
	m |= ((UINT32) (*data++) << 8);
	m |= ((UINT32) (*data++) << 16);
	m |= ((UINT32) (*data++) << 24);
	v = (UINT32) * data++;
	v |= ((UINT32) (*data++) << 8);
	v |= ((UINT32) (*data++) << 16);
	v |= ((UINT32) (*data++) << 24);

	configDp(en, stat);
	configAutoDpModeEntry(ame_en, m, v);
}

void ReadDpControls(UINT8 * data)
{
	*data++ = (UINT8) DpEnabled;
	*data++ = (UINT8) (DpStatus.word);
	*data++ = (UINT8) ((DpStatus.word >> 8) & 0xFF);
	*data++ = (UINT8) ((DpStatus.word >> 16) & 0xFF);
	*data++ = (UINT8) ((DpStatus.word >> 24) & 0xFF);
	*data++ = (UINT8) DpAutoModeEntryEnabled;
	*data++ = (UINT8) (DpModeEntryMask.word);
	*data++ = (UINT8) ((DpModeEntryMask.word >> 8) & 0xFF);
	*data++ = (UINT8) ((DpModeEntryMask.word >> 16) & 0xFF);
	*data++ = (UINT8) ((DpModeEntryMask.word >> 24) & 0xFF);
	*data++ = (UINT8) (DpModeEntryValue.word);
	*data++ = (UINT8) ((DpModeEntryValue.word >> 8) & 0xFF);
	*data++ = (UINT8) ((DpModeEntryValue.word >> 16) & 0xFF);
	*data++ = (UINT8) ((DpModeEntryValue.word >> 24) & 0xFF);
}

void ReadDpStatus(UINT8 * data)
{
	*data++ = (UINT8) (DpConfig.word);
	*data++ = (UINT8) ((DpConfig.word >> 8) & 0xFF);
	*data++ = (UINT8) ((DpConfig.word >> 16) & 0xFF);
	*data++ = (UINT8) ((DpConfig.word >> 24) & 0xFF);
	*data++ = (UINT8) (DpPpStatus.word);
	*data++ = (UINT8) ((DpPpStatus.word >> 8) & 0xFF);
	*data++ = (UINT8) ((DpPpStatus.word >> 16) & 0xFF);
	*data++ = (UINT8) ((DpPpStatus.word >> 24) & 0xFF);
	*data++ = (UINT8) (DpPpConfig.word);
	*data++ = (UINT8) ((DpPpConfig.word >> 8) & 0xFF);
	*data++ = (UINT8) ((DpPpConfig.word >> 16) & 0xFF);
	*data++ = (UINT8) ((DpPpConfig.word >> 24) & 0xFF);
}
