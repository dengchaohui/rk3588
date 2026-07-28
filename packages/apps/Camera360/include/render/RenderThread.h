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
#ifndef _HANDWRITTEN_DEMO_H
#define _HANDWRITTEN_DEMO_H
#include <utils/threads.h>
#include <SkCanvas.h>
#include <SkBitmap.h>

using namespace android;

class RenderThread : public Thread {

public:
    RenderThread();
    void setDrawSize(int width, int height);
    void setDrawMap(void *drawmap);
    void setScale(float X,float y);
    void setmScale(float X, float Y);
    int render();

private:

    void onFirstRef();
    bool threadLoop();

    SkBitmap bitmap;
    SkPaint  paint;

    //Render coordinates
    unsigned int  x     = 0;
    unsigned int  y     = 0;
    unsigned int  lastX = 0;
    unsigned int  lastY = 0;

    // Pencil info
    bool isPressed = false;
    bool isPen =false;

    //Depend on pencil device
    float XScale = 0.0 ;
    float YScale = 0.0;

    //Depend on touch screen
    float mXScale =  0.0;
    float mYScale =  0.0;

    //Rendor size
    int src_width = 0;
    int src_height = 0;

    //Redirecting virtual addresses
    void *map = NULL;
};


#endif // _HANDWRITTEN_DEMO_H

