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

//#define LOG_NDEBUG 0
#define LOG_TAG "HEVCDecoder"
#include <utils/Log.h>
#include <dlfcn.h>  // for dlopen/dlclose

//#undef ALOGD
//#define ALOGD


#include <OMX_Component.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include <media/stagefright/foundation/hexdump.h>
#include <HEVCDecoder.h>
#include "vpu_global.h"
#include <fcntl.h>

#define MAX_STREAM_LENGHT 1024*500


namespace android {

int64_t HEVCDecoder::getConsumeStream()
{
    return mTotalByte;
}


HEVCDecoder::HEVCDecoder(const sp<MediaSource> &source)
     :mSource(source),
      mStarted(false),
      mInputBuffer(NULL),
      mAnchorTimeUs(0),
      mNumFramesOutput(0),
      mPendingSeekTimeUs(-1),
      mPendingSeekMode(MediaSource::ReadOptions::SEEK_CLOSEST_SYNC),
      mTargetTimeUs(-1),
      cpu_fd(-1),
      cpuscale_fd(-1),
      mWidth(0),
      mHeight(0),
      mYuvLen(0),
      mTotalByte(0),
      _success(true){
    ALOGV("HEVCDecoder::HEVCDecoder in");
    mFormat = new MetaData;
    int32_t deInt = 1;
    mFormat->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_RAW);

    CHECK(mSource->getFormat()->findInt32(kKeyWidth, &mWidth));
    CHECK(mSource->getFormat()->findInt32(kKeyHeight, &mHeight));

    mFormat->setInt32(kKeyWidth, mWidth);
    mFormat->setInt32(kKeyHeight, mHeight);
    mFormat->setInt32(kKeyDeIntrelace, deInt);

	mYuvLen = ((mWidth+15)&(~15))* ((mHeight+15)&(~15))*3/2;

	mFormat->setInt32(kKeyColorFormat, OMX_COLOR_FormatYUV420Planar);

    mFormat->setCString(kKeyDecoderComponent, "HEVCDecoder");

    mTimestamps.clear();

    lastTimestamps = 0;

    int64_t durationUs;
    if (mSource->getFormat()->findInt64(kKeyDuration, &durationUs)) {
        mFormat->setInt64(kKeyDuration, durationUs);
    }

    mHevcHandle = NULL;
    mHevc_lib_handle = NULL;
    mHevcapi = NULL;

    outputTime = 0;
    cpu_fd = open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", O_WRONLY);
    if(cpu_fd > 0){
    	char para[64]={0};
		int paraLen = 0;
        paraLen = sprintf(para, "userspace");
        write(cpu_fd,para,paraLen);
    }else{
        ALOGE("cpu frep open fail");
    }

    cpuscale_fd = open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed", O_WRONLY);
    if(cpuscale_fd){
        char para[64]={0};
		int paraLen = 0;
        int scale = 1400000;
        paraLen = sprintf(para, "%d",scale);
        write(cpuscale_fd,para,paraLen);

    }else{
        ALOGE("cpu cpuscale_fd open fail");
    }
#ifdef DUP_HEVC
    fp = NULL;
    fp = fopen("data/video/hevc.bin","wb");
#endif
	ALOGV("HEVCDecoder::HEVCDecoder out");
}

HEVCDecoder::~HEVCDecoder() {

    if (mStarted) {
        stop();
    }
    if(cpu_fd > 0){
        char para[64]={0};
		int paraLen = 0;
        paraLen = sprintf(para,"interactive");
        write(cpu_fd,para,paraLen);
        close(cpu_fd);
        cpu_fd = -1;
    }

    if(cpuscale_fd > 0){
       close(cpuscale_fd);
       cpuscale_fd = -1;
    }

    if(mHevc_lib_handle){
        dlclose(mHevc_lib_handle);
    }
 #ifdef DUP_HEVC
    fclose(fp);
 #endif

}

status_t HEVCDecoder::start(MetaData *) {
    CHECK(!mStarted);
	ALOGV("HEVCDecoder::start in");

    mHevcapi = (rkhevc_api*)malloc(sizeof(rkhevc_api));

    if(mHevcapi == NULL){
        return !OK;
    }

    mHevc_lib_handle = dlopen("/system/lib/libhevcdec.so", RTLD_LAZY);
    if (mHevc_lib_handle == NULL) {
        ALOGE("dlopen hevc library failure\n");
        return !OK;
    }

    mHevcapi->init= (int (*)(void **hevc_obj,int nb_pthreads))dlsym(mHevc_lib_handle, "libOpenHevcInit");

    mHevcapi->decode= (int (*)(void *hevc_obj,
                                unsigned char *buff,
                                int nal_len))dlsym(mHevc_lib_handle, "libOpenHevcDecode");

    mHevcapi->getDecinfo = (int (*)(void *hevc_obj,
                                    unsigned int *width,
                                    unsigned int *height,
                                    unsigned int *stride ))dlsym(mHevc_lib_handle, "libOpenHevcGetPictureSize2");

    mHevcapi->getFrame= (int (*)(void *hevc_obj,
                                 int got_picture,
                                 unsigned char **Y,
                                 unsigned char **U,
                                 unsigned char **V ))dlsym(mHevc_lib_handle, "libOpenHevcGetOutput");

    mHevcapi->setCheckMD5= (void (*)(void *hevc_obj,int val))dlsym(mHevc_lib_handle, "libOpenHevcSetCheckMD5");

    mHevcapi->decClose= (void (*)(void *hevc_obj))dlsym(mHevc_lib_handle, "libOpenHevcClose");

    mHevcapi->flush= (void (*)(void *hevc_obj))dlsym(mHevc_lib_handle, "libOpenHevcFlush");


    uint32_t type;
    const void *data;
    size_t size;
    sp<MetaData> meta = mSource->getFormat();
    if(!_success)
		return UNKNOWN_ERROR;

    mSource->start();
    mAnchorTimeUs = 0;
    mNumFramesOutput = 0;
    mPendingSeekTimeUs = -1;
    mPendingSeekMode = ReadOptions::SEEK_CLOSEST_SYNC;
    mTargetTimeUs = -1;
    mStarted = true;
    thread_count = 1;
	ALOGV("HEVCDecoder::start out ");


    if ((meta->findData(kKeyHVCC, &type, &data, &size)) && size) {
        // Parse the AVCDecoderConfigurationRecord
        thread_count = sysconf(_SC_NPROCESSORS_ONLN);

        ALOGI("set thread_count %d",thread_count);

        mHevcapi->init(&mHevcHandle,thread_count);
        if(mHevcHandle == NULL){
            ALOGE("get hevc decoder fail");
            return UNKNOWN_ERROR;
        }

        mHevcapi->setCheckMD5(mHevcHandle,0);

        uint8_t *ptr = (uint8_t *)data;

        CHECK(size >= 7);
        uint8_t *tp = ptr;
        CHECK_EQ(ptr[0], 1);  // configurationVersion == 1

        CHECK(size >= 22);
        CHECK_EQ((unsigned)ptr[0], 1u);  // configurationVersion == 1

        uint8_t count = ptr[22];
        int got_picture = 0;
        ptr += 23;
        size -= 23;
        for(int i= 0; i < count; i++){
            ptr += 3;
            size -= 3;
            size_t length = U16_AT(ptr);
            ptr += 2;
            size -= 2;
            ALOGI("length = %d",length);
#ifdef DUP_HEVC
            uint32_t Nal = 0x01000000;
            if(fp){
                fwrite(&Nal,1,4,fp);
                fwrite(ptr,1,length,fp);
                fflush(fp);
            }
#endif
            got_picture = mHevcapi->decode(mHevcHandle,ptr,length);
            ptr += length;
            size -= length;
        }
    }else{
         mHevcapi->init(&mHevcHandle,1);
        if(mHevcHandle == NULL){
            ALOGE("get hevc decoder fail");
            return UNKNOWN_ERROR;
        }
        mHevcapi->setCheckMD5(mHevcHandle,0);
    }
    return OK;
}


status_t HEVCDecoder::stop() {
    CHECK(mStarted);

    if (mInputBuffer) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    mSource->stop();

    mStarted = false;

    mTimestamps.clear();
    ALOGI("HEVCDecoder stop");
    mHevcapi->flush(mHevcHandle);
    mHevcapi->decClose(mHevcHandle);
    if(mHevcapi){
        free(mHevcapi);
        mHevcapi = NULL;
    }
    return OK;
}

sp<MetaData> HEVCDecoder::getFormat() {
    return mFormat;
}

static int find_start_code (unsigned char *Buf, int zeros_in_startcode)
{
    int i;
    for (i = 0; i < zeros_in_startcode; i++)
        if(Buf[i] != 0)
            return 0;
    return Buf[i];
}

static int get_next_nal(FILE* inpf, unsigned char* Buf)
{
    int pos = 0;
    int StartCodeFound = 0;
    int info2 = 0;
    int info3 = 0;
    while(!feof(inpf)&&(/*Buf[pos++]=*/fgetc(inpf))==0);

    while (pos < 3) Buf[pos++] = fgetc (inpf);
    while (!StartCodeFound)
    {
        if (feof (inpf))
        {
            //            return -1;
            return pos-1;
        }
        Buf[pos++] = fgetc (inpf);
        info3 = find_start_code(&Buf[pos-4], 3);
        if(info3 != 1)
            info2 = find_start_code(&Buf[pos-3], 2);
        StartCodeFound = (info2 == 1 || info3 == 1);
    }
    fseek (inpf, - 4 + info2, SEEK_CUR);
    return pos - 4 + info2;
}

status_t HEVCDecoder::read(
        MediaBuffer **out, const ReadOptions *options) {
    *out = NULL;
	ALOGV("HEVCDecoder::read in");

    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        ALOGV("seek requested to %lld us (%.2f secs)", seekTimeUs, seekTimeUs / 1E6);

        CHECK(seekTimeUs >= 0);
        mPendingSeekTimeUs = seekTimeUs;
        mPendingSeekMode = mode;

        if (mInputBuffer) {
            mInputBuffer->release();
            mInputBuffer = NULL;
        }
        mHevcapi->flush(mHevcHandle);
        mTimestamps.clear();
    }

   if (mInputBuffer == NULL) {
        ALOGV("fetching new input buffer.");
        bool seeking = false;

            for (;;) {
                if (mPendingSeekTimeUs >= 0) {
                    ALOGV("reading data from timestamp %lld (%.2f secs)",
                         mPendingSeekTimeUs, mPendingSeekTimeUs / 1E6);
                }

                ReadOptions seekOptions;
                if (mPendingSeekTimeUs >= 0) {
                    seeking = true;
                    seekOptions.setSeekTo(mPendingSeekTimeUs, mPendingSeekMode);
                    mPendingSeekTimeUs = -1;
                }


                status_t err = mSource->read(&mInputBuffer, &seekOptions);

                seekOptions.clearSeekTo();

                if (err != OK) {

                    *out = NULL;
                    return (*out == NULL)  ? err : (status_t)OK;
                }

                if (mInputBuffer->range_length() > 0) {
                    mTotalByte += mInputBuffer->range_length();
                    break;
                }

                mInputBuffer->release();
                mInputBuffer = NULL;
            }

        if (seeking) {
            int64_t targetTimeUs;
            if (mInputBuffer->meta_data()->findInt64(kKeyTargetTime, &targetTimeUs)
                    && targetTimeUs >= 0) {
                mTargetTimeUs = targetTimeUs;
            } else {
                mTargetTimeUs = -1;
            }
             int64_t inputTime = 0;
             mInputBuffer->meta_data()->findInt64(kKeyTime, &inputTime);
             outputTime = inputTime;
        }
    }

	uint32_t aOutputLength = 0;
	uint8_t * pInput = (uint8_t *)mInputBuffer->data();
	uint32_t aInBufSize = mInputBuffer->range_length();
    int64_t inputTime = 0;
    mInputBuffer->meta_data()->findInt64(kKeyTime, &inputTime);
///	int64_t outputTime = 0LL;
    int got_picture = 0;
    uint8_t *Y, *U, *V;
    uint32_t height,width,stride;

    if(inputTime != lastTimestamps)
    {
 //       ALOGI("push time %lld",inputTime);
        mTimestamps.push_back(inputTime);
        lastTimestamps = inputTime;
    }

    // ALOGI("get buff size %d",aInBufSize);
 //   fwrite(pInput,1,aInBufSize,fp);
 //   fflush(fp);
#ifdef DUP_HEVC
            uint32_t Nal = 0x01000000;
            if(fp){
                fwrite(&Nal,1,4,fp);
                fwrite(pInput,1,aInBufSize,fp);
                fflush(fp);
            }
#endif
    got_picture = mHevcapi->decode(mHevcHandle,pInput,aInBufSize);

    mHevcapi->getFrame(mHevcHandle,got_picture, &Y, &U, &V);

    if (got_picture != 0) {
        mHevcapi->getDecinfo(mHevcHandle,&width, &height, &stride);
        uint32_t picWidth = stride - 32;
        picWidth = (picWidth +31) & (~31);
        int32_t oldWidth,oldHeight;
        if((mFormat->findInt32(kKeyWidth, &oldWidth)) &&
          (mFormat->findInt32(kKeyHeight, &oldHeight))) {
            if ((oldWidth != picWidth) ||
                    (oldHeight != height)) {
               mFormat->setInt32(kKeyWidth, picWidth);
               mFormat->setInt32(kKeyHeight, height);
               mWidth = picWidth;
               mHeight = height;
               ALOGE("INFO_FORMAT_CHANGED found");
                if(mInputBuffer)
            	{
            		mInputBuffer->release();
            		mInputBuffer = NULL;
            	}
               return INFO_FORMAT_CHANGED;
            }
        }
    }

    if(mInputBuffer)
	{
		mInputBuffer->release();
		mInputBuffer = NULL;
	}
    MediaBuffer *aOutBuf = new MediaBuffer(mWidth*mHeight*3/2);
	uint8_t * data = (uint8_t *)aOutBuf->data();

	if(got_picture) {
    /*    if(mTimestamps.size())
        {
            outputTime = *mTimestamps.begin();
            mTimestamps.erase(mTimestamps.begin());
  //          ALOGI("pop time %lld",outputTime);
        }*/
        uint8_t* pYDstBuf = (uint8_t*)data;
        uint8_t* pYSrcBuf = (uint8_t*)Y;

        uint8_t* pUDstBuf = (uint8_t*)data + mWidth*mHeight;
        uint8_t* pUSrcBuf = (uint8_t*)V;

        uint8_t* pVDstBuf = (uint8_t*)data+ mWidth*mHeight*5/4;
        uint8_t* pVSrcBuf = (uint8_t*)U;

        for (int k =0; k < mHeight; k++) {
            memcpy(pYDstBuf, pYSrcBuf, mWidth);
            pYDstBuf +=mWidth;
            pYSrcBuf +=stride;
        }

        for (int k =0; k <mHeight/2; k++) {
            memcpy(pUDstBuf, pUSrcBuf, mWidth/2);
            pUDstBuf +=mWidth/2;
            pUSrcBuf +=stride/2;

            memcpy(pVDstBuf, pVSrcBuf, mWidth/2);
            pVDstBuf +=mWidth/2;
            pVSrcBuf +=stride/2;
        }

        outputTime += 41667;
        aOutBuf->meta_data()->setInt64(kKeyTime,outputTime);

	}

	if(!got_picture)
		aOutBuf->set_range(0, 0);

	*out = aOutBuf;
	ALOGV("HEVCDecoder::read out");
	return OK;
}

void HEVCDecoder::signalBufferReturned(MediaBuffer *buffer) {

}

}  // namespace android
