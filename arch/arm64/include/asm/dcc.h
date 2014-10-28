/* Copyright (c) 2014 The Linux Foundation. All rights reserved.
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

<<<<<<< HEAD:arch/arm/boot/dts/qcom/mpq8092-cdp.dts
/dts-v1/;

/include/ "mpq8092.dtsi"
/include/ "mpq8092-cdp.dtsi"

/ {
	model = "Qualcomm MPQ 8092 CDP";
	compatible = "qcom,mpq8092-cdp", "qcom,mpq8092", "qcom,cdp";
	qcom,board-id = <1 0>,
			<1 1>,
			<21 0>,
			<21 1>;

	aliases {
		serial0 = &blsp1_uart4;
	};
};

&blsp1_uart4 {
	status = "ok";
};

&ehci {
	status = "ok";
	qcom,ext-hub-reset-gpio = <&msmgpio 0 0>;
	vbus-supply = <&hsusb2_otg>;
};
||||||| merged common ancestors
/dts-v1/;

#include "mpq8092.dtsi"
#include "mpq8092-cdp.dtsi"

/ {
	model = "Qualcomm MPQ 8092 CDP";
	compatible = "qcom,mpq8092-cdp", "qcom,mpq8092", "qcom,cdp";
	qcom,board-id = <1 0>,
			<1 1>,
			<21 0>,
			<21 1>;

	aliases {
		serial0 = &blsp1_uart4;
	};
};

&blsp1_uart4 {
	status = "ok";
};

&ehci {
	status = "ok";
	qcom,ext-hub-reset-gpio = <&msmgpio 0 0>;
	vbus-supply = <&hsusb2_otg>;
};
=======
#include <asm/barrier.h>

static inline u32 __dcc_getstatus(void)
{
	u32 __ret;
	asm volatile("mrs %0, mdccsr_el0" : "=r" (__ret)
			: : "cc");

	return __ret;
}


static inline char __dcc_getchar(void)
{
	char __c;

	asm volatile("mrs %0, dbgdtrrx_el0" : "=r" (__c));
	isb();

	return __c;
}

static inline void __dcc_putchar(char c)
{
	asm volatile("msr dbgdtrtx_el0, %0"
		: /* No output register */
		: "r" (c));
	isb();
}
>>>>>>> 07723b4952fbbd1b6f76c1219699ba0b30b189e1:arch/arm64/include/asm/dcc.h
