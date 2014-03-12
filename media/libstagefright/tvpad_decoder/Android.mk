LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                         \
	TvpadSource.cpp

#LOCAL_SHARED_LIBRARIES+= \
	libstagefright \
	libutils\
	libstagefright_omx\
	libstagefright_foundation\

#LOCAL_C_INCLUDES += \
	$(JNI_H_INCLUDE) \
	$(TOP)/frameworks/av/include/media/stagefright \
        $(TOP)/external/flac/include \
        $(TOP)/external/tremolo \
        $(TOP)/external/openssl/include \
	$(TOP)/hardware/rk29/libon2 \
	$(TOP)/hardware/rk29/libhisense

 #LOCAL_LDFLAGS := $(LOCAL_PATH)/tv2pad.a
#LOCAL_LDFLAGS := $(LOCAL_PATH)/libclientdata.a
#LOCAL_LDLIBS = -L$(LOCAL_PATH)

LOCAL_MODULE:= libtvpad_decoder
#LOCAL_MODULE_TAGS:= optional

#ifeq ($(TARGET_ARCH),arm)
#    LOCAL_CFLAGS += -Wno-psabi
#endif

include $(BUILD_STATIC_LIBRARY)
