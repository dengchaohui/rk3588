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
#include <hardware/gralloc.h>
#include <unistd.h>
#include <sys/time.h>
namespace android{
#define DISPLAY_FORMAT CAMERA_DISPLAY_FORMAT_YUV420SP/*CAMERA_DISPLAY_FORMAT_YUV420P*/
ProcessAdapter::ProcessAdapter()
{
    LOG_FUNCTION_NAME
//    strcpy(mDisplayFormat,CAMERA_DISPLAY_FORMAT_YUV420SP/*CAMERA_DISPLAY_FORMAT_YUV420SP*/);
    strcpy(mDisplayFormat,DISPLAY_FORMAT);
    mFrameProvider =  NULL;
    mProcessRuning = -1;
    mDislayBufNum = 0;
    mDisplayWidth = 0;
    mDisplayHeight = 0;
	mInternBufferCount = 0;
    mProcessState = 0;
	

#if MULTI_PT_USING_QUEUE	
	mPCmdsem.Create();
	mStateComplete.Create();
#else
	processThreadCommandQ = new MessageQueue("processCmdQ");
#endif	
    //create display thread
    mProcessThread = new ProcessThread(this);
    mProcessThread->run("ProcessThread", ANDROID_PRIORITY_DISPLAY);
    //initSurround3d();
    LOG_FUNCTION_NAME_EXIT
}

ProcessAdapter::~ProcessAdapter()
{
    LOG_FUNCTION_NAME
    if(mProcessThread != NULL){
        //stop thread and exit
        if(mProcessRuning != STA_PROCESS_STOP)
            stopProcess();
        mProcessThread->requestExitAndWait();
        mProcessThread.clear();
    }

#if MULTI_PT_USING_QUEUE	
	mPCmdsem.Release();
	mStateComplete.Release();
#else
	if(processThreadCommandQ)
		delete processThreadCommandQ;
#endif	
	//LOGD("mProcessThread success delete and exit\n");
    if (mSurround3d) {
        delete(mSurround3d);
    }
	//LOGD("mSurround3d success delete and exit\n");
    LOG_FUNCTION_NAME_EXIT
}

void ProcessAdapter::setSurround3dType(int type)
{
    enum RK_SURROUND_3D_CMD t = RK_SURROUND_3D_CMD(type);

    mSurround3d->rk_surround_3d_cmd_proc(t, NULL);
}


void ProcessAdapter::setScreenOffset(int x_offset, int y_offset)
{
    this->ScreenOffset[0] = x_offset;
	this->ScreenOffset[1] = y_offset;
	mSurround3d->rk_surround_3d_cmd_proc(rk_cmd_render_set_screen_offset, ScreenOffset);
}

void ProcessAdapter::setInternBufferAcc(int offet)
{
    Mutex::Autolock lock(mProcessLock);
	mInternBufferCount += offet;
	return;
}

void ProcessAdapter::initSurround3d()
{
    LOGD("initSurround3d====");
    mSurround3d = new surround_3d();
	mSurround3d->rk_surround_3d_set_car_model_path(model_path);
	mSurround3d->rk_surround_3d_set_calib_base_path(calib_result_path);
	mSurround3d->rk_surround_3d_set_base_content_path(base_content_path);
    mSurround3d->rk_surround_3d_cam_param_init(CAMERA_NUM, 1920, 1080, image_buf);
    mSurround3d->rk_surround_3d_init_set_output_size(1920, 1080);
    mSurround3d->rk_surround_3d_set_egl_type(EGL_PBUFFER);
    mSurround3d->rk_surround_3d_set_model_scale_factor(2, 2, 2);
    mSurround3d->rk_surround_3d_init();

    if (mSurround3d->ret_record) {
        LOGD("surround3d init sucess");
    } else {
        LOGE("surround3d init fail");
    }
}

void ProcessAdapter::deinitSurround3d()
{
    if (mSurround3d)
        mSurround3d->rk_surround_3d_deinit();
}

void ProcessAdapter::setProcessState(int state)
{
    mProcessState = state;
}

int ProcessAdapter::getProcessState()
{
    Mutex::Autolock lock(mProcessLock);
    return mProcessState;
}

int ProcessAdapter::getReadyFramesIndex(int curId)
{
    if (mReadyCounts[curId] == CONFIG_CAMERA_NUM) return curId;
    /*for (int i = 0; i < CONFIG_CAMERA_PREVIEW_BUF_CNT; i++) {
        if (mReadyCounts[i] == CONFIG_CAMERA_PREVIEW_BUF_CNT) return i;
    }*/
    return -1;
}

bool ProcessAdapter::isNeedSendToProcess()
{
    Mutex::Autolock lock(mProcessLock);
    //printf("cur frame state is %d\n", mProcessState);
    if((mProcessRuning == STA_PROCESS_PAUSE) || (mProcessRuning == STA_PROCESS_STOP)
        ||(mProcessState == CMD_PROCESS_PAUSE_PREPARE)
        ||(mProcessState == CMD_PROCESS_PAUSE_DONE)
        ||(mProcessState == CMD_PROCESS_STOP_PREPARE)
        ||(mProcessState == CMD_PROCESS_STOP_DONE))
        return false;
    else{
        LOG2("need to process this frame");
        return true;
    }
}

void ProcessAdapter::notifyShareFd(int *fd, int camera_id, void* sem_data)
{
    mProcessLock.lock();
    LOG2("notifyShareFd:%d state:%d recevie camera %d index:%d", mProcessRuning, mProcessState,
        camera_id);

     if((mProcessRuning == STA_PROCESS_RUNNING)
         &&(mProcessState != CMD_PROCESS_PAUSE_PREPARE)
         &&(mProcessState != CMD_PROCESS_PAUSE_DONE)
         &&(mProcessState != CMD_PROCESS_STOP_PREPARE)
         &&(mProcessState != CMD_PROCESS_STOP_DONE)
     )
    {
        int cur_camera_id = camera_id;
        for(int m = 0; m < CONFIG_CAMERA_PREVIEW_BUF_CNT; m++){
            LOGD("notifyShareFd GET fd is %d %d\n", Framefd[camera_id][m], fd[m]);
            Framefd[camera_id][m] = fd[m];
            //memcpy(&Framefd[camera_id][0], &fd[0], sizeof(int) * CONFIG_CAMERA_NUM);
        }
#if MULTI_PT_USING_QUEUE
		mPThreadValidCameraID.push(cur_camera_id);
		mPThreadCmdsQueue.push(CMD_PROCESS_SHARE_FD);
		mPCmdsem.Signal();
#else
        Message_cam msg;
        msg.command = CMD_PROCESS_SHARE_FD;
        msg.arg1 = sem_data;
        msg.arg2 = (void *)(long)cur_camera_id;
        processThreadCommandQ->put(&msg);
#endif
        mProcessCond.signal();
    }
    mProcessLock.unlock();
}

void ProcessAdapter::notifyNewFrame(FramInfo_s* frame)
{
    LOG2("Processrun:%d state:%d recevie camera %d index:%d", mProcessRuning, mProcessState,
        frame->cameraId, frame->frame_index);
    mProcessLock.lock();
    //send a frame to display
    if((mProcessRuning == STA_PROCESS_RUNNING)
        &&(mProcessState != CMD_PROCESS_PAUSE_PREPARE)
        &&(mProcessState != CMD_PROCESS_PAUSE_DONE)
        &&(mProcessState != CMD_PROCESS_STOP_PREPARE)
        &&(mProcessState != CMD_PROCESS_STOP_DONE))
   {
        mReadyCounts[frame->frame_index]++;
        LOG2("index %d readycount:%d", frame->frame_index, mReadyCounts[frame->frame_index]);
        mFrames[frame->frame_index][frame->cameraId] = *frame;
        int readyIndex = getReadyFramesIndex(frame->frame_index);
        if (readyIndex != -1) {
            LOG2("ready to process index:%d", readyIndex);
#if MULTI_PT_USING_QUEUE
			mPThreadValidIndex.push(readyIndex);
			mPThreadCmdsQueue.push(CMD_PROCESS_FRAME);
			mPCmdsem.Signal();
#else
            Message_cam msg;
            msg.command = CMD_PROCESS_FRAME;
            msg.arg1 = NULL;
            msg.arg2 = (void *) (long)readyIndex;
            processThreadCommandQ->put(&msg);
#endif
            mProcessCond.signal();			
        }
    }else{
    //must return frame if failed to send display
        if (mCameras[frame->cameraId] != NULL) {
            CameraAdapter* camAdapter = mCameras[frame->cameraId]->getCameraAdapter();
            if (camAdapter != NULL)
                camAdapter->returnFrame(frame->frame_index,frame->used_flag);
        }

    }
    mProcessLock.unlock();
}

int ProcessAdapter::startProcess(int width, int height)
{
    int err = NO_ERROR;
    Message_cam msg;
    Semaphore sem;
    LOG_FUNCTION_NAME
    mProcessLock.lock();
    if (mProcessRuning == STA_PROCESS_RUNNING) {
        LOGD("%s(%d): process thread is already run",__FUNCTION__,__LINE__);
        goto cameraProcessThreadStart_end;
    }
    mDisplayWidth = width;
    mDisplayHeight = height;
    setProcessState(CMD_PROCESS_START_PREPARE);

#if MULTI_PT_USING_QUEUE
	mPThreadCmdsQueue.push(CMD_PROCESS_START);
	mPCmdsem.Signal();
#else
	msg.command = CMD_PROCESS_START;
    sem.Create();
    msg.arg1 = (void*)(&sem);
	LOGD("....................START PROCESS\n");
    processThreadCommandQ->put(&msg);
	LOGD("....................END PROCESS\n");
#endif
	
    mProcessCond.signal();
#if !MULTI_PT_USING_QUEUE
cameraProcessThreadStart_end:
#endif
    mProcessLock.unlock();
#if MULTI_PT_USING_QUEUE
	mStateComplete.Wait();
    if(mProcessState != CMD_PROCESS_START_DONE)
        err = -1;
#else
	if(msg.arg1){
        sem.Wait();
        if(mProcessState != CMD_PROCESS_START_DONE)
            err = -1;
    }
#endif
#if MULTI_PT_USING_QUEUE
cameraProcessThreadStart_end:
#endif
    LOG_FUNCTION_NAME_EXIT
    return err;
}
//exit display
int ProcessAdapter::stopProcess()
{
    int err = NO_ERROR;
    Message_cam msg;
    Semaphore sem;
    LOG_FUNCTION_NAME
    mProcessLock.lock();
    if (mProcessRuning == STA_PROCESS_STOP) {
        LOGD("%s(%d): process thread is already pause",__FUNCTION__,__LINE__);
        goto cameraProcessThreadPause_end;
    }
    setProcessState(CMD_PROCESS_STOP_PREPARE);

#if MULTI_PT_USING_QUEUE
	mPThreadCmdsQueue.push(CMD_PROCESS_STOP);
	mPCmdsem.Signal();
#else
    msg.command = CMD_PROCESS_STOP ;
    sem.Create();
    msg.arg1 = (void*)(&sem);
    processThreadCommandQ->put(&msg);
#endif
	mProcessCond.signal();

#if !MULTI_PT_USING_QUEUE
cameraProcessThreadPause_end:
#endif
    mProcessLock.unlock();

#if MULTI_PT_USING_QUEUE
	mStateComplete.Wait();
    if(mProcessState != CMD_PROCESS_STOP_DONE)
        err = -1;
#else
    if(msg.arg1){
        sem.Wait();
		//LOGD("SUCCESS WAIT SIG state %d\n", mProcessState);
        if(mProcessState != CMD_PROCESS_STOP_DONE)
            err = -1;
    }
#endif

#if MULTI_PT_USING_QUEUE
cameraProcessThreadPause_end:
#endif
    LOG_FUNCTION_NAME_EXIT
    return err;
}

int ProcessAdapter::pauseProcess()
{
    int err = NO_ERROR;
    Message_cam msg;
    Semaphore sem;
    mProcessLock.lock();
    LOG_FUNCTION_NAME
    if (mProcessRuning == STA_PROCESS_PAUSE) {
        LOGD("%s(%d): display thread is already stop",__FUNCTION__,__LINE__);
        goto cameraProcessThreadStop_end;
    }
    setProcessState(CMD_PROCESS_PAUSE_PREPARE);
	
#if MULTI_PT_USING_QUEUE
	mPThreadCmdsQueue.push(CMD_PROCESS_PAUSE);
	mPCmdsem.Signal();
#else
    msg.command = CMD_PROCESS_PAUSE ;
    sem.Create();
    msg.arg1 = (void*)(&sem);
    processThreadCommandQ->put(&msg);
#endif
	mProcessCond.signal();

#if !MULTI_PT_USING_QUEUE
cameraProcessThreadStop_end:
#endif
    mProcessLock.unlock();

#if MULTI_PT_USING_QUEUE
	mStateComplete.Wait();
    if(mProcessState != CMD_PROCESS_PAUSE_DONE)
        err = -1;
#else
    if(msg.arg1){
        sem.Wait();
		//LOGD("SUCCESS WAIT SIG state %d\n", mProcessState);
        if(mProcessState != CMD_PROCESS_PAUSE_DONE)
            err = -1;
    }
#endif

#if MULTI_PT_USING_QUEUE
cameraProcessThreadStop_end:
#endif
    LOG_FUNCTION_NAME_EXIT
    return err;
}

int ProcessAdapter::getProcessStatus(void)
{
    Mutex::Autolock lock(mProcessLock);
    return mProcessRuning;
}

void ProcessAdapter::setFrameProvider(FrameProvider* framePro)
{
    mFrameProvider = framePro;
}

#if DUMP_CAMERA_DATA
int test_frame_count = 0;
#endif

void ProcessAdapter::processThread()
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

	//LOGD("%s: run success\n", __FUNCTION__);

    LOG_FUNCTION_NAME
    while (mProcessRuning != STA_PROCESS_STOP) {
process_receive_cmd:

#if MULTI_PT_USING_QUEUE
		mPCmdsem.Wait();
        if (mPThreadCmdsQueue.empty() == false ) {
            enum ProcessThreadCommands cur_cmd = mPThreadCmdsQueue.front();
			mPThreadCmdsQueue.pop();
            switch (cur_cmd)
#else
        if (processThreadCommandQ->isEmpty() == false ) {
            processThreadCommandQ->get(&msg);
            switch (msg.command)
            
#endif
			{
                case CMD_PROCESS_START:
                {
                    LOGD("%s(%d): receive CMD_PROCESS_START", __FUNCTION__,__LINE__);
					
                    initSurround3d();
                    mProcessRuning = STA_PROCESS_RUNNING;
                    setProcessState(CMD_PROCESS_START_DONE);
                    
#if MULTI_PT_USING_QUEUE
					mStateComplete.Signal();	
#else
                    if(msg.arg1)
                        ((Semaphore*)msg.arg1)->Signal();
#endif
					break;
                }
                case CMD_PROCESS_SHARE_FD:
                {
                    LOGD("%s(%d): receive CMD_PROCESS_SHARE_FD", __FUNCTION__,__LINE__);
                    if (mProcessRuning != STA_PROCESS_RUNNING)
                        goto process_receive_cmd;
                    setProcessState(CMD_PROCESS_FRAME_SHARE_FD_PREPARE);
#if MULTI_PT_USING_QUEUE
					int camera_id = mPThreadValidCameraID.front();
					mPThreadValidCameraID.pop();
#else
                    int camera_id = (long) msg.arg2;
#endif
					int mFramefd[CONFIG_CAMERA_PREVIEW_BUF_CNT];

                    for(int m = 0; m < CONFIG_CAMERA_PREVIEW_BUF_CNT; m++){
                        LOGD("notifyShareFd GET fd is %d\n", Framefd[camera_id][m]);
                        mFramefd[m] = Framefd[camera_id][m];
                        //memcpy(&Framefd[camera_id][0], &fd[0], sizeof(int) * CONFIG_CAMERA_NUM);
                    }
                    mSurround3d->rk_surround_3d_send_share_fd(mFramefd, camera_id);
                    setProcessState(CMD_PROCESS_FRAME_SHARE_FD_DONE);
					setInternBufferAcc(CONFIG_CAMERA_PREVIEW_BUF_CNT);
#if MULTI_PT_USING_QUEUE
#else
					if(msg.arg1)
                        ((Semaphore*)msg.arg1)->Signal();
#endif
					break;
                }
                case CMD_PROCESS_PAUSE:
                {
                    LOGD("%s(%d): receive CMD_PROCESS_PAUSE", __FUNCTION__,__LINE__);
                    //cameraDisplayBufferDestory();
                    mProcessRuning = STA_PROCESS_PAUSE;
                    setProcessState(CMD_PROCESS_PAUSE_DONE);
#if MULTI_PT_USING_QUEUE
					mStateComplete.Signal();
#else
                    if(msg.arg1)
                        ((Semaphore*)msg.arg1)->Signal();
#endif
                    break;
                }

                case CMD_PROCESS_STOP:
                {
                    LOGD("%s(%d): receive CMD_PROCESS_STOP", __FUNCTION__,__LINE__);
                    //cameraDisplayBufferDestory();
                    deinitSurround3d();
                    mProcessRuning = STA_PROCESS_STOP;
                    setProcessState(CMD_PROCESS_STOP_DONE);
					//LOGD("===== deinitSurround3d success\n");
					
#if MULTI_PT_USING_QUEUE
					mStateComplete.Signal();
#else
                    if(msg.arg1)
                        ((Semaphore*)msg.arg1)->Signal();
#endif
                    continue;
                }
                case CMD_PROCESS_FRAME:
                {
#if !MULTI_PT_USING_QUEUE
					if(msg.arg1)
                        ((Semaphore*)msg.arg1)->Signal();
#endif					
                    if (mProcessRuning != STA_PROCESS_RUNNING)
                        goto process_receive_cmd;
                    setProcessState(CMD_PROCESS_FRAME_PROCESSING);

#if MULTI_PT_USING_QUEUE
					int index = mPThreadValidIndex.front();
					mPThreadValidIndex.pop();
#else
                    int index = (long) msg.arg2;
#endif
                    int dispbuf_index = -1;
                    long dispbuf_phy = 0, dispbuf_vir = 0;
					int disp_fd = -1;
                    unsigned char* mFramesAddr[CONFIG_CAMERA_NUM];
                    int *mmframe_fd = (int*)malloc(CONFIG_CAMERA_NUM * sizeof(int));
                    int *mmframe_index =(int*)malloc(CONFIG_CAMERA_NUM * sizeof(int));
                    for (int i = 0; i < CONFIG_CAMERA_NUM; i++) {
                         LOG2("preview buf %d:0x%x fd [%d]index[%d]", index, mFrames[index][i].vir_addr,
                             mFrames[index][i].drm_fd, mFrames[index][i].frame_index);
                         mFramesAddr[i] = (unsigned char* )mFrames[index][i].vir_addr;
                         mmframe_fd[i] = mFrames[index][i].drm_fd;
                         mmframe_index[i] = mFrames[index][i].frame_index;
                         //printf("preview buf %d:0x%x fd [%d]index[%d] new fd [%d]index[%d]\n", index, mFrames[index][i].vir_addr,
                         //    mFrames[index][i].drm_fd, mFrames[index][i].frame_index,mmframe_fd[i], mmframe_index[i]);
                    }

                    //get a free buffer
                    dispbuf_index = mDisplayAdapter->getOneAvailableBuffer(&dispbuf_phy, &dispbuf_vir, &disp_fd);
                    //LOGD("display buf %d:0x%x", dispbuf_index, dispbuf_vir);
                    if (dispbuf_index == -1 || (getInternBufferShareCount() != CONFIG_CAMERA_NUM * CONFIG_CAMERA_PREVIEW_BUF_CNT)) {
                        LOGE("%s(%d):no available buffer",__FUNCTION__,__LINE__);
                        //return buffers
                        for (int i = 0; i < CONFIG_CAMERA_NUM; i++) {
                            mCameras[i]->getCameraAdapter()->returnFrame(mFrames[index][i].frame_index,mFrames[index][i].used_flag);
                            mReadyCounts[index]--;
                        }

                        free(mmframe_fd);
                        free(mmframe_index);
                        setProcessState(CMD_PROCESS_FRAME_PROCESSING_DONE);
                    } else {
                        mDisplayAdapter->setBufferState(dispbuf_index,1);

                        //to do 360 render
                         struct timeval tpend1, tpend2;
                         long usec0 = 0;

#if DUMP_CAMERA_DATA
						 if(test_frame_count > 1000 && test_frame_count < 1100)
					 	{
			 	    	     FILE* fp =NULL;
						     char filename[40];
						     filename[0] = 0x00;
						     sprintf(filename, "/data/CAMERANV12_%dx%d", 1080, 1920);
						     fp = fopen(filename, "ab+");
						     if (fp) {
						        fwrite((char*)mFramesAddr[2], 1, 1920*1080 * 3 /2,fp);
								fflush(fp);
						        fclose(fp);
						        LOGD("Write success rgb data to %s\n",filename);
						     } else {
						        LOGE("Create %s failed(%d, %s)",filename,fp, strerror(errno));
						     }
					 	}

						test_frame_count++;
#endif
                        gettimeofday(&tpend1, NULL);
                        mSurround3d->rk_surround_3d_set_data_fd(mmframe_fd, mmframe_index, (unsigned char *)&disp_fd);
                        gettimeofday(&tpend2, NULL);
                        usec0 = 1000 * (tpend2.tv_sec - tpend1.tv_sec) + (tpend2.tv_usec - tpend1.tv_usec) / 1000;
                        LOGD("rk-deubg render consume time=%ld ms \n", usec0);

                        if (mDisplayAdapter->isNeedSendToDisplay())
                            mDisplayAdapter->notifyNewFrame(dispbuf_index);
                        //return buffers
                        for (int i = 0; i < CONFIG_CAMERA_NUM; i++) {
                             //printf("return frame camera %d index %d\n", i, mmframe_index[i]);
                             mCameras[i]->getCameraAdapter()->returnFrame(mmframe_index[i], mFrames[index][i].used_flag);
                             mReadyCounts[index]--;
                        }
                        free(mmframe_fd);
                        free(mmframe_index);
                        setProcessState(CMD_PROCESS_FRAME_PROCESSING_DONE);
                    }
                    break;
                }

                default:
                {
                    LOGE("%s(%d): receive unknow command(0x%x)!", __FUNCTION__,__LINE__,msg.command);
                    break;
                }
            }
        }

        mProcessLock.lock();
#if MULTI_PT_USING_QUEUE
        if (mPThreadCmdsQueue.empty()== false ) {
#else
		if (processThreadCommandQ->isEmpty() == false ) {
#endif
            mProcessLock.unlock();
            goto process_receive_cmd;
        }
        LOG2("%s(%d): process thread pause here... ", __FUNCTION__,__LINE__);
        mProcessCond.wait(mProcessLock);
        mProcessLock.unlock();
        LOG2("%s(%d): process thread wake up... ", __FUNCTION__,__LINE__);
        goto process_receive_cmd;

    }
    LOG_FUNCTION_NAME_EXIT
}
} // namespace android