  LOCAL_PATH:= $(call my-dir)

  include $(CLEAR_VARS)
  
  LOCAL_SRC_FILES := gpswrap.c

  LOCAL_MODULE := gps.tegra

  LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

  LOCAL_MODULE_TAGS := optional

  LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    libdl \
    libc
  
  include $(BUILD_SHARED_LIBRARY)

