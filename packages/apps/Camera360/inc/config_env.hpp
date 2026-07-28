#ifndef __CONFIG_ENV_H__
#define __CONFIG_ENV_H__

#ifdef WIN32
#define CALIB_USE_VIEW (1)
#else
#define CALIB_USE_VIEW (0)
#endif

#define INPUT_RAW 0
#define INPUT_IMAGE 1
#define CONFIG_USE_TEMPLATE_FILE 1

/* 开发者 模型 */
//#define TEMPLATE_SIZE_DS 3210
//#define TEMPLATE_SIZE_DH 5100

/* 21#展厅 */
#define TEMPLATE_SIZE_DS 3120
#define TEMPLATE_SIZE_DH 4250

#endif