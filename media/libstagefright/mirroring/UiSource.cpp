#define LOG_TAG "UiSource"

#include <vpu_mem.h>

#include "UiSource.h"
 namespace android {
#define MIRRORING_IOCTL_MAGIC 0x60
#define MIRRORING_START				_IOW(MIRRORING_IOCTL_MAGIC, 0x1, unsigned int)
#define MIRRORING_STOP				_IOW(MIRRORING_IOCTL_MAGIC, 0x2, unsigned int)
#define MIRRORING_SET_ROTATION			_IOW(MIRRORING_IOCTL_MAGIC, 0x3, unsigned int)
#define MIRRORING_DEVICE_OKAY			_IOW(MIRRORING_IOCTL_MAGIC, 0x4, unsigned int)
#define MIRRORING_SET_TIME			_IOW(MIRRORING_IOCTL_MAGIC, 0x5, unsigned int)
#define MIRRORING_GET_TIME			_IOW(MIRRORING_IOCTL_MAGIC, 0x6, unsigned int)
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

#define MIRRORING_VIDEO_OPEN				_IOW(MIRRORING_IOCTL_MAGIC, 0x11, unsigned int)
#define MIRRORING_VIDEO_CLOSE			_IOW(MIRRORING_IOCTL_MAGIC, 0x12, unsigned int)
#define MIRRORING_VIDEO_GET_BUF			_IOW(MIRRORING_IOCTL_MAGIC, 0x13, unsigned int)



#define	MIRRORING_AUDIO_SET_BUFFER_SIZE		0xa1
#define	MIRRORING_AUDIO_SET_BYTEPERFRAME		0xa2


#define MIRRORING_COUNT_ZERO				-111
#define VIDEO_ENCODER_CLOSED			-222
#define AUDIO_ENCODER_CLOSED			-222
#define ENCODER_BUFFER_FULL			-333


#define MIRRORING_IOCTL_ERROR			-1111

 void rgbtoyuv(unsigned char* rgb,unsigned char*yuv,int width,int height)
{
	int i,j;
	unsigned long* rgb32 = (unsigned long*)rgb;
	unsigned char* y = yuv;
	unsigned char* u = yuv + width * height;
	unsigned char* v = yuv + width * height  + 512 * 320;
	unsigned long pixel;
	unsigned char Y,U,V;
	unsigned long R,G,B;
	unsigned short* uv =(unsigned short*)( yuv + width * height);
	for(i = 0; i < height; i++)
	{
		for(j = 0; j < width; j+=2)
		{
			pixel = rgb32[i * width + j];

			y[i * width + j] = (((pixel & 0xff) * 0x838 + ((pixel >> 8 ) & 0xff) * 0x1023 + ((pixel >> 16 ) & 0xff) *0x322) >> 13) + 16;
		
			
			pixel = rgb32[i * width + j + 1];
			y[i * width + j + 1] =(((pixel & 0xff) * 0x838 + ((pixel >> 8 ) & 0xff) * 0x1023 + ((pixel >> 16 ) & 0xff) *0x322) >> 13) + 16;
		
			if((i & 1 )==0)
			{
				pixel = rgb32[i * width + j];
				R = rgb32[i * width + j] & 0xff;
				G = (rgb32[i * width + j] >> 8) & 0xff;
				B = (rgb32[i * width + j] >> 16 ) &0xff;
				//u[i * width /4 + j/2] =  (  (   B *0x708  - R * 0x25f - G * 0x4A8)  /0x1000) + 128;
			//	v[i * width /4 + j /2] = ((  (   (R * 0xE10) - (G * 0xBC4) - B *0x249) )    >> 13) + 128;//((  (   (R * 0.615) - (G * 0.515) - B *0.1) )    )+128 ;	
				uv[i * width /4 + j /2] =	( (  (   B *0x708  - R * 0x25f - G * 0x4A8)  /0x1000) + 128) | ((((  (   (R * 0xE10) - (G * 0xBC4) - B *0x249) )    >> 13) + 128)<< 8);
			}
		}
	}
}

void rgb565toyuv(unsigned char* rgb,unsigned char*yuv,int width,int height)
{
	int i,j;
	unsigned short* rgb16 = (unsigned short*)rgb;
	unsigned char* y = yuv;
	unsigned char* u = yuv + width * height;
	unsigned char* v = yuv + width * height  + 512 * 384;
	unsigned short* uv =(unsigned short*)( yuv + width * height);
	unsigned long pixel;
	unsigned char Y,U,V;
	unsigned long R,G,B;
	for(i = 0; i < height; i++)
	{
		for(j = 0; j < width; j+=2)
		{
			pixel = rgb16[i * width + j];

			y[i * width + j] =  (((pixel >>11) * 0x41C0 + ((pixel >> 5 ) & 0x3f)* 0x408C + (pixel & 0x1f )* 0x1910) >> 13) + 16;
		
			
			pixel = rgb16[i * width + j + 1];
			y[i * width + j + 1] =(((pixel >>11) * 0x41C0 + ((pixel >> 5 ) & 0x3f)* 0x408C + (pixel & 0x1f )* 0x1910) >> 13) + 16;
		
			if((i & 1 )==0)
			{
				pixel = rgb16[i * width + j];
				R = (rgb16[i * width + j] >> 11 ) << 3;
				G = ((rgb16[i * width + j] >> 5) & 0x3f)<<2;
				B = (rgb16[i * width + j] &0x1f ) <<3;
			//	u[i * width /4 + j/2] =  
				//	(  ((rgb16[i * width + j] &0x1f )*0x3840- (rgb16[i * width + j] >> 11 ) * 0x12F8 - ((rgb16[i * width + j] >> 5) & 0x3f) * 0x12A0)  /0x1000) + 128;

			//	v[i * width /4 + j /2] = ((  (((rgb16[i * width + j] >> 11 ) * 0x7080) - (((rgb16[i * width + j] >> 5) & 0x3f) * 0x2F10) - (rgb16[i * width + j] &0x1f ) *0x1248) )    >> 13) + 128;//((  (   (R * 0.615) - (G * 0.515) - B *0.1) )    )+128 ;	
				uv[i * width /4 + j /2] = ((  ((rgb16[i * width + j] &0x1f )*0x3840- (rgb16[i * width + j] >> 11 ) * 0x12F8 - ((rgb16[i * width + j] >> 5) & 0x3f) * 0x12A0)  /0x1000) + 128) | 
					((((  (((rgb16[i * width + j] >> 11 ) * 0x7080) - (((rgb16[i * width + j] >> 5) & 0x3f) * 0x2F10) - (rgb16[i * width + j] &0x1f ) *0x1248) )    >> 13) + 128) << 8);
			}
		}
	}
}
FILE *uisource_output;
#define RK30 1
#define RK29 0
UiSource::UiSource(int width, int height, int colorFormat)
        : mWidth(width),
          mHeight(height),
          mColorFormat(colorFormat),
          mSize((width * height * 3) / 2),
          mStarted(false),
          fd( -1),
          ui_source_start_time_us(0){
	last_value = 0;
	output = input = uisource_output = NULL;
	// uisource_output = fopen("/data/uisource_output.txt","wb");
	//  output =  fopen("/data/test/out.rgb","wb");
	//  input = fopen("/data/test_dec44.yuv","rb");

	int ret = 0;
	int i;
	unsigned long para_buff[6];
	
	VPUMemLinear_t* ui_buf[2];
	
	int err = 0;
	for(i = 0; i < 2; i++)
	{
		
		ui_buf[i] = new VPUMemLinear_t; 
		ui_buf_void[i] = (void *)ui_buf[i];
		err = VPUMallocLinear(ui_buf[i], ((mWidth+ 15) & 0xfff0) * mHeight * 4);
		if (err)
		{
			ALOGD("err  %dtemp->phy_addr %x mWidth %d mHeight %d",err,ui_buf[i]->phy_addr,mWidth,mHeight);
			return;
		}
		ALOGD("ui_buf[%d]->phy_addr %x ui_buf_void %x %x",i,ui_buf[i]->phy_addr,ui_buf_void[0],ui_buf_void[1]);
	}
				
					  
	fd = open("/dev/mirroring", 00000002, 0);
	if (fd < 0)
	{
		
		ALOGD("fd is error %d",fd);
		return ;
	}
	
	{
		para_buff[0] =	mWidth;
		para_buff[1] =	mHeight;
		para_buff[2] =	2;
		for(i = 0; i < 2; i++)
		{
			para_buff[3+i] =	ui_buf[i]->phy_addr;
		}
		if (ioctl(fd, MIRRORING_VIDEO_OPEN, para_buff) < 0)
		{
			ALOGD("ioctl MIRRORING_VIDEO_OPEN error re   1");
			ioctl(fd, MIRRORING_VIDEO_CLOSE, NULL);
			close(fd);
			fd = -1;
			return ;
		}
		char *tmp = (char*)para_buff;
		if (ioctl(fd, MIRRORING_GET_CHIPINFO, tmp) < 0)
		{
			ALOGD("ioctl MIRRORING_GET_CHIPINFO error re   1 %x",MIRRORING_GET_CHIPINFO);
			ioctl(fd, MIRRORING_VIDEO_CLOSE, NULL);
			close(fd);
			fd = -1;
			return ;
		}
		if(!strcmp( tmp, "29"))
			chip_type = RK29;
		else if (!strcmp( tmp, "30"))
			chip_type = RK30;
		else
		{
			ALOGD("ioctl MIRRORING_VIDEO_OPEN error chip_type %s",tmp);
			ioctl(fd, MIRRORING_VIDEO_CLOSE, NULL);
			chip_type = -1;
			close(fd);
			fd = -1;
			return ;
		}
	}
	ALOGD("UiSource::UiSource output %x  error %d mWidth %d %d chip_type %d",output,errno,mWidth,mHeight,chip_type);
}
UiSource::~UiSource() 

{
	ALOGD(" UiSource::~UiSource");
	int i;
	VPUMemLinear_t* ui_buf; 

	
	
	if(mStarted == true)
		stop();
	if(fd >=0)
	{
		if(ioctl(fd, MIRRORING_VIDEO_CLOSE, NULL) <  -1)
		{
			ALOGD("ioctl error 0");
		}
		close(fd);
	}
	if(uisource_output)
		fclose(uisource_output);
	if(input)
		fclose(input);
	if(output)
		fclose(output);
	
	for(i = 0; i < 2; i++)
	{
		ui_buf = (VPUMemLinear_t*)ui_buf_void[i];
		if(ui_buf)
		{
			VPUFreeLinear(ui_buf);
			free(ui_buf);
		}
	}
	
	ALOGD(" UiSource::~UiSource exit");
}
sp<MetaData> UiSource::getFormat() {
    sp<MetaData> meta = new MetaData;
    meta->setInt32(kKeyWidth, mWidth);
    meta->setInt32(kKeyHeight, mHeight);
    meta->setInt32(kKeyColorFormat, mColorFormat);
    meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_RAW);

    return meta;
}

status_t UiSource::start(MetaData *params) {
	if(!(mColorFormat == OMX_COLOR_FormatYUV420SemiPlanar || mColorFormat == OMX_COLOR_FormatYUV420Planar))
	{
		ALOGD("UiSource::start colorFormat is wrong %d",mColorFormat);
		return UNKNOWN_ERROR;
	}
	else	if(fd < 0)
	{
		ALOGD("UiSource::start error  fd is wrong %d",fd);
		return -1111;
		
	}
	else	if(ui_buf_void[0] == NULL || ui_buf_void[1]==NULL)
	{
		ALOGD("can't alloc vpumem ui_buf %x %x",ui_buf_void[0],ui_buf_void[1]);
		return -1111;
	}
	
  	mStarted = true;
	return OK;
}

status_t UiSource::stop() {
	ALOGD("UiSource::stop()");
	if(mStarted == false)
	{
		ALOGD("UiSource can't be started before stop 0");
		return UNKNOWN_ERROR;
	}
	return OK;
}
extern	FILE *test_ts_txt;

 status_t UiSource::read(
        MediaBuffer **buffer, const MediaSource::ReadOptions *options) {
            
	VPUMemLinear_t* ui_buf ;
    #if 1
    
	struct timeval timeFirst,timesec;
	static int video_frame = 0;
	status_t err = OK;// = mGroup.acquire_buffer(buffer);
	int phy_addr = 0;
    int imgtype = 0;
	static int count = 0;
	int64_t Timestamp;
	int64_t cur_Timestamp,last_Timestamp;
	int	try_time =3;
	int ret = 0;
	temp[1] = -2;
	static int ui_time=0;
	if(mStarted == false)
		return -1111;
	ui_time++;
	if((ui_time%100)==0)
		ALOGV("ui_time 1 %d ui_source_start_time_us %lld",ui_time,ui_source_start_time_us);
	if(ui_source_start_time_us == 0 )
	{
		ret = ioctl(fd,MIRRORING_GET_TIME,&ui_source_start_time_us);
		ALOGD("ui_source_start_time_us 11d11 %lld ",ui_source_start_time_us);
		if(ret < 0)
		{
			ALOGD("UiSource gettime error");
			return -1111;
		}
	}	
	if(ui_source_start_time_us == 0 )
	{
		ALOGD("can't get ui_source_start_time_us");
		return -2111;
	}
	do
	{
		if(try_time<=0)
			return -2111;
		if((ret = ioctl(fd, MIRRORING_VIDEO_GET_BUF,temp)) < 0)
		if(temp[7]==1)
			usleep(5000);
		try_time--;
		
	}
	while (temp[7] == 1);
	
	if(temp[1] > 1 || temp[7] !=0)
	{
		ALOGD("buffer index is error ");
		return -1111;
	}
	memcpy(&Timestamp,&temp[2],8);
	memcpy(&cur_Timestamp,&temp[4],8);
	if(ui_source_start_time_us > Timestamp)
		Timestamp = ui_source_start_time_us;
	ALOGV("temp[1] %d ",temp[1]);
	ui_buf = ((VPUMemLinear_t*)ui_buf_void[temp[1]]);
	
	if((temp[0]&0xff) ==1)
	{
	
		VPUMemFlush(ui_buf);
		(*buffer) = new MediaBuffer(ui_buf->vir_addr,mSize);	
		imgtype = OMX_COLOR_FormatYUV420SemiPlanar;
		phy_addr = ui_buf->phy_addr;
		if(output)//temp[6]==638)
		{
			fwrite(ui_buf->vir_addr,mWidth*mHeight*3/2,1,output);	//mWidth*mHeight*3/2,1,output);	
			fflush(output);
		}

	}	
	else
	{

    	if((temp[0] >> 16)== 32)
    	{
			(*buffer) = new MediaBuffer(ui_buf->vir_addr,mWidth*mHeight*4);//(ui_buf->vir_addr,1024*768*4);
			if(chip_type == 0)
				imgtype = OMX_COLOR_Format32bitARGB8888;//OMX_COLOR_Format32bitARGB8888;   //OMX_COLOR_Format32bitBGRA8888;//OMX_COLOR_Format32bitARGB8888;//OMX_COLOR_Format32bitARGB8888;
			else
			imgtype = OMX_COLOR_Format32bitBGRA8888;//OMX_COLOR_Format32bitARGB8888;   //OMX_COLOR_Format32bitBGRA8888;//OMX_COLOR_Format32bitARGB8888;//OMX_COLOR_Format32bitARGB8888;

    	}
	    else
		{
			(*buffer) = new MediaBuffer(ui_buf->vir_addr,mWidth*mHeight*2);//(ui_buf->vir_addr,1024*768*2);
			imgtype = OMX_COLOR_Format16bitRGB565;
			
		}
		phy_addr = ui_buf->phy_addr;
		
	}
	ALOGV("OMX_COLOR_Format32bitARGB8888 %d imgtype %d",OMX_COLOR_Format32bitARGB8888,imgtype);
	if(count %20 == 0 && count == 0)
		ALOGD("Uisource ui_source_start_time_us %lld curtime %lld cur time %lld lasttime %lld delta time  %lld    rgb_num %d yuv_num %d temp %d %d %d imgtype %d ui_buf->phy_addr %x count %d",
			ui_source_start_time_us,Timestamp, cur_Timestamp,Timestamp - ui_source_start_time_us,
			cur_Timestamp - last_Timestamp,
			temp[0],temp[1],temp[6] ,imgtype,ui_buf->phy_addr,count);
 	ALOGV("OMX_COLOR_Format32bitARGB8888 %d imgtype %d",OMX_COLOR_Format32bitARGB8888,imgtype);
 	(*buffer)->set_range(0, mSize);
 	(*buffer)->meta_data()->clear();
	if( Timestamp - ui_source_start_time_us < 200000ll)
		(*buffer)->meta_data()->setInt64(kKeyTime, (Timestamp - ui_source_start_time_us) / 10);//(mNumFramesOutput * 1000000) / 24);
	else
		(*buffer)->meta_data()->setInt64(kKeyTime, Timestamp - ui_source_start_time_us -180000ll );//-200000ll);// (int64_t)timeFirst.tv_sec * 1000000ll + timeFirst.tv_usec - ui_source_start_time_us -200000ll);//(mNumFramesOutput * 1000000) / 24);
	(*buffer)->meta_data()->setInt32(kKeyColorFormat,imgtype);
	(*buffer)->meta_data()->setInt32(kKeyBusAdds,phy_addr);
	
	#endif
	last_Timestamp = cur_Timestamp;
	count++;
	{
		int ret_ui;
	 	if((ret_ui = access("data/test/test_ts_txt_file",0)) == 0)//test_file!=NULL)
		{
			if(test_ts_txt == NULL)
				test_ts_txt = fopen("data/test/test_ts_txt.txt","wb+");
			else
			{
				int64_t cur_time;
				static  int64_t last_video_time_us;
				static  int64_t last_video_systime;
				int64_t timeUs = Timestamp - ui_source_start_time_us -180000ll;
				cur_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;		
				
				fprintf(test_ts_txt,"Uisource sys time %lld timeUs %lld delta sys time  %lld delta time   %lld   ,ui_source_start_time_us %lld\n",
					cur_time,timeUs,cur_time - last_video_systime,timeUs - last_video_time_us,ui_source_start_time_us);
				last_video_time_us = timeUs;
				last_video_systime = cur_time;
					
			}
		}
		else
		{
			
			if(test_ts_txt!=NULL)
			{
				fclose(test_ts_txt);
				test_ts_txt = NULL;
			}
			ALOGV("test_ts_txt==NULL ret %d",ret_ui);
		}
	}
	return OK;
}
}

