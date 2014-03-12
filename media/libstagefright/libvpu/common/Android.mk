# Copyright 2006 The Android Open Source Project
LOCAL_PATH:= $(call my-dir)
BUILD_VPU_MEM_TEST := false
BUILD_VPU_TEST := false
BUILD_PPOP_TEST := false
BUILD_RK_LIST_TEST := false

include $(CLEAR_VARS)

LOCAL_MODULE := libvpu

ifeq ($(PLATFORM_VERSION),4.0.4)
	LOCAL_CFLAGS := -DAVS40 \
	-Wno-multichar 
else
	LOCAL_CFLAGS += -Wno-multichar 
endif

LOCAL_ARM_MODE := arm

LOCAL_PRELINK_MODULE := false


LOCAL_SHARED_LIBRARIES := libion libcutils	

LOCAL_C_INCLUDES := $(LOCAL_PATH) \
		    $(LOCAL_PATH)/include \
		    $(TOP)/hardware/libhardware/include \

LOCAL_SRC_FILES := vpu_mem.c \
                   vpu_drv.c \
	               vpu.c \
				   rk_list.cpp \
				   ppOp.cpp

LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

ifeq ($(BUILD_VPU_MEM_TEST),true)
include $(CLEAR_VARS)
LOCAL_MODULE := vpu_mem_test
LOCAL_CFLAGS += -Wno-multichar -DBUILD_VPU_MEM_TEST=1
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := libion libvpu libcutils
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
		    $(LOCAL_PATH)/include \
		    $(TOP)/hardware/libhardware/include
LOCAL_SRC_FILES := vpu_mem.c
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
endif

ifeq ($(BUILD_VPU_TEST),true)
include $(CLEAR_VARS)
LOCAL_MODULE := vpu_test
LOCAL_CFLAGS += -Wno-multichar -DBUILD_VPU_TEST=1
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := libion libvpu libcutils
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
		    $(LOCAL_PATH)/include \
		    $(TOP)/hardware/libhardware/include
LOCAL_SRC_FILES := vpu.c
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
endif


ifeq ($(BUILD_PPOP_TEST),true)
include $(CLEAR_VARS)
LOCAL_MODULE := pp_test
LOCAL_CPPFLAGS += -Wno-multichar -DBUILD_PPOP_TEST=1
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := libion libvpu libcutils
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
		    $(LOCAL_PATH)/include \
		    $(TOP)/hardware/libhardware/include
LOCAL_SRC_FILES := ppOp.cpp
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
endif
ifeq ($(BUILD_RK_LIST_TEST),true)
include $(CLEAR_VARS)
LOCAL_MODULE := rk_list_test
LOCAL_CFLAGS += -Wno-multichar -DBUILD_RK_LIST_TEST=1
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := libion libvpu libcutils
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
		    $(LOCAL_PATH)/include \
		    $(TOP)/hardware/libhardware/include
LOCAL_SRC_FILES := rk_list.cpp
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
endif
