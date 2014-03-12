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
#define LOG_TAG "WimoSource"
#include <utils/Log.h>

#include <include/WimoSource.h>
#include <media/stagefright/foundation/ADebug.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <utils/CallStack.h>

#include <sys/poll.h>
#include <linux/tcp.h>

namespace android {
#define file_test
FILE* video_file;
FILE* video_header_file;
FILE* audio_file;
FILE* video_file_writeback;
FILE* video_header_file_writeback;
FILE* audio_file_writeback;
FILE* wimo_txt;


static int frame_num = -1;
static int last_frame_num=-1;
static int sequence_num=-1;
static int last_sequence_num=-1;
static int64_t last_sys_time = 0;
static int frame_count=0;
static int frame_lost=0;
static int frame_start=0;

#define WIMO_SRC_WRITE_DATA_DEBUG 1


int time  = 0;
WimoSource::WimoSource(const char *filename)
	:mFinalResult(1),
    connect_flag(1),
    mSocket_Audio(-1),
    mSocket_Video_Header(-1),
    mSocket_Video(-1),
    mOffset (0),
	mLength (-1),
	pack_num (0),
	head_offset (0),
	tail_offset (0),
	data_len (0),
	file_offset (0),
	mStartThreadFlag(false)

{

 	off64_t size;
	char *cur_idx ;
	char *next_idx ;
	char addr_idx[18];
	ALOGD("WimoSource::WimoSource");
	time = 0;
    video_header_file = video_file = audio_file = NULL;
	video_header_file_writeback = video_file_writeback = audio_file_writeback =NULL;
	wimo_txt = out_mFile = in_mFile  = NULL;


	  frame_num = -1;
	  last_frame_num=-1;
	  sequence_num=-1;
	  last_sequence_num=-1;
	  last_sys_time = 0;
	  frame_count=0;
	  frame_lost=0;
	  frame_start=0;
	//in_mFile = fopen("/data/in.ts","wb");
	//video_header_file = fopen("/data/video_header_file","rb");
	//out_mFile = fopen("/data/out.ts","wb");
	//video_file = fopen("/data/video_file","rb");
	//audio_file = fopen("/data/audio_file","rb");
	//video_header_file_writeback = fopen("/mnt/storage/video_header_file_writeback","wb");
	//video_file_writeback = fopen("/mnt/storage/video_file_writeback","wb");
	//audio_file_writeback = fopen("/mnt/storage/audio_file_writeback","wb");
	if(video_header_file_writeback == NULL || video_file_writeback == NULL || audio_file_writeback == NULL)
		ALOGD("write_back file can't be open");
	ALOGD("file op %x %x %x %x %x %x",video_header_file_writeback , video_file_writeback , audio_file_writeback
		,video_header_file , video_file , audio_file);

	mLength = 3000000;
	udp_packet_size = 64*1024;
	cache_size = 1310720 * 12;
	rec_buf = (uint8_t*)malloc(udp_packet_size + cache_size + 12);//1310924
	int i ;
	int flags;
	int addr_end = 0;
	if(strlen(filename) > 30)
		return;


 //   ALOGD("WimoSource::WimoSource %x filename %s local_port %d ip_addr %s\n",this,filename, local_port,ip_addr);
#if 1
	mSocket_Video = mSocket_Audio 	= -1;
	audio_port_remote 				= 0x812;
	video_port_remote 				= 0x814;
	video_header_port_remote 		= 0x813;



 	audio_port_local 				= 0x812;
	video_port_local 				= 0x814;
	video_header_port_local 		= 0x813;


	next_idx = strchr(filename,':');
	if(next_idx == NULL || next_idx - filename  > 16)
	{
		ALOGD("address is error");
		goto error_handle;
	}

	memcpy(addr_idx,filename,next_idx - filename);
	addr_idx[next_idx - filename]= '\0';
	ALOGD("addr_idx %s len %d",addr_idx,next_idx - filename);

	{

		mSocket_Audio = socket(AF_INET, SOCK_DGRAM , 0);
		if(mSocket_Audio <= 0)
		{
			connect_flag = 0;
	        ALOGE("receiver: failed to alloc mSocket_Video\n");
			return;
		}
	    int on = 1;
	    int rc;
	    rc = setsockopt(mSocket_Audio, SOL_SOCKET,  SO_REUSEADDR, (&on), sizeof(on));
	    if (rc < 0) {
	        ALOGE("receiver: failed to setsockopt\n");
		  	return ;
	    }
	    ALOGD("WimoSource::WimoSource %x filename %s audio_port_local %d ip_addr %s  mSocket_Audio %d\n",
			this,filename, audio_port_local,addr_idx,mSocket_Audio);
	    memset(mRTP_REC_Addr.sin_zero, 0, sizeof(mRTP_REC_Addr.sin_zero));
	    mRTP_REC_Addr.sin_family = AF_INET;//inet_addr("192.168.0.100");//AF_INET;

	    mRTP_REC_Addr.sin_addr.s_addr = INADDR_ANY;//inet_addr("192.168.0.100");//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;
	    if((flags = fcntl(mSocket_Audio, F_GETFL,O_NONBLOCK)) < 0)
		{

			ALOGD("fcntl nonblockF_GETFL error");
			goto error_handle;
		}
	    flags |= O_NONBLOCK;
	    if(fcntl(mSocket_Audio, F_SETFL,flags) < 0)
		{
			ALOGD("fcntl nonblockF_SETFL error");
			goto error_handle;
		}
	    mRTP_REC_Addr.sin_port = htons(audio_port_local);//htons(5666);//(local_port);
	    int err;
	    err =     bind(mSocket_Audio, (const struct sockaddr *)(&mRTP_REC_Addr),sizeof(mRTP_REC_Addr));
	    if(err!=0)
	    {
			ALOGD("bind ret %d audio_port_local %d errno %d",err,audio_port_local,errno);
			goto error_handle;

	    }
	    memset(mRTPAddr.sin_zero, 0, sizeof(mRTPAddr.sin_zero));
	    mRTPAddr.sin_family = AF_INET;

	    mRTPAddr.sin_addr.s_addr = inet_addr(addr_idx);// inet_addr("192.168.0.101");;//inet_addr(ip_addr);//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;
	    mRTPAddr.sin_port = htons(audio_port_remote);//htons(5666);//(local_port);
	    err = connect(mSocket_Audio,(const struct sockaddr *)( &mRTPAddr), sizeof(mRTPAddr));
	    ALOGD("connect err %d  errno %d %s",err,errno,addr_idx);

	}
	{

		mSocket_Video_Header= socket(AF_INET, SOCK_DGRAM , 0);
		if(mSocket_Video_Header <= 0)
		{
			connect_flag = 0;
	        ALOGE("receiver: failed to alloc mSocket_Video\n");
			return;
		}
	    int on = 1;
	    int rc;
	    rc = setsockopt(mSocket_Video_Header, SOL_SOCKET,  SO_REUSEADDR, (&on), sizeof(on));
	    if (rc < 0) {
	        ALOGE("receiver: failed to setsockopt\n");
		  	return ;
	    }
	    ALOGD("WimoSource::WimoSource %x filename %s videoheader_local %d ip_addr %s  mSocket_Video_Header %d\n",
			this,filename, video_header_port_local,addr_idx,mSocket_Video_Header);
	    memset(mRTP_REC_Addr.sin_zero, 0, sizeof(mRTP_REC_Addr.sin_zero));
	    mRTP_REC_Addr.sin_family = AF_INET;//inet_addr("192.168.0.100");//AF_INET;

	    mRTP_REC_Addr.sin_addr.s_addr = INADDR_ANY;//inet_addr("192.168.0.100");//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;
	    if((flags = fcntl(mSocket_Video_Header, F_GETFL,O_NONBLOCK)) < 0)
		{

			ALOGD("fcntl nonblockF_GETFL error");
			goto error_handle;
		}
	    flags |= O_NONBLOCK;
	    if(fcntl(mSocket_Video_Header, F_SETFL,flags) < 0)
		{
			ALOGD("fcntl nonblockF_SETFL error");
			goto error_handle;
		}
	    mRTP_REC_Addr.sin_port = htons(video_header_port_local);//htons(5666);//(local_port);
	    int err;
	    err =     bind(mSocket_Video_Header, (const struct sockaddr *)(&mRTP_REC_Addr),sizeof(mRTP_REC_Addr));
	    if(err!=0)
	    {
			ALOGD("bind ret %d audio_port_local %d errno %d",err,video_header_port_local,errno);
			goto error_handle;

	    }
	    memset(mRTPAddr.sin_zero, 0, sizeof(mRTPAddr.sin_zero));
	    mRTPAddr.sin_family = AF_INET;

	    mRTPAddr.sin_addr.s_addr = inet_addr(addr_idx);// inet_addr("192.168.0.101");;//inet_addr(ip_addr);//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;
	    mRTPAddr.sin_port = htons(video_header_port_remote);//htons(5666);//(local_port);
	    err = connect(mSocket_Video_Header,(const struct sockaddr *)( &mRTPAddr), sizeof(mRTPAddr));
	    ALOGD("video_header connect err %d  errno %d %s mRTP_REC_Addr.sin_port %d video_header_port_local %d",err,errno,addr_idx,mRTP_REC_Addr.sin_port,video_header_port_local);

	}
	{
	    mSocket_Video = socket(AF_INET, SOCK_DGRAM , 0);
		if(mSocket_Video <= 0)
		{
			connect_flag = 0;
	        ALOGE("receiver: failed to alloc mSocket_Video\n");
			return;
		}
	    int on = 1;
	    int rc;
	    rc = setsockopt(mSocket_Video, SOL_SOCKET,  SO_REUSEADDR, (&on), sizeof(on));
	    if (rc < 0) {
	        ALOGE("receiver: failed to setsockopt\n");
		  	return ;
	    }
	    ALOGD("WimoSource::WimoSource %x filename %s video_port_local %d ip_addr %s  mSocket_Video %d\n",this,filename, video_port_local,addr_idx,mSocket_Video);
	    memset(mRTP_REC_Addr.sin_zero, 0, sizeof(mRTP_REC_Addr.sin_zero));
	    mRTP_REC_Addr.sin_family = AF_INET;//inet_addr("192.168.0.100");//AF_INET;
	    mRTP_REC_Addr.sin_addr.s_addr = INADDR_ANY;//inet_addr("192.168.0.100");//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;
	    if((flags = fcntl(mSocket_Video, F_GETFL,O_NONBLOCK)) < 0)
		{

			ALOGD("fcntl nonblockF_GETFL error");
			goto error_handle;
		}
	    flags |= O_NONBLOCK;
	    if(fcntl(mSocket_Video, F_SETFL,flags) < 0)
		{
			ALOGD("fcntl nonblockF_SETFL error");
			goto error_handle;
		}
	    mRTP_REC_Addr.sin_port = htons(video_port_local);//htons(5666);//(local_port);
	    int err;
	    err =     bind(mSocket_Video, (const struct sockaddr *)(&mRTP_REC_Addr),sizeof(mRTP_REC_Addr));
	    if(err!=0)
	    {
			ALOGD("bind ret %d video_port_local %d errno %d",err,video_port_local,errno);
			goto error_handle;

	    }
	    memset(mRTPAddr.sin_zero, 0, sizeof(mRTPAddr.sin_zero));
	    mRTPAddr.sin_family = AF_INET;
	    mRTPAddr.sin_addr.s_addr = inet_addr(addr_idx);// inet_addr("192.168.0.101");;//inet_addr(ip_addr);//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;
	    mRTPAddr.sin_port = htons(video_port_remote);//htons(5666);//(local_port);
	    err = connect(mSocket_Video,(const struct sockaddr *)( &mRTPAddr), sizeof(mRTPAddr));
	    ALOGD("connect err %d  errno %d %s mRTPAddr.sin_addr.s_addr %x video_port_remote %d",err,errno,addr_idx,mRTPAddr.sin_addr.s_addr,video_port_remote);

	}



#endif





//	pthread_create(&mWimorecThread, NULL, rec_data_send, this);
//	  pthread_create(&mWimoThread, NULL, rec_data, this);





	return;
error_handle:
	connect_flag = 0;
	if(mSocket_Video >= 0)
	{
		close(mSocket_Video);
	  	mSocket_Video = -1;
	}
	if(mSocket_Audio >= 0)
	{
		close(mSocket_Audio);
	  	mSocket_Audio = -1;
	}
	if(mSocket_Video_Header>= 0)
	{
		close(mSocket_Video_Header);
	  	mSocket_Video_Header = -1;
	}
	ALOGD("Audio_output_source error audio_port_local %d errno %d",audio_port_local,errno);
	return;
}

WimoSource::WimoSource(unsigned long addr,unsigned short rx_port,unsigned short tx_port)
{
	#ifdef file_test
//	rec_buf = (uint8_t*)malloc(0xc18dc + 188 * 6);


	//out_mFile = fopen("/data/out.ts","wb");
	#endif
}

WimoSource::~WimoSource() {
    ALOGD("WimoSource::~WimoSource start");



	connect_flag = 0;

    while(do_end == 0)
    {
    	usleep(10000);
    }

    ALOGI("stop wimo recive thread ok");

    if(mSocket_Video >= 0)
	{
		close(mSocket_Video);
		mSocket_Video = -1;
		ALOGD("rec_data end_flag %d connect_flag %d mSocket_Video %d",end_flag,connect_flag,mSocket_Video);
	}
	if(mSocket_Audio >= 0)
	{
		close(mSocket_Audio);
	  	mSocket_Audio = -1;
	}
	if(mSocket_Video_Header>= 0)
	{
		close(mSocket_Video_Header);
	  	mSocket_Video_Header = -1;
	}

	if(audio_file!=NULL)
		fclose(audio_file);
	if(video_file!=NULL)
		fclose(video_file);
    if(out_mFile != NULL)
		fclose(out_mFile);
    if(in_mFile!=NULL)
		fclose(in_mFile);

    if(rec_buf)
		free(rec_buf);

    ALOGI("wimo source deconstruct complete....");

}

ssize_t WimoSource::seek( off64_t offset, int whence)
{
	return OK;
}
status_t WimoSource::initCheck() const {

    return OK;
}
void* WimoSource::rec_data_send(void* me)
{
	WimoSource* wimoSource = static_cast<WimoSource *>(me);
	ALOGD("%x wimoSource %x",me,wimoSource);
	wimoSource->ThreadWrapper_send(NULL);
	return NULL;
}


void *WimoSource::ThreadWrapper_send(void *)
{
#if 0
	int ret = 0;
	int64_t timeFirst,timesec;
	unsigned char* send_video_buf = (unsigned char*)malloc(128*1024);
	unsigned char* send_video_header_buf = (unsigned char*)malloc(128*1024);
	unsigned char* send_audio_buf = (unsigned char*)malloc(128*1024);
	int	video_offset = 0;
	int	video_header_offset = 0;
	int	audio_offset = 0;
	int	first_header_video_sync = 0;
	int	first_video_sync = 0;
	int	first_audio_sync = 0;
	int audio_data_len = 0;
	int video_data_len = 0;
	int video_header_data_len = 0;
	int	temp1 ,temp2;
	int64_t timestamp;
	ALOGD("before ThreadWrapper_send loop");
	while(connect_flag)
	{


		time++;
		ALOGV("time %d ftell(video_header_file) %d" ,time,ftell(video_header_file));
		video_header_data_len = 0;
		video_data_len = 0;
		{
			Mutex::Autolock autoLock(mLock);
			if(0)//video_file_writeback)
			{

				if(first_header_video_sync == 0)
				{
					ALOGD("looking for first video_header");
					if(video_header_data_len == 0)
					{
						video_header_offset = 0;
						video_header_data_len = 64;
						if(fread(send_video_header_buf,64,1,video_header_file) == 0)
						{
							video_header_data_len = 0;
							first_header_video_sync = 0;
							fseek(video_header_file,0,SEEK_SET);
							ALOGD("at the end of video_header_file 1");
							continue;
						}
					}
					while(1)
					{

								ALOGD("len %d video_header_offset %d data %c%c%c%c%c%c%c%c   %c%c%c%c%c%c%c%c ",video_header_data_len,video_header_offset,
							send_video_header_buf[0],send_video_header_buf[1],send_video_header_buf[2],send_video_header_buf[3],send_video_header_buf[4],send_video_header_buf[5],send_video_header_buf[6],send_video_header_buf[7],
							send_video_header_buf[video_header_offset],send_video_header_buf[video_header_offset+1],
							send_video_header_buf[video_header_offset+2],
							send_video_header_buf[video_header_offset+3] , send_video_header_buf[video_header_offset+4],
							send_video_header_buf[video_header_offset+5] ,
						send_video_header_buf[video_header_offset+6] ,send_video_header_buf[video_header_offset+7]
						);
						if(send_video_header_buf[video_header_offset] == 'R' && send_video_header_buf[video_header_offset+1] == 'K'&&
							send_video_header_buf[video_header_offset+2] == 'V'
							&& send_video_header_buf[video_header_offset+3] == 'B'&& send_video_header_buf[video_header_offset+4] == 'A'&&
							send_video_header_buf[video_header_offset+5] == 'A'
							&& send_video_header_buf[video_header_offset+6] == 'A'&& send_video_header_buf[video_header_offset+7] == 'A')
						{
							first_header_video_sync = 1;
							fseek(video_header_file,-video_header_data_len,SEEK_CUR);
							ALOGD("find first video_header ftell %d video_header_data_len %d",ftell(video_header_file),video_header_data_len);
							break;
						}
						video_header_offset++;
						video_header_data_len--;
						if(video_header_data_len == 8)
						{
							fseek(video_header_file,-video_header_data_len,SEEK_CUR);
							video_header_data_len = 0;
							break;
						}

					}

					if(first_header_video_sync == 0)
						continue;
				}


				if(video_header_data_len==0)
				{
					video_header_offset = 8;
					video_header_data_len =56;


					if(fread(send_video_header_buf,64,1,video_header_file) == 0)
					{
						first_header_video_sync = 0;
						video_header_data_len = 0;
						fseek(video_header_file,0,SEEK_SET);
						ALOGD("at the end of video_header_file");
						continue;
					}
				}
				if(send_video_header_buf[0] == 'R' && send_video_header_buf[1] == 'K'&&
send_video_header_buf[2] == 'V'	&& send_video_header_buf[3] == 'B'&& send_video_header_buf[4] == 'A'&&
send_video_header_buf[5] == 'A'	&& send_video_header_buf[6] == 'A'&& send_video_header_buf[7] == 'A')
				{
					//ALOGD("case 1 fetll %d %c",ftell(video_header_file),send_video_header_buf[0]);
					while(video_header_data_len >=8)
					{

						if(send_video_header_buf[video_header_offset] == 'R' && send_video_header_buf[video_header_offset+1] == 'K'&&
send_video_header_buf[video_header_offset+2] == 'V'	&& send_video_header_buf[video_header_offset+3] == 'B'&& send_video_header_buf[video_header_offset+4] == 'A'&&
send_video_header_buf[video_header_offset+5] == 'A'	&& send_video_header_buf[video_header_offset+6] == 'A'&& send_video_header_buf[video_header_offset+7] == 'A')
						{


							memcpy(&rec_buf[tail_offset], send_video_header_buf,video_header_offset);//recvfrom(mSocket_Video, &rec_buf[tail_offset], 188 * 6, 0,  (const struct sockaddr *)(&mRTP_REC_Addr),  &len);




							if(tail_offset + video_header_offset > cache_size)
							{
								memcpy(rec_buf,&rec_buf[cache_size],tail_offset + video_header_offset - cache_size);

							}

							if(data_len + video_header_offset < cache_size)
							{
								data_len += video_header_offset;
								tail_offset += video_header_offset;
								tail_offset %=cache_size;
							}
							else
							{
								ALOGD("buffer is full tail_offset %d head_offset %d datalen %d",tail_offset,head_offset,data_len);
							}




							fwrite(send_video_header_buf,video_header_offset,1,video_header_file_writeback);
							fseek(video_header_file,-video_header_data_len,SEEK_CUR);
							ALOGD("len %d video_header_offset %d  time %d fetll %d %2x temp %d %d ret %d",
								video_header_data_len,video_header_offset,time,ftell(video_header_file)
								,send_video_header_buf[13],temp1,temp2,ret);

							memcpy(&timestamp,&send_video_header_buf[8],8);
							break;
						}
						video_header_offset++;
						video_header_data_len--;
					}
					if(video_header_data_len <8)
					{
						ALOGD("error in bitstream audio_data_len = 0 time %d",time);
						video_header_data_len = 0;
						first_header_video_sync = 0;
					}

				}
				else
				{
					ALOGD(" case 2 fetll %d  time %d temp %d %d",ftell(video_header_file),time,temp1,temp2);
					first_header_video_sync 	= 	0;
					video_header_data_len 		= 	0;
					fseek(video_header_file,-video_header_data_len,SEEK_CUR);
					//ALOGD("error in bitstream no sync header");
				}
			}
			ALOGV("time %d ftell(video_file) %d" ,time,ftell(video_file));
			if(video_file)
			{
				int	find_two_frame = 0;

				while(find_two_frame == 0)
				{
					video_data_len = 0;
					if(first_video_sync == 0)
					{

						{
							ALOGD("looking for first video");
							if(video_data_len == 0)
							{
								video_offset = 0;
								video_data_len = 64 * 1024;
								if(fread(send_video_buf,64 * 1024,1,video_file) == 0)
								{
									video_data_len = 0;
									first_video_sync = 0;
									fseek(video_file,0,SEEK_SET);
									ALOGD("at the end of video_file 1");
									continue;
								}
							}
							while(video_data_len >= 8)
							{

								ALOGD("len %d video_offset %d data %c%c%c%c%c%c%c%c   %c%c%c%c%c%c%c%c ",video_data_len,video_offset,
									send_video_buf[0],send_video_buf[1],send_video_buf[2],send_video_buf[3],send_video_buf[4],send_video_buf[5],send_video_buf[6],send_video_buf[7],
									send_video_buf[video_offset],send_video_buf[video_offset+1],
									send_video_buf[video_offset+2],send_video_buf[video_offset+3] , send_video_buf[video_offset+4],
									send_video_buf[video_offset+5] ,send_video_buf[video_offset+6] ,send_video_buf[video_offset+7]
								);
								if(send_video_buf[video_offset] == 'R' && send_video_buf[video_offset+1] == 'K'&&
									send_video_buf[video_offset+2] == 'V'
									&& send_video_buf[video_offset+3] == 'B'&& send_video_buf[video_offset+4] == 'B'&&
									send_video_buf[video_offset+5] == 'B'
									&& send_video_buf[video_offset+6] == 'B'&& send_video_buf[video_offset+7] == 'B')
								{
									first_video_sync = 1;
									fseek(video_file,-video_data_len,SEEK_CUR);
									ALOGD("find first video ftell %d video_data_len %d",ftell(video_file),video_data_len);
									break;
								}
								video_offset++;
								video_data_len--;


							}
							if(video_data_len < 8)
							{
								fseek(video_file,-video_data_len,SEEK_CUR);
								video_data_len = 0;
								break;
							}

						}
						if(first_video_sync == 0)
								continue;
					}

					if(video_data_len==0)
					{
						video_offset = 8;
						video_data_len =64*1024-8;



						if((ret = fread(send_video_buf,64*1024,1,video_file)) == 0)
						{
							ALOGD("at the end of video_file fread ret size %d fetll %d",ret,ftell(video_file));
							first_video_sync = 0;
							video_data_len = 0;
							fseek(video_file,0,SEEK_SET);
							continue;
						}
						ALOGV("fread ret size %d fetll %d",ret,ftell(video_file));
					}
					if(send_video_buf[0] == 'R' && send_video_buf[1] == 'K'&&
						send_video_buf[2] == 'V'	&& send_video_buf[3] == 'B'&& send_video_buf[4] == 'B'&&
						send_video_buf[5] == 'B'	&& send_video_buf[6] == 'B'&& send_video_buf[7] == 'B')
					{
						ALOGV("case 1 fetll %d %c video_offset %d video_data_len %d",
							ftell(video_file),send_video_buf[0],video_offset,video_data_len);
						while(video_data_len >=8)
						{

							if(send_video_buf[video_offset] == 'R' && send_video_buf[video_offset+1] == 'K'&&
								send_video_buf[video_offset+2] == 'V'
								&& send_video_buf[video_offset+3] == 'B'&& send_video_buf[video_offset+4] == 'B'&&
								send_video_buf[video_offset+5] == 'B'
								&& send_video_buf[video_offset+6] == 'B'&& send_video_buf[video_offset+7] == 'B')
							{


								memcpy(&rec_buf[tail_offset], send_video_buf,video_offset);//recvfrom(mSocket_Video, &rec_buf[tail_offset], 188 * 6, 0,  (const struct sockaddr *)(&mRTP_REC_Addr),  &len);




								if(tail_offset + video_offset > cache_size)
								{
									memcpy(rec_buf,&rec_buf[cache_size],tail_offset + video_offset - cache_size);

								}

								if(data_len + video_offset < cache_size)
								{
									data_len += video_offset;
									tail_offset += video_offset;
									tail_offset %=cache_size;
								}
								else
								{
									ALOGV("buffer is full tail_offset %d head_offset %d datalen %d",tail_offset,head_offset,data_len);
								}




								fwrite(send_video_buf,video_offset,1,video_file_writeback);
								fseek(video_file,-video_data_len,SEEK_CUR);
								ALOGV("len %d video_offset %d  time %d fetll %d  temp %d %d %d %d %d %d %d %d",
									video_data_len,video_offset,time,ftell(video_file)
									,send_video_buf[video_offset+8], send_video_buf[8], send_video_buf[video_offset+9], send_video_buf[9]
									,send_video_buf[10],send_video_buf[11],send_video_buf[video_offset+10],send_video_buf[video_offset+11]);
								if(((send_video_buf[video_offset+8]!= send_video_buf[8] )|| (send_video_buf[video_offset+9] != send_video_buf[9]))
									&&		(send_video_buf[10] & 0x80) )
								{
									find_two_frame = 1;
									ALOGD("newframe len %d video_offset %d  time %d fetll %d  temp %d %d %d %d %d %d value %d",
									video_data_len,video_offset,time,ftell(video_file)
									,send_video_buf[video_offset+8], send_video_buf[8], send_video_buf[video_offset+9], send_video_buf[9]
									,send_video_buf[10],send_video_buf[11]);
								}
								memcpy(&timestamp,&send_video_buf[8],8);
								break;
							}
							video_offset++;
							video_data_len--;
						}
						if(video_data_len <8)
						{
							ALOGD("error in bitstream audio_data_len = 0 time %d",time);
							video_data_len = 0;
							first_video_sync = 0;
						}

					}
					else
					{
						ALOGD(" case 2 fetll %d  time %d temp %d %d video_data_len %d",ftell(video_file),time,temp1,temp2,video_data_len);
						first_video_sync 	= 	0;
						video_data_len 		= 	0;
						fseek(video_file,-video_data_len,SEEK_CUR);
						ALOGD("error in bitstream no sync header video_data_len %d ftell(video_file) %d",
							video_data_len,ftell(video_file));
					}

				}

			}
			ALOGV("time %d ftell(audio_file) %d" ,time,ftell(audio_file));
			if(0)//audio_file)
			{
				int	find_two_frame = 0;
				int i;
				//while(find_two_frame == 0)
			//	for(i = 0; i< 2; i++)
				{
					audio_data_len = 0;
					if(first_audio_sync == 0)
					{

						{
							ALOGD("looking for first audio");
							if(audio_data_len == 0)
							{
								audio_offset = 0;
								audio_data_len = 2 * 1024;
								if(fread(send_audio_buf,2 * 1024,1,audio_file) == 0)
								{
									audio_data_len = 0;
									first_audio_sync = 0;
									fseek(audio_file,0,SEEK_SET);
									ALOGD("at the end of audio_file 1");
									continue;
								}
							}
							while(audio_data_len >= 8)
							{

								ALOGV("len %d audio_offset %d data %c%c%c%c%c%c%c%c   %c%c%c%c%c%c%c%c ",audio_data_len,audio_offset,
									send_audio_buf[0],send_audio_buf[1],send_audio_buf[2],send_audio_buf[3],send_audio_buf[4],
									send_audio_buf[5],send_audio_buf[6],send_audio_buf[7],
									send_audio_buf[audio_offset],send_audio_buf[audio_offset+1],
									send_audio_buf[audio_offset+2],send_audio_buf[audio_offset+3] , send_audio_buf[audio_offset+4],
									send_audio_buf[audio_offset+5] ,send_audio_buf[audio_offset+6] ,send_audio_buf[audio_offset+7]
								);
								if(send_audio_buf[audio_offset] == 'R' && send_audio_buf[audio_offset+1] == 'K'&&
									send_audio_buf[audio_offset+2] == 'A'
									&& send_audio_buf[audio_offset+3] == 'B'&& send_audio_buf[audio_offset+4] == 'B'&&
									send_audio_buf[audio_offset+5] == 'B'
									&& send_audio_buf[audio_offset+6] == 'B'&& send_audio_buf[audio_offset+7] == 'B')
								{
									first_audio_sync = 1;
									fseek(audio_file,-audio_data_len,SEEK_CUR);
									ALOGV("find first audio ftell %d audio_data_len %d",ftell(audio_file),audio_data_len);
									break;
								}
								audio_offset++;
								audio_data_len--;


							}
							if(audio_data_len < 8)
							{
								fseek(audio_file,-audio_data_len,SEEK_CUR);
								audio_data_len = 0;
								break;
							}

						}
						if(first_audio_sync == 0)
								continue;
					}

					if(audio_data_len==0)
					{
						audio_offset = 8;
						audio_data_len =2*1024-8;



						if(fread(send_audio_buf,2*1024,1,audio_file) == 0)
						{
							first_audio_sync = 0;
							audio_data_len = 0;
							fseek(audio_file,0,SEEK_SET);
							ALOGD("at the end of audio_file");
							continue;
						}
					}
					if(send_audio_buf[0] == 'R' && send_audio_buf[1] == 'K'&&
						send_audio_buf[2] == 'A'	&& send_audio_buf[3] == 'B'&& send_audio_buf[4] == 'B'&&
						send_audio_buf[5] == 'B'	&& send_audio_buf[6] == 'B'&& send_audio_buf[7] == 'B')
					{
						ALOGV("case 1 fetll %d %c %c video_offset %d",ftell(audio_file),send_audio_buf[0],
							send_audio_buf[audio_offset]  ,audio_offset);
						while(audio_data_len >=8)
						{

							if(send_audio_buf[audio_offset] == 'R' && send_audio_buf[audio_offset+1] == 'K'&&
								send_audio_buf[audio_offset+2] == 'A'
								&& send_audio_buf[audio_offset+3] == 'B'&& send_audio_buf[audio_offset+4] == 'B'&&
								send_audio_buf[audio_offset+5] == 'B'
								&& send_audio_buf[audio_offset+6] == 'B'&& send_audio_buf[audio_offset+7] == 'B')
							{


								memcpy(&rec_buf[tail_offset], send_audio_buf,audio_offset);//recvfrom(mSocket_Video, &rec_buf[tail_offset], 188 * 6, 0,  (const struct sockaddr *)(&mRTP_REC_Addr),  &len);




								if(tail_offset + audio_offset > cache_size)
								{
									memcpy(rec_buf,&rec_buf[cache_size],tail_offset + audio_offset - cache_size);

								}

								if(data_len + audio_offset < cache_size)
								{
									data_len += audio_offset;
									tail_offset += audio_offset;
									tail_offset %=cache_size;
								}
								else
								{
									ALOGD("buffer is full tail_offset %d head_offset %d datalen %d",tail_offset,head_offset,data_len);
								}


								fwrite(send_audio_buf,audio_offset,1,audio_file_writeback);
								fseek(audio_file,-audio_data_len,SEEK_CUR);
								ALOGD("len %d audio_offset %d  time %d fetll %d  temp %d %d %d %d %d %d value %d",
									audio_data_len,audio_offset,time,ftell(audio_file)
									,send_audio_buf[audio_offset+8], send_audio_buf[8], send_audio_buf[audio_offset+9], send_audio_buf[9]
									,send_audio_buf[audio_offset+10],send_audio_buf[audio_offset+11]);

								memcpy(&timestamp,&send_audio_buf[8],8);
								break;
							}
							audio_offset++;
							audio_data_len--;
						}
						if(audio_data_len <8)
						{
							ALOGD("error in bitstream audio_data_len = 0 time %d",time);
							audio_data_len = 0;
							first_audio_sync = 0;
						}

					}
					else
					{
						ALOGD(" case 2 fetll %d  time %d temp %d %d",ftell(audio_file),time,temp1,temp2);
						first_audio_sync 	= 	0;
						audio_data_len 		= 	0;
						fseek(audio_file,-audio_data_len,SEEK_CUR);
						//ALOGD("error in bitstream no sync header");
					}

				}

			}
		}
		usleep(30000);


	}
error_no_file:
	if(send_video_buf)
		free(	send_video_buf);
	if(send_video_header_buf)
		free(	send_video_header_buf);
	if(send_audio_buf)
		free(send_audio_buf);
	pthread_exit (0);
#endif
	return NULL;
}
void* WimoSource::rec_data(void* me)
{

	WimoSource* wimoSource = static_cast<WimoSource *>(me);
	ALOGD("%x WimoSource %x",me,wimoSource);
	wimoSource->ThreadWrapper(NULL);
	return NULL;
}

void *WimoSource::ThreadWrapper(void *)
{
	int ret = 0;
	int64_t timeFirst,timesec;
	int	receive_data=0;
	struct pollfd fds_rtp[3];

	fds_rtp[0].fd = mSocket_Video_Header;
	fds_rtp[0].events = POLLIN;
	fds_rtp[1].fd = mSocket_Video;
	fds_rtp[1].events = POLLIN;
	fds_rtp[2].fd = mSocket_Audio;
	fds_rtp[2].events = POLLIN;
	while(connect_flag)
	{

		if((ret = poll(fds_rtp, 3, 20)) > 0)
		{
			int i;
			for(i = 0; i < 3; i++)
			{
				if(fds_rtp[i].revents & POLLIN)
				{

					do
					{
						Mutex::Autolock autoLock(mLock);
		                int pktHdrSize = 12;
						ret = recv(fds_rtp[i].fd, &rec_buf[tail_offset + pktHdrSize], udp_packet_size, 0);//recvfrom(mSocket_Video, &rec_buf[tail_offset], 188 * 6, 0,  (const struct sockaddr *)(&mRTP_REC_Addr),  &len);
						ALOGV("video socket receive ret %d", ret);
						if(ret<-1||ret ==0)
						{
							ALOGV("mSocket_Video ret = %d",ret);

							connect_flag = 0;
							DataRecCondition.signal();
							break;
						}
						if(ret > 0)
						{
		                    /* add video stream header to buffer */
						//	if(rec_buf[tail_offset + 16] == 0xFF && rec_buf[tail_offset + 17] == 0xD9)
		                  //  memcpy(&rec_buf[tail_offset], "RKVBAAAA", 8);

							//else
							if(i==2)

								memcpy(&rec_buf[tail_offset], "RKABAAAA", 8);
							else	if(i == 0)
								memcpy(&rec_buf[tail_offset], "RKVBAAAA", 8);

							else	if(i==1)
								memcpy(&rec_buf[tail_offset], "RKVBBBBB", 8);
		                    /* copy every frame size to the header */
		                    memcpy(&rec_buf[tail_offset+8], &ret, 4);
							if(i == 2)//i == 1)
							{
								int retrtptxt;
								if((retrtptxt = access("data/test/out_mFile_file",0)) == 0)//test_file!=NULL)
								{
									if(out_mFile == NULL)
										out_mFile = fopen("data/test/out_mFile","wb");
				                    if (out_mFile != NULL) {
				                        fwrite(rec_buf + tail_offset  , 1, ret+12 , out_mFile);
				                        fflush(out_mFile);
				                    }
								}
							}
							if(i == 2)
							{
								int retrtptxt;

								int j=0;
								uint16_t *buf = (uint16_t *)(rec_buf + tail_offset + pktHdrSize + 4);
								for(j = 0; j < (ret-4)/2;j++)
								{
									buf[j] = ((((buf[j])>>8)&0xff)|((buf[j] << 8)&0xff00));
								}
								if((retrtptxt = access("data/test/in_mFile_file",0)) == 0)//test_file!=NULL)
								{
									if(in_mFile == NULL)
										in_mFile = fopen("data/test/in_mFile","wb");
				                    if (in_mFile != NULL) {

										ALOGD("audio ret %d",ret);

								//		if(!(rec_buf[tail_offset + pktHdrSize +  4]==0xFF && rec_buf[tail_offset + pktHdrSize +  5]==0xD9 ))
											fwrite(rec_buf + tail_offset + pktHdrSize + 4, 1, ret - 4, in_mFile);//(rec_buf + tail_offset , 1, ret + 12, in_mFile);//(rec_buf + tail_offset + pktHdrSize + 4, 1, ret - 4, in_mFile);

				                        fflush(in_mFile);
				                    }
								}
							}

							{
								int retrtptxt;
								if((retrtptxt = access("data/test/wimo_txt_file",0)) == 0)//test_file!=NULL)
								{

									int64_t sys_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
									if(wimo_txt == NULL)
										wimo_txt = fopen("data/test/wimo_txt","wb");
				                    if (wimo_txt != NULL) {
										static int cur_framed8_num = 0;
										static int64_t last_time=0;
										static int64_t last_time_d8=0;
										if(rec_buf[tail_offset + pktHdrSize  + 4]==0xFF && rec_buf[tail_offset + pktHdrSize + 5]==0xD8 )
										{
											static int frame_11 = 0;

											cur_framed8_num =( rec_buf[tail_offset + pktHdrSize] << 8) | rec_buf[tail_offset + pktHdrSize+1];
											last_time_d8 = sys_time;
											ALOGD("Wimover1 recv FFD8 data time %15lld cur_framed8_num %d data_len %d\n",
											 sys_time,cur_framed8_num,data_len);
											fprintf(wimo_txt,"Wimover1 recv FFD8 data time %15lld cur_framed8_num %d data_len %d\n",
											 sys_time,cur_framed8_num,data_len);
										}
										if(rec_buf[tail_offset + pktHdrSize  + 2]&0x80 )
										{
											static int frame_11 = 0;
											if(cur_framed8_num == ( rec_buf[tail_offset + pktHdrSize] << 8) | rec_buf[tail_offset + pktHdrSize+1])
											{
												ALOGD("Wimover1 recv FFD9 data time %15lld  %15lld %15lld delta %15lld  %15lld cur_framed8_num %d rec_buf[tail_offset + pktHdrSize  + 3] %d\n",
											 		last_time_d8,last_time,sys_time,sys_time- last_time_d8,sys_time - last_time,cur_framed8_num,rec_buf[tail_offset + pktHdrSize  + 3]);
												fprintf(wimo_txt,"Wimover1 recv FFD9 data time %15lld  %15lld %15lld delta %15lld  %15lld cur_framed8_num %d rec_buf[tail_offset + pktHdrSize  + 3] %d\n",
											 		last_time_d8,last_time,sys_time,sys_time- last_time_d8,sys_time - last_time,cur_framed8_num,rec_buf[tail_offset + pktHdrSize  + 3]);
												last_time = sys_time;
											}
											else
												ALOGD("Wimover11 recv FFD9 data time %15lld  %15lld %15lld delta %15lld  %15lld cur_framed8_num %d rec_buf[tail_offset + pktHdrSize  + 3] %d\n",
											 		last_time_d8,last_time,sys_time,sys_time- last_time_d8,sys_time - last_time,cur_framed8_num,rec_buf[tail_offset + pktHdrSize  + 3]);


										}
											{
												int frame_num =( rec_buf[tail_offset + pktHdrSize] << 8) | rec_buf[tail_offset + pktHdrSize+1];
												int seq_num = rec_buf[tail_offset + pktHdrSize+3];
		if(rec_buf[tail_offset + pktHdrSize  + 4]==0xFF && rec_buf[tail_offset + pktHdrSize + 5]==0xD8 )
			fprintf(wimo_txt,"Wimover1 D8 recv data time %15lld frame_num %d seq_num %d\n",
				 sys_time,frame_num,seq_num);

		else if(rec_buf[tail_offset + pktHdrSize  + 2]&0x80 )

		fprintf(wimo_txt,"Wimover1 D9 recv data time %15lld frame_num %d seq_num %d\n",
			 sys_time,frame_num,seq_num);
		else
		fprintf(wimo_txt,"Wimover1 normal recv data time %15lld frame_num %d seq_num %d\n",
			 sys_time,frame_num,seq_num);
											}
										if(rec_buf[tail_offset + pktHdrSize  + 4]==0xFF && rec_buf[tail_offset + pktHdrSize + 5]==0xD9 )
										{


											last_frame_num = frame_num;
											frame_num = (*(rec_buf + tail_offset + pktHdrSize  ) << 8) |
												*(rec_buf + tail_offset + pktHdrSize  + 1);
											if(frame_num == -1)
												frame_start =  frame_num;
											else if(sequence_num == -1 && last_sequence_num == -1)
												frame_lost +=1;





											frame_lost += (frame_num - last_frame_num - 1);
											fprintf(wimo_txt,"Wimover1 recv data time %15lld %15lld delta %15lld frame_num %d %d delta %d sequence %d %d framecount %d framelost %d\n",
											 last_sys_time,sys_time,sys_time - last_sys_time,last_frame_num,frame_num,frame_num- last_frame_num,sequence_num,last_sequence_num,
											 frame_num - frame_start,frame_lost);
											ALOGV("Wimover1 recv data time %15lld %15lld delta %15lld frame_num %d %d delta %d sequence %d %d framecount %d framelost %d\n",
											 last_sys_time,sys_time,sys_time - last_sys_time,last_frame_num,frame_num,frame_num- last_frame_num,sequence_num,last_sequence_num,
											 frame_num - frame_start,frame_lost);
											fflush(wimo_txt);
											last_sys_time = sys_time;
											sequence_num = -1;
											last_sequence_num = -1;

										}
										else if (rec_buf[tail_offset + pktHdrSize  + 4]==0xFF && rec_buf[tail_offset + pktHdrSize + 5]==0xD8 )
										{
											sequence_num = 0;
											last_sequence_num = -1;
										}
										else if(!(sequence_num == -1 && last_sequence_num == -1))
										{
												ALOGV("sequence_num %d %d frame_num %d ",sequence_num,last_sequence_num,frame_num);
												if(sequence_num != last_sequence_num + 1)
												{
													sequence_num = -1;
													last_sequence_num = -1;
												}
												else
												{
													last_sequence_num = sequence_num;
													sequence_num = (*(rec_buf + tail_offset + pktHdrSize + 2  ) << 8) |
														*(rec_buf + tail_offset + pktHdrSize +3 );
												}
										}

				                    }
								}

							}
		                    ALOGV("socket video has data, ret = %d  ",ret);
		                    ret +=pktHdrSize;
							if(tail_offset + ret > cache_size)
							{
								memcpy(rec_buf,&rec_buf[cache_size],tail_offset + ret- cache_size);

							}

							if(data_len + ret < cache_size)
							{
								data_len += ret;
								tail_offset += ret;
								tail_offset %=cache_size;
							}
							else
							{
								ALOGD("buffer is full tail_offset %d head_offset %d datalen %d",
								    tail_offset,head_offset,data_len);
								data_len = tail_offset = head_offset = 0;

							}

						}

						else
							ALOGV("ret = %d  ",ret);
					}while(ret > 0);
				}
				if (fds_rtp[i].revents & POLLHUP)
				{
					ALOGD(" disconnect fd %d    %d",fds_rtp[0].revents,fds_rtp[0].revents);
					shutdown(fds_rtp[i].fd, SHUT_RDWR);
					close(fds_rtp[i].fd);
					fds_rtp[i].fd = -1;
					break;
				}
				if(fds_rtp[i].revents & (POLLERR  | POLLNVAL))
				{
					ALOGD("disconnect error  %d",fds_rtp[0].revents);
					shutdown(fds_rtp[i].fd, SHUT_RDWR);
					close(fds_rtp[i].fd);
					fds_rtp[i].fd = -1;
					break;
				}
			}
		}



	}
	do_end = 1;
	pthread_exit (0);
	return NULL;
}

void WimoSource::queueEOS(status_t finalResult) {
    ALOGD("WimoSource::queueEOS finalResult %d",finalResult);
	if(finalResult != OK)
		ALOGD("WimoSource::queueEOS finalResult %d is false",finalResult);

    if(finalResult == INFO_TIME_OUT)
    {
		finalResult = OK;
    }
    mFinalResult = finalResult;
}

ssize_t WimoSource::readAt(off64_t offset, void *data, size_t size) {

    Mutex::Autolock autoLock(mLock);

	int ret = size;
	int size1 = size;
	//char temp[15] = "RK_WIMO_V1.0";
	char temp[15] = "RK_WIMO";

	if(size == 11)
	{
	     ALOGD("read size == 11");
	     memcpy(data,temp,12);
	     return 11;
	}
	if(size == 15)
	{
	     ALOGD("read size == 15");
	     memcpy(data,temp,15);
	     return 15;
	}
	if(mFinalResult == OK || connect_flag==0)
	{
		ALOGD("WimoSource::readAt mFinalResult %d disconnect",mFinalResult);
		return -1111;
	}


	{
		static int last_file_offset = 0;
		int64_t timeFirst,timesec;


		if(data_len < size)
		{
			ALOGV("data_len < size file_offset + data_len  %d %d,  offset + size %d %d  time  %lld  %lld waitting",
			file_offset , data_len, offset , size,timeFirst,timesec);
			return -2111;
		}
		memcpy(data,&rec_buf[head_offset],size);//[head_offset + (offset - file_offset) ],size);
		uint8_t *temp_buffer  = &rec_buf[head_offset];


		data_len -= size;
		head_offset += size;
		head_offset %= cache_size;


		file_offset+=size;
		last_file_offset = file_offset;
	}
	return ret;


}

status_t WimoSource::getSize(off64_t *size) {

    if (mStartThreadFlag == false) {
        startThread();
    }

	*size = data_len;

    return OK;
}

void WimoSource::startThread()
{
    if (mStartThreadFlag == false) {
        ALOGI("here start wimo recive thread");
        pthread_create(&mWimoThread, NULL, rec_data, this);

        mStartThreadFlag = true;
    } else {
        return;
    }
}


}  // namespace android
