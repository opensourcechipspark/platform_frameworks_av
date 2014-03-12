/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HEVC_DECODER_H_

#define HEVC_DECODER_H_

#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaSource.h>
#include <utils/Vector.h>
#include <utils/List.h>



namespace android {

typedef struct _rkhevc_api {
    int (*init)(void **hevc_obj,int nb_pthreads);
    int (*decode)(void *hevc_obj,unsigned char *buff, int nal_len);
    int (*getDecinfo)(void *hevc_obj,unsigned int *width,
                   unsigned int *height, unsigned int *stride);
    int (*getFrame)(void *hevc_obj,int got_picture, unsigned char **Y,
                    unsigned char **U, unsigned char **V);

    void (*setCheckMD5)(void *hevc_obj,int val);
    void (*decClose)(void *hevc_obj);
    void (*flush)(void *hevc_obj);
}rkhevc_api;


struct HEVCDecoder : public MediaSource,
                    public MediaBufferObserver {
    HEVCDecoder(const sp<MediaSource> &source);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

    virtual void signalBufferReturned(MediaBuffer *buffer);

    int64_t getConsumeStream();

protected:
    virtual ~HEVCDecoder();

private:
    void *mHevcHandle;
    sp<MediaSource> mSource;
    bool mStarted;
    List<int64_t> mTimestamps;
    int64_t lastTimestamps;
    sp<MetaData> mFormat;
    MediaBuffer *mInputBuffer;
    int64_t mAnchorTimeUs;
    int64_t mNumFramesOutput;
    int64_t mPendingSeekTimeUs;
    int64_t outputTime;
    MediaSource::ReadOptions::SeekMode mPendingSeekMode;
    int64_t mTargetTimeUs;
	int32_t mWidth;
	int32_t mHeight;
	uint32_t mYuvLen;
    int32_t  thread_count;
    bool _success;
#ifdef DUP_HEVC
    FILE *fp;
#endif
    rkhevc_api *mHevcapi;
    void *mHevc_lib_handle;

    int cpu_fd;
    int cpuscale_fd;
    int64_t mTotalByte;
    HEVCDecoder(const HEVCDecoder &);
    HEVCDecoder &operator=(const HEVCDecoder &);
};

}  // namespace android

#endif  // HEVC_DECODER_H_
