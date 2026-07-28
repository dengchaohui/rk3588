/*
 * Copyright (C) 2016 Rockchip Electronics Co., Ltd.
 * Authors:
 *  Zhiqin Wei <wzq@rock-chips.com>
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

#define LOG_NDEBUG 0
#define LOG_TAG "main"

#include <stdint.h>
#include <sys/types.h>
#include <math.h>
#include <pthread.h>
#include <fcntl.h>
#include <utils/misc.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <linux/stddef.h>

#include <utils/Atomic.h>
#include <utils/Errors.h>
#include <utils/Log.h>
#include <utils/Thread.h>
#include <android/bitmap.h>
#include <vector>

#include <jni.h>

#include "CameraHal.h"
#include "ExternalCameraMemManager.h"

///////////////////////////////////////////////////////
class AvmXMLParameters {
	public:
		// Camera parameters
		int camera_num;		 	/* Number of cameras */
		std::string car_model_path;
		std::string calib_result_path;
		std::string file_base_path;
		std::vector<std::vector<int>> camera_fd;
		int readXML(const char* filename);

	private:
		int setParam(int num, const char* val);
     	int getParam(const char* name);
		int readCamera(const char* src, int index);
};


AvmXMLParameters *rk_avm_param_init = NULL; 

using namespace android;

int objectsInit(std::string path);
int auto_calib(void * buf[]);
void objectsFree();
int calib_set_pic_size(int width, int height);
int calib_set_template_mode(int mode);
int calib_set_template_size(int ds, int dh);
int calib_get_input_data(std::string data_input_base_path, unsigned char **input_data);
extern void avm_view_set_offset_en(int val);
extern void avm_view_offset_adjust(int code);

#define RK_AVM_MIDDLEWARE_VERSION 202207121052
#define RUN_AVM_ALGO 1

int algo_status = 0;
CameraHal* gCameraHals[CONFIG_CAMERA_NUM];
MemManagerBase* camMemManager;
BufferProvider* displayBuf;
DisplayAdapter* displayAdapter;
ProcessAdapter* processAdapter;
static int g_start_x[2] = {0, 0};
static int g_start_y[2] = {0, 0};

static int run_algo_count = 0;

extern "C"
JNIEXPORT void JNICALL Java_com_example_myapplication_MainActivity_startAlgo(JNIEnv* env,
        jobject thiz)
{
    property_set("persist.sys.camera360", "1");
    if(run_algo_count == 0)
		++run_algo_count;
	LOGD("start run algo count %d ==================\n", run_algo_count);
	
	rk_avm_param_init = new AvmXMLParameters;

	if(!rk_avm_param_init)
		return;
	
	int xml_ret = rk_avm_param_init->readXML("/data/AppData_rk/avm_settings.xml");

	if(xml_ret)
		LOGE("read xml failed \n");

	LOGD("========== [%d %d %d %d] =========== \n", rk_avm_param_init->camera_fd[0][0], rk_avm_param_init->camera_fd[1][0],
	    rk_avm_param_init->camera_fd[2][0], rk_avm_param_init->camera_fd[3][0]);

	LOGD("========== [%s] [%s] [%s] =========== \n", rk_avm_param_init->file_base_path.c_str(), rk_avm_param_init->calib_result_path.c_str(),
	    rk_avm_param_init->car_model_path.c_str());	

#if RUN_AVM_ALGO
    if(run_algo_count == 1){
		algo_status = 1;

		/*为显示和处理线程初始化内存配置器*/
	    if(camMemManager == NULL)
	        camMemManager = new GrallocDrmMemManager(false);
		else
			LOGD("===========  camMemManager has been constructed, no need new\n");

		if(displayBuf == NULL)
	        displayBuf = new BufferProvider(camMemManager);
		else
			LOGD("===========  displayBuf has been constructed, no need new\n");

		if(displayAdapter == NULL)
	        displayAdapter = new DisplayAdapter();
		else
			LOGD("===========  displayAdapter has been constructed, no need new\n");

		if(processAdapter == NULL)
	        processAdapter = new ProcessAdapter();
		else
			LOGD("===========  processAdapter has been constructed, no need new\n");

		
	    if(processAdapter && displayBuf)
	        displayAdapter->setDisplayBufProvider(displayBuf);

		if(processAdapter && displayAdapter){
			processAdapter->setSurroundModelPath(rk_avm_param_init->car_model_path);
			processAdapter->setSurroundCalibPath(rk_avm_param_init->calib_result_path);
		    processAdapter->setSurroundBasePath(rk_avm_param_init->file_base_path);
	        processAdapter->setDisplayAdapter(displayAdapter);
		}

		/*每个camera对象共享处理线程资源*/

	    for (int i = 0; i < CONFIG_CAMERA_NUM; i++) {
			if(CameraHal::g_myCamFd[i] != -1)
			{
			     close(CameraHal::g_myCamFd[i]);
				 CameraHal::g_myCamFd[i] = -1;
			}

			if(xml_ret)
	        	gCameraHals[i] = new CameraHal(i,i);
			else
				gCameraHals[i] = new CameraHal(i,rk_avm_param_init->camera_fd[i][0]);
	        gCameraHals[i]->setProcessAdapter(processAdapter);
	    }

    	processAdapter->setCameras(gCameraHals);

    	processAdapter->startProcess(1920, 1080);
		
    	displayAdapter->startDisplay(1920, 1080);

    	for (int i = 0; i < CONFIG_CAMERA_NUM; i++) {
        	char* param = gCameraHals[i]->getParameters();
        	CameraParameters params;
        	String8 str_params(param);

        	params.unflatten(str_params);

	        params.setPreviewSize(1920, 1080);
	        gCameraHals[i]->setParameters(params);
	        gCameraHals[i]->bufferPrepare();
	        LOGD("camera %d bufferPrepare over==========", i);
	    }
	
		for (int i = 0; i < CONFIG_CAMERA_NUM; i++) {
	        gCameraHals[i]->startPreview();
	        LOGD("camera %d startPreview over==========", i);
	    }
    }
#endif
    //pthread_exit(NULL);
    return;
}

extern "C"
JNIEXPORT void JNICALL Java_com_example_myapplication_MainActivity_stopAlgo(JNIEnv* env,jobject thiz)
{
    algo_status = 0;

	if(run_algo_count == 1)
	    --run_algo_count;
	LOGD("stop run algo count %d ==================\n", run_algo_count);

#if RUN_AVM_ALGO

    if(run_algo_count == 0){

		displayAdapter->pauseDisplay();
	    processAdapter->pauseProcess();

	    for (int i = 0; i < CONFIG_CAMERA_NUM; i++) {
	        gCameraHals[i]->stopPreview();
	        LOGD("camera %d stopPreview over==========", i);
	        delete gCameraHals[i];
			gCameraHals[i] = NULL;
			
	    }

	    delete processAdapter;
		processAdapter = NULL;
		LOGD("SUCCESS DELETE processAdapter\n");
	    delete displayAdapter;
		displayAdapter = NULL;
		LOGD("SUCCESS DELETE displayAdapter\n");
	    delete displayBuf;
		displayBuf = NULL;
		LOGD("SUCCESS DELETE displayBuf\n");
	    delete camMemManager;
		camMemManager = NULL;
		LOGD("SUCCESS DELETE camMemManager\n");

		delete rk_avm_param_init;
		rk_avm_param_init = NULL;
    }
    property_set("persist.sys.camera360", "2");
#endif
}

extern "C"
JNIEXPORT void JNICALL Java_com_example_myapplication_MainActivity_drawIntoBitmap(JNIEnv* env,
        jobject thiz, jobject dstBitmap, jlong elapsedTime)
{
    // Grab the dst bitmap info and pixels
    AndroidBitmapInfo dstInfo;
    void* dstPixels;
    AndroidBitmap_getInfo(env, dstBitmap, &dstInfo);
    AndroidBitmap_lockPixels(env, dstBitmap, &dstPixels);

    if(algo_status){
		if(displayAdapter)
            displayAdapter->fillDisplayBuffer(dstPixels, dstInfo.width, dstInfo.height);
    }else{
        uint32_t *dest = (uint32_t *)dstPixels;
        int i,j;
        for(i=0;i<100;i++){
            for(j=0;j<100;j++){
                dest[i*dstInfo.width + j] = 0xff0000ff;
            }
        }
    }

    AndroidBitmap_unlockPixels(env, dstBitmap);
}

extern "C"
JNIEXPORT void JNICALL Java_com_example_myapplication_MainActivity_setDiaplaySize(JNIEnv* env,
        jobject thiz, jint width, jint height)
{
    LOGD("setDisplaySize %d %d\n",width,height);
	if(displayAdapter)
        displayAdapter->setDisplaySize(width, height);
}

extern "C"
JNIEXPORT void JNICALL Java_com_example_myapplication_MainActivity_setAlgoType(JNIEnv* env,
        jobject thiz, jint type)
{
    LOGD("setSurround3dType %d==========", type);
    if(algo_status){
		if(processAdapter)
            processAdapter->setSurround3dType(type);
    }
}

extern "C"
JNIEXPORT void JNICALL Java_com_example_myapplication_MainActivity_onScreenTouch(JNIEnv* env,
		jobject thiz, jint sx, jint sy, jint mx, jint my)
{
	LOGD("onScreenTouch start(%d,%d) current (%d,%d)",sx,sy,mx,my);

	if(sx > 1260)
		return;

	if(mx > 1260)
		mx = 1260;

    int dx ,dy;
    if(sx != g_start_x[0]){
		g_start_x[0] = sx;
		g_start_x[1] = mx;
		dx = mx - sx;
    }
	else
	{
	    dx = mx - g_start_x[1];
		g_start_x[1] = mx;
	}

	if(sy != g_start_y[0]){
		g_start_y[0] = sy;
		g_start_y[1] = my;
		dy = my - sy;
    }
	else
	{
	    dy = my - g_start_y[1];
		g_start_y[1] = my;
	}
		
	if(algo_status){
		if(processAdapter)
            processAdapter->setScreenOffset(dx, dy);
	}
}

#define TEMPLATE_SIZE_DS 3120
#define TEMPLATE_SIZE_DH 4250
		
extern "C"
JNIEXPORT void JNICALL Java_com_example_myapplication_ui_main_PlaceholderFragment_startCalib(JNIEnv* env, jobject thiz, jint ab, jint bc, jint ds)
{
	LOGD("==============startCalib==========");

#if 0
	int ret = 0;
	unsigned char* input_data[4] = {NULL, NULL,NULL, NULL};
	int rows, cols, channels;
	std::string base_path = "/data/AppData";

	calib_set_pic_size(1920, 1080);
	calib_set_template_mode(0);
	calib_set_template_size(TEMPLATE_SIZE_DS, TEMPLATE_SIZE_DH);

	LOGD("BASE PATH IS %s %d %d\n", base_path.c_str(), ab, bc);

	ret = calib_get_input_data(base_path, input_data);

	LOGD("==== SUCCESS GET INPUT DATA\n");

	auto_calib((void **)input_data);

	for(int index = 0; index < CAMERAS_NUM; index++)
	{
		if(input_data[index] != NULL)
			free(input_data[index]);
	}

	LOGD("==== RUN HERE %d\n", __LINE__);

#endif
}

extern "C"
JNIEXPORT void JNICALL Java_com_example_myapplication_MainActivity_setViewAdj(JNIEnv* env, jobject thiz, jint code)
{
	LOGD("setViewAdj %d==========",code);
	avm_view_offset_adjust(code);
}

extern "C"
JNIEXPORT void JNICALL Java_com_example_myapplication_MainActivity_setViewAdjEn(JNIEnv* env, jobject thiz, jint en)
{
	LOGD("setViewAdjEn %d==========",en);
	avm_view_set_offset_en(en);
}	

//int main() {
//    Java_com_example_myapplication_MainActivity_startAlgo(NULL, NULL);
//    return 0;
//}
