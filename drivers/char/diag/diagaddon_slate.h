/*
 * Copyright (C) 2010-2012 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __DIAGADDON_SLATE_H__
#define __DIAGADDON_SLATE_H__

#define DIAG_IOCTL_GET_STATE_AND_CONN_INFO  15
#define DIAG_IOCTL_GET_KEY_EVENT_MASK       16
#define DIAG_IOCTL_GET_PEN_EVENT_MASK       17

#define DIAG_MASK_CMD_SAVE                           0
#define DIAG_MASK_CMD_RESTORE                        1
#define DIAG_MASK_CMD_ADD_GET_RSSI                   2
#define DIAG_MASK_CMD_ADD_GET_STATE_AND_CONN_ATT     3
#define DIAG_MASK_CMD_ADD_GET_SEARCHER_DUMP          4

#define DIAG_LOG_CMD_TYPE_NONE                    0
#define DIAG_LOG_CMD_TYPE_GET_RSSI                1
#define DIAG_LOG_CMD_TYPE_GET_STATE_AND_CONN_ATT  2
#define DIAG_LOG_CMD_TYPE_RESTORE_LOG_MASKS       3
#define DIAG_LOG_CMD_TYPE_GET_1x_SEARCHER_DUMP    4

#define DIAG_LOG_TYPE_RSSI                        0
#define DIAG_LOG_TYPE_STATE                       1
#define DIAG_LOG_TYPE_CONN_ATT                    2

#endif
