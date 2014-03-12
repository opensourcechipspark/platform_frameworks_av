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

#include "AACDecoder_MIRRORING.h"
#define LOG_TAG "AACDecoder_MIRRORING"

#include "../../include/ESDS.h"

#include "pvmp4audiodecoder_api.h"

#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#define REPLAY_SOCKET_PATH "@replay_socket"


//#define ALOGD ALOGV
namespace android {
 FILE* pcm_test_file;
 FILE* aac_test;
 FILE* aac_test_file;
extern FILE* rtp_ts_txt;
extern	int64_t video_timeUs;
 extern  int64_t audio_timeUs;
extern			int64_t  video_adjustment;

extern 	int64_t Rtp_Audio_Start_TimeUs;
extern	int 	connection_flag;
extern	int64_t latency_time;
AACDecoder_MIRRORING::AACDecoder_MIRRORING(const sp<MediaSource> &source)
    : mSource(source),
      mStarted(false),
      mBufferGroup(NULL),
      mConfig(new tPVMP4AudioDecoderExternal),
      mDecoderBuf(NULL),
      mAnchorTimeUs(0),
      mNumSamplesOutput(0),
	  adjust_flag(0),
      mInputBuffer(NULL) {
        last_timeUs = 0;
		Waiting_timeUs=0;
    sp<MetaData> srcFormat = mSource->getFormat();
    start_timeUs = 0;

    int32_t sampleRate;
    CHECK(srcFormat->findInt32(kKeySampleRate, &sampleRate));

    mMeta = new MetaData;
    mMeta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_RAW);

    // We'll always output stereo, regardless of how many channels are
    // present in the input due to decoder limitations.
    mMeta->setInt32(kKeyChannelCount, 2);
    mMeta->setInt32(kKeySampleRate, sampleRate);

    int64_t durationUs;
	Video_timeUs = 0;
    if (srcFormat->findInt64(kKeyDuration, &durationUs)) {
        mMeta->setInt64(kKeyDuration, durationUs);
    }
    mMeta->setCString(kKeyDecoderComponent, "AACDecoder_MIRRORING");
    wimo_flag = 0;
	connect_flag = 1;
	isAccpetSuccess =0;
	Cur_TimeUs = gpu_adjustment = 0;
	server_sockfd = client_sockfd = -1;
    mInitCheck = initCheck();
	ALOGD("before thread start");
	pthread_create(&mThread, NULL, rec_data, this);
}

status_t AACDecoder_MIRRORING::initCheck() {
    memset(mConfig, 0, sizeof(tPVMP4AudioDecoderExternal));
    mConfig->outputFormat = OUTPUTFORMAT_16PCM_INTERLEAVED;
    mConfig->aacPlusEnabled = 1;

    // The software decoder doesn't properly support mono output on
    // AACplus files. Always output stereo.
    mConfig->desiredChannels = 2;

    UInt32 memRequirements = PVMP4AudioDecoderGetMemRequirements();
    mDecoderBuf = malloc(memRequirements);

    status_t err = PVMP4AudioDecoderInitLibrary(mConfig, mDecoderBuf);
    if (err != MP4AUDEC_SUCCESS) {
        ALOGE("Failed to initialize MP4 audio decoder");
        return UNKNOWN_ERROR;
    }

    uint32_t type;
    const void *data;
    size_t size;
    sp<MetaData> meta = mSource->getFormat();
    if (meta->findData(kKeyESDS, &type, &data, &size)) {
        ESDS esds((const char *)data, size);
        CHECK_EQ(esds.InitCheck(), (status_t)OK);

        const void *codec_specific_data;
        size_t codec_specific_data_size;
        esds.getCodecSpecificInfo(
                &codec_specific_data, &codec_specific_data_size);

        mConfig->pInputBuffer = (UChar *)codec_specific_data;
        mConfig->inputBufferCurrentLength = codec_specific_data_size;
        mConfig->inputBufferMaxLength = 0;
		UChar * spec = (UChar *)codec_specific_data;
		for(int i = 0; i < codec_specific_data_size; i++)
		{
			ALOGE("spec[%d] = 0x%x",i+1,spec[i]);
		}

        if (PVMP4AudioDecoderConfig(mConfig, mDecoderBuf)
                != MP4AUDEC_SUCCESS) {
            return ERROR_UNSUPPORTED;
        }
    }else if(meta->findData(kKeyAacInfo,&type,&data,&size)){

     	mConfig->pInputBuffer = (UChar *)data;
        mConfig->inputBufferCurrentLength = size;
        mConfig->inputBufferMaxLength = 0;

        if (PVMP4AudioDecoderConfig(mConfig, mDecoderBuf)
                != MP4AUDEC_SUCCESS) {
            return ERROR_UNSUPPORTED;
        }
    }

    return OK;
}

AACDecoder_MIRRORING::~AACDecoder_MIRRORING() {
    if (mStarted) {
        stop();
    }
	if(client_sockfd)
	{
		close(client_sockfd);
		client_sockfd = -1;
	}
	if(server_sockfd)
	{
		close(server_sockfd);
		server_sockfd = -1;
	}
	void* retval1;
  	connect_flag = 0;
   	pthread_join(mThread, &retval1);
	ALOGD("AACDecoder_MIRRORING::~AACDecoder_MIRRORING   " );


    delete mConfig;
    mConfig = NULL;
}

status_t AACDecoder_MIRRORING::start(MetaData *params) {
    CHECK(!mStarted);
    if(params!=NULL)
	wimo_flag = 1;
    mBufferGroup = new MediaBufferGroup;
    mBufferGroup->add_buffer(new MediaBuffer(4096 * 2));

    mSource->start();

    mAnchorTimeUs = 0;
    mNumSamplesOutput = 0;
    mStarted = true;
    mNumDecodedBuffers = 0;
    mUpsamplingFactor = 2;

    return OK;
}

status_t AACDecoder_MIRRORING::stop() {
    CHECK(mStarted);

    if (mInputBuffer) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    free(mDecoderBuf);
    mDecoderBuf = NULL;

    delete mBufferGroup;
    mBufferGroup = NULL;

    mSource->stop();

    mStarted = false;

    return OK;
}

sp<MetaData> AACDecoder_MIRRORING::getFormat() {
    return mMeta;
}

void* AACDecoder_MIRRORING::rec_data(void* me)
{
	AACDecoder_MIRRORING* mAACDecoder_MIRRORING = static_cast<AACDecoder_MIRRORING *>(me);
	ALOGD("%x AACDecoder_MIRRORING rec_data %x",me,mAACDecoder_MIRRORING);
	mAACDecoder_MIRRORING->ThreadWrapper(NULL);
	return NULL;
}
void *AACDecoder_MIRRORING::ThreadWrapper(void *)
{

	//int server_sockfd, client_sockfd = -1;
	int server_len, client_len;
	struct sockaddr_un server_address;
	struct sockaddr_un client_address;
	fd_set fdSet;
	int flags;

	struct pollfd fds[2];

	unlink(REPLAY_SOCKET_PATH);

	if((server_sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	{
	        ALOGE("latency:server socket error!");
	        goto ERROR_HANDLE_GPU_WIMO;
	}

	server_len = sizeof(server_address);
	bzero(&server_address, server_len);
	server_address.sun_family = AF_UNIX;
	strcpy (server_address.sun_path, REPLAY_SOCKET_PATH);

	server_address.sun_path[0] = '\0';
	//strncpy(server_address.sun_path, REPLAY_SOCKET_PATH, sizeof(REPLAY_SOCKET_PATH));
	if((flags = fcntl(server_sockfd, F_GETFL,O_NONBLOCK)) < 0)
	{

		ALOGD("fcntl nonblockF_GETFL error");
		goto ERROR_HANDLE_GPU_WIMO;
	}
    flags |= O_NONBLOCK;
    if(fcntl(server_sockfd, F_SETFL,flags) < 0)
	{
		ALOGD("fcntl nonblockF_SETFL error");
		goto ERROR_HANDLE_GPU_WIMO;
	}
	ALOGD("before bind errno %d sizeof(REPLAY_SOCKET_PATH) %d server_address.sun_path %s server_len %d",
		errno,sizeof(REPLAY_SOCKET_PATH),server_address.sun_path,server_len);
	if(bind(server_sockfd, (struct sockaddr *)&server_address, server_len) == -1)
	{
	        ALOGE("latency:server bind error! errno %d",errno);
	        goto ERROR_HANDLE_GPU_WIMO;
	}


	ALOGD("before listen errno %d",errno);

	if(listen(server_sockfd, 5) == -1)
	{
	        ALOGE("latency:server listen error!");
	        goto ERROR_HANDLE_GPU_WIMO;
	}

LISTENTO_GPU_NEWS:
	isAccpetSuccess = 0;
	Video_timeUs = 0;
	gpu_adjustment = 200000ll;
	ALOGD("after listen errno %d",errno);

	while(connect_flag && client_sockfd == -1)
	{
		struct timeval timeVal;
		FD_ZERO(&fdSet);
		FD_SET(server_sockfd, &fdSet);
		timeVal.tv_sec = 1;
		timeVal.tv_usec = 0;
		ALOGD("before select errno %d",errno);

		int rtn = select(server_sockfd+1, &fdSet, NULL, NULL, &timeVal);
		switch(rtn)
		{
		case -1:
			break;
		case 0:
			break;
		default:
			if(FD_ISSET(server_sockfd, &fdSet))
			{
				if(client_sockfd > 0)
				{
					close(client_sockfd);
				}

				client_len = sizeof(client_address);
				bzero(&client_address, client_len);
				ALOGD("before accept errno %d",errno);
				if((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_address, &client_len)) == -1)
				{
						ALOGE("latency:server accept error!");
						if(client_sockfd > 0)
						{
							close(client_sockfd);
						}
						goto LISTENTO_GPU_NEWS;
				}
				fds[0].fd = client_sockfd;
				fds[0].events = POLLIN;
				isAccpetSuccess = 1;
				ALOGD("connection setup");
			}
		}
	}



	ALOGD("before socket work client_sockfd %d",client_sockfd);
	while(client_sockfd > 0)
	{
		int ret;
		if((ret = poll(fds, 1, 5)) > 0)
		{
			if(fds[0].revents & POLLIN)
			{
				int64_t latency[2] ;
				char *data = (char *)latency;
				int len = sizeof(latency);
				while(len > 0)
				{
					int n = recv(client_sockfd, data, sizeof(latency),0);
					if(n < 0)
					{
						ALOGD("Error server recv latency : %s , because of the switch of app",  strerror(errno));
						isAccpetSuccess = 0;
						if(client_sockfd)
						{
							close(client_sockfd);
							client_sockfd = -1;
						}

						goto LISTENTO_GPU_NEWS;
					}
					data += n;
					len -= n;
				}

				if(len == 0)
				{
					gpu_adjustment 	= latency[0];
					Video_timeUs	= latency[1];
					if(rtp_ts_txt != NULL)

							{

		fprintf(rtp_ts_txt,"latency:server recv adjustment = %lld Video_timeUs %lld size %d\n", gpu_adjustment,Video_timeUs,sizeof(latency));
		fflush(rtp_ts_txt);

							}
					ALOGV("latency:server recv adjustment = %lld Video_timeUs %lld size %d", gpu_adjustment,Video_timeUs,sizeof(latency));
				}
			}
			if (fds[0].revents & POLLHUP)
			{
				ALOGD(" disconnect fd %d    %d",fds[0].revents,fds[0].revents);
				isAccpetSuccess = 0;
				if(client_sockfd)
				{
					close(client_sockfd);
					client_sockfd = -1;
				}

				goto LISTENTO_GPU_NEWS;

			}
			if(fds[0].revents & (POLLERR  | POLLNVAL))
			{
				ALOGD("disconnect error  %d",fds[1].revents);

				isAccpetSuccess = 0;
				if(client_sockfd)
				{
					close(client_sockfd);
					client_sockfd = -1;
				}

				goto LISTENTO_GPU_NEWS;
			}

		}
		{
			char *data = (char *)&Cur_TimeUs;
			int len = sizeof(int64_t);
			ALOGV("latency:server send latency = %lld isAccpetSuccess %d", Cur_TimeUs,isAccpetSuccess);
			while(len > 0)
			{
				int n = send(client_sockfd, data, sizeof(int64_t),0);
				if(n < 0)
				{
					ALOGD("Error server send latency : %s , because of the switch of app",  strerror(errno));
					isAccpetSuccess = 0;
					if(client_sockfd)
					{
						close(client_sockfd);
						client_sockfd = -1;
					}

					goto LISTENTO_GPU_NEWS;
				}
				data += n;
				len -= n;
			}

		}
	}
ERROR_HANDLE_GPU_WIMO:

	if(client_sockfd)
	{
		close(client_sockfd);
		client_sockfd = -1;
	}
	if(server_sockfd)
	{
		close(server_sockfd);
		server_sockfd = -1;
	}

	return NULL;
}


status_t AACDecoder_MIRRORING::read(
        MediaBuffer **out, const ReadOptions *options) {
    /*
     ** end of aac audio stream in the case of initCheck is not OK,
     ** avoid abnormal playing later.   @Jun 16, 2011. by hbb
     */
	int64_t timeFirst,timeSec	 ;
	int set_flag = 0;
	int64_t temptimeus;
	int64_t cur_rtp_timeUs = Rtp_Audio_Start_TimeUs;//rtp_ts_1_timeUs;
	int64_t cur_video_timeUs = video_timeUs;
	int64_t adjustment ;
	timeFirst = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
	if(rtp_ts_txt != NULL)

	{

		fprintf(rtp_ts_txt," AACDecoder_MIRRORING::read in\n");
		fflush(rtp_ts_txt);

	}
    if(start_timeUs ==0 )
    {
		start_timeUs = timeFirst;
		last_adujst_time = timeFirst;
    }
    if(mInitCheck != OK) {
        ALOGE("mInitCheck is not OK, so end aac audio stream");
        return ERROR_END_OF_STREAM;
    }

    status_t err;

    *out = NULL;

    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        CHECK(seekTimeUs >= 0);

        mNumSamplesOutput = 0;

        if (mInputBuffer) {
            mInputBuffer->release();
            mInputBuffer = NULL;
        }

        // Make sure that the next buffer output does not still
        // depend on fragments from the last one decoded.
        PVMP4AudioDecoderResetBuffer(mDecoderBuf);
    } else {
        seekTimeUs = -1;
    }
repeat:
    {

	    int looptime=0;
	    while (mInputBuffer == NULL) {
			err = mSource->read(&mInputBuffer, options);

			if (err != OK) {
			    return err;
			}
			int64_t timeUs;
		  	timeFirst = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
			if (mInputBuffer->meta_data()->findInt64(kKeyTime, &timeUs)) {
			}
			else {
			        CHECK(seekTimeUs < 0);
			}
			temptimeus = timeUs;
			#if 1

			if(connection_flag == 1)
  				cur_rtp_timeUs = Rtp_Audio_Start_TimeUs;//rtp_ts_1_timeUs;
  			else
				cur_rtp_timeUs = start_timeUs;


			if(isAccpetSuccess)
			{
				cur_video_timeUs = Video_timeUs;
				adjustment = gpu_adjustment;
				ALOGD("AACDecoder_MIRRORING::read Video_timeUs %lld timeUs %lld",Video_timeUs,timeUs);
			}
			else
			{
				cur_video_timeUs = video_timeUs;
				adjustment = video_adjustment;
			}
if(1)//nection_flag == 0 || cur_video_timeUs == 0)
			{
				if(cur_rtp_timeUs + timeUs < timeFirst - 100000ll )
				{
					if(last_timeUs < timeUs)//loop tntil the real timeUs catch up with the old setted one  , if there is no data,the old setted is also faster than the real timeUs.so it's okay
					{
						if(cur_rtp_timeUs + timeUs < timeFirst - 300000ll  || (timeFirst - last_adujst_time > 50000000ll &&
							cur_rtp_timeUs + timeUs < timeFirst - 100000ll && adjustment <= 200000ll))//recalculate the timeUs.
						{
							if(rtp_ts_txt != NULL)

							{
								if(timeFirst - last_adujst_time > 50000000ll &&
							cur_rtp_timeUs + timeUs < timeFirst - 100000ll && adjustment <= 200000ll)
									fprintf(rtp_ts_txt," AACDecoder TimeStamp regular adjust timeUs %lld cur_rtp_timeUs %lld last_timeUs %lld timeFirst %lld adujst %lld start_timeUs %lld delta %lld timeFirst - last_adujst_time %lld video_adjustment %lld\n"
																	,timeUs,cur_rtp_timeUs,last_timeUs,timeFirst,last_adujst_time,start_timeUs,timeFirst-cur_rtp_timeUs-timeUs,timeFirst - last_adujst_time,adjustment);

								else
								fprintf(rtp_ts_txt," AACDecoder TimeStamp delay 300 ms timeUs %lld cur_rtp_timeUs %lld last_timeUs %lld timeFirst %lld adujst %lld start_timeUs %lld delta %lld timeFirst - last_adujst_time %lld video_adjustment %lld\n"
								,timeUs,cur_rtp_timeUs,last_timeUs,timeFirst,last_adujst_time,start_timeUs,timeFirst-cur_rtp_timeUs-timeUs,timeFirst - last_adujst_time,adjustment);

								fflush(rtp_ts_txt);
							}
							ALOGV("delay 300 ms timeUs %lld cur_rtp_timeUs %lld last_timeUs %lld timeFirst %lld adujst %lld start_timeUs %lld delta %lld\n"
								,timeUs,cur_rtp_timeUs,last_timeUs,timeFirst,last_adujst_time
								,start_timeUs,timeFirst-cur_rtp_timeUs-timeUs);
							timeUs +=((timeFirst - cur_rtp_timeUs - timeUs ) /23) *23;
							mInputBuffer->meta_data()->setInt64(kKeyTime, timeUs);
							set_flag = 1;
							last_adujst_time = timeFirst;
						}

						else
						{
							if(rtp_ts_txt != NULL)

							{

								fprintf(rtp_ts_txt," AACDecoder TimeStamp  delay 100-300 ms timeUs %lld cur_rtp_timeUs %lld last_timeUs %lld timeFirst %lld adujst %lld start_timeUs %lld delta %lld timeFirst - last_adujst_time %lld\n"
								,timeUs,cur_rtp_timeUs,last_timeUs,timeFirst,last_adujst_time,start_timeUs,timeFirst-cur_rtp_timeUs-timeUs,timeFirst - last_adujst_time);
								fflush(rtp_ts_txt);
							}
							ALOGV("delay 100-300 ms timeUs %lld cur_rtp_timeUs %lld last_timeUs %lld timeFirst %lld start_timeUs %lld delta %lld\n"
								,timeUs,cur_rtp_timeUs,last_timeUs,timeFirst,start_timeUs,timeFirst-cur_rtp_timeUs-timeUs);
						}


					}
					else
					{
						if(rtp_ts_txt != NULL)

						{

							fprintf(rtp_ts_txt," AACDecoder TimeStamp delay drop packets timeUs %lld cur_rtp_timeUs %lld last_timeUs %lld timeFirst %lld adujst %lld start_timeUs %lld delta %lld timeFirst - last_adujst_time %lld\n"
								,timeUs,cur_rtp_timeUs,last_timeUs,timeFirst,last_adujst_time,start_timeUs,timeFirst-cur_rtp_timeUs-timeUs,timeFirst - last_adujst_time);

						}
						if (mInputBuffer) {
							mInputBuffer->release();
							mInputBuffer = NULL;
						}
						continue;
					}
				}
				else
				{
					if(rtp_ts_txt != NULL)
					{
						fprintf(rtp_ts_txt," AACDecoder TimeStampdelay delay in 100ms timeUs %lld cur_rtp_timeUs %lld last_timeUs %lld timeFirst %lld adujst %lld start_timeUs %lld delta %lld timeFirst - last_adujst_time %lld\n"
								,timeUs,cur_rtp_timeUs,last_timeUs,timeFirst,last_adujst_time,start_timeUs,timeFirst-cur_rtp_timeUs-timeUs,timeFirst - last_adujst_time);
					}
				}
			}
			else
				#endif

			#if 1
			{

				if(last_timeUs < timeUs)
				{
					if(timeFirst - cur_rtp_timeUs  - adjustment < timeUs)//- 100000ll < timeUs)//loop tntil the real timeUs catch up with the old setted one  , if there is no data,the old setted is also faster than the real timeUs.so it's okay
					{
						if(timeUs > cur_video_timeUs + latency_time || Waiting_timeUs !=0)//recalculate the timeUs.
						{
							int64_t temptime = timeFirst;
							Waiting_timeUs = 0;
							while(timeUs > timeFirst - cur_rtp_timeUs  - adjustment)// - 100000ll)
							{
								if( timeFirst - temptime > 3000000ll )
								{
									Waiting_timeUs = 1;
									if(rtp_ts_txt != NULL)
									{
		fprintf(rtp_ts_txt,"AACDecoder timestamp reverse buffer timeFirst %15lld %15lld %15lld Rtp_Audio_Start_TimeUs %15lld Waiting_timeUs %15lld video_adjustment %15lld timeUs %15lld video_timeUs %15lld audio_timeUs %15lld delta %15lld %15lld %15lld %15lld %15lld %15lld %15lld %15lld %d\n"
		,temptime,last_adujst_time,timeFirst,cur_rtp_timeUs,Waiting_timeUs,adjustment,timeUs,cur_video_timeUs,audio_timeUs,temptime - cur_rtp_timeUs - cur_video_timeUs,
		temptime - cur_rtp_timeUs  - adjustment,temptime - cur_rtp_timeUs  -timeUs,
		timeFirst - cur_rtp_timeUs - cur_video_timeUs,timeFirst - cur_rtp_timeUs  - adjustment,
		timeFirst - cur_rtp_timeUs  -timeUs,	timeUs - cur_video_timeUs ,timeFirst - last_adujst_time,(timeUs > cur_video_timeUs  ));
		fflush(rtp_ts_txt);
		ALOGV("AACDecoder timestamp reverse buffer timeFirst %15lld %15lld %15lld Rtp_Audio_Start_TimeUs %15lld Waiting_timeUs %15lld video_adjustment %15lld timeUs %15lld video_timeUs %15lld audio_timeUs %15lld delta %15lld %15lld %15lld %15lld %15lld %15lld %15lld %15lld %d\n"
		,temptime,last_adujst_time,timeFirst,cur_rtp_timeUs,Waiting_timeUs,adjustment,timeUs,cur_video_timeUs,audio_timeUs,temptime - cur_rtp_timeUs - cur_video_timeUs,
		temptime - cur_rtp_timeUs  - adjustment,temptime - cur_rtp_timeUs  -timeUs,
		timeFirst - cur_rtp_timeUs - cur_video_timeUs,timeFirst - cur_rtp_timeUs  - adjustment,
		timeFirst - cur_rtp_timeUs  -timeUs,	timeUs - cur_video_timeUs ,timeFirst - last_adujst_time,(timeUs > cur_video_timeUs  ));
									}
									break;
								}
								usleep(10000);
								timeFirst = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
							}
							if(rtp_ts_txt != NULL)

							{

		fprintf(rtp_ts_txt,"AACDecoder timestamp after reverse buffer timeFirst %15lld %15lld %15lld Rtp_Audio_Start_TimeUs %15lld Waiting_timeUs %15lld video_adjustment %15lld timeUs %15lld video_timeUs %15lld audio_timeUs %15lld delta %15lld %15lld %15lld %15lld %15lld %15lld %15lld %15lld %d\n"
		,temptime,last_adujst_time,timeFirst,cur_rtp_timeUs,Waiting_timeUs,adjustment,timeUs,cur_video_timeUs,audio_timeUs,temptime - cur_rtp_timeUs - cur_video_timeUs,
		temptime - cur_rtp_timeUs  - adjustment,temptime - cur_rtp_timeUs  -timeUs,
		timeFirst - cur_rtp_timeUs - cur_video_timeUs,timeFirst - cur_rtp_timeUs  - adjustment,
		timeFirst - cur_rtp_timeUs  -timeUs,	timeUs - cur_video_timeUs ,timeFirst - last_adujst_time,(timeUs > cur_video_timeUs  ));
		fflush(rtp_ts_txt);
							ALOGV("AACDecoder timestamp after reverse buffer timeFirst %15lld %15lld %15lld Rtp_Audio_Start_TimeUs %15lld Waiting_timeUs %15lld video_adjustment %15lld timeUs %15lld video_timeUs %15lld audio_timeUs %15lld delta %15lld %15lld %15lld %15lld %15lld %15lld %15lld %15lld %d\n"
		,temptime,last_adujst_time,timeFirst,cur_rtp_timeUs,Waiting_timeUs,adjustment,timeUs,cur_video_timeUs,audio_timeUs,temptime - cur_rtp_timeUs - cur_video_timeUs,
		temptime - cur_rtp_timeUs  - adjustment,temptime - cur_rtp_timeUs  -timeUs,
		timeFirst - cur_rtp_timeUs - cur_video_timeUs,timeFirst - cur_rtp_timeUs  - adjustment,
		timeFirst - cur_rtp_timeUs  -timeUs,	timeUs - cur_video_timeUs ,timeFirst - last_adujst_time,(timeUs > cur_video_timeUs  ))	;
							}

					//		set_flag = 1;
						}

						else
						{
							if(rtp_ts_txt != NULL)
							{
							ALOGV("AACDecoder normal dec timeFirst %15lld Rtp_Audio_Start_TimeUs %15lld Waiting_timeUs %15lld video_adjustment %15lld timeUs %15lld video_timeUs %15lld delta %15lld %15lld %15lld %15lld %15lld %15lld %15lld\n"
		,timeFirst,cur_rtp_timeUs,Waiting_timeUs,adjustment,timeUs,cur_video_timeUs,timeFirst - cur_rtp_timeUs - cur_video_timeUs,
		timeFirst - cur_rtp_timeUs  - adjustment,timeFirst - cur_rtp_timeUs  -timeUs,timeUs - cur_video_timeUs);
				fprintf(rtp_ts_txt,"AACDecoder normal dec timeFirst %15lld Rtp_Audio_Start_TimeUs %15lld Waiting_timeUs %15lld video_adjustment %15lld timeUs %15lld video_timeUs %15lld delta %15lld %15lld %15lld %15lld %15lld %15lld %15lld\n"
		,timeFirst,cur_rtp_timeUs,Waiting_timeUs,adjustment,timeUs,cur_video_timeUs,timeFirst - cur_rtp_timeUs - cur_video_timeUs,
		timeFirst - cur_rtp_timeUs  - adjustment,timeFirst - cur_rtp_timeUs  -timeUs,timeUs - cur_video_timeUs);
				fflush(rtp_ts_txt);
							}
						}
					}
					else
					{
						if(rtp_ts_txt != NULL)
						{
						ALOGV("AACDecoder discard datus outofdate timeFirst %15lld Rtp_Audio_Start_TimeUs %15lld  video_adjustment %15lld timeUs %15lld video_timeUs %15lld delta %15lld %15lld %15lld %15lld %15lld %15lld %15lld\n"
		,timeFirst,cur_rtp_timeUs,adjustment,timeUs,cur_video_timeUs,timeFirst - cur_rtp_timeUs - cur_video_timeUs,
		timeFirst - cur_rtp_timeUs  - adjustment,timeFirst - cur_rtp_timeUs  -timeUs,timeUs - cur_video_timeUs);
		fprintf(rtp_ts_txt,"AACDecoder discard datus outofdate timeFirst %15lld Rtp_Audio_Start_TimeUs %15lld  video_adjustment %15lld timeUs %15lld video_timeUs %15lld delta %15lld %15lld %15lld %15lld %15lld %15lld %15lld\n"
		,timeFirst,cur_rtp_timeUs,adjustment,timeUs,cur_video_timeUs,timeFirst - cur_rtp_timeUs - cur_video_timeUs,
		timeFirst - cur_rtp_timeUs  - adjustment,timeFirst - cur_rtp_timeUs  -timeUs,timeUs - cur_video_timeUs);
		fflush(rtp_ts_txt);

						}
						Waiting_timeUs = 0;
						if (mInputBuffer) {
							mInputBuffer->release();
							mInputBuffer = NULL;
						}
					}
				}
				else
				{
					Waiting_timeUs = 0;
					if (mInputBuffer) {
						mInputBuffer->release();
						mInputBuffer = NULL;
					}
					if(rtp_ts_txt != NULL)
					{
					ALOGV("AACDecoder_MIRRORING TimeStamp loop again ms timeUs %lld cur_rtp_timeUs %lld last_timeUs %lld timeFirst %lld start_timeUs %lld \n"
			,timeUs,cur_rtp_timeUs,last_timeUs,timeFirst,start_timeUs);
		fprintf(rtp_ts_txt,"AACDecoder_MIRRORING TimeStamp loop again ms timeUs %lld cur_rtp_timeUs %lld last_timeUs %lld timeFirst %lld start_timeUs %lld \n"
			,timeUs,cur_rtp_timeUs,last_timeUs,timeFirst,start_timeUs);
		fflush(rtp_ts_txt);
					}

				}

			}



			#endif

	    }
	   	{
		   	int64_t timeUs;
		   	if (mInputBuffer->meta_data()->findInt64(kKeyTime, &timeUs)) {
	            if(mAnchorTimeUs != timeUs)
	            {
	            mAnchorTimeUs = timeUs;
	            mNumSamplesOutput = 0;
	            }
		    } else {
	            // We must have a new timestamp after seeking.
	            CHECK(seekTimeUs < 0);
	        }


		#if 0
			if(mAnchorTimeUs  > 184000ll)
		  		 mAnchorTimeUs -= 184000ll;
			else
				mAnchorTimeUs = 0;
		#endif
			last_timeUs = timeUs;
	   	}
		int ret;


		if((ret = access("data/test/aac_test_file",0)) == 0)//test_file!=NULL)
		{
			ALOGV("aac_test_file!=NULL aac_test_file %x",aac_test);
			if(aac_test !=NULL)
			{
				fwrite(mInputBuffer->data(),mInputBuffer->range_length(),1,aac_test);
			}
			else
				aac_test = fopen("data/test/aac_test","wb+");
		}
		else
		{
			if(aac_test!=NULL)
			{
				fclose(aac_test);
				aac_test = NULL;
			}
			ALOGV("aac_test==NULL ret %d",ret);
		}
    }


    MediaBuffer *buffer;
    CHECK_EQ(mBufferGroup->acquire_buffer(&buffer), (status_t)OK);

    mConfig->pInputBuffer =
        (UChar *)mInputBuffer->data() + mInputBuffer->range_offset();

    mConfig->inputBufferCurrentLength = mInputBuffer->range_length();
    mConfig->inputBufferMaxLength = 0;
    mConfig->inputBufferUsedLength = 0;
    mConfig->remainderBits = 0;

    mConfig->pOutputBuffer = static_cast<Int16 *>(buffer->data());
    mConfig->pOutputBuffer_plus = &mConfig->pOutputBuffer[2048];
	//ALOGE("inputlen %d input[0] = %x input[1]%x",  mConfig->inputBufferCurrentLength,(mConfig->pInputBuffer)[0],(mConfig->pInputBuffer)[1]);
    Int decoderErr = MP4AUDEC_SUCCESS;
	if(mConfig->isMutilChannle)
	{
		decoderErr = PVMP4AudioDecodeFrameSixChannel(mConfig, mDecoderBuf);
	}
	else
	{
		decoderErr = PVMP4AudioDecodeFrame(mConfig, mDecoderBuf);
	}


	if (mInputBuffer != NULL) {
		   mInputBuffer->set_range(
				   mInputBuffer->range_offset() + mConfig->inputBufferUsedLength,
				   mInputBuffer->range_length() - mConfig->inputBufferUsedLength);
		   if (mInputBuffer->range_length() <= 3) {
			   mInputBuffer->release();
			   mInputBuffer = NULL;
		   }
	   }

	//if the input data no enough,will drop this frame inputdata. get the next frame data.
	if(decoderErr != MP4AUDEC_SUCCESS)
	{

		 if(mInputBuffer)
		{
		   mInputBuffer->release();
		   mInputBuffer = NULL;
		}

		 if(buffer)
		{
			buffer->release();
			buffer = NULL;
		}
		 goto repeat;

	}

    /*
     * AAC+/eAAC+ streams can be signalled in two ways: either explicitly
     * or implicitly, according to MPEG4 spec. AAC+/eAAC+ is a dual
     * rate system and the sampling rate in the final output is actually
     * doubled compared with the core AAC decoder sampling rate.
     *
     * Explicit signalling is done by explicitly defining SBR audio object
     * type in the bitstream. Implicit signalling is done by embedding
     * SBR content in AAC extension payload specific to SBR, and hence
     * requires an AAC decoder to perform pre-checks on actual audio frames.
     *
     * Thus, we could not say for sure whether a stream is
     * AAC+/eAAC+ until the first data frame is decoded.
     */
    if (++mNumDecodedBuffers <= 2) {
        ALOGV("audio/extended audio object type: %d + %d",
            mConfig->audioObjectType, mConfig->extendedAudioObjectType);
        ALOGV("aac+ upsampling factor: %d desired channels: %d",
            mConfig->aacPlusUpsamplingFactor, mConfig->desiredChannels);

        CHECK(mNumDecodedBuffers > 0);
        if (mNumDecodedBuffers == 1) {
            mUpsamplingFactor = mConfig->aacPlusUpsamplingFactor;
            // Check on the sampling rate to see whether it is changed.
            int32_t sampleRate;
            CHECK(mMeta->findInt32(kKeySampleRate, &sampleRate));
			ALOGV("--->aac samplerae %d",sampleRate);
            if (mConfig->samplingRate != sampleRate) {
                mMeta->setInt32(kKeySampleRate, mConfig->samplingRate);
                ALOGV("Sample rate was %d Hz, but now is %d Hz",
                        sampleRate, mConfig->samplingRate);
                buffer->release();
               // mInputBuffer->release();
               // mInputBuffer = NULL;
                return INFO_FORMAT_CHANGED;
            }
        } else {  // mNumDecodedBuffers == 2
            if (mConfig->extendedAudioObjectType == MP4AUDIO_AAC_LC ||
                mConfig->extendedAudioObjectType == MP4AUDIO_LTP) {
                if (mUpsamplingFactor == 2) {
                    // The stream turns out to be not aacPlus mode anyway
                    ALOGV("Disable AAC+/eAAC+ since extended audio object type is %d",
                        mConfig->extendedAudioObjectType);
                    mConfig->aacPlusEnabled = 0;
                }
            } else {
                if (mUpsamplingFactor == 1) {
                    // aacPlus mode does not buy us anything, but to cause
                    // 1. CPU load to increase, and
                    // 2. a half speed of decoding
                    ALOGV("Disable AAC+/eAAC+ since upsampling factor is 1");
                    mConfig->aacPlusEnabled = 0;
                }
            }
        }
    }

    size_t numOutBytes =
        mConfig->frameLength * sizeof(int16_t) * mConfig->desiredChannels;
    if (mUpsamplingFactor == 2) {
        if (mConfig->desiredChannels == 1) {
            memcpy(&mConfig->pOutputBuffer[1024], &mConfig->pOutputBuffer[2048], numOutBytes * 2);
        }
        numOutBytes *= 2;
    }

    if (decoderErr != MP4AUDEC_SUCCESS) {
        ALOGE("AAC decoder returned error %d, substituting silence", decoderErr);

        memset(buffer->data(), 0, numOutBytes);

        // Discard input buffer.
        if(mInputBuffer)
        {
        mInputBuffer->release();
        mInputBuffer = NULL;
        }
        // fall through
    }

    buffer->set_range(0, numOutBytes);

	if(isAccpetSuccess)
	{

		Cur_TimeUs =  mAnchorTimeUs
            + (mNumSamplesOutput * 1000000) / mConfig->samplingRate;


	}
    buffer->meta_data()->setInt64(
            kKeyTime,
            mAnchorTimeUs
                + (mNumSamplesOutput * 1000000) / mConfig->samplingRate);

    mNumSamplesOutput += mConfig->frameLength;
	*out = buffer;
	{
		if(set_flag == 1)
		{
			ALOGD("read end memset to zero audio packet in case two  last_timeUs %lld %lld %lld curtime %lld startime %lld cur_rtp_timeUs %lld",
				last_timeUs,timeFirst - start_timeUs - last_timeUs,
				timeFirst - start_timeUs - last_timeUs,
				timeFirst, start_timeUs,cur_rtp_timeUs);
		 	memset( ( void *)buffer->data(),0, buffer->size());
		}

		{
			int retaac;
			if((retaac = access("data/test/aac_test_file",0)) == 0)//test_file!=NULL)
			{
				ALOGV("aac_test_file!=NULL aac_test_file %x",pcm_test_file);
				if(pcm_test_file !=NULL)
				{
					fwrite(buffer->data(),numOutBytes,1,pcm_test_file);
				}
				else
					pcm_test_file = fopen("data/test/pcm_test_file","wb");
			}
			else
			{
				if(pcm_test_file!=NULL)
				{
					fclose(pcm_test_file);
					aac_test_file = NULL;
				}
				ALOGV("pcm_test_file==NULL retavc %d",retaac);
			}


			if((retaac = access("data/test/rtp_ts_txt_file",0)) == 0)//test_file!=NULL)
			{

				if(rtp_ts_txt == NULL)
					rtp_ts_txt = fopen("data/test/rtp_ts_txt.txt","wb+");
				else
				{
					int64_t cur_time;
					static  int64_t last_audio_time_us;
					static  int64_t last_audio_systime;
					int64_t timeUs = last_timeUs;
					cur_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;

					fprintf(rtp_ts_txt,"AACDecoder_MIRRORING aac mNumSamplesOutput %lld %lld sys time %lld timeUs %lld cur_rtp_timeUs %lld temptimeus %lld delta sys time aa %lld delta time aa %lld  start_timeUs %lld buffer->size() %d set_flag %d decoderErr  %d \n",
						mNumSamplesOutput,mAnchorTimeUs
                + (mNumSamplesOutput * 1000000) / mConfig->samplingRate,cur_time,timeUs,cur_rtp_timeUs,temptimeus,cur_time - last_audio_systime,timeUs - last_audio_time_us,start_timeUs,numOutBytes,set_flag,decoderErr);
					last_audio_time_us = timeUs;
					last_audio_systime = cur_time;
					fflush(rtp_ts_txt);


				}
			}
			else
			{

				if(rtp_ts_txt!=NULL)
				{
					fclose(rtp_ts_txt);
					rtp_ts_txt = NULL;
				}
				ALOGV("rtp_ts_txt==NULL ret %d",retaac);
			}

		}

	}
    return OK;
}

}  // namespace android
