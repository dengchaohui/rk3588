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
#ifndef HAL_ROCKCHIP_CAMERA360_CAMERAHAL_H_
#define HAL_ROCKCHIP_CAMERA360_CAMERAHAL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <math.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <utils/Log.h>
#include <utils/threads.h>
#include <cutils/properties.h>
#include <cutils/atomic.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/threads.h>
#include <ui/GraphicBufferAllocator.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/GraphicBuffer.h>
#include <system/window.h>
#include <hardware/hardware.h>
#include <hardware/camera.h>
#include <camera/CameraParameters.h>
#include <linux/v4l2-controls.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <queue>

#include "ExternalCameraMemManager.h"
#include "CameraHal_Tracer.h"
#include "MessageQueue.h"
#include "Semaphore.h"

#include "inc/surround_3d.hpp"

#include "display/vop_buffers.h"
#include "display/vop_args.h"
#include "display/vop_set.h"
#include "display/vop_info.h"
#include "display/util/common.h"
#include "display/util/format.h"
//#include "render/RenderInputEvent.h"
//#include "render/RenderThread.h"

#define SCREEN_WIDTH   1080  // 屏幕分辨率
#define SCREEN_HEIGHT  1920  // 屏幕分辨率

#define IMAGE_WIDTH   1080  // 送显图像分辨率
#define IMAGE_HEIGHT  1920  // 送显图像分辨率

#define MULTI_PT_USING_QUEUE 1

#define CONFIG_CAMERA_NUM 4

#define CONFIG_CAMERA_PREVIEW_BUF_CNT 4
#define CONFIG_CAMERA_DISPLAY_BUF_CNT 2
#define CONFIG_CAMERA_VIDEO_BUF_CNT 4

#define CAMERA_DRIVER_SUPPORT_FORMAT_MAX   64

#define V4L2_BUFFER_MAX             32
#define V4L2_BUFFER_MMAP_MAX        16
#define PAGE_ALIGN(x)   (((x) + 0xFFF) & (~0xFFF)) // Set as multiple of 4K

#define KEY_CONTINUOUS_PIC_NUM  "rk-continous-pic-num"
#define KEY_CONTINUOUS_PIC_INTERVAL_TIME "rk-continous-pic-interval-time"
#define KEY_CONTINUOUS_SUPPORTED    "rk-continous-supported"
#define KEY_PREVIEW_W_FORCE  "rk-previwe-w-force"
#define KEY_PREVIEW_H_FORCE  "rk-previwe-h-force"

#define CAMERA_DISPLAY_FORMAT_YUV420P   CameraParameters::PIXEL_FORMAT_YUV420P
#define CAMERA_DISPLAY_FORMAT_YUV420SP   CameraParameters::PIXEL_FORMAT_YUV420SP
#define CAMERA_DISPLAY_FORMAT_RGB565     CameraParameters::PIXEL_FORMAT_RGB565
#define CAMERA_DISPLAY_FORMAT_RGB888     "rgb888"
#define CAMERA_DISPLAY_FORMAT_NV12       "nv12"

/**************yzm**************/
/* ddl@rock-chips.com : Add ioctrl -  V4L2_CID_SCENE for camera scene control */
#define V4L2_CID_CAMERA_CLASS_BASE_ROCK   (V4L2_CID_CAMERA_CLASS_BASE + 40)
#define V4L2_CID_SCENE        (V4L2_CID_CAMERA_CLASS_BASE_ROCK+1)
#define V4L2_CID_EFFECT       (V4L2_CID_CAMERA_CLASS_BASE_ROCK+2)
#define V4L2_CID_FLASH        (V4L2_CID_CAMERA_CLASS_BASE_ROCK+3)
#define V4L2_CID_FOCUS_CONTINUOUS   (V4L2_CID_CAMERA_CLASS_BASE_ROCK+4)
#define V4L2_CID_FOCUSZONE       (V4L2_CID_CAMERA_CLASS_BASE_ROCK+5)
#define V4L2_CID_FACEDETECT (V4L2_CID_CAMERA_CLASS_BASE_ROCK+6)
#define V4L2_CID_HDR        (V4L2_CID_CAMERA_CLASS_BASE_ROCK+7)
#define V4L2_CID_ISO        (V4L2_CID_CAMERA_CLASS_BASE_ROCK + 8)
#define V4L2_CID_ANTIBANDING    (V4L2_CID_CAMERA_CLASS_BASE_ROCK + 9)
#define V4L2_CID_WHITEBALANCE_LOCK  (V4L2_CID_CAMERA_CLASS_BASE_ROCK + 10)
#define V4L2_CID_EXPOSURE_LOCK    (V4L2_CID_CAMERA_CLASS_BASE_ROCK + 11)
#define V4L2_CID_METERING_AREAS   (V4L2_CID_CAMERA_CLASS_BASE_ROCK + 12)
#define V4L2_CID_WDR        (V4L2_CID_CAMERA_CLASS_BASE_ROCK + 13)
#define V4L2_CID_EDGE       (V4L2_CID_CAMERA_CLASS_BASE_ROCK + 14)
#define V4L2_CID_JPEG_EXIF      (V4L2_CID_CAMERA_CLASS_BASE_ROCK + 15)
/***************yzm***************/

#define DUMP_CAMERA     0x01
#define DUMP_DISPLAY    0x10

namespace android {

typedef struct gralloc_drm_handle_t    rk_gralloc_drm_handle_t;
#define NATIVE_HANDLE_TYPE               rk_gralloc_drm_handle_t


//目前只有CameraAdapter为frame provider，display及event类消费完frame后，可通过该类
//将buffer返回给CameraAdapter,CameraAdapter实现该接口。

//描述帧信息，如width，height，bufaddr，fmt，便于帧消费类收到帧后做后续处理。
//包括zoom的信息
typedef struct FramInfo
{
    ulong_t phy_addr;
    ulong_t vir_addr;
    int frame_width;
    int frame_height;
    short cameraId;
    ulong_t frame_index;
    int frame_fmt;
    int zoom_value;
    ulong_t used_flag;
    int frame_size;
    void* res;
    int drm_fd;
    bool vir_addr_valid;
    bool is_even_field;

}FramInfo_s;

struct render_buffer {
    void *start;
    size_t length;
    struct v4l2_buffer v4l2_buf;
};

class FrameProvider
{
public:
   virtual int returnFrame(long index,int cmd)=0;
   virtual ~FrameProvider(){};
   FrameProvider(){};
   FramInfo_s mPreviewFrameInfos[CONFIG_CAMERA_PREVIEW_BUF_CNT];
};

typedef struct rk_buffer_info {
    Mutex* lock;
    long phy_addr;
    long vir_addr;
    int share_fd;
    int buf_state;
} rk_buffer_info_t;

class BufferProvider{
public:
    int createBuffer(int count,int width, int height, int perbufsize,buffer_type_enum buftype,bool is_cif_driver);
    int createBuffer(int count,int width, int height, int perbufsize, int cameraFd, io_method mIom);
    int freeBuffer();
    virtual int setBufferStatus(int bufindex,int status,int cmd=0);
    virtual int getOneAvailableBuffer(long *buf_phy,long *buf_vir, int *fd);
    int getBufferStatus(int bufindex);
    int getBufCount();
    long getBufPhyAddr(int bufindex);
    long getBufVirAddr(int bufindex);
    int getBufShareFd(int bufindex);
    int flushBuffer(int bufindex);
    BufferProvider(MemManagerBase* memManager):mBufInfo(NULL),mCamBuffer(memManager){}
    virtual ~BufferProvider(){mCamBuffer = NULL;mBufInfo = NULL;}

protected:
    rk_buffer_info_t* mBufInfo;
    int mBufCount;
    buffer_type_enum mBufType;
    MemManagerBase* mCamBuffer;
};

//preview buffer 绠＄悊
class PreviewBufferProvider:public BufferProvider
{
public:

    enum PREVIEWBUFSTATUS {
        CMD_PREVIEWBUF_DISPING = 0x01,
        CMD_PREVIEWBUF_VIDEO_ENCING = 0x02,
        CMD_PREVIEWBUF_SNAPSHOT_ENCING = 0x04,
        CMD_PREVIEWBUF_DATACB = 0x08,
        CMD_PREVIEWBUF_WRITING = 0x10,
    };

#define CAMERA_PREVIEWBUF_ALLOW_DISPLAY(a) ((a&CMD_PREVIEWBUF_WRITING)==0x00)
#define CAMERA_PREVIEWBUF_ALLOW_ENC(a) ((a&CMD_PREVIEWBUF_WRITING)==0x00)
#define CAMERA_PREVIEWBUF_ALLOW_ENC_PICTURE(a) ((a&CMD_PREVIEWBUF_WRITING)==0x00)
#define CAMERA_PREVIEWBUF_ALLOW_DATA_CB(a)  ((a&CMD_PREVIEWBUF_WRITING)==0x00)
#define CAMERA_PREVIEWBUF_ALLOW_WRITE(a)   ((a&(CMD_PREVIEWBUF_DISPING|CMD_PREVIEWBUF_VIDEO_ENCING|CMD_PREVIEWBUF_SNAPSHOT_ENCING|CMD_PREVIEWBUF_DATACB|CMD_PREVIEWBUF_WRITING))==0x00)

    virtual int setBufferStatus(int bufindex,int status,int cmd);

    PreviewBufferProvider(MemManagerBase* memManager):BufferProvider(memManager){}
    ~PreviewBufferProvider(){}
};

class DisplayAdapter;
class ProcessAdapter;
class CameraHal;


/*************************
CameraAdapter 璐熻矗涓庨┍鍔ㄩ�氫俊锛屼笖涓哄抚鏁版嵁鐨勬彁渚涜�咃紝涓篸isplay鍙妋sgcallback鎻愪緵鏁版嵁銆?***************************/
class CameraAdapter:public FrameProvider
{
public:
    CameraAdapter(int cameraId, int dev_id);
    virtual ~CameraAdapter();

    void setProcessAdapterRef(ProcessAdapter& refProcessAdap) {mRefProcessAdapter = &refProcessAdap;}
    void setPreviewBufProvider(BufferProvider* bufprovider);
    void setEncBufProvider(BufferProvider* bufprovider);
    CameraParameters & getParameters();
    virtual int getCurPreviewState(int *drv_w,int *drv_h);
    virtual int getCurVideoSize(int *video_w, int *video_h);
    virtual bool isNeedToRestartPreview();
    int getCameraFd();
    int initialize();
	void setThreadResume();
	int getV4lMemtype();
	
    virtual status_t startBufferPrepare(int preview_w,int preview_h,int w, int h, int fmt,bool is_capture);
    virtual status_t startPreview();
    virtual status_t stopPreview();
   // virtual int initialize() = 0;
    virtual int returnFrame(long index,int cmd);
    virtual int setParameters(const CameraParameters &params_set,bool &isRestartValue);
    virtual void initDefaultParameters(int camFd);
    virtual status_t autoFocus();
    virtual status_t cancelAutoFocus();

    virtual void dump(int cameraId);
    virtual bool getFlashStatus();
    virtual int selectPreferedDrvSize(int *width,int * height,bool is_capture){ return 0;}
    virtual void debugShowFPS();
    virtual int flashcontrol();

    bool cif_driver_iommu;


protected:
    //talk to driver
    virtual int cameraCreate(int cameraId);
    virtual int cameraDestroy();

    virtual int cameraSetSize(int w, int h, int fmt, bool is_capture);
    virtual int cameraStream(bool on);
    virtual int cameraStart();
    virtual int cameraStop();
    //dqbuf
    virtual int getFrame(FramInfo_s** frame);
    virtual int cameraAutoFocus(bool auto_trig_only);

    //qbuf
 //   virtual status_t fillThisBuffer();

    //define  the frame info ,such as w, h ,fmt ,dealflag(preview callback ? display ? video enc ? picture?)
    virtual int reprocessFrame(FramInfo_s* frame);
    virtual int adapterReturnFrame(int index,int cmd);

private:
    class CameraPreviewThread :public Thread
    {
        //deque 鍒板抚鍚庢牴鎹渶瑕佸垎鍙戠粰DisplayAdapter绫诲強EventNotifier绫汇�?
        CameraAdapter* mPreivewCameraAdapter;
    public:
        CameraPreviewThread(CameraAdapter* adapter)
            : Thread(false), mPreivewCameraAdapter(adapter) { }

        virtual bool threadLoop() {
            mPreivewCameraAdapter->previewThread();

            return true;
        }
    };

#if 1
    void autofocusThread();
    class AutoFocusThread : public Thread {
        CameraAdapter* mCameraAdapter;
    public:
        AutoFocusThread(CameraAdapter* hw)
            : Thread(false), mCameraAdapter(hw) { }

        virtual bool threadLoop() {
            mCameraAdapter->autofocusThread();

            return false;
        }
    };

    sp<AutoFocusThread>  mAutoFocusThread;
    Mutex mAutoFocusLock;
    bool mExitAutoFocusThread;
    Condition mAutoFocusCond;
#endif


protected:
    virtual void previewThread();
protected:
    ProcessAdapter* mRefProcessAdapter;
    sp<CameraPreviewThread> mCameraPreviewThread;
    int mPreviewRunning;
    int mPictureRunning;
    BufferProvider* mPreviewBufProvider;
    BufferProvider* mEncBufProvider;
    int mCamDrvWidth;
    int mCamDrvHeight;
    int mCamPreviewH ;
    int mCamPreviewW ;
    int mVideoWidth;
    int mVideoHeight;
	bool isThreadNeedtoRun;

    bool mDumpFrame;

    struct v4l2_plane planes[1];

    uint32_t mCapture_type;

    unsigned int mCamDriverSupportFmt[CAMERA_DRIVER_SUPPORT_FORMAT_MAX];
    enum v4l2_memory mCamDriverV4l2MemType;

    unsigned int mCamDriverPreviewFmt;
    struct v4l2_capability mCamDriverCapability;

    int mPreviewErrorFrameCount;
    int mPreviewFrameIndex;

    Mutex mCamDriverStreamLock;
    bool mCamDriverStream;

    bool camera_device_error;

    CameraParameters mParameters;
    unsigned int CameraHal_SupportFmt[6];

    char *mCamDriverV4l2Buffer[V4L2_BUFFER_MAX];
    unsigned int mCamDriverV4l2BufferLen;

    int mCurBufLength;
    int mZoomVal;
    int mZoomMin;
    int mZoomMax;
    int mZoomStep;

    int mCamFd;
    int mCamId;
	int mCamDevId;

    int mCurFrameCount;
    int mLastFrameCount;
    nsecs_t mLastFpsTime;
};

//soc camera adapter
class CameraISPAdapter: public CameraAdapter
{
public:
    CameraISPAdapter(int cameraId);
    virtual ~CameraISPAdapter();

    /*********************
    talk to driver
    **********************/
    //parameters
    virtual int setParameters(const CameraParameters &params_set,bool &isRestartValue);
    virtual void initDefaultParameters(int camFd);
    virtual int cameraAutoFocus(bool auto_trig_only);
    virtual int selectPreferedDrvSize(int *width,int * height,bool is_capture);
    virtual int flashcontrol();
private:
    static unsigned int mFrameSizesEnumTable[][2];

    typedef struct frameSize_s{
        unsigned int width;
        unsigned int height;
        unsigned int fmt;
        int framerate;
    }frameSize_t;
    Vector<frameSize_t> mFrameSizeVector;

    int cameraFramerateQuery(unsigned int format, unsigned int w, unsigned int h, int *min, int *max);
    int cameraFpsInfoSet(CameraParameters &params);
    int cameraConfig(const CameraParameters &tmpparams,bool isInit,bool &isRestartValue);


    unsigned int mCamDriverFrmWidthMax;
    unsigned int mCamDriverFrmHeightMax;
    String8 mSupportPreviewSizeReally;

    struct v4l2_querymenu mWhiteBalance_menu[20];
    int mWhiteBalance_number;

    struct v4l2_querymenu mEffect_menu[20];
    int mEffect_number;

    struct v4l2_querymenu mScene_menu[20];
    int mScene_number;

    struct v4l2_querymenu mAntibanding_menu[20];
    int mAntibanding_number;


    struct v4l2_querymenu mFlashMode_menu[20];
    int mFlashMode_number;


};

class CameraSOCAdapter: public CameraAdapter
{
public:
    CameraSOCAdapter(int cameraId, int dev_id);
    virtual ~CameraSOCAdapter();
    virtual int cameraStop();
    virtual int setParameters(const CameraParameters &params_set,bool &isRestartValue);
    virtual void initDefaultParameters(int camFd);
    virtual int reprocessFrame(FramInfo_s* frame);


private:
    int cameraConfig(const CameraParameters &tmpparams,bool isInit,bool &isRestartValue);

    int mCamDriverFrmWidthMax;
    int mCamDriverFrmHeightMax;
    String8 mSupportPreviewSizeReally;

    struct v4l2_querymenu mWhiteBalance_menu[20];
    int mWhiteBalance_number;

    struct v4l2_querymenu mEffect_menu[20];
    int mEffect_number;

    struct v4l2_querymenu mScene_menu[20];
    int mScene_number;

    struct v4l2_querymenu mAntibanding_menu[20];
    int mAntibanding_number;

    int mZoomMin;
    int mZoomMax;
    int mZoomStep;

    struct v4l2_querymenu mFlashMode_menu[20];
    int mFlashMode_number;

};


/*************************
DisplayAdapter 涓哄抚鏁版嵁娑堣垂鑰咃紝浠嶤ameraAdapter鎺ユ敹甯ф暟鎹苟鏄剧ず
***************************/
class DisplayAdapter//:public CameraHal_Tracer
{
    class DisplayThread : public Thread {
        DisplayAdapter* mDisplayAdapter;
    public:
        DisplayThread(DisplayAdapter* disadap)
            : Thread(false), mDisplayAdapter(disadap){}

        virtual bool threadLoop() {
            mDisplayAdapter->displayThread();

            return false;
        }
    };

public:
    enum DisplayThreadCommands {
        // Comands
        CMD_DISPLAY_PAUSE,
        CMD_DISPLAY_START,
        CMD_DISPLAY_STOP,
        CMD_DISPLAY_FRAME,
        CMD_DISPLAY_INVAL
    };

    enum DISPLAY_STATUS{
        STA_DISPLAY_RUNNING,
        STA_DISPLAY_PAUSE,
        STA_DISPLAY_STOP,
    };
    enum DisplayCommandStatus{
        CMD_DISPLAY_START_PREPARE = 1,
        CMD_DISPLAY_START_DONE,
        CMD_DISPLAY_PAUSE_PREPARE,
        CMD_DISPLAY_PAUSE_DONE,
        CMD_DISPLAY_STOP_PREPARE,
        CMD_DISPLAY_STOP_DONE,
        CMD_DISPLAY_FRAME_PREPARE,
        CMD_DISPLAY_FRAME_DONE,
    };

    typedef struct rk_displaybuf_info {
        Mutex* lock;
        buffer_handle_t* buffer_hnd;
        NATIVE_HANDLE_TYPE *priv_hnd;
        long phy_addr;
        long vir_addr;
        int buf_state;
        int stride;
    } rk_displaybuf_info_t;

    bool isNeedSendToDisplay();
    void notifyNewFrame(int index);

    int startDisplay(int width, int height);
    int stopDisplay();
    int pauseDisplay();

    int getDisplayStatus(void);
    void setDisplayBufProvider(BufferProvider* provider) {mDisplayBufs = provider;}
    void setProcessAdapter(ProcessAdapter* processAdapter) {mProcessAdapter = processAdapter;}
    int getOneAvailableBuffer(long *buf_phy,long *buf_vir, int *fd);
    void dump();

    void setBufferState(int index,int status);
    int fillDisplayBuffer(void *buffer, int width, int height);
    int setDisplaySize(int width, int height);

    DisplayAdapter();
    ~DisplayAdapter();

private:
    int cameraDisplayBufferCreate(int width, int height, const char *fmt,int numBufs);
    int cameraDisplayBufferDestory(void);
    void displayThread();
    void setDisplayState(int state);

    int initDrmHwc();
    int deintDrmHwc();

    int mReadyIndex = -1;
    bool mDumpFrame;

	std::queue<enum DisplayThreadCommands> mDThreadCmdsQueue;
	std::queue<int> mDThreadBufIndex;
	Semaphore mDCmdsem;
	Semaphore mStateComplete;

    rk_displaybuf_info_t* mDisplayBufInfo;
    int mDislayBufNum;
    int mDisplayWidth;
    int mDisplayHeight;
    int mDisplayRuning;
    char mDisplayFormat[30];
    BufferProvider* mDisplayBufs;

    struct win_arg win_args;
    struct bo *win_bo;
    struct file_arg file_args;
    struct drm_resource drm_res;
    uint32_t fb_id;
    //sp<RenderThread> paintThread;

    FrameProvider* mFrameProvider;
    ProcessAdapter* mProcessAdapter;

    Mutex mDisplayLock;
    Condition mDisplayCond;
    int mDisplayState;

    void *fill_buffer;
    int fill_width;
    int fill_height;
    bool need_fill_buf;
    Semaphore fillbuffer_sem;

    MemManagerBase* mTmpMemManager;
    BufferProvider* mTmpBuf;

    MessageQueue    *displayThreadCommandQ;
    sp<DisplayThread> mDisplayThread;
};

class ProcessAdapter//:public CameraHal_Tracer
{
    class ProcessThread : public Thread {
        ProcessAdapter* mProcessAdapter;
    public:
        ProcessThread(ProcessAdapter* proadap)
            : Thread(false), mProcessAdapter(proadap){}

        virtual bool threadLoop() {
            mProcessAdapter->processThread();

            return false;
        }
    };

public:
    enum ProcessThreadCommands {
        // Comands
        CMD_PROCESS_PAUSE,
        CMD_PROCESS_START,
        CMD_PROCESS_STOP,
        CMD_PROCESS_FRAME,
        CMD_PROCESS_SHARE_FD,
        CMD_PROCESS_INVAL
    };

    enum PROCESS_STATUS{
        STA_PROCESS_RUNNING,
        STA_PROCESS_PAUSE,
        STA_PROCESS_STOP,
    };
    enum ProcessCommandStatus{
        CMD_PROCESS_START_PREPARE = 1,
        CMD_PROCESS_START_DONE,
        CMD_PROCESS_PAUSE_PREPARE,
        CMD_PROCESS_PAUSE_DONE,
        CMD_PROCESS_STOP_PREPARE,
        CMD_PROCESS_STOP_DONE,
        CMD_PROCESS_FRAME_PREPARE,
        CMD_PROCESS_FRAME_PROCESSING,
        CMD_PROCESS_FRAME_DONE,
        CMD_PROCESS_FRAME_PROCESSING_DONE,
        CMD_PROCESS_FRAME_SHARE_FD_PREPARE,
        CMD_PROCESS_FRAME_SHARE_FD_DONE,
    };

    bool isNeedSendToProcess();
    void notifyNewFrame(FramInfo_s* frame);
    void notifyShareFd(int *fd, int camera_id, void *sem);

    int startProcess(int width, int height);
    int stopProcess();
    int pauseProcess();

    int getProcessStatus(void);
    int getProcessState();
	inline int getInternBufferShareCount(){return mInternBufferCount;}
	void setInternBufferAcc(int offet);
    void setFrameProvider(FrameProvider* framePro);
    void setDisplayAdapter(DisplayAdapter* displayAdapter) {mDisplayAdapter = displayAdapter;}
    void setCameras(CameraHal** cameras) {mCameras = cameras;}
	void setSurroundModelPath(std::string path){model_path = path;}
	void setSurroundCalibPath(std::string path){calib_result_path = path;}
	void setSurroundBasePath(std::string path){base_content_path = path;}
	std::string getSurroundModelPath(){return model_path;}
    void dump();

    int getReadyFramesIndex(int curId);
	void setScreenOffset(int x_offset, int y_offset);
    void setSurround3dType(int type);
    ProcessAdapter();
    ~ProcessAdapter();

private:
    void processThread();
    void setBufferState(int index,int status);
    void setProcessState(int state);

    void initSurround3d();
    void deinitSurround3d();

    int mReadyIndex = -1;
	std::string model_path;
	std::string calib_result_path;
	std::string base_content_path;

    std::queue<enum ProcessThreadCommands> mPThreadCmdsQueue;
	std::queue<int> mPThreadValidIndex;
	std::queue<int> mPThreadValidCameraID;
	Semaphore mPCmdsem;
	Semaphore mStateComplete;
    int mInternBufferCount;
    int mDislayBufNum;
    int mDisplayWidth;
    int mDisplayHeight;
    int mProcessRuning;
    char mDisplayFormat[30];
    FramInfo_s mFrames[CONFIG_CAMERA_PREVIEW_BUF_CNT][CONFIG_CAMERA_NUM];
    int mReadyCounts[CONFIG_CAMERA_NUM] = {0};
    int Framefd[CONFIG_CAMERA_NUM][CONFIG_CAMERA_PREVIEW_BUF_CNT] = {0};
	int ScreenOffset[2] = {0,0};

    CameraHal** mCameras;

    surround_3d* mSurround3d;

    FrameProvider* mFrameProvider;

    DisplayAdapter* mDisplayAdapter;

    Mutex mProcessLock;
    Condition mProcessCond;
    int mProcessState;

    MessageQueue    *processThreadCommandQ;
    sp<ProcessThread> mProcessThread;
};



/***********************
CameraHal绫昏礋璐ｄ笌cameraservice鑱旂郴锛屽疄鐜?cameraservice瑕佹眰瀹炵幇鐨勬帴鍙ｃ�傛绫诲彧璐熻矗鍏叡璧勬簮鐨勭敵璇凤紝浠ュ強浠诲姟鐨勫垎鍙戙�?***********************/
class CameraHal
{
public:
/*--------------------Interface Methods---------------------------------*/
    /**
     * Start preview mode.
     */

    static int g_myCamFd[CONFIG_CAMERA_NUM];
    int startPreview();

    int bufferPrepare();

    /**
     * Stop a previously started preview.
     */
    void stopPreview();

    /**
     * Set the camera parameters. This returns BAD_VALUE if any parameter is
     * invalid or not supported.
     */
    int setParameters(const char *parms);
    int setParameters(const CameraParameters &params_set);
    int setParametersUnlock(const CameraParameters &params_set);

    /** Retrieve the camera parameters.  The buffer returned by the camera HAL
        must be returned back to it with put_parameters, if put_parameters
        is not NULL.
     */
    char* getParameters();

    /** The camera HAL uses its own memory to pass us the parameters when we
        call get_parameters.  Use this function to return the memory back to
        the camera HAL, if put_parameters is not NULL.  If put_parameters
        is NULL, then you have to use free() to release the memory.
    */
    void putParameters(char *);

    /**
     * Release the hardware resources owned by this object.  Note that this is
     * *not* done in the destructor.
     */
    void release();

/*--------------------Internal Member functions - Public---------------------------------*/
    /** Constructor of CameraHal */
    CameraHal(int cameraId, int dev_id);

    // Destructor of CameraHal
    virtual ~CameraHal();

    int getCameraFd();
    int  getCameraId();

    CameraAdapter* getCameraAdapter() {return mCameraAdapter;}
    void setProcessAdapter(ProcessAdapter* processAdapter) {mProcessAdapter = processAdapter;mCameraAdapter->setProcessAdapterRef(*processAdapter);}
    CameraAdapter* mCameraAdapter;
    ProcessAdapter* mProcessAdapter;
    BufferProvider* mPreviewBuf;
    BufferProvider* mVideoBuf;
    BufferProvider* mRawBuf;
    BufferProvider* mJpegBuf;
    MemManagerBase* mCamMemManager;
public:
    bool mInitState;

private:
    int selectPreferedDrvSize(int *width,int * height,bool is_capture);
    void setCamStatus(int status, int type);
    int checkCamStatus(int cmd);
    enum CommandThreadCommands {
        // Comands
        CMD_PREVIEW_START = 1,
        CMD_PREVIEW_STOP,
        CMD_PREVIEW_CAPTURE_CANCEL,
        CMD_CONTINUOS_PICTURE,
        CMD_BUFFER_PREPARE,

        CMD_AF_START,
        CMD_AF_CANCEL,

        CMD_SET_PREVIEW_WINDOW,
        CMD_SET_PARAMETERS,
        CMD_START_FACE_DETECTION,
        CMD_EXIT,

    };

    enum CommandThreadStatus{
       CMD_STATUS_RUN ,
       CMD_STATUS_STOP ,

    };

    enum CommandStatus{
        CMD_PREVIEW_START_PREPARE             = 0x01,
        CMD_PREVIEW_START_DONE                = 0x02,
        CMD_PREVIEW_START_MASK                = 0x03,

		CMD_PREVIEW_BUFFER_PREPARE          = 0x04,
		CMD_PREVIEW_BUFFER_PREPARE_DONE     = 0x05,
		CMD_PREVIEW_BUFFER_PREPARE_MASK     = 0x06,

        CMD_PREVIEW_STOP_PREPARE            = 0x07,
        CMD_PREVIEW_STOP_DONE                = 0x08,
        CMD_PREVIEW_STOP_MASK                = 0x0C,

        CMD_CONTINUOS_PICTURE_PREPARE        = 0x10,
        CMD_CONTINUOS_PICTURE_DONE            = 0x20,
        CMD_CONTINUOS_PICTURE_MASK            = 0x30,

        CMD_PREVIEW_CAPTURE_CANCEL_PREPARE     = 0x40,
        CMD_PREVIEW_CAPTURE_CANCEL_DONE     = 0x80,
        CMD_PREVIEW_CAPTURE_CANCEL_MASK     = 0xC0,

        CMD_AF_START_PREPARE                = 0x100,
        CMD_AF_START_DONE                    = 0x200,
        CMD_AF_START_MASK                    = 0x300,

        CMD_AF_CANCEL_PREPARE                = 0x400,
        CMD_AF_CANCEL_DONE                    = 0x800,
        CMD_AF_CANCEL_MASK                    = 0xC00,

        CMD_SET_PREVIEW_WINDOW_PREPARE        = 0x1000,
        CMD_SET_PREVIEW_WINDOW_DONE            = 0x2000,
        CMD_SET_PREVIEW_WINDOW_MASK            = 0x3000,

        CMD_SET_PARAMETERS_PREPARE            = 0x4000,
        CMD_SET_PARAMETERS_DONE                = 0x8000,
        CMD_SET_PARAMETERS_MASK                = 0xC000,

        CMD_EXIT_PREPARE                    = 0x10000,
        CMD_EXIT_DONE                        = 0x20000,
        CMD_EXIT_MASK                        = 0x30000,

        STA_RECORD_RUNNING                    = 0x40000,
        STA_PREVIEW_CMD_RECEIVED            = 0x80000,

        STA_DISPLAY_RUNNING                    = 0x100000,
        STA_DISPLAY_PAUSE                    = 0x200000,
        STA_DISPLAY_STOP                    = 0x400000,
        STA_DISPLAY_MASK                    = 0x700000,
    };

    class CommandThread : public Thread {
        CameraHal* mHardware;
    public:
        CommandThread(CameraHal* hw)
            : Thread(false), mHardware(hw) { }

        virtual bool threadLoop() {
            mHardware->commandThread();

            return false;
        }
    };

   friend class CommandThread;
   void commandThread();
   sp<CommandThread> mCommandThread;
   MessageQueue *commandThreadCommandQ;
   int mCommandRunning;

   std::queue<enum CommandThreadCommands> mHThreadCmdsQueue;
   std::queue<CameraParameters> mHThreadData;
   Semaphore mHCmdsem;
   Semaphore mStateComplete;

   void updateParameters(CameraParameters & tmpPara);
   CameraParameters mParameters;


   mutable Mutex mLock;        // API lock -- all public methods
  // bool mRecordRunning;
  // bool mPreviewCmdReceived;
   int mCamFd;
   unsigned int mCameraStatus;
   int mCamId;
   int mCamDev;
};


}
#endif
