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
#ifndef __VOP_ARGS_H
#define __VOP_ARGS_H

#ifdef __cplusplus
extern "C" {
#endif
#define DRM_PLANE_TYPE_OVERLAY 0
#define DRM_PLANE_TYPE_PRIMARY 1
#define DRM_PLANE_TYPE_CURSOR  2
#define DRM_PLANE_PERFORMANCE  4


#define DRM_ATOMIC_ADD_PROP(object_id, type ,prop_id, value) \
  ret = drmModeAtomicAddProperty(req, object_id, prop_id, value); \
  fprintf(stderr, "    plane_id = %d %s = %d\n",object_id, type, value > 32767? value >> 16: value); \
  if (ret < 0) \
  fprintf(stderr, "Failed to add prop[%d] to [%d]", value, prop_id);

struct plane_prop {
  int crtc_id;
  int fb_id;

  int src_x;
  int src_y;
  int src_w;
  int src_h;

  int crtc_x;
  int crtc_y;
  int crtc_w;
  int crtc_h;

  int zpos;
};
struct win_arg {
  char win_type_name[30];                             /* the win type*/
  uint32_t connector_type;                      /* the type of Connector to bind to */
  uint32_t mode_w,mode_h;                       /* the mode of resolution */
  uint32_t crop_left, crop_top, crop_w, crop_h; /* src crop info */
  uint32_t disp_left, disp_top, disp_w, disp_h; /* dst disp ingo */
  uint32_t zpos;                                /* disp zpos */
  char format_str[5];                           /* need to leave room for terminating \0 */
  unsigned int fourcc;
};

struct file_arg {
  char file_path[256];  /* the file path*/
  uint32_t w, h;        /* the file image w,h info */
  size_t size;
  uint32_t flags;       /* the flags of buffer */
  char format_str[5];   /* need to leave room for terminating \0 */
  unsigned int fourcc;
  void *virtual_addr;   /* the virtual address */
  void *phy_addr;       /* the physical address */
};

struct drm_resource{
  int fd;

  drmModeResPtr res;
  drmModeCrtcPtr crtc;
  drmModePlanePtr plane;

  struct plane_prop plane_prop;
  uint32_t found_crtc;
};

int parse_win(struct win_arg *win, const char *p);
int parse_file(struct file_arg *file_args, const char *p);
int parse_chip(const char *p);
void usage(char *name);

#ifdef __cplusplus
}
#endif
#endif
