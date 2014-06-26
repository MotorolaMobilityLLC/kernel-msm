/*
 *  sep_applets.h - Security Processor applet definitions
 *
 *  Copyright(c) 2012-2013 Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program; if not, write to the Free Software Foundation, Inc., 59
 *  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef _SEP_APPLETS_H
#define _SEP_APPLETS_H

/* Kernel side threads (Agents as DX calls them) */
#define RPMB_AGENT_ID            0

/* Applet UUIDs */
#define DEFAULT_APP_UUID { 0x00, 0xDE, 0xFA, 0x01, 0xDE, 0xFA, 0x02, 0xDE, \
	0xFA, 0x03, 0xDE, 0xFA, 0x04, 0xDE, 0xFA, 0xFF }

#define HDCP_APP_UUID { 0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87,     \
	0x98, 0xA9, 0xBA, 0xCB, 0xDC, 0xED, 0xFE, 0x0F }

#define CMD_RPMB_ENABLE          1
#define CMD_IMAGE_VERIFY         3
#define HDCP_RX_HDMI_STATUS 0x80000080

#endif /* _SEP_APPLETS_H_ */
