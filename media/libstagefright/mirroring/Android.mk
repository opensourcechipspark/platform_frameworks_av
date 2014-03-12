LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=           \
	Audio_OutPut_Source.cpp		\
	UiSource.cpp			\
	RtpSource.cpp			\
	SenderSource.cpp
	
	
#LOCAL_SHARED_LIBRARIES+= \
	libstagefright \
	libutils\
	libstagefright_omx\
	libstagefright_foundation\

LOCAL_C_INCLUDES += \
	$(JNI_H_INCLUDE) \
	$(TOP)/frameworks/av/include/media/stagefright \
	$(TOP)/frameworks/av/include/media \
	$(TOP)/frameworks/av/media/libstagefright/libvpu/common/include \
	$(TOP)/frameworks/av/media/libstagefright/libvpu/common \
	$(TOP)/frameworks/av/media/libstagefright \
	$(TOP)/frameworks/native/include/media/openmax

LOCAL_MODULE:= libstagefright_mirroring
#LOCAL_MODULE_TAGS:= optional

#ifeq ($(TARGET_ARCH),arm)
#    LOCAL_CFLAGS += -Wno-psabi
#endif

include $(BUILD_STATIC_LIBRARY)
