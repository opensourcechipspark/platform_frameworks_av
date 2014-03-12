LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  HEVCDecoder.cpp \


LOCAL_MODULE := libstagefright_hevcdec

LOCAL_C_INCLUDES := \
	$(TOP)/frameworks/av/media/libstagefright/include \
  $(TOP)/frameworks/native/include/media/openmax \
  $(TOP)/hardware/rk29/libon2 \


include $(BUILD_STATIC_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
