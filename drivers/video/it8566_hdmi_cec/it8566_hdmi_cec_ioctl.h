/*
 * ITE it8566 HDMI CEC driver
 *
 * Copyright(c) 2014 ASUSTek COMPUTER INC. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _IT8566_HDMI_CEC_IOCTL_H
#define _IT8566_HDMI_CEC_IOCTL_H

/*error code for send cec_msg ioctl*/
enum {
	RESULT_SUCCESS = 0,
	RESULT_NACK = 1,        /* not acknowledged */
	RESULT_BUSY = 2,        /* bus is busy */
	RESULT_FAIL = 3,
};

#define CEC_MESSAGE_BODY_MAX_LENGTH	16 /*should be 14? */
struct it8566_cec_msg {
	/* logical address of sender */
	unsigned short initiator;

	/* logical address of receiver */
	unsigned short destination;

	/* Length in bytes of body, range [0, CEC_MESSAGE_BODY_MAX_LENGTH] */
	unsigned short length;
	unsigned char body[CEC_MESSAGE_BODY_MAX_LENGTH];
	/* for sent cec result*/
	unsigned char result;
};

#define IT8566_HDMI_CEC_IOCTL_SEND_MESSAGE \
	_IOWR('\x66', 1, struct it8566_cec_msg)
#define IT8566_HDMI_CEC_IOCTL_RCEV_MESSAGE \
	_IOR('\x66', 2, struct it8566_cec_msg)
#define IT8566_HDMI_CEC_IOCTL_SET_LA \
	_IOW('\x66', 3, unsigned char)

/*for it8566 status*/
#define RESULT_CEC_BUS_ERR	0x1
#define RESULT_CEC_NACK		(0x1 << 1)
#define RESULT_TX_BUSY		(0x1 << 2)
#define RESULT_RX_BUSY		(0x1 << 3)
#define RESULT_TX_SUCCESS	(0x1 << 7)

#endif /* _IT8566_HDMI_CEC_IOCTL_H */
