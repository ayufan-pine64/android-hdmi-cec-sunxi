LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := hdmi_cec.test

LOCAL_MODULE_TAGS := optional

## Libs

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    liblog \
    libdl

LOCAL_SRC_FILES += \
    sunxi.c

LOCAL_CFLAGS += \
    -fno-short-enums \
    -D_ANDROID_ \
    -Iinclude/

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_RELATIVE_PATH := hw

include $(BUILD_SHARED_LIBRARY)

