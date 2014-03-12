/*
 * Copyright (C) 2011 The Android Open Source Project
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


#define LOG_NDEBUG 1
#define LOG_TAG "PreviewRenderer"
#include <utils/Log.h>

#include "PreviewRenderer.h"

#include <media/stagefright/foundation/ADebug.h>
#include <gui/Surface.h>

#include "vpu_global.h"
#include <linux/android_pmem.h>
#include "gralloc_priv.h"
#include <sys/ioctl.h>
#include <fcntl.h>

#include <cutils/properties.h>

#include "rga.h"
#include "version.h"

#define PREVIEW_RENDER_DEBUG 0

#if PREVIEW_RENDER_DEBUG
#define PRE_RENDER_LOG LOGD
#else
#define PRE_RENDER_LOG
#endif
namespace android {

PreviewRenderer* PreviewRenderer::CreatePreviewRenderer (
        const sp<Surface> &surface, size_t width, size_t height) {
    /* 16 bits align */
    width = (width + 15) & (~15);
    height = (height + 15) & (~15);

    PreviewRenderer* renderer = new PreviewRenderer(surface, width, height);

    if (renderer->init() != 0) {
        delete renderer;
        return NULL;
    }

    return renderer;
}

PreviewRenderer::PreviewRenderer(
        const sp<Surface> &surface,
        size_t width, size_t height)
    : mSurface(surface),
      mWidth(width),
      mHeight(height) {
}

int PreviewRenderer::init() {
    int err = 0;
    ANativeWindow* anw = mSurface.get();

    RK_CHIP_TYPE rkChip =NONE;
    rga_fd = open("/dev/rga",O_RDWR,0);
    if (rga_fd >=0) {
        rkChip = RK30;
    } else {
        rkChip = RK29;
    }
    err = native_window_api_connect(anw, NATIVE_WINDOW_API_CPU);
    if (err) goto fail;

    err = native_window_set_usage(
            anw, GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN);
    if (err) goto fail;

    err = native_window_set_buffer_count(anw, 3);
    if (err) goto fail;

    err = native_window_set_scaling_mode(
            anw, NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
    if (err) goto fail;

    if (rkChip == RK30) {
        err = native_window_set_buffers_geometry(
                anw,
                ((mWidth + 31) & (~31)),
                ((mHeight + 15) & (~15)),
                HAL_PIXEL_FORMAT_RGB_565);
    } else {
        err = native_window_set_buffers_geometry(
                anw,
                ((mWidth + 15) & (~15)),
                ((mHeight + 15) & (~15)),
                HAL_PIXEL_FORMAT_RGB_565);
    }
    if (err) goto fail;

    android_native_rect_t crop;
    crop.left = 0;
    crop.top = 0;
    crop.right = mWidth & (~3); //if no 4 aglin crop csy
    crop.bottom = mHeight;

    err = native_window_set_crop(anw, &crop);
    if (err) goto fail;

    err = native_window_set_buffers_transform(anw, 0);
    if (err) goto fail;

    return 0;
fail:
    return err;
}

PreviewRenderer::~PreviewRenderer() {
    native_window_api_disconnect(mSurface.get(), NATIVE_WINDOW_API_CPU);
    if (rga_fd >0) {
        if(rga_fd){
            close(rga_fd);
    		rga_fd = -1;
        }
    }
}


//
// Provides a buffer and associated stride
// This buffer is allocated by the SurfaceFlinger
//
// For optimal display performances, you should :
// 1) call getBufferYV12()
// 2) fill the buffer with your data
// 3) call renderYV12() to take these changes into account
//
// For each call to getBufferYV12(), you must also call renderYV12()
// Expected format in the buffer is YV12 formats (similar to YUV420 planar fromat)
// for more details on this YV12 cf hardware/libhardware/include/hardware/hardware.h
//
void PreviewRenderer::getBufferYV12(uint8_t **data, size_t *stride) {
    int err = OK;

    if ((err = native_window_dequeue_buffer_and_wait(mSurface.get(),
            &mBuf)) != 0) {
        ALOGW("native_window_dequeue_buffer_and_wait returned error %d", err);
        return;
    }

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    PRE_RENDER_LOG("getBufferYV12 1st, w: %d, h: %d, stride: %d",
        mWidth, mHeight, mBuf->stride);
    Rect bounds(mWidth, mHeight);

    void *dst;
    CHECK_EQ(0, mapper.lock(mBuf->handle,
            GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN,
            bounds, &dst));

    *data   = (uint8_t*)dst;
    *stride = ((mWidth + 15) & (~15));   //mBuf->stride;
    PRE_RENDER_LOG("getBufferYV12 2nd, w: %d, h: %d, stride: %d",
        mWidth, mHeight, mBuf->stride);
}


//
// Display the content of the buffer provided by last call to getBufferYV12()
//
// See getBufferYV12() for details.
//
void PreviewRenderer::renderYV12() {
    int err = OK;

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    if (mBuf!= NULL) {
        Rect bounds(mWidth, mHeight);
        void *dst;
        CHECK_EQ(0, mapper.unlock(mBuf->handle));
        CHECK_EQ(0, mapper.lock(mBuf->handle,
        GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN,
            bounds, &dst));

        PRE_RENDER_LOG("renderYV12, w: %d, h: %d",
            mWidth, mHeight);
        if (dst != NULL) {
           PRE_RENDER_LOG("dst addr: 0x%X", dst);
        }
        uint32_t w = ((mWidth+15) & (~15));
        uint32_t h = ((mHeight+15) & (~15));
        uint8_t* pOutPlaneY = (uint8_t*)dst;
        VPUMemLinear_t vpuTmpFrame;
        memset(&vpuTmpFrame, 0, sizeof(VPUMemLinear_t));
        err = VPUMallocLinear(&vpuTmpFrame, (w*h*3) >>1);
        if(err)
        {
            return;
        }
        uint8_t *ptmp = (uint8_t *)vpuTmpFrame.vir_addr;
        memcpy(ptmp, pOutPlaneY, ((w * h * 3) >>1));
        VPUMemClean(&vpuTmpFrame);
        if (rga_fd == -1) {
            ALOGE("no rga dev!");
            return ;
        } else if (rga_fd >0) {
            if (rga_fd >0) {
    			struct rga_req  Rga_Request;
    	        memset(&Rga_Request,0x0,sizeof(Rga_Request));
    	        Rga_Request.src.yrgb_addr =  (int)vpuTmpFrame.phy_addr+ 0x60000000;
    	        Rga_Request.src.uv_addr  = Rga_Request.src.yrgb_addr +
                        ((mWidth + 15)&(~15)) * ((mHeight+ 15)&(~15));
    	        Rga_Request.src.v_addr   =  Rga_Request.src.uv_addr +
                        ((((mWidth + 15)&(~15)) * ((mHeight+ 15)&(~15))) >> 2);
    	        Rga_Request.src.vir_w = (mWidth + 15)&(~15);
    	        Rga_Request.src.vir_h = (mHeight + 15)&(~15);
    	        Rga_Request.src.format = RK_FORMAT_YCbCr_420_P;
    	      	Rga_Request.src.act_w = mWidth;
    	        Rga_Request.src.act_h = mHeight;
    	        Rga_Request.src.x_offset = 0;
    	        Rga_Request.src.y_offset = 0;
    	        Rga_Request.dst.yrgb_addr = (uint32_t)dst;
    	        Rga_Request.dst.uv_addr  = 0;
    	        Rga_Request.dst.v_addr   = 0;
    	       	Rga_Request.dst.vir_w = (mWidth+ 31)&(~31);
    	        Rga_Request.dst.vir_h = (mHeight + 15)&(~15);
    	        Rga_Request.dst.format = RK_FORMAT_RGB_565;
    	        Rga_Request.clip.xmin = 0;
    	        Rga_Request.clip.xmax = ((mWidth+ 31)&(~31)) - 1;
    	        Rga_Request.clip.ymin = 0;
    	        Rga_Request.clip.ymax = ((mHeight + 15)&(~15)) - 1;
    	    	Rga_Request.dst.act_w = mWidth;
    	    	Rga_Request.dst.act_h = mHeight;
    	    	Rga_Request.dst.x_offset = 0;
    	    	Rga_Request.dst.y_offset = 0;
    	    	Rga_Request.rotate_mode = 0;
    	       	Rga_Request.mmu_info.mmu_en    = 1;
    	       	Rga_Request.mmu_info.mmu_flag  = ((2 & 0x3) << 4) | 1;
    	    	if(ioctl(rga_fd, RGA_BLIT_SYNC, &Rga_Request) != 0)
    	    	{
    	            ALOGE("rga RGA_BLIT_SYNC fail");
    	    	}
    		}
        }
        if (vpuTmpFrame.phy_addr) {
            VPUFreeLinear(&vpuTmpFrame);
        }
        CHECK_EQ(0, mapper.unlock(mBuf->handle));
        if ((err = mSurface->ANativeWindow::queueBuffer(mSurface.get(), mBuf, -1)) != 0) {
            ALOGW("Surface::queueBuffer returned error %d", err);
        }
    }
    mBuf = NULL;
}

}  // namespace android
