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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>     /* System V */
#include <sys/ioctl.h>  /* BSD and Linux */
#include <drm_fourcc.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "display/vop_args.h"
#include "display/vop_set.h"
#include "display/util/common.h"
#include "display/util/format.h"


#if 0

int dumpWinInfo(struct win_arg *win){

  printf("win_type  = %d \n",win->win_type);
  return 0;
}
int dumpBufferInfo(struct file_arg *file){
  printf("file_path  = %s \n",file->file_path);
  return 0;
}


int parse_win(struct win_arg *win, const char *p)
{
  char *end;

  win->win_type = strtoul(p, &end, 10);
  if (*end != '@')
    return -EINVAL;

  p = end + 1;
  win->crtc_id = strtoul(p, &end, 10);
  if (*end != ':')
    return -EINVAL;

  //src crop
  p = end + 1;
  win->crop_left = strtoul(p, &end, 10);
  if (*end != ',')
    return -EINVAL;

  p = end + 1;
  win->crop_top = strtoul(p, &end, 10);
  if (*end != ',')
    return -EINVAL;

  p = end + 1;
  win->crop_w = strtoul(p, &end, 10);
  if (*end != ',')
    return -EINVAL;

  p = end + 1;
  win->crop_h = strtoul(p, &end, 10);
  if (*end != ':')
    return -EINVAL;

  //disp_rec
  p = end + 1;
  win->disp_left = strtoul(p, &end, 10);
  if (*end != ',')
    return -EINVAL;

  p = end + 1;
  win->disp_top = strtoul(p, &end, 10);
  if (*end != ',')
    return -EINVAL;

  p = end + 1;
  win->disp_w = strtoul(p, &end, 10);
  if (*end != ',')
    return -EINVAL;

  p = end + 1;
  win->disp_h = strtoul(p, &end, 10);

  if (*end == '#') {
    p = end + 1;
    win->zpos = strtod(p, &end);
    if (win->zpos < 0.0)
      return -EINVAL;
  } else {
    win->zpos = 0;
  }

  if (*end == '@') {
    p = end + 1;
    if (strlen(p) != 4)
      return -EINVAL;

    strcpy(win->format_str, p);
  } else {
    strcpy(win->format_str, "XR24");
  }

  win->fourcc = util_format_fourcc(win->format_str);
  if (win->fourcc == 0) {
    fprintf(stderr, "unknown format %s\n", win->format_str);
    return -EINVAL;
  }

  return 0;
}

int parse_file(struct file_arg *file_args, const char *p)
{
  char *end;

  file_args->w = strtoul(p, &end, 10);
  if (*end != 'x')
    return -EINVAL;

  p = end + 1;
  file_args->h = strtoul(p, &end, 10);

  if (*end == '@') {
    p = end + 1;
    strncpy(file_args->format_str, p, 4);
    end = end + 5;
  } else {
    strcpy(file_args->format_str, "XR24");
  }

  file_args->fourcc = util_format_fourcc(file_args->format_str);
  if (file_args->fourcc == 0) {
    fprintf(stderr, "unknown format %s\n", file_args->format_str);
    return -EINVAL;
  }

  if (*end == '#'){
    p = end + 1;
    strcpy(file_args->file_path, p);
    fprintf(stderr, "file path = %s \n", file_args->file_path);
  }
  return 0;
}

int parse_chip(const char *p)
{
  char *end;

  char chip_name[6];
  struct win_arg win_args[4];
  struct file_arg file_args[4];
  int num = 0;

  memset(chip_name,0x00,6*sizeof(char));
  memset(win_args,0x00,sizeof(win_args));
  memset(file_args,0x00,sizeof(file_args));
  strncpy(chip_name, p ,6);
  printf("Chip name = %s \n",chip_name);
  p = p + 6;

  if(*p == '#'){
     p = p + 1;
     num = strtoul(p, &end, 10);
  }
  // default config
  //pthread_t thread_memcpy[1];
  pthread_t thread_rga[1];
  win_args[0].crtc_id = 0;
  win_args[0].win_type = 0;
  win_args[0].crop_left = 0;
  win_args[0].crop_top = 0;
  win_args[0].crop_w = 4096;
  win_args[0].crop_h = 2160;
  win_args[0].fourcc = DRM_FORMAT_NV12;
  win_args[0].disp_left = 0;
  win_args[0].disp_top = 0;
  win_args[0].disp_w = 0;
  win_args[0].disp_h = 0;

  file_args[0].w = 4096;
  file_args[0].h = 2160;
  file_args[0].fourcc = DRM_FORMAT_NV12;

  win_args[1].crtc_id = 0;
  win_args[1].win_type = 1;
  win_args[1].crop_left = 0;
  win_args[1].crop_top = 0;
  win_args[1].crop_w = 0;
  win_args[1].crop_h = 0;
  win_args[1].fourcc = DRM_FORMAT_ARGB8888;
  win_args[1].disp_left = 0;
  win_args[1].disp_top = 0;
  win_args[1].disp_w = 0;
  win_args[1].disp_h = 0;

  file_args[1].w = 0;
  file_args[1].h = 0;
  file_args[1].fourcc = DRM_FORMAT_ARGB8888;

  win_args[2].crtc_id = 0;
  win_args[2].win_type = 2;
  win_args[2].crop_left = 0;
  win_args[2].crop_top = 0;
  win_args[2].crop_w = 0;
  win_args[2].crop_h = 0;
  win_args[2].fourcc = DRM_FORMAT_ARGB8888;
  win_args[2].disp_left = 0;
  win_args[2].disp_top = 0;
  win_args[2].disp_w = 0;
  win_args[2].disp_h = 0;

  file_args[2].w = 0;
  file_args[2].h = 0;
  file_args[2].fourcc = DRM_FORMAT_ARGB8888;

  win_args[3].crtc_id = 0;
  win_args[3].win_type = 3;
  win_args[3].crop_left = 0;
  win_args[3].crop_top = 0;
  win_args[3].crop_w = 0;
  win_args[3].crop_h = 0;
  win_args[3].fourcc = DRM_FORMAT_ARGB8888;
  win_args[3].disp_left = 0;
  win_args[3].disp_top = 0;
  win_args[3].disp_w = 0;
  win_args[3].disp_h = 0;

  file_args[3].w = 0;
  file_args[3].h = 0;
  file_args[3].fourcc = DRM_FORMAT_ARGB8888;

  if(!strcmp(chip_name,"rk3229")){
    if(num == 0)
      num = 4;
    win_args[1].crop_w = 1920;
    win_args[1].crop_h = 1080;
    file_args[1].w = 1920;
    file_args[1].h = 1080;
    //improve_system_DDR_load_by_memcpy(thread_memcpy,&num);
    improve_system_DDR_load_by_rga(thread_rga,&num);
    vop_set(win_args, file_args,2,1);
  }else if(!strcmp(chip_name,"rk3288")){
    if(num == 0)
      num = 32;
    //improve_system_DDR_load_by_memcpy(thread_memcpy,&num);
    improve_system_DDR_load_by_rga(thread_rga,&num);
    vop_set(win_args, file_args,2,1);
  }else if(!strcmp(chip_name,"rk3328")){
    if(num == 0)
      num = 4;
    win_args[1].crop_w = 1920;
    win_args[1].crop_h = 1080;
    file_args[1].w = 1920;
    file_args[1].h = 1080;
    win_args[2].crop_w = 1920;
    win_args[2].crop_h = 1080;
    file_args[2].w = 1920;
    file_args[2].h = 1080;
    win_args[2].win_type = 3;
    //improve_system_DDR_load_by_memcpy(thread_memcpy,&num);
    improve_system_DDR_load_by_rga(thread_rga,&num);
    vop_set(win_args, file_args,3,1);
  }else if(!strcmp(chip_name,"rk3326")){
    if(num == 0)
      num = 32;
    win_args[0].win_type = 1;
    win_args[0].crop_w = 1920;
    win_args[0].crop_h = 1080;
    file_args[0].w = 1920;
    file_args[0].h = 1080;
    file_args[0].fourcc = DRM_FORMAT_ARGB8888;

    win_args[1].win_type = 3;
    win_args[2].win_type = 0;

    improve_system_DDR_load_by_rga(thread_rga,&num);
    vop_set(win_args, file_args,3,1);

  }else if(!strcmp(chip_name,"rk3368")){
    if(num == 0)
      num = 1;
    //improve_system_DDR_load_by_memcpy(thread_memcpy,&num);
    improve_system_DDR_load_by_rga(thread_rga,&num);
    vop_set(win_args, file_args,2,1);

  }else if(!strcmp(chip_name,"rk3399")){
    if(num == 0)
      num = 1;
    //improve_system_DDR_load_by_memcpy(thread_memcpy,&num);
    improve_system_DDR_load_by_rga(thread_rga,&num);
    vop_set(win_args, file_args,2,1);
  }else{
    printf("Unknown chip type \n ");
    return -1;
  }
  return 0;
}
void usage(char *name)
{
  fprintf(stderr, "usage: %s [-pwfdch]\n", name);
  fprintf(stderr, "\nTest options:\n\n");
  fprintf(stderr, "\t-p          Print vop information\n");
  fprintf(stderr, "\t-w          <win_id>@<crtc_id>:<left>,<top>,<w>,<h>:<left>,<top>,<w>,<h>[#zpos][@<format>]\n");
  fprintf(stderr, "\t-f          <w>x<h>[@<format>][#<file_path>]\n");
  fprintf(stderr, "\t-d          Level : Increase system bandwidth \n");
  fprintf(stderr, "\t-c          Chip type : rk3399 rk3326 rk3368 rk3328 rk3288 rk3128 rk3188\n");
  fprintf(stderr, "\t-h          Help \n");
  exit(0);
}
#endif
