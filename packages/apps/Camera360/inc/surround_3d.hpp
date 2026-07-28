#ifndef __SURROUND_3D_H__
#define __SURROUND_3D_H__

#include "surround_3d_inf.hpp"
#include "camera_tex.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

//#include "opencv2/opencv.hpp"
//#include "opencv2/videoio.hpp"

#define CAMERA_NUM 4


enum RK_SURROUND_3D_MAIN_STATE
{
    rk_state_main_idle,
	rk_state_main_calib,
    rk_state_main_init,
    rk_state_main_start,
    rk_state_main_render,
};


enum RK_SURROUND_3D_CALIB_STATE
{
    rk_state_calib_idle,
	rk_state_calib_fish_eye,
	rk_state_calib_defish,
	rk_state_calib_grid,
	rk_state_calib_seam,
	rk_state_calib_ready
};

enum RK_SURROUND_3D_RENDER_STATE
{
    rk_state_render_idle,
	rk_state_render_front_wide,
	rk_state_render_front_top_fish_eye,
	rk_state_render_front_top_far,
	rk_state_render_front_3D,

	rk_state_render_back_wide,
	rk_state_render_back_top_fish_eye,
	rk_state_render_back_top_far,
	rk_state_render_back_3D,

	rk_state_render_left_top_2D,
	rk_state_render_left_top_3D,
	rk_state_render_right_top_2D,
	rk_state_render_right_top_3D,
	rk_state_render_left_right_top_2D,

	rk_state_render_3d_view_adjuest
};

extern class surround_3d *sur_3d;

extern EGLSurface g_eglsurface;
extern EGLDisplay g_egldisplay;
extern EGLContext g_eglcontext;


class surround_3d {
public:
	surround_3d() :file_base_path(""), calib_base_path(""), ret_record(-1), quit_flag(false)
	{
		s3d_mutex = PTHREAD_MUTEX_INITIALIZER;
	}

	surround_3d(std::string file_path, std::string calib_path)
	{
		file_base_path = file_path;
		calib_base_path = calib_path;
		ret_record = (-1);
		quit_flag = false;
		s3d_mutex = PTHREAD_MUTEX_INITIALIZER;
	}
	~surround_3d();

	int rk_surround_3d_init();
	int rk_surround_3d_run();
	int rk_surround_3d_deinit();
	int rk_surround_3d_cmd_proc(enum RK_SURROUND_3D_CMD cmd, void *data);

	int rk_surround_3d_init_set_output_size(int width, int height);
	void rk_surround_3d_cam_param_init(int cam_num, int width, 
		int height, input_data_type cam_type);
	void rk_surround_3d_set_model_scale_factor(float x, float y, float z);
	void rk_surround_3d_set_egl_type(EGL_SFTYPE egl_type);
	int rk_surround_3d_send_cmd(int type);
	void rk_surround_3d_send_inf();
	void rk_surround_3d_get_result();
	void rk_surround_3d_send_share_fd(int *fd, int camera_id);
	void rk_surround_3d_set_car_model_path(std::string path);
	void rk_surround_3d_set_calib_base_path(std::string path){calib_base_path = path;}
	void rk_surround_3d_set_base_content_path(std::string path){file_base_path = path;}

    void rk_surround_3d_setcamera_data(unsigned char** addrs, unsigned char *disp_buf, int num);
	void rk_surround_3d_set_data_fd(int *fd, int *frame_index, unsigned char *disp_buf);

    enum RK_SURROUND_3D_MAIN_STATE main_state; // main state
    enum RK_SURROUND_3D_CALIB_STATE calib_state;
    enum RK_SURROUND_3D_RENDER_STATE render_state;

	struct suround_3d_inf_s s3d_inf;
	struct avm_render_param arp;

    struct pic_size_s out_frame0;
    struct pic_size_s out_frame1;

	std::string file_base_path;
	std::string calib_base_path;

	pthread_mutex_t s3d_mutex;

	int ret_record;
	bool quit_flag;
};

#endif
