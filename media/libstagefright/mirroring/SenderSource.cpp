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
#define LOG_TAG "SenderSource"

#include <media/stagefright/SenderSource.h>
//#include <media/stagefright/MediaDebug.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <utils/CallStack.h>
namespace android {


SenderSource::SenderSource(unsigned long addr, unsigned short remort_port,unsigned short local_port,int		type)//type means rk mirroring or mirroring
	:connect_flag(1),
	end_flag(0)
{
	int on = 1;
    int rc;
	int err;
	fds[0].fd = -1;
	fds[0].events = POLLIN;
	fds[1].fd = -1;
	fds[1].events = POLLIN;
    rx_addr = addr;
	rx_port = remort_port;
	tx_port = local_port;
	ALOGD("SenderSource rx_addr %x rx_port %d tx_port %d",rx_addr,rx_port,tx_port);
	mType 	= type;
	mSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if(mSocket < 0)
	{
		ALOGD("Socket failed in init");
		return;
	}
    rc = setsockopt(mSocket, SOL_SOCKET,  SO_REUSEADDR, (&on), sizeof(on));
    if (rc < 0) {
		

        close(mSocket);
	    mSocket = -1;
        ALOGE("mpeg2tswriter sender: failed to setsockopt\n");
	  return ;
	}
    memset(mRTPAddr.sin_zero, 0, sizeof(mRTPAddr.sin_zero));
    mRTPAddr.sin_family = AF_INET;
	mRTPAddr.sin_addr.s_addr =  htonl(rx_addr);//inet_addr(url_addr);//addr;//inet_addr(url_addr);//addr;//INADDR_ANY;//addr;//INADDR_ANY;//addr;
    mRTPAddr.sin_port = htons(rx_port);
	memset(mRTCPAddr.sin_zero, 0, sizeof(mRTCPAddr.sin_zero));
    mRTCPAddr.sin_family = AF_INET;
    mRTCPAddr.sin_addr.s_addr = INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;
    mRTCPAddr.sin_port = htons(tx_port);//htons(5666);//(local_port);
    err = bind(mSocket, (const struct sockaddr *)(&mRTCPAddr),sizeof(mRTCPAddr));
	if(err)
	{
		close(mSocket);
	  	mSocket = -1;
        ALOGE("mpeg2tswriter bind 1 : failed to setsockopt mSocket %d\n",mSocket);
		return ;
	}
	ALOGD("SenderSource::SenderSource mRTPAddr.sin_addr.s_addr is %x,mRTPAddr.sin_port %x   err %d mflag %d\n",
	   	 	mRTPAddr.sin_addr.s_addr,mRTPAddr.sin_port,err,mType);
	if(mType == 1)
		pthread_create(&mThread, NULL, rec_data, this);
}
void* SenderSource::rec_data(void* me)
{
	SenderSource* mSenderSource = static_cast<SenderSource *>(me);
	ALOGD("%x SenderSource %x",me,mSenderSource);
	mSenderSource->ThreadWrapper(NULL);
	return NULL;
}
void *SenderSource::ThreadWrapper(void *)
{
	int ret = 0;
	int len = sizeof(mRTPAddr_RTP[1]);
	int on = 1;
    int rc;
	int err;
	int	flag = 1;
	struct sockaddr_in  mRTPAddr_RTP_temp;
    fds[0].fd= socket(AF_INET, SOCK_STREAM, 0);
	fds[0].events = POLLIN;
	fds[0].revents = 0;
	fds[1].fd = -1;
	fds[1].events = 0;//POLLIN;
	fds[1].revents = 0;
	if(fds[0].fd < 0)
	{
        ALOGE("SenderSource open : failed  fds[0].fd %d errorno %d\n",fds[0].fd,errno);
	    goto thread_end;
	}
	if((flag = fcntl(fds[0].fd, F_GETFL,O_NONBLOCK)) < 0)
	{
		ALOGD("fcntlGETFL error");
		goto thread_end;
	}
    rc = setsockopt(fds[0].fd, SOL_SOCKET,  SO_REUSEADDR, (&on), sizeof(on));
    if (rc < 0) {
		shutdown(fds[0].fd, SHUT_RDWR);
        close(fds[0].fd);
	  	fds[0].fd = -1;
		end_flag = 1 ;
        ALOGE("SenderSource sender: failed to setsockopt %d\n",errno);
	    goto thread_end;
    }		
	ret = setsockopt( fds[0].fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag) );
	if (ret == -1) {
	  ALOGD("Couldn't setsockopt\n");
	  goto thread_end;
	}
	memset(mRTPAddr_RTP[0].sin_zero, 0, sizeof(mRTPAddr_RTP[0].sin_zero));
    mRTPAddr_RTP[0].sin_family = AF_INET;
    mRTPAddr_RTP[0].sin_addr.s_addr = INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;//inet_addr(ip_addr);//INADDR_ANY;
    mRTPAddr_RTP[0].sin_port = htons(tx_port+1);//htons(5666);//(local_port);
    err =     bind(fds[0].fd, (const struct sockaddr *)(&mRTPAddr_RTP[0]),sizeof(mRTPAddr_RTP[0]));
	if(err)
	{
		shutdown(fds[0].fd, SHUT_RDWR);
		close(fds[0].fd);
	  	fds[0].fd = -1;
		end_flag = 1;
        ALOGE("SenderSource bind: failed to %d fds[0].fd %d addr %x port %x\n",
			errno,fds[0].fd,mRTPAddr_RTP[0].sin_addr.s_addr,mRTPAddr_RTP[0].sin_port);
	    goto thread_end;
	}
	ret = listen(fds[0].fd, 1);
	ALOGD("SenderSource::ThreadWapper 3 mRTPAddr_RTP[0].sin_addr.s_addr is %x,sin_port %x fds.fd[0] %d  %d %d %d  ret  %d errno %d\n",
   	 	mRTPAddr_RTP[0].sin_addr.s_addr,mRTPAddr_RTP[0].sin_port,fds[0].fd,
   	 	mRTPAddr_RTP[1].sin_addr.s_addr,mRTPAddr_RTP[1].sin_port,fds[1].fd,ret,errno);




	while(!end_flag)
	{
		if ((ret = poll(fds, 2, 50)) > 0)//poll(fds, 2, -1) > 0)  -1 ==inftim
		{
			ALOGV("fds[0].revents %d %d fds[1] %d %d addr  %x addr %x %x",
				fds[0].revents,fds[0].fd,fds[1].revents,fds[1].fd,mRTPAddr_RTP[0].sin_port,
				mRTPAddr_RTP[1].sin_addr.s_addr,mRTPAddr_RTP[1].sin_port);
			if (fds[0].revents & POLLIN)
			{
                int client_sock = accept(fds[0].fd, (sockaddr*)&mRTPAddr_RTP[1], &len);
				ALOGD(" Socket addr %x port  %x len %d errno %d ret %d client_sock %d",
					mRTPAddr_RTP[1].sin_addr.s_addr,mRTPAddr_RTP[1].sin_port,len,errno,ret,client_sock);
				getpeername(fds[1].fd,(sockaddr*)&mRTPAddr_RTP_temp, &len);
					ALOGD(" Socket 1addr %x port  %x len %d errno %d ret %d client_sock %d",
					mRTPAddr_RTP_temp.sin_addr.s_addr,mRTPAddr_RTP_temp.sin_port,len,errno,ret,client_sock);
				
                if (client_sock >= 0) {
		      		fds[1].fd = client_sock;
					struct timeval nNetTimeout;
					//nNetTimeout.tv_sec = 0;
					//nNetTimeout.tv_usec = 10;
				//	setsockopt(fds[1].fd,SOL_SOCKET,SO_SNDTIMEO,(char *)&nNetTimeout,sizeof(int));
					connect_flag = 1;
                } else {
                    ALOGE("receiver: accept client %d failed\n", client_sock);
					break;
                }				
			}
			if (fds[0].revents & POLLHUP)
			{
				ALOGD("sender shutdown fds[0] %d  fds[1]  %d",fds[0].revents,fds[1].revents);
				shutdown(fds[0].fd, SHUT_RDWR);
				close(fds[0].fd);
				fds[0].fd = -1;
				break;
	                    
			}
			if (fds[1].fd > 0)
			{
				
				if (fds[1].revents & POLLIN)
				{
					static int sign = 0;
					if(sign == 0)
						{
					ALOGD("fds0 %d  errno %d",fds[1].revents,errno);
					sign = 1;
						}
					shutdown(fds[1].fd, SHUT_RDWR);
					close(fds[1].fd);
					fds[1].fd = -1;
					break;
				}
				if (fds[1].revents & POLLHUP)
				{
					getsockname(fds[1].fd,(sockaddr*)&mRTPAddr_RTP_temp, &len);
					ALOGD("fds 1 %d  %d errno %d %d %d",fds[1].fd,fds[1].revents,errno,mRTPAddr_RTP_temp.sin_addr.s_addr,mRTPAddr_RTP_temp.sin_port);
					
					shutdown(fds[1].fd, SHUT_RDWR);
					close(fds[1].fd);
					fds[1].fd = -1;
					break;
				}
				if(fds[1].revents & (POLLERR  | POLLNVAL))
				{
					ALOGD("fds 2 error  %d errno %d",fds[1].revents,errno);
					shutdown(fds[1].fd, SHUT_RDWR);
					close(fds[1].fd);
					fds[1].fd = -1;
					break;
				}	
			}
			
		}
	}
thread_end:
    do_end = 1;
	connect_flag = 0;
	ALOGD("SenderSource::SenderSource mRTPAddr_RTP[0].sin_addr.s_addr is %x,mRTPAddr_RTP[0].sin_port %x     %d fds %d %d do_end %d\n",
   	 	mRTPAddr_RTP[0].sin_addr.s_addr,mRTPAddr_RTP[0].sin_port,ret,fds[0].fd,fds[1].fd,do_end);
	return NULL;
}


SenderSource::~SenderSource() 
{
	void* retval1;
	connect_flag = 0;
   	pthread_join(mThread, &retval1);
	if(mSocket >= 0)
	{
  	  close(mSocket);
  	  mSocket = -1;	
	}
	if(mType == 1)
	{
		if(fds[0].fd >= 0)
		{
		  shutdown(fds[0].fd, SHUT_RDWR);
	  	  close(fds[0].fd);
	  	  fds[0].fd = -1;	
		}
		if(fds[1].fd >= 0)
		{
		  shutdown(fds[1].fd, SHUT_RDWR);
	  	  close(fds[1].fd);
	  	  fds[1].fd = -1;	
		}
	}
	ALOGD("SenderSource::~SenderSource mType %d fd %d %d",mType,fds[0].fd,fds[1].fd);
}
		



status_t SenderSource::initCheck() const {
	if(mType == 0)
		return mSocket > -1 ? OK : NO_INIT;
	else if(mType == 1)
		return ((mSocket > -1) && (fds[0].fd > -1) && (fds[1].fd > -1)) ? OK : NO_INIT;
	else
	{
		ALOGD("SenderSource::initCheck mType %d error  ",mType);
		return UNKNOWN_ERROR;
	}
}
status_t SenderSource::start()
{
	return 0;
}

status_t SenderSource::stop()
{
	end_flag = 1;
	if(mType == 1)
	{
	while(do_end == 0)
    {
    	usleep(10000);
    }
	}
	ALOGD("SenderSource::stop");
	return 0;
}

void SenderSource::queueEOS(status_t finalResult) {
}

ssize_t SenderSource::SendData(const void *data, size_t size,int data_type) 
{
	int ret;
	if(connect_flag != 0)
	{
		char* temp = (char*)data;
		if(mSocket < 0)
		{
			ALOGD("senddata  failed mSocket %d mType %d",mSocket,mType);
			goto Connecttion_failed;
		}
		if(mType == 0)
		{
			
			ret = sendto(mSocket, data, size, 0,(const struct sockaddr *)( &mRTPAddr),sizeof(mRTPAddr));
			ALOGV("internalWrite audio_flag %d msocket %d fds[1].fd %d %2x%2x%2x ret %d size %d",
				AudioStream,mSocket,fds[1].fd,temp[0],temp[1],temp[2],ret,size );
		}
		else	if(mType == 1)
		{
			if(fds[1].fd < 0)
			{
				ALOGD("senddata  failed mSocket %d mType %d",mSocket,mType);
				goto Connecttion_failed;
			}	
		    if ((mSocket >= 0 && data_type == 1) || (mSocket>=0 && fds[1].fd <0))
			{
				ret = sendto(mSocket, data, size, 0,(const struct sockaddr *)( &mRTPAddr),sizeof(mRTPAddr));
				ALOGV("internalWrite audio_flag %d msocket %d fds[1].fd %d %2x%2x%2x ret %d size %d",
					AudioStream,mSocket,fds[1].fd,temp[0],temp[1],temp[2],ret,size );
			}
			else	if(mSocket>=0 && fds[1].fd >=0)
			{
				ret = sendto(
					fds[1].fd, data, size, 0,
					(const struct sockaddr *)( &mRTPAddr_RTP[1]),
					sizeof(mRTPAddr));
				if(ret < 0)
					ALOGD("internalWrite return err video_flag %d msocket %d fds[1].fd %d %2x%2x%2x ret %d errno %d",
						data_type,mSocket,fds[1].fd,temp[0],temp[1],temp[2],ret ,errno);
				if(ret < 0 && (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
				{
					ALOGD("internalWrite video_flag %d msocket %d fds[1].fd %d %2x%2x%2x ret %d errno %d",
						data_type,mSocket,fds[1].fd,temp[0],temp[1],temp[2],ret ,errno);
					return -2111;
				}
			}
		}
		else
		{
			connect_flag = 0;
			ALOGD("ALOGD internalWrite mType err %d ret %d size %d AudioStream %d",mType,ret,size,data_type);
			goto Connecttion_failed;
		}
		if(ret!=size)
		{
		ALOGD("LOGD internalWrite ret %d size %d AudioStream %d errno %d",ret,size,data_type,errno);
		//	connect_flag = 0;
			
			goto Connecttion_failed;
		}

			
		return 0;
	}
Connecttion_failed:

	return -1111;
}



}  // namespace android
