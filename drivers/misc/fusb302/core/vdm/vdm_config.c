#ifdef FSC_HAVE_VDM

#include "vdm_config.h"

#ifdef FSC_HAVE_DP
#include "DisplayPort/dp.h"
#endif // FSC_HAVE_DP

void ConfigureVdmResponses(FSC_U8 * bytes)
{
	FSC_U16 _svid = 0;
	FSC_U32 _mode = 0;

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

void ReadVdmConfiguration(FSC_U8 * data)
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

#ifdef FSC_HAVE_DP
void configDp(FSC_BOOL enabled, FSC_U32 status)
{
	DpEnabled = enabled;
	DpStatus.word = status;
}

void configAutoDpModeEntry(FSC_BOOL enabled, FSC_U32 mask, FSC_U32 value)
{
	DpAutoModeEntryEnabled = enabled;
	DpModeEntryMask.word = mask;
	DpModeEntryValue.word = value;
}

void WriteDpControls(FSC_U8 * data)
{
	FSC_BOOL en;
	FSC_BOOL ame_en;
	FSC_U32 m, v, stat;

	m = 0;
	v = 0;
	en = FALSE;
	ame_en = FALSE;

	en = *data++ ? TRUE : FALSE;
	stat = (FSC_U32) * data++;
	stat |= ((FSC_U32) (*data++) << 8);
	stat |= ((FSC_U32) (*data++) << 16);
	stat |= ((FSC_U32) (*data++) << 24);

	ame_en = *data++ ? TRUE : FALSE;
	m = (FSC_U32) * data++;
	m |= ((FSC_U32) (*data++) << 8);
	m |= ((FSC_U32) (*data++) << 16);
	m |= ((FSC_U32) (*data++) << 24);
	v = (FSC_U32) * data++;
	v |= ((FSC_U32) (*data++) << 8);
	v |= ((FSC_U32) (*data++) << 16);
	v |= ((FSC_U32) (*data++) << 24);

	configDp(en, stat);
	configAutoDpModeEntry(ame_en, m, v);
}

void ReadDpControls(FSC_U8 * data)
{
	*data++ = (FSC_U8) DpEnabled;
	*data++ = (FSC_U8) (DpStatus.word);
	*data++ = (FSC_U8) ((DpStatus.word >> 8) & 0xFF);
	*data++ = (FSC_U8) ((DpStatus.word >> 16) & 0xFF);
	*data++ = (FSC_U8) ((DpStatus.word >> 24) & 0xFF);
	*data++ = (FSC_U8) DpAutoModeEntryEnabled;
	*data++ = (FSC_U8) (DpModeEntryMask.word);
	*data++ = (FSC_U8) ((DpModeEntryMask.word >> 8) & 0xFF);
	*data++ = (FSC_U8) ((DpModeEntryMask.word >> 16) & 0xFF);
	*data++ = (FSC_U8) ((DpModeEntryMask.word >> 24) & 0xFF);
	*data++ = (FSC_U8) (DpModeEntryValue.word);
	*data++ = (FSC_U8) ((DpModeEntryValue.word >> 8) & 0xFF);
	*data++ = (FSC_U8) ((DpModeEntryValue.word >> 16) & 0xFF);
	*data++ = (FSC_U8) ((DpModeEntryValue.word >> 24) & 0xFF);
}

void ReadDpStatus(FSC_U8 * data)
{
	*data++ = (FSC_U8) (DpConfig.word);
	*data++ = (FSC_U8) ((DpConfig.word >> 8) & 0xFF);
	*data++ = (FSC_U8) ((DpConfig.word >> 16) & 0xFF);
	*data++ = (FSC_U8) ((DpConfig.word >> 24) & 0xFF);
	*data++ = (FSC_U8) (DpPpStatus.word);
	*data++ = (FSC_U8) ((DpPpStatus.word >> 8) & 0xFF);
	*data++ = (FSC_U8) ((DpPpStatus.word >> 16) & 0xFF);
	*data++ = (FSC_U8) ((DpPpStatus.word >> 24) & 0xFF);
	*data++ = (FSC_U8) (DpPpConfig.word);
	*data++ = (FSC_U8) ((DpPpConfig.word >> 8) & 0xFF);
	*data++ = (FSC_U8) ((DpPpConfig.word >> 16) & 0xFF);
	*data++ = (FSC_U8) ((DpPpConfig.word >> 24) & 0xFF);
}
#endif // FSC_HAVE_DP

#endif // FSC_HAVE_VDM
