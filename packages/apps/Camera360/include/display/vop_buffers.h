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

#ifndef __BUFFERS_H__
#define __BUFFERS_H__

#include "display/util/pattern.h"
#include "display/vop_args.h"

#ifdef __cplusplus
extern "C" {
#endif

/* memory type definitions. */
enum drm_rockchip_gem_mem_type {
    /* Physically Continuous memory and used as default. */
    ROCKCHIP_BO_CONTIG      = 1 << 0,
    /* cachable mapping. */
    ROCKCHIP_BO_CACHABLE    = 1 << 1,
    /* write-combine mapping. */
    ROCKCHIP_BO_WC          = 1 << 2,
    ROCKCHIP_BO_SECURE      = 1 << 3,
    ROCKCHIP_BO_MASK        = ROCKCHIP_BO_CONTIG | ROCKCHIP_BO_CACHABLE |
    ROCKCHIP_BO_WC | ROCKCHIP_BO_SECURE
};

struct bo
{
    int fd;
    void *ptr;
    size_t size;
    size_t offset;
    size_t pitch;
    unsigned handle;
};

struct bo *bo_create(int fd, unsigned int format,
		   unsigned int width, unsigned int height,
		   unsigned int handles[4], unsigned int pitches[4],
		   unsigned int offsets[4], enum util_fill_pattern pattern,
		   struct file_arg *file_args);
void bo_destroy(struct bo *bo);

struct bo *initial_win_buffer(struct drm_resource *drm_res, struct file_arg *file_args,uint32_t *fb_id);

#ifdef __cplusplus
}
#endif

#endif
