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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>

#include "drm.h"
#include "drm_fourcc.h"

#include "libdrm_macros.h"
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "display/vop_buffers.h"

struct drm_rockchip_gem_phys {
	uint32_t handle;
	uint32_t phy_addr;
};

#define DRM_ROCKCHIP_GEM_GET_PHYS	0x04
#define DRM_IOCTL_ROCKCHIP_GEM_GET_PHYS		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_ROCKCHIP_GEM_GET_PHYS, struct drm_rockchip_gem_phys)


/* -----------------------------------------------------------------------------
 * Buffers management
 */

static struct bo *
bo_create_dumb(int fd, unsigned int width, unsigned int height, unsigned int bpp, unsigned int flags)
{
	struct drm_mode_create_dumb arg;
	struct bo *bo;
	int ret;

	bo = calloc(1, sizeof(*bo));
	if (bo == NULL) {
		fprintf(stderr, "failed to allocate buffer object\n");
		return NULL;
	}

	memset(&arg, 0, sizeof(arg));
	arg.bpp = bpp;
	arg.width = width;
	arg.height = height;
  arg.flags = flags;

	ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &arg);
	if (ret) {
		fprintf(stderr, "failed to create dumb buffer: %s\n",
			strerror(errno));
		free(bo);
		return NULL;
	}

	bo->fd = fd;
	bo->handle = arg.handle;
	bo->size = arg.size;
	bo->pitch = arg.pitch;

	return bo;
}

static int bo_map(struct bo *bo, void **out)
{
	struct drm_mode_map_dumb arg;
	void *map;
	int ret;

	memset(&arg, 0, sizeof(arg));
	arg.handle = bo->handle;

	ret = drmIoctl(bo->fd, DRM_IOCTL_MODE_MAP_DUMB, &arg);
	if (ret)
		return ret;

	map = drm_mmap(0, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED,
		       bo->fd, arg.offset);
	if (map == MAP_FAILED)
		return -EINVAL;

	bo->ptr = map;
	*out = map;

	return 0;
}

static void bo_unmap(struct bo *bo)
{
	if (!bo->ptr)
		return;

	drm_munmap(bo->ptr, bo->size);
	bo->ptr = NULL;
}

unsigned int get_phy_addr(int fd, struct bo *bo)
{
	struct drm_rockchip_gem_phys arg;
	int ret = 0;

	arg.handle = bo->handle;

	ret = drmIoctl(fd, DRM_IOCTL_ROCKCHIP_GEM_GET_PHYS, &arg);
	if (ret) {
		fprintf(stderr, "failed to get phy addr: %s\n",
			strerror(errno));
		return 0;
	}

	return arg.phy_addr;
}


struct bo *
bo_create(int fd, unsigned int format,
	  unsigned int width, unsigned int height,
	  unsigned int handles[4], unsigned int pitches[4],
	  unsigned int offsets[4], enum util_fill_pattern pattern,
	  struct file_arg *file_args)
{
	unsigned int virtual_height;
	struct bo *bo;
	unsigned int bpp;
	void *planes[3] = { 0, };
	void *virtual = 0;
	int ret = 0;

	switch (format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		bpp = 8;
		break;

	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_BGRA4444:
	case DRM_FORMAT_BGRX4444:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_BGRA5551:
	case DRM_FORMAT_BGRX5551:
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
		bpp = 16;
		break;

	case DRM_FORMAT_BGR888:
	case DRM_FORMAT_RGB888:
		bpp = 24;
		break;

	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_RGBX1010102:
	case DRM_FORMAT_BGRA1010102:
	case DRM_FORMAT_BGRX1010102:
		bpp = 32;
		break;

	default:
		fprintf(stderr, "unsupported format 0x%08x\n",  format);
		return NULL;
	}

	switch (format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		virtual_height = height * 3 / 2;
		break;

	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		virtual_height = height * 2;
		break;

	default:
		virtual_height = height;
		break;
	}

	bo = bo_create_dumb(fd, width, virtual_height, bpp, file_args->flags);
	if (!bo)
		return NULL;

  file_args->size = bo->size;

  if(file_args->flags & ROCKCHIP_BO_CONTIG || file_args->flags & ROCKCHIP_BO_SECURE){
    file_args->phy_addr = (void*)(intptr_t)get_phy_addr(fd, bo);
    fprintf(stderr, "get phy addr:0x%x\n",file_args->phy_addr);
  }

  if((file_args->flags & ROCKCHIP_BO_SECURE) == 0){
    ret = bo_map(bo, &virtual);
    if (ret) {
      fprintf(stderr, "failed to map buffer: %s\n",
      	strerror(-errno));
      bo_destroy(bo);
      return NULL;
  	}
  }
	/* just testing a limited # of formats to test single
	 * and multi-planar path.. would be nice to add more..
	 */
	switch (format) {
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
		offsets[0] = 0;
		handles[0] = bo->handle;
		pitches[0] = bo->pitch;

		planes[0] = virtual;
		break;

	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		offsets[0] = 0;
		handles[0] = bo->handle;
		pitches[0] = bo->pitch;
		pitches[1] = pitches[0];
		offsets[1] = pitches[0] * height;
		handles[1] = bo->handle;

		planes[0] = virtual;
		planes[1] = virtual + offsets[1];
		break;

	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		offsets[0] = 0;
		handles[0] = bo->handle;
		pitches[0] = bo->pitch;
		pitches[1] = pitches[0] / 2;
		offsets[1] = pitches[0] * height;
		handles[1] = bo->handle;
		pitches[2] = pitches[1];
		offsets[2] = offsets[1] + pitches[1] * height / 2;
		handles[2] = bo->handle;

		planes[0] = virtual;
		planes[1] = virtual + offsets[1];
		planes[2] = virtual + offsets[2];
		break;

	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_BGRA4444:
	case DRM_FORMAT_BGRX4444:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_BGRA5551:
	case DRM_FORMAT_BGRX5551:
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_BGR888:
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_RGBX1010102:
	case DRM_FORMAT_BGRA1010102:
	case DRM_FORMAT_BGRX1010102:
		offsets[0] = 0;
		handles[0] = bo->handle;
		pitches[0] = bo->pitch;

		planes[0] = virtual;
		break;
  }
  if((file_args->flags & ROCKCHIP_BO_SECURE) == 0){

    if(file_args->file_path[0] == 'y'){
      util_fill_pattern(format, pattern, planes, width, height, pitches[0]);
    }else{
      memset(planes[0],0x80,bo->size);
    }
  }
	return bo;
}

void bo_destroy(struct bo *bo)
{
	struct drm_mode_destroy_dumb arg;
	int ret;
  if(bo->ptr != NULL){
	  bo_unmap(bo);
	  bo->ptr = NULL;
	}
	memset(&arg, 0, sizeof(arg));
	arg.handle = bo->handle;

	ret = drmIoctl(bo->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &arg);
	if (ret)
		fprintf(stderr, "failed to destroy dumb buffer: %s\n",
			strerror(errno));

	free(bo);
  bo = NULL;
}

struct bo *initial_win_buffer(struct drm_resource *drm_res,struct file_arg *file_args,uint32_t *fb_id)
{
  uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
  struct bo * win_bo;
  int ret;
  int drm_res_fd = drm_res->fd;

  uint64_t cap = 0;
  ret = drmGetCap(drm_res_fd, DRM_CAP_DUMB_BUFFER, &cap);
  if (ret || cap == 0) {
    fprintf(stderr, "driver doesn't support the dumb buffer API\n");
    return NULL;
  }

  win_bo = bo_create(drm_res_fd, file_args->fourcc, file_args->w, file_args->h, handles,
			     pitches, offsets, UTIL_PATTERN_SMPTE, file_args);
  if (win_bo == NULL)
	    return NULL;
  file_args->virtual_addr = win_bo->ptr;

  ret = drmModeAddFB2(drm_res_fd, file_args->w, file_args->h, file_args->fourcc, handles,
          pitches, offsets, fb_id, 0);
  if (ret)
    fprintf(stderr, "failed to create fb_id %d\n", ret);

  return win_bo;
}

