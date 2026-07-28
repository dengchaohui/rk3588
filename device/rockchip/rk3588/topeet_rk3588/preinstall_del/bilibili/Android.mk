LOCAL_PATH := $(my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := bilibili
LOCAL_MODULE_CLASS := APPS
LOCAL_MODULE_PATH := $(TARGET_OUT_ODM)/bundled_uninstall_back-app
LOCAL_SRC_FILES := $(LOCAL_MODULE)$(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_CERTIFICATE := PRESIGNED
LOCAL_DEX_PREOPT := false
LOCAL_ENFORCE_USES_LIBRARIES := false
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := $(COMMON_ANDROID_PACKAGE_SUFFIX)
LOCAL_JNI_SHARED_LIBRARIES_ABI := arm64
MY_LOCAL_PREBUILT_JNI_LIBS := \
	lib/arm64/libAPSE_7.0.1.so\
	lib/arm64/libAPSE_J.so\
	lib/arm64/libBiliBMM3AAudioProcess.so\
	lib/arm64/libBugly.so\
	lib/arm64/libBurstLinker.so\
	lib/arm64/libCtaApiLib.so\
	lib/arm64/libMNN.so\
	lib/arm64/libPerfgeniusApi.so\
	lib/arm64/libSapling.so\
	lib/arm64/libacckitjni-lib.so\
	lib/arm64/libadjni.so\
	lib/arm64/libandroidx.graphics.path.so\
	lib/arm64/libapkpatch.so\
	lib/arm64/libavif-jni.so\
	lib/arm64/libavif.so\
	lib/arm64/libbili.so\
	lib/arm64/libbiliAudio3AProcess_JNI.so\
	lib/arm64/libbili_core.so\
	lib/arm64/libbili_core_dumper.so\
	lib/arm64/libbiliid.so\
	lib/arm64/libbilirtclibrary.so\
	lib/arm64/libblive_pandora.so\
	lib/arm64/libblkv.so\
	lib/arm64/libblog.so\
	lib/arm64/libbreflect.so\
	lib/arm64/libbvc-xcode-tools.so\
	lib/arm64/libbytehook.so\
	lib/arm64/libc++_shared.so\
	lib/arm64/libchronos.so\
	lib/arm64/libcrypto.so\
	lib/arm64/libcrypto_c.so\
	lib/arm64/libdav1d.so\
	lib/arm64/libdexvmp.so\
	lib/arm64/libdynamicengine.so\
	lib/arm64/libentryexpro.so\
	lib/arm64/libfb_dalvik-internals.so\
	lib/arm64/libgifimage.so\
	lib/arm64/libgraphics-core.so\
	lib/arm64/libhardcoder.so\
	lib/arm64/libhpatchz.so\
	lib/arm64/libignet.so\
	lib/arm64/libijk.so\
	lib/arm64/libijkDolbyVision.so\
	lib/arm64/libijkaicenter.so\
	lib/arm64/libijkffext.so\
	lib/arm64/libijkffmpeg.so\
	lib/arm64/libijkplayer.so\
	lib/arm64/libijksdl.so\
	lib/arm64/libimagepipeline.so\
	lib/arm64/liblpLite.so\
	lib/arm64/libmmkv.so\
	lib/arm64/libmnn_predictor.so\
	lib/arm64/libmsaoaidauth.so\
	lib/arm64/libmsaoaidsec.so\
	lib/arm64/libnative-filters.so\
	lib/arm64/libnative-imagetranscoder.so\
	lib/arm64/libnirvana.so\
	lib/arm64/libopenh264.so\
	lib/arm64/libquickjs-jni.so\
	lib/arm64/libquickjs.so\
	lib/arm64/libsafemode.so\
	lib/arm64/libshadowhook.so\
	lib/arm64/libsqliteJni.so\
	lib/arm64/libssl.so\
	lib/arm64/libstartup-optimize.so\
	lib/arm64/libstatic-webp.so\
	lib/arm64/libtencentloc.so\
	lib/arm64/libtf.so\
	lib/arm64/libtoyger.so\
	lib/arm64/libweibosdkcore.so\
	lib/arm64/libzkfv_tj.so\

MY_APP_LIB_PATH := $(TARGET_OUT_ODM)/bundled_uninstall_back-app/$(LOCAL_MODULE)/lib/$(LOCAL_JNI_SHARED_LIBRARIES_ABI)
ifneq ($(LOCAL_JNI_SHARED_LIBRARIES_ABI), None)
$(warning MY_APP_LIB_PATH=$(MY_APP_LIB_PATH))
LOCAL_POST_INSTALL_CMD :=     mkdir -p $(MY_APP_LIB_PATH)     $(foreach lib, $(MY_LOCAL_PREBUILT_JNI_LIBS), ; cp -f $(LOCAL_PATH)/$(lib) $(MY_APP_LIB_PATH)/$(notdir $(lib)))
endif
include $(BUILD_PREBUILT)

