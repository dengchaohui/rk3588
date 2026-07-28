#include "inc/surround_3d.hpp"

//class surround_3d *sur_3d = NULL;

int surround_3d::rk_surround_3d_init()
{
    if (calib_base_path != "") {
        arp.cam_vertics_path = calib_base_path + "/array";
        arp.exc_path = calib_base_path + "/compensator/array";
        arp.mask_path = calib_base_path + "/mask";
		arp.avm_view_off_path = calib_base_path + "/avm_view_adj_offset";
    }
    else {
        ret_record = -1;
        RENDER_LOGE("NO CALIB DATA PATH, PLEASE CHECK\n");
        return -1;
    }

    if (file_base_path != "")
    {
        if (arp.cam.type == image_pic)
            arp.cam.path = file_base_path + "/camera_inputs/src_";

		if(arp.model_path == "")
        	arp.model_path = file_base_path + "/models/ferrari.dae";
    }
    else
    {
        RENDER_LOGE("NO CAR MODEL DATA PATH, PLEASE CHECK\n");
        ret_record = -1;
        return -1;
    }

    set_cam_param(&arp.cam);
    set_3d_model_sf(&arp.msf);
    set_disp_res(&arp.disp);
    egl_init(arp.egl_type);

    RENDER_LOGD("RENDER HERE, %S:%d\n", __FUNCTION__, __LINE__);

    //////////////////////// Shaders ////////////////////////////
    ret_record = programsInit();
    if (ret_record == -1)
    {
        RENDER_LOGE("load opengles shaders failed, please check\n");
        return (-1);
    }

    RENDER_LOGD("RENDER HERE, %S:%d\n", __FUNCTION__, __LINE__);

    ret_record = RenderInit(&arp);

    if (ret_record == 1)
    {
        main_state = rk_state_main_init;
        RENDER_LOGI("render init success\n");
    }else
    {
        RENDER_LOGI("render init failed\n");
    }

    for(GLenum err; (err = glGetError()) != GL_NO_ERROR;)
    {
        RENDER_LOGE("%s[%d]:render failed, get error mode %d\n",__FUNCTION__, __LINE__, err);
    }

    return ret_record;
}

int surround_3d::rk_surround_3d_init_set_output_size(int width, int height)
{
    arp.disp.width = width;
    arp.disp.height = height;
    out_frame0.height = height;
    out_frame0.width = width;
    return 0;
}

void surround_3d::rk_surround_3d_set_car_model_path(std::string path)
{
	arp.model_path = path;
}


void surround_3d::rk_surround_3d_cam_param_init(int cam_num, int width,
    int height, input_data_type cam_type)
{
    arp.cam.camera_num = cam_num;
    arp.cam.width = width;
    arp.cam.height = height;
    arp.cam.type = cam_type;
}

void surround_3d::rk_surround_3d_set_model_scale_factor(float x, float y, float z)
{
    arp.msf.x = x;
    arp.msf.y = y;
    arp.msf.z = z;
}

void surround_3d::rk_surround_3d_set_egl_type(EGL_SFTYPE egl_type)
{
    arp.egl_type = egl_type;//DEFAULT IS PBUFFER
}

void surround_3d::rk_surround_3d_send_inf()
{
    int length = s3d_inf.buf_in[0].size.height * s3d_inf.buf_in[0].size.width * 3;
    unsigned char* buf[4];

    for (int i = 0; i < CAMERA_NUM; i++)
    {
        buf[i] = s3d_inf.buf_in[i].addr;
    }

    //set_frame_buffer(buf, length);
}

void surround_3d::rk_surround_3d_get_result() {

}

int surround_3d::rk_surround_3d_cmd_proc(enum RK_SURROUND_3D_CMD cmd, void *data)
{
    switch (cmd)
    {
    case rk_cmd_render_front_wide:
    case rk_cmd_render_front_top_fish_eye:
    case rk_cmd_render_front_top_far:
    case rk_cmd_render_front_3D:
    case rk_cmd_render_back_wide:
    case rk_cmd_render_back_top_fish_eye:
    case rk_cmd_render_back_top_far:
    case rk_cmd_render_back_3D:
    case rk_cmd_render_left_top_2D:
    case rk_cmd_render_left_top_3D:
    case rk_cmd_render_right_top_2D:
    case rk_cmd_render_right_top_3D:
	case rk_cmd_render_left_right_top_2D:
	case rk_cmd_render_left_right_top_3D:	
	case rk_cmd_render_3d_view_adjuest:
	case rk_cmd_render_top_3d_view_adjuest:
		avm_rev_app_msg(cmd, NULL);
        break;
	case rk_cmd_render_set_screen_offset:
		avm_rev_app_msg(cmd, data);
        break;
	case rk_cmd_calib_defish:
	case rk_cmd_calib_grid:
	case rk_cmd_calib_search_count:
		
		break;
    default:
		avm_rev_app_msg(cmd, NULL);
        break;
    }
    return 0;
}

void surround_3d::rk_surround_3d_setcamera_data(unsigned char** addrs, unsigned char *disp_buf, int num) {
    //render_update_camera_src_data(addrs, disp_buf, num);
}

void surround_3d::rk_surround_3d_send_share_fd(int *fd, int camera_id)
{
    avm_get_share_fd(fd, camera_id, 4);
}

void surround_3d::rk_surround_3d_set_data_fd(int *fd, int *frame_index, unsigned char *disp_buf)
{
    render_update_with_camera_index(fd, frame_index, disp_buf);
    avm_render_swap_buffers(this->arp.egl_type);
}

surround_3d::~surround_3d()
{
    RENDER_LOGD("START DESTROY EGLDISPLAY\n");
	if(g_egldisplay != EGL_NO_DISPLAY && g_eglsurface != EGL_NO_SURFACE && g_eglcontext != EGL_NO_CONTEXT){
		eglMakeCurrent(g_egldisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroySurface(g_egldisplay, g_eglsurface);
		eglDestroyContext(g_egldisplay, g_eglcontext);	
		eglTerminate(g_egldisplay);
		g_egldisplay = EGL_NO_DISPLAY;
	    g_eglsurface = EGL_NO_SURFACE;
	    g_eglcontext = EGL_NO_CONTEXT;
	}
	RENDER_LOGD("END DESTROY EGLDISPLAY\n");
}

int surround_3d::rk_surround_3d_run()
{
    /* cmd proc */
    pthread_mutex_lock(&s3d_mutex);
    //rk_surround_3d_send_cmd(13);

    /* state changed */

    main_state = rk_state_main_render;

    for(GLenum err; (err = glGetError()) != GL_NO_ERROR;)
    {
        RENDER_LOGE("%s[%d]:render failed, get error mode %d\n",__FUNCTION__, __LINE__, err);
    }

    /* state run */
    struct timeval tpend1, tpend2;
    long usec0 = 0;
    gettimeofday(&tpend1, NULL);

   // Render();
    //glFinish();

    gettimeofday(&tpend2, NULL);
    usec0 = 1000 * (tpend2.tv_sec - tpend1.tv_sec) + (tpend2.tv_usec - tpend1.tv_usec) / 1000;

    RENDER_LOGD("rk-deubg render consume time=%ld ms \n", usec0);

   // avm_render_swap_buffers(this->arp.egl_type);

    pthread_mutex_unlock(&s3d_mutex);
    return 0;
}

int surround_3d::rk_surround_3d_deinit()
{
    if (main_state >= rk_state_main_init) {
        RenderCleanup();
        programsDestroj();
    }
    return 0;
}
