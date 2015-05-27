#ifndef VPU_API_INTERFACE_H_
#define VPU_API_INTERFACE_H_
#include "vpu_global.h"

typedef struct
{
   int width;
   int height;
   int rc_mode;
   int bitRate;
   int framerate;
   int	qp;
   int	reserved[10];
}EncParams1;

typedef enum
{
    H264ENC_YUV420_PLANAR = 0,  /* YYYY... UUUU... VVVV */
    H264ENC_YUV420_SEMIPLANAR = 1,  /* YYYY... UVUVUV...    */
    H264ENC_YUV422_INTERLEAVED_YUYV = 2,    /* YUYVYUYV...          */
    H264ENC_YUV422_INTERLEAVED_UYVY = 3,    /* UYVYUYVY...          */
    H264ENC_RGB565 = 4, /* 16-bit RGB           */
    H264ENC_BGR565 = 5, /* 16-bit RGB           */
    H264ENC_RGB555 = 6, /* 15-bit RGB           */
    H264ENC_BGR555 = 7, /* 15-bit RGB           */
    H264ENC_RGB444 = 8, /* 12-bit RGB           */
    H264ENC_BGR444 = 9, /* 12-bit RGB           */
    H264ENC_RGB888 = 10,    /* 24-bit RGB           */
    H264ENC_BGR888 = 11,    /* 24-bit RGB           */
    H264ENC_RGB101010 = 12, /* 30-bit RGB           */
    H264ENC_BGR101010 = 13  /* 30-bit RGB           */
} H264EncPictureType;

typedef enum {
    NONE,
    SET_H264_NO_THREAD
} CONTROL_CMD;
/**
 * Enumeration used to define the possible video compression codings.
 * NOTE:  This essentially refers to file extensions. If the coding is
 *        being used to specify the ENCODE type, then additional work
 *        must be done to configure the exact flavor of the compression
 *        to be used.  For decode cases where the user application can
 *        not differentiate between MPEG-4 and H.264 bit streams, it is
 *        up to the codec to handle this.
 */

//sync with the omx_video.h
typedef enum OMX_ON2_VIDEO_CODINGTYPE {
    OMX_ON2_VIDEO_CodingUnused,     /**< Value when coding is N/A */
    OMX_ON2_VIDEO_CodingAutoDetect, /**< Autodetection of coding type */
    OMX_ON2_VIDEO_CodingMPEG2,      /**< AKA: H.262 */
    OMX_ON2_VIDEO_CodingH263,       /**< H.263 */
    OMX_ON2_VIDEO_CodingMPEG4,      /**< MPEG-4 */
    OMX_ON2_VIDEO_CodingWMV,        /**< all versions of Windows Media Video */
    OMX_ON2_VIDEO_CodingRV,         /**< all versions of Real Video */
    OMX_ON2_VIDEO_CodingAVC,        /**< H.264/AVC */
    OMX_ON2_VIDEO_CodingMJPEG,      /**< Motion JPEG */
    OMX_ON2_VIDEO_CodingFLV1 = 0x01000000,       /**< Sorenson H.263 */
    OMX_ON2_VIDEO_CodingDIVX3,                   /**< DIVX3 */
    OMX_ON2_VIDEO_CodingVPX,                     /**< VP8 */
    OMX_ON2_VIDEO_CodingVP6,
    OMX_ON2_VIDEO_CodingKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_ON2_VIDEO_CodingVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_ON2_VIDEO_CodingMax = 0x7FFFFFFF
} OMX_ON2_VIDEO_CODINGTYPE;

typedef struct tag_VPU_API {
    void* (*         get_class_On2Decoder)(void);
    void  (*     destroy_class_On2Decoder)(void *decoder);
    int   (*      deinit_class_On2Decoder)(void *decoder);
    int   (*        init_class_On2Decoder)(void *decoder);
    int   (*        init_class_On2Decoder_M4VH263)(void *decoder, VPU_GENERIC *vpug);
    int   (*        init_class_On2Decoder_VC1)(void *decoder, unsigned char *tmpStrm, unsigned int size,unsigned int extraDataSize);
    int   (*        init_class_On2Decoder_VP6)(void *decoder, int codecid);
    int   (*        init_class_On2Decoder_AVC)(void *decoder,int tsFlag);
    int   (*       reset_class_On2Decoder)(void *decoder);
    int   (*dec_oneframe_class_On2Decoder)(void *decoder, unsigned char* aOutBuffer, unsigned int *aOutputLength, unsigned char* aInputBuf, unsigned int* aInBufSize);
    int   (*dec_oneframe_class_On2Decoder_WithTimeStamp)(void *decoder, unsigned char* aOutBuffer, unsigned int *aOutputLength, unsigned char* aInputBuf, unsigned int* aInBufSize, long long *InputTimestamp);
    int   (*get_oneframe_class_On2Decoder)(void *decoder, unsigned char* aOutBuffer, unsigned int* aOutputLength);
    void  (*set_width_Height_class_On2Decoder_RV)(void *decoder, unsigned int* width, unsigned int* height);

	void* (*         get_class_On2Encoder)(void);
    void  (*     destroy_class_On2Encoder)(void *encoder);
    int   (*      deinit_class_On2Encoder)(void *encoder);
    int   (*        init_class_On2Encoder)(void *encoder,EncParams1 *aEncOption, unsigned char * aOutBuffer,unsigned int* aOutputLength);
    int   (*enc_oneframe_class_On2Encoder)(void *encoder, unsigned char* aOutBuffer, unsigned int * aOutputLength,unsigned char* aInputBuf,unsigned int  aInBuffPhy,unsigned int *aInBufSize,unsigned int * aOutTimeStamp, int* aSyncFlag);
    void  (*enc_getconfig_class_On2Encoder)(void * AvcEncoder,EncParams1* vpug);
    void  (*enc_setconfig_class_On2Encoder)(void * AvcEncoder,EncParams1* vpug);
    int   (*enc_setInputFormat_class_On2Encoder)(void * AvcEncoder,H264EncPictureType inputFormat);
    void  (*enc_SetintraPeriodCnt_class_On2Encoder)(void * AvcEncoder);
    void  (*enc_SetInputAddr_class_On2Encoder)(void * AvcEncoder,unsigned long input);
    int   (*rk_codec_dec_control)(void *handle, CONTROL_CMD cmd,unsigned int *data);
} VPU_API;

#ifdef __cplusplus
extern "C"
{
#endif
 void vpu_api_init(VPU_API *vpu_api, OMX_ON2_VIDEO_CODINGTYPE video_coding_type);
#ifdef __cplusplus
}
#endif

typedef void (*VpuApiInitFactory)(VPU_API *vpu_api, OMX_ON2_VIDEO_CODINGTYPE video_coding_type);
//Function name for extractor factory function. Extended extractor must export a function with this name.
static const char* VPU_API_INIT = "vpu_api_init";

#endif

