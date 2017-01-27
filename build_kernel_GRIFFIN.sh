#!/bin/sh
export KERNELDIR=`pwd`
export CROSS_COMPILE="aarch64-linux-androidkernel-"
export ARCH=arm64

#Set Correct Path Here
export WIFIDRIVER=$KERNELDIR/../vendor-qcom-opensource-wlan-qcacld-2.0
export GREYBUSDRIVER=$KERNELDIR/../motorola-external-greybus
export V4L2DRIVER=$KERNELDIR/../motorola-external-v4l2_hal

if [ ! -f $KERNELDIR/.config ];
then
	make defconfig TurboZ_defconfig
fi

. $KERNELDIR/.config

cd $KERNELDIR/
make -j10 || exit 1

mkdir -p $KERNELDIR/BUILT_GRIFFIN/modules
mkdir -p $KERNELDIR/BUILT_GRIFFIN/modules/qca_cld

rm $KERNELDIR/BUILT_GRIFFIN/modules/*
rm $KERNELDIR/BUILT_GRIFFIN/zImage

find -name '*.ko' -exec cp -av {} $KERNELDIR/BUILT_GRIFFIN/modules/ \;

if [ ! -f $KERNELDIR/arch/arm64/boot/zImage ];
then
	cp $KERNELDIR/arch/arm64/boot/Image.gz-dtb $KERNELDIR/BUILT_GRIFFIN/zImage
else
	cp $KERNELDIR/arch/arm64/boot/zImage $KERNELDIR/BUILT_GRIFFIN/zImage
fi

make -j10 M=$WIFIDRIVER WLAN_ROOT=$WIFIDRIVER MODNAME=wlan BOARD_PLATFORM=msm8996 CONFIG_QCA_CLD_WLAN=m WLAN_OPEN_SOURCE=1 modules
cp $WIFIDRIVER/wlan.ko $KERNELDIR/BUILT_GRIFFIN/modules/qca_cld/qca_cld_wlan.ko
rm $KERNELDIR/BUILT_GRIFFIN/modules/qca_cld_wlan.ko

cd $GREYBUSDRIVER 
make -j10 -C $KERNELDIR M=$GREYBUSDRIVER
find -name '*.ko' -exec cp -av {} $KERNELDIR/BUILT_GRIFFIN/modules/ \;

cd $V4L2DRIVER
make -j10 -C $KERNELDIR M=$V4L2DRIVER
find -name '*.ko' -exec cp -av {} $KERNELDIR/BUILT_GRIFFIN/modules/ \;

cd $KERNELDIR
${CROSS_COMPILE}strip --strip-unneeded $KERNELDIR/BUILT_GRIFFIN/modules/*
${CROSS_COMPILE}strip --strip-unneeded $KERNELDIR/BUILT_GRIFFIN/modules/qca_cld/*
