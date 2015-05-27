LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                         \
        version.cpp
$(SHELL $(LOCAL_PATH)/version.sh)
LOCAL_CFLAGS += -Wno-multichar
LOCAL_MODULE:= libstagefright_version
include $(BUILD_STATIC_LIBRARY)
include $(CLEAR_VARS)
include frameworks/av/media/libstagefright/codecs/common/Config.mk

LOCAL_SRC_FILES:=                         \
        ACodec.cpp                        \
        AACExtractor.cpp                  \
        AACWriter.cpp                     \
        AMRExtractor.cpp                  \
        AMRWriter.cpp                     \
        AudioPlayer.cpp                   \
        AudioSource.cpp                   \
        FrameQueueManage.cpp              \
        AwesomePlayer.cpp                 \
        CameraSource.cpp                  \
        CameraSourceTimeLapse.cpp         \
        DataSource.cpp                    \
        DRMExtractor.cpp                  \
        ESDS.cpp                          \
        FileSource.cpp                    \
        FLACExtractor.cpp                 \
        HTTPBase.cpp                      \
        JPEGSource.cpp                    \
        MP3Extractor.cpp                  \
        MIRRORINGWriter.cpp               \
        MPEG2TSWriter.cpp                 \
        MPEG4Extractor.cpp                \
        MPEG4Writer.cpp                   \
        MediaAdapter.cpp                  \
        MediaBuffer.cpp                   \
        MediaBufferGroup.cpp              \
        MediaCodec.cpp                    \
        MediaCodecList.cpp                \
        MediaDefs.cpp                     \
        MediaExtractor.cpp                \
		ExtendedExtractor.cpp             \
	RkAudioDecoder.cpp		  \
		MediaMuxer.cpp                    \
        MediaSource.cpp                   \
        MetaData.cpp                      \
        NuCachedSource2.cpp               \
        NuMediaExtractor.cpp              \
        OMXClient.cpp                     \
        OMXCodec.cpp                      \
        OggExtractor.cpp                  \
        SampleIterator.cpp                \
        SampleTable.cpp                   \
        SkipCutBuffer.cpp                 \
        StagefrightMediaScanner.cpp       \
        StagefrightMetadataRetriever.cpp  \
        SurfaceMediaSource.cpp            \
        ThrottledSource.cpp               \
        TimeSource.cpp                    \
        TimedEventQueue.cpp               \
        Utils.cpp                         \
        VBRISeeker.cpp                    \
        WAVExtractor.cpp                  \
        WVMExtractor.cpp                  \
        XINGSeeker.cpp                    \
        avc_utils.cpp                     \
        mp4/FragmentedMP4Parser.cpp       \
        mp4/TrackFragment.cpp             \
		get_ape_id3.cpp                   \
        get_flac_id3.cpp                  \
        ApeGetFileInfo.cpp                \
		Audio_Mirror_Source.cpp		  \
		Video_Mirror_Source.cpp		  

LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/av/include/media/stagefright/timedtext \
        $(TOP)/frameworks/native/include/media/hardware \
        $(TOP)/frameworks/native/include/media/openmax \
        $(TOP)/frameworks/native/services/connectivitymanager \
        $(TOP)/external/flac/include \
        $(TOP)/external/tremolo \
	    $(TOP)/frameworks/av/include/media/stagefright \
        $(TOP)/frameworks/av/media/libstagefright/rtsp \
        $(TOP)/frameworks/av/media/libstagefright/include \
        $(TOP)/frameworks/av/media/libstagefright/libvpu/common/include \
        $(TOP)/frameworks/av/media/libstagefright/libvpu/common \
        $(TOP)/external/openssl/include \
    	$(TOP)/frameworks/av/media/libstagefright/libvpu/common/include \
	$(TOP)/hardware/rk29/libon2 

LOCAL_SHARED_LIBRARIES := \
        libbinder \
        libcamera_client \
        libconnectivitymanager \
        libcutils \
        libdl \
        libdrmframework \
        libexpat \
        libgui \
        libicui18n \
        libicuuc \
        liblog \
        libsonivox \
        libssl \
        libstagefright_omx \
        libstagefright_yuv \
        libsync \
        libui \
        libutils \
        libvorbisidec \
        libz \
        libpowermanager \
    	librk_on2 \
		libmedia 

LOCAL_STATIC_LIBRARIES := \
        libstagefright_color_conversion \
				libstagefright_aacdec_mirroring \
        libstagefright_aacenc \
        libstagefright_flacdec\
        libstagefright_rkon2dec \
				libstagefright_rkon2enc \
        libstagefright_avcenc \
        libstagefright_timedtext \
        libstagefright_wimover1 \
				libvpx \
        libwebm \
        libstagefright_mpeg2ts \
        libstagefright_id3 \
		libstagefright_hevcdec \
        libFLAC \
				libmedia_helper \
        libtvpad_decoder \
        libstagefright_version\
				libstagefright_mirroring

LOCAL_SRC_FILES += \
        chromium_http_stub.cpp
LOCAL_CPPFLAGS += -DCHROMIUM_AVAILABLE=1

LOCAL_SHARED_LIBRARIES += libstlport
include external/stlport/libstlport.mk

LOCAL_SHARED_LIBRARIES += \
        libstagefright_enc_common \
        libstagefright_avc_common \
        libstagefright_foundation \
        libdl\
        libvpu

LOCAL_CFLAGS += -Wno-multichar

LOCAL_MODULE:= libstagefright

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
