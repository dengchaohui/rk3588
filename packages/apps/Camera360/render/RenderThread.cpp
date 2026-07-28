/*************************************************
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * Author:
 *        libin <bin.li@rock-chips.com>
 * Date:
 *        2020-01-09
 * Description:
 *        Output display content by calling libdrm interface.
 *
 **************************************************/
// #define LOG_NDEBUG 0
#define LOG_TAG "RenderThread"

#include <utils/Log.h>
#include <linux/input.h>

#include <errno.h>
#include <math.h>

#include "render/RenderInputEvent.h"
#include "render/RenderThread.h"

using namespace android;

RenderThread::RenderThread() {}

void RenderThread::onFirstRef() {
    if (init_getevent() != 0) {
        ALOGE("error: EventThread did not work!\n");
    }
}

void RenderThread::setDrawSize(int width, int height)
{
    src_width=width;
    src_height=height;
}
void RenderThread::setDrawMap(void *drawmap){
    map = drawmap ;
}

void RenderThread::setScale(float X, float Y){
    XScale=X;
    YScale=Y;
}

void RenderThread::setmScale(float X, float Y){
    mXScale=X;
    mYScale=Y;
}


bool RenderThread::threadLoop() {

   nsecs_t start = systemTime(SYSTEM_TIME_MONOTONIC);

   input_event e;
   SkCanvas canvas(bitmap);

   canvas.drawPoint(0, 0, paint);

      /*
       *  just for debug print
       */
      nsecs_t end= systemTime(SYSTEM_TIME_MONOTONIC);
      ALOGV("drawcanvas, time=%0.3fms", (end - start) * 0.000001f);
      
   return false;
}

int RenderThread::render(){
    paint.setARGB(255, 255, 0, 0);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setAntiAlias(true);
    paint.setStrokeWidth(4);
    bitmap.allocN32Pixels(src_width, src_height);

    if(map != NULL)
      bitmap.setPixels(map);

    run("RenderThread", PRIORITY_URGENT_DISPLAY);

    ALOGV("RenderThread render success! \n");
    return 0;
}
