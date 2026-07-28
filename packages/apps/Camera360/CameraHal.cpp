/******************************************************************************
 *
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co., Ltd.
 * Modification based on code covered by the License (the "License").
 * You may not use this software except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED TO YOU ON AN "AS IS" BASIS and ROCKCHIP DISCLAIMS
 * ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE,
 * WHETHER EXPRESS,IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION,
 * ANY IMPLIED WARRANTIES OF TITLE, NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY
 * QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR PURPOSE.
 * Rockchip shall not be liable to make any corrections to this software or to
 * provide any support or assistance with respect to it.
 *
 *****************************************************************************/
/**
* @file CameraHal.cpp
*
* This file maps the Camera Hardware Interface to V4L2.
*
*/

#include "CameraHal.h"
#include <pthread.h>

#include <binder/IPCThreadState.h>

#include <utils/CallStack.h>
#include <binder/IServiceManager.h>
#include <binder/IMemory.h>

#include <sys/stat.h>
#include <unistd.h>
#include <linux/fb.h>


namespace android {

int CameraHal::g_myCamFd[CONFIG_CAMERA_NUM] = {-1, -1, -1, -1};

CameraHal::CameraHal(int cameraId, int dev_id)
{
    char trace_level[PROPERTY_VALUE_MAX];
    int level;

    property_get("sys.camera.camera360_debug", trace_level, "0");

    sscanf(trace_level,"%d",&level);

    setTracerLevel(level);

    mInitState = true;
    mCamId = cameraId;
	mCamDev = dev_id;
    mCamFd = -1;
    mCommandRunning = -1;
    mCameraStatus = 0;

    mVideoBuf = NULL;
    mPreviewBuf = NULL;
    mRawBuf = NULL;
    mJpegBuf = NULL;

    mCamMemManager = new GrallocDrmMemManager(false);
    mPreviewBuf = new BufferProvider(mCamMemManager);

#if MULTI_PT_USING_QUEUE
	mHCmdsem.Create();
   	mStateComplete.Create();
#else
	commandThreadCommandQ = new MessageQueue("commandCmdQ");
#endif	

    LOGD("%s(%d): Camera Hal memory is alloced from GRALLOC device",__FUNCTION__,__LINE__);

    LOGD("it is a soc camera!");
    mCameraAdapter = new CameraSOCAdapter(cameraId, dev_id);
    mCameraAdapter->setPreviewBufProvider(mPreviewBuf);
    if(mCameraAdapter->initialize() < 0){
        mInitState = false;
        LOGE("Error: camera initialize failed.");
        return;
    }

	CameraHal::g_myCamFd[cameraId] = getCameraFd();
    updateParameters(mParameters);

    //command thread
    mCommandThread = new CommandThread(this);
    mCommandThread->run("CameraCmdThread", ANDROID_PRIORITY_URGENT_DISPLAY);

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::updateParameters(CameraParameters & tmpPara)
{
    LOG_FUNCTION_NAME

    tmpPara = mCameraAdapter->getParameters();

    LOG_FUNCTION_NAME_EXIT
}

//release resouse
CameraHal::~CameraHal()
{
    LOG_FUNCTION_NAME
    if(mCameraAdapter){
        delete mCameraAdapter;
        mCameraAdapter = NULL;
    }
	
    if(mPreviewBuf){
        delete mPreviewBuf;
        mPreviewBuf = NULL;
    }

	if(mCamMemManager){
        delete mCamMemManager;
        mCamMemManager =NULL;
    }
	
    if(mVideoBuf){
        delete mVideoBuf;
        mVideoBuf = NULL;
    }
    if(mRawBuf){
        delete mRawBuf;
        mRawBuf = NULL;
    }
    if(mJpegBuf){
        delete mJpegBuf;
        mJpegBuf = NULL;
    }
	
    if(mCommandThread != NULL){
		setCamStatus(CMD_EXIT_PREPARE, 1);

#if MULTI_PT_USING_QUEUE
		mHThreadCmdsQueue.push(CMD_EXIT);
		mHCmdsem.Signal();
		mStateComplete.Wait();
#else
        Message_cam msg;
        Semaphore sem;
        msg.command = CMD_EXIT;
        sem.Create();
        msg.arg1 = (void*)(&sem);
        
        commandThreadCommandQ->put(&msg);
        if(msg.arg1){
            sem.Wait();
        }
#endif		
        if(mCameraStatus&CMD_EXIT_DONE)
            LOG1("exit command OK.");
        mCommandThread->requestExitAndWait();
        mCommandThread.clear();
    }

#if MULTI_PT_USING_QUEUE
	mHCmdsem.Release();
   	mStateComplete.Release();
#else

	if(commandThreadCommandQ)
		delete commandThreadCommandQ;
#endif	

    LOGD("CameraHal destory success");
    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::release()
{
    LOG_FUNCTION_NAME
    Mutex::Autolock lock(mLock);
    LOG_FUNCTION_NAME_EXIT
}

//async
int CameraHal::startPreview()
{
    LOG_FUNCTION_NAME
    Message_cam msg;

    //主线程通过command线程下发命令至preview线程开启视频流
    Mutex::Autolock lock(mLock);
    if ((mCommandThread != NULL)) {
		setCamStatus(CMD_PREVIEW_START_PREPARE, 1);

#if MULTI_PT_USING_QUEUE
		mHThreadCmdsQueue.push(CMD_PREVIEW_START);
		mHCmdsem.Signal();
#else
        msg.command = CMD_PREVIEW_START;
        msg.arg1  = NULL;    
        commandThreadCommandQ->put(&msg);
#endif
    }
   // mPreviewCmdReceived = true;
   	setCamStatus(STA_PREVIEW_CMD_RECEIVED, 1);
    LOG_FUNCTION_NAME_EXIT
    return NO_ERROR ;
}

int CameraHal::bufferPrepare()
{
	 LOG_FUNCTION_NAME
	 Message_cam msg;
	
	 //主线程通过command线程下发命令至preview线程开启视频流
	 Mutex::Autolock lock(mLock);
	 if ((mCommandThread != NULL)) {
	 	 setCamStatus(CMD_PREVIEW_BUFFER_PREPARE, 1);

#if MULTI_PT_USING_QUEUE
		mHThreadCmdsQueue.push(CMD_BUFFER_PREPARE);
		mHCmdsem.Signal();
#else
		msg.command = CMD_BUFFER_PREPARE;
		msg.arg1  = NULL;
		commandThreadCommandQ->put(&msg);
#endif
	 }
	// mPreviewCmdReceived = true;
	setCamStatus(CMD_PREVIEW_BUFFER_PREPARE_DONE, 1);
	LOG_FUNCTION_NAME_EXIT
	return NO_ERROR ;

}

//MUST SYNC
void CameraHal::stopPreview()
{
    LOG_FUNCTION_NAME
    Message_cam msg;
    Semaphore sem;
    Mutex::Autolock lock(mLock);
    if ((mCommandThread != NULL)) {
		setCamStatus(CMD_PREVIEW_STOP_PREPARE, 1);

#if MULTI_PT_USING_QUEUE
		mHThreadCmdsQueue.push(CMD_PREVIEW_STOP);
		mHCmdsem.Signal();
		mStateComplete.Wait();
#else

		msg.command = CMD_PREVIEW_STOP;
        sem.Create();
        msg.arg1 = (void*)(&sem);   
        commandThreadCommandQ->put(&msg);

        if(msg.arg1){
            sem.Wait();
        }
#endif
        if(mCameraStatus&CMD_PREVIEW_STOP_DONE)
            LOGD("stop preview OK.");
    }
    //mPreviewCmdReceived = false;
    setCamStatus(STA_PREVIEW_CMD_RECEIVED, 0);
    LOG_FUNCTION_NAME_EXIT
}


int CameraHal::setParameters(const char* parameters)
{
    CameraParameters params;
    String8 str_params(parameters);

    params.unflatten(str_params);
    return setParameters(params);
}

//sync
int CameraHal::setParameters(const CameraParameters &params_set)
{
    int err = 0;
    LOG_FUNCTION_NAME
    //get preview status
    //if(mPreviewCmdReceived){
    if(mCameraStatus&STA_PREVIEW_CMD_RECEIVED){
        //preview has been started,some parameter can't change

    }

    Message_cam msg;
    Semaphore sem;
    Mutex::Autolock lock(mLock);
    if ((mCommandThread != NULL)) {
		setCamStatus(CMD_SET_PARAMETERS_PREPARE, 1);

#if MULTI_PT_USING_QUEUE
		mHThreadData.push(params_set);
		mHThreadCmdsQueue.push(CMD_PREVIEW_STOP);
		mHCmdsem.Signal();
		mStateComplete.Wait();
#else
        msg.command = CMD_SET_PARAMETERS;
        sem.Create();
        msg.arg1 = (void*)(&sem);
        msg.arg2 = (void*)(&params_set);
        
        commandThreadCommandQ->put(&msg);
        if(msg.arg1){
            sem.Wait();
        }
#endif		
        if(mCameraStatus&CMD_SET_PARAMETERS_DONE){
            LOG1("set parameters OK.");
        }else{
            err = -1;
        }
    }
    LOG_FUNCTION_NAME_EXIT
    return err;
}

char* CameraHal::getParameters()
{
    String8 params_str8;
    char* params_string;
    const char * valstr = NULL;
    CameraParameters mParams = mParameters;

    params_str8 = mParams.flatten();

    // camera service frees this string...
    params_string = (char*) malloc(sizeof(char) * (params_str8.length()+1));
    strcpy(params_string, params_str8.string());

    ///Return the current set of parameters

    return params_string;
}

void CameraHal::putParameters(char *parms)
{
    free(parms);
}

int CameraHal::getCameraFd()
{
    Mutex::Autolock lock(mLock);

    return mCameraAdapter->getCameraFd();
}

int CameraHal::getCameraId()
{

    return mCamId;
}


int CameraHal::selectPreferedDrvSize(int *width,int * height,bool is_capture)
{
    return mCameraAdapter->selectPreferedDrvSize(width,height,is_capture);
}

void CameraHal::commandThread()
{
    Message_cam msg;
    bool  shouldLive = true;
    bool has_message;
    int err = 0,tmp_arg1;
   struct v4l2_control control;
   struct v4l2_queryctrl hdr;
    int prevStatus = -1,drv_w,drv_h,picture_w,picture_h;
    int app_previw_w = 0,app_preview_h = 0;
    bool isRestartPreview = false;
    char prop_value[PROPERTY_VALUE_MAX];
    char res_prop[PROPERTY_VALUE_MAX];
    LOG_FUNCTION_NAME

    while(shouldLive) {
get_command:
        //mCommandRunning = -1;

#if MULTI_PT_USING_QUEUE
		mHCmdsem.Wait();
		enum CommandThreadCommands cur_cmd = mHThreadCmdsQueue.front();
		mHThreadCmdsQueue.pop();
		switch(cur_cmd)
#else
        memset(&msg,0,sizeof(msg));
        commandThreadCommandQ->get(&msg);

        switch(msg.command)
#endif
        {
            case CMD_PREVIEW_START:
            {
                LOGD("%s(%d):receive CMD_PREVIEW_START",__FUNCTION__,__LINE__);

                err = mCameraAdapter->startPreview();

				if(err != 0)
                    goto PREVIEW_START_OUT;
                setCamStatus(CMD_PREVIEW_START_DONE, 1);
				PREVIEW_START_OUT:

#if !MULTI_PT_USING_QUEUE
                if(msg.arg1)
                    ((Semaphore*)(msg.arg1))->Signal();
#endif				
                LOGD("%s(%d): CMD_PREVIEW_START out",__FUNCTION__,__LINE__);
                break;
            }

			case CMD_BUFFER_PREPARE:
			{
			    LOGD("%s(%d):receive CMD_BUFFER_PREPARE",__FUNCTION__,__LINE__);
				RESTART_PREVIEW_INTERNAL:

                //2. current preview status ?
                mParameters.getPreviewSize(&app_previw_w,&app_preview_h);
                prevStatus = mCameraAdapter->getCurPreviewState(&drv_w,&drv_h);
                int prefered_w = app_previw_w, prefered_h = app_preview_h;
                selectPreferedDrvSize(&prefered_w,&prefered_h,false);

                if(prevStatus){
                    //get preview size
                    if((prefered_w != drv_w) || (prefered_h != drv_h)){
                        //stop eventnotify
                        //mEventNotifier->stopReceiveFrame();
                        //need to stop preview.
                        err=mCameraAdapter->stopPreview();
                        if(err != 0)
                            goto PREVIEW_BUFFER_PREPARE_OUT;
                        //set new drv size;
                        drv_w = prefered_w;
                        drv_h = prefered_h;
                        err=mCameraAdapter->startBufferPrepare(app_previw_w,app_preview_h,drv_w, drv_h, 0, false);

                        if(err != 0)
                            goto PREVIEW_BUFFER_PREPARE_OUT;
                    }

                }else{

                    drv_w = prefered_w;
                    drv_h = prefered_h;
                    //selet a proper preview size.
                    err=mCameraAdapter->startBufferPrepare(app_previw_w,app_preview_h,drv_w, drv_h, 0, false);
                    if(err != 0)
                        goto PREVIEW_BUFFER_PREPARE_OUT;

                }

                setCamStatus(CMD_PREVIEW_BUFFER_PREPARE_DONE, 1);
                PREVIEW_BUFFER_PREPARE_OUT:
#if !MULTI_PT_USING_QUEUE
				if(msg.arg1)
                    ((Semaphore*)(msg.arg1))->Signal();
#endif				
                LOGD("%s(%d): PREVIEW_BUFFER_PREPARE_OUT out",__FUNCTION__,__LINE__);
				break;
			}

            case CMD_PREVIEW_STOP:
            {
                LOGD("%s(%d):receive CMD_PREVIEW_STOP",__FUNCTION__,__LINE__);
                //stop eventnotify
                //mEventNotifier->stopReceiveFrame();
                //stop preview
                mCameraAdapter->setThreadResume();
                err=mCameraAdapter->stopPreview();
                if(err != 0)
                    goto CMD_PREVIEW_STOP_OUT;
                //wake up wait thread.

                setCamStatus(CMD_PREVIEW_STOP_DONE, 1);
        CMD_PREVIEW_STOP_OUT:
#if !MULTI_PT_USING_QUEUE
				if(msg.arg1)
					((Semaphore*)(msg.arg1))->Signal();
#else
				mStateComplete.Signal();	
#endif	

                LOGD("%s(%d): CMD_PREVIEW_STOP out",__FUNCTION__,__LINE__);
                break;
            }
            case CMD_SET_PREVIEW_WINDOW:
            {
                LOGD("%s(%d):receive CMD_SET_PREVIEW_WINDOW",__FUNCTION__,__LINE__);

                setCamStatus(CMD_SET_PREVIEW_WINDOW_DONE, 1);
                if(msg.arg1)
                    ((Semaphore*)(msg.arg1))->Signal();
                LOGD("%s(%d): CMD_SET_PREVIEW_WINDOW out",__FUNCTION__,__LINE__);
                break;
            }
            case CMD_SET_PARAMETERS:
            {
                LOG1("%s(%d): receive CMD_SET_PARAMETERS", __FUNCTION__,__LINE__);
                //set parameters
                CameraParameters* para = (CameraParameters*)msg.arg2;
                if(mCameraAdapter->setParameters(*para,isRestartPreview) == 0){
                    //update parameters
                    updateParameters(mParameters);
                    setCamStatus(CMD_SET_PARAMETERS_DONE, 1);
                }else{
                    //update parameters
                    updateParameters(mParameters);
                }
#if !MULTI_PT_USING_QUEUE
				if(msg.arg1)
					((Semaphore*)(msg.arg1))->Signal();
#else
				mStateComplete.Signal();	
#endif	

                LOG1("%s(%d): CMD_SET_PARAMETERS out",__FUNCTION__,__LINE__);
                if((isRestartPreview) && (mCameraStatus & STA_PREVIEW_CMD_RECEIVED)) {
                    LOGD("%s:setparameter demand restart preview!",__FUNCTION__);
#if !MULTI_PT_USING_QUEUE					
                    msg.arg1 = NULL;
#endif
                    goto RESTART_PREVIEW_INTERNAL;
                }
                break;

            }
            case CMD_PREVIEW_CAPTURE_CANCEL:
            {
                LOGD("%s(%d): receive CMD_PREVIEW_CAPTURE_CANCEL", __FUNCTION__,__LINE__);
                //mEventNotifier->flushPicture();
                setCamStatus(CMD_PREVIEW_CAPTURE_CANCEL_DONE, 1);
                //reset pic num to 1
                mParameters.set(KEY_CONTINUOUS_PIC_NUM,"1");
                mCameraAdapter->setParameters(mParameters,isRestartPreview);
                if(msg.arg1)
                    ((Semaphore*)(msg.arg1))->Signal();
                LOGD("%s(%d): CMD_PREVIEW_CAPTURE_CANCEL out",__FUNCTION__,__LINE__);
                break;
            }
            case CMD_CONTINUOS_PICTURE:
             {
                LOGD("%s(%d): receive CMD_CONTINUOS_PICTURE", __FUNCTION__,__LINE__);

                setCamStatus(CMD_CONTINUOS_PICTURE_DONE, 1);
            CMD_CONTINUOS_PICTURE_OUT:
                if(msg.arg1)
                    ((Semaphore*)(msg.arg1))->Signal();
                LOGD("%s(%d): CMD_CONTINUOS_PICTURE out",__FUNCTION__,__LINE__);
                break;
            }
            case CMD_AF_START:
            {
                LOGD("%s(%d): receive CMD_AF_START", __FUNCTION__,__LINE__);
                mCameraAdapter->autoFocus();

                setCamStatus(CMD_AF_START_DONE, 1);
                if(msg.arg1)
                    ((Semaphore*)(msg.arg1))->Signal();
                LOGD("%s(%d): exit receive CMD_AF_START", __FUNCTION__,__LINE__);
                break;
            }
            case CMD_AF_CANCEL:
            {
                LOGD("%s(%d): receive CMD_AF_CANCEL", __FUNCTION__,__LINE__);
                mCameraAdapter->cancelAutoFocus();
                setCamStatus(CMD_AF_CANCEL_DONE, 1);
                if(msg.arg1)
                    ((Semaphore*)(msg.arg1))->Signal();
                break;
            }
            case CMD_START_FACE_DETECTION:
                LOGD("%s(%d): receive CMD_AF_CANCEL", __FUNCTION__,__LINE__);

                break;
            case CMD_EXIT:
            {
                LOGD("%s(%d): receive CMD_EXIT", __FUNCTION__,__LINE__);
                shouldLive = false;

                setCamStatus(CMD_EXIT_DONE, 1);

#if MULTI_PT_USING_QUEUE
				mStateComplete.Signal();
#else
                if(msg.arg1)
                    ((Semaphore*)(msg.arg1))->Signal();
#endif
                break;
            }
            default:
                LOGE("%s(%d) receive unknow command(0x%x)\n", __FUNCTION__,__LINE__,msg.command);
                break;
        }
    }

    LOG_FUNCTION_NAME_EXIT
    return;
}

int CameraHal::checkCamStatus(int cmd)
{
    int err = 0;
    switch(cmd)
    {
        case CMD_PREVIEW_START:
        {
            break;
        }
        case CMD_PREVIEW_STOP:
        {
            if((mCameraStatus&STA_DISPLAY_STOP))
                err = -1;
            break;
        }
        case CMD_SET_PREVIEW_WINDOW:
        {
            if((mCameraStatus&STA_DISPLAY_RUNNING) || (mCameraStatus&STA_RECORD_RUNNING))
                err = -1;
            break;
        }
        case CMD_SET_PARAMETERS:
        {
            break;
        }
        case CMD_PREVIEW_CAPTURE_CANCEL:
        {
            break;
        }
        case CMD_CONTINUOS_PICTURE:
        {
            if((mCameraStatus&STA_DISPLAY_RUNNING)==0x0)
                err = -1;
            break;
        }
        case CMD_AF_START:
        {
            break;
        }
        case CMD_AF_CANCEL:
        {
            break;
        }
    }


    return err;
}
//type:1-set, 0-clear
void CameraHal::setCamStatus(int status, int type)
{
    if(type)
    {
        if(status&CMD_PREVIEW_START_PREPARE)
        {
            mCameraStatus &= ~CMD_PREVIEW_START_MASK;
            mCameraStatus |= CMD_PREVIEW_START_PREPARE;
        }
        if(status&CMD_PREVIEW_START_DONE)
        {
            mCameraStatus &= ~CMD_PREVIEW_START_MASK;
            mCameraStatus |= CMD_PREVIEW_START_DONE;
        }
        if(status&CMD_PREVIEW_STOP_PREPARE)
        {
            mCameraStatus &= ~CMD_PREVIEW_STOP_MASK;
            mCameraStatus |= CMD_PREVIEW_STOP_PREPARE;
        }
        if(status&CMD_PREVIEW_STOP_DONE)
        {
            mCameraStatus &= ~CMD_PREVIEW_STOP_MASK;
            mCameraStatus |= CMD_PREVIEW_STOP_DONE;
        }
        if(status&CMD_SET_PREVIEW_WINDOW_PREPARE)
        {
            mCameraStatus &= ~CMD_SET_PREVIEW_WINDOW_MASK;
            mCameraStatus |= CMD_SET_PREVIEW_WINDOW_PREPARE;
        }
        if(status&CMD_SET_PREVIEW_WINDOW_DONE)
        {
            mCameraStatus &= ~CMD_SET_PREVIEW_WINDOW_MASK;
            mCameraStatus |= CMD_SET_PREVIEW_WINDOW_DONE;
        }

        if(status&CMD_SET_PARAMETERS_PREPARE)
        {
            mCameraStatus &= ~CMD_SET_PARAMETERS_MASK;
            mCameraStatus |= CMD_SET_PARAMETERS_PREPARE;
        }
        if(status&CMD_SET_PARAMETERS_DONE)
        {
            mCameraStatus &= ~CMD_SET_PARAMETERS_MASK;
            mCameraStatus |= CMD_SET_PARAMETERS_DONE;
        }

        if(status&CMD_PREVIEW_CAPTURE_CANCEL_PREPARE)
        {
            mCameraStatus &= ~CMD_PREVIEW_CAPTURE_CANCEL_MASK;
            mCameraStatus |= CMD_PREVIEW_CAPTURE_CANCEL_PREPARE;
        }
        if(status&CMD_PREVIEW_CAPTURE_CANCEL_DONE)
        {
            mCameraStatus &= ~CMD_PREVIEW_CAPTURE_CANCEL_MASK;
            mCameraStatus |= CMD_PREVIEW_CAPTURE_CANCEL_DONE;
        }

        if(status&CMD_CONTINUOS_PICTURE_PREPARE)
        {
            mCameraStatus &= ~CMD_CONTINUOS_PICTURE_MASK;
            mCameraStatus |= CMD_CONTINUOS_PICTURE_PREPARE;
        }
        if(status&CMD_CONTINUOS_PICTURE_DONE)
        {
            mCameraStatus &= ~CMD_CONTINUOS_PICTURE_MASK;
            mCameraStatus |= CMD_CONTINUOS_PICTURE_DONE;
        }

        if(status&CMD_AF_START_PREPARE)
        {
            mCameraStatus &= ~CMD_AF_START_MASK;
            mCameraStatus |= CMD_AF_START_PREPARE;
        }
        if(status&CMD_AF_START_DONE)
        {
            mCameraStatus &= ~CMD_AF_START_MASK;
            mCameraStatus |= CMD_AF_START_DONE;
        }

        if(status&CMD_AF_CANCEL_PREPARE)
        {
            mCameraStatus &= ~CMD_AF_CANCEL_MASK;
            mCameraStatus |= CMD_AF_CANCEL_PREPARE;
        }
        if(status&CMD_AF_CANCEL_DONE)
        {
            mCameraStatus &= ~CMD_AF_CANCEL_MASK;
            mCameraStatus |= CMD_AF_CANCEL_DONE;
        }

        if(status&CMD_EXIT_PREPARE)
        {
            mCameraStatus &= ~CMD_EXIT_MASK;
            mCameraStatus |= CMD_EXIT_PREPARE;
        }
        if(status&CMD_EXIT_DONE)
        {
            mCameraStatus &= ~CMD_EXIT_MASK;
            mCameraStatus |= CMD_EXIT_DONE;
        }

        if(status&STA_RECORD_RUNNING)
        {
            mCameraStatus |= STA_RECORD_RUNNING;
        }

        if(status&STA_PREVIEW_CMD_RECEIVED)
        {
            mCameraStatus |= STA_PREVIEW_CMD_RECEIVED;
        }

        if(status&STA_DISPLAY_RUNNING)
        {
            mCameraStatus &= ~STA_DISPLAY_MASK;
            mCameraStatus |= STA_DISPLAY_RUNNING;
        }
        if(status&STA_DISPLAY_PAUSE)
        {
            mCameraStatus &= ~STA_DISPLAY_MASK;
            mCameraStatus |= STA_DISPLAY_PAUSE;
        }
        if(status&STA_DISPLAY_STOP)
        {
            mCameraStatus &= ~STA_DISPLAY_MASK;
            mCameraStatus |= STA_DISPLAY_STOP;
        }
    }
    else
    {
        if(status&STA_RECORD_RUNNING)
        {
            mCameraStatus &= ~STA_RECORD_RUNNING;
        }

        if(status&STA_PREVIEW_CMD_RECEIVED)
        {
            mCameraStatus &= ~STA_PREVIEW_CMD_RECEIVED;
        }
    }
}
}; // namespace android

