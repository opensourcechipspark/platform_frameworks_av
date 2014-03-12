#include "WimoVer1demux.h"

#include <utils/Log.h>
#include <media/stagefright/MediaErrors.h>
#undef LOG_TAG
#define LOG_TAG "WimoVer1demux"

#define WIMO_READ_FILE_BUFFER_SIZE 2048
#define WIMO_VIDEO_STREAM_BUFFER_SIZE 0xC0000
#define WIMO_AUDIO_STREAM_BUFFER_SIZE 0x2000

#define WIMO_RK_AUDIO_HEAD  0x524B4142   //RKAB
#define WIMO_RK_VIDEO_HEAD  0x524B5642   //RKVB

#define WIMO_HEAD_FLAG    0x41414141      //"AAAA"
#define WIMO_STREAM_FLAG  0x42424242      //"BBBB"

#define WIMO_LOW_WATER_MARK_BYTES 2048
#define WIMO_LOW_WATER_SLEEP_US 100

#define WIMO_WAIT_DATA_TIMEOUT_TIME_US 30000000LL

#define HAVE_WIMO_VIDEO_FRAME_VALID_CHECK 1

#define WIMO_VER1_DEMUX_DEBUG 1
#define WIMO_VER1_WRITE_DATA_DEBUG 0
#define WIMO_VER1_USE_TEST_FILE 0

#if WIMO_VER1_WRITE_DATA_DEBUG
static FILE* pFile = NULL;
#endif

#if WIMO_VER1_USE_TEST_FILE
static FILE* pWimoRecTest = NULL;
static long recTestFileSize = 0;
#endif


#if WIMO_VER1_DEMUX_DEBUG
#define WIMOMUX_LOG ALOGD
#else
#define WIMOMUX_LOG
#endif

static uint32_t iTestVideoFrmTimeMs = 0;

/** Returns a time value in milliseconds based on a clock starting at
 *  some arbitrary base. Given a call to GetTime that returns a value
 *  of n a subsequent call to GetTime made m milliseconds later should
 *  return a value of (approximately) (n+m). This method is used, for
 *  instance, to compute the duration of call. */
static int64_t GetSysTimeUs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (int64_t)tv.tv_sec * 1000000ll + tv.tv_usec;

}

namespace android {
	FILE* test_txt;

IWimoVer1Reader::~IWimoVer1Reader()
{
}

/*=====================================================*/
/*MPG_FF_FILE class*/
WIMO_VER1_FF_FILE::WIMO_VER1_FF_FILE(IWimoVer1Reader* pReader)
{
	long long total = 0;
	long long available = 0;
	int ret = 0;
	
	if(pReader == NULL)
	{
		ALOGE("wimo file construct, in put file is NULL, error");
		goto FAIL;
	}

	m_input = pReader;
	m_offset = 0;

    oscl_memset(&m_FileBuf, 0, sizeof(WimoReadFileBuffer));
    oscl_memset(&m_VideoBuf, 0, sizeof(WimoReadVideoBuf));
    oscl_memset(&m_SkipBuf, 0, sizeof(WimoSkipBuffer));

    m_FileBuf.buf = (uint8*)oscl_malloc(WIMO_READ_FILE_BUFFER_SIZE);

    if (m_FileBuf.buf == NULL) {
        ALOGE("malloc %d bytes buffer fail\n", WIMO_READ_FILE_BUFFER_SIZE);
        goto FAIL;
    } else {
        m_FileBuf.capability = WIMO_READ_FILE_BUFFER_SIZE;
    }

    m_VideoBuf.buf = (uint8*)oscl_malloc(WIMO_VIDEO_STREAM_BUFFER_SIZE);

    if (m_VideoBuf.buf == NULL) {
        ALOGE("malloc %d bytes buffer fail\n", WIMO_VIDEO_STREAM_BUFFER_SIZE);
        goto FAIL;
    } else {
        m_VideoBuf.capability = WIMO_VIDEO_STREAM_BUFFER_SIZE;
    }

    m_SkipBuf.buf = (uint8*)oscl_malloc(1024);

    if (m_SkipBuf.buf == NULL) {
        ALOGE("malloc 1024 bytes buffer fail\n");
        goto FAIL;
    } else {
        m_SkipBuf.capability = 1024;
    }

    /* set 0x7FFFFFFFFFFFFFFF as test default file size. */
	_fileSize = 0x7FFFFFFFFFFFFFFF;

    iTestVideoFrmTimeMs = 0;

	WIMOMUX_LOG("wimo file construct out, the size of this file = 0x%X", _fileSize);

    return;
FAIL:
    ALOGE("WIMO_VER1_FF_FILE construct fail");
}

WIMO_VER1_FF_FILE::~WIMO_VER1_FF_FILE()
{
    if (m_FileBuf.buf) {
        oscl_free(m_FileBuf.buf);
        m_FileBuf.buf = NULL;
    }

    if (m_VideoBuf.buf) {
        oscl_free(m_VideoBuf.buf);
        m_VideoBuf.buf = NULL;
    }

    if (m_SkipBuf.buf) {
        oscl_free(m_SkipBuf.buf);
        m_SkipBuf.buf = NULL;
    }
}

void WIMO_VER1_FF_FILE::setDemux(WimoVer1Demux* demux)
{
    m_demux = demux;
}

int WIMO_VER1_FF_FILE::Read(uint8 *buf, int size)
{
    int needRead = size;
    WimoUpdataErrorCode errUpdate = WIMO_UPDATE_FILE_BUF_OK;

    if ((size <=0) || (buf ==NULL)) {
        return 0;
    }

    while(m_FileBuf.remain <(uint32)size) {
        if (m_FileBuf.remain) {
            memcpy(buf, m_FileBuf.buf + m_FileBuf.offset, m_FileBuf.remain);
            buf +=m_FileBuf.remain;
            size -=m_FileBuf.remain;
            m_FileBuf.buf_off_in_file +=m_FileBuf.remain;
            m_FileBuf.remain = 0;
        }

        errUpdate = updateFileBuf();

        if (errUpdate == WIMO_UPDATE_FILE_END) {
            return 0;
        } else if (errUpdate == WIMO_UPDATE_FILE_BUF_FAIL){
            WIMOMUX_LOG("Read fail");
            return -1;
        }
    }

    if (size >0) {
        memcpy(buf, m_FileBuf.buf + m_FileBuf.offset, size);
        m_FileBuf.offset +=size;
        m_FileBuf.remain -=size;
        m_FileBuf.buf_off_in_file +=size;
	}

	return needRead;
}

uint64 WIMO_VER1_FF_FILE::Tell()
{
	return m_FileBuf.buf_off_in_file;
}

int WIMO_VER1_FF_FILE::Skip(int size)
{
    if ((size <0) || ((uint32)size >m_FileBuf.remain)) {
    	m_offset =m_FileBuf.buf_off_in_file + size;

    	if(m_offset >=_fileSize) {
            m_offset = _fileSize;
    	}

        m_FileBuf.fileOff = m_offset;
        m_FileBuf.buf_off_in_file = m_FileBuf.fileOff;
        m_FileBuf.offset = 0;
        m_FileBuf.remain = 0;

    } else {
        m_FileBuf.offset +=size;
        m_FileBuf.remain -=size;
        m_FileBuf.buf_off_in_file +=size;
    }

	return 0;
}

int WIMO_VER1_FF_FILE::isEOF()
{
    return (m_FileBuf.buf_off_in_file >=_fileSize);
}

inline int64_t WIMO_VER1_FF_FILE::getAvailableSize()
{
    long long total =0;
    long long available =0;
    int ret =0;

    ret = m_input->Length(&total, &available);
    if(ret)
    {
        return 0;
    }

    return available;
}

bool WIMO_VER1_FF_FILE::checkWimoBufDataReady()
{
    int64_t avaiSize = getAvailableSize();

    return avaiSize >WIMO_LOW_WATER_MARK_BYTES;
}

int WIMO_VER1_FF_FILE::Seek(long long ofs)
{
	if((ofs <0) || (ofs>=_fileSize)) {
        WIMOMUX_LOG("Seek fail");
		return -1;	//seek error
	} else if (ofs >=_fileSize) {
	    ofs = _fileSize;
	}

	m_offset =ofs;
    m_FileBuf.fileOff = m_offset;
    m_FileBuf.buf_off_in_file = m_FileBuf.fileOff;
    m_FileBuf.offset = 0;
    m_FileBuf.remain = 0;

	return 0;
}

uint8 WIMO_VER1_FF_FILE::ReadByte()
{
    uint8 result = 0;
    WimoUpdataErrorCode errUpdate = WIMO_UPDATE_FILE_BUF_OK;

    if (!m_FileBuf.remain) {
        errUpdate = updateFileBuf();

        if (errUpdate == WIMO_UPDATE_FILE_END) {
            /* set demux to stop */
            m_demux->setDemuxState(WIMO_STOP);
            return 0;
        } else if (errUpdate == WIMO_UPDATE_FILE_BUF_FAIL) {
            /* set demux to stop */
            m_demux->setDemuxState(WIMO_STOP);
            return -1;
        }
    }

    result = *(m_FileBuf.buf + m_FileBuf.offset);
    m_FileBuf.offset++;
    m_FileBuf.buf_off_in_file++;
    m_FileBuf.remain--;

	return result;
}

uint16 WIMO_VER1_FF_FILE::Read2Byte()
{
	uint16 tmp;

	Read((uint8*)&tmp,2);
	tmp = (tmp<<8)|(tmp>>8);
	return tmp;
}

uint32 WIMO_VER1_FF_FILE::Read3Byte()
{
	uint32 tmp;

	Read((uint8*)&tmp,3);
	tmp = ((tmp<<16)&0xff0000)|((tmp>>16)&0xff)|(tmp&0xff00);
	return tmp;
}

uint32 WIMO_VER1_FF_FILE::Read4Byte()
{
	uint32 tmp;

	Read((uint8*)&tmp,4);
	tmp = ((tmp<<24)&0xff000000)|((tmp>>8)&0xff00)|((tmp<<8)&0xff0000)|((tmp>>24)&0xff);
	return tmp;
}

int WIMO_VER1_FF_FILE::skipBytesByRead(int size) {
    if (size <=0) {
        return 0;
    }

    if (m_SkipBuf.capability <=size) {
        m_SkipBuf.buf = (uint8_t*)realloc((void*)m_SkipBuf.buf, size);

        if (m_SkipBuf.buf) {
           m_SkipBuf.capability = size;
        }
    } else {
        if (m_SkipBuf.buf) {
            Read(m_SkipBuf.buf, size);
        }
    }

    return size;
}

int WIMO_VER1_FF_FILE::storeVideoPktFromWimoData(int size)
{
    if (size <=0) {
        return 0;
    }

    int remain = m_VideoBuf.capability - m_VideoBuf.offset;
    int i =0;

    if (remain <=size) {
        m_VideoBuf.buf = (uint8_t*)realloc((void*)m_VideoBuf.buf, m_VideoBuf.capability <<1);

        if (m_VideoBuf.buf) {
            ALOGI("realloc video pkt buffer to %d bytes ok",
                m_VideoBuf.capability <<1);
            m_VideoBuf.capability = m_VideoBuf.capability <<1;
        } else {
            ALOGI("realloc video pkt buffer to %d bytes fail------->",
                m_VideoBuf.capability <<1);
        }
    }

    if (m_VideoBuf.buf) {
        uint32_t bufFree = m_VideoBuf.capability - m_VideoBuf.offset;
        if (bufFree <size) {
            ALOGI("warnning........ this packet will be skipped, may be error wimo packet.");
            return 0;
        }
        if (Read(m_VideoBuf.buf + m_VideoBuf.offset, size) !=size) {
            ALOGI("storeVideoPktFromWimoData read pkt fail, not get size: %d bytes", size);
            return 0;
        }

        if (size <=64) {
            /* skip zero padding bytes */
            uint8_t* pChkBuf = m_VideoBuf.buf + m_VideoBuf.offset;
            for (i =0; i <size -1; i++) {
                if ((*pChkBuf = 0xFF) && (*(pChkBuf+1) = 0xD9)) {
                    break;
                }
            }

            size = (i + 2)>0 ? (i+2) : size;
        }

        m_VideoBuf.offset +=size;
        m_VideoBuf.size +=size;
    }

    return size;
}

void WIMO_VER1_FF_FILE::resetBuffer()
{
    m_VideoBuf.size =0;
    m_VideoBuf.offset =0;
    m_VideoBuf.broken = 0;
}

void WIMO_VER1_FF_FILE::setBufferDataIsBroken()
{
    m_VideoBuf.broken = 1;
}

int WIMO_VER1_FF_FILE::getBrokenFlag()
{
    return m_VideoBuf.broken;
}

WimoUpdataErrorCode WIMO_VER1_FF_FILE::updateFileBuf()
{
   
	int64_t start_time = GetSysTimeUs();
	 while (getAvailableSize() == 0) {
        if ((GetSysTimeUs() - start_time) >WIMO_WAIT_DATA_TIMEOUT_TIME_US) {
            ALOGI("wait wimo data time out------------->>");
            m_demux->setDemuxState(WIMO_STOP);
            return WIMO_UPDATE_FILE_END;
        }
        if (m_demux->getDemuxState() == WIMO_STOP) {
            return WIMO_UPDATE_FILE_END;
        }
        usleep(WIMO_LOW_WATER_SLEEP_US);
    }
  //  if (fileRemain == 0) {
    //    return WIMO_UPDATE_FILE_END;
    //}
	 uint64 fileRemain = getAvailableSize();// - m_FileBuf.fileOff;
    uint32 canRead = 0;
	int ret =0;
    canRead = fileRemain > WIMO_READ_FILE_BUFFER_SIZE ? WIMO_READ_FILE_BUFFER_SIZE : fileRemain;

#if WIMO_VER1_USE_TEST_FILE
    if (pWimoRecTest == NULL) {
        return WIMO_UPDATE_FILE_END;
    }

    if (m_demux->getDemuxState() == WIMO_STOP) {
        return WIMO_UPDATE_FILE_END;
    }

    uint32 curPos = ftell(pWimoRecTest);
    uint32 recTestRemain = recTestFileSize - curPos;

    if (recTestRemain == 0) {
        fseek(pWimoRecTest, 0, SEEK_SET);
        recTestRemain = recTestFileSize;
        //return WIMO_UPDATE_FILE_END;
    }

    canRead = recTestRemain > WIMO_READ_FILE_BUFFER_SIZE ? WIMO_READ_FILE_BUFFER_SIZE : recTestRemain;

    ret = fread((void*)m_FileBuf.buf, 1, canRead, pWimoRecTest);
#else
    int64_t beginTimeUs = GetSysTimeUs();
    int64_t avaiSize = getAvailableSize();

    if (avaiSize <canRead) {
        if (m_demux->getDemuxState() != WIMO_STOP) {
            m_demux->setDemuxState(WIMO_INSUFFICIENT_DATA);
        }
    }


    /* current data is not enough, sleep for wait */
    while (avaiSize <canRead) {
        if ((GetSysTimeUs() - beginTimeUs) >WIMO_WAIT_DATA_TIMEOUT_TIME_US) {

            ALOGI("wait wimo data time out------------->>");

            m_demux->setDemuxState(WIMO_STOP);

            return WIMO_UPDATE_FILE_END;
        }

        if (m_demux->getDemuxState() == WIMO_STOP) {
            return WIMO_UPDATE_FILE_END;
        }

        usleep(WIMO_LOW_WATER_SLEEP_US);

        avaiSize = getAvailableSize();
    }

    ret = m_input->Read(m_offset, (long)canRead, m_FileBuf.buf) == 0 ? canRead : 0;

#if 0   //WIMO_VER1_WRITE_DATA_DEBUG
    if (pFile) {
        fwrite(m_FileBuf.buf, 1, canRead, pFile);
        fflush(pFile);
    }
#endif

#endif

    if ((uint64)ret != canRead) {
        WIMOMUX_LOG("updateFileBuf fail, ret: %d, canRead: %d", ret, canRead);
        return WIMO_UPDATE_FILE_BUF_FAIL;
    } else {
        m_offset +=canRead;
        m_FileBuf.fileOff +=canRead;
        m_FileBuf.offset =0;
        m_FileBuf.remain = canRead;
    }


    //update source cache.
    m_input->updatecache(m_offset);

    return WIMO_UPDATE_FILE_BUF_OK;
}

int WimoVer1Demux::wimoParseWimoPkt(WIMO_VER1_FF_FILE *fp, WimoPacket *pkt)
{
   return 0;
}

int WimoVer1Demux::wimo_start()
{
    WIMOMUX_LOG("wimo_start in, m_ThreadState: %d", m_ThreadState);
	test_txt = NULL;
    if(m_ThreadState == WIMO_IDLE) {
        WIMOMUX_LOG("create read frame thread------>");
        //create read packet thread
    	pthread_attr_t attr;
    	pthread_attr_init(&attr);
    	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    	pthread_create(&mThread, &attr, readFrameThread, this);
    	pthread_attr_destroy(&attr);

        {
            Mutex::Autolock autoLock(mLock);
            m_ThreadState = WIMO_START;
        }
    }

#if WIMO_VER1_WRITE_DATA_DEBUG
    pFile = fopen("/data/video/wimoDemuxRec", "wb+");

    if (pFile ==NULL) {
        ALOGE("open file fail in wimo_start");
    }
#endif

#if WIMO_VER1_USE_TEST_FILE
    pWimoRecTest = fopen("/sdcard/wimoSocketRev", "r");
    //pWimoRecTest = fopen("/sdcard/wimoRec_ok", "r");

    if (pWimoRecTest ==NULL) {
        ALOGE("open file fail in wimo_start");
        return -1;
    }

    recTestFileSize = 0;

    fseek(pWimoRecTest, 0, SEEK_END);
    recTestFileSize = ftell(pWimoRecTest);
    ALOGI("wimo recive test file size: %ld", recTestFileSize);

    fseek(pWimoRecTest, 0, SEEK_SET);
#endif

    return OK;
}

void* WimoVer1Demux::readFrameThread(void* me)
{
    WimoVer1Demux *ap = (WimoVer1Demux*)me;

    WIMOMUX_LOG("read frame thread------>");
    for(;;) {

        if(ap->frameProcess()) {
            break;
        }

        usleep(100);
    }

    return NULL;
}

int WimoVer1Demux::frameProcess()
{
    {
        Mutex::Autolock autoLock(mLock);

        if (m_ThreadState == WIMO_PAUSE) {
            return 0;
        } else if (m_ThreadState == WIMO_STOP) {
            return -1;
        }
    }

    if((wimo_getVideoFrmCache() >=100) && (wimo_getAudioFrmCache() >=0)) {   //cache is full
       return 0;
    }

    if (0 > wimo_readPkt()) {
        WIMOMUX_LOG("set thread stop in frameProcess at file Pos = %lld", m_strProp[0].fp->Tell());

        {
            Mutex::Autolock autoLock(mLock);
            m_ThreadState = WIMO_STOP;
        }
        return -1;
    }

    return 0;
}

void WimoVer1Demux::wimo_pause()
{
    Mutex::Autolock autoLock(mLock);

    if (m_ThreadState != WIMO_STOP) {
        m_ThreadState = WIMO_PAUSE;
    }
}

void WimoVer1Demux::wimo_stop()
{
    WIMOMUX_LOG("wimo_stop in");

    if(m_ThreadState != WIMO_STOP) {
        WIMOMUX_LOG("stop wimo demux read frame thread------>");
        m_ThreadState = WIMO_STOP;
        void *dummy;
        pthread_join(mThread, &dummy);
    }

#if WIMO_VER1_WRITE_DATA_DEBUG
    if (pFile) {
        fclose(pFile);
        pFile = NULL;
    }
#endif

#if WIMO_VER1_USE_TEST_FILE
    if (pWimoRecTest) {
        fclose(pWimoRecTest);
        pWimoRecTest = NULL;
    }
#endif

    WIMOMUX_LOG("wimo_stop out");
    return;
}

int WimoVer1Demux::wimo_read_header(WIMO_VER1_FF_FILE *fp)
{
    m_strProp[0].fp = fp;

    /* set default values */
    uint16_t vIdx = WIMO_STREAM_IDX_VIDEO;
    uint16_t aIdx = WIMO_STREAM_IDX_AUDIO;

    m_nbstreams = 2;
    m_strProp[vIdx].codecid = WIMO_VER1_CODEC_ID_MJPEG;
    m_strProp[vIdx].start_time =0;
    m_strProp[vIdx].duration = 60000;
    m_strProp[vIdx].strmIdx =vIdx;
    m_strProp[vIdx].streamName[0] = 'm';
    m_strProp[vIdx].streamName[1] = 'j';
    m_strProp[vIdx].streamName[2] = 'p';
    m_strProp[vIdx].streamName[3] = 'g';
    m_strProp[vIdx].streamtype = WIMO_STREAM_TYPE_VIDEO;

    m_strProp[aIdx].codecid = WIMO_VER1_CODEC_ID_WAV;
    m_strProp[aIdx].start_time =0;
    m_strProp[aIdx].duration = 60000;
    m_strProp[aIdx].strmIdx =aIdx;
    m_strProp[aIdx].streamName[0] = 'w';
    m_strProp[aIdx].streamName[1] = 'a';
    m_strProp[aIdx].streamName[2] = 'v';
    m_strProp[aIdx].streamName[3] = '/';
    m_strProp[aIdx].streamtype = WIMO_STREAM_TYPE_AUDIO;

    m_strProp[aIdx].bits_per_sample = 16;
    m_strProp[aIdx].rate = 44100;
    m_strProp[aIdx].number_of_audio_channels = 2;

    wimo_GenWaveHeader(m_strProp[aIdx].strmIdx);

    m_startTime = 0;
    m_duration = 15000;

    m_width = 1280;
    m_height = 800;

    return 0;
}


void WimoVer1Demux::alternateCode(uint16 *buf, int bufSize)
{
    int i;
    for (i = 0; i < bufSize / 2; i++) {
        wimo_swap16(&buf[i]);
    }
}

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM     0x0001
#endif

#ifndef MK32BIT
#define MK32BIT(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))
#endif

int WimoVer1Demux::wimo_GenWaveHeader(int streamindex)
{
    ALOGI("wimo_GenWaveHeader in, streamindex: %d", streamindex);

    uint32 bitsPerSample = m_strProp[streamindex].bits_per_sample;
    uint32 samplePerSec = m_strProp[streamindex].rate;
    uint32 chnNo = m_strProp[streamindex].number_of_audio_channels;
    uint32 duration = m_strProp[streamindex].duration - m_strProp[streamindex].start_time;
    uint32 bytesPerSec = bitsPerSample * samplePerSec / 8;
    uint32 nBlockAlign = chnNo * bitsPerSample / 8;

    ALOGI("wimo_GenWaveHeader, bitsPerSample: %d, samRate: %d, channel: %d",
        bitsPerSample, samplePerSec, chnNo);

    m_wavehdr.FormatTag = WAVE_FORMAT_PCM; // wave type
    m_wavehdr.Channels = chnNo;
    m_wavehdr.AvgBytesPerSec = bytesPerSec;
    m_wavehdr.BitsPerSample = bitsPerSample;
    m_wavehdr.BlockAlign = nBlockAlign;
    m_wavehdr.SamplesPerBlock = (((nBlockAlign - (7 * chnNo)) * 8) / (bitsPerSample * chnNo)) + 2;
    m_wavehdr.SamplesPerSec = samplePerSec;
    m_wavehdr.cbSize = 20; // ignore for WAVE_FORMAT_PCM

    return sizeof(WimoWAVEFORMATEX);
}

int WimoVer1Demux::getStrmIdxByCodecId(WimoCodecId id)
{
    for (int i=0; i<m_nbstreams; i++) {
        if (id == (uint32)(m_strProp[i].codecid)) {
            return m_strProp[i].strmIdx;
        }
    }

    return -1;
}

int32 WimoVer1Demux::get_ID_list(uint32 *ids, int& size)
{
    int i =0;
    for (; i<m_nbstreams; i++) {
        if (m_strProp[i].codecid !=WIMO_VER1_CODEC_ID_UNKNOW) {
            ids[i] = m_strProp[i].codecid;
            WIMOMUX_LOG("get_ID_list, trackId[%d] = 0x%X \n", i, ids[i]);
        } else {
            continue;
        }
    }

    size = m_nbstreams;

    return size >=WIMOVER1_MAX_STREAM_NUM ? WIMOVER1_MAX_STREAM_NUM : size;
}

int WimoVer1Demux::wimo_readFrm(int aTrkId, MediaBuffer **out)
{
    WimoCodecId codecId = (WimoCodecId)aTrkId;
    MediaBuffer *buffer = NULL;
    int64_t timeUs = 0LL;
    uint32_t frmLen = 0;
    int retReadPkt = 0;
    int stridx = getStrmIdxByCodecId(codecId);
    WIMO_VER1_FF_FILE *fp = m_strProp[0].fp;

    if (stridx < 0 || stridx > m_nbstreams) {
        ALOGE("stridx: %d is invalid, m_nbstreams: %d", stridx, m_nbstreams);
        goto fail;
    }

    if (codecId == WIMO_VER1_CODEC_ID_MJPEG) {

        while(1) {
            if (m_ThreadState == WIMO_STOP) {

               return WIMO_READ_FRAME_END_OF_STREAM;

            } else if (m_ThreadState == WIMO_INSUFFICIENT_DATA) {
                if (fp->checkWimoBufDataReady() == true) {
                    m_ThreadState = WIMO_DATA_READY;
                } else {
                    *out = new MediaBuffer(0);
                    return INFO_WIMO_INSUFFICIENT;
                }
            }

            {
                Mutex::Autolock autoLock(mLock);

                if(m_frmQueInfo.vFrmCache) {
                    break;
                } else {
                    *out = new MediaBuffer(0);
                    return INFO_WIMO_INSUFFICIENT;
                }
            }


            usleep(10);
        }

        Mutex::Autolock autoLock(mLock);

        buffer = m_frmQueInfo.vFrmQue.editItemAt(0);
        buffer->meta_data()->findInt64(kKeyTime, &timeUs);
        frmLen = buffer->range_length();

        *out = buffer;
        m_frmQueInfo.vFrmQue.removeAt(0);
        m_frmQueInfo.vFrmCache--;
        //WIMOMUX_LOG("read one video frame, len: %d, ts: %d", frmLen, timeUs);

    } else if (codecId == WIMO_VER1_CODEC_ID_WAV) {

        while(1) {
            if (m_ThreadState == WIMO_STOP) {

               return WIMO_READ_FRAME_END_OF_STREAM;

            } else if (m_ThreadState == WIMO_INSUFFICIENT_DATA) {
                if (fp->checkWimoBufDataReady() == true) {
                    m_ThreadState = WIMO_DATA_READY;
                } else {
                    usleep(100);
                    continue;
                }
            }

            {
                Mutex::Autolock autoLock(mLock);

                if(m_frmQueInfo.aFrmCache)
                    break;
            }

            usleep(10);
        }

        Mutex::Autolock autoLock(mLock);

        buffer = m_frmQueInfo.aFrmQue.editItemAt(0);
        buffer->meta_data()->findInt64(kKeyTime, &timeUs);
        frmLen = buffer->range_length();

        *out = buffer;
        m_frmQueInfo.aFrmQue.removeAt(0);
        m_frmQueInfo.aFrmCache--;

        //WIMOMUX_LOG("read one audio frame, len: %d, ts: %lld", frmLen, timeUs);

    } else {
        goto fail;
    }

    return WIMO_READ_FRAME_SUCCESS;

fail:
    return WIMO_READ_FRAME_FAIL;
}
int WimoVer1Demux::wimo_outOneWimoPkt(int* outPktIdx)
{
    uint32 code;
    uint32 flag =0;
    uint8 tmpByte =0;

    WimoPacket *pkt = &m_readPkt;
    WIMO_VER1_FF_FILE *fp = m_strProp[0].fp;

    pkt->size = 0;
    pkt->time = 0;
    pkt->type = WIMO_STREAM_TYPE_NONE;
    pkt->brokenFlag = 0;

    if ((fp ==NULL) || (fp->isEOF())) {
        ALOGE("wimo_outOneWimoPkt fail, fp is end of now");
        return -1;
    }

seek:
    code = 0xFFFFFFFF;
    do {
        if (m_ThreadState == WIMO_STOP) {
            return 0;
        }

        code = (code << 8) | fp->ReadByte();

        if (fp->isEOF()) {
            ALOGE("wimo_outOneWimoPkt fail, fp is end of now");
            return -1;
        }

        if ((code ==WIMO_RK_AUDIO_HEAD) ||
                (code ==WIMO_RK_VIDEO_HEAD)) {
            flag = fp->Read4Byte();
            break;
        }
    } while (1);


    switch (code) {
        case WIMO_RK_AUDIO_HEAD:
            /* audio stream */
            pkt->idx = WIMO_STREAM_IDX_AUDIO;
            pkt->size = fp->Read4Byte();
            pkt->size = BSWAP32(pkt->size);
            /*
             ** wimo v1.0, skip 4 bytes = TimeStamp(4 bytes).
            */
            pkt->size = pkt->size >4 ? pkt->size -4 : 0;
            pkt->type = WIMO_STREAM_TYPE_AUDIO;

            /* skip frame TimeStamp, 4 bytes */
            pkt->time= fp->Read4Byte();

            ALOGV("read one audio packet, size: 0x%X", pkt->size);
            break;

        case WIMO_RK_VIDEO_HEAD:
            /* video stream head */
            if (flag ==WIMO_HEAD_FLAG) {
                uint32 vHeadSize = fp->Read4Byte();
                vHeadSize = BSWAP32(vHeadSize);
                /*
		                 ** 12 bytes = WIMO_RK_VIDEO_HEAD(4 bytes) + head or stream header(4 bytes) +
		                 ** the size of packet(4 bytes).
	                   */
                uint32 remainSize = vHeadSize >12 ? vHeadSize - 12 : 0;

                ALOGV("read one vieo head, skip %d bytes", remainSize);

                /* skip remain head bytes*/
                fp->skipBytesByRead(remainSize);

                pkt->size =0;
                break;

            } else if (flag ==WIMO_STREAM_FLAG){
                pkt->idx = WIMO_STREAM_IDX_VIDEO;
                pkt->size = fp->Read4Byte();
                pkt->size = BSWAP32(pkt->size);
                /*
		                 ** wimo v1.0, skip 4 bytes = FrameNo(2 bytes) + PacketSequence(2 bytes).
		             */
                pkt->size = (pkt->size >4) ? pkt->size -4 : 0;
                pkt->type = WIMO_STREAM_TYPE_VIDEO;

                /* skip frame no, 2 bytes */
                pkt->frame_num = fp->Read2Byte();
		
                pkt->sequence = fp->Read2Byte();

                ALOGV("find one video stream head, seq: %d, size: %d, filePos: %lld",
                    pkt->sequence, pkt->size, fp->Tell());

                if (pkt->size >(1024*1024)) {
                    ALOGI("may be this is a error wimo socket, size: %d------------>", pkt->size);
				}
            }
            break;

        default:
            break;
    }

    if ((pkt->type == WIMO_STREAM_TYPE_NONE)) {
        fp->skipBytesByRead(pkt->size);
        goto seek;
    } else if (pkt->size == 0) {
        goto seek;
    } else {
        if (pkt->type == WIMO_STREAM_TYPE_VIDEO) {
            if (pkt->preVideoPktSeq == pkt->sequence) {
                /* same sequence data, skip it. */
                fp->skipBytesByRead(pkt->size);
                goto seek;
            }

	        if (((pkt->sequence & 0xff) !=((pkt->preVideoPktSeq& 0xff)+1) && 
				!((pkt->sequence & 0xff) == 0 || (pkt->preVideoPktSeq& 0xff00)))||

			((pkt->sequence & 0xff) ==((pkt->preVideoPktSeq& 0xff)+1) && 
				pkt->preVideoPktFrame_num != pkt->frame_num)) {


				{
					int retrtptxt;
					if((retrtptxt = access("data/test/test_txt_file",0)) == 0)//test_file!=NULL)
					{
						
						if(test_txt == NULL)
							test_txt = fopen("data/test/test_txt","wb");
	                    if (test_txt != NULL) {
							fprintf(test_txt,"pkt->sequence %x pkt->preVideoPktSeq %x pkt->preVideoPktFrame_num %d pkt->frame_num  %d %d %d\n ", 
								pkt->sequence ,pkt->preVideoPktSeq ,
								pkt->preVideoPktFrame_num , pkt->frame_num,((pkt->sequence & 0xff) !=((pkt->preVideoPktSeq& 0xff)+1))
								,((pkt->sequence & 0xff) != 0 || !(pkt->preVideoPktSeq& 0xff00)));
	                        fflush(test_txt);
	                    }
					}
	                ALOGV("pkt->sequence %x pkt->preVideoPktSeq %x ", 
								pkt->sequence ,pkt->preVideoPktSeq);
	                pkt->brokenFlag = 1;
				}
	        }
			pkt->preVideoPktFrame_num = pkt->frame_num;
            pkt->preVideoPktSeq = pkt->sequence;
        }

        if (pkt->time != 0) {
            m_strProp[pkt->idx].curTime = pkt->time;
        }
    }

    *outPktIdx = pkt->idx;
    return 0;
}

int WimoVer1Demux::checkVideoFrameHeader(uint8_t* buf, uint32_t size, uint8_t** dst, uint32_t* dstSize)
{
    if ((buf == NULL) || (size <=1) || (dst == NULL) || (dstSize == NULL)) {
        return -1;
    }

    bool headFound = false;
    int32_t off = size - 1;
    while((off >=1) && (off <=(size -1))) {
        if ((*(buf+off-1) == 0xFF) && (*(buf+off) == 0xD8)) {
            ALOGV("find 0xFF D8 at offset: %d", off -1);
            headFound = true;
            break;
        }

        off--;
    }

    if (headFound == true) {
        *dst = buf + off - 1;
        *dstSize = size - (off -1);
        ALOGV("after checkFrameHeader, src size: %d, dstSize: %d",
            size, *dstSize);
        return 0;
    } else {
        return -1;
    }
}

void WimoVer1Demux::wimo_storeVideoFrame(int aStrmIdx, uint32 aPktTime)
{
    //Mutex::Autolock autoLock(mLock);
    WIMO_VER1_FF_FILE *fp = m_strProp[0].fp;

    uint8* pOutData = NULL;
    uint32 frmTimeStamp = 0;
    uint32 frmLen = fp->m_VideoBuf.size;

    uint8* pSrc = fp->m_VideoBuf.buf;
    uint32 srcSize = fp->m_VideoBuf.size;
    uint8* pDst = pSrc;

#if HAVE_WIMO_VIDEO_FRAME_VALID_CHECK
    if (fp->getBrokenFlag()) {
        /*
         ** broken frame process, just reset buffer info as we
         ** have consume those buffer data.
        */
        fp->resetBuffer();
        return;
    }

    /* check frame header: 0xFF D8 */
    if (checkVideoFrameHeader(pSrc, srcSize, &pDst, &frmLen)) {
        /*
         ** invalid video frame, just reset buffer info as we have consume those data.
        */
        fp->resetBuffer();
        return;
    }
#endif
    /* video frame info*/
    frmTimeStamp = aPktTime;

    //set test frame timeMs
    frmTimeStamp = iTestVideoFrmTimeMs;
    iTestVideoFrmTimeMs +=0;

    MediaBuffer *buffer = new MediaBuffer(frmLen);
    pOutData = (unsigned char *)buffer->data();

    memcpy(pOutData, pDst, frmLen);

#if 0   //WIMO_VER1_WRITE_DATA_DEBUG
    if (pFile) {
        fwrite(pOutData, 1, frmLen, pFile);
        fflush(pFile);
    }
#endif

    fp->m_VideoBuf.size =0;
    fp->m_VideoBuf.offset =0;

    buffer->meta_data()->setInt64(kKeyTime, (int64_t)frmTimeStamp * 1000);
    buffer->set_range(0, frmLen);

    m_frmQueInfo.vFrmQue.push(buffer);
    m_frmQueInfo.vFrmCache++;

    //WIMOMUX_LOG("store one video frame ok, size: %d", frmLen);
    return;
}

void WimoVer1Demux::wimo_storeAudioPkt(int aStrmIdx, uint32 aPktSize, uint32 aPktTime)
{
    //Mutex::Autolock autoLock(mLock);
    WIMO_VER1_FF_FILE *fp = m_strProp[0].fp;

    uint8* pOutData = NULL;
    MediaBuffer *buffer = NULL;
    uint32 frameLen = aPktSize;
    uint32 ts = 0;
    uint32 iAudioPktLen = 0;

    if ((m_strProp[aStrmIdx].codecid == WIMO_VER1_CODEC_ID_WAV) && !bSendWavConfig) {
        buffer = new MediaBuffer(frameLen + sizeof(WimoWAVEFORMATEX));
        pOutData = (unsigned char *)buffer->data();
        oscl_memcpy(pOutData, &m_wavehdr, sizeof(WimoWAVEFORMATEX));
        pOutData +=sizeof(WimoWAVEFORMATEX);
        iAudioPktLen +=sizeof(WimoWAVEFORMATEX);
        bSendWavConfig = true;
        ALOGI("here copy wav head data to 1st audio packet");
    } else {
        buffer = new MediaBuffer(frameLen);
        pOutData = (unsigned char *)buffer->data();
    }

    /* audio frame info*/
    iAudioPktLen += frameLen;

    ts = m_strProp[aStrmIdx].curTime - m_startTime;

    if(buffer && buffer->data()) {
        buffer->meta_data()->setInt64(kKeyTime, (int64_t)ts * 1000);
        buffer->set_range(0, iAudioPktLen);

        /* read audio pkt data */
        if (fp->Read(pOutData, frameLen) !=frameLen) {
            buffer->release();
            return;
        }

#if WIMO_VER1_WRITE_DATA_DEBUG
        if (pFile) {
            fwrite(buffer->data(), 1, iAudioPktLen, pFile);
            fflush(pFile);
        }
#endif

        m_frmQueInfo.aFrmQue.push(buffer);
        m_frmQueInfo.aFrmCache++;
    }

    //WIMOMUX_LOG("store one audio packet, len = %d, ts = %d\n", iAudioPktLen, ts);
    return;
}



int WimoVer1Demux::wimo_readPkt()
{
    Mutex::Autolock autoLock(mLock);

    int ret = 0;
    int pktIndx = 0;
    WimoPacket *pkt = NULL;
    WIMO_VER1_FF_FILE *fp = m_strProp[0].fp;

    pkt = &m_readPkt;
    pkt->size = 0;
    pkt->type = WIMO_STREAM_TYPE_NONE;
    pkt->sequence = 0;
    pkt->idx =0;

    for (int i=0; i<m_nbstreams; i++) {
        m_strProp[i].preTime = m_strProp[i].curTime;
    }

RedoGetPacket:
    if (m_ThreadState == WIMO_STOP) {
        return 0;
    }

    if (pkt->size <= 0) {
        if (wimo_outOneWimoPkt(&pktIndx) <0) {
            return -1;
        }
    }

    if (pkt->size > 0) {
        if (m_readPkt.type == WIMO_STREAM_TYPE_VIDEO) {

            ALOGV("storeVideoPktFromWimoData, read %d bytes, sequence: %d",
                pkt->size, m_readPkt.sequence);

            if (pkt->brokenFlag) {
                fp->setBufferDataIsBroken();
                pkt->brokenFlag =0;
            }

            /* read video pkt data to buffer */
            if (fp->storeVideoPktFromWimoData(pkt->size) ==0) {
                return 0;
            }

            /*check frame end */
            if ((pkt->size <=64) || (m_readPkt.sequence & 0x8000)) {

                wimo_storeVideoFrame(pktIndx, pkt->time);
            } else {
                pkt->time = 0;
                pkt->size = 0;
            }
        } else if (m_readPkt.type == WIMO_STREAM_TYPE_AUDIO) {
            ALOGV("store one audio frame, pkt size: %d", pkt->size);
            wimo_storeAudioPkt(pktIndx, pkt->size, pkt->time);
        }
    } else {
        goto RedoGetPacket;
    }

    return 0;
}


void WimoVer1Demux::wimo_read_close()
{
    flushVideoFrmQue();
    flushAudioFrmQue();
    WIMOMUX_LOG("wimo_read_close success");
}

void WimoVer1Demux::flushVideoFrmQue()
{
    //Mutex::Autolock autoLock(mLock);

    WIMOMUX_LOG("flushVideoFrmQue, video frame: %d", m_frmQueInfo.vFrmQue.size());

    size_t i =0;
    for (i =0; i < m_frmQueInfo.vFrmQue.size(); ++i) {
        MediaBuffer *buffer = m_frmQueInfo.vFrmQue.editItemAt(i);

        buffer->setObserver(NULL);
        buffer->release();
    }
    m_frmQueInfo.vFrmCache = 0;
    m_frmQueInfo.vFrmQue.clear();
}

void WimoVer1Demux::flushAudioFrmQue()
{
    //Mutex::Autolock autoLock(mLock);

    size_t i =0;

    for (i =0; i < m_frmQueInfo.aFrmQue.size(); ++i) {
        MediaBuffer *buffer = m_frmQueInfo.aFrmQue.editItemAt(i);

        buffer->setObserver(NULL);
        buffer->release();
    }

    m_frmQueInfo.aFrmCache =0;
    m_frmQueInfo.aFrmQue.clear();

}

}
