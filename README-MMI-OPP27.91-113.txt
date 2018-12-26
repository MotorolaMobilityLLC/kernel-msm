MMI-OPP27.91-113 For Moto E5 Plus

Kernel Modules:
---------------
Download prebuilts folder from Android distribution. Move it to $my_top_dir

For example:

git clone https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8

mkdir -p  $my_top_dir/prebuilts/gcc/linux-x86/arm

mv arm-eabi-4.8 $my_top_dir/prebuilts/gcc/linux-x86/arm/.

cross=$my_top_dir/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/arm-eabi- 

mkdir -p $my_top_dir/out/target/product/generic/obj/kernel

kernel_out_dir=$my_top_dir/out/target/product/generic/obj/kernel

/bin/bash -c "( perl -le 'print \"# This file was automatically generated from:\n#\t\" . join(\"\n#\t\", @ARGV) . \"\n\"' kernel/arch/arm/configs/msmcortex-perf_defconfig kernel/arch/arm/configs/ext_config/moto-msmcortex.config kernel/arch/arm/configs/ext_config/mot8937.config kernel/arch/arm/configs/ext_config/mot8937-hannah.config && cat kernel/arch/arm/configs/msmcortex-perf_defconfig kernel/arch/arm/configs/ext_config/moto-msmcortex.config kernel/arch/arm/configs/ext_config/mot8937.config kernel/arch/arm/configs/ext_config/mot8937-hannah.config ) > $kernel_out_dir/mapphone_defconfig || ( rm -f $kernel_out_dir/mapphone_defconfig && false )"

cp $kernel_out_dir/mapphone_defconfig $kernel_out_dir/.config 

make -j12 -C kernel O=$kernel_out_dir ARCH=arm CROSS_COMPILE=$cross KBUILD_BUILD_USER= KBUILD_BUILD_HOST= defoldconfig

make -j12 -C kernel O=$kernel_out_dir ARCH=arm CROSS_COMPILE=$cross oldconfig

make -j12 -C kernel O=$kernel_out_dir ARCH=arm CROSS_COMPILE=$cross KBUILD_BUILD_USER= KBUILD_BUILD_HOST= defoldconfig

make -j12 -C kernel O=$kernel_out_dir ARCH=arm CROSS_COMPILE=$cross KBUILD_BUILD_USER= KBUILD_BUILD_HOST= headers_install

make -j12 -C kernel KBUILD_RELSRC=$my_top_dir/kernel O=$kernel_out_dir ARCH=arm CROSS_COMPILE=$cross KBUILD_BUILD_USER= KBUILD_BUILD_HOST=

make -j12 -C kernel KBUILD_RELSRC=$my_top_dir/kernel O=$kernel_out_dir ARCH=arm CROSS_COMPILE=$cross KBUILD_BUILD_USER= KBUILD_BUILD_HOST= dtbs

make -j12 -C kernel KBUILD_RELSRC=$my_top_dir/kernel O=$kernel_out_dir ARCH=arm CROSS_COMPILE=$cross KBUILD_BUILD_USER= KBUILD_BUILD_HOST= modules

make -j12 -C kernel KBUILD_RELSRC=$my_top_dir/kernel O=$kernel_out_dir INSTALL_MOD_PATH=$my_top_dir/out/target/product/generic INSTALL_MOD_STRIP="--strip-debug --remove-section=.note.gnu.build-id" ARCH=arm CROSS_COMPILE=$cross KBUILD_BUILD_USER= KBUILD_BUILD_HOST= modules_install

make -C kernel O=$kernel_out_dir ARCH=arm CROSS_COMPILE=$cross clean

make -C kernel O=$kernel_out_dir ARCH=arm CROSS_COMPILE=$cross mrproper

make -C kernel O=$kernel_out_dir ARCH=arm CROSS_COMPILE=$cross distclean

make -j12 -C kernel O=$kernel_out_dir ARCH=arm CROSS_COMPILE=$cross KBUILD_BUILD_USER= KBUILD_BUILD_HOST= tags

env KCONFIG_NOTIMESTAMP=true make -j12 -C kernel O=$kernel_out_dir ARCH=arm CROSS_COMPILE=$cross KBUILD_BUILD_USER= KBUILD_BUILD_HOST= menuconfig

env KCONFIG_NOTIMESTAMP=true make -j12 -C kernel O=$kernel_out_dir ARCH=arm CROSS_COMPILE=$cross KBUILD_BUILD_USER= KBUILD_BUILD_HOST= savedefconfig

WLAN Driver:
------------
make -j12 -C kernel M=$my_top_dir/vendor/qcom/opensource/wlan/prima O=$kernel_out_dir ARCH=arm CROSS_COMPILE=$cross modules WLAN_ROOT=$my_top_dir/vendor/qcom/opensource/wlan/prima MODNAME=wlan BOARD_PLATFORM=msm8937 CONFIG_PRONTO_WLAN=m
