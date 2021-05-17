#!/bin/bash
# Copyright (c) 2020, Motorola. All rights reserved.

# Script to merge all Moto kernel config

usage() {
	echo "Usage: $0 <input_platform_name> $1<required defconfig>"
	echo "For example $0 lahaina $1 qgki_defconfig"
	echo "Note: The output is all valid moto kernel config files"
	exit 1
}

if [ "$#" -ne 2 ]; then
	echo "Error: Invalid number of arguments"
	usage
fi

#echo "framgement_moto: TARGET_PRODUCT=${TARGET_PRODUCT} TARGET_BUILD_VARIANT=${TARGET_BUILD_VARIANT}"

TARGET_PRODUCT_NAME=${TARGET_PRODUCT%%_*}
TARGET_PRODUCT_TYPE=${TARGET_PRODUCT#*_}
LOCAL_PLATFORM_NAME=${1}
REQUIRED_DEFCONFIG=${2}
echo "moto framgement config: LOCAL_PLATFORM_NAME=${LOCAL_PLATFORM_NAME} TARGET_PRODUCT_NAME=$TARGET_PRODUCT_NAME TARGET_PRODUCT_TYPE=$TARGET_PRODUCT_TYPE"

#skip msi products
if [ $TARGET_PRODUCT_NAME == "msi" ]; then
    echo "This is msi, skip moto config fragements"
    exit 0
fi

MOTO_CONFIG_DIR=$CONFIGS_DIR/ext_config
export MOTO_REQUIRED_CONFIG=""

# merge factory config or userdebug config
if [ $TARGET_PRODUCT_TYPE == "factory" ]; then
    if [ -e $MOTO_CONFIG_DIR/factory-${LOCAL_PLATFORM_NAME}-${TARGET_PRODUCT_NAME}.config ]; then
        MOTO_REQUIRED_CONFIG+=" $MOTO_CONFIG_DIR/factory-${LOCAL_PLATFORM_NAME}-${TARGET_PRODUCT_NAME}.config"
    fi

    if [ -e $MOTO_CONFIG_DIR/factory-${LOCAL_PLATFORM_NAME}.config ]; then
        MOTO_REQUIRED_CONFIG+=" $MOTO_CONFIG_DIR/factory-${LOCAL_PLATFORM_NAME}.config"
    fi
elif [ $TARGET_BUILD_VARIANT == "userdebug" ] && [ $REQUIRED_DEFCONFIG != ${LOCAL_PLATFORM_NAME}-gki_defconfig ]; then
    #MMI_STOPSHIP : kernel: skip all userdebug config
    if [ ${TARGET_PRODUCT_NAME} != "berlin" ] && [ ${TARGET_PRODUCT_NAME} != "berlna" ]; then
    if [ -e $MOTO_CONFIG_DIR/debug-${LOCAL_PLATFORM_NAME}-${TARGET_PRODUCT_NAME}.config ]; then
        MOTO_REQUIRED_CONFIG+=" $MOTO_CONFIG_DIR/debug-${LOCAL_PLATFORM_NAME}-${TARGET_PRODUCT_NAME}.config"
    fi

    if [ -e $MOTO_CONFIG_DIR/debug-${LOCAL_PLATFORM_NAME}.config ]; then
        MOTO_REQUIRED_CONFIG+=" $MOTO_CONFIG_DIR/debug-${LOCAL_PLATFORM_NAME}.config"
    fi

    if [ -e $MOTO_CONFIG_DIR/${LOCAL_PLATFORM_NAME}_debug.config ]; then
        MOTO_REQUIRED_CONFIG+=" $MOTO_CONFIG_DIR/${LOCAL_PLATFORM_NAME}_debug.config"
        if [ -e $MOTO_CONFIG_DIR/${LOCAL_PLATFORM_NAME}_consolidate.config ]; then
            MOTO_REQUIRED_CONFIG+=" $MOTO_CONFIG_DIR/${LOCAL_PLATFORM_NAME}_consolidate.config"
        fi
    fi
    fi
fi

#merge moto product config
if [ -e $MOTO_CONFIG_DIR/moto-${LOCAL_PLATFORM_NAME}-${TARGET_PRODUCT_NAME}.config ]; then
    MOTO_REQUIRED_CONFIG+=" $MOTO_CONFIG_DIR/moto-${LOCAL_PLATFORM_NAME}-${TARGET_PRODUCT_NAME}.config"
fi

#merge moto platform config
if [ -e $MOTO_CONFIG_DIR/moto-${LOCAL_PLATFORM_NAME}.config ]; then
    MOTO_REQUIRED_CONFIG+=" $MOTO_CONFIG_DIR/moto-${LOCAL_PLATFORM_NAME}.config"
fi

#merge moto platform config that depended on -QGKI.config
if [ $REQUIRED_DEFCONFIG != ${LOCAL_PLATFORM_NAME}-gki_defconfig ]; then
    if [ -e $MOTO_CONFIG_DIR/moto-${LOCAL_PLATFORM_NAME}-depQGKI.config ]; then
        MOTO_REQUIRED_CONFIG+=" $MOTO_CONFIG_DIR/moto-${LOCAL_PLATFORM_NAME}-depQGKI.config"
    fi
fi

echo "require moto blend configs:$MOTO_REQUIRED_CONFIG"
