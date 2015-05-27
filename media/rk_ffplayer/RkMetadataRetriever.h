#ifndef RK_METADATA_RETRIEVER_H
#define RK_METADATA_RETRIEVER_H

#ifndef INT64_C
#define INT64_C
#define UINT64_C
#endif

#include <media/MediaMetadataRetrieverInterface.h>
#include <utils/KeyedVector.h>
#include "vpu_api_interface.h"

typedef struct VC1InterLacedCheck {
    bool have_check;
    bool interlaced;
}VC1InterLacedCheck_t;

namespace android {

class RK_MetadataRetriever: public MediaMetadataRetrieverInterface{
public:
    RK_MetadataRetriever();
    virtual ~RK_MetadataRetriever();
    virtual status_t setDataSource(
            const char *url,
            const KeyedVector<String8, String8> *headers);

    virtual status_t setDataSource(int fd, int64_t offset, int64_t length);

    virtual VideoFrame *getFrameAtTime(int64_t timeUs, int option);

    virtual MediaAlbumArt *extractAlbumArt();

    virtual const char *extractMetadata(int keyCode);
private:

    bool                        isRkHwSupport(void *stream);
    int32_t                     parseNALSize(const uint8_t *data) const;
    static  void                ffmpegNotify(void* ptr, int level, const char* fmt, va_list vl);
    OMX_ON2_VIDEO_CODINGTYPE    mVideo_type;
    VPU_API                     mDec_api;
    void*                       mDec_handle;
    bool                        mHwdecFlag;
    int32_t                     mNALLengthSize;
    int64_t                     mLastTimeUs;
    bool                        mHeaderSendFlag;
    void                        DecoderVideoInit(void* stream);
    status_t                    prepareVideo();
    bool                        hwprepare(void *stream);
    void                        AvcDataParser(uint8_t **inputBuffer,uint8_t *tmpBuffer,uint32_t *inputLen);
    void                        RvDataParser(uint8_t **inputBuffer,uint8_t *tmpBuffer,uint32_t *inputLen,int64_t timeUs);
    void                        Vc1DataParser(void *stream,uint8_t **inputBuffer,uint8_t *tmpBuffer,uint32_t *inputLen);
    bool                        Sfdec(void *stream,void *packet_t,VideoFrame **frame);
    bool                        Hwdec(void *stream,void *packet_t,VideoFrame **frame);
    int32_t                     checkVc1StreamKeyFrame(void* stream, void* packet);
    void *			            mMovieFile;
    int 						mAudioStreamIndex;
    int 						mVideoStreamIndex;

    int                         mDuration;
    int                         mCurrentPosition;
    int                         mVideoWidth;
    int                         mVideoHeight;
    bool                        mIsHevc;
    void                        *mHevcHandle;
    VC1InterLacedCheck_t        mVc1InterlaceChk;
    Mutex                       mLock;

    bool mParsedMetaData;

    KeyedVector<int, String8> mMetaData;

    MediaAlbumArt *mAlbumArt;

    void parseMetaData();

    RK_MetadataRetriever(const RK_MetadataRetriever &);

    RK_MetadataRetriever &operator=(
            const RK_MetadataRetriever &);
};
}
#endif // RK_METADATA_RETRIEVER_H
