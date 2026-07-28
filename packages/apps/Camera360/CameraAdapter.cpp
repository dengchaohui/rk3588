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
namespace android{
    static int xioctl(int fh, int request, void *arg)
    {
        int r;
        int retry = 20;
        do {
            r = ioctl(fh, request, arg);
            retry--;
            if(retry == 0)
                break;
        } while (-1 == r && EINTR == errno);
        return r;
    }


CameraAdapter::CameraAdapter(int cameraId, int dev_id):mPreviewRunning(0),
                                           mCamId(cameraId), mCamDevId(dev_id)
{
    LOG_FUNCTION_NAME
    mCameraPreviewThread = NULL;
	mAutoFocusThread = NULL;
	mEncBufProvider = NULL;
    mPreviewRunning = 0;
    mPictureRunning = 0;
    mPreviewBufProvider = NULL;
    mCamDrvWidth = 0;
    mCamDrvHeight = 0;
    mVideoWidth = 0;
    mVideoHeight = 0;
    mCamDriverStream = false;
    camera_device_error = false;
    mPreviewFrameIndex = 0;
    mPreviewErrorFrameCount = 0;
    mCamFd = -1;
    mCamDriverPreviewFmt = 0;
    mZoomVal = 100;
    mCurFrameCount = 0;
    mLastFrameCount = 0;
    mLastFpsTime = 0;

    CameraHal_SupportFmt[0] = V4L2_PIX_FMT_NV12;
    CameraHal_SupportFmt[1] = V4L2_PIX_FMT_NV16;
    CameraHal_SupportFmt[2] = V4L2_PIX_FMT_YUYV;
    CameraHal_SupportFmt[3] = V4L2_PIX_FMT_RGB565;
    CameraHal_SupportFmt[4] = 0x00;
    CameraHal_SupportFmt[5] = 0x00;
    memset(mCamDriverSupportFmt, 0, sizeof(mCamDriverSupportFmt));
    cif_driver_iommu = false;

    char dump_type[PROPERTY_VALUE_MAX];
    int type;

    property_get("sys.camera.camera360_dump", dump_type, "0");

    sscanf(dump_type,"%d",&type);

    if (type & DUMP_CAMERA)
        mDumpFrame = true;
    else
        mDumpFrame = false;
   LOG_FUNCTION_NAME_EXIT
}

int CameraAdapter::getV4lMemtype(){return mCamDriverV4l2MemType;}


int CameraAdapter::initialize()
{
    int ret = -1;
    //create focus thread
    LOG_FUNCTION_NAME
    
    if((ret = cameraCreate(mCamId)) < 0)
        return ret;

    /*注意指针，应用中虽然指针是CameraAdapter 但是new对象是Soc,实际调用Soc initDefaultParameters(int camFd)*/
    initDefaultParameters(mCamId);
    LOG_FUNCTION_NAME_EXIT
    return ret;
}

CameraAdapter::~CameraAdapter()
{
    LOG_FUNCTION_NAME

    if(mAutoFocusThread != NULL){
        mAutoFocusLock.lock();
        mExitAutoFocusThread = true;
        mAutoFocusLock.unlock();
        mAutoFocusCond.signal();
        mAutoFocusThread->requestExitAndWait();
        mAutoFocusThread.clear();
        mAutoFocusThread = NULL;
    }

    if(mEncBufProvider)
        mEncBufProvider->freeBuffer();

    this->cameraDestroy();
    LOG_FUNCTION_NAME_EXIT
}

void CameraAdapter::setPreviewBufProvider(BufferProvider* bufprovider)
{
    mPreviewBufProvider = bufprovider;
}

void CameraAdapter::setEncBufProvider(BufferProvider* bufprovider)
{
    mEncBufProvider = bufprovider;
}

CameraParameters & CameraAdapter::getParameters()
{
    return mParameters;
}

int CameraAdapter::getCurPreviewState(int *drv_w,int *drv_h)
{
    *drv_w = mCamDrvWidth;
    *drv_h = mCamDrvHeight;
    return mPreviewRunning;
}

int CameraAdapter::getCurVideoSize(int *video_w, int *video_h)
{
    *video_w = mVideoWidth;
    *video_h = mVideoHeight;
    return mPreviewRunning;

}

bool CameraAdapter::isNeedToRestartPreview()
{
    int preferPreviewW=0,preferPreviewH=0;
    int previewFrame2AppW=0,previewFrame2AppH=0;
    bool ret = false;
    char prop_value[PROPERTY_VALUE_MAX];

    mParameters.getPreviewSize(&previewFrame2AppW, &previewFrame2AppH);
    //case 1: when setVideoSize is suported

    if(mPreviewRunning && (mParameters.get(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES) != NULL)) {
        mParameters.getVideoSize(&mVideoWidth,&mVideoHeight);
    }else{
        mVideoWidth = -1;
        mVideoHeight = -1;
    }

    if(previewFrame2AppW >= mVideoWidth){
        preferPreviewW = previewFrame2AppW;
        preferPreviewH = previewFrame2AppH;
    } else {
        preferPreviewW = mVideoWidth;
        preferPreviewH = mVideoHeight;
    }
    //not support setvideosize
    if(mVideoWidth == -1){
        mVideoWidth = preferPreviewW;
        mVideoHeight = preferPreviewH;
    }

    LOGD("mPreviewFrame2AppW (%dx%d)",previewFrame2AppW,previewFrame2AppH);
    LOGD("mCamPreviewW (%dx%d)",mCamPreviewW,mCamPreviewH);
    LOGD("video width (%dx%d)",mVideoWidth,mVideoHeight);

    if(mPreviewRunning && ((preferPreviewW != mCamPreviewW) || (preferPreviewH != mCamPreviewH)) && (mVideoWidth != -1))
    {
      ret = true;
    }
    return ret;
}

void CameraAdapter::dump(int cameraId)
{
    LOG2("%s CameraAdapter dump cameraId(%d)\n", __FUNCTION__,cameraId);

}

bool CameraAdapter::getFlashStatus()
{
    return false;
}

status_t CameraAdapter::startBufferPrepare(int preview_w,int preview_h,int w, int h, int fmt,bool is_capture)
{
    LOG_FUNCTION_NAME
	unsigned int frame_size = 0,i;
	struct bufferinfo_s previewbuf;
	int ret = 0,buf_count = CONFIG_CAMERA_PREVIEW_BUF_CNT;
	LOGD("%s%d:preview_w = %d,preview_h = %d,drv_w = %d,drv_h = %d",__FUNCTION__,__LINE__,preview_w,preview_h,w,h);
	mPreviewFrameIndex = 0;
	mPreviewErrorFrameCount = 0;
	switch (mCamDriverPreviewFmt)
	{
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_YUV420:
			frame_size = ((w+15)&(~15))*((h+15)&(~15))*3/2;
			break;
		case V4L2_PIX_FMT_NV16:
		case V4L2_PIX_FMT_YUV422P:
		default:
			frame_size = w*h*2;
			break;
	}

	//for test capture
	if(is_capture){
		buf_count = 1;
	}
	if(mPreviewBufProvider->createBuffer(buf_count,w, h, frame_size,PREVIEWBUFFER,false) < 0)
	{
		LOGE("%s%d:create preview buffer failed",__FUNCTION__,__LINE__);
		return -1;
	}

	int sFd[CONFIG_CAMERA_PREVIEW_BUF_CNT];

	for(int m = 0; m < CONFIG_CAMERA_PREVIEW_BUF_CNT; m++)
	{
		sFd[m] = mPreviewBufProvider->getBufShareFd(m);
		LOGD("%s(%d): mCamID %d get share fd %d\n", __FUNCTION__, __LINE__, mCamId, sFd[m]);
	}

	mRefProcessAdapter->notifyShareFd(sFd, mCamId, NULL);
	
	//set size
	if(cameraSetSize(w, h, mCamDriverPreviewFmt,is_capture)<0){
		ret = -1;
		goto start_preview_end;
	}
	
	mCamDrvWidth = w;
	mCamDrvHeight = h;
	mCamPreviewH = preview_h;
	mCamPreviewW = preview_w;

	memset(mPreviewFrameInfos,0,sizeof(mPreviewFrameInfos));

	LOGD("CREATE IMAGE TEXTURE SUCCESS, need START CAMERA \n");

	return 0;
	start_preview_end:
	mCamDrvWidth = 0;
	mCamDrvHeight = 0;
	mCamPreviewH = 0;
	mCamPreviewW = 0;
	return ret;	

}


status_t CameraAdapter::startPreview()
{
	int ret = 0;

    //camera start
    if(cameraStart() < 0){
        ret = -1;
        goto start_preview_end;
    }
    //new preview thread
    mCameraPreviewThread = new CameraPreviewThread(this);
    mCameraPreviewThread->run("CameraPreviewThread",ANDROID_PRIORITY_DISPLAY);

    mPreviewRunning = 1;
    LOGD("%s(%d):OUT",__FUNCTION__,__LINE__);
    return 0;
start_preview_end:
    return ret;
}

status_t CameraAdapter::stopPreview()
{
    LOGD("%s(%d):IN",__FUNCTION__,__LINE__);
    if(mPreviewRunning == 1){
        mPreviewRunning = 0;

	    //quit preview thread
        if(mCameraPreviewThread != NULL){
            LOGD("%s(%d):%d requestExitAndWait",__FUNCTION__,__LINE__, mCamId);
            mCameraPreviewThread->requestExitAndWait();
            LOGD("%s(%d):%d clear",__FUNCTION__,__LINE__, mCamId);
            mCameraPreviewThread.clear();

            mCameraPreviewThread = NULL;
        }
	
		//camera stop
        int err = cameraStream(false);
		
		if (mCamFd > 0) {
			close(mCamFd);
			mCamFd = -1;
			CameraHal::g_myCamFd[mCamId] = -1;
		}

        //cameraStop();
        //destroy preview buffer
        if(mPreviewBufProvider)
            mPreviewBufProvider->freeBuffer();
    }
	
    mCamDrvWidth = 0;
    mCamDrvHeight = 0;
    LOGD("%s(%d):OUT",__FUNCTION__,__LINE__);
    return 0;
}

int CameraAdapter::setParameters(const CameraParameters &params_set,bool &isRestartValue)
{

    return 0;
}

void CameraAdapter::initDefaultParameters(int camFd)
{

}

status_t CameraAdapter::autoFocus()
{

    mAutoFocusCond.signal();
    return 0;
}

status_t CameraAdapter::cancelAutoFocus()
{
    return 0;
}

int CameraAdapter::getCameraFd()
{
    return mCamFd;
}

int CameraAdapter::flashcontrol()
{
    return 0;
}

//talk to driver
//open camera
//static int global_id_offset = 11;
int CameraAdapter::cameraCreate(int cameraId)
{
    int err = 0,iCamFd;
    int pmem_fd,i,j,cameraCnt;
    char cam_path[20];
    char cam_num[3];
    char *ptr_tmp;
    struct v4l2_fmtdesc fmtdesc;
    char cameraDevicePathCur[20];
    char decode_name[50];

    LOG_FUNCTION_NAME

    memset(cameraDevicePathCur,0x00, sizeof(cameraDevicePathCur));
    sprintf(cameraDevicePathCur,"/dev/video%d", mCamDevId);
    iCamFd = open(cameraDevicePathCur, O_RDWR);
    if (iCamFd < 0) {
        LOGE("%s(%d): open camera%d(%s) dev(%d) is failed ,err: %s",__FUNCTION__,__LINE__,cameraId,mCamDevId,cameraDevicePathCur,strerror(errno));
        goto exit;
    }else
		LOGE("%s(%d): cur camera [%d %d]", __FUNCTION__, __LINE__, cameraId, iCamFd);
	
    memset(&mCamDriverCapability, 0, sizeof(struct v4l2_capability));
    err = xioctl(iCamFd, VIDIOC_QUERYCAP, &mCamDriverCapability);
    if (err < 0) {
        LOGE("%s(%d): %s query device's capability failed , err: %s \n",__FUNCTION__,__LINE__,cameraDevicePathCur,strerror(errno));
        goto exit1;
    }
    mCapture_type = mCamDriverCapability.device_caps;
    LOGD("Camera driver: %s Card Name:%s Driver version: 0x%x.0x%x.0x%x  ",mCamDriverCapability.driver,
        mCamDriverCapability.card, (mCamDriverCapability.version>>16) & 0xff,(mCamDriverCapability.version>>8) & 0xff,
        mCamDriverCapability.version & 0xff);

    memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.index = 0;
    LOGD("mCamDriverCapability.device_caps:%d", mCamDriverCapability.device_caps);
    if (mCamDriverCapability.device_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE){
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		LOGD(">>>>>>>>>>> mCamDriverCapability.device_caps:%d", mCamDriverCapability.device_caps);
    }else{
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }

	
    while (xioctl(iCamFd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        mCamDriverSupportFmt[fmtdesc.index] = fmtdesc.pixelformat;
        LOGD("mCamDriverSupportFmt: fmt = %d(%s),index = %d",fmtdesc.pixelformat,fmtdesc.description,fmtdesc.index);

        fmtdesc.index++;
    }

    i = 0;
    while (CameraHal_SupportFmt[i]) {
        LOGD("CameraHal_SupportFmt:fmt = %d,index = %d",CameraHal_SupportFmt[i],i);
        j = 0;
        while (mCamDriverSupportFmt[j]) {
            if (mCamDriverSupportFmt[j] == CameraHal_SupportFmt[i]) {
                break;
            }
            j++;
        }
        if (mCamDriverSupportFmt[j] == CameraHal_SupportFmt[i]) {
            break;
        }
        i++;
    }

    if (CameraHal_SupportFmt[i] == 0x00) {
        LOGE("%s(%d): all camera driver support format is not supported in CameraHal!!",__FUNCTION__,__LINE__);
        j = 0;
        while (mCamDriverSupportFmt[j]) {
            LOG1("pixelformat = '%c%c%c%c'",
                mCamDriverSupportFmt[j] & 0xFF, (mCamDriverSupportFmt[j] >> 8) & 0xFF,
                (mCamDriverSupportFmt[j] >> 16) & 0xFF, (mCamDriverSupportFmt[j] >> 24) & 0xFF);
            j++;
        }
        goto exit1;
    } else {
        mCamDriverPreviewFmt = CameraHal_SupportFmt[i];
        LOGD("%s(%d): mCamDriverPreviewFmt(%c%c%c%c) is cameraHal and camera driver is also supported!!",__FUNCTION__,__LINE__,
            mCamDriverPreviewFmt & 0xFF, (mCamDriverPreviewFmt >> 8) & 0xFF,
            (mCamDriverPreviewFmt >> 16) & 0xFF, (mCamDriverPreviewFmt >> 24) & 0xFF);

        LOGD("mCamDriverPreviewFmt  = %d",mCamDriverPreviewFmt);

    }

    LOGD("%s(%d): Current driver is %s, v4l2 memory is %s %d\n",__FUNCTION__,__LINE__,mCamDriverCapability.driver,
        (mCamDriverV4l2MemType==V4L2_MEMORY_MMAP)?"V4L2_MEMORY_MMAP":"V4L2_MEMORY_OVERLAY", mCamDriverV4l2MemType);
    mCamFd = iCamFd;

    //create focus thread for soc or usb camera.
    //mAutoFocusThread = new AutoFocusThread(this);
    //mAutoFocusThread->run("AutoFocusThread", ANDROID_PRIORITY_URGENT_DISPLAY);
    //mExitAutoFocusThread = false;


    LOG_FUNCTION_NAME_EXIT
    return 0;

exit1:
    if (iCamFd > 0) {
        close(iCamFd);
        iCamFd = -1;
    }

exit:
    LOGE("%s(%d): exit with error -1",__FUNCTION__,__LINE__);
    return -1;
}

int CameraAdapter::cameraDestroy()
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT
    return 0;
}

int CameraAdapter::cameraSetSize(int w, int h, int fmt, bool is_capture)
{
    int err=0;
    struct v4l2_format format;

    /* Set preview format */
    if (mCapture_type & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    else
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = w;
    format.fmt.pix.height = h;
    format.fmt.pix.pixelformat = fmt;
    format.fmt.pix.field = V4L2_FIELD_NONE;        /* ddl@rock-chips.com : field must be initialised for Linux kernel in 2.6.32  */
    LOGD("%s(%d):IN, w = %d,h = %d",__FUNCTION__,__LINE__,w,h);
    if (is_capture) {                           /* ddl@rock-chips.com: v0.4.1 add capture and preview check */
        format.fmt.pix.priv = 0xfefe5a5a;
    } else {
        format.fmt.pix.priv = 0x5a5afefe;
    }

    err = xioctl(mCamFd, VIDIOC_S_FMT, &format);
    if ( err < 0 ){
        LOGE("%s(%d): VIDIOC_S_FMT failed ,err: %s",__FUNCTION__,__LINE__,strerror(errno));
    } else {
        LOG1("%s(%d): VIDIOC_S_FMT %dx%d '%c%c%c%c'",__FUNCTION__,__LINE__,format.fmt.pix.width, format.fmt.pix.height,
                fmt & 0xFF, (fmt >> 8) & 0xFF,(fmt >> 16) & 0xFF, (fmt >> 24) & 0xFF);
    }

    mCurBufLength = format.fmt.pix.sizeimage;

    return err;
}

int CameraAdapter::cameraStream(bool on)
{
    int err = 0;
    int cmd ;
    enum v4l2_buf_type type;
    
    if (mCapture_type & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    else
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cmd = (on)?VIDIOC_STREAMON:VIDIOC_STREAMOFF;

    //mCamDriverStreamLock.lock();
    err = xioctl(mCamFd, cmd, &type);
    if (err < 0) {
        LOGE("%s(%d): %s Failed ,err: %s",__FUNCTION__,__LINE__,((on)?"VIDIOC_STREAMON":"VIDIOC_STREAMOFF"),strerror(errno));
        goto cameraStream_end;
    }
    mCamDriverStream = on;
	LOGD("%s(%d):on = %d",__FUNCTION__,__LINE__,on);

cameraStream_end:
    //mCamDriverStreamLock.unlock();
    return err;
}

// query buffer ,qbuf , stream on
int CameraAdapter::cameraStart()
{
    int preview_size,i;
    int err;
    int nSizeBytes;
    int buffer_count;
    struct v4l2_format format;
    enum v4l2_buf_type type;
    struct v4l2_requestbuffers creqbuf;
    struct v4l2_buffer buffer;
    CameraParameters  tmpparams;

    LOG_FUNCTION_NAME

    buffer_count = mPreviewBufProvider->getBufCount();
    if (mCapture_type & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    else
        creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    creqbuf.memory = mCamDriverV4l2MemType;
    creqbuf.count  = buffer_count;

	if (xioctl(mCamFd, VIDIOC_REQBUFS, &creqbuf) < 0) {
        LOGE ("%s(%d): VIDIOC_REQBUFS Failed. %s",__FUNCTION__,__LINE__, strerror(errno));
        goto fail_reqbufs;
    }

    if (buffer_count == 0) {
        LOGE("%s(%d): preview buffer havn't alloced",__FUNCTION__,__LINE__);
        goto fail_reqbufs;
    }
    memset(mCamDriverV4l2Buffer, 0x00, sizeof(mCamDriverV4l2Buffer));
    for (int i = 0; i < buffer_count; i++) {
        struct v4l2_plane vplanes[1];
        memset(&buffer, 0, sizeof(struct v4l2_buffer));
        buffer.type = creqbuf.type;
        buffer.memory = creqbuf.memory;
        //buffer.flags = 0;
        buffer.index = i;

        buffer.m.planes = vplanes;
        buffer.length = 1;
        if (xioctl(mCamFd, VIDIOC_QUERYBUF, &buffer) < 0) {
            LOGE("%s(%d): VIDIOC_QUERYBUF Failed ,err: %s",__FUNCTION__,__LINE__,strerror(errno));
            goto fail_bufalloc;
        }

        //LOGD("QUERY BUFFER LENGTH %d, palne length %d, byteused %d pane used %d\n",
        //    buffer.length, buffer.m.planes[0].length, buffer.bytesused, buffer.m.planes[0].bytesused);

        if (buffer.memory == V4L2_MEMORY_OVERLAY) {

            buffer.m.offset = mPreviewBufProvider->getBufPhyAddr(i);
            mCamDriverV4l2Buffer[i] = (char*)mPreviewBufProvider->getBufVirAddr(i);
        } else if (buffer.memory == V4L2_MEMORY_MMAP) {

        }
        else if(buffer.memory == V4L2_MEMORY_DMABUF)
        {
            buffer.m.fd = mPreviewBufProvider->getBufShareFd(i);
            //LOGD("mCamID %d fd is %d, buffer.m.fd is %d \n",mCamId, mPreviewBufProvider->getBufShareFd(i), buffer.m.fd);
            if (!V4L2_TYPE_IS_MULTIPLANAR(buffer.type))
                buffer.length = mCurBufLength;
        }

        if (V4L2_TYPE_IS_MULTIPLANAR(buffer.type)) {

            vplanes[0].m.fd = mPreviewBufProvider->getBufShareFd(i);
            //vplanes[0].length = mCurBufLength;
            //LOGD("mCamID %d fd is %d, vplanes.m.fd is %d \n",mCamId, mPreviewBufProvider->getBufShareFd(i), vplanes[0].m.fd);
            buffer.m.planes = vplanes;
            buffer.length = 1;
        }

        mCamDriverV4l2BufferLen = mCapture_type & V4L2_CAP_VIDEO_CAPTURE_MPLANE ? buffer.m.planes[0].length : buffer.bytesused;
       // LOGD("v4l2 buffer length:%d, byteused:%d, plane length:%d, byteused %d\n", buffer.length, buffer.bytesused, buffer.m.planes[0].length,
       //      buffer.m.planes[0].bytesused);

        mPreviewBufProvider->setBufferStatus(i, 1,PreviewBufferProvider::CMD_PREVIEWBUF_WRITING);
        err = xioctl(mCamFd, VIDIOC_QBUF, &buffer);
        if (err < 0) {
            LOGE("%s(%d): VIDIOC_QBUF Failed,err=%d[%s], cur fd %d, bfd %d, addr %p cameraID %d\n",__FUNCTION__,__LINE__,err, strerror(errno),
                mPreviewBufProvider->getBufShareFd(i), buffer.m.fd, mPreviewBufProvider->getBufVirAddr(i), mCamId);
            mPreviewBufProvider->setBufferStatus(i, 0,PreviewBufferProvider::CMD_PREVIEWBUF_WRITING);
            goto fail_bufalloc;
        }
    }


    mPreviewErrorFrameCount = 0;
    mPreviewFrameIndex = 0;
    cameraStream(true);
	isThreadNeedtoRun = true;
    LOG_FUNCTION_NAME_EXIT
    return 0;

fail_bufalloc:
    mPreviewBufProvider->freeBuffer();
fail_reqbufs:
    LOGE("%s(%d): exit with error(%d)",__FUNCTION__,__LINE__,-1);
    return -1;
}

int CameraAdapter::cameraStop()
{
	LOGD("%s(%d):IN",__FUNCTION__,__LINE__);

    int i, buffer_count;
    if (mCamDriverV4l2MemType==V4L2_MEMORY_MMAP) {
        buffer_count = mPreviewBufProvider->getBufCount();
        for (i=0; i<buffer_count; i++) {
            LOGE("%s(%d): munmap %d",__FUNCTION__,__LINE__,i);
            munmap(mCamDriverV4l2Buffer[i], mCamDriverV4l2BufferLen);
        }
    }

	LOGD("%s(%d):OUT",__FUNCTION__,__LINE__);

    return 0;
}
int CameraAdapter::cameraAutoFocus(bool auto_trig_only)
{
    return 0;
}

void CameraAdapter::debugShowFPS()
{
    float mFps = 0;
    mCurFrameCount++;
    if (!(mCurFrameCount & 0x1F)) {
        nsecs_t now = systemTime();
        nsecs_t diff = now - mLastFpsTime;
        mFps = ((mCurFrameCount - mLastFrameCount) * float(s2ns(1))) / diff;
        mLastFpsTime = now;
        mLastFrameCount = mCurFrameCount;
        LOGD("Camera %d Frames, %2.3f FPS", mCurFrameCount, mFps);
    }
    // XXX: mFPS has the value we want
}

//dqbuf
int CameraAdapter::getFrame(FramInfo_s** tmpFrame){

   struct v4l2_buffer cfilledbuffer1;
   int ret = 0;

    memset(&cfilledbuffer1, 0, sizeof(struct v4l2_buffer));
    if (mCapture_type & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        cfilledbuffer1.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    else
        cfilledbuffer1.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cfilledbuffer1.memory = mCamDriverV4l2MemType;
    if (V4L2_TYPE_IS_MULTIPLANAR(cfilledbuffer1.type)) {
            cfilledbuffer1.m.planes = planes;
            cfilledbuffer1.length = 1;
    }

    FILTER_FRAMES:
    /* De-queue the next avaliable buffer */
    //LOGD("%s(%d): camera %d get frame in",__FUNCTION__,__LINE__,mCamId);
    if (xioctl(mCamFd, VIDIOC_DQBUF, &cfilledbuffer1) < 0) {
        LOGE("%s(%d): cameraID [%d %d]  VIDIOC_DQBUF Failed!!! err[%s][%d %d] [%d %d]\n",__FUNCTION__,__LINE__,mCamId, mCamFd, strerror(errno), mCamDriverStream, 
			mPictureRunning, mRefProcessAdapter->getProcessStatus(), mRefProcessAdapter->getProcessState());
        if (errno == EIO) {
            mCamDriverStreamLock.lock();
            if (mCamDriverStream) {
                camera_device_error = true;
                LOGE("%s(%d): camera driver or device may be error, so notify CAMERA_MSG_ERROR",
                        __FUNCTION__,__LINE__);
            }
            mCamDriverStreamLock.unlock();
        } else {
            mCamDriverStreamLock.lock();
            if (mCamDriverStream) {
                mPreviewErrorFrameCount++;
                if (mPreviewErrorFrameCount >= 2) {
                    mCamDriverStreamLock.unlock();
                    camera_device_error = true;
                    LOGE("%s(%d): mPreviewErrorFrameCount is %d, camera driver or device may be error, so notify CAMERA_MSG_ERROR",
                        __FUNCTION__,__LINE__,mPreviewErrorFrameCount);
                } else {
                    mCamDriverStreamLock.unlock();
                }
            } else {
                mCamDriverStreamLock.unlock();
            }
        }
        ret = -1;
        goto getFrame_out;
    } else {
        mPreviewErrorFrameCount = 0;
    }

    LOG2("%s(%d):  camera %d deque a  frame %d success",__FUNCTION__,__LINE__,mCamId, cfilledbuffer1.index);

    if((mPreviewFrameIndex++ < 0))
    {
        LOGD("%s:filter frame %d",__FUNCTION__,mPreviewFrameIndex);
        mCamDriverStreamLock.lock();
        if(mCamDriverStream)
        xioctl(mCamFd, VIDIOC_QBUF, &cfilledbuffer1);
        mCamDriverStreamLock.unlock();
        goto FILTER_FRAMES;
    }

    // fill frame info:w,h,phy,vir
    mPreviewFrameInfos[cfilledbuffer1.index].cameraId = mCamId;
    mPreviewFrameInfos[cfilledbuffer1.index].frame_fmt=  mCamDriverPreviewFmt;
    mPreviewFrameInfos[cfilledbuffer1.index].frame_height = mCamDrvHeight;
    mPreviewFrameInfos[cfilledbuffer1.index].frame_width = mCamDrvWidth;
    mPreviewFrameInfos[cfilledbuffer1.index].frame_index = cfilledbuffer1.index;

    if(mCamDriverV4l2MemType == V4L2_MEMORY_OVERLAY){
        if(cif_driver_iommu){
            mPreviewFrameInfos[cfilledbuffer1.index].phy_addr = mPreviewBufProvider->getBufShareFd(cfilledbuffer1.index);
        }else
            mPreviewFrameInfos[cfilledbuffer1.index].phy_addr = mPreviewBufProvider->getBufPhyAddr(cfilledbuffer1.index);
    }else
        mPreviewFrameInfos[cfilledbuffer1.index].phy_addr = 0;

    if(cfilledbuffer1.memory != V4L2_MEMORY_DMABUF)
        mPreviewFrameInfos[cfilledbuffer1.index].vir_addr = (unsigned long)mCamDriverV4l2Buffer[cfilledbuffer1.index];
    else
        mPreviewFrameInfos[cfilledbuffer1.index].vir_addr = mPreviewBufProvider->getBufPhyAddr(cfilledbuffer1.index);
    //get zoom_value
    mPreviewFrameInfos[cfilledbuffer1.index].zoom_value = mZoomVal;
    mPreviewFrameInfos[cfilledbuffer1.index].used_flag = 0;
    mPreviewFrameInfos[cfilledbuffer1.index].frame_size = cfilledbuffer1.bytesused;
    mPreviewFrameInfos[cfilledbuffer1.index].res        = NULL;
    mPreviewFrameInfos[cfilledbuffer1.index].drm_fd = mPreviewBufProvider->getBufShareFd(cfilledbuffer1.index);

    *tmpFrame = &(mPreviewFrameInfos[cfilledbuffer1.index]);
    LOG2("%s(%d): fill  frame info success",__FUNCTION__,__LINE__);
    debugShowFPS();
getFrame_out:
    if(camera_device_error)
        LOGE("get frame error");
    return ret ;
}

int CameraAdapter::adapterReturnFrame(int index,int cmd){
    int buf_state = -1;
   struct v4l2_buffer vb;
    mCamDriverStreamLock.lock();

    if (!mCamDriverStream) {
        LOGD("%s(%d): preview thread is pause, so buffer %d isn't enqueue to camera",__FUNCTION__,__LINE__,index);
        mCamDriverStreamLock.unlock();
        return 0;
    }
    mPreviewBufProvider->setBufferStatus(index,0, cmd);
    buf_state = mPreviewBufProvider->getBufferStatus(index);
    LOG2("cameera(%d):return buf index(%d),cmd(%d),buf_state(%d)",mCamId,index,cmd,buf_state);
    if (buf_state == 0) {

        if(mCamDriverV4l2MemType != V4L2_MEMORY_DMABUF){
            if (munmap(mCamDriverV4l2Buffer[index], mCamDriverV4l2BufferLen) != 0) {
                LOGE("%s: V4L2 buffer unmap failed: %s", __FUNCTION__, strerror(errno));
                return -EINVAL;
            }

            memset(&vb, 0, sizeof(struct v4l2_buffer));
            if (mCapture_type & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
                vb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            else
                vb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            vb.memory = mCamDriverV4l2MemType;
            vb.index = index;
            //vb.m.offset = mPreviewBufProvider->getBufPhyAddr(index);
            if (V4L2_TYPE_IS_MULTIPLANAR(vb.type)) {
                vb.m.planes = planes;
                vb.length = 1;
            }
        }else{

            memset(&vb, 0, sizeof(struct v4l2_buffer));
            if (mCapture_type & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
                vb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            else
                vb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            vb.memory = mCamDriverV4l2MemType;
            vb.index = index;

            if (V4L2_TYPE_IS_MULTIPLANAR(vb.type)) {
                struct v4l2_plane vplanes[1];
                vplanes[0].m.fd = mPreviewBufProvider->getBufShareFd(index);
                vb.m.planes = vplanes;
                LOG2("%s(%d): mCamID %d buffer index %d fd %d %d %d\n", __FUNCTION__, __LINE__, mCamId,index,
                    vb.m.planes[0].m.fd, vplanes[0].m.fd, mPreviewBufProvider->getBufShareFd(index));
                vb.length = 1;
            }
        }
		
        mPreviewBufProvider->setBufferStatus(index,1,PreviewBufferProvider::CMD_PREVIEWBUF_WRITING);
        if (xioctl(mCamFd, VIDIOC_QBUF, &vb) < 0) {
            LOGE("%s(%d): VIDIOC_QBUF %d Failed!!! err[%s]\n", __FUNCTION__,__LINE__,index, strerror(errno));
            mPreviewBufProvider->setBufferStatus(index,0,PreviewBufferProvider::CMD_PREVIEWBUF_WRITING);
        }

    } else {
        LOGE("%s(%d): buffer %d state 0x%x\n",__FUNCTION__,__LINE__, index, buf_state);
    }

    mCamDriverStreamLock.unlock();
    return 0;
}

int CameraAdapter::returnFrame(long index,int cmd)
{
    return adapterReturnFrame(index,cmd);
}

//define  the frame info ,such as w, h ,fmt
int CameraAdapter::reprocessFrame(FramInfo_s* frame)
{
    //  usb camera may do something
    return 0;
}

void CameraAdapter::setThreadResume()
{
    isThreadNeedtoRun = false;
}

void CameraAdapter::previewThread(){
    FramInfo_s* tmpFrame = NULL;
    int buffer_log = 0;
    int ret = 0;


	while(isThreadNeedtoRun){

		mCamDriverStreamLock.lock();
	    if (mCamDriverStream == false || mPreviewRunning == 0) {
	        mCamDriverStreamLock.unlock();
	        return;
	    }
	    mCamDriverStreamLock.unlock();
		
	    //fill frame info
	    if(mRefProcessAdapter->getProcessStatus() != ProcessAdapter::STA_PROCESS_PAUSE && 
			mRefProcessAdapter->getProcessStatus() != ProcessAdapter::STA_PROCESS_STOP &&
			mRefProcessAdapter->getProcessState() != ProcessAdapter::CMD_PROCESS_PAUSE_PREPARE &&
			mRefProcessAdapter->getProcessState() != ProcessAdapter::CMD_PROCESS_PAUSE_DONE &&
			mRefProcessAdapter->getProcessState() != ProcessAdapter::CMD_PROCESS_STOP_PREPARE &&
			mRefProcessAdapter->getProcessState() != ProcessAdapter::CMD_PROCESS_STOP_DONE){
		    //dispatch a frame
		    tmpFrame = NULL;

	        //get a frame
	        //LOGD("%s%d mCameraID [%d] curframe status [%d %d] [%d %d]\n",__FUNCTION__, __LINE__, mCamId, mPreviewRunning, mCamDriverStream,
	        //    mRefProcessAdapter->getProcessStatus(), mRefProcessAdapter->getProcessState());
		    ret = getFrame(&tmpFrame);

		    if (mDumpFrame) {
		         FILE* fp =NULL;
		         char filename[40];
		         filename[0] = 0x00;
		         sprintf(filename, "/data/camera/camera%d_NV12_%dx%d", mCamId, tmpFrame->frame_width, tmpFrame->frame_height);
		         fp = fopen(filename, "ab+");
		         if (fp) {
		            fwrite((char*)tmpFrame->vir_addr, 1,tmpFrame->frame_width*tmpFrame->frame_height *3/2,fp);
		            fclose(fp);
		            LOG1("Write success yuv data to %s",filename);
		         } else {
		            LOGE("Create %s failed(%d, %s)",filename,fp, strerror(errno));
		         }
		    }


		    LOG2("%s(%d),frame addr = %p,%dx%d,index(%d)",__FUNCTION__,__LINE__,tmpFrame->vir_addr,tmpFrame->frame_width,tmpFrame->frame_height,tmpFrame->frame_index);

		    if((ret!=-1) && (!camera_device_error)){
		        mPreviewBufProvider->setBufferStatus(tmpFrame->frame_index, 0,PreviewBufferProvider::CMD_PREVIEWBUF_WRITING);
		        //set preview buffer status
		        ret = reprocessFrame(tmpFrame);
		        if(ret < 0) {
		            returnFrame(tmpFrame->frame_index,buffer_log);
		            return;
		        }

		        buffer_log = 0;
		        //display ?
		        if(mRefProcessAdapter->isNeedSendToProcess())
		            buffer_log |= PreviewBufferProvider::CMD_PREVIEWBUF_DISPING;

		        mPreviewBufProvider->setBufferStatus(tmpFrame->frame_index,1,buffer_log);

		        if(buffer_log & PreviewBufferProvider::CMD_PREVIEWBUF_DISPING){
		              tmpFrame->used_flag = PreviewBufferProvider::CMD_PREVIEWBUF_DISPING;
		              mRefProcessAdapter->notifyNewFrame(tmpFrame);
		        }

		        if(buffer_log == 0)
		            returnFrame(tmpFrame->frame_index, buffer_log);
		    }else if(camera_device_error){
		        //notify app erro
		        LOGE("camera preview error");
		        return;
		    }else if((ret==-1) && (!camera_device_error)){
				LOGE("camera getframe error");

		        return;
		    }
	    }
	}
    LOG_FUNCTION_NAME_EXIT
    return;
}


void CameraAdapter::autofocusThread()
{
    int err;
    LOG_FUNCTION_NAME
    while (1) {
        mAutoFocusLock.lock();
        /* check early exit request */
        if (mExitAutoFocusThread) {
            mAutoFocusLock.unlock();
            LOG1("%s(%d) exit", __FUNCTION__,__LINE__);
            goto autofocusThread_end;
        }
        mAutoFocusCond.wait(mAutoFocusLock);
        LOGD("wait out\n");
        /* check early exit request */
        if (mExitAutoFocusThread) {
            mAutoFocusLock.unlock();
            LOG1("%s(%d) exit", __FUNCTION__,__LINE__);
            goto autofocusThread_end;
        }
        mAutoFocusLock.unlock();

           //const char *mfocusMode = params.get(CameraParameters::KEY_FOCUS_MODE);
        //if(mfocusMode)
            //err = cameraAutoFocus(mfocusMode/*CameraParameters::FOCUS_MODE_AUTO*/,false);
            err = cameraAutoFocus(false);


    }

autofocusThread_end:
    LOG_FUNCTION_NAME_EXIT
    return;
}

}

