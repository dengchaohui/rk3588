/******************************************************************************
 *
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
 * BY DOWNLOADING, INSTALLING, COPYING, SAVING OR OTHERWISE USING THIS SOFTWARE,
 * YOU ACKNOWLEDGE THAT YOU AGREE THE SOFTWARE RECEIVED FROM ROCKCHIP IS PROVIDED
 * TO YOU ON AN "AS IS" BASIS and ROCKCHIP DISCLAIMS ANY AND ALL WARRANTIES AND
 * REPRESENTATIONS WITH RESPECT TO SUCH FILE, WHETHER EXPRESS, IMPLIED, STATUTORY
 * OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR
 * A PARTICULAR PURPOSE.
 * Rockchip hereby grants to you a limited, non-exclusive, non-sublicensable and
 * non-transferable license (a) to install, save and use the Software; (b) to copy
 * and distribute the Software in binary code format only.
 * Except as expressively authorized by Rockchip in writing, you may NOT: (a) distribute
 * the Software in source code; (b) distribute on a standalone basis but you may distribute
 * the Software in conjunction with platforms incorporating Rockchip integrated circuits;
 * (c) modify the Software in whole or part;(d) decompile, reverse-engineer, dissemble,
 * or attempt to derive any source code from the Software;(e) remove or obscure any copyright,
 * patent, or trademark statement or notices contained in the Software.
 *
 *****************************************************************************/
#include "CameraHal.h"
#include "RgaCropScale.h"
#include <hardware/gralloc.h>
#include "BufferAllocatorWrapper.h"

namespace android{

#define DISPLAY_FORMAT CAMERA_DISPLAY_FORMAT_RGB888/*CAMERA_DISPLAY_FORMAT_YUV420P*/

DisplayAdapter::DisplayAdapter()           
{
    LOG_FUNCTION_NAME
//    strcpy(mDisplayFormat,CAMERA_DISPLAY_FORMAT_YUV420SP/*CAMERA_DISPLAY_FORMAT_YUV420SP*/);
    strcpy(mDisplayFormat,DISPLAY_FORMAT);
    mFrameProvider =  NULL;
    mDisplayRuning = -1;
    mDislayBufNum = 0;
    mDisplayWidth = 0;
    mDisplayHeight = 0;
    mDisplayBufInfo =NULL;
    mDisplayState = 0;
    //create display thread

#if MULTI_PT_USING_QUEUE	
	mDCmdsem.Create();
	mStateComplete.Create();
#else
	displayThreadCommandQ = new MessageQueue("displayCmdQ");
#endif	

    mTmpMemManager = new GrallocDrmMemManager(false);
    mTmpBuf = NULL;
    mDisplayThread = new DisplayThread(this);
    mDisplayThread->run("DisplayThread",ANDROID_PRIORITY_DISPLAY);

    char dump_type[PROPERTY_VALUE_MAX];
    int type;

    //property_get("sys.camera.camera360_dump", dump_type, "0");

    //sscanf(dump_type,"%d",&type);

    //if (type & DUMP_DISPLAY)
     //   mDumpFrame = true;
    //else
     //   mDumpFrame = false;

    mDumpFrame = false;
    fillbuffer_sem.Create();
    need_fill_buf = false;
    //initDrmHwc();
    LOG_FUNCTION_NAME_EXIT
}

DisplayAdapter::~DisplayAdapter()
{
    LOG_FUNCTION_NAME

    if(mDisplayThread != NULL){
        //stop thread and exit
        if(mDisplayRuning != STA_DISPLAY_STOP)
            stopDisplay();

        mDisplayThread->requestExitAndWait();
        mDisplayThread.clear();
    }

    if (mTmpBuf) {
        mTmpBuf->freeBuffer();
        delete mTmpBuf;
        mTmpBuf = NULL;
    }

#if MULTI_PT_USING_QUEUE	
	mDCmdsem.Release();
	mStateComplete.Release();
#else		
	if(displayThreadCommandQ)
		delete displayThreadCommandQ;
#endif

    LOG_FUNCTION_NAME_EXIT
}

void DisplayAdapter::dump()
{

}

int DisplayAdapter::initDrmHwc() {
  return 0;
}

void DisplayAdapter::setDisplayState(int state)
{
    mDisplayState = state;
}

bool DisplayAdapter::isNeedSendToDisplay()
{
    Mutex::Autolock lock(mDisplayLock);

    if((mDisplayRuning == STA_DISPLAY_PAUSE) || (mDisplayRuning == STA_DISPLAY_STOP)
        ||(mDisplayState == CMD_DISPLAY_PAUSE_PREPARE)
        ||(mDisplayState == CMD_DISPLAY_PAUSE_DONE)
        ||(mDisplayState == CMD_DISPLAY_STOP_PREPARE)
        ||(mDisplayState == CMD_DISPLAY_STOP_DONE))
        return false;
    else{
        LOG2("need to display this frame");
        return true;
    }
}

void DisplayAdapter::notifyNewFrame(int index)
{
    mDisplayLock.lock();
    //send a frame to display
    if((mDisplayRuning == STA_DISPLAY_RUNNING)
        &&(mDisplayState != CMD_DISPLAY_PAUSE_PREPARE)
        &&(mDisplayState != CMD_DISPLAY_PAUSE_DONE)
        &&(mDisplayState != CMD_DISPLAY_STOP_PREPARE)
        &&(mDisplayState != CMD_DISPLAY_STOP_DONE))
   {
        LOG2("dispbuf %d sendto display", index);
        //mReadyIndex = index;

#if MULTI_PT_USING_QUEUE
		mDThreadBufIndex.push(index);
		mDThreadCmdsQueue.push(CMD_DISPLAY_FRAME);
		mDCmdsem.Signal();
#else
        Message_cam msg;
        msg.command = CMD_DISPLAY_FRAME;
        msg.arg1 = NULL;
        msg.arg2 = (void*) (long) index;
        displayThreadCommandQ->put(&msg);
#endif
		mDisplayCond.signal();
    }else{
    //must return frame if failed to send display
        if(mDisplayBufs)
            mDisplayBufs->setBufferStatus(index, 0);
    }
    mDisplayLock.unlock();
}


int DisplayAdapter::startDisplay(int width, int height)
{
    int err = NO_ERROR;
    Message_cam msg;
    Semaphore sem;
    LOG_FUNCTION_NAME
    mDisplayLock.lock();
    #if 1
        if (mDisplayRuning == STA_DISPLAY_RUNNING) {
            LOGD("%s(%d): display thread is already run",__FUNCTION__,__LINE__);
            goto cameraDisplayThreadStart_end;
        }
    #endif
    mDisplayWidth = width;
    mDisplayHeight = height;
    setDisplayState(CMD_DISPLAY_START_PREPARE);
#if MULTI_PT_USING_QUEUE
	mDThreadCmdsQueue.push(CMD_DISPLAY_START);
	mDCmdsem.Signal();
#else

	msg.command = CMD_DISPLAY_START;
    sem.Create();
    msg.arg1 = (void*)(&sem);
    displayThreadCommandQ->put(&msg);
#endif
    mDisplayCond.signal();

#if !MULTI_PT_USING_QUEUE
cameraDisplayThreadStart_end:
#endif
    mDisplayLock.unlock();

#if MULTI_PT_USING_QUEUE
	mStateComplete.Wait();
	if(mDisplayState != CMD_DISPLAY_START_DONE)
		err = -1;
#else
    if(msg.arg1){
        sem.Wait();
        if(mDisplayState != CMD_DISPLAY_START_DONE)
            err = -1;
    }
#endif

#if MULTI_PT_USING_QUEUE
cameraDisplayThreadStart_end:
#endif
    LOG_FUNCTION_NAME_EXIT
    return err;
}

int DisplayAdapter::stopDisplay()
{
    int err = NO_ERROR;
    Message_cam msg;
    Semaphore sem;
    LOG_FUNCTION_NAME
    mDisplayLock.lock();
    if (mDisplayRuning == STA_DISPLAY_STOP) {
        LOGD("%s(%d): display thread is already pause",__FUNCTION__,__LINE__);
        goto cameraDisplayThreadStop_end;
    }
    setDisplayState(CMD_DISPLAY_STOP_PREPARE);
#if MULTI_PT_USING_QUEUE
	mDThreadCmdsQueue.push(CMD_DISPLAY_STOP);
	mDCmdsem.Signal();
#else
	msg.command = CMD_DISPLAY_STOP;
	sem.Create();
	msg.arg1 = (void*)(&sem);
	displayThreadCommandQ->put(&msg);
#endif
	mDisplayCond.signal();

#if !MULTI_PT_USING_QUEUE
cameraDisplayThreadStop_end:
#endif
	mDisplayLock.unlock();

#if MULTI_PT_USING_QUEUE
	mStateComplete.Wait();
	if(mDisplayState != CMD_DISPLAY_STOP_DONE)
		err = -1;
#else
	if(msg.arg1){
		sem.Wait();
		if(mDisplayState != CMD_DISPLAY_STOP_DONE)
			err = -1;
	}
#endif
	
#if MULTI_PT_USING_QUEUE
cameraDisplayThreadStop_end:
#endif
	LOG_FUNCTION_NAME_EXIT
	return err;
}

int DisplayAdapter::pauseDisplay()
{
    int err = NO_ERROR;
    Message_cam msg;
    Semaphore sem;
    mDisplayLock.lock();
    LOG_FUNCTION_NAME
    if (mDisplayRuning == STA_DISPLAY_PAUSE) {
        LOGD("%s(%d): display thread is already stop",__FUNCTION__,__LINE__);
        goto cameraDisplayThreadStop_end;
    }
    setDisplayState(CMD_DISPLAY_PAUSE_PREPARE);
#if MULTI_PT_USING_QUEUE
	mDThreadCmdsQueue.push(CMD_DISPLAY_PAUSE);
	mDCmdsem.Signal();
#else
	msg.command = CMD_DISPLAY_PAUSE;
	sem.Create();
	msg.arg1 = (void*)(&sem);
	displayThreadCommandQ->put(&msg);
#endif
	mDisplayCond.signal();

#if !MULTI_PT_USING_QUEUE
cameraDisplayThreadStop_end:
#endif
	mDisplayLock.unlock();

#if MULTI_PT_USING_QUEUE
	mStateComplete.Wait();
	if(mDisplayState != CMD_DISPLAY_PAUSE_DONE)
		err = -1;
#else
	if(msg.arg1){
		sem.Wait();
		if(mDisplayState != CMD_DISPLAY_PAUSE_DONE)
			err = -1;
	}
#endif

#if MULTI_PT_USING_QUEUE
cameraDisplayThreadStop_end:
#endif
	LOG_FUNCTION_NAME_EXIT
	return err;

}

int DisplayAdapter::fillDisplayBuffer(void *buffer, int width, int height)
{

    fill_buffer = buffer;
    fill_width = width;
    fill_height = height;

    need_fill_buf = true;
    fillbuffer_sem.Wait();
    need_fill_buf = false;
    return 0;
}

int DisplayAdapter::setDisplaySize(int width, int height)
{
    fill_width = width;
    fill_height = height;

    if (mTmpBuf) {
        mTmpBuf->freeBuffer();
        delete mTmpBuf;
        mTmpBuf = NULL;
    }

    mTmpBuf = new BufferProvider(mTmpMemManager);
    mTmpBuf->createBuffer(1, width, height, width*height*2, DISPBUFFER,false);
    return 0;
}

int DisplayAdapter::getDisplayStatus(void)
{
    Mutex::Autolock lock(mDisplayLock);

    return mDisplayRuning;
}

//internal func
int DisplayAdapter::cameraDisplayBufferCreate(int width, int height, const char *fmt,int numBufs)
{
    int framesize = 0;
    if (!strcmp(fmt, CAMERA_DISPLAY_FORMAT_RGB888))
        framesize = ((width +15)&(~15))*((height+15)&(~15)) * 4;
    else if (!strcmp(fmt, CAMERA_DISPLAY_FORMAT_NV12))
        framesize = ((width +15)&(~15))*((height+15)&(~15))*3/2;
    else
        framesize = ((width +15)&(~15))*((height+15)&(~15)) * 4;
    if (mDisplayBufs->createBuffer(CONFIG_CAMERA_DISPLAY_BUF_CNT, width, height, framesize, DISPBUFFER,false) < 0) {
        LOGE("display buff create fail");
        return -1;
    }
    return 0;
}

int DisplayAdapter::cameraDisplayBufferDestory(void)
{
    if (mDisplayBufs){
        LOGD("%s(%d):freeBuffer cnt %d",__FUNCTION__,__LINE__,mDisplayBufs->getBufCount());
        mDisplayBufs->freeBuffer();
        LOGD("freeBuffer done");
    }
    return 0;
}

void DisplayAdapter::setBufferState(int index,int status)
{
     if (mDisplayBufs)
         mDisplayBufs->setBufferStatus(index,status);
}

int DisplayAdapter::getOneAvailableBuffer(long *buf_phy,long *buf_vir, int *fd)
{
    if (mDisplayBufs)
        return mDisplayBufs->getOneAvailableBuffer(buf_phy,buf_vir, fd);
    return -1;
}

extern BufferAllocator* cameraHal_bufferAllocator;

int test_display_buffer = 0;
void DisplayAdapter::displayThread()
{
    int err,stride,i,queue_cnt;
    long dequeue_buf_index,queue_buf_index,queue_display_index;
    buffer_handle_t *hnd = NULL;
    NATIVE_HANDLE_TYPE *phnd;
    GraphicBufferMapper& mapper = GraphicBufferMapper::get();
    Message_cam msg;
    void *y_uv[3];
    long frame_used_flag = -1;
    Rect bounds;

    LOG_FUNCTION_NAME
    while (mDisplayRuning != STA_DISPLAY_STOP) {
display_receive_cmd:

#if MULTI_PT_USING_QUEUE
		mDCmdsem.Wait();

		if (mDThreadCmdsQueue.empty() == false ) {
            enum DisplayThreadCommands cue_cmd = mDThreadCmdsQueue.front();
			mDThreadCmdsQueue.pop();

            switch (cue_cmd)
#else
		if (displayThreadCommandQ->isEmpty() == false ) {
			displayThreadCommandQ->get(&msg);

			switch (msg.command)
#endif
            {
                case CMD_DISPLAY_START:
                {
                    LOGD("%s(%d): receive CMD_DISPLAY_START", __FUNCTION__,__LINE__);
                    //cameraDisplayBufferDestory();
                    err = cameraDisplayBufferCreate(mDisplayWidth, mDisplayHeight,mDisplayFormat,CONFIG_CAMERA_DISPLAY_BUF_CNT);
                    //err = initDrmHwc();
                    if (err == 0){
                    mDisplayRuning = STA_DISPLAY_RUNNING;
                    setDisplayState(CMD_DISPLAY_START_DONE);
                    }

					#if MULTI_PT_USING_QUEUE
					mStateComplete.Signal();
					#else
                    if(msg.arg1)
                        ((Semaphore*)msg.arg1)->Signal();
					#endif
                    break;
                }

                case CMD_DISPLAY_PAUSE:
                {
                    LOGD("%s(%d): receive CMD_DISPLAY_PAUSE", __FUNCTION__,__LINE__);
                    mDisplayRuning = STA_DISPLAY_PAUSE;
                    setDisplayState(CMD_DISPLAY_PAUSE_DONE);
#if MULTI_PT_USING_QUEUE
					mStateComplete.Signal();
#else
                    if(msg.arg1)
                        ((Semaphore*)msg.arg1)->Signal();
#endif

                    break;
                }

                case CMD_DISPLAY_STOP:
                {
                    LOGD("%s(%d): receive CMD_DISPLAY_STOP", __FUNCTION__,__LINE__);
					if(mDisplayBufs){
                        mDisplayBufs->setBufferStatus(0, 0);
                        mDisplayBufs->setBufferStatus(1, 0);
					}
                    cameraDisplayBufferDestory();
                    mDisplayRuning = STA_DISPLAY_STOP;
                    setDisplayState(CMD_DISPLAY_STOP_DONE);
#if MULTI_PT_USING_QUEUE
					mStateComplete.Signal();
#else
                    if(msg.arg1)
                        ((Semaphore*)msg.arg1)->Signal();
#endif

                    continue;
                }

                case CMD_DISPLAY_FRAME:
                {

#if !MULTI_PT_USING_QUEUE			
                    if(msg.arg1)
                        ((Semaphore*)msg.arg1)->Signal();
#endif
                    if (mDisplayRuning != STA_DISPLAY_RUNNING)
                        goto display_receive_cmd;

                    long buf_phy = 0,buf_vir = 0;
                    long tmp_vir = 0;
					int disp_drmfd = -1, tmp_fd = -1;
#if MULTI_PT_USING_QUEUE
					int buf_index = mDThreadBufIndex.front();
					mDThreadBufIndex.pop();

#else
                    int buf_index = (long) msg.arg2;
#endif
                    LOGD("recevie buf to display index:%d [%d %d]", buf_index, fill_width, fill_height);

                    //get a free buffer
                    buf_vir = mDisplayBufs->getBufVirAddr(buf_index);
                    tmp_vir = mTmpBuf->getBufVirAddr(0);
					disp_drmfd = mDisplayBufs->getBufShareFd(buf_index);
					tmp_fd = mTmpBuf->getBufShareFd(0);
                    if(need_fill_buf){
                        if(mTmpBuf){
                            int ret = camera2::RgaCropScale::rga_simple(
                                mDisplayWidth, mDisplayHeight,
                                (unsigned long)disp_drmfd, (unsigned long)tmp_fd,
                                fill_width, fill_height,
                                HAL_PIXEL_FORMAT_BGRA_8888, HAL_PIXEL_FORMAT_RGB_565, true);

								ret = DmabufHeapCpuSyncStart(cameraHal_bufferAllocator, tmp_fd, kSyncReadWrite,
															 NULL,
															 NULL);

                                memcpy(fill_buffer, (char *)tmp_vir, fill_width*fill_height*2);

								ret = DmabufHeapCpuSyncEnd(cameraHal_bufferAllocator, tmp_fd, kSyncReadWrite,
                                   NULL, NULL);
#if 0
							LOGD("LibCamera360 test_display_buffer %d\n ", test_display_buffer);
							 if(test_display_buffer > 5 && test_display_buffer < 15)
						 	{
						 	     LOGD("LibCamera360 start dump\n ", test_display_buffer);
				 	    	     FILE* fp =NULL;
							     char filename[40];
							     filename[0] = 0x00;
							     sprintf(filename, "/data/DISP565_%dx%d", fill_height, fill_width);
							     fp = fopen(filename, "ab+");
							     if (fp) {
							        fwrite((char*)tmp_vir, 1, fill_width * fill_height * 4,fp);
									fflush(fp);
							        fclose(fp);
							        LOGD("LibCamera360 Write success rgb data to %s\n",filename);
							     } else {
							        LOGE("LibCamera360 Create %s failed(%d, %s)",filename,fp, strerror(errno));
							     }
								 LOGD("LibCamera360 end dump\n ", test_display_buffer);
						 	}

						    test_display_buffer++;
#endif
                        }
                        fillbuffer_sem.Signal();
                    }


/*
                     LOG2("win_buffer success! vir_addr = 0x%x , phy_addr = %p , size = %zu \n",file_args.virtual_addr,file_args.phy_addr,file_args.size);

                    int ret = camera2::RgaCropScale::rga_nv12_scale_crop(
                        mDisplayWidth, mDisplayHeight,
                        (unsigned long)buf_vir, (unsigned long)file_args.virtual_addr,
                        mDisplayWidth, mDisplayHeight,
                        HAL_PIXEL_FORMAT_RGBA_8888, HAL_PIXEL_FORMAT_RGBA_8888,
                        100, 90, false, true,
                        true, true);

                    if (ret != 0) memcpy(file_args.virtual_addr, (char *)buf_vir, mDisplayWidth*mDisplayHeight*4);

                  if (mDumpFrame) {
                         FILE* fp =NULL;
                         char filename[40];
                         filename[0] = 0x00;
                         sprintf(filename, "/data/camera/RGBA8888_%dx%d_2", mDisplayWidth, mDisplayHeight);
                         fp = fopen(filename, "wb");
                         if (fp) {
                            fwrite((char*)file_args.virtual_addr, 1,mDisplayWidth*mDisplayWidth *4,fp);
                            fclose(fp);
                            LOG1("Write success rgb data to %s",filename);
                         } else {
                            LOGE("Create %s failed(%d, %s)",filename,fp, strerror(errno));
                         }
                    }
                    */
                    mDisplayBufs->setBufferStatus(buf_index, 0);
                    break;
                }

                default:
                {
                    LOGE("%s(%d): receive unknow command(0x%x)!", __FUNCTION__,__LINE__,msg.command);
                    break;
                }
            }
        }

        mDisplayLock.lock();

#if MULTI_PT_USING_QUEUE
        if (mDThreadCmdsQueue.empty()== false ) {
#else
		if (displayThreadCommandQ->isEmpty() == false ) {
#endif
            mDisplayLock.unlock();
            goto display_receive_cmd;
        }
        LOG2("%s(%d): display thread pause here... ", __FUNCTION__,__LINE__);
        mDisplayCond.wait(mDisplayLock);
        mDisplayLock.unlock();
        LOG2("%s(%d): display thread wake up... ", __FUNCTION__,__LINE__);
        goto display_receive_cmd;

    }
    LOG_FUNCTION_NAME_EXIT
}

} // namespace android

