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

#ifndef __VOP_SET_H
#define __VOP_SET_H
#ifdef __cplusplus
extern "C" {
#endif
#include "vop_args.h"

int vop_init(struct drm_resource *drm_res,struct win_arg win_args,uint32_t fb_id);
int vop_set(struct drm_resource *drm_res,struct win_arg win_args,uint32_t fb_id);
int vop_disable(struct drm_resource *drm_res,struct win_arg win_args);
int find_active_crtc_by_connector_type(struct drm_resource *drm_res,struct win_arg win_args);
int find_available_win(struct drm_resource *drm_res,struct win_arg win_args);
int vop_uninit(struct drm_resource *drm_res);

#ifdef __cplusplus
}
#endif

#endif

