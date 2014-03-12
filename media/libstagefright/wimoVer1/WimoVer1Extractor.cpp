/*
 * Copyright (C) 2010 The Android Open Source Project
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

#define LOG_TAG "WimoVer1Extractor"
#include <utils/Log.h>

#include "WimoVer1Extractor.h"

#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>
#include <utils/String8.h>

#define WIMO_VER1_EXTRACTOR_DEBUG 1
#define WIMO_VER1_EXTRACTOR_WRITE_DATA 0

#if WIMO_VER1_EXTRACTOR_DEBUG
#define WIMOV1_LOG ALOGD
#else
#define WIMOV1_LOG
#endif

#if WIMO_VER1_EXTRACTOR_WRITE_DATA
static FILE* pFile = NULL;
#endif


namespace android {

struct WimoVer1DataSourceReader : public IWimoVer1Reader {
    WimoVer1DataSourceReader(const sp<DataSource> &source)
        : mSource(source) {
    }

    virtual int Read(long long position, long length, unsigned char* buffer) {
        CHECK(position >= 0);
        CHECK(length >= 0);

        if (length == 0) {
            return 0;
        }

        ssize_t n = mSource->readAt(position, buffer, length);

        if (n <= 0) {
            ALOGI("wimover1 extractor readAt: %lld, %ld bytes fail, ret n: %d",
                position, length, n);
            return -1;
        }

        return 0;
    }

    virtual int Length(long long* total, long long* available) {
        off64_t size;
        if (mSource->getSize(&size) != OK) {
            return -1;
        }

        if (total) {
            *total = 0x7FFFFFFF;
        }

        if (available) {
            *available = size;
        }

        return 0;
    }

	virtual void updatecache(long long off)
	{
		mSource->updatecache(off);
	}
private:
    sp<DataSource> mSource;

    WimoVer1DataSourceReader(const WimoVer1DataSourceReader &);
    WimoVer1DataSourceReader &operator=(const WimoVer1DataSourceReader &);
};

////////////////////////////////////////////////////////////////////////////////


struct WimoVer1Source : public MediaSource {
    WimoVer1Source(
            const sp<WimoVer1Extractor> &extractor, size_t index);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

private:
    enum Type {
	    WAV,
        MJPEG,
        OTHER
    };

    sp<WimoVer1Extractor> mExtractor;
    size_t mTrackIndex;
    Type mType;
    uint32 mTrackId;

    WimoVer1Source(const WimoVer1Source &);
    WimoVer1Source &operator=(const WimoVer1Source &);
};

WimoVer1Source::WimoVer1Source(
        const sp<WimoVer1Extractor> &extractor, size_t index)
    : mExtractor(extractor),
      mTrackIndex(index),
      mType(OTHER),
      mTrackId(0) {
    WIMOV1_LOG("WimoVer1Source construction in");
    const char *mime;
    CHECK(mExtractor->mTracks.itemAt(index).mMeta->
            findCString(kKeyMIMEType, &mime));

    if (!strcasecmp(mime, MEDIA_MIMETYPE_VIDEO_MJPEG)) {
        mType = MJPEG;
    }else if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_WAV)) {
        mType = WAV;
    }

    mTrackId = mExtractor->mTracks.itemAt(index).mtrackID;
    WIMOV1_LOG("WimoVer1Source construction out, mType = %d, mTrackId = %d", mType, mTrackId);
}

status_t WimoVer1Source::start(MetaData *params) {
    WIMOV1_LOG("start---->, mType: %d, trackIdx: %d", mType, mTrackIndex);

    if (mType ==MJPEG) {
        mExtractor->wimoVer1Dmx->wimo_start();
    }

    return OK;
}

status_t WimoVer1Source::stop() {
    WIMOV1_LOG("stop---->, mType: %d", mType);
    if (mType !=MJPEG) {
        mExtractor->wimoVer1Dmx->wimo_pause();
    } else {
        mExtractor->wimoVer1Dmx->wimo_stop();
    }

    return OK;
}

sp<MetaData> WimoVer1Source::getFormat() {
    return mExtractor->mTracks.itemAt(mTrackIndex).mMeta;
}

status_t WimoVer1Source::read(
        MediaBuffer **out, const ReadOptions *options) {

    //WIMOV1_LOG("WimoVer1Source::read in, mType = %d, mTrackId = %d",(int)mType, mTrackId);
    *out = NULL;
    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    int ret = 0;
    //read seek process
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {

        seekTimeUs = seekTimeUs >0 ? seekTimeUs : 0;

        //seek process
    }

    /* read packet */
    ret =0;

    if((mType == MJPEG) || (mType == WAV)) {
        ret = mExtractor->wimoVer1Dmx->wimo_readFrm(mTrackId, out);

        if((mType == MJPEG) && (ret == WIMO_READ_FRAME_SUCCESS)) {

            int64_t frameTs = 0;
            int frameSize = 0;
            (*out)->meta_data()->findInt64(kKeyTime, &frameTs);
            frameSize = (*out)->range_length();

#if WIMO_VER1_EXTRACTOR_WRITE_DATA
            if (pFile && frameSize) {
                fwrite((uint8_t*)((*out)->data()), 1, frameSize, pFile);
                fflush(pFile);
            }
#endif
            /*WIMOV1_LOG("read one video frame, size: %d, ts: %d",
                frameSize, frameTs);*/
        } else if (ret == INFO_DISCONTINUITY) {
            return INFO_DISCONTINUITY;
        }

    } else {
    	ALOGE("unsupported codec id, error");
		return ERROR_UNSUPPORTED;
    }

    if((ret == WIMO_READ_FRAME_END_OF_STREAM) || (ret == WIMO_READ_FRAME_FAIL))    //end of stream
    {
        WIMOV1_LOG("stream of mType = %d reach end of track", mType);
        return ERROR_END_OF_STREAM;
    }

    return OK;
}

////////////////////////////////////////////////////////////////////////////////

WimoVer1Extractor::WimoVer1Extractor(const sp<DataSource> &source)
    : mDataSource(source),
      mExtractedThumbnails(false) {

    WIMOV1_LOG("WimoVer1Extractor construct in");
    //initialization
    wimoVer1Dmx = NULL;
    fileHandle = NULL;
    wimoVer1PCMExtraSize = 0;
    oscl_memset(wimoVer1PCMExtraData, 0, WIMOVER1_MAX_AUDIO_WAVFMT_SIZE);
    oscl_memset(&sWimoVer1Format, 0, sizeof(WimoVer1Format));
    oscl_memset(&sSeekInfo, 0, sizeof(WimoVer1SeekInfo));
    bWavCodecPrivateSend = false;
    bSuccess = true;

#if WIMO_VER1_EXTRACTOR_WRITE_DATA
    if (pFile) {
        fclose(pFile);
        pFile = NULL;
    }

    pFile = fopen("/sdcard/wimoJpeg", "wb+");

    if (pFile == NULL) {
        ALOGI("can not open write file for wimover1Extractor");
    }
#endif

    // create wimo file handle
    mReaderHandle = new WimoVer1DataSourceReader(mDataSource);
    CHECK(mReaderHandle != NULL);

    fileHandle = new WIMO_VER1_FF_FILE(mReaderHandle);
    CHECK(fileHandle != NULL);
    WIMOV1_LOG("create file handle success");

    // create wimo file demux
    wimoVer1Dmx = new WimoVer1Demux(fileHandle);
    CHECK(wimoVer1Dmx != NULL);
    WIMOV1_LOG("create wimoVer1Dmx success");

    // read header
    if (0 > wimoVer1Dmx->wimo_read_header(fileHandle))
    {
	    WIMOV1_LOG("read header fail");
        bSuccess = false;
    	return;
    }

    sWimoVer1Format.iVideoWidth = (uint16)wimoVer1Dmx->get_videoWidth();
    sWimoVer1Format.iVideoHeight = (uint16)wimoVer1Dmx->get_videoHeight();

    WIMOV1_LOG("video width = %d, height = %d",
        sWimoVer1Format.iVideoWidth, sWimoVer1Format.iVideoHeight);

    if((sWimoVer1Format.iVideoWidth == 0) || (sWimoVer1Format.iVideoHeight == 0)) {
        ALOGE("video width or height error");
        bSuccess = false;
        return;
    }
    addTracks();
    WIMOV1_LOG("WimoVer1Extractor construct out success");
}

WimoVer1Extractor::~WimoVer1Extractor() {
    WIMOV1_LOG("WimoVer1Extractor deconstruct in");
    if(mReaderHandle) {
        delete mReaderHandle;
        mReaderHandle = NULL;
    }

    if(fileHandle) {
        delete fileHandle;
        fileHandle = NULL;
    }

    if(wimoVer1Dmx) {
        wimoVer1Dmx->wimo_read_close();
        delete wimoVer1Dmx;
        wimoVer1Dmx = NULL;
    }

#if WIMO_VER1_EXTRACTOR_WRITE_DATA
    if(pFile) {
        fclose(pFile);
        pFile = NULL;
    }
#endif
}

size_t WimoVer1Extractor::countTracks() {

    if(!bSuccess)
        return 0;

    WIMOV1_LOG("countTracks in, there are %d track(s)", mTracks.size());
    return mTracks.size();
}

sp<MediaSource> WimoVer1Extractor::getTrack(size_t index) {
    WIMOV1_LOG("getTrack in");
    if ((!bSuccess) || (index >= mTracks.size())) {
        return NULL;
    }

    return new WimoVer1Source(this, index);
}

sp<MetaData> WimoVer1Extractor::getTrackMetaData(
        size_t index, uint32 flags) {
    WIMOV1_LOG("getTrackMetaData in");
    if (index >= mTracks.size()) {
        return NULL;
    }

    if ((flags & kIncludeExtensiveMetaData) && !mExtractedThumbnails) {
        findThumbnails();
        mExtractedThumbnails = true;
    }

    return mTracks.itemAt(index).mMeta;
}

void WimoVer1Extractor::addTracks() {
    uint32 iIdList[16];
    int iNumTracks = 0;
    int i =0;

    iNumTracks =wimoVer1Dmx->get_ID_list(iIdList, iNumTracks);
    if (iNumTracks == 0) {
        ALOGE("error: no tracks in this mpg file");
        return;
    }

    for (i = 0; i <iNumTracks; i++)
    {
        uint32 id = iIdList[i];

    	size_t codecPrivateSize;
    	unsigned char *codecPrivate = NULL;

    	codecPrivateSize = 0;
    	codecPrivate = NULL;

    	sp<MetaData> meta = new MetaData;
    	WIMOV1_LOG("addTracks process stream id = 0x%X",id);

    	if (id == WIMO_VER1_CODEC_ID_MJPEG)	//MJPEG
    	{
    		meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_MJPEG);
        	meta->setInt32(kKeyWidth, 	sWimoVer1Format.iVideoWidth);
        	meta->setInt32(kKeyHeight, sWimoVer1Format.iVideoHeight);
    	} else if(id == WIMO_VER1_CODEC_ID_WAV) {
            /*
             ** wav. note: we add wav codec private data to the first audio packet,
             ** then send to wav decoder.
            */
    		meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_WAV);
    		meta->setInt32(kKeySampleRate,  wimoVer1Dmx->m_wavehdr.SamplesPerSec);
        	meta->setInt32(kKeyChannelCount, wimoVer1Dmx->m_wavehdr.Channels);

    		WIMOV1_LOG("wav audio, sampleRate = %d, channelCnt = %d",
    			wimoVer1Dmx->m_wavehdr.SamplesPerSec, wimoVer1Dmx->m_wavehdr.Channels);
    	} else {
            meta->setCString(kKeyMIMEType, "unsupported codecId");
    	}

    	long long duratioMs = wimoVer1Dmx->get_duration();
    	meta->setInt64(kKeyDuration, duratioMs * 1000);
    	WIMOV1_LOG("track duration is %lld(ms)", duratioMs);

    	mTracks.push();
    	TrackInfo *trackInfo = &mTracks.editItemAt(mTracks.size() - 1);
    	trackInfo->mtrackID = id;
    	trackInfo->mMeta = meta;
    }
}

void WimoVer1Extractor::findThumbnails() {
    WIMOV1_LOG("findThumbnails in");
    return;
}

sp<MetaData> WimoVer1Extractor::getMetaData() {
    WIMOV1_LOG("getMetaData in");
    sp<MetaData> meta = new MetaData;
    meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_CONTAINER_WIMO_VER1);

    return meta;
}

}  // namespace android
