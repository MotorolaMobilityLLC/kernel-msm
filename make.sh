#!/bin/bash
make \
ARCH=arm \
CROSS_COMPILE=~/work/gcc-linaro-7.1.1/bin/arm-linux-gnueabihf-
cp arch/arm/boot/zImage AnyKernel2/
# cp arch/arm/boot/dt.img AnyKernel2/dtb
# scripts/dtbTool -s 2048 -o arch/arm/boot/dt.img -p scripts/dtc/ arch/arm/boot/dts/qcom/
cd AnyKernel2/ && zip ../lsm-anykernel.zip $(ls) -r &>/dev/null
echo "Installer zip created @ 'lsm-anykernel.zip'!"

