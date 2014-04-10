/*
// ASUS_BSP +++ Tingyi "[8226][MDSS] ASUS MDSS DEBUG UTILITY (AMDU) support."
#ifdef CONFIG_ASUS_MDSS_DEBUG_UTILITY
#include "mdss_asus_debug.h"
#endif
// ASUS_BSP --- Tingyi "[8226][MDSS] ASUS MDSS DEBUG UTILITY (AMDU) support."
*/

#include "mdss_dsi.h"

// for debug_log_flag
#define AMDU_DEBUGFAGL_LOGDSICMD 	(0x0001)
#define AMDU_DEBUGFAGL_LOGONOFFCMD  (0x0002)


// Supported commands
#define DSI_CMD_NORON "normal"
#define DSI_CMD_ALLPON "allpon"
#define SET_LOGFLAG "setlogfg:"
#define CLR_LOGFLAG "clrlogfg:"
#define DSICHECKSTATUS "status"
#define DSI_WRITE_CMD "write:"
#define DSI_READ_CMD "read:"
// ASUS_BSP +++ Tingyi "[A11][MDSS] MIPI switch support for DDS"
#ifdef CONFIG_MDSS_MIPISWITCH
#define FAKEPADIN "fakepadin"
#define FAKEPADOUT "fakepadout"
#endif
// ASUS_BSP --- Tingyi "[A11][MDSS] MIPI switch support for DDS"

// exported functions
int create_amdu_debugfs(struct dentry *parent);
unsigned int set_amdu_logflag(unsigned int new_flag);
unsigned int clr_amdu_logflag(unsigned int new_flag);
unsigned int get_amdu_logflag(void);

// integrated AMDU to MDSS
int notify_amdu_panel_on_cmds_start(struct mdss_dsi_ctrl_pdata *ctrl);
int notify_amdu_panel_on_cmds_stop(void);
int notify_amdu_dsi_cmd_dma_tx(struct dsi_buf *tp);

int is_ambient_on(void);
