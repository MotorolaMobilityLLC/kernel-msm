#ifndef _LINUX_SERIAL_MAX3110_H
#define _LINUX_SERIAL_MAX3110_H

/**
 * struct plat_max3110 - MAX3110 SPI UART platform data
 * @irq_edge_trigger: if IRQ is edge triggered
 *
 * You should use this structure in your machine description to specify
 * how the MAX3110 is connected.
 *
 */
struct plat_max3110 {
	int irq_edge_triggered;
};

#endif
