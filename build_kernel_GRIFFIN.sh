#!/bin/sh
export KERNELDIR=`readlink -f .`

export ARCH=arm64

if [ ! -f $KERNELDIR/.config ];
then
	make defconfig TurboZ_defconfig
fi

. $KERNELDIR/.config

mv .git .git-halt

cd $KERNELDIR/
make -j3 || exit 1

mkdir -p $KERNELDIR/BUILT_GRIFFIN/lib/modules

rm $KERNELDIR/BUILT_GRIFFIN/lib/modules/*
rm $KERNELDIR/BUILT_GRIFFIN/zImage

find -name '*.ko' -exec cp -av {} $KERNELDIR/BUILT_GRIFFIN/lib/modules/ \;
${CROSS_COMPILE}strip --strip-unneeded $KERNELDIR/BUILT_GRIFFIN/lib/modules/*
if [ ! -f $KERNELDIR/arch/arm64/boot/zImage ];
then
	cp $KERNELDIR/arch/arm64/boot/Image.gz $KERNELDIR/BUILT_GRIFFIN/zImage
else
	cp $KERNELDIR/arch/arm64/boot/zImage $KERNELDIR/BUILT_GRIFFIN/zImage
fi

mv .git-halt .git
