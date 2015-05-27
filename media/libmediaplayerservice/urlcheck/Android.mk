LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                 \
        Urlcheck.cpp     \

LOCAL_C_INCLUDES:= \
	$(JNI_H_INCLUDE) \

LOCAL_CFLAGS += -Wno-multichar

LOCAL_MODULE:= libstagefright_urlcheck

include $(BUILD_STATIC_LIBRARY)
