#include "fusb30X.h"
#include "platform.h"

BOOL DeviceWrite(UINT8 regAddr, UINT8 length, UINT8 * data)
{
	BOOL error;
	error =
	    platform_i2c_write(FUSB300SlaveAddr, FUSB300AddrLength, length,
			       length, FUSB300IncSize, regAddr, data);
	if (error == FALSE)
		return TRUE;
	return FALSE;
}

BOOL DeviceRead(UINT8 regAddr, UINT8 length, UINT8 * data)
{
	BOOL error;
	error =
	    platform_i2c_read(FUSB300SlaveAddr, FUSB300AddrLength, length,
			      length, FUSB300IncSize, regAddr, data);
	if (error == FALSE)
		return TRUE;
	return FALSE;
}
