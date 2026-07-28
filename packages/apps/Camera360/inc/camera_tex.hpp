#ifndef CAMERA_TEX_HPP_
#define CAMERA_TEX_HPP_


#include <string>
#include <fstream>
#include <iomanip>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <utils/Log.h>


#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "surround_3d_inf.hpp"

/*******************************************************************************************
 * Global functions
 *******************************************************************************************/

#define CAMERAS_NUM 4

#ifdef _WIN64

#define RENDER_LOGD(format, ...) printf("[%s][%d]: " format "\n", __FUNCTION__,\
                            __LINE__, ##__VA_ARGS__)				  
#define RENDER_LOGE(format, ...) printf("[%s][%d]: " format "\n", __FUNCTION__,\
                            __LINE__, ##__VA_ARGS__)				  
#define RENDER_LOGI(format, ...) printf("[%s][%d]: " format "\n", __FUNCTION__,\
                            __LINE__, ##__VA_ARGS__)				  
#define RENDER_LOGW(format, ...) printf("[%s][%d]: " format "\n", __FUNCTION__,\
                            __LINE__, ##__VA_ARGS__)
#else
#define RENDER_LOGE(args...) do {printf(args);ALOGE(args);}while(0)
#define RENDER_LOGW(args...) do {printf(args);ALOGW(args);}while(0)
#define RENDER_LOGI(args...) do {printf(args);ALOGI(args);}while(0)
#define RENDER_LOGD(args...) do {printf(args);ALOGD(args);}while(0)
#endif

enum input_data_type {
	image_pic,
	image_buf,
	image_videos
};

enum EGL_SFTYPE {
	EGL_PBUFFER,
	EGL_WINDOW
};

struct render_int_buf
{
    void *addr;
    int fd;
    int length;
    int filled;
};


struct disp_param {
	int width;
	int height;
};

struct model_scale_factor {
	float x;
	float y;
	float z;
};

struct render_location_factor
{
	float px;
	float py;
	float pz;
};

struct render_angle_factor
{
	float rx;
	float ry;
};

struct vObj
{
	GLuint	vao;
	GLuint	vbo;
	GLuint	tex;
	int num;
};


struct camera_param
{
	int camera_num;
	int width;
	int height;
	enum input_data_type type;
	std::string path;
	void* buf_ptr;
};

struct avm_input_buf
{
    unsigned char *src;
    int filled;
};

struct avm_render_param
{
	struct disp_param disp;
	struct camera_param cam;
	struct model_scale_factor msf;
	struct render_location_factor rlf;
	struct render_angle_factor raf;
    struct avm_input_buf aib[CAMERAS_NUM];
	std::string mask_path;
	std::string exc_path;
	std::string cam_vertics_path;
	std::string model_path;
	std::string fisheye_vpath;
	std::string fisheye_wpath;
	std::string avm_view_off_path;
	enum EGL_SFTYPE egl_type;
	int is_realtime_rotate;
	int flag_write_data_format;
	int max_save_limit;
	int avm_view_offset_en;
	enum RK_SURROUND_3D_CMD cmd_type;
};


extern int flag_render;
extern EGLNativeWindowType native_window;
extern int quit_flag;

int programsInit();
void programsDestroj();

void egl_init(EGL_SFTYPE type);
bool RenderInit(struct avm_render_param* rparam);
void Render();
void RenderCleanup();

void set_frame_buffer(unsigned char** buf_addr, int length);
void set_cam_param(struct camera_param* p_cam);
void set_disp_res(struct disp_param* p_disp);
void set_3d_model_sf(struct model_scale_factor* sf);
void avm_render_swap_buffers(EGL_SFTYPE type);
void avm_render_output_buf(void* buf);
void *avm_get_output_addr();
void render_update_camera_src_data(unsigned char **src, unsigned char *disp_buf, int buffer_num);
void avm_get_share_fd(int *fd, int camera_id, int camera_buf_num);
void render_update_with_camera_index(int *fd, int *frame_index, unsigned char* disp_buf);
void avm_rev_app_msg(enum RK_SURROUND_3D_CMD cmd_type, void *data);


int render_drm_init();
void *render_alloc_drm_buf(int drmfd, int *fd,int in_w, int in_h, int in_bpp);
void render_uninit_drm(int drmfd);
void render_uninit_drm(void *viraddr, int size, unsigned int gfd, int drmfd);

void render_alloc_input_pbo(int num, int width, int height, int bpp);
void render_alloc_yuv_input_pbo(int num, int width, int height, int bpp);


int init_drm();
void uninit_drm(int drmfd);
void release_drm_obj(void *addr, int length, int fd, int drmfd, unsigned int drm_handle);
void *sur_alloc_drm_buf(int drmfd, int *fd,int in_w, int in_h, int in_bpp);

#endif /* CAMERA_TEX_HPP_ */
