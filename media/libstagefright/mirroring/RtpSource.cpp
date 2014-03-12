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

#include <include/RtpSource.h>
#include <media/stagefright/foundation/ADebug.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <utils/CallStack.h>
namespace android {

FILE* rtpsource_test_file;
FILE* rtpsource_data_file;
FILE* rtpsource_read_data_file;
FILE* rtp_ts_txt;
FILE* rtp_ts_txt_file;
int64_t audio_timeUs;
int64_t video_timeUs;
int64_t audio_sys_timeUs;
int64_t video_sys_timeUs;
int64_t last_audio_time_us,last_video_time_us;
int64_t last_audio_systime,last_video_systime;
int64_t video_latency[3],audio_latency[3],cur_video_latency,cur_audio_latency,video_adjustment,audio_adjustment;
int		sum_time;
int64_t rtp_start_time = 0;
int64_t Rtp_Audio_Start_TimeUs;
int64_t started_realtime_player;
int64_t last_adjust_time;
int		connection_flag;
int64_t	latency_time;
RtpSource::RtpSource(const char *filename,int type)
	:mFinalResult(1),
    connect_flag(1),
    connect_Type(0),
    player_type(type)
{
 	off64_t size;
	char *cur_idx ;
	char *next_idx ;
	char *addr_idx;
	char *local_port_idx;
	int i ;
	int flags;
	int on = 1;
    int rc;
	latency_time = 0;
	unalign_len[0]= unalign_len[1]= 0;
	sum_time = -1;
	connection_flag = 0 ;
	Rtp_Audio_Start_TimeUs = 0;
	last_adjust_time = 0;
	audio_timeUs = 0;
 	video_timeUs = 0;
	audio_sys_timeUs = 0;
	video_sys_timeUs = 0;
	last_audio_time_us = 0;
	last_video_time_us = 0;
	last_audio_systime = 0;
	last_video_systime = 0;
	rtp_start_time = 0;
	Rtp_Audio_Start_TimeUs = 0;
    video_adjustment = 200000ll;
	cur_video_latency 	= video_latency[0] 	= video_latency[1] 	= video_latency[2] 	= 0;
	cur_audio_latency 	= audio_latency[0] 	= audio_latency[1] 	= audio_latency[2] 	= 0;


	resync_flag = 0;

	rec_buf 			= (uint8_t*)malloc(65424 + 6291984);//1310924
	data_url = (char*)malloc(strlen(filename));
	align_buf[0] = (unsigned char*)malloc(11*188);
	align_buf[1] = (unsigned char*)malloc(11*188);
	if(data_url == NULL || rec_buf == NULL ||  align_buf[0] == NULL ||  align_buf[1] == NULL )
		goto error_handle;
	ALOGD("RtpSource::RtpSource data_url %x",data_url);

	strcpy(data_url,filename);
	cur_idx 			= data_url;
	rtpsource_test_file = rtpsource_data_file = NULL;
    //  mFile = fopen("/data/test.ts", "rb");
	//out_mFile = fopen("/data/out.ts","wb");
	//in_mFile = fopen("/data/in.ts","wb");
	mOffset 			= 0;
	mLength 			= -1;
	getSize(&size);
	mLength = size;
	pack_num = 0;
	head_offset = 0;
	tail_offset = 0;
	data_len = 0;
	file_offset = 0;
	local_port = remote_port = 0;
	rx_port = tx_port = 0;
	next_idx = strchr(cur_idx,':');
	addr_idx = cur_idx;
	if(next_idx == NULL || next_idx - cur_idx  > 16)

  	{
		ALOGD("address is error");
		goto error_handle;
	}
	*next_idx='\0';
	cur_idx = next_idx + 1;
	next_idx =  strchr(cur_idx,':');
	local_port_idx = cur_idx;
	if(next_idx == NULL || next_idx - cur_idx  > 5)
	{
		ALOGD("local_port is error");
		goto error_handle;
	}
	*next_idx='\0';
	cur_idx = next_idx + 1;
	ALOGD(" addr_idx %s local_port_idx %s string len %d",addr_idx,local_port_idx,next_idx - cur_idx);

	i = 0;
	                               //lsl
	while(local_port_idx[i] != '\0')          //lsl
	{
		int j;
		for(j = 0;j < 10;j++)
		{
			if( local_port_idx[i ] == '0' + j)
			{
				rx_port = j+rx_port * 10;
				ALOGV("%d  local_port %d",j,rx_port);
			}
		}
		i++;
	}


	next_idx =  strchr(cur_idx,':');
	local_port_idx = cur_idx;
	if(next_idx == NULL || next_idx - cur_idx  > 5)
	{
		ALOGD("local_port is error");
		goto error_handle;
	}
	*next_idx='\0';
	cur_idx = next_idx + 1;
	tx_addr = inet_addr(addr_idx);
  	tx_port = rx_port;
    ALOGV("RtpSource::RtpSource %x filename %s local_port %d cur_idx %s\n",this,filename, local_port,cur_idx);
    mSocket_Rec_Rtp = socket(AF_INET, SOCK_DGRAM , 0);
	if(mSocket_Rec_Rtp <= 0)
	{
		goto error_handle;
	}

    rc = setsockopt(mSocket_Rec_Rtp, SOL_SOCKET,  SO_REUSEADDR, (&on), sizeof(on));
    if (rc < 0) {
        ALOGE("receiver: failed to setsockopt\n");
		goto error_handle;
    }
    ALOGV("RtpSource::RtpSource %x filename %s local_port %d addr_idx %s  mSocket_Rec_Rtp %d\n",
		this,filename, rx_port,addr_idx,mSocket_Rec_Rtp);
    memset(mRTP_REC_Addr.sin_zero, 0, sizeof(mRTP_REC_Addr.sin_zero));
    mRTP_REC_Addr.sin_family = AF_INET;//inet_addr("192.168.0.100");//AF_INET;

    mRTP_REC_Addr.sin_addr.s_addr = INADDR_ANY;//inet_addr("192.168.0.100");//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;
    if((flags = fcntl(mSocket_Rec_Rtp, F_GETFL,O_NONBLOCK)) < 0)
	{

		ALOGD("fcntl nonblockF_GETFL error");
		goto error_handle;
	}
    flags |= O_NONBLOCK;
    if(fcntl(mSocket_Rec_Rtp, F_SETFL,flags) < 0)
	{
		ALOGD("fcntl nonblockF_SETFL error");
		goto error_handle;
	}
    mRTP_REC_Addr.sin_port = htons(rx_port);//htons(5666);//(local_port);
    int err;
    err =     bind(mSocket_Rec_Rtp, (const struct sockaddr *)(&mRTP_REC_Addr),sizeof(mRTP_REC_Addr));
    if(err!=0)
    {
		ALOGD("bind ret %d local_port %d errno %d",err,rx_port,errno);
		goto error_handle;

    }
    memset(mRTPAddr.sin_zero, 0, sizeof(mRTPAddr.sin_zero));
    mRTPAddr.sin_family = AF_INET;

    mRTPAddr.sin_addr.s_addr = tx_addr;//inet_addr(addr_idx);// inet_addr("192.168.0.101");;//inet_addr(ip_addr);//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;
    mRTPAddr.sin_port = htons(tx_port);//htons(5666);//(local_port);
    #if 0
    err = connect(mSocket_Rec_Rtp,(const struct sockaddr *)( &mRTPAddr), sizeof(mRTPAddr));
	if(err!=0)
	{
	    ALOGD("set err rtp %d  errno %d %s",err,errno,addr_idx);
		goto error_handle;
	}
	#endif
	ALOGV("cur_idx %s",cur_idx);
	if(cur_idx[0] == '1')
	{
		if(cur_idx[1] == '\0')
			latency_time = 184000ll;
		ALOGD("cur_idx_1 %s latency_time %lld",cur_idx,latency_time);
		connect_Type = 1;
		connection_flag = 1;
		pthread_create(&mThread, NULL, rec_data, this);
	}
  	pthread_create(&mThread_Rtp, NULL, rec_data_Rtp, this);
	return;
error_handle:
	connect_flag = 0;
	if(mSocket_Rec_Rtp >= 0)
	{
		shutdown(mSocket_Rec_Rtp, SHUT_RDWR);
		close(mSocket_Rec_Rtp);
	  	mSocket_Rec_Rtp = -1;
	}
	if(mSocket_Rec_Client >= 0)
	{
		shutdown(mSocket_Rec_Client, SHUT_RDWR);
		close(mSocket_Rec_Client);
		mSocket_Rec_Client = -1;
	}
	ALOGD("bind ret %d local_port %d errno %d",err,rx_port,errno);
	return;
}

RtpSource::RtpSource(unsigned long addr,unsigned short rx_port,unsigned short tx_port)
{

}

RtpSource::~RtpSource() {
	void* retval1;
    ALOGV("RtpSource::~RtpSource start mSocket_Rec_Client %d mSocket_Rec_Rtp %d fd[0] %d fd[1] %d",
		mSocket_Rec_Client,mSocket_Rec_Rtp,fds[0].fd,fds[1].fd);
	connect_flag = 0;
   	pthread_join(mThread, &retval1);
   	pthread_join(mThread_Rtp, &retval1);
    if(mSocket_Rec_Rtp >= 0)
	{
		shutdown(mSocket_Rec_Rtp, SHUT_RDWR);
		close(mSocket_Rec_Rtp);
		mSocket_Rec_Rtp = -1;
		ALOGD("rec_data  connect_flag %d mSocket_Rec %d",connect_flag,mSocket_Rec_Rtp);
	}
	if(mSocket_Rec_Client >= 0)
	{
		shutdown(mSocket_Rec_Client, SHUT_RDWR);
		close(mSocket_Rec_Client);
		mSocket_Rec_Client = -1;
	}

	if(align_buf[1])
		free(align_buf[1]);
	if(align_buf[0])
		free(align_buf[0]);
    if(rec_buf)
		free(rec_buf);
	if(data_url)
		free(data_url);
    ALOGD("RtpSource::~RtpSource  retval1 %d",retval1);
}




ssize_t RtpSource::seek( off64_t offset, int whence)
{
	return OK;
}
status_t RtpSource::initCheck() const {

    return mSocket_Rec_Rtp > -1 ? OK : NO_INIT;
}
void* RtpSource::rec_data_Rtp(void* me)
{
	RtpSource* rtpsource = static_cast<RtpSource *>(me);
	ALOGD("%x rtpsource 1 %x",me,rtpsource);
	rtpsource->ThreadWrapper_Rtp(NULL);
	return NULL;
}
void *RtpSource::ThreadWrapper_Rtp(void *)
{
	int ret = 0;
	int first_rec_packet = 0;
	int64_t timeFirst,timesec;
	timeval tm;
	int len;
	int error=-1;
	struct pollfd fds_rtp[2];
	fds_rtp[0].fd = mSocket_Rec_Rtp;
	fds_rtp[0].events = POLLIN;
	fds_rtp[1].fd = -1;
	fds_rtp[1].events = POLLIN;
	while(connect_flag)
	{
		if((ret = poll(fds_rtp, 1, 20)) > 0)
		{
			int i;
			for(i = 0; i < 1; i++)
			{
				if(fds_rtp[0].revents & POLLIN)
				{
					do{
						{
							Mutex::Autolock autoLock(mLock);
							unsigned char* data =  &rec_buf[tail_offset];
							unsigned char* cur_data = &rec_buf[tail_offset];
							errno = 0;
							if(data_len + 64*1024 > 6291984)
							{
								ret = recv(mSocket_Rec_Rtp, &rec_buf[6291984], 65424, 0);//recvfrom(mSocket_Rec, &rec_buf[tail_offset], 188 * 6, 0,  (const struct sockaddr *)(&mRTP_REC_Addr),  &len);
								cur_data = &rec_buf[6291984];
								ALOGD("(data_len + 64*1024 > 6291984)");
							}
							else
								ret = recv(mSocket_Rec_Rtp, &rec_buf[tail_offset], 65424, 0);//recvfrom(mSocket_Rec, &rec_buf[tail_offset], 188 * 6, 0,  (const struct sockaddr *)(&mRTP_REC_Addr),  &len);
							ALOGV("Audio ret %d errno %d",ret,errno);
							if(ret <= 0)
							{
								if(ret == 0 || (ret < -1)|| ((ret < -1) && (errno!=EAGAIN) ))
								{
									ALOGV("ret %d errno %d",ret,errno);
									goto Connecttiong_end;
								}
								break;
							}
							else
							{
								int64_t cur_time;
								cur_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
								if(rtp_start_time == 0)
								{
									rtp_start_time = cur_time;
								}
								for(int ts_pack = 0; ts_pack < ret;ts_pack+=188)
								{
									char* cur_data = (char*)&rec_buf[tail_offset + ts_pack];
									int64_t 	PTS = 0;
									int64_t timeUs;
									int64_t sys_timeUs;
									if(cur_data[0] == 0x47 && cur_data[1] == 0x41 && cur_data[2] == 0xe1 )
									{
										PTS = ((cur_data[13] ) & 6) <<29;
										PTS |= cur_data[14] <<22;
										PTS |= (cur_data[15] & 0xfe) <<14;
										PTS |= (cur_data[16] ) <<7;
										PTS |= (cur_data[17] >>1);
										timeUs 		= audio_timeUs		= PTS * 100ll / 9ll;
										audio_sys_timeUs 	= cur_time;
										if(Rtp_Audio_Start_TimeUs == 0)
										{
											last_adjust_time = cur_time;
											Rtp_Audio_Start_TimeUs = cur_time;
										}
										if(timeUs- last_adjust_time > 60000000ll)
										{
											last_adjust_time = cur_time;
											Rtp_Audio_Start_TimeUs = cur_time - timeUs;
										}
										if(Rtp_Audio_Start_TimeUs > cur_time - PTS * 100ll / 9ll)
											Rtp_Audio_Start_TimeUs = cur_time - PTS * 100ll / 9ll;
									}
									else if(cur_data[0] == 0x47 && cur_data[1] == 0x41 && cur_data[2] == 0xe2 &&
										((cur_data[18] == 0x0 && cur_data[19] == 0x0 && cur_data[20] == 0x0 && cur_data[21] == 0x1 && cur_data[22] == 0x6e  )))
									{
										PTS = ((cur_data[13] ) & 6) <<29;
										PTS |= cur_data[14] <<22;
										PTS |= (cur_data[15] & 0xfe) <<14;
										PTS |= (cur_data[16] ) <<7;
										PTS |= (cur_data[17] >>1);
										timeUs 			= video_timeUs		= PTS * 100ll / 9ll;
										video_sys_timeUs 	= cur_time;
									}
									{
										int retrtptxt;
										if((retrtptxt = access("data/test/rtp_ts_txt_file",0)) == 0)//test_file!=NULL)
										{
											if(rtp_ts_txt == NULL)
												rtp_ts_txt = fopen("data/test/rtp_ts_txt.txt","wb");
											else
											{
												if(cur_data[0] == 0x47 && cur_data[1] == 0x41 && cur_data[2] == 0xe1)
												{
fprintf(rtp_ts_txt,"RtpSource AudioData start time %16lld  %16lld %16lld 	delta %16lld  %16lld %16lld sys_timeUs %lld %16lld %16lld delta sys time av %16lld aa %16lld   timeUs %16lld %16lld  delta time av %16lld aa %16lld PTS %d data_len tail_offset head_offset %d %d %d ret %d %2x%2x%2x%2x%2x\n",
started_realtime_player,rtp_start_time,Rtp_Audio_Start_TimeUs,
cur_time - started_realtime_player - timeUs,cur_time - rtp_start_time - timeUs,cur_time - Rtp_Audio_Start_TimeUs - timeUs,
audio_sys_timeUs,video_sys_timeUs,last_adjust_time,
audio_sys_timeUs - last_video_systime,audio_sys_timeUs - last_audio_systime,audio_timeUs,video_timeUs,timeUs - last_video_time_us
,timeUs - last_audio_time_us,PTS,data_len,tail_offset,head_offset,ret,cur_data[13],cur_data[14],cur_data[15],cur_data[16],cur_data[17]);
last_audio_time_us = timeUs;
last_audio_systime = cur_time;
												}
												else	if(cur_data[0] == 0x47 && cur_data[1] == 0x41 && cur_data[2] == 0xe2)
												{
if(cur_data[18] == 0x0 && cur_data[19] == 0x0 && cur_data[20] == 0x0 && cur_data[21] == 0x1)
{
fprintf(rtp_ts_txt,"RtpSource VideoData start time %16lld  %16lld %lld 	delta %16lld  %16lld %16lld sys_timeUs %16lld %16lld delta sys time va %16lld vv %16lld   timeUs %16lld %16lld  delta time va %16lld vv %16lld PTS %d data_len tail_offset head_offset %d %d %d ret %d %2x%2x%2x%2x%2x\n",
started_realtime_player,rtp_start_time,Rtp_Audio_Start_TimeUs,cur_time - started_realtime_player - timeUs - latency_time,
cur_time - rtp_start_time - timeUs - latency_time,cur_time - Rtp_Audio_Start_TimeUs - timeUs - latency_time,
audio_sys_timeUs,video_sys_timeUs,audio_timeUs,video_timeUs,
video_sys_timeUs - last_audio_systime,video_sys_timeUs - last_video_systime,timeUs - last_audio_time_us
,timeUs - last_video_time_us,PTS,data_len,tail_offset,head_offset,ret,cur_data[13],cur_data[14],cur_data[15],cur_data[16],cur_data[17]);
last_video_time_us = timeUs;
last_video_systime = cur_time;
}
												}
											}
										}
										else
										{
											if(rtp_ts_txt!=NULL)
											{
												fclose(rtp_ts_txt);
												rtp_ts_txt = NULL;
											}
											ALOGV("rtp_ts_txt==NULL ret %d",retrtptxt);
										}
									}
								}
								if(1)
								{
									int ret_rtp;
									ALOGV("ret %d",ret);
									if((ret_rtp = access("data/test/rtpsource_test_file",0)) == 0)//test_file!=NULL)
									{
										ALOGV("test_file!=NULL test_264 %x",rtpsource_data_file);
										if(rtpsource_data_file !=NULL)
										{
											fwrite(cur_data,ret,1,rtpsource_data_file);
										}
										else
											rtpsource_data_file = fopen("data/test/rtpsource_test_264_file","wb+");
									}
									else
									{
										if(rtpsource_data_file!=NULL)
										{
											fclose(rtpsource_data_file);
											rtpsource_data_file = NULL;
										}
										ALOGV("rtpsource_data_file==NULL ret_acc %d",ret_rtp);
									}
								}
								if(data_len + ret <= 6291984)
								{
									if(data_len + 64*1024 >6291984)
									{
										memcpy(data,&rec_buf[6291984],ret);
									}
									if(tail_offset + ret > 6291984)
									{
										memcpy(rec_buf,&rec_buf[6291984],tail_offset + ret - 6291984);
									}
									if(rtp_ts_txt!=NULL)
											fprintf(rtp_ts_txt,"rtp normal write tail_offset %d head_offset %d datalen %d ret %d   %2x%2x%2x %2x%2x%2x%2x%2x%2x%2x%2x%2x%2x\n"
											,tail_offset, head_offset, data_len, ret,
											temp_buf[0],temp_buf[1],temp_buf[2], temp_buf[18], temp_buf[19], temp_buf[20],
											temp_buf[21], temp_buf[22], temp_buf[23], temp_buf[24], temp_buf[25],
											temp_buf[26], temp_buf[27]);
								ALOGV("data_len %d tail_offset %d ret %d  111",data_len ,tail_offset,ret);
									data_len += ret;
									tail_offset += ret;
									tail_offset %=6291984;
								}
								else
								{
										head_offset = 0;
									tail_offset = 0;
									data_len = 0;
									resync_flag = 1;
									if(rtp_ts_txt!=NULL)
											fprintf(rtp_ts_txt,"rtp resync_flag == 1 tail_offset %d head_offset %d datalen %d ret  %d %2x%2x%2x %2x%2x%2x%2x%2x%2x%2x%2x%2x%2x\n"
											,tail_offset, head_offset, data_len,ret,
											temp_buf[0],temp_buf[1],temp_buf[2], temp_buf[18], temp_buf[19], temp_buf[20],
											temp_buf[21], temp_buf[22], temp_buf[23], temp_buf[24], temp_buf[25],
											temp_buf[26], temp_buf[27]);
								}
							}
							ALOGV("data_len %d tail_offset %d ret %d",data_len ,tail_offset,ret);
						}
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
Connecttiong_end:
	ALOGD("ThreadWrapper_Rtp exit");
	do_end = 1;
	connect_flag = 0;
	pthread_exit (0);
	return NULL;
}

void* RtpSource::rec_data(void* me)
{
	RtpSource* rtpsource = static_cast<RtpSource *>(me);
	ALOGD("%x rtpsource rec_data %x",me,rtpsource);
	rtpsource->ThreadWrapper(NULL);
	return NULL;
}
/*

¡¡µ±¶Ô¶Ë»úÆ÷crash»òÕßÍøÂçÁ¬½Ó±»¶Ï¿ª(±ÈÈçÂ·ÓÉÆ÷²»¹¤×÷£¬ÍøÏß¶Ï¿ªµÈ)£¬
´ËÊ±·¢ËÍÊý¾Ý¸ø¶Ô¶ËÈ»ºó¶ÁÈ¡±¾¶Ësocket»á·µ»ØETIMEDOUT»òÕßEHOSTUNREACH
»òÕßENETUNREACH(ºóÁ½¸öÊÇÖÐ¼äÂ·ÓÉÆ÷ÅÐ¶Ï·þÎñÆ÷Ö÷»ú²»¿É´ïµÄÇé¿ö)¡£
	connect

¡¡¡¡µ±¶Ô¶Ë»úÆ÷crashÖ®ºóÓÖÖØÐÂÆô¶¯£¬È»ºó¿Í»§¶ËÔÙÏòÔ­À´µÄÁ¬½Ó·¢ËÍÊý¾Ý£¬
ÒòÎª·þÎñÆ÷¶ËÒÑ¾­Ã»ÓÐÔ­À´µÄÁ¬½ÓÐÅÏ¢£¬´ËÊ±·þÎñÆ÷¶Ë»ØËÍRST¸ø¿Í»§¶Ë£¬
´ËÊ±¿Í»§¶Ë¶Á±¾µØ¶Ë¿Ú·µ»ØECONNRESET´íÎó¡£
		write
¡¡¡¡µ±·þÎñÆ÷ËùÔÚµÄ½ø³ÌÕý³£»òÕßÒì³£¹Ø±ÕÊ±£¬»á¶ÔËùÓÐ´ò¿ªµÄÎÄ¼þÃèÊö·û½øÐÐclose£
¬Òò´Ë¶ÔÓÚÁ¬½ÓµÄsocketÃèÊö·ûÔò»áÏò¶Ô¶Ë·¢ËÍFIN·Ö½Ú½øÐÐÕý³£¹Ø±ÕÁ÷³Ì¡£
¶Ô¶ËÔÚÊÕµ½FINÖ®ºó¶Ë¿Ú±äµÃ¿É¶Á£¬´ËÊ±¶ÁÈ¡¶Ë¿Ú»á·µ»Ø0±íÊ¾µ½ÁËÎÄ¼þ½áÎ²(¶Ô¶Ë²»»áÔÙ·¢ËÍÊý¾Ý)¡£¡¡
		recv
¡¡¡¡µ±Ò»¶ËÊÕµ½RSTµ¼ÖÂ¶ÁÈ¡socket·µ»ØECONNRESET£¬´ËÊ±Èç¹ûÔÙ´Îµ÷ÓÃwrite·¢ËÍÊý¾Ý¸ø¶Ô¶ËÔò´¥·¢SIGPIPEÐÅºÅ£
¬ÐÅºÅÄ¬ÈÏÖÕÖ¹½ø³Ì£¬Èç¹ûºöÂÔ´ËÐÅºÅ»òÕß´ÓSIGPIPEµÄÐÅºÅ´¦Àí³ÌÐò·µ»ØÔòwrite³ö´í·µ»ØEPIPE¡£
12£¬ÊÇ·ÇÕý³£¹Ø±Õ£¬·¢ËÍ¶ËÍ¨¹ýconnect,writeÀ´¼ì²âµ½³ö´í¡£ÕâÊÇÖ÷¶¯µÄ
3£¬ÏµÍ³ÔËÐÐ¹Ø±Õ£¬»áÍ¨Öª¿Í»§£¬¿Í»§Í¨¹ýselect ¿É¶ÁÖªµÀ¶Ô¶Ë¹Ø±Õ

*/
void *RtpSource::ThreadWrapper(void *)
{
	int ret = 0;
	int recv_len;
	int	avail_len;
	int	unalign_data_len;
	int first_rec_packet = 0;
	int64_t timeFirst,timesec;
	timeval tm;
	int error=-1;
	int err;
	int	flag = 1;
	int on = 1;
    int rc;
	int rcvbuf_len;
	int flags;
    int len = sizeof(rcvbuf_len);
	int	fd_num = (connect_Type==1) ? 2 : 1;
	fds[0].fd = -1;
	fds[0].events = POLLIN;
	fds[1].fd = -1;
	fds[1].events = POLLIN;
	if(connect_Type==1)
	{
		while(connect_flag)
		{
			int	iFd;
			fd_set set;
		      mSocket_Rec_Client = socket(AF_INET, SOCK_STREAM , 0);
			  ALOGD("mSocket_Rec_Client %d",mSocket_Rec_Client);
			if(mSocket_Rec_Client <= 0)
			{
				goto Connecttiong_end_2;
			}
		    fds[1].fd = mSocket_Rec_Client;
			fds[1].events = POLLIN;

		    rc = setsockopt(mSocket_Rec_Client, SOL_SOCKET,  SO_REUSEADDR, (&on), sizeof(on));
		    if (rc < 0) {
		        ALOGE("receiver: failed to setsockopt\n");
				goto Connecttiong_end_2;
		    }
		    memset(mRTPAddr_Client.sin_zero, 0, sizeof(mRTPAddr_Client.sin_zero));
		    mRTPAddr_Client.sin_family = AF_INET;//inet_addr("192.168.0.100");//AF_INET;
		    mRTPAddr_Client.sin_addr.s_addr = INADDR_ANY;//inet_addr("192.168.0.100");//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;
#if 1
			if((flags = fcntl(mSocket_Rec_Client, F_GETFL,O_NONBLOCK)) < 0)
			{
				ALOGD("fcntl nonblockF_GETFL error");
				goto Connecttiong_end_2;
			}
		   	flags |= O_NONBLOCK;
		    if(fcntl(mSocket_Rec_Client, F_SETFL,flags) < 0)
			{
				ALOGD("fcntl nonblockF_SETFL error");
				goto Connecttiong_end_2;
			}
#endif
		    mRTPAddr_Client.sin_port = htons(rx_port + 1);//htons(5666);//(local_port);
		    ALOGD("mRTPAddr_Local addr %x %x mRTPAddr_Client.sin_port %x %x",
							mRTPAddr_Local.sin_port,	mRTPAddr_Local.sin_addr.s_addr,mRTPAddr_Client.sin_port ,mRTPAddr_Client.sin_addr.s_addr);
#if 0
		    err =     bind(mSocket_Rec_Client, (const struct sockaddr *)(&mRTPAddr_Client),sizeof(mRTPAddr_Client));
		    if(err!=0)
		    {
				ALOGD("bind ret %d local_port %d errno %d",err,rx_port,errno);
				goto Connecttiong_end_2;

		    }
#endif

			ret = setsockopt( mSocket_Rec_Client, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag) );
			if (ret == -1) {
			  ALOGD("Couldn't setsockopt mSocket_Rec_Client\n");
			  goto Connecttiong_end_2;
			}
		    memset(mRTPAddr_Local.sin_zero, 0, sizeof(mRTPAddr_Local.sin_zero));
		    mRTPAddr_Local.sin_family = AF_INET;
		    mRTPAddr_Local.sin_addr.s_addr = tx_addr;//inet_addr(addr_idx);// inet_addr("192.168.0.101");;//inet_addr(ip_addr);//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;
		    mRTPAddr_Local.sin_port = htons(tx_port+1);//htons(5666);//(local_port);

#if	1
			ret = connect(mSocket_Rec_Client,(const struct sockaddr *)( &mRTPAddr_Local), sizeof(sockaddr));
		    ALOGD("RtpSource::ThreadWrapper 1 mRTPAddr_Local addr %x port %x ret %d errno %d connect_flag %d",
				mRTPAddr_Local.sin_addr.s_addr,mRTPAddr_Local.sin_port ,ret,errno,connect_flag);
			if(errno!=EINPROGRESS)
			{
			    ALOGD(" error ret  %d  errno %d  mRTPAddr_Local.sin_port  %d",ret,errno,mRTPAddr_Local.sin_port );
				goto Connecttiong_end_2;
			}
#endif
			tm.tv_sec = 0;
			tm.tv_usec = 1000000;
			FD_ZERO(&set);
			FD_SET(mSocket_Rec_Client, &set);
			if((iFd = select(mSocket_Rec_Client+1, 0, &set, NULL, &tm)) != 1)//select time out
			{
				ALOGD("RtpSource end  1 connect_flag  %d iFd %d errno %d",
					connect_flag,iFd,errno);
				shutdown(mSocket_Rec_Client, SHUT_RDWR);
				close(mSocket_Rec_Client);
				mSocket_Rec_Client = -1;
				continue;
			}

			ALOGD("RtpSource  connect_flag  %d iFd %d errno %d",connect_flag,iFd,errno);
			if((err = getsockopt(mSocket_Rec_Client, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len)) != 0)
	  		{
	  			ALOGD("getsockopt error %d error %d",err,error);
				goto Connecttiong_end_2;//fd error or operation error
	  		}
			if(error != 0  )
		  	{
		  		ALOGD("mRTPAddr_Local error != 0  addr %x %x mRTPAddr_Client.sin_port %x %x connect_flag %d errno %d error %d  iFd %d",
				mRTPAddr_Local.sin_port,  mRTPAddr_Local.sin_addr.s_addr,mRTPAddr_Client.sin_port ,
				mRTPAddr_Client.sin_addr.s_addr,connect_flag,errno,error,iFd);
				shutdown(mSocket_Rec_Client, SHUT_RDWR);
				close(mSocket_Rec_Client);
				mSocket_Rec_Client = -1;
				usleep(10000);
				continue;
		  	}
			else
			{

				struct sockaddr_in mRTPAddr_RTP_temp;
				int	len =sizeof(mRTPAddr_RTP_temp);
				getsockname(fds[1].fd,(sockaddr*)&mRTPAddr_RTP_temp, &len);
				ALOGD("RtpSource  setup %x %x fd %d" ,mRTPAddr_RTP_temp.sin_port,  mRTPAddr_RTP_temp.sin_addr.s_addr,fds[1].fd);
				break;
			}
		}
	}
	ALOGD("mRTPAddr_Local finish1 addr %x %x mRTPAddr_Client.sin_port %x %x connect_flag %d errno %d",
			mRTPAddr_Local.sin_port,  mRTPAddr_Local.sin_addr.s_addr,mRTPAddr_Client.sin_port ,
			mRTPAddr_Client.sin_addr.s_addr,connect_flag,errno);
	while(connect_flag)
	{
		if((ret = poll(fds, fd_num, 20)) > 0)
		{
			int i;
			for(i = 0; i < fd_num; i++)
			{
				if(fds[i].revents & POLLIN)
				{
					unalign_data_len = unalign_len[i];
					temp_buf = align_buf[i];
					do{
						{
							Mutex::Autolock autoLock(mLock);
							unsigned char* data =  &rec_buf[tail_offset];
							errno = 0;
							if(data_len + 64*1024 > 6291984)
							{
								ret = recv(fds[i].fd, &temp_buf[unalign_data_len], 10 * 188, 0);//recvfrom(mSocket_Rec, &rec_buf[tail_offset], 188 * 6, 0,  (const struct sockaddr *)(&mRTP_REC_Addr),  &len);
								ALOGV("(data_len + 64*1024 > 6291984)");
							}
							else
								ret = recv(fds[i].fd, &temp_buf[unalign_data_len], 10 * 188, 0);//recvfrom(mSocket_Rec, &rec_buf[tail_offset], 188 * 6, 0,  (const struct sockaddr *)(&mRTP_REC_Addr),  &len);

							ALOGV("i %d ret %d,errno %d unalign_video_len %d",i,ret ,errno,unalign_data_len);
							if(ret <= 0)//Í¨¹ý´Ë´¦ºÍpollupÅÐ¶Ï¶Ô·½ÊÇ·ñ¶Ï¿ªÁ¬½Ó£¬ÖÁÓÚÈç¹ûcrashµÄ»°¿ÉÒÔÍ¨¹ýÐ­Òé²ãµÄÐÄÌø°ü²ì¾õ²¢ÍË³ö
							{
								ALOGV("ret %d errno %d",ret,errno);
								if(ret == 0 || (ret < -1)|| ((ret < -1) && (errno!=EAGAIN) ))
								{
								ALOGD( "ThreadWrapper recv ret %d errno %d",ret,errno);
									goto Connecttiong_end_2;
								}

								break;
							}
							else if(ret > 10*188)
							{
								ALOGD("recv data is bigger than 1880 ret %d",ret);
							}
							else
							{
								int64_t cur_time;
								cur_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
								if(rtp_start_time == 0)
								{
									rtp_start_time = cur_time;
								}
								ALOGV("RtpSource Video before proc unalign_data_len %d  ret %d data_len %d %2x%2x%2x%2x\n",
									unalign_data_len,ret,data_len,temp_buf[0],temp_buf[1],temp_buf[2],temp_buf[3]);
								recv_len = ret;
								avail_len = ret + unalign_data_len - (ret + unalign_data_len ) % 188 ;
								unalign_data_len = ret + unalign_data_len -avail_len;



								for(int ts_pack = 0; ts_pack < avail_len;ts_pack+=188)
								{
									char* cur_data = (char*)&temp_buf[ts_pack];
									int64_t 	PTS = 0;
									int64_t timeUs;
									int64_t sys_timeUs;
									if(cur_data[0] == 0x47 && cur_data[1] == 0x41 && cur_data[2] == 0xe1 )
									{
										PTS = ((cur_data[13] ) & 0xe) <<29;
										PTS |= cur_data[14] <<22;
										PTS |= (cur_data[15] & 0xfe) <<14;
										PTS |= (cur_data[16] ) <<7;
										PTS |= (cur_data[17] >>1);
										timeUs 		= audio_timeUs		= PTS * 100ll / 9ll;
										audio_sys_timeUs 	= cur_time;
										if(Rtp_Audio_Start_TimeUs == 0)
										{
											Rtp_Audio_Start_TimeUs = cur_time;
										}
										if(Rtp_Audio_Start_TimeUs > cur_time - PTS * 100ll / 9ll || timeUs < last_audio_time_us)
											Rtp_Audio_Start_TimeUs = cur_time - PTS * 100ll / 9ll;
									}
									else if(cur_data[0] == 0x47 && cur_data[1] == 0x41 && cur_data[2] == 0xe2 &&
										((cur_data[18] == 0x0 && cur_data[19] == 0x0 && cur_data[20] == 0x0 && cur_data[21] == 0x1 && cur_data[22] == 0x6e  )))
									{
										PTS = ((cur_data[13] ) & 0xe) <<29;
										PTS |= cur_data[14] <<22;
										PTS |= (cur_data[15] & 0xfe) <<14;
										PTS |= (cur_data[16] ) <<7;
										PTS |= (cur_data[17] >>1);
										timeUs 			= video_timeUs		= PTS * 100ll / 9ll;
										video_sys_timeUs 	= cur_time;

									}

									{
										int retrtptxt;
										if((retrtptxt = access("data/test/rtp_ts_txt_file",0)) == 0)//test_file!=NULL)
										{

											if(rtp_ts_txt == NULL)
												rtp_ts_txt = fopen("data/test/rtp_ts_txt.txt","wb");
											else
											{

												if(cur_data[0] == 0x47 && cur_data[1] == 0x41 && cur_data[2] == 0xe1)
												{
fprintf(rtp_ts_txt,"RtpSource AudioData start time %16lld  %16lld %16lld 	delta %16lld  %16lld %16lld sys_timeUs %16lld %16lld delta sys time av %16lld aa %16lld   timeUs %16lld %16lld  delta time av %16lld aa %16lld PTS %d data_len tail_offset head_offset %d %d %d ret %d %2x%2x%2x%2x%2x\n",
started_realtime_player,rtp_start_time,Rtp_Audio_Start_TimeUs,
cur_time - started_realtime_player - timeUs,cur_time - rtp_start_time - timeUs,cur_time - Rtp_Audio_Start_TimeUs - timeUs,
audio_sys_timeUs,video_sys_timeUs,
audio_sys_timeUs - last_video_systime,audio_sys_timeUs - last_audio_systime,audio_timeUs,video_timeUs,timeUs - last_video_time_us
,timeUs - last_audio_time_us,PTS,data_len,tail_offset,head_offset,ret,cur_data[13],cur_data[14],cur_data[15],cur_data[16],cur_data[17]);
last_audio_time_us = timeUs;
last_audio_systime = cur_time;
												}
												else	if(cur_data[0] == 0x47 && cur_data[1] == 0x41 && cur_data[2] == 0xe2 &&cur_data[4] == 0x0 &&
										cur_data[5] == 0x0 && cur_data[6] == 0x1 &&
										((cur_data[18] == 0x0 && cur_data[19] == 0x0 && cur_data[20] == 0x0 && cur_data[21] == 0x1 && cur_data[22] == 0x6e  )))
												{
{
fprintf(rtp_ts_txt,"RtpSource VideoData start time %16lld  %16lld %16lld 	delta %16lld  %16lld %16lld sys_timeUs %16lld %16lld delta sys time va %16lld vv %16lld   timeUs %16lld %16lld  delta time va %16lld vv %16lld PTS %d data_len tail_offset head_offset %d %d %d ret %d %2x%2x%2x%2x%2x%2x%2x%2x%2x%2x \n",
started_realtime_player,rtp_start_time,Rtp_Audio_Start_TimeUs,cur_time - started_realtime_player - timeUs,
cur_time - rtp_start_time - timeUs ,cur_time - Rtp_Audio_Start_TimeUs - timeUs ,
audio_sys_timeUs,video_sys_timeUs,
video_sys_timeUs - last_audio_systime,video_sys_timeUs - last_video_systime,audio_timeUs,video_timeUs,timeUs - last_audio_time_us
,timeUs - last_video_time_us,PTS,data_len,tail_offset,head_offset,ret,cur_data[13],cur_data[14],cur_data[15],cur_data[16],cur_data[17],cur_data[22], cur_data[23], cur_data[24], cur_data[25],
											cur_data[26], cur_data[27]);
last_video_time_us = timeUs;
last_video_systime = cur_time;
}
												}

											}
										}
										else
										{

											if(rtp_ts_txt!=NULL)
											{
												fclose(rtp_ts_txt);
												rtp_ts_txt = NULL;
											}
											ALOGV("rtp_ts_txt==NULL ret %d",retrtptxt);
										}

									}


								}












								if(1)
								{
									int ret_rtp;
									if((ret_rtp = access("data/test/rtpsource_test_file",0)) == 0)//test_file!=NULL)
									{
										ALOGV("test_file!=NULL test_264 %x",rtpsource_data_file);
										if(rtpsource_data_file !=NULL)
										{
											fwrite(temp_buf,avail_len  ,1,rtpsource_data_file);
										}
										else
										{
											rtpsource_data_file = fopen("data/test/rtpsource_test_264_file","wb+");
											if(rtpsource_data_file !=NULL)
											{
												fwrite(temp_buf,avail_len  ,1,rtpsource_data_file);
											}
										}
									}
									else
									{
										if(rtpsource_data_file!=NULL)
										{
											fclose(rtpsource_data_file);
											rtpsource_data_file = NULL;
										}
										ALOGV("rtpsource_data_file==NULL ret_acc %d",ret_rtp);
									}
								}

								if(resync_flag == 1 )
								{
									if(temp_buf[18] == 0x0 && temp_buf[19] == 0x0 && temp_buf[20] == 0x0 &&
										temp_buf[21] == 0x1 && (temp_buf[22] & 0x1f) == 0x5)
									{
										if(rtp_ts_txt!=NULL)
											fprintf(rtp_ts_txt,"find avc sync header tail_offset %d head_offset %d datalen %d ret %d avail_len %d unalign_data_len %d %2x%2x%2x %2x%2x%2x%2x%2x%2x%2x%2x%2x%2x\n"
											,tail_offset, head_offset, data_len, avail_len,avail_len , unalign_data_len,
											temp_buf[0],temp_buf[1],temp_buf[2], temp_buf[18], temp_buf[19], temp_buf[20],
											temp_buf[21], temp_buf[22], temp_buf[23], temp_buf[24], temp_buf[25],
											temp_buf[26], temp_buf[27]);
										resync_flag = 0;
									}
									else	if(temp_buf[0] == 0x47 && temp_buf[1] == 0x41 && temp_buf[2] == 0xe1)
									{
									}
									else
									{
										if(rtp_ts_txt!=NULL)
											fprintf(rtp_ts_txt,"discard data to resync  tail_offset %d head_offset %d datalen %d ret %d avail_len %d unalign_data_len %d %2x%2x%2x %2x%2x%2x%2x%2x%2x%2x%2x%2x%2x\n"
											,tail_offset, head_offset, data_len, avail_len,avail_len , unalign_data_len,
											temp_buf[0],temp_buf[1],temp_buf[2], temp_buf[18], temp_buf[19], temp_buf[20],
											temp_buf[21], temp_buf[22], temp_buf[23], temp_buf[24], temp_buf[25],
											temp_buf[26], temp_buf[27]);
										goto recv_loop_end;
									}
								}

								if(data_len + avail_len <= 6291984)
								{
										if(rtp_ts_txt!=NULL)
											fprintf(rtp_ts_txt,"normal write tail_offset %d head_offset %d datalen %d ret %d avail_len %d unalign_data_len %d %2x%2x%2x %2x%2x%2x%2x%2x%2x%2x%2x%2x%2x\n"
											,tail_offset, head_offset, data_len, avail_len,avail_len , unalign_data_len,
											temp_buf[0],temp_buf[1],temp_buf[2], temp_buf[18], temp_buf[19], temp_buf[20],
											temp_buf[21], temp_buf[22], temp_buf[23], temp_buf[24], temp_buf[25],
											temp_buf[26], temp_buf[27]);
									if(tail_offset + avail_len > 6291984)
									{
										memcpy(data,&temp_buf[0],6291984 - tail_offset);
										memcpy(rec_buf,&temp_buf[6291984 - tail_offset],tail_offset + avail_len - 6291984);
									}
									else
										memcpy(data,&temp_buf[0],avail_len);

									ALOGV("data_len %d tail_offset %d ret %d  111 unalign_data_len %d drecv_len %d %x",
										data_len ,tail_offset,avail_len,unalign_data_len,recv_len,temp_buf[avail_len-1]);
									data_len += avail_len;
									tail_offset += avail_len;
									tail_offset %=6291984;

								}
								else
								{



									if(temp_buf[18] == 0x0 && temp_buf[19] == 0x0 && temp_buf[20] == 0x0 &&
										temp_buf[21] == 0x1 && (temp_buf[22] & 0x1f) == 0x5)
									{
										memcpy(&rec_buf[0],&temp_buf[0],avail_len);
										data_len = avail_len;
										tail_offset = avail_len;
										head_offset = 0;
										if(rtp_ts_txt!=NULL)
											fprintf(rtp_ts_txt,"buffer is full but it is avc sync header tail_offset %d head_offset %d datalen %d ret %d avail_len %d unalign_data_len %d %2x%2x%2x %2x%2x%2x%2x%2x%2x%2x%2x%2x%2x\n"
											,tail_offset, head_offset, data_len, avail_len,avail_len , unalign_data_len,
											temp_buf[0],temp_buf[1],temp_buf[2], temp_buf[18], temp_buf[19], temp_buf[20],
											temp_buf[21], temp_buf[22], temp_buf[23], temp_buf[24], temp_buf[25],
											temp_buf[26], temp_buf[27]);
									}
									else
									{
										head_offset = 0;
										tail_offset = 0;
										data_len = 0;
										resync_flag = 1;
										if(rtp_ts_txt!=NULL)
											fprintf(rtp_ts_txt,"buffer is full it is nothing tail_offset %d head_offset %d datalen %d ret %d avail_len %d unalign_data_len %d %2x%2x%2x %2x%2x%2x%2x%2x%2x%2x%2x%2x%2x\n"
											,tail_offset, head_offset, data_len, avail_len,avail_len , unalign_data_len,
											temp_buf[0],temp_buf[1],temp_buf[2], temp_buf[18], temp_buf[19], temp_buf[20],
											temp_buf[21], temp_buf[22], temp_buf[23], temp_buf[24], temp_buf[25],
											temp_buf[26], temp_buf[27]);
									}
								}
recv_loop_end:
								memcpy(temp_buf,&temp_buf[avail_len],unalign_data_len);



							}
							ALOGV("data_len %d tail_offset %d ret %d",data_len ,tail_offset,avail_len);
						}
					}while(ret > 0);
					unalign_len[i] = unalign_data_len;

				}
				if (fds[i].revents & POLLHUP)
				{
					ALOGD(" disconnect fd %d    %d",fds[0].revents,fds[1].revents);
					shutdown(fds[i].fd, SHUT_RDWR);
					close(fds[i].fd);
					fds[i].fd = -1;
					break;

				}
				if(fds[i].revents & (POLLERR  | POLLNVAL))
				{
					ALOGD("disconnect error  %d",fds[1].revents);

					shutdown(fds[i].fd, SHUT_RDWR);
					close(fds[i].fd);
					fds[i].fd = -1;
					break;
				}
			}
		}
		int64_t First_time;
		First_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
		if(((First_time- rtp_start_time)/1000000)/5 > sum_time)
		{
			int temp;
			sum_time++;
			cur_video_latency = video_latency[1];
			//if(video_latency[0] < video_latency[1])
				//cur_video_latency = video_latency[1];
			if(video_latency[1] < video_latency[2])
				cur_video_latency = video_latency[2];
			//video_latency[0] = video_latency[1];
			video_latency[1] = video_latency[2];
			video_latency[2] = 0;
			if(rtp_ts_txt!=NULL)
		fprintf(rtp_ts_txt,"RtpSource sum_time plus %d First_time %lld cur latency %lld %lld adjustment %lld %lld video_latency %lld %lld %lld audio_latency %lld %lld %lld\n",
			sum_time,First_time,cur_audio_latency,cur_video_latency,audio_adjustment,video_adjustment,
			audio_latency[0],audio_latency[1],audio_latency[2],video_latency[0],video_latency[1],video_latency[2]);
		}

		if(Rtp_Audio_Start_TimeUs == 0)
			video_latency[2] = 0;
		else
		{
			if(video_latency[2] < First_time - Rtp_Audio_Start_TimeUs - latency_time - video_timeUs)
				video_latency[2] = First_time - Rtp_Audio_Start_TimeUs - latency_time - video_timeUs;
		}
		if(video_latency[2] > cur_video_latency)
			cur_video_latency = video_latency[2];

        if(cur_video_latency < 200000ll)
				video_adjustment = 200000ll;//200000ll;//30000000ll;//200000ll;

		else	if(cur_video_latency < 500000ll)
			video_adjustment = 500000ll;//20000000ll;//500000ll;
		else	if(cur_video_latency < 1000000ll)
			video_adjustment = 1000000ll;//20000000ll;//1000000ll;
		else	if(cur_video_latency < 3000000ll)
			video_adjustment = 3000000ll;//20000000ll;//3000000ll;
		else
			video_adjustment = 5000000ll;//20000000ll;//5000000ll;
		#if 0
		audio_latency[2] = 0;
		audio_latency[2] = First_time - Rtp_Audio_Start_TimeUs - audio_timeUs;
		if(audio_latency[2] > cur_audio_latency)
			cur_audio_latency = audio_latency[2];

		if(cur_audio_latency < 200000ll)
			audio_adjustment = 200000ll;

		else	if(cur_audio_latency < 500000ll)
			audio_adjustment = 500000ll;
		else	if(cur_audio_latency < 1000000ll)
			audio_adjustment = 1000000ll;
		else	if(cur_audio_latency < 3000000ll)
			audio_adjustment = 3000000ll;
		#endif
		if(rtp_ts_txt!=NULL)
		fprintf(rtp_ts_txt,"RtpSource sum_time %d First_time %lld cur latency %lld %lld adjustment %lld %lld video_latency %lld %lld %lld audio_latency %lld %lld %lld\n",
			sum_time,First_time,cur_audio_latency,cur_video_latency,audio_adjustment,video_adjustment,
			audio_latency[0],audio_latency[1],audio_latency[2],video_latency[0],video_latency[1],video_latency[2]);
	}
Connecttiong_end_2:
	ALOGD("exit loop");
	do_end = 1;
	connect_flag = 0;
	pthread_exit (0);
	return NULL;
}
void RtpSource::queueEOS(status_t finalResult) {
	Mutex::Autolock autoLock(mLock);
    ALOGD("RtpSource::queueEOS finalResult %d",finalResult);
	if(finalResult != OK)
		ALOGD("RtpSource::queueEOS finalResult %d is false",finalResult);

    if(finalResult == INFO_TIME_OUT)
    {
		finalResult = OK;
    }
    mFinalResult = finalResult;
}

ssize_t RtpSource::readAt(off64_t offset, void *data, size_t size) {


	int ret = size;
	int size1 = size;
	Mutex::Autolock autoLock(mLock);
	if(size == 11)
	{
		if(player_type == 0)
		{
			char temp[12] = "RK_WIMO";
			ALOGD("read size == 11 temp %s",temp);
	     memcpy(data,temp,12);
		}
		else if(player_type == 1)
		{
			char temp[12] = "RK_GPU_STRM";
			ALOGD("read size == 11 temp %s",temp);
	     	memcpy(data,temp,12);
		}
		else
			return -1111;
	     return 11;
	}
	if(mFinalResult == OK || connect_flag==0)
	{
		ALOGD("RtpSource::readAt mFinalResult %d or disconnect  connect_flag %d",mFinalResult,connect_flag);
		return -1111;
	}


	{
		static int last_file_offset = 0;
		int64_t timeFirst,timesec;

      	//timeFirst = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
		if(data_len < size)
		{
			//timesec = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;

			ALOGV("data_len < size file_offset + data_len  %d %d,  offset + size %d %d  time  %lld  %lld waitting",
			file_offset , data_len, offset , size,timeFirst,timesec);
			return -2111;
		}
		char* data_1 = (char*)data;
		ALOGV("data_len %d size %d tail %d head %d %2x%2x%2x%2x%2x%2x%2x%2x",data_len,size,tail_offset,head_offset,data_1[0]
			,data_1[1],data_1[2],data_1[3],data_1[4],data_1[5],data_1[6],data_1[7]);
		memcpy(data,&rec_buf[head_offset],size);//[head_offset + (offset - file_offset) ],size);
		uint8_t *temp_buffer  = &rec_buf[head_offset];

		if(1)
		{
			int ret_rtp;
			if((ret_rtp = access("data/test/rtpsource_test_file",0)) == 0)//test_file!=NULL)
			{

				if(rtpsource_read_data_file !=NULL)
				{
					fwrite(data,size  ,1,rtpsource_read_data_file);
				}
				else
				{
					rtpsource_read_data_file = fopen("data/test/rtpsource_test_264_read_file","wb+");
					if(rtpsource_read_data_file !=NULL)
					{
						fwrite(data,size  ,1,rtpsource_read_data_file);
					}
				}
			}
			else
			{
				if(rtpsource_read_data_file!=NULL)
				{
					fclose(rtpsource_read_data_file);
					rtpsource_read_data_file = NULL;
				}
			}
		}
		data_len -= size;
		head_offset += size;
		head_offset %= 6291984;
		file_offset+=size;
		last_file_offset = file_offset;
	}
	return ret;


}

status_t RtpSource::getSize(off64_t *size) {


    *size = 0x8000000;
    return OK;
}

}  // namespace android
