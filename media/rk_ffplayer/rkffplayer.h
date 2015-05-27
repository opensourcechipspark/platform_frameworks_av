#ifndef FFMPEG_MEDIAPLAYER_H
#define FFMPEG_MEDIAPLAYER_H

#ifndef INT64_C
#define INT64_C
#define UINT64_C
#endif

#include <pthread.h>
#include <utils/threads.h>
#include <media/MediaPlayerInterface.h>
#include <utils/List.h>
#include <utils/Vector.h>
#include "vpu_global.h"

struct DecoderAudio;
struct DecoderVideo;
struct DecoderSubtitle;
struct PacketQueue;
struct FFTimedText;

namespace android {
#ifdef AVS44
struct IGraphicBufferProducer;
#else
struct ISurfaceTexture;
#endif
struct RkAudioPlayer;
struct RkFrameManage;
struct RkRenderer;

#define MEDIA_PLAYER_DECODED 1 << 8

typedef struct RkFFSeekFlushInfo
{
    bool    aFlushFlag;
    int64_t flushTimeUs;
}RkFFSeekFlushInfo_t;

typedef struct RkFFStrmSelect
{
    int32_t aStrmIdx;
    int32_t vStrmIdx;
    int32_t sStrmIdx;

}RkFFStrmSelect_t;

class FF_MediaPlayer
{
public:
    FF_MediaPlayer();
    ~FF_MediaPlayer();

    void setListener(const wp<MediaPlayerBase> &listener);

    status_t setDataSource(int fd, int64_t offset, int64_t length);
    status_t setDataSource(const char *uri, const KeyedVector<String8, String8> *headers);

#ifdef AVS44
    status_t setSurfaceTexture(const sp<IGraphicBufferProducer> &bufferProducer);
#else
    status_t setSurfaceTexture(const sp<ISurfaceTexture> &surfaceTexture);
#endif

    void MakeFullHeaders(const KeyedVector<String8, String8> *overrides, String8 *headers);//ht for mutil-headers
    void setAudioSink(const sp<MediaPlayerBase::AudioSink> &audioSink);

    void            checkVideoInfoChange(void* frame,DEC_TYPE dec_type);
    void            initRender(int halFormat);
	status_t        start();
	status_t        pause();
	bool            isPlaying();
	status_t        getVideoWidth(int *w);
	status_t        getVideoHeight(int *h);
	status_t        seekTo(int64_t timeUs);
	status_t        getDuration(int64_t *durationUs);
	status_t        reset();
    status_t        getPosition(int64_t *positionUs);
	status_t        setAudioStreamType(int type);
	status_t		prepare();
    status_t        prepareAsync();
	void            notify(int msg, int ext1, int ext2);
    status_t        invoke(const Parcel &request, Parcel *reply);
    status_t        getParameter(int key, Parcel *reply);
    status_t         setParameter(int key, const Parcel &request);
    uint32_t        flags() const ;

	status_t        resume();

    enum SeekType {
        NO_SEEK,
        SEEK,
        SEEK_VIDEO_ONLY,
        SEEK_DONE,
    };
    SeekType mSeeking;

    RkFrameManage   *pfrmanager;
    int64_t         onDisplayEvent();

    void seekAudioIfNecessary();
    void finishSeekIfNecessary(void* avPkt);

    enum FlagMode {
        SET,
        CLEAR,
        ASSIGN
    };
    void modifyFlags(unsigned value, FlagMode mode);
private:
     enum PlayerStatus{
        PLAYER_STATE_ERROR        = 0,
        PLAYER_IDLE               = 0x01,
        PLAYER_INITIALIZED        = 0x02,
        PLAYER_PREPARING          = 0x04,
        PLAYER_PREPARED           = 0x08,
        PLAYER_STARTED            = 0x10,
        PLAYER_PAUSED             = 0x20,
        PLAYER_STOPPED            = 0x40,
        PLAYER_PLAYBACK_COMPLETE  = 0x80,
        PLAYER_DECODED            = 0x0100,
        PLAYER_READ_END_OF_STREAM = 0x0200,
    };

    enum {
        PLAYING             = 0x01,
        LOOPING             = 0x02,
        FIRST_FRAME         = 0x04,
        PREPARING           = 0x08,
        PREPARED            = 0x10,
        AT_EOS              = 0x20,
        PREPARE_CANCELLED   = 0x40,
        CACHE_UNDERRUN      = 0x80,
        AUDIO_AT_EOS        = 0x0100,
        VIDEO_AT_EOS        = 0x0200,
        AUTO_LOOPING        = 0x0400,

        // We are basically done preparing but are currently buffering
        // sufficient data to begin playback and finish the preparation phase
        // for good.
        PREPARING_CONNECTED = 0x0800,

        // We're triggering a single video event to display the first frame
        // after the seekpoint.
        SEEK_PREVIEW        = 0x1000,

        AUDIO_RUNNING       = 0x2000,
        AUDIOPLAYER_STARTED = 0x4000,

        INCOGNITO           = 0x8000,

        TEXT_RUNNING        = 0x10000,
        TEXTPLAYER_INITIALIZED  = 0x20000,

        SLOW_DECODER_HACK   = 0x40000,
    };

    struct TrackInfo{
#ifdef AVS40
        media_parameter_keys type;
#else
        media_track_type type;
#endif
        int32_t index;
        String16 mLanguage;
        Vector<void *> packet;
    };
    status_t                setVideoScalingMode(int32_t mode);
    status_t                setVideoScalingMode_l(int32_t mode);
    status_t                selectTrack(size_t trackIndex, bool select);
    status_t                getTrackInfo(Parcel *reply);
	status_t                setDataSource(const char *url);
    status_t                selectAudioTrack_l(size_t trackIndex);
    status_t                selectSubtitleTrack_l(size_t trackIndex);
	status_t				prepareAudio();
	status_t				prepareVideo();
    status_t                prepareSubtitle();
	bool				    shouldCancel(PacketQueue* queue);
	static void				ffmpegNotify(void* ptr, int level, const char* fmt, va_list vl);
	static void*			startPlayer(void* ptr);

	static void 			decode(void* frame, int64_t pts,DEC_TYPE dec_type,void *me);
	static void 			decode(int16_t* buffer, int buffer_size,int64_t pts,int32_t dealt,void *me);
    static void             decodeSubtitle(void* obj, int32_t msg_type, void *me);

	void					decodeMovie(void* ptr);
    int64_t                 readpacket();
    int64_t                 hlsReadBaseTimeUs(bool discontinuity,int64_t currentbase = -1);
    void                    onBufferingUpdate();
    int32_t                 getCacheSize();
    int32_t                 getCacheduration();

    void                    notifyListener_l(int msg, int ext1, int ext2 = 0);
    void                    notifyTimedTextListener(const Parcel *parcel);
    void                    parseTimedText(void* sp, int32_t msg_type);
    void                    freeFFSubPicture(void* sp);
    void                    writeTimedTextToParcel(void* data, int size, int timeMs);
    void                    pushOtherPacket(void *packet);
    void                    deleteOtherPacket();
    bool                    checkConsumeAll();
    void                    checkAC3orDTS(int index);
    status_t                pause_l();
    void                    initAudioPlayer();
    bool                    startAllDecoders(void* ptr);
    int32_t                 checkVc1StreamKeyFrame(void* stream, void* packet);

    Vector<TrackInfo*>      mTrackInfos;
    Vector<FFTimedText*>    mTimedText;
    Mutex                   mTimedLock;
	double                  mTime;
	pthread_t               mPlayerThread;
    bool                    mThread_sistarted;
	PacketQueue*            mVideoQueue;
    bool                    mHaveSeek;
    int64_t                 GetNowUs();
    int64_t                 hlsBaseTimeProcess(int64_t pts);

    Condition               mResetCondition;
    wp<MediaPlayerBase>     mListener;

    sp<ANativeWindow>       mNativeWindow;

    sp<MediaPlayerBase::AudioSink> mAudioSink;

    void *			            mMovieFile;
    RkFFStrmSelect_t            mSelect;
    DecoderAudio*				mDecoderAudio;
	DecoderVideo*             	mDecoderVideo;
    DecoderSubtitle*            mDecoderSub;

	int32_t                     mColorFormat;
    int32_t                     mCurSubFrmDurMs;
    int32_t                     mVideoScalingMode;

    int64_t mLatencyUs;

    size_t mFrameSize;

    void*                       mCookie;
    PlayerStatus                mCurrentState;
    int64_t                     mDurationUs;
    int                         mCurrentPosition;
    int                         mSeekPosition;
    bool                        mPrepareSync;
    status_t                    mPrepareStatus;
    int                         mStreamType;
    bool                        mLoop;
    RkFFSeekFlushInfo_t         mAudioFluInfo;
    float                       mLeftVolume;
    float                       mRightVolume;
    int                         mVideoWidth;
    int                         mVideoHeight;
    int                         mRkHwdecFlag;
    RkRenderer                  *mRender;
    Mutex                       mLock;
    RkAudioPlayer               *mAudioPlayer;

    uint32_t mFlags;
    int64_t  mStartedRealTime;
    int64_t  mPreBufferUpdateTimeUs;
    int64_t mVideoTimeUs;

    bool mSeekNotificationSent;
    int64_t mSeekTimeUs;
    int64_t mPreSeekTimeUs;
    int64_t mPreSeekSysTimeUs;
    bool isHttpFlag;
    bool isHlsFlag;
    bool mCanSeek;

    int32_t mAudioNum;
    int32_t mVideoNum;
    int64_t mAudioLastTimeUs;
    int64_t mVideoLastTimeUs;
    int64_t mHlsBaseTimeUs;
    int64_t mVideoBaseTimeUs;
    int32_t mContinueDisCard;
    String8 mUri;
    bool isQvm;
    int32_t videoFrameRate;
    bool mAudioDis;
    String8 mUriHeaders;//ht for mutil-headers
    FF_MediaPlayer(const FF_MediaPlayer &);
    FF_MediaPlayer &operator=(const FF_MediaPlayer &);
};
}
#endif // FFMPEG_MEDIAPLAYER_H
