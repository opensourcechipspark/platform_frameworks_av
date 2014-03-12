#include <media/stagefright/Audio_OutPut_Source.h>
#include <sys/poll.h>
#include <cutils/sockets.h>
#include <sys/un.h>
#include <utils/CallStack.h>
#include <vpu_mem.h>
#define LOG_TAG "Audio_OutPut_Source"
#define MIRRORING_DEBUG
//#define in_cycle

void foo(void)
{
#if 0
    android::CallStack stack;
    stack.update(2);
    stack.dump();
#endif
}

//#define in_cycle
namespace android {

extern FILE *test_ts_txt;

#define MIRRORING_IOCTL_MAGIC 0x60
#define MIRRORING_START						_IOW(MIRRORING_IOCTL_MAGIC, 0x1, unsigned int)
#define MIRRORING_STOP						_IOW(MIRRORING_IOCTL_MAGIC, 0x2, unsigned int)
#define MIRRORING_SET_ROTATION				_IOW(MIRRORING_IOCTL_MAGIC, 0x3, unsigned int)
#define MIRRORING_DEVICE_OKAY				_IOW(MIRRORING_IOCTL_MAGIC, 0x4, unsigned int)
#define MIRRORING_SET_TIME					_IOW(MIRRORING_IOCTL_MAGIC, 0x5, unsigned int)
#define MIRRORING_GET_TIME					_IOW(MIRRORING_IOCTL_MAGIC, 0x6, unsigned int)
#define MIRRORING_GET_ROTATION				_IOW(MIRRORING_IOCTL_MAGIC, 0x7, unsigned int)
#define MIRRORING_SET_ADDR 					_IOW(MIRRORING_IOCTL_MAGIC, 0x8, unsigned int)
#define MIRRORING_GET_ADDR 					_IOW(MIRRORING_IOCTL_MAGIC, 0x9, unsigned int)
#define MIRRORING_SET_SIGNAL					_IOW(MIRRORING_IOCTL_MAGIC, 0xa, unsigned int)
#define MIRRORING_GET_SIGNAL					_IOW(MIRRORING_IOCTL_MAGIC, 0xb, unsigned int)
#define MIRRORING_SET_WIRELESSDB				_IOW(MIRRORING_IOCTL_MAGIC, 0xc, unsigned int)
#define MIRRORING_GET_WIRELESSDB				_IOW(MIRRORING_IOCTL_MAGIC, 0xd, unsigned int)
#define MIRRORING_GET_WIRELESS_DATALOST		_IOW(MIRRORING_IOCTL_MAGIC, 0xe, unsigned int)
#define MIRRORING_SET_FRAMERATE				_IOW(MIRRORING_IOCTL_MAGIC, 0xf, unsigned int)
#define MIRRORING_GET_CHIPINFO				_IOR(MIRRORING_IOCTL_MAGIC, 0x31, unsigned int)

#define MIRRORING_AUDIO_OPEN           	      	_IOW(MIRRORING_IOCTL_MAGIC, 0x21, unsigned int)
#define MIRRORING_AUDIO_CLOSE                	_IOW(MIRRORING_IOCTL_MAGIC, 0x22, unsigned int)
#define MIRRORING_AUDIO_GET_BUF            		_IOW(MIRRORING_IOCTL_MAGIC, 0x23, unsigned int)
#define MIRRORING_AUDIO_SET_PARA                     _IOW(MIRRORING_IOCTL_MAGIC, 0x24, unsigned int)
#define	MIRRORING_AUDIO_SET_VOL			_IOW(MIRRORING_IOCTL_MAGIC, 0x25, unsigned int)
#define	MIRRORING_AUDIO_GET_VOL			_IOR(MIRRORING_IOCTL_MAGIC, 0x26, unsigned int)

Audio_OutPut_Source::Audio_OutPut_Source(int32_t sampleRate, int32_t numChannels)
    : mStarted(false),
      mSampleRate(sampleRate),
      mNumChannels(numChannels),
      mPhase(0){
    CHECK((status_t)(numChannels == 1 || numChannels == 2));
    ALOGD("Audio_OutPut_Source::Audio_OutPut_Source 1");
 #ifdef MIRRORING_DEBUG
    audio_input = audio_input_test = NULL;
  //  audio_input = fopen("/data/audioinput.pcm","wb");
 
  //  audio_input_test = fopen("/data/audio_input_test.pcm","wb");
#endif
    buffer_size = 52;
    nBytePerFrame_src = 4096; 
    nSamplePerFrame_src = 1024;
    is_last_has_data = 0;
	kBufferSize = 2048 * mNumChannels;
	frameSize = mNumChannels * sizeof(int16_t);
	numFramesPerBuffer = nSamplePerFrame_src;
	last_pcm_timestamp = 0;
	start_data_time_us = 0;
	mOffset = 0;
	file_offset = 0;

	Input_Buf = audio_enc_buf = NULL;
    ALOGD("Audio_OutPut_Source::Audio_OutPut_Source");

    fd = -1;
	fd = open("/dev/mirroring", 00000002, 0);
	if (fd < 0)
	{
		
		ALOGD("Audio_OutPut_Source::Audio_OutPut_Source open fd is error %d",fd);
		return ;
	}

	
	unsigned long para_buff[6];
	if (( ioctl(fd, MIRRORING_AUDIO_OPEN, para_buff)) < 0)
	{
		ALOGD("ioctl MIRRORING_AUDIO_OPEN error ");
		close(fd);
		fd = -1;
		return ;
	}
	Input_Buf = (unsigned char*)malloc(4120);
	audio_enc_buf = (unsigned char*)malloc(4096);
	if(Input_Buf == NULL || audio_enc_buf == NULL)
		return;
    
}

 Audio_OutPut_Source::~Audio_OutPut_Source() {
    if (mStarted) {
        stop();
    }
	if(audio_enc_buf!=NULL)
		free(audio_enc_buf);
	if(Input_Buf!=NULL)
		free(Input_Buf);
	if(fd >=0)
	{
		ioctl(fd, MIRRORING_AUDIO_CLOSE, NULL);
		close(fd);
		
	}
	
  #ifdef MIRRORING_DEBUG
     if(audio_input!=NULL)
	fclose(audio_input);
    if(audio_input_test!=NULL)
	fclose(audio_input_test);
#endif
    ALOGD("Audio_OutPut_Source::~Audio_OutPut_Source");
	
   


	
}

status_t Audio_OutPut_Source::start(MetaData *params) {
	int ret = 0;
	
	if(mStarted)
	{
		ALOGD("Audio_Output_Source already start");
		return -1111;
	}
	
	int err;
	if (fd < 0)
	{
		
		ALOGD("Audio_OutPut_Source fd is error %d",fd);
		return -1111;
	}
	
	if((mNumChannels< 1 || mNumChannels > 2) || mSampleRate != 44100)
	{
		ALOGD("channel num can't be support or sampleRate not equal to system samplerate");
		return -1111;
	}
	if(Input_Buf == NULL || audio_enc_buf == NULL)
	{
		ALOGD("Input_Buf == NULL || audio_enc_buf == NULL");
		return -1111;
	}
	mStarted = true;
	return OK;
    

}

status_t Audio_OutPut_Source::stop() {
    
    if(mStarted == false)
	{
		ALOGD("Audio_OutPut_Source can't be started before stop 0");
		return -1111;
	
	}
    mStarted = false;

    return OK;
}

sp<MetaData> Audio_OutPut_Source::getFormat() {
    sp<MetaData> meta = new MetaData;
    meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_RAW);
    meta->setInt32(kKeyChannelCount, mNumChannels);
    meta->setInt32(kKeySampleRate, mSampleRate);
    meta->setInt32(kKeyMaxInputSize, kBufferSize);

    return meta;
}


status_t Audio_OutPut_Source::read(
        MediaBuffer **out, const ReadOptions *options) {
	*out = NULL;
	Mutex::Autolock autoLock(mLock);
	unsigned long temp[2];
	int64_t timeFirst,timesec;
	int 	ret;
	int		try_time = 0;
	int		find_data = 0;
	static int	audio_num=0;
	if((audio_num%100)==0)
		ALOGD("audio_num %d",audio_num++);
	if(mStarted == false)
	{
		return -1111;
	}
	if(start_data_time_us == 0 )
	{
		ret = ioctl(fd,MIRRORING_GET_TIME,&start_data_time_us);
		if(ret < 0)
		{
			ALOGD("Audio_OutPut_Source gettime error");
			return -1111;
		}
	}	
	if(start_data_time_us == 0)
	{
		return -2111;
	}
	while (!find_data && try_time < 3)
	{
		ret = ioctl(fd, MIRRORING_AUDIO_GET_BUF, Input_Buf);
		
		if( ret < 0)
		{
			ALOGD("ioctl MIRRORING_AUDIO_GET_BUF ret %d",ret);
			return -1111;
		}
		else if(Input_Buf[4116] == 0)
		{
			if(is_last_has_data > 0 && try_time < 3)
			{
				usleep(3000);
			}
			else
			{
				is_last_has_data = 0;
				find_data = 1;
			}
		}
		else	if(Input_Buf[4116] == 1)
		{
			memcpy(&timeFirst,	(void*)Input_Buf + nBytePerFrame_src,	sizeof(timeval));
			memcpy(&timesec,	(void*)Input_Buf + nBytePerFrame_src + sizeof(timeval),	sizeof(timeval));
			memcpy(temp,(void*)Input_Buf + nBytePerFrame_src + sizeof(timeval) * 2 ,sizeof(timeval));
			
			if(timeFirst - last_pcm_timestamp < 10000ll)//( ((int64_t)mPhase * 1000000ll)/ mSampleRate   + start_data_time_us > (int64_t)timeFirst.tv_sec * 1000000ll + timeFirst.tv_usec  +  100000ll))
			{
				ALOGV("audio data outofdate mNumChannels %d numFramesPerBuffer %d  frameSize %d time %lld %lld %lld %lld %lld %lld %lld %d %d  head %d tail %d ",	
					mNumChannels,numFramesPerBuffer,  frameSize,
					timeFirst,	timesec, ((int64_t)mPhase * 1000000ll)/ mSampleRate   + start_data_time_us 
					,start_data_time_us,((int64_t)mPhase * 1000000ll)/ mSampleRate ,((int64_t)mPhase * 1000000ll)/ mSampleRate   + start_data_time_us -	(int64_t)timesec.tv_sec * 1000000ll - timesec.tv_usec,
					((int64_t)mPhase * 1000000ll)/ mSampleRate   + start_data_time_us -	timeFirst,mPhase,mSampleRate, temp[0], temp[1]);
			}
			else
			{
				
				ALOGV("audio data okay mNumChannels %d numFramesPerBuffer %d  frameSize %d time %lld %lld %lld %lld %lld %lld %lld %d %d  head %d tail %d ",	
					mNumChannels,numFramesPerBuffer,  frameSize,
					timeFirst,	timesec, ((int64_t)mPhase * 1000000ll)/ mSampleRate	+ start_data_time_us 
					,start_data_time_us,((int64_t)mPhase * 1000000ll)/ mSampleRate ,((int64_t)mPhase * 1000000ll)/ mSampleRate	 + start_data_time_us - (int64_t)timesec.tv_sec * 1000000ll - timesec.tv_usec,
					((int64_t)mPhase * 1000000ll)/ mSampleRate	 + start_data_time_us - timeFirst,mPhase,mSampleRate, temp[0], temp[1]);
				find_data = 1;
			}
			is_last_has_data ++;
			
		}
		try_time++;
	}
	if(Input_Buf[4116] == 1)
	{
		ALOGV("audio_outputread mNumChannels %d numFramesPerBuffer %d  frameSize %d time %lld %lld %lld %lld %lld %lld %lld %d %d  head %d tail %d ",	
			mNumChannels,numFramesPerBuffer,  frameSize,timeFirst,	timesec, ((int64_t)mPhase * 1000000ll)/ mSampleRate   + start_data_time_us 
			,start_data_time_us,((int64_t)mPhase * 1000000ll)/ mSampleRate ,
			((int64_t)mPhase * 1000000ll)/ mSampleRate   + start_data_time_us -	timesec,
			((int64_t)mPhase * 1000000ll)/ mSampleRate   + start_data_time_us -	timeFirst,mPhase,mSampleRate, temp[0], temp[1]);
		short *src = (short*)Input_Buf;// (short*)audio_buf->vir_addr;//audio_temp_buffer;
		short *dst = (short*)audio_enc_buf;
		int i=0;
		if(mNumChannels == 1)
		{
			for(i = 0; i < numFramesPerBuffer;i++)
			{
				dst[i] = src[2*i];
			}
		}
		else	if(mNumChannels == 2)
			memcpy(dst,src,numFramesPerBuffer * frameSize);
		else
			ALOGD("mNumChannels is err %d",mNumChannels);


		last_pcm_timestamp = timeFirst;
		if(audio_input_test!=NULL)
			fwrite(src,4096,1,audio_input_test);
		if(audio_input!=NULL)
			fwrite(dst,2048,1,audio_input);
	}
	else
	{
		ALOGV("audio_output_source no data");
		memset(audio_enc_buf,0,numFramesPerBuffer * frameSize);
		if(audio_input_test!=NULL)
		{
			fwrite(audio_enc_buf,2048,1,audio_input_test);
			fwrite(audio_enc_buf,2048,1,audio_input_test);
		}
	}
	
	(*out) = new MediaBuffer(audio_enc_buf,numFramesPerBuffer * frameSize);
	(*out)->meta_data()->setInt64(kKeyTime, ((int64_t)mPhase * 1024ll * 1000000ll) / (int64_t)mSampleRate);	
	mPhase ++;
	(*out)->set_range(0, numFramesPerBuffer * frameSize);	
	ALOGV("Wimo Audio TimeUs %lld systime %lld ",
		((int64_t)mPhase * 1024ll * 1000000ll) / (int64_t)mSampleRate,systemTime(SYSTEM_TIME_MONOTONIC) / 1000);
	{
		int ret_txt;
		if((ret_txt = access("data/test/test_ts_txt_file",0)) == 0)//test_file!=NULL)
		{
			if(test_ts_txt == NULL)
				test_ts_txt = fopen("data/test/test_ts_txt.txt","wb+");
			else
			{
				int64_t cur_time;
				static  int64_t last_audio_time_us;
				static  int64_t last_audio_systime;
				static  int64_t timeUs = ((int64_t)mPhase * 1024ll * 1000000ll) / mSampleRate;
				cur_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;		
			
				fprintf(test_ts_txt,"Audio_output_source sys time %lld timeUs %lld delta sys time %lld delta time  %lld mPhase % d start_data_time_us %lld ((int64_t)mPhase * 1000000ll) / mSampleRate %lld\n",
					cur_time,timeUs,cur_time - last_audio_systime
					,timeUs - last_audio_time_us,mPhase,start_data_time_us,((int64_t)mPhase * 1000000ll) / mSampleRate);
				last_audio_time_us = timeUs;
				last_audio_systime = cur_time;
				
					
			}
		}
		else
		{
			
			if(test_ts_txt!=NULL)
			{
				fclose(test_ts_txt);
				test_ts_txt = NULL;
			}
			ALOGV("test_ts_txt==NULL ret %d",ret_txt);
		}
	}
    return OK;
}





}
