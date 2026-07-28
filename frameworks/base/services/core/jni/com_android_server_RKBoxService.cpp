/*
 * Copyright (c) 2020 Rockchip Electronics Co., Ltd
 */
#define LOG_TAG "RKBOX"

#define LOG_NDEBUG 0

#include <nativehelper/JNIHelp.h>
#include "jni.h"
#include "android_runtime/AndroidRuntime.h"
#include "rkbox/rklog.h"


namespace android {

class JRKBox {
public:
    JRKBox();
    ~JRKBox();
private:
};

JRKBox* mRKBox = nullptr;

JRKBox::JRKBox() {
    ALOGD("%s", __FUNCTION__);
    start_rklog();
}

JRKBox::~JRKBox() {
    ALOGD("%s", __FUNCTION__);
}


static int nativeInit(JNIEnv* env, jobject obj) {
    int ret = 0;
    ALOGD("%s", __FUNCTION__);

    mRKBox = new JRKBox();
    return static_cast<jint>(ret);
}

// ----------------------------------------------------------------------------

static const JNINativeMethod sRKBoxManagementServiceMethods[] = {
    /* name, signature, funcPtr */
    { "nativeInit", "()I",
            (void*) nativeInit},
};

#define FIND_CLASS(var, className) \
        var = env->FindClass(className); \
        LOG_FATAL_IF(! (var), "Unable to find class " className)

#define GET_METHOD_ID(var, clazz, methodName, fieldDescriptor) \
        var = env->GetMethodID(clazz, methodName, fieldDescriptor); \
        LOG_FATAL_IF(! (var), "Unable to find method" methodName)

int register_com_android_server_RKBoxManagementService(JNIEnv* env) {
    int res = jniRegisterNativeMethods(env, "com/android/server/RKBoxManagementService",
            sRKBoxManagementServiceMethods, NELEM(sRKBoxManagementServiceMethods));
    LOG_FATAL_IF(res < 0, "Unable to register native methods.");
    (void)res; // Don't complain about unused variable in the LOG_NDEBUG case

    jclass clazz;
    FIND_CLASS(clazz, "com/android/server/RKBoxManagementService");
    return 0;
}

} /* namespace android */
