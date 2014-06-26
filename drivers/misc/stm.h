/*
 * stm.h
 *
 *  Copyright (C) Intel 2011
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
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The STM (Sytem Trace Macro) Unit driver configure trace output
 * to the Intel Tangier PTI port and DWC3 USB xHCI controller
 * out of the mobile device for analysis with a debugging tool
 * (Lauterbach, Fido). This is part of a solution for the MIPI P1149.7,
 * compact JTAG, standard and USB Debug-Class
 *
 * This header file will allow other parts of the OS to use the
 * interface to write out it's contents for debugging a mobile system.
 */

#ifndef _STM_H
#define _STM_H

#include <linux/pci.h>

/* STM_CTRL register bitmap */
/**
 * struct stm_ctrl - STM control block
 * @usb_debug_en : STM needs to redirece the trace packet to the USB3
 * @pti_io_idle_threshold : threshold for disabling the IO clock.
 * @pkt_transfer_size : asserts the *buff_avail signal after it has
 * 1 or 2 KB of data in buffer
 * @dis_dcu7_use : disables the useage of DCU7 instead of PTI_Disable
 * @en_sw_ms : enables software master usage
 * @mst_id_en : enables the PTI unit to suppress sending the Master Command
 * @d64_cmd_en : PTI unit to use the D64 commands
 * @pti_out_mode_sel
 *	0 = PTI 4-bits legacy end user
 *	1 = PTI 4-bits NiDnT
 *	2 = PTI 16-bits
 *	3 = PTI 12-bits
 *	4 = PTI 8-bits
 * @pti_out_en : PTI output enable muxselects that propagate
 * to the FLIS to be enabled
 * @lossy_mode_enable : Output Agent will continue to accept writes,
 * even if the queuese are full. The data will be dropped and the
 * dropped packet indicator will be incremented
 * @time_stamp_enable : Enable time stamping the final packet in trace record.
 */
struct stm_ctrl {
	union {
		struct {
			u32             time_stamp_enable:1;
			u32             lossy_mode_enable:1;
			u32             pti_out_en:1;
			u32             reserved:1;
			u32             pti_out_mode_sel:4;
			u32             d64_cmd_en:1;
			u32             mst_id_en:1;
			u32             en_sw_ms:1;
			u32             dis_dcu7_use:1;
			u32             pkt_transfer_size:1;
			u32             pti_io_idle_threshold:5;
			u32             usb_debug_en:1;
			u32             reserved31_19:13;
		};
		u32 reg_word;
	};
} __packed;

/**
 * struct stm_usb3_ctrl - STM buffer USB3 hardware EBC
 * @region_closure_threshold : This is the threshold for closing
 * the 1KB region in the debug trace buffer. STM will wait for the
 * configured time as specified in this field and then closes the region.
 * The unit of this field is in 64 us. Eg when this field value is set
 * to 0xffff, then it indicates 2 ms
 * @empty_packets_threshold : When STM does not have data to send,
 * it can send empty packets to keep the USB3 alive. This is useful
 * in case of ISOC traffic, because in this mode the wake up latency
 * is high. STM will send the configured number of empty packets as
 * specified in this field.
 */
struct stm_usb3_ctrl {
	union {
		struct {
			u32             region_closure_threshold:15;
			u32             empty_packets_threshold:6;
			u32             reserved31_21:11;
		};
		u32 reg_word;
	};
} __packed;

struct stm_dev {
	unsigned long stm_addr;
	unsigned long stm_reg_base;
	unsigned long stm_trb_base;
	void __iomem *stm_ioaddr;
	void __iomem *trb_ioaddr;
	struct stm_ctrl stm_ctrl_hwreg;
	struct stm_usb3_ctrl stm_usb3_hwreg;
};

int stm_dev_init(struct pci_dev *pdev, struct stm_dev *dev_stm);
void stm_dev_clean(struct pci_dev *pdev, struct stm_dev *dev_stm);

#endif /* _STM_H */
