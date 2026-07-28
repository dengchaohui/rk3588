/*
 * Copyright (c) 2018, Fuzhou Rockchip Electronics Co., Ltd
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

#define LOG_TAG "CameraHal"
#define LOG_NDEBUG 0

#include <sys/stat.h>
#include <unistd.h>
#include <log/log.h>
#include <ui/GraphicBuffer.h>
#include "ExternalCameraMemManager.h"
#include "BufferAllocatorWrapper.h"

extern "C" {
#define virtual vir
#include <drm.h>
#include <drm_mode.h>
#undef virtual
}

#include <xf86drm.h>
#include <xf86drmMode.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define FMT_NUM_PLANES 1

#define BUFFER_COUNT 4

namespace android {

BufferAllocator* cameraHal_bufferAllocator = NULL;

MemManagerBase::MemManagerBase()
{
    mPreviewBufferInfo = NULL;
}
MemManagerBase::~MemManagerBase()
{
    mPreviewBufferInfo = NULL;
}

static void *sur_alloc_drm_buf(int *fd,int in_w, int in_h, int in_bpp, unsigned int *drmHandle)
{
    int ret;
    void *map = NULL;

    void *vir_addr = NULL;
    struct drm_prime_handle fd_args;
    struct drm_mode_map_dumb mmap_arg;
    struct drm_mode_destroy_dumb destory_arg;

    struct drm_mode_create_dumb alloc_arg;


    int drm_fd = drmOpen("rockchip", NULL);
    if (drm_fd < 0) {
        printf("failed to open rockchip drm: %s\n", strerror(errno));
		LOGD("failed to open rockchip drm: %s\n", strerror(errno));
        return NULL;
    }

    memset(&alloc_arg, 0, sizeof(alloc_arg));
    alloc_arg.bpp = in_bpp;
    alloc_arg.width = in_w;
    alloc_arg.height = in_h;
    //alloc_arg.flags = (1<<1);

    ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &alloc_arg);
    if (ret) {
        LOGE("failed to create dumb buffer: %s\n", strerror(errno));
        return NULL;
    }

    memset(&fd_args, 0, sizeof(fd_args));
	fd_args.fd = -1;
	fd_args.handle = alloc_arg.handle;;
	fd_args.flags = 0;
	ret = drmIoctl(drm_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &fd_args);

	if (ret)
	{
	    LOGE("%s: handle_to_fd failed ret=%d,err=%s, handle=%x \n", __FUNCTION__, ret, strerror(errno),fd_args.handle);
		return NULL;
    }
	
    LOGD("%s: Dump fd = %d \n",__FUNCTION__, fd_args.fd);
    *fd = fd_args.fd;

  //handle to Virtual address
    memset(&mmap_arg, 0, sizeof(mmap_arg));
    mmap_arg.handle = alloc_arg.handle;
	
	*drmHandle = alloc_arg.handle;

    ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &mmap_arg);
    if (ret) {
        LOGE("%s: failed to create map dumb: %s\n",__FUNCTION__, strerror(errno));
        vir_addr = NULL;
        goto destory_dumb;
    }

	vir_addr = map = mmap(0, alloc_arg.size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, mmap_arg.offset);
    if (map == MAP_FAILED) {
        LOGE("%s: failed to mmap buffer: %s\n", __FUNCTION__, strerror(errno));
        vir_addr = NULL;
        goto destory_dumb;
    }
	
    LOGD("%s:alloc map=%x \n",__FUNCTION__, map);
    drmClose(drm_fd);
    return vir_addr;
destory_dumb:
  memset(&destory_arg, 0, sizeof(destory_arg));
  destory_arg.handle = alloc_arg.handle;
  ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destory_arg);
  if (ret)
    LOGE("failed to destory dumb %d\n", ret);
  if(drm_fd)
      drmClose(drm_fd);
  return vir_addr;
}



static void release_drm_obj(void *addr, int length, int fd, unsigned int drm_handle)
{
	if (-1 == munmap(addr, length))
		return;
	else
	{
		LOGD("munmap release success addr [%p]\n", addr);
	}

	int drm_fd = drmOpen("rockchip", NULL);
    if (drm_fd < 0) {
        LOGE("failed to open rockchip drm: %s\n", strerror(errno));
        return ;
    }

	struct drm_mode_destroy_dumb destory_arg;
	close(fd);
	memset(&destory_arg, 0, sizeof(destory_arg));
	destory_arg.handle = drm_handle;
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destory_arg);
    drmClose(drm_fd);            
}


static int xioctl(int fh, int request, void *arg)
{
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

static BufferAllocator *render_dma_heap_init()
{
    int ret = -1;
    BufferAllocator* bufferAllocator = CreateDmabufHeapBufferAllocator();
    if (!bufferAllocator) {
        LOGE("unable to get allocator\n");
        return NULL;
    }

	ret = MapDmabufHeapNameToIonHeap(bufferAllocator, kDmabufSystemHeapName,
                                     "" /* no mapping for non-legacy */,
                                     0 /* no mapping for non-legacy ion */,
                                     ~0 /* legacy ion heap mask */, 0 /* legacy ion heap flag */);
    if (ret < 0) {
        LOGE("MapDmabufHeapNameToIonHeap failed: %d\n", ret);
        return NULL;
    }

	return bufferAllocator;
}

static void *render_dma_heap_buf(int *export_fd, int width, int height, int bpp)
{
    int length = width * height * bpp / 8;
	int fd = -1, ret = 0;
    size_t i = 0;
	void *ptr =NULL;

	if(cameraHal_bufferAllocator == NULL )
	{
	    cameraHal_bufferAllocator = render_dma_heap_init();
	}

    fd = DmabufHeapAlloc(cameraHal_bufferAllocator, kDmabufSystemHeapName, length, 0, 0);
    if (fd < 0) {
        LOGE("Alloc failed: %d\n", fd);
        return NULL;
    }

	*export_fd = fd;

    ptr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        LOGE("mmap failed\n");
        return NULL;
    }

	LOGD("%s(%d): alloc heap fd is %d addr is %p\n", __FUNCTION__, __LINE__, fd, ptr);

	return ptr;
	
}

static void render_dma_heap_uninit(int *fd, int** addr, int buf_len, int buf_num)
{
    for(int i = 0; i < buf_num; i++)
	{
	    munmap(addr[i], buf_len);
		close(fd[i]);
	}
	
}


static void errno_exit(const char *s)
{
    LOGE("%s error %d, %s\n", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}


void MemManagerBase::setBufferStatus(enum buffer_type_enum buf_type,
                                unsigned int buf_idx, int status) {
    struct bufferinfo_s *buf_info;

    switch(buf_type)
    {
        case PREVIEWBUFFER:
            buf_info = mPreviewBufferInfo;
            break;
        case DISPBUFFER:
            buf_info = mDisplayBufferInfo;
            break;
        default:
            LOGE("Buffer type(0x%x) is invaildate",buf_type);
            goto getVirAddr_end;
    }

    if (buf_idx >= buf_info->mNumBffers) {
        LOGE("Buffer index(0x%x) is invalidate, Total buffer is 0x%x",
            buf_idx,buf_info->mNumBffers);
        goto getVirAddr_end;
    }

    (buf_info+buf_idx)->mStatus = status;

getVirAddr_end:
    return;
}

unsigned long MemManagerBase::getBufferAddr(enum buffer_type_enum buf_type,
                                unsigned int buf_idx, buffer_addr_t addr_type)
{
    unsigned long addr = 0x00;
    struct bufferinfo_s *buf_info;

    switch(buf_type)
    {
        case PREVIEWBUFFER:
            buf_info = mPreviewBufferInfo;
            break;
        case DISPBUFFER:
            buf_info = mDisplayBufferInfo;
            break;
        default:
            LOGE("Buffer type(0x%x) is invaildate",buf_type);
            goto getVirAddr_end;
    }

    if (buf_idx > buf_info->mNumBffers) {
        LOGE("Buffer index(0x%x) is invalidate, Total buffer is 0x%x",
            buf_idx,buf_info->mNumBffers);
        goto getVirAddr_end;
    }

    if (addr_type == buffer_addr_vir) {
        addr = (buf_info+buf_idx)->mVirBaseAddr;
    } else if (addr_type == buffer_addr_phy) {
        addr = (buf_info+buf_idx)->mPhyBaseAddr;
    } else if (addr_type == buffer_sharre_fd) {
        addr = (buf_info+buf_idx)->mShareFd;
    }

getVirAddr_end:
    return addr;
}

int MemManagerBase::getIdleBufferIndex(enum buffer_type_enum buf_type) {
    unsigned long addr = 0x00;
    struct bufferinfo_s *buf_info;
    int index = -1;

    switch(buf_type)
    {
        case PREVIEWBUFFER:
            buf_info = mPreviewBufferInfo;
            break;
        case DISPBUFFER:
            buf_info = mDisplayBufferInfo;
            break;
        default:
            LOGE("Buffer type(0x%x) is invaildate",buf_type);
            goto getVirAddr_end;
    }

    for (int i = 0; i < buf_info->mNumBffers; i++)
        if ((buf_info+i)->mStatus == 0) {
            index = i;
            break;
        }

    if (index >= buf_info->mNumBffers) {
        LOGE("Buffer index(0x%x) is invalidate, Total buffer is 0x%x",
            index,buf_info->mNumBffers);
        return -1;
    }
    
getVirAddr_end:
    return index;

}

int MemManagerBase::dump()
{
    return 0;
}

GrallocDrmMemManager::GrallocDrmMemManager(bool iommuEnabled)
                    :MemManagerBase(),
                    mPreviewData(NULL),
                    mHandle(NULL),
                    mOps(NULL)
{

    mOps = get_cam_ops(CAM_MEM_TYPE_GRALLOC);

    if (mOps) {
        mHandle = mOps->init(iommuEnabled ? 1:0,
                        CAM_MEM_FLAG_HW_WRITE |
                        CAM_MEM_FLAG_HW_READ  |
                        CAM_MEM_FLAG_SW_WRITE |
                        CAM_MEM_FLAG_SW_READ,
                        0);
    }

    mDisplayData = NULL;
}

GrallocDrmMemManager::~GrallocDrmMemManager()
{
    LOGD("destruct mem manager");
    if (mPreviewData) {
        destroyPreviewBuffer();
        mPreviewData = NULL;
    }

	if (mDisplayData) {
        destroyDisplayBuffer();
        mDisplayData = NULL;
    }
	
    if(mHandle)
        mOps->deInit(mHandle);
}

int GrallocDrmMemManager::createGrallocDrmBuffer(struct bufferinfo_s* grallocbuf, unsigned int halPixFmt)
{
    int ret =0,i = 0;
    int numBufs;
    int frame_size;
	int cameraFd;
    cam_mem_info_t** tmpalloc = NULL;
    struct bufferinfo_s* tmp_buf = NULL;

    if (!grallocbuf) {
        LOGE("gralloc_alloc malloc buffer failed");
        return -1;
    }

    numBufs = grallocbuf->mNumBffers;
    frame_size = grallocbuf->mPerBuffersize;
    grallocbuf->mBufferSizes = numBufs*PAGE_ALIGN(frame_size);
	cameraFd = grallocbuf->mcameraFd;
    switch(grallocbuf->mBufType)
    {
        case PREVIEWBUFFER:
            tmpalloc = mPreviewData ;
            if((tmp_buf  = (struct bufferinfo_s*)malloc(numBufs*sizeof(struct bufferinfo_s))) != NULL) {
                mPreviewBufferInfo = tmp_buf;
            } else {
                LOGE("gralloc_alloc malloc buffer failed");
            return -1;
            }
        break;
        case DISPBUFFER:
            tmpalloc = mDisplayData ;
            if((tmp_buf  = (struct bufferinfo_s*)malloc(numBufs*sizeof(struct bufferinfo_s))) != NULL) {
                mDisplayBufferInfo = tmp_buf;
            } else {
                LOGE("gralloc_alloc malloc buffer failed");
                return -1;
            }
        break;
        default:
            LOGE("do not support this buffer type");
            return -1;
    }

    if(grallocbuf->mIoMethod == IO_METHOD_MMAP){
	    for(i = 0;i < numBufs; i++) {
#ifndef RK_GRALLOC_4
	        *tmpalloc = mOps->alloc(mHandle,grallocbuf->mPerBuffersize);
#else
	        *tmpalloc = mOps->alloc(mHandle,grallocbuf->mPerBuffersize, halPixFmt,
	                            grallocbuf->width, grallocbuf->height);
#endif
	        if (*tmpalloc) {
	            LOGD("alloc success");
	        } else {
	            LOGE("gralloc mOps->alloc failed");
	            ret = -1;
	            break;
	        }
	        grallocbuf->mPhyBaseAddr = (unsigned long)((*tmpalloc)->phy_addr);
	        grallocbuf->mVirBaseAddr = (unsigned long)((*tmpalloc)->vir_addr);
	        grallocbuf->mPerBuffersize = PAGE_ALIGN(frame_size);
	        grallocbuf->mShareFd     = (unsigned int)((*tmpalloc)->fd);
	        grallocbuf->mStatus      = 0;
	        LOGD("grallocbuf->mVirBaseAddr=0x%lx, grallocbuf->mShareFd=0x%lx",
	            grallocbuf->mVirBaseAddr, grallocbuf->mShareFd);
	        *tmp_buf = *grallocbuf;
	        tmp_buf++;
	        tmpalloc++;
	    }
    }else if(grallocbuf->mIoMethod == IO_METHOD_DMABUF)
    {
		int export_dmafd;
		int bpp = 16;

		if(grallocbuf->mBufType == PREVIEWBUFFER){
		    for(i = 0 ; i < numBufs; i++)
	    	{
				mPreviewBufferInfo[i].mPerBuffersize = frame_size;
				mPreviewBufferInfo[i].mVirBaseAddr = (unsigned long)render_dma_heap_buf(
					&export_dmafd, grallocbuf->width, grallocbuf->height, bpp);//(unsigned long)sur_alloc_drm_buf(&export_dmafd, grallocbuf->width, grallocbuf->height, 
					//bpp, &mDrmHandle);

				if(mPreviewBufferInfo[i].mVirBaseAddr == NULL)
				{
					LOGE("gralloc mOps->alloc failed");
	                ret = -1;
	                break;
				}
				
				mPreviewBufferInfo[i].mPhyBaseAddr = mPreviewBufferInfo[i].mVirBaseAddr;
				mPreviewBufferInfo[i].mShareFd = export_dmafd;
				mPreviewBufferInfo[i].mStatus = 0;
				mPreviewBufferInfo[i].height = grallocbuf->height;
				mPreviewBufferInfo[i].width = grallocbuf->width;
				mPreviewBufferInfo[i].mIoMethod = grallocbuf->mIoMethod;
				mPreviewBufferInfo[i].mNumBffers = grallocbuf->mNumBffers;
	    	}
		}
		else if(grallocbuf->mBufType == DISPBUFFER)
		{
	         bpp = 32;
		     for(i = 0 ; i < numBufs; i++)
	    	{
				mDisplayBufferInfo[i].mPerBuffersize = frame_size;
				mDisplayBufferInfo[i].mVirBaseAddr = (unsigned long)render_dma_heap_buf(
					&export_dmafd, grallocbuf->width, grallocbuf->height, bpp);//(unsigned long)sur_alloc_drm_buf(&export_dmafd, grallocbuf->width, grallocbuf->height, 
					//32, &mDrmHandle);

				if(mDisplayBufferInfo[i].mVirBaseAddr == NULL)
				{
					LOGE("gralloc mOps->alloc failed");
	                ret = -1;
	                break;
				}
				
				mDisplayBufferInfo[i].mPhyBaseAddr = mDisplayBufferInfo[i].mVirBaseAddr;
				mDisplayBufferInfo[i].mShareFd = export_dmafd;
				mDisplayBufferInfo[i].mStatus = 0;
				mDisplayBufferInfo[i].height = grallocbuf->height;
				mDisplayBufferInfo[i].width = grallocbuf->width;
				mDisplayBufferInfo[i].mIoMethod = grallocbuf->mIoMethod;
				mDisplayBufferInfo[i].mNumBffers = grallocbuf->mNumBffers;
	    	}
		}
    }

    if(ret < 0) {
        LOGE("............................alloc failed !");
        while(--i >= 0) {
            
		    if(grallocbuf->mIoMethod == IO_METHOD_MMAP){
				 --tmp_buf;
				 --tmpalloc;
                mOps->free(mHandle,*tmpalloc);
		    }else{

			    if(grallocbuf->mBufType == PREVIEWBUFFER){
		    	    munmap((void*)mPreviewBufferInfo[i].mVirBaseAddr, grallocbuf->width * grallocbuf->height * 2);
                    close(mPreviewBufferInfo[i].mShareFd);
			    }else if(grallocbuf->mBufType == DISPBUFFER)
		    	{
				    munmap((void*)mDisplayBufferInfo[i].mVirBaseAddr, grallocbuf->width * grallocbuf->height * 4);
                    close(mDisplayBufferInfo[i].mShareFd);
		    	}
		    }
        }
		
        switch(grallocbuf->mBufType) {
            case PREVIEWBUFFER:
            if(mPreviewBufferInfo) {
                free(mPreviewBufferInfo);
                mPreviewBufferInfo = NULL;
            }
            break;
            case DISPBUFFER:
            if(mDisplayBufferInfo) {
                free(mDisplayBufferInfo);
                mDisplayBufferInfo = NULL;
            }
            break;
            default:
            break;
        }
    }
    return ret;
}

void GrallocDrmMemManager::destroyGrallocDrmBuffer(buffer_type_enum buftype)
{
    cam_mem_info_t** tmpalloc = NULL;
    struct bufferinfo_s* tmp_buf = NULL;

	int numbufs = 0;

    switch(buftype)
    {
        case PREVIEWBUFFER:
            tmpalloc = mPreviewData;
            tmp_buf = mPreviewBufferInfo;
		    numbufs = tmp_buf->mNumBffers;
        break;
        case DISPBUFFER:
            tmpalloc = mDisplayData;
            tmp_buf = mDisplayBufferInfo;
		    numbufs = tmp_buf->mNumBffers;
        break;
        default:
            LOGE("buffer type is wrong !");
        break;
    }

    for(unsigned int i = 0;(tmp_buf && (i < numbufs));i++) { 
		if(tmp_buf[i].mIoMethod == IO_METHOD_DMABUF)
		{
		    //release_drm_obj((void*)tmp_buf[i].mVirBaseAddr, 
			//	tmp_buf[i].mPerBuffersize, tmp_buf[i].mShareFd, 
			//	mDrmHandle);

			LOGD(">>>>> tmp_buf[i].mVirBaseAddr is %x\n", tmp_buf[i].mVirBaseAddr);

			if(buftype == PREVIEWBUFFER)
			    munmap((void*)tmp_buf[i].mVirBaseAddr, tmp_buf[i].width * tmp_buf[i].height * 2);
			else
				munmap((void*)tmp_buf[i].mVirBaseAddr, tmp_buf[i].width * tmp_buf[i].height * 4);
            close(tmp_buf[i].mShareFd);
		}
		else
		{
			if(*tmpalloc && (*tmpalloc)->vir_addr) {
	            LOGD("free graphic buffer");
	            mOps->free(mHandle,*tmpalloc);
	        }
			tmpalloc++;
	    }     
    }

    switch(buftype)
    {
        case PREVIEWBUFFER:
			if(mPreviewData)
                free(mPreviewData);
            mPreviewData = NULL;
			if(mPreviewBufferInfo)
                free(mPreviewBufferInfo);
            mPreviewBufferInfo = NULL;
            LOGD("free mPreviewData");
        break;
        case DISPBUFFER:
			if(mDisplayData)
                free(mDisplayData);
            mDisplayData = NULL;
			if(mDisplayBufferInfo)
                free(mDisplayBufferInfo);
            mDisplayBufferInfo = NULL;
            LOGD("free mDisplayData");
        break;
        default:
            LOGE("buffer type is wrong !");
        break;
    }

	if(cameraHal_bufferAllocator != NULL && (mPreviewBufferInfo == NULL) && (mDisplayBufferInfo == NULL))
	{
	    FreeDmabufHeapBufferAllocator(cameraHal_bufferAllocator);
	    cameraHal_bufferAllocator = NULL;
	}
}

int GrallocDrmMemManager::createPreviewBuffer(struct bufferinfo_s* previewbuf)
{
    int ret;
    Mutex::Autolock lock(mLock);

    if(previewbuf->mBufType != PREVIEWBUFFER)
        LOGE("the type is not PREVIEWBUFFER");

    if(previewbuf->mIoMethod != IO_METHOD_DMABUF){
	    if(!mPreviewData) {
	        mPreviewData = (cam_mem_info_t**)malloc(sizeof(cam_mem_info_t*) * previewbuf->mNumBffers);
	        if(!mPreviewData) {
	            LOGE("malloc mPreviewData failed!");
	            ret = -1;
	            return ret;
	        }
	    } else if ((*mPreviewData)->vir_addr) {
	        LOGD("FREE the preview buffer alloced before firstly");
	        destroyPreviewBuffer();
	    }

	    memset(mPreviewData,0,sizeof(cam_mem_info_t*)* previewbuf->mNumBffers);
    }

    ret = createGrallocDrmBuffer(previewbuf, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
    if (ret == 0) {
        LOGD("Preview buffer information(phy:0x%lx vir:0x%lx size:0x%zx)",
            mPreviewBufferInfo->mPhyBaseAddr,
            mPreviewBufferInfo->mVirBaseAddr,
            mPreviewBufferInfo->mBufferSizes);
    } else {
        LOGE("Preview buffer alloc failed");
        if (mPreviewData){
            free(mPreviewData);
            mPreviewData = NULL;
        }
    }

    return ret;
}
int GrallocDrmMemManager::destroyPreviewBuffer()
{
    Mutex::Autolock lock(mLock);
    destroyGrallocDrmBuffer(PREVIEWBUFFER);

    return 0;
}

int GrallocDrmMemManager::createDisplayBuffer(struct bufferinfo_s* displaybuf)
{
    int ret;
    Mutex::Autolock lock(mLock);

    if(displaybuf->mBufType != DISPBUFFER)
        LOGE("the type is not DISPBUFFER");

	if(displaybuf->mIoMethod != IO_METHOD_DMABUF){

	    if(!mDisplayData) {
	        mDisplayData = (cam_mem_info_t**)malloc(sizeof(cam_mem_info_t*) * displaybuf->mNumBffers);
	        if(!mDisplayData) {
	            LOGE("malloc mDisplayData failed!");
	            ret = -1;
	            return ret;
	        }
	    } else if ((*mDisplayData)->vir_addr) {
	        LOGD("FREE the display buffer alloced before firstly");
	        destroyDisplayBuffer();
	    }

	    memset(mDisplayData,0,sizeof(cam_mem_info_t*)* displaybuf->mNumBffers);
	}

    ret = createGrallocDrmBuffer(displaybuf, HAL_PIXEL_FORMAT_RGBA_8888);
    if (ret == 0) {
        LOGD("Display buffer information(phy:0x%lx vir:0x%lx size:0x%zx)",
            mDisplayBufferInfo->mPhyBaseAddr,
            mDisplayBufferInfo->mVirBaseAddr,
            mDisplayBufferInfo->mBufferSizes);
    } else {
        LOGE("Display buffer alloc failed");
        if (mDisplayData){
            free(mDisplayData);
            mDisplayData = NULL;
        }
    }

    return ret;
}
int GrallocDrmMemManager::destroyDisplayBuffer()
{
    Mutex::Autolock lock(mLock);
    destroyGrallocDrmBuffer(DISPBUFFER);

    return 0;
}


int GrallocDrmMemManager::flushCacheMem(buffer_type_enum buftype)
{
    Mutex::Autolock lock(mLock);
    cam_mem_info_t** tmpalloc = NULL;
    struct bufferinfo_s* tmp_buf = NULL;

	if(mPreviewBufferInfo != NULL && mPreviewBufferInfo[0].mIoMethod == IO_METHOD_DMABUF)
		return 0;

    switch(buftype)
    {
        case PREVIEWBUFFER:
            tmpalloc = mPreviewData;
            tmp_buf = mPreviewBufferInfo;
        break;
        case DISPBUFFER:
            tmpalloc = mDisplayData;
            tmp_buf = mDisplayBufferInfo;
        break;
        default:
            LOGE("buffer type is wrong !");
        break;
    }

	if(tmp_buf->mIoMethod == IO_METHOD_MMAP){
	    for(unsigned int i = 0;(tmp_buf && (i < tmp_buf->mNumBffers));i++) {
	        if(*tmpalloc && (*tmpalloc)->vir_addr) {
#ifndef RK_GRALLOC_4
            int ret = mOps->flush_cache(mHandle, *tmpalloc);
#else
            int ret = mOps->flush_cache(mHandle, *tmpalloc, (*tmpalloc)->width, (*tmpalloc)->height);
#endif
            if(ret != 0)
                LOGD("flush cache failed !");
        }
        tmpalloc++;
    }
	}
    return 0;
}

}/* namespace android */
