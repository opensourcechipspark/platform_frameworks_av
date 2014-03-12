LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=								\
        WimoVer1Extractor.cpp		\
        WimoVer1demux.cpp				\
        WimoSource.cpp

LOCAL_C_INCLUDES:= \
	$(JNI_H_INCLUDE) \
        $(TOP)/frameworks/av/include/media/stagefright \
	$(TOP)/frameworks/av/media/libstagefright\

LOCAL_CFLAGS += -Wno-multichar

LOCAL_MODULE:= libstagefright_wimover1

include $(BUILD_STATIC_LIBRARY)
