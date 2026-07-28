#ifndef __SURROUND_3D_INF_H__
#define __SURROUND_3D_INF_H__

enum RK_SURROUND_3D_CMD
{
	rk_cmd_render_front_wide = 0x0,		/* 前摄宽视野 */
	rk_cmd_render_front_top_fish_eye,	/* 前摄顶视 + 鱼眼 */
	rk_cmd_render_front_top_far,		/* 前摄顶视 + 远视野 */
	rk_cmd_render_front_3D,				/* 前摄3D */

	rk_cmd_render_back_wide,			/* 后摄宽视野 */
	rk_cmd_render_back_top_fish_eye,	/* 后摄顶视 + 鱼眼 */
	rk_cmd_render_back_top_far,			/* 后摄顶视 + 远视野 */
	rk_cmd_render_back_3D,				/* 后摄3D */

	rk_cmd_render_left_top_2D,			/* 左摄顶视 + 2D */
	rk_cmd_render_left_top_3D,			/* 左摄顶视 + 3D */
	rk_cmd_render_right_top_2D,			/* 右摄顶视 + 2D */
	rk_cmd_render_right_top_3D,			/* 右摄顶视 + 2D */

	rk_cmd_render_left_right_top_2D,	/* 左右摄顶视 + 2D */
	rk_cmd_render_left_right_top_3D,
	
	rk_cmd_render_3d_view_adjuest, /* 环绕车3D */
	rk_cmd_render_top_3d_view_adjuest,/* 环绕车3D */

	rk_cmd_render_set_screen_offset,

	rk_cmd_calib_init = 0x40,
	rk_cmd_calib_defish,
	rk_cmd_calib_search_count,
	rk_cmd_calib_grid,
	rk_cmd_calib_deinit,
};

typedef struct pic_size_s
{
    int width;
    int height;
} pic_size_t;

typedef struct pic_buf_s
{
    unsigned char *addr;
    int format;
	struct pic_size_s size;
} pic_buf_t;

typedef struct suround_3d_inf_s
{
	struct pic_buf_s buf_in[4];
	struct pic_buf_s buf_result0;
	struct pic_buf_s buf_result1;

} suround_3d_inf_t;

int rk_surround_3d_demo();

class surround_3d_inf {
public:
	surround_3d_inf();
	~surround_3d_inf();

	int rk_surround_3d_inf_init();
	int rk_surround_3d_inf_run();
	int rk_surround_3d_inf_deinit();
	int rk_surround_3d_inf_send_cmd(enum RK_SURROUND_3D_CMD cmd, void *data);

	struct suround_3d_inf_s s3d_inf;
};

#endif
