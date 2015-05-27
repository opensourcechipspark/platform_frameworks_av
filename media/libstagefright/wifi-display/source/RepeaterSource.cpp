//#define LOG_NDEBUG 0
#define LOG_TAG "RepeaterSource"
#include <utils/Log.h>

#include "RepeaterSource.h"

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MetaData.h>

#include <sys/ioctl.h>
#include "rga.h"
#include <fcntl.h>
#include <poll.h>
#include <ui/Rect.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/DisplayInfo.h>
#include "gralloc_priv.h"
#define ASYNC_RGA 1
#define	FOR_US_HDCP	0
namespace android {
extern FILE* omx_txt;
RepeaterSource::RepeaterSource(const sp<MediaSource> &source, double rateHz,int width,int height)
    : mStarted(false),
      mSource(source),
      mRateHz(rateHz),
      mWidth(width),
      mHeight(height),
      mBuffer(NULL),
      mResult(OK),
      mLastBufferUpdateUs(-1ll),
      rga_fd(-1),
      mStartTimeUs(-1ll),
      mFrameCount(0),
      mNumPendingBuffers(0),
      maxbuffercount(1),
      mLast_TimeUs(-1ll),
      vpu_mem_index(0),
      mLast_SysTime(-1ll){
#ifdef FOR_TCL
 	  mSrcWidth 	= (width * 96 / 100) 	& 0xfff0;// 32 pixel aligned in width
 	  mSrcHeight	= (height * 96 / 100) 	& 0xfff0;//16 pixel aligned in height
 #else
	  mSrcWidth 	= width;// 32 pixel aligned in width
 	  mSrcHeight	= height;//16 pixel aligned in height 
	
#endif
}

RepeaterSource::~RepeaterSource() {
    CHECK(!mStarted);
}

double RepeaterSource::getFrameRate() const {
    return mRateHz;
}

void RepeaterSource::setFrameRate(double rateHz) {
          ALOGD("RepeaterSource setFrameRate 0");
    Mutex::Autolock autoLock(mLock);
          ALOGD("RepeaterSource setFrameRate 1");

    if (rateHz == mRateHz) {
        return;
    }

    if (mStartTimeUs >= 0ll) {
        int64_t nextTimeUs = mStartTimeUs + (mFrameCount * 1000000ll) / mRateHz;
        mStartTimeUs = nextTimeUs;
        mFrameCount = 0;
    }
    mRateHz = rateHz;
}

status_t RepeaterSource::start(MetaData *params) {
    CHECK(!mStarted);

    status_t err = mSource->start(params);

    if (err != OK) {
        return err;
    }
	#if ASYNC_RGA
	rga_fd  = open("/dev/rga",O_RDWR,0);
	if(rga_fd < 0)
	{
		return rga_fd;
	}
	for(int i = 0; i < maxbuffercount; i++)
	{
		vpuenc_mem[i] = (VPUMemLinear_t*)malloc(sizeof( VPUMemLinear_t)); 
	    ALOGD("mWidth %d mHeight %d",mWidth,mHeight);
		err = VPUMallocLinear((VPUMemLinear_t*)vpuenc_mem[i], ((mWidth + 15) & 0xfff0) * mHeight * 4);
		if (err)
		{
			ALOGD("err  %dtemp->phy_addr %x mWidth %d mHeight %d", err, ((VPUMemLinear_t*)vpuenc_mem[i])->phy_addr, mWidth, mHeight);
			return err;
		}
		#ifdef	FOR_TCL
		memset(vpuenc_mem[i]->vir_addr,0,((mWidth + 15) & 0xfff0) * mHeight * 4);
		#endif
	}
    

	
	
	mUsingTimeUs = mCurTimeUs = 0;
	#endif
    mBuffer = NULL;
    mResult = OK;
    mStartTimeUs = -1ll;
    mFrameCount = 0;

    mLooper = new ALooper;
    mLooper->setName("repeater_looper");
    mLooper->start();

    mReflector = new AHandlerReflector<RepeaterSource>(this);
    mLooper->registerHandler(mReflector);
    mStarted = true;

    postRead();


    return OK;
}

status_t RepeaterSource::stop() {
    ALOGD("RepeaterSource stop");
    CHECK(mStarted);
    mStarted = false;
	{
		Mutex::Autolock autoLock(mLock);
	    while(mNumPendingBuffers>0)
	    {
      		mMediaBuffersAvailableCondition.wait(mLock);
	    }
	    mMediaBuffersAvailableCondition.broadcast();
	}
    if (mLooper != NULL) {
        mLooper->stop();
        mLooper.clear();

        mReflector.clear();
    }
    
    if (mBuffer != NULL) {
        ALOGV("releasing mbuf %p", mBuffer);
        mBuffer->release();
        mBuffer = NULL;
    }
#if ASYNC_RGA
	for(int i = 0; i < maxbuffercount; i++)
	{
		if(vpuenc_mem[i]!=NULL)
		{
			VPUFreeLinear((VPUMemLinear_t*)vpuenc_mem[i]);
			free((VPUMemLinear_t*)vpuenc_mem[i]);
		}
	}
	
	if(rga_fd > 0)
    {
        close(rga_fd);
        rga_fd = -1;
    }
#endif
    status_t err = mSource->stop();
    return err;
}

sp<MetaData> RepeaterSource::getFormat() {
    return mSource->getFormat();
}

status_t RepeaterSource::read(
        MediaBuffer **buffer, const ReadOptions *options) {
    int64_t seekTimeUs;
    ReadOptions::SeekMode seekMode;
    int64_t in_time = ALooper::GetNowUs(); 
		int64_t sys_time2;
		int64_t sys_time3;
    CHECK(options == NULL || !options->getSeekTo(&seekTimeUs, &seekMode));
    while (mStarted && mNumPendingBuffers == maxbuffercount) {
        Mutex::Autolock autoLock(mLock);
        mMediaBuffersAvailableCondition.wait(mLock);
    }
    while(mStarted)
	{
		{
	        bool stale = false;
				
	        Mutex::Autolock autoLock(mLock);
        int64_t bufferTimeUs = -1ll;
	        bool find_frame = true;
	        int64_t nowUs = ALooper::GetNowUs();
	        int64_t nowTimeUs;
			 in_time = nowUs;
	        if (mStartTimeUs < 0ll) {
	          while ((mLastBufferUpdateUs < 0ll || mBuffer == NULL)
                  && mResult == OK) {
            mCondition.wait(mLock);
          }
				if(mBuffer != NULL)
				{
	            mBuffer->meta_data()->findInt64(kKeyTime, &mStartTimeUs);
	            bufferTimeUs = mStartTimeUs;
	            nowTimeUs 	 = bufferTimeUs;
				}
				else
				{
	            find_frame = false;
	            assert(0);
				}
	        } else
	        {
	          mBuffer->meta_data()->findInt64(kKeyTime, &nowTimeUs);
	          if(mLast_TimeUs + 25000ll < nowTimeUs )
	          {
	            bufferTimeUs = nowTimeUs;
	          }
	          else if(mLast_SysTime + 100000ll < nowUs)
	          {
	          	 bufferTimeUs = mLast_TimeUs + 100000ll;
	          }
	          else if(mLast_SysTime + 50000ll < nowUs && mFrameCount < 30)
	          {
	            bufferTimeUs = mLast_TimeUs + 45000;
	          }
	          else
	          {
	            find_frame = false;
	          }
	  		
	  		  ALOGV("RepeaterSource looking  start  %15lld  sys_time %15lld %15lld timeUs %15lld %15lld %15lld delta %15lld %15lld   mFrameCount %d  mBuffer %x \n",
				mStartTimeUs,mLast_SysTime,nowUs , mLast_TimeUs,bufferTimeUs,nowTimeUs ,
				nowUs - mLast_SysTime,(bufferTimeUs - mLast_TimeUs), mFrameCount,mBuffer);
	            
	        }
	        if(!mStarted)
	        {
	          assert(mBuffer!=NULL);
	        }
	  	    if(find_frame)
	        {
	            if (mResult != OK) {
	                CHECK(mBuffer == NULL);
	                return mResult;
	            }
	  		
	    		{
	    			  int retrtptxt;
	    			  int64_t sys_time;
	    			  unsigned long buffer_handle =*((unsigned long*)(mBuffer->data() + mBuffer->range_offset() + 8));
	    			  sys_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;	
	    			 
	    			  if((retrtptxt = access("data/test/omx_txt_file",0)) == 0)//test_file!=NULL)
	    			  {
	    				  
	    				  if(omx_txt == NULL)
	    					  omx_txt = fopen("data/test/omx_txt.txt","ab");
	    				  if(omx_txt != NULL)
	    				  {
	    						fprintf(omx_txt,"RepeaterSource Video  start  %15lld  sys_time %15lld %15lld %15lld timeUs %15lld %15lld %15lld  %15lld  delta %15lld %15lld mUsingTimeUs %15lld mCurTimeUs %15lld  mFrameCount %d buffer_handle %x mBuffer %x	%x %x vpu_mem_index %d this %x mNumPendingBuffers %d\n",
	    						mStartTimeUs,mLast_SysTime, nowUs ,sys_time, mLast_TimeUs,bufferTimeUs,nowTimeUs ,mLastBufferUpdateUs,
	    							nowUs - mLast_SysTime,bufferTimeUs - mLast_TimeUs,mUsingTimeUs, mCurTimeUs
	    						,mFrameCount,buffer_handle,mBuffer,*((long*)(mBuffer->data())),*((long*)(mBuffer->data()+4)) , vpu_mem_index,this,mNumPendingBuffers);
	    				
	    					
	    					fflush(omx_txt);
	    				  }
	    			  }
	    			  
	            }
	  		    nowUs = ALooper::GetNowUs();
	            if (nowUs - mLastBufferUpdateUs > 5000000ll) {
	                mLastBufferUpdateUs = -1ll;
	                stale = true;
	            } else {
	            
#if ASYNC_RGA
	    			
	    			*buffer = new MediaBuffer(24);
	    			char *data = (char *)(*buffer)->data();
	    			uint32_t temp = 0x1234;
	    			buffer_handle_t handle = (buffer_handle_t)*((long*)(mBuffer->data()+16));
	    			memcpy(data + 4, &temp , 4);
	    			memcpy(data + 8, &vpuenc_mem[vpu_mem_index], 4);
	    			memcpy(data + 12, &rga_fd, 4);
					memcpy(data + 16, &handle, 4);
					private_handle_t *mHandle = (private_handle_t*)handle;
					memcpy(data + 20, &(mHandle->share_fd), 4);
	    			if(VPUMemJudgeIommu() == 0 && mCurTimeUs != mUsingTimeUs)
	    			{
	    				const Rect rect(mWidth, mHeight);
	    				uint8_t *img=NULL;
						    				
	    				int res = GraphicBufferMapper::get().lock(handle,
	    						GRALLOC_USAGE_SW_READ_MASK,//GRALLOC_USAGE_HW_VIDEO_ENCODER,
	    						rect, (void**)&img);

	    				if (res != OK) {
	    					ALOGE("%s: Unable to lock image buffer %p for access", __FUNCTION__,
	    						*((long*)(mBuffer->data()+16)));
	    					
	    					GraphicBufferMapper::get().unlock(handle);
	    					return res;
	    				}
	    				else
	    				{
	    					if(rga_fd < 0){
	    						ALOGD("memcpy");  
	    						memcpy(((VPUMemLinear_t*) vpuenc_mem[vpu_mem_index])->vir_addr,img,mWidth * mHeight * 4);
	    					}else{
	    					struct rga_req	Rga_Request;
	    						#ifdef	FOR_TCL
	    						
	    						memset(&Rga_Request,0x0,sizeof(Rga_Request));
	                          
	    						Rga_Request.src.yrgb_addr =  (int)img;
	    						Rga_Request.src.uv_addr  = 0;
	    						Rga_Request.src.v_addr	 =	0;
	    						Rga_Request.src.vir_w = (mSrcWidth + 15)&(~15);
	    						Rga_Request.src.vir_h = mSrcHeight;
	    						Rga_Request.src.format = RK_FORMAT_RGBA_8888;
	    			
	    						Rga_Request.src.act_w = mSrcWidth;
	    						Rga_Request.src.act_h = mSrcHeight;
	    						Rga_Request.src.x_offset = 0;
	    						Rga_Request.src.y_offset = 0;
	    			
	    					
	    			
	    						Rga_Request.dst.yrgb_addr =(int)((VPUMemLinear_t*) vpuenc_mem[vpu_mem_index])->vir_addr;
	    						Rga_Request.dst.uv_addr  = 0;
	    						Rga_Request.dst.v_addr	 = 0;
	    						Rga_Request.dst.vir_w = (mWidth + 15)&(~15);
	    						Rga_Request.dst.vir_h = mHeight;
	    						Rga_Request.dst.format = Rga_Request.src.format;
	    						Rga_Request.dst.act_w = mSrcWidth;
	    						Rga_Request.dst.act_h = mSrcHeight;
	    						Rga_Request.dst.x_offset = (mWidth - mSrcWidth) / 2;
	    						Rga_Request.dst.y_offset = (mHeight - mSrcHeight) / 2 -4;
	    						ALOGV("RepeaterSource  w %d h %d vir w %d h %d act w %d h %d offset w %d h %d",
	    							mWidth,mHeight,Rga_Request.dst.vir_w,Rga_Request.dst.vir_h,
	    							Rga_Request.dst.act_w,Rga_Request.dst.act_h,Rga_Request.dst.x_offset,Rga_Request.dst.y_offset);

	    						
	    						Rga_Request.clip.xmin = 0;
	    						Rga_Request.clip.xmax = (mWidth + 15)&(~15) - 1;
	    						Rga_Request.clip.ymin = 0;
	    						Rga_Request.clip.ymax = mHeight - 1;
	    			
	    						#else
	    						memset(&Rga_Request,0x0,sizeof(Rga_Request));
	    						Rga_Request.src.yrgb_addr =mHandle->share_fd;//jmj  (int)img;
	    						Rga_Request.src.uv_addr  =0;//0;
	    						Rga_Request.src.v_addr	 =	0;
	    						Rga_Request.src.vir_w = mHandle->stride;//(mWidth + 15)&(~15);
	    						Rga_Request.src.vir_h = mHeight;
	    						Rga_Request.src.format = RK_FORMAT_RGBA_8888;
	    			
	    						Rga_Request.src.act_w = mWidth;
	    						Rga_Request.src.act_h = mHeight;
	    						Rga_Request.src.x_offset = 0;
	    						Rga_Request.src.y_offset = 0;
	    			
	    					
	    			
	    						Rga_Request.dst.yrgb_addr =0;//mHandle->share_fd;//jmj (int)((VPUMemLinear_t*) vpuenc_mem[vpu_mem_index])->vir_addr;
	    						Rga_Request.dst.uv_addr  = (int)((VPUMemLinear_t*) vpuenc_mem[vpu_mem_index])->phy_addr;
	    						Rga_Request.dst.v_addr	 = 0;
	    						Rga_Request.dst.vir_w = (mWidth + 15)&(~15);
	    						Rga_Request.dst.vir_h = mHeight;
	    						Rga_Request.dst.format = Rga_Request.src.format;
	    						Rga_Request.dst.act_w = mWidth;
	    						Rga_Request.dst.act_h = mHeight;
	    						Rga_Request.dst.x_offset = 0;
	    						Rga_Request.dst.y_offset = 0;

	    						
	    						Rga_Request.clip.xmin = 0;
	    						Rga_Request.clip.xmax = (mWidth + 15)&(~15) - 1;
	    						Rga_Request.clip.ymin = 0;
	    						Rga_Request.clip.ymax = mHeight - 1;
	    			
	    						#endif

	    						#if SURFACE_ORIGINAL_SIZE
	    						Rga_Request.rotate_mode = 0;// jmj
	    						#else
	    						Rga_Request.rotate_mode = 1;// jmj
	    						Rga_Request.scale_mode = 1;//jmj
	    						Rga_Request.sina = 0;//jmj
	    						Rga_Request.cosa = 65536;//jmj
	    						#endif
	    		//				Rga_Request.mmu_info.mmu_en    = 1;
	    		//				Rga_Request.mmu_info.mmu_flag  = ((2 & 0x3) << 4) | 1;
	    					
	    						int ret;
    	                		sys_time2 = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;	
	    						if(ret=ioctl(rga_fd, RGA_BLIT_ASYNC, &Rga_Request) != 0)
	    						{
	    							ALOGE("RepeaterSource rga RGA_BLIT_SYNC fail %x ret %d",ret);
	    						} 
	    				
	    			  sys_time3 = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;	
    					
									 
	    					}
	    				}
	                  
	                  GraphicBufferMapper::get().unlock(handle);
	                  mUsingTimeUs = mCurTimeUs;
	    			}
	              (*buffer)->setObserver(this);
	              (*buffer)->add_ref();
	              mNumPendingBuffers++;
					if(mNumPendingBuffers>maxbuffercount)
						mNumPendingBuffers=maxbuffercount;
					vpu_mem_index++;
					vpu_mem_index%=maxbuffercount;
#else
	    			*buffer = mBuffer;
	               (*buffer)->add_ref();
#endif
	              (*buffer)->meta_data()->setInt64(kKeyTime, bufferTimeUs);
	              ++mFrameCount;
	              
	        //      
	            }
	            mLast_SysTime = nowUs;
	            mLast_TimeUs = bufferTimeUs;
				if(0)
				 {
    			  int retrtptxt;
    			  int64_t sys_time;
    			  sys_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;	
    			 
    			  if((retrtptxt = access("data/test/omx_txt_file",0)) == 0)//test_file!=NULL)
    			  {
    				  
    				  if(omx_txt == NULL)
    					  omx_txt = fopen("data/test/omx_txt.txt","ab");
    				  if(omx_txt != NULL)
    				  {
    						fprintf(omx_txt,"RepeaterSource Video  exit  %15lld  %15lld  %15lld  %15lld %15lld %15lld  %15lld  %15lld  %15lld  %15lld this %x\n",
    						in_time ,nowUs,sys_time2,sys_time3,sys_time, mLast_TimeUs,sys_time-in_time,sys_time-nowUs,sys_time-sys_time2,sys_time-sys_time3,this);
    				
    					
    					fflush(omx_txt);
    				  }
    			  }
    			  
            }			
	        }
			
        if (!stale && find_frame ) {
	            break;
	        }
	        if(stale == true)
	        {
	          mStartTimeUs = -1ll;
	          mFrameCount = 0;
	          ALOGV("now dormant");
	        }
		}
		usleep(2000);
    }
	
    return OK;
}

void RepeaterSource::postRead() {
    (new AMessage(kWhatRead, mReflector->id()))->post();
}

void RepeaterSource::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatRead:
        {
		MediaBuffer *buffer;
		int64_t starttime1 =  systemTime(SYSTEM_TIME_MONOTONIC) / 1000;	
		status_t err;
		do
		{
			if(mStarted == false)
			{
				ALOGD("RepeaterSource::onMessageReceived mCondition.broadcast");
				mCondition.broadcast();
				return;
			}
             	err = mSource->read(&buffer);
			}while(buffer == NULL && err == OK);
			
		 		//Mutex::Autolock autoLock(mLock);
       	 		//mMediaBuffersAvailableCondition.wait(mLock);
		 		
        	int64_t starttime2 =  systemTime(SYSTEM_TIME_MONOTONIC) / 1000;	
            Mutex::Autolock autoLock(mLock);
            if (mBuffer != NULL) {
				{
				  int retrtptxt;
				  int64_t sys_time;
				  static int64_t last_time_us = 0;
				  static int64_t last_sys_time = 0;
				  int64_t timeUs ,bufferTimeUs;
				  unsigned long buffer_handle =*((unsigned long*)(buffer->data() + buffer->range_offset() + 8));
				  unsigned long last_buffer_handle =*((unsigned long*)(mBuffer->data() + mBuffer->range_offset() + 8));
				  buffer->meta_data()->findInt64(kKeyTime, &timeUs);
				  mBuffer->meta_data()->findInt64(kKeyTime, &bufferTimeUs);
				  sys_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;		
				  if((retrtptxt = access("data/test/omx_txt_file",0)) == 0)//test_file!=NULL)
				  {
					  
					  if(omx_txt == NULL)
						  omx_txt = fopen("data/test/omx_txt.txt","ab");
					  if(omx_txt != NULL)
					  {
							fprintf(omx_txt,"RepeaterSource Video 11 and release count %d sys_time %lld %lld %lld start  %lld timeUs %lld %lld delta %lld %lld  %lld buffer_handle %x %x buffer %x %x refcount %d %d 	%x %x \n",
							mBuffer->refcount(),starttime1,starttime2,sys_time,mStartTimeUs,timeUs ,bufferTimeUs,
							(timeUs - last_time_us)	,sys_time - mStartTimeUs ,
							sys_time - last_sys_time,buffer_handle,last_buffer_handle,mBuffer,buffer,mBuffer->refcount(),buffer->refcount()
							,*((long*)(mBuffer->data())),*((long*)(mBuffer->data()+4)));
						fflush(omx_txt);
					  }
				  }
				  last_sys_time = sys_time;
				  last_time_us = timeUs;
				}
                mBuffer->release();
                mBuffer = NULL;
            }
            mBuffer = buffer;
            mResult = err;
            mLastBufferUpdateUs = ALooper::GetNowUs();
			
#if ASYNC_RGA
			mCurTimeUs = mLastBufferUpdateUs;
#endif
            mCondition.broadcast();

            if (err == OK) {
                postRead();
            }
            break;
           
        }

        default:
            TRESPASS();
    }
}
void RepeaterSource::signalBufferReturned(MediaBuffer *buffer) {
    Mutex::Autolock autoLock(mLock);
    
    buffer->setObserver(0);
    buffer->release();
    --mNumPendingBuffers;
    mMediaBuffersAvailableCondition.broadcast();
	int	retrtptxt;
		if((retrtptxt = access("data/test/omx_txt_file",0)) == 0)//test_file!=NULL)
	  {
	  	int	sys_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;		
		  
		  if(omx_txt == NULL)
			  omx_txt = fopen("data/test/omx_txt.txt","ab");
		  if(omx_txt != NULL)
		  {
				fprintf(omx_txt,"RepeaterSource signalBufferReturned sys_time %lld mNumPendingBuffers %d %d\n",
				sys_time,mNumPendingBuffers,mNumPendingBuffers);
				
			fflush(omx_txt);
		  }
	  }
}
void RepeaterSource::wakeUp() {
    Mutex::Autolock autoLock(mLock);
    ALOGD("RepeaterSource wakeUp out mCondition.broadcast() mLastBufferUpdateUs %lld mBuffer %x",
		mLastBufferUpdateUs, mBuffer);
    if (mLastBufferUpdateUs < 0ll && mBuffer != NULL) {
        mLastBufferUpdateUs = ALooper::GetNowUs();
		{
    			  int retrtptxt;
    			  int64_t sys_time;
					int64_t timeUs;
					mBuffer->meta_data()->findInt64(kKeyTime, &timeUs);
    			  sys_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;	
    			  if((retrtptxt = access("data/test/omx_txt_file",0)) == 0)//test_file!=NULL)
    			  {
    				  if(omx_txt == NULL)
    					  omx_txt = fopen("data/test/omx_txt.txt","ab");
    				  if(omx_txt != NULL)
    				  {
    						fprintf(omx_txt,"RepeaterSource Video  wakeUp  %15lld  %15lld %15lld delta %15lld %15lld\n",
									mLast_TimeUs,timeUs,mLastBufferUpdateUs,timeUs-mLast_TimeUs,mLastBufferUpdateUs-mLast_TimeUs);
    						fflush(omx_txt);
    				  }
    			  }
    			  assert(mLast_TimeUs<0);
		}
		int64_t updataTimeUs;
		if(mLast_TimeUs > 0)
			mBuffer->meta_data()->setInt64(kKeyTime, mLast_TimeUs+1ll);
        mCondition.broadcast();
    }
}

}  // namespace android
