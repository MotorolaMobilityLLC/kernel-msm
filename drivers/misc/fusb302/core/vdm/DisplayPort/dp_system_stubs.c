#include "../../platform.h"
#include "interface_dp.h"

VOID informStatus(DisplayPortStatus_t stat)
{
	/* TODO: 'system' should implement this */
	/* this function is called to inform the 'system' of the DP status of the port partner */
	DpPpStatus.word = stat.word;
}

VOID informConfigResult(BOOL success)
{
	/* TODO: 'system' should implement this */
	/* this function is called when a config message is either ACKd or NAKd by the other side */
	if (success == TRUE)
		DpPpConfig.word = DpPpRequestedConfig.word;
}

VOID updateStatusData(VOID)
{
	/* TODO: 'system' should implement this */
	/* this function is called to get an update of our status - to be sent to the port partner */
}

BOOL DpReconfigure(DisplayPortConfig_t config)
{
	/* TODO: 'system' should implement this */
	/* called with a DisplayPort configuration to do! */
	/* return TRUE if/when successful, FALSE otherwise */
	DpConfig.word = config.word;
	/* must actually change configurations here before returning TRUE! */
	return TRUE;
}
