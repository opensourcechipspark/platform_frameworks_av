#ifndef REPEATER_SOURCE_H_

#define REPEATER_SOURCE_H_

#include <media/stagefright/foundation/ABase.h>
#include <media/stagefright/foundation/AHandlerReflector.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MediaBuffer.h>
#include "vpu_mem.h"

#define SUSPEND_VIDEO_IF_IDLE   0  

namespace android {

// This MediaSource delivers frames at a constant rate by repeating buffers
// if necessary.
struct RepeaterSource : public MediaSource,
                    public MediaBufferObserver{
    RepeaterSource(const sp<MediaSource> &source, double rateHz,int width,int height);

    virtual status_t start(MetaData *params);
    virtual status_t stop();
    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

    void onMessageReceived(const sp<AMessage> &msg);

    // If RepeaterSource is currently dormant, because SurfaceFlinger didn't
    // send updates in a while, this is its wakeup call.
    void wakeUp();

    double getFrameRate() const;
    void setFrameRate(double rateHz);
 // The call for the StageFrightRecorder to tell us that
    // it is done using the MediaBuffer data so that its state
    // can be set to FREE for dequeuing
    virtual void signalBufferReturned(MediaBuffer* buffer);
protected:
    virtual ~RepeaterSource();

private:
    enum {
        kWhatRead,
    };

    Mutex mLock;
    Condition mCondition;

    bool mStarted;

    sp<MediaSource> mSource;
    double mRateHz;

    sp<ALooper> mLooper;
    sp<AHandlerReflector<RepeaterSource> > mReflector;

    MediaBuffer *mBuffer;
    status_t mResult;
    int64_t mLastBufferUpdateUs;
	int32_t rga_fd; 
	int		mWidth;
	int		mHeight;
#if	1//def	FOR_TCL
	int		mSrcWidth;
	int		mSrcHeight;
#endif
  	VPUMemLinear_t 	*vpuenc_mem[2];
	int		vpu_mem_index;
    int64_t mStartTimeUs;
    int32_t mFrameCount;
	int32_t mRepeat_time;
	int64_t mLast_TimeUs;
	int64_t mLast_SysTime;
	int64_t	mCurTimeUs;
	int64_t mUsingTimeUs;
	int		mUsingHdcp;
	int		mNumPendingBuffers;
	int		maxbuffercount;
	Condition mMediaBuffersAvailableCondition;
    void postRead();

    DISALLOW_EVIL_CONSTRUCTORS(RepeaterSource);
};

}  // namespace android

#endif // REPEATER_SOURCE_H_
