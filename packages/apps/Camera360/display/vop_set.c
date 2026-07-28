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

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cutils/properties.h>
#include <time.h>

#include <sys/mman.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>     /* System V */
#include <sys/ioctl.h>  /* BSD and Linux */

#include <pthread.h>

#include "display/vop_buffers.h"
#include "display/vop_args.h"
#include "display/vop_set.h"
#include "display/util/common.h"
#include "display/util/format.h"

int find_active_crtc(struct drm_resource *drm_res, struct win_arg win_args){
  int drm_res_fd = 0;
  drmModeModeInfo *mode = NULL;
  drmModeResPtr res = NULL;
  drmModeConnectorPtr connector = NULL;
  drmModeCrtcPtr crtc = NULL;
  drmModeObjectPropertiesPtr props = NULL;
  drmModePropertyPtr prop = NULL;
  uint32_t found_crtc = 0;
  uint32_t found_conn = 0;

  drm_res_fd = drm_res->fd;
  res = drm_res->res;

  for (int i = 0; i < res->count_crtcs; ++i) {
    crtc = drmModeGetCrtc(drm_res_fd, res->crtcs[i]);
    if (!crtc) {
      fprintf(stderr, "Could not get crtc %u: %s\n",res->crtcs[i], strerror(errno));
      continue;
    }

    fprintf(stderr,"<<<<< Want to find active CRTC: crtc_id = %d:\n",crtc->crtc_id);

    props = drmModeObjectGetProperties(drm_res_fd, crtc->crtc_id,
               DRM_MODE_OBJECT_CRTC);
    if (!props) {
      fprintf(stderr, "Failed to found props crtc[%d] %s\n",crtc->crtc_id, strerror(errno));
      drmModeFreeCrtc(crtc);
      crtc = NULL;
      continue;
    }

    for (uint32_t j = 0; j < props->count_props; j++) {
      prop = drmModeGetProperty(drm_res_fd, props->props[j]);
      fprintf(stderr,"\t Index = %2u, prop_name = %15s, value = %lu \n", j, prop->name, props->prop_values[j]);
      if (!strcmp(prop->name, "ACTIVE")) {
        if (props->prop_values[j]) {
          found_crtc = 1 << i;
          /*
           * Found active connect.
           */
          for (i = 0; i < res->count_connectors; ++i) {
            connector = drmModeGetConnector(drm_res_fd, res->connectors[i]);

            /* connector must be connected */
            if(connector->connection == DRM_MODE_CONNECTED){
              found_conn = 1;
            }else{
              drmModeFreeConnector(connector);
              connector = NULL;
              continue;
            }
            for(int j = 0;;){
              drmModeModeInfo *mode_temp = &(connector->modes[j]);
              if(mode_temp == NULL)
                break;
              if(win_args.mode_w == 0 || win_args.mode_h == 0){
                mode = mode_temp;
                break;
              }
              if(mode_temp->hdisplay == win_args.mode_w && mode_temp->vdisplay == win_args.mode_h){
                mode = mode_temp;
                break;
              }
              j++;
            }
            if(mode == NULL){
              drmModeFreeConnector(connector);
              connector = NULL;
              continue;
            }
            fprintf(stderr,"<<<<< To find connector_id = %u, connection = %d,mode w=%d,h=%d\n",
              connector->connector_id,connector->connection,mode->hdisplay,mode->vdisplay);

            if (found_conn){
              fprintf(stderr, ">>>>> Find active connect_id %d\n", connector->connector_id);
              break;
            }
            drmModeFreeConnector(connector);
            connector = NULL;
            mode = NULL;
          }
        }
      }
      drmModeFreeProperty(prop);
      prop = NULL;
      if(found_crtc){
        fprintf(stderr, ">>>>> Find active crtc %d\n", crtc->crtc_id);
        drmModeFreeObjectProperties(props);
        props = NULL;
        drm_res->crtc = crtc;
        drm_res->found_crtc = found_crtc;
        return 0;
      }
    }
    drmModeFreeCrtc(crtc);
    drmModeFreeObjectProperties(props);
    crtc = NULL;
    props = NULL;
  }
  return -ENODEV;
}

int find_active_crtc_by_connector_type(struct drm_resource *drm_res, struct win_arg win_args){
  int drm_res_fd = 0;
  drmModeModeInfo *mode = NULL;
  drmModeResPtr res = NULL;
  drmModeConnectorPtr connector = NULL;
  drmModeCrtcPtr crtc = NULL;
  drmModeObjectPropertiesPtr props = NULL;
  drmModePropertyPtr prop = NULL;
  uint32_t found_crtc_id = 0;
  uint32_t found_enc_id = 0;
  uint32_t found_crtc = 0;
  uint32_t found_conn = 0;

  drm_res_fd = drm_res->fd;
  res = drm_res->res;

  /*
   * Found active connector.
   */
  for (int i = 0; i < res->count_connectors; ++i) {
    connector = drmModeGetConnector(drm_res_fd, res->connectors[i]);

    /* connector must be connected */
    if(connector->connection == DRM_MODE_CONNECTED &&
       win_args.connector_type == connector->connector_type){
      found_conn = 1;
      found_enc_id = connector->encoder_id;
    }else{
      drmModeFreeConnector(connector);
      connector = NULL;
      continue;
    }
    for(int j = 0;;){
      drmModeModeInfo *mode_temp = &(connector->modes[j]);
      if(mode_temp == NULL)
        break;
      if(win_args.mode_w == 0 || win_args.mode_h == 0){
        mode = mode_temp;
        break;
      }
      if(mode_temp->hdisplay == win_args.mode_w && mode_temp->vdisplay == win_args.mode_h){
        mode = mode_temp;
        break;
      }
      j++;
    }
    if(mode == NULL){
      drmModeFreeConnector(connector);
      connector = NULL;
      continue;
    }
    fprintf(stderr,"<<<<< To find connector_id = %u, connection = %d,mode w=%d,h=%d\n",
      connector->connector_id,connector->connection,mode->hdisplay,mode->vdisplay);

    if (found_conn){
      fprintf(stderr, ">>>>> Find active connect_id %d\n", connector->connector_id);
      drmModeFreeConnector(connector);
      connector = NULL;
      mode = NULL;
      break;
    }
    drmModeFreeConnector(connector);
    connector = NULL;
    mode = NULL;
  }

  /*
   * Found encoder.
   */
  for (int i = 0; i < res->count_encoders; ++i) {
      drmModeEncoderPtr e = drmModeGetEncoder(drm_res_fd, res->encoders[i]);
      if (!e) {
        fprintf(stderr,"Failed to get encoder %d", res->encoders[i]);
        drmModeFreeEncoder(e);
        e = NULL;
        return -ENODEV;
      }

      fprintf(stderr,"<<<<< Want to find dst active encoder: encoder-id=%d, crtc_id=%d, encoder_type=%d:\n",e->encoder_id,e->crtc_id,e->encoder_type);

      if(found_enc_id == e->encoder_id){
        found_crtc_id = e->crtc_id;
        fprintf(stderr,">>>>> Find dst active encoder: encoder-id=%d, crtc_id = %d:\n",e->encoder_id,e->crtc_id);
        break;
      }
    }


  /*
   * Found crtc.
   */
  for (int i = 0; i < res->count_crtcs; ++i) {
    crtc = drmModeGetCrtc(drm_res_fd, res->crtcs[i]);
    if (!crtc) {
      fprintf(stderr, "Could not get crtc %u: %s\n",res->crtcs[i], strerror(errno));
      continue;
    }

    fprintf(stderr,"<<<<< Want to find dst active CRTC: crtc_id = %d, crtc-mode=%dx%dp%d clk=%d,flags=0x%x:\n",crtc->crtc_id,
            crtc->mode.hdisplay,crtc->mode.vdisplay,crtc->mode.vrefresh,
            crtc->mode.clock,crtc->mode.flags);

    props = drmModeObjectGetProperties(drm_res_fd, crtc->crtc_id,
               DRM_MODE_OBJECT_CRTC);
    if (!props) {
      fprintf(stderr, "Failed to found props crtc[%d] %s\n",crtc->crtc_id, strerror(errno));
      drmModeFreeCrtc(crtc);
      crtc = NULL;
      continue;
    }

    for (uint32_t j = 0; j < props->count_props; j++) {
      prop = drmModeGetProperty(drm_res_fd, props->props[j]);
      fprintf(stderr,"\t Index = %2u, prop_name = %15s, value = %lu \n", j, prop->name, props->prop_values[j]);
      if (!strcmp(prop->name, "ACTIVE")) {
        if (props->prop_values[j] && crtc->crtc_id == found_crtc_id) {
          found_crtc = 1 << i;
        }
      }
      drmModeFreeProperty(prop);
      prop = NULL;

      if(found_crtc){
          fprintf(stderr, ">>>>> Find active crtc %d\n", crtc->crtc_id);
          drmModeFreeObjectProperties(props);
          mode = NULL;
          props = NULL;
          drm_res->crtc = crtc;
          drm_res->found_crtc = found_crtc;
          return 0;
      }
    }
    drmModeFreeObjectProperties(props);
    drmModeFreeCrtc(crtc);
    props = NULL;
    crtc = NULL;
  }
  return -ENODEV;
}

int find_available_win(struct drm_resource *drm_res, struct win_arg win_args){
  int drm_res_fd = 0;
  drmModeResPtr res = NULL;
  drmModeObjectPropertiesPtr props = NULL;
  drmModePropertyPtr prop = NULL;
  drmModePlaneResPtr plane_res = NULL;
  drmModePlanePtr plane = NULL;

  uint32_t found_crtc = 0;
  uint32_t found_plane = 0;

  drm_res_fd = drm_res->fd;
  res = drm_res->res;
  found_crtc = drm_res->found_crtc;

  plane_res = drmModeGetPlaneResources(drm_res_fd);
  for(uint32_t i = 0; i < plane_res->count_planes; i++){
    plane = drmModeGetPlane(drm_res_fd, plane_res->planes[i]);
    if (!plane) {
      fprintf(stderr, "Failed to found plane[%d] %s\n",plane->plane_id, strerror(errno));
      drmModeFreePlaneResources(plane_res);
      plane_res = NULL;
      return -ENODEV;
    }

    props = drmModeObjectGetProperties(drm_res_fd, plane->plane_id,DRM_MODE_OBJECT_PLANE);
    if (!props) {
      fprintf(stderr, "Failed to found props plane[%d] %s\n",plane->plane_id, strerror(errno));
      drmModeFreePlane(plane);
      drmModeFreePlaneResources(plane_res);
      plane = NULL;
      plane_res = NULL;
      return -ENODEV;
    }
    //fprintf(stderr, "\t Index = %2u, found_crtc = %d ,possible_crtcs = %d \n", i, found_crtc, plane->possible_crtcs);
    if ((plane->possible_crtcs & found_crtc) > 0) {
    }else{
      drmModeFreeObjectProperties(props);
      drmModeFreePlane(plane);
      props = NULL;
      plane = NULL;
      continue;
    }
    for (uint32_t j = 0; j < props->count_props; j++) {
      prop = drmModeGetProperty(drm_res_fd, props->props[j]);
      //fprintf(stderr,"\t Index = %2u, prop_name = %15s, value = %lu \n", j, prop->name, props->prop_values[j]);
      if(!strcmp(prop->name, "NAME")){
        fprintf(stderr,">>>>> Check plane = %d name=%s, acquire name=%s\n",plane->plane_id, prop->enums[0].name, win_args.win_type_name);
        if(!strncmp(prop->enums[0].name,win_args.win_type_name,strlen(win_args.win_type_name))){
          fprintf(stderr,"<<<<< Find plane = %d name=%s, acquire name=%s\n",plane->plane_id, prop->enums[0].name, win_args.win_type_name);
          found_plane++;
          break;
        }
      }
      drmModeFreeProperty(prop);
      prop = NULL;
    }
    if(found_plane){
        fprintf(stderr,">>>>> Find active plane_id = %2d \n", plane->plane_id);
        break;
    }
    drmModeFreeObjectProperties(props);
    drmModeFreePlane(plane);
    props = NULL;
    plane = NULL;
  }

  if(!found_plane){
    fprintf(stderr,">>>>> Fail to find active plane\n");
    drmModeFreePlaneResources(plane_res);
    plane_res = NULL;
    return -1;
  }

  for (uint32_t i = 0; i < props->count_props; i++) {
    prop = drmModeGetProperty(drm_res_fd, props->props[i]);
    if (!strcmp(prop->name, "CRTC_ID"))
      drm_res->plane_prop.crtc_id = prop->prop_id;
    else if (!strcmp(prop->name, "FB_ID"))
      drm_res->plane_prop.fb_id = prop->prop_id;
    else if (!strcmp(prop->name, "SRC_X"))
      drm_res->plane_prop.src_x = prop->prop_id;
    else if (!strcmp(prop->name, "SRC_Y"))
      drm_res->plane_prop.src_y = prop->prop_id;
    else if (!strcmp(prop->name, "SRC_W"))
      drm_res->plane_prop.src_w = prop->prop_id;
    else if (!strcmp(prop->name, "SRC_H"))
      drm_res->plane_prop.src_h = prop->prop_id;
    else if (!strcmp(prop->name, "CRTC_X"))
      drm_res->plane_prop.crtc_x = prop->prop_id;
    else if (!strcmp(prop->name, "CRTC_Y"))
      drm_res->plane_prop.crtc_y = prop->prop_id;
    else if (!strcmp(prop->name, "CRTC_W"))
      drm_res->plane_prop.crtc_w = prop->prop_id;
    else if (!strcmp(prop->name, "CRTC_H"))
      drm_res->plane_prop.crtc_h = prop->prop_id;
    else if (!strcmp(prop->name, "zpos")) {
      drm_res->plane_prop.zpos = prop->prop_id;
    }
    drmModeFreeProperty(prop);
    prop = NULL;
  }
  drmModeFreePlaneResources(plane_res);
  plane_res = NULL;
  drmModeFreeObjectProperties(props);
  props = NULL;
  drm_res->plane = plane;
  return 0;
}


int vop_init(struct drm_resource *drm_res, struct win_arg win_args, uint32_t fb_id){

  int ret = 0;
  int drm_res_fd = 0;
  if(drm_res->fd <= 0){
      fprintf(stderr, "vop_init fail, drm_resource fd = %d", drm_res->fd);
      return -1;
  }
  drm_res_fd = drm_res->fd;

  ret = drmSetClientCap(drm_res_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
  if (ret) {
    fprintf(stderr, "Failed to set atomic cap %s", strerror(errno));
    return ret;
  }

  ret = drmSetClientCap(drm_res_fd, DRM_CLIENT_CAP_ATOMIC, 1);
  if (ret) {
    fprintf(stderr, "Failed to set atomic cap %s", strerror(errno));
    return ret;
  }

  drm_res->res = drmModeGetResources(drm_res_fd);
  if (!drm_res->res) {
    fprintf(stderr, "Failed to get resources: %s\n",
      strerror(errno));
    return -ENODEV;
  }

  //Find a usable crtc
  if(win_args.connector_type != 0){
    ret = find_active_crtc_by_connector_type(drm_res,win_args);
    if(ret < 0){
      fprintf(stderr, "vop_init can't find dst active connector,type = %d \n",win_args.connector_type);
    }
  }

  //Find available win
  ret = find_available_win(drm_res,win_args);
  return ret;

}

int vop_set(struct drm_resource *drm_res, struct win_arg win_args, uint32_t fb_id){
  int drm_res_fd = 0;
  int ret;
  uint32_t flags = 0;
  drmModeCrtcPtr crtc = NULL;
  drmModePlanePtr plane = NULL;

  drm_res_fd = drm_res->fd;
  crtc = drm_res->crtc;
  plane = drm_res->plane;

  static int frame_cnt = 0;
  fprintf(stderr,">>>>> To commit frame = %d: \n",frame_cnt++);

#ifdef ENABLE_ATOMIC_COMMIT
  drmModeAtomicReq *req = drmModeAtomicAlloc();


  DRM_ATOMIC_ADD_PROP(plane->plane_id, "crtc-id", drm_res->plane_prop.crtc_id, crtc->crtc_id);           //crtc_id
  DRM_ATOMIC_ADD_PROP(plane->plane_id, "  fb-id", drm_res->plane_prop.fb_id, fb_id);                     //src fd
  DRM_ATOMIC_ADD_PROP(plane->plane_id, "  src-x", drm_res->plane_prop.src_x, win_args.crop_left << 16);        //src xoffset
  DRM_ATOMIC_ADD_PROP(plane->plane_id, "  src-y", drm_res->plane_prop.src_y, win_args.crop_top << 16);         //src yoffset
  DRM_ATOMIC_ADD_PROP(plane->plane_id, "  src-W", drm_res->plane_prop.src_w, win_args.crop_w << 16);     //src Width
  DRM_ATOMIC_ADD_PROP(plane->plane_id, "  src-H", drm_res->plane_prop.src_h, win_args.crop_h << 16);     //src Height
  DRM_ATOMIC_ADD_PROP(plane->plane_id, "  dst-x", drm_res->plane_prop.crtc_x, win_args.disp_left);       //dst xoffset
  DRM_ATOMIC_ADD_PROP(plane->plane_id, "  dst-y", drm_res->plane_prop.crtc_y, win_args.disp_top);        //dst yoffset
  DRM_ATOMIC_ADD_PROP(plane->plane_id, "  dst-W", drm_res->plane_prop.crtc_w, win_args.disp_w);          //dst Width
  DRM_ATOMIC_ADD_PROP(plane->plane_id, "  dst-H", drm_res->plane_prop.crtc_h, win_args.disp_h);          //dst Height
  DRM_ATOMIC_ADD_PROP(plane->plane_id, "   zpos", drm_res->plane_prop.zpos, win_args.zpos);              //zpos
  //flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
  ret = drmModeAtomicCommit(drm_res_fd, req, flags, NULL);
  if (ret)
    fprintf(stderr, "atomic: couldn't commit new state: %s\n", strerror(errno));

  drmModeAtomicFree(req);
#else

  ret = drmModeSetPlane(drm_res_fd, plane->plane_id, crtc->crtc_id, fb_id, flags,
                        win_args.disp_left, win_args.disp_top, win_args.disp_w, win_args.disp_h,
                        win_args.crop_left, win_args.crop_top, win_args.crop_w << 16, win_args.crop_h << 16);
  if (ret)
    fprintf(stderr, "drmModeSetPlane: commit fail, %s\n", strerror(errno));
#endif
  return ret;
}

int vop_disable(struct drm_resource *drm_res,struct win_arg win_args){

  int drm_res_fd = 0;
  drmModeCrtcPtr crtc = NULL;
  drmModePlanePtr plane = NULL;
  drmModeObjectPropertiesPtr props = NULL;
  drmModePropertyPtr prop = NULL;

  drm_res_fd = drm_res->fd;
  crtc = drm_res->crtc;
  plane = drm_res->plane;


  drmModeAtomicReqPtr pset = drmModeAtomicAlloc();
  if (!pset) {
    fprintf(stderr,"Failed to allocate property set");
    return -ENOMEM;
  }

  int ret;

  props = drmModeObjectGetProperties(drm_res_fd, plane->plane_id,DRM_MODE_OBJECT_PLANE);
  for (uint32_t j = 0; j < props->count_props; j++) {
    prop = drmModeGetProperty(drm_res_fd, props->props[j]);
    //fprintf(stderr,"\t Index = %2u, prop_name = %15s, value = %lu \n", j, prop->name, props->prop_values[j]);
    if (!strcmp(prop->name, "CRTC_ID")) {
      ret = drmModeAtomicAddProperty(pset, plane->plane_id, prop->prop_id,0) < 0;
      if (ret) {
        fprintf(stderr,"Failed to add plane %d disable to pset", plane->plane_id);
        drmModeFreeProperty(prop);
        drmModeAtomicFree(pset);
        return ret;
      }
    }
    if (!strcmp(prop->name, "FB_ID")) {
      ret = drmModeAtomicAddProperty(pset, plane->plane_id, prop->prop_id,0) < 0;
      if (ret) {
        fprintf(stderr,"Failed to add plane %d disable to pset", plane->plane_id);
        drmModeFreeProperty(prop);
        drmModeAtomicFree(pset);
        return ret;
      }
    }
    drmModeFreeProperty(prop);
    prop = NULL;
  }
  drmModeFreeObjectProperties(props);
  props = NULL;

  uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;
  ret = drmModeAtomicCommit(drm_res_fd, pset, flags, NULL);
  if (ret) {
    fprintf(stderr,"Failed to commit pset ret=%d\n", ret);
    drmModeAtomicFree(pset);
    return ret;
  }

  drmModeAtomicFree(pset);

  return 0;
}


int vop_uninit(struct drm_resource *drm_res){
  if(drm_res->res)
    drmModeFreeResources(drm_res->res);
  if(drm_res->crtc)
    drmModeFreeCrtc(drm_res->crtc);
  if(drm_res->plane)
    drmModeFreePlane(drm_res->plane);
  return 0;
}


