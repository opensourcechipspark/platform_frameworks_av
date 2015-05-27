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

#define LOG_TAG "SoftwareRenderer"
#include <utils/Log.h>

#include "../include/SoftwareRenderer.h"

#include <cutils/properties.h> // for property_get
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MetaData.h>
#include <system/window.h>
#include <ui/GraphicBufferMapper.h>
#include <gui/IGraphicBufferProducer.h>
#include <linux/android_pmem.h>
#include <sys/ioctl.h>
#include "rga.h"
#include <fcntl.h>
#include "version.h"
#include "gralloc_priv.h"

struct MemToMemInfo
{
    int SrcAddr;
    int DstAddr;
    int MenSize;
};

typedef enum SOFT_RENDER_FRAME_MAP {
    FRAME_CAN_FREE           = 0x01,
    IS_RK_HEVC_HW            = 0x02,
    RGA_NOT_SUPPORT_DST_VIR_WIDTH   = 0x04,
};

typedef enum ANB_PRIVATE_BUF_TYPE {
    ANB_PRIVATE_BUF_NONE    = 0,
    ANB_PRIVATE_BUF_VIRTUAL = 0x01,
    ANB_PRIVATE_BUF_BUTT,
};


#define WRITE_DATA_DEBUG 0

#if WRITE_DATA_DEBUG
static FILE* pOutFile = NULL;
#endif

#define HW_YUV2RGB
namespace android {

static bool runningInEmulator() {
    char prop[PROPERTY_VALUE_MAX];
    return (property_get("ro.kernel.qemu", prop, NULL) > 0);
}

SoftwareRenderer::SoftwareRenderer(
        const sp<ANativeWindow> &nativeWindow, const sp<MetaData> &meta)
    : mConverter(NULL),
      mYUVMode(None),
      mNativeWindow(nativeWindow),
      mHintTransform(0),
      init_Flag(true),
      rga_fd(-1),
      power_fd(-1),
      mHttpFlag(0),
      mFlags (0){
#if WRITE_DATA_DEBUG
    pOutFile = fopen("/sdcard/Movies/rgb.dat", "wb");
    if (pOutFile) {
        ALOGE("pOutFile open ok!");
    } else {
        ALOGE("pOutFile open fail!");
    }
#endif
    mStructId.clear();
    int32_t tmp;
    mSwdecFlag = 0;
    CHECK(meta->findInt32(kKeyColorFormat, &tmp));
    mColorFormat = (OMX_COLOR_FORMATTYPE)tmp;
    if (meta->findInt32(kKeyRkHevc, &tmp)) {
        mFlags |=IS_RK_HEVC_HW;
    }

    CHECK(meta->findInt32(kKeyWidth, &mWidth));
    CHECK(meta->findInt32(kKeyHeight, &mHeight));
    ALOGI("SoftwareRenderer construct, width: %d, heigth: %d, mNativeWindow: %p",
        mWidth, mHeight, mNativeWindow.get());

    if (!meta->findRect(
                kKeyCropRect,
                &mCropLeft, &mCropTop, &mCropRight, &mCropBottom)) {
        mCropLeft = mCropTop = 0;
        mCropRight = mWidth - 1;
        mCropBottom = mHeight - 1;
    }

    mCropWidth = mCropRight - mCropLeft + 1;
    mCropHeight = mCropBottom - mCropTop + 1;

    int32_t rotationDegrees;
    if (!meta->findInt32(kKeyRotation, &rotationDegrees)) {
        rotationDegrees = 0;
    }

    char prop_value[PROPERTY_VALUE_MAX];
    sf_info *info = sf_info::getInstance();
    RK_CHIP_TYPE mBoardType = (info->get_chip_type());
    rga_fd  = open("/dev/rga",O_RDWR,0);
    if (rga_fd > 0) {
        mBoardType = RK30;
    }
    int32_t deInterlaceFlag = 0;
    meta->findInt32(kKeyIsHttp, &mHttpFlag);
	meta->findInt32(kKeyIsDeInterlace, &deInterlaceFlag);
    if(!mHttpFlag){
		mHttpFlag = VPUMemJudgeIommu();
    }
    if(property_get("sf.power.control", prop_value, NULL)&& atoi(prop_value) > 0){
        if(!(mHttpFlag || deInterlaceFlag)){
            power_fd = open("/dev/video_state", O_WRONLY);
            if(power_fd == -1){
                ALOGE("power control open fd fail");
            }
            ALOGV("power control open fd suceess and write to 1");
            char para[200]={0};
			int paraLen = 0;
			int ishevc =  mFlags&IS_RK_HEVC_HW;
			paraLen = sprintf(para, "1,width=%d,height=%d,ishevc=%d,videoFramerate=%d,streamBitrate=%d",mWidth,mHeight,ishevc,0,0);
            write(power_fd, para, paraLen);
        }
    }
    if (RK30 == mBoardType) {
        if (property_get("sys.hwc.compose_policy", prop_value, NULL) && atoi(prop_value) > 0){
            meta->findInt32(kKeyIsHttp, &mHttpFlag);
        }else{
            ALOGI("hwc no open");
            mHttpFlag = 1;
        }
        if (rga_fd  < 0) {
            ALOGE("open /dev/rk30-rga fail!");
        }
    }else{
        init_Flag = false;
        ALOGE("oh! board info no found, may be no rkxx or rkxx set error");
    }
    int halFormat;
    size_t bufWidth, bufHeight;

    switch (mColorFormat) {
        case OMX_COLOR_FormatYUV420Planar:
        case OMX_TI_COLOR_FormatYUV420PackedSemiPlanar:
        {
            if (!runningInEmulator()) {
                halFormat = HAL_PIXEL_FORMAT_YV12;
                if ((mWidth % 32) || 1) {
                    bufWidth = ((mCropWidth + 31) & (~31));
                    bufHeight = ((mCropHeight + 15)&(~15));
                } else {
                bufWidth = ((mCropWidth + 1) & (~1));
                bufHeight = ((mCropHeight + 1)&(~1));
                }
                mSwdecFlag = 1;
                break;
            }

            // fall through.
        }

        default:
			if(mHttpFlag){
            	halFormat = HAL_PIXEL_FORMAT_YCrCb_NV12;
			}else{
			    halFormat = HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO;
			}
            bufWidth = mCropWidth;
            bufHeight = mCropHeight;

            mConverter = new ColorConverter(
                    mColorFormat, OMX_COLOR_Format16bitRGB565);
            CHECK(mConverter->isValid());
            break;
    }

    CHECK(mNativeWindow != NULL);
    CHECK(mCropWidth > 0);
    CHECK(mCropHeight > 0);
    CHECK(mConverter == NULL || mConverter->isValid());

    CHECK_EQ(0,
            native_window_set_usage(
            mNativeWindow.get(),
            GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN
            | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP));

    CHECK_EQ(0,
            native_window_set_scaling_mode(
            mNativeWindow.get(),
            NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW));

#if 0   /* let surfaceFlinger decide to alloc buffer count */
    /*
     ** we need to request more than 3 counts(current min buffer count in gui)
     ** from native window in network case, other wise request will be fail.
    */
    if (mHttpFlag) {
        CHECK_EQ(0, native_window_set_buffer_count(mNativeWindow.get(), 4));
    } else {
        CHECK_EQ(0, native_window_set_buffer_count(mNativeWindow.get(), 2));
    }
#endif

    android_native_rect_t crop;
	crop.left = 0;
	crop.top = 0;
    crop.right = mWidth - 64; //if no 4 aglin crop csy
	crop.bottom = mHeight;

    if(halFormat != HAL_PIXEL_FORMAT_YV12){
	    crop.right = mWidth & (~3); //if no 4 aglin crop csy
		crop.bottom = mHeight;

        int32_t tmpWidth = 0;
        if (mFlags & IS_RK_HEVC_HW) {
            tmpWidth = (((bufWidth  + 255)&(~255)) | 256);
        } else {
            tmpWidth = ((bufWidth  + 15)&(~15));
        }
        if (tmpWidth >4096) {
            mFlags |=RGA_NOT_SUPPORT_DST_VIR_WIDTH;
        }

	    if(RK30 == mBoardType && mHttpFlag){
            if (mFlags & IS_RK_HEVC_HW) {
                if (!(mFlags & RGA_NOT_SUPPORT_DST_VIR_WIDTH)) {
                     bufWidth = (((bufWidth  + 255)&(~255)) | 256);
                }
	            bufHeight = (bufHeight + 7)&(~7);
            } else {
                if (!(mFlags & RGA_NOT_SUPPORT_DST_VIR_WIDTH)) {
                    bufWidth = (bufWidth  + 31)&(~31);
                }
	            bufHeight = (bufHeight + 15)&(~15);
            }
	    }else{
	        if (mFlags & IS_RK_HEVC_HW) {
                if (!(mFlags & RGA_NOT_SUPPORT_DST_VIR_WIDTH)) {
                    bufWidth = (((bufWidth  + 255)&(~255)) | 256);
                }
	            bufHeight = (bufHeight + 7)&(~7);
            } else {
                if (!(mFlags & RGA_NOT_SUPPORT_DST_VIR_WIDTH)) {
                    bufWidth = (bufWidth  + 15)&(~15);
                }
	            bufHeight = (bufHeight + 15)&(~15);
            }
		}
    }

	int32_t isMbaff =0;
    if (meta->findInt32(kKeyIsMbaff, &isMbaff) && isMbaff) {
        crop.bottom = (crop.bottom >16) ? crop.bottom - 16 : crop.bottom;
    }
	
    CHECK_EQ(0, native_window_set_buffers_geometry(
                mNativeWindow.get(),
                bufWidth,
                bufHeight,
                halFormat));
    CHECK_EQ(0, native_window_set_crop(
                mNativeWindow.get(), &crop));

    uint32_t transform;
    switch (rotationDegrees) {
        case 0: transform = 0; break;
        case 90: transform = HAL_TRANSFORM_ROT_90; break;
        case 180: transform = HAL_TRANSFORM_ROT_180; break;
        case 270: transform = HAL_TRANSFORM_ROT_270; break;
        default: transform = 0; break;
    }

    if (transform) {
        CHECK_EQ(0, native_window_set_buffers_transform(
                    mNativeWindow.get(), transform));
    }
}

static int32_t hwcYuv2Yuv(
            private_handle_t* anb_handle,
            VPU_FRAME *frame ,
            int32_t mWidth,
            int32_t mHeight,
            int32_t rga_fd,
            int32_t flags,
            int32_t transform,
            void* mapper_dst) {
    if ((anb_handle ==NULL) || (rga_fd <0)) {
        return -1;
    }

    struct rga_req  Rga_Request;
    memset(&Rga_Request,0x0,sizeof(Rga_Request));
    
    if (!VPUMemJudgeIommu()) {
        Rga_Request.src.uv_addr  = frame->FrameBusAddr[0];
    }else{
        Rga_Request.src.yrgb_addr = frame->FrameBusAddr[0];
        Rga_Request.src.uv_addr  = (int32_t)frame->vpumem.vir_addr;
    }

    if (flags & IS_RK_HEVC_HW) {
        Rga_Request.src.vir_w = (((mWidth + 255)&(~255)) | 256);
        Rga_Request.src.vir_h = (mHeight + 7)&(~7);
    } else {
        Rga_Request.src.vir_w = (mWidth + 15)&(~15);
        Rga_Request.src.vir_h = (mHeight + 15)&(~15);
    }
    Rga_Request.src.format = RK_FORMAT_YCbCr_420_SP;
  	Rga_Request.src.act_w = mWidth;
    Rga_Request.src.act_h = mHeight;
    Rga_Request.src.x_offset = 0;
    Rga_Request.src.y_offset = 0;
    Rga_Request.dst.yrgb_addr = anb_handle->share_fd;
    Rga_Request.dst.uv_addr  = 0;
    Rga_Request.dst.v_addr   = 0;
    if (flags & IS_RK_HEVC_HW) {
        if (!(flags & RGA_NOT_SUPPORT_DST_VIR_WIDTH)) {
            Rga_Request.dst.vir_w = (((mWidth + 255)&(~255)) | 256);
        } else {
            Rga_Request.dst.vir_w = mWidth;
        }
        Rga_Request.dst.vir_h = (mHeight + 7)&(~7);
    } else {
        if (!(flags & RGA_NOT_SUPPORT_DST_VIR_WIDTH)) {
            Rga_Request.dst.vir_w = (mWidth + 31)&(~31);
        } else {
            Rga_Request.dst.vir_w = mWidth;
        }
        Rga_Request.dst.vir_h = (mHeight + 15)&(~15);
    }
    Rga_Request.dst.format = RK_FORMAT_YCbCr_420_SP;
    Rga_Request.clip.xmin = 0;
    Rga_Request.clip.ymin = 0;
    if (flags & IS_RK_HEVC_HW) {
        if (!(flags & RGA_NOT_SUPPORT_DST_VIR_WIDTH)) {
            Rga_Request.clip.xmax = (((mWidth + 255)&(~255)) | 256) -1;
        } else {
            Rga_Request.clip.xmax = mWidth;
        }
        Rga_Request.clip.ymax = (mHeight + 7)&(~7) -1;
    } else {
        if (!(flags & RGA_NOT_SUPPORT_DST_VIR_WIDTH)) {
            Rga_Request.clip.xmax = (mWidth + 31)&(~31) - 1;
        } else {
            Rga_Request.clip.xmax = mWidth;
        }
        Rga_Request.clip.ymax = (mHeight + 15)&(~15) - 1;
    }
	Rga_Request.dst.act_w = mWidth;
	Rga_Request.dst.act_h = mHeight;
	Rga_Request.dst.x_offset = 0;
	Rga_Request.dst.y_offset = 0;
	Rga_Request.rotate_mode = 0;
   	Rga_Request.mmu_info.mmu_en    = 0;
    Rga_Request.render_mode = pre_scaling_mode;
#if 0
    if(transform == HAL_TRANSFORM_ROT_270){
        Rga_Request.dst.act_w = Rga_Request.dst.vir_h;
        Rga_Request.dst.act_h = Rga_Request.dst.vir_w;
        Rga_Request.dst.x_offset = 0;
        Rga_Request.dst.y_offset = Rga_Request.dst.vir_h-1;
        Rga_Request.sina = -65536;
        Rga_Request.cosa = 0;
        Rga_Request.rotate_mode = 1;
    }
#endif
    if (anb_handle->type == ANB_PRIVATE_BUF_VIRTUAL) {
        Rga_Request.dst.uv_addr = 0;// (unsigned int)mapper_dst;
        Rga_Request.mmu_info.mmu_en = 1;
        Rga_Request.mmu_info.mmu_flag = ((2 & 0x3) << 4) | 1;
        if (VPUMemJudgeIommu()) {
            Rga_Request.mmu_info.mmu_flag |=((1<<31) | (1<<10) | (1<<8));
        } else {
            Rga_Request.mmu_info.mmu_flag |=((1<<31) | (1<<10));
        }
    }

	if(ioctl(rga_fd, RGA_BLIT_SYNC, &Rga_Request) != 0)
	{
        ALOGE("rga RGA_BLIT_SYNC fail");
	}
    return 0;
}


static int32_t hwcRender(
            private_handle_t* anb_handle,
            int32_t Width,
            int32_t Height,
            int32_t rga_fd,
            void* mapper_dst) {
    if ((anb_handle ==NULL) || (rga_fd <0)) {
        return -1;
    }

    struct rga_req  Rga_Request;
    memset(&Rga_Request,0x0,sizeof(Rga_Request));

    Rga_Request.src.vir_w = (Width + 15)&(~15);
    Rga_Request.src.vir_h = (Height + 15)&(~15);
    Rga_Request.src.format = RK_FORMAT_YCbCr_420_SP;
  	Rga_Request.src.act_w = Width;
    Rga_Request.src.act_h = Height;
    Rga_Request.src.x_offset = 0;
    Rga_Request.src.y_offset = 0;
    Rga_Request.dst.yrgb_addr = anb_handle->share_fd;
    Rga_Request.dst.uv_addr  = 0;
    Rga_Request.dst.v_addr   = 0;
	Rga_Request.dst.vir_w = (Width + 31)&(~31);
	Rga_Request.dst.vir_h = Height;
    Rga_Request.dst.format = RK_FORMAT_RGB_565;
    Rga_Request.clip.xmin = 0;

	Rga_Request.clip.xmax = (Width + 31)&(~31) -1;

    Rga_Request.clip.ymin = 0;
    Rga_Request.clip.ymax = Height - 1;
	Rga_Request.dst.act_w = Width;
	Rga_Request.dst.act_h = Height;
	Rga_Request.dst.x_offset = 0;
	Rga_Request.dst.y_offset = 0;
	Rga_Request.rotate_mode = 0;
    Rga_Request.render_mode = color_fill_mode;
    if (anb_handle->type == ANB_PRIVATE_BUF_VIRTUAL) {
        Rga_Request.dst.uv_addr  =  (unsigned int)mapper_dst;
        Rga_Request.mmu_info.mmu_en = 1;
   	    Rga_Request.mmu_info.mmu_flag  = ((2 & 0x3) << 4) | 1;
        Rga_Request.mmu_info.mmu_flag |= (1<<31) | (1<<10);
    }
	if(ioctl(rga_fd, RGA_BLIT_SYNC, &Rga_Request) != 0)
	{
        ALOGE("rga RGA_BLIT_SYNC fail");
        return -1;
	}
    return 0;
}

SoftwareRenderer::~SoftwareRenderer() {
#if WRITE_DATA_DEBUG
    if (pOutFile) {
        fclose(pOutFile);
        pOutFile = NULL;
    }
#endif
    int err;
    bool mStatus = true;
    int32_t Width = 1280;
    int32_t Height = 720;
    if(!mHttpFlag && init_Flag){
        if (rga_fd >0){
            if(native_window_set_buffers_geometry(mNativeWindow.get(),(Width + 31)&(~31),(Height + 15)&(~15),
                        HAL_PIXEL_FORMAT_RGB_565) != 0){
                ALOGE("native_window_set_buffers_geometry fail");
                goto Fail;
            }
            android_native_rect_t crop;
            crop.left = 0;
            crop.top = 0;
            crop.right = Width;
            crop.bottom = Height;
            CHECK_EQ(0, native_window_set_crop(mNativeWindow.get(), &crop));
        }else{
            if(native_window_set_buffers_geometry(mNativeWindow.get(),
                    (Width + 15)&(~15),(Height + 15)&(~15),HAL_PIXEL_FORMAT_YCrCb_NV12) != 0){
                ALOGE("native_window_set_buffers_geometry fail");
                goto Fail;
            }
            android_native_rect_t crop;
            crop.left = 0;
            crop.top = 0;
            crop.right = Width;
            crop.bottom = Height;
        }
        ANativeWindowBuffer *buf;
        if ((err = native_window_dequeue_buffer_and_wait(mNativeWindow.get(),
            &buf)) != 0) {
            goto Fail;
        }
        GraphicBufferMapper &mapper = GraphicBufferMapper::get();
        Rect bounds((Width + 15)&(~15), (Height + 15)&(~15));
        void *dst;
        if(mapper.lock(
                    buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst) != 0){
            goto Fail;
        }

        if (rga_fd >0) {
            ALOGV("SoftwareRenderer deconstruct, use rag render one RGB565 frame");
            private_handle_t *handle = (private_handle_t*)buf->handle;
            if(hwcRender(handle, Width, Height, rga_fd, dst) <0){
                mStatus = false;
            }
        }else{
            int32_t size = Width*Height;
            ALOGI("Width = %d,Height = %d",Width,Height);
        	memset(dst,0x0,size);
            memset(dst+size,0x80,size/2);
        }
        ALOGV("SoftwareRenderer deconstruct queue buffer Id: 0x%X", buf);
        if(mapper.unlock(buf->handle) != 0){
            goto Fail;
        }
        if(mStatus){
            if ((err = mNativeWindow->queueBuffer(mNativeWindow.get(), buf,
                    -1)) != 0) {
                ALOGW("Surface::queueBuffer returned error %d", err);
            }
        }else{
            ALOGE("SoftwareRenderer last frame process error");
            mNativeWindow->cancelBuffer(mNativeWindow.get(), buf, -1);
        }
    }
Fail:
    while(!mStructId.isEmpty())
    {
        VPU_FRAME *frame =  mStructId.editItemAt(0);
        if(frame->vpumem.phy_addr)
        {
            VPUMemLink(&frame->vpumem);
            VPUFreeLinear(&frame->vpumem);
        }
        free(frame);
        mStructId.removeAt(0);
    }
    delete mConverter;
    mConverter = NULL;
    char prop_value[PROPERTY_VALUE_MAX];
    if(property_get("sf.power.control", prop_value, NULL) && atoi(prop_value) > 0){
        char para[200]={0};
        int paraLen = 0;
        int ishevc =  mFlags&IS_RK_HEVC_HW;
		paraLen = sprintf(para, "0,width=%d,height=%d,ishevc=%d,videoFramerate=%d,streamBitrate=%d",mWidth,mHeight,ishevc,0,0);
        if(power_fd > 0){
            ALOGV("power control close fd and write to 0");
            write(power_fd, para, paraLen);
            close(power_fd);
        }
    }
    if(rga_fd > 0) {
        close(rga_fd);
        rga_fd = -1;
    }
}

static int ALIGN(int x, int y) {
    // y must be a power of 2.
    return (x + y - 1) & ~(y - 1);
}

void SoftwareRenderer::render(
        const void *data, size_t size, void *platformPrivate) {
    ANativeWindowBuffer *buf;
    int err;
    if(!init_Flag){
        ALOGE("Board config no found no render");
        return;
    }
    if(mHttpFlag){
        int32_t tmptransform = 0;
        mNativeWindow->query(mNativeWindow.get(), NATIVE_WINDOW_TRANSFORM_HINT, &tmptransform);
        if(tmptransform != mHintTransform){
            mHintTransform = tmptransform;
            switch(tmptransform){
                case HAL_TRANSFORM_ROT_270:
                    CHECK_EQ(0, native_window_set_buffers_transform(mNativeWindow.get(), HAL_TRANSFORM_ROT_90));
                    break;
                default:
                    CHECK_EQ(0, native_window_set_buffers_transform(mNativeWindow.get(), 0));
                    break;
            }
        }
    }
    if ((err = native_window_dequeue_buffer_and_wait(mNativeWindow.get(),
            &buf)) != 0) {
        ALOGE("Surface::dequeueBuffer returned error %d", err);
        VPU_FRAME *frame = (VPU_FRAME *)data;
        if(frame->vpumem.phy_addr)
        {
            VPUMemLink(&frame->vpumem);
            VPUFreeLinear(&frame->vpumem);
        }
        return;
    }

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    int32_t align = (mSwdecFlag == true) ? (2-1) : (16-1);
    Rect bounds((mWidth + align)&(~align), (mHeight + align)&(~align));

    void *dst;
    CHECK_EQ(0, mapper.lock(
                buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst));


    private_handle_t *handle = (private_handle_t*)buf->handle;

    if(!mStructId.isEmpty()&& !mHttpFlag && !mSwdecFlag)
    {
         ALOGV("mStructId size in = %d",mStructId.size());
         for(int32_t i = mStructId.size() -1; i>= 0; i--)
         {
             VPU_FRAME *tem  = mStructId.editItemAt(i);
             if (tem->Res[1] & FRAME_CAN_FREE) {
                if(tem->vpumem.phy_addr)
                {
                    ALOGV("release releaseId = 0x%x",tem->vpumem.phy_addr);
                    VPUMemLink(&tem->vpumem);
                    VPUFreeLinear(&tem->vpumem);
                }
                mStructId.removeAt(i);
                free(tem);
             } else {
                 if(tem->Res[0]== (uint32_t)buf) {
                    tem->Res[1] |=FRAME_CAN_FREE;
                 }
             }
         }
         ALOGV("mStructId size out = %d",mStructId.size());
    }
#ifndef HW_YUV2RGB
    if (mConverter) {
        mConverter->convert(
                data,
                mWidth, mHeight,
                mCropLeft, mCropTop, mCropRight, mCropBottom,
                dst,
                buf->stride, buf->height,
                0, 0, mCropWidth - 1, mCropHeight - 1);
    } else if (mColorFormat == OMX_COLOR_FormatYUV420Planar) {
        const uint8_t *src_y = (const uint8_t *)data;
        const uint8_t *src_u = (const uint8_t *)data + mWidth * mHeight;
        const uint8_t *src_v = src_u + (mWidth / 2 * mHeight / 2);

        uint8_t *dst_y = (uint8_t *)dst;
        size_t dst_y_size = buf->stride * buf->height;
        size_t dst_c_stride = ALIGN(buf->stride / 2, 16);
        size_t dst_c_size = dst_c_stride * buf->height / 2;
        uint8_t *dst_v = dst_y + dst_y_size;
        uint8_t *dst_u = dst_v + dst_c_size;

        for (int y = 0; y < mCropHeight; ++y) {
            memcpy(dst_y, src_y, mCropWidth);

            src_y += mWidth;
            dst_y += buf->stride;
        }

        for (int y = 0; y < (mCropHeight + 1) / 2; ++y) {
            memcpy(dst_u, src_u, (mCropWidth + 1) / 2);
            memcpy(dst_v, src_v, (mCropWidth + 1) / 2);

            src_u += mWidth / 2;
            src_v += mWidth / 2;
            dst_u += dst_c_stride;
            dst_v += dst_c_stride;
        }
    } else {
        CHECK_EQ(mColorFormat, OMX_TI_COLOR_FormatYUV420PackedSemiPlanar);

        const uint8_t *src_y =
            (const uint8_t *)data;

        const uint8_t *src_uv =
            (const uint8_t *)data + mWidth * (mHeight - mCropTop / 2);

        uint8_t *dst_y = (uint8_t *)dst;

        size_t dst_y_size = buf->stride * buf->height;
        size_t dst_c_stride = ALIGN(buf->stride / 2, 16);
        size_t dst_c_size = dst_c_stride * buf->height / 2;
        uint8_t *dst_v = dst_y + dst_y_size;
        uint8_t *dst_u = dst_v + dst_c_size;

        for (int y = 0; y < mCropHeight; ++y) {
            memcpy(dst_y, src_y, mCropWidth);

            src_y += mWidth;
            dst_y += buf->stride;
        }

        for (int y = 0; y < (mCropHeight + 1) / 2; ++y) {
            size_t tmp = (mCropWidth + 1) / 2;
            for (size_t x = 0; x < tmp; ++x) {
                dst_u[x] = src_uv[2 * x];
                dst_v[x] = src_uv[2 * x + 1];
            }

            src_uv += mWidth;
            dst_u += dst_c_stride;
            dst_v += dst_c_stride;
        }
    }
#else
    VPU_FRAME *frame = (VPU_FRAME *)data;
    if(!mHttpFlag){
		if(mSwdecFlag){
            if ((mWidth % 32) || 1) {
                int32_t k =0;
                int32_t n = mWidth * mHeight;
                int32_t w1 = ((mWidth + 31) & (~31));
                int32_t h1 = ((mHeight + 15) & (~15));
                int32_t n_align = w1 * h1;

                uint8_t* pYSrc = (uint8_t*)data;
                uint8_t* pVSrc = (uint8_t*)data + n;
                uint8_t* pUSrc = (uint8_t*)data + n*5/4;

                uint8_t* pYDst = (uint8_t*)dst;
                uint8_t* pUDst = (uint8_t*)dst + n_align;
                uint8_t* pVDst = (uint8_t*)dst + n_align*5/4;

                for (k =0; k <mHeight; k++) {
                    memcpy(pYDst, pYSrc, mWidth);
                    pYSrc +=mWidth;
                    pYDst +=w1;
                }

                for (k =0; k <(mHeight >>1); k++) {
                    memcpy(pUDst, pUSrc, (mWidth >>1));
                    pUSrc +=(mWidth >>1);
                    pUDst +=(w1 >>1);

                    memcpy(pVDst, pVSrc, (mWidth >>1));
                    pVSrc +=(mWidth >>1);
                    pVDst +=(w1 >>1);
                }
            } else {
                memcpy(dst, data, mWidth * mHeight *3/2);
            }
        }else{
            if (!(mFlags & IS_RK_HEVC_HW)) {
    	        frame->FrameWidth = (mWidth + 15)&(~15);
    	        frame->FrameHeight = (mHeight + 15)&(~15);
                frame->OutputWidth = 0x21;//default NV12_video
            }
	        VPU_FRAME *info = (VPU_FRAME *)malloc(sizeof(VPU_FRAME));
	    	ALOGV("push buffer Id = 0x%x",buf);
	        memcpy(info,data,sizeof(VPU_FRAME));
	    	VPUMemLink(&frame->vpumem);
	        VPUMemDuplicate(&info->vpumem,&frame->vpumem);
	    	VPUFreeLinear(&frame->vpumem);
#if GET_VPU_INTO_FROM_HEAD
	        memcpy(dst,info,sizeof(VPU_FRAME));
#else
	        memcpy(dst+2*handle->stride*handle->height,info,sizeof(VPU_FRAME));
#endif
	        info->Res[0] = (uint32_t)buf;
            info->Res[1] = 0;
	        mStructId.push(info);
		}
    }else{
        if(mSwdecFlag){
        	if ((mWidth % 32) || 1) {
                int32_t k =0;
                int32_t n = mWidth * mHeight;
                int32_t w1 = ((mWidth + 31) & (~31));
                int32_t h1 = ((mHeight + 15) & (~15));
                int32_t n_align = w1 * h1;
                uint8_t* pYSrc = (uint8_t*)data;
                uint8_t* pVSrc = (uint8_t*)data + n;
                uint8_t* pUSrc = (uint8_t*)data + n*5/4;
                uint8_t* pYDst = (uint8_t*)dst;
                uint8_t* pUDst = (uint8_t*)dst + n_align;
                uint8_t* pVDst = (uint8_t*)dst + n_align*5/4;
                for (k =0; k <mHeight; k++) {
                    memcpy(pYDst, pYSrc, mWidth);
                    pYSrc +=mWidth;
                    pYDst +=w1;
                }
                for (k =0; k <(mHeight >>1); k++) {
                    memcpy(pUDst, pUSrc, (mWidth >>1));
                    pUSrc +=(mWidth >>1);
                    pUDst +=(w1 >>1);
                    memcpy(pVDst, pVSrc, (mWidth >>1));
                    pVSrc +=(mWidth >>1);
                    pVDst +=(w1 >>1);
                }
            } else {
        	memcpy(dst,data,mWidth*mHeight*3/2);
            }
        } else if (rga_fd >0) {
            hwcYuv2Yuv(handle,frame,mWidth,mHeight,rga_fd, mFlags, mHintTransform, dst);
        }
    }
#if WRITE_DATA_DEBUG
    fwrite(dst,1, mWidth*mHeight*4, pOutFile);
#endif
#endif

    CHECK_EQ(0, mapper.unlock(buf->handle));

    if ((err = mNativeWindow->queueBuffer(mNativeWindow.get(), buf,
            -1)) != 0) {
        ALOGW("Surface::queueBuffer returned error %d", err);
    }
    buf = NULL;
}

}  // namespace android
