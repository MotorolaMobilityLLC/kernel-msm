LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := cld-fwlog-record
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../CORE/SERVICES/COMMON
LOCAL_SHARED_LIBRARIES := libc libcutils
LOCAL_SRC_FILES := cld-fwlog-record.c
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := cld-fwlog-netlink
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES := $(TARGET_OUT_HEADERS)/diag/include \
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/common/inc \
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../../wlan/utils/asf/inc \
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../CORE/SERVICES/COMMON
LOCAL_SHARED_LIBRARIES := libc libcutils libdiag libhardware_legacy
LOCAL_SRC_FILES := cld-fwlog-netlink.c parser.c nan-parser.c cld-diag-parser.c
LOCAL_CFLAGS += -DCONFIG_ANDROID_LOG
LOCAL_CFLAGS += -DANDROID
LOCAL_LDLIBS += -llog
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := cnss_diag
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES := $(TARGET_OUT_HEADERS)/diag/include \
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/common/inc \
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../../wlan/utils/asf/inc \
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../CORE/SERVICES/COMMON
LOCAL_SHARED_LIBRARIES := libc libcutils libdiag libhardware_legacy
LOCAL_SRC_FILES := cld-fwlog-netlink.c parser.c nan-parser.c cld-diag-parser.c
LOCAL_CFLAGS += -DCONFIG_ANDROID_LOG
LOCAL_CFLAGS += -DANDROID
LOCAL_LDLIBS += -llog
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_MODULE := cld-fwlog-parser
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../CORE/SERVICES/COMMON
LOCAL_SHARED_LIBRARIES := libc libcutils
LOCAL_SRC_FILES := cld-fwlog-parser.c
include $(BUILD_EXECUTABLE)
